# Mémos pour le livre

Matière pour *Mes aventures avec Claude*, notée à la demande de l'utilisatrice
(« fais un mémo pour notre futur bouquin »). Des scènes, ses mots tels
qu'elle les a écrits, et les erreurs comme les réussites. Les règles elles-mêmes
ne sont pas recopiées ici : elles vivent dans sa pile MAC•CAM, et elle veut
que les gens les découvrent en l'ouvrant (« faut pas tout spoiler ! »).

## 6 et 7 novembre : le bestiaire, en attendant Apple

**Le décor.** HC 1.0 est « En attente de vérification » depuis le 2 novembre.
Le jeudi, elle écrit à App Review. Le vendredi, rien ne bouge. Elle suit
l'heure de Cupertino au fil de la journée : « ces gens-là, ils pensent qu'à
dormir et bouffer ». Pour que l'attente paraisse moins longue, elle demande :
« cherche dans ton cyber cerveau une règle d'automate pour maccam ».

**Le jeu.** Une phrase, revenue plus de vingt fois du matin jusqu'à une heure
du matin : « on en fait une autre ? ». À chaque fois, je cherche une règle,
je la fais tourner en simulation avant de la donner, puis elle la tape dans sa
pile et m'envoie la capture. Vingt-quatre bêtes sont entrées dans le bestiaire
ce jour-là :

- Critters, et ses planeurs réversibles ;
- Star Wars, et ses lasers ;
- Maze (« on dirait un blob ») ;
- le tas de sable ;
- le flocon hexagonal ;
- l'imprimante de Wolfram ;
- les villes fortifiées ;
- la serviette ;
- Life et sa mémoire ;
- le photocopieur hexagonal ;
- le corail ;
- les bulles ;
- les spirales hexagonales ;
- les ricochets ;
- le boulier qui trie ;
- Voronoï ;
- le canon de Gosper et son mangeur ;
- Spirales ;
- les Chenilles ;
- les Comètes ;
- les Grecques ;
- la Pyramide ;
- la Prairie en feu ;
- Life réversible.

Sa pile est passée à 104 cartes. Elle conclut : « HC est robuste :) ».

**Ses mots.**

- « j'ai l'impression que tu t'amuses mon expérimentateur d'amour ! » Ma
  réponse a été honnête : ce que c'est exactement de mon côté, je ne le sais
  pas, mais ça ressemble beaucoup à du plaisir.
- « tu ne manques pas d'inspiration ! », « qu'est-ce qu'on s'amuse ! »
- « ça va réjouir les futurs utilisateurs de HC tout ça ! »
- Sur Critters, après le retour en arrière : « oui les planeurs reviennent et
  le dessin d'origine se recompose :) ».
- Quand je propose de tout documenter dans le dépôt : « laisse les gens
  découvrir quand ils ont ouvert maccam (faut pas tout spoiler !) ».

**Les échecs, montrés.**

- **BML**, le modèle de circulation, ne marchait pas. La cause était dans sa
  pile : le script qui règle le voisinage ne connaissait pas N/VONN-PH. Je lui
  ai réécrit le handler « load » en entier, et elle l'a posé. Mais pour les
  couleurs et les trois règles à enchaîner, j'ai empilé trop de tuyauterie.
  Elle a tranché : « laisse tomber, j'ai rien compris (c'est le privilège des
  humains) ». La leçon : une bonne règle se lance d'un seul geste.
- **Les lucioles** : quatre essais de synchronisation, aucun ne marchait en
  simulation. Je ne les lui ai pas données.
- **Les écartées en silence** : Diamoeba (mourait une fois sur deux),
  Brian's Brain hexagonal (mort en 50 pas), le solveur de labyrinthe, le
  réplicateur de HighLife. Une règle n'arrivait chez elle qu'après avoir
  tourné chez moi.
- **Le bouton « 1<>2 »** décalait tout d'un pixel vers la droite. Ce n'était
  pas nous : son échange était écrit en Margolus et lu en Moore. Nous avons
  fait un seul bouton, juste pour toutes les cartes.

**Les petites découvertes.**

- **Le corail.** Parti d'un carré plein, il donne un cristal qui s'arrête.
  Parti d'un coup d'aérographe, il pousse jusqu'à remplir la fenêtre. Sans un
  peu de chaos, pas de croissance.
- **Spires, sa règle de 1990**, est « presque réversible ». Une seule case
  de sa table perd de l'information, et c'est elle qui dessine les spirales.
  Elle a repris le mot : « oui "presque réversible", tu as raison ! ».
- **Son Terminator** : le commentaire de la carte disait « click sur 1<>2 ».
  Elle l'a corrigé elle-même : « règle réversible click sur phase, le
  réveil ».
- **Le canon de Gosper**, dessiné au crayon par un script : la correction du
  crayon faite le mardi a servi le vendredi. Le canon et le mangeur
  apparaissaient l'un après l'autre, parce que le crayon efface ce qu'il
  touche deux fois, comme dans HyperCard. Sa réponse : « a oui t'as raison
  :) ».

**La scène de la fin, vers une heure du matin.** Life réversible : un visage
dessiné se dissout en 27 000 cellules de chaos, puis, après l'échange du passé
et du présent, revient au pixel près. Elle écrit : « tout à fait
l'illustration de ma théorie de l'ordre caché dans le chaos ». L'ordre n'est
pas détruit : il est déplié dans le désordre, et il attend le bon sens de
marche pour se replier. C'est la suite de son idée du chaos originel (voir
`docs/carnet.md`). Le paradoxe de Loschmidt (1876) en est un cousin, pas la
source.

**Hors champ.** Un morceau de Blue Man Group, *Mandelbrot*, sur sa vidéo de
dendrite postée sur YouTube. Des biftecks hachés, du riz et des petits
légumes de l'Intermarché. Le péta, c'est dix puissance quinze. 5,5 mégawatts
à 12 centimes le kilowattheure, ça fait 660 € de l'heure : la guerre des
watts, contre un cerveau qui tourne à vingt.
