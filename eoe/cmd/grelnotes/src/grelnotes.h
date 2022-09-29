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
 * File: grelnotes.h
 *
 * Description: Primary include file for the graphical release notes
 *	browser.
 *
 **************************************************************************/


#ident "$Revision: 1.3 $"


#define PROG_VERSION	"3.1"

/* Environment variables */

#define ENV_RELNOTES_PATH	"RELNOTESPATH"

/* No resource specified defaults */

#define DEF_RELNOTES_PATH	"/usr/relnotes/"
#define DEF_TEMP_PATH		"/usr/tmp"
#define DEF_HELP_PROG_MSG	"Program: %s"
#define DEF_HELP_VER_MSG	"Version: %s"

/* Temporary filename info */

#define TEMP_PREFIX		"grel"

/* Cursor types */

#define BUSY_CURSOR		0
#define NORMAL_CURSOR		1

/* Movement button info */

#define PRODUCT_BUTTONS		0
#define CHAPTER_BUTTONS		1
#define MOVE_NEXT		0
#define MOVE_PREV		1

/* Button sensitivity */

#define ENABLED			True
#define DISABLED		False

/* Error conditions */

#define NO_CMDLINE_PRODUCT	0
#define NO_CMDLINE_CHAPTER	1
#define NO_PRODUCTS		2
#define NO_PRINTER		3

/* Help tokens */

#define PROGRAM_HELP		0
#define VERSION_HELP		1

/* Resource macros */

#define DEFARGS(num)    Arg _args[num]; register int _n
#define STARTARGS       _n = 0
#define SETARG(r,v)     XtSetArg(_args[_n], r, v); _n++
#define ARGLIST         _args, _n


/* Application Resources */

/*	Resource names and classes */

#define GrNrelnotesPath		"relnotesPath"
#define GrCRelnotesPath		"RelnotesPath"

#define GrNtempPath		"tempPath"
#define GrCTempPath		"TempPath"

#define GrNhelpProgramMsg	"helpProgramMsg"
#define GrCHelpProgramMsg	"HelpProgramMsg"

#define GrNhelpVersionMsg	"helpVersionMsg"
#define GrCHelpVersionMsg	"HelpVersionMsg"

#define GrNprintSubmitMsg	"printSubmitMsg"
#define GrCPrintSubmitMsg	"PrintSubmitMsg"

#define GrNproductNotFoundMsg	"productNotFoundMsg"
#define GrCProductNotFoundMsg	"ProductNotFoundMsg"

#define GrNchapterNotFoundMsg	"chapterNotFoundMsg"
#define GrCChapterNotFoundMsg	"ChapterNotFoundMsg"

#define GrNnoProductsMsg	"noProductsMsg"
#define GrCNoProductsMsg	"NoProductsMsg"

#define GrNnoPrinterMsg		"noPrinterMsg"
#define GrCNoPrinterMsg		"NoPrinterMsg"

/*	Resource structure */

typedef struct {
    char *relnotes_path;	/* Release notes root directory */
    char *temp_path;		/* Temporary file directory */
    char *help_prog_msg;	/* Help program name message */
    char *help_ver_msg;		/* Help program version message */
    char *print_submit_msg;	/* Print job submittal message */
    char *product_notfound_msg;	/* Product not found message */
    char *chapter_notfound_msg;	/* Chapter not found message */
    XmString no_products_msg;	/* No release notes isntalled message */
    XmString no_printer_msg;	/* No printer selected message */
} app_data_t;


/* Product and Chapter structures */

typedef struct {
    char *title;			/* Chapter title from TC */
    char *fname;			/* Chapter filename */
} CHAPTER;

typedef struct {
    char *title;			/* Product title */
    unchar nchapters;			/* Number of chapters in product */
    CHAPTER *chapters;			/* List of chapters */
} PRODUCT;

typedef struct {
    ushort nproducts;			/* Number of products */
    ushort nprod_alloc;			/* Product space allocation */
    PRODUCT *products;			/* List of products */
} PRODUCT_LIST;


/* Public variables */

extern Widget toplevel;
extern Widget text_viewer;
extern PRODUCT *current_product;
extern CHAPTER *current_chapter;
extern char real_relnotes_path[];
extern char *temp_fname;
extern char *prog_name, *prog_classname, *prog_version;
extern app_data_t app_data;


/* Public functions */

extern Widget		create_appmenu(Widget);
extern Widget		create_place(Widget);
extern Widget		create_buttons(Widget);
extern PRODUCT_LIST	*find_relnotes(void);
extern void		scan(void);
extern int		build_prodmenu(PRODUCT_LIST*);
extern int		build_chapmenu(PRODUCT*);
extern void 		display_product(Widget, XtPointer, XtPointer);
extern void 		display_chapter(Widget, XtPointer, XtPointer);
extern void		set_product_name(char*);
extern void		set_chapter_name(char*);
extern void		quit(Widget, XtPointer, XtPointer);
extern void		set_cursor(Widget, int);
extern void		set_button_state(int, int);
extern void		next_product(Widget, XtPointer, XtPointer);
extern void		next_chapter(Widget, XtPointer, XtPointer);
extern void		display_error(int, char*, int);
extern void		display_help(Widget, XtPointer, XtPointer);
extern void		print_handler(Widget, XtPointer, XtPointer);
extern void		search_handler(Widget, XtPointer, XtPointer);
extern void		set_print_search_state(int);
extern XmString		create_message(char**);
