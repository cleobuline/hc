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

https://github.com/user-attachments/assets/3f2150d7-665b-4b79-8978-b6f2b3017958

*Le flipper de démonstration, écrit en HyperTalk pur avec des boutons
polygones et `the keysDown`. Il est dans le DMG. À partir de la 0.7.9, chaque
obstacle porte son propre script : on en ajoute un en le copiant.*

## Installer

Télécharger le DMG de la dernière [version](https://github.com/cleobuline/hc/releases)
et copier `HC.app` dans `/Applications`. L'application n'est pas notariée ;
une seule fois :

```sh
xattr -dr com.apple.quarantine /Applications/HC.app
```

## Construire

Sur Mac, avec Xcode :

```sh
xcodebuild -project HC.xcodeproj -target HC -configuration Release build
```

## Tester sous Linux

Le noyau et l'interpréteur sont en C99 : plus de 260 harnais, comparés à
HyperCard sous Basilisk II, tournent sans Mac. Sur Ubuntu ou Debian :

```sh
sudo apt install build-essential zlib1g-dev git
git clone https://github.com/cleobuline/hc.git
cd hc
make test          # la suite de non-régression
make test-asan     # la même sous AddressSanitizer, UBSan et LeakSanitizer
```

`zlib1g-dev` est nécessaire : `build-essential` n'apporte pas `zlib.h`.
Détails dans [`tests/LISEZMOI.md`](tests/LISEZMOI.md).

## Licence

[MIT](LICENSE) : libre d'utiliser, de modifier et de redistribuer, à condition
de citer les auteurs, **Patricia Benedetto** et **Claude** (Anthropic).
