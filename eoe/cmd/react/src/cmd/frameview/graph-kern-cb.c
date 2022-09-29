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
 * File:	graph-kern-cb.c           			          *
 *									  *
 * Description:	This file handles the scrolling text for grtmon  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

void set_slider_b_width_and_loc();

void 
thumb_b_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct *) callD;
    int *scale = (int *)clientD;
    XtPointer userData;
    int i,cpu;


    if (cbs->value == (B_THUMB - 1)) {
	kern_disp_mode = WIDE;
	for (i=0;i<gr_numb_cpus;i++) {
	     cpu = gr_cpu_map[i];
	     kern_disp_minor[cpu] = 0;
	 }
	ortho_b = max_val_a;
	XtVaSetValues(thumbWheel_b, SgNhomePosition, B_THUMB - 2, NULL);
    }
    else if (cbs->value == (B_THUMB - 2)) {
	kern_disp_mode = WIDE;
	for (i=0;i<gr_numb_cpus;i++) {
	     cpu = gr_cpu_map[i];
	     kern_disp_minor[cpu] = 0;
	 }
	ortho_b = max_val_a;
	XtVaSetValues(thumbWheel_b, SgNhomePosition, B_THUMB - 1, NULL);
    }
    else {
	if (kern_disp_mode == WIDE)
	    ortho_b = (int) (((float)cbs->value/(float)B_THUMB)*max_val_a);
	else {
	ortho_b = kern_focus_min + 
	    (((((float)cbs->value/(float)B_THUMB)*max_val_a)/(float)(max_val_a - THUMB_B_MIN)) * 
	     ((float)(kern_focus_max-kern_focus_min)));
	}
    }
    set_slider_b_width_and_loc();

}


void 
range_b_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    float max_pos_val;

    XmScaleCallbackStruct *call_value = (XmScaleCallbackStruct *) callD;
	
    if (kern_disp_mode == WIDE) {
	max_pos_val =(float)((int)(100.0-(100.0*(float)ortho_b/(float)max_val_a)));
	range_b = 100.0*(float)call_value->value/max_pos_val;
    }
    else {
	max_pos_val =(float)((int)(100.0-(100.0*(float)ortho_b/(float)kern_focus_max)));
	range_b = 100.0*(float)call_value->value/max_pos_val;
    }
    
}


void 
close_kern_graph_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtVaSetValues (kern_button, XmNset, False, NULL);
    kern_graph_on = FALSE;

}


void resize_kern_graph(Widget w, XtPointer data, XtPointer callData)
{
    Dimension       width, height;

    glXMakeCurrent(kern_graph.display, 
		   XtWindow(kern_graph.glWidget), 
		   kern_graph.context);
    
    XtVaGetValues(kern_graph.glWidget, XmNwidth, &width, XmNheight, &height, NULL);
    kern_graph.width = width;
    kern_graph.height = height;
    glViewport(0, 0, (GLint) width, (GLint) height);
    
}

/* initialize window if we haven't been here before,
   else either manage or unmanage the graph as appropriate */
void
manage_kern_graph_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    static int varset = 0;
    static Widget overheadDialog, overheadForm, glFrame;
    XmToggleButtonCallbackStruct *call_data = 
			(XmToggleButtonCallbackStruct *) callD;
    int n;
    Arg args[20];
    Atom wm_delete_window;


    if (no_frame_mode) {
	grtmon_message("internal error please report number", FATAL_MSG, 24);
	return;
    }

    if (!varset) {
	
	varset++;
	
	kern_graph.display = XtDisplay(toplevel);

	/* create an OpenGL rendering context */
	kern_graph.context = glXCreateContext(kern_graph.display, master_vi, None, GL_TRUE);
	if (kern_graph.context == NULL)
	    grtmon_message("could not create rendering context", WARNING_MSG, 26);





	overheadDialog = XtVaCreateWidget ("overhead dialog",
	    xmDialogShellWidgetClass, toplevel,
	    XmNtitle, "Real-Time Monitor (Start of Frame Overhead)",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);
	

	wm_delete_window = XmInternAtom(XtDisplay(overheadDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (overheadDialog, wm_delete_window, 
				 close_kern_graph_CB, NULL);


	kern_graph.topForm = XtVaCreateWidget ("kernw",
            xmFormWidgetClass, overheadDialog,
	    XmNwidth, 1000,
            XmNheight, (gr_numb_cpus <= 10) ? ((2*gr_numb_cpus+1)*50) : 1000,
            NULL);

	XtManageChild(kern_graph.topForm);
	
        overheadForm = XtVaCreateWidget ("overheadForm",
	    xmFormWidgetClass, kern_graph.topForm,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNdeleteResponse, XmUNMAP,
            XmNwidth, 1000,
            XmNheight, SCROLL_BAR_WIDTH,
 	    NULL);

	XtManageChild (overheadForm);

	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNminimum, 1); n++;
	XtSetArg (args[n], XmNmaximum, B_THUMB); n++;
	XtSetArg (args[n], SgNhomePosition, B_THUMB); n++;
	XtSetArg (args[n], SgNunitsPerRotation, 10); n++;
	thumbWheel_b = SgCreateThumbWheel (overheadForm, "thumb", args, n);
	XtManageChild (thumbWheel_b);
	XtAddCallback (thumbWheel_b, XmNvalueChangedCallback, thumb_b_CB, (XtPointer)&ortho_emp);
	XtAddCallback (thumbWheel_b, XmNdragCallback, thumb_b_CB, (XtPointer)&ortho_emp);


	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, thumbWheel_b); n++;
	XtSetArg (args[n], XmNshowValue, False); n++;
	XtSetArg (args[n], XmNsliderSize, 100); n++;
	XtSetArg (args[n], XmNshowArrows, False); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNmaximum, 100); n++;
	
	scroll_b = XmCreateScrollBar(overheadForm, "scroll", args, n);
	XtManageChild (scroll_b);
	
	XtAddCallback (scroll_b, XmNdragCallback, range_b_CB, NULL);
	XtAddCallback (scroll_b, XmNvalueChangedCallback, range_b_CB, NULL);


        glFrame = XtVaCreateManagedWidget("glFrame", 
            xmFrameWidgetClass, kern_graph.topForm,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, overheadForm,
            NULL);

	XtManageChild(glFrame);



	kern_graph.glWidget = XtVaCreateManagedWidget("kern_graph.glWidget", 
            glwMDrawingAreaWidgetClass, glFrame,
	    GLwNvisualInfo, master_vi, 
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
            XmNwidth, 1000,
            XmNheight, (gr_numb_cpus <= 10) ? ((2*gr_numb_cpus+1)*50) : 1000,
            NULL);

	kern_graph.width = 1000;
	kern_graph.height = (gr_numb_cpus <= 10) ? ((2*gr_numb_cpus+1)*50) : 1000;

	XtAddCallback(kern_graph.glWidget, XmNresizeCallback, resize_kern_graph, NULL);
	XtAddCallback(kern_graph.glWidget, XmNinputCallback, get_mouse_pos_B, NULL);
	
	kern_graph.window = XtWindow(kern_graph.glWidget);
	XtRealizeWidget(overheadDialog);
	
	glXMakeCurrent(kern_graph.display, 
		       XtWindow(kern_graph.glWidget), 
		       kern_graph.context);
	
	setup_font(kern_graph.display);

	glClearDepth(1.0);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_MODELVIEW);
	


    }
    /* only main menu can remove overhed graph */
    if (call_data->set) {
	kern_graph_on++;
	XtManageChild (kern_graph.glWidget);
	XtManageChild (kern_graph.topForm);
    } else {
	XtUnmanageChild (kern_graph.glWidget);
	XtUnmanageChild (kern_graph.topForm);
	kern_graph_on--;
    }



    XtVaSetValues(thumbWheel_b, SgNhomePosition, B_THUMB - 1, NULL);
}



#define KERN_X_START_P (LEFT_BORDER_B/SCALE_FACT_B)
#define KERN_X_END_P ((1.0/SCALE_FACT_B) + (LEFT_BORDER_B/SCALE_FACT_B))
#define KERN_X_ERR_P 0.01

void
get_mouse_pos_B (Widget w, XtPointer clientD, XtPointer callD)
{
    int x,y,i,j,k;
    Dimension dim_width,dim_height;
    int width,height;
    float percent,start_val;
    XmDrawingAreaCallbackStruct *call_data;
    float x_per, y_per;
    float scale_per;
    float adj_val;
    float y_abs;
    int temp_pos;
    int cpu, cpu_numb, cpu_pos;
    int minor, cur_time, temp_cpu;
    float exp_fact;

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
	
	if ((x_per < KERN_X_START_P) || (x_per > KERN_X_END_P)) {
	    return;
	}

	y_abs = (ortho_by-(ortho_by*y_per))-TOP_BORDER_B;
	temp_pos = y_abs/(2*HEIGHT);

	
	if ((y_abs >= ((temp_pos+1)*2*HEIGHT - (0.5+RECT_WIDTH)*HEIGHT)) &&
	    (y_abs <= ((temp_pos+1)*2*HEIGHT - (0.5-RECT_WIDTH)*HEIGHT)))
	    cpu_pos = temp_pos;
	else
	    cpu_pos = -1;

	/* now look for mapping */
	cpu = -1;
	cpu_numb = 0;
	for (i=0; i<gr_numb_cpus; i++) {
	    if (cpuOn[i]) {
		if (cpu_pos == cpu_numb)
		    cpu = gr_cpu_map[i];
		cpu_numb++;
	    }
	}
	if (cpu == -1) return;


	/* I have the cpu the user has clicked on, now we need to 
	   first determine the minor frame and then possibly display
	   the time for that process, for cuteness let's do assume
	   the idle process is at the end of the minor */

	scale_per = (x_per - KERN_X_START_P)/(KERN_X_END_P - KERN_X_START_P);
	percent = (float)ortho_b/(float)max_val_a;
	start_val = (((1-percent)*max_val_a) * ((float)range_b/100));
	adj_val = (scale_per*ortho_b)+start_val;

	
	minor = (int)adj_val/major_frame_account[cpu].minor_time;

	cur_time = minor*major_frame_account[cpu].minor_time;


	exp_fact = 1.0;
	if (graph_info[cpu].max_kern_minor[minor].ex_ev_val > 0)
	    exp_fact = (float)(ortho_b*EXP_PER)/
		(float)graph_info[cpu].max_kern_minor[minor].ex_ev_val;

	if (exp_fact < 1) exp_fact = 1.0;


	if (((adj_val - cur_time) > (exp_fact*graph_info[cpu].max_kern_minor[minor].ex_ev_val
				     -  KERN_X_ERR_P*ortho_b)) &&
	    ((adj_val - cur_time) < (graph_info[cpu].max_kern_minor[minor].ex_ev_val +
				     ((EXP_PER + KERN_X_ERR_P)*ortho_b)))) {
	    goto_frame_number(graph_info[cpu].max_kern_minor[minor].ex_ev_maj_frame, cpu);
	    return;
	}

	if (kern_disp_mode == FOCUS) {
	    if ((adj_val > (graph_info[cpu].max_kern_minor[kern_disp_minor[cpu]].ex_ev_val - 
			    KERN_X_ERR_P*ortho_b)) &&
		(adj_val < (graph_info[cpu].max_kern_minor[kern_disp_minor[cpu]].ex_ev_val +
			    ((EXP_PER + KERN_X_ERR_P)*ortho_b)))) {
		goto_frame_number(graph_info[cpu].max_kern_minor[kern_disp_minor[cpu]].ex_ev_maj_frame, cpu);
	    }
	    return;
	}

	if ((adj_val > ((minor*major_frame_account[cpu].minor_time) 
			- (KERN_X_ERR_P * ortho_b))) &&
	    (adj_val < ((minor*major_frame_account[cpu].minor_time) 
			+ graph_info[cpu].ave_minor[minor].start_time
			+ ((KERN_X_ERR_P+EXP_PER) * ortho_b)))) {
	    kern_disp_val = minor*major_frame_account[cpu].minor_time;
	    kern_disp_mode = FOCUS;
	    kern_disp_minor[cpu] = minor;

	    ortho_b = (int)(1.25 * (graph_info[cpu].ave_minor[minor].start_time));
	    kern_focus_min = ortho_b/20;
	    kern_focus_max = ortho_b*5;
	    XtVaSetValues(thumbWheel_b, XmNvalue, B_THUMB/6, NULL);
	    set_slider_b_width_and_loc();

	    /* see if there are any other kernel bars going
	       to be shown in conjunction with this one */
	    for (j=0; j<gr_numb_cpus; j++) {
		temp_cpu = gr_cpu_map[j];
		if (cpu != temp_cpu) {
		    for (k=0; k<major_frame_account[temp_cpu].numb_minors; k++) {
			if ((k*major_frame_account[temp_cpu].minor_time >= cur_time) &&
			    k*major_frame_account[temp_cpu].minor_time <= (cur_time + kern_focus_max)) {
			    kern_disp_minor[temp_cpu] = k;
			}
		    }
		}
	    }
	    
	    goto found_one;
	}
	
      found_one: ;
	break;
      default:
	break;
    }

}


void
set_slider_b_width_and_loc()
{
    int size_val;
    int range_val;
    

    if (kern_disp_mode == WIDE) {
	size_val = (int)(0.99999999+B_RANGE*((float)ortho_b/(float)max_val_a));
    }
    else {
	size_val = (int)(0.99999999+B_RANGE*((float)ortho_b/(float)(kern_focus_max)));
    }
    if (size_val < 1) 
	size_val = 1;
    if (size_val > (int)B_RANGE) 
	size_val = (int)B_RANGE;

    range_val = (int) range_b;

    if ((range_val + size_val) > (int)B_RANGE) {
      range_val = (int)(B_RANGE - size_val);
    }

    XmScaleSetValue(scroll_b, 0);
    XtVaSetValues(scroll_b, XmNsliderSize, 1, NULL);

    XmScaleSetValue(scroll_b, range_val);

    XtVaSetValues(scroll_b, XmNsliderSize, size_val, NULL);

    if (kern_disp_mode == WIDE) {
	XtVaSetValues(thumbWheel_b, XmNvalue, 
		        (int)(B_THUMB*((float)ortho_b/(float)max_val_a)), NULL);
    }

}



