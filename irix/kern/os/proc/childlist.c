/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.8 $"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/proc.h>
#include <ksys/childlist.h>
#include "pproc_private.h"

struct zone *child_pid_zone;

void
child_pidlist_init()
{
	child_pid_zone = kmem_zone_init(sizeof(child_pidlist_t), "child pids");
}

/* Add pid to proc p's list of children */
void
child_pidlist_add(proc_t *p, pid_t pid)
{
	child_pidlist_t *cpid;

	cpid = kmem_zone_zalloc(child_pid_zone, KM_SLEEP);
	cpid->cp_pid = pid;

	mutex_lock(&p->p_childlock, PZERO);

	cpid->cp_next = p->p_childpids;
	p->p_childpids = cpid;

	mutex_unlock(&p->p_childlock);
}

/* Remove pid from proc p's list of children */
void
child_pidlist_delete(proc_t *p, pid_t pid)
{
	child_pidlist_t **list;
	child_pidlist_t *cpid;

	mutex_lock(&p->p_childlock, PZERO);

	list = &p->p_childpids;

	while (*list && (*list)->cp_pid != pid) {
		list = &((*list)->cp_next);
	}
	ASSERT(*list);
	ASSERT((*list)->cp_pid == pid);

	cpid = *list;
	*list = cpid->cp_next;

	mutex_unlock(&p->p_childlock);

	kmem_zone_free(child_pid_zone, cpid);
}
