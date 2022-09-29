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
 * File: search.c
 *
 * Description: Regular expression text search support for the release
 *	notes browser.
 *
 **************************************************************************/


#ident "$Revision: 1.1 $"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/SelectioB.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

#include "grelnotes.h"
#include "TextView.h"


/* File scope globals */

static Widget search_expr_widget;
static Widget searchb_widget;
static Widget info_widget;
static expr_changed; 
static VwrPositionStruct orig_start_pos, orig_end_pos;


/* Local functions */

static Widget create_search_dialog(void);
static void search_cb(Widget, XtPointer, XtPointer);
static void dismiss_cb(Widget, XtPointer, XtPointer);
static void search_sense_cb(Widget, XtPointer, XtPointer);
static void init_orig_pos_cb(Widget, XtPointer, XtPointer);


/**************************************************************************
 *
 * Function: search_handler
 *
 * Description: Brings up the search dialog
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

void search_handler(Widget w, XtPointer client_data, XtPointer call_data)
{
    static Widget search_dialog = NULL;

    /*
     * Create the dialog if first time
     */
    if (!search_dialog)
	search_dialog = create_search_dialog();

    /*
     * Check for search button sensitivity
     */
    search_sense_cb(NULL, NULL, NULL);

    /*
     * Popup the dialog
     */
    XtManageChild(search_dialog);
}


/*
 ==========================================================================
			   LOCAL FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: create_search_dialog
 *
 * Description: Creates the search dialog
 *
 * Parameters: none
 *
 * Return: Created dialog widget
 *
 **************************************************************************/

static Widget create_search_dialog()
{
    XmString dismiss_str, search_str, expr_str;
    XmString info_str;
    Widget dialog, rowc;
    Widget widget;
    DEFARGS(20);

    /*
     * Create labels for buttons and such
     */
    dismiss_str = XmStringCreateSimple("Dismiss");
    search_str = XmStringCreateSimple("Search");
    expr_str = XmStringCreateSimple("Search For:");
    info_str = XmStringCreateSimple("Search Info:");

    /*
     * Create a prompt dialog
     */
    STARTARGS;
    SETARG(XmNokLabelString, search_str);
    SETARG(XmNcancelLabelString, dismiss_str);
    SETARG(XmNdialogTitle, search_str);
    SETARG(XmNautoUnmanage, False);
    SETARG(XmNselectionLabelString, info_str);
    dialog = XmCreatePromptDialog(toplevel, "searchDialog", ARGLIST);
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_SEPARATOR));

    /*
     * Get the widget IDs for later use
     */
    searchb_widget = XmSelectionBoxGetChild(dialog, XmDIALOG_OK_BUTTON);
    info_widget = XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT);

    /*
     * Make the info text field read-only
     */
    STARTARGS;
    SETARG(XmNeditable, False);
    XtSetValues(info_widget, ARGLIST);

    /*
     * Create a row column to hold the additional dialog items
     */
    rowc = XtCreateManagedWidget("searchDialogRc", xmRowColumnWidgetClass,
		    		   dialog, NULL, 0);

    /*
     * Create search string label and text field
     */
    STARTARGS;
    SETARG(XmNlabelString, expr_str);
    XtCreateManagedWidget("searchExprLabel", xmLabelWidgetClass,
				rowc, ARGLIST);
    search_expr_widget = XtCreateManagedWidget("searchExprText",
				xmTextFieldWidgetClass,
				rowc, NULL, 0);

    /*
     * Register a value changed callback for the search
     * expression field so that we can set search button
     * sensitivity.
     */
    XtAddCallback(search_expr_widget, XmNvalueChangedCallback,
						search_sense_cb, NULL);


    /*
     * Register action button callbacks
     */
    XtAddCallback(dialog, XmNokCallback, search_cb, (XtPointer)dialog);
    XtAddCallback(dialog, XmNcancelCallback, dismiss_cb, (XtPointer)dialog);
    XtAddCallback(dialog, XmNhelpCallback, display_help,
						(XtPointer)PROGRAM_HELP);

    /*
     * Deallocate all strings
     */
    XmStringFree(dismiss_str);
    XmStringFree(search_str);
    XmStringFree(expr_str);
    XmStringFree(info_str);

    /*
     * Add callback for text load notifications and
     * initialize the positions
     */
    XtAddCallback(text_viewer, VwrNloadCallback, init_orig_pos_cb,
						(XtPointer)NULL);
    init_orig_pos_cb(NULL, NULL, NULL);

    return dialog;
}


/**************************************************************************
 *
 * Function: search_cb
 *
 * Description: Performs the requested regular expression text search.
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

static void search_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget dialog = (Widget)client_data;
    char *search_expr;
    VwrPositionStruct start_pos, end_pos;
    int found, back = 0;

    /*
     * Set cursor to busy
     */
    set_cursor(toplevel, BUSY_CURSOR);
    set_cursor(dialog, BUSY_CURSOR);

    /*
     * Get the search expression and search for it in
     * the text. We only want to specify the string when
     * it has changed.
     */
    if (expr_changed) {
	expr_changed = False;
	init_orig_pos_cb(NULL, NULL, NULL);
        search_expr = XmTextFieldGetString(search_expr_widget);
        if ((found = VwrTextViewSearch(text_viewer, search_expr, False,
				&start_pos, &end_pos)) == 1) {
	    orig_start_pos = start_pos;
	    orig_end_pos = end_pos;
	}
        XtFree((char*)search_expr);
    } else {
        if ((found = VwrTextViewSearch(text_viewer, NULL, False,
				&start_pos, &end_pos)) == 1) {
	    if (!memcmp(&start_pos, &orig_start_pos, sizeof(VwrPositionStruct))
			&& !memcmp(&end_pos, &orig_end_pos,
						sizeof(VwrPositionStruct))) {
		back = 1;
	    } else {
		back = 0;
	    }
	}
    }

    /*
     * If we found the string highlight it otherwise indicate
     * not found to the user
     */
    if (found) {
	if (back)
	    XmTextSetString(info_widget, "Found (back to start)");
	else
	    XmTextSetString(info_widget, "Found");
	VwrTextViewSetSelection(text_viewer, &start_pos, &end_pos, False);
    } else {
	XmTextSetString(info_widget, "Not Found");
    }

    /*
     * Set cursor to normal
     */
    set_cursor(toplevel, NORMAL_CURSOR);
    set_cursor(dialog, NORMAL_CURSOR);
}


/**************************************************************************
 *
 * Function: dismiss_cb
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

static void dismiss_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    XtUnmanageChild((Widget)client_data);
}


/**************************************************************************
 *
 * Function: search_sense_cb
 *
 * Description: Checks whether there is a search expression entered in the
 *	search expression text field to determine whether the search
 *	button should be set to sensitive or insensitive.
 *
 * Parameters:
 *      w (I) - widget that invoked this callback
 *      client_data (I) - unused
 *      call_data (I) - unused
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void search_sense_cb(Widget w, XtPointer client_data,
                                                XtPointer call_data)
{
    char *str;
    register char *sptr;
    Boolean sensitive = False;

    expr_changed = True;
    str = XmTextFieldGetString(search_expr_widget);
    if (*str)
	sensitive = True;
    XtFree((char*)str);

    XtSetSensitive(searchb_widget, sensitive);
}


/**************************************************************************
 *
 * Function: init_orig_pos_cb
 *
 * Description: Initializes the original search find position variables.
 *
 * Parameters: 
 *      w (I) - widget that invoked this callback
 *      client_data (I) - unused
 *      call_data (I) - unused
 *
 * Return: none
 *
 **************************************************************************/
/* ARGSUSED */

static void init_orig_pos_cb(Widget w, XtPointer client_data,
						XtPointer call_data)
{
    orig_start_pos.col = orig_start_pos.line = -1;
    orig_end_pos.col = orig_end_pos.line = -1;
}

