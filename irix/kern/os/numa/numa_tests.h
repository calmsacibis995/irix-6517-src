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

#ifndef __NUMA_NUMA_TESTS_H__
#define __NUMA_NUMA_TESTS_H__

#define NUMATEST_MAX_NODES 64
#define NUMATEST_HWPAGES   4

typedef struct refcounter_set {
        uint counter[NUMATEST_MAX_NODES];
} refcounter_set_t;                          

typedef struct refcounter_info {
        pfn_t            pfn;
        refcounter_set_t set[NUMATEST_HWPAGES];
} refcounter_info_t;
        
typedef struct mld_plac_info {
        pmo_handle_t  handle;
        uint          nodeid;
        uint          radius;
        __uint64_t    size;
} mld_plac_info_t;
        




#endif /* __NUMA_NUMA_TESTS_H__ */
