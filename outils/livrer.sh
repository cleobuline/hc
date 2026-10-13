#!/bin/bash
# livrer.sh — TOUTE LA LIVRAISON D'UN COUP, depuis un dossier qui contient
# HC.app et les piles : signer l'app, la notariser, l'agrafer ; fabriquer le
# DMG, le signer, le notariser, l'agrafer ; vérifier comme un utilisateur.
#
# Les étapes sont celles de docs/livraison.md (3 à 7), avec outils/notarise.py
# à la place de notarytool — le Mac de l'utilisatrice est sous Catalina, avec
# Xcode 12.4, où notarytool n'existe pas.
#
#     outils/livrer.sh                     le dossier ~/Desktop/HC
#     outils/livrer.sh ~/ailleurs/HC       un autre dossier
#     outils/livrer.sh --essai             tout vérifier, ne rien signer ni envoyer
#
# LES RÉGLAGES — la clé d'API, ses identifiants, le certificat — vivent HORS
# DU DÉPÔT, dans ~/notarisation/reglages, un petit fichier lu au départ :
#
#     CLE="$HOME/notarisation/AuthKey_XXXXXXXXXX.p8"
#     ID_CLE="XXXXXXXXXX"
#     EMETTEUR="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#     IDENTITE="Developer ID Application: … (TEAMID)"
#     ENTITLEMENTS="$HOME/…/hc/HC/HC.entitlements"     # facultatif
#
# ON PEUT LE RELANCER APRÈS UNE INTERRUPTION. Une app déjà signée par
# IDENTITE n'est pas resignée — la resigner changerait son empreinte et
# perdrait sa notarisation ; si son ticket existe déjà chez Apple, elle est
# agrafée sans être renvoyée. Même chose pour un DMG déjà signé. Pour refaire
# un DMG — des piles ont changé —, supprimer l'ancien d'abord.
#
# MESURÉ SUR LE MAC DE L'UTILISATRICE le 13 octobre (Catalina, Xcode 12.4),
# du premier coup : HC 0.8.2 et vingt-trois piles. L'app resignée a retrouvé
# l'empreinte déjà notarisée — le ticket a été agrafé sans renvoi —, le DMG a
# été accepté en vingt secondes, et spctl dit « Notarized Developer ID » pour
# les deux. Chaque étape dit ce qu'elle fait, et le script s'arrête à la
# première erreur.

set -eu

ICI="$(cd "$(dirname "$0")" && pwd)"
REGLAGES="$HOME/notarisation/reglages"
DOSSIER="$HOME/Desktop/HC"
ESSAI=0

for a in "$@"; do
    case "$a" in
        --essai) ESSAI=1 ;;
        -h|--help) sed -n '2,32p' "$0"; exit 0 ;;
        -*) echo "option inconnue : $a" >&2; exit 2 ;;
        *) DOSSIER="$a" ;;
    esac
done

etape() { printf '\n== %s\n' "$*"; }
ok()    { printf '   ok : %s\n' "$*"; }
stop()  { printf '\n!! %s\n' "$*" >&2; exit 1; }

# ------------------------------------------------------------ les réglages

etape "réglages"
[ -f "$REGLAGES" ] || stop "pas de fichier $REGLAGES — voir l'en-tête de ce script"
# shellcheck disable=SC1090
. "$REGLAGES"
for v in CLE ID_CLE EMETTEUR IDENTITE; do
    eval "val=\${$v:-}"
    [ -n "$val" ] || stop "$v n'est pas défini dans $REGLAGES"
done
[ -f "$CLE" ] || stop "clé d'API introuvable : $CLE"
if [ -z "${ENTITLEMENTS:-}" ]; then
    ENTITLEMENTS="$ICI/../HC/HC.entitlements"
fi
[ -f "$ENTITLEMENTS" ] || stop "entitlements introuvables : $ENTITLEMENTS (ENTITLEMENTS dans $REGLAGES)"
NOTARISE="$ICI/notarise.py"
[ -f "$NOTARISE" ] || stop "notarise.py introuvable à côté de ce script : $NOTARISE"
security find-identity -v -p codesigning | grep -qF "\"$IDENTITE\"" ||
    stop "le certificat « $IDENTITE » n'est pas dans le trousseau"
ok "clé, certificat, entitlements, notarise.py"

notariser() {   # notariser <fichier> <nom à afficher> — rend la main seulement si Apple accepte
    python3 "$NOTARISE" "$1" --cle "$CLE" --id-cle "$ID_CLE" --emetteur "$EMETTEUR" ||
        stop "notarisation refusée ou interrompue pour $2 (le journal est au-dessus)"
}

signe_par_nous() {   # signe_par_nous <chemin> — 0 si signé par IDENTITE
    codesign -dv --verbose=4 "$1" 2>&1 | grep -qF "Authority=$IDENTITE"
}

# ------------------------------------------------------------ l'app

etape "le dossier $DOSSIER"
APP="$DOSSIER/HC.app"
[ -d "$APP" ] || stop "pas de HC.app dans $DOSSIER"
VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$APP/Contents/Info.plist")"
ARCHS="$(lipo -archs "$APP/Contents/MacOS/HC")"
for arch in x86_64 arm64; do          # chacune pour soi, dans n'importe quel ordre
    case " $ARCHS " in
        *" $arch "*) ;;
        *) stop "HC.app n'est pas universelle ($ARCHS) : construire en Release" ;;
    esac
done
DMG="$(dirname "$DOSSIER")/HC-$VERSION.dmg"
ok "HC $VERSION, $ARCHS"
echo "   contenu du DMG :"
ls -1 "$DOSSIER" | sed 's/^/     /'
echo "   DMG : $DMG"

if [ "$ESSAI" = 1 ]; then
    etape "essai de la clé d'API"
    python3 "$NOTARISE" "$APP/Contents/Info.plist" --cle "$CLE" --id-cle "$ID_CLE" \
        --emetteur "$EMETTEUR" --essai >/dev/null && ok "jeton signé"
    printf "\n--essai : rien n'a été signé ni envoyé.\n"
    exit 0
fi

etape "1. signer HC.app"
if signe_par_nous "$APP"; then
    ok "déjà signée par $IDENTITE : on ne la resigne pas"
else
    # Une app copiée par le Finder porte des attributs étendus, que codesign
    # refuse : « resource fork, Finder information, or similar detritus not
    # allowed ». Ils ne font pas partie de l'app ; on les efface.
    xattr -cr "$APP"
    codesign --force --deep --options runtime --timestamp \
        --entitlements "$ENTITLEMENTS" --sign "$IDENTITE" "$APP"
    ok "signée"
fi
codesign --verify --deep --strict "$APP" || stop "la signature de HC.app ne se vérifie pas"
codesign -dv "$APP" 2>&1 | grep -q "flags=.*runtime" ||
    stop "HC.app est signée sans Hardened Runtime : la notarisation la refuserait"
ok "signature vérifiée, Hardened Runtime présent"

etape "2. notariser et agrafer HC.app"
if xcrun stapler staple "$APP" >/dev/null 2>&1; then
    ok "déjà notarisée : ticket agrafé sans renvoi"
else
    ZIP="$(mktemp -d)/HC.zip"
    ditto -c -k --keepParent "$APP" "$ZIP"
    notariser "$ZIP" "HC.app"
    rm -f "$ZIP"
    xcrun stapler staple "$APP"
fi
xcrun stapler validate "$APP" >/dev/null || stop "le ticket de HC.app ne se valide pas"
ok "HC.app notarisée et agrafée"

# ------------------------------------------------------------ le DMG

etape "3. fabriquer et signer le DMG"
if [ -e "$DMG" ]; then
    signe_par_nous "$DMG" ||
        stop "$DMG existe déjà sans notre signature : le supprimer ou le renommer"
    ok "DMG déjà fait et signé : on le reprend"
else
    hdiutil create -volname "HC $VERSION" -srcfolder "$DOSSIER" -ov -format UDZO "$DMG" >/dev/null
    codesign --force --timestamp --sign "$IDENTITE" "$DMG"
    ok "fabriqué et signé"
fi

etape "4. notariser et agrafer le DMG"
if xcrun stapler staple "$DMG" >/dev/null 2>&1; then
    ok "déjà notarisé : ticket agrafé sans renvoi"
else
    notariser "$DMG" "$(basename "$DMG")"
    xcrun stapler staple "$DMG"
fi
xcrun stapler validate "$DMG" >/dev/null || stop "le ticket du DMG ne se valide pas"
ok "DMG notarisé et agrafé"

# ------------------------------------------------------------ comme un utilisateur

etape "5. vérifier comme un utilisateur"
spctl -a -vvv -t install "$DMG" 2>&1 | sed 's/^/   /'
spctl -a -vvv -t install "$DMG" 2>&1 | grep -q "accepted" ||
    stop "Gatekeeper refuse le DMG"
spctl -a -vvv "$APP" 2>&1 | sed 's/^/   /'
spctl -a -vvv "$APP" 2>&1 | grep -q "accepted" ||
    stop "Gatekeeper refuse HC.app"

printf '\nPRÊT : %s\n' "$DMG"
printf "Signé, notarisé, agrafé. Il ne reste qu'à le joindre à la release,\n"
printf "SOUS LE NOM HC.dmg : le lien releases/latest/download/HC.dmg en dépend.\n"
printf "Le renommer ne touche ni à la signature ni au ticket.\n"
