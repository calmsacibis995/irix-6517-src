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
 * File: errors.c
 *
 * Description: Error messages and dialog handling.
 *
 **************************************************************************/


#ident "$Revision: 1.2 $"


#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/MessageB.h>

#include "grelnotes.h"


static Widget create_error_dialog(void);
static void cancel_callback(Widget, XtPointer, XtPointer);


/**************************************************************************
 *
 * Function: display_error
 *
 * Description: Displays the error dialog with the speicifed message
 *
 * Parameters: 
 *	error_id (I) - integer ID for the error
 *	client_str (I) - a client specified string to display with certain
 *			 error messages
 *	quit_button (I) - if TRUE a quit button is provided. Otherwise
 *			  only the OK button is provided.
 *
 *	Note: client_str is only used by the NO_CMDLINE_PRODUCT to display
 *	      the product name and NO_CMDLINE_CHAPTER to display the chapter
 *	      name.
 *
 * Return: none
 *
 **************************************************************************/

void display_error(int error_id, char *client_str, int quit_button)
{
    static Widget error_dialog = NULL;
    char buffer[1024];
    XmString mess;
    DEFARGS(5);

    /*
     * If first time, create the error dialog window
     */
    if (!error_dialog)
	error_dialog = create_error_dialog();

    /*
     * Construct the error message into a multi-line compound string
     */
    STARTARGS;
    switch (error_id) {
        case NO_CMDLINE_PRODUCT:
	    (void)sprintf(buffer, app_data.product_notfound_msg, client_str);
	    mess = XmStringCreateLtoR(buffer, XmFONTLIST_DEFAULT_TAG);
    	    SETARG(XmNmessageString, mess);
            XtSetValues(error_dialog, ARGLIST);
    	    XmStringFree(mess);
	    break;
        case NO_CMDLINE_CHAPTER:
	    (void)sprintf(buffer, app_data.chapter_notfound_msg, client_str);
	    mess = XmStringCreateLtoR(buffer, XmFONTLIST_DEFAULT_TAG);
    	    SETARG(XmNmessageString, mess);
            XtSetValues(error_dialog, ARGLIST);
    	    XmStringFree(mess);
	    break;
        case NO_PRODUCTS:
    	    SETARG(XmNmessageString, app_data.no_products_msg);
            XtSetValues(error_dialog, ARGLIST);
	    break;
        case NO_PRINTER:
    	    SETARG(XmNmessageString, app_data.no_printer_msg);
            XtSetValues(error_dialog, ARGLIST);
	    break;
    }

    /*
     * Display the string then free it
     */

    /*
     * Decide on whether we need the quit button
     */
    if (quit_button)
        XtManageChild(XmMessageBoxGetChild(error_dialog,
						XmDIALOG_CANCEL_BUTTON));
    else
        XtUnmanageChild(XmMessageBoxGetChild(error_dialog,
						XmDIALOG_CANCEL_BUTTON));

    /*
     * Bring up the dialog
     */
    XtManageChild(error_dialog);
}


/*
 ==========================================================================
			  LOCAL FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: create_error_dialog
 *
 * Description: Creates the error dialog 
 *
 * Parameters: none
 *
 * Return: Created dialog widget
 *
 **************************************************************************/

static Widget create_error_dialog()
{
    Widget dialog;
    DEFARGS(10);

    /*
     * Create a modal dialog
     */
    STARTARGS;
    SETARG(XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
    dialog = XmCreateErrorDialog(toplevel, "errorDialog", ARGLIST);

    /*
     * Register callbacks for OK and Cancel and blow off Help
     */
    XtAddCallback(dialog, XmNcancelCallback, cancel_callback, NULL);
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));

    return dialog;
}


/**************************************************************************
 *
 * Function: cancel_callback
 *
 * Description: Handles the cancel error dialog button. In this case we quit
 *	 the app.
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

static void cancel_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
    XtUnmanageChild(w);
    quit(NULL, NULL, NULL);
}

