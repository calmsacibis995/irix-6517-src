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
 * File:	graph.c 					          *
 *									  *
 * Description:	This file does the initialization and contains general    *
 *              tools for frameview                     		  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN
#include "graph.h"

void init_graphic_vars();

boolean last_text_display_on;

GLuint times14_base;

static String   frameviewResources[] = {
    "*sgiMode: true",	   /* enable IRIX 5.2+ look & feel */
    "*useSchemes: all",	   /* and SGI schemes */
    "*title: rtmon",
    "*main_graph.glWidget*width: 300", "*main_graph.glWidget*height: 300", NULL
};

XtAppContext app;

void resize_main_graph(Widget w, XtPointer data, XtPointer callData)
{
    Dimension       width, height;

    glXMakeCurrent(main_graph.display, 
		   XtWindow(main_graph.glWidget), 
		   main_graph.context);
    
    XtVaGetValues(main_graph.glWidget, XmNwidth, &width, XmNheight, &height, NULL);
    main_graph.width = width;
    main_graph.height = height;
    glViewport(0, 0, (GLint) width, (GLint) height);
    
}


void
makeRasterFont(Display *loc_display)
{
    XFontStruct *fontInfo;
    Font id;
    unsigned int first, last;
    Display *xdisplay;


/*    fontInfo = XLoadQueryFont(main_graph.display, 
	"-adobe-helvetica-medium-r-normal--17-120-100-100-p-88-iso8859-1");*/
/*    fontInfo = XLoadQueryFont(main_graph.display, 
	"-adobe-times-medium-r-normal--17-120-100-100-p-88-iso8859-1");*/
/*    fontInfo = XLoadQueryFont(main_graph.display, 
        "-adobe-times-medium-r-normal--11-80-100-100-p-54-iso8859-1");*/
    fontInfo = XLoadQueryFont(loc_display, 
        "-adobe-times-medium-r-normal--14-100-100-100-p-74-iso8859-1");
/*    fontInfo = XLoadQueryFont(main_graph.display, 
    "-adobe-new century schoolbo-medium-r-normal--14-100-100-100-p-82-iso8859-1");*/

    if (fontInfo == NULL) {
        grtmon_message("font not found", FATAL_MSG, 20);
	exit_frameview();
    }

    id = fontInfo->fid;
    first = fontInfo->min_char_or_byte2;
    last = fontInfo->max_char_or_byte2;

    times14_base = glGenLists((GLuint) last+1);
    if (times14_base == 0) {
        grtmon_message("out of display lists", FATAL_MSG, 21);
	exit_frameview();
    }
    glXUseXFont(id, first, last-first+1, times14_base+first);

}

void
printString(char *s)
{
    glPushAttrib (GL_LIST_BIT);
    glListBase(times14_base);
    glCallLists(strlen(s), GL_UNSIGNED_BYTE, (GLubyte *)s);
    glPopAttrib ();
}

void
setup_font(Display *loc_display)
{
    makeRasterFont (loc_display);
    glShadeModel (GL_FLAT);    

}

void
mainGraph(int argc, char* argv[])
{
    int n, i, cpu;
    Widget overheadForm, overheadDialog;
    Atom wm_delete_window;
    Widget menubar, mainw, glFrame;

    Colormap     cmap;
    static int attributeList[] = { GLX_RGBA, None };

    Arg args[20];

    argc = 0;

    toplevel = XtAppInitialize(&app, "rtmon", NULL, 0, &argc, argv,
			       frameviewResources, NULL, 0);

    main_graph.display = XtDisplay(toplevel);

    /* create an OpenGL rendering context */
    main_graph.context = glXCreateContext(main_graph.display, master_vi, None, GL_TRUE);
    if (main_graph.context == NULL)
	grtmon_message("could not create rendering context", ERROR_MSG, 23);

    while (! major_frame_account_valid) {
      sleep(1);
    }
    
    init_graphic_vars();

    overheadDialog = XtVaCreateWidget ("overhead dialog",
	xmDialogShellWidgetClass, toplevel,
	XmNtitle, "Real-Time Monitor (Major Frame View)",
        XmNdeleteResponse, XmUNMAP,
	NULL);
	

    wm_delete_window = XmInternAtom(XtDisplay(overheadDialog),
				    "WM_DELETE_WINDOW",
				    FALSE);
    
    XmAddWMProtocolCallback (overheadDialog, wm_delete_window, 
			     exit_frameview, NULL);


    mainw = XtVaCreateWidget ("mainw",
        xmFormWidgetClass, overheadDialog,
	XmNwidth, 1000,
        XmNheight, (gr_numb_cpus <= 10) ? (48+(2*gr_numb_cpus+1)*50) : 1000,
        NULL);


    XtManageChild(mainw);

    menubar = makeMenus (mainw);

    overheadForm = XtVaCreateWidget ("overheadForm",
	xmFormWidgetClass, mainw,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNdeleteResponse, XmUNMAP,
        XmNwidth, 1000,
        XmNheight, SCROLL_BAR_WIDTH,
 	NULL);



    XtManageChild (overheadForm);
/*
    n = 0;
    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
    XtSetArg (args[n], XmNminimum, 1); n++;
    XtSetArg (args[n], XmNmaximum, A_THUMB); n++;
    XtSetArg (args[n], SgNhomePosition, A_THUMB); n++;
    XtSetArg (args[n], SgNunitsPerRotation, 1); n++;
    thumbWheel_a = SgCreateThumbWheel (overheadForm, "thumb", args, n);
    XtManageChild (thumbWheel_a);
    XtAddCallback (thumbWheel_a, XmNvalueChangedCallback, thumbCB, (XtPointer)&ortho_emp);
    XtAddCallback (thumbWheel_a, XmNdragCallback, thumbCB, (XtPointer)&ortho_emp);

    XtVaSetValues(thumbWheel_a, SgNhomePosition, A_THUMB - 1, NULL);
*/

    n = 0;
    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    /*    XtSetArg (args[n], XmNrightWidget, thumbWheel_a); n++;
    XtSetArg (args[n], XmNrightWidget, thumbWheel_a); n++;*/
    XtSetArg (args[n], XmNshowValue, False); n++;
    XtSetArg (args[n], XmNsliderSize, MAX_SLIDER_A); n++;
    XtSetArg (args[n], XmNshowArrows, True); n++;
    XtSetArg (args[n], XmNminimum, 0); n++;
    XtSetArg (args[n], XmNmaximum, MAX_SLIDER_A); n++;
    
    scroll_a = XmCreateScrollBar(overheadForm, "scroll", args, n);
    XtManageChild (scroll_a);

    XtAddCallback (scroll_a, XmNdragCallback, scroll_a_CB, NULL);
    XtAddCallback (scroll_a, XmNvalueChangedCallback, scroll_a_CB, NULL);
    XtAddCallback (scroll_a, XmNincrementCallback, inc_scroll_a_CB, NULL);
    XtAddCallback (scroll_a, XmNdecrementCallback, dec_scroll_a_CB, NULL);


    XtManageChild (overheadForm);


    glFrame = XtVaCreateManagedWidget("glFrame", 
        xmFrameWidgetClass, mainw,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNtopWidget, menubar,
        XmNbottomWidget, overheadForm,
        NULL);

    XtManageChild(glFrame);


    main_graph.glWidget = XtVaCreateManagedWidget("main_graph.glWidget", 
        glwMDrawingAreaWidgetClass, glFrame,
	GLwNvisualInfo, master_vi, 
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNtopAttachment, XmATTACH_FORM,
        XmNwidth, 1000,
        XmNheight, (gr_numb_cpus <= 10) ? ((2*gr_numb_cpus+1)*50) : 1000,
        NULL);


    XtAddCallback(main_graph.glWidget, XmNresizeCallback, resize_main_graph, NULL);
    XtAddCallback(main_graph.glWidget, XmNinputCallback, get_mouse_pos_A, NULL);

    main_graph.window = XtWindow(main_graph.glWidget);


/*    XtRealizeWidget(toplevel);*/
    XtRealizeWidget(overheadDialog);
    /*
     * Once widget is realized (ie, associated with a created X window), we
     * can bind the OpenGL rendering context to the window.
     */
    glXMakeCurrent(main_graph.display, 
		   XtWindow(main_graph.glWidget), 
		   main_graph.context);

    setup_font(main_graph.display);


    glClearDepth(1.0);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glMatrixMode(GL_MODELVIEW);


    work_procid = XtAppAddWorkProc (app, drawGraphs, NULL);

    for (i = gr_numb_cpus - 1; i >= 0; i--) {
	if (cpuOn[i]) {
	    cpu = gr_cpu_map[i];
	
	    set_reasonable_start(cpu);
	}
    }


    XtAppMainLoop(app);
}


/*
 * This routine will install a particular gl widgets's colormap onto the
 * top level window.  It may not be called until after the windows have
 * been realized.
 */
void 
installColormap (Widget toplevel, Widget glw)
{
   Window windows[2];

   windows[0] = XtWindow(glw);
   windows[1] = XtWindow(toplevel);
   XSetWMColormapWindows(XtDisplay(toplevel), XtWindow(toplevel), windows, 2);
} 




Boolean 
drawGraphs (XtPointer callD)
{
    int i,j;
    int frame_time, max_frame_time;

    if (! rtmon_program_running)
		exit_frameview();

    if (defining_template != 1) {
	if (defining_template_cpu >=0) {
	    fix_template[defining_template_cpu] = defining_template;
	    defining_template = 0;
	    defining_template_cpu = -1;
	}

	for (i=0; i<MAX_PROCESSORS; i++) {
	    if (fix_template[i] == 1) {
		defining_template = 1;
		defining_template_cpu = i;
		define_template(i, numb_maj_frames[i]-1);
	    }
	}
    }

    for (i=0; i<MAX_PROCESSORS; i++) {
	if (graph_info[i].special_frame) {
	    max_frame_time = 0;
	    /* yes I really want i for this for loop also */
	    for (i=0; i<MAX_PROCESSORS; i++) {
		frame_time = 0;
		for (j=0; j<major_frame_account[i].template_minors; j++) {
		    if (graph_info[i].special_minor[j] != SPECIAL_MINOR_LOST) {
			frame_time += major_frame_account[i].minor_time +
			    graph_info[i].special_minor_time[j];
		    }
		}
		if (frame_time > max_frame_time)
		    max_frame_time = frame_time;
	    }
	    max_val_a = max_frame_time;
	}
    }


    if (no_frame_mode)
	draw_main_no_frame_graph();
    else
	draw_main_graph();
    
    if (kern_graph_on) {
	draw_kern_graph();
    }    
    /* this major processes percentage graph */
    if (proc_graph_on) {
	draw_proc_graph();
    }

    if (kern_funct_graph_on) {
      draw_kern_funct_graph();
    }
    draw_raw_graph();

    sginap (10);
    return((Boolean)0);
}

/* orient overloaded, if it's greater than of equal to 100 that means
   we're calling from draw_nf_events and we can skip all the checks
   and just draw the arrow */
void
draw_arrow (int ortho, int max_val, float my_scale, float pos, 
	    int type, float color_vector[3], int orient)
{
    float percent;
    float start_val, end_val;
    float act_pos;

    if (orient < 100) {
	percent = (float)ortho/(float)max_val;
	
	start_val = (((1-percent)*max_val) * ((float)my_scale/100));
	end_val = start_val + (float)ortho;
	
	act_pos = pos - start_val;
	
	if ((act_pos < 0) || (act_pos > end_val))
	    return;
    }
    else {
	act_pos = pos;
	orient = orient - 100;
    }

    draw_line(act_pos, 0.5*HEIGHT+INTR_WIDTH*HEIGHT, act_pos, 
	       0.5*HEIGHT-INTR_WIDTH*HEIGHT, 1, color_vector);
    
    if (type == EX_EV_INTR_ENTRY)
	draw_triangle(act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT + INTR_TRI_HEIGHT,
		      act_pos + INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT+INTR_TRI_HEIGHT*0.5,
		      color_vector);
    else if (type == EX_EV_INTR_EXIT)
	draw_triangle(act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT + INTR_TRI_HEIGHT,
		      act_pos - INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT+INTR_TRI_HEIGHT*0.5,
		      color_vector);
    else if (type == EX_EV_USER)
	draw_triangle(act_pos - 0.5*INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT + INTR_TRI_HEIGHT,
		      act_pos + 0.5*INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      color_vector);
    else if ((type == EX_EV_KERN) && (orient == 0)) 
	draw_triangle(act_pos,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT - INTR_TRI_HEIGHT,
		      act_pos + INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT - INTR_TRI_HEIGHT*0.5,
		      color_vector);
    else if ((type == EX_EV_KERN) && (orient == 1)) 
	draw_triangle(act_pos,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT - INTR_TRI_HEIGHT,
		      act_pos - INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT - INTR_TRI_HEIGHT*0.5,
		      color_vector);
    else /* we shouldn't be here */
	;
	    
/*	draw_triangle(act_pos - INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT + INTR_TRI_HEIGHT,
		      act_pos + INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      color_vector);*/
}

/* orient overloaded, if it's greater than of equal to 100 that means
   we're calling from draw_nf_events and we can skip all the checks
   and just draw the arrow */
void
draw_arrow_new (int ortho, int max_val, float act_pos, 
	    int type, float color_vector[3], int orient)
{

    if (orient < 100) {
	if ((act_pos < start_val_a) || (act_pos > start_val_a + width_a))
	    return;
    }
    else {
	orient = orient - 100;
    }

    draw_line(act_pos, 0.5*HEIGHT+INTR_WIDTH*HEIGHT, act_pos, 
	       0.5*HEIGHT-INTR_WIDTH*HEIGHT, 1, color_vector);
    
    if (type == EX_EV_INTR_ENTRY)
	draw_triangle(act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT + INTR_TRI_HEIGHT,
		      act_pos + INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT+INTR_TRI_HEIGHT*0.5,
		      color_vector);
    else if (type == EX_EV_INTR_EXIT)
	draw_triangle(act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT + INTR_TRI_HEIGHT,
		      act_pos - INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT+INTR_TRI_HEIGHT*0.5,
		      color_vector);
    else if (type == EX_EV_USER)
	draw_triangle(act_pos - 0.5*INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT + INTR_TRI_HEIGHT,
		      act_pos + 0.5*INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      color_vector);
    else if ((type == EX_EV_KERN) && (orient == 0)) 
	draw_triangle(act_pos,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT - INTR_TRI_HEIGHT,
		      act_pos + INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT - INTR_TRI_HEIGHT*0.5,
		      color_vector);
    else if ((type == EX_EV_KERN) && (orient == 1)) 
	draw_triangle(act_pos,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT - INTR_TRI_HEIGHT,
		      act_pos - INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT-INTR_WIDTH*HEIGHT - INTR_TRI_HEIGHT*0.5,
		      color_vector);
    else /* we shouldn't be here */
	;
	    
/*	draw_triangle(act_pos - INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      act_pos,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT + INTR_TRI_HEIGHT,
		      act_pos + INTR_TRI_WIDTH*(float)ortho,
		      0.5*HEIGHT+INTR_WIDTH*HEIGHT,
		      color_vector);*/
}


/* here start and end are in terms of microseconds and we need
   to convert to graphics points and figure out where to place
   on the figure, also draws the label of the pid if label is 
   true */
void
draw_frame_rect (int ortho, int max_val, float my_scale, float start, float end, 
		 int height_flag, float color_vector[3], boolean label, 
		 char *label_str, boolean time_lab, int utime, 
		 int ktime, float kcolor_vector[3])
{
    float percent;
    char t_str[32];
    float start_val, end_val;
    float act_start, act_end;

    percent = (float)ortho/(float)max_val;

    /* a little hack here so we still need only one function */
    if ((ortho == ortho_b) && (kern_disp_mode == FOCUS)) {
	percent = (float)ortho_b/(float)(kern_focus_max - kern_focus_min);
	start_val = kern_disp_val + 
	    (((1-percent)*(float)(kern_focus_max - kern_focus_min)) * ((float)range_b/100));
    }
    else
	start_val = (((1-percent)*max_val) * ((float)my_scale/100));

    end_val = start_val + ortho;

    act_start = start - start_val;
    if (act_start >end_val) return; /* this recetangle not in view */
    if (act_start < 0) act_start = 0;  /* only seeing some of rectangle */
    
    act_end = end - start_val;
    if (act_end < 0)  return; /* this recetangle not in view */
    if (act_end > end_val) act_end = end_val; /* only seeing some of rectangle */

    /* overloaded time_lab, a 2 means we're just going to draw idle time */
    if (time_lab == 2) {
	glColor3fv(color_vector);
	if (graph_mode == SUMMARIZED) {
	    if (GraphA_label_type == MSECS)
		sprintf(t_str, "%d", utime/1000);
	    else
		sprintf(t_str, "%d", utime);
	    glRasterPos2i(act_start,  0.25*HEIGHT);
	    printString (t_str);
	}
	draw_rect(act_start, 0.5*HEIGHT+RECT_WIDTH*HEIGHT, act_end, 
		  0.5*HEIGHT, color_vector);
	glRasterPos2i(act_start,  HEIGHT*1.0);

	printString (label_str);
	return;
    }

    
    if (label) {
	glColor3fv(color_vector);

	glRasterPos2i(act_start,  HEIGHT*1.0);

	printString (label_str);

    }
    if (time_lab) {
	glColor3fv(color_vector);
	if (GraphA_label_type == MSECS)
	    sprintf(t_str, "%d   ", utime/1000);
	else
	    sprintf(t_str, "%d   ", utime);
	glRasterPos2i(act_start,  -0.16*HEIGHT);
	printString (t_str);
	glColor3fv(kcolor_vector);
	if (GraphA_label_type == MSECS)
	    sprintf(t_str, "%d   ", ktime/1000);
	else
	    sprintf(t_str, "%d   ", ktime);
	printString (t_str);
    }
    draw_rect(act_start, 0.5*HEIGHT+RECT_WIDTH*HEIGHT, act_end, 
	      0.5*HEIGHT-RECT_WIDTH*HEIGHT, color_vector);
}
/* here start and end are in terms of microseconds and we need
   to convert to graphics points and figure out where to place
   on the figure, also draws the label of the pid if label is 
   true */
void
draw_frame_rect_new (int ortho, int max_val, float start, float end, 
		 int height_flag, float color_vector[3], boolean label, 
		 char *label_str, boolean time_lab, int utime, 
		 int ktime, float kcolor_vector[3])
{
    float percent;
    char t_str[32];
    float start_val, end_val;
    float act_start, act_end;



    /* a little hack here so we still need only one function */
    if ((ortho == ortho_b) && (kern_disp_mode == FOCUS)) {
	percent = (float)ortho_b/(float)(kern_focus_max - kern_focus_min);
	start_val = kern_disp_val + 
	    (((1-percent)*(float)(kern_focus_max - kern_focus_min)) * ((float)range_b/100));
    }
    else
	start_val = (float)start_val_a;

    end_val = start_val + ortho;

    act_start = start - start_val;
    if (act_start >end_val) return; /* this recetangle not in view */
    if (act_start < 0) act_start = 0;  /* only seeing some of rectangle */
    
    act_end = end - start_val;
    if (act_end < 0)  return; /* this recetangle not in view */
    if (act_end > end_val) act_end = end_val; /* only seeing some of rectangle */

    /* overloaded time_lab, a 2 means we're just going to draw idle time */
    if (time_lab == 2) {
	glColor3fv(color_vector);
	if (graph_mode == SUMMARIZED) {
	    if (GraphA_label_type == MSECS)
		sprintf(t_str, "%d", utime/1000);
	    else
		sprintf(t_str, "%d", utime);
	    glRasterPos2i(act_start,  0.25*HEIGHT);
	    printString (t_str);
	}
	draw_rect(act_start, 0.5*HEIGHT+RECT_WIDTH*HEIGHT, act_end, 
		  0.5*HEIGHT, color_vector);
	glRasterPos2i(act_start,  HEIGHT*1.0);

	printString (label_str);
	return;
    }

    
    if (label) {
	glColor3fv(color_vector);

	glRasterPos2i(act_start,  HEIGHT*1.0);

	printString (label_str);

    }
    if (time_lab) {
	glColor3fv(color_vector);
	if (GraphA_label_type == MSECS)
	    sprintf(t_str, "%d   ", utime/1000);
	else
	    sprintf(t_str, "%d   ", utime);
	glRasterPos2i(act_start,  -0.16*HEIGHT);
	printString (t_str);
	glColor3fv(kcolor_vector);
	if (GraphA_label_type == MSECS)
	    sprintf(t_str, "%d   ", ktime/1000);
	else
	    sprintf(t_str, "%d   ", ktime);
	printString (t_str);
    }
    draw_rect(act_start, 0.5*HEIGHT+RECT_WIDTH*HEIGHT, act_end, 
	      0.5*HEIGHT-RECT_WIDTH*HEIGHT, color_vector);
}


/************************************************/
/* This function assumes that the y positions   */
/* will always be the same.			*/
/************************************************/
void
draw_rect (float left, float top, float right, 
	   float bot, float color_vector[3])
{
    float ll[2], ul[2], ur[2], lr[2];

    ll[0] = left;
    ll[1] = bot;

    ul[0] = left;
    ul[1] = top;

    ur[0] = right;
    ur[1] = top;

    lr[0] = right;
    lr[1] = bot;

    glColor3fv(color_vector);

    glBegin(GL_POLYGON);
	glVertex2fv(ll);
	glVertex2fv(ul);
	glVertex2fv(ur);
	glVertex2fv(lr);
    glEnd();
}



void
draw_triangle (float p1x, float p1y, float p2x, float p2y,
	       float p3x, float p3y, float color_vector[3])
{
    float p1[2], p2[2], p3[2];

    p1[0] = p1x;
    p1[1] = p1y;

    p2[0] = p2x;
    p2[1] = p2y;

    p3[0] = p3x;
    p3[1] = p3y;

    glColor3fv(color_vector);

    glBegin(GL_POLYGON);
	glVertex2fv(p1);
	glVertex2fv(p2);
	glVertex2fv(p3);
    glEnd();
}




void
draw_line(float x1,float y1,float x2,float y2,int width, float color_vector[3]) 
{
    float start[2], end[2];

    glColor3fv(color_vector);
    glLineWidth((GLfloat)(width));

    start[0] = x1;
    start[1] = y1;

    end[0] = x2;
    end[1] = y2;

    glBegin(GL_LINE_STRIP);
	glVertex2fv(start);
	glVertex2fv(end);
    glEnd();
}

void
init_graphic_vars()
{
    int i;

    /* see TIMESTAMP_ADJ_COMMENT in frameview.h
       adjust_mode = OFF;
       */
    graph_mode = SUMMARIZED;
    frame_sync = BY_TIME;
    work_procid = 0;
    kern_graph_on = 0;
    kern_funct_graph_on = 0;
    proc_graph_on = 0;
    last_text_display_on = FALSE;
    display_frame_type = ACTUAL;
    kern_disp_mode = WIDE;
    GraphA_label_type = 0;
    GraphA_divisions = 3;
    GraphA_tics = 4;
    start_val_a = 0;
    range_b = 0;
    gr_numb_cpus = 0;
    gr_max_processes = 0;
    max_val_a = 0;
    gr_last_show.cpu = -1;
    /* start off by figuring out which processors are on */
    for (i=0;i<MAX_PROCESSORS;i++) {
	kern_disp_minor[i] = 0;
	if (cpu_on[i]) {
	    if (major_frame_account[i].numb_processes > gr_max_processes)
		gr_max_processes = major_frame_account[i].numb_processes;
	    gr_cpu_map[gr_numb_cpus] = i;
	    gr_numb_cpus++;
	    if ((major_frame_account[i].numb_minors * 
		major_frame_account[i].minor_time) > max_val_a)
		max_val_a = major_frame_account[i].numb_minors *
		             major_frame_account[i].minor_time;
	}
    }
    nf_refresh_rate = 1000000;
    init_proc_colors(12); /* don't allocate for idle loop */
    width_a = max_val_a;
    width_ay = 2*HEIGHT*gr_numb_cpus + HEIGHT;
    width_a_min = 10;
    ortho_b = max_val_a;
    ortho_by = 2*HEIGHT*gr_numb_cpus + 0.5*HEIGHT;
    ortho_c = MAJ_PROC_WIDTH*gr_numb_cpus;
    ortho_emp = max_val_a;
    zoom_line_l = -1;
    zoom_line_r = -1;
    scan_line_m = -1;
    scan_line_l = -1;
    scan_line_r = -1;
    current_search_mode = SEARCH_ALL;
    current_search_index = 0;
    current_search_major = 0;
    current_search_found = FALSE;
    current_search_on = OFF;

    if (no_frame_mode) {
	width_a_min = 10;
    }

    TstampTextDisplay = NULL; /* indicate Tstamp Text Display not yet open */    
    searchWindow = NULL; /* indicate search window not yet open */    
}

void
wait_for_define_template(int cpu)
{
    XEvent event_return;

    defining_template = 1;
    define_template(cpu, maj_frame_numb[cpu]);
    while(defining_template == 1) {
	XtAppNextEvent(app, &event_return);
	XtDispatchEvent(&event_return);
	
    }
}




