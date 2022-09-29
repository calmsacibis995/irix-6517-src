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

#define	_CL_DEBUG	0

#include "sys/debug.h"
#include "sys/types.h"
#include "sys/uio.h"
#include "sys/errno.h"
#include "sys/poll.h"
#include "sys/ddi.h"
#include "sys/cmn_err.h"
#include "sys/graph.h"
#include "sys/hwgraph.h"
#include "sys/iograph.h"
#include "sys/kmem.h"
#include "sys/immu.h"
#include "sys/alenlist.h"
#include "sys/lio.h"
#include "sys/poll.h"
#include "sys/pfdat.h"
#include "sys/partition.h"

#include "ksys/xpc.h"
#include "ksys/sthread.h"
#include "ksys/partition.h"

#include "sys/SN/SN0/arch.h"

#define	CL_MESSAGE_SIZE		256	/* ring entry size */
#define	CL_MESSAGE_COUNT	NBPP/CL_MESSAGE_SIZE	/* 64 ring entries */

/*
 * Macro: 	CL_HEX
 * Purpose: 	Convert # into 2 character hex representation.
 * Parameters:	_x - # to convert
 *		_s - string with at least 3 characters to fill in, null
 *		     terminated.
 */
static		char _hex[] = "0123456789abcdef";
#define	CL_HEX(_x, _s)	(_s)[0] = _hex[((_x) >> 4)]; \
                        (_s)[1] = _hex[((_x) & 0xf)];\
                        (_s)[2] = '\0'

/*
 * Defines:	CL_XPC_CHAN_XXXX
 * Purpose:	Defines the XPC channel number assigned to each of the 
 *		devices.
 * Notes: 	Devices are assigned as follows:
 *
 *		PART  - partition administration device, used by mkpart. 
 *		CLSHM - shared memory administration device. 
 *		RAW   - raw devices
 */
#define	CL_XPC_CHAN_PART	(XP_RAW_BASE)
#define	CL_XPC_CHAN_CLSHM	(CL_XPC_CHAN_PART+1)
#define	CL_XPC_CHAN_RAW		(CL_XPC_CHAN_CLSHM+1)
#define	CL_XPC_CHAN_LIMIT	(XP_RAW_BASE+XP_RAW_RINGS)

/*
 * Defines: 	CL_VCHAN/CL_RCHAN
 * Purpose: 	Normalizes channel numbers to allow indexing into 
 *		channel array. VCHAN in the nomralized CL channel, 
 *		starting at 0, RCHAN starts raw device naming at 0.
 */
#define	CL_VCHAN(_c)		((_c) - XP_RAW_BASE)
#define CL_RCHAN(_c)		((_c) - CL_XPC_CHAN_RAW)

#if	_CL_DEBUG
#   define	DPRINTF(_x)		cl_debug _x
#else
#   define	DPRINTF(_x)
#endif

typedef	struct ior_s {
    struct ior_s *io_next;		/* forward pointer */
    uio_t	*io_uio;		/* uio for operation */
    alenlist_t	io_al;			/* addrelen list for operation */
    int		io_rw;			/* B_READ/B_WRITE flags */
#   define	IO_FAST_ACC	4	/* Fast usracc cookies kept */
    int		io_fastacc[IO_FAST_ACC];
}  ior_t;

typedef struct ioq_s {
    void	*ioq_thread;		/* thread associated with queue */
    xpc_t	(*ioq_io)(xpchan_t, alenlist_t, ssize_t *);
    void 	(*ioq_callback)(uio_t *, int);	/* callback function */
    ior_t	*ioq_ior;		/* I/O requests */
    ior_t	*ioq_ior_tail;		/* I/O request tail */
    sema_t	ioq_sema;		/* semaphore we wait on */
} ioq_t;

typedef struct cl_s {
    mutex_t	cl_lock;
    enum {
	clClosed, 
	clOpen,
	clError
    }		cl_state;
    __uint32_t	cl_channel;		/* channel # */
    partid_t	cl_partid;		/* partition ID */
    int		cl_errno;		/* Errno if error */
    int		cl_ms;			/* message size */
    int		cl_mc;			/* message count */
    int		cl_idata;		/* fast data size */
    xpchan_t	cl_cid;			/* channel ID */
    ioq_t	cl_rq;			/* Async read queue */
    ioq_t	cl_wq;			/* Async write queue */
    sema_t	cl_rd_sema;		/* Sema for cl reads */
    sema_t	cl_wr_sema;		/* Sema for cl writes */
    xpm_t	*cl_xpm;		/* Active xpm */
    xpmb_t	*cl_xpmb;		/* Activate xpmb */
    __uint32_t	cl_cnt;			/* # XPMB's */
} cl_t;	

#define	CL_LOCK(_cl)	mutex_lock(&(_cl)->cl_lock, 0)
#define	CL_UNLOCK(_cl)	mutex_unlock(&(_cl)->cl_lock)

#define	CL_RLOCK(_cl)	(psema(&(_cl)->cl_rd_sema, PWAIT|PCATCH) \
			 ? EINTR : 0)
#define	CL_RUNLOCK(_cl)	vsema(&(_cl)->cl_rd_sema)

#define	CL_WLOCK(_cl)	(psema(&(_cl)->cl_wr_sema, PWAIT|PCATCH) \
			 ? EINTR : 0)
#define	CL_WUNLOCK(_cl)	vsema(&(_cl)->cl_wr_sema)

#define	NEW(_x)		(_x) = kmem_alloc(sizeof(*(_x)), 0)
#define	FREE(_x)      	kmem_free((_x), sizeof(*(_x)))

cl_t	*cl_devs[MAX_PARTITIONS][XP_RAW_RINGS];

int		cl_devflag = D_MP;

#define	CL_XERRNO(_r)	xpc_errno[_r]
#define	CL_RETURN(_r)	return(CL_XERRNO(_r))

        void	cl_init(void);
static	int	cl_create_alenlist(ior_t *io);
static	void	cl_destroy_alenlist(ior_t *io);
        int	cl_read(dev_t dev, uio_t *uio);
static	void	cl_async_thread(cl_t *cl, ioq_t *ioq);
static	void	cl_queue_asyncio(cl_t *cl, ioq_t *ioq, ior_t *ior);
static	int	cl_ior_init(uio_t *uio, int rw, ior_t *io);
static	void	cl_ior_done(ior_t *io);
        int	cl_write(dev_t dev, uio_t *uio);
        int	cl_poll(dev_t dev, short events, int anyyet, short *reventsp, 
			struct pollhead **phpp, unsigned int *);
        int 	cl_open(dev_t *dev, int flag);
        int	cl_close(dev_t dev);

#if _CL_DEBUG

/*VARARGS*/
static	void
cl_debug(char *s, ...)
/*
 * Function: 	cl_debug
 * Purpose:	To print debug inforamtion on the console.
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
#endif

static	void
cl_create_device(const partid_t p, __uint32_t chan, __uint32_t vchan,
		 const char *s1, const char *ds)
/*
 * Function: 	cl_create_device
 * Purpose:	Add a new cl device to the device graph and init 
 *		the associated cl data structure.
 * Parameters:	p - partition remote end of device associated with.
 *		channel - channel #
 *		path - path to devicde
 *		cl - pointer to cl structure for device.
 * Returns:	Nothing
 */
{
    cl_t		*cl;
    graph_error_t	ge;
    char		name[MAXPATHLEN];
    dev_t		td;
    char		ps[3];		/* partition string */

    CL_HEX(p, ps);			/* Partition # */
    sprintf(name, "%s/%s/%s/%s", EDGE_LBL_XPLINK, s1, ps, ds);

    ge = hwgraph_char_device_add(hwgraph_root, name, "cl_", &td);
    if (GRAPH_SUCCESS != ge) {
	cmn_err(CE_WARN, 
		"cl_part_ativate: failed to add raw device: %s: %d\n",
		name, ge);
	return;
    }

    if (NULL == cl_devs[p][vchan]) {
	/* Only do this if it has not been added before. */
	cl = kmem_alloc(sizeof(cl_t), 0);
	cl->cl_state   = clClosed;
	cl->cl_channel = chan;
	cl->cl_partid  = p;
	cl->cl_ms      = CL_MESSAGE_SIZE;
	cl->cl_mc      = CL_MESSAGE_COUNT;
	cl->cl_idata   = XPM_IDATA_LIMIT(CL_MESSAGE_SIZE, 1);
	cl->cl_cid     = NULL;
	cl->cl_xpm     = NULL;
	cl->cl_xpmb    = NULL;
	bzero(&cl->cl_rq, sizeof(cl->cl_rq));
	bzero(&cl->cl_wq, sizeof(cl->cl_wq));
	init_mutex(&cl->cl_lock, MUTEX_DEFAULT, "cl", vchan);
	init_sema(&cl->cl_rd_sema, 1, "cl", vchan);
	init_sema(&cl->cl_wr_sema, 1, "cl", vchan);
	cl_devs[p][vchan] = cl;
	device_info_set(td, cl);
    }
}

void
cl_part_activate(partid_t p)
/*
 * Function: 	cl_part_activate
 * Purpose:	Called whan a partition is activated.
 * Parameters:	p - partition # that is being activated. 
 * Returns:	Nothing.
 */
{
    __uint32_t	c;
    char	ds[3];			/* device string */
    xpc_t	xr;

    DPRINTF(("cl_part_activate: activating partition: %d\n", p));

    xr = xpc_avail(p);
    if (xpcSuccess != xr) {
	DPRINTF(("cl_part_activate: xpc_avail(%d) = %d (%s)\n", 
		 p, xr, xpc_decode[xr]));
	return;
    }

    /* Create partition management device */

    cl_create_device(p, CL_XPC_CHAN_PART, CL_VCHAN(CL_XPC_CHAN_PART), 
		     EDGE_LBL_XPLINK_ADMIN, "partition");
    cl_create_device(p, CL_XPC_CHAN_CLSHM, CL_VCHAN(CL_XPC_CHAN_CLSHM), 
		     EDGE_LBL_XPLINK_ADMIN, "clshm");

    for (c = CL_XPC_CHAN_RAW; c < CL_XPC_CHAN_LIMIT; c++) {
	/* create raw devices */
	CL_HEX(CL_RCHAN(c), ds);
	cl_create_device(p, c, CL_VCHAN(c), EDGE_LBL_XPLINK_RAW, ds);
    }
}

#if 0
static	void
cl_part_deactivate(partid_t p)
/*
 * Function: 	cl_part_deacivate
 * Purpose:	Called whan a partition is deactivated
 * Parameters:	p - partition being deactivated
 * Returns:	Nothing.
 */
{
    cmn_err(CE_PANIC, "cl_part_deactivate: %d\n", p);
}
#endif
void
cl_init(void)
/*
 * Function: 	cl_init
 * Purpose:	Initialization entry point for cl driver.
 * Parameters:	None
 * Returns:	Nothing
 */
{
    if (INVALID_PARTID == part_id()) {
	return;
    }

    DPRINTF(("cl_init: registering handlers\n"));

    /* 
     * Register with partition support for notification of partition
     * changes.
     */

    part_register_handlers(cl_part_activate, NULL);
}

static	int
cl_create_alenlist(ior_t *io)
/*
 * Function: 	cl_create_alenlist
 * Purpose:	Generate physical address/length pairs for the requested
 *		I/O operation.
 * Parameters:	lr - pointer to link request structure, following fields
 *		must be filled in: lr_sema< lr_uio.
 * Returns:	0 for success, errno otherwise.
 * Notes:	If 0 returned, lnDestrorAlenList must be called to 
 *		free up the alen list. It is assumed that uio structs 
 *		marked as kernel async io are already pinned.
 *	
 */
{
    register uio_t 	*uio = io->io_uio;
    register iovec_t	*iov;
    register int	i;
    int			errno;

    io->io_al = NULL;

    iov = uio->uio_iov;
    for (i=0; i < uio->uio_iovcnt; i++){
	if (uio->uio_pio != KAIO) {
	    errno = 
		fast_useracc(iov->iov_base, iov->iov_len, io->io_rw, 
			     i < IO_FAST_ACC ? &io->io_fastacc[i] : NULL);
	    if (0 != errno) {
		break;
	    }
	}
	io->io_al = uvaddr_to_alenlist(io->io_al, uio->uio_iov[i].iov_base, 
				       uio->uio_iov[i].iov_len, 0);
	iov++;
    }
    if (0 != errno) {
	if (uio->uio_pio != KAIO) {
	    for (iov = uio->uio_iov; i > 0; i--, iov++) {
		fast_unuseracc(iov->iov_base, iov->iov_len, io->io_rw,
			       i < IO_FAST_ACC ? &io->io_fastacc[i]:NULL);
	    }
	}
	alenlist_done(io->io_al);
    }
    return(errno);
}

static	void
cl_destroy_alenlist(ior_t *io)
/*
 * Function: 	cl_destroy_alenlist
 * Purpose:	Free up an address length list and unpin pages we previously
 *		pinned. 
 * Parameters:	lr - pointer to link resource.
 * Returns:	nothing.
 * Notes:	Should only be called on lr's that have previously had
 *		clCreateAlenList called on them.
 */
{
    register  uio_t   	*uio = io->io_uio;
    register iovec_t 	*iov;
    register int	i;

    if (io->io_al) {
	if (uio->uio_pio != KAIO) {
	    iov = uio->uio_iov;
	    for (i=0; i < uio->uio_iovcnt; i++) {
		fast_unuseracc(iov->iov_base, iov->iov_len, io->io_rw, 
			       i < IO_FAST_ACC ? &io->io_fastacc[i]:NULL);
		iov++;
	    }
	}
	alenlist_done(io->io_al);
    }
}

int
cl_read(dev_t dev, uio_t *uio)
/*
 * Function: 	clread
 * Purpose:	User entry point for reading from a cross partition device.
 * Parameters:	dev - device to read from
 *		uio - pointer to user I/O vector.
 * Returns:	0 for success, errno otherwise.
 */
{
    int		errno;			/* return value/wait result */
    cl_t 	*cl;
    ior_t	io;
    xpc_t	xr;

    cl = (cl_t *)device_info_get(dev);
    ASSERT(NULL != cl);

    if(errno = CL_RLOCK(cl)) {
	return(errno);
    }

    if (cl->cl_state == clError) {
	CL_RUNLOCK(cl);
	return(cl->cl_errno);
    }

    /*
     * If the request is small, attempt to do a "fast" I/O. If anything 
     * fails, then attempt to go the slow way, and handle any error
     * that way.
     */
    
    if ((uio->uio_pio != KAIO) && (uio->uio_resid < cl->cl_idata)) {
	if (!cl->cl_xpm) {
	    xr = xpc_receive(cl->cl_cid, &cl->cl_xpm);
	    if (xpcSuccess == xr) {
		ASSERT(cl->cl_xpm);
		cl->cl_xpmb = (xpmb_t *)(cl->cl_xpm + 1);
		cl->cl_cnt  = cl->cl_xpm->xpm_cnt;
	    } else {
		ASSERT(!cl->cl_xpm);
	    }
	} else {
	    xr = xpcSuccess;
	}
	    
	if (xpcSuccess == xr) {
	    ASSERT(cl->cl_xpm);
	    if ((cl->cl_cnt == 1) && (cl->cl_xpmb->xpmb_flags & XPMB_F_IMD)
		&& (cl->cl_xpmb->xpmb_cnt == uio->uio_resid)) {
		errno = uiomove((void *)((__psunsigned_t)cl->cl_xpmb + 
					 cl->cl_xpmb->xpmb_ptr), 
				cl->cl_xpmb->xpmb_cnt, UIO_READ, uio);
		if (!errno) {
		    xpc_done(cl->cl_cid, cl->cl_xpm);
		    cl->cl_xpm = NULL;
		}
		CL_RUNLOCK(cl);
		return(errno);
	    }
	}
    }    

    errno = cl_ior_init(uio, B_READ, &io);
    if (errno) {
	CL_RUNLOCK(cl);
	return(errno);
    }
    if (uio->uio_pio == KAIO) {	/* kernel async I/O */
	CL_RUNLOCK(cl);
	cl_queue_asyncio(cl, &cl->cl_rq, &io);
	return(0);
    } 

    /*
     * Hmmmm - at this point, lets do the brute force approach.
     */
    while (uio->uio_resid > 0) {
	if (!cl->cl_xpm) {
	    xr = xpc_receive(cl->cl_cid, &cl->cl_xpm);
	    if (xr) {			/* while */
		ASSERT(!cl->cl_xpm);
		break;
	    } else {
		ASSERT(cl->cl_xpm);
		cl->cl_xpmb = (xpmb_t *)(cl->cl_xpm + 1);
		cl->cl_cnt  = cl->cl_xpm->xpm_cnt;
	    }
	}
	xr = xpc_receive_xpmb(cl->cl_xpmb, &cl->cl_cnt, &cl->cl_xpmb, 
			      io.io_al, &uio->uio_resid);
	if (xr) {
	    break;			/* while */
	} else if (!cl->cl_cnt) {	/* done with this message */
	    xpc_done(cl->cl_cid, cl->cl_xpm);
	    cl->cl_xpm  = NULL;
	}
    }
    CL_RUNLOCK(cl);
    cl_ior_done(&io);
    CL_RETURN(xr);
}

static	void
cl_async_thread(cl_t *cl, ioq_t *ioq)
/*
 * Function: 	cl_async_thread
 * Purpose:	Provides a context to perform ASYNC I/O functions.
 * Parameters:	cl - pointer to cl device to operate on.
 *		ioq - pointer to ioq to operate on.
 * Returns:	Does not return.
 */
{
    ior_t	*io;
    xpc_t	xr;
#   define	KAIO_CALLBACK(_u, _e) \
        ((void (*)(uio_t *, int))(_u)->uio_pbuf)((_u), (_e))

    while (1) {
	do {
	    CL_LOCK(cl);
	    io = ioq->ioq_ior;
	    if (io) {			/* pick off our work */
		ioq->ioq_ior = io->io_next;
		CL_UNLOCK(cl);
		xr = ioq->ioq_io(cl->cl_cid, io->io_al, 
				 &io->io_uio->uio_resid);
		KAIO_CALLBACK(io->io_uio, CL_XERRNO(xr));
		FREE(io);
	    } else {
		CL_UNLOCK(cl);
	    }
	} while (io);
	(void)psema(&ioq->ioq_sema, PZERO);
    }
}

static	void
cl_queue_asyncio(cl_t *cl, ioq_t *ioq, ior_t *ior)
/*
 * Function: 	cl_queue_asyncio
 * Purpose:	To queue and async I/O operation to be performed "sometime".
 * Parameters:	cl - link the operation is associated with.
 *		lrq - queue the operation is to be queued on.
 *		lr - link resource to queue.
 * Returns:	Nothing
 */
{
    ior_t	*io;

    NEW(io);
    *io = *ior;

    CL_LOCK(cl);
    if (NULL == ioq->ioq_thread) {
	sthread_create("cl_async", NULL, 0, 0, 0, 0, 
		       (st_func_t *)cl_async_thread, (void *)cl, 
		       (void *)ioq, 0, 0);
    }
    io->io_next = ioq->ioq_ior_tail;
    if (ioq->ioq_ior_tail) {
	ioq->ioq_ior_tail->io_next = io;
    } else {
	ioq->ioq_ior = ioq->ioq_ior_tail = io;
    }
    CL_UNLOCK(cl);
    vsema(&ioq->ioq_sema);		/* Wake up Mr. thread */
}

static	int
cl_ior_init(uio_t *uio, int rw, ior_t *io)
/*
 * Function: 	cl_init_ior
 * Purpose:	To allocate a link resource used to perform a read or a 
 *		write operation, and pin user pages.
 * Parameters:	uio - user uio struct
 *		rw - B_READ or B_WRITE
 *		lrp - address of lr pointer filled in on return.
 * Returns:	0 - lr allocated and filled in
 *		errno - lr returned is null.
 */
{
    int		errno;

    io->io_rw 	= rw;
    io->io_uio  = uio;
    io->io_al   = NULL;
    io->io_next = NULL;
    
    errno = cl_create_alenlist(io);

    return(errno);
}

static	void
cl_ior_done(ior_t *io)
/*
 * Function: 	cl_free_XX
 * Purpose:	To free a link read/write resource, and free alen list
 *		and upin pages for non-async I/O.
 * Parameters:	io  - pointer to io request.
 * Returns:	Nothing
 */
{	
    if (io->io_uio->uio_pio != KAIO) {
 	cl_destroy_alenlist(io);
    }
}

   
int
cl_write(dev_t dev, uio_t *uio)
/*
 * Function: 	cl_write
 * Purpose:	System call interface to send data on raw cl0net device
 * Parameters:	dev - device wo send data on
 *		uio - user io list describing data to send.
 * Returns:	errno
 */
{	
    xpm_t	*xpm;
    xpc_t	xr = xpcSuccess;
    cl_t	*cl;
    int		errno;			/* return value/wait result */
    ior_t	io;
    xpmb_t	*xpmb, *xpmb_end;
    alenaddr_t	alp;
    size_t	l, all;
    int		al_result;

    cl = (cl_t *)device_info_get(dev);

    if (cl->cl_state == clError) {
	return(cl->cl_errno);
    }

    /* Fast track small writes */
    if (uio->uio_resid < cl->cl_idata) {
	xr = xpc_allocate(cl->cl_cid, &xpm);
	if (xpcSuccess != xr) {
	    return(CL_XERRNO(xr));
	}
	ASSERT(xpm);
	/* 
	 * If the allocation failed, just let the long way worry about
	 * it, this "fast" path should be as simple as possible.
	 */
	xpm->xpm_type 	  = XPM_T_DATA;
	xpm->xpm_cnt	  = 1;
	xpmb = (xpmb_t *)xpm + 1;
	xpmb->xpmb_cnt   = uio->uio_resid;
	xpmb->xpmb_ptr   = sizeof(*xpmb);
	xpmb->xpmb_flags = XPMB_F_IMD;
	errno = uiomove((void *)(xpmb + 1), cl->cl_idata, UIO_WRITE, uio);
	if (errno) {
	    xpm->xpm_cnt  = 0;
	    xpm->xpm_tcnt = 0;
	    (void)xpc_send(cl->cl_cid, xpm);
	    return(errno);
	}
	xr = xpc_send(cl->cl_cid, xpm);
	return(CL_XERRNO(xr));
    }

    if (errno = cl_ior_init(uio, B_WRITE, &io)) {
	return(errno);
    }

    if (uio->uio_pio == KAIO) {
	cl_queue_asyncio(cl, &cl->cl_wq, &io);
    } else {
	if(errno = CL_WLOCK(cl)) {
	    return(errno);
	}
	l = uio->uio_resid;
	while (l > 0) {
	    if (xpcSuccess != (xr = xpc_allocate(cl->cl_cid, &xpm))) {
		break;
	    }
	    xpm->xpm_type = XPM_T_DATA;
	    xpm->xpm_cnt  = 0;
	    xpmb     = (xpmb_t *)(xpm + 1);
	    xpmb_end = xpmb + ((cl->cl_ms - sizeof(xpm)) / sizeof(xpmb_t));
	    
	    while ((l > 0) && (xpmb < xpmb_end)) {
		al_result = alenlist_get(io.io_al, NULL, 0, &alp, &all, 0);
		if (ALENLIST_SUCCESS != al_result) {
		    /* Need if to keep compiler happy on non-debug kernels */
		    ASSERT(al_result == ALENLIST_SUCCESS);
		}
		ASSERT(l >= all);
		xpmb->xpmb_flags = XPMB_F_PTR;
		xpmb->xpmb_cnt   = all;
		xpmb->xpmb_ptr   = alp;
		xpmb++;
		l -= all;
		xpm->xpm_cnt  += 1;
		xpm->xpm_tcnt += all;
	    }

	    if (l == 0) {
		xpm->xpm_flags |= XPM_F_SYNC;
	    }
	    
	    if (xpcSuccess != (xr = xpc_send(cl->cl_cid, xpm))) {
		break;
	    }
	}
	uio->uio_resid = l;
	cl_ior_done(&io);
	errno = CL_XERRNO(xr);
        CL_WUNLOCK(cl);
    }
    return(errno);
}

int
cl_poll(dev_t dev, short events, int anyyet, short *reventsp, 
	struct pollhead **phpp, unsigned int *genp)
/*
 * Function: 	cl_poll
 * Purpose:	Provide user level select/poll ability
 * Parameters:
 * Returns:
 */
{
    cl_t	*cl;

    cl = (cl_t *)device_info_get(dev);

    CL_RETURN(xpc_poll(cl->cl_cid, events, anyyet, reventsp, phpp, genp));
}

/* ARGSUSED */
int
cl_open(dev_t *dev, int flag)
/*
 * Function: 	cl_open
 * Purpose:	System call interface to "write" on a cl0 ring.
 * Parameters:	dev - pointer to device
 *		flag - 
 * Returns:	0 on success, errno otherwise.
 */
{
    xpc_t	xr;
    cl_t	*cl;
    int		errno;
    int		xpc_flags = 0;

    cl = (cl_t *)device_info_get(*dev); /* Find our ring */
    if (NULL == cl) {
	return(ENODEV);
    }

    xpc_flags  = XPC_CONNECT_BWAIT;
    xpc_flags |= (flag & O_NDELAY) ? XPC_CONNECT_AOPEN : 0;
    xpc_flags |= (flag & O_NONBLOCK) ? XPC_CONNECT_NONBLOCK : 0;


    CL_LOCK(cl);
    switch(cl->cl_state) {
    case clOpen:
	errno = 0;
	break;
    case clClosed:
	xr = xpc_connect(cl->cl_partid, cl->cl_channel, cl->cl_ms, 
			 cl->cl_mc, NULL, xpc_flags, 0, &cl->cl_cid);
	errno = CL_XERRNO(xr);
	if (!errno) {
	    ASSERT(NULL != cl->cl_cid);
	    cl->cl_state = clOpen;
	}
	break;
    case clError:
	errno = cl->cl_errno;
	break;
    default:
	errno = EPROTO;
	break;
    }
    CL_UNLOCK(cl);
    return(errno);
}    

int
cl_close(dev_t dev)
/*
 * Function: 	cl_close
 * Purpose:	System call interface to close a raw device.
 * Parameters:	dev - device to close.
 * Returns:	0
 */
{
    cl_t	*cl;

    cl = (cl_t *)device_info_get(dev);
    ASSERT(NULL != cl);

    /*
     * Zap all anync I/O operations etc. 
     */
    CL_LOCK(cl);
    ASSERT(NULL != cl->cl_cid);
    ASSERT(cl->cl_state == clOpen);
    if (cl->cl_xpm) {
        /* Finish up any pending message */
	xpc_done(cl->cl_cid, cl->cl_xpm);
    }
    cl->cl_xpm  = NULL;
    cl->cl_xpmb = NULL;
    cl->cl_cnt  = 0;
    xpc_disconnect(cl->cl_cid);
    cl->cl_state = clClosed;
    cl->cl_cid = 0;
    CL_UNLOCK(cl);
    return(0);
}
