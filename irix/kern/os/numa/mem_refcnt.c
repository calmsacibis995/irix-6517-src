/*****************************************************************************
 * Copyright 1997, Silicon Graphics, Inc.
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
 ****************************************************************************/

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <ksys/ddmap.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <sys/sat.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/idbgentry.h>
#include <sys/sysmacros.h>
#include <sys/pda.h>
#include <sys/timers.h>
#include <ksys/sthread.h>
#include <sys/schedctl.h>
#include <sys/nodepda.h>
#include <sys/hubspc.h>
#include <sys/iograph.h>
#include <sys/invent.h>
#include <sys/hwgraph.h>
#include <ksys/mem_refcnt.h>
#include <sys/SN/hwcntrs.h>
#include "pfms.h"
#include "numa_hw.h"
#include "migr_control.h"

int
mem_refcnt_attach(dev_t hub)
{
        dev_t refcnt_dev;
        
        hwgraph_char_device_add(hub,
                                "refcnt",
                                "hubspc_", 
				&refcnt_dev);
        device_info_set(refcnt_dev, (void*)(ulong)HUBSPC_REFCOUNTERS);

        return (0);
}


/*ARGSUSED*/
int
mem_refcnt_open(dev_t *devp, mode_t oflag, int otyp, cred_t *crp)
{
        cnodeid_t node;
        
        ASSERT( (hubspc_subdevice_t)(ulong)device_info_get(*devp) == HUBSPC_REFCOUNTERS );

        if (!cap_able(CAP_MEMORY_MGT)) {
                return (EPERM);
        }

        node = master_node_get(*devp);

        ASSERT( (node >= 0) && (node < numnodes) );

        if (NODEPDA(node)->migr_refcnt_counterbuffer == NULL) {
                return (ENODEV);
        }

        ASSERT( NODEPDA(node)->migr_refcnt_counterbase != NULL );
        ASSERT( NODEPDA(node)->migr_refcnt_cbsize != NULL );

        return (0);
}

/*ARGSUSED*/
int
mem_refcnt_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
        return 0;
}

/*ARGSUSED*/
int
mem_refcnt_mmap(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
        cnodeid_t node;
        int errcode;
        char* buffer;
        size_t blen;
        
        ASSERT( (hubspc_subdevice_t)(ulong)device_info_get(dev) == HUBSPC_REFCOUNTERS );

        node = master_node_get(dev);

        ASSERT( (node >= 0) && (node < numnodes) );

        ASSERT( NODEPDA(node)->migr_refcnt_counterbuffer != NULL);
        ASSERT( NODEPDA(node)->migr_refcnt_counterbase != NULL );
        ASSERT( NODEPDA(node)->migr_refcnt_cbsize != 0 );

        /*
         * XXXX deal with prot's somewhere around here....
         */

        buffer = NODEPDA(node)->migr_refcnt_counterbuffer;
        blen = NODEPDA(node)->migr_refcnt_cbsize;

        /*
         * Force offset to be a multiple of sizeof(refcnt_t)
         * We round up.
         */

        off = (((off - 1)/sizeof(refcnt_t)) + 1) * sizeof(refcnt_t);

        if ( ((buffer + blen) - (buffer + off + len)) < 0 ) {
                return (EPERM);
        }

        errcode = v_mapphys(vt,
                            buffer + off,
                            len);

        return errcode;
}

/*ARGSUSED*/
int
mem_refcnt_unmap(dev_t dev, vhandl_t *vt)
{
        return 0;
}

/* ARGSUSED */
int
mem_refcnt_ioctl(dev_t dev,
                 int cmd,
                 void *arg,
                 int mode,
                 cred_t *cred_p,
                 int *rvalp)
{
        cnodeid_t node;
        int errcode;
        
        ASSERT( (hubspc_subdevice_t)(ulong)device_info_get(dev) == HUBSPC_REFCOUNTERS );

        node = master_node_get(dev);

        ASSERT( (node >= 0) && (node < numnodes) );

        ASSERT( NODEPDA(node)->migr_refcnt_counterbuffer != NULL);
        ASSERT( NODEPDA(node)->migr_refcnt_counterbase != NULL );
        ASSERT( NODEPDA(node)->migr_refcnt_cbsize != 0 );

        errcode = 0;
        
        switch (cmd) {
        case RCB_INFO_GET:
        {
                rcb_info_t rcb;
                
                rcb.rcb_len = NODEPDA(node)->migr_refcnt_cbsize;
                
                rcb.rcb_sw_sets = NODEPDA(node)->migr_refcnt_numsets;
                rcb.rcb_sw_counters_per_set = numnodes;
                rcb.rcb_sw_counter_size = sizeof(refcnt_t);

                rcb.rcb_base_pages = NODEPDA(node)->migr_refcnt_numsets /
                                     NUM_OF_HW_PAGES_PER_SW_PAGE();  
                rcb.rcb_base_page_size = NBPP;
                rcb.rcb_base_paddr = ctob(slot_getbasepfn(node, 0));
                
                rcb.rcb_cnodeid = node;
                rcb.rcb_granularity = MD_PAGE_SIZE;
                rcb.rcb_hw_counter_max = MIGR_COUNTER_MAX_GET(node);
                rcb.rcb_diff_threshold = MIGR_THRESHOLD_DIFF_GET(node);
                rcb.rcb_abs_threshold = MIGR_THRESHOLD_ABS_GET(node);
                rcb.rcb_num_slots = node_getnumslots(node);

                if (copyout(&rcb, arg, sizeof(rcb_info_t))) {
                        errcode = EFAULT;
                }

                break;
        }
        case RCB_SLOT_GET:
        {
                rcb_slot_t slot[MAX_MEM_SLOTS];
                int s;
                int nslots;

                nslots = node_getnumslots(node);
                ASSERT(nslots <= MAX_MEM_SLOTS);
                for (s = 0; s < nslots; s++) {
                        slot[s].base = (__uint64_t)ctob(slot_getbasepfn(node, s));
                        slot[s].size  = (__uint64_t)ctob(slot_getsize(node, s));
                }
                if (copyout(&slot[0], arg, nslots * sizeof(rcb_slot_t))) {
                        errcode = EFAULT;
                }
                
                *rvalp = nslots;
                break;
        }
                
        default:
                errcode = EINVAL;
                break;

        }
        
        return errcode;
}




        
        
