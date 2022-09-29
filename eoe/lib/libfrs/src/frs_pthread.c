/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Frame Scheduler Library: Pthread specific interfaces
 */

#include <sys/types.h>
#include <sys/frs.h>
#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>

#define FRS_MULTITHREADED	0x80000000
#define FRS_ID_PREALLOC		50

static pthread_mutex_t	id_table_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t	*id_table = NULL;
static int		id_entries = 0;

static int
frs_id_update(pthread_t pthread, tid_t tid)
{
	pthread_t *tmp;
	int size;

	if (pthread_mutex_lock(&id_table_lock))
		return -1;

	if (id_entries <= tid) {
		/*
		 * Grow the id_table
		 */
		size = sizeof(pthread_t) * (tid + FRS_ID_PREALLOC);
		if ((tmp = (pthread_t*) realloc(id_table, size)) == NULL) {
			(void) pthread_mutex_unlock(&id_table_lock);
			setoserror(ENOMEM);
			return -1;
		}

		id_table = tmp;
		id_entries = tid + FRS_ID_PREALLOC;
	}

	/*
	 * Use the tid as the table index
	 */
	id_table[tid] = pthread;

	if (pthread_mutex_unlock(&id_table_lock))
		return -1;

	return 0;
}

/* frs_id_convert:
 *
 * Convert a list of tids (uthread ids) into a list of corresponding pthread ids
 */
static int
frs_id_convert(tid_t *tidlist, int entries)
{
	int x;

	if (pthread_mutex_lock(&id_table_lock))
		return -1;

	if (entries <= 0 || entries > id_entries) {
		(void) pthread_mutex_unlock(&id_table_lock);
		setoserror(EINVAL);
		return -1;
	}

	for (x=0; x < entries; x++) {
		tid_t tid = tidlist[x];

		/* Sanity check */
		if (tid > id_entries) {
			(void) pthread_mutex_unlock(&id_table_lock);
			setoserror(ESRCH);
			return -1;
		}

		tidlist[x] = id_table[tid];
	}

	if (pthread_mutex_unlock(&id_table_lock))
		return -1;

	return 0;
}

static int
frs_pthread_to_tid(pthread_t pthread, tid_t *tid)
{
	extern int __pttokt(pthread_t, tid_t *);

	if (__pttokt(pthread, tid)) {
		setoserror(ESRCH);
		return -1;
	}

	/* Sanity Check */
	if (*tid < 0) {
		setoserror(ESRCH);
		return -1;
	}

	if (frs_id_update(pthread, *tid))
		return -1;

	*tid |= FRS_MULTITHREADED;
	return 0;
}

frs_t*
__frs_pthread_create(int cpu,
		     int intr_source,
		     int intr_qualifier,
		     int n_minors,
		     tid_t controller,
		     int num_slaves)
{
	frs_t* frs;
	tid_t  master_controller;
	tid_t  this_controller;

	/*
	 * Determine the FRS type and set-up the controller tids
	 */
	if (controller == FRS_SYNC_MASTER) {
		/*
		 * We are assigning the current thread as the Master Controller
		 */
		if (frs_pthread_to_tid(pthread_self(), &master_controller))
			return NULL;
		this_controller = master_controller;
	} else {
		/*
		 * Before we assign the current thread as a Slave Controller,
		 * make sure the passed-in master controller tid is valid.
		 */ 
		if ((controller & FRS_MULTITHREADED) == 0) {
			setoserror(EINVAL);
			return NULL;
		}

		if (frs_pthread_to_tid(pthread_self(), &this_controller))
			return NULL;
	}

	if ((frs = (frs_t*) malloc(sizeof(frs_t))) == 0)
		return NULL;

	frs->frs_info.cpu = cpu;
	frs->frs_info.intr_source = intr_source;
	frs->frs_info.intr_qualifier = intr_qualifier;
	frs->frs_info.n_minors = n_minors;
	frs->frs_info.num_slaves = num_slaves;
	frs->frs_info.controller = controller;

	frs->master_controller = master_controller;
	frs->this_controller = this_controller;
	
	if (schedctl(MPTS_FRS_CREATE, &frs->frs_info) < 0)
		return NULL;
	
	return (frs);
}
	
frs_t*
__frs_pthread_create_vmaster(int cpu,
			     int n_minors,
			     int n_slaves,
			     frs_intr_info_t *intr_info)
{
	frs_t* frs;
	tid_t  master_controller;

	if (frs_pthread_to_tid(pthread_self(), &master_controller))
		return NULL;

	if ((frs = (frs_t*)malloc(sizeof(frs_t))) == 0) {
		setoserror(ENOMEM);
		return NULL;
	}

	frs->frs_info.cpu = cpu;
	frs->frs_info.intr_source = FRS_INTRSOURCE_VARIABLE;
	frs->frs_info.intr_qualifier = 0;
	frs->frs_info.n_minors = n_minors;
	frs->frs_info.controller = FRS_SYNC_MASTER;
	frs->frs_info.num_slaves = n_slaves;

	frs->master_controller = master_controller;
        frs->this_controller = master_controller;
    
	if (schedctl(MPTS_FRS_CREATE, &frs->frs_info, intr_info) < 0)
		return NULL;

	return (frs);
}

int
frs_pthread_enqueue(frs_t* frs,
		    pthread_t target_pthread,
		    int minor_index,
		    uint disc)
{
	frs_queue_info_t frs_queue_info;
	tid_t target_tid;

	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (frs_pthread_to_tid(target_pthread, &target_tid))
		return -1;

	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.thread = target_tid;
	frs_queue_info.minor_index = minor_index;
	frs_queue_info.discipline = disc;

	return (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info));
}

int
frs_pthread_remove(frs_t* frs, int minor_index, pthread_t target_pthread)
{
	frs_queue_info_t frs_queue_info;
	tid_t target_tid;

	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (frs_pthread_to_tid(target_pthread, &target_tid))
		return -1;
    	    
	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.minor_index = minor_index;
	frs_queue_info.thread = target_tid;
	frs_queue_info.discipline = 0; 	/* not required */

	return (schedctl(MPTS_FRS_PREMOVE, &frs_queue_info));
}

int
frs_pthread_insert(frs_t* frs,
		   int minor_index,
		   pthread_t target_pthread,
		   uint discipline,
		   pid_t base_pthread)
{
	frs_queue_info_t frs_queue_info;
	tid_t target_tid;
	tid_t base_tid;

	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (frs_pthread_to_tid(target_pthread, &target_tid))
		return -1;

	if (frs_pthread_to_tid(base_pthread, &base_tid))
		return -1;

	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.minor_index = minor_index;
	frs_queue_info.thread = target_tid;
	frs_queue_info.discipline = discipline;

	return (schedctl(MPTS_FRS_PINSERT, &frs_queue_info, &base_tid));
}

int
frs_pthread_setattr(frs_t* frs,
		    int minor,
		    pthread_t target_pthread,
		    frs_attr_t attr,
		    void* param)
{
	frs_attr_info_t frs_attr_info;
	tid_t target_tid = 0;

	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (target_pthread) {
		if (frs_pthread_to_tid(target_pthread, &target_tid))
			return -1;
	}

	frs_attr_info.controller = frs->this_controller;
	frs_attr_info.minor = minor;
	frs_attr_info.attr = attr;
	frs_attr_info.param = param;
	frs_attr_info.thread = target_tid;
	
	return (schedctl(MPTS_FRS_SETATTR, &frs_attr_info));
}

int
frs_pthread_getattr(frs_t* frs,
		    int minor,
		    pthread_t target_pthread,
		    frs_attr_t attr,
		    void* param)
{
	frs_attr_info_t frs_attr_info;
	tid_t target_tid = 0;

	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (target_pthread) {
		if (frs_pthread_to_tid(target_pthread, &target_tid))
			return -1;
	}

	frs_attr_info.controller =  frs->this_controller;
	frs_attr_info.minor = minor;
	frs_attr_info.attr = attr;
	frs_attr_info.param = param;
	frs_attr_info.thread = target_tid;
	
	return (schedctl(MPTS_FRS_GETATTR, &frs_attr_info));
}

int
frs_pthread_readqueue(frs_t *frs, int minor_index, pthread_t *pt_list)
{
	frs_queue_info_t frs_queue_info;
	int entries;

	if (frs == 0 || pt_list == 0) {
		setoserror(EINVAL);
		return -1;
	}
   
	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.minor_index = minor_index;

	/* not required for this call */
	frs_queue_info.thread = 0;
	frs_queue_info.discipline = 0; 

	entries = schedctl(MPTS_FRS_READQUEUE, &frs_queue_info, (tid_t*) pt_list);

	if (entries > 0 && frs_id_convert((tid_t*) pt_list, entries))
		return -1;

	return (entries);
}
