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

#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/exec.h>
#include <ksys/vproc.h>
#include <ksys/pid.h>
#include <sys/idbgentry.h>
#include "vproc_private.h"
#include "pproc_private.h"

void
vproc_init(void)
{
	vproc_startup();
}

vproc_t *
vproc_lookup(
	pid_t	pid,
	int	flag)
{
	vproc_t	*vpr;

	vpr = pid_to_vproc(pid, flag);
	return(vpr);
}

void
vproc_lookup_prevent(
	vproc_t		*vp)
{
	PID_PROC_FREE(vp->vp_pid);
}

int
vproc_exec(
	vproc_t		*vp,
	struct uarg	*args)
{
	int error;

	/* Once we call VPROC_EXEC, any subsequent errors are cleaned
	 * up on the 'physical' side - i.e. pproc_exec. Thus we clear
	 * out the uarg cleanup bits, to indicate to upper layers that
	 * necessary cleanup has been handled.
	 */
	args->ua_exec_cleanup = 0;

	VPROC_EXEC(vp, args, error);
	return (error);
}

/* ARGSUSED */
int 
vproc_is_local(
	vproc_t		*vp)
{
	return (1);
}

/* ARGSUSED */
int
remote_procscan(
        int     (*pfunc)(proc_t *, void *, int),
        void    *arg)
{
	return(0);
}

vproc_t *
idbg_vproc_lookup(pid_t pid)
{
	vproc_t	*vpr;

	vpr = idbg_pid_to_vproc(pid);
	return(vpr);
}

/* ARGSUSED */
int
idbg_remote_procscan(
        int     (*pfunc)(proc_t *, void *, int),
        void    *arg)
{
	return(0);
}
