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
 * File:	gr-cb.c                  			          *
 *									  *
 * Description:	This file contain general graphics callback functions  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

/* stuff for the template defining CB */
Widget TemplateProcText, TemplateMinorText;
int def_temp_numb_in_old_minor, def_temp_numb_in_new_minor;
Widget *TemplateInMinorText;
int define_template_cpu;
Widget AllSearchButton, MajSearchButton;

void switch_to_file_scan_type();

void 
all_search_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(MajSearchButton, XmNset, False, NULL);

}

void 
maj_search_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(AllSearchButton, XmNset, False, NULL);

}


void 
all_search_disarm_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(AllSearchButton, XmNset, True, NULL);


}

void 
maj_search_disarm_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    	XtVaSetValues(MajSearchButton, XmNset, True, NULL);


}


/*  brings up window that queires user on the event they want to search for */
void
search_event_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    static int varset = 0;
    static Widget searchDialog;
    Widget ButtonArea, searchButton, reverseButton, doneButton;
    Atom wm_delete_window;
    Widget searchDisplay;
    Widget cpuLabel, eventLabel;
    Widget separator0, separator1, separator12, separator2, separator3;
    Widget SearchRangeArea, SearchRangeLabel;
    XmString tempString;
    int window_width;

    if (!varset) {
	current_search_cpu = -1;
	varset++;

	window_width = 380;
	
	searchDialog = XtVaCreateWidget ("Find Event",
	    xmDialogShellWidgetClass, toplevel,
	    XmNtitle, "Find Event",
            XmNdeleteResponse, XmUNMAP,
            XmNwidth, window_width,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(searchDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (searchDialog, wm_delete_window, 
				 close_search_CB, NULL);


	searchWindow = XtVaCreateWidget ("text",
	    xmFormWidgetClass, searchDialog,
            XmNwidth, window_width,
	    NULL);



	separator0 = XtVaCreateManagedWidget ("Event name:",
	    xmSeparatorWidgetClass, searchWindow,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width - 1,
            NULL);

	eventLabel = XtVaCreateManagedWidget ("Event name:",
	    xmLabelWidgetClass, searchWindow,
	    XmNx, 2,
	    XmNy, 25,
	    NULL);


	SearchEventText = XtVaCreateManagedWidget ("event text",
	    xmTextFieldWidgetClass, searchWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator0,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, eventLabel,
            XmNwidth, 280,
            XmNvalue, "start of minor",
	    NULL);


	separator1 = XtVaCreateManagedWidget ("Event name:",
	    xmSeparatorWidgetClass, searchWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmSHADOW_ETCHED_IN,
	    XmNtopWidget, SearchEventText,
	    XmNleftAttachment, XmATTACH_FORM,
            XmNheight, 20,
            XmNwidth, window_width - 1,
            NULL);
  
	cpuLabel = XtVaCreateManagedWidget ("On cpu number:",
	    xmLabelWidgetClass, searchWindow,
	    XmNx, 2,
	    XmNy, 78,
	    NULL);

	SearchCpuText = XtVaCreateManagedWidget ("cpu text",
	    xmTextFieldWidgetClass, searchWindow,
            XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator1,
            XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, cpuLabel,
            XmNwidth, 250,
            XmNvalue, "1",
	    NULL);

	if (! no_frame_mode) {
	    separator12 = XtVaCreateManagedWidget ("Event name:",
	        xmSeparatorWidgetClass, searchWindow,
	        XmNtopAttachment, XmATTACH_WIDGET,
	        XmNorientation, XmHORIZONTAL,
                XmCSeparatorType, XmSHADOW_ETCHED_IN,
	        XmNtopWidget, SearchCpuText,
	        XmNleftAttachment, XmATTACH_FORM,
                XmNwidth, window_width - 1,
                XmNheight, 20,
                NULL);


	    SearchRangeArea = XtVaCreateManagedWidget ("action_area",
	        xmFormWidgetClass, searchWindow,
	        XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, separator12,					
	        XmNleftAttachment, XmATTACH_FORM,
                XmNwidth, window_width-1,
                XmNheight, 30,
	        NULL);


	    tempString = XmStringCreateSimple("Search: ");
	    SearchRangeLabel = XtVaCreateManagedWidget ("Search",
	        xmLabelWidgetClass, SearchRangeArea,
                XmNlabelString, tempString,
	        XmNx, 2,
	        XmNy, 4,
	        NULL);
	    XmStringFree(tempString);


	    tempString = XmStringCreateSimple ("through all events");
	    AllSearchButton = XtVaCreateManagedWidget ("all",
                xmToggleButtonWidgetClass, SearchRangeArea,
                XmNlabelString, tempString,
                XmNindicatorType, XmONE_OF_MANY,
                XmNset, current_search_mode ? False : True,
                XmNx, 65,
	        XmNy, 0,
                NULL);
	    XmStringFree(tempString);
	    XtAddCallback (AllSearchButton, XmNarmCallback, all_search_CB, NULL);
	    XtAddCallback (AllSearchButton, XmNdisarmCallback, all_search_disarm_CB, NULL);
	    
	    tempString = XmStringCreateSimple ("in this major only");
	    MajSearchButton = XtVaCreateManagedWidget ("maj",
                xmToggleButtonWidgetClass, SearchRangeArea,
                XmNlabelString, tempString,
                XmNindicatorType, XmONE_OF_MANY,
                XmNset, current_search_mode ? True : False,
                XmNx, 220,
	        XmNy, 0,
                NULL);
	    XmStringFree(tempString);
	    XtAddCallback (MajSearchButton, XmNarmCallback, maj_search_CB, NULL);
	    XtAddCallback (MajSearchButton, XmNdisarmCallback, maj_search_disarm_CB, NULL);

	    separator2 = XtVaCreateManagedWidget ("sep2",
	        xmSeparatorWidgetClass, searchWindow,
	        XmNtopAttachment, XmATTACH_WIDGET,
	        XmNorientation, XmHORIZONTAL,
                XmCSeparatorType, XmSHADOW_ETCHED_IN,
	        XmNtopWidget, SearchRangeArea,
	        XmNleftAttachment, XmATTACH_FORM,
                XmNheight, 20,
                XmNwidth, window_width - 1,
                NULL);
	}
	else {
	    separator2 = XtVaCreateManagedWidget ("Event name:",
	        xmSeparatorWidgetClass, searchWindow,
	        XmNtopAttachment, XmATTACH_WIDGET,
	        XmNorientation, XmHORIZONTAL,
                XmCSeparatorType, XmSHADOW_ETCHED_IN,
	        XmNtopWidget, SearchCpuText,
	        XmNleftAttachment, XmATTACH_FORM,
                XmNheight, 20,
                XmNwidth, window_width - 1,
                NULL);
	}

	ButtonArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, searchWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_FORM,
	    NULL);


	tempString = XmStringCreateSimple ("Search Forward");
	searchButton = XtVaCreateManagedWidget ("search",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_FORM,
	    /*XmNleftPosition, 4,*/
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (searchButton, XmNactivateCallback, Forward_Search_CB, NULL);

	tempString = XmStringCreateSimple ("Reverse Search");
	reverseButton = XtVaCreateManagedWidget ("reverse",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, searchButton,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (reverseButton, XmNactivateCallback, Reverse_Search_CB, NULL);

	tempString = XmStringCreateSimple ("Done");
	doneButton = XtVaCreateManagedWidget ("done",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, reverseButton,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (doneButton, XmNactivateCallback, close_search_CB, NULL);
  
	separator3 = XtVaCreateManagedWidget ("Event name:",
	        xmSeparatorWidgetClass, searchWindow,
	        XmNtopAttachment, XmATTACH_WIDGET,
	        XmNorientation, XmHORIZONTAL,
                XmCSeparatorType, XmSHADOW_ETCHED_IN,
	        XmNtopWidget, ButtonArea,
	        XmNleftAttachment, XmATTACH_FORM,
                XmNheight, 2,
                XmNwidth, window_width - 1,
                NULL);
    }
    XtManageChild(searchWindow);
    current_search_on = ON;
}



void 
close_goto_frame_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(GotoFrameWindow);

}


void 
stop_menu_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(StopMenu);
    XtUnmanageChild(StopMenuSep);
    glob_getting_data = FALSE;
}

/*  brings up window that queires user on the event they want to search for */
void
goto_frame_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    static int varset = 0;
    static Widget gotoDialog;
    Atom wm_delete_window;

    Widget ButtonArea, gotoButton, gotodoneButton, doneButton;
    static Widget cpuLabel, numberLabel;
    Widget separator0, separator1, separator2;
    XmString tempString;


    if (!varset) {
	varset++;


	gotoDialog = XtVaCreateWidget ("Goto Frame Number",
	    xmDialogShellWidgetClass, toplevel,
	    XmNtitle, "Goto Frame Number",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(gotoDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (gotoDialog, wm_delete_window, 
				 close_goto_frame_CB, NULL);


	GotoFrameWindow = XtVaCreateWidget ("goto",
	    xmFormWidgetClass, gotoDialog,
	    NULL);



	separator0 = XtVaCreateManagedWidget ("sep0",
	    xmSeparatorWidgetClass, GotoFrameWindow,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
            XmNheight, 20,
            NULL);

	numberLabel = XtVaCreateManagedWidget ("Frame Number: ",
	    xmLabelWidgetClass, GotoFrameWindow,
	    XmNx, 2,
	    XmNy, 25,
	    NULL);


	GotoNumberText = XtVaCreateManagedWidget ("number text",
	    xmTextFieldWidgetClass, GotoFrameWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator0,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, numberLabel,
            XmNwidth, 90,
            XmNvalue, "1",
	    NULL);


	separator1 = XtVaCreateManagedWidget ("sep1",
	    xmSeparatorWidgetClass, GotoFrameWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmSHADOW_ETCHED_IN,
	    XmNtopWidget, GotoNumberText,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
            XmNheight, 20,
            NULL);
  
	cpuLabel = XtVaCreateManagedWidget ("On cpu number: ",
	    xmLabelWidgetClass, GotoFrameWindow,
	    XmNx, 2,
	    XmNy, 78,
	    NULL);

	GotoCpuText = XtVaCreateManagedWidget ("goto cpu text",
	    xmTextFieldWidgetClass, GotoFrameWindow,
            XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator1,
            XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, cpuLabel,
            XmNwidth, 80,
            XmNvalue, "1",
	    NULL);

	separator2 = XtVaCreateManagedWidget ("sep2",
	    xmSeparatorWidgetClass, GotoFrameWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmSHADOW_ETCHED_IN,
	    XmNtopWidget, GotoCpuText,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
            XmNheight, 20,
            NULL);


	ButtonArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, GotoFrameWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_FORM,
	    NULL);


	tempString = XmStringCreateSimple ("Goto");
	gotoButton = XtVaCreateManagedWidget ("goto",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (gotoButton, XmNactivateCallback, goto_frame_number_CB, NULL);

	tempString = XmStringCreateSimple ("Goto & Done");
	gotodoneButton = XtVaCreateManagedWidget ("gotodone",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, gotoButton,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (gotodoneButton, XmNactivateCallback, goto_frame_done_CB, NULL);

	tempString = XmStringCreateSimple ("Done");
	doneButton = XtVaCreateManagedWidget ("done",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, gotodoneButton,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (doneButton, XmNactivateCallback, close_goto_frame_CB, NULL);


    }
    XtManageChild(GotoFrameWindow);
}


void 
ignore_define_template_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(DefineTemplateWindow);
    defining_template = -1;

}


void 
acceptnew_define_template_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int i;
    char msg_str[64];


    major_frame_account[define_template_cpu].template_processes =
	atoi(XmTextFieldGetString(TemplateProcText));
    if (major_frame_account[define_template_cpu].template_processes >
	major_frame_account[define_template_cpu].numb_processes) {
	sprintf(msg_str, "there are not that many processes in a major frame on cpu %d",
	       define_template_cpu);
	grtmon_message(msg_str, ERROR_MSG, 10);
	major_frame_account[define_template_cpu].template_processes =
	    major_frame_account[define_template_cpu].numb_processes;
    }

    major_frame_account[define_template_cpu].template_minors =
	atoi(XmTextFieldGetString(TemplateMinorText));
    if (major_frame_account[define_template_cpu].template_minors >
	major_frame_account[define_template_cpu].numb_minors) {
	sprintf(msg_str, "there are not that many minors in a major frame on cpu %d",
	       define_template_cpu);
	grtmon_message(msg_str, ERROR_MSG, 11);
	major_frame_account[define_template_cpu].template_minors =
	    major_frame_account[define_template_cpu].numb_minors;
    }


    for (i=0; i<major_frame_account[define_template_cpu].template_minors; i++) {
	major_frame_account[define_template_cpu].template_in_minor[i] = 
	    atoi(XmTextFieldGetString(TemplateInMinorText[i]));
	if (major_frame_account[define_template_cpu].template_in_minor[i] >
	    major_frame_account[define_template_cpu].numb_in_minor[i]) {
	    sprintf(msg_str, "there are not that many processes in minor %d for major frame on cpu %d",
		   i, define_template_cpu);
	    grtmon_message(msg_str, ERROR_MSG, 12);
	    major_frame_account[define_template_cpu].template_in_minor[i] =
		major_frame_account[define_template_cpu].numb_in_minor[i];
	}
    }
    XtUnmanageChild(DefineTemplateWindow);
    defining_template = -2;

}


void 
ignoreall_define_template_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    
    template_matching = FALSE;
    XtUnmanageChild(DefineTemplateWindow);
    defining_template = -3;
}


void 
close_define_template_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(DefineTemplateWindow);
}


/*  brings up window that queires user on the event they want to search for */
void
define_template_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int cpu, frame;

    if ((defining_template == 1) || (defining_template_cpu >= 0))
	return;
    cpu = 1;
    frame = maj_frame_numb[cpu];

    define_template(cpu, frame);


}

#define WIDGET_SPACING 33
#define VAR_LABEL_START 140

#define MAKE_LABEL_WIDGETS(index) \
            old_in_minorLabel[index] = XtVaCreateManagedWidget ("numb", \
	        xmLabelWidgetClass, DefineTemplateWindow, \
	        XmNx, 2, \
	        XmNy, VAR_LABEL_START + (i+2)*WIDGET_SPACING, \
	        NULL); \
	    new_in_minorLabel[index] = XtVaCreateManagedWidget ("numb", \
	        xmLabelWidgetClass, DefineTemplateWindow, \
	        XmNx, NEW_COL, \
	        XmNy, VAR_LABEL_START + (i+2)*WIDGET_SPACING, \
	        NULL); \
	    TemplateInMinorText[index] = XtVaCreateManagedWidget ("templateinminor", \
	        xmTextFieldWidgetClass, DefineTemplateWindow, \
	        XmNleftAttachment, XmATTACH_WIDGET, \
	        XmNleftWidget, new_in_minorLabel[index], \
	        XmNy, VAR_LABEL_START-5 + (i+2)*WIDGET_SPACING, \
                XmNwidth, 34, \
                XmNvalue, "1", \
	        NULL);

/*  brings up window that queires user on the event they want to search for */
void
define_template(int cpu , int frame)
{
#define NEW_COL 265
    static int varset = 0;
    static Widget dtDialog;
    Atom wm_delete_window;
    int i;
    int display_in_minor;
    static Widget ButtonArea, ignoreButton, acceptnewButton, ignoreallButton;
    static Widget titleLabel, reasonLabel, reason1Label;
    static Widget oldLabel, newLabel;
    static Widget old_procLabel, new_procLabel;
    static Widget old_minorLabel, new_minorLabel;
    static Widget *old_in_minorLabel, *new_in_minorLabel;
    static int alloc_in_minor;
    Widget separator0, separator1, separator2;
    XmString tempString;
    char temp_str[128];
    char reason_str[128];
    int reasons;
    int numb_realloc;


    if (!varset) {
	varset++;


	dtDialog = XtVaCreateWidget ("Define Frame Template",
	    xmDialogShellWidgetClass, toplevel,
	    XmNtitle, "Define Frame Template",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(dtDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (dtDialog, wm_delete_window, 
				 close_define_template_CB, NULL);


	DefineTemplateWindow = XtVaCreateWidget ("define",
	    xmFormWidgetClass, dtDialog,
            XmNwidth, 522,
            XmNheight, 500,
	    NULL);

	titleLabel = XtVaCreateManagedWidget ("Hi",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, 2,
	    XmNy, 15,
	    NULL);

	reasonLabel = XtVaCreateManagedWidget ("hi",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, 2,
	    XmNy, 40,
	    NULL);

	reason1Label = XtVaCreateManagedWidget ("hi",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, 10,
	    XmNy, 60,
	    NULL);

	separator0 = XtVaCreateManagedWidget ("sep0",
	    xmSeparatorWidgetClass, DefineTemplateWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, reason1Label,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
            XmNheight, 20,
            NULL);

	oldLabel = XtVaCreateManagedWidget ("Old Template Values",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, 50,
	    XmNy, 100,
	    NULL);

	newLabel = XtVaCreateManagedWidget ("New Template Values",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, 320,
	    XmNy, 100,
	    NULL);

	old_procLabel = XtVaCreateManagedWidget ("numb",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, 2,
	    XmNy, VAR_LABEL_START,
	    NULL);

	new_procLabel = XtVaCreateManagedWidget ("numb",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, NEW_COL,
	    XmNy, VAR_LABEL_START,
	    NULL);

	TemplateProcText = XtVaCreateManagedWidget ("templateproc",
	    xmTextFieldWidgetClass, DefineTemplateWindow,
	    XmNy, VAR_LABEL_START-5,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, new_procLabel,
            XmNwidth, 34,
            XmNvalue, "1",
	    NULL);
	
	old_minorLabel = XtVaCreateManagedWidget ("numb",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, 2,
	    XmNy, VAR_LABEL_START+WIDGET_SPACING,
	    NULL);

	new_minorLabel = XtVaCreateManagedWidget ("numb",
	    xmLabelWidgetClass, DefineTemplateWindow,
	    XmNx, NEW_COL,
	    XmNy, VAR_LABEL_START+WIDGET_SPACING,
	    NULL);

	TemplateMinorText = XtVaCreateManagedWidget ("templatemionr",
	    xmTextFieldWidgetClass, DefineTemplateWindow,
	    XmNy, VAR_LABEL_START+WIDGET_SPACING-5,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, new_minorLabel,
            XmNwidth, 34,
            XmNvalue, "1",
	    NULL);

	
	/* we'll arbitrarily start of allocating space for 8 processes
	   in a minor and then realloc if we need more */
	old_in_minorLabel = ((Widget *) malloc(8*sizeof(Widget)));
	new_in_minorLabel = ((Widget *) malloc(8*sizeof(Widget)));
	TemplateInMinorText = ((Widget *) malloc(8*sizeof(Widget)));

	def_temp_numb_in_old_minor = 8;
	def_temp_numb_in_new_minor = 8;
	alloc_in_minor = 8;

	for (i=0; i<8; i++) {
	    MAKE_LABEL_WIDGETS(i)
	}

	ButtonArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, DefineTemplateWindow,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    NULL);

	separator1 = XtVaCreateManagedWidget ("sep1",
	    xmSeparatorWidgetClass, DefineTemplateWindow,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, ButtonArea,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
            XmNheight, 20,
            NULL);

	separator2 = XtVaCreateManagedWidget ("sep2",
	    xmSeparatorWidgetClass, DefineTemplateWindow,
	    XmNx, 250,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator0,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, separator1,
	    XmNorientation, XmVERTICAL,
            XmCSeparatorType, XmNO_LINE,
            XmNwidth, 20,
            NULL);

	tempString = XmStringCreateSimple ("Ignore");
	ignoreButton = XtVaCreateManagedWidget ("ignore",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (ignoreButton, XmNactivateCallback, 
		       ignore_define_template_CB, NULL);

	tempString = XmStringCreateSimple ("Accept New");
	acceptnewButton = XtVaCreateManagedWidget ("acceptnew",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNx, 205,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (acceptnewButton, XmNactivateCallback, 
		       acceptnew_define_template_CB, NULL);

	tempString = XmStringCreateSimple ("Ignore All");
	ignoreallButton = XtVaCreateManagedWidget ("ignoreall",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNx, 422,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (ignoreallButton, XmNactivateCallback, 
		       ignoreall_define_template_CB, NULL);

    }

    if (major_frame_account[cpu].template_minors > alloc_in_minor) {
	/* we need to realloc more space since */
	numb_realloc = major_frame_account[cpu].template_minors - 
	    alloc_in_minor + 4;

	old_in_minorLabel =((Widget *) realloc((void *)old_in_minorLabel,
				    (alloc_in_minor+numb_realloc)*sizeof(Widget)));
	new_in_minorLabel =((Widget *) realloc((void *)new_in_minorLabel,
				     (alloc_in_minor+numb_realloc)*sizeof(Widget)));
	TemplateInMinorText =((Widget *) realloc((void *)TemplateInMinorText,
				     (alloc_in_minor+numb_realloc)*sizeof(Widget)));

	for (i=alloc_in_minor; i<alloc_in_minor+numb_realloc; i++) {
	    MAKE_LABEL_WIDGETS(i)
	}

	alloc_in_minor = alloc_in_minor + numb_realloc;

    }

    /* set the label for the title */ 
    sprintf(temp_str, "Major frame %d on cpu %d timeoffset %d",
	    frame, cpu, 
	    TIC2USEC(maj_frame_block[cpu][frame].time - absolute_start_time));
    tempString = XmStringCreateSimple(temp_str);
    XtVaSetValues(titleLabel, XmNlabelString, tempString, NULL);
    XmStringFree(tempString);


    /* set the labels for the reason */ 
    reasons = 0;
    strcpy(reason_str, "");

    if (major_frame_account[cpu].template_processes !=
	major_frame_account[cpu].numb_processes) {
	strcpy(reason_str, "Unequal number of processes in major");
	reasons++;
    }
    if (major_frame_account[cpu].template_minors !=
	major_frame_account[cpu].numb_minors) {
	if (strlen(reason_str) > 1) {
	    strcat(reason_str, ", Unequal number of minors in major");
	}
	else {
	    strcpy(reason_str, "Unequal number of minors in major");
	}
	reasons++;
    }
    for (i=0; i<major_frame_account[cpu].numb_minors;i++) {
	if (major_frame_account[cpu].template_in_minor[i] !=
	    major_frame_account[cpu].numb_in_minor[i]) {
	    if (strlen(reason_str) > 1) {
		sprintf(temp_str, ", Unequal number of processes in minor %d", i);
		strcat(reason_str, temp_str);
	    }
	    else {
		sprintf(temp_str, "Unequal number of processes in minor %d", i);
		strcpy(reason_str, temp_str);
	    }
	    reasons++;
	}
    }
    if (reasons == 0) {
	strcpy(reason_str, "Called from menu");
    }
    tempString = XmStringCreateSimple(reason_str);
    XtVaSetValues(reason1Label, XmNlabelString, tempString, NULL);
    XmStringFree(tempString);
    

    if (reasons <= 1) {
	sprintf(temp_str, "Reason for template definition:");
    }
    else {
	sprintf(temp_str, "Reasons for template definition:");
    }
    tempString = XmStringCreateSimple(temp_str);
    XtVaSetValues(reasonLabel, XmNlabelString, tempString, NULL);
    XmStringFree(tempString);
    

    /* set the label for the old_proc */ 
    sprintf(temp_str, "Number processes in major: %d",
	    major_frame_account[cpu].template_processes);
    tempString = XmStringCreateSimple(temp_str);
    XtVaSetValues(old_procLabel, XmNlabelString, tempString, NULL);
    XmStringFree(tempString);

    /* set the label for the new_proc */ 
    sprintf(temp_str, "Number processes in major: ");
    tempString = XmStringCreateSimple(temp_str);
    XtVaSetValues(new_procLabel, XmNlabelString, tempString, NULL);
    XmStringFree(tempString);
    sprintf(temp_str, "%d", major_frame_account[cpu].numb_processes);
    XtVaSetValues(TemplateProcText, XmNvalue, temp_str, NULL);

    /* set the label for the old_minor */ 
    sprintf(temp_str, "Number minors in major: %d",
	    major_frame_account[cpu].template_minors);
    tempString = XmStringCreateSimple(temp_str);
    XtVaSetValues(old_minorLabel, XmNlabelString, tempString, NULL);
    XmStringFree(tempString);

    /* set the label for the new_minor */ 
    sprintf(temp_str, "Number minors in major: ");
    tempString = XmStringCreateSimple(temp_str);
    XtVaSetValues(new_minorLabel, XmNlabelString, tempString, NULL);
    XmStringFree(tempString);
    sprintf(temp_str, "%d", major_frame_account[cpu].numb_minors);
    XtVaSetValues(TemplateMinorText, XmNvalue, temp_str, NULL);



    if (def_temp_numb_in_old_minor > major_frame_account[cpu].template_minors) {
	for (i=major_frame_account[cpu].template_minors;
	     i<def_temp_numb_in_old_minor; i++) {
	    XtUnmanageChild(old_in_minorLabel[i]);
	}
    }
    else {
	for (i=def_temp_numb_in_old_minor;
	     i<major_frame_account[cpu].template_minors; i++) {
	    XtManageChild(old_in_minorLabel[i]);
	}
    }
    for (i=0;i<def_temp_numb_in_old_minor;i++) {
	sprintf(temp_str, "Number processes in minor %d: %d", i,
		major_frame_account[cpu].template_in_minor[i]);
	tempString = XmStringCreateSimple(temp_str);
	XtVaSetValues(old_in_minorLabel[i], XmNlabelString, tempString, NULL);
	XmStringFree(tempString);

  
    }
    def_temp_numb_in_old_minor = major_frame_account[cpu].template_minors;



    if (def_temp_numb_in_new_minor > major_frame_account[cpu].numb_minors) {
	for (i=major_frame_account[cpu].numb_minors;
	     i<def_temp_numb_in_new_minor; i++) {
	    XtUnmanageChild(new_in_minorLabel[i]);
	    XtUnmanageChild(TemplateInMinorText[i]);
	}
    }
    else {
	for (i=def_temp_numb_in_new_minor;
	     i<major_frame_account[cpu].numb_minors; i++) {
	    XtManageChild(new_in_minorLabel[i]);
	    XtManageChild(TemplateInMinorText[i]);
	}
    }
    for (i=0;i<def_temp_numb_in_new_minor;i++) {
	sprintf(temp_str, "Number processes in minor %d: ", i);
	tempString = XmStringCreateSimple(temp_str);
	XtVaSetValues(new_in_minorLabel[i], XmNlabelString, tempString, NULL);
	XmStringFree(tempString);
	sprintf(temp_str, "%d", major_frame_account[cpu].numb_in_minor[i]);
	XtVaSetValues(TemplateInMinorText[i], XmNvalue, temp_str, NULL);
    }
    def_temp_numb_in_new_minor = major_frame_account[cpu].numb_minors;


    if (def_temp_numb_in_old_minor >= def_temp_numb_in_new_minor)
	display_in_minor = def_temp_numb_in_old_minor;
    else
	display_in_minor = def_temp_numb_in_new_minor;

    XtVaSetValues(DefineTemplateWindow, XmNheight, 
		   268+WIDGET_SPACING*display_in_minor, NULL);

    define_template_cpu = cpu;

    XtManageChild(DefineTemplateWindow);
}




void
FileFullCB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(FileSysFull);

}


void
file_sys_full_error()
{
    int n;
    Arg args[20];
    Widget button;
    static int varset = 0;

    XmString title,message;


    if (!varset) {
	varset = 1;

	title = XmStringCreateLtoR ("file error", XmSTRING_DEFAULT_CHARSET);
	message = XmStringCreateLtoR ("Logging Stopped - file system full?",
				      XmSTRING_DEFAULT_CHARSET);
	
	n=0;
	XtSetArg (args[n], XmNdefaultButtonType, XmDIALOG_OK_BUTTON); n++;
	XtSetArg (args[n], XmNdialogTitle, title);  n++;
	XtSetArg (args[n], XmNmessageString, message);  n++;
	

	FileSysFull = XmCreateWarningDialog (mainForm, "file full", args, n);

	button = XmMessageBoxGetChild (FileSysFull, XmDIALOG_CANCEL_BUTTON);
	XtUnmanageChild (button);
	button = XmMessageBoxGetChild (FileSysFull, XmDIALOG_HELP_BUTTON);
	XtUnmanageChild (button);

	XtManageChild(FileSysFull);

	XtAddCallback (FileSysFull, XmNokCallback, FileFullCB, NULL);
	XmStringFree(title);
	XmStringFree(message);
    }

}

void
get_search_params(char *search_str)
{
    Boolean is_set;
    int cpu;

    if (no_frame_mode)
	is_set = True;
    else
	XtVaGetValues(AllSearchButton, XmNset, &is_set, NULL);
    cpu = atoi(XmTextFieldGetString(SearchCpuText));

    strcpy(search_str, XmTextFieldGetString(SearchEventText));

    if ((int)is_set == 1) {
	if (current_search_cpu != cpu) {
	    if (no_frame_mode) {
		current_search_major = 0;
		current_search_index = 0;
	    }
	    else {
		current_search_major = 1;
		current_search_index = maj_frame_block[cpu][1].nf_ex_ev_count+1;
	    }
	    goto_frame_number(current_search_major, cpu);
	}
	current_search_mode = SEARCH_ALL;
    }
    else {
	current_search_mode = SEARCH_MAJ;
	
	if (current_search_cpu != cpu) {
	    current_search_major = maj_frame_numb[cpu];
	    current_search_index = maj_frame_block[cpu][current_search_major].nf_ex_ev_count;
	    goto_frame_number(current_search_major, cpu);
	}
    }
    current_search_cpu = cpu;
}

void 
Reverse_Search_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    char search_str[64];
    int found;
    int cpu;
    float start_val;



    if (! current_search_on)
	search_event_CB(NULL, NULL, NULL);

    get_search_params(search_str);

    cpu = current_search_cpu;
    found = 0;

    while (current_search_index > 0) {
	current_search_index--;
	if ((graph_info[cpu].no_frame_event[current_search_index].evt == 
	    TSTAMP_EV_START_OF_MAJOR) &&
	    (! no_frame_mode)) {
	    if (current_search_mode == SEARCH_ALL) {
		current_search_major--;
		if (current_search_major < 1) {
		    current_search_major = numb_maj_frames[cpu]-2;
		    goto_frame_number(current_search_major, cpu);
		    /* goto frame clears current_search_cpu */
		    current_search_cpu = cpu;
		    break;
		}
		goto_frame_number(current_search_major, cpu);
		/* goto frame clears current_search_cpu */
		current_search_cpu = cpu;
	    }
	    else {
		break;
	    }
	}
	if (strcmp(search_str, tstamp_event_name[graph_info[cpu].no_frame_event[current_search_index].evt]) == 0){
	    search_line = graph_info[cpu].no_frame_event[current_search_index].start_time;
	    if (no_frame_mode) {
		if ((search_line < nf_start_val_a) || 
		    (search_line > nf_start_val_a + nf_width_a)) {
		    nf_start_val_a = search_line - (3*nf_width_a)/4;
		    readjust_nf_start_and_end(0);
		}
	    }
	    else {
		search_line = search_line - graph_info[cpu].maj_absolute_start;

		start_val = (float)start_val_a;
		
		if ((search_line < start_val) || 
		    (search_line > start_val + width_a)) {
		    start_val_a = search_line - width_a/4;
		    if (start_val_a < 0)
			start_val_a = 0;
		    set_slider_a_width_and_loc();
		}
	    }
	    current_search_found = TRUE;
	    found = 1;
	    break;
	}
    }

    if (! found) {
	current_search_found = FALSE;
	if (current_search_mode == SEARCH_ALL) {
	    grtmon_message("no more events in stream - reset to end", ALERT_MSG, 105);
	    if (no_frame_mode) {
		current_search_index = graph_info[cpu].no_frame_event_count;
		nf_start_val_a = nf_max_val_a - nf_width_a;
		readjust_nf_start_and_end(0);
	    }
	    else {
		current_search_index = maj_frame_block[cpu][numb_maj_frames[cpu]-1].nf_ex_ev_count - 1;
	    }
	}
	else {
	    grtmon_message("no more events in stream - reset to end", ALERT_MSG, 106);
	    start_val_a = max_val_a - width_a;
	    set_slider_a_width_and_loc();
	    current_search_index = maj_frame_block[cpu][current_search_major+1].nf_ex_ev_count - 1;
	}
    }

}


void 
Forward_Search_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    char search_str[64];
    int found;
    int cpu;
    float start_val;

    if (! current_search_on)
	search_event_CB(NULL, NULL, NULL);
    get_search_params(search_str);
    found = 0;
    cpu = current_search_cpu;


    while (current_search_index < graph_info[cpu].no_frame_event_count) {
	current_search_index++;
	if ((graph_info[cpu].no_frame_event[current_search_index].evt == 
	    TSTAMP_EV_START_OF_MAJOR) &&
	    (! no_frame_mode)) {
	    if (current_search_mode == SEARCH_ALL) {
		current_search_major++;
		if (current_search_major > numb_maj_frames[cpu] - 2) {
		    current_search_major = 1;
		    goto_frame_number(current_search_major, cpu);
		    /* goto frame clears current_search_cpu */
		    current_search_cpu = cpu;
		    break;
		}
		goto_frame_number(current_search_major, cpu);
		/* goto frame clears current_search_cpu */
		current_search_cpu = cpu;
	    }
	    else {
		break;
	    }
	}
	if (strcmp(search_str, tstamp_event_name[graph_info[cpu].no_frame_event[current_search_index].evt]) == 0){
	    search_line = graph_info[cpu].no_frame_event[current_search_index].start_time;
	    if (no_frame_mode) {
		if ((search_line < nf_start_val_a) || 
		    (search_line > nf_start_val_a + nf_width_a)) {
		    nf_start_val_a = search_line - nf_width_a/4;
		    readjust_nf_start_and_end(0);
		}
	    }
	    else {
		search_line = search_line - graph_info[cpu].maj_absolute_start;
		if ((search_line < start_val_a) || 
		    (search_line > start_val_a + width_a)) {
		    start_val_a = search_line - width_a/4;
		    if (start_val_a < 0)
			start_val_a = 0;
		    if (start_val_a + width_a > max_val_a)
			start_val_a = max_val_a - width_a;
		    set_slider_a_width_and_loc();

		}
	    }
	    current_search_found = TRUE;
	
	    found = 1;
	    break;
	}
    }


    if (! found) {
	current_search_found = FALSE;
	if (current_search_mode == SEARCH_ALL) {
	    grtmon_message("no more events in stream - reset to start", ALERT_MSG, 104);
	    if (no_frame_mode) {
		current_search_index = 0;
		nf_start_val_a = 0;
		readjust_nf_start_and_end(0);
	    }
	    else {
		current_search_index = maj_frame_block[cpu][1].nf_ex_ev_count+1;


		start_val = (float)start_val_a;
		
		if ((search_line < start_val) || 
		    (search_line > start_val + width_a)) {
		    start_val_a = search_line - width_a/4;
		    if (start_val_a < 0)
			start_val_a = 0;
		    set_slider_a_width_and_loc();
		}
	    }
	}
	else {
	    grtmon_message("no more events in this major - reset to start", ALERT_MSG, 103);
	    current_search_index = maj_frame_block[cpu][current_search_major].nf_ex_ev_count;
	}
    }

}



void
scan_file_next_CB()
{
    int i,cpu;

    if (scan_type == FILE_DATA) {
	if (frame_sync == AS_IS) {
	    for (i=0; i<MAX_PROCESSORS; i++) {
		if (cpu_on[i]) {
		    if ((maj_frame_numb[i] + 1) < (numb_maj_frames[i] - 1))
			goto_frame_number(maj_frame_numb[i] + 1, i);
		}
	    }
	}
	else {
	    i=0;
	    while (i<MAX_PROCESSORS) {
		if (cpu_on[i]) {
		    if ((maj_frame_numb[i] + 1) < (numb_maj_frames[i] - 1))
			goto_frame_number(maj_frame_numb[i] + 1, i);
		    break;
		}
		i++;
	    }
	}
    }
}


void
scan_file_prev_CB()
{
    int i,cpu;

    if (scan_type == FILE_DATA) {
	if (frame_sync == AS_IS) {
	    for (i=0; i<MAX_PROCESSORS; i++) {
		if (cpu_on[i]) {
		    if (maj_frame_numb[i] > FIRST_MAJOR_FRAME)
			goto_frame_number(maj_frame_numb[i] - 1, i);
		}
	    }
	}
	else {
	    i=0;
	    while (i<MAX_PROCESSORS) {
		if (cpu_on[i]) {
		    if (maj_frame_numb[i] > FIRST_MAJOR_FRAME)
			goto_frame_number(maj_frame_numb[i] - 1, i);
		    break;
		}
		i++;
	    }
	}
    }
}


void
next_cpu_frame_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int cpu,i;

    if (scan_type == FILE_DATA) {

	cpu = gr_cpu_map[(int)clientD];

	if ((maj_frame_numb[cpu] + 1) < (numb_maj_frames[cpu] - 1)) {
	    goto_frame_number(maj_frame_numb[cpu] + 1, cpu);
	}

    }

}


void
prev_cpu_frame_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int cpu,i;

    if (scan_type == FILE_DATA) {

	cpu = gr_cpu_map[(int)clientD];

	if (maj_frame_numb[cpu] > FIRST_MAJOR_FRAME) {
	    goto_frame_number(maj_frame_numb[cpu] - 1, cpu);
	}

    }

}


void
obtain_goto_frame_number(int *cpu, int *frame_number)
{
    int i;

    i = 0;
    while (i<MAX_PROCESSORS) {
	if (cpu_on[i]) {
	    *cpu = i;
	    break;
	}
	i++;
    }

    *frame_number = FIRST_MAJOR_FRAME;

    if (GotoFrameWindow != NULL) {
	*cpu = atoi(XmTextFieldGetString(GotoCpuText));
	*frame_number = atoi(XmTextFieldGetString(GotoNumberText));
    }
}

void
goto_frame_number_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int cpu, frame_number;

    obtain_goto_frame_number(&cpu, &frame_number);

    goto_frame_number(frame_number, cpu);

}


void
goto_frame_done_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int cpu, frame_number;


    obtain_goto_frame_number(&cpu, &frame_number);

    XtUnmanageChild(GotoFrameWindow);

    goto_frame_number(frame_number, cpu);


}

void
goto_frame_number(int number, int cpu)
{
    merge_event_t mq;
    int i;
    int frame_numb[MAX_PROCESSORS];
    char msg_str[64];


    if ((live_stream) && (! logging)) {
	grtmon_message("since you are not logging you can't goto previous frames", ERROR_MSG, 16);
	return;
    }
    if (! cpu_on[cpu]) {
	sprintf(msg_str, "invalid cpu %d",cpu);
	grtmon_message(msg_str, ERROR_MSG, 17);
	return;
    }
    if (number > numb_maj_frames[cpu]) {
	sprintf(msg_str, "there are not that many frames on cpu %d",cpu);
	grtmon_message(msg_str, ERROR_MSG, 18);
	return;
    }
    
    switch_to_file_scan_type();

    clear_event_list();
    erase_event_list = FALSE;

    if (cpu == current_search_cpu) {
	/* indicate our search indices are no longer valid */
	current_search_cpu = -1;
	current_search_found = FALSE;
    }

    if (frame_sync == AS_IS) {
	for (i=0; i<MAX_PROCESSORS; i++) {
	    if (cpu_on[i]) {
		if (cpu!=i)
		    read_and_stat_maj_frame(maj_frame_numb[i], i);
		else
		    read_and_stat_maj_frame(number, cpu);
	    }
	}
    }
    else {
	for (i=0; i<MAX_PROCESSORS; i++) {
	    if (cpu_on[i]) {
		if (cpu!=i) {
		    read_and_stat_maj_frame(maj_frame_in_time_sync(cpu, number, i), i);
		}
		else
		    read_and_stat_maj_frame(number, cpu);
	    }	
	}
    }
}


/* a binary search to find the major frame on my_cpu starting 
   immediately after the major_frame sync_frame on sync_cpu */
int
maj_frame_in_time_sync(int sync_cpu, int sync_frame, int my_cpu)
{
    long long time_val;
    int start_index, end_index;

    time_val = maj_frame_block[sync_cpu][sync_frame].time;

    start_index = 1;
    end_index = numb_maj_frames[my_cpu] - 2;

    while ((end_index - start_index) > 1) {
	if (maj_frame_block[my_cpu][(end_index + start_index)/2].time < time_val) {
	    start_index = (end_index + start_index)/2;
	}
	else {
	    end_index = (end_index + start_index)/2;
	}
	
    }
    if (maj_frame_block[my_cpu][start_index].time == time_val) 
	return(start_index);
    else
	return(end_index);
}


void
switch_to_file_scan_type()
{
    int i;

    if (scan_type != FILE_DATA) {

	clear_stats_and_menus();
	
	text_display_on = FALSE;

	XtVaSetValues (file_real_val[REAL_DATA],
		       XmNset, False, NULL);
	
	scan_type = FILE_DATA;  /* set this as late as possible to 
				   shrink timing window */
	
	XtVaSetValues (file_real_val[FILE_DATA],
		       XmNset, True, NULL);
    }
}




void 
close_search_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    current_search_on = OFF;
    XtUnmanageChild(searchWindow);

}



void 
reset_all_maxes()
{
    int i,cpu,j;

    reset_graph_maxes();
    for (i=0;i<gr_numb_cpus;i++) {
	cpu = gr_cpu_map[i];
	for (j=0;j<major_frame_account[cpu].numb_minors;j++) {
	    graph_info[cpu].max_minor[j].sum_val = 0;
	    graph_info[cpu].max_minor[j].ex_ev_val = 0;
	    graph_info[cpu].max_kern_minor[j].sum_val = 0;
	    graph_info[cpu].max_kern_minor[j].ex_ev_val = 0;
	}
	graph_info[cpu].maj_ave_process[0].max_time = INF_TIME;
	for (j=1;j<major_frame_account[cpu].numb_processes;j++) {
	    graph_info[cpu].maj_ave_process[j].max_time = 0;
	}
    }
}

void
reset_maxes_CB()
{

    reset_all_maxes();

}


void
reset_graph_maxes()
{
    int i,cpu,j;

    for (i=0;i<gr_numb_cpus;i++) {
	cpu = gr_cpu_map[i];
	for (j=0;j<major_frame_account[cpu].numb_minors;j++) {
	    graph_info[cpu].max_minor[j].sum_val = 0;
	    graph_info[cpu].max_minor[j].ex_ev_val = 0;
	    graph_info[cpu].max_kern_minor[j].sum_val = 0;
	    graph_info[cpu].max_kern_minor[j].ex_ev_val = 0;
	}
	graph_info[cpu].maj_ave_process[0].max_time = INF_TIME;
	for (j=1;j<major_frame_account[cpu].numb_processes;j++) {
	    graph_info[cpu].maj_ave_process[j].max_time = 0;
	}
    }
}


void 
exposeCB (Widget w, XtPointer clientD, XtPointer callD)
{

    drawGraphs (callD);
}


void
quit_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    exit_frameview();
} 


void
enableCB (Widget w, XtPointer cpu_num, XtPointer callD)
{
    register int i;
    XmToggleButtonCallbackStruct *call_data = 
			(XmToggleButtonCallbackStruct *) callD;
    if ((int)cpu_num == -1) 
	for (i=0; i < gr_numb_cpus; i++) {
	    cpuOn[i] = call_data->set;
	    XtVaSetValues (enable[i], XmNset, True, NULL);
    } else
	cpuOn[(int)cpu_num] = call_data->set;
}


void 
reset_maj_ave_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    register int cpu;

    for (cpu=0;cpu<gr_numb_cpus;cpu++) {
	XtVaSetValues (maj_ave_menu[cpu][maj_ave_last[cpu]],
		       XmNset, False, NULL);
	maj_ave_last[cpu] = 1;
	XtVaSetValues (maj_ave_menu[cpu][maj_ave_last[cpu]],
		       XmNset, True, NULL);
	rtmon_set_proc_ave_factor(gr_cpu_map[cpu],1);
    }


}


void
maj_ave_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int ave_num;
    int cpu;


    ave_num = ((int)clientD)%1000;
    cpu = ((int)clientD)/1000;


    XtVaSetValues (maj_ave_menu[cpu][maj_ave_last[cpu]],
		   XmNset, False, NULL);
    maj_ave_last[cpu] = ave_num;
    XtVaSetValues (maj_ave_menu[cpu][maj_ave_last[cpu]],
		   XmNset, True, NULL);
    rtmon_set_proc_ave_factor(gr_cpu_map[cpu],ave_num);

}



void
unit_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtVaSetValues (unitVal[GraphA_label_type],
		   XmNset, False, NULL);
    GraphA_label_type = (int)clientD;
    XtVaSetValues (unitVal[GraphA_label_type],
		   XmNset, True, NULL);
}


/* see TIMESTAMP_ADJ_COMMENT in frameview.h
void
adjust_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtVaSetValues (adjust_val[adjust_mode],
		   XmNset, False, NULL);
    reset_all_maxes();
    adjust_mode = (int)clientD;
    XtVaSetValues (adjust_val[adjust_mode],
		   XmNset, True, NULL);
}
*/

void
frame_display_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtVaSetValues (frame_display_val[display_frame_type],
		   XmNset, False, NULL);
    display_frame_type = (int)clientD;
    XtVaSetValues (frame_display_val[display_frame_type],
		   XmNset, True, NULL);
}


void
graph_mode_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtVaSetValues (graph_mode_val[graph_mode],
		   XmNset, False, NULL);
    graph_mode = (int)clientD;
    XtVaSetValues (graph_mode_val[graph_mode],
		   XmNset, True, NULL);
}


void
frame_sync_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtVaSetValues (frame_sync_val[frame_sync],
		   XmNset, False, NULL);
    frame_sync = (int)clientD;
    XtVaSetValues (frame_sync_val[frame_sync],
		   XmNset, True, NULL);
}


void
file_real_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int type;
    int cpu,i;
    int old_scan_type;

    type = (int)clientD;



    if ((live_stream) && (! logging)) {
	if (type == FILE_DATA) {
	
	    grtmon_message("since you are not logging you can't goto previous frames", ERROR_MSG, 19);
	}
	XtVaSetValues (file_real_val[FILE_DATA],
		       XmNset, False, NULL);
	XtVaSetValues (file_real_val[REAL_DATA],
		       XmNset, True, NULL);

	return;
    }

    old_scan_type = scan_type;
    if (type == FILE_DATA) {
	  
	XtManageChild(NextFrameSep);
	XtManageChild(NextFrameAll);
	XtManageChild(NextFrameCpu);
	for (i=0; i < gr_numb_cpus; i++) {
	    XtManageChild(next_frame_menu[i]);
	}
	XtManageChild(PrevFrameSep);
	XtManageChild(PrevFrameAll);
	XtManageChild(PrevFrameCpu);
	for (i=0; i < gr_numb_cpus; i++) {
	    XtManageChild(prev_frame_menu[i]);
	}

	for (i=0;i<gr_numb_cpus;i++) {
	    cpu = gr_cpu_map[i];
	}

	scan_type = type;  /* set this as early as possible to 
			      shrink timing window */
    
	XtVaSetValues (file_real_val[old_scan_type],
		       XmNset, False, NULL);

	if ((type != old_scan_type) || (type == FILE_DATA)) {
	    
	    clear_stats_and_menus();
	    
	}
	
	XtVaSetValues (file_real_val[scan_type],
		       XmNset, True, NULL);

	for (i=0; i<MAX_PROCESSORS; i++) {
	    if (cpu_on[i]) {
		goto_frame_number(1, i);
	    }
	}
    }
    else {

	for (i=0; i < gr_numb_cpus; i++) {
	    XtUnmanageChild(next_frame_menu[i]);
	}
	XtUnmanageChild(NextFrameCpu);
	XtUnmanageChild(NextFrameAll);
	XtUnmanageChild(NextFrameSep);
	for (i=0; i < gr_numb_cpus; i++) {
	    XtUnmanageChild(prev_frame_menu[i]);
	}
	XtUnmanageChild(PrevFrameCpu);
	XtUnmanageChild(PrevFrameAll);
	XtUnmanageChild(PrevFrameSep);

	
	if (type != old_scan_type) {
	    clear_stats_and_menus();
	}

	text_display_on = FALSE;

	XtVaSetValues (file_real_val[old_scan_type],
		       XmNset, False, NULL);

	scan_type = type;  /* set this as late as possible to 
			      shrink timing window */

	XtVaSetValues (file_real_val[scan_type],
		       XmNset, True, NULL);

    }
}

void
clear_stats_and_menus()
{
    int i,cpu;

    /* clear statistics */
    /* reset_graph_maxes();*/

    
    for (i=0;i<gr_numb_cpus;i++) {
	cpu = gr_cpu_map[i];
	if (maj_ave_menu[i][maj_ave_last[i]] != NULL)
	    XtVaSetValues (maj_ave_menu[i][maj_ave_last[i]],
			   XmNset, False, NULL);
	maj_ave_last[i] = 1;
	if (maj_ave_menu[i][maj_ave_last[i]] != NULL)
	    XtVaSetValues (maj_ave_menu[i][maj_ave_last[i]],
			   XmNset, True, NULL);

	rtmon_set_proc_ave_factor(cpu,1);
	
	XtVaSetValues (proc_ave_menu[i][proc_ave_last[i]],
		       XmNset, False, NULL);
	proc_ave_last[i] = 1;
	XtVaSetValues (proc_ave_menu[i][proc_ave_last[i]],
		       XmNset, True, NULL);
	rtmon_set_frame_ave_factor(cpu,1);
	
    }


}



