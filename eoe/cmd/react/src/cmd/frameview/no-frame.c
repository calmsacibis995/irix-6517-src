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
 * File:	no-frame.c					          *
 *									  *
 * Description:	This file contains the routines that handle      	  *
 *              gathering event in no_frame mode.                         *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#define GRMAIN extern
#include "graph.h"

int last_user_index[MAX_PROCESSORS], last_kernel_index[MAX_PROCESSORS];
int last_user_pid_map[MAX_PROCESSORS];
int last_user_pid[MAX_PROCESSORS];
int nf_ex_ev_add_state[MAX_PROCESSORS];
int nesting_count[MAX_PROCESSORS];

short no_frame_pid_map[MAX_PID];
int no_frame_pid_index;
static int frame_info_initialized = 0;
static int finished_init_frame_info = 0;

void add_event_to_no_frame_list(int index, int cpu, merge_event_t *mq);
void init_no_frame_info();

#define NF_EX_EV_ADD(add_type) \
graph_info[cpu].no_frame_event[loc_index].start_time = \
    (TIC2USECL(mq[index].time - absolute_start_time)); \
/* copy over event info */ \
graph_info[cpu].no_frame_event[loc_index].evt = mq[index].event; \
graph_info[cpu].no_frame_event[loc_index].pid = mq[index].pid; \
for (i=0; i<6; i++) \
    graph_info[cpu].no_frame_event[loc_index].quals[i] = mq[index].quals[i]; \
/* we don't know the end time yet so... */             \
graph_info[cpu].no_frame_event[loc_index].end_time = \
    graph_info[cpu].no_frame_event[loc_index].start_time; \
graph_info[cpu].no_frame_event[loc_index].type = add_type; \
;;graph_info[cpu].no_frame_event[loc_index].quals[5] = (long long)nesting_count[cpu];\
graph_info[cpu].no_frame_event[loc_index].disp = 0;


/* this macro takes care of events that occur at only one instant 
   in time, i.e., they do not span an interval */
#define NF_EX_EV_ADD_INSTANT(add_type) \
/* we need to fix last event'd end time */      \
graph_info[cpu].no_frame_event[loc_index].start_time = \
    (TIC2USECL(mq[index].time - absolute_start_time)); \
graph_info[cpu].no_frame_event[loc_index].end_time = \
    graph_info[cpu].no_frame_event[loc_index].start_time; \
/* copy over event info */ \
graph_info[cpu].no_frame_event[loc_index].evt = mq[index].event; \
graph_info[cpu].no_frame_event[loc_index].pid = mq[index].pid; \
for (i=0; i<6; i++) \
    graph_info[cpu].no_frame_event[loc_index].quals[i] = mq[index].quals[i]; \
graph_info[cpu].no_frame_event[loc_index].type = add_type;\
;;graph_info[cpu].no_frame_event[loc_index].quals[5] = (long long) nesting_count[cpu];\
graph_info[cpu].no_frame_event[loc_index].disp = 1;




/***************************************************************
  This function makes use of the explicit mode logic state machine from stats.c:

  the below logic/state machine that makes use of the three
  EX_EV_ADD macros is more or less mirrored in no-frame.c 
  as of aug 1995, if the below is modified you may want to 
  also consider changing the code in no-frmae.c
  the big difference is that in stats.c we are aware of
  what the next event is so we can operate on the current event
  while in no-frame.c we don't know what's in the future so
  we are always filling in information for events gone by,
  this means we have to keep track of a little more
  ***************************************************************/

void
add_event_to_no_frame_list(int index, int cpu, merge_event_t *mq)
{
    int loc_index;
    int temp_numb_events;
    int i, test;

    loc_index = graph_info[cpu].no_frame_event_count;

    if (loc_index == 0) {

	test = atomicInc(&frame_info_initialized);
	if (test ==0) {
	    init_no_frame_info();
	    absolute_start_time = absolute_start_time;
	}
	else 
	    /* wait until the other process has finishing initializing */
	    while (! finished_init_frame_info) ;

	no_frame_event_start_time[cpu] = mq[index].time;
	if (mq[index].time < absolute_start_time)
            absolute_start_time = mq[index].time;

	/* we've got to put something in event 0 or else the back
	   sets in the macro for end time will have problems
	   and this is eally the only place where i want to 
	   do the check for first event */
	graph_info[cpu].no_frame_event[loc_index].start_time = 
	    (TIC2USECL(mq[index].time - absolute_start_time)); 
	/* we don't know the end time yet so... */             
	graph_info[cpu].no_frame_event[loc_index].end_time = 
	    graph_info[cpu].no_frame_event[loc_index].start_time+1; 
	/* well not really but we'll just not use event 0 */
	graph_info[cpu].no_frame_event[loc_index].type = 0;

	graph_info[cpu].no_frame_event_count = graph_info[cpu].no_frame_event_count + 1;

	graph_info[cpu].nf_start_event = 0;
	graph_info[cpu].nf_end_event = 0;

	nf_width_a = 1.0;
	nf_max_val_a = 1;
	nf_start_val_a = 1;
	
	nesting_count[cpu] = 0;

	/* just indicate we've started up - graphics will not be spawned
	   until the below variable is set */
    }

    loc_index = graph_info[cpu].no_frame_event_count;

    if ((mq[index].event == TSTAMP_EV_EOSWITCH_RTPRI) ||
	(mq[index].event == TSTAMP_EV_EOSWITCH)) {

	if (no_frame_pid_map[mq[index].pid] == -1) {
	    /* remember we could have multiple processors accessing
	       this pid map */
	    no_frame_pid_map[mq[index].pid] = atomicInc(&no_frame_pid_index);
	}
	if (nf_ex_ev_add_state[cpu] == EX_EV_USER) {
	    graph_info[cpu].no_frame_event[last_user_index[cpu]].end_time = 
		(TIC2USECL(mq[index].time - absolute_start_time)); 
	    graph_info[cpu].no_frame_event[last_user_index[cpu]].disp = 1; 
	}
	else {
	    graph_info[cpu].no_frame_event[last_kernel_index[cpu]].end_time = 
		(TIC2USECL(mq[index].time - absolute_start_time)); 
	    graph_info[cpu].no_frame_event[last_kernel_index[cpu]].disp = 1;
	}

	nf_ex_ev_add_state[cpu] = EX_EV_USER;
	last_user_index[cpu] = loc_index;
	last_user_pid_map[cpu] = no_frame_pid_map[mq[index].pid];
	last_user_pid[cpu] = mq[index].pid;

	NF_EX_EV_ADD(no_frame_pid_map[mq[index].pid])
    }
    else if (mq[index].event == EVENT_WIND_EXIT_IDLE) {
	if (nf_ex_ev_add_state[cpu] == EX_EV_USER) {
	    graph_info[cpu].no_frame_event[last_user_index[cpu]].end_time = 
		(TIC2USECL(mq[index].time - absolute_start_time)); 
	    graph_info[cpu].no_frame_event[last_user_index[cpu]].disp = 1; 
	}
	nf_ex_ev_add_state[cpu] = EX_EV_USER;
	last_user_index[cpu] = loc_index;
	last_user_pid_map[cpu] = 0;
	last_user_pid[cpu] = 0;
	
	graph_info[cpu].no_frame_event[last_kernel_index[cpu]].end_time = 
	    (TIC2USECL(mq[index].time - absolute_start_time)); 
	graph_info[cpu].no_frame_event[last_kernel_index[cpu]].disp = 1; 

	NF_EX_EV_ADD(0)
    }
    else if ((IS_USER_EVENT(mq[index].event)) && (mq[index].event < MIN_KERNEL_ID)) {
	NF_EX_EV_ADD_INSTANT(EX_EV_USER)
    }
    else if ((mq[index].event == START_KERN_FUNCTION) ||
             (mq[index].event == END_KERN_FUNCTION)) {
	NF_EX_EV_ADD_INSTANT(EX_EV_KERN)
    }
    else if ((mq[index].event == TSTAMP_EV_INTRENTRY) ||
	     (mq[index].event == TSTAMP_EV_EVINTRENTRY) ||
	     (IS_INT_ENT_EVENT(mq[index].event))) {
	NF_EX_EV_ADD(EX_EV_KERN)
	nesting_count[cpu] = nesting_count[cpu]+ 1;
	if (nf_ex_ev_add_state[cpu] == EX_EV_USER) {
	    
	    nf_ex_ev_add_state[cpu] = EX_EV_KERN;
	    last_kernel_index[cpu] = loc_index;
	    
	    graph_info[cpu].no_frame_event[last_user_index[cpu]].end_time = 
		(TIC2USECL(mq[index].time - absolute_start_time)); 
	    graph_info[cpu].no_frame_event[last_user_index[cpu]].disp = 1; 

	    graph_info[cpu].no_frame_event_count = 
		graph_info[cpu].no_frame_event_count + 1;
	    loc_index =  graph_info[cpu].no_frame_event_count;
	}

	NF_EX_EV_ADD_INSTANT(EX_EV_INTR_ENTRY)
    }
    else if ((mq[index].event == TSTAMP_EV_INTREXIT) ||
	     (mq[index].event == TSTAMP_EV_EVINTREXIT)) {
	nesting_count[cpu] = nesting_count[cpu] - 1;
	if (nesting_count[cpu] == 0) {
	    /* we're leaving kernel */
	    nf_ex_ev_add_state[cpu] = EX_EV_USER;
	    
	    last_user_index[cpu] = loc_index;
	    
	    graph_info[cpu].no_frame_event[last_kernel_index[cpu]].end_time = 
		(TIC2USECL(mq[index].time - absolute_start_time)); 
	    graph_info[cpu].no_frame_event[last_kernel_index[cpu]].disp = 1; 
	    
	    NF_EX_EV_ADD(last_user_pid_map[cpu])
	    /* this is a special event that we're inserting so the
	       macro doesn't quite do the right thing */
	    graph_info[cpu].no_frame_event[loc_index].evt = MY_EVENT_EXIT_TO_USER; 
	    graph_info[cpu].no_frame_event[loc_index].pid = last_user_pid[cpu];

	    graph_info[cpu].no_frame_event_count = 
		graph_info[cpu].no_frame_event_count + 1;
	    loc_index =  graph_info[cpu].no_frame_event_count;

	}
	NF_EX_EV_ADD_INSTANT(EX_EV_INTR_EXIT)
    }
    else if (mq[index].event == EVENT_TASK_STATECHANGE){
	NF_EX_EV_ADD_INSTANT(EX_EV_STATECHANGE)
    }
    else {
	if (nf_ex_ev_add_state[cpu] == EX_EV_USER) {
	    
	    nf_ex_ev_add_state[cpu] = EX_EV_KERN;
	    last_kernel_index[cpu] = loc_index;
	    
	    graph_info[cpu].no_frame_event[last_user_index[cpu]].end_time = 
		(TIC2USECL(mq[index].time - absolute_start_time)); 
	    graph_info[cpu].no_frame_event[last_user_index[cpu]].disp = 1; 
	}



	NF_EX_EV_ADD(EX_EV_KERN)

	
    }

    graph_info[cpu].no_frame_event_count = graph_info[cpu].no_frame_event_count + 1;


}

void
init_no_frame_info()
{
    int i;

    no_frame_pid_index = 1;
    for (i=0; i<MAX_PID; i++) {
	no_frame_pid_map[i] = -1;
    }
    finished_init_frame_info = 1;
}
