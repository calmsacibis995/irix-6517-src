/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)sys_term.c	5.11 (Berkeley) 9/14/90";
#endif /* not lint */

#include "telnetd.h"
#include "pathnames.h"

#include <utmp.h>
struct	utmp wtmp;

#ifdef sgi
#define BSD 43
#else
char	wtmpf[]	= "/usr/adm/wtmp";
char	utmpf[] = "/etc/utmp";
#endif

#ifdef _IRIX4
#include <xutmp.h>

static struct utmp entry;

#else /* !_IRIX4 */

#include <unistd.h>
#include <utmpx.h>

static struct utmpx entry;
#endif /* _IRIX4 */

#if !(BSD > 43)
#include <paths.h>
#endif

#define SCPYN(a, b)	(void) strncpy(a, b, sizeof(a))
#define SCMPN(a, b)	strncmp(a, b, sizeof(a))

#ifdef	STREAMS
#include <sys/stream.h>
#endif
#ifdef sgi
#include <sys/sysmacros.h>
#include <sys/stropts.h>
#include <sys/sat.h>
#include <sys/capability.h>
#else
#include <sys/tty.h>
#endif
#ifdef	t_erase
#undef	t_erase
#undef	t_kill
#undef	t_intrc
#undef	t_quitc
#undef	t_startc
#undef	t_stopc
#undef	t_eofc
#undef	t_brkc
#undef	t_suspc
#undef	t_dsuspc
#undef	t_rprntc
#undef	t_flushc
#undef	t_werasc
#undef	t_lnextc
#endif


#ifndef	USE_TERMIO
struct termbuf {
	struct sgttyb sg;
	struct tchars tc;
	struct ltchars ltc;
	int state;
	int lflags;
} termbuf, termbuf2;
# define	cfsetospeed(tp, val)	(tp)->sg.sg_ospeed = (val)
# define	cfsetispeed(tp, val)	(tp)->sg.sg_ispeed = (val)
#else	/* USE_TERMIO */
# ifdef	SYSV_TERMIO
#	define termios termio
# endif
# ifndef	TCSANOW
#  ifdef TCSETS
#   define	TCSANOW		TCSETS
#   define	TCSADRAIN	TCSETSW
#   define	tcgetattr(f, t)	iotcl(f, TCGETS, t)
#  else
#   ifdef TCSETA
#    define	TCSANOW		TCSETA
#    define	TCSADRAIN	TCSETAW
#    define	tcgetattr(f, t)	ioctl(f, TCGETA, t)
#   else
#    define	TCSANOW		TIOCSETA
#    define	TCSADRAIN	TIOCSETAW
#    define	tcgetattr(f, t)	iotcl(f, TIOCGETA, t)
#   endif
#  endif
#  define	tcsetattr(f, a, t)	ioctl(f, a, t)
#  define	cfsetospeed(tp, val)	(tp)->c_cflag &= ~CBAUD; \
					(tp)->c_cflag |= (val)
#  ifdef CIBAUD
#   define	cfsetispeed(tp, val)	(tp)->c_cflag &= ~CIBAUD; \
					(tp)->c_cflag |= ((val)<<IBSHIFT)
#  else
#   define	cfsetispeed(tp, val)	(tp)->c_cflag &= ~CBAUD; \
					(tp)->c_cflag |= (val)
#  endif
# endif /* TCSANOW */
struct termios termbuf, termbuf2;	/* pty control structure */
#endif	/* USE_TERMIO */

/*
 * init_termbuf()
 * copy_termbuf(cp)
 * set_termbuf()
 *
 * These three routines are used to get and set the "termbuf" structure
 * to and from the kernel.  init_termbuf() gets the current settings.
 * copy_termbuf() hands in a new "termbuf" to write to the kernel, and
 * set_termbuf() writes the structure into the kernel.
 */

init_termbuf()
{
#ifndef	USE_TERMIO
	(void) ioctl(pty, TIOCGETP, (char *)&termbuf.sg);
	(void) ioctl(pty, TIOCGETC, (char *)&termbuf.tc);
	(void) ioctl(pty, TIOCGLTC, (char *)&termbuf.ltc);
# ifdef	TIOCGSTATE
	(void) ioctl(pty, TIOCGSTATE, (char *)&termbuf.state);
# endif
#else
#ifdef sgi
	(void) tcgetattr(pty, &termbuf);
#else
	(void) tcgetattr(pty, (char *)&termbuf);
#endif
#endif
	termbuf2 = termbuf;
}

#if defined(LINEMODE) && defined(TIOCPKT_IOCTL)
copy_termbuf(cp, len)
char *cp;
int len;
{
	if (len > sizeof(termbuf))
		len = sizeof(termbuf);
	bcopy(cp, (char *)&termbuf, len);
	termbuf2 = termbuf;
}
#endif	/* defined(LINEMODE) && defined(TIOCPKT_IOCTL) */

set_termbuf()
{
	/*
	 * Only make the necessary changes.
	 */
#ifndef	USE_TERMIO
	if (bcmp((char *)&termbuf.sg, (char *)&termbuf2.sg, sizeof(termbuf.sg)))
		(void) ioctl(pty, TIOCSETN, (char *)&termbuf.sg);
	if (bcmp((char *)&termbuf.tc, (char *)&termbuf2.tc, sizeof(termbuf.tc)))
		(void) ioctl(pty, TIOCSETC, (char *)&termbuf.tc);
	if (bcmp((char *)&termbuf.ltc, (char *)&termbuf2.ltc,
							sizeof(termbuf.ltc)))
		(void) ioctl(pty, TIOCSLTC, (char *)&termbuf.ltc);
	if (termbuf.lflags != termbuf2.lflags)
		(void) ioctl(pty, TIOCLSET, (char *)&termbuf.lflags);
#else	/* USE_TERMIO */
	if (bcmp((char *)&termbuf, (char *)&termbuf2, sizeof(termbuf)))
#ifdef sgi
		(void) tcsetattr(pty, TCSADRAIN, &termbuf);
#else
		(void) tcsetattr(pty, TCSADRAIN, (char *)&termbuf);
#endif
#endif	/* USE_TERMIO */
}


/*
 * spcset(func, valp, valpp)
 *
 * This function takes various special characters (func), and
 * sets *valp to the current value of that character, and
 * *valpp to point to where in the "termbuf" structure that
 * value is kept.
 *
 * It returns the SLC_ level of support for this function.
 */

#ifndef	USE_TERMIO
spcset(func, valp, valpp)
int func;
cc_t *valp;
cc_t **valpp;
{
	switch(func) {
	case SLC_EOF:
		*valp = termbuf.tc.t_eofc;
		*valpp = (cc_t *)&termbuf.tc.t_eofc;
		return(SLC_VARIABLE);
	case SLC_EC:
		*valp = termbuf.sg.sg_erase;
		*valpp = (cc_t *)&termbuf.sg.sg_erase;
		return(SLC_VARIABLE);
	case SLC_EL:
		*valp = termbuf.sg.sg_kill;
		*valpp = (cc_t *)&termbuf.sg.sg_kill;
		return(SLC_VARIABLE);
	case SLC_IP:
		*valp = termbuf.tc.t_intrc;
		*valpp = (cc_t *)&termbuf.tc.t_intrc;
		return(SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_ABORT:
		*valp = termbuf.tc.t_quitc;
		*valpp = (cc_t *)&termbuf.tc.t_quitc;
		return(SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_XON:
		*valp = termbuf.tc.t_startc;
		*valpp = (cc_t *)&termbuf.tc.t_startc;
		return(SLC_VARIABLE);
	case SLC_XOFF:
		*valp = termbuf.tc.t_stopc;
		*valpp = (cc_t *)&termbuf.tc.t_stopc;
		return(SLC_VARIABLE);
	case SLC_AO:
		*valp = termbuf.ltc.t_flushc;
		*valpp = (cc_t *)&termbuf.ltc.t_flushc;
		return(SLC_VARIABLE);
	case SLC_SUSP:
		*valp = termbuf.ltc.t_suspc;
		*valpp = (cc_t *)&termbuf.ltc.t_suspc;
		return(SLC_VARIABLE);
	case SLC_EW:
		*valp = termbuf.ltc.t_werasc;
		*valpp = (cc_t *)&termbuf.ltc.t_werasc;
		return(SLC_VARIABLE);
	case SLC_RP:
		*valp = termbuf.ltc.t_rprntc;
		*valpp = (cc_t *)&termbuf.ltc.t_rprntc;
		return(SLC_VARIABLE);
	case SLC_LNEXT:
		*valp = termbuf.ltc.t_lnextc;
		*valpp = (cc_t *)&termbuf.ltc.t_lnextc;
		return(SLC_VARIABLE);
	case SLC_FORW1:
		*valp = termbuf.tc.t_brkc;
		*valpp = (cc_t *)&termbuf.ltc.t_lnextc;
		return(SLC_VARIABLE);
	case SLC_BRK:
	case SLC_SYNCH:
	case SLC_AYT:
	case SLC_EOR:
		*valp = (cc_t)0;
		*valpp = (cc_t *)0;
		return(SLC_DEFAULT);
	default:
		*valp = (cc_t)0;
		*valpp = (cc_t *)0;
		return(SLC_NOSUPPORT);
	}
}

#else	/* USE_TERMIO */

spcset(func, valp, valpp)
int func;
cc_t *valp;
cc_t **valpp;
{

#define	setval(a, b)	*valp = termbuf.c_cc[a]; \
			*valpp = &termbuf.c_cc[a]; \
			return(b);
#define	defval(a) *valp = ((cc_t)a); *valpp = (cc_t *)0; return(SLC_DEFAULT);

	switch(func) {
	case SLC_EOF:
		setval(VEOF, SLC_VARIABLE);
	case SLC_EC:
		setval(VERASE, SLC_VARIABLE);
	case SLC_EL:
		setval(VKILL, SLC_VARIABLE);
	case SLC_IP:
		setval(VINTR, SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_ABORT:
		setval(VQUIT, SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_XON:
#ifdef	VSTART
		setval(VSTART, SLC_VARIABLE);
#else
		defval(0x13);
#endif
	case SLC_XOFF:
#ifdef	VSTOP
		setval(VSTOP, SLC_VARIABLE);
#else
		defval(0x11);
#endif
	case SLC_EW:
#ifdef	VWERASE
		setval(VWERASE, SLC_VARIABLE);
#else
		defval(0);
#endif
	case SLC_RP:
#ifdef	VREPRINT
		setval(VREPRINT, SLC_VARIABLE);
#else
		defval(0);
#endif
	case SLC_LNEXT:
#ifdef	VLNEXT
		setval(VLNEXT, SLC_VARIABLE);
#else
		defval(0);
#endif
	case SLC_AO:
#if !defined(VDISCARD) && defined(VFLUSHO)
# define VDISCARD VFLUSHO
#endif
#ifdef	VDISCARD
		setval(VDISCARD, SLC_VARIABLE|SLC_FLUSHOUT);
#else
		defval(0);
#endif
	case SLC_SUSP:
#ifdef	VSUSP
		setval(VSUSP, SLC_VARIABLE|SLC_FLUSHIN);
#else
		defval(0);
#endif
#ifdef	VEOL
	case SLC_FORW1:
		setval(VEOL, SLC_VARIABLE);
#endif
#ifdef	VEOL2
	case SLC_FORW2:
		setval(VEOL2, SLC_VARIABLE);
#endif
	case SLC_AYT:
#ifdef	VSTATUS
		setval(VSTATUS, SLC_VARIABLE);
#else
		defval(0);
#endif

	case SLC_BRK:
	case SLC_SYNCH:
	case SLC_EOR:
		defval(0);

	default:
		*valp = 0;
		*valpp = 0;
		return(SLC_NOSUPPORT);
	}
}
#endif	/* USE_TERMIO */


/*
 * getpty()
 *
 * Allocate a pty.  As a side effect, the external character
 * array "line" contains the name of the slave side.
 *
 * Returns the file descriptor of the opened pty.
 */
#ifdef sgi
extern char *_getpty(int*, int, mode_t, int);
extern char *line;

getpty()
{
	int p;

	line = _getpty(&p, O_RDWR|O_NDELAY, 0600, 0);
	if (0 == line)
		return(-1);
	return p;
}

#else /* sgi */
#ifndef	__GNUC__
char *line = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
#else
static char Xline[] = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
char *line = Xline;
#endif

getpty()
{
	register int p;
	register char c, *p1, *p2;
	register int i;

	(void) sprintf(line, "/dev/ptyXX");
	p1 = &line[8];
	p2 = &line[9];

	for (c = 'p'; c <= 's'; c++) {
		struct stat stb;

		*p1 = c;
		*p2 = '0';
		if (stat(line, &stb) < 0)
			break;
		for (i = 0; i < 16; i++) {
			*p2 = "0123456789abcdef"[i];
			p = open(line, 2);
			if (p > 0) {
				line[5] = 't';
				return(p);
			}
		}
	}
	return(-1);
}
#endif /* sgi */

#ifdef	LINEMODE
/*
 * tty_flowmode()	Find out if flow control is enabled or disabled.
 * tty_linemode()	Find out if linemode (external processing) is enabled.
 * tty_setlinemod(on)	Turn on/off linemode.
 * tty_isecho()		Find out if echoing is turned on.
 * tty_setecho(on)	Enable/disable character echoing.
 * tty_israw()		Find out if terminal is in RAW mode.
 * tty_binaryin(on)	Turn on/off BINARY on input.
 * tty_binaryout(on)	Turn on/off BINARY on output.
 * tty_isediting()	Find out if line editing is enabled.
 * tty_istrapsig()	Find out if signal trapping is enabled.
 * tty_setedit(on)	Turn on/off line editing.
 * tty_setsig(on)	Turn on/off signal trapping.
 * tty_issofttab()	Find out if tab expansion is enabled.
 * tty_setsofttab(on)	Turn on/off soft tab expansion.
 * tty_islitecho()	Find out if typed control chars are echoed literally
 * tty_setlitecho()	Turn on/off literal echo of control chars
 * tty_tspeed(val)	Set transmit speed to val.
 * tty_rspeed(val)	Set receive speed to val.
 */

tty_flowmode()
{
#ifndef USE_TERMIO
	return((termbuf.tc.t_startc) > 0 && (termbuf.tc.t_stopc) > 0);
#else
	return(termbuf.c_iflag & IXON ? 1 : 0);
#endif
}


tty_linemode()
{
#ifndef	USE_TERMIO
	return(termbuf.state & TS_EXTPROC);
#else
	return(termbuf.c_lflag & EXTPROC);
#endif
}

tty_setlinemode(on)
int on;
{
#ifdef	TIOCEXT
	set_termbuf();
	(void) ioctl(pty, TIOCEXT, (char *)&on);
	init_termbuf();
#else	/* !TIOCEXT */
# ifdef	EXTPROC
	if (on)
		termbuf.c_lflag |= EXTPROC;
	else
		termbuf.c_lflag &= ~EXTPROC;
# endif
#endif	/* TIOCEXT */
}

tty_isecho()
{
#ifndef USE_TERMIO
	return (termbuf.sg.sg_flags & ECHO);
#else
	return (termbuf.c_lflag & ECHO);
#endif
}
#endif	/* LINEMODE */

tty_setecho(on)
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.sg.sg_flags |= ECHO|CRMOD;
	else
		termbuf.sg.sg_flags &= ~(ECHO|CRMOD);
#else
	if (on)
		termbuf.c_lflag |= ECHO;
	else
		termbuf.c_lflag &= ~ECHO;
#endif
}

#if defined(LINEMODE) && defined(KLUDGELINEMODE)
tty_israw()
{
#ifndef USE_TERMIO
	return(termbuf.sg.sg_flags & RAW);
#else
	return(!(termbuf.c_lflag & ICANON));
#endif
}
#endif	/* defined(LINEMODE) && defined(KLUDGELINEMODE) */

tty_binaryin(on)
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.lflags |= LPASS8;
	else
		termbuf.lflags &= ~LPASS8;
#else
	if (on) {
		termbuf.c_lflag &= ~ISTRIP;
	} else {
		termbuf.c_lflag |= ISTRIP;
	}
#endif
}

tty_binaryout(on)
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.lflags |= LLITOUT;
	else
		termbuf.lflags &= ~LLITOUT;
#else
	if (on) {
		termbuf.c_cflag &= ~(CSIZE|PARENB);
		termbuf.c_cflag |= CS8;
		termbuf.c_oflag &= ~OPOST;
	} else {
		termbuf.c_cflag &= ~CSIZE;
		termbuf.c_cflag |= CS7|PARENB;
		termbuf.c_oflag |= OPOST;
	}
#endif
}

tty_isbinaryin()
{
#ifndef	USE_TERMIO
	return(termbuf.lflags & LPASS8);
#else
	return(!(termbuf.c_iflag & ISTRIP));
#endif
}

tty_isbinaryout()
{
#ifndef	USE_TERMIO
	return(termbuf.lflags & LLITOUT);
#else
	return(!(termbuf.c_oflag&OPOST));
#endif
}

#ifdef	LINEMODE
tty_isediting()
{
#ifndef USE_TERMIO
	return(!(termbuf.sg.sg_flags & (CBREAK|RAW)));
#else
	return(termbuf.c_lflag & ICANON);
#endif
}

tty_istrapsig()
{
#ifndef USE_TERMIO
	return(!(termbuf.sg.sg_flags&RAW));
#else
	return(termbuf.c_lflag & ISIG);
#endif
}

tty_setedit(on)
int on;
{
#ifndef USE_TERMIO
	if (on)
		termbuf.sg.sg_flags &= ~CBREAK;
	else
		termbuf.sg.sg_flags |= CBREAK;
#else
	if (on)
		termbuf.c_lflag |= ICANON;
	else
		termbuf.c_lflag &= ~ICANON;
#endif
}

tty_setsig(on)
int on;
{
#ifndef	USE_TERMIO
	if (on)
		;
#else
	if (on)
		termbuf.c_lflag |= ISIG;
	else
		termbuf.c_lflag &= ~ISIG;
#endif
}
#endif	/* LINEMODE */

tty_issofttab()
{
#ifndef	USE_TERMIO
	return (termbuf.sg.sg_flags & XTABS);
#else
# ifdef	OXTABS
	return (termbuf.c_oflag & OXTABS);
# endif
# ifdef	TABDLY
	return ((termbuf.c_oflag & TABDLY) == TAB3);
# endif
#endif
}

tty_setsofttab(on)
int on;
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.sg.sg_flags |= XTABS;
	else
		termbuf.sg.sg_flags &= ~XTABS;
#else
	if (on) {
# ifdef	OXTABS
		termbuf.c_oflag |= OXTABS;
# endif
# ifdef	TABDLY
		termbuf.c_oflag &= ~TABDLY;
		termbuf.c_oflag |= TAB3;
# endif
	} else {
# ifdef	OXTABS
		termbuf.c_oflag &= ~OXTABS;
# endif
# ifdef	TABDLY
		termbuf.c_oflag &= ~TABDLY;
		termbuf.c_oflag |= TAB0;
# endif
	}
#endif
}

tty_islitecho()
{
#ifndef	USE_TERMIO
	return (!(termbuf.sg.sg_flags & CTLECH));
#else
# ifdef	ECHOCTL
	return (!(termbuf.c_lflag & ECHOCTL));
# endif
# ifdef	TCTLECH
	return (!(termbuf.c_lflag & TCTLECH));
# endif
#if !defined(ECHOCTL) && !defined(TCTLECH)
	return (0);	/* assumes ctl chars are echoed '^x' */
# endif
#endif
}

tty_setlitecho(on)
int on;
{
#ifndef	USE_TERMIO
	if (on)
		termbuf.sg.sg_flags &= ~CTLECH;
	else
		termbuf.sg.sg_flags |= CTLECH;
#else
# ifdef	ECHOCTL
	if (on)
		termbuf.c_lflag &= ~ECHOCTL;
	else
		termbuf.c_lflag |= ECHOCTL;
# endif
# ifdef	TCTLECH
	if (on)
		termbuf.c_lflag &= ~TCTLECH;
	else
		termbuf.c_lflag |= TCTLECH;
# endif
#endif
}

/*
 * A table of available terminal speeds
 */
struct termspeeds {
	int	speed;
	int	value;
} termspeeds[] = {
	{ 0,     B0 },    { 50,    B50 },   { 75,    B75 },
	{ 110,   B110 },  { 134,   B134 },  { 150,   B150 },
	{ 200,   B200 },  { 300,   B300 },  { 600,   B600 },
	{ 1200,  B1200 }, { 1800,  B1800 }, { 2400,  B2400 },
	{ 4800,  B4800 }, { 9600,  B9600 }, { 19200, B9600 },
	{ 38400, B9600 }, { -1,    B9600 }
};

tty_tspeed(val)
{
	register struct termspeeds *tp;

	for (tp = termspeeds; (tp->speed != -1) && (val > tp->speed); tp++)
		;
	cfsetospeed(&termbuf, tp->value);
}

tty_rspeed(val)
{
	register struct termspeeds *tp;

	for (tp = termspeeds; (tp->speed != -1) && (val > tp->speed); tp++)
		;
	cfsetispeed(&termbuf, tp->value);
}



/*
 * getptyslave()
 *
 * Open the slave side of the pty, and do any initialization
 * that is necessary.  The return value is a file descriptor
 * for the slave side.
 */
getptyslave()
{
	register int t = -1;

# ifdef	LINEMODE
	/*
	 * Opening the slave side may cause initilization of the
	 * kernel tty structure.  We need remember whether or not
	 * linemode was turned on, so that we can re-set it if we
	 * need to.
	 */
	int waslm = tty_linemode();
# endif


	/*
	 * Make sure that we don't have a controlling tty, and
	 * that we are the session (process group) leader.
	 */
# ifdef	TIOCNOTTY
	t = open(_PATH_TTY, O_RDWR);
	if (t >= 0) {
		(void) ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	}
# endif



	t = cleanopen(line);
	if (t < 0)
		fatalperror(net, line);

	/*
	 * set up the tty modes as we like them to be.
	 */
	init_termbuf();
# ifdef	LINEMODE
	if (waslm)
		tty_setlinemode();
# endif	/* LINEMODE */

	/*
	 * Settings for sgtty based systems
	 */
# ifndef	USE_TERMIO
	termbuf.sg.sg_flags |= CRMOD|ANYP|ECHO|XTABS;
	termbuf.sg.sg_ospeed = termbuf.sg.sg_ispeed = B9600;
# endif	/* USE_TERMIO */


	/*
	 * Settings for all other termios/termio based
	 * systems, other than 4.4BSD.  In 4.4BSD the
	 * kernel does the initial terminal setup.
	 */
#if defined(USE_TERMIO) && (BSD <= 43)
#  ifndef	OXTABS
#   define OXTABS	0
#  endif
	termbuf.c_lflag |= ECHO;
	termbuf.c_oflag |= ONLCR|OXTABS;
	termbuf.c_iflag |= ICRNL;
	termbuf.c_iflag &= ~IXOFF;
	cfsetospeed(&termbuf, B9600);
	cfsetispeed(&termbuf, B9600);
# endif /* defined(USE_TERMIO) && !defined(CRAY) && (BSD <= 43) */

	/*
	 * Set the tty modes, and make this our controlling tty.
	 */
	set_termbuf();
	if (login_tty(t) == -1)
		fatalperror(net, "login_tty");
	if (net > 2)
		(void) close(net);
	if (pty > 2)
		(void) close(pty);
}

#ifndef	O_NOCTTY
#define	O_NOCTTY	0
#endif
/*
 * Open the specified slave side of the pty,
 * making sure that we have a clean tty.
 */
cleanopen(line)
char *line;
{
	register int t;
	cap_t ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	cap_value_t cap_fowner = CAP_FOWNER;
	cap_value_t cap_dac_override[] = {CAP_DAC_READ_SEARCH, CAP_DAC_WRITE};


	/*
	 * Make sure that other people can't open the
	 * slave side of the connection.
	 */
	ocap = cap_acquire(1, &cap_fowner);
	(void) chown(line, 0, 0);
	cap_surrender(ocap);
#ifdef sgi

	t = open(line, O_RDWR);
	if (t < 0 || fchmod(t, 0))
		return(-1);

	/*
	 * Hangup anybody else using this ttyp, then reopen it for
	 * ourselves.
	 */
	(void) signal(SIGHUP, SIG_IGN);
	ocap = cap_acquire(1, &cap_device_mgt);
	vhangup();
	cap_surrender(ocap);
	(void) signal(SIGHUP, SIG_DFL);
	setpgrp();
	ocap = cap_acquire(2, cap_dac_override);
	t = open(line, O_RDWR);
	cap_surrender(ocap);
	if (t < 0)
		return(-1);
	/*
	 * Make the pty world-writable so programs like write & talk work.
	 * XXX should be protected, with "approved" setgid programs allowed
	 * to write to ptys.
	 */
	if (fchmod(t, 0622))
		return(-1);
#else
	(void) chmod(line, 0600);

#if (BSD > 43)
	(void) revoke(line);
# endif
	t = open(line, O_RDWR|O_NOCTTY);
	if (t < 0)
		return(-1);

	/*
	 * Hangup anybody else using this ttyp, then reopen it for
	 * ourselves.
	 */
#if (BSD <= 43)
	(void) signal(SIGHUP, SIG_IGN);
	vhangup();
	(void) signal(SIGHUP, SIG_DFL);
	t = open(line, O_RDWR|O_NOCTTY);
	if (t < 0)
		return(-1);
# endif
#endif /* sgi */
	return(t);
}

#if BSD <= 43
login_tty(t)
int t;
{
#ifndef sgi
# ifndef NO_SETSID
	if (setsid() < 0)
		fatalperror(net, "setsid()");
# else
	if (setpgrp(0,0) < 0)
		fatalperror(net, "setpgrp()");
# endif
#endif
# ifdef	TIOCSCTTY
	if (ioctl(t, TIOCSCTTY, (char *)0) < 0)
		fatalperror(net, "ioctl(sctty)");
# else
	close(open(line, O_RDWR));
# endif
	(void) dup2(t, 0);
	(void) dup2(t, 1);
	(void) dup2(t, 2);
#ifdef sgi
	adut();
	closelog();			/* (so syslog() can work below) */
	for (t = getdtablehi(); --t > 2; )
		(void)close(t);
	return 0;
#else
	close(t);
#endif
}
#endif	/* BSD <= 43 */


/*
 * startslave(host)
 *
 * Given a hostname, do whatever
 * is necessary to startup the login process on the slave side of the pty.
 */

/* ARGSUSED */
startslave(host)
char *host;
{
	register int i;
#ifndef sgi
	long time();
#endif


	if ((i = fork()) < 0)
		fatalperror(net, "fork");
	if (i) {
	} else {
		getptyslave();
		start_login(host);
		/*NOTREACHED*/
	}
}

char	*envinit[3];
extern char **environ;

init_env()
{
#ifdef sgi
	extern char *telnet_getenv();
#else
	extern char *getenv();
#endif
	char **envp;

	envp = envinit;
#ifdef sgi
	if (*envp = telnet_getenv("TZ"))
#else
	if (*envp = getenv("TZ"))
#endif
		*envp++ -= 3;
	*envp = 0;
	environ = envinit;
}



/*
 * start_login(host)
 *
 * Assuming that we are now running as a child processes, this
 * function will turn us into the login process.
 */

start_login(host)
char *host;
{
#ifndef sgi
	register char *cp;
#endif
	register char **argv;
	char **addarg();
#ifdef sgi
	extern char *telnet_getenv();
#endif

	/*
	 * -h : pass on name of host.
	 *		WARNING:  -h is accepted by login if and only if
	 *			getuid() == 0.
	 * -p : don't clobber the environment (so terminal type stays set).
	 */
	argv = addarg(0, "login");
	argv = addarg(argv, "-h");
	argv = addarg(argv, host);
#ifndef NO_LOGIN_P
	argv = addarg(argv, "-p");
#endif
#ifdef	BFTPDAEMON
	/*
	 * Are we working as the bftp daemon?  If so, then ask login
	 * to start bftp instead of shell.
	 */
	if (bftpd) {
		argv = addarg(argv, "-e");
		argv = addarg(argv, BFTPPATH);
	} else 
#endif
#ifdef sgi
	if (telnet_getenv("USER")) {
		argv = addarg(argv, telnet_getenv("USER"));
#else
	if (getenv("USER")) {
		argv = addarg(argv, getenv("USER"));
#endif
	}

#ifndef _IRIX4
	execv(_PATH_SCHEME, argv);
	syslog(LOG_ERR, "%s: %m\n", _PATH_SCHEME);
#endif /* !_IRIX4 */

	execv(_PATH_LOGIN, argv);
	syslog(LOG_ERR, "%s: %m\n", _PATH_LOGIN);
	fatalperror(net, _PATH_LOGIN);
	/*NOTREACHED*/
}

char **
addarg(argv, val)
register char **argv;
register char *val;
{
	register char **cpp;
	char *malloc();

	if (argv == NULL) {
		/*
		 * 10 entries, a leading length, and a null
		 */
		argv = (char **)malloc(sizeof(*argv) * 12);
		if (argv == NULL)
			return(NULL);
		*argv++ = (char *)10;
		*argv = (char *)0;
	}
	for (cpp = argv; *cpp; cpp++)
		;
	if (cpp == &argv[(int)argv[-1]]) {
		--argv;
		*argv = (char *)((int)(*argv) + 10);
		argv = (char **)realloc(argv, (int)(*argv) + 2);
		if (argv == NULL)
			return(NULL);
		argv++;
		cpp = &argv[(int)argv[-1] - 10];
	}
	*cpp++ = val;
	*cpp = 0;
	return(argv);
}

/*
 * cleanup()
 *
 * This is the routine to call when we are all through, to
 * clean up anything that needs to be cleaned up.
 */
void
cleanup()
{

#ifdef sgi
	rmut();
#else
#if (BSD > 43)
	char *p;

	p = line + sizeof("/dev/") - 1;
	if (logout(p))
		logwtmp(p, "", "");
	(void)chmod(line, 0666);
	(void)chown(line, 0, 0);
	*p = 'p';
	(void)chmod(line, 0666);
	(void)chown(line, 0, 0);
# else
	rmut();
	vhangup();	/* XXX */
# endif
#endif /* sgi */
	(void) shutdown(net, 2);
	exit(1);
}


/*
 * rmut()
 *
 * This is the function called by cleanup() to
 * remove the utmp entry for this person.
 */

#if BSD <= 43 && !defined(sgi)
rmut()
{
	register f;
	int found = 0;
	struct utmp *u, *utmp;
	int nutmp;
	struct stat statbf;
	char *malloc();
	long time();
	off_t lseek();

	f = open(utmpf, O_RDWR);
	if (f >= 0) {
		(void) fstat(f, &statbf);
		utmp = (struct utmp *)malloc((unsigned)statbf.st_size);
		if (!utmp)
			syslog(LOG_ERR, "utmp malloc failed");
		if (statbf.st_size && utmp) {
			nutmp = read(f, (char *)utmp, (int)statbf.st_size);
			nutmp /= sizeof(struct utmp);
		
			for (u = utmp ; u < &utmp[nutmp] ; u++) {
				if (SCMPN(u->ut_line, line+5) ||
				    u->ut_name[0]==0)
					continue;
				(void) lseek(f, ((long)u)-((long)utmp), L_SET);
				SCPYN(u->ut_name, "");
				SCPYN(u->ut_host, "");
				(void) time(&u->ut_time);
				(void) write(f, (char *)u, sizeof(wtmp));
				found++;
			}
		}
		(void) close(f);
	}
	if (found) {
		f = open(wtmpf, O_WRONLY|O_APPEND);
		if (f >= 0) {
			SCPYN(wtmp.ut_line, line+5);
			SCPYN(wtmp.ut_name, "");
			SCPYN(wtmp.ut_host, "");
			(void) time(&wtmp.ut_time);
			(void) write(f, (char *)&wtmp, sizeof(wtmp));
			(void) close(f);
		}
	}
	(void) chmod(line, 0666);
	(void) chown(line, 0, 0);
	line[strlen("/dev/")] = 'p';
	(void) chmod(line, 0666);
	(void) chown(line, 0, 0);
}  /* end of rmut */
#endif	/* CRAY */


/* add ourself to utmp */
adut()
{
	int f;
	cap_t ocap;
	cap_value_t cap_dac_write = CAP_DAC_WRITE;
	SCPYN(entry.ut_user, "telnet");
	SCPYN(entry.ut_id, &line[(sizeof("/dev/tty") - 1)]);
	SCPYN(entry.ut_line, line+sizeof("/dev/")-1);
	entry.ut_pid = getpid();
	entry.ut_type = LOGIN_PROCESS;
#ifdef _IRIX4
	entry.ut_time = time(0);
	setutent();
	(void)pututline(&entry);
	endutent();
#ifdef _XUTMP_FILE
	_xsetutent();		/* login adds host */
	_xutupdate(&entry, NULL, NULL, 0, NULL); 
	_xendutent();
#endif
	f = open(WTMP_FILE, O_WRONLY|O_APPEND);
	if (f >= 0) {
		write(f, (char *)&entry, sizeof(entry));
		close(f);
	}
#else /* !_IRIX4 */
	(void) time(&entry.ut_tv.tv_sec);

	setutxent();
	ocap = cap_acquire(1, &cap_dac_write);
	pututxline(&entry); /* updates utmpx and utmp */
	cap_surrender(ocap);
	endutxent();

	ocap = cap_acquire(1, &cap_dac_write);
	updwtmpx(WTMPX_FILE, &entry);
	cap_surrender(ocap);
#endif /* _IRIX4 */
}

rmut()
{
	int f;
	cap_t ocap;
	cap_value_t cap_dac_write = CAP_DAC_WRITE;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	static char satid[5], satline[33];
	
	SCPYN(entry.ut_user, "telnet");
	SCPYN(entry.ut_id, &line[(sizeof("/dev/tty") - 1)]);
	SCPYN(entry.ut_line, line+sizeof("/dev/")-1);
	entry.ut_pid = getpid();
	entry.ut_type = DEAD_PROCESS;
#ifdef _IRIX4
	entry.ut_time = time(0);
	setutent();
	(void)pututline(&entry);
	endutent();
#ifdef _XUTMP_FILE
	_xsetutent();
	_xutupdate(&entry, NULL, NULL, 1, NULL);
	_xendutent();
#endif
	f = open(WTMP_FILE, O_WRONLY|O_APPEND);
	if (f >= 0) {
		entry.ut_user[0] = '\0';
		write(f, (char *)&entry, sizeof(entry));
		close(f);
	}
#else /* !_IRIX4 */
	(void) time(&entry.ut_tv.tv_sec);

	setutxent();
	ocap = cap_acquire(1, &cap_dac_write);
	pututxline(&entry);
	cap_surrender(ocap);
	endutxent();

	ocap = cap_acquire(1, &cap_dac_write);
	updwtmpx(WTMPX_FILE, &entry);
	cap_surrender(ocap);
#endif /* _IRIX4 */
	strncpy(satid, entry.ut_id, sizeof(entry.ut_id));
	strncpy(satline, line, strlen(line));
	ocap = cap_acquire(1, &cap_audit_write);
	satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
		  "telnetd|+|?|Remote telnet logout id %s line %s",
		   satid, satline);
	cap_surrender(ocap);
}
