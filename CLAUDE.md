# HC — notes de travail

## ON SE PARLE EN FRANÇAIS, TOUJOURS

Toutes les réponses à l'utilisatrice sont en FRANÇAIS — les explications, les
questions, les comptes rendus, les tableaux. Pas une phrase en anglais, même
après un long passage d'outils, même après un résumé de contexte : c'est
précisément là que les rechutes sont arrivées, plusieurs fois le 1er octobre,
et elles l'ont mise en colère à juste titre. Elle est plus Molière que
Shakespeare.

Les messages de commit sont en français aussi (voir plus bas). Seules les
notes de version, dans docs/releases/, s'écrivent en anglais.

## HC PARLE ANGLAIS À SES UTILISATEURS, SES SOURCES RESTENT EN FRANÇAIS

Décidé par l'utilisatrice le 17 octobre, après un commentaire public sur les
dialogues en français : tout ce que HC affiche — dialogues, alertes,
messages, menus — passe en anglais, lot par lot. Les commentaires des
sources restent en français, « que ça plaise ou non ». Le panneau « À
propos » reste bilingue, voulu ainsi. Les NSLog et les traces de mise au
point ne sont pas des textes d'utilisateur : ils restent en français.

Quand HC imite une erreur d'HyperCard, le bon texte anglais est celui
d'HyperCard, au mot près : il se mesure DANS HYPERCARD (Basilisk II), et
s'écrit « non mesuré » tant qu'il ne l'est pas.

## « the skipErrors » EST UN SECRET

Propriété propre à HC, ajoutée le 1er octobre à la demande de l'utilisatrice
(voir g_skip_errors dans hc_core.c et docs/mesures/erreur_abandon.txt) :
non posée, rien ne change ; « true » saute en silence les lignes fautives ;
« false » arrête tout le script comme HyperCard.

ELLE NE S'ANNONCE NULLE PART : ni dans les notes de version, ni dans une aide,
ni dans un README. C'est « notre petit secret », voulu ainsi. Le code, les
harnais et docs/mesures/ en parlent, et c'est tout.

## LES EXTENSIONS ANNONCÉES : LES BOUTONS POLYGONES, ET « the keysDown »

Décidé par l'utilisatrice le 2 octobre : HC reste un clone d'HyperCard, et
n'a qu'une extension annoncée, pour les jeux, à commencer par son flipper —
les boutons polygones et leurs couleurs. Le même soir, elle a accordé une
SECONDE pièce : « the keysDown », les touches tenues, sans quoi les batteurs
ne peuvent pas répondre pendant que la bille roule. Toutes deux s'annoncent
dans les notes de version.

HC portait déjà, avant elles, ses couleurs (textColor, la peinture, les
icônes en couleur), annoncées en 0.7.3.

Le vocabulaire est celui de LiveCode et SuperCard (style « polygon », « the
points », « the keysDown »), pour ne rien inventer. Les mots nouveaux n'existent que là où
HyperCard n'avait rien : aucun mot existant ne change de sens, et aucune pile
d'HyperCard ne change de comportement.

Toute autre extension se propose à l'utilisatrice, et ne se fait pas sans
elle. (skipErrors n'est pas concerné : il reste le petit secret ci-dessus.)

LE 11 OCTOBRE AU SOIR, ELLE A LEVÉ SA PROPRE RÈGLE (ci-dessous) POUR UNE
TROISIÈME PIÈCE, qu'elle a proposée elle-même : les couleurs des CHAMPS,
« pour être cohérent avec notre HC couleur ». backColor pour le fond,
foreColor pour le cadre, l'opacité en quatrième nombre (« r,v,b,alpha »),
EXACTEMENT comme les boutons — même stockage, mêmes noms de couleur, même
règle « rien de posé ». Rien pour le texte : textColor sur les morceaux le
fait déjà. Elle s'annonce dans les notes de version comme les deux autres.

LE 15 OCTOBRE AU SOIR, UNE QUATRIÈME PIÈCE, décidée par elle : les XCMD et
XFCN, refaits en WEBASSEMBLY, enfermés — jamais de code natif chargé à côté
d'une pile (un cheval de Troie, et le Hardened Runtime le refuse). Chantier
lancé au forfait du vendredi 16 octobre. Le juge : sa pile MAC•CAM Workshop,
écrite au début des années 90, tourne dans HC sans changer une ligne de ses
scripts. Tout est dans docs/mesures/xcmd.txt : le mesuré, les routes
écartées, l'ordre des marches. La pile elle-même, au format d'Apple, n'est
jamais commise.

Dans ce chantier, LE 17 OCTOBRE, elle a voulu les vraies bulles d'aide de
System 7 pour l'XCMD Ballons de sa pile — « ça servira à d'autres users
qui voudront mettre des bulles dans leurs piles » : les rappels
HMGetBalloons et HMShowBalloon, et un menu Help qui ne porte que « Show
Balloons ». Rien de neuf en HyperTalk.

ET IL N'Y EN AURA PAS D'AUTRE. Dit par l'utilisatrice le 2 octobre au soir :
après l'éditeur de polygone et l'alpha des couleurs de bouton (le chantier
noté dans docs/mesures/polygone.txt), plus aucune extension. La 0.7.8 doit
être une version de LONGUE DURÉE, proche du définitif : la suite est de la
fidélité à HyperCard et de la solidité, pas des nouveautés.

## LE CÔTÉ COCOA SE VÉRIFIE, ET IL NE FAUT PAS S'EN PRIVER

Les quinze fichiers `.m` ne se compilent pas dans le conteneur : c'est une
machine Linux, sans SDK Cocoa ni AppKit. `xcodebuild` n'y changerait rien —
il lui faut macOS.

**Mais la machine macOS existe déjà**, dans l'intégration continue de ce
dépôt : le job « interface Cocoa » de `.github/workflows/noyau.yml` tourne
`xcodebuild` sur `macos-latest`, et il met une trentaine de secondes.

Donc, après toute modification d'un `.m` ou d'un `.h` d'interface :

1. pousser ;
2. `actions_list` / `list_workflow_jobs` sur la course déclenchée ;
3. lire la conclusion du job « interface Cocoa » AVANT d'annoncer que c'est
   fait ;
4. en cas d'échec, `get_job_logs` donne les erreurs du compilateur.

Et lire le `conclusion` DU JOB, au sommet de la réponse. Chercher
`"status": "completed"` dans le texte brut tombe sur la première ÉTAPE
terminée — « Set up job » l'est au bout de deux secondes — et l'on croit le
job fini alors qu'il compile encore : `conclusion` vaut alors `null`, ce qui
ressemble à un échec sans en être un. Fait, et refait une fois de plus ici.

Ça a coûté trois allers-retours de ne pas y penser : `icon_couleur_de`,
`hc_icon_copie_dessin_hcicon` et `selectLine:inTextView:` étaient toutes trois
des références en avant, refusées par le seul compilateur qui ne tourne pas
ici, et trouvées par l'utilisatrice dans Xcode. Le compilateur était à une
minute de distance.

## LE NOYAU, LUI, SE VÉRIFIE ICI

    make verifie            le noyau compile
    make avertissements     onze familles, gcc ET clang, ZÉRO toléré
    make analyse            clang --analyze, ZÉRO toléré (1 min 20, aussi en CI)
    ./tests/lance.sh        la suite de non-régression
    ./tests/lance.sh --asan la même sous ASan, UBSan et LeakSanitizer
    ./tests/lance.sh --enregistre   réenregistre les témoins
    ./tests/releve.sh       ce qui relit encore du TEXTE (« v3 relit »)
    make fuzz               des piles abîmées par milliers, AVANT une version
                            (son témoin d'abord : docs/mesures/fuzzing.txt)

`--asan` PASSE AVANT DE POUSSER, et l'on attend son résultat. Les témoins se
réenregistrent après avoir LU le diff, jamais avant.

`make avertissements` compile AVEC LES DEUX COMPILATEURS, et ce n'est pas du
zèle non plus : l'utilisatrice a ouvert Xcode et y a lu SEIZE avertissements sur
du code que cette cible venait de déclarer propre — quinze « possible misuse of
comma operator » et un « may be uninitialized ». La cause n'était pas qu'il
manquait un compilateur : clang, avec nos drapeaux, trouvait zéro. Ces
familles-là — `-Wcomma`, `-Wconditional-uninitialized`, `-Wnewline-eof` — ne sont
ni dans `-Wall` ni dans `-Wextra` ; c'est Xcode qui les ajoute.

Une porte qui ne pose pas les mêmes questions que la machine de l'utilisatrice
n'est pas une porte, c'est une surprise, et elle arrive toujours du mauvais côté.

`make avertissements` compile aussi aux TROIS niveaux `-O0 -O1 -O2`, et ce n'est pas
du zèle : `-Wformat-truncation` a besoin de bornes que l'analyse de flot
propage différemment selon `-O`. « delete menu » sur un nom de plus de 63
caractères échouait en annonçant la réussite ; gcc le disait, à `-O0`
seulement, et la porte compilait en `-O2`.

Et un avertissement n'est pas une mesure : c'est une QUESTION posée au code.
J'ai annoncé trois fois « le nom d'un menu amputé en silence » sur la foi d'un
`-Wformat-truncation`, sans lire les huit lignes au-dessus où le refus est
écrit. Aller lire la réponse, toujours.

## LA MÉTHODE

Reproduire et MESURER avant de toucher au code. Une seule mesure sépare
rarement deux hypothèses ; deux mesures côte à côte, oui.

Un relevé qui CONFIRME ce que fait déjà le code mérite plus de méfiance qu'un
relevé qui le contredit : le second fait travailler, le premier fait conclure.
Et vérifier de quel côté vient un relevé — HyperCard ou HC — avant d'en tirer
quoi que ce soit.

### DEMANDER UN BANC EN DISANT DE QUEL CÔTÉ ON LE VEUT

Quatre fois un relevé non étiqueté a failli passer pour l'autre côté, et la
première fois ça m'a fait RETIRER une accusation qui était juste. La charge de
l'étiquette ne revient pas à l'utilisatrice : elle revient à celui qui demande
la mesure. Donc, en donnant un banc à jouer, écrire lequel des deux on veut, en
toutes lettres et dans la phrase même :

    « à jouer DANS HYPERCARD (Basilisk II) »      pour l'oracle
    « à jouer DANS HC (l'application) »           pour notre côté

Et quand un relevé arrive sans étiquette : demander, avant d'en tirer la
moindre conclusion. Le dernier, sur le curseur de « find » après un échec,
rendait « carte 1 » — ce que HC fait DÉJÀ. Pris pour HyperCard, il aurait clos
une question qu'il ne tranchait pas.

Chercher le SITE JUMEAU de chaque correction. Une porte annoncée dans un
commentaire et jamais percée est le défaut signature de ce projet.

Ce qui n'est pas mesuré s'écrit comme non mesuré, dans le harnais et dans
`docs/mesures/`. Une supposition inscrite comme un fait coûte plus cher qu'un
trou.

Les messages de commit sont en français et nomment la cause, le symptôme et la
correction.
