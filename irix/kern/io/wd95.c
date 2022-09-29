/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1993, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if defined(EVEREST) || defined(IP22)
#ident	"io/wd95.c: $Revision: 1.160 $"

#define WD95_CMDHIST	0

#include <sys/types.h>
#include <sys/cpu.h>
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
#include <sys/wd95a.h>
#include <sys/wd95a_struct.h>
#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/failover.h>
#include "wd95a_wcs.h"
#include <bstring.h>
#include <string.h>

#define	WD95PRI 0	/* Doesn't matter, since pri isn't used anymore */

#define INITLOCK(l,n,s) init_mutex(l, MUTEX_DEFAULT, n, s)
#define LOCK(l)         mutex_lock(&(l), WD95PRI)
#define UNLOCK(l)       mutex_unlock(&(l))
#define IOLOCK(l,s)     mutex_lock(&(l), WD95PRI)
#define IOUNLOCK(l,s)   {wbflush(); mutex_unlock(&(l));}
#define SPL_DECL(s)

#define	WCS_I_ATTENTION		0x4F
#define	WCS_I_DATA_NOT_OK	0x1A

#define WDEBUG		0
#define WTIMEOUT	0
#define	D_C_DEBUG	0
#define	D_X_DEBUG	0
#define	WD_I_DEBUG	0	/* XXX Turn off logging for speed */

#define	MSG_SDTR	0x01		/* sync request message */
#define	MSG_WIDE	0x03		/* wide request message */
#define	MSG_WIDE_8	0x00		/* info for 8 bit */
#define	MSG_WIDE_16	0x01		/* info for 16 bit */

#define	MSG_EXT		0x01		/* extended message byte */
#define	MSG_SDTR_LEN	3		/* sync request len */

#define	SCSI_INFO_BITS	7		/* inquiry bit info */
#define	SCSI_WIDE_BIT	0x20		/* set if dev can support 16 bit */
#define	SCSI_SYNC_BIT	0x10		/* set if dev can support SYNC */
#define	SCSI_CTQ_BIT	0x02		/* set if dev can support CMD TAGS */

#define	MAX_OFFSET	32		/* maximum byte in scsi req/ack que */

#ifdef IP22
#define	SPL_WD95	spl5
#else
#define	SPL_WD95	spl3
#endif

#define PL_WD95		pldisk

#if D_C_DEBUG
int	wdcp = 0;
#define	wdcprintf(_x)	if (wdcp) {printf _x ;}
#else
#define	wdcprintf(_x)
#endif

#if D_X_DEBUG
#define	wdxprintf(_x)	printf _x
#else
#define	wdxprintf(_x)
#endif

#if WD_I_DEBUG
int	wdip = 0;
#define	wdiprintf(_x)	if (wdip) {printf _x ;}
#else
#define	wdiprintf(_x)
#endif

#if WDEBUG
int	 wd95trace = 0;
int	 wd95queue = 0;
#define wdprintf(x) printf x
#else
#define wd95queue 0
#define wdprintf(x)
#endif

int	wd95inited;	/* suppress selection timeout msgs when set */
int	wd95_sync_en = 1;	/* allow synchronous transactions */
int	wd95_wide_en = 1;	/* allow wide negotiations */
int	wd95_disc_en = 1;	/* and this is for disconnect/reconnect */
int	wd95_ctq_en = 1;	/* allow cmd tag queueing */
int	wd95_no_next = 0;	/* this is so that we do this command now */

wd95ctlrinfo_t	*wd95dev;
wd95unitinfo_t	*wd95unitinfo;

extern int wd95cnt;
extern scuzzy_t scuzzy[];
extern u_char wd95_sync_off[];
extern uint wd95mintimeout;
extern char arg_wd95hostid[];

/* Prototypes: */
static void	wd95start(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui);
static int	wd95busreset(register wd95ctlrinfo_t *ci);
static void	wd_rm_wq(register wd95unitinfo_t *ui, register wd95request_t *wreq);
static void	wd_clr_wq(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui, 
			wd95luninfo_t *li, char *data);

static void	wd95a_do_cmd(wd95unitinfo_t *, wd95request_t *);

static int wd95abort(scsi_request_t *req);

#ifdef EVEREST
extern int	scsi_is_external(const int);
extern void	wd95_usergo(dmamap_t *, struct buf *, char *, int);
#endif
#ifdef IP22
extern dmamap_t *scsi_getchanmap(dmamap_t *, int);
#define scsi_is_external(x) 1
#endif

/* externs from master.d/wd95a */
extern uint	 wd95_hostid;

/* SCSI command arrays */
#define	SCSI_INQ_CMD	0x12
#define	SCSI_REQ_SENSE	0x03
static u_char dk_inquiry[] =	{SCSI_INQ_CMD, 0, 0, 0,    4, 0};

/* used to get integer from non-aligned integers */
#define MKINT(x) (((x)[0] << 24) + ((x)[1] << 16) + ((x)[2] << 8) + (x)[3])

/*
 * Define macros for approved kernel memory alloc function.
 */
#define dma_malloc(len) kmem_alloc(len, KM_CACHEALIGN)
#define dma_contig(len) kmem_alloc(len, KM_CACHEALIGN | KM_PHYSCONTIG)
#define dma_free(ptr) kern_free(ptr)

#if defined(EVEREST)
#define	ACC_SHIFT	3		/* if you prefer shifting */
#define	WD_C_B(_ctlr)	(((_ctlr / 8) * 10) + (_ctlr % 8))
#else
#define	WD_C_B(_ctlr)	(_ctlr)
#endif /* EVEREST */

#if WD_I_DEBUG
#define	WDI_LOG_CNT	512


set_wd_reg(register volatile char * sbp, register u_char reg, register u_char val,
		register wd95ctlrinfo_t *ci)
{
	register ushort *wdi_log;
	if (wd95inited) wdiprintf(("w%d %d ", reg, val));
	if (ci && (wdi_log = ci->wdi_log)) {
		ushort wdi_cnt = *wdi_log;
		wdi_log[wdi_cnt++] = (reg << 8) | val;
		if (wdi_cnt >= WDI_LOG_CNT)
			*wdi_log = 1;
		else
			*wdi_log = wdi_cnt;
	}
#ifdef IP22
	*(volatile unsigned char *)(sbp) = (reg); *(volatile unsigned char *)(sbp + 8) = (val);
#else
	*(uint *)(sbp + (reg << ACC_SHIFT) + 4) = val;
#endif
}

#define SET_WD_REG(_sbp, _reg, _val) set_wd_reg((u_char *) _sbp, _reg, _val, ci)

get_wd_reg(register volatile char * sbp, register u_char reg,
		register wd95ctlrinfo_t *ci)
{
	register ushort *wdi_log;
	register u_char val;

#ifdef IP22
	*(volatile unsigned char *)(sbp) = (reg); val = *(volatile unsigned char *)(sbp + 8);
#else
	val = *(uint *)(sbp + (reg << ACC_SHIFT) + 4);
#endif
	if (ci && (wdi_log = ci->wdi_log)) {
		ushort wdi_cnt = *wdi_log;
		wdi_log[wdi_cnt++] = ((reg | 1) << 8) | val;
		if (wdi_cnt >= WDI_LOG_CNT)
			*wdi_log = 1;
		else
			*wdi_log = wdi_cnt;
	}
	return val;
}

#define GET_WD_REG(_sbp, _reg) get_wd_reg((u_char *) _sbp, _reg, ci)

#else /* WD_I_DEBUG */

#ifdef IP22
#define SET_WD_REG(_sbp, _reg, _val)\
        { *(volatile unsigned char *)(_sbp) = (_reg); *(volatile unsigned char *)(_sbp + 8) = (_val); }

#define GET_WD_REG(_sbp, _reg) \
        ( *(volatile unsigned char *)(_sbp) = (_reg), *(volatile unsigned char *)(_sbp + 8) )

#else
#define	GET_WD_REG(_sbp, _reg) \
		(*(volatile unsigned int *)(_sbp + (_reg << ACC_SHIFT) + 4) & 0xff)
#define	SET_WD_REG(_sbp, _reg, _val) \
		*(volatile unsigned int *)(_sbp + (_reg << ACC_SHIFT) + 4) = (_val)
#endif /* IP22 */
#endif /* WD_I_DEBUG */


#define	SCNF_INIT	((4 << W95_SCNF_CLK_SHFT) | W95_SCNF_SPUE | W95_SCNF_SPDEN)
#ifdef IP22
#define	DMACNF_INIT	(W95_DCNF_DMA16 | W95_DCNF_DPEN | W95_DCNF_BURST)
#else
#define	DMACNF_INIT	(W95_DCNF_DMA16 | W95_DCNF_DPEN | W95_DCNF_PGEN | \
				W95_DCNF_BURST)
#endif

#define	SPW(per) (((per) << W95_SPW_SCLKA_SFT) | ((per) << W95_SPW_SCLKN_SFT))
#define	SPW_SLOW	SPW(2)


#define	TIMEOUT_INIT	77		/* timeout value */
#define	SLEEP_INIT	0		/* sleep value */

#define	ISRM_INIT	(W95_ISRM_STOPWCSM | W95_ISRM_INTWCSM | \
				W95_ISRM_STOPUM | W95_ISRM_UEIM | \
				W95_ISRM_VBUSYIM | W95_ISRM_SREJM)

#define	STOPUM_INIT	(W95_STPUM_ABORTM | W95_STPUM_SCSITM | \
				W95_STPUM_PAREM | W95_STPUM_LRCEM | \
				W95_STPUM_TCUNDM | W95_STPUM_SOEM)

#define	UEIM_INIT	(W95_UEIM_FIFOEM | W95_UEIM_ATNIM | \
				W95_UEIM_UDISCM | W95_UEIM_TCOVRM | \
				W95_UEIM_USELM | W95_UEIM_URSELM | \
				W95_UEIM_UPHASM | W95_UEIM_RSTINTM)

#define	CTLA_INIT	(W95_CTLA_PBS | W95_CTLA_BYTEC)

#define	WD95_RESEL_EN	(W95_RESP_RSELL | W95_RESP_RSELH)

#define	DMATIM_INIT	(W95_DTIM_DMAOE)

#define	W95_TC_CLR	(W95_CTL_LDTC | W95_CTL_LDTCL)


#include <sys/iograph.h>
/*
 * get the vertex handle corr. to the wd95 controller
 * with the given adapter number
 */

vertex_hdl_t
wd95_ctlr_vhdl_get(int adap)
{
	char		loc_str[8];
	vertex_hdl_t	vhdl;
	/*REFERENCED*/
	graph_error_t	rv;
	sprintf(loc_str,"%s/%d",EDGE_LBL_SCSI_CTLR,SCSI_EXT_CTLR(adap));

	/* 
	 * just does the path traversal to get the vertex handle
	 * vertex has already been added to the hardware graph
	 */
	rv = hwgraph_traverse(	hwgraph_root,
				loc_str,
				&vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (vhdl != GRAPH_VERTEX_NONE));
	rv = rv;
	return(vhdl);
}

#if WD95_CMDHIST
wd95_cmdhist_dump(wd95unitinfo_t *ui, wd95request_t *wreq)
{
	int i, x;
	printf("  Command bytes:");
	if (wreq)
		for (i = 0; i < wreq->sreq->sr_cmdlen; i++)
			printf(" 0x%x", wreq->sreq->sr_command[i]);
	printf("\n");
	for (x = ui->prevcmd_rotor; x < 16; x++)
	{
		for (i = 0; i < 10; i++)
			printf(" 0x%x", ui->prevcmd[x][i]);
		printf("\n");
	}
	for (x = 0; x < ui->prevcmd_rotor; x++)
	{
		for (i = 0; i < 10; i++)
			printf(" 0x%x", ui->prevcmd[x][i]);
		printf("\n");
	}
}

wd95_cmdhist_log(wd95unitinfo_t *ui, scsi_request_t *req)
{
	ui->prevcmd_rotor--;
	if (ui->prevcmd_rotor < 0)
		ui->prevcmd_rotor = 15;
	bcopy(req->sr_command, ui->prevcmd[ui->prevcmd_rotor], req->sr_cmdlen);
}
#else
#define wd95_cmdhist_dump(ui, wreq)
#define wd95_cmdhist_log(ui, req)
#endif

#if WDEBUG
static void
printdata(char *prefix, char *ptr, int length)
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
wd95sensepr(int ctlr, int unit, int key, int asc, int asq)
{
	printf("wd95_%dd%d: ", WD_C_B(ctlr), unit);
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


static void
wd95timostart(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui)
{

	SPL_DECL(s)

#if WDEBUG
	printf("wd95_%dd%d: attempting to start\n", WD_C_B(ci->number), ui->number);
#endif
	IOLOCK(ci->lock, s);
	ui->tostart = 0;
	wd95start(ci, ui);
	IOUNLOCK(ci->lock, s);
}

wd95request_t *
get_wqueued(register wd95luninfo_t *li, int tag)
{
	register wd95request_t	*wdp = li->wqueued;

	if (tag == 0) {		/* special tag, ie no tag */
		while (wdp && wdp->ctq)
			wdp = wdp->next;
	} else {
		while (wdp && (!wdp->ctq || (wdp->cmd_tag != (u_char)tag) ))
			wdp = wdp->next;
	}
	return wdp;
}

#define	NEXT_LUN(_x)	((((_x) + 1) < WD95_MAXLUN) ? ((_x) + 1) : 0)

static struct scsi_request *
getreq(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui)
{
	register wd95luninfo_t  *li;
	scsi_request_t *req = NULL;
	int		lun, i;

	/*
	 * Look through the request queue for an eligible command.
	 * and remember that all the different lun requests are queued up
	 * on this same queue.
	 */
	lun = ui->lun_round;
	for (i = 0; i < WD95_MAXLUN; i++) {
		lun = NEXT_LUN(lun);
		if ((li = ui->lun_info[lun]) == NULL)
			continue;
		if (li->l_ca)
			continue;
		if (req = li->reqhead) {
			if (li->wqueued) {
				if (req->sr_command[0] == SCSI_INQ_CMD) {
					req = NULL;
					continue;
				}
				/* don't add CTQs if we already have a
				   non-tagged command */
				if (!li->wqueued->ctq) {
					req = NULL;
					continue;
				}
				if (!req->sr_tag &&
				    req->sr_command[0] != SCSI_REQ_SENSE) {
					req = NULL;
					continue;
				}
				if (ui->wide_req || ui->sync_req) {
					req = NULL;
					continue;
				}
			}
			if ((req->sr_command[0] == SCSI_INQ_CMD) ||
			    (req->sr_command[0] == SCSI_REQ_SENSE)) {
			    if (ui->sync_en) ui->sync_req = 1;
			    if (ui->wide_en) ui->wide_req = 1;
			}
			if (req->sr_command[0] == SCSI_REQ_SENSE &&
					req->sr_spare) {
			    li->l_ca = 1;
			}

		/* If request is for an odd number of bytes, we must fix-up. */
		/* Don't do this for IP22, hpc3 seems to handle it ok. */
#if !defined(IP22)
			if ((req->sr_buflen % 2) && (req->sr_flags & SRF_DIR_IN)) {
				if (ci->pagebusy) {
#ifdef DEBUG
					printf("wd95_%dd%d: fixup page busy\n",
					    WD_C_B(ci->number), ui->number);
#endif
					if (!ui->tostart) {
					    itimeout(wd95timostart, ci, 2*HZ,
						 PL_WD95, ui);
					    ui->tostart = 1;
					}
					req = NULL;
					continue;
				}
				ci->pagebusy = 1;
				req->sr_ha_flags |= WFLAG_ODDBYTEFIX;
			}
#endif
			break;
		}
	}
	if (req != NULL) {
		if ((li->reqhead = req->sr_ha) == NULL)
			li->reqtail = NULL;
		req->sr_ha = NULL;
		ui->lun_round = lun;
	}
	return req;
}

void
wd95requeue(wd95unitinfo_t *ui)
{
	wd95ctlrinfo_t	*ci = ui->ctrl_info;
	wd95luninfo_t  *li;
	scsi_request_t *req;

	while (req = ui->auxhead) {
		if ((ui->auxhead = req->sr_ha) == NULL)
			ui->auxtail = NULL;
		req->sr_ha = NULL;
		if (ui->aenreq && !(ui->u_ca || ci->reset)) {
			if (req->sr_flags & SRF_AEN_ACK)
				ui->aenreq = 0;
			else {
				req->sr_status = SC_ATTN;
				wbflush();
				UNLOCK(ci->lock);
				(*req->sr_notify)(req);
				LOCK(ci->lock);
				continue;
			}
		}
		li = ui->lun_info[req->sr_lun];
		if (li->reqhead == NULL)
			li->reqhead = req;
		else
			li->reqtail->sr_ha = req;
		li->reqtail = req;
	}
}

#define WD95_INCTARG(t) ((t + 1) & (ci->max_round - 1));

static void
wd95start(register wd95ctlrinfo_t *ci, register wd95unitinfo_t *ui)
{
	register wd95luninfo_t	*li;
	register scsi_request_t	*req;
	register wd95request_t	*wreq;
	int			 unit_cntr;
	int			 origtarg;
	int			 time_val;

	/*
	 * If we are currently connected to the bus, or are in the middle
	 * of a bus reset, hold off on commands.
	 */
	if (ci->active || ci->reset)
		return;

	/*
	 * Try to start a command on the current unit.  If we can't, try the
	 * other units.  Since we skip the host adaptor id, we go through the
	 * loop max round times minus 1, unless we find something.
	 */
	unit_cntr = 0;
	origtarg = ui->number;
	while (unit_cntr++ < ci->max_round - 1) {
		/*
		 * the first time thru the loop we will be checking the
		 * device first passed in.  Subsequent times thru we will
		 * need to setup for the round_robin device.
		 */
		if (ui == NULL) {
			while ((ci->round_robin == origtarg) ||
			       (ci->round_robin == ci->host_id)) {
			    ci->round_robin = WD95_INCTARG(ci->round_robin);
			}
			ui = ci->unit[ci->round_robin];
			if (ui->auxhead)
				wd95requeue(ui);
			ci->round_robin = WD95_INCTARG(ci->round_robin);
		}
		/*
		 * Find out if the current unit is eligible for a command.
		 * Get a wreq if available, then get a req.
		 * If we get a wreq, but can't get a req, put the wreq back.
		 */
		if (ui->u_ca || (wreq = ui->wrfree) == NULL)
			ui = NULL;
		else {		/* CTQ enabled is checked in getreq */
			if ((req = getreq(ci, ui)) != NULL) {
				ui->wrfree = wreq->next;
			} else {
				ui = NULL;
			}
		}
		if (ui != NULL)
			break;
	}
	if (ui == NULL)
		return;

	ci->active = 1;
	ui->active = 1;
	ui->cur_lun = wreq->lun = req->sr_lun;
	li = ui->lun_info[ui->cur_lun];
	wreq->active = 1;
	wreq->tlen = 0;
	wreq->sreq = req;
	wreq->resid = wreq->save_resid = req->sr_buflen;
	if (li->ctq_en) {
		switch (req->sr_tag) {
		    case SC_TAG_SIMPLE:
		    case SC_TAG_ORDERED:
		    case SC_TAG_HEAD:
			wreq->ctq = 1;
			break;

		    default:
			wreq->ctq = 0;
			break;
		}
	} else {
		wreq->ctq = 0;
	}
	ci->cur_target = ui->number;

	/* add it to the que for this device */
	wreq->next = li->wqueued;
	li->wqueued = wreq;

	wd95_cmdhist_log(ui, req);
	wd95a_do_cmd(ui, wreq);

	ASSERT(wreq->timeout_id == 0);
	time_val = (req->sr_timeout > wd95mintimeout * HZ) ? req->sr_timeout :
						wd95mintimeout * HZ;
	wreq->timeout_id = itimeout(wd95timeout, wreq, time_val, PL_WD95,
				(int)wreq->timeout_num);
	wreq->reset_num = ci->reset_num;
	ASSERT(wreq->timeout_id != 0);
}


static scsi_request_t *
setup_sense_req(u_char ctlr, u_char targ, u_char lun) {
	register scsi_request_t *sprq;

	if (sprq = kmem_zalloc(sizeof(scsi_request_t) + SC_CLASS0_SZ,
					KM_CACHEALIGN)) {
		sprq->sr_ctlr = ctlr;
		sprq->sr_target = targ;
		sprq->sr_lun = lun;
		sprq->sr_command = (u_char *)&sprq[1];
		sprq->sr_cmdlen = SC_CLASS0_SZ;		/* never changes */
		sprq->sr_timeout = 30*HZ;		/* never changes */
		sprq->sr_tag = 0;
	}
	return sprq;   
}


	
int
wd95alloc(vertex_hdl_t lun_vhdl,int opt, void(*cb)())
{
	wd95ctlrinfo_t		*ci;
	wd95unitinfo_t		*ui;
	wd95luninfo_t		*li;
	mutex_t			*o_sem;
	u_char			qdepth = opt & SCSIALLOC_QDEPTH;

	scsi_ctlr_info_t	*ctlr_info ;
	u_char			ctlr;
	vertex_hdl_t 		ctlr_vhdl;
	u_char 			targ,lun;
	scsi_lun_info_t 	*lun_info = scsi_lun_info_get(lun_vhdl);

	targ 		= SLI_TARG(lun_info);
	lun 		= SLI_LUN(lun_info);
	ctlr_vhdl 	= SLI_CTLR_VHDL(lun_info);

	ctlr_info 	= scsi_ctlr_info_get(ctlr_vhdl);
	ctlr 		= SCI_ADAP(ctlr_info);

	/*
	 * Check for validity of arguments.
	 */
	if (ctlr >= wd95cnt || targ >= WD95_LUPC || lun >= WD95_MAXLUN) {
		return ENODEV;
	}
	ci = &wd95dev[ctlr];
	if (!ci->present || (targ == ci->host_id))
		return ENODEV;
	ui = ci->unit[targ];
	li = ui->lun_info[lun];

	o_sem = &ui->opensem;
	mutex_lock(o_sem, PRIBIO);
	if (li == NULL || li->tinfo.si_inq == NULL) {
		mutex_unlock(o_sem);
		(void)wd95info(lun_vhdl);
		mutex_lock(o_sem, PRIBIO);
		li = ui->lun_info[lun];
		if (li == NULL || li->tinfo.si_inq == NULL) {
			mutex_unlock(o_sem);
			return ENODEV;
		}
	}
	if (opt & SCSIALLOC_EXCLUSIVE) {
		if (li->refcnt) {
			mutex_unlock(o_sem);
			return EBUSY;
		}
		li->excl_act = 1;
	} else {
		if (li->excl_act) {
			mutex_unlock(o_sem);
			return EBUSY;
		}
	}
	if (cb) {
		if (li->sense_callback != NULL) {
			mutex_unlock(o_sem);
			return EINVAL;	/* EBUSY better, but want unique error... */
		} else {
			li->sense_callback = cb;
		}
	}
	li->refcnt++;
	if ( !li->u_sense_req ) {
		li->u_sense_req = setup_sense_req(ctlr, targ, lun);
	}
	if (qdepth > ui->wreq_alc[lun]) {
	    register wd95request_t *wreq;
	    int qcnt = qdepth - ui->wreq_alc[lun];

	    if ((qcnt + ui->wreq_cnt) < 255) {
		wreq = kmem_alloc(sizeof(wd95request_t) * qcnt,	KM_CACHEALIGN);
		if (wreq) {
		    ui->wreq_cnt += qcnt;
		    ui->wreq_alc[lun] += qcnt;
		    while (qcnt-- > 0) {
			wreq->ctlr = ctlr;
			wreq->unit = targ;
			wreq->lun = lun;
			wreq->cmd_tag = ui->cur_tag++;
			wreq->timeout_id = 0;
			wreq->next = ui->wrfree;
			ui->wrfree = wreq;
			wreq++;
		    }
		}
	    }
	}

	mutex_unlock(o_sem);
	return SCSIALLOCOK;
}

static void
wd95free(vertex_hdl_t lun_vhdl, void (*cb)())
{
	wd95ctlrinfo_t	*ci;
	wd95unitinfo_t	*ui;
	wd95luninfo_t	*li;
	scsi_ctlr_info_t	*ctlr_info;
	u_char			adap;

	vertex_hdl_t 		ctlr_vhdl;
	u_char 			targ,lu;
	scsi_lun_info_t 	*lun_info = scsi_lun_info_get(lun_vhdl);;

	targ 			= SLI_TARG(lun_info);
	lu 			= SLI_LUN(lun_info);
	ctlr_vhdl 		= SLI_CTLR_VHDL(lun_info);

	ctlr_info 		= scsi_ctlr_info_get(ctlr_vhdl);

	adap 			= SCI_ADAP(ctlr_info);
	if(adap >= wd95cnt || targ > WD95_LUPC || lu >= WD95_MAXLUN)
		return;

	ci = &wd95dev[adap];

	ui = ci->unit[targ];

	li = ui->lun_info[lu];

	mutex_lock(&ui->opensem, PRIBIO);
	if ( cb ) {
		if (li->sense_callback == cb) {
			li->sense_callback = NULL;
		}
		else printf("wd95(%d,%d,%d): callback 0x%x not active\n",
			WD_C_B(adap), targ, lu, li->sense_callback);
	}
	if ( ! li->refcnt ) {
#if wd95_verbose
		printf("attempted to free unallocated channel %d, %d, %d\n",
			WD_C_B(adap), target, lun);
#endif
	} else {
		if (--li->refcnt == 0) {
			li->excl_act = 0;
			if (li->sense_callback)
			    printf("wd95(%d,%d,%d) freed, but callback still 0x%x\n",
				WD_C_B(adap), targ, lu, li->sense_callback);
			li->sense_callback = NULL;
		}
#if wd95_verbose
		else printf("wd95(%d,%d,%d) but refcnt still %d\n",
			WD_C_B(adap), target, lun, li->refcnt);
#endif
	}
	mutex_unlock(&ui->opensem);
	return;
}

void
wd95command(scsi_request_t *req)
{
	wd95ctlrinfo_t	*ci;
	wd95unitinfo_t	*ui;
	wd95luninfo_t	*li;
	u_char		ctlr;
	u_char		targ;
	u_char		lun;
	SPL_DECL(s)

	req->sr_resid = req->sr_buflen;
	req->sr_status = 0;
	req->sr_scsi_status = req->sr_ha_flags = req->sr_sensegotten = 0;
	req->sr_spare = NULL;
	req->sr_ha = NULL;

	ctlr = req->sr_ctlr;
	if (ctlr >= wd95cnt) {
		req->sr_status = SC_REQUEST;
		goto err_wd95command;
	}
		
	ci = &wd95dev[ctlr];


	/*
	 * Check for validity of arguments.
	 */
	if ((ctlr = req->sr_ctlr) >= wd95cnt ||
	    (targ = req->sr_target) >= WD95_LUPC ||
	    (lun = req->sr_lun) >= WD95_MAXLUN ||
	    (targ == wd95dev[ctlr].host_id) ||
	    (req->sr_buflen >= 0xffffff) ||
	    (req->sr_notify == NULL)) {
		req->sr_status = SC_REQUEST;
		goto err_wd95command;
	}

	/* Buffers must aligned on a four-byte boundary */
	if (req->sr_buflen != 0 && ((ulong) req->sr_buffer % 4)) {
		printf("wd95%dd%d: starting address must be four-byte aligned\n",
		       WD_C_B(ctlr), targ);
		printcdb(req->sr_command);
		req->sr_status = SC_ALIGN;
		goto err_wd95command;
	}
	ui = ci->unit[targ];
	li = ui->lun_info[lun];

	/*
	 * Check to see if a device has been seen by wd95info previously.
	 */
	if (li == NULL || li->tinfo.si_inq == NULL || li->u_sense_req == NULL) {
		req->sr_status = SC_HARDERR;
		goto err_wd95command;
	}

	if((req->sr_flags & SRF_NEG_SYNC) && !ui->sync_en) {
		/* negotiate sync */
		ui->sync_en = 1;
		ui->sync_req = 1;
	}
	else if((req->sr_flags & SRF_NEG_ASYNC) && ui->sync_en) {
		/* negotiate async */
		ui->sync_en = 0;	/* do_a_cmd doesn't do this yet; XXX */
		ui->sync_req = 1;
	}

	wdcprintf(("w9c%dd%d", WD_C_B(ctlr), targ));
	IOLOCK(ci->lock, s);

	/*
	 * If ui->u_ca is set, we queue the request in the controller's
	 * auxiliary queue since we are executing in an interrupt
	 * routine and need to hold onto it for later
	 */
	if (ui->u_ca) {
		if (ui->auxhead == NULL)
			ui->auxhead = req;
		else
			ui->auxtail->sr_ha = req;
		ui->auxtail = req;
		goto end_wd95command;
	}
	/*
	 * Queue the command and attempt to issue it.
	 */
#if 0
	LOCK(ci->qlock);
#endif
	if (ui->aenreq && !(ui->u_ca || ci->reset)) {
		if (req->sr_flags & SRF_AEN_ACK)
			ui->aenreq = 0;
		else {
			req->sr_status = SC_ATTN;
#if 0
			UNLOCK(ci->qlock);
#endif
			IOUNLOCK(ci->lock, s);
			goto err_wd95command;
		}
	}
	if (li->reqhead == NULL)
		li->reqhead = req;
	else
		li->reqtail->sr_ha = req;
	li->reqtail = req;
	wd95start(ci, ui);
#if 0
	UNLOCK(ci->qlock);
#endif
end_wd95command:
	IOUNLOCK(ci->lock, s);
	if (wd95_no_next) {
		wd95request_t *wreq = li->wqueued;
		int timeo_time = (req->sr_timeout > wd95mintimeout * HZ) ?
				 req->sr_timeout : wd95mintimeout * HZ;

		timeo_time = rtodc() + (timeo_time + 2*HZ -1)/HZ;;
		while (ci->active) {
			while (!wd95_intpresent(ci) && rtodc() < timeo_time)
				DELAY(25);
			if (wd95_intpresent(ci))
				wd95intr(ci->number); /* do it */
			else
				wd95timeout(wreq, wreq->timeout_num);
		}
	}
	return;

err_wd95command:
	if (req->sr_notify != NULL)
		(*req->sr_notify)(req);
}


static void
wd95done(struct scsi_request *req)
{
	vsema(req->sr_dev);
}

struct scsi_target_info *
wd95info(vertex_hdl_t lun_vhdl)
{
	wd95ctlrinfo_t	*ci;
	wd95unitinfo_t	*ui;
	wd95luninfo_t	*li;
	scsi_request_t	*req;
	u_char		*scsi;
	char		registered = 0;
	char		stat;
	char		scsistat;
	mutex_t		*o_sem;
	sema_t		*sema;
	scsi_ctlr_info_t	*ctlr_info;
	u_char			ctlr;

	vertex_hdl_t 		ctlr_vhdl;
	u_char 			targ,lun;
	scsi_lun_info_t 	*lun_info = scsi_lun_info_get(lun_vhdl);

	int retry = 2;
	int busy_retry = 8;

	targ 			= SLI_TARG(lun_info);
	lun 			= SLI_LUN(lun_info);
	ctlr_vhdl 		= SLI_CTLR_VHDL(lun_info);

	ctlr_info 		= scsi_ctlr_info_get(ctlr_vhdl);

	ctlr 			= SCI_ADAP(ctlr_info);
	SPL_DECL(s)


	/*
	 * Check for validity of arguments.
	 */
	if (ctlr >= wd95cnt || targ >= WD95_LUPC || lun >= WD95_MAXLUN) {
		return NULL;
	}
	ci = &wd95dev[ctlr];
	if (!ci->present || (targ == ci->host_id))
		return NULL;

	ui = ci->unit[targ];

	o_sem = &ui->opensem;
	mutex_lock(o_sem, PRIBIO);
	if ((li = ui->lun_info[lun]) == NULL) {
		li = (wd95luninfo_t *)kmem_zalloc(sizeof(wd95luninfo_t), KM_CACHEALIGN);
		ui->lun_info[lun] = li;
	}

	/*
	 * If we haven't done so already, allocate DMA maps, sense and
	 * inquiry buffers for this device.
	 */
	if (li->tinfo.si_inq == NULL) {
		li->tinfo.si_inq = kmem_alloc(SCSI_INQUIRY_LEN +10, KM_CACHEALIGN);
		li->tinfo.si_sense = kmem_alloc(SCSI_SENSE_LEN, KM_CACHEALIGN);
		li->u_sense_req = setup_sense_req(ctlr, targ, lun);
	} else
		registered = 1;

	/*
	 * Issue inquiry to find out if this address has a device.
	 * If we get an error, free the DMA map, sense and inquiry buffers.
	 */
	scsi = &(li->tinfo.si_inq[SCSI_INQUIRY_LEN]);
	bcopy(dk_inquiry, scsi, sizeof(dk_inquiry));
	scsi[4] = SCSI_INQUIRY_LEN;
	scsi[1] = lun << 5;

	req = kern_calloc(1, sizeof(*req));
	sema = kern_calloc(1, sizeof(sema_t));
	init_sema(sema, 0,"w95", ctlr<<4 | targ);

	do {

		bzero(req, sizeof(*req));

		req->sr_ctlr = ctlr;
		req->sr_target = targ;
		req->sr_lun = lun;
		req->sr_command = scsi;
		req->sr_lun_vhdl = lun_vhdl;
		req->sr_cmdlen = sizeof(dk_inquiry);
		req->sr_flags = SRF_DIR_IN | SRF_FLUSH | SRF_AEN_ACK | \
					SRF_NEG_SYNC;
		req->sr_timeout = 10 * HZ;
		req->sr_buffer = li->tinfo.si_inq;
		req->sr_buflen = SCSI_INQUIRY_LEN;
		req->sr_sense = NULL;
		req->sr_notify = wd95done;
		req->sr_dev = sema;
	        bzero(li->tinfo.si_inq, SCSI_INQUIRY_LEN);
#ifdef IP22
		dki_dcache_wbinval(li->tinfo.si_inq, SCSI_INQUIRY_LEN);
#endif
		wd95command(req);
		psema(sema, PRIBIO);

		stat = req->sr_status;
		if ((scsistat = req->sr_scsi_status) == ST_BUSY) 
			delay(HZ/2);

	} while ((stat == SC_ATTN && retry--) || 
		 (scsistat == ST_BUSY && busy_retry--));

	freesema(sema);
	kern_free(sema);
	kern_free(req);

	if (stat || scsistat) {
_info_errexit:
		if(!registered) {
			if (lun != 0) {
				IOLOCK(ci->lock, s);
				ui->lun_info[lun] = NULL;
				IOUNLOCK(ci->lock, s);
			}

			/* only if alloced on this call, otherwise can free
			 * one that worked before on things like BUSY status,
			 * and that slows things down (extra malloc/free, and
			 * redundant add_to_inventory calls). */
			kern_free(li->tinfo.si_inq);
			kern_free(li->tinfo.si_sense);
			kern_free(li->u_sense_req);

			li->tinfo.si_inq = NULL;
			li->tinfo.si_sense = NULL;
			li->u_sense_req = NULL;
			if (lun != 0)
				kern_free(li);
		}
		mutex_unlock(o_sem);
		return NULL;
	}
	ui->present = 1;

	/*
	 * Check device type qualifier.
	 * If the LUN is not configurable, don't allow access.
	 */
	if ((li->tinfo.si_inq[0] & 0xC0) == 0x40 && 
	    !fo_is_failover_candidate(li->tinfo.si_inq, lun_vhdl)) {
		if (!wd95inited)
			printf("wd95_%dd%dl%d: lun not valid, device type "
			       "qualifier 0x%x\n", WD_C_B(ctlr), targ, lun,
			       (li->tinfo.si_inq[0] & 0xE0) >> 5);
		goto _info_errexit;
	}

	li->tinfo.si_ha_status = 0;
	{
		unchar *ip = li->tinfo.si_inq;
		register u_char flags = wd95_bus_flags[ctlr];
		IOLOCK(ci->lock, s);
		if ( (ip[SCSI_INFO_BITS] & SCSI_WIDE_BIT) && wd95_wide_en 
				&& !ci->narrow ) {
			ui->wide_en = 1;
			ui->wide_req = 1;
			li->tinfo.si_ha_status |= SRH_WIDE;
		}
		if (wd95_sync_en && (flags & WD95BUS_SYNC)) {
			ui->sync_en = 1;
			ui->sync_req = 1;
		}
		else li->tinfo.si_ha_status |= SRH_NOADAPSYNC;
		if (wd95_disc_en && (flags & WD95BUS_DISC)) {
			ui->disc = 1;
			li->tinfo.si_ha_status |= SRH_DISC;
		}
		if (wd95_ctq_en && wd95_disc_en && (flags & WD95BUS_CTQ) &&
			    (ip[SCSI_INFO_BITS] & SCSI_CTQ_BIT)) {
			li->ctq_en = 1;
			li->ctq_drop_err = 1;
			li->tinfo.si_ha_status |= SRH_TAGQ | SRH_QERR1;
		}
		IOUNLOCK(ci->lock, s);
	}


	scsi_device_update(li->tinfo.si_inq,lun_vhdl);
	if(ui->sync_en && !ui->sync_req)	/* pointless for now, as
		the driver doesn't do sync on inquire, and it always sets
		req in this routine, but... */
		li->tinfo.si_ha_status |= SRH_SYNCXFR;

#if defined(EVEREST)
	li->tinfo.si_ha_status |= SRH_MAPUSER;
#endif
	li->tinfo.si_maxq = WD95_QDEPTH;
	mutex_unlock(o_sem);
	return &li->tinfo;
}

scsi_request_t *wd95dmp_q = NULL;

int
wd95dump(vertex_hdl_t ctlr_vhdl)
{
	register wd95ctlrinfo_t *ci;
	register wd95unitinfo_t	*ui;
	register wd95luninfo_t	*li;
	register wd95request_t *wreq;
	register scsi_request_t *req;
	int	cnt, lun;
	u_char 			adap;
	scsi_ctlr_info_t 	*ctlr_info;

	ctlr_info	= scsi_ctlr_info_get(ctlr_vhdl);
	adap = SCI_ADAP(ctlr_info);
	wd95_no_next = 1;

	ci = &wd95dev[adap];
	ci->active = 0;

	INITLOCK(&ci->lock, "dpq", adap);
	LOCK(ci->lock);

	wd95busreset(ci);
	wd95_fin_reset(ci);
	DELAY(1000000); /* wait 1 second for peripherals to do POR testing */

	for (cnt = 0; cnt < WD95_LUPC; cnt++) {
		ui = ci->unit[cnt];
		ui->disc = 0;
		for (lun = 0; lun < WD95_MAXLUN; lun++) {
			if ((li = ui->lun_info[lun]) == NULL)
				continue;
			while (wreq = li->wqueued) {
				req = wreq->sreq;
				if ( !(req->sr_ha = li->reqhead) )
					li->reqtail = req;
				li->reqhead = req;
				wd_rm_wq(ui, wreq);
			}
			if (li->reqhead) {
				li->reqtail->sr_ha = wd95dmp_q;
				wd95dmp_q = li->reqhead;
				li->reqhead = li->reqtail = NULL;
			}
		}
		ui->active = 0;
	}
	wd95_disc_en = 0;

	UNLOCK(ci->lock);

	return 1;
}


/*
 * Clear Interrupt Status Registers and double check to see they are clear.
 */
/* ARGSUSED */
static int
wd95_clear_isrs(register wd95ctlrinfo_t *ci, register volatile char *ibp)
{
	u_char reg, mask;

	/*
	 * Clear interrupt registers.
	 */
	reg = GET_WD_REG(ibp, WD95A_N_UEI);
	for (mask = 0x80; mask != 0; mask >>= 1)
		if (reg & mask) {
			SET_WD_REG(ibp, WD95A_N_UEI, mask);
		}
	reg = GET_WD_REG(ibp, WD95A_N_STOPU);
	for (mask = 0x20; mask != 0; mask >>= 1)
		if (reg & mask) {
			SET_WD_REG(ibp, WD95A_N_STOPU, mask);
		}
	reg = GET_WD_REG(ibp, WD95A_N_ISR);
	for (mask = 0x40; mask != 0; mask >>= 1)
		if (reg & mask) {
			SET_WD_REG(ibp, WD95A_N_ISR, mask);
		}

	/*
	 * Check to make sure registers are cleared.
	 */
	if (GET_WD_REG(ibp, WD95A_N_UEI) ||
	    GET_WD_REG(ibp, WD95A_N_STOPU) ||
	    GET_WD_REG(ibp, WD95A_N_ISR))
	{
		return 1;
	}
	return 0;
}


/*
 * Yank the master reset pin and hold it for 100 microsecs.
 * Then sleep for 1000ms to give the SCSI disks time to reset.
 */
static int
wd95busreset(register wd95ctlrinfo_t *ci)
{
	register volatile char *ibp = ci->ha_addr;
	int val;
	int retval = 0;

	val = GET_WD_REG(ibp, WD95A_S_CTL) & W95_CTL_SETUP;

	SET_WD_REG(ibp, WD95A_S_CTL, 0);
	SET_WD_REG(ibp, WD95A_N_TC_0_7, 0);
	SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
	scsidma_flush(ci->d_map, 0); /* Turn off DMA */

	SET_WD_REG(ibp, WD95A_S_CTL, W95_CTL_RSTO | 1);
	DELAY(400);

	/*
	 * Check to make sure that the bus is clear after a SCSI reset.
	 * If it isn't that's probably due to bad cabling/HW connections.
	 */
#define	SC1_BUSSIGNALS	(W95_SC1_REQ | W95_SC1_ACK | W95_SC1_MSG | \
			 W95_SC1_CD | W95_SC1_IO | W95_SC1_ATNL)
#define SC0_BUSSIGNALS	(W95_SC0_SELI | W95_SC0_BSYI | W95_SC0_SDP)
	if ((GET_WD_REG(ibp, WD95A_S_SC1) & SC1_BUSSIGNALS) ||
	    (GET_WD_REG(ibp, WD95A_S_SC0) & SC0_BUSSIGNALS))
	{
		printf("wd95_%d: SCSI bus not clear after reset\n",
				WD_C_B(ci->number));
		retval = -1;
	}

	/*
	 * Deassert SCSI bus reset and clear the reset interrupt.
	 */
	SET_WD_REG(ibp, WD95A_S_CTL, 0);
	SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_RSTINTL);

	/*
	 * Reset the WD95 chip and ensure ISRs are cleared.
	 */
	wd93_resetbus(ci->number);
	DELAY(400);
	if (init_wd95a(ibp, ci->host_id, ci)) {
		printf("wd95_%d: failed Built-In SelfTest\n",WD_C_B(ci->number));
		retval = -1;
	}
	SET_WD_REG(ibp, WD95A_N_CTL, 0);
	if (wd95_clear_isrs(ci, ibp)) {
		printf("wd95_%d: Unable to clear interrupt status regs\n",
				WD_C_B(ci->number));
		retval = -1;
	}
	SET_WD_REG(ibp, WD95A_N_ISRM, ISRM_INIT);

	SET_WD_REG(ibp, WD95A_S_CTL, val);	/* restore orig value */
	return retval;
}

/* go and clean up all the things that we have queued */
void
wd95_fin_reset(wd95ctlrinfo_t *ci)
{
	register wd95unitinfo_t	*ui;
	int	cnt;

	for (cnt = 0; cnt < WD95_LUPC; cnt++) {
		ui = ci->unit[cnt];
		if (ui->wide_en) ui->wide_req = 1;
		if (ui->sync_en) ui->sync_req = 1;
		ui->ext_msg = 0;
		ui->sync_per = SPW_SLOW;
		ui->sync_off = 0;
	}
}

/* ARGSUSED */
int
wd95ioctl(vertex_hdl_t ctlr_vhdl, uint cmd, scsi_ha_op_t *op)
{
	int err = 0;
	int 			adap;
	scsi_ctlr_info_t	*ctlr_info;
	vertex_hdl_t		lun_vhdl;
	
	ctlr_info = scsi_ctlr_info_get(ctlr_vhdl);
	adap = SCI_ADAP(ctlr_info);
	op = op;	/* to get rid of the compiler warning */
	switch (cmd)
	{
	case SOP_RESET:
	{
		struct wd95ctlrinfo *ci;
		SPL_DECL(pri)
		if ((ci = &wd95dev[adap]) == NULL)
		{
			err = EINVAL;
			break;
		}
		IOLOCK(ci->lock, pri);
		wd95reset(ci, ci->unit[ci->host_id], NULL, "user reset request");
		IOUNLOCK(ci->lock, pri);
		break;
	}

	case SOP_SCAN:
	{
		struct wd95ctlrinfo *ci;
		wd95unitinfo_t	*ui;
		wd95luninfo_t	*li;
		int i,j,refcnt;

		if (adap >= wd95cnt || ((ci = &wd95dev[adap]) == NULL) || !ci->present)	/* CONTROLLER */
		{
			err = EINVAL;
			break;
		}
		LOCK(ci->scanlock);
		for (i = 0; i < WD95_LUPC; i++)	/* TARGET */
		{
			/*
			 * Need to determine if the lun already exists
			 * from a previous scan so as to determine if the
			 * refcnt field is valid.
			 */
			ui = ci->unit[i];
			if (ui) {
				li = ui->lun_info[0];
				if (li && li->tinfo.si_inq) {	/* li present and inquiry buffer allocated (sanity check) */
					refcnt = li->refcnt;
				}
			}
			else refcnt = 0;
			/* 
			 * if the vertex for the device is not there
			 * create a vertex for the device
			 */
			lun_vhdl = scsi_device_add(ctlr_vhdl,i,0);	/* LUN 0 */
			/*
			 * check if the device actually exists 
			 * if there is no device remove  the vertex from the graph
			 */
			if (!wd95info(lun_vhdl) && !refcnt)
				scsi_device_remove(ctlr_vhdl,i,0);

			if (ci->unit[i]->present &&
			    !(wd95_bus_flags[adap] & WD95BUS_NO_PROBE_LUN))
			{
				for (j = 1; j < 8; j++) {	/* LUN 1-7 */
					li = ui->lun_info[j];
					if (li && li->tinfo.si_inq) {	/* li present and inquiry buffer allocated */
						refcnt = li->refcnt;
					}
					else refcnt = 0;
					/* 
					 * if the vertex for the device is not there
					 * create a vertex for the device
					 */
					lun_vhdl = scsi_device_add(ctlr_vhdl,i,j);
					/*
					 * check if the device actually exists 
					 * if there is no device remove  the vertex from the graph
					 */
					if (!wd95info(lun_vhdl) && !refcnt)
						scsi_device_remove(ctlr_vhdl,i,j);
				}
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

#define NUMOFCHARPERINT	10
/*
 * create a vertex for the wd95 controller and add the info
 * associated with it
 */
wd95ctlrinfo_t *
wd95_ctlr_add(int adap)
{
	vertex_hdl_t		ctlr_vhdl;
	char			loc_str[8];
	/*REFERENCED*/
	graph_error_t		rv;
	wd95ctlrinfo_t		*ci;
	scsi_ctlr_info_t	*ctlr_info;

	sprintf(loc_str,"%s/%d",EDGE_LBL_SCSI_CTLR,SCSI_EXT_CTLR(adap));
	rv = hwgraph_path_add(	hwgraph_root,
				loc_str,
				&ctlr_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (ctlr_vhdl != GRAPH_VERTEX_NONE));
	rv = rv;

	ctlr_info = (scsi_ctlr_info_t *)hwgraph_fastinfo_get(ctlr_vhdl);
	if (!ctlr_info) {
	    ctlr_info 			= scsi_ctlr_info_init();
	    ci 				= (wd95ctlrinfo_t *)kern_calloc(1,sizeof(*ci));
	    SCI_ADAP(ctlr_info)		= adap;
	    SCI_CTLR_VHDL(ctlr_info)	= ctlr_vhdl;

	    SCI_ALLOC(ctlr_info)	= wd95alloc;
	    SCI_COMMAND(ctlr_info)	= wd95command;
	    SCI_FREE(ctlr_info)		= wd95free;
	    SCI_DUMP(ctlr_info)		= wd95dump;
	    SCI_INQ(ctlr_info)		= wd95info;
	    SCI_IOCTL(ctlr_info)	= wd95ioctl;
            SCI_ABORT(ctlr_info)        = wd95abort;

		
	    SCI_INFO(ctlr_info)		= ci;
	    scsi_ctlr_info_put(ctlr_vhdl,ctlr_info);
	    scsi_bus_create(ctlr_vhdl);
	} else
	    ci = SCI_CTLR_INFO(ctlr_info);

	hwgraph_vertex_unref(ctlr_vhdl);
	return(ci);
}
/*
 * Probe the controller and find out what SCSI devices are attached.
 * Set up work queues.
 */
/*ARGSUSED*/
int
wd95edtinit(struct edt *e)
{

	wd95ctlrinfo_t	*ci;
	int		adap;
	int		i;
	wd95unitinfo_t	*ui;
	wd95luninfo_t	*li;
	int		cver;
	int		j;
	register volatile char *sbp;
	vertex_hdl_t	ctlr_vhdl,lun_vhdl;
	
	wd95inited = 1;	/* disable printing of selection timeouts */
	for (adap = 0; adap < wd95cnt; adap++) {
		ci = &wd95dev[adap];
		if ( !ci->ha_addr )
			continue;	/* no adapter here */

		/*
		 * Check/set transfer rate, validate connections, etc.
		 */
		sbp = ci->ha_addr;
		SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
		cver = GET_WD_REG(sbp, WD95A_S_CVER);
		ci->sync_period = wd95_syncperiod[adap];
		if (ci->sync_period < 25) {
			if ((cver & W95_CVER_SE) && scsi_is_external(adap))
				ci->sync_period = 50;
			else
				ci->sync_period = 25;
		}
		if (cver & W95_CVER_SE) {
			ci->diff = 0;
			if (ci->sync_period < 50 && scsi_is_external(adap)) {
			    printf("wd95_%d: WARNING - syncperiod < 50 not suggested"
				   " on external single ended buses\n", WD_C_B(adap));
			}
		} else {
			ci->diff = 1;
			if ( !(cver & W95_CVER_DSENS) ) {
			    printf("wd95_%d: differential bus with single ended drive"
				    " connected - bypassing\n", WD_C_B(adap));
			    ci->ha_addr = NULL;
			    continue;
			}
		}
		ci->revision = cver & 0xF;
		ci->sync_offset = wd95_sync_off[adap];
		if (ci->sync_offset > MAX_OFFSET)
			ci->sync_offset = MAX_OFFSET;
		if ( ! (wd95_bus_flags[adap] & WD95BUS_WIDE) ) {
			ci->narrow = 1;
		}

		/*
		 * initialize SW structures
		 */
		INITLOCK(&ci->lock, "wd95", adap);
		INITLOCK(&ci->auxlock, "wga", adap);
		INITLOCK(&ci->scanlock, "scan", adap);

		ci->page = kvpalloc(1, VM_DIRECT, 0);
		for (i = 0; i < WD95_LUPC; i++) {
			ci->unit[i] = ui = &wd95unitinfo[adap * WD95_LUPC + i];
			ui->ctrl_info = ci;
			ui->number = i;
			ui->sync_per = SPW_SLOW;
			ui->sync_off = 0;
			ui->sel_tar = WCS_I_SEL;
			ui->lun_round = 0;
			init_mutex(&ui->opensem, MUTEX_DEFAULT,"wuo", adap*16 + i);
			ui->wrfree = NULL;
			li = ui->lun_info[0] = &ui->lun_0;
			li->reqhead = li->reqtail = NULL;
			li->wqueued = NULL;
			ui->cur_tag = 1;
			for (j = 0; j < WD95_QDEPTH; j++) {
				wd95request_t *wreq = &ui->wreq[j];
				wreq->ctlr = adap;
				wreq->unit = i;
				wreq->lun = 0;
				wreq->cmd_tag = ui->cur_tag++;
				wreq->timeout_id = 0;
				wreq->next = ui->wrfree;
				ui->wrfree = wreq;
			}
			ui->wreq_cnt = WD95_QDEPTH;
			ui->wreq_alc[0] = WD95_QDEPTH;
		}

		/*
		 * Reset SCSI bus on adapters that the PROM doesn't touch.
		 * Also go to normal mode and enable interrupts.
		 */
		SET_WD_REG(sbp, WD95A_S_CTL, 0);
#if defined(EVEREST)
		if (adap > 7)
#endif
			wd95busreset(ci);
		SET_WD_REG(sbp, WD95A_N_ISRM, ISRM_INIT);

		ci->present = 1;
	}
	/*
	 * Probe for devices, and add found controllers to inventory.
	 * Delay 1000ms if we reset SCSI buses above.
	 */
	if (wd95cnt > 8) DELAY(1000000);
	for (adap = 0; adap < wd95cnt; adap++) {
		ci = &wd95dev[adap];
		if ((!ci) || (!ci->present))
			continue;


		ctlr_vhdl = wd95_ctlr_vhdl_get(adap);
		for (i = 0; i < WD95_LUPC; i++) {
			lun_vhdl = scsi_device_add(ctlr_vhdl,i,0);
			if (!wd95info(lun_vhdl))
				scsi_device_remove(ctlr_vhdl,i,0);

			if (ci->unit[i]->present && ((wd95_bus_flags[adap] & \
						 WD95BUS_NO_PROBE_LUN) == 0)) {
				u_char	lun;
				for (lun = 1; lun < WD95_MAXLUN; lun++)
				{
					lun_vhdl = scsi_device_add(ctlr_vhdl,i,lun);
				        if (!wd95info(lun_vhdl))
						scsi_device_remove(ctlr_vhdl,i,lun);
				}
			}
		}
		/* For the reference got from wd95_ctlr_vhdl_get() */
		hwgraph_vertex_unref(ctlr_vhdl);

		device_inventory_add(ctlr_vhdl,INV_DISK,INV_SCSICONTROL,
				     SCSI_EXT_CTLR(adap),
				     (ci->diff << 7) | ci->revision, 
				     INV_WD95A);

		printf("WD95A SCSI controller %d - ", WD_C_B(adap));
		if (ci->diff)
			printf("differential");
		else
			printf("single ended");
		if (scsi_is_external(adap))
			printf(" external");
		else
			printf(" internal");
		printf(", rev %d, min xfer period %dns\n", ci->revision,
		       ci->sync_period * 4);
	}

	wd95inited = 0;
	return 0;
}


/*
 * Called at system halt time because of lboot magic.
 *
 * For now, don't do anything.  Some future architectures may want to reset
 * the SCSI buses at this point.
 */
void
wd95halt()
{
#if 0
	wd95ctlrinfo_t	*ci;
	int		 adap;
	SPL_DECL(pri)

	for (adap = 0; adap < wd95cnt; adap++) {
		ci = &wd95dev[adap];
		if (ci->present) {
			IOLOCK(ci->lock, pri);
			wd95busreset(ci);
			wd95_fin_reset(ci);
			IOUNLOCK(ci->lock, pri);
		}
	}
#endif	/* 0 */
}


void
wd95resetstart(wd95ctlrinfo_t *ci)
{
	SPL_DECL(s)

	IOLOCK(ci->lock, s);
	ci->reset = 0;
	ci->active = 0;
#if WTIMEOUT
	printf("wd95_%d: starting, round robin is %d\n",
	       WD_C_B(ci->number), ci->round_robin);
#endif
	wd95start(ci, ci->unit[ci->round_robin]);
	IOUNLOCK(ci->lock, s);
}


void
wd95reset(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui, wd95request_t *wreq, char *rp)
{
	int cnt, unit;

	if (ci->reset)
		return;

	if (wreq != NULL) {
		if (wreq->timeout_id) {
			untimeout(wreq->timeout_id);
			wreq->timeout_id = 0;
			wreq->timeout_num++;
		}
	}

	ci->reset = 1;
	ci->reset_num++;
	if (ui != NULL) {
		unit = ui->number;


		if (unit != ci->host_id)
			ui->u_ca = 1;

		printf("wd95_%dd%d: %s - resetting SCSI bus\n",
		       WD_C_B(ci->number), unit, rp);

		wd95_cmdhist_dump(ui, wreq);
		if (wd95busreset(ci) == -1)
			printf("wd95_%dd%d: - check connections\n",WD_C_B(ci->number), unit);

	} else
		unit = ci->host_id;

	wd95_fin_reset(ci);
	/* wait 1 second for peripherals to do POR testing */
	itimeout(wd95resetstart, ci, HZ, PL_WD95);

	if (wreq != NULL)
		wd95complete(ci, ui, wreq, wreq->sreq);
	
	for (cnt = 0; cnt < WD95_LUPC; cnt++) {
		/* We've already cleared this unit if we called wd95complete. */
		if (wreq != NULL && cnt == unit)
			continue;
		ui = ci->unit[cnt];
		ui->u_ca = 1;
		ui->aenreq = 1;
		wd95clear(ci, ui, rp);
		ui->u_ca = 0;
		/* now if any new requests came in, requeue them */
		if (ui->auxhead)
			wd95requeue(ui);
		ui->active = 0;
	}
	ci->cur_target = ci->host_id;
}

/* Stub for abort message */
static int
wd95abort(scsi_request_t *wreq)
{
	wreq = wreq;
	return EINVAL;
}

void
wd95timeout(wd95request_t *wreq, int timeout_num)
{
	register wd95ctlrinfo_t	*ci;
	register wd95unitinfo_t	*ui;
	uint			ctlr, unit;
	SPL_DECL(pri)

	ctlr = wreq->ctlr;
	unit = wreq->unit;
	ci = &wd95dev[ctlr];
	ui = ci->unit[unit];
	IOLOCK(ci->lock, pri);
	/*
	 * We may have missed canceling the (threaded) timeout
	 * w/untimeout previously, so make one last sanity check
	 * before we proceed to avoid removing this same wreq again.
	 */
	if (wreq->timeout_num != timeout_num)
		goto end_timeout;
	wd95_cmdhist_dump(ui, wreq);
	if (ci->reset) {
		printf("wd95_%dd%d: timeout - SCSI bus reset already in progress\n",
		        WD_C_B(ctlr), unit);
		goto end_timeout;
	}
	if (wreq->reset_num != ci->reset_num) {
		printf("wd95_%dd%d: timeout - SCSI bus reset already done\n",
			WD_C_B(ctlr), unit);
		goto end_timeout;
	}

	wreq->timeout_id = 0;
	wreq->sreq->sr_status = SC_CMDTIME;

	wd95reset(ci, ui, wreq, "timeout");

end_timeout:
	IOUNLOCK(ci->lock, pri);
}

/* ARGSUSED */
static void
wd95reqsense_fini(struct scsi_request *req)
{
#ifdef DEBUG
	printf("wd95reqsense fini called\n");
#endif
}

/*
 * Issue a request sense to the controller from interrupt.
 */
static void
wd95reqsense(register wd95ctlrinfo_t *ci, register wd95unitinfo_t *ui,
			register scsi_request_t *req)
{
	u_char		lun = req->sr_lun;
	wd95luninfo_t	*li = ui->lun_info[lun];
	scsi_request_t	*sprq = li->u_sense_req;
	caddr_t	cp;

#if WDEBUG
	printf("wd95_%dd%d: check condition\n", WD_C_B(ci->number), ui->number);
#endif
	sprq->sr_spare = req;
	sprq->sr_buffer = li->tinfo.si_sense;

	cp = (caddr_t)sprq->sr_command;
	cp[0] = SCSI_REQ_SENSE;
	cp[1] = lun << 5;
	cp[2] = cp[3] = cp[5] = 0;
	cp[4] = sprq->sr_buflen = SCSI_SENSE_LEN;
	sprq->sr_flags = SRF_DIR_IN;
	sprq->sr_notify = wd95reqsense_fini;

	sprq->sr_scsi_status = sprq->sr_status = 0;

	if (cachewrback)
		dki_dcache_inval(li->tinfo.si_sense, SCSI_SENSE_LEN);

	if (ui->abortqueue) {
		ui->abortqueue = 0;
		printf("wd95_%dd%d: abort acknowledge\n",
				WD_C_B(ci->number), ui->number);
	}
	if ((sprq->sr_ha = li->reqhead) == NULL) {
		li->reqtail = sprq;
	}
	li->reqhead = sprq;
	li->l_ca = 1;
	if (li->ctq_drop_err)
		wd_clr_wq(ci, ui, li, "Clearing ctq");
	li->l_ca = 0;
	ui->sense = 1;

	wd95start(ci, ui);
}

static void
wd_rm_wq(register wd95unitinfo_t *ui, register wd95request_t *wreq)
{
	wd95luninfo_t	*li = ui->lun_info[wreq->lun];

	if (wreq->timeout_id) {
		untimeout(wreq->timeout_id);
		wreq->timeout_id = 0;
		wreq->timeout_num++;
	}
	if (li->wqueued == wreq) {
		li->wqueued = wreq->next;
	} else {
		wd95request_t *wdp = li->wqueued;
		while (wdp && wdp->next != wreq)
			wdp = wdp->next;
		ASSERT(wdp != NULL);
		wdp->next = wreq->next;
	}
	wreq->active = 0;
	wreq->next = ui->wrfree;
	ui->wrfree = wreq;
}

/*
 * Complete processing on a command.
 */
void
wd95complete(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui, wd95request_t *wreq,
			scsi_request_t *req)
{
	u_char lun = wreq->lun;
	wd95luninfo_t	*li = ui->lun_info[lun];
	ci->active = 0;
	ci->cur_target = ci->host_id;
#if WDEBUG
	if (wd95trace || !wreq->active) {
		wd95trace--;
		printf("wd95_%dd%d: ui 0x%x; wreq == 0x%x; req == 0x%x\n",
		        WD_C_B(ci->number), ui->number, ui, wreq, req);
	}
#endif
	req->sr_resid = wreq->resid;
	wd95_cmdhist_log(ui, req);
	wd_rm_wq(ui, wreq);
	li->wcurrent = NULL;

	ui->active = 0;
	ui->ext_msg = 0;

	if (req->sr_spare) { /* completion of req sense */
		scsi_request_t	*oreq;

		li->l_ca = 0;
		ui->sense = 0;
		oreq = req->sr_spare;
		dki_dcache_inval(li->tinfo.si_sense, SCSI_SENSE_LEN);
#if !WDEBUG
		if (oreq->sr_sense == NULL)
#endif
		{
			wd95sensepr(ci->number, ui->number,
				    li->tinfo.si_sense[2],
				    li->tinfo.si_sense[12],
				    li->tinfo.si_sense[13]);
			printf("\n");
		}

		/*
		 * If they don't want sense data, then just let them know that
		 *   there was a check condition.
		 * If we got an error on the request sense command, set
		 *   sr_sense_gotten to -1, and transfer the sr_status and
		 *   sr_scsi_status values, if appropriate.
		 * Else copy the sense data.
		 */
		if (oreq->sr_sense == NULL)
			oreq->sr_scsi_status = ST_CHECK;
		else if (req->sr_status || req->sr_scsi_status) {
			oreq->sr_sensegotten = -1;
			if (oreq->sr_status == 0)
				oreq->sr_status = req->sr_status;
			oreq->sr_scsi_status = req->sr_scsi_status;
		} else {
			oreq->sr_sensegotten = MIN(SCSI_SENSE_LEN - wreq->resid,
						   oreq->sr_senselen);
			bcopy(li->tinfo.si_sense, oreq->sr_sense,
			      oreq->sr_sensegotten);
			oreq->sr_scsi_status = ST_GOOD;
		}

		if (li->sense_callback)
			(*li->sense_callback)(oreq->sr_lun_vhdl, 
				      (char *)li->tinfo.si_sense);

		req = oreq;
	} else {
		if (req->sr_scsi_status == ST_CHECK &&
		    *req->sr_command != SCSI_REQ_SENSE) {
			wd95reqsense(ci, ui, req);
			return;
		}
	}

	if (req != NULL) {
		ASSERT((ulong) req < (ulong) ui || (ulong) req > (ulong) ui+1);

		if ((req->sr_flags & SRF_FLUSH) &&
		    (req->sr_flags & SRF_DIR_IN))
			dki_dcache_inval(req->sr_buffer, req->sr_buflen);

		/* copy data from spare align buffer */
		if (req->sr_ha_flags & WFLAG_ODDBYTEFIX) {
			int i, j;
			char *startaddr;

			i = io_poff(req->sr_buffer + req->sr_buflen);
			if (i > req->sr_buflen) {
				startaddr = (char *)&ci->page[io_poff(req->sr_buffer)];
				i = req->sr_buflen;
			}
			else
				startaddr = (char *)ci->page;

			j = i - req->sr_resid;
			if (j > 0) {
				dki_dcache_inval(startaddr, j);
				bcopy(startaddr,
				      &req->sr_buffer[req->sr_buflen - i], j);
			}
			ci->pagebusy = 0;
		}

		/* report completion on current command */
		wbflush();
		UNLOCK(ci->lock);
		(*req->sr_notify)(req);
		LOCK(ci->lock);

		/*
		 * If there was an error, then all additional commands
		 * oustanding should be aborted.
		 */
		if (ui->u_ca) {
			/* Clear the in-progress and wait queues */
			wd95clear(ci, ui, "error in other command");
			ui->u_ca = 0;
			/* now if any new requests came in, requeue them */
			if (ui->auxhead)
				wd95requeue(ui);
		}
	}

	/* Start up any waiting commands. */
	wd95start(ci, ui);
#if 0
	LOCK(ci->auxlock);
	ci->intr = 0;
	while (ci->auxhead) {
		req = ci->auxhead;
		if ((ci->auxhead = req->sr_ha) == NULL)
			ci->auxtail = NULL;
		ui = ci->unit[req->sr_target];
		req->sr_ha = NULL;

		if (ui->aenreq && !(ui->u_ca || ci->reset)) {
			if (req->sr_flags & SRF_AEN_ACK)
				ui->aenreq = 0;
			else {
				req->sr_status = SC_ATTN;
				printf("wd95: aborting, Q'd during intr\n");
				(*req->sr_notify)(req);
				continue;
			}
		}

		if (li->reqhead == NULL)
			li->reqhead = req;
		else
			li->reqtail->sr_ha = req;
		li->reqtail = req;
#if 0
		printf("wd95: starting during interrupt\n");
#endif
		wd95start(ci, ui);
	}
	UNLOCK(ci->auxlock);
#endif
}

static void
wd_clr_wq(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui, wd95luninfo_t *li, char *data)
{
	register scsi_request_t	*req;
	register wd95request_t	*wreq;

	while (wreq = li->wqueued) {
		req = wreq->sreq;
		if (req->sr_spare) {
		    printf("wd95_%dd%d: req sense 0x%x 0x%x aborted - %s\n",
			     WD_C_B(ci->number), ui->number, wreq, req, data);
		    req = req->sr_spare;
		    ui->sense = 0;
		} else {
#if WTIMEOUT
		    printf("wd95_%dd%d: request 0x%x 0x%x aborted - %s\n",
			     WD_C_B(ci->number), ui->number, wreq, req, data);
#endif
		    req->sr_resid = req->sr_buflen;
		}
		req->sr_scsi_status = 0;
		if (req->sr_status == 0) {
			req->sr_status = SC_ATTN;
		}
		wd_rm_wq(ui, wreq);

		if (req->sr_ha_flags & WFLAG_ODDBYTEFIX) {
			req->sr_ha_flags &= ~WFLAG_ODDBYTEFIX;
			ci->pagebusy = 0;
		}

		wbflush();
		UNLOCK(ci->lock);
		(*req->sr_notify)(req);
		LOCK(ci->lock);
	}
}

/*
 * This will be the case if a SCSI bus reset is issued, and not all
 * commands are rejected with error.
 */
void
wd95clear(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui, char *data)
{
	wd95luninfo_t		*li;
	register scsi_request_t	*req;
	register scsi_request_t	*nrq = NULL;
	u_char			lun;

	for (lun = 0; lun < WD95_MAXLUN; lun++) {
	    if ((li = ui->lun_info[lun]) == NULL)
		continue;
	    /* grab hold of the waiting queue for later */
	    if (li->reqhead) {
		li->reqtail->sr_ha = nrq;
		nrq = li->reqhead;
	    }
	    li->reqhead = li->reqtail = NULL;
	    wd_clr_wq(ci, ui, li, data);
	}

	/* Clear the wait queue */
	while (req = nrq) {
		li = ui->lun_info[req->sr_lun];
		if (req->sr_spare) {
			register scsi_request_t *oreq;
			oreq = req->sr_spare;
			oreq->sr_ha = req->sr_ha;
			req = oreq;
			ui->sense = 0;
		}
		req->sr_status = SC_ATTN;
#if WTIMEOUT
		printf("wd95_%dd%d: queued request cancelled by %s\n",
		          WD_C_B(ci->number), ui->number, data);
#endif
		nrq = req->sr_ha;
		wbflush();
		UNLOCK(ci->lock);
		(*req->sr_notify)(req);
		LOCK(ci->lock);
	}
}

#ifdef EVEREST
#define WD95_DMA_LIMIT 1
#else
#define WD95_DMA_LIMIT 3
#endif

void
wd95setdma(register wd95ctlrinfo_t *ci, wd95request_t *wreq)
{
	register volatile char	*ibp = ci->ha_addr;
	register scsi_request_t	*req = wreq->sreq;
	register wd95unitinfo_t	*ui = ci->unit[wreq->unit];
	wd95luninfo_t		*li = ui->lun_info[wreq->lun];
	int			flags = (int)wreq->flags;
	ulong		 	startaddr = 0;

	li->wcurrent = wreq;
	if (req->sr_buflen && !((req->sr_buflen - wreq->resid) & WD95_DMA_LIMIT)) {
	    uint	 tc;
	    uint	 wd95war_count;
	    dmamap_t	*dmamap = ci->d_map;

	    /*
	     * If SRF_MAPBP, sr_buffer must be zero; only bottom 12 bits of
	     * startaddr are valid and useable.
	     */
	    startaddr = (ulong) req->sr_buffer + req->sr_buflen - wreq->resid;

	    if (req->sr_flags & SRF_MAPBP)  
		tc = dma_mapbp(dmamap, req->sr_bp, wreq->resid);
#if defined(EVEREST)
	    else if (req->sr_flags & SRF_MAPUSER)
		tc = wreq->resid;
	    else if (req->sr_ha_flags & WFLAG_ODDBYTEFIX)
		tc = dma_map2(dmamap, (caddr_t) startaddr, (caddr_t) ci->page,
		              wreq->resid);
#endif
	    else
		tc = dma_map(dmamap, (caddr_t) startaddr, wreq->resid);

#if !defined(EVEREST)
	    if (req->sr_flags & SRF_FLUSH) {
		if (!(req->sr_flags & SRF_DIR_IN))
			dki_dcache_wb((void *)startaddr, tc);
		else if (cachewrback)
			dki_dcache_inval((void *)startaddr, tc);
	    }
#endif
#if defined(EVEREST)
	    /* Non-Everest will have only rev H or better */
	    /*
	     * On revision 1 wd95 chips (rev E or G), there is a nasty wd95 bug
	     * in async transfers in, where the transfer count is not
	     * decremented when the chip FIFO fills up.  We work around this by
	     * making sure that we don't program the wd95 for more bytes than
	     * we will be able to give it, without a delay.  With the S1/wd95,
	     * that is two cache lines, or 0x100 - offset in cacheline bytes.
	     * Beyond that, an IBUS access will be needed.
	     */
	    wd95war_count = 0x100 - (startaddr & 0x7F);
	    if ((ci->revision == 1) &&
		((ui->sync_off & 0x1F) == 0) &&
		(req->sr_flags & SRF_DIR_IN) &&
		(wreq->resid > wd95war_count))
	    {
		tc = wreq->tlen = wd95war_count;
		SET_WD_REG(ibp, WD95A_N_TC_0_7, (tc & 0xff));
		if (tc > 0xff)
		    SET_WD_REG(ibp, WD95A_N_TC_8_15, ((tc >> 8) & 0xff));
		SET_WD_REG(ibp, WD95A_N_CTL, W95_CTL_LDTC);
		SET_WD_REG(ibp, WD95A_N_ISRM, 0x7F);
	    }
	    else
#endif
	    {
		ASSERT(tc == wreq->resid);
		if ((ui->sync_off & 0x1F) && (tc & 1))
			tc++;
		wreq->tlen = tc;
		SET_WD_REG(ibp, WD95A_N_TC_0_7, (tc & 0xff));
		if (tc > 0xff) {
		    SET_WD_REG(ibp, WD95A_N_TC_8_15, ((tc >> 8) & 0xff));
		    if (tc > 0xffff)
			SET_WD_REG(ibp, WD95A_N_TC_16_23, ((tc >> 16) & 0xff));
		}
		SET_WD_REG(ibp, WD95A_N_CTL, W95_CTL_LDTCL);
		SET_WD_REG(ibp, WD95A_N_ISRM, 0x6F);
	    }
	    flags |= WCS_I_DATA_OK |
		     ((req->sr_flags & SRF_DIR_IN) ? WCS_I_DIR : 0);
#if defined(EVEREST)
	    if (req->sr_flags & SRF_MAPUSER)
		wd95_usergo(dmamap, req->sr_bp, (caddr_t)startaddr,
			    req->sr_flags & SRF_DIR_IN);
	    else
#endif
	    wd93_go(dmamap, (caddr_t) startaddr, req->sr_flags & SRF_DIR_IN);
	}
	if (startaddr & 1)
	    flags &= ~WCS_I_DATA_OK;
	wdcprintf(("flags 0x%x\n", flags));
	SET_WD_REG(ibp, WD95A_N_FLAG, flags);
}

void
wd95rsel(register wd95ctlrinfo_t *ci)
{
	register volatile char	*ibp = ci->ha_addr;
	register wd95unitinfo_t	*ui = NULL;
	wd95luninfo_t		*li = NULL;
	wd95request_t		*wreq = NULL;
	int			flags, target;

	if (ci->active) {
		wd95reset(ci, ui, wreq, "reselect when active");
		return;
	}

	target = GET_WD_REG(ibp, WD95A_N_SRCID);
	if ( !(target & W95_SRCID_SIV) ) {
		wd95reset(ci, ui, wreq, "reselect not valid");
		return;
	}
	ci->active = 1;

	ci->cur_target = target & W95_SRCID_M;
	ui = ci->unit[ci->cur_target];
	SET_WD_REG(ibp, WD95A_N_SPW, ui->sync_per);
	SET_WD_REG(ibp, WD95A_N_STC, ui->sync_off);

	ui->cur_lun = GET_WD_REG(ibp, W95_DPR_R(WCS_I_ID_IN)) & (WD95_MAXLUN-1);
	li = ui->lun_info[ui->cur_lun];
	ASSERT(li);

	if (ci->revision == 1) {
		/*
		 * Workaround for WD95 bug - need to issue KILL to chip after
		 * ID message in after reselection, else chip's internal state
		 * will be wrong and self inconsistent regarding number of
		 * rising REQ edges seen.
		 */
		flags = 0;
		SET_WD_REG(ibp, WD95A_N_CTL, W95_CTL_KILL);
		while (!(GET_WD_REG(ibp, WD95A_N_STOPU) & W95_STPU_ABORTI))
			if(flags++ == 100) {
				wd95reset(ci, ui, wreq,
					  "reselection failure\n");
				return;
			}
		SET_WD_REG(ibp, WD95A_N_STOPU, 1);
	}

	if (li->wqueued == NULL) {
		wd95reset(ci, ui, wreq, "unexpected reselect");
		return;
	}

	if (wreq = get_wqueued(li, 0)) {
		wd95setdma(ci, wreq);
		if (wreq->resid != wreq->save_resid) {
			SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
			flags = GET_WD_REG(ibp, WD95A_N_FLAG);
			flags = flags & ~WCS_I_DATA_OK;
			SET_WD_REG(ibp, WD95A_N_FLAG, flags);
		}
	}

	SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
}

void
wd95_backout(register wd95ctlrinfo_t *ci, register wd95unitinfo_t *ui,
		wd95request_t *wreq, register scsi_request_t *req)
{
	register volatile char *ibp = ci->ha_addr;
	wd95luninfo_t	*li = ui->lun_info[req->sr_lun];

	/* back out current request */

	scsidma_flush(ci->d_map, req->sr_flags & SRF_DIR_IN);
	if (req->sr_ha_flags & WFLAG_ODDBYTEFIX) {
		req->sr_ha_flags &= ~WFLAG_ODDBYTEFIX;
		ci->pagebusy = 0;
	}
	if ( !(req->sr_ha = li->reqhead) )
		li->reqtail = req;
	li->reqhead = req;
	wd_rm_wq(ui, wreq);
	li->wcurrent = NULL;
	li->l_ca = 0;
	ci->cur_target = ci->host_id;
	ci->active = 0;
	ui->active = 0;
	ui->ext_msg = 0;
	SET_WD_REG(ibp, WD95A_N_TC_0_7, 0);
	SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
}

void
wd95_proc_disc(register wd95ctlrinfo_t *ci, register wd95unitinfo_t *ui,
	wd95request_t *wreq, int rtc)
{
	register scsi_request_t *req = wreq->sreq;
	wd95luninfo_t *li = ui->lun_info[wreq->lun];
	volatile char *ibp = ci->ha_addr;
	
	ASSERT(ci->active);
	{
		if (scsidma_flush(ci->d_map, req->sr_flags & SRF_DIR_IN))
			req->sr_status = SC_HARDERR;
		wreq->starting = 0;
		if (wreq->tlen) {
		    wreq->resid -= wreq->tlen - rtc;
		    wreq->tlen = 0;
		}
	}
	ui->ext_msg = 0;
	ci->active = 0;
	ci->cur_target = ci->host_id;
	li->wcurrent = NULL;
	SET_WD_REG(ibp, WD95A_N_SPW, SPW_SLOW);
	SET_WD_REG(ibp, WD95A_N_STC, 0);
	/* clear out the tc pipe, lowbyte write clears upper */
	SET_WD_REG(ibp, WD95A_N_TC_0_7, 0);
	SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
	SET_WD_REG(ibp, WD95A_N_RESPONSE, WD95_RESEL_EN);
	wd95start(ci, ui);
}


void
wd95finish(register wd95ctlrinfo_t *ci, register wd95unitinfo_t *ui,
	wd95request_t *wreq, int rtc)
{
	register volatile char *ibp = ci->ha_addr;
	register struct scsi_request *req = wreq->sreq;
	char	status;

	if (scsidma_flush(ci->d_map, req->sr_flags & SRF_DIR_IN))
	    req->sr_status = SC_HARDERR;
	status = GET_WD_REG(ibp, W95_DPR_R(WCS_I_STATUS));
	wdcprintf((" fini, stat 0x%x\n", status));
	if (req->sr_status == 0)
	    req->sr_scsi_status = status;
	if (wreq->tlen) {
	    wreq->resid -= wreq->tlen - rtc;
	    wreq->tlen = 0;
	    if (wreq->resid < 0)
		req->sr_status = SC_HARDERR;
	}
	SET_WD_REG(ibp, WD95A_N_TC_0_7, 0);
	SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
	SET_WD_REG(ibp, WD95A_N_RESPONSE, WD95_RESEL_EN);
	wd95complete(ci, ui, wreq, req);
}


#define	MSG_DISC	0x04			/* disconnect msg image */
#define	MSG_SAVE_DP	0x02			/* save data ptr image */
#define	MSG_RESTORE_DP	0x03			/* restore data ptr image */
#define	MSG_REJECT	0x07			/* reject msg */
#define	MSG_IGN_WD_RES	0x23			/* ignore wide residue */

void
wd95intr(int ctlr)
{
	register wd95ctlrinfo_t	*ci = &wd95dev[ctlr];
	register wd95unitinfo_t	*ui;
	register wd95luninfo_t	*li;
	register wd95request_t	*wreq;
	register scsi_request_t	*req;
	volatile char		*ibp = ci->ha_addr;
	int			rtc;
	int			cstat, sqaddr = 0;
	int			msg_type;

	LOCK(ci->lock);

#if 0
	LOCK(ci->auxlock);
	ci->intr = 1;
	UNLOCK(ci->auxlock);
#endif

	SET_WD_REG(ibp, WD95A_S_CTL, 0);
	while ((cstat = GET_WD_REG(ibp, WD95A_N_ISR)) & W95_ISR_INT0) {
	    wdiprintf(("wd95%dd%d: isr 0x%x", WD_C_B(ctlr), ci->cur_target, cstat));
	    /*
	     * Now determine which target (or unit)
	     * it is associated with.
	     */
	    if (ci->cur_target != ci->host_id) {
		    ui = ci->unit[ci->cur_target];
		    li = ui->lun_info[ui->cur_lun];
		    if (wreq = li->wcurrent)
		      req = wreq->sreq;
	    } else {
		    ui = NULL;
		    wreq = NULL;
		    req = NULL;
		    wdiprintf(("rec "));
	    }

	    rtc = GET_WD_REG(ibp, WD95A_N_TC_16_23);
	    rtc = (rtc << 8) + GET_WD_REG(ibp, WD95A_N_TC_8_15);
	    rtc = (rtc << 8) + GET_WD_REG(ibp, WD95A_N_TC_0_7);
	    wdiprintf((" rtc 0x%x", rtc));

	    if (cstat & W95_ISR_SREJ) {
		int uei = GET_WD_REG(ibp, WD95A_N_UEI);
		int request_cancelled = 0;

		sqaddr = GET_WD_REG(ibp, WD95A_N_SQADR);
		if (wreq && wreq->starting) {
		    wd95_backout(ci, ui, wreq, req);
		    wreq = NULL;  req = NULL;
		    request_cancelled = 1;
		}
		SET_WD_REG(ibp, WD95A_N_TC_0_7, 0);
		SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
		SET_WD_REG(ibp, WD95A_N_ISR, W95_ISR_SREJ);
		if ((sqaddr == WCS_I_RSLCONT) || (uei & W95_UEI_URSEL) ||
		    (sqaddr == 0x32) || (sqaddr == 0x4d) ||
		    (sqaddr == WCS_I_DISC) || (sqaddr == WCS_I_COMPLETE)) {
		    if (request_cancelled)
			continue;
		    else
			printf("wd95_%d: SREJ - reselected when not starting"
				" a command\n", WD_C_B(ctlr), wreq);
		}
		else
		    printf("wd95_%d: SREJ with sqaddr at 0x%x -- expected"
			    " reselect\n", WD_C_B(ctlr), sqaddr);
		if (wreq != NULL) {
			wreq->sreq->sr_status = SC_HARDERR;
		}
		wd95reset(ci, ui, wreq, "unexpected SREJ");
		goto intr_endloop;
	    }
	    if (cstat & W95_ISR_BUSYI) {
		int	tc;

		ASSERT(rtc == 0);
		SET_WD_REG(ibp, WD95A_N_ISR, W95_ISR_BUSYI);
#if defined(EVEREST)
		/*
		 * Only a bug with chips prior to rev H.
		 * If the resid != this transfer len, that means that we only
		 * did a partial (to workaround a nasty wd95 bug).  Do more
		 * partials until we only have 0x100 bytes or fewer left.
		 * 0x100 is the magic number that the S1 and wd95 can buffer
		 * up before the wd95 gets a TC overrun, and the S1 needs the
		 * Ibus to offload a cacheline.
		 */
		if (wreq->resid != wreq->tlen)
		{
		    wreq->resid -= wreq->tlen;
		    if (wreq->resid > 0x100) {
			tc = wreq->tlen = 0x100;
			SET_WD_REG(ibp, WD95A_N_TC_0_7, (tc & 0xff));
			SET_WD_REG(ibp, WD95A_N_TC_8_15, ((tc >> 8) & 0xff));
			SET_WD_REG(ibp, WD95A_N_CTL, W95_CTL_LDTC);
		    }
		    else
		    {
			tc = wreq->resid;
			if ((ui->sync_off & 0x1F) && (tc & 1))
				tc++;
			wreq->tlen = tc;
			SET_WD_REG(ibp, WD95A_N_TC_0_7, (tc & 0xff));
			if (tc > 0xff)
			    SET_WD_REG(ibp, WD95A_N_TC_8_15, ((tc >> 8) & 0xff));
			SET_WD_REG(ibp, WD95A_N_CTL, W95_CTL_LDTCL);
			SET_WD_REG(ibp, WD95A_N_ISRM, 0x6F);
		    }
		}
#endif /* defined(EVEREST) */
	    }
	    if (cstat & W95_ISR_VBUSYI) {
		ASSERT(0);
		SET_WD_REG(ibp, WD95A_N_ISR, W95_ISR_VBUSYI);
	    }
	    if (cstat & (W95_ISR_STOPU | W95_ISR_UEI)) {
		int s_status, e_status;

		s_status = GET_WD_REG(ibp, WD95A_N_STOPU);
		e_status = GET_WD_REG(ibp, WD95A_N_UEI);

		if (e_status & W95_UEI_RSTINTL) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_RSTINTL);
			printf("wd95_%d:  WD95 saw SCSI reset\n",
				WD_C_B(ctlr));
			wd95reset(ci, NULL, NULL, "from bus");
			goto intr_endloop;
		}

		wdiprintf((" stpu 0x%x, uei 0x%x", s_status, e_status));
		if (s_status & W95_STPU_SCSIT) {
			SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
			SET_WD_REG(ibp, WD95A_N_STOPU, W95_STPU_SCSIT);
			if (req) {
			    req->sr_status = SC_TIMEOUT;
			    scsidma_flush(ci->d_map, req->sr_flags & SRF_DIR_IN);
#if !WDEBUG
			    if (wd95inited == 0)
#endif
				printf("wd95_%dd%d: does not respond"
				       " (selection timeout)\n",
				       WD_C_B(ci->number), ui->number);
			    wd95complete(ci, ui, wreq, req);
			}
		}

		if (e_status & W95_UEI_TCOVR) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_TCOVR);
			SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
		}

		/*
		 * Extraneous bits that we don't know what to do with yet.
		 */
		if (e_status & W95_UEI_UDISC) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_UDISC);
			sqaddr = GET_WD_REG(ibp, WD95A_N_SQADR);
			switch (sqaddr) {
			    case 0x32:
                                if (wreq) wd95_proc_disc(ci, ui, wreq, rtc);
                                else {
                                       printf("wd95_%dd%d: bogus disconnect, sqaddr 0x%x,"
                                       " check cabling, termination, and term power\n",
                                       WD_C_B(ctlr), ci->cur_target, sqaddr);
				       wd95reset(ci, ui, wreq, "bogus disconnect");
				       goto intr_endloop;
                                }
				break;

			    case 0x4d:
                                if (wreq) wd95finish(ci, ui, wreq, rtc);
                                else {
                                       printf("wd95_%dd%d: bogus disconnect, sqaddr 0x%x,"
                                       " check cabling, termination, and term power\n",
                                       WD_C_B(ctlr), ci->cur_target, sqaddr);
				       wd95reset(ci, ui, wreq, "bogus disconnect");
				       goto intr_endloop;
                                }
				break;	

			    default:
				printf("wd95_%dd%d: unexpected disconnect, sqaddr 0x%x,"
				        " check cabling, termination, and term power\n",
				        WD_C_B(ctlr), ci->cur_target, sqaddr);
				if (wreq != NULL) {
					wreq->sreq->sr_status = SC_HARDERR;
				}
				wd95reset(ci, ui, wreq, "unexpected disconnect");
				goto intr_endloop;
			}
		}
		if (s_status & W95_STPU_ABORTI) {
			SET_WD_REG(ibp, WD95A_N_STOPU, W95_STPU_ABORTI);
			printf("wd95_%dd%d: ABORTI error\n", WD_C_B(ctlr));
		}
		if (s_status & W95_STPU_PARE) {
			SET_WD_REG(ibp, WD95A_N_STOPU, W95_STPU_PARE);
			if (wreq != NULL)
				wreq->sreq->sr_status = SC_PARITY;
			wd95reset(ci, ui, wreq, "parity error");
			goto intr_endloop;
		}
		if (s_status & W95_STPU_LRCE) {
			SET_WD_REG(ibp, WD95A_N_STOPU, W95_STPU_LRCE);
			printf("wd95_%d: LRC error -- data corrupted\n",
				WD_C_B(ctlr));
		}
		if (s_status & W95_STPU_TCUND) {
			SET_WD_REG(ibp, WD95A_N_STOPU, W95_STPU_TCUND);
			printf("wd95_%d: TCUND error\n", WD_C_B(ctlr));
		}
		if (s_status & W95_STPU_SOE) {
			SET_WD_REG(ibp, WD95A_N_STOPU, W95_STPU_SOE);
			if (wreq != NULL) 
				wreq->sreq->sr_status = SC_HARDERR;
			wd95reset(ci, ui, wreq, "SCSI offset error");
			goto intr_endloop;
		}
		if (e_status & W95_UEI_FIFOE) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_FIFOE);
			printf("wd95_%d: FIFOE error\n", WD_C_B(ctlr));
		}
		if (e_status & W95_UEI_ATNI) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_ATNI);
			printf("wd95_%d: ATNI error\n", WD_C_B(ctlr));
		}
		if (e_status & W95_UEI_USEL) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_USEL);
			printf("wd95_%d: Error - WD95 has been selected\n",
				WD_C_B(ctlr));
		}
	    }
	    if (cstat & (W95_ISR_STOPWCS | W95_ISR_INTWCS)) {

		SET_WD_REG(ibp, WD95A_N_ISR, W95_ISR_STOPWCS | W95_ISR_INTWCS);

		sqaddr = GET_WD_REG(ibp, WD95A_N_SQADR);
		wdiprintf((" sqaddr 0x%x", sqaddr));
		switch (sqaddr) {
		case WCS_I_COMPLETE:
		    wd95finish(ci, ui, wreq, rtc);
		    break;

		case WCS_I_DISC:
		    wd95_proc_disc(ci, ui, wreq, rtc);
		    break;

		case WCS_I_RSLCONT:
		    wd95rsel(ci);
		    break;

		case WCS_I_DATA_NOT_OK:
		    scsidma_flush(ci->d_map, req->sr_flags&SRF_DIR_IN);
		    SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
		    wreq->resid = wreq->save_resid;
		    wd95setdma(ci, wreq);
		    {
		      char flags = GET_WD_REG(ibp, WD95A_N_FLAG);
		      if (flags & WCS_I_DATA_OK) {
			SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
		      } else {
			wd95reset(ci, ui, wreq, "Odd byte address attempt\n");
		      }
		    }
		    break;
		    
		case WCS_I_UNK_MSG:
		    {
		    int nsq;
		    char msg_in, flags;

		    msg_in = GET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_IN));
		    nsq = WCS_I_DISPATCH;
		    switch (msg_in) {
			case MSG_SAVE_DP:
			    wreq->resid -= (wreq->tlen - rtc);
			    wreq->save_resid = wreq->resid;
			    scsidma_flush(ci->d_map, req->sr_flags&SRF_DIR_IN);
			    SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
			    flags = GET_WD_REG(ibp, WD95A_N_FLAG);
			    flags = flags & ~WCS_I_DATA_OK;
			    SET_WD_REG(ibp, WD95A_N_FLAG, flags);
			    break;

			case MSG_RESTORE_DP:
			    wreq->resid = wreq->save_resid;
			    SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
			    flags = GET_WD_REG(ibp, WD95A_N_FLAG);
			    flags = flags & ~WCS_I_DATA_OK;
			    SET_WD_REG(ibp, WD95A_N_FLAG, flags);
			    break;

			case MSG_REJECT:
			    nsq = WCS_I_ITL_NEXUS;
			    if (ui->ext_msg) {
				    if (ui->wide_req) {
					    ui->wide_req = 0;
					    ui->wide_en = 0;
				    }
				    if (ui->sync_req) {
					    ui->sync_req = 0;
					    ui->sync_en = 0;
				    }
				    ui->ext_msg = 0;
			    }
			    break;
		    }
		    SET_WD_REG(ibp, WD95A_N_SQADR, nsq);
		    }
		    break;

		case WCS_I_MSG_CTQ:
		    {
		      int msg, tag_num;

		      msg = GET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_IN));
		      tag_num = GET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_IN+1));
		      switch (msg) {
		       case SC_TAG_SIMPLE:
		       case SC_TAG_ORDERED:
		       case SC_TAG_HEAD:
			if (li->wcurrent) {
			  SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
			  if (scsidma_flush(ci->d_map, req->sr_flags & SRF_DIR_IN))
				req->sr_status = SC_HARDERR;
			}
			/* now find the proper wreq for this one... */
			if (wreq = get_wqueued(li, tag_num)) {
			  wd95setdma(ci, wreq);
			  if (wreq->resid != wreq->save_resid) {
			    int flags;
			    SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
			    flags = GET_WD_REG(ibp, WD95A_N_FLAG);
			    flags = flags & ~WCS_I_DATA_OK;
			    SET_WD_REG(ibp, WD95A_N_FLAG, flags);
			  }
			}
			break;

		       case MSG_IGN_WD_RES:
			wreq->resid += tag_num;
			break;
		      }
		      SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
		    }
		    break;

#if 0	/* XXX 0 length is problem */
		case WCS_I_MSGI_0:
#endif
		case WCS_I_MSGI_1:
		case WCS_I_MSGI_2:
		case WCS_I_MSGI_3:
		case WCS_I_MSGI_4:
		case WCS_I_MSGI_5:
		    ui->ext_msg = 0;
		    msg_type = GET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_IN+2));
		    switch (msg_type) {

		    case MSG_SDTR:
			{
			uint offset, period;
			/*
			 * This is hard coded for a 40mhz clock.  A 50 mhz
			 * clock will require more than changing constants.
			 */
			period = GET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_IN+3));
			ui->sync_per = SPW(((period * 2) - 26) / 25);
			offset = GET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_IN+4));
			ASSERT(offset <= ci->sync_offset);
			ui->sync_off = offset | (ui->sync_off & W95_STC_WSCSI);
			ui->sync_req = 0;
			if (offset == 0)
			    ui->sync_en = 0;
#ifdef DEBUG
			if (showconfig) {
			    printf("wd95_%dd%d: sync per %d, off %d, SPW %x\n",
				    WD_C_B(ctlr), ci->cur_target, period,
				    offset, ui->sync_per);
			}
#endif
			SET_WD_REG(ibp, WD95A_N_SPW, ui->sync_per);
			SET_WD_REG(ibp, WD95A_N_STC, ui->sync_off);
			SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
			}
			break;

		    case MSG_WIDE:
			{
			int	width;
			width = GET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_IN+3));
			if (width == MSG_WIDE_16) {
			    ui->sync_off |= W95_STC_WSCSI;
			    SET_WD_REG(ibp, WD95A_N_STC, ui->sync_off);
			}
			else
			    ui->wide_en = 0;
			ui->wide_req = 0;

			if (showconfig) {
			    printf("wd95_%dd%d: %s\n", WD_C_B(ctlr),
				    ci->cur_target,
				    width == MSG_WIDE_16 ? "wide" : "narrow");
			}
			if (ui->sync_req) {
			    SET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_OUT_L), 5);
			    SET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_O_LST-4), MSG_EXT);
			    SET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_O_LST-3), MSG_SDTR_LEN);
			    SET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_O_LST-2), MSG_SDTR);
			    SET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_O_LST-1), ci->sync_period);
			    SET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_O_LST-0),
				(ui->wide_en ? (ci->sync_offset/2) :
						 ci->sync_offset));
			    ui->ext_msg = 1;
			    SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_ATTENTION);
			} else {
			    SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
			}
			}
			break;

		    default:
			printf("unknown ext msg type 0x%x\n", msg_type);
			break;
		    } /* switch (msg_type) */
		    break;

		case WCS_I_SEL_E_NM:
		    ui->ext_msg = 0;
		    /* fall thru */

		case WCS_I_SEL_NOMSG:
		    /* we went right to some other phase */
		    ui->wide_en = ui->sync_en = ui->disc = 0;
		    ui->sync_req = ui->wide_req = 0;
		    ui->sel_tar = 0x7d;
		    SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_NATN);
		    break;
		    
		default:
		    printf("wd95_%dd%d: unknown wcs loc, sqaddr 0x%x\n",
			    WD_C_B(ctlr), ci->cur_target, sqaddr);
		    break;
		}
	    }
	    if (cstat & W95_ISR_UEI) {
		int e_status;

		e_status = GET_WD_REG(ibp, WD95A_N_UEI);

		if (e_status & W95_UEI_URSEL) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_URSEL);
			if (!(GET_WD_REG(ibp, WD95A_N_SR) & W95_SR_ACTIVE)) {
				SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_NRESEL);
			}
		}
		if (e_status & W95_UEI_UPHAS) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_UPHAS);
			sqaddr = GET_WD_REG(ibp, WD95A_N_SQADR);
			wdiprintf((" sqaddr 0x%x", sqaddr));
			wdiprintf((" fifos 0x%x", GET_WD_REG(ibp,WD95A_N_FIFOS)));
			switch (sqaddr) {
			    case WCS_I_RSLCONT:
#ifdef DEBUG
				printf("wd95_%d: UPHAS -- reselected\n",
					WD_C_B(ctlr));
#endif
				wd95rsel(ci);
				break;

			    case WCS_I_DATA:
			    {
				/*
				 * We come here if data fifo hasn't counted to
				 * 0, but the target has changed phase (will
				 * happen if not as many bytes transferred as
				 * allowed for).  This will also occur if there
				 * is an odd byte count.  We read the fifo
				 * status register.  If it reads one byte, then
				 * we pull the byte out of the fifos register
				 * and put it in the right spot.  Otherwise,
				 * we will read tc remainder at disconnect or
				 * command complete.
				 */
				u_char	fifo;
				ulong	loc;

				ASSERT(req != NULL);
				fifo = GET_WD_REG(ibp, WD95A_N_FIFOS) & 0x1F;
				if (fifo == 1)
				{
				    if ((req->sr_flags & SRF_MAPBP) ||
					(req->sr_flags & SRF_MAPUSER))
				    {
					req->sr_status = SC_HARDERR;
				    }
				    else
				    {
					fifo = GET_WD_REG(ibp, WD95A_N_DATA);
					loc = (req->sr_buflen - wreq->resid) +
					      wreq->tlen - rtc;
					if ((req->sr_ha_flags & WFLAG_ODDBYTEFIX) &&
				    (io_poff(req->sr_buffer) + loc >=
				     io_ctob(io_btoct(io_poff(req->sr_buffer) + req->sr_buflen))))
					{
						loc = io_poff(req->sr_buffer + loc);
						ci->page[loc] = fifo;
#ifdef IP22
						dki_dcache_wb(&ci->page[loc], 1);
#endif
					}
					else
					{
						req->sr_buffer[loc] = fifo;
#ifdef IP22
						dki_dcache_wb(&req->sr_buffer[loc], 1);
#endif
					}
					wreq->resid -= 1 + wreq->tlen - rtc;
					wreq->tlen = 0;
					SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
				    }
				}
#if WDEBUG
				SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
				break;
#endif
				/* fall through if not WDEBUG */
			    }

			    default:
#if WDEBUG
				/*
				 * On sync neg msg reject, we come through here.
				 * Also after restarting from msg reject.
				 */
				printf("wd95_%d: unexpected phase at sqaddr "
				        "0x%x, possible message reject on sync"
					" negotiation\n", WD_C_B(ctlr), sqaddr);
#endif
				SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
			}
		}
	    }
intr_endloop:
	    wdiprintf(("\n"));
	} /* while we have interrupts pending */
	wd95start(ci, ci->unit[ci->round_robin]);

	wdiprintf(("	leaving wd95intr\n"));
	wbflush();
	UNLOCK(ci->lock);
}


#define	MSG_IDENT	0x80		/* identify message value */
#define	MSG_DISC_PRIV	0x40		/* disconnect priv bit */

static void
wd95a_do_cmd(register wd95unitinfo_t *ui, register wd95request_t *wreq)
{
	register wd95ctlrinfo_t *ci = ui->ctrl_info;
	register volatile char *sbp = ci->ha_addr;
	register scsi_request_t	 *sp = wreq->sreq;
	int	len = sp->sr_cmdlen;
	int	dpr = WCS_I_CMD_BLK;
	u_char	*cp = sp->sr_command;
	int	flags, msg, sqaddr, cmd;

	cmd = *cp;
	wdcprintf((" req == 0x%x sr_spare == 0x%x cmd 0x%x", sp, sp->sr_spare, cmd));

	wreq->starting = 1;
	/* setup the command in the Dual port registers and clean out tc */
	SET_WD_REG(sbp, WD95A_S_CTL, W95_TC_CLR);
	wdxprintf(("isr 0x%x sr 0x%x ", GET_WD_REG(sbp, WD95A_N_ISR),
		   GET_WD_REG(sbp, WD95A_N_SR)));

	/*
	 * If it's a vendor unique command, we have to set the appropriate
	 * length in the CDBSIZ registers.  Since lengths vary, we have to
	 * do it on a per-command basis.
	 */
        switch (*cp & 0xE0) {
        case 0x60:
            SET_WD_REG(sbp, WD95A_N_CTL, 1);
            SET_WD_REG(sbp, WD95A_S_CDBSIZ1, (len - 1));
            SET_WD_REG(sbp, WD95A_N_CTL, 0);
            break;
        case 0x80:
            SET_WD_REG(sbp, WD95A_N_CTL, 1);
            SET_WD_REG(sbp, WD95A_S_CDBSIZ1, (len - 1) << 4);
            SET_WD_REG(sbp, WD95A_N_CTL, 0);
            break;
        case 0xC0:
            SET_WD_REG(sbp, WD95A_N_CTL, 1);
            SET_WD_REG(sbp, WD95A_S_CDBSIZ2, (len - 1));
            SET_WD_REG(sbp, WD95A_N_CTL, 0);
            break;
        case 0xE0:
            SET_WD_REG(sbp, WD95A_N_CTL, 1);
            SET_WD_REG(sbp, WD95A_S_CDBSIZ2, (len - 1) << 4);
            SET_WD_REG(sbp, WD95A_N_CTL, 0);
            break;
        }

	while (len-- > 0) {
	    SET_WD_REG(sbp, W95_DPR_R(dpr++), (int)(*cp++));
	}
	flags = 0;

	msg = MSG_IDENT | (sp->sr_lun & 0x7);
	if (ui->disc) {
	    msg |= MSG_DISC_PRIV;
	}
	SET_WD_REG(sbp, W95_DPR_R(WCS_I_ID_OUT), msg);

	SET_WD_REG(sbp, WD95A_N_SPW, ui->sync_per);
	SET_WD_REG(sbp, WD95A_N_STC, ui->sync_off);
	sqaddr = ui->sel_tar;
	if (ui->wide_en) {
	    if (ui->wide_req && ! ui->ext_msg) {
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT_L), 4);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-3), MSG_EXT);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-2), 2);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-1), MSG_WIDE);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-0), MSG_WIDE_16);
		ui->ext_msg = 1;
		flags |= WCS_I_MULTI_MSG;
		sqaddr = WCS_I_SEL;
	    }
	}
	if (ui->sync_en) {
	    if (ui->sync_req && ! ui->ext_msg) {
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT_L), 5);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-4), MSG_EXT);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-3), MSG_SDTR_LEN);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-2), MSG_SDTR);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-1), ci->sync_period);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-0),
			 (ui->wide_en ? (ci->sync_offset/2) : ci->sync_offset));
		ui->ext_msg = 1;
		flags |= WCS_I_MULTI_MSG;
		sqaddr = WCS_I_SEL;
	    }
	}
	/* clear CTQ if REQ_SENSE */
	if ((cmd == SCSI_REQ_SENSE) || (cmd == SCSI_INQ_CMD)) {
	    wreq->ctq = 0;
	}
	if (wreq->ctq) {
	    if ( !ui->ext_msg ) {
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT_L), 2);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-1), sp->sr_tag);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_O_LST-0), wreq->cmd_tag);
		flags |= WCS_I_MULTI_MSG;
	    } else {
		wreq->ctq = 0;
	    }
	}

	wreq->flags = (u_char)flags;
	wd95setdma(ci, wreq);

	SET_WD_REG(sbp, WD95A_N_DESTID, sp->sr_target);
	SET_WD_REG(sbp, WD95A_N_SQADR, sqaddr);
}


/*
 * Returns 0 if chip is okay and ready.
 */
/* ARGSUSED */
int
init_wd95a(register volatile char *sbp, int host_id, register wd95ctlrinfo_t *ci)
{
	/* go to setup mode */
	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);

	/*
	 * Run BIST
	 */
#define FLAG_CHECK 0
#if FLAG_CHECK
	{
	register u_int tmp;

	SET_WD_REG(sbp, WD95A_S_TEST0, 0x43);
	DELAY(10);
	if (GET_WD_REG(sbp, WD95A_S_TEST1) != 0x3F) {
		debug(0);
		return 1;
	}
	SET_WD_REG(sbp, WD95A_S_TEST0, 0x42);
	for (tmp = GET_WD_REG(sbp, WD95A_S_TEST1);
	     !((tmp & 0x3) && (tmp & 0xC) && (tmp & 0x30));
	     tmp = GET_WD_REG(sbp, WD95A_S_TEST1));
	if (tmp != 0x2A) {
		debug(0);
		return 1;
	}
	}
#endif
	SET_WD_REG(sbp, WD95A_S_TEST0, 0x40);

	SET_WD_REG(sbp, WD95A_S_SCNF, SCNF_INIT);
	SET_WD_REG(sbp, WD95A_S_OWNID, host_id);

	SET_WD_REG(sbp, WD95A_S_TIMOUT, TIMEOUT_INIT);
	SET_WD_REG(sbp, WD95A_S_SLEEP, SLEEP_INIT);

	SET_WD_REG(sbp, WD95A_S_IDFLAG0, 0);
	SET_WD_REG(sbp, WD95A_S_IDFLAG1, 0);

	SET_WD_REG(sbp, WD95A_S_DMACNF, DMACNF_INIT);
	SET_WD_REG(sbp, WD95A_S_DMATIM, DMATIM_INIT);

	SET_WD_REG(sbp, WD95A_S_PLR, 0);
	SET_WD_REG(sbp, WD95A_S_PSIZE0, 0);
	SET_WD_REG(sbp, WD95A_S_PSIZE1, 0);

	SET_WD_REG(sbp, WD95A_S_CMPIX, 1);
	SET_WD_REG(sbp, WD95A_S_CMPVAL, 0x20);
	SET_WD_REG(sbp, WD95A_S_CMPMASK, 0xfc);
	
	/* go to normal mode */
	SET_WD_REG(sbp, WD95A_S_CTL, 0);

	SET_WD_REG(sbp, WD95A_N_CTLA, CTLA_INIT);

	SET_WD_REG(sbp, WD95A_N_ISRM, 0);
	SET_WD_REG(sbp, WD95A_N_STOPUM, STOPUM_INIT);
	SET_WD_REG(sbp, WD95A_N_UEIM, UEIM_INIT);

	SET_WD_REG(sbp, WD95A_N_SPW, SPW_SLOW);

	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
	SET_WD_REG(sbp, WD95A_S_SQDMA, WCS_NOWHERE);	/* not a dma master */
	SET_WD_REG(sbp, WD95A_S_SQRSL, WCS_I_NRESEL);	/* initiator */

	return 0;
}

void
load_95a_wcs(register volatile char *sbp, int wcs_type)
{
	unsigned int *up;
	int i;
#ifdef WDI_LOG_CNT
	wd95ctlrinfo_t *ci = NULL;
#endif

	/* go to setup mode */
	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP | W95_CTL_KILL);

	switch (wcs_type) {

	    case WCS_INITIATOR:
		up = _wcs_ini_load;
		break;

	    case WCS_SCIP:
		up = _wcs_scip_load;
		break;

	    default:
		return;
	}

	SET_WD_REG(sbp, WD95A_S_CSADR, 0);
	for (i = 0; i < WCS_SIZE; i++, up++) {
		SET_WD_REG(sbp, WD95A_S_CSPRT0, *up & 0xff);
		SET_WD_REG(sbp, WD95A_S_CSPRT1, (*up >> 8) & 0xff);
		SET_WD_REG(sbp, WD95A_S_CSPRT2, (*up >> 16) & 0xff);

		/*
		 * this one must be last because it causes the update of
		 * the address counter for the wcs in the chip
		 */
		SET_WD_REG(sbp, WD95A_S_CSPRT3, (*up >> 24) & 0xff);
	}
}

void
init_95a_wcs(register volatile char *sbp, int wcs_type)
{
	wd95ctlrinfo_t *ci = NULL;

	load_95a_wcs(sbp, wcs_type);

	/* issue abort */
	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_ABORT);

	/* clear out the interrupt that we caused with the abort */
	wd95_clear_isrs(ci, sbp);
}

	
/*	prevent chip from interrupting during rest of boot till ready,
	and do any initialization that is needed prior to use of
	the driver.  Called before main().  Needs to return a value
	for standalone.
*/
int
wd95_earlyinit(int adap)
{

	wd95ctlrinfo_t *ci;
	unsigned char hexdig;
	static int worn = 1;

	if (wd95cnt == 0)
		return 0;

	(void)wd95_ctlr_add(adap);
	/* allocate array on first call; wd95cnt may be changed in
	 * IP*.c before the first call to this routine. */
	if (!wd95dev) {
		wd95dev=(wd95ctlrinfo_t *)kern_calloc(sizeof(*wd95dev), wd95cnt);
		wd95unitinfo =(wd95unitinfo_t *)kern_calloc(
				sizeof(*wd95unitinfo), wd95cnt * WD95_LUPC);
		if (!wd95dev || !wd95unitinfo)
			cmn_err(CE_PANIC, "no memory for scsi device array");
	}

	ci = &wd95dev[adap];
	ci->ha_addr = (caddr_t)scuzzy[adap].d_addr;
	ci->number = adap;

	if (wd95_hostid) {
		ci->host_id = wd95_hostid;
	} 
	else if (hexdig = arg_wd95hostid[0]) 
		if (hexdig>='0' && hexdig<='9') 
		    ci->host_id = hexdig - '0';
		else
		   if (hexdig>='a' && hexdig<='f') 
		       ci->host_id = hexdig - 'a' + 10;
		   else
		      if (hexdig>='A' && hexdig<='F') 
			  ci->host_id = hexdig - 'A' + 10;
		      else {
			printf("wd95: WARNING - illigal NVRAM host scsi id\n");
			printf("wd95: WARNING - Setting host scsi ids to 0\n");
			ci->host_id = 0;
		      }
	     else  {
		ci->host_id = 0;
	     }

	if (ci->host_id > 7 && worn) {
	    printf("\nwd95: WARNING - setting  scsi controller id > 7 may");
	    printf(" disable some scsi devices\n\n");
	    worn = 0;
	}

	if (!wd95_present(ci->ha_addr))
		goto err_init;

#if defined(_STANDALONE)
	/*
	 * If the bus reset doesn't go well, ignore the bus
	 */
	if (wd95busreset(ci) == -1) {
		printf(" - bus may not work properly\n");
	}
#endif

#ifdef WDI_LOG_CNT
	ci->wdi_log = (ushort *)kern_calloc(sizeof(ushort), WDI_LOG_CNT);
#endif
	/*
	 * Initialize controller.  Set up controller initialization block in
	 * main memory, then copy it down to the controller and execute the
	 * Initialize Controller command.
	 */
	init_95a_wcs(ci->ha_addr, WCS_INITIATOR);
	if (init_wd95a(ci->ha_addr, ci->host_id, ci))
		goto err_init;

	ci->d_map = dma_mapalloc(DMA_SCSI, adap, 0, 0);
	if (ci->d_map == ((dmamap_t *)-1l))
		goto err_init;
#ifdef IP22
	if ((ci->d_map = scsi_getchanmap(ci->d_map, 0)) == NULL)
		goto err_init;
#endif

	init_sema(&ci->d_sleepsem, 0,"dps", adap);
	init_sema(&ci->d_onlysem, 0,"dpo", adap);
	INITLOCK(&ci->qlock, "dpq", adap);

	ci->round_robin = 0;
	ci->max_round = WD95_LUPC;


	return 0;	/* for ide; */

err_init:
	ci->ha_addr = NULL;
	return -1;
}

#define	TEST_VAL	TIMEOUT_INIT

int
wd95_present(register volatile char *sbp)
{
	int id;
#ifdef WDI_LOG_CNT
	wd95ctlrinfo_t *ci = NULL;
#endif

	if ( ! sbp )
		return 0;

	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
	SET_WD_REG(sbp, WD95A_S_TIMOUT, TEST_VAL);

	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
	id = GET_WD_REG(sbp, WD95A_S_TIMOUT) & 0xff;

	if (id != TEST_VAL) {
#if 0
		printf("w_p: no ctlr at 0x%x\n", sbp);
#endif
		return 0;
	}
	return 1;
}

int
wd95_intpresent(wd95ctlrinfo_t *ci)
{
	register volatile char *ibp = ci->ha_addr;
	int cstat;

	cstat = GET_WD_REG(ibp, WD95A_N_ISR);
	return (cstat & W95_ISR_INT0) ? 1 : 0;
}

#if WD_I_DEBUG
wdilog_dump(int ctlr)
{
	struct wd95ctlrinfo *ci;
	ushort *log;
	ushort i;

	ci = &wd95dev[ctlr];
	log = ci->wdi_log;
	printf("log at 0x%x, next entry %d\n", log, log[0]);
	for (i = log[0]; i < WDI_LOG_CNT; i++) {
		printf("%s reg 0x%x data %d (0x%x)",
		       (log[i] & 0x100) ? "READ" : "WRITE",
		       (log[i] & 0xFE00) >> 8, log[i] & 0xFF, log[i] & 0xFF);
		if (i & 1)
			printf("\n");
		else
			printf("   ");
	}
	for (i = 1; i < log[0]; i++) {
		printf("%s reg 0x%x data %d (0x%x)",
		       (log[i] & 0x100) ? "READ" : "WRITE",
		       (log[i] & 0xFE00) >> 8, log[i] & 0xFF, log[i] & 0xFF);
		if (i & 1)
			printf("\n");
		else
			printf("   ");
	}
	printf("\n");
}
#endif
#endif /* EVEREST */
