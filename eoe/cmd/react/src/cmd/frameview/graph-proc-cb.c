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
 * File:	graph-proc-cb.c           			          *
 *									  *
 * Description:	This file handles the scrolling text for grtmon  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

void set_slider_c_width_and_loc();

void 
thumb_c_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct *) callD;

    ortho_c = cbs->value;
    set_slider_c_width_and_loc();
}



void 
range_c_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    float max_pos_val;

    XmScaleCallbackStruct *call_value = (XmScaleCallbackStruct *) callD;

    max_pos_val =(float)((int)(100.0-(100.0*(float)ortho_c/(float)(MAJ_PROC_WIDTH*gr_numb_cpus))));
    range_c = 100.0*(float)call_value->value/max_pos_val;

}

void 
close_proc_graph_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtVaSetValues (proc_button, XmNset, False, NULL);
    proc_graph_on = FALSE;

}



void
get_mouse_pos_C (Widget w, XtPointer clientD, XtPointer callD)
{
    int x,y;
    int width,height;
    Dimension dim_width,dim_height;
    XmDrawingAreaCallbackStruct *call_data;
    float x_per, y_per;

    call_data = (XmDrawingAreaCallbackStruct *) callD;

    switch(call_data->event->type) {
      case KeyRelease:
	; /* do nothing at least not yet */
	break;
      case ButtonPress:
	x = call_data->event->xbutton.x;
	y = call_data->event->xbutton.y;

	XtVaGetValues(main_graph.glWidget, XmNwidth, &dim_width, 
		      XmNheight, &dim_height, NULL);

	width = (int) dim_width;
	height = (int) dim_height;

	x_per = ((float) x)/((float) width);
	y_per = 1.0 - ((float) y)/((float) height);
	break;
      default:
	break;
    }

}

void resize_proc_graph(Widget w, XtPointer data, XtPointer callData)
{
    Dimension       width, height;

    glXMakeCurrent(proc_graph.display, 
		   XtWindow(proc_graph.glWidget), 
		   proc_graph.context);
    
    XtVaGetValues(proc_graph.glWidget, XmNwidth, &width, XmNheight, &height, NULL);
    proc_graph.width = width;
    proc_graph.height = height;
    glViewport(0, 0, (GLint) width, (GLint) height);
    
}


/* major process average is graph C */
void
manage_proc_graph_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    static int varset = 0;
    static Widget overheadDialog, overheadForm;
    static Widget resetButton, resetMenu, resetAll, glFrame;
    static Widget maj_average_menu[MAX_NUMB_TO_AVE+1];
    static Widget average_button[MAX_NUMB_TO_AVE+1];
    static Widget menuBar;
    int whichGraph = (int)clientD;
    XmToggleButtonCallbackStruct *call_data = 
			(XmToggleButtonCallbackStruct *) callD;
    int i,j,n;
    int ii;  /* when we go though a loop this variable represents
		the gr_cpu_map of i */	
    Arg args[20];

    XmString enableAll_accel;
    char temp_str[32];
    XmString tempString;
    Atom wm_delete_window;

 
    if (no_frame_mode) {
	grtmon_message("internal error please report number", FATAL_MSG, 30);
	return;
    }
	
    if (!varset) {
	varset++;

	proc_graph.display = XtDisplay(toplevel);

	/* create an OpenGL rendering context */
	proc_graph.context = glXCreateContext(proc_graph.display, master_vi, None, GL_TRUE);
	if (proc_graph.context == NULL)
	    grtmon_message("could not create rendering context", WARNING_MSG, 32);
	
	
	
	overheadDialog = XtVaCreateWidget ("overhead dialog",
	    xmDialogShellWidgetClass, toplevel,
	    XmNtitle, "Real-Time Monitor (Process Time per Major Frame)",
	    XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);

	wm_delete_window = XmInternAtom(XtDisplay(overheadDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (overheadDialog, wm_delete_window, 
				 close_proc_graph_CB, NULL);


	proc_graph.topForm = XtVaCreateWidget ("procw",
            xmFormWidgetClass, overheadDialog,
	    XmNwidth, (gr_numb_cpus <= 4) ? (gr_numb_cpus*2*MAJ_PROC_WIDTH) : 1000,
            XmNheight, 300,
            NULL);

	XtManageChild(proc_graph.topForm);

        overheadForm = XtVaCreateWidget ("overheadForm",
	    xmFormWidgetClass, proc_graph.topForm,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNdeleteResponse, XmUNMAP,
	    XmNwidth, (gr_numb_cpus <= 4) ? (gr_numb_cpus*2*MAJ_PROC_WIDTH) : 1000,
            XmNheight, SCROLL_BAR_WIDTH,
 	    NULL);

	XtManageChild (overheadForm);

	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNminimum, MAJ_PROC_WIDTH); n++;
	XtSetArg (args[n], XmNmaximum, MAJ_PROC_WIDTH*gr_numb_cpus); n++;
	XtSetArg (args[n], SgNhomePosition, MAJ_PROC_WIDTH*gr_numb_cpus); n++;
	XtSetArg (args[n], SgNunitsPerRotation, 1); n++;
	thumbWheel_c = SgCreateThumbWheel (overheadForm, "thumb", args, n);
	XtManageChild (thumbWheel_c);
	XtAddCallback (thumbWheel_c, XmNvalueChangedCallback, thumb_c_CB, (XtPointer)&ortho_c);
	XtAddCallback (thumbWheel_c, XmNdragCallback, thumb_c_CB, (XtPointer)&ortho_c);

    
	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, thumbWheel_c); n++;
	XtSetArg (args[n], XmNshowValue, False); n++;
	XtSetArg (args[n], XmNsliderSize, 100); n++;
	XtSetArg (args[n], XmNshowArrows, False); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNmaximum, 100); n++;
	
	scroll_c = XmCreateScrollBar(overheadForm, "scroll", args, n);
	XtManageChild (scroll_c);
	
	XtAddCallback (scroll_c, XmNdragCallback, range_c_CB, NULL);
	XtAddCallback (scroll_c, XmNvalueChangedCallback, range_c_CB, NULL);



	menuBar = XmCreateMenuBar (proc_graph.topForm, "menuBar", NULL, 0);
	XtVaSetValues (menuBar,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNtopAttachment, XmATTACH_FORM,
	    NULL);
	XtManageChild (menuBar);

	resetButton = XtVaCreateManagedWidget ("Averaging Factor",
	xmCascadeButtonWidgetClass, menuBar,
	XmNmnemonic, 'A',
	NULL);

	resetMenu = XmCreatePulldownMenu (menuBar, "Average", NULL, 0);
	XtVaSetValues (resetButton, XmNsubMenuId, resetMenu, NULL);

	enableAll_accel = XmStringCreateSimple ("Ctrl<Key>R");
	tempString = XmStringCreateSimple ("Ctrl-R");
	sprintf (temp_str, "Reset all to %d", DEFAULT_AVE_VAL);

	resetAll = XtVaCreateManagedWidget (temp_str,
	    xmPushButtonWidgetClass, resetMenu,
            XmNmnemonic, 'R',
	    XmNaccelerator, enableAll_accel,
	    XmNacceleratorText, tempString,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (resetAll, XmNactivateCallback,
	    reset_maj_ave_CB, (XtPointer) (-1));

	XtVaCreateManagedWidget ("separator",
				 xmSeparatorGadgetClass, resetMenu, NULL);

	for (i=0; i < gr_numb_cpus; i++) {
	    ii = gr_cpu_map[i];
	    sprintf (temp_str, "cpu %d", ii);

	    average_button[i] = XtVaCreateManagedWidget (temp_str, 
	        xmCascadeButtonWidgetClass, resetMenu, NULL);

	    maj_average_menu[i] = XmCreatePulldownMenu (average_button[i], temp_str, NULL, 0);
	    XtVaSetValues (average_button[i], XmNsubMenuId, maj_average_menu[i], NULL);

	    maj_ave_last[i] = graph_info[ii].maj_numb_to_ave;
	    for (j=1; j <= MAX_NUMB_TO_AVE+1; j++) {
		if (j==1) 
		    sprintf (temp_str, "display last major frame");
		else if (j==MAX_NUMB_TO_AVE+1) 
		    sprintf (temp_str, "display comprehensive average");
		else 
		    sprintf (temp_str, "average of last %d major frames", j);

		maj_ave_menu[i][j] = XtVaCreateManagedWidget (temp_str,
	            xmToggleButtonWidgetClass, maj_average_menu[i], 
                    XmNset, (j==graph_info[ii].maj_numb_to_ave) ? True : False,
		    NULL);
	            XtAddCallback (maj_ave_menu[i][j], 
				   XmNvalueChangedCallback, 
				   maj_ave_CB, (XtPointer)(i*1000+j));
	    }
	}

	glFrame = XtVaCreateManagedWidget("glFrame", 
            xmFrameWidgetClass, proc_graph.topForm,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, menuBar,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, overheadForm,
            NULL);

	XtManageChild(glFrame);


	proc_graph.glWidget = XtVaCreateManagedWidget("proc_graph.glWidget", 
            glwMDrawingAreaWidgetClass, glFrame,
	    GLwNvisualInfo, master_vi, 
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
	    XmNwidth, (gr_numb_cpus <= 4) ? (gr_numb_cpus*2*MAJ_PROC_WIDTH) : 1000,
            XmNheight, 300,
            NULL);

	proc_graph.width = (gr_numb_cpus <= 4) ? (gr_numb_cpus*2*MAJ_PROC_WIDTH) : 1000;
	proc_graph.height = 300;

	XtAddCallback(proc_graph.glWidget, XmNresizeCallback, resize_proc_graph, NULL);
	XtAddCallback(proc_graph.glWidget, XmNinputCallback, get_mouse_pos_C, NULL);
	
	proc_graph.window = XtWindow(proc_graph.glWidget);
	XtRealizeWidget(overheadDialog);
	
	glXMakeCurrent(proc_graph.display, 
		       XtWindow(proc_graph.glWidget), 
		       proc_graph.context);
	
	setup_font(proc_graph.display);

	glClearDepth(1.0);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_MODELVIEW);
	

    }
    /* only main menu can remove overhed graph */
  
    if (call_data->set) {
	proc_graph_on++;
	XtManageChild (proc_graph.glWidget);
	XtManageChild (proc_graph.topForm);
    } else {
	XtUnmanageChild (proc_graph.glWidget);
	XtUnmanageChild (proc_graph.topForm);
	proc_graph_on--;
    }
}


void 
reset_proc_ave_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    register int cpu;

    for (cpu=0;cpu<gr_numb_cpus;cpu++) {
	XtVaSetValues (proc_ave_menu[cpu][proc_ave_last[cpu]],
		       XmNset, False, NULL);
	proc_ave_last[cpu] = 1;
	XtVaSetValues (proc_ave_menu[cpu][proc_ave_last[cpu]],
		       XmNset, True, NULL);
	rtmon_set_frame_ave_factor(gr_cpu_map[cpu],1);
    }

}


void
proc_ave_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    int ave_num;
    int cpu;


    ave_num = ((int)clientD)%1000;
    cpu = ((int)clientD)/1000;


    XtVaSetValues (proc_ave_menu[cpu][proc_ave_last[cpu]],
		   XmNset, False, NULL);
    proc_ave_last[cpu] = ave_num;
    XtVaSetValues (proc_ave_menu[cpu][proc_ave_last[cpu]],
		   XmNset, True, NULL);
    rtmon_set_frame_ave_factor(gr_cpu_map[cpu],ave_num);
}


void
set_slider_c_width_and_loc()
{
    int size_val;
    int range_val;
    


    size_val = (int)(0.99999999+C_RANGE*((float)ortho_c/(float)(MAJ_PROC_WIDTH*gr_numb_cpus)));
    
    if (size_val < 1) 
	size_val = 1;
    if (size_val > (int)C_RANGE) 
	size_val = (int)C_RANGE;

    range_val = (int) range_c;

    if ((range_val + size_val) > (int)C_RANGE) {
      range_val = (int)(C_RANGE - size_val);
    }

    XmScaleSetValue(scroll_c, 0);
    XtVaSetValues(scroll_c, XmNsliderSize, 1, NULL);

    XmScaleSetValue(scroll_c, range_val);

    XtVaSetValues(scroll_c, XmNsliderSize, size_val, NULL);

    if (kern_disp_mode == WIDE) {
	XtVaSetValues(thumbWheel_c, XmNvalue, ortho_c, NULL);
    }

}




