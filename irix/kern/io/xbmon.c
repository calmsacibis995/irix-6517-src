/* 
 *	This driver has the prefix of "xbmon".
 */

#include <sys/types.h>
#include <ksys/vfile.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <ksys/ddmap.h>
#include <sys/mload.h>
#include <sys/param.h>
#include <sys/ddi.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xbow.h>

#include <sys/xbmon.h>			/* for those shared with appls. */
#ifdef IP30
#include <sys/slotnum.h>
#endif	/* IP30 */


#define	NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

#define	XB_MAX_OPEN			1

#define	XB_DRIVER_STATE_UNLOADED	0
#define	XB_DRIVER_STATE_INIT		1

#define	NO_ERROR			0
#define	INVALID_UNIT			-1
#define	UNIMPLEMENTED			EINVAL

#define	XB_STATE_CLOSED			0
#define	XB_STATE_OPEN			1

/* from .h */
#define XB_DEFAULT_TIMEOUT      	(HZ/100)       /* 10 msec      */
#define XB_MAX_BUFSIZE          	2000000        /* 20M entries  */

#define XB_DEFAULT_BUFSIZE      	10000          /* 10K entries */

#define XB_FLAG_OPEN            	0x01           /* the device is open */
#define XB_FLAG_RUNNING         	0x02           /* the timer is running */
/* end: from .h */

#define	INVALID_TIMEOUTID		0
#define	INVALID_BUFSIZE			0

/*	XXX - Put these constant defns in xbow.h eventually : */

#define XBOW_WID_PERF_LINK_SEL_SHFT	20
#define XBOW_WID_PERF_LINK_SEL_MSK	0x00700000	
#define XBOW_WID_PERF_CTR_MSK		0x000FFFFF	
#define XB_CTRL_IBUF_LEVEL_SHFT		25

#define XB_CTRL_PERF_MODE_NONE		0x0
#define XB_CTRL_PERF_MODE_SRC		0x10000000
#define XB_CTRL_PERF_MODE_DST		0x20000000
#define XB_CTRL_PERF_MODE_LEVEL		0x30000000

char			*xbmonmversion = M_VERSION;
int			xbmon_devflag = D_MP;

typedef struct {
        xb_event *eq_buffer;
        xb_event *eq_bufend;
        xb_event *eq_in;
        xb_event *eq_out;
        int eq_maxevents;
        int eq_overflows;
} event_queue;

typedef struct xbmon_soft_s {
	vertex_hdl_t	conn;		/* connection point (xbow) */
	vertex_hdl_t	vhdl;		/* our vertex (xbmon) */
	xbow_t	       *xbow;		/* for PIO to xbow chip */

	unsigned	isopen :1;	/* xbmon is exclusive-open */

	/* add more per-crossbow state here */

	event_queue 	xb_traceq; 		/* circular queue */
	lock_t		xb_qlock;
	u_short		xb_flags;		/* device's state */
	u_int		xb_timeout_value;	/* timeout for timer */
	u_int		xb_timeout_id;		/* timer id */
	u_int		xb_bufsize;
	int		xb_multiplex;
	xb_modeselect_t xb_modesel_a,		/* mode for Perf Cntr A */
			xb_modesel_b;		/* mode for Perf Cntr B */

	xbowreg_t	xb_prev_cntr_a_value,
			xb_prev_cntr_b_value,
			xb_prev_retry_rcv_value[MAX_XBOW_PORTS],
			xb_prev_retry_xmt_value[MAX_XBOW_PORTS];

	__uint64_t	xb_prev_nsec_value;
	__uint64_t	xb_prev_sec_value;

	xb_vcntr_block_t *xb_vcounters;

	int		xb_active[MAX_XBOW_PORTS];
	int		xb_active_count;
	int		xb_curlinkidx;

}		xbmon_soft_t;

#define	xbmon_soft_set(v,i)	(hwgraph_fastinfo_set((v),(arbitrary_info_t)(i)))
#define	xbmon_soft_get(v)	((xbmon_soft_t *)hwgraph_fastinfo_get((v)))

static u_short		xb_driver_state = XB_DRIVER_STATE_UNLOADED;

static xbowreg_t	xb_perf_modes[4]={
				XB_CTRL_PERF_MODE_NONE,
				XB_CTRL_PERF_MODE_SRC,
				XB_CTRL_PERF_MODE_DST,
				XB_CTRL_PERF_MODE_LEVEL};

__uint64_t		xb_eventmask = 0X0UL;

static void xb_timeout_rtn(caddr_t arg);
static void xb_enable_cntr(xbow_t *xbow, int counter, xb_modeselect_t *mode);
static void xb_disable_cntr(xbmon_soft_t *soft);

/* queue operations: */
static int  eq_init(event_queue *eq, u_int maxev);
static void eq_free(event_queue *eq, u_int maxev);
static int  eq_enqueue(xbmon_soft_t *soft, event_queue *eq, xb_event *new_event);
static int  eq_deqmulti(xbmon_soft_t *soft, event_queue *eq, uio_t *uiop);
#ifdef notdef
static int  eq_dequeue1(xbmon_soft_t *soft, event_queue *eq, xb_event *event);
#endif


#ifdef	DEBUG
u_int	xb_debug = 1;
#define	DBG_PRINT(x)		\
		if (xb_debug)	\
			printf x 
#else	/* DEBUG */
#define	DBG_PRINT(x)
#endif	/* DEBUG */


/* ---------------------  queue support --------------------------- */

int
eq_init(event_queue *eq, u_int maxev)
{
	eq->eq_buffer = (xb_event *) kmem_zalloc(sizeof(xb_event)*maxev,
					KM_NOSLEEP|KM_CACHEALIGN);

	if (eq->eq_buffer == NULL)
	    return ENOMEM;

	eq->eq_bufend = eq->eq_buffer + maxev;
	eq->eq_in = eq->eq_buffer;
	eq->eq_out = eq->eq_buffer;
	eq->eq_maxevents = maxev;
	eq->eq_overflows = 0;

	return (0);
}

void
eq_free(event_queue *eq, u_int maxev)
{

       if (eq->eq_buffer != NULL)
	    kmem_free(eq->eq_buffer, sizeof(xb_event)*maxev);

	eq->eq_buffer = NULL;
	eq->eq_bufend = NULL;
	eq->eq_in = NULL;
	eq->eq_out = NULL;
	eq->eq_maxevents = 0;
	eq->eq_overflows = 0;
}

/* eq_enqueue returns 0 for failure and 1 for success */
int
eq_enqueue(xbmon_soft_t *soft, event_queue *eq, xb_event *new_event)
{
	xb_event	*temp_in;
	int		spinl;

	spinl = mutex_spinlock(&soft->xb_qlock);

	/* calculate in + 1 to check if the queue is full */
	temp_in = eq->eq_in + 1;

	if (temp_in == eq->eq_bufend)
	    temp_in = eq->eq_buffer;

	if (temp_in==eq->eq_out) {   /* if queue is full, return 0 */
	    eq->eq_overflows++;
	    mutex_spinunlock(&soft->xb_qlock, spinl);
	    return (0);
	}

	bcopy(new_event, eq->eq_in, sizeof(xb_event));

	eq->eq_in = temp_in;

	mutex_spinunlock(&soft->xb_qlock, spinl);
	return (1);  
 
} /* end : eq_enqueue */

#ifdef notdef
int
eq_dequeue1(xbmon_soft_t *soft, event_queue *eq, xb_event *event)
{
	xb_event	*temp_in;
	xb_event	*temp_out;
	int		spinl;

	/* protected access to eq_in : */

	spinl = mutex_spinlock(&soft->xb_qlock);
	temp_in = eq->eq_in;
	temp_out = eq->eq_out;
	mutex_spinunlock(&soft->xb_qlock, spinl);

	if (temp_out == temp_in)
	    return (0);  /* queue is empty */

	bcopy(eq->eq_out, event, sizeof(xb_event));

	eq->eq_out++;

	if (eq->eq_out==eq->eq_bufend)
	    eq->eq_out = eq->eq_buffer;

       return (1);

} /* end : eq_dequeue1 */
#endif

int
eq_deqmulti(xbmon_soft_t *soft, event_queue *eq, uio_t *uiop)
{
	int		retcnt, retcnt2;
	int		cnt, error;
	xb_event	*temp_in;
	xb_event	*temp_out;
	int		spinl;

	cnt = uiop->uio_resid / sizeof(xb_event);

	/* protected access to eq_in & eq_out : */

	spinl = mutex_spinlock(&soft->xb_qlock);
	temp_in = eq->eq_in;
	temp_out = eq->eq_out;
	mutex_spinunlock(&soft->xb_qlock, spinl);

	/* check to see if queue is empty */

	if (temp_out == temp_in)
	    return (0);  

	/*
	 * there are two cases to consider: if the eq_buffer
	 * pointers are not "wrapped", then all of the queue 
	 * entries are contiguous.
	 *
	 */

	if (temp_in > temp_out) {  /* not wrapped */

	    retcnt = temp_in - temp_out;
	    if (retcnt > cnt)
		retcnt = cnt;

	    /*
	     * copy the data out:  If error, get out right away, and don't
	     * bother updating the queue pointers since we didn't copy
	     * anything out anyway.
	     *
	     */

	    error = uiomove((caddr_t)temp_out,
			    retcnt * sizeof(xb_event),
			    UIO_READ,
			    uiop);
	    if (error)
		return (error);

	    /* update the 'out' pointer: */
	    temp_out += retcnt;

	} else {   /* eq_buffer is wrapped */

	    /*
	     * calculate the # of elements from here to the end of the
	     * eq_buffer. If it's more than what we want, reduce it.
	     */

	    retcnt = (eq->eq_bufend - temp_out);
	    if (retcnt > cnt)
		retcnt = cnt;

	    /* copy the first chunk of data: */

	    error = uiomove((caddr_t)temp_out,
			    retcnt * sizeof(xb_event),
			    UIO_READ,
			    uiop);
	    if (error)
		return (error);

	    temp_out += retcnt;

	    if (temp_out == eq->eq_bufend)
		temp_out = eq->eq_buffer;

	    /*
	     * subtract what we've copied so far from the total we
	     * want to copy. If we did it all, return success now.
	     */

	    cnt -= retcnt;
	    if (cnt == 0)
		goto exit;

	    /*
	     * There is still some more to copy.  Find out
	     * how much is left between the beginning of the eq_buffer
	     * and the 'in' pointer.
	     */

	    retcnt2 = temp_in - eq->eq_buffer;
	    if (retcnt2 > cnt)
		retcnt2 = cnt; 

	    /*
	     * copy the second chunk of data. If there's an error, return now,
	     * the queue pointers will _not_ get updated, but that's okay since
	     * we'll get to read it again next time when uiomove does not fail.:
	     */

	    error = uiomove((caddr_t)eq->eq_buffer,
			    retcnt2 * sizeof(xb_event),
			    UIO_READ,
			    uiop);
	    if (error)
		return error;

	    /* update the 'out' pointer */

	    temp_out = eq->eq_buffer + retcnt2;
	}

	/*
	 * Finally, update the eq_out pointer. This would normally be done
	 * in the protection of a spinlock, but we don't expect an interrupt
	 * while writing just the eq_out variable (that would be interrupting
	 * a single memory write, highly unlikely :-)
	 *
	 */ 
exit:    
	eq->eq_out = temp_out;
	return (0); 

} /* end : eq_deqmulti */

/* ---------------------  queue support : end  -------------------- */


/*
 *	Called when driver is loaded.
 */
void
xbmon_init(void)
{
#if DEBUG && ATTACH_DEBUG
printf("xbmon_init\n");
#endif
	xwidget_driver_register(XBOW_WIDGET_PART_NUM,
				XBOW_WIDGET_MFGR_NUM,
				"xbmon_",
				0);	
} /* end : xbmon_init() */

/*
 *	Like all [new] attach routines, we are passed a
 *	connection point; we do not own this vertex. We
 *	get to build an edge with the name of our
 *	chosing, pointing to a vertex that will be
 *	presented to open/close/read/write/ioctl.
 *
 */
int
xbmon_attach(vertex_hdl_t conn)
{
	vertex_hdl_t	vhdl;
	xbow_t	       *xbow;
	xbmon_soft_t   *soft;

#if DEBUG && ATTACH_DEBUG
cmn_err(CE_CONT, "%v: xbmon_attach\n",conn);
#endif

	xbow = (xbow_t *)xtalk_piotrans_addr
		(conn, 0, 0, sizeof (xbow_t), 0);
	ASSERT(xbow != NULL);

	NEW(soft);
	ASSERT(soft != NULL);
	soft->conn = conn;
	soft->xbow = xbow;
	soft->isopen = 0;
	/* get the dynamic resources we want */

	soft->xb_flags = 0;
	soft->xb_timeout_value = XB_DEFAULT_TIMEOUT;
	soft->xb_timeout_id = INVALID_TIMEOUTID;
	soft->xb_bufsize = XB_DEFAULT_BUFSIZE;
	soft->xb_active_count = 0;
	soft->xb_curlinkidx = 0;

	xb_eventmask = 0x0UL;
	xb_driver_state	= XB_DRIVER_STATE_INIT;	

	if (soft->xb_qlock == NULL)
	    spinlock_init(&soft->xb_qlock, "soft->xb_qlock");

	hwgraph_char_device_add(conn, EDGE_LBL_PERFMON, "xbmon_", &vhdl);
	soft->vhdl = vhdl;
	xbmon_soft_set(vhdl, soft);

	return 0;

} /* end : xbmon_attach() */

/*
 *	Called when driver is unloaded.
 *
 *	Currently, we assume xbow.c and xbmon.c
 *	are always linked into the system, so the
 *	chances of this being meaningful are slim.
 */
int
xbmon_unload(void)
{

	/* deallocate all the dynamic resources allocated in xbinit(). */

	xb_driver_state = XB_DRIVER_STATE_UNLOADED;	

    	return 0;

} /* end : xbmon_unload() */

/*
 *	Called when driver is opened.
 */
int
xbmon_open(dev_t *devp, int mode)
{
	vertex_hdl_t	vhdl;
	xbmon_soft_t   *soft;
	/*REFERENCED*/
	xbow_t	       *xbow;
	int		i;
	timespec_t      tv;

	if (xb_driver_state != XB_DRIVER_STATE_INIT) {
	    return EIO;
	}

	if ((mode & FNDELAY) || (mode & FNONBLOCK)) {
	    return EINVAL;		/* sorry, they are not supported. */
	}

	vhdl = dev_to_vhdl(*devp);
	soft = xbmon_soft_get(vhdl);
	xbow = soft->xbow;

	if (soft->isopen)
		return EBUSY;		/* exclusive access only */

	soft->xb_bufsize = XB_DEFAULT_BUFSIZE;
	if (eq_init(&soft->xb_traceq, soft->xb_bufsize) != 0) {
		return ENOMEM;
	}

	soft->xb_timeout_value = XB_DEFAULT_TIMEOUT;
	soft->xb_timeout_id = INVALID_TIMEOUTID;
	soft->xb_flags = XB_FLAG_OPEN;

	soft->xb_modesel_a.link = 0;
	soft->xb_modesel_a.mode = 0;
	soft->xb_modesel_b.link = 0;
	soft->xb_modesel_b.mode = 0;

	soft->xb_vcounters = (xb_vcntr_block_t *)kmem_zalloc(_PAGESZ, KM_CACHEALIGN);
	if (soft->xb_vcounters==NULL) return ENOMEM;

	soft->isopen = 1;

	/* initialize the flags field of xb_vcounters block used for mmap -
	   figure out which links are alive right now  */

	if ((soft->xb_vcounters == NULL) || (xbow == NULL)) {
		return EINVAL;
		}
	soft->xb_active_count = 0;

        nanotime(&tv);
	soft->xb_prev_sec_value = tv.tv_sec;
	soft->xb_prev_nsec_value = tv.tv_nsec;

	for (i=0; i < MAX_XBOW_PORTS; i++) {
		soft->xb_prev_retry_rcv_value[i] = 0;
		soft->xb_prev_retry_xmt_value[i] = 0;

		soft->xb_vcounters->vcounters[i].flags &= ~XB_VCNTR_LINK_ALIVE;

		if (xbow->xb_link(i).link_status & XB_STAT_LINKALIVE) {
			soft->xb_vcounters->vcounters[i].flags |= XB_VCNTR_LINK_ALIVE;
			soft->xb_active[soft->xb_active_count] = i;
			soft->xb_active_count++;
			soft->xb_prev_retry_rcv_value[i] = 
			   (xbow->xb_link(i).link_aux_status & XB_AUX_STAT_RCV_CNT)>>24;

			soft->xb_prev_retry_xmt_value[i] =
			   (xbow->xb_link(i).link_aux_status & XB_AUX_STAT_XMT_CNT)>>16;
		} 
	}

	return (0);

} /* end : xbmon_open() */


/*
 *	Called when driver is closed.
 */
int
xbmon_close(dev_t dev)
{
	vertex_hdl_t	vhdl;
	xbmon_soft_t   *soft;
	/*REFERENCED*/
	xbow_t	       *xbow;

	vhdl = dev_to_vhdl(dev);
	soft = xbmon_soft_get(vhdl);
	xbow = soft->xbow;

	soft->xb_flags &= ~(XB_FLAG_OPEN);
	if (soft->xb_timeout_id != INVALID_TIMEOUTID) 
		untimeout(soft->xb_timeout_id);

	xb_eventmask = 0X0UL;		/* prevent people from tracing. */

	xb_disable_cntr(soft);
    
	eq_free(&soft->xb_traceq, soft->xb_bufsize);	/* queue support */

	soft->isopen = 0;

	return (0);
} /* end : xbmon_close() */

/*
 *
 */
int
xbmon_read(dev_t dev, uio_t *uiop)
{
	int	error;
	vertex_hdl_t	vhdl;
	xbmon_soft_t   *soft;
	/*REFERENCED*/
	xbow_t	       *xbow;

	vhdl = dev_to_vhdl(dev);
	soft = xbmon_soft_get(vhdl);
	xbow = soft->xbow;

	error = eq_deqmulti(soft, &soft->xb_traceq, uiop);
	return (error);

} /* end : xbmon_read() */

/*
 *
 */
int
xbmon_write(dev_t dev, uio_t *uiop)
{
	xb_event new_event;

	int	error = NO_ERROR;
	timespec_t tv;

	vertex_hdl_t	vhdl;
	xbmon_soft_t   *soft;
	/*REFERENCED*/
	xbow_t	       *xbow;

	vhdl = dev_to_vhdl(dev);
	soft = xbmon_soft_get(vhdl);
	xbow = soft->xbow;

	error = uiomove((caddr_t)&new_event,
			sizeof (new_event),
			UIO_WRITE,
			uiop);
	if (error)
	    return (error); 

	/*nanotime(&(new_event.tv));*/
	nanotime(&tv);
	new_event.sec = tv.tv_sec;
	new_event.nsec = tv.tv_nsec;

	if (eq_enqueue(soft, &soft->xb_traceq, &new_event) == 0) {
	    return (ENOMEM);
	}
	
	return (0);

} /* end : xbmon_write() */

/*
 *
 */
int
xbtrace(caddr_t soft_context, __uint64_t event_id, __uint64_t event_data)
{
	xbmon_soft_t	*soft;
        xb_event	new_event;
	timespec_t	tv;

	soft = (xbmon_soft_t *) soft_context; /* convert context to this xbmon's soft */

	new_event.event_id = event_id;
	new_event.data = event_data;
	nanotime(&tv);
	new_event.sec = tv.tv_sec;
	new_event.nsec = tv.tv_nsec;
	/*nanotime(&(new_event.tv));*/

	eq_enqueue(soft, &soft->xb_traceq, &new_event);

	return (0);
} /* end : xbtrace() */

/*
 * ioctl routine
 */
/*ARGSUSED*/
int
xbmon_ioctl(dev_t dev,
	    int cmd,
	    __uint64_t arg,
	    int mode,
	    struct cred *crp,
	    int *rvp)
{
	__uint32_t	version;
	vertex_hdl_t	vhdl;
	xbmon_soft_t   *soft;
	xbow_t	       *xbow;

	vhdl = dev_to_vhdl(dev);
	soft = xbmon_soft_get(vhdl);
	xbow = soft->xbow;

	switch (cmd) {
	case XB_SET_TIMEOUT_VALUE:
		if (soft->xb_timeout_id != INVALID_TIMEOUTID) {
		    /* sorry, the timer is still running!	*/
		    return (EBUSY);
		}
		soft->xb_timeout_value = arg;
		break;

	case XB_GET_TIMEOUT_VALUE:
		if (arg == NULL) {
		    return (EINVAL);
		} else {
		    if (copyout((caddr_t) & soft->xb_timeout_value, 
				(caddr_t) arg, 
				sizeof(soft->xb_timeout_value)) != 0) {
			    return (EINVAL);
		    }
		}
		break;

	case XB_START_TIMER:
		if (soft->xb_timeout_id != INVALID_TIMEOUTID) {
		    /* The timer is already running!	*/
		    return (EBUSY);
		}

		soft->xb_curlinkidx = 0;
		soft->xb_prev_cntr_a_value = xbow->xb_perf_ctr_a & XBOW_WID_PERF_CTR_MSK;
		soft->xb_prev_cntr_b_value = xbow->xb_perf_ctr_b & XBOW_WID_PERF_CTR_MSK;

		soft->xb_timeout_id = 
			timeout(xb_timeout_rtn, (caddr_t) soft, 
				soft->xb_timeout_value); 
		if (soft->xb_timeout_id == INVALID_TIMEOUTID) {
		    cmn_err(CE_NOTE, 
			    "xb_timeout_rtn(): timeout() failed.\n");
		    return (EIO);
		}
 
		soft->xb_flags |= XB_FLAG_RUNNING;
		break;

	case XB_STOP_TIMER:
		if (soft->xb_timeout_id == INVALID_TIMEOUTID) {
		    /* Sorry, the timer is _NOT_ running!	*/
		    return (EBUSY);
		}
		soft->xb_flags &= ~(XB_FLAG_RUNNING);
		untimeout(soft->xb_timeout_id);

		soft->xb_timeout_id = INVALID_TIMEOUTID;

		break;

	case XB_GET_OVERFLOW_COUNT:
		if (arg == NULL) {
		    return (EINVAL);
		} else {
		    if (copyout((caddr_t)&soft->xb_traceq.eq_overflows,
				(caddr_t)arg, 
				sizeof(soft->xb_traceq.eq_overflows)) != 0) {
			 return (EINVAL);
		    }
		    soft->xb_traceq.eq_overflows = 0;
		}
		break;

	case XB_SET_BUFFER_SIZE:
		if (soft->xb_timeout_id != INVALID_TIMEOUTID) {
		    /* sorry, the timer is still running!	*/
		    return (EBUSY);
		}

		if ((arg <= INVALID_BUFSIZE) ||
		    (arg == NULL) ||
		    (arg > XB_MAX_BUFSIZE)) {
			return (EINVAL); /* ***change to EINVBUFSZ */
		}

		/* free up old soft->xb_traceq */
		eq_free(&soft->xb_traceq, soft->xb_bufsize);
		soft->xb_bufsize = arg;
		if (eq_init(&soft->xb_traceq, soft->xb_bufsize) != 0) {
		    return (ENOMEM);
		}
	
		break;

	case XB_GET_BUFFER_SIZE:
		if (arg == NULL) {
		    return (EINVAL);
		} else {
		    if (copyout((caddr_t) &soft->xb_bufsize, 
				(caddr_t) arg, 
				sizeof(soft->xb_bufsize)) != 0) {
			    return (EINVAL);
		    }
		}
		break;

	case XB_SET_BITSWITCH:
		if (arg == NULL) {
		    return (EINVAL);
		} else {
		    if (copyin((caddr_t) arg,
				(caddr_t) &xb_eventmask,
				sizeof(xb_eventmask)) != 0) {
			return (EINVAL);
		    }
		}
		break;

	case XB_GET_BITSWITCH:
		if (arg == NULL) {
		    return (EINVAL);
		} else {
		    if (copyout((caddr_t) &xb_eventmask, 
				(caddr_t) arg, 
				sizeof (xb_eventmask)) != 0) {
			return (EINVAL);
		    }
		}
		break;

	case XB_SELMODE_CNTR_A:
		if (arg == NULL) {
		    return (EINVAL);
		} else {
		    if (copyin((caddr_t) arg,
				(caddr_t) &soft->xb_modesel_a,
				sizeof(soft->xb_modesel_a)) != 0) {
			return (EINVAL);
		    }
		    soft->xb_multiplex=0;
		    soft->xb_modesel_a.link &= XB_MON_LINK_MASK;
		    soft->xb_modesel_a.mode &= XB_MON_MODE_MASK;
		    soft->xb_modesel_a.level &= XB_MON_LEVEL_MASK;
		    xb_enable_cntr(xbow, 0, &soft->xb_modesel_a);
		}
		break;

	case XB_SELMODE_CNTR_B:
		if (arg == NULL) {
		    return (EINVAL);
		} else {
		    if (copyin((caddr_t) arg,
				(caddr_t) &soft->xb_modesel_b,
				sizeof(soft->xb_modesel_b)) != 0) {
			return (EINVAL);
		    }
		    soft->xb_multiplex=0;
		    soft->xb_modesel_b.link &= XB_MON_LINK_MASK;
		    soft->xb_modesel_b.mode &= XB_MON_MODE_MASK;
		    soft->xb_modesel_b.level &= XB_MON_LEVEL_MASK;
		    xb_enable_cntr(xbow, 1, &soft->xb_modesel_b);
		}
		break;

	case XB_ENABLE_MPX_MODE:
		if (soft->xb_timeout_id != INVALID_TIMEOUTID) {
		    /* Sorry, the timer is _NOT_ running!	*/
		    return (EBUSY);
		}
		soft->xb_multiplex=1;
		soft->xb_curlinkidx = 0;

		/* if there are no active widgets, don't allow multiplex mode */
		if (soft->xb_active_count == 0) {
			return EINVAL;
			}

		soft->xb_modesel_a.mode = XB_MON_MODE_SRC;
		soft->xb_modesel_a.link = soft->xb_active[0]; 
		soft->xb_modesel_b.mode = XB_MON_MODE_DST;
		soft->xb_modesel_b.link = soft->xb_active[1];  /* XXX - used to be 0 */
		xb_enable_cntr(xbow, 0, &soft->xb_modesel_a);
		xb_enable_cntr(xbow, 1, &soft->xb_modesel_b);

		break;

	case XB_GET_VERSION:
		if (arg == NULL) {
		    return (EINVAL);
		} else {
		    version = XB_DRIVER_VERSION;
		    if (copyout((caddr_t) &version, 
				(caddr_t) arg, 
				sizeof(version)) != 0) {
			    return (EINVAL);
		    }
		}
		break;


        case XB_GET_SLOTNUM:
	{
	    __uint32_t widgetNum;      /* arg that comes in */
	    __uint32_t slotNum;        
#if !IP30
	    int xbowNum;	       
	    vertex_hdl_t xswitch_vhdl;  /* xswitch the xbow is attached to */
#endif	/* IP30 */

       	    if (arg == NULL) {
		return (EINVAL);
	    } else if (copyin((caddr_t) arg, (caddr_t) &widgetNum,
			      sizeof(widgetNum)) != 0) {
		return (EINVAL);
	    }
	    if(!XBOW_WIDGET_IS_VALID(widgetNum)) { 
		return (EINVAL);
	    }
#if !IP30
	    xswitch_vhdl = hwgraph_connectpt_get(soft->conn);
	    xbowNum = xswitch_id_get(xswitch_vhdl);
    	    slotNum = get_widget_slotnum(xbowNum, widgetNum);
#else
	    slotNum = (widgetNum == HEART_ID ?
		SLOTNUM_NODE_CLASS : SLOTNUM_XTALK_CLASS) | widgetNum;
#endif	/* IP30 */

	    if (copyout((caddr_t) &slotNum, (caddr_t) arg,
			sizeof(slotNum)) != 0) {
		return (EINVAL);
	    }
	    break;
	}


	default:
		return (EINVAL);			
	} /* switch */

	return (0);

} /* end : xbmon_ioctl() */

/*
 * map routine
 */
/*ARGSUSED*/
int
xbmon_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
        vertex_hdl_t            vhdl;
        xbmon_soft_t            *soft;

        caddr_t                 kaddr;
	int			error;

        vhdl = dev_to_vhdl(dev);
        soft = xbmon_soft_get(vhdl);

	len = ctob(btoc(len));  /* make len page aligned */
	kaddr = (caddr_t)soft->xb_vcounters;

        if (kaddr == NULL)
                return EINVAL;

	error = v_mapphys(vt, kaddr, len);
        return(error);

} /* end : xbmon_map() */

/*
 * unmap routine
 */
/*ARGSUSED*/
int
xbmon_unmap(dev_t dev, vhandl_t *vt)
{
        return(0);

} /* end : xbmon_unmap */

static
void
xb_enable_cntr(xbow_t *xbow, int counter, xb_modeselect_t *modesel)
{
        volatile xbowreg_t *perfreg;
        xbowreg_t ctrlreg;

	switch (counter) {
	    case 0:	perfreg = &(xbow->xb_perf_ctr_a);
			break;
	    case 1:	perfreg = &(xbow->xb_perf_ctr_b);
			break;
	    default:    return;
	}

       *perfreg = (modesel->link & (MAX_XBOW_PORTS-1)) << XBOW_WID_PERF_LINK_SEL_SHFT;

	ctrlreg = xbow->xb_link(modesel->link).link_control;
	ctrlreg &= ~(XB_CTRL_PERF_CTR_MODE_MSK | XB_CTRL_IBUF_LEVEL_MSK);
	ctrlreg |= xb_perf_modes[(modesel->mode) & 3];
	if (modesel->mode == XB_MON_MODE_LEVEL) {
		ctrlreg |= modesel->level << XB_CTRL_IBUF_LEVEL_SHFT;
	}
	xbow->xb_link(modesel->link).link_control = ctrlreg;
	

} /* end : xb_enable_cntr */

static
void
xb_disable_cntr(xbmon_soft_t *soft)
{
	xbow_t		*xbow;
        xbowreg_t	 ctrlreg;
	int		 i;

	xbow = soft->xbow;

	if (soft->xb_active_count <= 0) return;

	for (i=0; i < soft->xb_active_count; i++) {
		ctrlreg = xbow->xb_link(soft->xb_active[i]).link_control;
		ctrlreg &= ~(XB_CTRL_PERF_CTR_MODE_MSK | XB_CTRL_IBUF_LEVEL_MSK);
		ctrlreg |= xb_perf_modes[0 & 3];
/* XXX - try something like: 
	*	ctrlreg |= XB_MON_MODE_NONE; 
	*/

		xbow->xb_link(soft->xb_active[i]).link_control = ctrlreg;
	}	

} /* end : xb_disable_cntr */

static
void
xb_timeout_rtn(caddr_t arg)
{
	xbmon_soft_t 	*soft;
	xbow_t 		*xbow;
	__int32_t	delta;
	__int32_t	delta_xmt, delta_rcv;
	__int64_t	delta_sec, delta_nsec;
	xbowreg_t	cur_value_a, cur_value_b;
	xbowreg_t       cur_cnt_retry_xmt, cur_cnt_retry_rcv;
	int		this_link;	/* link_monitored */

	xb_event       	new_event;
	timespec_t 	tv;
	xb_vcntr_block_t *blk;
	xb_vcounter_t	 *vc;

	soft = (xbmon_soft_t *)arg;
	xbow = soft->xbow;
	
	/* could this be a race condition!?	*/
	if (!(soft->xb_flags & XB_FLAG_OPEN))
	    return;

	nanotime(&tv);

	delta_nsec = tv.tv_nsec - soft->xb_prev_nsec_value;
	delta_sec = tv.tv_sec - soft->xb_prev_sec_value;
	if (delta_nsec < 0) {
		delta_nsec = 1000000000 + delta_nsec;
		delta_sec = delta_sec - 1;
	}
	new_event.sec = tv.tv_sec;
	new_event.nsec = tv.tv_nsec;
	/*nanotime(&new_event.tv);*/    /* get timestamp */
	cur_value_a = (xbow->xb_perf_ctr_a & XBOW_WID_PERF_CTR_MSK);
	cur_value_b = (xbow->xb_perf_ctr_b & XBOW_WID_PERF_CTR_MSK);

	blk = soft->xb_vcounters;


	/* SRC link : transmits */
	if (soft->xb_modesel_a.mode != 0){   /* monitoring is on */

		this_link = soft->xb_modesel_a.link;

		new_event.event_id = ((__uint64_t)0x80 << 56) |
			     ((__uint64_t)0xA << 16) |
			     ((__uint64_t)soft->xb_modesel_a.mode << 8) |
			     ((__uint64_t)soft->xb_modesel_a.link);

		delta = (__int32_t) (cur_value_a - soft->xb_prev_cntr_a_value);
		soft->xb_prev_cntr_a_value = cur_value_a;

		if (delta < 0){
			delta = -delta;
		}
	
		new_event.data = (__uint64_t)delta;

		cur_cnt_retry_xmt =
		(xbow->xb_link(this_link).link_aux_status & XB_AUX_STAT_XMT_CNT) >> 16;

		delta_xmt = (__int32_t) (cur_cnt_retry_xmt - soft->xb_prev_retry_xmt_value[this_link]);
		if (delta_xmt < 0) delta_xmt += 256;

		soft->xb_prev_retry_xmt_value[this_link] = cur_cnt_retry_xmt;

		vc = &(blk->vcounters[soft->xb_modesel_a.link]);
		vc->flags = 0x01;
		vc->vsrc += (__uint64_t)delta;
		vc->tsrc.nsec = delta_nsec;
		vc->tsrc.sec = delta_sec;
		vc->cxmt += (__uint64_t)delta_xmt;

		/* queue support */
		(void)eq_enqueue(soft, &soft->xb_traceq, &new_event);	
	}

	/* DST link : receives */
	if (soft->xb_modesel_b.mode != 0){   /* monitoring is on */

		this_link = (soft->xb_modesel_b.link);

		new_event.event_id = ((__uint64_t)0x80 << 56) |
			     ((__uint64_t)0xB << 16) |
			     ((__uint64_t)soft->xb_modesel_b.mode << 8) |
			     ((__uint64_t)soft->xb_modesel_b.link);

                delta = (__int32_t) (cur_value_b - soft->xb_prev_cntr_b_value);
                soft->xb_prev_cntr_b_value = cur_value_b;

		if (delta < 0){
			delta = -delta;
		}
	
		new_event.data = (__uint64_t)delta; 

		cur_cnt_retry_rcv =
		(xbow->xb_link(this_link).link_aux_status & XB_AUX_STAT_RCV_CNT) >> 24;

		delta_rcv = (__int32_t) (cur_cnt_retry_rcv - soft->xb_prev_retry_rcv_value[this_link]);
		if (delta_rcv < 0) delta_rcv += 256;

		soft->xb_prev_retry_rcv_value[this_link] = cur_cnt_retry_rcv;

		vc = &(blk->vcounters[soft->xb_modesel_b.link]);
		vc->vdst += (__uint64_t)delta;
		vc->tdst.nsec = delta_nsec;
		vc->tdst.sec = delta_sec;
		vc->crcv += (__uint64_t)delta_rcv;

		/* queue support */
		(void)eq_enqueue(soft, &soft->xb_traceq, &new_event);	
	}

	/* For MPX mode only, determine which port to monitor next */
	if (soft->xb_multiplex==1 && soft->xb_active_count>0) {
		soft->xb_curlinkidx++;
		if (soft->xb_curlinkidx >= soft->xb_active_count) {
			soft->xb_curlinkidx = 0;
			}
		soft->xb_modesel_a.link = soft->xb_active[soft->xb_curlinkidx];
		soft->xb_modesel_b.link = 
			soft->xb_active[((soft->xb_curlinkidx+1)%soft->xb_active_count)];

		xb_enable_cntr(xbow, 0, &soft->xb_modesel_a);
		xb_enable_cntr(xbow, 1, &soft->xb_modesel_b);

		soft->xb_prev_cntr_a_value = (xbow->xb_perf_ctr_a & XBOW_WID_PERF_CTR_MSK);
		soft->xb_prev_cntr_b_value = (xbow->xb_perf_ctr_b & XBOW_WID_PERF_CTR_MSK);

        }

	soft->xb_timeout_id = 
		timeout(xb_timeout_rtn, arg, soft->xb_timeout_value); 
	if (soft->xb_timeout_id == INVALID_TIMEOUTID) {
	    cmn_err(CE_NOTE, "xb_timeout_rtn(): timeout() failed.\n");
	}

} /* end : xb_timeout_rtn() */
