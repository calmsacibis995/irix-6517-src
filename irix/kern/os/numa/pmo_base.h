/*
 * os/numa/pmo_base.h
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

#ifndef __NUMA_PMO_BASE_H__
#define __NUMA_PMO_BASE_H__

#include <sys/mmci.h>
#include <sys/numa.h>

#ifdef MMCI_REFCD

typedef struct rop_info {
        void*   rop;
        char*   file;
        int     line;
        int     refcount;
        int     refcop;
        struct rop_info* next;
} rop_info_t;

typedef struct refcd_record {
        void*       pmo;
        rop_info_t* rop_info;
} refcd_record_t;

#define REFCD_TRACE_LEN (1024*12)

typedef struct refcd_trace {
        refcd_record_t record[REFCD_TRACE_LEN];
        int            index;
        int            active;
        lock_t         lock;
} refcd_trace_t;

extern refcd_trace_t refcd_trace;

#define pmo_base_refcd_trace_lock() mutex_spinlock(&refcd_trace.lock)
#define pmo_base_refcd_trace_unlock(s) mutex_spinunlock(&refcd_trace.lock, (s))

extern void pmo_base_refcd_trace_init(void);
extern void pmo_base_refcd_trace_activate(void);
extern void  pmo_base_refcd_trace_stop(void);
extern void pmo_base_refcd_trace_clean(void);

#endif /* MMCI_REFCD */

extern void pmo_base_init(void* pmo_arg, pmo_type_t type, pmo_method_t destructor);


#endif /* __NUMA_PMO_BASE_H__ */
