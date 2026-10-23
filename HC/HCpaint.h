#import <Cocoa/Cocoa.h>

/* Calque de peinture : sérialisation vers le noyau et retour.
   Écrit en HCP1 (RGBA compressé zlib), relit HCP1 et l'ancien PNG. */
NSString          *hcp_encode(NSBitmapImageRep *rep);
/* RGBA 8 bits, non planaire, lignes compactes : le format que tout le dessin
   suppose. nil si l'image est vide, démesurée, ou ne peut être allouée. */
NSBitmapImageRep  *hcp_canonique(NSBitmapImageRep *src);
NSBitmapImageRep  *hcp_decode(NSString *b64);
