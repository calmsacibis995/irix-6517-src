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
 * File:	graph-kfunct.h					          *
 *									  *
 * Description:	Definitions for the kernel function graph	          *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

/*
 * Defines
 */

#define KF_MAX_NUMBER_EVENTS_TO_DISPLAY		200

/*
#define SEC_COL_A (LEFT_BORDER_A -0.055)
#define CPU_COL_A (LEFT_BORDER_A -0.040) 
#define CPUN_COL_A (LEFT_BORDER_A -0.030)
#define LEFT_COL_A 0.005
*/

#define KF_SEC 0.008
#define KF_CPU 0.025
#define KF_CPU1 0.033


/* #define THUMB_KF_MIN 250  */ /* based on THUMB_B_MIN */
#define THUMB_KF_MIN		1 /* 10 */  /* relative value */
#define THUMB_KF_MAX		1000  /* relative value */
#define THUMB_KF_UNITS_PER_ROTATION  200
#define THUMB_KF_ANGLE_RANGE		2000
#define KF_RANGE		1000.0

#define KF_RANGE_SLIDER_INCR_WIDTH_MULTIPLIER	(0.05)
#define KF_RANGE_SLIDER_PAGE_INCR_WIDTH_MULTIPLIER (1.0)

#define LEFT_BORDER_KF 0.07
#define TOP_BORDER_KF  (-5.0)  /* based on TOP_BORDER_B */
#define SCALE_FACT_KF 1.12


#define KF_X_START_P (LEFT_BORDER_KF/SCALE_FACT_KF)
#define KF_X_END_P ((1.0/SCALE_FACT_KF) + (LEFT_BORDER_KF/SCALE_FACT_KF))
#define KF_X_ERR_P 0.01

#define KF_START_PER (LEFT_BORDER_KF/SCALE_FACT_FK)
#define KF_END_PER ((1.0/SCALE_FACT_KF) + (LEFT_BORDER_KF/SCALE_FACT_KF))

#define MIN_POINTS_TO_PRINT_NAME	35
#define FUNCTION_DIVISOR_X_LENGTH	(ceil((double)(kf_width_a * 0.005)))
#define EXTRA_SPACE_FOR_FUNCT_NAME	(kf_width_a * 0.002)

#define MIN_POINTS_TO_DRAW_TIMELINE	95
#define NUM_POINTS_TO_DRAW_TIME_NUMBER	85
#define NUM_POINTS_TO_DRAW_SMALL_TIME_NUMBER	55
#define BIG_NUMBER_STRING		7
#define NUM_POINTS_TO_DRAW_ARROW	10
#define Y_TO_DRAW_ARROW			2
#define NUM_POINTS_SEPARATE_ARROW_TEXT	10

#define ZOOM_FRAME_HEIGHT	(0.6*HEIGHT)

/* attempt for a gradual zooming */
extern int zoom_new_start;		/* start position after zoom */
extern int zoom_new_width;		/* width after zoom */
extern int zoom_start_change;		/* delta start */
extern int zoom_width_change;		/* delta width */

enum zoom_st { Z_NO_ZOOM, Z_LOCATING_ZOOM, Z_IMPLEM_ZOOM};
					/* zoom states */
extern enum zoom_st zoom_state;
#define ZOOM_NSTEPS		7

#define HELP_WINDOW_WIDTH	500
#define HELP_WINDOW_HEIGHT	500

#define HELP_FILE_NAME		"frameview.hlp"

/* maximum zoom out */
extern int kf_max_zoomout_flag;
extern int kf_orig_start;	/* original start and width before zoom out */
extern int kf_orig_width;

extern int kf_force_start_to_zero; /* force start time to be zero
				      (kf_readjust_start_and_end()) */

/* function search */
extern int search_found_cpu;
extern int search_found_time;

#define SEARCH_ARROW_HEAD_WIDTH		(double)(kf_width_a * 0.008)
#define SEARCH_ARROW_HEAD_HEIGHT	(0.3*HEIGHT)
#define SEARCH_ARROW_BODY_WIDTH		(double)(kf_width_a * 0.002)
#define SEARCH_ARROW_BODY_HEIGHT	(0.4*HEIGHT)

/* the cursor */
extern int kf_cursor_time;	/* time where cursor is */

/* function rectangle */
#define KF_WIDTH_RECT_NARROW		0.35
#define KF_TOP_RECT_NARROW		0.50
#define KF_WIDTH_RECT_WIDE		0.50
#define KF_TOP_RECT_WIDE		0.60

/*
 * Function prototypes
 */

void thumb_kf_CB (Widget w, XtPointer clientD, XtPointer callD);
void range_kf_CB (Widget w, XtPointer clientD, XtPointer callD);
void close_kf_graph_CB (Widget w, XtPointer clientD, XtPointer callD);
void resize_kf_graph(Widget w, XtPointer data, XtPointer callData);
void manage_kern_funct_graph_CB (Widget w, XtPointer clientD, XtPointer callD);
void get_mouse_pos_KF (Widget w, XtPointer clientD, XtPointer callD);

int is_enough_space_for_funct_name(int funcno, int time_interval);
void draw_function_name(int funcno, int x_pos);
void draw_function_divisor(int cpu, int ev_index);
void draw_time_line();
void draw_zoom_frame(int left_position, int right_position);
void do_progressive_zoom();

void draw_function_search();
void draw_kf_cursor();

int get_max_time();

void getCursorPosition(int *time_pos, int *y_pos);
int get_cpu_by_y(int y_pos);

void help_using_CB(Widget w, XtPointer clientD, XtPointer callD);
void close_help_using_CB(Widget w, XtPointer clientD, XtPointer callD);
char *get_help_as_string();

void find_function_CB(Widget w, XtPointer clientD, XtPointer callD);
void close_find_function_CB(Widget w, XtPointer clientD, XtPointer callD);
void find_next_function_button_CB(Widget w, XtPointer clientD, XtPointer callD);
void find_from_beginning_button_CB(Widget w, XtPointer clientD, XtPointer callD);
void find_from_end_button_CB(Widget w, XtPointer clientD, XtPointer callD);
void obtain_cpu_and_funcname(int *cpu, char *funcname);

void seek_to_function(int cpu, char funcname[]);
void seek_to_function_from_beginning(int cpu, char funcname[]);
void seek_to_function_from_end(int cpu, char funcname[]);

char *us_to_valunit_str(int us_value);
char *us_to_sec_str(int us_value);

void draw_funct_rect(int left_time, int right_time, int flag, int funcno,
                     int other_funcno);
void draw_funct_detail_info(int curr_time, int cpu);

