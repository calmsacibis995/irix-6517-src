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
 * File:	master-cb.c              			          *
 *									  *
 * Description:	This file contains call backs for the master window  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

#define MAJ_EVENT 1
#define MIN_EVENT 2
#define TRIG_EVENT 3



#define START_LIVE_STREAM_WINDOW_WIDTH 318

void help_general_display();

Widget YesHostButton, NoHostButton, YesLogButton, NoLogButton;
Widget BaseFilenameText, RemoteHostText;
Widget cpuToggleButton[MAX_PROCESSORS];

Widget EventNumbWindow;
Widget EventNumbLabel;
Widget EventNumbText;

int setting_numb_event;

Widget HelpDisplay, TopHelpShell;

void 
close_event_numb_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(EventNumbWindow);

}

#define MAX_EVENT_NUMB 65536

void 
okay_event_numb_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    char event_str[32];
    int event_numb;
    char menu_lab[32];
    XmString tempString;
    char msg_str[64];

    strcpy(event_str, XmTextFieldGetString(EventNumbText));

    event_numb = atoi(event_str);

    if ((event_numb < 0) || (event_numb > MAX_EVENT_NUMB)) {
	sprintf(msg_str, "event number must be between 0 and %d\n", MAX_EVENT_NUMB);
	grtmon_message(msg_str, ALERT_MSG, 85);
	event_numb = 0;
    }

    if (setting_numb_event == MAJ_EVENT) {
	maj_event[choice_maj_event].numb = event_numb;
	sprintf(menu_lab, "user defined (%d)", event_numb);
	tempString = XmStringCreateSimple (menu_lab);

	XtVaSetValues (maj_val[numb_maj_events-1], XmNlabelString, 
		       tempString, NULL);
	XmStringFree (tempString);
    }
    else if (setting_numb_event == MIN_EVENT) {
	min_event[choice_min_event].numb = event_numb;
	sprintf(menu_lab, "user defined (%d)", event_numb);
	tempString = XmStringCreateSimple (menu_lab);

	XtVaSetValues (min_val[numb_min_events-1], XmNlabelString, 
		       tempString, NULL);
	XmStringFree (tempString);
    }
    else if (setting_numb_event == TRIG_EVENT) {
	trigger_event[choice_trigger_event].numb = event_numb;
	sprintf(menu_lab, "user defined (%d)", event_numb);
	tempString = XmStringCreateSimple (menu_lab);

	XtVaSetValues (trigger_val[numb_trigger_events-1], XmNlabelString, 
		       tempString, NULL);
	XmStringFree (tempString);
    }
    else
	grtmon_message("internal error please report number", ERROR_MSG, 86);

    XtUnmanageChild(EventNumbWindow);
}


void
get_event_numb_CB()
{
    static int varset = 0;
    static Widget EventNumbDialog;
    Atom wm_delete_window;
    Widget ButtonArea, CancelButton, OkayButton;
    XmString tempString;
    Widget separator0, separator1, separator2;
    int window_width;

    if (!varset) {
	varset++;

	
	window_width = 300;
	EventNumbDialog =  XtVaCreateWidget ("Get Event Number",
	    xmDialogShellWidgetClass, master_toplevel,
	    XmNtitle, "Open File",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(EventNumbDialog),
				    "WM_DELETE_WINDOW",
				    FALSE);

	XmAddWMProtocolCallback(EventNumbDialog, wm_delete_window, 
				close_event_numb_CB, NULL);

	EventNumbWindow = XtVaCreateWidget ("text",
	    xmFormWidgetClass, EventNumbDialog,
            XmNwidth, window_width,
	    NULL);

	separator0 = XtVaCreateManagedWidget ("sep0",
	    xmSeparatorWidgetClass, EventNumbWindow,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width - 1,
            NULL);

	EventNumbLabel = XtVaCreateManagedWidget ("",
	    xmLabelWidgetClass, EventNumbWindow,
	    XmNx, 5,
	    XmNy, 25,
            XmNwidth, 180,
	    NULL);

	EventNumbText = XtVaCreateManagedWidget ("event number text",
	    xmTextFieldWidgetClass, EventNumbWindow,
            XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator0,
            XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, EventNumbLabel,
            XmNwidth, 80,
            XmNvalue, "0",
	    NULL);

	separator1 = XtVaCreateManagedWidget ("sep1",
	    xmSeparatorWidgetClass, EventNumbWindow,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, EventNumbText,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width - 1,
            NULL);

	ButtonArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, EventNumbWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator1,
	    XmNleftAttachment, XmATTACH_FORM,
	    NULL);

	tempString = XmStringCreateSimple ("Okay");
	OkayButton = XtVaCreateManagedWidget ("okay",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator1,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (OkayButton, XmNactivateCallback, okay_event_numb_CB, NULL);

	tempString = XmStringCreateSimple ("Cancel");
	CancelButton = XtVaCreateManagedWidget ("cancel",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNx, window_width-87,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (CancelButton, XmNactivateCallback, close_event_numb_CB, NULL);

	separator2 = XtVaCreateManagedWidget ("sep2",
	    xmSeparatorWidgetClass, EventNumbWindow,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, ButtonArea,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 1,
            XmNwidth, window_width - 1,
            NULL);


    }
    XtManageChild(EventNumbWindow);

    if (setting_numb_event == MAJ_EVENT)
	tempString = XmStringCreateSimple ("Enter Major Event Number: ");
    else if (setting_numb_event == MIN_EVENT)
	tempString = XmStringCreateSimple ("Enter Minor Event Number: ");
    else if (setting_numb_event == TRIG_EVENT)
	tempString = XmStringCreateSimple ("Enter Trigger Event Number: ");
    else
	grtmon_message("internal error please report number", ERROR_MSG, 87);
    XtVaSetValues (EventNumbLabel, XmNlabelString, tempString, NULL);
    XmStringFree (tempString);

}


void
trigger_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtVaSetValues (trigger_val[choice_trigger_event],
		   XmNset, False, NULL);
    choice_trigger_event = (int)clientD;
    if (choice_trigger_event == (numb_trigger_events - 1)) {
	setting_numb_event = TRIG_EVENT;
	get_event_numb_CB();
    }
    XtVaSetValues (trigger_val[choice_trigger_event],
		   XmNset, True, NULL);
    draw_frame_start();
}


void
maj_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtVaSetValues (maj_val[choice_maj_event],
		   XmNset, False, NULL);
    choice_maj_event = (int)clientD;

    if (choice_maj_event == (numb_maj_events - 1)) {
	setting_numb_event = MAJ_EVENT;
	get_event_numb_CB();
    }
    XtVaSetValues (maj_val[choice_maj_event],
		   XmNset, True, NULL);
    draw_frame_start();
}


void
min_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtVaSetValues (min_val[choice_min_event],
		   XmNset, False, NULL);
    choice_min_event = (int)clientD;
    if (choice_min_event == (numb_min_events - 1)) {
	setting_numb_event = MIN_EVENT;
	get_event_numb_CB();
    }
    XtVaSetValues (min_val[choice_min_event],
		   XmNset, True, NULL);
    draw_frame_start();
}



void 
close_open_file_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(OpenFileWindow);

}

void 
load_file_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    char filename[64];
    char open_file[128];
    int i, count;
    int pid;
    char argv[4][32];
    int argc;
    struct pid_elem *new_elem;
    int save_no_frame_mode, save_collecting_nf_events;
    char msg_str[64];

    save_no_frame_mode = no_frame_mode;
    save_collecting_nf_events = collecting_nf_events;

    strcpy(filename, XmTextFieldGetString(OpenFileText));

    switch ((pid = fork())) {
      case 0:
	/* child process */
	init_stats();
	no_frame_mode = save_no_frame_mode;
	collecting_nf_events = save_collecting_nf_events;

	/* appropriate code taken from parse args */
	major_frame_start_event = TSTAMP_EV_START_OF_MAJOR;
	minor_frame_start_event = TSTAMP_EV_START_OF_MINOR;
	frame_trigger_event = TSTAMP_EV_INTRENTRY;
	debug = FALSE;
	logging = FALSE;
	live_stream = FALSE;
	show_live_stream = FALSE;
	gethostname(hostname, 128);

	get_procs_from_filelist(filename);
	
	count = 0;
	for (i=0; i<MAX_PROCESSORS; i++) {
	    if (cpu_on[i]) {
		count++;
		sprintf(open_file, "%s.%d.wvr", filename, i);
		fd[i] = open(open_file, O_RDONLY);
		if (fd[i] <0) {

		    sprintf(msg_str, "couldn't open %s for reading\n",open_file);
		    grtmon_message(msg_str, ERROR_MSG, 88);

		    perror(" could not be opened");
		    exit(-1);
		}
	    }
	}
	if (count == 0) {
	    sprintf(msg_str, "could not find any files with %s prefix\n",filename);
	    grtmon_message(msg_str, ERROR_MSG, 89);
	    exit(0);
	}
	getting_data_from = FILE_DATA; 
	scan_type = FILE_DATA; 
	
	old_main();
	break;
      case -1:
	perror("[create_process]: fork() failed");
	exit(1);
      default:
	new_elem = (struct pid_elem*) malloc (sizeof (struct pid_elem));
	new_elem->next = master_pid_list;
	new_elem->pid = pid;
	master_pid_list = new_elem;

	XtUnmanageChild(OpenFileWindow);
	return;
    }

}



void 
load_file(char *filename)
{
    char open_file[128];
    int i, count;
    int pid;
    char argv[4][32];
    int argc;
    struct pid_elem *new_elem;
    int save_no_frame_mode, save_collecting_nf_events;
    char msg_str[64];

    save_no_frame_mode = no_frame_mode;
    save_collecting_nf_events = collecting_nf_events;

    switch ((pid = fork())) {
      case 0:
	/* child process */
	init_stats();
	no_frame_mode = save_no_frame_mode;
	collecting_nf_events = save_collecting_nf_events;

	/* appropriate code taken from parse args */
	major_frame_start_event = TSTAMP_EV_START_OF_MAJOR;
	minor_frame_start_event = TSTAMP_EV_START_OF_MINOR;
	frame_trigger_event = TSTAMP_EV_INTRENTRY;
	debug = FALSE;
	logging = FALSE;
	live_stream = FALSE;
	show_live_stream = FALSE;
	gethostname(hostname, 128);

	get_procs_from_filelist(filename);
	
	count = 0;
	for (i=0; i<MAX_PROCESSORS; i++) {
	    if (cpu_on[i]) {
		count++;
		sprintf(open_file, "%s.%d.wvr", filename, i);
		fd[i] = open(open_file, O_RDONLY);
		if (fd[i] <0) {
		    sprintf(msg_str, "couldn't open %s for reading\n",open_file);
		    grtmon_message(msg_str, ERROR_MSG, 90);
		    perror(" could not be opened");
		    exit(-1);
		}
	    }
	}
	if (count == 0) {
	    sprintf(msg_str, "could not find any files with %s prefix\n",filename);
	    grtmon_message(msg_str, ERROR_MSG, 91);
	    exit(0);
	}
	getting_data_from = FILE_DATA; 
	scan_type = FILE_DATA; 
	
	old_main();
	break;
      case -1:
	perror("[create_process]: fork() failed");
	exit(1);
      default:
	new_elem = (struct pid_elem*) malloc (sizeof (struct pid_elem));
	new_elem->next = master_pid_list;
	new_elem->pid = pid;
	master_pid_list = new_elem;

	XtUnmanageChild(OpenFileWindow);
	return;
    }

}

void 
open_file_ok_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    String d_str;
    int i,count;

    XmFileSelectionBoxCallbackStruct *fsb = (XmFileSelectionBoxCallbackStruct *) callD;

    XmStringGetLtoR(fsb->value, XmFONTLIST_DEFAULT_TAG, &d_str);

    count = 0;
    for (i=strlen(d_str); i>0; i--) {
	if (d_str[i] == '.')
	    count++;
	if (count == 2) {
	    d_str[i] = '\0';
	    i=0;
	}

    }

    load_file(d_str);
    XtFree(d_str);

}

void
open_file_cancel_CB(Widget w, XtPointer clientD, XtPointer callD)
{
    XtUnmanageChild(OpenFileWindow);
}


void
open_file_help_CB(Widget w, XtPointer clientD, XtPointer callD)
{
    grtmon_message("The file to load must must have a .wvr extension", WARNING_MSG, 92);
}

void
open_file_CB()
{
    static int varset = 0;
    static Widget OpenFileDialog;
    int window_width, i;
    Atom wm_delete_window;
    XmString tempString;
    char dir_str[128], sys_str[128];
    Arg arg;

/*
    varset = random() %6;

    if (varset == 0)
	grtmon_message("this is a test to see how long the strings can be", ERROR_MSG, 2);
    if (varset == 1)
	grtmon_message("a short one", WARNING_MSG, 2);
    if (varset == 2)
	grtmon_message("what happens if the text of the \n message is very very long, does the button automatically  wrap or what really happens", NOTICE_MSG, 2);
    if (varset == 3)
	grtmon_message("this is a test to see how long the strings can be", FATAL_MSG, 2);
    if (varset == 4)
	grtmon_message("a short one", ALERT_MSG, 2);
    if (varset == 5)
	grtmon_message("what happens if the text of the \n message is very very long, does the button automatically  wrap or what really happens", DEBUG_MSG, 2);

    return;
*/
    if (!varset) {
	varset++;

	window_width = 265;
	OpenFileDialog =  XtVaCreateWidget ("Open File",
	    xmDialogShellWidgetClass, master_toplevel,
	    XmNtitle, "Open File",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(OpenFileDialog),
				    "WM_DELETE_WINDOW",
				    FALSE);

	XmAddWMProtocolCallback (OpenFileDialog, wm_delete_window, 
			     close_open_file_CB, NULL);

	strcpy(dir_str, default_filename);
	for (i=strlen(dir_str); i>0; i--) {
	    if (dir_str[i] == '/') {
		dir_str[i] = '\0';
		i=0;
	    }
	}


	tempString = XmStringCreateSimple (default_filename);
	OpenFileWindow = XtVaCreateManagedWidget ("text",
	    xmFileSelectionBoxWidgetClass, OpenFileDialog,
            XmNwidth, 500,
            XmNheight, 400,
	    NULL);
	XmStringFree (tempString);


	XtAddCallback (OpenFileWindow, XmNokCallback, open_file_ok_CB, NULL);
	XtAddCallback (OpenFileWindow, XmNcancelCallback, open_file_cancel_CB, NULL);
	XtAddCallback (OpenFileWindow, XmNhelpCallback, open_file_help_CB, NULL);


	tempString = XmStringCreateSimple (dir_str);
	XtSetArg(arg, XmNdirectory, tempString);
	XtSetValues(OpenFileWindow, &arg, 1);
	XmStringFree (tempString);

	tempString = XmStringCreateSimple ("*.wvr");
	XtVaSetValues(OpenFileWindow, XmNpattern, tempString, NULL);
	XmStringFree (tempString);

	tempString = XmStringCreateSimple (default_filename);
	XtVaSetValues(OpenFileWindow, XmNdirSpec, tempString, NULL);
	XmStringFree (tempString);

	return;
    }
    tempString = XmStringCreateSimple ("*.wvr");
    XtVaSetValues(OpenFileWindow, XmNpattern, tempString, NULL);
    XmStringFree (tempString);

    XtManageChild(OpenFileWindow);

  
}


void
old_open_file_CB()
{
    static int varset = 0;
    static Widget OpenFileDialog;
    Atom wm_delete_window;
    Widget FileNameLabel;
    Widget ButtonArea, CancelButton, LoadButton;
    XmString tempString;
    Widget separator0, separator1, separator2;
    int window_width;

    if (!varset) {
	varset++;

	window_width = 265;
	OpenFileDialog =  XtVaCreateWidget ("Open File",
	    xmDialogShellWidgetClass, master_toplevel,
	    XmNtitle, "Open File",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(OpenFileDialog),
				    "WM_DELETE_WINDOW",
				    FALSE);

	XmAddWMProtocolCallback (OpenFileDialog, wm_delete_window, 
			     close_open_file_CB, NULL);

	OpenFileWindow = XtVaCreateWidget ("text",
	    xmFormWidgetClass, OpenFileDialog,
            XmNwidth, window_width,
	    NULL);

	separator0 = XtVaCreateManagedWidget ("sep0",
	    xmSeparatorWidgetClass, OpenFileWindow,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width - 1,
            NULL);

	FileNameLabel = XtVaCreateManagedWidget ("Base name of file: ",
	    xmLabelWidgetClass, OpenFileWindow,
	    XmNx, 2,
	    XmNy, 25,
	    NULL);

	OpenFileText = XtVaCreateManagedWidget ("file open text",
	    xmTextFieldWidgetClass, OpenFileWindow,
            XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator0,
            XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, FileNameLabel,
            XmNwidth, 120,
            XmNvalue, default_filename,
	    NULL);

	separator1 = XtVaCreateManagedWidget ("sep1",
	    xmSeparatorWidgetClass, OpenFileWindow,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, OpenFileText,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width - 1,
            NULL);

	ButtonArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, OpenFileWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator1,
	    XmNleftAttachment, XmATTACH_FORM,
	    NULL);

	tempString = XmStringCreateSimple ("Load");
	LoadButton = XtVaCreateManagedWidget ("load",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator1,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (LoadButton, XmNactivateCallback, load_file_CB, NULL);

	tempString = XmStringCreateSimple ("Cancel");
	CancelButton = XtVaCreateManagedWidget ("cancel",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNx, window_width-87,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (CancelButton, XmNactivateCallback, close_open_file_CB, NULL);

	separator2 = XtVaCreateManagedWidget ("sep2",
	    xmSeparatorWidgetClass, OpenFileWindow,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, ButtonArea,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 1,
            XmNwidth, window_width - 1,
            NULL);


    }
    XtManageChild(OpenFileWindow);
}

void 
yes_host_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int i;

    XtVaSetValues(NoHostButton, XmNset, False, NULL);
    
    for (i=numb_processors; i<MAX_PROCESSORS; i++) {
	XtManageChild(cpuToggleButton[i]);
    }


}

void 
no_host_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int i;
    char lab_str[64];
    XmString tempString;

    XtVaSetValues(YesHostButton, XmNset, False, NULL);
    
    for (i=numb_processors; i<MAX_PROCESSORS; i++)
	XtUnmanageChild(cpuToggleButton[i]);

    XtVaSetValues(RemoteHostText, XmNvalue, "", NULL);
    


}

void 
yes_log_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(NoLogButton, XmNset, False, NULL);


}

void 
no_log_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(YesLogButton, XmNset, False, NULL);
	XtVaSetValues(BaseFilenameText, XmNvalue, "", NULL);


}

void 
yes_host_disarm_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(YesHostButton, XmNset, True, NULL);


}

void 
no_host_disarm_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(NoHostButton, XmNset, True, NULL);


}

void 
yes_log_disarm_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(YesLogButton, XmNset, True, NULL);


}

void 
no_log_disarm_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(NoLogButton, XmNset, True, NULL);


}

void 
close_start_live_stream_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(StartLiveStreamWindow);

}

void 
start_LS_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int i;
    Boolean is_set;
    int pid;
    struct pid_elem *new_elem;
    int count, fromlen, sock;
    char base_filename[128];
    struct sockaddr_in from;
    char sys_str[128];
    char open_file[128];
    char msg_str[64];


    switch ((pid = fork())) {
      case 0:
	/* child process */
	
	init_stats();
	
	gethostname(hostname, 128);

	/* appropriate code taken from parse args */
	major_frame_start_event = TSTAMP_EV_START_OF_MAJOR;
	minor_frame_start_event = TSTAMP_EV_START_OF_MINOR;
	frame_trigger_event = TSTAMP_EV_INTRENTRY;
	debug = FALSE;
	live_stream = TRUE;
	show_live_stream = TRUE;

	XtVaGetValues(YesLogButton, XmNset, &is_set, NULL);
	if (is_set) {
	    logging = TRUE;
	    strcpy(base_filename, XmTextFieldGetString(BaseFilenameText));
	    if (strlen(base_filename) == 0)
		strcpy(base_filename, "fv-data");
	    if (strlen(base_filename) > 100) {
		grtmon_message("truncating filename to 100 charatcers", ALERT_MSG, 93);
		base_filename[100] = '\0';
	    }
	}
	else {
	    logging = FALSE;
	}

	XtVaGetValues(YesHostButton, XmNset, &is_set, NULL);
	if (is_set) {
	    strcpy(hostname, XmTextFieldGetString(RemoteHostText));
	}

	for (i=0; i<MAX_PROCESSORS; i++) {
	    XtVaGetValues(cpuToggleButton[i], XmNset, &is_set, NULL);
	    cpu_on[i] = (int)is_set;
	    if ((cpu_on[i]) && (logging)) {
		sprintf(open_file, "%s.%d.wvr", base_filename, i);
		fd[i] = open(open_file, O_RDONLY);
		if (fd[i] >= 0) {
		    sprintf(msg_str, "file %s already exists, moving to %s.old",
			   open_file, open_file);
		    grtmon_message(msg_str, ALERT_MSG, 94);
		    sprintf(sys_str, "mv %s %s.old", open_file, open_file);
		    system(sys_str);
		    close(fd[i]);
		}
		fd_fvw[i] = open(open_file, O_WRONLY|O_CREAT,0666);
		if (fd_fvw[i] <0) {
		    sprintf(msg_str, "could not open %s for writing", open_file);
		    grtmon_message(msg_str, ERROR_MSG, 95);
		    perror("FrameView start");
		    exit(0);
		    }
		fd_fvr[i] = open(open_file, O_RDONLY);
		if (fd_fvr[i] <0) {
		    sprintf(msg_str, "could not open %s for reading", open_file);
		    grtmon_message(msg_str, ERROR_MSG, 96);
		    perror("FrameView start");
		    exit(0);
		}
	    }
	}
	getting_data_from = REAL_DATA;
	scan_type = REAL_DATA;
	count = 0;
	for (i=0; i<MAX_PROCESSORS; i++) {
	    if (cpu_on[i]) {
		rtmond_control_t rtmond;
		
		count++;
		if (rtmond_setup_control(&rtmond, hostname, 
					 RTMON_PROTOCOL1) < 0) {
		    sprintf(msg_str, "could eastablish connection to %s", 
			    hostname);
		    grtmon_message(msg_str, ERROR_MSG, 109);  
		    return;
		}
		
		fd[i] = rtmond_get_connection(&rtmond, i, RTMON_ALL);
		
	    }
	}
	if (count == 0) {
	    grtmon_message("no cpus for real-time stream", ERROR_MSG, 98);
	    exit(0);
	}
	old_main();
	break;
     case -1:
	perror("[create_process]: fork() failed");
	exit(1);
      default:
	new_elem = (struct pid_elem*) malloc (sizeof (struct pid_elem));
	new_elem->next = master_pid_list;
	new_elem->pid = pid;
	master_pid_list = new_elem;

	XtUnmanageChild(StartLiveStreamWindow);
	return;
    }

}

void
start_live_stream_CB()
{
    static int varset = 0;
    static Widget StartLiveStreamDialog;
    static Widget cpuLabel;
    Atom wm_delete_window;
    Widget separator0, separator1, separator2, sepV;
    XmString tempString;
    Widget CpuButtonArea;
    Widget LogFileArea, LogFileLabel;
    Widget BaseFilenameLabel;
    Widget HostFilenameLabel;
    char tmp_str[32];
    Widget RemoteHostArea, RemoteHostLabel;
    Widget StartButton, CancelButton;
    int i, adj;
    int window_width;
    Widget StartLiveStreamSep4;
    Widget StartLiveStreamSep3, StartLiveStreamButtonArea;


    if (!varset) {
	varset++;

	window_width = START_LIVE_STREAM_WINDOW_WIDTH;

	StartLiveStreamDialog =  XtVaCreateWidget ("Start Live Stream",
	    xmDialogShellWidgetClass, master_toplevel,
	    XmNtitle, "Start Live Stream",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);

	wm_delete_window = XmInternAtom(XtDisplay(StartLiveStreamDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (StartLiveStreamDialog, wm_delete_window, 
				 close_start_live_stream_CB, NULL);


	StartLiveStreamWindow = XtVaCreateWidget ("text",
	    xmFormWidgetClass, StartLiveStreamDialog,
            XmNwidth, window_width,
	    NULL);


	separator0 = XtVaCreateManagedWidget ("sep0",
	    xmSeparatorWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width-1,
            NULL);


	cpuLabel = XtVaCreateManagedWidget ("cpu",
	    xmLabelWidgetClass, StartLiveStreamWindow,
	    XmNx, 1,
	    XmNy, 20,
	    NULL);

	/* this needs to be here or else the CpuButtonArea gets
	   goofed up for small numbers of processors */
	sepV = XtVaCreateManagedWidget ("sepV",
	    xmSeparatorWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, cpuLabel,			
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNorientation, XmVERTICAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 170,
            XmNwidth, 1,
            NULL);

	if (numb_processors == 1) 
	    tempString = XmStringCreateSimple("Select to monitor CPU 0");
	else
	    tempString = XmStringCreateSimple("Select CPUs to monitor:");

	XtVaSetValues(cpuLabel, XmNlabelString, tempString, NULL);
	XmStringFree(tempString);

	CpuButtonArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, cpuLabel,					
	    XmNleftAttachment, XmATTACH_FORM,
            XmNwidth, window_width-1,
            XmNheight, 170,
	    NULL);

	for (i=0;i<MAX_PROCESSORS;i++) {
	    adj = 0;
	    if (i > 11) adj = 10*((i-12)/6);
	    sprintf(tmp_str, "%d", i);
	    tempString = XmStringCreateSimple (tmp_str);
	    cpuToggleButton[i] = XtVaCreateManagedWidget ("cpu",
                xmToggleButtonWidgetClass, CpuButtonArea,
                XmNlabelString, tempString,
                XmNx, 4 + adj + 45*(i/6),
	        XmNy, 2 + 28*(i%6),
                NULL);
	    XmStringFree(tempString);
	}

	for (i=numb_processors; i<MAX_PROCESSORS; i++)
	    XtUnmanageChild(cpuToggleButton[i]);


	separator1 = XtVaCreateManagedWidget ("sep1",
	    xmSeparatorWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, sepV,					
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width-1,
            NULL);

	LogFileArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, separator1,					
	    XmNleftAttachment, XmATTACH_FORM,
            XmNwidth, window_width-1,
            XmNheight, 30,
	    NULL);


	tempString = XmStringCreateSimple("Log file: ");
	LogFileLabel = XtVaCreateManagedWidget ("log file",
	    xmLabelWidgetClass, LogFileArea,
            XmNlabelString, tempString,
	    XmNx, 2,
	    XmNy, 4,
	    NULL);
	XmStringFree(tempString);


	tempString = XmStringCreateSimple ("yes");
	YesLogButton = XtVaCreateManagedWidget ("yes",
                xmToggleButtonWidgetClass, LogFileArea,
                XmNlabelString, tempString,
                XmNindicatorType, XmONE_OF_MANY,
                XmNset, True,
                XmNx, 65,
	        XmNy, 0,
                NULL);
	XmStringFree(tempString);
	XtAddCallback (YesLogButton, XmNarmCallback, yes_log_CB, NULL);
	XtAddCallback (YesLogButton, XmNdisarmCallback, yes_log_disarm_CB, NULL);

	tempString = XmStringCreateSimple ("no");
	NoLogButton = XtVaCreateManagedWidget ("no",
                xmToggleButtonWidgetClass, LogFileArea,
                XmNlabelString, tempString,
                XmNindicatorType, XmONE_OF_MANY,
                XmNset, False,
                XmNx, 125,
	        XmNy, 0,
                NULL);
	XmStringFree(tempString);
	XtAddCallback (NoLogButton, XmNarmCallback, no_log_CB, NULL);
	XtAddCallback (NoLogButton, XmNdisarmCallback, no_log_disarm_CB, NULL);


	BaseFilenameText = XtVaCreateManagedWidget ("base filename text",
	    xmTextFieldWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, LogFileArea,					
            XmNx, 123,
            XmNwidth, 100,
            XmNvalue, "fv-data",
	    NULL);

	BaseFilenameLabel = XtVaCreateManagedWidget ("base filename:  ",
	    xmLabelWidgetClass, StartLiveStreamWindow,
            XmNx, 12, 
	    XmNy, 218,
	    NULL);


	separator2 = XtVaCreateManagedWidget ("sep2",
	    xmSeparatorWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, BaseFilenameText,			
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width-1,
            NULL);

	RemoteHostArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, separator2,					
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 30,
	    NULL);

	tempString = XmStringCreateSimple("Remote host: ");
	RemoteHostLabel = XtVaCreateManagedWidget ("remote host",
	    xmLabelWidgetClass, RemoteHostArea,
            XmNlabelString, tempString,
	    XmNx, 2,
	    XmNy, 4,
	    NULL);
	XmStringFree(tempString);


	tempString = XmStringCreateSimple ("yes");
	YesHostButton = XtVaCreateManagedWidget ("yes",
                xmToggleButtonWidgetClass, RemoteHostArea,
                XmNlabelString, tempString,
                XmNindicatorType, XmONE_OF_MANY,
                XmNset, False,
                XmNx, 105,
	        XmNy, 0,
                NULL);
	XmStringFree(tempString);
	XtAddCallback (YesHostButton, XmNarmCallback, yes_host_CB, NULL);
	XtAddCallback (YesHostButton, XmNdisarmCallback, yes_host_disarm_CB, NULL);

	tempString = XmStringCreateSimple ("no");
	NoHostButton = XtVaCreateManagedWidget ("no",
                xmToggleButtonWidgetClass, RemoteHostArea,
                XmNlabelString, tempString,
                XmNindicatorType, XmONE_OF_MANY,
                XmNset, True,
                XmNx, 165,
	        XmNy, 0,
                NULL);
	XmStringFree(tempString);
	XtAddCallback (NoHostButton, XmNarmCallback, no_host_CB, NULL);
	XtAddCallback (NoHostButton, XmNdisarmCallback, no_host_disarm_CB, NULL);

	RemoteHostText = XtVaCreateManagedWidget ("host filename text",
	    xmTextFieldWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, RemoteHostArea,					
            XmNx, 123,
            XmNwidth, 100,
            XmNvalue, "",
	    NULL);

	HostFilenameLabel = XtVaCreateManagedWidget ("host filename:  ",
	    xmLabelWidgetClass, StartLiveStreamWindow,
            XmNx, 12, 
	    XmNy, 300,
	    NULL);

	StartLiveStreamSep3 = XtVaCreateManagedWidget ("sep3",
	    xmSeparatorWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, RemoteHostText,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width-1,
            NULL);


	StartLiveStreamButtonArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, StartLiveStreamSep3,
	    XmNleftAttachment, XmATTACH_FORM,
	    NULL);

	StartButton = XtVaCreateManagedWidget ("Start",
	    xmPushButtonWidgetClass, StartLiveStreamButtonArea,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XtAddCallback (StartButton, XmNactivateCallback, start_LS_CB, NULL);

	CancelButton = XtVaCreateManagedWidget ("Cancel",
	    xmPushButtonWidgetClass, StartLiveStreamButtonArea,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNx, window_width-87,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XtAddCallback (CancelButton, XmNactivateCallback, close_start_live_stream_CB, NULL);


	StartLiveStreamSep4 = XtVaCreateManagedWidget ("sep4",
	    xmSeparatorWidgetClass, StartLiveStreamWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, StartLiveStreamButtonArea,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 2,
            XmNwidth, window_width-1,
            NULL);


    }
    XtManageChild(StartLiveStreamWindow);


}


void 
help_general_CB ()
{

    help_general_display ();

}


void 
close_help_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(HelpDisplay);
    XtUnmanageChild(TopHelpShell);

}



void
HelpChangedCB (Widget w, XtPointer clientD, XtPointer callD)
{
    ;
}

void
HelpVerifyCB (Widget w, XtPointer clientD, XtPointer callD)
{
    ;
}

void
help_general_display()
{
    static int varset = 0;
    static Widget HelpDialog;
    int n;
    Arg args[20];
    char tempChar[255];
    Atom wm_delete_window;

    if (!varset) {
	varset++;

	load_help_text();

	HelpDialog = XtVaCreateWidget ("help",
	    xmDialogShellWidgetClass, master_toplevel,
	    XmNtitle, "Help",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(HelpDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (HelpDialog, wm_delete_window, 
				 close_help_CB, NULL);


	TopHelpShell = XtVaCreateWidget ("help",
	    xmFormWidgetClass, HelpDialog,
	    NULL);

	n = 0;
	XtSetArg (args[n], XmNwidth, 650); n++;
	XtSetArg (args[n], XmNheight, 500); n++;

	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;

	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg(args[n], XmNeditable, False); n++;
	XtSetArg(args[n], XmNallowResize, False); n++;
	HelpDisplay = XmCreateScrolledText(TopHelpShell, "text", args, n);
	

	XtManageChild(HelpDisplay);

	XtAddCallback (HelpDisplay, XmNvalueChangedCallback, HelpChangedCB, NULL);
	XtAddCallback (HelpDisplay, XmNmodifyVerifyCallback, HelpVerifyCB, NULL);


    }

    XmTextSetString(HelpDisplay, help_text);


    XtManageChild(TopHelpShell);
    XtManageChild (HelpDisplay);

}


