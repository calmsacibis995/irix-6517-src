/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.7 $"

/*	culine.c --differs from uucp
	line.c	2.4	1/28/84 00:56:32
	only in fixline() where termio line parameters
	for cu are set before remote connection is made.
*/
#include "uucp.h"

#define PACKSIZE	64
#define HEADERSIZE	6
#define SNDFILE	'S'
#define RCVFILE 'R'
#define RESET	'X'

int	linebaudrate = 0;	/* for speedup hook in pk (unused in ATTSV) */

extern Oddflag, Evenflag, Duplex, Terminal;	/*for cu options*/
extern char *P_PARITY;

#ifdef ATTSV

static struct termio Savettyb;
/*
 * set speed/echo/mode...
 *	tty 	-> terminal name
 *	spwant 	-> speed
 *	type	-> type
 *
 *	if spwant == 0, speed is untouched
 *	type is unused, but needed for compatibility
 *
 * return:  
 *	none
 */
/* ARGSUSED */
void
fixline(int tty, int spwant, int type)
{
	struct termio		lv;

	CDEBUG(6, "fixline(%d, ", tty);
	CDEBUG(6, "%d)\n", spwant);
	if (ioctl(tty, TCGETA, &lv) != 0)
		return;

/* set line attributes associated with -h, -t, -e, and -o options */

	lv.c_iflag = lv.c_oflag = lv.c_lflag = (ushort)0;
	lv.c_iflag = (IGNPAR | IGNBRK | ISTRIP | IXON | IXOFF);
	lv.c_cc[VEOF] = '\1';

	if (spwant)
		lv.c_ospeed = (speed_t)spwant;

	lv.c_cflag |= ( CREAD | (spwant ? HUPCL : 0));
	
	if(Evenflag) {				/*even parity -e */
		if(lv.c_cflag & PARENB) {
			VERBOSE(P_PARITY, 0);
			exit(1);
		}else 
			lv.c_cflag |= (PARENB | CS7);
	}
	else if(Oddflag) {			/*odd parity -o */
		if(lv.c_cflag & PARENB) {
			VERBOSE(P_PARITY, 0);
			exit(1);
		}else {
			lv.c_cflag |= PARODD;
			lv.c_cflag |= (PARENB | CS7);
		}
	}
	else
		lv.c_cflag |= CS8;


	if(!Duplex)				/*half duplex -h */
		lv.c_iflag &= ~(IXON | IXOFF);
	if(Terminal)				/* -t */
		lv.c_oflag |= (OPOST | ONLCR);

#ifdef NO_MODEM_CTRL
	/*   CLOCAL may cause problems on pdp11s with DHs */
	if (type == D_DIRECT) {
		CDEBUG(4, "fixline - direct\n", "");
		lv.c_cflag |= CLOCAL;
	} else
#endif /* NO_MODEM_CTRL */

		lv.c_cflag &= ~CLOCAL;
	
	ASSERT(ioctl(tty, TCSETAW, &lv) >= 0,
	    "RETURN FROM fixline ioctl", "", errno);
	return;
}

void
sethup(int dcf)
{
	struct termio ttbuf;

	if (ioctl(dcf, TCGETA, &ttbuf) != 0)
		return;
	if (!(ttbuf.c_cflag & HUPCL)) {
		ttbuf.c_cflag |= HUPCL;
		(void) ioctl(dcf, TCSETAW, &ttbuf);
	}
}

void
ttygenbrk(register int fn)
{
	if (isatty(fn)) 
		(void) ioctl(fn, TCSBRK, 0);
}


/*
 * optimize line setting for sending or receiving files
 * return:
 *	none
 */
void
setline(register char type)
{
	static struct termio tbuf;
	
	if (ioctl(Ifn, TCGETA, &tbuf) != 0)
		return;
	DEBUG(2, "setline - %c\n", type);
	switch (type) {
	case RCVFILE:
		if (tbuf.c_cc[VMIN] != PACKSIZE) {
		    tbuf.c_cc[VMIN] = PACKSIZE;
		    (void) ioctl(Ifn, TCSETAW, &tbuf);
		}
		break;

	case SNDFILE:
	case RESET:
		if (tbuf.c_cc[VMIN] != HEADERSIZE) {
		    tbuf.c_cc[VMIN] = HEADERSIZE;
		    (void) ioctl(Ifn, TCSETAW, &tbuf);
		}
		break;
	}
}

int
savline(void)
{
	int ret;

	ret = ioctl(0, TCGETA, &Savettyb);
	Savettyb.c_cflag = (Savettyb.c_cflag & ~CS8) | CS7;
	Savettyb.c_oflag |= OPOST;
	Savettyb.c_lflag |= (ISIG|ICANON|ECHO);
	return(ret);
}

int
restline(void)
{
	return(ioctl(0, TCSETAW, &Savettyb));
}
 
#else /* !ATTSV */

#error can't compile this way

static struct sgttyb Savettyb;

/***
 *	fixline(tty, spwant, type)	set speed/echo/mode...
 *	int tty, spwant;
 *
 *	if spwant == 0, speed is untouched
 *	type is unused, but needed for compatibility
 *
 *	return codes:  none
 */

/*ARGSUSED*/
fixline(tty, spwant, type)
int tty, spwant, type;
{
	struct sgttyb	ttbuf;
	struct sg_spds	*ps;
	int		 speed = -1;

	DEBUG(6, "fixline(%d, ", tty);
	DEBUG(6, "%d)\n", spwant);

	if (ioctl(tty, TIOCGETP, &ttbuf) != 0)
		return;
	if (spwant > 0) {
		for (ps = spds; ps->sp_val; ps++)
			if (ps->sp_val == spwant) {
				speed = ps->sp_name;
				break;
			}
		ASSERT(speed >= 0, "BAD SPEED", "", spwant);
		ttbuf.sg_ispeed = ttbuf.sg_ospeed = speed;
	} else {
		for (ps = spds; ps->sp_val; ps++)
			if (ps->sp_name == ttbuf.sg_ispeed) {
				spwant = ps->sp_val;
				break;
			}
		ASSERT(spwant >= 0, "BAD SPEED", "", ttbuf.sg_ispeed);
	}
	ttbuf.sg_flags = (ANYP | RAW);
	(void) ioctl(tty, TIOCSETP, &ttbuf);
	(void) ioctl(tty, TIOCHPCL, STBNULL);
	(void) ioctl(tty, TIOCEXCL, STBNULL);
	linebaudrate = spwant;		/* for hacks in pk driver */
	return;
}

sethup(dcf)
int	dcf;
{
	if (isatty(dcf)) 
		(void) ioctl(dcf, TIOCHPCL, STBNULL);
}

/***
 *	genbrk		send a break
 *
 *	return codes;  none
 */

ttygenbrk(fn)
{
	if (isatty(fn)) {
		(void) ioctl(fn, TIOCSBRK, 0);
		nap(6);				/* 0.1 second break */
		(void) ioctl(fn, TIOCCBRK, 0);
	}
	return;
}

/*
 * V7 and RT aren't smart enough for this -- linebaudrate is the best
 * they can do.
 */
/*ARGSUSED*/
setline(dummy) { }

savline()
{
	int	ret;

	ret = ioctl(0, TIOCGETP, &Savettyb);
	Savettyb.sg_flags |= ECHO;
	Savettyb.sg_flags &= ~RAW;
	return(ret);
}

restline()
{
	return(ioctl(0, TIOCSETP, &Savettyb));
}
#endif
