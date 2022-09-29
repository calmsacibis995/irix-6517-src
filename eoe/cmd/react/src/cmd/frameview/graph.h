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
 * File:	rtprof.h 						  *          
 *									  *
 * Description:	This file defines the structures and decalred all the     *
 *              variables needed by the graphical portion of the          *
 *              real-time performance monitoring tools                    *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

/* Motif includes */
#include <Xm/Xm.h>
#include <Xm/AtomMgr.h>
#include <Xm/CascadeB.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/FileSB.h>
#include <Xm/Label.h>
#include <Xm/MainW.h>
#include <Xm/MessageB.h>
#include <Xm/MwmUtil.h>
#include <Xm/PanedW.h>
#include <Xm/PushB.h>
#include <Xm/Protocols.h>
#include <Xm/RowColumn.h>
#include <Xm/Scale.h>
#include <Xm/ScrollBar.h>
#include <Xm/SeparatoG.h>
#include <Xm/Separator.h>
#include <Xm/SelectioB.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>



#include <GL/GLwMDrawA.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <Sgm/ThumbWheel.h>
#include <Mrm/MrmPublic.h>

#include <fmclient.h>
#include <X11/Shell.h> 
#include <X11/Xlib.h>
#include <X11/Xutil.h>


#define STATMAIN extern
#include "stats.h"


#define HEIGHT 10
#define TOP_BORDER_A 5.0
#define TOP_BORDER_B (-5.0)  /* we actually slice it off */
#define RECT_WIDTH 0.35
#define INTR_WIDTH 0.40
#define INTR_TRI_HEIGHT 1.5
#define INTR_TRI_WIDTH 0.0075  /* percentage of ortho */

/* DO NOT CHANGE THIS unless you check for all occurrences of 100 
   in the code and also take care of range_b */
#define B_RANGE 100.0
#define C_RANGE 100.0

#define MAX_SLIDER_A 100

#define A_THUMB 1000
#define B_THUMB 1000

#define LEFT_BORDER_A 0.17
#define SCALE_FACT_A 1.2

#define NF_LEFT_BORDER 0.09
#define NF_SCALE_FACT 1.12

#define LEFT_BORDER_B 0.07
#define SCALE_FACT_B 1.1

#define LEFT_BORDER_C 40.0

#define MAXCPU MAX_PROCESSORS	/* maximum number of cpus allowed */

#define KERN_COLORS 8   /* number of kernel startup events */

#define WIDE 0
#define FOCUS 1

/* search modes */
#define FROM_START 0
#define FROM_CUR 1

#define SUMMARIZED 0
#define EXPLICIT 1

#define BY_TIME 0
#define AS_IS 1

#define THUMB_B_MIN 250

#define EXP_PER 0.03

#define MAJ_PROC_WIDTH 100

#define DEFAULT_AVE_VAL 1
#define MAX_COLORS 30

#define USECS 0
#define MSECS 1

#define NUMB_TICS 11
#define NUMB_DIVS 6

#define EPSILON 1 /* a very small x adjustment */
#define EPSILON_B 3 /* a very small x adjustment */

/* don't change without being careful, these are dependent on 
   menu position */
#define COMPUTED 0
#define ACTUAL 1

#define SCROLL_BAR_WIDTH 22

typedef struct {
    int width, height;
    Display *display;
    Widget glWidget, topForm;
    Window window;
    GLXContext context;
} glx_wind_t;

typedef struct {
    int cpu,minor,process;  /* -1 in cpu indicates no show last */
} gr_last_show_t;



/* Function prototypes */
Boolean drawGraphs (XtPointer);
void installColormap (Widget toplevel, Widget glw);
void scroll_a_CB (Widget, XtPointer, XtPointer);
void inc_scroll_a_CB (Widget, XtPointer, XtPointer);
void dec_scroll_a_CB (Widget, XtPointer, XtPointer);
void exposeCB (Widget, XtPointer, XtPointer);
void enableCB (Widget, XtPointer, XtPointer);
void enableMenuCB (Widget, XtPointer, XtPointer);
void stop_menu_CB (Widget, XtPointer, XtPointer);
void reset_maj_ave_CB (Widget, XtPointer, XtPointer);
void maj_ave_CB (Widget, XtPointer, XtPointer);
void manage_kern_graph_CB (Widget, XtPointer, XtPointer);
void manage_kern_funct_graph_CB (Widget, XtPointer, XtPointer);
void text_display (boolean text_on);
void manage_proc_graph_CB (Widget, XtPointer, XtPointer);
void get_mouse_pos_A (Widget w, XtPointer clientD, XtPointer callD);
void proc_key_down_A (Widget w, XtPointer clientD, XEvent *event, Boolean *unused);
void get_mouse_pos_B (Widget w, XtPointer clientD, XtPointer callD);
void get_mouse_pos_C (Widget w, XtPointer clientD, XtPointer callD);
void range_a_CB (Widget, XtPointer, XtPointer);
void range_b_CB (Widget, XtPointer, XtPointer);
void range_c_CB (Widget, XtPointer, XtPointer);
void close_kern_graph_CB (Widget, XtPointer, XtPointer);
void close_proc_graph_CB (Widget, XtPointer, XtPointer);
void close_raw_tstamp_CB (Widget, XtPointer, XtPointer);
void close_search_CB (Widget, XtPointer, XtPointer);
void close_open_file_CB (Widget, XtPointer, XtPointer);
void define_template_CB (Widget w, XtPointer clientD, XtPointer callD);
void close_define_template_CB (Widget w, XtPointer clientD, XtPointer callD);
void close_goto_frame_CB (Widget w, XtPointer clientD, XtPointer callD);
void goto_frame_number_CB (Widget w, XtPointer clientD, XtPointer callD);
void goto_frame_done_CB (Widget w, XtPointer clientD, XtPointer callD);
void TextChangedCB (Widget w, XtPointer clientD, XtPointer callD);
void TextVerifyCB (Widget w, XtPointer clientD, XtPointer callD);
void unit_CB (Widget w, XtPointer clientD, XtPointer callD);
void tic_CB (Widget w, XtPointer clientD, XtPointer callD);
void div_CB (Widget w, XtPointer clientD, XtPointer callD);
void next_cpu_frame_CB (Widget w, XtPointer clientD, XtPointer callD);
void prev_cpu_frame_CB (Widget w, XtPointer clientD, XtPointer callD);
void frame_display_CB (Widget w, XtPointer clientD, XtPointer callD);
void adjust_CB (Widget w, XtPointer clientD, XtPointer callD);
void graph_mode_CB (Widget w, XtPointer clientD, XtPointer callD);
void frame_sync_CB (Widget w, XtPointer clientD, XtPointer callD);
void reset_proc_ave_CB (Widget, XtPointer, XtPointer);
void proc_ave_CB (Widget, XtPointer, XtPointer);
void search_event_CB (Widget, XtPointer, XtPointer);
void Forward_Search_CB (Widget, XtPointer, XtPointer);
void Reverse_Search_CB (Widget, XtPointer, XtPointer);
void goto_frame_CB (Widget, XtPointer, XtPointer);
void quit_CB (Widget, XtPointer, XtPointer);
void scaleCB (Widget, XtPointer, XtPointer);
void thumbCB (Widget, XtPointer, XtPointer);
void zoom_control_CB (Widget, XtPointer, XtPointer);
void zoom1_CB (Widget, XtPointer, XtPointer);
void zoom2_CB (Widget, XtPointer, XtPointer);
void zoom3_CB (Widget, XtPointer, XtPointer);
void zoom4_CB (Widget, XtPointer, XtPointer);
void zoom5_CB (Widget, XtPointer, XtPointer);
void zoom6_CB (Widget, XtPointer, XtPointer);
void zoom7_CB (Widget, XtPointer, XtPointer);
void zoom8_CB (Widget, XtPointer, XtPointer);
void zoom9_CB (Widget, XtPointer, XtPointer);
void zoom0_CB (Widget, XtPointer, XtPointer);
void scroll_l1_CB (Widget, XtPointer, XtPointer);
void scroll_l2_CB (Widget, XtPointer, XtPointer);
void scroll_l10_CB (Widget, XtPointer, XtPointer);
void scroll_r1_CB (Widget, XtPointer, XtPointer);
void scroll_r2_CB (Widget, XtPointer, XtPointer);
void scroll_r10_CB (Widget, XtPointer, XtPointer);
void file_real_CB (Widget w, XtPointer clientD, XtPointer callD);
void scan_file_next_CB();
void scan_file_prev_CB();
void thumb_b_CB (Widget, XtPointer, XtPointer);
void thumb_c_CB (Widget, XtPointer, XtPointer);
void init_proc_colors(int numb_colors);
void back_color_CB ();
void reset_maxes_CB ();
void open_file_CB();
void frame_start_events_CB();
void help_general_CB();
void start_live_stream_CB();
void file_sys_full_error();
void FileFullCB (Widget w, XtPointer clientD, XtPointer callD);
void reset_graph_maxes();
void toggle_text_disp_CB (Widget, XtPointer, XtPointer);
void prefCancelCB (Widget, XtPointer, XtPointer);
void changeColorCB (Widget, XtPointer, XtPointer);
void clear_stats_and_menus();
void reset_all_maxes();
void set_slider_a_width_and_loc();
void start_search(int cpu);
void obtain_search_info(int *cpu, int *event, int *type, int *mode);
void obtain_goto_frame_number(int *cpu, int *number);
void continue_search(int cpu, int event, int type);
void goto_frame_number(int number, int cpu);
void get_event_type(int *event, int *type, char *event_str);
void define_template(int cpu , int frame);
void get_scan_line_val();
void frame_start_events_CB();
void trigger_CB();
void maj_CB();
void min_CB();

void draw_line(float x1,float y1,float x2,float y2,int width, float color_vector[3]);
void draw_frame_rect_new (int ortho, int max_val, float start, float end, 
		      int height_flag, float color_vector[3], boolean label, 
		      char label_str[5], boolean time_lab, int utime, 
		      int ktime, float kcolor_vector[3]);
void draw_frame_rect (int ortho, int max_val, float scale, float start, float end, 
		      int height_flag, float color_vector[3], boolean label, 
		      char label_str[5], boolean time_lab, int utime, 
		      int ktime, float kcolor_vector[3]);
void draw_rect (float left, float top, float right, float bot, float color_vector[3]);
void draw_triangle (float p1x, float p1y, float p2x, float p2y, float p3x, float p3y, float color_vector[3]);
void draw_arrow (int ortho, int max_val, float my_scale, float pos, int type, float color_vector[3], int orient);
void draw_arrow_new (int ortho, int max_val, float pos, int type, float color_vector[3], int orient);

void readjust_nf_start_and_end(int print_it);

void draw_main_no_frame_graph();
void draw_main_graph();
void draw_kern_graph();
void draw_proc_graph();

Widget makeMenus (Widget parent);



extern float blackvect[];
extern float redvect[];
extern float red1vect[];
extern float greenvect[];
extern float green1vect[];
extern float bluevect[];
extern float blue1vect[];
extern float yellowvect[];
extern float yellow1vect[];
extern float purplevect[];
extern float purple1vect[];
extern float cyanvect[];
extern float cyan1vect[];
extern float magentavect[];
extern float magenta1vect[];
extern float whitevect[];
extern float greyvect[];
extern float idlevect[];
extern float ex_ev_idlevect[];
extern float ex_ev_kernvect[];
extern float ex_ev_intrvect[];
extern float ex_ev_uservect[];

extern float backgnd[];
extern short bkgnd[];

extern float kern_color[KERN_COLORS][3];
/*extern GLXconfig rgb_mode[];*/
extern int dblBuf[];
extern int *snglBuf;
extern int zBuf[];


extern int adj_val[16];

GRMAIN XtAppContext app_context;
GRMAIN XtWorkProcId work_procid;
GRMAIN volatile int cpuOn[MAXCPU];
GRMAIN volatile glx_wind_t main_graph, kern_graph, kern_funct_graph, proc_graph;
GRMAIN volatile int kern_graph_on, kern_funct_graph_on, proc_graph_on;
GRMAIN volatile fmfonthandle fhScale8, fhScale10, fhScale12;

GRMAIN volatile Widget enable[MAXCPU];
GRMAIN volatile Widget toplevel, master_toplevel;

GRMAIN volatile int display_frame_type;
GRMAIN volatile Widget frame_display_val[2];
GRMAIN volatile Widget graph_mode_val[2];
GRMAIN volatile Widget frame_sync_val[2];

GRMAIN volatile Widget scroll_a, scroll_b, scroll_c;

GRMAIN volatile int gr_cpu_map[MAXCPU];
GRMAIN volatile int gr_numb_cpus;
GRMAIN volatile float range_b, range_c;
GRMAIN volatile float nf_range_a;

GRMAIN float proc_color[MAX_COLORS][3];
GRMAIN volatile int numb_proc_colors;
GRMAIN float kern_startup_color[3];

GRMAIN volatile Widget proc_ave_menu[MAXCPU][MAX_NUMB_TO_AVE+1];
GRMAIN volatile int proc_ave_last[MAXCPU];
GRMAIN volatile Widget maj_ave_menu[MAXCPU][MAX_NUMB_TO_AVE+1];
GRMAIN volatile int maj_ave_last[MAXCPU];
GRMAIN volatile Widget unitVal[2];
GRMAIN volatile Widget file_real_val[2];
GRMAIN volatile int GraphA_label_type;
GRMAIN volatile int GraphA_divisions, GraphA_tics;
GRMAIN volatile Widget ticVal[NUMB_TICS], divVal[NUMB_DIVS];
GRMAIN volatile int gr_max_processes;
GRMAIN volatile int kern_disp_mode, kern_disp_val, kern_disp_minor[MAXCPU];
GRMAIN volatile int width_ay, width_a_min;
GRMAIN volatile long long nf_width_a, nf_start_val_a, nf_max_val_a;
GRMAIN volatile int width_a, start_val_a, max_val_a;
GRMAIN volatile nf_refresh_rate; /* in usecs */
GRMAIN volatile int ortho_b, ortho_by, ortho_c, ortho_emp;
GRMAIN volatile gr_last_show_t gr_last_show;
GRMAIN volatile Widget next_frame_menu[MAXCPU];
GRMAIN volatile Widget prev_frame_menu[MAXCPU];
GRMAIN volatile Widget TstampTextDisplay, ToggleTextMenu;
GRMAIN volatile Widget StopMenu, StopMenuSep;
GRMAIN volatile boolean text_display_on;
GRMAIN volatile Widget enableButton, enableMenu, enableAll;
GRMAIN volatile Widget mainForm, FileSysFull;
GRMAIN volatile Widget thumbWheel_a;
GRMAIN volatile Widget kern_button, proc_button, kern_funct_button;
GRMAIN volatile Widget glWidgetC; 
GRMAIN volatile Widget searchWindow, SearchCpuText, SearchEventText;
GRMAIN volatile Widget OpenFileText;
GRMAIN volatile Widget OpenFileWindow;
GRMAIN volatile Widget StartLiveStreamWindow;
GRMAIN volatile Widget GotoFrameWindow, GotoNumberText, GotoCpuText;
GRMAIN volatile Widget ZoomWindow, ZoomFromText, ZoomToText;
GRMAIN volatile Widget DefineTemplateWindow;
GRMAIN volatile long long zoom_line_l, zoom_line_r;
GRMAIN volatile long long scan_line_m, scan_line_l, scan_line_r;
GRMAIN volatile Widget thumbWheel_b;
GRMAIN volatile Widget thumbWheel_c;
GRMAIN volatile int kern_focus_min, kern_focus_max;
GRMAIN volatile Widget NextFrameAll, PrevFrameAll;
GRMAIN volatile Widget NextFrameCpu, PrevFrameCpu;
GRMAIN volatile Widget NextFrameMenu, PrevFrameMenu;
GRMAIN volatile Widget NextFrameSep, PrevFrameSep;
GRMAIN volatile Widget FileLiveButton;
GRMAIN volatile Widget maj_val[MAX_CHOICE_EVENTS];
GRMAIN volatile Widget min_val[MAX_CHOICE_EVENTS];
GRMAIN volatile Widget trigger_val[MAX_CHOICE_EVENTS];

GRMAIN volatile int graph_mode; 
GRMAIN volatile int frame_sync; 
GRMAIN char old_event_list[EVENT_LIST_SIZE];

GRMAIN volatile Widget adjust_val[2];

GRMAIN volatile glx_wind_t kf_graph;
GRMAIN volatile Widget scroll_kf;
GRMAIN volatile float range_kf;
GRMAIN volatile int GraphKF_label_type;
GRMAIN volatile int GraphKF_divisions, GraphKF_tics;
GRMAIN volatile int kf_disp_mode, kf_disp_val;
GRMAIN volatile int kf_width_a, kf_start_val_a, kf_max_val_a;
GRMAIN volatile int kf_height_a;
GRMAIN volatile kf_refresh_rate; /* in usecs */
GRMAIN volatile int kf_zoom_line;
GRMAIN volatile int kf_time_line_begin, kf_time_line_end;
GRMAIN volatile Widget thumbWheel_kf;
GRMAIN volatile int kf_focus_min, kf_focus_max;

#define MAX_DIALOGS 10
GRMAIN int msg_in_use[MAX_DIALOGS];

GRMAIN char *help_text;
GRMAIN XVisualInfo *master_vi;

