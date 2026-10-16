/* hc_xcmd.h — écrire un XCMD ou un XFCN pour HC
 *
 * Sur le modèle de HyperXCmd.h, l'en-tête d'Apple pour les externes de
 * HyperCard : le même bloc de paramètres, les mêmes noms de rappels. Ce qui
 * change : le module est compilé en WebAssembly et tourne ENFERMÉ dans HC —
 * il ne touche que sa propre mémoire et les rappels ci-dessous ; les
 * paramètres sont des chaînes C, plus des Handle ni des chaînes Pascal ; un
 * rappel s'appelle directement, plus par un saut à entryPoint avec un
 * numéro de requête.
 *
 *     #include "hc_xcmd.h"
 *
 *     HC_XCMD(bonjour)                     bonjour "Patricia"
 *     {
 *         SendCardMessage("beep");
 *     }
 *
 *     HC_XFCN(double)                      put double(21)
 *     {
 *         paramPtr->returnValue = "42";
 *     }
 *
 * Le nom entre parenthèses est celui qu'appelle HyperTalk. Un même module
 * peut en porter plusieurs, commandes et fonctions mêlées. Voir LISEZMOI.md
 * pour compiler, et exemples/bonjour.c. */

#ifndef HC_XCMD_H
#define HC_XCMD_H

/* LE BLOC DE PARAMÈTRES. Ses champs sont ceux de HyperCard, dans le même
 * ordre, jusqu'à passFlag ; la suite du bloc d'Apple (entryPoint, request,
 * inArgs…) servait à appeler les rappels et n'a plus lieu d'être.
 *
 * HC le remplit avant l'appel, dans la mémoire du module. Les chaînes qu'il
 * y pose ne vivent que le temps de l'appel : les recopier pour les garder. */
typedef struct XCmdBlock {
    short          paramCount;   /* 0 à 16 */
    char          *params[16];   /* params[0] est le PREMIER paramètre */
    char          *returnValue;  /* XFCN : la valeur rendue ; NULL : vide */
    unsigned char  passFlag;     /* 1 : HC poursuit sa recherche, comme « pass » */
} XCmdBlock, *XCmdPtr;

/* HC lit ce bloc à des places fixes : elles ne doivent pas bouger. */
_Static_assert(sizeof(void *) == 4, "un module HC se compile pour wasm32");
_Static_assert(__builtin_offsetof(XCmdBlock, params) == 4, "params à +4");
_Static_assert(__builtin_offsetof(XCmdBlock, returnValue) == 68, "returnValue à +68");
_Static_assert(__builtin_offsetof(XCmdBlock, passFlag) == 72, "passFlag à +72");

/* LES RAPPELS — ce que HC tend au module, et RIEN d'autre.
 *
 * Le module les importe du module « hc » ; HC refuse de charger un module
 * qui en demande un qu'il ne connaît pas, plutôt que de le laisser tomber au
 * premier appel. Les autres rappels de HyperCard (EvalExpr, GetGlobal,
 * SetGlobal, GetFieldByName…) viendront un à un, sous les mêmes noms. */
#define HC_RAPPEL(nom) __attribute__((import_module("hc"), import_name(#nom)))

/* Une ligne de HyperTalk, jouée sur la carte courante comme si on la
 * tapait dans la boîte de messages : « put "x" », « go next », « beep ». */
HC_RAPPEL(SendCardMessage) void SendCardMessage(const char *msg);

/* DÉCLARER UN XCMD OU UN XFCN.
 *
 * Chacun devient une fonction exportée sous le nom « XCMD bonjour » ou
 * « XFCN bonjour » : c'est par ce nom que HC sait ce qu'apporte un module. */
#define HC_XCMD(nom) HC_ENTREE_(XCMD, nom)
#define HC_XFCN(nom) HC_ENTREE_(XFCN, nom)

#define HC_ENTREE_(genre, nom)                                           \
    static void genre##_##nom(XCmdPtr paramPtr);                         \
    __attribute__((export_name(#genre " " #nom)))                        \
    void hc_entree_##genre##_##nom(XCmdPtr p);                           \
    void hc_entree_##genre##_##nom(XCmdPtr p) { genre##_##nom(p); }      \
    static void genre##_##nom(XCmdPtr paramPtr)

#endif
