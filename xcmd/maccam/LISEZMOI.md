# MAC•CAM Workshop dans HC

MAC•CAM Workshop est un atelier d'automates cellulaires, écrit au début des
années 90 avec THINK C, sur le modèle de la CAM-6 de Toffoli et Margolus
(*Cellular Automata Machines*, MIT Press, 1987). La pile tourne sous
HyperCard grâce à deux XFCN :

| XFCN | rôle | ici |
| --- | --- | --- |
| `FORTH` | compile les règles écrites en Forth, en tables | **`forth.c`** |
| `MACCAM` | la fenêtre de la machine, qui applique les tables | sa machine : **`maccam_coeur.c`** ; sa fenêtre : à venir |

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

## Construire, et l'essayer

```
cd xcmd
make CLANG=~/emsdk/upstream/bin/clang
```

donne `maccam/forth.wasm`. Dans HC, sur la pile MAC•CAM Workshop :
Objects ▸ Modules… ▸ Add…, choisir `forth.wasm`. Puis, sur une carte de
l'atelier, le bouton qui compile (son ballon : « Permet de compiler le
programme FORTH ») : le champ « Pile » montre ce que rend FORTH, et les
champs Pln0 à PLN3 reçoivent les tables.
