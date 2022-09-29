/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_listen.c	1.5"
#ifdef __STDC__
        #pragma weak t_listen	= _t_listen
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
#include "_import.h"


extern int t_errno;
extern struct _ti_user *_t_checkfd();

t_listen(fd, call)
int fd;
struct t_call *call;
{
	struct strbuf ctlbuf;
	struct strbuf rcvbuf;
	int flg = 0;
	int retval;
	register union T_primitives *pptr;
	register struct _ti_user *tiptr;

	if ((tiptr = _t_checkfd(fd)) == NULL)
		return(-1);

#ifdef _BUILDING_LIBXNET
	if ((tiptr->ti_state != T_IDLE) && (tiptr->ti_state != T_INCON)) {
		t_errno = TOUTSTATE;
		return(-1);
	}
#endif

	/*
         * check if something in look buffer
	 */
	if (tiptr->ti_lookflg) {
		t_errno = TLOOK;
		return(-1);
	}

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		return(-1);
	}

#ifdef _BUILDING_LIBXNET
	if (tiptr->ti_qlen == 0) {
		t_errno = TBADQLEN;
		return(-1);
	}

	if (tiptr->ti_ocnt >= tiptr->ti_qlen) {
		t_errno = TQFULL;
		return(-1);
	}
#endif

	ctlbuf.maxlen = tiptr->ti_ctlsize;
	ctlbuf.len = 0;
	ctlbuf.buf = tiptr->ti_ctlbuf;
	rcvbuf.maxlen = tiptr->ti_rcvsize;
	rcvbuf.len = 0;
	rcvbuf.buf = tiptr->ti_rcvbuf;

	if ((retval = getmsg(fd, &ctlbuf, &rcvbuf, &flg)) < 0) {
		if (errno == EAGAIN)
			t_errno = TNODATA;
		else
			t_errno = TSYSERR;
		return(-1);
	}
	if (rcvbuf.len == -1) rcvbuf.len = 0;

	/*
	 * did I get entire message?
	 */
	if (retval) {
		t_errno = TSYSERR;
		errno = EIO;
		return(-1);
	}

	/*
	 * is ctl part large enough to determine type
	 */
	if (ctlbuf.len < sizeof(long)) {
		t_errno = TSYSERR;
		errno = EPROTO;
		return(-1);
	}

	pptr = (union T_primitives *)ctlbuf.buf;

	switch(pptr->type) {

		case T_CONN_IND:
			if ((ctlbuf.len < sizeof(struct T_conn_ind)) ||
			    (ctlbuf.len < (pptr->conn_ind.OPT_length
			    + pptr->conn_ind.OPT_offset))) {
				t_errno = TSYSERR;
				errno = EPROTO;
				return(-1);
			}
#ifdef _BUILDING_LIBXNET
			if (((call->udata.maxlen != 0) &&
				(rcvbuf.len > call->udata.maxlen)) ||
			    ((call->addr.maxlen != 0) &&
			   (pptr->conn_ind.SRC_length > call->addr.maxlen)) ||
			    ((call->opt.maxlen != 0) &&
			   (pptr->conn_ind.OPT_length > call->opt.maxlen)))
			{
				t_errno = TBUFOVFLW;
				/*
				 * XTI spec says that we should be able to
				 * disconnect even after TBUFOVFLW, so set
				 * sequence here.
				 */
				call->sequence = 
					(long) pptr->conn_ind.SEQ_number;
				tiptr->ti_ocnt++;
				tiptr->ti_state = 
				    TLI_NEXTSTATE(T_LISTN, tiptr->ti_state);
				return(-1);
			}
#else
			if ((rcvbuf.len > call->udata.maxlen) ||
			    (pptr->conn_ind.SRC_length > call->addr.maxlen) ||
			    (pptr->conn_ind.OPT_length > call->opt.maxlen))
			{
				t_errno = TBUFOVFLW;
				tiptr->ti_ocnt++;
				tiptr->ti_state = 
				    TLI_NEXTSTATE(T_LISTN, tiptr->ti_state);
				return(-1);
			}
#endif

#ifdef _BUILDING_LIBXNET
			call->addr.len = 0;
			call->opt.len = 0;
			call->udata.len = 0;
			if (call->addr.maxlen != 0) {
#endif
			memcpy(call->addr.buf, ctlbuf.buf +
				pptr->conn_ind.SRC_offset,
				(int)pptr->conn_ind.SRC_length);
			call->addr.len = pptr->conn_ind.SRC_length;
#ifdef _BUILDING_LIBXNET
			}
			if (call->opt.maxlen != 0) {
#endif
			memcpy(call->opt.buf, ctlbuf.buf +
				pptr->conn_ind.OPT_offset,
				(int)pptr->conn_ind.OPT_length);
			call->opt.len = pptr->conn_ind.OPT_length;
#ifdef _BUILDING_LIBXNET
			}
			if (call->udata.maxlen != 0) {
#endif
			memcpy(call->udata.buf, rcvbuf.buf, (int)rcvbuf.len);
			call->udata.len = rcvbuf.len;
#ifdef _BUILDING_LIBXNET
			}
#endif
			call->sequence = (long) pptr->conn_ind.SEQ_number;

			tiptr->ti_ocnt++;
			tiptr->ti_state = TLI_NEXTSTATE(T_LISTN, tiptr->ti_state);
			return(0);

		case T_DISCON_IND:
			_t_putback(tiptr, rcvbuf.buf, rcvbuf.len, ctlbuf.buf, ctlbuf.len);
			t_errno = TLOOK;
			return(-1);

		default:
			break;
	}

	t_errno = TSYSERR;
	errno = EPROTO;
	return(-1);
}
