/**************************************************************************
 *									  *
 * 	      Copyright (c) 1991 Silicon Graphics, Inc.			  *
 *			All Rights Reserved		  		  *
 *									  *
 *         THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI             *
 *									  *
 * The copyright notice above does not evidence any actual of intended    *
 * publication of such source code, and is an unpublished work by Silicon *
 * Graphics, Inc. This material contains CONFIDENTIAL INFORMATION that is *
 * the property of Silicon Graphics, Inc. Any use, duplication or         *
 * disclosure not specifically authorized by Silicon Graphics is strictly *
 * prohibited.								  *
 *									  *
 * RESTRICTED RIGHTS LEGEND:						  *
 *									  *
 * Use, duplication or disclosure by the Government is subject to         *
 * restrictions as set forth in subdivision (c)(1)(ii) of the Rights in   *
 * Technical Data and Computer Software clause at DFARS 52.227-7013,	  *
 * and/or in similar or successor clauses in the FAR, DOD or NASA FAR     *
 * Supplement. Unpublished - rights reserved under the Copyright Laws of  *
 * the United States. Contractor is SILICON GRAPHICS, INC., 2011 N.       *
 * Shoreline Blvd., Mountain View, CA 94039-7311                          *
 **************************************************************************
 *
 * File: TextViewP.h
 *
 * Description: Private include file for the multi-font text viewer widget.
 *
 **************************************************************************/


#ident "$Revision: 1.1 $"


#ifndef _VWR_TEXTVIEWP_H_
#define _VWR_TEXTVIEWP_H_


#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>
#include "TextView.h"


/* No resource specified defaults */

#define DEF_COLOR		0xFFFFFFFF

#define DEF_TEXT_COLS		80
#define DEF_TEXT_ROWS		24

#define DEF_NORM_FONT		"Fixed"
#define DEF_UNDERLINE_FONT	"Fixed"
#define DEF_BOLD_FONT		"Fixed"
#define DEF_BOLDUNDER_FONT	"Fixed"

#define DEF_NORM_FORE		DEF_COLOR
#define DEF_UNDERLINE_FORE	DEF_COLOR
#define DEF_BOLD_FORE		DEF_COLOR
#define DEF_BOLDUNDER_FORE	DEF_COLOR

#define DEF_SELECT_FORE 	DEF_COLOR
#define DEF_SELECT_BACK 	DEF_COLOR

#define DEF_BLANK_COMPRESS	0

/* Misc macros */

#define TV_FWIDTH(f)	(((f)->per_char) ? (\
				((f)->per_char['n'].width > 0) ? \
					(f)->per_char['n'].width: \
					(f)->max_bounds.width \
				): (f)->max_bounds.width)
#define TV_MAX(a,b)	(((a) > (b)) ? (a) : (b))


/* Class part and record structures */

typedef struct {
    int dummy;		/* To keep compiler happy */
} VwrTextViewClassPart;

typedef struct _VwrTextViewClassRec {
    CoreClassPart core_class;
    XmPrimitiveClassPart primitive_class;
    VwrTextViewClassPart vwrTextView_class;
} VwrTextViewClassRec;


/* Instance part and record structures */

typedef struct {
    /* Resources */
    int text_columns;			/* Number of columns for text view */
    int text_rows;			/* Number of rows for text view */
    vwr_text_style_t styles[NUM_STYLES];/* Text styles (font, fore, etc.) */
    Pixel select_fore;			/* Selection foreground color */
    Pixel select_back;			/* Selection background color */
    int blank_compress;			/* Num lines for blank compression */
    char *filename;			/* Text file to view */
    Widget 	vsb;			/* Viewer's vert. scrollbar, if any */
    Widget 	hsb;			/* Viewer's horiz. scrollbar, if any */
    XtCallbackList load_callback;	/* Viewer load callback functions */

    /* Private state */
    vwr_text_t 	text;			/* Viewer text */
    GC          fill_gc;		/* GC for line filling */
    GC          select_fill_gc;		/* GC for selected line filling */
    GC		blanks[NUM_NORM_STYLES];	/* Line clearing GCs */
    int		def_height, def_width;	/* Default height and width */
    int         fontheight;		/* Max font height, descent + ascent */
    int         fontwidth;		/* Max font width */
    int         fontascent;		/* Max font ascent */
    int         top;			/* Line at top of window */
    int		left;			/* Column at left of window */
    VwrPositionStruct	sel_mark;	/* Select mark */
    VwrPositionStruct	sel_point;	/* Select point */
    XtIntervalId	timer;		/* Scrolling timer */
    int		top_pending;		/* Scroll drag value */
    char 	*sel_data;		/* Selection data buffer */
    char	*search_next;		/* Start addr for next text search */
    char 	*comp_exp;		/* Compiled search regular expr */
} VwrTextViewPart;

typedef struct _VwrTextViewRec {
    CorePart core;
    XmPrimitivePart primitive;
    VwrTextViewPart textView;
} VwrTextViewRec;

#endif
