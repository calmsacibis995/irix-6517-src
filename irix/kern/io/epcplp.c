#ident	"io/epcplp.c:  $Revision: 1.31 $"

#if defined(EVEREST)
/* 
 * epcplp.c - driver for the Everest parallel port
 */

#include <values.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evintr.h>
#include <sys/immu.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/plp.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/invent.h>
#include <sys/systm.h>
#include <bstring.h>
#include <string.h>

int plpdevflag = 0;

#ifdef DEBUG
int plp_debug = 0;
#define dprintf(x) if (plp_debug) printf x
#else
#define dprintf(x)
#endif


/* Locking code to replace earlier definitions of LOCK/UNLOCK as
 * aliases for spl5()/splx().  Assume that LOCK/UNLOCK are properly
 * paired and accomodate nested usage.
 */

static mutex_t	plpmutex;
static int	plpmutex_lock_nesting = 0;

static int plp_LOCK (void) {
    if (!mutex_mine (&plpmutex))
	mutex_lock (&plpmutex, PZERO);
    dprintf (("plp_LOCK: nesting now %d\n", plpmutex_lock_nesting+1));
    return ++plpmutex_lock_nesting;
}

static void plp_UNLOCK (int chknest) {
    ASSERT (chknest == plpmutex_lock_nesting);
    dprintf (("plp_UNLOCK: nesting is %d\n", plpmutex_lock_nesting));
    if (!--plpmutex_lock_nesting)
	mutex_unlock (&plpmutex);
}

#define LOCK()		plp_LOCK()
#define UNLOCK(a)	plp_UNLOCK(a)


/* Values for various locks */
#define PLP_BUSY	0x1
#define PLP_WANTED	0x2

/* Values for "state" */
#define PLP_ALIVE	0x1	/* parallel port exists */
#define PLP_WROPEN	0x2	/* open for writing */
#define PLP_RDOPEN	0x4	/* open for reading */
#define PLP_NBIO	0x8	/* set for non-blocking reads */

/* goes in timeout_id when a timeout occurs. */
#define PLP_TIMEDOUT	((toid_t) (-1))

/* struct defining state of parallel port
 * 	- rlock and wlock protect r/w loops so data on a single r/w 
 *		call is contiguous
 *	- dlock protects the dma channel (regs, dl, bp)
 */

/* Number of possible parallel ports.  This really amounts to how many
 *  interrupts we have available.
 */
#define NPLP		4

/* Number of 4k buffers per parallel port. - We need a minimum of 2.
 */
#define PLP_NBUFS	4

/* There are two plp buf structures for each port (read, write) 
 */
typedef struct plpbuf_s {
    volatile toid_t timeout_id;		/* timeout number */
    volatile int hw_buf;
    volatile int sw_buf;
    volatile int dma_active;
    volatile char *buf_space[PLP_NBUFS];
    volatile int buf_len[PLP_NBUFS];
    volatile int buf_resid[PLP_NBUFS];	/* What resid was at buf start */
    volatile int part_xfer[PLP_NBUFS];
    int buf_lock;			/* The driver sleeps on this when it's
				   	 * out of buffers or on blocking IO */
    volatile int copyflag;		/* copying data flag */
    int init_resid;			/* uio_resid at the start of the call */
    caddr_t back_ptr;			/* Pointer to parent plp struct */
} plpbuf_t;

/* Allocate an array of structures for plp drivers.
 */
typedef struct plp_s {
    volatile unsigned char state;	/* software state of port */
    volatile unsigned int control;	/* value for strobe/control register */
    volatile unsigned int last_control;	/* last value written to plp control */
    volatile unsigned int pulls;	/* value to use for pull-{up/down}s */
    volatile unsigned int last_pulls;	/* last value written to plp pulls */
    volatile int rlock, wlock, dlock;	/* read, write, and dma locks */
    volatile __psint_t epcbase;		/* base address of this port's EPC */
    plpbuf_t rd_buf;			/* read buffer structure */
    plpbuf_t wr_buf;			/* write buffer structure */
    volatile int rto, wto;		/* read/write timeout in secs */
} plp_t;

static plp_t plpport[EV_MAX_SLOTS];

static int plp_intrs[NPLP] = {
	EVINTR_LEVEL_EPC_PPORT,
	EVINTR_LEVEL_EPC_PPORT1,
	EVINTR_LEVEL_EPC_PPORT2,
	EVINTR_LEVEL_EPC_PPORT3 };

/* prototypes */
struct eframe_s;
static void plpintr (struct eframe_s *, unsigned int);

static void plp_stop_dma (plp_t *);
static void plp_reset(plp_t *);
caddr_t epc_probe (int);
static int plp_lock(volatile int *);
static int plp_unlock(volatile int *);

#ifdef PLP_READS
static void plp_dma (plp_t *, int);
static void plp_read_flush(plp_t *);
#endif

static void plp_initbufs(plp_t *, int, int);
static void plp_startwrite(plp_t *);
static void plp_startread(plp_t *, int);
static int handle_part_write(plp_t *, uio_t *);
static void plptout(plpbuf_t *);
extern void store_half(void *, short);

#define SET(_r, _v)	store_double((long long *)(plp->epcbase + (_r)),\
				(long long)(_v))
#define GET(_r)		(int)load_double((long long *)(plp->epcbase + (_r)))
#define SETHALF(_r, _v)	store_half((short *)(plp->epcbase + (_r)),\
				(short)(_v))

#define PLP_NEXT(_b)	((_b + 1) % PLP_NBUFS)

#define EPC_PPDEFAULT	(EPC_PPSTB_L | EPC_PPBUSY_H | EPC_PPACK_L \
		| EPC_PPBUSYMODE \
		| EPC_PPIN \
		| (2 << EPC_PPPLSSHIFT) \
		| (2 << EPC_PPHLDSHIFT) \
		| (2 << EPC_PPSTPSHIFT))

#define EPC_PPPULDEF	(EPC_PPSTRBPUL_H | EPC_PPBUSYPUL_H)

#define PLP_RTO		(20 * HZ)	/* default: 1 sec timeout on a read */

#define PLP_WTO		0		/* default: never timeout on a write */

/*
 * plpinit - initialize parallel port driver
 */
void
plpinit (void)
{
    caddr_t epcaddr;
    plp_t *plp;
    plpbuf_t *bufp;
    __psint_t p;
    int i, j;
    char *tmp;
    int nplps = 0;

    dprintf(("plpinit: initializing epcplp\n"));

    mutex_init (&plpmutex, MUTEX_DEFAULT, "plpmutex");

    /* initialize parallel port structs
     */
    for (p = EV_MAX_SLOTS - 1; p >= 0; p--) {
	plp = &(plpport[p]);
	plp->state = 0;
	if (epcaddr = epc_probe(p)) {

	    if (nplps >= NPLP) {
		printf("plp: Parallel port in slot %d ignored (Max. of %d supported)\n",
							p, NPLP);
		continue;	
	    }

	    nplps++;

	    add_to_inventory(INV_PARALLEL, INV_EPC_PLP, (char)p, (char)0, 0L);
	    plp->epcbase = (__psint_t)epcaddr;
	    plp->control = EPC_PPDEFAULT;
	    plp->pulls = EPC_PPPULDEF;
	    plp->state = PLP_ALIVE;
	    plp->rlock = plp->wlock = plp->dlock = 0;
	    plp->rto = PLP_RTO;
	    plp->wto = PLP_WTO;

	    /* set up DMA buffers
	     */

	    for (i = 0; i <= 1; i++) {
		if (i)
		    bufp = &(plp->rd_buf);
		else
		    bufp = &(plp->wr_buf);

		tmp = kvpalloc(btoc(EPC_PPBLKSZ*PLP_NBUFS), VM_DIRECT, 0);
		if (tmp == NULL) {
		    /* Complain loudly */
		    printf("plp: Can't allocate buffers for plp slot %d\n",
							    p);
		    /* Turn off all subsequent ports and this one. */
		    for (; p < NPLP; p++)
			plp->state = 0;
		    return;
		}
		for (j = 0; j < PLP_NBUFS; j++) {
		    bufp->buf_space[j] = &tmp[j*EPC_PPBLKSZ];
		}
	    }

	    /* Initialize various variables associated with the buffers.
	     */
	    plp_initbufs(plp, 0, 1);

	    dprintf(("plp: Setting up hardware. plp->epcbase == %x\n",
				plp->epcbase));

	    /* Set up hardware: */

	    /* Set up the internal EPC parallel port register to
	     * the default values
	     */
	    SET(EPC_PPCTRL, EPC_PPDEFAULT);

	    /* Set up our bits in EPC_PRSTSET, EPC_PRSTCLR to EPC_PPPULDEF
	     * 		polarity is reversed.
	     * Use EPC_PULLMASK to avoid screwing up e-net
	     */

	    SET(EPC_PRSTSET, ~EPC_PPPULDEF & EPC_PPPULLMASK);
	    SET(EPC_PRSTCLR, EPC_PPPULDEF & EPC_PPPULLMASK);
	    plp->last_pulls = EPC_PPPULDEF;

	    /* Set up EPC_PPORT (PBUS reg.) */
	    /* Who knows what this should be set to!! */
	    SET(EPC_PPORT, 3);

	    /* Clear the high part of the address - we're not using it.
	     */
	    SET(EPC_PPBASEHI, 0);

            /* Set up the parallel port interrupt vector and destination.
             */
            SET(EPC_IIDPPORT,
                (plp_intrs[nplps - 1] << 8) | EVINTR_DEST_EPC_PPORT);

	    plp = &(plpport[p]);

	    /* Set up the kernel to handle our interrupts.
	     */
	    evintr_connect((evreg_t *)(plp->epcbase + EPC_IIDPPORT),
				plp_intrs[nplps - 1],
				SPLDEV,
				EVINTR_DEST_EPC_PPORT,
				(EvIntrFunc)plpintr,
				(void *)p);
			/* Pass the slot number as the parameter */
	    dprintf(("plp: Assigned interrupt level 0x%x to plp %d.\n",
				plp_intrs[nplps], p));
	} else {
	    plp->state=0;
	}

    }

    dprintf(("plp: Done. Initialized %d plps\n", nplps));
    return;
}

int
plpunload(void)
{
    return 0;
}

/*
 * plpopen - open parallel port device
 */
int
plpopen (dev_t *devp, int mode)
{
    plp_t *plp;

    dprintf(("plp: Opening plp minor number %d\n", minor(*devp)));

    if (minor(*devp) >= EV_MAX_SLOTS) {
	dprintf(("plp: Minor number out of range\n"));
	return ENXIO;
    }

    plp = &plpport[minor(*devp)];

    if (!(plp->state & PLP_ALIVE))
	return ENXIO;

    if (mode & FWRITE)
	plp->state |= PLP_WROPEN;


#ifdef notdef
    /* For backward compatibility, don't start reads by default, since
     * many programs open the port for reading and writing.  Allow reading
     * to be enabled with an ioctl call PLPIOCREAD
     */
    /* if this is the first open for read, then start
     * up the read processing
     */
    if ((mode & FREAD) && !(plp->state & PLP_RDOPEN)) {
    plp_read_flush (plp);		/* flush read buffer */
    plp->state |= PLP_RDOPEN;
}
#endif /* notdef */

    dprintf(("plp: Succeeded.\n"));

    return 0;
}

/*
 * plpclose - close parallel port device
 */
int
plpclose (dev_t dev)
{
    plp_t *plp = &plpport[minor(dev) & 15];
    int s;

    /* XXX - If we implement non-blocking writes, we'll need to wait for them
     * to complete here before shutting things down.
     */

    plp->state &= ~(PLP_WROPEN | PLP_RDOPEN | PLP_NBIO);

    plp_stop_dma (plp);

    /* Release all of the locks
     */
    plp_unlock (&plp->dlock);
    plp_unlock (&plp->rlock);
    plp_unlock (&plp->wlock);

    s = LOCK();

    /* Clear any pending timeouts - Done from high spl to make sure that
     * a timeout doesn't occur between testing plp->XX_buf.timeout_id
     * and clearing it via untimeout.
     */
    if (plp->wr_buf.timeout_id && plp->wr_buf.timeout_id != PLP_TIMEDOUT) {
	untimeout(plp->wr_buf.timeout_id);
	plp->wr_buf.timeout_id = 0;
    }

    if (plp->rd_buf.timeout_id && plp->rd_buf.timeout_id != PLP_TIMEDOUT) {
	untimeout(plp->rd_buf.timeout_id);
	plp->rd_buf.timeout_id = 0;
    }

    UNLOCK(s);

    /* restore default settings 
     */
    plp->control = EPC_PPDEFAULT;
    plp->pulls = EPC_PPPULDEF;
    plp->rto = PLP_RTO;
    plp->wto = PLP_WTO;

    /* Reinitialize buffer counts, pointers, flags...
     */
    plp_initbufs(plp, 0, 1);

    return 0;
}


/*
 * plpread - read from parallel port device
 */
int
plpread (dev_t dev, uio_t *uiop)
{
    int bytes_to_read;
    int minor_num = minor(dev);
    plp_t *plp = &(plpport[minor_num]);

    dprintf(("plp: ENTERING READ - Reading from plp%d (0x%x).\n", minor_num,plp));

    /* get read lock */
    if (plp_lock (&plp->rlock)) {
	return EINTR;
    }

    if (plp->rto) {
	plp->rd_buf.timeout_id = timeout(plptout, &(plp->rd_buf), plp->rto);
	dprintf(("plp: Setting timeout value to %d (id == 0x%x)\n", plp->rto,
							&(plp->rd_buf)));
    }

    dprintf(("plp:      uio_resid = 0x%x\n", uiop->uio_resid));

    /* If we're not already set up for reads, turn them on now.
     */
    plp->state |= PLP_RDOPEN;

    while (uiop->uio_resid) {
	/* Start with a very simple implementation.  When a read is requested,
	 *  start it up.  Don't release it until you're done.  No background
	 *  reading will be done.  No ioctl is necessary.
	 * No doubt, this sill need to be changed to accomodate the nasty
	 *  little Ricoh scanner, but it should serve our needs here.
	 */

	/* In this implementation, we never need to change sw or hw.  We'll
	 * start out using 0 all of the time until the _real_ code is
	 * written.
	 */

	bytes_to_read = MIN(uiop->uio_resid, EPC_PPMAXXFER
		- plp->wr_buf.buf_len[plp->wr_buf.sw_buf]);

	dprintf(("plp: Attempting to read 0x%x bytes to buffer _%d_ at 0x%x\n",
		bytes_to_read, plp->wr_buf.sw_buf,
		(caddr_t)plp->wr_buf.buf_space[plp->wr_buf.sw_buf]));

	/* get DMA lock */
	if (plp_lock (&plp->dlock)) {
	    return EINTR;
	}

	plp_startread(plp, bytes_to_read);

	/* Wait for this turkey to finish.
	 */

	dprintf(("plp: Sleeping...\n"));
	if (sleep ((caddr_t)&(plp->rd_buf.buf_lock), PUSER)) {
	    plp_unlock(&plp->rlock);
	    return EINTR;
	}

	if (plp->rd_buf.timeout_id == PLP_TIMEDOUT) {
	    uiomove((caddr_t)plp->wr_buf.buf_space[0],
		plp->rd_buf.part_xfer[0], UIO_READ, uiop);

	    plp_unlock (&plp->rlock);

	    dprintf(("plp: Read timed out.  Transferred 0x%x bytes.\n",
					plp->rd_buf.part_xfer[0]));

	    /* Clear out timed out flag */
	    plp->rd_buf.timeout_id = 0;

	    if (!plp->rd_buf.part_xfer[0])
		return EAGAIN;
	    else
		return 0;
	}

	uiomove((caddr_t)plp->wr_buf.buf_space[0],
		bytes_to_read, UIO_READ, uiop);
    }

    if (plp->wr_buf.timeout_id) {
	untimeout(plp->wr_buf.timeout_id);
	plp->wr_buf.timeout_id = 0;
    }

    plp_unlock (&plp->rlock);

    dprintf(("plp: Read complete.\n"));

    return 0;
}


/*
 * plpwrite - write to parallel port device
 */
int
plpwrite (dev_t dev, uio_t *uiop)
{
    int minor_num = minor(dev);
    plp_t *plp = &(plpport[minor_num]);
    int s;
    int move_bytes;

    dprintf(("plpwrite: ENTERING WRITE to plp%d (0x%x).\n", minor_num, plp));

    /* get write lock */
    if (plp_lock (&plp->wlock)) {
	return EINTR;
    }

    dprintf(("plpwrite: uio_resid = 0x%x\n", uiop->uio_resid));

    if (plp->wto) {
	plp->wr_buf.timeout_id = timeout(plptout, &(plp->wr_buf), plp->wto);
	dprintf(("plpwrite: setting timeout value to %d sec "
		 "(id == 0x%x)\n", plp->wto, plp->wr_buf.timeout_id));
    }

    /* We need to know the original transaction size to know if we managed
     * to transfer any data.
     */    
    plp->wr_buf.init_resid = uiop->uio_resid;

    while (uiop->uio_resid) {

	dprintf (("plpwrite: top of loop, uio_resid %d\n",
		  uiop->uio_resid));

	/* If we're still pointing at the hardware buffer and write DMA has
	 * started, use the next buffer.  Remember, the minumum number of
	 * buffers is 2.
	 */
	s = LOCK();
	if ((plp->wr_buf.hw_buf == plp->wr_buf.sw_buf)
						 && plp->wr_buf.dma_active)
	    plp->wr_buf.sw_buf = PLP_NEXT(plp->wr_buf.sw_buf);

	/* Make sure this buffer isn't full yet.  If so, are there more? */
	if (plp->wr_buf.buf_len[plp->wr_buf.sw_buf] == EPC_PPMAXXFER) {

	    /* Keep the interrupt handler from messing this up.  */
	    dprintf(("plpwrite: buffer %d is full.\n",
			plp->wr_buf.sw_buf));

	    /* If all buffers are in use, wait for a write buffer to open up */
	    
	    while ((PLP_NEXT(plp->wr_buf.sw_buf) == plp->wr_buf.hw_buf)) {
		dprintf(("plpwrite: sleeping for a buffer\n"));
		UNLOCK(s);
		if (sleep ((caddr_t)&(plp->wr_buf.buf_lock), PUSER)) {
		    dprintf (("plpwrite: awoke from sleep interrupted, "
			      "returning EINTR\n"));
		    plp_unlock(&plp->wlock);
		    return EINTR;
		}
		s = LOCK ();
		if (plp->wr_buf.timeout_id == PLP_TIMEDOUT) {
		    int	rval;
		    UNLOCK(s);
		    rval = handle_part_write(plp, uiop);
		    dprintf (("plpwrite: awoke from sleep (timeout), "
			      "returning %d\n", rval));
		    return rval;
		}
		dprintf (("plpwrite: awoke from sleep normally\n"));

	    } /* while */

	    /* Point copies at the next buffer */
	    plp->wr_buf.sw_buf = PLP_NEXT(plp->wr_buf.sw_buf);
	    dprintf(("plpwrite: advancing to next buffer %d\n",
			plp->wr_buf.sw_buf));
	}

	/* Tell the driver we're in the process of copying data into
	 * the write sw_buf.
	 */
	plp->wr_buf.copyflag = 1;

	/* If this is the first write to this block, set the resid value for
	 * it.
	 */
	if (!plp->wr_buf.buf_len[plp->wr_buf.sw_buf])
	    plp->wr_buf.buf_resid[plp->wr_buf.sw_buf] = uiop->uio_resid;
	
	/* Okay, the driver can run now.  */
	UNLOCK(s);

	move_bytes = MIN(uiop->uio_resid, EPC_PPMAXXFER
		- plp->wr_buf.buf_len[plp->wr_buf.sw_buf]);

        dprintf(("plpwrite: attempting to copy 0x%x bytes to "
		 "buffer _%d_ at 0x%x\n",
		move_bytes, plp->wr_buf.sw_buf,
		(caddr_t)plp->wr_buf.buf_space[plp->wr_buf.sw_buf]));	

	/* Copy as much data as we can from the iovecs into the plp buffers */
	uiomove((caddr_t)plp->wr_buf.buf_space[plp->wr_buf.sw_buf]
			+ plp->wr_buf.buf_len[plp->wr_buf.sw_buf],
		move_bytes, UIO_WRITE, uiop);

	s = LOCK();

	plp->wr_buf.copyflag = 0;

	/* The interrupt handler will look at buf_len to determine
	 * whether it should start up another write.  Don't fool
	 * it.
	 */
	plp->wr_buf.buf_len[plp->wr_buf.sw_buf] += move_bytes;	
	dprintf(("plpwrite: sw_buf == %d, len %d\n",
		 plp->wr_buf.sw_buf, plp->wr_buf.buf_len [plp->wr_buf.sw_buf]));

	/* If there isn't a write DMA in progress, start one up,
	 * suspending reads if necessary.
	 */

	if (!plp->wr_buf.dma_active) {

	    /* get DMA lock */
	    if (plp_lock (&plp->dlock)) {
		UNLOCK (s);
		return EINTR;
	    }

	    dprintf(("plpwrite: starting DMA.  hw==%d, len==0x%x, "
		     "epcbase==0x%x\n",
		plp->wr_buf.hw_buf,
		plp->wr_buf.buf_len[plp->wr_buf.hw_buf],
		plp->epcbase));
	    plp_startwrite(plp);

	    plp->wr_buf.sw_buf = PLP_NEXT(plp->wr_buf.sw_buf);
	    dprintf(("plpwrite: incremented sw to %d\n",
			plp->wr_buf.sw_buf));
	} else {
	    dprintf(("plpwrite: DMA already active.  hw = %d\n",
					plp->wr_buf.hw_buf));
	}

	UNLOCK(s);

    } /* while(uiop->uio_resid) */

    /* Sleep until the write completes.
     * As long as the port is still in write mode and the DMA lock
     * isn't free, there are still bytes to be written.  No one else
     * can get the DMA lock for writes until we release the write lock.
     */
    s = LOCK();
    while ((plp->dlock & PLP_BUSY) && !(plp->control & EPC_PPIN)) {
	dprintf(("plpwrite: sleeping (waiting for DMA completion)...\n"));
	UNLOCK(s);
	if (sleep ((caddr_t)&(plp->wr_buf.buf_lock), PUSER)) {
	    dprintf (("plpwrite: awoke from sleep interrupted\n"));
	    plp_unlock(&plp->wlock);
	    return EINTR;
	}

	s = LOCK ();
	if (plp->wr_buf.timeout_id == PLP_TIMEDOUT) {
	    int	rval;
	    UNLOCK (s);
	    rval = handle_part_write(plp, uiop);
	    dprintf (("plpwrite: awoke from sleep (timed out), "
		      "returning %d\n", rval));
	    return rval;
	}
	dprintf(("plpwrite: awoke from sleep normally, "
		 "plp->wr_buf.timeout_id == %d\n", plp->wr_buf.timeout_id));
    }
    UNLOCK(s);

    dprintf (("plpwrite: write complete, timeout_id %d\n",
		plp->wr_buf.timeout_id));
    if (plp->wr_buf.timeout_id) {
	untimeout(plp->wr_buf.timeout_id);
	plp->wr_buf.timeout_id = 0;
    }

    plp_unlock (&plp->wlock);

    return 0;
}


/*
 * plpioctl - control parallel port device
 */
/* ARGSUSED */
int
plpioctl (dev_t dev, int cmd, int arg, int mode, struct cred *crp, int *rvp)
{
    /* XXX - All the ioctl code needs to be rewritten */

     plp_t *plp = &plpport[minor(dev)];
    unsigned char ext;
    unsigned int pulse, setup, hold, dc;

    switch (cmd) {
    case PLPIOCRESET:
	dprintf(("plp: Resetting printer\n"));
	plp_reset (plp);
	break;
    case PLPIOCSTATUS:
	/* The PLPx bits are scrambled on Everest
	 * this keeps user programs compatible.
	 */
	ext = GET(EPC_PPSTAT);
	*rvp = (ext & EPC_PPFAULT ? 0 : PLPFAULT)
	      | (ext & EPC_PPNOINK ? PLPEOI : 0)
	      | (ext & EPC_PPNOPAPER ? PLPEOP : 0)
	      | (ext & EPC_PPONLINE ? PLPONLINE : 0);
	break;
    case PLPIOCTYPE:
	if (arg == PLP_TYPE_CENT) {
	    plp->control &= ~EPC_PPSTB_H;
	    plp->control &= ~EPC_PPACK_H;
	    plp->pulls |= EPC_PPSTRBPUL_H;
	} else if (arg == PLP_TYPE_VERS) {
	    plp->control |= EPC_PPSTB_H;
	    plp->control |= EPC_PPACK_H;
	    plp->pulls &= EPC_PPSTRBPUL_H;
	} else {
	    return EINVAL;
	}
	break;
    case PLPIOCSTROBE:
	/* First, we need to decode the HPC "rise," "fall," and "dc" times. */
	hold = (arg >> 8) & 0x7f;
	dc = (arg >> 24) & 0x7f;
	setup = dc - ((arg >> 16) & 0x7f);
	pulse = dc - setup - hold;
dprintf(("plp: fall = 0x%x, rise = 0x%x, hold = 0x%x, dc = 0x%x in 30ns incrs\n",
			setup, pulse, hold, dc));
dprintf(("plp: setup = %d ns, pulse = %d ns, hold = %d ns\n",
				setup * 30, pulse * 30, hold * 30));
	setup = (setup * 3 + 99) / 100;
	pulse = (pulse * 3 + 99) / 100;
	hold = (hold * 3 + 99) / 100;
dprintf(("plp: setup = %d us, pulse = %d us, hold = %d us\n", setup, pulse, hold));
	plp->control &= ~EPC_STBMASK;
	plp->control |= (unsigned int)PLP_ESTROBE(setup,pulse,hold);
	break;
    case PLPIOCESTROBE:
	plp->control &= ~EPC_STBMASK;
	plp->control |= (unsigned int)arg & EPC_STBMASK;
	break;
    case PLPIOCIGNACK:
	/* On Everest, this really controls BUSY mode, not ACK mode which is
	 * somewhat broken.
	 */
	if (arg) {
	    plp->control |= EPC_PPSACKMODE;
	    plp->control &= ~EPC_PPBUSYMODE;
	} else {
	    plp->control |= EPC_PPBUSYMODE;
	    plp->control &= ~EPC_PPSACKMODE;
	}
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
#ifdef PLP_READS
	    plp_read_flush (plp);		/* flush read buffer */
#else
		/* For now, this ioctl does nothing.  We don't have a parallel
		 * input device to test reads on.
		 */
#endif /* PLP_READS */
	    plp->state |= PLP_RDOPEN;
	}
	break;
    case FIONBIO:
	if (arg)
	    plp->state |= PLP_NBIO;
	else
	    plp->state &= ~PLP_NBIO;
	break;
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
plp_lock(volatile int *lockaddr)
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
plp_unlock(volatile int *lockaddr)
{
    *lockaddr &= ~PLP_BUSY;

    if (*lockaddr & PLP_WANTED) {
	*lockaddr &= ~PLP_WANTED;
	wakeup ((caddr_t)lockaddr);
	return 1;
    }
    return 0;
}


/* Initialize variables associated with buffers.
 * 	0 initializes read buffer's variables.
 * 	1 initializes write buffer's variables.
 */
static void
plp_initbufs(plp_t *plp, int start, int end)
{
    plpbuf_t *bufp;
    int i,j;

    /* initialize buffer variables */
    for (i = start; i <= end; i++) {
	if (i)
		bufp = &(plp->wr_buf);
	else
		bufp = &(plp->rd_buf);

	for (j = 0; j < PLP_NBUFS; j++) {
	    /* Leave "space" alone.  We don't free it. */
	    bufp->buf_len[j] = 0;
	    bufp->part_xfer[j] = 0;
	    bufp->buf_resid[j] = 0;
	}
	bufp->hw_buf = 0;
	bufp->sw_buf = 0;
	bufp->dma_active = 0;
	bufp->buf_lock = 0;
        bufp->copyflag = 0;
        bufp->timeout_id = 0;
	
	bufp->back_ptr = (caddr_t)plp;
    }

    dprintf(("plp: initbufs: 0x%x -> wr_buf == 0x%x,\n\t-> rd_buf == 0x%x\n",
					plp, &(plp->wr_buf), &(plp->rd_buf)));

    return;
}


static int handle_part_write(plp_t *plp, uio_t *uiop) {

    int transfer;
    int new_resid;

    /* The value to restore to uio_resid is the buf_resid of hw_buf +
     * whatever's in part_xfer.
     */

    new_resid = plp->wr_buf.buf_resid[plp->wr_buf.hw_buf] +
		plp->wr_buf.part_xfer[plp->wr_buf.hw_buf];


    dprintf(("plp: handle_part_write: buf_resid %d, part_xfer %d\n",
		plp->wr_buf.buf_resid[plp->wr_buf.hw_buf],
		plp->wr_buf.part_xfer[plp->wr_buf.hw_buf]));
    dprintf(("plp: handle_part_write: new_resid %d, init_resid %d\n",
		new_resid, plp->wr_buf.init_resid));

    transfer = plp->wr_buf.init_resid - new_resid;
    dprintf(("plp: handle_part_write: transferred %d out of %d bytes\n",
	    transfer, plp->wr_buf.init_resid));

    uiop->uio_resid = new_resid;

    /* Clear the "timed out" flag */
    plp->wr_buf.timeout_id = 0;

    /* Reinitialize the write buffer structure. */
    plp_initbufs(plp, 1, 1);

    plp_unlock (&plp->wlock);

    if (transfer)
	return 0;	/* moved SOME data */
    return EAGAIN;
}


#ifdef PLP_READS
/* flush read buffer */
void plp_read_flush (plp_t *plp)
{
    /* Clear the read buffers _only_ */
    plp_initbufs(plp, 0, 0);
}
#endif


#ifdef PLP_READS
/*
 * plpintr_read - initiate read from interrupt level
 */
static void
plpintr_read ( plp_t *plp, int resethpc)
{

/* XXX - This routine should either start DMA in or else schedule a timeout.
 * The reason for this is that if we're out of buffer space, we should 
 * schedule a timeout to try again when a buffer might be available.
 * Is this really true, or should we just count on getting an interrupt
 * and start another read as soon as the previous one is done...
 * It's different for the EPC than the HPC.
 */
/* The previous timeout code follows: */
	/* we ran out of rbufs.
	 * if a process was waiting to write, give up channel,
	 * otherwise, schedule a timeout for a dummy read.
	 */
	if (!plp_unlock (&plp->dlock))
	    plp->timeoutid = timeout(plpintr, plp, PLP_RTO);
	plp->dl = 0;

	/* Otherwise their code just started up a read...  */
	/* Wonder why _this_ was here... */
	if (resethpc) {
	    plp->regs->ctrl = PAR_CTRL_RESET;
	    plp->regs->ctrl = 0;
	}
	/* here's the actual read start-up after the port's changed direction and
	 * everything. (?)
	 */
	plp_dma (plp, rb, B_READ);
}
#endif /* PLP_READS */


/* plptout - The routine called by all timeouts
 */
static void
plptout(plpbuf_t *buf)
{
    int s;
    plp_t *plp;

    dprintf(("plp: plptout: Timeout! (id == 0x%x)\n", buf));
    /* Deal with the timeout: */

    /* Don't want interrupts in the middle of this */
    s = LOCK();

    dprintf(("plptout: Getting plp through back pointer (0x%x).\n",
						    buf->back_ptr));

    plp = (plp_t *)buf->back_ptr;

    /* Stop DMA iff it's going the direction the timer was set for. */

    dprintf(("plptout: Determining timeout direction.\n"));

    if (buf == &(plp->wr_buf)) {
	/* For writes: */

	dprintf(("plptout: Write.\n"));

	/* Stop DMA iff the port is writing (also sets part_xfer) */
	if (!(plp->control & EPC_PPIN)) {
	    dprintf(("plptout: Stopping a write!\n"));
	    plp_stop_dma(plp);

	    dprintf(("plptout: Write stopped!\n"));
	    /* Release the DMA lock */
	    plp_unlock(&plp->dlock);
	} else {
	    dprintf(("plptout: No write to stop!\n"));
	    /* IF DMA isn't going our way, nothing's been transferred */
	     buf->part_xfer[buf->hw_buf] = 0;
	}

    } else if (buf == &(plp->rd_buf)) {
	/* For reads: */

	dprintf(("plptout: Read.\n"));

	/* Stop DMA iff the port is reading (also sets part_xfer) */
	if (plp->control & EPC_PPIN) {
	    dprintf(("plptout: Stopping a read!\n"));
	    plp_stop_dma(plp);

	    /* Release the DMA lock */
	    plp_unlock(&plp->dlock);
	} else {
	    dprintf(("plptout: No read to stop!\n"));
	    /* IF DMA isn't going our way, nothing's been transferred */
	     buf->part_xfer[buf->hw_buf] = 0;
	}
	/* Consolidate the data. */
    } else {
	    printf("plptout Spurious timeout (id == 0x%x)\n", buf);
    }

    /* Set a flag indicating that an interrupt occurred.
     */
    buf->timeout_id = PLP_TIMEDOUT;

    /* Wakeup the user side to handle the partial write.
     */

    UNLOCK(s);
    dprintf(("plptout: Waking up user side\n"));
    wakeup(&(buf->buf_lock));
    dprintf(("plptout: Done!\n"));
}


/*
 * plpintr - parallel port device interrupt handler
 *
 */

/* ARGSUSED */
static void
plpintr(struct eframe_s *ep, unsigned int plp_pun)
{
    plp_t *plp;
    int	  s;

    dprintf (("plpintr: interrupt!\n"));
    s = LOCK ();

    plp  = &(plpport[plp_pun]);

    /* Is it a read? */
    if (plp->control & EPC_PPIN) {
	dprintf(("plpintr: read into buffer 0 complete.\n"));

	plp->rd_buf.dma_active = 0;

	/* Release DMA lock. */
        UNLOCK (s);
	plp_unlock (&plp->dlock);

	dprintf(("plpintr: Waking up user side.\n"));

	wakeup(&(plp->rd_buf.buf_lock));

	return;

    } else {
	dprintf(("plpintr: write of buffer %d complete.\n",
						    plp->wr_buf.hw_buf));

	/* Clear the length of this thing so we can use it again. */
	plp->wr_buf.buf_len[plp->wr_buf.hw_buf] = 0;

	/* Point at the next buffer */
	plp->wr_buf.hw_buf = PLP_NEXT(plp->wr_buf.hw_buf);

	/* Is there more to be written?  (Make sure no one's copying into
	 * the buffer we want to write right now.)
	 */

	if (plp->wr_buf.hw_buf == PLP_NEXT(plp->wr_buf.sw_buf)) {

	    /* Sit and wait for the user side to start us up. */
	    plp->wr_buf.dma_active = 0;

	    /* Release DMA lock. */
	    plp_unlock (&plp->dlock);

	    /* Increment the sw_buf. */
	    plp->wr_buf.sw_buf == PLP_NEXT(plp->wr_buf.sw_buf);

	    dprintf(("plpintr: Can't start a write (1): "
			"hw=%d, sw=%d, cpfl=%d, buflen %d\n",
				plp->wr_buf.hw_buf, plp->wr_buf.sw_buf,
				plp->wr_buf.copyflag,
				plp->wr_buf.buf_len[plp->wr_buf.hw_buf]));

	} else if ((plp->wr_buf.copyflag 
			    && plp->wr_buf.hw_buf == plp->wr_buf.sw_buf) ||
		    !plp->wr_buf.buf_len[plp->wr_buf.hw_buf]) {
	    /* Sit and wait for the user side to start us up. */

	    dprintf(("plpintr: Can't start a write (2): "
			"hw=%d, sw=%d, cpfl=%d, buflen %d\n",
				plp->wr_buf.hw_buf, plp->wr_buf.sw_buf,
				plp->wr_buf.copyflag,
				plp->wr_buf.buf_len[plp->wr_buf.hw_buf]));

	    plp->wr_buf.dma_active = 0;

	    /* Release DMA lock. */
	    plp_unlock (&plp->dlock);

	} else {

	    dprintf(("plpintr: Starting a write! (hw=%d, sw=%d hw_len=%d)\n",
				plp->wr_buf.hw_buf, plp->wr_buf.sw_buf,
				plp->wr_buf.buf_len[plp->wr_buf.hw_buf]));

	    if (plp->wr_buf.hw_buf == plp->wr_buf.sw_buf) {
		plp->wr_buf.sw_buf = PLP_NEXT(plp->wr_buf.sw_buf);

		dprintf(("plpintr: incremented sw_buf to %d\n",
			plp->wr_buf.sw_buf));
	    }
	    /* Begin DMA and reenable our interrupt.
	     */
	    plp_startwrite(plp);
	}

        UNLOCK (s);
	dprintf(("plpintr: Waking up user side.\n"));

	wakeup(&(plp->wr_buf.buf_lock));

    } /* if EPC_PPIN */

#if 0
     plp_t *plp = (plp_t *)plp_pun;
    int pcount;
    memd_t *dl, *desc;

    /* XXX - Once again, needs to be completely rewritten...
     * 	We'll need to duplicate some of this timeout stuff too.
     */

    /* read or write timed out
     */
    if (plp) {
	plp_stop_dma (plp);

	/* On a partial write, set an error flag. On a partial
	 * read, put the data onto the rbuf list for the channel. 
	 */
	/* XXX - look at part_xfer, copy, shift, reset len and part_xfer.
	 * reset dma_active, which we leave high until after all this.
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

#endif
}


static void 
plp_startread(plp_t *plp, int size)
{
    plp->rd_buf.dma_active = 1;

    plp->control |= EPC_PPIN;	/* We'll be reading */

    if (plp->pulls != plp->last_pulls) {
	SET(EPC_PRSTSET, ~plp->last_pulls & EPC_PPPULLMASK);
	SET(EPC_PRSTCLR, plp->last_pulls & EPC_PPPULLMASK);
	plp->last_pulls = plp->pulls;
    }

    /* Set the low part of the address.
     */
    SET(EPC_PPBASELO, K0_TO_PHYS(plp->wr_buf.buf_space[0]));

    dprintf(("plp:  Printing from 0x%x\n",
		K0_TO_PHYS(plp->wr_buf.buf_space[0])));

    /* Set the read length.
     */
    SET(EPC_PPLEN, size);

    dprintf(("plp:  Reading 0x%x bytes.\n", size));

    /* Set all of the parameters except for the START_DMA bit.
     */
    SET(EPC_PPCTRL, plp->control & ~(EPC_PPSTARTDMA));

    dprintf(("plp:  Writing 0x%x to EPC_PPCTRL.\n",
				plp->control & ~(EPC_PPSTARTDMA)));

    /* Enable parallel port interrupts.  The vector should still be stored
     * for us by plpinit.
     */
    SET(EPC_IMSET, EPC_INTR_PPORT);

    dprintf(("plp:  Enabling interrupts (Writing 0x%x)\n",
		EPC_INTR_PPORT));
    dprintf(("plp:  Starting DMA by writing 0x%x\n",
		plp->control | EPC_PPSTARTDMA));

    /* Let the DMA begin.
     */
    SET(EPC_PPCTRL, plp->control | EPC_PPSTARTDMA);

    return;
}


/* Start the blasted thing writing.  Assume it's stopped.
 * Must be called from spl5.
 */
static void
plp_startwrite(plp_t *plp)
{
    int hw_buf;
    char print_buf[81];

    hw_buf = plp->wr_buf.hw_buf;

    plp->wr_buf.dma_active = 1;

    plp->control &= ~EPC_PPIN;	/* We'll be writing */

    strncpy(print_buf, (char *)plp->wr_buf.buf_space[hw_buf],
				MIN(60, plp->wr_buf.buf_len[hw_buf]));
    print_buf[MIN(60, plp->wr_buf.buf_len[hw_buf])] = '\0';

    dprintf(("plp_startwrite: start = {%s}\n", print_buf));

    strncpy(print_buf, (char *)plp->wr_buf.buf_space[hw_buf] +
				plp->wr_buf.buf_len[hw_buf] -
				MIN(60, plp->wr_buf.buf_len[hw_buf]),
			MIN(60, plp->wr_buf.buf_len[hw_buf]));
    print_buf[MIN(60, plp->wr_buf.buf_len[hw_buf])] = '\0';

    dprintf(("plp_startwrite: end = {%s}\n", print_buf));

    if (plp->pulls != plp->last_pulls) {
	SET(EPC_PRSTSET, ~plp->last_pulls & EPC_PPPULLMASK);
	SET(EPC_PRSTCLR, plp->last_pulls & EPC_PPPULLMASK);
	plp->last_pulls = plp->pulls;
    }

    /* Set the low part of the address.
     */
    SET(EPC_PPBASELO, K0_TO_PHYS(plp->wr_buf.buf_space[hw_buf]));

    dprintf(("plp_startwrite: printing from 0x%x\n",
		K0_TO_PHYS(plp->wr_buf.buf_space[hw_buf])));

    /* Set the buffer length.
     */
    SET(EPC_PPLEN, plp->wr_buf.buf_len[hw_buf]);

    dprintf(("plp_startwrite: printing for 0x%x bytes.\n",
		plp->wr_buf.buf_len[hw_buf]));

    /* Set all of the parameters except for the START_DMA bit.
     */
    SET(EPC_PPCTRL, plp->control & ~(EPC_PPSTARTDMA));

    dprintf(("plp_startwrite: writing 0x%x to EPC_PPCTRL.\n",
				plp->control & ~(EPC_PPSTARTDMA)));

    /* Enable parallel port interrupts.  The vector should still be stored
     * for us by plpinit.
     */
    SET(EPC_IMSET, EPC_INTR_PPORT);

    dprintf(("plp_startwrite: enabling interrupts (Writing 0x%x)\n",
		EPC_INTR_PPORT));
    dprintf(("plp_startwrite: starting DMA by writing 0x%x\n",
		plp->control | EPC_PPSTARTDMA));

    /* Let the DMA begin.
     */
    SET(EPC_PPCTRL, plp->control | EPC_PPSTARTDMA);

    return;
}


#if 0
/*
 * plp_read_copy - copy data from a channel's input buffer list to vaddr
 */
static int 
plp_read_copy ( plp_t *plp, caddr_t vaddr, int nbytes)
{
    memd_t *rb;
    caddr_t cp = vaddr;
    int bcount = nbytes;

    /* XXX - needs to be rewritten */

    /* start with the first rbuf on the list 
     */
    LGETNODE (plp->rbufs, rb);
    while (rb && bcount) {
	/* if readbuf fits in read request, then
	 * copy data, and free rbuf
	 */
	if (rb->memd_bc <= bcount) {
	    bcopy (PHYS_TO_K1(rb->memd_buf), cp, rb->memd_bc);
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
	    bcopy (PHYS_TO_K1(rb->memd_buf), cp, bcount);
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

#endif

/* hardware functions for parallel interface
 */

#ifdef PLP_READS
/*
 * plp_dma - start dma on descriptor list dl and channel pointed to by plp
 *
 * dma channel must currently not be in use, dl is the
 * descriptor list for the transfer which is a physical address.
 */
/* ARGSUSED */
static void
plp_dma ( plp_t *plp, int flags)
{

    /* XXX - all new */
#if 0
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
#endif /* 0 */
    return;
}
#endif	/* PLP_READS */

/*
 * plp_stop_dma - stop current dma transfer
 *
 */
static void
plp_stop_dma(plp_t *plp)
{
    int s;

    /* Shut down DMA.
     */
    s = LOCK();
    SET(EPC_PPCTRL, plp->control & ~EPC_PPSTARTDMA);

    if (plp->control & EPC_PPIN) {
	/* input mode */
	plp->rd_buf.part_xfer[0] = GET(EPC_PPCOUNT);
	plp->rd_buf.dma_active = 0;
    } else {
	/* output mode */
	plp->wr_buf.part_xfer[plp->wr_buf.hw_buf] = GET(EPC_PPCOUNT);
	plp->wr_buf.dma_active = 0;
    }

    UNLOCK(s);

    /* Don't turn off the dma_active switch until we do whatever we're
     * going to do with the partially transferred data.
     */
    return;
}

/*
 * epc_probe - probe for EPC chips.
 *
 * returns the base address of the EPC in the slot requested or 0 if none
 */
caddr_t
epc_probe (int slot)
{
    evcfginfo_t *cfginfo;
    evbrdinfo_t *brd;
    int window;
    caddr_t epcbase;
    int i;

#if defined(MULTIKERNEL)
    /* All I/O boards belong ONLY to the golden cell */
    if (evmk_cellid != evmk_golden_cellid)
    	return(0);
#endif /* MULTIKERNEL */
    cfginfo = EVCFGINFO;

    brd = &(cfginfo->ecfg_board[slot]);

    /* If it ain't an IO board, there won't be an EPC chip
     */
    if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_IO)
	return 0;

    /* Is the IO4 enabled?
     */
    if (!brd->eb_enabled) {
	printf("plp: IO4 in slot %d disabled.\n", slot);
	return 0;
    }

    /* Check all physical adapters for EPC chips.  padap 0 is illegal.
     */
    for (i = 1; i < IO4_MAX_PADAPS; i++) {
	/* Is this adapter an EPC?
	 */
	if (brd->eb_ioarr[i].ioa_type == IO4_ADAP_EPC)
	    /* Is it enabled?
	     */
	    if (brd->eb_ioarr[i].ioa_enable) {
		window = brd->eb_io.eb_winnum;
		epcbase = (caddr_t)SWIN_BASE(window, i);
		dprintf(("plp: Found EPC in slot %d, padap %d, swin %d, base = %x\n",
			slot, i, window, epcbase));
		/* Found a live one.
		 */
		return epcbase;
	    } else {
		printf("plp: The EPC on IO4 in slot %d disabled.\n", slot);
		return 0;
	    }
    }

    /* If we're still here, we didn't find one.
     */
    return 0;
}


/*
 * plp_reset - reset the EPC parallel port interface
 */
static void
plp_reset( plp_t *plp)
{
    /* Can't reset the port while someone's doing DMA 
     */
    if (plp_lock (&plp->dlock))
	return;

    /* Shut down DMA, just in case.  Reset parameters.
     */
    SET(EPC_PPCTRL, plp->control & ~EPC_PPSTARTDMA);

    /* Pull RESET and PRT low.  (RESET is inverted.)
     */
    SETHALF((short)EPC_PPORT, EPC_PPRESET);

    /* Wait.
     */
    DELAY(400);

    /* Pull RESET and PRT back up.  (RESET is inverted.)
     */
    SETHALF((short)EPC_PPORT, EPC_PPPRT);

    plp_unlock (&plp->dlock);
    return;
}

#endif /* EVEREST */
