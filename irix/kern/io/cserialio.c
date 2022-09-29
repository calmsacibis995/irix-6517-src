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

/* Character interface upper layer for modular serial i/o driver */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <ksys/serialio.h>
#include <sys/serialio.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/strmp.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/stty_ld.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/ksignal.h>

/* port union shortcuts */
#define sio_read_sv	sio_sv
#define sio_write_sv	sio_sv1

/* values for flags */
#define READER 	0x0001	/* reader sleeping on this port */
#define WRITER 	0x0002	/* writer sleeping on this port */
#define HW_ERR	0x0004	/* a lower layer error occurred */
#define DRAIN	0x0008	/* output being drained */
#define BRK	0x0010	/* a break is in progress */

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

int csio_devflag = D_MP;

static void csio_data_ready	(sioport_t *port);
static void csio_output_lowat	(sioport_t *port);
static void csio_ncs		(sioport_t *port, int ncs);
static void csio_dDCD		(sioport_t *port, int dcd);
static void csio_dCTS		(sioport_t *port, int cts);
static void csio_detach		(sioport_t *port);

static void csio_drain		(sioport_t *port);
static int csio_config		(sioport_t *port);

static struct serial_callup csio_callup = {
    csio_data_ready,
    csio_output_lowat,
    csio_ncs,
    csio_dDCD,
    csio_dCTS,
    csio_detach
};

/* circular i/o buffer constants */
#define CBUFLEN 256
typedef ushort_t sio_pc_t; /* cbuf prod/cons pointer */

/* must be power of 2 */
#if CBUFLEN & (CBUFLEN-1)
choke;
#endif

#define CBUFMASK (CBUFLEN-1)

/* macro evaluates true if cbuf is more than 3/4 full */
#define CBUF_HIWAT(upper) \
	((upper)->sio_rx_prod - (upper)->sio_rx_cons > (CBUFLEN * 3 / 4))

struct upper {
    sv_t sio_sv;
    sv_t sio_sv1;

    /* transmit/receive buffers used to keep data flowing to/from
     * the hardware at interrupt time without having to wait for
     * a potentially slow upper half to wake up, get streams monitor
     * or whatever else it may have to do
     */
    sio_pc_t sio_tx_prod, sio_tx_cons;
    sio_pc_t sio_rx_prod, sio_rx_cons;

    char sio_tx_data[CBUFLEN];
    char sio_rx_data[CBUFLEN];
    int sio_flags;

    /* timeout duration in ticks to hold chars for before waking up a
     * reader.
     */
    int sio_itimer;
    toid_t sio_toid;

    struct termio sio_termio;
    pid_t sio_sigpid;	/* pid of process to send sig to on BREAK */
};

static void
csio_pull_cbuf(sioport_t *port)
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

	bytes = DOWN_READ(port,
			  upper->sio_rx_data + (upper->sio_rx_prod & CBUFMASK),
			  avail);
	if (bytes < 0) {
	    upper->sio_flags |= HW_ERR;
	    break;
	}
	upper->sio_rx_prod += bytes;
    } while(bytes);
    
    ASSERT((sio_pc_t)(upper->sio_rx_prod - upper->sio_rx_cons) <= CBUFLEN);

    dprintf(("cbuf has %d bytes\n",
	     (sio_pc_t)(upper->sio_rx_prod - upper->sio_rx_cons)));
}

static void
itimer_timeout(void *arg)
{
    sioport_t *port = (sioport_t*)arg;
    struct upper *upper;

    ASSERT(sio_port_islocked(port) == 0);
    SIO_LOCK_PORT(port);

    if (upper = port->sio_upper) {

	upper->sio_toid = 0;
	
	if ((upper->sio_rx_prod != upper->sio_rx_cons ||
	     (upper->sio_flags & HW_ERR)) && (upper->sio_flags & READER)) {
	    upper->sio_flags &= ~READER;
	    sv_broadcast(&upper->sio_read_sv);
	}
    }

    SIO_UNLOCK_PORT(port);
}

static void
csio_data_ready(sioport_t *port)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    if (upper == 0)
	return;

    dprintf(("data ready\n"));
    csio_pull_cbuf(port);

    /* if we got some data or an error and someone is sleeping on
     * a read, wake him up
     */
    if ((upper->sio_rx_prod != upper->sio_rx_cons ||
	 (upper->sio_flags & HW_ERR)) && (upper->sio_flags & READER)) {
	dprintf(("wakeup reader\n"));

	/* If a timeout is requested and currently active, don't do
	 * anything. The timeout will wake up the reader. If a timeout
	 * is requested and is not currently active, set up a
	 * timeout. If a timeout is not requested, wake up the reader
	 * now. In any case, if the input buffer is more than 3/4
	 * full, wake up the reader now.
	 */
	if (upper->sio_itimer && !CBUF_HIWAT(upper)) {
	    if (upper->sio_toid == 0)
		upper->sio_toid = timeout(itimer_timeout, port, upper->sio_itimer);
	    ASSERT(upper->sio_toid);
	}
	else {
	    upper->sio_flags &= ~READER;
	    sv_broadcast(&upper->sio_read_sv);
	}
    }
}

/* write whatever data we can to the device from the circular buffer */
static void
write_tx_buf(sioport_t *port)
{
    int bytes = 1, avail;
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));
    
    while(upper->sio_tx_cons != upper->sio_tx_prod && bytes) {
	if ((upper->sio_tx_prod & CBUFMASK) > (upper->sio_tx_cons & CBUFMASK))
	    avail = (sio_pc_t)(upper->sio_tx_prod - upper->sio_tx_cons);
	else
	    avail = CBUFLEN - (upper->sio_tx_cons & CBUFMASK);
	bytes = DOWN_WRITE(port,
			   upper->sio_tx_data + (upper->sio_tx_cons & CBUFMASK),
			   avail);
	if (bytes < 0) {
	    /* pretend we wrote the bytes, but log the error */
	    upper->sio_flags |= HW_ERR;
	    upper->sio_tx_cons += avail;
	}
	else {
	    upper->sio_tx_cons += bytes;
	    dprintf(("wrote %d bytes to hardware\n", bytes));
	}
    }
}

static void
csio_output_lowat(sioport_t *port)
{
    struct upper *upper = port->sio_upper;

    ASSERT(sio_port_islocked(port));

    if (upper == 0)
	return;

    dprintf(("output lowat\n"));

    /* if there is nothing further to write from the tx_buf
     * we are no longer interested in being notified when output
     * drains
     */
    if (upper->sio_tx_prod == upper->sio_tx_cons) {
	if (DOWN_NOTIFICATION(port, N_ALL_OUTPUT, 0) < 0)
	    upper->sio_flags |= HW_ERR;

	/* If someone is waiting for output to drain, wake them up */
	if (upper->sio_flags & (HW_ERR | DRAIN)) {
	    upper->sio_flags &= ~DRAIN;
	    sv_broadcast(&upper->sio_write_sv);
	}
    }
    else
	/* try to send some more data from the output buffer. */
	write_tx_buf(port);

    /* if we have finished this buffer or a hardware error occurred,
     * wake up the writer process to send some more data or process
     * the error
     */
    if ((upper->sio_tx_prod == upper->sio_tx_cons ||
	 (upper->sio_flags & HW_ERR)) && (upper->sio_flags & WRITER)) {
	upper->sio_flags &= ~WRITER;
	dprintf(("wakeup writer\n"));
	sv_broadcast(&upper->sio_write_sv);
    }
}

/* ARGSUSED */
static void
csio_ncs(sioport_t *port, int ncs)
{
	struct upper *upper;
	ASSERT(sio_port_islocked(port));
	if ((upper = port->sio_upper) == 0)
		return;

	if ((ncs & NCS_BREAK) &&
	    (upper->sio_termio.c_iflag & IGNBRK) == 0 &&
	    (upper->sio_termio.c_iflag & BRKINT)) {

		upper->sio_tx_prod = upper->sio_tx_cons;
		upper->sio_rx_prod = upper->sio_rx_cons;

		if (upper->sio_sigpid)
			sigtopid(upper->sio_sigpid, SIGINT,
					SIG_ISKERN|SIG_NOSLEEP, 0,
					(cred_t *)NULL, (k_siginfo_t *)NULL);
	}

	dprintf(("csio ncs %d\n", ncs));
}

/* ARGSUSED */
static void
csio_dDCD(sioport_t *port, int dcd)
{
	dprintf(("csio ddcd %d\n", dcd));
}

/* ARGSUSED */
static void
csio_dCTS(sioport_t *port, int cts)
{
	dprintf(("csio dcts %d\n", cts));
}

/* port has been detached by lower layer, notify anyone using it */
static void
csio_detach (sioport_t *port)
{
	struct upper *upper;
	ASSERT(sio_port_islocked(port) == 0);

	SIO_LOCK_PORT(port);
	if (upper = port->sio_upper) {
		upper->sio_flags |= HW_ERR;
		if (upper->sio_flags & READER)
			sv_broadcast(&upper->sio_read_sv);
		if (upper->sio_flags & WRITER)
			sv_broadcast(&upper->sio_write_sv);
	}
	SIO_UNLOCK_PORT(port);
}

/* callback called when the first process to open the port exits.
 * Needed because if one proc opens the port and becomes the signal
 * target, then another proc opens the port, then the first exits, the
 * close routine will not get called and the sio_sigpid field will
 * point to a dead vproc. If we get a break it's anyone's
 * guess what happens.
 */
static void
csio_exit(void *arg)
{
	sioport_t *port = (sioport_t*)arg;
	struct upper *upper;

	SIO_LOCK_PORT(port);
	if ((upper = port->sio_upper) && (upper->sio_sigpid == current_pid()))
		upper->sio_sigpid = 0;

	SIO_UNLOCK_PORT(port);
}

/* ARGSUSED */
int
csio_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{ 
    sioport_t *port;
    dev_t dev = *devp;
    int error;
    struct upper *upper;

    port = (sioport_t *)device_info_get(dev);
    if (!port)
	return(ENODEV);

    if ((upper = (struct upper*)
	 kmem_zalloc(sizeof(struct upper), KM_SLEEP)) == 0)
	return(ENOMEM);
    sv_init(&upper->sio_sv, 0, "sio_sv");
    sv_init(&upper->sio_sv1, 0, "sio_sv1");

    SIO_LOCK_PORT(port);

    /* make sure this port isn't owned by another upper layer */
    if (port->sio_callup && port->sio_callup != &csio_callup) {
	SIO_UNLOCK_PORT(port);
	sv_destroy(&upper->sio_sv);
	sv_destroy(&upper->sio_sv1);
	kmem_free(upper, sizeof(struct upper));
	return(EBUSY);
    }

    /* If this is the first open, go through the opening protocol */
    if (port->sio_upper == 0) {
	char buf[16];
	extern struct stty_ld def_stty_ld;

	/* set the callup vector */
	port->sio_callup = &csio_callup;

	port->sio_upper = upper;

#if DEBUG
	port->sio_lockcalls = L_LOCK_ALL;
#endif

	if (DOWN_OPEN(port)) {
	    error = EIO;
	    goto err_return;
	}

	/* default to rs232, internal clock only */
	(void)DOWN_SET_PROTOCOL(port, PROTO_RS232);
	(void)DOWN_SET_EXTCLK(port, 0);

	/* register a callback to clear sio_sigproc when this process
	 * exits
	 */
	error = add_exit_callback(current_pid(), 0,
				(void (*)(void *))csio_exit, port);
	ASSERT(error == 0);
	
	upper->sio_termio = def_stty_ld.st_termio;
	if (error = csio_config(port))
	    goto err_return;

	if (DOWN_RX_TIMEOUT(port, 10) < 0) {
	    error = ENODEV;
	    goto err_return;
	}	    
	
	/* DMA must be enabled before calling READ since READ does
	 * a DRAIN, which requires DMA in order to succeed
	 */
	if (DOWN_NOTIFICATION(port, N_DATA_READY | N_BREAK, 1) < 0) {
	    error = ENODEV;
	    goto err_return;
	}
	while((error = DOWN_READ(port, buf, sizeof(buf))) > 0);
	if (error < 0) {
	    DOWN_NOTIFICATION(port, N_ALL, 0);
	    error = ENODEV;
	    goto err_return;
	}
	upper->sio_tx_prod = upper->sio_tx_cons;
	upper->sio_rx_prod = upper->sio_rx_cons;

	upper->sio_sigpid = current_pid();

	SIO_UNLOCK_PORT(port);
    }
    else {
	SIO_UNLOCK_PORT(port);
	sv_destroy(&upper->sio_sv);
	sv_destroy(&upper->sio_sv1);
	kmem_free(upper, sizeof(struct upper));
    }	

    dprintf(("csio_open\n"));
    return(0);

  err_return:
    port->sio_callup = 0;
    port->sio_upper = 0;
    SIO_UNLOCK_PORT(port);
    sv_destroy(&upper->sio_sv);
    sv_destroy(&upper->sio_sv1);
    kmem_free(upper, sizeof(struct upper));
    return(error);
}

/* ARGSUSED */
int
csio_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    sioport_t *port;
    struct upper *upper;

    port = (sioport_t *)device_info_get(dev);
    if (!port)
	return(ENODEV);

    SIO_LOCK_PORT(port);
    upper = port->sio_upper;

    /* don't return until the output tx_buf has been drained to
     * the hardware or a hardware error occurred
     */
    csio_drain(port);

    DOWN_NOTIFICATION(port, N_ALL, 0);

    port->sio_callup = 0;
    port->sio_upper = 0;
    SIO_UNLOCK_PORT(port);

    sv_destroy(&upper->sio_sv);
    sv_destroy(&upper->sio_sv1);
    kmem_free(upper, sizeof(struct upper));

    dprintf(("csio_close\n"));
    return(0);
}

static int
is_blocking(sioport_t *port)
{
    struct upper *upper = port->sio_upper;
    struct termio *tp = &upper->sio_termio;

    ASSERT(sio_port_islocked(port));

    /* we are non blocking only if in non-canonical mode and VMIN and
     * VTIME are both 0
     */
    if ((tp->c_lflag & ICANON) == 0 &&
	tp->c_cc[VMIN] == 0 &&
	tp->c_cc[VTIME] == 0)
	return(0);
    else
	return(1);
}

/* ARGSUSED */
int
csio_read(dev_t dev, uio_t *uiop, cred_t *crp)
{
    int bytes, total;
    iovec_t *iov;
    sioport_t *port;
    struct upper *upper;

    dprintf(("csio_read\n"));

    port = (sioport_t *)device_info_get(dev);
    if (!port)
	return(EIO);

    total = (int)uiop->uio_resid;
    iov = uiop->uio_iov;

    SIO_LOCK_PORT(port);
    upper = port->sio_upper;

    while(uiop->uio_resid) {

	/* pull as much data to the circular buffer as we can
	 */
	csio_pull_cbuf(port);

	/* grab data from the circular buffer. The number of bytes
	 * we can grab at a time is the smallest of: the size of
	 * the receiving uio buffer, the number of bytes in the
	 * circular buffer, and the number of bytes from the current
	 * consumer pointer to the end of the buffer in the circular
	 * buffer.
	 */
	bytes = (int)MIN(iov->iov_len,
			 MIN((sio_pc_t)(upper->sio_rx_prod - upper->sio_rx_cons),
			      CBUFLEN - (upper->sio_rx_cons & CBUFMASK)));

	if (uiop->uio_segflg == UIO_SYSSPACE) /* XXX necessary? */
	    bcopy(upper->sio_rx_data + (upper->sio_rx_cons & CBUFMASK),
		  iov->iov_base, bytes);
	else {
	    if (copyout(upper->sio_rx_data + (upper->sio_rx_cons & CBUFMASK),
			iov->iov_base, bytes)) {
		SIO_UNLOCK_PORT(port);
		return(EFAULT);
	    }
	}

	/* if we got some data, update pointers and continue trying
	 * to read
	 */
	if (bytes) {
	    dprintf(("read got %d bytes from cbuf\n", bytes));
	    upper->sio_rx_cons += bytes;
	    uiop->uio_resid -= bytes;
	    ASSERT(uiop->uio_resid >= 0);
	    
	    uiop->uio_offset += bytes;
	    iov->iov_base = (void*)((__psint_t)iov->iov_base + bytes);
	    iov->iov_len -= bytes;
	    
	    if (iov->iov_len == 0) {
		uiop->uio_iovcnt--;
		iov++;
	    }
	}
	/* if we got no data this time and we have not gotten any at
	 * all so far, block waiting if in blocking mode.
	 */
	else if (is_blocking(port)) {
	    ASSERT(upper->sio_rx_prod == upper->sio_rx_cons);

	    /* check for a hardware error before sleeping */
	    if (upper->sio_flags & HW_ERR) {
		SIO_UNLOCK_PORT(port);
		eprintf(("hw err on read\n"));
		return(EIO);
	    }

	    if (uiop->uio_resid == total) {
		upper->sio_flags |= READER;
		dprintf(("reader sleeping\n"));
		if (sio_sleep(port, &upper->sio_read_sv, PZERO+1))
		    return(EINTR);

		SIO_LOCK_PORT(port);
	    }
	    else
		break;
	}
	else
	    break;
    }

    dprintf(("read returning with %d bytes\n", total - uiop->uio_resid));

    SIO_UNLOCK_PORT(port);

    if (uiop->uio_resid == total)
	return(EWOULDBLOCK);
    else
	return(0);
}

/* ARGSUSED */
int
csio_write(dev_t dev, uio_t *uiop, cred_t *crp)
{
    sioport_t *port;
    iovec_t *iov;
    int bytes;
    struct upper *upper;

    port = (sioport_t *)device_info_get(dev);
    if (!port)
	return(EIO);

    iov = uiop->uio_iov;

    SIO_LOCK_PORT(port);
    upper = port->sio_upper;

    dprintf(("write\n"));
    while(uiop->uio_resid) {

	/* write data to the circular buffer. The number of bytes
	 * we can write at a time is the smallest of: the size of
	 * the uio buffer, the number of available bytes in the
	 * circular buffer, and the number of bytes from the current
	 * producer pointer to the end of the buffer in the circular
	 * buffer.
	 */
	bytes = (int)MIN(iov->iov_len,
	      MIN(CBUFLEN - (sio_pc_t)(upper->sio_tx_prod - upper->sio_tx_cons),
			     CBUFLEN - (upper->sio_tx_prod & CBUFMASK)));

	if (uiop->uio_segflg == UIO_SYSSPACE)
	    bcopy(iov->iov_base,
		  upper->sio_tx_data + (upper->sio_tx_prod & CBUFMASK), bytes);
	else {
	    if (copyin(iov->iov_base,
		       upper->sio_tx_data + (upper->sio_tx_prod & CBUFMASK),
		       bytes)) {
		SIO_UNLOCK_PORT(port);
		return(EFAULT);
	    }
	}
	upper->sio_tx_prod += bytes;

	if (bytes) {

	    /* kick off the TX */
	    write_tx_buf(port);
	    
	    /* write_tx_buf may have generated a hardware error */
	    if ((upper->sio_flags & HW_ERR) ||
		DOWN_NOTIFICATION(port, N_OUTPUT_LOWAT, 1) < 0) {
		SIO_UNLOCK_PORT(port);
		eprintf(("hw error on write\n"));
		return(EIO);
	    }

	    dprintf(("wrote %d bytes to cbuf\n", bytes));
	    uiop->uio_resid -= bytes;
	    ASSERT(uiop->uio_resid >= 0);
	    
	    uiop->uio_offset += bytes;
	    iov->iov_base = (void*)((__psint_t)iov->iov_base + bytes);
	    iov->iov_len -= bytes;
	    
	    if (iov->iov_len == 0) {
		uiop->uio_iovcnt--;
		iov++;
	    }
	}
	else {
	    ASSERT((sio_pc_t)
		   (upper->sio_tx_prod - upper->sio_tx_cons) == CBUFLEN);

	    /* make sure there isn't a hardware error posted before we
	     * sleep
	     */
	    if (upper->sio_flags & HW_ERR) {
		SIO_UNLOCK_PORT(port);
		eprintf(("hw err on write\n"));
		return(EIO);
	    }

	    /* we couldn't write anything further to the cbuf. let
	     * the interrupt handler take it for a while, and wake us
	     * up when the cbuf is empty.
	     */
	    /* XXX NBIO? */
	    upper->sio_flags |= WRITER;
	    dprintf(("writer sleeping\n"));
	    if (sio_sleep(port, &upper->sio_write_sv, PZERO+1))
		return(EINTR);

	    SIO_LOCK_PORT(port);
	}
    }
    ASSERT(uiop->uio_iovcnt == 0);

    SIO_UNLOCK_PORT(port);
    return(0);
}

/* timeout so drain doesn't wait forever if output is stalled for some
 * reason.
 */
static void
drain_timeout(void *arg)
{
    sioport_t *port = (sioport_t*)arg;
    struct upper *upper;

    SIO_LOCK_PORT(port);
    if ((upper = port->sio_upper) && (upper->sio_flags & DRAIN)) {
	upper->sio_flags &= ~DRAIN;
	upper->sio_tx_cons = upper->sio_tx_prod;
	sv_broadcast(&upper->sio_write_sv);
    }
    SIO_UNLOCK_PORT(port);
}

static void
csio_drain(sioport_t *port)
{
    struct upper *upper = port->sio_upper;
    toid_t toid;

    ASSERT(sio_port_islocked(port));

    /* sleep until all output has drained and BRK has completed */
    toid = timeout(drain_timeout, port, 10 * HZ);
    while((upper->sio_tx_prod != upper->sio_tx_cons ||
	   (upper->sio_flags & BRK)) &&
	  !(upper->sio_flags & HW_ERR)) {

	upper->sio_flags |= DRAIN;
	if (sio_sleep(port, &upper->sio_write_sv, PZERO+1)) {
	    SIO_LOCK_PORT(port);
	    break;
	}
	SIO_LOCK_PORT(port);
    }
    untimeout(toid);
}

static int
csio_config(sioport_t *port)
{
    struct termio *tp = &((struct upper*)(port->sio_upper))->sio_termio;

    int baud, byte_size, stop_bits, parenb, parodd;

    baud = tp->c_ispeed = tp->c_ospeed;
    switch(tp->c_cflag & CSIZE) {
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
    stop_bits = tp->c_cflag & CSTOPB;
    parenb = tp->c_cflag & PARENB;
    parodd = tp->c_cflag & PARODD;
	  
    if (DOWN_CONFIG(port, baud, byte_size, stop_bits,
		    parenb, parodd))
	return(EINVAL);

    /* return error only if attempting to enable HFC on hardware
     * that doesn't support it
     */
    if (DOWN_ENABLE_HFC(port, tp->c_cflag & CNEW_RTSCTS) &&
	(tp->c_cflag & CNEW_RTSCTS))
	return(EINVAL);

    return(0);
}
	    
static void
termio_to_termios(struct termio *termio, struct termios *termios)
{
    termios->c_iflag = termio->c_iflag;
    termios->c_oflag = termio->c_oflag;
    termios->c_cflag = termio->c_cflag;
    termios->c_lflag = termio->c_lflag;
    termios->c_ospeed = termio->c_ospeed;
    termios->c_ispeed = termio->c_ispeed;
    bcopy(termio->c_cc, termios->c_cc, sizeof(termios->c_cc));
}

static void
termios_to_termio(struct termios *termios, struct termio *termio)
{
    termio->c_iflag = termios->c_iflag;
    termio->c_oflag = termios->c_oflag;
    termio->c_cflag = termios->c_cflag;
    termio->c_lflag = termios->c_lflag;
    termio->c_ospeed = termios->c_ospeed;
    termio->c_ispeed = termios->c_ispeed;
    bcopy(termios->c_cc, termio->c_cc, sizeof(termio->c_cc));
}

static void
brk_wakeup(caddr_t *arg)
{
    sioport_t *port = (sioport_t*)arg;
    struct upper *upper;
    SIO_LOCK_PORT(port);
    DOWN_BREAK(port, 0);

    if (upper = port->sio_upper) {
	/* call csio_output_lowat to kick off any waiting output if needed,
	 * or to wake up csio_drain()
	 */
	upper->sio_flags &= ~BRK;
	csio_output_lowat(port);
    }
    SIO_UNLOCK_PORT(port);
}

/*ARGSUSED*/
int
csio_ioctl(dev_t dev,
	   int cmd,
	   void *arg,
	   int mode,
	   struct cred *cr,
	   int *rvp)
{
    sioport_t *port;
    struct upper *upper;
    struct termios termios;
    int err = 0;
    struct termio *tp;

    port = (sioport_t *)device_info_get(dev);

    SIO_LOCK_PORT(port);
    upper = port->sio_upper;
    tp = &upper->sio_termio;

    switch(cmd) {
      case TCGETA:
	/* The argument is a pointer to a termio structure.  Get the
	 * parameters associated with the terminal and store in the
	 * termio structure referenced by arg.
	 */

	if (copyout(tp, arg, sizeof(struct termio)))
	    err = EFAULT;

	break;

      case TCSETA:
	/* The argument is a pointer to a termio structure.  Set the
	 * parameters associated with the terminal from the structure
	 * referenced by arg.  The change is immediate.
	 */
	if (copyin(arg, tp, sizeof(struct termio)))
	    err = EFAULT;
	else
	    err = csio_config(port);
	break;

      case TCSETAW:
	/* The argument is a pointer to a termio structure.  Wait for
	 * the output to drain before setting the new parameters.
	 * This form should be used when changing parameters that will
	 * affect output.
	 */
	if (copyin(arg, tp, sizeof(struct termio)))
	    err = EFAULT;
	else {
	    csio_drain(port);
	    err = csio_config(port);
	}

	break;

      case TCSETAF:
	/* The argument is a pointer to a termio structure.  Wait for
	 * the output to drain, then flush the input queue and set the
	 * new parameters.
	 */
	if (copyin(arg, tp, sizeof(struct termio)))
	    err = EFAULT;
	else {
	    csio_drain(port);
	    upper->sio_rx_prod = upper->sio_rx_cons;
	    err = csio_config(port);
	}
	break;

      case TCGETS:
	/* The argument is a pointer to a termios structure.  Get the
	 * parameters associated with the terminal and store in the
	 * termios structure referenced by arg.  See tcgetattr(3).
	 */
	termio_to_termios(tp, &termios);
	if (copyout(&termios, arg, sizeof(struct termios)))
	    err = EFAULT;
	break;

      case TCSETS:
	/* The argument is a pointer to a termios structure.  Set the
	 * parameters associated with the terminal from the structure
	 * referenced by arg.  The change is immediate.  See
	 * tcsetattr(3).
	 */
	if (copyin(arg, &termios, sizeof(termios)))
	    err = EFAULT;
	else {
	    termios_to_termio(&termios, tp);
	    err = csio_config(port);
	}
	break;

      case TCSETSW:
	/* The argument is a pointer to a termios structure.  Wait for
	 * the output to drain before setting the new parameters.
	 * This form should be used when changing parameters that will
	 * affect output.  See tcsetattr(3).
	 */
	if (copyin(arg, &termios, sizeof(termios)))
	    err = EFAULT;
	else {
	    termios_to_termio(&termios, tp);
	    csio_drain(port);
	    err = csio_config(port);
	}
	break;

      case TCSETSF:
	/* The argument is a pointer to a termios structure.  Wait for
	 * the output to drain, then flush the input queue and set the
	 * new parameters.  See tcsetattr(3).
	 */
	if (copyin(arg, &termios, sizeof(termios)))
	    err = EFAULT;
	else {
	    termios_to_termio(&termios, tp);
	    csio_drain(port);
	    upper->sio_rx_prod = upper->sio_rx_cons;
	    err = csio_config(port);
	}
	break;

      case TCSBRK:
	/* The argument is an int value.  Wait for the output to
	 * drain.  If arg is 0, then send a break (zero bits for 0.25
	 * seconds).  See tcsendbreak(3) and tcdrain(3).
	 */
	csio_drain(port);
	if (arg == 0 && (upper->sio_flags & BRK) == 0) {
	    DOWN_BREAK(port, 1);
	    upper->sio_flags |= BRK;
	    SIO_UNLOCK_PORT(port);
	    timeout(brk_wakeup, (caddr_t)port, HZ/4);
	    SIO_LOCK_PORT(port);
	}
	break;

      case TCFLSH: {
	  /* The argument is an int value.  If arg is 0, flush the input
	   * queue; if 1, flush the output queue; if 2, flush both the
	   * input and output queues. See tcflush(3).
	   */
	  int x = (int)(__psint_t)arg + 1;
	  if (x & 1)
	      upper->sio_rx_prod = upper->sio_rx_cons;
	  if (x & 2)
	      upper->sio_tx_prod = upper->sio_tx_cons;
	  break;
      }

      case SIOC_RS422:
	if (DOWN_SET_PROTOCOL(port, arg ? PROTO_RS422 : PROTO_RS232))
	    err = EINVAL;
	break;

      case SIOC_EXTCLK:
	if (DOWN_SET_EXTCLK(port, (int)(long)arg))
	    err = EINVAL;
	break;

      case SIOC_ITIMER: {
	  int val = (int)(long)arg;

	  if (val < 0 || val > 10 * HZ)
	      err = EINVAL;
	  else
	      upper->sio_itimer = val;
	  break;
      }

      default:
	err = EINVAL;
	break;
    }

    SIO_UNLOCK_PORT(port);
    return(err);
}
