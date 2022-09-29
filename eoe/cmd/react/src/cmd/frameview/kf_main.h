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
 * File:	kf_main.h						  *
 *									  *
 * Description:	This file contains data structures for the		  *
 *              kernel functions mode					  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1995 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

/*
 * Defines
 */

#define MAXFUNCTIONS		(1<<15)
#define MAXFUNCTSHOWN		2000		/* XXX */
#define MAXFUNCTNAME		64

/*
 * macros
 */

#define is_kernel_function(cpu,i)  ((graph_info[cpu].no_frame_event[i].evt == START_KERN_FUNCTION) || (graph_info[cpu].no_frame_event[i].evt == END_KERN_FUNCTION))
#define get_kernel_function_number(cpu,i) (graph_info[cpu].no_frame_event[i].quals[0] & 0xffff)
#define get_kernel_pid_from_event(cpu,i) ((graph_info[cpu].no_frame_event[i].quals[0] >> 16) & 0xffff)
#define is_kernel_function_entry(cpu,i) (graph_info[cpu].no_frame_event[i].evt == START_KERN_FUNCTION)
#define is_kernel_function_exit(cpu,i) (graph_info[cpu].no_frame_event[i].evt \
		 == END_KERN_FUNCTION)
#define get_matching_index(cpu,index) (graph_info[cpu].no_frame_event[index].matching_index)
#define get_event_time(cpu,index) (graph_info[cpu].no_frame_event[index].start_time)

/*
 * Data structures
 */

/* struct funct_info: information about kernel functions; function number
   is an index in this struct */
struct funct_info {
	int valid_flag;		/* is this entry valid? */
	char *name;		/* function name */
	int color_index;	/* index in color db */
};

extern struct funct_info funct_info[MAXFUNCTIONS];
extern int funct_info_last_index;

void funct_info_insert(int funcno);

#define get_function_color(funcno)  (get_color_by_index(funct_info[funcno].color_index))

/* struct function_position: info about function segments and the times
   they occurred */

struct function_position {
	int func_index;
	int left_time;
	int right_time;
};

extern struct function_position function_position[MAXCPU][MAXFUNCTSHOWN];
extern int function_position_index[MAXCPU];
extern int get_current_func_no_count;		/* used for debugging */
extern int get_current_func_no_rec_count;	/* used for debugging */

#define CURR_NO_NEXT			0x1
#define CURR_NO_PID_CHANGE		0x2
#define CURR_PID_CHANGED		0x4
#define CURR_NO_MATCH			0x8
#define CURR_OTHER_FCT			0x10
#define CURR_USED_NEXT			0x20
#define CURR_RECURSIVE			0x40
#define CURR_PRINT_DEBUG		0x80
#define CURR_DONT_USE_CACHE		0x100

#define CURR_MAX_TRIES			1000

/* %%% */
/* support for matching function entries and exits in the presence of
 context switches and functions that start in one cpu and finish in
 another */

union cpu_and_index {
	struct ci {
		unsigned int index:25;
		unsigned int cpu:7;
	} ci;
	int	integer;
};

#define MAX_FUNCT_STACK		1000

extern union cpu_and_index function_stack[MAX_FUNCT_STACK];
extern int function_stack_index;  /* initialized to zero */

/* "exit cache": cache used by get_current_func_no to avoid so many
  recursive calls */

struct exit_cache {
	int index;		/* key */
	int val_index;
	int flag;		/* flag used by get_current_func_no */
	short funcno;
	char cpu;
	char valid;		/* is cache entry valid? */
};

#define MAX_EXIT_CACHE		1000	/* # entries in cache */
#define EXIT_CACHE_MAXTRIES	10	/* max rehash tries */

#define exit_cache_hashfunc(index) (index % MAX_EXIT_CACHE)
#define exit_cache_rehashfunc(ind) ((ind + 10) % MAX_EXIT_CACHE)

void exit_cache_invalidate(int cpu);
void exit_cache_insert(int index, int cpu, int val_funcno, int val_index,
                       int val_cpu, int val_flag);
int exit_cache_search(int index, int cpu);


extern struct exit_cache *exit_cache[MAXCPU];
extern int exit_cache_hit_count;
 
void build_matching_list();
int get_matching_time(int index_cpu);
int get_matching_cpu(int index_cpu);
int get_matching_ind(int index_cpu);
int get_time_from_index_cpu(int index_cpu);
int get_cpu_from_index_cpu(int index_cpu);
int get_index_from_index_cpu(int index_cpu);
int is_matching_events(int cpu1,int index1, int cpu2, int index2);
int get_current_func_no(int cpu, int index, int *flag, int *other_func,
                        int *start_index, int *start_cpu,
                        int *other_func_index, int *other_cpu);


/* %%% */


/*
 * Function prototypes
 */

int get_next(int cpu, int index);
int get_previous(int cpu, int index);
#ifdef ORIG_BUILD_MATCHING_LIST
void build_matching_list(int cpu);
int get_current_func_no(int cpu, int index);
#endif
int get_function_index_by_position(int time_pos, int cpu);
int get_function_entry_index_by_position(int time_pos, int cpu);
int funcname_to_funcno(char *funcname);
void get_cmdname_by_pid(int pid, char *cmdname);

