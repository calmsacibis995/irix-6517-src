/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.  		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#define _TDEBUG       0

#include <stdarg.h>

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/ksynch.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <ksys/vfile.h>
#include <sys/vsocket.h>
#include <ksys/fdt.h>
#include <sys/termio.h>
#include <sys/strmp.h>
#include <sys/tsd.h>
#include <sys/debug.h>

#include <sys/kstropts.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <string.h>

#if	_TDEBUG
#   define	TELOPTS			/* To get debugging info */
#   define	TELCMDS			/* More debugging info */
#endif

#include <arpa/telnet.h>

#include <bsd/sys/socketvar.h>



#ifndef	FALSE
#   define	FALSE	0
#endif

#ifndef	TRUE
#   define	TRUE	!(FALSE)
#endif

/*
 * Macro: TDEBUG
 * Purpose: if TDEBUG is defined, compiles into printf statments, otherwise
 *          does not generate code.
 */
#if _TDEBUG > 1
#   define	TDEBUG2(x)	tn_debug x
#else 
#   define	TDEBUG2(x)
#endif

#if _TDEBUG
#   define	TDEBUG(x)	tn_debug x
#else
#   define	TDEBUG(x)	
#endif

#define	TN(maj, min)	\
    (((min) < tn_maxunit) ? (&tn_devices[(min)]) : 0)

#define	TN_MINOR(tn)		((tn) - &tn_devices[0])
#define	TN_MAJOR(tn)		(TSD_MAJOR)

#define	TNLOCK(tn)		mp_mutex_spinlock(&(tn)->tn_lock)
#define	TNUNLOCK(tn, pl)	mp_mutex_spinunlock(&(tn)->tn_lock, pl)
#define	TNCHECKLOCK(tn)		ASSERT(spinlock_islocked(&(tn)->tn_lock))
#define	TNQ(q)			((tn_t *)((q)->q_ptr))

static	mblk_t *tn_telnetReceive(tn_t *, mblk_t *);
static	mblk_t *tn_socketRead(tn_t *);
static	int	tn_connectSocket(tn_t *tn, struct tcssocket_info *);
static	int	tn_sendToRemote(tn_t *, const char *, int, int);
static	void	dooption(tn_t *, int);

/*
 * Variable: tnm_info
 * Purpose: Streams driver ...
 */
static struct module_info tnm_info = {
    1234, 				/* id */
    "tsd",				/* module name */
    0,					/* minimum packet size */
    4096,				/* maximum packet size */
    256,				/* flow control, High */
    0,					/* flow control Low */
    MULTI_THREADED,	                /* locking */
};

static	int	tn_open(queue_t *, dev_t *, int, int, struct cred *);
static	int	tn_close(queue_t *, int, struct cred *);
static	int	tn_rsrv(queue_t *);
static	int	tn_wput(queue_t *, mblk_t *);
static	int	tn_wsrv(queue_t *);

#ifdef NOTDEF
static	void	tn_reqOption(tn_t *, int);
#endif


static struct qinit tn_rinit ={
    NULL, tn_rsrv, tn_open, tn_close, NULL, &tnm_info, NULL
};

static struct qinit tn_winit = {
    tn_wput, tn_wsrv, NULL, NULL, NULL, &tnm_info, NULL
};

struct streamtab tninfo = {
     &tn_rinit, &tn_winit, NULL, NULL
 };


/*
 * We keep track of each side of the option negotiation.
 */

#define	MY_STATE_WILL		0x01
#define	MY_WANT_STATE_WILL	0x02
#define	MY_STATE_DO		0x04
#define	MY_WANT_STATE_DO	0x08

/*
 * Macros to check the current state of things
 */

#define	my_state_is_do(tn, opt)	     	((tn)->tn_options[opt]&MY_STATE_DO)
#define	my_state_is_will(tn, opt)     	((tn)->tn_options[opt]&MY_STATE_WILL)
#define my_want_state_is_do(tn, opt)  	((tn)->tn_options[opt]&MY_WANT_STATE_DO)
#define my_want_state_is_will(tn, opt)  ((tn)->tn_options[opt]&MY_WANT_STATE_WILL)

#define	my_state_is_dont(tn, opt)	(!my_state_is_do(tn, opt))
#define	my_state_is_wont(tn, opt)	(!my_state_is_will(tn, opt))
#define my_want_state_is_dont(tn, opt)	(!my_want_state_is_do(tn, opt))
#define my_want_state_is_wont(tn, opt)	(!my_want_state_is_will(tn, opt))

#define	set_my_state_do(tn, opt)	((tn)->tn_options[opt] |= MY_STATE_DO)
#define	set_my_state_will(tn, opt)	((tn)->tn_options[opt] |= MY_STATE_WILL)
#define	set_my_want_state_do(tn, opt)	((tn)->tn_options[opt] |= MY_WANT_STATE_DO)
#define	set_my_want_state_will(tn, opt)	((tn)->tn_options[opt] |= MY_WANT_STATE_WILL)

#define	set_my_state_dont(tn, opt)	((tn)->tn_options[opt] &= ~MY_STATE_DO)
#define	set_my_state_wont(tn, opt)	((tn)->tn_options[opt] &= ~MY_STATE_WILL)
#define	set_my_want_state_dont(tn, opt)	((tn)->tn_options[opt] &= ~MY_WANT_STATE_DO)
#define	set_my_want_state_wont(tn, opt)	((tn)->tn_options[opt] &= ~MY_WANT_STATE_WILL)

/*
 * Tricky code here.  What we want to know is if the MY_STATE_WILL
 * and MY_WANT_STATE_WILL bits have the same value.  Since the two
 * bits are adjacent, a little arithmatic will show that by adding
 * in the lower bit, the upper bit will be set if the two bits were
 * different, and clear if they were the same.
 */
#define my_will_wont_is_changing(tn, opt) \
			(((tn)->tn_options[opt]+MY_STATE_WILL) & MY_WANT_STATE_WILL)

#define my_do_dont_is_changing(tn, opt) \
			(((tn)->tn_options[opt]+MY_STATE_DO) & MY_WANT_STATE_DO)

/*
 * Make everything symetrical
 */

#define	HIS_STATE_WILL			MY_STATE_DO
#define	HIS_WANT_STATE_WILL		MY_WANT_STATE_DO
#define HIS_STATE_DO			MY_STATE_WILL
#define HIS_WANT_STATE_DO		MY_WANT_STATE_WILL

#define	his_state_is_do			my_state_is_will
#define	his_state_is_will		my_state_is_do
#define his_want_state_is_do		my_want_state_is_will
#define his_want_state_is_will		my_want_state_is_do

#define	his_state_is_dont		my_state_is_wont
#define	his_state_is_wont		my_state_is_dont
#define his_want_state_is_dont		my_want_state_is_wont
#define his_want_state_is_wont		my_want_state_is_dont

#define	set_his_state_do		set_my_state_will
#define	set_his_state_will		set_my_state_do
#define	set_his_want_state_do		set_my_want_state_will
#define	set_his_want_state_will		set_my_want_state_do

#define	set_his_state_dont		set_my_state_wont
#define	set_his_state_wont		set_my_state_dont
#define	set_his_want_state_dont		set_my_want_state_wont
#define	set_his_want_state_wont		set_my_want_state_dont

#define his_will_wont_is_changing	my_do_dont_is_changing
#define his_do_dont_is_changing		my_will_wont_is_changing


#define	TN_DO(tn, o)	tn_send(tn, DO, o)
#define	TN_DONT(tn, o)	tn_send(tn, DONT, o)
#define	TN_WILL(tn, o)	tn_send(tn, WILL, o)
#define	TN_WONT(tn, o)	tn_send(tn, WONT, o)

#define	SB_CLEAR(tn)	(tn)->tn_soc = (tn)->tn_soBuffer
#define	SB_TERM(tn)	(tn)->tn_soe = (tn)->tn_soc
#define	SB_ACCUM(tn, c)	\
    if ((tn)->tn_soc < ((tn)->tn_soBuffer + sizeof((tn)->tn_soBuffer))) {\
	*((tn)->tn_soc++) = (c);\
    }
#define	SB_GET(tn)	(*((tn)->tn_soc++) & 0xff)
#define	SB_EOF(tn)	((tn)->tn_soc >= (tn)->tn_soe)
#define	SB_LEN(tn)	((tn)->tn_soe - (tn)->tn_soc)

/*
 * State for recv fsm
 */
#define	TS_DATA		0	/* base state */
#define	TS_IAC		1	/* look for double IAC's */
#define	TS_CR		2	/* CR-LF ->'s CR */
#define	TS_SB		3	/* throw away begin's... */
#define	TS_SE		4	/* ...end's (suboption negotiation) */
#define	TS_WILL		5	/* will option negotiation */
#define	TS_WONT		6	/* wont " "*/
#define	TS_DO		7	/* do " " */
#define	TS_DONT		8	/* dont " "*/

int	tndevflag = D_MP;

/*
 * Variable: tn_devices
 * Purpose: An array of all devices.
 */
static	tn_t	*tn_devices	= NULL;
/*
 * Variable: tn_deviceFree
 * Purpose: Linked list of device structures that are NOT currently in use. 
 */
static	tn_t	*tn_devicesFree= NULL;

/*
 * Variable: tn_openCount
 * Purpose: Describes the number of open telnet connections (including 
 *	    device 0).
 */
static	volatile int	tn_openCount;

#if	_TDEBUG
static	void
/*VARARGS2*/
tn_debug(char *s, ...)
/*
 * Function: tn_debug
 * Purpose: To print out a debug message on the console
 * Parameters:	s - format string
 * Returns:	nothing
 */
{
    va_list	ap;

    va_start(ap, s);
    icmn_err(CE_CONT, s, ap);
    va_end(ap);	
}

#endif

static	void
tninit(void)
/*
 * Function: tn_init
 * Purpose:  To initialize the streams telnet devices.
 * Parameters: none
 * Returns:    nothing
 * 
 * Locks held on entry:	none
 * Locks held on exit:	none
 * Locks aquired:	none
 */
{
    tn_t	*tn;

    TDEBUG(("tn_init: Initializing tsd: %d devices supported\n", tn_maxunit));

    tn_devices = kmem_alloc(tn_maxunit * sizeof(tn_t), KM_NOSLEEP);
    if (NULL == tn_devices) {
	TDEBUG(("tn_init: kmem_alloc failed\n"));
	return;
    }

    bzero(tn_devices, sizeof(tn_t) * tn_maxunit);

    for (tn = tn_devices; tn < &tn_devices[tn_maxunit]; tn++) {
	tn->tn_open   = FALSE;
	tn->tn_next   = tn + 1;		/* link to next */
	init_spinlock(&tn->tn_lock, "tsd", tn - tn_devices);
    }

    /* Do not put 0 on free list */

    tn_devices[0].tn_next = NULL;	/* just for saftey */
    tn_devicesFree = &tn_devices[1];	/* set free list */
    (tn - 1)->tn_next = NULL;		/* zap last ones pointer */
}

/* ARGSUSED */
static int
tn_open(register queue_t *rq, dev_t *d,int flag, int sflag, struct cred *cred)
/*
 * Function: tn_open
 * Purpose: To open a telnet streams device 
 * Parameters:	rq - pointer to read queue
 *		d - pointer to device (major/minor)
 *		flag - generic flags stuff
 *		sflag - streams flag
 *		cred - credentials (not used)
 * Returns:
 * 
 * Locks held on entry: none
 * Locks held on exit:	none
 * Locks aquired:	
 */
{
    register	tn_t	*tn, *t;
    int		pl;			/* lock priority level */
    int		error;

    TDEBUG(("tn_open: major(%d) minor(%d)\n", 
	    getemajor(*d), geteminor(*d)));

    if (NULL == tn_devices) {
	tninit();
	if (NULL == tn_devices) {
	    TDEBUG(("tn_open: tn_devices not allocated\n"));
	    return(ENXIO);
	}
    } 
    /*
     * If we do not care what device we are using, then pick one.
     */

    error = 0;

    if (CLONEOPEN == sflag) {
	tn = tn_devicesFree;
	if (NULL == tn) {
	    TDEBUG(("tn_open: no more free devices\n"));
	    error = ENXIO;
	} else {
	    tn_devicesFree = tn->tn_next;
	    pl = TNLOCK(tn);
	    tn->tn_next = NULL;
	    tn->tn_flags = TN_CLEAR;
	    TDEBUG(("tn_open: allocated device major(%d) minor(%d)\n",
		    getemajor(*d), TN_MINOR(tn)));
	    *d = makedevice(getemajor(*d), TN_MINOR(tn));
	}
    } else {
	tn = TN(getemajor(*d), geteminor(*d));
	if (NULL == tn) {
	    TDEBUG(("tn_open: invalid major/minor pair major(%d) minor(%d)\n",
		   getemajor(*d), geteminor(*d)));
	    error = ENODEV;
	} else {
	    /* If not free, and Exclusive set, then fail */
	    pl = TNLOCK(tn);
	    if (tn->tn_open) {
		if (tn->tn_flags & TN_EXCLUSIVE) {
		    error = EBUSY;
		}
	    } else if (0 != TN_MINOR(tn)) {
		/*
		 * Unlink from the tn_devicesFree list if not device
		 * 0.  We know that there is at least 1 device on the
		 * list, and that the node we are looking for is
		 * there, so no need to special case empty list or not
		 * on list.
		 */
		if (tn_devicesFree == tn) {
		    tn_devicesFree = tn->tn_next;
		} else {
		    for (t = tn_devicesFree; t->tn_next != tn; t = t->tn_next)
			;
		    t->tn_next = tn->tn_next;
		}
		tn->tn_next = NULL;	/* For safety  */
	    }
	}
    }

    if (0 == error) {
	/*
	 * Now initialize all of the telnet information.
	 */
	if (!tn->tn_open) {
	    TDEBUG(("tn_open: first open of minor device %d\n", 
		    TN_MINOR(tn)));
	    tn_openCount++;
	    strctltty(rq);		/* make us controlling tty */
	    tn->tn_open    = TRUE;
	    rq->q_ptr      = (caddr_t)tn;
	    WR(rq)->q_ptr  = (caddr_t)tn;
	    tn->tn_rq      = rq;
	    tn->tn_wq      = WR(rq);
	    tn->tn_data    = NULL;
	    tn->tn_control = NULL;
	    tn->tn_vsocket = NULL;
	    tn->tn_file    = NULL;
	    tn->tn_flags   = TN_CLEAR;
	    tn->tn_soc     = tn->tn_soBuffer;
	    tn->tn_soe     = tn->tn_soBuffer;
	    qprocson(rq);
	} else {
	    ASSERT(NULL != rq->q_ptr);	
	    ASSERT(NULL != WR(rq)->q_ptr);
	}
    } else if (NULL == tn) {		/* no unlock required */
	return(error);
    }

    TDEBUG(("tn_open: open(%d) error(%d)\n", TN_MINOR(tn), error));
    TNUNLOCK(tn, pl);
    return(error);

}

/* ARGSUSED */
static	int
tn_close(queue_t *rq, int flag, struct cred *cred)
/*
 * Function: tn_close
 * Purpose: To close a streams telnet device.
 * Parameters: rq - pointer to thje read queue.
 * Returns:    0 - success
 *	      !0 - socket not attached
 * 
 * Locks held on entry:	none
 * Locks held on exit:	none
 * Locks aquired:	tn_lock for specified device.
 */
{
    tn_t	*tn;
    int		pl;
#if _TDEBUG
    int		error;
#endif
    mblk_t	*cb, *nb;		/* current block/next block */
    minor_t	m;

    m = TN_MINOR((tn_t *)rq->q_ptr);
    TDEBUG(("tn_close: minor(%d)\n", m));
    ASSERT(NULL != tn_devices);

    tn = (tn_t *)rq->q_ptr;
    ASSERT(NULL != tn);
    ASSERT(tn->tn_open);
    pl = TNLOCK(tn);

    str_unbcall(rq);			/* stop waiting for buffers */
    qprocsoff(rq);			/* turn off processing */

    /*
     * If socket has been attached, release it and remove our reference
     * to it.
     */
    if (NULL != tn->tn_vsocket) {
      struct socket *so =
	  (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(tn->tn_vsocket)));
	    
	ASSERT(NULL != tn->tn_file);

	SOCKET_LOCK(so);
	so->so_state &= ~SS_WANT_CALL;
	SOCKET_UNLOCK(so);
#if _TDEBUG
	error = vfile_close(tn->tn_file);
#else
	(void) vfile_close(tn->tn_file);
#endif
	TDEBUG(("tn_close: fclose: error(%d)\n", error));
	tn->tn_vsocket = NULL;		/* XXX */
    }

    /* If we are waiting on buffers, cancel the request */ 

    if (0 != tn->tn_rqBufCallId) {
	unbufcall(tn->tn_rqBufCallId);
	tn->tn_rqBufCallId = 0;
    }
    if (0 != tn->tn_wqBufCallId) {
	unbufcall(tn->tn_wqBufCallId);
	tn->tn_wqBufCallId = 0;
    }
    /* Free any blocks on data/control queues */

    for (cb = tn->tn_control; NULL != cb; cb = nb) {
	nb = cb->b_next;
	freemsg(cb);
    }
    tn->tn_control = NULL;

    for (cb = tn->tn_data; NULL != cb; cb = nb) {
	nb = cb->b_next;
	freemsg(cb);
    }
    tn->tn_data = NULL;

    tn->tn_open = FALSE;
    if ((1 == tn_openCount--) && (tn_devices[0].tn_open)) {
	/*
	 * No more opens except device 0, send EBADF.
	 */
	(void)putctl1(tn_devices[0].tn_rq->q_next, M_ERROR, EBADF);
    }

    if (0 != m) {			/* magic device */
		
	tn->tn_next = tn_devicesFree;
	tn_devicesFree = tn;
	/*
	 * This device closed, send minor number upstream on device
	 * 0 if open. If we can not allocate memory, just give up.
	 */

	if (tn_devices[0].tn_open) {	/* OK, send up date */
	    TDEBUG(("tn_close: minor %d closed, reporting to minor 0\n", m));
	    cb = allocb(sizeof(minor_t), BPRI_HI);
	    if (NULL != cb) {
		cb->b_datap->db_type = M_DATA;
		*(minor_t *)cb->b_wptr = m;
		cb->b_wptr += sizeof(minor_t);
		MP_STREAMS_INTERRUPT(tn_devices[0].tn_rq->q_monpp,
				putnext,
				(void *)tn_devices[0].tn_rq, 
				(void *)cb,
				0);
	    }
	}
    }
    TNUNLOCK(tn, pl);
    
    return(0);
}

static	int	
tn_rsrv(queue_t *rq)
/*
 * Function: tn_rsrv (Telnet read service) 
 * Purpose:  
 * Parameters:	rq - pointer to read to to process
 * Returns:	0
 * 
 * Locks held on entry:	none
 * Locks held on exit:	none
 * Locks aquired:	tn_lock for specified queue
 */

{
    tn_t	*tn;			/* This telnet connection */
    mblk_t	*iBlock, *oBlock;	/* incomming/outgoing block */
    int		pl;			/* interrupt level */

    tn = TNQ(rq);
    TDEBUG(("tn_rsrv: called minor(%d)\n", TN_MINOR(tn)));


    pl = TNLOCK(tn);
    while(canput(tn->tn_rq->q_next)) {
	iBlock = getq(rq); 
	if (NULL == iBlock) {
	    /*
	     * If no incomming block pending, we attempt to read from the
	     * socket. This is the ONLY place we receive the data. When
	     * data is pending on the socket for read, the socket support
	     * calls soreceive.
	     */
	    TDEBUG(("tn_rsrv: Attempting to receive socket data\n"));
	    iBlock = tn_socketRead(tn);
	}
	if (NULL == iBlock) {
	    /* Nothing in the queue or on the socket */
	    TDEBUG(("tn_rsrv: No socket data pending\n"));
	    break;
	}
	TDEBUG(("tn_rsrv: Recieved socket data\n"));

	if (iBlock->b_datap->db_type == M_DATA) {
	    /*
	     * At this point, we have some data, and we know we can send it
	     * up stream - so send it on up.
	     */
	    oBlock = tn_telnetReceive(tn, iBlock);
	    if (iBlock->b_rptr >= iBlock->b_wptr) {
		freemsg(iBlock);
		iBlock = NULL;
	    } else {
		putbq(tn->tn_rq, iBlock);
	    }
	    if (NULL == oBlock) {
		/* 
		 * Nothing to forward up the chain, so we continue.
		 */
		continue;
	    }
	} else {
	    /*
	     * We ran into some sort of problem, so we process the 
	     * list and get outa here.
	     */ 
	    oBlock = iBlock;
	    break;
	}

	/*
	 * At this point, if all of the incomming data has been used up, then
	 * the block has been freed. If there was some remaining, it has been
	 * re-queued, and we may possibly have a message to send up.
	 */

	if (NULL != oBlock) {
	    putnext(tn->tn_rq, oBlock);
	}
    }
    TNUNLOCK(tn, pl);
    return(0);
}

static	mblk_t *
tn_ioctlPut(tn_t *tn, mblk_t *b)
/*
 * Function: tn_ioctlPut
 * Purpose: To process an IOCTL requrest from upstream during a PUT.
 * Parameters:	tn - pointer to the device the command came down stream on.
 *		b  - pointer to mblk containing message.
 * Returns:	Reply mblk, or NULL if none.
 * 
 * Locks held on entry:	none
 * Locks held on exit:	none
 * Locks aquired:	tn->tn_lock
 */
{
    struct iocblk	*ioc;
    int			pl;

    ioc = (struct iocblk *)b->b_rptr;

    TDEBUG(("tn_ioctlPut: minor(%d) cmd(%x)\n", TN_MINOR(tn), ioc->ioc_cmd));
    pl = TNLOCK(tn);

    switch(ioc->ioc_cmd) {
    case FIOASYNC:
	if ((*(int*)(b->b_cont->b_rptr))) {
	    tn->tn_flags |= TN_SIGIO;
	} else {
	    tn->tn_flags &= ~TN_SIGIO;
	}
	b->b_datap->db_type = M_IOCACK;
	ioc->ioc_count = 0;
	break;
    case TCSSOCKET:			/* set socket */
	if (sizeof(struct tcssocket_info) != ioc->ioc_count) {
	    ioc->ioc_error = EINVAL;
	    b->b_datap->db_type = M_IOCNAK;
	} else {
	    ioc->ioc_error = 
		tn_connectSocket(tn, 
				 (struct tcssocket_info *)b->b_cont->b_rptr);
	    if (0 != ioc->ioc_error) {
		b->b_datap->db_type = M_IOCNAK;
	    } else {
		b->b_datap->db_type = M_IOCACK;
	    }
	}
	ioc->ioc_count = 0;
	break;
    case TIOCSTART:			/* in case queue is plugged */
	b->b_datap->db_type = M_IOCACK;
	tn->tn_flags &= ~TN_TXSTOP;
	qenable(tn->tn_wq);
	ioc->ioc_count = 0;
	break;
    default: 				/* Deal with it later */
	putq(tn->tn_wq, b);
	b = NULL;
    }
    TNUNLOCK(tn, pl);
    TDEBUG(("tn_ioctlPut: minor(%d) Compete\n", TN_MINOR(tn)));
    return(b);
}

static	int
tn_wput(queue_t *wq, mblk_t *b)
/*
 * Function: tn_wput
 * Purpose:	To queue a packet in a downstream direction.
 * Parameters:	wq - pointer to write queue
 *		b  - pointer to mblk
 * Returns:	0
 * 
 * Locks held on entry:	none
 * Locks held on exit:	none
 * Locks aquired:	tn->tn_lock
 */
{
    tn_t	*tn;

    tn = TNQ(wq);			/* find our queue */
    ASSERT((tn >= tn_devices) && (tn < &tn_devices[tn_maxunit]));

    TDEBUG(("tn_wput: minor(%d) db_type(%d) length(%d)\n", TN_MINOR(tn), 
	    b->b_datap->db_type, b->b_wptr - b->b_rptr));
    
    switch(b->b_datap->db_type) {
    case M_FLUSH:			/* flushing eh? */
	sdrv_flush(wq, b);
	b = NULL;			/* no reply */
	break;
    case M_DATA:
	putq(wq, b);			/* on the q it goes - handle later */
	b = NULL;
	break;
    case M_DELAY:			/* ignore for now */
	freemsg(b);
	b = NULL;
	break;
    case M_IOCTL:
	b = tn_ioctlPut(tn, b);
	break;
    default:
	freemsg(b);
	b = NULL;
	TDEBUG(("tn_wput: Invalid db_type(%d)\n", b->b_datap->db_type));
	sdrv_error(wq, b);
    }

    if (NULL != b) {
	qreply(wq, b);
    }
    return(0);
}

static	int
tn_sendToRemote(tn_t *tn, const char *data, int length, int control)
/*
 * Function: tn_sendToRemote
 * Purpose: To build and send messages down stream to the remote sned
 *	    of the telnet connection.
 * Parameters:	tn - pointer to telnet conenction
 *		data - pointer to data
 *		length - length of data
 *		control - if TRUE, it is a control command that should
 *			NOTE be translated.
 * Returns: TRUE - successfully sent message (or at least queued 
 *		   it/buffered it)
 *	   FALSE - failed
 *
 * Notes: This routine will call bufcall to re-process the upstream queue, 
 *	  if the message can not be sent.
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    mblk_t *b, *tb;

    TDEBUG(("tn_sendToRemote: minor(%d) bytes(%d)\n", TN_MINOR(tn), length));

    b = allocb(length, BPRI_MED);
    if (NULL == b) {
	if (0 != tn->tn_rqBufCallId) {
	    unbufcall(tn->tn_rqBufCallId);
	}
	tn->tn_rqBufCallId = bufcall(1, BPRI_MED, qenable, (long)tn->tn_rq);
	TDEBUG(("tn_sendToRemote: failed to allocate buffer minor(%d\n", 
		TN_MINOR(tn)));
	return(FALSE);
    }
    b->b_datap->db_type = M_DATA;
    bcopy(data, b->b_wptr, length);
    b->b_wptr += length;
    if (control) {			/* put on control queue */
	if (NULL == tn->tn_control) {	/* first on list */
	    tn->tn_control = b;
	    b->b_next = NULL;
	} else {
	    for (tb = tn->tn_control; NULL != tb->b_next; tb = tb->b_next)
		 ;
	    tb->b_next = b;
	    b->b_next = NULL;
	}
	qenable(tn->tn_wq);		/* enable queue */
    } else {	
	putq(tn->tn_wq, b);
	if (!(tn->tn_flags & TN_TXSTOP)) {
	    qenable(tn->tn_wq);
	}
    }
    TDEBUG(("tn_sendToRemote: minor(%d) bytes(%d) Queued\n", 
	    TN_MINOR(tn), length));
    return(TRUE);
}

static	int	
tn_send(tn_t *tn, int n, int option)
/*
 * Function: tn_send
 * Purpose:  To send an option code to the other end of the telnet connection.
 * Parameters:	tn - pointer to telnet conenction
 *		n - DO/DONT/WILL/WONT
 *		option - option value
 * Returns: same as tn_sendToRemote
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    char optionString[3];

    optionString[0] = IAC;
    optionString[1] = n;
    optionString[2] = option;

    TDEBUG(("tn_send: <IAC, %s, %s>\n", TELCMD(n), TELOPT(option)));

    return(tn_sendToRemote(tn, optionString, sizeof(optionString), TRUE));
}

static	int
tn_socketSend(tn_t *tn, mblk_t *b, int flags)
/*
 * Function: tn_socketSend
 * Purpose: To send data on a socket
 * Parameters:	tn - pointer to telnet connection
 *		b - pointer to block to send.
 *		flags - flags for sosend, such as OOB. 
 * Returns:	error = from socket routines.
 * 
 * Locks held on entry:	tn_lock
 * Locks held on exit:	tn_lock
 * Locks aquired:	none
 */
{
    iovec_t	iov;
    uio_t	uio;
    int		error;			/* errno */


    TDEBUG(("tn_socketSend: minor(%d)\n", TN_MINOR(tn)));
    TNCHECKLOCK(tn);
    ASSERT(tn->tn_vsocket);

    /* Build uio and iovec parameters */

    uio.uio_iov	   = &iov;
    uio.uio_sigpipe = 0;
    uio.uio_offset = 0;
    uio.uio_iovcnt = 1;
    uio.uio_segflg = UIO_SYSSPACE;

    if (b->b_wptr > b->b_rptr) {
	/*
	 * Only send if there is data.
	 */
	iov.iov_base = (caddr_t)b->b_rptr;
	iov.iov_len  = b->b_wptr - b->b_rptr;
	uio.uio_resid  = iov.iov_len;
	    
	TDEBUG(("tn_socketSend: %d bytes @ 0x%x\n", 
		iov.iov_len, iov.iov_base));
	
	VSOP_SEND(tn->tn_vsocket, NULL, &uio, flags, NULL, error);
	ASSERT(uio.uio_sigpipe == 0);

	TDEBUG(("tn_socketSend: errno(%d) resid(%d) bytes\n", 
		error, uio.uio_resid));

	b->b_rptr = b->b_wptr - uio.uio_resid;
	return(error);
    }
    return(0);
}

static	void
tn_setTerm(tn_t *tn, struct termio *tio)
/*
 * Function: tn_setTerm
 * Purpose: TO process a set term IOCTL request
 * Parameters: 	tn - pointer to the telnet connection
 *		tio - pointer to the term io structure
 * Returns: nothing
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    if (NULL != tn->tn_vsocket) {	/* no socket yet - initial settings */
	if ((tio->c_iflag & IXON) && 
	    ((tio->c_cc[VSTART] != CDEL) || tio->c_cc[VSTOP] != CDEL)) {
	    tn->tn_flags |= TN_FLOW_CNTL;
	} else {
	    tn->tn_flags &= ~TN_FLOW_CNTL;
	    if (tn->tn_flags & TN_TXSTOP) { /* if stopped - continue */
		tn->tn_flags &= ~TN_TXSTOP;
		qenable(tn->tn_wq);
	    }
	}
    }
    tn->tn_termio = *tio;
}

static	mblk_t	*
tn_ioctlSrv(tn_t *tn, mblk_t *b)
/*
 * Function: tn_ioctlSrv
 * Purpose: IOCTL functions processed during the write service routine
 * Parameters:	tn - pointer to telnet connection
 *		b - pointer to ioctl request block
 * Returns: NULL if no reply, !NULL if reply block.
 * Notes: If the block passed to the routine must be freed, it is up to 
 *	this routine to free it. THis routine owns blocks that are passed 
 *	to it, and gices up ownership on blocks it returns.
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
#   define STERMIO(b)	((struct termio *)((b)->b_cont->b_rptr))
#   define SWINSIZE(b)	((struct winsize *)((b)->b_cont->b_rptr))
    struct iocblk	*ioc;

    ioc = (struct iocblk *)b->b_rptr;

    TDEBUG(("tn_ioctlSrv: minor(%d) cmd(%x) cnt(%d)\n", 
	    TN_MINOR(tn), ioc->ioc_cmd, ioc->ioc_count));

    b->b_datap->db_type = M_IOCACK;

    switch (ioc->ioc_cmd) {
    case TCSETAF:
	(void)putctl1(tn->tn_rq->q_next, M_FLUSH, FLUSHR);
    case TCSETA: 
    case TCSETAW:
    {
	if (ioc->ioc_count != sizeof(struct termio)) {
	    ioc->ioc_error = EINVAL;
	    b->b_datap->db_type = M_IOCNAK;
	    break;
	}

	tn_setTerm(tn, STERMIO(b));
	ioc->ioc_count = 0;
	break;
    }

    case TCSBRK:			/* let output empty */
	ioc->ioc_count = 0;
	break;

    case TCGETA:
	if (sizeof(struct termio) != ioc->ioc_count) {
	    ioc->ioc_error = EINVAL;
	    b->b_datap->db_type = M_IOCNAK;
	} else {
	    *STERMIO(b) = tn->tn_termio;
	}
	break;

    case TCXONC:
	if (sizeof(int) != ioc->ioc_count) {
	    ioc->ioc_error = M_IOCNAK;
	    b->b_datap->db_type = M_IOCNAK;
	    break;
	}
	switch(*(int *)b->b_cont->b_rptr) {
	case 0:				/* stop output */
	    tn->tn_flags |= TN_TXSTOP;
	    break;
	case 1:				/* resume output */
	    tn->tn_flags &= ~TN_TXSTOP;
	    qenable(tn->tn_wq);
	    break;
	case 2:				/* stop input */
	    if (!tn_sendToRemote(tn, (char *)&tn->tn_termio.c_cc[VSTOP],
				 sizeof(&tn->tn_termio.c_cc[VSTOP]), 
				 FALSE)) {
		putbq(tn->tn_wq, b);
		return(NULL);
	    }
	    break;
	case 3:				/* resume input */
	    if (!tn_sendToRemote(tn, (char *)&tn->tn_termio.c_cc[VSTART],
				 sizeof(&tn->tn_termio.c_cc[VSTART]), 
				 FALSE)) {
		putbq(tn->tn_wq, b);
		return(NULL);
	    }
	    break;
	default:
	    b->b_datap->db_type = M_IOCNAK;
	    ioc->ioc_error = EINVAL;
	    break;
	}
	ioc->ioc_count = 0;
	break;

    case TIOCSTI:			/* Simulate terminal input */
    {
	mblk_t	*ob;

	if (!canput(tn->tn_rq->q_next)) {
	    putbq(tn->tn_wq, b);
	    return(NULL);
	}
	ob = allocb(1, BPRI_LO);
	if (NULL == ob) {
	    putbq(tn->tn_wq, b);
	    return(NULL);
	}
	*ob->b_wptr++ = *b->b_cont->b_rptr;
	putq(tn->tn_rq, ob);
	ioc->ioc_count = 0;
	break;
    }
	
    case TIOCOUTQ:			/* queue size */
	if (sizeof(int) != ioc->ioc_count) {
	    ioc->ioc_error = EINVAL;
	    b->b_datap->db_type = M_IOCNAK;
	} else {
	    *((int *)b->b_cont->b_rptr) = qsize(tn->tn_wq);
	}
	break;

    case TIOCGWINSZ:			/* get window size */
	if (sizeof(struct winsize) != ioc->ioc_count) {
	    ioc->ioc_error = EINVAL;
	    b->b_datap->db_type = M_IOCNAK;
	} else {
	    *SWINSIZE(b) = tn->tn_ws;
	}
	break;

    case TIOCSWINSZ:			/* set window size */
	if (sizeof(struct winsize) != ioc->ioc_count) {
	    ioc->ioc_error = EINVAL;
	    b->b_datap->db_type = M_IOCNAK;
	} else {
	    tn->tn_ws = *SWINSIZE(b);
	    (void)putctl1(tn->tn_rq->q_next, M_PCSIG, SIGWINCH);
	}
	break;

    case TIOCEXCL:			/* exclusive open */
	tn->tn_flags |= TN_EXCLUSIVE;
	break;
	
    case TIOCNXCL:			/* !exclusive open */
	tn->tn_flags &= ~TN_EXCLUSIVE;
	break;

    case TIOCSTOP:			/* stop .... */
	tn->tn_flags |= TN_TXSTOP;
	break;

    case TIOCSTART:			/* start ... */
	cmn_err(CE_PANIC, "tn_ioctlSrv: TIOCSTART unexpected");
	break;

    case TIOCSPGRP:
	if (sizeof(int) != ioc->ioc_count) {
	    ioc->ioc_error = EINVAL;
	    b->b_datap->db_type = M_IOCNAK;
	} else {
	    tn->tn_group = *(int *)b->b_cont->b_rptr;
	}
	break;
	
    case TIOCGPGRP:
	if (sizeof(int) != ioc->ioc_count) {
	    ioc->ioc_error = EINVAL;
	    b->b_datap->db_type = M_IOCNAK;
	} else {
	    *(int *)b->b_cont->b_rptr = tn->tn_group;
	}
	break;

    default:
	ioc->ioc_error = EINVAL;
	ioc->ioc_count = 0;
	break;
    }

    TDEBUG(("tn_ioctlSrv: reply %s errno(%d)\n", 
	    b->b_datap->db_type == M_IOCACK ? "M_IOCACK" : "M_IOCNAK",
	    ioc->ioc_error));
    return(b);
}

/* ARGSUSED */
static	int
tn_sendIoctlUpStream(tn_t *tn, int cmd, void *d, int l)
/*
 * Function: tn_sendIoctlUpStream
 * Purpose: To send an ioctl up steam.
 * Parameters:	tn - pointer to the telnet data structure
 *		cmd- ioctl command to send up stream.
 *		d - pointer to the data to send upstream
 *		l - length of data (must be > 0)
 * Returns:	TRUE - success
 *		FALSE - failed
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    mblk_t	*bioc, *bioctl;

    TDEBUG(("tn_sendIoctlUpStream: minor(%d) cmd(%d)\n", TN_MINOR(tn), cmd));

    ASSERT(0 < l);

    bioc = allocb(sizeof(struct iocblk), BPRI_HI);
    if (NULL == bioc) {
	return(FALSE);
    }

    bioctl = allocb(l, BPRI_HI);
    if (NULL == bioctl) {
	freeb(bioc);
	return(FALSE);
    }

    bioc->b_wptr += sizeof(struct iocblk);
    bioc->b_datap->db_type = M_IOCACK;
    bioc->b_cont = bioctl;

    bcopy(d, bioctl->b_wptr, l);
    bioctl->b_wptr += l;

    putnext(tn->tn_rq, bioc);
    return(TRUE);
}

static	mblk_t 	*
tn_sendQueue(tn_t *tn, mblk_t *b)
/*
 * Function: tn_sendQueue
 * Purpose: To attempt to send all messages in a queue. Those that are
 *		sent are freed.
 * Parameters:	tn - pointer to telnet conenction to send
 *		b  - pointer to queue to try to send
 * Returns:	Pointer to new queue (which may be NULL is all are sent).
 * Notes: 	If a message fails to be sent, we attempt to send an 
 *		error up stream. If it fails to go upstream, then the 
 *		message is left on the queue, 
 * 
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    mblk_t	*tb;
    int		error;

    while (NULL != b) {
	TDEBUG(("tn_sendQueue: minor(%d) type(%d)\n", TN_MINOR(tn), 
		b->b_datap->db_type));
	error = tn_socketSend(tn, b, 0);

	switch(error) {
	case 0:				/* sent some data */
	    if (b->b_rptr >= b->b_wptr) {
		tb = b->b_next;
		freemsg(b);
		b = tb;
	    }
	    break;
	case EWOULDBLOCK:
	case EAGAIN:
	    return(b);
	case ECONNABORTED:		/* socket toasted */
	case ENOTCONN:
	case EPIPE:
	default:
	    (void)putctl1(tn->tn_rq->q_next, M_ERROR, error);
	    return(b);
	}
    }
    return(b);
}

static	int
tn_wsrv(queue_t *wq)
/*
 * Function: tn_wsrv (Telnet write service routine)
 * Purpose: To service the downstream direction of a telnet stream.
 * Parameters:	wq - pointer to the write queue
 * Returns: 0
 * Note: All messages on the control Q are sent out as is, messages passed
 *	down from above are scanned for IAC, since they must be escaped
 *	with another.
 * 
 * Locks held on entry:	none
 * Locks held on exit:	none
 * Locks aquired:	tn->tn_lock
 */
{
    tn_t	*tn;
    int		pl, done;
    mblk_t	*b, *db;		/* cur/temp/data block */

    TDEBUG(("tn_wsrv: Called\n"));

    tn = TNQ(wq);			/* pick up telnet entry */
    pl = TNLOCK(tn);
    ASSERT(tn->tn_open);		/* must be open */
    
    /*
     * First business, try to finish sending any blocks that were started. 
     * First, complete all data blocks, then all control blocks. We do 
     * NOT do any translation on them.
     */
    if (NULL != tn->tn_data) {
	if (NULL != (tn->tn_data = tn_sendQueue(tn, tn->tn_data))) {
	    TNUNLOCK(tn, pl);
	    return(0);
	}
    }
    if (NULL != tn->tn_control) {
	if (NULL != (tn->tn_control = tn_sendQueue(tn, tn->tn_control))) {
	    TNUNLOCK(tn, pl);
	    return(0);
	}
    }

    /*
     * Ok, nothing in progress, so lets look at what we can do. Note that
     * the remainder of this code ASSUMES that tn_data is NULL!
     */

    done = FALSE;
    while (!done) {
	b = getq(wq);
	if (NULL == b) {
	    break;
	}
	TDEBUG(("tn_wsrv: minor(%d) type(%d)\n", TN_MINOR(tn), 
		b->b_datap->db_type));
	switch(b->b_datap->db_type) {	/* what to do ... */
	case M_IOCTL:			/* special message */
	    b = tn_ioctlSrv(tn, b);
	    if (NULL == b) {
		done = TRUE;
	    } else {
		qreply(wq, b);
	    }
	    break;
	case M_DATA:			/* normal data message */
	    if (NULL == tn->tn_vsocket) {
		/*
		 * If no socket associated with the stream yet, pass up an 
		 * error.
		 */
		freemsg(b);		/* release block */
		(void)putctl1(tn->tn_rq->q_next, M_IOCNAK, EBADF); 
		continue;
	    }  else if (tn->tn_flags & TN_TXSTOP) {
		/*
		 * Hmmm - transmit disabled write now, I guess we just 
		 * leave it hanging around on the Q.
		 */
		putbq(wq, b);
		done = TRUE;
	    } else {
		mblk_t	*tb;		/* temp output block */
		register char *cp;
		/*
		 * Normally we will be sending normal ASCII data, however, if
		 * the silly goober puts a IAC in the string, then we must 
		 * duplicate it. To handle this, for each IAC we see in the 
		 * stream of bytes, we break the block, and start copying 
		 * into a new one.  If we get a block full of IAC's, well, 
		 * we have a bad performance problem. But hey, this is 
		 * for terminals - so IACs should be RARE.
		 *
		 * %%% technically, we could deadlock here since we will not
		 * free up a buffer until we get another........ bummer.
		 */
		for (cp = (char *)b->b_rptr, db = NULL; cp < (char *)b->b_wptr; cp++) {
		    if (IAC == *cp) {	/* break blocks */
			tb = allocb(cp - (char *)b->b_rptr + 2, BPRI_LO);
			if (NULL == tb) {
			    if (0 == tn->tn_wqBufCallId) {
				tn->tn_wqBufCallId = bufcall(cp - (char *)b->b_wptr+1,
							     BPRI_LO,
							     qenable, 
							     (long)tn->tn_wq);
			    }
			    putbq(wq, b);
			    TNUNLOCK(tn, pl);
			    return(0);
			} else {
			    bcopy(b->b_rptr, tb->b_rptr, cp - (char *)b->b_rptr + 1);
			    tb->b_wptr += cp - (char *)b->b_rptr + 1;
			    *tb->b_wptr++ = IAC;
			    pl = TNLOCK(tn);
			    if (NULL == db) { /* first on list */
				tn->tn_data = db = tb;
			    } else {
				db->b_next = tb;
				db = tb;
			    }
			    tb->b_next = NULL;
			}
		    }
		}

		/*
		 * If data block we started out with is not empty, 
		 * we add it to the end of the data queue.
		 */
		if (b->b_rptr >= b->b_wptr) {
		    freemsg(b);
		} else if (NULL == db) {
		    tn->tn_data = b;
		} else {
		    db->b_next = b;
		}
		b->b_next = NULL;
		tn->tn_data = tn_sendQueue(tn, tn->tn_data);
		done = (NULL != tn->tn_data); /* could not all go */
	    }
	    break;
	default:
	    cmn_err(CE_PANIC, "tn_wsrv: invalid buffer type");
	    break;
	}
    }
    TNUNLOCK(tn, pl);
    return(0);
}

static	mblk_t *
tn_socketRead(tn_t *tn)
/*
 * Function: tn_socketRead
 * Purpose: To receive a message off of a socket.
 * Parameters:	tn - pointer to telnet structure
 * Returns:	pointer to mblk with message or null if failed.
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none	
 *
 * Notes: This routine will call bufcall if it can not allocate sufficient 
 *	  memory for the message.
 */
{
#   define	MAX_SOCKET_BLOCK	4096
    mblk_t	*b;
    uint_t	bytes; /* # bytes to try and read */
    int		error; /* return */
    uio_t	uio;
    iovec_t	iov;
    struct socket *so = NULL;

    TDEBUG(("tn_socketRead: Called minor(%d)\n", TN_MINOR(tn)));
    TNCHECKLOCK(tn);

    if (NULL == tn->tn_vsocket) {
	TDEBUG(("tn_socketRead: Socket not attached to minor(%d)\n", 
		TN_MINOR(tn)));
	return(NULL);
    }
    so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(tn->tn_vsocket)));

    bytes = so->so_rcv.sb_cc;
    if (0 == bytes) {
	if (so->so_state & SS_ISDISCONNECTING) {
	    (void)putctl(tn->tn_rq->q_next, M_HANGUP);
	}
	return(NULL);
    } else if (bytes > MAX_SOCKET_BLOCK) {
	bytes = MAX_SOCKET_BLOCK;
    }

    b = allocb(bytes, BPRI_MED);
    if (NULL == b) {
	/*
	 * If we could not allocate a buffer, set up a "bufcall", 
	 * and hope we can process it later.
	 */
	TDEBUG(("tn_socketRead: minor(%d) no buffers available\n", 
		TN_MINOR(tn)));
	if (0 == tn->tn_rqBufCallId) {
	    tn->tn_rqBufCallId = bufcall(bytes, BPRI_MED, (void (*)())tn_rsrv, 
					 (long)tn->tn_rq);
	}
    } else {
	/*
	 * Ok, we have the buffers, now lets go get the data.
	 */
	iov.iov_base	= (caddr_t)b->b_datap->db_base;
	iov.iov_len	= bytes;

	uio.uio_iov	= &iov;
	uio.uio_iovcnt 	= 1;
	uio.uio_segflg	= UIO_SYSSPACE;	/* we are reading into system space */
	uio.uio_offset	= 0;
	uio.uio_resid	= iov.iov_len;

	VSOP_RECEIVE(tn->tn_vsocket, NULL, &uio, 0, NULL, error); /* bang */

	TDEBUG(("tn_socketRead: minor(%d) soreceive returned(%d) "
		"bytes [req(%d) got(%d)]\n", 
		TN_MINOR(tn), error, bytes, bytes - uio.uio_resid));

	if (0 != error) {
	    /*
	     * For any of these error cases, the allocate block is used to 
	     * forward up the error.
	     */
	    switch(error) {
	    case ECONNABORTED:
	    case ENOTCONN:
		b->b_datap->db_type = M_HANGUP;
		b->b_wptr = b->b_rptr;
		break;
	    case EWOULDBLOCK:		/* nothing there */
	    case EAGAIN:
		freemsg(b);
		b = NULL;
		break;
	    default:
		cmn_err(CE_WARN, "tn_socketRead: unexpected result (%d)",
			error);
		b->b_datap->db_type = M_ERROR;
		b->b_wptr = b->b_rptr;
		*b->b_wptr++ = error;
		break;
	    }
	} else if (bytes == uio.uio_resid) {
	    /* 
	     * BSD sockets return 0 bytes on read with success if 
	     * other end gone.
	     */
	    b->b_datap->db_type = M_HANGUP;
	    b->b_wptr = b->b_rptr;
	} else {
	    b->b_wptr += bytes - uio.uio_resid;
	}
	
    }

    return(b);
}

/* ARGSUSED */
static	int
tn_socketPending(struct socket *socket, int event, queue_t *rq)
/*
 * Function: tn_socketPending
 * Purpose: Called when socket has pending data.
 * Parameters:	socket - socket with event pending
 *		event  - event that is pending
 *		rq - read stream queue
 * Returns:	0
 * 
 * Locks held on entry:	none
 * Locks held on exit:	none
 * Locks aquired:	tn->tn_lock
 */
{
    tn_t	*tn;
    int		pl;
    queue_t	*wq;

    tn = (tn_t *)rq->q_ptr;

    TDEBUG(("tn_socketPending: minor(%d) event(%d)\n", TN_MINOR(tn), event));

    ASSERT( tn->tn_lock );

    pl = TNLOCK(tn);
    switch(event) {
    case SSCO_INPUT_ARRIVED:
	qenable(rq);
	break;
    case SSCO_OUTPUT_AVAILABLE:
	wq = WR(rq);
	enableok(wq);			/* allow this to progress */
	qenable(wq);			/* ... and schedule it */
	break;
    default:
	TDEBUG(("tn_socketPending: Unexpected event: %d\n", event));
	break;
    }
    TNUNLOCK(tn, pl);
    return(0);
}

static	int
tn_connectSocket(tn_t *tn, struct tcssocket_info *tsi)
/*
 * Function: tn_connectSocket
 * Purpose: To associate a socket with a telnet connection
 * Parameters: 	tn - pointer to the telnet connection
 *		sockect info ioctl parameters
 * Returns:	nothing
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    int			error;
    struct socket	*so = NULL;

    TDEBUG(("tn_connectSocket: minor(%d)\n", TN_MINOR(tn)));

    if (NULL != tn->tn_vsocket) {
	return(EBUSY);
    } else {

	/* If the socket is there, connect it to the stream */

	error = getvsock(tsi->tsi_fd, &tn->tn_vsocket);
	if (0 == error) {
	    so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(tn->tn_vsocket)));
	    error = getf(tsi->tsi_fd, &tn->tn_file);
	    ASSERT(0 == error && tn->tn_file);
	    VFILE_REF_HOLD(tn->tn_file);		/* be sure it stays around */
	    so->so_callout = tn_socketPending;
	    so->so_callout_arg = (caddr_t)tn->tn_rq;
	    ASSERT(tn->tn_wq->q_monpp);
            so->so_monpp = tn->tn_rq->q_monpp;
	    so->so_state |= (SS_TPISOCKET | SS_WANT_CALL |SS_NBIO);

	    bcopy(tsi->tsi_name, tn->tn_name, sizeof(tn->tn_name));
	    bcopy(tsi->tsi_options, tn->tn_options,sizeof(tn->tn_options));
	    bcopy(tsi->tsi_name, tn->tn_name, sizeof(tn->tn_name));
	    bcopy(tsi->tsi_doDontResp, 
		  tn->tn_doDontResp ,sizeof(tn->tn_doDontResp));
	    bcopy(tsi->tsi_willWontResp, 
		  tn->tn_willWontResp ,sizeof(tn->tn_willWontResp));
	    bcopy(tsi->tsi_name, tn->tn_name, sizeof(tn->tn_name));
	    qenable(tn->tn_wq);
	    qenable(tn->tn_rq);
	} else {
	    TDEBUG(("tn_connectSocket: getSock failed\n"));
	    tn->tn_vsocket = NULL;
	} 
    } 
    return(error);
}
	    

	    
/**************************************************************************
 *                                                                        *
 *                     	      Kernel Telnetd                              *
 *                                                                        *
 **************************************************************************/


/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)defs.h	5.9 (Berkeley) 9/14/90
 */

#ifdef NOTDEF
static	int
tn_setRemote(tn_t *tn)
/*
 * Function: tn_setRemote
 * Purpose: Called to set up options as if we are the "remote".
 * Parameters: tn - pointer to the telnet connection to set up.
 * Returns: 0 - success, -1 - failed.
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    static	char	soTtype[] = {IAC,SB,TELOPT_TTYPE,TELQUAL_SEND,IAC,SE};

    /* Try to get terminal type */

    if (his_state_is_will(tn, TELOPT_TTYPE)) {
	(void)tn_sendToRemote(tn, soTtype, sizeof(soTtype), TRUE);
    }
    return(0);
}

static	void
tn_reqOption(tn_t *tn, int option)
/*
 * Function: tn_reqOption
 * Purpose: To request that a particular option be enabled.
 * Parameters: 	tn - pointer to telnet connection
 *		option - option we "request" be enabled.
 * Returns: nothing
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    if (tn->tn_doDontResp[option] == 0 
	&& his_state_is_will(tn, option) 
	|| his_want_state_is_will(tn, option)){
	return;
    }
    tn->tn_doDontResp[option]++;
    set_his_want_state_will(tn, option);

    (void)TN_DO(tn, option);
}
#endif	/* NOTDEF */

static	void
dooption(tn_t *tn, int option)
{
    int	sendWill;

    TDEBUG(("dooption: option(%s)[%d]\n", TELOPT(option), option));

    if (tn->tn_willWontResp[option]) {
		tn->tn_willWontResp[option]--;
		if (tn->tn_willWontResp[option] 
		    && my_state_is_will(tn, option)) {
		    tn->tn_willWontResp[option]--;
		}
	}
	if ((tn->tn_willWontResp[option] == 0) 
	    && (my_want_state_is_wont(tn, option))) {

	    switch(option) { 
	    case TELOPT_ECHO:		/* ECHO MODE */
		tn->tn_termio.c_lflag |= ECHO;
		sendWill = tn_sendIoctlUpStream(tn, TCSETA, &tn->tn_termio, 
						sizeof(tn->tn_termio));
		break;
	    case TELOPT_BINARY:	
		tn->tn_termio.c_iflag &= ~ICANON;
		sendWill = tn_sendIoctlUpStream(tn, TCSETA, &tn->tn_termio, 
						sizeof(tn->tn_termio));
		break;	
	    case TELOPT_SGA:
		sendWill = TRUE;
		break;
	    case TELOPT_STATUS:
		sendWill = TRUE;
		break;
	    default:
		sendWill = FALSE;
		break;
	    }
	    if (sendWill) {
		set_my_want_state_will(tn, option);
		TN_WILL(tn, option);
	    } else {
		tn->tn_willWontResp[option]++;
		TN_WONT(tn, option);
	    }
	}
    set_my_state_will(tn, option);
}  /* end of dooption */

static	void
dontoption(tn_t *tn, int option)
{
    TDEBUG(("dontoption: option(%s)[%d]\n", TELOPT(option), option));


    if (tn->tn_willWontResp[option]) {
	tn->tn_willWontResp[option]--;
	if (tn->tn_willWontResp[option] && my_state_is_wont(tn, option)) {
	    tn->tn_willWontResp[option]--;
	}
    }
    if ((tn->tn_willWontResp[option] == 0) 
	&& (my_want_state_is_will(tn, option))) {
	switch(option) {
	case TELOPT_ECHO:			/* MODE_ECHO=FALSE */
	    tn->tn_termio.c_lflag &= ~ECHO;
	    (void)tn_sendIoctlUpStream(tn, TCSETA, &tn->tn_termio, 
				       sizeof(tn->tn_termio));
	    break;
	default:
	    break;
	}
	set_my_want_state_wont(tn, option);
	if (my_state_is_will(tn, option)) {
	    TN_WONT(tn, option);
	}
    }

    set_my_state_wont(tn, option);
}  /* end of dontoption */

static	void
willoption(tn_t *tn, int option)
{
    int		sendDo;

    TDEBUG(("willoption: option(%s)[%d]\n", TELOPT(option), option));
    /*
     * 2 cases possible, this is a confirmation, OR this is information. 
     * If it is confirmation, then thats all we need to do. If it is 
     * information, then we "may" turn it on.
     */

    if (tn->tn_doDontResp[option]) {
	tn->tn_doDontResp[option]--;
	if (tn->tn_doDontResp[option] && his_state_is_will(tn, option)) {
	    tn->tn_doDontResp[option]--;
	}
    }

    if (tn->tn_doDontResp[option] == 0) {
	if (his_want_state_is_wont(tn, option)) {
	    switch (option) {
	    case TELOPT_BINARY:
		sendDo = TRUE;
		break;
	    case TELOPT_ECHO:
		break;
	    case TELOPT_TM:
		sendDo = TRUE;
		break;
	    case TELOPT_TTYPE:
	    case TELOPT_SGA:
	    case TELOPT_NAWS:
	    case TELOPT_TSPEED:
	    case TELOPT_XDISPLOC:
		sendDo = TRUE;
		break;
	    default:
		sendDo = FALSE;
		break;
	    }

	}
	if (sendDo) {
	    set_his_want_state_will(tn, option);
	    TN_DO(tn, option);
	} else {
	    tn->tn_doDontResp[option]++;
	    TN_DONT(tn, option);
	}
    }

    set_his_state_will(tn, option);
}  /* end of willoption */

static	void
wontoption(tn_t *tn, int option)
{

    TDEBUG(("wontoption: option(%s)[%d]\n", TELOPT(option), option));

    if (tn->tn_doDontResp[option]) {
	tn->tn_doDontResp[option]--;
	if (tn->tn_doDontResp[option] && his_state_is_wont(tn, option)) {
	    tn->tn_doDontResp[option]--;
	}
    }
    if (tn->tn_doDontResp[option] == 0) {
	if (his_want_state_is_will(tn, option)) {
	    switch(option) {
	    case TELOPT_ECHO:
		/*
		 * Only in the case of start up do we do this - to be
		 * sure things work OK with 4.2 systems.
		 */
		tn->tn_willWontResp[option]++;
		TN_WILL(tn, option);
		break;
	    case TELOPT_BINARY:
		tn->tn_termio.c_iflag |= ICANON;
		break;
	    case TELOPT_TTYPE:		/*  */
		break;
	    case TELOPT_NAWS:		/* we support window size */
		break;
	    default:
		break;
	    }
	    set_his_want_state_wont(tn, option);
	    if (his_state_is_will(tn, option)) {
		TN_DONT(tn, option);
	    }
	}
    }

    set_his_state_wont(tn, option);
} /* end of wontoption */

/*
 * suboption()
 *
 *	Look at the sub-option buffer, and try to be helpful to the other
 * side.
 *
 *	Currently we recognize:
 *
 *	Terminal type is
 *	Window size
 */
suboption(tn_t *tn)
{
#	define	tolower(c)	\
    ((((c)>='A') && ((c)<='Z')) ? ((c)-('A'-'a')) : (c))

    register int subchar;

    subchar = SB_GET(tn);
    switch (subchar) {
    case TELOPT_TTYPE: {
	int	i;

	if (his_state_is_wont(tn, TELOPT_TTYPE)) /* Ignore if option disabled*/
	    break;
	if (SB_GET(tn) != TELQUAL_IS) {
	    return(-1);			/* ??? XXX but, this is the most robust */
	}

	for (i = 0; i < sizeof(tn->tn_name)-1 && !SB_EOF(tn); i++) {
	    register int c;
	    c = SB_GET(tn);	
	    tn->tn_name[i] = (char)tolower(c);
	}
	tn->tn_name[i] = '\0';
	break;
    } /* end of case TELOPT_TTYPE */

    case TELOPT_NAWS: {
	register int xwinsize, ywinsize;

	if (!his_state_is_wont(tn, TELOPT_NAWS)) { /* Ignore if option disabled */
	    if (SB_EOF(tn)) {
		break;
	    }
	    xwinsize = SB_GET(tn) << 8;
	    if (SB_EOF(tn)) {
		break;
	    }
	    xwinsize |= SB_GET(tn);
	    if (SB_EOF(tn)) {
		break;
	    }
	    ywinsize = SB_GET(tn) << 8;
	    if (SB_EOF(tn)) {
		break;
	    }
	    ywinsize |= SB_GET(tn);
	    tn->tn_ws.ws_row = ywinsize;
	    tn->tn_ws.ws_col = xwinsize;
	    
	    /* OK - send signal up to user */
	    
	    (void)putctl1(tn->tn_rq->q_next, M_PCSIG, SIGWINCH);
	    
	    /* And update stty_ld stuff */

	    (void)tn_sendIoctlUpStream(tn, TIOCSWINSZ, &tn->tn_ws, 
				       sizeof(tn->tn_ws));
	    break;
	}
    }
    default: /* Well, just leave it for now */
	break;
    }

    SB_CLEAR(tn);
    return(0);
#undef	tolower
}  /* end of suboption */

static mblk_t	*
tn_telnetReceive(tn_t *tn, mblk_t *b)
/*
 * Function: tn_telnetRecieve
 * Purpose: To process an inbound telnet packet
 * Parameters: 	tn - telnet connection message arrived on.
 *		b  - mlock containing data
 * Returns:	pointer to LIST of mblks to pass up the stream.
 * Notes: This routine replaces "telrcv" in telnetd/state.c.
 * 
 * Locks held on entry:	tn->tn_lock
 * Locks held on exit:	tn->tn_lock
 * Locks aquired:	none
 */
{
    register char 	c, *cp;
    register mblk_t	*ob;		/* output (upstream) block */
    register	u_char	*obc;		/* ouput buffer current */
    int			done;		/* TRUE -->  break loop */

    TDEBUG(("tn_telnetReceive: Called\n"));

    ob = allocb(b->b_wptr - b->b_rptr, BPRI_MED);
    if (NULL == ob) {
	if (0 != tn->tn_rqBufCallId) {
	    unbufcall(tn->tn_rqBufCallId);
	}
	tn->tn_rqBufCallId = bufcall(b->b_wptr - b->b_rptr, BPRI_MED,
				     qenable, (long)tn->tn_rq);
	return(NULL);
    } 
    obc = ob->b_wptr;

    for (cp = (char *)b->b_rptr, done = FALSE; !done && (cp < (char *)b->b_wptr); cp++) {
	c = *cp & 0377;
	switch (tn->tn_state) {
	case TS_CR:
	    TDEBUG2(("tn_telnetReceive: state(TS_CR)\n"));
	    tn->tn_state = TS_DATA;
	    /* Strip off \n or \0 after a \r */
	    if ((c == 0) || (c == '\n')) {
		break;
	    }
	    /* FALL THROUGH */

	case TS_DATA:
	    TDEBUG2(("tn_telnetReceive: state(TS_DATA)\n"));
	    if (c == IAC) {
		/*
		 * If we got an IAC, and we have already saved some data, 
		 * then we pass it back to be processed. We will get 
		 * back to this point again .... sometime.
		 */
		tn->tn_state = TS_IAC;
		if (obc >= ob->b_rptr) { /* have some data */
		    done = TRUE;
		}
	    } else {
		/*
		 * We now map \r\n ==> \r for pragmatic reasons.
		 * Many client implementations send \r\n when
		 * the user hits the CarriageReturn key.
		 *
		 * We USED to map \r\n ==> \n, since \r\n says
		 * that we want to be in column 1 of the next
		 * printable line, and \n is the standard
		 * unix way of saying that (\r is only good
		 * if CRMOD is set, which it normally is).
		 */
		if ((c == '\r') && his_state_is_wont(tn, TELOPT_BINARY)) {
		    tn->tn_state = TS_CR;
		    c = '\n';
		}
		/* 
		 * Check for flow control....
		 */

		if (tn->tn_flags & TN_FLOW_CNTL) {
		    if (((tn->tn_flags & TN_TXSTOP) && 
			 (c == tn->tn_termio.c_cc[VSTART])) ||
			((IXANY & tn->tn_termio.c_iflag) && 
			 (c != tn->tn_termio.c_cc[VSTOP]))) {
			tn->tn_flags &= ~TN_TXSTOP;
			qenable(tn->tn_wq);
			if (c == tn->tn_termio.c_cc[VSTART]) {
			    break;	/* eat the cfharacter */
			}
		    } else if (c == tn->tn_termio.c_cc[VSTOP]) {
			tn->tn_flags |= TN_TXSTOP;
			break;		/* eat the character */
		    }
		}
		*obc++ = c;
	    }
	    break;

	case TS_IAC:
	    TDEBUG2(("tn_telnetReceive: state(TS_IAC) cmd(%s)\n", TELCMD(c)));
	    switch (c) {
		/*
		 * Send the process on the pty side an
		 * interrupt.  Do this with a NULL or
		 * interrupt char; depending on the tty mode.
		 */
	    case IP:
	    case BREAK:
	    case ABORT:
		flushq(tn->tn_rq, FLUSHDATA);
		if (!putctl1(tn->tn_rq->q_next, M_PCSIG,
			     c == IP ? SIGINT : SIGQUIT)) {
		    if (0 != tn->tn_rqBufCallId) {
			unbufcall(tn->tn_rqBufCallId);
		    }
		    tn->tn_rqBufCallId = bufcall(1, BPRI_HI,qenable,
						 (long)tn->tn_rq);
		}
		done = TRUE;
		break;

		/*
		 * Are You There?
		 */
	    case AYT:
	    {
		static const char aytReply[] = "\r\n[Yes]\r\n";
		if (!tn_sendToRemote(tn,aytReply,strlen(aytReply),TRUE)){
		    return(NULL);		/* process later */
		}
		break;
	    }
		/*
		 * Abort Output
		 */
	    case AO:
	    {
		static const char aoReply[] = { IAC , DM };
		/* 
		 * We could execute this more than once if tn_sendUp works, 
		 * and this fails, but hey, it is harmless right?
		 */
		if (!putctl1(tn->tn_rq->q_next, M_FLUSH, FLUSHW) || 
		    (!tn_sendToRemote(tn, aoReply, sizeof(aoReply), TRUE))) {
		    if (0 != tn->tn_rqBufCallId) {
			unbufcall(tn->tn_rqBufCallId);
			tn->tn_rqBufCallId = bufcall(1, BPRI_HI, qenable, 
						     (long)tn->tn_rq);
		    } 
		    done = TRUE;	/* come back again */
		}
		break;
	    }
		/*
		 * Erase Character and
		 * Erase Line
		 */
	    case EC:
		*obc++ = tn->tn_termio.c_cc[VERASE];
		break;
	    case EL:
		*obc++ = tn->tn_termio.c_cc[VKILL];
		break;
#if 0
		/*
		 * Check for urgent data...
		 */
	    case DM:
		SYNCHing = stilloob(net);	%%%%%
		    settimer(gotDM);
		break;
#endif

		/*
		 * Begin option subnegotiation...
		 */
	    case SB:		
		tn->tn_state = TS_SB;
		SB_CLEAR(tn);
		continue;

	    case WILL:
		tn->tn_state = TS_WILL;
		continue;

	    case WONT:
		tn->tn_state = TS_WONT;
		continue;

	    case DO:
		tn->tn_state = TS_DO;
		continue;

	    case DONT:
		tn->tn_state = TS_DONT;
		continue;
	    case IAC:
		*obc++ = c;
		break;
	    case NOP:			/* just swallow */
		break;
	    case SUSP:
		break;
	    default:
		TDEBUG(("tn_telnetReceive: Unknown IAC option (%d)\n", c));
	    }
	    tn->tn_state = TS_DATA;
	    break;

	case TS_SB:
	    TDEBUG2(("tn_telnetReceive: TS_SB\n"));
	    if (c == IAC) {
		tn->tn_state = TS_SE;
	    } else {
		SB_ACCUM(tn, c);
	    }
	    break;

	case TS_SE:
	    TDEBUG2(("tn_telnetReceive: TS_SE\n"));
	    if (c != SE) {
		if (c != IAC) {
		    /*
		     * bad form of suboption negotiation.
		     * handle it in such a way as to avoid
		     * damage to local state.  Parse
		     * suboption buffer found so far,
		     * then treat remaining stream as
		     * another command sequence.
		     */
		    SB_TERM(tn);
		    suboption(tn);
		    tn->tn_state = TS_IAC;
		    cp--;		/* BACK up so that we re-process this character */
		    break;
		}
		SB_ACCUM(tn, c);
		tn->tn_state = TS_SB;
	    } else {
		SB_TERM(tn);
		suboption(tn);		/* handle sub-option */
		tn->tn_state = TS_DATA;
	    }
	    break;

	case TS_WILL:
	    TDEBUG2(("tn_telnetReceive: state(TS_WILL)\n"));
	    willoption(tn, c);
	    tn->tn_state = TS_DATA;
	    continue;

	case TS_WONT:
	    TDEBUG2(("tn_telnetReceive: state(TS_WONT)\n"));
	    wontoption(tn, c);
	    tn->tn_state = TS_DATA;
	    continue;
	case TS_DO:
	    TDEBUG2(("tn_telnetReceive: state(TS_DO)\n"));
	    dooption(tn, c);
	    tn->tn_state = TS_DATA;
	    continue;

	case TS_DONT:
	    TDEBUG2(("tn_telnetReceive: state(TS_DONT)\n"));
	    dontoption(tn, c);
	    tn->tn_state = TS_DATA;
	    continue;
	default:

#if _TDEBUG
	    cmn_err(CE_PANIC, "tsd: invalid state=%d", tn->tn_state);
#else
	    cmn_err(CE_WARN, "tsd: invalid state=%d", tn->tn_state);
#endif
	    freemsg(b);
	    break;
	}
    }
    b->b_rptr = (unsigned char *)cp;		/* update read pointer */
    ob->b_wptr = obc;
    return(ob);
}

