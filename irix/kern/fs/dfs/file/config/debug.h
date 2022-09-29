/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1994, 1991, 1990 Transarc Corporation - All rights reserved. */

/* debug.h -- declares various macros used as debugging aid.
 *
 */

#if !defined(TRANSARC_CONFIG_DEBUG_H)
#define TRANSARC_CONFIG_DEBUG_H

#if !defined(KERNEL)
#include <stdio.h>
#endif /* !defined(KERNEL) */

/*
 * Define debugging levels, from 1 being lowest priority and 7 being
 * highest priority.  DEBUG_LEVEL_0 and DEBUG_FORCE_PRINT mean to
 * print the message regardless of the current setting.
 */
#define DEBUG_LEVEL_0		0
#define DEBUG_LEVEL_1		01
#define DEBUG_LEVEL_2		03
#define DEBUG_LEVEL_3		07
#define DEBUG_LEVEL_4		017
#define DEBUG_LEVEL_5		037
#define DEBUG_LEVEL_6		077
#define DEBUG_LEVEL_7		0177

#define DEBUG_FORCE_PRINT	DEBUG_LEVEL_0

/*
 * Debug modules
 *   This stuff is apparently only used in the kernel. -ota 901105
 */
#define	CM_DEBUG	0	/* Cache Manager */
#define	PX_DEBUG	1	/* Protocol Exporter */
#define	HS_DEBUG	2	/* Host Module */
#define	VL_DEBUG	3	/* Volume Module */

#define	AG_DEBUG	4	/* Aggregate Module */
#define	VR_DEBUG	5	/* Volume Registry Module */
#define	RX_DEBUG	6	/* RPC/Rx Module */
#define FP_DEBUG	7	/* Free Pool Module */

#define	NFSTR_DEBUG	8	/* NFS/AFS Translator Module */
#define XCRED_DEBUG	9	/* Extended Credential Module */
#define	XVFS_DEBUG	10	/* Xvnode Module */
#define	XGLUE_DEBUG	11	/* Glue layer */

#define ACL_DEBUG	12	/* ACL Module */
#define FSHS_DEBUG	13	/* File server host module */

#define MAXMODS_DEBUG	20

#if defined(AFS_DEBUG) && defined(KERNEL)
/*
 *  Should get here only once per kernel instance!
 */
#ifndef AFSDEBUG_ARRAY
#define AFSDEBUG_ARRAY
char afsdebug[MAXMODS_DEBUG];
#else
extern char afsdebug[20];
#endif /* AFSDEBUG_ARRAY */
#define	AFSLOG(index, level, str) \
    if ((afsdebug[(index)] & (level)) || !(level)) (osi_printf str)
#else
#define	AFSLOG(index, level, str)
#endif /* AFS_DEBUG & KERNEL */

/*
 * Debugging macro package.  The actual variables should be declared in
 * debug.c
 */

#if defined(AFS_DEBUG)
#if defined(lint)
#define dprintf(flag, str) osi_printf str
#define dlprintf(flag, level, str) osi_printf str
#define dmprintf(flag, bit, str) osi_printf str
#else /* lint */

#if defined(KERNEL)
#define dprintf(flag, str) \
      (void)((flag) ? \
	     (osi_printf str, \
	      osi_printf("\t%s, %d\n", __FILE__, __LINE__), 0) : 0)
#else /* defined(KERNEL) */
/* flush the debugging output stream outside the kernel */
#define dprintf(flag, str) \
      (void)((flag) ? \
	     (osi_printf str, \
	      osi_printf("\t%s, %d\n", __FILE__, __LINE__), fflush(stdout)):0)
#endif /* defined(KERNEL) */

#define dlprintf(flag, level, str) dprintf((flag) >= (level), str)
#define dmprintf(flag, bit, str) dprintf((flag) & (1 << ((bit) - 1)), str)

#endif /* lint */

#else /* AFS_DEBUG */

#define dprintf(flag, str)
#define dlprintf(flag, level,str)
#define dmprintf(flag, bit, str)

#endif /* AFS_DEBUG */

#endif /* TRANSARC_CONFIG_DEBUG_H */

