#import "HCglobals.h"

HCTool  gTool = TOOL_BROWSE;
HCInk   gInk = INK_BLACK;
int     gPattern = 19;
int     gLineWidth = 2;
BOOL    gShapeFilled = NO;
int gBrush = 5;
Object *gSelected = NULL;
id gView = nil;
NSBitmapImageRep *gClipboard = nil;
int gClipW = 0, gClipH = 0;
NSPoint gClipPts[4096];
int gClipPtsCount = 0;
NSMutableDictionary *gPaintCache = nil;

