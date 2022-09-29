/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * $Revision: 1.28 $
 */

#ifndef _SYS_TERMIOS_H
#define _SYS_TERMIOS_H

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>
#include <sys/ttydev.h>
#include <sys/types.h>

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0 /* Disable special character functions */
#endif

#if (_NO_POSIX && _NO_XOPEN4) || _ABIAPI
#define	CTRL(c)	((c)&037)
#define IBSHIFT 16

/* required by termio.h and VCEOF/VCEOL */
#define	NCC	8
#endif /* (_NO_POSIX && _NO_XOPEN4) || _ABIAPI  */

#define NCCS	23

/*
 * types defined by POSIX. These are better off in types.h, but 
 * the standard says that they have to be in termios.h.
 */
#if (_MIPS_SZLONG == 32)
typedef unsigned long tcflag_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef __uint32_t tcflag_t;
#endif

typedef unsigned char cc_t;

#if (_MIPS_SZLONG == 32)
typedef unsigned long speed_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef __uint32_t speed_t;
#endif

/*
 * Ioctl control packet
 * We retain both versions so that the kernel can use both,
 * e.g. when converting from one to the other.
 */
struct termios {
	tcflag_t	c_iflag;	/* input modes */
	tcflag_t	c_oflag;	/* output modes */
	tcflag_t	c_cflag;	/* control modes */
	tcflag_t	c_lflag;	/* line discipline modes */
#if !defined(_OLD_TERMIOS) && _NO_ABIAPI
	speed_t		c_ospeed;	/* output speed */
	speed_t		c_ispeed;	/* input speed - not supported */
#endif
	cc_t		c_cc[NCCS];	/* control chars */
};

struct __new_termios {
	tcflag_t	c_iflag;	/* input modes */
	tcflag_t	c_oflag;	/* output modes */
	tcflag_t	c_cflag;	/* control modes */
	tcflag_t	c_lflag;	/* line discipline modes */
	speed_t		c_ospeed;	/* output speed */
	speed_t		c_ispeed;	/* input speed - not supported */
	cc_t		c_cc[NCCS];	/* control chars */
};

struct __old_termios {
	tcflag_t	c_iflag;	/* input modes */
	tcflag_t	c_oflag;	/* output modes */
	tcflag_t	c_cflag;	/* control modes */
	tcflag_t	c_lflag;	/* line discipline modes */
	cc_t		c_cc[NCCS];	/* control chars */
};

#define __NEW_MAX_BAUD 500000

/* 
 * POSIX termios functions
 * These functions get mapped into ioctls.
 * Both the old and new versions exist in libc.  Old apps
 * that haven't been recompiled, and apps that are
 * recompiled and define _OLD_TERMIOS, use the
 * old routines.  Recompiled apps use the new routines by default
 * (except MIPS ABI).
 */

#ifndef _KERNEL

#if !defined(_OLD_TERMIOS) && _NO_ABIAPI

extern speed_t __new_cfgetospeed (const struct termios *);
extern int __new_cfsetospeed (struct termios *, speed_t);
extern speed_t __new_cfgetispeed (const struct termios *);
extern int __new_cfsetispeed (struct termios *, speed_t);
extern int __new_tcgetattr (int, struct termios *);
extern int __new_tcsetattr (int, int, const struct termios *);

/*REFERENCED*/
static speed_t cfgetospeed (const struct termios *__t) { 
    return __new_cfgetospeed(__t);
}
/*REFERENCED*/
static int cfsetospeed (struct termios *__t, speed_t __s) { 
    return __new_cfsetospeed(__t,__s);
}
/*REFERENCED*/
static speed_t cfgetispeed (const struct termios *__t) { 
    return __new_cfgetispeed(__t);
}
/*REFERENCED*/
static int cfsetispeed (struct termios *__t, speed_t __s) { 
    return __new_cfsetispeed(__t,__s);
}
/*REFERENCED*/
static int tcgetattr (int __fd, struct termios *__t) { 
    return __new_tcgetattr(__fd,__t);
}
/*REFERENCED*/
static int tcsetattr (int __fd, int __act, const struct termios *__t) { 
    return __new_tcsetattr(__fd, __act, __t);
}

#else /* _OLD_TERMIOS || _ABIAPI */

extern speed_t cfgetospeed (const struct termios *);
extern int cfsetospeed (struct termios *, speed_t);
extern speed_t cfgetispeed (const struct termios *);
extern int cfsetispeed (struct termios *, speed_t);
extern int tcgetattr (int, struct termios *);
extern int tcsetattr (int, int, const struct termios *);

#endif /* _OLD_TERMIOS || _ABIAPI */

extern int tcsendbreak (int, int);
extern int tcdrain (int);
extern int tcflush (int, int);
extern int tcflow (int, int);


#if _XOPEN4UX
extern pid_t tcgetsid (int);
#endif

#endif /* !defined(_KERNEL) */

/* === c_cc array indices === */

#define	VINTR	0
#define	VQUIT	1
#define	VERASE	2
#define	VKILL	3
#define	VEOF	4
#define	VEOL	5
#define	VEOL2	6
#define	VMIN	4
#define	VTIME	5
#define	VSWTCH	7
#define VSTART	8
#define VSTOP	9
#define	VSUSP   10
#define	VDSUSP	 11
#define	VREPRINT 12
#define	VDISCARD 13
#define VWERASE 14
#define VLNEXT	15
#define VRPRNT	VREPRINT
#define VFLUSHO	VDISCARD
/* 16 thru 19 reserved for future use */

/*
 * control characters from Xenix termio.h
 */
#if (_NO_POSIX && _NO_XOPEN4) || _ABIAPI
#define	VCEOF	NCC		/* RESERVED true EOF char (V7 compatability) */
#define	VCEOL	(NCC + 1)	/* RESERVED true EOL char */

/* === control characters === */

#define	CNUL	0
#define	CDEL	0377

/* S5 default control chars */
#define	CESC	'\\'		/* LDISC0 'literal next' default */
#define	CINTR	0177		/* DEL */
#define	CQUIT	034		/* FS, cntl | */
#define	CERASE	CTRL('H')	/* XXXrs should be '#' ??!!?? */
#define	CKILL	CTRL('U')	/* XXXrs should be '@' ??!!?? */
#define CEOL	0
#define CEOL2	0
#define	CEOF	CTRL('d')	/* cntl d */
#define CEOT	CEOF		/* 4.3BSD compatibility */
#define	CSTART	CTRL('q')	/* cntl q */
#define	CSTOP	CTRL('s')	/* cntl s */
#define	CSWTCH	CTRL('z')	/* cntl z */
#define	CNSWTCH	0
#define	CSUSP	CSWTCH          /* 4.3BSD job-control signal */

/* SGI  LDISC1 default control chars */
#define	CLNEXT	CTRL('v')
#define	CWERASE	CTRL('w')
#define	CFLUSHO	CTRL('o')
#define	CFLUSH	CFLUSHO		/* back compatibility */
#define	CRPRNT	CTRL('r')
#define	CDSUSP	CTRL('y')	/* delayed job-control */
#define CBRK	0377		/* SGI 4.3BSD compatibility */

#endif /* (_NO_POSIX && _NO_XOPEN4) || _ABIAPI */


/* === input modes (c_iflag) === */

#define	IGNBRK	0000001
#define	BRKINT	0000002
#define	IGNPAR	0000004
#define	PARMRK	0000010
#define	INPCK	0000020
#define	ISTRIP	0000040
#define	INLCR	0000100
#define	IGNCR	0000200
#define	ICRNL	0000400
#define	IUCLC	0001000
#define	IXON	0002000
#define	IXANY	0004000
#define	IXOFF	0010000
#define IMAXBEL 0020000
#define	IBLKMD	0040000	

/* === output modes (c_oflag) === */

#define	OPOST	0000001
#if _XOPEN4
#define	OLCUC	0000002
#define	ONLCR	0000004
#define	OCRNL	0000010
#define	ONOCR	0000020
#define	ONLRET	0000040
#define	OFILL	0000100
#define	OFDEL	0000200
#define	NLDLY	0000400
#define	NL0	0
#define	NL1	0000400
#define	CRDLY	0003000
#define	CR0	0
#define	CR1	0001000
#define	CR2	0002000
#define	CR3	0003000
#define	TABDLY	0014000
#define	TAB0	0
#define	TAB1	0004000
#define	TAB2	0010000
#define	TAB3	0014000
#endif /* _XOPEN4 */
#if (_NO_POSIX && _NO_XOPEN4) || _ABIAPI
#define XTABS	0014000
#endif /* _NO_POSIX && _NO_XOPEN4 || _ABIAPI */
#if _XOPEN4
#define	BSDLY	0020000
#define	BS0	0
#define	BS1	0020000
#define	VTDLY	0040000
#define	VT0	0
#define	VT1	0040000
#define	FFDLY	0100000
#define	FF0	0
#define	FF1	0100000
#endif /* _XOPEN4 */
#if (_NO_POSIX && _NO_XOPEN4) || _ABIAPI
#define PAGEOUT 0200000
#define WRAP	0400000

/* === control modes (c_cflag) === */

#define	CBAUD		000000017
#endif /* (_NO_POSIX && _NO_XOPEN4) || _ABIAPI */
#define	CSIZE		000000060
#define	CS5		0
#define	CS6		000000020
#define	CS7		000000040
#define	CS8		000000060
#define	CSTOPB		000000100
#define	CREAD		000000200
#define	PARENB		000000400
#define	PARODD		000001000
#define	HUPCL		000002000
#define	CLOCAL		000004000
#if (_NO_POSIX && _NO_XOPEN4) || _ABIAPI
#define RCV1EN		000010000
#define XMT1EN		000020000
#define	LOBLK		000040000
#define	XCLUDE		000100000 /* *V7* exclusive use coming fron XENIX */
#define CIBAUD		003600000
#define PAREXT		004000000
#define CNEW_RTSCTS	010000000 /* RiscOS API compliance. */
#endif /* (_NO_POSIX && _NO_XOPEN4) || _ABIAPI */

/* === line discipline 0 (a.k.a. local) modes (c_lflag) === */

#define	ISIG	0000001
#define	ICANON	0000002
#if _XOPEN4 
#define	XCASE	0000004
#endif /* _XOPEN4 */
#define	ECHO	0000010
#define	ECHOE	0000020
#define	ECHOK	0000040
#define	ECHONL	0000100
#define	NOFLSH	0000200
#define	IEXTEN	0000400  	/* POSIX flag - enable POSIX extensions */
#define ITOSTOP  0100000
#define TOSTOP  ITOSTOP

#if (_NO_POSIX && _NO_XOPEN4) || _ABIAPI
#define	ECHOCTL	0001000
#define	ECHOPRT	0002000
#define	ECHOKE	0004000
#define	DEFECHO	0010000
#define	FLUSHO	0020000
#define	PENDIN	0040000


#define	TIOC	('T'<<8)

#define	__NEW_TCGETA	(TIOC|201)
#define	__NEW_TCSETA	(TIOC|202)
#define	__NEW_TCSETAW	(TIOC|203)
#define	__NEW_TCSETAF	(TIOC|204)
#define	__OLD_TCGETA	(TIOC|1)
#define	__OLD_TCSETA	(TIOC|2)
#define	__OLD_TCSETAW	(TIOC|3)
#define	__OLD_TCSETAF	(TIOC|4)

#if !defined(_OLD_TERMIOS) && _NO_ABIAPI
#define	TCGETA	__NEW_TCGETA
#define	TCSETA	__NEW_TCSETA
#define	TCSETAW __NEW_TCSETAW
#define	TCSETAF __NEW_TCSETAF
#else /* _OLD_TERMIOS || _ABIAPI */
#define	TCGETA	__OLD_TCGETA
#define	TCSETA	__OLD_TCSETA
#define	TCSETAW __OLD_TCSETAW
#define	TCSETAF __OLD_TCSETAF
#endif /* _OLD_TERMIOS || _ABIAPI */

#define	TCSBRK	(TIOC|5)
#define	TCXONC	(TIOC|6)
#define	TCFLSH	(TIOC|7)

/* for SGI compatibility  */

# include <sys/ioctl.h>
/*
 * Line Disciplines
 */
#define LDISC0	0			/* ancient, standard */
#define LDISC1	1			/* new, 4.3BSD-like in streams */
#define NTTYDISC LDISC1			/* compatibility for BSD programs */


#define TIOCFLUSH	(TIOC|12)
#define TCSETLABEL	(TIOC|31)	/* set device label */
#define	TCDSET		(TIOC|32)
#define	TCBLKMD		(TIOC|33)
#define	TIOCPKT		(TIOC|112)	/* pty: set/clear packet mode */
#define	TIOCPKT_DATA		0x00	/* data packet */
#define	TIOCPKT_FLUSHREAD	0x01	/* flush packet */
#define	TIOCPKT_FLUSHWRITE	0x02	/* flush packet */
#define	TIOCPKT_NOSTOP		0x10	/* no more ^S, ^Q */
#define	TIOCPKT_DOSTOP		0x20	/* now do ^S ^Q */
#define	TIOCPKT_IOCTL		0x40	/* state change of pty driver */
#define TIOCNOTTY (TIOC|113)		/* disconnect from tty & pgrp */
#define TIOCSTI	(TIOC|114)		/* simulate terminal input */
#define	TIOCSPGRP	_IOW('t', 118, int)	/* set pgrp of tty */
#define	TIOCGPGRP	_IOR('t', 119, int)	/* get pgrp of tty */
#define	TIOCCONS	_IOW('t', 120, int)	/* make this the console */
#define TIOCGWINSZ	_IOR('t', 104, struct winsize)	/* get window size */
#define TIOCSWINSZ	_IOW('t', 103, struct winsize)	/* set window size */
#define	TFIOC	('F'<<8)
#define oFIONREAD (TFIOC|127)		/* pre-3.5 value of FIONREAD */
#define TO_STOP LOBLK   		/* send SIGTTOU for background output */

#ifndef IOCTYPE
#define	IOCTYPE	0xff00
#endif


/* termios ioctls */

#define __NEW_TCGETS		(TIOC|213)
#define __NEW_TCSETS		(TIOC|214)
#define __NEW_TCSETSW		(TIOC|215)
#define	__NEW_TCSETSF		(TIOC|216)
#define __OLD_TCGETS		(TIOC|13)
#define __OLD_TCSETS		(TIOC|14)
#define __OLD_TCSETSW		(TIOC|15)
#define	__OLD_TCSETSF		(TIOC|16)
#if !defined(_OLD_TERMIOS) && _NO_ABIAPI
#define TCGETS		__NEW_TCGETS
#define TCSETS		__NEW_TCSETS
#define TCSETSW		__NEW_TCSETSW
#define	TCSETSF		__NEW_TCSETSF
#else /* _OLD_TERMIOS || _ABIAPI */
#define TCGETS		__OLD_TCGETS
#define TCSETS		__OLD_TCSETS
#define TCSETSW		__OLD_TCSETSW
#define	TCSETSF		__OLD_TCSETSF
#endif /* _OLD_TERMIOS || _ABIAPI */
#endif /* (_NO_POSIX && _NO_XOPEN4) || _ABIAPI */

#define TCSANOW		(('T'<<8)|14)	/* same as TCSETS  */
#define TCSADRAIN	(('T'<<8)|15)	/* same as TCSETSW */
#define	TCSAFLUSH	(('T'<<8)|16)	/* same as TCSETSF */

/* termios option flags */

#define TCIFLUSH	0  /* flush data received but not read */
#define TCOFLUSH	1  /* flush data written but not transmitted */
#define TCIOFLUSH	2  /* flush both data both input and output queues */	

#define TCOOFF		0  /* suspend output */
#define TCOON		1  /* restart suspended output */
#define TCIOFF		2  /* suspend input */
#define TCION		3  /* restart suspended input */

/* TIOC ioctls for BSD, ptys, job control and modem control */

#if _NO_POSIX && _NO_XOPEN4
#define	tIOC	('t'<<8)
#endif /* _NO_POSIX && _NO_XOPEN4 */


/* Slots for 386/XENIX compatibility */
/* BSD includes these ioctls in ttold.h */

#ifndef _SYS_TTOLD_H

#if _NO_POSIX && _NO_XOPEN4
#define TIOCGETD	(tIOC|0)
#define TIOCSETD	(tIOC|1)
#define TIOCHPCL	(tIOC|2)
#define TIOCGETP	(tIOC|8)
#define TIOCSETP  	(tIOC|9)
#define TIOCSETN	(tIOC|10)
#define TIOCEXCL	(tIOC|13)
#define TIOCNXCL	(tIOC|14)
#define TIOCSETC	(tIOC|17)
#define TIOCGETC	(tIOC|18)
/*
 * BSD ioctls that are not the same as XENIX are included here.
 * There are also some relevant ioctls from SUN/BSD sys/ttycom.h
 * BSD pty ioctls like TIOCPKT are not supported in SVR4.
 */

#define	TIOCLBIS	(tIOC|127)	/* bis local mode bits */
#define	TIOCLBIC	(tIOC|126)	/* bic local mode bits */
#define	TIOCLSET	(tIOC|125)	/* set entire local mode word */
#define	TIOCLGET	(tIOC|124)	/* get local modes */
#define	TIOCSBRK	(tIOC|123)	/* set break bit */
#define	TIOCCBRK	(tIOC|122)	/* clear break bit */
#define	TIOCSDTR	(tIOC|121)	/* set data terminal ready */
#define	TIOCCDTR	(tIOC|120)	/* clear data terminal ready */
#define	TIOCSLTC	(tIOC|117)	/* set local special chars */
#define	TIOCGLTC	(tIOC|116)	/* get local special chars */
#define	TIOCOUTQ	(tIOC|115)	/* driver output queue size */
#define	TIOCSTOP	(tIOC|111)	/* stop output, like ^S */
#define	TIOCSTART	(tIOC|110)	/* start output, like ^Q */
#endif /* _NO_POSIX && _NO_XOPEN4 */

#endif /* end _SYS_TTOLD_H */

/* POSIX job control ioctls */

#if _NO_POSIX && _NO_XOPEN4
#define	TIOCGSID	(tIOC|22)	/* get session id on ctty*/
#define	TIOCSSID	(tIOC|24)	/* set session id on ctty*/

/* Modem control */
#define	TIOCMSET	(tIOC|26)	/* set all modem bits */
#define	TIOCMBIS	(tIOC|27)	/* bis modem bits */
#define	TIOCMBIC	(tIOC|28)	/* bic modem bits */
#define	TIOCMGET	(tIOC|29)	/* get all modem bits */
#define		TIOCM_LE	0001		/* line enable */
#define		TIOCM_DTR	0002		/* data terminal ready */
#define		TIOCM_RTS	0004		/* request to send */
#define		TIOCM_ST	0010		/* secondary transmit */
#define		TIOCM_SR	0020		/* secondary receive */
#define		TIOCM_CTS	0040		/* clear to send */
#define		TIOCM_CAR	0100		/* carrier detect */
#define		TIOCM_CD	TIOCM_CAR
#define		TIOCM_RNG	0200		/* ring */
#define		TIOCM_RI	TIOCM_RNG
#define		TIOCM_DSR	0400		/* data set ready */

/* pseudo-tty */

#define	TIOCREMOTE	(tIOC|30)	/* remote input editing */
#ifdef __notdef__
/* this is not currently supported */
#define TIOCSIGNAL	(tIOC|31)	/* pty: send signal to slave */
#endif

/*
 * SVR4 compatability pty ioctl commands
 */
#define ISPTM		(('P'<<8)|1)	/* query for master */
#define UNLKPT		(('P'<<8)|2)	/* unlock master/slave pair */
/* IRIX extensions */
#define SVR4SOPEN	(('P'<<8)|100)	/* use SVR4 semantics on slave open */

/* Some more 386 xenix stuff */

#define	LDIOC	('D'<<8)

#define	LDOPEN	(LDIOC|0)
#define	LDCLOSE	(LDIOC|1)
#define	LDCHG	(LDIOC|2)
#define	LDGETT	(LDIOC|8)
#define	LDSETT	(LDIOC|9)

/* Slots for 386 compatibility */

#define	LDSMAP		(LDIOC|10)
#define	LDGMAP		(LDIOC|11)
#define	LDNMAP		(LDIOC|12)

/*
 * These are retained for 386/XENIX compatibility.
 */

#define	DIOC	('d'<<8)
#define DIOCGETP        (DIOC|8)                /* V7 */
#define DIOCSETP        (DIOC|9)                /* V7 */

/*
 * Returns a non-zero value if there
 * are characters in the input queue.
 */
#define FIORDCHK        (('f'<<8)|3)            /* V7 */
#endif /* _NO_POSIX && _NO_XOPEN4 */

#ifndef _SYS_TTOLD_H
#ifndef _SYS_PTEM_H

#if _NO_POSIX && _NO_XOPEN4
/* Windowing structure to support JWINSIZE/TIOCSWINSZ/TIOCGWINSZ */
struct winsize {
	unsigned short ws_row;       /* rows, in characters*/
	unsigned short ws_col;       /* columns, in character */
	unsigned short ws_xpixel;    /* horizontal size, pixels */
	unsigned short ws_ypixel;    /* vertical size, pixels */
};
#endif /* _NO_POSIX && _NO_XOPEN4 */

#endif /* end _SYS_PTEM_H */
#endif /* end _SYS_TTOLD_H */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_TERMIOS_H */
