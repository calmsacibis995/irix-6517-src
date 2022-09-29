/*
 * Copyright 1999, Silicon Graphics, Inc.
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

#ifndef _mem_h
#define _mem_h

#define MEM_DATA_BARS		3
#define	MEM_DATA_SIZE(nodes)	 ((nodes) * (MEM_DATA_BARS))

extern int nodemem_map[];	/* Mem (in MB) configured on each node. */
extern int max_nodenum;		/* Largest node number found in search. */
extern int max_nodemem;		/* Largest memory found on a node. */

extern int get_nodemem_map(void);	/* Get per-node phys memory mapping. */

extern u_char *mem_graph(void);	/* Fill array of mem values for graphing. */

#endif /* _mem_h */
