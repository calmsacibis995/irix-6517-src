/*
 * draw.h
 *
 * Header file for draw.c
 */

#include <X11/Xlib.h>

#define NOSVAL (-1)

typedef struct tagProgName {
    char *name;
    struct tagProgName *next;
} PROGNAME;

enum tagBloatType { Physical, Resident, Size, MappedObjects };

/*
 * Secondary types, for squeezing more info into each bar
 */
enum tagSecondType { Nostype, Priv, Shared, Phys, Res };

typedef enum tagBloatType BloatType;
typedef enum tagSecondType SecondType;

extern PROGRAM * DrawSetup(PROGRAM *new, PROGRAM *old, long physMem,
			   long freeMem, BloatType type,
			   SecondType stype, int all,
			   int *barTotal, int *numBars);

extern void UseDefaultVisual(void);

extern void SetNoDoubleBuffer(void);

extern void SetFontName(const char* fontName);

extern void Resize(int width, int height);

extern void Init(int argc, char **argv,Display *dpy, int eventMask,
		 PROGNAME *progNames, long threshHold);

extern void ShowPrintMode(int printMode);

extern void
Draw(PROGRAM *bloatList, int barTotal, int numBars, char *progName,
     pid_t pid, BloatType type, SecondType stype);

extern void DrawShadow(PROGRAM *bloat, int barTotal, char *progName);

extern void SetThreshHold(long thresh);

extern void Help(void);

extern char * Select(PROGRAM *bloat, long x, long y, int *procMode,
		     int dragging);

extern PROGRAM * SelectRegion(PROGRAM *bloat, long x, long y);

extern void WaitMessage(char *message, char *detail);
