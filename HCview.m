#import "HCview.h"
#import "hc_core.h"

typedef enum { TOOL_BROWSE, TOOL_BUTTON, TOOL_FIELD } HCTool;
static HCTool gTool = TOOL_BROWSE;
static Object *gSelected = NULL;   // objet sélectionné en mode édition

HCView *gView = nil;               // la vue courante
static NSTextField *gMsgBox = nil; // la message box

static NSPoint gDragStart;
static NSRect  gDragRect;
static BOOL    gDragging = NO;
static int     gNewCount = 0;
static Object *gEditTarget = NULL;
static NSTextView *gEditView = nil;
static NSPanel *gEditPanel = nil;
static Object *gPressed = NULL;
// dessine un objet (bouton ou champ) à son rectangle
static void draw_part(Object *o) {
    if (!o->visible) return;

    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);

    if (o->type == OBJ_BUTTON) {
        [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
        NSRectFill(r);
        [[NSColor blackColor] setStroke];
        NSFrameRect(r);
        const char *nm = o->name ? o->name : "";
        NSString *s = [NSString stringWithUTF8String:nm];
        [s drawAtPoint:NSMakePoint(o->x + 8, o->y + o->h/2 - 8) withAttributes:nil];
    }
    else if (o->type == OBJ_FIELD) {
            [[NSColor colorWithWhite:0.97 alpha:1.0] setFill];
            NSRectFill(r);
            [[NSColor colorWithWhite:0.4 alpha:1.0] setStroke];
            NSFrameRect(r);
            const char *tx = o->contents ? o->contents : "";
            NSString *s = [NSString stringWithUTF8String:tx];
            [s drawInRect:NSInsetRect(r, 4, 4) withAttributes:nil];
        }
}

// trouve la part (bouton/champ) contenant le point, carte puis fond
static Object *part_at(Object *card, NSPoint p) {
    if (!card) return NULL;
    for (int i = card->nparts - 1; i >= 0; i--) {
        Object *o = card->parts[i];
        if (o->visible &&
            p.x >= o->x && p.x <= o->x + o->w &&
            p.y >= o->y && p.y <= o->y + o->h)
            return o;
    }
    if (card->bg)
        for (int i = card->bg->nparts - 1; i >= 0; i--) {
            Object *o = card->bg->parts[i];
            if (o->visible &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
    return NULL;
}

// reçoit toute sortie du noyau
static void cocoa_line(HcLineKind kind, int depth, const char *text) {
    (void)depth;
    if (kind == HC_MSG && gMsgBox) {
        [gMsgBox setStringValue:[NSString stringWithUTF8String:text]];
    } else {
        NSLog(@"%s", text);
    }
}

static void cocoa_field_changed(Object *field) {
    (void)field;
    [gView setNeedsDisplay:YES];
}

@implementation HCView

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);

    Object *card = hc_current_card();
    if (!card) return;

    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++)
            draw_part(card->bg->parts[i]);

    for (int i = 0; i < card->nparts; i++)
        draw_part(card->parts[i]);

    if (gSelected) {
        NSRect r = NSMakeRect(gSelected->x, gSelected->y, gSelected->w, gSelected->h);
        [[NSColor redColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(r, -2, -2)];
        [path setLineWidth:2];
        [path stroke];
    }

    if (gDragging) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:gDragRect];
        [path setLineWidth:1];
        CGFloat dash[] = {4, 3};
        [path setLineDash:dash count:2 phase:0];
        [path stroke];
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    Object *hit = part_at(hc_current_card(), p);

    // double-clic en mode édition : éditer le script
    if (gTool != TOOL_BROWSE && hit && [event clickCount] == 2) {
        [self editScriptOf:hit];
        return;
    }

    if (gTool == TOOL_BROWSE) {
        if (hit) {
            gPressed = hit;
            hc_send(hit, "mouseDown");
            [self setNeedsDisplay:YES];
        }
        
        return;
    }

    // mode bouton/champ
    if (hit) {
        gSelected = hit;
        gDragging = NO;
    } else {
        gSelected = NULL;
        gDragStart = p;
        gDragRect = NSMakeRect(p.x, p.y, 0, 0);
        gDragging = YES;
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event {
    if (!gDragging) return;
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    CGFloat x = MIN(gDragStart.x, p.x);
    CGFloat y = MIN(gDragStart.y, p.y);
    CGFloat w = fabs(p.x - gDragStart.x);
    CGFloat h = fabs(p.y - gDragStart.y);
    gDragRect = NSMakeRect(x, y, w, h);
    [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)event {
    // --- mode flèche : envoyer mouseUp au script ---
    NSLog(@"mouseUp appelé, gTool=%d gPressed=%s", (int)gTool, gPressed ? "OUI" : "non");
    if (gTool == TOOL_BROWSE) {
        if (gPressed) {
            NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
            Object *hit = part_at(hc_current_card(), p);
            if (hit == gPressed)
                hc_send(gPressed, "mouseUp");
            gPressed = NULL;
            [self setNeedsDisplay:YES];
        }
        return;
    }

    // --- mode édition : finir la création par drag ---
    if (!gDragging) return;
    gDragging = NO;
    if (gDragRect.size.width < 8 || gDragRect.size.height < 8) {
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
    if (!card) return;
    char name[64];
    Object *o;
    if (gTool == TOOL_BUTTON) {
        snprintf(name, sizeof name, "Bouton %d", ++gNewCount);
        o = hc_new_button(card, name);
    } else {
        snprintf(name, sizeof name, "Champ %d", ++gNewCount);
        o = hc_new_field(card, name);
    }
    o->x = (int)gDragRect.origin.x;
    o->y = (int)gDragRect.origin.y;
    o->w = (int)gDragRect.size.width;
    o->h = (int)gDragRect.size.height;
    gSelected = o;
    [self setNeedsDisplay:YES];
}
- (void)installMessageBox {
    gView = self;
    NSRect b = [self bounds];
    gMsgBox = [[NSTextField alloc] initWithFrame:NSMakeRect(10, b.size.height - 34, b.size.width - 20, 24)];
    [gMsgBox setEditable:YES];
    [gMsgBox setSelectable:YES];
    [gMsgBox setBezeled:YES];
    [gMsgBox setDrawsBackground:YES];
    [gMsgBox setBackgroundColor:[NSColor colorWithWhite:0.96 alpha:1.0]];
    [gMsgBox setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [gMsgBox setStringValue:@""];
    [gMsgBox setTarget:self];
    [gMsgBox setAction:@selector(messageBoxEntered:)];
    [self addSubview:gMsgBox];

    static HcHost host;
    host.line = cocoa_line;
    host.field_changed = cocoa_field_changed;
    hc_set_host(&host);
}

- (void)messageBoxEntered:(id)sender {
    NSString *cmd = [gMsgBox stringValue];
    if ([cmd length] == 0) return;
    hc_do([cmd UTF8String]);
    [self setNeedsDisplay:YES];
    [gMsgBox selectText:nil];
}

- (void)installToolPalette {
    NSPanel *palette = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(520, 400, 120, 60)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [palette setTitle:@"Outils"];
    [palette setFloatingPanel:YES];

    NSView *content = [palette contentView];

    NSButton *(^mk)(NSString*, NSInteger, CGFloat) = ^NSButton*(NSString *title, NSInteger tag, CGFloat x) {
        NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, 10, 36, 36)];
        [b setTitle:title];
        [b setTag:tag];
        [b setTarget:self];
        [b setAction:@selector(toolChosen:)];
        [b setBezelStyle:NSBezelStyleRegularSquare];
        [content addSubview:b];
        return b;
    };
    mk(@"fleche", TOOL_BROWSE, 4);
    mk(@"B", TOOL_BUTTON, 42);
    mk(@"F", TOOL_FIELD, 80);

    [palette makeKeyAndOrderFront:nil];
}

- (void)toolChosen:(id)sender {
    gTool = (HCTool)[sender tag];
    gSelected = NULL;
    [self setNeedsDisplay:YES];
    NSLog(@"outil : %d", (int)gTool);
}
- (void)editScriptOf:(Object *)obj {
    gEditTarget = obj;

    NSPanel *panel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 200, 480, 340)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    char d[64]; hc_describe(obj, d, sizeof d);
    [panel setTitle:[NSString stringWithFormat:@"Script de %s", d]];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(10, 44, 460, 286)];
    [scroll setHasVerticalScroller:YES];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    NSTextView *tv = [[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]];
    [tv setFont:[NSFont fontWithName:@"Monaco" size:12]];
    [tv setAutoresizingMask:NSViewWidthSizable];
    const char *cur = hc_script_of(obj);
    [tv setString:cur ? [NSString stringWithUTF8String:cur] : @""];
    [scroll setDocumentView:tv];
    [[panel contentView] addSubview:scroll];

    gEditView = tv;

    NSButton *ok = [[NSButton alloc] initWithFrame:NSMakeRect(390, 8, 80, 30)];
    [ok setTitle:@"OK"];
    [ok setBezelStyle:NSBezelStyleRounded];
    [ok setTarget:self];
    [ok setAction:@selector(saveScript:)];
    [ok setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
    [[panel contentView] addSubview:ok];

    gEditPanel = panel;
    [panel makeKeyAndOrderFront:nil];
}

- (void)saveScript:(id)sender {
    if (gEditTarget && gEditView) {
        hc_set_script(gEditTarget, [[gEditView string] UTF8String]);
    }
    [gEditPanel close];
    gEditPanel = nil; gEditView = nil; gEditTarget = nil;
    [self setNeedsDisplay:YES];
}
@end
