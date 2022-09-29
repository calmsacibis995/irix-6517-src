/*
 * File: xpc.c
 * Purpose: Celular IRIX Cross Partition Communication Support.
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

#include "sys/debug.h"
#include "sys/types.h"
#include "sys/uio.h"
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
#include "sys/errno.h"

#include "ksys/xthread.h"
#include "ksys/sthread.h"
#include "ksys/xpc.h"
#include "ksys/partition.h"

#if SN
#include "sys/SN/arch.h"
#include "sys/SN/addrs.h"
#include "sys/SN/klpartvars.h"
#if defined (SN0)
#include "sys/SN/SN0/bte.h"
#include "sys/SN/SN0/sn0.h"
#endif
#include "sys/SN/intr.h"
#endif

#include "xpc_private.h"

extern	int	xpc_override;

static	void	xpc_debug(char *, ...);

static	int	xpc_pull_data_fast(void *, paddr_t, void *, size_t);
static	int	xpc_pull_data(void *, paddr_t, void *, size_t);
static	volatile hubreg_t *xpc_find_target(partid_t);
static	xpc_t	xpc_swait(xpr_t *);
static	xpc_t	xpc_rwait(xpr_t *);
static	xpc_t	xpc_get_remote(partid_t);
static	void	xpc_part_activate(partid_t);
static 	void	xpc_control_interrupt(void *, int);
static	void	xpc_part_deactivate(partid_t);
        void	xpc_init(void);
static	xpc_t	xpc_allocate_xpm(xpr_t *, xpm_t **);
        void	xpc_done(xpr_t *, xpm_t *);
        xpc_t	xpc_allocate(xpr_t *, xpm_t **);
        xpc_t	xpc_unallocate(xpr_t *, xpm_t *);
static	boolean_t xpc_check_notify_complete(xnq_t *, __uint64_t, short);
static	void	xpc_notify_continue(xpr_t *, __uint64_t, short);
static	xpc_t	xpc_notify_wait(xpr_t *, xpm_t *, xpc_async_rtn_t, void *);
static	void	xpc_partition_dead(xpc_t, xpr_t *);
        xpc_t	xpc_send(xpr_t *, xpm_t *);
        xpc_t	xpc_send_control(xpr_t *, unsigned char, xpc_t, __uint64_t, 
				 __uint64_t, __uint64_t);
static	void	xpc_set_error(const int line, xpr_t *xpr, xpc_t xr);
static	void	xpc_setup_new_xpr(xpr_t *, xpr_t *, partid_t, __uint32_t);
static	void	xpc_free_ring_buffers(xpr_t *);
static	void	xpc_allocate_ring_buffers(xpr_t *);
static	void	xpc_connect_timeout(pr_t *);
        void	xpc_free(xpr_t *, xpm_t *);
	void 	xpc_disconnect(xpr_t *);
	xpc_t	xpc_connect(partid_t, int, __uint32_t, __uint32_t, 
			    xpc_async_rtn_t, __uint32_t, __uint32_t,
			    struct xpr_s **);
static	void	xpc_process_disconnect(xpr_t *);
static	xpc_t	xpc_process_connect(xpr_t *);
static	void	xpc_process_control(xpc_t, xpr_t *, partid_t, int, void *);
static	void	xpc_interrupt(void *, int);
static	xpc_t	xpc_cache_remote_gp(partid_t);
static  void	xpc_receive_xpm(xpr_t *);
static	void	xpc_pull_xpm(bte_handle_t *, xpr_t *);
        xpc_t	xpc_poll(xpr_t *, short, int, short *, struct pollhead **,
			 unsigned int *genp);

#define	XPC_SET_ERROR(_x, _r)	\
    xpc_set_error(__LINE__, (_x), (_r))

#define	XPC_HUBREV	HUB_REV_2_3

#if defined(DEBUG)
/*
 * May want to turn this on to debug at times, but never in a non-debug 
 * kernel.
 */
#define	_XPC_DEBUG	0
#endif

#if	_XPC_DEBUG
#   define	DPRINTF(_x)		xpc_debug _x
#   define	DPRINTXPR(_s, _x)	xpc_dump_xpr((_s), (_x), xpc_debug)
#   define	DPRINTXPM(_s, _x, _m)	xpc_dump_xpm((_s), (_x), (_m), \
						     xpc_debug)
#else
#   define	DPRINTF(_x)
#   define	DPRINTXPR(_s, _x)
#   define	DPRINTXPM(_s, _x, _m)
#endif

/*
 * Variable: 	xpc_vertex
 * Purpose: 	Defines the vertex to which all of the partition devices
 *		are added.
 */
static	vertex_hdl_t	xpc_vertex;

static	pr_t		xpc_rings[MAX_PARTITIONS];
static	xp_map_t	*xpc_map;

/*
 * Variable:	xpc_decode
 * Purpose:	Maps character names to the xpc_t return values. This 
 *		array is exported.
 */
const char *xpc_decode[] = {
    "xpcSuccess", "xpcDone", "xpcTimeout", "xpcError", "xpcPartid", 
    "xpcChannel", "xpcAsync", "xpcAsycUnknown", "xpcIntr", 
    "xpcDisconnected", "xpcConnected", "xpcFull", "xpcBusy", "xpcRetry",
    "xpcVersion", "xpcNoOvrd", "xpcConnectInProg"
};

/*
 * Variable:	xpc_errno
 * Purpose:	Maps a rough xpc to errro, index with xpc_t to translate
 *		to errno.
 */
const int  xpc_errno[] = {
    0,					/* xpcSuccess */
    0,					/* xpcDone */
    EIO,				/* xpcTimeout */
    EIO,				/* xpcError */
    ENODEV,				/* xpcPartid */
    ENODEV,				/* xpcChannel */
    EPROTO,				/* xpcAsync */
    EPROTO,				/* xpcAsyncUnkown */
    EINTR,				/* xpcIntr */
    ENOTCONN,				/* xpcDisconnected */
    0,					/* xpcConnected */
    EWOULDBLOCK,			/* xpcNoWait */
    EBUSY,				/* xpcBusy */    
    EAGAIN,				/* xpcRetry */
    ENOTSUP,				/* xpcVersion */
    ENOTSUP,				/* xpcNoOvrd */
    EISCONN				/* xpcConnectInProg */
};

/*VARARGS*/
static	void
xpc_debug(char *s, ...)
/*
 * Function: 	xpc_debug
 * Purpose:	To print debug information on the console.
 * Parameters:	s   - template
 *		... - printf style arguments.
 * Returns:	Nothing
 */
{
    va_list	ap;

    va_start(ap, s);
    icmn_err(CE_CONT, s, ap);
    va_end(ap);
}

void
xpc_dump_xpe(char *s, void (*printf)(char *, ...))
/*
 * Function: 	xpc_dump_xpe
 * Purpose:	Dump all the cross partition maps.
 * Parameters:	s - string ot prefix lines with
 *		print - print routine.
 * Returns: nothing
 */
{
    int		i, c;
    xpe_t	*x;
    pr_t	*pr;
    xpr_t	*xpr;

    if (!xpc_map) {
	printf("%s: no partition maps\n", s);
	return;
    }

    for (i = 0; i < MAX_PARTITIONS; i++) {
	x = &xpc_map->xpm_xpe[i];
	pr = &xpc_rings[i];
	if (pr->pr_rflags & XPMP_F_VALID) {
	    printf("%s:Partid(%d) V%d.%d F(0x%x) <", 
		   s, i, XPC_VERSION_MAJOR(pr->pr_rversion), 
		   XPC_VERSION_MINOR(pr->pr_rversion), pr->pr_rflags);
	    if (pr->pr_rflags & XPMP_F_VALID) printf("Valid ");
	    if (pr->pr_rflags & XPMP_F_HR) printf("DR_hub ");
	    if (pr->pr_rflags & XPMP_F_HROVRD) printf("DR_ovrd ");
	    printf(">\n");

	    printf("%s:\t XPE C(0x%x) gp(0x%x) c(%d)\n",
		   s, pr->pr_xpe.xpe_control, pr->pr_xpe.xpe_gp, 
		   pr->pr_xpe.xpe_cnt);
	}
	if (x->xpe_control) {
	    pr = &xpc_rings[i];
	    printf("%s:\t GP status: id(%d) busy(%d) wait(%d) p_int_id(%d)\n", 
		   s, pr->pr_ggp_id, pr->pr_ggp_busy, pr->pr_ggp_waitid, 
		   part_interrupt_id(i));
	    printf("%s:\t GP stats:  cnt(%d) pcnt(%d) wcnt(%d)\n", 
		   s, pr->pr_ggp_cnt, pr->pr_ggp_pcnt, pr->pr_ggp_wcnt);
	    if (pr->pr_xpr) {
		for (c = 0; c < x->xpe_cnt; c++) {
		    xpr = pr->pr_xpr + c;
		    if (xpr->xpr_flags) { /* might be interesting */
			printf("%s:\t channel(%d) xpr(0x%x)\n", 
			       s, c, pr->pr_xpr + c);
		    }
		}
	    }
	}
    }
}

static void xpc_debug_get_flag_string(__uint32_t flags, char *flag_st)
/*
 * Function: 	xpc_debug_get_flag_string
 * Purpose:	concats all a string for each flag onto the flag string
 * Parameters:	flags	- the flags field that we are printing out
 *		flag_st - string to concat flag text to.
 * Returns:	Nothing
 */
{
    if (flags & XPR_F_SHUTDOWN)		strcat(flag_st, "SHUTDOWN ");
    if (flags & XPR_F_RSHUTDOWN)	strcat(flag_st, "RSHUTDOWN ");
    if (flags & XPR_F_CONNECT)		strcat(flag_st, "CONNECT ");
    if (flags & XPR_F_RCONNECT)		strcat(flag_st, "RCONNECT ");
    if (flags & XPR_F_REPLY)		strcat(flag_st, "REPLY ");
    if (flags & XPR_F_RREPLY)		strcat(flag_st, "RREPLY ");
    if (flags & XPR_F_ERROR)		strcat(flag_st, "ERROR ");
    if (flags & XPR_F_ALLOC)		strcat(flag_st, "ALLOC ");
    if (flags & XPR_F_AVAIL)		strcat(flag_st, "AVAIL ");
    if (flags & XPR_F_OPEN)		strcat(flag_st, "OPEN ");
    if (flags & XPR_F_AOPEN)		strcat(flag_st, "AOPEN ");
    if (flags & XPR_F_NONBLOCK)		strcat(flag_st, "NONBLOCK ");
    if (flags & XPR_F_BWAIT)		strcat(flag_st, "BWAIT ");
    if (flags & XPR_F_DRAINED)		strcat(flag_st, "DRAINED ");
    if (flags & XPR_F_BUSYCON)		strcat(flag_st, "BUSYCON ");
    if (flags & XPR_F_BUSYDIS)		strcat(flag_st, "BUSYDIS ");
    if (flags & XPR_F_ERRBUSY)		strcat(flag_st, "ERRBUSY ");
    if (flags & XPR_F_CACHEBUSY)	strcat(flag_st, "CACHEBUSY ");
    if (flags & XPR_F_REPLYPEND)	strcat(flag_st, "REPLYPEND ");
    if (flags & XPR_F_FREEPEND)		strcat(flag_st, "FREEPEND ");
    if (flags & XPR_F_RDPOLL)		strcat(flag_st, "RDPOLL ");
    if (flags & XPR_F_WRPOLL)		strcat(flag_st, "WRPOLL ");
    if (flags & XPR_F_XPOLL)		strcat(flag_st, "XPOLL ");
    if (flags & XPR_F_RD_WAIT)		strcat(flag_st, "RD_WAIT ");
    if (flags & XPR_F_WR_WAIT)		strcat(flag_st, "WR_WAIT ");
    if (flags & XPR_F_NOTIFY)		strcat(flag_st, "NOTIFY ");
}

#if _XPC_HISTORY_DEBUG
static void xpc_debug_get_history_string(xpm_history_t history, 
					 char *history_st)
/*
 * Function: 	xpc_debug_get_history_string
 * Purpose:	concats all the info about the history structure 
 *		onto the history string.
 * Parameters:	history		- history info to report
 *		history_st	- history string to build on
 * Returns:	Nothing
 */
{
    history_st[0] = '\0';
    switch(history.xpm_history_type) {
    case XPM_HISTORY_PRE_SEND:	  strcat(history_st, "Pre-send");    break;
    case XPM_HISTORY_POST_SEND:   strcat(history_st, "Post-send");   break;
    case XPM_HISTORY_RECEIVE:     strcat(history_st, "Receive");     break;
    case XPM_HISTORY_CONNECT:	  strcat(history_st, "Connect");     break;
    case XPM_HISTORY_DISCONNECT:  strcat(history_st, "Disconnect");  break;
    }
    strcat(history_st, ": Cmd <");
    switch(history.xpm_history_cmd) {
    case XPMC_C_OPEN:           strcat(history_st, "OPEN");        break;
    case XPMC_C_OPEN_REPLY:     strcat(history_st, "OPEN_REPLY");  break;
    case XPMC_C_SHUTDOWN:       strcat(history_st, "SHUTDOWN");    break;
    default:                    strcat(history_st, "OTHER");       break;
    }
    strcat(history_st, ">; Xr <");
    sprintf(&(history_st[strlen(history_st)]), "%d", history.xpm_history_xr);
    strcat(history_st, ">; XPR Flags <");
    xpc_debug_get_flag_string(history.xpm_history_flags, history_st);
    strcat(history_st, ">\n");
}
#endif /* _XPC_HISTORY_DEBUG */

void
xpc_dump_xpr(char *s, xpr_t *xpr, void (*printf)(char *, ...))
/*
 * Function: 	xpc_dump_xpr
 * Purpose:	Display inforamtion about a ring.
 * Parameters:	s 	- string indicating source of call
 *		xpc 	- pointer to ring to display information on.
 *		printf 	- pointer to print routine to call.
 * Returns:	Nothing
 */
{
    char	sb[256];
    xpm_t	*xpm;
	
    sprintf(sb, 
	    "%s: Partid(%d) Channel(%d): Magic(%s) XPR=0x%x\n", 
	    s, xpr->xpr_partid, xpr->xpr_channel, 
	    xpr->xpr_magic==XPR_MAGIC ? (char *)&xpr->xpr_magic : "*invalid*",
	    xpr);
    printf(sb);
    sprintf(sb,"\tFlags: 0x%x <", xpr->xpr_flags);
    xpc_debug_get_flag_string(xpr->xpr_flags, sb);
    
    strcat(sb, ">\n");
    printf(sb);

    printf("\tithreads: Maximum(%d) Current(%d) Pending(%d) Busy(%d)\n", 
	   xpr->xpr_ithreads_max,  xpr->xpr_ithreads_active, 
	   xpr->xpr_ithreads_pending, xpr->xpr_ithreads_busy);
    printf("\tRings: Local(0x%x) Remote(0x%x) Cache_remote(0x%x)\n", 
	   xpr->xpr_lr, xpr->xpr_rr, xpr->xpr_crr);
    printf("\tasyncf: 0x%x Target: 0x%x Limit: l(%d) r(%d) \n",
	   xpr->xpr_async_rtn, xpr->xpr_target, xpr->xpr_llimit, 
	   xpr->xpr_rlimit);
    printf("\tXP LGP<0x%x> RGP<0x%x>\n", xpr->xpr_lgp, xpr->xpr_rgp);

    if (xpr->xpr_flags & XPR_F_ERROR) {
	printf("\tError: %s(%d) Line: %d \n", xpc_decode[xpr->xpr_xr], 
	       xpr->xpr_xr, xpr->xpr_xr_line);
    }

    if (xpr->xpr_lgp && xpr->xpr_rgp) {
	printf("\t\t Local: wget(%d) get(%d) wput(%d) put(%d) wrap(%d)\n", 
	       xpr->xpr_wlgp.gp_get, xpr->xpr_lgp->gp_get, 
	       xpr->xpr_wlgp.gp_put, xpr->xpr_lgp->gp_put,
	       xpr->xpr_lwrap);
	printf("\t\t Remote: wget(%d) get(%d) wput(%d) put(%d) wrap(%d)\n", 
	       xpr->xpr_wrgp.gp_get, xpr->xpr_rgp->gp_get, 
	       xpr->xpr_wrgp.gp_put, xpr->xpr_rgp->gp_put,
	       xpr->xpr_rwrap);
	printf("\t\t Cache Remote Ring: get(%d) put(%d)\n", 
	       xpr->xpr_crr_gp.gp_get, xpr->xpr_crr_gp.gp_put);
	printf("\t\t Avail (%d) Pend(%d) CRR_pend(%d)\n", 
	       XPR_AVAIL(xpr), XPR_PEND(xpr), XPR_CRR_PEND(xpr));
    } 
    printf("\t\t Count: send(%d) sooo(%d) recv(%d) "
	   "xpmpull(%d) alloc_wait(%d)\n",
	   xpr->xpr_sendcnt, xpr->xpr_sendooocnt, xpr->xpr_rcvcnt,
	   xpr->xpr_xpmcnt, xpr->xpr_allcnt);
    if (xpr->xpr_xn) {
	xnq_t	*xn;
	for (xn = xpr->xpr_xn; xn; xn = xn->xn_next) {
	    printf("\t\txn: wrap(%d) put(%d) sync@(0x%x)\n", 
		   xn->xn_wrap, xn->xn_put, &xn->xn_sv);
	}
    }
    
    if (xpr->xpr_done) {
	printf("\t\tActive Incoming: ");
	for (xpm = xpr->xpr_done; xpm; xpm = xpm->xpm_ptr) {
	    printf("0x%x ", xpm);
	}
	printf("\n");
    }
    if (xpr->xpr_ready) {
	printf("\t\tActive Outgoing: ");
	for (xpm = xpr->xpr_ready; xpm; xpm = xpm->xpm_ptr) {
	    printf("0x%x ", xpm);
	}
	printf("\n");
    }	

#if _XPC_HISTORY_DEBUG
    {
        int i;
        char string[512];
        if(xpr->xpr_history_wrapped) {
            for(i = xpr->xpr_history_index; i < XPM_HISTORY_MAX; i++) {	
                xpc_debug_get_history_string(xpr->xpr_history[i], string);
                printf(string);
            }
        }
        for(i = 0; i < xpr->xpr_history_index; i++) {
            xpc_debug_get_history_string(xpr->xpr_history[i], string);
            printf(string);
        }
    }
#endif /* _XPC_HISTORY_DEBUG */
}

void
xpc_dump_xpm(char *s, xpr_t *xpr, xpm_t *m, void (*printf)(char *, ...))
/*
 * Function: 	xpr_dump_xpm
 * Purpose:	To display information about a message.
 * Parameters:	s	- pointer to string indicating source of call.
 *		xpr 	- pointer to ring message arrived on.
 *		m 	- pointer to message.
 * Returns:	Nothing.
 */
{
    char	sb[256];
    static char *xpm_text[] = {
	"invalid", "control", "data"
    };
    static	char	*cntl_text[] = {
	"invalid", "open", "open-reply", "nop", "target", "shutdown"
    };
    char	*t;
    int		i;
    xpmb_t	*b;
    xpmc_t	*cm;

    if (m->xpm_type < sizeof(xpm_text)/sizeof(xpm_text[0])) {
	t =  xpm_text[m->xpm_type];
    } else {
	t = "*invalid*";
    }

    if (xpr) {
	printf("%s: Partid(%d) Channel(%d): type(%s-%d) f(0x%x) cnt(%d)\n",
	       s, xpr->xpr_partid, xpr->xpr_channel, t, m->xpm_type, 
	       m->xpm_flags, m->xpm_cnt);
    } else {
	printf("%s: type(%s-%d) cnt(%d) f(0x%x) <", 
	       s, t, m->xpm_type, m->xpm_cnt, m->xpm_flags);
	if (m->xpm_flags & XPM_F_INTERRUPT) 	printf("interrupt ");
	if (m->xpm_flags & XPM_F_NOTIFY)	printf("notify ");
	if (m->xpm_flags & XPM_F_SYNC) 		printf("sync ");
	if (m->xpm_flags & XPM_F_DONE) 		printf("done/ready ");
	printf(">\n");
    }
	
    switch(m->xpm_type) {
    case XPM_T_CONTROL:
	b = (xpmb_t *)(m + 1);
	cm = (xpmc_t *)((__psunsigned_t)b + b->xpmb_ptr);
	if (cm->xpmc_cmd < sizeof(cntl_text)/sizeof(cntl_text[0])) {
	    t = cntl_text[cm->xpmc_cmd]; 
	} else {
	    t = "*INVALID*";
	}
	if (xpr) {
	    printf("%s: Partid(%d) CNTL   : cmd<%d-%s> link<%d> error<%d>\n", 
		   s, xpr->xpr_partid, cm->xpmc_cmd, t, cm->xpmc_channel,
		   cm->xpmc_xr);
	    printf("%s: Partid(%d) CNTL   : parms "
		   "<0=0x%x 1=0x%x 2=0x%x 3=0x%x>\n",
		   s, xpr->xpr_partid, cm->xpmc_parm[0], cm->xpmc_parm[1], 
		   cm->xpmc_parm[2], cm->xpmc_parm[3]);
	} else {
	    printf("%s: CNTL : cmd<%d-%s> link<%d> error<%d>\n", 
		   s, cm->xpmc_cmd, t, cm->xpmc_channel, cm->xpmc_xr);
	    printf("%s: CNTL : parms <0=0x%x 1=0x%x 2=0x%x 3=0x%x>\n",
		   s, cm->xpmc_parm[0], cm->xpmc_parm[1], 
		   cm->xpmc_parm[2], cm->xpmc_parm[3]);
	}
	break;
    case XPM_T_DATA:
	for (i = 0; i < m->xpm_cnt; i++) {
	    b = XPM_BUF(m, i);
	    sprintf(sb, "%s:\tcnt(%d) ptr(0x%x,0x%x) f 0x%x(", 
		    s, b->xpmb_cnt, b->xpmb_ptr, 
		    (b->xpmb_flags & XPMB_F_IMD)
		      ?   (__psunsigned_t)b+(__psunsigned_t)b->xpmb_ptr
		      :   b->xpmb_ptr, b->xpmb_flags);
	    if (b->xpmb_flags & XPMB_F_IMD) strcat(sb, "imd ");
	    if (b->xpmb_flags & XPMB_F_PTR) strcat(sb, "ptr ");
	    strcat(sb, ")\n");
	    printf(sb);
	}
	break;
    default:
	break;
    }
}

static __inline xpc_t
xpc_pull_data_fast(void *bte, paddr_t sp, void *dp, size_t l)
/*
 * Function: 	xpc_pull_data_fast
 * Purpose:	Same as pull data but only handles physical
 *		to physical cache line size transfers that are alligned.
 * Parameters:	bte - pointer to bte to use, NULL if none.
 *		sp  - source pointer (PHYS)
 *		dp  - destination pointer (K0 or PHYS)
 *		l   - # of bytes.
 * Returns:	xpcError - failed
 *		xpcSuccess- transfer complete.
 */
{
    bte_handle_t *lbte;
    xpc_t	r;

    ASSERT(XP_ALIGNED(sp) && XP_ALIGNED(dp) && XP_ALIGNED(l));

    if (!bte) {
	if (!(lbte = bte_acquire(1))) {
	    return(xpcError);
	}
    } else {
	lbte = bte;
    }
    r = bte_copy(lbte, sp,(paddr_t)K0_TO_PHYS(dp), l,BTE_NORMAL) 
	    ? xpcError : xpcSuccess;
    if (!bte) {
	bte_release(lbte);
    }
#if DEBUG
    if (r) {
	cmn_err(CE_WARN+CE_CPUID, "xpc_pull_data_fast: bte_bcopy: %s [%d]\n", 
		xpc_decode[r], r);
    }
#endif

    return(r);
}

/* ARGSUSED */
static	xpc_t
xpc_pull_data(void *bte, paddr_t sp, void *dp, size_t l)
/*
 * Function: 	xpc_pull_data
 * Purpose:	To copy data from remote partition.
 * Parameters:	bte - bte ro use as returned from bte_acquire. If null, 
 *		     a bte is aquired for use.
 *		sp - source pointer (across partition)
 *		dp - destination pointer (local partition)
 *		l  - length to move
 * Returns: xpcSuccess - all OK
 *	    xpcError   - transfer failed.
 * Notes: The sender aligns buffer when possible, so on a source buffer that
 *	  is not aligned we do the double copy trick - pull it over with the 
 *	  BTE, and then bcopy it into the target buffer.
 */
{
#define	XP_BTE_ASYNC	0
    xpc_t	r = xpcSuccess;		/* return value */
    int		result, bIdx;
    paddr_t 	b;
    paddr_t 	bteBase, bteOff;
    size_t	pBteLen, bteUnits, units;
    boolean_t	freeBte = B_FALSE;
    char	buffer[XP_PIPE_SIZE * 2 + XP_ALIGN_SIZE];
    paddr_t 	sbp[2];
    

    ASSERT(sp && dp);

    if (!l) {
	return(xpcSuccess);
    }

    if (!bte) {
	if (NULL == (bte = bte_acquire(1))) {
	    return(xpcError);
	}
	freeBte = B_TRUE;
    }
    /*
     * Check for cases we want fast - aligned source/destination/length.
     */
    if (XP_ALIGNED(sp) && XP_ALIGNED(dp) && XP_ALIGNED(l) && 
	(IS_KSEGDM(dp) || IS_KPHYS(dp))) {
	if (bte_copy(bte, sp, (paddr_t)dp, l, BTE_NORMAL)) {
	    r = xpcError;
	} 
    } else {
	/*
	 * 2 copy model
	 *
	 * Using 2 buffers, we attempt to pipeline the data transfer so that
	 * the BTE is transfering into one buffer, while we copy out of the 
	 * other.
	 * 
	 * All transfers must start on an XP_ALIGN boundary for DMA, and 
	 * be a multiple of XP_ALIGN bytes long. 
	 * 
	 * bteUnits - # of XP_ALIGN_SIZE regions left to DMA.
	 * l        - # of bytes left to copy.
	 */

	sbp[0] = kvtophys((void *)(XP_ALIGN(((paddr_t)buffer 
					     + XP_ALIGN_SIZE - 1))));
	sbp[1] = kvtophys((void *)(sbp[0] + XP_PIPE_SIZE));

	bteUnits = XP_UNITS(l + XP_ALIGN_SIZE - 1 + XP_OFFSET(sp));
	bteBase  = XP_ALIGN(sp);
	bteOff	 = XP_OFFSET(sp);

	pBteLen  = 0;
	bIdx	 = 0; 		/* start off using BTE into index 0 */

	/*
	 * For here on, we are going to do cached copies into the 
	 * target address. Convert destination pointer to CAC, if
	 * it is physical.
	 */
	if (IS_KPHYS(dp)) {
	    dp = (void *)PHYS_TO_K0(dp);
	}

	while (bteUnits || l) {
#define	XP_PIPE_UNITS	XP_PIPE_SIZE / BTE_LEN_MINSIZE
	    units = bteUnits > XP_PIPE_UNITS ? XP_PIPE_UNITS : bteUnits;
	    /*
	     * Start up transfer of complete "units" or multiples of 
	     * cache lines.
	     */
	    if (units) {
		b = (paddr_t)sbp[bIdx];
#if XP_BTE_ASYNC
		result = bte_copy(bte, bteBase, b, units * XP_ALIGN_SIZE, 
				  BTE_ASYNC);
#else 
		result = bte_copy(bte, bteBase, b, units * XP_ALIGN_SIZE, 
				  BTE_NORMAL);
#endif
		bteBase += units * XP_ALIGN_SIZE;
	    }
	    /*
	     * If we already have data in one of the buffers, copy it now.
	     */
	    if (pBteLen) {
		bcopy((void *)((paddr_t)PHYS_TO_K0(sbp[!bIdx])+bteOff),
		      (void *)dp, pBteLen);
		l     -= pBteLen;
		dp   = (void *)((paddr_t)dp + pBteLen);
		bteOff = 0;		/* only !0 the first time */
	    }
	    /*
	     * DMA should be done now ...
	     */
	    if (units) {
#if XP_BTE_ASYNC
		result = bte_rendezvous(bte, 0);
#endif
		if (result) {
		    r = xpcError;
		    break;
		}
		bIdx = !bIdx; 	/* flip indication of BTE buffer  */
		bteUnits -= units;
		/* Now set up the pBte values ..... */
		pBteLen = (units * XP_ALIGN_SIZE) - bteOff;
		if (pBteLen > l) {	/* don't spill over */
		    pBteLen = l;
		}
	    }
	}
    }
    if (freeBte) {
	bte_release(bte);
    }

#if DEBUG
    if (r) {
	cmn_err(CE_CPUID+CE_WARN, "xpc_pull_data: bte_bcopy: %s [%d]\n", 
		xpc_decode[r], r);
    }
#endif
    return(r);
}

static	volatile hubreg_t *
xpc_find_target(partid_t p)
/*
 * Function: 	xpc_find_target
 * Purpose:	To locate a target processor is a specific partition.
 * Parameters:	p - partition to locate processor in.
 * Returns:	Hubreg address of CC interrupt regster for a target, 
 */
{
    pn_t	*pn;

    if (!(pn = part_get_nasid(p, 0))) {
	return(0);
    }
    DPRINTF(("xpc_find_target: partGetNasid: %d %x <%s%s%s>\n", 
	     pn->pn_partid, pn->pn_nasid, 
	     pn->pn_cpuA ? "cpuA" : "" , 
	     pn->pn_cpuA && pn->pn_cpuB ? "+" : "", 
	     pn->pn_cpuB ? "cpuB" : ""));
	     
    return(REMOTE_HUB_ADDR(pn->pn_nasid, 
			   pn->pn_cpuA ? PI_CC_PEND_SET_A : PI_CC_PEND_SET_B));
}

static	xpc_t
xpc_swait(xpr_t *xpr)
/*
 * Function: 	xpc_swait
 * Purpose:	Wait for space to become available to send a message.
 * Parameters:	xpr - pointer to ring to wait on.
 * Returns:	xpcSuccess - outgoing buffers available. 
 *		xpcIntr - wait interrupted.
 * Notes:	This routine is overloaded during connection setup. 
 *		SWAIT is slept on when waiting for the connection to 
 *		be set up.
 */
{
    xpc_t	xr = xpcSuccess;

    ASSERT(XPR_LOCKED(xpr));
    if (xpr->xpr_flags & XPR_F_ERROR) {
	xr = xpr->xpr_xr;
	XPR_UNLOCK(xpr);
    } else {
	xpr->xpr_flags |= XPR_F_WR_WAIT;
	if (xpr->xpr_flags & XPR_F_BWAIT) {
	    xr = sv_wait_sig(&xpr->xpr_wrwait_sv, PWAIT|PCATCH, 
			     &xpr->xpr_lock, xpr->xpr_lock_spl)
		? xpcIntr : xpcSuccess;
	} else {
	    sv_wait(&xpr->xpr_wrwait_sv, PZERO, 
		    &xpr->xpr_lock, xpr->xpr_lock_spl);
	}
	/* Check for error that caused us to wake up. No need for lock */
	if (xpr->xpr_flags & XPR_F_ERROR) {
	    xr = xpr->xpr_xr;
	}
    }
    return(xr);
}

static	xpc_t
xpc_rwait(xpr_t *xpr)
/*
 * Function: 	xpc_rwait
 * Purpose:	Wait for a message or error to arrive on a channel.
 * Parameters:	xpr - pointer to ring to wait on.
 * Returns:	xpc_t
 */
{
    xpc_t	xr = xpcSuccess;

    XPR_LOCK(xpr);
    if (xpr->xpr_flags & XPR_F_RSHUTDOWN) {
	XPC_SET_ERROR(xpr, xpcDisconnected);
    } else if (!(xpr->xpr_flags & XPR_F_ERROR) && !XPR_PEND(xpr)) {
	xpr->xpr_flags |= XPR_F_RD_WAIT;
	if (xpr->xpr_flags & XPR_F_BWAIT) {
	    xr = sv_wait_sig(&xpr->xpr_rdwait_sv, PWAIT|PCATCH, 
			     &xpr->xpr_lock, xpr->xpr_lock_spl) 
		? xpcIntr : xpcSuccess;
	} else {
	    sv_wait(&xpr->xpr_rdwait_sv, PZERO, 
		    &xpr->xpr_lock, xpr->xpr_lock_spl);
	}
	if (xpr->xpr_flags & XPR_F_ERROR) {
	    xr = xpr->xpr_xr;
	}
	return(xr);
    } 
    XPR_UNLOCK(xpr);	
    return(xpr->xpr_xr);
}

static	xpc_t
xpc_get_remote(partid_t p)
/*
 * Function: 	xpc_get_remote
 * Purpose:	Get remote xpc parameters.
 * Parameters:	p - partition to feth remote parameters from
 * Returns:	xpc_t
 */
{
    pr_t	*pr = &xpc_rings[p];
    xp_map_t	*xp, rxpm;
			  
    if (part_get_var(p, PART_VAR_XPC, (void *)&xp)) {
	DPRINTF(("xpc_get_remote: invalid XP(%d) xp=0x%x\n", p, xp));
	return(xpcError);
    } else if (!xp) {			/* XP variable not set yet */
	return(xpcRetry);
    }

    /* Check remote XP map is set up, and version number is compatible. */

    if (xpc_pull_data(NULL, (paddr_t)xp, &rxpm, sizeof(xp_map_t))) {
	DPRINTF(("xpc_get_remote: failed to pull XPC_XP\n"));
	return(xpcError);
    }
    if (rxpm.xpm_magic != XPM_MAGIC) {
	/*
	 * Not ready to set up connection yet.
	 */
	DPRINTF(("xpc_get_remote: invalid XPV(%d)\n", p));
	return(xpcRetry);
    } 

    if (!(rxpm.xpm_flags & XPMP_F_VALID)) {
	DPRINTF(("xpc_get_remote: remote XPMAP(%d) not valid\n", p));
	return(xpcRetry);
    }
    pr->pr_rversion = rxpm.xpm_version;
    pr->pr_rflags   = rxpm.xpm_flags;
    pr->pr_xpe 	    = rxpm.xpm_xpe[part_id()];
    pr->pr_rgpAddr  = rxpm.xpm_xpe[part_id()].xpe_gp;
    
    /* Check for downrev Hubs and overrides */

    if (pr->pr_rflags & XPMP_F_HR) {
	if (!(pr->pr_rflags & XPMP_F_HROVRD)) { /* no remote override */
	    cmn_err(CE_NOTE,"Partition %d does not support XPC messaging\n",
		    p);
	    return(xpcNoOvrd);
	}
	DPRINTF(("xpc_get_remote: partid(%d) downrev hubs: OVRD\n", p));
    }

    /* Valid local copy at this point */

    if (!XPC_VERSION_COMP(XPC_VERSION, pr->pr_rversion)) {
	cmn_err(CE_NOTE,"xpc: Remote partition %d requested connection\n", p);
	cmn_err(CE_NOTE,"xpc: Local version %d.%d does not support Remote "
		"version %d.%d\n", 
		XPC_VERSION_MAJOR(XPC_VERSION), 
		XPC_VERSION_MINOR(XPC_VERSION),
		XPC_VERSION_MAJOR(pr->pr_rversion),
		XPC_VERSION_MINOR(pr->pr_rversion));
	return(xpcVersion);
    }

    DPRINTF(("xpc: Local Version %d.%d Remote Version (partition %d) "
	     "%d.%d\n", 
	     XPC_VERSION_MAJOR(XPC_VERSION), 
	     XPC_VERSION_MINOR(XPC_VERSION),
	     p, XPC_VERSION_MAJOR(pr->pr_rversion),
	     XPC_VERSION_MINOR(pr->pr_rversion)));
    return(xpcSuccess);
}

static	void
xpc_part_activate(partid_t p)
/*
 * Function: 	xpc_part_activate
 * Purpose:	A new partition has been activated.
 * Parameters:	p - partition Id that appeared.
 * Returns:	Nothing
 */
{
    static		char _hex[] = "0123456789abcdef";
#define	XPC_HEX(_x, _s)	(_s)[0] = _hex[((_x) >> 4)]; \
                        (_s)[1] = _hex[((_x) & 0xf)];\
                        (_s)[2] = '\0'
    char		ps[3];		/* partition string */
    graph_error_t	ge;
    char		name[MAXPATHLEN];
    int			i;
    dev_t		td;
    pr_t		*pr =  &xpc_rings[p];
    xpr_t		*cxpr;

    ASSERT(p < MAX_PARTITIONS);

    /* Check if we should even talk to this character */

    switch(xpc_get_remote(p)) {
    case xpcVersion:
    case xpcNoOvrd:
	return;				/* Can't talk in this case */
    default:
	break;				/* Don't know - figure out later */
    }

    /* If partition rings already exist, then we are mostly done. */

    if (NULL == (cxpr = pr->pr_xpr)) {		/* not seen before */
	DPRINTF(("xpc_part_activate: Adding new partition %d\n", p));

	/*
	 * Allocate all of the rings as a contiguous chunk of memory.
	 */
	cxpr = kmem_zalloc(sizeof(xpr_t) * XP_RINGS, KM_SLEEP);
	ASSERT(cxpr);			/* We sleep, better get it */

	/* Allocate all the required GET/PUT values */

	pr->pr_xpr  = cxpr;
	pr->pr_lgp  = kmem_zalloc(GP_SIZE, VM_DIRECT|VM_CACHEALIGN|KM_SLEEP);
	pr->pr_rgp  = kmem_zalloc(GP_SIZE, VM_DIRECT|VM_CACHEALIGN|KM_SLEEP);

	GP_MAGIC_SET(*pr->pr_lgp);

	init_spinlock(&pr->pr_ggp_lock, "pr_ggp_lock", (long)p);
	sv_init(&pr->pr_ggp_sv, SV_FIFO, "pr_ggp_sv");

	/* Initialize all of the ring structures - even if not used */

	for (i = 0; i < XP_RINGS; i++) {
	    xpc_setup_new_xpr(cxpr + i, cxpr, p, i);
	}

	/* Now allocate ring buffers for control ring */

	cxpr->xpr_rsize    = XP_CONTROL_ESIZE;
	cxpr->xpr_rlimit   = XP_CONTROL_ELIMIT - 1;
	cxpr->xpr_lsize    = XP_CONTROL_ESIZE;
	cxpr->xpr_llimit   = XP_CONTROL_ELIMIT - 1;
	cxpr->xpr_ithreads_max = 65536;

	xpc_allocate_ring_buffers(cxpr);
	cxpr->xpr_flags |= XPR_F_ALLOC;

	cxpr->xpr_async_rtn= (xpc_async_rtn_t)xpc_process_control;

	/* Fill in map - so other partition can find us */

	xpc_map->xpm_xpe[p].xpe_control	= K0_TO_PHYS(cxpr->xpr_lr);
	xpc_map->xpm_xpe[p].xpe_cnt	= XP_RINGS;
	xpc_map->xpm_xpe[p].xpe_gp	= K0_TO_PHYS(pr->pr_lgp);

	XPC_HEX(p, ps);

	sprintf(name, "%s/control/%s", EDGE_LBL_XPLINK_KERNEL, ps);
	if (GRAPH_SUCCESS != 
	     (ge = hwgraph_path_add(xpc_vertex, name, &td))) {
	    kmem_free(cxpr, sizeof(xpr_t) * XP_RINGS);
	    kmem_free(pr->pr_lgp, GP_SIZE);
	    kmem_free(pr->pr_rgp, GP_SIZE);
	    bzero(pr, sizeof(*pr));
	    cmn_err(CE_WARN, "xpc_part_activate: failed to add %s: %d\n", 
		    name, ge);
	    return;
	}

	/* Set up kernel special RPC rings */

	for (i = XP_RPC_BASE; i < XP_RPC_BASE+XP_RPC_RINGS; i++) {
	    sprintf(name, "%s/rpc/%s/%d", EDGE_LBL_XPLINK_KERNEL, 
		    ps, i - XP_RPC_BASE);
	    if (GRAPH_SUCCESS == hwgraph_path_add(xpc_vertex, name, &td)) {
		device_info_set(td, cxpr);
	    }
	}
    } else {
	cxpr->xpr_xr    = xpcSuccess;
	cxpr->xpr_flags = XPR_F_ALLOC;	/* reset flags */
	DPRINTF(("xpc_part_activate: Adding old partition %d\n", p));
    }

    if (!(cxpr->xpr_target = xpc_find_target(p))) {
	/* pick a target - if it fails, partition must be going down*/
	DPRINTF(("xpc_part_activate: Unable to find remote target p(%d)\n",p));
	return;
    }
    pr->pr_ggp_id   = 0;		/* Set id(count) to 0 */
    pr->pr_ggp_busy = 0;
    pr->pr_ggp_waitid = 0;
    pr->pr_connect_wait = XPR_CONNECT_RATE; /* how long to wait for 1st wait */
    pr->pr_connect_try = XPR_CONNECT_TO; /* # tries to connect */
    pr->pr_ggp_cnt  = 0;
    pr->pr_ggp_pcnt = 0;
    pr->pr_ggp_wcnt = 0;

    /* Set up control ring interrupt handler */

    part_interrupt_set(p, 2, xpc_control_interrupt);

    /* Hold off completion for now */

    XPR_LOCK(cxpr);
    cxpr->xpr_flags |= XPR_F_CONNECT;
    XPR_GP_SET_RESET(cxpr);

    /*
     * There are some race conditions that can cause the interrupts to be 
     * lost due to threads and partitions booting at the same time. To
     * get around this, we use a big stick and retry the connection 
     * interrupt until timeout.
     */
    (void)itimeout(xpc_connect_timeout, pr, pr->pr_connect_wait, pltimeout);
    /* 
     * Send interrupt to remote side to try and establish a connection. If
     * it fails, just let the timeout catch it and clean up.
     */
    XPR_SEND_INT(cxpr);
    /*
     * Wait for the connection to get esablished. This wait is subject 
     * to timeouts...
     */
    (void)xpc_swait(cxpr);
    DPRINTXPR("xpc_part_activate", cxpr);
}

/*ARGSUSED*/
static void
xpc_control_interrupt(void *arg, int idle)
/*
 * Function: 	xpc_control_interrupt
 * Purpose:	Interrupt handler used before control ring is set up.
 *		Handle establishing control ring connection.
 * Parameters:	arg - partition id of interrupting partition.
 *		idle - # idle threads (not used).
 * Returns:	Nothing
 * 
 * Locks held on entry:	None
 * Locks held on exit:	None
 * Locks aquired:	XPR_LOCK
 * 
 * Notes:	Must run as interrupt thread to acquire BTE.
 *
 * Control Ring Interrupt Handshake:
 *
 *  State Transitions: X = no transition, I = invalid, otherwise, new state.
 *  Each transition updates the local ring, then sends an interrupt to the
 *  remote side.
 *
 *  Local\Remote|    INVALID   |     RESET     |     CONNECT     |    READY
 *  ------------+------------------------------------------------+-----------
 *  INVALID	|       I      |       I       |        I	 |    RESET
 *  RESET	|       X      |    CONNECT    |     CONNECT     |    RESET
 *  CONNECT	|     RESET    |    CONNECT    |      READY      |    READY
 *  READY	|     RESET    |     RESET     |      READY      |    READY
 *
 */
{
    partid_t	p = (partid_t)(__psunsigned_t)arg;
    xpr_t	*cxpr;
    pr_t	*pr = &xpc_rings[p];
    bte_handle_t *bte;
    xpc_t	xr;

    cxpr = xpc_rings[p].pr_xpr;		/* control ring pointer */

    XPR_LOCK(cxpr);
    if (cxpr->xpr_flags & (XPR_F_AVAIL|XPR_F_OPEN)) {
	/* 
	 * If another thread got launched into here, don't process
	 * connection setup again.	
	 */
	XPR_UNLOCK(cxpr);
	return;
    }

    XPR_UNLOCK(cxpr);			/* get_remote can use BTE */
    xr = xpc_get_remote(p);		/* Whats up doc? */
    if (xpcRetry == xr) {		/* Don't set error in this case */
	return;
    } else if (xpcSuccess != xr) {
	XPR_LOCK(cxpr);
	XPC_SET_ERROR(cxpr, xr);
	XPR_UNLOCK(cxpr);
	return;
    }
    /* 
     * Grab a BTE now, since we may sleep waiting for one.
     */
    if (!(bte = bte_acquire(1))) {
	return;
    }
    XPR_LOCK(cxpr);

    if (!pr->pr_xpe.xpe_control || 
	(p != part_from_addr(pr->pr_xpe.xpe_control))) {
	DPRINTF(("xpc_control_interrupt: partition %d: "
		 "invalid control setup\n", p));
	XPR_UNLOCK(cxpr);
	bte_release(bte);
	return;
    }

    cxpr->xpr_rr   = (xpm_t *)pr->pr_xpe.xpe_control;
    
    /*
     * Pull over remote GPs, and be sure that the control ring entries are 
     * invalid.
     */
    if (xpc_pull_data_fast(bte, pr->pr_rgpAddr, pr->pr_rgp, GP_SIZE)) {
	XPC_SET_ERROR(pr->pr_xpr, xpcError);
	XPR_UNLOCK(cxpr);
	bte_release(bte);
	return;
    }

    DPRINTF(("xpc_control_interrupt: partid(%d): Lcl<%s> Rmt<%s>\n",
	     cxpr->xpr_partid, XPR_GP_LSTATE(cxpr), XPR_GP_RSTATE(cxpr)));

    if (XPR_GP_TEST_RESET(cxpr)) {
	if (XPR_GP_RTEST_RESET(cxpr) || XPR_GP_RTEST_CONNECT(cxpr)) {
	    XPR_GP_SET_CONNECT(cxpr);
	    XPR_SEND_INT(cxpr);
	}
    } else if (XPR_GP_TEST_CONNECT(cxpr)) {
	if (XPR_GP_RTEST_READY(cxpr)) {
	    XPR_GP_SET_READY(cxpr);
	} else if (XPR_GP_RTEST_CONNECT(cxpr)) {
	    XPR_GP_SET_READY(cxpr);
	} else if (XPR_GP_RTEST_RESET(cxpr)) {
	    XPR_GP_SET_CONNECT(cxpr);
	} else {
	    XPR_GP_SET_RESET(cxpr);
	}
	XPR_SEND_INT(cxpr);
    } else if (XPR_GP_TEST_READY(cxpr)) {
	if (!XPR_GP_RTEST_CONNECT(cxpr) && !XPR_GP_RTEST_READY(cxpr)) {	
	    XPR_GP_SET_RESET(cxpr);
	} 
	XPR_SEND_INT(cxpr);		/* bump him again */
    }

    if (XPR_GP_TEST_READY(cxpr) && XPR_GP_RTEST_READY(cxpr)) {
	part_interrupt_set(p, 32, xpc_interrupt);
	/*
	 * There may or may not be someone waiting for the control 
	 * ring setup to complete. If there is, let them go now.
	 */
	XPR_SGO(cxpr);
	cxpr->xpr_flags &= ~XPR_F_CONNECT;
	cxpr->xpr_flags |= XPR_F_AVAIL | XPR_F_OPEN;
	XPR_GP_RSET_READY(cxpr);	/* valid "working" values */
	XPR_UNLOCK(cxpr);
	(void)part_start_thread(cxpr->xpr_partid, 1, B_TRUE); 
    }  else {
	XPR_UNLOCK(cxpr);
    }
    bte_release(bte);
}

static	void
xpc_part_deactivate(partid_t p)
/*
 * Function: 	xpc_partition_deactivate
 * Purpose:	Process a deactivation notification from the 
 *		partitioning system
 * Parameters:	p - partition id to deactivate
 * Returns:	Nothing
 */
{
    pr_t	*pr = &xpc_rings[p];

    if (pr->pr_xpr) {
	/*
	 * If we know about the partition, then declare it dead. If the
	 * partition system was notified of the error from XPC, then 
	 * the partition_dead code avoids the infinite recursion by
	 * detecting that error recovery is already in progress. The
	 * partition_dead code is called by xpc_set_error. 
	 */
	XPR_LOCK(pr->pr_xpr);
	XPC_SET_ERROR(pr->pr_xpr, xpcDisconnected);
	XPR_UNLOCK(pr->pr_xpr);
    }
}

void
xpc_init(void)
/*
 * Function: 	xpc_init
 * Purpose:	Initialization entry point for cross partition communication.
 * Parameters:	None
 * Returns:	Nothing
 */
{
    graph_error_t	ge;

    DPRINTF(("xpc: Version (%d.%d)\n",
	     XPC_VERSION_MAJOR(XPC_VERSION), XPC_VERSION_MINOR(XPC_VERSION)));

    if (INVALID_PARTID == part_id()) {
	DPRINTF(("xpc_init: partitions disabled\n"));
	return;
    } else if (!bte_avail()) {
	DPRINTF(("xpc_init: bte not available\n"));
	return;
    }
    
    /* Add it to the HW graph */

    ge = hwgraph_path_add(hwgraph_root, EDGE_LBL_XPLINK, &xpc_vertex);
    if (GRAPH_SUCCESS != ge) {
	cmn_err(CE_WARN, "xpc_add_partition: Failed to add: %s: %d\n", 
		EDGE_LBL_XPLINK, ge);	
	return;
    }

    /*
     * Allocate cross partition map, initialize it, and then publish it
     * to allow others to find us.
     */
    xpc_map = (xp_map_t *)kmem_zalloc(sizeof(xp_map_t), VM_DIRECT);

    /* Init the XPMAP */

    xpc_map->xpm_version = XPC_VERSION;
    xpc_map->xpm_flags   = 0;
    xpc_map->xpm_magic   = XPM_MAGIC;

    if (XPC_HUBREV > verify_snchip_rev()) {
	xpc_map->xpm_flags |= XPMP_F_HR;
	if (xpc_override) {
	    cmn_err(CE_NOTE, "XPC Override enabled\n");
	    xpc_map->xpm_flags |= XPMP_F_HROVRD;
	}
#if defined(CELL) && defined(DEBUG)
        else {
	    cmn_err(CE_WARN, "XPC Override Autoset for DEBUG CELL kernel\n");
	    xpc_map->xpm_flags |= XPMP_F_HROVRD;
	}
#endif
    }

    xpc_map->xpm_flags |= XPMP_F_VALID;

    part_set_var(PART_VAR_XPC, 
		 (__psunsigned_t)TO_PHYS((__psunsigned_t)xpc_map));

    /* Set up activation handlers - if local partition can handle messages */

    if (!(xpc_map->xpm_flags&XPMP_F_HR)||(xpc_map->xpm_flags&XPMP_F_HROVRD)) {
	part_register_handlers(xpc_part_activate, xpc_part_deactivate);
    }
}

static	xpc_t
xpc_allocate_xpm(xpr_t *xpr, xpm_t **m)
/*
 * Function: 	xpc_allocate_xpm
 * Purpose:	Allocate a single message entry on the outgoing ring buffer.
 * Parameters:	xpr - ring to allocate message on.
 *		m - pointer to message pointer filled in on return.
 * Returns:	xpc_t
 */
{
    xpm_t	*xpm = NULL;
    xpc_t	xr;
    xnq_t	*xn;

    /* Fast normal path */

    if (XPR_AVAIL(xpr)) {
	xpm = (xpm_t *)XPR_LMSG(xpr);
	xn  = XPR_NQ(xpr);
	xn->xn_wrap = xpr->xpr_lwrap;
	XPR_INC_PUT(xpr);
	xr = xpcSuccess;
    } else if (xpr->xpr_flags & XPR_F_NONBLOCK) {
	return(xpcNoWait);
    } else if (xr = xpc_cache_remote_gp(xpr->xpr_partid)) {
	return(xr);
    } else {
	do {
	    XPR_LOCK(xpr);		/* Block GP updates  */
	    xpr->xpr_allcnt++;
	    /*
	     * Whoever wakes us up will have cached everything we need.
	     * all we ever do here is check if there is some free entries.
	     */
	    if (XPR_AVAIL(xpr)) {
		xpm = (xpm_t *)XPR_LMSG(xpr);
		xn  = XPR_NQ(xpr);
		xn->xn_wrap = xpr->xpr_lwrap;
		XPR_INC_PUT(xpr);
		XPR_UNLOCK(xpr);
	    } else {
		XPR_SUNLOCK(xpr);
		xr = xpc_swait(xpr); /* unlocks ring */
		XPR_SLOCK(xpr);
		if (xpcSuccess != xr) {
		    return(xr);
		}
	    }
	} while (!xpm);
    }

    ASSERT(xpm);
    ASSERT(xpr->xpr_wlgp.gp_put != xpr->xpr_wrgp.gp_get);
    if (XPR_AVAIL(xpr) < ((xpr->xpr_llimit + 1) / 8)) {
	xpm->xpm_flags = XPM_F_INTERRUPT;
    } else {
	xpm->xpm_flags = XPM_F_CLEAR;
    }
    *m = xpm;
    return(xr);
}

static	void
xpc_partition_dead(xpc_t xr, xpr_t *xpr)
/*
 * Function: 	xpc_partition_dead
 * Purpose:	To declare a remote partition "dead" or unusable, 
 *		and attempt to clean up.
 * Parameters:	xr  - error value to set on rings. 
 *		xpr - pointer to "a" ring in the partition we are declaring
 *		      as dead. 
 * Returns:	Nothing.
 * Locks held on entry: NO XPC_LOCK locks. 
 */
{
    int		i;
    xpr_t	*cxpr = xpr->xpr_control,
                *x;

    /* 
     * First - mark the control ring as being in the ERROR state, this 
     * will stop all control messages which in turn will terminate
     * any local opens etc. 
     */
    XPR_LOCK(cxpr);

    /* If the ERROR bit is already set, someone else already beat us to it */

    if (cxpr->xpr_flags & XPR_F_ERROR) {
	XPR_UNLOCK(cxpr);
	return;
    }
    
    DPRINTXPR("xpc_partition_dead", xpr);

    cxpr->xpr_flags  |= XPR_F_ERROR;	/* Mark not avail/open */
    cxpr->xpr_xr     = xr;		/* why not? */
    XPR_GP_SET_ERROR(cxpr);
    GP_SET(cxpr->xpr_crr_gp, 0, 0);

    XPR_UNLOCK(cxpr);

    for (i = 1; i < XP_RINGS; i++) {
	/* 
	 * For every ring that is open, force an error, and wake up any
	 * waiting procs. For any device that is not open, the ring is
	 * freed now since a remote shutdown of the link will never be
	 * seen. The control link must be unlocked to avoid a deadlock
	 * if another thread is trying to aquire the control lock while
	 * holding a normal devices xpr lock. The ERROR flag above will
	 * make sure we only get into here once.
	 */
	x = cxpr + i;
	XPR_LOCK(x);
	/* fake a shutdown if we are open/avail/alloc or we have sent a 
	 ** shutdown message and never received a rshutdown response */
	if ((x->xpr_flags & (XPR_F_OPEN | XPR_F_AVAIL | XPR_F_ALLOC)) ||
	    (x->xpr_flags & XPR_F_SHUTDOWN && 
	     !(x->xpr_flags & XPR_F_RSHUTDOWN))) {
	    x->xpr_flags |= XPR_F_RSHUTDOWN; /* fake "shutdown" */
	    XPC_SET_ERROR(x, xr);
	    xpc_process_disconnect(x);
        } else {
	    XPR_UNLOCK(x);
	}
    }

    /*
     * Be sure to signal all that care about the problem.
     */
    part_deactivate(cxpr->xpr_partid);
    
    /* Set up to accept an interrupt again later */

    part_interrupt_set(cxpr->xpr_partid, 1, xpc_control_interrupt);
}

xpc_t
xpc_send_control(xpr_t *xpr, unsigned char c, xpc_t xr, 
		 __uint64_t p1, __uint64_t p2, __uint64_t p3)
/*
 * Function: 	xpc_send_control
 * Purpose:	To send a control message over the control ring associated 
 *		with the provided ring.
 * Parameters:	xpr - ring the message is associated with (NOT the control
 *		      fing itself).
 *		c   - control command to send.
 *		xr  - xpc error code
 *		p[1-3] - control message parameters.
 * Returns:	xpc_t
 */
{
    xpr_t	*cxpr;			/* control ring pointer */
    xpmc_t	*xpmc;			/* pointer to control message */
    xpmb_t	*xpmb;			/* pointer to xpmb list */
    xpm_t	*xpm;			/* pointer to xp message */
    xpc_t	r;			/* result */

    cxpr = xpr->xpr_control;

    if (xpcSuccess != (r = xpc_allocate(cxpr, &xpm))) {
	return(r);
    }

    xpm->xpm_type      = XPM_T_CONTROL;
    xpm->xpm_cnt       = 1;
    xpm->xpm_flags    |= XPM_F_SYNC;

    xpmb = (xpmb_t *)xpm + 1;		/* Fill in xpmb  */
    
    xpmb->xpmb_cnt     = sizeof(xpmc_t);
    xpmb->xpmb_ptr     = sizeof(xpmb_t);
    xpmb->xpmb_flags   = XPMB_F_IMD;

    xpmc = (xpmc_t *)(xpmb + 1);	/* Fill in xpmc */

    xpmc->xpmc_cmd     = c;
    xpmc->xpmc_channel = xpr->xpr_channel;
    xpmc->xpmc_xr      = xr;
    xpmc->xpmc_parm[0] = p1;
    xpmc->xpmc_parm[1] = p2;
    xpmc->xpmc_parm[2] = p3;

    XPM_HISTORY_STATE(XPM_HISTORY_PRE_SEND, c, xr, xpr);
    r = xpc_send(cxpr, xpm);
    XPM_HISTORY_STATE(XPM_HISTORY_POST_SEND, c, xr, xpr);

    return(r);
}

/*ARGSUSED*/
static	void
xpc_set_error(const int line, xpr_t *xpr, xpc_t xr)
/*
 * Function:	xpc_set_error
 * Purpose:	To post an error to a ring, and reflect it back to 
 *		all who may be waiting.
 * Parameters:	line - Line # called from.
 *		xpr - pointer to ring to post error on.
 *		xr - error code to post.
 * Returns:	Nothing.
 * 
 * Note:	No open is allowed until the error is cleared, and 
 *		the error is ONLY cleared on close.
 *
 *		No message is sent to the remote end of the ring, this is
 * 		used to update local link status only.
 */
{

#if _XPC_DEBUG
    char	b[256];
#endif
    xnq_t	*xn;

    ASSERT(XPR_LOCKED(xpr));
    /*
     * If already error pending, do nothing. 
     */
    if (xpr->xpr_flags & (XPR_F_ERROR|XPR_F_ERRBUSY)) {
	return;
    }
    /* 
     * Declaring control rings dead means we can not rely on xpc rings 
     * at all.
     */
    xpr->xpr_flags |= XPR_F_ERRBUSY;
    /* really need to have XPR_F_ERROR set here as well, but
     * xpc_partition_dead counts on it to be unset when it goes in ???  */

    if (xpr->xpr_channel == XP_CONTROL_CHANNEL) {
	switch(xr) {
	case xpcVersion:		/* Don't nuke partition for this */
	case xpcNoOvrd:
	    break;
	default:
	    XPR_UNLOCK(xpr);
	    xpc_partition_dead(xr, xpr);
	    XPR_LOCK(xpr);
	    break;
	}
    }

    xpr->xpr_xr      = xr;
    xpr->xpr_xr_line = line;
    xpr->xpr_flags |= XPR_F_ERROR;
    xpr->xpr_flags &= ~XPR_F_AVAIL;

#if _XPC_DEBUG
    sprintf(b, "%s:%d", "xpc_set_error", line);
    DPRINTXPR(b, xpr);
#endif

    /* Wake those waiting for notify completion */

    for (xn = xpr->xpr_xn; xpr->xpr_xn; xn = xpr->xpr_xn) {
	/* unlink this element so don't have to worry about problems
	 * because of the UNLOCK and lock around the async routine */
	xpr->xpr_xn = xn->xn_next; 
	if (xn->xn_xpm_flags & XPM_F_SYNC) {
	    sv_signal(&xn->xn_sv);
	} else {
	    ASSERT(xn->xn_rtn);
	    XPR_UNLOCK(xpr);
	    xn->xn_rtn(xr, xpr, xpr->xpr_partid, xpr->xpr_channel, 
		       xn->xn_par);
	    XPR_LOCK(xpr);
	}
    }

    xpr->xpr_xn = xpr->xpr_xn_end = NULL;

    XPR_RGO(xpr);			/* let receivers go */
    XPR_SGO(xpr);			/* let senders go */

    /* Signal error to anyone using poll */

    if (xpr->xpr_flags & XPR_F_XPOLL) {
	xpr->xpr_flags &= ~XPR_F_XPOLL;
	pollwakeup(xpr->xpr_ph, POLLPRI);
    }

    if (xpr->xpr_flags & (XPR_F_RDPOLL|XPR_F_WRPOLL)) {
	xpr->xpr_flags &= ~(XPR_F_RDPOLL | XPR_F_WRPOLL);
	pollwakeup(xpr->xpr_ph, POLLERR);
    }

    /* async notification required */
    if (xpr->xpr_async_rtn && xpr->xpr_channel != XP_CONTROL_CHANNEL) {
	XPR_UNLOCK(xpr);
	xpr->xpr_async_rtn(xr, xpr, xpr->xpr_partid, xpr->xpr_channel, 0);
	XPR_LOCK(xpr);
    }

    xpr->xpr_flags &= ~XPR_F_ERRBUSY;
    if(xpr->xpr_flags & XPR_F_FREEPEND) {
	xpc_process_disconnect(xpr);
	XPR_LOCK(xpr);
    }
}

static	void
xpc_setup_new_xpr(xpr_t *xpr, xpr_t *cxpr, partid_t p, __uint32_t c)
/*
 * Function: 	xpc_setup_new_xpr
 * Purpose:	Set up the initial valus for a cross partition 
 *		communication ring.
 * Parameters:	xpr 	- pointer to ring to initialize
 *		cxpr	- pointer to control ring for xpr.
 *		p 	- remote destination partition id associated with
 *			  ring.
 *		c 	- channel number being set up.
 * Returns:	nothing
 */
{
    pr_t	*pr = &xpc_rings[p];

    bzero(xpr, sizeof(*xpr));

    xpr->xpr_magic    = XPR_MAGIC;

    xpr->xpr_control  = cxpr;
    xpr->xpr_target   = 0;
    xpr->xpr_partid   = p;
    xpr->xpr_channel  = c;
    xpr->xpr_ithreads_active = 0;
    xpr->xpr_ithreads_pending= 0;
    xpr->xpr_ithreads_busy   = 0;
    xpr->xpr_ithreads_max    = 0;

    xpr->xpr_lgp     = &pr->pr_lgp[c+1];
    xpr->xpr_rgp     = &pr->pr_rgp[c+1];

    init_spinlock(&xpr->xpr_lock, "xpr_lock", (long)c);
    init_mutex(&xpr->xpr_wr_lock, MUTEX_DEFAULT, "xpr_wr_lock", (long)c);
    init_sema(&xpr->xpr_rd_sema, 1, "xpr_rd_sema", (long)c);

    sv_init(&xpr->xpr_wrwait_sv, SV_FIFO, "xpr_wrwait");
    sv_init(&xpr->xpr_rdwait_sv, SV_FIFO, "xpr_rdwait");

    xpr->xpr_sendcnt = 0;
    xpr->xpr_ph = phalloc(KM_SLEEP);
}    

static	void
xpc_connect_timeout(pr_t *pr)
/*
 * Function: 	xpc_connect_timeout
 * Purpose:	To generate a connect interrupt periodically until
 *		the remote side either connects OR we have timeout out the
 *		ring.
 * Parameters:	xpr - ring we are trying to connect.
 * Returns:	nothing
 */
{
    xpr_t	*xpr = pr->pr_xpr;
#ifndef CELL_IRIX
    __uint64_t	state;
#endif

    ASSERT(xpr == xpr->xpr_control);

    if ((xpr->xpr_flags & XPR_F_ERROR) || !(xpr->xpr_flags & XPR_F_CONNECT)) {
	return;
    }

    /* wait X times as long this time */
    pr->pr_connect_wait *= XPR_CONNECT_WAIT_X;

    if (0 == pr->pr_connect_try--) {
#ifndef	CELL_IRIX
	if (!part_get_var(xpr->xpr_partid, PART_VAR_STATE, &state) 
	    && (PV_STATE_STOPPED == state)) {
	    /* Don't get update if remote at breakpoint */
	    pr->pr_connect_try = XPR_CONNECT_TO; /* 5 more chances to WIN */
	    pr->pr_connect_wait = XPR_CONNECT_RATE; /* reset wait time */
	} else {
#endif
	    XPR_LOCK(xpr);
	    XPC_SET_ERROR(xpr, xpcTimeout);
	    XPR_UNLOCK(xpr);
	    return;
#ifndef	CELL_IRIX
	} 
#endif
    }
    XPR_SEND_INT(xpr);
    (void)itimeout(xpc_connect_timeout, pr, pr->pr_connect_wait,  pltimeout);
}

void
xpc_disconnect(xpr_t *xpr)
/*
 * Function: 	xpc_disconnect
 * Purpose:	To terminate a channel.
 * Parameters:	xpr - pointer to ring to shutdown.
 * Returns:	nothing
 */
{
    XPR_LOCK(xpr);
    ASSERT(xpr->xpr_flags & XPR_F_OPEN);

    XPM_HISTORY_STATE(XPM_HISTORY_DISCONNECT, XPMC_C_NOP, xpcSuccess, xpr);
    xpr->xpr_flags &= ~(XPR_F_OPEN|XPR_F_AOPEN);
    xpc_process_disconnect(xpr);	/* Unlocks ring */
}

static	void
xpc_free_ring_buffers(xpr_t *xpr)
/*
 * Function: 	xpc_complete_shutdown
 * Purpose:	complete the shutdown process and free assocated buffers.
 * Parameters:	xpr - pointer to xpr
 * Returns:	Nothing
 */
{
    int	i;

    ASSERT(XPR_LOCKED(xpr));
    ASSERT(!(xpr->xpr_flags & (XPR_F_BUSYCON|XPR_F_BUSYDIS))); /* ???? */
#   define	FREE(_x, _s)if (_x) {kmem_free((void *)(_x), (_s)); (_x)=0;}

    if (xpr->xpr_xn_list) {
	/* Destroy sync variables - so metering does not get confused */
	for (i = 0; i <= xpr->xpr_llimit; i++) {
	    sv_destroy(&xpr->xpr_xn_list[i].xn_sv);
	}
    }

    xpr->xpr_flags 	= 0;
    xpr->xpr_async_rtn  = NULL;

    FREE(xpr->xpr_lr, (xpr->xpr_llimit + 1) * xpr->xpr_lsize);
    FREE(xpr->xpr_crr,(xpr->xpr_rlimit + 1) * xpr->xpr_rsize);
    FREE(xpr->xpr_xn_list, sizeof(xnq_t) * xpr->xpr_llimit);
    XPR_GP_SET_ERROR(xpr);
    GP_SET(*xpr->xpr_rgp, 0, 0);
    GP_SET(xpr->xpr_wrgp, 0, 0);
    GP_SET(xpr->xpr_crr_gp, 0, 0);
    XPR_UNLOCK(xpr);
	
#undef	FREE
}

static	void
xpc_allocate_ring_buffers(xpr_t *xpr)
/*
 * Function: 	xpc_allocate_ring_buffers
 * Purpose:	Allocate ring buffers etc associated with a ring.
 * Parameters:	xpr - pointer to ring to complete setup.
 * Returns:	Nothing
 * Notes:	Assumes all of the local and remote ring sizes are filled in.
 */
{
    int	i;

    if (xpr->xpr_flags & XPR_F_ALLOC) {	/* Do nothing */
	return; 
    }

    /* Allocate Local/Remote Ring buffer */

    xpr->xpr_lr	= kmem_zalloc((xpr->xpr_llimit + 1) * xpr->xpr_lsize, 
			      VM_CACHEALIGN | VM_DIRECT | KM_SLEEP);
    xpr->xpr_crr= kmem_zalloc((xpr->xpr_rlimit + 1) * xpr->xpr_rsize,     
			      VM_CACHEALIGN | VM_DIRECT | KM_SLEEP);
				   
    /* Allocate xnq entires - one for each message */

    xpr->xpr_xn_list = kmem_zalloc((xpr->xpr_llimit + 1) * sizeof(xnq_t), 
				   VM_CACHEALIGN | VM_DIRECT | KM_SLEEP);
    for (i = 0; i <= xpr->xpr_llimit; i++) {
	sv_init(&xpr->xpr_xn_list[i].xn_sv, SV_DEFAULT, "xnq");
	xpr->xpr_xn_list[i].xn_put = i;
    }

    /* Clear up GP values */

    GP_SET(xpr->xpr_wlgp, 0, 0);
    GP_SET(xpr->xpr_wrgp, 0, 0);
    GP_SET(xpr->xpr_crr_gp, 0, 0);
    GP_SET(*xpr->xpr_lgp, 0, 0);
    GP_SET(*xpr->xpr_rgp, 0, 0);
    xpr->xpr_rwrap = 0;
    xpr->xpr_lwrap = 0;

    xpr->xpr_sendooocnt = 0;
    xpr->xpr_sendcnt = 0;
    xpr->xpr_rcvcnt  = 0;
    xpr->xpr_xpmcnt  = 0;
    xpr->xpr_allcnt = 0;
}

xpc_t	
xpc_connect(partid_t p, int c, __uint32_t ms, __uint32_t mc, 
	    xpc_async_rtn_t ar, uint32_t flags, __uint32_t t, xpr_t **cid)
/*
 * Function: 	xpc_connect
 * Purpose:	Establish a connection to a remote partition. 
 * Parameters:	p  - partition ID of destination.
 *		c  - channel to connect.
 *		ms - message size in bytes, MUST be power of 2.
 *		mc - # of messages in ring. ms * mc must <= NBPP.
 *		     The message system MAY reserve some of the 
 *		     entires for internal use (1 or 2 messages).
 *		ar  - function to call when asynchronous notification
 *		      of an incomming message or error is required.
 *		flags - indicating type of open....
 *		t   - max number of threads allowed to be processing messages
 *		      at a given time.
 *		cid- pointer to location to store "ring pointer", used 
 *		     on all the other calls. 
 * Returns:	xpc_t
 * Notes:	Must be called with a context, and will sleep waiting
 *		for connection to be established or until a failure 
 *		is detected. The message system implements a very coarse
 *		grain timeout mechanism so this routine may be called before
 *		interpartition heart beats are active.
 */
{
    xpr_t	*xpr;
    xpc_t	xr = xpcSuccess;	/* result */
#   define	XPR_F_OPEN_FLAGS (XPR_F_OPEN | XPR_F_AOPEN | \
				  XPR_F_NONBLOCK | XPR_F_BWAIT)

    if ((p >= MAX_PARTITIONS) || !(xpc_rings[p].pr_xpr)) {
	return(xpcPartid); 
    } else if (!XP_ALIGNED(ms)) {
	return(xpcError);
    } 

    xpr = xpc_rings[p].pr_xpr + c;

    XPR_LOCK(xpr);
    if ((xpr->xpr_flags == (XPR_F_OPEN|XPR_F_AVAIL|XPR_F_ALLOC)) ||
	       ((xpr->xpr_flags & (XPR_F_OPEN|XPR_F_CONNECT)) ==
		(XPR_F_OPEN|XPR_F_CONNECT))) {
	xr = xpcConnectInProg;
    } else if (xpr->xpr_flags & ~XPR_F_RCONNECT) {
	/* Anything other than remote connect request, say we are busy */
	xr = xpcBusy;
    }
    if (xr) {
	XPR_UNLOCK(xpr);
	return(xr);
    }

    if (!(xpr->xpr_target = xpc_find_target(xpr->xpr_partid))) {
	XPR_UNLOCK(xpr);
	return(xpcPartid);		/* partition not there now */
    }

    XPM_HISTORY_STATE(XPM_HISTORY_CONNECT, XPMC_C_NOP, xpcSuccess, xpr);
    xpr->xpr_flags |= XPR_F_OPEN;
    xpr->xpr_flags |= (flags & XPC_CONNECT_AOPEN) 	? XPR_F_AOPEN    : 0;
    xpr->xpr_flags |= (flags & XPC_CONNECT_NONBLOCK)   	? XPR_F_NONBLOCK : 0;
    xpr->xpr_flags |= (flags & XPC_CONNECT_BWAIT)	? XPR_F_BWAIT	 : 0;
    xpr->xpr_llimit = mc - 1;
    xpr->xpr_lsize  = ms;
    xpr->xpr_async_rtn = ar;

    xpr->xpr_xr = xpcSuccess;		/* Start off clean */
    xpr->xpr_ithreads_max = t;

    GP_SET(*xpr->xpr_lgp, 0, 0);	     /* Clear any error left behind */

    xpr->xpr_flags |= XPR_F_CONNECT | XPR_F_BUSYCON;

    XPR_UNLOCK(xpr);
    if (xr = xpc_send_control(xpr, XPMC_C_OPEN, xpcSuccess, xpr->xpr_llimit, 
			      xpr->xpr_lsize, 0)) {
	XPR_LOCK(xpr); 
	xpr->xpr_flags &= ~XPR_F_BUSYCON;
	xpr->xpr_flags &= ~XPR_F_OPEN_FLAGS;
	xpc_process_disconnect(xpr);
	return(xr);
    }

    XPR_LOCK(xpr); 
    XPR_GP_SET_READY(xpr);

    xpr->xpr_flags &= ~XPR_F_BUSYCON;	/* Clear busy - for process_connect*/
    if (xpr->xpr_flags & (XPR_F_RSHUTDOWN|XPR_F_SHUTDOWN|XPR_F_ERROR)) {
	xpr->xpr_flags &= ~XPR_F_OPEN_FLAGS;
        xr = (xpr->xpr_flags & XPR_F_ERROR) ? xpr->xpr_xr : xpcDisconnected;
	xpc_process_disconnect(xpr);	/* Frees/Unlocks ring */
	return(xr);
    }

    xr = xpc_process_connect(xpr);	/* Process any queued thingy */

    XPR_LOCK(xpr);

    /* check for error at end of xpc_process_connect not returned from it */
    if(xpr->xpr_flags & (XPR_F_SHUTDOWN|XPR_F_RSHUTDOWN|XPR_F_ERROR)) {
        xr = (xpr->xpr_flags & XPR_F_ERROR) ? xpr->xpr_xr : xpcDisconnected;
    } else if(!(xpr->xpr_flags & XPR_F_OPEN)) {
        /* between the unlock in xpc_process_connect and lock above, could 
	** have gotten a XPMC_C_OPEN_REPLY with an error in which case we 
        ** would have turned off all flags at this point. */
        XPR_UNLOCK(xpr);
        return(xpcDisconnected);
    }

    if ((xpcSuccess != xr) && (xpcDone != xr)) {
	xpr->xpr_flags &= ~XPR_F_OPEN_FLAGS;
	xpc_process_disconnect(xpr);
	/* 
	 * If an error occured, the ring is already free, just return.
	 */
	return(xr);
    }


    /* Now wait for remote reply to our open. */

    if (!(flags & XPC_CONNECT_AOPEN)) {	
	/*
	 * If asynchronous open not requested, wait for reply iff CONNECT
	 * is still set. If it is not set, then operation is complete. 
	 */
	if (!(xpr->xpr_flags & XPR_F_AVAIL)) {
	    xr = xpc_swait(xpr);
	    XPR_LOCK(xpr);
	}
	/*
	 * Since xpc_swait releases the XPR_LOCK, even if xpc_swait returns
	 * xpcSuccess the ring may be closed or have an error.
	 */
	if ((xpcSuccess == xr) && (xpr->xpr_flags & XPR_F_ERROR)) {
	    xr = xpr->xpr_xr;
	}
	if (xpcSuccess != xr) {
	    xpr->xpr_flags &= ~XPR_F_OPEN_FLAGS;
	    xpc_process_disconnect(xpr);
	} else {
            XPR_UNLOCK(xpr);
	}
    } else {
	if (xpr->xpr_flags & XPR_F_ERROR) {
	    xr = xpr->xpr_xr;
	    xpr->xpr_flags &= ~XPR_F_OPEN_FLAGS;
	    xpc_process_disconnect(xpr);
	} else {
	    xr = (xpr->xpr_flags & XPR_F_CONNECT) ? xpcSuccess : xpcDone;
	    XPR_UNLOCK(xpr);
	}
    }
    if ((xpcSuccess == xr) || (xpcDone == xr)) {
	*cid = xpr;
    } else {
	*cid = NULL;
    }
    return(xr);
}

static	void
xpc_process_disconnect(xpr_t *xpr)
/*
 * Function: 	xpc_process_disconnect
 * Purpose:	Completes disconnect processing, and free's ring is required.
 * Parameters:	xpr - pointer to ring
 * Returns:	Nothing
 * 
 * Locks held on entry:	XPR_LOCK
 * Locks held on exit:	none
 */
{
    xpc_t xr; 

    ASSERT(XPR_LOCKED(xpr));

    if (xpr->xpr_flags & (XPR_F_BUSYDIS|XPR_F_BUSYCON)) {
	XPR_UNLOCK(xpr);
	return;
    } 

    xpr->xpr_flags |=XPR_F_BUSYDIS;

    /* do we owe a sending of a OPEN_REPLY with a busy flag? */
    if(xpr->xpr_flags & XPR_F_REPLYPEND) {
        while(xpr->xpr_flags & XPR_F_REPLYPEND) {
	    xpr->xpr_flags &= ~XPR_F_REPLYPEND;
	    XPR_UNLOCK(xpr);
	    xr = xpc_send_control(xpr, XPMC_C_OPEN_REPLY, xpcBusy, 0, 0, 0);
	    XPR_LOCK(xpr);	
	}
	/* if neither SHUTDOWN or RSHUTDOWN then our last message
	** should take care of closing down the opposite ring or
	** if we couldn't send the control message */
	if(!(xpr->xpr_flags & (XPR_F_SHUTDOWN|XPR_F_RSHUTDOWN)) ||
	   xr != xpcSuccess) {
	    xpr->xpr_flags |= XPR_F_SHUTDOWN|XPR_F_RSHUTDOWN;
	}
    }

    if (!(xpr->xpr_flags & XPR_F_SHUTDOWN)) { /* Shutdown not sent */
	xpr->xpr_flags |= XPR_F_SHUTDOWN;
	XPR_UNLOCK(xpr);
	xr = xpc_send_control(xpr, XPMC_C_SHUTDOWN, xpr->xpr_xr, 0, 0, 0);
	XPR_LOCK(xpr);
	/* if we can't send on the control channel then the other side
	** never got our message */
	if(xr != xpcSuccess) {
	    xpr->xpr_flags |= XPR_F_RSHUTDOWN;
	}
    }

    if (xpr->xpr_flags & XPR_F_OPEN) {	/* Let open free rings */
	xpr->xpr_flags &= ~XPR_F_BUSYDIS;
	XPR_UNLOCK(xpr);
	return;
    }

    /* check if we got a OPEN message while we were just sending */
    if(xpr->xpr_flags & XPR_F_REPLYPEND) {
        while(xpr->xpr_flags & XPR_F_REPLYPEND) {
	    xpr->xpr_flags &= ~XPR_F_REPLYPEND;
	    XPR_UNLOCK(xpr);
	    xr = xpc_send_control(xpr, XPMC_C_OPEN_REPLY, xpcBusy, 0, 0, 0);
	    XPR_LOCK(xpr);	
	    /* if the other side never got our message, then we
	    ** can just assume that we got a remote shutdown really */
	    if(xr != xpcSuccess) {
		xpr->xpr_flags |= XPR_F_RSHUTDOWN;
	    }
	}
    }

    xpr->xpr_flags &= ~XPR_F_BUSYDIS;

    /*
     * If both ends have issued a shutdown, or an error has been seen
     * which may indicate the remote end is dead, or the remote
     * end never attempted to conenct free the rings.
     */
    if ((xpr->xpr_flags & XPR_F_RSHUTDOWN) && /* Both end shut down */
	(xpr->xpr_flags & XPR_F_SHUTDOWN)) {
	if(xpr->xpr_flags & XPR_F_ERRBUSY) {
	    xpr->xpr_flags |= XPR_F_FREEPEND;
	    XPR_UNLOCK(xpr);
	} else {
	    xpc_free_ring_buffers(xpr);
	}
    } else {
	XPR_UNLOCK(xpr);
    }
}    

static xpc_t
xpc_process_connect(xpr_t *xpr)
/*
 * Function: 	xpc_process_connect
 * Purpose:	Process a connect message from a remote partition.
 * Parameters:	xpr - ring to process the connection request on.
 * Returns:	Nothing 
 */
{
    xpc_t	xr;

    ASSERT(XPR_LOCKED(xpr));

    if (xpr->xpr_flags & (XPR_F_BUSYDIS|XPR_F_BUSYCON)) {
	XPR_UNLOCK(xpr);
	return(xpcSuccess);
    } else if (xpr->xpr_flags & (XPR_F_ERROR|XPR_F_RSHUTDOWN|XPR_F_SHUTDOWN)) {
	XPR_UNLOCK(xpr);
	return(xpr->xpr_xr);
    }

    xpr->xpr_flags |= XPR_F_BUSYCON;	/* Mark busy */

    /* If CONNECT+RCONNECT, be sure ring buffers are allocated */

    if ((xpr->xpr_flags & XPR_F_CONNECT)&&(xpr->xpr_flags & XPR_F_RCONNECT)) {
	XPR_UNLOCK(xpr);		/* Be sure we can sleep */
	xpc_allocate_ring_buffers(xpr);
	XPR_LOCK(xpr);			/* Could now have error */
	xpr->xpr_flags |= XPR_F_ALLOC;

	/*
	 * Ring could now have an error (XPR_F_ERROR), seen a remote shutdown
	 * (XPR_F_RSHUTDOWN), a local shutdown (XPR_F_SHUTDOWN).
	 * In any of these cases, stop forward motion return.
	 */
	if (xpr->xpr_flags & (XPR_F_ERROR|XPR_F_RSHUTDOWN|XPR_F_SHUTDOWN) ||
	    !(xpr->xpr_flags & XPR_F_OPEN)) { 
	    xr = (xpr->xpr_flags & XPR_F_ERROR) ? xpr->xpr_xr:xpcDisconnected;
	    xpr->xpr_flags &= ~XPR_F_BUSYCON;
            XPC_SET_ERROR(xpr, xr);
	    xpc_process_disconnect(xpr);
	    return(xr);
	}
    } else {
	xpr->xpr_flags &= ~XPR_F_BUSYCON;
	XPR_UNLOCK(xpr);
	return(xpcSuccess);		/* Nothing more to do now */
    }

    /*
     * Check if we need to send a reply - if so, send it.
     */

    if (!(xpr->xpr_flags & XPR_F_ERROR) && !(xpr->xpr_flags & XPR_F_REPLY)) {
	paddr_t lr = (paddr_t)xpr->xpr_lr;	/* pick up before unlock */
	xpr->xpr_flags |= XPR_F_REPLY;
	XPR_UNLOCK(xpr);
	ASSERT(lr);
	xr = xpc_send_control(xpr, XPMC_C_OPEN_REPLY, xpcSuccess, lr,
			   (__uint64_t)REMOTE_HUB_ADDR(cputonasid(cpuid()), 
						      get_cpu_slice(cpuid()) 
						      ? PI_CC_PEND_SET_B
						      : PI_CC_PEND_SET_A), 
			      0);
	XPR_LOCK(xpr);
	ASSERT(xpr->xpr_lr);
	if (xr) {
	    XPC_SET_ERROR(xpr, xr);
	}
	if (xpr->xpr_flags & (XPR_F_ERROR|XPR_F_RSHUTDOWN|XPR_F_SHUTDOWN) ||
	    !(xpr->xpr_flags & XPR_F_OPEN)) { 
	    xr = (xpr->xpr_flags & XPR_F_ERROR) ? xpr->xpr_xr:xpcDisconnected;
	    xpr->xpr_flags &= ~XPR_F_BUSYCON;
            XPC_SET_ERROR(xpr, xr);
	    xpc_process_disconnect(xpr);
	    return(xr);
	}
    }

    if (!(xpr->xpr_flags & XPR_F_ERROR) && (xpr->xpr_flags & XPR_F_RREPLY)) {
	ASSERT(xpr->xpr_lr && xpr->xpr_crr && xpr->xpr_rr);

	/* Complete ring setup */

	xpr->xpr_flags |= XPR_F_AVAIL;
	xpr->xpr_flags &= 
	    ~(XPR_F_CONNECT|XPR_F_RCONNECT|XPR_F_RREPLY|XPR_F_REPLY);

	/*
	 * If any poll for write or exception fds, 
	 * tell them we are ready.
	 */
	if (xpr->xpr_flags & XPR_F_XPOLL) {
	    xpr->xpr_flags &= ~XPR_F_XPOLL;
	    pollwakeup(xpr->xpr_ph, POLLPRI);
	}

	if (xpr->xpr_flags & XPR_F_WRPOLL) {
	    xpr->xpr_flags &= ~XPR_F_WRPOLL;
	    pollwakeup(xpr->xpr_ph, POLLOUT|POLLWRNORM);
	}
	/*
	 * If anyone waiting, let them go now, otherwise, 
	 * call async handler. 
	 */
	XPR_SGO(xpr);		/* wake up sleeper */
	if (xpr->xpr_flags & XPR_F_AOPEN) {
	    xpr->xpr_flags &= ~(XPR_F_AOPEN|XPR_F_BUSYCON);
	    XPR_UNLOCK(xpr);
	    if (xpr->xpr_async_rtn) {
		xpr->xpr_async_rtn(xpr->xpr_flags & XPR_F_ERROR
				   ? xpr->xpr_xr : xpcConnected, 
				   xpr, xpr->xpr_partid, 
				   xpr->xpr_channel, NULL);
		/*
		 * At this point, the connection setup is complete, but
		 * if an error occured, the result is in xpc_xr, so just 
		 * return that.
		 */
	    } 
	    return(xpr->xpr_xr);
	}
    }	
    xpr->xpr_flags &= ~XPR_F_BUSYCON;
    XPR_UNLOCK(xpr);
    return(xpr->xpr_xr);
}

/*ARGSUSED*/
static	void
xpc_process_control(xpc_t xr, xpr_t *cxpr, partid_t partid, int c, void *k)
/*
 * Function: 	xpc_process_control
 * Purpose:	To process an incomming message/interrupt on the control ring.
 * Parameters:	xpr - pointer to the control ring.
 *		partid - partition id of sending partition.
 *		c   - channel # message arriving on.
 *		k   - message pointer.
 * Returns:	Nothing
 */
{
    xpm_t	*xpm = (xpm_t *)k;	/* Message pointer */
    xpr_t	*xpr;			/* ring pointer for working */
    xpmb_t	*xpmb;			/* xpmb pointers */
    xpmc_t	tcm, *cm;		/* control message pointer */

    if (xpcAsync != xr) {
	XPR_LOCK(cxpr);
	XPC_SET_ERROR(cxpr, xr);
	XPR_UNLOCK(cxpr);
	return;
    }

    DPRINTXPM("xpc_process_control", cxpr, xpm);
	
    /* 
     * Right now, only 1 immediate buffer is supported. That immediate buffer
     * is a control message (xpmc_t).  Anything else indicates remote side 
     * is totally screwed up.
     */

    xpmb = (xpmb_t *)(xpm + 1);
    tcm  = *(xpmc_t *)((__psunsigned_t)xpmb + xpmb->xpmb_ptr);
    cm   = &tcm;

    if ((xpm->xpm_cnt != 1) || (xpmb->xpmb_cnt != sizeof(xpmc_t))) {
	xpc_done(cxpr, xpm);		/* release the message */
	XPR_LOCK(cxpr);
	XPC_SET_ERROR(cxpr, xpcError);
	XPR_UNLOCK(cxpr);
	return;
    }

    /* Verify valid channel */

    if (cm->xpmc_channel < xpc_map->xpm_xpe[cxpr->xpr_partid].xpe_cnt) {
	xpr = cxpr + cm->xpmc_channel; /* get the channel */
    } else {
	xpr = NULL;
    }

    XPM_HISTORY_STATE(XPM_HISTORY_RECEIVE, cm->xpmc_cmd, cm->xpmc_xr, xpr);

    switch(cm->xpmc_cmd) {
    case XPMC_C_OPEN:			/* New connection request */
	if (NULL == xpr) {		/* no such channel  */
            xpc_done(cxpr, xpm);
	    (void)xpc_send_control(cxpr, XPMC_C_OPEN_REPLY, 
				   xpcChannel, 0, 0, 0);
	    break;
	} 
	XPR_LOCK(xpr);
	/* If something going on, just return busy */
	if (xpr->xpr_flags & (XPR_F_SHUTDOWN | 	/* local shutdown seen */
			      XPR_F_RSHUTDOWN |	/* remote shutdown seen */
			      XPR_F_ERROR | 	/* error seen */
			      XPR_F_RCONNECT))  /* already seen open */
        {
	    XPC_SET_ERROR(xpr, xpcBusy);
	    xpr->xpr_flags |= XPR_F_REPLYPEND;
	    xpc_done(cxpr, xpm);
	    xpc_process_disconnect(xpr);
	} else if(xpr->xpr_flags & (XPR_F_AVAIL|XPR_F_ALLOC)) {
	    XPC_SET_ERROR(xpr, xpcError); 
	    XPR_UNLOCK(xpr);
            xpc_done(cxpr, xpm);
	} else {
	    /* Save required parameters */
	    xpr->xpr_flags |= XPR_F_RCONNECT;
            xpc_done(cxpr, xpm);
	    xpr->xpr_rlimit = XPMC_PARM(cm, 0, __int32_t);
	    xpr->xpr_rsize  = XPMC_PARM(cm, 1, __int32_t);
	    xpc_process_connect(xpr);	/* Unlocks Ring */
	}
	break;
    case XPMC_C_OPEN_REPLY:		/* reply to open request */
	if (!xpr) {			/* other side totally wacked */
            xpc_done(cxpr, xpm);
	    XPR_LOCK(cxpr);
	    XPC_SET_ERROR(cxpr, xpcError);
	    XPR_UNLOCK(cxpr);
	    return;
	}
		    
	XPR_LOCK(xpr);
	if (cm->xpmc_xr) {
	    if(!(xpr->xpr_flags & (XPR_F_SHUTDOWN|XPR_F_RSHUTDOWN))) {
                xpr->xpr_flags |= (XPR_F_SHUTDOWN|XPR_F_RSHUTDOWN);
            }
	    XPC_SET_ERROR(xpr, cm->xpmc_xr);
            xpc_done(cxpr, xpm);
	    xpc_process_disconnect(xpr);
	} else if (!(xpr->xpr_flags & XPR_F_CONNECT)) {
	    XPR_UNLOCK(xpr);
            xpc_done(cxpr, xpm);
	    (void)xpc_send_control(xpr,XPMC_C_SHUTDOWN,xpcDisconnected,0,0,0);
	} else if (xpr->xpr_flags & (XPR_F_SHUTDOWN|XPR_F_RSHUTDOWN|
				    XPR_F_ERROR)) { 
	    /* disconnecting or in error state so ignor */
	    XPR_UNLOCK(xpr);
            xpc_done(cxpr, xpm);
	} else {
	    /* Pick up remote parameters */
	    xpr->xpr_rr     = XPMC_PARM(cm, 0, xpm_t *);
	    xpr->xpr_target = XPMC_PARM(cm,1,volatile hubreg_t *);
	    ASSERT(xpr->xpr_flags & XPR_F_RCONNECT);
	    xpr->xpr_flags |= XPR_F_RREPLY;
	    xpc_done(cxpr, xpm);
	    ASSERT(XPMC_PARM(cm, 0, xpm_t *));
	    ASSERT(xpr->xpr_rr);
	    xpc_process_connect(xpr);	/* Unlocks ring */
	}
	break;
    case XPMC_C_SHUTDOWN:
	if (!xpr) {
            xpc_done(cxpr, xpm);
	    XPR_LOCK(cxpr);
	    XPC_SET_ERROR(cxpr, xpcDisconnected);
	    XPR_UNLOCK(cxpr);
	    return;
	}
	XPR_LOCK(xpr);

	/* If remote shutdown of control link - nuke partition */

	if (xpr == cxpr) {
            xpc_done(cxpr, xpm);
	    XPC_SET_ERROR(xpr, xpcDisconnected);	
	    XPR_UNLOCK(xpr);
	    return;
	}

	/*
	 * If local ring is not open, and does not have any flags
	 * set or we have seen both the flags for a shutdown sequence, 
         * then this SHUTDOWN was unexpected. In this case, 
	 * simply send a shutdown back to let remote end clean up. 
	 * No local flags are set since local end is cleaned up already.
	 */
	if (!(xpr->xpr_flags) || ((xpr->xpr_flags & XPR_F_SHUTDOWN) && 
                                  (xpr->xpr_flags & XPR_F_RSHUTDOWN))) {
	    XPR_UNLOCK(xpr);
            xpc_done(cxpr, xpm);
	    (void)xpc_send_control(xpr, XPMC_C_SHUTDOWN, xpcSuccess, 0, 0, 0);
	    break;
	}

	/* If remote end sent an error - pass it along */
	
	xpr->xpr_flags |= XPR_F_RSHUTDOWN; /* mark shut down */

	if (cm->xpmc_xr) {
	    XPC_SET_ERROR(xpr, cm->xpmc_xr);
            xpc_done(cxpr, xpm);
	    xpc_process_disconnect(xpr);
	} else if ((xpr->xpr_flags & (XPR_F_BUSYCON|XPR_F_BUSYDIS)) ||
		    ((xpr->xpr_flags & XPR_F_AVAIL) && XPR_PEND(xpr))) {
	/*
	 * If ring is still available, and there are pending messages
	 * allow things to drain. If ring is BUSY, thread that set it
	 * busy must handle things.
	 */
	    XPR_UNLOCK(xpr);
            xpc_done(cxpr, xpm);
	} else {
	    if(XPR_LR_EMPTY(xpr)) {
		xpr->xpr_flags |= XPR_F_DRAINED;
	    }
	    XPC_SET_ERROR(xpr, xpcDisconnected);
            xpc_done(cxpr, xpm);
	    xpc_process_disconnect(xpr);
	} 
	break;
    case XPMC_C_NOP:			/* Not much to do here */
        xpc_done(cxpr, xpm);
	break;
    case XPMC_C_TARGET:
        xpc_done(cxpr, xpm);
	XPR_LOCK(xpr);
	if (xpr->xpr_flags & XPR_F_AVAIL) {
	    xpr->xpr_target = XPMC_PARM(cm, 0, hubreg_t *);
	} 
	XPR_UNLOCK(xpr);
	break;
    default:
        xpc_done(cxpr, xpm);
	xpc_dump_xpm("xpc control", cxpr, xpm, xpc_debug);
	XPR_LOCK(cxpr);
	XPC_SET_ERROR(cxpr, xpcError);
	XPR_UNLOCK(cxpr);
    }
}
static	void
xpc_interrupt(void *arg, int idle)
/*
 * Function: 	xpc_interrupt
 * Purpose:	Process from a partition.
 * Parameters:	arg  - partition ID.
 *		idle - # threads idle.
 * Returns:	Nothing
 */
{
    partid_t	p   = (partid_t)(__psunsigned_t)arg;
    pr_t	*pr = &xpc_rings[p];
    xpr_t	*xpr;
    __int32_t	c;
    int 	active, threads, started, busy, pending;

    if (xpcSuccess != xpc_cache_remote_gp(p)) {
	return;
    }

    /* 
     * Look at all the rings and see what there is to do. xpr_cache_remote_gp
     * has already take care of waking up all those that needed waking,
     * all we do here is call any asynchronous receive routines that we need
     * to. This can not be done from the cache_remote_gp routines since that
     * routine can be called from a few places, and we don't want it to 
     * end up sleeping when it may have been sending something important.
     */
    for (c = 0; c < XP_RINGS; c++) {
	xpr = pr->pr_xpr + c;
	if (0 == xpr->xpr_ithreads_max) {
	    /* If this is not a threaded ring, ignore it */
	    continue;
	} else if ((active = atomicAddInt(&xpr->xpr_ithreads_active, 1)) > 
		   xpr->xpr_ithreads_max) {
	    atomicAddInt(&xpr->xpr_ithreads_active, -1);
	    continue;
	} 

	while ((xpr->xpr_flags & XPR_F_AVAIL)
	       && !(xpr->xpr_flags & XPR_F_ERROR)
	       && XPR_PEND(xpr)) {
	    /* No longer pending, mark us as busy */
	    compare_and_dec_int_gt(&xpr->xpr_ithreads_pending, 0);
	    busy = atomicAddInt(&xpr->xpr_ithreads_busy, 1);

	    if (busy > XPR_PEND(xpr)) {
		atomicAddInt(&xpr->xpr_ithreads_busy, -1);
		break;			/* while */
	    }
	    /*
	     * Estimate how many threads are required - the ithreads_pending/
	     * active/busy counters are atomically updated, 
	     *
	     *	(XPR_PEND - busy) is approx # messages not assigned to threads
	     *  (pending) is approx # threads requested but not here yet.
	     * 
	     * Thus, (XPR_PEND - busy - pending) is approx # threads to 
	     * request for this ring.  If the number of pending threads is
	     * 0 - count us as 1 pending thread. Note that xpc_receive_xpm
	     * decrements BOTH counters. This is the normal case of one thread
	     * entering with only 1 messagepending. 
	     * 
	     *	1) Start too many, in which case they will exit without doing
	     *	   any work.
	     *	2) Start too few, in which case the first one in will start
	     *	   more when it checks the counters. 
	     */

	    /* # messages - # busy threads - #pending threads  */

	    pending = xpr->xpr_ithreads_pending; 
	    threads = XPR_PEND(xpr) - busy - pending;
	    if (threads < 0) {
		threads = 0;
	    } else if (threads + active > xpr->xpr_ithreads_max) {
		threads = xpr->xpr_ithreads_max - active;
	    }
	    if (0 != threads) {
		atomicAddInt(&xpr->xpr_ithreads_pending, threads);
	    } else if (0 == idle) {
		/*
		 * Requesting 0 threads, and 0 idle threads, so we must 
		 * be able to start at least one, otherwise we may commit
		 * the last thread - which we can never do.
		 */
		threads = 1;
	    }

	    if (0 != threads) {
		started = part_start_thread(xpr->xpr_partid,threads,B_FALSE);
		if (0 == idle) {
		    /*
		     * Once we have successfully started another thread, we
		     * know we are not the last idle thread any more. 
		     */ 
		    if (started) {
			idle = started;
		    } else {
			atomicAddInt(&xpr->xpr_ithreads_busy, -1);
			break;		/* While */
		    } 
		}
	    }
	    xpc_receive_xpm(xpr);
	}
	atomicAddInt(&xpr->xpr_ithreads_active, -1);
    }
}

static	xpc_t
xpc_cache_remote_gp(partid_t p)
/*
 * Function: 	xpc_cache_remote_gp
 * Purpose:	Cache remote get/put pointers and merge them with 
 *		local copies.  
 * Parameters:	p - partition to cache GP's for
 * Returns:	xpc_t.
 * Notes:	The interrupt count from the source partition is used to 
 *		determine if another remote DMA of the GPs is required. If 
 *		the interrupt count from the remote partition is greater 
 *		than the last time the GPs were fetched, then they are 
 *		refetched. 
 *	
 *		Since a 64-bit unsigned count is used, the interrupt 
 *		count can reach 2^64 - 1. Assume a absolutely rediculous 
 *		rate of 4 billion interrupts per second, this count will 
 *		not wrap for:
 *	
 *		2^64 / 2^32 /  60  /  60  /  24  / 365 = 136 years
 *		       rate   sec    hour   day   year 
 *
 *  		So we ignore the wrap issue!
 *
 *		Since the BTE_NOTIFICATION_WAR used on <2.3 HUBs can cause
 *		the BTE code to unlock the bte, and acquire both bte's on
 *		our node, the serializtion of the remote gp fetch must NOT
 *		be holding a BTE. 
 */
{
    register pr_t  	*pr  = &xpc_rings[p];
    register xpr_t 	*xpr = pr->pr_xpr;
    gp_t	   	gp;
    __int32_t	   	c;
    xpc_t		xr;
    __uint64_t		int_id;		/* interrupt id */
    bte_handle_t	*bte = NULL;	/* local bte */
    int			il;

    /*
     * The following block is optimized for 2 common cases:
     *
     * 	1) The interrupt id that we are attempting to process does not 
     *	   require an actual fetch of the GP's, and then can be determined
     *	   by looking at the last interrupt_id value for which the GP's
     *	   were fetched.
     *
     *	2) The interrupt id that we are attempting to process requires
     *	   a fetch of the GP's, and no other thread is currently fetching
     *	   them. 
     */
    int_id = part_interrupt_id(p);
    xr     = xpcSuccess;
    atomicAddLong(&pr->pr_ggp_cnt, 1);
    while (int_id > pr->pr_ggp_id) {
	il = mutex_spinlock(&pr->pr_ggp_lock);
	if (!pr->pr_ggp_busy) {
	    pr->pr_ggp_busy = pr->pr_ggp_waitid > int_id 
		                 ? pr->pr_ggp_waitid : int_id;
	    /* We want to do it ... */
	    mutex_spinunlock(&pr->pr_ggp_lock, il);
	    bte = bte_acquire(1);
	    if (bte) {
		xr = xpc_pull_data_fast(bte, pr->pr_rgpAddr, 
					pr->pr_rgp, GP_SIZE);
		if (xpcSuccess == xr) {
		    pr->pr_ggp_id = pr->pr_ggp_busy;
		    pr->pr_ggp_pcnt++;
		}
		pr->pr_ggp_busy = 0;
	    } else {
		xr = xpcError;
	    }
	    il = mutex_spinlock(&pr->pr_ggp_lock);
	    sv_broadcast(&pr->pr_ggp_sv);
	    mutex_spinunlock(&pr->pr_ggp_lock, il);
	    break;			/* while */
	} else {
	    pr->pr_ggp_wcnt++;
	    if (int_id > pr->pr_ggp_waitid) {
		pr->pr_ggp_waitid = int_id;
	    }
	    sv_wait(&pr->pr_ggp_sv, PZERO, &pr->pr_ggp_lock, il);
	}
    }

    if ((xpcSuccess == xr) && !GP_MAGIC_TEST(pr->pr_rgp[0])) {
	xr = xpcError;
    }
    if (xpcSuccess != xr) {
	if (bte) {
	    bte_release(bte);
	}
	XPR_LOCK(pr->pr_xpr);
	XPC_SET_ERROR(pr->pr_xpr, xr);
	XPR_UNLOCK(pr->pr_xpr);
	return(xr);
    }

    for (xpr = pr->pr_xpr, c = 1; c <= XP_RINGS; c++, xpr++) {
	if ((xpr->xpr_flags & (XPR_F_AVAIL|XPR_F_OPEN))
	    != (XPR_F_OPEN|XPR_F_AVAIL)) {
	    continue;
	}
	if (GP_EQUAL(pr->pr_rgp[c], xpr->xpr_wrgp) ||
	    (xpr->xpr_flags & XPR_F_CACHEBUSY)) {
	    /* 
	     * If a quick check shows nothing has changed, then don't do the
	     * rest.
	     */
	    continue;
	}
	/* Check if cached get different from get */

	XPR_LOCK(xpr);

	/* see if someone else is already in the xpc_cache_remote_gp code so
	 * we don't race with them. */
	if(xpr->xpr_flags & XPR_F_CACHEBUSY) {
	    XPR_UNLOCK(xpr);
	    continue;

	}
	xpr->xpr_flags |= XPR_F_CACHEBUSY;


	GP_GET(*xpr->xpr_rgp, gp);

#define	GP_TEST_ERROR(_g)	(((_g).gp_get == -1) && ((_g).gp_put == -1))
	if (GP_TEST_ERROR(gp)) {	/* Hmmm remote screwed up */
	    if(XPR_LR_EMPTY(xpr)) {
		xpr->xpr_flags |= XPR_F_DRAINED;
	    }
	    XPC_SET_ERROR(xpr, xpcDisconnected);
	    XPR_UNLOCK(xpr);
	    continue;
 	}

	if (xpr->xpr_wrgp.gp_get != gp.gp_get) {
	    if (gp.gp_get < xpr->xpr_wrgp.gp_get) {/* wrapped */
#if defined(DEBUG)
		if (xpr->xpr_lwrap <= xpr->xpr_rwrap) {
		    cmn_err(CE_PANIC, "XPC-ASSERT(%d): xpr(0x%x) "
			    "gp(%d,%d) lw(%d) rw(%d)\n", 
			    __LINE__, xpr, gp.gp_get, gp.gp_put, 
			    xpr->xpr_lwrap, xpr->xpr_rwrap);
		}
#endif 
		xpr->xpr_rwrap++;
#if defined(DEBUG)
		if (xpr->xpr_lwrap != xpr->xpr_rwrap) {
		    cmn_err(CE_PANIC, 
			    "XPC-ASSERT(%d): xpr(0x%x) gp(%d,%d) lw(%d) "
			    "rw(%d)\n", __LINE__, xpr, gp.gp_get, gp.gp_put, 
			    xpr->xpr_lwrap, xpr->xpr_rwrap);
		}
#endif 
	    }
	    
	    /*
	     * Even though we updated the wrap count, the allocation 
	     * scheme will NOT re-use the ring entires until the remote
	     * get values are updated in the wrgp. Therefore, we use the
	     * stored value of the rwrap counter and the new value of the 
	     * remote get counter to decide which messages have been 
	     * received. Storing the new wrgp.gp_get value could 
	     * cause us to re-use a message that has an entry on 
	     * the notify queue if we don't process the notify queue first.
	     */
	    if (xpr->xpr_flags & XPR_F_NOTIFY) {
		xpc_notify_continue(xpr, xpr->xpr_rwrap, gp.gp_get);
	    }

	    xpr->xpr_wrgp.gp_get = gp.gp_get;

	    if (xpr->xpr_flags & XPR_F_WR_WAIT) {
		/* If write pending and space now, wake up writer. */
		if (XPR_AVAIL(xpr)) {
		    XPR_SGO(xpr);
		}
	    }

	    if (xpr->xpr_flags & XPR_F_WRPOLL) {
		if (XPR_AVAIL(xpr)) {
		    pollwakeup(xpr->xpr_ph, POLLOUT|POLLWRNORM);
		    xpr->xpr_flags &= ~XPR_F_WRPOLL;
		}
	    }
	}

	/*
	 * Possible events to deal with if remote put pointer has changed
	 * since last time we looked.
	 */
	if (xpr->xpr_wrgp.gp_put != gp.gp_put) {
	    xpr->xpr_wrgp.gp_put = gp.gp_put;
	    if (xpr->xpr_flags & XPR_F_RD_WAIT) {
		/* If read pending and there is data space, wake up reader */
		if (XPR_PEND(xpr)) {
		    XPR_RGO(xpr);
		}
		if (xpr->xpr_flags & XPR_F_RDPOLL) {
		    if (XPR_PEND(xpr)) {
			pollwakeup(xpr->xpr_ph, POLLIN|POLLRDNORM);
			xpr->xpr_flags &= ~XPR_F_RDPOLL;
		    }
		}
	    }
	    if (bte && XPR_TRY_RLOCK(xpr)) {
		/*
		 * Only try to cache the headers if we have a bte, because
		 * if we don't, the thread that issues the actual "read"
		 * will have to acquire one anyway.
		 */
		xpc_pull_xpm(bte, xpr);
		XPR_RUNLOCK(xpr);
	    }
	}
	xpr->xpr_flags &= ~XPR_F_CACHEBUSY;
	XPR_UNLOCK(xpr);
    }
    if (bte) {
	bte_release(bte);
    }
    return(xpcSuccess);
}

#if	defined(DEBUG)
void	
xpc_check(xpr_t *xpr, xpm_t *m)
/*
 * Function: 	xpc_check
 * Purpose:	Verify a message is valid in terms of the message size 
 *		associated with the channel. 
 * Parameters:	xpr- Pointer to ring for which message was built.
 *		m  - pointer to message.
 * Returns:	Nothing, routine will panic if message is invalid.
 *
 * Notes:	This routine is intended as a debugging aid. It 
 *		verifies that immediate data has not overflowed, 
 *		and various other things.
 * 	
 *		Use XPC_CHECK_MESSAGE in-line to avoid #if DEBUG's
 *	
 */
{
#define	XPCCHECK(_x, _m, _c) if (!(_c)) DPRINTXPM("xpc_check", _x, _m); \
                                     ASSERT(_c)
    void	*mEnd;			/* Pointer to byte passed end */
    int		b, bj;			/* buffer index */


    mEnd = (char *)m + xpr->xpr_lsize;

    /*
     * Check for:
     *		1) overlapping immediate data regions, which may be valid
     *		   some day, but right now do not make sense.
     *		2) any immediate data that spans passed end of message.
     *		3) Valid flags
     */
    for (b = 0; b < m->xpm_cnt; b++) {
	XPCCHECK(xpr, m, !(XPM_BUF_FLAGS(m, b) & ~XPMB_F_INMASK));
	if (!(XPM_BUF_FLAGS(m, b) & (XPMB_F_PTR|XPMB_F_IMD))) {
	    continue;
	}
	if (XPM_BUF_FLAGS(m, b) & XPMB_F_IMD) {
	    XPCCHECK(xpr, m, 
		     XPM_BUF_PTR(m, b) + XPM_BUF_CNT(m, b) <= (paddr_t)mEnd);
	    for (bj = b+1; bj < m->xpm_cnt; bj++) {
		if (XPM_BUF_FLAGS(m, bj) & XPMB_F_IMD) {
		    /* Overlapping buffers ? */
		    if(XPM_BUF_PTR(m, bj) < XPM_BUF_PTR(m, b)) {
			XPCCHECK(xpr, m, XPM_BUF_PTR(m, bj)+XPM_BUF_CNT(m, bj)
				 < XPM_BUF_PTR(m, b));
		    } else {
			XPCCHECK(xpr, m, XPM_BUF_PTR(m, bj) 
				 >= XPM_BUF_PTR(m, b) + XPM_BUF_CNT(m, b));
		    }
		} else if (XPM_BUF_FLAGS(m, bj) & XPMB_F_PTR) {
		    XPCCHECK(xpr, m, IS_KSEGDM(XPM_BUF_PTR(m, bj)) ||
			     IS_KPHYS(XPM_BUF_PTR(m, bj)));
		}
	    }
	}
    }
}

#endif

xpc_t 
xpc_receive_xpmb(xpmb_t *xpmb, __uint32_t *c, xpmb_t **xpmb_out, 
		 alenlist_t al, ssize_t *size)
/*
 * Function: 	xpc_receive_xpmb
 * Purpose:	To read an incomming message into a user buffer.
 * Parameters:	xpmb - xpmb's associated with a message, describing
 *			physical addressews to pull data from.
 *		c - pointer to count of number of xpmb's valid, updated
 *		    on return.
 *		xpmb_out - pointer to first "unfinished" xpmb, 
 *		    if partly consumed, ptr and cnt are update to reflect
 *		    residule. If NULL, xpmb is not updated.
 *		al - addrlen list to read data into (pages pinned).
 *		size - total bytes on entry to read, residual bytes on
 *			exit.
 * Returns:	xpc_t
 */
{
    size_t	l;			/* Length remaining total transfer */
    size_t	tl;			/* temp length */
    paddr_t 	clpa, crpa;		/* Current local/remote phys addr */
    size_t	cll, crl;		/* Current local/remote len */
    void	*bte;			/* BTE handle */
    int		al_result, cnt;
    xpc_t	xr = xpcSuccess;
    unsigned char flags;		/* xpmb flags */

    ASSERT(c > 0);

    l   = *size;
    cnt = *c;

    if (!(bte = bte_acquire(1))) {	/* get us a bte */
	return(xpcError);
    }

    cll = 0;
    crl = 0;

    while ((l > 0) && (xr == xpcSuccess)) {
	if (0 == crl) {			/* next remote buffer */
	    if (!cnt) {			/* If none left, bail... */
		break;			/* while */
	    } 
	    flags = xpmb->xpmb_flags;
	    if ((0 == xpmb->xpmb_cnt) || !(flags & (XPMB_F_IMD|XPMB_F_PTR))){
		/* Nothing to do with this one */
		xpmb++;
		cnt--;
		continue;
	    }
	    crpa = xpmb->xpmb_ptr + (flags & XPMB_F_IMD ? (paddr_t)xpmb : 0);
	    crl  = xpmb->xpmb_cnt;	/* remote length */
	    xpmb++;
	    cnt--;
	}

	if (0 == cll) {			/* next local buffer */
	    al_result = alenlist_get(al,NULL,crl, (alenaddr_t *)&clpa,&cll,0);
	    if (ALENLIST_SUCCESS != al_result) {
		/* Needed to keep compiler happy on non-debug kernels */
		ASSERT(al_result == ALENLIST_SUCCESS);
	    }
	}

	/* Now move as much data as possible */

	tl = cll > crl ? crl : cll;
	if (flags & XPMB_F_IMD) {
	    bcopy((void *)crpa, (void *)PHYS_TO_K0(clpa), tl);
	} else {
	    xr = xpc_pull_data(bte, crpa, (void *)clpa, tl);
	    if (xpcSuccess != xr) {
		break;
	    }
	}
	clpa += tl;			/* local address */
	crpa += tl;			/* remote address */
	cll  -= tl;			/* local length */
	crl  -= tl;			/* remote length */
	l    -= tl;			/* Update total length */
    }

    bte_release(bte);			/* Give up the BTE */

    if (crl) {				/* didn't consume all */
	xpmb--;				/* Back up to last one consumed */
	cnt++;				/* Increment count */
	if (flags & XPMB_F_IMD) {	/* If immediate - update offset */
	    xpmb->xpmb_ptr += xpmb->xpmb_cnt - crl;
	} else {			/* else update the pointer */
	    xpmb->xpmb_ptr = crpa;
	}
	xpmb->xpmb_cnt = crl;		/* Update length */
	if (xpmb_out) {
	    *xpmb_out = xpmb;
	}
    } else if (xpmb_out) {		/* Say we are done */
	*xpmb_out = NULL;
    }
	
    *size = l;				/* Update resid */
    *c    = cnt;			/* Update count */
    return(xr);
}

static	boolean_t
xpc_check_notify_complete(xnq_t *xn, __uint64_t wrap, short get)
/*
 * Function: 	xpc_check_notify_complete
 * Purpose:	Check if a pending notify queue entry has completed.
 * Parameters:	xn - pointer to notify queue entry.
 *		wrap - remote wrap count
 *		get - remote  get value
 * Returns:	B_TRUE - message  has been completly received.
 *		B_FALSE - message has not been completely received.
 */
{
    return((wrap > xn->xn_wrap)||((wrap == xn->xn_wrap)&&(get > xn->xn_put))
	   ? B_TRUE : B_FALSE);
}

static	void
xpc_notify_continue(xpr_t *xpr, __uint64_t wrap, short get)
/*
 * Function: 	xpc_notify_continue
 * Purpose:	Check if anyone waiting for a notify should be woken up on
 *		the specified ring.
 * Parameters:	xpr - pointer to xpr to check.
 *		wrap - remote wrap count (new value about to be set)
 *		get  - remote get (new value bout to be set).
 * Returns:	Nothing
 * Notes:	Assumes caller has cached remote get/put values.
 */
{
    register xnq_t	*xn;

    ASSERT(XPR_LOCKED(xpr));

    /*
     * If the wrap count is > that the wrap count we are waiting on, the
     * operation is done. If the wrap count is <, then we are not done 
     * waiting. If it is equal, then we must check if the get pointer on 
     * the remote end is >= the local put pointer. If so, we are done 
     * waiting.
     */
    while (NULL != (xn = xpr->xpr_xn)) {
	if (xpc_check_notify_complete(xn, wrap, get)) {
	    xpr->xpr_xn = xn->xn_next; /* unlink it  */
	    if (!xn->xn_next) {
		xpr->xpr_xn_end = NULL;
	    }
	    if (xn->xn_xpm_flags & XPM_F_SYNC) { /* waiting thread? */
		sv_signal(&xn->xn_sv);
	    } else {
		ASSERT(xn->xn_rtn);
		/* should be ok to unlock here because each time through
		 * loop we look at xn from the xpr->xpr_xn and not just
		 * go down it as a link list */
		XPR_UNLOCK(xpr);
		xn->xn_rtn(xpcDone, xpr, xpr->xpr_partid, xpr->xpr_channel, 
			   xn->xn_par);
		XPR_LOCK(xpr);
	    }
	} else {
	    return;
	}
    }
    xpr->xpr_flags &= ~XPR_F_NOTIFY;
}

static	xpc_t
xpc_notify_wait(xpr_t *xpr, xpm_t *xpm, xpc_async_rtn_t r, void *key)
/*
 * Function: 	xpc_notify_wait
 * Purpose:	Wait until the message specified has been consumed by
 *		the remote end of the channel.
 * Parameters:	xpr - link to wait on.
 *		xpm - message to wait on.
 *		key - used for async routine notification.
 * Returns:	xpc_t
 * Notes:	XPC_SLOCK held on entry, released on return.
 */
{
    xnq_t	*xn, *txn, *pxn;
    short 	idx;
    xpc_t	xr;
	
    idx = ((__psunsigned_t)xpm-(__psunsigned_t)xpr->xpr_lr) / xpr->xpr_lsize;

    xn = &xpr->xpr_xn_list[idx];	/* pick up our xnq entry */
    
    XPR_LOCK(xpr);

    if (xpr->xpr_flags & XPR_F_ERROR) {
	xr = xpr->xpr_xr;
	XPR_UNLOCK(xpr);
	XPR_SUNLOCK(xpr);
    } else {
	xn->xn_next = NULL;
	xn->xn_rtn  = r;
	xn->xn_par  = key;
	xn->xn_xpm_flags = xpm->xpm_flags;
	if (!xpc_check_notify_complete(xn, xpr->xpr_rwrap, 
				       xpr->xpr_wrgp.gp_get)) {
	    if (!xpr->xpr_xn) {		/* If nothing queued, simple */
		xpr->xpr_xn_end = xpr->xpr_xn = xn;
		xpr->xpr_flags |= XPR_F_NOTIFY; /* signal notify waiting */
	    } else if (xpc_check_notify_complete(xpr->xpr_xn_end, 
						 xn->xn_wrap, xn->xn_put)) {
		/*
		 * Normally, a new entry will belong at the end of the queue. 
		 * Check if that is the case.
		 */
		
		xpr->xpr_xn_end->xn_next = xn; /* Belong at end */
		xpr->xpr_xn_end = xn;	/* keep tail in sync */
	    } else {			/* Scan for correct location */
		xpr->xpr_sendooocnt++;	/* out of order count */
		for (txn = xpr->xpr_xn, pxn = NULL; 
		     xpc_check_notify_complete(txn, xn->xn_wrap, 
					       xn->xn_put);
		     txn = txn->xn_next) {
		    pxn = txn;
		}
		    
		ASSERT(!pxn || 
		       !xpc_check_notify_complete(txn, xn->xn_wrap, 
						  xn->xn_put));
		if (!pxn) {		/* insert at head of Q */
		    xn->xn_next = xpr->xpr_xn;
		    xpr->xpr_xn = xn;
		} else {
		    xn->xn_next = pxn->xn_next;
		    pxn->xn_next = xn;
		}
	    }
	    XPR_SUNLOCK(xpr);		/* release write lock for sleep */
	    if (xpm->xpm_flags & XPM_F_SYNC) {
		XPR_UNLOCK_SYNC(xpr, &xn->xn_sv); /* good night */
		/*
		 * At this point, the xnq struct for our message can no longer 
		 * be considered "usable". The wakeup from the wait (SYNC) 
		 * effectively frees the xnq entry, since the message can be 
		 * reused any time after the message is "consumed", not to be
		 * mistaken with when we notice it is consumed.
		 */

		/* we need to make sure that we don't return xpr->xpr_xr
		 * if we have gotten a remote shutdown and started to 
		 * shutdown after the write we just sent was consumed,
		 * before we were told that it was finished successfully */
		XPR_LOCK(xpr);
		if(xpr->xpr_flags & XPR_F_DRAINED) {
		    xr = xpcSuccess;
		} else {
		    xr = xpr->xpr_xr;
		}
	    } else {
		xr = xpr->xpr_xr;
	    }
	} else {
	    /*
	     * If message was async, must report done, sync just 
	     * reports success. Check flags BEFORE releasing SLOCK
	     * since message could be reused at that point.
	     */
	    xr = (xn->xn_xpm_flags & XPM_F_SYNC) ? xpcSuccess : xpcDone;
	    XPR_SUNLOCK(xpr);
	    XPR_UNLOCK(xpr);
	    return(xr);
	}

    }

    XPR_UNLOCK(xpr);
    return(xr);
}

void
xpc_done(xpr_t *xpr, xpm_t *xpm)
/*
 * Function: 	xpc_done
 * Purpose:	Indicate completion of pulling accross a specific message
 *		and associated data.
 * Parameters:	xpr - pointer to ring assocated with message.
 *		xpm - message pointer as passed out on async notification.
 * Returns: 	Nothing.
 */
{
    unsigned char f;			/* all completed message flags */
    xpm_t	*m, *nm;
    short	get;

    ASSERT(!(xpm->xpm_flags & XPM_F_DONE));
    XPR_LOCK(xpr);
    xpr->xpr_rcvcnt++;
/*    xpr->xpr_rcv_bytes += xpm->xpm_tcnt;*/
    ASSERT(xpr->xpr_done);
    xpm->xpm_flags |= XPM_F_DONE;
    if (xpr->xpr_done == xpm) {		/* head of done queue */
	f = 0;
	for (m = xpr->xpr_done, nm = m->xpm_ptr, f = m->xpm_flags; 
	     nm && (nm->xpm_flags & XPM_F_DONE); nm = nm->xpm_ptr) {
	    f |= nm->xpm_flags;
	    m = nm;
	}

	/* m points to last message completed, update our get pointer */

	get = ((paddr_t)m-(paddr_t)xpr->xpr_crr)/xpr->xpr_rsize;
	XPR_INC(get, xpr->xpr_rlimit);
	xpr->xpr_lgp->gp_get = get;

	if (f & (XPM_F_NOTIFY|XPM_F_INTERRUPT)) {
	    XPR_SEND_INT(xpr);
	}

	/* nm points to next message - first message NOT done */

	xpr->xpr_done = nm;
	if (NULL == nm) {
	    xpr->xpr_done_tail = NULL;
	}
    }
    XPR_UNLOCK(xpr);
}

xpc_t
xpc_allocate(xpr_t *xpr, xpm_t **xpm)
/*
 * Function: 	xpc_allocate
 * Purpose:	Allocate a message on a given channel ring.
 * Parameters:	xpr - pointer to ring to allocate outgoing message on.
 *		xpm - pointer to message pointer filled in on return.
 * Returns:	Pointer to message on success, NULL if failed.
 */
{
    xpm_t	*m = NULL;
    xpc_t	xr;

    /* rqmond is not allowed to send messages */
    ASSERT( !(curthreadp->k_flags&KT_SERVER));

    if (xpr->xpr_flags & XPR_F_ERROR) {
	xr = xpr->xpr_xr;
    } else if (!(xpr->xpr_flags & XPR_F_AVAIL)) {
	xr = xpcDisconnected;
    } else {
	XPR_SLOCK(xpr);
	if (xpcSuccess == (xr = xpc_allocate_xpm(xpr, &m))) {
	    ASSERT(!(m->xpm_flags & XPM_F_READY));
	    m->xpm_ptr = NULL;
	    if (!xpr->xpr_ready_tail) {
		xpr->xpr_ready = m;
	    } else {
		xpr->xpr_ready_tail->xpm_ptr = m;
	    }
	    xpr->xpr_ready_tail = m;
	}
	XPR_SUNLOCK(xpr);
    }
    *xpm = m;
    return(xr);
}

xpc_t
xpc_send_notify(xpr_t *xpr, xpm_t *xpm, xpc_async_rtn_t r, void *key)
/*
 * Function: 	xpc_send
 * Purpose:	Send a cross partition message previously allocated using 
 *		xpcAllocate. The routine is asynchronous, it does NOT
 *		wait for a reply.
 * Parameters:	xpr - pointer to ring to send on.
 *		xpm  - pointer to message allocated with xpcAllocate.
 * Returns:	xpc 
 * Notes:	This routine does NOT free any memory pointer to by the
 *		buffer pointers. 
 *
 *		If the message flag XPCM_F_NOTIFY is set, this routine 
 *		sends the message synchronously with regards to me
 *		message system - this only means the remote end of the
 *		channel has receive the message and all associated data. 
 *		It allows the sender to free up or re-use any buffers 
 *		referenced by the message, but does NOT mean the message
 *		has been processed at the remote end by a receiver. If the
 * 		message is not marked with NOTIFY, non -immediate buffers 
 *		must not be altered until the sender knows (through some 
 *		higher level protocol) that the receiver has received the 
 *		data.
 */
{
    xpc_t	xr;
    xpm_t	*m, *nm;
    short	put;

    XPC_CHECK_XPM(xpr, xpm);

    ASSERT(!(xpm->xpm_flags & XPM_F_READY));
    ASSERT(xpr->xpr_ready);

    if (xpm->xpm_flags & XPM_F_SYNC) {
	xpm->xpm_flags |= XPM_F_NOTIFY;
    }
    if (xpm->xpm_flags & XPM_F_NOTIFY) {
	xpm->xpm_flags |= XPM_F_INTERRUPT;
    }

    XPR_SLOCK(xpr);
    xpm->xpm_flags |= XPM_F_READY;

    xpr->xpr_sendcnt++;
    DPRINTXPM("xpc_send", xpr, xpm);

    /* See if this message was holding things up */

    if (xpr->xpr_ready == xpm) {
	for (m = xpr->xpr_ready, nm = m->xpm_ptr; 
	     nm && (nm->xpm_flags & XPM_F_READY); nm = nm->xpm_ptr) {
	    m = nm;
	}

	/* 
	 * m points to last message to send, local put pointer must be updated
	 * to point to message AFTER current message).
	 */

	put = ((paddr_t)m - (paddr_t)xpr->xpr_lr) / xpr->xpr_lsize;
	XPR_INC(put, xpr->xpr_llimit);
	xpr->xpr_lgp->gp_put = put;
	XPR_SEND_INT(xpr);		/* Interrupt remote side */ 

	/* nm points to next message - first message NOT sent */

	xpr->xpr_ready = nm;
	if (NULL == nm) {
	    xpr->xpr_ready_tail = NULL;
	}
    }
    if (xpm->xpm_flags & XPM_F_NOTIFY) {
	xr = xpc_notify_wait(xpr, xpm, r, key);
    } else {
	XPR_SUNLOCK(xpr);
	xr = xpcSuccess;
    }
    return(xr);
}

xpc_t
xpc_send(xpr_t *xpr, xpm_t *xpm) 
/*
 * Function: 	xpc_send
 * Purpose:	To send a previously allocated message.
 * Parameters:	xpr - pointer to ring to send message on.
 *		xpm - pointer to message to send.
 * Returns:	xpc_t
 */

{
    ASSERT(!(xpm->xpm_flags & XPM_F_NOTIFY) || (xpm->xpm_flags & XPM_F_SYNC));
    return(xpc_send_notify(xpr, xpm, NULL, NULL));
}

static	void
xpc_pull_xpm(bte_handle_t *bte, xpr_t *xpr)
/*
 * Function: 	xpc_pull_xpm
 * Purpose:	Pull across XPM message headers currently pending in a 
 *		remote ring.
 * Parameters:	bte - pointer to BTE to use, NULL causes BTE allocation.
 *		xpr - pointer to ring.
 * Returns:	Nothing, but ring flags may be updated.
 * Notes:	Pulling XPM messages requires holding the XPR_RLOCK. 
 */
{
    paddr_t	rmp, lmp;		/* remote/local message pointer */
    __uint32_t	cnt;
    xpc_t	xr;
    
    ASSERT(XPR_LOCKED(xpr));

    /* How many messages are possible to be pulled ? */

    if (XPR_CRR_PEND(xpr)) {		/* Don't worry until we have to */
	return;
    }

    cnt = XPR_PEND(xpr) - XPR_CRR_PEND(xpr);

    if (cnt) {
	rmp = (paddr_t)xpr->xpr_rr + xpr->xpr_crr_gp.gp_put * xpr->xpr_rsize;
	lmp = (paddr_t)xpr->xpr_crr + xpr->xpr_crr_gp.gp_put * xpr->xpr_rsize;
	if (xpr->xpr_crr_gp.gp_put + cnt > xpr->xpr_rlimit) {
	    cnt = xpr->xpr_rlimit - xpr->xpr_crr_gp.gp_put + 1;
	    xpr->xpr_crr_gp.gp_put = 0;	/* wrapped */
	} else {
	    xpr->xpr_crr_gp.gp_put += cnt;
	}
	xr = xpc_pull_data_fast(bte, rmp, (void *)lmp, cnt * xpr->xpr_rsize);
	if (xpcSuccess != xr) {
	    XPC_SET_ERROR(xpr, xr);
	} 
	xpr->xpr_xpmcnt++;
    }
}

static xpm_t *
xpc_next_xpm(xpr_t *xpr)
/*
 * Function: 	xpc_next_xpm
 * Purpose:	Select the next message header to be processed, and queue it.
 * Parameters:	spr - ring to select message from.
 * Returns:	Pointer to message, or NULL if non or error.
 */
{
    xpm_t	*xpm;
    bte_handle_t *bte;

    bte = bte_acquire(1);
    if (!bte) {
	return(NULL);
    }
    XPR_LOCK(xpr);
    xpc_pull_xpm(bte, xpr);
    bte_release(bte);
    if (XPR_CRR_PEND(xpr) && !(xpr->xpr_flags & XPR_F_ERROR)) {
	xpm = (xpm_t *)XPR_CRMSG(xpr);
	XPR_INC(xpr->xpr_crr_gp.gp_get, xpr->xpr_rlimit);
	xpm->xpm_flags &= ~XPM_F_DONE;
	xpm->xpm_ptr    = NULL;
	if (NULL == xpr->xpr_done) {
	    xpr->xpr_done = xpm;
	} else {
	    xpr->xpr_done_tail->xpm_ptr = xpm;
	}
	xpr->xpr_done_tail = xpm;
    } else {
	xpm = NULL;
    } 
    XPR_UNLOCK(xpr);
    return(xpm);
}

static	void
xpc_receive_xpm(xpr_t *xpr)
/*
 * Function: 	xpr_receive_xpm
 * Purpose:	Receive and process one xpm message. 
 * Parameters:	xpr - ring to receive xpm on.
 * Returns:	Nothing.
 */
{
    xpm_t	*xpm;
    xpc_t	xr;

    xr = XPR_RLOCK(xpr);
    /*
     * Decrement busy after we have to RLOCK to minimize the race with
     * xpc_interrupt counting the number of threads required. If we decrement
     * before RLOCK, if we are suspended for a while we could start 
     * another thread for this message. 
     */
    atomicAddInt(&xpr->xpr_ithreads_busy, -1);
    if (xpcSuccess == xr) {
	xpm = xpc_next_xpm(xpr);
	XPR_RUNLOCK(xpr);
	if (xpm) {
	    xpr->xpr_async_rtn(xpcAsync, xpr, xpr->xpr_partid,
			       xpr->xpr_channel, xpm);
	}
    }
}

xpc_t
xpc_receive(xpr_t *xpr, xpm_t **xpm)
/*
 * Function: 	xpc_receive
 * Purpose:	Retrieve a single incomming message from the message ring.
 * Parameters:	xpr - ring to retrieve message from.
 *		xpm - pointer to meassage pointer filled in on return.
 * Returns:	xpc_t (if not xpcSuccess, *xpm is NULL).
 */
{
    xpc_t	xr;

    if (xr = XPR_RLOCK(xpr)) {
	return(xr);
    }

    if (xpr->xpr_flags & XPR_F_ERROR) {
	XPR_RUNLOCK(xpr);
	*xpm = NULL;
	return(xpr->xpr_xr);
    }
    
    while (!XPR_PEND(xpr)) {
	XPR_RUNLOCK(xpr);
	if (xpr->xpr_flags & XPR_F_NONBLOCK) {
	    return(xpcNoWait);
	}
	xr = xpc_rwait(xpr);
	if (xr) {
	    *xpm = NULL;
	    return(xr);
	}
	if (xr = XPR_RLOCK(xpr)) {
	    return(xr);
	}
    }
    ASSERT(XPR_PEND(xpr));

    *xpm = xpc_next_xpm(xpr);
    XPR_RUNLOCK(xpr);
    if (!*xpm) {
	ASSERT(xpr->xpr_flags & XPR_F_ERROR);
	xr = xpr->xpr_xr;
    } else {
	xr = xpcSuccess;
    }
    return(xr);
}


xpc_t
xpc_poll(xpr_t *xpr, short events, int anyyet, short *reventsp, 
	 struct pollhead **phpp, unsigned int *genp)
/*
 * Function: 	xpc_poll
 * Purpose:	poll/select support.
 * Parameters:	xpr - pointer to ring to poll.
 *		As per poll entry point in device driver.
 * Returns:	xpc_t
 */
{
    XPR_LOCK(xpr);
    if (events & (POLLPRI|POLLRDBAND)) {
	if (xpr->xpr_flags & XPR_F_ERROR) {
	    /* Forget about the rest, just report remote is gone */
	    events &= (POLLPRI|POLLRDBAND);
	} else {
	    events &= ~(POLLPRI|POLLRDBAND);
	    xpr->xpr_flags |= XPR_F_XPOLL;
	}
    } else if (xpr->xpr_flags & XPR_F_ERROR) {
	XPR_UNLOCK(xpr);
	*reventsp = POLLHUP;
	return(xpr->xpr_xr);
    }

    if (events & (POLLIN | POLLRDNORM)) { 
	if (!XPR_PEND(xpr) || !(xpr->xpr_flags & XPR_F_AVAIL)) {
	    events &= ~(POLLIN | POLLRDNORM);
	    xpr->xpr_flags |= XPR_F_RDPOLL;
	}
    }

    if (events & (POLLOUT | POLLWRNORM)) {
	if (!(xpr->xpr_flags & XPR_F_AVAIL) || (0 == XPR_AVAIL(xpr))) {
	    events &= ~(POLLOUT | POLLWRNORM);
	    xpr->xpr_flags |= XPR_F_WRPOLL;
	}
    }

    *reventsp = events;
    if (!anyyet && (0 == events)) {
	*phpp = xpr->xpr_ph;
	/* snapshot pollhead generation number while we hold the lock */
	*genp = POLLGEN(xpr->xpr_ph);
    }

    XPR_UNLOCK(xpr);
    return(xpcSuccess);
}

xpc_t
xpc_avail(partid_t p)
/*
 * Function: 	xpc_avail
 * Purpose:	Check if xpc is available to the specified partition.
 * Parameters:	p - target partition ID to check.
 * Returns:	xpc_t
 */
{
    xpr_t	*cxpr;

    if (!xpc_map) {			/* No XPC map -  */
	return(xpcRetry);		/* Try later */
    } else if ((xpc_map->xpm_flags & XPMP_F_HR)
	       && !(xpc_map->xpm_flags & XPMP_F_HROVRD)) {
	return(xpcNoOvrd);
    }

    cxpr = xpc_rings[p].pr_xpr;

    if (!cxpr) {
	return(xpcRetry);
    } else if (cxpr->xpr_flags & XPR_F_ERROR) {
	return(cxpr->xpr_xr);
    } else if (cxpr->xpr_flags & XPR_F_AVAIL) {
	return(xpcSuccess);
    } else {
	return(xpcBusy);
    }
}

