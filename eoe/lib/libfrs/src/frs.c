/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Frame Scheduler Library
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

extern frs_t* __frs_pthread_create(int cpu,
				   int intr_source,
				   int intr_qualifier,
				   int n_minors,
				   tid_t controller,
				   int num_slaves);

extern frs_t* __frs_pthread_create_vmaster(int cpu,
					   int n_minors,
					   int n_slaves,
					   frs_intr_info_t *intr_info);

frs_t*
frs_create(int cpu,
           int intr_source,
           int intr_qualifier,
           int n_minors,
	   pid_t controller,
           int num_slaves)
{
	frs_t* frs;

	/*
	 * If we are a pthread application, redirect
	 */
	if (pthread_self() != -1) {
		frs = __frs_pthread_create(cpu,
					   intr_source,
					   intr_qualifier,
					   n_minors,
					   (tid_t) controller,
					   num_slaves);
		return (frs);
	}

	if ((frs = (frs_t*)malloc(sizeof(frs_t))) == 0)
		return NULL;

	frs->frs_info.cpu = cpu;
	frs->frs_info.intr_source = intr_source;
	frs->frs_info.intr_qualifier = intr_qualifier;
	frs->frs_info.n_minors = n_minors;
	frs->frs_info.controller = controller;
	frs->frs_info.num_slaves = num_slaves;

	if (controller == FRS_SYNC_MASTER) {
		frs->master_controller = getpid();
	} else {
		frs->master_controller = controller;
	}

        frs->this_controller = getpid();
    
	if (schedctl(MPTS_FRS_CREATE, &frs->frs_info, NULL) < 0)
		return NULL;

	return (frs);
}

frs_t*
frs_create_vmaster(int cpu,
		   int n_minors,
		   int n_slaves,
		   frs_intr_info_t *intr_info)
{
	frs_t *frs;

	/*
	 * If we are a pthread application, redirect
	 */
	if (pthread_self() != -1) {
		frs = __frs_pthread_create_vmaster(cpu,
						   n_minors,
						   n_slaves,
						   intr_info);
		return (frs);
	}

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

	frs->master_controller = getpid();
        frs->this_controller = getpid();
    
	if (schedctl(MPTS_FRS_CREATE, &frs->frs_info, intr_info) < 0)
		return NULL;

	return (frs);
}
	
frs_t*
frs_create_master(int cpu,
                  int intr_source,
                  int intr_qualifier,
                  int n_minors,
                  int num_slaves)
{
	return (frs_create(cpu,
                           intr_source,
                           intr_qualifier,
                           n_minors,
			   FRS_SYNC_MASTER,
                           num_slaves));
}

frs_t*
frs_create_slave(int cpu, frs_t* sync_master_frs)
{
	if (sync_master_frs == 0) {
		setoserror(EINVAL);
		return NULL;
	}

	if (sync_master_frs->frs_info.controller != FRS_SYNC_MASTER) {
		setoserror(EINVAL);
		return NULL;
	}

	return (frs_create(cpu,
			   sync_master_frs->frs_info.intr_source,
			   sync_master_frs->frs_info.intr_qualifier,
			   sync_master_frs->frs_info.n_minors,
			   sync_master_frs->master_controller,
			   0));  /* num_slaves is always 0 for slaves */
}

int
frs_destroy(frs_t* frs)
{
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (schedctl(MPTS_FRS_DESTROY, frs->this_controller) < 0)
		return -1;

	free(frs);
   
	return 0;
}

int
frs_enqueue(frs_t* frs, pid_t pid, int minor_index, uint disc)
{
	frs_queue_info_t frs_queue_info;
    
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}
    	    
	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.thread = pid;
	frs_queue_info.minor_index = minor_index;
	frs_queue_info.discipline = disc;

	if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) < 0)
		return -1;

	return 0;
}

int
frs_start(frs_t* frs)
{
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (schedctl(MPTS_FRS_START, frs->this_controller) < 0)
		return -1;
    
	return 0;
}

int
frs_join(frs_t* frs)
{
	int minor;
    
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}
   
	if ((minor = schedctl(MPTS_FRS_JOIN, frs->this_controller)) < 0)
		return -1;

	return (minor);
}

int
frs_yield(void)
{
	int minor;

	if ((minor = schedctl(MPTS_FRS_YIELD)) < 0)
		return -1;
    
	return (minor);
}

int
frs_userintr(frs_t* frs)
{
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (schedctl(MPTS_FRS_INTR, frs->this_controller) < 0)
		return -1;
    
	return 0;
}

int
frs_getqueuelen(frs_t* frs, int minor_index)
{
	frs_queue_info_t frs_queue_info;
	int queuelen;
    
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}
    	    
	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.minor_index = minor_index;

	/* not required for this call */
	frs_queue_info.thread = 0;
	frs_queue_info.discipline = 0; 

	if ((queuelen = schedctl(MPTS_FRS_GETQUEUELEN, &frs_queue_info)) < 0)
		return -1;

	return (queuelen);
}

int
frs_readqueue(frs_t* frs, int minor_index, pid_t* pidlist)
{
	frs_queue_info_t frs_queue_info;
	int entries;

	if (frs == 0 || pidlist == 0) {
		setoserror(EINVAL);
		return -1;
	}
    	    
	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.minor_index = minor_index;

	/* not required for this call */
	frs_queue_info.thread = 0;
	frs_queue_info.discipline = 0; 

	entries = schedctl(MPTS_FRS_READQUEUE, &frs_queue_info, pidlist);
	if (entries < 0)
		return -1;

	return (entries);
}

int
frs_premove(frs_t* frs, int minor_index, pid_t remove_pid)
{
	frs_queue_info_t frs_queue_info;
    
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}
    	    
	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.minor_index = minor_index;
	frs_queue_info.thread = remove_pid;
	/* not required */
	frs_queue_info.discipline = 0;

	if (schedctl(MPTS_FRS_PREMOVE, &frs_queue_info) < 0)
		return -1;

	return 0;
}

int
frs_pinsert(frs_t* frs,
            int minor_index,
            pid_t insert_pid,
            uint discipline,
            pid_t base_pid)
{
	frs_queue_info_t frs_queue_info;
    
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}
    	    
	frs_queue_info.controller = frs->this_controller;
	frs_queue_info.minor_index = minor_index;
	frs_queue_info.thread = insert_pid;
	frs_queue_info.discipline = discipline;

	if (schedctl(MPTS_FRS_PINSERT, &frs_queue_info, &base_pid) < 0)
		return -1;

	return 0;
}

int
frs_stop(frs_t* frs)
{
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (schedctl(MPTS_FRS_STOP, frs->this_controller) < 0)
		return -1;

	return 0;
}

int
frs_resume(frs_t* frs)
{
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	if (schedctl(MPTS_FRS_RESUME, frs->this_controller) < 0)
		return -1;

	return 0;
}

int
frs_setattr(frs_t* frs, int minor, pid_t pid, frs_attr_t attr, void* param)
{
	frs_attr_info_t frs_attr_info;
	
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	frs_attr_info.controller = frs->this_controller;
	frs_attr_info.minor = minor;
	frs_attr_info.attr = attr;
	frs_attr_info.param = param;
	frs_attr_info.thread = pid;
	
	if (schedctl(MPTS_FRS_SETATTR, &frs_attr_info) < 0)
		return -1;

	return 0;
}

int
frs_getattr(frs_t* frs, int minor, pid_t pid, frs_attr_t attr, void* param)
{
	frs_attr_info_t frs_attr_info;
	
	if (frs == 0) {
		setoserror(EINVAL);
		return -1;
	}

	frs_attr_info.controller = frs->this_controller;
	frs_attr_info.minor = minor;
	frs_attr_info.attr = attr;
	frs_attr_info.param = param;
	frs_attr_info.thread = pid;
	
	if (schedctl(MPTS_FRS_GETATTR, &frs_attr_info) < 0)
		return -1;

	return 0;
}
