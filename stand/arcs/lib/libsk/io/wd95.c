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

/*
 * This file contains a relatively straightforward port of the kernel
 * level wd95 driver, circa August '93.  It contains a lot of code which
 * isn't really needed by the prom, but I figured I'd leave it, at least
 * for awhile, since one never knows what the future will bring.
 */

#if defined(EVEREST)
#ident	"io/wd95.c: $Revision: 1.33 $"

#define	SCSI_3		1
#define WD95_CMDHIST	0

#include <sys/types.h>
#include <sys/cpu.h>

#include <arcs/hinv.h>
#include <arcs/cfgdata.h>

#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/dmamap.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/vmereg.h>
#include <sys/pio.h>
#include <sys/invent.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/scsi.h>
#define _NSCSI_NOPROTOS_
#include <sys/newscsi.h>
#include <sys/scsimsgs.h>	/* correct values of SC_NUMADDSENSE */
#include <sys/wd95a.h>
#include <sys/wd95a_struct.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <sys/wd95a_wcs.h>
#include <libsc.h>
#include <libsk.h>

/*
 * Define macros for kernel to standalone conversion.
 */
#define kmem_alloc(X, Y) dmabuf_malloc(X)
#define kvpalloc(X, Y, Z) dmabuf_malloc((X) * IO_NBPC)
#define dcache_inval(X, Y)
#define dma_mapfree(X)
#define splvme() 0
#define splockspl(x,y) 0
#define spunlockspl(x,y)
#define splock(x) 0
#define spunlock(x,y)
#define initnlock(x,y)
#define spsema(x)
#define svsema(x)
#define showconfig 0
#define WD95_DEFAULT_FLAGS 0 

/*
 * NOTE: these tables are used by the smfd, tpsc, and dksc drivers, and will
 * probably be used by other SCSI drivers in the future.  That is why they are
 * now in scsi.c, even though not referenced here.
 */
char *scsi_key_msgtab[SC_NUMSENSE] = {
        "No sense",                             /* 0x0 */
        "Recovered Error",                      /* 0x1 */
        "Drive not ready",                      /* 0x2 */
        "Media error",                          /* 0x3 */
        "Unrecoverable device error",           /* 0x4 */
        "Illegal request",                      /* 0x5 */
        "Unit Attention",                       /* 0x6 */
        "Data protect error",                   /* 0x7 */
        "Unexpected blank media",               /* 0x8 */
        "Vendor Unique error",                  /* 0x9 */
        "Copy Aborted",                         /* 0xa */
        "Aborted command",                      /* 0xb */
        "Search data successful",               /* 0xc */
        "Volume overflow",                      /* 0xd */
        "Reserved (0xE)",                       /* 0xe */
        "Reserved (0xF)",                       /* 0xf */
};

/*
 * A number of previously unspec'ed errors were spec'ed in SCSI 2; these were
 * added from App I, SCSI-2, Rev 7.  The additional sense qualifier adds more
 * info on some of these...
 */
char *scsi_addit_msgtab[SC_NUMADDSENSE] = {
        NULL,   /* 0x00 */
        "No index/sector signal",               /* 0x01 */
        "No seek complete",                     /* 0x02 */
        "Write fault",                          /* 0x03 */
        "Driver not ready",                     /* 0x04 */
        "Drive not selected",                   /* 0x05 */
        "No track 0",                           /* 0x06 */
        "Multiple drives selected",             /* 0x07 */
        "LUN communication error",              /* 0x08 */
        "Track error",                          /* 0x09 */
        "Error log overflow",                   /* 0x0a */
        NULL,                                   /* 0x0b */
        "Write error",                          /* 0x0c */
        NULL,                                   /* 0x0d */
        NULL,                                   /* 0x0e */
        NULL,                                   /* 0x0f */
        "ID CRC or ECC error",                  /* 0x10  */
        "Unrecovered data block read error",    /* 0x11 */
        "No addr mark found in ID field",       /* 0x12 */
        "No addr mark found in Data field",     /* 0x13 */
        "No record found",                      /* 0x14 */
        "Seek position error",                  /* 0x15 */
        "Data sync mark error",                 /* 0x16 */
        "Read data recovered with retries",     /* 0x17 */
        "Read data recovered with ECC",         /* 0x18 */
        "Defect list error",                    /* 0x19 */
        "Parameter overrun",                    /* 0x1a */
        "Synchronous transfer error",           /* 0x1b */
        "Defect list not found",                /* 0x1c */
        "Compare error",                        /* 0x1d */
        "Recovered ID with ECC",                /* 0x1e */
        NULL,                                   /* 0x1f */
        "Invalid command code",                 /* 0x20 */
        "Illegal logical block address",        /* 0x21 */
        "Illegal function",                     /* 0x22 */
        NULL,                                   /* 0x23 */
        "Illegal field in CDB",                 /* 0x24 */
        "Invalid LUN",                          /* 0x25 */
        "Invalid field in parameter list",      /* 0x26 */
        "Media write protected",                /* 0x27 */
        "Media change",                         /* 0x28 */
        "Device reset",                         /* 0x29 */
        "Log parameters changed",               /* 0x2a */
        "Copy requires disconnect",             /* 0x2b */
        "Command sequence error",               /* 0x2c */
        "Update in place error",                /* 0x2d */
        NULL,                                   /* 0x2e */
        "Tagged commands cleared",              /* 0x2f */
        "Incompatible media",                   /* 0x30 */
        "Media format corrupted",               /* 0x31 */
        "No defect spare location available",   /* 0x32 */
        "Media length error",                   /* 0x33 (tape only) */
        NULL,                                   /* 0x34 */
        NULL,                                   /* 0x35 */
        "Toner/ink error",                      /* 0x36 */
        "Parameter rounded",                    /* 0x37 */
        NULL,                                   /* 0x38 */
        "Saved parameters not supported",       /* 0x39 */
        "Medium not present",                   /* 0x3a */
        "Forms error",                          /* 0x3b */
        NULL,                                   /* 0x3c */
        "Invalid ID msg",                       /* 0x3d */
        "Self config in progress",              /* 0x3e */
        "Device config has changed",            /* 0x3f */
        "RAM failure",                          /* 0x40 */
        "Data path diagnostic failure",         /* 0x41 */
        "Power on diagnostic failure",          /* 0x42 */
        "Message reject error",                 /* 0x43 */
        "Internal controller error",            /* 0x44 */
        "Select/Reselect failed",               /* 0x45 */
        "Soft reset failure",                   /* 0x46 */
        "SCSI interface parity error",          /* 0x47 */
        "Initiator detected error",             /* 0x48 */
        "Inappropriate/Illegal message",        /* 0x49 */
};

extern int cachewrback;

#define WDEBUG		1
#define WTIMEOUT	0
#define	D_C_DEBUG	0
#define	D_X_DEBUG	0
#define	WD_I_DEBUG	0	/* XXX Turn off logging for speed */

#define	MSG_SDTR	0x01		/* sync request message */
#define	MSG_WIDE	0x03		/* wide request message */
#define	MSG_WIDE_8	0x00		/* info for 8 bit */
#define	MSG_WIDE_16	0x01		/* info for 16 bit */

#define	SCSI_INFO_BITS	7		/* inquiry bit info */
#define	SCSI_WIDE_BIT	0x20		/* set if dev can support 16 bit */
#define	SCSI_SYNC_BIT	0x10		/* set if dev can support SYNC */
#define	SCSI_CMDT_BIT	0x02		/* set if dev can support CMD TAGS */

#define	MAX_OFFSET	32		/* maximum byte in scsi req/ack que */

#define	SPL_WD95	spl3


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
#define	wdiprintf(_x)	printf _x
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
int	wd95_sync_en = 0;	/* Disallow synchronous transactions */
int	wd95_wide_en = 0;	/* Disallow wide negotiations */
int	wd95_disc_en = 0;	/* Disallow disconnect/reconnect */
int	wd95_cmdt_en = 0;	/* Disallow cmd tag queueing */
int	wd95_no_next = 1;	/* this is so that we do this command now */

wd95ctlrinfo_t	*wd95dev;
wd95unitinfo_t	*wd95unitinfo;

#define wd95cnt scsicnt
extern int wd95cnt;
extern scuzzy_t scuzzy[];

/* Prototypes: */
void		wd95intr(int, struct eframe_s *);
void		wd95timeout(wd95request_t *);
static void	wd95a_do_cmd(wd95unitinfo_t *, wd95request_t *);
void		wd95clear(wd95ctlrinfo_t *, wd95unitinfo_t *, char *);
static void	wd95_abort(register volatile char *);
void		wd95complete(wd95ctlrinfo_t *, wd95unitinfo_t *, wd95request_t *,
			     scsi_request_t *);
static void	wd95start(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui);
struct scsi_target_info *wd95info(u_char ctlr, u_char targ, u_char lun);
static int	wd95busreset(register wd95ctlrinfo_t *ci);
static void	wd_rm_wq(register wd95unitinfo_t *ui, register wd95request_t *wreq);
int		wd95_earlyinit(int);
void		wd95_fin_reset(wd95ctlrinfo_t *);
int		wd95_intpresent(wd95ctlrinfo_t *);
int		init_wd95a(volatile char *, int, wd95ctlrinfo_t *);
int		wd95_present(volatile char *);

/* extern prototypes */
extern int	s1dma_flush(dmamap_t *);
extern int	scsi_is_external(const int);
extern void	scsi_go(dmamap_t*, caddr_t, int);

/* externs from master.d/wd95a */
extern uint			 wd95_hostid;

/* SCSI command arrays */
#define	SCSI_INQ_CMD	0x12
#define	SCSI_REQ_SENSE	0x03
static u_char dk_inquiry[] =	{SCSI_INQ_CMD, 0, 0, 0,    4, 0};

/* used to get integer from non-aligned integers */
#define MKINT(x) (((x)[0] << 24) + ((x)[1] << 16) + ((x)[2] << 8) + (x)[3])

#if defined(EVEREST)

#define	ACCESS_SIZE	8		/* each register is 8 byte aligned */
#define	ACC_SHIFT	3		/* if you prefer shifting */

#if WD_I_DEBUG
#define	WDI_LOG_CNT	512

set_wd_reg(register caddr_t sbp, register u_char reg, register u_char val,
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
	*(uint *)(sbp + (reg << ACC_SHIFT) + 4) = val;
}

#define SET_WD_REG(_sbp, _reg, _val) set_wd_reg((u_char *) _sbp, _reg, _val, ci)

get_wd_reg(register caddr_t sbp, register u_char reg,
		register wd95ctlrinfo_t *ci)
{
	register ushort *wdi_log;
	register u_char val;
	val = *(uint *)(sbp + (reg << ACC_SHIFT) + 4);
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

#define	GET_WD_REG(_sbp, _reg) \
		(*(volatile unsigned int *)(_sbp + (_reg << ACC_SHIFT) + 4) & 0xff)
#define	SET_WD_REG(_sbp, _reg, _val) \
		*(volatile unsigned int *)(_sbp + (_reg << ACC_SHIFT) + 4) = (_val)
#endif /* WD_I_DEBUG */

#define	GET_DMA_REG(_sbp, _reg) \
		*(uint *)(_sbp + _reg + 4)
#define	SET_DMA_REG(_sbp, _reg, _val) \
		*(uint *)(_sbp + _reg + 4) = (_val)

#define	SCNF_INIT	((4 << W95_SCNF_CLK_SHFT) | W95_SCNF_SPUE | \
				W95_SCNF_SPDEN)
#define	DMACNF_INIT	(W95_DCNF_DMA16 | W95_DCNF_DPEN | W95_DCNF_PGEN | \
				W95_DCNF_BURST)

#define	WD_C_B(_ctlr)	(_ctlr)

#else /* EVEREST */

#define	WD_C_B(_ctlr)	(_ctlr)

#endif /* EVEREST */

#define	SPW(per) (((per) << W95_SPW_SCLKA_SFT) | ((per) << W95_SPW_SCLKN_SFT))
#define	SPW_SLOW	SPW(2)

#define	TIMEOUT_INIT	77		/* Normal timeout value */
#define TIMEOUT_FAST	15		/* Fast timeout value */

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

#if WD95_CMDHIST
wd95_cmdhist_dump(wd95unitinfo_t *ui, wd95request_t *wreq)
{
	int i, x;
	printf("  Command bytes:");
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

#ifdef DEBUG
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

	int	s;

#if WDEBUG
	printf("wd95_%dd%d: attempting to start\n", WD_C_B(ci->number), ui->number);
#endif
	s = splockspl(ci->lock, SPL_WD95);
	ui->tostart = 0;
	wd95start(ci, ui);
	spunlockspl(ci->lock, s);
}

static struct scsi_request *
getreq(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui)
{
	wd95luninfo_t  *li;
	scsi_request_t *req;
	scsi_request_t *oreq;

	/*
	 * Look through the request queue for an eligible command.
	 */
	oreq = NULL;
	for (req = ui->reqhead; req != NULL; oreq = req, req = req->sr_ha) {
		li = ui->lun_info[req->sr_lun];
		if (li->wqueued && !li->cmdt_en)
			continue;

#if ODD_BYTE_WAR
		/* WARNING:  You need to include the odd-byte workaround if
		 * you enable sync mode or wide mode.
		 */
		if ((req->sr_buflen % 2) && (req->sr_flags & SRF_DIR_IN)) {
			if (ci->pagebusy) {
				panic("wd95: pagebusy.  You should worry\n");
#ifdef DEBUG
				printf("wd95_%dd%d: fixup page busy\n",
					WD_C_B(ci->number), ui->number);
#endif
				if (!ui->tostart)
					ui->tostart = 1;
				continue;
			}
			ci->pagebusy = 1;
			req->sr_ha_flags |= WFLAG_ODDBYTEFIX;
		}
#endif /* ODD_BYTE_WAR */
		break;
	}

	if (req != NULL) {
		if (oreq == NULL)
			ui->reqhead = req->sr_ha;
		else
			oreq->sr_ha = req->sr_ha;

		if (ui->reqhead == NULL)
			ui->reqtail = NULL;
		else if (ui->reqtail == req)
			ui->reqtail = oreq;
		req->sr_ha = NULL;
	}
	return req;
}

void
wd95requeue(wd95unitinfo_t *ui)
{
	wd95ctlrinfo_t	*ci = ui->ctrl_info;
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
				if (req->sr_notify == NULL) {
					vsema(&ui->start);
				} else {
					svsema(ci->lock);
					(*req->sr_notify)(req);
					spsema(ci->lock);
				}
				continue;
			}
		}
		if (ui->reqhead == NULL)
			ui->reqhead = req;
		else
			ui->reqtail->sr_ha = req;
		ui->reqtail = req;
	}
}


#define WD95_INCTARG(t) ((t + 1) & (ci->max_round - 1));

static void
wd95start(register wd95ctlrinfo_t *ci, register wd95unitinfo_t *ui)
{
	register wd95luninfo_t	*li;
	register scsi_request_t	*req;
	register wd95request_t	*wreq;
	dmamap_t		*dmamap;
	int			 dmalen;
	int			 unit_cntr;
	int			 origtarg;
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
		 * Find out if the current unit is eligible for a command.
		 * Get a wreq if available, then get a req.
		 * If we get a wreq, but can't get a req, put the wreq back.
		 */
		if (ui->u_ca || (wreq = ui->wrfree) == NULL)
			ui = NULL;
		else {
			if ((req = getreq(ci, ui)) != NULL) {
				ui->wrfree = wreq->next;
			} else {
				ui = NULL;
			}
		}
		if (ui != NULL)
			break;
		/*
		 * If we can't issue a command to the current target, try the
		 * next target, using round robin.
		 */
		if (ci->round_robin == origtarg ||
		    ci->round_robin == ci->host_id)
			ci->round_robin = WD95_INCTARG(ci->round_robin);
			ui = ci->unit[ci->round_robin];
			ci->round_robin = WD95_INCTARG(ci->round_robin);
			if (ui->auxhead)
				wd95requeue(ui);
	}
	if (unit_cntr == ci->max_round)
		return;
	ci->active = 1;
	ui->active = 1;
	ui->cur_lun = wreq->lun = req->sr_lun;
	li = ui->lun_info[ui->cur_lun];
	wreq->active = 1;
	wreq->sreq = req;
	wreq->resid = req->sr_buflen;
	ci->cur_target = ui->number;

	/* add it to the que for this device */
	wreq->next = li->wqueued;
	li->wqueued = wreq;

	/*
	 * Take care of mapping, if needed, and set data address and length.
	 */
	dmamap = ci->d_map;
	if (req->sr_buflen != 0) {
		if (req->sr_flags & SRF_MAPBP)
			dmalen = dma_mapbp(dmamap, req->sr_bp, req->sr_buflen);
		else
			dmalen = dma_map(dmamap, req->sr_buffer, req->sr_buflen);
	}
	if (dmalen < req->sr_buflen)
		printf("wd95: Firmware error, data loss imminent!\n");
	wd95a_do_cmd(ui, wreq);
	wreq->reset_num = ci->reset_num;
}

int
wd95alloc(u_char ctlr, u_char targ, u_char lun, int opt, void(*cb)())
{
	wd95ctlrinfo_t	*ci;
	wd95unitinfo_t	*ui;
	wd95luninfo_t	*li;
	sema_t		*o_sem;
	int		ret = SCSIDRIVER_WD95;
	int		qdepth = opt & SCSIALLOC_QDEPTH;

	/*
	 * Check for validity of arguments.
	 */
	if (ctlr >= wd95cnt || targ >= WD95_LUPC || lun != 0) {
		return SCSIDRIVER_NULL;
	}

	ci = &wd95dev[ctlr];
	if (!ci->present || (targ == ci->host_id))
		return SCSIDRIVER_NULL;
	ui = ci->unit[targ];
	li = ui->lun_info[lun];

	o_sem = &ui->opensem;
	psema(o_sem, PRIBIO);
	if (li == NULL || li->tinfo.si_inq == NULL) {
		(void)wd95info(ctlr, targ, lun);
		li = ui->lun_info[lun];
		if (li == NULL || li->tinfo.si_inq == NULL) {
			vsema(o_sem);
			return SCSIDRIVER_NULL;
		}
	}
	if (opt & SCSIALLOC_EXCLUSIVE) {
		if (li->refcnt) {
			vsema(o_sem);
			return SCSIDRIVER_NULL;
		}
		li->excl_act = 1;
	} else {
		if (li->excl_act) {
			vsema(o_sem);
			return SCSIDRIVER_NULL;
		}
	}
	if (cb) {
		if (ui->sense_callback != NULL) {
			vsema(o_sem);
			return SCSIDRIVER_NULL;
		} else {
			ui->sense_callback = cb;
		}
	}
	li->refcnt++;
	if ( !li->u_sense_req ) {
	    li->u_sense_req = kmem_alloc(sizeof(scsi_request_t) + SC_CLASS0_SZ, KM_CACHEALIGN);
	    {
		register scsi_request_t	*sprq = li->u_sense_req;

		sprq->sr_ctlr = ctlr;
		sprq->sr_target = targ;
		sprq->sr_lun = lun;
		sprq->sr_command = (u_char *)&sprq[1];
		sprq->sr_cmdlen = SC_CLASS0_SZ;		/* never changes */
		sprq->sr_timeout = 30*HZ;		/* never changes */
	    }
	}
	vsema(o_sem);
	return SCSIDRIVER_WD95;
}

static void
wd95free(u_char adap, u_char targ, u_char lu, void (*cb)())
{
	wd95ctlrinfo_t	*ci;
	wd95unitinfo_t	*ui;
	wd95luninfo_t	*li;

	if(adap >= wd95cnt || targ > WD95_LUPC || lu >= SCSI_MAXLUN)
		return;

	ci = &wd95dev[adap];
	ui = ci->unit[targ];
	li = ui->lun_info[lu];

	psema(&ui->opensem, PRIBIO);
	if ( cb ) {
		if (ui->sense_callback == cb) {
			ui->sense_callback = NULL;
		}
#if wd95_verbose
		else printf("wd95(%d,%d,%d): callback 0x%x not active\n",
			WD_C_B(adap), target, lun, ui->sense_callback);
#endif
	}
	if ( ! li->refcnt ) {
#if wd95_verbose
		printf("attempted to free unallocated channel %d, %d, %d\n",
			WD_C_B(adap), target, lun);
#endif
	} else {
		if (--li->refcnt == 0) {
			li->excl_act = 0;
#if wd95_verbose
			if (ui->sense_callback)
			    printf("wd95(%d,%d,%d) but callback still 0x%x\n",
				WD_C_B(adap), target, lun, ui->sense_callback);
#endif
			ui->sense_callback = NULL;
		}
#if wd95_verbose
		else printf("wd95(%d,%d,%d) but refcnt still %d\n",
			WD_C_B(adap), target, lun, li->refcnt);
#endif
	}
	vsema(&ui->opensem);
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
	int		s;

	req->sr_resid = req->sr_buflen;
	req->sr_status = 0;
	req->sr_scsi_status = req->sr_ha_flags = req->sr_sensegotten = 0;
	req->sr_spare = NULL;
	req->sr_ha = NULL;

	/*
	 * Check for validity of arguments.
	 */
	if ((ctlr = req->sr_ctlr) >= wd95cnt ||
	    (targ = req->sr_target) >= WD95_LUPC ||
	    (lun = req->sr_lun) >= SCSI_MAXLUN ||
	    (targ == wd95dev[ctlr].host_id) ||
	    (req->sr_buflen >= 0xffffff)) {
		req->sr_status = SC_REQUEST;
		goto err_wd95command;
	}

	/* Buffers must aligned on a four-byte boundary */
	if (req->sr_buflen != 0 && ((ulong) req->sr_buffer % 4)) {
		printf("wd95%dd%d: starting address must word aligned\n",
		       WD_C_B(ctlr), targ);
		printcdb(req->sr_command);
		req->sr_status = SC_ALIGN;
		goto err_wd95command;
	}

	ci = &wd95dev[ctlr];
	ui = ci->unit[targ];
	li = ui->lun_info[lun];

	/*
	 * Check to see if a device has been seen by wd95info previously.
	 */
	if (li == NULL || li->tinfo.si_inq == NULL) {
		req->sr_status = SC_HARDERR;
		goto err_wd95command;
	}

	/*
	 * If sr_notify is NULL, we have to acquire the start semaphore.
	 */
	if (req->sr_notify == NULL && !wd95_no_next)
		psema(&ui->start, PRIBIO);

	wdcprintf(("w9c%dd%d", WD_C_B(ctlr), targ));
	s = splockspl(ci->lock, SPL_WD95);
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
	if (ui->aenreq && !(ui->u_ca || ci->reset)) {
		if (req->sr_flags & SRF_AEN_ACK)
			ui->aenreq = 0;
		else {
			req->sr_status = SC_ATTN;
			if (req->sr_notify == NULL)
				vsema(&ui->start);
			spunlockspl(ci->lock, s);
			goto err_wd95command;
		}
	}
	if (ui->reqhead == NULL)
		ui->reqhead = req;
	else
		ui->reqtail->sr_ha = req;
	ui->reqtail = req;
	wd95start(ci, ui);

end_wd95command:
	spunlockspl(ci->lock, s);
	if (wd95_no_next) {
		wd95request_t *wreq = li->wqueued;
		int timeout = 0;
		while (ci->active) {
			while (!wd95_intpresent(ci) && 
			       timeout++ < (req->sr_timeout*100))
				us_delay(100);
#if EVEREST
			if (IS_KSEG1(req->sr_buffer)) {
			    	flush_iocache(s1adap2slot(ctlr));
			}
#endif
			if (wd95_intpresent(ci))
				wd95intr(ci->number, NULL); /* do it */
			else
				wd95timeout(wreq);
		}
	}
	if (req->sr_notify == NULL && !wd95_no_next) {
		psema(&ui->done, PRIBIO);
		vsema(&ui->start);
	}
	return;

err_wd95command:
	if (req->sr_notify != NULL)
		(*req->sr_notify)(req);
}

struct scsi_target_info *
wd95info(u_char ctlr, u_char targ, u_char lun)
{
	wd95ctlrinfo_t	*ci;
	wd95unitinfo_t	*ui;
	wd95luninfo_t	*li;
	scsi_request_t	*req;
	u_char		*scsi;
	char		stat;

	/*
	 * Check for validity of arguments.
	 */
	if (ctlr >= wd95cnt || targ >= WD95_LUPC || lun >= SCSI_MAXLUN) {
		return NULL;
	}
	ci = &wd95dev[ctlr];
	if (!ci->present || (targ == ci->host_id))
		return NULL;
	ui = ci->unit[targ];
	if ((li = ui->lun_info[lun]) == NULL) {
		li = (wd95luninfo_t *) kern_calloc(1, sizeof(wd95luninfo_t));
		ui->lun_info[lun] = li;
	}	

	/*
	 * If we haven't done so already, allocate DMA maps, sense and
	 * inquiry buffers for this device.
	 */
	if (li->tinfo.si_inq == NULL) {
		li->tinfo.si_inq = kmem_alloc(SCSI_INQUIRY_LEN +10, KM_CACHEALIGN);
		li->tinfo.si_sense = kmem_alloc(SCSI_SENSE_LEN, KM_CACHEALIGN);
	}
	/*
	 * Issue inquiry to find out if this address has a device.
	 * If we get an error, free the DMA map, sense and inquiry buffers.
	 */
	scsi = &(li->tinfo.si_inq[SCSI_INQUIRY_LEN]);
	bcopy(dk_inquiry, scsi, sizeof(dk_inquiry));
	scsi[4] = SCSI_INQUIRY_LEN;
	scsi[1] = lun << 5;

	do {
		req = kern_calloc(1, sizeof(*req));
		if (req == NULL)
			printf("Uh oh, we're in deep shit\n");
		req->sr_ctlr = ctlr;
		req->sr_target = targ;
		req->sr_lun = lun;
		req->sr_command = scsi;
		req->sr_cmdlen = sizeof(dk_inquiry);
		req->sr_flags = SRF_DIR_IN | SRF_FLUSH | SRF_AEN_ACK;
		req->sr_timeout = 10 * HZ;
		req->sr_buffer = li->tinfo.si_inq;
		req->sr_buffer[0] = 0x7f;
		req->sr_buflen = SCSI_INQUIRY_LEN;
		req->sr_sense = NULL;
		req->sr_notify = NULL;
		wd95command(req);

		stat = req->sr_status|req->sr_scsi_status|req->sr_sensegotten;
		kern_free(req);
	} while (stat == SC_ATTN);
	
	if (stat || li->tinfo.si_inq[0] == 0x7f) {
		dmabuf_free(li->tinfo.si_inq);
		dmabuf_free(li->tinfo.si_sense);

		li->tinfo.si_inq = NULL;
		li->tinfo.si_sense = NULL;
		if (lun != 0) {
			kern_free(li);
			ui->lun_info[lun] = NULL;
		}
		return NULL;
	}

	/* Switch off all the scary stuff we don't want 
	 * to support 
	 */
	ui->wide_en  = 0;
	ui->wide_req = 0;
	ui->sync_en  = 0;
	ui->sync_req = 0;
	ui->disc     = 0;
	li->cmdt_en  = 0;

	li->tinfo.si_maxq = WD95_QDEPTH;
	return &li->tinfo;
}


/*
 * scsi_inq -- A compatibility function used by the cdrom stuff
 */
u_char *
scsi_inq(u_char c, u_char t, u_char l)
{
        struct scsi_target_info *info;
        info = wd95info(c, t, l);
        if (info != NULL)
                return info->si_inq;
        else
                return NULL;
}

/*
 * Clear Interrupt Status Registers and double check to see they are clear.
 */
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
 * Then sleep for 250ms to give the SCSI disks time to reset.
 */
static int
wd95busreset(register wd95ctlrinfo_t *ci)
{
	volatile register caddr_t ibp = ci->ha_addr;
	int val;
	int retval = 0;

	val = GET_WD_REG(ibp, WD95A_S_CTL) & W95_CTL_SETUP;

	SET_WD_REG(ibp, WD95A_S_CTL, 0);
	SET_WD_REG(ibp, WD95A_N_TC_0_7, 0);
	SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
	s1dma_flush(ci->d_map); /* XXX readflag not avail or used */

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
	s1_reset(ci->number);
	DELAY(400);
	if (init_wd95a(ibp, ci->host_id, ci)) {
		printf("wd95_%d: failed Built-In SelfTest\n");
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
		ui->sync_per = SPW_SLOW;
		ui->sync_off = 0;
	}
}


void
wd95reset(wd95ctlrinfo_t *ci, wd95unitinfo_t *ui, wd95request_t *wreq, char *rp)
{
	int cnt, unit;

	if (wreq != NULL)
		if (wreq->timeout_id) {
			untimeout(wreq->timeout_id);
			wreq->timeout_id = 0;
		}

	if (ui != NULL)
		unit = ui->number;
	else
		unit = 0;
	ASSERT(!ci->reset);

#if 0
	ci->reset = 1;
	ci->reset_num++;
#endif
	if (ui != NULL)
		ui->u_ca = 1;
	printf("wd95_%dd%d: %s - resetting SCSI bus\n",
		WD_C_B(ci->number), unit, rp);

	if (wd95busreset(ci) == -1)
		printf(" - check connections\n");
	wd95_fin_reset(ci);

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
		ui->ext_msg = 0;
		/* now if any new requests came in, requeue them */
		if (ui->auxhead)
			wd95requeue(ui);
		ui->active = 0;
	}
	ci->cur_target = ci->host_id;
}

void
wd95timeout(wd95request_t *wreq)
{
	register wd95ctlrinfo_t	*ci;
	register wd95unitinfo_t	*ui;
	uint			ctlr, unit;
	int			pri, cnt;

	ctlr = wreq->ctlr;
	unit = wreq->unit;
	ci = &wd95dev[ctlr];
	ui = ci->unit[unit];
	pri = splockspl(ci->lock, SPL_WD95);
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
	spunlockspl(ci->lock, pri);
}


static void
wd_rm_wq(register wd95unitinfo_t *ui, register wd95request_t *wreq)
{
	wd95luninfo_t	*li = ui->lun_info[wreq->lun];

	if (wreq->timeout_id) {
		untimeout(wreq->timeout_id);
		wreq->timeout_id = 0;
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
	wd_rm_wq(ui, wreq);

	ui->active = 0;

	if (ui->sense) { /* completion of req sense */
		scsi_request_t	*oreq;
		int	retlen;

		ui->sense = 0;
		oreq = req->sr_spare;
		dcache_inval(li->tinfo.si_sense, SCSI_SENSE_LEN);
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
			oreq->sr_sensegotten = SCSI_SENSE_LEN - wreq->resid;
			bcopy(li->tinfo.si_sense, oreq->sr_sense,
			      MIN(oreq->sr_sensegotten, oreq->sr_senselen));
			oreq->sr_scsi_status = ST_GOOD;
		}

		if ( ui->sense_callback )
			(*ui->sense_callback)(li->tinfo.si_sense);

		req = oreq;
	}

	if (req != NULL) {
		ASSERT((ulong) req < (ulong) ui || (ulong) req > (ulong) ui+1);
		if (req->sr_spare)
			dma_mapfree(req->sr_spare);
		if ((req->sr_flags & SRF_FLUSH) &&
		    (req->sr_flags & SRF_DIR_IN))
			dcache_inval(req->sr_buffer, req->sr_buflen);

		/* copy data from spare align buffer */
#if ODD_BYTE_WAR
		if (req->sr_ha_flags & WFLAG_ODDBYTEFIX) {
			int i, j;
			char *startaddr;

			req->sr_buflen--;
			req->sr_resid--;
			i = (poff(req->sr_buffer) + req->sr_buflen) % NBPP;
			if (i > req->sr_buflen) {
				startaddr = &ci->page[poff(req->sr_buffer)];
				i = req->sr_buflen;
			}
			else
				startaddr = ci->page;

			j = i - req->sr_resid;
			if (j > 0) {
				dcache_inval(startaddr, j);
				bcopy(startaddr,
				      &req->sr_buffer[req->sr_buflen - i], j);
			}
			ci->pagebusy = 0;
		}
#endif /* ODD_BYTE_WAR */

		/* report completion on current command */
		if (req->sr_notify == NULL) {
			vsema(&ui->done);
		}
		else {
			svsema(ci->lock);
			(*req->sr_notify)(req);
			spsema(ci->lock);
		}

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
	register scsi_request_t	*nrq;
	register wd95request_t	*wreq;
	u_char			lun;

	/* grab hold of the waiting queue for later */
	nrq = ui->reqhead;
	ui->reqhead = ui->reqtail = NULL;

	for (lun = 0; lun < SCSI_MAXLUN; lun++) {
	    if ((li = ui->lun_info[lun]) == NULL)
		continue;
	    while (wreq = li->wqueued) {
		req = wreq->sreq;
		if (li->sense) {
		    printf("wd95_%dd%d: req sense 0x%x 0x%x aborted - %s\n",
			     WD_C_B(ci->number), ui->number, wreq, req, data);
		    req = req->sr_spare;
		    li->sense = 0;
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
		if (req->sr_spare)
			dma_mapfree(req->sr_spare);
		wd_rm_wq(ui, wreq);

#if ODD_BYTE_WAR
		if (req->sr_ha_flags & WFLAG_ODDBYTEFIX)
			ci->pagebusy = 0;
#endif /* ODD_BYTE_WAR */

		if (req->sr_notify == NULL) {
			vsema(&ui->done);
		} else {
			svsema(ci->lock);
			(*req->sr_notify)(req);
			spsema(ci->lock);
		}
	    }
	}

	/* Clear the wait queue */
	while (req = nrq) {
		li = ui->lun_info[req->sr_lun];
		if (li->sense) {
			register scsi_request_t *oreq;
			oreq = req->sr_spare;
			oreq->sr_ha = req->sr_ha;
			req = oreq;
			li->sense = 0;
		}
		req->sr_status = SC_ATTN;
#if WTIMEOUT
		printf("wd95_%dd%d: queued request cancelled by %s\n",
		          WD_C_B(ci->number), ui->number, data);
#endif
		nrq = req->sr_ha;
		if (req->sr_notify == NULL) {
			vsema(&ui->done);
		} else {
			svsema(ci->lock);
			(*req->sr_notify)(req);
			spsema(ci->lock);
		}
	}
}


void
wd95rsel(register wd95ctlrinfo_t *ci)
{
	register volatile char	*ibp = ci->ha_addr;
	register wd95unitinfo_t	*ui = NULL;
	wd95luninfo_t		*li = NULL;
	wd95request_t		*wreq = NULL;
	register scsi_request_t	*req = NULL;
	int			flags, target;
	ulong		 	startaddr;

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

	ui->cur_lun = GET_WD_REG(ibp, W95_DPR_R(WCS_I_ID_IN)) & (SCSI_MAXLUN-1);
	li = ui->lun_info[ui->cur_lun];
	ASSERT(li);

	/*
	 * Workaround for WD95 bug - need to issue KILL to chip after ID message
	 * in after reselection, else chip's internal state will be wrong and
	 * self inconsistent regarding number of rising REQ edges seen.
	 */
	flags = 0;
	SET_WD_REG(ibp, WD95A_N_CTL, W95_CTL_KILL);
	while (!(GET_WD_REG(ibp, WD95A_N_STOPU) & W95_STPU_ABORTI))
		if(flags++ == 100) {
			wd95reset(ci, ui, wreq, "reselection failure\n");
			return;
		}
	SET_WD_REG(ibp, WD95A_N_STOPU, 1);

	flags = 0;
	if ((wreq = li->wqueued) == NULL) {
		wd95reset(ci, ui, wreq, "unexpected reselect");
		return;
	}
	req = wreq->sreq;
	if (req->sr_buflen) {
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
#if ODD_BYTE_WAR 
	    else if (req->sr_ha_flags & WFLAG_ODDBYTEFIX)
		tc = dma_map2(dmamap, (caddr_t) startaddr, (caddr_t) ci->page,
		              wreq->resid);
#endif
	    else
		tc = dma_map(dmamap, (caddr_t) startaddr, wreq->resid);

	    /*
	     * When true, workaround a nasty wd95 bug by doing only as many
	     * bytes as we are guaranteed to be able to do without getting
	     * the Ibus.  0x100 - offset is the number of bytes that the S1
	     * and wd95 should be able to buffer before needing the Ibus.
	     */
	    wd95war_count = 0x100 - (startaddr & 0x7F);
	    if (((ui->sync_off & 0x1F) == 0) &&
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
	    {
		tc = wreq->resid;
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
	    scsi_go(dmamap, (caddr_t) startaddr, req->sr_flags & SRF_DIR_IN);
	}
	SET_WD_REG(ibp, WD95A_N_FLAG, flags);
	SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
}

void
wd95_backout(register wd95ctlrinfo_t *ci, register wd95unitinfo_t *ui,
		wd95request_t *wreq, register scsi_request_t *req)
{
	register volatile char *ibp = ci->ha_addr;

	/* back out current request */

	s1dma_flush(ci->d_map);

#if ODD_BYTE_WAR
	if (req->sr_ha_flags & WFLAG_ODDBYTEFIX) {
		req->sr_ha_flags &= ~WFLAG_ODDBYTEFIX;
		ci->pagebusy = 0;
	}
#endif
	if ( !(req->sr_ha = ui->reqhead) )
		ui->reqtail = req;
	ui->reqhead = req;
	wd_rm_wq(ui, wreq);
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
	volatile char *ibp = ci->ha_addr;
	
	ASSERT(ci->active);
	{
		if (s1dma_flush(ci->d_map))
			req->sr_status = SC_HARDERR;
		wreq->starting = 0;
		if (wreq->tlen) {
		    wreq->resid -= wreq->tlen - rtc;
		    wreq->tlen = 0;
		}
	}
	ci->active = 0;
	ci->cur_target = ci->host_id;
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
	wd95request_t *wreq, struct scsi_request *req, int rtc, volatile char *ibp)
{
	char	status;

	if (s1dma_flush(ci->d_map))
	    req->sr_status = SC_HARDERR;
	status = GET_WD_REG(ibp, W95_DPR_R(WCS_I_STATUS));
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

void
wd95intr(int ctlr, struct eframe_s *ep)
{
	register wd95ctlrinfo_t	*ci;
	register wd95unitinfo_t	*ui;
	register wd95luninfo_t	*li;
	register wd95request_t	*wreq;
	register scsi_request_t	*req;
	volatile char		*ibp;
	int			rtc;
	int			cstat, sqaddr = 0;
	int			msg_type, flags;

	ci = &wd95dev[ctlr];
	ibp = ci->ha_addr;

	SET_WD_REG(ibp, WD95A_S_CTL, 0);
	while ((cstat = GET_WD_REG(ibp, WD95A_N_ISR)) & W95_ISR_INT0) {

	    /*
	     * Now determine which target (or unit)
	     * it is associated with.
	     */
	    if (ci->cur_target != ci->host_id) {
		    ui = ci->unit[ci->cur_target];
		    li = ui->lun_info[ui->cur_lun];
		    wreq = li->wqueued;
		    ASSERT(wreq);
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
		/*
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
		    if (wreq->resid > 0x100)
		    {
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
	    }
	    if (cstat & W95_ISR_VBUSYI) {
		ASSERT(0);
		SET_WD_REG(ibp, WD95A_N_ISR, W95_ISR_VBUSYI);
	    }
	    if (cstat & (W95_ISR_STOPU | W95_ISR_UEI)) {
		int s_status, e_status;

		s_status = GET_WD_REG(ibp, WD95A_N_STOPU);
		e_status = GET_WD_REG(ibp, WD95A_N_UEI);

		wdiprintf((" stpu 0x%x, uei 0x%x", s_status, e_status));
		if (s_status & W95_STPU_SCSIT) {
			SET_WD_REG(ibp, WD95A_N_STOPU, W95_STPU_SCSIT);
			req->sr_status = SC_TIMEOUT;
			s1dma_flush(ci->d_map);
			if (wd95inited)
				printf("wd95_%dd%d: does not respond, selection timeout\n",
				       WD_C_B(ci->number), ui->number);
			wd95complete(ci, ui, wreq, req);
		}
		if (e_status & W95_UEI_TCOVR) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_TCOVR);
			SET_WD_REG(ibp, WD95A_N_CTL, W95_TC_CLR);
		}

		/*
		 * Extraneous bits that we don't know what to do with yet.
		 */
		if (e_status & W95_UEI_UDISC) {
			int	status;

			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_UDISC);
			sqaddr = GET_WD_REG(ibp, WD95A_N_SQADR);
			switch (sqaddr) {
			    case 0x32:
				wd95_proc_disc(ci, ui, wreq, rtc);
				break;

			    case 0x4d:
				wd95finish(ci, ui, wreq, req, rtc, ibp);
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
		if (e_status & W95_UEI_RSTINTL) {
			SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_RSTINTL);
			printf("wd95_%d:  WD95 saw SCSI reset\n",
				WD_C_B(ctlr));
		}
	    }
	    if (cstat & (W95_ISR_STOPWCS | W95_ISR_INTWCS)) {

		SET_WD_REG(ibp, WD95A_N_ISR, W95_ISR_STOPWCS | W95_ISR_INTWCS);

		sqaddr = GET_WD_REG(ibp, WD95A_N_SQADR);
		wdiprintf((" sqaddr 0x%x", sqaddr));
		switch (sqaddr) {
		case WCS_I_COMPLETE:
		    wd95finish(ci, ui, wreq, req, rtc, ibp);
		    break;

		case WCS_I_DISC:
		    wd95_proc_disc(ci, ui, wreq, rtc);
		    break;

		case WCS_I_RSLCONT:
		    wd95rsel(ci);
		    break;

		case WCS_I_UNK_MSG:
		    {
		    int nsq;
		    char msg_in;

		    msg_in = GET_WD_REG(ibp, W95_DPR_R(WCS_I_MSG_IN));
		    nsq = WCS_I_DISPATCH;
		    switch (msg_in) {
			case MSG_SAVE_DP:
#if 0 /* XXX */
			    X## need to set verified resid from tentative resid
#endif
			    break;
			case MSG_RESTORE_DP:
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

			if (showconfig) {
			    printf("wd95_%dd%d: sync per %d, off %d, SPW %x\n",
				    WD_C_B(ctlr), ci->cur_target, period,
				    offset, ui->sync_per);
			}
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
			SET_WD_REG(ibp, WD95A_N_SQADR, WCS_I_DISPATCH);
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
				if (fifo == 1) {
				    if (req->sr_flags & SRF_MAPBP)
					req->sr_status = SC_HARDERR;
				    else {
					fifo = GET_WD_REG(ibp, WD95A_N_DATA);
					loc = (req->sr_buflen - wreq->resid) +
					      wreq->tlen - rtc;

#if ODD_BYTE_WAR
					if ((req->sr_ha_flags & WFLAG_ODDBYTEFIX) &&
					    (loc >= ctob(btoct(
					     poff(req->sr_buffer) + req->sr_buflen))))
					{
						loc = poff(req->sr_buffer + loc);
						ci->page[loc] = fifo;
					}
					else
#endif /* ODD_BYTE_WAR */
						req->sr_buffer[loc] = fifo;
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
#if 1 /* XXX Do we want this? */
	wd95start(ci, ci->unit[ci->round_robin]);
#endif

wd95intr_end:
	wdiprintf(("	leaving wd95intr\n"));
	svsema(ci->lock);
}


#define	MSG_IDENT	0x80		/* identify message value */
#define	MSG_DISC_PRIV	0x40		/* disconnect priv bit */
#define	MSG_EXT		0x01		/* extended message byte */
#define	MSG_SDTR_LEN	3		/* sync request len */

static void
wd95a_do_cmd(register wd95unitinfo_t *ui, register wd95request_t *wreq)
{
	register wd95ctlrinfo_t *ci = ui->ctrl_info;
	register volatile char *sbp = ci->ha_addr;
	register scsi_request_t	 *sp = wreq->sreq;
	int	len = sp->sr_cmdlen;
	int	dpr = WCS_I_CMD_BLK;
	u_char	*cp = sp->sr_command;
	int	flags, msg, sqaddr, cmd, cstat;

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
#if 0
	if (ui->disc) {
	    msg |= MSG_DISC_PRIV;
	}
#endif
	SET_WD_REG(sbp, W95_DPR_R(WCS_I_ID_OUT), msg);

	SET_WD_REG(sbp, WD95A_N_SPW, ui->sync_per);
	SET_WD_REG(sbp, WD95A_N_STC, ui->sync_off);
	sqaddr = ui->sel_tar;
	if (ui->wide_en) {
	    if (ui->wide_req && ! ui->ext_msg && (cmd != SCSI_REQ_SENSE) &&
			 (cmd != SCSI_INQ_CMD)) {
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT_L), 4);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+1), MSG_EXT);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+2), 2);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+3), MSG_WIDE);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+4), MSG_WIDE_16);
		ui->ext_msg = 1;
		flags |= WCS_I_MULTI_MSG;
		sqaddr = WCS_I_SEL_EXT;
	    }
	}
	if (ui->sync_en) {
	    if (ui->sync_req && ! ui->ext_msg && (cmd != SCSI_REQ_SENSE) &&
			(cmd != SCSI_INQ_CMD)) {
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT_L), 5);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+0), MSG_EXT);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+1), MSG_SDTR_LEN);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+2), MSG_SDTR);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+3), ci->sync_period);
		SET_WD_REG(sbp, W95_DPR_R(WCS_I_MSG_OUT+4),
			 (ui->wide_en ? (ci->sync_offset/2) : ci->sync_offset));
		ui->ext_msg = 1;
		flags |= WCS_I_MULTI_MSG;
		sqaddr = WCS_I_SEL_EXT;
	    }
	}

	if (sp->sr_buflen) {
	    int	    tc;
	    int	    wd95war_count;

	    /*
	     * When true, workaround a nasty wd95 bug by doing only as many
	     * bytes as we are guaranteed to be able to do without getting
	     * the Ibus.  0x100 - offset is the number of bytes that the S1
	     * and wd95 should be able to buffer before needing the Ibus.
	     * If this sp is of type map_bp, then sr_buffer should be 0.
	     */
	    wd95war_count = 0x100 - ((uint) sp->sr_buffer & 0x7F);
	    if (((ui->sync_off & 0x1F) == 0) &&
		(sp->sr_flags & SRF_DIR_IN) &&
		(wreq->resid > wd95war_count))
	    {
		tc = wreq->tlen = wd95war_count;
		SET_WD_REG(sbp, WD95A_N_TC_0_7, (tc & 0xff));
		if (tc > 0xff)
		    SET_WD_REG(sbp, WD95A_N_TC_8_15, ((tc >> 8) & 0xff));
		SET_WD_REG(sbp, WD95A_N_CTL, W95_CTL_LDTC);
		SET_WD_REG(sbp, WD95A_N_ISRM, 0x7F);
	    }
	    else
	    {
		tc = sp->sr_buflen;
		if ((ui->sync_off & 0x1F) && (tc & 1))
			tc++;
		wreq->tlen = tc;
		SET_WD_REG(sbp, WD95A_N_TC_0_7, (tc & 0xff));
		if (tc > 0xff) {
		    SET_WD_REG(sbp, WD95A_N_TC_8_15, ((tc >> 8) & 0xff));
		    if (tc > 0xffff)
			SET_WD_REG(sbp, WD95A_N_TC_16_23, ((tc >> 16) & 0xff));
		}
		SET_WD_REG(sbp, WD95A_N_CTL, W95_CTL_LDTCL);
		SET_WD_REG(sbp, WD95A_N_ISRM, 0x6F);
	    }
	    flags |= WCS_I_DATA_OK |
		     ((sp->sr_flags & SRF_DIR_IN) ? WCS_I_DIR : 0);
	    scsi_go(ci->d_map, sp->sr_buffer, sp->sr_flags & SRF_DIR_IN);
	}

	wdcprintf(("flags 0x%x\n", flags));
	SET_WD_REG(sbp, WD95A_N_FLAG, flags);
	SET_WD_REG(sbp, WD95A_N_DESTID, sp->sr_target);
	SET_WD_REG(sbp, WD95A_N_SQADR, sqaddr);
}


/*
 * Returns 0 if chip is okay and ready.
 */
int
init_wd95a(register volatile char *sbp, int host_id, register wd95ctlrinfo_t *ci)
{
	register u_int tmp;

	/* go to setup mode */
	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);

	/*
	 * Run BIST
	 */
#define FLAG_CHECK 0
#if FLAG_CHECK
	SET_WD_REG(sbp, WD95A_S_TEST0, 0x43);
	DELAY(10);
	if (GET_WD_REG(sbp, WD95A_S_TEST1) != 0x3F)
{
debug(0);
		return 1;
}
	SET_WD_REG(sbp, WD95A_S_TEST0, 0x42);
	for (tmp = GET_WD_REG(sbp, WD95A_S_TEST1);
	     !((tmp & 0x3) && (tmp & 0xC) && (tmp & 0x30));
	     tmp = GET_WD_REG(sbp, WD95A_S_TEST1));
	if (tmp != 0x2A)
{
debug(0);
		return 1;
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

int
load_95a_wcs(register volatile char *sbp, int wcs_type)
{
	unsigned long *up;
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

	    default:
		return -1;
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

	return 0;
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

static void
wd95_abort(register volatile char *ibp)
{
	int new_stat;
#ifdef WDI_LOG_CNT
	wd95ctlrinfo_t *ci = NULL;
#endif

	SET_WD_REG(ibp, WD95A_N_CTL, W95_CTL_ABORT);
	do {
		DELAY(1);
		new_stat = GET_WD_REG(ibp, WD95A_N_STOPU);
	} while (! (new_stat & W95_STPU_ABORTI));

	SET_WD_REG(ibp, WD95A_N_STOPU, W95_STPU_ABORTI);
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
	char *cp;
	ushort *sp;
	int lcnt;
	char name[9];

	if (wd95cnt == 0)
		return 0;

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

	/* Set the scsi host id */
	if ((cp = getenv("scsihostid")) != NULL)
	        ci->host_id = atoi(cp);
	else
	        ci->host_id = 0;

	/* Make sure we don't screw things up if the host ID is invalid */
	if (ci->host_id < 0 || ci->host_id > 7) 
	        ci->host_id = 0;

	if (!wd95_present(ci->ha_addr))
		goto err_init;

	ci->d_map = dma_mapalloc(DMA_SCSI, adap, 0, 0);
	if(ci->d_map == ((dmamap_t *)-1))
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

	init_sema(&ci->d_sleepsem, 0, "dps", adap);
	init_sema(&ci->d_onlysem, 0, "dpo", adap);
	initnlock(&ci->qlock, makesname(name, "dpq", adap));

	ci->round_robin = 0;
	ci->max_round = WD95_LUPC;

	return 0;	/* for ide; */

err_init:
	printf("Error occurred in earlyinit\n");
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

wd95_intpresent(wd95ctlrinfo_t *ci)
{
	register volatile char *ibp = ci->ha_addr;
	int cstat;

	cstat = GET_WD_REG(ibp, WD95A_N_ISR);
	return (cstat & W95_ISR_INT0) ? 1 : 0;
}


void
scsi_setsyncmode(scsisubchan_t *sp, int enable)
{
}


void
wd95set_seltimeout(int adap, int tout)
{
	volatile char *sbp = scuzzy[adap].d_addr;

	SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
	SET_WD_REG(sbp, WD95A_S_TIMOUT, tout);
	SET_WD_REG(sbp, WD95A_S_CTL, 0);
}


scsisubchan_t *subchan[SCSI_MAXCTLR][SCSI_MAXTARG];

scsisubchan_t *
allocsubchannel(adap, target, lu)
{
	scsisubchan_t *sp;

	if (adap >= SCSI_MAXCTLR || target >= SCSI_MAXTARG || lu != 0)
		return NULL;
	if (subchan[adap][target] != NULL)
		return NULL;
	if (wd95alloc(adap, target, lu, 1, NULL) == 0)
		return NULL;

	subchan[adap][target] = kern_calloc(sizeof(scsisubchan_t), 1);
	sp = subchan[adap][target];
	sp->s_adap = adap;
	sp->s_target = target;
	sp->s_lu = lu;

	return sp;
}

void
freesubchannel(sp)
register scsisubchan_t *sp;
{
	wd95free(sp->s_adap, sp->s_target, sp->s_lu, NULL);
	subchan[sp->s_adap][sp->s_target] = NULL;
	kern_free(sp);
}


void
doscsi(register scsisubchan_t *sp)
{
	struct scsi_request req;
	char command[10];
	bzero(&req, sizeof(req));
	req.sr_ctlr = sp->s_adap;
	req.sr_target = sp->s_target;
	req.sr_lun = sp->s_lu;
	req.sr_command = sp->s_cmd;
	req.sr_cmdlen = sp->s_len;
	req.sr_flags = SRF_AEN_ACK;
	if (sp->s_buf->b_flags & B_READ)
		req.sr_flags |= SRF_DIR_IN;
	req.sr_timeout = sp->s_timeoutval;
	req.sr_buffer = sp->s_buf->b_dmaaddr;
	req.sr_buflen = sp->s_buf->b_bcount;

	wd95command(&req);
	sp->s_buf->b_resid = req.sr_resid;
	sp->s_error = req.sr_status;
	sp->s_status = req.sr_scsi_status;
	if (sp->s_notify)
		(*sp->s_notify)(sp);
}


int sc_inq_stat;

/* scsi early command */
static caddr_t
scsi_ecmd(u_char adap, u_char targ, u_char lun,
	  u_char *inquiry, int il,
	  void *data, int dl)
{
	register scsisubchan_t *sp;
	int first = 1;
	buf_t *bp;
	caddr_t ret;

	if((sp = allocsubchannel(adap, targ, lun)) == NULL ||
		(bp = (buf_t *)kern_calloc(sizeof(*bp), 1)) == NULL)
		return NULL;

	sp->s_notify = NULL;
	sp->s_cmd = inquiry;
	sp->s_len = il;
	sp->s_buf = bp;
	/* SCSI 2 spec strongly recommends all devices be able to
	 * respond successfully to inq within 2 secs of power on.
	 * some ST4 1200N (1.2) drives occasionally accept the cmd
	 * and disc, but don't come back within 1 second, even when
	 * motor start jumper is on, at powerup.   Only seen on IP12,
	 * probably because it gets to the diag faster.  */
	sp->s_timeoutval = 2*HZ;
again:
	/*  So we don't keep data left from an earlier call.  Also
	 * insures null termination. */
	bzero(data, dl);

	bp->b_dmaaddr = (caddr_t) data;
	bp->b_bcount = dl;
	bp->b_flags = B_READ|B_BUSY;
	doscsi(sp);
	if(sp->s_error == SC_GOOD && sp->s_status == ST_GOOD)
		ret = (caddr_t) data;
	else if (first-- && sp->s_error != SC_TIMEOUT) {
		us_delay(1000);
		goto again;     /* one retry, if it not a timeout. */
	}
	else
		ret = NULL;
	sc_inq_stat = sp->s_error;	/* mainly for standalone ide */
	freesubchannel(sp);
	kern_free((caddr_t)bp);

	return(ret);
}


static int
scsi_rc(u_char adap, u_char targ, u_char lun, struct scsidisk_data *d)
{
	unsigned int *dc;
	u_char inq[10];

	bzero(inq,10);
	inq[0] = 0x25;
	dc = dmabuf_malloc(sizeof(unsigned int) * 2);

	scsi_ecmd(adap,targ,lun,inq,10,dc,8);

#ifdef _MIPSEL
	swapl(dc,2);
#endif
	d->MaxBlocks = dc[0];
	d->BlockSize = dc[1];

	dmabuf_free(dc);
	return(0);
}


union scsicfg {
	struct scsidisk_data disk;
	struct floppy_data fd;
};

static COMPONENT scsiadaptmpl = {
	AdapterClass,			/* Class */
	SCSIAdapter,			/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	9,				/* IdentifierLength */
	"WD33C95A\000"			/* Identifier */
};
static COMPONENT scsitmpl, scsictrltmpl;

void
scsiinstall(COMPONENT *c)
{
	int adap = c->Key;
	struct scsi_target_info *info;
	char *which, *extra, *inv;
	u_char targ, lu;
	COMPONENT *tmp, *tmp2;
        union scsicfg cfgd;
        char id[26];            /* 8 + ' ' + 16 + null */
        int type;
	int dksc_strat();
	int tpsc_strat();
	extern int multi_io4;

	if (multi_io4)
		printf("Scanning SCSI adapter %d for devices...\n", adap);

	lu = 0;

	/*
	 * Probe for devices, and add controller to inventory.  We
	 * set the timeout value fairly low to decrease the overall
 	 * probe time.
	 */
	wd95set_seltimeout(adap, TIMEOUT_FAST);
	for (targ = 1; targ < WD95_LUPC; targ++) {

		wd95inited = 0;
		if ((info = wd95info(adap, targ, 0)) == NULL)
			continue;
		wd95inited = 1;
		inv = info->si_inq;
		/* ARCS Identifier is "Vendor Product" */
		strncpy(id,inv+8,8); id[8] = '\0';
		for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
		strcat(id," ");
		strncat(id,inv+16,16); id[25] = '\0';
		for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;

		scsictrltmpl.IdentifierLength = strlen(id);
		scsictrltmpl.Identifier = id;

		switch(inv[0]) {
		case 0:	/* hard disk */
		    extra = "disk";
		    which = (inv[1] & 0x80) ? "removable " : "";

		    scsictrltmpl.Type = DiskController;
		    scsictrltmpl.Key = targ;
		    tmp = AddChild(c, &scsictrltmpl, (void *) NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("disk ctrl");

		    if (inv[1]&0x80) {
			    scsitmpl.Type = FloppyDiskPeripheral;
			    scsitmpl.Flags = Removable|Input|Output;
			    scsitmpl.ConfigurationDataSize =
				    sizeof(struct floppy_data);
		    }
		    else {
			    scsitmpl.Type = DiskPeripheral;
			    scsitmpl.Flags = Input|Output;
			    scsitmpl.ConfigurationDataSize =
				    sizeof(struct scsidisk_data);
		    }
	 	    bzero(&cfgd.disk,sizeof(struct scsidisk_data));
                    scsi_rc(adap,targ,lu,&cfgd.disk);

		    cfgd.disk.Version = SGI_ARCS_VERS;
		    cfgd.disk.Revision = SGI_ARCS_REV;
		    cfgd.disk.Type = SCSIDISK_TYPE;
		    scsitmpl.Key = lu;

		    tmp = AddChild(tmp, &scsitmpl, (void *)&cfgd);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("scsi disk");
		    RegisterDriverStrategy(tmp,dksc_strat);
		    break;

		case 1:	/* sequential == tape */
		    type = tpsctapetype(inv, 0);
		    which = "tape";
		    extra = inv;
		    sprintf(inv, " type %d", type);

		    scsictrltmpl.Type = TapeController;
		    scsictrltmpl.Key = targ;
		    tmp = AddChild(c,&scsictrltmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("tape ctrl");

		    scsitmpl.Type = TapePeripheral;
		    scsitmpl.Flags = Removable|Input|Output;
		    scsitmpl.ConfigurationDataSize = 0;
		    scsitmpl.Key = lu;

		    tmp = AddChild(tmp,&scsitmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("scsi tape");
		    RegisterDriverStrategy(tmp,tpsc_strat);
		    break;
		default:
		    type = inv[1] & INV_REMOVE;
		    /* ANSI level: SCSI 1, 2, etc.*/
		    type |= inv[2] & INV_SCSI_MASK;

		    if ((inv[0]&0x1f) == INV_CDROM) {
			    scsictrltmpl.Type = CDROMController;
			    scsitmpl.Type = DiskPeripheral;
			    scsitmpl.Flags = ReadOnly|Removable|Input;
			    which = "CDROM";
		    }
		    else {
			    scsictrltmpl.Type = OtherController;
			    scsitmpl.Type = OtherPeripheral;
			    scsitmpl.Flags = 0;
			    sprintf(which=inv, " device type %u",
					inv[0]&0x1f);
		    }

		    scsitmpl.ConfigurationDataSize = 0;
		    scsictrltmpl.Key = targ;
		    tmp = AddChild(c,&scsictrltmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("other ctrl");

		    scsitmpl.Key = lu;

		    tmp2 = AddChild(tmp,&scsitmpl,(void *)NULL);
		    if (tmp2 == (COMPONENT *)NULL) cpu_acpanic("scsi device");

		    if (tmp->Type == CDROMController)
			RegisterDriverStrategy(tmp2,dksc_strat);

		    extra = "";
		    break;
		}
		if(showconfig) {
		    cmn_err(CE_CONT,"SCSI %s%s (%d,%d", which,
			extra?extra:"", adap, targ);
		    if(lu)
			cmn_err(CE_CONT, ",%d", lu);
		    cmn_err(CE_CONT, ")\n");
		}
	}

	wd95set_seltimeout(adap, TIMEOUT_INIT);

	wd95inited = 1;
	/* We now make a pass through all of targets again,
	 * reverting anything that looks like a Toshiba CD ROM.
	 */
	revert_walk(GetChild(c));
}


/* probe if controller chip is present
 */
static void
scsihinv(COMPONENT *c, u_char adap)
{
	{
	    c = AddChild(c,&scsiadaptmpl,(void *)NULL);
	    if (c == (COMPONENT *)NULL) cpu_acpanic("scsi adapter");
	    c->Key = adap;

	    /* fill in unchanged parts of templates */
	    bzero(&scsictrltmpl,sizeof(COMPONENT));
	    scsictrltmpl.Class = ControllerClass;
	    scsictrltmpl.Version = SGI_ARCS_VERS;
	    scsictrltmpl.Revision = SGI_ARCS_REV;
	    scsitmpl.Class = PeripheralClass;
	    scsitmpl.Version = SGI_ARCS_VERS;
	    scsitmpl.Revision = SGI_ARCS_REV;
	    scsitmpl.AffinityMask = 0x01;

	    RegisterInstall(c,scsiinstall);
	}
}


/*
 * Probe the controller and find out what SCSI devices are attached.
 * Set up work queues.
 */
void
scsiedtinit(COMPONENT *c)
{
	wd95ctlrinfo_t	*ci;
	wd95unitinfo_t	*ui;
	wd95luninfo_t	*li;
	volatile short	*ptr;
	int		adap;
	unchar		ipl;
	char		name[9];
	int		i, j;
	int		cver;
	volatile char*  sbp;

	wd95inited = 0;	/* disable printing of selection timeouts */
	us_delay(500000);
	for (adap = 0; adap < wd95cnt; adap++) {
		ci = &wd95dev[adap];
		if ( !ci->ha_addr )
			continue;	/* no adapter here */

		ci->number = adap;

                /*
                 * Check/set transfer rate, validate connections, etc.
                 */
                sbp = ci->ha_addr;
                SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
                cver = GET_WD_REG(sbp, WD95A_S_CVER);
                ci->sync_period = 50;
                if (cver & W95_CVER_SE) {
                        ci->diff = 0;
                } else {
                        ci->diff = 1;
                        if ( !(cver & W95_CVER_DSENS) ) {
                            printf("wd95_%d: differential bus with single"
				   " ended drive connected - bypassing\n", 
				   WD_C_B(adap));
                            ci->ha_addr = NULL;
                            continue;
                        }
		}

		ci->sync_offset = MAX_OFFSET;
		if ( ! (WD95_DEFAULT_FLAGS & WD95BUS_WIDE) ) {
                        ci->narrow = 1;
                }

		/*
		 * initialize SW structures
		 */
		initnlock(&ci->lock, makesname(name, "wd95", adap));
		initnlock(&ci->auxlock, makesname(name, "wga", adap));
		ci->page = kvpalloc(1, VM_DIRECT, 0);
		for (i = 0; i < WD95_LUPC; i++) {
			ci->unit[i] = ui = &wd95unitinfo[adap * WD95_LUPC + i];
			ui->ctrl_info = ci;
			ui->number = i;
			ui->sync_per = SPW_SLOW;
			ui->sync_off = 0;
			ui->sel_tar  = WCS_I_SEL;

			init_sema(&ui->start, 1, "wus", adap*16 + i);
			init_sema(&ui->done, 0, "wud", adap*16 + i);
			ui->reqhead = ui->reqtail = NULL;
			ui->wrfree = NULL;
			li = ui->lun_info[0] = &ui->lun_0;
			li->wqueued = NULL;
			for (j = 0; j < WD95_QDEPTH; j++) {
				ui->wreq[j].ctlr = adap;
				ui->wreq[j].unit = i;
				ui->wreq[j].next = ui->wrfree;
				ui->wrfree = &ui->wreq[j];
			}
			ui->wreq_cnt = WD95_QDEPTH;
			li->u_sense_req = kmem_alloc(sizeof(scsi_request_t) +
						SC_CLASS0_SZ, KM_CACHEALIGN);
                        {
                            register scsi_request_t *sprq = li->u_sense_req;

                            sprq->sr_ctlr = adap;
                            sprq->sr_target = i;
                            sprq->sr_lun = 0;
                            sprq->sr_command = (u_char *)&sprq[1];
                            sprq->sr_cmdlen = SC_CLASS0_SZ; /* never changes */
                            sprq->sr_timeout = 30*HZ;   /* never changes */
                        }
		}

                /*
                 * Reset SCSI bus on adapters that the PROM doesn't touch.
                 * Also go to normal mode and enable interrupts.
                 */
                SET_WD_REG(sbp, WD95A_S_CTL, 0);
#if 0
		wd95busreset(ci);
#endif
                SET_WD_REG(sbp, WD95A_N_ISRM, ISRM_INIT);

                ci->present = 1;
		scsihinv(c, ci->number);
	}
}

#endif /* defined(EVEREST) */
