#ifndef HCtext_h
#define HCtext_h

#import <Cocoa/Cocoa.h>
#import "hc_core.h"

/* ═══ Police, style et geometrie du texte ════════════════════════════════════
 *
 * Ce que HCtext.m offre au reste de l'interface. Les fonctions absentes de
 * cette liste — font_with_traits, font_by_loose_name, run_base_font,
 * style_attrs, apply_run_style, apply_selection_highlight, utf16_from_byte —
 * restent privees : personne ne les appelle du dehors. */

/* Vrai pendant qu'on construit la chaine DESTINEE A L'EDITEUR. Voir le
 * commentaire de sa definition : le dessin et la NSTextView n'espacent pas
 * les lignes pareil, et field_attr_string a besoin de savoir lequel il sert. */
extern BOOL gForEditor;

/* ---- Police d'un objet ---- */
NSFont       *obj_base_font(Object *o, CGFloat defSize);
NSFont       *obj_font(Object *o, CGFloat defSize);
NSDictionary *obj_attrs(Object *o, CGFloat defSize, NSColor *color);

/* ---- Chaine attribuee d'un champ, avec ses plages de style ---- */
NSAttributedString *field_attr_string(Object *o, NSString *s, NSDictionary *at);

/* Mise en page d'un champ, conservée d'un redessin à l'autre. Rend le
 * disposeur et, par `conteneur`, le conteneur associé — de quoi ne tracer
 * que la plage de glyphes réellement visible. Voir HCtext.m. */
NSLayoutManager *field_layout(Object *o, NSString *s, NSDictionary *at,
                              CGFloat largeur, NSTextContainer **conteneur);
int                 style_bits_from_attrs(NSDictionary *a);

/* ---- Conversion d'index : octets UTF-8 <-> unites UTF-16 ---- */
int byte_from_utf16(NSString *s, NSUInteger u16);

/* ---- Geometrie : une seule source de verite pour le dessin et les clics ---- */
NSRect  field_text_rect(Object *o);        /* le rectangle de texte, sans defilement */
CGFloat field_max_scroll(Object *o);       /* ce qui depasse de la partie visible */
NSRect  field_text_draw_rect(Object *o);   /* le meme, decale du defilement courant */
CGFloat field_text_height(Object *o, NSRect tr);
void    field_clamp_scroll(Object *o);

#endif /* HCtext_h */
