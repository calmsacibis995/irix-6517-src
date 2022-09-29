/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 * 			All Rights Reserved.				  *
 *									  *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.; *
 * the contents of this file may not be disclosed to third parties,	  *
 * copied or duplicated in any form, in whole or in part, without the	  *
 * prior written permission of Silicon Graphics, Inc.			  *
 *									  *
 * RESTRICTED RIGHTS LEGEND:						  *
 *	Use, duplication or disclosure by the Government is subject to    *
 *	restrictions as set forth in subdivision (c)(1)(ii) of the Rights *
 *	in Technical Data and Computer Software clause at DFARS 	  *
 *	252.227-7013, and/or in similar or successor clauses in the FAR,  *
 *	DOD or NASA FAR Supplement. Unpublished - rights reserved under   *
 *	the Copyright Laws of the United States.			  *
 **************************************************************************
 *
 * File: TextView.c
 *
 * Description: Implementation file for the TextView multifont, file
 *	text viewing widget. The basis for this widget comes from Doug
 *	Young's 'fileviewer.c' example program in his book.
 *
 **************************************************************************/


#ident "$Revision: 1.4 $"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <bstring.h>
#include <ctype.h>
#include <libgen.h>
#include <Xm/XmP.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Xmu.h>
#include <X11/Xmu/Atoms.h>
#include <Xm/Frame.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include "Formatter.h"
#include "TextViewP.h"


/* Amount of overlap (lines) in page up/down */

#define PAGE_OVERLAP	2

/* Number of milliseconds before redrawing text during a scrollbar drag */

#define SCROLL_DELAY	100

/* Text selection tokens */

#define SELECT		1		/* Select text */
#define DESELECT	0		/* Deselect text */

#define END_CHAR	1e+6		/* Position at right end of line */
#define LINE_END	2e+6		/* Position passed right end of line */
#define TEXT_END	1e+6		/* Position passed bottom of text */

#define BLANK_LINE_XSEL	10		/* Empty line select x position */

/* Keyboard navigation tokens */

#define FIRST_PAGE	0
#define LAST_PAGE	1
#define LINE_UP		2
#define LINE_DOWN	3
#define PAGE_UP		4
#define PAGE_DOWN	5
#define COL_LEFT	6
#define COL_RIGHT	7

/* Resource macros */

#define DEFARGS(num)    Arg _args[num]; register int _n
#define STARTARGS       _n = 0
#define SETARG(r,v)     XtSetArg(_args[_n], r, v); _n++
#define ARGLIST         _args, _n


/* Widget resouce list */

#define offset(field)	XtOffsetOf(VwrTextViewRec, field)
static XtResource resources[] = {
	/* Columns of text */
	{
	VwrNtextColumns, VwrCTextColumns, XtRInt, sizeof(int),
	offset(textView.text_columns), XtRImmediate,
					(XtPointer)DEF_TEXT_COLS
	},
	/* Rows of text */
	{
	VwrNtextRows, VwrCTextRows, XtRInt, sizeof(int),
	offset(textView.text_rows), XtRImmediate,
					(XtPointer)DEF_TEXT_ROWS
	},
	/* Normal font */
	{
	VwrNnormFont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
	offset(textView.styles[NORMAL_STYLE].font), XtRString,
					DEF_NORM_FONT 
	},
	/* Underline font */
	{
	VwrNunderlineFont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
	offset(textView.styles[UNDERLINE_STYLE].font), XtRString,
					DEF_UNDERLINE_FONT
	},
	/* Bold font */
	{
	VwrNboldFont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
	offset(textView.styles[BOLD_STYLE].font), XtRString,
					DEF_BOLD_FONT
	},
	/* Bold-underline font */
	{
	VwrNboldUnderFont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
	offset(textView.styles[BOLDUNDER_STYLE].font), XtRString,
					DEF_BOLDUNDER_FONT
	},
	/* Normal text foreground color */
	{
	VwrNnormFore, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(textView.styles[NORMAL_STYLE].fore), XtRImmediate,
					(XtPointer)DEF_NORM_FORE 
	},
	/* Underline text foreground color */
	{
	VwrNunderlineFore, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(textView.styles[UNDERLINE_STYLE].fore), XtRImmediate,
					(XtPointer)DEF_UNDERLINE_FORE
	},
	/* Bold text foreground color */
	{
	VwrNboldFore, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(textView.styles[BOLD_STYLE].fore), XtRImmediate,
					(XtPointer)DEF_BOLD_FORE
	},
	/* Bold-underline text foreground color */
	{
	VwrNboldUnderFore, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(textView.styles[BOLDUNDER_STYLE].fore), XtRImmediate,
					(XtPointer)DEF_BOLDUNDER_FORE
	},
	/* Selection foreground color */
	{
	VwrNselectionFore, XtCBackground, XtRPixel, sizeof(Pixel),
	offset(textView.select_fore), XtRImmediate,
					(XtPointer)DEF_SELECT_FORE
	},
	/* Selection background color */
	{
	VwrNselectionBack, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(textView.select_back), XtRImmediate,
					(XtPointer)DEF_SELECT_BACK
	},
	/* Blank line compression */
	{
	VwrNblankCompress, VwrCBlankCompress, XtRInt, sizeof(int),
	offset(textView.blank_compress), XtRImmediate,
					(XtPointer)DEF_BLANK_COMPRESS
	},
	/* Text file to view */
	{
	VwrNtextFile, VwrCTextFile, XtRString, sizeof(char*),
	offset(textView.filename), XtRImmediate, (XtPointer)NULL
	},
	/* Vertical scrollbar to manage */
	{
	VwrNverticalScrollBar, VwrCScrollBar, XtRWidget, sizeof(Widget),
	offset(textView.vsb), XtRImmediate, (XtPointer)NULL
	},
	/* Horizontal scrollbar to manage */
	{
	VwrNhorizontalScrollBar, VwrCScrollBar, XtRWidget, sizeof(Widget),
	offset(textView.hsb), XtRImmediate, (XtPointer)NULL
	},
	/* Load Viewer callbacks */
	{
	VwrNloadCallback, VwrCLoadCallback, XtRCallback, sizeof(XtPointer),
	offset(textView.load_callback), XtRCallback, (XtPointer)NULL
	},
};


/* Action function declarations */

static void first_page(Widget, XEvent*, String*, Cardinal*);
static void last_page(Widget, XEvent*, String*, Cardinal*);
static void line_up(Widget, XEvent*, String*, Cardinal*);
static void line_down(Widget, XEvent*, String*, Cardinal*);
static void col_left(Widget, XEvent*, String*, Cardinal*);
static void col_right(Widget, XEvent*, String*, Cardinal*);
static void page_up(Widget, XEvent*, String*, Cardinal*);
static void page_down(Widget, XEvent*, String*, Cardinal*);
static void next_tgroup(Widget, XEvent*, String*, Cardinal*);
static void prev_tgroup(Widget, XEvent*, String*, Cardinal*);
static void start_select(Widget, XEvent*, String*, Cardinal*);
static void drag_select(Widget, XEvent*, String*, Cardinal*);
static void complete_select(Widget, XEvent*, String*, Cardinal*);


/* Translation table and action table definitions */

static char def_translations[] =
	"<Btn1Down>:		StartSelect()	\n\
	 <Btn1Motion>:		DragSelect()	\n\
	 <Btn1Up>:		CompleteSelect()\n\
	 <Key>Tab:		NextTabGroup()	\n\
	 Ctrl<Key>Tab:		NextTabGroup()	\n\
	 Shift<Key>Tab:		PrevTabGroup()	\n\
	 Ctrl Shift<Key>Tab:	PrevTabGroup()	\n\
	 <Key>osfBeginLine:	FirstPage()	\n\
	 <Key>KP_Home:		FirstPage()	\n\
	 <Key>KP_7:		FirstPage()	\n\
	 <Key>osfEndLine:	LastPage()	\n\
	 <Key>KP_End:		LastPage()	\n\
	 <Key>KP_1:		LastPage()	\n\
	 <Key>osfUp:		LineUp()	\n\
	 <Key>KP_Up:		LineUp()	\n\
	 <Key>KP_8:		LineUp()	\n\
	 <Key>osfDown:		LineDown()	\n\
	 <Key>KP_Down:		LineDown()	\n\
	 <Key>KP_2:		LineDown()	\n\
	 <Key>osfLeft:		ColLeft()	\n\
	 <Key>KP_Left:		ColLeft()	\n\
	 <Key>KP_4:		ColLeft()	\n\
	 <Key>osfRight:		ColRight()	\n\
	 <Key>KP_Right:		ColRight()	\n\
	 <Key>KP_6:		ColRight()	\n\
	 <Key>osfPageUp:	PageUp()	\n\
	 <Key>KP_Prior:		PageUp()	\n\
	 <Key>KP_9:		PageUp()	\n\
	 <Key>osfPageDown:	PageDown()	\n\
	 <Key>KP_Next:		PageDown()	\n\
	 <Key>KP_3:		PageDown()	\n\
	 <Key>space:		PageDown()";

static XtActionsRec actions[] = {
    {"FirstPage",	first_page},
    {"LastPage",	last_page},
    {"LineUp",		line_up},
    {"LineDown",	line_down},
    {"ColLeft",		col_left},
    {"ColRight",	col_right},
    {"PageUp",		page_up},
    {"PageDown",	page_down},
    {"NextTabGroup",	next_tgroup},
    {"PrevTabGroup",	prev_tgroup},
    {"StartSelect",	start_select},
    {"DragSelect",	drag_select},
    {"CompleteSelect",	complete_select},
};


/* Widget method declarations */

static void initialize(Widget, Widget, ArgList, Cardinal*);
static XtGeometryResult query_geometry(Widget, XtWidgetGeometry*,
					XtWidgetGeometry*);
static void expose(Widget, XEvent*, Region);
static Boolean set_values(Widget, Widget, Widget, ArgList, Cardinal*);
static void resize(Widget);
static void realize(Widget, XtValueMask*, XSetWindowAttributes*);
static void destroy(Widget);

/* Widget private method declarations */

static void	get_font_info(VwrTextViewPart*);
static void	create_gcs(VwrTextViewWidget);
static void	load_viewer(VwrTextViewWidget);
static void	clear_line(Display*, Window, VwrTextViewWidget, int, int, int);
static void	draw_line(Display*, Window, VwrTextViewWidget, int, int, int);
static void	vsb_dragged(Widget, VwrTextViewWidget,
				 XmScrollBarCallbackStruct*);
static void	vsb_moved(Widget, VwrTextViewWidget,
				 XmScrollBarCallbackStruct*);
static void	vert_scroll_text(VwrTextViewWidget, XtIntervalId*);
static void	horiz_scroll_text(Widget, VwrTextViewWidget,
				 XmScrollBarCallbackStruct*);
static void	vsb_callbacks(VwrTextViewWidget);
static void	hsb_callbacks(VwrTextViewWidget);
static void	handle_keyboard(VwrTextViewWidget, int);
static void	goto_line(VwrTextViewWidget, int);
static Boolean	transfer_selection(Widget, Atom*, Atom*, Atom*,
				XtPointer*, unsigned long*, int*);
static void	transfer_complete(Widget, Atom*, Atom*);
static char*	get_selection(VwrTextViewPart*, unsigned long*);
static void	lose_selection(Widget, Atom*);
static void	xy2lc(VwrTextViewPart*, int, int, int*, int*);
static void	lc2seg(VwrTextViewPart*, int, int, vwr_text_seg_t**, int*);
static int	y2l(VwrTextViewPart*, int);
static int	x2c(VwrTextViewPart*, int, int);
static int	selcmp(VwrPositionStruct*, VwrPositionStruct*);
static void	highlight(VwrTextViewWidget, VwrPositionStruct*,
				VwrPositionStruct*, int);
static vwr_text_seg_t*	get_seg(VwrTextViewPart*, VwrPositionStruct*,
				int, vwr_text_line_t*);
static Boolean	select_segs(vwr_text_seg_t*, vwr_text_seg_t*, int);
static void	draw_segs(VwrTextViewWidget, int, vwr_text_seg_t*,
				vwr_text_seg_t*, int);
static void	compact_line(VwrTextViewPart*, int);
static void	lc_bounds(VwrTextViewPart*, VwrPositionStruct*);
static void	init_selection(VwrTextViewPart*);
static void	parse_escapes(register char*, register char*);


/* External declarations */

extern char *__loc1;


/* Callback data */

static VwrTextViewLoadCallbackStruct load_callback_data;


/* Class record intialization */

VwrTextViewClassRec vwrTextViewClassRec = {
    {		/* Core class fields */
	/* superclass */		(WidgetClass) &xmPrimitiveClassRec,
	/* class_name */		"TextView",
	/* widget_size */		sizeof(VwrTextViewRec),
	/* class_initialize */		NULL,
	/* class_part_initialize */	NULL,
	/* class_inited */		False,
	/* initialize */		initialize,
	/* initialize_hook */		NULL,
	/* realize */			realize,
	/* actions */			actions,
	/* num_actions */		XtNumber(actions),
	/* resources */			resources,
	/* num_resources */		XtNumber(resources),
	/* xrm_class */			NULLQUARK,
	/* compress_motion */		True,
	/* compress_exposure */		XtExposeCompressMultiple,
	/* compress_enterleave */	True,
	/* visible_interest */		False,
	/* destroy */			destroy,
	/* resize */			resize,
	/* expose */			expose,
	/* set_values */		set_values,
	/* set_values_hook */		NULL,
	/* set_values_almost */		XtInheritSetValuesAlmost,
	/* get_values_hook */		NULL,
	/* accept_focus */		NULL,
	/* version */			XtVersion,
	/* callback_private */		NULL,
	/* tm_table */			def_translations,
	/* query_geometry */		query_geometry,
	/* display_accelerator */	XtInheritDisplayAccelerator,
	/* extension */			NULL,
    },
    {		/* Primitive class fields */
	/* border_highlight */		(XtWidgetProc)_XtInherit,
	/* border_unhighlight */	(XtWidgetProc)_XtInherit,
	/* translations */		def_translations,
	/* arm_and_activate */		NULL,
	/* syn_resources */		NULL,
	/* num_syn_resources */		0,
	/* extension */			NULL,
    },
    {		/* TextView class fields */
	/* extension */			NULL,
    },
};

WidgetClass vwrTextViewWidgetClass = (WidgetClass) &vwrTextViewClassRec;


/**************************************************************************
 *
 * Function: initialize
 *
 * Description: Widget intialization method
 *
 * Parameters: 
 *	treq (I) - widget as set by arg list, resource database and defaults
 *	tnew (I) - new widget
 *	args (I) - argument list in Va creation
 *	num_args (I) - number of arguments
 *	
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void initialize(Widget treq, Widget tnew, 
					ArgList args, Cardinal *num_args)
{
    VwrTextViewWidget new = (VwrTextViewWidget)tnew;
    VwrTextViewPart *tvp = &new->textView;
    register int min_dim;

    /*
     * Get the font statistics
     */
    get_font_info(tvp);

    /*
     * Calculate required canvas height based on required number of
     * text columns and rows and the dimensions of the font. Set
     * the core class size to this canvas size if either core
     * dimension is less than or equal to the highlight thickness.
     * Note that we test against twice the highlight and shadow
     * thicknesses because there is a highlight and shadow on both
     * sides (e.g. top and bottom).
     */
    tvp->def_height = tvp->text_rows * tvp->fontheight;
    tvp->def_width = tvp->text_columns * tvp->fontwidth;
    min_dim = 2 * new->primitive.highlight_thickness +
	      2 * new->primitive.shadow_thickness;
    if (new->core.height <= min_dim)
        new->core.height = tvp->def_height;
    if (new->core.width <= min_dim)
        new->core.width = tvp->def_width;

    /*
     * Nuke the border highlight and shadow
     */
    new->primitive.highlight_thickness = 0;
    new->primitive.shadow_thickness = 0;
    new->core.border_width = 0;

    /*
     * Create the program's GCs
     */
    create_gcs(new);

    /*
     * Init text buffer, top line index and scrolling related vars
     */
    tvp->top = 0;
    tvp->top_pending = 0;
    tvp->left = 0;
    tvp->timer = NULL;
    VwrInitText(&tvp->text);

    /*
     * Init selection variables
     */
    init_selection(tvp);

    /*
     * Init text search variables
     */
    tvp->comp_exp = NULL;
    tvp->search_next = NULL;

    /*
     * Init Xmu selection atom caching
     */
    (void)XmuInternAtom(XtDisplay(new), XmuMakeAtom("NULL"));

    /*
     * If a vertical scrollbar has been specified, register callbacks for it
     */
    if (tvp->vsb)
	vsb_callbacks(new);

    /*
     * If a horizontal scrollbar has been specified, register callbacks for it
     */
    if (tvp->hsb)
	hsb_callbacks(new);
}


/**************************************************************************
 *
 * Function: realize
 *
 * Description: Performs task at the time the widget is realised
 *
 * Parameters: 
 *	w (I) - this widget
 *	mask (I) - mask for window attributes
 *	attribs (I) - window attributes struct
 *
 * Return: none
 *
 **************************************************************************/

static void realize(Widget w, XtValueMask *mask, XSetWindowAttributes *attribs)
{
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Call the supreclass realize
     */
    (*(&xmPrimitiveClassRec)->core_class.realize)(w, mask, attribs);

    /*
     * If a file has been specified, load it into the viewer
     */
    if (tvp->filename)
	load_viewer(tv);
}


/**************************************************************************
 *
 * Function: query_geometry
 *
 * Description: Responds to a query for widget size changes
 *
 * Parameters: 
 *	w (I) - this widget
 *	proposed (I) - proposed new geometry
 *	answer (I) - suggested geometry
 *
 * Return: Response to geometry change request
 *
 **************************************************************************/
 /* ARGSUSED */

static XtGeometryResult query_geometry(Widget w, XtWidgetGeometry *proposed,
					XtWidgetGeometry *answer)
{
    unsigned int amask = 0, pmask;
    XtGeometryResult result;
    VwrTextViewWidget tv = (VwrTextViewWidget)w;

    pmask = proposed->request_mode;
    amask = CWWidth | CWHeight;
    if ((pmask & amask) == amask) {
	answer->width = (proposed->width < tv->textView.def_width) ?
						tv->textView.def_width:
						proposed->width;
	answer->height = (proposed->height < tv->textView.def_height) ?
						tv->textView.def_height:
						proposed->height;
	if ((proposed->width == answer->width) &&
	    (proposed->height == answer->height))
	    result = XtGeometryYes;
	else 
	    result = XtGeometryAlmost;
    }
    else {
	amask = 0;
	result = XtGeometryNo;
    }

    answer->request_mode = amask;
    return result;
}


/**************************************************************************
 *
 * Function: expose
 *
 * Description: Handles the expose events, redrawing the text viewer.
 *
 * Parameters: 
 *	w (I) - this widget
 *	event (I) - expose event structure
 *	region (I) - composite region of exposure
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void expose(Widget w, XEvent *event, Region region)
{
    register int yloc, ind;
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Verify that we are realized
     */
    if (!XtIsRealized(tv))
	return;

    /*
     * Loop through each line until the bottom of the window is reached, or
     * we run out of lines. Redraw any lines that intersect the exposed
     * region.
     */
    ind = tvp->top;
    yloc = 0;
    while (ind < tvp->text.nlines && yloc < tv->core.height) {
	if (XRectInRegion(region, 0, yloc,
				tv->core.width, tvp->fontheight) !=
				RectangleOut)
	    draw_line(XtDisplay((Widget)tv), XtWindow(tv), tv, ind, tvp->left,
								yloc);
	yloc += tvp->fontheight;
	ind++;
    }
}


/**************************************************************************
 *
 * Function: resize
 *
 * Description: Handles the resizing of the text viewer.
 *
 * Parameters: 
 *	w (I) - this widget
 *
 * Return: none
 *
 **************************************************************************/

static void resize(Widget w)
{
    DEFARGS(10);
    int delta;
    int slider_size, scroll_max, scroll_value;
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Reset the vertical scrollbar slider to indicate the relative
     * proportion of text displayed and also the new page size.
     * We need to scroll the text down in those situations where
     * we are at the bottom of the text and the window grows in
     * in size.
     */
    if (tvp->vsb) {
        if ((slider_size = tv->core.height / tvp->fontheight) < 1)
	    slider_size = 1;
	STARTARGS;
	SETARG(XmNmaximum, &scroll_max);
	SETARG(XmNvalue, &scroll_value);
        XtGetValues(tvp->vsb, ARGLIST);
        if (slider_size > scroll_max || !tvp->text.nlines)
	    slider_size = scroll_max;
        delta = scroll_max - slider_size;
        if (scroll_value > delta)
	    tvp->top = scroll_value = (delta < 0) ? 0: delta;
	STARTARGS;
	SETARG(XmNsliderSize, slider_size);
	SETARG(XmNpageIncrement, slider_size);
	SETARG(XmNvalue, scroll_value);
        XtSetValues(tvp->vsb, ARGLIST);
    }

    /*
     * Reset the horizontal scrollbar slider to indicate the relative
     * proportion of text displayed and also the new page size.
     */
    if (tvp->hsb) {
        if ((slider_size = tv->core.width) < tvp->fontwidth)
	    slider_size = tvp->fontwidth;
	STARTARGS;
	SETARG(XmNmaximum, &scroll_max);
	SETARG(XmNvalue, &scroll_value);
        XtGetValues(tvp->hsb, ARGLIST);
        if (slider_size > scroll_max || !tvp->text.max_line_len)
	    slider_size = scroll_max;
        delta = scroll_max - slider_size;
        if (scroll_value > delta) {
	    scroll_value = (delta < 0) ? 0: delta;
	    tvp->left = -scroll_value;
	}
	STARTARGS;
	SETARG(XmNsliderSize, slider_size);
	SETARG(XmNpageIncrement, slider_size);
	SETARG(XmNvalue, scroll_value);
        XtSetValues(tvp->hsb,ARGLIST);
    }
}


/**************************************************************************
 *
 * Function: set_values
 *
 * Description: Handles the setting of widget resources using the
 *	SetValues Xt function
 *
 * Parameters: 
 *	cur (I) - current settings of widget variables
 *	req (I) - request values if widget varaibles not processed yet
 *		  by superclass
 *	new (I) - new widget values already processed by superclass
 *	args (I) - arguments to SetValues function
 *	num_args (I) - number of arguments in args
 *
 * Return: True if XCleaerArea to be called and expose event generated,
 *	False if no redisplay requireno redisplay required.
 *
 **************************************************************************/
/* ARGSUSED */

static Boolean set_values(Widget cur, Widget req, Widget new,
				ArgList args, Cardinal *num_args)
{
    Boolean do_expose = False;
    VwrTextViewWidget ctv = (VwrTextViewWidget) cur;
    VwrTextViewWidget ntv = (VwrTextViewWidget) new;

    /*
     * New text file to load
     */
    if (ctv->textView.filename == NULL) {
	if (ntv->textView.filename != NULL) {
	    load_viewer(ntv);
	    do_expose = True;
	}
    }
    else if (ntv->textView.filename != NULL) {
	load_viewer(ntv);
	do_expose = True;
    }

    /*
     * Scrollbars
     */
    if ((ctv->textView.vsb != ntv->textView.vsb) && (ntv->textView.vsb)) {
	vsb_callbacks(ntv);
	resize(new);
	do_expose = True;
    }
    if ((ctv->textView.hsb != ntv->textView.hsb) && (ntv->textView.hsb)) {
	hsb_callbacks(ntv);
	resize(new);
	do_expose = True;
    }

    return do_expose;
}


/**************************************************************************
 *
 * Function: destroy
 *
 * Description: Handles destruction of the widget
 *
 * Parameters: 
 *	w (I) - this widget
 *
 * Return: none
 *
 **************************************************************************/

static void destroy(Widget w)
{
    int i;
    VwrTextViewWidget tv = (VwrTextViewWidget)w;

    /*
     * Free the GCs
     */
    for (i = 0; i < NUM_STYLES; i++)
        XtReleaseGC(w, tv->textView.styles[i].gc);
    XtReleaseGC(w, tv->textView.fill_gc);
    XtReleaseGC(w, tv->textView.select_fill_gc);
}


/*
 ==========================================================================
			 	ACTION FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: first_page, last_page, line_up, line_down, page_up, page_down,
 *	     next_tgroup, prev_tgroup
 *
 * Description: Keyboard action functions to navigate the document and
 *		support tab groups
 *
 * Parameters: 
 *	w (I) - this widget
 *	event (I) - event that triggered the action
 *	args (I) - action parameters
 *	nargs (I) - number of parameters
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void first_page(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    handle_keyboard((VwrTextViewWidget)w, FIRST_PAGE);
}

/* ARGSUSED */

static void last_page(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    handle_keyboard((VwrTextViewWidget)w, LAST_PAGE);
}

/* ARGSUSED */

static void line_up(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    handle_keyboard((VwrTextViewWidget)w, LINE_UP);
}

/* ARGSUSED */

static void line_down(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    handle_keyboard((VwrTextViewWidget)w, LINE_DOWN);
}

/* ARGSUSED */

static void col_left(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    handle_keyboard((VwrTextViewWidget)w, COL_LEFT);
}

/* ARGSUSED */

static void col_right(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    handle_keyboard((VwrTextViewWidget)w, COL_RIGHT);
}

/* ARGSUSED */

static void page_up(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    handle_keyboard((VwrTextViewWidget)w, PAGE_UP);
}

/* ARGSUSED */

static void page_down(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    handle_keyboard((VwrTextViewWidget)w, PAGE_DOWN);
}

/* ARGSUSED */

static void next_tgroup(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
}

/* ARGSUSED */

static void prev_tgroup(Widget w, XEvent *event, String *args, Cardinal *nargs)
{
    XmProcessTraversal(w, XmTRAVERSE_PREV_TAB_GROUP);
}


/**************************************************************************
 *
 * Function: start_select
 *
 * Description: Begins a new ICCCM text selection
 *
 * Parameters: 
 *	w (I) - this widget
 *	event (I) - event that triggered the action
 *	args (I) - action parameters
 *	nargs (I) - number of parameters
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void start_select(Widget w, XEvent *event, String *args,
							Cardinal *nargs)
{
    VwrTextViewPart *tvp = &((VwrTextViewWidget)w)->textView;
    XButtonEvent *xbe = (XButtonEvent*)event;

    /*
     * Unhighlight any previous selection
     */
    lose_selection(w, NULL);

    /*
     * Map the pointer coordinates into a line and column for
     * the mark location
     */
    xy2lc(tvp, xbe->x, xbe->y, &tvp->sel_mark.line, &tvp->sel_mark.col);

    /*
     * Assume the mark and point start out at the same place
     */
    tvp->sel_point = tvp->sel_mark;
}


/**************************************************************************
 *
 * Function: drag_select
 *
 * Description: Handles the dragging of the ICCCM selection
 *
 * Parameters: 
 *	w (I) - this widget
 *	event (I) - event that triggered the action
 *	args (I) - action parameters
 *	nargs (I) - number of parameters
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void drag_select(Widget w, XEvent *event, String *args,
							Cardinal *nargs)
{
    int cmp, cmp_new;
    VwrPositionStruct new_p;
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;
    XMotionEvent *xme = (XMotionEvent*)event;

    /*
     * Map the pointer coordinates to line and column position
     * for the point position
     */
    xy2lc(tvp, xme->x, xme->y, &new_p.line, &new_p.col);

    /*
     * This bit of machinery avoids the clearing and redrawing of
     * the entire selection every time the pointer is moved. Only
     * the part of the selection that is back over is cleared
     */
    cmp = selcmp(&tvp->sel_point, &tvp->sel_mark);
    cmp_new = selcmp(&new_p, &tvp->sel_point);
    if (cmp < 0 && cmp_new > 0)
	highlight(tv, &tvp->sel_point, &new_p, DESELECT);
    else if (cmp > 0 && cmp_new < 0) {
	highlight(tv, &new_p, &tvp->sel_point, DESELECT);
    }

    /*
     * Assign the new position to be the point
     */
    tvp->sel_point.line = new_p.line;
    tvp->sel_point.col = new_p.col;

    /*
     * Highlight the new selection. We make sure the we
     * pass the pistionally inferior point as the first
     * argument and the positionally superior point as
     * the second positional argument
     */
    if ((cmp = selcmp(&tvp->sel_mark, &tvp->sel_point)) < 0)
	highlight(tv, &tvp->sel_mark, &tvp->sel_point, SELECT);
    else if (cmp > 0)
	highlight(tv, &tvp->sel_point, &tvp->sel_mark, SELECT);
}


/**************************************************************************
 *
 * Function: complete_select
 *
 * Description: Handles gaining ownership of the selection when the
 *	selecting process is complete
 *
 * Parameters: 
 *	w (I) - this widget
 *	event (I) - event that triggered the action
 *	args (I) - action parameters
 *	nargs (I) - number of parameters
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void complete_select(Widget w, XEvent *event, String *args,
							Cardinal *nargs)
{
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;
    XButtonEvent *xbe = (XButtonEvent*)event;

    /*
     * If there is a selection, attempt to own it.
     * If the ownership request fails print a warning
     * and unhighlight the selection
     */
    if (selcmp(&tvp->sel_mark, &tvp->sel_point)) {
	if (XtOwnSelection(w, XA_PRIMARY, xbe->time, transfer_selection,
			lose_selection, transfer_complete) == False) {
	    XtWarning("TextView - cannot own selection, try again\n");
	    lose_selection(w, NULL);
	}
    }
}


/*
 ==========================================================================
			 	PRIVATE FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: get_font_info
 *
 * Description: Gets the font size statistics and stores and stores them
 *	in the instance structure.
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *
 * Return: none
 *
 **************************************************************************/

static void get_font_info(VwrTextViewPart *tvp)
{
    XFontStruct *norm_style, *under_style, *bold_style, *bunder_style;
    int norm_width, under_width, bold_width, bunder_width;

    /*
     * Get pointers to the style fonts
     */
    norm_style = tvp->styles[NORMAL_STYLE].font;
    under_style = tvp->styles[UNDERLINE_STYLE].font;
    bold_style = tvp->styles[BOLD_STYLE].font;
    bunder_style = tvp->styles[BOLDUNDER_STYLE].font;

    /*
     * Determine maximum font height
     */
    tvp->fontheight = TV_MAX(norm_style->ascent + norm_style->descent,
    				under_style->ascent + under_style->descent);
    tvp->fontheight = TV_MAX(tvp->fontheight,
				bold_style->ascent + bold_style->descent);
    tvp->fontheight = TV_MAX(tvp->fontheight,
				bunder_style->ascent + bunder_style->descent);


    /*
     * Determine maximum font ascent
     */
    tvp->fontascent = TV_MAX(norm_style->ascent, under_style->ascent);
    tvp->fontascent = TV_MAX(tvp->fontascent, bold_style->ascent);
    tvp->fontascent = TV_MAX(tvp->fontascent, bunder_style->ascent);

    /*
     * Determine maximum font width
     *
     * 		If the per_char field of the XFontStruct is NULL
     *		then the font is fixed width and the max_bounds.width
     *		is used to determine the width. If the per_char field
     *		is non-NULL then we have a proportional font and we
     *		use the width of the 'n' character to determine the
     *		width (the max width for proportional fonts is too large).
     *
     *		The max of the three font styles is used for the width.
     */
    norm_width = TV_FWIDTH(norm_style);
    under_width = TV_FWIDTH(under_style);
    bold_width = TV_FWIDTH(bold_style);
    bunder_width = TV_FWIDTH(norm_style);

    tvp->fontwidth = TV_MAX(norm_width, under_width);
    tvp->fontwidth = TV_MAX(tvp->fontwidth, bold_width);
    tvp->fontwidth = TV_MAX(tvp->fontwidth, bunder_width);
}


/**************************************************************************
 *
 * Function: create_gcs
 *
 * Description: Creates the Graphics Contexts for the program
 *
 * Parameters: 
 *	tv (I) - this widget
 *	
 * Return: none
 *
 **************************************************************************/

static void create_gcs(VwrTextViewWidget tv)
{
    XGCValues gcv;
    XtGCMask mask;
    register int i;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Create the GCs for drawing text in the various fonts
     */
    mask = GCFont | GCForeground | GCBackground;
    gcv.background = tv->core.background_pixel;
    for (i = 0; i < NUM_NORM_STYLES; i++) {
        gcv.font = tvp->styles[i].font->fid;
        gcv.foreground = tvp->styles[i].fore;
        gcv.foreground = (tvp->styles[i].fore == DEF_COLOR) ?
				tv->primitive.foreground: tvp->styles[i].fore;
        tvp->styles[i].gc = XtGetGC((Widget)tv, mask, &gcv);
    }

    /*
     * Create the GCs for drawing selected text in the various fonts
     */
    mask = GCFont | GCForeground | GCBackground;
    gcv.background = (tvp->select_back == DEF_COLOR) ?
				tv->primitive.foreground: tvp->select_back;
    gcv.foreground = (tvp->select_fore == DEF_COLOR) ?
				tv->core.background_pixel: tvp->select_fore;
    for (i = 0; i < NUM_NORM_STYLES; i++) {
        gcv.font = tvp->styles[i].font->fid;
        tvp->styles[i | SELECT_MASK].gc = XtGetGC((Widget)tv, mask, &gcv);
        tvp->styles[i | SELECT_MASK].font = tvp->styles[i].font;
    }

    /*
     * Create line clearing GCs
     */
    mask = GCFont | GCForeground | GCBackground;
    gcv.foreground = gcv.background = tv->core.background_pixel;
    for (i = 0; i < NUM_NORM_STYLES; i++) {
        gcv.font = tvp->styles[i].font->fid;
        tvp->blanks[i] = XtGetGC((Widget)tv, mask, &gcv);
    }

    /*
     * Create the blank line and end of line fill GC
     */
    mask = GCForeground;
    gcv.foreground = tv->core.background_pixel;
    tvp->fill_gc = XtGetGC((Widget)tv, mask, &gcv);
    gcv.foreground = (tvp->select_back == DEF_COLOR) ?
				tv->primitive.foreground: tvp->select_back;
    tvp->select_fill_gc = XtGetGC((Widget)tv, mask, &gcv);
}


/**************************************************************************
 *
 * Function: load_viewer
 *
 * Description: Reads into the viewer the file specified by the textFile
 *	resource.
 *
 * Parameters:
 *	tv (I) - this widget
 *
 * Return: none
 *
 **************************************************************************/

static void load_viewer(VwrTextViewWidget tv)
{
    DEFARGS(15);
    int slider_size;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Make sure we have a file to read
     */
    if (tvp->filename == NULL)
    	return;

    /*
     * The selection must be reinitialized so that we do not carry
     * any old selection information into a new text display where
     * the old information is meaningless (and can cause core dumps).
     */
    init_selection(tvp);

    /*
     * Clear the text search start address
     */
    tvp->search_next = NULL;

    /*
     * Read the nroff output text into the widget state structure
     * text buffer
     */
    if (VwrReadText(tvp->filename, &tvp->text, tvp->styles,
					tvp->blank_compress) < 0) {
	XtWarning("TextView - cannot load text\n");
	return;
    }

    /*
     * Position viewer at top of text and to the left edge
     */
    tvp->top = 0;
    tvp->left = 0;

    /*
     * Set the vertical scrollbar so that movements are reported in terms
     * of lines of text. Set the scrolling increment to a single line, and
     * the page increment to the number of lines the canvas widget can hold.
     * Also set the slider size to be proportional.
     */
    if (tvp->vsb) {
        if ((slider_size = tv->core.height / tvp->fontheight) < 1)
	    slider_size = 1;
        if (slider_size > tvp->text.nlines)
	    slider_size = tvp->text.nlines;
	STARTARGS;
        SETARG(XmNminimum, 0);
        SETARG(XmNmaximum, tvp->text.nlines);
        SETARG(XmNincrement, 1);
        SETARG(XmNsliderSize, slider_size);
        SETARG(XmNpageIncrement, slider_size);
        SETARG(XmNvalue, tvp->top);
        XtSetValues(tvp->vsb, ARGLIST);
    }

    /*
     * Set the horizontal scrollbar so that movements are reported in terms
     * of pixels. Set the scrolling increment to a single pixel, and
     * the page increment to the number of pixel the canvas widget can hold.
     * Also set the slider size to be proportional.
     */
    if (tvp->hsb) {
        if ((slider_size = tv->core.width) < tvp->fontwidth)
	    slider_size = tvp->fontwidth;
        if (tv->core.width >= tvp->text.max_line_len)
	    slider_size = tvp->text.max_line_len;
	STARTARGS;
        SETARG(XmNminimum, 0);
        SETARG(XmNmaximum, tvp->text.max_line_len);
        SETARG(XmNincrement, tvp->fontwidth);
        SETARG(XmNsliderSize, slider_size);
        SETARG(XmNpageIncrement, slider_size);
        SETARG(XmNvalue, tvp->left);
        XtSetValues(tvp->hsb, ARGLIST);
    }

    /*
     * Call any callbacks that the user has registered for file load
     */
    load_callback_data.filename = tvp->filename;
    XtCallCallbacks((Widget)tv, VwrNloadCallback,
					(XtPointer)&load_callback_data);
}


/**************************************************************************
 *
 * Function: vsb_dragged
 *
 * Description: Handles vertical scrollbar drags.
 *
 * Parameters: 
 *	w (I) - scrollbar widget that invoked the callback
 *	tv (I) - text viewer widget
 *	call_data (I) - standard scrollbar callback data structure
 *
 * Return: none
 *
 **************************************************************************/

static void vsb_dragged(Widget w, VwrTextViewWidget tv,
			     XmScrollBarCallbackStruct *call_data)
{
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * We keep track of the scrollbar position and set a 
     * timer. Only when the timer expires do we actually
     * redraw the text
     */
    tvp->top_pending = call_data->value;
    if (!tvp->timer)
	tvp->timer = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
				SCROLL_DELAY,
				(XtTimerCallbackProc)vert_scroll_text,
				(XtPointer)tv);
}


/**************************************************************************
 *
 * Function: vsb_moved
 *
 * Description: Handles vertical scrollbar value changes.
 *
 * Parameters: 
 *	w (I) - scrollbar widget that invoked the callback
 *	tv (I) - text viewer widget
 *	call_data (I) - standard scrollbar callback data structure
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void vsb_moved(Widget w, VwrTextViewWidget tv,
			     XmScrollBarCallbackStruct *call_data)
{
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Here we redraw the text on demand. We clear any
     * pending timers just in case
     */
    if (tvp->timer)
	XtRemoveTimeOut(tvp->timer);
    tv->textView.top_pending = call_data->value;
    vert_scroll_text(tv, NULL);
}


/**************************************************************************
 *
 * Function: vert_scroll_text
 *
 * Description: Scrolls the text vertically in the text viewer
 *
 * Parameters: 
 *	tv (I) - this widget
 *	timer (I) - ID of timer when called as timeout proc
 *
 * Return: none
 *
 **************************************************************************/
 /* ARGSUSED */

static void vert_scroll_text(VwrTextViewWidget tv, XtIntervalId *timer)
{
    register int yloc, ind, old_ind;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Clear any timers
     */
    tvp->timer = NULL;

    /*
     * Assign the pending scrollbar position to the scrollbar
     * value variable. Clean each line of the old text and draw
     * the new text
     */
    if (tvp->top == tvp->top_pending)
	return;
    old_ind = tvp->top;
    ind = tvp->top = tvp->top_pending;
    yloc = 0;
    while (ind < tvp->text.nlines && yloc < tv->core.height) {
	clear_line(XtDisplay((Widget)tv), XtWindow(tv), tv, old_ind,
						tvp->left, yloc);
	draw_line(XtDisplay((Widget)tv), XtWindow(tv), tv, ind, 
						tvp->left, yloc);
	yloc += tvp->fontheight;
	old_ind++;
	ind++;
    }
    
    /*
     * If the viewer is sized larger than the bottom of the text
     * then clear the gap at the bottom of junk
     */
    if (yloc < tv->core.height) {
        XFillRectangle(XtDisplay((Widget)tv), XtWindow(tv), tvp->fill_gc,
			0, yloc,
			tv->core.width, tv->core.height - yloc);
    }
}


/**************************************************************************
 *
 * Function: horiz_scroll_text
 *
 * Description: Handles horizontal scrollbar value changes.
 *
 * Parameters: 
 *	w (I) - scrollbar widget that invoked the callback
 *	tv (I) - text viewer widget
 *	call_data (I) - standard scrollbar callback data structure
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void horiz_scroll_text(Widget w, VwrTextViewWidget tv,
			      XmScrollBarCallbackStruct *call_data)
{
    VwrTextViewPart *tvp = &tv->textView;
    register int yloc, ind;

    /*
     * Assign the scrollbar position to the scrollbar
     * value variable. Clean each line of the old text and draw
     * the new text at scrolled position.
     */
    if (tvp->left == -call_data->value)
	return;
    ind = tvp->top;
    yloc = 0;
    while (ind < tvp->text.nlines && yloc < tv->core.height) {
	clear_line(XtDisplay((Widget)tv), XtWindow(tv), tv, ind,
						tvp->left, yloc);
	draw_line(XtDisplay((Widget)tv), XtWindow(tv), tv, ind,
						-call_data->value, yloc);
	yloc += tvp->fontheight;
	ind++;
    }
    tvp->left = -call_data->value;
    
    /*
     * If the viewer is sized larger than the bottom of the text
     * then clear the gap at the bottom of junk
     */
    if (yloc < tv->core.height) {
        XFillRectangle(XtDisplay((Widget)tv), XtWindow(tv), tvp->fill_gc,
			0, yloc,
			tv->core.width, tv->core.height - yloc);
    }
}


/**************************************************************************
 *
 * Function: vsb_callbacks
 *
 * Description: Registers the widget vertical scrollbar management code
 *	with the vertical scrollbar callbacks
 *
 * Parameters: 
 *	tv (I) - this widget
 *
 * Return: none
 *
 **************************************************************************/

static void vsb_callbacks(VwrTextViewWidget tv)
{
    XtAddCallback(tv->textView.vsb, XmNdecrementCallback,
		  (XtCallbackProc)vsb_moved, (XtPointer)tv);
    XtAddCallback(tv->textView.vsb, XmNincrementCallback,
		  (XtCallbackProc)vsb_moved, (XtPointer)tv);
    XtAddCallback(tv->textView.vsb, XmNpageDecrementCallback,
		  (XtCallbackProc)vsb_moved, (XtPointer)tv);
    XtAddCallback(tv->textView.vsb, XmNpageIncrementCallback,
		  (XtCallbackProc)vsb_moved, (XtPointer)tv);
    XtAddCallback(tv->textView.vsb, XmNtoBottomCallback,
		  (XtCallbackProc)vsb_moved, (XtPointer)tv);
    XtAddCallback(tv->textView.vsb, XmNtoTopCallback,
		  (XtCallbackProc)vsb_moved, (XtPointer)tv);
    XtAddCallback(tv->textView.vsb, XmNdragCallback,
		  (XtCallbackProc)vsb_dragged, (XtPointer)tv);
}


/**************************************************************************
 *
 * Function: hsb_callbacks
 *
 * Description: Registers the widget horizontal scrollbar management code
 *	with the horizontal scrollbar callbacks
 *
 * Parameters: 
 *	tv (I) - this widget
 *
 * Return: none
 *
 **************************************************************************/

static void hsb_callbacks(VwrTextViewWidget tv)
{
    XtAddCallback(tv->textView.hsb, XmNdecrementCallback,
		  (XtCallbackProc)horiz_scroll_text, (XtPointer)tv);
    XtAddCallback(tv->textView.hsb, XmNincrementCallback,
		  (XtCallbackProc)horiz_scroll_text, (XtPointer)tv);
    XtAddCallback(tv->textView.hsb, XmNpageDecrementCallback,
		  (XtCallbackProc)horiz_scroll_text, (XtPointer)tv);
    XtAddCallback(tv->textView.hsb, XmNpageIncrementCallback,
		  (XtCallbackProc)horiz_scroll_text, (XtPointer)tv);
    XtAddCallback(tv->textView.hsb, XmNtoBottomCallback,
		  (XtCallbackProc)horiz_scroll_text, (XtPointer)tv);
    XtAddCallback(tv->textView.hsb, XmNtoTopCallback,
		  (XtCallbackProc)horiz_scroll_text, (XtPointer)tv);
    XtAddCallback(tv->textView.hsb, XmNdragCallback,
		  (XtCallbackProc)horiz_scroll_text, (XtPointer)tv);
}


/**************************************************************************
 *
 * Function: draw_line
 *
 * Description: Draws a single line of text made up of string segments.
 *	Each segment has a different GC for changes in font and/or color.
 *
 * Parameters: 
 *	disp (I) - X server connection pointer
 *	win (I) - window in which to draw
 *	tv (I) - this widget
 *	ind (I) - starting index in line array to start drawing text
 *	xloc (I) - starting x location in viewer to start drawing text
 *	yloc (I) - starting y location in viewer to start drawing text
 *
 * Return: none
 *
 **************************************************************************/

static void draw_line(Display *disp, Window win, VwrTextViewWidget tv,
		      int ind, int xloc, int yloc)
{
    register int x, y, fill_width;
    register char *cptr;
    register vwr_text_line_t *lptr;
    register vwr_text_seg_t *tsptr;
    register VwrTextViewPart *tvp = &tv->textView;

    if (ind < 0 || ind >= tvp->text.nlines)
	return;
    lptr = &tvp->text.lines[ind];

    /*
     * If there is text on the line, draw it and fill any remaing
     * area after the text with the fill GC
     */
    if (lptr->line_len) {
        x = xloc;
        y = yloc + tvp->fontascent;
        cptr = lptr->line_chars;
        for (tsptr = lptr->segs; tsptr; tsptr = tsptr->next) {
            XDrawImageString(disp, win, tvp->styles[tsptr->seg_type].gc, x, y,
			     cptr, tsptr->seg_nchars);
	    x += tsptr->seg_len;
	    cptr += tsptr->seg_nchars;
        }
	if ((fill_width = tv->core.width - ((int)lptr->line_len + xloc)) > 0) {
	    XFillRectangle(disp, win, (lptr->end_select)?
	    			tvp->select_fill_gc: tvp->fill_gc,
	       			lptr->line_len + xloc, yloc,
	      			fill_width, tvp->fontheight - 1);
	}
    /*
     * If there is no text on the line simply fill the line with the
     * fill GC
     */
    } else {
        XFillRectangle(disp, win, (lptr->end_select)?
			tvp->select_fill_gc: tvp->fill_gc,
			0, yloc,
			tv->core.width, tvp->fontheight - 1);
    }
}


/**************************************************************************
 *
 * Function: clear_line
 *
 * Description: Clears a line of text
 *
 * Parameters: 
 *	disp (I) - X server connection pointer
 *	win (I) - window in which to draw
 *	tv (I) - this widget
 *	ind (I) - starting index in line array to start clearing text
 *	xloc (I) - starting x location in viewer to start clearing text
 *	yloc (I) - starting y location in viewer to start clearing text
 *
 * Return: none
 *
 **************************************************************************/

static void clear_line(Display *disp, Window win, VwrTextViewWidget tv,
		      int ind, int xloc, int yloc)
{
    register int x, y;
    register char *cptr;
    register vwr_text_line_t *lptr;
    register vwr_text_seg_t *tsptr;
    register VwrTextViewPart *tvp = &tv->textView;

    if (ind < 0 || ind >= tvp->text.nlines)
	return;
    lptr = &tvp->text.lines[ind];

    /*
     * If there is text on the line, clear it
     */
    if (lptr->line_len) {
        x = xloc;
        y = yloc + tvp->fontascent;
        cptr = lptr->line_chars;
        for (tsptr = lptr->segs; tsptr; tsptr = tsptr->next) {
            XDrawImageString(disp, win,
			tvp->blanks[tsptr->seg_type & ~SELECT_MASK], x, y,
			cptr, tsptr->seg_nchars);
	    x += tsptr->seg_len;
	    cptr += tsptr->seg_nchars;
        }
    }
}


/**************************************************************************
 *
 * Function: handle_keyboard
 *
 * Description: Handles keyboard navigation from the action functions
 *
 * Parameters: 
 *	tv (I) - this widget
 *	nav (I) - direction token (FIRST_PAGE, etc.)
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void handle_keyboard(VwrTextViewWidget tv, int nav)
{
    XEvent event;
    register int top_line, left_x;
    int old_top, max_top;
    int old_left_x;
    int page_amount;
    DEFARGS(5);
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * If no lines, there is nothing to do
     */
    if (!tvp->text.nlines)
	return;

    /*
     * Get current top line and compute page and doc size metrics
     */
    old_top = top_line = tvp->top;
    page_amount = tv->core.height / tvp->fontheight;
    if ((max_top = tvp->text.nlines - page_amount) < 0)
	max_top = 0;

    /*
     * Get left x position
     */
    old_left_x = left_x = tvp->left;

    switch (nav) {
	case FIRST_PAGE:
	    top_line = 0;
	    break;
	case LAST_PAGE:
	    top_line = max_top;
	    break;
	case LINE_UP:
	    if (--top_line < 0)
		top_line = 0;
	    break;
	case LINE_DOWN:
	    if (++top_line > max_top)
		top_line = max_top;
	    break;
	case COL_LEFT:
	    left_x += tvp->fontwidth;
	    if (left_x > 0)
		left_x = 0;
	    break;
	case COL_RIGHT:
	    left_x -= tvp->fontwidth;
	    if ((tv->core.width - left_x) > tvp->text.max_line_len)
		left_x = tv->core.width - tvp->text.max_line_len;
	    if (left_x > 0)
		left_x = 0;
	    break;
	case PAGE_UP:
	    top_line -= (page_amount - PAGE_OVERLAP);
	    if (top_line < 0)
		top_line = 0;
	    break;
	case PAGE_DOWN:
	    top_line += (page_amount - PAGE_OVERLAP);
	    if (top_line > max_top)
		top_line = max_top;
	    break;
	default:			/* Unknown nav token */
	    return;
    }

    /*
     * Flush pending KeyPress events from queue to minimize
     * run-on after auto-repeat
     */
    while (XCheckMaskEvent(XtDisplay((Widget)tv), KeyPressMask, &event));

    /* 
     * Don't scroll vertically if no change there
     */
    if (old_top != top_line)
        goto_line(tv, top_line);

    /* 
     * Don't scroll horizontally if no change there
     */
    if (old_left_x != left_x) {
        XmScrollBarCallbackStruct hsb_call_data;
	STARTARGS;
	SETARG(XmNvalue, -left_x);
        XtSetValues(tvp->hsb, ARGLIST);
        hsb_call_data.value = -left_x;
        horiz_scroll_text(tvp->hsb, tv, &hsb_call_data);
    }
}


/**************************************************************************
 *
 * Function: goto_line
 *
 * Description: Scrolls the text display so that the specified line number
 *	is the top line in the display. No bounding or validity checking
 *	is performed.
 *
 * Parameters: 
 *	tv (I) - this widget
 *	line (I) - line number to place at the top of the viewer
 *
 * Return: none
 *
 **************************************************************************/

static void goto_line(VwrTextViewWidget tv, int line)
{
    DEFARGS(5);
    XmScrollBarCallbackStruct vsb_call_data;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Move scrollbar slider to new position
     * Fill a scrollbar callback struct with what is needed
     * Call the scrollbar movement handler
     */
    STARTARGS;
    SETARG(XmNvalue, line);
    XtSetValues(tvp->vsb, ARGLIST);
    vsb_call_data.value = line;
    vsb_moved(tvp->vsb, tv, &vsb_call_data);
}


/**************************************************************************
 *
 * Function: transfer_selection
 *
 * Description: Transfers the selection to the requestor
 *
 * Parameters: 
 *	w (I) - this widget
 *	selection (I) - type of selection requested
 *	target (I) - type of target selection
 *	type_return (O) - type of selection returned
 *	value_return (O) - selection data
 *	length_return (O) - length of returned selection data
 *	format_return (O) - format of returned data selection
 *
 * Return: True if the transfer was completed, False otherwise.
 *
 **************************************************************************/

static Boolean transfer_selection(Widget w, Atom *selection,
					Atom *target, Atom *type_return,
					XtPointer *value_return,
					unsigned long *length_return,
					int *format_return)
{
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;
    XSelectionRequestEvent *req = XtGetSelectionRequest(w, *selection,
							(XtRequestId)NULL);
    static Atom text_atom = None;

    /*
     * Motif defines the TEXT atom
     */
    if (text_atom == None)
        text_atom = XInternAtom(XtDisplay(tv), "TEXT", True);

    /*
     * Handle the XA_TARGETS ICCCM atom. This code
     * is direct from O'Reilly Vol 4. This is crazy and
     * should be wraped up - X really falls short here
     */
    if (*target == XA_TARGETS(XtDisplay(tv))) {
	Atom *targetP;
	Atom *std_targets;
	unsigned long std_length;
	XmuConvertStandardSelection(w, req->time, selection,
					target, type_return,
					(caddr_t*)&std_targets,
					&std_length, format_return);
	*value_return = XtMalloc(sizeof(Atom) * (std_length + 1));
	targetP = *(Atom**)value_return;
	*length_return = std_length + 1;
	*targetP++ = XA_STRING;
	bcopy((void*)std_targets, (void*)targetP,
				(int)(sizeof(Atom) * std_length));
	XtFree((char*)std_targets);
	*type_return = XA_ATOM;
	*format_return = sizeof(Atom) * 8;
	return True;
    }
    /*
     * This handles actual useful requests for the selected text
     */
    else if ((*target != None) &&
		((*target == XA_STRING) || (*target == text_atom))) {
	*value_return = tvp->sel_data = get_selection(tvp, length_return);
	*type_return = *target;
	*format_return = 8;
	return True;
    }
    /*
     * This is more ICCCM nonsense and is verbatim O'Reilly
     */
    else {
	if (XmuConvertStandardSelection(w, CurrentTime, selection,
					target, type_return,
					(caddr_t*)value_return,
					length_return, format_return))
	    return True;
	else {
	    XtWarning("TextView - unsupported selection requested\n");
	    return False;
	}
    }
}


/**************************************************************************
 *
 * Function: transfer_complete
 *
 * Description: Cleans up after a selection transfer
 *
 * Parameters: 
 *	w (I) - this widget
 *	selection (I) - selection type
 *	target (I) - selection target type
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void transfer_complete(Widget w, Atom *selection, Atom *target)
{
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Free the selection text buffer if one was allocated
     */
    if (tvp->sel_data) {
	XtFree((char*)tvp->sel_data);
	tvp->sel_data = NULL;
    }
}


/**************************************************************************
 *
 * Function: get_selection
 *
 * Description: Places the currently selected text into a selection
 *	buffer.
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *	lenp (O) - amount of data in selection
 *
 * Return: Pointer to allocated selection buffer
 *
 **************************************************************************/

static char* get_selection(VwrTextViewPart *tvp, unsigned long *lenp)
{
    int line1, line2;
    int cmp;
    char *data, *dptr, *cptr;
    register int i, j;
    register vwr_text_seg_t *seg;
    register vwr_text_line_t *lptr;

    /*
     * Determine the selection line span
     */
    if ((cmp = selcmp(&tvp->sel_mark, &tvp->sel_point)) < 0) {
	line1 = tvp->sel_mark.line;
	line2 = tvp->sel_point.line;
    } else if (cmp > 0) {
	line1 = tvp->sel_point.line;
	line2 = tvp->sel_mark.line;
    } else {
	*lenp = 0;
	return NULL;
    }

    /*
     * Do some bounds checking on the line positions
     */
    if (line1 == TEXT_END || line2 < 0) {
	*lenp = 0;
	return NULL;
    }
    if (line1 < 0)
        line1 = 0;
    if (line2 == TEXT_END)
	line2 = tvp->text.nlines - 1;

    /*
     * Determine the amount of data to be transfered
     */
    *lenp = 0;
    for (i = line1; i <= line2; i++) {
	lptr = &tvp->text.lines[i];
	for (seg = lptr->segs; seg; seg = seg->next) {
	    if (seg->seg_type & SELECT_MASK)
		*lenp += seg->seg_nchars;
	}
	if (lptr->end_select)
	    (*lenp)++;
    }

    /*
     * Allocate the buffer for the transfer data
     */
    dptr = data = (char*)XtMalloc(*lenp + 1);

    /*
     * Fill the buffer with the selected data
     */
    for (i = line1; i <= line2; i++) {
	lptr = &tvp->text.lines[i];
	cptr = lptr->line_chars;
	for (seg = lptr->segs; seg; seg = seg->next) {
	    if (seg->seg_type & SELECT_MASK) {
		for (j = 0; j < seg->seg_nchars; j++) {
		   *dptr++ = *cptr++;
		}
	    } else
		cptr += seg->seg_nchars;
	}
	if (lptr->end_select)
	    *dptr++ = '\n';
    }
    *dptr = '\0';

    return data;
}


/**************************************************************************
 *
 * Function: lose_selection
 *
 * Description: Handles the loss of selection (unhighlighting).
 *
 * Parameters: 
 *	w (I) - this widget
 *	atom (I) - atom that describes the selection type
 *
 * Return: 
 *
 **************************************************************************/
/* ARGSUSED */

static void lose_selection(Widget w, Atom *sel_atom)
{
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;
    int cmp;

    /*
     * Make sure we pass in the inferior position as first
     * argument
     */
    if ((cmp = selcmp(&tvp->sel_mark, &tvp->sel_point)) < 0)
	highlight(tv, &tvp->sel_mark, &tvp->sel_point, DESELECT);
    else if (cmp > 0)
	highlight(tv, &tvp->sel_point, &tvp->sel_mark, DESELECT);
}


/**************************************************************************
 *
 * Function: xy2lc
 *
 * Description: Converts a mouse pointer x,y coordinate pair to the
 *	corresponding line,col pair for use in selections.
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *	x,y (I) - pointer coordinates
 *	linep (O) - line corresponding to y coordinate. If the y position
 *		    is less than 0 then linep will be set to -TEXT_END. If
 *		    y is greater than the viewer max y then linep will be
 *		    set to TEXT_END.
 *	colp (O) - column corresponding to the x coordinate. colp will
 *		   be set to -LINE_END if x < 0 and LINE_END if X > line
 *		   length. Newline chars are handled by setting colp
 *		   to END_CHAR.
 *
 * Return: none
 *
 **************************************************************************/

static void xy2lc(VwrTextViewPart *tvp, int x, int y, int *linep, int *colp)
{
    /*
     * Determine the line
     */
    *linep = y2l(tvp, y);

    /*
     * Do bounds checking on the line
     */
    if ((*linep == TEXT_END) || (*linep == -TEXT_END)) {
	*colp = LINE_END;
	return;
    }

    /*
     * Determine the column
     */
    *colp = x2c(tvp, *linep, x);
}


/**************************************************************************
 *
 * Function: lc2seg
 *
 * Description: Converts a line,column pair into a segment and offset
 *	into that segment.
 *
 * Parameters: 
 *	tvp (I) - this widget's isntance part
 *	line (I) - line number (0 base)
 *	col (I) - column number (0 base)
 *	segp (O) - segment pointer
 *	offp (O) - offset into segment
 *
 * Return: none
 *
 **************************************************************************/

static void lc2seg(VwrTextViewPart *tvp, int line, int col,
					vwr_text_seg_t **segp, int *offp)
{
    vwr_text_line_t *lptr;
    register vwr_text_seg_t *tsptr;
    register int nchars;

    /*
     * Bounds checking
     */
    if (line < 0 || line == TEXT_END || col < 0 || col == LINE_END ||
							col == END_CHAR) {
	*segp = NULL;
	*offp = 0;
	return;
    }

    lptr = &tvp->text.lines[line];

    /*
     * More bounds checking
     */
    if (col == 0) {
	*segp = lptr->segs;
	*offp = 0;
	return;
    }

    /*
     * Search for the segment
     */
    nchars = 0;
    for (tsptr = lptr->segs; tsptr; tsptr = tsptr->next) {
	if (nchars + tsptr->seg_nchars > col) {
	    *segp = tsptr;
	    *offp = col - nchars;
	    break;
	}
	nchars += tsptr->seg_nchars;
    }
}


/**************************************************************************
 *
 * Function: y2l
 *
 * Description: Converts a y pixel coordinate into a 0 based line number.
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *	y (I) - y pixel coordinate
 *
 * Return: The line number is returned.
 *
 **************************************************************************/

static int y2l(VwrTextViewPart *tvp, int y)
{
    int line;

    /*
     * Calculate the line
     */
    line = y / tvp->fontheight + tvp->top;

    /*
     * Do bounds checking on the line
     */
    if (line >= tvp->text.nlines)
	return TEXT_END;
    if (line < 0)
	return -TEXT_END;

    return line;
}


/**************************************************************************
 *
 * Function: x2c
 *
 * Description: Converts an x pixel coordinate into a 0 based text line
 *	character position (column number).
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *	line (I) - line number
 *	x (I) - y pixel coordinate
 *
 * Return: The column number is returned.
 *
 **************************************************************************/

static int x2c(VwrTextViewPart *tvp, int line, int x)
{
    vwr_text_line_t *lptr;
    XFontStruct *font;
    int col;
    register char *cptr;
    register int xnew, xold, len;
    register vwr_text_seg_t *tsptr;

    /*
     * Compensate for horizontal scrolling
     */
    x -= tvp->left;

    /*
     * Get a pointer to the line structure
     */
    lptr = &tvp->text.lines[line];

    /*
     * If the line is empty and we have not gone far enough
     * in x to select it return 0
     */
    if ((!lptr->line_len) && (x < BLANK_LINE_XSEL))
	return 0;

    /*
     * Bounds checking
     */
    if (x > (int)lptr->line_len)
	return LINE_END;
    if (x < 0)
	return -LINE_END;

    /*
     * Calculate the column. There is complication in that we
     * must decide which character is closer to the x coord.
     */
    col = LINE_END;
    len = 0;
    cptr = lptr->line_chars;
    for (tsptr = lptr->segs; tsptr; tsptr = tsptr->next) {
	if (len + tsptr->seg_len > x) {
	    font = tvp->styles[tsptr->seg_type].font;
	    xold = xnew = len;
	    while ((xnew += XTextWidth(font, cptr, 1)) <= x) {
		cptr++;
		xold = xnew;
	    }
	    if ((xnew - x) < (x - xold))
		cptr++;
	    col = (*cptr == '\n') ? END_CHAR: (int)(cptr - lptr->line_chars);
	    break;
	}
        len += tsptr->seg_len;
        cptr += tsptr->seg_nchars;
    }

    return col;
}


/**************************************************************************
 *
 * Function: selcmp
 *
 * Description: Compares two selection points and returns a value less
 *	than, equal to or greater than 0 depending on whether p1 is
 *	positionally less than, equal to or greater than p2.
 *
 * Parameters: 
 *	p1, p2 - points to compare
 *
 * Return: p1 < p2 returns < 0; p1 == p2 returns 0; p1 > p2 returns > 0
 *
 **************************************************************************/

static int selcmp(VwrPositionStruct *p1, VwrPositionStruct *p2)
{
    register int delta;

    if ((delta = p1->line - p2->line) != 0)
	return delta;
    if ((delta = p1->col - p2->col) != 0)
	return delta;
    return 0;
}


/**************************************************************************
 *
 * Function: highlight
 *
 * Description: Highlights the text from point p1 to point p2. It is
 *	assumed that point p2 is poisitionally greater than point p1.
 *
 * Parameters: 
 *	tv (I) - this widget
 *	p1, p2 (I) - text span to highlight
 *	sel_flag (I) - one of SELECT or DESELECT
 *
 * Return: none
 *
 **************************************************************************/

static void highlight(VwrTextViewWidget tv, VwrPositionStruct *p1,
					VwrPositionStruct *p2, int sel_flag)
{
    register Boolean changed;
    register int i, yloc;
    register int line1, line2;
    register vwr_text_line_t *lptr, *lptr1, *lptr2;
    vwr_text_seg_t *start_seg, *end_seg;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Bounds check the lines
     */
    if (p1->line == TEXT_END || p2->line < 0)
	return;
    if (p1->line < 0)
        line1 = 0;
    else
        line1 = p1->line + 1;

    /*
     * Fill the lines between p1 and p2, if any
     */
    if (line1 < tvp->text.nlines) {
	if (p2->line > 0) {
            if (p2->line == TEXT_END)
                line2 = tvp->text.nlines - 1;
	    else
		line2 = p2->line - 1;
            yloc = (line1 - tvp->top) * tvp->fontheight;
            for (i = line1; i <= line2; i++) {
                lptr = &tvp->text.lines[i];
		changed = select_segs(lptr->segs, NULL, sel_flag);
		if (changed || (lptr->end_select != sel_flag)) {
	            lptr->end_select = sel_flag;
    		    compact_line(tvp, i);
	            draw_line(XtDisplay((Widget)tv), XtWindow(tv),
					tv, i, tvp->left, yloc);
		}
	        yloc += tvp->fontheight;
            }
	}
    }

    /*
     * Highlight the p1 and p2 lines
     */
    start_seg = end_seg = NULL;

    line1 = (p1->line < 0) ? 0: p1->line;
    line2 = (p2->line == TEXT_END) ? tvp->text.nlines - 1: p2->line;

    lptr1 = &tvp->text.lines[line1];
    lptr2 = &tvp->text.lines[line2];

    start_seg = get_seg(tvp, p1, sel_flag, lptr1);

    /*
     * If the start line and end line are the same
     */
    if (line1 == line2) {
	if (p2->col >= 0) {
	    end_seg = get_seg(tvp, p2, sel_flag, lptr1);
	}
	draw_segs(tv, line1, start_seg, end_seg, sel_flag);
    /*
     * If the end line is greater than the start line
     * (end < start cannot happen)
     */
    } else {
	lptr1->end_select = sel_flag;
	draw_segs(tv, line1, start_seg, end_seg, sel_flag);
        start_seg = end_seg = NULL;
	if (p2->col >= 0) {
	    end_seg = get_seg(tvp, p2, sel_flag, lptr2);
	    draw_segs(tv, line2, lptr2->segs, end_seg, sel_flag);
	}
    }
}


/**************************************************************************
 *
 * Function: get_seg
 *
 * Description: Locates the segment containing the specified point.
 *	If the point lies at the beginning of the segment then that
 *	segment is returned intact. If the point lies within the
 *	segment, the segment is split and the new segment, which
 *	starts with the point, is returned. The newly created
 *	segment inherits the attributes of the old segment except for
 *	character count and length.
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *	point (I) - point to locate
 *	sel_flag (I) - selection flag (SELECT or DESELECT)
 *	lptr (I) - if the column is passed the end of the line
 *		   (col == LINE_END) then the segment returned is
 *		   NULL and the end select flag is set to sel_flag.
 *		   If the returned segment is non-NULL, this flag
 *		   is left unchanged.
 *
 * Return: Pointer to the segment that starts with the point. If the
 *	   col is LINE_END then the pointer returned is NULL and the
 *	   end_select flag is set to the specified sel_flag. If the
 *	   return value is non-NULL then the end_select is left 
 *	   unchanged.
 *
 **************************************************************************/

static vwr_text_seg_t* get_seg(VwrTextViewPart *tvp, VwrPositionStruct *point,
				int sel_flag, vwr_text_line_t *lptr)
{
    int off;
    vwr_text_seg_t *seg, *new_seg;

    /*
     * Determine if the column is passed the end of the line
     */
    if (point->col == LINE_END) {
	lptr->end_select = sel_flag;
	return NULL;
    }

    /*
     * Find the segment and offset that corresponds to
     * the specified line and column
     */
    lc2seg(tvp, point->line, (point->col < 0) ? 0: point->col, &seg, &off);

    /*
     * If there was an offset into segment then the col does
     * not occur at the start of a segment. We need to split
     * the segment into two so that a segment starts at the
     * specified col
     */
    if (off) {
	lptr = &tvp->text.lines[point->line];

        new_seg = (vwr_text_seg_t*)XtMalloc(sizeof(vwr_text_seg_t));
        new_seg->next = seg->next;
        new_seg->seg_type = seg->seg_type;
        new_seg->seg_nchars = seg->seg_nchars - off;
        new_seg->seg_len = XTextWidth(tvp->styles[seg->seg_type].font,
		    		    &lptr->line_chars[point->col],
		    		    new_seg->seg_nchars);
        seg->next = new_seg;
        seg->seg_nchars = off;
        seg->seg_len -= new_seg->seg_len;
        seg = new_seg;
    }

    return seg;
}


/**************************************************************************
 *
 * Function: select_segs
 *
 * Description: Selects all segments starting at the specified start
 *	segment, up to but not including the specified ending segment
 *
 * Parameters: 
 *	start_seg, end_seg (I) - segment span to select
 *	sel_flag (I) - one of SELECT or DESELECT
 *
 * Return: False if all segments in the specified segment span were already
 *	in the state specified by sel_flag. True if at least one segment
 *	required a state change.
 *
 **************************************************************************/

static Boolean select_segs(vwr_text_seg_t *start_seg, vwr_text_seg_t *end_seg,
						int sel_flag)
{
    register vwr_text_seg_t *tsptr;
    register Boolean changed = False;

    if (start_seg) {
	if (sel_flag)
            for (tsptr = start_seg; tsptr != end_seg; tsptr = tsptr->next) {
		if (!(tsptr->seg_type & SELECT_MASK)) {
		    changed = True;
	            tsptr->seg_type |= SELECT_MASK;
		}
            }
	else
            for (tsptr = start_seg; tsptr != end_seg; tsptr = tsptr->next) {
		if (tsptr->seg_type & SELECT_MASK) {
		    changed = True;
	            tsptr->seg_type &= ~SELECT_MASK;
		}
            }
    }

    return changed;
}


/**************************************************************************
 *
 * Function: draw_segs
 *
 * Description: Draws a selected line on the screen
 *
 * Parameters: 
 *	tv (I) - this widget
 *	line (I) - line number (0 base)
 *	start_seg (I) - starting segment to select
 *	end_seg (I) - end segment to select
 *	sel_flag (I) - one of SELECT or DESELECT
 *	
 * Return: none
 *
 **************************************************************************/

static void draw_segs(VwrTextViewWidget tv, int line,
			vwr_text_seg_t *start_seg, vwr_text_seg_t *end_seg,
			int sel_flag)
{
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Highlight or unhighlight the specified segments in the line
     */
    (void)select_segs(start_seg, end_seg, sel_flag);

    /*
     * Coalesce segments fragmented by the selection drag process
     */
    compact_line(tvp, line);

    /*
     * Draw the selected line
     */
    draw_line(XtDisplay((Widget)tv), XtWindow(tv), tv, line, tvp->left,
    				(line - tvp->top) * tvp->fontheight);
}


/**************************************************************************
 *
 * Function: compact_line
 *
 * Description: Coalesces the contiguous segments on a line that have
 *	the same font and selection attributes.
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *	line (I) - line number to compact
 *
 * Return: none
 *
 **************************************************************************/

static void compact_line(VwrTextViewPart *tvp, int line)
{
    register vwr_text_line_t *lptr;
    register vwr_text_seg_t *seg, *next_seg;

    lptr = &tvp->text.lines[line];
    if ((seg = lptr->segs) == NULL)
	return;
    while (seg->next) {
	next_seg = seg->next;
	if (seg->seg_type == next_seg->seg_type) {
            seg->seg_nchars += next_seg->seg_nchars;
            seg->seg_len += next_seg->seg_len;
            seg->next = next_seg->next;
	    XtFree((char*)next_seg);
	} else
	    seg = seg->next;
    }
}


/**************************************************************************
 *
 * Function: lc_bounds
 *
 * Description: Performs bounding of a line number and column number
 *	pair based on the current text.
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *	pos (I,O) - text position
 *
 * Return: none
 *
 **************************************************************************/

static void lc_bounds(VwrTextViewPart *tvp, VwrPositionStruct *pos)
{
    register vwr_text_t *tptr = &(tvp->text);

    if (pos->line < 0)
	pos->line = -TEXT_END;
    else if (pos->line >= tptr->nlines)
	pos->line = TEXT_END;

    if (pos->line == -TEXT_END || pos->line == TEXT_END)
	pos->col = LINE_END;
    else if (pos->col < 0)
	pos->col = -LINE_END;
    else if (pos->col == tptr->lines[pos->line].line_nchars)
	pos->col = END_CHAR;
    else if (pos->col > tptr->lines[pos->line].line_nchars)
	pos->col = LINE_END;
}


/**************************************************************************
 *
 * Function: init_selection
 *
 * Description: Initializes the primary selection variables.
 *
 * Parameters: 
 *	tvp (I) - this widget's instance part
 *
 * Return: none
 *
 **************************************************************************/

static void init_selection(VwrTextViewPart *tvp)
{
    tvp->sel_mark.line = -1;
    tvp->sel_mark.col = -1;
    tvp->sel_point.line = -1;
    tvp->sel_point.col = -1;
    tvp->sel_data = NULL;
}


/**************************************************************************
 *
 * Function: parse_escapes
 *
 * Description: Parses the specified string for escape sequences and
 *	replace them with the corresponding escape characters.
 *
 * Parameters: 
 *	src (I) - source string
 *	dst (O) - destination string
 *
 * Return: none
 *
 **************************************************************************/

static void parse_escapes(register char *src, register char *dst)
{
    static char *esc_rep = "abfnrtv";
    static char *esc_ch = "\a\b\f\n\r\t\v";
    register char *lptr, ch;
    char *sptr;

    while (*src) {

	/*
	 * If we have an escape sequence process it
	 */
	if (*src == '\\') {
	    /*
	     * If not a pre-define escape sequence check for
	     * a numerical escape sequence
	     */
	    if ((lptr = strchr(esc_rep, src[1])) == NULL) {
		if (src[1] == 'x') {
		    ch = (char)strtol(&src[2], &sptr, 16);
		    if (sptr == &src[2]) {
			*dst++ = *src++;
			*dst++ = *src++;
		    } else {
			*dst++ = ch;
			src = sptr;
		    }
		} else if (isdigit(src[1])) {
		    *dst++ = (char)strtol(&src[1], &sptr, 8);
		    src = sptr;
		} else {
	    	    *dst++ = *src++;
		}
	    }
	    /*
	     * Copy the proper escape char for the pre-defined sequence
	     */
	    else {
		*dst++ = esc_ch[(int)(lptr - esc_rep)];
		src += 2;
	    }
	}
	/*
	 * Otherwise just copy character to output buffer
	 */
	else
	    *dst++ = *src++;
    }
    *dst = '\0';
}


/*
 ==========================================================================
 		PUBLIC CONVENIENCE AND UTILITY FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: VwrCreateScrolledTextView
 *
 * Description: Convenience function to create a scrolled text viewer
 *
 * Parameters: 
 *	name (I) - text viewer instance name
 *	parent (I) - text viewer's parent widget
 *	vsb_flag (I) - if True, a vertical scrollbar is provided.
 *	hsb_flag (I) - if True, a horizontal scrollbar is provided.
 *
 * Return: The created TextView widget. Applications need to use XtParent
 *	to get the Frame around the viewer and an XtParent on that to
 *	get the Scrolled Window widget that this text viewer is in.
 *
 **************************************************************************/

Widget VwrCreateScrolledTextView(Widget parent, char *name, Boolean vsb_flag,
					Boolean hsb_flag)
{
    DEFARGS(10);
    char *buffer;
    Widget text_viewer, text_swin, text_frame;
    Widget text_vert_scrollbar = NULL, text_horiz_scrollbar = NULL;

    /*
     * Allocate space for the name buffer
     */
    buffer = (char*)XtMalloc((strlen(name) + 20) * sizeof(char));

    /*
     * Create a scrolled window to put the text viewer in
     */
    (void)sprintf(buffer, "%sSW", name);
    text_swin = XtCreateManagedWidget(buffer, xmScrolledWindowWidgetClass,
					parent, NULL, 0);

    /*
     * Create the drawing surface and a frame for it.
     */
    (void)sprintf(buffer, "%sFR", name);
    STARTARGS;
    SETARG(XmNshadowType, XmSHADOW_IN);
    text_frame = XtCreateManagedWidget(buffer, xmFrameWidgetClass,
					text_swin, ARGLIST);

    /*
     * Create the vertical scrollbar
     */
    if (vsb_flag) {
        (void)sprintf(buffer, "%sVSB", name);
        text_vert_scrollbar = XtCreateManagedWidget(buffer,
					xmScrollBarWidgetClass,
					text_swin, NULL, 0);
    }

    /*
     * Create the horizontal scrollbar, if desired
     */
    if (hsb_flag) {
        (void)sprintf(buffer, "%sHSB", name);
	STARTARGS;
	SETARG(XmNorientation, XmHORIZONTAL);
        text_horiz_scrollbar = XtCreateManagedWidget(buffer,
					xmScrollBarWidgetClass,
					text_swin, ARGLIST);
    }

    /*
     * Create the text viewing widget
     */
    STARTARGS;
    SETARG(VwrNverticalScrollBar, text_vert_scrollbar);
    SETARG(VwrNhorizontalScrollBar, text_horiz_scrollbar);
    text_viewer = XtCreateManagedWidget(name, vwrTextViewWidgetClass,
					text_frame, ARGLIST);

    /*
     * Register the scrollbar(s) and work area for the ScrolledWindow widget.
     */
    STARTARGS;
    SETARG(XmNverticalScrollBar, text_vert_scrollbar);
    SETARG(XmNhorizontalScrollBar, text_horiz_scrollbar);
    SETARG(XmNworkWindow, text_frame);
    XtSetValues(text_swin, ARGLIST);

    /*
     * Deallocate name buffer
     */
    XtFree((char*)buffer);

    return text_viewer;
}


/**************************************************************************
 *
 * Function: VwrTextViewClearSelection
 *
 * Description: Clears the Primary Selection highlight in a text viewer.
 *
 * Parameters: 
 *	w (I) - TextView widget
 *
 * Return: none
 *
 **************************************************************************/

void VwrTextViewClearSelection(Widget w)
{
    lose_selection(w, NULL);
}


/**************************************************************************
 *
 * Function: VwrTextViewSetSelection
 *
 * Description: Highlights text and optinally sets the Primary selection
 *	The user may specify whether the primary selection is obtained.
 *	In any case, the original primary selectionis lost. Note that row
 *	and column numbers start at 0.
 *
 * Parameters: 
 *	w (I) - TextView widget
 *	start_pos (I) - starting text position to highlight
 *	end_pos (I) - ending position
 *	primary_select (I) - if 1, indicates that primary selection is to
 *		             be set. If 0, only highlighting is performed.
 *
 * Return: None
 *
 **************************************************************************/

void VwrTextViewSetSelection(Widget w, VwrPositionStruct *start_pos,
				VwrPositionStruct *end_pos,
				Boolean primary_select)
{
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;
    VwrPositionStruct tp;
    int cmp, new_top, page_amount, sel_amount;

    /*
     * Clear the old selection if any
     */
    lose_selection(w, NULL);

    /*
     * Assign the new selection
     */
    tvp->sel_mark = *start_pos;
    tvp->sel_point = *end_pos;

    /*
     * Increment to include last character
     */
    tvp->sel_point.col++;

    /*
     * Do bounds checking on the span
     */
    lc_bounds(tvp, &tvp->sel_mark);
    lc_bounds(tvp, &tvp->sel_point);

    /*
     * Make sure point is greater than mark
     */
    if ((cmp = selcmp(&tvp->sel_mark, &tvp->sel_point)) > 0) {
	tp = tvp->sel_mark;
	tvp->sel_mark = tvp->sel_point;
	tvp->sel_point = tp;
    }

    /*
     * Highlight the text and scroll to see it
     */
    if (cmp) {
	highlight(tv, &tvp->sel_mark, &tvp->sel_point, SELECT);
	if (primary_select) {
	    if (XtOwnSelection(w, XA_PRIMARY,
		    XtLastTimestampProcessed(XtDisplay(w)), transfer_selection,
		    lose_selection, transfer_complete) == False) {
	        XtWarning("TextView - cannot own selection, try again\n");
	        lose_selection(w, NULL);
	    }
	}
        page_amount = tv->core.height / tvp->fontheight;
	sel_amount = tvp->sel_point.line - tvp->sel_mark.line + 1;
	if (sel_amount <= page_amount) {
	    if (tvp->sel_mark.line < tvp->top)
		new_top = tvp->sel_mark.line;
	    else
		new_top = tvp->top;
	    if (tvp->sel_point.line >= new_top + page_amount)
		new_top = tvp->sel_point.line - page_amount + 1;
	} else {
	    new_top = tvp->sel_mark.line;
	}
	if (new_top < 0)
	    new_top = 0;
	else if (new_top + page_amount > tvp->text.nlines)
	    new_top = tvp->text.nlines - page_amount;
	goto_line(tv, new_top);
    }
}


/**************************************************************************
 *
 * Function: VwrTextViewReadSelection
 *
 * Description: Returns the currently highlighted text, if any. The
 *	text need be highlighted but does not need to have been made
 *	the primary selection.
 *
 *	NOTE: This function allocates a buffer for storage of the
 *	text selection. It is the users responsibility to free this
 *	buffer using XtFree when this buffer is no longer needed.
 *
 * Parameters: 
 *	w (I) - this widget
 *	lenp (O) - number of bytes in returned selection buffer.
 *
 * Return: A pointer to the selection buffer or NULL if no text is
 *	highlighted.
 *
 **************************************************************************/

char* VwrTextViewReadSelection(Widget w, unsigned long *lenp)
{
    return get_selection(&((VwrTextViewWidget)w)->textView, lenp);
}


/**************************************************************************
 *
 * Function: VwrTextViewSearch
 *
 * Description: Searches the current text for the specified regular
 *	expression.
 *
 * Parameters: 
 *	w (I) - this widget
 *	search_str (I) - regular expression search string. If this
 *		string is NULL, the previously specified string will
 *		be used. Specifying this as NULL saves recompilation
 *		of the expression string.
 *	from_top (I) - if True, search starts at beggining of text. If
 *		False, search continues where it left off.
 *	start_pos (O) - starting text position of found string
 *	end_pos (O) - ending position
 *
 * Return: 1 is returned if the search string has been found. 0 is
 *	returned if the search string has not been found. If 0 is
 *	returned, the values of the line and column variables is
 *	undefined. If -1 is returned an error has occurred in
 *	compiling the regular expression.
 *
 **************************************************************************/

int VwrTextViewSearch(Widget w, char *search_str, Boolean from_top,
			VwrPositionStruct *start_pos,
			VwrPositionStruct *end_pos)
{
    char *old_start, *buf;
    int start_off, end_off;
    register int char_count, i;
    VwrTextViewWidget tv = (VwrTextViewWidget)w;
    VwrTextViewPart *tvp = &tv->textView;

    /*
     * Compile the regular expression
     */
    if (search_str) {

	/*
	 * Free any previous compiled expression
	 */
	if (tvp->comp_exp)
	    free((char*)tvp->comp_exp);

	/*
	 * Copy the search expression and replace common escape
	 * sequences with their character equivalents
	 */
	buf = (char*)malloc(strlen(search_str) + 1);
	parse_escapes(search_str, buf);

	/*
	 * Compile the new expression
	 */
	if ((tvp->comp_exp = regcmp(buf, NULL)) == NULL)
	    return -1;
	
	/*
	 * Free the buffer
	 */
	free((char*)buf);
    }

    /*
     * Sanity check starting pointer
     */
    if (!tvp->search_next || from_top)
	tvp->search_next = tvp->text.char_buf;

    /*
     * Search for the string
     */

    /* Remember where we started */
    old_start = tvp->search_next;
    if ((tvp->search_next = regex(tvp->comp_exp, tvp->search_next)) == NULL) {

	/* If we couldn't match and we started at the beginning, forget it */
	if (old_start == tvp->text.char_buf) {
	    return 0;
	}

	/* Otherwise start at the beginning and try for a match */
	else {
    	    if ((tvp->search_next = regex(tvp->comp_exp,
						tvp->text.char_buf)) == NULL)
		return 0;
	}
    }

    /*
     * Map the match start and end character pointer addresses
     * to line and column numbers
     */

    /* Convert address to character buffer offsets */
    start_off = (int)(__loc1 - tvp->text.char_buf);
    end_off = (int)(tvp->search_next - 1 - tvp->text.char_buf);

    /* Convert offsets to line numbers */
    char_count = 0;
    for (i = 0; i < tvp->text.nlines; i++) {
	char_count += tvp->text.lines[i].line_nchars + 1;
	if (start_off < char_count) {
	    start_pos->line = i;
	    break;
	}
    }
    for (i = start_pos->line; i < tvp->text.nlines; i++) {
	if (end_off < char_count) {
	    end_pos->line = i;
	    break;
	}
	char_count += tvp->text.lines[i].line_nchars + 1;
    }

    /* Convert line addresses to column numbers */
    start_pos->col = (int)(__loc1 -
				tvp->text.lines[start_pos->line].line_chars);
    end_pos->col = (int)(tvp->search_next - 1 -
				tvp->text.lines[end_pos->line].line_chars);

    return 1;
}

