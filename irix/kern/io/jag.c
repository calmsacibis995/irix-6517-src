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
#ident	"$Revision: 1.79 $"
#include <bstring.h>
#include <string.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/invent.h>
#include <sys/dmamap.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/vmereg.h>
#include <sys/pio.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/scsi.h>
#include <sys/jag.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <sys/errno.h>

#define JDEBUG 0
#if JDEBUG
int		 jagtrace = 0;
int		 jagqueue = 0x1000;
#define jdprintf(x) printf x
#else
#define jagqueue 0
#define jdprintf(x)
#endif

int		 jag_is_initted;	/* suppress selection timeout msgs when set */

/*
 * The following arrays are for conversions from internal unit number
 * to SCSI id, and from device number to internal unit number.
 */
char	jag_scsi_id[JAG_UPC];	/* internal unit number to SCSI id */
signed char jag_unitnum[JAG_LUPC]; /* device number to internal unit number */
char	jag_devnum[JAG_UPC];	/* internal unit number to device number */


/* Forward declarations */
void				 jagintr(int);
static void			 jagtimeout(struct jagrequest *);
static struct jagmemcqe		*getcqe(uint);
static volatile struct jag_cqe 	*getvmecqe(struct jagmemcqe *);

/* externs from master.d/jag */
extern struct jagunitinfo	 jagunitinfo[][JAG_UPC];
extern struct jagctlrinfo	 jagctlrinfo[];
extern struct jagmemcqe		 jagcqe[][JAG_NCQE];
extern uint			 jagcqenext[];
extern struct jag_cib		 jag_cib[];
extern struct jag_crb		*jag_crb[];
extern uint			 jag_numctlr;
extern uint			 jag_hostid;
extern ushort			 jag_nmap[][JAG_LUPC];

/* SCSI command arrays */
static u_char dk_inquiry[] =		{0x12, 0, 0, 0,    4, 0};

/* used to get integer from non-aligned integers */
#define MKINT(x) (((x)[0] << 24) + ((x)[1] << 16) + ((x)[2] << 8) + (x)[3])

/*
 * Define macros for approved kernel memory alloc function.
 */
#if _MIPS_SIM == _ABI64
#define dma_contig(len) kmem_alloc(len, KM_CACHEALIGN | VM_DIRECT)
#else
#define dma_contig(len) kmem_alloc(len, KM_CACHEALIGN | KM_PHYSCONTIG)
#endif
#define dma_free(ptr) kern_free(ptr)


#if JDEBUG
static void
printdata(prefix, ptr, length)
char *prefix;
char *ptr;
int length;
{
	int	i;

	printf("%s (%d):\n  ", prefix, length);
	if (length > 20) length = 20;
	for (i = 0; i < (length / 4) * 4; i += 4)
		printf(" %x %x %x %x", ptr[i], ptr[i+1], ptr[i+2],
		       ptr[i+3]);
	for (; i < length; i++)
		printf(" %x", ptr[i]);
	printf("\n");
}

static void
printcdb(register unchar *cdb)
{
	printf(" CDB: %x %x %x %x %x %x", cdb[0], cdb[1], cdb[2], cdb[3],
	       cdb[4], cdb[5]);
	if (cdb[0] >= 0x20)
		printf(" %x %x %x %x", cdb[6], cdb[7], cdb[8], cdb[9]);
}
#else
#define printdata(x,y,z)
#define printcdb(x)
#endif


void
jagsensepr(int ctlr, int unit, int key, int asc, int asq)
{
	printf("jag%dd%d: ", ctlr, unit);
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
	printf(" asq 0x%x", asq);
}


static struct jagmemcqe *
getcqe(uint ctlrno)
{
	struct jagmemcqe	*cmd;

	cmd = &jagcqe[ctlrno][jagcqenext[ctlrno]];
	if (++jagcqenext[ctlrno] == JAG_NCQE)
		jagcqenext[ctlrno] = 0;
	return cmd;
}


static volatile struct jag_cqe *
getvmecqe(struct jagmemcqe *cqe_onboard)
{
	return cqe_onboard->cqe_offboard;
}


static int
jagxfersizemax(u_char c, u_char t)
{
	return (jag_nmap[c][t] - 1) * IO_NBPP;
}


static void jagstart(struct jagctlrinfo *ci, struct jagunitinfo *ui);

static void
jagtimostart(struct jagctlrinfo *ci, struct jagunitinfo *ui)
{

	int	s;

	printf("jag%dd%d: attempting to start\n", ci->number, ui->number);
	s = io_splockspl(ci->lock, splvme);
	ui->tostart = 0;
	jagstart(ci, ui);
	io_spunlockspl(ci->lock, s);
}


static struct scsi_request *
getreq(struct jagctlrinfo *ci, struct jagunitinfo *ui)
{
	struct scsi_request *req;
	struct scsi_request *oreq;

	/*
	 * Look through the request queue for an eligible command.
	 */
	oreq = NULL;
	for (req = ui->reqhead; req != NULL; oreq = req, req = req->sr_ha)
	{
		/* If request is for an odd number of bytes, we must fix-up. */
		if ((req->sr_buflen % 2) && (req->sr_flags & SRF_DIR_IN))
		{
			if (ci->pagebusy)
			{
				jdprintf(("jag: fixup page busy\n"));
				if (!ui->tostart)
				{
					timeout(jagtimostart, ci, 2*HZ, ui);
					ui->tostart = 1;
				}
				continue;
			}
			ci->pagebusy = 1;
			req->sr_ha_flags |= JFLAG_ODDBYTEFIX;
		}

		/* If too large for standard map, we allocate a large one. */
		if (req->sr_buflen > jagxfersizemax(ci->number, ui->number))
		{
			req->sr_spare =
			dma_mapalloc(DMA_A32VME, ci->vmebusno,
				io_btoc(io_poff(req->sr_buffer) + req->sr_buflen),
				VM_NOSLEEP);
			if (req->sr_spare == NULL)
			{
				jdprintf(("jag: mapalloc failed\n"));
				if (req->sr_ha_flags & JFLAG_ODDBYTEFIX)
				{
					ci->pagebusy = 0;
					req->sr_ha_flags &= ~JFLAG_ODDBYTEFIX;
				}
				if (!ui->tostart)
				{
					timeout(jagtimostart, ci, 2*HZ, ui);
					ui->tostart = 1;
				}
				continue;
			}
		}
		break;
	}

	if (req != NULL)
	{
		if (oreq == NULL)
			ui->reqhead = req->sr_ha;
		else
			oreq->sr_ha = req->sr_ha;

		if (ui->reqhead == NULL)
			ui->reqtail = NULL;
		else if (ui->reqtail == req)
			ui->reqtail = oreq;
	}
	return req;
}


static void
jagstart(struct jagctlrinfo *ci, struct jagunitinfo *ui)
{
	struct jag_iopb		*iopb;
	struct jag_cqe		*cqe;
	volatile struct jag_cqe	*vmecqe;
	struct jagmemcqe	*cmd;
	struct scsi_request	*req;
	struct jagrequest	*jreq;
	dmamap_t		*dmamap;
	void			*dmaaddr;
	int			 dmalen;

	/*
	 * First, make sure that there is a command to issue, and that we aren't
	 * in the middle of a SCSI bus reset.
	 * If we are in the middle of a bus reset, hold off on commands.
	 */
	if (ui->reqhead == NULL || ci->reset[0] || ci->reset[1] || ui->u_ca)
		return;

	/*
	 * Get a jreq if available, then get a req.
	 * If we get a jreq, and then can't get a req, put the jreq back.
	 */
	if ((jreq = ui->jrfree) == NULL)
		return;
	ui->jrfree = jreq->next;

	if ((req = getreq(ci, ui)) == NULL)
	{
		jreq->next = ui->jrfree;
		ui->jrfree = jreq;
		return;
	}
	jreq->active = 1;
	jreq->sreq = req;

	/*
	 * Get a Command Queue Entry and IOPB.
	 * Fill out command code, options, SCSI unit, and SCSI command.
	 */
	cmd = getcqe(ci->number);
	iopb = &cmd->iopb;
	iopb->cmd = 0x20;
	if (req->sr_flags & SRF_DIR_IN)
		iopb->options = 0x001 | jagqueue;
	else
		iopb->options = 0x101 | jagqueue;
	iopb->unit = jag_scsi_id[ui->number];
	bcopy(req->sr_command, iopb->scsi, req->sr_cmdlen);

	/*
	 * Take care of mapping, if needed, and set data address and length.
	 */
	if (req->sr_buflen != 0)
	{
		if (req->sr_spare)
			dmamap = req->sr_spare;
		else
			dmamap = jreq->dmamap;

		dmaaddr = req->sr_buffer;
		if (req->sr_flags & SRF_MAPBP)
			dmalen = dma_mapbp(dmamap, req->sr_bp, req->sr_buflen);
		else if (req->sr_ha_flags & JFLAG_ODDBYTEFIX)
		{
			/*
			 * If the odd transfer doesn't cross a page
			 * boundary, transfer only to the page buffer.
			 */
			if (io_poff(dmaaddr) + req->sr_buflen <= IO_NBPP)
			{
				dmalen = dma_map(dmamap, (caddr_t)ci->page,
						 req->sr_buflen);
				dmaaddr = ci->page;
				jdprintf(("ODDBYTE: on one page\n"));
			}
			else
			{
				dmalen = dma_map2(dmamap, dmaaddr,
					     (caddr_t)ci->page, req->sr_buflen);
				/*
				 * Assume that JFLAG_ODDBYTEFIX is only set
				 * if we are DMAing into memory.
				 * Flush a whole page for simplicity.
				 */
				if (cachewrback && (req->sr_flags & SRF_FLUSH))
					dki_dcache_inval(ci->page, IO_NBPP);
				jdprintf(("ODDBYTE: crosses page\n"));
			}
		}
		else
			dmalen = dma_map(dmamap, dmaaddr, req->sr_buflen);

		ASSERT( ((dmalen & 1) == 0) || \
		        (req->sr_ha_flags & JFLAG_ODDBYTEFIX) || \
		       !(req->sr_flags & SRF_DIR_IN));
		ASSERT(dmalen == req->sr_buflen);
		iopb->addr = dma_mapaddr(dmamap, dmaaddr);
		iopb->length = (dmalen + 1) & ~1;
	}
	else
	{
		iopb->addr = NULL;
		iopb->length = 0;
	}

	cqe = &cmd->cqe;
	cqe->qecreg = ui->abortqueue ? 0x13 : 0x11;
#if _MIPS_SIM == _ABI64
	ASSERT(IS_KSEG0(jreq));
	ASSERT(((ulong) jreq & 3) == 0);
	cqe->tag.on = (uint) (K0_TO_PHYS(jreq) >> 2);
#else
	cqe->tag.on = (uint) jreq;
#endif
	cqe->wq_num = ui->number + 1;
	vmecqe = getvmecqe(cmd);

	/* invalidate/writeback cache for data and cqe, if necessary. */
	if (req->sr_flags & SRF_FLUSH)
	{
		if (!(req->sr_flags & SRF_DIR_IN))
			dcache_wb(dmaaddr, dmalen);
		else if (cachewrback)
			dki_dcache_inval(dmaaddr, dmalen);
	}

	/*
	 * Set iopb length on board and in memory.  Writeback cache so
	 * board will see correct data, and then start things off!
	 */
	cqe->iopblen = vmecqe->iopblen = (req->sr_cmdlen + 0x20) >> 1;
	dcache_wb(cmd, sizeof(*cmd));
	vmecqe->qecreg = 0x11;	/* start command */
	if (ui->abortqueue)
	{
		ui->abortqueue = 0;
		jdprintf(("jag%dd%d: abort acknowledge\n",
			  ci->number, ui->devnum));
	}

	jreq->timeout_id = timeout(jagtimeout, jreq, req->sr_timeout);
}


static void
jaghinv(u_char *inv, int adap, int targ, int lun)
{
	u_char	*extra;
	char	*which;
	int	 type, disktype;

	switch (inv[0] & 0x1F)
	{
	case 0: /* disk */

		/* Check to see if this hard disk is really a CD-ROM */
		if (cdrom_inquiry_test(&inv[8])) {
			inv[0] = INV_CDROM; inv[1] = INV_REMOVE;
			goto cdrom;
		}

		extra = "disk";
		type = 0;
		which = "";
		if (inv[1] & 0x80) {		/* removeable media */
			which = "removable ";
			disktype = INV_SCSIFLOPPY;
			if (strncmp((char *) &inv[8], "TEAC", 4) == 0) 
				type = INV_TEAC_FLOPPY;
		} else {
			disktype = INV_VSCSIDRIVE;
		}
		add_to_inventory(INV_DISK, disktype, adap, targ, type);
		break;

	case 1: /* sequential == tape */
		type = tpsctapetype((ct_g0inq_data_t *)inv, 0);
		which = "tape";
		extra = inv;
		sprintf((char *)inv, " type %d", type);
		add_to_inventory(INV_TAPE, INV_VSCSITAPE, adap, targ, type);
		break;

	default:
cdrom:
		type = inv[1] & INV_REMOVE;
		/* ANSI level: SCSI 1, 2, etc.*/
		type |= inv[2] & INV_SCSI_MASK;
		add_to_inventory(INV_VSCSI, inv[0] & 0x1f, adap, targ,
				 type | (lun<<8));

		/*
		 * handle CDROM specially
		 */
		if((inv[0]&0x1f) == INV_CDROM)
		    which = "CDROM";
		else
		    sprintf(which=(char *)inv, " device type %u", inv[0]&0x1f);
		extra = "";
		break;
	}

	if(showconfig)
	{
		cmn_err(CE_CONT,"SCSI %s%s (%d,%d", which,
			extra ? extra : (u_char *) "", adap, targ);
		if (lun)
			cmn_err(CE_CONT, ",%d", lun);
		cmn_err(CE_CONT, ")\n");
	}
}


void
jagcommand(struct scsi_request *req)
{
	struct jagctlrinfo	*ci;
	struct jagunitinfo	*ui;
	u_char			 ctlr;
	u_char			 targ;
	int			 s;

	req->sr_resid = req->sr_buflen;
	req->sr_status = 0;
	req->sr_scsi_status = req->sr_ha_flags = req->sr_sensegotten = 0;
	req->sr_spare = NULL;
	req->sr_ha = NULL;

	/*
	 * Odd controller numbers are actually the second channel of a
	 * single Jaguar VME controller.
	 */
	ctlr = (req->sr_ctlr - SCSI_JAGSTART) / 2;
	targ = req->sr_target + (req->sr_ctlr & 1) * 8;

	/*
	 * Check for validity of arguments.
	 */
	if (ctlr >= jag_numctlr || req->sr_target > 7 ||
	    req->sr_lun != 0 || jag_unitnum[targ] == -1 ||
	    req->sr_notify == NULL)
	{
		req->sr_status = SC_REQUEST;
		goto err_jagcommand;
	}

	/* Buffers must aligned on a four-byte boundary */
	if (req->sr_buflen != 0 && ((ulong) req->sr_buffer % 4))
	{
		printf("jag%dd%d: starting address must be four-byte aligned\n",
		       ctlr, targ);
		printcdb(req->sr_command);
		req->sr_status = SC_ALIGN;
		goto err_jagcommand;
	}

	ci = &jagctlrinfo[ctlr];
	ui = ci->unit[jag_unitnum[targ]];

	/*
	 * Check to see if a device has been seen by jaginfo previously.
	 */
	if (ui->tinfo.si_inq == NULL)
	{
		req->sr_status = SC_HARDERR;
		goto err_jagcommand;
	}

	s = splvme();

	/*
	 * If ci->intr is set, we queue the request in the controller's
	 * auxiliary queue since we are executing in an interrupt
	 * routine and can't grab the controller spinlock.
	 */
	spsema(ci->auxlock);
	if (ci->intr)
	{
		/* jdprintf(("jag: queueing during interrupt\n")); */
		if (ci->auxhead == NULL)
			ci->auxhead = req;
		else
			ci->auxtail->sr_ha = req;
		ci->auxtail = req;
		svsema(ci->auxlock);
		goto end_jagcommand;
	}
	svsema(ci->auxlock);

	/*
	 * Queue the command and attempt to issue it.
	 */
	spsema(ci->lock);
	if (ui->aenreq && !(ui->u_ca || ci->reset[ui->devnum / 8]))
	{
		if (req->sr_flags & SRF_AEN_ACK)
			ui->aenreq = 0;
		else
		{
			req->sr_status = SC_ATTN;
			svsema(ci->lock);
			splx(s);
			goto err_jagcommand;
		}
	}
	if (ui->reqhead == NULL)
		ui->reqhead = req;
	else
		ui->reqtail->sr_ha = req;
	ui->reqtail = req;
	jagstart(ci, ui);
	svsema(ci->lock);

end_jagcommand:
	splx(s);
	return;

err_jagcommand:
	if (req->sr_notify != NULL)
		(*req->sr_notify)(req);
}


static void
jagdone(struct scsi_request *req)
{
	vsema(req->sr_dev);
}

struct scsi_target_info *
jaginfo(u_char ctlr, u_char targ, u_char lun)
{
	struct jagctlrinfo	*ci;
	struct jagunitinfo	*ui;
	struct scsi_request	*req;
	static u_char		 scsi[6];
	int			 i;
	u_char			 jagctlr;	/* driver ctlr # */
	u_char			 jagtarg;	/* driver targ # */
	char			 registered = 0;
	sema_t			*sema;

	/*
	 * Odd controller numbers are actually the second channel of a
	 * single Jaguar VME controller.
	 */
	jagctlr = (ctlr - SCSI_JAGSTART) / 2;
	jagtarg = targ + (ctlr & 1) * 8;

	/*
	 * Check for validity of arguments.
	 */
	if (jagctlr >= jag_numctlr || targ > 7 ||
	    lun != 0 || jag_unitnum[jagtarg] == -1)
	{
		return NULL;
	}

	ci = &jagctlrinfo[jagctlr];
	if (!ci->present)
		return NULL;
	ui = ci->unit[jag_unitnum[jagtarg]];

	/*
	 * If we haven't done so already, allocate DMA maps, sense and
	 * inquiry buffers for this device.
	 */
	if (ui->tinfo.si_inq == NULL)
	{
		for (i = 0; i < JAG_QDEPTH; i++)
			ui->jreq[i].dmamap = dma_mapalloc(DMA_A32VME,
				ci->vmebusno, jag_nmap[jagctlr][jagtarg], 0);
		ui->tinfo.si_inq = dma_contig(SCSI_INQUIRY_LEN);
		ui->tinfo.si_sense = dma_contig(SCSI_SENSE_LEN);
	}
	else
		registered = 1;

	/*
	 * Issue inquiry to find out if this address has a device.
	 * If we get an error, free the DMA map, sense and inquiry buffers.
	 */
	sema = kern_calloc(1, sizeof(*sema));
	init_sema(sema, 0, "jagga", ctlr<<4 | targ);
	req = kern_calloc(1, sizeof(*req));

	bcopy(dk_inquiry, scsi, sizeof(dk_inquiry));
	scsi[4] = SCSI_INQUIRY_LEN;

	req->sr_ctlr = ctlr;
	req->sr_target = targ;
	req->sr_lun = lun;
	req->sr_command = scsi;
	req->sr_cmdlen = sizeof(dk_inquiry);
	req->sr_flags = SRF_DIR_IN | SRF_FLUSH | SRF_AEN_ACK;
	req->sr_timeout = 10 * HZ;
	req->sr_buffer = ui->tinfo.si_inq;
	req->sr_buflen = SCSI_INQUIRY_LEN;
	req->sr_sense = NULL;
	req->sr_notify = jagdone;
	req->sr_dev = sema;

	jagcommand(req);
	psema(sema, PRIBIO);

	/* if any status values non-zero, command failed */
	i = req->sr_status | req->sr_scsi_status | req->sr_sensegotten;
	kern_free(req);
	freesema(sema);
	kern_free(sema);
	if (i)
	{
		for (i = 0; i < JAG_QDEPTH; i++)
			dma_mapfree(ui->jreq[i].dmamap);
		dma_free(ui->tinfo.si_inq);
		dma_free(ui->tinfo.si_sense);
		ui->tinfo.si_inq = NULL;
		return NULL;
	}

	if (!registered)
		jaghinv(ui->tinfo.si_inq, jagctlr, jagtarg, lun);

	ui->tinfo.si_maxq = JAG_QDEPTH;
	ui->tinfo.si_ha_status = 0;	/* can't report any status easily */
	return &ui->tinfo;
}


int
jagalloc(u_char ctlr, u_char targ, u_char lun, int opt, void (*cb)())
{
	struct jagctlrinfo		*ci;
	struct jagunitinfo		*ui;
	uint				 jagctlr;
	uint				 jagtarg;

	if (jaginfo(ctlr, targ, lun) == NULL)
		return ENODEV;

	jagctlr = (ctlr - SCSI_JAGSTART) / 2;
	jagtarg = targ + (ctlr & 1) * 8;
	ci = &jagctlrinfo[jagctlr];
	ASSERT(ci->present);
	ui = ci->unit[jag_unitnum[jagtarg]];

	if (ui->exclusive)
		return EBUSY;

	if (opt & SCSIALLOC_EXCLUSIVE)
	{
		if (ui->refcount != 0)
			return EBUSY;
		else
			ui->exclusive = 1;
	}
		
	if (cb != NULL)
	{
		if (ui->sense_callback != NULL)
			return EINVAL;	/* EBUSY better, but want unique error... */
		else
			ui->sense_callback = cb;
	}

	ui->refcount++;
	return SCSIALLOCOK;
}


void
jagfree(u_char ctlr, u_char targ, u_char lun, void (*cb)())
{
	struct jagctlrinfo		*ci;
	struct jagunitinfo		*ui;
	uint				 jagctlr;
	uint				 jagtarg;

	/*
	 * Check for validity of arguments.
	 */
	jagctlr = (ctlr - SCSI_JAGSTART) / 2;
	jagtarg = targ + (ctlr & 1) * 8;
	if (jagctlr >= jag_numctlr || targ > 7 ||
	    lun != 0 || jag_unitnum[jagtarg] == -1)
		return;
	ci = &jagctlrinfo[jagctlr];
	ASSERT(ci->present);
	ui = ci->unit[jag_unitnum[jagtarg]];

	if (ui->exclusive)
		ui->exclusive = 0;

	if (cb != NULL)
	{
		if (ui->sense_callback == cb)
			ui->sense_callback = NULL;
		else
			printf("jag%dd%d: callback 0x%x not active\n",
			       jagctlr, jagtarg, cb);
	}

	if (ui->refcount != 0)
		ui->refcount--;
#ifdef DEBUG
	else
		printf("jag%dd%d: jagfree called when refcount 0\n",
		       jagctlr, jagtarg);
#endif
}


/*
 * Yank the master reset pin and hold it for 100 microsecs.
 * Then sleep for 1.6 sec to give the SCSI disks time to reset.
 */
static void
jagreset(volatile struct jag_mcsb *mcsb)
{
	mcsb->mcr = 0x1000;
	DELAY(100);
	mcsb->mcr = 0;
	DELAY(1600000);
}


/*
 * Probe the controller and find out what SCSI devices are attached.
 * Set up work queues.
 */
void
jagedtinit(struct edt *e)
{
	struct jagctlrinfo		*ci;
	struct jagunitinfo		*ui;
	volatile struct jag_mcsb	*mcsb;
	volatile struct jag_cib		*cib;
	union {
		struct jag_iopb normal;
		struct jag_wqiopb wq;
	}				 jagiopb;
	volatile struct jag_iopb	*iopb;
	struct jag_wqiopb		*wqiopb;
	volatile struct jag_crb		*crb;
	volatile struct jag_cqe		*cqe;
	volatile short			*ptr;
	int				 ctlrno;
	unchar				 ipl;
	ulong				 vec;
	vme_intrs_t 			*intrs = e->e_bus_info;
	piomap_t			*piomap;
	int				 i, j;
	int				 cougar_d64 = 0;
#if EVEREST
	dmamap_t			*dp, *cqemap;
	paddr_t				 dma_addr;
	__psunsigned_t			 pgstart, pgend, numpgs;
#endif /* EVEREST */
	extern int			 jag_01J_ignore_xfercount_exceptions;

	jag_is_initted = 1;	/* disable printing of selection timeouts */

	/* map I/O address space */

	piomap = pio_mapalloc(e->e_bus_type, e->e_adap, &e->e_space[0],
		PIOMAP_FIXED,"jag");
	if (piomap == 0)
		goto out;

	e->e_base = pio_mapaddr(piomap, e->e_iobase);

	mcsb = (struct jag_mcsb *) e->e_base;
	ctlrno = intrs->v_unit = e->e_ctlr;
	if (badaddr(&mcsb->msr, sizeof(mcsb->msr)))
	{
		cmn_err(CE_CONT, "jag%d: controller not installed\n", ctlrno);
		pio_mapfree(piomap);
		goto out;
	}
	vec = intrs->v_vec;
	ipl = intrs->v_brl;
	if (vec == 0)
		vec = intrs->v_vec = vme_ivec_alloc(e->e_adap);
	if (vec == 255) {
		cmn_err(CE_WARN,"jagedtinit: no interrupt vector\n");
		pio_mapfree(piomap);
		goto out;
	}
	vme_ivec_set(e->e_adap, vec, (int(*)(int))jagintr, intrs->v_unit);

	/*
	 * initialize the scsi id and unit number translation tables
	 */
	for (i = 0; i < jag_hostid; i++)
	{
		jag_scsi_id[i] = i;
		jag_scsi_id[i+7] = 0x40 + i;
		jag_unitnum[i] = i;
		jag_unitnum[i+8] = i + 7;
		jag_devnum[i] = i;
		jag_devnum[i+7] = i + 8;
	}
	jag_unitnum[i] = jag_unitnum[i+8] = -1;
	for (; i < 7; i++)
	{
		jag_scsi_id[i] = i + 1;
		jag_scsi_id[i+7] = 0x40 + i + 1;
		jag_unitnum[i+1] = i;
		jag_unitnum[i+9] = i + 7;
		jag_devnum[i] = i + 1;
		jag_devnum[i+7] = i + 9;
	}

	/*
	 * setup pointers into short I/O space
	 */
	ci = &jagctlrinfo[ctlrno];
	ci->number = ctlrno;
	ci->vmebusno = e->e_adap;
	ci->mcsb = mcsb;
	ci->mce = (struct jag_cqe *) ((ulong) mcsb + sizeof(struct jag_mcsb));
	ci->cmdq = &ci->mce[1];
	ci->hus = (short *) &ci->cmdq[JAG_NCQE];
	ci->csb = (struct jag_csb *) ((ulong) mcsb + 0x788);
	crb = (struct jag_crb *) ((ulong) ci->csb - sizeof(struct jag_crb));
	ci->crb = crb;

	/*
	 * initialize SW structures
	 */
	init_spinlock(&ci->lock, "jag", ctlrno);
	init_spinlock(&ci->auxlock, "jagga", ctlrno);
	ci->page = kvpalloc(1, VM_DIRECT, 0);
	for (i = 0; i < JAG_UPC; i++)
	{
		ci->unit[i] = ui = &jagunitinfo[ctlrno][i];
		ui->number = i;
		ui->devnum = jag_devnum[i];
		ui->reqhead = ui->reqtail = NULL;
		ui->jrfree = NULL;
		for (j = 0; j < JAG_QDEPTH; j++)
		{
			ui->jreq[j].ctlr = ctlrno;
			ui->jreq[j].unit = i;
			ui->jreq[j].next = ui->jrfree;
			ui->jrfree = &ui->jreq[j];
		}
	}

	/*
	 * Reset controller and check controller health.
	 */
	jagreset(mcsb);
	if ((mcsb->msr & 3) != 2)
	{
		cmn_err(CE_CONT, "jag%d: failed powerup diags\n", ctlrno);
		pio_mapfree(piomap);
		goto out;
	}

	/*
	 * Initialize controller.  Set up controller initialization block in
	 * main memory, then copy it down to the controller and execute the
	 * Initialize Controller command.
	 */
	cib = (struct jag_cib *) ci->hus;
	cib->ncqe = JAG_NCQE;
	cib->dmaburst = JAG_DMABURST;
	cib->normal_vec = vec;
	cib->error_vec = vec;
	cib->scsi0_id = (ushort) jag_hostid;
	cib->scsi1_id = (ushort) jag_hostid;
	cib->crb_offset = (ushort) (0x788 - sizeof(struct jag_crb));
	cib->sel_timeout_h = JAG_SELTIMEOUT >> 16;
	cib->sel_timeout_l = JAG_SELTIMEOUT & 0xFFFF;
	cib->wq0_timeout_h = JAG_CMDTIMEOUT >> 16;
	cib->wq0_timeout_l = JAG_CMDTIMEOUT & 0xFFFF;
	cib->vme_timeout_h = JAG_VMETIMEOUT >> 16;
	cib->vme_timeout_l = JAG_VMETIMEOUT & 0xFFFF;
	cib->reserve1 = cib->reserve2 = 0;
	cib->crb_memtype = 0x60B;	/* 32 bit wide, block mode */
	jag_crb[ctlrno] = dma_contig(sizeof(struct jag_crb));
#if EVEREST
	if( (dp = dma_mapalloc(DMA_A32VME, ci->vmebusno, 2, VM_NOSLEEP)) 
		== NULL ) {
		pio_mapfree(piomap);
		goto out;
	}
	(void)dma_map(dp,(caddr_t)jag_crb[ctlrno],sizeof(struct jag_crb));
	dma_addr = dma_mapaddr(dp,(caddr_t)jag_crb[ctlrno]);
	cib->crb_addr_h = dma_addr >> 16;
	cib->crb_addr_l = dma_addr & 0xFFFF;
#else
	cib->crb_addr_h = kvtophys(jag_crb[ctlrno]) >> 16;
	cib->crb_addr_l = kvtophys(jag_crb[ctlrno]) & 0xFFFF;
#endif /* EVEREST */
	cib->err_recovery = 1;
	cib->reserve3 = 0;

	ui = &jagunitinfo[ctlrno][0];
	iopb = &jagiopb.normal;
	iopb->reserve1 = iopb->reserve2 = 0;
	iopb->reserve3 = iopb->reserve4 = 0;
	iopb->intvector = vec;
	iopb->errvector = vec;
	iopb->intlevel = ipl;
	iopb->cmd = 0x41;
	iopb->options = 0;
	iopb->iopb_memtype = 0x300;
	iopb->addr = (ulong) ci->hus - (ulong) ci->mcsb;
	iopb->length = sizeof(struct jag_cib);
	ptr = (short *) ((ulong) ci->hus + sizeof(struct jag_cib));
	for (i = 0; i < 14; i++)
		ptr[i] = ((short *) iopb)[i];

	cqe = ci->mce;
	cqe->iopbaddr = (ushort) ((ulong) ptr - (ulong) mcsb);
	cqe->iopblen = 0;
	cqe->wq_num = 0;
	cqe->qecreg = 1;

	while (!(crb->crsw & 1));
	if (crb->iopb.status != 0)
	{
		cmn_err(CE_CONT,
			"jag%d: error in initialization, code 0x%x\n",
			ctlrno, crb->iopb.status);
		pio_mapfree(piomap);
		goto out;
	}
	crb->crsw = 0;

	/*
	 * set up work queues and command queue entries
	 */
	wqiopb = &jagiopb.wq;
	bzero(wqiopb,sizeof(wqiopb));
	for (i = 1; i <= JAG_UPC; i++)
	{
		ui = &jagunitinfo[ctlrno][i-1];
		wqiopb->cmd = 0x42;
		wqiopb->options = 0;
		wqiopb->wqnum = (ushort) i;
		wqiopb->wopt = 0x8009;  /* parity, force initialization */
		wqiopb->nslots = JAG_QDEPTH;
		wqiopb->timeout_h = JAG_CMDTIMEOUT >> 16;
		wqiopb->timeout_l = JAG_CMDTIMEOUT && 0xFFFF;
		ptr = ci->hus;
		for (j = 0; j < 0x14; j++)
			ptr[j] = ((short *) wqiopb)[j];

		cqe->iopbaddr = (ushort) ((ulong) ptr - (ulong) mcsb);
		cqe->qecreg = 1;

		while (!(crb->crsw & 1));
		if (crb->iopb.status != 0)
		{
			cmn_err(CE_CONT,
				"jag%d: error in wq%d init, code 0x%x\n",
				ctlrno, i, crb->iopb.status);
			pio_mapfree(piomap);
			goto out;
		}
		crb->crsw = 0;
	}

#if EVEREST
	pgstart = (__psunsigned_t) jagcqe[ctlrno] / IO_NBPP;
	pgend = ((__psunsigned_t) jagcqe[ctlrno+1] - 1) / IO_NBPP;
	numpgs = pgend - pgstart + 1;
	cqemap = dma_mapalloc(DMA_A32VME, e->e_adap, numpgs, 0);
	(void) dma_map(cqemap, (caddr_t) (pgstart * IO_NBPP), numpgs * IO_NBPP);
#if NOTDEF
	cougar_d64 = (ci->csb->cougarid == 0x4220);
#endif /* NOTDEF */
#endif /* EVEREST */
	for (i = 0; i < JAG_NCQE; i++)
	{
		struct jagmemcqe *cmd;
		ulong addr;
		volatile struct jag_cqe *cqeoff;

		cqeoff = &cqe[i+1];
		cmd = &jagcqe[ci->number][i];
		cmd->cqe_offboard = cqeoff;

		iopb = &cmd->iopb;
		iopb->reserve1 = iopb->reserve2 = 0;
		iopb->reserve3 = iopb->reserve4 = 0;
		iopb->intvector = vec;
		iopb->errvector = vec;
		iopb->intlevel = ipl;
		iopb->iopb_memtype = (cougar_d64) ? 0xE0B : 0x60B;
		cqeoff->iopbaddr = 0x60B;
#if EVEREST
		addr = dma_mapaddr(cqemap,(caddr_t)&cmd->cqe);
#if _MIPS_SIM == _ABI64
		ASSERT(!(addr & 0xFFFFFFFF00000000));
#endif
#else
		addr = kvtophys(&cmd->cqe);
#endif /* EVEREST */
		cqeoff->tag.off.tagh = (ushort) (addr >> 16);
		cqeoff->tag.off.tagl = (ushort) (addr & 0xFFFF);
		cqeoff->iopblen = 0; /* default length */
	}

	/*
	 * start queued mode; wait for ack
	 */
	mcsb->mcr = 1;
	while (!(crb->crsw & 0x20));
	crb->crsw = 0;
	if (showconfig)
		printf("Jaguar VME-SCSI controller %d, Firmware rev %c%c%c\n",
		       ctlrno, ci->csb->fwrev[0], ci->csb->fwrev[1],
		       ci->csb->fwrev[2]);
	ci->present = 1;
	if (!strncmp((char *) ci->csb->fwrev, "01J", 3))
		if (jag_01J_ignore_xfercount_exceptions)
			ci->ignorexcept = 1;

	scsi_command[SCSIDRIVER_JAG] = jagcommand;
	scsi_alloc[SCSIDRIVER_JAG] = jagalloc;
	scsi_free[SCSIDRIVER_JAG] = jagfree;
	scsi_info[SCSIDRIVER_JAG] = jaginfo;
	scsi_driver_table[SCSI_JAGSTART + ctlrno * 2] = SCSIDRIVER_JAG;
	scsi_driver_table[SCSI_JAGSTART + ctlrno * 2 + 1] = SCSIDRIVER_JAG;

	/*
	 * We are about to use the in memory, CRB; cache invalidate it
	 *   in case it is cache dirty.
	 * Probe for devices, and add controller to inventory.
	 */
	dki_dcache_inval(jag_crb[ctlrno], sizeof(struct jag_crb));
	for (i = 0; i < JAG_UPC; i++)
		jaginfo(ci->number*2 + SCSI_JAGSTART + i/7,
			jag_devnum[i] % 8, 0);
	add_to_inventory(INV_DISK, INV_JAGUAR, intrs->v_unit, 0,
	                 (ci->csb->fwrev[0] << 16) | (ci->csb->fwrev[1] << 8) |
			 (ci->csb->fwrev[2]));
out:
	jag_is_initted = 0;
}


static void
jagtimeout(struct jagrequest *jreq)
{
	register struct jagctlrinfo	*ci;
	uint				 ctlr;
	uint				 unit;
	uint				 bus;
	int				 pri;
	volatile struct jag_cqe		*mce;
	volatile struct jag_iopb	*iopb;

	ctlr = jreq->ctlr;
	unit = jreq->unit;
	if (unit >= 7)
		bus = 1;
	else
		bus = 0;
	ci = &jagctlrinfo[ctlr];
	pri = splvme();
	spsema(ci->lock);
	jreq->timeout_id = 0;
	jreq->sreq->sr_ha_flags |= JFLAG_TIMEOUT;

	if (ci->reset[bus])
	{
		printf("jag%dd%d: timeout -- bus reset already in progress\n",
		       ctlr, jag_devnum[unit]);
		goto end_jagtimeout;
	}

	printf("jag%dd%d: timeout -- resetting SCSI bus %d on controller %d\n",
	       ctlr, jag_devnum[unit], bus, ctlr);

	mce = ci->mce;
	iopb = (struct jag_iopb *) ci->hus;
	iopb->cmd = 0x22;
	iopb->options = 1;
	iopb->intvector = iopb->errvector = jagcqe[ctlr][unit].iopb.errvector;
	iopb->intlevel = jagcqe[ctlr][unit].iopb.intlevel;
	iopb->reserve4 = (unit >= 7 ? 0x8000 : 0);
	mce->iopbaddr = (ushort) ((ulong) iopb - (ulong) ci->mcsb);
	mce->iopblen = 0;
	mce->wq_num = 0;
	mce->qecreg = 1;

	ci->reset[bus] = 1;

end_jagtimeout:
	svsema(ci->lock);
	splx(pri);
}


/*
 * Print appropriate error message and do a bit of error handling.
 */
static void
jagerror(struct jagctlrinfo *ci, struct jagunitinfo *ui,
	 struct jag_crb  *crb, struct jag_iopb *iopb)
{
	struct jagrequest	*jreq;
	struct scsi_request	*req;
	ushort		 	 stat;

	ASSERT(!(crb->crsw & 0x80));

#if _MIPS_SIM == _ABI64
	jreq = (struct jagrequest *) (PHYS_TO_K0((ulong) crb->tag << 2));
#else
	jreq = (struct jagrequest *) crb->tag;
#endif
	req = jreq->sreq;
	stat = iopb->status;

	if (req->sr_ha_flags & JFLAG_TIMEOUT)
		req->sr_status = SC_CMDTIME;
	else if (crb->crsw & 0x10)
		req->sr_status = SC_ATTN;
	else if ((stat & 0xFF) == 0x15 && (ci->reset[ui->devnum / 8]))
	{
		if (req->sr_status == 0)
			req->sr_status = SC_ATTN;
	}
	else if ((stat & 0xFF) == 0x30)
		req->sr_status = SC_TIMEOUT;
	else if (!(stat & 0xFF) && (stat & 0xFF00))
		req->sr_scsi_status = stat >> 8;
	else
		req->sr_status = SC_HARDERR;

	/*
	 * During controller initialization, don't print selection timeouts.
	 * Don't print anything for busy status; device driver should handle.
	 */
#if !JDEBUG
	if (jag_is_initted && stat == 0x30)
		return;
	if (stat == ST_BUSY << 8)
		return;
#endif

	printf("jag%dd%d: ", ci->number, ui->devnum);
	printf("crsw 0x%x, status 0x%x", crb->crsw, stat);
	jdprintf((" driver stat 0x%x ", req->sr_status));
	printcdb(iopb->scsi);
	if (ui->sense)
	{
		ui->sense = 0;
		req->sr_sensegotten = -1;
		printf(" REQUEST SENSE failed", ci->number, ui->devnum);
	}
	printf("\n");
}


/*
 * Issue a request sense to the controller from interrupt.
 */
static void
jagreqsense(struct jagctlrinfo *ci, struct jagunitinfo *ui,
	    struct jag_crb *crb, struct jagrequest *jreq)
{
	struct jag_iopb		*iopb;
	struct jag_cqe		*cqe;
	volatile struct jag_cqe	*vmecqe;
	struct jagmemcqe	*cmd;

	cmd = getcqe(ci->number);
	cqe = &cmd->cqe;

	iopb = &cmd->iopb;
	iopb->cmd = 0x20;
	iopb->options = 1;
	iopb->unit = jag_scsi_id[ui->number];

#ifdef DEBUG
	printf("jag%dd%d: check condition", ci->number, ui->devnum);
	printcdb(crb->iopb.scsi);
	printf("\n");
#endif

	bzero(iopb->scsi, 6);
	iopb->scsi[0] = 3;	/* request sense */
	iopb->scsi[4] = SCSI_SENSE_LEN;
#if EVEREST
	(void)dma_map(jreq->dmamap, (caddr_t)ui->tinfo.si_sense, SCSI_SENSE_LEN);
	iopb->addr = dma_mapaddr(jreq->dmamap, (caddr_t)ui->tinfo.si_sense);
#else
	iopb->addr = kvtophys(ui->tinfo.si_sense);
#endif /* EVEREST */
	iopb->length = SCSI_SENSE_LEN;

	cqe->qecreg = ui->abortqueue ? 0x13 : 0x11;
	cqe->tag.on = crb->tag;
	cqe->wq_num = ui->number + 1;

	vmecqe = getvmecqe(cmd);
	ASSERT(!(vmecqe->qecreg & 1));

	if (cachewrback)
		dki_dcache_inval(ui->tinfo.si_sense, SCSI_SENSE_LEN);

	/*
	 * Set iopb length on board and in memory.  Writeback cache so
	 * board will see correct data, and then start things off!
	 */
	cqe->iopblen = vmecqe->iopblen = 0;
	dcache_wb(cmd, sizeof(*cmd));
	vmecqe->qecreg = 0x11;  /* start command */
	if (ui->abortqueue)
	{
		ui->abortqueue = 0;
		jdprintf(("jag%dd%d: abort acknowledge\n",
			  ci->number, ui->devnum));
	}

	if (jreq->timeout_id)
		untimeout(jreq->timeout_id);
	jreq->timeout_id = timeout(jagtimeout, jreq, 30*HZ);
	ui->sense = 1;
}


/*
 * Complete processing on a command.
 * Returns a pointer to a buf if an iodone is needed.
 */
void
jagcomplete(struct jagctlrinfo *ci, struct jagunitinfo *ui,
	    struct jagrequest *jreq, struct scsi_request *req)
{
	if (ui->sense) /* completion of req sense */
	{
		ui->sense = 0;
		dki_dcache_inval(ui->tinfo.si_sense, SCSI_SENSE_LEN);
		if (req->sr_sense == NULL)
			jagsensepr(ci->number, ui->devnum,
				   ui->tinfo.si_sense[2],
				   ui->tinfo.si_sense[10],
				   ui->tinfo.si_sense[11]);
		else
		{
#ifdef DEBUG
			jagsensepr(ci->number, ui->devnum,
				   ui->tinfo.si_sense[2],
				   ui->tinfo.si_sense[12],
				   ui->tinfo.si_sense[13]);
			printf("\n");
#endif
			bcopy(ui->tinfo.si_sense, req->sr_sense,
			      req->sr_sensegotten);
		}
		if (ui->sense_callback != NULL)
			(*ui->sense_callback)(req->sr_ctlr, req->sr_target, 0,
					      (char *)ui->tinfo.si_sense);
	}

	if (req->sr_spare)
		dma_mapfree(req->sr_spare);
	if (jreq->timeout_id)
	{
		untimeout(jreq->timeout_id);
		jreq->timeout_id = 0;
	}
	jreq->active = 0;
	jreq->next = ui->jrfree;
	ui->jrfree = jreq;
}


/*
 * This routine is used when there is a controller error.
 */
void
jagscerror(struct jagctlrinfo *ci, u_short *crb)
{
	printf("jag%d: controller error; code 0x%x\n", ci->number, crb[6]);
	/* XXX we should reset the SCSI bus here XXX */
}


/*
 * This routine is used when there is no CRB for an IOPB.  This will be
 * the case if a SCSI bus reset is issued, and not all commands are
 * rejected with error.
 */
/*ARGSUSED*/
void
jagclear(struct jagctlrinfo *ci, struct jagunitinfo *ui, char *data)
{
	struct scsi_request	*req;
	struct scsi_request	*nrq;
	struct jagrequest	*jreq;
	int			 j;

	/* Look for outstanding IOPBs */
	for (j = 0; j < JAG_QDEPTH; j++)
	{
		jreq = &ui->jreq[j];
		if (jreq->active)
		{
			req = jreq->sreq;
			req->sr_scsi_status = 0;
			if (req->sr_status != 0)
			{
				jdprintf(("jag%dd%d: request 0x%x 0x%x aborted - %s\n",
				          ci->number, ui->devnum, jreq, req, data));
				if (req->sr_spare)
					dma_mapfree(req->sr_spare);
				if (jreq->timeout_id)
				{
					untimeout(jreq->timeout_id);
					jreq->timeout_id = 0;
				}
				jreq->active = 0;
				if (req->sr_ha_flags & JFLAG_ODDBYTEFIX)
					ci->pagebusy = 0;

				(*req->sr_notify)(req);

				jreq->next = ui->jrfree;
				ui->jrfree = jreq;
			}
#if JDEBUG
			else
			{
				printf("jag%dd%d: no word on request 0x%x 0x%x\n",
				       ci->number, ui->devnum, jreq, req);
				jagtrace = 10;
			}
#endif
		}
	}

	/* Clear the wait queue */
	req = ui->reqhead;
	while (req != NULL)
	{
		req->sr_status = SC_ATTN;
		jdprintf(("jag%dd%d: queued request cancelled by %s\n",
		          ci->number, ui->devnum, data));
		nrq = req->sr_ha;
		(*req->sr_notify)(req);
		req = nrq;
	}
	ui->reqhead = ui->reqtail = NULL;
}


void
jagintr(int ctlr)
{
	register struct jagctlrinfo	*ci;
	register struct jagunitinfo	*ui = NULL;
	register struct jag_crb		*crb;		/* crb in host mem */
	register struct jag_crb		*crbo;		/* crb on Jaguar */
	register struct jag_iopb	*iopb;
	register struct scsi_request	*req = NULL;
	struct jagrequest		*jreq;
	ushort			 	 vmecrsw;
	int				 i, j;

	ci = &jagctlrinfo[ctlr];
	crbo = (struct jag_crb *) ci->crb;
	crb = jag_crb[ctlr];
	iopb = &crb->iopb;

	spsema(ci->lock);
	vmecrsw = crbo->crsw;
	if (!(vmecrsw & 1))
	{
		printf("jag%d: spurious interrupt\n", ctlr);
		svsema(ci->lock);
		return;
	}

	spsema(ci->auxlock);
	ci->intr = 1;
	svsema(ci->auxlock);

	/*
	 * Determine if there was an error.  If so, then the crb may not have
	 * made it back to local memory, so we copy it in by hand.
	 */
	if (vmecrsw & 0x84)
		for (i = 0; i < sizeof(struct jag_crb) / sizeof(short); i++)
			((short *) crb)[i] = ((short *) crbo)[i];

	/*
	 * If bit 7 (0x80) in the crsw is set, that means that a controller
	 * error occurred.  This error is different from other errors in that
	 * there is no IOPB associated with it.
	 */
	if (crb->crsw & 0x80)
	{
		jagscerror(ci, (ushort *) crb);
		goto jagintr_end;
	}

	/*
	 * Check the work queue number.  If it is 0, that indicates that a
	 * command which was not SCSI Passthrough has completed.  Currently,
	 * the only such command executed after system startup is the
	 * SCSI bus reset.
	 */
	if (crb->wq_num == 0) /* reset SCSI bus completion */
	{
		/*
		 * Find out what bus the reset was on.  If there are any
		 * IOPBs outstanding, it means that the board has lost
		 * track, since it returns them with error before it returns
		 * the Reset SCSI Bus IOPB.
		 */
		vmecrsw = iopb->reserve4 >> 15;
		for (i = 0; i < JAG_UPC/2; i++)
		{
			ui = ci->unit[i + vmecrsw * (JAG_UPC/2)];
			ui->aenreq = 1;
			ui->abortqueue = 1;
			jagclear(ci, ui, "bus reset");
			ui->u_ca = 0;
		}

		if (crb->crsw & 0x2)
			printf("jag%d: reset complete on bus %d\n",
			       ctlr, vmecrsw);
		else
			printf("jag%d: reset unsuccessful on bus %d\n",
			       ctlr, vmecrsw);

		/* Clear the reset in progress flag. */
		ci->reset[vmecrsw] = 0;
		goto jagintr_end;
	}

	/*
	 * Now that the crb is associated with an IOPB, we can determine
	 * which target (or unit) it is associated with.
	 */
	ui = &jagunitinfo[ctlr][crb->wq_num - 1];
#if _MIPS_SIM == _ABI64
	jreq = (struct jagrequest *) (PHYS_TO_K0((ulong) crb->tag << 2));
#else
	jreq = (struct jagrequest *) crb->tag;
#endif
	req = jreq->sreq;
#if JDEBUG
	if (jagtrace)
	{
		jagtrace--;
		printf("jag%dd%d: ui 0x%x; jreq == 0x%x; req == 0x%x\n",
		       ctlr, ui->devnum, ui, jreq, req);
	}
	if (!jreq->active)
	{
		printf("jag%dd%d: ui 0x%x; jreq == 0x%x (NOT ACTIVE); req == 0x%x\n",
		       ctlr, ui->devnum, ui, jreq, req);
	}
#endif
	ASSERT(jreq->active);

	/*
	 * Status 0x34 is a transfer count exception.  Amount of
	 * data actually transferred is in the iopb length field.
	 */
	if ((iopb->status & 0xFF) == 0x34)
	{
		if (ui->sense)
			req->sr_sensegotten = iopb->length;
		else
		{
			if (ci->ignorexcept)
				req->sr_resid = 0;
			else
				req->sr_resid = req->sr_buflen - iopb->length;
		}
		iopb->status &= 0xFF00;
	}
	else if ((iopb->status & 0xFF) == 0)
	{
		if (ui->sense)
			req->sr_sensegotten = SCSI_SENSE_LEN;
		else
			req->sr_resid = 0;
	}
	if (ui->sense && ((short) req->sr_senselen < req->sr_sensegotten))
		req->sr_sensegotten = req->sr_senselen;

	/*
	 * Bit 2 (0x04) indicates a regular error associated with an IOPB.
	 * If the error is a check condition, then issue the request sense.
	 * IOPB's that complete with errors during SCSI bus reset are held
	 * until reset is complete.
	 */
	if (crb->crsw & 0x14)
	{
		ui->abortqueue = 1;
		if (iopb->status == (ST_CHECK << 8) && !ui->sense)
		{
			ui->u_ca = 1;
			ui->aenreq = 1;
			jagreqsense(ci, ui, crb, jreq);
			req = NULL;
			goto jagintr_end;
		}
		jagerror(ci, ui, crb, iopb);

		/*
		 * If in the middle of a SCSI bus reset, or a command was
		 * aborted, don't report completion yet.
		 */
		if (ci->reset[ui->devnum / 8] || (crb->crsw & 0x10))
		{
			req = NULL;
			goto jagintr_end;
		}
	}
	else
	{
		if ((crb->crsw & 0x08) && (iopb->status & 0xFF))
			printf("jag%dd%d: exception code %x\n",
			       ci->number, ui->devnum, iopb->status);
#if JDEBUG
		if (ci->reset[0] || ci->reset[1])
		printf("jag%dd%d: GOOD completion after reset issued\n",
		       ctlr, ui->devnum);
#endif
	}

	jagcomplete(ci, ui, jreq, req);

jagintr_end:
	dki_dcache_inval(crb, sizeof(*crb));
	crbo->crsw = 0;

	if (req != NULL)
	{
		if ((req->sr_flags & SRF_FLUSH) &&
		    (req->sr_flags & SRF_DIR_IN))
			dki_dcache_inval(req->sr_buffer, req->sr_buflen);

		/* copy data from spare align buffer */
		if (req->sr_ha_flags & JFLAG_ODDBYTEFIX)
		{
			i = (io_poff(req->sr_buffer) + req->sr_buflen) % IO_NBPP;
			if (i > req->sr_buflen)
				i = req->sr_buflen;
			j = i - req->sr_resid;
			if (j > 0)
			{
				dki_dcache_inval(ci->page, j);
				bcopy(ci->page,
				      &req->sr_buffer[req->sr_buflen - i], j);
			}
			ci->pagebusy = 0;
		}

		/* report completion on current command */
		(*req->sr_notify)(req);

		/*
		 * If there was an error, then all additional commands
		 * oustanding should be aborted.
		 */
		if (ui->u_ca)
		{
			/* Clear the in-progress and wait queues */
			jagclear(ci, ui, "error in other command");
			ui->u_ca = 0;
		}

		/* Start up any waiting commands. */
		jagstart(ci, ui);
	}

	spsema(ci->auxlock);
	ci->intr = 0;
	while (ci->auxhead)
	{
		req = ci->auxhead;
		ci->auxhead = req->sr_ha;
		ui = ci->unit[jag_unitnum[req->sr_target+(req->sr_ctlr & 1)*8]];
		req->sr_ha = NULL;

		if (ui->aenreq && !(ui->u_ca || ci->reset[ui->devnum / 8]))
		{
			if (req->sr_flags & SRF_AEN_ACK)
				ui->aenreq = 0;
			else
			{
				req->sr_status = SC_ATTN;
				jdprintf(("jag: aborting, Q'd during intr\n"));
				(*req->sr_notify)(req);
				continue;
			}
		}

		if (ui->reqhead == NULL)
			ui->reqhead = req;
		else
			ui->reqtail->sr_ha = req;
		ui->reqtail = req;
		/* jdprintf(("jag: starting during interrupt\n")); */
		jagstart(ci, ui);
	}
	svsema(ci->auxlock);

	svsema(ci->lock);
}
#endif /* EVEREST */
