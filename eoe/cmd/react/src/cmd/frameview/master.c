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
 * File:	master.c					          *
 *									  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

/* code in this file taken from the paperplane.c demo sample code */

#define GRMAIN extern
#include "graph.h"

#define FRAME_START_GL_HEIGHT 150
#define FRAME_START_WINDOW_WIDTH 700
#define FRAME_START_WINDOW_HEIGHT 180

#define FRAME_START_MAJ_START 25
#define FRAME_START_MAJ_WIDTH 195
#define FRAME_START_MIN_START 250
#define FRAME_START_MIN_WIDTH 195
#define FRAME_START_TRIGGER_START 475
#define FRAME_START_TRIGGER_WIDTH 170
#define FRAME_START_BAR_HEIGHT 10

#define FRAME_START_MAJ_MARGIN 30
#define FRAME_START_MIN_MARGIN 40
#define FRAME_START_EV_MARGIN 50
#define FRAME_START_MAJ1 75
#define FRAME_START_MAJ2 625
#define FRAME_START_GAP 3
#define FRAME_START_MIN1 (((FRAME_START_MAJ2-FRAME_START_MAJ1)/3)+FRAME_START_MAJ1)
#define FRAME_START_MIN2 ((2*(FRAME_START_MAJ2-FRAME_START_MAJ1)/3)+FRAME_START_MAJ1)
#define FRAME_START_MAJ_TOP (FRAME_START_GL_HEIGHT - FRAME_START_MAJ_MARGIN)
#define FRAME_START_MAJ_BOT (FRAME_START_BAR_HEIGHT + FRAME_START_MAJ_MARGIN)
#define FRAME_START_MIN_TOP (FRAME_START_GL_HEIGHT - FRAME_START_MIN_MARGIN)
#define FRAME_START_MIN_BOT (FRAME_START_BAR_HEIGHT + FRAME_START_MIN_MARGIN)
#define FRAME_START_EV_TOP (FRAME_START_GL_HEIGHT - FRAME_START_EV_MARGIN)
#define FRAME_START_EV_BOT (FRAME_START_BAR_HEIGHT + FRAME_START_EV_MARGIN)
#define FRAME_START_MID ((FRAME_START_MAJ_BOT+FRAME_START_MAJ_TOP)/2)

#define FRAME_START_MAJ_COLOR bluevect
#define FRAME_START_MIN_COLOR magentavect
#define FRAME_START_TRIGGER_COLOR greenvect
#define FRAME_START_OTHER_COLOR greyvect

int shmid;

int no_frame_menu_val;
int kern_funct_menu_val;
Widget no_frame_val[2];
Widget kern_funct_val[2];
Widget NoFrameMenu;
Widget KernFunctMenu;

Widget FrameStartWindow;
glx_wind_t frame_start;


#ifdef noGLwidget
#include <Xm/DrawingA.h>	/* Motif drawing area widget */
#else
/** NOTE: in IRIX 5.2, the OpenGL widget headers are mistakenly in   **/
/** <GL/GLwDrawA.h> and <GL/GlwMDraw.h> respectively.  Below are the **/
/** _official_ standard locations.                                   **/
#ifndef __sgi
#ifdef noMotifGLwidget
#include <X11/GLw/GLwDrawA.h> /* STANDARD: pure Xt OpenGL drawing area widget */
#else
#include <X11/GLw/GLwMDrawA.h> /* STANDARD: Motif OpenGL drawing area widget */
#endif
#else
#ifdef noMotifGLwidget
#include <GL/GLwDrawA.h> /* IRIX 5.2: pure Xt OpenGL drawing area widget */
#else
#include <GL/GLwMDrawA.h> /* IRIX 5.2: Motif OpenGL drawing area widget */
#endif
#endif
#endif
#include <X11/keysym.h>


static int master_dblBuf[] = {
    GLX_DOUBLEBUFFER, GLX_RGBA, GLX_DEPTH_SIZE, 16,
    GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1,
    None
};
static int *master_snglBuf = &master_dblBuf[1];
static String   fallbackResources[] = {
    "*sgiMode: true",		/* try to enable IRIX 5.2+ look & feel */
    "*useSchemes: all",		/* and SGI schemes */
    "*title: FrameView",
    "*glxarea*width: 250", "*glxarea*height: 200", NULL
};
Display     *dpy;
GLboolean    doubleBuffer = GL_TRUE, moving = GL_FALSE, made_current = GL_FALSE;
XtAppContext master_app;
XtWorkProcId workId = 0;
Widget       mainw, menubar, menupane, cascade, frame, glxarea;
GLXContext   cx;
Arg          menuPaneArgs[1], args[1];



#define v3f glVertex3f /* v3f was the short IRIS GL name for glVertex3f */

Boolean logo_main();
Boolean text_main();

void logo_initialize();
void resize_window();
void tick();



void resize(Widget w, XtPointer data, XtPointer callData)
{
    Dimension       width, height;

    if(made_current) {
	XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, NULL);
	glViewport(0, 0, (GLint) width, (GLint) height);
    }
}


void l_draw(Widget w)
{
    GLfloat         red, green, blue;
    int             i;

    glXMakeCurrent(dpy, XtWindow(glxarea), cx);


    glClear(GL_DEPTH_BUFFER_BIT);
    /* paint black to blue smooth shaded polygon for background */
    glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glBegin(GL_POLYGON);
        glColor3f(0.0, 0.0, 0.0);
        v3f(-20, 20, -19); v3f(20, 20, -19);
        glColor3f(0.0, 0.0, 1.0);
        v3f(20, -20, -19); v3f(-20, -20, -19);
    glEnd();

    if (doubleBuffer) glXSwapBuffers(dpy, XtWindow(w));
    if(!glXIsDirect(dpy, cx))
        glFinish(); /* avoid indirect rendering latency from queuing */
}

Boolean animate(XtPointer data)
{

    return False;		/* leave work proc active */
}



void
no_frame_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtVaSetValues (no_frame_val[no_frame_menu_val],
		   XmNset, False, NULL);
    no_frame_mode = (int)clientD;
    no_frame_menu_val = (int)clientD;
    XtVaSetValues (no_frame_val[no_frame_menu_val],
		   XmNset, True, NULL);
}


void
no_frame_menu_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XmString tempString;

    if (no_frame_menu_val) {
	tempString = XmStringCreateSimple ("No Frame Mode (off)");
	XtVaSetValues(NoFrameMenu, XmNlabelString, tempString, NULL);
	XmStringFree (tempString);
	no_frame_menu_val = False;
	no_frame_mode = False;
    }
    else {
	tempString = XmStringCreateSimple ("No Frame Mode (on)");
	XtVaSetValues(NoFrameMenu, XmNlabelString, tempString, NULL);
	XmStringFree (tempString);
	no_frame_menu_val = True;
	no_frame_mode = True;
    }
    XtManageChild(NoFrameMenu);
}


void
kern_funct_menu_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XmString tempString;

    if (kern_funct_menu_val) {
	tempString = XmStringCreateSimple ("Kernel Function Mode (off)");
	XtVaSetValues(KernFunctMenu, XmNlabelString, tempString, NULL);
	XmStringFree (tempString);
	kern_funct_menu_val = False;
	kern_funct_mode = False;
    }
    else {
	tempString = XmStringCreateSimple ("Kernel Fuction Mode (on)");
	XtVaSetValues(KernFunctMenu, XmNlabelString, tempString, NULL);
	XmStringFree (tempString);
	kern_funct_menu_val = True;
	kern_funct_mode = True;
    }
    XtManageChild(KernFunctMenu);
}


void
kern_funct_CB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtVaSetValues (kern_funct_val[kern_funct_menu_val],
		   XmNset, False, NULL);
    kern_funct_mode = (int)clientD;
    kern_funct_menu_val = (int)clientD;
    XtVaSetValues (kern_funct_val[kern_funct_menu_val],
		   XmNset, True, NULL);
}


void master_quit_CB(Widget w, XtPointer data, XtPointer callData)
{
    struct pid_elem *search_elem;
    char sys_str[128];

    search_elem = master_pid_list;
    while (search_elem != NULL) {
	kill (search_elem->pid, SIGINT);
	search_elem = search_elem->next;
    }
    sprintf(sys_str, "rm -f %s.fv* %s.fr*", tmp_dir_str, tmp_dir_str);
    system(sys_str);
    shmdt(0);
    shmctl(shmid,IPC_RMID); 
    sleep(1);
    exit(0);
}

void input(Widget w, XtPointer data, XtPointer callData)
{
    XmDrawingAreaCallbackStruct *cd = (XmDrawingAreaCallbackStruct *) callData;
    char            buf[1];
    KeySym          keysym;
    int             rc;

    if(cd->event->type == KeyPress)
	if(XLookupString((XKeyEvent *) cd->event, buf, 1, &keysym, NULL) == 1)
	    switch (keysym) {
	    case XK_space:
	        if (!moving) { /* advance one frame if not in motion */

	        }
	        break;
	    case XK_Escape:
	        exit(0);
	    }
}

void map_state_changed(Widget w, XtPointer data, XEvent * event, Boolean * cont)
{
    switch (event->type) {
    case MapNotify:
	if (moving && workId != 0) workId = XtAppAddWorkProc(master_app, animate, NULL);
	break;
    case UnmapNotify:
	if (moving) XtRemoveWorkProc(workId);
	break;
    }
}

void
get_frame_defaults(FILE *fp)
{
    char def_str[128];
    char garbstr[255];
    char msg_lvl_str[32];
    int debug_enabled;
    char msg_str[64];

    debug_enabled = 0;

#ifdef DEBUG
    debug_enabled = 1;
#endif

    strcpy(default_filename, "fv-data");

    while ((fscanf(fp, "%s", def_str)) != EOF) {
	if ((strcmp(def_str, "/*")) == 0) {
	    fgets(garbstr, 255, fp);
	}
	else if ((strcmp(def_str, "no_frame_mode")) == 0) {
	    fscanf(fp, "%s", def_str);
	    if ((strcmp(def_str, "off")) == 0) default_no_frame_mode = 0;
	    if ((strcmp(def_str, "on")) == 0) default_no_frame_mode = 1;
	}
	else if ((strcmp(def_str, "kernel_function_mode")) == 0) {
	    fscanf(fp, "%s", def_str);
	    if ((strcmp(def_str, "off")) == 0) default_kern_funct_mode = 0;
	    if ((strcmp(def_str, "on")) == 0) default_kern_funct_mode = 1;
	}
	else if ((strcmp(def_str, "filename")) == 0) {
	    fscanf(fp, "%s", default_filename);
	}
	else if ((strcmp(def_str, "message_level")) == 0) {
	    fscanf(fp, "%s", msg_lvl_str);
	    if ((strcasecmp(msg_lvl_str, "DEBUG")) == 0){
		if (debug_enabled)
		    message_level = DEBUG_MSG;
		else {
		    grtmon_message("debugging not enabled, bumping to NOTICE", ALERT_MSG, 47);
		    message_level = NOTICE_MSG;
		}
	    }
	    /* allow force of debugging with code */
	    else if ((strcasecmp(msg_lvl_str, "22785")) == 0)
		message_level = DEBUG_MSG;
	    else if ((strcasecmp(msg_lvl_str, "NOTICE")) == 0)
		message_level = NOTICE_MSG;
	    else if ((strcasecmp(msg_lvl_str, "ALERT")) == 0)
		message_level = ALERT_MSG;
	    else if ((strcasecmp(msg_lvl_str, "WARNING")) == 0)
		message_level = WARNING_MSG;
	    else if ((strcasecmp(msg_lvl_str, "ERROR")) == 0)
		message_level = ERROR_MSG;
	    else {
		grtmon_message("unkown message level, defaulting to ALERT", ALERT_MSG, 9);
	    }
	}
	else {
	    sprintf(msg_str, "unknown default directive: %s\n", def_str);
	    grtmon_message(msg_str, ALERT_MSG, 102);
	}
    }
}

void
init_msg_dialog_info()
{
    int i;
    FILE *fp;

    shmid = shmget(IPC_PRIVATE, sizeof(msg_data_t), PERM_ALL);
    if (shmid == -1)
    {
	perror("Could not allocate shared memory");
	exit(1);
    }
    msg_data = (msg_data_t *)shmat(shmid, 0, 0);
    if ((int)msg_data == -1)
    {
	perror("Could not map shared memory segment");
	exit(1);
    }
    msg_data->access_count = 0;
    
    init_choice_events();

    for (i=0; i<MAX_DIALOGS; i++) {
	msg_in_use[i] = 0;
    }
}

void
master_sig_int_handler()
{
    master_quit_CB(NULL, NULL, NULL);
}

int multiSamples = 16;		/* Number of multisamples to use (0 == off) */
GLboolean msRequested = GL_FALSE;  /* Number of multisamples specified in the
                                    *  command line
                                    */

#define SET_MS_ATTRIB() if (1) { \
  for (multiIndex = 0; attr[multiIndex] != GLX_SAMPLES_SGIS; multiIndex++) \
    ; \
  attr[++multiIndex] = multiSamples; \
} else 
GLboolean dbuffer = GL_TRUE;    /* Doublebuffer? */
GLboolean stereo = GL_FALSE;    /* Use stereo? */


static int RGBA_SB_attributes[] = {
  GLX_RGBA,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DEPTH_SIZE, 1,
  None,
};

static int RGBA_SB_ST_attributes[] = {
  GLX_RGBA,
  GLX_STEREO,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DEPTH_SIZE, 1,
  GLX_STENCIL_SIZE, 0,
  GLX_SAMPLES_SGIS, 0,
  None,
};

static int RGBA_DB_attributes[] = {
  GLX_RGBA,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DOUBLEBUFFER,
  GLX_DEPTH_SIZE, 24,
  GLX_STENCIL_SIZE, 0,
  GLX_SAMPLES_SGIS, 0,
  None,
};

static int RGBA_DB_ST_attributes[] = {
  GLX_RGBA,
  GLX_STEREO,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DOUBLEBUFFER,
  GLX_DEPTH_SIZE, 1,
  GLX_STENCIL_SIZE, 0,
  GLX_SAMPLES_SGIS, 0,
  None,
};

void
get_master_vi()
{
    int *attr;
    int multiIndex;
  char title[256];

    while (master_vi == NULL) {
	attr = dbuffer ?
	    (stereo ? RGBA_DB_ST_attributes : RGBA_DB_attributes) :
	    (stereo ? RGBA_SB_ST_attributes : RGBA_SB_attributes);
	if (multiSamples != 0)
	    SET_MS_ATTRIB();
	master_vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr);
	if (master_vi == NULL) {
	    if (multiSamples != 0) {  /* Try to reduce number of samples */
		int logMs;
		for (logMs = 4; logMs && master_vi == NULL; logMs--) {
		    attr[multiIndex] = logMs > 1 ? 1 << logMs : 0;
		    if (attr[multiIndex] == multiSamples)
			continue;
		    master_vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr);
		}
		if (master_vi != NULL) {
		    if (msRequested) {
			sprintf(title, "inform 'FrameView: Visuals of %d samples are not\
 supported.  Using %d samples'", multiSamples, attr[multiIndex]);
			system(title);
		    }
		    multiSamples = attr[multiIndex];
		} else {
		    /* For backwards compatibility: For servers that do not understand
		     * the multisample attribute, turn it off completely
		     */ 
		    multiSamples = 0;
		    attr[multiIndex - 1] = None;
		    master_vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr);
		    if (master_vi != NULL && msRequested) {
			system("inform 'Ideas: This X server does not support multisampling'");
			exit (1);
		    }
		}
      }
	    if (master_vi == NULL)
		if (stereo)
		    stereo = GL_FALSE;
		else {
		    /* Nothing left to do, bail */
		    sprintf(title, "inform 'FrameView: No matching visual on \"%s\"'", getenv("DISPLAY"));
		    system(title);
		    exit(1);
		}
	}
    }
    
}

void
process_command_line_args()
{

    if (strlen(cl_filename) > 0) {
	strcpy(default_filename, cl_filename);
    }
    if (strlen(cl_kern_mode) > 0) {
	if (strcmp(cl_kern_mode, "on") == 0) {
	    default_kern_funct_mode = 1;
	}
	else {
	    default_kern_funct_mode = 0;
	}
    }
    if (strlen(cl_message_level) > 0) {
	/* allow force of debugging with code */
	if ((strcasecmp(cl_message_level, "22785")) == 0)
	    message_level = DEBUG_MSG;
	else if ((strcasecmp(cl_message_level, "NOTICE")) == 0)
	    message_level = NOTICE_MSG;
	else if ((strcasecmp(cl_message_level, "ALERT")) == 0)
	    message_level = ALERT_MSG;
	else if ((strcasecmp(cl_message_level, "WARNING")) == 0)
	    message_level = WARNING_MSG;
	else if ((strcasecmp(cl_message_level, "ERROR")) == 0)
	    message_level = ERROR_MSG;
    }
    if (strlen(cl_no_frame_mode) > 0) {
	if (strcmp(cl_no_frame_mode, "on") == 0) {
	    default_no_frame_mode = 1;
	}
	else {
	    default_no_frame_mode = 0;
	}
    }

}

void
master_main(int argc, char *argv[])
{
    Widget OpenFile, StartLiveStream, FrameStartEvents, QuitButton;
    XmString tempString;
    Widget FileButton, ControlButton;
    Widget ControlMenu;
    Widget HelpMenu, HelpButton;
    Widget GeneralHelp;
    int i;
    char temp_str[64];
    FILE *fp;
    XSizeHints hints;

    static int attributeList[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None};


    signal(SIGINT, master_sig_int_handler);

    /* initialization for those things we're going to put in the
       control menu */
    collecting_nf_events = TRUE;
    default_no_frame_mode = FALSE;
    default_kern_funct_mode = FALSE;
    message_level = ALERT_MSG;

    
    /* check and see if any default files exist */
    if ((fp = fopen(".frameview_defaults", "r")) != NULL) {
	get_frame_defaults(fp);
    }
    else {
	if ((fp = fopen("~/.frameview_defaults", "r")) != NULL) {
	    get_frame_defaults(fp);
	}
    }

    process_command_line_args();

    init_msg_dialog_info();

    kern_funct_mode = default_kern_funct_mode;
    no_frame_mode = default_no_frame_mode;

    master_pid_list = NULL;
    master_toplevel = XtAppInitialize(&master_app, "Paperplane", NULL, 0, &argc, argv,
			       fallbackResources, NULL, 0);
    dpy = XtDisplay(master_toplevel);
    if (dpy == NULL) {
      fprintf(stderr, "Can't connect to display \"%s\"\n", getenv("DISPLAY"));
      exit(1);
    }

    /* find an OpenGL-capable RGB visual with depth buffer */
/*    get_master_vi();*/
    master_vi =  glXChooseVisual(dpy, DefaultScreen(dpy), attributeList);
    if (master_vi == NULL) {
      system("inform 'could not obtain visual'");
}


/*    master_vi = glXChooseVisual(dpy, DefaultScreen(dpy), master_dblBuf);
    if (master_vi == NULL) {
	master_vi = glXChooseVisual(dpy, DefaultScreen(dpy), master_snglBuf);
	if (master_vi == NULL) {
	    master_vi = glXChooseVisual(dpy, DefaultScreen(dpy), zBuf);
	    if (master_vi == NULL) {
		XtAppError(master_app, "no RGB visual with depth buffer");
	    }
	    doubleBuffer = GL_FALSE;
	}
    }
*/
    /* create an OpenGL rendering context */
    cx = glXCreateContext(dpy, master_vi, /* no display list sharing */ None,
        /* favor direct */ GL_TRUE);
    if (cx == NULL)
	XtAppError(master_app, "could not create rendering context");
    /* create an X colormap since probably not using default visual */
    XtAddEventHandler(master_toplevel, StructureNotifyMask, False,
                      map_state_changed, NULL);
    mainw = XmCreateMainWindow(master_toplevel, "mainw", NULL, 0);

    XtManageChild(mainw);

    /* create menu bar */
    menubar = XmCreateMenuBar(mainw, "menubar", NULL, 0);
    XtManageChild(menubar);

    menupane = XmCreatePulldownMenu(menubar, "menupane", NULL, 0);

    tempString = XmStringCreateSimple ("Ctrl-O");
    OpenFile = XtVaCreateManagedWidget ("Open",
	    xmPushButtonWidgetClass, menupane,
	    XmNmnemonic, 'O',
	    XmNaccelerator, "Ctrl<Key>O",
	    XmNacceleratorText, tempString,
	    NULL);
    XtAddCallback (OpenFile, XmNactivateCallback, open_file_CB, 0);
    XmStringFree (tempString);



    XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, menupane, NULL);


    tempString = XmStringCreateSimple ("Ctrl-L");
    StartLiveStream = XtVaCreateManagedWidget ("Start Live Stream",
	    xmPushButtonWidgetClass, menupane,
	    XmNmnemonic, 'L',
	    XmNaccelerator, "Ctrl<Key>L",
	    XmNacceleratorText, tempString,
	    NULL);
    XtAddCallback (StartLiveStream, XmNactivateCallback, 
		   start_live_stream_CB, NULL);
    XmStringFree (tempString);


    XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, menupane, NULL);


    tempString = XmStringCreateSimple ("Ctrl-Q");
    QuitButton = XtVaCreateManagedWidget ("Quit",
	    xmPushButtonWidgetClass, menupane,
	    XmNmnemonic, 'Q',
	    XmNaccelerator, "Ctrl<Key>Q",
	    XmNacceleratorText, tempString,
	    NULL);
    XtAddCallback (QuitButton, XmNactivateCallback, master_quit_CB, 0);
    XmStringFree (tempString);




    FileButton = XtVaCreateManagedWidget ("File",
	    xmCascadeButtonWidgetClass, menubar,
	    XmNsubMenuId, menupane,
	    XmNmnemonic, 'F',
	    NULL);


    ControlMenu = XmCreatePulldownMenu(menubar, "ControlMenu", NULL, 0);

    ControlButton = XtVaCreateManagedWidget ("Control",
	    xmCascadeButtonWidgetClass, menubar,
	    XmNsubMenuId, ControlMenu,
	    XmNmnemonic, 'C',
	    NULL);

    tempString = XmStringCreateSimple ("Ctrl-E");
    FrameStartEvents = XtVaCreateManagedWidget ("Frame Start Events",
	    xmPushButtonWidgetClass, ControlMenu,
	    XmNmnemonic, 'E',
	    XmNaccelerator, "Ctrl<Key>E",
	    XmNacceleratorText, tempString,
	    NULL);
    XtAddCallback (FrameStartEvents, XmNactivateCallback, 
		   frame_start_events_CB, NULL);
    XmStringFree (tempString);

    XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, ControlMenu, NULL);

    NoFrameMenu = XtVaCreateManagedWidget ("No Frame Mode (off)", 
        xmPushButtonWidgetClass, ControlMenu, NULL);  

    no_frame_menu_val = default_no_frame_mode;
    no_frame_mode = default_no_frame_mode;

    if (no_frame_menu_val) {
	tempString = XmStringCreateSimple ("No Frame Mode (on)");
	XtVaSetValues(NoFrameMenu, XmNlabelString, tempString, NULL);
	XmStringFree (tempString);
    }	

    XtAddCallback (NoFrameMenu, XmNactivateCallback, 
		   no_frame_menu_CB, NULL);
    XtManageChild(NoFrameMenu);
    


    XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, ControlMenu, NULL);

    KernFunctMenu = XtVaCreateManagedWidget ("Kernel Function Mode (off)", 
        xmPushButtonWidgetClass, ControlMenu, NULL);  

    kern_funct_menu_val = default_kern_funct_mode;
    kern_funct_mode = default_kern_funct_mode;

    if (kern_funct_menu_val) {
	tempString = XmStringCreateSimple ("Kernel Function Mode (on)");
	XtVaSetValues(KernFunctMenu, XmNlabelString, tempString, NULL);
	XmStringFree (tempString);
    }	

    XtAddCallback (KernFunctMenu, XmNactivateCallback, 
		   kern_funct_menu_CB, NULL);
    XtManageChild(KernFunctMenu);
 
/*  code below for setting up a submenu with check marks for on and off
    KernFunctButton = XtVaCreateManagedWidget ("Kernel Function Mode", 
        xmCascadeButtonWidgetClass, ControlMenu, NULL);  

    KernFunctMenu = XmCreatePulldownMenu (KernFunctButton, 
					  "Kernel Function Menu",
					  NULL, 0);
    XtVaSetValues (KernFunctButton, XmNsubMenuId, KernFunctMenu, NULL);

    kern_funct_menu_val = False;
    for (i=0;i<2;i++) {
	if (i==0)
	    sprintf(temp_str, "%s", "off");
	else
	    sprintf(temp_str, "%s", "on");

	kern_funct_val[i] = XtVaCreateManagedWidget (temp_str,
	    xmToggleButtonWidgetClass, KernFunctMenu, 
	    XmNset, (i==default_kern_funct_mode) ? True : False,
	    NULL);

	XtAddCallback (kern_funct_val[i], XmNvalueChangedCallback, 
		       kern_funct_CB, (XtPointer)i);
    }
*/

    HelpMenu = XmCreatePulldownMenu(menubar, "HelpMenu", NULL, 0);

    HelpButton = XtVaCreateManagedWidget ("Help",
	    xmCascadeButtonWidgetClass, menubar,
	    XmNsubMenuId, HelpMenu,
	    XmNmnemonic, 'H',
	    NULL);

    tempString = XmStringCreateSimple ("Ctrl-H");
    GeneralHelp = XtVaCreateManagedWidget ("General Help",
	    xmPushButtonWidgetClass, HelpMenu,
	    XmNmnemonic, 'H',
	    XmNaccelerator, "Ctrl<Key>H",
	    XmNacceleratorText, tempString,
	    NULL);
    XtAddCallback (GeneralHelp, XmNactivateCallback, 
		   help_general_CB, NULL);
    XmStringFree (tempString);


    /* create framed drawing area for OpenGL rendering */
    frame = XmCreateFrame(mainw, "frame", NULL, 0);
    XtManageChild(frame);
#ifdef noGLwidget
    glxarea = XtVaCreateManagedWidget("glxarea", xmDrawingAreaWidgetClass,
                                      frame, NULL);
#else
#ifdef noMotifGLwidget
    /* notice glwDrawingAreaWidgetClass lacks an 'M' */
    glxarea = XtVaCreateManagedWidget("glxarea", glwDrawingAreaWidgetClass,
#else
    glxarea = XtVaCreateManagedWidget("glxarea", glwMDrawingAreaWidgetClass,
#endif
				      frame, GLwNvisualInfo, master_vi, NULL);
#endif
    XtAddCallback(glxarea, XmNexposeCallback, (XtCallbackProc)l_draw, NULL);
    XtAddCallback(glxarea, XmNresizeCallback, resize, NULL);
    XtAddCallback(glxarea, XmNinputCallback, input, NULL);
    /* set up application's window layout */
    XmMainWindowSetAreas(mainw, menubar, NULL, NULL, NULL, frame);
    XtRealizeWidget(master_toplevel);
    /*
     * Once widget is realized (ie, associated with a created X window), we
     * can bind the OpenGL rendering context to the window.
     */
    glXMakeCurrent(dpy, XtWindow(glxarea), cx);				      
    made_current = GL_TRUE;
    /* setup OpenGL state */
    glClearDepth(1.0);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glMatrixMode(GL_PROJECTION);
    glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 20);
    glMatrixMode(GL_MODELVIEW);

    setup_font(dpy);
    srandom(getpid());

    XtAppAddWorkProc (master_app, logo_main, NULL);


				      
    /* start event processing */
    XtAppMainLoop(master_app);
}



void 
close_frame_start_CB (Widget w, XtPointer clientD, XtPointer callD)
{

    XtUnmanageChild(FrameStartWindow);

}

void
draw_frame_start()
{
    GLint mm;
    int i,pos,count,numb,kind;
    char numb_str[32];

    glXMakeCurrent(frame_start.display, 
		   XtWindow(frame_start.glWidget), 
		   frame_start.context);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3sv(bkgnd);
    
    numb = random()%10 + 5;
    count = 0;
/*    for (i=0; i<numb; i++) {
	pos = (random()%(FRAME_START_MAJ2 - FRAME_START_MAJ1)) + FRAME_START_MAJ1;
	kind = random()%2;
	if (count < 4) kind = 0;
	if (kind) {
	    draw_line(pos, FRAME_START_EV_BOT,
		      pos, FRAME_START_EV_TOP,
		      1, FRAME_START_OTHER_COLOR);
	}
	else {
	    draw_line(pos, FRAME_START_EV_BOT,
		      pos, FRAME_START_EV_TOP,
		      1, FRAME_START_TRIGGER_COLOR);
	}
	count++;
    }
*/
    draw_rect(FRAME_START_MAJ_START,FRAME_START_BAR_HEIGHT,
	      FRAME_START_MAJ_START+FRAME_START_MAJ_WIDTH,0, FRAME_START_MAJ_COLOR);
    draw_rect(FRAME_START_MIN_START,FRAME_START_BAR_HEIGHT,
	      FRAME_START_MIN_START+FRAME_START_MIN_WIDTH,0,FRAME_START_MIN_COLOR);
    draw_rect(FRAME_START_TRIGGER_START,FRAME_START_BAR_HEIGHT,
	      FRAME_START_TRIGGER_START+FRAME_START_TRIGGER_WIDTH,
	      0,FRAME_START_TRIGGER_COLOR);


    /* draw first major frame line and associated events */
    draw_line(FRAME_START_MAJ1, FRAME_START_MAJ_BOT,
	      FRAME_START_MAJ1, FRAME_START_MAJ_TOP,
	      1, redvect);

    if (choice_trigger_event < numb_trigger_events) {
	draw_line(FRAME_START_MAJ1, FRAME_START_EV_BOT,
		  FRAME_START_MAJ1, FRAME_START_EV_TOP,
		  1, FRAME_START_TRIGGER_COLOR);
	glRasterPos2i(FRAME_START_MAJ1, FRAME_START_MAJ_TOP);
	glColor3fv(FRAME_START_TRIGGER_COLOR);
	sprintf(numb_str, "%d", trigger_event[choice_trigger_event].numb);
        printString(numb_str);

	draw_line(FRAME_START_MAJ1+FRAME_START_GAP, FRAME_START_EV_BOT,
		  FRAME_START_MAJ1+FRAME_START_GAP, FRAME_START_EV_TOP,
		  1, FRAME_START_MAJ_COLOR);
	glRasterPos2i(FRAME_START_MAJ1+FRAME_START_GAP, FRAME_START_EV_TOP);
	glColor3fv(FRAME_START_MAJ_COLOR);
	sprintf(numb_str, "%d", maj_event[choice_maj_event].numb);
        printString(numb_str);
    }
    else {
	draw_line(FRAME_START_MAJ1, FRAME_START_EV_BOT,
		  FRAME_START_MAJ1, FRAME_START_EV_TOP,
		  1, FRAME_START_MAJ_COLOR);
	glRasterPos2i(FRAME_START_MAJ1, FRAME_START_MAJ_TOP);
	glColor3fv(FRAME_START_MAJ_COLOR);
	sprintf(numb_str, "%d", maj_event[choice_maj_event].numb);
        printString(numb_str);
    }


    /* draw first minor frame line and associated events */
    if (choice_min_event < numb_min_events) {
        draw_line(FRAME_START_MIN1, FRAME_START_MIN_BOT,
		  FRAME_START_MIN1, FRAME_START_MIN_TOP,
		  1, redvect);
	if (choice_trigger_event < numb_trigger_events) {
	    draw_line(FRAME_START_MIN1, FRAME_START_EV_BOT,
		      FRAME_START_MIN1, FRAME_START_EV_TOP,
		      1, FRAME_START_TRIGGER_COLOR);
	    glRasterPos2i(FRAME_START_MIN1, FRAME_START_MIN_TOP);
	    glColor3fv(FRAME_START_TRIGGER_COLOR);
	    sprintf(numb_str, "%d", trigger_event[choice_trigger_event].numb);
	    printString(numb_str);

	    draw_line(FRAME_START_MIN1+FRAME_START_GAP, FRAME_START_EV_BOT,
		      FRAME_START_MIN1+FRAME_START_GAP, FRAME_START_EV_TOP,
		      1, FRAME_START_MIN_COLOR);
	    glRasterPos2i(FRAME_START_MIN1+FRAME_START_GAP+4, FRAME_START_EV_TOP-4);
	    glColor3fv(FRAME_START_MIN_COLOR);
	    sprintf(numb_str, "%d", min_event[choice_min_event].numb);
	    printString(numb_str);
 	}
	else {
	    draw_line(FRAME_START_MIN1, FRAME_START_EV_BOT,
		      FRAME_START_MIN1, FRAME_START_EV_TOP,
		      1, FRAME_START_MIN_COLOR);
	    glRasterPos2i(FRAME_START_MIN1, FRAME_START_MIN_TOP);
	    glColor3fv(FRAME_START_MIN_COLOR);
	    sprintf(numb_str, "%d", min_event[choice_min_event].numb);
	    printString(numb_str);
	}
    }


    /* draw horizontal axis */
    draw_line(FRAME_START_MAJ1, FRAME_START_MID,
	      FRAME_START_MAJ2, FRAME_START_MID,
	      1, redvect);


    /* draw second minor frame line and associated events */
    if (choice_min_event < numb_min_events) {
        draw_line(FRAME_START_MIN2, FRAME_START_MIN_BOT,
		  FRAME_START_MIN2, FRAME_START_MIN_TOP,
		  1, redvect);
	if (choice_trigger_event < numb_trigger_events) {
	    draw_line(FRAME_START_MIN2, FRAME_START_EV_BOT,
		      FRAME_START_MIN2, FRAME_START_EV_TOP,
		      1, FRAME_START_TRIGGER_COLOR);
	    glRasterPos2i(FRAME_START_MIN2, FRAME_START_MIN_TOP);
	    glColor3fv(FRAME_START_TRIGGER_COLOR);
	    sprintf(numb_str, "%d", trigger_event[choice_trigger_event].numb);
	    printString(numb_str);

	    draw_line(FRAME_START_MIN2+FRAME_START_GAP, FRAME_START_EV_BOT,
		      FRAME_START_MIN2+FRAME_START_GAP, FRAME_START_EV_TOP,
		      1, FRAME_START_MIN_COLOR);
	    glRasterPos2i(FRAME_START_MIN2+FRAME_START_GAP+4, FRAME_START_EV_TOP-4);
	    glColor3fv(FRAME_START_MIN_COLOR);
	    sprintf(numb_str, "%d", min_event[choice_min_event].numb);
	    printString(numb_str);
	}
	else {
	    draw_line(FRAME_START_MIN2, FRAME_START_EV_BOT,
		      FRAME_START_MIN2, FRAME_START_EV_TOP,
		      1, FRAME_START_MIN_COLOR);
	    glRasterPos2i(FRAME_START_MIN2, FRAME_START_MIN_TOP);
	    glColor3fv(FRAME_START_MIN_COLOR);
	    sprintf(numb_str, "%d", min_event[choice_min_event].numb);
	    printString(numb_str);
	}
    }


    /* draw second major frame line and associated events */

    draw_line(FRAME_START_MAJ2, FRAME_START_MAJ_BOT,
	      FRAME_START_MAJ2, FRAME_START_MAJ_TOP,
	      1, redvect);

    if (choice_trigger_event < numb_trigger_events) {
	draw_line(FRAME_START_MAJ2, FRAME_START_EV_BOT,
		  FRAME_START_MAJ2, FRAME_START_EV_TOP,
		  1, FRAME_START_TRIGGER_COLOR);
	glRasterPos2i(FRAME_START_MAJ2, FRAME_START_MAJ_TOP);
	glColor3fv(FRAME_START_TRIGGER_COLOR);
	sprintf(numb_str, "%d", trigger_event[choice_trigger_event].numb);
        printString(numb_str);

	draw_line(FRAME_START_MAJ2+FRAME_START_GAP, FRAME_START_EV_BOT,
		  FRAME_START_MAJ2+FRAME_START_GAP, FRAME_START_EV_TOP,
		  1, FRAME_START_MAJ_COLOR);
	glRasterPos2i(FRAME_START_MAJ2+FRAME_START_GAP, FRAME_START_EV_TOP);
	glColor3fv(FRAME_START_MAJ_COLOR);
	sprintf(numb_str, "%d", maj_event[choice_maj_event].numb);
        printString(numb_str);
    }
    else {
	draw_line(FRAME_START_MAJ2, FRAME_START_EV_BOT,
		  FRAME_START_MAJ2, FRAME_START_EV_TOP,
		  1, FRAME_START_MAJ_COLOR);
	glRasterPos2i(FRAME_START_MAJ2, FRAME_START_MAJ_TOP);
	glColor3fv(FRAME_START_MAJ_COLOR);
	sprintf(numb_str, "%d", maj_event[choice_maj_event].numb);
    }


    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,  FRAME_START_WINDOW_WIDTH,  0, FRAME_START_GL_HEIGHT);
    glMatrixMode(mm);
    glXSwapBuffers(frame_start.display, XtWindow(frame_start.glWidget));

}

void
frame_start_events_CB()
{
    static int varset = 0;
    int window_width, window_height, gl_height;
    Atom wm_delete_window;
    static Widget FrameStartDialog;
    static Widget MenuParent;
    Widget TriggerMenuBar;
    Widget MajMenuBar;
    Widget MinMenuBar;
    Widget TriggerMenu, TriggerButton;
    Widget MajMenu, MajButton;
    Widget MinMenu, MinButton;
    Widget glFrame;
    int i;
    char temp_str[64];



   if (!varset) {
	varset++;

	frame_start.display = XtDisplay(master_toplevel);
	/* find an OpenGL-capable RGB visual with depth buffer */

/*	vi = glXChooseVisual(frame_start.display, 
			     DefaultScreen(frame_start.display), master_dblBuf);
	if (vi == NULL) {
	    vi = glXChooseVisual(frame_start.display, 
				 DefaultScreen(frame_start.display), 
				 master_snglBuf);
	    if (vi == NULL)
		system("inform 'error: no RGB visual with depth buffer'");
	}
*/
	/* create an OpenGL rendering context */
	frame_start.context = glXCreateContext(frame_start.display, 
					       master_vi, None, GL_TRUE);
	if (frame_start.context == NULL)
	    system("inform 'error: could not create rendering context'");



	gl_height = FRAME_START_GL_HEIGHT;
	window_width = FRAME_START_WINDOW_WIDTH;
	window_height = FRAME_START_WINDOW_HEIGHT;
	FrameStartDialog =  XtVaCreateWidget ("Frame Start",
	    xmDialogShellWidgetClass, master_toplevel,
	    XmNtitle, "Frame Start Events",
            XmNdeleteResponse, XmUNMAP,
	    XmNx, 15,
	    XmNy, 15,
	    NULL);


	wm_delete_window = XmInternAtom(XtDisplay(FrameStartDialog),
				    "WM_DELETE_WINDOW",
				    FALSE);

	XmAddWMProtocolCallback (FrameStartDialog, wm_delete_window, 
			     close_frame_start_CB, NULL);

	FrameStartWindow = XtVaCreateWidget ("frame",
	    xmFormWidgetClass, FrameStartDialog,
            XmNwidth, window_width,
            XmNheight, window_height,
	    NULL);

	TriggerMenuBar = XmCreateMenuBar (FrameStartWindow, "menuBar", NULL, 0);
	XtVaSetValues (TriggerMenuBar,
	    XmNx, FRAME_START_TRIGGER_START,
	    XmNy, FRAME_START_GL_HEIGHT,
	    NULL);
	XtManageChild (TriggerMenuBar);

	TriggerButton = XtVaCreateManagedWidget ("Frame Trigger Event",
	    xmCascadeButtonWidgetClass, TriggerMenuBar,
	    NULL);

	TriggerMenu = XmCreatePulldownMenu (TriggerMenuBar, "Frame Trigger Event", 
					    NULL, 0);
	XtVaSetValues (TriggerButton, XmNsubMenuId, TriggerMenu, NULL);

	frame_trigger_event = default_trigger_event;

	for (i=0; i<=numb_trigger_events; i++) {
	    if (i==numb_min_events) 
		sprintf(temp_str, "none");
	    else
		sprintf(temp_str, "%s (%d)", trigger_event[i].name, 
			trigger_event[i].numb);

	    trigger_val[i] = XtVaCreateManagedWidget (temp_str,
	        xmToggleButtonWidgetClass, TriggerMenu, 
	        XmNset, False,
	        NULL);

	    if (default_trigger_event == trigger_event[i].numb) {
		XtVaSetValues(trigger_val[i], XmNset, True, NULL);
	    }
	    XtAddCallback (trigger_val[i], XmNvalueChangedCallback, 
			   trigger_CB, (XtPointer)i);
	}



	MajMenuBar = XmCreateMenuBar (FrameStartWindow, "menuBar", NULL, 0);
	XtVaSetValues (MajMenuBar,
	    XmNx, FRAME_START_MAJ_START,
	    XmNy, FRAME_START_GL_HEIGHT,
	    NULL);
	XtManageChild (MajMenuBar);

	MajButton = XtVaCreateManagedWidget ("Major Frame Start Event",
	    xmCascadeButtonWidgetClass, MajMenuBar,
	    NULL);

	MajMenu = XmCreatePulldownMenu (MajMenuBar, "Major Frame Start Event", 
					    NULL, 0);
	XtVaSetValues (MajButton, XmNsubMenuId, MajMenu, NULL);

	major_frame_start_event = default_maj_event;

	for (i=0; i<numb_maj_events; i++) {
	    sprintf(temp_str, "%s (%d)", maj_event[i].name, 
		    maj_event[i].numb);

	    maj_val[i] = XtVaCreateManagedWidget (temp_str,
	        xmToggleButtonWidgetClass, MajMenu, 
	        XmNset, False,
	        NULL);

	    if (default_maj_event == maj_event[i].numb) {
		XtVaSetValues(maj_val[i], XmNset, True, NULL);
	    }
	    XtAddCallback (maj_val[i], XmNvalueChangedCallback, 
			   maj_CB, (XtPointer)i);
	}



	MinMenuBar = XmCreateMenuBar (FrameStartWindow, "menuBar", NULL, 0);
	XtVaSetValues (MinMenuBar,
	    XmNx, FRAME_START_MIN_START,
	    XmNy, FRAME_START_GL_HEIGHT,
	    NULL);
	XtManageChild (MinMenuBar);

	MinButton = XtVaCreateManagedWidget ("Minor Frame Start Event",
	    xmCascadeButtonWidgetClass, MinMenuBar,
	    NULL);

	MinMenu = XmCreatePulldownMenu (MinMenuBar, "Minor Frame Start Event", 
					    NULL, 0);
	XtVaSetValues (MinButton, XmNsubMenuId, MinMenu, NULL);

	minor_frame_start_event = default_min_event;

	for (i=0; i<=numb_min_events; i++) {
	    if (i==numb_min_events) 
		sprintf(temp_str, "none");
	    else
		sprintf(temp_str, "%s (%d)", min_event[i].name, 
			min_event[i].numb);

	    min_val[i] = XtVaCreateManagedWidget (temp_str,
	        xmToggleButtonWidgetClass, MinMenu, 
	        XmNset, False,
	        NULL);

	    if (default_min_event == min_event[i].numb) {
		XtVaSetValues(min_val[i], XmNset, True, NULL);
	    }
	    XtAddCallback (min_val[i], XmNvalueChangedCallback, 
			   min_CB, (XtPointer)i);
	}

	XtManageChild (FrameStartWindow);

	glFrame = XtVaCreateManagedWidget("glFrame", 
            xmFrameWidgetClass, FrameStartWindow,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNheight, gl_height,
            NULL);


	frame_start.glWidget = XtVaCreateManagedWidget("FrameStartglWidget", 
            glwMDrawingAreaWidgetClass, glFrame,
	    GLwNvisualInfo, master_vi, 
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM,
            XmNwidth, window_width,
            XmNheight, gl_height,
            NULL);

	frame_start.width = window_width;
	frame_start.width = gl_height;

	XtAddCallback(frame_start.glWidget, XmNexposeCallback, 
		      (XtCallbackProc)draw_frame_start, NULL);

	frame_start.window = XtWindow(frame_start.glWidget);
	XtRealizeWidget(FrameStartDialog);
	glXMakeCurrent(frame_start.display, 
		       XtWindow(frame_start.glWidget), 
		       frame_start.context);
	
	setup_font(frame_start.display);

	glClearDepth(1.0);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_MODELVIEW);




    }
    XtManageChild(FrameStartWindow);
    draw_frame_start();

}


/* code below here taken from the ideas in motion demo */


#define MAT_LOGO 		1

#define LIGHT_TMP		10




#define X 0
#define Y 1
#define Z 2

#define DEG *M_PI/180.0
#define RAD *180.0/M_PI

float move_speed;		/* Spline distance per second */



#define SPEED_SLOW		0.2	/* Spline distances per second */
#define SPEED_MEDIUM		0.4
#define SPEED_FAST		0.7
#define SPEED_SUPER_FAST	1.0

float idmat[4][4] = {
  {1.0, 0.0, 0.0, 0.0},
  {0.0, 1.0, 0.0, 0.0},
  {0.0, 0.0, 1.0, 0.0},
  {0.0, 0.0, 0.0, 1.0},
};

float light1_ambient[] = { 0.0,0.0,0.0,1.0 };
float light1_lcolor[] = { 1.0,1.0,1.0,1.0 };
float light1_position[] = { 0.0,1.0,0.0,0.0 };

float light2_ambient[] = { 0.0,0.0,0.0,1.0 };
float light2_lcolor[] = { 0.3,0.3,0.5,1.0 };
float light2_position[] = { -1.0,0.0,0.0,0.0 };

float light3_ambient[] = { 0.2,0.2,0.2,1.0 };
float light3_lcolor[] = { 0.2,0.2,0.2,1.0 };
float light3_position[] = { 0.0,-1.0,0.0,0.0 };

float lmodel_LVW[] = { 0.0 };
float lmodel_ambient[] = { 0.3,0.3,0.3,1.0 };
float lmodel_TWO[] = { GL_TRUE };

float mat_logo_ambient[] = {0.1, 0.1, 0.1, 1.0};
float mat_logo_diffuse[] = {0.5, 0.4, 0.7, 1.0};
float mat_logo_specular[] = {1.0, 1.0, 1.0, 1.0};
float mat_logo_shininess[] = {30.0};

GLubyte stipple[32*32];

typedef float vector[3];
typedef vector parameter[4];

/*
 * Function definitions
 */
void initialize(void);

parameter *calc_spline_params(vector *ctl_pts, int n);
void calc_spline(vector v, parameter *params, float current_time);
void normalize(vector v);
float dot(vector v1, vector v2);


void draw_logo();

void
init_materials() {
  int x, y;

  /* Stipple pattern */
  for (y = 0; y < 32; y++)
    for (x = 0; x < 4; x++) 
      stipple[y * 4 + x] = (y % 2) ? 0xaa : 0x55;

  glNewList(MAT_LOGO, GL_COMPILE); 
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_logo_ambient); 
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_logo_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_logo_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_logo_shininess);
  glEndList(); 

}

void init_lights()
{
  static float ambient[] = { 0.1, 0.1, 0.1, 1.0 };
  static float diffuse[] = { 0.5, 1.0, 1.0, 1.0 };
  static float position[] = { 90.0, 90.0, 150.0, 0.0 };
  
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position);

  glLightfv (GL_LIGHT1, GL_AMBIENT, light1_ambient);
  glLightfv (GL_LIGHT1, GL_SPECULAR, light1_lcolor);
  glLightfv (GL_LIGHT1, GL_DIFFUSE, light1_lcolor);
  glLightfv (GL_LIGHT1, GL_POSITION, light1_position);
    
  glLightfv (GL_LIGHT2, GL_AMBIENT, light2_ambient);
  glLightfv (GL_LIGHT2, GL_SPECULAR, light2_lcolor);
  glLightfv (GL_LIGHT2, GL_DIFFUSE, light2_lcolor);
  glLightfv (GL_LIGHT2, GL_POSITION, light2_position);

  glLightfv (GL_LIGHT3, GL_AMBIENT, light3_ambient);
  glLightfv (GL_LIGHT3, GL_SPECULAR, light3_lcolor);
  glLightfv (GL_LIGHT3, GL_DIFFUSE, light3_lcolor);
  glLightfv (GL_LIGHT3, GL_POSITION, light3_position);
  
  glLightModelfv (GL_LIGHT_MODEL_LOCAL_VIEWER, lmodel_LVW);
  glLightModelfv (GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
}

short dev, val;

float current_time=0.0;
float hold_time=0.0;		/* Used when auto-running */

float tmplight[] = {
  GL_POSITION, 0.0, 0.0, 0.0, 0.0, 
};

GLfloat tv[4][4] = {
  {1.0, 0.0, 0.0, 0.0},
  {0.0, 1.0, 0.0, -1.0},
  {0.0, 0.0, 1.0, 0.0},
  {0.0, 0.0, 0.0, 0.0},
};

#define TABLERES 12

float pcr, pcg, pcb, pca;

vector table_points[TABLERES+1][TABLERES+1];
int tablecolors[TABLERES+1][TABLERES+1];

vector paper_points[4] = {
  {-0.8, 0.0, 0.4},
  {-0.2, 0.0, -1.4},
  {1.0, 0.0, -1.0},
  {0.4, 0.0, 0.8},
};

float dot(vector, vector);

#define TIME 15
#define START_TIME 11.999

vector light_pos_ctl[] = {
  {0.0, 1.8, 0.0},
  {0.0, 1.8, 0.0},
  {0.0, 1.6, 0.0},

  {0.0, 1.6, 0.0},
  {0.0, 1.6, 0.0},
  {0.0, 1.6, 0.0},
  {0.0, 1.4, 0.0},

  {0.0, 1.3, 0.0},
  {-0.2, 1.5, 2.0},
  {0.8, 1.5, -0.4},
  {-0.8, 1.5, -0.4},

  {0.8, 2.0, 1.0},
  {1.8, 5.0, -1.8},
  {8.0, 10.0, -4.0},
  {8.0, 10.0, -4.0},
  {8.0, 10.0, -4.0},
};

vector logo_pos_ctl[] = {
  {0.0, -0.5, 0.0},

  {0.0, -0.5, 0.0},
  {0.0, -0.5, 0.0},

  {0.0, -0.5, 0.0},
  {0.0, -0.5, 0.0},
  {0.0, -0.5, 0.0},
  {0.0, 0.0, 0.0},

  {0.0, 0.6, 0.0},
  {0.0, 0.75, 0.0},
  {0.0, 0.8, 0.0},
  {0.0, 0.8, 0.0},

  {0.0, 0.5, 0.0},
  {0.0, 0.5, 0.0},
  {0.0, 0.5, 0.0},
  {0.0, 0.5, 0.0},
  {0.0, 0.5, 0.0},
};


vector logo_rot_ctl[] = {

  {0.0, 0.0, -18.4},

  {0.0, 0.0, -18.4},
  {0.0, 0.0, -18.4},

  {0.0, 0.0, -18.4},
  {0.0, 0.0, -18.4},
  {0.0, 0.0, -18.4},
  {0.0, 0.0, -18.4},
  {0.0, 0.0, -18.4},

#if 0
  {90.0, 0.0, -90.0},
  {180.0, 180.0, 90.0},
#endif  
  {240.0, 360.0, 180.0},
  {90.0, 180.0, 90.0},

  {11.9, 0.0, -18.4},
  {11.9, 0.0, -18.4},
  {11.9, 0.0, -18.4},
  {11.9, 0.0, -18.4},
  {11.9, 0.0, -18.4},
};


vector view_from_ctl[] = {
  {-1.0, 1.0, -4.0},

  {-1.0, -3.0, -4.0},	/* 0 */
  {-3.0, 1.0, -3.0},	/* 1 */

  {-1.8, 2.0, 5.4},	/* 2 */
  {-0.4, 2.0, 1.2},	/* 3 */
  {-0.2, 1.5, 0.6},	/* 4 */
  {-0.2, 1.2, 0.6},	/* 5 */

  {-0.8, 1.0, 2.4},	/* 6 */
  {-1.0, 2.0, 3.0},	/* 7 */
  {0.0, 4.0, 3.6},	/* 8 */
  {-0.8, 4.0, 1.2},	/* 9 */

  {-0.2, 3.0, 0.6},	/* 10 */
  {-0.1, 2.0, 0.3},	/* 11 */
  {-0.1, 2.0, 0.3},	/* 12 */
  {-0.1, 2.0, 0.3},	/* 13 */
  {-0.1, 2.0, 0.3},	/* 13 */
};

vector view_to_ctl[] = {
  {-1.0, 1.0, 0.0},

  {-1.0, -3.0, 0.0},
  {-1.0, 1.0, 0.0},

  {0.1, 0.0, -0.3},
  {0.1, 0.0, -0.3},
  {0.1, 0.0, -0.3},
  {0.0, 0.2, 0.0},

  {0.0, 0.6, 0.0},
  {0.0, 0.8, 0.0},
  {0.0, 0.8, 0.0},
  {0.0, 0.8, 0.0},

  {0.0, 0.8, 0.0},
  {0.0, 0.8, 0.0},
  {0.0, 0.8, 0.0},
  {0.0, 0.8, 0.0},
  {0.0, 0.8, 0.0},
};


vector view_from, view_to, light_pos, logo_pos, logo_rot;

parameter *view_from_spline, *view_to_spline,
	  *light_pos_spline, *logo_pos_spline,
	  *logo_rot_spline;

parameter *calc_spline_params(vector *, int);

double a3, a4;

#define SET_MS_ATTRIB() if (1) { \
  for (multiIndex = 0; attr[multiIndex] != GLX_SAMPLES_SGIS; multiIndex++) \
    ; \
  attr[++multiIndex] = multiSamples; \
} else 


float x_rot, y_rot, z_rot;
float x_cumal, y_cumal;
int rotate_type;

Boolean
logo_main()
{
  float x, y, z, c;
  static int logo_initiliazed = 0;
  int i;
  GLboolean auto_run = GL_FALSE; /* If set, then automatically run forever */
  struct timeval start;
  struct timeval current;
  float timediff;	
  float timeoffset;             /* Used to compute timing */
  float new_speed;              /* Set new animation speed? */
  GLboolean resetclock = GL_TRUE; /* Reset the clock? */
  GLboolean timejerk = GL_FALSE; /* Set to indicate time jerked!
                                   (menu pulled down) */
  GLboolean paused = GL_FALSE;	/* Paused? */
  GLboolean right = GL_FALSE;	/* Draw right eye? */
  XEvent event;
  char buffer[5];
  int bufsize = 5;
  KeySym key;
  XComposeStatus compose;
  
  if (! logo_initiliazed)
	  goto check_message_queue;
  if (!logo_initiliazed) {
      /* .4 spline distance per second by default */
      move_speed = SPEED_MEDIUM;
      new_speed = SPEED_MEDIUM;
      timeoffset = START_TIME;
   
      logo_initialize();
      
      x_rot = 0.1;
      y_rot = 0.0;
      z_rot = 0.0;
      x_cumal = y_cumal = 0;
      rotate_type = 0;
      current_time = timeoffset;


  }

  logo_initiliazed = 1;

    if ((current_time) > (TIME*1.0)-3.0) {
      if (auto_run) {
	hold_time += current_time - (TIME - 3.001);
	if (hold_time > 3.0) {	/* 3 second hold */
	  hold_time = 0.0;
	  resetclock = GL_TRUE;
	}
      }
      current_time = TIME*1.0-3.001;
    }

    while(XPending(dpy)) {
      XNextEvent(dpy, &event);
      if (event.type == ConfigureNotify)
	resize_window();
    }


    calc_spline(view_from, view_from_spline, current_time);
    calc_spline(view_to, view_to_spline, current_time);
    calc_spline(light_pos, light_pos_spline, current_time);
    calc_spline(logo_pos, logo_pos_spline, current_time);
    calc_spline(logo_rot, logo_rot_spline, current_time);
    
    tmplight[1] = light_pos[X] - logo_pos[X];
    tmplight[2] = light_pos[Y] - logo_pos[Y];
    tmplight[3] = light_pos[Z] - logo_pos[Z];
    
    glNewList(LIGHT_TMP, GL_COMPILE); 
    glMaterialf(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, * tmplight); 
    glEndList();
    
    tv[0][0] = tv[1][1] = tv[2][2] = light_pos[Y];
    
    glColor3ub(0,  0,  0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); 

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(.1*(450),  5.0/4.0,  0.5,  20.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(&idmat[0][0]); 
    
    gluLookAt(view_from[X],  view_from[Y],  view_from[Z],
	      view_to[X],  view_to[Y],  view_to[Z], 
	      0.0, 1.0, 0.0);
    


    glDisable(GL_LIGHT2);
    glDisable(GL_LIGHT3); 
    glEnable(GL_LIGHT1);
    glLightfv(GL_LIGHT1, GL_POSITION, light_pos);
    
    if (logo_pos[Y] > -0.33) {

      glCallList(MAT_LOGO);


      logo_rot[X] = x_rot;
      logo_rot[Y] = y_rot;
      logo_rot[Z] = z_rot;
      
      if (rotate_type == 0) {
	  x_rot += 0.2;
	  if (x_rot > 360) {
	      x_cumal += 1.0;
	      x_rot = x_cumal;
	      rotate_type = 1;
	  }
      }
      else {
	  y_rot += 0.2;
	  if (y_rot > 360) {
	      y_cumal += 1.0;
	      y_rot = y_cumal;
	      rotate_type = 0;
	  }
      }



      glPushMatrix();
      glTranslatef(logo_pos[X],  logo_pos[Y],  logo_pos[Z]);
      glScalef(0.04,  0.04,  0.04);

      glColor3fv(redvect);


      glRasterPos3i(-1,6,-12);
      printString ("Frameview");


      glRotatef (0.1 * (-900), 1.0, 0.0, 0.0);
      glRotatef (0.1 * ((int)(10.0*logo_rot[Z])), 0.0, 0.0, 1.0);
      glRotatef (0.1 * ((int)(10.0*logo_rot[Y])), 0.0, 1.0, 0.0);
      glRotatef (0.1 * ((int)(10.0*logo_rot[X])), 1.0, 0.0, 0.0);
      glRotatef (0.1 * (353), 1.0, 0.0, 0.0);
      glRotatef (0.1 * (450), 0.0, 1.0, 0.0);
      glEnable(GL_LIGHTING);
      draw_logo();


      glPopMatrix();
    }
    



    glXSwapBuffers(dpy, XtWindow(glxarea));


    /* Time jerked -- adjust clock appropriately */
    if (timejerk) {
      timejerk = GL_FALSE;
      timeoffset = current_time;
      gettimeofday(&start, NULL);
    }
    
    /* Reset our timer */
    if (resetclock) {
      resetclock = GL_FALSE;
      paused = GL_FALSE;
      timeoffset = START_TIME;
      gettimeofday(&start, NULL);
    }
    
    /* Compute new time */
    gettimeofday(&current, NULL);
    timediff = (current.tv_sec - start.tv_sec) + 
      (current.tv_usec - start.tv_usec) / 1000000.0;
    if (!paused) current_time = timediff * move_speed + timeoffset;

    
    /* Adjust to new speed */
    if (new_speed != move_speed) {
      move_speed = new_speed;
      timeoffset = current_time;
      gettimeofday(&start, NULL);
    }
 check_message_queue:
  /* see dialog.c for comment */

  if (msg_data->ready) {
      message_dialog(msg_data->msg, msg_data->type, msg_data->identifier);
  }
  sginap(100);
  return((Boolean)0);
}

void
logo_initialize()
{
  int hasCap;


  glXGetConfig(dpy, master_vi, GLX_USE_GL, &hasCap);

/*  if (!hasCap) {
    system("inform 'Your system must support OpenGL to run ideas'");
    exit(1);
  }
  glXGetConfig(dpy, vi, GLX_RGBA, &hasCap);
  if (!hasCap) {
    system("inform 'Your system must support RGB mode to run ideas'");
    exit(1);
  }
  glXGetConfig(dpy, vi, GLX_DEPTH_SIZE, &hasCap);
  if (hasCap == 0) {
    system("inform 'Your system must have a z-buffer to run ideas'");
    exit(1);
  }
  */
  glXMakeCurrent(dpy, XtWindow(glxarea), cx);

  init_lights();
  init_materials();



  view_from_spline = calc_spline_params(view_from_ctl, TIME);
  view_to_spline = calc_spline_params(view_to_ctl, TIME);
  light_pos_spline = calc_spline_params(light_pos_ctl, TIME);
  logo_pos_spline = calc_spline_params(logo_pos_ctl, TIME);
  logo_rot_spline = calc_spline_params(logo_rot_ctl, TIME);

  resize_window();

  glMatrixMode(GL_MODELVIEW);


}



void
resize_window() 
{
    Dimension dim_width,dim_height;


    XtVaGetValues(glxarea, XmNwidth, &dim_width, 
		  XmNheight, &dim_height, NULL);


    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective (45.0, 5.0/4.0, 0.5, 20.0); 
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, (int)dim_width, (int)dim_height);
}



void calc_spline(vector v, parameter *params, float current_time)
{

    float t, tt;
    int i, j;

    tt = t = current_time - (float)((int)current_time);

    for (i=0; i<3; i++) {


	v[i] = params[(int)current_time][3][i] +
	       params[(int)current_time][2][i] * t +
	       params[(int)current_time][1][i] * t * t +
	       params[(int)current_time][0][i] * t * t * t;
    }

}


parameter *calc_spline_params(vector *ctl_pts, int n)
{

    int i, j;
    parameter *params;

    if (n<4) {
	fprintf(stderr,
	    "calc_spline_params: not enough control points\n");
	return (NULL);
    }

    params = (parameter *)malloc(sizeof(parameter) * (n-3));

    for (i=0; i<n-3; i++) {

	for (j=0; j<3; j++) {

	    params[i][3][j] = ctl_pts[i+1][j];
	    params[i][2][j] = ctl_pts[i+2][j] - ctl_pts[i][j];
	    params[i][1][j] =  2.0 * ctl_pts[i][j] +
			      -2.0 * ctl_pts[i+1][j] +
			       1.0 * ctl_pts[i+2][j] +
			      -1.0 * ctl_pts[i+3][j];
	    params[i][0][j] = -1.0 * ctl_pts[i][j] +
			       1.0 * ctl_pts[i+1][j] +
			      -1.0 * ctl_pts[i+2][j] +
			       1.0 * ctl_pts[i+3][j];

	}
    }

    return (params);
}


void normalize(vector v)
{
    float r;

    r = sqrt(v[X]*v[X] + v[Y]*v[Y] + v[Z]*v[Z]);

    v[X] /= r;
    v[Y] /= r;
    v[Z] /= r;
}


float dot(vector v1, vector v2)
{
    return v1[X]*v2[X]+v1[Y]*v2[Y]+v1[Z]*v2[Z];
}


/* code below here is taken from the textfun demo */


typedef struct {
    short           width;
    short           height;
    short           xoffset;
    short           yoffset;
    short           advance;
    char           *bitmap;
}               PerCharInfo, *PerCharInfoPtr;

typedef struct {
    int             min_char;
    int             max_char;
    int             max_ascent;
    int             max_descent;
    GLuint          dlist_base;
    PerCharInfo     glyph[1];
}               FontInfo, *FontInfoPtr;

typedef struct {
    char           *name;
    char           *xlfd;
    XFontStruct    *xfont;
    FontInfoPtr     fontinfo;
}               FontEntry, *FontEntryPtr;


static FontEntry fontEntry[] =
{
    {"Fixed", "fixed", NULL, NULL},
    {"Utopia", "-adobe-utopia-medium-r-normal--20-*-*-*-p-*-iso8859-1", NULL, NULL},
    {"Schoolbook", "-adobe-new century schoolbook-bold-i-normal--20-*-*-*-p-*-iso8859-1", NULL, NULL},
    {"Rock", "-sgi-rock-medium-r-normal--20-*-*-*-p-*-iso8859-1", NULL, NULL},
    {"Rock (hi-res)", "-sgi-rock-medium-r-normal--50-*-*-*-p-*-iso8859-1", NULL, NULL},
    {"Curl", "-sgi-curl-medium-r-normal--20-*-*-*-p-*-*-*", NULL, NULL},
    {"Curl (hi-res)", "-sgi-curl-medium-r-normal--50-*-*-*-p-*-*-*", NULL, NULL},
    {"Dingbats", "-adobe-itc zapf dingbats-medium-r-normal--35-*-*-*-p-*-adobe-fontspecific", NULL, NULL}
};
#define NUM_FONT_ENTRIES sizeof(fontEntry)/sizeof(FontEntry)

static char    *defaultMessage[] =
{"FrameView"};
#define NUM_DEFAULT_MESSAGES sizeof(defaultMessage)/sizeof(char*)


GLboolean       motion = GL_FALSE,
                dollying = GL_TRUE;




GLfloat         theta = 0;
GLfloat         distance = 19, angle = 0;
GLuint          base;
int             numMessages;
char          **messages;

void 
text_draw(Widget w)
{
    GLfloat         red, green, blue;
    int             i;

    glClear(GL_DEPTH_BUFFER_BIT);

    /* paint black to blue smooth shaded polygon for background */
    glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glBegin(GL_POLYGON);
    glColor3f(1.0, 1.0, 1.0);
    glVertex3f(-20, 20, -19);
    glVertex3f(20, 20, -19);
    glColor3f(0.0, 0.0, 1.0);
    glVertex3f(20, -20, -19);
    glVertex3f(-20, -20, -19);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_FLAT);

    glPushMatrix();
    glTranslatef(0, 0, -distance);
    glRotatef(angle, 0, 0, 1);

    glCallList(base);
    glPopMatrix();

    if (doubleBuffer)
	glXSwapBuffers(dpy, XtWindow(w));
    if (!glXIsDirect(dpy, cx))
	glFinish();		/* avoid indirect rendering latency from
				 * queuing */

}



#define DEBUG_GLYPH4(msg,a,b,c,d) { /* nothing */ }
#define DEBUG_GLYPH(msg) { /* nothing */ }


#define MAX_GLYPHS_PER_GRAB 512 /* this is big enough for 2^9 glyph character sets */

FontInfoPtr
SuckGlyphsFromServer(Display * dpy, Font font)
{
    Pixmap          offscreen;
    XFontStruct    *fontinfo;
    XImage         *image;
    GC              xgc;
    XGCValues       values;
    int             numchars;
    int             width, height, pixwidth;
    int             i, j;
    XCharStruct    *charinfo;
    XChar2b         character;
    char           *bitmapData;
    int             x, y;
    int             spanLength;
    int             charWidth, charHeight, maxSpanLength;
    int             grabList[MAX_GLYPHS_PER_GRAB];
    int             glyphsPerGrab = MAX_GLYPHS_PER_GRAB;
    int             numToGrab, thisglyph;
    FontInfoPtr     myfontinfo;

    fontinfo = XQueryFont(dpy, font);
    if (!fontinfo)
	return NULL;

    numchars = fontinfo->max_char_or_byte2 - fontinfo->min_char_or_byte2 + 1;
    if (numchars < 1)
	return NULL;

    myfontinfo = (FontInfoPtr) malloc(sizeof(FontInfo) + (numchars - 1) * sizeof(PerCharInfo));
    if (!myfontinfo)
	return NULL;

    myfontinfo->min_char = fontinfo->min_char_or_byte2;
    myfontinfo->max_char = fontinfo->max_char_or_byte2;
    myfontinfo->max_ascent = fontinfo->max_bounds.ascent;
    myfontinfo->max_descent = fontinfo->max_bounds.descent;
    myfontinfo->dlist_base = 0;

    width = fontinfo->max_bounds.rbearing - fontinfo->min_bounds.lbearing;
    height = fontinfo->max_bounds.ascent + fontinfo->max_bounds.descent;

    maxSpanLength = (width + 7) / 8;
    /*
     * Be careful determining the width of the pixmap; the X protocol allows
     * pixmaps of width 2^16-1 (unsigned short size) but drawing coordinates
     * max out at 2^15-1 (signed short size).  If the width is too large,
     * we need to limit the glyphs per grab.
     */
    if ((glyphsPerGrab * 8 * maxSpanLength) >= (1 << 15)) {
	glyphsPerGrab = (1 << 15) / (8 * maxSpanLength);
    }
    pixwidth = glyphsPerGrab * 8 * maxSpanLength;
    offscreen = XCreatePixmap(dpy, RootWindow(dpy, DefaultScreen(dpy)),
			      pixwidth, height, 1);

    values.font = font;
    values.background = 0;
    values.foreground = 0;
    xgc = XCreateGC(dpy, offscreen, GCFont | GCBackground | GCForeground, &values);

    XFillRectangle(dpy, offscreen, xgc, 0, 0, 8 * maxSpanLength * glyphsPerGrab, height);
    XSetForeground(dpy, xgc, 1);

    numToGrab = 0;
    if (fontinfo->per_char == NULL) {
	charinfo = &(fontinfo->min_bounds);
	charWidth = charinfo->rbearing - charinfo->lbearing;
	charHeight = charinfo->ascent + charinfo->descent;
	spanLength = (charWidth + 7) / 8;
    }
    for (i = 0; i < numchars; i++) {
	if (fontinfo->per_char != NULL) {
	    charinfo = &(fontinfo->per_char[i]);
	    charWidth = charinfo->rbearing - charinfo->lbearing;
	    charHeight = charinfo->ascent + charinfo->descent;
	    if (charWidth == 0 || charHeight == 0) {
		/* Still must move raster pos even if empty character */
		myfontinfo->glyph[i].width = 0;
		myfontinfo->glyph[i].height = 0;
		myfontinfo->glyph[i].xoffset = 0;
		myfontinfo->glyph[i].yoffset = 0;
		myfontinfo->glyph[i].advance = charinfo->width;
		myfontinfo->glyph[i].bitmap = NULL;
		goto PossiblyDoGrab;
	    }
	}
	grabList[numToGrab] = i;

	/* XXX is this right for large fonts? */
	character.byte2 = (i + fontinfo->min_char_or_byte2) & 255;
	character.byte1 = (i + fontinfo->min_char_or_byte2) >> 8;

	/*
	 * XXX we could use XDrawImageString16 which would also paint the
	 * backing rectangle but X server bugs in some scalable font
	 * rasterizers makes it more effective to do XFillRectangles to clear
	 * the pixmap and XDrawImage16 for the text.
	 */
	XDrawString16(dpy, offscreen, xgc,
		      -charinfo->lbearing + 8 * maxSpanLength * numToGrab,
		      charinfo->ascent, &character, 1);

	numToGrab++;

      PossiblyDoGrab:

	if (numToGrab >= glyphsPerGrab || i == numchars - 1) {
	    image = XGetImage(dpy, offscreen,
		  0, 0, pixwidth, height, 1, XYPixmap);
	    for (j = 0; j < numToGrab; j++) {
		thisglyph = grabList[j];
		if (fontinfo->per_char != NULL) {
		    charinfo = &(fontinfo->per_char[thisglyph]);
		    charWidth = charinfo->rbearing - charinfo->lbearing;
		    charHeight = charinfo->ascent + charinfo->descent;
		    spanLength = (charWidth + 7) / 8;
		}
		bitmapData = calloc(height * spanLength, sizeof(char));
		if (!bitmapData)
		    goto FreeFontAndReturn;
		system("inform 'glpyh error'");
                DEBUG_GLYPH4("index %d, glyph %d (%d by %d)\n",
		    j, thisglyph + fontinfo->min_char_or_byte2, charWidth, charHeight);
		for (y = 0; y < charHeight; y++) {
		    for (x = 0; x < charWidth; x++) {
			/*
			 * XXX The algorithm used to suck across the font ensures
			 * that each glyph begins on a byte boundary.  In theory
			 * this would make it convienent to copy the glyph into
			 * a byte oriented bitmap.  We actually use the XGetPixel
			 * function to extract each pixel from the image which is
			 * not that efficient.  We could either do tighter packing
			 * in the pixmap or more efficient extraction from the
			 * image.  Oh well.
			 */
			if (XGetPixel(image, j * maxSpanLength * 8 + x, charHeight - 1 - y)) {
			    DEBUG_GLYPH("x");
			    bitmapData[y * spanLength + x / 8] |= (1 << (x & 7));
			} else {
			    DEBUG_GLYPH(" ");
			}
		    }
		    DEBUG_GLYPH("\n");
		}
		myfontinfo->glyph[thisglyph].width = charWidth;
		myfontinfo->glyph[thisglyph].height = charHeight;
		myfontinfo->glyph[thisglyph].xoffset = -charinfo->lbearing;
		myfontinfo->glyph[thisglyph].yoffset = charinfo->descent;
		myfontinfo->glyph[thisglyph].advance = charinfo->width;
		myfontinfo->glyph[thisglyph].bitmap = bitmapData;
	    }
	    XDestroyImage(image);
	    numToGrab = 0;
	    /* do we need to clear the offscreen pixmap to get more? */
	    if (i < numchars - 1) {
		XSetForeground(dpy, xgc, 0);
		XFillRectangle(dpy, offscreen, xgc, 0, 0, 8 * maxSpanLength * glyphsPerGrab, height);
		XSetForeground(dpy, xgc, 1);
	    }
	}
    }
    XFreeGC(dpy, xgc);
    XFreePixmap(dpy, offscreen);
    return myfontinfo;

  FreeFontAndReturn:
    XDestroyImage(image);
    XFreeGC(dpy, xgc);
    XFreePixmap(dpy, offscreen);
    for (j = i - 1; j >= 0; j--) {
	if (myfontinfo->glyph[j].bitmap)
	    free(myfontinfo->glyph[j].bitmap);
    }
    free(myfontinfo);
    return NULL;
}

void
tick()
{
    if (dollying) {
	theta += 0.1;
	distance = cos(theta) * 7 + 12;
    }
}

void
MakeCube(void)
{
    /*
     * No back side to the cube is drawn since the animation makes sure the
     * back side can never be visible.  The "wobble" function is constrained
     * so not to rotate far enough around to reveal the back side.
     */
    glNewList(1, GL_COMPILE);
    glBegin(GL_QUAD_STRIP);
    /* back left post */
    glColor3f(6.0, 0.5, 0.5);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 1, 0);
    /* front left post */
    glVertex3f(0, 0, 1);
    glVertex3f(0, 1, 1);
    glColor3f(1.0, 0.0, 0.0);
    /* front right post */
    glVertex3f(1, 0, 1);
    glVertex3f(1, 1, 1);
    /* back right post */
    glColor3f(6.0, 0.5, 0.5);
    glVertex3f(1, 0, 0);
    glVertex3f(1, 1, 0);
    glEnd();
    glBegin(GL_QUADS);
    /* top face */
    glVertex3f(1, 1, 0);
    glVertex3f(1, 1, 1);
    glVertex3f(0, 1, 1);
    glVertex3f(0, 1, 0);
    /* bottom face */
    glVertex3f(1, 0, 0);
    glVertex3f(1, 0, 1);
    glVertex3f(0, 0, 1);
    glVertex3f(0, 0, 0);
    glEnd();
    glEndList();
}

void
MakeGlyphDisplayList(FontInfoPtr font, int c)
{
    PerCharInfoPtr  glyph;
    char           *bitmapData;
    int             width, height, spanLength;
    int             x, y;

    if (c < font->min_char || c > font->max_char)
	return;
    if (font->dlist_base == 0) {
	font->dlist_base = glGenLists(font->max_char - font->min_char + 1);
	if (font->dlist_base == 0)
	    XtAppError(master_app, "could not generate font display lists");
    }
    glyph = &font->glyph[c - font->min_char];
    glNewList(c - font->min_char + font->dlist_base, GL_COMPILE);
    bitmapData = glyph->bitmap;
    if (bitmapData) {
	int             oldx = 0, oldy = 0;

	glPushMatrix();
	glTranslatef(-glyph->xoffset, -glyph->yoffset, 0);
	width = glyph->width;
	spanLength = (width + 7) / 8;
	height = glyph->height;
	for (x = 0; x < width; x++) {
	    for (y = 0; y < height; y++) {
		if (bitmapData[y * spanLength + x / 8] & (1 << (x & 7))) {
		    int             y1, count;

                    /*
		     * Fonts tend to have good vertical repetion.  If we find that
		     * the vertically adjacent  pixels in the glyph bitmap are also enabled,
		     * we can scale a single cube instead of drawing a cube per pixel.
		     */
		    for (y1 = y + 1, count = 1; y < height; y1++, count++) {
			if (!(bitmapData[y1 * spanLength + x / 8] & (1 << (x & 7))))
			    break;
		    }
		    glTranslatef(x - oldx, y - oldy, 0);
		    oldx = x;
		    oldy = y;
		    if (count > 1) {
			glPushMatrix();
			glScalef(1, count, 1);
			glCallList(1);
			glPopMatrix();
			y += count - 1;
		    } else {
			glCallList(1);
		    }
		}
	    }
	}
	glPopMatrix();
    }
    glTranslatef(glyph->advance, 0, 0);
    glEndList();
}

GLuint
GetGlyphDisplayList(FontInfoPtr font, int c)
{
    PerCharInfoPtr  glyph;

    if (c < font->min_char || c > font->max_char)
	return 0;
    if (font->dlist_base == 0)
	XtAppError(master_app, "font not display listed");
    return c - font->min_char + font->dlist_base;
}

void
MakeStringDisplayList(FontInfoPtr font, char *message, GLuint dlist)
{
    char  *c;

    for (c = message; *c != '\0'; c++) {
	MakeGlyphDisplayList(font, *c);
    }
    glNewList(dlist, GL_COMPILE);
    for (c = message; *c != '\0'; c++) {
	glCallList(GetGlyphDisplayList(font, *c));
    }
    glEndList();
}

int
GetStringLength(FontInfoPtr font, char *message)
{
    char  *c;
    int             ch;
    int             width = 0;

    for (c = message; *c != '\0'; c++) {
	ch = *c;
	if (ch >= font->min_char && ch <= font->max_char) {
	    width += font->glyph[ch - font->min_char].advance;
	}
    }
    return width;
}

void
SetupMessageDisplayList(FontEntryPtr fontEntry, int num, char *message[])
{
    FontInfoPtr     fontinfo = fontEntry->fontinfo;
    GLfloat         scaleFactor;
    int             totalHeight, maxWidth, height, width;
    int             i;

    if (!fontinfo) {
	fontinfo = SuckGlyphsFromServer(dpy, fontEntry->xfont->fid);
	fontEntry->fontinfo = fontinfo;
    }
    height = fontinfo->max_ascent + fontinfo->max_descent;
    maxWidth = 0;
    for (i = 0; i < num; i++) {
	MakeStringDisplayList(fontinfo, message[i], base + i + 1);
	width = GetStringLength(fontinfo, message[i]);
	if (width > maxWidth)
	    maxWidth = width;
    }

#define SHRINK_FACTOR 25.0	/* empirical */

    totalHeight = height * num - fontinfo->max_descent;
    if (maxWidth > totalHeight) {
	scaleFactor = SHRINK_FACTOR / maxWidth;
    } else {
	scaleFactor = SHRINK_FACTOR / totalHeight;
    }

    glNewList(base, GL_COMPILE);
    glScalef(scaleFactor, scaleFactor, 1); /* 1 in Z gives glyphs constant depth */
    for (i = 0; i < num; i++) {
	glPushMatrix();
	width = GetStringLength(fontinfo, message[i]);
	glTranslatef(-width / 2.0, height * (num - i - 1) - totalHeight / 2.0, 0);
	glCallList(base + i + 1);
	glPopMatrix();
    }
    glEndList();
}

void
fontSelect(Widget widget, XtPointer client_data, XmRowColumnCallbackStruct * cbs)
{
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *) cbs->callbackstruct;
    FontEntryPtr    fontEntry = (FontEntryPtr) cbs->data;

    if (state->set) {
	SetupMessageDisplayList(fontEntry, numMessages, messages);
	if (!motion)
	    text_draw(glxarea);
    }
}


Boolean
text_main()
{
    int             i;


    for (i = 0; i < NUM_FONT_ENTRIES; i++) {
	fontEntry[i].xfont = XLoadQueryFont(dpy, fontEntry[i].xlfd);
	if (i == 0 && !fontEntry[i].xfont)
	    XtAppError(master_app, "could not get basic font");
    }

    fontEntry[0].fontinfo = SuckGlyphsFromServer(dpy, fontEntry[0].xfont->fid);
    if (!fontEntry[0].fontinfo)
	XtAppError(master_app, "could not get font glyphs");


    /*
     * Once widget is realized (ie, associated with a created X window), we
     * can bind the OpenGL rendering context to the window.
     */
    glXMakeCurrent(dpy, XtWindow(glxarea), cx);


    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);
    glMatrixMode(GL_PROJECTION);
    glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 80);
    glMatrixMode(GL_MODELVIEW);

    MakeCube();



    numMessages = NUM_DEFAULT_MESSAGES;
    messages = defaultMessage;


    base = glGenLists(numMessages + 1);
    SetupMessageDisplayList(&fontEntry[1], numMessages, messages);


    return((Boolean)1);
}









