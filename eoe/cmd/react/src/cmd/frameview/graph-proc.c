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
 * File:	graph-proc.c					          *
 *									  *
 * Description:	This file draws the processor graph                       *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

void draw_maj_proc_lines();
void draw_center_string(int ortho, float x_pos, float y_pos, char *str, float color_vector[3]);


void
draw_proc_graph()
{
    GLint mm;

    /* make this the active gl widget to draw on */
    glXMakeCurrent(proc_graph.display, 
		   XtWindow(proc_graph.glWidget), 
		   proc_graph.context);
    
    
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3sv(bkgnd);
    
    
    glTranslatef((LEFT_BORDER_C/(float)proc_graph.width)*ortho_c,  HEIGHT*2.5,  0);
    
    draw_maj_proc_lines();
    
    glTranslatef((LEFT_BORDER_C/(float)proc_graph.width)*ortho_c*-1,  HEIGHT*-2.5,  0);
    
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,  ortho_c + 10,  0,  14*HEIGHT);
    glMatrixMode(mm);
    glXSwapBuffers(proc_graph.display, XtWindow(proc_graph.glWidget));

}



#define SINGLE_C 18.0
#define DOUBLE_C 26.0
#define TRIPLE_C 34.0
#define PER_C 40.0
#define BIG_TIC_C 4.8
#define SMALL_TIC_C 2.0
#define GAP_C 50.0

void
draw_maj_proc_lines()
{
    int adj_index;
    int i,ii,j,k,l;
    float percent;
    float start_val, end_val;
    char string[255];
    float cpu_start, cpu_end;
    float act_cpu_start, act_cpu_end;
    float mid;
    float cur_x, bar_width;
    float total_time;
    int to_disp_cpus, room_for_cpus, start_cpu, cpu_width;
    float fw;
    char msg_str[64];

    
    fw = (float)proc_graph.width;
    to_disp_cpus = 0;
    for (i=0;i<gr_numb_cpus;i++) {
	if (cpuOn[i]) {
	    to_disp_cpus++;
	}
    }

    room_for_cpus = (int)ortho_c/100;


    if (room_for_cpus < to_disp_cpus) {
	cpu_width = (int)((float)ortho_c / (float)room_for_cpus);
	start_cpu = (int)((float)(to_disp_cpus - room_for_cpus) * ((float)range_c/100));
    }
    else {
	cpu_width = (int)((float)ortho_c / (float)to_disp_cpus);
	start_cpu = 0;
    }

    percent = (float)ortho_c/(float)(MAJ_PROC_WIDTH*to_disp_cpus);;

    start_val = (((1-percent)*(MAJ_PROC_WIDTH*to_disp_cpus)) *
		 ((float)range_c/100));
    
    end_val = start_val + ortho_c;

 
    i=-1;
    for (l=0;l<gr_numb_cpus;l++) {
	ii = gr_cpu_map[l];
	if (cpuOn[l]) {
	    i++;
	    cpu_start = i*MAJ_PROC_WIDTH;
	    cpu_end = ((i+1)*MAJ_PROC_WIDTH) - (GAP_C/fw)*ortho_c;
	    
	    act_cpu_start = cpu_start - start_val;
	    if (act_cpu_start >= end_val) goto nexti; /* not in view */
	    if (act_cpu_start < (-0.5)*MAJ_PROC_WIDTH) goto nexti; /* only seeing part */

	    act_cpu_end = cpu_end - start_val;
	    if (act_cpu_end <= 0)  goto nexti; /* not in view */
	    if (act_cpu_end > end_val + (0.25*MAJ_PROC_WIDTH)) goto nexti; /* only seeing part*/

	    /* draw y axis then x axis */
	    draw_line(act_cpu_start,0,act_cpu_start,10*HEIGHT,1,yellowvect);
	    draw_line(act_cpu_start,-0.5,act_cpu_end,-0.5,1,yellowvect);

	    /* draw tics on y axis */
	    glColor3fv(yellowvect);
	    sprintf(string, "0\n");
	    glRasterPos2i(act_cpu_start-(SINGLE_C/fw)*ortho_c , 0-0.15*HEIGHT);
	    printString (string);		
	    sprintf(string, "100\n");
	    glRasterPos2i(act_cpu_start-(TRIPLE_C/fw)*ortho_c , 10*HEIGHT-0.15*HEIGHT);
	    printString (string);		
	    sprintf(string, "%% cpu\n");
	    glRasterPos2i(act_cpu_start-(PER_C/fw)*ortho_c , 11*HEIGHT-0.15*HEIGHT);
	    printString (string);
	    for (j=1;j<=10;j=j++) {
		draw_line(act_cpu_start-(BIG_TIC_C/fw)*ortho_c,j*HEIGHT,
			  act_cpu_start+(BIG_TIC_C/fw)*ortho_c,j*HEIGHT,
			  1,yellowvect);
		draw_line(act_cpu_start-(SMALL_TIC_C/fw)*ortho_c,j*HEIGHT-0.5*HEIGHT,
			  act_cpu_start+(SMALL_TIC_C/fw)*ortho_c,j*HEIGHT-0.5*HEIGHT,
			  1,yellowvect);
	    }
	    for (j=1;j<=9;j=j++) {
		sprintf(string, "%d\n",j*10);
		glRasterPos2i(act_cpu_start-(DOUBLE_C/fw)*ortho_c , j*HEIGHT-0.15*HEIGHT);
		printString (string);
	    }
	    mid = (act_cpu_start+act_cpu_end)/2;
	    sprintf(string, "cpu %d\n",ii);
	    adj_index = strlen(string);
	    if ((adj_index < 0) || (adj_index > 6)) {
		sprintf(msg_str, "(3) graph label %s length %d out of range [1,6]\n",string,adj_index);
		grtmon_message(msg_str, ALERT_MSG, 33);
	    }

	    glRasterPos2i(mid - ((float) ortho_c)/adj_val[adj_index],  -2*HEIGHT);
	    printString (string);

	    /* now draw actual bars */
	    cur_x = act_cpu_start + (BIG_TIC_C/fw)*ortho_c;
	    bar_width = (act_cpu_end - cur_x)/major_frame_account[ii].template_processes;

	    total_time = (float) (major_frame_account[ii].template_minors *
				  major_frame_account[ii].minor_time);
	    /* draw idle process bar */
	    draw_rect(cur_x, 10*HEIGHT*
		      (((float) (graph_info[ii].maj_ave_process[0].ktime +
				 graph_info[ii].maj_ave_process[0].utime))/
		       total_time),
		      cur_x + bar_width, 0, whitevect);
	    draw_center_string(ortho_c, cur_x + bar_width/2.0, 
			       -1*HEIGHT, "idle", whitevect);
	    draw_line(cur_x, 10*HEIGHT*
		      (((float)graph_info[ii].maj_ave_process[0].max_time)/total_time),
		      cur_x + bar_width, 10*HEIGHT*
		      (((float)graph_info[ii].maj_ave_process[0].max_time)/total_time),
		      1, redvect);

	    cur_x = cur_x + bar_width;
	    
	    for (k=1;k<major_frame_account[ii].template_processes;k++) {
		draw_rect(cur_x, 10*HEIGHT*
		   (((float) (graph_info[ii].maj_ave_process[k].ktime +
			      graph_info[ii].maj_ave_process[k].utime))/
		    total_time),
			  cur_x + bar_width, 0, proc_color[(2*(k-1))%(2*numb_proc_colors)]);
		
		draw_center_string(ortho_c, cur_x + bar_width/2.0, 
				   -1*HEIGHT, 
				   graph_info[ii].maj_ave_process[k].name,
				   proc_color[(2*(k-1))%(2*numb_proc_colors)]);
	   
		draw_line(cur_x, 10*HEIGHT*
		      (((float)graph_info[ii].maj_ave_process[k].max_time)/total_time),
		      cur_x + bar_width, 10*HEIGHT*
		      (((float)graph_info[ii].maj_ave_process[k].max_time)/total_time),
		      1, whitevect);

		cur_x = cur_x + bar_width;
	    }
	}
	
      nexti:;
    }



}


void
draw_center_string(int ortho, float x_pos, float y_pos, char *str, float color_vector[3])
{
    int adj_index;
    
    glColor3fv(color_vector);
    
    adj_index = strlen(str);
    if (adj_index > 6) {
	adj_index = 5;
	str[6] = '\0';
    }
    
    glRasterPos2i(x_pos - ((float) ortho)/adj_val[adj_index] ,  y_pos);
    printString (str);
}
