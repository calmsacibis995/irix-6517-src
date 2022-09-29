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

#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <ksys/behavior.h>
#include <sys/idbgentry.h>
#include <ksys/kqueue.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include "pproc_private.h"
#include "vsession_private.h"

static void idbg_vsession(__psint_t);
static void idbg_vsessionvn(__psint_t);
static void idbg_vsession_bhv_print(vsession_t *);

#if CELL
#include "cell/dsession.h"

void idbg_dssession_print(dssession_t *);
void idbg_dcsession_print(dcsession_t *);
#endif

void
sessionidbg_init(void)
{
	idbg_addfunc("vsession", idbg_vsession);
	idbg_addfunc("vsessionvn", idbg_vsessionvn);
}

/*
 * Dump	the psession structure and associated procs
 */
static void
idbg_psession(__psint_t x)
{
	psession_t *psp = (psession_t *) x;
	qprintf("psession 0x%x:\n", x);
	qprintf("    &wait 0x%x &lock 0x%x\n", &psp->se_wait, &psp->se_lock);
	qprintf("    flag 0x%x memcnt 0x%x\n", psp->se_flag, psp->se_memcnt);
	qprintf("    ttycnt 0x%x ttyvp 0x%x ttycall 0x%x\n",
		psp->se_ttycnt, psp->se_ttyvp, psp->se_ttycall);
	qprintf("    ttycallarg 0x%x\n", psp->se_ttycallarg);
}

vsession_t *
idbg_vsession_lookup(
	pid_t	sid)
{
	vsessiontab_t	*vq;

	if (sid < 0 || sid > MAXPID)
		return NULL;
	vq = VSESSION_ENTRY(sid);
	return vsession_qlookup_locked(&vq->vsesst_queue, sid);
}

static void
vsessionprint(vsession_t *vsession, vsessiontab_t *vq)
{
	qprintf("vsession 0x%x:\n", vsession);
	qprintf("    sid %d ref %d vq 0x%x\n",
		vsession->vs_sid, vsession->vs_refcnt, vq);
	qprintf("    behavior head 0x%x\n", &vsession->vs_bhvh);
	idbg_vsession_bhv_print(vsession);
}

static void
idbg_vsession(__psint_t x)
{
	kqueue_t	*kq;
	int		i;
	vsessiontab_t	*vq;
	vsession_t	*vsession;

	if (x == -1) {
#if DEBUG
		extern long vsession_limbo;
#endif

		qprintf("Dumping vsession table for cell %d\n", cellid());
#if DEBUG
		qprintf("  %ld sessions marked for dealloc\n", vsession_limbo);
#endif
		for (i = 0; i < vsessiontabsz; i++) {
			vq = &vsessiontab[i];
			kq = &vq->vsesst_queue;
			/*
			 * for every element on this hash queue
			 */
			for (vsession = (vsession_t *)kqueue_first(kq);
			     vsession != (vsession_t *)kqueue_end(kq);
			     vsession = (vsession_t *)kqueue_next(&vsession->vs_queue)) {
				vsessionprint(vsession, vq);
			}
		}
	} else if (x < 0) {
		vsession = (vsession_t *)x;
		vsessionprint(vsession, NULL);
		if (vsession != idbg_vsession_lookup(vsession->vs_sid)) {
			qprintf("vsession 0x%x, sid %d cannot be looked-up\n",
				vsession, vsession->vs_sid);
		}
	} else { /* x > 0 */
		vsession = idbg_vsession_lookup((pid_t) x);
		if (vsession != NULL)
			vsessionprint(vsession, NULL);
	}
}

static void
idbg_vsessionvn(__psint_t x)
{
	kqueue_t	*kq;
	int		i;
	vsessiontab_t	*vq;
	vsession_t	*vsession;

	qprintf("Searching vsession table for vnode/0x%x on cell %d\n",
								x, cellid());

	for (i = 0; i < vsessiontabsz; i++) {
		vq = &vsessiontab[i];
		kq = &vq->vsesst_queue;
		/*
		 * for every element on this hash queue
		 */
		for (vsession = (vsession_t *)kqueue_first(kq);
		     vsession != (vsession_t *)kqueue_end(kq);
		     vsession = (vsession_t *)kqueue_next(&vsession->vs_queue)){

			bhv_desc_t	*bhv;

			bhv = VSESSION_TO_FIRST_BHV(vsession);

			if (BHV_POSITION(bhv) == VSESSION_POSITION_PS) {
				psession_t *psp = (psession_t *)
						      (__psint_t)BHV_PDATA(bhv);

				if (psp->se_ttyvp == (vnode_t *)x)
					qprintf(
				    "  psession for sid %d using vnode 0x%x\n",
							vsession->vs_sid, x);
			}
		}
	}
}

static void
idbg_vsession_bhv_print(
	vsession_t	*vsp)
{
	bhv_desc_t	*bhv;

	bhv = VSESSION_TO_FIRST_BHV(vsp);

	switch (BHV_POSITION(bhv)) {
#if CELL
	case VSESSION_POSITION_DS:
		idbg_dssession_print(BHV_PDATA(bhv));
		return;
	case VSESSION_POSITION_DC:
		idbg_dcsession_print(BHV_PDATA(bhv));
		return;
#endif
	case VSESSION_POSITION_PS:
		idbg_psession((__psint_t)BHV_PDATA(bhv));
		return;
	default:
		qprintf("Unknown behavior position %d\n", BHV_POSITION(bhv));
	}
}

#if CELL
void
idbg_dcsession_print(
	dcsession_t	*dcp)
{
	qprintf("dcsession 0x%x:\n", dcp);
	qprintf("    token state - client 0x%x\n", dcp->dcsess_tclient);
	qprintf("    bhv 0x%x\n", &dcp->dcsess_bhv);
	idbg_psession((__psint_t)BHV_PDATA(BHV_NEXT(&dcp->dcsess_bhv)));
}

void
idbg_dssession_print(
	dssession_t	*dsp)
{
	qprintf("dssession 0x%x:\n", dsp);
	qprintf("    token state - client 0x%x, server 0x%x\n",
		dsp->dssess_tclient, dsp->dssess_tserver);
	qprintf("    notify_idle %d\n", dsp->dssess_notify_idle);
	qprintf("    bhv 0x%x\n", &dsp->dssess_bhv);
	idbg_psession((__psint_t)BHV_PDATA(BHV_NEXT(&dsp->dssess_bhv)));
}
#endif	/* CELL */
