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
 **************************************************************************/

/*
 * Frame Scheduler Stubs
 */

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/frs.h>
#include <sys/errno.h>

void
frs_handle_counter_intr(void)
{
}

void
frs_handle_timein(void)
{
}

void
frs_handle_group_intr(void)
{
}

void
frs_handle_eintr(void)
{
}

/*ARGSUSED*/
void
frs_handle_vsyncintr(void* frs)
{
}

void
frs_handle_uli(void)
{
}

void
frs_handle_driverintr(void)
{
}

/*ARGSUSED*/
int
frs_driver_export(int frs_driver_id,
		  void (*frs_func_set)(intrgroup_t*),
		  void (*frs_func_clear)(void))
{
	return (-1);
}

void
frs_driver_init(void)
{
}

/*ARGSUSED*/
void*
frs_fsched_acqfrsmod(int cpu)
{
	return (0);
}

/*ARGSUSED*/
void
frs_fsched_relfrs(int cpu)
{
}

struct kthread*
frs_fsched_dispatch(void)
{
	return (0);
}

/*ARGSUSED*/
void
frs_thread_exit(uthread_t *ut)
{
        return;
}

/*ARGSUSED*/
int
frs_user_create(void* arg, void *arg2)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_destroy(tid_t controller)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_enqueue(void* arg)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_join(tid_t controller, long* cminor)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_start(tid_t controller)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_yield(long* cminor)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_eventintr(tid_t controller)
{
	return (ENODEV);
}

int
frs_user_dequeue(void)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_stop(tid_t controller)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_resume(tid_t controller)
{
	return (ENODEV);
}

/*ARGSUSED*/
void*
frs_alloc(int cpu)
{
	return (0);
}

/*ARGSUSED*/
int
frs_user_getqueuelen(void* arg, long* len)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_readqueue(void* arg1, void* arg2, long *retlen)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_premove(void* arg)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_setattr(void* arg1)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_getattr(void* arg1)
{
	return (ENODEV);
}

/*ARGSUSED*/
int
frs_user_pinsert(void* arg1, void* arg2)
{
	return (ENODEV);
}

/*ARGSUSED*/
cpuid_t
frs_fsched_getfrscpu(void* frs )
{
        return (0);
}

/*ARGSUSED*/
void
frs_cpu_frsobjp_print(void* x)
{
}

/*ARGSUSED*/
void
frs_thread_init(uthread_t *ut)
{
}

/*ARGSUSED*/
void
frs_thread_destroy(uthread_t *ut)
{
}
