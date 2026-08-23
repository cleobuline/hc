//
//  HCpaint.m
//  HC
//
//  Created by Patricia Benedetto on 21/08/2026.
//

#import "HCpaint.h"
#include <zlib.h>

/* Ramène une rep à un format canonique : RGBA 8 bits, non planaire,
   lignes compactes. Cocoa aligne parfois bytesPerRow sur 4 ou 16 octets ;
   sans ce passage, on compresserait le remplissage de fin de ligne et
   l'image relue serait décalée en diagonale. */
static NSBitmapImageRep *hcp_canonique(NSBitmapImageRep *src)
{
    NSInteger w = [src pixelsWide], h = [src pixelsHigh];

    if ([src bytesPerRow] == w * 4 && [src samplesPerPixel] == 4 &&
        [src bitsPerSample] == 8 && ![src isPlanar])
        return src;                      /* déjà bon : rien à faire */

    NSBitmapImageRep *dst =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                pixelsWide:w pixelsHigh:h
                                             bitsPerSample:8 samplesPerPixel:4
                                                  hasAlpha:YES isPlanar:NO
                                            colorSpaceName:NSDeviceRGBColorSpace
                                               bytesPerRow:w * 4 bitsPerPixel:32];
    NSGraphicsContext *ctx =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:dst];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [src drawInRect:NSMakeRect(0, 0, w, h)];
    [NSGraphicsContext restoreGraphicsState];
    return dst;
}


/* Calque -> HCP1 -> base64. Rend nil si la compression échoue. */
NSString *hcp_encode(NSBitmapImageRep *rep)
{
    if (!rep) return nil;
    NSBitmapImageRep *plan = hcp_canonique(rep);
    NSInteger w = [plan pixelsWide], h = [plan pixelsHigh];

    NSCAssert([plan bytesPerRow] == w * 4, @"hcp_encode : lignes non compactes");
    uLong  n_src = (uLong)(w * h * 4);
    uLongf n_dst = compressBound(n_src);

    NSMutableData *out = [NSMutableData dataWithLength:12 + n_dst];
    unsigned char *p = [out mutableBytes];

    memcpy(p, "HCP1", 4);
    p[4]  = (w >> 24) & 0xff;  p[5]  = (w >> 16) & 0xff;   /* gros-boutiste,  */
    p[6]  = (w >>  8) & 0xff;  p[7]  =  w        & 0xff;   /* écrit à la main */
    p[8]  = (h >> 24) & 0xff;  p[9]  = (h >> 16) & 0xff;
    p[10] = (h >>  8) & 0xff;  p[11] =  h        & 0xff;

    if (compress2(p + 12, &n_dst, [plan bitmapData], n_src, 9) != Z_OK)
        return nil;

    [out setLength:12 + n_dst];
    return [out base64EncodedStringWithOptions:0];
}


/* base64 -> calque. Accepte l'ancien PNG et le nouveau HCP1. */
NSBitmapImageRep *hcp_decode(NSString *b64)
{
    if (!b64 || ![b64 length]) return nil;

    NSData *d = [[NSData alloc]
        initWithBase64EncodedString:b64
                            options:NSDataBase64DecodingIgnoreUnknownCharacters];
    if ([d length] < 12) return nil;

    const unsigned char *p = [d bytes];

    if (memcmp(p, "\x89PNG", 4) == 0)              /* ancienne pile */
        return [NSBitmapImageRep imageRepWithData:d];

    if (memcmp(p, "HCP1", 4) != 0) return nil;     /* signature inconnue */

    NSInteger w = ((NSInteger)p[4]  << 24) | ((NSInteger)p[5]  << 16) |
                  ((NSInteger)p[6]  <<  8) |  (NSInteger)p[7];
    NSInteger h = ((NSInteger)p[8]  << 24) | ((NSInteger)p[9]  << 16) |
                  ((NSInteger)p[10] <<  8) |  (NSInteger)p[11];
    if (w <= 0 || h <= 0 || w > 20000 || h > 20000) return nil;

    NSBitmapImageRep *rep =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                pixelsWide:w pixelsHigh:h
                                             bitsPerSample:8 samplesPerPixel:4
                                                  hasAlpha:YES isPlanar:NO
                                            colorSpaceName:NSDeviceRGBColorSpace
                                               bytesPerRow:w * 4 bitsPerPixel:32];

    uLongf attendu = (uLongf)(w * h * 4), obtenu = attendu;
    if (uncompress([rep bitmapData], &obtenu, p + 12,
                   (uLong)([d length] - 12)) != Z_OK || obtenu != attendu)
        return nil;

    return rep;
}
