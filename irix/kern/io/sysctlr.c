#ident	"io/sysctlr.c:  $Revision: 1.19 $"

#if defined(EVEREST)
/* 
 * sysctlr.c - driver for the Everest System Controller
 * This driver is intended to be used only by the system controller daemon
 * for displaying power-meter data, fetching environmental information
 * and shutting down the system in an orderly fashion when the keyswitch
 * is turned off or the temperature approaches the automatic shutdown level.
 */

#include <values.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/immu.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/i8251uart.h>
#include <sys/systm.h>

int syscdevflag = 0;

#ifdef DEBUG
int sysctlr_debug = 0;
#define dprintf(x) if (sysctlr_debug) printf x
#define wprintf(x) if (sysctlr_debug & 2) printf x
#define rprintf(x) if (sysctlr_debug & 1) printf x
#else
#define	dprintf(x)
#define wprintf(x)
#define rprintf(x)
#endif

#define LOCK()		spl5()
#define UNLOCK(a)	splx(a)

#define SYSC_BUFSIZE	2048 	/* Must be a power of two */
#define SC_NEXT(a)	((++(a)) & (SYSC_BUFSIZE - 1))
/* Use a 2k buffer for starters */

/* Values for various locks */
#define SYSC_BUSY	0x1
#define SYSC_WANTED	0x2

/* Values for "state" */
#define SYSC_ALIVE	0x1	/* Driver has been successfully initialized */
#define SYSC_WROPEN	0x2	/* Open for writing */
#define SYSC_RDOPEN	0x4	/* Open for reading */

#define SYSC_TIMEOUT	(HZ)	/* Time out after 1 second. */
#define SYSC_TIMEDOUT	(toid_t)0

struct eframe_s;

/* Structure containing the definition of system controller read/write
 * buffers
 */
typedef struct sysc_buf_s {
    volatile int length;		/* Queue length */
    volatile int req;			/* Request length */
    unsigned int lock;			/* Wake me up here */
    volatile int head;			/* Head index for read queue */
    volatile int tail;			/* Tail index for read queue */
    volatile toid_t timeout_id;		/* Timeout number */
    volatile int timeout_val;		/* Timeout length */
    volatile char space[SYSC_BUFSIZE];	/* Buffer space */
} sysc_buf_t;


/* Structure containing all information about the current state of the
 * CC UART.  Since there can only be one, I've made it global.
 */
typedef struct sysc_s {
    volatile unsigned char state;	/* Software state of port */
    volatile unsigned int control;	/* Value for strobe/control reg. */
    volatile int rlock, wlock;		/* Read and write locks */
    volatile caddr_t ccuartbase;	/* Base address the CC UART */
    volatile sysc_buf_t rd_buf;		/* Read buffer structure */
    volatile sysc_buf_t wr_buf;		/* Write buffer structure */
} sysc_t;


/* The aforementioned global system controller structure. */
volatile sysc_t sysc;

/* Macros for doing doubleword access to the CC UART.  (They have to be
 * eight-byte read/writes even though we only use four bytes.
 */
#define SET(_r, _v)	store_double((long long *)(sysc.ccuartbase + (_r)),\
				(long long)(_v))
#define GET(_r)		(int)load_double((long long *)(sysc.ccuartbase + (_r)))

/* Get/release locks.
 */
static int sysc_lock(volatile int *);
static int sysc_unlock(volatile int *);

static void sysctout(sysc_buf_t *);
static void clear_tout(volatile toid_t *);

/* Offsets to the command/data registers of the UART from the base.
 */
#define         CMD     0x0
#define         DATA    0x8

/*
 * syscclear - Clear out the sysc structure.
 */
static void
sysc_clear(void)
{
	dprintf(("sysctlr: Setting up software\n"));

	sysc.rd_buf.length = 0;
	sysc.wr_buf.length = 0;
	sysc.rd_buf.timeout_id = 0;
	sysc.wr_buf.timeout_id = 0;
	sysc.rd_buf.timeout_val = SYSC_TIMEOUT;
	sysc.wr_buf.timeout_val = SYSC_TIMEOUT;
	sysc.rd_buf.tail = 0;
	sysc.rd_buf.head = 0;
	sysc.rd_buf.req = 0;
	sysc.wr_buf.req = 0;
}


/*
 * syscinit - initialize system controller driver
 */
void
syscinit(void)
{
	sysc.ccuartbase = (caddr_t)EV_UART_BASE;

	dprintf(("sysctlr: Initializing the UART\n"));

/* Right now, we're assuming that the PROM has initialized the port.
 */
#if SET_UP_THE_PORT
	/* SET UP THE PORT */

	/* Clear state by writing three zeroes */
	SET(CMD, 0);
	SET(CMD, 0);
	SET(CMD, 0);

	/* Reset the UART */
	SET(CMD, I8251_RESET);

	/* Set up the parameters */
	sysc.control = I8251_ASYNC16X|I8251_NOPAR|I8251_8BITS|I8251_STOPB1;
	SET(CMD, sysc.control);

	/* Soft reset */
	SET(CMD, I8251_RESET);

	/* Set up the parameters again */
	SET(CMD, sysc.control);

	/* Enable transmit, receive.  Reset errors */

        SET(CMD, I8251_TXENB | I8251_RXENB | I8251_RESETERR);

#endif /* SET_UP_THE_PORT */

	/* SET UP THE SOFTWARE STRUCTURES */

	sysc_clear();

	/* Tell the world we're alive */
	sysc.state |= SYSC_ALIVE;

	return;
}


/*
 * syscopen - open system controller device
 */
/* ARGSUSED */
int
syscopen (dev_t *devp, int mode)
{
    dprintf(("sysctlr: Opening.\n"));

    if (!(sysc.state & SYSC_ALIVE))
	return ENXIO;

    if (sysc.state & (SYSC_RDOPEN | SYSC_WROPEN))
	return EAGAIN;

    if (mode & FWRITE) {
	sysc.state |= SYSC_WROPEN;
	dprintf(("sysctlr: Open for write\n"));
    }

    if (mode & FREAD) {
	sysc.state |= SYSC_RDOPEN;
	dprintf(("sysctlr: Open for read\n"));
    }

    dprintf(("sysctlr: Succeeded.\n"));

    return 0;
}


/*
 * syscclose - close system controller device
 */
/* ARGSUSED */
int
syscclose (dev_t dev)
{
    int s;

    dprintf(("sysctlr: Closing\n"));

    s = LOCK();
    /* Clear any pending timeouts
     */

    clear_tout(&sysc.rd_buf.timeout_id);
    clear_tout(&sysc.wr_buf.timeout_id);

    sysc.state &= ~(SYSC_WROPEN | SYSC_RDOPEN);

    /* Release the locks
     */
    sysc_unlock (&sysc.rlock);
    sysc_unlock (&sysc.wlock);

    sysc_clear();

    UNLOCK(s);

    return 0;
}


/*
 * syscread - read from the system controller device
 */
/* ARGSUSED */
int
syscread (dev_t dev, uio_t *uiop)
{
    int s;
    int size;
    int move_bytes;
    int total;

    rprintf(("sc: READ\n"));

    /* get read lock */
    if (sysc_lock (&sysc.rlock)) {
	return EINTR;
    }


    total = 0;

    while (uiop->uio_resid) {
	rprintf(("resid=0x%x\n", uiop->uio_resid));

	s = LOCK();
	
	/* Snapshot the size */
	size = sysc.rd_buf.length;

	/* Copy what's in the buffer. */
	move_bytes = MIN(uiop->uio_resid, size);

	UNLOCK(s);


rprintf(("h=%d, t=%d, mb=%d, sz=%d\n",
		sysc.rd_buf.head, sysc.rd_buf.tail, move_bytes, size));

	if (move_bytes) {
	    rprintf(("Copying %d bytes (size == %d)\n", move_bytes, size));
	    if (size + move_bytes < SYSC_BUFSIZE) {
rprintf(("1 xfer %d to %d\n", sysc.rd_buf.tail, sysc.rd_buf.tail +
							    move_bytes - 1));
		uiomove((caddr_t)(sysc.rd_buf.space + sysc.rd_buf.tail),
					    move_bytes, UIO_READ, uiop);
		if (move_bytes == 1)
			rprintf(("char = 0x%x\n", *(sysc.rd_buf.space + sysc.rd_buf.tail)));
	    } else {
rprintf(("Xfer1 %d to %d\n", sysc.rd_buf.tail, SYSC_BUFSIZE - 1));
		uiomove((caddr_t)sysc.rd_buf.space + sysc.rd_buf.tail,
					    SYSC_BUFSIZE - sysc.rd_buf.tail,
					    UIO_READ, uiop);
rprintf(("Xfer2 %d to %d\n", 0,
			    move_bytes - SYSC_BUFSIZE + sysc.rd_buf.tail - 1));
		uiomove((caddr_t)sysc.rd_buf.space,
					    move_bytes - SYSC_BUFSIZE 
							    + sysc.rd_buf.tail,
					    UIO_READ, uiop);
	    }    
	}

	total += move_bytes;

	s = LOCK();

	sysc.rd_buf.length -= move_bytes;
	sysc.rd_buf.tail += move_bytes;
	sysc.rd_buf.tail &= (SYSC_BUFSIZE - 1);

	sysc.rd_buf.req = MIN(uiop->uio_resid, SYSC_BUFSIZE - 1);

	if (sysc.rd_buf.length < sysc.rd_buf.req) {
rprintf(("Sleeping for %d characters.\n", sysc.rd_buf.req - sysc.rd_buf.length));
rprintf(("req=%d, l=%d, h=%d, t=%d\n", sysc.rd_buf.req, sysc.rd_buf.length,
					sysc.rd_buf.head, sysc.rd_buf.tail));
	    sysc.rd_buf.timeout_id = timeout(sysctout, (char *)&(sysc.rd_buf),
					     sysc.rd_buf.timeout_val);
	    /* Sleep until this part is done. */
	    if (sleep ((caddr_t)&(sysc.rd_buf.lock), PUSER)) {
		clear_tout(&sysc.rd_buf.timeout_id);
		UNLOCK(s);
		sysc_unlock(&sysc.rlock);
		return EINTR;
	    }

	    if (sysc.rd_buf.timeout_id == SYSC_TIMEDOUT) {
		sysc.rd_buf.timeout_id = 0;
		sysc_unlock(&sysc.rlock);
rprintf(("req=%d, l=%d, h=%d, t=%d\n", sysc.rd_buf.req, sysc.rd_buf.length,
					sysc.rd_buf.head, sysc.rd_buf.tail));
		UNLOCK(s);
		return 0;
	    }

	} else {
rprintf(("req=%d\n", sysc.rd_buf.req));
	}

	clear_tout(&sysc.rd_buf.timeout_id);

	UNLOCK(s);
rprintf(("req=%d, l=%d, h=%d, t=%d\n", sysc.rd_buf.req, sysc.rd_buf.length,
					sysc.rd_buf.head, sysc.rd_buf.tail));
    }
    sysc_unlock (&sysc.rlock);

    rprintf(("sysc: RD DONE.\n"));

rprintf(("req=%d, l=%d, h=%d, t=%d\n", sysc.rd_buf.req, sysc.rd_buf.length,
					sysc.rd_buf.head, sysc.rd_buf.tail));
    return 0;
}


/*
 * syscwrite - write to system controller device
 */
/* ARGSUSED */
int
syscwrite (dev_t dev, uio_t *uiop)
{
    int s;
    int move_bytes;

    wprintf(("sysctlr: ENTERING WRITE\n"));

    /* get write lock */
    if (sysc_lock (&sysc.wlock)) {
	return EINTR;
    }

    while (uiop->uio_resid) {

	wprintf(("sysctlr: uio_resid = 0x%x\n", uiop->uio_resid));

	/* There can't be any writes going on now so no "LOCK" */
	move_bytes = MIN(uiop->uio_resid,
				SYSC_BUFSIZE - 1);

wprintf(("Move %d bytes\n", move_bytes));
	
        uiomove((caddr_t)sysc.wr_buf.space, move_bytes, UIO_WRITE, uiop);

	s = LOCK();

	/* We'll transfer one byte ourselves. */
	sysc.wr_buf.req = move_bytes - 1;

	sysc.wr_buf.length = 1;

wprintf(("Send byte.\n"));

	SET(DATA, sysc.wr_buf.space[0]);

wprintf(("Sleep.  req = %d, length = %d\n", sysc.wr_buf.req,
						sysc.wr_buf.length));

	sysc.wr_buf.timeout_id = timeout(sysctout, (char *)&(sysc.wr_buf),
					     sysc.wr_buf.timeout_val);

	/* Sleep until this part is done. */
	if (sleep ((caddr_t)&(sysc.wr_buf.lock), PUSER)) {
	    UNLOCK(s);
            sysc_unlock(&sysc.wlock);
            return EINTR;
        }

	if (sysc.wr_buf.timeout_id == SYSC_TIMEDOUT) {
	    sysc.wr_buf.timeout_id = 0;
	    uiop->uio_resid += sysc.wr_buf.req;
	    sysc_unlock(&sysc.wlock);
	    UNLOCK(s);
	    return 0;
	}

	clear_tout(&sysc.wr_buf.timeout_id);

        UNLOCK(s);

    } /* while(uiop->uio_resid) */

    sysc_unlock (&sysc.wlock);

    wprintf(("sysctlr: WT DONE. resid == %d\n", uiop->uio_resid));
    
    return 0;
}


/*
 * syscioctl - control system controller device
 */
/* ARGSUSED */
int
syscioctl (dev_t dev, int cmd, int arg, int mode, struct cred *crp, int *rvp)
{
    switch(cmd) {
	case SYSC_QLEN:
	    *rvp = sysc.rd_buf.length;
	    break;
	case SYSC_WTOUT:
	    if (arg <= 0)
		return EINVAL;
	    sysc.wr_buf.timeout_val = arg;   
	    break;
	case SYSC_RTOUT:
	    if (arg <= 0)
		return EINVAL;
	    sysc.rd_buf.timeout_val = arg;   
	    break;
	default:
	    return EINVAL;
    }

    return 0;
}


/* 
 * sysc_lock - protect read and write channels.
 *
 * returns 1 if it catches a signal before getting the lock
 * returns 0 if it gets the lock successfully
 */
static int 
sysc_lock(volatile int *lockaddr)
{
    int s = LOCK();

    while (*lockaddr & SYSC_BUSY) {
	*lockaddr |= SYSC_WANTED;
	if (sleep ((caddr_t)lockaddr, PUSER)) {
	    UNLOCK(s);
	    return 1;
	}
    }
    *lockaddr |= SYSC_BUSY;
    UNLOCK(s);
    return 0;
}


/* 
 * sysc_unlock - give up read and write channel access locks
 *
 * returns 1 if it wakes up another process waiting on the lock
 * returns 0 if no process gets awakened
 */
static int 
sysc_unlock(volatile int *lockaddr)
{
    *lockaddr &= ~SYSC_BUSY;

    if (*lockaddr & SYSC_WANTED) {
	*lockaddr &= ~SYSC_WANTED;
	wakeup ((caddr_t)lockaddr);
	return 1;
    }
    return 0;
}


/*
 * syscintr - System controller port device interrupt handler
 *
 */
/* ARGSUSED */
void
syscintr(struct eframe_s *ep)
{
    int state;

    /* If error occurs early in boot, sysctlr might not be initialized */
    if (sysc.ccuartbase == 0)
	syscinit();

    /* Get UART status */
    state = GET(CMD);

    /* If a character is present, read it into the buffer. */
    if (state & I8251_RXRDY) {
/* rprintf(("Reading a character: sysc.rd_buf.req = %d\n", sysc.rd_buf.req)); */
	if (sysc.rd_buf.length <= SYSC_BUFSIZE - 1) {
	    sysc.rd_buf.space[sysc.rd_buf.head] = GET(DATA);
/* rprintf(("R %c.\n", sysc.rd_buf.space[sysc.rd_buf.length])); */
	    sysc.rd_buf.head = SC_NEXT(sysc.rd_buf.head);
	    sysc.rd_buf.length++;

	    if (sysc.rd_buf.length == sysc.rd_buf.req) {
/* rprintf(("Wake rd: l == %d\n", sysc.rd_buf.length)); */
		wakeup((char *)&(sysc.rd_buf.lock));
	    }
	} else {
	    printf("sysctlr: Read overflow!\n");
	    wakeup((char *)&sysc.rd_buf.lock);
	}
    }


    /* If there are characters to transmit and the UART is ready,
     * transmit one.
     */
    if ((state & I8251_TXRDY) && (sysc.wr_buf.req > 0)) {
	/* Transmit a character */
	SET(DATA, sysc.wr_buf.space[sysc.wr_buf.length]);
	/* Adjust data structures */
	sysc.wr_buf.length++;
	sysc.wr_buf.req--;
	if (sysc.wr_buf.req <= 0)
   	    wakeup((char *)&sysc.wr_buf.lock); 
    }

    /* Turn the interrupt back off. */
    EV_SET_LOCAL(EV_CIPL124, EV_UARTINT_MASK);

    return;
}


static void sysctout(sysc_buf_t *buf)
{

	dprintf(("sysc: tout!\n"));

	/* Tell user side  we timed out. */
	buf->timeout_id = SYSC_TIMEDOUT;

	/* Wake it up */
	wakeup(&(buf->lock));
	return;
}

static void clear_tout(volatile toid_t *id)
{
	if (*id && (*id != SYSC_TIMEDOUT))
		untimeout(*id);
	*id = 0;
}

#endif /* EVEREST */

