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
 * File:	dialog.c					          *
 *									  *
 * Description:	This file contains the routines that create      	  *
 *              dialog message boxes                                      *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#define GRMAIN extern
#include "graph.h"


static int msg_dialog_count = 0;
Widget MsgDialog[MAX_DIALOGS], MsgWindow[MAX_DIALOGS], MsgShell[MAX_DIALOGS];

static int msg_varset = 0;
void msg_close_CB (Widget w, XtPointer clientD, XtPointer callD);
void msg_help_CB (Widget w, XtPointer clientD, XtPointer callD);
void msg_close_last ();
void setup_msg_box();

#ifdef ZERO
    char msg_str[64];

    grtmon_message(msg_str, ,);

#endif


/* identifier should be unique for easy identification of location
   current identifier number to use: 110
*/

/* only one process can use the graphics so we need to have all possible
   processes coorindate with the parent process that start things up
   so we'll use the shared area msg_data to put the message in and then
   indicate it's ready via setting the ready bit */
void
grtmon_message(char *msg, int type, int identifier)
{
    int ret_val;

    ret_val = atomicInc(&msg_data->access_count);

    while (ret_val !=0) {
	atomicDec(&msg_data->access_count);
	ret_val = atomicInc(&msg_data->access_count);
	sginap(10);
    }

    strcpy(msg_data->msg, msg);
    msg_data->type = type;
    msg_data->identifier = identifier;


    msg_data->ready = 1;

    while(msg_data->ready)
	sginap(10);
    atomicDec(&msg_data->access_count);

}

void
message_dialog(char *msg, int type, int identifier)
{
    XmString tempString;
    char type_str[32];
    char msg_str[256];
    int which_msg;
    int i;
    
    if (type > message_level) {
	msg_data->ready = 0;
	return;
    }
    if (!msg_varset) {
	setup_msg_box();
    }  
    
    if (msg_dialog_count == MAX_DIALOGS)
	msg_close_last();
    else
	msg_dialog_count++;

    for (i=0; i<MAX_DIALOGS; i++) {
	if (msg_in_use[i] == 0) {
	    which_msg = i;
	    i=MAX_DIALOGS;
	}
    }



    if (strlen (msg) > 150)
	msg[150] = '\0';

    if (type <= ERROR_MSG) {
	if (type == FATAL_MSG)
	    strcpy(type_str, "Fatal Error");
	else
	    strcpy(type_str, "Error");
	XtVaSetValues(MsgDialog[which_msg], XmNdialogType, XmDIALOG_ERROR, NULL);
    }
    else if (type <= ALERT_MSG) {
	if (type == WARNING_MSG)
	    strcpy(type_str, "Warning");
	else
	    strcpy(type_str, "Alert");
	XtVaSetValues(MsgDialog[which_msg], XmNdialogType, XmDIALOG_WARNING, NULL);
    }
    else{
	if (type == NOTICE_MSG)
	    strcpy(type_str, "Notice");
	else
	    strcpy(type_str, "Debug");
	XtVaSetValues(MsgDialog[which_msg], XmNdialogType, XmDIALOG_INFORMATION, NULL);
    }

    sprintf(msg_str, "FrameView %s", type_str);
    XtVaSetValues(MsgShell[which_msg], XmNtitle, msg_str, NULL);

    sprintf(msg_str, "%s %d: %s", type_str, identifier, msg);

    tempString = XmStringCreateSimple (msg_str);
    XtVaSetValues(MsgDialog[which_msg], XmNmessageString, tempString, NULL);
    XmStringFree (tempString);

    XtManageChild(MsgDialog[which_msg]);
    XtManageChild(MsgWindow[which_msg]);
    XtManageChild(MsgShell[which_msg]);
    msg_in_use[which_msg] = 1;
    msg_data->ready = 0;

}

void
setup_msg_box()
{
    XmString tempString;
    Atom wm_delete_window;
    Widget tempWidget;
    Widget msg_toplevel;
    int which_msg;


    msg_varset++;

    for (which_msg=0; which_msg<MAX_DIALOGS; which_msg++) {
	MsgShell[which_msg] =  XtVaCreateWidget ("MsgShell",
	    xmDialogShellWidgetClass, master_toplevel,
	    XmNtitle, "Message",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);

	wm_delete_window = XmInternAtom(XtDisplay(MsgShell[which_msg]),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback(MsgShell[which_msg], wm_delete_window, 
			    msg_close_CB, NULL);

	MsgWindow[which_msg] = XtVaCreateWidget ("text",
	    xmFormWidgetClass, MsgShell[which_msg],
	    NULL);

	MsgDialog[which_msg] = XtVaCreateWidget ("MsgDialog",
	    xmMessageBoxWidgetClass, MsgWindow[which_msg],
            XmNtitle, "",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);

	tempWidget = XmMessageBoxGetChild (MsgDialog[which_msg], XmDIALOG_CANCEL_BUTTON);
	XtUnmanageChild (tempWidget);
    
	tempWidget = XmMessageBoxGetChild (MsgDialog[which_msg], XmDIALOG_OK_BUTTON);
	XtAddCallback (tempWidget, XmNactivateCallback, msg_close_CB, NULL);
	
	tempWidget = XmMessageBoxGetChild (MsgDialog[which_msg], XmDIALOG_HELP_BUTTON);
	XtAddCallback (tempWidget, XmNactivateCallback, msg_help_CB, NULL);
	
	tempString = XmStringCreateSimple ("Continue");
	XtVaSetValues(MsgDialog[which_msg], XmNokLabelString, tempString, NULL);
	XmStringFree (tempString);
	
    }
}    

void 
msg_close_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int which_msg;
    int i;
    Widget parent;

    parent = XtParent(w);

    for (i=0; i<MAX_DIALOGS; i++) {
	if (w==MsgShell[i] || parent == MsgDialog[i]) {
	    which_msg = i;
	}
    }
    XtUnmanageChild(MsgDialog[which_msg]);
    XtUnmanageChild(MsgWindow[which_msg]);
    XtUnmanageChild(MsgShell[which_msg]);
    msg_in_use[which_msg] = 0;
}


void
msg_close_last ()
{

    XtUnmanageChild(MsgDialog[MAX_DIALOGS-1]);
    XtUnmanageChild(MsgWindow[MAX_DIALOGS-1]);
    XtUnmanageChild(MsgShell[MAX_DIALOGS-1]);
    msg_in_use[MAX_DIALOGS-1] = 0;
}

void 
msg_help_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    system("inform 'sorry no help available for this message'");

}



