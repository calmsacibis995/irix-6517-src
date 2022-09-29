#ifndef	_SYS_REPL_VNODE_H
#define	_SYS_REPL_VNODE_H

/******************************************************************************
 * repl_vnode.h
 *
 * Defines the vnode replication interface exported to outside world
 * Only interface that needs to be exported is the mechanism to 
 * create & attach replication attribute to vnode, and free them when 
 * the vnode is being reclaimed.
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
 ******************************************************************************/

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#ifdef	NUMA_REPLICATION
struct pfdat;


/* 
 * Routine to initialize replicate specific data structures.
 * This routine is called once at system boot time. 
 * It's repsonsible for setting up
 * data structures needed for replication (both policy and mechanism).
 */

void    repl_init(void);

/*
 * Interface to interpose/dispose vnode replication layer.
 */
extern void repl_interpose(struct vnode *, char *);
extern void repl_dispose(struct vnode *);

#else   /* !NUMA_REPLICATION */

#define	repl_init()
#define	repl_interpose(vp, name)
#define repl_dispose(vp)

#endif  /* NUMA_REPLICATION */


#ifdef	NUMA_REPLICATION
extern struct pfdat *repl_pfind(struct vnode *, pgno_t, int, void *, struct pfdat *);
#else
#define	repl_pfind(vp, pg, flag, pm, pfdp)
#endif	/* NUMA_REPLICATION */

#endif /* C || C++ */
#endif /* _SYS_REPL_VNODE_H */
