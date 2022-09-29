#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xm/Xm.h>
#include <X11/Xm/RowColumn.h>
#include <X11/Xm/Label.h>

/*
 * Data structures for the window
 */
#define	SECOND		1000
struct WinData {
	char	*pathname;	/* pathname of RAID device */
	Boolean	showband;	/* T/F: show bandwidth too? */
	int	interval;	/* time between checks (secs) */
	Pixel	ok_color;	/* background color if RAID OK */
	Pixel	warn_color;	/* background color if RAID not OK */
	Widget	rowcolumn;	/* handle to window itself */
	Widget	label;		/* handle to label widget */
	Widget	bandlabel;	/* handle to bandwidth label widget */
	XmFontList fontlist;	/* font list for labels */
} windata;
XtAppContext appcontext;	/* application context for MOTIF */
void check_RAID(XtPointer, XtIntervalId *);

/*
 * Data structures for Xt argument and resource processing
 */
XrmOptionDescRec options[] = {
	{"-showband",	"Showband",	XrmoptionNoArg,  "True"},
	{"-interval",	"Interval",	XrmoptionSepArg, NULL},
	{"-okColor",	"OkColor",	XrmoptionSepArg, NULL},
	{"-warnColor",	"WarnColor",	XrmoptionSepArg, NULL},
};
XtResource resources[] = {
	{"okcolor", "OkColor", XtRPixel,
		sizeof(Pixel), XtOffsetOf(struct WinData, ok_color),
		XtRString, "green4"},
	{"warncolor", "WarnColor", XtRPixel,
		sizeof(Pixel), XtOffsetOf(struct WinData, warn_color),
		XtRString, "yellow1"},
	{"interval", "Interval", XtRInt,
		sizeof(int), XtOffsetOf(struct WinData, interval),
		XtRImmediate, (XtPointer) 15},
	{"showband", "Showband", XtRBoolean,
		sizeof(Boolean), XtOffsetOf(struct WinData, showband),
		XtRImmediate, (XtPointer) FALSE},
};


main(int argc, char *argv[])
{
	XFontStruct *font;
	Widget toplevel;
	Display *dpy;
	Arg arg[5];

	toplevel = XtAppInitialize(&appcontext, "RAID_Monitor",
				   options, XtNumber(options),
				   &argc, argv,
				   NULL, NULL, 0);
	if (argc != 2) {
		fprintf(stderr, "usage: %s [-showband] [-interval secs] [-okcolor name] [-warncolor name] devname\n",
				argv[0]);
		fprintf(stderr, "resources: raid_monitor.Showband  - show bandwidth too?              (false)\n");
		fprintf(stderr, "resources: raid_monitor.Interval  - seconds between status checks    (15)\n");
		fprintf(stderr, "resources: raid_monitor.OkColor   - background color if RAID OK      (green)\n");
		fprintf(stderr, "resources: raid_monitor.WarnColor - background color if RAID not OK  (yellow)\n");
		exit(1);


	}
	windata.pathname = argv[1];

	XtGetApplicationResources(toplevel,
				  &windata, resources, XtNumber(resources),
				  NULL, 0);

	dpy = XtDisplay(toplevel);
	font = XLoadQueryFont(dpy, "-*-*-*-*-*-*-*-360-*-*-*-*-*-*");
	windata.fontlist = XmFontListCreate(font, "charset1");

	/* create the text window */
	XtSetArg(arg[0], XmNorientation, XmHORIZONTAL);
	windata.rowcolumn = XmCreateRowColumn(toplevel, windata.pathname, arg, 1);
	XtManageChild(windata.rowcolumn);

	/* put the text into the window */
	XtSetArg(arg[0], XmNfontList, windata.fontlist);
	windata.label = XmCreateLabel(windata.rowcolumn, windata.pathname, arg, 1);
	XtManageChild(windata.label);

	if (windata.showband) {
		XtSetArg(arg[0], XmNfontList, windata.fontlist);
		windata.bandlabel = XmCreateLabel(windata.rowcolumn, "0", arg, 1);
		XtManageChild(windata.bandlabel);
	}

	/* open the RAID */
	status_open(windata.pathname);

	/* check the RAID status for the first time */
	check_RAID(NULL, NULL);

	/* make it all show up on the screen and jump into it */
	XtRealizeWidget(toplevel);
	XtAppMainLoop(appcontext);
}


/*
 * check RAID - see if anything has changed
 */
void
check_RAID(XtPointer stuff, XtIntervalId *elapsed)
{
	XmString newlabel;
	char value[80];
	Pixel color;
	Arg arg[5];

	if (windata.showband) {
		/* read bandwidth from stdin */
		strcpy(value, "    ");
		scanf("%s", &value[4]);
		strcat(value, "    ");
		newlabel = XmStringCreate(value, "charset1");
	}

	/* check the RAID for changes */
#ifdef TESTING
{ static int flip = 0; flip ^= 1; if (flip)
#else
	if (status_check())
#endif
		color = windata.warn_color;
	else
		color = windata.ok_color;
#ifdef TESTING
}
#endif

	XtSetArg(arg[0], XmNbackground, color);
	XtSetValues(windata.rowcolumn, arg, 1);

	XtSetArg(arg[0], XmNbackground, color);
	XtSetValues(windata.label, arg, 1);

	if (windata.showband) {
		XtSetArg(arg[0], XmNbackground, color);
		XtSetArg(arg[1], XmNlabelString, newlabel);
		XtSetValues(windata.bandlabel, arg, 2);
	}

	/* schedule next status check */
	XtAppAddTimeOut(appcontext, windata.interval * SECOND, check_RAID, NULL);
}
