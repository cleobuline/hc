#ifndef HCglobals_h
#define HCglobals_h
#import <Cocoa/Cocoa.h>
#import "hc_core.h"

typedef enum { TOOL_BROWSE, TOOL_BUTTON, TOOL_FIELD, TOOL_PENCIL, TOOL_ERASER,
               TOOL_LINE, TOOL_RECT, TOOL_OVAL, TOOL_FILL, TOOL_FREEFORM,
               TOOL_LASSO, TOOL_SELRECT, TOOL_BRUSH , TOOL_TEXT} HCTool;

typedef enum { INK_BLACK, INK_WHITE, INK_ERASE } HCInk;


extern HCTool gTool ;
extern HCInk   gInk;
extern int     gPattern;
extern int     gLineWidth;
extern BOOL    gShapeFilled;
extern Object *gSelected;
extern NSView *gView;
extern id gView;
extern int gBrush;
extern int gTextSize;
extern BOOL gTransparentBg;
#endif
