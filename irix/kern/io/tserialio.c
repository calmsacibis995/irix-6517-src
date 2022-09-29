/*
 * Copyright (C) 1986, 1992, 1993, 1994, Silicon Graphics, Inc.
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

/* Timestamped interface upper layer for modular serial i/o driver */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/serialio.h>
#include <ksys/serialio.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/strmp.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/stty_ld.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/cmn_err.h>
#include <sys/callo.h>
#include <sys/pfdat.h>
#include <sys/ddi.h>
#include <sys/mload.h>
/* Fix for pv #481256 */
#include <sys/poll.h>
/* End fix for pv #481256 */

#include <sys/tserialio_pvt.h>

#include <sys/atomic_ops.h>

#ifdef TSERIALIO_TIMEOUT_IS_THREADED
#include <ksys/xthread.h>
xpt_t tsio_thread_info = { 0 };
void start_tsio_timeout_thread(void);
#else
extern void (*tsio_timercallback_ptr)(void); /* from ml/timer.c */
#endif

#ifdef TSERIALIO_TIMEOUT_IS_THREADED

/* The threaded version of the driver is currently used on IP27 & IP30.
 * We're including a couple of scalability fixes for only that version
 * because scalability is not a critical issue on the low-end machines
 * and because we don't have adequate test coverage to ensure that the
 * mods don't have ill side-effects on the non-IOC3 platforms. */

#define TSERIALIO_SCALABLE

/* The IOC3 lower layer needs data-ready notification turned on in
 * order to enable receive interrupts.  Even draining at 1ms intervals,
 * the input buffer can overrun at 38400bps.  This is enabled for
 * the threaded version of the driver which is currently used on IP27
 * & IP30, the IOC3 platforms. */

#define TSERIALIO_NOTIFY_DATA_READY

#endif

void wake_rbs_doorbell(void);

int tsiodevflag = D_MP;        /* required for dynamic loading */
char *tsiomversion = M_VERSION;/* Required for dynamic loading */

/* Locking between the wake_rbs process and the top half */

#define WAKE_RBS_IDLE 0
#define WAKE_RBS_SCANNING 1
#define WAKE_RBS_RESTART 2
int wake_rbs_state=WAKE_RBS_IDLE;

/*
    These are the state transitions for the atomic variable that
    synchronizes between the wake_rbs "process" and the top half

Transitions made by the wake_rbs doorbell:

    CURRENT STATE  NEW STATE  SIDE EFFECT
    -------------  ---------  -----------
    IDLE           SCANNING   restart wake_rbs
    SCANNING       RESTART    none
    RESTART        RESTART    none

Transitions at the end of a pass in wake_rbs:

    CURRENT STATE  NEW STATE  SIDE EFFECT
    -------------  ---------  -----------
    IDLE           IDLE       print error message and exit process
    SCANNING       IDLE       exit process
    RESTART        SCANNING   restart the scan
*/

/* Fix for pv bug  460909. */
#if IP22 || IP32
extern int fastclock;                            /* from ml/timer.c */
#endif
/* End fix for pv bug  460909. */

#define SIO_LOCK_LEVEL SIO_LOCK_MUTEX

/* user rb structures ------------------------------------------- */

/* how this driver works:
 *
 * This driver performs +-1ms accurate timestamping of input
 * serial bytes and scheduling of output serial bytes. It does
 * its work off of either the 1ms  profiler tick at splprof
 * (tsio_timercallback_ptr) on some systems and on others it uses
 * a 1ms xthread periodic timeout.
 *
 * The received and transmitted data passes directly between user-mode
 * and the 1 ms periodic service routine by the use of a lightweight,
 * race-free ringbuffer which is mapped into the user address space.
 * Data transfer requires no system calls.  The urb data structure 
 * encapsulates the state for one ringbuffer between user-mode and 
 * the driver (in only one direction of transfer--so a traditional 
 * TX/RX serialport connection requires two ringbuffers).
 *
 * For cases where the user-mode side wishes to block until the ringbuffer 
 * is sufficiently full (RX) or empty (TX), this driver has a poll() entry
 * point which user-mode can use to block until the ringbuffer reaches the 
 * desired fillpoint.  if the user always used poll() to decide when to 
 * read/write more data, then the user-mapped ringbuffer would not be
 * much of a win (since user-mode would do about as many systems calls
 * to accomplish the same task as traditional read()/write()).  However,
 * typical real applications doing MIDI or video deck control are almost
 * always doing something else as well (ie, audio or video!) and they
 * often poll the device (in the sense of checking the 
 * buffer's "nfilled" count, which can be done with no system calls),
 * using the other operations they are doing (audio/video) to regulate
 * their usage of the serial ringbuffer.  so the mapped ringbuffer is
 * indeed a win.
 *
 * When it wants to wake up a poll()ing process, the 1ms service routine cannot
 * call pollwakeup(), because it is at splprof.  therefore, the interrupt
 * has to schedule a timepoke (an immediate timeout) at a lower spl
 * to actually call pollwakeup(). 
 *
 * this driver currently does support fan-out of RX data to multiple
 * user-mode consumers, but it does not support merging of TX data from 
 * multiple user-mode producers (note: both of these things are needed in
 * order for this driver to fully support MIDI).  so for now, for a given
 * serial device which tsio owns, there are 0 or more urbs for the RX 
 * direction on that device, and there are 0 or 1 urbs for the TX direction
 * on that device.
 */

/* toplevel states:
 *
 * This driver is designed to be used via a library, not directly.
 * Since the library is the only real direct user of this device's
 * entry points, we need only support a simple usage of the entry
 * points, and protect against other usages that could be damaging.
 *
 * For this reason, the driver restricts the possible use of its entry
 * points to a simple, linear set of states.  The driver will only
 * allow a given fd to advance to the next state; any attempt to
 * do otherwise results in a failed system call. 
 *
 * First the library open()s the driver.  The hwgraph entry passed in
 * contains a struct which contains the the desired serialio port
 * structure.  The driver is a clone-open driver so it clones the
 * hwgraph entry, finds a free urb struct, and puts a pointer to the urb
 * struct into the cloned hwgraph entry..
 *
 * then the library performs a TSIO_ACQUIRE_RB ioctl(), specifying the 
 * remaining communications parameters, queue size, and a direction.
 *
 * then the library mmap()s the ringbuffer region.  this action actually
 * starts data flowing.
 *
 * then the library unmap()s the ringbuffer region.  this terminates
 * data flow.
 *
 * then the library close()s the file descriptor.
 *
 * we test for all possible orderings of system calls other than this.
 *
 * the toplevel_state field of the urb tells what state the urb is currently
 * in.  this field is intended to be read only by top-level routines, not the
 * interrupt handler or timepoke routines.
 */
#define TSIO_BETWEEN_OPEN_AND_ACQUIRE 0
#define TSIO_BETWEEN_ACQUIRE_AND_MAP 1
#define TSIO_BETWEEN_MAP_AND_UNMAP 2
#define TSIO_BETWEEN_UNMAP_AND_CLOSE 3

/* coordination of data structures
 *
 * 
 * == here are the pieces of code which need to be coordinated:
 * 
 * - splprof millisecond interrupt (no context)
 * - spl0 timepoke (no context)
 * - open
 * - acquire ioctl
 * - map
 * - other ioctls (only allowed in TSIO_BETWEEN_MAP_AND_UNMAP toplevel state)
 * - unmap
 * - close
 * 
 * == here are the data structures, all of which are initially zero:
 * 
 * there is one sioport for each physical serial jack on the machine.
 * our driver gets at sioports via the hwgraph and claims
 * ownership of the port by setting sioport->sio_callup to our callup
 * vectors (while holding the port lock).
 * 
 * each sioport which our driver owns also has one tsio_upper structure,
 * pointed to by sioport->sio_upper.  those tsio_upper structures are
 * allocated out of porttab[].  for reasons explained below, each
 * tsio_upper structure also contains a pointer back to its corresponding
 * sioport.
 * 
 * then there is the urb (user ring buffer) data structure.  there is one
 * urb for each user-mode open() of our driver.  each open() represents a
 * connection to one physical port in one direction (TX or RX).
 * 
 * therefore, there is a many-to-one relationship between
 * sioport+tsio_upper's and urbs.  each urb contains a pointer to the
 * tsio_upper structure for its port (which itself contains a pointer to
 * the sioport).
 * 
 * the fields relevant to coordination are:
 * 
 * - the table of urbs: urbtab[]
 *     - urbtab[i].allocated - whether or not this entry in urbtab is taken
 *     - urbtab[i].intrflags - whether or not the interrupt/timepoke routine 
 *                             should try to transfer data on/wakeup this urb
 *                             (actually a bitmask; see below)
 * 
 * - int n_urb_active - count of urbs with active field set
 * 
 * - the table of tsio_upper structures: porttab[]
 *     - porttab[i].allocated - whether or not this entry in porttab is taken
 *     - porttab[i].intrflags - whether or not the splprof interrupt routine
 *                              should try to transfer data on this port
 *                              (actually a bitmask; see below)
 *     - porttab[i].n_urb_allocated - number of urbs pointing here
 *     - porttab[i].n_urb_active - number of activeurbs pointing here
 * 
 * - sio's sioport data structures
 *     - sio_callup, which indicates whether tsio owns the port
 *     - sio_upper, containing a pointer to our tsio_upper data structure
 *     - sioport has its own spinlock (see below)
 * 
 * an 'active' urb is a urb with urbtab[i].intrflags&URB_ACTIVE set
 * an 'active' port is a port with porttab[i].intrflags&PORT_ACTIVE set
 * 
 * == protection for driver entry points:
 * 
 * all driver entry points are single-threaded by the use of a global
 * sleeping lock, LOCK_TSIO() and UNLOCK_TSIO().
 * 
 * == protection for urbs between toplevel and non-toplevel routines
 * 
 * the field urbtab[i].intrflags is used to coordinate the toplevel
 * routines (driver entry points) and non-toplevel routines (interrupt
 * and timepoke).  when a urb is first activated, the toplevel routines
 * can set up the urb, then set the urbtab[i].intrflags field to
 * URB_ACTIVE.  this is pretty simple and has no coordination problems.
 * however, when it's time to deactivate a urb, we must be more careful:
 * 
 * a. the toplevel routines want to clear the URB_ACTIVE bit of
 * urbtab[i].intrflags so it's safe to deallocate all of the urb's
 * resources.  they have to know that none of the non-toplevel routines
 * are currently using the urb while they clear the bit.  they are
 * willing to spin a little to guarantee this.
 * 
 * b. the non-toplevel routines want to access urbtab[i], and they use
 * urbtab[i].intrflags to decide this.  if they see that the URB_ACTIVE
 * bit is set, then they want to execute a little bit of code using that
 * urb, with the guarantee that urbtab[i].intrflags&URB_ACTIVE will not
 * change while they are using the urb.  these routines do not want to
 * spin--they are holding off interrupts as they execute.
 * 
 * There is no need for the non-toplevel routines to spin waiting for a
 * lock.  If the toplevel routines are currently deactivting the urb, then
 * the non-toplevel routines simply want to give up and move on to the
 * next urb without waiting.  
 * 
 * this non-spinlock solution is accomplished with the SGI atomic
 * operation compare_and_swap_int, which performs this test and set
 * atomically to all other users of the passed-in memory location:
 * 
 * int
 * compare_and_swap_int(int *loc, int compare_value, int assign_value)
 * {
 *   if ( (*loc) == compare_value )
 *     {
 *       *loc = assign_value;
 *       return 1;
 *     }
 *   return 0;
 * }
 * 
 * here is how it is used:
 * 
 * - urbtab[i].intrflags is initially zero (inactive).
 * - map() calls activate_urb, which sets urbtab[i].intrflags to URB_ACTIVE
 * - the intr routine uses this technique to safely access a urb:
 * 
 *   while (urbtab[i].intrflags & URB_ACTIVE)
 *     {
 *       if (compare_and_swap_int(&urbtab[i].intrflags,
 *                                (urbtab[i].intrflags & URB_INUSE_BY_TIMEPOKE)
 *                                  | URB_ACTIVE,
 *                                (urbtab[i].intrflags & URB_INUSE_BY_TIMEPOKE)
 *                                  | URB_ACTIVE | URB_INUSE_BY_INTR))
 *         {
 *           {the ACTIVE bit was set, and now we have set our INUSE bit}
 *           {it's safe to use the urb until we clear our INUSE bit}
 *           ...
 *           {clear our INUSE bit}
 *           atomicClearInt(&urbtab[i].intrflags, URB_INUSE_BY_INTR);
 *           break;
 *         }
 *     }
 * 
 * - the timepoke routine uses the exact same code, with the words
 *   INTR and TIMEPOKE flipped around.
 * 
 * - unmap() calls deactivate_urb(), which uses this technique to safely
 *   deactivate a urb:
 * 
 *   while (!compare_and_swap_int(&urbtab[i].intrflags, URB_ACTIVE, 0))
 *     ; {spin}
 * 
 *   after this code segment, it is safe to deallocate the urb's resources;
 *   the interrupt is not touching the urb and will not touch it again.
 * 
 * - how does this work?  each non-toplevel routine wants to see if the ACTIVE
 *   bit is set.  if so, it atomically sets its INUSE bit, uses the urb, and  
 *   then clears its INUSE bit.
 * 
 *   the toplevel routine wants to clear the ACTIVE bit.  it waits around
 *   until the ACTIVE bit is the only bit set (meaning the INUSE bits are all
 *   zero), and when that happens it atomically clears the ACTIVE bit 
 *   (by setting the whole word to 0).
 * 
 *   there is one subtlety: there are two inuse bits, one for the intr and  
 *   one for the timepoke.  each non-toplevel routine has to check for both 
 *   ACTIVE and ACTIVE|URB_INUSE_BY_{the other non-toplevel routine}.  
 *   the code which accomplishes this above does so by peeking at the value 
 *   of the other non-toplevel routine's INUSE bit, and building a 
 *   compare_value which is most likely to match in a compare_and_swap_int 
 *   operation.  it is extremely unlikely but possible that intrflags 
 *   will change between the time that the non-toplevel routine peeks at it 
 *   and the time that compare_and_swap_int loads it as part of its ll/sc 
 *   sequence.  the while() loop correctly handles all such cases.
 * 
 * == protection for n_urb_active and n_urb_allocated between toplevel and 
 *    non-toplevel routines
 * 
 * none.  the non-toplevel routines do not use any of the fields
 * called n_urb_active or n_urb_allocated.  this includes the fields
 * inside the tsio_upper structure and the global variables.
 * 
 * == protection for porttab[] between toplevel and non-toplevel routines
 * 
 * we have exactly the same protection issue for each sioport+tsio_upper
 * as we do for each urb, except the problem is limited to the toplevel
 * routine and the splprof interrupt.  the solution we use is essentially
 * the same.
 * 
 * - porttab[i].intrflags is initially zero (inactive).
 * - map() calls activate_urb, which sets porttab[i].intrflags to PORT_ACTIVE
 * - interrupt code which wants to manipulate a port first goes to the
 *   tsio_upper structure for that port and does:
 * 
 *   if (compare_and_swap_int(&porttab[i].intrflags, 
 *                            PORT_ACTIVE, 
 *                            PORT_ACTIVE|PORT_INUSE_BY_INTR))
 *     {
 *       {the ACTIVE bit was set, and now we have set our INUSE bit also}
 *       {it's safe to use the port until we clear our INUSE bit}
 *       sioport *port = porttab[i].sioport;
 *       ...
 *       {clear our INUSE bit}
 *       atomicClearInt(&porttab[i].intrflags, PORT_INUSE_BY_INTR);
 *     }
 * 
 *   the code is simpler than the urb protection case because there's only
 *   one INUSE bit.
 * 
 * - unmap() calls deactivate_urb(), which does this if the port is
 *   closing down:
 * 
 *   while (!compare_and_swap_int(&porttab[i].intrflags, PORT_ACTIVE, 0))
 *     ; {spin}
 * 
 *   after this code segment, it is safe to deallocate the port's resources;
 *   the interrupt is not touching the port and will not touch it again.
 * 
 * == protection for sioports between toplevel and non-toplevel routines
 * 
 * sio has its own spinlock built into each sioport * which is supposed
 * to protect the members of that sioport.  this spinlock is manipulated
 * by SIO_LOCK_PORT() and SIO_UNLOCK_PORT(). this lock is used to protect access
 * to a given sioport not only within the upper and lower layer driver
 * that owns the port, but also other drivers that are checking if they
 * can grab the port.
 * 
 * we use SIO_LOCK_PORT() in the intended manner from our toplevel routines
 * when we attempt to seize control of a port in the sio system.
 * 
 * once we have seized control (by setting sio_callup with the port lock
 * held), the most any other driver should ever do when it has the
 * SIO_LOCK_PORT() lock held is check sio_callup, see that we own the port,
 * and give up.
 * 
 */

#define URB_ACTIVE            0x01
#define URB_INUSE_BY_INTR     0x10
#define URB_INUSE_BY_TIMEPOKE 0x20

#define PORT_ACTIVE            0x01
#define PORT_INUSE_BY_INTR     0x10

#ifdef MOUNTAINGATE
/* Mountaingate stuff - see below */
typedef void (*callback_t)(int minor, stamp_t now); /* Mountaingate */
sioport_t *mg_current_sioport = NULL; /* Mountaingate */
/* End Mountaingate stuff */
#endif /* MOUNTAINGATE */

/*
  tsio upper layer data that hangs off each sioport.
  each sioport represents one physical port (TX and RX).
*/
typedef struct tsio_upper 
{ 
  int allocated;                /* this entry of porttab is used */
  int intrflags;                /* whether intr should look at this port */
  sioport_t *port;              /* sio data structure for this port */
  int n_urb_allocated;          /* number of urbs pointing here */
  int n_urb_active;             /* number of active urbs pointing here */
  tsio_comm_params_t commparams; /* baud, stop, parity, ... */
#ifdef MOUNTAINGATE
  callback_t mg_callback;       /* Mountaingate callback - see below */
#endif
} tsio_upper;

/*
   user ring buffer.  represents one serial port open in one direction.
*/
typedef struct urb
{
  int allocated;                /* this entry of urbtab is used */
  int intrflags;                /* whether intr/poke should look at this rb */
  int toplevel_state;           /* which entry point: TSIO_BETWEEN_* */
  int direction;                /* TSIO_TOMIPS, TSIO_FROMMIPS */
  
  int portidx;                  /* index of port in porttab */

  urbheader_t *header;          /* pointer to user-mapped header */
  unsigned char *data;          /* pointer to user-mapped rb data */
  stamp_t *stamps;              /* pointer to user-mapped rb timestamps */

  int maplen;      /* total bytes malloced for a region which contains:
                      1. sizeof(urbheader_t) bytes of header
                      2. the user's entire data ring buffer + 1 guard byte
                      3. any padding needed here
                      4. the user's enture stamp ring buffer + 1 guard stamp
                      5. padding out to a page size
                      this is what is mapped into user's address space
                    */

  int nlocs;        /* Same value as in tsio_acquireurb_t */
  int TXheadRXtail; /* our tail for TOMIPS, our head for FROMMIPS transfers */

  int itail;        /* used internally by splprof interrupt */
  int ihead;        /* used internally by splprof interrupt */

  struct pollhead pq; /* poll queue (for select, poll) */
  int wakeup; /* indication from splprof intr to spl0 wake_rbs timeout */
} urb;

#ifdef TSERIALIO_SCALABLE
#define N_URB 32     /* max # of user rbs on all ports */
#else
#define N_URB 16     /* max # of user rbs on all ports */
#endif

urb *urbtab;         /* table of user rbs for all ports */
int n_urb_active;    /* number of urb slots in urbtab which are ready */
                     /* to be accessed in interrupt handler. */

tsio_upper *porttab; /* table of ports which tsio is managing */


mutex_t tsio_mutex; /* global mutex that makes entry points single-threaded */
#define INIT_TSIO_LOCK()    mutex_init(&tsio_mutex, MUTEX_DEFAULT, "tsio")
#define DESTROY_TSIO_LOCK() mutex_destroy(&tsio_mutex)
#define LOCK_TSIO()         mutex_lock(&tsio_mutex, 0)
#define UNLOCK_TSIO()       mutex_unlock(&tsio_mutex)

#ifdef MOUNTAINGATE
/* Mountaingate functions ---------------------------------------------- */

/* called when a portidx struct is being allocated */
static void mg_clearout_port(int portidx)
{
  tsio_upper *upper = &porttab[portidx];
  upper->mg_callback = NULL;
}

/* see Mountaingate proposal.  these functions assume tons of
 * stuff, like that they're running at <=38400 baud on an O2
 * with certain OS software with a special driver that calls
 * these after their user-mode code sets up a TSport, etc.
 */
void tsio_register_callback(int minor, callback_t funcptr)
{
  sioport_t *port;
  tsio_upper *upper;

  /* function skips TSIO_ locks due to assumptions */

  port = sio_getport(minor);

  if (NULL == port)
    return;

  SIO_LOCK_PORT(port);

  if (NULL == port->sio_upper)
    {
      SIO_UNLOCK_PORT(port);
      return;
    }

  if (port->sio_callup != &tsio_callup)
    {
      SIO_UNLOCK_PORT(port);
      return;
    }

  upper = (tsio_upper *)(port->sio_upper);

  upper->mg_callback = funcptr;

  SIO_UNLOCK_PORT(port);
}

static void mg_handle_port(int portidx, stamp_t now)
{
  tsio_upper *upper = &porttab[portidx];
  sioport_t *port = upper->port;

  mg_current_sioport = port;

  /* this handler will call tsio_uart_{read,write} */
  (*upper->mg_callback)(upper->port_minor_number, now);

  mg_current_sioport = NULL;
}

int tsio_uart_read(unsigned char *buf, int atmost)
{
  int nrxbytes = 0;
  int nread;
  ASSERT(issplprof(getsr()));
  ASSERT(mg_current_sioport);
  ASSERT(!sio_port_islocked(mg_current_sioport));
  SIO_LOCK_PORT(mg_current_sioport);
  ASSERT(sio_port_islocked(mg_current_sioport));
  while (nrxbytes < atmost &&
         (nread = DOWN_READ(mg_current_sioport,
                            (char *)(buf + nrxbytes),
                            atmost - nrxbytes)) > 0)
    nrxbytes += nread;
  SIO_UNLOCK_PORT(mg_current_sioport);
  return nrxbytes;
}

int tsio_uart_write(unsigned char *buf, int atmost)
{
  int rc;
  ASSERT(issplprof(getsr()));
  ASSERT(mg_current_sioport);
  ASSERT(!sio_port_islocked(mg_current_sioport));
  SIO_LOCK_PORT(mg_current_sioport);
  ASSERT(sio_port_islocked(mg_current_sioport));
  rc = DOWN_WRITE(mg_current_sioport, (char *)buf, atmost);
  SIO_UNLOCK_PORT(mg_current_sioport);
  return rc;
}
/* End Mountaingate functions ------------------------------------------ */
#endif /* MOUNTAINGATE */

/* 1 millisecond interrupt function ------------------------------------ */

stamp_t get_current_ust(void)
{
  int was_splprof = issplprof(getsr());
  int s;
  stamp_t ust;

  if (!was_splprof)
    s = splprof();
  
  update_ust();
  get_ust_nano((unsigned long long *)(&ust));
  
  if (!was_splprof)
    splx(s);

  return ust;
}


#ifdef TSIO_DEBUG
#define CHECK
#else
#undef CHECK
#endif

#ifdef CHECK
#define TIMEPOKE_ENTERED 0xb00f
int tsio_timepoke_mutex = 0;
#endif

void
tsio_timepoke_to_wake_rbs()
{
    int	urbidx;
    int	more_to_do;
    int s;

#ifdef CHECK
    if (tsio_timepoke_mutex != 0) {
	printf("note: tsio_timepoke_to_wake_rbs re-entered\n");
	return;
    }
    tsio_timepoke_mutex = TIMEPOKE_ENTERED;
#endif

    more_to_do = 1;
    while (more_to_do) {
	for (urbidx = 0; urbidx < N_URB; urbidx++) {
	    urb *urb = &urbtab[urbidx];

	    /* PEEK: see comment in see_if_intr_should_use_urb() */
	    if (!urb->wakeup)
		continue;

	    /* try and access this urb.  see comments at the top of this file
             * for explanation.
             */
	    while (urb->intrflags & URB_ACTIVE) {
		int	other = (urb->intrflags & URB_INUSE_BY_INTR);
		ASSERT(!(urb->intrflags & URB_INUSE_BY_TIMEPOKE));
		if (compare_and_swap_int(&urb->intrflags,
		    other | URB_ACTIVE,
		    other | URB_ACTIVE | URB_INUSE_BY_TIMEPOKE)) {
		    /* the ACTIVE bit was set, and now we have set our INUSE bit */
		    /* it's safe to use the urb until we clear our INUSE bit */

		    ASSERT(urb->allocated);

		    if (urb->wakeup) {
			/*
                         * all the man pages say never to wakeup on more than
                         * one event at the same time.  but then again all 6-10 
                         * drivers in irix/kern which have xxxxpoll() actually 
                         * do this, so...
                         */
			urb->wakeup = 0;
			pollwakeup(&urb->pq, POLLIN | POLLOUT | POLLRDNORM);
		    }

		    /* clear our INUSE bit */
		    atomicClearInt(&urb->intrflags, URB_INUSE_BY_TIMEPOKE);
		    break;
		}
	    }
	}
	while (1) {
	    if (wake_rbs_state == WAKE_RBS_IDLE) {
		/* We should never be idle in tsio_timepoke_to_wake_rbs since
	           it is always entered non-idle and it's only made non-idle
	           just before exiting.  If the following cmn_err is executed,
	           the locking has become very screwed up. */
		cmn_err(CE_WARN,
		    "wake_rbs_state is IDLE in tsio_timepoke_to_wake_rbs\n");
		more_to_do = 0;
		break;
	    }
	    s = compare_and_swap_int(&wake_rbs_state, WAKE_RBS_SCANNING, WAKE_RBS_IDLE);
	    if (s) {
		more_to_do = 0;
		break;
	    }
	    s = compare_and_swap_int(&wake_rbs_state, WAKE_RBS_RESTART, WAKE_RBS_SCANNING);
	    if (s) {
		more_to_do = 1;
		break;
	    }
	}
    }
#ifdef CHECK
    tsio_timepoke_mutex = 0;
#endif
}


void intr_is_done_using_urb(urb *urb);
/*
 * splprof intr calls this to try and gain access to a urb.
 * if we return 0, the urb was uninteresting and the intr routine should
 *                 not access it (and should not call intr_is_done_using_urb)
 * if we return 1, the urb was interesting and has been marked inuse.
 *                 the intr routine should access it, then call
 *                 intr_is_done_using_urb() to indicate that it's done.
 */
int see_if_intr_should_use_urb(urb *urb, int portidx, int direction)
{
  /* PEEK: make sure it's worth even trying to set our INUSE bit.
   * this test is just an optimization to avoid doing the 
   * compare_and_swap_int if we don't have to.  the values we peek at 
   * can change if this urb is just now being set up or torn down, 
   * so we'll have to test them again later, but this test will never 
   * cause us to skip a urb that we should not have skipped.
   */
  if (urb->portidx != portidx ||
      urb->direction != direction)
    return 0;

  /* try and access this urb.  see comments at the top of this file
   * for explanation.
   */
  while (urb->intrflags & URB_ACTIVE)
    {
      int other = (urb->intrflags & URB_INUSE_BY_TIMEPOKE);
      ASSERT(!(urb->intrflags & URB_INUSE_BY_INTR));
      if (compare_and_swap_int(&urb->intrflags,
                               other | URB_ACTIVE,
                               other | URB_ACTIVE | URB_INUSE_BY_INTR))
        {
          /* the ACTIVE bit was set, and now we have set our INUSE bit */
          /* it's safe to use the urb until we clear our INUSE bit */

          ASSERT(urb->allocated);
          
          if (urb->portidx != portidx ||
              urb->direction != direction)
            {
              intr_is_done_using_urb(urb);
              return 0;
            }
          
          return 1;
        }
    }
  return 0;
}
int intr_currently_using_urb(urb *urb)
{
  return urb->intrflags & URB_INUSE_BY_INTR;
}
void intr_is_done_using_urb(urb *urb)
{
  /* clear our INUSE bit */
  atomicClearInt(&urb->intrflags, URB_INUSE_BY_INTR);
}

/* bytes per millisecond that we handle in this routine.  128 means we
 * can't handle more than 128,000 bytes a second, which is pretty 
 * reasonable (that's 1,152,000baud at 9 symbols/byte).  this is used
 * for input because we need to know how big of an array to make,
 * and it's used for input and output in order to bound the amount of
 * CPU time this routine can eat up.
 */
#define MAX_BYTES_PER_TICK 128

void tsio_handle_port(int portidx, stamp_t now)
{
  tsio_upper *upper = &porttab[portidx];
  sioport_t *port = upper->port;
  int urbidx, i;

  int n_tx_urbs = 0;
  int last_tx_urb = -1; /* optimization for non-MIDI case */

  int n_rx_urbs = 0;
  unsigned char rxbytes[MAX_BYTES_PER_TICK];
  int nrxbytes;

  ASSERT(port);
  
  /* ==== iterate through TX/TSIO_FROMMIPS/output urb's on this port */
  /*      acquire access to as many urbs as we can                   */
  /*      get head/tail snapshots for those urbs                     */

  for (urbidx=0; urbidx < N_URB; urbidx++)
    {
      urb *urb = &urbtab[urbidx];

      if (see_if_intr_should_use_urb(urb, portidx, TSIO_FROMMIPS) == 0)
        continue;

      n_tx_urbs++;
      last_tx_urb = urbidx;

      /* get our (protected copy of) head */
      
      urb->ihead = urb->TXheadRXtail;
      
      /* get an uncorrupted snapshot of user tail */
      {
        urb->itail = ((volatile urbheader_t *)urb->header)->tail;
        
        /* check if user has corrupted header tail value */
        if (urb->itail < 0 || urb->itail >= urb->nlocs) 
          {
            cmn_err(CE_WARN, "tsio intr: user rb tail is corrupted\n");
            /*
             * we forcibly set the user's tail to indicate that the
             * buffer is empty; putting tail back into a valid state
             * helps to prevent hundreds of identical cmn_err's from
             * spewing out on the console.
             *
             * XXX is this likely to be good strategy on MP?
             */
            urb->itail = urb->ihead; /* assume buf is empty */
            ((volatile urbheader_t *)urb->header)->tail = urb->itail;
          }
      }

#ifdef TSIO_DEBUG      
      /* debugging */
      if (upper->commparams.flags & TSIO_FLAGS_INTR_DEBUG)
        {
          static stamp_t then=0;
          if (now - then > 1000000000LL)
            {
              printf("%lld: head %d tail %d headstamp %lld headdata %d\n",
                     now, urb->ihead, urb->itail, 
                     urb->stamps[urb->ihead], urb->data[urb->ihead]);
              then = now;
            }
        }
#endif
    }
      
  /* ==== perform actual TX on hardware                        */
  /*      adjust each urb->ihead, limited by each urb->itail */

  if (n_tx_urbs > 0)
    {
      int n;
      urb *urb;

      ASSERT(n_tx_urbs == 1); /* WE DON'T DO MIDI NOW! */
      ASSERT(last_tx_urb >= 0 && last_tx_urb < N_URB);
      urb = &urbtab[last_tx_urb];
      
      SIO_LOCK_PORT(port);

#ifdef TSERIALIO_SCALABLE
      /* This is the scalable version of the output loop.  It writes
       * pending data to lower-layer in only two passes if current data 
       * wraps around the end of the ring buffer, or only one pass if it
       * does not.

       * The original version of the output loop preserved below writes
       * the data one byte at a time, flushing each byte to the hardware.
       * this takes too long with lots of bytes on multiple ports. */

      for (;;) {
	  int n_written;

	  for (i = urb->ihead; (i != urb->itail) && (i < urb->nlocs); i++) {
	      if (urb->stamps[i] > now) {
		  break;
	      }
	  }

	  if (i == urb->ihead) {
	      /* nothing to write */
	      break;
	  }

	  n = i - urb->ihead;
	  n_written = DOWN_WRITE(port,(char*)urb->data+urb->ihead,n);

#ifdef TSIO_DEBUG
	  if (upper->commparams.flags & TSIO_FLAGS_TX_DEBUG)
	  {
	      char* c = (char*)urb->data+urb->ihead;
	      SIO_UNLOCK_PORT(port);
	      for (i = 0; i < n_written; i++) {
		  printf("[%d]", c[i]); /* debugging */
	      }
	      SIO_LOCK_PORT(port);
	  }
#endif

	  urb->ihead += n_written;

	  if (urb->ihead == urb->nlocs) {
	      /* ringbuf wrap */
	      urb->ihead = 0;
	  } else {
	      /* no wraparound, so no second time through loop */
	      break;
	  }
	  if (n_written < n) {
	      /* device fifo full */
	      break;
	  }
      }
#else
      /* transfer any and all appropriate urb data */
      n = 0;
      while (n < MAX_BYTES_PER_TICK &&
             urb->ihead != urb->itail &&
             urb->stamps[urb->ihead] <= now)
        {
          unsigned char c = urb->data[urb->ihead];
          
          if (DOWN_WRITE(port, (char *)&c, 1) != 1)
            break; /* hardware FIFO is full */
          
#ifdef TSIO_DEBUG
          if (upper->commparams.flags & TSIO_FLAGS_TX_DEBUG)
            {
              SIO_UNLOCK_PORT(port);
              printf("[%d]", c); /* debugging */
              SIO_LOCK_PORT(port);
            }
#endif
              
          urb->ihead++;
          if (urb->ihead >= urb->nlocs) urb->ihead -= urb->nlocs;
          n++;
        }
#endif

      SIO_UNLOCK_PORT(port);
    }
      
  /* ==== iterate through TX/TSIO_FROMMIPS/output urb's on this port */
  /*      update heads, deal with wakeups                            */
  /*      release urbs since we are now done with them               */

  for (urbidx=0; urbidx < N_URB; urbidx++)
    {
      urb *urb = &urbtab[urbidx];

      if (!intr_currently_using_urb(urb))
        continue;

      if (urb->ihead != urb->TXheadRXtail) /* did head change? */
        {
          /* update head of user's rb.  This is stored in TWO places: */
          urb->TXheadRXtail = urb->ihead; /* FROMMIPS: head */
          *((volatile int *)&(((urbheader_t *)urb->header)->head)) = 
            urb->ihead;
        }

#ifdef TSIO_DEBUG
      /* debugging */
      if (upper->commparams.flags & TSIO_FLAGS_INTR_DEBUG)
        {
          static stamp_t then=0;
          
          if (now - then > 1000000000LL)
            {
              int n_filled = urb->itail - urb->ihead;
              if (n_filled < 0) n_filled += urb->nlocs;
              printf("%lld: intreq %d, fillpt %lld, filled %d\n",
                     now,
                     ((volatile urbheader_t *)urb->header)->intreq,
                     ((volatile urbheader_t *)urb->header)->fillpt,
                     n_filled);
              then = now;
            }
        }
#endif
      
      /* wakeup user mode app if requested and nfilled < fillpt */
      if (((volatile urbheader_t *)urb->header)->intreq)
        {
          int fillunits = ((volatile urbheader_t *)urb->header)->fillunits;
          
          /* compute # of bytes filled */
          
          stamp_t n_filled = urb->itail - urb->ihead;
          if (n_filled < 0) n_filled += urb->nlocs;
          
          if (fillunits == TSIO_FILLUNITS_UST) /* make it UST instead */
            {
              if (n_filled != 0) /* if there are some UST stamps on q */
                n_filled = urb->stamps[urb->itail] - now;
              /* else n_filled = 0 UST also! */
            }
          
          if (n_filled < ((volatile urbheader_t *)urb->header)->fillpt)
            {
              ((volatile urbheader_t *)urb->header)->intreq = 0;
              /* we will perform actual pollwakeup() from lower spl */
              urb->wakeup = 1;
	      wake_rbs_doorbell();
            }
        }
      
      intr_is_done_using_urb(urb);
    }

  /* ==== iterate through RX/TSIO_TOMIPS/input urb's on this port    */
  /*      acquire access to as many urbs as we can                   */
  /*      get head/tail snapshots for those urbs                     */
  
  for (urbidx=0; urbidx < N_URB; urbidx++)
    {
      urb *urb = &urbtab[urbidx];

      if (see_if_intr_should_use_urb(urb, portidx, TSIO_TOMIPS) == 0)
        continue;

      n_rx_urbs++;

      /* get our (protected copy of) tail */
      
      urb->itail = urb->TXheadRXtail;
      
      /* get an uncorrupted snapshot of user head */
      {
        urb->ihead = ((volatile urbheader_t *)urb->header)->head;
        
        /* check if user has corrupted header head value */
        if (urb->ihead < 0 || urb->ihead >= urb->nlocs) 
          {
            cmn_err(CE_WARN, "tsio intr: user rb head is corrupted\n");
            /*
             * we forcibly set the user's head to indicate that the
             * buffer is full; putting head back into a valid state
             * helps to prevent hundreds of identical cmn_err's from
             * spewing out on the console.
             *
             * XXX is this likely to be good strategy on MP?
             */
            urb->ihead = urb->itail+1; /* assume buf is full */
            if (urb->ihead >= urb->nlocs) urb->ihead -= urb->nlocs; /* wrap */
            ((volatile urbheader_t *)urb->header)->head = urb->ihead;
          }
      }

#ifdef TSIO_DEBUG      
      /* debugging */
      if (upper->commparams.flags & TSIO_FLAGS_INTR_DEBUG)
        {
          static stamp_t then=0;
          if (now - then > 1000000000LL)
            {
              printf("%lld: head %d tail %d tailstamp %lld taildata %d\n",
                     now, urb->ihead, urb->itail, 
                     urb->stamps[urb->itail], urb->data[urb->itail]);
              then = now;
            }
        }
#endif
    }

  /* ==== perform actual RX on hardware */

  if (n_rx_urbs > 0)
    {
      int nread;
      
      /* get actual input data -> rxbytes */
      
      /* XXX must handle line and hardware errors--
         annoyingly, this involves callback in current scheme */
      
      SIO_LOCK_PORT(port);

      nrxbytes = 0;
      while (nrxbytes < MAX_BYTES_PER_TICK &&
             (nread = DOWN_READ(port,
                                (char *)(rxbytes + nrxbytes),
                                MAX_BYTES_PER_TICK - nrxbytes)) > 0)
        nrxbytes += nread;

      SIO_UNLOCK_PORT(port);
    }

  /* ==== iterate through RX/TSIO_TOMIPS/input urb's on this port    */
  /*      update tails, deal with wakeups                            */
  /*      release urbs                                               */

  for (urbidx=0; urbidx < N_URB; urbidx++)
    {
      urb *urb = &urbtab[urbidx];

      if (!intr_currently_using_urb(urb))
        continue;

      /* transfer all available data into urb */
      for(i=0; i < nrxbytes; i++)
        {
          int n_fillable = urb->ihead - urb->itail - 1;
          if (n_fillable < 0) n_fillable += urb->nlocs; /* wrap */
          
          if (n_fillable == 0) /* user buffer overflow; too bad, u lose */
            break;
          
          urb->data[urb->itail] = rxbytes[i];
          urb->stamps[urb->itail] = now;
          
          urb->itail++;
          if (urb->itail >= urb->nlocs) urb->itail -= urb->nlocs;
        }
      
      if (urb->itail != urb->TXheadRXtail) /* did tail change? */
        {
          /* update tail of user's rb.  This is stored in TWO places: */
          urb->TXheadRXtail = urb->itail; /* FROMMIPS: tail */
          *((volatile int *)&(((urbheader_t *)urb->header)->tail)) = 
            urb->itail;
        }

#if 0      
      /* debugging */
      if (upper->commparams.flags & TSIO_FLAGS_INTR_DEBUG)
        {
          static stamp_t then=0;
          
          if (now - then > 1000000000LL)
            {
              int n_filled = urb->itail - urb->ihead;
              if (n_filled < 0) n_filled += urb->nlocs;
              printf("%lld: intreq %d, fillpt %lld, filled %d\n",
                     now,
                     ((volatile urbheader_t *)urb->header)->intreq,
                     ((volatile urbheader_t *)urb->header)->fillpt,
                     n_filled);
              then = now;
            }
        }
#endif
      
      /* wakeup user mode app if requested and nfilled >= fillpt */
      if (((volatile urbheader_t *)urb->header)->intreq)
        {
          int fillunits = ((volatile urbheader_t *)urb->header)->fillunits;
          
          /* compute # of bytes filled */
          
          stamp_t n_filled = urb->itail - urb->ihead;
          if (n_filled < 0) n_filled += urb->nlocs;
          
          if (fillunits == TSIO_FILLUNITS_UST) /* make it UST instead */
            {
              if (n_filled != 0) /* if there are some UST stamps on q */
                n_filled = now - urb->stamps[urb->ihead];
              /* else n_filled = 0 UST also! */
            }
          
          if (n_filled >= ((volatile urbheader_t *)urb->header)->fillpt)
            {
              ((volatile urbheader_t *)urb->header)->intreq = 0;
              /* we will perform actual pollwakeup() from lower spl */
              urb->wakeup = 1;
	      wake_rbs_doorbell();
            }
        }
      
      intr_is_done_using_urb(urb);
    }
}

/* ARGSUSED */
void
tsio_tick(void *arg)
{
  stamp_t now = get_current_ust();
  int portidx;

  for(portidx=0;  portidx < N_URB;  portidx++)
    {
      /* try and access this port.  see comments at the top of this file
       * for explanation.
       */
      if (compare_and_swap_int(&porttab[portidx].intrflags, 
                               PORT_ACTIVE, 
                               PORT_ACTIVE|PORT_INUSE_BY_INTR))
        {
#ifdef MOUNTAINGATE
          if (porttab[portidx].mg_callback) /* Mountaingate */
            mg_handle_port(portidx, now); /* Mountaingate */
          else
            tsio_handle_port(portidx, now);
#else
            tsio_handle_port(portidx, now);
#endif

          /* now release this port */
          atomicClearInt(&porttab[portidx].intrflags, PORT_INUSE_BY_INTR);
        }
    }
}


void
start_tsio_ticks(void)
{

#if defined(TSERIALIO_TIMEOUT_IS_THREADED)
        start_tsio_timeout_thread();
#else
/* Fix for pv bug  460909. */
#if defined(MP)
    extern void startaudioclock(void);

    startaudioclock();
#else
   if (!fastclock) {
       enable_fastclock();
   }
#endif
/* End fix for pv bug  460909. */
  tsio_timercallback_ptr = (void (*)(void))tsio_tick;
  atomicSetUint(&fastick_callback_required_flags,
                FASTICK_CALLBACK_REQUIRED_TSERIALIO_MASK);
#endif

}


void
stop_tsio_ticks(void)
{
#if defined(TSERIALIO_TIMEOUT_IS_THREADED)
    xthread_periodic_timeout_stop(&tsio_thread_info);
#else
  atomicClearUint(&fastick_callback_required_flags,
                  FASTICK_CALLBACK_REQUIRED_TSERIALIO_MASK);
  tsio_timercallback_ptr = NULL;
#endif
  /* XXX need to actually stop the clock under some circumstances - see
         the kdsp driver for details */
}

/* urb activate/deactivate ----------------------------------------------- */

static int tsio_getbytesize(tcflag_t cflag)
{
  int byte_size = 8;

  /* set number of data bits */
  switch (cflag & CSIZE) 
    {
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

  return byte_size;
}

int
activate_urb(urb *urb)
{
  sioport_t *port;
  tsio_upper *upper;

  upper = &porttab[urb->portidx];
  ASSERT(urb->portidx >= 0 && urb->portidx < N_URB);
  port = upper->port;
  ASSERT(port);

  /* see if we're the first urb on this port */

  if (upper->n_urb_active == 0)
    {
      /* yup -- initialize port */

      SIO_LOCK_PORT(port);
  
      if (DOWN_OPEN(port)) 
        { SIO_UNLOCK_PORT(port); return ENODEV; }
      
      if (DOWN_ENABLE_HFC(port, 0)) 
        { SIO_UNLOCK_PORT(port); return ENODEV; }
      
#ifdef TSERIALIO_NOTIFY_DATA_READY
      if (DOWN_NOTIFICATION(port, N_ALL & ~N_DATA_READY , 0)) 
        { SIO_UNLOCK_PORT(port); return ENODEV; }

      if (DOWN_NOTIFICATION(port, N_DATA_READY, 1)) 
        { SIO_UNLOCK_PORT(port); return ENODEV; }
#else
      if (DOWN_NOTIFICATION(port, N_ALL, 0)) 
        { SIO_UNLOCK_PORT(port); return ENODEV; }
#endif
      
      /* XXX assume RX_TIMEOUT off by default: accurate? */
      /* if not, how do you set it off? does 0 mean off? */
      
      if (DOWN_SET_PROTOCOL(port, 
                            (upper->commparams.flags & TSIO_FLAGS_RS422) ?
                            PROTO_RS422 : PROTO_RS232))
        { SIO_UNLOCK_PORT(port); return ENODEV; }
        
      if (DOWN_SET_EXTCLK(port, upper->commparams.extclock_factor))
        { SIO_UNLOCK_PORT(port); return ENODEV; }
      
      if (DOWN_CONFIG(port, 
                      upper->commparams.ospeed, 
                      tsio_getbytesize(upper->commparams.cflag),
                      upper->commparams.cflag & CSTOPB,
                      upper->commparams.cflag & PARENB,
                      upper->commparams.cflag & PARODD))
        { SIO_UNLOCK_PORT(port); return ENODEV; }

      if (DOWN_SET_DTR(port, 1))
        { SIO_UNLOCK_PORT(port); return ENODEV; }
      
      if (DOWN_SET_RTS(port, 1))
        { SIO_UNLOCK_PORT(port); return ENODEV; }

      /* clear out RX bytes -- very important because intr routine
       * relies on constantly small hw FIFO sizes if UST stamps
       * are to be accurate.  ignore any errors.
       */
      {
        char buf[16];
        while(DOWN_READ(port, buf, sizeof(buf)) > 0);
      }
      /* XXX should also force drain of TX bytes in activate, 
       * same reason -- does h/w and sio let us detect this?
       * does it let us know how long to sleep for to drain h/w?
       */

      /* XXX neither drain code works if external clocking and there
       * is no clock.  must set internal clock while draining.
       */
      SIO_UNLOCK_PORT(port);

      ASSERT(upper->intrflags == 0);
      upper->intrflags = PORT_ACTIVE; /* this is what intr looks at */
    }

  ASSERT(n_urb_active > 0 || urb->intrflags == 0);
  urb->intrflags = URB_ACTIVE; /* this is what intr/timepoke look at */

  /* if this is the first user rb for any port, start interrupts */
  if (n_urb_active == 0)
    start_tsio_ticks();

  /* some bookkeeping.  neither intr nor timepoke look at these */
  n_urb_active++;
  upper->n_urb_active++;
  
  return 0;
}


void deactivate_urb(urb *urb)
{
  tsio_upper *upper;

  upper = &porttab[urb->portidx];
  ASSERT(urb->portidx >= 0 && urb->portidx < N_URB);

  /* some bookkeeping.  neither intr nor timepoke look at these */
  upper->n_urb_active--;
  n_urb_active--;

  /* if this is the last user rb for any port, stop interrupts */
  if (n_urb_active == 0)
    stop_tsio_ticks();

  /* deactivate this urb.  see comments at the top of this file
   * for explanation.
   */
  ASSERT(urb->intrflags & URB_ACTIVE);
  while (!compare_and_swap_int(&urb->intrflags, URB_ACTIVE, 0))
    ; /* spin */
  ASSERT(urb->intrflags == 0);

  /* now it is safe to deallocate the urb's resources; the interrupt/timepoke
   * will not touch the urb any more (and are not touching it now)
   */

  /* see if we were the last urb on this port */

  ASSERT(upper->intrflags & PORT_ACTIVE);
  if (upper->n_urb_active == 0)
    {
      /* XXX may want to flush TX/RX queues of hardware here to be nice
       * to next guy.
       */

      /* deactivate this port.  see comments at the top of this file
       * for explanation.
       */
      while (!compare_and_swap_int(&upper->intrflags, PORT_ACTIVE, 0))
        ; /* spin */
      ASSERT(upper->intrflags == 0);
      
      /* now it is safe to deallocate the port's resources; the interrupt
       * will not touch the port any more (and is not touching it now)
       */
    }
}

/* serialio fun --------------------------------------------------------- */

/* ARGSUSED */
void tsio_data_ready(sioport_t *port)
{ cmn_err(CE_WARN, "tsio: spurious data_ready upcall"); }
/* ARGSUSED */
void tsio_output_lowat(sioport_t *port)
{ cmn_err(CE_WARN, "tsio: spurious output_lowat upcall"); }
/* ARGSUSED */
void tsio_ncs(sioport_t *port, int ncs)
{ cmn_err(CE_WARN, "tsio: spurious ncs upcall"); }
/* ARGSUSED */
void tsio_dDCD(sioport_t *port, int dcd)
{ cmn_err(CE_WARN, "tsio: spurious dDCD upcall"); }
/* ARGSUSED */
void tsio_dCTS(sioport_t *port, int cts)
{ cmn_err(CE_WARN, "tsio: spurious dCTS upcall"); }
/* ARGSUSED */
void tsio_detach(sioport_t *port)
{ cmn_err(CE_WARN, "tsio: spurious detach upcall"); }

static struct serial_callup tsio_callup = 
{
  tsio_data_ready,
  tsio_output_lowat,
  tsio_ncs,
  tsio_dDCD,
  tsio_dCTS,
  tsio_detach
};

/* driver init ----------------------------------------------------------- */

void
tsioinit()
{
  int urbidx;
#ifdef TSIO_DEBUG
  printf("tsioinit\n");
#endif
  urbtab=0;
  porttab = (tsio_upper *)kmem_zalloc(sizeof(tsio_upper)*N_URB, KM_NOSLEEP);
  if(porttab==0){
    cmn_err(CE_WARN, "tsio: failed to initialize - can't allocate porttab");
    return;
  }
  urbtab = (urb *)kmem_zalloc(sizeof(urb)*N_URB, KM_NOSLEEP);
  if(urbtab==0){
    cmn_err(CE_WARN, "tsio: failed to initialize - can't allocate urbtab");
    kmem_free(porttab,sizeof(tsio_upper)*N_URB);
    porttab=0;
    return;
  }
  INIT_TSIO_LOCK();
  for (urbidx=0; urbidx < N_URB; urbidx++)
    initpollhead(&urbtab[urbidx].pq);
}

/* driver unload --------------------------------------------------------- */

void
tsiounload()
{
#ifdef TSIO_DEBUG
  printf("tsiounload\n");
#endif
  if(porttab){
    kmem_free(porttab,sizeof(tsio_upper)*N_URB);
    porttab=0;
  } else {
    return;
  }
  if(urbtab){
    kmem_free(urbtab,sizeof(urb)*N_URB);
    urbtab=0;
  } else {
    return;
  }
  /* XXX check for open fd's and close the various resources */
  DESTROY_TSIO_LOCK();
}

/* driver open and close ------------------------------------------------- */

#undef PRINTF_ON_MINOR_DEVICE_EVENT

/* ARGSUSED */
int
tsioopen(dev_t *devp, int oflag, int otyp, cred_t *crp) 
{
  int i;
  sioport_t *port;
  int first_free_portidx, portidx;
  int urbidx;
  tsiotype_t *tsiotype, *clone;
  vertex_hdl_t vhdl, nvhdl;

  if(urbtab==0)return ENODEV;

  LOCK_TSIO();

  vhdl = dev_to_vhdl(*devp);
  if (vhdl == GRAPH_VERTEX_NONE)
     return(ENODEV);

  if ((tsiotype = (tsiotype_t *)hwgraph_fastinfo_get(vhdl)) == NULL){
    dprintf(("no tsiotype\n"));
    return(ENODEV);
  }

  if ((port = (sioport_t *)tsiotype->port) == NULL){
    dprintf(("no port\n"));
    return(ENODEV);
  }

  /* Try to find a free urb */
  
  urbidx = -1;
  for(i=0; i < N_URB; i++) 
    {
      if (urbtab[i].allocated == 0)
        {
          urbidx = i;
          break;
        }
    }
  if (urbidx == -1) /* no free urbs */
    {
      UNLOCK_TSIO();
      return ENODEV;
    }

  /* see if this serial port is already opened by tsio via another urb */
  /* otherwise find a free entry in porttab. */

  first_free_portidx = -1;
  for(i=0; i < N_URB; i++)
    {
      if (porttab[i].allocated == 0)
        {
          if (first_free_portidx == -1) first_free_portidx = i;
        }
      else if (porttab[i].port == port)
        break;
    }
  if (i < N_URB) /* found already-opened port -- modify porttab[i] */
    {
      portidx = i;
      ASSERT(portidx >= 0 && portidx < N_URB);
      porttab[portidx].n_urb_allocated++;
      ASSERT(port->sio_callup == &tsio_callup); /* we better own the port */
      ASSERT(port->sio_upper == &porttab[portidx]); /* ditto */
    }
  else /* nope--grab port and allocate a new entry in porttab */
    {
      if (first_free_portidx < 0) /* no free entries in porttab */
        {
          UNLOCK_TSIO();
          return ENODEV;
        }
      portidx = first_free_portidx;
      ASSERT(portidx >= 0 && portidx < N_URB);

      /* 
       * PEEK: make sure this port isn't owned by another upper layer.
       * This test is an optimization to avoid acquiring the spinlock
       * if we don't have to.  if and when we lock, we'll check again.
       *
       * XXX need to look at through volatile pointer?
       */
      if (port->sio_callup != NULL)
        { 
          ASSERT(port->sio_callup != &tsio_callup);
          UNLOCK_TSIO();
          return EBUSY;
        }
      
      SIO_LOCK_PORT(port);

      /* if it's not in porttab, we shouldn't own it */
      /* it could be owned by nobody or another upper layer */
      ASSERT(port->sio_callup == NULL ||
             port->sio_callup != &tsio_callup);

      /* make sure this port isn't owned by another upper layer */
      /* XXX need to look at through volatile pointer ?? */
      if (port->sio_callup != NULL)
        { 
          SIO_UNLOCK_PORT(port);
          UNLOCK_TSIO();
          return EBUSY;
        }
      
      /* grab the port for ourselves.  this locks out other drivers */

      port->sio_callup = &tsio_callup;
      port->sio_upper = &porttab[portidx];
      sio_upgrade_lock(port, SIO_LOCK_SPL7);

      porttab[portidx].intrflags = 0; /* just in case */
      porttab[portidx].allocated = 1;
      porttab[portidx].port = port;
      porttab[portidx].n_urb_allocated = 1;
      porttab[portidx].n_urb_active = 0;
#ifdef MOUNTAINGATE
      mg_clearout_port(portidx); /* Mountaingate */
#endif

      SIO_UNLOCK_PORT(port);
    }

  /* ok, all the port stuff went well, now allocate the urb */
  
  urbtab[urbidx].intrflags = 0; /* just in case */
  urbtab[urbidx].allocated = 1;
  urbtab[urbidx].portidx = portidx;
  urbtab[urbidx].toplevel_state = TSIO_BETWEEN_OPEN_AND_ACQUIRE;

  clone = (tsiotype_t*)kmem_zalloc(sizeof(tsiotype_t), KM_NOSLEEP);
  if(clone == NULL) {
	/* XXX we should ungrab the port here. */
	return(ENOMEM);
  }
  clone->tsio_upper=tsiotype->tsio_upper;
  clone->port=tsiotype->port;
  clone->urbidx=urbidx;
  clone->portidx=portidx;
  if(hwgraph_vertex_clone(vhdl, &nvhdl) != GRAPH_SUCCESS) {
        printf(("Couldn't create clone tsio device\n"));
        kmem_free(clone, sizeof(tsiotype_t));
	/* XXX we should ungrab the port here. */
        return(ENODEV);
  }
  hwgraph_fastinfo_set(nvhdl, (arbitrary_info_t)clone);
  *devp = vhdl_to_dev(nvhdl);                /* Now we have a clone */

  UNLOCK_TSIO();

  return 0;
}

/* ARGSUSED */
int
tsioclose(dev_t dev, int flag, int otyp, cred_t *crp)
{
  urb *urb;
  sioport_t *port;
  tsio_upper *upper;
  int err;
  tsiotype_t *tsiotype;
  vertex_hdl_t vhdl;

  LOCK_TSIO();

  vhdl = dev_to_vhdl(dev);
  if (vhdl == GRAPH_VERTEX_NONE)
     return(ENODEV);

  if ((tsiotype = (tsiotype_t *)hwgraph_fastinfo_get(vhdl)) == NULL){
    dprintf(("no tsiotype\n"));
    return(ENODEV);
  }
  ASSERT(tsiotype->urbidx >= 0 && tsiotype->urbidx < N_URB);
  urb=&urbtab[tsiotype->urbidx];

#ifdef TSIO_DEBUG
#ifdef PRINTF_ON_MINOR_DEVICE_EVENT
  printf("tsioclose\n");
#endif
#endif

  upper = &porttab[urb->portidx];
  ASSERT(urb->portidx >= 0 && urb->portidx < N_URB);
  port = upper->port;
  ASSERT(port);

  /* if map happened, make sure unmap also happened */
  /* the OS seems to do this automatically but... */
  if (urb->toplevel_state == TSIO_BETWEEN_MAP_AND_UNMAP)
    {
      cmn_err(CE_WARN, "tsio close: close without unmap"); 

      deactivate_urb(urb);
      
      /* now interrupt will not touch this urb */
      
      kvpfree(urb->header,btoc(urb->maplen));
      urb->header = NULL;

      urb->toplevel_state = TSIO_BETWEEN_UNMAP_AND_CLOSE;
    }

  /* do the deed */

  upper->n_urb_allocated--;
  urb->intrflags = 0;       /* just in case */
  urb->toplevel_state = -1; /* just in case */
  urb->direction = -1;      /* just in case */
  urb->portidx = -1;        /* just in case */
  urb->wakeup = 0;          /* just in case */
  urb->allocated = 0;

  /* see if this is the last urb on this port */

  if (upper->n_urb_allocated == 0)
    {
      /* give port back to sio land */
      SIO_LOCK_PORT(port);
      port->sio_callup = NULL;
      port->sio_upper = 0;
      sio_downgrade_unlock_lock(port, SIO_LOCK_LEVEL);
      /* the sioport is now history */

      upper->intrflags = 0; /* just in case */
      upper->port = NULL; /* just in case */
      upper->allocated = 0;
    }

  device_info_set(vhdl, NULL);
  err = hwgraph_vertex_destroy(vhdl);
  if(err != GRAPH_IN_USE && err != GRAPH_SUCCESS){
    cmn_err(CE_WARN, "tsio close: BUSY"); 
    return(EBUSY);
  }

#ifdef NEVER
  if(err==GRAPH_IN_USE)
    cmn_err(CE_WARN, "tsio close: GRAPH_IN_USE"); 
  else
    cmn_err(CE_WARN, "tsio close: GRAPH_SUCCESS"); 
#endif

  kmem_free(tsiotype, sizeof(tsiotype));

  UNLOCK_TSIO();

  return(0);
}

/* driver ioctl ---------------------------------------------------------- */

/* ARGSUSED */
int
tsioioctl(dev_t dev, int cmd, void *arg, int mode, 
          struct cred *cred, int *rval)
{
  tsio_acquireurb_t tsio_acquireurb;
  urb *urb;
  tsiotype_t *tsiotype;
  vertex_hdl_t vhdl;
  
  LOCK_TSIO();

  vhdl = dev_to_vhdl(dev);
  if (vhdl == GRAPH_VERTEX_NONE)
     return(ENODEV);

  if ((tsiotype = (tsiotype_t *)hwgraph_fastinfo_get(vhdl)) == NULL){
    dprintf(("no tsiotype\n"));
    return(ENODEV);
  }

  ASSERT(tsiotype->urbidx >= 0 && tsiotype->urbidx < N_URB);
  urb=&urbtab[tsiotype->urbidx];

  /* 
   *  user-mode library first does open(), then ioctl(TSIO_ACQUIRE_RB), 
   *  then mmap() 
   */
  if (urb->toplevel_state == TSIO_BETWEEN_OPEN_AND_ACQUIRE &&
      cmd != TSIO_ACQUIRE_RB)
    { UNLOCK_TSIO();  return EINVAL; } 
  
  switch(cmd) 
    {
    case TSIO_ACQUIRE_RB:
#ifdef TSIO_DEBUG
#ifdef PRINTF_ON_MINOR_DEVICE_EVENT
      printf("tsioioctl: acquire m=%d\n", m);
#endif
#endif
      
      if (urb->toplevel_state != TSIO_BETWEEN_OPEN_AND_ACQUIRE)
        { UNLOCK_TSIO(); return EINVAL; } 
      
      /* Read in the arguments */
      if ((copyin(arg, (caddr_t) & tsio_acquireurb,
                           sizeof(tsio_acquireurb_t))) == -1) 
        { UNLOCK_TSIO(); return EFAULT; } 
      
      ASSERT(urb->portidx >= 0 && urb->portidx < N_URB);

      /* driver does not support merging of TX data: only allow 
       * >1 acquire for the RX direction on this port 
       *
       * in other words, WE DON'T DO MIDI NOW!
       */
      if (tsio_acquireurb.direction == TSIO_FROMMIPS) /* TX/output */
        {
          int urbidx;
          
          for (urbidx=0; urbidx < N_URB; urbidx++)
            if (urbtab[urbidx].allocated &&
                urbtab[urbidx].portidx == urb->portidx &&
                urbtab[urbidx].toplevel_state!=TSIO_BETWEEN_OPEN_AND_ACQUIRE &&
                urbtab[urbidx].direction == TSIO_FROMMIPS)
              break;
          
          if (urbidx != N_URB)
            { UNLOCK_TSIO(); return EBUSY; }  /* we no merge */
        }

      /* restriction on port's queuesize */
      {
        int capacity = tsio_acquireurb.nlocs-1;
        if (capacity < 20 ||
            capacity >= 102400)
          { UNLOCK_TSIO(); return EINVAL; } 
      }
      
      /* see if we're the first urb on this port. if so, we set comm params */
      if (porttab[urb->portidx].n_urb_allocated == 1) /* the 1 is us! */
        {
          porttab[urb->portidx].commparams = tsio_acquireurb.commparams;
        }
      else /* port has been opened before -- comm params must match */
        {
          tsio_comm_params_t *curcp=&porttab[urb->portidx].commparams;
          tsio_comm_params_t *newcp=&tsio_acquireurb.commparams;
          
          if (((curcp->cflag & TSIO_USED_CFLAGS) !=
               (newcp->cflag & TSIO_USED_CFLAGS)) ||
              (curcp->ospeed != newcp->ospeed) ||
              (curcp->flags != newcp->flags) ||
              (curcp->extclock_factor != newcp->extclock_factor))
            { UNLOCK_TSIO(); return EBUSY; } 
        }

      /* acquire has succeeded. signify by setting toplevel_state */
      urb->toplevel_state = TSIO_BETWEEN_ACQUIRE_AND_MAP;

      urb->direction = tsio_acquireurb.direction;
      urb->nlocs = tsio_acquireurb.nlocs;
      urb->TXheadRXtail = 0;
      break;

    default:
#ifdef TSIO_DEBUG
      printf("tsioioctl invalid m=%d cmd=%d\n", m, cmd);
#endif
      UNLOCK_TSIO();
      return EINVAL;
    }
  
  UNLOCK_TSIO();
  return 0;
}


/* driver map/unmap ----------------------------------------------------- */

/* ARGSUSED */
int
tsiomap(dev_t dev,       /* device number */
        vhandl_t *vt,    /* handle to caller's virtual address space */
        off_t off,       /* offset into device */
        int len,         /* number of bytes to map */
        int prot)        /* protections */
{
  int err;
  urb *urb;
  int dataoff;
  int stampsoff;
  int totallen;
  tsiotype_t *tsiotype;
  vertex_hdl_t vhdl;

  LOCK_TSIO();

  vhdl = dev_to_vhdl(dev);
  if (vhdl == GRAPH_VERTEX_NONE)
     return(ENODEV);

  if ((tsiotype = (tsiotype_t *)hwgraph_fastinfo_get(vhdl)) == NULL){
    dprintf(("no tsiotype\n"));
    return(ENODEV);
  }

  ASSERT(tsiotype->urbidx >= 0 && tsiotype->urbidx < N_URB);
  urb=&urbtab[tsiotype->urbidx];

#ifdef TSIO_DEBUG
#ifdef PRINTF_ON_MINOR_DEVICE_EVENT
  printf("tsiomap len=%d\n", len);
#endif
#endif
  
  /* 
   * user-mode library first does open(), then ioctl(TSIO_ACQUIRE_RB), 
   * then mmap() 
   */
  if (urb->toplevel_state != TSIO_BETWEEN_ACQUIRE_AND_MAP)
    {
      UNLOCK_TSIO();
      return EINVAL;
    }

  /* future ioctls: if necessary, check for TSIO_BETWEEN_MAP_AND_UNMAP */

  /* allocate memory for user rb: 
   * header + data rb + pad + stamp rb + pad out to page 
   */
  dataoff = sizeof(urbheader_t);
  stampsoff = roundup(dataoff+urb->nlocs, __builtin_alignof(stamp_t));
  totallen = stampsoff + sizeof(stamp_t)*urb->nlocs;

  /* note: both len and totallen are not rounded up to nearest pagesize */
  
  if (len != totallen) /* sanity check user input */
    {
      UNLOCK_TSIO();
      return EINVAL;
    }
  
#ifdef _VCE_AVOIDANCE
  if (vce_avoidance)
    urb->header = (urbheader_t *)kvpalloc(btoc(len),VM_VACOLOR,
                                           colorof(v_getaddr(vt)));
  else
#endif /* _VCE_AVOIDANCE */
    urb->header = (urbheader_t *)kvpalloc(btoc(len),0, 0);
  
  if(urb->header==NULL)
    {
      UNLOCK_TSIO();
      return ENOMEM;
    }
  
  urb->data = (unsigned char *)(((char *)urb->header) + dataoff);
  urb->stamps = (stamp_t *)(((char *)urb->header) + stampsoff);
  urb->maplen = len;
  
  /*
   * clean the RB header,  because as soon as we make it
   * active,  it's up for grabs by the interrupt routine. 
   * We want intreq to be 0 to keep the interrupt routine 
   * from trying to "wake" an RB that hasn't finished 
   * initializing yet.
   */
  ((urbheader_t *)urb->header)->intreq = 0;
  ((urbheader_t *)urb->header)->head = 0;
  ((urbheader_t *)urb->header)->tail = 0;
  ((urbheader_t *)urb->header)->fillpt=0;   /* rb fill point */
  ((urbheader_t *)urb->header)->fillunits = TSIO_FILLUNITS_BYTES;

  /*
   * attempt to activate the urb.  this may involve initializing
   * the sioport, which could fail.
   */
  if (err=activate_urb(urb))
    {
      kvpfree(urb->header,btoc(len));
      urb->header = NULL;
      UNLOCK_TSIO();
      return(err);
    }

  /* Map it into the user's space.  If it can't be mapped, undo what we've
   * done so far and return an error.
   */
  if (err=v_mapphys(vt, urb->header, len))
    {
      deactivate_urb(urb);
      kvpfree(urb->header,btoc(len));
      urb->header = NULL;
      UNLOCK_TSIO();
      return(err);
    }

  /* map has succeeded.  indicate by advancing to next toplevel_state */
  urb->toplevel_state = TSIO_BETWEEN_MAP_AND_UNMAP;

  UNLOCK_TSIO();

  return 0;
}

/* ARGSUSED */
int
tsiounmap(dev_t dev,       /* device number */
          vhandl_t *vt)    /* handle to caller's virtual address space */
{
  urb *urb;
  tsiotype_t *tsiotype;
  vertex_hdl_t vhdl;

  LOCK_TSIO();

  vhdl = dev_to_vhdl(dev);
  if (vhdl == GRAPH_VERTEX_NONE)
     return(ENODEV);

  if ((tsiotype = (tsiotype_t *)hwgraph_fastinfo_get(vhdl)) == NULL){
    dprintf(("no tsiotype\n"));
    return(ENODEV);
  }

  ASSERT(tsiotype->urbidx >= 0 && tsiotype->urbidx < N_URB);
  urb=&urbtab[tsiotype->urbidx];

#ifdef TSIO_DEBUG
#ifdef PRINTF_ON_MINOR_DEVICE_EVENT
  printf("tsiounmap\n");
#endif
#endif

  if (urb->toplevel_state != TSIO_BETWEEN_MAP_AND_UNMAP)
    {
      UNLOCK_TSIO();
      return EINVAL;
    }

  deactivate_urb(urb);
  
  /* now interrupt will not touch this urb */
  
  kvpfree(urb->header,btoc(urb->maplen));
  urb->header = NULL;

  /* unmap has succeeded.  indicate by advancing to next toplevel state */
  urb->toplevel_state = TSIO_BETWEEN_UNMAP_AND_CLOSE;

  UNLOCK_TSIO();

  return 0; 
}

/* driver poll ----------------------------------------------------------- */

/* ARGSUSED */
int
tsiopoll(dev_t dev,
           register short events,
           int anyyet,
           short *reventsp,
           struct pollhead **phpp,
           unsigned int *genp) /* loadable module entry point */
{
  int ready;
  urb *urb;
  int fillunits;
  tsiotype_t *tsiotype;
  vertex_hdl_t vhdl;
  
  LOCK_TSIO();

  ready = events;

  vhdl = dev_to_vhdl(dev);
  if (vhdl == GRAPH_VERTEX_NONE)
     return(ENODEV);

  if ((tsiotype = (tsiotype_t *)hwgraph_fastinfo_get(vhdl)) == NULL){
    dprintf(("no tsiotype\n"));
    return(ENODEV);
  }

  ASSERT(tsiotype->urbidx >= 0 && tsiotype->urbidx < N_URB);
  urb=&urbtab[tsiotype->urbidx];

  if (urb->toplevel_state != TSIO_BETWEEN_MAP_AND_UNMAP)
    {
      UNLOCK_TSIO();
      return EINVAL;
    }
  
  ASSERT(urb->header != NULL);
  fillunits = ((volatile urbheader_t *)urb->header)->fillunits;

  if (fillunits != TSIO_FILLUNITS_BYTES &&
      fillunits != TSIO_FILLUNITS_UST)
    {
      cmn_err(CE_WARN, "tsio poll: fillunits corrupted\n");
      UNLOCK_TSIO();
      return EINVAL;
    }

    /* Fix for pv #481256 */
    /*
     * Snapshot pollhead generation number before checking event state
     * since we don't hold a lock while doing the check.
     */
    *genp = POLLGEN(&urb->pq);
    /* End fix for pv #481256 */
  
  if (urb->direction == TSIO_TOMIPS) /* RX, input */
    {
      stamp_t n_filled;
      int tail = urb->TXheadRXtail; /* tail for RX */

      /* get a snapshot of the user's rb head */
      int head = ((volatile urbheader_t *)urb->header)->head;
      
      if (head < 0 || head >= urb->nlocs)
        {
          cmn_err(CE_WARN, "tsio poll: user rb head is corrupted\n"); 
          /*
           * we forcibly set the user's head to indicate that the
           * buffer is full; putting head back into a valid state
           * helps to prevent hundreds of identical cmn_err's from
           * spewing out on the console.
           */
          head = tail+1;
          if (head > urb->nlocs) head -= urb->nlocs;
          ((volatile urbheader_t *)urb->header)->head = head;
          UNLOCK_TSIO();
          return EINVAL;
        }
      
      /* first compute # of bytes filled */
      
      n_filled = tail - head;
      if (n_filled < 0) n_filled += urb->nlocs; /* wrap */
      
      if (fillunits == TSIO_FILLUNITS_UST) /* make that UST filled instead */
        {
          if (n_filled != 0) /* if there are some UST stamps on queue */
            n_filled = get_current_ust() - urb->stamps[head];
        }
      
      if (n_filled < ((volatile urbheader_t *)urb->header)->fillpt)
        {
          ready &= ~(POLLIN | POLLRDNORM);

          if (!anyyet)
            *phpp = &urb->pq;
          
          ((volatile urbheader_t *)urb->header)->intreq = 1;
        }
    }
  else if (urb->direction == TSIO_FROMMIPS) /* TX, output */
    {
      stamp_t n_filled;
      int head = urb->TXheadRXtail; /* head for TX */

      /* get a snapshot of user's rb tail */
      int tail = ((volatile urbheader_t *)urb->header)->tail;
      
      if (tail < 0 || tail >= urb->nlocs)
        {
          cmn_err(CE_WARN, "tsio poll: user rb tail is corrupted\n");
          /*
           * we forcibly set the user's tail to indicate that the
           * buffer is empty; putting tail back into a valid state
           * helps to prevent hundreds of identical cmn_err's from
           * spewing out on the console.
           */
          tail = head;
          ((volatile urbheader_t *)urb->header)->tail = tail;
          UNLOCK_TSIO();
          return EINVAL;
        }
      
      /* first compute # of bytes filled */
      
      n_filled = tail - head;
      if (n_filled < 0) n_filled += urb->nlocs; /* wrap */
      
      if (fillunits == TSIO_FILLUNITS_UST) /* make that UST filled instead */
        {
          if (n_filled != 0) /* if there are some UST stamps on queue */
            n_filled = urb->stamps[tail] - get_current_ust();
        }

      if (n_filled >= ((volatile urbheader_t *)urb->header)->fillpt)
        {
          ready &= ~POLLOUT;
          
          if (!anyyet)
            *phpp = &urb->pq;
          
          ((volatile urbheader_t *)urb->header)->intreq = 1;
        }
    }

  *reventsp = ready;

  UNLOCK_TSIO();

  return(0);
}

#ifdef TSERIALIO_TIMEOUT_IS_THREADED
#define ONEms (1000000 / NSEC_PER_CYCLE)

extern int tserialio_intr_pri;

void
start_tsio_timeout_thread(void)
{
        xthread_periodic_timeout("tsio",
                                tserialio_intr_pri,
                                ONEms,
                                CPU_NONE,
                                &tsio_thread_info,
                                (xpt_func_t *)tsio_tick,
                                (void *)0);
}
#endif


void
wake_rbs_doorbell(void)
{
  int s;
  while(1) {
    s=compare_and_swap_int(&wake_rbs_state,WAKE_RBS_SCANNING,WAKE_RBS_RESTART);
    if(s)break;
    s=compare_and_swap_int(&wake_rbs_state,WAKE_RBS_RESTART,WAKE_RBS_RESTART);
    if(s)break;
    s=compare_and_swap_int(&wake_rbs_state,WAKE_RBS_IDLE,WAKE_RBS_SCANNING);
    if(s) {
        itimeout(tsio_timepoke_to_wake_rbs, 0, TIMEPOKE_NOW, plbase);
	break;
    }
  }
}
