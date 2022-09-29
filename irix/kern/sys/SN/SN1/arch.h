/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN1_ARCH_H__
#define __SYS_SN_SN1_ARCH_H__

/*
 * This is the maximum number of nodes that can be part of a kernel.
 * Effectively, it's the maximum number of compact node ids (cnodeid_t).
 */
#define MAX_COMPACT_NODES	64

/*
 * This is the maximum number of NASIDS that can be present in a system.
 * (Highest NASID plus one.)
 */
#define MAX_NASIDS		2048

/*
 * MAXCPUS refers to the maximum number of CPUs in a single kernel.
 * This is not necessarily the same as MAXNODES * CPUS_PER_NODE
 */
#define MAXCPUS			128

/*
 * MAX_REGIONS refers to the maximum number of hardware partitioned regions.
 */
#define	MAX_REGIONS		64

/*
 * MAX_PARITIONS refers to the maximum number of logically defined 
 * partitions the system can support.
 */
#define MAX_PARTITIONS		MAX_REGIONS


#define NASID_MASK_BYTES	((MAX_NASIDS + 7) / 8)

#if defined (PSEUDO_SN1)
/*
 * Slot constants for SN1
 */

#define MAX_MEM_SLOTS   8                      /* max slots per node */

#define SLOT_SHIFT      	(29)
#define SLOT_MIN_MEM_SIZE	(32*1024*1024)

#else /* PSEUDO_SN1 */

#error <BOMB: Need to get the correct SN1 stuff here>

#endif /* PSEUDO_SN1 */


#endif __SYS_SN_SN1_ARCH_H__
