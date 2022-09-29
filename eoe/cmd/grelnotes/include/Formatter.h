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
 * File: Formatter.h
 *
 * Description: Include file for the text document reading functions
 *
 **************************************************************************/

#ident "$Revision: 1.2 $"


#ifndef _VWR_FORMATTER_H_
#define _VWR_FORMATTER_H_

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>


/* Text format tokens */

#define FMT_UNKNOWN	0
#define FMT_GENERIC	1
#define FMT_NROFF_MAN	2
#define FMT_NROFF_MM	3
#define FMT_ZCAT	4
#define FMT_PCAT	5
#define FMT_GZCAT	6
#define FMT_GZCAT_HTML	7

#define NUM_FMT		8

/* Font style tokens */

#define NUM_STYLES      	8
#define NUM_NORM_STYLES      	4

#define UNKNOWN_STYLE   	0xF

#define NORMAL_STYLE    	0x0
#define UNDERLINE_STYLE 	0x1
#define BOLD_STYLE      	0x2
#define BOLDUNDER_STYLE 	0x3

#define NORMAL_SELECT   	0x4
#define UNDERLINE_SELECT 	0x5
#define BOLD_SELECT      	0x6
#define BOLDUNDER_SELECT 	0x7

#define SELECT_MASK		0x4


/* Text style structure */

typedef struct {
    XFontStruct *font;                  /* Font */
    Pixel fore;                         /* Foreground color */
    GC gc;                              /* Graphics context */
} vwr_text_style_t;

/* Text segment structure */
/*      A contiguous piece of text on a line in the same font
*/

typedef struct _vwr_text_seg_t {
    ushort seg_nchars;                  /* # chars in segment */
    uint seg_len:  12;                  /* Pixel length of segment */
    uint seg_type: 4;			/* Style type index */
    struct _vwr_text_seg_t *next;	/* Pointer to next segment on list */
} vwr_text_seg_t;

/* Text line structure */

typedef struct {
    char *line_chars;			/* Line text string */
    vwr_text_seg_t *segs;		/* List of string segments */
    vwr_text_seg_t *last_seg;		/* Last segment on list */
    ushort line_nchars;			/* # chars (not incl. \n) */
    uint line_len:	15;		/* Total pixel length of line */
    uint end_select:	1;		/* End of line selected flag */
} vwr_text_line_t;

/* Text structure */

typedef struct {
    vwr_text_line_t *lines;             /* Lines of text */
    int nlines;                         /* Number of lines of text */
    uint max_line_len;			/* Pixel length of longest line */
    char *char_buf;			/* Complete processed text buffer */
} vwr_text_t;


/* Public functions */

#ifdef __cplusplus
extern "C" {
#endif
extern int	VwrFormatText(char*, char*, int);
extern void	VwrInitText(vwr_text_t*);
extern int	VwrReadText(char*, vwr_text_t*, vwr_text_style_t*, int);
extern void	VwrFreeText(vwr_text_t*);
#ifdef __cplusplus
}
#endif


#endif
