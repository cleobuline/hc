# MAC•CAM Workshop dans HC

MAC•CAM Workshop est un atelier d'automates cellulaires, écrit au début des
années 90 avec THINK C, sur le modèle de la CAM-6 de Toffoli et Margolus
(*Cellular Automata Machines*, MIT Press, 1987). La pile tourne sous
HyperCard grâce à deux XFCN :

| XFCN | rôle | ici |
| --- | --- | --- |
| `FORTH` | compile les règles écrites en Forth, en tables | **`forth.c`** |
| `MACCAM` | la fenêtre de la machine, qui applique les tables | à venir |

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
bits de l'index dépend du voisinage déclaré. Il n'est documenté nulle part :
il est **mesuré** sur les 175 tables que la pile garde, faites par le FORTH
d'origine.

| voisinage | bits 0 → 11 |
| --- | --- |
| `N/MOORE` (et rien) | EAST S.EAST N.EAST CENTER SOUTH NORTH WEST S.WEST N.WEST CENTER' &CENTER' &CENTER |
| `N/VONN` | EAST' EAST CENTER' CENTER SOUTH' SOUTH NORTH' NORTH WEST' WEST &CENTER' &CENTER |
| `N/MARG-PH` | CCW' CCW OPP' OPP CW' CW CENTER' CENTER &CENTER' &CENTER PHASE PHASE' |
| `N/MARG-HV` | les mêmes, puis HORZ VERT |
| `N/MOORE-HV` | Moore, puis HORZ VERT |

Les fichiers :

| fichier | rôle |
| --- | --- |
| `forth_coeur.c` | le compilateur, en C ordinaire : il tourne dans le module comme dans le harnais `tests/harnais/forth.c` |
| `forth.c` | son passage dans HC : l'XFCN, et `SetGlobal` |

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
