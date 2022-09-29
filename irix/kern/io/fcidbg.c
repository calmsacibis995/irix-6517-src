/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if EVEREST || SN0 || IP30
#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/graph.h>
#include <sys/scsi.h>

#define SH_SG_DESCRIPTOR void
#include <sys/fcadp.h>

void	idbg_fc(uintptr_t);
extern int fcadp_init_time;

struct fc_idbg_func
{
	char	*name;
	void	(*func)();
	char	*descrip;
};

struct fc_idbg_func fcidbg_funcs[] = {
    "fc", idbg_fc, "Print fcadp driver info",
    0, 0, 0
};


void
fcidbg_init(void)
{
	struct fc_idbg_func	*fp;

	for (fp = fcidbg_funcs; fp->name; fp++)
		idbg_addfunc(fp->name, fp->func);
}

/*
++	kp fc ctlr - print data structures for fcadp controller
 */
void
idbg_fc(uintptr_t ctlr)
{
	struct fcadpctlrinfo *ci;
	struct fcadptargetinfo *ti;
	struct fcadpluninfo *li;
	vertex_hdl_t lun_vhdl;
#pragma weak fcadpctlr
	extern struct fcadpctlrinfo *fcadpctlr[];
#pragma weak fcadpalloc
	extern fcadpalloc();
	int i;

	if (fcadpalloc == NULL)
		return;

	ci = fcadpctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("fcadp ctlr %d does not exist\n", ctlr);
		return;
	}
	qprintf("fcadp%d: ctlrinfo @ 0x%x, pciconfig @ 0x%x\n",
		ctlr, ci, ci->config_addr);
	qprintf("ibar0 @ 0x%x  ibar1 @ 0x%x\n", ci->ibar0_addr, ci->ibar1_addr);

	if (ci->quiesce)
		qprintf("[quiesce %d]", ci->quiesce);
	if (ci->lip_wait)
		qprintf("[waiting to receive lip]");
	if (ci->lip_issued)
		qprintf("[lip TCB issued]");
	if (ci->lipmap_valid)
		qprintf("[lipmap valid]");

	if (ci->primbusy)
		qprintf("[primitive busy]");
	if (ci->userprim)
		qprintf("[user primitive active]");
	if (fcadp_init_time)
		qprintf("[fcadp_init_time %d]", fcadp_init_time);
	if (ci->bigmap_busy)
		qprintf("[big DMA map busy]");

	if (ci->off_line)
		qprintf("[off line]");
	if (ci->dead)
		qprintf("[dead]");
	if (ci->intr_state)
		qprintf("[intr_state %d]");
	if (ci->LoS)
		qprintf("[LOS]");

	if (ci->intr)
		qprintf("[intr]");
	if (ci->scsi_cmds)
		qprintf("[scsi cmd done]");

	qprintf(" host id %d\nlip_attempts %d, lip_timeout %d, lipcount %d\n",
		ci->host_id, ci->lip_attempt, ci->lipcount);
	qprintf("timeout_interval %d, timeout_id %d, lip_timeout %d\n",
		ci->timeout_interval, ci->timeout_id, ci->lip_timeout);
	qprintf("commands active/hiwater %d/%d, max SCSI commands %d\n",
		ci->cmdcount, ci->hiwater, ci->maxqueue);
	qprintf("quiesce timeout %d, quiesce time %d, error_id %d\n",
		ci->quiesce_timeout, ci->quiesce_time, ci->error_id);

	qprintf("lnksvc_queue head/tail 0x%x/0x%x\n",
		ci->lnksvc_queue.head, ci->lnksvc_queue.tail);
	qprintf("scsi_queue head/tail 0x%x/0x%x\n",
		ci->scsi_queue.head, ci->scsi_queue.tail);
	qprintf("tcbhdr ptr array 0x%x, free tcbhdr 0x%x\n",
		ci->tcbptrs, ci->freetcb);
	qprintf("tcb queue 0x%x, tcbindex %d (0x%x)\n",
		ci->tcbqueue, ci->tcbindex,
		(uintptr_t) ci->tcbqueue + 128 * ci->tcbindex);

	qprintf("primitive tcbhdr 0x%x, lip tcbhdr 0x%x\n",
		ci->prim_tcb, ci->lip_tcb);
	qprintf("config 0x%x, control_blk 0x%x\ndoneq 0x%x, portname 0x%x\n",
		ci->config, ci->control_blk, ci->doneq, ci->portname);
	qprintf("bigmap 0x%x, lipmap 0x%x\nstatus_addr 0x%x, reqchain 0x%x\n",
		ci->bigmap, ci->lipmap, ci->status_addr, ci->req_complete_chain);
	for (i = 0; i < FCADP_MAXTARG; i++)
	{
		ti = ci->target[i];
		if (ti != NULL)
		{
			qprintf("target %d @ 0x%x: q_state %d",
				ti->number, ti, ti->q_state);
			if (ti->needplogi)
				qprintf(" needplogi");
			if (ti->plogi)
				qprintf(" plogi");
			if (ti->needprli)
				qprintf(" needprli");
			if (ti->prli)
				qprintf(" prli");
			if (ti->needadisc)
				qprintf(" needadisc");
			qprintf("\n  timeout_progress %d\n",
				ti->timeout_progress);
			qprintf("  waithead/tail 0x%x/0x%x, scsi_next 0x%x\n",
				ti->waithead, ti->waittail, ti->scsi_next);
			qprintf("  lnksvc_next 0x%x, abts_next 0x%x\n",
				ti->lnksvc_next, ti->abts_next);
			qprintf("  first active TCB 0x%x, aborting TCB 0x%x\n",
				ti->active, ti->abort);
			qprintf("  plogi/prli payload 0x%x/0x%x\n  lunmask 0x%x\n",
				ti->plogi_payload, ti->prli_payload, ti->lunmask);
			
			lun_vhdl = scsi_lun_vhdl_get(ci->ctlrvhdl, i, 0);
			if (lun_vhdl == GRAPH_VERTEX_NONE)
				continue;
			li = SLI_INFO(scsi_lun_info_get(lun_vhdl));
			if (li != NULL)
			{
				qprintf("  lun 0: exclusive %d, refcount %d"
				       " sense_callback 0x%x\n", li->exclusive,
				       li->refcount, li->sense_callback);
				qprintf("    inq data 0x%x, sense 0x%x, "
				       "maxq %d, qdepth %d\n",
				       li->tinfo.si_inq, li->tinfo.si_sense,
				       li->tinfo.si_maxq, li->tinfo.si_qdepth);
			}
		}
	}
}
#endif /* EVEREST || SN0 || IP30 */
