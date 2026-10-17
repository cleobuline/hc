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
| `exemples/ballons.c` | l'XCMD Ballons : une bulle d'aide au survol d'un bouton |
| `exemples/fulldrag.c` | l'XFCN FullDrag de Frédéric Rinaldi : glisser un contour gris |
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

## Une fenêtre

Comme les « XWindows » de HyperCard 2 : un externe ouvre une fenêtre par
`NewXWindow`, et les scripts la désignent ensuite par son titre.

```
get maccam("10,40,266,296", "CAM")
send "step" to window "CAM"
set the scale of window "CAM" to 2
put the phaseA of window "CAM"
close window "CAM"
```

Chacun de ces gestes revient à l'externe qui a ouvert la fenêtre, appelé
avec `paramCount` à -1 et, dans `params[0]`, un `XWEventInfo` :

```c
HC_XFCN(fenetre)                       /* put fenetre("Essai") */
{
    static unsigned char image[64 * 64], palette[768];
    if (paramPtr->paramCount == -1) {
        XWEventInfoPtr e = (XWEventInfoPtr)paramPtr->params[0];
        if (e->what == xSendEvt && !strcmp(e->eventParams[0], "dessine"))
            DrawXWindow(e->eventWindow, image, 64, 64, palette, 4);
        else
            paramPtr->passFlag = 1;    /* HC sert le reste : rect, loc… */
        return;
    }
    Rect r = { 40, 10, 296, 266 };     /* haut, gauche, bas, droite */
    static char s[16];
    snprintf(s, sizeof s, "%d", NewXWindow(&r, "Essai", 1, 17, 1, 1));
    paramPtr->returnValue = s;
}
```

| événement | quand | |
| --- | --- | --- |
| `xOpenEvt` | l'appel qui l'a ouverte est fini | |
| `xSendEvt` | `send … to window` | `eventParams[0]` : la ligne |
| `xSetPropEvt` | `set the … of window` | `[0]` la propriété, `[1]` la valeur |
| `xGetPropEvt` | `the … of window` | `[0]` la propriété ; `eventResult` : la valeur |
| `xCloseEvt` | `close window`, ou sa case | |

Ce que l'externe passe (`passFlag = 1`), HC le sert comme pour toute
fenêtre : `rect`, `loc`, `visible`, `name`, `width`, `height`. Il le sert
aussi quand le module lui-même, pendant qu'il tourne, pose une propriété de
sa fenêtre par `SendCardMessage`.

| rappel | ce qu'il fait |
| --- | --- |
| `NewXWindow(&rect, titre, visible, procID, couleur, flottante)` | l'ouvrir ; 0 si elle ne s'ouvre pas |
| `CloseXWindow(w)` | la fermer, sans `xCloseEvt` |
| `DrawXWindow(w, octets, largeur, hauteur, palette, echelle)` | son image : un octet par point, une palette de 256 couleurs RVB |
| `CopyXWindow(w)` | son image, telle qu'on la voit, au presse-papiers |
| `Button()` | le bouton de la souris est-il enfoncé — ou Cmd-. tapé ? |
| `GetCardPicture(&rect, noir)` | la peinture de la carte : 1 où l'encre est sombre |
| `StrToRect(texte, &rect)` | « gauche,haut,droite,bas », celui des scripts, en `Rect` |
| `DragGrayRgn(rects, n, depart, &limite, &marge, axe)` | le contour gris de rectangles suit la souris tant que le bouton est enfoncé ; rend le déplacement, `HiWord` vertical, `LoWord` horizontal |

HC garde l'image et la redessine lui-même : il n'y a pas d'événement
« update » à servir.

L'instance du module garde sa mémoire d'un appel à l'autre, et ses
fenêtres vivent autant qu'elle. Une faute, le module retiré ou la pile
fermée les ferment. Cmd-. aussi, sauf pour un module qui guette `Button` :
celui-là a une seconde pour rendre la main de lui-même, et garde ses
fenêtres.

## Les bulles d'aide

Celles de System 7, que l'utilisatrice allume par « Show Balloons », au
menu Help de HC :

| rappel | ce qu'il fait |
| --- | --- |
| `HMGetBalloons()` | les bulles sont-elles allumées ? |
| `HMShowBalloon(texte, &zone)` | une bulle pointée sur la souris, tant qu'elle reste dans `zone` (coordonnées carte) ; 0 si elles sont éteintes |

`exemples/ballons.c` est l'XCMD Ballons de MAC•CAM Workshop, refaite :

```
on mouseEnter
  ballons the rect of me, "Ouvre la fenêtre de la machine CAM"
end mouseEnter
```

Toute pile peut s'en servir : il suffit d'ajouter `ballons.wasm` à ses
modules. Il est déjà construit, dans `tests/donnees/modules/`.

## Glisser un contour

`exemples/fulldrag.c` est l'XFCN FullDrag de Frédéric Rinaldi (1990-1992),
refaite sur `DragGrayRgn` :

```
on mouseDown
  put fulldrag(the rect of me, the rect of this card) into delta
end mouseDown
```

Tant que le bouton est enfoncé, le contour gris suit la souris ; elle rend
le déplacement, « dh,dv ». `fulldrag.wasm` est construit, à côté de
`ballons.wasm`. Ses paramètres `<test>`, `<if true>` et `<if false>` ne
sont pas encore faits : ils rendent une erreur.

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
- Une fenêtre ne reçoit ni clic ni frappe : HC la déplace et la ferme
  lui-même, et le module ne voit que ses messages et ses propriétés.
  `GetNewXWindow`, `SetXWIdleTime` et `GetXWindowValue` n'y sont pas.

Le pourquoi de tout cela, et les mesures : `docs/mesures/xcmd.txt`.
