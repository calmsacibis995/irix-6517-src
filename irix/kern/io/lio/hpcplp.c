#ident	"io/hpcplp.c:  $Revision: 1.40 $"

#if defined(IP20)
/* 
 * hpcplp.c - driver for the IP20 parallel port
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <ksys/vfile.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/immu.h>
#include <sys/buf.h>
#include <sys/uio.h>

#include <sys/ioctl.h>
#include <sys/debug.h>
#include <sys/mload.h>

#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/hpcplpreg.h>
#include <sys/plp.h>

#include <sys/ddi.h>

int plpdevflag = 0;

char *plpmversion = M_VERSION;

extern int cachewrback;

#ifdef DEBUG
int plp_debug = 1;
#define dprintf if (plp_debug) printf
void plp_show_dl(memd_t *);
#endif

/* size of space for memory descriptor nodes, add one in case one of
 * the descriptors crosses a page boundary and needs to be thrown away.
 */
#define MDMEMSIZE	((NMD + 1) * sizeof(memd_t))

/* list of all free memory descriptors (K0 addresses) 
 */
static memd_t *free_mds = 0; 

/* memory for receive buffers and memory descriptors */
static caddr_t rbufmem;
static caddr_t mdmem;
#ifdef IP20
static caddr_t lmembufs[NXMD];		/* ptrs to low memory for xmit bufs */
#endif
static int plpinitfail;

#define LOCK()	spl5();
#define UNLOCK(a)  splx(a);

/* insert/remove node _n_ into/from list _l_
 */
#define PUTNODE(l,n) (n)->memd_forw = (l); (l) = (n);
#define GETNODE(l,n) (n) = (l); if ((l) != (memd_t *)0) (l) = (l)->memd_forw;
#define LPUTNODE(l,n) {int s; s=LOCK(); PUTNODE(l,n); UNLOCK(s);}
#define LGETNODE(l,n) {int s; s=LOCK(); GETNODE(l,n); UNLOCK(s);}

/* struct defining state of parallel port
 * 	- rlock and wlock protect r/w loops so data on a single r/w 
 *		call is contiguous
 *	- dlock protects the dma channel (regs, dl, bp)
 */

typedef struct plp_s {
    unsigned char state;	/* software state of port */
#define PLP_ALIVE	0x1	/* parallel port exists */
#define PLP_WROPEN	0x2	/* open for writing */
#define PLP_RDOPEN	0x4	/* open for reading */
#define PLP_NBIO	0x8	/* set for non-blocking reads */
#define PLP_INUSE	0x10	/* reference flag for unloads */
#define PLP_OCCUPIED	0x20	/* process is busy */

    unsigned int control;	/* value to use for strobe/control register */
    int rlock, wlock, dlock;	/* read, write, and dma locks */
#define PLP_BUSY	0x1
#define PLP_WANTED	0x2

    plp_regs_t *regs;		/* address of HPC registers for port */
    memd_t *dl;			/* descriptor list given to HPC: PHYS addr */
    struct buf *bp;		/* current buf for write */
    memd_t *rbufs;		/* list of buffers of data that have 
				 * been read on this channel.
				 * pointers are K0 addrs, buffers are K1 */
    int rto, wto;		/* read/write timeout in secs */
    int timeoutid;		/* read/write timeout */
} plp_t;

plp_t plpport[NPLP];

extern int plpprobe[];		/* from master.d/hpcplp */

static int plpio (struct buf *);
struct eframe_s;
static void plpintr (int, struct eframe_s *);

static void plp_dma (plp_t *, memd_t *, int);
static void plp_stop_dma (plp_t *);
static void plp_reset(plp_t *);
static plp_regs_t *plp_probe (int);
static void plp_read_flush (plp_t *);
static int plp_read_copy (plp_t *, caddr_t, int);
static int plp_queue (memd_t **, void *, int);
static void plp_free (memd_t *);
static void rbuf_init(void);
static void rbuf_save (plp_t *);
static void del_rbuf (memd_t *);
static memd_t *new_rbuf (int);
static int plp_lock(int *);
static int plp_unlock(int *);
static int plp_mem_init(void);
static void plp_mem_free(void);

/*
 * plpinit - initialize parallel port driver
 */
void
plpinit (void)
{
    memd_t *node_space;
    unsigned int end_space;
    plp_regs_t *plpaddr;
    plp_t *p;
    int plp;

    /* cpu-dependent allocation of space for memory descriptors, 
     * receive buffers, and transmit buffers (if needed).  Creates
     * mdmem, rbufmem, and lmembufs[NXMD].  Returns ENOMEM if memory 
     * cannot be allocated.
     */
    if (plp_mem_init()) {
	cmn_err(CE_WARN, "plp: initialization failed, opens will be refused");
	plpinitfail = 1;
	return;
    }

    /* initialize free_mds to contain all memory descriptors allocated
     * from mdmem.  
     */
    node_space = (memd_t *)mdmem;
    end_space = (unsigned int)mdmem + MDMEMSIZE;

    while (((unsigned int)node_space + sizeof (memd_t)) <= end_space) {
	/* throw out descriptors that cross page boundaries
	 */
	if (btoc(node_space) == btoc((int)node_space+sizeof(memd_t)-1))
	    LPUTNODE (free_mds, (memd_t *)PHYS_TO_K0(kvtophys(node_space)));
	node_space++;
    }

    /* allocate buffers for read data and initialize
     * read data list
     */
    rbuf_init();

    /* initialize parallel port struct
     */
    for (plp = 0; plp < NPLP; ++plp) {
	p = &plpport[plp];
	p->state = 0;
	p->timeoutid = 0;
	if (plpaddr = plp_probe(plp)) {
	    /* set interrupt vector depending on hpc number
	     */
	    ASSERT(plp == 0);		/* allow only parallel port 0 */
	    setlclvector (plp ? PLP_VEC_XTRA : PLP_VEC_NORM, plpintr, 0);
	    p->regs = plpaddr;
	    p->control = PLP_CONTROL_DEF;
	    p->state = PLP_ALIVE;
	    p->rbufs = (memd_t *)0;
	    p->dl = (memd_t *)0;
	    p->rlock = p->wlock = p->dlock = 0;
	    p->rto = PLP_RTO;
	    p->wto = PLP_WTO;
	    plp_reset(p);
	}
    }
    return;
}

/*
 * plpunload - check every minor device number to be sure that there
 * 	are no open references to this driver.  Return EBUSY if the driver
 * 	is still in use.  If the driver is no longer in use, then free up
 *	any allocated resources and return 0.  The higher level mload
 *	unload function will guarantee that no opens are called on this
 *	driver while it is being unloaded.
 */
int
plpunload(void)
{
    int plp, rcnt;

    for (plp = 0; plp < NPLP; ++plp) {
	if (plpport[plp].state & PLP_INUSE)
	    return EBUSY;
    }

    for (plp = 0; plp < NPLP; ++plp) {
	if (plpport[plp].state & PLP_ALIVE)
	    setlclvector (plp ? PLP_VEC_XTRA : PLP_VEC_NORM, 0, 0);
    }

    plp_mem_free();
    return 0;
}

/*
 * plpopen - open parallel port device
 */
int
plpopen (dev_t *devp, int mode)
{
    plp_t *plp;

    if (plpinitfail)
        return EIO;

    if (plpunit(*devp) >= NPLP)
        return ENXIO;

    plp = &plpport[plpunit(*devp)];

    if (geteminor(*devp) & PLPBI) {
        int s = LOCK();
        if (!(plp->state & PLP_OCCUPIED))
            plp_read_flush(plp);        /* flush read buffer */
        plp->state |= PLP_RDOPEN|PLP_OCCUPIED;
        UNLOCK(s);
    } else
        plp->state |= PLP_WROPEN|PLP_OCCUPIED;

    plp->state |= PLP_INUSE;
    return 0;

}

/*
 * plpclose - close parallel port device
 */
int
plpclose (dev_t dev)
{
    plp_t *plp = &plpport[plpunit(dev)];

    plp->state &= ~(PLP_WROPEN | PLP_RDOPEN | PLP_NBIO | PLP_OCCUPIED);
    plp_stop_dma (plp);
    if (plp->regs->ctrl & PAR_CTRL_PPTOMEM)
	rbuf_save (plp);
    plp_read_flush (plp);		/* flush read buffer */
    plp_unlock (&plp->dlock);
    plp->dl = 0;
    plp->rto = PLP_RTO;
    plp->wto = PLP_WTO;
    plp->control = PLP_CONTROL_DEF;	/* restore default strobe length */

    plp->state &= ~PLP_INUSE;
    return 0;
}

/*
 * plpread - read from parallel port device
 */
int
plpread (dev_t dev, uio_t *uiop)
{
    plp_t *plp = &plpport[plpunit(dev)];

    if (uiop->uio_resid == 0) 
	return 0;

    /*
     * If using output only device but PLPIOCREAD ioctl
     * hasn't been done then return zero.
     */
    if ((!(geteminor(dev))) && (!(plp->state & PLP_RDOPEN)))
        return EIO;


    return uiophysio (plpio, NULL, dev, B_READ, uiop);
}

/*
 * plpwrite - write to parallel port device
 */
int
plpwrite (dev_t dev, uio_t *uiop)
{
    if (uiop->uio_resid == 0)
	return 0;

    return uiophysio (plpio, NULL, dev, B_WRITE, uiop);
}

/*
 * plpioctl - control parallel port device
 */
int
plpioctl (dev_t dev, int cmd, int arg, int mode, struct cred *crp, int *rvp)
{
    plp_t *plp = &plpport[plpunit(dev)];
    unsigned char ext;

    switch (cmd) {
    case PLPIOCRESET:
	plp_reset (plp);
	break;
    case PLPIOCSTATUS:
	/* EOI and EOP are interchanged on IP20
	 * this keeps user programs compatible
	 */
	ext = (plp->regs->extreg & 0xf) ^ PLPFAULT;
	*rvp = (ext & (PLPFAULT | PLPONLINE))
	      | (ext & PLPEOI ? PLPEOP : 0)
	      | (ext & PLPEOP ? PLPEOI : 0);
	break;
    case PLPIOCTYPE:
	if (arg == PLP_TYPE_CENT)
	    plp->control &= ~PAR_CTRL_POLARITY;
	else if (arg == PLP_TYPE_VERS)
	    plp->control |= PAR_CTRL_POLARITY;
	else
	    return EINVAL;
	break;
    case PLPIOCSTROBE:
	plp->control &= ~PAR_STROBE_MASK;
	plp->control |= (unsigned int)arg & PAR_STROBE_MASK;
	break;
    case PLPIOCIGNACK:
	if (arg)
	    plp->control |= PAR_CTRL_IGNACK;
	else
	    plp->control &= ~PAR_CTRL_IGNACK;
	break;
    case PLPIOCSETMASK:
	/* for compatibility with IP6 */
	break;
    case PLPIOCWTO:
	plp->wto = arg * HZ;
	break;
    case PLPIOCRTO:
	plp->rto = arg * HZ;
	break;
    case PLPIOCREAD:
	/* if this is the first open for read, then start
	 * up the read processing
	 */
	if (!(plp->state & PLP_RDOPEN)) {
	    plp_read_flush (plp);		/* flush read buffer */
	    plp->state |= PLP_RDOPEN;
	}
	break;
    case FIONBIO:
	if (arg)
	    plp->state |= PLP_NBIO;
	else
	    plp->state &= ~PLP_NBIO;
	break;
    case PLPIOMODE:
        break;		/* Backward compatibility */
    default:
	return EINVAL;
    }
    return 0;
}

/* 
 * plp_lock - protect read, write, and dma channel accesses
 *
 * returns 1 if it catches a signal before getting the lock
 * returns 0 if it gets the lock successfully
 */
static int 
plp_lock(int *lockaddr)
{
    int s = LOCK();

    while (*lockaddr & PLP_BUSY) {
	*lockaddr |= PLP_WANTED;
	if (sleep ((caddr_t)lockaddr, PUSER)) {
	    UNLOCK(s);
	    return 1;
	}
    }
    *lockaddr |= PLP_BUSY;
    UNLOCK(s);
    return 0;
}

/* 
 * plp_unlock - give up read, write, and dma channel access locks
 *
 * returns 1 if it wakes up another process waiting on the lock
 * returns 0 if no process gets awakened
 */
static int 
plp_unlock(int *lockaddr)
{
    *lockaddr &= ~PLP_BUSY;

    if (*lockaddr & PLP_WANTED) {
	*lockaddr &= ~PLP_WANTED;
	wakeup ((caddr_t)lockaddr);
	return 1;
    }
    return 0;
}

/*
 * plpintr_read - initiate read from interrupt level
 */
static void
plpintr_read (plp_t *plp, int resethpc)
{
    memd_t *rb = new_rbuf(1);

    if (rb == 0) {
	/* we ran out of rbufs.
	 * if a process was waiting to write, give up channel,
	 * otherwise, schedule a timeout for a dummy read.
	 */
	if (!plp_unlock (&plp->dlock))
	    plp->timeoutid = timeout(plpintr, plp, PLP_RTO);
	plp->dl = 0;
    } else {
	/* we got an rbuf. start a read dma on the channel
	 */
	rb->memd_bc = NBPRD;
	rb->memd_eox = 1;
	rb->memd_forw = 0;
	if (cachewrback)
		dki_dcache_wbinval ((void *)rb, sizeof(memd_t));
	rb = (memd_t *) kvtophys (rb);
	if (resethpc) {
	    plp->regs->ctrl = PAR_CTRL_RESET;
	    plp->regs->ctrl = 0;
	}
	plp_dma (plp, rb, B_READ);
    }
}

/*
 * plpintr - parallel port device interrupt handler
 *
 * if plp is set, then the transfer timed out, otherwise,
 * we got a real interrupt and the devices get polled if 
 * they are alive
 */
static void
plpintr (int plp_pun, struct eframe_s *ep)
{
    plp_t *plp = (plp_t *)plp_pun;
    int pcount;
    memd_t *dl, *desc;

    /* read or write timed out
     */
    if (plp) {
	plp_stop_dma (plp);

	/* On a partial write, set an error flag. On a partial
	 * read, put the data onto the rbuf list for the channel. 
	 */
	if (!(plp->regs->ctrl & PAR_CTRL_PPTOMEM)) {	/* partial write */
	    /* add up transfer size from memory descriptors
	     */
	    dl = plp->dl;
	    pcount = 0;
	    while ((dl != 0) && (dl != plp->regs->nbdp)) {
		desc = (memd_t *)PHYS_TO_K0(dl);
		if (desc->memd_nbdp == (unsigned)plp->regs->nbdp)
		    desc->memd_bc -= plp->regs->bc;
		pcount += desc->memd_bc;
		dl = desc->memd_forw;
	    }
	    plp->bp->b_resid -= pcount;
	    plp->bp->b_error = EAGAIN;	/* indicates write timed out */

	    /* if a write fails, maybe the receiver is waiting
	     * to transmit, so initiate a read if the port is open
	     * for reading
	     */
	    if (plp->state & PLP_RDOPEN)
		plpintr_read(plp, 1);
	    else
		plp_unlock (&plp->dlock);
	    wakeup (&plp->dl);
	}
	else {						/* partial read */
	    /* if a read times out, then allow a write if there is
	     * one waiting.  if no write is waiting, then start
	     * a read if the port is open for reading
	     */
	    rbuf_save (plp);
	    if (!plp_unlock (&plp->dlock) && (plp->state & PLP_RDOPEN)) {
		plp_lock (&plp->dlock);
		plpintr_read(plp, 1);
	    }
	}
    } 
    
    /* got a real read or write interrupt
     */
    else {
	/* poll the plp devices to find one that has interrupted
	 * and service it
	 */
	for (pcount = 0; pcount < NPLP; ++pcount) {
	    plp = &plpport[pcount];
	    if ((plp->state & PLP_ALIVE) && (plp->regs->ctrl & PAR_CTRL_INT)) {
		plp_stop_dma (plp);
		if (plp->regs->ctrl & PAR_CTRL_PPTOMEM)
		    rbuf_save (plp);
		else
		    wakeup (&plp->dl);

		/* if the transfer was successful, always initiate a
		 * read if the port is open for reading
		 */
		if (plp->state & PLP_RDOPEN)
		    plpintr_read(plp, 0);
		else
		    plp_unlock (&plp->dlock);
	    }
	}
    }
    return;
}

/*
 * plpio - do parallel port device I/O
 */
static int
plpio (struct buf *bp)
{
    int xbytes;		/* number of bytes in current transfer */
    void *vaddr;	/* address of current transfer */
    plp_t *plp = &plpport[plpunit(bp->b_edev)];
    memd_t *dl;
    int s;

    bp->b_resid = bp->b_bcount;

    /* try to satisfy a read request on buffered data
     */
    if (bp->b_flags & B_READ) {
	/* grab the read lock so no processes get intermingled data 
	 */ 
	if (plp_lock (&plp->rlock)) {
	    bp->b_error = EINTR;
	} else {
	    while (bp->b_resid && !bp->b_error) {
		vaddr = bp->b_dmaaddr + (bp->b_bcount - bp->b_resid);
		/* copy as much data as we have buffered on the channel
		 */
		if (plp->rbufs)
		    bp->b_resid -= plp_read_copy (plp, vaddr, bp->b_resid);

		if (plp->state & PLP_NBIO)
		    break;

		/* if necessary, wait for additional data to arrive 
		 * asynchronously
		 */
		s = LOCK();
		if (bp->b_resid && !plp->rbufs) {
		    if (sleep (&plp->rbufs, PUSER))
			bp->b_error = EINTR;
		}
		UNLOCK(s);
	    }
	    plp_unlock (&plp->rlock);
	}
    }
    
    else {
	/* grab the write lock so no processes send intermingled data
	 */
	if (plp_lock (&plp->wlock)) {
	    bp->b_error = EINTR;
	} else {
	    while (bp->b_resid && !bp->b_error) {
		/* determine the virtual address of the next portion of
		 * the buffer to transfer, then create a memory descriptor
		 * list for the transfer
		 */
		vaddr = bp->b_dmaaddr + (bp->b_bcount - bp->b_resid);
		xbytes = plp_queue (&dl, vaddr, bp->b_resid);
		if (xbytes < 0) {
		    bp->b_error = EINTR;
		    break;
		}
    		if (cachewrback)
		    dki_dcache_wbinval (vaddr, xbytes);

		/* get the dma channel lock.  the interrupt routine
		 * will unlock the channel when writing is allowed again.
		 */
		if (plp_lock (&plp->dlock)) {
		    bp->b_error = EINTR;
		    plp_free (dl);
		    break;
		}

		/* start dma transfer
		 */
		plp->bp = bp;
		s = LOCK();
		plp_dma (plp, dl, B_WRITE);

		/* wait for transfer to complete
		 */
		if (sleep (&plp->dl, PUSER)) {
		    bp->b_error = EINTR;
		    plp_stop_dma(plp);
		    plp_unlock (&plp->dlock);
		}
		UNLOCK(s);

		/* put the descriptor list back on the free list
		 */
		plp_free (dl);

		/* if dma completes successfully update resid bytes, otherwise
		 * resid was already fixed up by the interrupt handler for
		 * a partial transfer, and we need to clear b_error
		 */
		if (!bp->b_error) 			/* complete xfer */
		    bp->b_resid -= xbytes;
		else if (bp->b_error == EAGAIN) { 	/* partial xfer */
		    bp->b_error = 0;
		    break;
		}
	    }
	    plp_unlock (&plp->wlock);
	}
    }

    if (bp->b_error)
	bp->b_flags |= B_ERROR;
    iodone (bp);

    /* value returned but ignored, thanks USL */
    return 0;
}

/*
 * plp_free - free a memory descriptor list 
 * 
 * the descriptor list was presumably created by plp_queue()
 * dl and all nbdp's in list are physical addresses
 */
static void
plp_free (memd_t *dl)
{
    int dowakeup = (free_mds == 0);
    memd_t *tmp;

    while (dl) {
	dl = (memd_t *) PHYS_TO_K0 (dl);
	tmp = dl->memd_forw;
	LPUTNODE (free_mds, dl);
	dl = tmp;
    }
    if (dowakeup)
	wakeup ((caddr_t) &free_mds);
    return;
}

/*
 * plp_queue - create a descriptor list from a vaddr and byte count.  
 *
 * pdl is the address of where to put the memory desc list
 * addr is the virtual address to map to memory descriptors
 * bcount is the length of the buffer
 * This is guaranteed to create a list of at least 1 memd
 * returns number of bytes that are queued in list
 */
static int
plp_queue (memd_t **pdl, void *vaddr, int bcount)
{
    memd_t  *next, *current;
    unsigned size, bytes = 0;
    paddr_t physaddr;
#ifdef IP20
    int lmoff = 0;
#endif

    /* get at least a first memd for the transfer
     */
    do {
	LGETNODE(free_mds, current);
	if (current == (memd_t *)0) {
	    if (sleep (&free_mds, PUSER))
		return -1;		/* interrupted */
	}
    } while (current == (memd_t *)0);
    
    /* first time through, we are guaranteed that
     * 'current' contains a usable node
     */
    ASSERT (current != (memd_t *)0);
    *pdl = (memd_t *)kvtophys(current);

    do {
	/* if the address crosses a page boundary
	 * set the size only up to that boundary
	 */
	if ((size = NBPP - poff(vaddr)) > bcount)
		size = bcount;
	current->memd_bc = size;
	current->memd_eox = 0;
	physaddr = kvtophys(vaddr);

#ifdef IP20
	if (physaddr&0xf0000000) {
	    /* DMA descriptor only 28 bits, so have to copy to
	     * allocated buffer and use that.  cached copies are
	     * fastest; see comments at top of IP20.c */
	    ASSERT(lmembufs[lmoff]);
	    physaddr = (paddr_t)lmembufs[lmoff++];
	    bcopy(vaddr, (void *)physaddr, size);
	    dki_dcache_wbinval((void *)physaddr, size);
	    physaddr = kvtophys((void *)physaddr);
	}
#endif
	current->memd_cbp = physaddr;

	bcount -= size;
	bytes += size;
	vaddr = (void *)((unsigned)vaddr + size);

	if (bcount > 0) {
	    LGETNODE(free_mds, next);
	    if (next) {
		current->memd_forw = (memd_t *)kvtophys(next);
		if (cachewrback)
		    dki_dcache_wbinval ((void *)current, sizeof(memd_t));
		current = next;
	    }
	}
	    
    } while (next != (memd_t *)0 && bcount > 0);

    current->memd_forw = 0;
    current->memd_eox = 1;
    if (cachewrback)
	dki_dcache_wbinval ((void *)current, sizeof(memd_t));
    return (bytes);
}

/*
 * plp_read_flush - flush the input buffer list on a channel
 */
static void
plp_read_flush (plp_t *plp)
{
    memd_t *rb;

    LGETNODE (plp->rbufs, rb);
    while (rb) {
	del_rbuf (rb);
	LGETNODE (plp->rbufs, rb);
    }
}

/*
 * plp_read_copy - copy data from a channel's input buffer list to vaddr
 */
static int 
plp_read_copy (plp_t *plp, caddr_t vaddr, int nbytes)
{
    memd_t *rb;
    caddr_t cp = vaddr;
    int bcount = nbytes;

    /* start with the first rbuf on the list 
     */
    LGETNODE (plp->rbufs, rb);
    while (rb && bcount) {
	/* if readbuf fits in read request, then
	 * copy data, and free rbuf
	 */
	if (rb->memd_bc <= bcount) {
	    bcopy ((void *)PHYS_TO_K1(rb->memd_buf), cp, rb->memd_bc);
	    bcount -= rb->memd_bc;
	    cp += rb->memd_bc;			
	    del_rbuf(rb);
	    /* get next rbuf from list if more bytes are to
	     * be transfered
	     */
	    if (bcount)
		LGETNODE (plp->rbufs, rb)
	} 
	/* copy a portion of the read buffer to cp
	 */
	else {
	    bcopy ((void *)PHYS_TO_K1(rb->memd_buf), cp, bcount);
	    cp += bcount;			
	    rb->memd_buf += bcount;
	    rb->memd_bc -= bcount;
	    bcount = 0;
	    /* push rbuf with remaining data back onto head of list
	     */
	    LPUTNODE (plp->rbufs, rb);
	}
    }
    return nbytes - bcount;
}

static memd_t *free_rbufs = 0; 	/* list of all free read descriptor nodes */

/*
 * rbuf_init - initialize free read buffer descriptor list
 */
static void
rbuf_init(void)
{
    caddr_t pbuf;
    int pcount;
    memd_t *md;

    /* allocate the desired number of pages
     */
    pbuf = rbufmem;

    for (pcount = 0; pcount < NRP * NRDPP; pcount++) {
	LGETNODE (free_mds, md);
	md->memd_buf = (caddr_t)kvtophys(pbuf);
	LPUTNODE (free_rbufs, md);
	pbuf += NBPRD;
    }
}

/*
 * rbuf_save - move newly received data from dl to rbuf list
 *
 * addresses are converted from physical to kernel again
 * unused nodes in descriptor list are returned to free rbuf list
 * this is called from interrupt level only
 * doesn't use LPUTNODE because it chains rbufs onto end of list
 */
static void
rbuf_save (plp_t *plp)
{
    memd_t *dl = plp->dl;
    memd_t *newbuf;
    memd_t *inbufs;
    int rbcount = 0;

    /* If we get a read timeout but there was never any
     * DMA started, then dl is empty.  This happens if
     * we run out of rbufs at interrupt level.
     */
    if (dl == 0)
	return;

    /* point inbufs at last buffer in rbuf list
     */
    if (inbufs = plp->rbufs)
	while (inbufs->memd_forw)
	    inbufs = inbufs->memd_forw;

    /* link newly received buffers onto rbuf list 
     */
    while ((dl != 0) && (dl != plp->regs->nbdp)) {
	newbuf = (memd_t *)PHYS_TO_K0(dl);

	/* adjust buffer length for last descriptor if transfer
	 * is incomplete
	 */
	if ((newbuf->memd_nbdp == (unsigned)plp->regs->nbdp) 
		&& (plp->regs->bc != 0)) {
	    newbuf->memd_bc -= plp->regs->bc;
	    if (newbuf->memd_bc == 0)
		break;
	}
	dl = newbuf->memd_forw;
	newbuf->memd_forw = 0;
	if (inbufs) 
	    inbufs->memd_forw = newbuf;
	else
	    plp->rbufs = newbuf;
	inbufs = newbuf;
	rbcount++;
    }
    ASSERT((dl == plp->regs->nbdp) || (dl == plp->regs->cbp));

    /* wakeup any process that may be awaiting input on this channel
     */
    if (rbcount)
	wakeup (&plp->rbufs);

    /* free up unused buffers 
     */
    while (dl != 0) {
	newbuf = (memd_t *)PHYS_TO_K0(dl);
	dl = newbuf->memd_forw;
	del_rbuf (newbuf);
    }
    plp->dl = 0;
}

/*
 * new_rbuf - allocate an rbuf from the free list
 *
 * will always return with an rbuf unless the list is empty
 * and it is called from interrupt level, or a signal is received
 */
static memd_t *
new_rbuf (int ilev)
{
    memd_t *rbuf;

    do {
	LGETNODE(free_rbufs, rbuf);
	if (rbuf == (memd_t *)0) {
	    /* we currently only allocate rbufs at interrupt
	     * level so this never sleeps
	     */
	    if (ilev || sleep (&free_rbufs, PUSER))
		break;
	}
    } while (rbuf == (memd_t *)0);
    return rbuf;
}

/*
 * del_rbuf - put an rbuf back onto the free list
 */
static void
del_rbuf (memd_t *rbuf)
{
    int dowakeup = (free_rbufs == 0);

    rbuf->memd_cbp &= ~(NBPRD - 1);	/* realign buffer pointer */
    LPUTNODE(free_rbufs, rbuf);
    if (dowakeup)
	wakeup (&free_rbufs);
}

/* hardware functions for parallel interface
 */

/*
 * plp_dma - start dma on descriptor list dl and channel pointed to by plp
 *
 * dma channel must currently not be in use, dl is the
 * descriptor list for the transfer which is a physical address.
 */
static void
plp_dma (plp_t *plp, memd_t *dl, int flags)
{
    plp->dl = dl;

    plp->regs->cbp = (memd_t *)0x0badbad0; 	/* for good measure */
    plp->regs->bc = 0x1bad;
    plp->regs->nbdp = dl;	

    if (flags & B_READ) {
	plp->regs->extreg = PLP_EXT_RESET;
	plp->regs->ctrl = PAR_CTRL_PPTOMEM | PAR_CTRL_STRTDMA | plp->control;
	if (plp->rto)
	    plp->timeoutid = timeout(plpintr, plp, plp->rto);
	else
	    plp->timeoutid = 0;
    } else {
	plp->regs->extreg = PLP_EXT_PRT | PLP_EXT_RESET;
	plp->regs->ctrl = PAR_CTRL_MEMTOPP | PAR_CTRL_STRTDMA | plp->control;
	if (plp->wto)
	    plp->timeoutid = timeout(plpintr, plp, plp->wto);
	else
	    plp->timeoutid = 0;
    }
    return;
}

/*
 * plp_stop_dma - stop current dma transfer
 *
 * disable any timeouts, stop the dma transfer, and
 * if reading, then flush the HPC fifo to memory.
 */
static void
plp_stop_dma (plp_t *plp)
{
    int timout = 100;
    unsigned data, fcnt;
    unsigned char *cbp, *datap;

    if (plp->timeoutid)
	untimeout(plp->timeoutid);
    plp->timeoutid = 0;

    /* if reading, then wait for memory flush to complete
     */
    if (plp->regs->ctrl & PAR_CTRL_PPTOMEM) {
	fcnt = (plp->regs->pntr >> 16) & 0x7f;
	if (fcnt > 0 && fcnt < 4) {
	    plp->regs->ctrl &= ~(PAR_CTRL_FLUSH | PAR_CTRL_STRTDMA);
	    data = plp->regs->fifo;
	    datap = (unsigned char *)&data;
	    /* if hpc hasn't yet updated the cbp, then use the
	     * cbp from the descriptor list, and force the hpc registers
	     * to valid values for downstream processing
	     */
	    if (plp->regs->nbdp == plp->dl) {
		memd_t *dl = (memd_t *)PHYS_TO_K0(plp->dl);
		cbp = (unsigned char *)PHYS_TO_K0(dl->memd_cbp);
		plp->regs->cbp = (memd_t *)dl->memd_cbp;
		plp->regs->nbdp = (memd_t *)dl->memd_nbdp;
		plp->regs->bc -= fcnt;
	    }
	    else
		cbp = (unsigned char *)PHYS_TO_K0(plp->regs->cbp);
	    if (fcnt >= 1)
		*cbp++ = *datap++;
	    if (fcnt >= 2)
		*cbp++ = *datap++;
	    if (fcnt >= 3)
		*cbp++ = *datap++;
	} else {
	    /* flush bit stops any transfer
	     */
	    plp->regs->ctrl |= PAR_CTRL_FLUSH;
	    while ((plp->regs->ctrl & PAR_CTRL_STRTDMA) && --timout)
		continue;
	    if (!timout)
		cmn_err(CE_WARN,"Parallel port DMA never finished\n");
	}
	plp->regs->ctrl &= ~(PAR_CTRL_FLUSH | PAR_CTRL_STRTDMA);
    } else {
	plp->regs->ctrl &= ~PAR_CTRL_STRTDMA;
#ifdef LATER
	/* check for a parity error on the transfer
	 */
	if (*(volatile unsigned int *)PHYS_TO_K1(PAR_ERR_ADDR) & PAR_DMA) {
	    unsigned int err, addr;

	    err = *(volatile unsigned int *)PHYS_TO_K1(PAR_ERR_ADDR);
	    addr = *(volatile unsigned int *)PHYS_TO_K1(DMA_PAR_ADDR);
	    log_perr (err, addr);
	}
#endif /* LATER */
    }
    return;
}

/*
 * plp_probe - probe for parallel port hardware
 *
 * returns the address of the parallel port registers
 */
static plp_regs_t *
plp_probe (int plp)
{
    unsigned int *hpcbase = (unsigned int *)
	    PHYS_TO_K1((plp & 1) ? HPC_1_ID_ADDR : HPC_0_ID_ADDR);

    if (plpprobe[plp] && !badaddr(hpcbase, 4))
	return (plp_regs_t *)((int)hpcbase + PAR_OFFSET);
    else
	return (plp_regs_t *)0;
}

/*
 * plp_reset - reset the HPC/parallel port interface
 */
static void
plp_reset(plp_t *plp)
{
    if (plp_lock (&plp->dlock))
	return;

    plp->regs->ctrl |= PAR_CTRL_RESET;		/* reset HPC logic */
    plp->regs->ctrl &= ~PAR_CTRL_RESET;

    plp->regs->extreg = 0;			/* reset printer */
    DELAY(400);
    plp->regs->extreg = PLP_EXT_PRT | PLP_EXT_RESET;

    plp->regs->ctrl |= PAR_CTRL_SOFTACK;	/* generate softack */
    plp->regs->ctrl &= ~PAR_CTRL_SOFTACK; 	

    plp_unlock (&plp->dlock);
    return;
}

#ifdef IP20

static int
plp_mem_init(void)
{
    int i;
    extern char *low_plp_desc;	/* low memory for descriptors */
    extern char *low_plp_rbuf;	/* low memory for rcv buffers */

    mdmem = (caddr_t)(((unsigned long)low_plp_desc + scache_linemask) &
	~scache_linemask);
    rbufmem = (caddr_t)(((unsigned long)low_plp_rbuf + (NBPP-1)) & ~(NBPP-1));
    ASSERT(mdmem);
    ASSERT(rbufmem);
    ASSERT(!(kvtophys(mdmem) & 0xf0000000));
    ASSERT(!(kvtophys(rbufmem) & 0xf0000000));

    for (i = 0; i < NXMD; ++i)
	lmembufs[i] = 0;

    if (physmem > 0x8000) {
	for (i = 0; i < NXMD; ++i) {
	    lmembufs[i] = (caddr_t)kvpalloc(1, VM_DIRECT|VM_NOSLEEP, 0);
	    if (!lmembufs[i] || (kvtophys(lmembufs[i]) & 0xf0000000)) {
		plp_mem_free();
		cmn_err(CE_WARN,
		    "plp: could not get buffers in low memory"
		    " for > 128 Mbyte system\n"
			"Try forcing driver to be linked staticly by removing the flags\n"
			"\"dR\" from /var/sysgen/master.d/plp, then run /etc/autoconfig\n"
			"and reboot");
		return ENOMEM;
	    }
	}
    }
    return 0;
}

static void
plp_mem_free(void)
{
    int i;

    if (physmem > 0x8000) {
	for (i = 0; i < NXMD; ++i) {
	    if (lmembufs[i])
		kvpfree(lmembufs[i], 1);
	    lmembufs[i] = 0;
	}
    }
}

#ifdef DEBUG
void
plp_show_dl(memd_t *dl)
{
    static int showcount;
    memd_t *k0dl;

    showcount++;

    if (!dl) {
	dprintf ("plp(%d): dl empty\n", showcount);
    } else {
	while (dl) {
	    k0dl = (memd_t *)PHYS_TO_K0(dl);
	    dprintf ("plp(%d): dl 0x%x, bc %d, cbp 0x%x, nbdp 0x%x\n",
		showcount, dl, k0dl->memd_bc, k0dl->memd_cbp, k0dl->memd_nbdp);
	    dl = k0dl->memd_forw;
	}
    }
}
#endif /* DEBUG */
#endif /* IP20 */
#endif /* defined IP20 */
