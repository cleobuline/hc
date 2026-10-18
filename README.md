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
- **Des XCMD et des XFCN, enfermés.** Comme sous HyperCard, une pile
  s'étend par des commandes et des fonctions écrites en C ; HC les compile
  en WebAssembly, et elles tournent sans pouvoir toucher au Mac. Du
  compilateur à un premier « Hello world » : [`xcmd/MANUEL.md`](xcmd/MANUEL.md).

Application macOS native, binaire universel, macOS 10.13 et plus.

https://github.com/user-attachments/assets/3f2150d7-665b-4b79-8978-b6f2b3017958

*Le flipper de démonstration, écrit en HyperTalk pur avec des boutons
polygones et `the keysDown`. Il est dans le DMG. À partir de la 0.7.9, chaque
obstacle porte son propre script : on en ajoute un en le copiant.*

## Installer

Télécharger le DMG de la dernière [version](https://github.com/cleobuline/hc/releases)
et copier `HC.app` dans `/Applications`. L'application est  notariée vous n'avez rien d'autre a faire qu'acceper  ;
 
 
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

## Signaler un bug

Un bug, un plantage, ou une pile qui ne se comporte pas comme dans
HyperCard ? [Ouvrez une issue](https://github.com/cleobuline/hc/issues/new/choose) :
un formulaire est prévu pour chaque cas. Un script court, ou une petite pile
qui montre le problème, est le plus utile des rapports.

## Provenance et inspiration

**Comment HC a été fait.** HC est écrit par Patricia Benedetto, avec Claude
(Anthropic) pour co-auteur, depuis juillet 2026, comme le dit la
[LICENCE](LICENSE). Apple n'a jamais publié les sources d'HyperCard : il n'y
avait donc rien à copier. HC est une réimplémentation « en salle blanche »,
faite à partir de ce qu'HyperCard fait visiblement. Son comportement a été
mesuré détail par détail dans HyperCard 2.4.1, sous l'émulateur Basilisk II.
Les mesures, et ce qui n'est pas encore mesuré, sont écrites dans
[`docs/mesures/`](docs/mesures/), et depuis les premières semaines, les
messages de commit nomment la cause, le symptôme et la correction.

**Le format des piles d'Apple** a été lu dans deux descriptions publiques et
indépendantes : `StackFormat.md`, de HyperCardPreview (Pierre Lorenzi), et
`stakread.py`, de mystextract (Andrew Plotkin), confrontées l'une à l'autre et
à de vraies piles. Aucun des deux projets ne porte de licence : aucun code
n'en a été repris. Les offsets sont des faits sur un format, le code est celui
de HC. La compression des dessins, « Wrath of Bill Atkinson », a été
retrouvée et nommée par Rebecca Bettencourt.

**Le code tiers.** Une seule bibliothèque : WAMR, le WebAssembly Micro
Runtime de la Bytecode Alliance, sous licence Apache 2.0 avec les exceptions
LLVM. Il vit dans [`HC/tiers/wamr`](HC/tiers/wamr), avec sa licence et ses
attributions, qui voyagent aussi dans l'application.

**Ce qui n'y est pas.** Aucun code d'Apple, aucun en-tête d'Apple, aucune
pile d'Apple. HyperCard et HyperTalk sont des marques d'Apple Inc. ; HC n'est
ni affilié à Apple, ni approuvé par Apple.

**L'inspiration.** HyperCard, de Bill Atkinson (1987), et HyperTalk, de Dan
Winkler. Là où HC va plus loin qu'HyperCard (la couleur, les boutons
polygones, `the keysDown`, les couleurs des champs), les mots sont ceux de
LiveCode et SuperCard : rien n'est inventé, et aucune pile d'HyperCard ne
change de comportement. Le chantier des XCMD et XFCN a eu pour juge MAC•CAM
Workshop, la pile de Patricia de 1990, un atelier d'automates cellulaires
bâti sur la CAM-6 de Tommaso Toffoli et Norman Margolus (*Cellular Automata
Machines*, MIT Press, 1987). Elle tourne dans HC sans qu'une ligne de ses
scripts ait changé.

## Licence

[MIT](LICENSE) : libre d'utiliser, de modifier et de redistribuer, à condition
de citer les auteurs, **Patricia Benedetto** et **Claude** (Anthropic).
