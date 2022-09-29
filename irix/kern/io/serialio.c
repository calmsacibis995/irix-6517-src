#ident	"$Revision: 1.123 $"
/*
 * Copyright (C) 1986, 1992, 1993, 1994, 1995 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/* Streams upper layer for modular serial i/o driver. This file handles
 * all hardware independent operations for serial i/o.
 *
 * All serial devices regardless of hardware type will share a common 
 * major device number. Different hardware types will have separate
 * non-overlapping minor device number ranges. The offset of a minor
 * number within its range will determine the instance of the device
 * of that particular type.
 *
 * Two high order bits in the minor device number will determine if
 * the port is a plain, modem or flow-modem port.
 */

#include "sys/debug.h"
#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/stream.h"
#include "sys/strids.h"
#include "sys/strmp.h"
#include "sys/stropts.h"
#include "sys/stty_ld.h"
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/termio.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sema.h"
#include "sys/ddi.h"
#include "sys/pio.h"
#include "sys/cred.h"
#include "ksys/serialio.h"
#include "sys/serialio.h"
#include "sys/capability.h"
#include "sys/cmn_err.h"
#include "sys/kopt.h"
#include <sys/kmem.h>
#include <sys/driver.h>
#include <sys/runq.h>
#include <sys/invent.h>
#if DEBUG
#include <sys/idbgentry.h>
#endif

#include <sys/iograph.h>
#include <sys/scsi.h>
#include <sys/hwgraph.h>
#include "sys/kthread.h"
#include "ksys/sthread.h"

#if IP30 || SN || IP32 || EVEREST
#define IS_DU	1
#endif	/* IP30 */

#define IOCARG(ptr) (*(int*)(ptr))

/* define which systems use the mmsc features in this file */
#ifdef SN0
#  define MMSC 1
#endif

/* This file provides support for Multi-Module System Controller
 * (MMSC) control as well as normal serial port input/output. On
 * systems with an MMSC, the console port is used both as a normal
 * console tty port and as a communications port between the system
 * controller daemon (mmscd) on the host and the MMSC hardware. In
 * order to allow this dual role, the console port is cabled directly
 * to the MMSC, and a serial port on the MMSC is used as the actual
 * console port for the machine. Ordinary input/output is passed
 * straight through the MMSC. An escape sequence exists to allow
 * output from the mmscd to be sent to the MMSC without being passed
 * on to the actual console, and to allow the MMSC to send input data
 * to the mmscd without it being passed to the console tty port.
 *
 * This functionality is provided by a secondary device file which
 * accesses the console port. Ordinary processes continue to access
 * the console device through /dev/ttyd1 as usual. The console port
 * may simultaneously be accessed by the mmscd through the file
 * /hw/machdep/mmsc_control. When /hw/machdep/mmsc_control is closed,
 * /dev/ttyd1 behaves exactly like any other serial port. When
 * /hw/machdep/mmsc_control is opened, there are some differences to
 * allow the mmscd to communicate with the MMSC.
 *
 * When /hw/machdep/mmsc_control is open, the serial driver begins
 * honoring the MMSC escape sequence protocol. On output, any instance
 * of the escape character sent to /dev/ttyd1 must be sent to the
 * hardware twice, instructing the MMSC to pass it straight
 * through. When the mmscd wishes to send a packet to the MMSC, it
 * first issues an ioctl to /hw/machdep/mmsc_control which instructs
 * the driver to suspend all output from /dev/ttyd1 as well as from
 * kernel printfs. (The MMSC packet must not be interleaved with any
 * other data or the MMSC protocol will be corrupted). The mmscd
 * then writes an MMSC packet, the first character of which is the
 * escape char, to /hw/machdep/mmsc_control. The data is passed
 * straight through to the MMSC. When output is done, the mmscd
 * issues an ioctl to allow output from /dev/ttyd1 and kernel printfs
 * to resume. If /hw/machdep/mmsc_control is closed, normal output is
 * automatically resumed.
 *
 * Input occurs normally if /hw/machdep/mmsc_control is not open. If
 * /hw/machdep/mmsc_control is open, the serial driver scans incoming
 * data for escaped packets and sends them up to
 * /hw/machdep/mmsc_control where they are read by the mmscd. All
 * other incoming data is passed up to /dev/ttyd1. If the MMSC
 * receives an esc char from the user on the real console device, it
 * sends the esc char to the host twice. The serial driver recognizes
 * this as an escaped esc char and passes one of the chars up to
 * /dev/ttyd1 and discards the other. If an esc char is received and
 * is not followed by another esc char, MMSC packet reception
 * begins. A state machine keeps track of the input as it comes
 * in. This machine must have knowledge of the packet format in order
 * to know when the packet ends. When the packet ends it is sent up to
 * /hw/machdep/mmsc_control and normal input processing resumes. The
 * packet which is sent up to /hw/machdep/mmsc_control is stripped of
 * its leading esc char.
 *
 * The portions of the MMSC packet format that this driver needs to
 * know are as follows (XX == don't care)
 *
 * bytes	meaning
 *     1	esc char
 *     1	XX
 *     2	length of data field (MSB, LSB)
 * 0-65535	data field
 *     1	XX
 *
 * Implementation of these two port types is somewhat
 * complex. Previously it has not been possible to open two port types
 * to the same device (e.g. ttyd1 and ttym1) because we couldn't
 * connect two streams to the same port. The MMSC features allow for
 * two distinct streams to connect to the same port by adding
 * additional queue pointers, etc, to the port structure. In addition,
 * a special port type is added to the hwgraph to allow the driver to
 * distinguish between /dev/ttyd1 and /hw/machdep/mmsc_control. The
 * port open protocol is executed whenever the first of these two
 * ports is opened, and the close protocol is only executed when both
 * ports are closed. There are two sets of output queues. Which queue
 * sends data to the hardware is decided by the aforementioned
 * ioctl. When the mmscd issues an ioctl instructing output from
 * /dev/ttyd1 to pause, this also instructs the serial driver to start
 * sending output from the queue for
 * /hw/machdep/mmsc_control. Inversly when the ioctl is issued to
 * resume output from /dev/ttyd1, the queue for /dev/ttyd1 supplies
 * the output data. On input, there are two sets of input buffer
 * pointers. When the beginning of an MMSC packet is detected, the
 * currently accumulated input data is flushed to the input buffer for
 * /dev/ttyd1. All data received thereafter goes to the buffer for
 * /hw/machdep/mmsc_control until the packet is finished. At that
 * point current data is flushed to that buffer, and data flow reverts
 * to the buffer for /dev/ttyd1. Input queues are enabled based on
 * whether or not their respective buffers contain any data.  */

/* a tty port can have preset attributes when it is opened. For
 * example a modem port will have modem signals, a flow control port
 * will have flow control signals, etc.
 */
#define	SIO_ATTR_MODEM	0x01
#define	SIO_ATTR_FLOW	0x02
#define	SIO_ATTR_RS422	0x04
#if MMSC
#  define SIO_ATTR_MMSC	0x08
#endif

typedef struct siotype_s {
    int			attrs;
    sioport_t	       *port;
} siotype_t;

#ifdef IS_DU
#define du_port ((sioport_t*)the_console_port)	/* pointer to console port */
static int def_console_baud = B9600;	/* console set baud rate */
#define isconsole(p)	(p == du_port)	/* port is console port */
void *the_console_port;
void *the_kdbx_port;
#endif


#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

void serial_initport(mutex_t *);
int serial_lock(mutex_t *, int);
int serial_unlock(mutex_t *, int);

/*    'extern'  stuff ...  */
extern int kdebugbreak;
extern struct stty_ld def_stty_ld;
extern int duart_rsrv_duration;         /* send input this often */

/* default cflags */
#define DEF_CFLAG 		((ushort_t)(CREAD | def_stty_ld.st_cflag))
#define CNTRL_A		'\001'

/*   Misc. defines ..  */
#define MIN_RMSG_LEN    4               /* minimum buffer size */
#define MAX_RMSG_LEN    25600           /* largest msg allowed */
#define XOFF_RMSG_LEN   256             /* send XOFF here */
#define MAX_RBUF_LEN    12800
#define MAX_RSRV_CNT    3               /* continue input timer this long */

/* Define the smallest power of 2 greater than the maximum number
 * of ports of all types that the system can have at once:
 */
#define MAXPORTS	1024               /* must be (2**n) */

/* The two lowest order unused bits in the minor number will specify
 * a modem or flow-modem port. For example, if MAXPORTS is 1024, bit
 * 10 indicates a modem port, bit 11 indicates a flow modem port
 */
#define PORT(dev)	((dev) & ((MAXPORTS) - 1))

/*
 * the "type" field of the "siotype" structure tells us whether we are
 * a normal port, a MODEM port, or a FLOW port. On SN0 there is also a
 * type for the MMSC control port which shares the console device
 */
#define ISMODEM(typep)	((typep)->attrs & SIO_ATTR_MODEM)
#define	ISFLOW(typep)	((typep)->attrs & SIO_ATTR_FLOW)
#define IS422(typep)	((typep)->attrs & SIO_ATTR_RS422)
#if MMSC
#  define ISMMSC(typep)	((typep)->attrs & SIO_ATTR_MMSC)
#endif

#define SEND_XON        1
#define SEND_XOFF       0
#define IGNORE_IXOFF    1


/* sio_termio  various fields... */
#define sio_iflag sio_termio.c_iflag      /* use some of the bits (see below) */
#define sio_cflag sio_termio.c_cflag      /* use all of the standard bits */
#define sio_ospeed sio_termio.c_ospeed    /* output speed */
#define sio_ispeed sio_termio.c_ispeed    /* input speed */
#define sio_line  sio_termio.c_line       /* line discipline */
#define sio_cc    sio_termio.c_cc         /* control characters */

/* bits in sio_state */
#define SIO_ISOPEN	0x1L		/* device is open */
#define SIO_WOPEN	0x2L		/* waiting for carrier */
#define SIO_DCD		0x4L		/* we have carrier */
#define SIO_TIMEOUT	0x8L		/* delaying */

#define SIO_BREAK	0x10L		/* breaking */
#define SIO_BREAK_QUIET 0x20L		/* finishing break */
#define SIO_TXSTOP	0x40L		/* output stopped by received XOFF */
#define SIO_LIT		0x80L		/* have seen literal character */

#define SIO_BLOCK	0x100L		/* XOFF sent because input full */
#define SIO_TX_TXON	0x200L		/* need to send XON */
#define SIO_TX_TXOFF	0x400L		/* need to send XOFF */
#define SIO_SE_PENDING	0x800L		/* buffer alloc event pending */

#define SIO_AUTO_HFC	0x1000L		/* lower level provides HFC */
#define SIO_RX_STRINTR	0x2000L		/* RX streams intr pending on port */
#define SIO_TX_STRINTR	0x4000L		/* TX streams intr pending on port */
#define SIO_TX_NOTIFY	0x8000L		/* TX lowat notification enabled */

#define SIO_BREAK_ON	0x10000L	/* receiving break */
#define SIO_CTS		0x20000L	/* CTS on */
#define SIO_WCLOSE	0x40000L	/* waiting for output to drain */
#define SIO_HW_ERR	0x80000L	/* hardware error occurred */

#define SIO_HW_ERR_SENT	0x100000L	/* hardware error processed */
#define SIO_CLOGGED	0x200000L	/* input is clogged */
#define SIO_DTR		0x400000L	/* DTR is asserted */
#define SIO_RTS		0x800000L	/* RTS is asserted */

#define SIO_EXTCLK	0x1000000L	/* external clock mode */
#define SIO_RS422	0x2000000L	/* RS422 compatibility mode */
#define SIO_FLOW	0x4000000L	/* do hardware flow control */
#define SIO_MODEM	0x8000000L	/* modem device */

#if MMSC
# define SIO_USE_MMSC	0x10000000L	/* when true get output from mmsc queue */
# define SIO_CONT_MMSC	0x20000000L	/* when true continue mmsc output */
# define SIO_MMSC_OUT	0x40000000L	/* current output is within an mmsc packet */
# define SIO_MMSC_IN	0x80000000L	/* current input is within an mmsc packet */

# define SIO_MMSC_ALL	(SIO_USE_MMSC | SIO_CONT_MMSC | \
			 SIO_MMSC_OUT | SIO_MMSC_IN)

#define SIO_PRINTF	0x100000000L	/* kernel printf owns console */
#endif

/* use software driven hardware flow control if user requests HFC and
 * lower layer doesn't support it
 */
#define USE_SOFT_HFC(upper) (((upper)->sio_cflag & CNEW_RTSCTS) && \
			     !((upper)->sio_state & SIO_AUTO_HFC))

#if MMSC
#  ifdef MMSC_ASCII_DEBUG
#    define MMSC_ESCAPE_CHAR 't'
#  else
#    define MMSC_ESCAPE_CHAR 0xa0
#  endif
#  define GETBP(port, pri) sio_getbp(port, pri, 0)
#  define RMSG_LEN(upper) ((upper)->sio_rmsg_len + (upper)->sio_rmsg_mmsc_len)
#else
#  define GETBP(port, pri) sio_getbp(port, pri)
#  define RMSG_LEN(upper) ((upper)->sio_rmsg_len)
#endif

/*
 *   Driver Entry routines in this file
 */
static void	sio_rsrv(queue_t *);
static int	sio_open(queue_t *, dev_t *, int, int, struct cred *);
static int	sio_close(queue_t *, int, struct cred *);
static int	sio_wput(queue_t *, mblk_t *);

static void	sio_con(sioport_t *);
static void	sio_coff(sioport_t *);
static void	sio_flushw(sioport_t *);
static void	sio_flushr(struct sioport *);
#if MMSC
static mblk_t  *sio_getbp(sioport_t *, uint_t, int);
#else
static mblk_t  *sio_getbp(sioport_t *, uint_t);
#endif
static void	sio_rx(sioport_t *);
static void	sio_tx(sioport_t *);
static void	sio_post_ncs(sioport_t *, int);
static void	sio_dDCD(sioport_t *, int);
static void	sio_dCTS(sioport_t *, int);
static void	sio_detach(sioport_t *port);
static void	sio_data_ready(sioport_t *);
static void	sio_output_lowat(sioport_t *);

/* callup vector for this streams upper layer */
static struct serial_callup sio_callup = {
    sio_data_ready,
    sio_output_lowat,
    sio_post_ncs,
    sio_dDCD,
    sio_dCTS,
    sio_detach
};

/* circular i/o buffer constants */
#define CBUFLEN 256
typedef ushort_t sio_pc_t; /* cbuf prod/cons pointer */

/* must be power of 2 */
#if CBUFLEN & (CBUFLEN - 1)
choke;
#endif

#define CBUFMASK (CBUFLEN-1)
#define CBUF_TX_EMPTY(upper) ((upper)->sio_tx_cons == (upper)->sio_tx_prod)

/* upper layer per port private data */
struct upper {
    sv_t sio_sv;

    /* transmit/receive buffers used to keep data flowing to/from
     * the hardware at interrupt time without having to wait for
     * a potentially slow upper half to wake up, get streams monitor
     * or whatever else it may have to do
     */
    sio_pc_t sio_tx_prod, sio_tx_cons;
    sio_pc_t sio_rx_prod, sio_rx_cons;

    char sio_tx_data[CBUFLEN];
    char sio_rx_data[CBUFLEN];
    char sio_rx_status[CBUFLEN];
    
    uchar_t sio_rsrv_cnt;	/* input timer count */
    int	sio_rsrv_duration;	/* input timer */
    
    ulong_t sio_state;		/* current state */
    
    struct termio sio_termio;
    queue_t *sio_rq, *sio_wq;	/* our queues */
    mblk_t *sio_rmsg, *sio_rmsge;	/* current input message, head/tail */
    int	sio_rmsg_len;		/* current input message length */
    int	sio_rbsize;		/* recent input message length */
    mblk_t *sio_rbp;		/* current input buffer */
    mblk_t *sio_wbp;		/* current output buffer */
    
    int	sio_tid;		/* (recent) output delay timer ID */
    int	sio_allocb_fail;	/* losses due to allocb() failures */
    int sio_ncs;		/* next char state */
    int	sio_fe, sio_over;	/* framing/overrun errors counts */

#if MMSC
    queue_t *sio_mmsc_rq;	/* queues for MMSC port */
    queue_t *sio_mmsc_wq;

    mblk_t *sio_rmsg_mmsc;	/* current input mmsc packet */
    mblk_t *sio_rmsge_mmsc;

    int sio_rmsg_mmsc_len;	/* current mmsc input packet len */

    enum fs {			/* states for mmsc input packet */
	FS_NORMAL,		/* recognition state machine.   */
	FS_SAW_ESC,
	FS_SAW_STATUS,
	FS_SAW_LEN_MSB,
	FS_SAW_LEN_LSB,
	FS_DONE
    } sio_mmsc_state;
    int sio_mmsc_packet_len;	/* remaining bytes to read into mmsc packet */
#endif
#ifdef MEASURE_XOFF_SKID
    int skidbytes;
    int sendbytes;
#endif
};

#define UPPER(port) ((struct upper*)(port)->sio_upper)

#define RX_PTRS_VALID(upper) \
((sio_pc_t)((upper)->sio_rx_prod - (upper)->sio_rx_cons) <= CBUFLEN)

#define TX_PTRS_VALID(upper) \
((sio_pc_t)((upper)->sio_tx_prod - (upper)->sio_tx_cons) <= CBUFLEN)

/*
 **************************************************************** 
 *  Unix STREAM device driver required structures and data.     *
 ****************************************************************
 *  The required structure are:			                *	
 *     - Module Info					        *
 *     - qinit for Read and Write (In and Out) queues           *
 *     - streamtab structure to point to all above              *
 *  Refer to your Unix Device Driver manual for more info       *
 **************************************************************** 
*/

/*   Module Info structure   */
static struct module_info siom_info = 
{
	STRID_SIO,			/* module ID */
	"sio",				/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* maximum packet size--infinite */
	128,				/* hi-water mark */
	16,				/* lo-water mark */
};

/*  'qinit' for  Input queue (Read queue)    */
static struct qinit siorinit = {
	NULL, (int (*)())sio_rsrv, sio_open, sio_close, NULL, &siom_info, NULL
};

/*  'qinit' for Output queue (Write queue)   */
static struct qinit siowinit = {
	sio_wput, NULL, NULL, NULL, NULL, &siom_info, NULL
};

/*  'streamtab' for the drive (put all above together)  */
#ifdef IS_DU
#define DEV_PREFIX "du"
int dudevflag = 0;
struct streamtab duinfo = {
#else
#define DEV_PREFIX "sio_"
int sio_devflag = 0;
struct streamtab sio_info = {
#endif
	&siorinit, &siowinit, NULL, NULL
};

static int sio_config(sioport_t *, tcflag_t, speed_t, struct termio *);
static void sio_iflow(sioport_t *, int, int);
static int  sio_start_rx(sioport_t *);
static void sio_start_tx(sioport_t *);
static void sio_stop_rx(sioport_t *, int);
static void sio_stop_tx(sioport_t *);

static splfunc_t sio_lock_funcs[] = {0, 0, splhi, spl7};

#ifdef DEBUG
static void debug_unlock(struct sio_lock *);
#endif

#define SIO_LOCK_LEVEL SIO_LOCK_MUTEX

void
sio_init_locks(struct sio_lock *lockp)
{
    spinlock_init(&lockp->spinlock, 0);
    mutex_init(&lockp->mutex, MUTEX_DEFAULT, 0);
    lockp->flags = 0;
    lockp->lock_levels[1] = lockp->lock_levels[2] = SIO_LOCK_UNUSED;
    lockp->max_level = lockp->lock_levels[0];

#ifdef DEBUG
    lockp->lock_cpu = -1;
    lockp->lock_kptr = 0;
    lockp->lock_ra = 0;
#endif
}

/*
 * SERIALIO port locking.
 * 
 * Serialio port locking presents several problems:
 * 
 * 1) We would like to be able to lock with the least obtrusive
 * locking mechanism allowable in all situations. For some situations
 * this may mean a mutex, for others it may mean a spinlock. We would
 * prefer a mutex, and if a spinlock is required we would prefer to
 * keep the spl level as low as possible. For the implementation,
 * we'll define a hierarchy of "locking levels", each of which
 * provides more protection than the last. The lowest level is the
 * mutex, or sleeping lock, and this is followed by spinlocks with
 * increasing spl level.
 *
 * 2) When a port is open, the upper and lower layers associated with
 * it may each have their own requirements for the lock. For example a
 * lower layer which uses an ithread may use a sleeping lock, whereas
 * a lower layer which interrupts at spl7 will require a spinlock at
 * spl7. An upper layer which never interrupts will be able to use a
 * sleeping lock, but an upper layer which locks the port from a
 * timeout may need a spinlock if the timeout is not run from a
 * thread, and may need spl7 if the timeout is from the profiling
 * interrupt. The type of lock needed is a dynamic thing which depends
 * on both the upper and lower layer.
 *
 * 3) There are some unfortunate cases where hardware requires the
 * sharing of a common lock between two adjacent uarts since they have
 * common registers which may not be accessed atomically. This is the
 * case with the Zilog 85x30. In this case, the port lock must be
 * shared between a lower layer and as many as two upper layers, since
 * each port may have a distinct upper layer opening it. The lock used
 * must meet the the highest locking level of the three potential
 * lockers (1 lower layer, 2 upper layers). This presents problems
 * when opening and closing a port with an upper layer that has a
 * higher locking level than those currently in use by the other upper
 * layer and the lower layer.
 *
 * 3a) We must be able to upgrade a lock on a port which is already
 * open, and potentially even has the lock currently held at a lower
 * locking level.
 *
 * 3b) We must also provide a means for the lower layer to notify the
 * upper layer locking routines that the same lock is to be used for
 * two distinct ports.
 *
 * 4) As a special case, we must be able to call kernel printf from an
 * arbitrarily high level interrupt, so the console port must always
 * be protected with an spl7 spinlock, the highest possible locking
 * level.
 *
 * These problems are solved as follows:
 *
 * First, problem 3b is solved by having the lower layer allocate a
 * locking structure (struct sio_lock defined in serialio.h) as it
 * discovers its ports. A pointer in the sioport_t is then set to
 * point to this structure. Lower layers which allow distinct locks
 * for distinct ports will allocate one of these structures per port,
 * whereas the 8530 will use the same struct for adjacent ports.
 *
 * For problems 1 & 2, we store up to 3 lock level requirements for
 * the 3 potential holders of this lock. When the lower layer is
 * creating the ports, it sets the first lock level to whatever it
 * requires.  This value is never changed thereafter. To solve problem
 * 4, if this port is the console port, the lower layer lock level is
 * set to the highest possible value. When an upper layer attaches
 * itself to a port, it allocates one of the two remaining slots and
 * sets its minimum locking requirement. We also store the greatest of
 * these 3 levels, the max_level, or the highest locking level
 * required to lock the port. This is the level at which the port must
 * be locked in order to satisfy all parties.
 *
 * Since the max_level is a dynamic value which may change depending
 * on the upper layers accessing the lock, the lock is considered held
 * only if the max_level observed before acquiring the lock is
 * unchanged after the lock is acquired. If the max_level was changed
 * during lock acquisition, whatever lock was acquired is released and
 * the (possibly different) lock is reacquired.
 *
 * Since the lock is only considered held if the max_level present
 * after acquiring the lock is the same as that used when acquiring
 * the lock, the lock is implicitly released if max_level is changed
 * by the lock holder.  Consequently if the lock is upgraded or
 * downgraded by the lock holder by changing max_level, the lock
 * holder must not subsequently do anything other than immediately
 * release whetever lock it held, and it must do so without modifying
 * any of the members in the lock struct after max_level has been
 * changed (other than the lock itself). This is handled automatically
 * by the lock management routines.
 *
 * To solve problem 3a, we must go through an opening protocol any
 * time a new upper layer is opened for the first time. If the upper
 * layer requires a locking level greater than the minimum, it must
 * upgrade the lock. This is done *after* setting the callup pointer,
 * since the lock may be released during upgrading, and a non-zero
 * callup pointer ensures that no other upper layer can race in and
 * grab the port. This upgrade is done *only* on the first open of the
 * port, and a corresponding downgrade is done when the port is closed
 * for the last time.
 * 
 * The locking level for a port must always guard against any
 * interrupts which may occur for that port. For the lower layer
 * hardware interrupts, this is trivial since the lower layer
 * allocates the lock struct and stuffs its locking requirement before
 * enabling the hardware interrupts. For the upper layers, it is
 * required that the upper layer go through the standard opening
 * protocol which may upgrade the lock to its requirements, before
 * scheduling any timeouts which may exceed the previously set lock
 * level. When closing the port, the upper layer must deactivate any
 * high level timeouts before possibly downgrading the lock.
 *
 */

/*
 * Ensure a lock is of at least the given lock_level. Upgrade it if it
 * isn't.
 */
void
sio_upgrade_lock(sioport_t *port, int lock_level)
{
    struct sio_lock *lockp = port->sio_lock;
    int i, was_spinlock, s;

    ASSERT(sio_port_islocked(port));

    /* Find a free lock level slot to store our level */
    if (lockp->lock_levels[1] == SIO_LOCK_UNUSED)
	i = 1;
    else
	i = 2;
    ASSERT(lockp->lock_levels[i] == SIO_LOCK_UNUSED);
    lockp->lock_levels[i] = lock_level;

    /* If the lock does not need to be upgraded, we're done */
    if (lockp->max_level >= lock_level)
	return;

    /* release the currently held lock. We must do all modifications
     * of the lock struct before changing max_level
     */
    if (lockp->flags & SIO_SPINLOCK_HELD) {
	lockp->flags &= ~SIO_SPINLOCK_HELD;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	s = lockp->token;
	was_spinlock = 1;
    }
    else {
	lockp->flags &= ~SIO_MUTEX_HELD;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	was_spinlock = 0;
    }

#ifdef DEBUG
    debug_unlock(lockp);
#endif

    /* Once we change max_level, the port is effectively no longer
     * locked, and it is not safe to do anything other than release
     * the currently held lock. Note that we even get a local copy of
     * the spinlock token and reset flags before doing this
     */
    lockp->max_level = lock_level;

    if (was_spinlock)
	mutex_spinunlock(&lockp->spinlock, s);
    else
	mutex_unlock(&lockp->mutex);

    /* reacquire the upgraded lock */
    SIO_LOCK_PORT(port);
    ASSERT(lockp->max_level >= lock_level);
}

/* possibly downgrade and unlock a lock
 */
void
sio_downgrade_unlock_lock(sioport_t *port, int lock_level)
{
    struct sio_lock *lockp = port->sio_lock;
    int i, max, s, was_spinlock;

    ASSERT(sio_port_islocked(port));

    /* Find our stored lock level and clear it */
    if (lockp->lock_levels[1] == lock_level)
	i = 1;
    else
	i = 2;
    ASSERT(lockp->lock_levels[i] == lock_level);
    lockp->lock_levels[i] = SIO_LOCK_UNUSED;
    
    /* find the new max_level now that our entry is cleared */
    max = SIO_LOCK_UNUSED;
    for(i = 0; i < 3; i++)
	if (lockp->lock_levels[i] > max)
	    max = lockp->lock_levels[i];
    
    /* do the portion of the port unlocking that requires modification
     * of lock struct members that we can't do once max_level has been
     * changed
     */
    if (lockp->flags & SIO_SPINLOCK_HELD) {
	lockp->flags &= ~SIO_SPINLOCK_HELD;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	s = lockp->token;
	was_spinlock = 1;
    }
    else {
	lockp->flags &= ~SIO_MUTEX_HELD;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	was_spinlock = 0;
    }

#ifdef DEBUG
    debug_unlock(lockp);
#endif

    /* Once we change max_level, the port is effectively no longer
     * locked, and it is not safe to do anything other than release
     * the currently held lock. Note that we even get a local copy of
     * the spinlock token and reset flags before doing this
     */
    lockp->max_level = max;

    if (was_spinlock)
	mutex_spinunlock(&lockp->spinlock, s);
    else
	mutex_unlock(&lockp->mutex);
}

int
sio_lock_port(sioport_t *port, int try)
{
    splfunc_t splfunc;
    struct sio_lock *lockp = port->sio_lock;
    int use_spinlock, s, lock_level;

    /* If this is the console, we must always open with a spinlock at
     * spl7, since we may be protecting from a kernel printf at an
     * arbitrarily high interrupt level.
     */
    ASSERT(!IS_CONSOLE(port) || lockp->lock_levels[0] == SIO_LOCK_SPL7);

  again:

    /* store a copy of the level we are locking at */
    lock_level = lockp->max_level;

    /* decide how to lock based on the level */
    if (lock_level == SIO_LOCK_MUTEX)
	use_spinlock = 0;
    else {
	use_spinlock = 1;
	splfunc = sio_lock_funcs[lock_level];
    }
    
    if (use_spinlock) {
	if (try) {
	    s = mutex_spintrylock_spl(&lockp->spinlock, splfunc);
	    if (s == 0)
		return(0);
	}
	else
	    s = mutex_spinlock_spl(&lockp->spinlock, splfunc);

	/* If max_level changed while we acquired the lock, the lock
	 * is not considered held. We must try again
	 */
	if (lock_level != lockp->max_level) {
	    mutex_spinunlock(&lockp->spinlock, s);
	    goto again;
	}

	lockp->token = s;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	lockp->flags |= SIO_SPINLOCK_HELD;
#ifdef DEBUG
	if (lockp->lock_cpu != -1)
	    debug(0);
	lockp->lock_cpu = cpuid();
#endif
    }
    else {

	/* we only "try" to acquire the lock on the console port, so by
	 * definition we can't get here
	 */
	ASSERT(!try);

	mutex_lock(&lockp->mutex, PZERO);

	/* If max_level changed while we acquired the lock, the lock
	 * is not considered held. We must try again
	 */
	if (lock_level != lockp->max_level) {
	    mutex_unlock(&lockp->mutex);
	    goto again;
	}

	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	lockp->flags |= SIO_MUTEX_HELD;
#ifdef DEBUG
	if (lockp->lock_kptr)
	    debug(0);
	lockp->lock_kptr = curthreadp;
#endif
    }

#ifdef DEBUG
    lockp->lock_ra = __return_address;
#endif
    return(1);
}

#ifdef DEBUG
static void
debug_unlock(struct sio_lock *lockp)
{
    lockp->lock_cpu = -1;
    lockp->lock_kptr = 0;
    lockp->lock_ra = 0;
}
#endif

void
sio_unlock_port(sioport_t *port)
{
    struct sio_lock *lockp = port->sio_lock;

#ifdef DEBUG
    debug_unlock(lockp);
#endif

    if (lockp->flags & SIO_SPINLOCK_HELD) {
	lockp->flags &= ~SIO_SPINLOCK_HELD;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	mutex_spinunlock(&lockp->spinlock, lockp->token);
    }
    else {
	lockp->flags &= ~SIO_MUTEX_HELD;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	mutex_unlock(&lockp->mutex);
    }
}

/* Trade the port lock for an sv */
int
sio_sleep(sioport_t *port, sv_t *sv, int pri)
{
    struct sio_lock *lockp = port->sio_lock;
    
#ifdef DEBUG
    debug_unlock(lockp);
#endif

    ASSERT(lockp->flags & SIO_LOCKS_MASK);
    if (lockp->flags & SIO_MUTEX_HELD) {
	lockp->flags &= ~SIO_MUTEX_HELD;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	return(sv_wait_sig(sv, pri, &lockp->mutex, 0));
    }
    else {
	lockp->flags &= ~SIO_SPINLOCK_HELD;
	ASSERT((lockp->flags & SIO_LOCKS_MASK) == 0);
	return(sv_wait_sig(sv, pri, &lockp->spinlock, lockp->token));
    }
}

#ifdef DEBUG
int
sio_port_islocked(sioport_t *port)
{
    struct sio_lock *lockp = port->sio_lock;

    return(((lockp->flags & SIO_MUTEX_HELD) &&
	    lockp->lock_kptr == curthreadp) ||
	   ((lockp->flags & SIO_SPINLOCK_HELD) &&
	    lockp->lock_cpu == cpuid()));
}
#endif

#if DEBUG
#define MAXCOLS 8
void
sio_prconbuf(sioport_t *port)
{
    int x, col;
    char c;
    struct upper *upper = port->sio_upper;

    if (upper == 0) {
	qprintf("no upper attached\n");
	return;
    }

    qprintf("input:\n");
    for(x = upper->sio_rx_prod - CBUFLEN + 1; x < upper->sio_rx_prod; x += MAXCOLS) {
	qprintf("0x%x: ", x & CBUFMASK);
	for(col = 0; col < MAXCOLS && x + col < upper->sio_rx_prod; col++) {
	    c = *(upper->sio_rx_data + ((x + col) & CBUFMASK));
	    qprintf("0x%x (%c) ", c, c);
	}
	qprintf("\n");
    }
    qprintf("output:\n");
    for(x = upper->sio_tx_prod - CBUFLEN + 1; x < upper->sio_tx_prod; x += MAXCOLS) {
	qprintf("0x%x: ", x & CBUFMASK);
	for(col = 0; col < MAXCOLS && x + col < upper->sio_tx_prod; col++) {
	    c = *(upper->sio_tx_data + ((x + col) & CBUFMASK));
	    qprintf("0x%x (%c) ", c, c);
	}
	qprintf("\n");
    }
}
#endif

/* circular buffer management. These buffers keep data flowing to
 * and from the hardware in a timely fashion at interrupt time even
 * if the top half entity (i.e. streams) is slow in responding
 */

/* write whatever data we can to the device from the circular buffer */
static void
sio_push_tx_buf(sioport_t *port)
{
    int bytes = 1, total = 0, avail;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));
    
    while(upper->sio_tx_cons != upper->sio_tx_prod && bytes) {

	ASSERT((sio_pc_t)(upper->sio_tx_prod - upper->sio_tx_cons) <= CBUFLEN);

	if ((upper->sio_tx_prod & CBUFMASK) > (upper->sio_tx_cons & CBUFMASK))
	    avail = (sio_pc_t)(upper->sio_tx_prod - upper->sio_tx_cons);
	else
	    avail = CBUFLEN - (upper->sio_tx_cons & CBUFMASK);

	/* if there was data to write, we either wrote it or failed to
	 * write it because the lower layer was congested. Either way we
	 * are now interested in a lowat notification
	 */
	if (!(upper->sio_state & SIO_TX_NOTIFY)) {
	    upper->sio_state |= SIO_TX_NOTIFY;
	    if (DOWN_NOTIFICATION(port, N_OUTPUT_LOWAT, 1) < 0)
		upper->sio_state |= SIO_HW_ERR;
	}

	bytes = DOWN_WRITE(port,
			   upper->sio_tx_data + (upper->sio_tx_cons & CBUFMASK),
			   avail);

	if (bytes < 0) {
	    dprintf(("hw error on write\n"));
	    /* pretent we actually wrote it all, but log the error
	     */
	    upper->sio_state |= SIO_HW_ERR;
	    upper->sio_tx_cons += avail;
	    ASSERT(TX_PTRS_VALID(upper));
	    total += avail;
	    dprintf(("error, fudge wrote %d bytes to hardware\n", avail));
	}
	else {
#ifdef MEASURE_XOFF_SKID
	    if (bytes > 0 && upper->sio_tx_data[upper->sio_tx_cons & CBUFMASK] ==
		upper->sio_cc[VSTOP]) {
		upper->skidbytes = 0;
	    }
	    upper->sendbytes += bytes;
#endif
	    upper->sio_tx_cons += bytes;
	    ASSERT(TX_PTRS_VALID(upper));
	    total += bytes;
	    dprintf(("wrote %d bytes to hardware\n", bytes));
	}
    }

    dprintf(("push %d bytes from tx buf\n", total));

    return;
}

/* write the passed data to the output circular buffer and
 * get the output started if possible
 * returns the number of bytes actually written
 */
static int
do_write_tx_buf(sioport_t *port, char *buf, int len)
{
    int total_avail, sequential_avail, avail;
    int total, bytes;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("write %d bytes to tx buf\n", len));

    total = 0;
    while(len > 0) {

	/* total number of free bytes in circular buffer */
	total_avail = CBUFLEN -
	    (sio_pc_t)(upper->sio_tx_prod - upper->sio_tx_cons);
	ASSERT(total_avail >= 0 && total_avail <= CBUFLEN);
	
	/* total number of bytes from current producer to end of buffer */
	sequential_avail = CBUFLEN - (upper->sio_tx_prod & CBUFMASK);
	ASSERT(sequential_avail > 0 && sequential_avail <= CBUFLEN);
	
	avail = MIN(total_avail, sequential_avail);

	if (avail == 0)
	    break;
	bytes = MIN(avail, len);
	bcopy(buf, upper->sio_tx_data + (upper->sio_tx_prod & CBUFMASK), bytes);
	buf += bytes;
	len -= bytes;
	upper->sio_tx_prod += bytes;
	ASSERT(TX_PTRS_VALID(upper));
	total += bytes;
    }
    
    /* push the data along if we wrote something */
    if (total > 0)
	sio_push_tx_buf(port);

    dprintf(("%d bytes copied successfully to tx buf\n", total));

    return(total);
}

/* handle TX of MMSC escape chars. If the MMSC port is open it will
 * use the esc char to indicate an MMSC packet is beginning. If the
 * tty port tries to send an esc char, we must send it twice so the
 * MMSC will pass it straight through to the real console port. If the
 * MMSC port is closed, we assume no MMSC is present and all output
 * goes out unmodified.
 */
#if MMSC
static int
sio_write_tx_buf(sioport_t *port, char *buf, int len)
{
    int total, offset, cbuf_free;
    /*REFERENCED*/
    int sent;
    char *cp;
    struct upper *upper = (struct upper*)port->sio_upper;

    /* output passes through unchanged if this is not the console, if
     * this is an SN00, if the mmsc port is not open, or if this is
     * output from the mmsc port.
     */
    if (!IS_CONSOLE(port) ||
#if SN0
	private.p_sn00 ||
#endif
	UPPER(port)->sio_mmsc_wq == 0 ||
	(UPPER(port)->sio_state & SIO_MMSC_OUT))
	return(do_write_tx_buf(port, buf, len));

    /* else this is console output on a tty port. Scan for the escape
     * char. If it exists, send it twice. Send all other bytes
     * unmodified.
     */
    total = 0;
    while(len > 0) {

	/* scan for an escape char */
	for(offset = 0, cp = buf; offset < len; offset++, cp++)
	    if (*cp == MMSC_ESCAPE_CHAR)
		break;

	/* If we got none, print the whole thing and we're done */
	if (offset == len)
	    return(total + do_write_tx_buf(port, buf, len));

	/* offset should be number of chars from start of buf to the
	 * esc char, *inclusive*, not the distance between them.
	 */
	offset++;

	/* see if we have anough space to send the data preceeding the
	 * escape char followed by two escape chars. If we do, send it
	 * all. If we don't have enough space, just attempt to send as
	 * much data as we can from the chars leading up to but not
	 * including the escape char. We don't want to get into a
	 * situation where we send the first esc char but don't have
	 * enough space to send the second one since this would
	 * require us to save some state bit indicating that the next
	 * char sent when we try again must be an esc char. Simpler to
	 * just leave the first esc char in the caller buffer and
	 * we'll see it again on the next try.
	 */
	cbuf_free = CBUFLEN -
	    (sio_pc_t)(upper->sio_tx_prod - upper->sio_tx_cons);
	if (cbuf_free < offset + 1)
	    /* not enough space, just try to send as much as we can up
	     * to but not including the esc char
	     */
	    return(total + do_write_tx_buf(port, buf, offset - 1));

	/* else we can write the whole thing including the extra esc
	 * char
	 */
	sent = do_write_tx_buf(port, buf, offset);
	ASSERT(sent == offset);
	
	/* cp still points to the esc char */
	sent = do_write_tx_buf(port, cp, 1);
	ASSERT(sent == 1);

	/* advance pointers */
	buf += offset;
	len -= offset;

	/* extra esc char doesn't count in total. Caller is only
	 * interested in how many of the bytes passed in were actually
	 * consumed
	 */
	total += offset;
    }

    return(total);
}
#else
# define sio_write_tx_buf do_write_tx_buf
#endif

/* callback when the hardware has hit the TX low water mark */
static void
sio_output_lowat(sioport_t *port)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("sio output lowat\n"));

    /* if there is nothing further to write from the tx_buf
     * we are no longer interested in being notified when output
     * drains.
     */
    if (upper->sio_tx_prod == upper->sio_tx_cons) {
	if (upper->sio_state & SIO_TX_NOTIFY) {
	    upper->sio_state &= ~SIO_TX_NOTIFY;
	    if (DOWN_NOTIFICATION(port, N_ALL_OUTPUT, 0) < 0) {
		    upper->sio_state |= SIO_HW_ERR;
		    return;
	    }
	}
    }
    else
	/* try to send some more data from the output buffer. */
	sio_push_tx_buf(port);

    /* if we have finished this buffer and close() is blocked waiting
     * for all output to flush to the hardware, wake it up. Else poke
     * the streams portion of the driver to send some more data
     */
    if (upper->sio_tx_prod == upper->sio_tx_cons) {

	if (upper->sio_state & SIO_WCLOSE) {
	    upper->sio_state &= ~SIO_WCLOSE;
	    sv_broadcast(&upper->sio_sv);
	}
	
	else if (!(upper->sio_state & SIO_TX_STRINTR)) {
	    upper->sio_state |= SIO_TX_STRINTR;
	    SIO_UNLOCK_PORT(port);
	    streams_interrupt((strintrfunc_t)sio_tx, port, NULL, NULL);
	    SIO_LOCK_PORT(port);
	}
    }
}

/* pull as much data as we can up from the hardware into the cbuf
 */
static int
sio_pull_cbuf(sioport_t *port)
{
    int bytes;
    int total_avail, sequential_avail, avail;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    /* read some data into the circular buffer */
    do {
	/* total bytes available in the input buffer */
	total_avail = CBUFLEN -
	    (sio_pc_t)(upper->sio_rx_prod - upper->sio_rx_cons);
	ASSERT(total_avail >= 0);

	/* bytes available from current producer index to end of buffer */
	sequential_avail = CBUFLEN - (upper->sio_rx_prod & CBUFMASK);

	avail = MIN(total_avail, sequential_avail);

	upper->sio_ncs = 0;
	bytes = DOWN_READ(port,
			  upper->sio_rx_data + (upper->sio_rx_prod & CBUFMASK),
			  avail);

	if (bytes < 0)
		return(-1);

#ifdef MEASURE_XOFF_SKID
	upper->skidbytes += bytes;
	if (bytes > 0)
	    upper->sendbytes = 0;
#endif

	/* If we only got one byte, there may be an error so copy
	 * the byte status. Otherwise just bzero the relevant
	 * status bytes since there can't be an error with bytes != 1
	 */
	if (bytes == 1)
	    upper->sio_rx_status[upper->sio_rx_prod & CBUFMASK] = upper->sio_ncs;
	else
	    bzero(upper->sio_rx_status + (upper->sio_rx_prod & CBUFMASK), bytes);

	upper->sio_rx_prod += bytes;
	ASSERT(RX_PTRS_VALID(upper));
    } while(bytes);
    
    ASSERT((sio_pc_t)(upper->sio_rx_prod - upper->sio_rx_cons) <= CBUFLEN);

    dprintf(("cbuf has %d bytes\n",
	     (sio_pc_t)(upper->sio_rx_prod - upper->sio_rx_cons)));
    return(0);
}

/* callback when the hardware has data available to read */
static void
sio_data_ready(sioport_t *port)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("sio data ready\n"));

    if (sio_pull_cbuf(port) < 0) {
	    queue_t *rq = upper->sio_rq;
	    upper->sio_state |= SIO_HW_ERR;
	    SIO_UNLOCK_PORT(port);
	    streams_interrupt((strintrfunc_t)qenable, rq, 0, 0);
	    SIO_LOCK_PORT(port);
	    return;
    }

    /* if we got some data, notify the streams portion
     * of the driver
     */
    if (upper->sio_rx_prod != upper->sio_rx_cons &&
	!(upper->sio_state & SIO_RX_STRINTR)) {
	upper->sio_state |= SIO_RX_STRINTR;
	SIO_UNLOCK_PORT(port);
	streams_interrupt((strintrfunc_t)sio_rx, port, NULL, NULL);
	SIO_LOCK_PORT(port);
    }
}

/* this port has been detached by the lower layer. Notify anyone
 * who may be using it
 */
/*ARGSUSED*/
static void
sio_detach(sioport_t *port)
{
    ASSERT(sio_port_islocked(port) == 0);
}

/* boot-time initialization of a port structure. Initialize anything
 * that we can from the upper layer to decrease the amount of
 * replicated init code that the lower layer must perform. This
 * includes creating the hwgraph nodes for this port.
 */
/*ARGSUSED*/
void
sio_initport(sioport_t *port, vertex_hdl_t vhdl, int which, int isconsole)
{
    static struct {
	int type;
	int attrs;
	char *name;
    } node_types[] = {
	{NODE_TYPE_D,		0,				"d"},
	{NODE_TYPE_MODEM,	SIO_ATTR_MODEM,			"m"},
	{NODE_TYPE_FLOW_MODEM,	SIO_ATTR_FLOW | SIO_ATTR_MODEM,	"f"},
	{NODE_TYPE_D_RS422,	SIO_ATTR_RS422,			"4d"},
	{NODE_TYPE_FLOW_RS422,	SIO_ATTR_RS422 | SIO_ATTR_FLOW,	"4f"}
    };

#define NUM_NODE_TYPES (sizeof(node_types) / sizeof(*node_types))

    siotype_t	       *siotype;
    vertex_hdl_t	tmp_vhdl;
    int			x, num_streams;
    /*REFERENCED*/
    graph_error_t	rc;
    miditype_t *miditype;
    tsiotype_t *tsiotype;

    /* save vertex handle */
    port->sio_vhdl = vhdl;

    /* add device nodes for serialio (streams) interface */
    for(num_streams = 0, x = 0; x < NUM_NODE_TYPES; x++)
	if (which & node_types[x].type)
	    num_streams++;
    
#if MMSC
    /* create MMSC port if necessary */
    if (
# if SN0
	!private.p_sn00 &&
# endif
	isconsole)
	num_streams++;
#endif

    siotype = (siotype_t*)
	kmem_zalloc(sizeof(siotype_t) * num_streams, KM_SLEEP);

    for(x = 0; x < NUM_NODE_TYPES; x++) {
	if (which & node_types[x].type) {
	    siotype->attrs = node_types[x].attrs;
	    siotype->port = port;
	    rc = hwgraph_char_device_add(vhdl, node_types[x].name,
					 DEV_PREFIX, &tmp_vhdl);
	    ASSERT(rc == GRAPH_SUCCESS);
	    hwgraph_chmod(tmp_vhdl, HWGRAPH_PERM_TTY);
	    device_info_set(tmp_vhdl, (void *)siotype);
	    siotype++;
	}
    }

#if MMSC
    if (
# if SN0
	!private.p_sn00 &&
# endif
	isconsole) {
	siotype->attrs = SIO_ATTR_MMSC;
	siotype->port = port;
	rc = hwgraph_char_device_add(hwgraph_root,
				     EDGE_LBL_MACHDEP "/mmsc_control",
				     DEV_PREFIX, &tmp_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_chmod(tmp_vhdl, HWGRAPH_PERM_MMSC_CONTROL);
	device_info_set(tmp_vhdl, (void *)siotype);
	siotype++;
    }
#endif

    /* add character interface if it is linked in */
    if ((which & NODE_TYPE_CHAR) && device_driver_get("csio_")) {
	rc = hwgraph_char_device_add(vhdl, "c", "csio_", &tmp_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_chmod(tmp_vhdl, HWGRAPH_PERM_TTY);
	device_info_set(tmp_vhdl, (void *)port);
    }

    /* add userial interface if it is linked in */
    if ((which & NODE_TYPE_USER) && device_driver_get("usio_")) {
	rc = hwgraph_char_device_add(vhdl, "us", "usio_", &tmp_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_chmod(tmp_vhdl, HWGRAPH_PERM_TTY);
	device_info_set(tmp_vhdl, (void *)port);
    }

    /* add midi interface if it is linked in */
    if ((which & NODE_TYPE_MIDI) && device_driver_get("midi_")) {
      miditype = kmem_zalloc(sizeof(miditype_t), KM_SLEEP);
      miditype->devflags = MIDIDEV_EXTERNAL;
      miditype->portidx = MIDIDEV_UNREGISTERED;
      miditype->port = port;
      miditype->midi_upper = NULL;

      rc = hwgraph_char_device_add(vhdl, "midi", "midi_", &tmp_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_chmod(tmp_vhdl, HWGRAPH_PERM_TTY);
	device_info_set(tmp_vhdl, (void *)miditype);
    }

    /* add tserialio interface if it is linked in */
    if ((which & NODE_TYPE_TIMESTAMPED) && device_driver_get("tsio")) {
      tsiotype = kmem_zalloc(sizeof(tsiotype_t), KM_SLEEP);
      tsiotype->port = port;
      tsiotype->tsio_upper = NULL;

      rc = hwgraph_char_device_add(vhdl, "ts", "tsio", &tmp_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_chmod(tmp_vhdl, HWGRAPH_PERM_TTY);
	device_info_set(tmp_vhdl, (void *)tsiotype);
    }
}

/*
 * Gobble any waiting input for a port
 */
static void
sio_rclr(sioport_t *port)
{
    char buf[16];
    int err;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("rclr port 0x%x\n", port));

    while((err = DOWN_READ(port, buf, sizeof(buf))) > 0);
    upper->sio_rx_prod = upper->sio_rx_cons;
    if (err < 0)
	    upper->sio_state |= SIO_HW_ERR;
}


/*
 * Shut down a port
 */
static void
sio_zap(sioport_t *port, int hup) 
{
    ASSERT(sio_port_islocked(port));

    dprintf(("zap port 0x%x hup %d\n", port, hup));

    sio_flushw(port);			/* forget pending output */
    
    sio_stop_rx(port, hup);
    sio_stop_tx(port);
    
    sio_rclr(port);
}

/*
 * Finish a delay for a port
 */
static void
sio_delay(sioport_t *port)
{
    struct upper *upper;

    ASSERT(sio_port_islocked(port) == 0);

    dprintf(("delay port 0x%x\n", port));

    SIO_LOCK_PORT(port);
    if ((upper = port->sio_upper) == 0) {
	SIO_UNLOCK_PORT(port);
	return;
    }

    if ((upper->sio_state & (SIO_BREAK | SIO_BREAK_QUIET)) != SIO_BREAK) {
	
	upper->sio_state &= ~(SIO_TIMEOUT | SIO_BREAK | SIO_BREAK_QUIET);

	if (upper->sio_state & SIO_WCLOSE) {
		upper->sio_state &= ~SIO_WCLOSE;
		sv_broadcast(&upper->sio_sv);
		SIO_UNLOCK_PORT(port);
	} else {
		SIO_UNLOCK_PORT(port);
		sio_start_tx(port);	/* resume output */
	}
    } 
    else {				/* unless need to quiet break */
	if (DOWN_BREAK(port, 0) < 0)
		upper->sio_state |= SIO_HW_ERR;
	upper->sio_state |= SIO_BREAK_QUIET;
	SIO_UNLOCK_PORT(port);
	upper->sio_tid = STREAMS_TIMEOUT(sio_delay, (caddr_t)port, HZ/20);
    }
}



/*
 * Heartbeat to send input up stream.
 * This is used to reduced the large cost of trying to send each
 * input byte upstream by itself. Not used by hardware with large
 * buffering capacity
 */
static void
sio_rsrv_timer(sioport_t *port)
{
    struct upper *upper;
    dprintf(("rsrv_timer port 0x%x\n", port));

    ASSERT(sio_port_islocked(port) == 0);

    SIO_LOCK_PORT(port);
    
    if ((upper = port->sio_upper) == 0) {
	SIO_UNLOCK_PORT(port);
	return;
    }

    if (--upper->sio_rsrv_cnt) {
	SIO_UNLOCK_PORT(port);
	(void)STREAMS_TIMEOUT(sio_rsrv_timer, (caddr_t)port, 
			      upper->sio_rsrv_duration);
	SIO_LOCK_PORT(port);
    }
    
    if (upper->sio_state & SIO_ISOPEN) {
#if MMSC
	int enb_mmsc_rq, enb_rq;
	
	/* figure out which queues need to be enabled. A queue needs
	 * to be enabled if there is an rmsg waiting, or if the
	 * current input buffer would eventually go to that queue.
	 */
	enb_mmsc_rq = upper->sio_rmsg_mmsc || 
	    (upper->sio_rbp && (upper->sio_state & SIO_MMSC_IN));
	enb_rq = upper->sio_rmsg || 
	    (upper->sio_rbp && !(upper->sio_state & SIO_MMSC_IN));
#endif

	SIO_UNLOCK_PORT(port);
	if (
#if MMSC
	    enb_rq && upper->sio_rq &&
#endif
	    canenable(upper->sio_rq))
	    qenable(upper->sio_rq);
#if MMSC
	if (enb_mmsc_rq && upper->sio_mmsc_rq && canenable(upper->sio_mmsc_rq))
	    qenable(upper->sio_mmsc_rq);
#endif
    }
    else
	SIO_UNLOCK_PORT(port);
}


/*
 * Flush output for a port.              
 */
static void
sio_flushw(sioport_t *port)
{
    toid_t toid;
    mblk_t *m1;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("flushw port 0x%x\n", port));

    m1 = upper->sio_wbp;
    upper->sio_wbp = NULL;

    upper->sio_tx_prod = upper->sio_tx_cons;

    if ((upper->sio_state & (SIO_TIMEOUT | SIO_BREAK | SIO_BREAK_QUIET))
	== SIO_TIMEOUT) {
	toid = upper->sio_tid;
	upper->sio_state &= ~SIO_TIMEOUT;
	SIO_UNLOCK_PORT(port);
	untimeout(toid);		/* forget stray timeout */
    }
    else
	SIO_UNLOCK_PORT(port);

    freemsg(m1);
    SIO_LOCK_PORT(port);
}

/*
 * Flush input for a port
 */
static void
sio_flushr(sioport_t *port)
{
    mblk_t *m1, *m2;
#if MMSC
    mblk_t *m3;
#endif
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("flushr port 0x%x\n", port));
    
    upper->sio_rx_prod = upper->sio_rx_cons;

    m1 = upper->sio_rmsg;
    upper->sio_rmsg = NULL;
    upper->sio_rmsg_len = 0;
    m2 = upper->sio_rbp;
    upper->sio_rbp = NULL;
#if MMSC
    m3 = upper->sio_rmsg_mmsc;
    upper->sio_rmsg_mmsc = 0;
    upper->sio_rmsg_mmsc_len = 0;
#endif

    SIO_UNLOCK_PORT(port);
    freemsg(m1);
    freemsg(m2);
#if MMSC
    freemsg(m3);
    if (upper->sio_mmsc_rq)
	qenable(upper->sio_mmsc_rq);
#endif
    if (upper->sio_rq)
	qenable(upper->sio_rq);	/* turn input back on */
    SIO_LOCK_PORT(port);
}

/*
 * Save a message on our write queue and start the output interrupt
 * if necessary.
 */
static void
sio_save(sioport_t *port, queue_t *wq, mblk_t *bp)
{
    ASSERT(sio_port_islocked(port) == 0);

    dprintf(("save port 0x%x q 0x%x m 0x%x\n", port, wq, bp));

    putq(wq, bp);				/* save the message */
    
    sio_start_tx(port);
}


/*
 * Get current tty parameters.
 */
void
sio_tcgeta(queue_t *wq, mblk_t *bp, struct termio *p)
{
    dprintf(("tcgeta q 0x%x\n", wq));

    *STERMIO(bp) = *p;
    
    bp->b_datap->db_type = M_IOCACK;
    qreply(wq, bp);
}
#ifdef IS_DU
void
tcgeta(queue_t *wq, mblk_t *bp, struct termio *p) { sio_tcgeta(wq,bp,p); }
#endif

/*
 * Set tty parameters.
 */
static void
sio_tcset(sioport_t *port, mblk_t *bp)
{
    tcflag_t cflag;
    speed_t ospeed;
    struct iocblk *iocp;
    struct termio *tp;
    struct upper *upper = port->sio_upper;
    int err;
    
    ASSERT(sio_port_islocked(port));

    dprintf(("tcset port 0x%x\n", port));

    iocp = (struct iocblk *)bp->b_rptr;
    tp = STERMIO(bp);
    
    if (tp->c_ospeed == __INVALID_BAUD) {
	tp->c_ospeed = upper->sio_ospeed;
    }
    tp->c_ispeed = upper->sio_ospeed;
    cflag = tp->c_cflag;
    ospeed = tp->c_ospeed;

    if (upper->sio_state & SIO_MODEM) {
	/*
	 * On modem ports, only super-user can change CLOCAL.
	 * Otherwise, fail to do so silently for historic reasons.
	 */
	if (((upper->sio_cflag & CLOCAL) ^ (cflag & CLOCAL)) &&
		!_CAP_CRABLE(iocp->ioc_cr, CAP_DEVICE_MGT)) {
	    cflag &= ~CLOCAL;
	    cflag |= upper->sio_cflag & CLOCAL;
	}
    }
    
    if (err = sio_config(port, cflag, ospeed, tp))
	    iocp->ioc_error = (err < 0) ? EIO : EINVAL;
    
    tp->c_cflag = upper->sio_cflag;	/* tell line discipline the results */
    tp->c_ospeed = upper->sio_ospeed;
    tp->c_ispeed = upper->sio_ispeed;
    
    iocp->ioc_count = 0;
    bp->b_datap->db_type = M_IOCACK;

    ASSERT(sio_port_islocked(port));
}


/*
 * Interrupt - Process an IOCTL. This function processes those IOCTLs
 * that must be done by the output interrupt.
 */
static void
sio_i_ioctl(sioport_t *port, mblk_t *bp)
{
    struct iocblk *iocp;
    int err;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));
    
    dprintf(("i_ioctl port 0x%x\n", port));

    iocp = (struct iocblk *)bp->b_rptr;
    switch (iocp->ioc_cmd) {
	
      case TCSBRK:
	
	iocp->ioc_count = 0;
	bp->b_datap->db_type = M_IOCACK;

	if (!IOCARG(bp->b_cont->b_rptr)) {
	    
	    if (DOWN_BREAK(port, 1) < 0) {
		    upper->sio_state |= SIO_HW_ERR;
		    iocp->ioc_error = EIO;
		    break;
	    }

	    upper->sio_state |= SIO_TIMEOUT | SIO_BREAK;
	    SIO_UNLOCK_PORT(port);
	    upper->sio_tid = STREAMS_TIMEOUT(sio_delay, (caddr_t)port, HZ/4);
	    SIO_LOCK_PORT(port);
	}
	
	break;
	
      case TCSETAF:
	
	sio_tcset(port, bp);
	sio_flushr(port);
	SIO_UNLOCK_PORT(port);
	(void)putctl1(upper->sio_rq->q_next, M_FLUSH, FLUSHR);
	SIO_LOCK_PORT(port);
	break;
	
      case TCSETAW:
	
	sio_tcset(port, bp);
	break;
	
      case SIOC_ITIMER: {
	      int timeout = IOCARG(bp->b_cont->b_rptr);

	      iocp->ioc_count = 0;
	      bp->b_datap->db_type = M_IOCACK;

	      if ((err = DOWN_RX_TIMEOUT(port, timeout)) > 0) {
		      if ((unsigned)timeout > HZ)
			      iocp->ioc_error = EINVAL;
		      else
			      upper->sio_rsrv_duration = timeout;
	      }
	      else if (err == 0)
		      upper->sio_rsrv_duration = 0;
	      else {
		      upper->sio_state |= SIO_HW_ERR;
		      iocp->ioc_error = EIO;
	      }
	      break;
      }
	
      case TIOCMGET: {
	  int ret = 0;

	  if (DOWN_QUERY_CTS(port))
	      ret |= TIOCM_CTS;
	  if (DOWN_QUERY_DCD(port))
	      ret |= TIOCM_CD;
	  if (upper->sio_state & SIO_RTS)
	      ret |= TIOCM_RTS;
	  if (upper->sio_state & SIO_DTR)
	      ret |= TIOCM_DTR;
	  
	  iocp->ioc_count = sizeof(ret);
	  *(int*)(bp->b_cont->b_rptr) = ret;
	  bp->b_datap->db_type = M_IOCACK;
	  break;
      }

      case SIOC_RS422: {
	  enum sio_proto p =
	      IOCARG(bp->b_cont->b_rptr) ? PROTO_RS422 : PROTO_RS232;

	  iocp->ioc_count = 0;
	  bp->b_datap->db_type = M_IOCACK;

	  if (DOWN_SET_PROTOCOL(port, p))
	      iocp->ioc_error = EINVAL;
	  break;
      }
	
      case SIOC_EXTCLK: {
	  int clock_factor = IOCARG(bp->b_cont->b_rptr);

	  iocp->ioc_count = 0;
	  bp->b_datap->db_type = M_IOCACK;

	  if (DOWN_SET_EXTCLK(port, clock_factor))
	      iocp->ioc_error = EINVAL;
	  break;
      }

      default:
	
	ASSERT(0);
    }
    
    SIO_UNLOCK_PORT(port);
    putnext(upper->sio_rq, bp);
    SIO_LOCK_PORT(port);
}

/*
 * Open a UART port as a stream.
 */
/* ARGSUSED */
static int
sio_open(queue_t	*rq,
	 dev_t		*devp,
	 int		flag,
	 int		sflag,
	 cred_t		*crp)
{
    sioport_t *port;
    siotype_t *type;
    vertex_hdl_t vhdl;
    int error;
    struct upper *upper;
    queue_t *wq = WR(rq);
#if IS_DU
    siotype_t du_type;
#endif

    dprintf(("open q 0x%x\n", rq));

    if (sflag)			/* only a simple stream driver */
	return(ENXIO);

    if (dev_is_vertex(*devp)) {
	vhdl = dev_to_vhdl(*devp);
	if (vhdl == GRAPH_VERTEX_NONE)
	    return ENODEV;
	type = (siotype_t *)device_info_get(vhdl);
    } else {
#if IS_DU
	if (minor(*devp) != 1)
	    return ENODEV;
	type = &du_type;
	du_type.attrs = 0;
	du_type.port = the_console_port;
#else
	return ENODEV;
#endif
    }
    if (type == NULL)
	return(ENODEV);

    port = type->port;
    if (port == NULL)
	return(ENODEV);

    dprintf(("open: Port 0x%x ", port));

    SYSINFO.rcvint = SYSINFO.xmtint = 0;
    SYSINFO.rawch = SYSINFO.outch = 0;
    
    /* Allocate upper layer private data */
    if ((upper = (struct upper*)
	 kmem_zalloc(sizeof(struct upper), KM_SLEEP)) == 0)
	return(ENOMEM);
    sv_init(&upper->sio_sv, 0, "sio_sv");

    SIO_LOCK_PORT(port);
    
    /* make sure this port isn't owned by another upper layer */
    if (port->sio_callup && port->sio_callup != &sio_callup) {
	SIO_UNLOCK_PORT(port);
	sv_destroy(&upper->sio_sv);
	kmem_free(upper, sizeof(struct upper));
	return(EBUSY);
    }
    
    if (port->sio_upper == 0) {	/* on the 1st open */
	
	tcflag_t cflag;
	speed_t ospeed;
	cflag = def_stty_ld.st_cflag;
	ospeed = def_stty_ld.st_ospeed;
	
	/* attach the callup vector before upgrading the lock */
	port->sio_callup = &sio_callup;

	/* upgrade the lock to our needs */
	sio_upgrade_lock(port, SIO_LOCK_LEVEL);

	port->sio_upper = upper;
#if DEBUG
	port->sio_lockcalls = L_LOCK_ALL;
#endif

	if (DOWN_OPEN(port)) {
	    error = EIO;
	    goto err_return;
	}

	/* select rs422 or rs232 based on the port type */
	if (DOWN_SET_PROTOCOL(port, IS422(type) ? PROTO_RS422 : PROTO_RS232)) {
	    error = ENODEV;
	    goto err_return;
	}

	/* start out with internal clock on first open */
	(void)DOWN_SET_EXTCLK(port, 0);

	if ( ISMODEM(type) ) {
	    upper->sio_state |= SIO_MODEM;
	    cflag &= ~CLOCAL;
	}
	
	if ( ISFLOW(type) ) {
	    /* upper->sio_state |= SIO_FLOW; */
	    cflag |= CNEW_RTSCTS;

	    /* if lower layer supports hardware flow control, we won't
	     * interfere. If it does not support it, we'll emulate it
	     * using RTS
	     */
	    if ((error = DOWN_ENABLE_HFC(port, 1)) == 0)
		upper->sio_state |= SIO_AUTO_HFC;
	    else if (error < 0) {
		error = ENODEV;
		goto err_return;
	    }
	}
	else {
	    if (DOWN_ENABLE_HFC(port, 0) < 0) {
		error = ENODEV;
		goto err_return;
	    }
	}
	
	if ((error = DOWN_RX_TIMEOUT(port, duart_rsrv_duration)) > 0) {
		if ((unsigned)duart_rsrv_duration > HZ)
			upper->sio_rsrv_duration = 2;
		else
			upper->sio_rsrv_duration = duart_rsrv_duration;
	}
	else if (error == 0)
	    upper->sio_rsrv_duration = 0;
	else {
	    error = ENODEV;
	    goto err_return;
	}

#ifdef IS_DU
	/* maintain default baud rate */
	if (isconsole(port)) {
		ospeed = def_console_baud;
	} else if ((void *) port == the_kdbx_port) {
		char *env = kopt_find("rbaud");

		if (atoi(env) > 0)
			ospeed = atoi(env);
	}
#endif

	if (error = sio_config(port, cflag, ospeed, &def_stty_ld.st_termio)) {
	    error = (error < 0) ? EIO : EINVAL;
	    goto err_return;
	}
	
	upper->sio_state |= SIO_WOPEN;
	
	sio_rclr(port);			/* discard input */
	

	if (!sio_start_rx(port)		/* wait for carrier */
	    && !(upper->sio_cflag & CLOCAL)
	    && !(flag & (FNONBLK | FNDELAY))) {
	    do {
		if (sio_sleep(port, &upper->sio_sv, PZERO+1)) {
		    SIO_LOCK_PORT(port);
		    upper->sio_state &= ~(SIO_WOPEN | SIO_ISOPEN);
		    sio_zap(port, HUPCL);
		    error = EINTR;
		    goto err_return;
		}
		SIO_LOCK_PORT(port);
		if (upper->sio_state & SIO_HW_ERR) {
		    sio_zap(port, HUPCL);
		    error = EIO;
		    goto err_return;
		}
	    } while (!(upper->sio_state & SIO_DCD));
	}
	
	/*
	 *    Connect this port to stream
	 */
	rq->q_ptr = (caddr_t)port;
	wq->q_ptr = (caddr_t)port;
#if MMSC
	if (ISMMSC(type)) {
	    upper->sio_mmsc_wq = wq;
	    upper->sio_mmsc_rq = rq;
	    upper->sio_mmsc_state = FS_NORMAL;
	    upper->sio_state &= ~SIO_MMSC_ALL;
	}
	else
#endif
	{
	    upper->sio_wq = wq;
	    upper->sio_rq = rq;
	}

	upper->sio_state |= SIO_ISOPEN;
	upper->sio_state &= ~SIO_WOPEN;
	upper->sio_cflag |= CREAD;
	
	SIO_UNLOCK_PORT(port);

#if MMSC
	/* don't push a line discipline on the MMSC port */
	if (!ISMMSC(type))
#endif
	    if (error = strdrv_push(rq, "stty_ld", devp, crp)) {
		SIO_LOCK_PORT(port);
		upper->sio_rq = NULL;
		upper->sio_wq = NULL;
		sio_zap(port, HUPCL);
		goto err_return;
	    }
    }
    else {  /*  This port on this Controller is already opened !  */

#if MMSC
	/* we can only open the MMSC port once */
	if (ISMMSC(type)) {
	    if (UPPER(port)->sio_mmsc_rq)
		goto failbusy;
	    
	    /* first open of mmsc port and "ordinary" port is already
	     * open.  we just connect our queues.
	     */
	    rq->q_ptr = (caddr_t)port;
	    wq->q_ptr = (caddr_t)port;
	    UPPER(port)->sio_mmsc_wq = wq;
	    UPPER(port)->sio_mmsc_rq = rq;
	    UPPER(port)->sio_mmsc_state = FS_NORMAL;
	    UPPER(port)->sio_state &= ~SIO_MMSC_ALL;
	}
	else
#endif
	{
#if MMSC
	    /* mmsc port may be open already, so this could be the
	     * first open for the tty port.  port might also be in
             * the "wait for carrier" state, in which case another open is
             * already in progress, and this open should return EBUSY.
	     */
	    if ( (UPPER(port)->sio_rq == 0) &&
		!(UPPER(port)->sio_state & SIO_WOPEN) ) {
		rq->q_ptr = (caddr_t)port;
		wq->q_ptr = (caddr_t)port;
		UPPER(port)->sio_wq = wq;
		UPPER(port)->sio_rq = rq;
		
		SIO_UNLOCK_PORT(port);
		if (error = strdrv_push(rq, "stty_ld", devp, crp)) {
		    SIO_LOCK_PORT(port);
		    upper->sio_rq = NULL;
		    upper->sio_wq = NULL;
		    SIO_UNLOCK_PORT(port);
		    sv_destroy(&upper->sio_sv);
		    kmem_free(upper, sizeof(struct upper));
		    return(error);
		}
		SIO_LOCK_PORT(port);
	    }
	    else
#endif
		
	    /*
	     * you cannot open two streams to the same device. the port
	     * structure can only point to one of them. therefore, you
	     * cannot open two different minor devices that are synonyms
	     * for the same device. that is, you cannot open both ttym1
	     * and ttyd1.
	     */
	    
	    if (UPPER(port)->sio_rq != rq) {
	    
#if MMSC
	      failbusy:
#endif
		/* fail if already open */
		SIO_UNLOCK_PORT(port);
		sv_destroy(&upper->sio_sv);
		kmem_free(upper, sizeof(struct upper));
		return(EBUSY);
	    }
	}

	ASSERT(port->sio_callup == &sio_callup);
	SIO_UNLOCK_PORT(port);
	sv_destroy(&upper->sio_sv);
	kmem_free(upper, sizeof(struct upper));
    }

    dprintf(("open completed normally\n"));
    return(0);

  err_return:
    port->sio_callup = 0;
    port->sio_upper = 0;
    sio_downgrade_unlock_lock(port, SIO_LOCK_LEVEL);
    sv_destroy(&upper->sio_sv);
    kmem_free(upper, sizeof(struct upper));
    return(error);
}

/* timeout so close doesn't wait forever if output is stalled for some
 * reason.
 */
static void
close_timeout(void *arg)
{
    sioport_t *port = (sioport_t*)arg;
    struct upper *upper;

    SIO_LOCK_PORT(port);
    if ((upper = port->sio_upper) && (upper->sio_state & SIO_WCLOSE)) {
	upper->sio_state &= ~(SIO_WCLOSE|SIO_TX_NOTIFY);
	upper->sio_tx_cons = upper->sio_tx_prod;
	sv_broadcast(&upper->sio_sv);
    }
    SIO_UNLOCK_PORT(port);
}

/*
 * Close a port.
 */
/* ARGSUSED */
static int
sio_close(queue_t	*rq,
	  int		flag,
	  cred_t	*crp)
{
    sioport_t *port;
    struct upper *upper;
    toid_t toid;
    
    dprintf(("close q 0x%x\n", rq));

    port = (sioport_t *)rq->q_ptr;
    
    SIO_LOCK_PORT(port);
    upper = port->sio_upper;

#if MMSC
    ASSERT(rq == upper->sio_rq || rq == upper->sio_mmsc_rq);
    
    /* If both the MMSC port and the tty port are open, just zero out
     * the queue pointers and leave the port alone since the other
     * stream is still using it. However if only one port is open and
     * we're closing it, go through the whole close ordeal.
     */
    if (upper->sio_rq && upper->sio_mmsc_rq) {
	if (rq == upper->sio_rq) {
	    upper->sio_rq = 0;
	    upper->sio_wq = 0;
	    SIO_UNLOCK_PORT(port);
	}
	else {
	    upper->sio_mmsc_rq = 0;
	    upper->sio_mmsc_wq = 0;
	    upper->sio_state &= ~SIO_CONT_MMSC;

	    SIO_UNLOCK_PORT(port);
	    /* kick off transmitter again in case it was stopped */
	    sio_start_tx(port);
	}
	return(0);
    }
#endif

    if (!(upper->sio_state & SIO_HW_ERR)) {

	/* make sure HFC is disabled so we don't hang forever closing
	 * a port with a deasserted cts line
	 *
         * We shouldn't need this since the timeout below will awaken us
         * anyway. Doing this is causing hardware flow control problem
         * for characters pending in the DMA buffers.
         * (void)DOWN_ENABLE_HFC(port, 0);
         */

	/* sleep until all pending output in the circular output buffer
	 * has been flushed to the hardware, otherwise we'll lose it
	 */
	toid = timeout(close_timeout, port, HZ * 10);
	while ((upper->sio_tx_prod != upper->sio_tx_cons) ||
	       (upper->sio_state & (SIO_TX_NOTIFY|SIO_BREAK|
				    SIO_BREAK_QUIET|SIO_TIMEOUT))) {
	    upper->sio_state |= SIO_WCLOSE;

	    /* make sure output is active.
	     * XXX this should always be the case anyway!! There appears
	     * to be some scenario in which we get here and the output
	     * interrupt is not set, and we sleep forever. This is a
	     * workaround but it's only masking the error since in no case
	     * should the output be stopped while there is data in the
	     * circular buffer anyway. (unless HFC is in force, but we
	     * cleared that above). Ideally I'd like to find out why this
	     * happens and correct it rather than work around.
	     */
	    sio_push_tx_buf(port);
	    ASSERT(upper->sio_state & SIO_TX_NOTIFY);

	    if (sio_sleep(port, &upper->sio_sv, PZERO+1)) {
		SIO_LOCK_PORT(port);
		upper->sio_state &= ~SIO_WCLOSE;
		break;
	    }
	    SIO_LOCK_PORT(port);
	}
	untimeout(toid);
	ASSERT((upper->sio_state & SIO_WCLOSE) == 0);
    }
    else
	/* closing due to a hardware error, toss any pending output */
	upper->sio_tx_prod = upper->sio_tx_cons;

    if (upper->sio_state & SIO_SE_PENDING) {
	upper->sio_state &= ~SIO_SE_PENDING;
	SIO_UNLOCK_PORT(port);
	str_unbcall(rq);
	SIO_LOCK_PORT(port);
    }
    
    sio_flushr(port);
    sio_flushw(port);
    
    upper->sio_state &= ~(SIO_ISOPEN | SIO_WOPEN);

    /* XXX necessary? we just destroy upper below, not sure if any of
     * the functions in the interim use these pointers though
     */
    upper->sio_rq = NULL;
    upper->sio_wq = NULL;
#if MMSC
    upper->sio_mmsc_rq = NULL;
    upper->sio_mmsc_wq = NULL;
#endif
    
    sio_zap(port, upper->sio_cflag & HUPCL);
    
    /* turn off all input interrupts. output interrupts are disabled
     * by sio_output_lowat when all output has finally drained
     */
    DOWN_NOTIFICATION(port, N_ALL, 0);
    upper->sio_state &= ~SIO_TX_NOTIFY;

    port->sio_callup = 0;
    port->sio_upper = 0;
    sio_downgrade_unlock_lock(port, SIO_LOCK_LEVEL);
    sv_destroy(&upper->sio_sv);
    kmem_free(upper, sizeof(struct upper));
    return(0);
}

/*
 * Read service. Send one or more character up the stream.
 */
static void
sio_rsrv(queue_t *rq)
{
    mblk_t *bp;
    sioport_t *port;
    struct upper *upper;
#if MMSC
    int ismmscq;
#endif
    
    dprintf(("rsrv q 0x%x\n", rq));

    if (!canput(rq->q_next) ) {         /* quit if upstream congested  */
	noenable(rq);
	return;
    }
    
    enableok(rq);

    port = (sioport_t *)rq->q_ptr;
    SIO_LOCK_PORT(port);
    if ((upper = port->sio_upper) == 0) {
	SIO_UNLOCK_PORT(port);
	return;
    }

    if (upper->sio_state & SIO_SE_PENDING) {
	upper->sio_state &= ~SIO_SE_PENDING;
	SIO_UNLOCK_PORT(port);
	str_unbcall(rq);
	SIO_LOCK_PORT(port);
    }
    
#if MMSC
    ASSERT(rq == upper->sio_rq || rq == upper->sio_mmsc_rq);
    ismmscq = (rq == upper->sio_mmsc_rq);

    /* if we are servicing the mmsc queue and current input is within
     * an mmsc packet, consider merging the current input with the
     * mmsc input.
     */
    if (upper->sio_state & SIO_MMSC_IN) {
	if (ismmscq) {
	    bp = upper->sio_rbp;
	    if (bp && (bp->b_wptr > bp->b_rptr) &&
		(!upper->sio_rmsg_mmsc || !upper->sio_rsrv_cnt)) {
		
		str_conmsg(&upper->sio_rmsg_mmsc, &upper->sio_rmsge_mmsc, bp);
		upper->sio_rmsg_mmsc_len += (bp->b_wptr - bp->b_rptr);
		upper->sio_rbp = NULL;
	    }
	}
    }

    /* else if this is the tty queue and current input is not within
     * an mmsc packet, consider merging the current input with the
     * tty input.
     */
    else if (!ismmscq)
#endif
    {
	bp = upper->sio_rbp;
	if (bp && (bp->b_wptr > bp->b_rptr) &&
	    (!upper->sio_rmsg || !upper->sio_rsrv_cnt)) {
	    
	    /* if tty port is not open, just toss the message. */
	    if (upper->sio_rq == 0)
		freemsg(bp);
	    else {
		str_conmsg(&upper->sio_rmsg, &upper->sio_rmsge, bp);
		upper->sio_rmsg_len += (bp->b_wptr - bp->b_rptr);
	    }
	    upper->sio_rbp = NULL;
	}
    }

#if MMSC
    bp = ismmscq ? upper->sio_rmsg_mmsc : upper->sio_rmsg;
#else
    bp = upper->sio_rmsg;
#endif

    if (bp) {
	int totalsize = RMSG_LEN(upper);
	if (totalsize > upper->sio_rbsize)
	    upper->sio_rbsize = totalsize;
	else
	    upper->sio_rbsize = (totalsize + upper->sio_rbsize) / 2;
	
#if MMSC
	if (ismmscq) {
	    upper->sio_rmsg_mmsc_len = 0;
	    upper->sio_rmsg_mmsc = 0;
	}
	else
#endif
	{
	    upper->sio_rmsg_len = 0;
	    upper->sio_rmsg = 0;
	}
	
	SIO_UNLOCK_PORT(port);	/* without too much blocking, */
	putnext(rq, bp);	/* send the message */
	SIO_LOCK_PORT(port);
    }
    
    sio_iflow(port, SEND_XON, !IGNORE_IXOFF);
    
    /* get a buffer now, rather than waiting for an interrupt */
    if (!upper->sio_rbp)
	(void)GETBP(port, BPRI_LO);
    
    /* if there is an error condition, notify upstream */
    if (upper->sio_state & SIO_HW_ERR) {
	if (!(upper->sio_state & SIO_HW_ERR_SENT)) {
	    upper->sio_state |= SIO_HW_ERR_SENT;
	    SIO_UNLOCK_PORT(port);
	    putctl1(upper->sio_rq->q_next, M_ERROR, EIO);
	}
	else
	    SIO_UNLOCK_PORT(port);
    }
    else {
	/* If the input is clogged and the cbuf is full, the lower
	 * layer will have disabled RX interrupts in response to our
	 * inability to retrieve the bytes that it is holding.  We'll
	 * have to reenable the interrupt to get things moving
	 * again. This state was presumably reached due to upstream
	 * congestion, which is presumably resolved now since we are
	 * here.
	 */
	if ((upper->sio_state & SIO_CLOGGED) &&
	    upper->sio_rx_prod == upper->sio_rx_cons + CBUFLEN)
		if (DOWN_NOTIFICATION(port, N_DATA_READY, 1) < 0)
			upper->sio_state |= SIO_HW_ERR;

	SIO_UNLOCK_PORT(port);
    }
}

/* make hwgraph nodes for the given device */
void
sio_make_hwgraph_nodes(sioport_t *port)
{
    char name[16], oname[16];
    int num;
    vertex_hdl_t ttydir_vhdl, target_vhdl;
    /*REFERENCED*/
    int rc;
    graph_edge_place_t place;

    /* only done once at boot time, then we're done */
    SIO_LOCK_PORT(port);
    if (port->sio_flags & SIO_HWGRAPH_INITED) {
	SIO_UNLOCK_PORT(port);
	return;
    }
    port->sio_flags |= SIO_HWGRAPH_INITED;
    SIO_UNLOCK_PORT(port);

    /* get the assigned number for this tty device */
    num = device_controller_num_get(port->sio_vhdl);

    if (num < 1 || num > 1000)
	return;
    
    /* make sure tty dir exists */
    rc = hwgraph_path_add(hwgraph_root, "ttys", &ttydir_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    
    /* walk the graph at this point, create duplicate edges
     * for every node found
     */
    place = EDGE_PLACE_WANT_REAL_EDGES;
    while(hwgraph_edge_get_next(port->sio_vhdl,
				oname,
				&target_vhdl,
				&place) == GRAPH_SUCCESS) {
	
	/* create the new edge */
	sprintf(name, "tty%s%d", oname, num);
	hwgraph_edge_add(ttydir_vhdl, target_vhdl, name);
    }
}

#if MMSC
/* acquire console for printf output. Return true on success, 0 on
 * failure. This is needed to prevent interleaving of kernel printf
 * output in an mmsc output packet.
 */
int
printf_acquire_console(void)
{
    struct upper *upper;
    sioport_t *port;
    int ret;

    if ((port = the_console_port) == 0)
	return(1);

    SIO_LOCK_PORT(port);

    if ((upper = UPPER(port)) == 0) {
	SIO_UNLOCK_PORT(port);
	return(1);
    }

    if (upper->sio_state & SIO_USE_MMSC)
	ret = 0;
    else {
	ret = 1;
	upper->sio_state |= SIO_PRINTF;
    }
    SIO_UNLOCK_PORT(port);
    return(ret);
}

/* release kernel printf ownership of console */
void
printf_release_console(void)
{
    struct upper *upper;
    sioport_t *port = the_console_port;

    if (port) {
	SIO_LOCK_PORT(port);
	if (upper = UPPER(port)) {
	    upper->sio_state &= ~SIO_PRINTF;
	    if (upper->sio_state & SIO_USE_MMSC) {
		SIO_UNLOCK_PORT(port);
		/* streams_interrupt needs to be called from a thread
		 * context, which we don't necessarily have here. So
		 * we'll set a timeout for right now, so it can run
		 * from the timeout thread.
		 */
		timeout((void(*)())streams_interrupt, (void*)sio_start_tx,
			0, port, NULL, NULL);
		return;
	    }
	}
	SIO_UNLOCK_PORT(port);
    }
}
#endif

/*
 * Write put function. Start processing a message
 */
static int
sio_wput(queue_t *wq, mblk_t *bp)
{
    sioport_t *port;
    struct iocblk *iocp;
    unchar c, type;
    struct upper *upper;
#if MMSC
    int mmsc_enb;
#endif

    dprintf(("wput q 0x%x\n", wq));

    port = (sioport_t *)wq->q_ptr;
    upper = port->sio_upper;
    ASSERT(sio_port_islocked(port) == 0);

    if ((type = bp->b_datap->db_type) == M_DATA
	|| type == M_IOCTL) {
	    SIO_LOCK_PORT(port);
	    if ((upper = port->sio_upper)
		&& (upper->sio_state & SIO_HW_ERR)) {
		    SIO_UNLOCK_PORT(port);
		    goto error;
	    }
	    SIO_UNLOCK_PORT(port);
    }

    /*   do according to Message type  */
    switch (bp->b_datap->db_type) {
	
      case M_FLUSH:	/* XXX may not want to restart output since flow
			   control may be messed up */
	c = *bp->b_rptr;
	sdrv_flush(wq, bp);
	
	if (c & FLUSHW) {
	    SIO_LOCK_PORT(port);
	    sio_flushw(port);
	    upper->sio_state &= ~SIO_TXSTOP;
	    
	    /* can't check error here, if we try to return an error,
	     * streams will close the port, send another flush, ...
	     */
	    (void)DOWN_ENABLE_TX(port);
	    SIO_UNLOCK_PORT(port);
	    sio_start_tx(port);	/* restart output */
	}
	if (c & FLUSHR) {
	    SIO_LOCK_PORT(port);
	    sio_flushr(port);
	    SIO_UNLOCK_PORT(port);
	}
	break;
	
      case M_DATA:
      case M_DELAY:
	sio_save(port, wq, bp);
	break;
	
      case M_IOCTL:
	iocp = (struct iocblk *)bp->b_rptr;
	
	switch (iocp->ioc_cmd) {
	    
	  case TCXONC:
	    switch (IOCARG(bp->b_cont->b_rptr)) {
	      case 0:			/* stop output */
		SIO_LOCK_PORT(port);
		upper->sio_state |= SIO_TXSTOP;
		if (DOWN_DISABLE_TX(port) < 0) {
			upper->sio_state |= SIO_HW_ERR;
			SIO_UNLOCK_PORT(port);
			goto error;
		}
		sio_stop_tx(port);
		SIO_UNLOCK_PORT(port);
		break;
	      case 1:			/* resume output */
		SIO_LOCK_PORT(port);
		upper->sio_state &= ~SIO_TXSTOP;
		if (DOWN_ENABLE_TX(port) < 0) {
			upper->sio_state |= SIO_HW_ERR;
			SIO_UNLOCK_PORT(port);
			goto error;
		}
		SIO_UNLOCK_PORT(port);
		sio_start_tx(port);
		break;
	      case 2:
		SIO_LOCK_PORT(port);
		sio_iflow(port, SEND_XOFF, IGNORE_IXOFF);
		SIO_UNLOCK_PORT(port);
		break;
	      case 3:
		SIO_LOCK_PORT(port);
		sio_iflow(port, SEND_XON, IGNORE_IXOFF);
		SIO_UNLOCK_PORT(port);
		break;
	      default:
		iocp->ioc_error = EINVAL;
		break;
	    }
	    
	    iocp->ioc_count = 0;
	    bp->b_datap->db_type = M_IOCACK;
	    qreply(wq, bp);
	    break;
	    
	  case TCGETA:
	    ASSERT(iocp->ioc_count == sizeof(struct termio));
	    sio_tcgeta(upper->sio_wq,bp, &upper->sio_termio);
	    break;
	    
	  case TCSETA:
	    ASSERT(iocp->ioc_count == sizeof(struct termio));
	    SIO_LOCK_PORT(port);
	    (void)sio_tcset(port,bp);
	    SIO_UNLOCK_PORT(port);
	    qreply(wq,bp);
	    break;
	    
	  case TCSBRK:	/* send BREAK */
	  case TCSETAF:	/* drain output, flush input, set parameters */
	  case TCSETAW:	/* drain output, set paramaters */
	  case SIOC_RS422:
	  case SIOC_EXTCLK:
	  case SIOC_ITIMER:
	  case TIOCMGET:
	    sio_save(port, wq, bp);
	    break;
	    
	  case SIOC_MKHWG: /* make hwgraph nodes */
	    sio_make_hwgraph_nodes(port);
	    bp->b_datap->db_type = M_IOCACK;
	    qreply(wq, bp);
	    break;

#if MMSC
	  case SIOC_MMSC_RLS:
	    mmsc_enb = 0;
	    goto mmsc_set;

	  case SIOC_MMSC_GRAB:
	    mmsc_enb = 1;

	  mmsc_set:
	    SIO_LOCK_PORT(port);
	    
	    /* this ioctl may only be issued on the MMSC port (not the
	     * tty port). No need to check for console, SN00, etc.
	     * since this queue only exists on appropriate systems.
	     */
	    if (wq != upper->sio_mmsc_wq) {
		SIO_UNLOCK_PORT(port);
		bp->b_datap->db_type = M_IOCNAK;
		qreply(wq, bp);
		break;
	    }

	    /* set MMSC mode and we're done */

	    if (mmsc_enb)
		UPPER(port)->sio_state |= (SIO_USE_MMSC | SIO_CONT_MMSC);
	    else
		UPPER(port)->sio_state &= ~SIO_CONT_MMSC;
	    SIO_UNLOCK_PORT(port);
	    bp->b_datap->db_type = M_IOCACK;
	    qreply(wq, bp);

	    /* kick off transmitter again in case it was stopped */
	    sio_start_tx(port);
	    break;
#endif

	  case TCBLKMD:
	    SIO_LOCK_PORT(port);
	    upper->sio_iflag |= IBLKMD;
	    SIO_UNLOCK_PORT(port);
	    iocp->ioc_count = 0;
	    bp->b_datap->db_type = M_IOCACK;
	    qreply(wq, bp);
	    break;
	    
	  default:
	    bp->b_datap->db_type = M_IOCNAK;
	    qreply(wq, bp);
	    break;
	}
	break;
	
      default:
      error:
	sdrv_error(wq, bp);
    }
    
    return(0);
}

/*
 * Get a new buffer.
 */
static mblk_t*
#if MMSC
sio_getbp(sioport_t *port, uint_t pri, int force_merge)
#else
sio_getbp(sioport_t *port, uint_t pri)
#endif
{
    mblk_t *bp;
    mblk_t *rbp;
    int size;
    int size0;
    struct upper *upper = port->sio_upper;
    
    ASSERT(sio_port_islocked(port));

    dprintf(("getbp port 0x%x\n", port));

#if MMSC
    if (force_merge) {

	/* force a merge to delimit an mmsc input packet. Depending on
	 * whether or not the mmsc state machine indicates we are in
	 * an input packet or not, the current accumulated input bytes
	 * should go to one or the other of the input messages to be
	 * sent up the correct stream.
	 */
	if ((rbp = upper->sio_rbp) && (rbp->b_wptr > rbp->b_rptr)) {
	    if (upper->sio_state & SIO_MMSC_IN) {
		str_conmsg(&upper->sio_rmsg_mmsc, &upper->sio_rmsge_mmsc, rbp);
		upper->sio_rmsg_mmsc_len += (rbp->b_wptr - rbp->b_rptr);
	    }
	    else {
		/* If tty port is not open, just throw the message
		 * away.
		 */
		if (upper->sio_rq == 0)
		    freemsg(rbp);
		else {
		    str_conmsg(&upper->sio_rmsg, &upper->sio_rmsge, rbp);
		    upper->sio_rmsg_len += (rbp->b_wptr - rbp->b_rptr);
		}
	    }
	    upper->sio_rbp = 0;
	}
    }
#endif

    rbp = upper->sio_rbp;
    
    /*
     *      if overflowing or current buffer empty, do not 
     *	need another buffer.
     */
    if ( ( RMSG_LEN(upper) >= MAX_RMSG_LEN ) ||
	( rbp && rbp->b_rptr >= rbp->b_wptr ) ) {
	bp = NULL;
    } 
    else {
	/*
	 *    get another buffer, but always keep room to grow.
	 *    this helps prevent deadlock
	 */
	size0 = (upper->sio_rbsize += upper->sio_rbsize / 4);
	if (size0 > MAX_RBUF_LEN)
	    size0 = upper->sio_rbsize = MAX_RBUF_LEN;
	else if (size0 < MIN_RMSG_LEN)
	    size0 = MIN_RMSG_LEN;
	size = size0;
	
	SIO_UNLOCK_PORT(port);
	do
	    bp = allocb(size, pri);
	while(bp == 0 && pri == BPRI_HI && (size >>= 2) >= MIN_RMSG_LEN);

	SIO_LOCK_PORT(port);

	if (bp == 0 && !(upper->sio_state & SIO_SE_PENDING) ) {
	    SIO_UNLOCK_PORT(port);
	    bp = str_allocb(size0, upper->sio_rq, BPRI_HI);
	    SIO_LOCK_PORT(port);
	    if (!bp)
		upper->sio_state |= SIO_SE_PENDING;
	}
    }
    
    rbp = upper->sio_rbp; /* reload after port unlock/relock */

    if ( !rbp ) {
	upper->sio_rbp = bp;
    }
    else if ( (bp) || (rbp->b_wptr >= rbp->b_datap->db_lim) ) {
	
	/*
	 * we have an old buffer and a new buffer, or old buffer is
	 * full 
	 */
#if MMSC
	if (upper->sio_state & SIO_MMSC_IN) {
	    str_conmsg(&upper->sio_rmsg_mmsc, &upper->sio_rmsge_mmsc, rbp);
	    upper->sio_rmsg_mmsc_len += (rbp->b_wptr - rbp->b_rptr);
	}
	else
#endif
	{
	    /* if tty port is not open, just toss the message */
	    if (upper->sio_rq == 0)
		freemsg(rbp);
	    else {
		str_conmsg(&upper->sio_rmsg, &upper->sio_rmsge, rbp);
		upper->sio_rmsg_len += (rbp->b_wptr - rbp->b_rptr);
	    }
	}
	upper->sio_rbp = bp;
    }
    
    if ( (RMSG_LEN(upper) >= XOFF_RMSG_LEN) || (!upper->sio_rbp) )
	sio_iflow ( port, SEND_XOFF, !IGNORE_IXOFF );
    
    return(upper->sio_rbp);
}

/*
 * This routine puts characters in upstream queue. 
 * The routine is slow and should be called infrequently.
 */
static void
sio_slowr(sioport_t *port, uchar_t c)
{
    mblk_t *bp;
    struct upper *upper = port->sio_upper;
    
    ASSERT(sio_port_islocked(port));

    dprintf(("slowr 0x%x\n", port));

    if (upper->sio_iflag & IBLKMD)	/* this kludge apes the old */
	return;			/* block mode hack */
    
    /*   get a buffer if we have none  */
    if ( !(bp = upper->sio_rbp) &&
	!(bp = GETBP(port, BPRI_HI)) ) {
	
	upper->sio_allocb_fail++;
	return;
    }
    
    /*
     *   Put the character into the buffer and 
     *   send the buffer if it is full
     */
    *bp->b_wptr = c;
    if (++bp->b_wptr >= bp->b_datap->db_lim) {
	(void)GETBP(port, BPRI_LO);	/* send buffer when full */
    }
}

/*
 * various upcalls that modify the next char received
 */
static void
sio_post_ncs(sioport_t *port, int ncs)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("ncs port 0x%x %d\n", port, ncs));

    upper->sio_ncs |= ncs;
}

/*
 * Interrupt handler for input character/error.
 * called without lock since this comes from streams_interrupt
 */
static void
sio_rx(sioport_t *port)
{
    char c;
    int ncs;
    mblk_t *bp;
    struct upper *upper;

    ASSERT(sio_port_islocked(port) == 0);
    SIO_LOCK_PORT(port);
    if ((upper = port->sio_upper) == 0) {
	SIO_UNLOCK_PORT(port);
	return;
    }

    upper->sio_state &= ~SIO_RX_STRINTR;
    
    dprintf(("rx port 0x%x\n", port));

    /* must be open to input */
    if ( (upper->sio_state & (SIO_ISOPEN | SIO_WOPEN)) != SIO_ISOPEN ) {
	if ( !(upper->sio_state & (SIO_ISOPEN | SIO_WOPEN)) )
	    sio_zap(port, upper->sio_cflag & HUPCL);
	else
	    sio_rclr(port);	/* just forget it if partly open */
	goto done_unlock;
    }
    
    /*
     *   Must be reading, to read.  DCD must be on for modem 
     *  port to read
     */ 
    if ( !(upper->sio_cflag & CREAD) ) {
	sio_rclr(port);
	goto done_unlock;
    }
    
    /* process all available characters */
    
    upper->sio_state &= ~SIO_CLOGGED;

    while(upper->sio_rx_prod != upper->sio_rx_cons) {
      reload:
	while(upper->sio_rx_prod != upper->sio_rx_cons) {
	    c = upper->sio_rx_data[upper->sio_rx_cons & CBUFMASK];
	    ncs = upper->sio_rx_status[upper->sio_rx_cons & CBUFMASK];

	    /* make sure we'll have somewhere to put this byte if we 
	     * decide to keep it. If not, that probably means the upstream
	     * is congested. We should just stop processing characters
	     * rather than throwing this byte away.
	     */
	    ASSERT(upper->sio_rx_prod != upper->sio_rx_cons);

	    if (!(bp = upper->sio_rbp)) {
		/* get a buffer if we have none */
		if (!(bp = GETBP(port, BPRI_HI))) {
		    upper->sio_allocb_fail++;
		    upper->sio_state |= SIO_CLOGGED;
		    goto loop_done;	/* forget it no buffer available */
		}

		/* GETBP releases and reacquires the port lock: make
		 * sure the char we have is still valid
		 */
		ASSERT(upper->sio_rbp);
		goto reload;
	    }

	    ASSERT(upper->sio_rx_prod != upper->sio_rx_cons);
#if MMSC
	    /* scan for incoming MMSC packets on the console when the
	     * MMSC port is open.
	     */
	    if (IS_CONSOLE(port) &&
# if SN0
		!private.p_sn00 &&
# endif
		upper->sio_mmsc_wq) {

		/* MMSC escape packet state machine */
		switch(upper->sio_mmsc_state) {

		  case FS_DONE:
		    /* packet is now fully received and packaged in a
		     * streams buffer. Force a merge of the buffer to
		     * delimit the packet, and get a new input buffer
		     * for subsequent input. Return to normal
		     * state. Drop through since the first char
		     * received after terminating the previous packet
		     * may be the esc char starting the next packet.
		     */
		    if ((bp = sio_getbp(port, BPRI_LO, 1)) == 0) {
		      allocfail:
			upper->sio_allocb_fail++;
			upper->sio_state |= SIO_CLOGGED;
			goto loop_done;	/* forget it no buffer available */
		    }
		    ASSERT(upper->sio_state & SIO_MMSC_IN);
		    upper->sio_state &= ~SIO_MMSC_IN;
		    upper->sio_mmsc_state = FS_NORMAL;

		  case FS_NORMAL:
		    /* normal input state. We are checking for an
		     * escape char indicating the beginning of an MMSC
		     * packet
		     */
		    if (c == MMSC_ESCAPE_CHAR) {
			/* just update the state and continue. This
			 * char is tossed.
			 */
			upper->sio_mmsc_state = FS_SAW_ESC;
			upper->sio_rx_cons++;
			ASSERT(RX_PTRS_VALID(upper));
			continue;
		    }
		    break;

		  case FS_SAW_ESC:
		    /* we previously saw an escape char. An esc char
		     * is sent as normal input by sending it twice. If
		     * we see another one now, we just pass it through
		     * and return to normal state. Otherwise we are
		     * beginning an MMSC packet
		     */
		    if (c == MMSC_ESCAPE_CHAR) {
			/* this is just an escaped escape char. Pass
			 * the second one through and return to normal.
			 */
			upper->sio_mmsc_state = FS_NORMAL;
		    }
		    else {
			/* this was the status byte. We don't
			 * interpret it, just pass it thru and advance
			 * the state. We are now beginning an MMSC
			 * packet, so force a merge of the current
			 * input buffer and allocate a new one.
			 */
			if ((bp = sio_getbp(port, BPRI_LO, 1)) == 0)
			    goto allocfail;
			ASSERT((upper->sio_state & SIO_MMSC_IN) == 0);
			upper->sio_state |= SIO_MMSC_IN;
			upper->sio_mmsc_state = FS_SAW_STATUS;
		    }
		    break;

		  case FS_SAW_STATUS:
		    /* this char is the high order byte of the 16 bit
		     * length field. Jot it down so we'll know how
		     * many bytes long the data is.
		     */
#ifdef MMSC_ASCII_DEBUG
		    upper->sio_mmsc_packet_len = ((c - '0') & 0xff) * 10;
#else
		    upper->sio_mmsc_packet_len = (c & 0xff) << 8;
#endif
		    upper->sio_mmsc_state = FS_SAW_LEN_MSB;
		    break;

		  case FS_SAW_LEN_MSB:
		    /* This char is the low order byte of the 16 bit
		     * length field.
		     */
#ifdef MMSC_ASCII_DEBUG
		    upper->sio_mmsc_packet_len += ((c - '0') & 0xff);
#else
		    upper->sio_mmsc_packet_len |= (c & 0xff);
#endif
		    /* account for checksum at end of packet */
		    upper->sio_mmsc_packet_len++;

		    upper->sio_mmsc_state = FS_SAW_LEN_LSB;
		    break;

		  case FS_SAW_LEN_LSB:
		    /* we now know the packet length. Just count down
		     * the bytes.  When we're done, we still have to
		     * advance to one more state since the last byte
		     * is not yet in a buffer.
		     */
		    if (-- upper->sio_mmsc_packet_len == 0)
			upper->sio_mmsc_state = FS_DONE;
		    break;
		}
	    }
	    else {
		/* make sure we aren't left in MMSC input mode in case
		 * someone closes the mmsc port during the middle of
		 * an input packet
		 */
		upper->sio_state &= ~SIO_MMSC_IN;
	    }
		
#endif /* MMSC */

	    ASSERT(upper->sio_rx_prod != upper->sio_rx_cons);
	    if (kdebug &&
		c == DEBUG_CHAR &&
#if MMSC
		!(upper->sio_state & SIO_MMSC_IN) &&
#endif
		IS_CONSOLE(port)) {

		/* toss this char */
		upper->sio_rx_cons++;
		ASSERT(RX_PTRS_VALID(upper));

		/* don't need to disable DMA here, symmon does it for us */
		SIO_UNLOCK_PORT(port);
		debug("ring");
		SIO_LOCK_PORT(port);

		continue;
	    }

	    upper->sio_rx_cons++;
	    ASSERT(RX_PTRS_VALID(upper));
	    
	    SYSINFO.rawch++;
	
	    /*
	     *     XON and XOFF
	     *     Start/Stop output (if permitted)
	     */ 
	    if ( upper->sio_iflag & IXON ) {
		
		uchar_t cs = c;
		
		if (upper->sio_iflag & ISTRIP)
		    cs &= 0x7f;
		
		if (upper->sio_state & SIO_TXSTOP
		    && (cs == upper->sio_cc[VSTART]
			|| (upper->sio_iflag & IXANY
			    && (cs != upper->sio_cc[VSTOP]
				|| upper->sio_line == LDISC0)))) {
		    
		    upper->sio_state &= ~SIO_TXSTOP;
		    if (DOWN_ENABLE_TX(port) < 0) {
			    upper->sio_state |= SIO_HW_ERR;
			    SIO_UNLOCK_PORT(port);
			    return;
		    }
		    SIO_UNLOCK_PORT(port);
		    sio_start_tx(port); /* restart output */
		    SIO_LOCK_PORT(port);
		    
		    if (cs == upper->sio_cc[VSTART])
			continue;
		} 
		
		else if (SIO_LIT & upper->sio_state) {
		    upper->sio_state &= ~SIO_LIT;
		} 
		
		else if (cs == upper->sio_cc[VSTOP]) {
		    
		    upper->sio_state |= SIO_TXSTOP;
		    if (DOWN_DISABLE_TX(port) < 0) {
			    upper->sio_state |= SIO_HW_ERR;
			    SIO_UNLOCK_PORT(port);
			    return;
		    }
		    sio_stop_tx(port); /* stop output */
		    continue;
		} 
		
		else if (cs == upper->sio_cc[VSTART]) {
		    continue;	/* ignore extra control-Qs */
		} 
		
		else if (cs == upper->sio_cc[VLNEXT]
			 && upper->sio_line != LDISC0) {
		    upper->sio_state |= SIO_LIT; /* just note escape */
		}
	    }
	
	    /*
	     *   we got a Break Interrupt
	     */
	    if (ncs & NCS_BREAK) {
		
		if (upper->sio_iflag & IGNBRK)
		    continue;	/* ignore it if ok */
		
		if (upper->sio_iflag & BRKINT) {
		    
		    sio_flushr(port);
		    SIO_UNLOCK_PORT(port);
		    (void)putctl1(upper->sio_rq->q_next,
				  M_FLUSH, FLUSHRW);
		    (void)putctl1(upper->sio_rq->q_next,
				  M_PCSIG, SIGINT);
		    SIO_LOCK_PORT(port);
		    continue;
		}
		
		if (upper->sio_iflag & PARMRK) {
		    
		    sio_slowr(port, '\377');
		    sio_slowr(port, '\000');
		}
		
		c = '\000';
	    }
	
	    else if (ncs & (NCS_PARITY | NCS_FRAMING | NCS_OVERRUN)) {
		
		if (ncs & NCS_OVERRUN) {
		    upper->sio_over++; /* count overrun */
		}
		
		if (upper->sio_iflag & IGNPAR) {
		    continue;
		} 
		
		else if ( !(upper->sio_iflag & INPCK) ) {
		    /* ignore input parity errors if asked */
		} 
		
		else if (ncs & (NCS_PARITY | NCS_FRAMING)) {
		    
		    if (ncs & NCS_FRAMING)
			upper->sio_fe++;
		    
		    if (upper->sio_iflag & PARMRK) {
			sio_slowr(port, '\377');
			sio_slowr(port, '\000');
		    } 
		    else {
			c = '\000';
		    }
		}
	    } 
	
	    else if (upper->sio_iflag & ISTRIP) { 
		c &= 0x7f;
	    } 
	
	    else if (c == '\377' && upper->sio_iflag & PARMRK) {
		sio_slowr(port, '\377');
	    }
	
	    *bp->b_wptr = c;
	
	    if (++bp->b_wptr >= bp->b_datap->db_lim) {
		(void)GETBP(port, BPRI_LO); /* send when full */
	    }
	}
	
	/* see if the hardware has any more data
	 */
	if (sio_pull_cbuf(port) < 0) {
		upper->sio_state |= SIO_HW_ERR;
		SIO_UNLOCK_PORT(port);
		return;
	}
    }
  loop_done:
    
    if (!upper->sio_rsrv_cnt && upper->sio_rbp) {
	
	if (upper->sio_rsrv_duration <= 0
	    || upper->sio_rsrv_duration > HZ / 10) {
	    
	    if (upper->sio_state & SIO_ISOPEN) {
#if MMSC
		int enb_mmsc_rq, enb_rq;

		/* figure out which queues need to be enabled. A queue
		 * needs to be enabled if there is an rmsg waiting, or
		 * if the current input buffer would eventually go to
		 * that queue.
		 */
		enb_mmsc_rq = upper->sio_rmsg_mmsc || 
		    (upper->sio_rbp && (upper->sio_state & SIO_MMSC_IN));
		enb_rq = upper->sio_rmsg || 
		    (upper->sio_rbp && !(upper->sio_state & SIO_MMSC_IN));

#endif
		SIO_UNLOCK_PORT(port);
		if (
#if MMSC
		    enb_rq && upper->sio_rq &&
#endif
		    canenable(upper->sio_rq))
		    qenable(upper->sio_rq);
#if MMSC
		if (enb_mmsc_rq && upper->sio_mmsc_rq &&
		    canenable(upper->sio_mmsc_rq))
		    qenable(upper->sio_mmsc_rq);
#endif
		SIO_LOCK_PORT(port);
	    }
	} 
	
	else {
	    SIO_UNLOCK_PORT(port);
	    (void)STREAMS_TIMEOUT(sio_rsrv_timer,
				  (caddr_t)port, upper->sio_rsrv_duration);
	    SIO_LOCK_PORT(port);
	    upper->sio_rsrv_cnt = MAX_RSRV_CNT;
	}
    }

  done_unlock:
    SIO_UNLOCK_PORT(port);
}

/*
 * Process Carrier_On Interrupt for a given port.
 */
static void
sio_con(sioport_t *port)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("con port 0x%x\n", port));

    if (upper->sio_state & SIO_WOPEN)
	sv_broadcast(&upper->sio_sv); 	/* awaken open() requests */
}

/*
 * Process Carrier_Off Interrupt for a given port.
 */
static void
sio_coff(sioport_t *port)
{
    struct upper *upper;

    ASSERT(sio_port_islocked(port) == 0);

    SIO_LOCK_PORT(port);
    if ((upper = port->sio_upper) == 0) {
	SIO_UNLOCK_PORT(port);
	return;
    }

    dprintf(("coff port 0x%x\n", port));

    /* if there are some bytes still hanging around in the rx buffer,
     * have a go at reading them before dropping the port
     */
    if (upper->sio_rx_prod != upper->sio_rx_cons) {
	SIO_UNLOCK_PORT(port);
	sio_rx(port);
	SIO_LOCK_PORT(port);
    }

    if ( !(upper->sio_cflag & CLOCAL) && /* worry only for an open modem */
	(upper->sio_state & SIO_ISOPEN) ) {
	
	sio_zap(port, HUPCL);	/* kill the modem */
	SIO_UNLOCK_PORT(port);
	flushq(upper->sio_wq, FLUSHDATA);
	(void)putctl1(upper->sio_rq->q_next, M_FLUSH, FLUSHW);
	(void)putctl(upper->sio_rq->q_next, M_HANGUP);
    }
    else
	SIO_UNLOCK_PORT(port);
}

/*
 * process delta DCD interrupt
 */
static void
sio_dDCD(sioport_t *port, int dcd)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("ddcd port 0x%x\n", port));

    /* If the port's state shows that DCD is active 
     * but the arg doesn't, reset the port's DCD state 
     * and kill the port's process
     *
     * If the port's state shows DCD inactive, but the 
     * arg does, set the port's DCD state to active 
     * and process the stream waiting for the carrier
     */
    if ((upper->sio_state & SIO_DCD) && !dcd) {
	SYSINFO.mdmint++;
	upper->sio_state &= ~SIO_DCD;
	SIO_UNLOCK_PORT(port);
	streams_interrupt((strintrfunc_t)sio_coff, port, NULL, NULL);
	SIO_LOCK_PORT(port);
    } 
    else if (!(upper->sio_state & SIO_DCD) && dcd) {
	SYSINFO.mdmint++;
	upper->sio_state |= SIO_DCD;
	sio_con(port);
    }
}

/*
 * process delta CTS interrupt
 */
static void
sio_dCTS(sioport_t *port, int cts)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("dcts port 0x%x\n", port));

    /* If the port's state shows that CTS is active but 
     * the arg doesn't, reset the port's CTS state 
     *
     * If the port doesn't show active CTS, but the arg
     * does, set the port's CTS state to active and 
     * restart transmit
     */
    if ((upper->sio_state & SIO_CTS) && !cts) {
	SYSINFO.mdmint++;
	upper->sio_state &= ~SIO_CTS;
	if (USE_SOFT_HFC(upper))
		if (DOWN_DISABLE_TX(port) < 0) {
			upper->sio_state |= SIO_HW_ERR;
			return;
		}
    } 
    else if (!(upper->sio_state & SIO_CTS) && cts) {
	SYSINFO.mdmint++;
	upper->sio_state |= SIO_CTS;
	if (USE_SOFT_HFC(upper))
		if (DOWN_ENABLE_TX(port) < 0) {
			upper->sio_state |= SIO_HW_ERR;
			return;
		}
	SIO_UNLOCK_PORT(port);
	streams_interrupt((strintrfunc_t)sio_start_tx, port, NULL, NULL);
	SIO_LOCK_PORT(port);
    }
}

/*
 * Send output to a port. Called to get things rolling from a process
 * or to keep things rolling from the TX lowat interrupt. Entry is
 * without the port lock since this can come from streams_interrupt
 */
static void
sio_tx(sioport_t *port)
{
    char c;
    mblk_t *wbp;
    int sent, bytes;
    struct upper *upper;

    ASSERT(sio_port_islocked(port) == 0);
    SIO_LOCK_PORT(port);
    if ((upper = port->sio_upper) == 0) {
	SIO_UNLOCK_PORT(port);
	return;
    }
    upper->sio_state &= ~SIO_TX_STRINTR;

    dprintf(("tx on port 0x%x\n", port));

    sent = 0;
    /* send all we can */
    while (1) {
	
	ASSERT(sio_port_islocked(port));

	if (!(upper->sio_state & SIO_ISOPEN)
	    || (upper->sio_state & (SIO_BREAK | SIO_BREAK_QUIET
				    | SIO_TXSTOP | SIO_TIMEOUT))
	    && !(upper->sio_state & (SIO_TX_TXON | SIO_TX_TXOFF))) {
	    
	    sio_stop_tx(port);
	    eprintf(("stop_tx: not open or break or xon/xoff"));
	    goto done_unlock;
	}
	
	/*   send XON or XOFF     */
	if (upper->sio_state & SIO_TX_TXON) {
	    
	    c = upper->sio_cc[VSTART];
	    if ((bytes = sio_write_tx_buf(port, &c, 1)) < 1)
		break;

	    upper->sio_state &= ~(SIO_TX_TXON | SIO_TX_TXOFF | SIO_BLOCK);
	} 
	
	else if (upper->sio_state & SIO_TX_TXOFF) {
	    
	    c = upper->sio_cc[VSTOP];
	    if ((bytes = sio_write_tx_buf(port, &c, 1)) < 1)
		break;

	    upper->sio_state &= ~SIO_TX_TXOFF;
	    upper->sio_state |= SIO_BLOCK;
	} 
	
	else {
#if MMSC
	    /* If there is currently a tty output packet in progress
	     * and MMSC output is pending, suspend the tty output and
	     * let the MMSC output run.
	     */
	    if (upper->sio_wbp &&
		(upper->sio_state &
		 (SIO_MMSC_OUT | SIO_USE_MMSC)) == SIO_USE_MMSC) {
		if (upper->sio_wq)
		    putbq(upper->sio_wq, upper->sio_wbp);
		else
		    /* We lose the message here, but this will
		     * probably never happen since the tty port has to
		     * be closed and init holds it open forever.
		     */
		    freemsg(upper->sio_wbp);
		
		upper->sio_wbp = 0;
	    }
#endif

	    if (!(wbp = upper->sio_wbp)) {	/* get another msg */

#if MMSC
		/* If the MMSC is active, we will take *all* output
		 * from it and just leave the tty port dangling until
		 * we are no longer transmitting on behalf of the
		 * MMSC.
		 */
		if (upper->sio_state & SIO_USE_MMSC) {
		    /* don't start any output while kernel printf has
		     * console ownership. We can't afford to
		     * interleave or the MMSC packet will be
		     * corrupted.
		     */
		    if (upper->sio_mmsc_wq && !(upper->sio_state & SIO_PRINTF)) {
			wbp = getq(upper->sio_mmsc_wq);
			ASSERT(!wbp || wbp->b_datap->db_type != M_IOCTL);
		    }
		    else
			wbp = 0;

		    /* if there is no more data to send and we are not
		     * requested to continue with MMSC output, revert
		     * back to tty output
		     */
		    if (wbp == 0 && !(upper->sio_state & SIO_CONT_MMSC)) {
			wbp = upper->sio_wq ? getq(upper->sio_wq) : 0;
			upper->sio_state &= ~(SIO_USE_MMSC | SIO_MMSC_OUT);
		    }
		    else
			upper->sio_state |= SIO_MMSC_OUT;
		}
		else {
		    wbp = upper->sio_wq ? getq(upper->sio_wq) : 0;
		    upper->sio_state &= ~SIO_MMSC_OUT;
		}
#else
		wbp = getq(upper->sio_wq);
#endif

		if (!wbp) {
		    dprintf(("tx: stop 3\n"));
		    sio_stop_tx(port);
		    goto done_unlock;
		}
		
		switch (wbp->b_datap->db_type) {
		    
		  case M_DATA:
		    upper->sio_wbp = wbp;
		    break;
		    
		    /* must wait until output drained */
		  case M_DELAY:	/* start output delay */
		    
		    if (sent == 0 && CBUF_TX_EMPTY(upper)) {
			upper->sio_state |= SIO_TIMEOUT;
			SIO_UNLOCK_PORT(port);
			upper->sio_tid = 
			    STREAMS_TIMEOUT(sio_delay,
					    (caddr_t)port,
					    IOCARG(wbp->b_rptr));
			freemsg(wbp);
			SIO_LOCK_PORT(port);
			continue;
		    } 	
		    else {
			upper->sio_wbp = 0;
			SIO_UNLOCK_PORT(port);
			putbq(upper->sio_wq, wbp);
			dprintf(("sio_tx return 1\n"));
			goto done;
		    }
		    
		    /* must wait until output drained */
		  case M_IOCTL:
		    if (sent == 0 && CBUF_TX_EMPTY(upper)) {
			sio_i_ioctl(port, wbp);
			continue;
		    } 
		    else {
			upper->sio_wbp = 0;
			SIO_UNLOCK_PORT(port);
			putbq(upper->sio_wq, wbp);
			dprintf(("sio_tx return 2\n"));
			goto done;
		    }
		    
		  default:
		    panic("bad uart msg");
		}
	    }
	    
	    if (wbp->b_rptr >= wbp->b_wptr) {
		upper->sio_wbp = wbp->b_cont;
		SIO_UNLOCK_PORT(port);
		freeb(wbp);
		SIO_LOCK_PORT(port);
		continue;
	    }
	    
	    bytes = sio_write_tx_buf(port, (char*)wbp->b_rptr,
				     (int)(wbp->b_wptr - wbp->b_rptr));
	    if (bytes <= 0)
		break;
	    wbp->b_rptr += bytes;
	}
	
	sent += bytes;
	SYSINFO.outch += bytes;
    }
    
    /*
     * If the queue is now empty, put an empty data block on it
     * to prevent a close from finishing prematurely.
     */
    if ( !upper->sio_wq->q_first && upper->sio_wbp) {
	SIO_UNLOCK_PORT(port);
	if (wbp = allocb(0, BPRI_HI))
	    putbq(upper->sio_wq, wbp);
	SIO_LOCK_PORT(port);
    }

    dprintf(("sio_tx done\n"));

  done_unlock:
    SIO_UNLOCK_PORT(port);
  done:
    ASSERT(sio_port_islocked(port) == 0);
}

/*
 * Stop transmission on a port.
 */
/*ARGSUSED*/
static void
sio_stop_tx(sioport_t *port)
{
    ASSERT(sio_port_islocked(port));

    dprintf(("stop tx port 0x%x\n", port));
}

/*
 * start transmission on a port. can be called from streams_interrupt
 * so the port must be unlocked on entry
 */
static void
sio_start_tx(sioport_t *port)
{
    ASSERT(sio_port_islocked(port) == 0);

    dprintf(("start tx port 0x%x\n", port));

    sio_tx(port);
}

/*
 * Stop the receive process.
 */
static void
sio_stop_rx(sioport_t *port, int hup)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("stop rx port 0x%x\n", port));

    /*
     *    deassert RTS, DTR if hup != 0 
     *    deassert output handshake signals on tty port
     */
    if (hup) {
	if ((DOWN_SET_DTR(port, 0) < 0
	     || DOWN_SET_RTS(port, 0) < 0)
	    && upper) {
		upper->sio_state |= SIO_HW_ERR;
		return;
	}
    }

    if (upper)
	upper->sio_state &= ~(SIO_RTS | SIO_DTR);
    
    /*
     * disable input interrupts
     */
    if (DOWN_NOTIFICATION(port, N_ALL_INPUT, 0) < 0 && upper)
	    upper->sio_state |= SIO_HW_ERR;
}

/*
 * Start the receive process.
 */
static int
sio_start_rx(sioport_t *port)
{
    int notify;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("start rx port 0x%x\n", port));

    if (DOWN_SET_DTR(port, 1) < 0 ||
	DOWN_SET_RTS(port, 1) < 0) {
	    upper->sio_state |= SIO_HW_ERR;
	    return(0);
    }
    
    upper->sio_state |= (SIO_RTS | SIO_DTR);

    notify = N_DATA_READY | N_BREAK | N_ALL_ERRORS;

    if (upper->sio_state & (SIO_WOPEN | SIO_ISOPEN)) {

	/* watch carrier-detect only if this is an open modem port */
	if ( !(upper->sio_cflag & CLOCAL))
	    notify |= N_DDCD;
	
	/* Watch CTS if this is a hardware flow control port and the
	 * hardware isn't handling it for us
	 */
	if ((upper->sio_cflag & CNEW_RTSCTS) &&
	    !(upper->sio_state & SIO_AUTO_HFC))
	    notify |= N_DCTS;
    }

    if (DOWN_NOTIFICATION(port, notify, 1) < 0) {
	    upper->sio_state |= SIO_HW_ERR;
	    return(0);
    }
    
    if (DOWN_QUERY_CTS(port))
	upper->sio_state |= SIO_CTS;
    else
	upper->sio_state &= ~SIO_CTS;
    
    if (DOWN_QUERY_DCD(port))
	upper->sio_state |= SIO_DCD;
    else
	upper->sio_state &= ~SIO_DCD;
    
    return(upper->sio_state & SIO_DCD);
}

/*
 * Port configuration.
 */

static int
sio_config(sioport_t *port, tcflag_t cflag, speed_t ospeed, struct termio *tp)
{
    tcflag_t delta_cflag;
    int delta_ospeed;
    int byte_size, err;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));
    
    dprintf(("config port 0x%x\n", port));

    delta_cflag = (cflag ^ upper->sio_cflag)
	& (CLOCAL|CSIZE|CSTOPB|PARENB|PARODD|CNEW_RTSCTS);
    delta_ospeed = (ospeed != upper->sio_ospeed);
    
    if (tp)
	upper->sio_termio = *tp;
    
    upper->sio_cflag = cflag;
    upper->sio_ospeed = upper->sio_ispeed = ospeed;
    
    /* 
     * always configures during open in case the last process set up 
     * the port to use the external clock and did not set it back to use
     * the internal BRG
     */
    if (delta_cflag || delta_ospeed || !(upper->sio_state & SIO_ISOPEN)) {
	
	if (ospeed == B0) { 	/* hang up line if asked */
	    SIO_UNLOCK_PORT(port);
	    sio_coff(port);
	    SIO_LOCK_PORT(port);
	    sio_zap(port, HUPCL);
	} 
	
	else {
	    
	    /* set number of data bits */
	    switch (cflag & CSIZE) {
	      case CS5:
		byte_size = 5;
		break;
	      case CS6:
		byte_size = 6;
		break;
	      case CS7:
		byte_size = 7;
		break;
	      case CS8:
		byte_size = 8;
		break;
	    }

	    if ((err = DOWN_CONFIG(port,
				   ospeed,
				   byte_size,
				   cflag & CSTOPB,
				   cflag & PARENB,
				   cflag & PARODD)) < 0 ||
		DOWN_NOTIFICATION(port, N_ALL, 0) < 0) {
		    upper->sio_state |= SIO_HW_ERR;
		    return(-1);
	    }
	    if (err > 0)
		return(1);
	    
	    upper->sio_state &= ~SIO_TX_NOTIFY;

	    (void)sio_start_rx(port);
	}
    }
    return(0);
}

/*
 * Flow control on a port.
 */
static void
sio_iflow(sioport_t *port, int send_xon, int ignore_ixoff)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    dprintf(("iflow port 0x%x\n", port));

    /* If hardware supports its own HFC, we don't need to do anything */
    if (upper->sio_state & SIO_AUTO_HFC)
	return;

    /*   if hardware flow control   */
    if ((upper->sio_cflag & CNEW_RTSCTS) &&
	(upper->sio_state & SIO_AUTO_HFC) == 0) {
	if (send_xon) {
	    if (DOWN_SET_RTS(port, 1) < 0) {
		    upper->sio_state |= SIO_HW_ERR;
		    return;
	    }

	    upper->sio_state |= SIO_RTS;
	} 
	
	else if (!send_xon) {
	    if (DOWN_SET_RTS(port, 0) < 0) {
		    upper->sio_state |= SIO_HW_ERR;
		    return;
	    }

	    upper->sio_state &= ~SIO_RTS;
	}
    }
    
    if (!ignore_ixoff && !(upper->sio_iflag & IXOFF))
	return;
    
    else { 
	if (send_xon && upper->sio_state & SIO_BLOCK) {
	    upper->sio_state |= SIO_TX_TXON;
	    SIO_UNLOCK_PORT(port);
	    sio_start_tx(port);
	    SIO_LOCK_PORT(port);
	}
	else if (!send_xon && !(upper->sio_state & SIO_BLOCK)) {
	    upper->sio_state |= SIO_TX_TXOFF;
	    upper->sio_state &= ~SIO_TX_TXON;
	    SIO_UNLOCK_PORT(port);
	    sio_start_tx(port);
	    SIO_LOCK_PORT(port);
	}
    }
}

#ifdef IS_DU			/* main system uart */

void
du_init()
{
#if EVEREST
    extern void everest_du_init(void);
    everest_du_init();
#endif
}

/*
 * du_conpoll - see if a receive char - used for debugging ONLY
 * Only poll once per second to limit impact if called very often.
 */
int du_chars_lost;
void
du_conpoll(void)
{
	char c;
#ifndef IP32
	static int lasttime;
	int now, cnt, do_read;
	struct upper *upper;
#endif
	extern void console_debug_poll(void);

	if (!kdebug || console_is_tport)
		return;

#if (IP27 || IP30 || IP32 || EVEREST) && !defined(SABLE)
	{
	    extern int console_device_attached;
	    if (!console_device_attached) {
		console_debug_poll();
		return;
	    }
	}
#endif

	if (!du_port) {
		console_debug_poll();
		return;
	}

#if IP32
	/*
	 * dbgconpoll is != 0 to be here
	 * the rx fct will check for debug break char ^A
	 * and dont worry about  the remote possiblity of losing
	 * a char input, at this point we want to DEBUG hangs
	 * and dont care about no stinkin char ...
	 */
	if (SIO_TRY_LOCK_PORT(du_port) == 0) {
	    if (kdebug) {
		/*
		 * when conpoll is on, the sampling will be about
		 * once every 80 ms from prfintr, if we couldnt
		 * lock the console port for a 14*80ms, go into
		 * the debugger.
		 */
		if (++dbgconpoll > 15) {
		    debug("ring");
		    dbgconpoll = 1; /* reset the console poll watchdog */
		}
	    }
	    return;
	} else {
	    dbgconpoll = 1; /* reset the console poll watchdog */
	    DOWN_READ(du_port, &c, 1);
	    SIO_UNLOCK_PORT(du_port);
	}
#else

	/*
	 * We call into streams routines that expect to only be called
	 * with a valid thread context. Several pieces of code in the
	 * kernel call du_conpoll while on the idle stack or from intr
	 * routines (prfintr, debug locks when called from the scheduler
	 * in idle mode, etc). just return in those cases to avoid problems
	 * in the streams code. we have a kernel thread that calls du_conpoll
	 * once a second anyway.
	 *
	 * Also, if any of the streams spinlocks that'll be needed
	 * in streams_interrupt() are already held, return to avoid
	 * a doubletrip in dhardlocks.
	 */
	if (!curthreadp || spinlock_islocked(&strhead_monp_lock) ||
		spinlock_islocked(&str_intr_lock) ||
		spinlock_islocked(&streams_mon.mon_lock)) {
		return;
	}

	if (private.p_kstackflag != PDA_CURKERSTK)
		return;

	if (SIO_TRY_LOCK_PORT(du_port) == 0)
	    return;

	now = get_timestamp();
	if (now - lasttime > timer_freq) {
	    lasttime = now;

	    /* we attempt to read a char from the console in a manner
	     * that will not drop the char. Note that this can only be
	     * done if the console device is opened by a process, but
	     * then again if that is not the case, we don't care if
	     * the char is dropped anyway.
	     */
	    if (upper = du_port->sio_upper) {
		/* port is open */

		/* check if input buffer is full. If so, don't bother
		 * calling sio_data_ready because it won't be able
		 * to read the debug char anyway, and do the explicit
		 * DOWN_READ to check for the debug char.
		 */
		if ((do_read = (upper->sio_rx_prod -
				upper->sio_rx_cons == CBUFLEN)) == 0)
		    sio_data_ready(du_port);
	    }
	    else
		/* port is not open */
		do_read = 1;

	    /* NOTE if this read gets a char it will be tossed. But 
	     * we only do this when no process has the console open,
	     * or the input cbuf is full. In the former case we don't
	     * care if the char is tossed, and the latter case will
	     * be very rare and should only occur under very heavy
	     * system load.
	     */
#if MMSC
	    /* Note that if we get to this point and read a char
	     * during MMSC packet input, the packet will be corrupt.
	     * This is unavoidable.  We must have this read here as a
	     * last resort to get into the debugger when the console
	     * stream is hung, but there is no way to know if the
	     * character read is within an MMSC packet without knowing
	     * what chars are before it, which we don't know here.
	     * Luckily if we ever get here anyway, it's very likely
	     * the system is sufficiently screwed up that we don't
	     * care if we corrupt an MMSC packet anyway, we just want
	     * to get into the debugger.
	     */
#endif
	    if (do_read) {
		cnt = DOWN_READ(du_port, &c, 1);
		if (cnt > 0) { /* may be negative */
		    du_chars_lost += cnt;
		    if (c == DEBUG_CHAR) {
			SIO_UNLOCK_PORT(du_port);
			debug("ring");
			return;
		    }
		}
	    }
	}
	SIO_UNLOCK_PORT(du_port);
#endif
}

void
du_down_write(sioport_t *port, char *buf, int len)
{
	int cnt;

	while (len > 0) {
		SIO_LOCK_PORT(port);
		cnt = DOWN_DU_WRITE(port,buf,len);
		SIO_UNLOCK_PORT(port);
		if (cnt < 0)
		    return;
		len -= cnt;
		buf += cnt;
	}
}

void
du_down_flush(sioport_t *port)
{
    SIO_LOCK_PORT(port);
    (void) DOWN_DU_FLUSH(port);
    SIO_UNLOCK_PORT(port);
}

#if IP27
static void
du_down_altwrite(char *buf, int len)
{
    sioport_t *elscport;

    du_down_write(du_port, buf, len);

    /* copy output to the diagnostic port if it is open */
    if ((elscport = private.p_elsc_portinfo) &&
	elscport->sio_upper)
	du_down_write(elscport, buf, len);
}
#else
#define du_down_altwrite(buf,len)	du_down_write(du_port,buf,len)
#endif

int
ducons_write(char *buf, int len)
{
    short cr = '\n\r';
    char *cp;
    int olen = len, offset;

    while(len > 0) {

	/* scan for a newline or an escape char */
	for(offset = 0, cp = buf; offset < len; offset++, cp++) {
	    if (*cp == '\n')
		break;
#if MMSC
	    else if (*cp == MMSC_ESCAPE_CHAR)
		break;
#endif
	}

	/* pass through what we saw up to but not including the char
	 * that stopped us
	 */
	du_down_altwrite(buf, offset);

	if (offset != len) {
	    /* if we stopped because of a newline, send a nl/cr
	     * combination.
	     */
	    if (*cp == '\n')
		du_down_altwrite((char*)&cr, 2);
#if MMSC
	    /* if we stopped because of an escape char, we just pass
	     * it over. It's not a printable char, so there is no
	     * reason why anyone would ever send it in a kernel
	     * printf. This eliminates having to decide whether or not
	     * the mmsc device is open or not which is meaningless
	     * during early boot. Best to not confuse the MMSC by just
	     * never sending an esc char.  */
#endif
	    /* skip over the nl or esc char. */
	    offset++;
	}
	buf += offset;
	len -= offset;
    }
    return(olen);
}

void
ducons_flush(void)
{
	if (du_port) {
		SIO_LOCK_PORT(du_port);
		DOWN_DU_FLUSH(du_port);
		SIO_UNLOCK_PORT(du_port);
	}
}

/*ARGSUSED1*/
void dump_dport_info(__psint_t n)
{
#if EVEREST
    extern void everest_dump_dport_info(__psint_t);
    everest_dump_dport_info(n);
#endif
}

/*ARGSUSED1*/
void reset_dport_info(__psint_t n)
{
#if EVEREST
    extern void everest_reset_dport_info(__psint_t);
    everest_reset_dport_info(n);
#endif
}

#ifdef DEBUG
/*ARGSUSED1*/
void dump_dport_actlog(__psint_t n)
{
#if EVEREST
    extern void everest_dump_dport_actlog(__psint_t);
    everest_dump_dport_actlog(n);
#endif
}

#endif /* DEBUG */

#endif /* IS_DU */

#if IS_DU

void
du_conpoll_thread(void)
{
	sema_t thread_sema;

	(void) setmustrun(master_procid);
	initnsema(&thread_sema, 0, "du_conpoll");

	while (1) {
		du_conpoll();

		timeout_nothrd((void(*)())cvsema, &thread_sema, HZ/4);
		psema(&thread_sema, PZERO);
	}
}

void
start_du_conpoll_thread(void)
{
    extern int du_conpoll_pri;

    if (!kdebug || console_is_tport)
	return;

    (void)sthread_create("du_conpoll",		/* Name */
			0,			/* Stack addr (use default) */
			0,			/* Stack size */
			0,			/* Flags */
			du_conpoll_pri,		/* Priority */
		 	KT_PS,			/* Sched flags */
			(st_func_t *)du_conpoll_thread,/* Function name */
			0,			/* Router pointer */
			0, 0, 0);		/* unused args */
}

#endif  /* IS_DU */
