
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-3b2:fs/procfs/prmachdep.c	1.5"*/
#ident  "$Revision: 1.52 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/fpu.h>
#include <sys/param.h>
#include <sys/reg.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>	/* XXX for sigset_t, required by ucontext.h */
#include <sys/kucontext.h>
#include <ksys/exception.h>
#include <sys/cmn_err.h>
#include <sys/uthread.h>
#include <sys/vnode.h>		/* for pvnode_t */
#include "procfs.h"
#include "prdata.h"		/* prototypes for prmachdep.c */

/*
 * Is there floating-point support?
 */
int
prhasfp(void)
{
	return 1;
}

/*
 * Set the PC to the specified virtual address.
 */
/* ARGSUSED */
void
prsvaddr(uthread_t *ut, caddr_t vaddr)
{
	ut->ut_exception->u_eframe.ef_epc = (k_machreg_t) vaddr;
}
