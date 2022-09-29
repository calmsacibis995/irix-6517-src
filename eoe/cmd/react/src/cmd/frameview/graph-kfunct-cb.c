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
 * File:	graph-kfunct-cb.c           			          *
 *									  *
 * Description:	This file handles the call backs for the kernel functions *
 *		graph							  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#define GRMAIN extern
#include "graph.h"
#include "graph-kfunct.h"
#include "kf_main.h"

void kf_binary_search(int cpu, int search_val, int is_start_val);
void set_slider_kf_width_and_loc();

/*
 * kf_readjust_start_and_end - takes new times and readjust for all cpus
 *  what the start and end event is
 */

void kf_readjust_start_and_end()
{
    int i,cpu;
    int printed_it;
    int old_width;
    int zoomout_width, zoomout_start;
    char msg_str[64];

#ifdef DEBUG
sprintf(msg_str, "kf_readjust_start_and_end()");
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

    printed_it = 0;
    kf_max_val_a = 0;
    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	cpu = gr_cpu_map[i];
	if (graph_info[cpu].no_frame_event[graph_info[cpu].
	                      no_frame_event_count-1].end_time > kf_max_val_a)
	    kf_max_val_a = graph_info[cpu].no_frame_event[
			       graph_info[cpu].no_frame_event_count-1].end_time;
    }

    if (kf_start_val_a < 0) 
	kf_start_val_a = 0;
    if ((kf_start_val_a + kf_width_a) > kf_max_val_a)
	kf_start_val_a = kf_max_val_a - kf_width_a;

sprintf(msg_str, "kf_readjust_start_and_end(): [1] max %d start %d width %d zoomout %d",kf_max_val_a,kf_start_val_a, kf_width_a, kf_max_zoomout_flag);
grtmon_message(msg_str, DEBUG_MSG, 100);


    /* doing max zoom out: if cannot zoom out completely (too many events),
       try at least to zoom out around the original middle of the screen */
    if(kf_max_zoomout_flag) {
	zoomout_start = kf_start_val_a;
	zoomout_width = kf_width_a;
    }

    try_again:
    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	if (cpu_on[gr_cpu_map[i]]) {

#ifdef DEBUG
sprintf(msg_str, "kf_readjust_start_and_end(): in if (cpu_on[gr_cpu_map[i]])");
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

	    cpu = gr_cpu_map[i];
/*
	  try_again:
*/
	    kf_binary_search(cpu, kf_start_val_a, 1);
	    kf_binary_search(cpu, kf_start_val_a+kf_width_a, 0);
	    if (graph_info[cpu].nf_end_event -  graph_info[cpu].nf_start_event
					> KF_MAX_NUMBER_EVENTS_TO_DISPLAY) {

		old_width = kf_width_a;
		kf_width_a = (int) (0.8 * kf_width_a);

		/* if zooming out, try to keep center near where it was */
		if(kf_max_zoomout_flag) {
			if(kf_width_a/2 >
			   kf_orig_start + kf_orig_width/2 - zoomout_start)
				kf_start_val_a = zoomout_start;
						/* touch left of window */
			else
				if(kf_width_a/2 >
					  zoomout_start + zoomout_width -
			   		  (kf_orig_start + kf_orig_width/2))
					kf_start_val_a = 
						zoomout_start + zoomout_width -
						kf_width_a; /* touch right */
				else
					kf_start_val_a = 
						kf_orig_start +
						  (kf_orig_width-kf_width_a)/2;

		}
		else {
			if(kf_force_start_to_zero)
				kf_start_val_a = 0;
			else
				kf_start_val_a += (old_width - kf_width_a)/2;
						/* centering window */
		}

		sprintf(msg_str, "CPU %d: attempting to display %d events.", cpu,
		  graph_info[cpu].nf_end_event-graph_info[cpu].nf_start_event);
		grtmon_message(msg_str, DEBUG_MSG, 100);
		sprintf(msg_str, "\tReducing window to %s",
			us_to_valunit_str(kf_width_a));
		grtmon_message(msg_str, DEBUG_MSG, 100);

		goto try_again;
	    }
	}
    }

sprintf(msg_str, "kf_readjust_start_and_end(): [2] max %d start %d width %d zoomout %d",
kf_max_val_a,kf_start_val_a, kf_width_a, kf_max_zoomout_flag);
grtmon_message(msg_str, DEBUG_MSG, 100);

    kf_max_zoomout_flag = 0;
    kf_force_start_to_zero = 0;

    set_slider_kf_width_and_loc();
}

void
kf_binary_search(int cpu, int search_val, int is_start_val)
{
    int temp_start, temp_end;

    temp_start = 0;
    temp_end = graph_info[cpu].no_frame_event_count;

    while (temp_start < (temp_end - 1)) {
	if (graph_info[cpu].no_frame_event[(temp_start+temp_end)/2].start_time
								<= search_val)
	    temp_start = (temp_start+temp_end)/2;
	else
	    temp_end = (temp_start+temp_end)/2;
    }
    if (is_start_val) {
	graph_info[cpu].nf_start_event = temp_start-1;
	if (graph_info[cpu].nf_start_event < 1)
	    graph_info[cpu].nf_start_event = 1;
    }
    else {
	graph_info[cpu].nf_end_event = temp_end+1;
	if (graph_info[cpu].nf_end_event >=
	    graph_info[cpu].no_frame_event_count)
	    graph_info[cpu].nf_end_event =
				graph_info[cpu].no_frame_event_count-1;
    }
}



/* ----------- */

#ifdef ORIG
void 
thumb_kf_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct *) callD;
    int *scale = (int *)clientD;
    XtPointer userData;
    int i,cpu;
    char msg_str[64];


    if (cbs->value == (gr_max_time - 1)) {
	kf_disp_mode = WIDE;
	for (i=0;i<gr_numb_cpus;i++) {
	     cpu = gr_cpu_map[i];
/*
	     kern_disp_minor[cpu] = 0;
sprintf(msg_str, "thumb_kf_CB: kern_disp_minor??");
grtmon_message(msg_str, DEBUG_MSG, 100);
*/
	 }
/*
	kf_width_a = gr_max_time;
*/
	kf_width_a = get_max_time();
	XtVaSetValues(thumbWheel_kf, SgNhomePosition, gr_max_time - 2, NULL);
    }
    else if (cbs->value == (gr_max_time - 2)) {
	kf_disp_mode = WIDE;
	for (i=0;i<gr_numb_cpus;i++) {
	     cpu = gr_cpu_map[i];
/*
	     kern_disp_minor[cpu] = 0;
*/
	 }
/*
	kf_width_a = gr_max_time;
*/
	kf_width_a = get_max_time();
	XtVaSetValues(thumbWheel_kf, SgNhomePosition, gr_max_time - 1, NULL);
    }
    else {
	if (kf_disp_mode == WIDE)
/*
	    kf_width_a = cbs->value;
*/
	    kf_width_a = (get_max_time()*cbs->value)/THUMB_KF_MAX;
	else {
/*
	    kf_width_a = kf_focus_min + 
	    (((float)cbs->value/(float)(gr_max_time - THUMB_KF_MIN)) * 
	     ((float)(kf_focus_max-kf_focus_min)));
*/
            kf_width_a = (get_max_time()*cbs->value)/THUMB_KF_MAX;
	}
    }

    if(kf_start_val_a + kf_width_a > get_max_time())
	kf_start_val_a = get_max_time() - kf_width_a;

sprintf(msg_str, "thumb_kf_CB: cbs->value %f kf_width_a %d kf_start_val_a %d",
(float)cbs->value,kf_width_a,kf_start_val_a);
grtmon_message(msg_str, DEBUG_MSG, 100);

    kf_readjust_start_and_end();

/*
    set_slider_kf_width_and_loc();
*/

}
#endif

/* -------------------------- */


void 
thumb_kf_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct *) callD;
    int *scale = (int *)clientD;
    XtPointer userData;
    int i,cpu;
    int middle;
    char msg_str[64];

    /* if cursor is not active or not visible, zoom around center;
       otherwise zoom around cursor */
    if((kf_cursor_time != -1) &&
       (kf_cursor_time >= kf_start_val_a) &&
       (kf_cursor_time < kf_start_val_a+kf_width_a))
	middle = kf_cursor_time;
    else
        middle = kf_start_val_a + kf_width_a/2;

    if(cbs->value >= THUMB_KF_MAX - 1){
	kf_max_zoomout_flag = 1;
	kf_orig_start = kf_start_val_a;
	kf_orig_width = kf_width_a;
    }

#ifdef DEBUG
sprintf(msg_str, "thumb_kf_CB (BEFORE): value %f width %d start %d middle %d",
(float)cbs->value,kf_width_a,kf_start_val_a, middle);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

    kf_width_a = ((double)get_max_time()*cbs->value)/(double)THUMB_KF_MAX;

    /* adjust start to be in the middle */
    kf_start_val_a = middle - kf_width_a/2;

    if(kf_start_val_a + kf_width_a > get_max_time())
	kf_start_val_a = get_max_time() - kf_width_a;

#ifdef DEBUG
sprintf(msg_str, "thumb_kf_CB (AFTER): val %d width %d start %d",
cbs->value,kf_width_a,kf_start_val_a);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif


    kf_readjust_start_and_end();
}



void 
range_kf_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    float max_pos_val;
    char msg_str[64];

    XmScaleCallbackStruct *call_value = (XmScaleCallbackStruct *) callD;
	
#ifdef OLD
    if (kf_disp_mode == WIDE) {
	max_pos_val =
	     (float)((int)(KF_RANGE-
			(KF_RANGE*(float)kf_width_a/(float)get_max_time())));
	range_kf = KF_RANGE*(float)call_value->value/max_pos_val;
    }
    else {
	max_pos_val =
	   (float)((int)(KF_RANGE-
			(KF_RANGE*(float)kf_width_a/(float)kf_focus_max)));
	range_kf = KF_RANGE*(float)call_value->value/max_pos_val;
    }
#endif

    switch(call_value->reason){
	case XmCR_INCREMENT:
		kf_start_val_a += kf_width_a *
				        KF_RANGE_SLIDER_INCR_WIDTH_MULTIPLIER;
		break;

	case XmCR_DECREMENT:
		kf_start_val_a -= kf_width_a *
				        KF_RANGE_SLIDER_INCR_WIDTH_MULTIPLIER;
		break;

	case XmCR_PAGE_INCREMENT:
		kf_start_val_a += kf_width_a *
				KF_RANGE_SLIDER_PAGE_INCR_WIDTH_MULTIPLIER;
		break;

	case XmCR_PAGE_DECREMENT:
		kf_start_val_a -= kf_width_a *
				KF_RANGE_SLIDER_PAGE_INCR_WIDTH_MULTIPLIER;
		break;

	default:
    		kf_start_val_a =
			(int)((float)get_max_time() *
				         (float)call_value->value / KF_RANGE);
    }

#ifdef DEBUG
sprintf(msg_str, "range_kf_CB: value %d reason %d range_kf %7.2f kf_start_val_a %d",
(int)call_value->value,call_value->reason,range_kf,kf_start_val_a);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
    
    kf_readjust_start_and_end();
}


void 
close_kf_graph_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtVaSetValues (kern_funct_button, XmNset, False, NULL);
    kern_funct_graph_on = FALSE;

}


void resize_kf_graph(Widget w, XtPointer data, XtPointer callData)
{
    Dimension       width, height;
    char msg_str[64];

sprintf(msg_str, "resize_kf_graph");
grtmon_message(msg_str, DEBUG_MSG, 100);


    glXMakeCurrent(kf_graph.display, 
		   XtWindow(kf_graph.glWidget), 
		   kf_graph.context);
    
    XtVaGetValues(kf_graph.glWidget,XmNwidth,&width,XmNheight, &height, NULL);
    kf_graph.width = width;
    kf_graph.height = height;
    glViewport(0, 0, (GLint) width, (GLint) height);
    
}

/*
 * initialize window if we haven't been here before,
 *  else either manage or unmanage the graph as appropriate
 */




#ifdef OLD
void
manage_kern_funct_graph_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    char msg_str[64];

    sprintf(msg_str, "Felipe in here you should put code which could look similar to ");
    grtmon_message(msg_str, DEBUG_MSG, 100);
    sprintf(msg_str, "what's in graph-kern-cb.c");
    grtmon_message(msg_str, DEBUG_MSG, 100);
    
    sprintf(msg_str, "just so you know I have already defined seom variables name you will");
    grtmon_message(msg_str, DEBUG_MSG, 100);
    sprintf(msg_str, "probably be using, they are:");
    grtmon_message(msg_str, DEBUG_MSG, 100);
    sprintf(msg_str, "   kern_funct_graph, kern_funct_graph_on\n");
    grtmon_message(msg_str, DEBUG_MSG, 100);
    sprintf(msg_str, "where necessary they have also been initialized");
    grtmon_message(msg_str, DEBUG_MSG, 100);
    sprintf(msg_str, "you may need more variables, you can define them in graph.h\n");
    grtmon_message(msg_str, DEBUG_MSG, 100);
    sprintf(msg_str, "I also included your changes, I think you needed to check for 1001");
    grtmon_message(msg_str, DEBUG_MSG, 100);
}
#endif


void
manage_kern_funct_graph_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    static int varset = 0;
    static Widget overheadDialog, overheadForm, glFrame;
    XmToggleButtonCallbackStruct *call_data = 
			(XmToggleButtonCallbackStruct *) callD;
    char msg_str[64];
    int n;
    Arg args[40];
    Atom wm_delete_window;
	Bool retval;

/* --------------- */
Widget menuBar;
Widget findButton;
Widget findMenu;
Widget findFunction;
Widget helpButton;
Widget helpMenu;
Widget helpUsing;
/* -------------- */

    kern_funct_graph_on = 1 - kern_funct_graph_on;

    if (!varset) {

sprintf(msg_str, "manage_kern_funct_graph_CB: initializing variables");
grtmon_message(msg_str, DEBUG_MSG, 100);

     kf_graph_init();
	
	varset++;
	
	kf_graph.display = XtDisplay(toplevel);

	/* create an OpenGL rendering context */
	kf_graph.context = glXCreateContext(kf_graph.display, master_vi, None,GL_TRUE);
	if (kf_graph.context == NULL) {
	    sprintf(msg_str, "error: could not create rendering context");
	    grtmon_message(msg_str, DEBUG_MSG, 100);
	}


	overheadDialog = XtVaCreateWidget ("overhead dialog",
	    xmDialogShellWidgetClass, toplevel,
	    XmNtitle, "Kernel Function Execution Times",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);

	wm_delete_window = XmInternAtom(XtDisplay(overheadDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (overheadDialog, wm_delete_window, 
				 close_kf_graph_CB, NULL);

	kf_graph.topForm = XtVaCreateWidget ("kfw",
            xmFormWidgetClass, overheadDialog,
	    XmNwidth, 1200,
            XmNheight, (gr_numb_cpus <= 10) ? ((2*gr_numb_cpus+1)*50) : 1000,
            NULL);

	XtManageChild(kf_graph.topForm);
	


/* --------- */
{
	n = 0;
	XtSetArg (args[n], XmNtopWidget, overheadDialog); n++;

/*
    menuBar = XmCreateMenuBar (kf_graph.topForm, "menuBar", NULL, 0);
*/
    menuBar = XmCreateMenuBar (kf_graph.topForm, "menuBar", args, n);

    XtVaSetValues (menuBar,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
            NULL);
    XtManageChild (menuBar);

    findButton = XtVaCreateManagedWidget ("Find",
            xmCascadeButtonWidgetClass, menuBar,
            XmNmnemonic, 'F',
            NULL);

    findMenu = XmCreatePulldownMenu (menuBar, "Find", NULL, 0);
    XtVaSetValues (findButton, XmNsubMenuId, findMenu, NULL);

    findFunction = XtVaCreateManagedWidget ("Find function",
            xmPushButtonWidgetClass, findMenu,
            NULL);
    XtAddCallback (findFunction, XmNactivateCallback, find_function_CB, NULL);

    helpButton = XtVaCreateManagedWidget ("Help",
            xmCascadeButtonWidgetClass, menuBar,
            XmNmnemonic, 'H',
            NULL);

    helpMenu = XmCreatePulldownMenu (menuBar, "Help", NULL, 0);
    XtVaSetValues (helpButton, XmNsubMenuId, helpMenu, NULL);

    helpUsing = XtVaCreateManagedWidget ("Using the tool",
            xmPushButtonWidgetClass, helpMenu,
            NULL);
    XtAddCallback (helpUsing, XmNactivateCallback, help_using_CB, NULL);


	sprintf(msg_str, "menuBar = 0x%x findButton 0x%x", menuBar,findButton);
	grtmon_message(msg_str, DEBUG_MSG, 100);
}
/* --------- */


        overheadForm = XtVaCreateWidget ("overheadForm",
	    xmFormWidgetClass, kf_graph.topForm,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNdeleteResponse, XmUNMAP,
            XmNwidth, 1200,
            XmNheight, SCROLL_BAR_WIDTH,
/*
            XmNheight, 34,
*/
 	    NULL);

	XtManageChild (overheadForm);



	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
/*
	XtSetArg (args[n], XmNminimum, THUMB_KF_MIN); n++;
	XtSetArg (args[n], XmNmaximum, gr_max_time); n++;
	XtSetArg (args[n], SgNhomePosition, gr_max_time); n++;
*/
	XtSetArg (args[n], XmNminimum, THUMB_KF_MIN); n++;
	XtSetArg (args[n], XmNmaximum, THUMB_KF_MAX); n++;
	XtSetArg (args[n], SgNhomePosition, THUMB_KF_MAX); n++;

	XtSetArg (args[n], SgNunitsPerRotation,THUMB_KF_UNITS_PER_ROTATION);n++;
	XtSetArg (args[n], SgNangleRange,THUMB_KF_ANGLE_RANGE);n++;
	thumbWheel_kf = SgCreateThumbWheel (overheadForm, "thumb", args, n);
	XtManageChild (thumbWheel_kf);
	XtAddCallback (thumbWheel_kf, XmNvalueChangedCallback,
				thumb_kf_CB, (XtPointer)&ortho_emp);
	XtAddCallback (thumbWheel_kf, XmNdragCallback, thumb_kf_CB,
							(XtPointer)&ortho_emp);


	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, thumbWheel_kf); n++;
/*
	XtSetArg (args[n], XmNshowValue, False); n++;
*/
	XtSetArg (args[n], XmNshowValue, True); n++;
/*
	XtSetArg (args[n], XmNsliderSize, 100); n++;
*/
	XtSetArg (args[n], XmNsliderSize, 10); n++;
/*
	XtSetArg (args[n], XmNshowArrows, False); n++;
*/
	XtSetArg (args[n], XmNshowArrows, True); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNmaximum, KF_RANGE); n++;
	
	scroll_kf = XmCreateScrollBar(overheadForm, "scroll", args, n);
	XtManageChild (scroll_kf);
	
	XtAddCallback (scroll_kf, XmNdragCallback, range_kf_CB, NULL);
	XtAddCallback (scroll_kf, XmNvalueChangedCallback, range_kf_CB, NULL);

	XtAddCallback (scroll_kf, XmNdecrementCallback, range_kf_CB, NULL);
	XtAddCallback (scroll_kf, XmNincrementCallback, range_kf_CB, NULL);
	XtAddCallback (scroll_kf, XmNpageDecrementCallback, range_kf_CB, NULL);
	XtAddCallback (scroll_kf, XmNpageIncrementCallback, range_kf_CB, NULL);

        glFrame = XtVaCreateManagedWidget("glFrame", 
            xmFrameWidgetClass, kf_graph.topForm,
/*
            xmFrameWidgetClass, menuBar,
*/

/*
            XmNtopAttachment, XmATTACH_FORM,
*/
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, overheadForm,
XmNtopAttachment,  XmATTACH_WIDGET,
XmNtopWidget, menuBar,
/*
XmNtopWidget, menuBar,
*/
/*
XmNrightWidget, menuBar,
XmNtopWidget, overheadForm,
XmNbottomWidget, menuBar,
            XmNbottomWidget, overheadForm,
            XmNtopWidget, overheadForm,
            XmNtopWidget, menuBar,
*/
            NULL);

	XtManageChild(glFrame);


	kf_graph.glWidget = XtVaCreateManagedWidget("kf_graph.glWidget", 
            glwMDrawingAreaWidgetClass, glFrame,
	    GLwNvisualInfo, master_vi, 
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
            XmNwidth, 1200,
            XmNheight, (gr_numb_cpus <= 10) ? ((2*gr_numb_cpus+1)*50) : 1000,
            NULL);

sprintf(msg_str, "manage_kern_funct_graph_CB: topForm 0x%x glWidget 0x%x",
kf_graph.topForm, kf_graph.glWidget);
grtmon_message(msg_str, DEBUG_MSG, 100);

	kf_graph.width = 1200;
	kf_graph.height = (gr_numb_cpus <= 10) ? ((2*gr_numb_cpus+1)*50) : 1000;

	XtAddCallback(kf_graph.glWidget, XmNresizeCallback, resize_kf_graph,
									NULL);
	XtAddCallback(kf_graph.glWidget,XmNinputCallback,get_mouse_pos_KF,NULL);
/*
	XtAddCallback(kf_graph.glWidget,GLwNinputCallback,get_mouse_pos_KF,NULL);
*/

/*
 * it works for the mouse but not for the keyboard
XtAddEventHandler(kf_graph.glWidget, KeyReleaseMask|ButtonPressMask,
						False,find_function_CB, 0);
*/

#ifdef TESTING
{
int testing();
/*
XtAddEventHandler(kf_graph.glWidget, FocusChangeMask, False,testing, 0);
*/
XtAddEventHandler(kf_graph.glWidget, 0xffffff, False,testing, 0);
}
#endif
	
	kf_graph.window = XtWindow(kf_graph.glWidget);


	XtRealizeWidget(overheadDialog);
/* FK
	XtRealizeWidget(menuBar);
	XtRealizeWidget(kf_graph.topForm);
*/
	
	retval = glXMakeCurrent(kf_graph.display, 
		       XtWindow(kf_graph.glWidget), 
		       kf_graph.context);

sprintf(msg_str, "manage_kern_funct_graph_CB: glXMakeCurrent returned %d",retval);
grtmon_message(msg_str, DEBUG_MSG, 100);
	
	setup_font(kf_graph.display);

	glClearDepth(1.0);
/*
	glClearColor(0.0, 0.0, 0.0, 0.0);
*/
	glClearColor(0.0, 1.0, 0.0, 0.0);
glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

    }

    /* only main menu can remove overhed graph */
    if (call_data->set) {
	kern_funct_graph_on++;
	XtManageChild (kf_graph.glWidget);
	XtManageChild (kf_graph.topForm);
    } else {
	XtUnmanageChild (kf_graph.glWidget);
	XtUnmanageChild (kf_graph.topForm);
	kern_funct_graph_on--;
    }

    XtVaSetValues(thumbWheel_kf, SgNhomePosition, KF_RANGE - 1, NULL);
}


/* SEE graph-kfunct.h -------------------------------
#define KF_X_START_P (LEFT_BORDER_KF/SCALE_FACT_KF)
#define KF_X_END_P ((1.0/SCALE_FACT_KF) + (LEFT_BORDER_KF/SCALE_FACT_KF))
#define KF_X_ERR_P 0.01

#define KF_START_PER (LEFT_BORDER_KF/SCALE_FACT_FK)
#define KF_END_PER ((1.0/SCALE_FACT_KF) + (LEFT_BORDER_KF/SCALE_FACT_KF))
*/


void
get_mouse_pos_KF (Widget w, XtPointer clientD, XtPointer callD)
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
    int new_start_val, new_end_val;
    char msg_str[64];

    call_data = (XmDrawingAreaCallbackStruct *) callD;

#ifdef DEBUG
sprintf(msg_str, "get_mouse_pos_KF ev tp %d",call_data->event->type);
    grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

    switch(call_data->event->type) {
      case KeyRelease:
sprintf(msg_str, "get_mouse_pos_KF: key released: 0x%x",
call_data->event->xkey.state);
grtmon_message(msg_str, DEBUG_MSG, 100);
	; /* do nothing at least not yet */
	break;
      case KeyPress:
sprintf(msg_str, "get_mouse_pos_KF: key pressed: 0x%x",
call_data->event->xkey.state);
grtmon_message(msg_str, DEBUG_MSG, 100);
	; /* do nothing at least not yet */
	break;
      case ButtonPress:
	x = call_data->event->xbutton.x;
	y = call_data->event->xbutton.y;
	XtVaGetValues(kf_graph.glWidget, XmNwidth, &dim_width, 
		      XmNheight, &dim_height, NULL);

	width = (int) dim_width;
	height = (int) dim_height;

	x_per = ((float) x)/((float) width);
	y_per = 1.0 - ((float) y)/((float) height);
	
	if ((x_per < KF_X_START_P) || (x_per > KF_X_END_P)) {
	    kf_cursor_time = -1;
	    return;
	}

	y_abs = (kf_height_a-(kf_height_a*y_per))-TOP_BORDER_KF;
	temp_pos = y_abs/(2*HEIGHT);

	scale_per = (x_per - KF_X_START_P)/(KF_X_END_P - KF_X_START_P);
        adj_val = (scale_per*kf_width_a)+kf_start_val_a;

	if ((y_abs >= ((temp_pos+1)*2*HEIGHT - (0.5+RECT_WIDTH)*HEIGHT)) &&
	    (y_abs <= ((temp_pos+1)*2*HEIGHT - (0.5-RECT_WIDTH)*HEIGHT)))
	    cpu_pos = temp_pos;
	else
	    cpu_pos = -1;

#ifdef DEBUG
sprintf(msg_str, "get_mouse_pos_KF: W %d H %d scale_per %f adj_val %7.1f cpu_pos %d",
width,height,scale_per,adj_val,cpu_pos);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

#ifdef DEBUG
{
int t,y;

if(cpu_pos != -1){
int funcno,func_index;
func_index = get_function_index_by_position((int) adj_val, cpu_pos);
/*funcno = get_current_func_no(cpu_pos, func_index);*/
sprintf(msg_str, "Mouse positioned at index %d func no %d",func_index,funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
}
getCursorPosition(&t,&y);
}
#endif

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

/*
	if (cpu == -1){
sprintf(msg_str, "get_mouse_pos_KF: returning (cpu == -1)");
grtmon_message(msg_str, DEBUG_MSG, 100);
		return;
	}
*/

#ifdef DEBUG
if(call_data->event->xbutton.button == Button1) {
sprintf(msg_str, "get_mouse_pos_KF: button 1");
grtmon_message(msg_str, DEBUG_MSG, 100);}
if(call_data->event->xbutton.button == Button2) {
sprintf(msg_str, "get_mouse_pos_KF: button 2");
grtmon_message(msg_str, DEBUG_MSG, 100);}
if(call_data->event->xbutton.button == Button3){
sprintf(msg_str, "get_mouse_pos_KF: button 3");
grtmon_message(msg_str, DEBUG_MSG, 100);}
#endif

/*
 * now we don't require user to click on the bottom of the screen to
 *	zoom; can click anywhere
 */

/*
	if ((temp_pos == gr_numb_cpus) &&
*/
	if((call_data->event->type == ButtonPress) &&
	   (call_data->event->xbutton.button == Button1)){


		/* user clicked in bottom scale and wants to zoom */
		if ((x_per < KF_X_START_P) || (x_per > KF_X_END_P)) {
		    kf_zoom_line = -1;
		    zoom_state = Z_NO_ZOOM;
		    kf_cursor_time = -1;
		    return;
		}
		else {
		    if ((zoom_state != Z_NO_ZOOM) && (kf_zoom_line >= 0)) {
sprintf(msg_str, "get_mouse_pos_KF: Should not be at this point!!");
grtmon_message(msg_str, DEBUG_MSG, 100);
			/* SHOULD NOT GET HERE */
			/* we have to perform a zoom: figure out the new
			   start and end events for each cpu */
			if (kf_zoom_line < adj_val) {
			    new_start_val = kf_zoom_line;
			    new_end_val = (int) adj_val;
			}
			else {
			    new_start_val = (int) adj_val;
			    new_end_val= kf_zoom_line;
			}
#ifdef INSTANTANEOUS_ZOOM
			kf_width_a = new_end_val - new_start_val;
			kf_start_val_a = new_start_val;
			kf_readjust_start_and_end();

#endif
			kf_zoom_line = -1;
			zoom_state = Z_NO_ZOOM;
		    }
		    else {
			kf_zoom_line = (int)adj_val;
			zoom_state = Z_LOCATING_ZOOM;
		    }
		}
	    }


/*
	if ((temp_pos == gr_numb_cpus) &&
*/
	if((call_data->event->type == ButtonPress) &&
	    (call_data->event->xbutton.button == Button2)){
		/* user wants a time line */
		if ((x_per < KF_X_START_P) || (x_per > KF_X_END_P)) {
			kf_time_line_begin = kf_time_line_end = -1;
			kf_cursor_time = -1;
			return;
		}
		else {
			if((kf_time_line_begin >= 0) &&
			   (kf_time_line_end   >= 0)) {
				kf_time_line_begin =
					kf_time_line_end = -1;
			}

			if(kf_time_line_begin < 0) {
				kf_time_line_begin = (int) adj_val;
			}
			else
				if((int) adj_val < kf_time_line_begin)
					kf_time_line_begin =
						kf_time_line_end = -1;
				else
					kf_time_line_end = (int) adj_val;
		}
#ifdef DEBUG
sprintf(msg_str, "get_mouse_pos_KF: time line begin %d end %d",kf_time_line_begin,
kf_time_line_end);
		grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
	}
	
	break;


      case ButtonRelease:
	x = call_data->event->xbutton.x;
	y = call_data->event->xbutton.y;
	XtVaGetValues(kf_graph.glWidget, XmNwidth, &dim_width, 
		      XmNheight, &dim_height, NULL);

	width = (int) dim_width;
	height = (int) dim_height;
	x_per = ((float) x)/((float) width);
	y_per = 1.0 - ((float) y)/((float) height);
	
	if ((x_per < KF_X_START_P) || (x_per > KF_X_END_P)) {
	    return;
	}

	y_abs = (kf_height_a-(kf_height_a*y_per))-TOP_BORDER_KF;
	temp_pos = y_abs/(2*HEIGHT);

	scale_per = (x_per - KF_X_START_P)/(KF_X_END_P - KF_X_START_P);
        adj_val = (scale_per*kf_width_a)+kf_start_val_a;

	if ((y_abs >= ((temp_pos+1)*2*HEIGHT - (0.5+RECT_WIDTH)*HEIGHT)) &&
	    (y_abs <= ((temp_pos+1)*2*HEIGHT - (0.5-RECT_WIDTH)*HEIGHT)))
	    cpu_pos = temp_pos;
	else
	    cpu_pos = -1;

#ifdef DEBUG
sprintf(msg_str, "get_mouse_pos_KF (REL): W %d H %d scale_per %f adj_val %7.1f cpu_pos %d",
width,height,scale_per,adj_val,cpu_pos);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

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

/*
	if ((temp_pos == gr_numb_cpus) &&
*/
	    if((call_data->event->type == ButtonRelease) &&
	    (call_data->event->xbutton.button == Button1)){


		/* user clicked in bottom scale and wants to zoom */
		if ((x_per < KF_X_START_P) || (x_per > KF_X_END_P)) {
		    kf_zoom_line = -1;
		    zoom_state = Z_NO_ZOOM;
		    kf_cursor_time = -1;
		    return;
		}
		else {
		    if ((kf_zoom_line >= 0) && ((int) adj_val > kf_zoom_line)){
			/* we have to perform a zoom: figure out the new
			   start and end events for each cpu */
			if (kf_zoom_line < adj_val) {
			    new_start_val = kf_zoom_line;
			    new_end_val = (int) adj_val;
			}
			else {
			    new_start_val = (int) adj_val;
			    new_end_val= kf_zoom_line;
			}
#ifdef INSTANTANEOUS_ZOOM
			kf_width_a = new_end_val - new_start_val;
			kf_start_val_a = new_start_val;
			kf_readjust_start_and_end();

			kf_zoom_line = -1;
#endif
			zoom_new_width = new_end_val - new_start_val;
			zoom_new_start = new_start_val;
			zoom_width_change = kf_width_a - zoom_new_width; /*>0*/
			zoom_start_change = new_start_val-kf_start_val_a;/*>=0*/
			zoom_state = Z_IMPLEM_ZOOM;

		    }
		    else {
/*
			kf_zoom_line = (int)adj_val;
*/
			if((kf_zoom_line >= 0) &&
			   ((int) adj_val == kf_zoom_line)){
				kf_cursor_time = (int) adj_val;
#ifdef DEBUG
sprintf(msg_str, "ZOOM: SAME PLACE!!setting kf_cursor_time = %d",kf_cursor_time);
				grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
			}
			zoom_state = Z_NO_ZOOM;
		    }
		}
	    }

	break;		/* case ButtonRelease */

      default:
	break;
    }

}


void
set_slider_kf_width_and_loc()
{
    int size_val;
    int range_val;
    char msg_str[64];


    if (kf_disp_mode == WIDE) {
	size_val =
/*
	     (int)(0.99999999+KF_RANGE*((float)kf_width_a/(float)gr_max_time));
*/
	   (int)(0.99999999+KF_RANGE*((float)kf_width_a/(float)get_max_time()));
    }
    else {
	size_val =
/*
	   (int)(0.99999999+KF_RANGE*((float)kf_width_a/(float)(kf_focus_max)));
*/
	   (int)(0.99999999+KF_RANGE*((float)kf_width_a/(float)get_max_time()));
    }
    if (size_val < 1) 
	size_val = 1;
    if (size_val > (int)KF_RANGE) 
	size_val = (int)KF_RANGE;

/*
    range_val = (int) range_kf;
*/
    range_val =
       (int)(KF_RANGE*((float)kf_start_val_a/(float)get_max_time()));

    if ((range_val + size_val) > (int)KF_RANGE) {
      range_val = (int)(KF_RANGE - size_val);
    }

    XmScaleSetValue(scroll_kf, 0);
    XtVaSetValues(scroll_kf, XmNsliderSize, 1, NULL);

    XmScaleSetValue(scroll_kf, range_val);
    XtVaSetValues(scroll_kf, XmNsliderSize, size_val, NULL);

#ifdef DEBUG
sprintf(msg_str, "set_slider_kf_width_and_loc: size_val %d range_val %d thumb %d",
size_val,range_val,(int)((size_val*THUMB_KF_MAX)/KF_RANGE));
    grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

    if (kf_disp_mode == WIDE) {
/*
	XtVaSetValues(thumbWheel_kf, XmNvalue, kf_width_a, NULL);
*/
	XtVaSetValues(thumbWheel_kf, XmNvalue,
			(int)((size_val*(float)THUMB_KF_MAX)/KF_RANGE), NULL);
    }

}

/*
 * getCursorPosition - get cursor position (time and y position)
 */

void getCursorPosition(int *time_pos, int *y_pos)
{
	int             dx, dy;
	Window          rep_root, rep_child;
	int             rep_rootx, rep_rooty;
	unsigned int    rep_mask;
	float x_per, y_per;
	float scale_per;
	float adj_val;
	float y_abs;
	Dimension dim_width,dim_height;
	int width,height;
    char msg_str[64];


	XQueryPointer (kf_graph.display, kf_graph.window, &rep_root, &rep_child,
		       &rep_rootx, &rep_rooty, &dx, &dy, &rep_mask);

#ifdef DEBUG
sprintf(msg_str, "getCursorPosition: dx = %d  dy = %d", dx, dy);
	grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

        XtVaGetValues(kf_graph.glWidget, XmNwidth, &dim_width,
                      XmNheight, &dim_height, NULL);

        width = (int) dim_width;
        height = (int) dim_height;

        x_per = ((float) dx)/((float) width);
        y_per = 1.0 - ((float) dy)/((float) height);

/*
        if ((x_per < KF_X_START_P) || (x_per > KF_X_END_P)) {
            return;
        }
*/

        y_abs = (kf_height_a-(kf_height_a*y_per))-TOP_BORDER_KF;
/*
        temp_pos = y_abs/(2*HEIGHT);
*/

        scale_per = (x_per - KF_X_START_P)/(KF_X_END_P - KF_X_START_P);
        adj_val = (scale_per*kf_width_a)+kf_start_val_a;

	if(time_pos != NULL)
		*time_pos = (int)adj_val;

	if(y_pos != NULL)
		*y_pos = (int) y_abs;

#ifdef DEBUG
sprintf(msg_str, "getCursorPosition: time %d y %d", (int)adj_val,(int) y_abs);
	grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
}

/*
 * get_cpu_by_y - get cpu number given Y position of cursor
 */

int get_cpu_by_y(int y_pos)
{
	int temp_pos;
	int cpu_pos;
    char msg_str[64];

        temp_pos = y_pos/(2*HEIGHT);

        if ((y_pos >= ((temp_pos+1)*2*HEIGHT - (0.5+RECT_WIDTH)*HEIGHT)) &&
            (y_pos <= ((temp_pos+1)*2*HEIGHT - (0.5-RECT_WIDTH)*HEIGHT)))
            cpu_pos = temp_pos;
        else
            cpu_pos = -1;

	if(cpu_pos >= gr_numb_cpus){
static int xxx = 0;
if(xxx == 0) {
   sprintf(msg_str, "get_cpu_by_y: cpu_pos = %d >= gr_numb_cpus (%d); setting to -1",
   cpu_pos,gr_numb_cpus);
grtmon_message(msg_str, DEBUG_MSG, 100);}
xxx = 1;
	cpu_pos = -1;
}

	return cpu_pos;
}

int testing(Widget w, XtPointer clientData, XEvent *event, Boolean *cont)
{
    char msg_str[64];
#ifdef DEBUG
	sprintf(msg_str, "\t\t\ttesting ev %d",event->type);
	grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
    return(0);
}


/* ===================================================================== */

/*
 * THE HELP WINDOW
 */

/*
 * help_using_CB: callback when help button is pressed
 */

Widget TopHelpTextShell;

void help_using_CB(Widget w, XtPointer clientD, XtPointer callD)
{
	static int varset = 0;
	static Widget textDialog;
	static Widget helpScrolledText;

	int n;
	Arg args[20];
	char tempChar[255];
	Atom wm_delete_window;
	char *s;

	if(!varset) {		/* initialize if it hasn't been already */
		varset++;

		textDialog = XtVaCreateWidget ("help text",
			xmDialogShellWidgetClass, toplevel,
		XmNtitle, "Help: Using the tool",
		XmNdeleteResponse, XmUNMAP,
		XmNx, 15,
		XmNy, 15,
		NULL);

		wm_delete_window = XmInternAtom(XtDisplay(textDialog),
						"WM_DELETE_WINDOW",
						FALSE);

		XmAddWMProtocolCallback (textDialog, wm_delete_window,
					close_help_using_CB, NULL);


		TopHelpTextShell = XtVaCreateWidget ("help text shell",
					xmFormWidgetClass, textDialog,
					NULL);

		n = 0;
		XtSetArg (args[n], XmNwidth, HELP_WINDOW_WIDTH); n++;
		XtSetArg (args[n], XmNheight, HELP_WINDOW_HEIGHT); n++;

		XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;

		XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
		XtSetArg(args[n], XmNeditable, False); n++;
		XtSetArg(args[n], XmNallowResize, True); n++;
		helpScrolledText = XmCreateScrolledText(TopHelpTextShell,
							"text", args, n);

		XmTextSetString(helpScrolledText, "TESTING!!");
		s = get_help_as_string();
		if(s != NULL) {
			XmTextSetString(helpScrolledText, s);
			free(s);
		}
		else
			XmTextSetString(helpScrolledText, "Internal error!");
	}
	XtManageChild(TopHelpTextShell);
	XtManageChild(helpScrolledText);
}


/*
 * close_help_using_CB
 */

void close_help_using_CB(Widget w, XtPointer clientD, XtPointer callD)
{
	XtUnmanageChild(TopHelpTextShell);
}

/*
 * get_help_as_string - returns the help file as one string; caller
 *	must free memory of the returned string
 */


char *get_help_as_string()
{
	FILE *fp;
	int fd;
	struct stat st;
	char *errorstring = strdup("Sorry, no help file available");
	char *retstr;
	char str[2048];
	char msg_str[64];

	if(errorstring == NULL){
		fprintf(stderr, "No memory for strdup\n");
		return NULL;
	}

	if((fp = fopen(HELP_FILE_NAME, "r")) == NULL){
		perror(HELP_FILE_NAME);
		sprintf(str,"%s/%s", INSTALL_DIR, HELP_FILE_NAME);
		sprintf(msg_str, "Trying %s", str);
		grtmon_message(msg_str, DEBUG_MSG, 100);
		if((fp = fopen(str, "r")) == NULL){
			perror(str);
			return errorstring;
		}
	}

 	/* get size, with size get memory */
	if((fd = fstat(fileno(fp), &st)) == -1){
		perror(HELP_FILE_NAME);
		return errorstring;
	}
	if(st.st_size <= 0){
		fprintf(stderr,"Empty help file\n");
		return errorstring;
	}
	if((retstr = malloc(st.st_size)) == NULL) {
		fprintf(stderr,"No memory!\n");
		return errorstring;
	}
	retstr[0] = '\0';
	
	while(fgets(str, 2047, fp) != NULL){
		strcat(retstr, str);
	}

	fclose(fp);

	return retstr;
}


/* =================================================================== */

/*
 * Find function
 */

Widget findFunctionWindow;
Widget nameText, cpuText;
Widget findNextButton, findFromBegButton, findFromEndButton;
Widget doneButton;

enum function_search { FS_LAST_POSITION, FS_BEGINNING, FS_END};
int last_search_cpu = -1;	/* cpu of last search */
int last_search_funcno;		/* function number of last search */
int last_search_index;

int search_found_cpu = -1;
int search_found_time;

void seek_to_function_common(int cpu, char funcname[],
                             enum function_search type);


void find_function_CB(Widget w, XtPointer clientD, XtPointer callD)
{
	static int varset = 0;
	static Widget findfuncDialog;
	Atom wm_delete_window;

	static Widget cpuLabel, nameLabel;
	static Widget ButtonArea;
	Widget separator0, separator1, separator2;
	XmString tempString;

	if (!varset) {
		varset++;

		findfuncDialog = XtVaCreateWidget ("Find function",
			xmDialogShellWidgetClass, toplevel,
			XmNtitle, "Find function",
			XmNdeleteResponse, XmUNMAP,
			XmNx, 15,
			XmNy, 15,
			NULL);

		wm_delete_window = XmInternAtom(XtDisplay(findfuncDialog),
                                        "WM_DELETE_WINDOW",
                                        FALSE);

		XmAddWMProtocolCallback (findfuncDialog, wm_delete_window,
					close_find_function_CB, NULL);

		findFunctionWindow = XtVaCreateWidget ("find funct window",
				xmFormWidgetClass, findfuncDialog,
				NULL);

		separator0 = XtVaCreateManagedWidget ("sep0",
				xmSeparatorWidgetClass, findFunctionWindow,
				XmNtopAttachment, XmATTACH_FORM,
				XmNorientation, XmHORIZONTAL,
				XmCSeparatorType, XmNO_LINE,
				XmNleftAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNheight, 20,
				NULL);

		nameLabel = XtVaCreateManagedWidget ("Function name",
				xmLabelWidgetClass, findFunctionWindow,
				XmNx, 2,
				XmNy, 25,
				NULL);

		nameText = XtVaCreateManagedWidget ("name text",
				xmTextFieldWidgetClass, findFunctionWindow,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, separator0,
				XmNleftAttachment, XmATTACH_WIDGET,
				XmNleftWidget, nameLabel,
				XmNwidth, 250,
/*
				XmNvalue, "1",
*/
				NULL);

		separator1 = XtVaCreateManagedWidget ("sep1",
				xmSeparatorWidgetClass, findFunctionWindow,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNorientation, XmHORIZONTAL,
				XmCSeparatorType, XmSHADOW_ETCHED_IN,
				XmNtopWidget, nameText,
				XmNleftAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNheight, 20,
				NULL);
 
		cpuLabel = XtVaCreateManagedWidget ("On CPU number:",
				xmLabelWidgetClass, findFunctionWindow,
				XmNx, 2,
				XmNy, 78,
				XmNleftAttachment, XmATTACH_FORM,
/*
				XmNrightAttachment, XmATTACH_FORM,
*/
				NULL);


		cpuText = XtVaCreateManagedWidget ("cpu text",
				xmTextFieldWidgetClass, findFunctionWindow,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, separator1,
				XmNleftAttachment, XmATTACH_WIDGET,
				XmNleftWidget, cpuLabel,
				XmNwidth, 238 /* 220 */,
				XmNvalue, "0",
				NULL);

		separator2 = XtVaCreateManagedWidget ("sep2",
				xmSeparatorWidgetClass, findFunctionWindow,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNorientation, XmHORIZONTAL,
				XmCSeparatorType, XmSHADOW_ETCHED_IN,
				XmNtopWidget, cpuText,
				XmNleftAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNheight, 20,
				NULL);

		ButtonArea = XtVaCreateManagedWidget ("action_area",
				xmFormWidgetClass, findFunctionWindow,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, separator2,
				XmNleftAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				NULL);

		tempString = XmStringCreateSimple ("Find next");
		findNextButton = XtVaCreateManagedWidget ("find next",
				xmPushButtonWidgetClass, ButtonArea,
				XmNlabelString, tempString,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, separator2,
				XmNleftAttachment, XmATTACH_FORM,
				XmNdefaultButtonShadowThickness, 1,
				NULL);
		XmStringFree (tempString);
		XtAddCallback (findNextButton, XmNactivateCallback,
				find_next_function_button_CB, NULL);

		tempString = XmStringCreateSimple ("Find from Beginning");
		findFromBegButton = XtVaCreateManagedWidget ("findfrombeg",
				xmPushButtonWidgetClass, ButtonArea,
				XmNlabelString, tempString,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, findNextButton,
				XmNleftAttachment, XmATTACH_FORM,
				XmNdefaultButtonShadowThickness, 1,
				NULL);
		XmStringFree (tempString);
		XtAddCallback (findFromBegButton, XmNactivateCallback,
				find_from_beginning_button_CB, NULL);

		tempString = XmStringCreateSimple ("Done");
		doneButton = XtVaCreateManagedWidget ("done",
				xmPushButtonWidgetClass, ButtonArea,
				XmNlabelString, tempString,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, findFromBegButton,
/*
				XmNleftAttachment, XmATTACH_POSITION,
				XmNleftPosition, HELP_WINDOW_WIDTH/2,
				XmNfractionBase, HELP_WINDOW_WIDTH,
*/
				XmNleftAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
/*
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, gotodoneButton,
*/
				XmNdefaultButtonShadowThickness, 1,
				NULL);
		XmStringFree (tempString);
		XtAddCallback (doneButton, XmNactivateCallback,
				close_find_function_CB, NULL);


		tempString = XmStringCreateSimple ("Find from End");
		findFromEndButton = XtVaCreateManagedWidget ("findfromend",
				xmPushButtonWidgetClass, ButtonArea,
				XmNlabelString, tempString,
/*
				XmNleftAttachment, XmATTACH_WIDGET,
				XmNleftWidget, findFromBegButton,
*/
				XmNbottomAttachment, XmATTACH_WIDGET,
				XmNbottomWidget, doneButton,
				XmNrightAttachment, XmATTACH_FORM,
				XmNdefaultButtonShadowThickness, 1,
				NULL);
		XmStringFree (tempString);
		XtAddCallback (findFromEndButton, XmNactivateCallback,
				find_from_end_button_CB, NULL);



	}

	XtManageChild(findFunctionWindow);
}

/* 
 * close_find_function_CB: close "find function" window
 */

void close_find_function_CB(Widget w, XtPointer clientD, XtPointer callD)
{
	XtUnmanageChild(findFunctionWindow);
}

/*
 * find_next_function_button_CB: function called when button
 *	"next function" is pressed
 */

void find_next_function_button_CB(Widget w, XtPointer clientD, XtPointer callD)
{
	char funcname[256];
	int cpu;

	funcname[0] = '\0';

	obtain_cpu_and_funcname(&cpu, funcname);
	if(funcname[0] != '\0')
		seek_to_function(cpu, funcname);
}

/*
 * obtain_cpu_and_funcname: obtain CPU number and function name from what
 *	the user has typed
 */

void obtain_cpu_and_funcname(int *cpu, char *funcname)
{
	int i;
	char *s;

	i = 0;
	while (i<MAX_PROCESSORS) {
		if (cpu_on[i]) {
			*cpu = i;
			break;
		}
		i++;
	}

	if (findFunctionWindow != NULL) {
		*cpu = atoi(XmTextFieldGetString(cpuText));
		s = XmTextFieldGetString(nameText);
		if(s != NULL)
			strcpy(funcname, XmTextFieldGetString(nameText));
	}
}

/*
 * find_from_beginning_button_CB: find function from the beginning
 */

void find_from_beginning_button_CB(Widget w, XtPointer clientD,
				   XtPointer callD)
{
	char funcname[256];
	int cpu;

	funcname[0] = '\0';

	obtain_cpu_and_funcname(&cpu, funcname);
	if(funcname[0] != '\0')
		seek_to_function_from_beginning(cpu, funcname);
}

/*
 * find_from_end_button_CB: find function from the end
 */

void find_from_end_button_CB(Widget w, XtPointer clientD, XtPointer callD)
{
	char funcname[256];
	int cpu;

	funcname[0] = '\0';

	obtain_cpu_and_funcname(&cpu, funcname);
	if(funcname[0] != '\0')
		seek_to_function_from_end(cpu, funcname);
}

/*
 * search functions - call a common function to do the job
 */

void seek_to_function(int cpu, char funcname[])
{
    char msg_str[64];
sprintf(msg_str, "Seek function %s on cpu %d", funcname, cpu);
grtmon_message(msg_str, DEBUG_MSG, 100);
	seek_to_function_common(cpu, funcname, FS_LAST_POSITION);
}

void seek_to_function_from_beginning(int cpu, char funcname[])
{
    char msg_str[64];
sprintf(msg_str, "Seek function %s on cpu %d", funcname, cpu);
grtmon_message(msg_str, DEBUG_MSG, 100);
	seek_to_function_common(cpu, funcname, FS_BEGINNING);
}

void seek_to_function_from_end(int cpu, char funcname[])
{
    char msg_str[64];
sprintf(msg_str, "Seek function %s on cpu %d", funcname, cpu);
grtmon_message(msg_str, DEBUG_MSG, 100);
	seek_to_function_common(cpu, funcname, FS_END);
}

/*
 * seek_to_function_common - serach for a particular function
 */

void seek_to_function_common(int cpu, char funcname[],
			     enum function_search type)
{
	int funcno;
	int forward_flag;
	int start_search_index;
	int alternate_search_index = -1;
	int found_index = -1;
	int i;
	int start, end;
	int start_time;
    char msg_str[64];

	search_found_cpu = -1;
	
	if(cpu >= gr_numb_cpus) { /* XXX */
		sprintf(msg_str, "\007No data from cpu %d", cpu);
		grtmon_message(msg_str, DEBUG_MSG, 100);
		return;
	}

	if((funcno = funcname_to_funcno(funcname)) < 0){
		/* maybe function "exists", but could still not be in the
		   data base if an event record from this function has not
		   appeared on any screen */
		for(i=0; i< MAXFUNCTIONS; i++)
			if((!strcmp(kern_funct_name_map[i], funcname)) &&
			   (funct_info[i].valid_flag == 0)) {
				funct_info_insert(i);
				break;
			}
		if(i < MAXFUNCTIONS)
			funcno = i;
		else {
			sprintf(msg_str, "\007No %s() on log file",funcname);
			grtmon_message(msg_str, DEBUG_MSG, 100);
			return;
		}
	}

	switch(type) {
		case FS_BEGINNING:		/* start from beginning */
			forward_flag = 1;
			start_search_index = 1;
			break;

		case FS_END:			/* start from end */
			forward_flag = 0;
			start_search_index =
				    graph_info[cpu].no_frame_event_count - 1;
			break;

		case FS_LAST_POSITION:
			forward_flag = 1;

			/* if tried before, remember where last successful
			   search ended, otherwise prepare 2 searches: one
			   in the beginning of the window and another
			   (if the 1st fails) from the beginning of the file */

			if((funcno == last_search_funcno) &&
			   (cpu == last_search_cpu) &&
			    (last_search_index != -1)){
				start_search_index = last_search_index;
sprintf(msg_str, "seek_to_function_common: using last indx %d", start_search_index);
				grtmon_message(msg_str, DEBUG_MSG, 100);
			}
			else {
				start_search_index =
						graph_info[cpu].nf_start_event;
				alternate_search_index = 1;
			}
			break;

		default:
			assert(0);
	}


	while(1) {
		if(forward_flag) {
			end = graph_info[cpu].no_frame_event_count - 1;
sprintf(msg_str, "seek_to_function_common: fwd search from indx %d to %d",
start_search_index, end);
			grtmon_message(msg_str, DEBUG_MSG, 100);
			for(i=start_search_index; i<= end; i++){
				if(is_kernel_function_entry(cpu,i) &&
				   (get_kernel_function_number(cpu,i)==funcno))
					break;
			}
			if(i <= end) {	/* found */
				found_index = i;
				last_search_index = i + 1;
			}
		}
		else {
			start = 1;
			for(i=start_search_index; i>= start; i--){
				if(is_kernel_function_entry(cpu,i) &&
				   (get_kernel_function_number(cpu,i)==funcno))
					break;
			}
			if(i >= start) {	/* found */
				found_index = i;
				last_search_index = i - 1;
			}
		}

		if(found_index >= 0)
			break;

#ifdef DEBUG
sprintf(msg_str, "seek_to_function_common: alternate_search_index %d",
alternate_search_index);
		grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

		if(alternate_search_index != -1){
			start_search_index = alternate_search_index;
			alternate_search_index = -1;
#ifdef DEBUG
sprintf(msg_str, "seek_to_function_common: using alternate indx %d",start_search_index);
			grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
		}
		else
			break;
	}

	if(found_index < 0) {
		sprintf(msg_str, "\007Cannot find %s()", funcname);
		grtmon_message(msg_str, DEBUG_MSG, 100);
		last_search_index = -1;
		last_search_cpu = -1;
		return;
	}

	start_time = graph_info[cpu].no_frame_event[found_index].start_time;

	last_search_cpu = cpu;
	last_search_funcno = funcno;
	search_found_cpu = cpu;
	search_found_time = start_time;

	sprintf(msg_str, "Found %s() at time %s", funcname,
						us_to_valunit_str(start_time));
	grtmon_message(msg_str, DEBUG_MSG, 100);

	/* move window if function is not on current window */
	if((start_time < kf_start_val_a) ||
	   (start_time > kf_start_val_a + kf_width_a)){
		kf_start_val_a = start_time - kf_width_a/2;
					/* center window on function start */
		if(kf_start_val_a < 0)
			kf_start_val_a = 0;
		kf_readjust_start_and_end();
	}
#ifdef DEBUG
	else {
		sprintf(msg_str, "seek_to_function_common: [%d - %d - %d]",
		kf_start_val_a, start_time, kf_start_val_a + kf_width_a); 
		grtmon_message(msg_str, DEBUG_MSG, 100);
	}
#endif
}


