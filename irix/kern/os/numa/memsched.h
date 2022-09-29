/*
 * os/numa/memsched.h
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


#ifndef __NUMA_MEMSCHED_H__
#define __NUMA_MEMSCHED_H__

extern int physmem_mldset_place(mldset_t* mldset);
extern void physmem_mld_place(mld_t* mld, cnodeid_t center, cnodemask_t skipbv);
extern int physmem_mld_place_here(mld_t* mld);
extern int physmem_maxradius(void);
extern void physmem_mld_place_onnode(mld_t* mld, cnodeid_t node);
extern void* physmem_select_masked_neighbor_node(cnodeid_t center,
                                          int radius,
					  cnodemask_t cnodemask,
                                          cnodeid_t* neighbor,
                                          selector_t selector,
                                          void* arg1,
                                          void* arg2);

#define SKIPN(skipbv, candidate) CNODEMASK_TSTB((skipbv), (candidate))
#define bin_to_gray(n) ((n)^((n)>>1))
#ifdef SN0XXL
#define MAX_NODES 256
#else
#define MAX_NODES 128
#endif

extern int log2_nodes;

extern cnodeid_t cube_to_cnode[MAX_NODES];
extern cnodeid_t cnode_to_cube[MAX_NODES];


 
#endif /* __NUMA_MEMSCHED_H_ */
