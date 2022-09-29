/*****************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
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

#ifndef __NUMA_MEMFIT_H__
#define __NUMA_MEMFIT_H__

#define MEMP_NBUCKETS 8

typedef struct mem_pressure {
        cnodemask_t bucket[MEMP_NBUCKETS];
        pfn_t       baselv[MEMP_NBUCKETS];
        pfn_t       cm_freemem;
} mem_pressure_t;


typedef struct memfit {
        mem_pressure_t           mem_pressure;
        volatile cnodemask_t     mem_unassigned;
        uint                     mem_mean_assign;
        int                      count;
        cnodeid_t                nrotor[MAXCPUS+2];
} memfit_t;
        

extern memfit_t global_memfit;
extern void memfit_master_update(pfn_t cm_freemem);
extern cnodeid_t memfit_selectnode(cnodemask_t);

extern cnodeid_t memfit_unassigned_getnode(cnodemask_t allow_mask);


#endif /* __NUMA_MEMFIT_H__ */
