#ident	"include/saioctl.h:  $Revision"

/*
 * saioctl.h -- standalone ioctl definitions
 */

/*
 * general ioctl's
 */
#define	FIOCNBLOCK	(('f'<<8)|1)	/* set non-blocking io */

/*
 * "tty" ioctl's
 */
#define	TIOCRAW		(('t'<<8)|1)	/* no special chars on char devices */
#define TIOCFLUSH	(('t'<<8)|2)
#define	TIOCREOPEN	(('t'<<8)|4)	/* reopen to effect baud rate chg */
#define	TIOCRRAW	(('t'<<8)|5)	/* no special chars on char devices */
					/* Unlike TIOCRAW in that NO special
					 * chars are interpreted at all.
					 * TIOCRAW interpretes ^C, ^S, and ^Q.
					 */
#define TIOCISGRAPHIC	(('t'<<8)|9)	/* return 0 iff graphical device */
#define TIOCINTRCHAR	(('t'<<8)|10)	/* set characters that cause intr */
#define TIOCSETXOFF	(('t'<<8)|11)	/* set value of xoff control */
#define TIOCISATTY	(('t'<<8)|12)	/* determine if descriptor is a tty */
#define TIOCCHECKSTOP	(('t'<<8)|13)	/* checks ^S/^Q */

/*
 * network ioctl's
 */
#define	NIOCBIND	(('n'<<8)|1)	/* bind network address */
#define	NIOCRESET	(('n'<<8)|2)	/* reset network interface */
#define	NIOCSIFADDR	(('n'<<8)|3)	/* set interface address */
#ifdef NETDBX
#define	NIOCREADANY	(('n'<<8)|4)	/* Read any mbuf off of socket */
#endif /* NETDBX */

/*
 * gfxgui icotls
 */
#define GIOCSETJMPBUF	(('g'<<8)|1)	/* set jump buffer for htp button */
#define GIOCSETBUTSTR	(('g'<<8)|2)	/* set htp button string */
		
#ifdef LANGUAGE_C

/* The definition of CTRL from termio.h doesn't work for
 * ansi-cpp,  so redefine it.
 */
#undef CTRL
#define	CTRL(x)		((x)&0x1f)	/* use CTRL('a') for ^A */

/* Global variables */
extern int Debug, Verbose, _udpcksum;

#ifndef NULL
#define NULL 0
#endif

#endif /* LANGUAGE_C */
