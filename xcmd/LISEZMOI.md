# Les modules de HC : XCMD et XFCN en WebAssembly

HyperCard s'étendait par des XCMD (des commandes) et des XFCN (des
fonctions), écrits en C ou en Pascal et rangés dans la pile. HC fait de
même, avec une différence : le module est compilé en **WebAssembly**, et
tourne **enfermé** dans HC. Il ne touche que sa propre mémoire et les
rappels que HC lui tend : une pile qu'on vous partage ne peut rien faire de
plus, avec ses modules, qu'avec ses scripts.

Ce dossier est le kit pour en écrire.

| fichier | rôle |
| --- | --- |
| `hc_xcmd.h` | l'en-tête, sur le modèle de `HyperXCmd.h` d'Apple |
| `hc_xcmd.c` | ce que chaque module emporte : la mémoire, les chaînes, `snprintf` |
| `inc/` | les en-têtes C réduits qui vont avec |
| `exemples/bonjour.c` | le premier module : une commande et une fonction |
| `maccam/` | les XFCN de MAC•CAM Workshop, refaites : `FORTH` d'abord |
| `Makefile` | de `.c` à `.wasm` |

## Écrire un module

```c
#include <stdio.h>
#include "hc_xcmd.h"

HC_XCMD(bonjour)                       /* bonjour "Patricia" */
{
    char ligne[300];
    snprintf(ligne, sizeof ligne, "put \"Bonjour, %s !\"",
             paramPtr->paramCount > 0 ? paramPtr->params[0] : "le monde");
    SendCardMessage(ligne);
}

HC_XFCN(bonjour)                       /* put bonjour("Patricia") */
{
    paramPtr->returnValue = "Bonjour !";
}
```

`paramPtr` est le bloc de paramètres de HyperCard : `paramCount`,
`params[]`, `returnValue`, `passFlag`. Ce sont des chaînes C : plus de
`Handle` à verrouiller, plus de chaînes Pascal, plus de `PasToZero`.

Les rappels portent les noms de HyperCard :

| rappel | ce qu'il fait |
| --- | --- |
| `SendCardMessage(ligne)` | joue une ligne de HyperTalk sur la carte courante |
| `EvalExpr(expr)` | la valeur d'une expression, ou `NULL` |
| `GetGlobal(nom)`, `SetGlobal(nom, valeur)` | une variable globale |
| `GetFieldByName(carte, nom)`, `…ByNum`, `…ByID` | le texte d'un champ, ou `NULL` |
| `SetFieldByName(carte, nom, valeur)`, `…ByNum`, `…ByID` | l'écrire ; 0 s'il n'existe pas |

`carte` vaut 1 pour un champ de la carte courante, 0 pour un champ de son
fond : c'est le `cardFieldFlag` de HyperCard.

Ils jouent comme la boîte de messages, sur la carte courante, mais avec
les variables du gestionnaire qui a appelé le module : `EvalExpr("x")` lit
sa variable locale `x`.

Ce qu'ils rendent est une chaîne allouée par `malloc` — chez HyperCard, un
`Handle` qu'on jetait par `DisposHandle` : la rendre par `free`.

```c
HC_XFCN(aujourdhui)                    /* put aujourdhui() */
{
    static char s[100];
    char *d = EvalExpr("the long date");
    snprintf(s, sizeof s, "Nous sommes le %s.", d ? d : "?");
    free(d);
    paramPtr->returnValue = s;
}
```

Une globale posée par `SetGlobal` se lit dans un gestionnaire qui la
déclare, comme d'habitude : `global pln0`.

## Compiler

Il faut un `clang` qui connaisse la cible `wasm32`, avec son éditeur de liens
`wasm-ld`. Ni `emcc` ni JavaScript.

```
make
```

Sur macOS, le clang d'Apple (celui d'Xcode) n'a pas cette cible, à notre
connaissance. Celui d'emsdk l'a, et il tourne même sous Catalina (mesuré
le 16 octobre sous macOS 10.15.8, alors que `emcc` n'y tourne pas) :

```
make CLANG=~/emsdk/upstream/bin/clang
```

Le module sort à côté de son source : `exemples/bonjour.wasm`.

## Ce que le kit ne fait pas encore

- La mémoire va par tailles en puissances de deux : un bloc rendu par
  `free` resert à un bloc de la même classe, jamais à un autre.
- `snprintf` ne connaît que `%d %u %x %c %s %%` ; `strtol` et `atoi`
  pour les entiers, rien pour les nombres à virgule.
- Rien pour les fichiers.
- `SendHCMessage`, et les rappels de conversion (`StrToNum`,
  `PasToZero`…), que les chaînes C rendent inutiles, n'y sont pas.

Le pourquoi de tout cela, et les mesures : `docs/mesures/xcmd.txt`.
