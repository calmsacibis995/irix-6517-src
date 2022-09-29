/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_sync.c	1.4.4.1"
#ifdef __STDC__
        #pragma weak t_sync	= _t_sync
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
#include "sys/param.h"
#include "sys/types.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include "ulimit.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/tihdr.h"
#include "sys/timod.h"
#include "sys/tiuser.h"
#include "signal.h"
#include "_import.h"


extern struct _ti_user *_ti_user;
extern int t_errno;
extern long openfiles;
extern struct _ti_user *_t_checkfd();
extern _null_tiptr();

t_sync(fd)
int fd;
{
	int retval;
	struct T_info_ack info;
	register struct _ti_user *tiptr;
	int retlen;
	void (*sigsave)();
	int arg,rval;

	/*
	 * Initialize "openfiles" - global variable
  	 * containing number of open files allowed
	 * per process.
	 */
	openfiles = OPENFILES;

	/*
         * if needed allocate the ti_user structures
	 * for all file desc.
	 */
	 if (!_ti_user) 
		if ((_ti_user = (struct _ti_user *)calloc(1, (unsigned)(openfiles*sizeof(struct _ti_user)))) == NULL) {
			t_errno = TSYSERR;
			return(-1);
		}

	/* XXX above code will leak memory if subsequent ops fail */
	sigsave = sigset(SIGPOLL, SIG_HOLD);
	info.PRIM_type = T_INFO_REQ;


#ifdef _BUILDING_LIBXNET
	/*
	 * X/Open expects t_sync() to work on descriptors opened with
	 * open(); crazy but true.  Push timod if it's not there.
	 */
	if ((retval = ioctl(fd, I_FIND, "timod")) <= 0) {
		if ((retval = ioctl(fd, I_PUSH, "timod")) < 0) {
			sigset(SIGPOLL, sigsave);
			t_errno = TBADF;
			return(-1);
		}
	}
#else
	if ((retval = ioctl(fd, I_FIND, "timod")) < 0) {
		sigset(SIGPOLL, sigsave);
		t_errno = TBADF;
		return(-1);
	}

	if (!retval) {
		sigset(SIGPOLL, sigsave);
		t_errno = TBADF;
		return(-1);
	}
#endif
	if (!_t_do_ioctl(fd, (caddr_t)&info, sizeof(struct T_info_req), TI_GETINFO, &retlen) < 0) {
		sigset(SIGPOLL, sigsave);
		return(-1);
	}
	sigset(SIGPOLL, sigsave);
			
	if (retlen != sizeof(struct T_info_ack)) {
		errno = EIO;
		t_errno = TSYSERR;
		return(-1);
	}

	/* 
	 * Range of file desc. is OK, the ioctl above was successful!
	 * Check for fork/exec case.
	 */

	if ((tiptr = _t_checkfd(fd)) == NULL) {

		tiptr = &_ti_user[fd];

		if (_t_alloc_bufs(fd, tiptr, info) < 0) {
			_null_tiptr(tiptr);
			t_errno = TSYSERR;
			return(-1);
		}
		/****** Hack to fix exec user level state problem *********/
		/****** DATAXFER and DISCONNECT cases are covered *********/
		/****** Solves the problems for execed servers*************/

			if (info.CURRENT_state == TS_DATA_XFER)
				tiptr->ti_state = T_DATAXFER;
			else  {
				if (info.CURRENT_state == TS_IDLE) {
					if((rval = ioctl(fd,I_NREAD,&arg)) < 0)  {
						t_errno = TSYSERR;
						return(-1);
					}
					if(rval == 0 )
						tiptr->ti_state = T_IDLE;
					else
						tiptr->ti_state = T_DATAXFER;
				}
				else
					tiptr->ti_state = T_FAKE;
	       	 }			
	}

	switch (info.CURRENT_state) {

	case TS_UNBND:
		return(T_UNBND);
	case TS_IDLE:
		return(T_IDLE);
	case TS_WRES_CIND:
		return(T_INCON);
	case TS_WCON_CREQ:
		return(T_OUTCON);
	case TS_DATA_XFER:
#ifdef _BUILDING_LIBXNET
		/*
		 *  Check to see if a T_CONNECT event is pending
		 *  on the stream head.  If so, the library state
		 *  must be T_OUTCON state not T_DATAXFER so that
		 *  the T_CONNECT event can be received.
		 */
		if ((retval = _t_look(fd)) == -1) {
			/*
			 * See comments in TS_IDLE case.
			 */
			tiptr->ti_state = T_FAKE;
			t_errno = TSYSERR;
			break;
		}
		if (retval == T_CONNECT) {
			retval = tiptr->ti_state = T_OUTCON;
		} else {
			retval = tiptr->ti_state = T_DATAXFER;
		}
		break;
#else
		return(T_DATAXFER);
#endif
	case TS_WIND_ORDREL:
		return(T_OUTREL);
	case TS_WREQ_ORDREL:
		return(T_INREL);
	default:
		t_errno = TSTATECHNG;
		return(-1);
	}
	return retval;
}
