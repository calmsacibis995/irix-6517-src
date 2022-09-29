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
 * File:	graph-kern.c					          *
 *									  *
 * Description:	This file draws the kernel graph                          *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

void draw_kern_ref_line(int tics, int divs);
void draw_kern_times(int cpu);
void draw_kern_maxes(int cpu);
void draw_kern_frame_lines (int line_end, int numb_minors, int minor_time);
void draw_max_kern_line_str(float x_pos, int print_val);
void draw_pos_kmax_string(float pos, int y_pos, int val, float color_vector[3]);


/* draw the kernel graph */
void
draw_kern_graph()
{
    static char string[255];
    int i,ii,j,k,temp_count;
    GLint mm;
    int numb_cpus_on;

    /* make this the active gl widget to draw on */
    glXMakeCurrent(kern_graph.display, XtWindow(kern_graph.glWidget), kern_graph.context);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3sv(bkgnd);
    
    
    glColor3fv(yellowvect);
    sprintf (string, "usecs");
    glRasterPos2i(ortho_b * .005,  0.5*HEIGHT);
    printString (string);
    
    
    /* draw scale at bottom for reference */
    glTranslatef(ortho_b*LEFT_BORDER_B,  0,  0);
    draw_kern_ref_line(GraphA_tics,GraphA_divisions);
    
    glTranslatef(ortho_b*-LEFT_BORDER_B,  HEIGHT,  0);
    
    
    numb_cpus_on=0;
    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	ii = gr_cpu_map[i];
	if (cpuOn[i]) {
	    numb_cpus_on++;
	    
	    glColor3fv(yellowvect);
	    sprintf (string, "cpu %d", ii);
	    glRasterPos2i(ortho_b * .005,  HEIGHT / 2);
	    /*glRasterPos2i(0,  HEIGHT / 2);*/
	    printString (string);
	    
	    glTranslatef(ortho_b * LEFT_BORDER_B,  0,  0);
	    
	    draw_kern_times(ii);
	    draw_kern_maxes(ii);
	    
	    draw_kern_frame_lines (ortho_b, 
				   major_frame_account[ii].template_minors,
				   major_frame_account[ii].minor_time);
	    
	    glTranslatef(0,  HEIGHT,  0);
	    
	    glColor3sv(bkgnd);
	    glRasterPos2i(0,  0.1*HEIGHT);
	    
	    /* the following lines of code is a big hack, gl was
	       not willing to display mutliple colors of strings
	       written back to back in different locations 
	       what we do then is actually write the same string
	       several times in the same position each time the
	       next color we really wanted */
	    
	    
	    temp_count = 
		graph_info[ii].ave_minor[kern_disp_minor[ii]].kern_ev_start_count;
	    
	    for (k=temp_count; k>=0; k--) {
		if (k==temp_count)
		    glColor3fv(whitevect);
		else
		    glColor3fv(kern_color[k%KERN_COLORS]);
		glRasterPos2i(0,  0.1*HEIGHT);
		
		
		for (j=0; j<=k; j++) {
		    printString (tstamp_event_name[graph_info[ii].ave_minor[kern_disp_minor[ii]].kern_ev_start_event[j]]);
		    printString ("   ");
		    if (j==temp_count-1) j++;
		    
		}
		if (k==temp_count)
		    printString (" max value");
		
	    }
	    
	    
	    glTranslatef(ortho_b * -LEFT_BORDER_B,  HEIGHT,  0);
	    
	}
    }
    
    glTranslatef(0,  ((2.0*HEIGHT * numb_cpus_on + HEIGHT) * -1),  0);
    
    /* each cpu bar + bottom ref line + chop off to make look good */
    ortho_by = 2.0*HEIGHT*numb_cpus_on + HEIGHT + TOP_BORDER_B;
    
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,  ortho_b * SCALE_FACT_B,  0, ortho_by);
    glMatrixMode(mm);
    glXSwapBuffers(kern_graph.display, XtWindow(kern_graph.glWidget));
    
}




void
draw_kern_ref_line(int tics, int divs)
{

    int adj_index;
    float start[2], end[2];
    float percent, pr_val;
    float start_val, end_val;
    float first_val, last_val;
    char string[255];
    int i, j, print_val;
    int at_val, at_val1;
    int last_at_val;
    char msg_str[64];

    
    if (kern_disp_mode == WIDE) {
	percent = (float)ortho_b/(float)max_val_a;
	start_val = (((1-percent)*max_val_a) * ((float)range_b/100));
    }
    else {
	percent = (float)ortho_b/(float)(kern_focus_max - kern_focus_min);
	start_val = kern_disp_val + 
	    (((1-percent)*(float)(kern_focus_max - kern_focus_min)) * ((float)range_b/100));
    }

    end_val = start_val + ortho_b;

    glColor3fv(yellowvect);

    for (i=0;i<=tics;i++) {
	pr_val = (((float)i/(float)tics)*(end_val - start_val)) + start_val;
	if ((i==0) && (((int)pr_val % 10) != 0)) pr_val = pr_val + 10;
	print_val = ((int)pr_val / 10)*10;
	if (i==0) first_val = print_val - start_val;
	if (i==tics) last_val = print_val - start_val;
	at_val = (int)print_val-(int)start_val;

	sprintf (string, "%d", (int)print_val);
	
	adj_index = strlen(string);
	if ((adj_index < 0) || (adj_index > 8)) {
	    sprintf(msg_str, "graph label %s length %d out of range [1,8]",string,adj_index);
	    grtmon_message(msg_str, ALERT_MSG, 101);
	}
	glRasterPos2i(at_val - ((float) ortho_b)/adj_val[adj_index],  1);
	printString (string);
	draw_line(at_val,HEIGHT*.4,at_val,HEIGHT*.9,1,yellowvect);
	if (i !=0) {  /* draw divisions */
	    for (j=1;j<=divs;j++) {
		at_val1 = last_at_val + (j * (float) (at_val - last_at_val) / (float) (divs +1));
		draw_line(at_val1,HEIGHT*.4,at_val1,HEIGHT*.7,1,yellowvect);
	    }
	}
	last_at_val = at_val;
    }
    draw_line(first_val,HEIGHT*.5,last_val,HEIGHT*.5,1,yellowvect);
}



void draw_kern_times(int cpu)
{
    int current_time;
    int i,j;
    float exp_fact;

    current_time = 0;
    

    for (i=0;i<major_frame_account[cpu].template_minors;i++) {
	exp_fact = 1.0;
	if (graph_info[cpu].max_kern_minor[i].ex_ev_val > 0)
	    exp_fact = (float)(ortho_b*EXP_PER)/
		(float)graph_info[cpu].max_kern_minor[i].ex_ev_val;
	if (exp_fact < 1) exp_fact = 1.0;
	
	current_time = i*major_frame_account[cpu].minor_time;

	if (kern_disp_mode == FOCUS) {
	    for (j=0; j<graph_info[cpu].ave_minor[i].kern_ev_start_count-1; j++) {
		draw_frame_rect(ortho_b, max_val_a, range_b, 
				graph_info[cpu].ave_minor[i].kern_ev_start_time[j]+current_time,
				graph_info[cpu].ave_minor[i].kern_ev_start_time[j+1]+current_time,
				1, kern_color[j%KERN_COLORS], FALSE, "", FALSE, 0, 0, 0);
		
	    }
/* see TIMESTAMP_ADJ_COMMENT in frameview.h
	    if (adjust_mode == OFF)
*/
		draw_frame_rect(ortho_b, max_val_a, range_b, 
				graph_info[cpu].ave_minor[i].kern_ev_start_time[j]+current_time,
				graph_info[cpu].ave_minor[i].kern_ev_start_time[j]+current_time + tstamp_adj_time,
				1, kern_color[j%KERN_COLORS], FALSE, "", FALSE, 0, 0, 0);
		
	  }
	else {
	    for (j=0; j<graph_info[cpu].ave_minor[i].kern_ev_start_count-1; j++) {
		draw_frame_rect(ortho_b, max_val_a, range_b, current_time, current_time = 
				current_time + exp_fact*
				(graph_info[cpu].ave_minor[i].kern_ev_start_time[j+1] -
				graph_info[cpu].ave_minor[i].kern_ev_start_time[j]),
				1, kern_color[j%KERN_COLORS], FALSE, "", FALSE, 0, 0, 0);
	    }
/* see TIMESTAMP_ADJ_COMMENT in frameview.h
	    if (adjust_mode == OFF)
*/
		draw_frame_rect(ortho_b, max_val_a, range_b, current_time, current_time = 
				current_time + exp_fact*tstamp_adj_time,
				1, kern_color[j%KERN_COLORS], FALSE, "", FALSE, 0, 0, 0);	    
	}

    }
}

void draw_kern_maxes(int cpu)
{
    int i;
    float exp_fact;

    for (i=0;i<major_frame_account[cpu].template_minors;i++) {
	exp_fact = 1.0;
	if (graph_info[cpu].max_kern_minor[i].ex_ev_val > 0)
	    exp_fact = (float)(ortho_b*EXP_PER)/
		(float)graph_info[cpu].max_kern_minor[i].ex_ev_val;
	if (exp_fact < 1) exp_fact = 1.0;

	draw_max_kern_line_str(i*major_frame_account[cpu].minor_time+
			       exp_fact*graph_info[cpu].max_kern_minor[i].ex_ev_val,
			       graph_info[cpu].max_kern_minor[i].ex_ev_val);
    }

}

void
draw_max_kern_line_str(float x_pos, int print_val)
{
  float percent;
  float start_val, end_val;
  float act_x;
  char string[255];
 
  
  if (kern_disp_mode == WIDE) {
      percent = (float)ortho_b/(float)max_val_a;
      start_val = (((1-percent)*max_val_a) * ((float)range_b/100));
  }
  else {
	percent = (float)ortho_b/(float)(kern_focus_max - kern_focus_min);
	start_val = kern_disp_val + 
	    (((1-percent)*(float)(kern_focus_max - kern_focus_min)) * ((float)range_b/100));
  }

  end_val = start_val + ortho_b;

  act_x = x_pos - start_val;

  if ((act_x > 0) && (act_x < end_val)) {
      draw_line(act_x,HEIGHT*.1,act_x,HEIGHT*.9,1,whitevect);
      draw_pos_kmax_string(act_x, HEIGHT*-0.1, print_val, whitevect);
  }
}





void draw_kern_frame_lines (int line_end, int numb_minors, int minor_time)
{


    float y1,y2;

    float x_pos, percent;
    char string[255];
    int i,adj_index;

    percent = (float)line_end/(float)max_val_a;



    draw_line(0, HEIGHT*.5, (numb_minors*minor_time - 
	      (((1-percent)*(max_val_a)) * ((float)range_b/100))),
	      HEIGHT*.5, 1, redvect);
    
    if (kern_disp_mode == FOCUS) 
	draw_line(0,0,0,HEIGHT,1,redvect);
    else {
	for (i=0;i<=numb_minors;i++) {
	    x_pos = i*minor_time - (((1-percent)*max_val_a) * 
				    ((float)range_b/100));
	    if ((x_pos+EPSILON_B >= 0) && (x_pos-EPSILON_B <= line_end)) {
		if ((i==0) || (i==numb_minors)) { /* major frame boundary */
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
    }
}


void
draw_pos_kmax_string(float pos, int y_pos, int val, float color_vector[3])
{
    char string[255];
    
    glColor3fv(color_vector);
    sprintf (string, "%d", (int)val);
    
    glRasterPos2i(pos,  y_pos);
    printString (string);
    
}
