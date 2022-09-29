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
 * File:	stats.c						          *
 *									  *
 * Description:	This file contains the routines that gather      	  *
 *              statistics for the real-time monitoring tools             *
 *              as well as some of the api library calls allowing         *
 *              users access to this data.                                *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#define STATMAIN
#include "stats.h"

#ifdef DEBUG 
extern FILE *fp[MAX_PROCESSORS]; 
#endif


/* local to major frame stats, but there's no way I'm doing a malloc
   every function call */
minor_frame_t *minor[MAX_PROCESSORS];
/* to keep track of cpu time across minor frames */
minor_process_t *maj_process[MAX_PROCESSORS]; 
boolean first_major_frame[MAX_PROCESSORS];
max_struct_t *max_minor[MAX_PROCESSORS];
max_struct_t *max_kern_minor[MAX_PROCESSORS];
int ex_ev_add_state[MAX_PROCESSORS];
int ex_ev_nesting_count[MAX_PROCESSORS];

/* a code if this minor frame was a stretch, steal, grab 
   for a steal instead of putting in a code we are going
   to put in the actual amount of time stolen since we'll
   need to know that in the next frame which may be across
   a major frame boundary */
int special_minor_state[MAX_PROCESSORS];

void
expand_stat_structure_numb_minors(int cpu, int numb_minors)
{
    int old_minor, i, j, k;

    old_minor = major_frame_account[cpu].numb_minors;

    if (old_minor >= numb_minors) {
      grtmon_message("internal alert please report number", ALERT_MSG, 69);
      return;
    }

    major_frame_account[cpu].numb_minors = numb_minors;

    minor[cpu] = ((minor_frame_t *) realloc((void *) minor[cpu], 
					    numb_minors*sizeof(minor_frame_t)));

    graph_info[cpu].actual_minor_time = 
	((int *) realloc((void *) graph_info[cpu].actual_minor_time,
			 numb_minors*sizeof(int)));

    major_frame_account[cpu].numb_in_minor = 
	((int *) realloc((void *) major_frame_account[cpu].numb_in_minor,
			numb_minors*sizeof(int)));


    major_frame_account[cpu].template_in_minor = 
	((int *) realloc((void *) major_frame_account[cpu].template_in_minor,
			numb_minors*sizeof(int)));


    for (i=old_minor; i<numb_minors; i++) {
	major_frame_account[cpu].numb_in_minor[i] = 1;
	major_frame_account[cpu].template_in_minor[i] = 1;
	minor[cpu][i].process = 
	    ((minor_process_t *) malloc(sizeof(minor_process_t)));
    }


    graph_info[cpu].ave_minor = 
	((minor_frame_t *) realloc((void *) graph_info[cpu].ave_minor,
				   numb_minors*sizeof(minor_frame_t)));


    graph_info[cpu].total_of_minors = 
	((minor_frame_t *) realloc((void *) graph_info[cpu].total_of_minors,
				   numb_minors*sizeof(minor_frame_t)));

    graph_info[cpu].special_minor = 
	((int *) realloc((void *) graph_info[cpu].special_minor,
			 numb_minors*sizeof(int)));

    graph_info[cpu].special_minor_time = 
	((int *) realloc((void *) graph_info[cpu].special_minor_time,
			 numb_minors*sizeof(int)));

    graph_info[cpu].max_minor = 
	((max_struct_t *) realloc((void *) graph_info[cpu].max_minor,
				  numb_minors*sizeof(max_struct_t)));

    max_minor[cpu] = 
	((max_struct_t *) realloc((void *) max_minor[cpu],
				  numb_minors*sizeof(max_struct_t)));

    graph_info[cpu].max_kern_minor = 
	((max_struct_t *) realloc((void *) graph_info[cpu].max_kern_minor,
				  numb_minors*sizeof(max_struct_t)));
    max_kern_minor[cpu] = 
	((max_struct_t *) realloc((void *) max_kern_minor[cpu],
				  numb_minors*sizeof(max_struct_t)));
    
    graph_info[cpu].exact_event = 
	((exact_event_list_t *) realloc((void *) graph_info[cpu].exact_event,
					numb_minors*sizeof(exact_event_list_t)));

    graph_info[cpu].minor = 
	((ar_of_minor_frame_t *) realloc((void *) graph_info[cpu].minor ,
					 MAX_NUMB_TO_AVE*numb_minors*sizeof(minor_frame_t)));


    /* now allocate space in each minor */
    for (i=old_minor; i<numb_minors; i++) {
	graph_info[cpu].ave_minor[i].process = 
	    ((minor_process_t *) malloc(sizeof(minor_process_t)));
	graph_info[cpu].total_of_minors[i].process = 
	    ((minor_process_t *) malloc(sizeof(minor_process_t)));
	
	for (j=0;j<MAX_NUMB_TO_AVE;j++) {
	    graph_info[cpu].minor[i][j].process = 
		((minor_process_t *) malloc(sizeof(minor_process_t)));
	}
	
	graph_info[cpu].ave_minor[i].numb_processes = 0;
	graph_info[cpu].ave_minor[i].start_time = 0;
	for (j=0;j<RTMON_MAX_KERN_STARTUP_EVENTS;j++) {
	    graph_info[cpu].ave_minor[i].kern_ev_start_time[j] = 0;
	    graph_info[cpu].ave_minor[i].kern_ev_start_event[j] = 0;
	}
	graph_info[cpu].ave_minor[i].kern_ev_start_count = 0;
	
	graph_info[cpu].total_of_minors[i].numb_processes = 0;
	graph_info[cpu].total_of_minors[i].start_time = 0;
	for (j=0;j<RTMON_MAX_KERN_STARTUP_EVENTS;j++) {
	    graph_info[cpu].total_of_minors[i].kern_ev_start_time[j] = 0;
	    graph_info[cpu].total_of_minors[i].kern_ev_start_event[j] = 0;
	}
	graph_info[cpu].total_of_minors[i].kern_ev_start_count = 0;
	
	graph_info[cpu].max_minor[i].sum_val = 0;
	graph_info[cpu].max_minor[i].ex_ev_val = 0;
	graph_info[cpu].max_kern_minor[i].sum_val = 0;
	graph_info[cpu].max_kern_minor[i].ex_ev_val = 0;
	
	max_minor[cpu][i].sum_val = 0;
	max_minor[cpu][i].ex_ev_val = 0;
	max_kern_minor[cpu][i].sum_val = 0;
	max_kern_minor[cpu][i].ex_ev_val = 0;
	
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    graph_info[cpu].minor[i][j].numb_processes = 0;
	    graph_info[cpu].minor[i][j].start_time = 0;
	    for (k=0;k<RTMON_MAX_KERN_STARTUP_EVENTS;k++) {
		graph_info[cpu].minor[i][j].kern_ev_start_time[k] = 0;
		graph_info[cpu].minor[i][j].kern_ev_start_event[k] = 0;
	    }
	    graph_info[cpu].minor[i][j].kern_ev_start_count = 0;
	    
	}

	for (j=0; j<major_frame_account[cpu].numb_in_minor[i]; j++) {
	    minor[cpu][i].process[j].pid = -1;
	    minor[cpu][i].process[j].ktime = 0;
	    minor[cpu][i].process[j].utime = 0;

	    strcpy(graph_info[cpu].ave_minor[i].process[j].name, "");
	    graph_info[cpu].ave_minor[i].process[j].pid = 0;
	    graph_info[cpu].ave_minor[i].process[j].ktime = 0;
	    graph_info[cpu].ave_minor[i].process[j].utime = 0;
	    graph_info[cpu].ave_minor[i].process[j].show_time = 0;
	    
	    strcpy(graph_info[cpu].total_of_minors[i].process[j].name, "");
	    graph_info[cpu].total_of_minors[i].process[j].pid = 0;
	    graph_info[cpu].total_of_minors[i].process[j].ktime = 0;
	    graph_info[cpu].total_of_minors[i].process[j].utime = 0;
	}

	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    for (k=0;k<major_frame_account[cpu].numb_in_minor[i]; k++) {
		strcpy (graph_info[cpu].minor[i][j].process[k].name,"");
		graph_info[cpu].minor[i][j].process[k].pid = 0;
		graph_info[cpu].minor[i][j].process[k].ktime = 0;
		graph_info[cpu].minor[i][j].process[k].utime = 0;
	    }
	}
    }
}

void
expand_stat_structure_numb_in_minor(int cpu, int minor_numb, 
				    int numb_in_minor)
{
    int old_proc, i, j;

    old_proc = major_frame_account[cpu].numb_in_minor[minor_numb];

    if (old_proc >= numb_in_minor) {
      grtmon_message("internal alert please report number", ALERT_MSG, 70);
      return;
    }

    major_frame_account[cpu].numb_in_minor[minor_numb] = numb_in_minor;

    minor[cpu][minor_numb].process = 
	((minor_process_t *) realloc((void *) minor[cpu][minor_numb].process,
				     numb_in_minor*sizeof(minor_process_t)));


    graph_info[cpu].ave_minor[minor_numb].process = 
	    ((minor_process_t *) realloc((void *) graph_info[cpu].ave_minor[minor_numb].process,
					 numb_in_minor*sizeof(minor_process_t)));

    graph_info[cpu].total_of_minors[minor_numb].process = 
	((minor_process_t *) realloc((void *) graph_info[cpu].total_of_minors[minor_numb].process,
				     numb_in_minor*sizeof(minor_process_t)));
	
    for (j=0;j<MAX_NUMB_TO_AVE;j++) {
	graph_info[cpu].minor[minor_numb][j].process = 
		((minor_process_t *) realloc((void *) graph_info[cpu].minor[minor_numb][j].process,
					     numb_in_minor*sizeof(minor_process_t)));
    }

    for (i=old_proc; i<numb_in_minor; i++) {

	minor[cpu][minor_numb].process[i].pid = -1;
	minor[cpu][minor_numb].process[i].ktime = 0;
	minor[cpu][minor_numb].process[i].utime = 0;


	strcpy(graph_info[cpu].ave_minor[minor_numb].process[i].name, "");
	graph_info[cpu].ave_minor[minor_numb].process[i].pid = 0;
	graph_info[cpu].ave_minor[minor_numb].process[i].ktime = 0;
	graph_info[cpu].ave_minor[minor_numb].process[i].utime = 0;
	graph_info[cpu].ave_minor[minor_numb].process[i].show_time = 0;
	
	strcpy(graph_info[cpu].total_of_minors[minor_numb].process[i].name, "");
	graph_info[cpu].total_of_minors[minor_numb].process[i].pid = 0;
	graph_info[cpu].total_of_minors[minor_numb].process[i].ktime = 0;
	graph_info[cpu].total_of_minors[minor_numb].process[i].utime = 0;
	
	
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    strcpy (graph_info[cpu].minor[minor_numb][j].process[i].name,"");
	graph_info[cpu].minor[minor_numb][j].process[i].pid = 0;
	    graph_info[cpu].minor[minor_numb][j].process[i].ktime = 0;
	    graph_info[cpu].minor[minor_numb][j].process[i].utime = 0;
	}
    }
}

void
expand_stat_structure_numb_processes(int cpu, int numb_processes)
{
    int old_procs, new_procs, i, j;

    old_procs = major_frame_account[cpu].numb_processes;

    if (old_procs >= numb_processes) {
      grtmon_message("internal alert please report number", ALERT_MSG, 71);
      return;
    }

    major_frame_account[cpu].numb_processes = numb_processes;

    maj_process[cpu] = 
	((minor_process_t *) realloc((void *) maj_process[cpu],
				     numb_processes*sizeof(minor_process_t)));

    graph_info[cpu].maj_process = 
	((ar_of_minor_process_t *) realloc((void *) graph_info[cpu].maj_process,
					   numb_processes*MAX_NUMB_TO_AVE*sizeof(minor_process_t)));

    graph_info[cpu].maj_ave_process = 
	((minor_process_t *) realloc((void *) graph_info[cpu].maj_ave_process,
				     numb_processes*sizeof(minor_process_t)));

    graph_info[cpu].maj_total_process = 
	((minor_process_t *) realloc((void *) graph_info[cpu].maj_total_process,
				     numb_processes*sizeof(minor_process_t)));


    for (i=old_procs;i<numb_processes; i++) {
	maj_process[cpu][i].pid = -1;
	maj_process[cpu][i].ktime = 0;
	maj_process[cpu][i].utime = 0;

	strcpy(graph_info[cpu].maj_ave_process[i].name, "");
	graph_info[cpu].maj_ave_process[i].pid = 0;
	graph_info[cpu].maj_ave_process[i].ktime = 0;
	graph_info[cpu].maj_ave_process[i].utime = 0;
	graph_info[cpu].maj_ave_process[i].max_time = 0;

	graph_info[cpu].maj_total_process[i].pid = 0;
	graph_info[cpu].maj_total_process[i].ktime = 0;
	graph_info[cpu].maj_total_process[i].utime = 0;
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    strcpy(graph_info[cpu].maj_process[i][j].name, "");
	    graph_info[cpu].maj_process[i][j].pid = 0;
	    graph_info[cpu].maj_process[i][j].ktime = 0;
	    graph_info[cpu].maj_process[i][j].utime = 0;	    
	}
    }
}

void
init_no_frame_event_list(int cpu)
{
    int no_frame_fd;
    char filename[128];
    void *no_frame_evt_ptr;
    char sys_str[64];
    char msg_str[64];


    if (getenv("TMPDIR") == NULL) {
	if ((no_frame_fd = open("/dev/zero", O_RDWR,0666)) == -1) {
	    sprintf (msg_str, "could not open %s errno %d", filename, errno);
	    grtmon_message(msg_str, ERROR_MSG, 72);
	    perror ("init_no_frame_event_list: open");
	    exit (-1);
	}
    }
    else {
	sprintf(filename, "%s%s_%d.%d", tmp_dir_str, NO_FRAME_FILENAME, my_extension, cpu);
	if ((no_frame_fd = open(filename, O_RDWR|O_CREAT,0666)) == -1) {
	    sprintf (msg_str, "could not open %s errno %d", filename, errno);
	    grtmon_message(msg_str, ERROR_MSG, 72);
	    perror ("init_no_frame_event_list: open");
	    exit (-1);
	}
	sprintf(sys_str, "chmod 666 %s", filename);
	system(sys_str);
    }
    

    no_frame_evt_ptr = mmap(0, 100000000, PROT_WRITE | PROT_READ,
			    MAP_SHARED | MAP_AUTORESRV | MAP_AUTOGROW, 
			    no_frame_fd, 0);

    if (no_frame_evt_ptr == (unsigned *)-1) {
	sprintf(msg_str, "mmap failed, errno %d\n",errno);
	grtmon_message(msg_str, ERROR_MSG, 73);
	perror ("mmap");
	exit(-1);
    }
		      
    graph_info[cpu].no_frame_event = (no_frame_event_list_t *)no_frame_evt_ptr;


    graph_info[cpu].no_frame_event_count = 0;
}


void
init_stat_structures(int cpu)
{
    int i,j,k;
    int space;

    for (i=0; i<MAX_PROCESSORS; i++) {
	special_minor_state[i] = 0;
    }

    major_frame_account[cpu].numb_minors = 1;
    major_frame_account[cpu].template_minors = 1;

    graph_info[cpu].ave_version = 0;

    space = major_frame_account[cpu].numb_minors;
    minor[cpu] = ((minor_frame_t *) malloc(space*sizeof(minor_frame_t)));

    major_frame_account[cpu].numb_in_minor = ((int *) malloc(space*sizeof(int)));
    major_frame_account[cpu].template_in_minor = ((int *) malloc(space*sizeof(int)));

    for (i=0;i<major_frame_account[cpu].numb_minors;i++) {
	major_frame_account[cpu].numb_in_minor[i] = 1;
	major_frame_account[cpu].template_in_minor[i] = 1;
	space = major_frame_account[cpu].numb_in_minor[i];
	minor[cpu][i].process = ((minor_process_t *) malloc(space*sizeof(minor_process_t)));
    }

    major_frame_account[cpu].numb_processes = 1;
    major_frame_account[cpu].template_processes = 1;

    space = major_frame_account[cpu].numb_processes;
    maj_process[cpu] = ((minor_process_t *) malloc(space*sizeof(minor_process_t)));
  
    /* first we'll allocate space for everything */
    space = major_frame_account[cpu].numb_minors * MAX_NUMB_TO_AVE;
    graph_info[cpu].minor = 
	((ar_of_minor_frame_t *) malloc(space*sizeof(minor_frame_t)));

    space = major_frame_account[cpu].numb_minors;
    graph_info[cpu].ave_minor = ((minor_frame_t *) malloc(space*sizeof(minor_frame_t)));
    graph_info[cpu].total_of_minors = ((minor_frame_t *) malloc(space*sizeof(minor_frame_t)));

    graph_info[cpu].actual_minor_time = ((int *) malloc(space*sizeof(int)));

    graph_info[cpu].special_minor = ((int *) malloc(space*sizeof(int)));
    graph_info[cpu].special_minor_time = ((int *) malloc(space*sizeof(int)));

    graph_info[cpu].max_minor = ((max_struct_t *) malloc(space*sizeof(max_struct_t)));
    max_minor[cpu] = ((max_struct_t *) malloc(space*sizeof(max_struct_t)));

    graph_info[cpu].max_kern_minor = ((max_struct_t *) malloc(space*sizeof(max_struct_t)));
    max_kern_minor[cpu] = ((max_struct_t *) malloc(space*sizeof(max_struct_t)));
    
    graph_info[cpu].exact_event = ((exact_event_list_t *) malloc(space*sizeof(exact_event_list_t)));

    /* now allocate space in each minor */
    for (i=0;i<major_frame_account[cpu].numb_minors;i++) {
	space = major_frame_account[cpu].numb_in_minor[i];
	graph_info[cpu].ave_minor[i].process = 
	    ((minor_process_t *) malloc(space*sizeof(minor_process_t)));
	graph_info[cpu].total_of_minors[i].process = 
	    ((minor_process_t *) malloc(space*sizeof(minor_process_t)));
	for (j=0;j<MAX_NUMB_TO_AVE;j++) {
	    space = major_frame_account[cpu].numb_in_minor[i];
	    graph_info[cpu].minor[i][j].process = 
		((minor_process_t *) malloc(space*sizeof(minor_process_t)));
	}
    }

    space = major_frame_account[cpu].numb_processes * MAX_NUMB_TO_AVE;
    graph_info[cpu].maj_process = 
	((ar_of_minor_process_t *) malloc(space*sizeof(minor_process_t)));

    space = major_frame_account[cpu].numb_processes;
    graph_info[cpu].maj_ave_process = ((minor_process_t *) malloc(space*sizeof(minor_process_t)));
    graph_info[cpu].maj_total_process = ((minor_process_t *) malloc(space*sizeof(minor_process_t)));

    /* the first time through a major frame we will initialize
       te ave data structre in graph info, so that we can verify
       all futures major frames look the same */
    first_major_frame[cpu] = TRUE;


    for (i=0; i<major_frame_account[cpu].numb_minors; i++) {
	graph_info[cpu].ave_minor[i].numb_processes = 0;
	graph_info[cpu].ave_minor[i].start_time = 0;
	for (j=0;j<RTMON_MAX_KERN_STARTUP_EVENTS;j++) {
	    graph_info[cpu].ave_minor[i].kern_ev_start_time[j] = 0;
	    graph_info[cpu].ave_minor[i].kern_ev_start_event[j] = 0;
	}
	graph_info[cpu].ave_minor[i].kern_ev_start_count = 0;
    
	graph_info[cpu].total_of_minors[i].numb_processes = 0;
	graph_info[cpu].total_of_minors[i].start_time = 0;
	for (j=0;j<RTMON_MAX_KERN_STARTUP_EVENTS;j++) {
	    graph_info[cpu].total_of_minors[i].kern_ev_start_time[j] = 0;
	    graph_info[cpu].total_of_minors[i].kern_ev_start_event[j] = 0;
	}
	graph_info[cpu].total_of_minors[i].kern_ev_start_count = 0;
	
	graph_info[cpu].max_minor[i].sum_val = 0;
	graph_info[cpu].max_minor[i].ex_ev_val = 0;
	graph_info[cpu].max_kern_minor[i].sum_val = 0;
	graph_info[cpu].max_kern_minor[i].ex_ev_val = 0;

	max_minor[cpu][i].sum_val = 0;
	max_minor[cpu][i].ex_ev_val = 0;
	max_kern_minor[cpu][i].sum_val = 0;
	max_kern_minor[cpu][i].ex_ev_val = 0;

	graph_info[cpu].numb_to_ave = 1;
	graph_info[cpu].numb_we_have = 0;
	graph_info[cpu].ave_index = 0;
	
	graph_info[cpu].maj_numb_to_ave = 1;
	graph_info[cpu].maj_numb_we_have = 0;
	graph_info[cpu].maj_ave_index = 0;
	
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    graph_info[cpu].minor[i][j].numb_processes = 0;
	    graph_info[cpu].minor[i][j].start_time = 0;
	    for (k=0;k<RTMON_MAX_KERN_STARTUP_EVENTS;k++) {
		graph_info[cpu].minor[i][j].kern_ev_start_time[k] = 0;
		graph_info[cpu].minor[i][j].kern_ev_start_event[k] = 0;
	    }
	    graph_info[cpu].minor[i][j].kern_ev_start_count = 0;

	}
	for (j=0; j<major_frame_account[cpu].numb_in_minor[i]; j++) {
	    strcpy(graph_info[cpu].ave_minor[i].process[j].name, "");
	    graph_info[cpu].ave_minor[i].process[j].pid = 0;
	    graph_info[cpu].ave_minor[i].process[j].ktime = 0;
	    graph_info[cpu].ave_minor[i].process[j].utime = 0;
	    graph_info[cpu].ave_minor[i].process[j].show_time = 0;
	    
	    strcpy(graph_info[cpu].total_of_minors[i].process[j].name, "");
	    graph_info[cpu].total_of_minors[i].process[j].pid = 0;
	    graph_info[cpu].total_of_minors[i].process[j].ktime = 0;
	    graph_info[cpu].total_of_minors[i].process[j].utime = 0;
	}
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    for (k=0;k<major_frame_account[cpu].numb_in_minor[i]; k++) {
		strcpy (graph_info[cpu].minor[i][j].process[k].name,"");
		graph_info[cpu].minor[i][j].process[k].pid = 0;
		graph_info[cpu].minor[i][j].process[k].ktime = 0;
		graph_info[cpu].minor[i][j].process[k].utime = 0;
	    }
	}
    }
    for (i=0;i<major_frame_account[cpu].numb_processes; i++) {
	strcpy(graph_info[cpu].maj_ave_process[i].name, "");
	graph_info[cpu].maj_ave_process[i].pid = 0;
	graph_info[cpu].maj_ave_process[i].ktime = 0;
	graph_info[cpu].maj_ave_process[i].utime = 0;
	graph_info[cpu].maj_ave_process[i].max_time = 0;
	graph_info[cpu].maj_ave_process[0].max_time = INF_TIME;

	graph_info[cpu].maj_total_process[i].pid = 0;
	graph_info[cpu].maj_total_process[i].ktime = 0;
	graph_info[cpu].maj_total_process[i].utime = 0;
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    strcpy(graph_info[cpu].maj_process[i][j].name, "");
	    graph_info[cpu].maj_process[i][j].pid = 0;
	    graph_info[cpu].maj_process[i][j].ktime = 0;
	    graph_info[cpu].maj_process[i][j].utime = 0;	    
	}
    }
}


#define	PRINT_IF_DBUG_INFO \
fprintf(fp[cpu], " for if index %d evt %d next_index %d evt %d adding %d\n", \
	 index, mq[index].event, next_index, mq[next_index].event, \
	 (TIC2USECL(mq[next_index].time - mq[index].time))); 

#define	PRINT_ELSE_DBUG_INFO \
fprintf(fp[cpu], "for else index %d evt %d next_index %d evt %d adding %d\n", \
	 index, mq[index].event, next_index, mq[next_index].event, \
	 (TIC2USECL(mq[next_index].time - mq[index].time)));


#define IF_ADJUST_MODE_ON_ADD \
if (adjust_mode == ON) { \
    maj_adj_count++; \
    min_adj_count++; \
}

#define IF_ADJUST_MODE_ON_SUB \
if (adjust_mode == ON) { \
    maj_adj_count--; \
    min_adj_count--; \
}

/* fill in the names of the processes */
#define NAME_MINOR_PROCESSES \
for (k=0; k<graph_info[cpu].ave_minor[i].numb_processes; k++) { \
    search_elem = &maj_pid_list[cpu]; \
    while((search_elem->next != NULL) &&  \
	  (search_elem->pid != minor[cpu][i].process[k].pid)) { \
	search_elem = search_elem->next; \
    } \
    if ((search_elem->next == NULL) &&  \
	(search_elem->pid != minor[cpu][i].process[k].pid)) { \
	/*we should not be here */ \
	strcpy(graph_info[cpu].ave_minor[i].process[k].name, "unknown"); \
    } \
    else { \
	strcpy(graph_info[cpu].ave_minor[i].process[k].name, \
	       search_elem->name); \
    } \
}

/* used when we receive a statechange vent since these are really
   only to windview - I decided not to yank them out of them stream
   at read in time in case we ever want to to something with them
   and so they will still appear in the text window */
#define PATCH_LAST_EVENT \
graph_info[cpu].exact_event[minor_frames].end_time[ex_ev_count-1] = \
    (TIC2USEC(mq[(index+1) % qsize].time - major_start)) - \
        (maj_adj_count * tstamp_adj_time); \
*patch_minor_time = *patch_minor_time + \
      (TIC2USEC(mq[next_index].time - mq[index].time)) - tstamp_adj_time; \
*patch_maj_time = *patch_maj_time + \
      (TIC2USEC(mq[next_index].time - mq[index].time)) - tstamp_adj_time;

#define EX_EV_ADD(add_type) \
graph_info[cpu].exact_event[minor_frames].start_time[ex_ev_count] = \
    (TIC2USEC(mq[index].time - major_start)) - \
        (maj_adj_count * tstamp_adj_time); \
graph_info[cpu].exact_event[minor_frames].end_time[ex_ev_count] = \
    (TIC2USEC(mq[(index+1) % qsize].time - major_start)) - \
        (maj_adj_count * tstamp_adj_time); \
graph_info[cpu].exact_event[minor_frames].type[ex_ev_count] = add_type; \
ex_ev_count++; \
if (ex_ev_count >= MAX_EXACT_EVENTS) { \
    grtmon_message("losing explicit events", WARNING_MSG, 74); \
    ex_ev_count--; \
}					

#define EX_EV_ADD_INSTANT(intr_type) \
graph_info[cpu].exact_event[minor_frames].start_time[ex_ev_count] = \
    (TIC2USEC(mq[index].time - major_start)) - \
        (maj_adj_count * tstamp_adj_time); \
graph_info[cpu].exact_event[minor_frames].type[ex_ev_count] = intr_type; \
ex_ev_count++; \
if (ex_ev_count >= MAX_EXACT_EVENTS) { \
    grtmon_message("losing explicit events", WARNING_MSG, 75); \
    ex_ev_count--; \
}					

#define ENTER_KERN_TIME \
patch_minor_time = &minor[cpu][minor_frames].process[proc_number].ktime; \
minor[cpu][minor_frames].process[proc_number].ktime = \
    minor[cpu][minor_frames].process[proc_number].ktime +  \
	(TIC2USEC(mq[next_index].time - mq[index].time)) - \
	    tstamp_adj_time; \
patch_maj_time = &maj_process[cpu][maj_proc_number].ktime; \
maj_process[cpu][maj_proc_number].ktime = \
    maj_process[cpu][maj_proc_number].ktime + \
	(TIC2USEC(mq[next_index].time - mq[index].time)) - \
	    tstamp_adj_time; 

#define ENTER_USER_TIME \
patch_minor_time = &minor[cpu][minor_frames].process[proc_number].utime; \
minor[cpu][minor_frames].process[proc_number].utime = \
    minor[cpu][minor_frames].process[proc_number].utime +  \
	(TIC2USEC(mq[next_index].time - mq[index].time)) - \
	    tstamp_adj_time; \
patch_maj_time = &maj_process[cpu][maj_proc_number].utime; \
maj_process[cpu][maj_proc_number].utime = \
    maj_process[cpu][maj_proc_number].utime + \
	(TIC2USEC(mq[next_index].time - mq[index].time)) - \
	    tstamp_adj_time;


/* a special note to whoever calls this - it is important to 
   realize that start_index and end_index are the major_frame
   events, but they are not the start and end of the major frame,
   instead it's the frame_trigger_event events that mark the actual
   start and end, therefore whoever calls this must make sure that
   these events are before the major frame event in the queue */

/* gather all the events occurring in a major frame and place them
   into containers based on what they are, we'll collect overall info
   that allows us to break the time down into the kernel, each processes'
   user time, and each processes kernel time, time assocaited with process
   id 0 will be consdiered slack, also do a specific breakdown of the
   events at the beginning of a minor frame, after we have gathered
   the stats for the whole frame we will update the averaging structures */
void
major_frame_stats(int start_index, int end_index, int cpu, int frame_numb,
		  merge_event_t *mq, int mq_entries, boolean write_graph_info)

{
    int index, prev_index;
    int search_index, search_count;
    maj_pid_list_t *search_elem;
    int adj_start_index;
    int begin_kern;
    int minor_frames;
    int numb_processes, proc_number;
    tag_and_time_t minor_start;
    tag_and_time_t major_start;
    int minor_end_index;
    int i,j,k,l;
    int qsize;  /* just a shorter the wrap value for the queue */
    int ave_ind, maj_ave_ind;
    int frame_starts_seen;
    int numb_maj_processes, maj_proc_number;
    int debugg_er1, debugg_er2;
    int ex_ev_count;
    int min_adj_count;  /* number of timestamps we need to subtract off */
    int maj_adj_count;  /* number of timestamps we need to subtract off */
    int delta_start_end;
    int current_pid;
    int last_current_pid;
    int next_index;
    int loc_tstamp_adj_time;
    int processed_special_minor;
    int *patch_minor_time, *patch_maj_time;

    qsize = mq_entries;

    if (start_index <= end_index) {
	delta_start_end = end_index - start_index;
    }
    else {
	delta_start_end = start_index + (mq_entries - end_index);
    }

    adj_start_index = -1;
    major_start = -1;

    minor_frames = 0;
    numb_maj_processes = 0;
    
    for (i=0; i<major_frame_account[cpu].numb_processes; i++) {
	maj_process[cpu][i].pid = -1;
	maj_process[cpu][i].ktime = 0;
	maj_process[cpu][i].utime = 0;
    }
    for (i=0;i<major_frame_account[cpu].numb_minors;i++) {
	for (j=0;j<major_frame_account[cpu].numb_in_minor[i];j++) {
	    minor[cpu][i].process[j].pid = -1;
	    minor[cpu][i].process[j].ktime = 0;
	    minor[cpu][i].process[j].utime = 0;
	}
    }


    index = start_index;

    if (graph_info[cpu].special_frame == 1) {
	/* this will indicate that the last frame was special, this
	   means we may still have to adjust the max_val_a */
	graph_info[cpu].special_frame = 2;  
    }
    else {
	graph_info[cpu].special_frame = 0;
    }
    maj_process[cpu][0].pid = 0;
    numb_maj_processes = 1;

    while(index != end_index) {  /* remember we're in a circular Q can't be < */

	/* do a check and see if we need to add an extra minor frame
	   before we start filling in data structures */

	if (minor_frames >= major_frame_account[cpu].numb_minors) {
	    expand_stat_structure_numb_minors(cpu, minor_frames+1);
	}

	if (write_graph_info) {  
	  /* clear minor special status, check though to see if the
	     previous minor frame stole time from us, if so indicate
	     by putting in the current minor frames structure */
	    if (special_minor_state[cpu] != 0) {
		if (special_minor_state[cpu] > 0) {
		    /* a steal occurred in the last minor */
		    special_minor_state[cpu] = 0;
		    graph_info[cpu].special_minor[minor_frames] = SPECIAL_MINOR_STOLEN;
		    graph_info[cpu].special_minor_time[minor_frames] = 
			special_minor_state[cpu];
		    graph_info[cpu].special_frame = 1;
		}
		else {
		  grtmon_message("internal warning please report number", WARNING_MSG, 76);
		}
	    }
	    else {
		graph_info[cpu].special_minor[minor_frames] = 0;
		processed_special_minor = 0;
	    }
	}
	if (index == (end_index-1)) {
	    grtmon_message("no events in frame", ALERT_MSG, 77);
	    goto end_major_frame;
	}

	/* we first have to back track to find the frame_trigger_event
	   event that triggered this major frame */
	
	search_index = index;
	search_count = 0;
	/* search back first */
	while((mq[search_index].event != frame_trigger_event)  &&
	      (search_count < 100) &&
	      (search_count <= (delta_start_end + 1))) {


	    search_count++;
	    search_index--;
	    if (search_index < 0)
		search_index = mq_entries-1;
	}
	if ((search_count >= 100) || (search_count > (delta_start_end))) {
	    grtmon_message("xno event frame_trigger_event for frame start", ALERT_MSG, 78);
	    goto end_major_frame;
	}
	/* ok right now we should be at the trigger event of a frame */
	
	/* set the adj_start_index - we will use this the
	   rest of this major frame */
	if (adj_start_index == -1)
	    adj_start_index = search_index;

	minor_start = mq[search_index].time;
	min_adj_count = 0;
	if (major_start == -1) {
	    major_start = mq[search_index].time;
	    maj_adj_count = 0;
	}
	loc_tstamp_adj_time = 0;
/* see TIMESTAMP_ADJ_COMMENT in frameview.h
	if (adjust_mode == ON) {
	    loc_tstamp_adj_time = tstamp_adj_time;
	}
	else {
	    loc_tstamp_adj_time = 0;
	}
*/

	/* now scan forward remembering events and times in the kernel
	   startup event structure
	   keep scanning until we find the EOSWITCH event that end
	   the kernel startup phase 
	   search_index points to the event after the EOSWITCH 
	   search_count value is important */
	search_count = 0;
	frame_starts_seen = 0;


	while ((search_index != end_index) && 
	       (search_count < RTMON_MAX_KERN_STARTUP_EVENTS)) {
	    if ((mq[search_index].event == minor_frame_start_event) ||
		(mq[search_index].event == major_frame_start_event)) {
		computed_minor_time = (int) mq[search_index].quals[0];
		if (frame_starts_seen == 1)
		    break; 
		frame_starts_seen++;
	    }


	    minor[cpu][minor_frames].kern_ev_start_time[search_count] =
		(TIC2USEC(mq[(search_index) % qsize].time - 
			 minor_start)) - (loc_tstamp_adj_time*(search_count+1));

/*	    minor[cpu][minor_frames].kern_ev_start_time[search_count] =
		(TIC2USECL(mq[(search_index+1) % qsize].time - 
			 mq[search_index].time)) - loc_tstamp_adj_time;
*/
	    /* see TIMESTAMP_ADJ_COMMENT in frameview.h
	    IF_ADJUST_MODE_ON_ADD
	    */
	    if (write_graph_info)
		graph_info[cpu].ave_minor[minor_frames].kern_ev_start_event[search_count] =
		    mq[search_index].event;
	
	    search_count++;
	    if (mq[search_index].event == TSTAMP_EV_EOSWITCH_RTPRI) {
		minor[cpu][minor_frames].kern_ev_start_time[search_count] =
		    (TIC2USEC(mq[(search_index) % qsize].time + tstamp_adj_time - 
			      minor_start)) - (loc_tstamp_adj_time*(search_count+1));
		break;
	    }
	    search_index = (search_index + 1) % qsize;
	}


	if ((mq[search_index].event == minor_frame_start_event) ||
	    (mq[search_index].event == major_frame_start_event)) {
	    /* ok now in the case there were no user events in this frame 
	       we have to do some checking and backtracking */
	    
	    /* we're going to go back to the dispatch of the idle loop
	       and then let the rest of the stats pick up from there */
	    while (mq[search_index].event != TSTAMP_EV_EODISP) {
		search_count--;
		search_index = (search_index - 1);
		if (search_index < 0)
		    search_index = qsize+search_index;
		/* see TIMESTAMP_ADJ_COMMENT in frameview.h
		IF_ADJUST_MODE_ON_SUB;
		*/
	    }
	}
	if (write_graph_info) {
	    graph_info[cpu].ave_minor[minor_frames].kern_ev_start_count =
		search_count;
	}

	/* we are pretending the last event in the kernel startup stuff
	   took only as long as a tstamp adj val - we have no way
	   of really knowing since the user ruins immediately after 
	   also a +1 was added to min_adj_count, so we subtract it
	   back off if we are adjusting for timestamps */
	max_kern_minor[cpu][minor_frames].ex_ev_val = tstamp_adj_time + 
	    (TIC2USEC(mq[search_index].time - minor_start)) -
		(loc_tstamp_adj_time*(min_adj_count+1));

	minor[cpu][minor_frames].start_time = 
	    max_kern_minor[cpu][minor_frames].ex_ev_val;


	
	/* consecutively go through all minor frames looking summing
	   event times associated with processes and divide into 
	   kernel or user time */


	/* this way we'll always get the idle loop first, it'll be
	   more predictable */
	minor[cpu][minor_frames].process[0].pid = 0;
	numb_processes = 1;


	ex_ev_count = 0;
	ex_ev_nesting_count[cpu] = 0;

	last_current_pid = -1;

	index = search_index;
	
	while ((mq[index].event != minor_frame_start_event) &&
		(mq[index].event != major_frame_start_event)) {

	    if (mq[index].event == TSTAMP_EV_EOSWITCH_RTPRI) {
		current_pid = mq[index].pid;
	    }

	    if (mq[index].event == TSTAMP_EV_LOST_TSTAMP) {
		if (write_graph_info) {
		    graph_info[cpu].special_frame = 1;
		    graph_info[cpu].special_minor[minor_frames] = 
			SPECIAL_MINOR_LOST;
		}
	    }

	    if (mq[index].event == TSTAMP_EV_RECV_EFRAME_STRETCH) {
		if (write_graph_info) {
		    graph_info[cpu].special_frame = 1;
		    if (processed_special_minor) {
			graph_info[cpu].special_minor_time[minor_frames] = 
			    graph_info[cpu].special_minor_time[minor_frames] +
				(int) mq[index].quals[2];
		    }
		    else {
			processed_special_minor = 1;
			if (special_minor_state[cpu] > 0) {
			    graph_info[cpu].special_minor[minor_frames] = 
				SPECIAL_MINOR_STOLEN_STRETCH;
			    graph_info[cpu].special_minor_time[minor_frames] = 
				(int) mq[index].quals[2] - special_minor_state[cpu];
			}
			else {
			    graph_info[cpu].special_minor[minor_frames] = 
				SPECIAL_MINOR_STRETCH;
			    graph_info[cpu].special_minor_time[minor_frames] = 
				(int) mq[index].quals[2];
			}
		    }

		}
	    }

	    if (mq[index].event == TSTAMP_EV_RECV_INJECTFRAME) {
		if (write_graph_info) {
		    graph_info[cpu].special_frame = 1;
		    if (processed_special_minor) {
			graph_info[cpu].special_minor_time[minor_frames] = 
			    graph_info[cpu].special_minor_time[minor_frames] +
				(int) mq[index].quals[2];
		    }
		    else {
			processed_special_minor = 1;
			if (special_minor_state[cpu] > 0) {
			    graph_info[cpu].special_minor[minor_frames] = 
				SPECIAL_MINOR_STOLEN_INJECT;
			    graph_info[cpu].special_minor_time[minor_frames] = 
				(int) mq[index].quals[2] - special_minor_state[cpu];
			}
			else {
			    graph_info[cpu].special_minor[minor_frames] = 
				SPECIAL_MINOR_INJECT;
			    graph_info[cpu].special_minor_time[minor_frames] = 
				(int) mq[index].quals[2];
			}
		    }
		}
	    }

	    if (mq[index].event == TSTAMP_EV_RECV_EFRAME_STEAL) {
		if (write_graph_info) {
		    graph_info[cpu].special_frame = 1;
		    if (processed_special_minor) {
			graph_info[cpu].special_minor_time[minor_frames] = 
			    graph_info[cpu].special_minor_time[minor_frames] +
				(int) mq[index].quals[2];
		    }
		    else {
			if (special_minor_state[cpu] > 0) {
			    graph_info[cpu].special_minor[minor_frames] = 
				SPECIAL_MINOR_STOLEN_STEAL;
			    graph_info[cpu].special_minor_time[minor_frames] = 
				(int) mq[index].quals[2] - special_minor_state[cpu];
			}
			else {
			    graph_info[cpu].special_minor[minor_frames] = 
				SPECIAL_MINOR_STEAL;
			    graph_info[cpu].special_minor_time[minor_frames] = 
				(int) mq[index].quals[2];
			}
		    }
		    /* indicate in a state variable to the next minor
		       on this cpu that this minor has stolen time from it */
		    special_minor_state[cpu] = 
			(int) graph_info[cpu].special_minor_time[minor_frames];

		}


	    }


	    if (mq[index].event == EVENT_WIND_EXIT_IDLE) {
		current_pid = 0;
	    }

	    if (current_pid != last_current_pid) {
		if (mq[index].pid != 0) 
		    minor_end_index = index;
		proc_number = -1;
		maj_proc_number = -1;

		/* search if this process has been included in minor frame struct */
		for (i=0; i<numb_processes; i++) {
		    if (minor[cpu][minor_frames].process[i].pid == current_pid) {
			/* found a match */
			proc_number = i;
			i = numb_processes;
		    }
		}
		/* search if this process has been included in major frame struct */
		for (i=0; i<numb_maj_processes; i++) {
		    if (maj_process[cpu][i].pid == current_pid) {
			/* found it */
			maj_proc_number = i;
			i = numb_maj_processes;
		    }
		}
 		if (maj_proc_number == -1) {
		    /* this is a new process that we haven't seen before in 
		       this major frame */
		    if (numb_maj_processes >= major_frame_account[cpu].numb_processes) {
			expand_stat_structure_numb_processes(cpu, numb_maj_processes+1);
		    }
		    maj_proc_number = numb_maj_processes;
		    maj_process[cpu][maj_proc_number].pid = current_pid;
		    numb_maj_processes++;
		}

		if (proc_number == -1) {
		    /* this is a new process that we haven't seen before in 
		       this minor frame */
		    if (numb_processes >= major_frame_account[cpu].numb_in_minor[minor_frames]) {
			expand_stat_structure_numb_in_minor(cpu, minor_frames, numb_processes+1);
		    }
		    proc_number = numb_processes;
		    minor[cpu][minor_frames].process[proc_number].pid = current_pid;
		    minor[cpu][minor_frames].process[proc_number].index_in_maj = maj_proc_number;
		    numb_processes++;
		}
		last_current_pid = current_pid;
	    }

	    /* here is where the meat is, at this point will we figure out
	       exactly how and where to account for the time of this event,
	       becuase of the flexibleness of the frame scheduler and
	       unpredictability of interrupts these accounting lines of
	       codes have several cases.  For each case the time is saved
	       in the minor frame structures, the major frame structures,
	       and the exact event list structure */

	    /***************************************************************
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

	    /* see TIMESTAMP_ADJ_COMMENT in frameview.h
	    IF_ADJUST_MODE_ON_ADD
	    */

	    next_index = (index + 1) % qsize;


	    if ((mq[index].event == TSTAMP_EV_EOSWITCH_RTPRI) ||
		(mq[index].event == TSTAMP_EV_EOSWITCH) ||
		(mq[index].event == EVENT_WIND_EXIT_IDLE)) {

		ex_ev_add_state[cpu] = EX_EV_USER;
		ENTER_USER_TIME
		EX_EV_ADD(maj_proc_number)
	    }
	    else if ((IS_USER_EVENT(mq[index].event)) && 
		     (mq[index].event < MIN_KERNEL_ID)) {
		EX_EV_ADD_INSTANT(EX_EV_USER)
	    }
  
	    else if ((mq[index].event == TSTAMP_EV_INTRENTRY) ||
		     (mq[index].event == TSTAMP_EV_EVINTRENTRY) ||
		     (IS_INT_ENT_EVENT(mq[index].event))) {
		ex_ev_nesting_count[cpu] = ex_ev_nesting_count[cpu]+ 1;
		ex_ev_add_state[cpu] = EX_EV_KERN;
		ENTER_KERN_TIME
		    EX_EV_ADD(EX_EV_KERN);
		EX_EV_ADD_INSTANT(EX_EV_INTR_ENTRY)
	    }
	    else if ((mq[index].event == TSTAMP_EV_INTREXIT) ||
		     (mq[index].event == TSTAMP_EV_EVINTREXIT)) {
		ex_ev_nesting_count[cpu] = ex_ev_nesting_count[cpu] - 1;
		
		if (ex_ev_nesting_count[cpu] == 0) {
		    /* we're leaving kernel */
		    ex_ev_add_state[cpu] = EX_EV_USER;
		    ENTER_USER_TIME;
		    EX_EV_ADD(maj_proc_number)
		}
		else {
		    /* still in the kernel */
		    ex_ev_add_state[cpu] = EX_EV_KERN;
		    ENTER_KERN_TIME;
		    EX_EV_ADD(EX_EV_KERN)
		}
		EX_EV_ADD_INSTANT(EX_EV_INTR_EXIT)
	    }
	    else if (mq[index].event == EVENT_TASK_STATECHANGE){
		/* do nothing we are going to ignore statechanges 
		   except we are going to have to fix the timings
		   of the last event */
		PATCH_LAST_EVENT;
	    }
	    
	    else {
		ex_ev_add_state[cpu] = EX_EV_KERN;
		EX_EV_ADD(EX_EV_KERN);
		ENTER_KERN_TIME
		    
	    }
	    
	    index = (index + 1) % qsize;
	}
	/* we're at the end of a frame, but we've included a few to
	   many events in time counting for pid 0, we need to move back
	   to when the frame started and subtract the differnce
	   off of pid 0 kernel time */
	search_index = index;
	search_count = 0;
	
	/* search back first */
	while((mq[search_index].event != frame_trigger_event) &&
	      (search_count < 100)) {
	    search_count++;
	    search_index--;
	    ex_ev_count--;

	    /* see TIMESTAMP_ADJ_COMMENT in frameview.h
	    IF_ADJUST_MODE_ON_SUB;
	    */

	    if (search_index < 0)
		search_index = mq_entries-1;
	}
	if (search_count >= 100) {
	    grtmon_message("no trigger event in back search", WARNING_MSG, 79);
	    goto end_major_frame;
	}

	/* subtract off the interrupt entry also because for the exact
	   events we don't want to see it */
	ex_ev_count--;
	if (write_graph_info)
	    graph_info[cpu].exact_event[minor_frames].count = ex_ev_count;
	
	minor[cpu][minor_frames].process[0].ktime =
	    minor[cpu][minor_frames].process[0].ktime - 
		(TIC2USEC(mq[index].time - mq[search_index].time)) -
		    (search_count * loc_tstamp_adj_time);

	if (write_graph_info) {
	    graph_info[cpu].actual_minor_time[minor_frames] = 
		(TIC2USEC(mq[search_index].time - mq[adj_start_index].time)) -
		    (min_adj_count * loc_tstamp_adj_time);
	  }

	max_minor[cpu][minor_frames].sum_val = 0;
	for (l=1; l<numb_processes; l++) {
	    max_minor[cpu][minor_frames].sum_val = 
		max_minor[cpu][minor_frames].sum_val + 
		    minor[cpu][minor_frames].process[l].utime + 
			minor[cpu][minor_frames].process[l].ktime;
	}
	max_minor[cpu][minor_frames].ex_ev_val = 
	    (TIC2USEC(mq[minor_end_index].time - minor_start)) -
		(min_adj_count * loc_tstamp_adj_time);


	minor[cpu][minor_frames].numb_processes = numb_processes;

	minor_frames++;
    }

    /* if this is the first major frame we have seen, set the
       major_frame_accounting information for the minor time
       each start of minor and start of major contains the time
       period for that frame so we'll get it from there */

    if (first_major_frame[cpu]) {
	major_frame_account[cpu].minor_time = computed_minor_time;
	major_frame_account_valid = TRUE;
    }

    /* okay we're done with this accounting all the time for this major
       frame now we need to update all the stats */

    /* first check to see that this major frame matches the 
       original template, or define that template if this is
       the first time through */



    for (i=0;i<minor_frames;i++) {
	if ((write_graph_info) &&
	    ((first_major_frame[cpu]) || (! template_matching))) {
	    /* only bother with this work if something has changed */
	    graph_info[cpu].ave_minor[i].numb_processes = 
		minor[cpu][i].numb_processes;
	    major_frame_account[cpu].template_in_minor[i] =
		minor[cpu][i].numb_processes;

	    NAME_MINOR_PROCESSES
	}

	for (j=0; j<minor[cpu][i].numb_processes; j++) {
	    if ((write_graph_info) &&
		((first_major_frame[cpu]) || (! template_matching))) {
		graph_info[cpu].ave_minor[i].process[j].pid = minor[cpu][i].process[j].pid;
		graph_info[cpu].ave_minor[i].process[j].index_in_maj = 
		    minor[cpu][i].process[j].index_in_maj;
	    }
	}
    }

    if (first_major_frame[cpu]) {
	major_frame_account[cpu].template_minors = minor_frames;
	major_frame_account[cpu].template_processes = numb_maj_processes;
    }
    else {
	if (template_matching) {
	    if ((major_frame_account[cpu].template_minors != minor_frames) ||
		(major_frame_account[cpu].template_processes != numb_maj_processes)) {
		    if (getpid() == graphics_proc_id) {
			wait_for_define_template(cpu);
			if (defining_template == -1) 
			    goto end_major_frame;
		    }
		    else {
			/* we can't have two processes writing graphics we'll
			   use the fix_template flag - see frameview.h */
			fix_template[cpu] = 1;
			while (fix_template[cpu] == 1) ;
			if (fix_template[cpu] == -1) 
			    goto end_major_frame;
		    }
	    }
	    for (i=0;i<minor_frames;i++) {
		if (major_frame_account[cpu].template_in_minor[i] != minor[cpu][i].numb_processes) {
		    if (getpid() == graphics_proc_id) {
			wait_for_define_template(cpu);
			if (defining_template == -1) 
			    goto end_major_frame;
		    }
		    else {
			/* we can't have two processes writing graphics we'll
			   use the fix_template flag - see frameview.h */
			fix_template[cpu] = 1;
			while (fix_template[cpu] == 1) ;
			if (fix_template[cpu] == -1) 
			    goto end_major_frame;
		    }
		}
	    }
	}
    }


    /* now do the same thing for the major process */
    for (i=0;i<numb_maj_processes;i++) {
	if ((write_graph_info) &&
	    ((first_major_frame[cpu]) || (! template_matching))) {
	    graph_info[cpu].maj_ave_process[i].pid = maj_process[cpu][i].pid;
	    
	    search_elem = &maj_pid_list[cpu];
	    while((search_elem->next != NULL) && 
		  (search_elem->pid != graph_info[cpu].maj_ave_process[i].pid))
		search_elem = search_elem->next;
	    if ((search_elem->next == NULL) && 
		(search_elem->pid !=  graph_info[cpu].maj_ave_process[i].pid)) {
		; /*we should not be here */
	    }
	    else 
		strcpy(graph_info[cpu].maj_ave_process[i].name,
		       search_elem->name);
	}
	else {
	    if (graph_info[cpu].maj_ave_process[i].pid != maj_process[cpu][i].pid) {
		grtmon_message("cpu mismatched major pid", ALERT_MSG, 80);
		debugg_er1 = 3;
		debugg_er2 = maj_process[cpu][i].pid;
		goto end_major_frame;
	    }
	}
    }


    /* if we are here then this major frame matches the template,
       first subtract off the one from the totals we don't need */

    uspsema(ave_sem[cpu]);

    if (! write_graph_info)
	goto just_do_maxes;

    ave_ind = graph_info[cpu].ave_index;
    if (graph_info[cpu].numb_to_ave <= MAX_NUMB_TO_AVE) {  
	/* if we are NOT doing comprehensive averaging, i.e., time limited */
	for (i=0;i<minor_frames;i++) {
	    graph_info[cpu].total_of_minors[i].start_time = 
		graph_info[cpu].total_of_minors[i].start_time -
		    graph_info[cpu].minor[i][ave_ind].start_time;
	    for (j=0;j<graph_info[cpu].minor[i][ave_ind].numb_processes;j++) {
		graph_info[cpu].total_of_minors[i].process[j].ktime = 
		    graph_info[cpu].total_of_minors[i].process[j].ktime -
			graph_info[cpu].minor[i][ave_ind].process[j].ktime;
		graph_info[cpu].total_of_minors[i].process[j].utime = 
		    graph_info[cpu].total_of_minors[i].process[j].utime -
			graph_info[cpu].minor[i][ave_ind].process[j].utime;
	    }
	    for (j=0; j<graph_info[cpu].ave_minor[i].kern_ev_start_count; j++ ) {
		graph_info[cpu].total_of_minors[i].kern_ev_start_time[j] = 
		    graph_info[cpu].total_of_minors[i].kern_ev_start_time[j] -
			graph_info[cpu].minor[i][ave_ind].kern_ev_start_time[j];
	    }
	}
    }

    /* first subtract off the one from the major totals we don't need */
    maj_ave_ind = graph_info[cpu].maj_ave_index;

    if (graph_info[cpu].maj_numb_to_ave <= MAX_NUMB_TO_AVE) {  
	/* if we are NOT doing comprehensive averaging, i.e., time limited */
	for (i=0;i<numb_maj_processes;i++) {
	    graph_info[cpu].maj_total_process[i].ktime = 
		graph_info[cpu].maj_total_process[i].ktime -
		    graph_info[cpu].maj_process[i][maj_ave_ind].ktime;
	    graph_info[cpu].maj_total_process[i].utime = 
		graph_info[cpu].maj_total_process[i].utime -
		    graph_info[cpu].maj_process[i][maj_ave_ind].utime;
	}
    }


    /* next copy over this major frame into the minor index spots */
    for (i=0;i<minor_frames;i++) {
	graph_info[cpu].minor[i][ave_ind].numb_processes = 
	    minor[cpu][i].numb_processes;
	graph_info[cpu].minor[i][ave_ind].start_time = minor[cpu][i].start_time;
	for (j=0; j<minor[cpu][i].numb_processes; j++) {
	    graph_info[cpu].minor[i][ave_ind].process[j].ktime = 
		minor[cpu][i].process[j].ktime;
	    graph_info[cpu].minor[i][ave_ind].process[j].utime = 
		minor[cpu][i].process[j].utime;
	}
	for (j=0; j<graph_info[cpu].ave_minor[i].kern_ev_start_count; j++ ) {
	    graph_info[cpu].minor[i][ave_ind].kern_ev_start_time[j] =
		minor[cpu][i].kern_ev_start_time[j];
	}
    }

    /* now do the same thing for the major process totals */
    for (i=0;i<numb_maj_processes;i++) {
	graph_info[cpu].maj_process[i][maj_ave_ind].ktime 
	    = maj_process[cpu][i].ktime;
	graph_info[cpu].maj_process[i][maj_ave_ind].utime 
	    = maj_process[cpu][i].utime;
	/* check for maxes and mins */
	if (i==0) {  /* overloaded for process 0 its a min */
	    if ((maj_process[cpu][i].utime + maj_process[cpu][i].ktime) <
		graph_info[cpu].maj_ave_process[i].max_time) {
		graph_info[cpu].maj_ave_process[i].max_time = 
		    maj_process[cpu][i].utime + maj_process[cpu][i].ktime;
	    }
	}
	else {
	    if ((maj_process[cpu][i].utime + maj_process[cpu][i].ktime) >
		graph_info[cpu].maj_ave_process[i].max_time) {
		graph_info[cpu].maj_ave_process[i].max_time = 
		    maj_process[cpu][i].utime + maj_process[cpu][i].ktime;
	    }
	}
    }

    first_major_frame[cpu] = FALSE;

    /* now add new entry to totals */
	    
    for (i=0;i<minor_frames;i++) {
	graph_info[cpu].total_of_minors[i].start_time = 
	    graph_info[cpu].total_of_minors[i].start_time +
		graph_info[cpu].minor[i][ave_ind].start_time;
	for (j=0;j<graph_info[cpu].minor[i][ave_ind].numb_processes;j++) {
	    graph_info[cpu].total_of_minors[i].process[j].ktime = 
		graph_info[cpu].total_of_minors[i].process[j].ktime +
		    graph_info[cpu].minor[i][ave_ind].process[j].ktime;
	    graph_info[cpu].total_of_minors[i].process[j].utime = 
		graph_info[cpu].total_of_minors[i].process[j].utime +
		    graph_info[cpu].minor[i][ave_ind].process[j].utime;
	}
	
	for (j=0; j<graph_info[cpu].ave_minor[i].kern_ev_start_count; j++ ) {
	    graph_info[cpu].total_of_minors[i].kern_ev_start_time[j] = 
		graph_info[cpu].total_of_minors[i].kern_ev_start_time[j] +
		    graph_info[cpu].minor[i][ave_ind].kern_ev_start_time[j];
	}
    }

    /* add entry to major totals */
    for (i=0;i<numb_maj_processes;i++) {
	graph_info[cpu].maj_total_process[i].ktime =
	    graph_info[cpu].maj_total_process[i].ktime +
		graph_info[cpu].maj_process[i][maj_ave_ind].ktime;
	graph_info[cpu].maj_total_process[i].utime =
	    graph_info[cpu].maj_total_process[i].utime +
		graph_info[cpu].maj_process[i][maj_ave_ind].utime;
    }

    /* figure out what to divide by for average */
    if (graph_info[cpu].numb_to_ave > MAX_NUMB_TO_AVE) {  /* comprehensive ave */
	graph_info[cpu].numb_we_have++;
    }
    else { /* limited time average */
	/* warning: yes this looks backwards but is not, we are not
	   calculating an index here, we are calculating a number */
	graph_info[cpu].numb_we_have++;
	if (graph_info[cpu].numb_we_have > graph_info[cpu].numb_to_ave) 
	    graph_info[cpu].numb_we_have = graph_info[cpu].numb_to_ave;
    }
    /* now do for major process ave */
    if (graph_info[cpu].maj_numb_to_ave > MAX_NUMB_TO_AVE) {  
	/* comprehensive ave */
	graph_info[cpu].maj_numb_we_have++;
    }
    else { /* limited time average */
	/* warning: yes this looks backwards but is not, we are not
	   calculating an index here, we are calculating a number */
	graph_info[cpu].maj_numb_we_have++;
	if (graph_info[cpu].maj_numb_we_have > graph_info[cpu].maj_numb_to_ave) 
	    graph_info[cpu].maj_numb_we_have = graph_info[cpu].maj_numb_to_ave;
    }
    
  just_do_maxes:

    /* check maximums */
    for (i=0;i<minor_frames;i++) {
	if (graph_info[cpu].max_minor[i].ex_ev_val < max_minor[cpu][i].ex_ev_val) {
	    graph_info[cpu].max_minor[i].ex_ev_val = max_minor[cpu][i].ex_ev_val;
	    graph_info[cpu].max_minor[i].ex_ev_maj_frame = frame_numb;
	}
	if (graph_info[cpu].max_minor[i].sum_val < max_minor[cpu][i].sum_val) {
	    graph_info[cpu].max_minor[i].sum_val = max_minor[cpu][i].sum_val;
	    graph_info[cpu].max_minor[i].sum_maj_frame = frame_numb;
	}
	if (graph_info[cpu].max_kern_minor[i].ex_ev_val < max_kern_minor[cpu][i].ex_ev_val) {
	    graph_info[cpu].max_kern_minor[i].ex_ev_val = max_kern_minor[cpu][i].ex_ev_val;
	    graph_info[cpu].max_kern_minor[i].ex_ev_maj_frame = frame_numb;
	}
    }

    if (! write_graph_info)
	goto finish_up;

    graph_info[cpu].ave_valid = FALSE;

    /* now go through and calculate averages */
    for (i=0;i<minor_frames;i++) {
	graph_info[cpu].ave_minor[i].start_time = 
	    graph_info[cpu].total_of_minors[i].start_time / 
		graph_info[cpu].numb_we_have;

	for (j=0;j<graph_info[cpu].minor[i][ave_ind].numb_processes;j++) {
	    graph_info[cpu].ave_minor[i].process[j].ktime = 
		graph_info[cpu].total_of_minors[i].process[j].ktime / 
		    graph_info[cpu].numb_we_have;
	    graph_info[cpu].ave_minor[i].process[j].utime = 
		graph_info[cpu].total_of_minors[i].process[j].utime / 
		    graph_info[cpu].numb_we_have;
	}

	for (j=0; j<graph_info[cpu].ave_minor[i].kern_ev_start_count; j++ ) {
	    graph_info[cpu].ave_minor[i].kern_ev_start_time[j] = 
		graph_info[cpu].total_of_minors[i].kern_ev_start_time[j] / 
		    graph_info[cpu].numb_we_have;
	}
    }
    /* now go through and calculate averages for majors*/
    for (i=0;i<numb_maj_processes;i++) {
	graph_info[cpu].maj_ave_process[i].ktime =
	    graph_info[cpu].maj_total_process[i].ktime / 
		graph_info[cpu].maj_numb_we_have;
	graph_info[cpu].maj_ave_process[i].utime =
	    graph_info[cpu].maj_total_process[i].utime / 
		graph_info[cpu].maj_numb_we_have;
    }
    graph_info[cpu].ave_version = graph_info[cpu].ave_version + 1;
    graph_info[cpu].ave_seen = FALSE;
    graph_info[cpu].ave_valid = TRUE;
    
    /* store the actual time from startup */
    graph_info[cpu].maj_absolute_start = 
	TIC2USECL(mq[adj_start_index].time - absolute_start_time);

    /* increment the averaging indices */
    if (graph_info[cpu].numb_to_ave > MAX_NUMB_TO_AVE)
	graph_info[cpu].ave_index = (graph_info[cpu].ave_index + 1) % 
	    MAX_NUMB_TO_AVE;
    else
	graph_info[cpu].ave_index = (graph_info[cpu].ave_index + 1) % 
	    graph_info[cpu].numb_to_ave;

    if (graph_info[cpu].maj_numb_to_ave > MAX_NUMB_TO_AVE)    
	graph_info[cpu].maj_ave_index = 
	    (graph_info[cpu].maj_ave_index + 1) % MAX_NUMB_TO_AVE;
    else
	graph_info[cpu].maj_ave_index = 
	    (graph_info[cpu].maj_ave_index + 1) % graph_info[cpu].maj_numb_to_ave;

  finish_up:

    usvsema(ave_sem[cpu]);
    

#ifdef DEBUG
    fprintf(fp[cpu], "\n\nmajor frame number %d\n", frame_numb);

    for (i=0;i<minor_frames;i++) {
	    fprintf(fp[cpu], "minor %d numb processes %d starttime %d \n",
		    i, numb_processes, minor[cpu][i].start_time);

	    fprintf(fp[cpu], " processes results\n");
	    for (j=0;j<minor[cpu][i].numb_processes;j++) {
		fprintf(fp[cpu], "  process %d pid %d ktime %d utime %d\n",
			j, minor[cpu][i].process[j].pid, 
			minor[cpu][i].process[j].ktime,
			minor[cpu][i].process[j].utime);
	    }
	}

    if (write_graph_info) {
	for (i=0;i<minor_frames;i++) {
	    fprintf(fp[cpu], "ave: minor %d numb processes %d starttime %d\n",
		    i, graph_info[cpu].ave_minor[i].numb_processes, 
		    graph_info[cpu].ave_minor[i].start_time);
	    for (j=0;j<graph_info[cpu].ave_minor[i].numb_processes;j++) {
		fprintf(fp[cpu], "    process %d pid %d ktime %d utime %d\n",
			j, graph_info[cpu].ave_minor[i].process[j].pid, 
			graph_info[cpu].ave_minor[i].process[j].ktime,
			graph_info[cpu].ave_minor[i].process[j].utime);
	    }
	    /*	fprintf(fp[cpu], "max so far %d\n", rtmon_get_max_time(cpu,i));
		fprintf(fp[cpu], "max so far %d\n", rtmon_get_max_kern_time(cpu,i));*/
	}
	for (i=0;i<numb_maj_processes;i++) {
	    if ((graph_info[cpu].maj_ave_process[i].pid < 100) &&
		(graph_info[cpu].maj_ave_process[i].pid != 0)) {
		sleep(100);
	    }
	    fprintf(fp[cpu],"ave: proc %d pid %d ktime %d utime %d\n",
		    i, graph_info[cpu].maj_ave_process[i].pid,
		    graph_info[cpu].maj_ave_process[i].ktime,
		    graph_info[cpu].maj_ave_process[i].utime);
	}
	fprintf(fp[cpu],"\n\n");
    }
#endif
    goto debug_end;

  end_major_frame:

#ifdef DEBUG
    fprintf(fp[cpu],"\n\nthis major frame discarded debug1 %d debug2 %d\n\n",
	    debugg_er1,debugg_er2);

    for (i=0;i<minor_frames;i++) {
	fprintf(fp[cpu], "minor %d numb processes %d starttime %d ev %d %d %d %d %d %d %d %d\n",
		i, numb_processes, minor[cpu][i].start_time);
	for (j=0;j<minor[cpu][i].numb_processes;j++) {
	    fprintf(fp[cpu], "  process %d pid %d ktime %d utime %d\n",
		    j, minor[cpu][i].process[j].pid, minor[cpu][i].process[j].ktime,
		    minor[cpu][i].process[j].utime);
	}
    }
    fprintf(fp[cpu],"\n\n");
#endif
  debug_end: ;


}

boolean
rtmon_set_frame_ave_factor(int cpu, int ave_value)
{
    int i,j,k;

    if (! cpu_on[cpu])
	return(FALSE);

    uspsema(ave_sem[cpu]);

    for (i=0; i<major_frame_account[cpu].numb_minors; i++) {
	graph_info[cpu].ave_minor[i].start_time = 0;
	graph_info[cpu].total_of_minors[i].start_time = 0;

	for (j=0;j<RTMON_MAX_KERN_STARTUP_EVENTS;j++) {
	    graph_info[cpu].ave_minor[i].kern_ev_start_time[j] = 0;
	    graph_info[cpu].ave_minor[i].kern_ev_start_event[j] = 0;

	    graph_info[cpu].total_of_minors[i].kern_ev_start_time[j] = 0;
	    graph_info[cpu].total_of_minors[i].kern_ev_start_event[j] = 0;
	}
	
	graph_info[cpu].numb_to_ave = ave_value;
	graph_info[cpu].numb_we_have = 0;
	graph_info[cpu].ave_index = 0;
	
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    graph_info[cpu].minor[i][j].start_time = 0;
	    for (k=0;k<RTMON_MAX_KERN_STARTUP_EVENTS;k++) {
		graph_info[cpu].minor[i][j].kern_ev_start_time[k] = 0;
		graph_info[cpu].minor[i][j].kern_ev_start_event[k] = 0;
	    }
	}
	for (j=0; j<major_frame_account[cpu].numb_in_minor[i]; j++) {
	    /* take these out so there's still something for the graph ? */
	    graph_info[cpu].ave_minor[i].process[j].ktime = 0;
	    graph_info[cpu].ave_minor[i].process[j].utime = 0;
	    
	    graph_info[cpu].total_of_minors[i].process[j].ktime = 0;
	    graph_info[cpu].total_of_minors[i].process[j].utime = 0;
	}
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    for (k=0;k<major_frame_account[cpu].numb_in_minor[i]; k++) {
		graph_info[cpu].minor[i][j].process[k].ktime = 0;
		graph_info[cpu].minor[i][j].process[k].utime = 0;
	    }
	}
    }

    usvsema(ave_sem[cpu]);

    return(TRUE);
}


boolean
rtmon_set_proc_ave_factor(int cpu, int ave_value)
{
    int i,j,k;

    if (! cpu_on[cpu])
	return(FALSE);

    uspsema(ave_sem[cpu]);
    

	
    graph_info[cpu].maj_numb_to_ave = ave_value;
    graph_info[cpu].maj_numb_we_have = 0;
    graph_info[cpu].maj_ave_index = 0;
    
       
    for (i=0;i<major_frame_account[cpu].numb_processes; i++) {
	graph_info[cpu].maj_ave_process[i].ktime = 0;
	graph_info[cpu].maj_ave_process[i].utime = 0;

	graph_info[cpu].maj_total_process[i].ktime = 0;
	graph_info[cpu].maj_total_process[i].utime = 0;
	for (j=0;j<MAX_NUMB_TO_AVE; j++) {
	    graph_info[cpu].maj_process[i][j].ktime = 0;
	    graph_info[cpu].maj_process[i][j].utime = 0;	    
	}
    }

    usvsema(ave_sem[cpu]);
    return(TRUE);
}


void
display_graphics(void *empty)
{
    int cpu;
    int count;

    graphics_proc_id = getpid();

    count = 0;
    mainGraph();
    while (! major_frame_account_valid) {
	sleep(1);
	    count++;
	if (count > 2) {
	    if (no_frame_mode) {
		grtmon_message("having problem finding an event", WARNING_MSG, 81);
	    }
	    else {
		grtmon_message("having problems locating a major frame", WARNING_MSG,82 );
	    }
	}
    }
}




boolean 
rtmon_name_pid(int pid, int cpu, char *name)
{
    maj_pid_list_t *search_elem;
    char msg_str[64];

    if (strlen(name) > MAX_PID_LENGTH) {
	sprintf(msg_str, "string length of %d is %d characters too long",
	       strlen(name), strlen(name) - MAX_PID_LENGTH);
	grtmon_message(msg_str, ALERT_MSG, 83);
	return(FALSE);
    }

    search_elem = &maj_pid_list[cpu];
    while((search_elem->next != NULL) && (search_elem->pid != pid)) {
	search_elem = search_elem->next;
    }
    if ((search_elem->next == NULL) && (search_elem->pid != pid)) {
	grtmon_message("failed to find pid match", ALERT_MSG, 84);
	return(FALSE);
    }
    
    strcpy(search_elem->name, name);
    return(TRUE);
}


