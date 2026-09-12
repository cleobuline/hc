# La suite de non-régression

Chaque harnais de `harnais/` est un programme C autonome : il construit une
pile en mémoire, y fait tourner du HyperTalk, et écrit ce qu'il observe. Sa
sortie est comparée **octet pour octet** au fichier correspondant dans
`attendu/`.

Aucun ne touche à Cocoa. C'est délibéré : le noyau et l'interprète sont du
C99 pur, ils se compilent et s'exécutent partout, et c'est ce qui rend cette
suite utilisable en intégration continue.

## Lancer

```sh
make test              # depuis la racine du dépôt
tests/lance.sh         # ou directement
tests/lance.sh chunk   # seulement les harnais dont le nom contient « chunk »
make test-asan         # la même chose sous AddressSanitizer et UBSan
```

## Quand un test devient différent

`lance.sh` montre les premières lignes d'écart. Deux cas, et il faut les
distinguer avant de toucher à quoi que ce soit :

- **une régression** — le comportement a changé sans qu'on l'ait voulu. On
  corrige le code, pas la référence.
- **une amélioration voulue** — un message plus précis, un emprunt en moins,
  une forme nouvellement reconnue. On relit l'écart ligne à ligne, on vérifie
  qu'il ne contient QUE ce qu'on attendait, puis :

```sh
make test-enregistre
```

Cette commande réécrit toutes les références depuis l'état courant. **Ne la
lancez jamais sans avoir lu l'écart** : elle transforme un bogue en norme
aussi facilement qu'elle entérine un progrès.

## L'horloge est gelée

`lance.sh` pose `HC_HORLOGE=1757606400` et `TZ=UTC`. Sans cela, tout harnais
qui affiche `the date`, `the time` ou `the seconds` passerait aujourd'hui et
échouerait demain — c'est-à-dire ne pourrait pas être versionné.

La variable est lue par le noyau lui-même (`hc_maintenant`, dans
`hc_core.c`) : elle n'existe que pour les tests, et sans elle rien ne change.

## Les deux moteurs

L'ancien interprète de lignes n'est plus appelé ; `HC_AVEC_V1=1` le rebranche
entièrement. Pour comparer les deux :

```sh
HC_AVEC_V1=1 tests/lance.sh
```

Les sorties diffèrent alors sur les messages d'erreur — c'est normal, et
c'est même la mesure : ce qui change nomme exactement ce que l'ancien moteur
fait encore.

## Ajouter un harnais

Un fichier `harnais/<nom>.c`, un `main` qui installe un `HcHost` minimal,
construit une pile, envoie des messages, et imprime. Puis
`make test-enregistre` pour créer sa référence, et on relit cette référence
avant de la commiter — c'est elle qui fera foi ensuite.

Un harnais qui a besoin d'un fichier en argument s'ajoute à la fonction
`arguments()` de `lance.sh`, avec ses données dans `donnees/`.
