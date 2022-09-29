#ident	"@(#)     xcrash/xcrash/xcrash.c     Ver. 1.1        Rel. PVTS 3.2        (93/01/13)"
/*
xcrash - Run tests attempting to crash and find bugs in the X server

Copyright 1992 by Hal Computer Systems, Incorporated.  All Rights Reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of HaL not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

HaL disclaims all warranties with regard to this software, including
all implied warranties of metchantability and fitness, in no event shall
HaL be liable for any special, indirect or consequential damages or
any damages whatsoever resulting from loss of use, data or profits,
whether in an action of contract, negligence or other tortious action,
arising out of or in connection with the use of performance of this
software.
*/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include "bitmap.xbm"

#ifdef SVR4
#define srandom(seed) srand(seed)
#define random() rand()
#endif

static char Xcrash_version[] = "xcrash_v1.3";

void Xcrash_error();

typedef struct Xcrash_struct
	{
	int		x,y,w,h;		/* current window size */
	int		scr;			/* Screen */
	int		depth;			/* depth of Screen */
	int		trace;			/* display trace of activity */
	int		quiet;			/* be very very quiet */
	int		numtests;		/* # of tests to perform */
	int		nasty;			/* perform nasty tests */
	int		seed;			/* random number seed */
	FILE		*infile;		/* input file */
	Display		*dpy;			/* Display */
	Window		win;			/* Window */
	GC		gc;			/* Graphics Context */
	Pixmap		pix;			/* current pixmap */
	Pixel		fg,bg;			/* fg and bg colors */
	} *Xcrash;

Xcrash	Xcrash_new(display,numtests,infile,trace,quiet,nasty,seed)
char *display,*infile;
int numtests,trace,quiet,nasty,seed;
/* Create a new instance of an Xcrash object. numtests is the number of
tests to perform.  quiet and trace are boolean values which tell me
how verbose I should be in displaying what Im doing.  infile is a file
to read data from (if null dont read).  display is a character string
display to connect to */
	{
	Xcrash c;
	Window root;

	c = (Xcrash)malloc(sizeof(struct Xcrash_struct));
	if (!c)
		return((Xcrash)0);
	c->numtests = numtests;
	c->trace = trace;
	c->quiet = quiet;
	c->nasty = nasty;
	c->seed = seed;
	if (infile)
		{
		c->infile =  fopen(infile,"r");
		if (!c->infile)
			{
			Xcrash_error(c,"ERROR: count not open input file");
			free(c);
			return((Xcrash)0);
			}
		}
	else
		c->infile = (FILE *)0;
	c->dpy = XOpenDisplay(display);
	if (!c->dpy)
		{
		if (c->infile)
			fclose(c->infile);
		Xcrash_error(c,"ERROR: could not open display");
		free(c);
		return ((Xcrash)0);
		}
        c->scr = DefaultScreen(c->dpy);
	c->fg = WhitePixel(c->dpy,c->scr);
	c->bg = BlackPixel(c->dpy,c->scr);
	root = RootWindow(c->dpy,c->scr);
	/* for color get a Direct Color visual here with at least 3/3/2 */
	c->win = XCreateSimpleWindow(c->dpy,root,0,0,600,600,1,c->fg,c->bg);
	c->x = 0;
	c->y = 0;
	c->w = 600;
	c->h = 600;
	XSelectInput(c->dpy,c->win,ExposureMask|StructureNotifyMask);
	XMapWindow(c->dpy,c->win);
        c->gc = XCreateGC(c->dpy,root,0,(XGCValues *)NULL);
	XSetPlaneMask(c->dpy,c->gc,AllPlanes);
	c->depth = DefaultDepth(c->dpy,c->scr);
        c->pix = XCreatePixmapFromBitmapData(c->dpy,c->win,
		(char *)bitmap_bits,bitmap_width,bitmap_height,
		c->fg,c->bg,c->depth);
	return(c);
	}

void	Xcrash_free(c)
Xcrash c;
/* Free an Xcrash object */
	{
	if (!c)
		return;
	if (c->infile)
		fclose(c->infile);
	XDestroyWindow(c->dpy,c->win);
	XFreePixmap(c->dpy,c->pix);
	XCloseDisplay(c->dpy);
	free(c);
	}

void Xcrash_error(c,msg)
Xcrash c;
char *msg;
/* Display message to stderr.  Not always an error even though youd
think so looking at the function name. */
	{
	if (!c->quiet)
		{
		fprintf(stderr,"xcrash: ");
		fprintf(stderr,msg);
		fprintf(stderr,"\n");
		fflush(stderr);
		}
	}

int Xcrash_ranint(c,name,max)
Xcrash c;
char *name;
int max;
/* Return an random integer with bounds 0 > value < max.  If Im reading
from an input stream, read the value from the stream.  Otherwise, Ill
generate the random value myself. */
	{
	static int initialized;
	int value,numread;
	char namein[100];

	if (!initialized)
		{
		if (c->seed == -1)
			srandom((int)time((time_t *)0));
		else
			srandom(c->seed);
		initialized = 1;
		}
	if (c->infile)
		{
		numread = fscanf(c->infile,"%90s%10d",namein,&value);
		if (numread == EOF)
			return(-1);
		if (numread != 2)
			{
			Xcrash_error(c,"ERROR: bad input file");
			return(-1);
			}
		if (strcmp(namein,name))
			{
			Xcrash_error(c,"ERROR: bad value name in input file");
			return(-1);
			}
		if (value < 0 | value > (max-1))
			{
			Xcrash_error(c,"ERROR: input value out of range");
			return(-1);
			}
		}
	else
		value = random()%max;
	if (c->trace)
		{
		printf ("%s %d\n",name,value);
		fflush(stdout);
		}
	return(value);
	}

void	Xcrash_random_xy(c,x,y)
Xcrash c;
short *x;
short *y;
/* Generate a random (can be out of screen bounds) value and place in x
and y */
	{
	if (!c->nasty)
		{
		*x = Xcrash_ranint(c,"x",c->w+10)-20;
		*y = Xcrash_ranint(c,"y",c->h+10)-20;
		}
	else
		{
		*x = Xcrash_ranint(c,"x",100000);
		*y = Xcrash_ranint(c,"y",100000);
		}
	}

void	Xcrash_change_GC(c,gcmask)
Xcrash c;
unsigned long gcmask;
/* Change the graphics context.  The mask signifies which parts are
possible for me to change */
	{
	XGCValues g;
	unsigned char dashes[5];
	unsigned long bitmask;
	int fontnum,loop,numdashes,dashoffset=0,numdashpixels;
	static XFontStruct *currentfont = 0;
	static char *fonts[] = 
		{
		"10x20","12x24","12x24kana","5x8","6x12",
		"8x13bold","8x16romankana","9x15bold","fixed",
		"heb6x13","kana14","kanji24","lucidasans-10",
		"lucidasans-bolditalic-14","lucidasanstypewriter-bold-8",
		"olcursor","olglyph-12","r16","rk16"
		};

	if (gcmask & GCArcMode)
		g.arc_mode = Xcrash_ranint(c,"am",2);
	if (gcmask & GCBackground)
		/* not used */
		gcmask ^= GCBackground;
	if (gcmask & GCCapStyle)
		g.cap_style = Xcrash_ranint(c,"cs",4);
	if (gcmask & GCClipMask)
		/* not used */
		gcmask ^= GCClipMask;
	if (gcmask & GCClipXOrigin)
		/* not used */
		gcmask ^= GCClipXOrigin;
	if (gcmask & GCClipYOrigin)
		/* not used */
		gcmask ^= GCClipYOrigin;
	if (gcmask & GCLineStyle)
		g.line_style = Xcrash_ranint(c,"ls",3);
	if (gcmask & GCDashList)
		{
		if (g.line_style != LineSolid)
			{
			numdashes = Xcrash_ranint(c,"nd",4);
			numdashes++;
			numdashpixels = 0;
			for (loop=0;loop<numdashes;loop++)
				{
				dashes[loop] = Xcrash_ranint(c,"ds",10);
				dashes[loop]++;
				numdashpixels += dashes[loop];
				}
			dashoffset = Xcrash_ranint(c,"do",numdashpixels);
			XSetDashes(c->dpy,c->gc,dashoffset,dashes,numdashes);
			}
		gcmask ^= GCDashList;
		}
	if (gcmask & GCDashOffset)
		/* performed in GCDashList */
		gcmask ^= GCDashOffset;
	if (gcmask & GCFillRule)
		g.fill_rule = Xcrash_ranint(c,"fr",2);
	if (gcmask & GCFillStyle)
		g.fill_style = Xcrash_ranint(c,"fs",4);
	if (gcmask & GCFont)
		{
		if (currentfont)
			XFreeFont(c->dpy,currentfont);
		fontnum = Xcrash_ranint(c,"ft",sizeof(fonts)/sizeof(char *));
		currentfont = XLoadQueryFont(c->dpy,fonts[fontnum]); 
		if (currentfont)
			g.font = currentfont->fid;
		else
			{
			char msg[120];
			sprintf(msg,"ERROR: No such font: %s", fonts[fontnum]);
			Xcrash_error(c,msg);
			gcmask ^= GCFont;
			}
		}
	if (gcmask & GCForeground)
		/* not used */
		gcmask ^= GCForeground;
		/*
		color - use direct color 3/3/2
		bg_color.red = Xcrash_ranint(c,"rd",8);
		bg_color.green = Xcrash_ranint(c,"gr",8);
		bg_color.blue = Xcrash_ranint(c,"bl",4);
		bg_color.flags = DoRed|DoGreen|DoBlue;
		XAllocColor(c->dpy,c->cmap,&bg_color);
		g.foreground = gc_color.pixel;
		*/
	if (gcmask & GCFunction)
		g.function = Xcrash_ranint(c,"func",16);
	if (gcmask & GCGraphicsExposures)
		g.graphics_exposures = Xcrash_ranint(c,"ge",2);
	if (gcmask & GCJoinStyle)
		g.join_style = Xcrash_ranint(c,"js",3);
	if (gcmask & GCLineWidth)
		g.line_width = Xcrash_ranint(c,"lw",50);
	if (gcmask & GCPlaneMask)
		{
		bitmask = 1 << Xcrash_ranint(c,"pm",c->depth);
		g.plane_mask ^= bitmask;
		}
	if (gcmask & GCStipple)
		/* not used */
		gcmask ^= GCStipple;
	if (gcmask & GCSubwindowMode)
		g.subwindow_mode = Xcrash_ranint(c,"sw",2);
	if (gcmask & GCTile)
		/* should create random pixmap */
		g.tile = c->pix;
	if (gcmask & GCTileStipXOrigin)
		/* not used */
		gcmask ^= GCTileStipXOrigin;
	if (gcmask & GCTileStipYOrigin)
		/* not used */
		gcmask ^= GCTileStipYOrigin;
	XChangeGC(c->dpy,c->gc,gcmask,&g);
	}

void	Xcrash_test_line(c)
Xcrash c;
	{
	unsigned long gcmask;
	int count,loop;
	XPoint *points;
	
	gcmask=	GCFunction|GCPlaneMask|GCLineWidth|GCLineStyle|
		GCCapStyle|GCJoinStyle|
		GCFillStyle|GCSubwindowMode|GCClipXOrigin|
		GCClipYOrigin|GCClipMask|GCForeground|GCBackground|
		GCTile|GCStipple|GCTileStipXOrigin|GCTileStipYOrigin|
		GCCapStyle|GCDashOffset|GCDashList;
	Xcrash_change_GC(c,gcmask);
	count = Xcrash_ranint(c,"count",10);
	count++;
	points = (XPoint *)malloc(sizeof(XPoint)*count);
	if (!points)
		return;
	for (loop=0;loop<count;loop++)
		Xcrash_random_xy(c,&points[loop].x,&points[loop].y);
	XDrawLines(c->dpy,c->win,c->gc,points,count,CoordModeOrigin);
	free(points);
	}

void	Xcrash_test_segment(c)
Xcrash c;
	{
	int count,loop;
	XSegment *segments;
	unsigned long gcmask;

	gcmask=	GCFunction|GCPlaneMask|GCLineWidth|GCLineStyle|
		GCCapStyle|GCJoinStyle|
		GCFillStyle|GCSubwindowMode|GCClipXOrigin|
		GCClipYOrigin|GCClipMask|GCForeground|GCBackground|
		GCTile|GCStipple|GCTileStipXOrigin|GCTileStipYOrigin|
		GCCapStyle|GCDashOffset|GCDashList;
	Xcrash_change_GC(c,gcmask);
	count = Xcrash_ranint(c,"count",10);
	count++;
	segments = (XSegment *)malloc(sizeof(XSegment)*count);
	if (!segments)
		return;
	for(loop=0;loop<count;loop++)
		{
		if (!loop)
			Xcrash_random_xy(c,&segments[0].x1,&segments[0].y1);
		else
			{
			segments[loop].x1=segments[loop-1].x2;
			segments[loop].y1=segments[loop-1].y2;
			}
		Xcrash_random_xy(c,&segments[loop].x2,&segments[loop].y2);
		}
	XDrawSegments(c->dpy,c->win,c->gc,segments,count);
	free(segments);
	}

void	Xcrash_test_point(c)
Xcrash c;
	{
	unsigned long gcmask;
	int loop,count;
	XPoint *points;

	gcmask=	GCFunction|GCPlaneMask|GCForeground|GCSubwindowMode|
		GCClipXOrigin|GCClipYOrigin|GCClipMask;
	Xcrash_change_GC(c,gcmask);
	count = Xcrash_ranint(c,"count",10);
	count++;
	points = (XPoint *)malloc(sizeof(XPoint)*count);
	if (!points)
		return;
	for (loop=0;loop<count;loop++)
		Xcrash_random_xy(c,&points[loop].x,&points[loop].y);
	XDrawPoints(c->dpy,c->win,c->gc,points,count,CoordModeOrigin);
	free(points);
	}

void	Xcrash_test_rect(c)
Xcrash c;
	{
	unsigned long gcmask;
	int count,loop;
	XRectangle *recs;

	gcmask=	GCFunction|GCPlaneMask|GCLineWidth|GCLineStyle|
		GCFillStyle|GCSubwindowMode|GCClipXOrigin|
		GCClipYOrigin|GCClipMask|GCForeground|GCBackground|
		GCTile|GCStipple|GCTileStipXOrigin|GCTileStipYOrigin|
		GCCapStyle|GCDashOffset|GCDashList;
	Xcrash_change_GC(c,gcmask);
	count = Xcrash_ranint(c,"count",8);
	count++;
	recs = (XRectangle *)malloc(sizeof(XRectangle)*count);
	if (!recs)
		return;
	for (loop=0;loop<count;loop++)
		{
		Xcrash_random_xy(c,&recs[loop].x,&recs[loop].y);
		recs[loop].width = Xcrash_ranint(c,"width",200);
		recs[loop].height = Xcrash_ranint(c,"height",200);
                }
	XDrawRectangles(c->dpy,c->win,c->gc,recs,count);
	XFillRectangles(c->dpy,c->win,c->gc,recs,count);
	free(recs);
	}

void	Xcrash_test_polygon(c)
Xcrash c;
	{
	unsigned long gcmask;
	int count,loop;
	XPoint *points;

	gcmask=	GCFunction|GCPlaneMask|GCFillStyle|GCFillRule|
		GCSubwindowMode|GCClipXOrigin|
		GCClipYOrigin|GCClipMask|GCForeground|GCBackground|
		GCTile|GCStipple|GCTileStipXOrigin|GCTileStipYOrigin;
	Xcrash_change_GC(c,gcmask);
	count = Xcrash_ranint(c,"count",10);
	count++;
	points =(XPoint *)malloc(sizeof(XPoint)*count);
	if (!points)
		return;
	for (loop=0;loop<count;loop++)
		Xcrash_random_xy(c,&points[loop].x,&points[loop].y);
	XFillPolygon(c->dpy,c->win,c->gc,points,count,Complex,CoordModeOrigin);
	free(points);
	}

void	Xcrash_test_arc(c)
Xcrash c;
	{
	XArc *arcs;
	int loop,count;
	unsigned long gcmask;

	gcmask=	GCFunction|GCPlaneMask|GCLineWidth|GCLineStyle|
		GCFillStyle|GCArcMode|GCSubwindowMode|GCClipXOrigin|
		GCClipYOrigin|GCClipMask|GCForeground|GCBackground|
		GCTile|GCStipple|GCTileStipXOrigin|GCTileStipYOrigin|
		GCCapStyle|GCDashOffset|GCDashList;
	Xcrash_change_GC(c,gcmask);
	count = Xcrash_ranint(c,"count",10);
	count++;
	arcs = (XArc *)malloc(sizeof(XArc)*count);
	if (!arcs)
		return;
	for (loop=0;loop<count;loop++)
		{
		Xcrash_random_xy(c,&arcs[loop].x,&arcs[loop].y);
		arcs[loop].width = Xcrash_ranint(c,"width",200);
		arcs[loop].height = Xcrash_ranint(c,"height",200);
		arcs[loop].angle1 = Xcrash_ranint(c,"ang1",400*64);
		arcs[loop].angle2 = Xcrash_ranint(c,"ang2",400*64);
                }
	XDrawArcs(c->dpy,c->win,c->gc,arcs,count);
	XFillArcs(c->dpy,c->win,c->gc,arcs,count);
	free(arcs);
	}

void	Xcrash_test_text(c)
Xcrash c;
	{
	short x,y;
	unsigned long gcmask;
	int loop;
	static char string[20] = "Hal Computers ";

	gcmask=	GCFunction|GCPlaneMask|GCFillStyle|
		GCFont|GCSubwindowMode|GCClipXOrigin|
		GCClipYOrigin|GCClipMask|GCForeground|
		GCBackground|GCTile|GCStipple|GCTileStipXOrigin|
		GCTileStipYOrigin;
	Xcrash_change_GC(c,gcmask);
	Xcrash_random_xy(c,&x,&y);
	for (loop=14;loop<20;loop++)
		string[loop] = Xcrash_ranint(c,"ch",256);
	XDrawString(c->dpy,c->win,c->gc,x,y,string,20);
	XDrawImageString(c->dpy,c->win,c->gc,x,y,string,20);
	}

void	Xcrash_test_text16(c)
Xcrash c;
	{
	short x,y;
	int loop;
	XChar2b string[4];
	unsigned long gcmask;

	gcmask=	GCFunction|GCPlaneMask|GCFillStyle|
		GCFont|GCSubwindowMode|GCClipXOrigin|
		GCClipYOrigin|GCClipMask|GCForeground|
		GCBackground|GCTile|GCStipple|GCTileStipXOrigin|
		GCTileStipYOrigin;
	Xcrash_change_GC(c,gcmask);
	Xcrash_random_xy(c,&x,&y);
	for (loop=0;loop<4;loop++)
		{
		string[loop].byte1 = Xcrash_ranint(c,"c1",256);
		string[loop].byte2 = Xcrash_ranint(c,"c2",256);
		}
	XDrawString16(c->dpy,c->win,c->gc,x,y,string,4);
	XDrawImageString16(c->dpy,c->win,c->gc,x,y,string,4);
	}

void	Xcrash_test_image(c)
Xcrash c;
	{
	XImage *image;
	short x1,y1,x2,y2,w,h;
	unsigned long gcmask;

	gcmask=	GCFunction|GCPlaneMask|GCSubwindowMode|
		GCClipXOrigin|GCClipYOrigin|
		GCClipMask|GCForeground|GCBackground;
	Xcrash_change_GC(c,gcmask);
	w = Xcrash_ranint(c,"w",c->w);
	h = Xcrash_ranint(c,"h",c->h);
	x1 = Xcrash_ranint(c,"x1",c->w - w);
	y1 = Xcrash_ranint(c,"y1",c->h - h);
	Xcrash_random_xy(c,&x2,&y2);
	image = XGetImage(c->dpy,c->win,x1,y1,w,h,AllPlanes,XYPixmap);
	if (image)
		{
		XPutImage(c->dpy,c->win,c->gc,image,0,0,x2,y2,w,h);
		XDestroyImage(image);
		}
	else
		Xcrash_error(c,"ERROR: XGetImage returned null");
	}

void	Xcrash_test_copy(c)
Xcrash c;
	{
	short x1,y1,x2,y2,w,h;
	unsigned long gcmask;
	int plane;

	gcmask=	GCFunction|GCPlaneMask|GCSubwindowMode|
		GCGraphicsExposures|GCClipXOrigin|GCClipYOrigin|
		GCClipMask|GCForeground|GCBackground;
	Xcrash_change_GC(c,gcmask);
	w = Xcrash_ranint(c,"w",c->w);
	h = Xcrash_ranint(c,"h",c->h);
	x1 = Xcrash_ranint(c,"x1",c->w - w);
	y1 = Xcrash_ranint(c,"y1",c->h - h);
	Xcrash_random_xy(c,&x2,&y2);
	XCopyArea(c->dpy,c->win,c->win,c->gc,x1,y1,w,h,x2,y2);
	plane = 1 << Xcrash_ranint(c,"pl",c->depth);
	XCopyPlane(c->dpy,c->win,c->win,c->gc,x1,y1,w,h,x2,y2,plane);
	}

void	Xcrash_flush_events(c)
Xcrash c;
/* Flush the X event queue, placing window geometry changes in myself */
	{
	XEvent pe;
	XExposeEvent *ee;
	XConfigureEvent *ce;

	while (!XPending(c->dpy))
		{
		XNextEvent(c->dpy, &pe);
		switch (pe.type)
			{
			case Expose:
				ee = (XExposeEvent *)&pe;
				while (ee->count)
					{
					XNextEvent(c->dpy,&pe);
					ee = (XExposeEvent *)&pe;
					}
				break;
			case ConfigureNotify:
				ce = (XConfigureEvent *)&pe;
				c->x = ce->x;
				c->y = ce->y;
				c->w = ce->width;
				c->h = ce->height;
				break;
			default:
				break;
			}
		}
	}

void	Xcrash_test_all(c)
Xcrash c;
/* Run through the different tests until the input file is empty, or
until I am out of tests. */
	{
	static int numtests;
	int currentfunc,done;
	static struct func_struct
		{
		void (*f)();
		char *name;
		} funcs[] =
		{
		{Xcrash_test_line,	"XDrawLines"},
		{Xcrash_test_segment,	"XDrawSegments"},
		{Xcrash_test_point,	"XDrawPoints"},
		{Xcrash_test_rect,	"XDrawRectangles/XFillRectangles"},
		{Xcrash_test_polygon,	"XFillPolygon"},
		{Xcrash_test_arc,	"XDrawArcs/XFillArcs"},
		{Xcrash_test_image,	"XGetImage/XPutImage"},
		{Xcrash_test_copy,	"XCopyArea/XCopyPlane"},
		{Xcrash_test_text,	"XDrawString/XDrawImageString"},
		{Xcrash_test_text16,	"XDrawString16/XDrawImageString16"}	
		};
		
	if (!c)
		return;
	numtests = sizeof(funcs)/sizeof(struct func_struct);
	Xcrash_ranint(c,Xcrash_version,1); /* version header for file */
	done = 0;
	while(!done)
		{
		Xcrash_flush_events(c);
		currentfunc = Xcrash_ranint(c,"function",numtests);
		if (currentfunc == -1)
			done = 1;
		else
			{
			Xcrash_error(c,funcs[currentfunc].name);
			funcs[currentfunc].f(c);
			XFlush(c->dpy);
			}
		if (!c->infile)
			done = (--(c->numtests) <= 0);
		}
	}

void Xcrash_usage()
	{
	fprintf (stderr,"usage: xcrash\n	");
	fprintf (stderr," [-display {host}] [-tests {number}]");
	fprintf (stderr," [-input {file}] [-quiet]\n	");
	fprintf (stderr," [-verbose] [-nasty] [-seed {number}]\n");
	}

int main(argc,argv)
int argc;
char **argv;
	{
	Xcrash c;
	int numtests=200,trace=0,quiet=0;
	int nasty=0,seed=-1,loop;
	char *infile=0,*display=0;

	for (loop=1;loop<argc;loop++)
		{
		if (argv[loop][0] != '-')
			{
			Xcrash_usage();
			return(1);
			}
		switch (argv[loop][1])
			{
			case 'd':	/* display */
				if (++loop >= argc)
					{
					Xcrash_usage();
					return(1);
					}
				display = argv[loop];
				break;
			case 'i':	/* input from file */
				if (++loop >= argc)
					{
					Xcrash_usage();
					return(1);
					}
				infile = argv[loop];
				break;
			case 'n':	/* nasty mode */
				nasty = 1;
				break;
			case 's':
				if (++loop > argc)
					{
					Xcrash_usage();
					return(1);
					}
				seed = atoi(argv[loop]);
				break;
			case 't':	/* number of tests */
				if (++loop > argc)
					{
					Xcrash_usage();
					return(1);
					}
				numtests = atoi(argv[loop]);
				break;
			case 'v':
				trace = 1;
				break;
			case 'q':
				quiet = 1;
				break;
			default:
				Xcrash_usage();
				return(1);
			}
		}

	c = Xcrash_new(display,numtests,infile,trace,quiet,nasty,seed);
	Xcrash_test_all(c);
	Xcrash_free(c);

	return(0);
	}
