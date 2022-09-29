/*
 * os/numa/pmo_process.c
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <ksys/as.h>
#include <sys/systm.h>
#include <sys/prctl.h>
#include <sys/pfdat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/kthread.h>
#include <sys/sysmp.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <sys/runq.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "memsched.h"
#include "pmo_process.h"
#include "os/proc/pproc_private.h"


pmo_handle_t
pmo_xface_process_mldlink(mldlink_info_t* mldlink_info_arg)
{
        vproc_t* vpr;
        proc_t* procp;
        mld_t* mld;
        mldlink_info_t mldlink_info;
        kthread_t* kt;

        if (copyin((void*)mldlink_info_arg,
                   (void*)&mldlink_info,
                   sizeof(mldlink_info_t))) {
                return (PMOERR_EFAULT);
        }
                   
        /*
         * if mldlink_info.pid == 0, the the request refers to the current process
         */

	if (mldlink_info.pid == 0) {
		mldlink_info.pid = current_pid();
        }

	vpr = VPROC_LOOKUP(mldlink_info.pid);
	if (vpr == NULL) {
		ASSERT(mldlink_info.pid != current_pid());
		return (PMOERR_ESRCH);
	}
	VPROC_GET_PROC(vpr, &procp);

        if ((mld = (mld_t*)pmo_ns_find(curpmo_ns(),
                                       mldlink_info.mld_handle,
                                       __PMO_MLD)) == NULL) {
		VPROC_RELE(vpr);
                return (PMOERR_INV_MLD_HANDLE);
        }

        /*
         * Has the mld been placed
         */
        if (mld_getnodeid(mld) == CNODEID_NONE) {
                /*
                 * Release the ref acquired by pmo_ns_find
                 */
                pmo_decref(mld, pmo_ref_find);
		VPROC_RELE(vpr);
                return (PMOERR_NOTPLACED);
        }

        /*
         * If a cpu has been specified, mustrun the process
         */
        if (mldlink_info.lcpuid != MLDLINK_ANYCPU) {

                if (mldlink_info.lcpuid < 0 || mldlink_info.lcpuid >= CPUS_PER_NODE) {
                        pmo_decref(mld, pmo_ref_find);
                        VPROC_RELE(vpr);
                        return (PMOERR_EINVAL);
                }

                if (mp_mustrun(mldlink_info.pid,
			       cnode_slice_to_cpuid(mld_getnodeid(mld),
						    mldlink_info.lcpuid),
                               0,          /* no lock */
                               0)) {       /* no relocation */
                        pmo_decref(mld, pmo_ref_find);
                        VPROC_RELE(vpr);
                        return (PMOERR_EINVAL);
                }
        }
        
	uscan_hold(&procp->p_proxy);
        kt = UT_TO_KT(prxy_to_thread(&procp->p_proxy));
        
#ifdef SOMETIME_LATER
        /*
         * setnoderun isn't doing the right thing for now.
         * we will ignore the MANDATORY request until
         * the next release...
         */
        if (mldlink_info.rqmode == RQMODE_MANDATORY) {
                if (!cpu_enabled(cnodetocpu(mld_getnodeid(mld)))) {
                        VPROC_RELE(vpr);
                        return (PMOERR_EDEADLK);
                }
                setnoderun(UT_TO_KT(prxy_to_thread(&procp->p_proxy)),
				mld_getnodeid(mld));
                aspm_proc_affset(kt, mld, mld_to_bv(mld));
        } else {
                aspm_proc_affset(kt, mld, mld_to_bv(mld));
        }
#endif
        
        aspm_proc_affset(kt, mld, mld_to_bv(mld));
	uscan_rele(&procp->p_proxy);

        /*
         * Release the ref acquired by pmo_ns_find.
         * aspm_proc_affset incs the mld reference to account
         * for the reference from p_mldlink.
         */
        pmo_decref(mld, pmo_ref_find);        
        VPROC_RELE(vpr);

        return (0);
      
}

void
pmo_process_mustrun_relocate(struct uthread_s* uthreadp, cnodeid_t dest_node)
{
        mld_t* mld;
        kthread_t* kt;
        int s;

        ASSERT(uthreadp);
        
        /*
         * We replace the mld if it's not already on the
         * mustrun node
         */

        kt = UT_TO_KT(uthreadp);
        s = kt_lock(kt);
        mld = kt->k_mldlink;
        ASSERT_ALWAYS(mld);
        pmo_incref(mld, pmo_ref_keep);
        kt_unlock(kt, s); 

        if (mld_getnodeid(mld) == dest_node) {
                /*
                 * We're all set.The mld was already placed on the mustrun node
                 */
                pmo_decref(mld, pmo_ref_keep);
                return;
        }

        physmem_mld_place_onnode(mld, dest_node);
        aspm_proc_affset(kt, mld, mld_to_bv(mld));        
        pmo_decref(mld, pmo_ref_keep);

        
}





