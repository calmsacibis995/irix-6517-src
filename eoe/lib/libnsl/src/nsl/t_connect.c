/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_connect.c	1.7"
#ifdef __STDC__
        #pragma weak t_connect	= _t_connect
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
#include "sys/param.h"
#include "sys/types.h"
#include "sys/stropts.h"
#include "sys/timod.h"
#include "sys/tiuser.h"
#include "fcntl.h"
#include "sys/signal.h"
#include "_import.h"

extern snd_conn_req(), rcv_conn_con();
extern int t_errno;
extern struct _ti_user *_t_checkfd();
extern void (*sigset())();


t_connect(fd, sndcall, rcvcall)
int fd;
struct t_call *sndcall;
struct t_call *rcvcall;
{
	int fctlflg;
	register struct _ti_user *tiptr;

	if ((tiptr = _t_checkfd(fd)) == NULL)
		return(-1);

#ifdef _BUILDING_LIBXNET
	/*
	 *  Must be in T_IDLE state, else fail with TOUTSTATE error.
	 */
	if (tiptr->ti_state != T_IDLE) {
		t_errno = TOUTSTATE;
		return(-1);
	}
#endif

	if (_snd_conn_req(fd, sndcall) < 0)
		return(-1);

	if ((fctlflg = fcntl(fd, F_GETFL, 0)) < 0) {
		t_errno = TSYSERR;
		return(-1);
	}

	if (fctlflg & (O_NDELAY | O_NONBLOCK)) {
		tiptr->ti_state = TLI_NEXTSTATE(T_CONNECT2, tiptr->ti_state);
		t_errno = TNODATA;
		return(-1);
	}

	if (_rcv_conn_con(fd, rcvcall) < 0) {
		if (t_errno == TLOOK)
			tiptr->ti_state = TLI_NEXTSTATE(T_CONNECT2, tiptr->ti_state);
		else if (t_errno == TBUFOVFLW)
			tiptr->ti_state = TLI_NEXTSTATE(T_CONNECT1, tiptr->ti_state);
#ifdef _BUILDING_LIBXNET
		/* this can happen if signal interrupt */
		else if (t_errno == TNODATA)
			tiptr->ti_state = 
				TLI_NEXTSTATE(T_CONNECT2, tiptr->ti_state);
#endif
		return(-1);
	}

	tiptr->ti_state = TLI_NEXTSTATE(T_CONNECT1, tiptr->ti_state);
	return(0);
}
