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
 * File: main.c
 *
 * Description: Main file for the graphical release notes browser.
 *
 **************************************************************************/


#ident "$Revision: 1.5 $"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <locale.h>
#include <sys/param.h>
#include <Xm/Xm.h>
#include <X11/StringDefs.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Protocols.h>
#include <Xm/AtomMgr.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>

#include "grelnotes.h"
#include "TextView.h"
#include "Formatter.h"


/* Program globals */

Widget toplevel;			/* Top level window */
Widget text_viewer;			/* Text viewer widget */
PRODUCT *current_product;		/* Currently selected product */
CHAPTER *current_chapter;		/* Currently selected chapter */
char *temp_fname;			/* A unique temporary filename */
char real_relnotes_path[MAXPATHLEN];	/* Release notes root directory */
char *prog_name;			/* The program name (path stripped) */
char *prog_classname = "Grelnotes";	/* The program class name */
char *prog_version = PROG_VERSION;	/* The program version */
app_data_t app_data;			/* Application resources */

/* File globals */

static Cursor busy_cursor;		/* A busy cursor */
static PRODUCT_LIST *plist;		/* Head of product list struct */
static char *cmdline_product;		/* Product specified on command line */
static char *cmdline_chapter;		/* Chapter specified on command line */


/* Application resouce list */

static XtResource resources[] = {
	/* Public Resources */

	/* Release notes root directory */
	{
	GrNrelnotesPath, GrCRelnotesPath, XtRString, sizeof(char*),
	XtOffsetOf(app_data_t, relnotes_path), XtRImmediate,
						(XtPointer)DEF_RELNOTES_PATH
	},
	/* Temporary file directory */
	{
	GrNtempPath, GrCTempPath, XtRString, sizeof(char*),
	XtOffsetOf(app_data_t, temp_path), XtRImmediate,
						(XtPointer)DEF_TEMP_PATH
	},

	/* Private Resources */

	/* Help on program name message */
	{
	GrNhelpProgramMsg, GrCHelpProgramMsg, XtRString, sizeof(char*),
	XtOffsetOf(app_data_t, help_prog_msg), XtRImmediate,
						(XtPointer)DEF_HELP_PROG_MSG
	},
	/* Help on program version message */
	{
	GrNhelpVersionMsg, GrCHelpVersionMsg, XtRString, sizeof(char*),
	XtOffsetOf(app_data_t, help_ver_msg), XtRImmediate,
						(XtPointer)DEF_HELP_VER_MSG
	},
	/* Print job submittal message */
	{
	GrNprintSubmitMsg, GrCPrintSubmitMsg, XtRString, sizeof(char*),
	XtOffsetOf(app_data_t, print_submit_msg), XtRImmediate, (XtPointer)""
	},
	/* Product not found message */
	{
	GrNproductNotFoundMsg, GrCProductNotFoundMsg, XtRString, sizeof(char*),
	XtOffsetOf(app_data_t, product_notfound_msg), XtRImmediate,
							(XtPointer)""
	},
	/* Chapter not found message */
	{
	GrNchapterNotFoundMsg, GrCChapterNotFoundMsg, XtRString, sizeof(char*),
	XtOffsetOf(app_data_t, chapter_notfound_msg), XtRImmediate,
							(XtPointer)""
	},
	/* No products message */
	{
	GrNnoProductsMsg, GrCNoProductsMsg, XmRXmString,
	sizeof(XmString),
	XtOffsetOf(app_data_t, no_products_msg), XtRString, (XtPointer)""
	},
	/* No pritner message */
	{
	GrNnoPrinterMsg, GrCNoPrinterMsg, XmRXmString,
	sizeof(XmString),
	XtOffsetOf(app_data_t, no_printer_msg), XtRString, (XtPointer)""
	},
};


/* Application specific command-line options */

static XrmOptionDescRec app_options[] = {
	{ "-rpath", "*relnotesPath", XrmoptionSepArg, NULL},
};


/* Local functions */

static int parse_chapter(char*, char*);
static PRODUCT *find_product(char*);
static CHAPTER *find_chapter(PRODUCT*, char*);


/**************************************************************************
 *
 * Function: main
 *
 * Description: Program entry point
 *
 * Parameters:
 *	argc (I) - command line argument count
 *	argv (I) - command line arguments
 *
 * Return: None
 *
 **************************************************************************/

int main(int argc, char *argv[])
{
    XtAppContext app_context;
    Widget top_form, appmenu, place_frame;
    Widget buttons;
    Atom destroy_atom;
    char *rpath;
    DEFARGS(15);

    /*
     * i18n support
     */
    setlocale(LC_ALL, "");
    XtSetLanguageProc(NULL, NULL, NULL);

    /*
     * Initialize the app and retrieve app specific resources
     */
    toplevel = XtAppInitialize(&app_context, prog_classname,
				 app_options, XtNumber(app_options),
				 &argc, argv, NULL, NULL, 0);
    XtGetApplicationResources(toplevel, (XtPointer)&app_data, resources,
			      XtNumber(resources), NULL, 0);

    /*
     * Set program name
     */
    prog_name = strrchr(argv[0], '/');
    prog_name = (prog_name == NULL) ? argv[0]: prog_name + 1;

    /*
     * Set release notes root directory. If the environment
     * variable defined by ENV_RELNOTES_PATH is set it will
     * take precedence over the value of the resource. Note
     * that we tack on a '/' to the end of the path if the
     * user has not specified one.
     */
    if ((rpath = getenv(ENV_RELNOTES_PATH)) == NULL)
        rpath = app_data.relnotes_path;
    if (rpath == NULL || *rpath == '\0')
	rpath = " ";
    realpath((rpath)? rpath: "", real_relnotes_path);
    if (real_relnotes_path[strlen(real_relnotes_path) - 1] != '/')
	strcat(real_relnotes_path, "/");

    /*
     * Check for product and possibly chapter specified on command line
     */
    if (argc >= 3) 
	cmdline_chapter = argv[2];
    if (argc >= 2)
	cmdline_product = argv[1];

    /*
     * Indicate an interest in WM app destruction if available
     */
    if ((destroy_atom = XmInternAtom(XtDisplay(toplevel),
					"WM_DELETE_WINDOW", True)) != None) {
        XmAddWMProtocols(toplevel, &destroy_atom, 1);
        XmAddWMProtocolCallback(toplevel, destroy_atom, quit, NULL);
    }

    /*
     * Create Form Widget to hold everything
     */
    top_form = XtCreateManagedWidget("topForm", xmFormWidgetClass,
				     toplevel, NULL, 0);

    /*
     * Create menubar and menu structure
     */
    appmenu = create_appmenu(top_form);
    STARTARGS;
    SETARG(XmNtopAttachment, XmATTACH_FORM);
    SETARG(XmNleftAttachment, XmATTACH_FORM);
    SETARG(XmNrightAttachment, XmATTACH_FORM);
    XtSetValues(appmenu, ARGLIST);

    /*
     * Create the place indication frame and labels
     */
    STARTARGS;
    SETARG(XmNtopAttachment, XmATTACH_WIDGET);
    SETARG(XmNtopWidget, appmenu);
    SETARG(XmNtopOffset, 10);
    SETARG(XmNleftAttachment, XmATTACH_FORM);
    SETARG(XmNleftOffset, 10);
    SETARG(XmNrightAttachment, XmATTACH_FORM);
    SETARG(XmNrightOffset, 10);
    place_frame = XtCreateManagedWidget("placeFrame", xmFrameWidgetClass,
					top_form, ARGLIST);
    (void)create_place(place_frame);

    /*
     * Create a scrolled text viewer
     */
    text_viewer = VwrCreateScrolledTextView(top_form, "textView", True, True);
    STARTARGS;
    SETARG(XmNtopAttachment, XmATTACH_WIDGET);
    SETARG(XmNtopWidget, place_frame);
    SETARG(XmNtopOffset, 10);
    SETARG(XmNbottomAttachment, XmATTACH_FORM);
    SETARG(XmNbottomOffset, 55);
    SETARG(XmNleftAttachment, XmATTACH_FORM);
    SETARG(XmNleftOffset, 10);
    SETARG(XmNrightAttachment, XmATTACH_FORM);
    SETARG(XmNrightOffset, 10);
    XtSetValues(XtParent(XtParent(text_viewer)), ARGLIST);

    /*
     * Create browsing control buttons
     */
    buttons = create_buttons(top_form);
    STARTARGS;
    SETARG(XmNtopAttachment, XmATTACH_OPPOSITE_FORM);
    SETARG(XmNtopOffset, -50);
    SETARG(XmNleftAttachment, XmATTACH_FORM);
    SETARG(XmNleftOffset, 10);
    SETARG(XmNrightAttachment, XmATTACH_FORM);
    SETARG(XmNrightOffset, 10);
    XtSetValues(buttons, ARGLIST);

    /*
     * Realize all widgets
     */
    XtRealizeWidget(toplevel);

    /*
     * Create a unique temporary filename for the relnotes chapters
     * to pcat and col into
     */
    temp_fname = tempnam(app_data.temp_path, TEMP_PREFIX);

    /*
     * Create a busy cursor glyph for later use
     */
    busy_cursor = XCreateFontCursor(XtDisplay(toplevel), XC_watch);

    /*
     * Catch the killing signals so that proper cleanup can be done
     */
    (void)signal(SIGINT, quit);
    (void)signal(SIGTERM, quit);
    (void)signal(SIGHUP, quit);

    /*
     * Scan for products
     */
    scan();

    /*
     * Off we go
     */
    XtAppMainLoop(app_context);

    /* NOTREACHED */
    return(1);
}


/**************************************************************************
 *
 * Function: scan
 *
 * Description: Supervises the scanning of products with release notes,
 *	the building of the product menu and the enabling or disabling
 *	of the product movement buttons. The first chapter of the first
 *	product in the list, if any, is displayed.
 *
 * Parameters: none
 *
 * Return: none
 *
 **************************************************************************/

void scan()
{
    PRODUCT *product;
    CHAPTER *chapter;

    /*
     * Look for release notes and if there are any, build the Product
     * menu.
     */
    set_cursor(toplevel, BUSY_CURSOR);		/* Set busy cursor */
    plist = find_relnotes();			/* Search for relnotes */
    if (!build_prodmenu(plist)) {		/* Build the product menu */
	set_button_state(PRODUCT_BUTTONS, DISABLED);	/* No products */
        set_print_search_state(DISABLED);
    }
    set_cursor(toplevel, NORMAL_CURSOR);	/* Clear busy cursor */

    /*
     * If there are product with release notes installed, check if
     * a particular product was specified on the command line and if
     * a particular chapter of that product was specified, otherwise
     * look for the 'IRIX' or 'eoe' release notes and failing
     * that display first product on the product list
     */
    if (plist->nproducts) {
	chapter = NULL;		/* Assume no chapter or product match */
	product = NULL;

	/*
	 * If specified product on command line, look for it in list
	 * and if specified a chapter with it look for that too
	 */
	if (cmdline_product) {
	    product = find_product(cmdline_product);
	    if (product && cmdline_chapter)
		chapter = find_chapter(product, cmdline_chapter);

	    if (!product)
		display_error(NO_CMDLINE_PRODUCT, cmdline_product, TRUE);
	    else if (chapter == NULL && cmdline_chapter)
		display_error(NO_CMDLINE_CHAPTER, cmdline_chapter, TRUE);

	    cmdline_product = NULL;
	    cmdline_chapter = NULL;
	}

	/*
	 * If no product specified on command line or if that product
	 * was not found, look for 'IRIX' or 'eoe' products and if
	 * that fails use first product on list.
	 */
	if (!product) {
	    if ((product = find_product("IRIX")) == NULL)
	        if ((product = find_product("eoe")) == NULL)
	            product = plist->products;
	}

	/*
	 * Finally, display the product and chapter if different
	 * from the first chapter
	 */
        display_product(NULL, (XtPointer)product, NULL);
	if (chapter != NULL && chapter != product->chapters)
            display_chapter(NULL, (XtPointer)chapter, NULL);
    }

    /*
     * If there are no products with relnotes tell the world
     */
    else {
	display_error(NO_PRODUCTS, NULL, TRUE);
    }
}


/**************************************************************************
 *
 * Function: display_product
 *
 * Description: Supervises the selection and display of a product's
 *	release notes.
 *
 * Parameters: 
 *	w (I) - invoking widget
 *	client_data (I) - pointer to the selected product structure
 *	call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

void display_product(Widget w, XtPointer client_data, XtPointer call_data)
{
    PRODUCT *product = (PRODUCT*)client_data;

    set_product_name(product->title);		/* Display product title */

    current_product = product;			/* Set current product */

    if (plist->nproducts > 1)			/* If more than 1 product */
        set_button_state(PRODUCT_BUTTONS, ENABLED);	/* ungray buttons */

    set_print_search_state(ENABLED);		/* Enable Print item */

    if (build_chapmenu(product) > 1)		/* Build chapter menu */
	set_button_state(CHAPTER_BUTTONS, ENABLED);
    else
	set_button_state(CHAPTER_BUTTONS, DISABLED);

    if (product->nchapters)			/* Display first chapter */
        display_chapter(NULL, (XtPointer)product->chapters, NULL);
}


/**************************************************************************
 *
 * Function: display_chapter
 *
 * Description: Supervises the selection and display of a chapter of a
 *	product's release notes.
 *
 * Parameters: 
 *	w (I) - invoking widget
 *	client_data (I) - pointer to the selected chapter structure
 *	call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

void display_chapter(Widget w, XtPointer client_data, XtPointer call_data)
{
    CHAPTER *chapter = (CHAPTER*)client_data;
    DEFARGS(5);

    set_cursor(toplevel, BUSY_CURSOR);

    set_chapter_name(chapter->title);	/* Display chapter name */

    current_chapter = chapter;		/* Set current chapter */

    /*
     * Issue the a command to the shell to uncompress the relnotes
     * pass them through col and output the result into the temporary
     * file
     */
    if (VwrFormatText(chapter->fname, temp_fname, FMT_PCAT) < 0) {
	perror(prog_name);
	exit(1);
    }
    STARTARGS;
    SETARG(VwrNtextFile, temp_fname);
    XtSetValues(text_viewer, ARGLIST);

    set_cursor(toplevel, NORMAL_CURSOR);
}


/**************************************************************************
 *
 * Function: next_product
 *
 * Description: Moves to the next product in the list either up or down
 *	as specified.
 *
 * Parameters: 
 *	w (I) - invoking widget
 *	client_data (I) - movement direction (MOVE_NEXT, MOVE_PREV)
 *	call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

void next_product(Widget w, XtPointer client_data, XtPointer call_data)
{
    PRODUCT *product;
    int nproducts;

    /*
     * Make the button's tab group current only if the button
     * was pressed using the keyboard. Otherwise, send focus
     * back to the top level group.
     */
    if (((XmPushButtonCallbackStruct*)call_data)->event->type != KeyPress)
        XmProcessTraversal(w, XmTRAVERSE_PREV_TAB_GROUP);

    product = plist->products;
    nproducts = plist->nproducts;
    switch ((int)client_data) {
	case MOVE_NEXT:
	    if (current_product == &product[nproducts-1])
		display_product(NULL, (XtPointer)product, NULL);
	    else
	        display_product(NULL, (XtPointer)(current_product + 1), NULL);
	    break;
	case MOVE_PREV:
	    if (current_product == product)
		display_product(NULL, (XtPointer)&product[nproducts - 1],
			        NULL);
	    else
	        display_product(NULL, (XtPointer)(current_product - 1), NULL);
	    break;
    }
}


/**************************************************************************
 *
 * Function: next_chapter
 *
 * Description: Moves to the next chapter in the list either up or down
 *	as specified.
 *
 * Parameters: 
 *	w (I) - invoking widget
 *	client_data (I) - movement direction (MOVE_NEXT, MOVE_PREV)
 *	call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

void next_chapter(Widget w, XtPointer client_data, XtPointer call_data)
{
    CHAPTER *chapter;
    int nchapters;

    /*
     * Make the button's tab group current only if the button
     * was pressed using the keyboard. Otherwise, send focus
     * back to the top level group.
     */
    if (((XmPushButtonCallbackStruct*)call_data)->event->type != KeyPress)
        XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);

    chapter = current_product->chapters;
    nchapters = current_product->nchapters;
    switch ((int)client_data) {
	case MOVE_NEXT:
	    if (current_chapter == &chapter[nchapters-1])
		display_chapter(NULL, (XtPointer)chapter, NULL);
	    else
	        display_chapter(NULL, (XtPointer)(current_chapter + 1), NULL);
	    break;
	case MOVE_PREV:
	    if (current_chapter == chapter)
		display_chapter(NULL, (XtPointer)&chapter[nchapters - 1], NULL);
	    else
	        display_chapter(NULL, (XtPointer)(current_chapter - 1), NULL);
	    break;
    }
}


/**************************************************************************
 *
 * Function: quit
 *
 * Description: Program cleanup and exit routine. This routine is called
 *	by the Quit item on the File menu and also from the window manager
 *	if it supports the WM_DELETE_WINDOW protocol.
 *
 * Parameters: 
 *	w (I) - invoking widget
 *	client_data (I) - client data
 *	call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

void quit(Widget w, XtPointer client_data, XtPointer call_data)
{
    /*
     * Delete the temporary file if it exists
     */
    if (access(temp_fname, F_OK) == 0)
        (void)unlink(temp_fname);

    exit(0);
}


/**************************************************************************
 *
 * Function: set_cursor
 *
 * Description: Sets the cursor to either busy (BUSY_CURSOR) or normal
 *	(NORMAL_CURSOR).
 *
 * Parameters: 
 *	w (I) - widget for which cursor is to be changed
 *	type (I) - one of BUSY_CURSOR or NORMAL_CURSOR
 *
 * Return: none
 *
 **************************************************************************/

void set_cursor(Widget w, int type)
{
    switch (type) {
	case BUSY_CURSOR:
    	    XDefineCursor(XtDisplay(w), XtWindow(w), busy_cursor);
	    break;
	case NORMAL_CURSOR:
    	    XUndefineCursor(XtDisplay(w), XtWindow(w));
	    break;
    }
    XFlush(XtDisplay(w));
}


/**************************************************************************
 *
 * Function: create_message
 *
 * Description: A utility function to create a compound string message from
 *	a multi-element array of strings. The array must be NULL terminated.
 *
 * Parameters:
 *      str_array (I) - NULL terminated array of message strings.
 *
 * Return: A compound string.
 *
 **************************************************************************/

XmString create_message(char **str_array)
{
    XmString xm_newstr, xtemp, xmstr = NULL;
    char *str;
    int i;

    for (i = 0, str = *str_array; *str; str = *++str_array, i++) {
	if (i) {
	    xm_newstr = XmStringSeparatorCreate();
	    xtemp = xmstr;
	    xmstr = XmStringConcat(xtemp, xm_newstr);
	    XmStringFree(xtemp);
	    XmStringFree(xm_newstr);
	}
	xm_newstr = XmStringCreateSimple(str);
	xtemp = xmstr;
	xmstr = XmStringConcat(xtemp, xm_newstr);
	XmStringFree(xtemp);
	XmStringFree(xm_newstr);
    }

    return xmstr;
}


/*
 ==========================================================================
			 LOCAL FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: parse_chapter
 *
 * Description: Parses the chapter number from the chapter title and
 *	compares it to the specified chapter number.
 *
 * Parameters: 
 *	title (I) - chapter title
 *	str (I) - string for comparison
 *
 * Return: TRUE if the chapter numbers match. FALSE if the do not match.
 *
 **************************************************************************/

static int parse_chapter(char *title, char *str)
{
    char *cptr, buffer[256];

    (void)strcpy(buffer, title);
    if ((cptr = strtok(buffer, ".")) == NULL)
	return FALSE;
    if (!strcasecmp(cptr, str))
	return TRUE;
    return FALSE;
}


/**************************************************************************
 *
 * Function: find_product, find_chapter
 *
 * Description: Linearly searches the product/chapter list for the speicified
 *	product/chapter.
 *
 * Parameters: 
 *	product_name (I) - name of product to search for
 *	p (I) - product whose chapters are to be searched
 *	chapter_name (I) - name of chapter to search for
 *
 * Return: Pointer to found product or NULL if not found
 *
 **************************************************************************/

static PRODUCT *find_product(char *product_name)
{
    PRODUCT *product, *p;
    int i;

    product = NULL;
    for (i = 0, p = plist->products; i < plist->nproducts; i++, p++) {
	if (!strcasecmp(p->title, product_name)) {
	    product = p;
	    break;
	}
    }

    return product;
}

static CHAPTER *find_chapter(PRODUCT *p, char *chapter_name)
{
    CHAPTER *chapter, *c;
    int i;

    chapter = NULL;
    for (i = 0, c = p->chapters; i < p->nchapters; i++, c++) {
        if (parse_chapter(c->title, chapter_name)) {
	    chapter = c;
	    break;
	}
    }

    return chapter;
}

