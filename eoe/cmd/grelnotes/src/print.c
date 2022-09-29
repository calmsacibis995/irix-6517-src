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
 * File: print.c
 *
 * Description: Printing support for the release notes browser. Provides
 *	printing of single chapter or all chapters in a product.
 *
 **************************************************************************/


#ident "$Revision: 1.6 $"


#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <limits.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/ToggleB.h>
#include <Xm/SelectioB.h>
#include <Xm/List.h>
#include <Xm/Text.h>

#include "grelnotes.h"


/* Print selections */

#define PRINT_CHAPTER	0
#define PRINT_PRODUCT	1

/* Printer listing command */

#define PRINTER_LIST_CMD	"LANG=C lpstat -d -a | LANG=C awk '/accepting/ {if ($2 != \"not\") print $1}; /^system default destination:/ { print $4 }; /^no system default destination/ { print \"_none_\"} '"

/* Misc info */

#define MAX_PRINTERS	256


static Widget create_print_dialog(void);
static void set_scope(Widget, XtPointer, XtPointer);
static void printit(Widget, XtPointer, XtPointer);
static void dismiss(Widget, XtPointer, XtPointer);
static void get_printer_list(void);
static int find_printer(char*);
static void printer_select_cb(Widget, XtPointer, XtPointer);


static int print_mode = PRINT_CHAPTER;
static char *printer_list[MAX_PRINTERS];
static char *default_printer;
static char *selected_printer;
static int num_printers;
static Widget plist_widget;


/**************************************************************************
 *
 * Function: print_handler
 *
 * Description: Brings up the print handler dialog
 *
 * Parameters:
 *      w (I) - invoking widget
 *      client_data (I) - client data
 *      call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

void print_handler(Widget w, XtPointer client_data, XtPointer call_data)
{
    static Widget print_dialog = NULL;

    /*
     * Create the dialog if first time
     */
    if (!print_dialog)
	print_dialog = create_print_dialog();

    /*
     * Clear the info text
     */
    XmTextSetString(XmSelectionBoxGetChild(print_dialog, XmDIALOG_TEXT), " ");

    /*
     * Put up printer list and selected printer
     */
    get_printer_list();

    /*
     * Popup the dialog
     */
    XtManageChild(print_dialog);
}


/*
 ==========================================================================
			   LOCAL FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: create_print_dialog
 *
 * Description: Creates the print dialog
 *
 * Parameters: none
 *
 * Return: Created dialog widget
 *
 **************************************************************************/

static Widget create_print_dialog()
{
    Widget dialog, scope_radio;
    Widget rowc;
    DEFARGS(20);

    /*
     * Create a prompt dialog
     */
    STARTARGS;
    SETARG(XmNautoUnmanage, False);
    dialog = XmCreatePromptDialog(toplevel, "printDialog", ARGLIST);
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_SEPARATOR));

    /*
     * Make the info text field read-only
     */
    STARTARGS;
    SETARG(XmNeditable, False);
    XtSetValues(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT), ARGLIST);

    /*
     * Create a row column to hold the additional dialog items
     */
    rowc = XtCreateManagedWidget("printDialogRc", xmRowColumnWidgetClass,
		    		   dialog, NULL, 0);

    /*
     * Create the scope label
     */
    XtCreateManagedWidget("printScopeLabel", xmLabelWidgetClass, rowc,
							NULL, 0);

    /*
     * Create the print scope toggle buttons
     */
    scope_radio = XmVaCreateSimpleRadioBox(rowc, "printScopeRadio",
				0, set_scope,
				XmNspacing, 0,
				XmVaRADIOBUTTON, NULL, 0, NULL, NULL,
				XmVaRADIOBUTTON, NULL, 0, NULL, NULL,
				NULL);
    XtManageChild(scope_radio);

    /*
     * Printer selection list and label
     */
    XtCreateManagedWidget("printPrinterListLabel", xmLabelWidgetClass,
				rowc, NULL, 0);
    plist_widget = XmCreateScrolledList(rowc, "printPrinterList", NULL, 0);
    XtManageChild(plist_widget);

    /*
     * Register the dialog action button callbacks
     */
    XtAddCallback(dialog, XmNokCallback, printit, (XtPointer)dialog);
    XtAddCallback(dialog, XmNcancelCallback, dismiss, (XtPointer)dialog);
    XtAddCallback(dialog, XmNhelpCallback, display_help,
						(XtPointer)PROGRAM_HELP);

    /*
     * Register the printer selection callback
     */
    XtAddCallback(plist_widget, XmNbrowseSelectionCallback,
						printer_select_cb, NULL);

    return dialog;
}


/**************************************************************************
 *
 * Function: set_scope
 *
 * Description: Sets the print scope (all chapters or just one chapter).
 *
 * Parameters: 
 *      w (I) - invoking widget
 *      client_data (I) - client data
 *      call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void set_scope(Widget w, XtPointer client_data, XtPointer call_data)
{
    if (((XmToggleButtonCallbackStruct*)call_data)->set == False)
	return;
    print_mode = ((int)client_data) ? PRINT_PRODUCT: PRINT_CHAPTER;
}


/**************************************************************************
 *
 * Function: printit
 *
 * Description: Handles the OK to print dialog button and prints the
 * 	selected release notes chapter(s). pcat is piped to lp for
 *	the selected printer.
 *
 * Parameters:
 *      w (I) - invoking widget
 *      client_data (I) - dialog widget
 *      call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void printit(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget dialog = (Widget)client_data;
    char buffer[BUFSIZ];
    int i;
    short bGzMode;
    FILE *fptr;

    /*
     * Set cursor to busy
     */
    set_cursor(toplevel, BUSY_CURSOR);
    set_cursor(dialog, BUSY_CURSOR);

    /*
     * Get the currently selected printer and remember
     * it for future use.
     */
    if (!selected_printer) {
	set_cursor(toplevel, NORMAL_CURSOR);
	set_cursor(dialog, NORMAL_CURSOR);
	display_error(NO_PRINTER, NULL, FALSE);
	return;
    }

    /* 
     * gzipped/html file
     */
    if (strstr(current_chapter->fname, ".gz")) {
	bGzMode = 1;
    } else {
	bGzMode = 0;
    }

    /*
     * Print a single chapter
     */
    if (print_mode == PRINT_CHAPTER) {
	if (bGzMode == 1) {
  	    (void)sprintf(buffer,
			"/usr/sbin/gzcat -c %s | %s /usr/bin/col | "
			"/usr/bin/lp -d%s -t'%s Relnotes, Ch. %d' 2>&1",
			current_chapter->fname,
			(strstr(current_chapter->fname, "/ch") ?
						"" : "/usr/sbin/html2term | "),
			selected_printer, current_product->title,
	    		atoi(strrchr(current_chapter->fname, '/')+3));
	} else {
  	    (void)sprintf(buffer,
	               "/usr/bin/pcat %s | /usr/bin/col | "
                       "/usr/bin/lp -d%s -t'%s Relnotes, Ch. %d' 2>&1",
			current_chapter->fname, selected_printer,
			current_product->title,
	    		atoi(strrchr(current_chapter->fname, '/')+3));
	}
    }

    /*
     * Print all chapters
     */
    else {

	if (bGzMode == 1) {
	    (void)strcpy(buffer, "/usr/sbin/gzcat -c ");
	} else {
	    (void)strcpy(buffer, "/usr/bin/pcat ");
	}

	for (i = 0; i < current_product->nchapters; i++) {
	     (void)strcat(buffer, current_product->chapters[i].fname);
	     (void)strcat(buffer, " ");
	}

	if (bGzMode == 1 && current_product->nchapters > 0
	    &&
            !(strstr(current_product->chapters[0].fname, "/ch"))) {
	    	(void)strcat(buffer, "| /usr/sbin/html2term "); 
	}

	(void)strcat(buffer, "| /usr/bin/col | /usr/bin/lp -d");
	(void)strcat(buffer, selected_printer);
	(void)strcat(buffer, " -t'");
	(void)strcat(buffer, current_product->title);
	(void)strcat(buffer, " Relnotes' 2>&1");
    }

    /*
     * Post a job submitted message
     */
    XmTextSetString(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT),
			app_data.print_submit_msg);
    XFlush(XtDisplay(dialog));

    /*
     * Issue the actual command to print and read the 
     * response.
     */
    if ((fptr = popen(buffer, "r")) == NULL) {
	(void)sprintf(buffer, "%s - cannot popen printing command\n",
							prog_name);
	set_cursor(toplevel, NORMAL_CURSOR);
	set_cursor(dialog, NORMAL_CURSOR);
	XtWarning(buffer);
	return;
    }
    *buffer = '\0';
    while (fgets(buffer, BUFSIZ, fptr) != NULL)
	;
    (void)pclose(fptr);

    /*
     * Post response in info field
     */
    buffer[strlen(buffer)-1] = '\0';
    XmTextSetString(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT), buffer);

    /*
     * Set cursor to normal
     */
    set_cursor(toplevel, NORMAL_CURSOR);
    set_cursor(dialog, NORMAL_CURSOR);
}


/**************************************************************************
 *
 * Function: dismiss
 *
 * Description: Handles the dismiss dialog button.
 *
 * Parameters:
 *	w (I) - invoking widget
 *	client_data (I) - dialog widget
 *	call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void dismiss(Widget w, XtPointer client_data, XtPointer call_data)
{
    XtUnmanageChild((Widget)client_data);
}


/**************************************************************************
 *
 * Function: get_printer_list
 *
 * Description: Creates a list of available printers and determines
 *	the default printer. The list of printer is then displayed.
 *
 * Parameters: none
 *
 * Return: none
 *
 **************************************************************************/

static void get_printer_list()
{
    register int i, select_pos;
    char buffer[PATH_MAX];
    FILE *fptr;
    XmString *printers_str;
    DEFARGS(5);

    /*
     * Free any existing printer information
     */
    if (default_printer) {
	free((char*)default_printer);
	default_printer = NULL;
    }
    if (num_printers) {
	for (i = 0; i < num_printers; i++)
	    free((char*)printer_list[i]);
	num_printers = 0;
    }

    /*
     * Get new printer info
     */
    if ((fptr = popen(PRINTER_LIST_CMD, "r")) == NULL) {
	(void)sprintf(buffer, "%s - cannot popen printing command\n",
								prog_name);
	XtWarning(buffer);
	return;
    }
    i = -1;
    while (fgets(buffer, PATH_MAX, fptr) != NULL) {
	buffer[strlen(buffer)-1] = '\0';
	if (i == -1)
	    default_printer = strdup(buffer);
	else
	    printer_list[i] = strdup(buffer);
	if (++i >= MAX_PRINTERS)
	    break;
    }
    num_printers = i;
    (void)pclose(fptr);

    /*
     * Display the new printer info
     */
    if (!num_printers)
	return;
    printers_str = (XmString*)XtMalloc(num_printers * sizeof(XmString));
    for (i = 0; i < num_printers; i++)
	printers_str[i] = XmStringCreateSimple(printer_list[i]);
    STARTARGS;
    SETARG(XmNitems, printers_str);
    SETARG(XmNitemCount, num_printers);
    XtSetValues(plist_widget, ARGLIST);
    for (i = 0; i < num_printers; i++)
	XmStringFree(printers_str[i]);
    XtFree((char*)printers_str);

    /*
     * Select the appropriate printer
     */
    select_pos = 0;
    if (selected_printer)
        select_pos = find_printer(selected_printer);
    if (!select_pos)
        select_pos = find_printer(default_printer);
    if (!select_pos)
	select_pos = 1;
    XmListSelectPos(plist_widget, select_pos, True);
    XmListSetBottomPos(plist_widget, select_pos);
}


/**************************************************************************
 *
 * Function: find_printer
 *
 * Description: Linearly searches the list of available printers looking
 *	for the specified printer name.
 *
 * Parameters: 
 *	printer_name (I) - name of printer to find
 *
 * Return: Returns the list index if the printer is found. Returns 0
 *	if the printer is not found.
 *
 **************************************************************************/

static int find_printer(char *printer_name)
{
    register int i;
    int loc = 0;

    for (i = 0; i < num_printers; i++) {
	if (!strcmp(printer_list[i], printer_name)) {
	    loc = i + 1;
	    break;
	}
    }

    return loc;
}


/**************************************************************************
 *
 * Function: printer_select_cb
 *
 * Description: Called when a printer is selected.
 *
 * Parameters: 
 *	w (I) - the invoking widget
 *	client_data (I) - not used
 *	call_data (I) - list callback struct
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void printer_select_cb(Widget w, XtPointer client_data,
				XtPointer call_data)
{
    if (selected_printer)
	XtFree((char*)selected_printer);
    XmStringGetLtoR(((XmListCallbackStruct*)call_data)->item,
			XmSTRING_DEFAULT_CHARSET, &selected_printer);
}
