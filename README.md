# HC

*[English version](README.en.md)*

**HC est HyperCard pour le Mac d'aujourd'hui.**

- **Compatible HyperTalk.** HC ouvre les piles HyperCard d'origine (le format
  binaire d'Apple, 1987–1998) et fait tourner leurs scripts **tels quels**,
  sans conversion ni réécriture. Sur un corpus de piles réelles, dont deux
  d'Apple, 99,7 % des scripts sont acceptés. Les écarts connus sont listés
  dans les [notes de version](docs/releases/).
- **Un sur-ensemble d'HyperCard, en couleur.** Tout ce qu'HyperCard sait
  faire, et la couleur en plus : peinture, icônes, et objets (`backColor`,
  `foreColor`, `textColor`).

Application macOS native, binaire universel, macOS 10.13 et plus.

## Installer

Télécharger le DMG de la dernière [version](https://github.com/cleobuline/hc/releases)
et copier `HC.app` dans `/Applications`. L'application n'est pas notariée ;
une seule fois :

```sh
xattr -dr com.apple.quarantine /Applications/HC.app
```

## Construire et tester

```sh
xcodebuild -project HC.xcodeproj -target HC -configuration Release build   # l'application, sur Mac
make test                                                                  # le noyau, partout
```

Le noyau et l'interpréteur sont en C99 et se testent aussi sous Linux :
plus de 260 harnais, comparés à HyperCard sous Basilisk II.
