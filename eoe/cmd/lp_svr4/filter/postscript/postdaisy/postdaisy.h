/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/postdaisy/RCS/postdaisy.h,v 1.1 1992/12/14 13:17:35 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 *
 * Definitions used by the PostScript translator for Diablo 1640 files.
 *
 * Diablo printers have horizontal and vertical resolutions of 120 and 48 dpi.
 * We'll use a single resolution of 240 dpi and let the program scale horizontal
 * and vertical positions by HSCALE and VSCALE.
 *
 */

#define RES		240
#define HSCALE		2
#define VSCALE		5

/*
 *
 * HMI is the default character spacing and VMI is the line spacing. Both values
 * are in terms of the 240 dpi resolution.
 *
 */

#define HMI		(12 * HSCALE)
#define VMI		(8 * VSCALE)

/*
 *
 * Paper dimensions don't seem to be all that important. They're just used to
 * set the right and bottom margins. Both are given in terms of the 240 dpi
 * resolution.
 *
 */

#define LEFTMARGIN	0
#define RIGHTMARGIN	3168
#define TOPMARGIN	0
#define BOTTOMMARGIN	2640

/*
 *
 * ROWS and COLUMNS set the dimensions of the horizontal and vertical tab arrays.
 * The way I've implemented both kinds of tabs leaves something to be desired, but
 * it was simple and should be good enough for now. If arrays are going to be used
 * to mark tab stops I probably should use malloc() to get enough space once the
 * initial hmi and vmi are know.
 *
 */

#define ROWS		400
#define COLUMNS		200

/*
 *
 * An array of type Fontmap helps convert font names requested by users into
 * legitimate PostScript names. The array is initialized using FONTMAP, which must
 * end with an entry that has NULL defined as its name field.
 *
 */

typedef struct {

	char	*name;			/* user's font name */
	char	*val;			/* corresponding PostScript name */

} Fontmap;

#define FONTMAP								\
									\
	{								\
	    "R", "Courier",						\
	    "I", "Courier-Oblique",					\
	    "B", "Courier-Bold",					\
	    "CO", "Courier",						\
	    "CI", "Courier-Oblique",					\
	    "CB", "Courier-Bold",					\
	    "CW", "Courier",						\
	    "PO", "Courier",						\
	    "courier", "Courier",					\
	    "cour", "Courier",						\
	    "co", "Courier",						\
	    NULL, NULL							\
	}

/*
 *
 * Some of the non-integer functions in postdaisy.c.
 *
 */

char	*get_font();

