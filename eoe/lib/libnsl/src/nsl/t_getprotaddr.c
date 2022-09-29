/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_getprotadd.c	1.1.1.1"
#ifdef __STDC__
#ifndef _LIBNSL_ABI
        #pragma weak t_getprotaddr	= _t_getprotaddr
#endif /* _LIBNSL_ABI */
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
#include "sys/errno.h"
#include "sys/types.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/tihdr.h"
#include "sys/tiuser.h"
#include "sys/timod.h"
#include "sys/signal.h"
#include "_import.h"


extern int t_errno;
extern int errno;
extern void (*sigset())();
extern int ioctl();

/*
int
t_getprotaddr(fd, name, type)
int fd;
struct t_bind *name;
struct t_bind *type;
{
	return(0);
}
*/


/*
extern void (*sigset(int, void (*)(int)))(int);
extern int ioctl(int, int, ... );
*/
extern struct _ti_user *_t_checkfd();

int
t_getprotaddr(int fd, struct t_bind *boundaddr, struct t_bind *peeraddr)
{
	struct _ti_user *tiptr;
	struct T_addr_req areq;
	struct T_addr_ack *aackp;
	struct strbuf ctlbuf;
	void (*sigsave)();
	int band = 0;
	int flags = MSG_HIPRI;

	if ((tiptr = _t_checkfd(fd)) == NULL) {
		return(-1);
	}

	areq.PRIM_type = T_ADDR_REQ;

	ctlbuf.len = sizeof(struct T_addr_req);
	ctlbuf.buf = (caddr_t)&areq;

	sigsave = sigset(SIGPOLL, SIG_HOLD);

	if (putmsg(fd, &ctlbuf, NULL, 0) < 0) {
		sigset(SIGPOLL, sigsave);
		t_errno = TSYSERR;
		return(-1);
	}

	ctlbuf.maxlen = tiptr->ti_ctlsize;
	ctlbuf.len = 0;
	ctlbuf.buf = tiptr->ti_ctlbuf; /* Now watch ctlbuf.buf also */

	if (getpmsg(fd, &ctlbuf, NULL, &band, &flags) < 0) {
		sigset(SIGPOLL, sigsave);
		t_errno = TSYSERR;
		return(-1);
	}

	sigset(SIGPOLL, sigsave);

	if (ctlbuf.len < sizeof(struct T_addr_ack)) {
		t_errno = TSYSERR;
		return(-1);
	}

	aackp = (struct T_addr_ack *) ctlbuf.buf; /* Now watch aackp also */
	if (aackp->PRIM_type != T_ADDR_ACK) {
		t_errno = TSYSERR;
		return(-1);
	}

	if (boundaddr) {
		/* Will buffer hold returned address? */
		if (boundaddr->addr.maxlen < aackp->LOCADDR_length) {
			t_errno = TBUFOVFLW;
			return(-1);
		}
		memcpy(boundaddr->addr.buf,
		       (char *) aackp + aackp->LOCADDR_offset,
			aackp->LOCADDR_length);
		boundaddr->addr.len = aackp->LOCADDR_length;
	}

	if (peeraddr && tiptr->ti_state == T_DATAXFER) {
		/* Will buffer hold returned address? */
		if (peeraddr->addr.maxlen < aackp->REMADDR_length) {
			t_errno = TBUFOVFLW;
			return(-1);
		}
		memcpy(peeraddr->addr.buf,
		       (char *) aackp + aackp->REMADDR_offset,
			aackp->REMADDR_length);
		peeraddr->addr.len = aackp->REMADDR_length;
	}
	return(0);
}
