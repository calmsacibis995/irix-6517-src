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
 * File:	kf_main.c						  *
 *									  *
 * Description:	This file contains the routines that handle      	  *
 *              kernel functions mode					  *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1995 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/

#include <stdio.h>
#include <assert.h>

#define GRMAIN extern
#include "graph.h"

#include "kf_main.h"
#include "kf_colors.h"


/*
 * Global variables
 */

struct funct_info funct_info[MAXFUNCTIONS]; /* XXX must change to malloc */
int funct_info_last_index = -1;

struct function_position function_position[MAXCPU][MAXFUNCTSHOWN];
int function_position_index[MAXCPU];


union cpu_and_index function_stack[MAX_FUNCT_STACK];
int function_stack_index = 0;

struct exit_cache *exit_cache[MAXCPU];

/*
 * Functions
 */

/*
 * get_next - given an index in the event list, returns next element which
 *	is a function entry/exit, or -1 if none exist
 */

int get_next(int cpu, int index)
{
	int i;
	int cnt;

	cnt = graph_info[cpu].no_frame_event_count;

	for(i=index+1; i< cnt; i++)
		if(is_kernel_function(cpu,i))
			return i;
	return -1;
}

/*
 * get_previous - given an index in the event list, returns previous element
 *	which is a function entry/exit, or -1 if none exist
 */

int get_previous(int cpu, int index)
{
	int i;

	for(i=index-1; i>= 0; i--)
		if(is_kernel_function(cpu,i))
			return i;
	return -1;
}

/* %%% */
/* support for matching function entries and exits in the presence of
 context switches and functions that start in one cpu and finish in 
 another */

/*
 * push_cpu_and_index - insert a function event (represented by index and
 *	cpu) in the function stack
 */

void push_cpu_and_index(int index, int cpu)
{
	int i;
	char msg_str[64];

	assert(index > 0);

	for(i=0; i<= function_stack_index; i++)
		if(function_stack[i].ci.index == 0)  /* free entry */
			break;
	if(i > function_stack_index)
		function_stack_index = i;
	if(function_stack_index >= MAX_FUNCT_STACK){
		sprintf(msg_str, 
		    "push_cpu_and_index: too many entries in function stack");
		grtmon_message(msg_str, DEBUG_MSG, 100);
		function_stack_index--;
		return;
	}
	graph_info[cpu].no_frame_event[index].matching_index = -1;
	function_stack[i].ci.index = index;
	function_stack[i].ci.cpu   = cpu;
}

/*
 * pop_cpu_and_index: search for an entry with given pid and funct # and
 *	largest possible time. If found return cpu and index and remove
 *	entry from stack
 */

union cpu_and_index pop_cpu_and_index(int functno, int pid)
{
	static union cpu_and_index zero_ci;
	union cpu_and_index ci;
	int cpu, index;
	int i;
	int maxtime;
	int ind;
	int found_cnt = 0;

	for(i=0; i<= function_stack_index; i++){
		cpu = function_stack[i].ci.cpu;
		index = function_stack[i].ci.index;
		if(is_kernel_function(cpu,index) &&
		   (get_kernel_function_number(cpu,index) == functno) &&
		   (get_kernel_pid_from_event(cpu,index) == pid)){
			found_cnt++;
			if(found_cnt == 1){	/* 1st time */
				maxtime = get_event_time(cpu,index);
				ind = i;
			}
			else {
				if(get_event_time(cpu,index) > maxtime){
					maxtime = get_event_time(cpu,index);
					ind = i;
				}
			}
		}
	}

	if(found_cnt > 0) {
		ci = function_stack[ind];
		function_stack[ind].ci.index = 0;
		function_stack[ind].ci.cpu = 0;
		return ci;
	}
	else
		return zero_ci;
}

/*
 * build_matching_list - build matching list: for each function entry event
 *      find the corresponding function exit. Because of context switches
 *	we have to search for the matching event using the pid. The matching
 *	event may be in other cpus.
 *
 *	We traverse all events in all cpus in timestamp order, inserting into
 *	the stack function entry events and searching in the stack when we
 *	have a function exit event.
 *
 *	We need to do the traversal in timestamp order, otherwise the function
 *	exit event that we find may be the incorrect one
 */

void build_matching_list()
{
	int min_time = 0;
	int min_time_cpu;
	int curr_index[MAXCPU];
	int done = 0;
	int i;
	int cpu_map_ind;
	int cpu;
	int index;
	union cpu_and_index ci_entry, ci_exit;
	static int first = 1;
	char msg_str[64];


	if(first)
		first = 0;
	else
		return;

	for(i=0; i< MAXCPU; i++)
		curr_index[i] = 1;

	while(!done) {
		done = 1;
		min_time = 0;
		for(cpu_map_ind = 0; cpu_map_ind<gr_numb_cpus; cpu_map_ind++){
			if (cpuOn[cpu_map_ind]){
				cpu = gr_cpu_map[cpu_map_ind];
				if(curr_index[cpu] <
					graph_info[cpu].no_frame_event_count) {
							/* still evts to read */
					if(done || get_event_time(cpu,
						 curr_index[cpu]) <= min_time) {
						min_time =
					   	get_event_time(cpu,
							     curr_index[cpu]);
						min_time_cpu = cpu;
					}
					done = 0;
				}
			}
		}

		if(!done){
#ifdef DEBUG
sprintf(msg_str, "build: min time %d cpu %d ind %d", min_time, min_time_cpu,
curr_index[min_time_cpu]);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
			index = curr_index[min_time_cpu];
			if(is_kernel_function_entry(min_time_cpu,index))
				push_cpu_and_index(index, min_time_cpu);
			else
				if(is_kernel_function_exit(min_time_cpu,index)){
					ci_entry = pop_cpu_and_index(
					    get_kernel_function_number(
							min_time_cpu,index),
					    get_kernel_pid_from_event(
							min_time_cpu, index));
					if(ci_entry.ci.index > 0) {
							/* found match */
						ci_exit.ci.cpu = min_time_cpu;
						ci_exit.ci.index = index;
						graph_info[min_time_cpu].
				no_frame_event[index].matching_index =
							   ci_entry.integer;
						graph_info[ci_entry.ci.cpu].
				no_frame_event[ci_entry.ci.index].
						matching_index =
							   ci_exit.integer;
#ifdef DEBUG
sprintf(msg_str, "build: match ENT (%d, %d, %d) EXT (%d, %d, %d)",
ci_entry.ci.cpu, ci_entry.ci.index,get_event_time(
 ci_entry.ci.cpu, ci_entry.ci.index),
ci_exit.ci.cpu, ci_exit.ci.index,get_event_time(
 ci_exit.ci.cpu, ci_exit.ci.index));
	grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
					}	
					else {
						graph_info[min_time_cpu].
                                no_frame_event[index].matching_index = -1;
					}

				}
			curr_index[min_time_cpu]++;
		}

	}
}

/*
 * support functions
 */

int get_matching_time(int index_cpu)
{
	union cpu_and_index ci;
	int match;

	ci.integer = index_cpu;
	match = graph_info[ci.ci.cpu].no_frame_event[ci.ci.index].
							matching_index;
	if(match == -1)
		return -1;
	ci.integer = match;
	return get_event_time(ci.ci.cpu, ci.ci.index);
}

int get_matching_cpu(int index_cpu)
{
	union cpu_and_index ci;
	int match;

	ci.integer = index_cpu;
	match = graph_info[ci.ci.cpu].no_frame_event[ci.ci.index].
							matching_index;
	if(match == -1)
		return -1;
	ci.integer = match;

	return ci.ci.cpu;
}


int get_matching_ind(int index_cpu)
{
	union cpu_and_index ci;
	int match;

	ci.integer = index_cpu;
	match = graph_info[ci.ci.cpu].no_frame_event[ci.ci.index].
							matching_index;
	if(match == -1)
		return -1;
	ci.integer = match;
	return ci.ci.index;
}

int get_time_from_index_cpu(int index_cpu)
{
	union cpu_and_index ci;

	ci.integer = index_cpu;
	return get_event_time(ci.ci.cpu, ci.ci.index);
}

int get_cpu_from_index_cpu(int index_cpu)
{
	union cpu_and_index ci;

	ci.integer = index_cpu;
	return ci.ci.cpu;
}

int get_index_from_index_cpu(int index_cpu)
{
	union cpu_and_index ci;

	ci.integer = index_cpu;
	return ci.ci.index;
}

/*
 * is_matching_events: return 1 if the 2 given events "match" (one is an
 *	entry function and the other is the corresponding exit)
 */

int is_matching_events(int cpu1,int index1, int cpu2, int index2)
{
	union cpu_and_index ci1, ci2;

	ci2.ci.cpu = cpu2;
	ci2.ci.index = index2;

	ci1.integer = get_matching_index(cpu1, index1);

	if(ci1.integer == ci2.integer)
		return 1;
	else
		return 0;
}


/*
 * get_current_func_no - get number of function currently being executed.
 *	Returns -1 if no current function (current function is unknown)
 *
 *	If the current element is a function ENTRY, then the current function
 *	is this element's function number
 *
 *	If the current element is a function EXIT, then we must go back in the
 *	event list and find out if there is a known function executing.
 *	When going back we stop (and return -1) if our search finds a
 *	function that starts in one cpu and finishes in another.
 *
 *	We set a flag if we find a function belonging to a different process
 *
 *	If the next event record belongs to a different process, we set
 *	other_func (because the "current function" is also that of the
 *	other process; there was a context switch between the current event
 *	and the next, and we don't know exactly what function was executing
 *	in the interval between the current and the next event).
 *
 */

int get_current_func_no_count = 0;
int get_current_func_no_rec_count = 0;
int exit_cache_hit_count = 0;

int get_current_func_no(int cpu, int index, int *flag, int *other_func,
			int *start_index, int *start_cpu,
			int *other_func_index, int *other_cpu)
{
	int func_no = -1;
	int prev_matching_entry;
	int previous;
	int orig_previous;
	int next;
	int loc_flag, loc_other_func;
	int st_ind;
	int i=0;
	int cache_ind;
    char msg_str[64];

	get_current_func_no_count++;
	if(*flag & CURR_RECURSIVE)
		get_current_func_no_rec_count ++;

if((*flag & CURR_PRINT_DEBUG) || (cpu == 0 && index < -2)) {
 sprintf(msg_str, "get_current_func_no: cpu %d index %d FCT: %s pid %d", cpu, index,
(index!=-1)?
 funct_info[get_kernel_function_number(cpu,index)].name:"NO FCT",
 (index!= -1)? get_kernel_pid_from_event(cpu,index): -1);
grtmon_message(msg_str, DEBUG_MSG, 100);
}

/*
	*flag = 0;
*/
	if((index != -1) && is_kernel_function(cpu,index)) {
		next = get_next(cpu, index);

if((*flag & CURR_PRINT_DEBUG) || (cpu == 0 && index < -2)) {
 sprintf(msg_str, "get_current_func_no: NEXT: %s pid %d",
 (next!= -1)?
 funct_info[get_kernel_function_number(cpu,next)].name:"NO FCT",
 (next!= -1)? get_kernel_pid_from_event(cpu,next): -1);
	 grtmon_message(msg_str, DEBUG_MSG, 100);}

if(*flag & CURR_PRINT_DEBUG) {
int nextnext = -1;
if(next != -1)
 nextnext = get_next(cpu, next);
sprintf(msg_str, "get_current_func_no: NEXT NEXT: %s pid %d",
 (nextnext!= -1)?
 funct_info[get_kernel_function_number(cpu,nextnext)].name:"NO FCT",
 (nextnext!= -1)? get_kernel_pid_from_event(cpu,nextnext): -1);
	grtmon_message(msg_str, DEBUG_MSG, 100);
}

if((*flag & CURR_PRINT_DEBUG) || (cpu == 0 && index < -2)){
int cp, in, cpind;

 if((index >= 0) && (cpu >= 0) && ((cpind = get_matching_index(cpu,index))>=0)){
   cp = get_cpu_from_index_cpu(cpind);
   in = get_index_from_index_cpu(cpind);
   sprintf(msg_str, "get_current_func_no: matching of %d: cpu %d index %d pid %d",
    index, cp, in,((cp >=0)&&(in >=0))? get_kernel_pid_from_event(cp,in): -1);
   grtmon_message(msg_str, DEBUG_MSG, 100);
 }
 else
   sprintf(msg_str, "get_current_func_no: no matching for %d",index);
grtmon_message(msg_str, DEBUG_MSG, 100);
}
		if(is_kernel_function_entry(cpu,index)) {
					/* FUNCTION ENTRY */
			func_no = get_kernel_function_number(cpu,index);
			if((start_index != NULL) && (start_cpu != NULL)){
			   *start_index = index;
			   *start_cpu = cpu;
			}
			if(next == -1) {
			   *flag |= CURR_NO_NEXT;
			}
			else {
			   if(is_kernel_function_entry(cpu,next)) {
			      if(get_kernel_pid_from_event(cpu, index) ==
				 get_kernel_pid_from_event(cpu, next)) {
				 *flag |= CURR_NO_PID_CHANGE;
			      }
			      else {
				 *flag |= CURR_PID_CHANGED;
			      }
		           }
			   else {    /* next is an exit event */
			      if(!is_matching_events(cpu,index, cpu, next))
					/* next event is an exit but not of the
					   same function: some ctx swtch
					   happened */
			         if(get_kernel_pid_from_event(cpu, index) ==
				    get_kernel_pid_from_event(cpu, next))
				     *flag |= CURR_NO_MATCH;
				 else {
				     *flag |=CURR_PID_CHANGED;
				 }
				 if((*flag & CURR_RECURSIVE) == 0) {
				     *flag |= CURR_OTHER_FCT;
				     *other_func =
					get_kernel_function_number(cpu,next);
				     if((other_func_index != NULL) &&
					(other_cpu != NULL)){
					  st_ind =get_matching_index(cpu, next);
				          if(st_ind != -1){
                                       		*other_func_index =
					       get_index_from_index_cpu(st_ind);
						*other_cpu =
					       get_cpu_from_index_cpu(st_ind);
					  }
					  else {
						*other_func_index = -1;
						*other_cpu = -1;
					  }
				      }
				 }
			  }
		       }
		}
		if(is_kernel_function_exit(cpu,index)) {
					/* FUNCTION EXIT */
			if(next != -1) {
				if(get_kernel_pid_from_event(cpu, index) !=
				   get_kernel_pid_from_event(cpu, next)) {
				   *flag |= CURR_PID_CHANGED;
				}
				if(is_kernel_function_exit(cpu,next) &&
				  ((*flag & CURR_RECURSIVE) == 0)){
				   *flag |= CURR_OTHER_FCT;
				   *other_func =
                                     get_kernel_function_number(cpu,next);
				   if((other_func_index != NULL) &&
				      (other_cpu != NULL)){
					  st_ind =get_matching_index(cpu, next);
				          if(st_ind != -1) {
                                       		*other_func_index =
					       get_index_from_index_cpu(st_ind);
					        *other_cpu =
					       get_cpu_from_index_cpu(st_ind);
					  }
					  else {
						*other_func_index = -1;
						*other_cpu = -1;
					  }
				   }
				}
			}

if(cpu == 0 && index < -2)
 sprintf(msg_str, "get_current_func_no: EXIT cpu %d index %d next %d", cpu,index,next);
			grtmon_message(msg_str, DEBUG_MSG, 100);

			prev_matching_entry = get_matching_index(cpu,index);
			if(get_cpu_from_index_cpu(prev_matching_entry) == cpu){
				/* if matching event is in another cpu
				   return -1 */

				assert(is_kernel_function_entry(
				get_cpu_from_index_cpu(prev_matching_entry),
				get_index_from_index_cpu(prev_matching_entry)));
				
				orig_previous = previous = get_previous(cpu,
					get_index_from_index_cpu(
							prev_matching_entry));
if(cpu == 0 && index < -2) {
 sprintf(msg_str, "get_current_func_no: cpu %d index %d: EX prv: fct: %s (%d)",
 cpu, index, (previous!=-1)?
 funct_info[get_kernel_function_number(cpu,previous)].name:"NO FCT",previous);
	 grtmon_message(msg_str, DEBUG_MSG, 100);
}

				/* go back to find entry of same pid */
				for(;;) {
				   if(previous == -1)
				      break;
				   if(get_kernel_pid_from_event(cpu, index) ==
				      get_kernel_pid_from_event(cpu, previous))
				      break;
				   else {
				      *flag |= CURR_PID_CHANGED;
if(cpu == 0 && index < -2)
 sprintf(msg_str, "\tpid of %d != from %d", index, previous);
				      grtmon_message(msg_str, DEBUG_MSG, 100);
				   }
				   if(++i >= CURR_MAX_TRIES){
				      previous = -1;
				      break;
				   }
				   previous = get_previous(cpu,previous);
				}

if(cpu == 0 && index < -2) {
 sprintf(msg_str, "get_current_func_no: cpu %d index %d: AFTER for : fct: %s (%d)",
 cpu, index, (previous!=-1)?
 funct_info[get_kernel_function_number(cpu,previous)].name:"NO FCT",previous);
	 grtmon_message(msg_str, DEBUG_MSG, 100);}

				if(previous >= 0)
				  if(is_kernel_function_entry(cpu, previous)){
			            func_no =
				      get_kernel_function_number(cpu,previous);
				    if((start_index != NULL) &&
				       (start_cpu != NULL)) {
                           	       *start_index = previous;
				       *start_cpu = cpu;
				    }
				  }
				  else {
				    /* call recursively */
if(cpu == 0 && index < -2)
 sprintf(msg_str, "get_current_func_no: cpu %d index %d: calling recursivelly ind %d",
 cpu, index, previous);
grtmon_message(msg_str, DEBUG_MSG, 100);
				   if(((*flag & CURR_DONT_USE_CACHE) == 0) &&
				      ((cache_ind = exit_cache_search(index,
					cpu)) != -1)) { /* found in cache */
					func_no =
					    exit_cache[cpu][cache_ind].funcno;
					*flag |=
					    exit_cache[cpu][cache_ind].flag;
					if(start_index!= NULL)
					    *start_index =
						exit_cache[cpu][cache_ind].
								val_index;
					if(start_cpu!=NULL)
					    *start_cpu =
						exit_cache[cpu][cache_ind].cpu;
				   }
				   else { /* not found in cache: recurse */

				     loc_flag = CURR_RECURSIVE |
					(*flag & CURR_DONT_USE_CACHE);
				     func_no =
				       get_current_func_no(cpu, previous,
				        &loc_flag, &loc_other_func,start_index,
					start_cpu, NULL, NULL);
				     *flag |= (loc_flag &
					(~(CURR_OTHER_FCT | CURR_USED_NEXT)));
/*
				     *other_func = loc_other_func;
*/
				     /* only the current call can use 'next' */
				     if(loc_flag & CURR_USED_NEXT)
					  func_no = -1;

				     if((func_no == -1) &&
				        (orig_previous != previous))
						   { /* try another process*/

if(cpu == 0 && index < -2)
 sprintf(msg_str, "get_current_func_no: cpu %d index %d: calling recursivelly II ind %d",
 cpu, index, orig_previous);
grtmon_message(msg_str, DEBUG_MSG, 100);
				       loc_flag = CURR_RECURSIVE |
					  (*flag & CURR_DONT_USE_CACHE);
				       func_no =
				         get_current_func_no(cpu, orig_previous,
					 &loc_flag, &loc_other_func,start_index,
					 start_cpu, NULL, NULL);
				       *flag |= (loc_flag &
					(~(CURR_OTHER_FCT | CURR_USED_NEXT)));
				       if(loc_flag & CURR_USED_NEXT)
					   func_no = -1;
				      }

				      exit_cache_insert(index, cpu, func_no,
					(start_index!=NULL)?(*start_index):0, 
					(start_cpu!=NULL)?(*start_cpu):0, 
					loc_flag &
                                        (~(CURR_OTHER_FCT | CURR_USED_NEXT)));
				    } /* not found in cache */
				  } /* call recursively */

			}
#ifdef DEBUG
else
 sprintf(msg_str, "get_current_func_no: cpu %d index %d matching in another CPU",
 cpu, index);
			grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

			/* still could not find funct. If next is an exit, then
			   probably we are in next's function */
			if((func_no == -1) &&
		   	   (next != -1) &&
		   	   is_kernel_function_exit(cpu, next) &&
			   ((*flag) & CURR_OTHER_FCT)){
				func_no = get_kernel_function_number(cpu,next);
				*flag &= (~CURR_OTHER_FCT);
				*flag |= CURR_USED_NEXT;
if(cpu == 0 && index < -2)
 sprintf(msg_str, "get_current_func_no: cpu %d index %d: using next fl 0x%x",
 cpu, index, *flag);
				grtmon_message(msg_str, DEBUG_MSG, 100);
				if((start_index != NULL) &&
				   (start_cpu != NULL)){
					st_ind = get_matching_index(cpu, next);
				        if(st_ind != -1) {
                                       		*start_index =
					       get_index_from_index_cpu(st_ind);
						*start_cpu =
					       get_cpu_from_index_cpu(st_ind);
					}
					else {
						*start_index = -1;
						*start_cpu = -1;
					}
				}
			}

		}
	}

	if((start_index != NULL) &&
	   (*flag & CURR_OTHER_FCT) &&
	   (other_func_index != NULL) &&
	   (*start_index == *other_func_index))
		*flag &= (~CURR_OTHER_FCT);

if(cpu == 0 && index < -2)
 sprintf(msg_str, "get_current_func_no: cpu %d index %d returning %d fl 0x%x",
cpu, index, func_no, *flag);
grtmon_message(msg_str, DEBUG_MSG, 100);

	return func_no;
}

/* =========== exit_cache ================================= */

/*
 * exit_cache_invalidate - invalidate exit cache.
 *
 *	MUST be called before any call to the other exit_cache functions
 */

void exit_cache_invalidate(int cpu)
{
	struct exit_cache *ptr;
	int i;
    char msg_str[64];

	if(exit_cache[cpu] == NULL) {
		exit_cache[cpu] = (struct exit_cache *)
			malloc(MAX_EXIT_CACHE * sizeof(struct exit_cache));
		if(exit_cache[cpu] == NULL) {
			sprintf(msg_str, "No memory for exit_cache");
			grtmon_message(msg_str, DEBUG_MSG, 100);
			exit(1);
		}
	}
	for(i=0; i< MAX_EXIT_CACHE; i++)
		exit_cache[cpu][i].valid = 0;
}

/*
 * exit_cache_insert - insert one element in exit cache if not already there
 */

void exit_cache_insert(int index, int cpu, int val_funcno, int val_index,
		       int val_cpu, int val_flag)
{
	int ind;
	int ntries = 0;

	ind = exit_cache_hashfunc(index);

	if(exit_cache[cpu][ind].valid == 0) {
		exit_cache[cpu][ind].valid = 1;
		exit_cache[cpu][ind].index     = index;  /* key */
		exit_cache[cpu][ind].val_index = val_index;
		exit_cache[cpu][ind].funcno    = val_funcno;
		exit_cache[cpu][ind].cpu       = val_cpu;
		exit_cache[cpu][ind].flag      = val_flag;
		return;
	}
	/* cache entry in use */
	if(exit_cache[cpu][ind].index == index) /* already in cache */
		return;

	/* position used: try other entries */
	for(; ntries < EXIT_CACHE_MAXTRIES; ntries++){
		ind = exit_cache_rehashfunc(ind);
		if(exit_cache[cpu][ind].valid == 0) {
			exit_cache[cpu][ind].valid = 1;
			exit_cache[cpu][ind].index     = index;  /* key */
			exit_cache[cpu][ind].val_index = val_index;
			exit_cache[cpu][ind].funcno    = val_funcno;
			exit_cache[cpu][ind].cpu       = val_cpu;
			exit_cache[cpu][ind].flag      = val_flag;
			return;
		}
		/* cache entry in use */
		if(exit_cache[cpu][ind].index == index) /* already in cache */
			return;
	}

	/* can't insert */
}

/*
 * exit_cache_search - search element in exit cache; return index in
 *	exit cache if found, -1 otherwise
 */

int exit_cache_search(int index, int cpu)
{
	int ind;
	int ntries = 0;

	ind = exit_cache_hashfunc(index);

	if(exit_cache[cpu][ind].valid == 0)
		return -1;
	if(exit_cache[cpu][ind].index == index) { /* found */
		exit_cache_hit_count ++;
		return ind;
	}

	/* position used by other entry: try "rehash" */
	for(; ntries < EXIT_CACHE_MAXTRIES; ntries++){
		ind = exit_cache_rehashfunc(ind);
		if(exit_cache[cpu][ind].valid == 0)
			return -1;
		if(exit_cache[cpu][ind].index == index) { /* found */
			exit_cache_hit_count ++;
			return ind;
		}
	}

	return -1;	/* not found */
}


	
/* ======================================================== */


/* %%% */

#ifdef ORIG_BUILD_MATCHING_LIST
/*
 * build_matching_list - build matching list: for each function entry event
 *	find the corresponding function exit
 */

void build_matching_list(int cpu)
{
	int i;
	int index_exit;
	int stack_level;
	int func_no;
	int cnt;
    char msg_str[64];

	static int last_indices[MAXCPU]; /* last indices processed;
					    zero-initialized */

#ifdef DEBUG
sprintf(msg_str, "\tbuild_matching_list: cpu %d ev cnt %d",
cpu, graph_info[cpu].no_frame_event_count);
	grtmon_message(msg_str, DEBUG_MSG, 100);
if(graph_info[cpu].no_frame_event_count > 1000)
  return;
#endif

	cnt = graph_info[cpu].no_frame_event_count;

	for (i=last_indices[cpu]+1;i< cnt; i++)
		graph_info[cpu].no_frame_event[i].matching_index = -1;

	for (i=last_indices[cpu]+1;i< cnt; i++){
		if(is_kernel_function_entry(cpu,i)) { /* find matching exit */
			func_no = get_kernel_function_number(cpu,i);
			stack_level = 1;
			for(index_exit = i+1; index_exit < cnt; index_exit++){
				if(is_kernel_function(cpu,index_exit) &&
				   (get_kernel_function_number(cpu,index_exit)==
						func_no)) {
				   if(is_kernel_function_entry(cpu,index_exit))
					stack_level++;
				   if(is_kernel_function_exit(cpu,index_exit)) {
					stack_level--;
					if(stack_level == 0)   /* found it */
					    break;
				   }
				}
			}
			if(index_exit < graph_info[cpu].no_frame_event_count){
						/* found */
#ifdef DEBUG
sprintf(msg_str, "\tbuild_matching_list: cpu %d fnc %d ENT %d EXIT %d - st t %d",
cpu, func_no,i,index_exit,
graph_info[cpu].no_frame_event[i].start_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
			  graph_info[cpu].no_frame_event[i].matching_index
								= index_exit;
			  graph_info[cpu].no_frame_event[index_exit].
							matching_index = i;
			}
			else
			  graph_info[cpu].no_frame_event[i].matching_index = -1;
		}
	}
	last_indices[cpu] = graph_info[cpu].no_frame_event_count - 1;
}
#endif

#ifdef ORIG_BUILD_MATCHING_LIST
/*
 * get_current_func_no - get number of function currently being executed.
 *	Returns -1 if no current function (current function is unknown)
 *
 *	If the current element is a function ENTRY, then the current function
 *	is this element's function number
 *	If the current element is a function EXIT, then we must go back in the
 *	event list and find out if there is a known function executing
 *
 */

int get_current_func_no(int cpu, int index)
{
	int func_no = -1;
	int prev_matching_entry;
	int previous;

	if((index != -1) && is_kernel_function(cpu,index)) {
		if(is_kernel_function_entry(cpu,index))
			func_no = get_kernel_function_number(cpu,index);
		else if(is_kernel_function_exit(cpu,index)) {
			prev_matching_entry = get_matching_index(cpu,index);
			previous = get_previous(cpu, prev_matching_entry);
#ifdef DEBUG
sprintf(msg_str, "get_current_func_no: [EXIT] match %d prev %d",
prev_matching_entry,previous);
			grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
			if(previous >= 0)
			   if(is_kernel_function_entry(cpu,previous))
				func_no =
				     get_kernel_function_number(cpu,previous);
			   else
				func_no =
				     get_current_func_no(cpu, previous);
						/* RECURSIVE */
			/* else func_no remains (-1) */
		}
	}

#ifdef DEBUG
sprintf(msg_str, "get_current_func_no: index %d returning %d",index,func_no);
	grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

	return func_no;
}
#endif

/*
 * funct_info_insert - create entry in funct_info for the given function
 *	number
 */

void funct_info_insert(int funcno)
{
	float *rgb_ptr;
	char msg_str[64];


	if((funcno >= MAXFUNCTIONS) || (funcno < 0)){
		sprintf(msg_str, "Invalid function number: %d",funcno);
		grtmon_message(msg_str, DEBUG_MSG, 100);
	}
	else {
		if(funct_info[funcno].valid_flag == 0){
			if(funcno > funct_info_last_index)
				funct_info_last_index = funcno;
			if(kern_funct_name_map[funcno][0] != '\0')
				funct_info[funcno].name =
					kern_funct_name_map[funcno];

			/* try to get the color from the function - color
			   data base. If not found get new color */
			if(funct_info[funcno].name != NULL){
				rgb_ptr = get_color_by_funcname(
					funct_info[funcno].name);

#ifdef DEBUG
sprintf(msg_str, "funct_info_insert: %s rgb_ptr 0x%x",funct_info[funcno].name,
rgb_ptr);
				grtmon_message(msg_str, DEBUG_MSG, 100);
#endif
				if(rgb_ptr != (float *)-1){
					funct_info[funcno].color_index
						= insert_color_in_db(rgb_ptr);
				}
				else
					funct_info[funcno].color_index =
								color_alloc();
			}
			else
				funct_info[funcno].color_index = color_alloc();


sprintf(msg_str, "funct_info_insert: func %d R %4.2f G %4.2f B %4.2f name: %s",
funcno,
color_db[funct_info[funcno].color_index].rgb[0],
color_db[funct_info[funcno].color_index].rgb[1],
color_db[funct_info[funcno].color_index].rgb[2],
(funct_info[funcno].name)?funct_info[funcno].name:"<no name>");
grtmon_message(msg_str, DEBUG_MSG, 100);
		}
		funct_info[funcno].valid_flag = 1;
	}
}

/*
 * funcname_to_funcno - get function number given name; returns -1 if
 *	not found
 */

int funcname_to_funcno(char *funcname)
{
	int i;

	for(i=0; i<= funct_info_last_index; i++)
		if(funct_info[i].valid_flag &&
		   (funct_info[i].name != NULL) &&
		   !strcmp(funct_info[i].name, funcname))
		return i;

	return -1;
}

/*
 * get_function_index_by_position - return function (event) index given a
 *	time position. The returned event is the last to occur before
 *	'time_pos'. From the returned value, use get_current_func_no()
 *	to find out the actual function number
 */

int get_function_index_by_position(int time_pos, int cpu)
{
	int i;
	int number_of_functions;
	char msg_str[64];

	number_of_functions = function_position_index[cpu];

#ifdef DEBUG
sprintf(msg_str, "get_function_index_by_position: t %d cpu %d", time_pos, cpu);
	grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

	for(i=0; i < number_of_functions; i++){

#ifdef DEBUG
sprintf(msg_str, "\ttrying fct indx %d LEFT %d RIGHT %d",
function_position[cpu][i].func_index,
function_position[cpu][i].left_time,
function_position[cpu][i].right_time);
grtmon_message(msg_str, DEBUG_MSG, 100);
#endif

		if((function_position[cpu][i].left_time <=  time_pos) &&
		   (function_position[cpu][i].right_time >  time_pos))
			return function_position[cpu][i].func_index;
	}

	return -1;
}

/*
 * get_function_entry_index_by_position - given time position, return
 *	event index of function entry
 */

int get_function_entry_index_by_position(int time_pos, int cpu)
{
	int prev_index;
	int prev_matching_entry;
	int previous;
	int ret_index = -1;

	prev_index = get_function_index_by_position(time_pos, cpu);
	/* assume prev_index is either function entry or exit */
	if(prev_index != -1){
		if(is_kernel_function_entry(cpu,prev_index))
			ret_index = prev_index;
#ifdef ORIG_BUILD_MATCHING_LIST
		else if(is_kernel_function_exit(cpu,prev_index)) {
			while(1){
			    prev_matching_entry =
					get_matching_index(cpu,prev_index);
			    previous = get_previous(cpu, prev_matching_entry);
			    if(previous >= 0)
			          if(is_kernel_function_entry(cpu,previous)){
				      ret_index = previous;
				      break;
			    	  }
			          else	/* funct exit */
				      prev_index = previous;
			    else
				break; /* not found */
			}
		     }
#endif
	}
	return ret_index;
}

/*
 * get_cmdname_by_pid - get command name given pid
 */

void get_cmdname_by_pid(int pid, char *cmdname)
{
	maj_pid_list_t *search_elem;
	char str[256];
	int i, cpu = -1;
    char msg_str[64];

	for (i = gr_numb_cpus - 1; i >= 0; i--) {
		if (cpuOn[i]) {
			cpu = gr_cpu_map[i];
			search_elem = &maj_pid_list[cpu];
			while(search_elem != NULL) {
				if(search_elem->pid == pid) {
					strcpy(cmdname, search_elem->name);
					return;
				}
				search_elem = search_elem->next;
			}
		}
	}

	if(cpu == -1) {
		sprintf(msg_str, "get_cmdname_by_pid: NO ACTIVE CPUS ??!!");
		grtmon_message(msg_str, DEBUG_MSG, 100);
		strcpy(cmdname, "<ERROR>");
		return;
	}
	cmdname[0] = '\0';	/* not found */
}

