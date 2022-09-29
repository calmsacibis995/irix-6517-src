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
 * File:	graph-main-cb.c           			          *
 *									  *
 * Description:	This file handles the scrolling text for grtmon  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

void binary_search(int cpu, int search_val, int is_start_val);

void 
old_thumbCB (Widget w, XtPointer clientD, XtPointer callD)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct *) callD;
    int *scale = (int *)clientD;

    if (no_frame_mode) {
	if (cbs->value == (A_THUMB - 1)) {
	    nf_start_val_a = 0;
	    nf_width_a = nf_max_val_a;
	    XtVaSetValues(thumbWheel_a, SgNhomePosition, A_THUMB - 2, NULL);
	}
	else if (cbs->value == (A_THUMB - 2)) {
	    nf_start_val_a = 0;
	    nf_width_a = nf_max_val_a;
	    XtVaSetValues(thumbWheel_a, SgNhomePosition, A_THUMB - 1, NULL);
	}
	else {
	    nf_width_a = (long long) (((float)cbs->value/(float)A_THUMB)*nf_max_val_a);
	}
	readjust_nf_start_and_end(1);
	
    }
    else {
	if (cbs->value == (A_THUMB - 1)) {
	    width_a = max_val_a;
	    XtVaSetValues(thumbWheel_a, SgNhomePosition, A_THUMB - 2, NULL);
	}
	else if (cbs->value == (A_THUMB - 2)) {
	    width_a = max_val_a;
	    XtVaSetValues(thumbWheel_a, SgNhomePosition, A_THUMB - 1, NULL);
	}
	else {
	  width_a =  (int) (((float)cbs->value/(float)A_THUMB)*max_val_a);
	}
	set_slider_a_width_and_loc();
    }
}



void 
scroll_a_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    float max_val;

    XmScaleCallbackStruct *call_value = (XmScaleCallbackStruct *) callD;
    if (no_frame_mode) {
	if (call_value->value != 0)
	    nf_start_val_a = (long long) (((float)(call_value->value+1)/MAX_SLIDER_A)*nf_max_val_a);
	else
	    nf_start_val_a = (long long) (((float)call_value->value/MAX_SLIDER_A)*nf_max_val_a);
	readjust_nf_start_and_end(0);
    }
    else {
	max_val =(float)((int)(100.0-(100.0*(float)width_a/(float)max_val_a)));
	/*rang = 100.0*(float)call_value->value/max_val;*/
	start_val_a = (int) (((float)(call_value->value))/100.0 * (float)max_val_a);
	if (call_value->value == (int)max_val)
	    start_val_a = max_val_a - width_a;
    }
}



#define START_PER_A (LEFT_BORDER_A/SCALE_FACT_A)
#define END_PER_A ((1.0/SCALE_FACT_A) + (LEFT_BORDER_A/SCALE_FACT_A))
#define MAX_X_ERR_A 0.01

#define NF_START_PER (NF_LEFT_BORDER/NF_SCALE_FACT)
#define NF_END_PER ((1.0/NF_SCALE_FACT) + (NF_LEFT_BORDER/NF_SCALE_FACT))

void
proc_key_down_A (Widget w, XtPointer clientD, XEvent *event, Boolean *unused)
{

    KeySym key;


    switch(event->type) {
      case KeyPress:
	  break;
      default:
	  ;
	  break;
   }
}

void
binary_search(int cpu, int search_val, int is_start_val)
{
    int temp_start, temp_end;

    temp_start = 0;
    temp_end = graph_info[cpu].no_frame_event_count;

    while (temp_start < (temp_end - 1)) {
	if (graph_info[cpu].no_frame_event[(temp_start+temp_end)/2].start_time <= search_val)
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
	if (graph_info[cpu].nf_end_event >= graph_info[cpu].no_frame_event_count)
	    graph_info[cpu].nf_end_event = graph_info[cpu].no_frame_event_count-1;
    }
}

/* this will take new new times and readjust for all cpus
   what the start and end event is */
void
readjust_nf_start_and_end(int print_it)
{
    int i,cpu;

    nf_max_val_a = 0;
    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	cpu = gr_cpu_map[i];
	if (graph_info[cpu].no_frame_event[graph_info[cpu].no_frame_event_count-1].end_time > nf_max_val_a)
	    nf_max_val_a = graph_info[cpu].no_frame_event[graph_info[cpu].no_frame_event_count-1].end_time;
    }

    if ((nf_start_val_a + nf_width_a) > nf_max_val_a)
	nf_start_val_a = nf_max_val_a - nf_width_a;
    if (nf_start_val_a < 0) 
	nf_start_val_a = 0;
    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	if (cpu_on[gr_cpu_map[i]]) {
	    cpu = gr_cpu_map[i];
	  try_again:
	    binary_search(cpu, nf_start_val_a, 1);
	    binary_search(cpu, nf_start_val_a+nf_width_a, 0);
	    if ((graph_info[cpu].nf_end_event -  graph_info[cpu].nf_start_event > MAX_NUMBER_NF_EVENTS) && (no_frame_mode)) {
		if (print_it) {
		    grtmon_message("You don't really want that many events, you won't be able to see anything.  I'll reduce the range for you.", ALERT_MSG, 27);
		    print_it = 0;
		}
		nf_width_a = nf_width_a - (long long) (0.1 * (float)nf_width_a);
		goto try_again;
	    }
	}
    }

/*    
    nf_width_a = 0.0;
    nf_start_val_a = 0x7ffffff;
    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	if (cpuOn[i]) {
	    cpu = gr_cpu_map[i];
	    width = (float) 
		(graph_info[cpu].no_frame_event[graph_info[cpu].nf_end_event].end_time -
		 graph_info[cpu].no_frame_event[graph_info[cpu].nf_start_event].start_time);
	    if (width > nf_width_a)
		nf_width_a = width;
	    if (graph_info[cpu].no_frame_event[graph_info[cpu].nf_start_event].start_time < nf_start_val_a) 
		nf_start_val_a = graph_info[cpu].no_frame_event[graph_info[cpu].nf_start_event].start_time;
	}
    }
*/
    set_slider_a_width_and_loc();


}


void
get_mouse_pos_A (Widget w, XtPointer clientD, XtPointer callD)
{
    int x,y,i,ii,j,k;
    Dimension dim_width,dim_height;
    int width,height;
    float start_val;
    XmDrawingAreaCallbackStruct *call_data;
    float x_per, y_per;
    float scale_per;
    float adj_val;
    float y_abs;
    int cpu_pos, cpu_numb, cpu, temp_pos;
    int minor;
    int cur_time;
    KeySym key;
    long long new_start_val, new_end_val;
    int button;

    call_data = (XmDrawingAreaCallbackStruct *) callD;

    switch(call_data->event->type) {
      case ButtonRelease:
	/* the old code this is for holding a button */
/*	if (gr_last_show.cpu != -1) {
	    graph_info[gr_last_show.cpu].ave_minor[gr_last_show.minor].process[gr_last_show.process].show_time = FALSE;
	    gr_last_show.cpu = -1; 
	}
	break;*/
	/* the new code allows the fact that if you move and are no
	   longer over the spot then the label stays - oooh exciting !!! */
	/* just fall through and use same code */
      case ButtonPress:
	x = call_data->event->xbutton.x;
	y = call_data->event->xbutton.y;
	button = call_data->event->xbutton.button;

	XtVaGetValues(main_graph.glWidget, XmNwidth, &dim_width, 
		      XmNheight, &dim_height, NULL);

	width = (int) dim_width;
	height = (int) dim_height;
	x_per = ((float) x)/((float) width);
	y_per = 1.0 - ((float) y)/((float) height);

	y_abs = (width_ay-(width_ay*y_per))-TOP_BORDER_A;
	temp_pos = y_abs/(2*HEIGHT);


	if (no_frame_mode) {
	    /* we have to do something entirely different here since
	       we display from event x to event y for no frame mode
	       plus we don't have any frames to edal with as the below
	       code assumes */

	    scale_per = (x_per - NF_START_PER)/(NF_END_PER - NF_START_PER);
	    adj_val = (scale_per*nf_width_a)+nf_start_val_a;


	    if ((((float)(width_ay - (int)(1.5*(float)HEIGHT)) - y_abs) < HEIGHT) &&
		(temp_pos != gr_numb_cpus)) {
		
		/* user clicked in between scale and bottom cpu
		   and wants to get a time line */
		if ((x_per < NF_START_PER) || (x_per > NF_END_PER)) {
		    scan_line_m = -1;
		    scan_line_l = -1;
		    scan_line_r = -1;
		    return;
		}
		else {
		    if (button == 1) {
			scan_line_m = -1;
			scan_line_l = (long long)adj_val;
		    }
		    if (button == 2) {
			scan_line_l = -1;
			scan_line_r = -1;
			if (call_data->event->type == ButtonRelease)
			    scan_line_m = -1;
			else {
			    scan_line_m = (long long)adj_val;
			}
		    }
		    if (button == 3) {
			scan_line_m = -1;
			scan_line_r = (long long)adj_val;
		    }
		}
		return;
	    }

	    if ((temp_pos == gr_numb_cpus) && (call_data->event->type == ButtonPress)){
		/* user clicked in bottom scale and wants to zoom */
		if ((x_per < NF_START_PER) || (x_per > NF_END_PER)) {
		    zoom_line_l = -1;
		    zoom_line_r = -1;
		    return;
		}
		else {
		    if (button == 1) {
			zoom_line_l = (long long)adj_val;
		    }
		    if (button == 3) {
			zoom_line_r = (long long)adj_val;
		    }
		    else {
			if ((zoom_line_l >= 0) && (zoom_line_r >= 0)) {
			    /* we have to perform a zoom, in nf mode that means
			       figuring out the new start and end events for each
			       cpu */
			    if (zoom_line_l < zoom_line_r) {
				new_start_val = zoom_line_l;
				new_end_val = (long long) zoom_line_r;
			    }
			    else {
				new_start_val = (long long) zoom_line_r;
				new_end_val= zoom_line_l;
			    }
			    nf_width_a = new_end_val - new_start_val;
			    if (nf_width_a < width_a_min) {
				nf_width_a = width_a_min;
			    }
			    nf_start_val_a = new_start_val;
			    readjust_nf_start_and_end(1);
			    
			    zoom_line_l = -1;
			    zoom_line_r = -1;
			    
			}
		    }
		}
	    }
	    return;

	}

	scale_per = (x_per - START_PER_A)/(END_PER_A - START_PER_A);
	start_val = (float)start_val_a;
	adj_val = (scale_per*width_a)+start_val;
	
	/* commented out this functionality is now correctly provided with scan line
	   user clicked in between scale and bottom cpu
	   and wants to get a time line 
	if ((((float)(width_ay - (int)(1.5*(float)HEIGHT)) - y_abs) < HEIGHT) &&
	    (temp_pos != gr_numb_cpus) &&
	    (call_data->event->type == ButtonPress)) {
	    if ((x_per < NF_START_PER) || (x_per > NF_END_PER)) {
		scan_line = -1;
		return;
	    }
	    else {
		scan_line = (long long)adj_val;
	    }
	    return;
	}
	commented out this functionality is now correctly provided with scan line */

	if ((temp_pos == gr_numb_cpus) && (call_data->event->type == ButtonPress)){
	    /* user clicked in bottom scale and wants to zoom */
	    if ((x_per < START_PER_A) || (x_per > END_PER_A)) {
		zoom_line_l = -1;
		zoom_line_r = -1;
		return;
	    }
	    else {
		if (button == 1) {
		    zoom_line_l = (long long)adj_val;
		}
		if (button == 3) {
		    zoom_line_r = (long long)adj_val;
		}
		else {
		    if ((zoom_line_l >= 0) && (zoom_line_r >= 0)) {

			width_a = abs(zoom_line_l - zoom_line_r);
			if (width_a < width_a_min) width_a = width_a_min;
			
			if (zoom_line_l < zoom_line_r) {
			    start_val_a = zoom_line_l;
			}
			else {
			    start_val_a = zoom_line_r;
			}

			zoom_line_l = -1;
			zoom_line_r = -1;
			
			set_slider_a_width_and_loc();
		    }
		}
	    }
	}


	if ((x_per < START_PER_A) || (x_per > END_PER_A))
	    return;


	if ((y_abs >= ((temp_pos+1)*2*HEIGHT - HEIGHT - (0.5+RECT_WIDTH)*HEIGHT)) &&
	    (y_abs <= ((temp_pos+1)*2*HEIGHT - HEIGHT - (0.5-RECT_WIDTH)*HEIGHT)))
	    cpu_pos = temp_pos;
	else {
	    if (button == 1) {
		scan_line_m = -1;
		scan_line_l = (long long)adj_val;
	    }
	    if (button == 2) {
		scan_line_l = -1;
		scan_line_r = -1;
		if (call_data->event->type == ButtonRelease)
		    scan_line_m = -1;
		else {
		    scan_line_m = (long long)adj_val;
		}
	    }
	    if (button == 3) {
		scan_line_m = -1;
		scan_line_r = (long long)adj_val;
	    }
	    cpu_pos = -1;
	}

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

	/* This is a lot of work :-)
	   I have the cpu the user has clicked on, now we need to 
	   first determine the minor frame and then possibly display
	   the time for that process, for cuteness let's do assume
	   the idle process is at the end of the minor */
	
	minor = (int)adj_val/major_frame_account[cpu].minor_time;
	/* now step through minor and see which proces it is */

	cur_time = minor*major_frame_account[cpu].minor_time;
	cur_time = cur_time + 
	    graph_info[cpu].ave_minor[minor].start_time;

	for (j=1;j<major_frame_account[cpu].numb_in_minor[minor];j++) {
	    if (adj_val <= (cur_time = cur_time + 
		   (graph_info[cpu].ave_minor[minor].process[j].ktime +
		    graph_info[cpu].ave_minor[minor].process[j].utime))) {
		if (call_data->event->type == ButtonRelease) {
                  graph_info[cpu].ave_minor[minor].process[j].show_time = FALSE;
		  gr_last_show.cpu = -1;
		  return;
	        }
		gr_last_show.cpu = cpu;
		gr_last_show.minor = minor;
		gr_last_show.process = j;
		if (graph_info[cpu].ave_minor[minor].process[j].show_time)
		    graph_info[cpu].ave_minor[minor].process[j].show_time = FALSE;
		else 
		    graph_info[cpu].ave_minor[minor].process[j].show_time = TRUE;
		j = major_frame_account[cpu].numb_in_minor[minor] + 1;
	    }
	}
	if (j==major_frame_account[cpu].numb_in_minor[minor]) {
	    /* intercept and see if we really clicked on a max time 
	     but only on a button press event */
	    if (call_data->event->type == ButtonPress) {	    
		cur_time = minor*major_frame_account[cpu].minor_time;
		if (graph_mode == SUMMARIZED) {
		    if (((adj_val - cur_time) > (graph_info[cpu].max_minor[minor].sum_val
						 - MAX_X_ERR_A*width_a)) &&
			((adj_val - cur_time) < (graph_info[cpu].max_minor[minor].sum_val
						 + MAX_X_ERR_A*width_a))) {
			goto_frame_number(graph_info[cpu].max_minor[minor].sum_maj_frame, cpu);
			return;
		    }
		}
		else {
		    if (((adj_val - cur_time) > (graph_info[cpu].max_minor[minor].ex_ev_val
						 - MAX_X_ERR_A*width_a)) &&
			((adj_val - cur_time) < (graph_info[cpu].max_minor[minor].ex_ev_val
						 + MAX_X_ERR_A*width_a))) {
			goto_frame_number(graph_info[cpu].max_minor[minor].ex_ev_maj_frame, cpu);
			return;
		    }
		}
	    }

	    if (call_data->event->type == ButtonRelease) {
		graph_info[cpu].ave_minor[minor].process[0].show_time = FALSE;
		gr_last_show.cpu = -1;
		return;
	    }
	    gr_last_show.cpu = cpu;
	    gr_last_show.minor = minor;
	    gr_last_show.process = 0;

	    if (graph_info[cpu].ave_minor[minor].process[0].show_time)
		graph_info[cpu].ave_minor[minor].process[0].show_time = FALSE;
	    else
		graph_info[cpu].ave_minor[minor].process[0].show_time = TRUE;
	}

	break;
      default:
	break;
    }

}


void
get_scan_line_val()
{
    int             x, y;
    Window          rep_root, rep_child;
    int             rep_rootx, rep_rooty;
    unsigned int    rep_mask;
    Dimension dim_width,dim_height;
    float x_per, start_val;
    float scale_per;
    float adj_val;

    XQueryPointer (main_graph.display, main_graph.window, &rep_root, &rep_child,
		   &rep_rootx, &rep_rooty, &x, &y, &rep_mask);

    XtVaGetValues(main_graph.glWidget, XmNwidth, &dim_width, 
		  XmNheight, &dim_height, NULL);
    

    x_per = ((float) x)/((float) dim_width);

    if (no_frame_mode) {
	scale_per = (x_per - NF_START_PER)/(NF_END_PER - NF_START_PER);
	adj_val = (scale_per*nf_width_a)+nf_start_val_a;
	if (x_per < NF_START_PER) {
	    scan_line_m = nf_start_val_a;
	}
	else if (x_per > NF_END_PER) {
	    scan_line_m = nf_start_val_a + nf_width_a;
	}
	else {
	    scan_line_m = (long long)adj_val;
	}
    }
    else {
	
	scale_per = (x_per - START_PER_A)/(END_PER_A - START_PER_A);
	start_val = (float)start_val_a;
	adj_val = (scale_per*width_a)+start_val;
	
	if (x_per < START_PER_A) {
	    scan_line_m = start_val;
	}
	else if (x_per > END_PER_A) {
	    scan_line_m = start_val + width_a;
	}
	else
	    scan_line_m = adj_val;
    }
}

void 
dec_scroll_a_CB (Widget w, XtPointer clientD, XtPointer callD)
{
 

    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a - (long long)(0.1*(float)nf_width_a);
	readjust_nf_start_and_end(0);
    }
    else {
	start_val_a = start_val_a - (int)((0.1*(float)width_a));
	if (start_val_a < 0)
	    start_val_a = 0;
	set_slider_a_width_and_loc();
    }

}

void 
inc_scroll_a_CB (Widget w, XtPointer clientD, XtPointer callD)
{
 
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a + (long long)(0.1*(float)nf_width_a);
	readjust_nf_start_and_end(0);
    }
    else {
	start_val_a = start_val_a + (int)((0.1*(float)width_a));

	if (start_val_a + width_a > max_val_a)
	    start_val_a = max_val_a - width_a;

	set_slider_a_width_and_loc();
    }


}


void
scroll_l1_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    if (no_frame_mode) {

    }
    else {

	set_slider_a_width_and_loc();
    }
} 


void
scroll_l2_CB (Widget w, XtPointer clientD, XtPointer callD)
{


    if (no_frame_mode) {

    }
    else {

	set_slider_a_width_and_loc();
    }

} 


void
scroll_l10_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a - (long long)(0.1*(float)nf_width_a);
	readjust_nf_start_and_end(0);
    }
    else {
	start_val_a = start_val_a - (int)(0.1*(float)nf_width_a);
	if (start_val_a < 0)
	    start_val_a = 0;
	set_slider_a_width_and_loc();
    }
} 


void
scroll_r1_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    if (no_frame_mode) {

    }
    else {

	set_slider_a_width_and_loc();
    }

} 


void
scroll_r2_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    if (no_frame_mode) {

    }
    else {

	set_slider_a_width_and_loc();
    }

} 


void
scroll_r10_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a + (long long)(0.1*(float)nf_width_a);
	readjust_nf_start_and_end(0);
    }
    else {

	start_val_a = start_val_a + (int)(0.1*(float)width_a);
	if (start_val_a + width_a > max_val_a) 
	    start_val_a = max_val_a - width_a; 
	set_slider_a_width_and_loc();
    }

} 

void
do_zoom()
{
    long long zoom_from, zoom_to;
    long long zoom_small, zoom_large;

    zoom_from = atoll(XmTextFieldGetString(ZoomFromText));
    zoom_to = atoll(XmTextFieldGetString(ZoomToText));

    if (zoom_from < zoom_to) {
	zoom_small = zoom_from;
	zoom_large = zoom_to;
    }
    else {
	zoom_small = zoom_to;
	zoom_large = zoom_from;
    }
    
    if (zoom_small < 0)
	zoom_small = 0;

    if (no_frame_mode) {
	nf_width_a = zoom_large - zoom_small;
	if (nf_width_a < width_a_min) {
	    nf_width_a = width_a_min;
	}
	nf_start_val_a = zoom_small;
	readjust_nf_start_and_end(1);
    }
    else {
	if (zoom_large > max_val_a)
	    zoom_large = max_val_a;
	width_a = zoom_large - zoom_small;
	if (width_a < width_a_min) 
	    width_a = width_a_min;
	start_val_a = zoom_small;
	if (start_val_a + width_a > max_val_a)
	    start_val_a = max_val_a - width_a;

	set_slider_a_width_and_loc();

    }
}

void 
close_zoom_control_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(ZoomWindow);

}


void 
zoom_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    do_zoom();

}


void 
zoom_done_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    do_zoom();
    XtUnmanageChild(ZoomWindow);

}


void
zoom_control_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    static int varset = 0;
    static Widget zoomDialog;
    Atom wm_delete_window;

    Widget ButtonArea, zoomButton, zoomdoneButton, doneButton;
    static Widget ZoomFromLabel, ZoomToLabel;
    Widget separator0, separator1, separator2;
    XmString tempString;


    if (!varset) {
	varset++;


	zoomDialog = XtVaCreateWidget ("Specify Zoom",
	    xmDialogShellWidgetClass, toplevel,
	    XmNtitle, "Specify Zoom",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(zoomDialog),
					"WM_DELETE_WINDOW",
					FALSE);

	XmAddWMProtocolCallback (zoomDialog, wm_delete_window, 
				 close_zoom_control_CB, NULL);


	ZoomWindow = XtVaCreateWidget ("zoom",
	    xmFormWidgetClass, zoomDialog,
	    NULL);



	separator0 = XtVaCreateManagedWidget ("sep0",
	    xmSeparatorWidgetClass, ZoomWindow,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmNO_LINE,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
            XmNheight, 20,
            NULL);

	ZoomFromLabel = XtVaCreateManagedWidget ("Zoom from: ",
	    xmLabelWidgetClass, ZoomWindow,
	    XmNx, 2,
	    XmNy, 25,
	    NULL);

	ZoomFromText = XtVaCreateManagedWidget ("Zoom from: ",
	    xmTextFieldWidgetClass, ZoomWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator0,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, ZoomFromLabel,
            XmNwidth, 120,
	    NULL);

	XtVaCreateManagedWidget ("usecs",
	    xmLabelWidgetClass, ZoomWindow,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, ZoomFromText,
	    XmNy, 25,
	    NULL);



	separator1 = XtVaCreateManagedWidget ("sep1",
	    xmSeparatorWidgetClass, ZoomWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmSHADOW_ETCHED_IN,
	    XmNtopWidget, ZoomFromText,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
            XmNheight, 20,
            NULL);
  
	ZoomToLabel = XtVaCreateManagedWidget ("Zoom to:     ",
	    xmLabelWidgetClass, ZoomWindow,
	    XmNx, 2,
	    XmNy, 78,
	    NULL);

	ZoomToText = XtVaCreateManagedWidget ("zoom to text",
	    xmTextFieldWidgetClass, ZoomWindow,
            XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator1,
            XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, ZoomToLabel,
            XmNwidth, 120,
	    NULL);

	XtVaCreateManagedWidget ("usecs",
	    xmLabelWidgetClass, ZoomWindow,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, ZoomToText,
	    XmNy, 78,
	    NULL);


	separator2 = XtVaCreateManagedWidget ("sep2",
	    xmSeparatorWidgetClass, ZoomWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNorientation, XmHORIZONTAL,
            XmCSeparatorType, XmSHADOW_ETCHED_IN,
	    XmNtopWidget, ZoomToText,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
            XmNheight, 20,
            NULL);


	ButtonArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, ZoomWindow,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_FORM,
	    NULL);


	tempString = XmStringCreateSimple ("Zoom");
	zoomButton = XtVaCreateManagedWidget ("zoom",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (zoomButton, XmNactivateCallback, zoom_CB, NULL);

	tempString = XmStringCreateSimple ("Zoom & Done");
	zoomdoneButton = XtVaCreateManagedWidget ("zoomdone",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, zoomButton,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (zoomdoneButton, XmNactivateCallback, zoom_done_CB, NULL);

	tempString = XmStringCreateSimple ("Done");
	doneButton = XtVaCreateManagedWidget ("done",
	    xmPushButtonWidgetClass, ButtonArea,
            XmNlabelString, tempString,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, separator2,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, zoomdoneButton,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XmStringFree (tempString);
	XtAddCallback (doneButton, XmNactivateCallback, close_zoom_control_CB, NULL);


    }
    XtManageChild(ZoomWindow);

}


void
zoom1_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a + (long long) (((float) nf_width_a) * 0.04545);
	nf_width_a = nf_width_a - (long long) (((float) nf_width_a) * 0.09090);
	if (nf_width_a < width_a_min)
	    nf_width_a = width_a_min;
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a/1.1;
	if (width_a < width_a_min)
	    width_a = width_a_min;
	set_slider_a_width_and_loc();
    }
} 


void
zoom2_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a + (long long) (((float) nf_width_a) * 0.1666);
	nf_width_a = nf_width_a - (long long) (((float) nf_width_a) * 0.3333);
	if (nf_width_a < width_a_min)
	    nf_width_a = width_a_min;
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a/1.5;
	if (width_a < width_a_min)
	    width_a = width_a_min;
	set_slider_a_width_and_loc();
    }
} 


void
zoom3_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a + (long long) (((float) nf_width_a) * 0.25);
	nf_width_a = nf_width_a - (long long) (((float) nf_width_a) * 0.5);
	if (nf_width_a < width_a_min)
	    nf_width_a = width_a_min;
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a/2.0;
	if (width_a < width_a_min)
	    width_a = width_a_min;
	set_slider_a_width_and_loc();
    }
} 


void
zoom4_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a + (long long) (((float) nf_width_a) * 0.375);
	nf_width_a = nf_width_a - (long long) (((float) nf_width_a) * 0.75);
	if (nf_width_a < width_a_min)
	    nf_width_a = width_a_min;
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a/4.0;
	if (width_a < width_a_min)
	    width_a = width_a_min;
	set_slider_a_width_and_loc();
    }
} 


void
zoom5_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = ((nf_start_val_a + nf_width_a)/2) - 1;
	nf_width_a = width_a_min;
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a_min;
	set_slider_a_width_and_loc();
    }
} 


void
zoom6_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a - (long long) (((float) nf_width_a) * 0.04545);
	nf_width_a = nf_width_a + (long long) (((float) nf_width_a) * 0.09090);
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a * 1.1;
	if (width_a > max_val_a)
	    width_a = max_val_a;
	if (start_val_a + width_a > max_val_a)
	    start_val_a = max_val_a - width_a;
	set_slider_a_width_and_loc();
    }
} 


void
zoom7_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a - (long long) (((float) nf_width_a) * 0.25);
	nf_width_a = nf_width_a + (long long) (((float) nf_width_a) * 0.5);
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a * 1.5;
	if (width_a > max_val_a)
	    width_a = max_val_a;
	if (start_val_a + width_a > max_val_a)
	    start_val_a = max_val_a - width_a;
	set_slider_a_width_and_loc();
    }
} 


void
zoom8_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a - (long long) (((float) nf_width_a) * 0.5);
	nf_width_a = nf_width_a + (long long) (((float) nf_width_a) * 1.0);
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a * 2.0;
	if (width_a > max_val_a)
	    width_a = max_val_a;
	if (start_val_a + width_a > max_val_a)
	    start_val_a = max_val_a - width_a;
	set_slider_a_width_and_loc();
    }
} 


void
zoom9_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = nf_start_val_a - (long long) (((float) nf_width_a) * 1.0);
	nf_width_a = nf_width_a + (long long) (((float) nf_width_a) * 2.0);
	readjust_nf_start_and_end(1);
    }
    else {
	width_a = width_a * 4.0;
	if (width_a > max_val_a)
	    width_a = max_val_a;
	if (start_val_a + width_a > max_val_a)
	    start_val_a = max_val_a - width_a;
	set_slider_a_width_and_loc();
    }
} 


void
zoom0_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    if (no_frame_mode) {
	nf_start_val_a = 0;
	nf_width_a = nf_max_val_a;
	readjust_nf_start_and_end(1);
    }
    else {
	start_val_a = 0;
	width_a = max_val_a;
	set_slider_a_width_and_loc();
    }
} 


void
tic_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtVaSetValues (ticVal[GraphA_tics+1],
		   XmNset, False, NULL);
    GraphA_tics = (int)clientD-1;
    XtVaSetValues (ticVal[GraphA_tics+1],
		   XmNset, True, NULL);
    
}



void
div_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtVaSetValues (divVal[GraphA_divisions],
		   XmNset, False, NULL);
    GraphA_divisions = (int)clientD;
    XtVaSetValues (divVal[GraphA_divisions],
		   XmNset, True, NULL);
    
}




void
set_slider_a_width_and_loc()
{
    int size_val;
    int start_val;
    

    if (no_frame_mode) {
	size_val = (int)(0.99999999+MAX_SLIDER_A*((float)nf_width_a/(float)nf_max_val_a));
	if (size_val < 1) 
	    size_val = 1;
	if (size_val > (int)MAX_SLIDER_A) 
	    size_val = (int)MAX_SLIDER_A;

	start_val = (int) ((float) MAX_SLIDER_A * (float) nf_start_val_a / (float) nf_max_val_a);
	if ((start_val + size_val) > (int)MAX_SLIDER_A) {
	    start_val = (int)(MAX_SLIDER_A - size_val);
	}
    }
    else {
	size_val = (int)(0.99999999+MAX_SLIDER_A*((float)width_a/(float)max_val_a));
	if (size_val < 1) 
	    size_val = 1;
	if (size_val > (int)MAX_SLIDER_A) 
	    size_val = (int)MAX_SLIDER_A;
	
	start_val = 100*start_val_a/max_val_a;
	
	if ((start_val + size_val) > MAX_SLIDER_A) {
	    start_val = MAX_SLIDER_A - size_val;
	}
    }

    XmScaleSetValue(scroll_a, 0);
    XtVaSetValues(scroll_a, XmNsliderSize, 1, NULL);


    XmScaleSetValue(scroll_a, start_val);

    XtVaSetValues(scroll_a, XmNsliderSize, size_val, NULL);

/*
    if (no_frame_mode)
	XtVaSetValues(thumbWheel_a, XmNvalue, 
		      (int)(A_THUMB*((float)nf_width_a/(float)nf_max_val_a)), NULL);
    else
	XtVaSetValues(thumbWheel_a, XmNvalue, 
		      (int)(A_THUMB*((float)width_a/(float)max_val_a)), NULL);
*/
}



