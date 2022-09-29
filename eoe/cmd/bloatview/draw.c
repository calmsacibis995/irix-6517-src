/*
 * draw.c
 *
 * Drawing bloat
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdbe.h>
#include <X11/Xatom.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <bstring.h>

#include "process.h"
#include "draw.h"

#define FREEPID (-1)         /* smaller than any valid pid */
#define OTHERPID 0x00010000  /* bigger than any valid pid */
#define IRIXPID 0x00010001   /* bigger than OTHERPID */

#define FREECOLOR (GREEN + 8)
#define IRIXCOLOR (MAGENTA + 8)

#define MARG_NUM 2
#define BAR_NUM 5
#define LABEL_NUM 13
#define LINELEFT_NUM 8
#define LINERIGHT_NUM 12
#define DENOM 26
#define YSHADOWOFF 25
#define XSHADOWOFF 25
#define MINWIDTH 600
#define MINHEIGHT 600
#define TEXTWIDTH 236
#define SLABELWIDTH 88
#define FONTWIDTH 9
#define FONTHEIGHT 15
#define NUMCOLORS 9

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static unsigned long barColors[6];

static int usingDBE = 0;
static Drawable backBuffer;

#define NBARCOLORS (sizeof barColors/sizeof *barColors - 2)
#define FREEINDEX NBARCOLORS
#define IRIXINDEX (NBARCOLORS + 1)

static long winWidth, winHeight, barWidth, hmargin, barHeight;
static long topMargin, bottomMargin;
static long labelLeft, lineLeft, lineRight, memThresh;
static long labelTop, labelBottom;
static int textHeight, halfTextHeight;
static unsigned long bgColor;
static unsigned long textColor;
static unsigned long black;
static unsigned long otherColor = -1;
static PROGNAME *drawNames;
static Display *theDpy;
static Window theWin;
static int useDefaultVisual;
static int doubleBuffer = 1;
static Visual *theVisual;
static int theDepth;
static GC theGC;
static const char *theFontName
    = "-*-medium-r-normal--15-150-*-*-*-90-iso8859-1";
static XFontStruct *theFont;
static int curX, curY;
static int printMode;
static int blackAndWhiteMode;

static char *titles[] = {
    "Physical Memory Breakdown",
    "Resident Sizes of Processes",
    "Total Sizes of Processes",
    "Resident Mappings",
};

static char *progTitles[] = {
    "Physical Memory use for %s (%s)",
    "Resident Size of %s (%s)",
    "Total Size of %s (%s)",
    "Resident Mapping Breakdown of %s",
};

static char *captions[] = {
    "Physical Memory: %d K (%d K used)",
    "Sum of Resident Sizes: %d K",
    "Sum of Total Sizes: %d K",
    "Physical Memory: %d K (%d K used)",
};

static char *progCaptions[] = {
    "Physical Memory Use: %d K",
    "Resident Size: %d K",
    "Total Size: %d K",
    "Resident Mapping Breakdown: %d K",
};

static char *secondLabels[] = {
    "gmemusage bug!",
    "Private",
    "Shared",
    "Physical",
    "Resident",
};

/*
 *  static int
 *  WeWantThisOne(char *progName, long size)
 *
 *  Description:
 *      Given the name of a program and its size, determine whether to
 *      display it separately.  If we return 1, it will get its own
 *      bar; if we return 0, it will get lumped in with a bunch of
 *      others.
 *
 *      The name check occurs if bloatview was started with the -p
 *      option.  Otherwise, just the threshhold is checked.
 *
 *  Parameters:
 *      progName  Name of program under consideration
 *      size      size of program under consideration
 *
 *  Returns:
 *      1 if this program is to be displayed separately
 *      0 if this program is to be lumped with "other"
 */

static int
WeWantThisOne(char *progName, long size)
{
    PROGNAME *name;

    if (drawNames) {
	name = drawNames;

	while (name) {
	    if (strcmp(progName, name->name) == 0) {
		return 1;
	    }
	    name = name->next;
	}
	return 0;
    }

    return size >= memThresh;
}

/*
 *  static PROGRAM *
 *  FirstProgram(PROGRAM *prog)
 *
 *  Description:
 *      Find the first non-skip program in a list
 *
 *  Parameters:
 *      prog  list of programs
 *
 *  Returns:
 *      first program in the list that doesn't have the skip flag set
 */

static PROGRAM *
FirstProgram(PROGRAM *prog)
{
    while (prog && prog->skip) {
	prog = prog->next;
    }

    return prog;
}

/*
 *  static PROGRAM *
 *  NextProgram(PROGRAM *prog)
 *
 *  Description:
 *      Find the next non-skip program in a list
 *
 *  Parameters:
 *      prog  list of prograsm
 *
 *  Returns:
 *      the next program in the list that doesn't have the skip
 *      flag set
 */

static PROGRAM *
NextProgram(PROGRAM *prog)
{
    if (!prog) {
	return NULL;
    }

    do {
	prog = prog->next;
    } while (prog && prog->skip);

    return prog;
}

/*
 *  static PROGRAM *
 *  PrevProgram(PROGRAM *prog)
 *
 *  Description:
 *      Find the previous non-skip rogram in a list
 *
 *  Parameters:
 *      prog   list of programs
 *
 *  Returns:
 *      The previous program in the list that doesn't have the skip
 *      flag set.
 */

static PROGRAM *
PrevProgram(PROGRAM *prog)
{
    if (!prog) {
	return NULL;
    }

    do {
	prog = prog->prev;
    } while (prog && prog->skip);

    return prog;
}

/*
 *  static void PickVisual(void)
 *
 *  Description:
 *      Figure out which visual we should use.  We are looking for a
 *      double-buffered visual.  We're looking for a TrueColor visual
 *      with as many pixels as possible to get the real colors we
 *      want.
 *      
 *      If we can't find a TrueColor visual with enough colors, we
 *      will double-buffer using a pixmap and the default visual
 *      because we do not want to create our own colormap and have
 *      colormap flashing.
 */
static void PickVisual(void)
{
    int major, minor;
    XdbeScreenVisualInfo	*info;
    int numScreens = 1;
    Window root = XDefaultRootWindow(theDpy);
    int trueDepth = 0;
    Visual *trueVis = NULL;
    XVisualInfo *visInfo, visTemplate;
    int i, nItems;
    
    theVisual = DefaultVisual(theDpy, DefaultScreen(theDpy));
    theDepth = DefaultDepth(theDpy, DefaultScreen(theDpy));

    if (useDefaultVisual || !doubleBuffer) {
	return;
    }

    if (!XdbeQueryExtension(theDpy, &major, &minor)) {
	return;
    }
    
    info = XdbeGetVisualInfo(theDpy, &root, &numScreens);
    if (info == NULL) {
	return;
    }

    for (i = 0; i < info->count; i++) {
	visTemplate.visualid = info->visinfo[i].visual;
	visInfo = XGetVisualInfo(theDpy, VisualIDMask, &visTemplate, &nItems);
	if (visInfo->class == TrueColor && visInfo->depth >= 8) {
	    if (trueVis == NULL || trueDepth < visInfo->depth) {
		trueVis = visInfo->visual;
		trueDepth = visInfo->depth;
	    }
	    usingDBE = 1;
	}
    }    

    if (trueVis != NULL) {
	assert(usingDBE);
	theVisual = trueVis;
	theDepth = trueDepth;
    }
}

/*
 *  static int ScaleWidth(int width)
 *
 *  Description:
 *      Scale a width in our idealized coordinate space (used for the
 * 	layout) into pixels.  Scaling is based on the size of the font
 * 	we're using versus the size of our idealized font.
 *
 *  Parameters:
 *      width  size to scale
 *
 *  Returns:
 *	Scaled width.
 */
static int ScaleWidth(int width)
{
    return (width * theFont->max_bounds.width) / FONTWIDTH;
}

/*
 *  static int ScaleHeight(int width)
 *
 *  Description:
 *      Scale a height in our idealized coordinate space (used for the
 * 	layout) into pixels.  Scaling is based on the size of the font
 * 	we're using versus the size of our idealized font.
 *
 *  Parameters:
 *      height  size to scale
 *
 *  Returns:
 *	Scaled height.
 */
static int ScaleHeight(int height)
{
    return (height * textHeight) / FONTHEIGHT;
}

/*
 *  static Colormap SetupColors(void)
 *
 *  Description:
 *      Set up the colors we'll be using.
 *
 *  Returns:
 *	A Colormap.  Other global variables are initialized to the
 *      proper Pixel values.
 */
static Colormap SetupColors(void)
{
    Colormap cmap = 0;
    int i;
    unsigned long colors[9];
    XColor color, exact;
    /*
     * These colors originated in the IrisGL colormap.
     */
    static const char * const colorNames[NUMCOLORS] = {
	"#000000",    /* black */
	"#8e8e38", "#c67171", "#388e83", "#7171c6",    /* Bars */
	"#71c671",    /* Free bar */
	"#8e388e",    /* Irix bar */
	"#00243f",    /* Background color */
	"#aaaaaa",    /* text color */
    };
    unsigned long white;

    if (usingDBE) {
	cmap = XCreateColormap(theDpy, DefaultRootWindow(theDpy),
			       theVisual, AllocNone);
    } else {
	cmap = DefaultColormap(theDpy, DefaultScreen(theDpy));
    }	

    for (i = 0; i < NUMCOLORS; i++) {
	if (!XAllocNamedColor(theDpy, cmap, colorNames[i],
			      &color, &exact)) {
	    fprintf(stderr, "Cannot allocate color: %s\n", colorNames[i]);
	    break;
	}
	colors[i] = color.pixel;
    }

    if (i == NUMCOLORS) {
	black = colors[0];
	for (i = 0; i < 6; i++) {
	    barColors[i] = colors[i + 1];
	}

	bgColor = colors[7];
	textColor = colors[8];
    } else {
	/*
	 * We didn't allocate all of our colors.  Fallback to a
	 * black-and-white color scheme.
	 */
	if (XAllocNamedColor(theDpy, cmap, "black", &color, &exact)) {
	    black = color.pixel;
	} else {
	    fprintf(stderr, "Can't allocate black\n");
	    exit(1);
	}
	if (XAllocNamedColor(theDpy, cmap, "white", &color, &exact)) {
	    white = color.pixel;
	} else {
	    fprintf(stderr, "Can't allocate white\n");
	    exit(1);
	}
	for (i = 0; i < 6; i++) {
	    barColors[i] = white;
	}
	bgColor = white;
	textColor = black;
	blackAndWhiteMode = 1;
    }

    return cmap;
}

/*
 * This code used to use IrisGL to render.  Now it uses X, so that
 * users with X terminals can see bloatview.  In order to facilitate
 * the conversion from IrisGL to X, the following GL functions were
 * written in terms of X: color, rectfi, recti, strwidth, cmov2i,
 * charstr, swapbuffers, and clear.
 */

/*
 * GL color function implemented in X.
 */
static void color(unsigned long index)
{
    XSetForeground(theDpy, theGC, index);
}

/*
 *  static void ConvertGLRectToX(int x1, int y1, int x2, int y2,
 *  			         int *x, int *y, int *width, int *height)
 *
 *  Description:
 *      Convert a GL rectangle to an X rectangle.
 *
 *  Parameters:
 *      x1       GL rectangle
 *      y1
 *      x2
 *      y2
 *      x        X rectangle.
 *      y
 *      width
 *      height
 */
static void ConvertGLRectToX(int x1, int y1, int x2, int y2,
			     int *x, int *y, int *width, int *height)
{
    if (x1 < x2) {
	*x = x1;
	*width = x2 - x1;
    } else {
	*x = x2;
	*width = x1 - x2;
    }
    y1 = winHeight - y1;
    y2 = winHeight - y2;
    if (y1 < y2) {
	*y = y1;
	*height = y2 - y1;
    } else {
	*y = y2;
	*height = y1 - y2;
    }
}

/*
 * GL rectfi function implemented in X.
 */
static void rectfi(int x1, int y1, int x2, int y2)
{
    int x, y, width, height;
    ConvertGLRectToX(x1, y1, x2, y2, &x, &y, &width, &height);
    XFillRectangle(theDpy, backBuffer, theGC, x, y, width, height);
}

/*
 * GL recti function implemented in X.
 */
static void recti(int x1, int y1, int x2, int y2)
{
    int x, y, width, height;
    ConvertGLRectToX(x1, y1, x2, y2, &x, &y, &width, &height);
    XDrawRectangle(theDpy, backBuffer, theGC, x, y, width, height);
}

/*
 * GL strwidth function implemented in X.
 */
static int strwidth(const char *str)
{
    int direction, ascent, descent;
    XCharStruct overall;

    XTextExtents(theFont, str, strlen(str), &direction, &ascent,
		 &descent, &overall);
    return overall.width;
}

/*
 * GL cmov2i function implemented in X.
 */
static void cmov2i(int x, int y)
{
    curX = x;
    curY = winHeight - y;
}

/*
 * GL charstr function implemented in X.
 */
static void charstr(const char *str)
{
    XDrawString(theDpy, backBuffer, theGC, curX, curY,
		str, strlen(str));
}

/*
 * GL swapbuffers function implemented in X.
 */
static void swapbuffers(void)
{
    XdbeSwapInfo info;
    if (!doubleBuffer) {
	return;
    }
    if (usingDBE) {
	info.swap_window = theWin;
	info.swap_action = XdbeUndefined;
	XdbeSwapBuffers(theDpy, &info, 1);
    } else {
	XCopyArea(theDpy, backBuffer, theWin, theGC, 0, 0, winWidth,
		  winHeight, 0, 0);
    }
}

/*
 * GL clear function implemented in X.
 */
static void clear(void)
{
    XFillRectangle(theDpy, backBuffer, theGC, 0, 0, winWidth,
		   winHeight);
}

/*
 *  PROGRAM *
 *  DrawSetup(PROGRAM *new, PROGRAM *old, long physMem, long freeMem,
 *  	      BloatType type, SecondType stype, int all, int *barTotal,
 *            int *numBars)
 *
 *  Description:
 *      Add a bar for free memory (Physical).  Allocate colors
 *      for each of the bars, add a bar for "other", and add a bar for
 *      Irix (Physical).
 *
 *  Parameters:
 *      new      The new list of programs to display as bars
 *      old      The last one displayed; we use this to keep the
 *               colors consistent
 *      physMem  Amount of physical memory on the system
 *      freeMem  Amount of memory the kernel says is free
 *      type     Type of bars to display
 *      stype    Secondary type of bars to display
 *      all      1 if this is all processes, 0 if this is a breakdown
 *               for one process
 *      barTotal gets total for the bar
 *	numBars  gets number of bars
 *
 *  Returns:
 *      The list of programs to display (same as new with some stuff
 *      added)
 */

PROGRAM *
DrawSetup(PROGRAM *new, PROGRAM *old, long physMem, long freeMem,
	  BloatType type, SecondType stype, int all, int *barTotal,
	  int *numBars)
{
    PROGRAM *prev = NULL, *prevColor = NULL, *head;
    long progTotal, otherMem = 0, userMem, otherSecondVal = 0;
    char otherBuf[100];
    int nBars;
    
    nBars = 0;
    
    progTotal = 0;
    
    if (type == Physical || type == MappedObjects) {
	if (all) {
	    *barTotal = physMem;
	    /*
	     * Add the free chunk at the top
	     */
	    head = malloc(sizeof *head);
	    /*
	     * Be very, very careful here.  FreeBloat will free
	     * progName and mapName, but not manType!!
	     */
	    head->progName = strdup("Free");
	    head->mapName = strdup("Free");
	    head->mapType = "Free";
	    head->value = freeMem;
	    head->size = head->resSize = head->weightSize = head->privSize
		= head->value;
	    head->secondValue = NOSVAL;
	    head->color = FREEINDEX;
	    head->pid = FREEPID;
	    head->pids = NULL;
	    head->nProc = 1;
	    head->skip = 0;
	    head->special = 1;
	    head->print = 1;
	    nBars++;
	    
	    head->prev = NULL;
	    head->next = new;
	    new->prev = head;
	} else {
	    head = new;
	}
	/*
	 * Add up all the sizes
	 */
	while (new) {
	    progTotal += new->weightSize;
	    new = new->next;
	}
	if (!all) {
	    *barTotal = progTotal;
	}
    } else {
	head = new;
	while (new) {
	    progTotal += type == Resident ? new->resSize : new->size;
	    new = new->next;
	}
	*barTotal = progTotal;
    }
    
    userMem = progTotal;
    new = (type == Physical || type == MappedObjects)
	&& all ? head->next : head;

    while (new) {
	/*
	 * Set the values for each bar appropriately
	 */
	switch (type) {
	case Physical:
	case MappedObjects:
	    new->value = new->weightSize;
	    break;
	case Resident:
	    new->value = new->resSize;
	    break;
	default:
	case Size:
	    new->value = new->size;
	    break;
	}

	switch (stype) {
	default:
	case Nostype:
	    new->secondValue = NOSVAL;
	    break;
	case Priv:
	    new->secondValue = new->privSize;
	    break;
	case Shared:
	    new->secondValue = new->weightSize - new->privSize;
	    break;
	case Phys:
	    new->secondValue = new->weightSize;
	    break;
	case Res:
	    new->secondValue = new->resSize;
	    break;
	}

	/*
	 * Set the skip flag, allocate colors (using the old list if
	 * it's valid), and add up the amount to be displayed in the
	 * "other" bar.
	 */
	if (!all || WeWantThisOne(new->progName, new->value)) {
	    new->skip = 0;
	    new->special = 0;
	    while (old && old->pid < new->pid) {
		old = old->next;
	    }
	    
	    if (old && !old->skip && old->pid == new->pid) {
		new->color = old->color;
	    } else if (prevColor) {
		new->color = (prevColor->color + 1) % NBARCOLORS;
	    } else {
		new->color = 0;
	    }
	    prevColor = new;
	    nBars++;
	} else {
	    new->skip = 1;
	    otherMem += new->value;
	    otherSecondVal += new->secondValue;
	}

	prev = new;
	new = new->next;
    }

    /*
     * Add the other bar
     */
    if (otherMem) {
	/*
	 * The check for old resets other color if the rest of the
	 * bars are also getting reset
	 */
	if (otherColor == -1 || !old) {
	    otherColor = prevColor ? (prevColor->color + 1) % NBARCOLORS : 0;
	}
	new = malloc(sizeof *new);
	if (!drawNames) {
	    snprintf(otherBuf, sizeof otherBuf, "< %d", memThresh);
	}	    
	new->progName = strdup(drawNames ? "Other" : otherBuf);
	new->mapName = NULL;
	new->value = otherMem;
	new->secondValue = otherSecondVal;
	new->pid = OTHERPID;
	new->pids = NULL;
	new->color = otherColor;
	new->next = NULL;
	new->prev = prev;
	new->skip = 0;
	new->special = 1;
	new->print = 0;
	new->nProc = 1;
	prev->next = new;
	prev = new;
	nBars++;
    }

    if ((type == Physical || type == MappedObjects) && all) {
	/*
	 * Add the Irix chunk at the bottom
	 */
	if (prev) {
	    new = malloc(sizeof *new);
	    /*
	     * Be very, very careful here.  FreeBloat will free
	     * progName and mapName, but not manType!!
	     */
	    new->progName = strdup(IRIX);
	    new->mapName = strdup(IRIX);
	    new->mapType = IRIX;
	    new->value = physMem - userMem - freeMem;
	    new->size = new->resSize = new->weightSize = new->privSize
		= new->value;
	    new->secondValue = NOSVAL;
	    new->color = IRIXINDEX;
	    new->pid = IRIXPID;
	    new->pids = NULL;
	    new->nProc = 1;
	    new->next = NULL;
	    new->prev = prev;
	    new->skip = 0;
	    new->special = 0;
	    new->print = 1;
	    prev->next = new;
	    nBars++;
	}
    }

    *numBars = nBars;
    return head;
}

/*
 *  void SetFontName(const char* fontName)
 *
 *  Description:
 *      Set the font to use.
 *
 *  Parameters:
 *      fontName  Name of the font to use.
 */
void SetFontName(const char* fontName)
{
    theFontName = fontName;
}

/*
 *  void UseDefaultVisual(void)
 *
 *  Description:
 *      Use the default visual.  This prevents double-buffering.
 */
void UseDefaultVisual(void)
{
    useDefaultVisual = 1;
}

/*
 *  void SetNoDoubleBuffer(void)
 *
 *  Description:
 *      Disable double-buffering of all kinds.
 */
void SetNoDoubleBuffer(void)
{
    doubleBuffer = 0;
}

/*
 *  void
 *  Resize(int width, int height)
 *
 *  Description:
 *      This sets some global variables based on the size of the window.
 */

void
Resize(int width, int height)
{
    if (width == winWidth && height == winHeight) {
	return;
    }

    winWidth = width;
    winHeight = height;

    hmargin = (MARG_NUM * winWidth) / DENOM;
    barWidth = (BAR_NUM * winWidth) / DENOM;

    if (hmargin * 3 < winHeight) {
	topMargin = hmargin;
	bottomMargin = (hmargin * 3) / 2;
	barHeight = winHeight - topMargin - bottomMargin;
    } else {
	barHeight = winHeight;
	topMargin = bottomMargin = 0;
    }
    labelLeft = winWidth - ScaleWidth(TEXTWIDTH + SLABELWIDTH);
    lineLeft = (winWidth * LINELEFT_NUM) / DENOM;
    lineRight = labelLeft - winWidth / DENOM;
    labelTop = winHeight - topMargin;
    labelBottom = bottomMargin;

    if (!usingDBE && doubleBuffer) {
	if (backBuffer) {
	    XFreePixmap(theDpy, backBuffer);
	}
	backBuffer = XCreatePixmap(theDpy, theWin, winWidth,
				   winHeight, theDepth);
    }
}

/*
 *  void ShowPrintMode(int mode)
 *
 *  Description:
 *      Turns print mode (where we print bloat to stdout as well as
 *      display it graphically on or off.  The only affect of calling
 *      this function is to change string in the title bar.
 *
 *  Parameters:
 *      mode  0 to turn print mode on, non-zero to turn it off.
 */
void ShowPrintMode(int mode)
{
    char title[100], hname[50];
    printMode = mode;
    if (theDpy != NULL && theWin != 0) {
	if (gethostname(hname, sizeof hname) < 0) {
	    strncpy(title, "gmemusage", sizeof title);
	    title[sizeof title - 1] = '\0';
	} else {
	    snprintf(title, sizeof title, "gmemusage @ %s", hname);
	}
	if (mode) {
	    strncat(title, " (print)", sizeof title - strlen(title));
	}
	XStoreName(theDpy, theWin, title);
    }
}

/*
 *  void
 *  Init(int argc, char **argv, Display *dpy, int eventMask,
 *       PROGNAME *progNames, long threshHold)
 *
 *  Description:
 *      Initialize.  Pick a visual, Create a window, etc.
 *
 *  Parameters:
 *      argc        For setting WM_COMMAND property
 *      argv        For setting WM_COMMAND property
 *	dpy	    X Display we're connected to
 *	eventMask   Specifies which X events we get
 *      progNames   List of program names given in -p option
 *      threshHold  the initial threshhold to use (can be changed
 */
void
Init(int argc, char **argv, Display *dpy, int eventMask,
     PROGNAME *progNames, long threshHold)
{
    XSizeHints *sizeHints;
    XClassHint *classHint;
    XSetWindowAttributes attr;
    XGCValues val;
    Colormap cmap;
    int minWidth, minHeight, dpyWidth, dpyHeight;;

    theDpy = dpy;

    PickVisual();
    cmap = SetupColors();

    attr.background_pixel = bgColor;
    attr.border_pixel = bgColor;
    attr.colormap = cmap;
    attr.event_mask = eventMask;
    attr.bit_gravity = NorthWestGravity;

    theFont = XLoadQueryFont(theDpy, theFontName);
    if (theFont == NULL) {
	theFont = XLoadQueryFont(theDpy, "fixed");
	if (theFont == NULL) {
	    theFont = XLoadQueryFont(theDpy, "*");
	    if (theFont == NULL) {
		fprintf(stderr, "Cannot load any fonts!\n");
		exit(1);
	    }
	}
    }

    textHeight = theFont->ascent + theFont->descent;
    halfTextHeight = textHeight / 2;

    /*
     * Scale our size based on the font used for initial layout at
     * development time
     */
    dpyWidth = DisplayWidth(theDpy, DefaultScreen(theDpy));
    dpyHeight = DisplayHeight(theDpy, DefaultScreen(theDpy));
    minWidth = ScaleWidth(MINWIDTH);
    if (minWidth > dpyWidth) {
	minWidth = dpyWidth;
    }
    minHeight = ScaleHeight(MINHEIGHT);
    if (minHeight > dpyHeight) {
	minHeight = dpyHeight;
    }

    theWin = XCreateWindow(dpy, DefaultRootWindow(dpy),
			   0, 0, minWidth, minHeight, 0,
			   theDepth, CopyFromParent,
			   theVisual,
			   CWBackPixel | CWBorderPixel | CWBitGravity
			   | CWColormap | CWEventMask, &attr);
    XMapWindow(dpy, theWin);

    if (!doubleBuffer) {
	backBuffer = theWin;
    } else if (usingDBE) {
	backBuffer = XdbeAllocateBackBufferName(theDpy, theWin,
						XdbeUndefined);
    } else {
	/* backBuffer will get set in Resize(). */
    }

    sizeHints = XAllocSizeHints();
    sizeHints->flags = PMinSize | PMaxSize;
    sizeHints->min_width = minWidth;
    sizeHints->min_height = minHeight;
    sizeHints->max_width = minWidth;
    sizeHints->max_height = dpyHeight;
    XSetWMNormalHints(dpy, theWin, sizeHints);
    XFree(sizeHints);

    classHint = XAllocClassHint();
    classHint->res_name = "gmemusage";
    classHint->res_class = "Gmemusage";
    XSetClassHint(theDpy, theWin, classHint);
    XFree(classHint);

    XSetCommand(theDpy, theWin, argv, argc);
    XSetIconName(dpy, theWin, "gmemusage");
    ShowPrintMode(printMode);

    XSetWMColormapWindows(dpy, theWin, &theWin, 1);

    Resize(minWidth, minHeight);

    val.font = theFont->fid;
    val.graphics_exposures = False;
    theGC = XCreateGC(theDpy, backBuffer,
		      GCFont | GCGraphicsExposures, &val);

    memThresh = threshHold;
    drawNames = progNames;
}

/*
 *  static void
 *  DrawBar(long top, long height, Colorindex barColor, long margin,
 *          long width, long split)
 *
 *  Description:
 *      Draw one of the memory bars
 *
 *  Parameters:
 *      top       The top of the bar
 *      height    The height of the bar
 *      barColor  The color of the bar
 *	margin    left side of bar
 *      width     width of bar
 *	long      split
 */

static void
DrawBar(long top, long height, unsigned long barColor, long margin,
	long width, long split)
{
    color(barColor);
    rectfi(margin, top - height, margin + width, top);
    color(black);
    recti(margin, top, margin + width, top - height);

    if (split) {
	recti(margin + width - split, top, margin + width, top - height);
    }
}

/*
 *  static void
 *  SetupLabels(PROGRAM *bloatList, int numBars)
 *
 *  Description:
 *      Set the labelOffset member of each non-skip element in the
 *      list.  This is kind of tricky.  We want each label to be as
 *      close as possible (vertically) to the center of the bar to
 *      which it corresponds, while at the same time preventing the
 *      labels from overlapping.
 *
 *      If there's no extra room, meaning that there are more labels
 *      than can fit in the space provided, we spread them evenly to
 *      minimize overlap.
 *
 *      Otherwise, we know that all the labels can fit without
 *      overlapping, but there may be clusters of labels that are too
 *      close together.  So we locate these clusters, and try to center
 *      them with respect to the bars to which they correspond.  We
 *      check how much extra space is above and below the cluster, and
 *      skew either way if there isn't enough for us to center
 *      ourselves.  Furthermore, once we've set up a cluster we have
 *      to go back up the list, pushing the previous labels up if our
 *      new cluster location overlaps an old cluster location.
 *
 *      The reason that all this works is that we have predetermined
 *      that there really is enough space to do this; we always know
 *      that our cluster will fit one way or the other, and we won't
 *      push labels up off the top of the window because we checked
 *      the space first and didn't assume there was more space above
 *      us than there actually is.
 *
 *  Parameters:
 *      bloatList  list of labels to spread out
 *      numBars    number of bars in the list
 */

static void
SetupLabels(PROGRAM *bloatList, int numBars)
{
    int offset;
    float aveSep, foffset;
    int nLabels, top, bottom, extraAbove, extraBelow;
    int nCluster, center, firstCenter, lastCenter;
    PROGRAM *bloat, *next, *prev, *elem;

    aveSep = (numBars > 1) ?
	(float)(labelTop - labelBottom) / (float)(numBars - 1)
	    : labelTop - labelBottom;

    /*
     * Spread 'em evenly if there's no extra space.  Do the
     * calculations in floating point so we utilize the extra space as
     * well as we can.
     */
    if (aveSep <= textHeight) {
	bloat = bloatList;
	foffset = labelTop;
	while (bloat) {
	    if (!bloat->skip) {
		bloat->labelOffset = foffset;
		foffset -= aveSep;
	    }
	    bloat = bloat->next;
	}
	return;
    }

    /*
     * If we got here, then there's enough space to have at least
     * textHeight space between each pair of labels.  We try our best
     * to keep the labels close vertically to the center of the bars
     * to which they correspond.
     */
    bloat = FirstProgram(bloatList);
    nLabels = 0;

    while (bloat) {
	/*
	 * Find the extent of this cluster.  We consider single labels
	 * that have lots of space on either side to be clusters of
	 * size one.
	 */
	firstCenter = bloat->center;
	bottom = firstCenter + halfTextHeight;
	next = NextProgram(bloat);
	nCluster = 1;
	lastCenter = bloat->center;
	center = bloat->center;
	bottom = bloat->center - textHeight;

	while (next && bottom <= next->center) {
	    nCluster++;
	    lastCenter = next->center;
	    center = lastCenter + (firstCenter - lastCenter) / 2;
	    bottom = center - (nCluster * textHeight) / 2;
	    next = NextProgram(next);
	}

	/*
	 * bottom and center have been set below; now figure out where
	 * the top is.
	 */
	top = center + ((nCluster - 1) * textHeight + 1) / 2;

	/*
	 * We want to have this cluster extend from top to bottom.
	 * Now we calculate how much extra space there is above and
	 * below us, and skew the cluster down if there's not enough
	 * space above us and up if there's not enough space below us.
	 *
	 * Remember, there's guaranteed to be enough space *somewhere*
	 * for this cluster to fit.
	 */
	extraAbove = labelTop - top - textHeight * nLabels;
	extraBelow = bottom - labelBottom - textHeight *
	    (numBars - nLabels - nCluster);

	if (extraAbove < 0) {
	    top += extraAbove;
	} else if (extraBelow < 0) {
	    top -= extraBelow;
	}

	nLabels += nCluster;

	elem = bloat;
	offset = top;
	
	/*
	 * Set the labelOffsets.  The members of the cluster will be
	 * exactly textHeight away from one another.
	 */
	while (nCluster--) {
	    elem->labelOffset = offset;
	    offset -= textHeight;
	    elem = NextProgram(elem);
	}
	
	/*
	 * Remember where we were for the next iteration
	 */
	next = elem;

	/*
	 * Push previous labels up if necessary
	 */
	elem = bloat;
	prev = PrevProgram(bloat);
	while (prev &&
	       prev->labelOffset - elem->labelOffset < textHeight) {
	    prev->labelOffset = elem->labelOffset + textHeight;
	    elem = prev;
	    prev = PrevProgram(elem);
	}

	bloat = next;
    }
}

/*
 *  void
 *  DrawLabels(PROGRAM *bloatList, SecondType stype)
 *
 *  Description:
 *      Draw the labels down the right side of the window
 *
 *  Parameters:
 *      bloatList  list of bars and labels
 *	stype      type of secondary labels
 */

void
DrawLabels(PROGRAM *bloatList, SecondType stype)
{
    PROGRAM *bloat = bloatList;
    char ctype, labelBuf[100];
    int value;

    if (blackAndWhiteMode) {
	color(black);
    }

    while (bloat) {
	if (!bloat->skip) {
	    if (!blackAndWhiteMode) {
		color(barColors[bloat->color]);
	    }
	    /*
	     * Size display resolution if values get too large
 	     */
	    if (bloat->value > 999999) {
		ctype = 'M';
		value = bloat->value / 1024;
	    } else {
		ctype = 'K';
		value = bloat->value;
	    }

	    /*
	     * If there is more than one copy of a program running,
	     * denote that with the number of copies running in
	     * parentheses.
	     */
	    if (bloat->nProc == 1) {
		snprintf(labelBuf, sizeof labelBuf, "%-16.16s: %6d %c",
			 bloat->progName, value, ctype);
	    } else if (bloat->nProc < 10) {
		snprintf(labelBuf, sizeof labelBuf, "%-12.12s (%d): %6d %c",
			 bloat->progName, bloat->nProc, value, ctype);
	    } else {
		snprintf(labelBuf, sizeof labelBuf, "%-11.11s (%d): %6d %c",
			 bloat->progName, bloat->nProc, value, ctype);
	    }
	    /*
	     * For some reason, halfTextHeight / 2 looks really nice.
	     */
	    cmov2i(labelLeft, bloat->labelOffset - halfTextHeight / 2);
	    charstr(labelBuf);

	    if (stype != Nostype && bloat->secondValue != NOSVAL) {
		if (bloat->secondValue > 999999) {
		    snprintf(labelBuf, sizeof labelBuf, "%6d M",
			     bloat->secondValue/1024);
		} else {
		    snprintf(labelBuf, sizeof labelBuf, "%6d K",
			     bloat->secondValue);
		}
		cmov2i(winWidth - ScaleWidth(SLABELWIDTH),
		       bloat->labelOffset - halfTextHeight / 2);
		charstr(labelBuf);
	    }
	}
	bloat = bloat->next;
    }
}

/*
 *  void
 *  DrawLines(PROGRAM *bloat)
 *
 *  Description:
 *      Draw the lines from the bars to the labels
 *
 *  Parameters:
 *      bloat  List of labels and bars
 */

void
DrawLines(PROGRAM *bloat)
{
    if (blackAndWhiteMode) {
	color(black);
    }
    while (bloat) {
	if (!bloat->skip) {
	    if (!blackAndWhiteMode) {
		color(barColors[bloat->color]);
	    }
	    XDrawLine(theDpy, backBuffer, theGC,
		      lineLeft, winHeight - bloat->center,
		      lineRight, winHeight - bloat->labelOffset);
	}
	bloat = bloat->next;
    }
}

/*
 *  void
 *  Draw(PROGRAM *bloatList, int barTotal, int numBars, char *progName,
 *       pid_t pid, BloatType type)
 *
 *  Description:
 *      Draw captions, bars, labels, and lines
 *
 *  Parameters:
 *      bloatList  bars and labels to draw
 *      barTotal   total memory used by bar
 *      numBars    number of bars
 *      progName   name of program (if this is memory use within a program)
 *      pid        process id (if this is memory use within a process)
 *      type       type of chart we're drawing.  Used to select the
 *                 proper captions
 *	stype	   secondary type of chart, for more captions and labels
 */

void
Draw(PROGRAM *bloatList, int barTotal, int numBars, char *progName,
     pid_t pid, BloatType type, SecondType stype)
{
    long top, height, split;
    int swidth, secondValTotal = 0;
    char buf[100], *str, buf2[30];
    PROGRAM *bloat;
    
    if (!progName) {
	color(bgColor);
	clear();
    }

    /*
     * Draw a title based on the type of graph
     */
    color(textColor);
    if (progName) {
	if (pid == -1) {
	    (void)strcpy(buf2, "all");
	} else {
	    (void)snprintf(buf2, sizeof buf2, "process %d", pid);
	}
	snprintf(buf, sizeof buf, progTitles[type], progName, buf2);
	str = buf;
    } else {
	str = titles[type];
    }
    swidth = strwidth(str);
    cmov2i(winWidth / 2 - swidth / 2,
	   winHeight - topMargin / 2 - halfTextHeight);
    charstr(str);

    /*
     * Draw the bars
     */
    top = winHeight - topMargin;

    bloat = bloatList;

    while (bloat) {
	if (!bloat->skip) {
	    height = ((long long)bloat->value * (long long)barHeight)
		    / barTotal;
	    split = bloat->value > 0 ?
		(bloat->secondValue * barWidth) / bloat->value : 0;
	    if (split > barWidth || split < 0) {
		split = 0;
	    }
	    DrawBar(top, height, barColors[bloat->color],
		    hmargin, barWidth, split);
	    bloat->top = top;
	    bloat->height = height;
	    bloat->center = top - height / 2;
	    top -= height;
	    secondValTotal += bloat->secondValue;
	}
	bloat = bloat->next;
    }

    color(textColor);
    /*
     * Draw the total
     */
    if (!progName && (type == Physical || type == MappedObjects)) {
	snprintf(buf, sizeof buf, captions[type], barTotal, barTotal -
		 bloatList->value);
    } else {
	snprintf(buf, sizeof buf, (progName ? progCaptions : captions)[type],
		 barTotal);
    }
    swidth = strwidth(buf);
    cmov2i(winWidth / 2 - swidth / 2, textHeight * 2 + halfTextHeight);
    charstr(buf);
    
    /*
     * Tell the user how to get help
     */
    str = progName ?
	"Use \"Page Up\" and \"Page Down\" to navigate processes" :
	    "Press 'h' for help.";
    swidth = strwidth(str);
    cmov2i(winWidth / 2 - swidth / 2, textHeight);
    charstr(str);

    /*
     * Draw the labels and lines
     */
    SetupLabels(bloatList, numBars);
    DrawLabels(bloatList, stype);
    DrawLines(bloatList);

    if (stype != Nostype) {
	color(textColor);
	cmov2i(winWidth - ScaleWidth(SLABELWIDTH),
	       winHeight - topMargin / 2 - halfTextHeight);
	charstr(secondLabels[stype]);

	cmov2i(winWidth - ScaleWidth(SLABELWIDTH),
	       textHeight * 2 + halfTextHeight);
	if (secondValTotal > 999999) {
	    snprintf(buf, sizeof buf, "%6d M", secondValTotal/1024);
	} else {
	    snprintf(buf, sizeof buf, "%6d K", secondValTotal);
	}
	charstr(buf);
    }

    swapbuffers();
}

/*
 *  void
 *  DrawShadow(PROGRAM *bloat, int barTotal, char *progName)
 *
 *  Description:
 *      Draw shadow bars, so the user can tell how a program's memory
 *      use breakdown fits in with the rest of memory.  The current
 *      program is drawn solid, the rest are just outlines.
 *
 *  Parameters:
 *      bloat     shadow bloat
 *      barTotal  total of the bars
 *      progName  name of the program to draw solid.
 */

void
DrawShadow(PROGRAM *bloat, int barTotal, char *progName)
{
    long top, height, left;

    color(bgColor);
    clear();

    color(textColor);

    top = winHeight - topMargin - YSHADOWOFF;
    left = hmargin - XSHADOWOFF;

    while (bloat) {
	if (!bloat->skip) {
	    height = ((long long)bloat->value * (long long)barHeight)
		    / barTotal;
	    (strcmp(bloat->progName, progName) == 0 ?
	     rectfi : recti)(left, top, left + barWidth, top - height);
	    bloat->top = top;
	    bloat->height = height;
	    top -= height;
	}
	bloat = bloat->next;
    }

    /*
     * Don't swapbuffers or gflush here; we're depending on a
     * subsequent call to Draw for that.
     */
}

/*
 *  char *
 *  Select(PROGRAM *bloat, long x, long y, int *procMode, int dragging)
 *
 *  Description:
 *      The user has clicked the mouse.  Figure out what he or she has
 *      selected.
 *
 *	This function helps implement the policy of what kind of
 *	display to do by modifying the procMode parameter.  What we're
 *	implementing is that if we're in procMode and the user clicks
 *	anywhere outside the shadow bar, we should go back to regular
 *	mode.  This may be more convenient than using the space bar,
 *	which is the "official" means of return.
 *
 *  Parameters:
 *      bloat      List of bloat
 *      x          x coordinate of mouse click
 *      y          y coordinate of mouse click
 *      procMode   1 if we're in procMode, 0 otherwise
 *      dragging   1 if we're dragging, 0 otherwise
 *
 *  Returns:
 *      name of the thing that was clicked on
 */

char *
Select(PROGRAM *bloat, long x, long y, int *procMode, int dragging)
{
    y = winHeight - y;
    if (*procMode) {
	if (y < bottomMargin - YSHADOWOFF
	    || y > winHeight - topMargin - YSHADOWOFF) {
	    if (!dragging) {
		*procMode = 0;
	    }
	    return NULL;
	}
	    
	/*
	 * Adjust x coordinate for procMode.  The bars will all have
	 * the correct top and height for comparison purposes because
	 * these got calculated in DrawShadow, but the x has to be
	 * adjusted because the comparison code below assumes the bars
	 * are drawn in the normal place instead of offset to the
	 * left.
	 */
	x += XSHADOWOFF;
    }

    if (hmargin <= x && x <= hmargin + barWidth) {
	while (bloat) {
	    if (!bloat->skip && !bloat->special && y <= bloat->top
		&& y > bloat->top - bloat->height) {
		return strdup(bloat->progName);
	    }
	    bloat = bloat->next;
	}
    } else if (!*procMode && labelLeft <= x && x <= winWidth - hmargin) {
	while (bloat) {
	    if (!bloat->skip && !bloat->special && y <=
		bloat->labelOffset + halfTextHeight &&
		y > bloat->labelOffset - halfTextHeight) {
		return strdup(bloat->progName);
	    }
	    bloat = bloat->next;
	}
    } else if (!dragging) {
	*procMode = 0;
    }

    return NULL;
}
    
/*
 *  PROGRAM *
 *  SelectRegion(PROGRAM *bloat, long x, long y)
 *
 *  Description:
 *      This function gets called when we're looking at the bloat for
 *      a single process and the user clicks on one of the regions.
 *      This returns the PROGRAM pointer for that region.
 *
 *  Parameters:
 *      bloat  program bloat for a single process
 *      x      x coordinate of mouse click
 *      y      y coordinate of mouse click
 *
 *  Returns:
 *	bloat info for the region clicked on, or NULL if no region
 *      was clicked on.
 */

PROGRAM *
SelectRegion(PROGRAM *bloat, long x, long y)
{
    y = winHeight - y;
    if (hmargin <= x && x <= hmargin + barWidth) {
	while (bloat) {
	    if (!bloat->skip && !bloat->special && y <= bloat->top
		&& y > bloat->top - bloat->height) {
		return bloat;
	    }
	    bloat = bloat->next;
	}
    } else if (labelLeft <= x && x <= winWidth - hmargin) {
	while (bloat) {
	    if (!bloat->skip && !bloat->special && y <=
		bloat->labelOffset + halfTextHeight &&
		y > bloat->labelOffset - halfTextHeight) {
		return bloat;
	    }
	    bloat = bloat->next;
	}
    }

    return NULL;
}

/*
 *  void
 *  Help(void)
 *
 *  Description:
 *      Display some (hopefully) useful information on running
 *      bloatview.
 */

void
Help(void)
{
    char *str;
    int swidth, tabStop;
    long textY;

    color(bgColor);
    clear();

    color(textColor);
    textY = winHeight - textHeight - halfTextHeight;

    str = "Command line usage:";
    swidth = strwidth(str);
    cmov2i(MINWIDTH / 2 - swidth / 2, textY);
    charstr(str);

    textY -= textHeight + halfTextHeight;

    tabStop = strwidth("gmemusage ");

    str = "gmemusage [ -i interval ] [ -m | -p | -r | -s ] [ -u ]";
    cmov2i(hmargin, textY);
    charstr(str);
    textY -= textHeight;

    cmov2i(hmargin + tabStop, textY);
    charstr("[ -a aiff-file ] [ -g growth-threshhold ] [ -y ]");
    textY -= textHeight;

    cmov2i(hmargin + tabStop, textY);
    charstr("[ -f font ] [ -v ] [ -n ]");
    textY -= textHeight;

    cmov2i(hmargin + tabStop, textY);
    charstr("[ -t thresh ] [ -d delta ] [ progs ... ]");
    textY -= textHeight + halfTextHeight;

    cmov2i(hmargin, textY);
    charstr("interval is the polling interval in milliseconds");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("-m selects Resident Mappings");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("-p selects Physical Memory Breakdown (default)");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("-r selects Resident Sizes of Processes");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("-s selects Total Sizes of Processes");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("-u forces update of inode lookup table");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("aiff-file is the sound for program growth");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("growth-threshhold is the minimum increase for growth sounds");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("-y turns on printing to stdout at each interval");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("font specifies the font to use");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("-v causes gmemusage to use the default visual");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("-n turns off double-buffering");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("thresh is the memory threshhold for displaying");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("    individual programs (defaults to 500 K),");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("delta is the amount by which the arrow keys");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("change the threshhold (defaults to 50 K), and");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("progs is a list of programs to view.");
    textY -= textHeight + halfTextHeight;

    str = "Runtime usage:";
    swidth = strwidth(str);
    cmov2i(MINWIDTH / 2 - swidth / 2, textY);
    charstr(str);
    textY -= textHeight + halfTextHeight;

    cmov2i(hmargin, textY);
    charstr("Click on a bar to view detailed memory usage.");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("Press the space bar to return to the overview.");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("'m' selects Resident Mappings");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("'p' selects Physical Memory Breakdown");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("'r' selects Resident Sizes of Processes");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("'s' selects Total Sizes of Processes");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("'v' cycles through some interesting information");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("'t' prints current information to stdout");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("'y' prints current information to stdout every interval");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("up arrow key increases the threshhold");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("down arrow key decreases the threshhold");
    textY -= textHeight;

    cmov2i(hmargin, textY);
    charstr("escape key exits");
    textY -= textHeight * 3;

    str = "Press the space bar to return to gmemusage Viewer";
    swidth = strwidth(str);
    cmov2i(MINWIDTH / 2 - swidth / 2, textHeight);
    charstr(str);

    swapbuffers();
}

/*
 *  void
 *  SetThreshHold(long thresh)
 *
 *  Description:
 *      Specify a new threshhold
 *
 *  Parameters:
 *      thresh  the new threshhold
 */

void
SetThreshHold(long thresh)
{
    memThresh = thresh;
}

/*
 *  void
 *  WaitMessage(char *message)
 *
 *  Description:
 *      Display a wait message instead of bars
 *
 *  Parameters:
 *      message   the message to display
 *      detail    another line to display, if not NULL
 */

void
WaitMessage(char *message, char *detail)
{
    int swidth;
    int textY;

    color(bgColor);
    clear();

    color(textColor);
    textY = winHeight / 2 + halfTextHeight;

    swidth = strwidth(message);
    cmov2i(winWidth / 2 - swidth / 2, textY);
    charstr(message);
    textY -= textHeight * 2;
    
    if (detail) {
	swidth = strwidth(detail);
	cmov2i(winWidth / 2 - swidth / 2, textY);
	charstr(detail);
    }

    swapbuffers();
    XFlush(theDpy);
}
