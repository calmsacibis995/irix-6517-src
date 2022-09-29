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

#if defined(EVEREST) || defined(SN0) || defined (IP30)

/*
 * High Level View - See  <(frame_hlv.ps)>  
 */

/*
 * Frame Scheduler - Kernel Entry Points for User Interface
 */

#include <sys/types.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/runq.h>
#include <sys/sysmp.h>
#include <sys/schedctl.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/rtmon.h>
#include <sys/frs.h>
#include <sys/signal.h>

#include "frame_barrier.h"
#include "frame_semabarrier.h"
#include "frame_base.h"
#include "frame_cpu.h"
#include "frame_process.h"
#include "frame_procd.h"
#include "frame_minor.h"
#include "frame_major.h"
#include "frame_sched.h"
#include "frame_debug.h"

#if _MIPS_SIM == _ABI64
typedef struct irix5_frs_fsched_info {
    int cpu;
    int intr_source;
    int intr_qualifier; /* period, pipenumber, driverid */
    int n_minors;
    tid_t controller;
    int num_slaves;
} irix5_frs_fsched_info_t;

static int irix5_to_frs_fsched_info(
    enum xlate_mode mode,
    void *to,
    int count,
    register xlate_info_t *info);

typedef struct irix5_frs_attr_info {
    app32_int_t		controller;  /* select the frame scheduler */
    app32_int_t		minor;       /* select minor within a frame sched */
    app32_int_t		thread;      /* select thread within a minor */
    frs_attr_t		attr;	     /* select which attribute */
    app32_ptr_t 	param;       /* the actual attribute values */
} irix5_frs_attr_info_t;

static int irix5_to_frs_attr_info(
    enum xlate_mode mode,
    void *to,
    int count,
    register xlate_info_t *info);
#endif

/**************************************************************************
 *                            User Access Methods                         *
 **************************************************************************/

/*
 * Create an FRS controller
 */
/* ARGSUSED */
int
frs_user_create(void* arg1, void* arg2)
{
	frs_fsched_info_t info;
	frs_intr_info_t *intr_info = NULL;
	/* REFERENCED */
	frs_fsched_t* frs = 0;
	int ret = 0;
	int size;

	if (arg1 == NULL) {
		ret = EFAULT;
		goto error;
	}

	if (COPYIN_XLATE(arg1, (void*)&info, sizeof(frs_fsched_info_t),
			 irix5_to_frs_fsched_info, get_current_abi(), 1)) {
		ret = EFAULT;
		goto error;
	}

	if (info.controller == FRS_SYNC_MASTER &&
	    info.intr_source == FRS_INTRSOURCE_VARIABLE) {
		size = sizeof(frs_intr_info_t) * info.n_minors;
		
		intr_info = (frs_intr_info_t*) kmem_zalloc(size, KM_SLEEP);
		if (intr_info == NULL) {
			ret = ENOMEM;
			goto error;
		}

		if (copyin(arg2, (void*) intr_info, size)) {
			ret = EFAULT;
			goto error;
		}
	}

        /*
         * Verify this cpu exists.
         */
        if ((ret = frs_cpu_verifyvalid(info.cpu)) != 0) {
                goto error;
        }

        /*
         * Verify the requested cpu is not the system clock cpu.
         */
        if ((ret = frs_cpu_verifynotclock(info.cpu)) != 0) {
                goto error;
        }
        
        /*
         * Verify this thread qualifies to be an frs controller,
	 * and if so mark it as one.
         */
        if ((ret = frs_thread_controller_access(curuthread)) != 0) {
                goto error;
        }

        /*
         * Now we create the frs object.
         * the returned object has one reference
         * already, due to the link from the sdesc
         * descriptor.
         */
        if ((frs = frs_fsched_create(&info, intr_info, &ret)) == NULL) {
                frs_thread_controller_unset(curuthread);
                goto error;
        }

	if (intr_info)
		kmem_free(intr_info, size);

	return (0);

error:
	if (intr_info)
		kmem_free(intr_info, size);

	frs_debug_message(DM_CREATE, 2, "[frs_user_create]: Failed", ret);
	return (ret);
}

int
frs_user_destroy(tid_t controller)
{
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
        int		 ret;

        /*
         * Get hold of the frs object.
	 *
         * If the specified controller is indeed a control thread, and its
	 * frs pointer is non-null, then we are pointing to a valid frs.
	 * As a valid frs, it has a reference to the sdesc structure, known
	 * to the master controller frs because of its contribution to the
         * reference count.
	 *
         * If the frs exists, then its sdesc pointer must be valid because
	 * the master must be alive. The master cannot be deallocated
	 * (including the sync-desc) until its ref count becomes zero.
         */
	frs_tid_to_threadid(controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

        /*
         * Release reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);
        
        /*
         * We have verified this thread was a controller.
         * All we need to do now is send a SIGKILL signal
         * to this thread to trigger the frs destruction.
         */
	frs_thread_sendsig(&control_thread_id, SIGKILL);
 
        return (0);

error:
        frs_debug_message(DM_DESTROY, 2,
                          "[frs_user_destroy]: FRS Destruction FAILED",
			  controller);
        return (ret);       
}

int
frs_user_enqueue(void* arg)
{
	frs_queue_info_t	 info;
	frs_fsched_t		*frs;
	frs_threadid_t		 control_thread_id;
	frs_threadid_t		 target_thread_id;
        int			 ret;

	if (arg == NULL) {
		return (EFAULT);
	}
	
	if (copyin(arg, (void*)&info, sizeof(frs_queue_info_t))) {
		return (EFAULT);
	}

        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(info.controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

	frs_tid_to_threadid(info.thread, &target_thread_id);
	if ((ret = frs_fsched_enqueue(frs,
				      &target_thread_id,
				      info.minor_index,
				      info.discipline)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

        return (0);

error:

        frs_debug_message(DM_ENQUEUE, 2,
                          "[frs_user_enqueue]: FRS ENQUEUE FAILED",
			  info.controller);
        return (ret);
}

int
frs_user_join(tid_t controller, long* cminor)
{
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
        int		 ret;

	/*
	 * Make sure the caller qualifies for FRS access
	 */
	if (ret = frs_thread_access(curuthread))
		goto error;

        /*
         * Get hold of the frs object and grab a reference
         */
	frs_tid_to_threadid(controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

        if ((ret = frs_fsched_join(frs, cminor)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

	return (0);

error:
        frs_debug_message(DM_JOIN, 2, "[frs_user_join]: FAILED join",
			  controller);
        return (ret);
}

int
frs_user_start(tid_t controller)
{
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
        int		 ret;

        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

        if ((ret = frs_fsched_start(frs)) != 0) {
		/*
		 * In the event of a startup error, the FRS is
		 * destroyed by frs_fsched_start().
		 *
		 * Therefore, there is no need to drop the ref
		 * we acquired above.
		 */
                goto error;
        }

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

	return (0);

error:
      
	frs_debug_message(DM_START, 2,
                          "[frs_user_start]: FRS start FAILED",
			  controller);
        ASSERT (ret != -1);
        return (ret);
}

int
frs_user_yield(long* cminor)
{
	frs_fsched_t* frs;
        int ret;

        /*
         * Get hold of the frame scheduler managing this process.
         */
        if ((frs = frs_thread_getfrs()) == NULL) {
                ret = FRS_ERROR_PROCESS_NFSCHED;
                goto error;
        }
                       
        if ((ret = frs_fsched_state_verify_canyield(frs)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        if ((ret = frs_fsched_yield(frs, cminor)) != 0) {
                frs_fsched_decrref(frs); 
                goto error;
        }
        
        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

	return (0);

error:
        frs_debug_message(DM_YIELD, 2,
                          "[frs_user_yield]: FAILED yield", current_pid());
        return (ret);        
	
}

int
frs_user_eventintr(tid_t controller)
{
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
        int		 ret;

	extern void frs_handle_userintr(frs_fsched_t* frs);

        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

        /*
         * Is this frame scheduler really ready to receive interrupts?
         */
        if ((ret = frs_fsched_state_verify_canintr(frs)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        /*
         * Now we verify this framescheduler is accepting user interrupts
         */
	if (!(frs->intrmask & FRS_INTRSOURCE_USER)) {
                ret = FRS_ERROR_INTR_INVSOURCE;
                frs_fsched_decrref(frs);
                goto error;
	}

	/*
	 * Trigger Interrupt
	 */
	frs_handle_userintr(frs);

        frs_fsched_decrref(frs);

	return (0);

error:
        frs_debug_message(DM_UEINTR, 2,
                          "[frs_user_eventintr]: FRS User Eventintr FAILED",
			  controller);
        return (ret);
}

int
frs_user_getqueuelen(void* arg, long* len)
{
	frs_fsched_t	*frs;
	frs_queue_info_t info;
	frs_threadid_t	 control_thread_id;
        int		 ret;

	if (arg == NULL || len == NULL) {
		return (EFAULT);
	}

	if (copyin(arg, (void*)&info, sizeof(frs_queue_info_t))) {
		return (EFAULT);
	}

        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(info.controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

        if ((ret = frs_fsched_getqueuelen(frs, info.minor_index, len)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

        return (0);

error:
        frs_debug_message(DM_GETQLEN, 2,
                          "[frs_user_getqueuelen]: Getqueuelen Failed",
			  info.controller);
        return (ret); 
}

int
frs_user_readqueue(void* arg1, void* arg2, long* retlen)
{
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
	long		 len;
	tid_t		*localtids;
	frs_queue_info_t info;
        int		 ret;

	if (arg1 == NULL || arg2 == NULL) {
		return (EFAULT);
	}
	
	if (copyin(arg1, (void*)&info, sizeof(frs_queue_info_t))) {
		return (EFAULT);
	}
	
        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(info.controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if(frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

	/*
	 * We need the queue length in order to allocate memory
	 */
	if ((ret = frs_fsched_getqueuelen(frs, info.minor_index, &len)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }
        
	if (len == 0) {
                frs_fsched_decrref(frs);
                ret = FRS_ERROR_MINOR_ISEMPTY;
                goto error;
	}
		
	localtids = (tid_t*)kmem_zalloc(len * sizeof(tid_t), KM_NOSLEEP);
	if (localtids == NULL) {
                frs_fsched_decrref(frs);
                ret = ENOMEM;
                goto error;
	}

        if ((ret = frs_fsched_readqueue(frs,
					info.minor_index,
					localtids,
					len * sizeof(int))) != 0) {
                frs_fsched_decrref(frs);
                bzero(localtids, len * sizeof(tid_t));
                kmem_free(localtids, len * sizeof(tid_t));
                goto error;
        }
                
	if (copyout(localtids, arg2, len * sizeof(tid_t))) {
                frs_fsched_decrref(frs);
                bzero(localtids, len * sizeof(tid_t));
		kmem_free(localtids, len * sizeof(tid_t));
                ret = EFAULT;
                goto error;
	}

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);
        bzero(localtids, len * sizeof(tid_t));
	kmem_free(localtids, len * sizeof(tid_t));
        
	*retlen = len;
        return (0);

error:
        frs_debug_message(DM_READQ, 2,
                          "[frs_user_readqueue]: Readqueue Failed",
			  info.controller);
        return (ret); 
}

int
frs_user_premove(void* arg)
{
	frs_fsched_t	*frs;
	frs_queue_info_t info;
	frs_threadid_t	 control_thread_id;
	frs_threadid_t	 target_thread_id;
        int		 ret;

	if (arg == NULL) {
		return (EFAULT);
	}

	if (copyin(arg, (void*)&info, sizeof(frs_queue_info_t))) {
		return (EFAULT);
	}
	
        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(info.controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

	frs_tid_to_threadid(info.thread, &target_thread_id);
	if ((ret = frs_fsched_premove(frs,
				      info.minor_index,
				      &target_thread_id)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

        return (0);

error:
        frs_debug_message(DM_REMOVEQ, 2,
                          "[frs_user_premove]: Premove Failed",
			  info.controller);
        return (ret); 
}

int
frs_user_pinsert(void* arg1, void* arg2)
{
	frs_fsched_t	*frs;
	int		 basetid;
	frs_queue_info_t info;
	frs_threadid_t	 control_thread_id;
	frs_threadid_t	 target_thread_id;
        int		 ret;

	if (arg1 == NULL || arg2 == NULL) {
		return (EFAULT);
	}

	if (copyin(arg1, (void*)&info, sizeof(frs_queue_info_t))) {
		return (EFAULT);
	}

	if (copyin(arg2, (void*)&basetid, sizeof(int))) {
		return (EFAULT);
	}

        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(info.controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

	frs_tid_to_threadid(info.thread, &target_thread_id);
	if ((ret = frs_fsched_pinsert(frs,
				      info.minor_index,
				      basetid,
				      &target_thread_id,
				      info.discipline)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

        return (0);

error:
        frs_debug_message(DM_PINSERTQ, 2,
                          "[frs_user_pinsert]: Pinsert Failed",
			  info.controller);
        return (ret); 
}
	
int
frs_user_dequeue(void)
{
	return (EINVAL);
}

int
frs_user_stop(tid_t controller)
{
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
        int		 ret;

	/*
	 * All we do here is disable the reception of
	 * the real-time event interrupt
	 */
        
        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

	if ((ret = frs_fsched_stop(frs)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

        return (0);

error:
        frs_debug_message(DM_STOP, 2,
			  "[frs_user_stop]: Stop Failed", controller);
        return (ret); 
}

int
frs_user_resume(tid_t controller)
{
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
        int		 ret;
	
	/*
	 * We reenable the reception of the real-time
	 * event interrupt
	 */

        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

	if ((ret = frs_fsched_resume(frs)) != 0) {
                frs_fsched_decrref(frs);
                goto error;
        }

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

        return (0);

error:
        frs_debug_message(DM_RESUME, 2,
			  "[frs_user_resume]: Resume Failed", controller);
        return (ret); 	
}

int
frs_user_setattr(void* arg1)
{
	frs_attr_info_t	 attr_info;
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
	int		 ret = 0;

	if (arg1 == 0) {
                return (EFAULT);
	}
	
	if (COPYIN_XLATE(arg1, (void*)&attr_info, sizeof(frs_attr_info_t),
			 irix5_to_frs_attr_info, get_current_abi(), 1)) {
                return (EFAULT);
	}
	
	if (attr_info.attr <= FRS_ATTR_INVALID ||
	    attr_info.attr >= FRS_ATTR_LAST ) {
                return (FRS_ERROR_ATTR_NEXIST);
	}

        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(attr_info.controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

	switch (attr_info.attr) {
        case FRS_ATTR_RECOVERY:
        {
                /*
                 * The param field points to
                 * a structure of type frs_recv_info_t
                 */
                frs_recv_info_t recv_info;
		
                if (attr_info.param == 0) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
		
                if (copyin(attr_info.param,
			   (void*)&recv_info,
			   sizeof(frs_recv_info_t))) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
		
                if ((ret = frs_fsched_setrecovery(frs, &recv_info)) != 0) {
                        frs_fsched_decrref(frs);
                        goto error;
                }
                break;
        }
        case FRS_ATTR_SIGNALS:
        {
                frs_signal_info_t signal_info;

                /*
                 * The param field points to
                 * a structure of type frs_signal_info_t
                 */
                if (attr_info.param == 0) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
                if (copyin(attr_info.param,
			   (void *)&signal_info,
			   sizeof(frs_signal_info_t))) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
                if ((ret = frs_fsched_set_signals(frs, &signal_info)) != 0) {
                        frs_fsched_decrref(frs);
                        goto error;
                }   
                break;
        }

        default:
                cmn_err(CE_PANIC, "[frs_user_setattr]: Invalid attr\n");
                break;
	}

        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

        return (0);

error:
        frs_debug_message(DM_SETATTR, 2,
                          "[frs_user_setattr]: Setattr Failed",
                          attr_info.controller);
        return (ret); 
}
	
int
frs_user_getattr(void* arg1)
{
	frs_attr_info_t	 attr_info;
	frs_fsched_t	*frs;
	frs_threadid_t	 control_thread_id;
	int		 ret;

	if (arg1 == 0) {
		return (EFAULT);
	}
	
	if (COPYIN_XLATE(arg1, (void*)&attr_info, sizeof(frs_attr_info_t),
	    irix5_to_frs_attr_info, get_current_abi(), 1)) {
		return (EFAULT);
	}
	
	if (attr_info.attr <= FRS_ATTR_INVALID ||
	    attr_info.attr >= FRS_ATTR_LAST ) {
                return (FRS_ERROR_ATTR_NEXIST);
	}

        /*
         * Get hold of the frs object and grab a reference.
         */
	frs_tid_to_threadid(attr_info.controller, &control_thread_id);
        frs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (frs == NULL) {
                ASSERT (ret != 0);
                goto error;
        }

	switch (attr_info.attr) {
        case FRS_ATTR_RECOVERY:
        {
                /*
                 * The param field points to
                 * a structure of type frs_recv_info_t
                 */
                frs_recv_info_t recv_info;
		  
                if (attr_info.param == 0) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
		  
                if ((ret = frs_fsched_getrecovery(frs, &recv_info)) != 0) {
                        frs_fsched_decrref(frs);
                        goto error;
                }
		  
                ASSERT (recv_info.rmode > MFBERM_INVALID && recv_info.rmode < MFBERM_LAST);
		  
                if (copyout((void*)&recv_info,
			    attr_info.param, sizeof(frs_recv_info_t))) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
                break;
        }
        case FRS_ATTR_SIGNALS:
        {
                frs_signal_info_t signal_info;
                
                if (attr_info.param == 0) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
                if ((ret = frs_fsched_get_signals(frs, &signal_info)) != 0) {
                        frs_fsched_decrref(frs);
                        goto error;
                }
                if (copyout((void*)&signal_info,
                            attr_info.param,
                            sizeof(frs_signal_info_t))) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
                break;
        }
        case FRS_ATTR_OVERRUNS:
        {
                frs_overrun_info_t overrun_info;
                
                if (attr_info.param == 0) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }

                if ((ret = frs_fsched_getoverruns(frs,
						  attr_info.minor,
						  attr_info.thread,
						  &overrun_info)) != 0) {
                        frs_fsched_decrref(frs);
                        goto error;
                }

                if (copyout((void*)&overrun_info, attr_info.param,
			    sizeof(frs_overrun_info_t))) {
                        frs_fsched_decrref(frs);
                        ret = EFAULT;
                        goto error;
                }
                break;
        }
        default:
		cmn_err_tag(1780, CE_WARN, "[frs_user_getattr]: Invalid attr\n");
		break;
	}
        
        /*
         * Release the reference we have on this frame scheduler
         */
        frs_fsched_decrref(frs);

        return (0);

error:
        frs_debug_message(DM_GETATTR, 2,
                          "[frs_user_getattr]: Getattr Failed",
                          attr_info.controller);
        return (ret); 

}


/**************************************************************************
 *                                 Print Tools                            *
 **************************************************************************/


void
frs_fsched_info_print(frs_fsched_info_t* info)
{
        ASSERT (info != NULL);

        cmn_err(CE_NOTE, "Fsched Info:");
	cmn_err(CE_NOTE,
                "cpu=%d, intr_source=%d, intr_qual=%d, n_minors:%d",
		info->cpu, info->intr_source, info->intr_qualifier,
		info->n_minors);
	cmn_err(CE_NOTE,
                "controller=%d, num_slaves=%d\n",
                info->controller, info->num_slaves);
}

void
frs_queue_info_print(frs_queue_info_t* info)
{
        ASSERT (info != NULL);

        cmn_err(CE_NOTE, "Queue Info:");
        cmn_err(CE_NOTE,
                "Controller: %d, Thread: %d, Minor: %d, Discipline: 0x%x",
                info->controller, info->thread, info->minor_index,
		info->discipline);
}

void
frs_attr_info_print(frs_attr_info_t* info)
{
        ASSERT (info != NULL);

        cmn_err(CE_NOTE, "Attr Info:");
        cmn_err(CE_NOTE,
                "Controller: %d, Minor: %d, Process: %d, Attr: %d",
                info->controller, info->minor, info->thread, info->attr);
}

/**************************************************************************
 *                             ABI Conversion Tools                       *
 **************************************************************************/

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int
irix5_to_frs_fsched_info(
    enum xlate_mode mode,
    void *to,
    int count,
    register xlate_info_t *info)
{
    COPYIN_XLATE_PROLOGUE(irix5_frs_fsched_info, frs_fsched_info);
    target->cpu = source->cpu;
    target->intr_source = source->intr_source;
    target->intr_qualifier = source->intr_qualifier;
    target->n_minors = source->n_minors;
    target->controller = source->controller;
    target->num_slaves = source->num_slaves;
    return 0;
}

/*ARGSUSED*/
static int
irix5_to_frs_attr_info(
    enum xlate_mode mode,
    void *to,
    int count,
    register xlate_info_t *info)
{
    COPYIN_XLATE_PROLOGUE(irix5_frs_attr_info, frs_attr_info);
    target->controller = source->controller;
    target->minor = source->minor;
    target->thread = source->thread;
    target->attr = source->attr;
    target->param = (void *)(__psint_t)(int)(source->param);
    return 0;
}
#endif	/* _ABI64 */
	
#endif  /* EVEREST || SN0 */
