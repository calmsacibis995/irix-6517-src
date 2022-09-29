/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.12 $"

#include <sys/types.h>
#include <sys/time.h>
#include <ksys/childlist.h>
#include <sys/idbgentry.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <ksys/pid.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>

#include "pid_private.h"
#include "pproc_private.h"
#include "vproc_private.h"

static void idbg_pidentry(__psint_t);
static void idbg_pidactive(__psint_t);
static void idbg_pidfree(__psint_t);
static void idbg_childlist(__psint_t);
static void idbg_vproc(__psint_t);

#if CELL
#include <ksys/cell/handle.h>
#include <ksys/cell/service.h>
#include "cell/dsproc.h"
#include "cell/dcproc.h"

void idbg_dsproc(dsproc_t *);
void idbg_dcproc(dcproc_t *);
#endif

extern short	pidfirstfree;
extern short	pidlastfree;
extern pid_active_t	pid_active_list;

void
procidbg_init(void)
{
	idbg_addfunc("pidentry", (void (*)())idbg_pidentry);
	idbg_addfunc("pidactive", (void (*)())idbg_pidactive);
	idbg_addfunc("pidfree", (void (*)())idbg_pidfree);
	idbg_addfunc("childlist", idbg_childlist);
	idbg_addfunc("vproc", idbg_vproc);

#if CELL
	idbg_addfunc("dsproc", idbg_dsproc);
	idbg_addfunc("dcproc", idbg_dcproc);
#endif
}

static char *wcode_str[] = {
	"",
	"CLD_EXITED",
	"CLD_KILLED",
	"CLD_DUMPED",
	"CLD_TRAPPED",
	"CLD_STOPPED",
	"CLD_CONTINUED"
};

void
idbg_childlist(__psint_t x)
{
	proc_t		*p;
	child_pidlist_t **list;

	if (x == -1L) {
		p = curprocp;
		if (p == NULL) {
			qprintf("no current process\n");
			return;
		}
	} else if (x < 0L) {
		p = (proc_t *) x;
		if (!procp_is_valid(p)) {
			qprintf("WARNING:0x%x is not a valid proc address\n", p);
		}
	} else if ((p = idbg_pid_to_proc(x)) == NULL) {
		qprintf("%d is not an active pid\n", (pid_t)x);
		return;
	}

	list = &p->p_childpids;

	while (*list) {
		uint_t	wcode = (unsigned)(*list)->cp_wcode;
		qprintf("child %d:", (*list)->cp_pid);
		switch (wcode) {
		case CLD_EXITED:
		case CLD_KILLED:
		case CLD_DUMPED:
			qprintf(" wcode %s", wcode_str[wcode]);
			qprintf(" wdata %d xstat %d",
				(*list)->cp_wdata, (*list)->cp_xstat);
			break;
		case CLD_STOPPED:
		case CLD_CONTINUED:
			qprintf(" wcode %s", wcode_str[wcode]);
			qprintf(" pgsigseq %d", (*list)->cp_pgsigseq);
			break;
		}
		qprintf("\n");
		list = &((*list)->cp_next);
	}
}

static void
print_pidentry(
	pid_entry_t	*pe)
{
	qprintf("\tEntry @ 0x%x pid %d\n", pe, pe->pe_pid);
	if (pe->pe_busy) {
		qprintf("\tBusy%s%s%s%s", 
			(pe->pe_sess ? " session" : ""),
			(pe->pe_pgrp ? " pgrp" : ""),
			(pe->pe_batch ? " batch" : ""),
			(pe->pe_proc ? " proc" : ""));
		if (pe->pe_vproc)
			qprintf(" vproc 0x%x", pe->pe_vproc);
		qprintf("\n");
	} else
		qprintf("Idle\n");
}

void
idbg_pidentry(__psint_t p)
{
	pid_slot_t	*ps;
	pid_entry_t	*pe;
	pid_t		pid;

	if (p == -1L) {
		qprintf("pidentry requires a pid or address\n");
		return;
	}
	if (p < 0L) {
		ps = (pid_slot_t *)p;
		if (ps < pidtab || ps > (pidtab + pidtabsz)) {
			qprintf("Invalid pid slot address\n");
			return;
		}
		pid = ps->ps_pid;
	} else {
		ps = PID_SLOT(p);
		pid = p;
	}
	qprintf("pid %d maps to 0x%x (index %d)\n",
		(pid_t)pid, ps, PID_INDEX(p));
	if (!pidslot_isactive(ps))
		qprintf("\tforw %d back %d\n", ps->ps_forw, ps->ps_back);
	else
		qprintf("\tBusycnt %d\n", ps->ps_busycnt);
	for (pe = ps->ps_chain; pe; pe = pe->pe_next)
		print_pidentry(pe);
}

/* ARGSUSED */
void
idbg_pidactive(__psint_t p)
{
	pid_entry_t	*pe;

	for (pe = (pid_entry_t *)kqueue_first(&pid_active_list.pa_queue);
	     pe != (pid_entry_t *)kqueue_end(&pid_active_list.pa_queue);
	     pe = (pid_entry_t *)kqueue_next(&pe->pe_queue)) {
		if (pe->pe_vproc)
			print_pidentry(pe);
	}
}

/* ARGSUSED */
void
idbg_pidfree(__psint_t p)
{
	int	i;
	int	cnt;

	for (i = pidfirstfree, cnt = 0;
	     i != -1;
	     i = pidtab[i].ps_forw, cnt++) {
		if (cnt > pidtabsz) {
			qprintf("\tFREE LIST CORRUPT!\n");
			return;
		}
		idbg_pidentry(i);
	}
	qprintf("%d free out of %d, head %d tail %d\n", cnt, pidtabsz,
			pidfirstfree, pidlastfree);
}

void
idbg_vproc_bhv_print(
	vproc_t	*vp)
{
	bhv_desc_t	*bhv;

	bhv = VPROC_BHV_FIRST(vp);

	switch (BHV_POSITION(bhv)) {
#if CELL
	case	VPROC_BHV_DS:
		idbg_dsproc(BHV_PDATA(bhv));
		return;
	case	VPROC_BHV_DC:
		idbg_dcproc(BHV_PDATA(bhv));
		return;
#endif
	case	VPROC_BHV_PP:
		shortproc(BHV_PDATA(bhv), 1);
		return;
	default:
		qprintf("Unknown behavior position %d\n", BHV_POSITION(bhv));
	}
}

void
idbg_vproc(__psint_t p)
{
	pid_t	pid;
	vproc_t	*vp;

	if (p == -1L) {
		pid = current_pid();
		if (pid == 0) {
			qprintf("no current process\n");
			return;
		}
		vp = idbg_vproc_lookup(pid);
	} else if (p < 0L) {
		vp = (vproc_t *)p;
		pid = vp->vp_pid;
		vp = idbg_vproc_lookup(pid);
		if ((vp == VPROC_NULL) || (vp->vp_pid != pid))
			qprintf("vproc 0x%x cannot be looked up\n", p);
		vp = (vproc_t *)p;
	} else {
		pid = (pid_t)p;
		vp = idbg_vproc_lookup(pid);
	}
	if ((vp == VPROC_NULL) || (vp->vp_pid != pid)) {
		qprintf("%d is not an active pid\n", pid);
		return;
	}
	qprintf("Vproc pid %d @ 0x%x\n", vp->vp_pid, vp);
	qprintf("\tref %d, lock 0x%x\n", vp->vp_refcnt, &vp->vp_refcnt_lock);
	qprintf("\tstate 0x%x, sv 0x%x\n", vp->vp_state, &vp->vp_cond);
	qprintf("\tbehavior head 0x%x\n", VPROC_BHV_HEAD(vp));
	idbg_vproc_bhv_print(vp);
}

#if CELL
void
idbg_dcproc(
	dcproc_t	*dcp)
{
	if (dcp == (dcproc_t *)-1LL)
		return;

	qprintf("dcproc 0x%x\n", dcp);
	qprintf("token client 0x%x, bhv 0x%x\n", dcp->dcp_tclient,
			&dcp->dcp_bhv);
	qprintf("Handle 0x%x (0x%x{0x%x,0x%x},0x%x)\n", &dcp->dcp_handle,
			HANDLE_TO_SERVICE(dcp->dcp_handle),
			SERVICE_TO_CELL(HANDLE_TO_SERVICE(dcp->dcp_handle)),
			SERVICE_TO_SVCNUM(HANDLE_TO_SERVICE(dcp->dcp_handle)),
			HANDLE_TO_OBJID(dcp->dcp_handle));
}

void
idbg_dsproc(
	dsproc_t	*dsp)
{
	if (dsp == (dsproc_t *)-1LL)
		return;

	qprintf("dsproc 0x%x\n", dsp);
	qprintf("mutex 0x%x flags 0x%x\n", &dsp->dsp_lock, dsp->dsp_flags);
	qprintf("token state - client 0x%x, server 0x%x\n", dsp->dsp_tclient,
				dsp->dsp_tserver);
	qprintf("bhv 0x%x\n", &dsp->dsp_bhv);
	shortproc(BHV_PDATA(BHV_NEXT(&dsp->dsp_bhv)), 1);
}
#endif	/* CELL */
