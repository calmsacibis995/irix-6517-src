
/*
 * File: 	xpc_private.h
 * Purpose:	Internal Cross partition communication structures.
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
#ifndef _ML_SN_XPC_PRIVATE_H_
#define _ML_SN_XPC_PRIVATE_H_ 1

#define	XP_PIPE_SIZE		(BTE_LEN_MINSIZE * 4)
#define	XP_CONTROL_CHANNEL	0
#define	XP_CONTROL_ELIMIT	/*64*/ 16
#define	XP_CONTROL_ESIZE	/* (NBPP/XP_CONTROL_ELIMIT)*/ 256

/*
 * Define: 	XPC_SCALE_TO
 * Purpose: 	Sacle factor for XPC timeouts, default is 1, but increased
 *		to 10 if DEBUG kernel since they take unreasonably long
 *		time to do anything.
 */
#if defined(DEBUG)
#   define	XPC_SCALE_TO	10
#else
#   define	XPC_SCALE_TO	1
#endif

/*
 * Defines: XPC_VERSION_
 * Purpose: Version number of XPC implementation. XPC can always talk to 
 *	    versions with same major #, and never talk to versions with 
 *	    a different version.
 */
#define	_XPC_VERSION(_major, _minor)	(((_major) << 4) | (_minor))
#define	XPC_VERSION_MAJOR(_v)		((_v) >> 4)
#define	XPC_VERSION_MINOR(_v)		((_v) & 0xf)

#define	XPC_VERSION_COMP(_v1, _v2)	\
    (XPC_VERSION_MAJOR(_v1) == XPC_VERSION_MAJOR(_v2))

/*
 * Typedef: xpmc_t (Cross Partition Control Message)
 * Purpose: Defines the format of a control message sent accross the
 *	    XPC interface.
 */ 
typedef	struct xpmc_s {
    unsigned char 	xpmc_cmd;
#   define	XPMC_C_INVALID	0
#   define	XPMC_C_OPEN	1	/* Open connection request */
#   define	XPMC_C_OPEN_REPLY 2	/* Reply to open request */
#   define	XPMC_C_NOP	3	/* Do nothing */
#   define	XPMC_C_TARGET	4	/* Set target nasid */
#   define	XPMC_C_SHUTDOWN	5	/* Terminate the connection */
    __uint32_t		xpmc_channel;	/* channel # in question */
    xpc_t		xpmc_xr;	/* xpc error */
    __uint64_t		xpmc_parm[4];	/* Command parameter */
} xpmc_t;
#define	XPMC_PARM(_c, _i, _t) ((_t)((_c)->xpmc_parm[_i]))

/*
 * Typedef: gp_t
 * Purpose: Maps the get/put values for the cross partition values. 
 * Notes: The follow table of values is used duing control ring setup
 *	  to idicate the state of a connection.
 *
 *		GET	PUT	Meaning
 *	-------+-----+-------+----------------------------------
 *	RESET  | -1  |  -1   | Ring is not connected, if was 
 *	       |     |       | connected, error occured.
 *	-------+-----+-------+----------------------------------
 *	CONNECT| -1  |  -2   | Connection reqeusted
 *	-------+-----+-------+----------------------------------
 *	READY  | >=0 |  >=0  | Connection established
 *	-------+-----+-------+----------------------------------
 */
typedef	struct gp_t {
    short	gp_put;			/* put pointer */
    short	gp_get;			/* get pointer */
} gp_t;


#define	GP_MAGIC		0x67706770	/* "gpgp" */
#define	GP_MAGIC_SET(_x)	(*(__uint32_t *)&(_x)) = GP_MAGIC
#define	GP_MAGIC_TEST(_x)	((*(__uint32_t *)&(_x)) == GP_MAGIC)

/*
 * Macros: GP_SET
 *	   GP_TEST
 * Purpose: Set and test local working GP values.
 */
#define	GP_SET(_gp, _g, _p)	(_gp).gp_get = (_g); (_gp).gp_put = (_p)

#define	GP_TEST(_x, _g, _p, _c)					\
    (((_x)->xpr_lgp->gp_get _c (_g)) && ((_x)->xpr_lgp->gp_put _c (_p)))

#define	XPR_GP_SET_RESET(_x)   \
    GP_SET((_x)->xpr_wlgp, -1, -1); GP_SET(*(_x)->xpr_lgp, -1, -1); 
#define	XPR_GP_SET_CONNECT(_x) \
    GP_SET((_x)->xpr_wlgp, -1, -2); GP_SET(*(_x)->xpr_lgp, -1, -2); 
#define	XPR_GP_SET_READY(_x)   \
    GP_SET((_x)->xpr_wlgp, 0, 0); GP_SET(*(_x)->xpr_lgp, 0, 0)
#define	XPR_GP_SET_ERROR(_x)   XPR_GP_SET_RESET(_x)

#define	XPR_GP_TEST_RESET(_x)	GP_TEST((_x), -1, -1, ==)
#define	XPR_GP_TEST_CONNECT(_x)	GP_TEST((_x), -1, -2, ==)
#define	XPR_GP_TEST_READY(_x)	GP_TEST((_x), 0, 0, >= )
#define	XPR_GP_TEST_ERROR(_x)	GP_TEST((-x), 0, 0, <)

/*
 * Macros: GP_RSET
 *	   GP_TTEST
 * Purpose: Set and test remote GP values (in local partition copies only).
 */
#define	GP_RSET(_x, _g, _p)					\
    (_x)->xpr_wrgp.gp_get = (_g);				\
    (_x)->xpr_wrgp.gp_put = (_p)

#define	GP_RTEST(_x, _g, _p, _c)				\
    (((_x)->xpr_rgp->gp_get _c (_g)) && ((_x)->xpr_rgp->gp_put _c (_p)))

#define	XPR_GP_RSET_READY(_x)	GP_RSET((_x), 0, 0)

#define	XPR_GP_RTEST_RESET(_x)	GP_RTEST((_x), -1, -1, ==)
#define	XPR_GP_RTEST_CONNECT(_x) GP_RTEST((_x), -1, -2, ==)
#define	XPR_GP_RTEST_READY(_x)	GP_RTEST((_x), 0, 0, >=)
#define	XPR_GP_RTEST_ERROR(_x)	GP_RTEST((_x), 0, 0, <)

#define	GP_SIZE		\
    (((sizeof(gp_t) * (XP_RINGS+1)) + XP_ALIGN_SIZE - 1) & XP_ALIGN_MASK)

/*
 * Macros: GP_EQUAL
 *	   GP_GET
 * Purpose: Compare or pick up both the get/put values in an atomic 
 * 	    load/compare.
 */
#define	GP_EQUAL(_g1, _g2)	\
    (*(__uint32_t *)&(_g1)) == (*(__uint32_t *)&(_g2))
#define	GP_GET(_g, _v)  *(__uint32_t *)&(_v) = *(__uint32_t *)&(_g)

/*
 * Typedef: xnq_t
 * Purpose: Defines notify Queue entry.
 */
typedef	struct xnq_s {
    struct xnq_s	*xn_next;	/* forward link */
    __uint64_t		xn_wrap;	/* wrap counter */
    sv_t		xn_sv;		/* sync variable sleeping on */
    unsigned char	xn_xpm_flags;	/* Copy of xpm flags */
    unsigned short	xn_put;		/* local put index */
    xpc_async_rtn_t	xn_rtn;		/* notify routine */
    void		*xn_par;	/* notify routine parameter */
} xnq_t;


/* 
 * Define:	_XPC_HISTORY_DEBUG
 * Purpose: 	For tracking history of flags and messages sent and 
 *		received by xpc control ring for a particular ring.  
 *		The messages are printed out with the "xpr"
 * 		command in idbg.
 */
#if defined(DEBUG)
#   define 	_XPC_HISTORY_DEBUG		0
#endif


#if _XPC_HISTORY_DEBUG

/* 
 * Define:	XPM_HISTORY_MAX
 * Purpose:	Number of history items to save for xpc control ring
 */
#define XPM_HISTORY_MAX		20

/* 
 * Typedef:	xpm_history_t
 * Purpose:	one piece of history for our ring.
 */

typedef struct xpm_history_s {
#define	XPM_HISTORY_PRE_SEND	0		/* saved before send */
#define XPM_HISTORY_POST_SEND	1		/* saved after send */
#define XPM_HISTORY_RECEIVE	2		/* saved process recv */
#define XPM_HISTORY_CONNECT	3		/* xpc_connect exec */
#define XPM_HISTORY_DISCONNECT	4		/* xpc_disconnect */
    unsigned char	xpm_history_type; 	/* send or receive */
    unsigned char 	xpm_history_cmd;	/* command */
    __uint32_t		xpm_history_flags;	/* flags currently set */
    xpc_t		xpm_history_xr;		/* xpc error */
} xpm_history_t;

/* 
 * Macro:	XPM_HISTORY_STATE
 * Purpose:	Save the state at this particular instant in the code
 */
#define XPM_HISTORY_STATE(_type, _cmd, _xr, _xpr)			\
    {									\
	(_xpr)->xpr_history_lock_spl = 					\
		    mutex_spinlock(&((_xpr)->xpr_history_lock));	\
        (_xpr)->xpr_history[(_xpr)->xpr_history_index]			\
	            .xpm_history_type = (_type);			\
        (_xpr)->xpr_history[(_xpr)->xpr_history_index]			\
		    .xpm_history_cmd = (_cmd);				\
        (_xpr)->xpr_history[(_xpr)->xpr_history_index]			\
		    .xpm_history_flags = (_xpr)->xpr_flags;		\
        (_xpr)->xpr_history[(_xpr)->xpr_history_index]			\
	            .xpm_history_xr = (_xr);				\
	(_xpr)->xpr_history_index++;					\
        if((_xpr)->xpr_history_index == XPM_HISTORY_MAX) {		\
	    (_xpr)->xpr_history_index = 0;				\
	    (_xpr)->xpr_history_wrapped = 1;				\
	}								\
	mutex_spinunlock(&((_xpr)->xpr_history_lock), 			\
			 (_xpr)->xpr_history_lock_spl);			\
    }
#else
#define XPM_HISTORY_STATE(_type, _cmd, _xr, _xpr)
#endif /* _XPC_HISTORY_DEBUG */

/*
 * Typedef: xpr_t
 * Purpose: Maps out the control structure maintained for each 
 *	    channel.  This structure is private to a partition, and
 *	    is NOT shared accross the partition wall.
 */
typedef	struct xpr_s {
#   define	XPR_MAGIC	0x78707272696e6700  /* "xprring\0" */
#   define	XPR_CHECK(_x)	ASSERT((_x)->xpr_magic == XPR_MAGIC)
    __uint64_t	xpr_magic;		/* magic number */
    partid_t	xpr_partid;		/* remote partition id */
    lock_t	xpr_lock;		/* lock:  for updating this struct */
    int		xpr_lock_spl;		/* SPL for spinlock */
    __uint32_t	xpr_flags;		/* General flags */

    /* connect/shutdown in progress flags */
#   define	XPR_F_SHUTDOWN	0x1	/* link being shut down */
#   define	XPR_F_RSHUTDOWN 0x2	/* remote end shut down */
#   define	XPR_F_CONNECT 	0x4	/* local conenct pending */
#   define	XPR_F_RCONNECT	0x8 	/* Remote connect pending */
#   define	XPR_F_REPLY	0x10	/* Local reply sent */
#   define	XPR_F_RREPLY	0x20	/* Remote reply received */

    /* error flags */
#   define	XPR_F_ERROR	0x40	/* fatal error - in xr */

    /* current state of xpr ring */
#   define	XPR_F_ALLOC	0x80	/* Rings allocated */
#   define	XPR_F_AVAIL	0x100	/* Local unix ref/open */
#   define	XPR_F_OPEN	0x200	/* Device is "open" */
#   define	XPR_F_AOPEN 	0x400	/* Async open */
#   define	XPR_F_NONBLOCK	0x800	/* Non-blocking I/O */
#   define    	XPR_F_BWAIT	0x1000	/* breakable waits (some signals OK) */
#   define	XPR_F_DRAINED	0x2000	/* The local ring has been drained */

    /* busy doing some other operation */
#   define	XPR_F_BUSYCON	0x4000	/* Busy in sending connecting msg */
#   define	XPR_F_BUSYDIS	0x8000	/* Busy in sending disconnect msg */
#   define	XPR_F_ERRBUSY  	0x10000	/* xpc_set_error in progress */ 
#   define	XPR_F_CACHEBUSY 0x20000	/* xpc_cache_remote_gp in progress */

    /* need to do this operation */
#   define	XPR_F_REPLYPEND 0x40000	/* Open reply busy to be sent */
#   define	XPR_F_FREEPEND 	0x80000	/* xpc_process_disconnect pending */

    /* select flags */
#   define	XPR_F_RDPOLL	0x100000	/* Read poll/select active */
#   define	XPR_F_WRPOLL	0x200000	/* Write poll/select active */
#   define	XPR_F_XPOLL	0x400000	/* X condition poll/select 
						 * active */

    /* reading and writing waiting */
#   define	XPR_F_RD_WAIT	0x800000	/* Reader waiting */
#   define	XPR_F_WR_WAIT	0x1000000	/* Write waiting */
    
    /* notification necessary */
#   define	XPR_F_NOTIFY	0x2000000	/* completion wait */


    xpc_t	xpr_xr;			/* Error for permanent failure */
    int		xpr_xr_line;		/* Line # error set from */
    volatile hubreg_t  *xpr_target;	/* target for interrupts */
    struct xpr_s *xpr_control;		/* Pointer to control ring */
    int		xpr_channel;		/* logical link # (/hw/xp/X/link */

    sv_t	xpr_rdwait_sv;		/* read data pending possibly */
    sv_t	xpr_wrwait_sv;		/* write waiting for notification */
    sema_t      xpr_rd_sema;		/* receive serialization */
    mutex_t    	xpr_wr_lock;		/* send serialization */

    int		xpr_ithreads_active;	/* # interrupt threads running */
    int		xpr_ithreads_max;	/* max # ithreads allowed to run */
    int		xpr_ithreads_pending;   /* # ithreads waiting on this ring */
    int		xpr_ithreads_busy;	/* # started threads */

    __int32_t	xpr_llimit;		/* local entry count */
    __int32_t	xpr_lsize;		/* local message size */
    __int32_t	xpr_rlimit;		/* remote entry count */
    __int32_t	xpr_rsize;		/* remote message size */

    /* Notify Queue values */

    xnq_t	*xpr_xn;		/* notify queue */
    xnq_t	*xpr_xn_end;		/* notify queue tail */
    xnq_t	*xpr_xn_list;		/* array of all XN entries */

    __uint64_t	xpr_rwrap;		/* remote wrap count */
    __uint64_t	xpr_lwrap;		/* local wrap count */

    xpm_t	*xpr_lr;		/* local ring */
    xpm_t	*xpr_rr;		/* remote ring */
    xpm_t	*xpr_crr;		/* remote ring cached headers */

    xpm_t	*xpr_done;		/* Q of received, active msgs */
    xpm_t 	*xpr_done_tail;		/* Tail of Q */

    xpm_t	*xpr_ready;		/* Q of allocated, unsent messages */
    xpm_t	*xpr_ready_tail;	/* Tail of Q */

    /* Cached copies of local and remote XP variables */

    gp_t	xpr_wlgp;		/* Working local Get/Put values */
    gp_t	xpr_wrgp;		/* Working remote Get/Put values */
    gp_t	*xpr_lgp;		/* local Get/Put values */
    gp_t	*xpr_rgp;		/* remote Get/Put values */
    gp_t	xpr_crr_gp;		/* Cache remote ring GPs */

    /* Functions to call when events occur */

    xpc_async_rtn_t xpr_async_rtn;	/* Async notification routine */

    struct pollhead *xpr_ph;		/* For polling stuff */

    __uint64_t	xpr_sendooocnt;		/* Send out of order count */
    __uint64_t	xpr_sendcnt;
    __uint64_t	xpr_rcvcnt;
    __uint64_t	xpr_xpmcnt;		/* # pulls of headers */
    __uint64_t	xpr_allcnt;		/* # failed allocates */

#if _XPC_HISTORY_DEBUG
    xpm_history_t xpr_history[XPM_HISTORY_MAX];	/* history info */
    int xpr_history_index;			/* next to save */
    int xpr_history_wrapped;			/* has history wrapped */
    lock_t xpr_history_lock;			/* lock for history info */
    int xpr_history_lock_spl;			/* SPL for spinlock */
#endif /* _XPC_HISTORY_DEBUG */

} xpr_t;

#define	XPR_LOCK(_x)		(_x)->xpr_lock_spl = \
                                    mutex_spinlock(&(_x)->xpr_lock)
#define	XPR_LOCKED(_x)		((_x)->xpr_lock & 1)
#define	XPR_UNLOCK(_x)		mutex_spinunlock(&(_x)->xpr_lock, \
						 (_x)->xpr_lock_spl)
#define	XPR_UNLOCK_SYNC(_x, _s)	sv_wait((_s), PZERO, &(_x)->xpr_lock, 	\
					(_x)->xpr_lock_spl)

#define	XPR_RGO(_x)	ASSERT(XPR_LOCKED(_x));	 			\
                        if((_x)->xpr_flags & XPR_F_RD_WAIT) {		\
			    (_x)->xpr_flags &= ~XPR_F_RD_WAIT;		\
                            sv_broadcast(&(_x)->xpr_rdwait_sv);		\
			}

#define	XPR_SGO(_x)	ASSERT(XPR_LOCKED(_x)); 			\
                        if((_x)->xpr_flags & XPR_F_WR_WAIT) {		\
			(_x)->xpr_flags &= ~XPR_F_WR_WAIT;		\
			    sv_broadcast(&(_x)->xpr_wrwait_sv);		\
			}

#define	XPR_SLOCK(_x)	mutex_lock(&(_x)->xpr_wr_lock, PZERO)
#define	XPR_RLOCK(_x)	(psema(&(_x)->xpr_rd_sema,			\
			       ((_x)->xpr_flags & XPR_F_BWAIT) ?	\
			       PWAIT|PCATCH : PZERO) ?			\
			 xpcIntr : xpcSuccess)
#define	XPR_TRY_RLOCK(_x)	cpsema(&(_x)->xpr_rd_sema)
#define	XPR_SUNLOCK(_x)	mutex_unlock(&(_x)->xpr_wr_lock)
#define	XPR_RUNLOCK(_x)	vsema(&(_x)->xpr_rd_sema)

/*
 * Macro: XPR_SEND_INT
 * Purpose: Issue cross call interrupt to notify remote end of 
 *	    ring event.
 * Parameters: _x - pointer to ring structure to issue interrupt for
 */
#if defined(SN)
#define	XPR_SEND_INT(_x)	*((_x)->xpr_target) = 0
#else
#define	XPR_SEND_INT(_x)	{extern int sendint(xpr_t *); sendint(_x);}
#endif

/*
 * Typedef:	xpe_t
 * Purpose:	Defines cross partition parameters passed using the 
 *		KLDIR entry. 
 */
typedef struct xpe_s {
    paddr_t		xpe_control;	/* paddr of control ring*/
    paddr_t		xpe_gp;		/* paddr of get/put values */
    __uint32_t		xpe_cnt;	/* # rings supported */
    char		xpe_fill[12];	/* For future use - MUST BE 0 */
} xpe_t;

/*
 * Typedef: 	pr_t
 * Purpose:	Maps out rings on a partition basis. There is
 *		one of these sturctures for each of the other 
 *		partitions, and it tracks the address of both 
 *		the local and remote ring counters, and the local
 *		rings.
 */
typedef	struct pr_s {
    xpr_t	*pr_xpr;		/* Array of rings */
    gp_t	*pr_lgp;		/* New local get/put values */
    gp_t	*pr_rgp;		/* New Real remote get/put address */
    unsigned char pr_rversion;		/* remote version */
    __uint32_t	pr_rflags;		/* remote flags */
    xpe_t	pr_xpe;			/* remote xpe */
    paddr_t	pr_rgpAddr;		/* remote partition get/put ptrs */

#   define	XPR_CONNECT_RATE    (HZ * 1 * XPC_SCALE_TO)
#   define	XPR_CONNECT_TO	    5	/* Give up after 5 */
#   define	XPR_CONNECT_WAIT_X  2	/* multipy wait time of last time by 
					 * this each time */
    int		pr_connect_try;		/* # tries remaining */
    int		pr_connect_wait;	/* how long waited last time */

    lock_t	pr_ggp_lock;		/* serialize pulling of gp */
    sv_t	pr_ggp_sv;		/* Sync variable to wait on */
    volatile __uint64_t	pr_ggp_id;	/* interrupt id last get */
    volatile __uint64_t	pr_ggp_busy;	/* Busy interrupt id */
    volatile __uint64_t	pr_ggp_waitid;	/* Highest waiting interrupt id */

    __int64_t	pr_ggp_cnt;		/* Total cache remote calls */
    __int64_t	pr_ggp_pcnt;		/* Total GP pulls */
    __int64_t	pr_ggp_wcnt;		/* Total wait counts */
} pr_t;

/*
 * Typedef:	xp_map_t
 * Purpose:	Defines the full cross partition paramter area. 
 */
typedef struct xp_map_s {
    __uint64_t		xpm_magic;	/* magic #, "xparmap\0" */
#   define		XPM_MAGIC	0x787061726d617000LL
    unsigned char	xpm_version;
#   define		XPC_VERSION _XPC_VERSION(1,1) /* 1.1 now */
    __uint32_t	 	xpm_flags;
#   define		XPMP_F_VALID	0x01 /* Flags valid */
#   define		XPMP_F_HR	0x02 /* downrev hubs */
#   define		XPMP_F_HROVRD	0x04 /* hubrev override */
    xpe_t		xpm_xpe[MAX_PARTITIONS];
} xp_map_t;
				


/*
 * Define: 	XPR_RMSG
 *		XPR_CRMSG
 *		XPR_LMSG
 * Purpose: 	Return a pointer to the "next" message the Local/Remote ring.
 * Parameters:  _x - pointer to the ring
 * Notes: 	Uses WORKING pointers.
 */
#define	XPR_RMSG(_x)	\
    ((paddr_t)((_x)->xpr_rr) + (_x)->xpr_wlgp.gp_get * (_x)->xpr_rsize)
#define	XPR_CRMSG(_x)	\
    ((paddr_t)((_x)->xpr_crr) + (_x)->xpr_crr_gp.gp_get * (_x)->xpr_rsize)
#define	XPR_LMSG(_x) 	\
    ((paddr_t)((_x)->xpr_lr) + (_x)->xpr_wlgp.gp_put * (_x)->xpr_lsize)

#define	XPR_NQ(_x)	&((_x)->xpr_xn_list[(_x)->xpr_wlgp.gp_put])


/*
 * Macro: 	XPR_AVAIL
 *		XPR_PEND
 *		XPR_NEXT
 * Purpose: 	Determine the number of entires available for "put" on
 * 	    	local ring.
 * Parameters: _x - pointer to local ring
 * Notes: Uses CACHED copies of local/remote variables.
 */
#define	XPR_INC(_x, _l)		(_x) = ((_x) == (_l) ? 0 : (_x) + 1)

#define	XPR_PEND(_x)							\
    ((_x)->xpr_wrgp.gp_put - (_x)->xpr_crr_gp.gp_get + 			\
     (((_x)->xpr_wrgp.gp_put<(_x)->xpr_crr_gp.gp_get) ? (_x)->xpr_rlimit+1:0))
#define	XPR_CRR_PEND(_x)						\
    ((_x)->xpr_crr_gp.gp_put - (_x)->xpr_crr_gp.gp_get +		\
     (((_x)->xpr_crr_gp.gp_put<(_x)->xpr_crr_gp.gp_get)?(_x)->xpr_rlimit+1:0))
#define XPR_AVAIL(_x)    ((_x)->xpr_wrgp.gp_get - (_x)->xpr_wlgp.gp_put +\
        (((_x)->xpr_wlgp.gp_put < (_x)->xpr_wrgp.gp_get)		\
            ? -1 : (_x)->xpr_llimit))

#define XPR_LR_EMPTY(_x) 						\
     ((_x)->xpr_wrgp.gp_get == (_x)->xpr_wlgp.gp_put)

/*
 * Define: 	XPR_INC/DEC_GET/PUT
 * Purpose: 	Add/Sub using the ring modulo arithmetic.
 * Parameters:	x - base value
 * Notes:	Result stored in "x"
 */

#define	XPR_INC_GET(_x, _g)	(_g)=XPR_INC(_g), (_x)			\
    (_x)->xpr_wlgp.gp_get = 						\
        XPR_INC((_x)->xpr_wlgp.gp_get, (_x)->xpr_rlimit)
    
#define	XPR_INC_PUT(_x)							\
    if (!((_x)->xpr_wlgp.gp_put =					\
          XPR_INC((_x)->xpr_wlgp.gp_put, (_x)->xpr_llimit))) 		\
        (_x)->xpr_lwrap++

#if defined(DEBUG)
/*
 * Function: 	XPR_GP_LSTATE
 *		XPR_GP_RSTATE
 * Purpose:	Returns pointer to string describing the current
 *		decoded GP values. Used for debugging only.
 */
static __inline char *
XPR_GP_LSTATE(xpr_t *xpr)
{
    if (XPR_GP_TEST_RESET(xpr))		return("Reset");
    if (XPR_GP_TEST_CONNECT(xpr))	return("Connect");
    if (XPR_GP_TEST_READY(xpr))		return("Ready");
    return("***");
}

static __inline char *
XPR_GP_RSTATE(xpr_t *xpr)
{
    if (XPR_GP_RTEST_RESET(xpr))	return("Reset");
    if (XPR_GP_RTEST_CONNECT(xpr))	return("Connect");
    if (XPR_GP_RTEST_READY(xpr))	return("Ready");
    return("***");
}

#endif

#endif /* _ML_SN_XPC_PRIVATE_H_ */
