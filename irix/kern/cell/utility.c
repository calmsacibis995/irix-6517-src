/*
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
 */

#ident $Id: utility.c,v 1.1 1997/09/08 13:48:26 henseler Exp $

#include <sys/types.h>
#include <ksys/cell.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/pda.h>
#include <sys/runq.h>
#include <ksys/cell/service.h>
#include <ksys/cell/subsysid.h>
#include <ksys/cell/cell_hb.h>
#include "invk_utility_stubs.h"
#include "I_utility_stubs.h"
#include <sys/page.h>
#ifdef	NUMA_BASE
#include "sys/SN/SN0/bte.h"
#endif

caddr_t	*time_sync_page;

void
utility_init()
{
	caddr_t *counter;

#ifdef NUMA_BASE
	pgno_t	pfn;
	/* REFERENCED */
	pfd_t *pfd;
	bte_handle_t *bte_handle;
	counter = kvpalloc(1, VM_DIRECT|VM_UNCACHED_PIO, 0);

	if (counter) {
		pfn = kvtophyspnum(counter);
		pfd = pfntopfdat(pfn);

		/*
		 * Poison the page so that it is never accessed cached.
		 * also avoids speculative accesses.
		 */
		bte_handle = bte_acquire(btoct(NBPP));

		bte_poison_copy(bte_handle, K1_TO_PHYS(counter),
				K0_TO_PHYS(counter), NBPP, BTE_POISON);
		bte_release(bte_handle);
		/* page_poison(pfd); */
	}
#else
	counter = kvpalloc(1, VM_DIRECT, 0);
#endif
	time_sync_page = counter;
	mesg_handler_register(utility_msg_dispatcher, UTILITY_SUBSYSID);
}

typedef struct {
	long	flag;
	timespec_t tv;
} intercell_time_t;

/* ARGSUSED */
void
phost_set_time(
	bhv_desc_t	*bdp,
	cell_t		source_cell)
{
	service_t	svc;
	volatile intercell_time_t *timep;
	int ospl, already;
	cpu_cookie_t was_running;
	pfd_t *pfd;
	pgno_t	pfn;

	if (cellid() == source_cell) {
		return;
	}

	pfn = kvtophyspnum(time_sync_page);
	pfd = pfntopfdat(pfn);
	export_page(pfd, source_cell, &already);

	timep = (intercell_time_t *)time_sync_page;
	SERVICE_MAKE(svc, source_cell, SVC_UTILITY);
	timep->flag = 0;
	was_running = setmustrun(clock_processor);
	invk_utility_get_time (svc, (void *)timep);

	ospl = spl7();	/* same as nanotime */

	/*
	 * WARNING:
	 * Need recovery support here - other cell could go down
	 * Could spin for a while, then look for cell,
	 * or resend message
	 */
	while (timep->flag == 0)
		;

	local_settime(timep->tv.tv_sec, timep->tv.tv_nsec);
	splx(ospl);
	restoremustrun(was_running);

}

extern int      _wbadaddr_val(volatile void *, int, volatile long *);
/*
 * Push the local time back to the remote cell (safely)
 * Push the time values frist, then the flag
 */
void
I_utility_get_time (
	void	*tp)
{
	timespec_t	tv;
	volatile intercell_time_t *timep = tp;
	long flag = 1, usec;
	int ospl;

	ospl = spl7();	/* same as nanotime */
	nanotime(&tv);
	usec = tv.tv_nsec / 1000;

	_wbadaddr_val(&timep->tv.tv_sec, sizeof(tv.tv_sec), (long *)&tv.tv_sec);
	_wbadaddr_val(&timep->tv.tv_nsec, sizeof(tv.tv_nsec), &usec);
	_wbadaddr_val(&timep->flag, sizeof(timep->flag), &flag);
	splx(ospl);
}

/*
 * At boot time, attempt to syhcnronize local time with global
 * time.
 */
void
cell_time_sync()
{
	/* Sue me... */
	phost_set_time (NULL, CELL_GOLDEN);
}
