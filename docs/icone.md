# L'icône de l'application

L'application embarque **`HC/AppIcon.icns`**, désigné par `CFBundleIconFile`
dans `Info.plist`. C'est le seul fichier d'icône qui compte au moment de la
compilation.

`HC/AppIcon.iconset/` est le dossier SOURCE, celui que `iconutil` lit pour
fabriquer l'`.icns`. Il n'est référencé nulle part dans le projet Xcode, et
`HC/Assets.xcassets/AppIcon.appiconset/` déclare dix emplacements sans y mettre
le moindre fichier — il ne produit donc rien.

## Deux tailles sont absentes, et c'est voulu

L'`iconset` ne contient pas `icon_16x16.png` ni `icon_32x32.png`, c'est-à-dire
les deux emplacements à l'échelle 1. L'`.icns` livré ne les contient pas non
plus : ses huit morceaux vont de `ic11` (32 px) à `ic10` (1024 px). macOS
réduit alors le 32 px pour les petites tailles, et l'icône est correcte
partout.

Ces deux fichiers ONT EXISTÉ, et ils étaient faux : deux carrés multicolores
au lieu de l'oiseau noir sur fond transparent. Mesuré — 115 et 891 couleurs
distinctes, zéro pour cent de pixels neutres, là où les huit autres sont à cent
pour cent de neutres. Des images étrangères glissées dans le lot.

Ils ne se voyaient pas, l'`.icns` livré ayant été fabriqué sans eux. Mais
c'était une porte ouverte : le jour où quelqu'un aurait relancé

    iconutil -c icns HC/AppIcon.iconset

`iconutil` les aurait pris sans broncher — ce sont des PNG valides, aux bonnes
dimensions — et le Finder aurait affiché un carré arc-en-ciel à petite taille.
Une régénération de routine, six mois plus tard, et personne pour faire le
lien. On les a donc retirés plutôt que laissés en embuscade.

## Si on veut un jour les deux petites tailles

Ne pas les fabriquer en réduisant le 256. **Une icône de seize pixels est un
dessin, pas une réduction** : à cette taille le trait fin de l'oiseau se ferme,
l'amande blanche de l'aile disparaît, et il ne reste qu'une tache. Elle se
redessine, avec moins de détails et des traits plus épais.

Tant que ce dessin n'existe pas, l'absence vaut mieux : la réduction que macOS
fait lui-même est réglée pour ça, et c'est ce qui est livré aujourd'hui.

## Régénérer l'icône

    iconutil -c icns HC/AppIcon.iconset

`iconutil` accepte un lot incomplet et n'ajoute que les emplacements présents.
Après quoi il faut vérifier ce qui est réellement entré dedans, plutôt que de
se fier au nom des fichiers.
