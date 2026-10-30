# Livrer une version de HC

Signer, notariser, empaqueter. La procédure est écrite ici parce qu'elle se
redécouvre mal : chaque étape a une raison, et une seule d'entre elles est
évidente.

## Ce que macOS exige, et ce qu'il n'exige pas

| | hors App Store | App Store |
| -- | -- | -- |
| signature **Developer ID** | exigée | non (certificat différent) |
| **notarisation** | exigée | non (revue à la place) |
| **Hardened Runtime** | **exigé** | exigé |
| **App Sandbox** | *optionnel* | exigé |

Depuis la 1.0, HC se distribue des deux façons — DMG notarisé et Mac App
Store — et c'est la MÊME application : Hardened Runtime et bac à sable des deux
côtés (#176). Voir le commentaire de `HC/HC.entitlements`, qui dit pourquoi et
quels droits, et `docs/mesures/bac_a_sable.txt`, qui dit ce qui a été mesuré.

## Prérequis, une seule fois

Compte Apple Developer actif. Puis, dans Xcode → Settings → Accounts, créer un
certificat **Developer ID Application**. Vérifier qu'il est là :

    security find-identity -v -p codesigning

Il doit lister une ligne « Developer ID Application: … (TEAMID) ». Noter le
TEAMID.

Créer ensuite un mot de passe d'application sur appleid.apple.com (Connexion et
sécurité → Mots de passe pour application), et le ranger dans le trousseau pour
ne plus le retaper :

    xcrun notarytool store-credentials "HC-notarisation" \
      --apple-id "<l'Apple ID du compte>" \
      --team-id "<TEAMID>" \
      --password "<le mot de passe d'application>"

## À chaque version

### 1. La version

`MARKETING_VERSION` dans les deux configurations de `HC.xcodeproj`. C'est lui
que `CFBundleShortVersionString` référence, donc lui que le Finder, la fenêtre
« À propos » **et `the version` en HyperTalk** annoncent — `cocoa_global_get`
le lit depuis le bundle.

**Et `HC_VERSION` dans `HC/hc_core.h`**, qui n'est que le repli des harnais
mais doit rester juste. Il est resté à `0.6.6` pendant trois versions parce que
le noyau annonçait interroger l'hôte et qu'aucun `.m` ne répondait.

    grep -n "HC_VERSION\|MARKETING_VERSION" HC/hc_core.h HC.xcodeproj/project.pbxproj

Changer `HC_VERSION` fait bouger **un seul témoin** de la suite, et ce n'est
plus celui qu'annonçait ce paragraphe. Il nommait `bilan2`, `bilan3`, `bilan4`,
`bilan5` et `mondenoms`, qui inventorient `the version` : depuis 0.6.9.4, `the
version` rend celle d'HyperCard — `2.4.1`, une constante — et ces cinq-là ne
bougent plus. Le seul qui affiche NOTRE numéro est `pilereelle`, section 3, qui
lit `the hcVersion` sous son propre nom et vérifie que les deux ne sont pas la
même chose :

    ./tests/lance.sh --enregistre pilereelle
    ./tests/lance.sh                       # et vérifier que RIEN d'autre n'a bougé

Le diff attendu fait exactement deux lignes — `the hcVersion` seul, puis
`the version & " / " & the hcVersion`. Toute autre ligne qui bouge n'est pas la
version.

Ce « rien d'autre » est la vraie vérification : si un autre harnais change en
même temps, ce n'est pas la version qui l'a fait.

**Le piège de 0.6.9.2 :** il avait été corrigé APRÈS la construction du DMG,
si bien que le binaire livré annonçait encore `0.6.5`. Version d'abord, tag
ensuite, DMG en dernier.

### 2. Construire en Release

    xcodebuild -project HC.xcodeproj -target HC -configuration Release build

`Release` est déjà universel : `ONLY_ACTIVE_ARCH = YES` n'est posé qu'en Debug
et `ARCHS` n'est pinné nulle part. Un build **Debug** ne contient que
l'architecture de la machine qui compile et ne se lancera pas ailleurs — c'est
la première chose à vérifier :

    cd "$(xcodebuild -project HC.xcodeproj -target HC -configuration Release \
      -showBuildSettings | awk -F' = ' '/ BUILT_PRODUCTS_DIR/{print $2}')"
    lipo -archs HC.app/Contents/MacOS/HC        # -> x86_64 arm64
    defaults read "$PWD/HC.app/Contents/Info" CFBundleShortVersionString

### 3. Signer

    codesign --force --deep --options runtime --timestamp \
      --entitlements HC/HC.entitlements \
      --sign "Developer ID Application: <nom> (<TEAMID>)" \
      HC.app

`--options runtime` active le Hardened Runtime dans la signature ; sans lui la
notarisation refuse. `--timestamp` est exigé aussi : une signature sans horodatage
sécurisé est rejetée.

Vérifier :

    codesign -dv --verbose=4 HC.app     # Authority=Developer ID Application: …
                                        # Runtime Version / flags: runtime

Une réponse `Signature=adhoc` signifie que la signature locale d'Xcode est
encore en place et que rien n'a été signé pour la distribution.

### 4. Notariser

Le service n'accepte pas un `.app` nu : il faut un conteneur.

    ditto -c -k --keepParent HC.app HC.zip
    xcrun notarytool submit HC.zip \
      --keychain-profile "HC-notarisation" --wait

`--wait` bloque jusqu'au verdict — quelques minutes d'ordinaire. En cas de
rejet, le journal dit précisément quoi :

    xcrun notarytool log <submission-id> \
      --keychain-profile "HC-notarisation"

#### Sans notarytool : Catalina et Xcode 12

`notarytool` n'existe que depuis Xcode 13, qui demande macOS 11.3. L'ancien
outil, `altool`, n'est plus accepté par Apple depuis le 1ᵉʳ novembre 2023 — le
bouton « Distribute App → Developer ID » d'Xcode 12 passe par lui. Sur un Mac
resté sous Catalina, on passe donc par l'API web d'Apple, celle qu'appelle
notarytool, avec `outils/notarise.py` : Python, `openssl` et `curl`, rien à
installer.

Une fois : une **clé d'API**, dans App Store Connect → Utilisateurs et accès →
Intégrations → API App Store Connect → Clés d'équipe → « + », accès
« Développeur ». Télécharger le fichier `AuthKey_<ID>.p8` — **Apple ne le donne
qu'une fois** —, le ranger hors du dépôt, et noter son identifiant (Key ID) et
celui de l'émetteur (Issuer ID, en haut de la page).

Vérifier la clé sans rien envoyer :

    python3 outils/notarise.py HC.zip --cle ~/cles/AuthKey_<ID>.p8 \
      --id-cle <Key ID> --emetteur <Issuer ID> --essai

Puis la même ligne sans `--essai` : le script soumet, envoie, attend le verdict
et affiche le journal d'Apple. Le même pour le DMG à l'étape 6, à la place de
`notarytool submit`. L'agrafage (`xcrun stapler`) existe dans Xcode 12.

**Tout d'un coup : `outils/livrer.sh`.** À partir d'un dossier qui contient
HC.app et les piles (`~/Desktop/HC` par défaut), il fait les étapes 3 à 7 :
signer et notariser l'app, l'agrafer, fabriquer le DMG à côté du dossier, le
signer, le notariser, l'agrafer, puis vérifier avec `spctl`. Ses réglages
— la clé, ses identifiants, le certificat — vivent dans `~/notarisation/reglages`,
hors du dépôt ; son en-tête dit lesquels. `--essai` vérifie tout sans rien
signer ni envoyer. Relancé après une interruption, il ne resigne pas une app
déjà signée et n'envoie pas ce qui est déjà notarisé. Joué ici contre des
imitations de codesign, stapler, hdiutil et spctl — la relance, un refus, un
vieux DMG non signé —, puis sur le Mac de l'utilisatrice (ci-dessous).

Un DMG déjà là n'est repris que s'il contient encore exactement le dossier :
le script l'ouvre en lecture seule et compare. Il reprenait autrefois tout DMG
signé, sur sa seule signature — une pile corrigée sans changer de version
partait donc avec l'ancienne, annoncée « PRÊT ». S'il diffère, le script
s'arrête et nomme les fichiers ; supprimer le DMG et relancer le refait. Et
`--essai` sur une clé refusée sortait avec le code 0 ; il s'arrête désormais.
Les deux trouvés le 13 octobre par un audit extérieur, corrigés le jour même,
essayés contre les imitations seulement — PAS ENCORE SUR UN MAC.

**MESURÉ le soir du 13 octobre, sur le Mac de l'utilisatrice**, avec cette
version : une livraison complète de 0.8.3, DMG neuf, du premier coup. La
relance, elle, s'est arrêtée à l'étape 1 — « detritus not allowed » : entre
les deux lancements, le paquet HC.app avait reçu `com.apple.FinderInfo` et
`com.apple.metadata:_kMDItemUserTags`, sans doute du Finder, et le script ne
les effaçait qu'avant de signer, pas sur le chemin de la reprise. Il les
efface désormais dans les deux cas. La comparaison du DMG au dossier n'a donc
toujours pas tourné sur un Mac.

**MESURÉ, du 12 au 13 octobre, sur le Mac de l'utilisatrice** (Catalina 10.15,
Xcode 12.4) : la répétition générale, HC 0.8.2 signée puis zippée. Apple a
accepté le jeton, l'envoi sur S3 a réussi, et le verdict est « Accepted »,
« Ready for distribution », sans aucune remarque (`"issues": null`), les deux
architectures dans le ticket. La première soumission du compte est restée plus
d'une heure « In Progress » ; `--suivre` a repris l'attente. Le lendemain,
`livrer.sh` a fait toute la livraison du premier coup, vingt-trois piles
comprises : le DMG accepté en vingt secondes, et « Notarized Developer ID »
pour le DMG comme pour l'app.

Ce que voit alors l'utilisatrice, le DMG téléchargé ouvert et l'app lancée :
la seule question que macOS pose de tout ce qui vient d'Internet — « cette
application a été téléchargée depuis Internet, voulez-vous vraiment
l'ouvrir ? » —, avec un bouton « Ouvrir », une fois. Plus de « endommagé ».

**Le DMG se joint à la release sous le nom `HC.dmg`** : le lien
`releases/latest/download/HC.dmg` en dépend. `livrer.sh` le fabrique sous
`HC-<version>.dmg` ; le renommer ne touche ni à la signature ni au ticket.

### 5. Agrafer

    xcrun stapler staple HC.app

L'agrafage écrit le ticket DANS le bundle, ce qui permet à l'app de s'ouvrir
sur une machine hors ligne. Sans lui, Gatekeeper doit interroger Apple à la
première ouverture, et un utilisateur sans réseau est bloqué.

### 6. Le DMG

Le DMG contient l'application ET la pile d'exemple du pendu. On les met
dans un dossier, et c'est ce dossier qu'on emballe :

    make verifie && ./tests/lance.sh pendu          # le jeu passe son test
    tests/.travail/bin/pendu tests/donnees/pendu_pile.txt Pendu.stack
    mkdir -p dmg && cp -R HC.app Pendu.stack dmg/
    hdiutil create -volname "HC <version>" -srcfolder dmg \
      -ov -format UDZO HC-<version>.dmg

La pile est ENGENDRÉE à chaque livraison, depuis ses sources
(tests/donnees/pendu_pile.txt et tests/harnais/pendu.c), et jamais
versionnée : un fichier engendré posé à côté de ses sources dérive dès la
première modification, sans que rien ne le signale. Le harnais « pendu » joue
quelques parties avant qu'on l'écrive.

Depuis la 0.8.4, le dossier porte aussi les deux manuels du premier XCMD,
copiés du dépôt tels quels :

    cp xcmd/MANUAL.md xcmd/MANUEL.md dmg/

La licence de WAMR, l'interprète des modules, n'a pas à y être mise : elle
voyage DANS l'app, en `HC.app/Contents/Resources/wamr/` (LICENSE et
ATTRIBUTIONS.md), copiée par la phase « Licence de WAMR » du projet.

Puis le signer et l'agrafer lui aussi — sinon c'est le DMG qui déclenche
l'alerte, même si l'app à l'intérieur est irréprochable :

    codesign --force --timestamp \
      --sign "Developer ID Application: <nom> (<TEAMID>)" HC-<version>.dmg
    xcrun notarytool submit HC-<version>.dmg \
      --keychain-profile "HC-notarisation" --wait
    xcrun stapler staple HC-<version>.dmg

### 7. Vérifier comme un utilisateur, pas comme l'auteur

    spctl -a -vvv -t install HC-<version>.dmg   # -> accepted, source=Notarized …

Et le test qui compte vraiment : **télécharger le DMG depuis la release** sur
une autre machine, ou au moins poser le drapeau de quarantaine à la main pour
reproduire ce que voit un utilisateur :

    xattr -w com.apple.quarantine \
      "0081;00000000;Safari;" HC-<version>.dmg

Si l'app s'ouvre après ça, la chaîne est complète. Sinon, `spctl` dit pourquoi.

### 8. Le tag et la release

    git tag -a HC-<version> -m "HC-<version> — <sujet>"
    git push origin HC-<version>

Puis la release sur GitHub, avec `docs/releases/HC-<version>.md` **collé** dans
le corps. Attention : ce corps est une COPIE, pas un lien. Modifier le fichier
du dépôt ensuite ne met pas la release à jour ; il faut l'éditer à la main.

**Et le DMG DEUX FOIS** : `HC-<version>.dmg`, et la même copie sous le nom
fixe `HC.dmg`.

    cp HC-<version>.dmg HC.dmg      # puis joindre les deux à la release

Le bouton « Télécharger » de hc.labynet.fr pointe sur
`https://github.com/cleobuline/hc/releases/latest/download/HC.dmg`, que GitHub
redirige vers le fichier `HC.dmg` de la DERNIÈRE release. Un seul clic, et
plus de page de release à traverser : décidé le 10 octobre, quand 9 300 vues
sur Reddit n'avaient donné qu'une poignée de téléchargements. Une release sans
`HC.dmg` casse ce bouton — GitHub répond 404 —, alors que son DMG numéroté,
lui, reste en place.

## Tant que la notarisation n'existe pas

Une version non notarisée se heurte à un message qui dit faux — *« HC est
endommagé et ne peut pas être ouvert »*. L'app n'est pas abîmée, elle n'est pas
notarisée. Le contournement, à donner aux utilisateurs :

    xattr -dr com.apple.quarantine /Applications/HC.app

C'est ce que porte la section « Installing » de la note de 0.6.9.4. Le jour où
la chaîne ci-dessus tourne, cette section disparaît des notes suivantes.

## Le Mac App Store — la 1.0, le 30 octobre

Ce que disait cette section le 13 octobre n'est plus le plan : la CI de GitHub
devait construire et envoyer, faute d'un Xcode assez récent chez
l'utilisatrice. Elle a préféré installer Monterey (12.7.2) et Xcode 14.2 sur
son Mac, partitions Lucie et Juliette, et envoyer elle-même ; ses certificats
et sa clé ne quittent pas sa machine.

Pourquoi Xcode 14.2 : les notes de version d'App Store Connect exigent Xcode 14
ou plus récent pour envoyer une application macOS, et Xcode 13 ne suffit plus.
Xcode 14.2 est le dernier qui tourne sous Monterey (de mémoire ; le 14.3
demande Ventura).

Ce qui est fait :
  1. le **bac à sable** (#176) : le dossier de travail, l'enregistrement par
     NSItemReplacementDirectory, les panneaux hors conteneur ;
  2. les **démos** ne voyagent pas dans l'application : File ▸ Load Demos…
     ouvre `https://labynet.fr/download/demo.dmg` dans le navigateur.
     Aucune pile d'Apple n'y est ;
  3. le **numéro de build** est réglable : `CFBundleVersion` vaut
     `$(CURRENT_PROJECT_VERSION)`. App Store Connect refuse deux envois de la
     même version avec le même build : le monter à chaque nouvel envoi
     (cible HC ▸ General ▸ Build) ;
  4. **`ITSAppUsesNonExemptEncryption`** vaut `false` : HC ne chiffre rien —
     pas de réseau, et zlib compresse sans chiffrer. App Store Connect ne
     pose donc plus la question à chaque envoi ;
  5. **`NSHumanReadableCopyright`** : « Copyright © 2026 Patricia
     Benedetto », comme `LICENSE`.

L'identifiant est **`fr.labynet.HC`**, celui de la fiche App Store Connect,
qui ne se change plus une fois créée. Le projet disait `labynet.HC` jusqu'au
30 octobre : le conteneur du bac à sable a donc changé de nom, et le dossier de
travail se rechoisit une fois.

Le schéma HC doit construire la cible pour l'archivage (« Archive » coché dans
Edit Scheme ▸ Build) : sans quoi Product ▸ Archive est grisé, comme le 30
octobre.

L'envoi, depuis Xcode 14.2 : destination « Any Mac », Product ▸ Archive, puis
dans l'Organizer, Distribute App ▸ App Store Connect ▸ Upload. La signature est
automatique, avec l'équipe de l'Association Labynet : certificat de
distribution d'applications Mac, certificat de l'installateur, et profil App
Store pour `fr.labynet.HC`.

Puis la revue humaine d'Apple, qui regardera aussi le nom : HyperCard est une
marque d'Apple. La fiche le dit : HC est un projet indépendant, et HyperCard
et HyperTalk sont des marques d'Apple.
