/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_optmgmt.c	1.3.4.1"
#ifdef __STDC__
        #pragma weak t_optmgmt	= _t_optmgmt
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
#include "sys/param.h"
#include "sys/types.h"
#include "errno.h"
#include "memory.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/tihdr.h"
#include "sys/timod.h"
#include "sys/tiuser.h"
#include "signal.h"
#include "_import.h"


extern int t_errno;
extern struct _ti_user *_t_checkfd();

t_optmgmt(fd, req, ret)
int fd;
struct t_optmgmt *req;
struct t_optmgmt *ret;
{
	int size;
	register char *buf;
	register struct T_optmgmt_req *optreq;
	register struct _ti_user *tiptr;
	void (*sigsave)();


	if ((tiptr = _t_checkfd(fd)) == NULL)
		return(-1);

#ifdef _BUILDING_LIBXNET
	if (!(req->flags & (T_NEGOTIATE | T_CHECK | T_DEFAULT | T_CURRENT)) 
		|| (req->flags & 
			~(T_NEGOTIATE | T_CHECK | T_DEFAULT | T_CURRENT)))
	{
		t_errno = TBADFLAG;
		return(-1);
	}
#endif

	sigsave = sigset(SIGPOLL, SIG_HOLD);
	buf = tiptr->ti_ctlbuf;
	optreq = (struct T_optmgmt_req *)buf;

#ifdef _BUILDING_LIBXNET
	optreq->PRIM_type = _XTI_T_OPTMGMT_REQ;
#else
	optreq->PRIM_type = T_OPTMGMT_REQ;
#endif
	optreq->OPT_length = req->opt.len;
	optreq->OPT_offset = 0;
	optreq->MGMT_flags = req->flags;
	size = sizeof(struct T_optmgmt_req);

	if (req->opt.len) {
		_t_aligned_copy(buf, req->opt.len, size,
			     req->opt.buf, &optreq->OPT_offset);
		size = optreq->OPT_offset + optreq->OPT_length;
	}

	if (!_t_do_ioctl(fd, buf, size, TI_OPTMGMT, NULL)) {
		sigset(SIGPOLL, sigsave);
		return(-1);
	}
	sigset(SIGPOLL, sigsave);


#ifdef _BUILDING_LIBXNET
	if ((ret->opt.maxlen != 0) && (optreq->OPT_length > ret->opt.maxlen))
#else
	if (optreq->OPT_length > ret->opt.maxlen)
#endif
	{
		t_errno = TBUFOVFLW;
		return(-1);
	}

#ifdef _BUILDING_LIBXNET
	ret->opt.len = 0;
	if ((ret->opt.maxlen != 0) && (optreq->OPT_length <= ret->opt.maxlen))
	{
#endif
	memcpy(ret->opt.buf, (char *) (buf + optreq->OPT_offset),
	       (int)optreq->OPT_length);
	ret->opt.len = optreq->OPT_length;
#ifdef _BUILDING_LIBXNET
	}
#endif

#ifdef _BUILDING_LIBXNET
	ret->flags |= optreq->MGMT_flags;
#else
	ret->flags = optreq->MGMT_flags;
#endif

	tiptr->ti_state = TLI_NEXTSTATE(T_OPTMGMT, tiptr->ti_state);
	return(0);
}

