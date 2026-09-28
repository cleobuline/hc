# La suite de non-régression

Chaque harnais de `harnais/` est un programme C autonome : il construit une
pile en mémoire, y fait tourner du HyperTalk, et écrit ce qu'il observe. Sa
sortie est comparée **octet pour octet** au fichier correspondant dans
`attendu/`.

Aucun ne touche à Cocoa. C'est délibéré : le noyau et l'interprète sont du
C99 pur, ils se compilent et s'exécutent partout, et c'est ce qui rend cette
suite utilisable en intégration continue.

## Sur une machine neuve

La suite ne dépend de rien d'autre que du dépôt et d'un compilateur C. Elle a
été vérifiée sur un clone nu, sans réglage préalable.

```sh
sudo apt install build-essential git      # Ubuntu / Debian
git clone https://github.com/cleobuline/hc.git
cd hc/tests && ./lance.sh
```

Sans git, l'archive suffit — 1,1 Mo, contre 7 Mo pour le clone et son
historique :

```sh
curl -L https://github.com/cleobuline/hc/archive/refs/heads/main.tar.gz | tar xz
cd hc-main/tests && ./lance.sh
```

**Le dossier `tests/` ne se télécharge pas seul** : les harnais compilent le
noyau depuis `../HC`. C'est voulu — une suite qui testerait une copie figée du
code ne testerait plus le code. Il faut donc le dépôt, pas le dossier.

Rien à installer côté Cocoa : aucun harnais ne le touche, et c'est ce qui
permet de faire tourner la suite sous Linux alors que l'application est
macOS. `--asan` n'a pas de prérequis supplémentaire sur Ubuntu : les
bibliothèques des sanitizers viennent avec gcc.

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
`arguments()` de `arguments.sh` — la table est partagée entre `lance.sh` et
`releve.sh`, et c'est tout son intérêt : `releve.sh` lançait les mêmes
binaires sans argument, et cinq harnais y étaient comptés alors qu'ils
sortaient aussitôt. Ses données vont dans `donnees/`.

## Les deux piles de torture

`tortureh` et `torture2` ne sont pas des harnais comme les autres : leur
HyperTalk vit dans `donnees/`, pas dans le C. C'est voulu — ce sont des
centaines de lignes faites pour être relues, modifiées, et surtout
**recopiées telles quelles dans HyperCard** pour comparer les deux côtés. Un
script enfermé dans des guillemets C ne se recopie pas.

`torture2` sait en outre s'écrire sur le disque :

```sh
tests/.travail/bin/torture2 tests/donnees/torture2_bouton.txt \
                            tests/donnees/torture2_pile.txt  torture.stack
```

produit une pile qu'on ouvre dans l'application pour cliquer soi-même. Le
même programme sert donc au test de non-régression et à la pile livrée :
monter la pile deux fois aurait donné deux piles qui divergent au premier
changement.

Son rapport a **trois sortes de lignes**, et la troisième est la plus
importante :

| | |
|---|---|
| `ok` | l'essai rend ce qu'HyperCard a été mesuré rendre |
| `ECHEC` | il rend autre chose — les deux valeurs suivent |
| `?` | personne n'a mesuré ce cas : la valeur s'affiche **sans verdict** |

Une ligne `?` ne se compte ni en réussite ni en échec. Les compter serait
inscrire une supposition comme un fait — et c'est ce qui a coûté le plus cher
dans ce projet.

Sa **section 7** ne demande pas ce que fait HyperCard mais si les deux
tournures qui posent la même question rendent la même chose. Un écart y est un
défaut de HC quel que soit HyperCard, et c'est ainsi que
« select the foundChunk » a été trouvé.

### Rejouer le banc dans HyperCard

Il faut y recopier les deux scripts à la main, dans Basilisk. Ils font 1162
lignes, dont les deux tiers sont des commentaires — précieux ici, inutiles
là-bas : la pile d'HyperCard est un instrument jetable, on la monte, on relève,
on la jette.

```sh
tests/denude.sh tests/donnees/torture2_bouton.txt > bouton_nu.txt
tests/denude.sh tests/donnees/torture2_pile.txt   > pile_nu.txt
```

543 lignes au lieu de 1162, et le banc dénudé rend exactement la même chose —
vérifié, pas supposé.

Et quand une seule section pose question, `isole.sh` la met dans son propre
bouton, cinquante lignes, avec quatre marqueurs :

```sh
tests/isole.sh tests/donnees/torture2_bouton.txt sectionChemins > sept.txt
```

    aucun marqueur          le script de la PILE est en cause (journal)
    « le bouton demarre »   l'appel échoue, pas le contenu
    « on entre »            c'est dans la section, et le dernier « ok » dit où
    les quatre              la section est bonne : la GROSSE copie est abîmée

Ces deux outils existent pour la même raison : **un collage tronqué ressemble
trait pour trait à un défaut du code.** 291 lignes par le presse-papiers d'un
émulateur, et l'on cherche dans HC un « egal » qu'HyperCard refusait, alors que
la section n'avait jamais été collée en entier. Trois allers-retours perdus.

La pile à monter de l'autre côté : quatre cartes nommées `Atelier`, `Deux`,
`Trois`, `Quatre` ; un champ de fond `T` **non partagé** ; sur l'Atelier trois
champs de carte **dans cet ordre** — `A`, `B`, `R` —, `R` défilant et
`dontSearch` vrai ; un bouton portant le script du bouton.

Le banc vérifie tout cela lui-même au démarrage et le nomme dans un dialogue
s'il manque quelque chose. Il ne l'a pas toujours fait : une carte nommée
« troix » a produit trois échecs qui accusaient `find` et les cartes marquées.

## Lire une pile au format d'origine

`harnais/origine.c` exerce `HC/hc_origine.c`, qui extrait les SCRIPTS et les
noms d'une pile binaire écrite par HyperCard lui-même — pas notre format texte,
celui d'Apple, blocs `STAK` `BKGD` `CARD` `TAIL` en gros-boutiste.

Ce harnais MONTE une pile au format d'origine avant de la relire, et ça pose un
problème qu'il faut voir : mon écrivain et mon lecteur seraient d'accord parce
qu'ils partagent la même croyance, et se tromperaient ensemble en silence. Trois
contrepoids, aucun venant de moi — la somme de contrôle exigée par le format,
la table MacRoman produite par le codec de Python, et **un lecteur tiers** qui
relit les mêmes octets :

    cc -std=gnu99 -O1 -I HC -o /tmp/orig tests/harnais/origine.c \
       HC/hc_origine.c HC/hc_importe.c HC/hc_core.c HC/hc_file.c … -lm -lz
    /tmp/orig /tmp/synthetique.stack        # écrit le fichier
    python3 stakread.py -o - /tmp/synthetique.stack

`stakread.py` se prend dans le dépôt `erkyrath/mystextract`. Il doit rendre les
mêmes scripts, les mêmes noms et les mêmes ids. Il n'est pas versionné ici :
aucune licence ne l'accompagne, et on ne lui emprunte rien — c'est un ARBITRE,
le rôle que Basilisk joue pour le langage.

Ce qui est mesuré, ce qui ne l'est pas, et ce qu'il faudrait — une vraie pile de
1993, qu'aucun réseau atteignable d'ici ne fournit — est dans
`docs/mesures/pile_origine.txt`. Les identifiants de part et leur espace de noms
sont dans `docs/mesures/identifiants_de_part.txt`.

### Le dessin

La pile du harnais porte deux blocs `BMAP` écrits à la main, qui exercent tous
les codes du compactage WOBA — **y compris les deux que les vraies piles
n'emploient jamais**, la ligne noire et `dh=16`. Le harnais affiche les deux
plans en entier, les trois états distincts (`#` l'encre, `.` le blanc opaque,
l'espace le transparent) et les octets en hexadécimal à droite : un pixel qui
bouge se lit dans le diff de la référence.

Quatre lignes y sont transformées par `dh` et `dv`, et leur résultat attendu est
écrit EN DUR — calculé à part, depuis la phrase de la spécification, et non en
faisant tourner le décodeur. C'est le seul endroit du harnais où une valeur
attendue est écrite à la main, et la raison en est que l'oracle du format ne voit
pas ces deux transformations. Tout est dans `docs/mesures/dessins.txt`, y compris
le mot « Bienvenue » qu'Apple a peint en 1993 et qui sert de second juge.
