/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if defined(EVEREST)

#define	SCIP_ITHREADS

#include <bstring.h>
#include <string.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/invent.h>
#include <sys/dmamap.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/immu.h>
#include <sys/edt.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/errno.h>
#include <sys/scsi.h>
#include <sys/scip.h>
#include <sys/wd95a.h>
#include <sys/wd95a_struct.h>
#include <sys/failover.h>
#include <io/scipfw.c>
#ifdef DEBUG
#include <sys/idbgentry.h>
#endif

extern int		  scip_maxadap;	/* number of highest numbered scip */
extern ushort		  scip_dmapages[];
extern u_char		  scip_bus_flags[];
extern scuzzy_t		  scuzzy[];
extern uint		  scip_hostid;
extern char		  arg_wd95hostid[];
extern uint		  scip_mintimeout;
int			  scipinittime = 0;
int			  scipdumpmode = 0;
struct scipctlrinfo	 *scipctlr[SCSI_MAXCTLR];
#define splscsi splhi
#define SCEXT(ctlr) (((ctlr) >> 3) * 10 + ((ctlr) & 7))

static void sciptimeout(struct sciprequest *, int, struct scipctlrinfo *);
static int scip_init(struct scipctlrinfo *);
static void scipreturn(struct scipctlrinfo *, register struct scipluninfo *, int);
static int scipstart(struct scipctlrinfo *);
static void scip_reset(struct scipctlrinfo *, int, int);
extern int scsi_is_external(const int);
static int scipabort(struct scsi_request *);

#define SCIPDEBUG 0
#if SCIPDEBUG
static void scip_status(int ctlr);
int	scipdebug;
#endif

/*
 * Macros for accessing scip registers
 */
#define SCIP_REG(number, ci) (*((volatile uint *) (ci->chip_addr + number)))
#define SCIP_ADDR(number, base) (*((volatile uint *) (base + number)))

#ifdef SCIP_ITHREADS
#define INITLOCK(l,n,s)	init_mutex(l, MUTEX_DEFAULT, n, s)
#define INITNLOCK(l,n)	mutex_init(l, MUTEX_DEFAULT, n)
#define LOCK(l)		mutex_lock(&(l), PRIBIO)
#define UNLOCK(l)	mutex_unlock(&(l))
#define IOLOCK(l,s)	mutex_lock(&(l), PRIBIO)
#define IOUNLOCK(l,s)	{wbflush(); mutex_unlock(&(l));}
#define SPL_DECL(s)
#else
#define INITLOCK(l,n,s)	init_spinlock(l, n, s)
#define INITNLOCK(l,n)	initnlock(l, n)
#define LOCK(l)		spsema(l)
#define UNLOCK(l)	svsema(l)
#define IOLOCK(l,s)	s = io_splockspl(l, splscsi)
#define IOUNLOCK(l,s)	io_spunlockspl(l, s)
#define SPL_DECL(s)	int s;
#endif /* SCIP_ITHREADS */

typedef	struct scipctlrinfo scipctlrinfo_t;

/*
 * scipreq management routines
 * Manage scipreq's.  There is one for each possible outstanding command.
 */
static struct sciprequest *
scipreq_get(struct scipctlrinfo *ci)
{
	struct sciprequest      *scipreq;

	scipreq = ci->request;
	if (scipreq != NULL)
		ci->request = scipreq->next;
	return scipreq;
}

static void
scipreq_put(struct scipctlrinfo *ci, int targ, struct sciprequest *scipreq)
{
	struct scipunitinfo *ui;

	ui = ci->unit[targ];
	if (scipreq == &ui->unitreq)
		ui->scipreqbusy = 0;
	else
	{
		scipreq->next = ci->request;
		ci->request = scipreq;
	}
}


/*
 * sciptimostart - start any waiting commands
 * Used if we could not start a command in scipstart due to lack of a
 * resource.  We set timeout to make sure command isn't left hanging.
 */
void
sciptimostart(struct scipctlrinfo *ci)
{
	SPL_DECL(pri)

	IOLOCK(ci->lock, pri);
	ci->restartpend = 0;
	while (scipstart(ci)) ;
	IOUNLOCK(ci->lock, pri);
}


static int
scipabort(struct scsi_request *req)
{
	req = req;
	return 1;
}

/*
 * scip command wait list management
 * Maintain a linked list of commands waiting to be issued and luns waiting
 * to issue commands.  Every time a command is issued to a lun, move that lun
 * to the end of the wait list.
 */
static void
scipenqueue(struct scipctlrinfo *ci, struct scipluninfo *li, struct scsi_request *req)
{
	if (li->waithead == NULL)
	{
		li->waithead = req;
		if (ci->waithead == NULL)
			ci->waithead = li;
		else
			ci->waittail->nextwait = li;
		ci->waittail = li;
	}
	else
		li->waittail->sr_ha = req;
	li->waittail = req;
}

static struct scsi_request *
scipdequeue(register struct scipctlrinfo *ci, struct sciprequest **sreqptr)
{
	register struct scipunitinfo	*ui;
	register struct scipluninfo	*li = NULL;
	register struct scsi_request	*req = NULL;
	struct sciprequest		*scipreq = NULL;
	struct scipluninfo		*saveli;
	void				*lastpage = NULL;

	saveli = ci->waittail;
	while (scipreq == NULL && li != saveli)
	{
		li = ci->waithead;
		ASSERT(li != NULL);
		req = li->waithead;

		/*
		 * If we have an odd-length request, try to allocate a page for
		 * sending DMA to.  If we can't set a timer to try again.
		 */
		if ((req->sr_flags & SRF_DIR_IN) && (req->sr_buflen & 1))
		{
			lastpage = kvpalloc(1, VM_NOSLEEP, 0);
			if (lastpage == NULL)
			{
				if (!ci->restartpend)
				{
					ci->restartpend = 1;
					timeout(sciptimostart, ci, HZ/4);
				}
				goto skip_lun; /* most efficient way */
			}
		}

		/*
		 * If the request size fits in a standard DMA map, try
		 * to allocate a scipreq from the floating pool.
		 * If the DMA is too large, or there is nothing available
		 * in the floating pool, try to get the per-target map.
		 * If it's busy too, free the lastpage buffer if necessary.
		 */
		if ((req->sr_flags & SRF_MAPUSER) ||
		    (io_numpages(req->sr_buffer, req->sr_buflen) <=
						scip_dmapages[ci->number]))
		{
			scipreq = scipreq_get(ci);
		}
		if (scipreq == NULL)
		{
			ui = ci->unit[req->sr_target];
			if (!ui->scipreqbusy)
			{
				scipreq = &ui->unitreq;
				ui->scipreqbusy = 1;
			}
		}
		if (scipreq == NULL && lastpage != NULL)
		{
			kvpfree(lastpage, 1);
			lastpage = NULL;
		}

		/*
		 * If we couldn't issue this request, or there is another
		 *   request waiting on this LUN, put the LUN on the end of the
		 *   wait queue for this controller.
		 * Otherwise remove this LUN from the wait queue.
		 */
skip_lun:
		if (scipreq == NULL)
		{
			if (li->nextwait != NULL)
			{
				ci->waithead = li->nextwait;
				ci->waittail->nextwait = li;
				ci->waittail = li;
			}
			req = NULL;
		}
		else if (req->sr_ha == NULL)
		{
			li->waithead = li->waittail = NULL;
			ci->waithead = li->nextwait;
			if (ci->waithead == NULL)
				ci->waittail = NULL;
		}
		else
		{
			li->waithead = req->sr_ha;
			if (li->nextwait != NULL)
			{
				ci->waithead = li->nextwait;
				ci->waittail->nextwait = li;
				ci->waittail = li;
			}
		}
		li->nextwait = NULL;
	}
	*sreqptr = scipreq;
	if (req != NULL)
		req->sr_ha = lastpage;
	return req;
}


/*
 * Return the next low priority scb and bump the index.
 */
static struct scip_scb *
scipscb_get(struct scipctlrinfo *ci)
{
	struct scip_scb		*scb;

	scb = (struct scip_scb *)
		((__psunsigned_t) ci->scb + ci->scb_iqadd * SCIP_SCBSIZE);
	ci->scb_iqadd++;
	if (ci->scb_iqadd == SCIP_SCBNUM)
		ci->scb_iqadd = 0;
	return scb;
}


/*
 * Write the current index into the scip IQ add register
 */
void
scip_go(struct scipctlrinfo *ci, struct scipluninfo *li,
	struct sciprequest *scipreq)
{
	register int offset;
	register struct sciprequest *sortscipreq;
	register struct sciprequest *prevscipreq = NULL;
	struct sciprequest *startscipreq;

	/*
	 * Sort this scipreq into the queue of active scipreqs according to
	 * the length of the timeout.  If the timeout is <= to the first
	 * scipreq, start looking with the second scipreq, where we look for
	 * a timeout <= the new one.  This means that if all the commands
	 * have the same timeout, we insert this one into the second position.
	 * This is done so that we will not have to stop and restart the
	 * timeout.
	 */
	offset = scipreq->req->sr_timeout;
	startscipreq = li->active;
	if (startscipreq != NULL && startscipreq->req->sr_timeout >= offset)
	{
		sortscipreq = startscipreq->next;
		prevscipreq = startscipreq;
	}
	else
		sortscipreq = startscipreq;
	while (sortscipreq != NULL && sortscipreq->req->sr_timeout > offset)
	{
		prevscipreq = sortscipreq;
		sortscipreq = sortscipreq->next;
	}
	if (prevscipreq == NULL)
		li->active = scipreq;
	else
		prevscipreq->next = scipreq;
	scipreq->next = sortscipreq;

	/*
	 * If the new request is at the head of the queue for the lun, stop
	 * the old timeout if it was running and start a new one.
	 */
	if (scipreq == li->active)
	{
		if (startscipreq != NULL)
		{
			untimeout(startscipreq->timeout_id);
			startscipreq->timeout_id = 0;
		}
		scipreq->timeout_id = timeout(sciptimeout, scipreq,
			(scipreq->req->sr_timeout > scip_mintimeout) ?
			    scipreq->req->sr_timeout : scip_mintimeout,
			ci->reset_num, ci);
	}

	offset = 0x8010 + 0x800 * ci->chan;
	SCIP_REG(offset, ci) = ci->scb_iqadd << 3;

	ci->cmdcount++;
}


static void
scipdumpclear(struct scipctlrinfo *ci)
{
	struct scipunitinfo	*ui;
	struct scipluninfo	*li;
	int			 targ;
	int			 lun;

	ci->waithead = ci->waittail = NULL;
	ci->intrhead = ci->intrtail = NULL;
	ci->request = NULL;
	for (targ = 0; targ < SCIP_MAXTARG; targ++)
	{
		ui = ci->unit[targ];
		ui->scipreqbusy = 0;
		for (lun = 0; lun < SCIP_MAXLUN; lun++)
		{
			li = ui->lun[lun];
			if (li != NULL)
			{
				li->l_ca_req = 0;
				li->waithead = li->waittail = NULL;
				li->active = NULL;
				li->nextwait = NULL;
			}
		}
	}
	DELAY(500000);
	ci->reset = 1;
	ci->intr = 0;
	ci->cmdcount = 0;
	if (ci->scb_hicnt != ci->finish->high)
		scipintr(NULL, ci->number);
	else
		ci->dead = 1;
}


static int
scipcrashdump(vertex_hdl_t ctlr_vhdl)
{
	struct scipctlrinfo	*ci;
	u_char			adap = SCI_ADAP(scsi_ctlr_info_get(ctlr_vhdl));

	scipdumpmode = 1;
	if ((ci = scipctlr[adap]) == NULL)
		return 0;
	if (ci->dead)
		return 0;
	INITNLOCK(&ci->lock, "scipdump");
	INITNLOCK(&ci->intrlock, "scipcrsh");
	scip_reset(ci, 0, 1);
	if (ci->dead)		/* reset didn't work */
		return 0;
	return 1;
}


/*
 * Start waiting commands
 * Returns 1 if a command was issued, 0 if not.
 */
static int
scipstart(struct scipctlrinfo *ci)
{
	struct scipunitinfo	*ui;
	struct scipluninfo	*li;
	struct sciprequest	*scipreq;
	struct scsi_request	*req;
	struct scip_scb		*scb;
	dmamap_t		*dmamap;
	__psunsigned_t		 tmp;
	void			*xlateaddr;

	/*
	 * If we are keeping this controller quiet, or it is full, or
	 * there are no commands we can issue, just return.
	 */
	if (ci->quiesce)
		return 0;
	if (ci->cmdcount >= SCIP_MAXQUEUE)
		return 0;
	if ((req = scipdequeue(ci, &scipreq)) == NULL)
		return 0;
	ui = ci->unit[req->sr_target];
	li = ui->lun[req->sr_lun];
	scb = scipscb_get(ci);

	/*
	 * fill out SCB.
	 */
	scb->opcode = 0;
	scb->target = req->sr_target;
	scb->lun = req->sr_lun;
	scb->tag_type = req->sr_tag;
	scb->flags = (req->sr_flags & SRF_DIR_IN ? SCB_DIR : 0);
	if (li->sense_ack)
	{
		scb->flags |= SCB_SENSE_ACK;
		li->sense_ack = 0;
	}

	/*
	 * Have the scip (re)negotiate on all inquiry commands, unless this
	 * is initialization time.  Some devices don't obey the spec if you
	 * try to negotiate wide mode with them (Archive DAT for one).
	 */
	if (req->sr_command[0] == 0x12 || ui->renegotiate)
	{
		if ((scip_bus_flags[ci->number] & 4) && ui->wide_supported)
			scb->flags |= SCB_DO_WIDE;
		if (scip_bus_flags[ci->number] & 1)
			scb->flags |= SCB_DO_SYNC;
		scb->xferfactor = ui->min_syncperiod;
		scb->offset = ui->max_syncoffset;
		ui->renegotiate = 0;
	}
	if (!(scip_bus_flags[ci->number] & 2))
		scb->flags |= SCB_NO_DISC_ALLOW;

	bcopy(req->sr_command, scb->scsi_cmd, req->sr_cmdlen);
	scb->cmd_length = req->sr_cmdlen;
	scb->dma_length = (req->sr_buflen + 1) & ~1; /* only even counts supported */
	if (req->sr_buflen != 0)
	{
		if (req->sr_spare)
			dmamap = req->sr_spare;
		else
			dmamap = scipreq->dmamap;
		
		if (req->sr_flags & SRF_MAPBP)
			tmp = dma_mapbp(dmamap, req->sr_bp, req->sr_buflen);
		else if (req->sr_flags & SRF_MAPUSER)
			tmp = req->sr_buflen;
		else if (req->sr_ha)
			tmp = dma_map2(dmamap, (caddr_t) req->sr_buffer, req->sr_ha, req->sr_buflen);
		else
			tmp = dma_map(dmamap, (caddr_t) req->sr_buffer, req->sr_buflen);
		ASSERT(tmp == req->sr_buflen);

		if (req->sr_flags & SRF_MAPUSER)
			xlateaddr = ((struct buf *)(req->sr_bp))->b_private;
		else
			xlateaddr = (void *) dmamap->dma_virtaddr;
		
		tmp = ev_kvtoiopnum(xlateaddr);
		scb->dma_xlatehi = tmp >> 20;
		scb->dma_offset = io_poff(req->sr_buffer);
		scb->dma_xlatelo = io_ctob(tmp) | io_poff(xlateaddr);
	}
	scb->sense_length = SCSI_SENSE_LEN;
	scb->sense_xlatehi = li->sense_xlatehi;
	scb->sense_offset = li->sense_offset;
	scb->sense_xlatelo = li->sense_xlatelo;

	scb->scipreq = scipreq;
	scipreq->req = req;
	*((long long *) &scb->scsi_status) = 0;
	scb->op_id = (uint) (ulong) scipreq->req;

	scip_go(ci, li, scipreq);
	return 1;
}


void
scipcommand(struct scsi_request *req)
{
	struct scipctlrinfo	*ci;
	struct scipunitinfo	*ui;
	struct scipluninfo	*li;
	uint			 ctlr, targ, lun;
	int			 pri;

	req->sr_status = req->sr_scsi_status = req->sr_sensegotten =
	req->sr_ha_flags = 0;
	req->sr_spare = req->sr_ha = NULL;
	req->sr_resid = req->sr_buflen;

	ctlr = req->sr_ctlr;
	targ = req->sr_target;
	lun = req->sr_lun;

	if (ctlr > scip_maxadap || targ > 15 || lun > 7 || req->sr_notify == NULL)
	{
		req->sr_status = SC_REQUEST;
		goto err_scipcommand;
	}
	if ((req->sr_buflen != 0 && ((__psunsigned_t) req->sr_buffer & 3)) ||
	    ((req->sr_buflen & 1) && (req->sr_flags & (SRF_MAPUSER|SRF_MAPBP))))
	{
		req->sr_status = SC_ALIGN;
		goto err_scipcommand;
	}

	ci = scipctlr[ctlr];
	ui = ci->unit[targ];
	li = ui->lun[lun];
	if (li == NULL)
	{
		req->sr_status = SC_REQUEST;
		goto err_scipcommand;
	}
	if (ci->dead)
	{
		cmn_err(CE_CONT, "scip%d: unusable due to fatal error\n",
			SCEXT(req->sr_ctlr));
		req->sr_status = SC_HARDERR;
		goto err_scipcommand;
	}

	/*
	 * For efficiency sake, we only grab the lock if intr is set.  We
	 * have to recheck to see if intr is set before continuing, since
	 * there is a small window in which it could have changed before we
	 * get the lock.
	 */
	if (ci->intr)
	{
		IOLOCK(ci->intrlock, pri);
		if (ci->intr)
		{
			if (ci->intrhead == NULL)
				ci->intrhead = req;
			else
				ci->intrtail->sr_ha = req;
			ci->intrtail = req;
			IOUNLOCK(ci->intrlock, pri);
			return;
		}
		IOUNLOCK(ci->intrlock, pri);
	}

	IOLOCK(ci->lock, pri);
	if (li->l_ca_req)
	{
		if (req->sr_flags & SRF_AEN_ACK)
			li->l_ca_req = 0;
		else
		{
			req->sr_status = SC_ATTN;
			IOUNLOCK(ci->lock, pri);
			goto err_scipcommand;
		}
	}
	scipenqueue(ci, li, req);
	scipstart(ci);
	IOUNLOCK(ci->lock, pri);

	if (scipdumpmode)
	{
		pri = 0;
		do
		{
			DELAY(100);
			li = (void *) PHYS_TO_K0(ci->finish->low);
			if ((__psunsigned_t) li == PHYS_TO_K0(0))
				continue;
		}
		while ((++pri < 600000) &&
		       (((__psunsigned_t) li == PHYS_TO_K0(0)) ||
		        ((struct scip_scb *) li == ci->scb_cqremove)));
		if (pri == 600000)
		{
			printf("\nscip%dd%dl%d: timeout; resetting SCSI bus\n",
			       SCEXT(ctlr), targ, lun);
			scip_reset(ci, 0, 1);
			req->sr_status = SC_CMDTIME;
		}
		else
			scipintr(NULL, ctlr);
	}
	return;

err_scipcommand:
	(*req->sr_notify)(req);
}


static void
scipdone(struct scsi_request *req)
{
	vsema(req->sr_dev);
}


struct scsi_target_info *
scipinfo(vertex_hdl_t lun_vhdl)
{
	struct scipctlrinfo	*ci;
	struct scipunitinfo	*ui;
	struct scipluninfo	*li;
	struct scsi_request	*req;
	static u_char		 scsi[6];
	sema_t			*sema;
	char			 registered = 0;
	u_char			 ctlr;
	uint			 tmp;
	struct scsi_target_info	*retval = NULL;
	u_char			extctlr,targ,lun;
	scsi_lun_info_t		*lun_info;
	int retry = 2;
	int busy_retry = 8;
	
	lun_info = scsi_lun_info_get(lun_vhdl);
	extctlr  = SLI_ADAP(lun_info);
	targ	 = SLI_TARG(lun_info);
	lun	 = SLI_LUN(lun_info);

	ctlr = extctlr;
	if (ctlr > scip_maxadap || targ > SCIP_MAXTARG || targ == scip_hostid)
		return NULL;

	if ((ci = scipctlr[ctlr]) == NULL)
		return NULL;
	ui = ci->unit[targ];
	mutex_lock(&ui->opensema, PRIBIO);
	li = ui->lun[lun];

	if (li == NULL)
	{
		li = kern_calloc(sizeof(struct scipluninfo), 1);
		ui->lun[lun] = li;
		li->tinfo.si_inq = kmem_alloc(SCSI_INQUIRY_LEN, VM_CACHEALIGN);
		li->tinfo.si_sense = kmem_zalloc(SCSI_SENSE_LEN, VM_CACHEALIGN);
		li->sensemap = dma_mapalloc(DMA_SCSI, ctlr,
			io_numpages(li->tinfo.si_sense, SCSI_SENSE_LEN), 0);
		tmp = dma_map(li->sensemap, (caddr_t) li->tinfo.si_sense, SCSI_SENSE_LEN);
		ASSERT(tmp == SCSI_SENSE_LEN);
		tmp = ev_kvtoiopnum(li->sensemap->dma_virtaddr);
		li->sense_xlatehi = tmp >> 20;
		li->sense_offset = io_poff(li->tinfo.si_sense);
		li->sense_xlatelo = io_ctob(tmp) | io_poff(li->sensemap->dma_virtaddr);
	}
	else
		registered = 1;

	sema = kern_calloc(1, sizeof(sema_t));
	init_sema(sema, 0, "sca", ctlr<<4 | targ);
	req = kern_calloc(1, sizeof(*req));

	do {

	    bzero(req, sizeof(*req));

	    scsi[0] = 0x12;  /* Inquiry command */
	    scsi[1] = scsi[2] = scsi[3] = scsi[5] = 0;
	    scsi[4] = SCSI_INQUIRY_LEN;

	    req->sr_ctlr = ctlr;
	    req->sr_target = targ;
	    req->sr_lun = lun;
	    req->sr_command = scsi;
	    req->sr_lun_vhdl = lun_vhdl;
	    req->sr_cmdlen = 6;	/* Inquiry command length */
	    req->sr_flags = SRF_DIR_IN | SRF_FLUSH | SRF_AEN_ACK;
	    req->sr_timeout = 10 * HZ;
	    req->sr_buffer = li->tinfo.si_inq;
	    req->sr_buflen = SCSI_INQUIRY_LEN;
	    req->sr_sense = NULL;
	    req->sr_notify = scipdone;
	    req->sr_dev = sema;

	    bzero(li->tinfo.si_inq, SCSI_INQUIRY_LEN);

	    scipcommand(req);
	    psema(sema, PRIBIO);

	    if ((req->sr_status == SC_GOOD) && (req->sr_scsi_status == ST_GOOD))
	    {
		/*
		 * Look at Inquiry data to see if device supports wide mode.
		 * If it does, then turn it on and signal to renegotiate.
		 */
		if (li->tinfo.si_inq[7] & 0x20)
			if (!ui->wide_supported)
			{
				ui->wide_supported = 1;
				ui->renegotiate = 1;
			}
		ui->present = 1;

		/*
		 * If the LUN is not supported and is not a candidate
		 * for failover, then return NULL.  
		 */
		if ((li->tinfo.si_inq[0] & 0xC0) == 0x40 && 
		    !fo_is_failover_candidate(li->tinfo.si_inq, lun_vhdl))
		{
			if (!scipinittime)
				printf("scip%dd%dl%d: lun not valid, peripheral "
				       "qualifier 0x%x\n", SCEXT(ctlr), targ, lun,
				       (li->tinfo.si_inq[0] & 0xE0) >> 5);
		}
		else
		{
			scsi_device_update(li->tinfo.si_inq,lun_vhdl);
			li->tinfo.si_maxq = SCIP_MAXQUEUE;
			li->tinfo.si_ha_status = SRH_WIDE | SRH_DISC | SRH_TAGQ |
						 SRH_MAPUSER | SRH_QERR1;
			retval = &li->tinfo;
		}
	    }

	    if (req->sr_scsi_status == ST_BUSY) delay(HZ/2);

	} while ((req->sr_status == SC_ATTN && retry--) || 
	         	(req->sr_scsi_status == ST_BUSY && busy_retry--));

	if ((retval == NULL) && !registered)
	{
		kern_free(li->tinfo.si_inq);
		kern_free(li->tinfo.si_sense);
		li->tinfo.si_inq = NULL;
		dma_mapfree(li->sensemap);
		kern_free(li);
		ui->lun[lun] = NULL;
	}

	kern_free(req);
	freesema(sema);
	kern_free(sema);
	mutex_unlock(&ui->opensema);
	return retval;
}


int
scipalloc(vertex_hdl_t lun_vhdl, int opt, void (*cb)())
{
	struct scipluninfo	*li;
	struct scipunitinfo	*ui;
	u_char			 ctlr;
	int			 retval = SCSIALLOCOK;
	u_char			extctlr,targ,lun;
	scsi_lun_info_t		*lun_info;

	lun_info  = scsi_lun_info_get(lun_vhdl);
	extctlr	  = SLI_ADAP(lun_info);
	targ	  = SLI_TARG(lun_info);
	lun	  = SLI_LUN(lun_info);	

	if (scipinfo(lun_vhdl) == NULL)
		return 0;
	
	ctlr = extctlr;
	ui = scipctlr[ctlr]->unit[targ];
	mutex_lock(&ui->opensema, PRIBIO);
	li = ui->lun[lun];
	if (li->exclusive)
	{
		retval = EBUSY;
		goto _allocout;
	}

	if (opt & SCSIALLOC_EXCLUSIVE)
	{
		if (li->refcount != 0)
		{
			retval = EBUSY;
			goto _allocout;
		}
		else
			li->exclusive = 1;
	}
	if (cb != NULL)
	{
		if (li->sense_callback != NULL)
		{
			retval = EINVAL; /* olson wants unique code */
			goto _allocout;
		}
		else
			li->sense_callback = cb;
	}
	li->refcount++;
_allocout:
	mutex_unlock(&ui->opensema);
	return retval;
}


void
scipfree(vertex_hdl_t lun_vhdl, void (*cb)())
{
	struct scipctlrinfo	*ci;
	struct scipluninfo	*li;
	u_char			 ctlr;
	u_char			extctlr,targ,lun;
	scsi_lun_info_t		*lun_info;

	lun_info = scsi_lun_info_get(lun_vhdl);
	extctlr  = SLI_ADAP(lun_info);
	targ	 = SLI_TARG(lun_info);
	lun	 = SLI_LUN(lun_info);
	
	ctlr = extctlr;
	if (ctlr > scip_maxadap || targ > SCIP_MAXTARG || lun > 7)
		return;
	if ((ci = scipctlr[ctlr]) == NULL)
		return;
	li = ci->unit[targ]->lun[lun];
	if (li->exclusive)
		li->exclusive = 0;
	if (cb != NULL)
	{
		if (li->sense_callback == cb)
			li->sense_callback = NULL;
		else
			printf("scip%dd%d: callback 0x%x not active\n",
			       SCEXT(ctlr), targ, cb);
	}
	if (li->refcount != 0)
		li->refcount--;
#ifdef DEBUG
	else
		printf("scip%dd%d: scipfree called when refcount 0\n",
		       SCEXT(ctlr), targ);
#endif
}


/*
 * After calling notify routines, put commands queued while notifying
 * on the regular wait queues.  Also clear interrupt flag.
 */
void
scipqintr(struct scipctlrinfo *ci)
{
	struct scsi_request	*req;
	struct scipluninfo	*li;

	LOCK(ci->intrlock);
	ci->intr = 0;
	while ((req = ci->intrhead) != NULL)
	{
		ci->intrhead = req->sr_ha;
		if (ci->intrtail == req)
		{
			ci->intrtail = NULL;
			ASSERT(ci->intrhead == NULL);
		}
		req->sr_ha = NULL;
		li = ci->unit[req->sr_target]->lun[req->sr_lun];
		if (li->l_ca_req)
		{
			if (req->sr_flags & SRF_AEN_ACK)
				li->l_ca_req = 0;
			else
			{
				req->sr_status = SC_ATTN;
				ci->intr = 1;
				UNLOCK(ci->intrlock);
				(*req->sr_notify)(req);
				LOCK(ci->intrlock);
				ci->intr = 0;
				continue;
			}
		}
		scipenqueue(ci, li, req);
	}
	UNLOCK(ci->intrlock);
}


/*
 * Complete processing on a command.
 * Take the scipreq off the active list and put it on the free list.
 * Decrement the command count.
 * Notify of command completion.
 */
static void
scipcomplete(struct scipctlrinfo *ci, struct scipluninfo *li,
	struct sciprequest *scipreq, struct scsi_request *req)
{
	register struct sciprequest	*prevscipreq;

	/* Flush IO4 cache if necessary */
	if (io4ia_war && io4_flush_cache((paddr_t) ci->chip_addr))
		panic("SCSI IO4 cache flush");

	/* remove scipreq from active list */
	if (scipreq == li->active)
		li->active = scipreq->next;
	else
	{
		prevscipreq = li->active;
		while (prevscipreq->next != scipreq)
		{
			prevscipreq = prevscipreq->next;
			ASSERT(prevscipreq != NULL);
		}
		prevscipreq->next = scipreq->next;
	}

	/*
	 * Turn off timeout if active, and start a new one for the next command.
	 */
	if (scipreq->timeout_id)
	{
		untimeout(scipreq->timeout_id);
		scipreq->timeout_id = 0;
		if (li->active)
			li->active->timeout_id = timeout(sciptimeout, li->active,
				(li->active->req->sr_timeout > scip_mintimeout) ?
				li->active->req->sr_timeout : scip_mintimeout,
				ci->reset_num, ci);
	}

	scipreq_put(ci, req->sr_target, scipreq);	/* put on free list */

	ci->cmdcount--;			/* decrement command count */

	/* notify of command completion */
	(*req->sr_notify)(req);
}


/*
 * Return all outstanding commands for this ctlr/lun.  First, return the
 * commands that were in progress.  Then return those on the wait list.
 */
static void
scipreturn(struct scipctlrinfo *ci, register struct scipluninfo *li,
	   int errcode)
{
	register struct scsi_request	*req;
	struct sciprequest		*scipreq;
	struct scsi_request		*nextreq;
	register struct scipluninfo	*tmpli;

	/*
	 * Get head of list of waiting reqs for future use.
	 */
	nextreq = li->waithead;
	li->waithead = li->waittail = NULL;

	/*
	 * Take LUN off list of waiting luns
	 */
	tmpli = ci->waithead;
	if (tmpli == li)
	{
		ASSERT(tmpli->nextwait != tmpli);
		ci->waithead = tmpli->nextwait;
		if (li == ci->waittail)
		{
			ASSERT(ci->waithead == NULL);
			ci->waittail = NULL;
		}
	}
	else
	{
		while (tmpli != NULL)
		{
			ASSERT(tmpli->nextwait != tmpli);
			if (tmpli->nextwait == li)
			{
				tmpli->nextwait = li->nextwait;
				break;
			}
			tmpli = tmpli->nextwait;
		}
		if (li == ci->waittail)
		{
			ASSERT(tmpli->nextwait == NULL);
			ASSERT(li->nextwait == NULL);
			ci->waittail = tmpli;
		}
	}
	li->nextwait = NULL;

	/*
	 * Send back the commands that were in progress.
	 */
	while ((scipreq = li->active) != NULL)
	{
		req = scipreq->req;
		if (req->sr_status == 0)
			req->sr_status = errcode;
		scipcomplete(ci, li, scipreq, req);
	}

	/*
	 * Send back the commands on the wait list.  For now, send them
	 * back with the same error code as those in progress.
	 */
	req = nextreq;
	while (req != NULL)
	{
		req->sr_status = errcode;
		nextreq = req->sr_ha;
		(*req->sr_notify)(req);
		req = nextreq;
	}
}


/*
 * Clear all commands from this ctlr (SCSI bus).
 */
static void
scipclear(struct scipctlrinfo *ci, int error)
{
	struct scipunitinfo	*ui;
	struct scipluninfo	*li;
	int			 i, j;

	ci->intr = 1;
	for (i = 0; i < SCIP_MAXTARG; i++)
	{
		ui = ci->unit[i];
		for (j = 0; j < 8; j++)
		{
			li = ui->lun[j];
			if (li != NULL)
			{
				scipreturn(ci, li, error);
				li->l_ca_req = 1;
				li->nextwait = NULL;
			}
		}
		ui->renegotiate = 1;
	}
	scipqintr(ci);
}


/*
 * Clear commands from controller and mark controller as dead
 */
static void
scipkill(struct scipctlrinfo *ci, char *msg)
{
	if (msg != NULL)
		cmn_err(CE_CONT, "scip%d: %s\n", SCEXT(ci->number), msg);
	ci->dead = 1;
	scipclear(ci, SC_HARDERR);
}


/*
 * sciptimeoreset - timeout the reset or clear the quiesce flag
 * If second time is set, that means this is the second time we've gotten
 * the timeout since we issued the reset.
 */
void
sciptimeoreset(struct scipctlrinfo *ci, int second_time)
{
	SPL_DECL(pri)

	IOLOCK(ci->lock, pri);
	ci->reset_timeout = 0;
	/*
	 * If ci->reset still set, we did not get a response to our high
	 * priority request.
	 */
	if (ci->reset)
	{
		if (second_time)
			scipkill(ci, "still no response to reset request (fatal)");
		else
		{
			ci->reset_timeout = timeout(sciptimeoreset, ci, HZ * 5, 1);
			cmn_err(CE_CONT, "scip%d: no response to reset request -"
				" allowing more time\n", SCEXT(ci->number));
		}
	}
	else
	{
		ci->quiesce = 0;
		while (scipstart(ci)) ;
	}
	IOUNLOCK(ci->lock, pri);
}


static void
resetcb_setup(struct scipctlrinfo *ci, register struct scip_resetcb *resetcb)
{
	bzero(resetcb, 128);
	resetcb->lo_cq_add_base_lo = resetcb->lo_iq_remove_base_lo =
		resetcb->lo_iq_add_base_lo = kvtophys(ci->scb);
	resetcb->lo_iq_mask = SCIP_SCBSIZE * SCIP_SCBNUM - 1;
	resetcb->med_cq_add_base_lo = resetcb->med_iq_remove_base_lo =
		resetcb->med_iq_add_base_lo = kvtophys(ci->mcb);
	resetcb->med_iq_mask = SCIP_SCBSIZE * SCIP_MCBNUM - 1;
	resetcb->hi_req_add_base_lo = kvtophys(ci->hcb);
	resetcb->error_scb_addr_lo = kvtophys(ci->ecb);
	resetcb->scip_host_id = scip_hostid;
	resetcb->complete_intr = SCIP_REG(0x7000, ci);
	resetcb->finish_ptr_lo = kvtophys(ci->finish);
}


/*
 * Reset the task and then tell the SCIP to reset the SCSI bus.
 */
void
scip_reset(register struct scipctlrinfo *ci, int error, int opcode)
{
	register struct scip_resetcb	*resetcb;

	/*
	 * If a reset or clean up from reset is already in progress, that
	 * means that all commands have been returned already, and none are
	 * in progress.  It also indicates that the bus has been reset
	 * since we last tried to do anything, so we can just return.
	 */
	if (ci->reset)
		return;

	/*
	 * Reset task and reload initialization block.
	 */
	SCIP_REG(0xA000, ci) = 0x10 << ci->chan;
	SCIP_REG(0xA000, ci) = (0x100 << ci->chan) | 1;
	SCIP_REG(0xA000, ci) = (0x100 << ci->chan);

	/*
	 * Reset controller info stuff to starting values.
	 */
	ci->scb_iqadd = 0;
	ci->scb_cqremove = ci->scb;
	ci->finish->low = ci->finish->med = 0;
	ci->waithead = ci->waittail = NULL;

	/*
	 * Build high pri reset request and start task.
	 */
	resetcb = (struct scip_resetcb *) ci->hcb;
	resetcb_setup(ci, resetcb);
	resetcb->opcode = opcode;
	SCIP_REG((0x8000 + 0x800 * ci->chan), ci) = 0x8000 | 1;
	SCIP_REG(0xA000, ci) = (0x10 << ci->chan) | 1;

	if (scipdumpmode)
		scipdumpclear(ci);
	else
	{
		ci->quiesce = 1;
		ci->reset = 1;
		ci->reset_num++;

		scipclear(ci, error);
		if (ci->reset_timeout)
			untimeout(ci->reset_timeout);
		ci->reset_timeout = timeout(sciptimeoreset, ci, HZ, 0);
	}
}


/*
 * Reset requested from another driver (user originated)
 */
static int
scipusr_reset(u_char adap)
{
	struct scipctlrinfo	*ci;
	SPL_DECL(pri)

	if (adap > scip_maxadap)
		return 0;
	if ((ci = scipctlr[adap]) == NULL)
		return 0;
	IOLOCK(ci->lock, pri);
	scip_reset(ci, SC_ATTN, 1);
	IOUNLOCK(ci->lock, pri);

	return 1;
}


/* ARGSUSED */
int
scipioctl(vertex_hdl_t ctlr_vhdl, uint cmd, scsi_ha_op_t *op)
{
	int err = 0;
	int adap = SCI_ADAP(scsi_ctlr_info_get(ctlr_vhdl));
	
	switch (cmd)
	{
	case SOP_RESET:
		if (!scipusr_reset(adap))
			err = EIO;
		break;

	case SOP_SCAN:
	{
		struct scipctlrinfo *ci;
		struct scipluninfo  *li;
		struct scipunitinfo *ui;
		int i,j,refcount;

		if ((ci = scipctlr[adap]) == NULL)
		{
			err = EINVAL;
			break;
		}
		LOCK(ci->scanlock);
		for (i = 0; i < SCIP_MAXTARG; i++) {
			vertex_hdl_t	lun_vhdl;

			ui = ci->unit[i];
			if (ui) {
				li = ui->lun[0];
				if (li) {
					refcount = li->refcount;
				}
			}
			else refcount = 0;
			/* 
			 * if the vertex for the device is not there
			 * create a vertex for the device
			 */
			lun_vhdl = scsi_device_add(ctlr_vhdl,i,0);
			/*
			 * check if the device actually exists 
			 * if there is no device remove  the vertex from the graph
			 */
			if (!scipinfo(lun_vhdl) && !refcount)
				scsi_device_remove(ctlr_vhdl,i,0);

			if (ci->unit[i]->present)
				for (j = 1; j < 8; j++) {
					li = ui->lun[j];
					if (li)
						refcount = li->refcount;
					else
						refcount = 0;
					/* 
					 * if the vertex for the device is not there
					 * create a vertex for the device
					 */
					lun_vhdl = scsi_device_add(ctlr_vhdl,i,j);
					/*
					 * check if the device actually exists 
					 * if there is no device remove  the vertex from the graph
					 */
					if (!scipinfo(lun_vhdl) && !refcount)
						scsi_device_remove(ctlr_vhdl,i,j);
				}	
	
		}
		UNLOCK(ci->scanlock);
		break;
	}
	
	default:
		err = EINVAL;
	}
	return err;
}


/*
 * Check for an error on the SCIP chip.
 */
static int
scipchkerr(int startctlr, int per_slice_bits)
{
	register struct scipctlrinfo	*ci;
	register int			 i;
	register uint			 reg;
	register int			 err = 0;

	ci = scipctlr[startctlr];

	reg = SCIP_REG(0x6000, ci);
	if (reg & 0x81F000)
	{
		printf("scip%d: CSR error 0x%x\n", SCEXT(startctlr),
		       reg & 0x81F000);
		err = 1;
		for (i = 0; i < 3; i++)
		{
			ci = scipctlr[startctlr + i];
			LOCK(ci->lock);
			scipkill(ci, "CSR error");
			UNLOCK(ci->lock);
		}
		return err;
	}
	for (i = 0; i < 3; i++)
	{
		if (reg & (per_slice_bits << i))
		{
			printf("scip%d: CSR error 0x%x\n", SCEXT(startctlr + i),
			       reg & (0x240 << i));
			err = 1;
			ci = scipctlr[startctlr + i];
			LOCK(ci->lock);
			scipkill(ci, "CSR error");
			UNLOCK(ci->lock);
		}
	}
	if (err)
		return err;
	if ((reg = SCIP_REG(0xA000, ci)) & 0x1000)
	{
		printf("scip%d: processor parity error\n", startctlr);
		err = 1;
		for (i = 0; i < 3; i++)
		{
			ci = scipctlr[startctlr + i];
			LOCK(ci->lock);
			scipkill(ci, "processor parity error");
			UNLOCK(ci->lock);
		}
	}
	return err;
}


/*
 * Timeout handler
 * For now, reset SCSI bus and continue.
 */
static void
sciptimeout(struct sciprequest *scipreq, int reset_number,
	    struct scipctlrinfo *ci)
{
	struct scipunitinfo	*ui;
	struct scipluninfo	*li;
	struct scsi_request 	*req;
	int			 pri;

	/*
	 * Check for errors in the scip chip.  These errors may be
	 * the cause of the timeout.
	 */
	pri = ci->number;
	if ((pri & 0x7) > 4)
		pri = (pri & ~7) | 5;
	else
		pri = (pri & ~7) | 2;
	if (scipchkerr(pri, 0x240))
		return;

	/*
	 * First, check to make sure we don't hit a timing window, by
	 * seeing if the timeout is valid.  It is possible for a timeout
	 * to occur when we are already resetting a controller, but before
	 * we have done an untimeout.  In that case, the later timeout
	 * would spin below in splockspl(), but essentially be invalid.
	 */
	IOLOCK(ci->lock, pri);
	scipreq->timeout_id = 0;
	if (ci->reset_num != reset_number)
	{
		cmn_err(CE_CONT, "scip%d: timeout - already handling\n",
			SCEXT(ci->number));
		goto sciptimeout_end;
	}

	req = scipreq->req;
	ui = ci->unit[req->sr_target];
	li = ui->lun[req->sr_lun];
	req->sr_status = SC_CMDTIME;
	cmn_err(CE_CONT, "scip%dd%dl%d: timeout (cmd 0x%x) - resetting bus\n",
		SCEXT(ci->number), ui->number, li->number, req->sr_command[0]);
	ASSERT(!ci->reset);
#if SCIPDEBUG
	scip_status(ci->number);
#endif
	scip_reset(ci, SC_ATTN, 1);

sciptimeout_end:
	IOUNLOCK(ci->lock, pri);
}


static void
scipsensepr(int key, int asc, int asq)
{
	if (key & 0x80)
		printf("FMK ");
	if (key & 0x40)
		printf("EOM ");
	if (key & 0x20)
		printf("ILI ");
	printf("sense key 0x%x (%s) asc 0x%x",
	       key & 0xF, scsi_key_msgtab[key & 0xF], asc);
	if (asc < SC_NUMADDSENSE && scsi_addit_msgtab[asc] != NULL)
		printf(" (%s)", scsi_addit_msgtab[asc]);
	printf(" asq 0x%x\n", asq);
}


static void
scipsense(struct scipluninfo *li, struct scsi_request *req, struct scip_scb *scb)
{
	req->sr_sensegotten = min(scb->sense_gotten, req->sr_senselen);
	if (req->sr_sense != NULL)
	{
		bcopy(li->tinfo.si_sense, req->sr_sense, req->sr_sensegotten);
		if (li->sense_callback)
			(*li->sense_callback)(req->sr_lun_vhdl,
				      	(char*)li->tinfo.si_sense);
#if SCIPDEBUG
		printf("scip%dd%d: CMD 0x%x ", SCEXT(req->sr_ctlr), req->sr_target,
		       req->sr_command[0]);
		scipsensepr(li->tinfo.si_sense[2], li->tinfo.si_sense[12],
			    li->tinfo.si_sense[13]);
#endif
	}
	else
	{
		if (req->sr_scsi_status == 0)
			req->sr_scsi_status = ST_CHECK;
		printf("scip%dd%d: CMD 0x%x ", SCEXT(req->sr_ctlr), req->sr_target,
		       req->sr_command[0]);
		scipsensepr(li->tinfo.si_sense[2], li->tinfo.si_sense[12],
			    li->tinfo.si_sense[13]);
	}
	li->l_ca_req = 1;
	li->sense_ack = 1;
}


/*
 * Processes SCIP return codes.
 * Returns one if a SCIP/SCSI reset gets done, zero otherwise.
 */
static int
scipctlrerror(struct scipctlrinfo *ci, struct scsi_request *req, struct scip_scb *scb)
{
	if (scb->scip_status == SCIP_STAT_CCABORT)
	{
#if SCIPDEBUG
		printf("scip%dd%d: command aborted by contingent "
		       "allegiance\n", SCEXT(req->sr_ctlr), req->sr_target);
#endif
		req->sr_status = SC_ATTN;
	}
	else
	{
		if (scb->scip_status == 4)
		{
#if !SCIPDEBUG
			if (!scipinittime)
#endif
			printf("scip%dd%d: does not respond "
			       "(selection timeout)\n",
			       SCEXT(req->sr_ctlr), req->sr_target);
			req->sr_status = SC_TIMEOUT;
		}
		else
		{
			printf("scip%dd%d: ", SCEXT(req->sr_ctlr), req->sr_target);
			if (scb->scip_status == 0x22)
				printf("scsi bus parity error, probably due to bad"
				       " SCSI bus data path");
			else
				printf("error 0x%x (0x%x 0x%x 0x%x), probably due to "
				       "bad SCSI bus data path or unexpected protocol",
				       scb->scip_status, scb->error_data[0],
				       scb->error_data[1], scb->error_data[2]);
			printf("\n\tresetting SCSI bus and SCIP\n");
			req->sr_status = SC_HARDERR;
#if SCIPDEBUG
			scip_status(ci->number);
#endif
			scip_reset(ci, SC_ATTN, 1);
			return 1;
		}
	}
	return 0;
}


/*
 * We got a non-zero status byte.  Pass it on to the device driver.
 */
static void
scipscsierror(struct scipluninfo *li, struct scsi_request *req, struct scip_scb *scb)
{
	/*
	 * If we get status of 2, that means that firmware got a check condition on
	 * a request sense command.  In that case, sense data is not valid, but the
	 * firmware still needs an acknowledge of sense data.
	 */
	if (scb->scsi_status == 2)
		li->sense_ack = 1;
#if SCIPDEBUG
	printf("scip%dd%d: scsi status 0x%x\n", SCEXT(req->sr_ctlr),
	       req->sr_target, scb->scsi_status);
#endif
	req->sr_scsi_status = scb->scsi_status;
}


void
scip_fixoddbyte(struct scsi_request *req)
{
	register u_char	*from;
	register u_char	*to;
	register int	 count;

	ASSERT(req->sr_buflen & 1);
	if (io_poff(req->sr_buffer) + req->sr_buflen > IO_NBPC)
	{
		from = req->sr_ha;
		to = (u_char *) io_ctob(io_btoct(&req->sr_buffer[req->sr_buflen]));
		count = io_poff(&req->sr_buffer[req->sr_buflen]);
	}
	else
	{
		from = (u_char *) ((__psunsigned_t) req->sr_ha + io_poff(req->sr_buffer));
		to = req->sr_buffer;
		count = req->sr_buflen;
	}
	req->sr_resid--;
	count -= req->sr_resid;
	if (count > 0)
		bcopy(from, to, count);
	kvpfree(req->sr_ha, 1);
}


/*
 * Take an interrupt from the SCIP chip.
 *
 * Because of a lack of interrupts levels in Everest, all SCSI channels
 * on a SCIP use the same interrupt level.  So scipintr must look at
 * the finish blocks of all chips.
 * The funny indentation is meant to have the main code all be at the
 * first level of indentation for readability.
 */
/* ARGSUSED */
void
scipintr(eframe_t *ep, __psunsigned_t startctlr)
{
    struct scipctlrinfo		*ci;
    struct scipunitinfo		*ui;
    struct scipluninfo		*li;
    struct scip_scb		*scb;
    struct sciprequest		*scipreq;
    struct scsi_request		*req;
    int				 adap;
    int				 endctlr;

    if (scipdumpmode)
	endctlr = startctlr + 1;
    else
	endctlr = startctlr + 3;

    if (scipchkerr(startctlr, 0x200))
	    return;

    for (adap = startctlr; adap != endctlr; adap++)
    {
	ci = scipctlr[adap];
	if (ci == NULL)
		continue;
	LOCK(ci->lock);
	if (ci->dead)
		goto scipintr_end;

	/*
	 * Check for a complete high priority SCB.
	 * Currently, we only expect this if a SCSI bus reset is in progress.
	 */
	if (ci->scb_hicnt != ci->finish->high)
	{
		ASSERT(ci->hcb->opcode == 1 || ci->hcb->opcode == 2);
		if (!(ci->hcb->flags & SCB_COMPLETE))
			panic("SCIP high priority SCB not complete");
#if SCIPDEBUG
		printf("scip%d: SCSI bus reset complete\n", SCEXT(ci->number));
#endif
		if (ci->reset)
			ci->reset = 0;
		else
			scipkill(ci, "unexpected high priority command finish");
		ci->scb_hicnt = ci->finish->high;
	}

	/*
	 * Check for error SCB
	 */
	if (ci->scb_errcnt != ci->finish->error)
	{
		scb = ci->ecb;
		printf("scip%d: SCB error code 0x%x - ", SCEXT(ci->number),
		       scb->scip_status);
		if (scb->scip_status == FIRM_UNEXP_RESET)
		{
			printf("SCSI bus reset detected\n");
			scip_reset(ci, SC_ATTN, 2);
		}
		else
		{
			printf("resetting SCSI bus\n");
			scip_reset(ci, SC_ATTN, 1);
		}
		ci->scb_errcnt = ci->finish->error;
		goto scipintr_end;
	}

	/*
	 * Check for completed low priority SCB's and process.
	 */
	while ((ci->scb_cqremove->flags & SCB_COMPLETE) && (ci->cmdcount > 0))
	{
		/*
		 * Check SCB for sanity; then find relevant data structures.
		 */
		scb = ci->scb_cqremove;
		scipreq = scb->scipreq;
		if ((uint) (ulong) scipreq->req != scb->op_id)
		{
			cmn_err(CE_PANIC, "scip%d: SCB tag mismatch\n",
				SCEXT(ci->number));
		}
		ASSERT(scipreq != NULL);
		ui = ci->unit[scb->target];
		li = ui->lun[scb->lun];
		req = scipreq->req;

		/*
		 * Do error checking and handling for this scb
		 */
		if (scb->scip_status)
		{
			if (scipctlrerror(ci, req, scb))
				goto scipintr_end;
		}
		else if (scb->scsi_status)
			scipscsierror(li, req, scb);
		else if (scb->sense_gotten > 0)
			scipsense(li, req, scb);
		req->sr_resid = scb->dma_resid;
		if (req->sr_ha != NULL)		/* return last page buffer */
			scip_fixoddbyte(req);
		else if (req->sr_buflen & 1)
			req->sr_resid--;
		if (scb->flags & SCB_ODD_BYTE_FLAG)
		{
			if (req->sr_flags & (SRF_MAPUSER|SRF_MAPBP))
				req->sr_status = SC_REQUEST;
			else
			{
				req->sr_buffer[req->sr_buflen - req->sr_resid] =
						scb->odd_byte;
				req->sr_resid--;
			}
		}
		if (scb->flags & SCB_ODD_SENSE_BYTE)
		{
			req->sr_sense[req->sr_sensegotten] = scb->odd_byte;
			req->sr_sensegotten++;
		}

		/*
		 * Update completion queue pointer
		 */
		ci->scb_cqremove = (struct scip_scb *)
			((__psunsigned_t) ci->scb_cqremove + SCIP_SCBSIZE);
		if ((__psunsigned_t) ci->scb_cqremove ==
		    (__psunsigned_t) ci->scb + SCIP_SCBSIZE * SCIP_SCBNUM)
			ci->scb_cqremove = ci->scb;

		/*
		 * Finish command processing
		 */
		ci->intr = 1;
		scipcomplete(ci, li, scipreq, req);
		scipqintr(ci);
	}
	while (scipstart(ci)) ;	/* yes, a null while */
scipintr_end:
	wbflush();
	UNLOCK(ci->lock);
    }
}

#include <sys/iograph.h>
#define NUMOFCHARPERINT	10
/*
 * create a vertex for the scip controller and add the info
 * associated with it
 */
vertex_hdl_t
scip_ctlr_add(int adap)
{
	vertex_hdl_t		ctlr_vhdl;
	char			loc_str[8];
	/*REFERENCED*/
	graph_error_t		rv;
	scsi_ctlr_info_t	*ctlr_info;

	sprintf(loc_str,"%s/%d",EDGE_LBL_SCSI_CTLR,SCSI_EXT_CTLR(adap));
	rv = hwgraph_path_add(	hwgraph_root,
				loc_str,
				&ctlr_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (ctlr_vhdl != GRAPH_VERTEX_NONE));
	
	
	ctlr_info = (scsi_ctlr_info_t *)hwgraph_fastinfo_get(ctlr_vhdl);
	if (!ctlr_info) {
	    ctlr_info 			= scsi_ctlr_info_init();
	    SCI_ADAP(ctlr_info)		= adap;
	    SCI_CTLR_VHDL(ctlr_info)	= ctlr_vhdl;

	    SCI_ALLOC(ctlr_info)	= scipalloc;
	    SCI_COMMAND(ctlr_info)	= scipcommand;
	    SCI_FREE(ctlr_info)		= scipfree;
	    SCI_DUMP(ctlr_info)		= scipcrashdump;
	    SCI_INQ(ctlr_info)		= scipinfo;
	    SCI_IOCTL(ctlr_info)	= scipioctl;
	    SCI_ABORT(ctlr_info)	= scipabort;
	    scsi_ctlr_info_put(ctlr_vhdl,ctlr_info);
	    scsi_bus_create(ctlr_vhdl);
	    
	}
	hwgraph_vertex_unref(ctlr_vhdl);
	return ctlr_vhdl;
}

/*
 * Probe all possible targets to see what's installed on the system.
 * For now, probe only LUN 0.
 */
/* ARGSUSED */
int
scipedtinit(struct edt *e)
{
	struct scipctlrinfo	*ci;
	u_char			 adap;
	int			 i;
	int			 j;
	vertex_hdl_t		 ctlr_vhdl, lun_vhdl;

	scipinittime = 1;
	scip_mintimeout *= HZ;
	for (adap = 8; adap <= scip_maxadap; adap++)
	{
		if ((ci = scipctlr[adap]) == NULL)
			continue;
		IOLOCK(ci->lock, i);
		scip_reset(ci, SC_ATTN, 1);
		IOUNLOCK(ci->lock, i);
	}
	for (adap = 0; adap <= scip_maxadap; adap++)
	{
		if ((ci = scipctlr[adap]) == NULL)
			continue;
		ctlr_vhdl  = scip_ctlr_add(adap);
		for (i = 0; i < SCIP_MAXTARG; i++)
		{
			init_mutex(&ci->unit[i]->opensema, MUTEX_DEFAULT,
				   "scp", adap*16 + i);
			lun_vhdl = scsi_device_add(ctlr_vhdl,i,0);
			if (!(scipinfo(lun_vhdl)))
				scsi_device_remove(ctlr_vhdl,i,0);
			if (ci->unit[i]->present)
				for (j = 1; j < 8; j++) {
					lun_vhdl = scsi_device_add(ctlr_vhdl,i,j);
					if (!(scipinfo(lun_vhdl)))
						scsi_device_remove(ctlr_vhdl,i,j);
				}
		}
		device_inventory_add(ctlr_vhdl,INV_DISK,INV_SCSICONTROL,SCEXT(adap),
				      (ci->diff << 7),INV_SCIP95);
		printf("SCIP SCSI controller %d - %s, %s, min xfer period %dns\n",
		       SCEXT(adap),
		       ci->diff ? "differential" : "single ended",
		       ci->external ? "external" : "internal",
		       ci->unit[0]->min_syncperiod * 4);
	}
	scipinittime = 0;
	return 0;
}


/*
 * Control DMA download -- does one transfer only
 */
static int
scipctrldma(volatile u_char *scipbase, char *addr, int sramloc, int count,
	    int direction)
{
	uint	page;

	if (direction == 1)	/* read from SCIP SRAM */
		direction = 0xC020;
	else
		direction = 0xC018;
	SCIP_ADDR(0xC000, scipbase) = sramloc;
	page = ev_kvtoiopnum(addr);
	SCIP_ADDR(0xC008, scipbase) = page >> 20;
	SCIP_ADDR(0xC010, scipbase) = io_ctob(page) | io_poff(addr);
	SCIP_ADDR(direction, scipbase) = count;
	while (!(SCIP_ADDR(direction, scipbase) & (3 << 12)));
	if (SCIP_ADDR(direction, scipbase) & (1 << 13))
		return 1;
	return 0;
}


/*
 * Initializes the wd95 controller.
 * In theory, this function can be replaced by a similar one if other
 * controllers are ever used.
 */
#include <io/wd95a_wcs.h>
static int
scip_wd95init(volatile char *wd95addr)
{
	if (!wd95_present(wd95addr))
		return 1;
	init_95a_wcs(wd95addr, WCS_SCIP);
	if (init_wd95a(wd95addr, scip_hostid, NULL))
		return 1;
	return 0;
}


/*
 * Send initializtion IOCB.
 */
static int
scip_init(struct scipctlrinfo *ci)
{
	struct scip_resetcb	*init;
	int			 err;
	uint			 sraminitaddr;

	/*
	 * Setup up init block (which is actually a reset cb) and put
	 * it down on the SCIP via control DMA.
	 */
	init = (struct scip_resetcb *) kvpalloc(1, VM_DIRECT, 0);

	resetcb_setup(ci, init);
	sraminitaddr = SCIPSRAM_INITBLOCK + 0x800 * ci->chan;
	err = scipctrldma(ci->chip_addr, (char *) init, sraminitaddr, 128, 0);

	kvpfree(init, 1);
	return err;
}


/*
 * Clear SCIP memory with bad parity.
 * Download firmware to the SCIP and check for signs of life.
 * Returns non-zero if there is an error.
 */
int
scipfw_download(volatile u_char *scipbase)
{
	char			*fw;
	int	 		 i, j;
	int	 		 scipfwsegments;
	int			 err = 0;

	if (SCIP_ADDR(0x6000, scipbase) & 0x4000) /* clear illegal PIO error */
		SCIP_ADDR(0x6000, scipbase) = 0x4000;
	SCIP_ADDR(0xC028, scipbase) = 0;	/* clear control DMA error */
	SCIP_ADDR(0xA000, scipbase) = 0x701;	/* reset PCs for all tasks */
	SCIP_ADDR(0xA000, scipbase) = 0x1700;	/* clear SRAM parity error */
	SCIP_ADDR(0xA000, scipbase) = 0x10000;	/* clear PIO timeout error */
	SCIP_ADDR(0xA008, scipbase) = 0xFE0000; /* clear parity error bits */

	SCIP_ADDR(0x6000, scipbase) = 0x10;  /* mux sel for WE clock edge */

	fw = kvpalloc(btoc(SCIP_SRAMSIZE), VM_DIRECT, 0);
	bzero(fw, SCIP_SRAMSIZE);
	SCIP_ADDR(0xA008, scipbase) = 0x1000000; /* force bad parity */
	for (i = 0; i < SCIP_MEMSIZE; i += 128)
		err |= scipctrldma(scipbase, &fw[i], i, 128, 0);
	SCIP_ADDR(0xA008, scipbase) = 0; /* clear bad parity */

	scipfwsegments = sizeof(firmware_s) / sizeof(firmware_s[0]);
	for (i = 0; i < scipfwsegments; i++)
	{
		bcopy(firmware_s[i].fragment, &fw[firmware_s[i].addr - 0x20000],
		      firmware_s[i].size);
		for (j = 0; j < firmware_s[i].size; j += 8)
			err |= scipctrldma(scipbase,
					&fw[j + firmware_s[i].addr - 0x20000],
					j + firmware_s[i].addr, 8, 0);
	}

#if 0
	for (i = 0; i < scipfwsegments; i++)
	{
		for (j = 0; j < firmware_s[i].size; j+= 8)
			err |= scipctrldma(scipbase,
				&fw[j + firmware_s[i].addr - 0x20000],
				j + firmware_s[i].addr, 8, 1);
		if (bcmp(firmware_s[i].fragment,
			 &fw[firmware_s[i].addr - 0x20000],
			 firmware_s[i].size))
			err = 1;
	}
#endif

	kvpfree(fw, btoc(SCIP_SRAMSIZE));
	return err;
}


/*
 * start up scip
 */
int
scip_startup(volatile u_char *scipbase)
{
	SCIP_ADDR(0xA000, scipbase) = 0x8001;	/* disable passthru SCSI intrs */
	SCIP_ADDR(0xA000, scipbase) = 0x11;	/* enables task 0 */
	SCIP_ADDR(0xA000, scipbase) = 0x21;	/* enables task 1 */
	SCIP_ADDR(0xA000, scipbase) = 0x41;	/* enables task 2 */
	/* XXX should check for life */
	return 0;
}


#define SET_WD_REG(baseaddr, regno, data) baseaddr[(regno) * 8 + 7] = (data)
#define GET_WD_REG(baseaddr, regno) (baseaddr[(regno) * 8 + 7])

int
wd95diffsense(register volatile char *sbp)
{
	char	cver;
	int	ret;

	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
	cver = GET_WD_REG(sbp, WD95A_S_CVER);
	if (cver & W95_CVER_SE)
		ret = 0;
	else if (cver & W95_CVER_DSENS)
		ret = 1;
	else
		ret = -1;
	SET_WD_REG(sbp, WD95A_S_CTL, 0);
	return ret;
}


/*
 * Allocate controller data structures (everything but scipluninfo structures).
 */
int
scip_earlyinit(int adap)
{
	struct scipctlrinfo	*ci;
	struct scipunitinfo	*ui;
	struct sciprequest	*scrq;
	struct scip_scb		*scb;
	struct scip_scb		*mcb;
	struct scip_scb		*hcb;
	struct scip_scb		*ecb;
	struct scipfinishinfo	*finish;
	volatile char		*wd95addr;
	int			 j;
	u_char			 tmp;
	int			 diffinfo;
	int			 external;
	extern u_char		 scip_syncfactor[];

	if (scip_hostid == 0)
	{
		if ((arg_wd95hostid[0] >= '0') && (arg_wd95hostid[0] <= '9'))
			scip_hostid = arg_wd95hostid[0] - '0';
		else
		if ((arg_wd95hostid[0] >= 'a') && (arg_wd95hostid[0] <= 'f'))
		        scip_hostid = arg_wd95hostid[0] - 'a' + 10;
		else
		if ((arg_wd95hostid[0] >= 'A') && (arg_wd95hostid[0] <= 'F'))
		        scip_hostid = arg_wd95hostid[0] - 'A' + 10;
	}
	wd95addr = (volatile char *) scuzzy[adap].d_addr;
	if (wd95addr == NULL)
		return -1;
	diffinfo = wd95diffsense(wd95addr);
	if (diffinfo < 0)
		printf("scip%d: differential bus with single ended drive"
		       " connected - bypassing\n", SCEXT(adap));
	external = scsi_is_external(adap);

	ci = kern_calloc(sizeof(*ci), 1);
	ui = kern_calloc(sizeof(*ui), SCIP_MAXTARG);
	/*
	 * The SCIP requires that the size of the SCB array be a power of
	 * two and that it start on a boundary equal to its size.  The memory
	 * is never freed once allocated, so we can adjust the pointer withou
	 * worrying about keeping the original.
	 */
	scb = kvpalloc(2 * btoc(SCIP_SCBSIZE * SCIP_SCBNUM) - 1, VM_DIRECT, 0);
	scb = (struct scip_scb *)
		(((ulong) scb + (SCIP_SCBSIZE * SCIP_SCBNUM) - 1) &
		~((SCIP_SCBSIZE * SCIP_SCBNUM) - 1));
	bzero(scb, SCIP_SCBSIZE * SCIP_SCBNUM);
	mcb = kvpalloc(btoc(SCIP_SCBSIZE * SCIP_MCBNUM), VM_DIRECT, 0);
	bzero(mcb, SCIP_SCBSIZE * SCIP_MCBNUM);
	hcb = kmem_zalloc(SCIP_SCBSIZE, VM_DIRECT | KM_CACHEALIGN);
	ecb = kmem_zalloc(SCIP_SCBSIZE, VM_DIRECT | KM_CACHEALIGN);
	finish = kmem_zalloc(sizeof(*finish), VM_DIRECT | KM_CACHEALIGN);
	scrq = kern_calloc(SCIP_FLOATQUEUE, sizeof(struct sciprequest));

	for (j = 0; j < SCIP_MAXTARG; j++)
	{
		ci->unit[j] = &ui[j];
		ui[j].number = j;
		tmp = scip_syncfactor[adap];
		if (tmp < 25)
		{
			if (external && !diffinfo)
				tmp = 50;
			else
				tmp = 25;
		}
		ui[j].min_syncperiod = tmp;
		ui[j].max_syncoffset = 32;
		ui[j].unitreq.dmamap = dma_mapalloc(DMA_SCSI, adap, 0, 0);
	}

	ci->number = adap;
	ci->diff = diffinfo;
	ci->external = external ? 1 : 0;
	ci->scb = scb;
	ci->scb_cqremove = scb;
	ci->mcb = mcb;
	ci->hcb = hcb;
	ci->ecb = ecb;
	ci->finish = finish;
	ci->chan = scuzzy[adap].channel;
	ci->chip_addr = scuzzy[adap].s1_base;

	for (j = 0; j < SCIP_FLOATQUEUE; j++)
	{
		scrq[j].next = &scrq[j+1];
		scrq[j].dmamap =
			dma_mapalloc(DMA_SCSI, adap, scip_dmapages[adap], 0);
	}
	scrq[SCIP_FLOATQUEUE - 1].next = NULL;
	ci->request = scrq;

	SCIP_REG(0x6000, ci) = 0x10;    /* mux sel for WE clock edge */

	if (scip_wd95init(wd95addr))
		goto err_init;

	if (scip_init(ci))
		goto err_init;

	/*
	 * Initialize all data structures necessary to do probing
	 */
	ci->page = kvpalloc(1, VM_DIRECT, 0);
	INITLOCK(&ci->lock, "scl", adap);
	INITLOCK(&ci->intrlock, "sci", adap);
	INITLOCK(&ci->scanlock, "scs", adap);

	scipctlr[adap] = ci;
	return 0;

err_init:
	scipctlr[adap] = NULL;
	return -1;
}


#if SCIPDEBUG
void scip_reset_all(int);
void scip_clr_pty(int);

void
scipdump(int ctlr, int sramloc)
{
	struct scipctlrinfo	*ci;
	uint			 page;
	int			 count=128;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	SCIP_REG(0xC000, ci) = sramloc;
	page = ev_kvtoiopnum((caddr_t) ci->ecb);
	SCIP_REG(0xC008, ci) = page >> 20;
	SCIP_REG(0xC010, ci) = io_ctob(page) | io_poff(ci->ecb);
	SCIP_REG(0xC020, ci) = count;
	while (!(SCIP_REG(0xC020, ci) & (3 << 12)));
	if (SCIP_REG(0xC020, ci) & (1 << 13))
	{
		qprintf("scip control DMA error\n");
		return;
	}
	qprintf("SCIP sram data:");
	for (page = 0; page < 32; page++)
	{
		if (page % 4 == 0)
			qprintf("\n%x ", sramloc + (page*4));
		qprintf("0x%x ", ((int *) ci->ecb)[page]);
	}
}


int
scipcdma_rd(int ctlr, int sramloc, int length, int *addr)
{
	struct scipctlrinfo	*ci;
	uint			 page;
	int                      counter;
	int                      csr;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return 1;
	}

	if (length > 128)
	{
		qprintf("length of %d is too big, 128 will be used\n", length);
		length = 128;
	}

	SCIP_REG(0xC000, ci) = sramloc;
	page = ev_kvtoiopnum((caddr_t) addr);
	SCIP_REG(0xC028, ci) = 0;          /* clear CDMA mach just in case */
	SCIP_REG(0xC008, ci) = page >> 20;
	SCIP_REG(0xC010, ci) = io_ctob(page) | io_poff(addr);
	SCIP_REG(0xC020, ci) = length;
	csr = 0;
	counter = 0;
	while (!(csr & (3 << 12)) && (counter < 1000))
	{
	  csr = SCIP_REG(0xC020, ci);
	  counter = counter + 1;
	}
	if ((SCIP_REG(0xC020, ci) & (1 << 13)) || (counter >= 1000))
	{
		qprintf("scip control DMA error\n");
		return 1;
	}
	return 0;

}

void
scip_cdma_rd(int ctlr, int sramloc, int length)
{
	struct scipctlrinfo	*ci;
        int      *fw;
	uint     page;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	if ((length > 128) || (length < 0))
	{
		qprintf("length of %d is too big or undefined, 128 will be used\n", length);
		length = 128;
	}

	fw = kvpalloc(btoc(128), VM_DIRECT, 0);
	scipcdma_rd(ctlr, sramloc, length, fw);
	qprintf("\nSCIP cdma data:");
	for (page = 0; page < length/4; page++)
	{
		if (page % 4 == 0)
			qprintf("\n%x ", sramloc + (page*4));
		qprintf("0x%x ", fw[page]);
	}
	qprintf("\n");
	kvpfree(fw, btoc(128));
}	

int
scipcdma_wr(int ctlr, int sramloc, int length, int *addr)
{
	struct scipctlrinfo	*ci;
	uint	page;
	int     csr;
	int     counter;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return 0;
	}

	SCIP_REG(0xC000, ci) = sramloc;
	page = ev_kvtoiopnum((caddr_t) addr);
	SCIP_REG(0xC028, ci) = 0;          /* clear CDMA mach just in case */
	SCIP_REG(0xC008, ci) = page >> 20;
	SCIP_REG(0xC010, ci) = io_ctob(page) | io_poff(addr);
	SCIP_REG(0xC018, ci) = length;
	csr = 0;
	counter = 0;
	while (!(csr & (3 << 12)) && (counter < 1000))
	{
	  csr = SCIP_REG(0xC020, ci);
	  counter = counter + 1;
	}
	if ((SCIP_REG(0xC020, ci) & (1 << 13)) || (counter >= 1000))
	{
		qprintf("scip control DMA error\n");
		return 1;
	}
	return 0;
      }

void
scip_cdma_wr(int ctlr, int sramloc, int length)
{
	struct scipctlrinfo	*ci;
        int                     *fw;
	uint                    page;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

/*	if (length > 128)
	{
		qprintf("length of %d is too big, 128 will be used\n", length);
		length = 128;
	}
*/
	fw = kvpalloc(btoc(128), VM_DIRECT, 0);
	fw[0] = 0x5a5a5a5a;
	fw[1] = 0xa5a5a5a5;
	page = 0;
	while (!(scipcdma_wr(ctlr, sramloc, 8, fw)) && page < length)
	  {
	    page = page + 8;
	    scip_reset_all(ctlr);
	    scip_clr_pty(ctlr);
	  }
	qprintf("\nSCIP cdma data written\n");
	kvpfree(fw, btoc(128));

}	

void
scip_dbg(int length, int val1, int val2)
{
	struct scipctlrinfo	*ci;
        int                     *fw;
	uint                    page;
	int                     ctlr = 5;
	int                     sramloc = 0;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

/*	if (length > 128)
	{
		qprintf("length of %d is too big, 128 will be used\n", length);
		length = 128;
	}
*/
	fw = kvpalloc(btoc(128), VM_DIRECT, 0);
	fw[0] = val1;
	fw[1] = val2;
	page = 0;
	while (!(scipcdma_wr(ctlr, sramloc, 8, fw)) && page < length)
	  {
	    page = page + 8;
	    scip_reset_all(ctlr);
	    scip_clr_pty(ctlr);
	  }
	qprintf("\nSCIP cdma data written\n");
	kvpfree(fw, btoc(128));

}	

void
scip_rdwr_loop(int length, int val1, int val2)
{
	struct scipctlrinfo	*ci;
        int                     *fwout;
        int                     *fwin;
	uint                    page;
	int                     ctlr = 5;
	int                     sramloc = 0;
	int                     rtnc = 0;
	uint			data;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

/*	if (length > 128)
	{
		qprintf("length of %d is too big, 128 will be used\n", length);
		length = 128;
	}
*/
	fwin  = kvpalloc(btoc(128), VM_DIRECT, 0);
	fwout = kvpalloc(btoc(128), VM_DIRECT, 0);
	fwout[0] = val1;
	fwout[1] = val2;
	page = 0;
	while (page < length)
	  {
	    scipcdma_wr(ctlr, sramloc, 8, fwout);
	    page = page + 1;
	    rtnc = scipcdma_rd(ctlr, sramloc, 8, fwin);
	    if ((rtnc != 0) || ((fwout[0] != fwin[0]) | (fwout[1] != fwin[1])))
	      {
		if (rtnc != 0)
		  {
		    qprintf("rd/wrt parity error on loop # %d  --- ", page);
		    data = SCIP_REG(0xA008, ci);
		    qprintf("SRAM Error Reg  = 0x%x\n", data);
		  }
		else
		  {
		    qprintf("rd/wrt miscompare on loop # %d\n ", page);
		    qprintf("    wrt data is %h %h\n", fwout[0], fwout[1]);
		    qprintf("    rd  data is %h %h\n", fwin[0],  fwin[1]);
		  }
		scip_reset_all(ctlr);
		scip_clr_pty(ctlr);
	      }
	  }
	qprintf("\nSCIP cdma data written\n");
	kvpfree(fwin, btoc(128));
	kvpfree(fwout, btoc(128));

}	

void
scip_wrt_lm(int ctrl, int sramloc, int value)
{

	struct scipctlrinfo	*ci;
        int      *fw;
	uint     page;

	ci = scipctlr[ctrl];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctrl);
		return;
	}

	fw = kvpalloc(btoc(8), VM_DIRECT, 0);

        scipcdma_rd(ctrl, sramloc, 8, fw);
	fw[(sramloc % 8)/4] = value;
	page = ev_kvtoiopnum((caddr_t) fw);

	SCIP_REG(0xC028, ci) = 0;   /* clear CDMA mach just in case */
	SCIP_REG(0xC000, ci) = sramloc;
	SCIP_REG(0xC008, ci) = page >> 20;
	SCIP_REG(0xC010, ci) = io_ctob(page) | io_poff(fw);
	SCIP_REG(0xC018, ci) = 8;

	while (!(SCIP_REG(0xC020, ci) & (3 << 12)));
	  
	kvpfree(fw, btoc(128));
}


int
scip_fw_check(int ctlr)
{
	int *fw;
	int i;
	int dataadr;
	int err = 0;
	int rtnc;

	fw = kvpalloc(btoc(firmware_s[0].size), VM_DIRECT, 0);
	for (i = 0; i < firmware_s[0].size; i += 8)
	  {
            rtnc = scipcdma_rd(ctlr, firmware_s[0].addr + i, 8, &fw[i/sizeof(int)]);
	    if (rtnc != 0)
	      return(1);
	  }
	for (i = 0; i < firmware_s[0].size / sizeof(int); i++)
		if (fragment0[i] != fw[i])
		{
		        dataadr = firmware_s[0].addr + (i*4);
			qprintf("miscompare at instruction %x\n", dataadr);
			qprintf("data is %x, should be %x\n",
				fw[i], fragment0[i]);
			err = 1;
		}
	if (err == 0)
		qprintf("no errors\n");
	kvpfree(fw, btoc(firmware_s[0].size));
	return err;
}

void
scip_get_rf(int ctlr)
{
	struct scipctlrinfo	*ci;
	uint			 page;
	int                      sramloc=0x2f800+1024+256;
	int                      *regfile;
	int                      channel;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}
	channel = ci->chan;

	SCIP_REG(0xA000, ci) = 0x10 << channel;       /* turn off enable bit */
	SCIP_REG(0xA000, ci) = (0x100 << channel) | 1;/* turn on  reset      */

	SCIP_REG(0xA000, ci) = (0x100 << channel);    /* turn off reset      */
	SCIP_REG(0xA000, ci) = (0x10 << channel) | 1; /* turn on  enable bit */

	regfile = kvpalloc(btoc(128), VM_DIRECT, 0);
	scipcdma_rd(ctlr, sramloc, 32, regfile);

	qprintf("SCIP register file task %x data:", channel);
	for (page = 0; page < 8; page++)
	{
		qprintf("\nreg %x  =  ", page);
		qprintf("0x%x ", regfile[page]);
	}
	qprintf("\n");
}


void
scip_status(int ctlr)
{
	struct scipctlrinfo	*ci;
	uint			 data;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	data = SCIP_REG(0x6000, ci);
	qprintf("\nCmd and Status Reg = 0x%x", data);

	data = SCIP_REG(0x7028, ci);
	qprintf("\nIBUS Error Reg     = 0x%x", data);

	data = SCIP_REG(0x7030, ci);
	qprintf("\nIBUS Error Log Hi  = 0x%x", data);

	data = SCIP_REG(0x7038, ci);
	qprintf("\nIBUS Error Log Lo  = 0x%x", data);

	data = SCIP_REG(0xA000, ci);
	qprintf("\nproc status reg    = 0x%x", data);

	data = SCIP_REG(0xA008, ci);
	qprintf("\nSRAM Error Reg     = 0x%x", data);

	data = SCIP_REG(0xC030, ci);
	qprintf("\nDMA Read Timout    = 0x%x", data);

	qprintf("\n");
	qprintf("**** CDMA Registers ****");
	
	data = SCIP_REG(0xc000, ci);
	qprintf("\nSRAM Address       = 0x%x", data);
	data = SCIP_REG(0xc008, ci);
	qprintf("\nPhys Address High  = 0x%x", data);
	data = SCIP_REG(0xc010, ci);
	qprintf("\nPhys Address Low   = 0x%x", data);
	data = SCIP_REG(0xc018, ci);
	qprintf("\nRead Count         = 0x%x", data);
	data = SCIP_REG(0xc020, ci);
	qprintf("\nWrite Count        = 0x%x", data);


	qprintf("\n");
	qprintf("**** TASK 0 Registers ****");

	data = SCIP_REG(0x8018, ci);
	qprintf("\nPC                 = 0x%x", data);

	data = SCIP_REG(0x8010, ci);
	qprintf("\nAdd Offset         = 0x%x", data);

	data = SCIP_REG(0x8000, ci);
	qprintf("\nPause Status       = 0x%x", data);

	data = SCIP_REG(0x8008, ci);
	qprintf("\nPause Mask         = 0x%x", data);

	qprintf("\n");
	qprintf(" **** TASK 1 Registers ****");

	data = SCIP_REG(0x8818, ci);
	qprintf("\nPC                 = 0x%x", data);

	data = SCIP_REG(0x8810, ci);
	qprintf("\nAdd Offset         = 0x%x", data);

	data = SCIP_REG(0x8800, ci);
	qprintf("\nPause Status       = 0x%x", data);

	data = SCIP_REG(0x8808, ci);
	qprintf("\nPause Mask         = 0x%x", data);

	qprintf("\n");
	qprintf("**** TASK 2 Registers ****");

	data = SCIP_REG(0x9018, ci);
	qprintf("\nPC                 = 0x%x", data);

	data = SCIP_REG(0x9010, ci);
	qprintf("\nAdd Offset         = 0x%x", data);

	data = SCIP_REG(0x9000, ci);
	qprintf("\nPause Status       = 0x%x", data);

	data = SCIP_REG(0x9008, ci);
	qprintf("\nPause Mask         = 0x%x", data);

	qprintf("\n");

}


void
scip_clr_pty(int ctlr)
{
	struct scipctlrinfo	*ci;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	
	SCIP_REG(0xC028, ci) = 0;
	SCIP_REG(0xA008, ci) = 0;
	SCIP_REG(0xA000, ci) = 0x1000;

}

void
scip_reset_all(int ctlr)
{
	struct scipctlrinfo	*ci;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	SCIP_REG(0xA000, ci) = 0x70;       /* turn off enable bit */
	SCIP_REG(0xA000, ci) = 0x701;      /* turn on  reset      */
}


void
scip_en_chan(int ctlr)
{
	struct scipctlrinfo	*ci;
	int                      channel;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}
	channel = ci->chan;

	SCIP_REG(0xA000, ci) = (0x100 << channel);    /* turn off  reset    */
	SCIP_REG(0xA000, ci) = (0x10 << channel) | 1; /* turn on enable bit */
}


void
scip_wdreg_rd(int ctlr, int regadr)
{
  	struct scipctlrinfo	*ci;
	int                     regdata;
	int                     offset;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	if (regadr == 0xff)
	  {
	    qprintf("possible registers are:\n");
	    qprintf("0x0    CTL      - Control Register\n");
	    qprintf("0x2    CTLA     - Control Register, Auxillary\n");
	    qprintf("0x4    ISR      - Interrupt Status Register\n");
	    qprintf("0x6    STOPU    - nexpected Stop Status Register\n");
	    qprintf("0x8    UEI      - Unexpected Event Intr Register\n");
	    qprintf("0xa    ISRM     - ISR Mask Register\n");
	    qprintf("0xc    STOPUM   - STOPU Mask Register\n");
	    qprintf("0xe    UEIM     - UEI Mask Register\n");
	    qprintf("0x10   RESPONSE - Automatic Response Control\n");
	    qprintf("0x12   SQINT    - WCS Sequencer Interrupt Addr\n");
	    qprintf("0x14   SQADR    - Current addr WCS is executing\n");
	    qprintf("0x16   STC      - SCSI Transfer Control Register\n");
	    qprintf("0x18   SPW      - SCSI Pulse Width Control Reg\n");
	    qprintf("0x1a   DESTID   - Destination ID Register\n");
	    qprintf("0x1c   SRCID    - Source ID Register\n");
	    qprintf("0x1e   FLAG     - Flag Register\n");
	    qprintf("0x20   TC_0_7   - Transfer Count 0..7\n");
	    qprintf("0x22   TC_8_15  - Transfer Count 8..15\n");
	    qprintf("0x24   TC_16_23 - Transfer Count 16..23\n");
	    qprintf("0x26   DATA     - Data Access Port\n");
	    qprintf("0x28   SR       - Status Register\n");
	    qprintf("0x2a   FIFOS    - Fifo Status Register\n");
	    return;
	  }


	offset = (0x800 * ci->chan) + (regadr << 3);
	regdata = SCIP_REG(offset, ci);
	regdata = regdata & 0xff;
	qprintf("\nRegister %x data = 0x%x", regadr, regdata);

	if (regadr == WD95A_N_ISR)
	  {
	    if (regdata & W95_ISR_STOPWCS)
	      qprintf("\n   STOPWCS");
	    if (regdata & W95_ISR_INTWCS)
	      qprintf("\n   INTWCS");
	    if (regdata & W95_ISR_STOPU)
	      qprintf("\n   STOPU");
	    if (regdata & W95_ISR_UEI)
	      qprintf("\n   UEI");
	    if (regdata & W95_ISR_BUSYI)
	      qprintf("\n   BUSYI");
	    if (regdata & W95_ISR_VBUSYI)
	      qprintf("\n   VBUSYI");
	    if (regdata & W95_ISR_SREJ)
	      qprintf("\n   SREJ");
	    if (regdata & W95_ISR_INT0)
	      qprintf("\n   INT0");
	  }
	if (regadr == WD95A_N_STOPU)
	  {
	    if (regdata & W95_STPU_ABORTI)
	      qprintf("\n   ABORTI");
	    if (regdata & W95_STPU_SCSIT)
	      qprintf("\n   SCSIT");
	    if (regdata & W95_STPU_PARE)
	      qprintf("\n   PARE");
	    if (regdata & W95_STPU_LRCE)
	      qprintf("\n   LRCE");
	    if (regdata & W95_STPU_TCUND)
	      qprintf("\n   TCUND");
	    if (regdata & W95_STPU_SOE)
	      qprintf("\n   SOE");
	  }
	if (regadr == WD95A_N_UEI)
	  {
	    if (regdata & W95_UEI_FIFOE)
	      qprintf("\n   FIFOE");
	    if (regdata & W95_UEI_ATNI)
	      qprintf("\n   ATNI");
	    if (regdata & W95_UEI_UDISC)
	      qprintf("\n   UDISC");
	    if (regdata & W95_UEI_TCOVR)
	      qprintf("\n   TCOVR");
	    if (regdata & W95_UEI_USEL)
	      qprintf("\n   USEL");
	    if (regdata & W95_UEI_URSEL)
	      qprintf("\n   URSEL");
	    if (regdata & W95_UEI_UPHAS)
	      qprintf("\n   UPHAS");
	    if (regdata & W95_UEI_RSTINTL)
	      qprintf("\n   RSTINTL");
	  }

	qprintf("\n");
      }

void
scip_wdreg_wr(int ctlr, int regadr, int regdata)
{
  	struct scipctlrinfo	*ci;
	int                     offset;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	offset = (0x800 * ci->chan) + (regadr << 3);
	SCIP_REG(offset, ci) = regdata;
      }

void
scip_wcdpr_rd(int ctlr, int dpradr)
{
  	struct scipctlrinfo	*ci;
	int                     dprdata;
	int                     offset;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	offset = ((64 + (dpradr << 1)) << 3) + (0x800 * ci->chan);
	dprdata = SCIP_REG(offset, ci);
	dprdata = dprdata & 0xff;
	qprintf("\nRegister %x data = 0x%x", dpradr, dprdata);
	qprintf("\n");
      }

void
scip_wcdpr_wr(int ctlr, int dpradr, int dprdata)
{
  	struct scipctlrinfo	*ci;
	int                     offset;

	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	offset = ((64 + (dpradr << 1)) << 3) + (0x800 * ci->chan);
	SCIP_REG(offset, ci) = dprdata;
      }

void
scip_lm_scb(int ctlr, int sramloc)
{
	struct scipctlrinfo	*ci;
        int                     *fw1;
	int                     length = 0x80;
	struct scip_scb         *fw;
	int                     i;


	ci = scipctlr[ctlr];
	if (ci == NULL)
	{
		qprintf("scip controller %d not configured\n", ctlr);
		return;
	}

	fw1 = kvpalloc(btoc(128), VM_DIRECT, 0);
	scipcdma_rd(ctlr, sramloc, length, fw1);
	fw = (struct scip_scb * )fw1;
	qprintf("\nLocal Memory SCB at location %h:\n", sramloc);
	qprintf("op/tar/lun    = %x %x 5x\n", fw->opcode,
					      fw->target,
					      fw->lun);
	qprintf("type/xfer/off = %x %x %x\n", fw->tag_type,
					      fw->xferfactor,
					      fw->offset);
	qprintf("flags         = %x\n", fw->flags);
	qprintf("scsi_cmd      = %x", fw->scsi_cmd[0]);
	for (i = 1; i < fw->cmd_length; i++)
	  {
	    qprintf(" %x", fw->scsi_cmd[i]);
	  }
	qprintf("\n");
	qprintf("cmd_length    = %x\n", fw->cmd_length);
	qprintf("dma_xlatehi   = %x\n", fw->dma_xlatehi);
	qprintf("dma_offset    = %x\n", fw->dma_offset);
	qprintf("dma_xlatelo   = %x\n", fw->dma_xlatelo);
	qprintf("dma_length    = %x\n", fw->dma_length);
	qprintf("sense_length  = %x\n", fw->sense_length);
	qprintf("sense_xlatehi = %x\n", fw->sense_xlatehi);
	qprintf("sense_offset  = %x\n", fw->sense_offset);
	qprintf("sense_xlatelo = %x\n", fw->sense_xlatelo);
	qprintf("scsi_status   = %x\n", fw->scsi_status);
	qprintf("scip_status   = %x\n", fw->scip_status);
	qprintf("sense_gotten  = %x\n", fw->sense_gotten);
	qprintf("odd_byte      = %x\n", fw->odd_byte);
	qprintf("dma_resid     = %x\n", fw->dma_resid);
	qprintf("error_data_0  = %x\n", fw->error_data[0]);
	qprintf("error_data_1  = %x\n", fw->error_data[1]);
	qprintf("error_data_2  = %x\n", fw->error_data[2]);
	qprintf("op_id         = %x\n", fw->op_id);

	qprintf("\n");
	kvpfree(fw1, btoc(128));
}	

void
scip_mem_scb(struct scip_scb *memloc)
{
        int              i;

	qprintf("\nSystem Memory SCB at location %x:\n", memloc);
	qprintf("op/tar/lun    = %x %x 5x\n", memloc->opcode,
					      memloc->target,
					      memloc->lun);
	qprintf("type/xfer/off = %x %x %x\n", memloc->tag_type,
					      memloc->xferfactor,
					      memloc->offset);
	qprintf("flags         = %x\n", memloc->flags);
	qprintf("scsi_cmd      = %x", memloc->scsi_cmd[0]);
	for (i = 1; i < memloc->cmd_length; i++)
	  {
	    qprintf(" %x", memloc->scsi_cmd[i]);
	  }
	qprintf("\n");
	qprintf("cmd_length    = %x\n", memloc->cmd_length);
	qprintf("dma_xlatehi   = %x\n", memloc->dma_xlatehi);
	qprintf("dma_offset    = %x\n", memloc->dma_offset);
	qprintf("dma_xlatelo   = %x\n", memloc->dma_xlatelo);
	qprintf("dma_length    = %x\n", memloc->dma_length);
	qprintf("sense_length  = %x\n", memloc->sense_length);
	qprintf("sense_xlatehi = %x\n", memloc->sense_xlatehi);
	qprintf("sense_offset  = %x\n", memloc->sense_offset);
	qprintf("sense_xlatelo = %x\n", memloc->sense_xlatelo);
	qprintf("scsi_status   = %x\n", memloc->scsi_status);
	qprintf("scip_status   = %x\n", memloc->scip_status);
	qprintf("sense_gotten  = %x\n", memloc->sense_gotten);
	qprintf("odd_byte      = %x\n", memloc->odd_byte);
	qprintf("dma_resid     = %x\n", memloc->dma_resid);
	qprintf("error_data_0  = %x\n", memloc->error_data[0]);
	qprintf("error_data_1  = %x\n", memloc->error_data[1]);
	qprintf("error_data_2  = %x\n", memloc->error_data[2]);
	qprintf("op_id         = %x\n", memloc->op_id);

	qprintf("\n");
}	

#endif

#endif /* Do we want to build this? */
