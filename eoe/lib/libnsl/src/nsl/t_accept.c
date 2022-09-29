/*	Copyright (c) 1991, 1993 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_accept.c	1.5.2.1"
#ifdef __STDC__
        #pragma weak t_accept	= _t_accept
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
#include "sys/param.h"
#include "sys/types.h"
#include "sys/errno.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/tihdr.h"
#include "sys/timod.h"
#ifdef _BUILDING_LIBXNET
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/xti.h>
#else
#include "sys/tiuser.h"
#endif
#include "sys/signal.h"
#include "_import.h"



extern int t_errno;
extern int errno;
extern struct _ti_user *_t_checkfd();
extern int _t_is_event();
extern void (*sigset())();
extern int ioctl();


t_accept(fd, resfd, call)
int fd;
int resfd;
struct t_call *call;
{
	char *buf;
	register struct T_conn_res *cres;
	struct strfdinsert strfdinsert;
	int size;
	int retval;
	register struct _ti_user *tiptr;
	register struct _ti_user *restiptr;
	void (*sigsave)();
#ifdef _BUILDING_LIBXNET
	struct T_info_ack inforeq;
	struct stat fd_stat_buf, resfd_stat_buf;
#endif

	if ((tiptr = _t_checkfd(fd)) == NULL)
		return(-1);

	if ((restiptr = _t_checkfd(resfd)) == NULL)
		return(-1);

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		return(-1);
	}

#ifdef _BUILDING_LIBXNET
	/*
	 *  Verify current state for valid t_accept() operation,
	 *  only allowed in T_INCON state.
	 */
	if (tiptr->ti_state != T_INCON) {
		t_errno = TOUTSTATE;
		return(-1);
	}

	/*
	 *  Perform an information request for validation of user data,
	 *  if user data present in call structure.
	 */
	if (call->udata.len > 0) {
		sigsave = sigset(SIGPOLL, SIG_HOLD);
		inforeq.PRIM_type = T_INFO_REQ;

		/* Get provider information. */ 
		if (!_t_do_ioctl(fd, (caddr_t)&inforeq,
				sizeof(struct T_info_req),
				TI_GETINFO, &retval)) {
			sigset(SIGPOLL, sigsave);
			return(-1);
		}
		sigset(SIGPOLL, sigsave);
		
		if (retval != sizeof(struct T_info_ack)) {
			t_errno = TSYSERR;
			errno = EIO;
			return(-1);
		}

		/*
		 *  Validate udata sizes.
		 */
		if (inforeq.CDATA_size < 0) {
			/* If -2, means NO data allowed. */
			if (inforeq.CDATA_size == -2) {
				t_errno = TBADDATA;
				return(-1);
			}
			/* -1, means unlimit data. */
		} else {
			/* Check data size upper boundary. */
			if (call->udata.len > inforeq.CDATA_size) {
				t_errno = TBADDATA;
				return(-1);
			}
		}
	}
	if (fd != resfd) {
                fstat(fd, &fd_stat_buf);
                fstat(resfd, &resfd_stat_buf);
                if ((fd_stat_buf.st_dev != resfd_stat_buf.st_dev) ||
        	      (major(fd_stat_buf.st_rdev) != 
				major(resfd_stat_buf.st_rdev))) {
                        t_errno = TPROVMISMATCH;
                        return(-1);
                }

#ifdef THISCAUSESFAILS
                if ((restiptr->ti_qlen != 0) && 
				(restiptr->ti_state == T_IDLE)) {
                        t_errno = TRESQLEN;
                        return(-1);
                }
#endif	/* THISCAUSESFAILS */

		/*
		 *  Verify current state for valid t_accept() operation,
		 *  on resfd, only allowed in T_IDLE state.
		 */
		if ((restiptr->ti_state != T_IDLE) && 
		    (restiptr->ti_state != T_UNBND)) {
			t_errno = TOUTSTATE;
			return(-1);
		}
	}
#endif	/* _BUILDING_LIBXNET */

	if (fd != resfd)
	{
		if ((retval = ioctl(resfd,I_NREAD,&size)) < 0)
		{
			t_errno = TSYSERR;
			return(-1);
		}
		if (retval)
		{
			t_errno = TBADF;
			return(-1);
		}
	}
#ifdef _BUILDING_LIBXNET
	else {
		/*
		 * if fd == resfd and there is anything at the stream head,
		 * return TINDOUT for T_LISTEN or TLOOK for T_DISCONNECT.
		 * retval contains the number of messages on the stream head.
		 */
		if ((retval = ioctl(resfd,I_NREAD,&size)) < 0) {
			t_errno = TSYSERR;
			return(-1);
		}
		if (retval) {
			int event;

			if ((event = (int)t_look(fd)) == -1) {
				t_errno = TSYSERR;
				return(-1);
			}
			/* the way I read the spec, TLOOK for all but INCON */
			if (event == T_LISTEN)
				t_errno = TINDOUT;
			else
				t_errno = TLOOK;
			return(-1);
		}
	}
#endif

	sigsave = sigset(SIGPOLL, SIG_HOLD);
	if (_t_is_event(fd, tiptr)) {
		if (fd == resfd) {
			sigset(SIGPOLL, sigsave);
			return(-1);
		} else {
			int event;

			if ((event = (int)t_look(fd)) == -1) {
				sigset(SIGPOLL, sigsave);
				t_errno = TSYSERR;
				return(-1);
			}
			sigset(SIGPOLL, sigsave);

			/* some queued connection requests on the fd? */
			if (event != T_LISTEN) {
				t_errno = TLOOK;
				return(-1);
			}
		}
	}

	buf = tiptr->ti_ctlbuf;
	cres = (struct T_conn_res *)buf;
	cres->PRIM_type = T_CONN_RES;
	cres->OPT_length = call->opt.len;
	cres->OPT_offset = 0;
	cres->SEQ_number = call->sequence;
	size = sizeof(struct T_conn_res);

	if (call->opt.len) {
		_t_aligned_copy(buf, call->opt.len, size,
			     call->opt.buf, &cres->OPT_offset);
		size = cres->OPT_offset + cres->OPT_length;
	}


	strfdinsert.ctlbuf.maxlen = tiptr->ti_ctlsize;
	strfdinsert.ctlbuf.len = size;
	strfdinsert.ctlbuf.buf = buf;
	strfdinsert.databuf.maxlen = call->udata.maxlen;
	strfdinsert.databuf.len = (call->udata.len? call->udata.len: -1);
	strfdinsert.databuf.buf = call->udata.buf;
	strfdinsert.fildes = resfd;
	strfdinsert.offset = sizeof(long);
	strfdinsert.flags = 0;      /* could be EXPEDITED also */

	if (ioctl(fd, I_FDINSERT, &strfdinsert) < 0) {
		if (errno == EAGAIN)
			t_errno = TFLOW;
		else
			t_errno = TSYSERR;
		sigset(SIGPOLL, sigsave);
		return(-1);
	}

	if (!_t_is_ok(fd, tiptr, T_CONN_RES)) {
		sigset(SIGPOLL, sigsave);
		return(-1);
	}

	sigset(SIGPOLL, sigsave);

	if (tiptr->ti_ocnt == 1) {
		if (fd == resfd)
			tiptr->ti_state = TLI_NEXTSTATE(T_ACCEPT1, tiptr->ti_state);
		else {
			tiptr->ti_state = TLI_NEXTSTATE(T_ACCEPT2, tiptr->ti_state);
			restiptr->ti_state = TLI_NEXTSTATE(T_PASSCON, restiptr->ti_state);
		}
	}
	else {
		tiptr->ti_state = TLI_NEXTSTATE(T_ACCEPT3, tiptr->ti_state);
		restiptr->ti_state = TLI_NEXTSTATE(T_PASSCON, restiptr->ti_state);
	}

	tiptr->ti_ocnt--;
	return(0);
}
