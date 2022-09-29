/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: ag_init.h,v $
 * Revision 65.4  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.3  1998/04/01 14:16:41  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added extern declaration for ag_RegisterVolumes().
 *
 * Revision 65.2  1997/12/16  17:05:49  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:20:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.425.1  1996/10/02  21:11:12  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:21  damon]
 *
 * Revision 1.1.420.2  1994/06/09  14:25:19  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:37:17  annie]
 * 
 * Revision 1.1.420.1  1994/02/04  20:35:56  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:20:31  devsrc]
 * 
 * Revision 1.1.418.1  1993/12/07  17:38:02  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  18:11:53  jaffe]
 * 
 * Revision 1.1.3.3  1993/01/21  16:32:35  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  18:26:24  cjd]
 * 
 * Revision 1.1.3.2  1993/01/13  19:52:03  shl
 * 	Transarc delta: jdp-ot6139-fts-restartability 1.3
 * 	  Selected comments:
 * 	    This delta makes it possible to restart interrupted fts operations.
 * 	    In particular, it addresses weaknesses of the clone and delete operations.
 * 	    Prior to this delta, those operations could leave the low-level fileset
 * 	    backing chains in an inconsistent state, making cleanup impossible.  These
 * 	    problems were addressed with the introduction of a new volop,
 * 	    VOL_BULKSETSTATUS, that allows backing chains and mulitiple volume IDs to be
 * 	    updated atomically.
 * 	    Made changes to mark partially cloned volumes with a new flag
 * 	    (VOL_CLONEINPROG).  Closing this delta to fix an episode/anode bug, but
 * 	    hopefully this is the last revision of this delta.
 * 	    Define value for AGOP_SYNC.
 * 	    Fix problem with setting a volume's status after it has been destroyed.
 * 	[1993/01/12  22:35:23  shl]
 * 
 * Revision 1.1  1992/01/19  02:51:29  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/* Copyright (C) 1989, 1990 Transarc Corporation - All rights reserved */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xaggr/RCS/ag_init.h,v 65.4 1999/07/21 19:01:14 gwehrman Exp $ */

#ifndef	_AG_INITH_
#define	_AG_INITH_
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/aggr.h>

/*
 * Basic aggregate ops system calls
 */
#define	AGOP_STAT		1 
#define	AGOP_VOLCREATE		2
#define	AGOP_VOLINFO		3
#define	AGOP_DETACH		4
#define	AGOP_ATTACH		5

/*
 * Auxiliary aggregate related system calls
 */
#define	AGOP_SCAN		6
#define	AGOP_AGGINFO		7
#define	AGOP_AGGENUMERATE	8
#define	AGOP_VOLENUMERATE	9
#define AGOP_SYNC		10


struct aggrList {			/* Identical to volser_aggrList */
    char	name[32];
    char	dvname[32];
#ifdef SGIMIPS
/* SGIMIPS - struct aggrList has an corresponding struct ftserver_aggrList 
 *	     (defined in ftserver_data.idl) in ftserver module. The following 
 *          changes are done to match those definitions.
 */
    unsigned int	Id;
    unsigned int	type;
    unsigned int	spare1;
    unsigned int	spare2;
    unsigned int	spare3;
    unsigned int	spare4;
#else
    unsigned long	Id;
    unsigned long	type;
    unsigned long	spare1;
    unsigned long	spare2;
    unsigned long	spare3;
    unsigned long	spare4;
#endif /* SGIMIPS */
};

struct aggrInfo {			/* Identical to volser_aggrInfo */
    char	name[32];
    char	devName[32];
#ifdef SGIMIPS
/* SGIMIPS - struct aggrInfo has an corresponding struct ftserver_aggrInfo 
 *	     (defined in ftserver_data.idl) in ftserver module. The following 
 *          changes are done to match those definitions.
 */
    int		type;
    int		totalUsable;
    int		realFree;
    int		minFree;
    int		spare[6];
#else
    long	type;
    long	totalUsable;
    long	realFree;
    long	minFree;
    long	spare[6];
#endif /* SGIMIPS */
    char	cspare[64];
};

/*
 * Globals/functions exported by ag_init.c
 */
extern void ag_setops(int entry, struct aggrops *ops);
#ifdef SGIMIPS
extern int ag_RegisterVolumes(struct aggr *aggrp);
#endif
#endif	/* _AG_INITH_ */
