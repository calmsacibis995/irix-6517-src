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
 * File:	graph-main.c					          *
 *									  *
 * Description:	This file draws the main graph                            *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

void draw_ref_line (int numb_tics, int divisions, float width, float start_val);
void draw_offset_time(int cpu);
void draw_process_times(int cpu);
void draw_explicit_times(int cpu);
void draw_minor_maxes(int cpu);
void draw_frame_lines (int cpu, int end, int numb_minors, int minor_time);
void draw_frame_lines_new (int cpu, int end, int numb_minors, int minor_time);
void draw_zoom_line_frame_mode(int height);
void draw_search_line_frame_mode(int height);
void draw_scan_line_frame_mode(int height);
void draw_max_line_str(float x_pos, int print_val);
void draw_pos_string(int ortho, float pos, int y_pos, long long val, float color_vector[3], int type);

void draw_nf_frame_lines(int cpu);
void draw_nf_events(int cpu);

#define SEC_COL_A (LEFT_BORDER_A -0.055)
#define CPU_COL_A (LEFT_BORDER_A -0.040) 
#define CPUN_COL_A (LEFT_BORDER_A -0.030)
#define LEFT_COL_A 0.005

void
draw_main_graph()
{
    int i, ii, j, numb_cpus_on;
    GLint mm;
    char string[255];

    /* make this the active gl widget to draw on */
    glXMakeCurrent(main_graph.display, 
		   XtWindow(main_graph.glWidget), 
		   main_graph.context);
    
    
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3sv(bkgnd);
    
    glMatrixMode(GL_MODELVIEW);
    
    glColor3fv(yellowvect);
    if (GraphA_label_type == MSECS)
	sprintf (string, "msecs");
    else
	sprintf (string, "usecs");
    glRasterPos2i(width_a * SEC_COL_A,  0.5*HEIGHT);
    
    printString(string);
    glFlush();
    
    
    /* draw scale at bottom for reference */
    glTranslatef(width_a*LEFT_BORDER_A,  0,  0);

    draw_ref_line(GraphA_tics,GraphA_divisions, width_a, start_val_a);

    
    glTranslatef(width_a*-LEFT_BORDER_A,  HEIGHT,  0);
    
    
    
    numb_cpus_on = 0;

    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	ii = gr_cpu_map[i];
	if (cpuOn[i]) {
	    numb_cpus_on++;
	    glTranslatef(0,  HEIGHT,  0); /* make room to draw frame line labels */
	    
	    /* draw cpu numbers on left side */
	    glColor3fv(yellowvect);
	    glRasterPos2i(width_a * CPU_COL_A,  HEIGHT*0.6);
	    printString ("cpu");
	    sprintf (string, "%d", ii);
	    glRasterPos2i(width_a * CPUN_COL_A,  HEIGHT*0.1);
	    printString (string);
	    
	    sprintf(string, "Major  %d\n", maj_frame_numb[ii]);
	    glRasterPos2i(width_a * LEFT_COL_A,  HEIGHT*0.595);
	    printString (string);
	    
	    
	    
	    glColor3fv(redvect);
	    if (GraphA_label_type == MSECS)
		sprintf (string, "msecs");
	    else
		sprintf (string, "usecs");
	    glRasterPos2i(width_a * SEC_COL_A,  -0.4*HEIGHT);
	    printString (string);
	    
	    draw_offset_time(ii);
	    
	    
	    glTranslatef(width_a * LEFT_BORDER_A,  0,  0);
	    
	    if (1) {
		if (graph_mode == SUMMARIZED)
		    draw_process_times(ii);
		else
		    draw_explicit_times(ii);
		
		draw_minor_maxes(ii);
	    }
	    else {
		/* test to show all colors */
		init_proc_colors(12);
		
		for (j=0;j<numb_proc_colors;j++) {
		    draw_frame_rect_new (width_a, max_val_a, 4000*j+200, 4000*j+3500, 
				     1, proc_color[j*2],FALSE, "",FALSE, 0, 0, 0);
		    draw_frame_rect_new (width_a, max_val_a, 4000*j+3500, 4000*j+4200, 
				     1, proc_color[j*2+j],FALSE, "",FALSE, 0, 0, 0);
		}
	    }
	    
	    
	    draw_frame_lines_new (ii,width_a,
			      major_frame_account[ii].template_minors,
			      major_frame_account[ii].minor_time);
	    glTranslatef(width_a * -LEFT_BORDER_A,  HEIGHT,  0);
	    
	}
	
    }
    
    
    glTranslatef(0,  ((2*HEIGHT*numb_cpus_on + HEIGHT) * -1),  0);
    
    
    width_ay = 2*HEIGHT*numb_cpus_on + HEIGHT + TOP_BORDER_A;
    
    if ((zoom_line_l >= 0) || (zoom_line_r >= 0)) {
	glTranslatef(width_a * LEFT_BORDER_A,  0,  0);
	draw_zoom_line_frame_mode(width_ay);
	glTranslatef(width_a * -LEFT_BORDER_A,  0,  0);
    }
    
    if ((scan_line_m >= 0) || (scan_line_l >= 0) || (scan_line_r >= 0)) {
	if (scan_line_m >= 0)
	    get_scan_line_val();
	glTranslatef(width_a * LEFT_BORDER_A,  0,  0);
	draw_scan_line_frame_mode(width_ay);
	glTranslatef(width_a * -LEFT_BORDER_A,  0,  0);
    }

    if (current_search_on && current_search_found) {
	glTranslatef(width_a * LEFT_BORDER_A,  0,  0);
	draw_search_line_frame_mode(width_ay);
	glTranslatef(width_a * -LEFT_BORDER_A,  0,  0);
     }
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,  width_a * SCALE_FACT_A ,  0,  width_ay);
    glMatrixMode(mm);

    glXSwapBuffers(main_graph.display, XtWindow(main_graph.glWidget));

}



#define NF_SEC 0.008
#define NF_CPU 0.025
#define NF_CPU1 0.033

void
draw_main_no_frame_graph()
{
    GLint mm;
    char string[255];
    int numb_cpus_on, cpu, i;
    float offset;
    static long long last_time = 0;
    long long end_val;
    static struct timeval last_tv = {0,0};
    struct timeval  tv;
    struct timezone tzone;

    if ((reading_count > 0) && (show_live_stream)) {
      
	gettimeofday(&tv, &tzone);
	if (((last_tv.tv_sec - tv.tv_sec) * 1000000 + last_tv.tv_usec - tv.tv_usec) >
	    nf_refresh_rate) {


	    end_val = 0;
	    for (i = gr_numb_cpus - 1; i >= 0; i--) {
		if (cpuOn[i]) {
		    cpu = gr_cpu_map[i];
		    if (graph_info[cpu].no_frame_event[graph_info[cpu].no_frame_event_count-1].end_time > 
			end_val) {
			end_val = graph_info[cpu].no_frame_event[graph_info[cpu].no_frame_event_count-1].end_time;
		    }
		}
	    }
	    nf_start_val_a = end_val - (long long)nf_refresh_rate;
	    nf_width_a = (long long)nf_refresh_rate;
	    readjust_nf_start_and_end(0);

	    last_tv.tv_sec = tv.tv_sec;
	    last_tv.tv_usec = tv.tv_usec;
	}
    }


    /* make this the active gl widget to draw on */
    glXMakeCurrent(main_graph.display, 
		   XtWindow(main_graph.glWidget), 
		   main_graph.context);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3sv(bkgnd);
    
    glMatrixMode(GL_MODELVIEW);
    
    glColor3fv(yellowvect);
    if (GraphA_label_type == MSECS)
	sprintf (string, "msecs");
    else
	sprintf (string, "usecs");

    glRasterPos2i(nf_width_a * NF_SEC,  0.5*HEIGHT);
    
    printString(string);
    glFlush();
    
    glTranslatef(nf_width_a*NF_LEFT_BORDER,  0,  0);

    draw_ref_line(GraphA_tics, GraphA_divisions, nf_width_a, nf_start_val_a);


    glTranslatef(-1.0*nf_width_a*NF_LEFT_BORDER,  HEIGHT,  0);

    numb_cpus_on = 0;
    offset = (float)nf_width_a * NF_LEFT_BORDER - (float)nf_start_val_a;


    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	cpu = gr_cpu_map[i];
	if (cpuOn[i]) {
	    numb_cpus_on++;

	    glTranslatef(0,  HEIGHT,  0); /* make room to draw frame line labels */
	    
	    /* draw cpu numbers on left side */
	    glColor3fv(yellowvect);
	    glRasterPos2f((float)nf_width_a * NF_CPU,  HEIGHT*0.6);
	    printString ("cpu");
	    sprintf (string, "%d", cpu);
	    glRasterPos2f((float)nf_width_a * NF_CPU1,  HEIGHT*0.1);
	    printString (string);
	
	    glColor3fv(redvect);
	    if (GraphA_label_type == MSECS)
		sprintf (string, "msecs");
	    else
		sprintf (string, "usecs");
	    glRasterPos2f((float)nf_width_a * NF_SEC,  -0.4*HEIGHT);
	    printString (string);

	    glTranslatef(offset,  0,  0);

	    draw_nf_frame_lines(cpu);

	    draw_nf_events(cpu);

	    glTranslatef(-1 * offset,  HEIGHT,  0);

	}
    }
    glTranslatef(0,  ((2*HEIGHT*numb_cpus_on + HEIGHT) * -1),  0);


    width_ay = 2*HEIGHT*numb_cpus_on + HEIGHT + TOP_BORDER_A;

    if (zoom_line_l >= 0) {
	draw_line((zoom_line_l + offset),0,
		  (zoom_line_l + offset),width_ay,1,yellowvect);
    }
    if (zoom_line_r >= 0) {
	draw_line((zoom_line_r + offset),0,
		  (zoom_line_r + offset),width_ay,1,yellowvect);
    }

    if (current_search_on && current_search_found){
	draw_line((search_line + offset),0,
		  (search_line + offset),width_ay,1,whitevect);
    }


    if (scan_line_m >= 0) {
	get_scan_line_val();
	draw_line((scan_line_m + offset),0,
		  (scan_line_m + offset),width_ay,1,cyanvect);
	glColor3fv(cyanvect);
	glRasterPos2f(scan_line_m + offset,  (int)(1.6*(float)HEIGHT));
	if (GraphA_label_type == MSECS)
	    sprintf (string, " %lld", (long long)scan_line_m/1000);
	else
	    sprintf (string, " %lld", (long long)scan_line_m);
	

	printString (string);
    }
    if ((scan_line_l >= 0) || (scan_line_r >= 0)) {
	if ((scan_line_l - scan_line_r) == 0)
	    scan_line_r = scan_line_l + 1;
    }
    if (scan_line_l >= 0) {
	draw_line((scan_line_l + offset),0,
		  (scan_line_l + offset),width_ay,1,cyanvect);
    }
    if (scan_line_r >= 0) {
	draw_line((scan_line_r + offset),0,
		  (scan_line_r + offset),width_ay,1,cyanvect);
    }
    if ((scan_line_r >= 0) && (scan_line_r >= 0)) {
	draw_line((scan_line_l + offset), (int)(1.0*(float)HEIGHT),
		  (scan_line_r + offset), (int)(1.0*(float)HEIGHT),
		  1, cyanvect);
	if (GraphA_label_type == MSECS)
	    sprintf (string, " %lld", (long long)abs(scan_line_l - scan_line_r)/1000);
	else
	    sprintf (string, " %lld", (long long)abs(scan_line_l - scan_line_r));
	if (scan_line_l < scan_line_r)
	    glRasterPos2i(scan_line_l + offset, (int)(1.1*(float)HEIGHT));
	else
	    glRasterPos2i(scan_line_r + offset, (int)(1.1*(float)HEIGHT));
	printString (string);
    }
    
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,  nf_width_a * NF_SCALE_FACT ,  0,  width_ay);
    glMatrixMode(mm);

    glXSwapBuffers(main_graph.display, XtWindow(main_graph.glWidget));
}

/* display numb_tics numbers across the line, places divisions tics
   between them and type 0 is usecs, type 1 msecs */
void
draw_ref_line(int numb_tics, int divisions, float width, float start_val)
{
    int adj_index;
    float pr_val;
    float end_val;
    float first_val, last_val;
    char string[255];
    int i, j;
    long long print_val;
    float at_val, at_val1;
    float last_at_val;
    char msg_str[64];

    end_val = start_val + width;

    glColor3fv(yellowvect);

    for (i=0;i<=numb_tics;i++) {
	pr_val = (((float)i/(float)numb_tics)*(end_val - start_val)) + start_val;
	if (width > 10000) { /* try to make things look neat */
	    if ((i==0) && (((long long)pr_val % 1000) != 0)) 
		pr_val = pr_val + 1000;
	    print_val = ((long long)pr_val / 1000)*1000;
	}
	else {
	    print_val = pr_val;
	}

	if (i==0)
	    at_val = print_val-(long long)start_val;
	else
	    at_val = (float)print_val-start_val;

	if (i==0) 
	    first_val = at_val;
	if (i==numb_tics) 
	    last_val = at_val;
	
	if (GraphA_label_type == MSECS)
	    sprintf (string, "%lld", (long long)print_val/1000);
	else
	    sprintf (string, "%lld", (long long)print_val);

	adj_index = strlen(string);
	if ((adj_index < 0) || (adj_index > 11)) {
	    sprintf(msg_str, "(5) graph label %s length %d out of range [1,11] %d %d\n",string,adj_index, start_val ,width);
	    grtmon_message(msg_str, ALERT_MSG, 28);
	}

	glRasterPos2f(at_val - ((float) width)/(float)adj_val[adj_index],  1);

	printString (string);
	draw_line(at_val,HEIGHT*.4,at_val,HEIGHT*.9,1,yellowvect);
	if (i !=0) {  /* draw divisions */
	    for (j=1;j<=divisions;j++) {
		at_val1 = last_at_val + (j * (float) (at_val - last_at_val) / (float) (divisions +1));
		draw_line(at_val1,HEIGHT*.4,at_val1,HEIGHT*.7,1,yellowvect);
	    }
	}
	last_at_val = at_val;
    }
    draw_line(first_val,HEIGHT*.5,last_val,HEIGHT*.5,1,yellowvect);

}



void
draw_offset_time(int cpu)
{
    static char string[255];

    glRasterPos2i(width_a * LEFT_COL_A,  0.0*HEIGHT);
    printString ("Time Offset");
    
    if (display_frame_type == COMPUTED) {
	if (GraphA_label_type == MSECS)
	    sprintf(string, "%lld\n",
		    (long long)
		    ((major_frame_account[cpu].minor_time*
		      major_frame_account[cpu].template_minors*
		      maj_frame_numb[cpu])/1000));
	else
	    sprintf(string, "%lld\n",
		    (long long)
		    (major_frame_account[cpu].minor_time*
		     major_frame_account[cpu].template_minors*
		     (maj_frame_numb[cpu] -1)));
    }
    else {  /* display actual time from start */
	if (GraphA_label_type == MSECS)
	    sprintf(string, "%lld\n",
		    graph_info[cpu].maj_absolute_start/1000);
	else
	    sprintf(string, "%lld\n",
		    graph_info[cpu].maj_absolute_start);
    }
    
    glRasterPos2i(width_a * LEFT_COL_A,  -0.4*HEIGHT);
    printString (string);
}



void
draw_process_times(int cpu)
{
    int current_time;
    int i,j;

    current_time = 0;
    

    for (i=0;i<major_frame_account[cpu].template_minors;i++) {
	current_time = i*major_frame_account[cpu].minor_time;
	draw_frame_rect_new(width_a, max_val_a, current_time,
			current_time = current_time + graph_info[cpu].ave_minor[i].start_time,
			1, kern_startup_color, FALSE, "", FALSE, 0, 0, 0);

	for (j=1;j<major_frame_account[cpu].template_in_minor[i];j++) {
	    if (graph_info[cpu].ave_minor[i].process[j].show_time) {
		draw_frame_rect_new(width_a, max_val_a, current_time,
	            current_time = current_time + graph_info[cpu].ave_minor[i].process[j].utime,1,
		       proc_color[(2*(graph_info[cpu].ave_minor[i].process[j].index_in_maj-1))%(2*numb_proc_colors)], 
			    TRUE, graph_info[cpu].ave_minor[i].process[j].name, TRUE, 
			graph_info[cpu].ave_minor[i].process[j].utime,
			graph_info[cpu].ave_minor[i].process[j].ktime,
				proc_color[(1+2*(graph_info[cpu].ave_minor[i].process[j].index_in_maj-1))%(2*numb_proc_colors)]);
	    }
	    else {
		draw_frame_rect_new(width_a, max_val_a, current_time,
	            current_time = current_time + graph_info[cpu].ave_minor[i].process[j].utime,1,
		       proc_color[(2*(graph_info[cpu].ave_minor[i].process[j].index_in_maj-1))%(2*numb_proc_colors)], 
			    TRUE, graph_info[cpu].ave_minor[i].process[j].name,FALSE, 0, 0, 0);
	    }
	    
	    draw_frame_rect_new(width_a, max_val_a, current_time,
		     current_time = current_time + graph_info[cpu].ave_minor[i].process[j].ktime,1,
		proc_color[(1+2*(graph_info[cpu].ave_minor[i].process[j].index_in_maj-1))%(2*numb_proc_colors)], 
			    FALSE,"",FALSE, 0, 0, 0);


	}
	/* if we want to display the idle time */
	if (graph_info[cpu].ave_minor[i].process[0].show_time) {
	    draw_frame_rect_new(width_a, max_val_a, current_time, 
			    major_frame_account[cpu].minor_time*(i+1),
			    1,idlevect,FALSE,"idle",2,
			    graph_info[cpu].ave_minor[i].process[0].utime+
			    graph_info[cpu].ave_minor[i].process[0].ktime, 0, whitevect);
	}
    }
}


/* this is called in the case the graph mode is EXPLIICT
   go through the list of events and display each one of them */
/* this function is similar to draw_nf_events for drawing
   no frame events, changes to either function so
   probably be made in the other */
void
draw_explicit_times(int cpu)
{
    int current_time;

    int ev_start_time, ev_end_time;
    int i,j;

    current_time = 0;


    for (i=0;i<major_frame_account[cpu].template_minors;i++) {
	current_time = i*major_frame_account[cpu].minor_time;
	draw_frame_rect_new(width_a, max_val_a, current_time,
			current_time = current_time + graph_info[cpu].ave_minor[i].start_time,
			1, kern_startup_color, FALSE, "", FALSE, 0, 0, 0);

	

	for (j=0; j<graph_info[cpu].exact_event[i].count; j++) {
	    ev_start_time = graph_info[cpu].exact_event[i].start_time[j];
	    ev_end_time = graph_info[cpu].exact_event[i].end_time[j];

	    if (graph_info[cpu].exact_event[i].type[j] == EX_EV_KERN) {
		/* check to see if we want to draw an entry arrow */
		if (j>0) {
		    if (graph_info[cpu].exact_event[i].type[j-1] == EX_EV_KERN)
			; /* do nothing we're in the middle of kernel events */
		    else
			draw_arrow_new(width_a, max_val_a, ev_start_time, EX_EV_KERN, ex_ev_kernvect, 0);
		}
		else
		    draw_arrow_new(width_a, max_val_a, ev_start_time, EX_EV_KERN, ex_ev_kernvect, 0);

		/* draw kernel box */
		draw_frame_rect_new(width_a, max_val_a, ev_start_time, ev_end_time, 1,
				ex_ev_kernvect, TRUE, "",FALSE, 0, 0, 0);

		/* check to see if we want to draw an exit arrow */
		if (j<graph_info[cpu].exact_event[i].count - 1) {
		    if (graph_info[cpu].exact_event[i].type[j+1] == EX_EV_KERN)
			; /* do nothing we're in the middle of kernel events */
		    else
			draw_arrow_new(width_a, max_val_a, ev_end_time, EX_EV_KERN, ex_ev_kernvect, 1);
		}
		else
		    draw_arrow_new(width_a, max_val_a, ev_end_time, EX_EV_KERN, ex_ev_kernvect, 1);
	    }
	    else if ((graph_info[cpu].exact_event[i].type[j] == EX_EV_INTR_ENTRY) ||
		     (graph_info[cpu].exact_event[i].type[j] == EX_EV_INTR_EXIT)) {
		draw_arrow_new(width_a, max_val_a,
			   ev_start_time,
			   graph_info[cpu].exact_event[i].type[j],
			   ex_ev_intrvect, 0);
	    }
	    else if (graph_info[cpu].exact_event[i].type[j] == EX_EV_USER) {
		draw_arrow_new(width_a, max_val_a, 
			   ev_start_time,
			   graph_info[cpu].exact_event[i].type[j],
			   ex_ev_uservect, 0);
	    }
	    else {  /* a user event */
		/* check and see whether it's the idle loop or real process */
		if (graph_info[cpu].exact_event[i].type[j] == 0) {
		    draw_frame_rect_new(width_a, max_val_a, ev_start_time, ev_end_time, 1,
				    ex_ev_idlevect, TRUE, "idle", 2, 0, 0, 0);
		}
		else {
		    draw_frame_rect_new(width_a, max_val_a, ev_start_time, ev_end_time, 1,
				    proc_color[(2*(graph_info[cpu].exact_event[i].type[j]-1))%(2*numb_proc_colors)], 
				    TRUE,
				    graph_info[cpu].maj_ave_process[graph_info[cpu].exact_event[i].type[j]].name,
				    FALSE, 0, 0, 0);
		}
	    }




	}
	/* if we want to display the idle time */
/*	if (graph_info[cpu].ave_minor[i].process[0].show_time) {
	    draw_frame_rect_new(width_a, max_val_a, current_time, 
			    major_frame_account[cpu].minor_time*(i+1),
			    1,whitevect,FALSE,"",2,
			    graph_info[cpu].ave_minor[i].process[0].utime+
			    graph_info[cpu].ave_minor[i].process[0].ktime, 0, whitevect);
	    
	}*/

    }
}


void 
draw_minor_maxes(int cpu)
{
    int i;

    if (graph_mode == SUMMARIZED) {
	for (i=0;i<major_frame_account[cpu].template_minors;i++) {
	    draw_max_line_str(i*major_frame_account[cpu].minor_time+
			      graph_info[cpu].max_minor[i].sum_val,
			      graph_info[cpu].max_minor[i].sum_val);
	}
    }
    else {
	for (i=0;i<major_frame_account[cpu].template_minors;i++) {
	    draw_max_line_str(i*major_frame_account[cpu].minor_time+
			      graph_info[cpu].max_minor[i].ex_ev_val,
			      graph_info[cpu].max_minor[i].ex_ev_val);
	}
    }
}



void
draw_frame_lines (int cpu, int line_end, int numb_minors, int minor_time)
{
    float y1,y2;
    float x_pos, percent;
    char string[255];
    int i,adj_index;
    int sum_minor_time;

    percent = (float)line_end/(float)max_val_a;

    sum_minor_time = 0;

    x_pos = 0;
    draw_pos_string(width_a, x_pos, -.4*HEIGHT, 
		    (long long)sum_minor_time, redvect, GraphA_label_type);
    draw_line(x_pos,0,x_pos,HEIGHT,1,redvect);

    for (i=0;i<numb_minors;i++) {
	if (graph_info[cpu].special_minor[i] != 0) {
	    glColor3fv(redvect);
	    switch (graph_info[cpu].special_minor[i]) {
	      case SPECIAL_MINOR_STRETCH:
		strcpy(string, "minor stretched");
		break;
	      case SPECIAL_MINOR_STOLEN_STRETCH:
		strcpy(string, "minor stolen from and stretched");
		break;
	      case SPECIAL_MINOR_INJECT:
		strcpy(string, "minor frame injected");
		break;
	      case SPECIAL_MINOR_STOLEN_INJECT:
		strcpy(string, "minor stolen from and frame injected");
		break;
	      case SPECIAL_MINOR_STEAL:
		strcpy(string, "minor stealing from next frame");
		break;
	      case SPECIAL_MINOR_STOLEN_STEAL:
		strcpy(string, "minor stolen from and stealing from next frame");
		break;
	      case SPECIAL_MINOR_STOLEN:
		strcpy(string, "minor stolen from");
		break;
	      case SPECIAL_MINOR_LOST:
		strcpy(string, "lost event this minor");
		break;
	      default:
		sprintf(string, "unknown special minor %d",
			graph_info[cpu].special_minor[i]);
		break;
	    }
	    glRasterPos2i(0, -0.1*HEIGHT);
	    printString (string);

	    if (graph_info[cpu].special_minor[i] != SPECIAL_MINOR_LOST) {
		sum_minor_time = sum_minor_time + minor_time + 
		    graph_info[cpu].special_minor_time[i];
	    }
	}
	else {
	    sum_minor_time = sum_minor_time + minor_time;
	}
	x_pos = sum_minor_time;


	if (display_frame_type == COMPUTED) {
	    draw_pos_string(width_a, x_pos, -.4*HEIGHT, 
			    (long long)sum_minor_time, redvect, GraphA_label_type);
	}
	else {
	    draw_pos_string(width_a, x_pos, -.4*HEIGHT, 
			    (long long)graph_info[cpu].actual_minor_time[i],
			    redvect, GraphA_label_type);
	}


	if ((x_pos+EPSILON >= 0) && (x_pos-EPSILON <= line_end)) {

	    if (i==numb_minors) { /* major frame boundary */
		y1 = 0;
		y2 = HEIGHT;
	    }
	    else {
		y1 = HEIGHT * 0.2;
		y2 = HEIGHT * 0.8;
	    }
	    draw_line(x_pos,y1,x_pos,y2,1,redvect);
	}
    }
    draw_line(0, HEIGHT*.5, sum_minor_time, HEIGHT*.5, 1, redvect);

}


void
draw_frame_lines_new (int cpu, int line_end, int numb_minors, int minor_time)
{
    float y1,y2;
    float x_pos;
    char string[255];
    int i,adj_index;
    int sum_minor_time;


    sum_minor_time = 0;

    x_pos = 0;
    draw_pos_string(width_a, x_pos, -.4*HEIGHT, 
		    (long long)sum_minor_time, redvect, GraphA_label_type);
    draw_line(x_pos,0,x_pos,HEIGHT,1,redvect);

    for (i=0;i<numb_minors;i++) {
	if (graph_info[cpu].special_minor[i] != 0) {
	    glColor3fv(redvect);
	    switch (graph_info[cpu].special_minor[i]) {
	      case SPECIAL_MINOR_STRETCH:
		strcpy(string, "minor stretched");
		break;
	      case SPECIAL_MINOR_STOLEN_STRETCH:
		strcpy(string, "minor stolen from and stretched");
		break;
	      case SPECIAL_MINOR_INJECT:
		strcpy(string, "minor frame injected");
		break;
	      case SPECIAL_MINOR_STOLEN_INJECT:
		strcpy(string, "minor stolen from and frame injected");
		break;
	      case SPECIAL_MINOR_STEAL:
		strcpy(string, "minor stealing from next frame");
		break;
	      case SPECIAL_MINOR_STOLEN_STEAL:
		strcpy(string, "minor stolen from and stealing from next frame");
		break;
	      case SPECIAL_MINOR_STOLEN:
		strcpy(string, "minor stolen from");
		break;
	      case SPECIAL_MINOR_LOST:
		strcpy(string, "lost event this minor");
		break;
	      default:
		sprintf(string, "unknown special minor %d",
			graph_info[cpu].special_minor[i]);
		break;
	    }
	    glRasterPos2i(0, -0.1*HEIGHT);
	    printString (string);

	    if (graph_info[cpu].special_minor[i] != SPECIAL_MINOR_LOST) {
		sum_minor_time = sum_minor_time + minor_time + 
		    graph_info[cpu].special_minor_time[i];
	    }
	}
	else {
	    sum_minor_time = sum_minor_time + minor_time;
	}
	x_pos = sum_minor_time - start_val_a;


	if (display_frame_type == COMPUTED) {
	    draw_pos_string(width_a, x_pos, -.4*HEIGHT, 
			    (long long)sum_minor_time, redvect, GraphA_label_type);
	}
	else {
	    draw_pos_string(width_a, x_pos, -.4*HEIGHT, 
			    (long long)graph_info[cpu].actual_minor_time[i],
			    redvect, GraphA_label_type);
	}


	if ((x_pos+EPSILON >= 0) && (x_pos-EPSILON <= line_end)) {

	    if (i==numb_minors) { /* major frame boundary */
		y1 = 0;
		y2 = HEIGHT;
	    }
	    else {
		y1 = HEIGHT * 0.2;
		y2 = HEIGHT * 0.8;
	    }
	    draw_line(x_pos,y1,x_pos,y2,1,redvect);
	}
    }
    draw_line(0, HEIGHT*.5, sum_minor_time, HEIGHT*.5, 1, redvect);

}



void
draw_zoom_line_frame_mode(int height) 
{
    float act_x;
    
    if (zoom_line_l >= 0) {
	act_x = (float)zoom_line_l - start_val_a;
	draw_line(act_x,0,act_x,height,1,yellowvect);
    }

    if (zoom_line_r >= 0) {
	act_x = (float)zoom_line_r - start_val_a;
	draw_line(act_x,0,act_x,height,1,yellowvect);
    }
}




void
draw_search_line_frame_mode(int height) 
{
    float act_x;
    
    act_x = (float)search_line - start_val_a;
    
    draw_line(act_x,0,act_x,height,1,whitevect);
    
}



void
draw_scan_line_frame_mode(int height) 
{
    float act_x;
    char string[255];


    if (scan_line_m >= 0) {
	act_x = (float)scan_line_m - start_val_a;
	draw_line(act_x,0,act_x,height,1,cyanvect);
	
	if (GraphA_label_type == MSECS)
	    sprintf (string, " %lld", scan_line_m/1000);
	else
	    sprintf (string, " %lld", scan_line_m);
	
	glRasterPos2i(act_x, HEIGHT);
	printString (string);
    }
    if ((scan_line_l >= 0) || (scan_line_r >= 0)) {
	if ((scan_line_l - scan_line_r) == 0)
	    scan_line_r = scan_line_l + 1;
    }
    if (scan_line_l >= 0) {
	draw_line((scan_line_l - start_val_a),0,
		  (scan_line_l - start_val_a),width_ay,1,cyanvect);
    }
    if (scan_line_r >= 0) {
	draw_line((scan_line_r - start_val_a),0,
		  (scan_line_r - start_val_a),width_ay,1,cyanvect);
    }
    if ((scan_line_r >= 0) && (scan_line_r >= 0)) {
	draw_line((scan_line_l - start_val_a), (int)(1.0*(float)HEIGHT),
		  (scan_line_r - start_val_a), (int)(1.0*(float)HEIGHT),
		  1, cyanvect);
	if (GraphA_label_type == MSECS)
	    sprintf (string, " %lld", (long long)abs(scan_line_l - scan_line_r)/1000);
	else
	    sprintf (string, " %lld", (long long)abs(scan_line_l - scan_line_r));
	if (scan_line_l < scan_line_r)
	    glRasterPos2i(scan_line_l - start_val_a, (int)(1.1*(float)HEIGHT));
	else
	    glRasterPos2i(scan_line_r - start_val_a, (int)(1.1*(float)HEIGHT));
	printString (string);

    }
    

}




void
draw_max_line_str(float x_pos, int print_val)
{
  float percent;
  float start_val, end_val;
  float act_x;
  char string[255];
 
  percent = (float)width_a/(float)max_val_a;
  
  start_val = (float)start_val_a;
  end_val = start_val + width_a;

  act_x = x_pos - start_val;

  if ((act_x >= 0) && (act_x <= end_val)) {
      draw_line(act_x,HEIGHT*.15,act_x,HEIGHT*.9,1,whitevect);
      draw_pos_string(width_a, act_x, -0.16*HEIGHT, 
		      (long long)print_val, whitevect, GraphA_label_type);
  }
}


void
draw_pos_string(int ortho, float pos, int y_pos, long long val, 
		float color_vector[3], int type)
{
    char string[255];
    int adj_index;
        char msg_str[64];

    glColor3fv(color_vector);
    
    if (type == MSECS)
	sprintf (string, "%lld", val/1000);
    else
	sprintf (string, "%lld", val);
    
    adj_index = strlen(string);
    if ((adj_index < 0) || (adj_index > 11)) {
	sprintf(msg_str, "(1) graph label %s length %d out of range [0,11]\n",string,adj_index);
	grtmon_message(msg_str, ALERT_MSG, 29);

    }
    glRasterPos2f(pos - ((float) ortho)/adj_val[adj_index] ,  (float)y_pos);
    printString (string);
    
}


void draw_nf_frame_lines(int cpu)
{
    long long start_val, end_val;

    start_val = 
	graph_info[cpu].no_frame_event[graph_info[cpu].nf_start_event].start_time;
    end_val = 
	graph_info[cpu].no_frame_event[graph_info[cpu].nf_end_event].end_time;

    if (start_val < nf_start_val_a)
	start_val = nf_start_val_a;
    if (end_val > nf_start_val_a + nf_width_a)
	end_val = nf_start_val_a + nf_width_a;
    draw_line(start_val, 0.5*HEIGHT, end_val, 0.5*HEIGHT, 1, redvect);
    draw_line(start_val, 0.0*HEIGHT, start_val, 1.0*HEIGHT, 1, redvect);
    draw_line(end_val, 0.0*HEIGHT, end_val, 1.0*HEIGHT, 1, redvect);

    draw_pos_string((end_val-start_val), start_val, (int) (-0.4*HEIGHT), 
		    start_val, redvect, GraphA_label_type);
    draw_pos_string((end_val-start_val), end_val, (int) (-0.4*HEIGHT), 
		    end_val, redvect, GraphA_label_type);
}

/* this function is similar to draw_explicit_times for drawing
   explicit mode of major frames changes to either function so
   probably be made in the other */

void draw_nf_events(int cpu)
{
    int i;
    long long ev_start_time, ev_end_time;

	    
    for (i=graph_info[cpu].nf_start_event; i<=graph_info[cpu].nf_end_event; i++) {
	ev_start_time = graph_info[cpu].no_frame_event[i].start_time;
	ev_end_time = graph_info[cpu].no_frame_event[i].end_time;
	if (!((ev_end_time < nf_start_val_a) ||
	      (ev_start_time > (nf_start_val_a+nf_width_a)))) {

	    if (graph_info[cpu].no_frame_event[i].type == EX_EV_KERN) {
		if (graph_info[cpu].no_frame_event[i].disp) {
		    /* draw kernel arrows */
		    if (ev_start_time >= nf_start_val_a)
			draw_arrow_new(nf_width_a, nf_max_val_a, ev_start_time, EX_EV_KERN, ex_ev_kernvect, 100);
		    
		    if (ev_end_time <= (nf_start_val_a+nf_width_a))
			draw_arrow_new(nf_width_a, nf_max_val_a, ev_end_time, EX_EV_KERN, ex_ev_kernvect, 101);
		    
		    /* draw kernel box */
		    if (ev_start_time < nf_start_val_a)
			ev_start_time = nf_start_val_a;
		    if (ev_end_time > (nf_start_val_a+nf_width_a))
			ev_end_time = nf_start_val_a+nf_width_a;
		    draw_rect(ev_start_time, 0.5*HEIGHT, ev_end_time, 
			      0.5*HEIGHT-RECT_WIDTH*HEIGHT, ex_ev_kernvect);
		}
	    }
	    
	    else if ((graph_info[cpu].no_frame_event[i].type == EX_EV_INTR_ENTRY) ||
		     (graph_info[cpu].no_frame_event[i].type == EX_EV_INTR_EXIT)) {
		if ((ev_start_time >= nf_start_val_a) && 
		    (ev_start_time <= (nf_start_val_a + nf_width_a))) {
		    draw_arrow_new(nf_width_a, nf_max_val_a, ev_start_time, graph_info[cpu].no_frame_event[i].type,
			       ex_ev_intrvect, 100);
		}
	    }
	    
	    else if (graph_info[cpu].no_frame_event[i].type == EX_EV_USER) {
		if ((ev_start_time >= nf_start_val_a) && 
		    (ev_start_time <= (nf_start_val_a + nf_width_a)))
		    draw_arrow_new(nf_width_a, nf_max_val_a, ev_start_time, graph_info[cpu].no_frame_event[i].type,
			       ex_ev_uservect, 100);
	    }
	    
	    else {  
		/* a user event */
		if (ev_start_time < nf_start_val_a)
		    ev_start_time = nf_start_val_a;
		if (ev_end_time > (nf_start_val_a+nf_width_a))
		    ev_end_time = nf_start_val_a+nf_width_a;
		/* check and see whether it's the idle loop or real process */
		if (graph_info[cpu].no_frame_event[i].type == 0) {
		    draw_rect(ev_start_time, 0.5*HEIGHT+RECT_WIDTH*HEIGHT, ev_end_time, 
			      0.5*HEIGHT, ex_ev_idlevect);
		}
		else {
		    draw_rect(ev_start_time, 0.5*HEIGHT+RECT_WIDTH*HEIGHT, ev_end_time, 
			      0.5*HEIGHT-RECT_WIDTH*HEIGHT, 
			      proc_color[(2*(graph_info[cpu].no_frame_event[i].type-1))%(2*numb_proc_colors)]);
		    glRasterPos2i(ev_start_time,  HEIGHT*1.0);

		    printString (pid_name_map[graph_info[cpu].no_frame_event[i].pid]);
		}
	    }
	}
    }
}



/* in case we finished reeading the whole file before the graphics
   had a chance to start this will at least display something reaonable */
void
set_reasonable_start(int cpu)
{
    int temp_count;

    temp_count = (graph_info[cpu].no_frame_event_count >= 1000)?
	            1000: graph_info[cpu].no_frame_event_count-1;

    /* if somebody else hasn't alrady specified a display range */
    if ((graph_info[cpu].nf_start_event == 0) &&
	(graph_info[cpu].nf_end_event == 0)) {
	nf_width_a = graph_info[cpu].no_frame_event[temp_count].end_time - 
	    graph_info[cpu].no_frame_event[1].start_time;

	nf_start_val_a = graph_info[cpu].no_frame_event[1].start_time;
	readjust_nf_start_and_end(0);
    }
}
