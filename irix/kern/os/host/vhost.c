/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: vhost.c,v 1.3 1997/06/05 21:54:43 sp Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>

#include "vhost_private.h"
#include "phost_private.h"

#ifdef DEBUG
struct ktrace	*vhost_trace = NULL;
cell_t		vhost_trace_id = -1;
static void	idbg_vhost(__psint_t); 
static void	idbg_vhost_trace(__psint_t);
#endif

void
vhost_init(void)
{
#ifdef DEBUG
	/* This test avoids multiple initialization for unikernel */
	if (vhost_trace == NULL) {
		idbg_addfunc("vhost", idbg_vhost);
		idbg_addfunc("vhosttrace", idbg_vhost_trace);
		vhost_trace = ktrace_alloc(1000, 0);
	}
#endif
	vhost_cell_init();		/* Distributed initialization */
}

vhost_t *
vhost_create()
{
	vhost_t	*vhp;

	vhp = (vhost_t *) kmem_alloc(sizeof(vhost_t), KM_SLEEP);
	ASSERT(vhp);
	bhv_head_init(&vhp->vh_bhvh, "vhost");
	phost_create(vhp);
	return vhp;
}

#ifdef DEBUG
/* ARGSUSED */
static void
idbg_vhost(__psint_t x)
{
	vhost_t	*vhp = vhost_local();
	qprintf("vhost 0x%x for cell %d\n", vhp, cellid());
	qprintf("    behavior head 0x%x:\n", &vhp->vh_bhvh);
	idbg_vhost_bhv_print(vhp);
}

static void
idbg_vhost_trace(__psint_t x)
{
	__psint_t	id;
	int		idx;
	int		count;

	if (x == -1) {
		qprintf("Displaying all entries\n");
		idx = -1;
		count = 0;
	} else if (x < 0) {
		idx = -1;
		count = (int)-x;
		qprintf("Displaying last %d entries\n", count);
	} else {
		qprintf("Displaying entries for cell %d\n", x);
		idx = 1;
		id = x;
		count = 0;
	}
		
	ktrace_print_buffer(vhost_trace, id, idx, count);
}
#endif
