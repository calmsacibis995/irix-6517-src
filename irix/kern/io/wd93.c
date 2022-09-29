#ident "io/wd93.c: $Revision: 3.222 $"
#if DEBUG 
static int printintr;
#endif

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986,1988, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/splock.h>
#include <sys/kopt.h>
#include <sys/cmn_err.h>
#include <sys/dmamap.h>
#include "sys/scsi.h"
#include "sys/wd93.h"
#include <sys/invent.h>
#include <sys/pda.h>
#include <sys/kmem.h>
struct buf;	/* for -xansi compiling and prototypes in systm.h */
#include <sys/systm.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <sys/errno.h>
#include <sys/iograph.h>
#include <string.h>
#include <ksys/hwg.h>

/* To make the INTR_KTHREADS changes touch fewer lines, just redefine
 * the spinlock routines to be mutex routines instead of changing every call.
 */
#undef mutex_spinlock
#undef mutex_spinunlock
#undef nested_spinlock
#undef nested_spinunlock
#undef spinlock_init
#undef init_spinlock

#define mutex_spinlock(x) (mutex_lock(x, PSCSI), 0)
#define mutex_spinunlock(x,s) mutex_unlock(x)
#define nested_spinlock(x) mutex_lock(x, PSCSI)
#define nested_spinunlock(x) mutex_unlock(x)
#define spinlock_init(x,name) mutex_init(x, MUTEX_DEFAULT, name);
#define init_spinlock(x,name,seq) init_mutex(x, MUTEX_DEFAULT, name, seq);


/* Make up a type for a callback function.  Cscope can't deal with the
 * parenthesis of typing a pointer to a function as a parameter in
 * a function definition.
 * The type is a pointer to a callback function which returns a void.
 */
typedef void (*p_cb_fun_t)();

/*
 * SCSI command interface.  These are the externally callable routines
 * for use by the various wd93 drivers.  With the new interface, they
 * should never be called directly, but rather through the scsi_*[]
 * array members.  See sys/scsi.h for details on the entry point routines.
 *
 */

#define INTR_ERR 0x1000

#define ALLOC_NOEXCL_HACK 0x80000


extern u_char wd93_syncenable[];	/* per adapter bitmap of target ID's
	for which sync can be negotiated (from master.d/wd93) */
extern u_char wd93_syncperiod[];	/* adapter indexed sync period */
extern int wd93_enable_disconnect[];	/* from master.d/wd93 */
extern int wd93_printsense; /* may be set in master.d/wd93 for customer debug */
extern int wd93burstdma;	/* set nonzero in IP*.c if CPU and/or IO
	board supports the burst mode DMA of the 93A */

extern wd93dev_t *wd93dev;
extern int wd93cnt;	/* set to 1 in master.d/wd93, and changed in IP*.c for
	machines that * support more than one */
extern scuzzy_t scuzzy[];
extern uint wd93mintimeout;

/* mostly for post-mortem analysis.  Was DEBUG only, but useful on
	non-debug crash files */
static uint wd93aborts, wd93resets, wd93timeouts;

/* these were in master.d/wd93; I no longer remember why;
 * they are initialized in wd93_init() in ml/IP*.c and ml/clover2.c
*/
int wd93cnt;
int wd93burstdma;
wd93dev_t *wd93dev;

int wd93_ha_structs = 128;	/* number of wd93_ha_t's per adapter;
	* also # of getchanmaps per adapter.  No reason to make really
	* large, even if lots of cmd queuing, since much above this would
	* mean bus was completely saturated anyway.
	*/

u_char wd93_ha_id[SC_MAXADAP] = {0,0};	/* allows users in stand
	stuff to check if a particulary id is the host adapter, rather than
	embedding the id in every user of the driver. For now, host adapter
	id is always 0. */

/*	these are used with the new sash,etc. , that may already have one
	of the targets in sync mode.  If we don't deal with it
	correctly, then we will get a bus timeout when we first
	attempt i/o.  After that, they are unused.
*/
static mutex_t wd93allocsem[SC_MAXADAP][SC_MAXTARG][SC_MAXLU];

/* need a struct per active channel for doing request sense */
scsi_request_t *wd93rqsubchan[SC_MAXADAP][SC_MAXTARG][SC_MAXLU];
static struct scsi_target_info wd93_info[SC_MAXADAP][SC_MAXTARG][SC_MAXLU];


/*	no_nextcmd serves as a flag to identify when the scsirelease
	routine should NOT start any waiting commands.  It is set when
	we have been put into dump mode by a device driver
	(Dump mode is for panic() dumping), or we are shutting down.
	In standalone, we don't have intr's, so it is always set, but
	uses a different value than dumping, so callback routines
	still get called. */
static int no_nextcmd = 0;
extern int idbg_wd93(int);

int wd93_int_present(wd93dev_t *);

/* local routines */
static u_char transval(u_char);
static int do_trinfo(wd93dev_t *, u_char *, int, uint usecs);
static int wd93busycnt(wd93dev_t *);
static int do_select(wd93dev_t *);
static int wd93acquire(wd93dev_t *, scsi_request_t *, int);
static int unex_info(wd93dev_t *, u_char, u_char);
static int wd93_dma(wd93dev_t *, scsi_request_t *sp, int);
static uint wait_scintr(wd93dev_t *, unsigned);
static int do_inquiry(wd93dev_t *);
static int sync_setup(wd93dev_t *, scsi_request_t *, int, int);
static int _sync_setup(wd93dev_t *, int, int, int);
static void wd93_timeout(scsi_request_t *);
static void wd93notactive(wd93dev_t *dp, scsi_request_t *sp,uint rel);
static void wd93reset( wd93dev_t *, int);
static void wd93release(wd93dev_t *);
static void handle_intr(wd93dev_t *);
static void handle_reset(wd93dev_t *);
static void wd93subrel(wd93dev_t *, scsi_request_t *, int);
static void save_datap(wd93dev_t *, scsi_request_t *, int);
static void startwd93(wd93dev_t *, scsi_request_t *, int);
static void restartcmd(wd93dev_t *, scsi_request_t *);
static void wd93continue(wd93dev_t *, scsi_request_t *, u_char);
static void setdest(wd93dev_t *, scsi_request_t *);
	/* we don't support stdargs.h in the kernel, so can't use ...  */
static void wd93_cmdabort(wd93dev_t *, scsi_request_t *, char *, long, long);
static wd93abort(scsi_request_t *);
static void wd93cmd_lci(wd93dev_t *, unchar);
static scsi_request_t *wd93findchan(wd93dev_t *, u_char, u_char);
static wd93oktostart(wd93dev_t *,scsi_request_t *);
static scsi_request_t *wd93_reqsense(wd93dev_t *, scsi_request_t *);
static void wd93command(scsi_request_t *);
int wd93alloc(vertex_hdl_t, int, p_cb_fun_t /* not used yet */);
static void wd93free(vertex_hdl_t, p_cb_fun_t /* not used yet */);
static int wd93dump(vertex_hdl_t);
static scsi_target_info_t *wd93info(vertex_hdl_t);
static int wd93_ioctl(vertex_hdl_t, uint, scsi_ha_op_t *);
static void wd93hinv(uint);
static u_char *wd93_inq(vertex_hdl_t);
static wd93dev_t *wd93_ctlr_add(int);
static vertex_hdl_t wd93_ctlr_vhdl_get(int);
extern void print_vertex(vertex_hdl_t, char *);
#if DEBUG || wd93_verbose
static void _wd93print_active_queue(wd93dev_t *dp);
#endif


/* move to wd93.h someday? */
extern dmamap_t *wd93_getchanmap(dmamap_t *);
extern int wd93dma_flush(dmamap_t *, int, int);
extern void wd93_go(dmamap_t *, u_char *, int);

#ifdef EVEREST
/* Everest-only prototypes */
extern int scsidma_flush(dmamap_t *, int);
#endif

#define putreg93(r,v)	{ *dp->d_addr=(r); *dp->d_data=(v); }
#define getreg93(r)	( *dp->d_addr=(r), *dp->d_data )
#define	getauxstat()	(*dp->d_addr)
#define putcmd93(v)	putreg93(WD93CMD, (v))
#define	putdata(v)	putreg93(WD93DATA, (v))
#define putcnt(h, m, l) { putreg93(WD93CNTHI, (h))  *dp->d_data=(m);  \
	 *dp->d_data=(l); } /* use the auto address incr feature */
#define getcnt(h,m,l) { h = getreg93(WD93CNTHI); m= *dp->d_data;  \
	l= *dp->d_data; } /* use the auto address incr feature */
#define	getstat()	getreg93(WD93STAT)
#define	getphase()	getreg93(WD93PHASE)
#define	getdata()	getreg93(WD93DATA)
/* note that wd93busy() shouldn't be used if there is a possiblity
	of wd93 dma in progress (at least if you care about the data),
	because it causes problems on the IP4 */
#define wd93busy()  (getauxstat() & AUX_CIP)

#define ifintclear(x)  if(wd93_int_present(dp)) { \
		state=getstat(); x;}

/*
 * create a vertex for the wd93 controller and add the info
 * associated with it
 */
#define NUMOFCHARPERINT	10
static wd93dev_t *
wd93_ctlr_add(int adap)
{
	vertex_hdl_t		ctlr_vhdl;
	char                    loc_str[LOC_STR_MAX_LEN];
	vertex_hdl_t		hpc_vhdl;
	wd93dev_t		*ci;
	scsi_ctlr_info_t        *ctlr_info;

	/* 
	 * Check for /hw/node/.../hpc in the graph ...
	 * if not found in the graph ... then place the
	 * graph in scsi_ctlr.
	 */
	sprintf(loc_str, "%s/%s/%s/%s",
		EDGE_LBL_NODE,EDGE_LBL_IO,EDGE_LBL_GIO,EDGE_LBL_HPC);
	(void)hwgraph_traverse(GRAPH_VERTEX_NONE,loc_str,&hpc_vhdl);

	/* 
	 * If hpc_vhdl is not found above it will return GRAPH_VERTEX_NONE,
	 * if GRAPH_VERTEX_NONE is passed to hwgraph_path_add it will start
	 * the graph from /hw by default.
	 */	
	sprintf(loc_str, "%s/%d",EDGE_LBL_SCSI_CTLR,SCSI_EXT_CTLR(adap));
	(void)hwgraph_path_add(hpc_vhdl, loc_str, &ctlr_vhdl);

	/* If we found hpc_vhdl we need links to /hw/scsi_ctlr */
	if (hpc_vhdl != GRAPH_VERTEX_NONE) {
		char src_name[10], edge_name[5];
	
		sprintf(loc_str, "%s/%s/%s/%s/%s/%d",EDGE_LBL_NODE,EDGE_LBL_IO,
			EDGE_LBL_GIO,EDGE_LBL_HPC,EDGE_LBL_SCSI_CTLR,SCSI_EXT_CTLR(adap));
		sprintf(src_name,"%s",EDGE_LBL_SCSI_CTLR);
		sprintf(edge_name,"%d",SCSI_EXT_CTLR(adap));

		hwgraph_link_add(loc_str, src_name, edge_name);
	}
	
	ctlr_info = (scsi_ctlr_info_t *)hwgraph_fastinfo_get(ctlr_vhdl);

	if (!ctlr_info) {
		ctlr_info                   = scsi_ctlr_info_init();
		ci                          = (wd93dev_t *)kern_calloc(1,sizeof(*ci));
		SCI_ADAP(ctlr_info)         = adap;
		SCI_CTLR_VHDL(ctlr_info)    = ctlr_vhdl;

		SCI_ALLOC(ctlr_info)        = wd93alloc;
		SCI_COMMAND(ctlr_info)      = wd93command;
		SCI_FREE(ctlr_info)         = wd93free;
		SCI_DUMP(ctlr_info)         = wd93dump;
		SCI_INQ(ctlr_info)          = wd93info;
		SCI_IOCTL(ctlr_info)        = wd93_ioctl;
		SCI_ABORT(ctlr_info)        = wd93abort;

		SCI_INFO(ctlr_info)         = ci;
		scsi_ctlr_info_put(ctlr_vhdl,ctlr_info);
		scsi_bus_create(ctlr_vhdl);
	} else
		ci = (wd93dev_t *) SCI_INFO(ctlr_info);

	hwgraph_vertex_unref(ctlr_vhdl);
	return(ci);
}
/*
 * get the vertex handle corr. to the wd93 controller
 * with the given adapter number
 */

static vertex_hdl_t
wd93_ctlr_vhdl_get(int adap)
{
	char		loc_str[LOC_STR_MAX_LEN];
	vertex_hdl_t    ctlr_vhdl;
	vertex_hdl_t    hpc_vhdl;

	/* 
	 * just does the path traversal to get the vertex handle
	 * vertex has already been added to the hardware graph
	 */
	sprintf(loc_str, "%s/%s/%s/%s",
		EDGE_LBL_NODE,EDGE_LBL_IO,EDGE_LBL_GIO,EDGE_LBL_HPC);
	(void)hwgraph_traverse(GRAPH_VERTEX_NONE, loc_str, &hpc_vhdl);

	/* 
	 * If hpc_vhdl is not found above it will return GRAPH_VERTEX_NONE,
	 * if GRAPH_VERTEX_NONE is passed to hwgraph_path_add it will start
	 * the graph from /hw by default.
	 */	
	sprintf(loc_str,"%s/%d",EDGE_LBL_SCSI_CTLR,SCSI_EXT_CTLR(adap));
	(void)hwgraph_traverse(hpc_vhdl, loc_str, &ctlr_vhdl);

	return(ctlr_vhdl);
}


/* Return info about requested target/lun */
static scsi_target_info_t *
wd93info(vertex_hdl_t lun_vhdl)
{
	scsi_target_info_t *info;
	scsi_ctlr_info_t        *ctlr_info ;
	u_char                  adap;
	vertex_hdl_t            ctlr_vhdl;
	u_char                  targ,lun;
	scsi_lun_info_t         *lun_info = scsi_lun_info_get(lun_vhdl);

	targ		= SLI_TARG(lun_info);
	lun            	= SLI_LUN(lun_info);
	ctlr_vhdl       = SLI_CTLR_VHDL(lun_info);

	ctlr_info       = scsi_ctlr_info_get(ctlr_vhdl);
	adap            = SCI_ADAP(ctlr_info);

	if (adap >= wd93cnt || targ >= SC_MAXTARG || lun >= SC_MAXLU)
		return NULL;
	if (wd93_inq(lun_vhdl) != NULL) {
		u_char flags = wd93dev[adap].d_syncflags[targ];
		info = &wd93_info[adap][targ][lun];
		info->si_ha_status = 0;
		if(wd93_enable_disconnect[adap])
			info->si_ha_status |= SRH_DISC;
		if(!(wd93_syncenable[adap] & (1 << targ)))
			info->si_ha_status |= SRH_NOADAPSYNC;
		if(flags & S_CANTSYNC)
			info->si_ha_status |= SRH_CANTSYNC;
		if(flags & S_SYNC_ON)
			info->si_ha_status |= SRH_SYNCXFR;
		if(flags & (S_SYNC_ON|S_CANTSYNC))
			info->si_ha_status |= SRH_TRIEDSYNC;
		/* can't do BADSYNC or TRIEDSYNC really right */
	}
	else
		info = NULL;
	return info;
}

/* need to implement things like the host adapter info someday... */
/* ARGSUSED2 */
static int
wd93_ioctl(vertex_hdl_t ctlr_vhdl, uint cmd, scsi_ha_op_t *op)
{
	scsi_ctlr_info_t        *ctlr_info ;
	u_char                  adap;
	int s;
	wd93dev_t *dp;

	ctlr_info       = scsi_ctlr_info_get(ctlr_vhdl);
	adap            = SCI_ADAP(ctlr_info);

	if (adap >= wd93cnt)
		return EINVAL;

	switch(cmd) {
	case SOP_RESET:
		wd93_resetbus(adap);	/* toggle wd93 bus reset line */
		break;
	case SOP_SCAN:
		dp = &wd93dev[adap];
		mutex_lock(&dp->d_slock,PRIBIO);
		wd93hinv(adap);
		mutex_unlock(&dp->d_slock);
		break;
	default:
		return EINVAL;
	}
	return 0;
}

/* XXX: bug: cb still not implemented... */
/* ARGUSED2 */
int
wd93alloc(vertex_hdl_t lun_vhdl, int opt, p_cb_fun_t cb)
{
	wd93dev_t		*dp;
	scsi_request_t		*sprq;
	mutex_t			*opensem;
	int			 ret = ENODEV;
	scsi_ctlr_info_t        *ctlr_info ;
	u_char                  adap;
	vertex_hdl_t            ctlr_vhdl;
	u_char                  target,lu;
	scsi_lun_info_t         *lun_info = scsi_lun_info_get(lun_vhdl);

	target          = SLI_TARG(lun_info);
	lu             	= SLI_LUN(lun_info);
	ctlr_vhdl       = SLI_CTLR_VHDL(lun_info);

	ctlr_info       = scsi_ctlr_info_get(ctlr_vhdl);
	adap            = SCI_ADAP(ctlr_info);

	if(adap >= wd93cnt || target >= SC_MAXTARG || lu >= SC_MAXLU ||
		target == wd93_ha_id[adap])
		return ENODEV;

	opensem = &wd93allocsem[adap][target][lu];
	mutex_lock(opensem, PRIBIO);

	dp = &wd93dev[adap];

	/* check for a valid dma map */
	if (dp->d_map == NULL || dp->d_map == (struct dmamap *) -1L)
		goto wd93allocdone;
	
	if(wd93rqsubchan[adap][target][lu]) {
		if(((opt & SCSIALLOC_EXCLUSIVE) && dp->d_refcnt[target][lu]) || 
			((dp->d_unitflags[target][lu]&U_EXCL) &&
			!(opt&ALLOC_NOEXCL_HACK))) {
			ret = EBUSY;
			goto wd93allocdone;
		}
	}
	else { /* done once only, no matter how many alloc/free pairs */
		if(!(dp->d_unitflags[target][lu] & U_CHMAP)) {
		    /* allocate 2 maps, and add 2 pool */
		    int s;
		    wd93_ha_t *wdh;
		    wdh = kern_calloc(sizeof(wd93_ha_t), 2);
		    if((wdh->wd_map = wd93_getchanmap(dp->d_map)) == NULL)  {
error:			cmn_err(CE_WARN, "Not enough memory for WD93 DMA maps");
			ret = EBUSY;
			goto wd93allocdone;
		    }
		    if((wdh[1].wd_map = wd93_getchanmap(dp->d_map)) == NULL) {
			/* add the one we got, but don't set flag */
			s = mutex_spinlock(&dp->d_qlock);
			wdh->wd_nextha = dp->d_ha_list;
			dp->d_ha_list = wdh;
			wbflush();
			mutex_spinunlock(&dp->d_qlock, s);
			goto error;
		    }
		    s = mutex_spinlock(&dp->d_qlock);
		    wdh->wd_nextha = &wdh[1];
		    wdh[1].wd_nextha = dp->d_ha_list;
		    dp->d_ha_list = wdh;
		    wbflush();
		    mutex_spinunlock(&dp->d_qlock, s);
		    dp->d_unitflags[target][lu] |= U_CHMAP;
		}

		/* Allocate a scsi_request for a request sense command;
		 * also allow enough room for the command bytes */
		sprq = (scsi_request_t *)
			kern_calloc(sizeof(scsi_request_t) + SC_CLASS0_SZ, 1);

		wd93rqsubchan[adap][target][lu] = sprq;
		sprq->sr_ctlr = adap;
		sprq->sr_target = target;
		sprq->sr_lun = lu;
		sprq->sr_command = (u_char *)&sprq[1];
		sprq->sr_cmdlen = SC_CLASS0_SZ;	/* never changes */
		sprq->sr_timeout = 30*HZ;	/* never changes */

		if(opt & SCSIALLOC_EXCLUSIVE)
			dp->d_unitflags[target][lu] |= U_EXCL;
		if(opt & SCSIALLOC_NOSYNC)
			dp->d_syncflags[target] |= S_NOSYNC;
	}
	dp->d_refcnt[target][lu]++;

	ret = SCSIALLOCOK;
wd93allocdone:
	mutex_unlock(opensem);
	return ret;
}

/* ARGSUSED1 */
static void
wd93free(vertex_hdl_t lun_vhdl, p_cb_fun_t cb)
{
	wd93dev_t		*dp;
	mutex_t			*opensem;
	scsi_ctlr_info_t        *ctlr_info ;
	u_char                  adap;
	vertex_hdl_t            ctlr_vhdl;
	u_char                  target,lu;
	scsi_lun_info_t         *lun_info = scsi_lun_info_get(lun_vhdl);

	target          = SLI_TARG(lun_info);
	lu             	= SLI_LUN(lun_info);
	ctlr_vhdl       = SLI_CTLR_VHDL(lun_info);

	ctlr_info       = scsi_ctlr_info_get(ctlr_vhdl);
	adap            = SCI_ADAP(ctlr_info);

	if(adap >= wd93cnt || target > SC_MAXTARG || lu > SC_MAXLU)
		return;

	opensem = &wd93allocsem[adap][target][lu];
	mutex_lock(opensem, PRIBIO);

	dp = &wd93dev[adap];

	if(--dp->d_refcnt[target][lu] == 0) {
		/* clear both U_HADAEN and U_EXCL if final free */
		dp->d_unitflags[target][lu] &= ~(U_HADAEN|U_EXCL);
		/* clear possible nosync from smfd/devscsi also */
		dp->d_syncflags[target] &= ~S_NOSYNC;
	}

	mutex_unlock(opensem);
	return;
}


/*
 * wd93dump - called prior to doing a panic dump
 * Also called internally when we are rebooting, and need
 * to disable sync mode.
 *
 * It really just puts the wd93 driver into dumping mode, by
 * reinitializing the adapter and the wd93 devices.
 * It is not necessary to clear the wait
 * lists since wd93acquire() and wd93release() won't touch them now.
 * We do have to reset the locks however, in case the panic
 * occured while a wd93 device was active in this driver.
 *
 * The actual dumping will be done by calls to wd93command() later.
 *	called at spl7()
 */
static int
wd93dump(vertex_hdl_t ctlr_vhdl)
{
	wd93dev_t *dp;
	scsi_ctlr_info_t        *ctlr_info ;
	u_char                  adap;

	ctlr_info       = scsi_ctlr_info_get(ctlr_vhdl);
	adap            = SCI_ADAP(ctlr_info);

	no_nextcmd = 1;

	dp = &wd93dev[adap];
	dp->d_flags &= ~D_BUSY;
	dp->d_consubchan  = (scsi_request_t *)NULL;

	/* acquire a new lock, but don't free old one; old one
	 * might be in use, and we could fail an assertion, etc.
	 */
	init_spinlock(&dp->d_qlock, "dpq", adap);
	nested_spinlock(&dp->d_qlock);

	wd93_resetbus(adap);	/* be sure all wd93 devices are reset,
		in case one of them caused the problem by hogging the
		wd93 bus, etc. */
	handle_reset(dp);	/* now clean everything up, including flags,
		reset wd, etc. */
	nested_spinunlock(&dp->d_qlock);

	return 1;	/* never fails */
}


/* Called via the io_halt array due to lboot magic.
 * Disable sync mode on all devices prior to rebooting, since proms
 * don't understand sync mode. It is assumed there is no i/o
 * outstanding...
 * wd93_resetbus() causes all devices to forget about sync mode, and
 * wd93reset() ensures that the wd chip does also.
 * The wd93A will be in advanced mode, but this will be reset by
 * the prom.
 * Finally, make sure no intr pending to confuse prom.
*/
void
wd93halt()
{
	int i;

	for (i = 0; i < wd93cnt; i++) {
	    wd93dev_t *dp = &wd93dev[i];
	    /* Don't do reset for systems that can handle sync mode in prom,
	     * if already in sync mode when entered.  Can't reset wd, or won't
	     * be able to tell we were in sync mode.  Only do it if the
	     * adapter was both initialized, and exists.  For others,
	     * just turn off sync mode. */
	    if(dp->d_map && dp->d_map != (struct dmamap *)-1L) {
		if(!(dp->d_flags&D_PROMSYNC)) {
			volatile u_char state;
		    wd93_resetbus(i);
		    wd93reset(dp, 0);
		    ifintclear( ; )
		}
		else {
		    int t;
		    for(t=0; t<SC_MAXTARG; t++)
			if(dp->d_syncreg[t] & 0x0f)
			    (void)_sync_setup(dp, t, 0, 0);
		}
	    }
	}
}

/* main entry point.  Do the housekeeping, start (or queue) the request,
 * and wait for it to complete if no notifier routine was specified.
 *
 * If no_nextcmd is set, we are either standalone, or doing a panic
 * dump, so spin waiting for completion (no interrupts.
 * Use the real time clock to wait the right length of time
 * so that we are independant of clock speed.
 * For the standalone code, we need to catch user interrupts so we can
 * cleanup.  Otherwise next wd93 access usually hangs the program.
*/
static void
wd93command(scsi_request_t *sp)
{
	int s, adap,targ,lun;
	wd93dev_t *dp;

	/* too much checking (cpu cycles) on each command? */
	if(!sp->sr_notify || (adap=sp->sr_ctlr) >= wd93cnt ||
		(targ=sp->sr_target) >= SC_MAXTARG || (lun=sp->sr_lun) >= SC_MAXLU ||
		targ == wd93_ha_id[adap]) {
		sp->sr_status = SC_REQUEST;
#ifdef DEBUG
		printf("null notify or bad unit in wd93_command, failit\n");
#endif
		if(sp->sr_notify)
			(*sp->sr_notify)(sp);
		return;
	}

	dp = &wd93dev[adap];

	if(dp->d_unitflags[targ][lun]&U_HADAEN) {
		if(sp->sr_flags & SRF_AEN_ACK)
			dp->d_unitflags[targ][lun] &= ~U_HADAEN;
		else {
#if DEBUG
			printf("wd93command: Deny resubmiting with AEN_ATN set-");
#endif
			sp->sr_status = SC_ATTN;
			(*sp->sr_notify)(sp);
			return;
		}
	}

	/* clear these here.  If the transaction survives to
	 * wd93subrel() without somebody setting wd_error, there was no error.
	 */
	sp->sr_scsi_status = sp->sr_status = 0;
	sp->sr_sensegotten = 0;
	sp->sr_ha_flags = 0;

	if((sp->sr_flags & SRF_FLUSH) && sp->sr_buflen) {
		if(sp->sr_flags & SRF_DIR_IN)
			dki_dcache_inval(sp->sr_buffer, sp->sr_buflen);
		else if(cachewrback)
			dcache_wb(sp->sr_buffer, sp->sr_buflen);
	}

	/* did higher level request sync negotiation? */
	if(sp->sr_flags & SRF_NEG_SYNC) {
	    if(!(dp->d_syncflags[targ] & S_SYNC_ON)) {
		dp->d_syncflags[targ] &= ~S_NOSYNC;
		sp->sr_ha_flags = SR_NEEDNEG_ON;
	    }
	}
	else if(sp->sr_flags & SRF_NEG_ASYNC) {
	    if(dp->d_syncflags[targ] & S_SYNC_ON) {
		dp->d_syncflags[targ] |= S_NOSYNC;
		sp->sr_ha_flags = SR_NEEDNEG_OFF;
	    }
	}
	else if(*sp->sr_command != 0x12 &&
		!(dp->d_syncflags[targ] & (S_SYNC_ON|S_CANTSYNC|S_NOSYNC)))
		/* not if inq, because don't want to do sync on hinv or info */
		sp->sr_ha_flags = SR_NEEDNEG_ON;

	if(!no_nextcmd) { /* Start or queue the command */
		s = mutex_spinlock(&dp->d_qlock);
		startwd93(dp, sp, 1);
		wbflush();
		mutex_spinunlock(&dp->d_qlock, s);
	}
	else {	/* doing panic dumps, no interrupts */
		uint timeo_time;
restart:
		s = mutex_spinlock(&dp->d_qlock);
		sp->sr_ha = (void *) dp->d_ha_list;
		startwd93(dp, sp, 1);
		wbflush();
		mutex_spinunlock(&dp->d_qlock, s);
		/* want to round up, and use an extra second, since we don't
		 * know just where in the one second interval we are. */
		timeo_time = (wd93mintimeout > sp->sr_timeout) ? wd93mintimeout :
			sp->sr_timeout;
		timeo_time = rtodc() + (timeo_time+2*HZ-1)/HZ;
		/* if sp->sr_ha is null, we must be doing reqsense procesing.
		 * still need to check for interrupts.  By definition if sr_ha
		 * is null, we must still be busy (with the reqsense); a hack I
		 * don't want to cleanup now doesn't null sr_ha on normal
		 * completion if no_nextcmd is set
		 */
		while(!sp->sr_ha || (((wd93_ha_t *)sp->sr_ha)->wd_flags&WD_BUSY)) {
			while(!wd93_int_present(dp) && rtodc() < timeo_time)
				DELAY(25);
			if(wd93_int_present(dp))
				wd93intr(dp->d_adap); /* handle the intr */
			else
				wd93_timeout(sp);
			if(sp->sr_ha && (((wd93_ha_t *)sp->sr_ha)->wd_flags & WD_RESTART))
				goto restart;	/* probably due to AUX_LCI */
		}
	}
}

static void
wd93hinv(uint adap)
{
	wd93dev_t *dp;
	u_char 			targ, lu, oldtimeo;
	u_char			*inqd;
	vertex_hdl_t            ctlr_vhdl, lun_vhdl;
	int refcnt;

	dp = &wd93dev[adap];

	if(!(dp->d_flags & (D_WD93|D_WD93A|D_WD93B))) {
	    if (showconfig)
		cmn_err(CE_NOTE,"wd93: controller %d not present at addr %x\n",
		    adap, dp->d_addr);
		return;
	}

	/* decrease timeout during inventory; with current T93SATN value,
	 * this gives ~8 ms, which should be plenty during system start.
	 * See bug 681340 on why we allow a longer timeout to be configured
	 * now.  The short of it is that some obsolete drives take longer
	 * than 8 msec to respond to the *first* selection after a reset
	 * or power on, and so were never responding to probing... So we make
	 * it 32msec if requested.  master.d/wd93 deliberately just says
	 * "longer", without specific values */
	{
		extern u_char wd93_longselect[];
		int divisor = wd93_longselect[adap] ? 8 : 32;
		oldtimeo = getreg93(WD93TOUT);
		putreg93(WD93TOUT, ((divisor-1)+oldtimeo)/divisor)
	}

	wd93_ctlr_add(adap);
	ctlr_vhdl = wd93_ctlr_vhdl_get(adap);

	/* now add all the wd93 devices for this adapter to the inventory.
	 * wd93 doesn't add lun's other than 0 to hinv; wd93 now (kudzu)
	 * also does the scsihinv() in inq, which means we add devices 
	 * that are successfully inquiried for the first time after boot, just
	 * like the other scsi controller drivers */
	for(targ=0; targ < SC_MAXTARG; targ++) {
		lu = 0;
		if (wd93_ha_id[adap] == targ)
			continue;
		refcnt = dp->d_refcnt[targ][lu];
		lun_vhdl = scsi_device_add(ctlr_vhdl, targ, lu);
		if ((inqd=wd93_inq(lun_vhdl)) == NULL && !refcnt)
			scsi_device_remove(ctlr_vhdl, targ, lu);
		if (inqd)  while(++lu < SC_MAXLU) { /* lun 0 there, check others */
			/* See bug 476670; we broke all non-zero LUNs when we
			 * converted to hwgraph; this fixes it. */
			refcnt = dp->d_refcnt[targ][lu];
			lun_vhdl = scsi_device_add(ctlr_vhdl, targ, lu);
			if((inqd=wd93_inq(lun_vhdl)) == NULL || (*inqd & 0x20) && !refcnt)
				scsi_device_remove(ctlr_vhdl, targ, lu);
		}
	}
	/* For the reference got thru wd93_ctlr_vhdl_get() */
	hwgraph_vertex_unref(ctlr_vhdl);
	/* put timeout back after inventory */
	putreg93(WD93TOUT, oldtimeo)

#if IP20 || IP22 || IP26 || IP28
#if IP20
	if (adap == 0)
#elif IP22
	if (adap <= 3)	/* GIO scsi card in indy/challenge S...; gio cards
		are buses 2 and 3 for indigo2 compatibility, which we never
		actually implemented.  bug 543126 */
#else
	if (adap <= 1)
#endif
		device_inventory_add(ctlr_vhdl, INV_DISK, INV_SCSICONTROL, adap, dp->d_ucode, 
			dp->d_flags & D_WD93A ? INV_WD93A :
			(dp->d_flags & D_WD93B ? INV_WD93B : INV_WD93));
	else
		device_inventory_add(ctlr_vhdl, INV_DISK, INV_GIO_SCSICONTROL, adap, dp->d_ucode, 
			dp->d_flags & D_WD93A ? INV_WD93A :
			(dp->d_flags & D_WD93B ? INV_WD93B : INV_WD93));
#else
	device_inventory_add(ctlr_vhdl, INV_DISK, INV_SCSICONTROL, adap, dp->d_ucode, 
		dp->d_flags & D_WD93A ? INV_WD93A :
		(dp->d_flags & D_WD93B ? INV_WD93B : INV_WD93));
#endif	/* IP20 || IP22 || IP26 || IP28 */

}



/*	prevent chip from interrupting during rest of boot till ready,
	and do any initialization that is needed prior to use of
	the driver.  Called before main().  Needs to return a value
	for standalone.
*/
wd93_earlyinit(int adap)
{
	wd93dev_t *dp; 
	uint i;

	/* allocate array on first call; wd93cnt may be changed in
	 * IP*.c before the first call to this routine. */
	if(!wd93dev &&
		!(wd93dev=(wd93dev_t*)kern_calloc(sizeof(*wd93dev), wd93cnt)))
		cmn_err(CE_PANIC, "no memory for wd93 device array");

	dp = &wd93dev[adap]; 
	dp->d_addr  = scuzzy[adap].d_addr;
	dp->d_data  = scuzzy[adap].d_data;
	dp->d_adap = adap;
	dp->d_map = (dmamap_t *)-1L; /* in case not present, for wd93alloc() */

	/* just a quick register test to verify chip is present */
	for(i = 0; i < SC_MAXTARG; i++) {
		u_char val;
		putreg93(WD93ID, i)
		val = getreg93(WD93ID);
		if (val != i)
			return -1;
	}
	dp->d_map = dma_mapalloc(DMA_SCSI, adap, 0, 0);
	if(dp->d_map == ((dmamap_t *)-1L))
		return -1;
	init_sv(&dp->d_sleepsem, SV_DEFAULT, "dps", adap);
	init_spinlock(&dp->d_qlock, "dpq", adap);
	init_mutex(&dp->d_slock, MUTEX_DEFAULT, "scan", adap);

	/*
	 * Initialize for fastest possible async transfer rate
	 */
	for (i = 0; i < SC_MAXTARG; i++)
	    dp->d_syncreg[i] = 0x20;

	{
		extern char arg_wd93hostid[];
		char	h_id = arg_wd93hostid[0];
		if ((h_id >= '0') && (h_id <= '7')) {
			wd93_ha_id[adap] = h_id - '0';
		}
	}
	wd93reset(dp, 0);	/* reset so we can tell if 93, 93A, or 93B */


	return 0;
}

static int wd93initdone;

/* do hinv stuff, and set scsi_* stuff */
void
wd93edtinit()
{
	/* must be in bss for standalone when re-initted.  */
	wd93dev_t *dp; 
	uint adap, lu, targ;
	extern char arg_wd93_syncenable[];
	u_char syncmap;

	if(wd93initdone)
		return;	/* only done once */
	wd93initdone = 1;

	/* avoid later multiplies, and allows users to set in seconds */
	wd93mintimeout *= HZ;

	/* if they told us a bitmask in standalone, use it for all adaps */
	if(is_specified((char *)arg_wd93_syncenable))
		syncmap = atoi((char *)arg_wd93_syncenable);
	else
		syncmap = 0;

#ifdef IP22
	/*
	 * With the optional wd93 GIO SCSI controller, initialization
	 * isn't done until the kernel starts.  As a consequence, some
	 * devices will stay in the reset state until the kernel boots.
	 * Since it can take a substantial period of time for these
	 * devices to run their selftests, and they don't respond to
	 * selection until these tests are done, we have to delay a
	 * while for them to finish.  5 seconds has been empirically
	 * determined to be enough for IBM drives.  The wait may have
	 * to be increase some day.
	 */
	if (wd93cnt > 2)
		DELAY(5000000);
#endif

	for(adap=0; adap<wd93cnt; adap++) {
	    if(!wd93dev[adap].d_map || (__psint_t)wd93dev[adap].d_map == -1)
			continue; /* failed to find/init this SCSI chip */
		if(syncmap)
			wd93_syncenable[adap] = syncmap;
		for(targ = 0; targ < SC_MAXTARG; targ++) {
		    for(lu = 0; lu < SC_MAXLU; lu++) {
			init_mutex(&wd93allocsem[adap][targ][lu], MUTEX_DEFAULT,
				"w93o", (targ<<5)+(lu<<4));
		    }
		}
		dp = &wd93dev[adap];
#if defined(IP20) || defined(IP22)	/* not needed on IP26 */
		if(physmem > 0x8000
#if defined(IP22)
			&& wd93cnt>2	/* i.e., has gio32 scsi on Indy */
#endif
		) {	/* only if > 128 MB of memory */
			int i;
			wd93_ha_t *wdh = kern_calloc(sizeof(wd93_ha_t), wd93_ha_structs);
			dp->d_ha_list = wdh;
			for(i=1; i<(wd93_ha_structs-1); i++, wdh++) {
				wdh->wd_nextha = &wdh[1];
				if((wdh->wd_map = wd93_getchanmap(dp->d_map)) == NULL) 
				panic("Not enough memory for WD93 DMA maps");
			}
			/* mark all possible allocated, so none done at alloc time */
			for(targ = 0; targ < SC_MAXTARG; targ++) {
				for(lu = 0; lu < SC_MAXLU; lu++) {
					dp->d_unitflags[targ][lu] |= U_CHMAP;
				}
			}
			wdh->wd_nextha = NULL;
		}
		else
#endif /* IP20 || IP22 */
			/* do it wd93alloc for all others.  Note that we will
			 * always allocate at least 14 per bus in wd93alloc,
			 * because of hinv probing, but that is a lot less than
			 * 128 per bus. (i.e., no per LUN maps get allocated) */
			dp->d_ha_list = NULL;

		wd93hinv(adap);
	}
}


/*
 * This pair of routines are used to do the hardware inventory, etc.
 * wd93_inq returns NULL on errors, else a ptr to up to SCSI_INQUIRY_LEN
 * bytes of inquiry data, which is enough for type info, manufacturer
 * name, product name, and rev.  wd93inqdone is used as the notify
 * routine for wd93command.
 */
static void
wd93inqdone(scsi_request_t *sp)
{
	vsema(sp->sr_dev);
}

static u_char *
wd93_inq(vertex_hdl_t lun_vhdl)
{
	scsi_request_t	*sp;
        scsi_target_info_t	*info;
	u_char			*ret;
	mutex_t			*opensem;
	sema_t			*donesem;
	int			 tries = 4;
	scsi_ctlr_info_t        *ctlr_info ;
	u_char                  adap;
	vertex_hdl_t            ctlr_vhdl;
	u_char                  targ,lun;
	scsi_lun_info_t         *lun_info = scsi_lun_info_get(lun_vhdl);

	targ		= SLI_TARG(lun_info);
	lun		= SLI_LUN(lun_info);
	ctlr_vhdl       = SLI_CTLR_VHDL(lun_info);

	ctlr_info       = scsi_ctlr_info_get(ctlr_vhdl);
	adap            = SCI_ADAP(ctlr_info);

	if(wd93alloc(lun_vhdl, ALLOC_NOEXCL_HACK|1, NULL) == SCSIDRIVER_NULL)
		return NULL;
	opensem = &wd93allocsem[adap][targ][lun];
	mutex_lock(opensem, PRIBIO);

	info = &wd93_info[adap][targ][lun];
	if (info->si_inq == NULL) {
		info->si_inq = kmem_alloc(SCSI_INQUIRY_LEN, KM_CACHEALIGN);
		info->si_sense = kmem_alloc(SCSI_SENSE_LEN, KM_CACHEALIGN);
	}

	sp = (scsi_request_t *)kern_calloc(sizeof(*sp) + SC_CLASS0_SZ, 1);
	sp->sr_command = (u_char *)&sp[1];
	sp->sr_sense = info->si_sense;
	sp->sr_senselen = SCSI_SENSE_LEN;
	donesem = kern_calloc(sizeof(*donesem), 1);
	init_sema(donesem, 0, "wiq", targ);
	sp->sr_dev = (void *)donesem;

	sp->sr_notify = wd93inqdone;
	sp->sr_command[0] = 0x12;
	sp->sr_command[1] = lun << 5;
	sp->sr_command[4] = SCSI_INQUIRY_LEN;
	sp->sr_cmdlen = SC_CLASS0_SZ;
	sp->sr_timeout = 10 * HZ;
	sp->sr_ctlr = adap;
	sp->sr_target = targ;
	sp->sr_lun = lun;

again:
	bzero(info->si_inq, SCSI_INQUIRY_LEN);
	sp->sr_buffer = info->si_inq;
	sp->sr_buflen = SCSI_INQUIRY_LEN;
	/* set AEN_ACK in case AEN still set from an earlier command;
	 * we have to assume that when this routine is called, that a
	 * reqsense has already been done... */
	sp->sr_flags = SRF_DIR_IN | SRF_MAP | SRF_AEN_ACK | SRF_FLUSH;
	wd93command(sp);
	psema(donesem, PRIBIO);

	if(sp->sr_status == SC_GOOD && sp->sr_scsi_status == ST_GOOD)
		ret = info->si_inq;
	else if (tries-- && sp->sr_status != SC_TIMEOUT &&
		 sp->sr_status != SC_CMDTIME)
	{
#ifdef DEBUG
		/* debug only, since you will get an error any time you
		 * retry on a check condition (which happens any time a
		 * device like tape, printer, etc., isn't ready). */
		printf("wd93(%d,%d,%d): error getting inquiry data srstat=%x, retrying\n",
		       adap, targ, lun, sp->sr_status);
		if(sp->sr_sensegotten>0) {
			int i;
			printf("chkcond: %x ", *sp->sr_sense);
			for(i=1; i<sp->sr_sensegotten; i++)
				printf("%x ", sp->sr_sense[i]);
			printf("\n");
		}
		else
			printf("no rqsense: status=%x scsistat=%x\n", sp->sr_status, sp->sr_scsi_status);
#endif
		delay(HZ/4);
		goto again;
	}
	else
	{
		kmem_free(info->si_inq, SCSI_INQUIRY_LEN);
		kmem_free(info->si_sense, SCSI_SENSE_LEN);
		info->si_inq = NULL;
		ret = NULL;
	}
	kern_free(sp);
	freesema(donesem);
	kern_free(donesem);
	mutex_unlock(opensem);
	wd93free(lun_vhdl, NULL);

	/* If attached, and lun supported, add to inventory.
	 * This means we add devices even if not present at boot
	 * (as of kudzu).  Could possibly show multiple devices at
	 * same ID, if they are hot swapped...
	*/
	if(ret && !(ret[0] & 0x20)) {
		scsi_device_update(ret, lun_vhdl);
		if (showconfig)
			cmn_err(CE_NOTE,"wd93: controller %d targ %d, lun %d\n",adap,targ, lun);
	}

	return ret;
}

/*
 * wd93intr - the SCSI interrupt handler.
 */
void
wd93intr(int adap)
{
	wd93dev_t		*dp = &wd93dev[adap];
#ifdef EVEREST 
	struct scsi_request	*sp;
	struct wd93_ha		*ssp;
#endif

	nested_spinlock(&dp->d_qlock);
#ifdef EVEREST
	/*
	 * On Everest, we need to turn off DMA right away.
	 */
	sp = dp->d_consubchan;
	if (sp)
	{
		ssp = (struct wd93_ha *) sp->sr_ha;
		scsidma_flush(ssp->wd_map, ssp->wd_dstid & SCDMA_IN);
	}
#endif
	if(getauxstat() & AUX_INT) {
		dp->d_flags |= D_BUSY;	/* busy now */
		handle_intr(dp);
	}
	nested_spinunlock(&dp->d_qlock);
}



/* send an abort message to a device, expecting it to go to bus free
 * afterwards.  Perhaps should have this a more general interface
 * and document it, but for now this is all we need, and easier
 * to maintain.  
*/
static
wd93abort(scsi_request_t *sp)
{
	wd93dev_t *dp;
	int s, ret=0;
	u_char msg[2], state;

	dp = &wd93dev[sp->sr_ctlr];
	s = mutex_spinlock(&dp->d_qlock);
	sp->sr_scsi_status = sp->sr_status = 0;


	sp->sr_ha_flags |= SR_NEED_ABORT;
	(void)wd93acquire(dp, sp, 0);
	/*	we don't set WD_BUSY, because handle_reset() would
		call wd93subrel, which would do a vsema on the semaphore;
		that wouldn't be good... All the code from here on checks
		wd_error where appropriate. */
	if(sp->sr_status)
		goto release;	/* probably a wd93 bus reset */

	/* ID msg (no disc) followed by the abort msg byte */
	msg[0] = 0x80; msg[1] = 6;

	/* Now send the message */
	putreg93(WD93LUN, sp->sr_lun)
	putreg93(WD93DESTID, sp->sr_target)

	/*	get to initiator state. */
	if(ret=do_select(dp)) {
		sp->sr_status = ret;
		goto error;
	}
	if(ret = do_trinfo(dp, msg, 2, 75000))
		goto error;
	state = wait_scintr(dp, 50000);
	if(state != ST_DISCONNECT) {
		wd93cmd_lci(dp, C93DISC);
		state = wait_scintr(dp, 2000);
	}

	if(!(getauxstat() & AUX_BSY))
		goto release;
	/* else fall through to error case */

error:
	wd93cmd_lci(dp, C93DISC);	/* try to ensure disconnected on errors */
	ifintclear( ; )
	if(!(getauxstat() & AUX_BSY)) {
		/* All done, cleaned up OK */
		putreg93(WD93PHASE, PH_NOSELECT)
	}
	else { /* chip still busy, blow it all away */
		wd93_resetbus(dp->d_adap);
		handle_reset(dp);
		cmn_err(CE_NOTE,
			"wd93 SCSI Bus=%d ID=%d LUN=%d: error during abort message, resetting bus\n",
			sp->sr_ctlr, sp->sr_target, sp->sr_lun);
		/* no callers check today, but some might someday */
		sp->sr_status = SC_HARDERR;
	}

release:
	sp->sr_ha_flags &= ~SR_NEED_ABORT;
	wd93notactive(dp, sp, 0); /* remove from the active list */
	wd93release(dp);

	wbflush();
	mutex_spinunlock(&dp->d_qlock, s);
	return ret ? 0 : 1;	/* defined to return 0 on error */
}


/*	do sync negotiations.  If enable is non-zero, we are setting it
	up, if zero, we want to turn it off (like for large transfers where
	we can't map the whole thing).  Must already own the adapter.
	must at at spl5 or above.
	This routine intermittently fails if other channels are busy
	and try to re-select.  Therefore it must only be called
	when no other channels are busy. 

	This process typically takes 2-5 ms to complete, since we have
	to handle all the phases.  Someday the driver should be fixed to
	set a flag that says to ignore wd93 interrupts, so we can run at a
	lower spl.  Fortunately, this doesn't happen very often.

	selected is 1 if the target initiated the negotiation, otherwise 0.

	Called from sync_setup in all cases, except from wd93halt, which
	is the reason this exists as a seperate routine with no sp struct
*/
static int
_sync_setup(wd93dev_t *dp, int targ, int enable, int selected)
{
	uint istate;
	u_char transperiod, ackoff;
	u_char state, cnt;
	u_char msg[6], msgin[5], *mptr;
	int ret, didmsgin, sel_status = 0;
	
	/* syncreg in case fail to enable sync, and for disables.  Clear
	 * targsync for wd93free; if we are called, driver knows target
	 * is in sync mode (except for handle_extmsgin, which sets targsync
	 * at end). */
	dp->d_syncreg[targ] = 0x20;

	/* for disable, or failed enable */
	dp->d_syncflags[targ] &= ~(S_SYNC_ON|S_TARGSYNC|S_CANTSYNC);

	if(enable) {
		/* done here also, so it can be changed on the fly with the
			kernel debugger or dbx */
		if(!(wd93_syncenable[dp->d_adap] & (1 << targ))) {
			/* set flag for this case, so we don't keep trying
			 * to enable from startwd93() */
			dp->d_syncflags[targ] |= S_NOSYNC;
			return 0;
		}
		/* 93A has a 12 deep fifo, 93 has 5, but Tech Brief JL0014
			from John Laube at WD says to use at most 4 on the 93.
			Turns out that you get about 7% better throughput on
			the 93A (at least on IOC2 on IP6), if you use 10 (there is
			no improvement at 12 on an IP12, so just leae it at 10). */
		ackoff = (dp->d_flags&(D_WD93A|D_WD93B)) ? 10 : 4;
	}
	else
		ackoff = 0;

	/* ID byte followed by 'sync' mesg; these were static initializers,
	 * but that won't work for PROM; make sure that for target
	 * iniatated sync, that we respond with nothing faster/longer
	 * than the target gave us, or the negotiations are likely
	 * to fail */
	msg[5] = selected >> 16;
	transperiod = selected>>8;
	if(transperiod>175) transperiod = 175;	/* slowest we can do */
	if(transperiod < wd93_syncperiod[dp->d_adap])
		/* not targ initiated sync, or faster than we can do */
		transperiod = wd93_syncperiod[dp->d_adap];
	if(!(dp->d_flags & D_WD93B) && transperiod<50)
		transperiod = 50;	/* only 93B can do fast wd93 */
	msg[4] = transperiod;
	msg[0] = 0x80; msg[1] = 1; msg[2] = 3; msg[3] = 1;
	if(!selected || msg[5] > ackoff)
		/* not targ initiated sync, or it has deeper offset than
		 * we do */
		msg[5] = ackoff;

	/* in case of an error, don't want our original values in d_syncreg! */
	transperiod = ackoff = 0;

	putreg93(WD93SYNC, 0)	/* don't want to try to transfer
		any data synchronously, in case previous device was in
		sync mode. */
	putreg93(WD93LUN, 0)
	putreg93(WD93DESTID, targ)

	/*	get to initiator state. This generates 2 interrupts, at
		least with CDC 94171.  first is status ST_SELECT,
		2nd is status ST_REQ_SMESGOUT */
	if(!selected && (sel_status=do_select(dp)))
		goto error;

	if(!selected)
		mptr=msg, cnt=sizeof(msg);
	else
		mptr=msg+1, cnt=sizeof(msg)-1;	/* no 0x80 */
	if(ret = do_trinfo(dp, mptr, cnt, 10000)) {
		if(ret == -1) {
			istate = do_trinfo(dp, msgin, -1, 10000);
			if(getstat() == ST_TRANPAUSE)
				wd93cmd_lci(dp, C93NACK);	/* negate the ack */
			/* should get phase change */
			(void)wait_scintr(dp, 1000);
			if(istate == 7 || (!istate && *msgin == 7)) {
				msgin[4] = 0; /* can't sync */
				msgin[3] = 0xff; /* 800 ns */
				dp->d_syncflags[targ] |= S_CANTSYNC;
				goto done;	/* handle cmd, etc. */
			}
		}
		goto error;	/* other fail, or not rej msg in */
	}

	/* wait for req msg in, then read the response; in some cases,
	 * such as a target initiated sync msg, we may not get an
	 * interrupt here. */
	istate = wait_scintr(dp, 2000);
	if((istate & 7) == 2) { /* get 1a on Toshiba 156FB,
		Most devices wait for the msg out phase ... */
		dp->d_syncflags[targ] |= S_CANTSYNC;
		if(do_inquiry(dp))
			goto inq_err;
		return 0;
	}

	if((istate & 7) == 7) {	/* only if bus phase is msgin */
		istate = do_trinfo(dp, msgin, -(int)sizeof(msgin), 10000);
		didmsgin = 1;
		if(getstat() == ST_TRANPAUSE)
			wd93cmd_lci(dp, C93NACK);	/* negate the ack */
		/* should get an intr, with state != pause */
		(void)wait_scintr(dp, 5000);
		if(istate) {
			if(istate == 7) {
				msgin[4] = 0; /* can't sync */
				msgin[3] = 0xff; /* 800 ns */
				dp->d_syncflags[targ] |= S_CANTSYNC;
			}
			else
				goto error;
		}
		else if(msgin[2]!=1 || msgin[1]!=3) {
			/* oops, whatever we got back, it wasn't a sync msg! */
			dp->d_syncflags[targ] |= S_CANTSYNC;
			goto error;
		}
	}
	else if(!selected && (istate & INTR_ERR))
		/* didn't get message out phase, and we initiated sync neg */
		goto error;
	else /* often happens on target initiated sync msg; since our
	 * limits are fairly low.  target accepted our reply to its
	 * sync msg, and we are probably at data in or our */
		didmsgin = 0;

done:
	ifintclear(;);

	state = getstat();	/* after the intr is cleared */
	if((state & 7) == 6) {
		/* message out; see this sometimes on sony m/o that does a
		 * msg reject after first byte of xtd msg out, then after we
		 * read the msg byte, it goes to msg out again; send an
		 * abort message */
		*msg = 0x6;
		(void)do_trinfo(dp, msg, 1, 10000);
	}

	/* MCI == cmd and 0x8 bit set means device wants a cmd; chk state again,
	 * in case it changed from the nack, etc.  0x40 is because we
	 * sometimes get this on msg rej back, and the wd won't change
	 * the state until another phase change; this is a bit of a hack... */
	if((state & 7) == 2 || state == 0x40)
		if(do_inquiry(dp))
			goto inq_err;

	/* on 93A, the transfer period affects async also, so always
	 * do this even if ackoff is 0 */
	if(didmsgin || selected) {
		transperiod = msgin[3] > msg[4] ? msgin[3] : msg[4];
		if(enable) {
			ackoff = msgin[4] < msg[5] ? msgin[4] : msg[5];
			if(ackoff == 0)	/* so we won't try again */
				dp->d_syncflags[targ] |= S_CANTSYNC;
		}
	}

	if(enable) {
		if(transperiod) {
			transperiod = transval(transperiod);
			if(dp->d_flags & D_WD93)
				transperiod++;	/* 93 is much more limited when
					using DMA, but that's life... */
		}
		dp->d_syncreg[targ] = (transperiod << 4) |
			(ackoff & 0xf);
		if(ackoff)
			dp->d_syncflags[targ] |= S_SYNC_ON;
	}
	/* else leave 0 if disabled */

	/* clear possible disc intr (observed on some Wren III disks);
	 * unfortunately, this can clear a reselection interrupt in
	 * some cases, when the reselecting device is fast enough to
	 * get back on the bus, which is part of the reason we don't
	 * do sync negotiations with other commands active */
	ifintclear(;);
	if(!(getauxstat() & AUX_BSY))
		return 0;

error:
	if(sel_status == SC_TIMEOUT)
		dp->d_syncflags[targ] |= S_CANTSYNC;
	wd93cmd_lci(dp, C93DISC);	/* try to ensure disconnected on errors */
	ifintclear( ; )
	if(!(getauxstat() & AUX_BSY)) {
		/* All done, cleaned up OK */
		putreg93(WD93PHASE, PH_NOSELECT)
		return sel_status;
	}
	/* else chip still busy, blow it all away */

inq_err:
	cmn_err(CE_NOTE,
		"wd93 SCSI Bus=%d ID=%d: SYNC negotiation error, resetting bus\n",
		dp->d_adap, targ);
	/* make sure we do not keep trying; might be due to some other
	 * device, but this is safest... */
	dp->d_syncflags[targ] |= S_CANTSYNC;
	wd93_resetbus(dp->d_adap);
	handle_reset(dp);
	return SC_HARDERR;
}

/* just call _sync_setup, after getting targ, lun; _sync routine
 * exists for wd93halt, which has no sp.
*/
static int
sync_setup(wd93dev_t *dp, scsi_request_t *sp, int enable, int selected)
{
	int flag = sp->sr_ha_flags;
	sp->sr_status = sp->sr_scsi_status = 0;
	sp->sr_ha_flags &= ~(SR_NEEDNEG_ON|SR_NEEDNEG_OFF);

	/* because in wd93command we could set NEEDNEG_ON for multiple
	 * commands and queue a number of them before any of them do
	 * the negotiation */
	if((flag & SR_NEEDNEG_ON) && (dp->d_syncflags[sp->sr_target]&S_SYNC_ON))
		return 0;

	if(sp->sr_status=_sync_setup(dp, sp->sr_target, enable, selected)) {
		/* if fails due to an external reset or device timeout, or the
		 * like, rather than timeout or error, can still be connected,
		 * so that must be cleaned up, as callers assume we are no longer
		 * connected after a sync failure */
		if(dp->d_consubchan == sp) {
			wd93notactive(dp, sp, 0);
			wd93release(dp);
		}
		return 1;
	}
	return 0;
}


/*	return the correct bits to put in the sync register given
	a tranfer period in ns/4, assuming WD clocked at 10Mhz.
	Works same at 20 Mhz with divisor of 4.
	For fast SCSI, we set one more bit, which doubles frequency,
	but doesn't give us a very wide range (100ns)
*/
static u_char
transval(u_char period)
{
#define FSS_BIT 8
	u_char ret;

	if(period > 175)
		ret = 0; /* slowest we can do is 800 ns, so use async */
	else if(period < 50) {	/* fast SCSI; slightly different skew
		timings, etc., so can't use FSS bit with periods > 50, even
		though it would give us 250 and 350 ns transfer periods, which
		we have wanted... */
		if(period <= 25)
			ret = FSS_BIT|2;
		else if(period <= 38)
			ret = FSS_BIT|3;
		else
			ret = FSS_BIT|4;
	}
	else
		ret = (period+24) / 25;
	return ret;
}


/*	this routine returns the # of busy channels other than the
	current owner of the adapter (if any).  Must be at spl5.
*/
static
wd93busycnt(wd93dev_t *dp)
{
	scsi_request_t *sp;

	for(sp=dp->d_active; sp; sp=((wd93_ha_t *)sp->sr_ha)->wd_link)
		if(sp != dp->d_consubchan)
			return 1;
	return 0;
}


/* raise and drop ack for a byte we got from the data register.
 * SHOULD be able to use trinfo, but apparently there is some
 * kind of bug in the WD chip (that I haven't yet figured a
 * way around), if req is already asserted on msg in, and is
 * reasserted as quickly as possible after ack drops.  the
 * trinfo cmd doesn't complete, even when the SBT bit is set.
 * So, the hack is to issue a trinfo cmd, then an abort.
*/
static void
ack_msgin(wd93dev_t *dp)
{
	volatile unchar state;

	wd93cmd_lci(dp, C93TRINFO|0x80);	/* single byte */
	DELAY(1);	/* be sure ack actually gets asserted */
	wd93cmd_lci(dp, C93ABORT);

	while(getauxstat() & AUX_DBR)
		state = getdata(); /* flush the fifo on the wd */
	(void)wait_scintr(dp, 2000);	/* should always get intr; after
		the fifo drained, not before... */

	wd93cmd_lci(dp, C93NACK);	/* negate the ack */
	DELAY(10);	/* give time for phase to change and chip to notice;
		for xtmsgin, may not even get phase change. */
	ifintclear(;) ;
}

/*	do a trinfo phase; if cnt is < 0, it's a transfer in, otherwise
	out.  The dest register should already be set up.
	Assumes used ONLY for msgin and msgout phases.
*/
static
do_trinfo(wd93dev_t *dp, u_char *ibuf, int cnt, uint usecs)
{
	int dirin = cnt < 0;
	u_char state;
	int i;

	if(dirin)
		cnt = -cnt;

	putcnt(0,0,cnt);
	wd93cmd_lci(dp, C93TRINFO);

#define TRMSGWAIT 700	/* give them up to 5 ms total for actual'
	bytes to be transferred; delay for phase change from usecs */
	ifintclear(; );
	for(i=0; i<cnt; i++, ibuf++) {
		register wait;
		/* if we get AUX_INT, phase must have changed; it shouldn't */
		for(wait=0; !((state=getauxstat()) & (AUX_DBR|AUX_INT)) &&
			wait < TRMSGWAIT; wait++)
			DELAY(7);
		if(wait == TRMSGWAIT)
			goto error;
		if(dirin) {
			*ibuf = getdata();
			if(i==0 && cnt != 1 && *ibuf == 7) {
				cnt = 0;	/* flag as reject msg */
				goto error;
			}
		}
		else {
			putdata(*ibuf);
		}
		if(state & AUX_INT)
			goto error;
	}
	/* should always get intr. usually 10 ms is enough even for
	 * slow drives like hitachi, which  takes a LONG
	 * time when the inq cmd is sent from do_inq().  It turns out
	 * that when we are finishing up the last bytes of a non-6 byte
	 * command for the 93, that some devices take a LONG time to
	 * go to the next bus phase.  In particular, the Toshiba CDROM
	 * takes up to ~450 ms from last byte of the c0 'play' cmd to
	 * the next phase (status, in this case).  This is pretty
	 * gross...  It could come up with some 3rd party devices
	 * also, so even though the toshiba firmware will be fixed,
	 * this must remain. */
	(void)wait_scintr(dp, usecs);
	if(getauxstat() & AUX_BSY) {
		wd93reset(dp, 0);
		return 1;	/* cmdabort on return for most callers */
	}
	return 0;

error:
	ifintclear(; );
	if(state == ST_TRANPAUSE) /* abort doesn't work if ack pending */
		wd93cmd_lci(dp, C93NACK);
	/* no intr from nack, but should go to new bus phase, causing intr */
	i = wait_scintr(dp, 1000);
	if(!dirin && (getstat()&0x7)==7) {
		return -1;	/* if doing msgout and get msgin, caller may want
		to see if msgin is reject */
	}
	ifintclear(; );
	wd93cmd_lci(dp, C93ABORT);
	while(getauxstat() & AUX_DBR)
		state = getdata(); /* flush the fifo on the wd */
	i = wait_scintr(dp, 1000);	/* should always get intr */
	return cnt ? 1 : 7;
}


/*	send an inquirycmd because in process of setting up sync
	mode, the CDC drives at least want a message; sending an
	abort msg or disconnecting doesn't work.
	An inquiry cmd is chosen because it works even with an
	ATN pending, and won't clear the ATN, so the upper level
	drivers don't lose anything.
*/
static
do_inquiry(wd93dev_t *dp)
{
	u_char aux, state;
	int wait;
	static u_char rdy[SC_CLASS0_SZ] = { 0x12 };	/* no data */

	if(do_trinfo(dp, rdy, sizeof(rdy), 50000))
		goto error;
	state = getstat();
	if(state != ST_TR_STATIN) {
		if((wait_scintr(dp, 50000) & INTR_ERR) ||
			getstat() != ST_TR_STATIN)
			goto error;
	}

	if(do_trinfo(dp, &aux, -1, 10000))	/* status byte */
		goto error;

	if((state=getstat()) != ST_TR_MSGIN) {
		(void)wait_scintr(dp, 10000);
		if(getstat() != ST_TR_MSGIN)
			goto error;
	}

	if(do_trinfo(dp, &aux, -1, 10000))	/* command complete */
		goto error;

	wd93cmd_lci(dp, C93NACK);	/* negate the ack */
	state = wait_scintr(dp, 5000);

	if(state != ST_DISCONNECT) {
		while(wd93busy())
			;
		for(wait=0; !(getauxstat()&AUX_INT) && wait<100000; wait++)
			;
		state = getstat();
	}
	wd93cmd_lci(dp, C93DISC);
	state = wait_scintr(dp, 2000);
	return 0;
error:
	return 1;
}


/*
 * wd93_timeout	- called if SCSI request takes too long to complete
 *
 * If the request is still in progress (the subchannel is still busy)
 * then abort it.  Called at high enough priority to hold off wd93
 * interrupts.
*/
static void
wd93_timeout(scsi_request_t *sp)
{
	wd93dev_t		*dp;
	int			 tval;
	char			*ms;
	int			s;
	struct wd93_ha		*ssp;

	dp = &wd93dev[sp->sr_ctlr];
	s = mutex_spinlock(&dp->d_qlock);
	ssp = (struct wd93_ha *) sp->sr_ha;

	if(ssp && (ssp->wd_flags & WD_BUSY)) {
#ifdef EVEREST
		/*
		 * On Everest, we need to turn off DMA.
		 */
		scsidma_flush(ssp->wd_map, ssp->wd_dstid & SCDMA_IN);
#endif
		wd93timeouts++;
		dp->d_flags |= D_BUSY;
		tval = (wd93mintimeout > sp->sr_timeout) ? wd93mintimeout :
			sp->sr_timeout;
		if((tval % HZ) == 0)
			ms = "", tval /= HZ;
		else
			ms = "m", tval *= 1000/HZ;
		sp->sr_status = SC_CMDTIME;	/* primarily for devscsi */
		wd93_cmdabort(dp, sp, "timeout after %d %ssec", tval,
			      (__psint_t)ms);
	}
	wbflush();
	mutex_spinunlock(&dp->d_qlock, s);
}


static void
wd93reset(wd93dev_t *dp, int noadv)
{
	u_char state;
	uint wstat;
	static firsttime;	/* in BSS, so if an 'init' is done, or we
		return to the PROM, we check the sync register again */
	u_char wd93freq = scuzzy[dp->d_adap].d_clock;
	u_char toutval = wd93freq ? T93SATN * 2 : T93SATN;

	if(!noadv)	/* only for standalone wd93free, if not IP12 */
		wd93freq |= 0x20|8;	/* 8 will enabled advanced features for 93A,
			giving a status of 1 after 93RESET, instead of 0.
			0x20 is RAF (really advanced features) for 93B. */

	while(wd93busy())	/* ensure the reset cmd isn't ignored */
		;
	/*	might be syncmode still from the prom or sash, so save
		the info if we were; see wd93alloc and wd93free
		for actual use */
	if(firsttime == 0) {
		firsttime = 1;
		state = getreg93(WD93SYNC);
	}

	putreg93(WD93ID, wd93freq | wd93_ha_id[dp->d_adap]);
#if wd93_verbose
	printf("wd%d: WD93ID = 0x%x\n", dp->d_adap,
	       wd93freq | wd93_ha_id[dp->d_adap]);
#endif

	/* clear possible interrupt to ensure chip accepts the command */
	ifintclear(; );
	putcmd93(C93RESET)
	wstat = wait_scintr(dp, 1000);	/* not too long; some systems have
		more than one wd93 chip; others don't */
	if(wstat & INTR_ERR) {
		wstat &= ~INTR_ERR;
		cmn_err(CE_WARN, "wd93 controller %d didn't reset correctly\n",
			dp->d_adap);
	}
	if(wstat == 1) {	/* A or B */
		if(wd93burstdma)	/* use burst mode DMA */
			dp->d_ctrlreg = 0x2d;
		else
			dp->d_ctrlreg = 0x8d;
		wstat = getreg93(WD93C1);	/* microcode rev on 93 A and B */
		dp->d_ucode = wstat;	/* set it for A also; A has revs 0-A,
			B has (as of 10/91) revs B and C. */
		if(wstat >= 0xB)
			state = D_WD93B;
		else
			state = D_WD93A;
	}
	else {
		dp->d_ctrlreg = 0x8d;
		state = D_WD93;
	}
	dp->d_flags = scuzzy[dp->d_adap].d_initflags | state;

	if(wd93_enable_disconnect[dp->d_adap])
		putreg93(WD93SRCID, 0x80) /* enable reselection */
	else
		putreg93(WD93SRCID, 0x0) /* no disconnects; for devices
		like Sony C501 that support disconnect, but don't work
		correctly (as of 9/89) */
	putreg93(WD93CTRL, dp->d_ctrlreg)
#if wd93_verbose
	printf("wd%d: WD93CTRL got 0x%x\n", dp->d_adap, dp->d_ctrlreg);
#endif
	*dp->d_data = toutval;	/* set selection timeout (uses
						auto-address-reg increment) */
}


/*	Reset the wd93 bus, and cleanup all pending commands.
	There are some cases where an abort message could be sent, but
	not in general.  The few exceptions aren't worth the effort.
	Now prints the messages before dumping wd93 info (for debug
	kernels), then do the reset, so it is easier to see what is
	going on in SYSLOG files.
*/
static void
wd93_cmdabort(wd93dev_t *dp, scsi_request_t *sp,
	char *msg, long a1, long a2)
{
	wd93aborts++;
	if(sp)
		cmn_err_tag(10, CE_WARN, "wd93 SCSI Bus=%d ID=%d LUN=%d: SCSI cmd=0x%x ", dp->d_adap,
			sp->sr_target, sp->sr_lun, *sp->sr_command);
	else
		cmn_err(CE_CONT, "wd93 SCSI Bus=%d: ", dp->d_adap);
	cmn_err(CE_CONT, msg, a1, a2);
	cmn_err(CE_CONT, ".  Resetting SCSI bus\n");
	/* for system monitor user interface stuff */
	cmn_err(CE_ALERT, "!Integral SCSI bus %d reset", dp->d_adap);
#if DEBUG
	idbg_wd93(0);
#endif	/* DEBUG */
	idbg_wd93(1);	/* stubbed if idbg.o not linked in; allows customers
		to get more info about wd93 failures if they want, as well as
		SGI development; should modify someday to dump only the adapter
		we are resetting... */
	wd93_resetbus(dp->d_adap);	/* toggle wd93 bus reset line */
	handle_reset(dp);	/* now clean everything up */
}

/*	set the phase register and start a select & transfer.
	Also release the chip register lock.  This shouldn't fail due
	to a simultaneous re-select, since we should always be connected as
	an initiator when this is called.  May get here with no connected
	channel on occasion; then we just set phase register; usually the
	result of hardware problems, but don't do a cmdabort at this
	point.
*/
static void
wd93_transfer(wd93dev_t *dp, u_char phase)
{
	putreg93(WD93DESTID, (dp->d_consubchan && dp->d_consubchan->sr_ha) ?
	 ((wd93_ha_t *)dp->d_consubchan->sr_ha)->wd_dstid : 0)
	putreg93(WD93PHASE, phase)
	putcmd93(C93SELATNTR)
}


/*	start a transfer going, being sure the destination register
	has the correct direction bit set if we are using a 93A.
	Similar to wd93_transfer, but we set the dest register,
	so we use wd_target.
	The own ID register has to be set correctly for 93A.  It should
	be 0 for all cases except when the cmd bytes are to be transferred
	(startwd93 only ).

	Note that this will sometimes fail because of a pending
	interrupt (or simultaneous reselect).  This should happen only
	when first starting a cmd, since we should be connected as an
	initiator everywhere else.  The cmd is then re-queued at the
	next interrupt (which SHOULD be a re-selection interrupt).
*/
static void
setdest(wd93dev_t *dp, scsi_request_t *sp)
{
	if(!sp)
		wd93_cmdabort(dp, sp,
		    "Spurious wd93 interrupt, no connected channel", 0, 0);
	putreg93(WD93DESTID,((wd93_ha_t *)sp->sr_ha)->wd_dstid)
	putreg93(WD93LUN, sp->sr_lun)
	putcmd93(C93SELATNTR)
}



/*
 * startwd93 - grab the adapter and start I/O for the desired command.
 * Note that the middle portion of this code is inlined in wd93subrel
 * if a check condition occurs.
 */
static void
startwd93(wd93dev_t *dp, scsi_request_t *sp, int acq)
{
	uint len;
	volatile unchar	*data;
	u_char *cmd;
	wd93_ha_t *ssp;

re_acq:
	/* Acquire the host adapter.  If didn't get it, request has been
	 * queued and will be restarted out of wd93release.
	 * acq==0 when doing autorequestsense.
	 * The 1 parameter says it's okay to queue the request.
	 * Last parameter (0) is ignored when ok_to_q is 1.
	 */
	if(acq && !wd93acquire(dp, sp, 1))
		return;

	if(sp->sr_ha_flags & (SR_NEEDNEG_ON|SR_NEEDNEG_OFF)) {
		if(sync_setup(dp, sp, (sp->sr_ha_flags & SR_NEEDNEG_ON), 0)) {
			sp->sr_status = 0;
			goto re_acq;
		}
		sp->sr_status = 0;
	}

	ssp = (wd93_ha_t *)sp->sr_ha;	/* set in wd93acquire!!! */

	/* For wd93command() restarts */
	ssp->wd_flags &= ~WD_RESTART;

	ssp->wd_tentative = 0;
	ssp->wd_bcnt = sp->sr_buflen;
	ssp->wd_buffer = sp->sr_buffer;

	if(sp->sr_flags & SRF_DIR_IN)
		ssp->wd_dstid = sp->sr_target|SCDMA_IN;
	else
		ssp->wd_dstid = sp->sr_target;

	/* this is partly paranois to be sure that MAPBP is only set
 	 * for cases where it should be. */
	if (sp->sr_flags & SRF_MAPBP)
		ssp->wd_flags |= WD_MAPBP;
	else
		ssp->wd_flags &= ~WD_MAPBP;
#if DEBUG
	if(printintr)
		printf("  starting, sr_flags=%x, dstid=%x\n", sp->sr_flags, ssp->wd_dstid);
#endif
	cmd = sp->sr_command;

	if(sp->sr_buflen) {
		if(wd93_dma(dp, sp, 1) <= 0) {
			/* set busy flag so that subrel will work! */
                        ((wd93_ha_t *)sp->sr_ha)->wd_flags |= WD_BUSY;
			wd93subrel(dp, sp, 1);
			return;
		}

		/* There is a bug in the 93 part that causes outstanding
		 * REQ's to be lost when in states ST_UNEX_[RS]DATA
		 * which only happens when we program the chip for
		 * a count less than that in the wd93 cmd block. The
		 * only problem is that we also need to re-negotiate
		 * for nosync mode for this to work....
		 * acq is always set here, because on a chkcond, we would
		 * have already started a command, and so sync would be off.
		*/
		if ((dp->d_flags & D_WD93) &&
		    (dp->d_syncflags[sp->sr_target] & S_SYNC_ON) &&
		    (ssp->wd_xferlen != ssp->wd_bcnt)) {
			ASSERT(acq);
			if(wd93busycnt(dp)) {
				/* need to turn off DMA on some machines */
				(void)wd93dma_flush(ssp->wd_map,
					ssp->wd_dstid&SCDMA_IN, 0);
				sp->sr_ha_flags |= SR_NEEDNEG_OFF;
				wd93notactive(dp, sp, 0);
				wd93release(dp);
				goto re_acq;
			}
			if (sync_setup(dp, sp, 0, 0)) {
				sp->sr_status = 0;
				ASSERT(acq);
				goto re_acq;
			}
			sp->sr_status = 0;
		}
	}
	else {	/* be sure no cnt left from previous cmd */
		putcnt(0, 0, 0);
		ssp->wd_xferlen = 0;
	}

	((wd93_ha_t *)sp->sr_ha)->wd_flags |= WD_BUSY;
	ssp->wd_timeid = timeout(wd93_timeout, sp,
		(wd93mintimeout > sp->sr_timeout) ? wd93mintimeout :
		sp->sr_timeout);

	putreg93(WD93SYNC, dp->d_syncreg[sp->sr_target])

	/*	this allows sending cmds not in groups 0, 1, or 5 successfully;
	*	if D_WD93, it simply assumes cmds in unrecognized
	*	groups are 6 bytes; if device thinks it's longer, you
	*	get an unexpected cmd phase; at which point you can do
	*	a resume SAT. If and when we get a device that needs
	*	this, we'll think about implementing for the plain 93
	*/
	len = sp->sr_cmdlen;
	if(dp->d_flags & (D_WD93A|D_WD93B))
		putreg93(WD93ID, len)

	/*	put the cmd bytes into the chip registers. The address
		register auto increments, so we don't have to reload it
		each time. */
	*dp->d_addr = WD93C1;
	data = dp->d_data;
	if(len > 12) /* program cnt reg right, but extra bytes have to */
		len = 12;	/* be handled in same way as on 93 */
	for( ; len>0; len--) {
		*data = *cmd++;
	}

	/* Start the I/O operation going with a select-and-transfer command.
	 * The interrupt handler drives the state machine from here.  */
	putreg93(WD93PHASE, PH_NOSELECT)
	ssp->wd_selretry = T93SRETRY;	/* number of times to attempt select */
	setdest(dp, sp);
#if defined(EVEREST)
	if (ssp->wd_bcnt)
		wd93_go(ssp->wd_map, ssp->wd_buffer, ssp->wd_dstid & SCDMA_IN);
#endif
}


/*
 * Restart a cmd; called when the SAT cmd is ignored because we tried to
 * issue it after being interrupted (LCI bit gets set).  We need to
 * reschedule the transfer that was just started and service the
 * reselection interrupt.  The AUX_LCI bit will be normally be set (but
 * may not be if the intr was caused by a reselect).  There is some
 * discrepancy between the 93 and 93A documentation here.
 * In the asynchronous case, just queue it up.
 * In the synchronous case, simply wake up the upper half to try again.
 * ALWAYS called at interupt time.  Need to remove the command from the
 * active list as well.
 * The adapter is NOT released, since the re-selecting channel will be
 * processed on return from this routine.
 */
static void
restartcmd(wd93dev_t *dp, scsi_request_t *sp)
{
	wd93_ha_t *ssp = (wd93_ha_t *)sp->sr_ha;

	untimeout( ssp->wd_timeid );
	ssp->wd_timeid = 0;

	/* need to turn off DMA on some machines */
	(void)wd93dma_flush(ssp->wd_map, ssp->wd_dstid&SCDMA_IN, 0);

	/* goes on end of queue, not head, because specially handled
	 * in the auto requestsense code.  If it wasn't, we could have
	 * sequencing problems, and start a new command on the same
	 * target */

	/* these are the queueing lines from wd93acquire */
	if(dp->d_waittail) { /* put on end of queue. */
		((wd93_ha_t *)dp->d_waittail->sr_ha)->wd_link = sp;
		dp->d_waittail = sp;
	}
	else
		dp->d_waithead = dp->d_waittail = sp;

	wd93notactive(dp, sp, 1); /* remove from the active list, but not free */
	((wd93_ha_t *)sp->sr_ha)->wd_flags |= WD_RESTART;
}


/* check to be sure that we don't already have a command active
 * for this target/lun, or that the request doesn't need to be
 * the only active command on the bus, and it's busy.
 */
static
wd93oktostart(wd93dev_t *dp, scsi_request_t *sp)
{
	scsi_request_t *tsp;

	/* if command needs to be the only active command, and other
	 * commands active, can't do it now */
	if((sp->sr_ha_flags & SR_NEEDONLY) && wd93busycnt(dp))
	    return 0;

	for(tsp=dp->d_active; tsp; tsp=((wd93_ha_t *) tsp->sr_ha)->wd_link) {
		if(sp->sr_target == tsp->sr_target &&
			sp->sr_lun == tsp->sr_lun)
			return 0;
	}
	return 1;
}


/*
 * wd93acquire - acquire  the host adapter
 *
 * NOTE: At interrupt time,  this routine may be called to
 * reschedule a request.  This won't deadlock because D_BUSY is set,
 *
 * If no_nextcmd is set, we are dumping or shutting down, so don't
 * queue or wait.
 *
 * NOTE: callers of this routine that don't go through wd93subrel()
 * must call wd93notactive() when done!
 *
 * Anyone who calls this without oktoq set must provide the old spl
 * so that we can properly release d_qlock.
 */
static
wd93acquire(wd93dev_t *dp, scsi_request_t *sp, int oktoq)
{
	while(!no_nextcmd) {
		if(!(dp->d_flags & D_BUSY) && dp->d_ha_list &&
			wd93oktostart(dp, sp))
			break;

		if(sp->sr_ha_flags & SR_NEEDONLY)
			dp->d_flags |= D_ONLY;
		if(oktoq) {
			if(dp->d_waittail) { /* put on end of queue. */
				sp->sr_ha = (void *)dp->d_ha_list;
				dp->d_ha_list = dp->d_ha_list->wd_nextha;
				((wd93_ha_t *) dp->d_waittail->sr_ha)->wd_link = sp;
				dp->d_waittail = sp;
			}
			else {
				sp->sr_ha = (void *)dp->d_ha_list;
				dp->d_ha_list = dp->d_ha_list->wd_nextha;
				dp->d_waithead = dp->d_waittail = sp;
			}
			return(0);
		} else {	/* wait until we can acquire */
			dp->d_sleepcount++;
			sv_wait(&dp->d_sleepsem, PSCSI, &dp->d_qlock, 0);

			/* Caller is already saving ospl, so don't worry
			 * about return value here.
			 */
			(void) mutex_spinlock(&dp->d_qlock);
		}
	}

	if (!sp->sr_ha) {   /* else may be re-acquiring after restartcmd(), or
		* in a few cases after a sync negotiation from wd93acquire */
		sp->sr_ha = (void *)dp->d_ha_list;
		dp->d_ha_list = dp->d_ha_list->wd_nextha;
	}

	((wd93_ha_t *)sp->sr_ha)->wd_link = dp->d_active;
	dp->d_active = sp;
	dp->d_flags |= D_BUSY;
	dp->d_consubchan = sp;

	return(1);
}


/*	Release the host adapter, and start next cmd, if any.
	If action is >0, check to see if a process is waiting
	to be the only active user of the bus.
*/
static void
wd93release(wd93dev_t *dp)
{
	scsi_request_t *sp;
	
	dp->d_flags &= ~D_BUSY;
	dp->d_consubchan = (scsi_request_t *)NULL;

	/* if dumping, shutting down, we don't want other cmds */
	if(no_nextcmd)
		return;

	if(dp->d_sleepcount) {
		dp->d_sleepcount--;
		sv_signal(&dp->d_sleepsem);
	}
	else if(sp=dp->d_waithead) {
		scsi_request_t *tsp, *tbaksp=NULL;

		if((dp->d_flags&D_ONLY) && !wd93busycnt(dp)) {
			/* at least one command wants to be the only active cmd
			 * the bus, usually for sync neg, but also aborts, since
			 * we have no active commands, start one of those first.
			 * if only one, clear the D_ONLY flag; linear search OK
			 * because this is quite rare */
			scsi_request_t *onlysp;
			for(onlysp=sp; onlysp && !(onlysp->sr_ha_flags&SR_NEEDONLY);
				onlysp=((wd93_ha_t *)onlysp->sr_ha)->wd_link)
					tbaksp = onlysp;
			tsp = onlysp;
			ASSERT(tsp); /* or D_ONLY would not be set */
			/* check rest of list */
			if(onlysp) for(onlysp=((wd93_ha_t *)onlysp->sr_ha)->wd_link;
				onlysp && !(onlysp->sr_ha_flags&SR_NEEDONLY);
				onlysp=((wd93_ha_t *)onlysp->sr_ha)->wd_link)
				;
			if(!onlysp)
				dp->d_flags &= ~D_ONLY;
		}
		else for(tsp=sp; tsp && !wd93oktostart(dp, tsp);
			tsp=((wd93_ha_t *)tsp->sr_ha)->wd_link)
			tbaksp = tsp;

		if(tsp) {
			if(!tbaksp)	/* first on list OK */
				dp->d_waithead = ((wd93_ha_t *)tsp->sr_ha)->wd_link;
			else { 		/* remove tsp from middle of list */
				/* I believe I have fixed the problem that could sometimes
				 * get there with sr_ha NULL, but I'll leave the assert for
				 * a while.  There is no safe recovery if it is NULL  */
				ASSERT(tbaksp->sr_ha);
				((wd93_ha_t *)tbaksp->sr_ha)->wd_link = 	
						((wd93_ha_t *)tsp->sr_ha)->wd_link;
			}
			if (tsp == dp->d_waittail)
				dp->d_waittail = tbaksp;
			((wd93_ha_t *)tsp->sr_ha)->wd_link = NULL;
			startwd93(dp, tsp, 1);
		}
		/* else no other cmds can be started (must all be for
		 * targets that already have active cmds) */
	}
}


/*
 * cleanup and release subchannel on wd93 cmd completion
 * also for convenience release the adapter (always except
 * from handle_reset() ).
 */
static void
wd93subrel(wd93dev_t *dp, scsi_request_t *sp, int reladap)
{
	scsi_request_t *sprq;

	if (!(((wd93_ha_t *)sp->sr_ha)->wd_flags & WD_BUSY)) {
		/*
		 * completion intr after a a reset, 'real' notify routine
		 * already called; the wd93release still needs to be
		 * done to possibly start next cmd
		 */
#if wd93_verbose
		printf("subrel, but NOT busy!\n");
#endif
		wd93notactive(dp, sp, 0);
		if(reladap)
			wd93release(dp);
		return;
	}

	if(((wd93_ha_t *)sp->sr_ha)->wd_timeid) {
		untimeout(((wd93_ha_t *)sp->sr_ha)->wd_timeid);
		((wd93_ha_t *)sp->sr_ha)->wd_timeid = 0;
	}
	/* else an error in startdma, before we setup timeout */

	if(sp->sr_status == SC_PARITY)
		cmn_err_tag(11, CE_WARN, "wd93 SCSI Bus=%d ID=%d LUN=%d: SCSI bus parity error\n",
		        sp->sr_ctlr, sp->sr_target, sp->sr_lun);
	else if(sp->sr_status == SC_MEMERR)
		cmn_err(CE_WARN,
			"wd93 SCSI Bus=%d ID=%d LUN=%d: host memory parity error during DMA\n",
			sp->sr_ctlr, sp->sr_target, sp->sr_lun);
	else if(sp->sr_scsi_status == ST_CHECK &&
		sp->sr_senselen && *sp->sr_command != 3) {
		if(wd93_printsense)
			cmn_err_tag(12, CE_NOTE, "wd93 SCSI Bus=%d ID=%d LUN=%d, CMD=0x%x: check condition"
				" start request sense\n", sp->sr_ctlr, sp->sr_target,
				sp->sr_lun, *sp->sr_command);
		sp->sr_sensegotten = -1;	/* in case of failure */
		sprq = wd93rqsubchan[sp->sr_ctlr][sp->sr_target][sp->sr_lun];
		if(!sprq || sprq->sr_spare) {
			/* 4/94; had a debug printf here we no longer get, but
			 * remain paranoid anyway... */
			/* If this happens, we are in trouble.  Somehow an
			 * earlier command got a chkcond, but a second command
			 * for the target got started before the automatic
			 * request sense, and *also* got * a chkcondition.
			 * What we really want to do is stall this command until
			 * the reqsense is done, so we can do a new reqsense...
			 * for right now, fake it, as though the reqsense had
			 * failed; dksc driver will retry, others will report
			 * errors back to user programs, etc.
			*/
			goto skipsense;
		}
		sprq->sr_buffer = sp->sr_sense;
		*sprq->sr_command = 3;
		sprq->sr_command[1] = sp->sr_lun << 5;
		sprq->sr_command[4] = sprq->sr_buflen = sp->sr_senselen;
		dki_dcache_inval(sp->sr_sense, sp->sr_senselen);
		sprq->sr_flags = SRF_DIR_IN | SRF_FLUSH;
		sprq->sr_spare = (void *) sp;
		/* set resid (if any) before we trash the bcnt during
		 * the reqsense. */
		sp->sr_resid = ((wd93_ha_t *)sp->sr_ha)->wd_bcnt;
#if wd93_verbose
printf("wd93subrel: original sp=0x%x ha=0x%x\n",sp,sp->sr_ha);
#endif
		/* use the sr_ha from the original request; when the
		 * orig request completes, it releases the sr_ha, the
		 * one used for the reqsense is never wd93noactive'd. */
		sprq->sr_ha = sp->sr_ha;
		((wd93_ha_t *)sprq->sr_ha)->wd_flags = WD_RQSENS;

		/* NOTE: we are *always* called as the result of an interrupt
		 * when we get here; when called from startwd93 on DMA failures,
		 * we will never do a request sense.
		 */
		wd93notactive(dp, sp, 1);  /* remove orig from active, but
			* don't free, because we are still using the hostadap
			* struct (now in sprq->sr_ha) */
		sp->sr_ha = NULL;	/* be paranoid for anybody looking at sr_ha,
			until this back on the active list */

		/* same code as in wd93acquire */
		((wd93_ha_t *)sprq->sr_ha)->wd_link = dp->d_active;
		dp->d_active = sprq;
		dp->d_consubchan = sprq;

		sprq->sr_scsi_status = sprq->sr_status = 0;
		sprq->sr_sensegotten = 0;
		startwd93(dp, sprq, 0);
		return;
	}
	else if(wd93_printsense > 2 && (sp->sr_scsi_status || sp->sr_status))
	    cmn_err(CE_NOTE,
		"wd93 SCSI %d,%d,%d: ctlrstat=%x, scsistat=%x, scsicmd=%x\n",
		sp->sr_ctlr, sp->sr_target, sp->sr_lun, sp->sr_status,
		sp->sr_scsi_status, *sp->sr_command);

skipsense:
	if(((wd93_ha_t *)sp->sr_ha)->wd_flags & WD_RQSENS)
		/* end reqsense, get original sr_request for notify, etc. */
		sp = wd93_reqsense(dp, sp);

	/* set resid, if any; happens frequently on tapes when
	 * reading with larger blk size than written with; if
	 * sensegotten set, we set resid in the autosense code
	 * above, before wd_bcnt got trashed during sense. */
	if(!sp->sr_sensegotten)
		sp->sr_resid = ((wd93_ha_t *)sp->sr_ha)->wd_bcnt;

	if(no_nextcmd != 1) {
		wd93notactive(dp, sp, 0);  
		if(reladap)
			wd93release(dp);
		nested_spinunlock(&dp->d_qlock);
		(*sp->sr_notify)(sp);
		nested_spinlock(&dp->d_qlock);
	}
	else
		wd93notactive(dp, sp, 1);  /* Remove from Active Q */
}


/* handle the return info, etc. after the reqsense, called only from
 * subrel.
 */
static scsi_request_t *
wd93_reqsense(wd93dev_t *dp, scsi_request_t *sp)
{
	scsi_request_t *sprq;
	wd93_ha_t *ssp = (wd93_ha_t *)sp->sr_ha;

	sprq = (scsi_request_t *)sp->sr_spare;
	sp->sr_spare = NULL;	/* for check if in use in wd93subrel */
	ssp->wd_flags &= ~WD_RQSENS;

	if(sp->sr_status || sp->sr_scsi_status) {
		sprq->sr_sensegotten = -1;
		sprq->sr_status = sp->sr_status; /* so caller knows error */
		sprq->sr_scsi_status = sp->sr_scsi_status;
	}
	else {
		sprq->sr_sensegotten = sp->sr_buflen - ssp->wd_bcnt;
		sprq->sr_scsi_status = 0;
	}

	if (sprq->sr_sensegotten <= 0) {
		/* mark as having done sense, but make len invalid, otherwise
		 * caller won't know orig cmd failed; should rarely happen */
		sprq->sr_sensegotten = -1;

		if(wd93_printsense)
			cmn_err_tag(13, CE_WARN, "wd93 SCSI Bus=%d ID=%d LUN=%d: sense failed"
				"wd93 status %d, scsi status 0x%x\n",
				 sp->sr_ctlr, sp->sr_target, sp->sr_lun,
				 sp->sr_status, sp->sr_scsi_status);
	}
	else if(wd93_printsense) {
		u_char *sense = sprq->sr_sense;
		int slen = sprq->sr_sensegotten;
		cmn_err(CE_CONT, "wd93 SCSI Bus=%d ID=%d LUN=%d: sense key=0x%x (%s)",
			 sp->sr_ctlr, sp->sr_target, sp->sr_lun, sense[2]&0xf,
			scsi_key_msgtab[sense[2]&0xf]);
		if(slen> 12) {
			cmn_err(CE_CONT, " ASC=0x%x", sense[12]);
			if(sense[12] < SC_NUMADDSENSE && scsi_addit_msgtab[sense[12]])
				cmn_err(CE_CONT, " (%s)", scsi_addit_msgtab[sense[12]]);
			if(slen > 13)
				cmn_err(CE_CONT, " ASQ=0x%x", sense[13]);
		}
		if(wd93_printsense > 1) {
			cmn_err(CE_CONT, "\nHex sense data: ");
			while(slen-- > 0)
				cmn_err(CE_CONT, "%x ", *sense++);
		}
		cmn_err(CE_CONT, "\n");
	}

	/* Remove the reqsense from the active list, but don't free, since
	 * we assign the HA struct back to original command */
	wd93notactive(dp, sp, 1);
	sprq->sr_ha = sp->sr_ha;	/* make sr_ha in original cmd valid again */

	/* same code as in wd93acquire - Put the original cmd back on the
	 * active list for normal completion processing.  */
	((wd93_ha_t *)sprq->sr_ha)->wd_link = dp->d_active;
	dp->d_active = sprq;

	dp->d_unitflags[sprq->sr_target][sprq->sr_lun] |= U_HADAEN;
	return sprq;
}


/*	actually do all the work of figuring out what the interrupt
	means and handle it.
*/
static void
handle_intr(wd93dev_t *dp)
{
	u_char state, phase, aux, lun, srcid;
	scsi_request_t *sp;
	wd93_ha_t *ssp;

	/* null if not D_BUSY; probably not correct if a re-select */
	sp = dp->d_consubchan;

	/* can't read anything but aux while CIP set, can't get anything but
	 * aux, cmd, data if BSY set.  Actually see this happen sometimes
	 * with R4400 (either CIP and BSY, or less often, just BSY).  When
	 * that happened on a resel, we didn't issue the restartcmd.  I've
	 * changed that logic, but to prevent other hard to track down bugs,
	 * do it right...  */
	while((aux = getauxstat()) & (AUX_CIP|AUX_BSY))
		;

	if(sp && (((wd93_ha_t *)sp->sr_ha)->wd_flags & WD_BUSY)) {
		if(aux & AUX_LCI)	/* the previous SAT cmd failed due to an
			intr.  queue it to restart later.  Should only happen
			when first starting a cmd, since other times we
			issue an SAT, we should already be connected
			as an initiator. */
			restartcmd(dp, sp);
		else if(aux & AUX_PE)
			sp->sr_status = SC_PARITY;
	}

  	/* Read all the registers BEFORE the status register,
	 * since the chip can re-interrupt after that, possibly
	 * changing the lun/srcid registers; we also get some
	 * benefit from the auto register incr.  The count registers
	 * will be OK since neither we nor the chip will change them.
	 * NOTE: the XC rev of the 93A has a bug that if an
	 * unexpected phase change occured during an SAT, the
	 * incorrect value may be in the phase register,
	 * so don't count on it too much...
	 * Only read data register when reselecting and msgin;
	 * hopefully it won't change after reading the state register;
	 * we don't have much choice. (actual read moved into various
	 * state checks to avoid checking state twice.) */
	lun = getreg93(WD93LUN);
	phase = *dp->d_data;	/* phase follows LUN */
	srcid = getreg93(WD93SRCID);
	state = *dp->d_data;	/* state follows SRCID */

#ifdef DEBUG
	if(printintr) {
		printintr--;
		if(sp) printf("targ=%d wd_fl=%x ", sp->sr_target,
			      ((wd93_ha_t *)sp->sr_ha)->wd_flags);
		printf("\n");
	}
#endif /* DEBUG */

	/* order to handle most common cases first.  reselect/disconnect
	 * is  most common (in r/w), then savedp, then complete.
	 * Move ST_RESELECT further down, because 93's are only
	 * on older slower systems anyway.  Not enough cases for a
	 * jump table, and compilers reorder switches diferently in different
	 * compilers, so make an explict series of if's. */
	if(state == ST_93A_RESEL) {
		dp->d_tdata = getdata();
		/* handle the reselect on 93A; has one less phase
		and interrupt.  Any pending SAT ignored due to a reselect;
		LCI may not be set, due to a problem in the chip.  The 'sp'
		in the test is the CURRENT sp, not the one reselecting; if
		it isn't in disconnected state, then the cmd must have been
		ignored (not sent to target), so set it up for restart. */
		if(!(srcid & 0x8))
			wd93_cmdabort(dp, sp, "reselect without ID", 0, 0);
		else {
		    if(sp && !(aux & AUX_LCI) && 
			((((wd93_ha_t *)sp->sr_ha)->wd_flags &
			(WD_BUSY|WD_DISCONNECT)) == WD_BUSY))
			restartcmd(dp, sp);	/* else handled above */
		    if(!(sp=wd93findchan(dp, srcid&SCTMSK, dp->d_tdata&SCLMSK)))
			return;
		    wd93continue(dp, sp, PH_IDENTRECV);
		}
	}
	else if(state == ST_DISCONNECT) {
		if (phase == PH_DISCONNECT && sp) {
			/* target is disconnecting; Save the data pointers, but
			 * set the tentative flag, because we don't know if
			 * this transfer is really valid yet.  It may be the
			 * last in a command, but it may also be a precursor
			 * to a restore pointers. */
			save_datap(dp, sp, 1);
			((wd93_ha_t *)sp->sr_ha)->wd_flags |= WD_DISCONNECT;
			wd93release(dp);
		}
		else {
			u_char cmd = getreg93(WD93CMD);
			if(cmd != C93DISC || phase != PH_NOSELECT)
				wd93_cmdabort(dp, sp,
				   "illegal disconnection interrupt: phase %x",
					phase, 0);
		}
	}
	else if(state == ST_SAVEDP) {
		/*
		 * Save data pointer requested by target -
		 * he's disconnecting to reselect later.
		 * Save pointers, then wait for next phase.
		 * Note that the channel is still busy, and virtually
		 * connected to the target.
		 */
		save_datap(dp, sp, 0);
		wd93_transfer(dp, PH_SAVEDP);	/* resume to next phase */
	}
	else if(state == ST_SATOK) { /* Command completed successfully.  */
		/* not really saving the ptrs, just updating xferlen, tlen, and
		 * doing dma stuff; rather than doing it all in line.  */
		save_datap(dp, sp, 0);
		sp->sr_scsi_status = lun;	/* target status byte */
		wd93subrel(dp, sp, 1); /* Release the subchannel. */
	}
	else if(state == ST_RESELECT) {	/* 93, or 93A without advanced features;
		SAT ignored due to a reselect; LCI may not be set, due to
		a problem in the chip. */
		if(phase == PH_NOSELECT && sp &&
			(((wd93_ha_t *)sp->sr_ha)->wd_flags & WD_BUSY) &&
			!(aux & AUX_LCI))	/* else handled above */
			restartcmd(dp, sp);
		/* no LUN till msg in phase, can't set d_consubchan.
		 * Wait for ID message on next interrupt. This occasionally
		 * fails due to an interrupt, but the chip seems to handle
		 * it OK (or perhaps the device is re-trying the selection.
		 * There isn't anything to reschedule if it fails... Don't
		 * want a wd93_transfer here, just update the phase
		 * register.  */
		putreg93(WD93PHASE, PH_RESELECT)
	}
	else if(state == ST_REQ_SMESGIN) {
		dp->d_tdata = getdata();
	    if(phase==PH_RESELECT || (phase==0 &&
		(dp->d_flags&(D_WD93A|D_WD93B)))) {
		    /* We received the identify message after reselection.
		     * We can restart the select and xfer at this point. */
		    if(!(sp=wd93findchan(dp, srcid&SCTMSK, dp->d_tdata&SCLMSK)))
			    return;
		    wd93continue(dp, sp, PH_RESELECT);
	    }
	    else {
		if(unex_info(dp, phase, state) > 0) {
		    /* not handling some state that we should be,
		     * or a device that isn't following protocol */
		    wd93_cmdabort(dp, sp, "unexpected message in %x, phase %x", 
			dp->d_tdata, phase);
		}
		/* else must have had unsuccessful target sync; should never
		 * from here...  If it did, cleanup already done. */
	    }
	}
	else if(state == ST_TIMEOUT) {
		ssp = (wd93_ha_t *)sp->sr_ha;
		if(phase != PH_NOSELECT)
			wd93_cmdabort(dp, sp, "Hardware error", 0, 0);
		else if(--ssp->wd_selretry <= 0) {
			/* Timed out attempting to select the device.
			 * Try the desired number of times before giving up.  */
			sp->sr_status = SC_TIMEOUT;
			(void) wd93dma_flush(ssp->wd_map,
					     ssp->wd_dstid & SCDMA_IN, 0);
			wd93subrel(dp, sp, 1);
		}
		else /* Restart the select and transfer.  The phase and ID */
			setdest(dp, sp); /* registers are still correct */

	}
	else if(state == ST_UNEXPDISC) {
		/* device disconnected unexpectedly.  This happens sometimes
		when a device with removable media (like a floppy) has the
		media removed during i/o */
		ssp = (wd93_ha_t *)sp->sr_ha;
		if(sp) /* need to turn off DMA on some machines */
		    (void)wd93dma_flush(ssp->wd_map, ssp->wd_dstid&SCDMA_IN, 0);
		putcmd93(C93DISC);  /* used to use wd93cmd_lci, but intr lost */
		wd93subrel(dp, sp, 1);
	}
	else if(state == ST_RESET)
		handle_reset(dp);
	else if(state == ST_INCORR_DATA)	/* another 93A bug... */
		wd93_transfer(dp, phase); /* resume SAT */
	else if(state == ST_PARITY || state == ST_PARITY_ATN) {
		if(sp)
			sp->sr_status = SC_PARITY;
		/* and continue the phase... Had to be a msg in if ATN,
		 * otherwise a read. IDENTRECV also does a NACK for us; which
		 * is needed, since ACK is left asserted to prevent the
		 * transfer from continuing.  We might want to put some
		 * kind of counter here and abort; on the other hand the
		 * transfer will probably time out,  since if every byte
		 * gets a parity error, it drops to about 1.1Kbytes/sec */
		wd93_transfer(dp, PH_IDENTRECV);
	}
	else if(sp && state == ST_TR_DATAOUT || state == ST_TR_DATAIN
		|| state == ST_TR_STATIN || state == ST_TR_MSGIN) {
		/* happens on 93, and 93A if not in advanced mode, when 
		 * a group 2,3,4,6, or 7 cmd is issued with more than 6
		 * cmd bytes, and we transferred the remaining bytes manually.
		 * Simply continue to the next phase.
		 * Have to re-put cnt and phase in case interrupt was
		 * pending at trinfo, and they were ignored at the
		 * PH_CDB_6 intr */
		uint len;
		ssp = (wd93_ha_t *)sp->sr_ha;
		len = ssp->wd_xferlen;
		putcnt((u_char)(len>>16), (u_char)(len>>8), (u_char)len)
		putreg93(WD93PHASE, PH_SAVEDP)
		setdest(dp, sp);
	}
	else {	/* default */
		if(state == ST_UNEX_SMESGIN)
		    dp->d_tdata = getdata();
		if((state & 0x48) != 0x48 || (unex_info(dp, phase, state)>0)) {
		/* we aren't handling some state that we should be... */
		if((state&0xf) == 0xf)
		    wd93_cmdabort(dp, sp, "Unexpected msg in %x, phase %x",
			dp->d_tdata, phase);
		else {
		    if(phase == PH_DATA && (state == ST_UNEX_RDATA ||
			state == ST_UNEX_SDATA)) {
			wd93_cmdabort(dp, sp,
		    "Too much data %s (probable SCSI bus cabling problem)",
			(long)(state==ST_UNEX_SDATA?"requested":"sent"), 0);
		    }
		    else
			wd93_cmdabort(dp, sp,
			    "Unexpected info phase %x, state %x",
			    phase, state);
		}
	    }
	}
}


#if DEBUG || wd93_verbose
static void
_wd93print_active_queue(wd93dev_t *dp)
{
	struct scsi_request *sp;

	printf("active queue: ");
	sp = dp->d_active;
	do {
		printf("sc%d,%d %d sp=%x ", sp->sr_ctlr, sp->sr_target, sp);
	}
	while (sp = ((wd93_ha_t *) sp->sr_ha)->wd_link);
	printf("\n");
}
#endif
#if wd93_verbose
#define wd93print_active_queue(a) _wd93print_active_queue(a) 
#else
#define wd93print_active_queue(a)
#endif

void
wd93resetdone(wd93dev_t *dp)
{
	wd93release(dp);	/* done with the adapter */
}

/*
 * reset all software state after a reset (interrupt or internal)
 * This is normally called from wd93_cmdabort() rather than as the
 * result of an interrupt.  If called as direct result of an 
 * interrupt, it was probably from an externally caused reset,
 * such powering on or off a wd93 device on the bus.
 *
 * Release all busy subchannels associated with
 * an adapter since we have reset it's SCSI bus.
 * If a target's not busy, it's queued waiting for the
 * adapter so leave it alone.
 * Setup to re-enable sync mode on devices that had it also.
 * If a callback routine tries to do another cmd (from subrel),
 * it will block or be queued in wd93command() since we haven't yet
 * released the adapter. 
 */
static void
handle_reset(wd93dev_t *dp)
{
	scsi_request_t *sp;
	int t;
	extern unsigned wd93reset_delay;
	wd93resets++;

	wd93reset(dp, 0);	/* reset advanced features, etc */
	dp->d_flags |= D_BUSY;	/* was cleared in wd93reset() */

	if(dp->d_consubchan) /* need to turn off DMA on some machines */
	{
		wd93_ha_t *wd = dp->d_consubchan->sr_ha;
		(void) wd93dma_flush(wd->wd_map, wd->wd_dstid & SCDMA_IN, 0);
		dp->d_consubchan = NULL;
	}

	while(sp = dp->d_active) {
		wd93print_active_queue(dp);
		if (sp->sr_status == 0)
			sp->sr_status = SC_ATTN;
		if(dp->d_syncflags[sp->sr_target] & S_SYNC_ON)
			sp->sr_ha_flags |= SR_NEEDNEG_ON;
		wd93subrel(dp, sp, 0);
	}

	/* for every possible target/lun, if it is in sync mode, clear
	 * it; this has to be done whether or not any commands are active,
	 * because we have to assume the target saw the reset, and we will
	 * run into the infamous WD bug of hung waiting for more reqs if we
	 * leave the chip programmed for sync mode on the next data
	 * tranfer.  clear NOSYNC also, if sync was on, because it probably
	 * indicates a devscsi open on a device that was already in sync mode;
	 * want sync renogiated in that case.
	 */
	for(t=0; t< SC_MAXTARG; t++)
	    if(dp->d_syncflags[t] & S_SYNC_ON) {
		    dp->d_syncflags[t] &= ~(S_SYNC_ON|S_NOSYNC);
		    dp->d_syncreg[t] = 0x20;
	    }
	untimeout(dp->d_tid);
	dp->d_tid = 0;
	if(no_nextcmd) {
		DELAY(wd93reset_delay*10000);
		wd93release(dp);	/* done with the adapter */
	}
	else /* don't hold off other interrupts */
		dp->d_tid = timeout(wd93resetdone, dp, wd93reset_delay);
}


/* handle target initiated sync negotiations.  First byte
 * handled by ack_msgin.  NOTE: this won't work on the 93,
 * only on the 93A.  The 93 never gives us the interrupt
 * for the unexpected msgin (although it does raise ACK)...
*/
static
handle_extmsgin(wd93dev_t *dp, scsi_request_t *sp)
{
	u_char msgin[255];
	wd93_ha_t *ssp;
	int i;

	if(!sp)	/* shouldn't be here if not connected! */
		return 1;
	ssp = (wd93_ha_t *)sp->sr_ha;

	/* WD hasn't yet even raised ack for the msg; this gets the msg
	 * byte acked; need to turn off dma first, with no data transfered.
	 * Ask for 2 bytes, since at a minimum, we get '1',len,1st byte. */
	putcnt(0,0,0);	/* reset counter */
	wd93dma_flush(ssp->wd_map, ssp->wd_dstid&SCDMA_IN, 0);
	ack_msgin(dp);
	*msgin = 1;
	if(do_trinfo(dp, &msgin[1], -2, 10000))
		return 1;	/* not much we can do... */
	(void)wait_scintr(dp, 1000);	/* should be tranpause intr */
	/* should cause an intr, with state != pause */
	wd93cmd_lci(dp, C93NACK);
	(void)wait_scintr(dp, 1000);

	if(msgin[1] > 1)
		i = do_trinfo(dp, &msgin[3], -(msgin[1]-1), 10000);

	wd93cmd_lci(dp, C93ATN); /* go to msg out before last byte nak'ed */
	wd93cmd_lci(dp, C93NACK);
	i = wait_scintr(dp, 1000);

	if(msgin[2] != 1) {	/* not sync */
		msgin[0] = 7;	/* message reject */
		i = do_trinfo(dp, msgin, 1, 10000);
		cmn_err(CE_WARN,
			"wd93 SCSI Bus=%d ID=%d LUN=%d: Unexpected extended msgin type %x, len %x\n",
			sp->sr_ctlr, sp->sr_target, sp->sr_lun, msgin[2], msgin[1]);
		if(i)
			return 1;	/* send reject didn't work */
	}
	else {
		/* accept or reject the sync negotiation, based on
		 * wd93_syncenable.  The target should not go to a cmd
		 * phase in sync_setup, so we should be able to simply
		 * continue the original command afterwards */
		if(sync_setup(dp, sp,
			wd93_syncenable[sp->sr_ctlr]&(1<<sp->sr_target),
			((unsigned)msgin[3]<<8)|((unsigned)msgin[4]<<16)|1))
			return -1;	/* unique value */
		if(dp->d_syncflags[sp->sr_target] & S_SYNC_ON)
			dp->d_syncflags[sp->sr_target] |= S_TARGSYNC;
	}

	if(ssp->wd_bcnt) /* must reprogram count, and re-program dma */
		i = wd93_dma(dp, sp, 0) <= 0 ? 1 : 0;
	else
		i = 0;
	if(!i) { /* reprogram dma, reset phase, etc.
		  * set WD_DISCONNECT for test in wd93continue */
		ssp->wd_flags |= WD_DISCONNECT;
		wd93continue(dp, sp, PH_IDENTRECV);
	}
	/* else will do cmdabort on return */
	return i;
}


/* we had an unexpected information phase; return 1 if caller
 * should do a wd93_cmdabort; 0 if OK; -1 if error, but clean
 * already done. */
static
unex_info(wd93dev_t *dp, u_char phase, u_char state)
{
	scsi_request_t *sp = dp->d_consubchan;
	wd93_ha_t *ssp;
	int i;

	if(sp) ssp = (wd93_ha_t *)sp->sr_ha;

	if(state == ST_UNEX_SSTATUS) {
	    if((phase >= PH_CDB_6) && (phase <= PH_IDENTRECV)) {
	/*	Data transfer command exited unexpectedly, this happens
		mostly with things like request sense, inquiry, and
		mode sense, where we ask for more data than this
		particular device returns.  Continue S&T at
		get status phase (by forcing transfer cnt to 0). The next phase
		SHOULD be PH_COMPLETE/SATOK. */
		if(sp && (ssp->wd_flags & WD_BUSY))
			/* update tlen, etc, so we can return resid at SATOK */
			save_datap(dp, sp, 0);
		putcnt(0, 0, 0)	/* so wd accepts new phase */
		wd93_transfer(dp, PH_DATA);
	    }
	    else {
		/* we have seen this on drives with 'broken' firmware;
		 * it could also happen on some bus problems.  By
		 * continuing the phase, we may get more info in 
		 * some cases (we certainly did on the bad drives).
		 */
		putreg93(WD93PHASE, PH_SAVEDP)
		setdest(dp, sp);
	    }
	}
	else if(state == ST_UNEX_SMESGIN) {
		if((phase == PH_DATA || phase == PH_IDENTRECV) &&
			(dp->d_tdata == 2 || dp->d_tdata == 4)) {
			/* target is disconnecting, continue at next phase
			 * save ptrs and flush dma; note this is an abornmal
			 * case for 93A, normal for 93; usually the PH_SAVEDP
			 * code in handle_intr is executed on a disconnect for
			 * the 93A. */
			if(sp) {
				if(dp->d_tdata == 2)
					save_datap(dp, sp, 0);
				else
					wd93dma_flush(ssp->wd_map, ssp->wd_dstid&SCDMA_IN, 0);
			}
			wd93_transfer(dp, PH_SAVEDP);
		}
		else if(dp->d_tdata == 1)	/* extended msg */
			return handle_extmsgin(dp, sp);
		else if(sp && dp->d_tdata == 3) {
			/*
			 * Restore Pointers; WD hasn't yet raised ack for the
			 * msg; this gets the msg byte acked; need to turn
			 * off dma first, with no data transfered.
			 */
			ssp->wd_bcnt += ssp->wd_tentative;
			ssp->wd_buffer -= ssp->wd_tentative;

			(void)wd93dma_flush(ssp->wd_map, ssp->wd_dstid&SCDMA_IN, 0);
			ack_msgin(dp);
			/*
			 * For some reason, the WD needs longer to see this
			 * transition than for an xtmsgin, so put a longer
			 * delay here.
			 */
			(void)wait_scintr(dp, 200);
			putcnt(0,0,0);

			/* Lifted from wd93continue */
			putreg93(WD93SYNC, dp->d_syncreg[sp->sr_target] )
			if (ssp->wd_bcnt && wd93_dma(dp, sp, 1) <= 0)
				return 0;
			putreg93(WD93PHASE, PH_IDENTRECV)
			setdest(dp, sp);
#if defined(EVEREST)
			if (ssp->wd_bcnt)
				wd93_go(ssp->wd_map, ssp->wd_buffer,
				        ssp->wd_dstid & SCDMA_IN);
#endif
			return 0;
		}
		else if((dp->d_flags & (D_WD93A|D_WD93B)) && phase != PH_NOSELECT) {
		/* 	jltb5 says could be a parity error and an
			trinfo cmd should be issued to get the data byte;
			will either get the message or a 43 intr indicating
			the parity error; not getting correct state due to
			bug in XC rev of 93A.  Go to phase identrecv so we
			can get the data or msg.  */
			wd93_transfer(dp, PH_IDENTRECV);
		}
		else if(sp && phase == PH_NOSELECT && dp->d_tdata == 0x80 &&
			(((wd93_ha_t *)sp->sr_ha)->wd_flags & WD_BUSY)) {
			/* See this sometimes on devices with a LONG reselection
			 * sequence, as with Sony CDROM, where from start of arb
			 * to targ+init ID takes ~55 usecs, when they reselect at
			 * the 'same' time we start a new cmd to a different device.
			 * Chip doesn't give us our 'normal' indication in this case. */
			restartcmd(dp, sp);	/* resched 'interrupted' cmd */
			phase = getreg93(WD93SRCID);
			if(!(sp=wd93findchan(dp, phase&SCTMSK, dp->d_tdata&SCLMSK)))
				return 0;	/* abort already done */
			if(((wd93_ha_t *)sp->sr_ha)->wd_flags & WD_DISCONNECT) 
				wd93continue(dp, sp, PH_SAVEDP);
			else
				return 1; /* if sp null */
		}
		else
			return 1;
	}
	else if(sp && state == ST_UNEX_SDATA &&	/* long read */
		(phase == PH_DATA || phase == PH_IDENTRECV) &&
		(ssp->wd_dstid&SCDMA_IN) ||
		(state == ST_UNEX_RDATA &&	/* long write */
		(phase == PH_DATA || phase == PH_IDENTRECV) &&
		!(ssp->wd_dstid&SCDMA_IN))) {
		/* Continue a transfer in which we couldn't program the dma map
		 * for a large enough transfer for the requested command.
		 * We are already selected in this case, so we just
		 * continue the transfer.  93A wants IDENTRECV,
		 * 93 wants RESELECT...  */
		save_datap(dp, sp, 0); /* setup data pointers for new xfer */

		if(ssp->wd_bcnt) { /* Continue at the data xfer phase */
			if((i=wd93_dma(dp, sp, 1)) <= 0)
				return i!=0; /* abort on return if not already done */
			if(dp->d_flags & (D_WD93A|D_WD93B)) {
				putreg93(WD93PHASE, PH_IDENTRECV)
			}
			else
				putreg93(WD93PHASE, PH_RESELECT)
			setdest(dp, sp);
#if defined(EVEREST)
			wd93_go(ssp->wd_map, ssp->wd_buffer,
				        ssp->wd_dstid & SCDMA_IN);
#endif
			return 0;
		}
		else { /* no more data to transfer, but target still thinks
			so.  Mainly happens due to device firmware bugs.
			This also happens with devscsi programs if the user
			has the cmd format wrong, or when driver writers make a
			mistake.  */
			return 1;
		}
	}
	else if(sp && state == ST_UNEX_CMDPH) {
		if(phase == PH_SELECT) {
			/* apple writer II SC, goes from select to cmdphase;
			 * it doesn't support disconnect/reselect */
			putreg93(WD93PHASE, 0x20)
			setdest(dp, sp);
		}
		else if(phase == PH_CDB_6 && sp && sp->sr_cmdlen > 6) {
			/* happens on 93, and 93A if not in advanced mode, when 
			 * a group 2,3,4,6, or 7 cmd is issued with more than 6
			 * cmd bytes.  groups 0,1, and 5 are all the chip knows
			 * for sure, others are assumed to be 6 bytes.   So
			 * transfer the next N bytes of the cmd. trinfo trashes
			 * chip cnt, so restore it afterwords.  allow for 'bad' lens
			 * in known groups also, by figuring out just where we are.
			 * We should get an ST_TR* interrupt after this.  Because
			 * some devices might not connect, tell trinfo to wait
			 * up to the entire spec'ed timeout... Toshiba CDROM
			 * has this problem today. */
			uint len = ssp->wd_xferlen;
			if(do_trinfo(dp, &sp->sr_command[6],
				sp->sr_cmdlen-(phase-PH_CDB_START),
				sp->sr_timeout*(1000000/HZ)))
				return 1;
			putcnt((u_char)(len>>16), (u_char)(len>>8), (u_char)len)
			putreg93(WD93PHASE, PH_SAVEDP)
			setdest(dp, sp);
		}
	}
	else /* This is either the result of bad hardware,
		 * or a programming error here or in the controller. */
		return 1;
	return 0;
}

/*	we are continuing a transfer after being re-selected.  A separate
	routine because the 93A doesn't need the intermediate step
	of resetting the phase register and waiting for another
	interrupt.
*/
static void
wd93continue(wd93dev_t *dp, scsi_request_t *sp, u_char phase)
{
	wd93_ha_t *ssp;
	if(sp) ssp = (wd93_ha_t *)sp->sr_ha;

	if(sp == NULL || !(ssp->wd_flags & WD_DISCONNECT))
	{
		/* Didn't expect reselection, since we don't think we're
		 * talking to the device.  Blow off the reselection.  */
		wd93_cmdabort(dp, sp, "unexpected reselection", 0, 0);
		return;
	}
	ssp->wd_flags &= ~WD_DISCONNECT;
	dp->d_consubchan = sp;
	putreg93(WD93SYNC, dp->d_syncreg[sp->sr_target])

	if(ssp->wd_bcnt && wd93_dma(dp, sp, 0) <= 0)
		return;

	putreg93(WD93PHASE, phase)
	/* Continue at the next phase.  LCI should never be set here,
		since are are already in the reselect phase! */
	setdest(dp, sp);
#if defined(EVEREST)
	if(ssp->wd_bcnt)
		wd93_go(ssp->wd_map, ssp->wd_buffer,
                        ssp->wd_dstid & SCDMA_IN);
#endif
}

/*
 * save_datap -
 * Remember where we are so we can pick up where we left off when we are
 * reselected.  Also used to flush dma and update tlen and xferlen when cmds
 * done.  The tentative parameter is used to support devices that disconnect
 * without a SAVE DATA POINTERS at the end of the last data transfer phase.
 * If tentative is 1, that means that we are calling from disconnect.
 */
static void
save_datap(wd93dev_t *dp, scsi_request_t *sp, int tentative)
{
	uint count_remain;
	uint count_xferd;
	u_char rhi, rmd;
	wd93_ha_t *ssp;

	if(!sp)
		wd93_cmdabort(dp, sp,
		    "Spurious wd93 interrupt, no connected channel", 0, 0);

	ssp = (wd93_ha_t *) sp->sr_ha;
	if(ssp->wd_flags & WD_SAVEDP)
		return;	/* already done */

	ssp->wd_flags |= WD_SAVEDP;
	ssp->wd_tentative = 0;

	/* Don't bother updating pointers if there wasn't any data transfer. */
	if(ssp->wd_xferlen) {
		getcnt(rhi, rmd, count_remain)
		count_remain += ((uint)rmd)<< 8;
		count_remain += ((uint)rhi)<< 16;

		/* if count_remain == wd_xferlen, then the device disconnected
		   immediately after the command was issued, without doing any
		   data transfer */
		count_xferd = ssp->wd_xferlen - count_remain;
		if(count_xferd) {
			ssp->wd_bcnt -= count_xferd;
			ssp->wd_buffer += count_xferd;
			ssp->wd_xferlen = count_remain;
			if (tentative)
				ssp->wd_tentative = count_xferd;
		}
	}
	else
		count_xferd = 0;

	/* Flush the DMA channel, and check for parity error; D_MAPRETAINED
	 * count of bytes transfered, when writing. */
	if(wd93dma_flush(ssp->wd_map, ssp->wd_dstid&SCDMA_IN, count_xferd))
		sp->sr_status = SC_MEMERR;	/* dma parity error? */
}


/*	Select a device, and wait for the selection interrupt.
	If we don't get the right phases on the interrupts, then
	some other device tried to re-select, or the WD was reset.
	If it was a reselect, we lost the interrupt, but all the
	drives so far will retry several times before giving up
	on a reselect.  Trying to fake the interrupt at this
	point leads to assorted problems (even if we pass the
	info back up and retry at a higher level).
*/
static
do_select(wd93dev_t *dp)
{
	uint iaux;

	putcmd93(C93SELATN)

	if((iaux = wait_scintr(dp, 250000)) & INTR_ERR)
		return SC_TIMEOUT;
	else if(iaux != ST_SELECT) {
		if(iaux == ST_REQ_SMESGOUT)
			return 0;	/* shouldn't happen... */
		return SC_HARDERR;
	}
	if((iaux = wait_scintr(dp, 5000)) & INTR_ERR)
		return SC_TIMEOUT;
	if(iaux != ST_REQ_SMESGOUT)
		return SC_HARDERR;
	return 0;
}


/*	wait until chip interrupts, or 'timer' elapses.  then
	clear the interrupt and return the status.  returns 
	status OR'ed with INTR_ERR on timeouts.
	If the aux status register shows the cmd was ignored,
	it clears the interrupt condition and re-submits the
	command. This is a form of polling for the wd93 interrupt.
	if aux register shows the cmd was ignored,
	it clears the interrupt condition and re-submits the
	command.

	Note that we probably allow twice the usecs spec'ed, due to
	instructions that are used, but be generous.
*/
static uint
wait_scintr(wd93dev_t *dp, uint usecs)
{
	u_char state;
	int wait;

	/* wait till WDC isn't busy so registers are accessible */
again:
	for(wait=0; wait<usecs && wd93busy(); wait++)
		DELAY(1);
	if(((state=getauxstat()) & AUX_LCI) && wait < usecs) {
		if(state & AUX_INT) {
			u_char cmd = getreg93(WD93CMD);
			state = getstat();
			putcmd93(cmd)	/* re-issue it */
			goto again;
		}
	}

	usecs /= 8;
	while(wait++<usecs) {
		ifintclear( return (uint)state; )
		DELAY(8);	/* fast polling is a problem for 93 */
	}

	state = getstat();
	return  state | INTR_ERR;	/* timed out waiting */
}


/*	Do dma_map call, and handle case where len returned is 0, meaning that
	either some device disconnected on a non-word boundary, or the initial
	address passed in was not properly aligned.
	If buffer is not mapped, then use the dma_mapbp routine instead.
	Returns 0 on errors, otherwise length mapped.

	If new is 0, we are continuing a transfer that was setup earlier.
	This only matters if D_MAPRETAINED is set, then we increment the map in
	wd93dma_flush, rather than re-programming it.  Still need to put the
	cnt in the chip and tell the HPC which maps to use.

	Also note that we need to be sure that we never try to program the
	24 bit counter in the WD chip to > 16 MB-4, so it won't overflow.
	None of the machines dma_map* routines do this today, but to be
	paranoid about future machines, we drop the count here, if needed.
*/
static int
wd93_dma(wd93dev_t *dp, scsi_request_t *sp, int new)
{
	uint len;
	wd93_ha_t *ssp = (wd93_ha_t *) sp->sr_ha;

	/* if xlen==0, we must have completed the first part of a long
	 * transfer, so we need to re-map.  This is normally handled
	 * in unex_info, but if the device disconnects at just the
	 * 'wrong' point, we can get here with xlen == 0. */
	if(!new && ssp->wd_xferlen && (dp->d_flags&D_MAPRETAIN)) {
		if((__psunsigned_t)ssp->wd_buffer&3)
			/* have to handle disconnects not on 32 bit boundary
			 * here, since we don't remap! */
			len = 0;
		else
			len = ssp->wd_xferlen;
	}
	else {
		/* don't want mod 16 Mbytes in 24 bit WD count regs!
		 * do it before dma_map*() so that we don't waste mapping registers
		 * and time figuring out addresses that we can't use; 0xC instead of
		 * 0xF so we don't have alignment problems on remap. */
		len = ssp->wd_bcnt > 0xfffffc ? 0xfffffc : ssp->wd_bcnt;
		if(ssp->wd_flags&WD_MAPBP) {
			len = dma_mapbp(ssp->wd_map, sp->sr_bp, len);
		} else {
#ifdef	IP20
			/* need to indicate direction somehow, and IP20 doesn't
			 * use index until after the dma starts...  Ugh.
			 * Fortunately, only SCSI uses dma_map() on IP20 */
			ssp->wd_map->dma_index = ssp->wd_dstid&SCDMA_IN;
#endif	/* IP20 */
			len = dma_map(ssp->wd_map, (caddr_t)ssp->wd_buffer, len);
		}
		ssp->wd_xferlen = len;
	}
	putcnt((u_char)(len>>16), (u_char)(len>>8), (u_char)len)

	if(len == 0) {	/* oops, not aligned right */
		sp->sr_status = SC_HARDERR;
		if(sp->sr_buflen == ssp->wd_bcnt) {
			/* initial address bad.  This can only happen from
			 * startwd93, so no bus reset is required.  (well,
			 * theoretically, it could happen after a disc/reconnect
			 * before any i/o, but the address would have been
			 * caught the first time)  */
			cmn_err(CE_WARN, "wd93 SCSI Bus=%d ID=%d LUN=%d:  I/O address %x not "
				"correctly aligned, can't DMA\n",
				sp->sr_ctlr, sp->sr_target, sp->sr_lun, sp->sr_buffer);
			return -1; /* no abort */
		}
		else
			wd93_cmdabort(dp, sp,
				"disconnected on non-word boundary (addr=%x, 0x%x left), can't DMA",
				(long)ssp->wd_buffer, ssp->wd_bcnt);
	}
	else if((int)len>0) {	/* else dma_mapbp failure... */
		/* this works even for the MAPBP case, because xferaddr is
		 * initially NULL, but still gets incremented on disconnects
		 * (in save_datap()), and wd93_go for all current machines
		 * only needs (at most) the offset within a page */
#if !defined(EVEREST)
		wd93_go(ssp->wd_map, ssp->wd_buffer,
			ssp->wd_dstid&SCDMA_IN);
#endif
		ssp->wd_flags &= ~WD_SAVEDP;	/* need to save at next disc; also
			* causes a flush even if no data transferred for
			* the machines that need it. */
	}
	return len;
}


/* issue a cmd repeatedly if LCI bit is set.  This is mostly used as
 * part of sync negotiations.
*/
static void
wd93cmd_lci(wd93dev_t *dp, unchar cmd)
{
	int i = 0;

	while(wd93busy())	/* shouldn't be normally */
		;
	do {	/* negate the ack */
		/* on 2nd thru nth clear possible intr */
		volatile u_char state;
		if(i++) ifintclear(; );
		putcmd93(cmd)
		while(wd93busy())
			;
	} while(getauxstat() & AUX_LCI);
}

/* given a dp, target, and lun, find the correct sp.
 * Older kernels found this via the d_subchan array
*/
static scsi_request_t *
wd93findchan(wd93dev_t *dp, u_char t, u_char l)
{
	scsi_request_t *sp;

	for(sp=dp->d_active; sp; sp=((wd93_ha_t *)sp->sr_ha)->wd_link)
		if(sp->sr_target == t && sp->sr_lun == l)
			return sp;
	wd93_cmdabort(dp, NULL, "ID=%d LUN=%d not found in active list", t, l);
	return NULL;
}


/*
 * Remove cmd from the active list.
 *	rel= 0 removes the driver/HA struct from the active list
 *		and puts HA struct on the free list
 *	rel= 1 removes the driver/HA struct from the active list only
 *
 * Note that wd93notactive should be called before wd93release otherwise
 *   you can get in a deadlock phenomenon. 
 */
static void
wd93notactive(wd93dev_t *dp, scsi_request_t *sp, uint rel)
{
	scsi_request_t *tsp;
	scsi_request_t *lastsp;

	/* Remove sp/HA from active list */
	((wd93_ha_t *)sp->sr_ha)->wd_flags &=
		~(WD_BUSY|WD_DISCONNECT|WD_SAVEDP|WD_RESTART);

	if (sp == dp->d_active)
		dp->d_active = ((wd93_ha_t *)sp->sr_ha)->wd_link;
	else {
		tsp = dp->d_active;
		while (tsp != sp) {
			lastsp = tsp;
			tsp = ((wd93_ha_t *) tsp->sr_ha)->wd_link;
			ASSERT(tsp != NULL);
		}
		((wd93_ha_t *)lastsp->sr_ha)->wd_link = 
			((wd93_ha_t *) sp->sr_ha)->wd_link;
	}
	((wd93_ha_t *)sp->sr_ha)->wd_link = NULL;

	/* Put HA struct back on free list, and clear sr_ha */
	if(!rel) {
		((wd93_ha_t *)sp->sr_ha)->wd_nextha = dp->d_ha_list;
		dp->d_ha_list = (wd93_ha_t *) sp->sr_ha;
		sp->sr_ha = NULL;
	}
}
