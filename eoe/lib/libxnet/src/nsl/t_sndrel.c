/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_sndrel.c	1.4"
#ifdef __STDC__
        #pragma weak t_sndrel	= _t_sndrel
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
#include "sys/tiuser.h"
#include "_import.h"


extern int t_errno;
extern int errno;
extern struct _ti_user *_t_checkfd();
extern int putmsg();


t_sndrel(fd)
int fd;
{
	struct T_ordrel_req orreq;
	struct strbuf ctlbuf;
	register struct _ti_user *tiptr;
#ifdef _BUILDING_LIBXNET
	int	event;
#endif


	if ((tiptr = _t_checkfd(fd)) == NULL)
		return(-1);

	if (tiptr->ti_servtype != T_COTS_ORD) {
		t_errno = TNOTSUPPORT;
		return(-1);
	}


#ifdef _BUILDING_LIBXNET
	if ((tiptr->ti_state != T_INREL) && (tiptr->ti_state != T_DATAXFER)) {
		t_errno = TOUTSTATE;
		return(-1);
	}
	t_errno = 0;
	if ((event = (int)t_look(fd)) == -1) {
		if(!t_errno)
			t_errno = TSYSERR;
		return(-1);
	}
	/*
	 *  If event, is T_DISCONNECT fail with TLOOK, else
	 *  continue with orderly release.
	 */
	if (event == T_DISCONNECT) {
		t_errno = TLOOK;
		return(-1);
	}
#endif

	orreq.PRIM_type = T_ORDREL_REQ;
	ctlbuf.maxlen = sizeof(struct T_ordrel_req);
	ctlbuf.len = sizeof(struct T_ordrel_req);
	ctlbuf.buf = (caddr_t)&orreq;

	if (putmsg(fd, &ctlbuf, NULL, 0) < 0) {
		if (errno == EAGAIN)
			t_errno = TFLOW;
		else
			t_errno = TSYSERR;
		return(-1);
	}

	tiptr->ti_state = TLI_NEXTSTATE(T_SNDREL, tiptr->ti_state);
	return(0);
}
