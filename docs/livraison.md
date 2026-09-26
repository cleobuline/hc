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

HC se distribue hors App Store. Donc : Hardened Runtime oui, sandbox non —
voir le commentaire dans `HC/HC.entitlements`, qui dit pourquoi.

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
que `CFBundleShortVersionString` référence, donc lui que le Finder et la
fenêtre « À propos » annoncent.

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

### 5. Agrafer

    xcrun stapler staple HC.app

L'agrafage écrit le ticket DANS le bundle, ce qui permet à l'app de s'ouvrir
sur une machine hors ligne. Sans lui, Gatekeeper doit interroger Apple à la
première ouverture, et un utilisateur sans réseau est bloqué.

### 6. Le DMG

    hdiutil create -volname "HC <version>" -srcfolder HC.app \
      -ov -format UDZO HC-<version>.dmg

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

## Tant que la notarisation n'existe pas

Une version non notarisée se heurte à un message qui dit faux — *« HC est
endommagé et ne peut pas être ouvert »*. L'app n'est pas abîmée, elle n'est pas
notarisée. Le contournement, à donner aux utilisateurs :

    xattr -dr com.apple.quarantine /Applications/HC.app

C'est ce que porte la section « Installing » de la note de 0.6.9.4. Le jour où
la chaîne ci-dessus tourne, cette section disparaît des notes suivantes.
