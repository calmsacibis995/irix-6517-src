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
 * File:	menu.c						          *
 *									  *
 * Description:	This file sets up the menus and backgroud for grtmon  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#define GRMAIN extern
#include "graph.h"


Widget
makeMenus (Widget parent)
{
    Widget BackColor, ResetMaxes;
    Widget menuBar, controlButton, controlMenu, controlQuit;
    Widget labelButton, labelMenu, unitButton, unitMenu;
    Widget ticButton, ticMenu, divButton, divMenu;
    Widget scanButton, scanMenu;
    Widget file_real_Menu;
    Widget displayButton, displayMenu;
    Widget zoomButton, zoomMenu, zoom_control, zoom[10];  /* 10 number keys */
    Widget scrollButton, scrollMenu, scroll[6];  /* right left by 1/2 & 1/10 */
    Widget GraphModeButton, GraphModeMenu;
    Widget adjustButton, adjustMenu;
    Widget DefineSearch, ForSearch, RevSearch;
    Widget GotoFrame, DefTempFrame;
    Widget FrameSyncButton, FrameSyncMenu;

    Widget resetButton, resetMenu, resetAll;
    Widget average_button[MAX_NUMB_TO_AVE+1];
    Widget graphButton, graphMenu;
    Widget compButton, compMenu;
    Widget average_menu[MAXCPU];
    Widget scaleButton, scaleMenu, scale;
    Widget StartLiveStream;

/*    XmString zoom_accel[10];
    XmString scroll_accel[6];
    XmString next_accel;
    XmString prev_accel;
    XmString goto_frame_accel;
    XmString forSearch_accel;
    XmString revSearch_accel;
    XmString quit_accel, resetAll_accel, enableAll_accel;
*/

    Arg args[20];
    int n;

    char temp_str[32];
    XmString tempString;

    int i,j;
    int ii;  /* when we go though a loop this variable represents
		the gr_cpu_map of i */

    /* for setting up tear away menus */
    n = 0;
/*    XtSetArg (args[n], XmNtearOffModel, XmTEAR_OFF_ENABLED); n++;*/


    /* create menu bars */
    menuBar = XmCreateMenuBar (parent, "menuBar", NULL, 0);
    XtVaSetValues (menuBar,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNtopAttachment, XmATTACH_FORM,
	    NULL);
    XtManageChild (menuBar);

    controlButton = XtVaCreateManagedWidget ("Control",
	    xmCascadeButtonWidgetClass, menuBar,
	    XmNmnemonic, 'C',
	    NULL);

    controlMenu = XmCreatePulldownMenu (menuBar, "Control", NULL, 0);
    XtVaSetValues (controlButton, XmNsubMenuId, controlMenu, NULL);


/*
    BackColor = XtVaCreateManagedWidget ("Background Color",
	    xmPushButtonWidgetClass, controlMenu,
	    NULL);
    XtAddCallback (BackColor, XmNactivateCallback, back_color_CB, NULL);

    XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, controlMenu, NULL);
*/

    if (! no_frame_mode) {
        ResetMaxes = XtVaCreateManagedWidget ("Reset Maxes",
    	    xmPushButtonWidgetClass, controlMenu,
    	    NULL);
        XtAddCallback (ResetMaxes, XmNactivateCallback, reset_maxes_CB, NULL);
    
        XtVaCreateManagedWidget ("separator",
    	    xmSeparatorGadgetClass, controlMenu, NULL);
    
        tempString = XmStringCreateSimple ("Ctrl-G");
        GotoFrame = XtVaCreateManagedWidget ("Goto Frame",
    	xmPushButtonWidgetClass, controlMenu,
            XmNmnemonic, 'G',
    	XmNaccelerator, "Ctrl<Key>G",
    	XmNacceleratorText, tempString,
    	NULL);
        XmStringFree (tempString);
        XtAddCallback (GotoFrame, XmNactivateCallback,
    		goto_frame_CB, (XtPointer) (-1));
    
        XtVaCreateManagedWidget ("separator",
    	    xmSeparatorGadgetClass, controlMenu, NULL);
    
        FrameSyncButton = XtVaCreateManagedWidget ("Frame Sync", 
            xmCascadeButtonWidgetClass, controlMenu, NULL);  
    
    
        FrameSyncMenu = XmCreatePulldownMenu (FrameSyncButton, "Frame Sync", 
					      NULL, 0);
        XtVaSetValues (FrameSyncButton, XmNsubMenuId, FrameSyncMenu, NULL);
    
        for (i=0;i<2;i++) {
    	if (i==0)
    	    sprintf(temp_str, "%s", "time");
    	else
	    sprintf(temp_str, "%s", "as is");

	frame_sync_val[i] = XtVaCreateManagedWidget (temp_str,
	    xmToggleButtonWidgetClass, FrameSyncMenu, 
	    XmNset, (i==frame_sync) ? True : False,
	    NULL);

	XtAddCallback (frame_sync_val[i], XmNvalueChangedCallback, 
		           frame_sync_CB, (XtPointer)i);
        }
    
    
        XtVaCreateManagedWidget ("separator",
    	    xmSeparatorGadgetClass, controlMenu, NULL);
    
        tempString = XmStringCreateSimple ("Ctrl-T");
        DefTempFrame = XtVaCreateManagedWidget ("Define Template",
            xmPushButtonWidgetClass, controlMenu,
            XmNmnemonic, 'T',
    	    XmNaccelerator, "Ctrl<Key>T",
    	    XmNacceleratorText, tempString,
    	    NULL);
        XmStringFree (tempString);
    
        XtAddCallback (DefTempFrame, XmNactivateCallback,
    		define_template_CB, (XtPointer) (-1));

	XtVaCreateManagedWidget ("separator",
        	    xmSeparatorGadgetClass, controlMenu, NULL);
    }
    DefineSearch = XtVaCreateManagedWidget ("Define Search",
	    xmPushButtonWidgetClass, controlMenu,
	    NULL);
    XtAddCallback (DefineSearch, XmNactivateCallback, search_event_CB, NULL);

    XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, controlMenu, NULL);

    tempString = XmStringCreateSimple ("Ctrl-S");
    ForSearch = XtVaCreateManagedWidget ("Search for Event",
	xmPushButtonWidgetClass, controlMenu,
        XmNmnemonic, 'S',
	XmNaccelerator, "Ctrl<Key>S",
	XmNacceleratorText, tempString,
	NULL);
    XmStringFree (tempString);

    XtAddCallback (ForSearch, XmNactivateCallback,
		Forward_Search_CB, (XtPointer) (-1));

    XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, controlMenu, NULL);

    tempString = XmStringCreateSimple ("Ctrl-R");
    RevSearch = XtVaCreateManagedWidget ("Reverse Search",
	xmPushButtonWidgetClass, controlMenu,
        XmNmnemonic, 'R',
	XmNaccelerator, "Ctrl<Key>R",
	XmNacceleratorText, tempString,
	NULL);
    XmStringFree (tempString);

    XtAddCallback (RevSearch, XmNactivateCallback,
		Reverse_Search_CB, (XtPointer) (-1));

    XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, controlMenu, NULL);

    if (! no_frame_mode) {
        GraphModeButton = XtVaCreateManagedWidget ("Graph Mode", 
            xmCascadeButtonWidgetClass, controlMenu, NULL);  
    
    
        GraphModeMenu = XmCreatePulldownMenu (GraphModeButton, "Graph Mode", 
					      NULL, 0);
        XtVaSetValues (GraphModeButton, XmNsubMenuId, GraphModeMenu, NULL);
    
        for (i=0;i<2;i++) {
    	if (i==0)
    	    sprintf(temp_str, "%s", "summarized time");
    	else
    	    sprintf(temp_str, "%s", "real-time explicit");
    
    	graph_mode_val[i] = XtVaCreateManagedWidget (temp_str,
    	    xmToggleButtonWidgetClass, GraphModeMenu, 
    	    XmNset, (i==graph_mode) ? True : False,
    	    NULL);
    
    	XtAddCallback (graph_mode_val[i], XmNvalueChangedCallback, 
    		           graph_mode_CB, (XtPointer)i);
        }
    

	XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, controlMenu, NULL);
    }
/* see TIMESTAMP_ADJ_COMMENT in frameview.h
    adjustButton = XtVaCreateManagedWidget ("Timestamp Adjustment", 
        xmCascadeButtonWidgetClass, controlMenu, NULL);  

    adjustMenu = XmCreatePulldownMenu (adjustButton, "Timestamp Adjustment", NULL, 0);
    XtVaSetValues (adjustButton, XmNsubMenuId, adjustMenu, NULL);

    for (i=0;i<2;i++) {
	if (i==0)
	    strcpy(temp_str, "off");
	else
	    strcpy(temp_str, "on");


	adjust_val[i] = XtVaCreateManagedWidget (temp_str,
	    xmToggleButtonWidgetClass, adjustMenu, 
	    XmNset, (i==adjust_mode) ? True : False,
	    NULL);

	XtAddCallback (adjust_val[i], XmNvalueChangedCallback, 
		       adjust_CB, (XtPointer)i);
    }
    

    XtVaCreateManagedWidget ("separator",
	xmSeparatorGadgetClass, controlMenu, NULL);
*/

    if ((! no_frame_mode) && (computed_minor_time > 0)){
        displayButton = XtVaCreateManagedWidget ("Frame Times", 
            xmCascadeButtonWidgetClass, controlMenu, NULL);  

        displayMenu = XmCreatePulldownMenu (displayButton, "Frame Times", NULL, 0);
        XtVaSetValues (displayButton, XmNsubMenuId, displayMenu, NULL);

        for (i=0;i<2;i++) {
	    if (i==0)
	        strcpy(temp_str, "computed");
	    else
	        strcpy(temp_str, "actual");

	    frame_display_val[i] = XtVaCreateManagedWidget (temp_str,
	        xmToggleButtonWidgetClass, displayMenu, 
	        XmNset, (i==display_frame_type) ? True : False,
	        NULL);

	    XtAddCallback (frame_display_val[i], XmNvalueChangedCallback, 
		           frame_display_CB, (XtPointer)i);
	}
	

	XtVaCreateManagedWidget ("separator",
            xmSeparatorGadgetClass, controlMenu, NULL);
    }


    StopMenu =  XtVaCreateManagedWidget ("Stop Live Stream",
	    xmPushButtonWidgetClass, controlMenu,
	    NULL);
    XtAddCallback (StopMenu, XmNactivateCallback, stop_menu_CB, 0);

    StopMenuSep = XtVaCreateManagedWidget ("separator",
        xmSeparatorGadgetClass, controlMenu, NULL);

    tempString = XmStringCreateSimple ("Ctrl-Q");
    controlQuit = XtVaCreateManagedWidget ("Quit",
	    xmPushButtonWidgetClass, controlMenu,
	    XmNmnemonic, 'Q',
	    XmNaccelerator, "Ctrl<Key>Q",
	    XmNacceleratorText, tempString,
	    NULL);
    XtAddCallback (controlQuit, XmNactivateCallback, quit_CB, 0);
    XmStringFree (tempString);
  
    if (! no_frame_mode) {
    if (!(live_stream && (!logging))) {
	
	if (live_stream)
	    strcpy(temp_str, "Live Stream");
	else 
	    strcpy(temp_str, "Scan File");
	
	scanButton = XtVaCreateManagedWidget ("Scan File",
	    xmCascadeButtonWidgetClass, menuBar,
	    XmNmnemonic, 'F',
	    NULL);

	scanMenu = XmCreatePulldownMenu (menuBar, temp_str, NULL, 0);
	XtVaSetValues (scanButton, XmNsubMenuId, scanMenu, NULL);


	if (live_stream) {
	    FileLiveButton = XtVaCreateManagedWidget ("Switch to:", 
                xmCascadeButtonWidgetClass, scanMenu, NULL);  

	    file_real_Menu = XmCreatePulldownMenu (FileLiveButton, "file real menu", 
						   NULL, 0);
	    XtVaSetValues (FileLiveButton, XmNsubMenuId, file_real_Menu, NULL);
	
	    for (i=0;i<2;i++) {
		if (i==0)
		    strcpy(temp_str, "live stream");
		else
		    strcpy(temp_str, "scan from file");
	    
		file_real_val[i] = XtVaCreateManagedWidget (temp_str,
	            xmToggleButtonWidgetClass, file_real_Menu, 
	            XmNset, (i==getting_data_from) ? True : False,
	            NULL);

		XtAddCallback (file_real_val[i], XmNvalueChangedCallback, 
		       file_real_CB, (XtPointer)i);
	    }
	
	
	    NextFrameSep = XtVaCreateManagedWidget ("separator",
				 xmSeparatorGadgetClass, scanMenu, NULL);
	}

  
	tempString = XmStringCreateSimple ("Ctrl-n");
	strcpy(temp_str, "Next Major Frame");
	NextFrameAll = XtVaCreateManagedWidget (temp_str,
					    xmPushButtonWidgetClass, scanMenu,
					    XmNmnemonic, 'N',
					    XmNaccelerator, "Ctrl<Key>n",
					    XmNacceleratorText, tempString,
					    NULL);
	XmStringFree (tempString);
	
	XtAddCallback (NextFrameAll, XmNactivateCallback,
		       scan_file_next_CB, (XtPointer) (-1));
	
	
	NextFrameCpu = XtVaCreateManagedWidget ("Next Frame on Cpu",
			   xmCascadeButtonWidgetClass, scanMenu, NULL);
	
	NextFrameMenu = XmCreatePulldownMenu (NextFrameCpu, 
					       "Next Frame on Cpu", NULL, 0);
	
	XtVaSetValues (NextFrameCpu, XmNsubMenuId, NextFrameMenu, NULL);
	
	for (i=0; i < gr_numb_cpus; i++) {
	    ii = gr_cpu_map[i];
	    sprintf (temp_str, "cpu %d", ii);
	    
	    next_frame_menu[i] = XtVaCreateManagedWidget (temp_str, 
			 xmPushButtonWidgetClass, NextFrameMenu, NULL);
	    
	    XtAddCallback (next_frame_menu[i], 
			   XmNactivateCallback, 
			   next_cpu_frame_CB, (XtPointer)i);
	}
	
	
	
	PrevFrameSep = XtVaCreateManagedWidget ("separator",
				 xmSeparatorGadgetClass, scanMenu, NULL);
	
	
	
	tempString = XmStringCreateSimple ("Ctrl-p");
	strcpy(temp_str, "Previous Major Frame");
	PrevFrameAll = XtVaCreateManagedWidget (temp_str,
					    xmPushButtonWidgetClass, scanMenu,
					    XmNmnemonic, 'P',
					    XmNaccelerator, "Ctrl<Key>p",
					    XmNacceleratorText, tempString,
					    NULL);
	XmStringFree (tempString);
	
	XtAddCallback (PrevFrameAll, XmNactivateCallback,
		scan_file_prev_CB, (XtPointer) (-1));
	
	
	PrevFrameCpu = XtVaCreateManagedWidget ("Previous Frame on Cpu",
		           xmCascadeButtonWidgetClass, scanMenu, NULL);
	
	PrevFrameMenu = XmCreatePulldownMenu (PrevFrameCpu, 
					       "Previous Frame on Cpu", NULL, 0);
    
	XtVaSetValues (PrevFrameCpu, XmNsubMenuId, PrevFrameMenu, NULL);
	
	for (i=0; i < gr_numb_cpus; i++) {
	    ii = gr_cpu_map[i];
	    sprintf (temp_str, "cpu %d", ii);
	    
	    prev_frame_menu[i] = XtVaCreateManagedWidget (temp_str, 
				     xmPushButtonWidgetClass, PrevFrameMenu, NULL);

	    XtAddCallback (prev_frame_menu[i], 
			   XmNactivateCallback, 
			   prev_cpu_frame_CB, (XtPointer)i);
	}
	if (live_stream) {
	    for (i=0; i < gr_numb_cpus; i++) {
		XtUnmanageChild(next_frame_menu[i]);
	    }
	    XtUnmanageChild(NextFrameCpu);
	    XtUnmanageChild(NextFrameAll);
	    XtUnmanageChild(NextFrameSep);
	    for (i=0; i < gr_numb_cpus; i++) {
		XtUnmanageChild(prev_frame_menu[i]);
	    }
	    XtUnmanageChild(PrevFrameCpu);
	    XtUnmanageChild(PrevFrameAll);
	    XtUnmanageChild(PrevFrameSep);
	}
    }

    resetButton = XtVaCreateManagedWidget ("Averaging Factor",
	xmCascadeButtonWidgetClass, menuBar,
	XmNmnemonic, 'A',
	NULL);

    resetMenu = XmCreatePulldownMenu (menuBar, "Reset", NULL, 0);
    XtVaSetValues (resetButton, XmNsubMenuId, resetMenu, NULL);

    tempString = XmStringCreateSimple ("Ctrl-A");
    sprintf(temp_str, "Reset all to %d", DEFAULT_AVE_VAL);

    resetAll = XtVaCreateManagedWidget (temp_str,
	xmPushButtonWidgetClass, resetMenu,
        XmNmnemonic, 'A',
	XmNaccelerator, "Ctrl<Key>A",
	XmNacceleratorText, tempString,
	NULL);
    XmStringFree (tempString);

    XtAddCallback (resetAll, XmNactivateCallback,
		reset_proc_ave_CB, (XtPointer) (-1));

    XtVaCreateManagedWidget ("separator",
	xmSeparatorGadgetClass, resetMenu, NULL);

    for (i=0; i < gr_numb_cpus; i++) {
	ii = gr_cpu_map[i];
	sprintf (temp_str, "cpu %d", ii);

	average_button[i] = XtVaCreateManagedWidget (temp_str, 
	    xmCascadeButtonWidgetClass, resetMenu, NULL);

	average_menu[i] = XmCreatePulldownMenu (average_button[i], 
						temp_str, NULL, 0);
	XtVaSetValues (average_button[i], XmNsubMenuId, average_menu[i], NULL);

	proc_ave_last[i] = graph_info[ii].numb_to_ave;
	for (j=1; j <= MAX_NUMB_TO_AVE+1; j++) {
	    if (j==1) 
		sprintf (temp_str, "display last major frame");
	    else if (j==MAX_NUMB_TO_AVE+1) 
		sprintf (temp_str, "display comprehensive average");
	    else 
		sprintf (temp_str, "average of last %d major frames", j);

	    proc_ave_menu[i][j] = XtVaCreateManagedWidget (temp_str,
	        xmToggleButtonWidgetClass, average_menu[i], 
		XmNset, (j==graph_info[ii].numb_to_ave) ? True : False,
	        NULL);
	    XtAddCallback (proc_ave_menu[i][j], 
			   XmNvalueChangedCallback, 
			   proc_ave_CB, (XtPointer)(i*1000+j));
	}
    }
    }
    enableButton = XtVaCreateManagedWidget ("Display/Hide CPU",
	xmCascadeButtonWidgetClass, menuBar,
	XmNmnemonic, 'D',
	NULL);

    enableMenu = XmCreatePulldownMenu (menuBar, "Enable", args, n);
    XtVaSetValues (enableButton, XmNsubMenuId, enableMenu, NULL);


    tempString = XmStringCreateSimple ("Ctrl-D");
    enableAll = XtVaCreateManagedWidget ("Display All",
	xmPushButtonWidgetClass, enableMenu,
	XmNmnemonic, 'D',
	XmNaccelerator, "Ctrl<Key>D",
	XmNacceleratorText, tempString,
	NULL);
    XmStringFree (tempString);
    XtAddCallback (enableAll, XmNactivateCallback,
		enableCB, (XtPointer) (-1));

    XtVaCreateManagedWidget ("separator",
	xmSeparatorGadgetClass, enableMenu, NULL);

    for (i=0; i < gr_numb_cpus; i++) {
	ii = gr_cpu_map[i];
	sprintf (temp_str, "cpu %d", ii);

	enable[i] = XtVaCreateManagedWidget (temp_str,
	    xmToggleButtonWidgetClass, enableMenu,
	    XmNset, True,
	    NULL);
	cpuOn[i] = 1;
	XtAddCallback (enable[i], XmNvalueChangedCallback, 
			enableCB, (XtPointer) i);
    }

    graphButton = XtVaCreateManagedWidget ("Graphs",
	xmCascadeButtonWidgetClass, menuBar,
	XmNmnemonic, 'G',
	NULL);

    graphMenu = XmCreatePulldownMenu (menuBar, "Graph", NULL, 0);
    XtVaSetValues (graphButton, XmNsubMenuId, graphMenu, NULL);

    if (! no_frame_mode) {
        kern_button = XtVaCreateManagedWidget ("Start of Frame Overhead",
	    xmToggleButtonWidgetClass, graphMenu,
	    XmNset, False,
	    NULL);
        XtAddCallback (kern_button, XmNvalueChangedCallback, 
		       manage_kern_graph_CB, NULL);

        XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, graphMenu, NULL);

        proc_button = XtVaCreateManagedWidget ("Process Time per Major Frame ",
	    xmToggleButtonWidgetClass, graphMenu,
	    XmNset, False,	      
	    NULL);
        XtAddCallback (proc_button, XmNvalueChangedCallback, 
		       manage_proc_graph_CB, NULL);


        XtVaCreateManagedWidget ("separator",
	    xmSeparatorGadgetClass, graphMenu, NULL);

    }

    text_display_on = FALSE;
    ToggleTextMenu = XtVaCreateManagedWidget ("Text Display",
	    xmToggleButtonWidgetClass, graphMenu,
	    NULL);
    XtAddCallback (ToggleTextMenu, XmNvalueChangedCallback, toggle_text_disp_CB, NULL);

    XtVaCreateManagedWidget ("separator",
	xmSeparatorGadgetClass, graphMenu, NULL);

    if (kern_funct_mode) {
        kern_funct_button = XtVaCreateManagedWidget ("Display Kernel Function Calls",
	    xmToggleButtonWidgetClass, graphMenu,
    	    XmNset, False,	      
	    NULL);
	XtAddCallback (kern_funct_button, XmNvalueChangedCallback, 
		       manage_kern_funct_graph_CB, NULL);
    }

    labelButton = XtVaCreateManagedWidget ("Graph Labels",
	    xmCascadeButtonWidgetClass, menuBar,
	    XmNmnemonic, 'L',
	    NULL);

    labelMenu = XmCreatePulldownMenu (menuBar, "Label", NULL, 0);
    XtVaSetValues (labelButton, XmNsubMenuId, labelMenu, NULL);
    

    unitButton = XtVaCreateManagedWidget ("units", 
	xmCascadeButtonWidgetClass, labelMenu, 
        NULL);  

    unitMenu = XmCreatePulldownMenu (unitButton, "units", NULL, 0);
    XtVaSetValues (unitButton, XmNsubMenuId, unitMenu, NULL);

    for (i=0;i<2;i++) {
	if (i==0)
	    strcpy(temp_str, "usecs");
	else
	    strcpy(temp_str, "msecs");

	unitVal[i] = XtVaCreateManagedWidget (temp_str,
	    xmToggleButtonWidgetClass, unitMenu, 
	    XmNset, (i==0) ? True : False,
	    NULL);

	XtAddCallback (unitVal[i], XmNvalueChangedCallback, 
		       unit_CB, (XtPointer)i);
    }


    ticButton = XtVaCreateManagedWidget ("tics", 
	xmCascadeButtonWidgetClass, labelMenu, NULL);  

    ticMenu = XmCreatePulldownMenu (ticButton, "tics", NULL, 0);
    XtVaSetValues (ticButton, XmNsubMenuId, ticMenu, NULL);

    for (i=2;i<NUMB_TICS;i++) {
	sprintf (temp_str, "%d",i);


	ticVal[i] = XtVaCreateManagedWidget (temp_str,
	    xmToggleButtonWidgetClass, ticMenu, 
	    XmNset, (i==GraphA_tics+1) ? True : False,
	    NULL);

	XtAddCallback (ticVal[i], XmNvalueChangedCallback, 
		       tic_CB, (XtPointer)i);
    }


    divButton = XtVaCreateManagedWidget ("divs", 
	xmCascadeButtonWidgetClass, labelMenu, NULL);  

    divMenu = XmCreatePulldownMenu (divButton, "divs", NULL, 0);
    XtVaSetValues (divButton, XmNsubMenuId, divMenu, NULL);

    for (i=1;i<NUMB_DIVS;i++) {
	sprintf (temp_str, "%d",i);


	divVal[i] = XtVaCreateManagedWidget (temp_str,
	    xmToggleButtonWidgetClass, divMenu, 
	    XmNset, (i==GraphA_divisions) ? True : False,
	    NULL);

	XtAddCallback (divVal[i], XmNvalueChangedCallback, 
		       div_CB, (XtPointer)i);
    }

    zoomButton = XtVaCreateManagedWidget ("Zoom",
	xmCascadeButtonWidgetClass, menuBar,
	XmNmnemonic, 'Z',
	NULL);

    
    zoomMenu = XmCreatePulldownMenu (menuBar, "Label", NULL, 0);
    XtVaSetValues (zoomButton, XmNsubMenuId, zoomMenu, NULL);

    tempString = XmStringCreateSimple ("Ctrl-Z");
    zoom_control = XtVaCreateManagedWidget ("Specify Zoom",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>Z",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom_control, XmNactivateCallback, zoom_control_CB, 0);
    XmStringFree (tempString);

    

    XtVaCreateManagedWidget ("separator",
	xmSeparatorGadgetClass, zoomMenu, NULL);

    tempString = XmStringCreateSimple ("Ctrl-1");
    zoom[1] = XtVaCreateManagedWidget ("Zoom in 1.1x",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>1",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[1], XmNactivateCallback, zoom1_CB, 0);
    XmStringFree (tempString);

    tempString = XmStringCreateSimple ("Ctrl-2");
    zoom[2] = XtVaCreateManagedWidget ("Zoom in 1.5x",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>2",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[2], XmNactivateCallback, zoom2_CB, 0);
    XmStringFree (tempString);

    tempString = XmStringCreateSimple ("Ctrl-3");
    zoom[3] = XtVaCreateManagedWidget ("Zoom in 2x",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>3",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[3], XmNactivateCallback, zoom3_CB, 0);
    XmStringFree (tempString);

    tempString = XmStringCreateSimple ("Ctrl-4");
    zoom[4] = XtVaCreateManagedWidget ("Zoom in 4x",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>4",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[4], XmNactivateCallback, zoom4_CB, 0);
    XmStringFree (tempString);

    tempString = XmStringCreateSimple ("Ctrl-5");
    zoom[5] = XtVaCreateManagedWidget ("Zoom in Full",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>5",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[5], XmNactivateCallback, zoom5_CB, 0);
    XmStringFree (tempString);
  
    XtVaCreateManagedWidget ("separator",
	xmSeparatorGadgetClass, zoomMenu, NULL);

    tempString = XmStringCreateSimple ("Ctrl-6");
    zoom[6] = XtVaCreateManagedWidget ("Zoom out 1.1x",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>6",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[6], XmNactivateCallback, zoom6_CB, 0);
    XmStringFree (tempString);
  
    tempString = XmStringCreateSimple ("Ctrl-7");
    zoom[7] = XtVaCreateManagedWidget ("Zoom out 1.5x",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>7",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[7], XmNactivateCallback, zoom7_CB, 0);
    XmStringFree (tempString);
  
    tempString = XmStringCreateSimple ("Ctrl-8");
    zoom[8] = XtVaCreateManagedWidget ("Zoom out 2x",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>8",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[8], XmNactivateCallback, zoom8_CB, 0);
    XmStringFree (tempString);
  
    tempString = XmStringCreateSimple ("Ctrl-9");
    zoom[9] = XtVaCreateManagedWidget ("Zoom out 4x",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>9",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[9], XmNactivateCallback, zoom9_CB, 0);
    XmStringFree (tempString);
  
    tempString = XmStringCreateSimple ("Ctrl-0");
    zoom[0] = XtVaCreateManagedWidget ("Zoom out Full",
	xmPushButtonWidgetClass, zoomMenu,
	XmNaccelerator, "Ctrl<Key>0",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (zoom[0], XmNactivateCallback, zoom0_CB, 0);
    XmStringFree (tempString);
  

    scrollButton = XtVaCreateManagedWidget ("Scroll",
	xmCascadeButtonWidgetClass, menuBar,
	XmNmnemonic, 'S',
	NULL);

    
    scrollMenu = XmCreatePulldownMenu (menuBar, "Label", NULL, 0);
    XtVaSetValues (scrollButton, XmNsubMenuId, scrollMenu, NULL);


/* old scroll choices with 6 different scroll options 
*    scroll_accel[0] = XmStringCreateSimple ("Ctrl<Key>F");
*    tempString = XmStringCreateSimple ("Ctrl-F");
*    scroll[0] = XtVaCreateManagedWidget ("Scroll Left 1 Screen",
*	xmPushButtonWidgetClass, scrollMenu,
*	XmNaccelerator, scroll_accel[0],
*	XmNacceleratorText, tempString,
*	NULL);
*    XtAddCallback (scroll[0], XmNactivateCallback, scroll_l1_CB, 0);
*    XmStringFree (tempString);
*
*    scroll_accel[1] = XmStringCreateSimple ("Ctrl<Key>G");
*    tempString = XmStringCreateSimple ("Ctrl-G");
*    scroll[1] = XtVaCreateManagedWidget ("Scroll Left 1/2 Screen",
*	xmPushButtonWidgetClass, scrollMenu,
*	XmNaccelerator, scroll_accel[1],
*	XmNacceleratorText, tempString,
*	NULL);
*    XtAddCallback (scroll[1], XmNactivateCallback, scroll_l2_CB, 0);
*    XmStringFree (tempString);
*
*    scroll_accel[2] = XmStringCreateSimple ("Ctrl<Key>H");
*    tempString = XmStringCreateSimple ("Ctrl-H");
*    scroll[2] = XtVaCreateManagedWidget ("Scroll Left 1/10 Screen",
*	xmPushButtonWidgetClass, scrollMenu,
*	XmNaccelerator, scroll_accel[2],
*	XmNacceleratorText, tempString,
*	NULL);
*    XtAddCallback (scroll[2], XmNactivateCallback, scroll_l10_CB, 0);
*    XmStringFree (tempString);
*
*    scroll_accel[3] = XmStringCreateSimple ("Ctrl<Key>J");
*    tempString = XmStringCreateSimple ("Ctrl-J");
*    scroll[3] = XtVaCreateManagedWidget ("Scroll Right 1/10 Screen",
*	xmPushButtonWidgetClass, scrollMenu,
*	XmNaccelerator, scroll_accel[3],
*	XmNacceleratorText, tempString,
*	NULL);
*    XtAddCallback (scroll[3], XmNactivateCallback, scroll_r10_CB, 0);
*    XmStringFree (tempString);
*
*
*    scroll_accel[4] = XmStringCreateSimple ("Ctrl<Key>K");
*    tempString = XmStringCreateSimple ("Ctrl-K");
*    scroll[4] = XtVaCreateManagedWidget ("Scroll Right 1/2 Screen",
*	xmPushButtonWidgetClass, scrollMenu,
*	XmNaccelerator, scroll_accel[4],
*	XmNacceleratorText, tempString,
*	NULL);
*    XtAddCallback (scroll[4], XmNactivateCallback, scroll_r2_CB, 0);
*    XmStringFree (tempString);
*
*
*    scroll_accel[5] = XmStringCreateSimple ("Ctrl<Key>L");
*    tempString = XmStringCreateSimple ("Ctrl-L");
*    scroll[5] = XtVaCreateManagedWidget ("Scroll Right 1 Screen",
*	xmPushButtonWidgetClass, scrollMenu,
*	XmNaccelerator, scroll_accel[5],
*	XmNacceleratorText, tempString,
*	NULL);
*    XtAddCallback (scroll[5], XmNactivateCallback, scroll_r1_CB, 0);
*    XmStringFree (tempString);
* old scroll choices with 6 different scroll options */


    tempString = XmStringCreateSimple ("Ctrl-B");
    scroll[2] = XtVaCreateManagedWidget ("Scroll Back",
	xmPushButtonWidgetClass, scrollMenu,
	XmNaccelerator, "Ctrl<Key>B",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (scroll[2], XmNactivateCallback, scroll_l10_CB, 0);
    XmStringFree (tempString);

    tempString = XmStringCreateSimple ("Ctrl-F");
    scroll[3] = XtVaCreateManagedWidget ("Scroll Forward",
	xmPushButtonWidgetClass, scrollMenu,
	XmNaccelerator, "Ctrl<Key>F",
	XmNacceleratorText, tempString,
	NULL);
    XtAddCallback (scroll[3], XmNactivateCallback, scroll_r10_CB, 0);
    XmStringFree (tempString);

    return (menuBar);
} 




void
back_color_CB ()
{
    static Widget prefDialog, prefPane, actionArea, doneBtn, cancelBtn;
    static Widget backgndColor, redScale, greenScale, blueScale;
    static Widget colorLabel;
    
    XmString tmpString;

    if (!prefDialog) {
	/* create preferences popup form */
	prefDialog = XtVaCreateWidget ("prefDialog",
	    xmDialogShellWidgetClass, toplevel,
	    XmNdeleteResponse, XmDO_NOTHING,
	    XmNtitle, "rt monitor",
	    XmNx, 150,
	    XmNy, 150,
	    NULL);

	prefPane = XtVaCreateWidget ("prefPane",
	    xmPanedWindowWidgetClass, prefDialog,
	    XmNsashWidth, 1,
	    XmNsashHeight, 1,
	    NULL);

	backgndColor = XtVaCreateManagedWidget ("background color",
	    xmFormWidgetClass, prefPane,
	    NULL);

	colorLabel = XtVaCreateManagedWidget ("Background Colors",
	    xmLabelWidgetClass, backgndColor,
	    NULL);

	tmpString = XmStringCreateSimple ("red");
	redScale = XtVaCreateManagedWidget ("Red scale",
	    xmScaleWidgetClass, backgndColor,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, colorLabel,
	    XmNorientation, XmHORIZONTAL,
	    XmNtitleString, tmpString,
	    XmNshowValue, True,
	    XmNminimum, 0,
	    XmNmaximum, 255,
	    XmNvalue, bkgnd[0],
	    NULL);
	XmStringFree (tmpString);
	XtAddCallback (redScale, XmNdragCallback, changeColorCB, 
						(XtPointer)0);
	XtAddCallback (redScale, XmNvalueChangedCallback, changeColorCB, 
						(XtPointer)0);

	tmpString = XmStringCreateSimple ("green");
	greenScale = XtVaCreateManagedWidget ("Green scale",
	    xmScaleWidgetClass, backgndColor,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, redScale,
	    XmNorientation, XmHORIZONTAL,
	    XmNtitleString, tmpString,
	    XmNshowValue, True,
	    XmNvalue, bkgnd[1],
	    XmNminimum, 0,
	    XmNmaximum, 255,
	    NULL);
	tmpString = XmStringCreateSimple ("red");
	XtAddCallback (greenScale, XmNdragCallback, changeColorCB, 
						(XtPointer)1);
	XtAddCallback (greenScale, XmNvalueChangedCallback, changeColorCB, 
						(XtPointer)1);

	tmpString = XmStringCreateSimple ("blue");
	blueScale = XtVaCreateManagedWidget ("Blue scale",
	    xmScaleWidgetClass, backgndColor,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, greenScale,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNorientation, XmHORIZONTAL,
	    XmNtitleString, tmpString,
	    XmNshowValue, True,
	    XmNvalue, bkgnd[2],
	    XmNminimum, 0,
	    XmNmaximum, 255,
	    NULL);
	XmStringFree (tmpString);
	XtAddCallback (blueScale, XmNdragCallback, changeColorCB, 
						(XtPointer)2);
	XtAddCallback (blueScale, XmNvalueChangedCallback, changeColorCB, 
						(XtPointer)2);

	actionArea = XtVaCreateManagedWidget ("action_area",
	    xmFormWidgetClass, prefPane,
	    XmNfractionBase, 39,
	    XmNleftOffset,   10,
	    XmNrightOffset,  10,
	    NULL);

	doneBtn = XtVaCreateManagedWidget ("Done",
	    xmPushButtonWidgetClass, actionArea,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment,  XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 19,
	    XmNshowAsDefault, True,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XtAddCallback (doneBtn, XmNactivateCallback, prefCancelCB, 	
				(XtPointer)prefPane);

#if 0
	cancelBtn = XtVaCreateManagedWidget ("Cancel",
	    xmPushButtonWidgetClass, actionArea,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 20,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNdefaultButtonShadowThickness, 1,
	    NULL);
	XtAddCallback (cancelBtn, XmNactivateCallback, prefCancelCB, 	
				(XtPointer)prefPane);
#endif
    }

    XtManageChild (prefPane);
}


void
changeColorCB (Widget w, XtPointer clientD, XtPointer callD)
{
    XmScaleCallbackStruct *call_value = (XmScaleCallbackStruct *) callD;

    bkgnd[(int)clientD] = call_value->value;
/*    greenvect[(int)clientD] = (float)(call_value->value)/255;*/
}
void
prefCancelCB (Widget w, XtPointer clientD, XtPointer callD)
{
    XtUnmanageChild ( (Widget)clientD);
}


/*  A list of acceleraion keys

Alt-A Averaging Factor Menu
Alt-C Control Menu
Alt-D Display/Hide CPU Menu
Alt-F Scan File Menu
Alt-G Graphs Menu
Alt-L Graph Labels Menu
Alt-S Scroll Menu
Alt-Z Zoom Menu

Ctrl-1 through Ctrl-0 Zoom
Ctrl-A Reset All average factors to 1
Ctrl-D Display All CPUs
Ctrl-F Scroll Left 1/10 Screen
Ctrl-B Scroll Right 1/10 Screen
Ctrl-G Goto Frame Number
Ctrl-N Next Major Frame
Ctrl-P Previous Major Frame
Ctrl-Q Quit
Ctrl-R Reverse Search
Ctrl-S Forward Search
Ctrl-T Define Template
Ctrl-Z Zoom

*/


