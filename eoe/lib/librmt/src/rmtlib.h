#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtlib.h,v 1.6 1992/08/02 20:54:30 ism Exp $"

/*
 *	This file is only included by the library routines.  It is not
 *	required for user code.
 *
 */


/*
** This version number should sync up with the rmt version number,
** RMT_VERSION, in cmd/rmt/rmt.c
** This is done so that during a open, we can decide if we are compatible
** enough with the remote protocol to run.
*/
#define	LIBRMT_VERSION	2
extern int server_version;

/*
 *	Note that local vs remote file descriptors are distinquished
 *	by adding a bias to the remote descriptors.  This is a quick
 *	and dirty trick that may not be portable to some systems.
 *  It should be greater than the largest open filedescriptor
 *  than can be returned by the OS, and should be a power of 2.
 */

#define REM_BIAS 8192


/*
 *	If ioctl calls are supported on remote device then define
 *	RMTIOCTL.
 */

#if BSD || sgi
#define RMTIOCTL
#endif

/*
 *	BUFMAGIC --- Magic buffer size
 *	MAXUNIT --- Maximum number of remote tape file units
 */

#define BUFMAGIC	64
#define MAXUNIT		4

/*
 *	Useful macros.
 *
 *	READ --- Return the number of the read side file descriptor
 *	WRITE --- Return the number of the write side file descriptor
 */

#define READ(fd)	(_rmt_Ctp[fd][0])
#define WRITE(fd)	(_rmt_Ptc[fd][1])

/*
 * System specific names
 */
#ifdef BSD
#define	RSH_PATH	"/usr/ucb/rsh"
#define	RMT_PATH	"/etc/rmt"
#endif

#ifdef sgi
#define	RSH_PATH	"/usr/bsd/rsh"
#define	RMT_PATH	"/etc/rmt"
#endif

extern int _rmt_Ctp[MAXUNIT][2];
extern int _rmt_Ptc[MAXUNIT][2];

void _rmt_abort(int);
