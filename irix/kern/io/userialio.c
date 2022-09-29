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

/* user mode upper layer for modular serial i/o driver */

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <ksys/serialio.h>
#include <sys/serialio.h>
#include <sys/errno.h>
#include <sys/kmem.h>

int usio_devflag = D_MP;

/* userialio upper layer private data */
struct upper {
    sv_t usio_sv;
    int flags;
    int usio_gen;
    struct termio usio_termio;
};

static int usio_config(sioport_t *);

#define USIO_SENDING_BREAK		0x01

/*ARGSUSED*/
int
usio_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    sioport_t *port;
    char buf[64];
    struct upper *upper;
    int err;

    port = (sioport_t *)device_info_get(*devp);
    if (!port)
	return(ENODEV);
    
    if ((upper = (struct upper*)
	 kmem_zalloc(sizeof(struct upper), KM_SLEEP)) == 0)
	return(ENOMEM);
    sv_init(&upper->usio_sv, 0, "usio_sv");

    /* exclusive open on the device. We can't sync between user
     * processes, so only one at a time can open this port. Also, we
     * need to allow access by only one upper layer at a time
     */
    SIO_LOCK_PORT(port);
    if (port->sio_callup) {
	err = EBUSY;
	goto backout;
    }
    
    if (DOWN_OPEN(port)) {
	err = EIO;
	goto backout;
    }
    
#if DEBUG
    port->sio_lockcalls = L_LOCK_ALL;
#endif

    /* default to rs232, internal clock only */
    (void)DOWN_SET_PROTOCOL(port, PROTO_RS232);
    (void)DOWN_SET_EXTCLK(port, 0);

    upper->usio_termio.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CNEW_RTSCTS);
    upper->usio_termio.c_cflag |= CS8;
    upper->usio_termio.c_ispeed = upper->usio_termio.c_ospeed = 9600;
    
    port->sio_upper = upper;

    usio_config(port);
    while(DOWN_READ(port, buf, sizeof(buf)) > 0);

    /* we never take any interrupts */
    DOWN_NOTIFICATION(port, N_ALL, 0);

    /* we never get any upcalls, so we have no callup struct. Stuff a
     * non-zero value so nobody else can open the port
     */
    port->sio_callup = (struct serial_callup*)1L;

    SIO_UNLOCK_PORT(port);

    return(0);

  backout:
    SIO_UNLOCK_PORT(port);
    sv_destroy(&upper->usio_sv);
    kmem_free(upper, sizeof(struct upper));
    return(err);
}

/*ARGSUSED*/
int
usio_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    sioport_t *port;
    struct upper *upper;

    port = (sioport_t *)device_info_get(dev);

    SIO_LOCK_PORT(port);

    DOWN_UNMAP(port);
    port->sio_callup = 0;
    upper = port->sio_upper;
    port->sio_upper = 0;

    SIO_UNLOCK_PORT(port);

    sv_destroy(&upper->usio_sv);
    kmem_free(upper, sizeof(struct upper));

    return(0);
}

static void
usio_wakeup(void *p, int generation)
{
    sioport_t *port = (sioport_t*)p;
    struct upper *upper;

    SIO_LOCK_PORT(port);
    if (upper = (struct upper*)port->sio_upper) {
	if (upper->usio_gen == generation &&
	    (upper->flags & USIO_SENDING_BREAK)) {
	    upper->flags &= ~USIO_SENDING_BREAK;
	    sv_broadcast(&upper->usio_sv);
	}
    }
    SIO_UNLOCK_PORT(port);
}

static int
usio_config(sioport_t *port)
{
    struct termio *tp = &((struct upper*)(port->sio_upper))->usio_termio;

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

/*ARGSUSED*/
int
usio_ioctl(dev_t dev,
	   int cmd,
	   void *arg,
	   int mode,
	   struct cred *cr,
	   int *rvp)
{
    sioport_t *port;
    int err = 0;
    struct upper *upper;
    struct termio *tp;
    struct termios termios;

    port = (sioport_t *)device_info_get(dev);
    SIO_LOCK_PORT(port);

    upper = (struct upper*)port->sio_upper;
    tp = &upper->usio_termio;

    switch(cmd) {
      case SIOC_USIO_GET_MAPID:
	*rvp = DOWN_GET_MAPID(port, 0);
	err = 0;
	break;

      case SIOC_USIO_GET_ARGS:
	if (arg == 0)
	    err = EFAULT;
	else
	    err = DOWN_GET_MAPID(port, arg);
	break;

      case SIOC_USIO_SET_SSCR:
	*rvp = DOWN_SET_SSCR(port, (int)(long)arg, 0);
	err = 0;
	break;

      case SIOC_USIO_RESET_SSCR:
	*rvp = DOWN_SET_SSCR(port, (int)(long)arg, 1);
	err = 0;
	break;

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
	    err = usio_config(port);
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
	    err = usio_config(port);
	}
	break;

      case TCSBRK:
	if (DOWN_BREAK(port, 1)) {
	    err = EINVAL;
	    break;
	}
	upper->flags |= USIO_SENDING_BREAK;
	upper->usio_gen++;
	timeout(usio_wakeup, port, HZ / 4, upper->usio_gen);
	while(upper->flags & USIO_SENDING_BREAK) {
	    if (sio_sleep(port, &upper->usio_sv, PUSER)) {
		SIO_LOCK_PORT(port);
		upper->flags &= ~USIO_SENDING_BREAK;
		DOWN_BREAK(port, 0);
		SIO_UNLOCK_PORT(port);
		return(EINTR);
	    }

	    SIO_LOCK_PORT(port);
	}
	DOWN_BREAK(port, 0);
	break;

      case SIOC_RS422:
	if (DOWN_SET_PROTOCOL(port, arg ? PROTO_RS422 : PROTO_RS232))
	    err = EINVAL;
	break;

      case SIOC_EXTCLK:
	if (DOWN_SET_EXTCLK(port, (int)(long)arg))
	    err = EINVAL;
	break;

      default:
	err = EINVAL;
	break;
    }

    SIO_UNLOCK_PORT(port);
    return(err);
}

/*ARGSUSED*/
int
usio_map(dev_t dev,
	 vhandl_t *vt,
	 off_t off,
	 size_t len,
	 uint_t prot)
{
    sioport_t *port;
    int err;

    port = (sioport_t *)device_info_get(dev);

    SIO_LOCK_PORT(port);
    err = DOWN_MAP(port, vt, off);
    SIO_UNLOCK_PORT(port);

    return(err);
}
