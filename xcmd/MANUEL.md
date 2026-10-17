# Votre premier XCMD pour HC, pas à pas

*[English version](MANUAL.md)*

De l'installation du compilateur jusqu'à un « Hello world » qui s'affiche
dans votre pile. Comptez une demi-heure la première fois, dont l'essentiel
à attendre un téléchargement.

Chaque commande de ce manuel a été jouée telle quelle avant d'être écrite.

## Ce qu'est un XCMD dans HC

Sous HyperCard, un XCMD (une commande) ou un XFCN (une fonction) était un
petit programme en C ou en Pascal, rangé dans la pile, que les scripts
appelaient comme un mot de HyperTalk.

HC fait la même chose, à une différence près : le programme est compilé en
**WebAssembly**. On obtient un fichier `.wasm`, qu'on ajoute à la pile et
qui voyage avec elle. Il tourne **enfermé** : il ne peut rien toucher sur
votre Mac, seulement parler à HC. On peut donc ouvrir sans crainte une pile
qu'on vous partage, même avec ses modules.

## Ce qu'il vous faut

- un Mac, avec macOS 10.15 (Catalina) ou plus récent ;
- de quoi télécharger plusieurs centaines de mégaoctets ;
- le dossier de HC, celui qui contient le dossier `xcmd`. Le DMG ne le
  contient pas — seulement l'application, des piles et ce manuel — : la fin
  de l'étape 2 dit comment l'obtenir.

Dans ce manuel, le dossier de HC s'appelle `~/hc` (le `~` veut dire « mon
dossier personnel »). Si le vôtre est ailleurs, remplacez `~/hc` par son
chemin. Astuce : dans le Terminal, glisser un dossier depuis le Finder y
écrit son chemin.

## Étape 1 — Ouvrir le Terminal

Applications ▸ Utilitaires ▸ **Terminal**. Une fenêtre s'ouvre, avec une
invite qui attend vos commandes.

Pour chaque commande de ce manuel : copiez-la, collez-la dans le Terminal
(⌘V), puis appuyez sur Retour. Attendez que l'invite revienne avant de
passer à la suivante.

## Étape 2 — Les outils de développement d'Apple

Ils apportent `git` et `make`. Si Xcode est déjà installé, ils le sont
sans doute déjà. Tapez :

```
xcode-select --install
```

Une fenêtre propose de les installer : acceptez. Si le Terminal répond
qu'ils sont déjà installés, c'est parfait, passez à la suite.

Si vous n'avez pas encore le dossier de HC, `git` vous l'apporte :

```
cd ~
git clone https://github.com/cleobuline/hc.git
```

Il arrive dans votre dossier personnel, sous le nom `hc` : c'est le `~/hc`
de ce manuel.

## Étape 3 — Installer le compilateur WebAssembly

Le compilateur `clang` d'Apple, celui d'Xcode, ne sait pas produire de
WebAssembly. On installe donc celui du projet **emsdk**, qui le sait. Il
tourne même sous Catalina : c'est mesuré, sur macOS 10.15.8.

Quatre commandes, l'une après l'autre :

```
cd ~
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
```

La dernière télécharge le compilateur : comptez plusieurs minutes. Puis :

```
./emsdk activate latest
```

Vérifiez que le compilateur est bien là, et qu'il connaît WebAssembly :

```
~/emsdk/upstream/bin/clang --print-targets | grep wasm32
```

Le Terminal doit répondre une ligne comme :

```
    wasm32      - WebAssembly 32-bit
```

C'est fait une fois pour toutes. Le compilateur est désormais
`~/emsdk/upstream/bin/clang`, et c'est ce chemin qu'on lui donnera.

## Étape 4 — Écrire « Hello world »

Le programme tient en six lignes. La façon la plus sûre de créer le
fichier est de coller ce bloc **entier** dans le Terminal :

```
cat > ~/hc/xcmd/exemples/hello.c <<'FIN'
#include "hc_xcmd.h"

HC_XCMD(hello)
{
    SendCardMessage("put \"Hello world !\"");
}
FIN
```

Ce que disent ces lignes :

- `#include "hc_xcmd.h"` : on se sert du kit de HC ;
- `HC_XCMD(hello)` : on déclare une commande nommée `hello` ; c'est le mot
  que les scripts emploieront ;
- `SendCardMessage(…)` : on fait jouer une ligne de HyperTalk à HC, ici
  `put "Hello world !"`, qui écrit dans la boîte de messages. C'est le même
  rappel que celui des XCMD de 1990.

Vous préférez un éditeur ? TextEdit convient, à une condition : passer le
document en texte brut (Format ▸ Convertir au format texte), sinon le
fichier ne sera pas lisible par le compilateur. Enregistrez-le sous le nom
`hello.c`, dans le dossier `xcmd/exemples` de HC.

## Étape 5 — Le compiler

```
cd ~/hc/xcmd
make CLANG=~/emsdk/upstream/bin/clang exemples/hello.wasm
```

Le Terminal affiche la commande qu'il exécute, une ligne qui commence par
le chemin du compilateur et finit par `hc_xcmd.c`. Si rien d'autre ne
s'affiche, c'est réussi : le module est né, à côté de son source.

```
exemples/hello.wasm
```

## Étape 6 — Ajouter le module à votre pile

Dans HC, ouvrez votre pile, puis **Objects ▸ Modules…**. Cliquez **Add…**
et choisissez `hello.wasm`, dans le dossier `xcmd/exemples` de HC.

Plus rapide : glissez `hello.wasm` depuis le Finder sur la fenêtre de la
pile.

Le panneau Modules montre alors le module, `hello`, et ce qu'il apporte —
sa taille dépend de la version du compilateur :

```
635 bytes, 1 external:

XCMD hello
```

## Étape 7 — L'essayer

Ouvrez la boîte de messages (⌘M, ou Tools ▸ Message), tapez `hello`,
puis Retour. Elle répond :

```
Hello world !
```

Et depuis un script, comme n'importe quelle commande, par exemple dans un
bouton :

```
on mouseUp
  hello
end mouseUp
```

## Étape 8 — Enregistrer la pile

Le module est rangé **dans la pile**, mais la pile n'est écrite sur le
disque que quand on l'enregistre : **File ▸ Save a Copy…** (⌘S). Malgré
son nom, cet article enregistre la pile elle-même, à l'endroit choisi. Le
module part avec elle : qui ouvre la pile ailleurs a le module, sans rien
installer.

## Aller un peu plus loin : un paramètre, et une fonction

Un XCMD reçoit les paramètres que le script lui passe, et un XFCN rend une
valeur. Collez ce bloc dans le Terminal :

```
cat > ~/hc/xcmd/exemples/salut.c <<'FIN'
#include <stdio.h>
#include <stdlib.h>
#include "hc_xcmd.h"

HC_XCMD(salut)
{
    char ligne[200];
    const char *qui = paramPtr->paramCount > 0 ? paramPtr->params[0]
                                               : "tout le monde";
    snprintf(ligne, sizeof ligne, "put \"Salut, %s !\"", qui);
    SendCardMessage(ligne);
}

HC_XFCN(double)
{
    static char resultat[20];
    int n = paramPtr->paramCount > 0 ? atoi(paramPtr->params[0]) : 0;
    snprintf(resultat, sizeof resultat, "%d", 2 * n);
    paramPtr->returnValue = resultat;
}
FIN
```

Puis compilez :

```
cd ~/hc/xcmd
make CLANG=~/emsdk/upstream/bin/clang exemples/salut.wasm
```

Ajoutez `salut.wasm` à la pile comme à l'étape 6, et dans la boîte de
messages :

| vous tapez | elle répond |
| --- | --- |
| `salut "Patricia"` | `Salut, Patricia !` |
| `salut` | `Salut, tout le monde !` |
| `put double(21)` | `42` |

`paramPtr` est le bloc de paramètres de HyperCard :

- `paramCount` : combien de paramètres ;
- `params[0]`, `params[1]`… : les paramètres eux-mêmes ;
- `returnValue` : ce que rend une fonction.

Ce sont des chaînes C ordinaires : plus de Handle ni de chaînes Pascal.

Un même fichier peut porter plusieurs commandes et fonctions, comme
`salut` et `double` ici.

**Après chaque modification du source**, recompilez, puis ajoutez de
nouveau le `.wasm` : il remplace celui du même nom.

## Quand ça ne marche pas

| ce que vous voyez | ce que ça veut dire |
| --- | --- |
| `git` ou `make` introuvables (« command not found »), ou une fenêtre qui propose d'installer des outils | les outils de l'étape 2 manquent : acceptez, ou refaites l'étape 2 |
| « no such file or directory » en tapant `./emsdk …` | vous n'êtes pas dans le dossier `emsdk` : tapez `cd ~/emsdk`, puis recommencez |
| un message qui parle de `wasm32` et de cible (« target ») | `make` a pris le clang d'Apple : il manque `CLANG=~/emsdk/upstream/bin/clang` |
| `No rule to make target` | le fichier `.c` n'est pas dans `xcmd/exemples`, ou son nom diffère |
| `error:` suivi d'une ligne de votre programme | une faute dans le source : le compilateur dit laquelle, et à quelle ligne |
| `warning:` | un avertissement, pas une erreur : le module est quand même fait, mais lisez-le |
| HC répond `personne ne répond à « hello »` | le module n'est pas dans **cette** pile : chaque pile a ses propres modules |
| HC refuse le module : `le module demande « … », que HC ne fournit pas` | le programme appelle une fonction que HC ne connaît pas ; ne vous servez que de celles du kit |
| la pile rouverte n'a plus le module | elle n'a pas été enregistrée après l'ajout (étape 8) |
| un module qui tourne sans fin | ⌘. l'arrête, comme un script |

## Pour aller plus loin

- `LISEZMOI.md`, dans ce même dossier : tout ce que le kit offre, avec les
  noms de HyperCard. Lire et écrire les champs et les variables globales,
  évaluer une expression, ouvrir une fenêtre, afficher une bulle d'aide…
- `exemples/bonjour.c` : une commande et une fonction du même nom.
- `exemples/ballons.c` et `exemples/fulldrag.c` : deux externes d'époque,
  refaites. Elles sont déjà construites dans `tests/donnees/modules/` :
  n'importe quelle pile peut s'en servir.
- `maccam/` : les externes de MAC•CAM Workshop, une pile de 1990 qui tourne
  dans HC sans qu'une ligne de ses scripts ait changé.
