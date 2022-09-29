/* terminal I/O definitions
 *	some System V, some BSD.
 *
 */


#ifndef _SYS_TERMIO_H
#define _SYS_TERMIO_H

#include <standards.h>
#include <sys/termios.h>

/* all the ioctl codes and flags are now in termios.h */

/* LDISC1 default control chars 
 *
 * Note that these are identical to definitions found in sys/termios.h
 * If these change, you need to change them there too.
 *
 * XXXrs why are these defined twice anyway?
 */
#define	CLNEXT	CTRL('v')
#define	CWERASE	CTRL('w')
#define	CFLUSHO	CTRL('o')
#define	CFLUSH	CFLUSHO		/* back compatibility */
#define	CRPRNT	CTRL('r')
#define	CDSUSP	CTRL('y')	/* delayed job-control (not implemented yet) */

#define __OLD_SSPEED __OLD_B9600
#define __NEW_SSPEED __NEW_B9600
#define	SSPEED	B9600

/*
 * Ioctl control packet
 * We retain both versions so that the kernel can use both,
 * e.g. when converting from one to the other.
 */
struct termio {
	tcflag_t	c_iflag;	/* input modes */
	tcflag_t	c_oflag;	/* output modes */
	tcflag_t	c_cflag;	/* control modes */
	tcflag_t	c_lflag;	/* line discipline modes */
#if !defined(_OLD_TERMIOS) && _NO_ABIAPI
	speed_t		c_ospeed;	/* output speed */
	speed_t		c_ispeed;	/* input speed - not supported */
#endif
	char		c_line;		/* line discipline */
	cc_t		c_cc[NCCS];	/* control chars */
};

struct __new_termio {
	tcflag_t	c_iflag;	/* input modes */
	tcflag_t	c_oflag;	/* output modes */
	tcflag_t	c_cflag;	/* control modes */
	tcflag_t	c_lflag;	/* line discipline modes */
	speed_t		c_ospeed;	/* output speed */
	speed_t		c_ispeed;	/* input speed - not supported */
	char		c_line;		/* line discipline */
	cc_t		c_cc[NCCS];	/* control chars */
};

struct __old_termio {
	tcflag_t	c_iflag;	/* input modes */
	tcflag_t	c_oflag;	/* output modes */
	tcflag_t	c_cflag;	/* control modes */
	tcflag_t	c_lflag;	/* line discipline modes */
	char		c_line;		/* line discipline */
	cc_t		c_cc[NCCS];	/* control chars */
};

/*
 * Terminal types
 */
#define	TERM_NONE	0	/* tty */
#define	TERM_TEC	1	/* TEC Scope */
#define	TERM_V61	2	/* DEC VT61 */
#define	TERM_V10	3	/* DEC VT100 */
#define	TERM_TEX	4	/* Tektronix 4023 */
#define	TERM_D40	5	/* TTY Mod 40/1 */
#define	TERM_H45	6	/* Hewlitt-Packard 45 */
#define	TERM_D42	7	/* TTY Mod 40/2B */

/*
 * Terminal flags
 */
#define TM_NONE		0000	/* use default flags */
#define TM_SNL		0001	/* special newline flag */
#define TM_ANL		0002	/* auto newline on column 80 */
#define TM_LCF		0004	/* last col of last row special */
#define TM_CECHO	0010	/* echo terminal cursor control */
#define TM_CINVIS	0020	/* do not send esc seq to user */
#define TM_SET		0200	/* must be on to set/res flags */

/*
 * structure of ioctl arg for LDGETT and LDSETT
 *	This structure is no longer used.
 */
#undef st_flgs
#undef st_termt
#undef st_crow
#undef st_ccol
#undef st_vrow
#undef st_lrow

struct	termcb	{
	char	st_flgs;	/* term flags */
	char	st_termt;	/* term type */
	char	st_crow;	/* gtty only - current row */
	char	st_ccol;	/* gtty only - current col */
	char	st_vrow;	/* variable row */
	char	st_lrow;	/* last row */
};
#endif
