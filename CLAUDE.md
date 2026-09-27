# HC — notes de travail

## LE CÔTÉ COCOA SE VÉRIFIE, ET IL NE FAUT PAS S'EN PRIVER

Les quinze fichiers `.m` ne se compilent pas dans le conteneur : c'est une
machine Linux, sans SDK Cocoa ni AppKit. `xcodebuild` n'y changerait rien —
il lui faut macOS.

**Mais la machine macOS existe déjà**, dans l'intégration continue de ce
dépôt : le job « interface Cocoa » de `.github/workflows/noyau.yml` tourne
`xcodebuild` sur `macos-latest`, et il met une trentaine de secondes.

Donc, après toute modification d'un `.m` ou d'un `.h` d'interface :

1. pousser ;
2. `actions_list` / `list_workflow_jobs` sur la course déclenchée ;
3. lire la conclusion du job « interface Cocoa » AVANT d'annoncer que c'est
   fait ;
4. en cas d'échec, `get_job_logs` donne les erreurs du compilateur.

Ça a coûté trois allers-retours de ne pas y penser : `icon_couleur_de`,
`hc_icon_copie_dessin_hcicon` et `selectLine:inTextView:` étaient toutes trois
des références en avant, refusées par le seul compilateur qui ne tourne pas
ici, et trouvées par l'utilisatrice dans Xcode. Le compilateur était à une
minute de distance.

## LE NOYAU, LUI, SE VÉRIFIE ICI

    make verifie            le noyau compile
    make avertissements     huit familles d'avertissements, ZÉRO toléré
    ./tests/lance.sh        la suite de non-régression
    ./tests/lance.sh --asan la même sous ASan, UBSan et LeakSanitizer
    ./tests/lance.sh --enregistre   réenregistre les témoins
    ./tests/releve.sh       ce qui appelle encore l'ancien moteur

`--asan` PASSE AVANT DE POUSSER, et l'on attend son résultat. Les témoins se
réenregistrent après avoir LU le diff, jamais avant.

`make avertissements` compile en `-O2` et `lance.sh` en `-O1` : deux
`-Wformat-truncation` ne se voient qu'au second. Un avertissement qui dépend du
niveau d'optimisation reste un avertissement.

## LA MÉTHODE

Reproduire et MESURER avant de toucher au code. Une seule mesure sépare
rarement deux hypothèses ; deux mesures côte à côte, oui.

Un relevé qui CONFIRME ce que fait déjà le code mérite plus de méfiance qu'un
relevé qui le contredit : le second fait travailler, le premier fait conclure.
Et vérifier de quel côté vient un relevé — HyperCard ou HC — avant d'en tirer
quoi que ce soit.

Chercher le SITE JUMEAU de chaque correction. Une porte annoncée dans un
commentaire et jamais percée est le défaut signature de ce projet.

Ce qui n'est pas mesuré s'écrit comme non mesuré, dans le harnais et dans
`docs/mesures/`. Une supposition inscrite comme un fait coûte plus cher qu'un
trou.

Les messages de commit sont en français et nomment la cause, le symptôme et la
correction.
