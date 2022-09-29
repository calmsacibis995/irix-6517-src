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
 * File:	file.c						          *
 *									  *
 * Description:	This file contains the routines that merge the kernel  	  *
 *              and user stream of time stamps.  It also contains some    *
 *              disk related functions for writing this to file           *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#define STATMAIN extern
#include "stats.h"

#define MAX_EVTS_INTR_AND_START 20
#define MAX_IO_EVS 10
#define SHORT_SIZE 2
#define INT_SIZE 4
#define LONG_LONG_SIZE 8

#define ADD_EOF_FOUND 0
#define ADD_EVT_ADDED 1
#define ADD_MAJ_FRAME 2
#define ADD_EVT_SKIPPED 3

void add_pid_name(int cpu, int tid, char *name);

void
sig_segv_handler(int sig, int code, struct sigcontext *sc)
{
    char msg_str[64];

    sprintf(msg_str, "in segv handler signal %d code %d",sig,code);
    grtmon_message(msg_str, DEBUG_MSG, 1);
}

#define CHECK_COLLECTING_NF_EVENTS  \
if (collecting_nf_events) {  \
    add_event_to_no_frame_list(index, cpu, mq);  \
    if ((mq[index-1].event == START_KERN_FUNCTION) || \
	(mq[index-1].event == END_KERN_FUNCTION)) \
	kern_funct_mode = TRUE; \
    if (! major_frame_account_valid) {  \
	count++;  \
	if (count > MAX_MAJ_FRAME_EVENTS) {  \
            grtmon_message("switching to no frame mode", ALERT_MSG, 0); \
		major_frame_account_valid = TRUE;  \
	    no_frame_mode = TRUE;  \
	}  \
	else {  \
	    if ((no_frame_mode) && (count > 2))  \
		major_frame_account_valid = TRUE;  \
	} \
    }  \
}

/* get_events will read events either from a file or from 
   the live stream and calls major frame stats every major
   frame it encounters */

long long get_time_val(merge_event_t *mq, int index, int last_index, 
		       int cpu, int frame_numb, uint lsb_time);

void
read_stream(void *cpu_val)
{
    int cpu, ret_val, loc_getting_data;
    int index, last_index;
    merge_event_t mq[MAX_MAJ_FRAME_EVENTS];
    char filename[32];
    int file_byte_offset;
    int last_maj_index;
    int count, i, cpu_count;
    int last_tstamp_ev_intrentry_off;
    long long last_tstamp_ev_intrentry_time;
    int add_frame;
    long long loc_start_time;
    char msg_str[64];
    int test_evt, test_revision;

    cpu = *(int *)cpu_val;

    verify_on[cpu] = TRUE;

    count = 0;
    atomicInc(&reading_count);
    /* we have to set up the signal */
/*    signal(SIGSEGV, sig_segv_handler);*/

    maj_frame_block[cpu] = ((maj_frame_block_t *) malloc (NUMB_MAJ_FRAMES_PER_BLOCK * sizeof(maj_frame_block_t)));
    maj_frame_block_allocs = 1;
    maj_frame_block[cpu][0].maj_frame_off = 0;
    maj_frame_block[cpu][0].time = 0;


    numb_maj_frames[cpu] = 0;
    maj_frame_numb[cpu] = 0;
    file_byte_offset = 0;

    index = 0;
    loc_getting_data = 1;

    /* okay let's determine whether it's the old style variable events
       or the new constant size, we'll see what the revision number is
       and then read in the rest of the event depending on the size
       as well as set the event_format variable */
/*    read(fd[cpu], &test_evt, INT_SIZE);
    read(fd[cpu], &test_revision, INT_SIZE);*/


    ret_val = ADD_EVT_SKIPPED;
    while (ret_val != ADD_EVT_ADDED) {
	ret_val = add_event_to_queue(fd[cpu], fd_fvw[cpu], index, last_index, mq, 
				     cpu, add_frame, &file_byte_offset, 
				     &last_tstamp_ev_intrentry_off, 
				     &last_tstamp_ev_intrentry_time, 
				     logging);
    }
    loc_start_time = mq[index].time;

    cpu_count = 0;
    for (i=0; i<MAX_PROCESSORS; i++) {
	if (cpu_on[i]) {
	    cpu_count++;
	}
    }

    /* we need to get mutex for the absolute start variable */
    ret_val = atomicInc(&absolute_start_access);
    while (ret_val != 0) {
	atomicDec(&absolute_start_access);
	sginap(0);
	ret_val = atomicInc(&absolute_start_access);
    }
    if (loc_start_time < absolute_start_time) {
	absolute_start_time = loc_start_time;
    }
    atomicDec(&absolute_start_access);
    atomicInc(&absolute_start_count);

    /* now wait for exeryone else to check their absolute start times */
    while (absolute_start_count != cpu_count)
	sginap(10);


    while ((loc_getting_data) && (glob_getting_data)) {
	last_index = index - 1;
	if (last_index<0) last_index = MAX_MAJ_FRAME_EVENTS - 1;
	add_frame = numb_maj_frames[cpu];
	if (add_frame < 0) 
	    add_frame = 0;
	ret_val = add_event_to_queue(fd[cpu], fd_fvw[cpu], index, last_index, mq, 
				     cpu, add_frame, &file_byte_offset, 
				     &last_tstamp_ev_intrentry_off, 
				     &last_tstamp_ev_intrentry_time, 
				     logging);

	switch(ret_val) {
	  case ADD_EOF_FOUND:
	    loc_getting_data = 0;
	    break;
	  case ADD_EVT_SKIPPED:
	    break;
	  case ADD_EVT_ADDED:
	    CHECK_COLLECTING_NF_EVENTS 
	    index = (index + 1) % MAX_MAJ_FRAME_EVENTS;
	    break;
	  case ADD_MAJ_FRAME:
	    count = 0;
	    CHECK_COLLECTING_NF_EVENTS 

	    /* We will start with the second full major frame since we
	       want to make sure we have tossed away the startup noise
	       as well as making sure that we have not started the
	       stream with a major frame event - this would prevent
	       us from looking back to the actual beginning of the
	       major frame which is the frame_trigger_event event */
	    if (numb_maj_frames[cpu] > 1) {
		if (!((live_stream) && (scan_type == FILE_DATA))) {
		    maj_frame_numb[cpu] = numb_maj_frames[cpu] - 1;
		}

		major_frame_stats(last_maj_index, index, cpu, 
				  numb_maj_frames[cpu] - 1, mq, 
				  MAX_MAJ_FRAME_EVENTS,
				  (!((live_stream) && (scan_type == FILE_DATA))));
	    }

	    if (numb_maj_frames[cpu] >= maj_frame_block_allocs *
		                       NUMB_MAJ_FRAMES_PER_BLOCK) {
		maj_frame_block_allocs++;

		maj_frame_block[cpu] = ((maj_frame_block_t *) 
					realloc ((void *) maj_frame_block[cpu],
						 (maj_frame_block_allocs *
						  NUMB_MAJ_FRAMES_PER_BLOCK *
						  sizeof(maj_frame_block_t))));
	    }

	    maj_frame_block[cpu][numb_maj_frames[cpu]].maj_frame_off = 
		last_tstamp_ev_intrentry_off;
	    maj_frame_block[cpu][numb_maj_frames[cpu]].nf_ex_ev_count = 
		graph_info[cpu].no_frame_event_count;
	    maj_frame_block[cpu][numb_maj_frames[cpu]].time = 
		last_tstamp_ev_intrentry_time;

	    numb_maj_frames[cpu]++;

	    last_maj_index = index;
	    index = (index + 1) % MAX_MAJ_FRAME_EVENTS;
	    break;
	  deafult:
	    grtmon_message("unexpected return from add_event_to_queue", ERROR_MSG, 3);
	    break;
	}
    }
#ifdef DEBUG
/*    fclose(fp[cpu]);*/
#endif

    sprintf(msg_str, "finished reading file for cpu %d\n",cpu); 
    grtmon_message(msg_str, NOTICE_MSG, 4);

    /* in case we finished reeading the whole file before the graphics
       had a chance to start this will at least display something reaonable
       however we can't do any grphics ourselves so we need to indicate to
       the graphics process to do soemting */
    atomicInc(&reading_count_done);

    atomicDec(&reading_count);



    kill(parent_id, SIGUSR1);
    exit(0);
}

void
clear_event_list()
{
    strcpy(event_list, "");
}

void
read_and_stat_maj_frame(int number, int cpu)
{
    int index, last_index, maj_start_index;
    int maj_count, event_list_size;
    merge_event_t mq[MAX_MAJ_FRAME_EVENTS];
    int extra1, extra2, extra3;
    int ret_val;
    long long start_time;
    int last_intrentry_length;
    /* the string we will build for the text window */
    char temp_event_str[128];
    char concat_event_str[EVENT_LIST_SIZE];


    /* if we are in the middle of reading several cpus we don't
       want to erase the list, else we do */
    if (erase_event_list)
	strcpy(event_list, "");

    strcpy(concat_event_str, "");


    index = 0;
    maj_count = 0;

    maj_frame_numb[cpu] = number;

    if (live_stream)
	lseek(fd_fvr[cpu], maj_frame_block[cpu][number].maj_frame_off, SEEK_SET);
    else
	lseek(fd[cpu], maj_frame_block[cpu][number].maj_frame_off, SEEK_SET);

    while (maj_count < 2) {
	last_index = index - 1;
	if (live_stream) {
	    ret_val = add_event_to_queue(fd_fvr[cpu], NULL, index, last_index, mq,
					 cpu, number, &extra1, &extra2, &extra3, FALSE);
	}
	else {
	    ret_val = add_event_to_queue(fd[cpu], NULL, index, last_index, mq,
					 cpu, number, &extra1, &extra2, &extra3, FALSE);
	}

	if (index == 0) {
	    start_time = mq[index].time;
	    sprintf(temp_event_str, "Major Frame %d for cpu %d time offset: %lld\n\n  time     pid    event               event number\n",
		    number, cpu,
		    TIC2USECL(mq[index].time - absolute_start_time));
	    strcat(concat_event_str, temp_event_str);
	}
	if (ret_val == ADD_EOF_FOUND) {
	  return;
	}
	else if (ret_val == ADD_EVT_SKIPPED) {
	  sprintf(temp_event_str, "                  %-20s%d\n",
		  tstamp_event_name[mq[index].event],mq[index].event);
	}
	else if (mq[index].pid == -1) {
	    sprintf(temp_event_str, "  %-8lld        %-20s%d\n",
		    TIC2USECL(mq[index].time - start_time),
		    tstamp_event_name[mq[index].event],mq[index].event);
	}
	else {
	    sprintf(temp_event_str, "  %-8lld %-5d  %-20s%d\n",
		    TIC2USECL(mq[index].time - start_time),
		    mq[index].pid, tstamp_event_name[mq[index].event],mq[index].event);
	}

	strcat(concat_event_str, temp_event_str);
	if (mq[index].event == frame_trigger_event)
	    last_intrentry_length = strlen(concat_event_str);


	switch(ret_val) {
	  case ADD_EOF_FOUND:
	    grtmon_message("unexpected end of file", ERROR_MSG, 5);
	    return;
	  case ADD_EVT_SKIPPED:
	    break;
	  case ADD_EVT_ADDED:
	    index = (index + 1) % MAX_MAJ_FRAME_EVENTS;
	    break;
	  case ADD_MAJ_FRAME:
	    if (maj_count == 0) 
		maj_start_index = index;
	    maj_count++;
	    index = (index + 1) % MAX_MAJ_FRAME_EVENTS;
	    break;
	  deafult:
	    grtmon_message("unexpected return from add_event_to_queue", ERROR_MSG, 6);
	    break;
	}
    }
    major_frame_stats(maj_start_index, index-1, cpu, number, mq, MAX_MAJ_FRAME_EVENTS, 1);
    
    event_list_size = strlen(event_list);
    if (event_list_size != 0) {
	temp_event_str[0] = '\n';
	temp_event_str[1] = '\n';
	strncat(event_list, temp_event_str, 2);
    }
    strncat(event_list, concat_event_str, last_intrentry_length);
    event_list[event_list_size + last_intrentry_length] = '\0';

}

#define COPY_QUALS for (j=0;j<6;j++) mq[index].quals[j] = (long long)quals[j];
#define COPY_LONG_QUALS for (j=0;j<6;j++) mq[index].quals[j] = long_quals[j];
int test[1000000];

int
add_event_to_queue(int my_fd, int fd_fv_loc, int index, int last_index, 
		   merge_event_t *mq, int cpu, int add_frame, int *offset,
		   int *last_tstamp_ev_intrentry_off, 
		   long long *last_tstamp_ev_intrentry_time, int loc_logging)
{
    short event_id;
    int revision;           /* WV_REV_ID | WV_EVT_PROTO_REV */
    int timestampFreq;      /* frequency of timer clock */
    int timestampPeriod;    /* maximum timer count before rollover */
    int autoRollover;       /* TRUE = target supplies EVENT_TIMER_ROLLOVER */
    int clkRate;            /* not used */
    int collectionMode;     /* RUE == context switch mode, otherwise, FALSE */
    int taskIdCurrent;      /* the currently running task */
    int CPU;                /* MIPS = 40, R3000 = 41, R4000 = 44 */
    int bspSize;            /* length of the following string */
    char bspName[128];      /* manufacturer of target computer */
    /* events for event_taskname */
    int status;
    int priority;
    int taskLockCount;
    int tid;
    int nameSize;
    char name[128];
    struct iovec ev_vec[MAX_IO_EVS];
    long long long_quals[6];
    uint quals[6];
    uint qual;
    int loc_proc;
    /*    event_t evt;*/
    unsigned short evt;
    int val1, val2;
    int i, numb_quals, spare, size, j;
    uint my_time;
    int proc;
    long long msb_time;
    int temp_size;
    char msg_str[64];

    if (read(my_fd,&evt,SHORT_SIZE) == 0) return(ADD_EOF_FOUND);

    if (loc_logging)
	write(fd_fv_loc, &evt, SHORT_SIZE);

    /* indicate null value - in case we don't fill it in we will know */
    mq[index].pid = -1;

    /* always fill in an event so we have something to report in the text 
       window even if we can't get a time or anything like that */
    mq[index].event = evt;

    if IS_USER_EVENT(evt) {
	

	ev_vec[0].iov_base = (caddr_t)&my_time; ev_vec[0].iov_len = INT_SIZE;
	ev_vec[1].iov_base = (caddr_t)&spare; ev_vec[1].iov_len = INT_SIZE;
	ev_vec[2].iov_base = (caddr_t)&size; ev_vec[2].iov_len = INT_SIZE;	
	if (readv(my_fd, ev_vec, 3) == 0) return(ADD_EOF_FOUND);
	if (loc_logging)
	    writev(fd_fv_loc, ev_vec, 3);

	/* 64 bits kernel events do not conform to user event specifications
	   so we will check and see if this event is one of those */
	if ((evt >= MIN_KERNEL_ID) && (size == 6*LONG_LONG_SIZE)) {

	    if (read(my_fd, long_quals, 6*LONG_LONG_SIZE) == 0)
		return(ADD_EOF_FOUND);
	    if (loc_logging)
		write(fd_fv_loc, long_quals, 6*LONG_LONG_SIZE);
	    
	    mq[index].event = evt;
	    if (evt >= MIN_KERNEL_ID) {
		mq[index].type = RTMON_KERNEL_EVENT;
	    }
	    else {
		mq[index].type = RTMON_USER_EVENT;
	    }

	    mq[index].time = get_time_val(mq, index, last_index, cpu, add_frame, my_time);
	    if (tstamp_adj_time == -1) {
		if (evt == TSTAMP_EV_YIELD2) {
		    tstamp_adj_time = TIC2USEC(mq[index].time - mq[last_index].time);
		}
	    }
	
	    *offset = *offset + SHORT_SIZE + 3*INT_SIZE + 6*LONG_LONG_SIZE;
	
	    COPY_LONG_QUALS;
	    if (evt == major_frame_start_event)
		return(ADD_MAJ_FRAME);
	    return(ADD_EVT_ADDED);
	}
	else {
	    if (read(my_fd, quals, size) == 0) 
		return(ADD_EOF_FOUND);
	    if (loc_logging)
		write(fd_fv_loc, quals, size);
	    
	    mq[index].event = evt;
	    if (evt >= MIN_KERNEL_ID) {
		mq[index].type = RTMON_KERNEL_EVENT;
	    }
	    else {
		mq[index].type = RTMON_USER_EVENT;
	    }
	    mq[index].time = get_time_val(mq, index, last_index, cpu, add_frame, my_time);
	    if (tstamp_adj_time == -1) {
		if (evt == TSTAMP_EV_YIELD2) {
		    tstamp_adj_time = TIC2USEC(mq[index].time - mq[last_index].time);
		}
	    }
	    
	    *offset = *offset + SHORT_SIZE + 3*INT_SIZE + size;
	    
	    COPY_QUALS;
	    if (evt == major_frame_start_event)
		return(ADD_MAJ_FRAME);
	    return(ADD_EVT_ADDED);
	}
    }
    else {
	switch(evt) {
	  case EVENT_CONFIG:
	    ev_vec[0].iov_base = (caddr_t)&revision; ev_vec[0].iov_len = INT_SIZE;
	    ev_vec[1].iov_base = (caddr_t)&timestampFreq; ev_vec[1].iov_len = INT_SIZE;
	    ev_vec[2].iov_base = (caddr_t)&timestampPeriod; ev_vec[2].iov_len = INT_SIZE;
	    ev_vec[3].iov_base = (caddr_t)&autoRollover; ev_vec[3].iov_len = INT_SIZE;
	    ev_vec[4].iov_base = (caddr_t)&clkRate; ev_vec[4].iov_len = INT_SIZE;
	    ev_vec[5].iov_base = (caddr_t)&collectionMode; ev_vec[5].iov_len = INT_SIZE;
	    ev_vec[6].iov_base = (caddr_t)&loc_proc; ev_vec[6].iov_len = INT_SIZE;
	    if (readv(my_fd, ev_vec, 7) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		writev(fd_fv_loc, ev_vec, 7);
	    *offset = *offset + SHORT_SIZE + 7*INT_SIZE;
	    /* careful tic period must be in usecs */
	    tic_period = 1000000.0/(float)timestampFreq;
	    /* use the below to hard code in 21 ns */
	    /*tic_period = 1000000.0/(float)47619048;*/
	    return(ADD_EVT_SKIPPED);
	  case EVENT_BUFFER:
	    if (read(my_fd, &taskIdCurrent, INT_SIZE) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		write(fd_fv_loc, &taskIdCurrent, INT_SIZE);
	    *offset = *offset + SHORT_SIZE + INT_SIZE;
	    return(ADD_EVT_SKIPPED);
	  case EVENT_TASKNAME:
	    ev_vec[0].iov_base = (caddr_t)&status; ev_vec[0].iov_len = INT_SIZE;
	    ev_vec[1].iov_base = (caddr_t)&priority; ev_vec[1].iov_len = INT_SIZE;
	    ev_vec[2].iov_base = (caddr_t)&taskLockCount; ev_vec[2].iov_len = INT_SIZE;
	    ev_vec[3].iov_base = (caddr_t)&tid; ev_vec[3].iov_len = INT_SIZE;
	    ev_vec[4].iov_base = (caddr_t)&nameSize; ev_vec[4].iov_len = INT_SIZE;
	    if (readv(my_fd, ev_vec, 5) == 0) return(ADD_EOF_FOUND);
	    temp_size = nameSize;
	    if (loc_logging)
		writev(fd_fv_loc, ev_vec, 5);
	    ev_vec[0].iov_base = (caddr_t)name; ev_vec[0].iov_len = nameSize*sizeof(char);
	    if (readv(my_fd, ev_vec, 1) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		writev(fd_fv_loc, ev_vec, 1);
	    *offset = *offset + SHORT_SIZE + 5*INT_SIZE + temp_size*sizeof(char);
	    name[nameSize] = '\0';
	    if (tid >=  KERN_FUNCT_ID_START) {
		strcpy(kern_funct_name_map[tid-KERN_FUNCT_ID_START], name);
	    }
	    else {
		add_pid_name(cpu, tid, name);
	    }
	    return(ADD_EVT_SKIPPED);
	  case EVENT_BEGIN:
	    ev_vec[0].iov_base = (caddr_t)&CPU; ev_vec[0].iov_len = INT_SIZE;
	    ev_vec[1].iov_base = (caddr_t)&bspSize; ev_vec[1].iov_len = INT_SIZE;
	    if (readv(my_fd, ev_vec, 2) == 0) return(ADD_EOF_FOUND);
	    temp_size = bspSize;
	    if (loc_logging)
		writev(fd_fv_loc, ev_vec, 2);
	    ev_vec[2].iov_base = (caddr_t)bspName; ev_vec[2].iov_len = bspSize*sizeof(char);
	    ev_vec[3].iov_base = (caddr_t)&taskIdCurrent; ev_vec[3].iov_len = INT_SIZE;
	    ev_vec[4].iov_base = (caddr_t)&collectionMode; ev_vec[4].iov_len = INT_SIZE;
	    ev_vec[5].iov_base = (caddr_t)&revision; ev_vec[5].iov_len = INT_SIZE;
	    if (readv(my_fd, ev_vec, 4) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		writev(fd_fv_loc, ev_vec, 4);
	    *offset = *offset + SHORT_SIZE + temp_size*sizeof(char) + 5*INT_SIZE;
	    return(ADD_EVT_SKIPPED);
	  case EVENT_TIMER_SYNC:
	    ev_vec[0].iov_base = (caddr_t)&my_time; ev_vec[0].iov_len = INT_SIZE;
	    ev_vec[1].iov_base = (caddr_t)&spare; ev_vec[1].iov_len = INT_SIZE;
	    if (readv(my_fd, ev_vec, 2) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		writev(fd_fv_loc, ev_vec, 2);
	    /* reset the time offset for the beginning of this major frame */
	    msb_time = spare;
	    maj_frame_block[cpu][add_frame].time = 
		    (msb_time<<32) + my_time;
	    *offset = *offset + SHORT_SIZE + 2*INT_SIZE;
	    return(ADD_EVT_SKIPPED);
	  case EVENT_TIMER_ROLLOVER:
	    if (read(my_fd,&my_time,INT_SIZE) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		write(fd_fv_loc, &my_time, INT_SIZE);
	    *offset = *offset + SHORT_SIZE + INT_SIZE;
	    return(ADD_EVT_SKIPPED);
	  case TSTAMP_EV_INTRENTRY:
	  case TSTAMP_EV_INTREXIT:
	  case TSTAMP_EV_EVINTRENTRY:  
	    /* we don't need the following case statement because it is
	     *  the same as the previous two and we get a compile error, if
	     *  it is changed so it is unique we should uncomment it
	     */
	    /*case TSTAMP_EV_EVINTREXIT:*/
	  case TSTAMP_EV_YIELD:
	  case TSTAMP_EV_RTCCOUNTER_INTR:
	  case TSTAMP_EV_CPUCOUNTER_INTR:
	  case TSTAMP_EV_PROFCOUNTER_INTR:
	  case TSTAMP_EV_GROUP_INTR:
	  case TSTAMP_EV_CPUINTR:
	  case TSTAMP_EV_NETINTR:
	  case EVENT_WIND_EXIT_IDLE:
	    if (read(my_fd,&qual,INT_SIZE) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		write(fd_fv_loc, &qual, INT_SIZE);
	    mq[index].event = evt;
	    mq[index].type = RTMON_KERNEL_EVENT;
	    mq[index].time = get_time_val(mq, index, last_index, cpu, add_frame, qual);
	    if (evt == TSTAMP_EV_INTRENTRY) {
		*last_tstamp_ev_intrentry_off = *offset;
		*last_tstamp_ev_intrentry_time = mq[index].time;
	    }
	    *offset = *offset + SHORT_SIZE + INT_SIZE;
	    COPY_QUALS

	    return(ADD_EVT_ADDED);
	  case TSTAMP_EV_SIGRECV: 
	  case TSTAMP_EV_SIGSEND: 
	  case TSTAMP_EV_EOSWITCH:
	  case TSTAMP_EV_EOSWITCH_RTPRI:
	  case EVENT_TASK_STATECHANGE:
	    if (read(my_fd, quals, 3*INT_SIZE) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		write(fd_fv_loc, quals, 3*INT_SIZE);
	    *offset = *offset + SHORT_SIZE + 3*INT_SIZE;
	    mq[index].event = evt;
	    mq[index].type = RTMON_KERNEL_EVENT;
	    mq[index].pid = quals[1];
	    mq[index].time = get_time_val(mq, index, last_index, cpu, add_frame, quals[0]);
	    COPY_QUALS
	    return(ADD_EVT_ADDED);
	  case TSTAMP_EV_EXIT:
	    if (read(my_fd,&qual,INT_SIZE) == 0) return(ADD_EOF_FOUND);
	    if (loc_logging)
		write(fd_fv_loc, &qual, INT_SIZE);
	    *offset = *offset + SHORT_SIZE + INT_SIZE;
	    COPY_QUALS
	    return(ADD_EVT_SKIPPED);
	  default:
	    if (no_frame_mode)
		sprintf(msg_str, "undefined event %d detected, cpu %d\n", evt, cpu);
	    else 
		sprintf(msg_str, "undefined event %d detected, cpu %d major frame %d\n", 
			evt, cpu, add_frame);
	    grtmon_message(msg_str, ERROR_MSG, 7);
sleep(5);
	    val1 = read(my_fd,test,4000000);
	    /*printf("val %d\n", val1);*/
	    close (fd_fv_loc);
kill(getpid(), SIGILL);
	    return(ADD_EVT_SKIPPED);
	}
    }
}

void
add_pid_name(int cpu, int tid, char *name)
{
    maj_pid_list_t *search_elem, *last_elem, *new_elem;
    char msg_str[64];

    if (strcmp(maj_pid_list[cpu].name, "idle") != 0) {
	/* initialize this maj pid list */
	maj_pid_list[cpu].pid = 0;
	strcpy(maj_pid_list[cpu].name, "idle");
	maj_pid_list[cpu].next = NULL;
    }
    search_elem = &maj_pid_list[cpu];

    while(search_elem != NULL) {
	if (search_elem->pid == tid)
	    return;
	last_elem = search_elem;
	search_elem = search_elem->next;
    }
    new_elem = ((maj_pid_list_t *) malloc(sizeof(maj_pid_list_t)));

    if (strlen(name) > MAX_PID_LENGTH) {
	name[MAX_PID_LENGTH] = '\0';
	sprintf(msg_str, "pid name too long, truncating to %s\n", name);
	grtmon_message(msg_str, ALERT_MSG, 8);
    }
    new_elem->pid = tid;
    strcpy(new_elem->name, name);
    new_elem->next = NULL;

    last_elem->next = new_elem;

}


long long
get_time_val(merge_event_t *mq, int index, int last_index, 
	     int cpu, int frame_numb, uint lsb_time)
{
    long long msb_time;

    if (index == 0) {
	if (collecting_nf_events && 
	     graph_info[cpu].no_frame_event_count < 10) {

	    msb_time = no_frame_event_start_time[cpu] >> 32;
	    return((msb_time<<32) + lsb_time);	
	}
	if (! collecting_nf_events && maj_frame_numb[cpu] == 0) {
	    /* this is the first time we are entering, we are assuming
	       there has already been a timer sync event so we will rely
	       of the that fact that we entered that time in the 
	       maj_frame_block data structure */
		
	    msb_time = maj_frame_block[cpu][0].time >> 32;
	    
	    return((msb_time<<32) + lsb_time);	
	}
    }
    if (last_index == -1) {
	/* this is the first event for a read from a random
	   major frame call */
	msb_time = maj_frame_block[cpu][frame_numb-1].time >> 32;
	
	return((msb_time<<32) + lsb_time);	
	
    }
    
    msb_time = mq[last_index].time >> 32;

    if (lsb_time < (uint)mq[last_index].time) {
	/* we observed a wrap */
	msb_time++;
    }
    return((msb_time<<32) + lsb_time);
}

