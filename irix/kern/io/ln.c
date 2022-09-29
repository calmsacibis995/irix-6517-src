/*
 * File: SN0net Memory --> memory driver
 *
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


#define	_LN_DEBUG	0
#define	_LN_DEBUG_INT	0

#include "sys/debug.h"
#include "sys/types.h"
#include "sys/uio.h"
#include "sys/errno.h"
#include "sys/poll.h"
#include "sys/ddi.h"
#include "sys/pda.h"
#include "sys/pfdat.h"
#include "sys/mips_addrspace.h"
#include "sys/cmn_err.h"
#include "sys/graph.h"
#include "sys/hwgraph.h"
#include "sys/iograph.h"
#include "sys/kmem.h"
#include "sys/immu.h"
#include "sys/atomic_ops.h"
#include "sys/alenlist.h"
#include "sys/poll.h"
#include "sys/partition.h"

#include "sys/SN/arch.h"
#include "sys/SN/addrs.h"
#include "sys/SN/partition.h"
#include "sys/SN/ln.h"
#if defined (SN0)
#include "sys/SN/SN0/bte.h"
#endif
#include "sys/SN/intr.h"

/*
 * Functions defined in this file - in this order.
 */
static	void	lnSetupLnr(lnr_t *, __uint32_t, __uint32_t);
static	int	lnProcessControl(lnr_t *);
static	void	lnNewInterrupt(partid_t, int);
static	void	lnInterrupt(partid_t, int);
static	void	lnAddPartition(partid_t p);
        void	lninit(void);
static	lnr_t	*lnFindRing(lnr_t *, const __uint32_t);
static  void	lnFreeRing(lnr_t *);
static	int	lnInitRing(lnr_t *, size_t);
static	lnMsg_t	*lnAllocSendMsg(lnr_t *, boolean_t);
static	int	lnSendControl(lnr_t *, lnCntl_t *);
static	int	lnCreateAlenList(uio_t *, alenlist_t *);
static	void	lnFreeAlenList(uio_t *, alenlist_t);
static	int	lnGetWait(lnr_t *, __uint32_t);
static	void	lnGetWakeup(lnr_t *);
static	int	lnSendMessage(lnr_t *, uio_t *);
static	int	lnPullData(lnr_t *, paddr_t, paddr_t, size_t);
static	int	lnReceiveMessage(lnr_t *, uio_t *);
        int	lnread(dev_t, uio_t *);
static	int	lnSendImmediate(lnr_t *, __psunsigned_t, size_t);
static	int	lnConnect(lnr_t *);
static	void	lnShutdownLink(lnr_t *, boolean_t);
        int	lnwrite(dev_t, uio_t *);
static	int	lnOpenControl(lnr_t *);
        int	lnpoll(dev_t, short, int, short *, struct pollhead **, unsigned int *);
        int	lnopen(dev_t *, int);
        int	lnclose(dev_t);


int	lndevflag = D_MP;

extern	int	ln_xp_raw;		/* raw interfaces */
extern	int	ln_xp_net;		/* network interfaces */

static	lnr_t		*ln_partitionRings[MAX_PARTITIONS];
static	xpMap_t		*ln_xpMap;
static	pfd_t		*ln_xpMapPfd;

#if _LN_DEBUG
#   define	DPRINTF(x)	lnDebug x
#   define	DPRINT_LNR(l)	lnDumpLnr(l)
#   define	DPRINT_LNM(l,s)	lnDumpLnm(l,s)
#   if _LN_DEBUG_INT
#       define	    DPRINTF_INT(x)	lnDebug x
#   else
#       define	    DPRINTF_INT(x)
#   endif
#else
#   define	DPRINTF(x)
#   define	DPRINT_LNR(l)
#   define	DPRINT_LNM(l, s)
#   define	DPRINTF_INT(x)
#endif


#if _LN_DEBUG

/*VARARGS*/
static	void
lnDebug(char *s, ...)
{
    va_list	ap;

    va_start(ap, s);
    icmn_err(CE_CONT|CE_CPUID, s, ap);
    va_end(ap);
}

static	void
lnDumpLnr(lnr_t *lnr)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    lnrXP_t	*lx;

    lnDebug("LNR[0x%x]\n", lnr);
    lnDebug("\tflags(0x%x) link(%d) control(0x%x) target(0x%x) seq(%d)\n",
	     lnr->lnr_flags, lnr->lnr_link, lnr->lnr_control, 
	    lnr->lnr_target, lnr->lnr_sequence);
    lx = lnr->lnr_lxp;
    lnDebug("\tlxp[0x%x]\n", lx);
    if (lx) {
	lnDebug("\t\tput(%d) limit(%d) get(%d) seq(%d) ring[0x%x]\n", 
		 lx->xp_put, lx->xp_limit, lx->xp_get, lx->xp_sequence, 
		lx->xp_ring);
    }
    lx = lnr->lnr_rxp;
    lnDebug("\trxp[0x%x]\n", lnr->lnr_rxp);
    if (lx) {
#if 0
	lnDebug("\t\tput(%d) limit(%d) get(%d) seq(%d) ring[0x%x]\n", 
		 lx->xp_put, lx->xp_limit, lx->xp_get, lx->xp_sequence,
		lx->xp_ring);
#endif
	lx = &lnr->lnr_crxp;
	lnDebug("\trxp[0x%x] ***CRXP***\n", lnr->lnr_rxp);
	if (lx) {
	    lnDebug("\t\tput(%d) limit(%d) get(%d) seq(%d) ring[0x%x]\n", 
		    lx->xp_put, lx->xp_limit, lx->xp_get, lx->xp_sequence,
		    lx->xp_ring);
	}
    }
}

void
lnDumpLnm(lnMsg_t *lnm, char *s)
{
    int		i;

    if (!lnm) {
	return;
    }
    lnDebug("\tLN (0x%x) Message(%s): type(%d) count(%d) flags(0x%x)\n", 
	     lnm, s, lnm->lnm_hdr.lnh_type, lnm->lnm_hdr.lnh_count, 
	     lnm->lnm_hdr.lnh_flags);
    if (lnm->lnm_hdr.lnh_type == LNH_T_INDIRECT) {
	for (i = 0; i < lnm->lnm_hdr.lnh_count; i++) {
	    lnDebug("\t\t[%d] 0x%x,%d\n", i, 
		     lnm->lnm_vec[i].lnv_base, lnm->lnm_vec[i].lnv_len);
	}
    }
}

#endif

static	int
lnFindRemoteControlRing(lnr_t *lnr)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    xpMap_t	*xpm;
    xpEntry_t	xpe;

    ASSERT(lnr->lnr_link == LN_CONTROL_LINK);

    xpm = (xpMap_t *)partFindXP(lnr->lnr_partid);
    if (0 == xpm) {
	return(ENODEV);
    }

    /* Pull over XPE from remote parition */

    if (0 != lnPullData(lnr, (paddr_t)&xpm->xpm_xpe[partID()], (paddr_t)&xpe, 
			sizeof(xpe))) {
	return(ENODEV);
    }
    
    if (0 == xpe.xpe_control) {
#if DEBUG
	extern void debug(char *);
	debug("ring");
#endif
	return(ENODEV);
    }

    ln_xpMap->xpm_xpe[lnr->lnr_partid].xpe_lpending = 
	TO_CAC((__psunsigned_t)&xpm->xpm_xpe[partID()].xpe_rpending);
    lnr->lnr_rxp = (lnrXP_t *)xpe.xpe_control;
    return(0);
}

static	void
lnSetupLnr(lnr_t *lnr, __uint32_t p, __uint32_t l)
/*
 * Function: 	lnSetupLnr
 * Purpose:	Set up the initial valus for a legonoet ring.
 * Parameters:	lnr - pointer to ring t initialize
 *		p - destination partition connected to
 *		l - "link" number in that partition.
 * Returns:	nothing
 */
{
    int	i;

    bzero(lnr, sizeof(*lnr));

    lnr->lnr_magic[0] = 'l'; 
    lnr->lnr_magic[1] = 'n';
    lnr->lnr_magic[2] = 'r'; 
    lnr->lnr_magic[3] = '\0';

    lnr->lnr_target   = 0;
    lnr->lnr_partid   = p;
    lnr->lnr_link     = l;
    lnr->lnr_sequence = 0;

    init_spinlock(&lnr->lnr_lock, "lnr_lock", 0);
    initnsema(&lnr->lnr_wr_sema, 1, "lnr_wr_sema");
    initnsema(&lnr->lnr_rd_sema, 1, "lnr_rd_sema");

    sv_init(&lnr->lnr_wrwait_sv, SV_FIFO, "lnr_wrwait");
    sv_init(&lnr->lnr_rdwait_sv, SV_FIFO, "lnr_rdwait");

    initnsema(&lnr->lnr_wrResource_sema, LNR_WR_CNT, "lnr_wrResource_sema");
    for (i = 0; i < LNR_WR_CNT; i++) {
	init_sema(&lnr->lnr_wrResources[i].lw_sema, 0, "lw_sema", i);
	lnr->lnr_wrResources[i].lw_next = &lnr->lnr_wrResources[i+1];
    }
    lnr->lnr_wrResources[LNR_WR_CNT-1].lw_next = NULL;
    lnr->lnr_wrFree = &lnr->lnr_wrResources[0];
    lnr->lnr_wrActive = NULL;

    lnr->lnr_copy[0] = (paddr_t)kmem_alloc(LN_PIPE_UNITS * LN_ALIGN_SIZE, 
					   KM_SLEEP);
    lnr->lnr_copy[1] = (paddr_t)kmem_alloc(LN_PIPE_UNITS * LN_ALIGN_SIZE, 
					   KM_SLEEP);

    if (!lnr->lnr_copy[0] || !lnr->lnr_copy[1]) {
	cmn_err(CE_PANIC, "kmem_alloc failed");
    }

    lnr->lnr_ph = phalloc(KM_SLEEP);
}    

static	void
lnPartitionDead(lnr_t *lnr)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    lnr_t	*clnr;

    /* Find control ring */

    clnr = lnr->lnr_control;

    LN_LOCK(clnr);
    cmn_err(CE_PANIC, "DEAD PARTITION");
    LN_UNLOCK(clnr);
}

static	int
lnProcessControl(lnr_t *clnr)
/*
 * Function: 	lnProcessControl
 * Purpose:	To process an incomming message/interrupt on the control ring.
 * Parameters:	lnr - pointer to the control ring.
 * Returns:	Nothing
 */
{
    lnMsg_t 	lnm;
    lnCntl_t 	*lnc;
    lnr_t	*lnr;

    LN_LOCK(clnr);

    if (!(clnr->lnr_flags & LNR_F_AVAIL)) {
	DPRINTF_INT(("lnProcessControl: cpu[%d]: FLAG[0x%x] ..\n", 
		     cpuid(), clnr->lnr_flags));
	if (0 != lnFindRemoteControlRing(clnr)) {
	    cmn_err(CE_WARN, "ln: Unable to locate remote control "
		    "ring for partition %d\n", clnr->lnr_partid);
	} else {
	    clnr->lnr_flags |= LNR_F_ROPEN;
	    if (clnr->lnr_flags & LNR_F_OPEN) { /* we have open waiting */
		LN_WRITE_GO(clnr);
	    }
	}
	LN_UNLOCK(clnr);
	return(0);
    }

    if (LNR_RXP_CACHE(clnr)) {
	LN_UNLOCK(clnr);
	lnPartitionDead(clnr);
	return(EIO);
    }

    DPRINTF_INT(("lnProcessControl: Pending: %d\n", LNR_PEND(clnr)));
    DPRINT_LNR(clnr);

    while (LNR_PEND(clnr)) {
	if (lnPullData(clnr, (paddr_t)LNR_GET(clnr), 
		       (paddr_t)&lnm, sizeof(lnm))) {
	    cmn_err(CE_PANIC, "lnPullData failed");
	}
	DPRINT_LNM(&lnm, "lnProcessControl");
	DPRINTF_INT(("lnProcessControl: msg@0x%x\n", LNR_GET(clnr)));
	lnc = (lnCntl_t *)lnm.lnm_imd;
	LNR_INC_GET(clnr);		/* Update ring */

	DPRINTF_INT(("lnProcessControl: cmd[%d] p[0,1,2,3] 0x%x,0x%x,0x%x,0x%x\n",
		 lnc->lnc_cmd, lnc->lnc_p[0], lnc->lnc_p[1], 
		 lnc->lnc_p[2], lnc->lnc_p[3]));
	
	lnr = lnFindRing(clnr, lnc->lnc_link);
	LN_UNLOCK(clnr);		/* Avoid deadlocks */
	switch(lnc->lnc_cmd) {
	case LN_CNTL_OPEN:		/* New connection request */
	    if (NULL == lnr) {		/* no such device */
		DPRINTF_INT(("lnProcessControl: ring not found: %d\b", 
			 lnc->lnc_p[2]));
		lnc->lnc_p[4] = ENODEV;
	    } else {			/* %%% WHAT IF OUT OF WACK  */
		LN_LOCK(lnr);
		if (lnr->lnr_flags & (LNR_F_ROPEN|LNR_F_AVAIL)) {
		    DPRINTF_INT(("Link LNR_F_AVAIL(%d)\n", lnr->lnr_link));
		    /*
		     * If available, then this message is a reply to our 
		     * "reply". 
		     */
		    lnc = NULL;
		} else {
		    DPRINTF_INT(("Link not yet LNR_F_AVAIL(%d)\n", lnr->lnr_link));
		    lnr->lnr_rxp  = (lnrXP_t *)lnc->lnc_p[0]; 
		    lnr->lnr_flags |= LNR_F_ROPEN;
		    if (lnr->lnr_flags & LNR_F_OPEN) { /* pending open? */
			lnc->lnc_p[0] = (paddr_t)lnr->lnr_lxp;
			lnc->lnc_p[1] = (paddr_t)lnr->lnr_rxp;
			lnc->lnc_p[3] = (__uint64_t)lnr->lnr_target;
			lnc->lnc_p[4] = 0;
			LN_WRITE_GO(lnr);	/* wake up sleeper */
		    } else {		/* No one waiting yet */
			lnc = NULL;		/* No reply yet */
		    }
		}
		LN_UNLOCK(lnr);
	    }
	    break;
	case LN_CNTL_SHUTDOWN:
	    if (NULL == lnr) {
		cmn_err(CE_WARN, "ln: Partition: %d Link: %d "
			         "Remote shutdown of non-existant link\n",
			clnr->lnr_partid, lnc->lnc_link);
		lnc = NULL;
		break;
	    }
	    LN_LOCK(lnr);
	    lnShutdownLink(lnr, B_TRUE); /* Unlocks on return */
	    lnc = NULL;
	    break;
	case LN_CNTL_NOP:
	    lnc = NULL;
	    break;
	case LN_CNTL_SEND_NOP:
	    lnc->lnc_cmd = LN_CNTL_NOP;
	    break;
	case LN_CNTL_TARGET:
	    LN_LOCK(lnr);
	    if (lnr->lnr_flags & LNR_F_AVAIL) {
		lnr->lnr_target = (hubreg_t *)(lnc->lnc_p[0]);
	    }
	    lnc = NULL;
	    break;
	default:
	    cmn_err(CE_WARN, 
		    "lnProcessControl: Invalid control message: "
		    "Type(%d) F(0x%x) C(0x%x)\n", lnm.lnm_hdr.lnh_type, 
		    lnm.lnm_hdr.lnh_flags, lnm.lnm_hdr.lnh_count);
	    cmn_err(CE_WARN, "lnProcessControl: CMD(%d) L(%d) errno(%d)\n",
		    lnc->lnc_cmd, lnc->lnc_link, lnc->lnc_errno);
	    ASSERT(0);
	}
	
	LN_LOCK(clnr);			/* need it locked now ....  */
	if (NULL != lnc) {		/* do we have a response to send? */
	    (void)lnSendControl(clnr, lnc);
	}
    }

    /* Sync up our end of the ring now */

    LNR_LXP_FLUSH(clnr);

    LN_UNLOCK(clnr);
    return(0);
}

/*ARGSUSED*/
static void
lnNewInterrupt(partid_t p, int r)
{
    lnAddPartition(p);
}

/* ARGSUSED */
static	void
lnInterrupt(partid_t p, int r)
/*
 * Function: 	lnIntrrupt
 * Purpose:	Process a cross/call interrupt from another partition
 * Parameters:	ef - pointer to eframe at time of interrupt
 *		a  - arbitrary parameter.
 * Returns:	nothing
 */

{
    lnr_t	*lnr;
    __uint64_t	lb;			/* link interrupt bit */
    __uint64_t	lp;			/* link pending mask */
    int		li;			/* region index/ link index */

    DPRINTF_INT(("lnIntr: cpu[%d] partid(%d) region(%d)\n", 
		 getcpuid(), p, r));

    /*
     * If we have not setup the region mapping for the source 
     * region yet, then it must be trying to set up a control 
     * link. Just ignore the interrupt until we are ready to 
     * handle it.
     */

    if (0 == ln_xpMap->xpm_xpe[p].xpe_lpending) {
	lp = 1 << LN_CONTROL_LINK;
    } else {
	LNR_LINK_PEND(p, lp);
	LNR_LINK_CLEAR(p, lp);
    }

    DPRINTF_INT(("lnIntr: Part pending: %d Links pending: 0x%x\n", 
		 p, lp));

    if (lp & (1 << LN_CONTROL_LINK)) {	/* control link pending */
	lnProcessControl(ln_partitionRings[p]);
	lp ^= (1 << LN_CONTROL_LINK);
    }
	    
    for (li = 0, lb = 1; li <= ln_xp_raw; li++, lb <<= 1) {
	if (!(lb & lp)) {
	    continue;
	}
	/* Ring "i" has posted an interrupt, lets go investigate ... */

	lnr = ln_partitionRings[p] + li;
	LN_LOCK(lnr);
	
	/*
	 * Pick up cached copy of remote XP, and figure out what to do.
	 */

	if (LNR_RXP_CACHE(lnr)) {
	    LN_UNLOCK(lnr);
	    lnPartitionDead(lnr);
	    continue;
	}

	if (lnr->lnr_flags & LNR_F_CWAIT) {
	    lnGetWakeup(lnr);
	}

	if (lnr->lnr_flags & LNR_F_RD_WAIT) {
	    /* If read pending and there is data space, wake up reader */
	    if (LNR_PEND(lnr)) {
		LN_READ_GO(lnr);
	    }
	}

	if (lnr->lnr_flags & LNR_F_WR_WAIT) {
	    /* If write pending and space now, wake up writer. */
	    if (LNR_AVAIL(lnr)) {
		LN_WRITE_GO(lnr);
	    }
	}

	if (lnr->lnr_flags & LNR_F_RDPOLL) {
	    if (LNR_PEND(lnr)) {
		pollwakeup(lnr->lnr_ph, POLLIN|POLLRDNORM);
		lnr->lnr_flags &= ~LNR_F_RDPOLL;
	    }
	}

	if (lnr->lnr_flags & LNR_F_WRPOLL) {
	    if (LNR_AVAIL(lnr)) {
		pollwakeup(lnr->lnr_ph, POLLOUT|POLLWRNORM);
		lnr->lnr_flags &= ~LNR_F_WRPOLL;
	    }
	}

	LN_UNLOCK(lnr);
    }
}

static	void
lnAddPartition(partid_t p)
/*
 * Function: 	lnAddPartition
 * Purpose:	To add a partition that appeard.
 * Parameters:	p - partition Id that appeared.
 * Returns:	Nothing
 */
{
#define EDGE_LBL_XP	"xp"
    graph_error_t	ge;
    char		name[MAXPATHLEN];
    int			i;
    extern void		sprintf(char *, const char *, ...);
    dev_t		td;
    lnr_t		*lnr, *clnr;
    extern void	install_ccintr(void *);

    /* If partition rings already exist, then we are done. */

    if (NULL != ln_partitionRings[p]) {
	return;
    }

    /*
     * Allocate all of the rings as a contiguous chunk of memory. +1
     * because of control ring.
     */
    if (NULL == (clnr=kmem_alloc(sizeof(lnr_t) * (ln_xp_raw + 1), KM_SLEEP))){
	cmn_err(CE_WARN, "lninit: unable to allocate partition %d rings\n",
		p);
	return;
    }
    ln_partitionRings[p] = clnr;
    /*
     * Create kernel "special" device, which is the control ring.
     * I do not know why we make it visible other that to make finding
     * it easier. 
     */
    sprintf(name, "%s/%d/cntl", EDGE_LBL_XP, p);
    if (GRAPH_SUCCESS != 
	(ge = hwgraph_char_device_add(hwgraph_root, name, "ln", &td))){
	cmn_err(CE_WARN, "lninit: hwgraph_device_add: failed: %d: %s\n", 
		ge, name);
	return;				/* %%%% CLEAN UP/REMOVE */
    } else {
	device_info_set(td, clnr);
	lnSetupLnr(clnr, p, 0);
    }

    /*
     * Fill in XPMAP - we use this as a cross partition parameter.
     */
    ln_xpMap->xpm_xpe[p].xpe_control  = 0;
    ln_xpMap->xpm_xpe[p].xpe_lpending = 0;
    ln_xpMap->xpm_xpe[p].xpe_rpending = 0;
    ln_xpMap->xpm_xpe[p].xpe_cnt      = ln_xp_raw + ln_xp_net;

    /* Now create the raw devices */

    for (i = 1, lnr = clnr + 1; i <= ln_xp_raw; i++, lnr++) {
	sprintf(name, "%s/%d/%d", EDGE_LBL_XP, p, i);
	if (GRAPH_SUCCESS != 
	    (ge = hwgraph_char_device_add(hwgraph_root, name, "ln", &td))){
	    cmn_err(CE_WARN, "lninit: hwgraph_device_add: failed: %d: %s\n", 
		    ge, name);
	} else {
	    device_info_set(td, lnr);
	    lnSetupLnr(lnr, p, i);
	    lnr->lnr_control   = clnr;
	}
    }

    /* ... and the network interfaces */
    for (i = 0; i < ln_xp_net; i++) {
    }

    /* Permit cross partition map */

    partPermitPage(p, ln_xpMapPfd, pAccessRW);
/* %%% IF FAILURE  ---- free rings %%%% */

    /* Set up normal interrupt for this partition. */

    partInterruptSet(p, lnInterrupt);
    
}

void
lninit(void)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    partid_t	p;

#define EDGE_LBL_XP	"xp"

    DPRINTF(("lninit: initializing ln driver\n"));

    ASSERT(LN_ALIGN_SIZE <= LN_IDATA_CNT); /* Just to be sure */


    if (INVALID_PARTID == partID()) {
	DPRINTF(("lninit: partitions disabled\n"));
	return;
    }
    
    /*
     * Allocate cross partition map, permit it to all other partitions
     * and  set the XP variable
     */
    ASSERT(sizeof(xpMap_t) < 1024*16);
    ln_xpMapPfd = partPageAllocate(partID(), pAccessRW, 0, 0, 1024*16, 0);
    if (NULL == ln_xpMapPfd) {
	cmn_err(CE_PANIC, "Unable to initialize XP map");
    }

    ln_xpMap = (xpMap_t *)TO_CAC(pfdattophys(ln_xpMapPfd));
    bzero(ln_xpMap, 1024*16);
    partSetXP((__psunsigned_t)TO_PHYS((__psunsigned_t)ln_xpMap));

    /*
     * Create devices for all possible partitions.
     */
    for (p = 0; p < MAX_PARTITIONS; p++) {
	partInterruptSet(p, lnNewInterrupt);
    }

    for (p = 0; p < MAX_PARTITIONS; p++) {
	if (NULL != partGet(p)) {	/* if partition exists */
	    lnAddPartition(p);
	}
    }
}

static	lnr_t	*
lnFindRing(lnr_t *clnr, const __uint32_t l)
/*
 * Function: 	lnFindRing
 * Purpose:	Find a ring given the control ring.
 * Parameters:	clnr - control ring to search off of.
 *		l    - link number to find.
 * Returns:  	Pointer to lnr, or NULL if not found.

 */
{
    if (l > ln_xp_raw) {
	return(NULL);
    } else {
	return(clnr + l);
    }
}


static	void
lnFreeRing(lnr_t *lnr)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    ASSERT(LN_IS_LOCKED(lnr));
    DPRINTF(("lnFreeRing: Freeing lnr[0x%x] Ring[0x%x]\n", 
	     lnr, lnr->lnr_lxp));

    lnr->lnr_flags &= ~(LNR_F_SHUTDOWN|LNR_F_RSHUTDOWN|LNR_F_AVAIL);

    pagefree_size(lnr->lnr_pfd, lnr->lnr_psz);
    lnr->lnr_pfd = NULL;
    lnr->lnr_psz = 0;

    lnr->lnr_lr = NULL;			/* just for safty */
    lnr->lnr_rr = NULL;			/* just for safty */
    LN_UNLOCK(lnr);
}

static int
lnInitRing(lnr_t *lnr, size_t size)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    paddr_t	pa;			/* temp paddr */
    pn_t	*pn;

    LNR_CHECK(lnr);
    ASSERT(LN_IS_LOCKED(lnr));

    lnr->lnr_pfd = pagealloc_size(0, 0, size);	/* %%% 0,0?? */
    lnr->lnr_psz = size;
    if (NULL == lnr->lnr_pfd) {
	return(ENOMEM);
    }

    /* Carve up the region of memory. */

    pa = pfdattophys(lnr->lnr_pfd);

    lnr->lnr_lxp= (lnrXP_t *)PHYS_TO_K0(pa);
    pa += LNRXP_SIZE;			/* Physical base of ring */

    bzero(&lnr->lnr_clxp, sizeof(lnr->lnr_clxp));
    bzero(&lnr->lnr_crxp, sizeof(lnr->lnr_crxp));
    bzero(lnr->lnr_lxp, size);		/* This zaps ring and xp struct! */
    lnr->lnr_lr = (lnMsg_t *)PHYS_TO_K0(pa); /* virtual base of ring */

    lnr->lnr_sequence = 0;
    lnr->lnr_rcvLen   = 0;
    lnr->lnr_rcvCp    = NULL;
    lnr->lnr_rcvLnv   = NULL;
    lnr->lnr_rcvLnvEnd= NULL;

    /* Try to find a CPU to send the interrupt to.... */

    pn = partGetNasid(lnr->lnr_partid, 0);
    if (NULL == pn) {
	cmn_err(CE_WARN+CE_CPUID, "No CPUs in partition: %d\n", 
		lnr->lnr_partid);
	return(ENODEV);
    }
    DPRINTF(("lnOpenControl: partGetNasid: %d %x <%s%s%s>\n", 
	     pn->pn_partid, pn->pn_nasid, 
	     pn->pn_cpuA ? "cpuA" : "" , 
	     pn->pn_cpuA && pn->pn_cpuB ? "+" : "", 
	     pn->pn_cpuB ? "cpuB" : ""));
	     
    lnr->lnr_target = 
	REMOTE_HUB_ADDR(pn->pn_nasid, 
			pn->pn_cpuA ? PI_CC_PEND_SET_A : PI_CC_PEND_SET_B);


    /*
     * Limit is the highest index + 1 that is valid in the ring.  After 
     * updating the "real" one, we update the "cached" copy.
     */

    lnr->lnr_clxp.xp_ring = pa;        /* physical base of ring */
    lnr->lnr_clxp.xp_limit = (size - LNRXP_SIZE) / LN_MESSAGE_SIZE;
    lnr->lnr_clxp.xp_sequence = (__uint32_t)-1;
    LNR_LXP_FLUSH(lnr);		        /* Push out to memory */

    return(0);
}

static	lnMsg_t	*
lnAllocSendMsg(lnr_t *lnr, boolean_t wait)
/*
 * Function: 	lnAllocSendMsg
 * Purpose:	Allocate a single message entry on the outgoing ring buffer.
 * Parameters:	lnr - ring to allocate message on.
 *		wait - if true, we will sleep waiting for entry to come free.
 * Returns:	Pointer to message, NULL if not allocated.
 * Notes:	Assumes remote X-partition data is cached.
 */
{
    lnMsg_t	*lnm;
    int		r;			/* result from wait */

    LNR_CHECK(lnr);

    lnm = NULL;
    DPRINTF(("lnAllocSendMsg: lnr[0x%x] wait=%s\n", lnr, wait ? "Yes":"No"));
    DPRINT_LNR(lnr);
    do {
	if (LNR_AVAIL(lnr)) {
	    lnm = &lnr->lnr_lr[LNR_NEXT(lnr)];
	    LNR_INC_PUT(lnr);
            break;
	} else if (wait) {
	    LN_LOCK(lnr);
	    LN_WRITE_WAIT(lnr, r);	/* Unlocks */
	    if (LNR_RXP_CACHE(lnr)) {
		lnPartitionDead(lnr);
		return(NULL);
	    }
	    if (0 != r) {
		wait = B_FALSE;
	    }
	}
    } while (wait);

    if (NULL != lnm) {
	/*
	 * If we are running low on entires, request an interrupt 
	 * from the other side when they get this one. %%% MAKE THRESHOLD
	 * THere is a race when we assign the sequence #, but it is OK 
	 * since wemay get an extra interrupt for a sequence # but 
	 * we handle that.
	 */
	if (!LNR_AVAIL(lnr)) {
	    lnm->lnm_hdr.lnh_flags = LNH_F_NOTIFY;
	    lnm->lnm_hdr.lnh_seq   = lnr->lnr_sequence;
	} else {
	    lnm->lnm_hdr.lnh_flags = LNH_F_CLEAR;
	}
	LNR_LXP_FLUSH(lnr);
	LNR_SEND_INT(lnr);		/* geta movin dude */
    }
	
    DPRINTF(("lnAllocSendMsg: lnr[0x%x] wait=%s msg=0x%x\n", 
	     lnr, wait ? "Yes" : "No", lnm));
    return(lnm);
}

static	int
lnSendControl(lnr_t *lnr, lnCntl_t *lnc)
/*
 * Function: 	lnSendControl
 * Purpose:	To send a control message to the remote end of the sn0net.
 * Parameters:	lnr - pointer to ring to message on.
 *		lnc - pointer to control message to send.
 * Returns: 	0 - Sent OK
 *		errno - error occured.
 * Notes:	Assumes caller will "cache" anc "flush" lxp values. 
 *		This is required to allow a connection message to be placed
 *		in the ring.
 */
{
    lnMsg_t	*lnm;
    int		errno;

    DPRINTF(("lnSendControl: lnr[0x%x] c[%d] p[0,1,2,3]="
	     "0x%x,0x%x,0x%x,0x%x\n", 
	     lnr, lnc->lnc_cmd, lnc->lnc_p[0], lnc->lnc_p[1], 
	     lnc->lnc_p[2], lnc->lnc_p[3]));

    ASSERT(LN_IS_LOCKED(lnr));

    if (LNR_RXP_CACHE(lnr)) {
	LN_UNLOCK(lnr);
	lnPartitionDead(lnr);
	LN_LOCK(lnr);			/* return the way we expect */
    }
    
    lnm = lnAllocSendMsg(lnr, B_FALSE);	/* Updates cached pointers */

    if (NULL != lnm) {
	lnm->lnm_hdr.lnh_type   = LNH_T_CONTROL;
	lnm->lnm_hdr.lnh_count  = sizeof(*lnc);
	bcopy(lnc, lnm->lnm_imd, sizeof(*lnc));
	errno = 0;
    } else {
	errno = EAGAIN;
    }
    DPRINT_LNM(lnm, "lnSendControl");
    LNR_LXP_FLUSH(lnr);
    LNR_SEND_INT(lnr);
    
    return(errno);
}


static	int
lnCreateAlenList(uio_t *uio, alenlist_t *al)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    iovec_t	*iov;
    int		iovCnt;
    int		errno;

    *al = 0;

    for (iov=uio->uio_iov,iovCnt=0; iovCnt < uio->uio_iovcnt; iovCnt++,iov++){
	errno = useracc(iov->iov_base, iov->iov_len, B_WRITE, NULL);
	if (0 != errno) {
	    break;
	}
	*al = uvaddr_to_alenlist(*al, uio->uio_iov[iovCnt].iov_base, 
				 uio->uio_iov[iovCnt].iov_len, 0);
    }
    if (0 != errno) {
	for (iov = uio->uio_iov; iovCnt >= 0; iovCnt--, iov++) {
	    unuseracc(iov->iov_base, iov->iov_len, B_WRITE);
	}
	alenlist_done(*al);
    }
    return(errno);
}

static	void
lnFreeAlenList(uio_t *uio, alenlist_t al)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    iovec_t	*iov;
    int		iovCnt;

    for (iov=uio->uio_iov,iovCnt=0; iovCnt < uio->uio_iovcnt; iovCnt++,iov++){
	unuseracc(iov->iov_base, iov->iov_len, B_WRITE);
    }
    alenlist_done(al);
}

static	int
lnGetWait(lnr_t *lnr, __uint32_t sequence)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 *
 * Notes: 
 */
{
    lnrWrWait_t	*lw;
    int		r;

    /*
     * Do not cache RXP copies, that is done by our caller, OR, by the
     * interrupt handler that wakes us up.
     */
    LN_LOCK(lnr);

    /* 
     * Check if we are already done.
     */
    if (sequence == lnr->lnr_crxp.xp_sequence) {
	LN_WRITE_UNLOCK(lnr);
	LN_UNLOCK(lnr);
	return(0);
    }

    LN_WRITE_UNLOCK(lnr);

    /* 
     *  If we have to sleep, pull off a write resource. We know
     *  we have one avail, and sleep on it.
     */
    lw = lnr->lnr_wrFree;
    lnr->lnr_wrFree = lw->lw_next;
    lnr->lnr_flags |= LNR_F_CWAIT;
    if (NULL == lnr->lnr_wrActive) {
	lnr->lnr_wrActive = lw;
    } else {
	lnr->lnr_wrActiveEnd->lw_next = lw;
    }
    lnr->lnr_wrActiveEnd = lw;
    lw->lw_next = NULL;
    lw->lw_sequence = sequence;

    LN_UNLOCK(lnr);

    do {
	r = psema(&lw->lw_sema, PWAIT|PCATCH);
	if (0 != r) {
	    if (!(lnr->lnr_flags & LNR_F_AVAIL)) {
		return(ENOTCONN);
	    }
	}
    } while (0 != r);

    return(0);
}

static	void
lnGetWakeup(lnr_t *lnr)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 * 
 * Locks held on entry:
 * Locks held on exit:
 * Locks aquired:
 * Notes: Remote XP values assumed cached on entry.
 */
{
    __uint64_t	seq;
    lnrWrWait_t *lw;

    ASSERT(LN_IS_LOCKED(lnr));
    
    /* Go down the ordered pending list - start up those that are done */

    seq = lnr->lnr_crxp.xp_sequence;
    while (NULL != (lw = lnr->lnr_wrActive)) {
	/*
	 * If the current sequence # in the ring is < than recored in
	 * the wait struct, then they wrapped.  In that case, a sequence
	 * # <= current  sequqnce number will result in a match for 
	 * current transaction.
	 */
	if ((seq >= lw->lw_sequence) 
	    || ((lnr->lnr_sequence < lw->lw_sequence) 
		&& (seq <= lnr->lnr_sequence))) {
	    vsema(&lw->lw_sema);
	    lnr->lnr_wrActive = lw->lw_next; /* unlink it  */
	    if (NULL == lw->lw_next) { 
		lnr->lnr_flags &= ~LNR_F_CWAIT;
		lnr->lnr_wrActiveEnd = NULL;
	    } 
	    lw->lw_next = lnr->lnr_wrFree; /* link on free chain */
	    lnr->lnr_wrFree = lw;
	} else {
	    return;
	}
    }
}

static	int
lnSendMessage(lnr_t *lnr, uio_t *uio)
/*
 * Function: 	lnSendMessage
 * Purpose:	To send a non-immediate format message.
 * Parameters:	lnr - pointer to ring to send on
 *		uio - pointer to uio passed in from user.
 * Returns:	0 - success, errno on failure.
 * 
 * Locks held on entry:	lnr_wr_sema
 * Locks held on exit:	lnr_wr_sema
 * Locks aquired:	None
 */
{
    size_t		l, tl, all;	/* alen - length */
    int			errno,r; /* return value */
    alenlist_t		al;
    alenaddr_t		alp;
    lnMsg_t		*lnm;
    lnVec_t		*lnv, *lnvEnd;
    boolean_t		wait = B_FALSE;
    __uint32_t		waitSeq;

    DPRINTF(("lnSendMessage: lnr[0x%x] uio[0x%x] uio->uio_resid[%d]\n", 
	     lnr, uio, uio->uio_resid));
    
    /*
     * Build up physical address/length pairs for the entire message for 
     * the lower level routines to handle.
     */

    errno = lnCreateAlenList(uio, &al);
    if (0 != errno) {
	return(errno);
    }
    l   = uio->uio_resid;
    lnm = NULL;
    lnv = lnvEnd = NULL;

    LN_WRITE(lnr, r);	/* %%%%% NOT NEEDED FOR IMMEDIATE %%%%%% */
    if (r) {
	lnFreeAlenList(uio, al);
	return(EINTR);
    }
    LN_WRITE_LOCK(lnr, r);
    if (r) {
	lnFreeAlenList(uio, al);
	return(EINTR);
    }
 
   if (LNR_RXP_CACHE(lnr)) {	/* cache as little as possible */
       lnPartitionDead(lnr);
       lnFreeAlenList(uio, al);
       LN_WRITE_DONE(lnr);
       LN_WRITE_UNLOCK(lnr);
       return(EIO);			/* I/O error */
   }

   while (l) {
	if (ALENLIST_SUCCESS != alenlist_get(al, NULL, 0, &alp, &all, 0)) {
	    cmn_err(CE_PANIC, "lnSendMessage: alenlist_get: failed\n");
	}

	l -= all;			/* Update uio residual */

	/* (1) First "partial" unit to get us to the alignment we require */

	if (LN_OFFSET(alp) != 0) {
	    tl = LN_ALIGN_SIZE - LN_OFFSET(alp);
	    tl = tl > all ? all : tl;
	    errno = lnSendImmediate(lnr, alp, tl);
	    if (0 != errno) {
		break;			/* while */
	    }
	    alp += tl;
	    all -= tl;
	    lnm  = NULL;		/* need new message */
	}

	/* (2) Complete "units" to send in this block */

	tl = LN_ALIGN(all);
	if (0 != tl) {
	    wait = B_TRUE;
	    if (NULL == lnm) {		/* Need to allocate another msg */
		lnm = lnAllocSendMsg(lnr, B_TRUE);
		if (NULL == lnm) {
		    errno = ECONNRESET;
		    break;
		}
		lnv    = lnm->lnm_vec;
		lnvEnd = &lnm->lnm_vec[LN_VEC_CNT];
		waitSeq = lnm->lnm_hdr.lnh_seq = ++lnr->lnr_sequence;
		lnm->lnm_hdr.lnh_flags |= LNH_F_NOTIFY;
		lnm->lnm_hdr.lnh_count = 0;
		lnm->lnm_hdr.lnh_type  = LNH_T_INDIRECT;
	    }

	    lnv->lnv_len  = tl;
	    lnv->lnv_base = alp;
	    lnm->lnm_hdr.lnh_count++;

	    all -= tl;			/* Update physical length */
	    alp += tl;			/* Update physical pointer */

	    if (++lnv == lnvEnd) {	/* Force allocate next time */
		lnm = NULL;
	    }
	}

	/* (3) Last partial "unit" in this block */

	if (all > 0) {
	    errno = lnSendImmediate(lnr, alp, all);
	    lnm = NULL;			/* Force allocation again */
	}
    }

    LNR_LXP_FLUSH(lnr);
    LNR_SEND_INT(lnr);

    /*
     * Need to hold LN_WRITE_LOCK until the sequence number checking is
     * complete.
     */
    if (wait) {				/* Do we need to wait until done? */
	lnGetWait(lnr, waitSeq);
    } else {
	LN_WRITE_UNLOCK(lnr);
    }
	
    uio->uio_resid = l;
    lnFreeAlenList(uio, al);

    LN_WRITE_DONE(lnr);
    return(errno);
}

/* ARGSUSED */
static	int
lnPullData(lnr_t *lnr, paddr_t sp, paddr_t dp, size_t l)
/*
 * Function: 	lnPullData
 * Purpose:	To copy data from remote partition.
 * Parameters:	lnr - pointer to "ring" data is being moved for.
 *		sp - source pointer (across partition)
 *		dp - destination pointer (local partition)
 *		l  - length to move
 * Returns: 0 - success
 *	    errno - failed.
 * Notes: The sender aligns buffer when possible, so on a source buffer that
 *	  is not aligned we do the double copy trick - pull it over with the 
 *	  BTE, and then bcopy it into the target buffer.
 */
{
#define	LN_BTE_ASYNC	0

    int		result, errno, bIdx;
    int		pil;			/* HACK gotta fix this */
#if LN_BTE_ASYNC
    void	*bte;
#endif
    paddr_t	b;
    paddr_t 	bteBase, bteOff;
    size_t	pBteLen, bteUnits, units;

    pil = splhi();			/* HACK - fix with threads */
#if LN_BTE_ASYNC
    bte = bte_acquire(1);		/* Get the BTE */
#endif

    errno = 0;

    if (LN_ALIGNED(sp) && LN_ALIGNED(dp) && LN_ALIGNED(l) && IS_KSEGDM(dp)) {
/* %%%% IS_KSEGDM - address could be physical right???? %%% */
#if LN_BTE_ASYNC
	result = bte_copy(sp, dp, l, BTE_ASYNC, bte);
	result = bte_rendezvous(bte, BTERNDVZ_SPIN);
#else
	result = bte_copy(sp, dp, l, BTE_NORMAL, NULL);
#endif

	if (result) {
	    errno = EIO;
	}
    } else {
	/*
	 * 2 copy model
	 *
	 * Using 2 buffers, we attempt to pipeline the data transfer so that
	 * the BTE is transfering into one buffer, while we copy out of the 
	 * other.
	 * 
	 * All transfers must start on an LN_ALIGN boundary for DMA, and 
	 * be a multiple of LN_ALIGN bytes long. 
	 * 
	 * bteUnits - # of LN_ALIGN_SIZE regions left to DMA.
	 * l        - # of bytes left to copy.
	 */

	bteUnits = LN_UNITS(l + LN_ALIGN_SIZE - 1 + LN_OFFSET(sp));
	bteBase  = LN_ALIGN(sp);
	bteOff	 = LN_OFFSET(sp);

	pBteLen  = 0;
	bIdx	 = 0; 		/* start off using BTE into index 0 */

	/*
	 * For here on, we are going to do cached copies into the 
	 * target address. Convert destination pointer to CAC, if
	 * it is physical.
	 */
	if (IS_KPHYS(dp)) {
	    dp = TO_CAC(dp);
	}

	while (bteUnits || l) {
	    units = bteUnits > LN_PIPE_UNITS ? LN_PIPE_UNITS : bteUnits;
	    /*
	     * Start up transfer of complete "units" or multiples of 
	     * cache lines.
	     */
	    if (units) {
		b = lnr->lnr_copy[bIdx];
#if LN_BTE_ASYNC
		result = bte_copy(bteBase, b, units * LN_ALIGN_SIZE, 
				  BTE_ASYNC, bte);
#else 
		result = bte_copy(bteBase, b, units * LN_ALIGN_SIZE, 
				  BTE_NORMAL, NULL);
#endif
		bteBase += units * LN_ALIGN_SIZE;
	    }
	    /*
	     * If we already have data in one of the buffers, copy it now.
	     */
	    if (pBteLen) {
		bcopy((void *)(lnr->lnr_copy[!bIdx]+bteOff),
		      (void *)dp, pBteLen);
		l     -= pBteLen;
		dp   += pBteLen;
		bteOff = 0;		/* only !0 the first time */
	    }
	    /*
	     * DMA should be done now ...
	     */
	    if (units) {
#if LN_BTE_ASYNC
		result = bte_rendezvous(bte, 0);
#endif
		if (result) {
		    errno = EIO;
		    break;
		}
		bIdx = !bIdx; 	/* flip indication of BTE buffer  */
		bteUnits -= units;
		/* Now set up the pBte values ..... */
		pBteLen = (units * LN_ALIGN_SIZE) - bteOff;
		if (pBteLen > l) {	/* don't spill over */
		    pBteLen = l;
		}
	    }
	}
    }
#if LN_BTE_ASYNC
    bte_release(bte);
#endif
    splx(pil);				/* HACK - fix with threads */
    return(errno);
}

static void
lnUpdateSeq(lnr_t *lnr, __uint32_t seq)
{
    lnr->lnr_clxp.xp_sequence = seq;
    LNR_LXP_FLUSH(lnr);
    LNR_SEND_INT(lnr);
}

static	int
lnReceiveMessage(lnr_t *lnr, uio_t *uio)
{
    size_t	all, cl, tl, l;
    int		errno;
    lnMsg_t	*lnm, lnmBuf;
    lnVec_t	*lnv, *lnvEnd;
    alenlist_t	al;
    alenaddr_t  alp;
    paddr_t cp;

    DPRINTF(("lnReceiveMessage: lnr[%x] uio[%x] uio->uio_resid[%d]\n", 
	     lnr, uio, uio->uio_resid));
    errno = lnCreateAlenList(uio, &al);
    if (errno != 0) {
	DPRINTF(("lnReceiveMessage: lnCreateAlenList: errno=%d\n", errno));
	return(errno);
    }

    l = uio->uio_resid;

    lnv    = lnr->lnr_rcvLnv;
    lnvEnd = lnr->lnr_rcvLnvEnd;
    cp 	   = lnr->lnr_rcvCp;		/* Current receive pointer */
    cl 	   = lnr->lnr_rcvLen;		/* Current receive length */
    
    DPRINTF(("lnReceiveMessage: init: lnv[0x%x] lnvEnd[0x%x] cp[0x%x], cl[%d]\n",
	     lnv, lnvEnd, cp, cl));

    if (0 != cl) {		/* Not incremented from last time */
	if (0 != lnPullData(lnr, (paddr_t)LNR_GET(lnr), 
			    (paddr_t)&lnmBuf, sizeof(lnmBuf))) {
	    cmn_err(CE_PANIC, "Failed to get LNMBUF *** FIXME");
	}
	lnm    = &lnmBuf;
    } else {
	lnm    = NULL;			/* Null if no previous message */
    }

    all = 0;
    alp = 0;

    while (l > 0) {
	if (0 == cl) {			/* pick up message pointers */
	    if ((NULL != lnv) && (++lnv < lnvEnd)) {
		/*
		 * If more data in current indirect message, go for that.
		 */
		cp = lnv->lnv_base;
		cl = lnv->lnv_len;
	    } else {
		/*
		 * Moving on to next message. If we had a last message, 
		 * and if had notify set, then set interrupt. Also, 
		 * if we had a last message, we need to increment our
		 * get pointer.
		 */
		if (NULL != lnm) {
		    if (lnm->lnm_hdr.lnh_flags & LNH_F_NOTIFY) {
			lnUpdateSeq(lnr, lnm->lnm_hdr.lnh_seq);
		    }
		    if (!LNR_PEND(lnr)) {
			break;
		    }
		    LNR_INC_GET(lnr);
		    lnm = NULL;
		}
		if (!LNR_PEND(lnr)) {
		    /* If no pending data - get out of here now */
		    cp     = NULL;
		    cl     = 0;
		    lnv    = NULL;
		    lnvEnd = NULL;
		    break;
		}  
		if (0 != lnPullData(lnr, (paddr_t)LNR_GET(lnr), 
				    (paddr_t)&lnmBuf, sizeof(lnmBuf))) {
		    cmn_err(CE_PANIC, "Failed to get LNMBUF");
		}
		lnm    = &lnmBuf;

		if (lnm->lnm_hdr.lnh_type == LNH_T_IMMEDIATE) {
		    lnv    = NULL;
		    lnvEnd = NULL;
		    cp     = (paddr_t)lnm->lnm_imd;
		    cl     = lnm->lnm_hdr.lnh_count;
		} else if (lnm->lnm_hdr.lnh_type == LNH_T_INDIRECT) {
		    lnv    = lnm->lnm_vec;
		    lnvEnd = &lnm->lnm_vec[lnm->lnm_hdr.lnh_count];
		    cp     = lnv->lnv_base;
		    cl     = lnv->lnv_len;
		} else {
		    errno = EPROTO;
		    break; /* while */
		}
	    }
	}

	if (0 == all) {			/* pick up user data pointers */
	    if (ALENLIST_SUCCESS != alenlist_get(al, NULL, 0, &alp, &all, 0)){
		cmn_err(CE_PANIC,"lnReceiveMessage: alenlist_get: failed\n");
	    }
	}

	/* Now move as much data as possible */

	tl = all > cl ? cl : all;
	if (lnm->lnm_hdr.lnh_type == LNH_T_IMMEDIATE) {
	    bcopy((void *)cp, (void *)TO_CAC(alp), tl);
	    errno = 0;
	} else {
	    errno = lnPullData(lnr, cp, alp, tl);
	}
	
	all -= tl;			/* decrment lengths */
	cl  -= tl;			/* increment pointers */
	l   -= tl;
	cp  += tl;
	alp += tl;
    }

    lnFreeAlenList(uio, al);

    /* Update lnr to reflect any pending data we are leaving. */

    if ((0 == cl)  && (NULL != lnm)) {
	if (lnm->lnm_hdr.lnh_flags & LNH_F_NOTIFY) {
	    lnUpdateSeq(lnr, lnm->lnm_hdr.lnh_seq);
	}
	if ((NULL == lnv) || 
	    ((NULL != lnv) && !((lnv + 1) < lnvEnd))) {
	    /*
	     * Push us on to next message.
	     */
	    LNR_INC_GET(lnr);
	    lnv    = NULL;
	    lnvEnd = NULL;
	    cp     = NULL;
	}
    } 
	     
    lnr->lnr_rcvLen = cl;
    lnr->lnr_rcvCp  = cp;
    lnr->lnr_rcvLnv = lnv;
    lnr->lnr_rcvLnvEnd = lnvEnd;

    uio->uio_resid = l;
    LNR_LXP_FLUSH(lnr);
    return(errno);
}

int
lnread(dev_t dev, uio_t *uio)
/*
 * Function: 	lnread
 * Purpose:	User entry point for reading from a cross partition device.
 * Parameters:	dev - device to read from
 *		uio - pointer to user I/O vector.
 * Returns:	0 for success, errno otherwise.
 */
{
    int		errno, r;		/* return value/wait result */
    lnr_t 	*lnr;

    lnr = (lnr_t *)device_info_get(dev);
    ASSERT(NULL != lnr);
    LNR_CHECK(lnr);

    LN_LOCK(lnr);
    if (!(lnr->lnr_flags & LNR_F_AVAIL)) {
	LN_UNLOCK(lnr);
	return(ENOTCONN);
    }
    LN_UNLOCK(lnr);
    LN_READ_LOCK(lnr, r);
    if (0 != r) {
	return(EINTR);
    }
    LN_LOCK(lnr);

    errno = 0;

    /*
     * Check if data is already available. If so, go ahead and read, 
     * otherwise, we sleep waiting for data.
     */

    if (LNR_RXP_CACHE(lnr)) {
	LN_UNLOCK(lnr);
	LN_READ_UNLOCK(lnr);
	lnPartitionDead(lnr);
	return(EIO);
    }

    while ((0 == errno) && (0 == LNR_PEND(lnr))) {
	if (lnr->lnr_flags & LNR_F_RSHUTDOWN) {
	    errno = ENOTCONN;
	    continue;
	} 
	LN_READ_WAIT(lnr, r);		/* Must be data - ring UNLOCKED*/
	LN_LOCK(lnr);
	if (0 != r) {
	    errno = EINTR; 
	    continue;
	} 
	if (LNR_RXP_CACHE(lnr)) {	/* May be updated */
	    LN_READ_UNLOCK(lnr);
	    lnPartitionDead(lnr);
	    LN_UNLOCK(lnr);
	    return(EIO);
	}
    } 
    LN_UNLOCK(lnr);			/* don't lock for data movement */

    if (0 == errno) {
	ASSERT(LNR_PEND(lnr));
	errno = lnReceiveMessage(lnr, uio);
    }

    LN_READ_UNLOCK(lnr);
    return(errno);
}

static	int
lnSendImmediate(lnr_t *lnr, __psunsigned_t d, size_t l)
/*
 * Function: 	lnSendImmediate
 * Purpose:	To put a small (fast) message on the outbound queue
 *		if possible. 
 * Parameters:	lnr - sn0net ring to queue message on
 *		d - pointer to data to send
 *		l - number of bytes.
 * Returns:	0 - All OK, message sent.
 *		ENOSPAC - if not enough room but should be 
 *		    sent as "big" message
 *		errno - other errno that should be passed back to 
 *		    user.
 */
{
    lnMsg_t	*lnm;			/* message header */
    int		errno;

    ASSERT(l <= LN_IDATA_CNT);
    DPRINTF(("lnSendImmediate: lnr[0x%x] d[0x%x] l[%d]\n", 
	     lnr, d, l));

    lnm = lnAllocSendMsg(lnr, B_TRUE);
    if (NULL == lnm) {
	DPRINTF(("lnSendImmediate: lnAllocSendMsg: failed\n"));
	errno = ENOTCONN;
    } else {
	/*
	 * Fill in the header information.
	 */
	lnm->lnm_hdr.lnh_type   = LNH_T_IMMEDIATE;
	lnm->lnm_hdr.lnh_count  = l;
	DPRINT_LNR(lnr);
	bcopy((void *)TO_CAC(d), lnm->lnm_imd, l);
	errno = 0;
    }
    return(errno);
}

static	int
lnConnect(lnr_t *lnr)
/*
 * Function: 	lnConnect
 * Purpose:	To synchronously establish a connection 
 * Parameters:	lnr - ring to set up connection
 * Returns:	errno
 * Notes:	Assumes a context, and may sleep
 */
{
    lnCntl_t	lnc;
    int		errno, r;		/* return value/wait result */

    DPRINTF(("lnConnect: lnr[0x%x]\n", lnr));
    ASSERT(LN_IS_LOCKED(lnr));

    lnr->lnr_flags |= LNR_F_OPEN;

    lnc.lnc_cmd = LN_CNTL_OPEN;
    lnc.lnc_link= lnr->lnr_link;
    lnc.lnc_p[0]= (paddr_t)lnr->lnr_lxp; /* pointer to X-partition struct */
    lnc.lnc_p[1]= (paddr_t)lnr->lnr_rxp; /* Remote pointer */
    lnc.lnc_p[3]= (__uint64_t)lnr->lnr_target;	/*target */ 

    LN_LOCK(lnr->lnr_control);
    errno = lnSendControl(lnr->lnr_control, &lnc);
    LN_UNLOCK(lnr->lnr_control);

    if (0 != errno) {
	return(errno);
    }

    /*	
     * At this point, there are 2 possible actions - if the remote side 
     * has already attempted a connection, then the link is now available.
     * If not, then we must sleep waiting for the remote end to connect.
     */

    while ((0 == errno) && !(lnr->lnr_flags & LNR_F_AVAIL)) {
	if (!(lnr->lnr_flags & LNR_F_ROPEN)) {
	    DPRINTF(("lnConnect: lnr[0x%x] waiting for remote\n", lnr));
	    LN_WRITE_WAIT(lnr, r);
	    LN_LOCK(lnr);
	    if (0 != r) {			/* open was interrupted */
		errno = EINTR;
		break;
	    } 
	} else {
	    lnr->lnr_flags &= ~(LNR_F_OPEN|LNR_F_ROPEN);
	    lnr->lnr_flags |= LNR_F_AVAIL;
	    errno = 0;
	    DPRINTF(("lnConnect: lnr[0x%x] Connection established\n", lnr));
	}
    }
    if (0 == errno) {
	lnr->lnr_rr = (lnMsg_t *)lnr->lnr_rxp->xp_ring;
    }

    return(errno);
}

static	void
lnShutdownLink(lnr_t *lnr, boolean_t remote)
/*
 * Function: 	lnShutdownLink
 * Purpose:	
 * Parameters:	lnr - pointer to ring to shut down.
 *		remote - if true, request came from remote.
 * Returns: 	nothing
 * 
 * Locks held on entry:	LN_LOCK
 * Locks held on exit:	none
 * Locks aquired:	LN_LOCK on control ring if not requested by remote
 *			end of link.
 */
{
    lnCntl_t	lnc;

    ASSERT(LN_IS_LOCKED(lnr));
    DPRINTF(("lnShutdownLink: lnr[0x%x] flags[0x%x] remote(%s)\n", 
	     lnr, lnr->lnr_flags, remote ? "true" : "false"));
    
    if (lnr->lnr_flags & (LNR_F_RDPOLL|LNR_F_WRPOLL)) {
	pollwakeup(lnr->lnr_ph, POLLERR);
    }

    if (remote) {
	if (lnr->lnr_flags & LNR_F_AVAIL) {
	    /* Only if opened do we do this */
	    lnr->lnr_flags |= LNR_F_RSHUTDOWN; /* remote request */
	    (void)LNR_RXP_CACHE(lnr);
	    LN_WRITE_GO(lnr); /* if writer/closer waiting */
	    LN_READ_GO(lnr); /* if readers waiting */
	    DPRINTF(("lnShutdownLink: AVAIL: flags->0x%x\n", lnr->lnr_flags));
	}
	if (lnr->lnr_flags & LNR_F_SHUTDOWN) {
	    DPRINTF(("lnShutdownLink: SHUTDOWN: flags->0x%x\n", lnr->lnr_flags));
	    lnFreeRing(lnr);
	} else {
	    DPRINTF(("lnShutdownLink: flags->0x%x\n", lnr->lnr_flags));
	    LN_READ_GO(lnr); /* Wake up readers */
	    LN_WRITE_GO(lnr); /* Wake up writers */
	    LN_UNLOCK(lnr);
	}
	return;
    } 

    ASSERT(lnr->lnr_link != LN_CONTROL_LINK); /* can't handle this yet */

    if (lnr->lnr_link == LN_CONTROL_LINK) {
	/*
	 * If shutting down the control link, be sure to remove 
	 * reference in XP area to it.
	 */
	ln_xpMap->xpm_xpe[lnr->lnr_partid].xpe_control = 0;
    }

    lnr->lnr_flags |= LNR_F_SHUTDOWN;	/* Say shutting down */

    if (lnr->lnr_flags & (LNR_F_AVAIL|LNR_F_ROPEN|LNR_F_RSHUTDOWN)) {
	lnc.lnc_cmd = LN_CNTL_SHUTDOWN;
	lnc.lnc_link= lnr->lnr_link;
	LN_UNLOCK(lnr);			/* Release before control locked */
	LN_LOCK(lnr->lnr_control);
	/*
	 * If it doesn't work - who cares. %%%% CONTROL RING FULL ???? RACE 
	 * FOR OPEN
	 */
	(void)lnSendControl(lnr->lnr_control, &lnc);
	LN_UNLOCK(lnr->lnr_control);
	LN_LOCK(lnr);			/* Lock ring again %%% WHAT IF CHANGE*/
    }

    if ((lnr->lnr_flags & LNR_F_RSHUTDOWN) ||
	!(lnr->lnr_flags & (LNR_F_AVAIL|LNR_F_ROPEN))) {
	DPRINTF(("lnShutdownLink: RSHUTDOWN: flags->0x%x\n", lnr->lnr_flags));
	lnFreeRing(lnr);
    } else {
	LN_UNLOCK(lnr);
    }

    DPRINTF(("lnShutdownLink: return: flags->0x%x\n", lnr->lnr_flags));
    return;
}


int
lnwrite(dev_t dev, uio_t *uio)
/*
 * Function: 	lnwrite
 * Purpose:	System call interface to send data on raw sn0net device
 * Parameters:	dev - device wo send data on
 *		uio - user io list describing data to send.
 * Returns:	errno
 */
{
    lnr_t	*lnr;
    int		errno;			/* return value/wait result */

    lnr = (lnr_t *)device_info_get(dev);
    LNR_CHECK(lnr);

    DPRINTF(("lnwrite: lnr[0x%x] uio[0x%x]\n", lnr, uio));

    if (!(lnr->lnr_flags & LNR_F_AVAIL) ||
	(lnr->lnr_flags & LNR_F_RSHUTDOWN)) {
	return(ENOTCONN);
    }
    errno = lnSendMessage(lnr, uio);
    return(errno);
}

static	int
lnOpenControl(lnr_t *lnr)
/*
 * Function: lnOpenControl
 * Purpose:  Open the control "channel" to the remote partition
 * Parameters: 	lnr - pointer to control ring.
 * Returns: 	0 - OK, control port open
 *		!0- errno
 */
{
    int		errno = 0, r;

    DPRINTF(("lnOpenControl: lnr(0x%x)\n", lnr));

    LN_LOCK(lnr);
    if ((lnr->lnr_flags&LNR_F_AVAIL) && (!(lnr->lnr_flags&LNR_F_SHUTDOWN))){
	/* Already open */
	LN_UNLOCK(lnr);
	return(0);
    }
    

    errno = lnInitRing(lnr, LN_CONTROL_SZ);
    DPRINTF(("lnOpenControl: lnr(0x%x): Allocating ring: %d\n", lnr, errno));
    if (0 != errno) {
	LN_UNLOCK(lnr);
	return(errno);
    }

    /* Set up xp */

    ln_xpMap->xpm_xpe[lnr->lnr_partid].xpe_control = K0_TO_PHYS(lnr->lnr_lxp);
    DPRINTF(("Setting part[%d] = 0x%x\n", lnr->lnr_partid, 
	     K0_TO_PHYS(lnr->lnr_lxp)));
    DPRINT_LNR(lnr);

    lnr->lnr_flags |= LNR_F_OPEN;
    if (!(lnr->lnr_flags & LNR_F_ROPEN)) {
	/*
	 * Remote has not talked to us yet, so interrupt and sleep waiting
	 * for a reply.
	 */
	LNR_SEND_INT(lnr);
	DPRINTF(("lnOpenControl: Waiting for remote connect\n"));
	LN_WRITE_WAIT(lnr, r);
	DPRINTF(("lnOpenControl: Remote connection received.\n"));
	LN_LOCK(lnr);
	if (0 != r) {
	    lnShutdownLink(lnr, B_TRUE);
	    return(EINTR);
	}
    }

    /*
     * If here - then both sides agree, and the XP structure should
     * be set up. Pick up the values and go, and continue.
     */
    errno = lnFindRemoteControlRing(lnr);
    if (0 != errno) {
	lnShutdownLink(lnr, B_TRUE);
	return(errno);
    }
    DPRINTF(("lnOpenControl: RXP[0x%x]\n", lnr->lnr_rxp));
    DPRINT_LNR(lnr);
    if (NULL == lnr->lnr_rxp) {	/* not set up yet */
	lnr->lnr_flags &= ~LNR_F_OPEN;
	LN_UNLOCK(lnr);
	return(ENODEV);
    }
    /* Now suck over the data */
    if (LNR_RXP_CACHE(lnr)) {
	lnr->lnr_flags |= LNR_F_ERROR;	/* %%% EXIT %%% */
    }

    lnr->lnr_rr    = (lnMsg_t *)lnr->lnr_crxp.xp_ring;
#if DEBUG
    if (!lnr->lnr_rr) {
	extern void debug(char *);
	debug("ring");
    }
#endif

    /* 
     * Verify the data we have as being consistent.
     */
    lnr->lnr_flags &= ~(LNR_F_OPEN|LNR_F_ROPEN);
    lnr->lnr_flags |= LNR_F_AVAIL;

    DPRINTF(("lnOpenControl: ROPEN set --- connected RR=0x%x\n", lnr->lnr_rr));

    if (!(lnr->lnr_flags & LNR_F_AVAIL) ||
	(lnr->lnr_flags & LNR_F_ERROR)) {
	errno = ENODEV;
    }

    DPRINTF(("lnOpenControl: Complete *******************\n"));
    LN_UNLOCK(lnr);
    return(errno);
}

int
lnpoll(dev_t dev, short events, int anyyet, short *reventsp, 
       struct pollhead **phpp, unsigned int *genp)
{
    lnr_t	*lnr;

    DPRINTF(("lnpoll: dev(0x%x) events(0x%x) anyyet(0x%x)\n", 
	     dev, events, anyyet));

    lnr = (lnr_t *)device_info_get(dev);
    
    LN_LOCK(lnr);
    if (!(lnr->lnr_flags & LNR_F_AVAIL) || 
	(lnr->lnr_flags & (LNR_F_SHUTDOWN|LNR_F_RSHUTDOWN|LNR_F_ERROR))) {
	LN_UNLOCK(lnr);
	*reventsp = POLLHUP;
	return(ENOTCONN);
    }

    if (LNR_RXP_CACHE(lnr)) {
	LN_UNLOCK(lnr);
	lnPartitionDead(lnr);
	return(EIO);
    }

    if (events & (POLLIN | POLLRDNORM)) { 
	if (!LNR_PEND(lnr)) {		/* pending stuff on ring */
	    events &= ~(POLLIN | POLLRDNORM);
	    lnr->lnr_flags |= LNR_F_RDPOLL;
	}
    }

    if (events & (POLLOUT | POLLWRNORM)) {
	if (!LNR_AVAIL(lnr)) {		/* room to send */
	    events &= ~(POLLOUT | POLLWRNORM);
	    lnr->lnr_flags |= LNR_F_WRPOLL;
	}
    }
    *reventsp = events;
    if (!anyyet && (0 == events)) {
	*phpp = lnr->lnr_ph;
	/* snapshot pollhead generation number while we hold the lock */
	*genp = POLLGEN(&lnr->lnr_ph);
    }
    LN_UNLOCK(lnr);
    DPRINTF(("lnpoll: Returning events: 0x%x\n", events));
     return(0);
}


/* ARGSUSED */
int
lnopen(dev_t *dev, int flag)
/*
 * Function: 	lnopen
 * Purpose:	System call interface to "write" on a sn0net ring.
 * Parameters:	dev - pointer to device
 *		flag - 
 * Returns:	0 on success, errno otherwise.
 */
{
    lnr_t	*lnr;
    int		errno, r;

    DPRINTF(("lnopen: dev(0x%x) flag(0x%x)\n", *dev, flag));

    lnr = (lnr_t *)device_info_get(*dev); /* Find our ring */
    errno = 0;

    DPRINTF(("lnopen: lnr[0x%x] flags[0x%x]\n", 
	     lnr, lnr->lnr_flags));

    LN_WRITE_LOCK(lnr, r);
    if (r) {
	return(EINTR);
    }
    LN_LOCK(lnr);

    if (lnr->lnr_flags & (LNR_F_SHUTDOWN|LNR_F_RSHUTDOWN)) {
	/*
	 * If shutdown not complete on this link - reject open request.
	 */
	LN_UNLOCK(lnr);
	LN_WRITE_UNLOCK(lnr);
	return(EBUSY);
    }

    /* Check if we need to init everything. */

    if (!(lnr->lnr_flags & LNR_F_AVAIL)) {
	/*
	 * First figure out if we can open the control ring - which can cause
	 * us to sleep - so we must release the LN_LOCK. This is OK since we
	 * are still holding the "write" lock to serialize opens.
	 */
	LN_UNLOCK(lnr);	/* following call can sleep */
	errno = lnOpenControl(lnr->lnr_control);
	if (0 != errno) {
	    LN_WRITE_UNLOCK(lnr);
	    return(errno);
	}

	LN_LOCK(lnr);
	/* Try and create it from here */
	errno = lnInitRing(lnr, 0x4000);
	if (0 != errno) {
	    LN_WRITE_UNLOCK(lnr);
	    LN_UNLOCK(lnr);
	    return(errno);
	}
	errno = lnConnect(lnr);
	if (0 != errno) {		/* clean up */
	    /*
	     * If we failed, shutdown is called with remote==TRUE because we
	     * do not want to send a message to the remote end.
	     */
	    lnShutdownLink(lnr, B_TRUE);
	    LN_WRITE_UNLOCK(lnr);	/* %%%%% CAN I DO THIS */
	    return(errno);
	}
    }

    LN_UNLOCK(lnr);
    LN_WRITE_UNLOCK(lnr);
    return(errno);
}    

#if 0
int
lnioctl(dev_t dev, int cmd, void *arg, int mode, struct cred *crp, int *rval)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    lnr_t	*lnr;
    lnCntl_t	lnc.

    lnr = (lnr_t *)device_info_get(dev);

    switch(cmd) {
    case FIONBIO:			/* set/clear non blocking I/O */
	break;
    case FIOASYNC:			/* ASYNC I/O Flag */
	break;
    case LN_CPUAFF:
	lnr->lnr_cpuaff = XXX;
	lnc.lnc_cmd = LN_CNTL_TARGET;
	lnc.lnc_link= lnr->lnr_link;
	lnc.lnc_p[0]= CPUID_TO_NASID(XXXX);
	return(lnSendControl(lnr, &lnc));
    default:
	return(EINVAL);
    }
    
}
    
#endif

int
lnclose(dev_t dev)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    lnr_t	*lnr;

    lnr = (lnr_t *)device_info_get(dev);
    ASSERT(NULL != lnr);

    LN_LOCK(lnr);
    
    /*
     * lnShutdown will unlock the ring, or possibly free it up if 
     * the remote end has already signalled a close.
     */
    lnShutdownLink(lnr, B_FALSE);
    return(0);
}
