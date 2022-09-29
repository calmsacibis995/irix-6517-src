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
 * File:	graph-raw-cb.c           			          *
 *									  *
 * Description:	This file handles the scrolling text for grtmon  	  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define GRMAIN extern
#include "graph.h"

void
draw_raw_graph()
{
    static int last_width=0, last_start=0;
    static int last_start_pos=0, last_end_pos=0;
    int i, j, cpu, diff, main_diff, start_pos, end_pos, length;
    char *search_pos, *text_str, *temp_str;
    char search_str[32];

    /* the string we will build for the text window */
    char temp_event_str[256];
    char concat_event_str[EVENT_LIST_SIZE];

  /* if we are in the middle of reading several cpus we don'r
       want to erase the list, else we do */
    if (erase_event_list)
	strcpy(event_list, "");

    if (no_frame_mode) {
	if ((text_display_on) &&
	    (!((last_width == nf_width_a) && (last_start == nf_start_val_a)))) {
		
	    strcpy(concat_event_str, "");

	    sprintf(temp_event_str, "No Frame Mode\n\n");

	    strcat(concat_event_str, temp_event_str);

	    /* for (i = gr_numb_cpus - 1; i >= 0; i--) {*/
	    for (i = 0; i < gr_numb_cpus; i++) {
		cpu = gr_cpu_map[i];
		if (cpuOn[i]) {   

		    sprintf(temp_event_str, "Processor %d events %d through %d\n",
			    cpu, graph_info[cpu].nf_start_event, 
			    graph_info[cpu].nf_end_event);
		    strcat(concat_event_str, temp_event_str);

		    sprintf(temp_event_str, "  Time     pid    event name          event id\n\n");
		    strcat(concat_event_str, temp_event_str);

		    for (j=graph_info[cpu].nf_start_event; 
			 j<=graph_info[cpu].nf_end_event; j++) {
			if (graph_info[cpu].no_frame_event[j].pid == -1) {
			    sprintf(temp_event_str, "  %-8lld        %-20s%d\n",
				    graph_info[cpu].no_frame_event[j].start_time,
				    tstamp_event_name[(int)(graph_info[cpu].no_frame_event[j].evt)],
				    (int)(graph_info[cpu].no_frame_event[j].evt));
			}
			else {
			    sprintf(temp_event_str, "  %-8lld %-5d  %-20s%d\n",
				    graph_info[cpu].no_frame_event[j].start_time,
				    graph_info[cpu].no_frame_event[j].pid,
				    tstamp_event_name[(int)(graph_info[cpu].no_frame_event[j].evt)],
				    (int)(graph_info[cpu].no_frame_event[j].evt));
			}
			strcat(concat_event_str, temp_event_str);
		    }
		}
	    }
	    XmTextSetString(TstampTextDisplay, concat_event_str);
	    last_width = nf_width_a;
	    last_start = nf_start_val_a;
	}
    }

    else {
	if (scan_type == FILE_DATA) {
	    if (strcmp(event_list, old_event_list) != 0) {
		if (ustestsema(event_list_sem) > 0) {
		    uspsema(event_list_sem);
		    if (text_display_on) {
			XmTextSetString(TstampTextDisplay, event_list);
			strcpy (old_event_list, event_list);
		    }
		    usvsema(event_list_sem);
		}
	    }
	}
    }

    if (current_search_on && current_search_found && text_display_on) {

	if (no_frame_mode) {
	    text_str = concat_event_str;
	    sprintf(search_str, "Processor %d", current_search_cpu);
	}
	else {
	    text_str = event_list;
	    sprintf(search_str, "cpu %d", current_search_cpu);
	}

	temp_str = strstr(text_str, search_str);

	main_diff = temp_str - text_str;

	if (temp_str != NULL)
	    text_str = temp_str;


	sprintf(search_str, "%lld", search_line);
	search_pos = strstr(text_str, search_str);


	/* this is ugly - turns out that through various math steps, search
	   line can be off 1 from what was written to the test display due
	   to round off error - so we'll try one on each side - oh well */
	if (search_pos == NULL) {
	    sprintf(search_str, "%lld", search_line-1);
	    search_pos = strstr(text_str, search_str);
	}

	if (search_pos == NULL) {
	    sprintf(search_str, "%lld", search_line+1);
	    search_pos = strstr(text_str, search_str);
	}

	if (search_pos != NULL) {
	    diff = search_pos - text_str;
	    
	    length = strlen(text_str);
	    
	    start_pos = diff;
	    while ((text_str[start_pos] != '\n') && (start_pos > 0)) {
		start_pos--;
	    }
	    start_pos++;
	    
	    end_pos = diff;
	    while ((text_str[end_pos] != '\n') && (end_pos < length)) {
		end_pos++;
	    }
	    
	    start_pos = start_pos + main_diff;
	    end_pos = end_pos + main_diff;

	    if (last_start_pos != start_pos) {
		
		XmTextSetHighlight(TstampTextDisplay, last_start_pos, last_end_pos, 
				   XmHIGHLIGHT_NORMAL);
		XmTextSetHighlight(TstampTextDisplay, start_pos, end_pos, 
				   XmHIGHLIGHT_SELECTED);
		
		XmTextShowPosition(TstampTextDisplay, end_pos);
		last_start_pos = start_pos;
		last_end_pos = end_pos;
	    }
	}
    }
    else {
	if (last_start_pos != 0) {
	    XmTextSetHighlight(TstampTextDisplay, last_start_pos, last_end_pos, 
			       XmHIGHLIGHT_NORMAL);

	    last_start_pos = 0;
	    last_end_pos = 0;
	}
    }
}

/*
good debuging code
			    sprintf(temp_event_str, "  %-8d        %-20s%d  st %d end %d nest %d\n",
				    graph_info[cpu].no_frame_event[j].start_time,
				    tstamp_event_name[graph_info[cpu].no_frame_event[j].evt],
				    graph_info[cpu].no_frame_event[j].evt,
				    graph_info[cpu].no_frame_event[j].type,
				    graph_info[cpu].no_frame_event[j].end_time,

			}
			else {
			    sprintf(temp_event_str, "  %-8d %-5d  %-20s%d  st %d end %d nest %d\n",
				    graph_info[cpu].no_frame_event[j].start_time,
				    graph_info[cpu].no_frame_event[j].pid,
				    tstamp_event_name[graph_info[cpu].no_frame_event[j].evt],
				    graph_info[cpu].no_frame_event[j].evt,
				    graph_info[cpu].no_frame_event[j].type,
				    graph_info[cpu].no_frame_event[j].end_time,


*/
