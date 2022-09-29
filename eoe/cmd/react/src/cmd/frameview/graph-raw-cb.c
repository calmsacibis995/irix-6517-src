/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************

 **************************************************************************
 *	 								  *
 * File:	graph-raw-cb.c           			          *
 *									  *
 * Description:	This file handles the scrolling text for grtmon  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

Widget TopTextShell;

void
toggle_text_disp_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (text_display_on == FALSE) {
	text_display_on = TRUE;
    }
    else {
	text_display_on = FALSE;
    }
    text_display(text_display_on);
}


/* brings up the tet window if text_on  is true and hides it
   if it's false, it alse starts up the text display window
   if it hasn't already been initialized */
void
text_display (boolean text_on)
{
    static int varset = 0;
    static Widget textDialog;
    int n, i;
    Arg args[20];
    char tempChar[255];
    Atom wm_delete_window;

    
    if ((!varset) && (text_on)){
	varset++;

	
	textDialog = XtVaCreateWidget ("text",
	    xmDialogShellWidgetClass, toplevel,
	    XmNtitle, "Real-Time Monitor (Raw Timestamp Display)",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(textDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (textDialog, wm_delete_window, 
				 close_raw_tstamp_CB, NULL);


	TopTextShell = XtVaCreateWidget ("text",
	    xmFormWidgetClass, textDialog,
	    NULL);

	n = 0;
	XtSetArg (args[n], XmNwidth, 500); n++;
	XtSetArg (args[n], XmNheight, 500); n++;

	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;

	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg(args[n], XmNeditable, False); n++;
	XtSetArg(args[n], XmNallowResize, True); n++;
	TstampTextDisplay = XmCreateScrolledText(TopTextShell, "text", args, n);
	

	XtManageChild(TstampTextDisplay);

	XtAddCallback (TstampTextDisplay, XmNvalueChangedCallback, TextChangedCB, NULL);
	XtAddCallback (TstampTextDisplay, XmNmodifyVerifyCallback, TextVerifyCB, NULL);

	/* make sure we have soemthing loaded into the text buffer */
	for (i=0; i<MAX_PROCESSORS; i++) {
	    if ((cpu_on[i]) && (! no_frame_mode)) {
		goto_frame_number(maj_frame_numb[i], i);
	    }
	}
    }
    if (varset) {
	if (text_on) {
	    /*XmTextSetString(TstampTextDisplay, "TstampDisplay is a \n  test"); 
	      XmTextInsert(TstampTextDisplay, 
	      XmTextGetCursorPosition (TstampTextDisplay),
	      "Hello there");
	      XmTextInsert(TstampTextDisplay, 
	      XmTextGetLastPosition (TstampTextDisplay),
	      "end");*/

	    XtVaSetValues (ToggleTextMenu, XmNset, True, NULL);
	    XtManageChild(TopTextShell);
	    
	    XtManageChild (TstampTextDisplay);

	    
	} 
	else {
	    XtUnmanageChild (TstampTextDisplay);
	    
	    XtUnmanageChild(TopTextShell);
	    
	    XtVaSetValues (ToggleTextMenu, XmNset, False, NULL);
	    
	    
	}
    }
}


void 
close_raw_tstamp_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtVaSetValues (ToggleTextMenu, XmNset, False, NULL);
    text_display_on = FALSE;

}



void
TextChangedCB (Widget w, XtPointer clientD, XtPointer callD)
{
    ;
}

void
TextVerifyCB (Widget w, XtPointer clientD, XtPointer callD)
{
    ;
}

