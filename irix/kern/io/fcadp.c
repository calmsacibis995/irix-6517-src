/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#define FCADPDEBUG 0
#define LOGIN_QUIESCE 1
#define FCADP_SPINLOCKS 1

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/reg.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/var.h>
#include <sys/scsi.h>
#include <sys/invent.h>
#include <sys/failover.h>
#include <sys/buf.h>
#include <sys/pfdat.h>
#include <sys/atomic_ops.h>
#include <sys/graph.h>

#include "sys/PCI/pciio.h"
#include "sys/pio.h"
#include "sys/PCI/pcibr.h"
#include <sys/iograph.h>

/*
 * Adaptec has somewhat fewer tiny header files.
 */
#include <io/adpfc/hwgeneral.h>
#include <io/adpfc/hwcustom.h>
#include <io/adpfc/hwtcb.h>
#include <io/adpfc/ulmconfig.h>
#include <io/adpfc/hwdef.h>
#include <io/adpfc/hwproto.h>

/*
 * SGI driver header file
 */
#include <sys/fcadp.h>
#include <sys/fcal.h>

/*
 * Driver variables
 */
ushort			 fcadp_maxadap;
struct fcadpctlrinfo	*fcadpctlr[FCADP_MAXADAP];
mutex_t			 fc_attach_sema;
int			 fcadpdevflag = D_MP; /* needed to get fcadpinit called */
int			 fcadp_init_time = 0;
extern int		 fcadp_hostid;
extern int		 fcadp_host_unfair;

/*
 * Forward declarations
 */
static void fc_queue_target(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti);
static void fc_dequeue_target_scsi(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti);
static int fc_getmapsize(struct scsi_request *req);
static int fcadp_ctlrinit(struct fcadpctlrinfo *ci, int alloc);
static void fc_login(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti);
static void fc_lip(struct fcadpctlrinfo *ci, int wait, uint lip_option, uint8_t target);
void fc_stop_quiesce(struct fcadpctlrinfo *ci);
void fcadpfree(vertex_hdl_t lun_vhdl, void (*cb)());

/*
 * structure used to access PCI devices
 */
struct fcadp_pciconfig
{
	u_int16_t	PCI_devID, PCI_vendID;
	u_int16_t	PCI_status, PCI_command;
	u_int8_t	PCI_class_code[3], PCI_revID;
	u_int8_t	rsvd1, PCI_header_type,
			PCI_latency_timer, PCI_cache_line;
	u_int32_t	PCI_base0;
	u_int32_t	PCI_base1;
	u_int32_t	PCI_base2;
	u_int32_t	PCI_base3;
	u_int32_t	PCI_base4;
	u_int32_t	PCI_base5;
	u_int32_t	unused1;
	u_int32_t	unused2;
	u_int32_t	PCI_rombase;
	u_int32_t	unused3;
	u_int32_t	unused4;
	u_int8_t	PCI_max_latency, PCI_min_grant,
			PCI_intr_pin, PCI_intr_line;
	u_int8_t	PCI_errorgen, PCI_intlinsel1,
			PCI_devstatus0, PCI_devconfig;
};

/*
 * Configuration parameters
 */
#if PCI_BIGENDIAN
#define FCADP_ENDIAN PCIIO_WORD_VALUES
#else
#define FCADP_ENDIAN PCIIO_BYTE_STREAM
#endif
#define FCADP_PCIDMA_FLAGS \
	( PCIBR_NOWRITE_GATHER | PCIBR_NOPREFETCH | PCIBR_NOPRECISE | \
	  PCIBR_BARRIER | PCIBR_VCHAN0 | PCIIO_DMA_A64 | FCADP_ENDIAN)
#define FCADP_PCIDMA_DATA_FLAGS \
	( PCIBR_NOWRITE_GATHER | PCIBR_PREFETCH | PCIBR_NOPRECISE | \
	  PCIBR_NOBARRIER | PCIBR_VCHAN0 | PCIIO_DMA_A64 | FCADP_ENDIAN)
#define kvtopci(X) kvtopciaddr(ci, X)
#define kvtopci_data(X) kvtopciaddr_data(ci, X)
#define phystopci_data(X) \
	pcibr_dmatrans_addr(ci->pcivhdl, 0, X, 0, FCADP_PCIDMA_DATA_FLAGS)
uint64_t kvtopciaddr(struct fcadpctlrinfo *, void *);
uint64_t kvtopciaddr_data(struct fcadpctlrinfo *, void *);


/*
 * Spinlocks and mutexes both supported as compile time options
 */
#if FCADP_SPINLOCKS
#define INITLOCK(l,n,s) init_spinlock(l, n, s)
#define IOLOCK(l,s)     s = io_splock(l)
#define IOUNLOCK(l,s)   io_spunlockspl(l, s)
#else
#define INITLOCK(l,n,s) init_mutex(l, MUTEX_DEFAULT, n, s)
#define IOLOCK(l,s)     mutex_lock(&(l), PRIBIO)
#define IOUNLOCK(l,s)   mutex_unlock(&(l))
#endif

/*
 * Used to keep track of last N commands issued.
 */
#define FCADP_CMD_DEBUG 0
#if FCADP_CMD_DEBUG
#define COMMAND_INDEX_SIZE 4096
struct
{
	ushort	tcb_number;
	u_char	adap_num;
	u_char	targ_num;
	u_char	scsicmd[10];
	u_char  sensegotten;
} command_list[COMMAND_INDEX_SIZE];
int command_index = 0;
#endif

/*
 * Allows ASSERT to put us into the debugger
 */
#undef ASSERT
#ifdef DEBUG
#define ASSERT(EX) if(!(EX)){printf("ASSERT FAIL "); printf(#EX"\n"); debug("ring");}
#else
#define ASSERT(EX)
#endif


static char *fc_done_status[] =
{
/*  0 */ "",
/*  1 */ "data overrun",
/*  2 */ "data underrun",
/*  3 */ "TCB parity error",
/*  4 */ "FCP response",
/*  5 */ "frame received out of order",
/*  6 */ "frame format error",
/*  7 */ "TCB maximum count exceeded",
/*  8 */ "target did not respond / no such target",
/*  9 */ "SOF error",
/* 10 */ "EOF error", 
/* 11 */ "TCB aborted",
};


uint32_t
int16_le(u_char *ptr)
{
	return (ptr[1] << 8) + ptr[0];
}

uint32_t
int24_le(u_char *ptr)
{
	return (ptr[2] << 16) + (ptr[1] << 8) + ptr[0];
}

uint32_t
int32_le(u_char *ptr)
{
	return (ptr[3] << 24) + (ptr[2] << 16) + (ptr[1] << 8) + ptr[0];
}

void
fc_cerr(struct fcadpctlrinfo *ci, char *fmt, ...)
{
	va_list		 ap;
	char		 buffer[512];

	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);
	if (strncmp(ci->hwgname, "fcadp", 5) == 0)
	{
		cmn_err(CE_CONT, "%s (%d): %s", ci->hwgname,
			ci->number, buffer);
	}
	else
	{
		cmn_err(CE_CONT, "%s (%d):\n  %s", ci->hwgname,
			ci->number, buffer);
	}
}

void
fc_terr(struct fcadpctlrinfo *ci, int target, char *fmt, ...)
{
	va_list		 ap;
	char		 buffer[256];

	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);
	if (strncmp(ci->hwgname, "fcadp", 5) == 0)
	{
		cmn_err(CE_CONT, "%st%d (%d): %s", ci->hwgname,
			target, ci->number, buffer);
	}
	else
	{
		cmn_err(CE_CONT, "%s (%d) t%d:\n  %s", ci->hwgname,
			ci->number, target, buffer); 
	}
}

void
fc_lerr(struct fcadpctlrinfo *ci, int target, int lun, char *fmt, ...)
{
	va_list		 ap;
	char		 buffer[256];

	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);
	if (strncmp(ci->hwgname, "fcadp", 5) == 0)
	{
		cmn_err(CE_CONT, "%st%dl%d (%d): %s", ci->hwgname,
			target, lun, ci->number, buffer);
	}
	else
	{
		cmn_err(CE_CONT, "%s (%d) t%dl%d:\n  %s", ci->hwgname,
			ci->number, target, lun, buffer); 
	}
}

/*
 * Debugging print facility
 */
extern int	fcadpdebug;
void
DBG(int debuglevel, char *format, ...)
{
	va_list ap;
	char tempstr[200];

	if (fcadpdebug >= debuglevel)
	{
		va_start(ap, format);
		vsprintf(tempstr, format, ap);
		printf(tempstr);
		va_end(ap);
		if (fcadpdebug > 10)
			fcadpdebug--;
	}
}

void
DBGC(int debuglevel, struct fcadpctlrinfo *ci, char *fmt, ...)
{
	va_list		 ap;
	char		 buffer[256];

	if (fcadpdebug < debuglevel)
		return;
	if (fcadpdebug > 10)
		fcadpdebug--;
	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);
	if (strncmp(ci->hwgname, "fcadp", 5) == 0)
	{
		cmn_err(CE_CONT, "%s (%d): %s", ci->hwgname, ci->number, buffer);
	}
	else
	{
		cmn_err(CE_CONT, "%s (%d):\n  %s", ci->hwgname, ci->number, buffer);
	}
}

void
DBGT(int debuglevel, struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti, char *fmt, ...)
{
	va_list		 ap;
	char		 buffer[256];

	if (fcadpdebug < debuglevel)
		return;
	if (fcadpdebug > 10)
		fcadpdebug--;
	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);
	if (strncmp(ci->hwgname, "fcadp", 5) == 0)
	{
		cmn_err(CE_CONT, "%st%d (%d): %s", ci->hwgname,
			ti->number, ci->number, buffer);
	}
	else
	{
		cmn_err(CE_CONT, "%s (%d) t%d:\n  %s", ci->hwgname,
			ci->number, ti->number, buffer); 
	}
}


/*
 * FCADP hardware graph support
 */
vertex_hdl_t
fcadp_device_add(vertex_hdl_t ctlr_vhdl, int targ, int lun)
{
	vertex_hdl_t	 lun_vhdl;
	vertex_hdl_t	 targ_vhdl;

	lun_vhdl = scsi_lun_vhdl_get(ctlr_vhdl, targ, lun);
	if (lun_vhdl == GRAPH_VERTEX_NONE)
	{
		DBG(2, "fcadp vertex %d: adding (%d,%d)\n", ctlr_vhdl, targ, lun);
		targ_vhdl = scsi_targ_vertex_add(ctlr_vhdl, targ);
		lun_vhdl = scsi_lun_vertex_add(targ_vhdl, lun);
	}
	return lun_vhdl;
}

void
fcadp_lun_remove(vertex_hdl_t ctlr_vhdl, int targ, int lun)
{
	vertex_hdl_t	 targ_vhdl;
	graph_error_t	 rv;
	char		 loc_str[20];

	DBG(2, "fcadp vertex %d: removing lun (%d,%d)\n", ctlr_vhdl, targ, lun);
	sprintf(loc_str, TARG_NUM_FORMAT, targ);
	rv = hwgraph_traverse(ctlr_vhdl, loc_str, &targ_vhdl);
	ASSERT(rv == GRAPH_SUCCESS); rv=rv;
	hwgraph_vertex_unref(targ_vhdl);

	(void) scsi_lun_vertex_remove(targ_vhdl, lun);
}

void
fcadp_target_remove(vertex_hdl_t ctlr_vhdl, int targ)
{
	(void) scsi_targ_vertex_remove(ctlr_vhdl, targ);
}


/*
 * The force variable is used to tell fcadp_device_remove to remove the device,
 * even if there is no target info structure allocated.  This would be the
 * case if a device was present on a loop but no Inquiry could be done.
 */
void
fcadp_device_remove(struct fcadpctlrinfo *ci, vertex_hdl_t ctlr_vhdl,
		    int tid, int force)
{
	struct fcadptargetinfo		*ti;
	struct fcadpluninfo		*li;
	vertex_hdl_t			 lun_vhdl;
	int				 lun;
	int				 removetarg = 1;

	if ((ti = ci->target[tid]) && ti->present)
	{
		DBG(2, "fcadp vertex %d: removing target (%d)\n", ctlr_vhdl, tid);
		for (lun = 0; lun < FCADP_MAXLUN; lun++)
		{
			lun_vhdl = scsi_lun_vhdl_get(ci->ctlrvhdl, tid, lun);
			if (lun_vhdl == GRAPH_VERTEX_NONE)
				continue;
			li = SLI_INFO(scsi_lun_info_get(lun_vhdl));
			ASSERT(li != NULL);
			if (li->present)
			{
				if (li->refcount == 0)
				{
					li->present = 0;
					fcadp_lun_remove(ctlr_vhdl, tid, lun);
				}
				else
					removetarg = 0;
			}
		}

		if (removetarg)
		{
			ti->present = 0;
			fcadp_target_remove(ctlr_vhdl, tid);
		}
	}
	else if (force)
	{
		DBG(2, "fcadp vertex %d: forcefully removing target (%d)\n", ctlr_vhdl, tid);
		fcadp_lun_remove(ctlr_vhdl, tid, 0);
		fcadp_target_remove(ctlr_vhdl, tid);
	}
}


static void
assign_int16(uint8_t *ptr, uint16_t value)
{
	ptr[0] = (uint8_t) value;
	ptr[1] = (uint8_t) (value >> 8);
}

static void
assign_int32(uint8_t *ptr, uint32_t value)
{
	ptr[0] = (uint8_t) value;
	ptr[1] = (uint8_t) (value >> 8);
	ptr[2] = (uint8_t) (value >> 16);
	ptr[3] = (uint8_t) (value >> 24);
}

static void
assign_int64(uint8_t *ptr, uint64_t value)
{
	ptr[0] = (uint8_t) value;
	ptr[1] = (uint8_t) (value >> 8);
	ptr[2] = (uint8_t) (value >> 16);
	ptr[3] = (uint8_t) (value >> 24);
	ptr[4] = (uint8_t) (value >> 32);
	ptr[5] = (uint8_t) (value >> 40);
	ptr[6] = (uint8_t) (value >> 48);
	ptr[7] = (uint8_t) (value >> 56);
}

/*ARGSUSED*/
void
fc_swapctl(register uint8_t *ptr, uint32_t count)
{
#if PCI_BIGENDIAN
	register int i;
	register uint8_t tmp;

	if ((uintptr_t) ptr & 0x7)
		cmn_err(CE_PANIC, "fc_swapctl to non dword-aligned boundary\n");
	if (count & 0x7)
		cmn_err(CE_PANIC, "fc_swapctl to non dword multiple bytecount\n");
	for (i = 0; i < count; i += 4 + (4 * PCI_DATA_64))
	{
#if PCI_DATA_64
		tmp = ptr[i];
		ptr[i] = ptr[i+7];
		ptr[i+7] = tmp;

		tmp = ptr[i+1];
		ptr[i+1] = ptr[i+6];
		ptr[i+6] = tmp;

		tmp = ptr[i+2];
		ptr[i+2] = ptr[i+5];
		ptr[i+5] = tmp;

		tmp = ptr[i+3];
		ptr[i+3] = ptr[i+4];
		ptr[i+4] = tmp;
#else
		tmp = ptr[i];
		ptr[i] = ptr[i+3];
		ptr[i+3] = tmp;

		tmp = ptr[i+1];
		ptr[i+1] = ptr[i+2];
		ptr[i+2] = tmp;
#endif
	}
#endif
}


#if PCI_BIGENDIAN
/*ARGSUSED*/
static void
swapdata(register uint8_t *ptr, uint32_t count)
{
	register int i;
	register uint8_t tmp;

	if ((uintptr_t) ptr & 0x3)
		cmn_err(CE_PANIC, "swapdata to non word-aligned boundary\n");
	count = (count + 3) & ~3;
	for (i = 0; i < count; i += 4)
	{
		tmp = ptr[i];
		ptr[i] = ptr[i+3];
		ptr[i+3] = tmp;

		tmp = ptr[i+1];
		ptr[i+1] = ptr[i+2];
		ptr[i+2] = tmp;
	}
}
#else
#define swapdata(X,Y)
#endif


/************** TCB management routines *****************/
/*
 * Get next TCB out of circular queue.
 */
struct _tcb *
fc_get_rrtcb(struct fcadpctlrinfo *ci)
{
	struct _tcb	*tcb;

	tcb = &ci->tcbqueue[ci->tcbindex];
	if (++ci->tcbindex == ci->config->number_tcbs)
		ci->tcbindex = 0;
	bzero(tcb, 128);
	return tcb;
}

/*
 * Return the address of the next tcb in the circular queue.
 */
void *
fc_next_rrtcb(struct fcadpctlrinfo *ci)
{
	return &ci->tcbqueue[ci->tcbindex];
}

/*
 * Get tcb header off of freelist
 * Assign a TCB to this tcb header now, since we're guaranteed to use
 * this TCB at this time.
 */
static struct fc_tcbhdr *
fc_gettcbhdr(struct fcadpctlrinfo *ci)
{
	struct fc_tcbhdr	*ret_tcb;

	ret_tcb = ci->freetcb;
	ASSERT(ret_tcb != NULL);
	ci->freetcb = ret_tcb->th_next;
	ret_tcb->th_tcb = fc_get_rrtcb(ci);
	ASSERT(ret_tcb != NULL);
	return ret_tcb;
}

/*
 * Put tcb header back on freelist
 */
void
fc_puttcbhdr(struct fcadpctlrinfo *ci, struct fc_tcbhdr *tcbhdr, struct scsi_request *req)
{
	struct fcadptargetinfo	*ti;
	struct fc_tcbhdr	*follow_tcb;
	struct fc_tcbhdr	*lead_tcb;

	ti = ci->target[tcbhdr->th_target];
	ti->timeout_progress = 0;

	/*
	 * Remove tcb from active list.
	 * If this is the last TCB removed, and the target was quiesced due to
	 * a fairness timeout, then start issuing commands again by requeueing
	 * the target into the controller wait queue.
	 */
	follow_tcb = NULL;
	lead_tcb = ti->active;
	while ((lead_tcb != NULL) && (lead_tcb != tcbhdr))
	{
		follow_tcb = lead_tcb;
		lead_tcb = lead_tcb->th_next;
	}
	ASSERT(lead_tcb != NULL);
	if (follow_tcb == NULL)
	{
		ASSERT(tcbhdr == lead_tcb);
		ti->active = lead_tcb->th_next;
		if (ti->active == NULL && ti->q_state >= FCTI_STATE_AGE)
		{
			ti->q_state -= FCTI_STATE_AGE;
			fc_queue_target(ci, ti);
		}
	}
	else
	{
		ASSERT(tcbhdr == lead_tcb);
		follow_tcb->th_next = lead_tcb->th_next;
	}

	/*
	 * Put tcb on free list
	 */
	tcbhdr->th_next = ci->freetcb;
	ci->freetcb = tcbhdr;

	/*
	 * Free large maps allocated in fc_get_scsireq
	 */
	if (tcbhdr->th_bigmap)
	{
		ci->bigmap_busy = 0;
		tcbhdr->th_bigmap = 0;
	}
	else if (tcbhdr->th_allocmap)
	{
		register int	npgs;

		ASSERT(req != NULL);
		npgs = fc_getmapsize(req);
		kvpfree(req->sr_spare, btoc(npgs * sizeof(SH_SG_DESCRIPTOR)));
		tcbhdr->th_allocmap = 0;
	}
	tcbhdr->th_type = TCBTYPE_IDLE;
	TCB_INACTIVE(ci, tcbhdr->th_number);
	ci->cmdcount--;

	if (ci->cmdcount == 0 && (ci->quiesce & USR_QUIESCE))
		ci->quiesce_timeout = timeout(fc_stop_quiesce, ci, ci->quiesce_time);
}


/********************* TCB fill routines **************************/
/*
 * Put the TCB into the list of active TCBs sorted by timeout length (longest
 *   timeout first).  Port login TCBs are special, because they are issued
 *   while old TCBs are still around, so we put them first.
 */
static void
fc_set_timeout(struct fcadptargetinfo *ti, struct fc_tcbhdr *tcbhdr, int timeoutval)
{
	register struct fc_tcbhdr *nxthdr;
	register struct fc_tcbhdr *prevhdr=NULL;
	int	 maxtick = 2 + (timeoutval - 1) / TIMEOUTCHECK_INTERVAL;

	tcbhdr->th_ticker = 0;
	tcbhdr->th_ticklimit = maxtick;
	if (ti->active == NULL ||
	    maxtick >= ti->active->th_ticklimit ||
	    tcbhdr->th_type == TCBTYPE_PLOGI)
	{
		tcbhdr->th_next = ti->active;
		ti->active = tcbhdr;
	}
	else
	{
		nxthdr = ti->active->th_next;
		prevhdr = ti->active;
		while (nxthdr != NULL && maxtick < nxthdr->th_ticklimit)
		{
			prevhdr = nxthdr;
			nxthdr = nxthdr->th_next;
		}
		prevhdr->th_next = tcbhdr;
		tcbhdr->th_next = nxthdr;
	}
}


/* ARGSUSED */
static __inline void
fcadp_mapbp(struct fcadpctlrinfo *ci, struct fc_tcbhdr *tcbhdr, struct scsi_request *req)
{
	struct _tcb	*tcb;
	SH_SG_DESCRIPTOR *sgmap;
	struct pfdat	*pfd;
	struct buf	*bp;
	uint64_t	 physaddr;
	int		 offset;
	int		 npg;
	int		 i;

	bp = req->sr_bp;
	if (BP_ISMAPPED(bp))
		panic("fcadp_mapbp: buffer is mapped");

	tcb = tcbhdr->th_tcb;
	offset = poff(bp->b_un.b_addr);
	npg = btoc(bp->b_bcount + offset);
	pfd = getnextpg(bp, NULL);

	if (npg == 1)
	{
		physaddr = pfdattophys(pfd) + offset;
		assign_int64(&tcb->sg_list_addr0, phystopci_data(physaddr));
		assign_int32(&tcb->sg_list_addr8, bp->b_bcount);
		tcb->sg_cache_ptr = 0x8;
	}
	else
	{
		if (req->sr_spare != NULL)
			sgmap = (SH_SG_DESCRIPTOR *) req->sr_spare;
		else
			sgmap = tcbhdr->th_map;

		physaddr = pfdattophys(pfd) + offset;
		assign_int64(sgmap[0].busAddress, phystopci_data(physaddr));
		assign_int32(sgmap[0].segmentLength, NBPC - offset);

		for (i = 1; i < npg; i++)
		{
			pfd = getnextpg(bp, pfd);
			physaddr = pfdattophys(pfd);
			assign_int64(sgmap[i].busAddress, phystopci_data(physaddr));
			assign_int32(sgmap[i].segmentLength, NBPC);
		}
		if ((offset + bp->b_bcount) % NBPC != 0)
			assign_int32(sgmap[npg-1].segmentLength,
				     (offset + bp->b_bcount) % NBPC);
		sgmap[npg-1].segmentLength[3] = 1;
		fc_swapctl((uint8_t *) sgmap, npg * sizeof(SH_SG_DESCRIPTOR));
		assign_int64(&tcb->sg_list_addr0, kvtopci(sgmap));
		*((uint32_t *) &tcb->sg_list_addr8) = 0;
		tcb->sg_cache_ptr = 0xb;
	}
}

static __inline void
fcadp_maplist(struct fcadpctlrinfo *ci, struct fc_tcbhdr *tcbhdr, struct scsi_request *req)
{
	struct _tcb	*tcb;
	SH_SG_DESCRIPTOR *sgmap;
	alenlist_t	 alenlist;
	int		 npg;
	alenaddr_t	 addr;
	size_t		 count;
	uint64_t	 pciaddr;
	int		 i;

	tcb = tcbhdr->th_tcb;
	alenlist = (alenlist_t) ((buf_t *) (req->sr_bp))->b_private;
	alenlist_cursor_init(alenlist, 0, NULL);
	npg = alenlist_size(alenlist);
	if (npg == 1)
	{
		if (alenlist_get(alenlist, NULL, 0, &addr, &count, 0))
			goto alen_failure;
		pciaddr = pcibr_dmatrans_addr(ci->pcivhdl, 0, (uint64_t) addr,
					      0, FCADP_PCIDMA_DATA_FLAGS);
		assign_int64(&tcb->sg_list_addr0, pciaddr);
		assign_int32(&tcb->sg_list_addr8, (uint32_t) count);
		tcb->sg_cache_ptr = 0x8;
	}
	else
	{
		if (req->sr_spare != NULL)
			sgmap = (SH_SG_DESCRIPTOR *) req->sr_spare;
		else
			sgmap = tcbhdr->th_map;
		for (i = 0; i < npg; i++)
		{
			if (alenlist_get(alenlist, NULL, 0, &addr, &count, 0))
				goto alen_failure;
			pciaddr = pcibr_dmatrans_addr(ci->pcivhdl, 0,
				  (uint64_t) addr, 0, FCADP_PCIDMA_DATA_FLAGS);
			assign_int64(sgmap[i].busAddress, pciaddr);
			assign_int32(sgmap[i].segmentLength, (uint32_t) count);
		}
		sgmap[npg-1].segmentLength[3] = 1;
		fc_swapctl((uint8_t *) sgmap, npg * sizeof(SH_SG_DESCRIPTOR));
		assign_int64(&tcb->sg_list_addr0, kvtopci(sgmap));
		*((uint32_t *) &tcb->sg_list_addr8) = 0;
		tcb->sg_cache_ptr = 0xb;
	}
	return;

alen_failure:
	panic("fcadp: ALEN failure\n");
}

/* ARGSUSED */
static __inline void
fcadp_map(struct fcadpctlrinfo *ci, struct fc_tcbhdr *tcbhdr, struct scsi_request *req)
{
	struct _tcb	*tcb;
	SH_SG_DESCRIPTOR *sgmap;
	int		 npg;
	uint8_t		*addr;
	uint32_t	 count;
	int		 thismap;
	int		 i;

	tcb = tcbhdr->th_tcb;
	npg = numpages(req->sr_buffer, req->sr_buflen);
	addr = req->sr_buffer;
	count = req->sr_buflen;
	if (npg == 1)
	{
		assign_int64(&tcb->sg_list_addr0, kvtopci_data(addr));
		assign_int32(&tcb->sg_list_addr8, count);
		tcb->sg_cache_ptr = 0x8;
	}
	else
	{
		if (req->sr_spare != NULL)
			sgmap = (SH_SG_DESCRIPTOR *) req->sr_spare;
		else
			sgmap = tcbhdr->th_map;
		for (i = 0; i < npg; i++)
		{
			assign_int64(sgmap[i].busAddress, kvtopci_data(addr));
			thismap = min(count, NBPC - poff(addr));
			assign_int32(sgmap[i].segmentLength, thismap);
			addr += thismap;
			count -= thismap;
		}
		sgmap[npg-1].segmentLength[3] = 1;
		fc_swapctl((uint8_t *) sgmap, npg * sizeof(SH_SG_DESCRIPTOR));
		assign_int64(&tcb->sg_list_addr0, kvtopci(sgmap));
		*((uint32_t *) &tcb->sg_list_addr8) = 0;
		tcb->sg_cache_ptr = 0xb;
	}
}

static __inline void
fill_normal_tcb(struct fcadpctlrinfo *ci, struct scsi_request *req,
	struct reference_blk *refblk, struct fc_tcbhdr *tcbhdr)
{
	struct _tcb	*tcb;

	refblk->control_blk = ci->control_blk;
	refblk->tcb = tcb = tcbhdr->th_tcb;
	refblk->tcb_number = tcbhdr->th_number;

	assign_int16(&tcb->fhead.our_xid0, tcbhdr->th_number);
	tcb->fhead.their_xid0 = 0xff;
	tcb->fhead.their_xid1 = 0xff;

	tcb->fhead.their_aid0 = TID_2_ALPA(tcbhdr->th_target);
	tcb->fhead.r_ctl = 0x01;

	tcb->fhead.delimiter = CLASS_THREE_NORMAL; /* class three normal */

	tcb->fhead.f_ctl0 = 0x8;
	tcb->fhead.f_ctl2 = 0x29;
	tcb->fhead.type = 8;

	/*
	 * Here, we used to set the their_bb_credit field with the credit
	 * value received at login time.  However, login credit does not
	 * work properly with the 1160 chip's current workarounds, so we
	 * have to do without.
	 */

	if (fcadp_host_unfair)
		tcb->fhead.seq_cfg1 = 0x18;	/* open half duplex */
	else
		tcb->fhead.seq_cfg1 = 0x40;     /* open half duplex, arb fair */

	/*
	 * Fill out TCB with information from scsi request
	 */
	*((uint16_t *) &tcb->scsi_cmd.fcp_lun_0) = (uint16_t) req->sr_lun;
	bcopy(req->sr_command, tcb->scsi_cmd.fcp_cdb, req->sr_cmdlen);
	*((int *)&tcb->scsi_cmd.fcp_dl0) = req->sr_buflen;
	if (req->sr_buflen != 0)
	{
		if (req->sr_flags & SRF_DIR_IN)
			tcb->scsi_cmd.scsi_cntl.rwrt_data = 2;
		else
		{
			if (req->sr_command[0] != 0x2A)
				swapdata(req->sr_buffer, req->sr_buflen);
			tcb->scsi_cmd.scsi_cntl.rwrt_data = 1;
			tcb->control = 2;
		}

		if (req->sr_flags & SRF_MAPBP)
			fcadp_mapbp(ci, tcbhdr, req);
		else if (req->sr_flags & SRF_ALENLIST)
			fcadp_maplist(ci, tcbhdr, req);
		else
			fcadp_map(ci, tcbhdr, req);
	}
	else
		tcb->sg_cache_ptr = 0xC;

	assign_int64(&tcb->next_tcb_addr0, kvtopci(fc_next_rrtcb(ci)));
	assign_int64(&tcb->info_host_addr0, kvtopci(tcbhdr->th_sense));
	assign_int32(&tcb->xfer_rdy_count0, (uint32_t) req->sr_buflen);
	tcb->hst_cfg_ctl = 0x10 | ADPFC_HST_CFG_CTL;
	assign_int16((uint8_t *) &tcb->tcb_index, tcbhdr->th_number);
}

static void
fill_plogi_tcb(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	struct _tcb		*tcb;
	struct _eframe_head	*eh;
	struct fc_plogi_payload	*pl;

	refblk->control_blk = ci->control_blk;
	refblk->tcb = tcb = tcbhdr->th_tcb;
	refblk->tcb_number = tcbhdr->th_number;

	/*
	 * setup payload
	 */
	pl = (struct fc_plogi_payload *) tcbhdr->th_sense;
	bzero(pl, sizeof(*pl));
	pl->command_code = 0x3000000;
	pl->common_service_param.fcph_version = 0x909; /* FC-PH 4.3 */
	/* bbcredit = 0 */
	pl->common_service_param.common_features = 0x8800; /* CovnArrr */
	pl->common_service_param.rcvdata_size = ADPFC_PAYLOAD_SIZE;
	pl->common_service_param.max_concurrent_seq = 0xff;
	/* relative offset = 0 */
	pl->common_service_param.e_d_tov = 0x100f4;
	pl->port_name = ci->portname;
	pl->node_name = ci->portname;
	/* class 1 and class 2 params 0 */
	pl->class3_service_param.service_options = 0x8000; /* class valid */
	/* initiator control = 0 */
	/* target control = 0 */
	pl->class3_service_param.rcv_data_size = ADPFC_PAYLOAD_SIZE;
	pl->class3_service_param.concurrent_seq = 0xff;
	/* eecredit = 0 (not used in class 3) */
	pl->class3_service_param.openseq_per_exchg = 1;
	/* vendor version all zeros (not valid (v in common...common_features)) */
	swapdata((uint8_t *) pl, sizeof(struct fc_plogi_payload));

	assign_int16(&tcb->fhead.our_xid0, tcbhdr->th_number);
	tcb->fhead.their_xid0 = 0xff;
	tcb->fhead.their_xid1 = 0xff;

	tcb->fhead.their_aid0 = TID_2_ALPA(tcbhdr->th_target);
	tcb->fhead.r_ctl = 0x22; /* unsolicited control extended link svc frm */
  
	tcb->fhead.delimiter = CLASS_THREE_NORMAL; /* class three normal */
	tcb->fhead.dat_ep_seqid = 0xff;

	tcb->fhead.dat_ep_seqcnt0 = 0xff;
	tcb->fhead.dat_ep_seqcnt1 = 0xff;
	tcb->fhead.f_ctl2 = (FCTL2_FIRST_SEQUENCE | FCTL2_END_SEQUENCE);
	/* extended link service: table 34 FC-PH, 18.4 */
	tcb->fhead.type = EXTENDED_LINK_SERVICE_TYPE;
	tcb->fhead.seq_cfg1 = 0x40;	/* open half duplex */

	tcb->control = SEND_LINK_CMD_RCV_LINK_REPLY;
	tcb->sg_cache_ptr = EMBEDDED_SG_LIST;

	/*
	 * Setup expected frame header at scsi_cmd + 2.
	 */
	eh = (struct _eframe_head *) (((char *)(&tcb->scsi_cmd)) + 2);
	assign_int16((uint8_t *) &eh->our_xid0, tcbhdr->th_number);
	eh->their_xid0 = 0xff; 
	eh->their_xid1 = 0xff;  
	eh->their_aid0 = TID_2_ALPA(tcbhdr->th_target);
	eh->r_ctl = 0x23;    
	eh->delimiter = CLASS_THREE_NORMAL;
	eh->seq_id = 0xff; 
	eh->type = EXTENDED_LINK_SERVICE_TYPE;

	assign_int64(&tcb->next_tcb_addr0, kvtopci(fc_next_rrtcb(ci)));
	assign_int64(&tcb->sg_list_addr0, kvtopci(pl));
	/*
	 * following line clobbers run_status field with 0, which is okay
	 */
	assign_int32(&tcb->sg_list_addr8, sizeof(struct fc_plogi_payload));
	assign_int64(&tcb->info_host_addr0, kvtopci(tcbhdr->th_map));
	assign_int32((uint8_t *) &tcb->xfer_rdy_count0, sizeof(struct fc_plogi_payload));
	tcb->hst_cfg_ctl = 0x11;
	assign_int16((uint8_t *) &tcb->tcb_index, tcbhdr->th_number);
}

static void
fill_prli_tcb(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	struct _tcb		*tcb;
	struct _eframe_head	*eh;
	struct fc_prli_payload	*pr;

	refblk->control_blk = ci->control_blk;
	refblk->tcb = tcb = tcbhdr->th_tcb;
	refblk->tcb_number = tcbhdr->th_number;

	/*
	 * Set up payload for Process Login
	 */
	pr = (struct fc_prli_payload *) tcbhdr->th_sense;
	bzero(pr, sizeof(*pr));
	pr->command_code = 0x20;
	pr->page_length = 0x10;
	pr->payload_length = FC_PRLI_PAYLOAD_LENGTH;
	pr->type_code = FC_PRLI_TYPE_CODE;
	pr->est_image_pair = 1;
	pr->svc_param = FC_PRLI_SVC_PARAM_SEND;
	swapdata((uint8_t *) pr, sizeof(struct fc_prli_payload));

	/*
	 * Setup tcb itself
	 */
	assign_int16(&tcb->fhead.our_xid0, tcbhdr->th_number);
	tcb->fhead.their_xid0 = 0xff;
	tcb->fhead.their_xid1 = 0xff;

	tcb->fhead.their_aid0 = TID_2_ALPA(tcbhdr->th_target);
	tcb->fhead.r_ctl = 0x22; /* unsolicited control extended link svc frm */
  
	tcb->fhead.delimiter = CLASS_THREE_NORMAL; /* class three normal */
	tcb->fhead.dat_ep_seqid = 0xff;

	tcb->fhead.dat_ep_seqcnt0 = 0xff;
	tcb->fhead.dat_ep_seqcnt1 = 0xff;
	tcb->fhead.f_ctl2 = (FCTL2_FIRST_SEQUENCE | FCTL2_END_SEQUENCE);
	/* extended link service: table 34 FC-PH, 18.4 */
	tcb->fhead.type = EXTENDED_LINK_SERVICE_TYPE;
	tcb->fhead.seq_cfg1 = 0x40;	/* open half duplex */

	tcb->control = SEND_LINK_CMD_RCV_LINK_REPLY;
	tcb->sg_cache_ptr = EMBEDDED_SG_LIST;

	/*
	 * Setup expected frame header at scsi_cmd + 2.
	 */
	eh = (struct _eframe_head *) (((char *)(&tcb->scsi_cmd)) + 2);
	assign_int16(&eh->our_xid0, tcbhdr->th_number);
	eh->their_xid0 = 0;
	eh->their_xid1 = 0;
	eh->their_aid0 = TID_2_ALPA(tcbhdr->th_target);
	eh->r_ctl = 0x23;    
	eh->delimiter = CLASS_THREE_NORMAL;
	eh->seq_id = 0xff; 
	eh->type = EXTENDED_LINK_SERVICE_TYPE;

	assign_int64(&tcb->next_tcb_addr0, kvtopci(fc_next_rrtcb(ci)));
	assign_int64(&tcb->sg_list_addr0, kvtopci(pr));
	/*
	 * following line clobbers run_status field with 0, which is okay
	 */
	assign_int32(&tcb->sg_list_addr8, sizeof(struct fc_prli_payload));
	assign_int64(&tcb->info_host_addr0, kvtopci(tcbhdr->th_map));
	assign_int32((uint8_t *) &tcb->xfer_rdy_count0, sizeof(struct fc_prli_payload));
	tcb->hst_cfg_ctl = 0x11;
	assign_int16((uint8_t *) &tcb->tcb_index, tcbhdr->th_number);
}

static void
fill_adisc_tcb(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	struct _tcb		*tcb;
	struct _eframe_head	*eh;
	struct fc_adisc_payload	*adisc;

	refblk->control_blk = ci->control_blk;
	refblk->tcb = tcb = tcbhdr->th_tcb;
	refblk->tcb_number = tcbhdr->th_number;

	/*
	 * Set up payload for Address Discovery
	 */
	adisc = (struct fc_adisc_payload *) tcbhdr->th_sense;
	bzero(adisc, sizeof(*adisc));
	adisc->command_code = 0x52 << 24;
	adisc->hard_address[2] = TID_2_ALPA(fcadp_hostid);
	adisc->nport_id = TID_2_ALPA(ci->host_id);

	/*
	 * Setup tcb itself
	 */
	assign_int16((uint8_t *) &tcb->fhead.our_xid0, tcbhdr->th_number);
	tcb->fhead.their_xid0 = 0xff;
	tcb->fhead.their_xid1 = 0xff;

	tcb->fhead.their_aid0 = TID_2_ALPA(tcbhdr->th_target);
	tcb->fhead.r_ctl = 0x22; /* unsolicited control extended link svc frm */
  
	tcb->fhead.delimiter = CLASS_THREE_NORMAL; /* class three normal */
	tcb->fhead.dat_ep_seqid = 0xff;

	tcb->fhead.dat_ep_seqcnt0 = 0xff;
	tcb->fhead.dat_ep_seqcnt1 = 0xff;
	tcb->fhead.f_ctl2 = (FCTL2_FIRST_SEQUENCE | FCTL2_END_SEQUENCE);
	/* extended link service: table 34 FC-PH, 18.4 */
	tcb->fhead.type = EXTENDED_LINK_SERVICE_TYPE;
  
	tcb->fhead.seq_cfg1 = 0x40; /* open half duplex */

	tcb->control = SEND_LINK_CMD_RCV_LINK_REPLY;
	tcb->sg_cache_ptr = EMBEDDED_SG_LIST;

	eh = (struct _eframe_head *) (((char *)(&tcb->scsi_cmd)) + 2);
	assign_int16((uint8_t *) &eh->our_xid0, tcbhdr->th_number);
	eh->their_xid0 = 0xff; 
	eh->their_xid1 = 0xff;  
	eh->their_aid0 = TID_2_ALPA(tcbhdr->th_target);
	eh->r_ctl = 0x23;    
	eh->delimiter = CLASS_THREE_NORMAL;
	eh->seq_id = 0xff; 
	eh->type = EXTENDED_LINK_SERVICE_TYPE;

	assign_int64((uint8_t *) &tcb->next_tcb_addr0, kvtopci(fc_next_rrtcb(ci)));
	assign_int64((uint8_t *) &tcb->sg_list_addr0, kvtopci(adisc));

	/* following line clobbers run_status field with 0, which is okay */
	assign_int32((uint8_t *) &tcb->sg_list_addr8, sizeof(struct fc_adisc_payload));
	assign_int64((uint8_t *) &tcb->info_host_addr0, kvtopci(tcbhdr->th_map));
	assign_int32((uint8_t *) &tcb->xfer_rdy_count0, sizeof(struct fc_adisc_payload));
	tcb->hst_cfg_ctl = 0x11;
	assign_int16((uint8_t *) &tcb->tcb_index, tcbhdr->th_number);
}

void
fill_abts_tcb(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	      struct fc_tcbhdr *tcbhdr, struct fc_tcbhdr *aborthdr)
{
	printf("fill_abts_tcb 0x%x 0x%x 0x%x 0x%x\n",
	       ci, refblk, tcbhdr, aborthdr);
}



/********************* command queueing routines *************************/
/*
 * Put the target on the appropriate controller wait queue.
 */
static void
fc_queue_target(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti)
{
	if (ti->q_state >= FCTI_STATE_QUIESCE)
		return;
	if (ti->abort)
	{
		if (ci->abts_queue.head == NULL)
			ci->abts_queue.head = ti;
		else
			ci->abts_queue.tail->abts_next = ti;
		ci->abts_queue.tail = ti;
		ti->q_state = FCTI_STATE_QUIESCE;
		fc_dequeue_target_scsi(ci, ti);
	}
	else if (ti->needplogi || ti->needprli || ti->needadisc)
	{
		if (ci->lnksvc_queue.head == NULL)
			ci->lnksvc_queue.head = ti;
		else
			ci->lnksvc_queue.tail->lnksvc_next = ti;
		ci->lnksvc_queue.tail = ti;
		ti->q_state = FCTI_STATE_QUIESCE;
		fc_dequeue_target_scsi(ci, ti);
	}
	else if (ti->waithead)
	{
		if (ci->scsi_queue.head == NULL)
			ci->scsi_queue.head = ti;
		else
			ci->scsi_queue.tail->scsi_next = ti;
		ci->scsi_queue.tail = ti;
		ti->q_state = FCTI_STATE_QUEUED;
	}
}

/*
 * Queue a scsirequest on the target scsi wait queue.
 */
static void
fc_queue_scsireq(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti,
		   struct scsi_request *req)
{
	if (ti->waithead == NULL)
		ti->waithead = req;
	else
		ti->waittail->sr_ha = req;
	ti->waittail = req;
	if (ti->q_state == FCTI_STATE_IDLE)
		fc_queue_target(ci, ti);
}

/*
 * Take scsi request off of SCSI wait queue.
 */
static void
fc_remove_scsireq(struct fcadptargetinfo *ti, struct scsi_request *req)
{
	ti->waithead = req->sr_ha;
	if (req->sr_ha == NULL)
		ti->waittail = NULL;
	ASSERT(ti->q_state == FCTI_STATE_QUEUED);
	if (ti->waithead == NULL)
		ti->q_state = FCTI_STATE_IDLE;
	req->sr_ha = NULL;
}

/*
 * Take the target off the controller scsi wait queue
 * Then add target to end of controller scsi wait queue if necessary
 */
static void
fc_requeue_target_scsi(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti)
{
	ci->scsi_queue.head = ti->scsi_next;
	ti->scsi_next = NULL;
	if (ci->scsi_queue.head == NULL)
		ci->scsi_queue.tail = NULL;
	if (ti->q_state == FCTI_STATE_QUEUED)
		fc_queue_target(ci, ti);
}


int
fc_getmapsize(struct scsi_request *req)
{
	register int	offset;

	if (req->sr_flags & SRF_ALENLIST)
		return alenlist_size((alenlist_t) ((buf_t *)(req->sr_bp))->b_private);
	else
	{
		if (req->sr_flags & SRF_MAPBP)
			offset = 0;
		else
			offset = poff(req->sr_buffer);
		return btoc(req->sr_buflen + offset);
	}
}

/*
 * Get a DMA map for the command.  If the transfer size is less than the
 * amount that can be mapped with a standard map, then return 1.  If we
 * reserve the per-controller big map, return 2.  If we allocate one,
 * return 3.  If unable to get one, return 0.
 */
static int
fc_getmap(struct fcadpctlrinfo *ci, struct scsi_request *req)
{
	SH_SG_DESCRIPTOR	*map = NULL;
	int			 retval;
	uint			 npgs;

	npgs = fc_getmapsize(req);
	if (npgs <= FCADP_MAPSIZE)
		return 1;
	if (!ci->bigmap_busy)
	{
		map = ci->bigmap;
		ci->bigmap_busy = 1;
		retval = 2;
	}
	else
	{
		map = kvpalloc(btoc(npgs * sizeof(SH_SG_DESCRIPTOR)),
			       VM_NOSLEEP | VM_PHYSCONTIG, 0);
		if (map == NULL)
			return 0;
		retval = 3;
	}
	req->sr_spare = map;
	return retval;
}


/*
 * Find the next request to issue and return it.
 */
static __inline struct fc_tcbhdr *
fc_get_scsireq(struct fcadpctlrinfo *ci)
{
	struct fcadptargetinfo	*ti = NULL;
	struct fc_tcbhdr	*tcbhdr = NULL;
	struct scsi_request	*req = NULL;
	int			 maptype = 0;

	/*
	 * Get first waiting request.
	 * If it's too big for a standard DMA map, then call fc_getbigmap to
	 *   attempt to get a big map.
	 * If that fails, then just wait until later.
	 */
	ti = ci->scsi_queue.head;
	req = ti->waithead;
	if ((maptype = fc_getmap(ci, req)) == 0)
		return NULL;

	/*
	 * Get a free tcbhdr.  There should be one available, because
	 * cmdcount is checked in fc_start.
	 */
	tcbhdr = fc_gettcbhdr(ci);
	if (maptype == 2)
		tcbhdr->th_bigmap = 1;
	else if (maptype == 3)
		tcbhdr->th_allocmap = 1;
	tcbhdr->th_req = req;
	tcbhdr->th_target = ti->number;
	tcbhdr->th_type = TCBTYPE_SCSI;
	fc_remove_scsireq(ti, req);
	fc_requeue_target_scsi(ci, ti);

	return tcbhdr;
}

/* 
 * Remove the target from the controller SCSI wait queue.
 * Used while performing logins, etc., when we don't want to
 * issue normal commands to any lun on the target.
 */
static void
fc_dequeue_target_scsi(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti)
{
	struct fcadptargetinfo *cur_ti;
	struct fcadptargetinfo *prev_ti = NULL;

	cur_ti = ci->scsi_queue.head;
	while (cur_ti != NULL)
	{
		if (cur_ti == ti)
		{
			if (prev_ti == NULL)
				ci->scsi_queue.head = ti->scsi_next;
			else
				prev_ti->scsi_next = ti->scsi_next;
			if (ti->scsi_next == NULL)
				ci->scsi_queue.tail = prev_ti;
			else
				ti->scsi_next = NULL;
			break;
		}
		prev_ti = cur_ti;
		cur_ti = cur_ti->scsi_next;
	}
}

/*
 * Remove target from all controller wait queues (scsi, lnksvc, abts).
 */
void
fc_dequeue_target(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti)
{
	struct fcadptargetinfo *cur_ti;
	struct fcadptargetinfo *prev_ti = NULL;

	fc_dequeue_target_scsi(ci, ti);

	cur_ti = ci->lnksvc_queue.head;
	while (cur_ti != NULL)
	{
		if (cur_ti == ti)
		{
			if (prev_ti == NULL)
				ci->lnksvc_queue.head = ti->lnksvc_next;
			else
				prev_ti->lnksvc_next = ti->lnksvc_next;
			if (ti->lnksvc_next == NULL)
				ci->lnksvc_queue.tail = prev_ti;
			else
				ti->lnksvc_next = NULL;
			break;
		}
		prev_ti = cur_ti;
		cur_ti = cur_ti->lnksvc_next;
	}

	cur_ti = ci->abts_queue.head;
	while (cur_ti != NULL)
	{
		if (cur_ti == ti)
		{
			if (prev_ti == NULL)
				ci->abts_queue.head = ti->abts_next;
			else
				prev_ti->abts_next = ti->abts_next;
			if (ti->abts_next == NULL)
				ci->abts_queue.tail = prev_ti;
			else
				ti->abts_next = NULL;
			break;
		}
		prev_ti = cur_ti;
		cur_ti = cur_ti->abts_next;
	}
}

/*
 * Prepare to issue ABTS or other link service command to target.
 * Remove target from SCSI wait queue.  We don't want to confuse it
 * with more SCSI commands while we perform link services.
 */
static struct fc_tcbhdr *
fc_get_request(struct fcadpctlrinfo *ci)
{
	struct fcadptargetinfo	*ti;
	struct fc_tcbhdr	*tcbhdr;

	ASSERT(ci->abts_queue.head || ci->lnksvc_queue.head);
	if (ci->abts_queue.head)
	{
		ti = ci->abts_queue.head;
		tcbhdr = fc_gettcbhdr(ci);
		tcbhdr->th_type = TCBTYPE_ABTS;
		tcbhdr->th_req = (struct scsi_request *) ti->active;

		ci->abts_queue.head = ti->abts_next;
		ti->abts_next = NULL;
	}
	else if (ci->lnksvc_queue.head)
	{
		ti = ci->lnksvc_queue.head;
		tcbhdr = fc_gettcbhdr(ci);
		if (ti->needplogi)
			tcbhdr->th_type = TCBTYPE_PLOGI;
		else if (ti->needprli)
			tcbhdr->th_type = TCBTYPE_PRLI;
		else if (ti->needadisc)
			tcbhdr->th_type = TCBTYPE_ADISC;
		tcbhdr->th_req = NULL;

		ci->lnksvc_queue.head = ti->lnksvc_next;
		ti->lnksvc_next = NULL;
	}
	tcbhdr->th_target = ti->number;

	return tcbhdr;
}

/*
 * Issue a command if possible.  Return 1 if a command was issued, 0 otherwise.
 */
static int
fc_start(struct fcadpctlrinfo *ci)
{
	struct fc_tcbhdr	*tcbhdr = NULL;
	struct reference_blk	 refblk;
	int			 retval = 0;
	int			 timeoutval;
	
	if (ci->quiesce)
		goto out;

	/*
	 * We allow link service or ABTS to exceed the maximum queue,
	 * because we reserved extra TCBs for them.
	 */
	if (ci->lnksvc_queue.head &&
	    ci->cmdcount < ci->maxqueue+FCADP_LNKSVC_SETASIDE)
		tcbhdr = fc_get_request(ci);
	else if (ci->scsi_queue.head && ci->cmdcount < ci->maxqueue)
		tcbhdr = fc_get_scsireq(ci);
	if (tcbhdr == NULL)
		goto out;

	/*
	 * Fill out information for the TCB and then deliver it to HIM
	 */
	timeoutval = 10 * HZ;	/* default value */
	switch (tcbhdr->th_type)
	{
	case TCBTYPE_SCSI:
		fill_normal_tcb(ci, tcbhdr->th_req, &refblk, tcbhdr);
		timeoutval = tcbhdr->th_req->sr_timeout;
#if FCADP_CMD_DEBUG
		command_list[command_index].tcb_number = refblk.tcb_number;
		command_list[command_index].adap_num = (uint8_t) ci->number;
		command_list[command_index].targ_num = tcbhdr->th_req->sr_target;
		bcopy(tcbhdr->th_req->sr_command, command_list[command_index].scsicmd,
		      tcbhdr->th_req->sr_cmdlen);
		if (++command_index == COMMAND_INDEX_SIZE)
			command_index = 0;
#endif
		break;
	case TCBTYPE_ADISC:
		fill_adisc_tcb(ci, &refblk, tcbhdr);
		break;
	case TCBTYPE_PLOGI:
		fill_plogi_tcb(ci, &refblk, tcbhdr);
#if LOGIN_QUIESCE
		if (ci->cmdcount == 0)
		{
			timeoutval = 3 * HZ;
			ci->quiesce |= LIP_QUIESCE;
		}
#endif
		break;
	case TCBTYPE_PRLI:
		fill_prli_tcb(ci, &refblk, tcbhdr);
		break;
	case TCBTYPE_ABTS:
		fill_abts_tcb(ci, &refblk, tcbhdr,
			      (struct fc_tcbhdr *) tcbhdr->th_req);
		break;
	}

	/*
	 * Put TCB on active list, increase command count, and deliver TCB.
	 */
	TCB_ACTIVE(ci, refblk.tcb_number);
	ci->cmdcount++;
	if (ci->cmdcount > ci->hiwater)
		ci->hiwater = ci->cmdcount;
	fc_swapctl((uint8_t *) refblk.tcb, sizeof(struct _tcb));
	DBG(11, "fcadp%dd%d: issue tcb %d type %d\n", ci->number, tcbhdr->th_target,
	    tcbhdr->th_number, tcbhdr->th_type);
	sh_deliver_tcb(&refblk);
	fc_set_timeout(ci->target[tcbhdr->th_target], tcbhdr, timeoutval);
	retval = 1;

out:
	return retval;
}


/*
 * Return 1 if the target id is on the loop and participated
 * in the last LIP.
 */
static int
find_target(struct fcadpctlrinfo *ci, u_char targ)
{
	int i;

	if (!ci->lipmap_valid)
		return 0;
	if (targ == ci->host_id)
		return 0;
	if (targ >= FCADP_MAXLPORT)
		return 0;
	for (i = 1; i <= ci->lipmap[0]; i++)
		if (targ == ci->lipmap[i])
			return 1;
	return 0;
}


/*
 * Reinitialize target state variables; return all link service commands.
 */
void
fc_reset_target_state(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti)
{
	struct fc_tcbhdr	*tcbhdr;
	struct fc_tcbhdr	*nxthdr;

	/*
	 * Clear various state variables in target structure.
	 */
	ti->plogi = 0;
	ti->prli = 0;
	ti->needplogi = 0;
	ti->abort = NULL;
	ti->q_state = FCTI_STATE_IDLE;
	ti->timeout_progress = 0;
	fc_dequeue_target(ci, ti);

	/*
	 * Clear active link service commands.
	 */
	tcbhdr = ti->active;
	while (tcbhdr != NULL)
	{
		nxthdr = tcbhdr->th_next;
		if (tcbhdr->th_type != TCBTYPE_SCSI)
			fc_puttcbhdr(ci, tcbhdr, NULL);
		tcbhdr = nxthdr;
	}
}

/*
 * Return all outstanding commands.
 * set sr_status to error_status1 for active commands,
 *                  error_status2 for queued commands
 */
void
fc_clear_target(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti,
		int error_status1, int error_status2)
{
	struct scsi_request	*req;
	struct scsi_request	*nxt;
	struct scsi_request	*list = NULL;
	struct fc_tcbhdr	*tcbhdr;
	struct fc_tcbhdr	*nxthdr;

	/*
	 * Clear queued commands.
	 */
	req = ti->waithead;
	while (req != NULL)
	{
		req->sr_status = error_status2;
		nxt = req->sr_ha;
		req->sr_ha = list;
		list = req;
		req = nxt;
	}
	ti->waithead = ti->waittail = NULL;

	/*
	 * Clear active commands.
	 */
	req = NULL;
	tcbhdr = ti->active;
	while (tcbhdr != NULL)
	{
		if (tcbhdr->th_type == TCBTYPE_SCSI)
		{
			req = tcbhdr->th_req;
			req->sr_status = error_status1;
			req->sr_ha = list;
			list = req;
		}
		nxthdr = tcbhdr->th_next;
		fc_puttcbhdr(ci, tcbhdr, req);
		tcbhdr = nxthdr;
		req = NULL;
	}
	ASSERT(ti->active == NULL);

	/*
	 * Return all commands to upper drivers
	 */
	IOUNLOCK(ci->himlock, ci->himlockspl);
	DBGT(2, ci, ti, "returning all target commands\n");
	while (list != NULL)
	{
		req = list;
		list = req->sr_ha;
		(*req->sr_notify)(req);
	}
	IOLOCK(ci->himlock, ci->himlockspl);
}

/*
 * If target still not visible, then clear all commands queued to it.
 */
void
fc_check_target(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti)
{
	IOLOCK(ci->himlock, ci->himlockspl);
	if (ti->check_pending)
	{
		if (ci->lipmap_valid)
		{
			ti->check_pending = 0;
			if (!find_target(ci, ti->number))
			{
				ti->disappeared = 1;
				fc_terr(ci, ti->number, "target did not reappear by %d"
					" clearing outstanding requests\n", lbolt);
				fc_reset_target_state(ci, ti);
				fc_clear_target(ci, ti, SC_TIMEOUT, SC_TIMEOUT);
			}
		}
		else
		{
			timeout(fc_check_target, ci, HZ*4, ti);
		}
	}
	IOUNLOCK(ci->himlock, ci->himlockspl);
}


/*
 * Clear controller status.  If the controller is dead, then clear_targ will
 * be set and all commands returned.
 */
void
fc_clear_controller(struct fcadpctlrinfo *ci, int clear_targ, int error_status)
{
	struct fcadptargetinfo	*ti;
	int			 tmp;

	if (clear_targ)
		sh_shutdown(ci->control_blk, 1);

	ci->quiesce |= LIP_QUIESCE;

	/*
	 * Clear any waiting lip timeouts.
	 * Wakeup processes waiting for lip completion.
	 */
	if (ci->lip_timeout)
	{
		untimeout(ci->lip_timeout);
		ci->lip_timeout = 0;
	}
	while (ci->lipcount)
	{
		vsema(&ci->lipsema);
		ci->lipcount--;
	}

	/*
	 * If a user issued primitive is active, wake it the waiting process.
	 */
	if (ci->userprim)
	{
		ci->userprim_status = EAGAIN;
		vsema(&ci->userprim_sema);
	}

	/*
	 * Clear all targets if we aren't planning to login.
	 * If we are planning to login, don't clear the targets yet.
	 * If a target has disappeared, give it some time to reappear
	 *   to workaround broken DG RAID.
	 */
	if (clear_targ)
		DBGC(2, ci, "returning all controller commands\n", ci->number);
	for (tmp = 0; tmp < FCADP_MAXTARG; tmp++)
	{
		ti = ci->target[tmp];
		if (ti == NULL)
			continue;
		fc_reset_target_state(ci, ti);
		if (clear_targ)
			fc_clear_target(ci, ti, error_status, error_status);
		else if (find_target(ci, tmp))
			ti->disappeared = 0;
		else if (!ti->disappeared && !ti->check_pending)
		{
			fc_terr(ci, tmp, "target disappeared @ %d, waiting\n", lbolt);
			ti->check_pending = 1;
			ti->q_state = FCTI_STATE_QUIESCE;
			ti->check_pending_id = timeout(fc_check_target, ci, HZ*4, ti);
		}
	}
}


void
fc_clear_unsol_tcb(struct fcadpctlrinfo *ci)
{
	struct fc_tcbhdr	*tcbhdr;
	int			 i;

	for (i = ci->config->num_unsol_tcbs; i != 0; i--)
	{
		if TCB_IS_ACTIVE(ci, i)
		{
			tcbhdr = FIND_TCB(ci, i);
			ASSERT(tcbhdr->th_type == TCBTYPE_UNSOL);

			/*
			 * Put tcb on free list, mark it as idle, inactivate
			 * it, and decrement the command count.
			 */
			tcbhdr->th_next = ci->freetcb;
			ci->freetcb = tcbhdr;
			tcbhdr->th_type = TCBTYPE_IDLE;
			TCB_INACTIVE(ci, tcbhdr->th_number);
		}
	}
}


/*
 * Returns 0 if successfully reset and initialized, non-zero otherwise
 */
int
fc_reset_controller(struct fcadpctlrinfo *ci)
{
	REGISTER		 base_address;

	ci->quiesce |= LIP_QUIESCE;

	base_address.reg = ci->ibar0_addr;
	sh_reset_chip(base_address);
	DELAYBUS(500);

	/*
	 * Reclaim unsolicited TCB's.
	 * Leave primitive TCB and LIP TCB alone.
	 * Reset various variables.
	 */
	fc_clear_unsol_tcb(ci);

	/*
	 * Reset internal controller data structures
	 * Here, data structures that affect the internal workings of
	 *   the driver and relect the state of the chip are reset.
	 *   In clear_controller, the structures that affect external
	 *   requests are cleared.
	 */
	ci->lip_wait = 0;
	ci->lip_issued = 0;
	ci->lipmap_valid = 0;

	ci->primbusy = 0;
	ci->tcbindex = 0;

	/*
	 * After resetting, a new LIP will have to be issued to be able
	 * to use the controller.
	 */
	return fcadp_ctlrinit(ci, 0);
}


void
fc_wait_lip(struct fcadpctlrinfo *ci, int relock)
{
	ci->lipcount++; /* Wait for LIP completion */
	IOUNLOCK(ci->himlock, ci->himlockspl);
	psema(&ci->lipsema, PRIBIO);
	DBGC(2, ci, "process awakened from lip\n");
	if (relock)
		IOLOCK(ci->himlock, ci->himlockspl);
}


/****************************** External interface routines *************************/
/* ARGSUSED */
int
fcadpalloc(vertex_hdl_t lun_vhdl, int opt, void (*cb)())
{
	struct fcadpctlrinfo	*ci;
	struct fcadptargetinfo	*ti;
	struct fcadpluninfo	*li;
	int			 retval = SCSIALLOCOK;
	u_char			 ctlr, targ, lun;
	scsi_lun_info_t		*lun_info;

	lun_info = scsi_lun_info_get(lun_vhdl);
	ctlr = SLI_ADAP(lun_info);
	targ = SLI_TARG(lun_info);
	lun = SLI_LUN(lun_info);

	if (ctlr > fcadp_maxadap || targ >= FCADP_MAXTARG || lun >= FCADP_MAXLUN)
		return ENODEV;
	if ((ci = fcadpctlr[ctlr]) == NULL)
		return ENODEV;
	if (targ == ci->host_id)
		return ENODEV;
	if (ci->dead)
		return EIO;

	/*
	 * Get controller mutex and allocate target and lun data structures
	 * if necessary.
	 */
	mutex_lock(&ci->opensema, PRIBIO);

	IOLOCK(ci->himlock, ci->himlockspl);
	if (!ci->lipmap_valid && (ci->lip_wait || ci->lip_issued))
		fc_wait_lip(ci, 1);
	if (!find_target(ci, targ))
		retval = ENODEV;
	IOUNLOCK(ci->himlock, ci->himlockspl);

	if (retval != SCSIALLOCOK)
		goto _allocout;

	ti = ci->target[targ];
	if (ti == NULL)
	{
		ti = kmem_zalloc(sizeof(*ti), 0);
		ti->number = targ;
		ti->plogi_payload = kmem_zalloc(sizeof(struct fc_plogi_payload), 0);
		ti->prli_payload = kmem_zalloc(sizeof(struct fc_prli_payload), 0);
		init_mutex(&ti->opensema, MUTEX_DEFAULT, "fct", (ci->number << 7) | targ);
		ci->target[targ] = ti;
	}

	li = SLI_INFO(lun_info);
	if (li == NULL)
	{
		li = kmem_zalloc(sizeof(*li), 0);
		SLI_INFO(lun_info) = li;
		li->number = lun;
		li->tinfo.si_inq = kmem_alloc(SCSI_INQUIRY_LEN, KM_CACHEALIGN);
		li->tinfo.si_sense = kmem_zalloc(SCSI_SENSE_LEN, KM_CACHEALIGN);
	}

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
			retval = EINVAL;
			goto _allocout;
		}
		else
			li->sense_callback = cb;
	}
	li->refcount++;

_allocout:
	mutex_unlock(&ci->opensema);
	return retval;
}

/* ARGSUSED */
void
fcadpfree(vertex_hdl_t lun_vhdl, void (*cb)())
{
	struct fcadpctlrinfo	*ci;
	struct fcadpluninfo	*li;
	u_char			 ctlr, targ;
	scsi_lun_info_t		*lun_info;

	lun_info = scsi_lun_info_get(lun_vhdl);
	ctlr = SLI_ADAP(lun_info);
	targ = SLI_TARG(lun_info);
	li = SLI_INFO(lun_info);
	ci = fcadpctlr[ctlr];

	mutex_lock(&ci->opensema, PRIBIO);
	if (li->exclusive)
		li->exclusive = 0;
	if (cb != NULL)
	{
		if (li->sense_callback == cb)
			li->sense_callback = NULL;
		else
			fc_lerr(ci, targ, li->number,
				"Error: fcadpfree - callback 0x%x not active\n",
				cb);
	}

	if (li->refcount != 0)
		li->refcount--;
	else
		fc_lerr(ci, targ, li->number,
			"Error: fcadpfree called when reference count is 0\n");
	mutex_unlock(&ci->opensema);
}


void
fcadpcommand(struct scsi_request *req)
{
	struct fcadpctlrinfo	*ci;
	struct fcadptargetinfo	*ti;
	struct fcadpluninfo	*li;
	scsi_lun_info_t		*lun_info;

	DBG(11, "fcadpcommand: 0x%x\n", req->sr_command[0]);

	req->sr_status = req->sr_scsi_status = req->sr_sensegotten =
	req->sr_ha_flags = 0;
	req->sr_spare = req->sr_ha = NULL;
	req->sr_resid = req->sr_buflen;

	if (req->sr_ctlr > fcadp_maxadap ||
	    req->sr_target >= FCADP_MAXTARG ||
	    req->sr_lun >= FCADP_MAXLUN ||
	    req->sr_notify == NULL)
	{
		req->sr_status = SC_REQUEST;
		goto _Err;
	}
	if (req->sr_buflen != 0 && ((uintptr_t) req->sr_buffer & 3))
	{
		req->sr_status = SC_ALIGN;
		goto _Err;
	}

	ci = fcadpctlr[req->sr_ctlr];
	ti = ci->target[req->sr_target];
	lun_info = scsi_lun_info_get(req->sr_lun_vhdl);
	li = SLI_INFO(lun_info);
	if (li == NULL)
	{
		req->sr_status = SC_REQUEST;
		goto _Err;
	}

	/*
	 * Attempt to login to target if necessary.
	 * Queue command.
	 * Issue commands until no more can be issued.
	 */
	IOLOCK(ci->himlock, ci->himlockspl);
	if (ci->dead)
	{
		req->sr_status = SC_TIMEOUT;
		goto _Unlock_Err;
	}
	if (!ci->lipmap_valid)
		fc_lip(ci, 0, TCBPRIM_LIPF7, 0);
	else if (ti->check_pending == 0)
	{
		if (!find_target(ci, req->sr_target))
		{
			req->sr_status = SC_TIMEOUT;
			goto _Unlock_Err;
		}
		else if (!ti->plogi)
		{
			/*
			 * If a previous login attempt ended in failure, but the
			 * user is trying to talk to the device, then we must try
			 * again, because they may have fixed it.
			 */
			if (ti->login_failed > FCADP_PLOGI_RETRY)
				ti->login_failed = 0;
			fc_login(ci, ti);
		}
	}
	fc_queue_scsireq(ci, ti, req);
	while (fc_start(ci))
		;
	IOUNLOCK(ci->himlock, ci->himlockspl);
	return;

_Unlock_Err:
	IOUNLOCK(ci->himlock, ci->himlockspl);
_Err:
	(*req->sr_notify)(req);
}


/*
 * Log in with target.  If already logged in with target, do a
 * Port Discovery to find if parameters have changed.
 */
static void
fc_login(register struct fcadpctlrinfo *ci, register struct fcadptargetinfo *ti)
{
	/*
	 * perform address discovery, if needed
	 */
	if (!ti->plogi && !ti->needplogi)
	{
		ti->needplogi = 1;
		ti->needprli = 1;
		ti->prli = 0;
		fc_queue_target(ci, ti);
	}
}


static void
fcadpdone(struct scsi_request *req)
{
	vsema(req->sr_dev);
}

/*
 * Find out which luns are supported
 */
#define GETLUNS_SENSELEN 14
#define GLSZ ((FCADP_MAXLUN * 8) + 8)
void
fc_getluns(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti,
	struct scsi_request *req, sema_t *sema)
{
	static u_char		 scsi[12]={0xA0, 0, 0, 0, 0, 0, GLSZ>>24,
					   GLSZ>>16, GLSZ>>8, (u_char) GLSZ, 0, 0};
	int			 i;
	struct report_luns_format {
		uint32_t	count;
		uint32_t	reserved;
		struct {
			uint16_t	lun_number;
			uint16_t	lun_levels[3];
		} lun_element[1];
	} *lun_list;
	uint8_t			*sense_buffer;
	int			 trycmd = 2;

	lun_list = kmem_zalloc((FCADP_MAXLUN * 8) + 8, VM_CACHEALIGN);
	sense_buffer = kmem_zalloc(GETLUNS_SENSELEN, 0);

	while (trycmd)
	{
		/*
		 * most of the fields are set in fcadpinfo
		 */
		req->sr_command = scsi;
		req->sr_cmdlen = 12;     /* report luns command length */
		req->sr_flags = SRF_DIR_IN | SRF_FLUSH | SRF_AEN_ACK;
		req->sr_timeout = 10 * HZ;
		req->sr_buffer = (u_char *) lun_list;
		req->sr_buflen = (FCADP_MAXLUN * 8) + 8;
		req->sr_sense = sense_buffer;
		req->sr_senselen = GETLUNS_SENSELEN;

		fcadpcommand(req);
		psema(sema, PRIBIO);

		/*
		 * Retry the REPORT LUNS on a unit attention
		 */
		if (req->sr_status == SC_GOOD &&
		    req->sr_sensegotten > 2 &&
		    req->sr_sense[2] == SC_UNIT_ATTN)
		{
			trycmd--;
		}
		else
		{
		    if ((req->sr_status == SC_GOOD) && (req->sr_scsi_status == ST_GOOD))
		    {
			IOLOCK(ci->himlock, ci->himlockspl);
			bzero(ti->lunmask, sizeof(ti->lunmask));
			/*
			 * Go through LUN list.  For FCP, we are only interested in the
			 * top 16 bits of the lun
			 */
			for (i = 0; i < lun_list->count / 8; i++)
			{
			    if (lun_list->lun_element[i].lun_number < FCADP_MAXLUN)
			    {
			        int lun;
				lun = lun_list->lun_element[i].lun_number;
				ti->lunmask[lun/8] |= 1 << (lun % 8);
			    }
			}
			IOUNLOCK(ci->himlock, ci->himlockspl);
		    }
		    trycmd = 0;
		}
	}

	kmem_free(lun_list, (FCADP_MAXLUN * 8) + 8);
	kmem_free(sense_buffer, GETLUNS_SENSELEN);
}


struct scsi_target_info *
fcadpinfo(vertex_hdl_t lun_vhdl)
{
	struct fcadpctlrinfo	*ci;
	struct fcadptargetinfo	*ti;
	struct fcadpluninfo	*li;
	struct scsi_request	*req;
	static u_char		 scsi[6];
	sema_t			*sema;
	struct scsi_target_info	*retval = NULL;
	u_char			 ctlr, targ, lun;
	scsi_lun_info_t		*lun_info;
	int			 retry = 0;

	lun_info = scsi_lun_info_get(lun_vhdl);
	ctlr = SLI_ADAP(lun_info);
	targ = SLI_TARG(lun_info);
	lun = SLI_LUN(lun_info);

	if (fcadpalloc(lun_vhdl, 0, NULL) != SCSIALLOCOK)
		{ci = NULL; goto out;}

	DBG(3, "fcadpinfo(%d,%d,%d)\n", ctlr, targ, lun);

	if ((ci = fcadpctlr[ctlr]) == NULL)
		goto out;
	ti = ci->target[targ];
	li = SLI_INFO(lun_info);

	mutex_lock(&ti->opensema, PRIBIO);

	/*
	 * We should be logged in to target.   Issue Inquiry.
	 */
	sema = kern_calloc(1, sizeof(sema_t));
	init_sema(sema, 0, "fci", (ctlr << 7) | targ);
	req = kern_malloc(sizeof(*req));

	do
	{
		bzero(req, sizeof(*req));
		scsi[0] = 0x12;  /* Inquiry command */
		scsi[1] = scsi[2] = scsi[3] = scsi[5] = 0;
		scsi[4] = SCSI_INQUIRY_LEN;

		req->sr_ctlr = ctlr;
		req->sr_target = targ;
		req->sr_lun = lun;
		req->sr_command = scsi;
		req->sr_cmdlen = 6;     /* Inquiry command length */
		req->sr_flags = SRF_DIR_IN | SRF_FLUSH | SRF_AEN_ACK;
		req->sr_timeout = 10 * HZ;
		req->sr_buffer = li->tinfo.si_inq;
		req->sr_buflen = SCSI_INQUIRY_LEN;
		req->sr_sense = NULL;
		req->sr_notify = fcadpdone;
		req->sr_dev = sema;
		req->sr_lun_vhdl = lun_vhdl;

		bzero(req->sr_buffer, req->sr_buflen);
		fcadpcommand(req);
		psema(sema, PRIBIO);

		if ((req->sr_status == SC_GOOD) && (req->sr_scsi_status == ST_GOOD))
		{
			ci->scsi_cmds = 1;

			/*
			 * If the LUN is not supported and is not a candidate
			 * for failover, then return NULL.  
			 */
			if ((li->tinfo.si_inq[0] & 0xC0) == 0x40 && 
			    !fo_is_failover_candidate(li->tinfo.si_inq, lun_vhdl))
			{
				fc_lerr(ci, ti->number, li->number,
					"Notice: lun not valid, peripheral qualifier 0x%x\n",
					(li->tinfo.si_inq[0] & 0xE0) >> 5);
			}
			else
			{
				scsi_device_update(li->tinfo.si_inq, lun_vhdl);
				li->tinfo.si_maxq = ci->maxqueue;
				li->tinfo.si_ha_status = SRH_TAGQ | SRH_QERR0 |
							SRH_MAPUSER | SRH_ALENLIST;
				retval = &li->tinfo;
			}

			if (lun == 0)
				fc_getluns(ci, ti, req, sema);
		}
	}
	while (retry++ < 5 &&
	       (req->sr_status == SC_ATTN ||
		req->sr_status == SC_CMDTIME ||
		req->sr_status == SC_HARDERR));

	kern_free(req);
	freesema(sema);
	kern_free(sema);
	mutex_unlock(&ti->opensema);
	fcadpfree(lun_vhdl, NULL);

out:
	if (fcadpdebug && (retval == NULL))
	{
		if (ci != NULL)
			fc_terr(ci, targ, "fcadpinfo inquiry failed\n");
		else
			cmn_err(CE_CONT, "fcadpinfo(%d,%d) %v alloc failed\n",
				targ, lun, lun_vhdl);
	}
	return retval;
}


/********************* Timeout Routines ***********************/
void
fcadp_cmd_dump(struct fcadpctlrinfo *ci)
{
	struct fcadptargetinfo *ti;
	struct fc_tcbhdr *tcbhdr;
	char number[16];
	int  i, j;

	if (fcadpdebug)
	{
		j = 0;
		ci->lip_buffer[0] = 0;
		for (i = 0; i < FCADP_MAXTARG; i++)
		{
			ti = ci->target[i];
			if (ti == NULL)
				continue;
			tcbhdr = ti->active;
			while (tcbhdr != NULL)
			{
				j++;
				if (tcbhdr->th_type == TCBTYPE_SCSI)
					sprintf(number, " 0x%x(%d)",
						tcbhdr->th_req->sr_command[0],
						tcbhdr->th_ticker * 2);
				else
					sprintf(number, " T%d(%d)",
						tcbhdr->th_type,
						tcbhdr->th_ticker * 2);
				strcat(ci->lip_buffer, number);
				if (j == 5)
				{
					fc_terr(ci, i, "Active %s\n", ci->lip_buffer);
					j = 0;
					ci->lip_buffer[0] = 0;
				}
				tcbhdr = tcbhdr->th_next;
			}
			if (j > 0)
			{
				fc_terr(ci, i, "Active %s\n", ci->lip_buffer);
				j = 0;
				ci->lip_buffer[0] = 0;
			}
		}
	}
}

/*
 * Return 1 if the controller was reset.
 */
static int
fc_check_timers(struct fcadpctlrinfo *ci, struct fcadptargetinfo *ti)
{
	struct fc_tcbhdr	*tcbhdr;
	int			 retval = 0;

	/*
	 * Go through list of active commands, first checking to see if
	 * a command has aged and then checking for forward progress.
	 * If the lipmap is not valid, that implies that we are in the
	 * middle of loop initialization, probably as a part of error
	 * recovery, so we should not do any more timeouts.
	 */
	IOLOCK(ci->himlock, ci->himlockspl);
	if (ci->lipmap_valid && ti->active)
	{
		if (ti->plogi && ti->q_state < FCTI_STATE_AGE)
		{
			tcbhdr = ti->active;
			while (tcbhdr != NULL)
			{
			    /*
			     * Check for one greater than tick limit so that
			     * we don't get both forward progress and age
			     * timeouts at the same time.
			     */
			    if (++tcbhdr->th_ticker > tcbhdr->th_ticklimit)
			    {
				struct scsi_request *req;
				if ((req = tcbhdr->th_req) != NULL)
				{
				    fc_lerr(ci, ti->number, req->sr_lun, "Aged SCSI cmd"
					" 0x%x, size %d -- quiescing target\n",
				        req->sr_command[0], req->sr_buflen);
				}
				else
				{
				    fc_terr(ci, ti->number,
					"Notice: aged command -- quiescing target\n");
				}
				ti->q_state += FCTI_STATE_AGE;
				fc_dequeue_target_scsi(ci, ti);
				break;
			    }
			    tcbhdr = tcbhdr->th_next;
			}
		}

		/*
		 * Check for completion of the first command in the active list.
		 * Active list is sorted by timeout length, longest first, with
		 * the exception that a PLOGI will always be first in the active
		 * list.  If we aren't logged in and the first tcbtype is not a
		 * PLOGI, then we don't expect any forward progress, so I don't
		 * increment the counter.
		 */
		if ((ti->plogi || ti->active->th_type == TCBTYPE_PLOGI) &&
		    ++ti->timeout_progress >= ti->active->th_ticklimit)
		{
			retval = 1;
			if (ti->active->th_type == TCBTYPE_PLOGI)
				fc_terr(ci, ti->number, "Error: N-port login timeout:"
				        " resetting adapter and LIPRST target\n");
			else
				fc_terr(ci, ti->number, "Error: forward progress timeout"
				        ": resetting adapter and initializing loop\n");

			/*
			 * Dump all outstanding commands
			 */
			fcadp_cmd_dump(ci);

			/*
			 * A little background:
			 * Port Logins to all targets are done only after LIP and
			 * before any other commands to different targets.
			 * So if needplogi is set, that means we timed out a Port
			 * Login command.  We might recover by LIPRST to the target.
			 */
			if (fc_reset_controller(ci) == 0)
			{
				/*
				 * Currently, the LIP type passed to fc_lip is
				 * ignored after the controller has been reset,
				 * since the HIM/ctlr automatically send LIPfail.
				 * We'd prefer a LIPRST if we could get it.
				 */
				if (ti->active->th_type == TCBTYPE_PLOGI)
				{
				    ti->login_failed++;
				    if (ti->login_failed > FCADP_PLOGI_RETRY)
					fc_terr(ci, ti->number,
						"N-port login failed\n");
				    fc_lip(ci, 0, TCBPRIM_LIPRST, ti->number);
				}
				else
				{
				    ci->error_id = ti->number;
				    if (ti->active->th_req != NULL)
				        ti->active->th_req->sr_status = SC_CMDTIME;
				    fc_lip(ci, 0, TCBPRIM_LIPF7, 0);
				}
			}
		}
	}
	IOUNLOCK(ci->himlock, ci->himlockspl);
	return retval;
}

static void
fc_timeout_check(struct fcadpctlrinfo *ci)
{
	struct fcadptargetinfo	*ti;
	int			 targ;
	void fcadpintr(struct fcadpctlrinfo *);

	if (ci->cmdcount > 0)
	{
		fcadpintr(ci);

		/*
		 * Check for command timeouts less frequently than possible
		 * missed interrupts.
		 */
		ci->timeout_interval += INTRCHECK_INTERVAL;
		if (ci->timeout_interval >= TIMEOUTCHECK_INTERVAL)
		{
			for (targ = 0; targ < FCADP_MAXTARG; targ++)
			{
				ti = ci->target[targ];
				if (ti != NULL)
					if (fc_check_timers(ci, ti))
						break;
			}
			ci->timeout_interval = 0;
		}
	}
#if 0
	else if (ci->lip_wait || ci->lip_issued)
	{
		int x = 0;
		int y = 0;

		if (ci->lip_wait) x = 1;
		if (ci->lip_issued) x |= 2;
		fcadpintr(ci);
		if (ci->lip_wait) y = 1;
		if (ci->lip_issued) y |= 2;
		if (x != y)
			fc_cerr(ci, "lip status changed after fcadpintr\n");
	}
#endif
	ci->timeout_id = timeout(fc_timeout_check, ci, INTRCHECK_INTERVAL);
}


void
fc_lip_error(struct fcadpctlrinfo *ci, char *reason, int incr_time)
{
	int	 tmp;

	/*
	 * First reset the chip.
	 * If that is successful, determine if we should continue trying
	 *   to use the controller.
	 */
	tmp = fc_reset_controller(ci);

	if (tmp == 0)
	{
		if (incr_time)
		{
			++ci->lip_attempt;
			if (ci->scsi_cmds)
			{
				if (ci->lip_attempt >= 60)
				{
					ci->scsi_cmds = 0;
					tmp = 1;
				}
			}
			else if (ci->lip_attempt >= 15)
				tmp = 1;
			else if (ci->LoS && ci->lip_attempt >= 2)
				tmp = 1;
		}
	}

	/*
	 * If we don't want to retry loop initialization, wake up everyone
	 *   waiting for lip and declare it a failure.
	 * Otherwise, retry the LIP.
	 */
	if (tmp)
	{
		fc_cerr(ci, "Loop init%s -- giving up [%d]\n",
			reason, ci->lip_attempt);
		ci->off_line = 1;
		fc_clear_controller(ci, 1, SC_TIMEOUT);
		ci->lip_attempt = 0;
	}
	else
	{
		if (ci->lip_attempt % 8 == 6)
		{
			if (fcadpdebug || !fcadp_init_time)
				fc_cerr(ci, "Loop init%s -- LIPRST all[%d]\n",
					reason, ci->lip_attempt);
			fc_lip(ci, 0, TCBPRIM_LIPRST, 255);
		}
		else
		{
			if (fcadpdebug || !fcadp_init_time)
				fc_cerr(ci, "Loop init%s -- retrying [%d]\n",
					reason, ci->lip_attempt);
			fc_lip(ci, 0, TCBPRIM_LIPF7, 0);
		}
	}
}


void
fc_lip_timeout(struct fcadpctlrinfo *ci)
{
	char	*reason;

	IOLOCK(ci->himlock, ci->himlockspl);
	if (ci->lip_timeout == 0)
		goto _end;
	ci->lip_timeout = 0;

	if (ci->lip_wait)
		reason = " timeout: LIP READY not received";
	else if (ci->lip_issued)
		reason = " timeout: LIP TCB not completed";
	else
	{
		/*
		 * If we ever get here, it's a bug.  Not fatal, but we
		 * should know about it, so I print the message so the
		 * bug can be fixed.
		 */
		fc_cerr(ci, "Notice: delayed loop initialization timeout\n");
		goto _end;
	}
	ASSERT(ci->lip_wait || ci->lip_issued || ci->lipmap_valid);

	fc_lip_error(ci, reason, 1);

_end:
	IOUNLOCK(ci->himlock, ci->himlockspl);
}


void
fill_prim_tcb(struct fcadpctlrinfo *ci, struct primitive_tcb *tcb,
	      struct reference_blk *ref, uint8_t arb_request,
	      uint8_t al_pd, uint8_t opn_prim, uint8_t ulm_prim, uint8_t cls_prim,
	      uint8_t alc_control, uint8_t prim_control, uint8_t flag)
{
	bzero(ref, sizeof(*ref));

	ref->control_blk = ci->control_blk;
	ref->tcb = (struct _tcb *) tcb;
	ref->tcb_number = 0;
	ref->func = SH_PRIMITIVE_REQUEST;
	ref->sub_func = flag;

	tcb->arb_request = arb_request;
	tcb->al_pd = al_pd;
	tcb->opn_prim = opn_prim;
	tcb->ulm_prim = ulm_prim;
	tcb->cls_prim = cls_prim;
	tcb->alc_control = alc_control;
	tcb->prim_control = prim_control;
}

/*
 * This function is called to send a LIP(F7,F7) or LIP(F7,AL_PS)
 * primitive, depending on value of host_id in ctlrinfo structure.
 */
static void
fc_lip(struct fcadpctlrinfo *ci, int wait, uint lip_option, uint8_t target)
{
	struct fc_tcbhdr	*tcbhdr = ci->prim_tcb;
	struct reference_blk	 refblk;
	uint8_t			 participating;

	if (wait)
		IOLOCK(ci->himlock, ci->himlockspl);

	if (ci->dead)
	{
		if (wait)
			IOUNLOCK(ci->himlock, ci->himlockspl);
		return;
	}

	if (ci->lip_wait || ci->lip_issued)
		goto _await_lip_complete;

	if (ci->off_line)
	{
		sh_enable_irq(ci->control_blk);
		ci->off_line = 0;
	}

	DBGC(3, ci, "Issuing lip primitive\n");
	ci->quiesce |= LIP_QUIESCE;
	ci->primbusy = 1;
	ci->lip_wait = 1;
	ci->lipmap_valid = 0;
	if (ci->host_id == 0xFF)
		participating = 0;
	else
		participating = PARTICIPATING;

	switch (lip_option)
	{
	case TCBPRIM_LIPF7:
		fill_prim_tcb(ci,
		    (struct primitive_tcb *) tcbhdr->th_tcb,
		    &refblk,
		    REQ_NON_ACTIVE,
		    0,
		    REQ_NON_ACTIVE,
		    REQ_INITIALIZE | participating,
		    REQ_NON_ACTIVE,
		    0xFF,
		    0x80,
		    LIP_TYPE_PRIMITIVE);
		break;

	case TCBPRIM_LIPRST:
		fill_prim_tcb(ci,
		    (struct primitive_tcb *) tcbhdr->th_tcb,
		    &refblk,
		    REQ_NON_ACTIVE,
		    (target == 255 ? 255 : tid_to_alpa[target]),
		    REQ_NON_ACTIVE,
		    REQ_INITIALIZE_RESET | participating,
		    REQ_NON_ACTIVE,
		    0xFF,
		    0x80,
		    LIP_TYPE_PRIMITIVE);
		break;
	
	default:
		ASSERT(0);
	}

	fc_swapctl((uint8_t *) refblk.tcb, sizeof(struct _tcb));

	/*
	 * Issue primitive TCB to send LIP primitive onto link.  Then
	 * wait for completion of the lip.
	 */
	tcbhdr->th_type = lip_option;
	sh_special_tcb(&refblk);

	/* It's possible that we've already issued a LIP TCB at this point */
	if (!ci->lip_timeout)
		ci->lip_timeout = timeout(fc_lip_timeout, ci, LIPTIMEOUT);

_await_lip_complete:
	if (wait)
		fc_wait_lip(ci, 0);  /* UNLOCK done in fc_wait_lip */
}


static void
fill_lip_tcb(struct fcadpctlrinfo *ci, struct reference_blk *refblk)
{
	struct lip_tcb	*tcb;

	tcb = (struct lip_tcb *) refblk->tcb;
	bzero(tcb, sizeof(struct lip_tcb));

	assign_int32(&tcb->xfer_rdy_count0, FC_LILP_SIZE);
	tcb->hst_cfg_ctl = 0x11;

	/* inform the sequencer that table 15 is in ram page 1 */
	tcb->table_page = 1;

	/*
	 * Most frame header bytes are 0
	 */
	tcb->fhead.our_xid0 =  0xff;
	tcb->fhead.our_xid1 =  0xff;
	tcb->fhead.their_xid0 =  0xff;
	tcb->fhead.their_xid1 =  0xff;
	tcb->fhead.their_aid0 =  0xef;
	tcb->fhead.r_ctl =  0x22;
	tcb->fhead.delimiter =  0x05;
	tcb->fhead.f_ctl2 = 0x38;		
	tcb->fhead.type  = 0x01;
	tcb->fhead.seq_cfg1 = 0x02; 

	if (ci->host_id != 0xFF)
	{
		int bitpos = 1 + 126 - ci->host_id;
		tcb->prev_ALPA0 = 4 + (bitpos / 8);
		tcb->prev_ALPA1 = 0x80 >> (bitpos % 8);
	}
	else if (fcadp_hostid)
	{
		int bitpos = 1 + 126 - fcadp_hostid;
		tcb->hard_ALPA0 = 4 + (bitpos / 8);
		tcb->hard_ALPA1 = 0x80 >> (bitpos % 8);
	}

	bcopy(&ci->portname, &tcb->portname0, sizeof(ci->portname));
	bzero(ci->lipmap, 20);
	assign_int64(&tcb->info_host_addr0, kvtopci(ci->lipmap));
	assign_int64(&tcb->next_tcb_addr0, kvtopci(fc_next_rrtcb(ci)));
}


/*
 * We've received a LIP primitive.  Issue the LIP TCB.
 */
static void
perform_lip(struct fcadpctlrinfo *ci)
{
	struct reference_blk	 refblk;
	struct fc_tcbhdr	*tcbhdr;

	if (ci->lip_issued)
		return;

	DBGC(3, ci, "Beginning lip\n");
	ci->quiesce |= LIP_QUIESCE;
	ci->lipmap_valid = 0;
	ci->lip_wait = 0;
	ci->lip_issued = 1;
	tcbhdr = ci->lip_tcb;
	tcbhdr->th_type = TCBTYPE_LIP;

	/*
	 * Issue LIP tcb
	 */
	refblk.control_blk = ci->control_blk;
	refblk.tcb = tcbhdr->th_tcb;
	refblk.tcb_number = tcbhdr->th_number;
	refblk.func = SH_DELIVER_LIP_TCB;
	fill_lip_tcb(ci, &refblk);

#if 0 /* XXX lip tcb not DMA'd for now, no need war broken hw */
	fc_swapctl((uint8_t *) refblk.tcb, sizeof(struct _tcb));
#endif
	TCB_ACTIVE(ci, refblk.tcb_number);
	sh_special_tcb(&refblk);

	if (ci->lip_timeout == 0)
		ci->lip_timeout = timeout(fc_lip_timeout, ci, LIPTIMEOUT);
}


/*
 * A lip has been done, so we know the addresses of devices on the loop.
 * For each device, call fcadpinfo to login with device and perform an
 * Inquiry command to LUN 0 to find out what type of device it is.
 */
static void
fc_probe(struct fcadpctlrinfo *ci, vertex_hdl_t ctlr_vhdl)
{
	struct fcadptargetinfo	*ti;
	struct fcadpluninfo	*li;
	uint8_t			 tid;
	uint			 lun;
	vertex_hdl_t		 lun_vhdl;
	scsi_lun_info_t		*lun_info;

	mutex_lock(&ci->probesema, PRIBIO);
	for (tid = 0; tid < FCADP_MAXTARG; tid++)
	{
		/*
		 * Check if device is on loop.  If not, remove it if it was
		 * added to the graph at one point.
		 */
		IOLOCK(ci->himlock, ci->himlockspl);
		while (!ci->lipmap_valid && (ci->lip_wait || ci->lip_issued))
			fc_wait_lip(ci, 1);
		lun = find_target(ci, tid);
		IOUNLOCK(ci->himlock, ci->himlockspl);
		if (!lun)
		{
		    fcadp_device_remove(ci, ctlr_vhdl, tid, 0);
		    continue;
		}

		lun_vhdl = fcadp_device_add(ctlr_vhdl, tid, 0);
		if (fcadpinfo(lun_vhdl))
		{
		    ti = ci->target[tid];
		    ti->present = 1;

		    li = SLI_INFO(scsi_lun_info_get(lun_vhdl));
		    li->present = 1;

		    for (lun = 1; lun < FCADP_MAXLUN; lun++)
		    {
			int	good_lun;

			good_lun = 0;
			if (ti->lunmask[lun/8] & (1 << (lun % 8)))
			{
			    lun_vhdl = fcadp_device_add(ctlr_vhdl, tid, lun);
			    if (fcadpinfo(lun_vhdl))
			    {
				li = SLI_INFO(scsi_lun_info_get(lun_vhdl));
				li->present = 1;
				good_lun = 1;
			    }
			}
			/*
			 * Attempt to remove the lun if it once existed and
			 * it is not open/in use.
			 */
			if (!good_lun &&
			    ((lun_vhdl = scsi_lun_vhdl_get(ci->ctlrvhdl, tid, lun)) !=
				    GRAPH_VERTEX_NONE) &&
			    (lun_info = scsi_lun_info_get(lun_vhdl)) &&
			    (li = SLI_INFO(lun_info)) &&
			    li->present &&
			    li->refcount == 0)
			{
			    li->present = 0;
			    fcadp_lun_remove(ctlr_vhdl, tid, lun);
			}
		    }
		}
		else
		{
		    fcadp_device_remove(ci, ctlr_vhdl, tid, 1);
		}
	}
	mutex_unlock(&ci->probesema);
}


void
fill_unsol_tcb(struct fcadpctlrinfo *ci, struct fc_tcbhdr *tcbhdr)
{
	struct _tcb	*tcb;

	tcb = tcbhdr->th_tcb;
	tcb->control = 1;
	bzero(tcbhdr->th_sense, 64);
	assign_int64((uint8_t *) &tcb->scsi_cmd, kvtopci(tcbhdr->th_sense));
	assign_int64(&tcb->info_host_addr0, kvtopci(tcbhdr->th_map));
	assign_int16((uint8_t *) &tcb->tcb_index, tcbhdr->th_number);
	assign_int64(&tcb->next_tcb_addr0, kvtopci(fc_next_rrtcb(ci)));
}


/*
 * type 0: loop port bypass
 * type 1: loop port enable
 * type 2: loop port enable all
 */
int
fc_bypass(struct fcadpctlrinfo *ci, int al_pd, int type)
{
	struct fc_tcbhdr	*tcbhdr;
	struct reference_blk	 refblk;
	int			 retval = 0;

	IOLOCK(ci->himlock, ci->himlockspl);
	if (!ci->lipmap_valid || ci->primbusy || ci->userprim)
	{
		IOUNLOCK(ci->himlock, ci->himlockspl);
		return EBUSY;
	}

	ci->primbusy = 1;
	ci->userprim = 1;
	ci->userprim_status = 0;

	tcbhdr = ci->prim_tcb;
	switch (type)
	{
	case 0:
		fill_prim_tcb(ci, (struct primitive_tcb *) tcbhdr->th_tcb,
			&refblk,
			0x41,
			tid_to_alpa[al_pd],
			0x43,
			0x4C,
			0x47,
			0xF7,
			0x40,
			LIP_TYPE_PRIMITIVE);
		tcbhdr->th_type = TCBPRIM_LPB;
		break;

	case 1:
		fill_prim_tcb(ci, (struct primitive_tcb *) tcbhdr->th_tcb,
			&refblk,
			0x41,
			tid_to_alpa[al_pd],
			0x43,
			0x4E,
			0x47,
			0xF7,
			0x20,
			LIP_TYPE_PRIMITIVE);
		tcbhdr->th_type = TCBPRIM_LPE;
		break;

	case 2:
		fill_prim_tcb(ci, (struct primitive_tcb *) tcbhdr->th_tcb,
			&refblk,
			0x41,
			0, /* ignored XXX? */
			0x43,
			0x4F,
			0x47,
			0xF7,
			0x20,
			LIP_TYPE_PRIMITIVE);
		tcbhdr->th_type = TCBPRIM_LPEALL;
		break;
	}
	fc_swapctl((uint8_t *) refblk.tcb, sizeof(struct _tcb));
	sh_special_tcb(&refblk);
	IOUNLOCK(ci->himlock, ci->himlockspl);

	psema(&ci->userprim_sema, PRIBIO);
	retval = ci->userprim_status;
	ci->userprim = 0;

	return retval;
}


void
fc_stop_quiesce(struct fcadpctlrinfo *ci)
{
	IOLOCK(ci->himlock, ci->himlockspl);
	if (ci->quiesce_timeout)
	{
		ci->quiesce &= ~USR_QUIESCE;
		ci->quiesce_timeout = 0;
		ci->quiesce_time = 0;
		while (fc_start(ci))
			;
	}
	IOUNLOCK(ci->himlock, ci->himlockspl);
}

int
fcadpioctl(vertex_hdl_t ctlr_vhdl, uint cmd, struct scsi_ha_op *op)
{
	scsi_ctlr_info_t	*ctlr_info;
	struct fcadpctlrinfo	*ci;
	int			 tmp;
	int			 err = 0;

	ctlr_info = scsi_ctlr_info_get(ctlr_vhdl);
	ci = SCI_INFO(ctlr_info);
	if (ci == NULL)
		return EINVAL;
	
	if (cmd == SOP_DEBUGLEVEL && op->sb_arg != 999)
	{
		if (op->sb_arg == 99)
		{
			fc_cerr(ci, "is");
			cmn_err(CE_CONT, " %v\n", ci->ctlrvhdl);
		}
		else
			fcadpdebug = op->sb_arg;
		return 0;
	}

	if (cmd == SOP_MAKE_CTL_ALIAS)
	{
		sprintf(ci->hwgname, "fcadp%d", (int) (uintptr_t) op);
		return 0;
	}

	if (ci->dead)
	{
		fc_cerr(ci, "Adapter fatal error -- unuseable\n");
		return EIO;
	}
	
	switch (cmd)
	{
	case SOP_DEBUGLEVEL:
		fc_cerr(ci, "fcadpioctl DEBUG: shutting down controller\n");
		IOLOCK(ci->himlock, ci->himlockspl);
		ci->off_line = 1;
		ci->lipmap_valid = 0;
		fc_clear_controller(ci, 1, SC_TIMEOUT);
		ci->lip_attempt = 0;
		IOUNLOCK(ci->himlock, ci->himlockspl);
		break;

	case SOP_RESET:
	case SOP_LIP:
	case SOP_LIPRST:
		tmp = 0;
		IOLOCK(ci->himlock, ci->himlockspl);
		if (!ci->dead && !fc_reset_controller(ci))
		{
			ci->lip_attempt = 0;
			tmp = 1;
		}
		IOUNLOCK(ci->himlock, ci->himlockspl);
		if (tmp)
		{
			if (cmd == SOP_LIPRST)
				fc_lip(ci, 1, TCBPRIM_LIPRST, op->sb_arg);
			else
				fc_lip(ci, 1, TCBPRIM_LIPF7, 0);
		}
		else
			err = EIO;
		break;
	
	case SOP_SCAN:
		fc_probe(ci, ctlr_vhdl);
		break;

	case SOP_QUIESCE:
		if (op->sb_opt == 0 || op->sb_arg == 0)
			return EINVAL;

		IOLOCK(ci->himlock, ci->himlockspl);
		if (ci->quiesce & USR_QUIESCE)
		{
			if (ci->cmdcount == 0)
			{
				untimeout(ci->quiesce_timeout);
				ci->quiesce_timeout =
					timeout(fc_stop_quiesce, ci, op->sb_arg);
			}
		}
		else
		{
			ci->quiesce |= USR_QUIESCE;
			if (ci->cmdcount == 0)
			{
				ci->quiesce_timeout =
				timeout(fc_stop_quiesce, ci, op->sb_arg);
			}
			else
			{
				ci->quiesce_timeout =
				timeout(fc_stop_quiesce, ci, op->sb_opt);
				ci->quiesce_time = op->sb_arg;
			}
		}
		IOUNLOCK(ci->himlock, ci->himlockspl);
		break;	

	case SOP_UN_QUIESCE:
		IOLOCK(ci->himlock, ci->himlockspl);
		ci->quiesce &= ~USR_QUIESCE;
		untimeout(ci->quiesce_timeout);
		ci->quiesce_timeout = 0;
		ci->quiesce_time = 0;
		while (fc_start(ci))
			;
		IOUNLOCK(ci->himlock, ci->himlockspl);
		break;

	case SOP_QUIESCE_STATE:
		if (ci->quiesce & USR_QUIESCE)
		{
			if (ci->cmdcount != 0)
				tmp = QUIESCE_IN_PROGRESS;
			else
				tmp = QUIESCE_IS_COMPLETE;
		}
		else
			tmp = NO_QUIESCE_IN_PROGRESS;
		copyout(&tmp, (void *) op->sb_addr, sizeof(int));
		break;
	
	case SOP_ENABLE_PBC:
		if (op->sb_arg < 126)
			err = fc_bypass(ci, op->sb_arg, 0);
		else
			err = EINVAL;
		break;

	case SOP_DISABLE_PBC:
		if (op->sb_arg < 126 || op->sb_arg == 255)
			err = fc_bypass(ci, op->sb_arg, 1);
		else
			err = EINVAL;
		break;
	
	case SOP_LPEALL:
		err = fc_bypass(ci, 0, 2);
		break;
	
	default:
		err = EINVAL;
	}

	return err;
}


void
fc_alloc_struct(struct fcadpctlrinfo *ci, struct shim_config *config)
{
	struct fc_tcbhdr	*tcbheader;
	u_char			*sense_data;
	int			 i, tmp;
	int			 cur_allocation;
	short			 curtcb = 0;

	/*
	 * Initialize locks and semaphores
	 */
	INITLOCK(&ci->himlock, "fch", ci->number);
	init_mutex(&ci->opensema, MUTEX_DEFAULT, "fcu", ci->number);
	init_mutex(&ci->probesema, MUTEX_DEFAULT, "fcp", ci->number);
	init_sema(&ci->lipsema, 0, "fcl", ci->number);
	init_sema(&ci->userprim_sema, 0, "fcu", ci->number);
	ci->config = kern_calloc(1, sizeof(struct shim_config));
	bcopy(config, ci->config, sizeof(struct shim_config));

#if 0
	printf("\tctrl blk %d, done queue %d, num_tcb %d, num_unsol %d\n",
	       ci->config->control_blk_size, ci->config->done_q_size,
	       ci->config->number_tcbs, ci->config->num_unsol_tcbs);
#endif

	/*
	 * Allocate requested memory and TCB's
	 */
	ci->control_blk = ci->config->control_blk =
		kmem_zalloc(ci->config->control_blk_size, VM_DIRECT);

	/*
	 * Workarounds for Hub2.1 problem; allocate extra memory for doneq and
	 * interrupt status data, so that we are more likely to avoid cacheline
	 * merges when a cacheline is shared.
	 */
	sense_data = kmem_zalloc(ci->config->done_q_size * 2, VM_DIRECT | VM_CACHEALIGN);
	ci->doneq = ci->config->doneq = (struct _doneq *)
		&sense_data[ci->config->done_q_size / 2];
	ci->status_addr = ci->config->dma = (u_char *)
		((uintptr_t) kmem_alloc(512, VM_DIRECT | VM_CACHEALIGN) + 256);

	ci->lipmap = kmem_zalloc(FC_LILP_SIZE, VM_DIRECT | VM_CACHEALIGN);

	/*
	 * Allocate a page at a time of sense buffers.  We want VM_DIRECT so
	 * that we don't use TLB entries for them, and we only want one at a
	 * time, so that we won't need contiguous direct memory when we want
	 * to attach on a system that has been running for a while.
	 */
	tmp = ci->config->number_tcbs;
	ci->maxqueue = tmp - FCADP_TCB_SETASIDE - ci->config->num_unsol_tcbs;
	ci->tcbptrs = kmem_zalloc(tmp * sizeof(void *), VM_DIRECT);
	ci->tcbqueue = kmem_zalloc(ci->config->number_tcbs * sizeof(tcb),
				  VM_DIRECT | VM_CACHEALIGN);
	curtcb = tmp - 1;
	while (tmp != 0)
	{
		/*
		 * Allocate a page's worth of TCBs and associated sense data.
		 * Find the lesser of the number of TCBs on a page or the
		 * number of TCBs remaining to allocate.
		 */
		cur_allocation = tmp > (ctob(1) / FCADP_MAXSENSE) ?
				       (ctob(1) / FCADP_MAXSENSE) : tmp;
		sense_data = kvpalloc(1, VM_DIRECT, 0);
		bzero(sense_data, ctob(1));
		tcbheader = kern_calloc(cur_allocation, sizeof(struct fc_tcbhdr));
		for (i = cur_allocation - 1; i >= 0; i--, curtcb--)
		{
			ci->tcbptrs[curtcb] = &tcbheader[i];
			TCB_INACTIVE(ci, curtcb);
			tcbheader[i].th_number = curtcb;
			tcbheader[i].th_map = kmem_zalloc(
				sizeof(SH_SG_DESCRIPTOR) * FCADP_MAPSIZE,
				VM_DIRECT | VM_CACHEALIGN);
			tcbheader[i].th_sense = &sense_data[i * FCADP_MAXSENSE];
			tcbheader[i].th_next = &tcbheader[i+1];
		}
		tcbheader[cur_allocation-1].th_next = ci->freetcb;
		ci->freetcb = tcbheader;
		tmp -= cur_allocation;
	}

	/*
	 * Now that all TCB's have been allocated, reserve TCB 0 for
	 * primitive TCB's.
	 */
	ci->prim_tcb = ci->freetcb;
	ci->freetcb = ci->freetcb->th_next;

	/*
	 * Reserve TCB N for LIPs, where N is the first tcb after the
	 * unsolicited TCBs.
	 * Find last unsolicited TCB.  Point the next field of the last
	 * unsolicited TCB to the the TCB _after_ the LIP TCB.  This removes
	 * the LIP TCB from the pool.
	 */
	tcbheader = ci->freetcb;
	for (i = 1; i < ci->config->num_unsol_tcbs; i++)
		tcbheader = tcbheader->th_next;
	ci->lip_tcb = tcbheader->th_next;
	tcbheader->th_next = tcbheader->th_next->th_next;
	ci->prim_tcb->th_next = ci->lip_tcb->th_next = NULL;
	ci->prim_tcb->th_tcb = kmem_zalloc(sizeof(struct _tcb), KM_CACHEALIGN);
	ci->lip_tcb->th_tcb = kmem_zalloc(sizeof(struct _tcb), KM_CACHEALIGN);
	ci->config->primitive_tcb = ci->prim_tcb->th_tcb;
	ci->config->first_tcb = ci->tcbqueue;	/* Priming TCB */

	/*
	 * Allocate last resort DMA map.
	 * We need enough pages to hold a map of size equal to the maximum
	 * possible transfer.  The map needs to be contiguous.  One page
	 * can map 16MB at a minimum.
	 */
	tmp = btoc(v.v_maxdmasz * sizeof(SH_SG_DESCRIPTOR));
	ci->bigmap = kvpalloc(tmp, VM_DIRECT, 0);
}


/*
 * Deliver unsolicited TCBs to sequencer.
 */
void
fc_unsol_tcb_init(struct fcadpctlrinfo *ci)
{
	struct fc_tcbhdr	*tcbheader;
	int			 tmp;

	for (tmp = 1; tmp <= ci->config->num_unsol_tcbs; tmp++)
	{
		struct reference_blk ref;

		/*
		 * The TCB's that we get here will never be put back into the
		 * freelist, so they don't count towards the maxqueue limit.
		 */
		tcbheader = fc_gettcbhdr(ci);
		tcbheader->th_type = TCBTYPE_UNSOL;
		fill_unsol_tcb(ci, tcbheader);
		ref.control_blk = ci->control_blk;
		ref.tcb = tcbheader->th_tcb;
		ref.tcb_number = tcbheader->th_number;
		ASSERT(ref.tcb_number == tmp);
		fc_swapctl((uint8_t *) ref.tcb, sizeof(struct _tcb));
		TCB_ACTIVE(ci, ref.tcb_number);
		sh_deliver_tcb(&ref);
	}
}

/*
 * Adaptec has reported that running the chip at 25 Mhz after reset
 * or poweron results in a higher likelihood of a successful bypass
 * on a hub.  We've been running with clock disabled. To change this,
 * change the RESET_SPEED and RESET_MASK compiler variables.  That
 * will cause board reprogrammings.  Note that board reprogrammings
 * (done at system startup time before filesystem mounts) may result
 * in a hang or panic.
 * 		disable	25MHz 
 * RESET_SPEED	0xF	0xC
 * RESET_MASK	0xF	0xF
 */
#define RESET_SPEED 0xF
#define RESET_MASK 0xF

/*
 * Return 0 if controller successfully initialized.  1 if not.
 */
static int
fcadp_ctlrinit(struct fcadpctlrinfo *ci, int alloc)
{
	struct shim_config	 config;
	int			 tmp;
	uint16_t		 media_type;
	uint16_t		 control_word;
        int write_seeprom(REGISTER, uint8_t *, int , int , ushort *);

	bzero(&config, sizeof(config));
	config.base_address.reg = ci->ibar0_addr;
	config.seqmembase.reg = ci->ibar1_addr;
	config.pcicfgbase.reg = ci->config_addr;

	/*
	 * Get chip configuration.
	 */
	if (tmp = sh_get_configuration(&config))
	{
		fc_cerr(ci, "error %d in getting config\n", tmp);
		ci->dead = 1;
		if (!alloc)
			fc_clear_controller(ci, 1, SC_TIMEOUT);
		return 1;
	}
	if (config.num_unsol_tcbs > FCADP_MAX_UNSOL_TCB)
		config.num_unsol_tcbs = FCADP_MAX_UNSOL_TCB;
	if (config.number_tcbs > FCADP_MAXCMD)
	{
		config.number_tcbs = FCADP_MAXCMD;
		config.done_q_size = FCADP_MAXCMD * config.doneq_elem_size;
	}

	if (alloc)
	{
		sh_read_seeprom(config.base_address, (UBYTE *) &control_word, 0, 1);
		if ((control_word & RESET_MASK) != RESET_SPEED)
		{
			fc_cerr(ci, "Notice: reprogramming adapter for multi-host"
				" uses (0x%x)\n", control_word);
			control_word = (control_word & ~RESET_MASK) | RESET_SPEED;
			if (!write_seeprom(config.base_address, ci->config_addr,
					   0, 1, &control_word))
			{
				sh_read_seeprom(config.base_address,
						(UBYTE *) &control_word, 0, 1);
			}
			else
				control_word = 0;
			if ((control_word & RESET_MASK) != RESET_SPEED)
				fc_cerr(ci, "WARNING: adapter reprogramming for "
					"multi-host uses failed (0x%x)\n",
					control_word);
			else
				fc_cerr(ci, "Notice: adapter reprogrammed for "
					"multi-host uses (0x%x)\n",
					control_word);
		}

		/*
		 * Find wwname
		 * Read 4 16-bit values starting at 16-bit address 3.
		 * If wwname is not valid, don't read media type.
		 */
		sh_read_seeprom(config.base_address, (u_char *)&ci->portname, 3, 4);
		if (ci->portname == 0 ||
		    ci->portname == (uint64_t) -1 ||
		    (ci->portname & ~0xF) == 0x2000080069000000)
		{
			fc_cerr(ci, "WARNING - invalid wwname is 0x%x\n",
				ci->portname);
			tmp = 0;
		}
		else
		{
			if (fcadpdebug)
				fc_cerr(ci, "wwname is 0x%x\n", ci->portname);
			tmp = 1;
		}

		/*
		 * Allocate per-controller data structures and initialize host_id
		 * to bad value.
		 */
		fc_alloc_struct(ci, &config);
		ci->host_id = 0xFF;
		ci->config->flag = SH_POWERUP_INITIALIZATION;

		/*
		 * Find media type if wwname is valid.  Otherwise, the SEEPROM
		 * is likely unprogramed and wrong.
		 */
		media_type = 0;
		if (tmp)
		{
			sh_read_seeprom(config.base_address, (u_char *)&media_type, 2, 1);
			if ((media_type & 0x180) == 0x180)
			{
				media_type = 1;
				ci->config->flag |= CURRENT_SENSING;
			}
			else if ((media_type & 0x180) == 0x100)
			{
				media_type = 1;
				/* VOLTAGE_SENSING is 0, so can't set flag */
			}
			else
				fc_cerr(ci, "WARNING:  SEEPROM MIA information "
					    "incorrect - media_type == 0x%x\n",
					    media_type & 0x180);
		}
	}
	else if (ci->host_id != 0xFF)
	{
		ci->config->flag = SH_SET_ALPA;
		ci->config->al_pa[0] = TID_2_ALPA(ci->host_id);
	}
	else
		ci->config->flag = 0;

	ci->config->recv_payload_size = ADPFC_PAYLOAD_SIZE;
	ci->config->lip_timeout = 0x60;

	/*
	 * Initialize controller
	 */
	tmp = sh_initialize(ci->config, ci);
	if (tmp)
	{
		fc_cerr(ci, "Error: initialization failure %d\n", tmp);
		ci->dead = 1;
		if (!alloc)
			fc_clear_controller(ci, 1, SC_TIMEOUT);
		return 1;
	}

	if (alloc)
	{
		if (media_type)
		{
			ci->mia_type = sh_read_mia_type(ci->control_blk);
			if (ci->mia_type != 0)
				fc_cerr(ci, "Information: Optical MIA attached(%d)\n",
					ci->mia_type);
		}

		IOLOCK(ci->himlock, ci->himlockspl);
	}
	
	fc_unsol_tcb_init(ci);
	sh_enable_irq(ci->control_blk);

	if (alloc)
		IOUNLOCK(ci->himlock, ci->himlockspl);

	return 0;
}



/*
 * In rare cases, a new interrupt will be signaled, just as one is
 * finishing.  When this happens, the Bridge will not see a state
 * transition, and we won't see the interrupt if we poll within the
 * window.  To workaround this, we could check twice for a pending
 * interrupt.  However, since there is a call to fcadpintr in the
 * fc_check_timers routine, we will catch the rare case when this
 * happens.
 *
 * With a single call to sh_interrupt_pending, we had unacceptable
 * latency problems where interrupts weren't processed for up to
 * the interrupt check latency (1/2 sec at the time).  This is because
 * the clear interrupt is a PIO write, which can be stuck in the
 * pipeline (II-Xtalk-Bridge) for quite some time, not completing
 * until the PIO read check for intr, causing them to be back to back.
 * 680409
 *
 * Once again: The double poll does not cover the case in which
 * the SEQ_INT bit signals multiple command completions.  It is
 * possible to miss the update in the done queue if we get
 *	completion->intr->completion->(intr already set)->
 *      clear_intr->exit_intr
 * To help with this, I tried a 10 us delay after sh_frontend_isr
 * (where the interrupt in cleared).  This 10 us delay also removed
 * the need for a second sh_interrupt_pending check.  Unfortunately,
 * it dramatically increased interrupt overhead.  The final solution
 * was to have a 50 Hz poll (up from 2 Hz) for missed command
 * completions and interrupts.  The 50 Hz poll, along with another
 * change (see below) also obviates the need for the second call to
 * sh_interrupt_pending.  The 50 hz poll uses less than .1% of a CPU
 * for two adapters.
 * 699938
 *
 * At the same time that 699938, I restructured the loop to release
 * the adapter spinlock and return completed commands on each pass
 * through the loop, rather than waiting until all passes are complete.
 * This has the advantage of holding the lock for a shorter period
 * of time as well as allowing more time for the PIO write that clears
 * the interrupt to retire.  The IOUNLOCK nearly guarantees that the
 * PIO read (of status) following the clear will be separated by a
 * few clock cycles.
 */
/* ARGSUSED */
void
fcadpintr(struct fcadpctlrinfo *ci)
{
	struct scsi_request	*chain;
	struct scsi_request	*req;

	do
	{
		IOLOCK(ci->himlock, ci->himlockspl);
		if (ci->intr || ci->dead)
		{
			IOUNLOCK(ci->himlock, ci->himlockspl);
			break;
		}
		ci->intr = 1;

		/*
		 * Process front end and back end interrupt.
		 */
		(void) sh_frontend_isr(ci->control_blk);
		sh_backend_isr(ci->control_blk);
		if (ci->intr_state == INTR_FATAL)
		{
			fc_cerr(ci, "Fatal controller error, shutting down\n");
			fc_clear_controller(ci, 1, SC_TIMEOUT);
		}
		else if (ci->intr_state == INTR_RESET_RETRY)
		{
			fc_lip_error(ci, ": controller error", 0);
		}
		else if (ci->intr_state == INTR_LOS)
		{
			if (!ci->off_line && !ci->lip_issued)
			{
				ci->LoS = 1;
				if (!ci->lip_wait)
					fc_lip(ci, 0, TCBPRIM_LIPF7, 0);
			}
		}
		ci->intr_state = INTR_CLEAR;

		while (fc_start(ci))
			;

		ci->intr = 0;
		if (ci->req_complete_chain)
		{
			chain = ci->req_complete_chain;
			ci->req_complete_chain = NULL;
		}
		else
			chain = NULL;

		IOUNLOCK(ci->himlock, ci->himlockspl);
		while (chain != NULL)
		{
			req = chain;
			chain = req->sr_ha;
			(*req->sr_notify)(req);
		}
	}
	while (sh_interrupt_pending(ci->control_blk));
}


static __inline void
fc_scsi_complete(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
		 struct fc_tcbhdr *tcbhdr)
{
	struct fcadptargetinfo	*ti;
	struct fcadpluninfo	*li;
	struct scsi_request	*req;
	struct fc_fcprsp_payload *fcprsp;
	uint8_t			*sense;
	struct fc_fcprsp_info	*fcprsp_info;
	uint			 rsplen = 0;
	uint			 senselen = 0;
	uint32_t		 adap_resid;

	req = tcbhdr->th_req;

	if (refblk->done_status == TCB_ERROR_RESPONSE) /* FCP-RSP frame received */
	{
		#pragma mips_frequency_hint NEVER

		fcprsp = (struct fc_fcprsp_payload *) tcbhdr->th_sense;
		swapdata((uint8_t *) fcprsp, 128);

		/*
		 * If we get QUEUEFULL status, put the request at the front of
		 * the wait queue, dequeue the target and return.
		 */
		if (fcprsp->scsi_status == 0x28)
		{
			ti = ci->target[req->sr_target];
			if (!(ti->q_state & FCTI_STATE_AGE))
			    fc_lerr(ci, req->sr_target, req->sr_lun,
				"CMD 0x%x - Queue Full, will reissue "
				"after queue drain\n", req->sr_command[0]);
			fc_puttcbhdr(ci, tcbhdr, req);
			ti->q_state |= FCTI_STATE_AGE;
			if (ti->waithead == NULL)
				ti->waittail = req;
			else
				req->sr_ha = ti->waithead;
			ti->waithead = req;
			fc_dequeue_target_scsi(ci, ti);
			return;
		}

		if (fcprsp->resid_overrun)
		{
			fc_lerr(ci, req->sr_target, req->sr_lun,
				"CMD 0x%x application or driver error -\n\t"
				"transfer count mismatch between CDB and Data"
				" Length field - difference is %d bytes\n",
				req->sr_command[0], fcprsp->resid_count);
			req->sr_status = SC_REQUEST;
		}
		else if (fcprsp->resid_underrun)
			req->sr_resid = fcprsp->resid_count;
		else if (!fcprsp->rsp_length_valid &&
			 !fcprsp->sense_length_valid)
		{
			fc_lerr(ci, req->sr_target, req->sr_lun,
				"Error: FCP_RSP_STATUS 0 scsi_status 0x%x\n",
				fcprsp->scsi_status);
			req->sr_status = SC_HARDERR;
		}
		else
			req->sr_resid = 0;

		adap_resid = sh_get_residual_length(ci->control_blk, tcbhdr->th_sense,
				tcbhdr->th_map, kvtopci(tcbhdr->th_map));
		if ((adap_resid > 0) && (req->sr_resid != adap_resid))
		{
			fc_lerr(ci, req->sr_target, req->sr_lun,
				"cmd 0x%x - residual mismatch %d/%d\n",
				req->sr_command[0], req->sr_resid, adap_resid);
			req->sr_resid = req->sr_buflen;
			req->sr_status = SC_HARDERR;
		}

		if (fcadpdebug > 4)
			printf("TCB %d error response, ur%d, ovr%d, status 0x%x, resid %d\n",
			       tcbhdr->th_number, fcprsp->resid_underrun,
			       fcprsp->resid_overrun, fcprsp->scsi_status,
			       fcprsp->resid_count);

		/*
		 * Process FCP_RSP_INFO data.
		 */
		if (fcprsp->rsp_length_valid &&
		    fcprsp->rsp_length >= 4)
		{
			rsplen = fcprsp->rsp_length;
			fcprsp_info = (struct fc_fcprsp_info *)
				&fcprsp->addl_info[0];
			if (fcprsp_info->rsp_code != 0)
			{
			    fc_lerr(ci, req->sr_target, req->sr_lun,
				"Error: command response code %d\n",
				fcprsp_info->rsp_code);
			    req->sr_status = SC_HARDERR;
			}
		}

		/*
		 * Process SCSI status
		 */
		if (fcprsp->scsi_status != 0 && fcprsp->scsi_status != 2)
			req->sr_scsi_status = fcprsp->scsi_status;

		/*
		 * Process sense data
		 * If not enough sense data or no place to put it, just indicate
		 * check condition in the SCSI status.
		 */
		if (fcprsp->scsi_status == 2)
		{
		    if (fcprsp->sense_length_valid)
			senselen = fcprsp->sense_length;

		    /*
		     * When there are at least 3 bytes of sense data, copy it to
		     * the appropriate place, or print it out, if there is no
		     * place for it.  Also call any necessary callback function.
		     */
		    if (senselen > 2)
		    {
			sense = &fcprsp->addl_info[rsplen];
			if (req->sr_sense)
			{
			    bcopy(sense, req->sr_sense,
				  min(senselen, req->sr_senselen));
			    req->sr_sensegotten =
				  min(senselen, req->sr_senselen);
#if FCADPDEBUG
			    if (!(req->sr_command[0] == 0xA0 && sense[2] == 5))
			    fc_lerr(ci, req->sr_target, req->sr_lun,
				"CMD 0x%x - sense key 0x%x asc 0x%x asq 0x%x\n",
				req->sr_command[0], sense[2], sense[12],
				(__scunsigned_t) sense[13]);
#endif
			}
			else
			{
			    req->sr_scsi_status = 2;
			    fc_lerr(ci, req->sr_target, req->sr_lun,
				"CMD 0x%x - sense key 0x%x asc 0x%x asq 0x%x\n",
				req->sr_command[0], sense[2], sense[12],
				(__scunsigned_t) sense[13]);
			}
			li = SLI_INFO(scsi_lun_info_get(req->sr_lun_vhdl));
			if (li->sense_callback)
				(*li->sense_callback)(req->sr_lun_vhdl, sense);
		    }
		    else
			req->sr_scsi_status = 2;
		}

		/*
		 * Clear FCPRSP data after examining
		 */
		bzero(fcprsp, FCADP_MAXSENSE);
	}
#if 0
	/*
	 * Underrun code does not seem to work correctly.  When a frame
	 * gets interrupted, it is possible to get a short transfer, when
	 * the target actually transferred all of its data.
	 */
	else if (refblk->done_status == TCB_UNDERRUN)
	{
		#pragma mips_frequency_hint NEVER
		uint32_t resid;
		resid = sh_get_residual_length(ci->control_blk, tcbhdr->th_sense,
				tcbhdr->th_map, kvtopci(tcbhdr->th_map));
		fc_lerr(ci, req->sr_target, req->sr_lun,
			"cmd 0x%x, length %d, resid %d\n", req->sr_command[0],
			req->sr_buflen, resid);
		req->sr_resid = resid;
	}
#endif
	else if (refblk->done_status != 0)
	{
		#pragma mips_frequency_hint NEVER
		fc_lerr(ci, req->sr_target, req->sr_lun,
			"Error code %d (%s)\n", refblk->done_status,
			fc_done_status[refblk->done_status]);
		req->sr_status = SC_HARDERR;
	}
	else
		req->sr_resid = 0;

#if FCADP_CMD_DEBUG
	command_list[command_index].tcb_number = 10000 + tcbhdr->th_number;
	command_list[command_index].adap_num = (uint8_t) ci->number;
	command_list[command_index].targ_num = tcbhdr->th_req->sr_target;
	bcopy(tcbhdr->th_req->sr_command, command_list[command_index].scsicmd,
	      tcbhdr->th_req->sr_cmdlen);
	command_list[command_index].sensegotten = req->sr_sensegotten;
	if (++command_index == COMMAND_INDEX_SIZE)
		command_index = 0;
#endif

	if ((req->sr_buflen) &&
	    (req->sr_resid != req->sr_buflen) &&
	    (req->sr_flags & SRF_DIR_IN) &&
	    (req->sr_command[0] != 0x28))
	{
		swapdata(req->sr_buffer, req->sr_buflen - req->sr_resid);
	}

	fc_puttcbhdr(ci, tcbhdr, req);
	if (ci->intr)
	{
		req->sr_ha = ci->req_complete_chain;
		ci->req_complete_chain = req;
	}
	else
	{
		#pragma mips_frequency_hint NEVER
		IOUNLOCK(ci->himlock, ci->himlockspl);
		(*req->sr_notify)(req);
		IOLOCK(ci->himlock, ci->himlockspl);
	}
}

void
fc_lip_complete(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	struct fcadptargetinfo	*ti;
	int			 i;
	int			 tid;
	int			 lisa = 0;

	if (ci->lip_timeout)
	{
		untimeout(ci->lip_timeout);
		ci->lip_timeout = 0;
	}
	TCB_INACTIVE(ci, tcbhdr->th_number);
	ci->lip_issued = 0;

	/*
	 * Check for bad status.  The status is usually the loop address
	 * negotiated.  Certain statuses have other meanings.
	 */
	if (refblk->done_status == 0x5 ||
	    refblk->done_status == 0x6 ||
	    refblk->done_status == 0x7 ||
	    refblk->done_status == 0x9 ||
	    refblk->done_status == 0xA)
	{
		char reason[20];
		sprintf(reason, ": status 0x%x", refblk->done_status);
		fc_lip_error(ci, reason, 0);
	}
	else
	{
		ci->LoS = 0;
		ci->host_id = ALPA_2_TID(refblk->done_status);
		ci->lip_attempt = 0;

		/*
		 * Process lip data
		 */
		swapdata(ci->lipmap, 128);
		if (ci->lipmap[0] > 0)
		{
			ci->lipmap_valid = 1;
			for (i = 1; i <= ci->lipmap[0]; i++)
				ci->lipmap[i] = ALPA_2_TID(ci->lipmap[i]);
		}
		else
		{
			uint64_t	lisa_map[2];
			uint8_t		current_id = 0;
			uint64_t	j;

			bcopy(&ci->lipmap[4], lisa_map, 16);
			lisa_map[0] &= ~((uint64_t) 1 << 63);
			for (i = 1; i >= 0; i--)
			{
				for (j = 1; j != 0; j *= 2)
				{
				    if (lisa_map[i] & j)
				    {
					ci->lipmap[0]++;
					ci->lipmap[ci->lipmap[0]] = current_id;
				    }
				    current_id++;
				}
			}
			ci->lipmap_valid = 1;
			lisa = 1;
		}
		ci->lip_buffer[0] = 0;
		if (fcadpdebug)
		{
			char number[5];

			for (i = 1; i <= ci->lipmap[0]; i++)
			{
				sprintf(number, " %d", ci->lipmap[i]);
				strcat(ci->lip_buffer, number);
			}
			strcat(ci->lip_buffer, "\n");
		}
		if (fcadpdebug || !fcadp_init_time)
		{
			if (!lisa)
				fc_cerr(ci, "Information: LIRP complete @ %d: "
					"%d devices, host id %d\n%s", lbolt,
					ci->lipmap[0], ci->host_id, ci->lip_buffer);
			else
				fc_cerr(ci, "Information: LISA complete @ %d: "
					"%d devices, host id %d\n%s", lbolt,
					ci->lipmap[0], ci->host_id, ci->lip_buffer);
		}

		/*
		 * Reset controller data structures.
		 */
		fc_clear_controller(ci, 0, 0);

		/*
		 * Clear unsolicited TCBs and redeliver.
		 */
		fc_clear_unsol_tcb(ci);
		fc_unsol_tcb_init(ci);

		/*
		 * Signal previous targets still in existence that they need to
		 *   log in.
		 */
		for (i = 1; i <= ci->lipmap[0]; i++)
		{
			tid = ci->lipmap[i];
			if (tid < FCADP_MAXTARG && (ti = ci->target[tid]) != NULL)
			{
				if (ti->check_pending)
				{
					fc_terr(ci, ti->number, "target reappeared @ %d\n", lbolt);
					ti->check_pending = 0;
					untimeout(ti->check_pending_id);
				}
				if (ti->login_failed > FCADP_PLOGI_RETRY)
					fc_clear_target(ci, ti, SC_CMDTIME, SC_CMDTIME);
				else
				{
					if (ti->login_failed > 0)
						fc_terr(ci, tid, "retrying N-port login\n");
					fc_login(ci, ti);
				}
			}
		}

		ci->quiesce &= ~LIP_QUIESCE;
	}
}


void
fc_plogi_complete(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	struct fcadptargetinfo	*ti;
	struct fc_plogi_payload	*plogi;
	int			 error = 0;

	ti = ci->target[tcbhdr->th_target];
	ti->needplogi = 0;

	/*
	 * Check to see if TCB completed correctly.
	 */
	if (refblk->done_status != 0)
	{
		fc_terr(ci, ti->number, "Error: N-port Login (%d) - %s\n",
			refblk->done_status, fc_done_status[refblk->done_status]);
		error = 1;
	}
	else
	{
		plogi = (struct fc_plogi_payload *) tcbhdr->th_map;
		swapdata((uint8_t *) plogi, sizeof(*plogi));
		if (plogi->command_code == (2 << 24))
		{
			ti->plogi = 1;
			if (ti->login_failed)
			{
				fc_terr(ci, ti->number,
					"N-port login retry successful\n");
				ti->login_failed = 0;
			}
			bcopy(plogi, ti->plogi_payload, sizeof(struct fc_plogi_payload));
		}
		else
		{
			if (plogi->command_code == (1 << 24))
			{
			    fc_terr(ci, ti->number,
				"Error: N-port login rejected - reason %d/%d\n",
				((struct fc_lsrjt_payload *) plogi)->reject.reason_code,
				((struct fc_lsrjt_payload *) plogi)->reject.reason_explanation);
			}
			else
			{
			    fc_terr(ci, ti->number, "Error: N-port login - "
				"command code of response frame is 0x%x\n",
				plogi->command_code);
			}
			error = 1;
		}
	}

	fc_puttcbhdr(ci, tcbhdr, NULL);
#if LOGIN_QUIESCE
	if (ci->cmdcount == 0)
		ci->quiesce &= ~LIP_QUIESCE;
#endif
	if (ti->abort == NULL)
		ti->q_state -= FCTI_STATE_QUIESCE;
	if (error)
	{
		ti->login_failed++;
		if (ti->login_failed <= FCADP_PLOGI_RETRY)
		{
			fc_terr(ci, ti->number, "retrying N-port login\n");
			fc_login(ci, ti);
		}
		else
		{
			fc_terr(ci, ti->number, "Error: N-port login failed\n");
			ti->needprli = 0;
			fc_reset_target_state(ci, ti);
			fc_clear_target(ci, ti, SC_HARDERR, SC_HARDERR);
		}
	}
	else
	{
		if (ci->error_id == ti->number)
		{
			fc_clear_target(ci, ti, SC_CMDTIME, SC_ATTN);
			ci->error_id = 255;
		}
		else
			fc_clear_target(ci, ti, SC_ATTN, SC_ATTN);
	}

	/*
	 * Put target back on ctlr wait queue if necessary.
	 */
	fc_queue_target(ci, ti);
}

void
fc_prli_complete(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	struct fcadptargetinfo	*ti;
	struct fc_prli_payload	*prli;
	int			 error = 0;

	ti = ci->target[tcbhdr->th_target];
	ti->needprli = 0;

	/*
	 * Check for proper completion of process login
	 */
	if (refblk->done_status != 0)
	{
		fc_terr(ci, ti->number, "Error: process login (%d) - %s\n",
			refblk->done_status, fc_done_status[refblk->done_status]);
		error = 1;
	}
	else
	{
		prli = (struct fc_prli_payload *) tcbhdr->th_map;
		swapdata((uint8_t *) prli, sizeof(*prli));
		if (prli->command_code != 2)
		{
			if (prli->command_code == 1)
			{
			    fc_terr(ci, ti->number,
				"Error: process login rejected - reason %d/%d\n",
				((struct fc_lsrjt_payload *) prli)->reject.reason_code,
				((struct fc_lsrjt_payload *) prli)->reject.reason_explanation);
			}
			else
			{
			    fc_terr(ci, ti->number, "Error: process login, command "
				"code of response frame is 0x%x\n", prli->command_code);
			}
			error = 1;
		}
		else if (prli->svc_param != FC_PRLI_SVC_PARAM_DESIRED)
		{
			fc_terr(ci, ti->number, "Error: PRLI service parameters 0x%x "
				"incorrect\n", prli->svc_param);
			error = 1;
		}
		else
		{
			ti->prli = 1;
			bcopy(prli, ti->prli_payload, sizeof(struct fc_prli_payload));
		}
	}

	fc_puttcbhdr(ci, tcbhdr, NULL);
	if (error)
	{
		fc_reset_target_state(ci, ti);
		fc_clear_target(ci, ti, SC_HARDERR, SC_HARDERR);
	}
	else if (ti->abort == NULL)
		ti->q_state -= FCTI_STATE_QUIESCE;

	/*
	 * Put target back on ctlr wait queue if necessary.
	 */
	fc_queue_target(ci, ti);
}

void
fc_abts_complete(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	struct fcadptargetinfo	*ti;

	ti = ci->target[tcbhdr->th_target];

	/*
	 * Check to see if TCB completed correctly.
	 * If not, we've got problems.  XXX
	 */
	if (refblk->done_status != 0)
	{
		fc_terr(ci, ti->number, "unable to abort command, status %d\n",
			refblk->done_status);
		fc_clear_target(ci, ti, SC_HARDERR, SC_HARDERR);
	}

	/*
	 * Put target back on ctlr wait queue if necessary.
	 */
	if (ti->abts_all == NULL)
	{
		ASSERT(ti->abort != ti->active);
		ti->abort = ti->active;
	}
	else
		ti->abort = NULL;
	ti->q_state -= FCTI_STATE_QUIESCE;
	fc_queue_target(ci, ti);
}

void
fc_adisc_complete(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	printf("adisc complete %x %x %x\n", ci, refblk, tcbhdr);
}


char *scsi_fcp_rctl_type[8] =
{
	"0",
	"Data",
	"2",
	"3",
	"4",
	"Transfer Ready",
	"Command",
	"Response",
};

char *els_rctl_type[8] =
{
	"0",
	"1",
	"Request",
	"Reply",
	"4",
	"5",
	"6",
	"7",
};

char *bls_rctl_type[8] =
{
	"0",
	"ABTS",
	"2",
	"3",
	"BA_ACC",
	"BA_RJT",
	"6",
	"7",
};

/*
 * An unsolicited TCB has completed.
 * Turn it around and reissue it.
 */
void
fc_unsol_complete(struct fcadpctlrinfo *ci, struct reference_blk *refblk,
	struct fc_tcbhdr *tcbhdr)
{
	struct reference_blk	 ref;
	struct _uframe_head	*framehdr;
	uint32_t		*payload;

	framehdr = (struct _uframe_head *) tcbhdr->th_sense;
	payload = (uint32_t *) tcbhdr->th_map;

	/*
	 * If the unsolicited TCB completed with error, all we can
	 * do is note it.
	 */
	if (refblk->done_status != 0)
	{
		fc_cerr(ci, "Error: %s (%d) on unsolicited TCB %d\n",
		        fc_done_status[refblk->done_status],
		        refblk->done_status, refblk->tcb_number);
	}
	/*
	 * Check for a logout (PLOGO) or a login (PLOGI)
	 */
	else if (framehdr->type == 0x1 &&
		 framehdr->r_ctl == 0x22 &&	/* Extended link svc request */
		 payload[0] == 0x5000000)	/* logout */
	{
		fc_terr(ci, ALPA_2_TID(int24_le(&framehdr->their_aid0)),
			"Notice: Logout requested - initializing loop\n");
		fc_lip(ci, 0, TCBPRIM_LIPF7, 0);
	}
	else if (framehdr->type == 0x1 &&
		 framehdr->r_ctl == 0x22 &&	/* Extended link svc request */
		 payload[0] == 0x3000000)	/* login */
	{
		if (fcadpdebug)
			fc_cerr(ci, "Notice: PLOGI received from id %d, ignoring\n",
				ALPA_2_TID(int24_le(&framehdr->their_aid0)));
	}
	/*
	 * dump the frame for analysis
	 */
	else
	{
		fc_cerr(ci, "Notice: unsolicited TCB(%d) complete: ",
			refblk->tcb_number);
		printf(" frame header follows (for diagnosis):\n");
		printf("\tR_CTL 0x%x  S_ID %d(0x%x)  TYPE 0x%x  F_CTL 0x%x\n",
		       framehdr->r_ctl,
		       ALPA_2_TID(int24_le(&framehdr->their_aid0)),
		       int24_le(&framehdr->their_aid0),
		       framehdr->type,
		       int24_le(&framehdr->f_ctl0));
		printf("\tSEQ_ID %d  DF_CTL 0x%x  SEQ_CNT %d  Their_XID 0x%x\n",
		       framehdr->seq_id,
		       framehdr->df_ctl,
		       int16_le(&framehdr->seq_cnt0),
		       int16_le(&framehdr->their_xid0));
		printf("\tOur_XID 0x%x  PARAMETER 0x%x  DELIMITER 0x%x\n",
		       int16_le(&framehdr->our_xid0),
		       int32_le(&framehdr->param0),
		       framehdr->delimiter);
		if (framehdr->type == 0x8 &&
		    (framehdr->r_ctl & 0xF8) == 0)
		{
			printf("\tUnexpected SCSI FCP %s\n",
			       scsi_fcp_rctl_type[framehdr->r_ctl & 7]);
		}
		else if (framehdr->type == 1 &&
			 (framehdr->r_ctl & 0xF8) == 0x20)
		{
			printf("\tUnexpected Extended Link Service %s"
			       " command code 0x%x\n",
			       els_rctl_type[framehdr->r_ctl & 7],
			       payload[0]);
		}
		else if (framehdr->type == 0 &&
			 (framehdr->r_ctl & 0xF8) == 0x80)
		{
			printf("\tUnexpected Link Service %s\n",
			       bls_rctl_type[framehdr->r_ctl & 7]);
		}
	}

	/*
	 * Reissue completed unsolicited TCB for further use.
	 */
	tcbhdr->th_tcb = fc_get_rrtcb(ci);
	fill_unsol_tcb(ci, tcbhdr);

	ref.control_blk = ci->control_blk;
	ref.tcb = tcbhdr->th_tcb;
	ref.tcb_number = tcbhdr->th_number;

	fc_swapctl((uint8_t *) ref.tcb, sizeof(struct _tcb));
	sh_deliver_tcb(&ref);
}


void
ulm_tcb_complete(struct reference_blk *r, void *ulm_info)
{
	struct reference_blk	*refblk;
	struct fcadpctlrinfo	*ci = ulm_info;
	struct fc_tcbhdr	*tcbhdr;

	refblk = r;
	if (refblk->tcb_number >= ci->config->number_tcbs)
	{
		#pragma mips_frequency_hint NEVER
		cmn_err(CE_PANIC,
			"%v (%d):\n  tcb_complete invalid TCB number %d\n",
			ci->ctlrvhdl, ci->number, refblk->tcb_number);
	}

	/*
	 * Find tcb header and scsi request
	 */
	tcbhdr = FIND_TCB(ci, refblk->tcb_number);
	if (!TCB_IS_ACTIVE(ci, refblk->tcb_number))
	{
		#pragma mips_frequency_hint NEVER
		cmn_err(CE_PANIC,
			"%v (%d):\n  tcb_complete TCB number %d not active\n",
			ci->ctlrvhdl, ci->number, refblk->tcb_number);
	}
	if (tcbhdr->th_number != refblk->tcb_number)
	{
		#pragma mips_frequency_hint NEVER
		cmn_err(CE_PANIC, "%v (%d):\n  "
		"tcb_complete header tcb number %d does not agree with refblk %d\n",
		ci->ctlrvhdl, ci->number, tcbhdr->th_number, refblk->tcb_number);
	}

#if FCADPDEBUG
	DBG(11, "fcadp%dd%d: complete tcb %d type %d\n", ci->number,
	    tcbhdr->th_target, tcbhdr->th_number, tcbhdr->th_type);
#endif

	/*
	 * Process completed TCB.
	 */
	if (tcbhdr->th_type == TCBTYPE_SCSI)
		fc_scsi_complete(ci, refblk, tcbhdr);
	else
	{
		#pragma mips_frequency_hint NEVER
		switch (tcbhdr->th_type)
		{
		case TCBTYPE_LIP:
			fc_lip_complete(ci, refblk, tcbhdr);
			break;
		case TCBTYPE_PLOGI:
			fc_plogi_complete(ci, refblk, tcbhdr);
			break;
		case TCBTYPE_PRLI:
			fc_prli_complete(ci, refblk, tcbhdr);
			break;
		case TCBTYPE_ADISC:
			fc_adisc_complete(ci, refblk, tcbhdr);
			break;
		case TCBTYPE_UNSOL:
			fc_unsol_complete(ci, refblk, tcbhdr);
			break;
		case TCBTYPE_ABTS:
			fc_abts_complete(ci, refblk, tcbhdr);
			break;
		default:
			cmn_err(CE_PANIC, "%v (%d):\n  unexpected tcb type %d\n",
				ci->ctlrvhdl, ci->number, tcbhdr->th_type);
			break;
		}
	}
}

void
ulm_primitive_complete(struct reference_blk *r, void *ulm_info)
{
	struct fcadpctlrinfo	*ci = ulm_info;
	struct fc_tcbhdr	*tcbhdr;

	DBGC(3, ci, "primitive complete\n", ci->number);
	if (!ci->primbusy)
	{
		fc_cerr(ci, "Notice: primitive completed, but not active\n");
		/* debug("ring"); */
	}

	tcbhdr = ci->prim_tcb;
	switch (tcbhdr->th_type)
	{
	case TCBPRIM_LIPF7:
	case TCBPRIM_LIPRST:
		break;

	case TCBPRIM_LPB:
		if (r->done_status != 0x40)
			ci->userprim_status = EIO;
		break;

	case TCBPRIM_LPE:
	case TCBPRIM_LPEALL:
		if (r->done_status != 0x20)
			ci->userprim_status = EIO;
		break;
	
	default:
		ASSERT(0);
	}

	if (ci->userprim)
		vsema(&ci->userprim_sema);
	ci->primbusy = 0;
}

void
ulm_event(struct reference_blk *r, INT event, void *ulm_info)
{
	struct fcadpctlrinfo	*ci = ulm_info;
	extern char		*sh_event_strings[];

	DBGC(3, ci, "ulm_event event %d, refblk addr 0x%x\n", event, r);
	switch (event)
	{
	case EVENT_FCAL_LIP:
		switch (r->done_status)
		{
		case 10:	/* LIP(F7,AL_PS) */
			if (fcadpdebug || !fcadp_init_time)
			if (ci->lipmap_valid)
				fc_cerr(ci, "Notice: LIPv received from %d (0x%x) @ "
					"%d\n", ALPA_2_TID(r->func), r->func, lbolt);
			break;
					
		case 8:		/* LIP(F7,F7) */
			if (fcadpdebug || !fcadp_init_time)
			if (ci->lipmap_valid)
				fc_cerr(ci, "Notice: LIPnv received @ %d\n", lbolt);
			break;
		
		case 12:	/* LIP(AL_PD,AL_PS) (LIPRST) */
			if (fcadpdebug || ci->lipmap_valid)
				fc_cerr(ci, "Notice: LIPRST detected @ %d\n", lbolt);
			break;

		case 255:	/* LIP(unknown, unknown) */
			if (fcadpdebug || !fcadp_init_time)
			if (ci->lipmap_valid)
				fc_cerr(ci, "Notice: LIP received @ %d\n", lbolt);
			break;

		case 11:	/* LIP(F8,AL_PS) */
			if (fcadpdebug || !fcadp_init_time)
			fc_cerr(ci, "Notice: LIPlfv from target %d (0x%x) @ %d\n",
				ALPA_2_TID(r->func), r->func, lbolt);
			break;

		case 9:		/* LIP(F8,F7) */
			if (fcadpdebug || !fcadp_init_time)
			fc_cerr(ci, "Notice: LIPlf received @ %d\n", lbolt);
			break;

		default:
			fc_cerr(ci, "Notice: unexpected LIP primitive %d received @ %d"
				"\n", r->done_status, lbolt);
			break;
		}
		ci->quiesce |= LIP_QUIESCE;
		if (!ci->off_line) /* if off line, we ignore LIPs received */
		{
			if (!ci->lip_issued)	/* If lip TCB not active, wait */
				ci->lip_wait = 1;
			if (!ci->lip_timeout)
				ci->lip_timeout =
				    timeout(fc_lip_timeout, ci, LIPTIMEOUT);
		}
		break;

	case EVENT_BYPASS:
		fc_cerr(ci, "Notice: LPB Primitive detected\n");
		break;
	
	case EVENT_ON_LOOP:
		fc_cerr(ci, "Notice: LPE Primitive detected\n");
		break;

	case EVENT_READY_FOR_LIP_TCB:
		ci->LoS = 0;
		perform_lip(ci);
		break;

	/* rev A1 chips have corrupted data at this point */
	case EVENT_CMC_RAM_PAR_ERR:
	case EVENT_MEM_PORT_PAR_ERR:
	case EVENT_RPB_TO_HST_PAR_ERR:
		fc_cerr(ci, "Error: %d - %s\n", event, sh_event_strings[event]);
		cmn_err(CE_PANIC, "Fatal error in fcadp hardware\n");
	
	/* potentially recoverable errors */
	case EVENT_SEQ_OP_CODE_ERR:
	case EVENT_RPB_PAR_ERR:
	case EVENT_SPB_TO_SFC_PAR_ERR:
	case EVENT_CMC_TO_HST_PAR_ERR:
	case EVENT_PCI_STATUS1_DPR:
	case EVENT_PCI_ERROR0_HR_DMA_DPR:
	case EVENT_PCI_ERROR1_CP_DMA_DPR:
	case EVENT_PCI_ERROR1_CIP_DMA_DPR:
	case EVENT_PCI_STATUS1_STA:
	case EVENT_PCI_STATUS1_RTA:
	case EVENT_PCI_ERROR0_HS_DMA_RTA:
	case EVENT_PCI_ERROR0_HR_DMA_RTA:
	case EVENT_PCI_ERROR1_CP_DMA_RTA:
	case EVENT_PCI_ERROR1_CIP_DMA_RTA:
	case EVENT_PCI_STATUS1_DPE:
	case EVENT_PCI_ERROR0_HS_DMA_DPE:
	case EVENT_PCI_ERROR1_CP_DMA_DPE:
	case EVENT_PCI_ERROR1_CIP_DMA_DPE:
	case EVENT_PCI_ERROR2_T_DPE:
	case EVENT_LPSM_ERROR:
		/*
		 * Don't print LPSM error message during LIP on normal kernels.
		 * It can happen during normal operation.
		 */
		if (fcadpdebug ||
		    !(ci->lip_wait || ci->lip_issued) ||
		    (event != EVENT_LPSM_ERROR))
		{
			fc_cerr(ci, "Error: %d - %s\n", event,
				sh_event_strings[event]);
		}
		if (ci->intr_state < INTR_RESET_RETRY)
			ci->intr_state = INTR_RESET_RETRY;
		break;
	
	case EVENT_SEQ_PAR_ERR:
		fc_cerr(ci, "Error: %d - %s - address 0x%x\n", event,
			sh_event_strings[event], *((uint16_t *)ci->ibar0_addr));
		if (ci->intr_state < INTR_RESET_RETRY)
			ci->intr_state = INTR_RESET_RETRY;
		break;

	/* unrecoverable errors */
	case EVENT_PCI_STATUS1_RMA:
	case EVENT_PCI_ERROR0_HS_DMA_RMA:
	case EVENT_PCI_ERROR0_HR_DMA_RMA:
	case EVENT_PCI_ERROR1_CP_DMA_RMA:
	case EVENT_PCI_ERROR1_CIP_DMA_RMA:
	case EVENT_PCI_STATUS1_SERR:
		fc_cerr(ci, "Error: %d - %s\n", event, sh_event_strings[event]);
		if (ci->intr_state < INTR_FATAL)
			ci->intr_state = INTR_FATAL;
		ci->dead = 1;
		ci->lipmap_valid = 0;
#if 0  /* trigger logic analyzer trace */
		{
		ci->ibar0_addr[0x21] = 0x40;
		while (!(ci->ibar0_addr[0x21] & 0x40))
			us_delay(1);
		ci->ibar0_addr[0x30] = 1;
		debug("ring");
		}
#endif
		break;

	/* special errors */
	case EVENT_LINK_ERRST_LOSIG_LIP:
	case EVENT_LINK_ERRST_LOSYNTOT_LIP:
	case EVENT_LINK_ERRST_LOSIG:
	case EVENT_LINK_ERRST_LOSYNTOT:
	case EVENT_SERDES_LKNUSE:
	case EVENT_SERDES_FAULT:
		if (fcadpdebug || !fcadp_init_time)
			fc_cerr(ci, "Error: %d - %s\n", event, sh_event_strings[event]);
		if (ci->intr_state < INTR_LOS)
			ci->intr_state = INTR_LOS;
		break;

	default:
		fc_cerr(ci, "Notice: unexpected event %d, refblk addr 0x%x\n", event, r);
		break;
	}
}

/*
 * ulm_kvtop: Return bus address given virtual memory address.
 *
 * Parms:	Input: category and virutal address
 *		Output: Physical address
 *
 * Remarks:	
 */
/* ARGSUSED */
BUS_ADDRESS
ulm_kvtop(VIRTUAL_ADDRESS vaddr, void *ulm_info)
{
#if defined(SN0) || defined(IP30)
	struct fcadpctlrinfo *ci = ulm_info;
#endif
	return kvtopci(vaddr);
}


/*
 * create a vertex for the fcadp controller and add the info
 * associated with it
 */
vertex_hdl_t
fcadp_ctlr_add(struct fcadpctlrinfo *ci)
{
	scsi_ctlr_info_t	*ctlr_info;
	vertex_hdl_t		 ctlr_vhdl;
	graph_error_t		 rv;

	rv = hwgraph_path_add(ci->pcivhdl, EDGE_LBL_SCSI_CTLR "/0", &ctlr_vhdl);
	ci->ctlrvhdl = ctlr_vhdl;
	sprintf(ci->hwgname, "%v", ctlr_vhdl);
	ASSERT(rv == GRAPH_SUCCESS); rv=rv;
	ASSERT(ctlr_vhdl != GRAPH_VERTEX_NONE);
	
	ctlr_info = (scsi_ctlr_info_t *) hwgraph_fastinfo_get(ctlr_vhdl);
	if (!ctlr_info)
	{
		ctlr_info = scsi_ctlr_info_init();
		SCI_CTLR_VHDL(ctlr_info) = ctlr_vhdl;
		SCI_ADAP(ctlr_info) = ci->number;
		SCI_INFO(ctlr_info) = ci;

		SCI_ALLOC(ctlr_info) = fcadpalloc;
		SCI_COMMAND(ctlr_info) = fcadpcommand;
		SCI_FREE(ctlr_info) = fcadpfree;
		SCI_DUMP(ctlr_info) = NULL;
		SCI_INQ(ctlr_info) = fcadpinfo;
		SCI_IOCTL(ctlr_info) = fcadpioctl;

		scsi_ctlr_info_put(ctlr_vhdl, ctlr_info);
		scsi_bus_create(ctlr_vhdl);
	}
	return ctlr_vhdl;
}


#if defined(SN0) || defined (IP30)
void
fcadp_vmc(vertex_hdl_t vhdl)
{
	pcibr_hints_fix_rrbs(vhdl);
	if (pcibr_alloc_all_rrbs(vhdl, 0, 4,0, 0,0, 0,0, 0,0) ||
	    pcibr_alloc_all_rrbs(vhdl, 1, 4,0, 0,0, 0,0, 0,0))
	{
		cmn_err(CE_CONT, "fcadp (%v) unable to allocate RRB's\n", vhdl);
	}
}

#include <sys/nic.h>
void
fcadpinit(void)
{
#ifdef DEBUG
	cmn_err(CE_CONT, "FCADP %d-bit mode\n", 32 + PCI_DATA_64 * 32);
#endif
	pciio_driver_register(FCADP_VENDID, FCADP_DEVID, "fcadp", 0);
	nic_vmc_add("030-0927-", fcadp_vmc);
	mutex_init(&fc_attach_sema, MUTEX_DEFAULT, "fcattex");
}

#define FC_NIC_PREFIX "FIBRE_CHANNEL"
int
fc_isxio(vertex_hdl_t vhdl)
{
	arbitrary_info_t ainfo;
	vertex_hdl_t vhdl0;
	
	vhdl0 = hwgraph_connectpt_get(hwgraph_connectpt_get(vhdl));
	if (hwgraph_info_get_LBL(vhdl0, INFO_LBL_NIC, &ainfo) == GRAPH_SUCCESS)
	{
		if (strstr((char *)ainfo, FC_NIC_PREFIX) != NULL)
			return 1;
	}
	return 0;
}

int
fcadpattach(vertex_hdl_t vhdl)
{
	struct fcadp_pciconfig	*config;
	struct fcadpctlrinfo	*ci;
	pciio_intr_t		 pciintr;
	device_desc_t		 fcadp_desc;
	vertex_hdl_t		 ctlr_vhdl;
	ushort			 adap;
	int a, b;

	/*
	 * Allocate and initialize ctlr info structure
	 */
	mutex_lock(&fc_attach_sema, PRIBIO);
	adap = fcadp_maxadap++;
	mutex_unlock(&fc_attach_sema);

	ci = kmem_zalloc(sizeof(*ci), VM_DIRECT);
	fcadpctlr[adap] = ci;
	ci->number = adap;
	ci->error_id = 255;
	ci->pcivhdl = vhdl;
	atomicAddInt(&fcadp_init_time, 1);

	fcadp_desc = device_desc_dup(vhdl);
	device_desc_intr_name_set(fcadp_desc, "Adaptec FC");
	device_desc_default_set(vhdl, fcadp_desc);

	config = (struct fcadp_pciconfig *)
		pciio_piotrans_addr(vhdl, 0, PCIIO_SPACE_CFG, 0, 256, 0);
	if (config == NULL)
	{
		cmn_err(CE_CONT, "fcadp (%v): config piotrans failed\n", vhdl);
		goto attach_out;
	}

	/*
	 * For the xtalk board, where the Bridge is owned by the two fc
	 * chips, set high priority to use all resources.
	 */
	if (fc_isxio(vhdl))
		pciio_priority_set(vhdl, PCI_PRIO_HIGH);
	
	a = 4; b = 0;
	pcibr_rrb_alloc(vhdl, &a, &b);

	/*
	 * Piotrans should never fail.  If it does, then we need to understand
	 * why it does.  Code to do piomap_alloc is left as an example of how
	 * to do it.
	 */
	ci->config_addr = (u_char *) config;
	ci->ibar0_addr = (u_char *)
		pciio_pio_addr(vhdl, 0, PCIIO_SPACE_WIN(0), 0, 256, 0, 0);
	ci->ibar1_addr = (u_char *)
		pciio_pio_addr(vhdl, 0, PCIIO_SPACE_WIN(3), 0, 256, 0, 0);
	if (ci->ibar0_addr == NULL || ci->ibar1_addr == NULL)
	{
		cmn_err(CE_CONT, "fcadp (%v): memspace piotrans failed\n", vhdl);
		goto attach_out;
	}

	pcibr_device_flags_set(vhdl, PCIBR_NOWRITE_GATHER);

	pciintr = pciio_intr_alloc(vhdl, fcadp_desc, 1, 0);
	pciio_intr_connect(pciintr, (intr_func_t) fcadpintr, ci, NULL);

        config->PCI_command = 0x55; /* PERRESPEN, MWRICEN, ISPACEEN, MASTEREN */
	config->PCI_cache_line = 32; /* 128 byte cachelines */
	config->PCI_devconfig |= 0x4; /* Dual Addr Cycle enable */
	config->PCI_latency_timer = 0xF0; /* LATTIME 240 clocks */
	config->PCI_base1 = 0; /* workaround adp chip bug */
	config->PCI_base2 = 0; /* workaround adp chip bug */

	/*
	 * Add controller to hwgraph and inventory
	 */
	ctlr_vhdl = fcadp_ctlr_add(ci);
	if (showconfig)
		fc_cerr(ci, "Adaptec AIC 1160 rev. %d found\n", config->PCI_revID);
	device_inventory_add(ctlr_vhdl, INV_DISK, INV_SCSICONTROL,
			     -1, config->PCI_revID, INV_FCADP);

	if (fcadp_ctlrinit(ci, 1))
		goto attach_out;

	/*
	 * Controller is initialized.  See what's connected.
	 * Do a loop initialization.  After it's completed, set up a timeout
	 * to check for progress of commands, then probe the loop for devices.
	 */
	ci->timeout_id = timeout(fc_timeout_check, ci, INTRCHECK_INTERVAL);
	fc_lip(ci, 1, TCBPRIM_LIPF7, 0);
	fc_probe(ci, ctlr_vhdl);

attach_out:
	atomicAddInt(&fcadp_init_time, -1);
	return 0;
}

uint64_t
kvtopciaddr(struct fcadpctlrinfo *ci, void *addr)
{
	uint64_t	kvaddr;

	kvaddr = kvtophys(addr);
	return pcibr_dmatrans_addr(ci->pcivhdl, 0, kvaddr, 0,
				   FCADP_PCIDMA_FLAGS);
}

uint64_t
kvtopciaddr_data(struct fcadpctlrinfo *ci, void *addr)
{
	uint64_t	kvaddr;

	kvaddr = kvtophys(addr);
	return pcibr_dmatrans_addr(ci->pcivhdl, 0, kvaddr, 0,
				   FCADP_PCIDMA_DATA_FLAGS);
}
#endif


#include <io/adpfc/hwequ.h>
/*
 *
 * sh_write_seeprom
 *
 * Write to seeprom
 *
 * Return Value: 0 - write to seeprom successfully
 *		 1 - illegal address or write error
 *
 * Parameters:	base address of memory space
 *		base address of config space
 *		seeprom address
 *		number of words to be written
 *		buffer for image to be written to seeprom
 */
int
write_seeprom(REGISTER base, uint8_t * config,
	      int seeprom_address, int no_of_word, ushort *buffer)
{
	uint8_t		 cmc_hctl, old_mode;
	uint32_t	*addr;
	int		 retval = 1;
	int		 i;

	/* valid address? */
	if (seeprom_address > 0x3f)
		return(1);

	/* keep cmc_hctl value for restore */
	cmc_hctl = INBYTE(AICREG(CMC_HCTL));

	/* pause sequencer if it is not paused yet */
	if (!(cmc_hctl & HPAUSEACK))
	{
		while ((INBYTE(AICREG(CMC_HCTL)) & HPAUSEACK) != HPAUSEACK)
			OUTBYTE(AICREG(CMC_HCTL), cmc_hctl | HPAUSE);
	}
	
	old_mode = INBYTE(AICREG(MODE_PTR));
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE0);

	/* Step 1.  Create target abort, look for STA status. */
	addr = (uint32_t *) AICREG(0x1C);
	(void) badaddr(addr, 4);
	if (!(config[STATUS1 ^ 3] & 0x08))
	{
		printf("Signal Target Abort not set in PCI status register\n");
		goto out;
	}
	
	for (i = 0;; i++)
	{
		/* Step 2.  SEEPROM Read word cycle */
		while (INBYTE(AICREG(SEECTL)) & BUSY);
		OUTBYTE(AICREG(SEEADR), 0x20);
		OUTBYTE(AICREG(SEECTL), 0x61);

		/* Step 3.  Clear SEE_WPEN -- must be 2us between steps 2 & 3 */
		us_delay(3);
		config[DMA_ERROR2 ^ 3] = 0;
		if (config[DMA_ERROR2 ^ 3] & 0x80)
		{
			if (i < 8)
				printf("SEE_WPEN not clear -- retrying\n");
			else
			{
				printf("SEE_WPEN not clear -- giving up\n");
				goto out;
			}
		}
		else
			break;
	}

	/* enable write to seeprom */
	while (INBYTE(AICREG(SEECTL)) & BUSY);
	OUTBYTE(AICREG(SEEADR),OP4|OP3);
	OUTBYTE(AICREG(SEECTL),OP2|START);

	while(INBYTE(AICREG(SEECTL)) & BUSY);

	/* write to the seeprom */
	for (i = 0; i < no_of_word; i++) {

		/* write one word to seeprom */
		OUTBYTE(AICREG(SEEADR), seeprom_address + i);
		OUTBYTE(AICREG(SEEDAT0), (UBYTE) buffer[i]);
		OUTBYTE(AICREG(SEEDAT1), (UBYTE) (buffer[i] >> 8));
		OUTBYTE(AICREG(SEECTL), OP2|OP0|START);

		while(INBYTE(AICREG(SEECTL)) & BUSY);
		us_delay(50);
	}
	retval = 0;

	/* Step 6.  disable write to seeprom */
	OUTBYTE(AICREG(SEEADR), 0);
	OUTBYTE(AICREG(SEECTL), OP2|START);

	/* Step 7.  Set SEE_WPEN and clear STA. */
	for (i = 0;; i++)
	{
		/* Step 7a.  SEEPROM Read word cycle */
		while (INBYTE(AICREG(SEECTL)) & BUSY);
		OUTBYTE(AICREG(SEEADR), 0x20);
		OUTBYTE(AICREG(SEECTL), 0x61);

		/* Step 7b.  Set SEE_WPEN -- must be 2us between steps 7a & 7b */
		us_delay(3);
		config[DMA_ERROR2 ^ 3] = 0x80;
		if (!(config[DMA_ERROR2 ^ 3] & 0x80))
		{
			if (i < 8)
				printf("SEE_WPEN not set -- retrying\n");
			else
				cmn_err(CE_PANIC, "SEE_WPEN not set -- giving up\n");
		}
		else
			break;
	}
	while (INBYTE(AICREG(SEECTL)) & BUSY);
out:
	/* Step 7c. */
	config[STATUS1 ^ 3] = STATUS1_STA;
	OUTBYTE(AICREG(MODE_PTR), old_mode);

	/* restore CMC_HCTL register only if PAUSE bit is not true */
	if (!(cmc_hctl & HPAUSEACK)) 
		OUTBYTE(AICREG(CMC_HCTL),cmc_hctl);

	return retval;
}
