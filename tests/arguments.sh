# Les harnais qui attendent un fichier en argument, et lequel.
#
# CE FICHIER EST PARTAGÉ, ET C'EST TOUT SON INTÉRÊT. lance.sh portait cette
# table ; releve.sh lançait les mêmes binaires SANS argument. Les harnais
# concernés sortaient aussitôt — « fichier introuvable » — sans rien exécuter,
# tout en étant comptés dans les « 197 harnais » du relevé. Mesuré : cinq
# programmes muets, dont test_exercice, le seul du corpus à exercer des
# fonctions utilisateur à un argument. Les sondes « aplat » et « carre »
# étaient visibles dans sa RÉFÉRENCE et absentes de l'AGRÉGAT.
#
# C'est la troisième fois que cet instrument mesure autre chose que ce qu'il
# annonce : l'armement depuis hc_set_host qui ne couvrait que 136 programmes
# sur 192, les binaires de harnais supprimés qui tournaient encore, et
# maintenant les arguments manquants. Chaque fois, le chiffre publié était
# trop flatteur. D'où une seule table, à un seul endroit.
arguments() {
  case "$1" in
    calreel)            echo "donnees/calendrier.txt" ;;
    navharn|navharn2|navharn3) echo "donnees/navtest.txt" ;;
    tortureh)           echo "donnees/torture_bouton.txt donnees/torture_pile.txt" ;;
    quelgest|analyse)   echo "donnees/rawchart.txt" ;;
    profond)            echo "donnees/endmanquant.txt" ;;
    # Trois harnais cherchaient leur donnée dans le répertoire courant : elle
    # est dans donnees/. Ils ne testaient donc plus rien — ils affichaient
    # « fichier introuvable », et leur fichier de référence enregistrait ce
    # message, si bien qu'ils passaient pour conformes.
    test_exercice|test_exercice2) echo "donnees/exercice.txt" ;;
    rendu)              echo "donnees/arcenciel.txt" ;;
    # Les deux bancs attendaient « draw.txt » depuis toujours, et il n'était
    # nulle part dans le dépôt : ils ne tournaient pas du tout.
    banc|banc_rom)      echo "donnees/draw.txt" ;;
    *)                  echo "" ;;
  esac
}
