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

#ident "$Id: phost.c,v 1.8 1997/09/08 13:56:34 henseler Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <ksys/vsession.h>
#include <sys/vfs.h>
#include <ksys/cell.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/sysget.h>
#include <sys/cred.h>
#include <sys/ktime.h>
#include <sys/runq.h>

#include "../proc/pproc_private.h"

#include "vhost_private.h"
#include "phost_private.h"

/* Forward references to physical op routines herein */
void	phost_register(bhv_desc_t *, int);
void	phost_deregister(bhv_desc_t *, int);
void	phost_killall(bhv_desc_t *, int, pid_t, pid_t, pid_t);
void	phost_reboot(bhv_desc_t *, int, char *);
void	phost_sync(bhv_desc_t *, int);
void	phost_credflush_wait(bhv_desc_t *, cell_t);

vhost_ops_t phost_ops = {
	BHV_IDENTITY_INIT_POSITION(VHOST_BHV_PP),
	phost_register,
	phost_deregister,
	phost_sethostid,
	phost_gethostid,
	phost_sethostname,
	phost_gethostname,
	phost_setdomainname,
	phost_getdomainname,
	phost_killall,
	phost_reboot,
	phost_sync,
	phost_sysget,
	phost_systune,
	phost_credflush,
	phost_credflush_wait,
	phost_sysacct,
	phost_set_time,
	phost_adj_time,
};

void
phost_create(
	vhost_t *vhp)
{
	phost_t		*php;

	php = (phost_t *) kmem_alloc(sizeof(phost_t), KM_SLEEP);
	ASSERT(php);

	bhv_desc_init(&php->ph_bhv, php, vhp, &phost_ops);
	bhv_insert_initial(&vhp->vh_bhvh, &php->ph_bhv);
}

/* ARGSUSED */
void
phost_register(
	bhv_desc_t	*bdp,
	int		vh_svc_mask)
{
	panic("phost_register: should not be here");
}

/* ARGSUSED */
void
phost_deregister(
	bhv_desc_t	*bdp,
	int		vh_svc_mask)
{
	panic("phost_deregister: should not be here");
}

/*
 * Arguments block for killallscan().
 */
typedef struct {
	int	signo;
	pid_t	pgid;
	pid_t	callers_pid;
	pid_t	callers_sid;
} killallargs_t;
	
/*
 * Routine called when rebooting the machine to kill all outstanding
 * processes.
 */
static int
killallscan(proc_t *p, void *arg, int mode)
{
	killallargs_t *ka;

	if (mode != 1)
		return 0;
	ka = arg;
	if (ka->signo < 0) {
		/* A_SHUTDOWN */
		ASSERT(ka->signo == -SIGKILL);
		if (p->p_pid != ka->callers_pid && p->p_pid > 1)
			sigtopid(p->p_pid, SIGKILL, SIG_ISKERN, 0, 0, 0);
	} else {
		/* A_KILLALL */
		if (p->p_pgid != ka->pgid && p->p_pid > 1 &&
		    (ka->callers_sid != p->p_sid ||
		     !ancestor(p->p_pid, ka->callers_pid)))
			sigtopid(p->p_pid, ka->signo, SIG_ISKERN, 0, 0, 0);
	}
	return 0;
}

/* ARGSUSED */
void
phost_killall(
	bhv_desc_t	*bdp,
	int		signo,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid)		/* 0 => global SIGKILL */
{
	killallargs_t	args;

	args.signo	 = signo;
	args.pgid	 = pgid;
	args.callers_pid = callers_pid;
	args.callers_sid = callers_sid;

	procscan(killallscan, (void *) &args);
}

/* ARGSUSED */
void
phost_reboot(
	bhv_desc_t	*bdp,
	int		fcn,
	char		*mdep)
{
	mdboot(fcn, mdep);
}

/* ARGSUSED */
void
phost_sync(
	bhv_desc_t	*bdp,
	int		flags)
{
	vfs_syncall(flags);
}

/* ARGSUSED */
void
phost_credflush(
	bhv_desc_t	*bdp,
	cell_t		server)
{
#ifdef CELL
	credid_flush_cache(server);
#else
	ASSERT(0);
#endif
}

#if !CELL_IRIX
/* ARGSUSED */
void
phost_set_time(
	bhv_desc_t	*bdp,
	cell_t		source_cell)
{
}
#endif

/* ARGSUSED */
void	
phost_credflush_wait(bhv_desc_t *bhp, cell_t cell)
{
	panic("phost_credflush_wait: should not be here");
}

/* ARGSUSED */
void
phost_adj_time(
	bhv_desc_t	*bdp,
	long		adjustment,
	long		*odelta)
{
	cpu_cookie_t was_running;

	was_running = setmustrun(clock_processor);
	*odelta = doadjtime(adjustment);
	restoremustrun(was_running);
}

#ifdef DEBUG
void
idbg_phost(__psint_t x)
{
	phost_t	*phost = (phost_t *) x;

	if (PHOST_TO_VHOST(phost) != vhost_local())
		qprintf("0x%x invalid for cell %d, 0x%x expected\n",
			phost, cellid(), vhost_local());
	else
		qprintf("phost 0x%x\n    bhv 0x%x\n", phost, &phost->ph_bhv);
}	
#endif
