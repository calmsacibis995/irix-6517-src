/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.7 $"

#ifndef _KSYS_CHILDLIST_H_
#define _KSYS_CHILDLIST_H_

#include <sys/types.h>
#include <sys/kmem.h>
#include <ksys/vpgrp.h>

struct proc;
extern void child_pidlist_add(struct proc *, pid_t);
extern void child_pidlist_delete(struct proc *, pid_t);

typedef struct child_pidlist {
	struct	child_pidlist *cp_next;	/* next child of proc */
	struct timeval	cp_utime;		/* utime of process at exit */
	struct timeval	cp_stime;		/* stime of process at exit */
	pid_t	cp_pid;			/* childs pid */
	pid_t	cp_pgid;		/* pgid of process at event - may
					 * not be accurate if child has
					 * continued.
					 */
	sequence_num_t	cp_pgsigseq;	/* pgrp signal sequence num */
	short	cp_wcode;		/* why child has stopped/exited */
	short	cp_wdata;		/* what signal caused event */
	short	cp_xstat;		/* exit status of child */
} child_pidlist_t;

extern struct zone *child_pid_zone;

#endif	/* _KSYS_CHILDLIST_H_ */
