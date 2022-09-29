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
 * File: help.c
 *
 * Description: Help messages and dialog handling.
 *
 **************************************************************************/


#ident "$Revision: 1.2 $"


#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/MessageB.h>

#include "grelnotes.h"


/* Help message - text is contained in the file grelnotes.hlp */

static char *program_help[] = {
#include "grelnotes.hlp"
		};


static Widget create_help_dialog(void);


/**************************************************************************
 *
 * Function: display_help
 *
 * Description: Displays the help dialog with the speicifed help message
 *
 * Parameters: 
 *      w (I) - invoking widget
 *      client_data (I) - help ID token
 *      call_data (I) - standard callback info
 *
 * Return: none
 *
 **************************************************************************/
 /* ARGSUSED */

void display_help(Widget w, XtPointer client_data, XtPointer call_data)
{
    static Widget help_dialog = NULL;
    char buffer[512];
    XmString mess;
    DEFARGS(5);

    /*
     * If this is first time, create the dialog window
     */
    if (!help_dialog)
	help_dialog = create_help_dialog();

    /*
     * Construct the help message into a multi-line compound string
     */
    switch ((int)client_data) {
        case PROGRAM_HELP:
	    mess = create_message(program_help);
	    break;
        case VERSION_HELP:
	    (void)sprintf(buffer, app_data.help_prog_msg, prog_name);
	    mess = XmStringCreateSimple(buffer);
	    mess = XmStringConcat(mess, XmStringSeparatorCreate());
	    mess = XmStringConcat(mess, XmStringSeparatorCreate());
	    (void)sprintf(buffer, app_data.help_ver_msg, prog_version);
	    mess = XmStringConcat(mess, XmStringCreateSimple(buffer));
	    break;
    }

    /*
     * Display the string then free it
     */
    STARTARGS;
    SETARG(XmNmessageString, mess);
    XtSetValues(help_dialog, ARGLIST);
    XmStringFree(mess);

    /*
     * Display the dialog
     */
    XtManageChild(help_dialog);
}


/*
 ==========================================================================
			  LOCAL FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: create_help_dialog
 *
 * Description: Creates the help dialog 
 *
 * Parameters: none
 *
 * Return: Created dialog widget
 *
 **************************************************************************/

static Widget create_help_dialog()
{
    Widget dialog;
    DEFARGS(5);

    /*
     * Create a modeless information dialog
     */
    STARTARGS;
    SETARG(XmNdialogStyle, XmDIALOG_MODELESS);
    dialog = XmCreateInformationDialog(toplevel, "helpDialog", ARGLIST);

    /*
     * Blow off the Cancel and Help buttons
     */
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));

    return dialog;
}

