/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * This module implements libc interfaces which are changed by pthreads.
 *
 * Key points:
	* Blocking system calls interact with the scheduling module for
	  VP starvation (see libc) and the interrupt module for forced
	  wakeups (cancellation/signals).
	* Cancellation is done here to avoid introducing cancellation
	  points into unsuspecting code.
 */


/*
 * Fix varargs prototypes.
 */
#define open(path, flag, arg)		open(path, flag, mode_t)
#define fcntl(fd, cmd, arg)		fcntl(fd, cmd, void *)
#define dmi(first, elipsis)		_dmi(first, elipsis)
#include "common.h"
#include "intr.h"
#include "pt.h"
#include "sys.h"
#include "vp.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>


/* ------------------------------------------------------------------ */


/*
 * ptblocking()
 *
 * Perform blocking call prologue: process pending interrupts and mark
 * self as blocking so the library knows to wake us up.
 * Maintain VP so process does not starve.
 * Return flags to pass to unblock part.
 */
int
ptblocking(void)
{
	register pt_t	*pt_self = PT;

	pt_self->pt_blocked++;

	while (pt_self->pt_flags & PT_SIGNALLED) {
		(void)intr_check(PT_SIGNALLED);
	}

	if (!pt_is_bound(pt_self)) {
		VP_RESERVE();
	}

	return (0);
}


/*
 * ptunblocking(success, flags)
 *
 * Undo VP starvation code.  If we were interrupted then process any
 * pending interrupts before returning.
 * Return indicating whether call should be restarted.
 */
/* ARGSUSED */
int
ptunblocking(int interrupted, int flags)
{
	register pt_t	*pt_self = PT;

	if (!pt_is_bound(pt_self)) {		/* VP reserved */
		VP_RELEASE();
	}

	if (interrupted && (pt_self->pt_flags & PT_SIGNALLED)) {
		pt_self->pt_blocked--;
		return (intr_check(PT_SIGNALLED));
	}

	pt_self->pt_blocked--;
	return (0);	/* no retry required */
}


/* ------------------------------------------------------------------ */


/*
 * POSIX cancellation (blocking points).
 *
 * Specified by POSIX.1C as cancellation points.
 */

/* ------------------------------------------------------------------ */
#include <unistd.h>

/* Not a simple cancellation point
 * This is a callback from libc so that cancellation remains internal
 * to the pthread library.
 */
int _SGIPT_ptselect(int nfds, fd_set *readfds, fd_set *writefds,
		    fd_set *exceptfds, struct timeval *timeout)
{
	return (select(nfds, readfds, writefds, exceptfds, timeout));
}


/* ------------------------------------------------------------------ */
#define pread preadx
#define pwrite pwritex
#define _SGI_COMPILING_LIBC /* Needed to quiet pread/pwrite defines */

#include <unistd.h>
#undef pread
#undef pwrite
int close(int fildes)
{
	extern int _close(int);
	PT_CNCL_POINT( int, _close, (fildes) );
}


int fsync(int fildes)
{
	extern int _fsync(int);
	PT_CNCL_POINT( int, _fsync, (fildes) );
}


int pause(void)
{
	extern int _pause(void);
	PT_CNCL_POINT( int, _pause, () );
}


ssize_t read(int fildes, void *buf, size_t nbyte)
{
	extern ssize_t _read(int, void *, size_t);
	PT_CNCL_POINT( ssize_t, _read, (fildes, buf, nbyte) );
}


unsigned sleep(unsigned seconds)
{
	extern unsigned _sleep(unsigned);
	PT_CNCL_POINT_NON0( unsigned, _sleep, (seconds) );
}


ssize_t write(int fildes, const void *buf, size_t nbyte)
{
	extern ssize_t _write(int, const void *, size_t);
	PT_CNCL_POINT( ssize_t, _write, (fildes, buf, nbyte) );
}


#include <fcntl.h>

int (fcntl)(int fildes, int cmd, void *arg)
{
	extern int _fcntl(int, int, void *);
	PT_CNCL_POINT( int, _fcntl, (fildes, cmd, arg) );
}


#include <sys/types.h>
#include <sys/stat.h>

int creat(const char *path, mode_t mode)
{
	extern int _creat(const char *, mode_t);
	PT_CNCL_POINT( int, _creat, (path, mode) );
}


int (open)(const char *path, int oflag, mode_t mode)
{
	extern int _open(const char *, int, mode_t);
	PT_CNCL_POINT( int, _open, (path, oflag, mode) );
}


/* ------------------------------------------------------------------ */

#include <mqueue.h>

ssize_t mq_receive(mqd_t mqdes, char *msg, size_t len, unsigned int *prio)
{
	extern int _mq_receive(mqd_t mqdes, char *msg,
			       size_t len, unsigned int *prio);
	PT_CNCL_POINT( int, _mq_receive, (mqdes, msg, len, prio) );
}


int mq_send(mqd_t mqdes, const char *msg, size_t len, unsigned int prio)
{
	extern int _mq_send(mqd_t mqdes, const char *msg,
			    size_t len, unsigned int prio);
	PT_CNCL_POINT( int, _mq_send, (mqdes, msg, len, prio) );
}


/* ------------------------------------------------------------------ */

#include <sys/types.h>
#include <sys/mman.h>

int msync(void *addr, size_t len, int flags)
{
	extern int _msync(void *, size_t, int);
	PT_CNCL_POINT( int, _msync, (addr, len, flags) );
}


/* ------------------------------------------------------------------ */

#include <time.h>

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	extern int	_nanosleep(const struct timespec *, struct timespec *);
	PT_CNCL_POINT_NON0( int, _nanosleep, (rqtp, rmtp) );
}


/* ------------------------------------------------------------------ */

#include <termios.h>

int tcdrain(int fildes)
{
	extern int _ioctl(int , int , ...);
	PT_CNCL_POINT( int, _ioctl, (fildes, TCSBRK, 1) );
}


/* ------------------------------------------------------------------ */

#include <sys/types.h>
#include <sys/wait.h>

pid_t wait(int *statptr)
{
	extern pid_t	_wait(int *);
	PT_CNCL_POINT( pid_t, _wait, (statptr) );
}


pid_t waitpid(pid_t pid, int *statptr, int options)
{
	extern pid_t	_waitpid(pid_t, int *, int);
	PT_CNCL_POINT( pid_t, _waitpid, (pid, statptr, options) );
}


/* ------------------------------------------------------------------ */

#include <stdlib.h>

int system (const char *string)
{
	extern int _system (const char *);
	PT_CNCL_POINT( int, _system, (string) );
}


/* ------------------------------------------------------------------ */


/*
 * Non-POSIX cancellation / blocking points.
 */

/* ------------------------------------------------------------------ */

long sginap(long ticks)
{
	extern long	_sginap(long);

	if (!ticks) {
		ptsched_yield();
		return (0);
	}
	PT_CNCL_POINT_NON0( long, _sginap, (ticks) );
}


/* ------------------------------------------------------------------ */

#include <sys/ipc.h>
#include <sys/msg.h>

int msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	extern int _msgrcv(int, void *, size_t, long, int);
	PT_CNCL_POINT( int, _msgrcv, (msqid, msgp, msgsz, msgtyp, msgflg));
}

int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
	extern int _msgsnd(int, const void *, size_t, int);
	PT_CNCL_POINT( int, _msgsnd, (msqid, msgp, msgsz, msgflg) );
}



/* ------------------------------------------------------------------ */

#include <sys/ipc.h>
#include <sys/sem.h>

int semop(int semid, struct sembuf *sops, size_t nsops)
{
	extern int _semop(int, struct sembuf *, size_t);
	PT_CNCL_POINT( int, _semop, (semid, sops, nsops) );
}


/* ------------------------------------------------------------------ */

#include <stropts.h>
#include <poll.h>

int getmsg(int fd, struct strbuf *ctlptr, struct strbuf *dataptr, int *flagsp)
{
	extern int _getmsg(int, struct strbuf *, struct strbuf *, int *);
	PT_CNCL_POINT( int, _getmsg, (fd, ctlptr, dataptr, flagsp) );
}

int getpmsg(int fd, struct strbuf *ctlptr, struct strbuf *dataptr,
	int *bandp, int *flagsp)
{
	extern int _getpmsg(int, struct strbuf *, struct strbuf *, int *, int*);
	PT_CNCL_POINT( int, _getpmsg, (fd, ctlptr, dataptr, bandp, flagsp) );
}

int putmsg(int fd, const struct strbuf *ctlptr, const struct strbuf *dataptr,
       int flags)
{
	extern int _putmsg(int, const struct strbuf *,
			   const struct strbuf *, int);
	PT_CNCL_POINT( int, _putmsg, (fd, ctlptr, dataptr, flags) );
}

int putpmsg(int fd, const struct strbuf *ctlptr, const struct strbuf *dataptr,
	int band, int flags)
{
	extern int _putpmsg(int, const struct strbuf *, const struct strbuf *,
			    int, int);
	PT_CNCL_POINT( int, _putpmsg, (fd, ctlptr, dataptr, band, flags) );
}

int poll(struct pollfd *fds, unsigned long nfds, int timeout)
{
	extern int _poll(struct pollfd *, unsigned long, int);
	PT_CNCL_POINT( int, _poll, (fds, nfds, timeout) );
}


/* ------------------------------------------------------------------ */

#include <sys/socket.h>

int accept(int s, void *addr, int *addrlen)
{
	extern int _accept(int, void *, int *);
	PT_CNCL_POINT( int, _accept, (s, addr, addrlen) );
}

int connect(int s, const void *name, int namelen)
{
	extern int _connect(int, const void *, int);
	PT_CNCL_POINT( int, _connect, (s, name, namelen) );
}

int recv(int s, void *buf, int len, int flags)
{
	extern int _recv(int, void *, int, int);
	PT_CNCL_POINT( int, _recv, (s, buf, len, flags) );
}

int recvfrom(int s, void *buf, int len, int flags, void *from, int *fromlen)
{
	extern int _recvfrom(int, void *, int, int, void *, int *);
	PT_CNCL_POINT( int, _recvfrom, (s, buf, len, flags, from, fromlen) );
}

int recvmsg(int s, struct msghdr *msg, int flags)
{
	extern int _recvmsg(int, struct msghdr *, int);
	PT_CNCL_POINT( int, _recvmsg, (s, msg, flags) );
}

int send(int s, const void *msg, int len, int flags)
{
	extern int _send(int, const void *, int, int);
	PT_CNCL_POINT( int, _send, (s, msg, len, flags) );
}

int sendto(int s, const void *msg, int len, int flags, const void *to, int tolen)
{
	extern int _sendto(int, const void *, int, int, const void *, int);
	PT_CNCL_POINT( int, _sendto, (s, msg, len, flags, to, tolen) );
}

int sendmsg(int s, const struct msghdr *msg, int flags)
{
	extern int _sendmsg(int, const struct msghdr *, int);
	PT_CNCL_POINT( int, _sendmsg, (s, msg, flags) );
}

int select(int nfds, fd_set *readfds, fd_set *writefds,
       fd_set *exceptfds, struct timeval *timeout)
{
	extern int _select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
	PT_CNCL_POINT( int, _select,
			  (nfds, readfds, writefds, exceptfds, timeout) );
}


/* ------------------------------------------------------------------ */

#include <sys/uio.h>

ssize_t readv(int fildes, const struct iovec *iov, int iovcnt)
{
	extern int _readv(int, const struct iovec *, int);
	PT_CNCL_POINT( ssize_t, _readv, (fildes, iov, iovcnt) );
}
ssize_t writev(int fildes, const struct iovec *iov, int iovcnt)
{
	extern int _writev(int, const struct iovec *, int);
	PT_CNCL_POINT( ssize_t, _writev, (fildes, iov, iovcnt) );
}

#if (_MIPS_SIM == _ABIO32)
ssize_t __pread32(int fildes, void *buf, size_t nbyte, off_t offset)
{
	extern int ___pread32(int, void *, size_t, off_t);
	PT_CNCL_POINT( ssize_t, ___pread32, (fildes, buf, nbyte, offset) );
}
ssize_t __write32(int fildes, void *buf, size_t nbyte, off_t offset)
{
	extern int ___pwrite32(int, void *, size_t, off_t);
	PT_CNCL_POINT( ssize_t, ___pwrite32, (fildes, buf, nbyte, offset) );
}
#endif
ssize_t pread(int fildes, void *buf, size_t nbyte, off64_t offset)
{
	extern int _pread64(int, void *, size_t, off64_t);
	PT_CNCL_POINT( ssize_t, _pread64, (fildes, buf, nbyte, offset) );
}

ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off64_t offset)
{
	extern int _pwrite64(int, const void *, size_t, off64_t);
	PT_CNCL_POINT( ssize_t, _pwrite64, (fildes, buf, nbyte, offset) );
}
ssize_t pread64(int fildes, void *buf, size_t nbyte, off64_t offset)
{
	extern int _pread64(int, void *, size_t, off64_t);
	PT_CNCL_POINT( ssize_t, _pread64, (fildes, buf, nbyte, offset) );
}

ssize_t pwrite64(int fildes, const void *buf, size_t nbyte, off64_t offset)
{
	extern int _pwrite64(int, const void *, size_t, off64_t);
	PT_CNCL_POINT( ssize_t, _pwrite64, (fildes, buf, nbyte, offset) );
}


/* ------------------------------------------------------------------ */

#include <sys/dmi_kern.h>

int (dmi)(int opcode, void *arg1, void *arg2, void *arg3,
	        void *arg4, void *arg5, void *arg6, void *arg7)
{
	PT_CNCL_POINT( int, _dmi, (opcode, arg1, arg2, arg3,
					   arg4, arg5, arg6, arg7) );
}


/* ------------------------------------------------------------------ */

#include <sys/types.h>
#include <wait.h>

int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options)
{
	extern int	_waitid(idtype_t, id_t, siginfo_t *, int);
	PT_CNCL_POINT( int, _waitid, (idtype, id, infop, options) );
}

#include <sys/resource.h>

pid_t wait3(int *statptr, int options, struct rusage *rusage)
{
	extern int	_wait3(int *, int, struct rusage *);
	PT_CNCL_POINT( pid_t, _wait3, (statptr, options, rusage) );
}


/* ------------------------------------------------------------------ */

#include <unistd.h>

int usleep(useconds_t seconds)
{
	extern int _usleep(useconds_t);
	PT_CNCL_POINT( int, _usleep, (seconds) );
}


/* ------------------------------------------------------------------ */

#include <unistd.h>

int lockf(int fildes, int function, off_t size)
{
	extern int _lockf(int, int, off_t);
	PT_CNCL_POINT( int, _lockf, (fildes, function, size) );
}

/* ------------------------------------------------------------------ */

#include <signal.h>

int sigpause(int sig)
{
	extern int _sigpause(int);
	PT_CNCL_POINT( int, _sigpause, (sig) );
}

