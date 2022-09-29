/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_cell.h,v $
 * Revision 65.1  1997/10/20 19:17:22  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.15.1  1996/10/02  17:07:20  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:40  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_cell.h,v 65.1 1997/10/20 19:17:22 jdoak Exp $ */

#ifndef	TRANSARC_CM_CELLH_
#define TRANSARC_CM_CELLH_

#include <cm_server.h>

/*
 * Cell module related macros
 */
#define	CL_TIMEOLDCELL	(5*60*60)	/* don't check down servers (5 hrs) */

/*
 * All information regarding a given cell
 */
struct cm_cell {
    struct cm_cell *next;		/* Next cell on linked list */
    osi_dlock_t lock;                   /* cell data-lock */
    struct cm_server
      *cellHosts[MAXVLHOSTSPERCELL];    /* DB servers (VLDB) for this cell */
    u_long hostGen;      		/* generation count on server array */
    long   lastIdx;                     /* last server (index) contacted */
    u_long lastIdxHostGen;              /* server gen for lastIdx */
    u_long lastIdxAddrGen;              /* address gen for lastIdx */
    u_long lastIdxTimeout;              /* re-eval timeout for lastIdx */
    afs_hyper_t cellId;	       	        /* unique id assigned by cm (high) */
    char *cellNamep;		       	/* char string name of cell */
    char *oldCellNamep;	       	        /* initial char string name of cell */
    u_long lastUsed;			/* when the cell was MRU */
    short cellIndex;		   	/* relative index number per cell */
    /* char states; */		     	/* state flags */
    char lclStates;			/* flags not set by user programs */
};

/*
 * cell's lclStates bits
 */
#define	CLL_IDSET	1		/* if the cell-ID field is set */
#define	CLL_LCLCELL	2	/* if this is the ``local cell'' */

/* 
 * Exported by cm_cell.c 
 */
extern struct cm_cell *cm_cells;
extern struct lock_data cm_celllock;
extern char cm_localCellName[];

extern struct cm_cell *cm_GetCellByName _TAKES((char *));
extern struct cm_cell *cm_GetCell _TAKES((afs_hyper_t *));
extern struct cm_cell *cm_GetCellByIndex _TAKES((long));
extern struct cm_cell *cm_NewCell _TAKES((char *, long, afsUUID *, int,
					  char **, struct sockaddr_in *));
extern long cm_FindCellID _TAKES((struct cm_cell *));

#endif /* TRANSARC_CM_CELLH_ */
