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
 * File:	stats.h 						  *          
 *									  *
 * Description:	This file defines the structures needed by the            *
 *              statistics functions assocaited with the rtmon tools      *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#define MAIN extern
#include "frameview.h"

/* these needs to be removed and we need to declare this space dynamically */
#define MAX_PROC_NAME_SIZE 16
#define MAX_EXACT_EVENTS 10000

#define SPECIAL_MINOR_STRETCH -1  /* if this minor frame was stretched */
#define SPECIAL_MINOR_STOLEN_STRETCH -2 /* we were stolen from and are stretched */

#define SPECIAL_MINOR_INJECT -3  /* if the minor frame was extended */
#define SPECIAL_MINOR_STOLEN_INJECT -4 /* unneeded currently 8-28-95  */

#define SPECIAL_MINOR_STEAL -5 /* if this minor is going to steal */
#define SPECIAL_MINOR_STOLEN_STEAL -6  /* we were stolen from and are going to steal */

#define SPECIAL_MINOR_STOLEN -7 /* previous minor stole from me */

#define SPECIAL_MINOR_LOST -8 /* indicate that we lost some timestamp events */

/* this stays */
/* after this number we compute copmrehensive ave */
#define MAX_NUMB_TO_AVE 10
#define RTMON_MAX_KERN_STARTUP_EVENTS 32

typedef struct {
    int sum_val;
    int ex_ev_val;
    int sum_maj_frame;
    int ex_ev_maj_frame;
} max_struct_t;


typedef struct {
    char name[MAX_PROC_NAME_SIZE];
    int index_in_maj;
    int pid;
    int ktime;
    int utime;
    int max_time;  /* overloaded for process 0 this is a min */
    boolean show_time; 
} minor_process_t;
typedef minor_process_t ar_of_minor_process_t[MAX_NUMB_TO_AVE];

typedef struct {
    int numb_processes; 
    int start_time;
    /* the time and pid assocaited with each process this minor frame 
     there's one for each process in the minor, process[MAX_PROC_IN_MINOR]; */
    minor_process_t *process;

    /* the specific time associated with each of the kernel start
       up events in the order we expect to see them print error if we miss */
    int kern_ev_start_time[RTMON_MAX_KERN_STARTUP_EVENTS];
    int kern_ev_start_event[RTMON_MAX_KERN_STARTUP_EVENTS];
    int kern_ev_start_count;
/*    int ev_intrentry, ev_ccount, ev_evintrentry, ev_start;
    int ev_evintrexit, ev_intrexit, ev_eodisp, ev_eoswitch;*/
} minor_frame_t;

typedef minor_frame_t ar_of_minor_frame_t[MAX_NUMB_TO_AVE];

/* a list of the events occurring after startup events, consecutive
   kernel or user events tagged together */
typedef struct{
    /* negative events denote kernel happenings, positive events pid indices
       into major frame list */
    int count;
    int type[MAX_EXACT_EVENTS]; 
    int start_time[MAX_EXACT_EVENTS];
    int end_time[MAX_EXACT_EVENTS];
} exact_event_list_t;


/* this list will be mapped to an autogrow mmapped file and will contain
   all the events since we started observing them */
typedef struct{
    /* negative events denote kernel happenings, positive events pid indices
       into major frame list */
    int type;
    long long start_time;
    long long end_time;
    int pid;
    event_t evt;
    long long quals[6];
    int disp; /* idicate whether this event is one of the ones we
		 have stored display information in or whether it's
		 just so we have an accurate account for the raw
		 tstamp display */
    int matching_index;
} no_frame_event_list_t;



/* this is meant on a major frame by major frame basis, in general we keep
   a history into the past so that when I new entry comes in we delete the 
   old one and re figure average.  If the numb_to_ave is greater than
   MAX_NUMB_TO_AVE then we will computer comprehensive average */
typedef struct {
    /* two dimensional array for storing old minor frames
       minor[MAX_MINOR_FRAMES][MAX_NUMB_TO_AVE] */
    ar_of_minor_frame_t *minor;
    /*  ave_minor[MAX_MINOR_FRAMES] total_of_minors[MAX_MINOR_FRAMES]; */
    minor_frame_t *ave_minor;
    minor_frame_t *total_of_minors; /* a optimization so we don't have to
				       re-sum everytime, we'll subtract off
				       the old one and add on the new one */
    int numb_to_ave;  /* how many frames we'll use to produce ave */
    int numb_we_have; /* how many we collected towards this end */
    int ave_index;  /* circularly go through throwing out old ones */
    /* two dimensional array for storing olf major frame info 
       maj_process[MAX_PROC_IN_MAJOR][MAX_NUMB_TO_AVE] */
    ar_of_minor_process_t *maj_process;
                     /* the first index refers to the number of the process
			in this major frame, the second index to the past history
			this is different than the one in minors, in here we are
			going to sum all process time over the whole major frame, 
			i.e., sum pid across minor frames for a global picture */
    /* maj_ave_process[MAX_PROC_IN_MAJOR]  maj_total_process[MAX_PROC_IN_MAJOR] */
    minor_process_t *maj_ave_process;  /* each process in major frame */
    minor_process_t *maj_total_process;  /* each process in major frame */
    tag_and_time_t maj_absolute_start;
    int maj_numb_to_ave;
    int maj_numb_we_have;
    int maj_ave_index;
    int *actual_minor_time; /* the actual start time of a minor */
    int *special_minor; /* a code if this minor frame was a stretch, steal, grab */
    int *special_minor_time; /* computed time attributed to the above special minor */
    int special_frame; /* we'll use this as a flag to the graphics to indicate that
			 at least one minor on this cpu was special */
    max_struct_t *max_minor; /* the maximum value seen in a minor for graph display*/
    max_struct_t *max_kern_minor; /* the maximum value seen in a minor for graph display */
    int numb_minors; /* as a check */
    boolean ave_seen;  /* whether the user has requested this ave or not */
    boolean ave_valid;  /* true if we're not in the middle of calculating avergae */
    int ave_version; /* a number used to match against a user request, verifies
			we provide user with numbers from the same average */
    exact_event_list_t *exact_event;  /* list of events after startup */
    no_frame_event_list_t *no_frame_event;  /* list of events after startup */
    int no_frame_event_count;
    int nf_start_event, nf_end_event;  /* the events we are displaying from to */
} overall_graph_info_t;


STATMAIN overall_graph_info_t graph_info[MAX_PROCESSORS]; 


