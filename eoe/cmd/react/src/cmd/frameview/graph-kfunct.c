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
 * File:	graph-kfunct.c					          *
 *									  *
 * Description:	This file draws the kernel graph                          *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

#include "kf_main.h"
#include "kf_colors.h"
#include "graph-kfunct.h"


/*
 * Global variables
 */

int zoom_new_start;              /* start position after zoom */
int zoom_new_width;              /* width after zoom */
int zoom_start_change;           /* delta start */
int zoom_width_change;           /* delta width */

enum zoom_st zoom_state;

int kf_max_zoomout_flag = 0;
int kf_orig_start;
int kf_orig_width;
int kf_force_start_to_zero = 0;

int kf_cursor_time;      /* time where cursor is */

/*
 * Function prototypes
 */

void kf_draw_ref_line (int numb_tics, int divisions, float width,
							float start_val);
void kf_draw_pos_string(int ortho, float pos, int y_pos, int val,
					float color_vector[3], int type);
void kf_draw_frame_lines(int cpu);
void kf_draw_events(int cpu);
void kf_set_reasonable_start(int cpu);
void kf_graph_init();

/*
 * draw_kern_funct_graph - draw the kernel function graph
 */

void
draw_kern_funct_graph()
{

    GLint mm;
    char string[255];
    int numb_cpus_on, cpu, i;
    int offset;
    static long long last_time = 0;
    int end_val, width;
    int funcno,func_index;
    int time_pos,y_pos, cpu_pos;
    int func_start_time, func_end_time;
    int func_entry_index, func_exit_index;
    static int first_time_in_this_funct = 1;
    static struct timeval last_tv = {0,0};
    struct timeval  tv;
    struct timezone tzone;
    char msg_str[64];

/*
    static int build_matching_list_called = 0;
*/
	Bool retval;
	static int write_funcname_color_index = 0;

#ifdef DEBUG
sprintf(msg_str, "draw_kern_funct_graph; sleep 3");
    grtmon_message(msg_str, DEBUG_MSG, 100);
sleep(3);
#endif

/*  changed: called by manage_kern_funct_graph_CB()
    if(first_time_in_this_funct){
	kf_graph_init();
	first_time_in_this_funct = 0;
    }
*/

/* FK 6/25 */
while(reading_count != 0){
	sleep(4);
sprintf(msg_str, "draw_kern_funct_graph: reading_count %d ",
reading_count);
grtmon_message(msg_str, DEBUG_MSG, 100);
}

   /* this is supposed to be temporary! XXX */
   if((++write_funcname_color_index)% 1000 == 0)
	write_funcname_color_db();

#ifdef ORIG_BUILD_MATCHING_LIST
   /* called to build matching list "incrementally" */
   for (i = gr_numb_cpus - 1; i >= 0; i--) {
	if (cpuOn[i]) {
	    cpu = gr_cpu_map[i];
	    build_matching_list(cpu);
	}
   }
#endif
	build_matching_list();

#ifdef SLEEP
sleep(1);    /* <=========================== */
#endif


    if (reading_count > 0) {
	gettimeofday(&tv, &tzone);
	if (((last_tv.tv_sec - tv.tv_sec) * 1000000 + last_tv.tv_usec - tv.tv_usec) >
	    nf_refresh_rate) {

	    end_val = 0;
	    for (i = gr_numb_cpus - 1; i >= 0; i--) {
		if (cpuOn[i]) {
		    cpu = gr_cpu_map[i];
		    if (graph_info[cpu].no_frame_event[graph_info[cpu].
					nf_end_event].end_time > end_val) {
			end_val =
				graph_info[cpu].no_frame_event[graph_info[cpu].
				nf_end_event].end_time;
		    }
		}
	    }
	    kf_start_val_a = end_val - kf_refresh_rate;
	    kf_width_a = kf_refresh_rate;
	    kf_readjust_start_and_end();

	    last_tv.tv_sec = tv.tv_sec;
	    last_tv.tv_usec = tv.tv_usec;
	
	}
    }

    if (reading_count_done > 0) {
	for (i = gr_numb_cpus - 1; i >= 0; i--) {
	    if (cpuOn[i]) {
		cpu = gr_cpu_map[i];
		
		kf_set_reasonable_start(cpu);
	    }
	}
    }

#ifdef DEBUG
sprintf(msg_str, "draw_kern_funct_graph: kf_start_val_a %d kf_width_a %d",
			kf_start_val_a, kf_width_a);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

    /* make this the active gl widget to draw on */
/*
    retval = glXMakeCurrent(main_graph.display, 
		   XtWindow(main_graph.glWidget), 
		   main_graph.context);
*/
    retval = glXMakeCurrent(kf_graph.display, 
		   XtWindow(kf_graph.glWidget), 
		   kf_graph.context);

    if(retval == 0)
	sprintf(msg_str, "draw_kern_funct_graph: glXMakeCurrent returned false");
grtmon_message(msg_str, DEBUG_MSG, 100);

/*
    glClearColor(0.0, 0.0, 0.0, 0.0);
*/
    glClearColor(0.0, 0.1, 0.1, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3sv(bkgnd);
    
    glMatrixMode(GL_MODELVIEW);
    
    glColor3fv(yellowvect);
    if (GraphKF_label_type == MSECS)
	sprintf (string, "msecs");
    else
	sprintf (string, "usecs");

    glRasterPos2i(kf_width_a * KF_SEC,  0.5*HEIGHT);
    
#ifdef PRINT_USEC_IN_YELLOW
    printString(string);
#endif
    glFlush();
    
    glTranslatef(kf_width_a*LEFT_BORDER_KF,  0,  0);

    kf_draw_ref_line(GraphKF_tics,GraphKF_divisions, kf_width_a,kf_start_val_a);

    get_current_func_no_count = 0;
    get_current_func_no_rec_count = 0;
    exit_cache_hit_count = 0;

    glTranslatef(-1.0*kf_width_a*LEFT_BORDER_KF,  HEIGHT,  0);

    numb_cpus_on = 0;
    offset = kf_width_a * LEFT_BORDER_KF - kf_start_val_a;
    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	cpu = gr_cpu_map[i];
	if (cpuOn[i]) {
	    numb_cpus_on++;

	    glTranslatef(0,  HEIGHT,  0);
				/* make room to draw frame line labels */
	    
	    /* draw cpu numbers on left side */
	    glColor3fv(yellowvect);
	    glRasterPos2i(kf_width_a * KF_CPU,  HEIGHT*0.6);
	    printString ("cpu");
	    sprintf (string, "%d", cpu);
	    glRasterPos2i(kf_width_a * KF_CPU1,  HEIGHT*0.1);
	    printString (string);
	
	    glColor3fv(redvect);
	    if (GraphKF_label_type == MSECS)
		sprintf (string, "msecs");
	    else
		sprintf (string, "usecs");
	    glRasterPos2i(kf_width_a * KF_SEC,  -0.4*HEIGHT);
#ifdef PRINT_TIME_IN_RED
	    printString (string);
#endif

	    glTranslatef(offset,  0,  0);

	    kf_draw_frame_lines(cpu);
/*
sprintf(msg_str, "draw_kern_funct_graph: kf_draw_frame_lines skipped");
grtmon_message(msg_str, DEBUG_MSG, 100);
*/

	    kf_draw_events(cpu);

	    glTranslatef(-1 * offset,  HEIGHT,  0);

	}
    }

#ifdef DEBUG
sprintf(msg_str, "draw_kern_funct_graph: get_current_func_no_count = %d rec %d hits %d",
get_current_func_no_count, get_current_func_no_rec_count, exit_cache_hit_count);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

    glTranslatef(0,  ((2*HEIGHT*numb_cpus_on + HEIGHT) * -1),  0);

    kf_height_a = 2*HEIGHT*numb_cpus_on + HEIGHT + TOP_BORDER_A;

    if((kf_time_line_begin >= 0) || (kf_time_line_end >= 0))
	draw_time_line();

    if(search_found_cpu != -1)
	draw_function_search(); /* draw arrow pointing to function just found */

    if(kf_cursor_time != -1)
	draw_kf_cursor();	/* draw cursor */

    /* show mouse-position-dependent info */
    getCursorPosition(&time_pos, &y_pos);


    if ((kf_zoom_line >= 0) && (zoom_state == Z_LOCATING_ZOOM)){
	if(time_pos >= 0)
		draw_zoom_frame(kf_zoom_line + offset, time_pos + offset);

#ifdef ORIG
	draw_line((kf_zoom_line + offset),0,
		  (kf_zoom_line + offset),kf_height_a,1,yellowvect);
#endif
    }

    if(zoom_state == Z_IMPLEM_ZOOM) {
	do_progressive_zoom();
	draw_zoom_frame(zoom_new_start + offset,
			zoom_new_start + zoom_new_width + offset);
    }
	

#ifdef DEBUG
sprintf(msg_str, "Mouse at time %d; y_pos %d",time_pos,y_pos);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

    cpu_pos = get_cpu_by_y(y_pos);
    if(cpu_pos != -1){
	draw_funct_detail_info(time_pos, cpu_pos);

#ifdef ORIG_BUILD_MATCHING_LIST
	func_index = get_function_index_by_position(time_pos, cpu_pos);
	funcno = get_current_func_no(cpu_pos, func_index);

#ifdef DEBUG
sprintf(msg_str, "Mouse positioned at index %d func no %d",func_index,funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
sleep(1);
#endif

	glColor3fv(get_function_color(funcno));
	glRasterPos2i(kf_width_a/2, HEIGHT*1.0);
	if(funcno >= 0){
		glColor3fv(get_function_color(funcno));

		/* get function entry and exit times */
#ifdef ORIG_BUILD_MATCHING_LIST
		func_exit_index = -1;
		func_entry_index =
		      get_function_entry_index_by_position(time_pos, cpu_pos);
		if(func_entry_index >= 0)
			func_exit_index =
		                get_matching_index(cpu_pos, func_entry_index);

		sprintf (string, "%d <--- %s ---> %d (%d)",
			(func_entry_index>=0)?graph_info[cpu_pos].
				no_frame_event[func_entry_index].start_time:-1,
				funct_info[funcno].name,
			(func_exit_index>=0)?graph_info[cpu_pos].
				no_frame_event[func_exit_index].start_time:-1,
			(func_entry_index>=0)?get_kernel_pid_from_event(
				cpu_pos, func_entry_index):-1);
#else
		func_exit_index = -1;
		func_entry_index =
		      get_function_entry_index_by_position(time_pos, cpu_pos);
		if(func_entry_index >= 0)
			func_exit_index =
		                get_matching_index(cpu_pos, func_entry_index);

{
char str1[100];
char str2[100];
		
		strcpy(str1,(func_entry_index>=0)?us_to_sec_str(
                                get_event_time(cpu_pos,func_entry_index)):
                                "no start time");
		strcpy(str2, (func_exit_index>=0)?us_to_sec_str(
                                get_time_from_index_cpu(
                                func_exit_index)): "no finish time");
		sprintf (string,
			"%s (cpu %d) <--- %s ---> %s (cpu %d) (pid %d)",
			str1,
			cpu_pos, /* XXX */
			funct_info[funcno].name,
			str2,
			(func_exit_index>=0)?get_cpu_from_index_cpu(
				func_exit_index):-1,
			(func_entry_index>=0)?get_kernel_pid_from_event(
				cpu_pos, func_entry_index):-1);
}


#ifdef DEBUG
sprintf(msg_str, "Entry time %d index 0x%x exit time %d index 0x%x",
get_event_time(cpu_pos,func_entry_index),
grtmon_message(msg_str, DEBUG_MSG, 100);
func_entry_index,
(func_exit_index>=0)?get_time_from_index_cpu(func_exit_index):-1,
func_exit_index);
#endif


#endif


#ifdef DEBUG
sprintf(msg_str, "Quals[0]: %d",
graph_info[cpu_pos].no_frame_event[func_entry_index].quals[0]);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
	}
	else{
		glColor3fv(whitevect);
		sprintf (string, "CPU %d",cpu_pos);
	}
	printString(string);

#endif

    }


    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,  kf_width_a * SCALE_FACT_KF ,  0,  kf_height_a);
    glMatrixMode(mm);

/*
    glXSwapBuffers(main_graph.display, XtWindow(main_graph.glWidget));
*/
    glXSwapBuffers(kf_graph.display, XtWindow(kf_graph.glWidget));
}


/*
 * kf_draw_ref_line: display numb_tics numbers across the line, places
 * divisions tics between them and type 0 is usecs, type 1 msecs
 */

void
kf_draw_ref_line(int numb_tics, int divisions, float width, float start_val)
{
    int adj_index;
    float pr_val;
    float end_val;
    float first_val, last_val;
    char string[255];
    int i, j, print_val;
    int at_val, at_val1;
    int last_at_val;

    end_val = start_val + width;

    glColor3fv(yellowvect);
/*
    glColor3fv(whitevect);
*/

    for (i=0;i<=numb_tics;i++) {
	pr_val =(((float)i/(float)numb_tics)*(end_val - start_val)) + start_val;

#ifdef ORIG
	if (width > 5000) { /* try to make things look neat */
	    if ((i==0) && (((int)pr_val % 1000) != 0)) 
		pr_val = pr_val + 1000;
	    print_val = ((int)pr_val / 1000)*1000;
	}
	else {
	    print_val = pr_val;
	}
#endif

	if (width > 20000) { /* try to make things look neat */
	    if ((i==0) && (((int)pr_val % 1000) != 0)) 
		pr_val = pr_val + 1000;
	    print_val = ((int)pr_val / 1000)*1000;
	}
	else if (width > 2000) {
		if ((i==0) && (((int)pr_val % 100) != 0))
		    pr_val = pr_val + 100;
		print_val = ((int)pr_val / 100)*100;
	     }
	     else if (width > 200) {
			if ((i==0) && (((int)pr_val % 10) != 0))
				pr_val = pr_val + 10;
			print_val = ((int)pr_val / 10)*10;
		  }
		  else {
			print_val = pr_val;
		  }

	if (i==0) 
	    first_val = print_val - start_val;
	if (i==numb_tics) 
	    last_val = print_val - start_val;
	at_val = (int)print_val-(int)start_val;

#ifdef PRINT_REF_TIMES_IN_USEC
	if (GraphKF_label_type == MSECS)
	    sprintf (string, "%d", (int)print_val/1000);
	else
	    sprintf (string, "%d", (int)print_val);
	
	adj_index = strlen(string);
	if ((adj_index < 0) || (adj_index > 8))
	    sprintf(msg_str, "error: (kf_draw_ref_line) graph label %s length %d out of range [1,8] %d %d",string,adj_index, start_val ,width);
grtmon_message(msg_str, DEBUG_MSG, 100);
	glRasterPos2i(at_val - ((float) width)/adj_val[adj_index],  1);
#else
	sprintf(string, "%s", us_to_sec_str((int)print_val));
	glRasterPos2i(at_val, 1);
#endif
	printString (string);
	draw_line(at_val,HEIGHT*.4,at_val,HEIGHT*.9,1,yellowvect);
	if (i !=0) {  /* draw divisions */
	    for (j=1;j<=divisions;j++) {
		at_val1 = last_at_val + (j * (float) (at_val - last_at_val)
						     / (float) (divisions +1));
		draw_line(at_val1,HEIGHT*.4,at_val1,HEIGHT*.7,1,yellowvect);
	    }
	}
	last_at_val = at_val;
    }
    draw_line(first_val,HEIGHT*.5,last_val,HEIGHT*.5,1,yellowvect);

}

/*
 * kf_draw_pos_string
 */

void
kf_draw_pos_string(int ortho, float pos, int y_pos, int val,
					       float color_vector[3], int type)
{
    char string[255];
    int adj_index;
    char msg_str[64];

    glColor3fv(color_vector);
    
    if (type == MSECS)
	sprintf (string, "%d", (int)val/1000);
    else
	sprintf (string, "%d", (int)val);
    
    adj_index = strlen(string);
    if ((adj_index < 0) || (adj_index > 8))
	sprintf(msg_str, "error: (kf_draw_pos_string) graph label %s length %d out of range [0,8]",string,adj_index);
grtmon_message(msg_str, DEBUG_MSG, 100);
    
    glRasterPos2i(pos - ((float) ortho)/adj_val[adj_index] ,  y_pos);
    printString (string);
    
}

/*
 *  kf_draw_frame_lines
 */

void kf_draw_frame_lines(int cpu)
{
    int start_val, end_val;
    char msg_str[64];

    start_val = 
	graph_info[cpu].no_frame_event[graph_info[cpu].nf_start_event].
								   start_time;
    end_val = 
	graph_info[cpu].no_frame_event[graph_info[cpu].nf_end_event].end_time;

#ifdef DEBUG
/* FK 6/25 */
{
static int xxx = 0;
if(xxx < 20){
  sprintf(msg_str, "kf_draw_frame_lines: start_val %d end_val %d", start_val, end_val);
grtmon_message(msg_str, DEBUG_MSG, 100);
  sprintf(msg_str, "kf_draw_frame_lines: kf_start_val_a %d kf_width_a %d",
				kf_start_val_a, kf_width_a);
grtmon_message(msg_str, DEBUG_MSG, 100);
}
xxx++;
}
#endif

    if (start_val < kf_start_val_a)
	start_val = kf_start_val_a;
    if (end_val > kf_start_val_a + kf_width_a)
	end_val = kf_start_val_a + kf_width_a;


#ifdef DEBUG
/* FK 6/25 */
{
static int yyy = 0;
if(yyy <20)
  sprintf(msg_str, "kf_draw_frame_lines: (after if's) start_val %d end_val %d",
			start_val, end_val);
grtmon_message(msg_str, DEBUG_MSG, 100);
yyy++;
}
#endif


    draw_line(start_val, 0.5*HEIGHT, end_val, 0.5*HEIGHT, 1, redvect);
				/* horizontal */
    draw_line(start_val, 0.0*HEIGHT, start_val, 1.0*HEIGHT, 1, redvect);
				/* vertical */
    draw_line(end_val, 0.0*HEIGHT, end_val, 1.0*HEIGHT, 1, redvect);
				/* vertical */

#ifdef DRAW_POS_STR_IN_RED
    kf_draw_pos_string((int)(end_val-start_val),start_val, (int) (-0.4*HEIGHT), 
		    (int) start_val, redvect, GraphKF_label_type);
    kf_draw_pos_string((int)(end_val-start_val), end_val, (int) (-0.4*HEIGHT), 
		    (int) end_val, redvect, GraphKF_label_type);
#endif
}

/*
 * kf_draw_events - draws events
 */

void kf_draw_events(int cpu)
{
    int i;
    int ev_start_time, ev_end_time;
    int first_in_window = 1;
    int prev_event, next_event;
    int funcno;
    int left_time, right_time;
    int flag, other_funcno;
    int end_event;
    char msg_str[64];

/* FK 6/25 */
static int xxx = 0;

    exit_cache_invalidate(cpu);
    function_position_index[cpu] = 0;
   
#ifdef DEBUG
sprintf(msg_str, "kf_draw_events: cpu %d FIRST indx %d t %d LAST indx %d t %d",
cpu,
graph_info[cpu].nf_start_event,
graph_info[cpu].no_frame_event[graph_info[cpu].nf_start_event].start_time,
graph_info[cpu].nf_end_event,
graph_info[cpu].no_frame_event[graph_info[cpu].nf_end_event].end_time);
grtmon_message(msg_str, DEBUG_MSG, 100);

sprintf(msg_str, "."); fflush(stdout);
#endif

    end_event = graph_info[cpu].nf_end_event;

    for(i=graph_info[cpu].nf_start_event; i<=end_event; i++) {

	ev_start_time = graph_info[cpu].no_frame_event[i].start_time;
	ev_end_time = graph_info[cpu].no_frame_event[i].end_time;

#ifdef DEBUG
/* FK 6/25 */
if(graph_info[cpu].nf_end_event < 50 && xxx < 20) {
sprintf(msg_str, "kf_draw_events: st t %d e t %d",ev_start_time,ev_end_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
xxx++;
}
#endif

	if (!((ev_end_time < kf_start_val_a) ||
	      (ev_start_time > (kf_start_val_a+kf_width_a)))) {

	    if (graph_info[cpu].no_frame_event[i].type == EX_EV_KERN) {
		if (graph_info[cpu].no_frame_event[i].disp) {
		    /* draw kernel arrows */

#ifdef DEBUG
/* FK 6/26 */
{
static int www = 0;
if(www < 30)
   sprintf(msg_str, "   kf_draw_events: (kernel, disp) st t %d end t %d",
ev_start_time, ev_end_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
www++;
}
#endif

#ifdef ORIG
		    if (ev_start_time >= kf_start_val_a)
			draw_arrow(kf_width_a, 0, 0, ev_start_time,
					      EX_EV_KERN, ex_ev_kernvect, 100);
		    
		    if (ev_end_time <= (kf_start_val_a+kf_width_a))
			draw_arrow(kf_width_a, 0, 0, ev_end_time,
					      EX_EV_KERN, ex_ev_kernvect, 101);
		    
		    /* draw kernel box */
		    if (ev_start_time < kf_start_val_a)
			ev_start_time = kf_start_val_a;
		    if (ev_end_time > (kf_start_val_a+kf_width_a))
			ev_end_time = kf_start_val_a+kf_width_a;
		    draw_rect(ev_start_time, 0.5*HEIGHT, ev_end_time, 
			      0.5*HEIGHT-RECT_WIDTH*HEIGHT, ex_ev_kernvect);
#endif

		    draw_function_divisor(cpu,i);

		    if(first_in_window){
			prev_event = get_previous(cpu,i);
			flag = 0;
		        funcno = get_current_func_no(cpu, prev_event,
				     &flag, &other_funcno,NULL,NULL,NULL,NULL);
#ifdef DEBUG
sprintf(msg_str, "\tFirst: index %d prev %d funcno %d",i,prev_event,funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
			first_in_window = 0;

			if(funcno >= 0){
			   funct_info_insert(funcno);
		           left_time = graph_info[cpu].no_frame_event[
						prev_event].start_time;
			   if(left_time < kf_start_val_a)
			     left_time = kf_start_val_a;
			   right_time = graph_info[cpu].no_frame_event[
						i].start_time;

#ifdef DEBUG
sprintf(msg_str, "\tFirst Rect: left %d right %d func %d",left_time,right_time,funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
			   draw_funct_rect(left_time, right_time, flag, funcno,
					   other_funcno);

/*
			   draw_rect(left_time, 0.5*HEIGHT, right_time,
				0.5*HEIGHT-RECT_WIDTH*HEIGHT,
				get_function_color(funcno));
*/

			   if(is_enough_space_for_funct_name(funcno,
							right_time-left_time))
				draw_function_name(funcno,left_time);

			   /* fill info to allow locating function given
			      time */
			   function_position[cpu][function_position_index[cpu]].
							func_index = prev_event;
			   function_position[cpu][function_position_index[cpu]].
							left_time  = left_time;
			   function_position[cpu][function_position_index[cpu]].
							right_time = right_time;
			   function_position_index[cpu]++;
			}
		    }

		    next_event = get_next(cpu,i);
		    flag = 0;
		    funcno = get_current_func_no(cpu, i,
		               &flag, &other_funcno,NULL,NULL, NULL,NULL);
#ifdef DEBUG
sprintf(msg_str, "\tindex %d next %d funcno %d",i,next_event,funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

		    if(funcno >= 0){
			funct_info_insert(funcno);
			left_time=graph_info[cpu].no_frame_event[i].start_time;
			if(next_event > 0) {
			    right_time = graph_info[cpu].no_frame_event[
							next_event].start_time;
			    if(right_time >= kf_start_val_a + kf_width_a)
				right_time = kf_start_val_a + kf_width_a;
			}
			else {
#ifdef DEBUG
			    sprintf(msg_str, "Warning: Function %d: unknown end",
						funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
			    right_time = kf_start_val_a + kf_width_a;
			}
#ifdef DEBUG
sprintf(msg_str, "\tRect: left %d right %d func %d",left_time,right_time,funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
/*
			draw_rect(left_time, 0.5*HEIGHT, right_time,
				0.5*HEIGHT-RECT_WIDTH*HEIGHT,
				get_function_color(funcno));
*/

			draw_funct_rect(left_time, right_time, flag, funcno,
					   other_funcno);

			if(is_enough_space_for_funct_name(funcno,
							right_time-left_time))
			    draw_function_name(funcno,left_time);


			function_position[cpu][function_position_index[cpu]].
							func_index = i;
			function_position[cpu][function_position_index[cpu]].
							left_time  = left_time;
			function_position[cpu][function_position_index[cpu]].
							right_time = right_time;
			function_position_index[cpu]++;
		   }

		}
	    }
	    
	    else if((graph_info[cpu].no_frame_event[i].type==EX_EV_INTR_ENTRY)||
		    (graph_info[cpu].no_frame_event[i].type==EX_EV_INTR_EXIT)) {
		if ((ev_start_time >= kf_start_val_a) && 
		    (ev_start_time <= (kf_start_val_a + kf_width_a))) {
		    draw_arrow(kf_width_a, 0, 0, ev_start_time,
			graph_info[cpu].no_frame_event[i].type,
		        ex_ev_intrvect, 100);
		}
	    }
	    
	    else if (graph_info[cpu].no_frame_event[i].type == EX_EV_USER) {
		if ((ev_start_time >= kf_start_val_a) && 
		    (ev_start_time <= (kf_start_val_a + kf_width_a)))
		    draw_arrow(kf_width_a, 0, 0, ev_start_time,
			graph_info[cpu].no_frame_event[i].type,
		        ex_ev_uservect, 100);

#ifdef DEBUG
/* FK 6/26 */
{
static int aaa = 0;
if(aaa < 80)
   sprintf(msg_str, "   kf_draw_events: (EX_EV_USER) st t %d end t %d",
ev_start_time, ev_end_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
aaa++;
}
#endif
	    }
	    
	    else {  
		/* a user event */
		if (ev_start_time < kf_start_val_a)
		    ev_start_time = kf_start_val_a;
		if (ev_end_time > (kf_start_val_a+kf_width_a))
		    ev_end_time = kf_start_val_a+kf_width_a;
		/* check and see whether it's the idle loop or real process */
		if (graph_info[cpu].no_frame_event[i].type == 0) {
		    draw_rect(ev_start_time, 0.5*HEIGHT+RECT_WIDTH*HEIGHT,
				ev_end_time, 0.5*HEIGHT, ex_ev_idlevect);
		}
		else {

#ifdef DEBUG
/* FK 6/26 */
{
static int zzz = 0;
if(zzz < 30)
   sprintf(msg_str, "   kf_draw_events: (user, not idle) st t %d end t %d",
ev_start_time, ev_end_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
zzz++;
}
#endif

		    draw_rect(ev_start_time, 0.5*HEIGHT+RECT_WIDTH*HEIGHT,
				ev_end_time, 0.5*HEIGHT-RECT_WIDTH*HEIGHT, 
			      proc_color[(2*(graph_info[cpu].no_frame_event[i].
				                type-1))%(2*numb_proc_colors)]);
		    glRasterPos2i(ev_start_time,  HEIGHT*1.0);

		    printString (
			  pid_name_map[graph_info[cpu].no_frame_event[i].pid]);
		}
	    }
	}
    }

    if(first_in_window){ /* no events occurred in this interval; must find out
			    whether we are inside some function */

	prev_event = graph_info[cpu].nf_start_event;

#ifdef DEBUG
sleep(1);
sprintf(msg_str, "\tfirst_in_window still on; prev_event %d",prev_event);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

	if(prev_event >= 0) {
	    /* first find last event before window */
	    while((prev_event+1 < graph_info[cpu].no_frame_event_count) &&
	          (graph_info[cpu].no_frame_event[prev_event+1].start_time <
					kf_start_val_a))
		prev_event++;

	    if(!(is_kernel_function(cpu,prev_event)))
		prev_event = get_previous(cpu,prev_event);

#ifdef DEBUG
sprintf(msg_str, "\tfirst_in_window still on II; prev_event %d",prev_event);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

	    if((prev_event >= 0) &&
	       (graph_info[cpu].no_frame_event[prev_event].start_time <=
					kf_start_val_a + kf_width_a)) {
		flag = 0;
	        funcno = get_current_func_no(cpu, prev_event,
		               &flag, &other_funcno,NULL,NULL,NULL,NULL);
#ifdef DEBUG
sprintf(msg_str, "\tFirst[no ev]: prev %d funcno %d",prev_event,funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
		if(funcno >= 0){
		     funct_info_insert(funcno);
		     left_time = kf_start_val_a;
		     right_time = kf_start_val_a + kf_width_a;

#ifdef DEBUG
sprintf(msg_str, "\tFirst Rect: left %d right %d func %d",left_time,right_time,funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

		     draw_funct_rect(left_time, right_time, flag, funcno,
					   other_funcno);

/*
		     draw_rect(left_time, 0.5*HEIGHT, right_time,
				0.5*HEIGHT-RECT_WIDTH*HEIGHT,
				get_function_color(funcno));
*/

		     if(is_enough_space_for_funct_name(funcno,
							right_time-left_time))
			   draw_function_name(funcno,left_time);

		     function_position[cpu][function_position_index[cpu]].
							func_index = prev_event;
		     function_position[cpu][function_position_index[cpu]].
							left_time  = left_time;
		     function_position[cpu][function_position_index[cpu]].
							right_time = right_time;
		     function_position_index[cpu]++;
		}
	   }
	}

#ifdef DEBUG
sprintf(msg_str, "ev indx START %d END %d",graph_info[cpu].nf_start_event,
graph_info[cpu].nf_end_event);
grtmon_message(msg_str, DEBUG_MSG, 100);
if(graph_info[cpu].nf_start_event>=0)
   sprintf(msg_str, "-- time %d",
graph_info[cpu].no_frame_event[graph_info[cpu].nf_start_event].start_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
sleep(2);
#endif

    }
}

/*
 * is_enough_space_for_funct_name - determines if there is enough space in
 *	screen to draw function name
 *
 * XXX not considering function name in this version
 */

int is_enough_space_for_funct_name(int funcno, int time_interval)
{
	Dimension dim_width,dim_height;
	int width,height;
	float points_per_time_unit;
    char msg_str[64];

	XtVaGetValues(kf_graph.glWidget, XmNwidth, &dim_width,
                      XmNheight, &dim_height, NULL);

	width = (int) dim_width;
	points_per_time_unit = (float)width/kf_width_a;

#ifdef DEBUG
sprintf(msg_str, "is_enough_space_for_funct_name: interv %d pts per un %f",
time_interval,points_per_time_unit);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

	if(time_interval*points_per_time_unit >= MIN_POINTS_TO_PRINT_NAME)
		return 1;
	else
		return 0;
}

/*
 * draw_function_name - draw function name
 */

void draw_function_name(int funcno, int x_pos)
{
	char *name;

	if((name = funct_info[funcno].name) == NULL)
		name = "NO NAME";
	glColor3fv(get_function_color(funcno));
	glRasterPos2i(x_pos + EXTRA_SPACE_FOR_FUNCT_NAME, HEIGHT*0.6);
	printString(name);
}


/*
 * draw_function_divisor - draw divisor between functions; indicate whether
 *	it is a function entry or exit
 */

void draw_function_divisor(int cpu, int ev_index)
{
	int x_pos = graph_info[cpu].no_frame_event[ev_index].start_time;

#ifdef OLD
	draw_line(x_pos, 0.1*HEIGHT, x_pos, 0.8*HEIGHT, 1, whitevect);
							/* vertical line */
#endif

	glColor3fv(whitevect);
	if(is_kernel_function_entry(cpu, ev_index)){
		glBegin(GL_LINE_STRIP);
		glVertex2f(x_pos + FUNCTION_DIVISOR_X_LENGTH, (-0.2)*HEIGHT);
		glVertex2f(x_pos, (-0.2)*HEIGHT);
		glVertex2f(x_pos, 0.9*HEIGHT);
		glVertex2f(x_pos + FUNCTION_DIVISOR_X_LENGTH, 0.9*HEIGHT);
		glEnd();
	}
	if(is_kernel_function_exit(cpu, ev_index)){
		glBegin(GL_LINE_STRIP);
		glVertex2f(x_pos - FUNCTION_DIVISOR_X_LENGTH, (-0.2)*HEIGHT);
		glVertex2f(x_pos, (-0.2)*HEIGHT);
		glVertex2f(x_pos, 0.9*HEIGHT);
		glVertex2f(x_pos - FUNCTION_DIVISOR_X_LENGTH, 0.9*HEIGHT);
		glEnd();
	}
}

/*
 * draw_time_line - draw the time line, marking a time interval
 */

void draw_time_line()
{
	Dimension dim_width,dim_height;
	int width,height;
	float points_per_time_unit;
	int time_interval, screen_time_interval;
	int left, right;
	int arrow_tail_x;
	int offset;
	int text_pos;
	int out_of_bound_left = 0;
	int out_of_bound_right = 0;
	int num_points_to_draw_number;
	char string[256];

	offset = kf_width_a * LEFT_BORDER_KF - kf_start_val_a;

	/* first draw vertical lines */
	if((kf_time_line_begin >= 0) &&
	   (kf_time_line_begin >= kf_start_val_a) &&
	   (kf_time_line_begin < kf_start_val_a+kf_width_a))
		draw_line(kf_time_line_begin + offset, 0,
		  	  kf_time_line_begin + offset,
						kf_height_a, 2, whitevect);

	if((kf_time_line_end >= 0) &&
	   (kf_time_line_end >= kf_start_val_a) &&
	   (kf_time_line_end < kf_start_val_a+kf_width_a))
		draw_line(kf_time_line_end + offset, 0,
		  	  kf_time_line_end + offset,
						kf_height_a, 2, whitevect);

	/* time line not active */
	if((kf_time_line_begin < 0) || (kf_time_line_end < 0))
		return;

	/* both lines would be both to the left or both to the right of
	   the screen */
	if((kf_time_line_begin < kf_start_val_a) &&
	   (kf_time_line_end   < kf_start_val_a))
		return;
	if((kf_time_line_begin >= kf_start_val_a + kf_width_a) &&
	   (kf_time_line_end   >= kf_start_val_a + kf_width_a))
		return;

	time_interval = kf_time_line_end - kf_time_line_begin;
	strcpy(string, us_to_valunit_str(time_interval));
	num_points_to_draw_number = (strlen(string) >= BIG_NUMBER_STRING)?
			NUM_POINTS_TO_DRAW_TIME_NUMBER:
			NUM_POINTS_TO_DRAW_SMALL_TIME_NUMBER;

	if(kf_time_line_begin < kf_start_val_a){
		out_of_bound_left  = 1;
		left = kf_start_val_a;
	}
	else
		left = kf_time_line_begin;
	if(kf_time_line_end   >= kf_start_val_a+kf_width_a){
		out_of_bound_right = 1;
		right = kf_start_val_a+kf_width_a;
	}
	else
		right = kf_time_line_end;

	screen_time_interval = right - left;

	XtVaGetValues(kf_graph.glWidget, XmNwidth, &dim_width,
                      XmNheight, &dim_height, NULL);

	width = (int) dim_width;
	points_per_time_unit = (float)width/kf_width_a;

	glColor3fv(whitevect);

	if(screen_time_interval*points_per_time_unit >=
	   					MIN_POINTS_TO_DRAW_TIMELINE){
			/* we have enough space for a little art ... */

/*
 *     | <---------------     time   --------------> |
 *   begin          arrow_tail_x
 */
		arrow_tail_x =  left + offset + (screen_time_interval -
		   (float)num_points_to_draw_number/points_per_time_unit)*
		   0.5;
		text_pos = arrow_tail_x + NUM_POINTS_SEPARATE_ARROW_TEXT/
					  points_per_time_unit;

		/* left arrow */
		glBegin(GL_LINE_STRIP);
		glVertex2f(left + offset
		    + NUM_POINTS_TO_DRAW_ARROW/points_per_time_unit,
		    HEIGHT+Y_TO_DRAW_ARROW);
		glVertex2f(left + offset, HEIGHT);
		glVertex2f(left + offset
		    + NUM_POINTS_TO_DRAW_ARROW/points_per_time_unit,
		    HEIGHT-Y_TO_DRAW_ARROW);
		glEnd();

		glBegin(GL_LINE_STRIP);
		glVertex2f(left + offset, HEIGHT);
		glVertex2f(arrow_tail_x, HEIGHT);
		glEnd();


		arrow_tail_x =  right + offset - (screen_time_interval -
		   (float)num_points_to_draw_number/points_per_time_unit)*
		   0.5;

		/* right_arrow */
		glBegin(GL_LINE_STRIP);
		glVertex2f(right + offset
		    - NUM_POINTS_TO_DRAW_ARROW/points_per_time_unit,
		    HEIGHT+Y_TO_DRAW_ARROW);
		glVertex2f(right + offset, HEIGHT);
		glVertex2f(right + offset
		    - NUM_POINTS_TO_DRAW_ARROW/points_per_time_unit,
		    HEIGHT-Y_TO_DRAW_ARROW);
		glEnd();

		glBegin(GL_LINE_STRIP);
		glVertex2f(right + offset, HEIGHT);
		glVertex2f(arrow_tail_x, HEIGHT);
		glEnd();


		/* time interval */
		glRasterPos2i(text_pos, HEIGHT);
	}
	else { /* no enough space for arrow */
		glRasterPos2i(right + offset
		  + NUM_POINTS_SEPARATE_ARROW_TEXT/points_per_time_unit,HEIGHT);
	}

/*
	sprintf (string, "%d", time_interval);
	printString (string);
*/
	printString(us_to_valunit_str(time_interval));
}


/*
 * draw_zoom_frame - draw frame when zoom button is activated
 */

void draw_zoom_frame(int left_position, int right_position)
{

	if(left_position <= right_position) {
		glColor3fv(whitevect);
		glLineWidth(2.0);

		glBegin(GL_LINE_LOOP);
		glVertex2f(left_position, ZOOM_FRAME_HEIGHT);
		glVertex2f(left_position, kf_height_a - ZOOM_FRAME_HEIGHT);
		glVertex2f(right_position, kf_height_a - ZOOM_FRAME_HEIGHT);
		glVertex2f(right_position, ZOOM_FRAME_HEIGHT);
		glEnd();

		glLineWidth(1.0);
	}
}


/*
 * do_progressive_zoom - implement progressive zoom
 */

void do_progressive_zoom()
{
	int change;

	if(kf_width_a > zoom_new_width) {
		change = zoom_width_change/ZOOM_NSTEPS;
		if(change < 1)
			change = 1;
		kf_width_a -= change;
		if(kf_width_a < zoom_new_width)
			kf_width_a = zoom_new_width;
	}
	else
		kf_width_a = zoom_new_width;
	if(kf_start_val_a < zoom_new_start) {
		change = zoom_start_change/ZOOM_NSTEPS;
		if(change < 1)
			change = 1;
		kf_start_val_a += change;
		if(kf_start_val_a > zoom_new_start)
			kf_start_val_a = zoom_new_start;
	}
	else
		kf_start_val_a = zoom_new_start;

	if((kf_width_a == zoom_new_width) &&
	   (kf_start_val_a == zoom_new_start)){
		zoom_state = Z_NO_ZOOM;		/* task completed */
		kf_zoom_line = -1;
	}

	kf_readjust_start_and_end();
}

/*
 * draw_function_search - draw arrow indicating the last function for which
 *	the last search last successful
 */

void draw_function_search()
{
	float head_height;
	float offset;

	offset = kf_width_a * LEFT_BORDER_KF - kf_start_val_a;

	/* search_found_cpu != -1 */

	if((search_found_time < kf_start_val_a) ||
	   (search_found_time >= kf_start_val_a + kf_width_a))
		return;		/* out of window */

	glColor3fv(yellowvect);

	head_height = (2*HEIGHT*(gr_numb_cpus - search_found_cpu));

	/* head */
	glBegin(GL_POLYGON);
		glVertex2f(search_found_time + offset, head_height);
		glVertex2f(search_found_time+offset - SEARCH_ARROW_HEAD_WIDTH/2,
			   head_height - SEARCH_ARROW_HEAD_HEIGHT);
		glVertex2f(search_found_time+offset + SEARCH_ARROW_HEAD_WIDTH/2,
			   head_height - SEARCH_ARROW_HEAD_HEIGHT);
	glEnd();

	/* body */
	glBegin(GL_POLYGON);
		glVertex2f(search_found_time +offset-SEARCH_ARROW_BODY_WIDTH/2, 
				head_height - SEARCH_ARROW_HEAD_HEIGHT);
		glVertex2f(search_found_time+offset -SEARCH_ARROW_BODY_WIDTH/2,
			   head_height -
			    SEARCH_ARROW_HEAD_HEIGHT-SEARCH_ARROW_BODY_HEIGHT);
		glVertex2f(search_found_time+offset +SEARCH_ARROW_BODY_WIDTH/2,
			   head_height -
			    SEARCH_ARROW_HEAD_HEIGHT-SEARCH_ARROW_BODY_HEIGHT);
		glVertex2f(search_found_time +offset+SEARCH_ARROW_BODY_WIDTH/2, 
				head_height - SEARCH_ARROW_HEAD_HEIGHT);
	glEnd();
}

/*
 * draw_kf_cursor - draw cursor
 */

void draw_kf_cursor()
{
	int offset;
	char string[256];

	offset = kf_width_a * LEFT_BORDER_KF - kf_start_val_a;

	if((kf_cursor_time >= 0) &&
	   (kf_cursor_time >= kf_start_val_a) &&
	   (kf_cursor_time < kf_start_val_a+kf_width_a)) {
		draw_line(kf_cursor_time + offset, 0,
		  	  kf_cursor_time + offset, kf_height_a, 1, yellowvect);

        	sprintf(string, "%s", us_to_sec_str(kf_cursor_time));
        	glRasterPos2i(kf_cursor_time+offset, 1.0*HEIGHT);
    		glColor3fv(yellowvect);
        	printString (string);
	}
}

/*
 * draw_funct_rect: draw a color rectangle corresponding to one function
 *
 *	The 'flag' and 'other_funcno' parameters come directly from a
 *	get_current_func_no() call
 */

void draw_funct_rect(int left_time, int right_time, int flag, int funcno,
		     int other_funcno)
{
	float width;
	float top;
	int nfunct = 1;
    char msg_str[64];

#ifdef DEBUG
sprintf(msg_str, "draw_funct_rect: L %d R %d fl 0x%x fno %d ofno %d",
left_time, right_time, flag, funcno, other_funcno);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

	if(funcno == -1)
		return;

	width = KF_WIDTH_RECT_WIDE * HEIGHT;
	top   = KF_TOP_RECT_WIDE * HEIGHT;

	if((flag & CURR_OTHER_FCT) && (other_funcno >= 0))
		nfunct = 2;
	else
		if(flag & (CURR_PID_CHANGED|CURR_NO_MATCH)){
			width = KF_WIDTH_RECT_NARROW * HEIGHT;
			top   = KF_TOP_RECT_NARROW * HEIGHT;
		}

	if(nfunct == 1)
		draw_rect(left_time, top, right_time,
                                top - width,
                                get_function_color(funcno));
	else {
		/* -------------------
		  |.		     |
		  |     .  other_f   |
	          |  funcno  .       |
	          |		 .   |
	           ------------------- */
		glColor3fv(get_function_color(funcno));
		glBegin(GL_POLYGON);
			glVertex2f((float)left_time, top - width);
			glVertex2f((float)left_time, top);
			glVertex2f((float)right_time, top - width);
		glEnd();

		glColor3fv(get_function_color(other_funcno));
		glBegin(GL_POLYGON);
			glVertex2f((float)left_time, top);
			glVertex2f((float)right_time, top);
			glVertex2f((float)right_time, top - width);
		glEnd();
	}
}

/*
 * draw_funct_detail_info - draw detail info regarding the current
 *	function(s). Parameters are time and cpu (supposed to be the
 *	position of the mouse)
 */

void draw_funct_detail_info(int curr_time, int cpu)
{
	int func_index;
	int funcno;
	int flag, other_func_no;
	int func_entry_index;
	int func_exit_index;
	int cpu_exit;
	int other_func_index;
	int cpu_entry;
	int other_cpu_entry;
	int nfunct = 0;
	int index;
	char str1[100];
	char str2[100];
	char string[256];
	int pid;
	char cmdname[256];
    char msg_str[64];
	
	func_index = get_function_index_by_position(curr_time, cpu);
#ifdef DEBUG
sprintf(msg_str, "draw_funct_detail_info: after get_funct... func_index %d fl 0x%x",
func_index, flag);
grtmon_message(msg_str, DEBUG_MSG, 100);
sprintf(msg_str, "draw_funct_detail_info: calling get_current_func_no cp %d ind %d",
cpu, func_index);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

/*
	flag = CURR_DONT_USE_CACHE | CURR_PRINT_DEBUG;
*/
	flag = CURR_DONT_USE_CACHE;
	funcno = get_current_func_no(cpu, func_index, &flag, &other_func_no,
				     &func_entry_index, &cpu_entry,
				     &other_func_index, &other_cpu_entry);

#ifdef DEBUG
sprintf(msg_str, "draw_funct_detail_info: t %d cp %d fno %d fl 0x%x oth fno %d",
curr_time, cpu, funcno, flag, other_func_no);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

#ifdef SLEEP
sleep(1);
#endif

	if(funcno >= 0){
		nfunct++;
		if(flag & CURR_OTHER_FCT)
			nfunct++;
	}

	glRasterPos2i(kf_width_a/2, HEIGHT*1.0);
	glColor3fv(whitevect);

	if(funcno >= 0){
		glColor3fv(get_function_color(funcno));
		glRasterPos2i(kf_width_a/2, HEIGHT*1.0);

		/* get function entry and exit times */
		func_exit_index = -1;
		cpu_exit = -1;
		if((func_entry_index >= 0) && (cpu_entry >= 0)){
			index = get_matching_index(cpu_entry, func_entry_index);
			if(index != -1){
			      func_exit_index = get_index_from_index_cpu(index);
			      cpu_exit        = get_cpu_from_index_cpu(index);
			}
		}

		strcpy(str1,((func_entry_index>=0) && (cpu_entry >= 0))?
				us_to_sec_str(get_event_time(
						cpu_entry,func_entry_index)):
                                "no start time");
		strcpy(str2, ((func_exit_index>=0) && (cpu_exit >= 0))?
				us_to_sec_str(get_time_from_index_cpu(index)):
				"no finish time");

		pid = ((func_entry_index >=0) && (cpu_entry >= 0))?
			get_kernel_pid_from_event(
				cpu_entry, func_entry_index):-1;
		get_cmdname_by_pid(pid, cmdname);
#ifdef DEBUG
sprintf(msg_str, "draw_funct_detail_info: TEST: pid %d name %s", pid, cmdname);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

		sprintf (string,
			"%s (cpu %d) <--- %s ---> %s (cpu %d) %s [%d]",
			str1,
			cpu_entry,
			funct_info[funcno].name,
			str2,
			(func_exit_index>=0)?get_cpu_from_index_cpu(
				index):-1,
			cmdname,
			pid);

		printString(string);

		if(nfunct == 2) {
			glColor3fv(get_function_color(other_func_no));
			glRasterPos2i(kf_width_a/2, HEIGHT*0.60);

			/* get function entry and exit times */
			func_exit_index = -1;
			cpu_exit = -1;
			if((other_func_index >= 0) && (other_cpu_entry >= 0)){
				index = get_matching_index(other_cpu_entry,
							   other_func_index);
				if(index != -1){
			        	func_exit_index =
					       get_index_from_index_cpu(index);
			      		cpu_exit        =
					       get_cpu_from_index_cpu(index);
				}
			}

			strcpy(str1,((other_func_index >= 0) &&
			             (other_cpu_entry  >= 0))?
				us_to_sec_str(get_event_time(other_cpu_entry,
					other_func_index)):
                                "no start time");
			strcpy(str2, ((func_exit_index>=0) &&
				      (cpu_exit >= 0))?
				us_to_sec_str(get_time_from_index_cpu(index)):
				"no finish time");

			pid = ((other_func_index >=0)& (other_cpu_entry >= 0))?
			   get_kernel_pid_from_event(
				other_cpu_entry, other_func_index):-1;
			get_cmdname_by_pid(pid, cmdname);

			sprintf (string,
			   "%s (cpu %d) <--- %s ---> %s (cpu %d) %s [%d]",
			   str1,
			   other_cpu_entry,
			   funct_info[other_func_no].name,
			   str2,
			   (func_exit_index>=0)?get_cpu_from_index_cpu(
				index):-1,
			   cmdname,
			   pid);


			printString(string);

		}
	}
	else{
		glColor3fv(whitevect);
		sprintf (string, "CPU %d",cpu);
		printString(string);
	}
}



#define KF_WIDTH_INITIAL_VALUE		(1)

/*
 * kf_set_reasonable_start - in case we finished reeading the whole file
 *	before the graphics had a chance to start this will at least
 *	display something reasonable
 */

void kf_set_reasonable_start(int cpu)
{
    char msg_str[64];
/* FK 6/25 */
	int reasonable_cnt = (graph_info[cpu].no_frame_event_count >= 1001)?
			1000: graph_info[cpu].no_frame_event_count-1;


#ifdef XXXX
    /* if somebody else hasn't already specified a display range */
    if ((graph_info[cpu].nf_start_event == 0) &&
	(graph_info[cpu].nf_end_event == 0)) {
#endif
    if(kf_width_a == KF_WIDTH_INITIAL_VALUE){
	kf_width_a = graph_info[cpu].no_frame_event[reasonable_cnt].end_time - 
	    graph_info[cpu].no_frame_event[1].start_time;

#ifdef DEBUG
/* FK 6/25 */
sprintf(msg_str, "kf_set_reasonable_start: set kf_width_a to %d",kf_width_a);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

	kf_start_val_a = graph_info[cpu].no_frame_event[1].start_time;
        kf_force_start_to_zero = 1;
	kf_readjust_start_and_end(); /* (graph-kfunct-cb.c) */
    }

}

/*
 * kf_graph_init - initialization ; called on first call to
 * manage_kern_funct_graph_CB()
 */

void kf_graph_init()
{
    char msg_str[64];

sprintf(msg_str, "kf_graph_init");
grtmon_message(msg_str, DEBUG_MSG, 100);

    kf_max_zoomout_flag = 0;
    kf_cursor_time = -1;

    read_color_db();
    read_funcname_color_db();

    /* copied from init_graphic_vars() */
    GraphKF_label_type = 0;
    GraphKF_divisions = 3;
    GraphKF_tics = 4;

    kf_refresh_rate = 1000000;

    kf_width_a = KF_WIDTH_INITIAL_VALUE;
    kf_height_a = 2*HEIGHT*gr_numb_cpus + HEIGHT;  /* similar to one of the
							"ortho" variables */
    kf_max_val_a = 1;
    kf_start_val_a = 1;

    kf_zoom_line = -1;
    zoom_state =  Z_NO_ZOOM;
    kf_time_line_begin = -1;
    kf_time_line_end = -1;

    /* variables used by graph-kfunct-cb.c */
    kf_disp_mode = WIDE;
    range_kf = 0;

    /* temporary */
    kf_focus_max = 1000;
    kf_focus_min = 10;
}

/*
 * get_max_time - obtain maximum time among known events
 */

int get_max_time()
{
	int i;
	int max_time = 0;
	int cpu;
    char msg_str[64];

        for (i=0; i<gr_numb_cpus; i++) {
            if (cpuOn[i]) {
                 cpu = gr_cpu_map[i];
{
static int yyy = 0;
if(yyy < 2)
   sprintf(msg_str, "get_max_time: cpu %d no_frame_event_count-1 %d time %d",cpu,
graph_info[cpu].no_frame_event_count-1,graph_info[cpu].no_frame_event[
graph_info[cpu].no_frame_event_count-1].start_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
yyy ++;
}
		 if(graph_info[cpu].no_frame_event[
			graph_info[cpu].no_frame_event_count-1].start_time
			> max_time)
		     max_time =
			graph_info[cpu].no_frame_event[
			   graph_info[cpu].no_frame_event_count-1].start_time;
            }
        }

{
static int xxx = 0;
if(xxx == 0)
   sprintf(msg_str, "get_max_time: %d",max_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
xxx = 1;
}
	return max_time;
}

/* ================ microseconds to printable representation ============= */

/*
 * us_to_valunit_str - given value in microseconds, convert to a string
 *	using "appropriate" units
 */

char *us_to_valunit_str(int us_value)
{
	static char string[100];

	if(us_value < 1000)
		sprintf(string,"%d us",us_value);
	else
		if (us_value < 1000000)
			sprintf(string,"%.3f ms", (float)us_value/1000);
		else
			sprintf(string,"%.6f s", (float)us_value/1000000);

	return string;
}

/*
 * us_to_sec_str - the same as above, but stick to seconds
 */

char *us_to_sec_str(int us_value)
{
	static char string[100];

	sprintf(string,"%.6f s", (float)us_value/1000000);
	return string;
}

