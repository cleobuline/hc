# MAC•CAM Workshop dans HC

MAC•CAM Workshop est un atelier d'automates cellulaires, écrit au début des
années 90 avec THINK C, sur le modèle de la CAM-6 de Toffoli et Margolus
(*Cellular Automata Machines*, MIT Press, 1987). La pile tourne sous
HyperCard grâce à deux XFCN :

| XFCN | rôle | ici |
| --- | --- | --- |
| `FORTH` | compile les règles écrites en Forth, en tables | **`forth.c`** |
| `MACCAM` | la fenêtre de la machine, qui applique les tables | **`maccam.c`**, sur sa machine **`maccam_coeur.c`** |

Et deux externes de plus, refaites pour toutes les piles : l'XCMD
`Ballons`, les bulles d'aide de ses boutons (`../exemples/ballons.c`), et
l'XFCN `FullDrag`, qui fait glisser un carré de dessin sur un autre ou sur
la fenêtre CAM (`../exemples/fulldrag.c`).

Le but : la pile d'origine tourne dans HC **sans changer une ligne de ses
scripts**. La pile elle-même ne se commet pas ; ce qu'on y a mesuré est dans
`docs/mesures/xcmd.txt`.

## Le FORTH

```
global pln0, pln1, pln2, pln3
put FORTH(bg fld "forth") into bg fld "pile"
```

Comme l'original, la fonction compile le programme qu'on lui donne, range
chaque table qu'une `MAKE-TABLE` a remplie dans la globale `pln0` à `pln3`, et
rend ce qui reste sur sa pile, ou la faute qui l'a arrêtée.

Une **table** est un texte de 4 096 caractères « 0 » ou « 1 » : la nouvelle
valeur d'une cellule pour chacun des 4 096 états de son voisinage. L'ordre des
bits de l'index dépend du voisinage déclaré. Il n'est documenté nulle part
ailleurs que dans le code du FORTH d'origine, qui en porte les huit tables :
**lu** là, **mesuré** pour trois d'entre eux sur les 175 tables que la pile
garde, et retrouvé dans le moteur de MACCAM — trois sources d'accord.

| voisinage | `major` | bits 0 → 11 |
| --- | --- | --- |
| `N/MOORE` (et rien) | 0 | EAST S.EAST N.EAST CENTER SOUTH NORTH WEST S.WEST N.WEST CENTER' &CENTER' &CENTER |
| `N/VONN` | 1 | EAST' EAST CENTER' CENTER SOUTH' SOUTH NORTH' NORTH WEST' WEST &CENTER' &CENTER |
| `N/MARG-PH` | 2 | CCW' CCW OPP' OPP CW' CW CENTER' CENTER &CENTER' &CENTER PHASE PHASE' |
| `N/MARG-HV` | 3 | les mêmes, puis HORZ VERT |
| `N/MOORE-PH` | 4 | Moore, puis PHASE PHASE' |
| `N/MOORE-HV` | 5 | Moore, puis HORZ VERT |
| `N/VONN-PH` | 6 | von Neumann, puis PHASE PHASE' |
| `N/VONN-HV` | 7 | von Neumann, puis HORZ VERT |

Les fichiers :

| fichier | rôle |
| --- | --- |
| `forth_coeur.c` | le compilateur, en C ordinaire : il tourne dans le module comme dans le harnais `tests/harnais/forth.c` |
| `forth.c` | son passage dans HC : l'XFCN, et `SetGlobal` |
| `maccam_coeur.c` | la machine de la fenêtre CAM, en C ordinaire, éprouvée par `tests/harnais/maccam.c` |
| `maccam.c` | la fenêtre CAM dans HC : l'XFCN, ses propriétés, ses messages |

## La machine

`maccam_coeur.c` est la machine de MACCAM, traduite de son code 68000 (lu et
désassemblé ; rien n'en a été exécuté dans HyperCard pour comparaison) :

- une grille de pixels de 8 bits — plan 0 sur le bit 7, plan 3 sur le bit 4 —,
  dont la bordure d'un pixel est refaite après chaque pas par le côté opposé :
  un tore ;
- deux demi-machines : CAM-A calcule les plans 0 et 1 par sa table (`pln7`,
  `pln6`), CAM-B les plans 2 et 3 (`pln5`, `pln4`) ; chacune lit le centre
  de l'autre comme « & » ;
- les huit balayages de `major`, dans l'ordre de bits des tables du FORTH ;
  les phases basculent après chaque pas ;
- `fill`, `erase`, `image`, et la palette du moteur MacCam moderne (plan 0
  blanc, plan 1 cyan, CAM-B jaune, orange, vert) : les palettes clut de la
  pile ne sont pas reprises — décidé par l'utilisatrice.

Son harnais, `tests/harnais/maccam.c`, la fait tourner avec le FORTH : le
planeur de Conway avance d'une case tous les quatre pas et revient à sa
place après un tour du tore.

## La fenêtre

```
get maccam(the rect of bg btn "startup", "CAM", "True", 17, "True", "True")
set the pln7 of window "CAM" to bg fld "pln0"
set the image7 of window "CAM" to the rect of bg btn "pln7"
send stepfast to window "CAM"
```

`maccam.c` refait l'XFCN d'origine, lue dans son code : ses six paramètres
(le rectangle de la fenêtre sur l'écran, son titre, visible, son style, en
couleur, flottante) et leurs défauts, ses trente propriétés, ses dix-sept
messages.

| messages | |
| --- | --- |
| `step` | un pas, selon `major` |
| `stepfast`, `stepmfast`, `stepvfast` | des pas jusqu'au clic — ou Cmd-. — : selon `major`, en Margolus, en von Neumann |
| `fill1` à `fill8`, `erasea`, `erase1`, `erase2` | allumer un bit partout, tout éteindre, éteindre les bits 0 ou 1 |
| `blacka` | rien, comme l'original |
| `toscrap` | la fenêtre au presse-papiers |

| propriétés | |
| --- | --- |
| `pln7` à `pln4` | les tables de CAM-A (plans 0 et 1) et de CAM-B (plans 2 et 3) |
| `image7` à `image0` | la peinture de la carte dans un rectangle, posée sur un plan |
| `major`, `alternate`, `phaseA`, `phaseB`, `minor` | le voisinage, le découpage de Margolus, les phases |
| `scale` | l'agrandissement ; la fenêtre se refait carrée |
| `PictResource` | une photographie de la fenêtre au presse-papiers |
| `palette` | acceptée, sans effet : la fenêtre garde sa palette |

Ce qui diffère de l'original, décidé par l'utilisatrice : les palettes clut
de la pile ne sont pas reprises, et `PictResource`, qui écrivait une
ressource PICT dans la pile, photographie la fenêtre. Comme l'original,
`the image0` et `the image1` relisent les tables, et `the phaseA` fait bip.

## L'essayer

Les quatre modules sont déjà construits, dans le dépôt :

- `tests/donnees/modules/forth.wasm`
- `tests/donnees/modules/maccam.wasm`
- `tests/donnees/modules/ballons.wasm`
- `tests/donnees/modules/fulldrag.wasm`

Dans HC, sur la pile MAC•CAM Workshop : Objects ▸ Modules… ▸ Add…, choisir
les quatre. Les bulles des boutons s'allument par Help ▸ Show Balloons. Puis, sur une carte de l'atelier, le bouton qui compile (son
ballon : « Permet de compiler le programme FORTH ») : le champ « Pile »
montre ce que rend FORTH, et les champs Pln0 à PLN3 reçoivent les tables.
Le bouton qui ouvre la fenêtre CAM (« Ouvre la fenêtre le la machine
CAM ») l'ouvre ; les autres la chargent et la font tourner.

## Les reconstruire

Seulement après avoir changé leurs sources. Il faut un clang qui connaisse
la cible WebAssembly — sur macOS, celui d'emsdk (voir `../LISEZMOI.md`) :

```
cd xcmd
make CLANG=~/emsdk/upstream/bin/clang
```

donne `maccam/forth.wasm` et `maccam/maccam.wasm`, à côté de leurs sources ;
`tests/donnees/modules/refais.sh` les recopie là où la suite de tests les
lit.
