/*
 * File: partition.h
 * Purpose: Maps partition structures.
 *
 * NOTICE: This file documents an UNSUPORTED INTERFACE and is subject to 
 *         change without notice. 
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

#ifndef	_SYS_PARTITION_H
#define	_SYS_PARTITION_H

#include <sys/types.h>
#include <sys/param.h>

typedef  struct pn_s {
    nasid_t		pn_nasid;	/* Current nasid */
    unsigned short 	pn_rsvd1;
    unsigned char 	pn_state;	/* Node state */
    unsigned 		pn_cpuA:1;	/* True if CPU A installed */
    unsigned 		pn_cpuB:1;	/* True if CPU B installed */
    unsigned 		:6;		/* Reserved, must be 0 */
    partid_t		pn_partid;	/* Partition ID owning node */
    unsigned char	pn_domainid;	/* Domain owning partition */
    unsigned short	pn_clusterid;	/* Cluster ID of partition */
    unsigned short	pn_cellid;	/* Cell ID of partition */
    moduleid_t		pn_module;	/* Module ID */
    unsigned short	pn_ncells;	/* Total # cells */
    unsigned short 	pn_rsvd2[6];	/* Reserved, must be 0 */
} pn_t;

typedef	struct part_info_s {
    partid_t	pi_partid;		/* partition ID */
    __uint32_t	pi_flags;		/* status flags */
#   define	PI_F_ACTIVE	0x01	/* active partition */
    __uint32_t	pi_reserverd[8];	/* reserved for future use */
} part_info_t;

/*
 * Operations for SYSSGI system call: SGI_PART_OPERATIONS
 */

#define	SYSSGI_PARTOP_GETPARTID		1 /* get partition ID */
#define	SYSSGI_PARTOP_NODEMAP		2 /* get system node map */
#define SYSSGI_PARTOP_PARTMAP		3 /* get specific partition info */
#define	SYSSGI_PARTOP_DEACTIVATE	4 /* deactivate a partition */
#define	SYSSGI_PARTOP_ACTIVATE		5 /* activate partitions */

#endif
