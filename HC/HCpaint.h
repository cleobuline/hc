#import <Cocoa/Cocoa.h>

/* Calque de peinture : sérialisation vers le noyau et retour.
   Écrit en HCP1 (RGBA compressé zlib), relit HCP1 et l'ancien PNG. */
NSString          *hcp_encode(NSBitmapImageRep *rep);
NSBitmapImageRep  *hcp_decode(NSString *b64);
