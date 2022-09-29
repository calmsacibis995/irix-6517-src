/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_sndudata.c	1.5"
#ifdef __STDC__
        #pragma weak t_sndudata	= _t_sndudata
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
#ifdef _BUILDING_LIBXNET
#include <xti.h>
#endif

extern struct _ti_user *_t_checkfd();
extern int t_errno;
extern int errno;

t_sndudata(fd, unitdata)
int fd;
register struct t_unitdata *unitdata;
{
	register struct T_unitdata_req *udreq;
	char *buf;
	struct strbuf ctlbuf;
	int size;
	register struct _ti_user *tiptr;

	if ((tiptr = _t_checkfd(fd)) == NULL)
		return(-1);

	if (tiptr->ti_servtype != T_CLTS) {
		t_errno = TNOTSUPPORT;
		return(-1);
	}

	if (!(tiptr->ti_flags & SENDZERO) && (int)unitdata->udata.len == 0) {
		t_errno = TBADDATA;
		return(-1);
	}

#ifdef _BUILDING_LIBXNET
	if (tiptr->ti_state != T_IDLE) {
		t_errno = TOUTSTATE;
		return(-1);
	}
	if ((tiptr->ti_tsdu > 0 ) && 
			((int)unitdata->udata.len > tiptr->ti_tsdu)) {
		t_errno = TBADDATA;
		errno = EPROTO;
		return(-1);
	}
#endif

	if ((int)unitdata->udata.len > tiptr->ti_maxpsz) {
		t_errno = TSYSERR;
		errno = EPROTO;
		return(-1);
	}

	buf = tiptr->ti_ctlbuf;
	udreq = (struct T_unitdata_req *)buf;

#ifdef _BUILDING_LIBXNET
	if(unitdata->opt.len) {
		struct t_opthdr *eoptreq; /* end of options */
		struct t_opthdr *curopt;  /* current option */
		struct t_opthdr *topthdrp;
		int	total_len = 0;

		topthdrp = (struct t_opthdr *)unitdata->opt.buf;
		eoptreq = (struct t_opthdr *)
			((void *)((char *)topthdrp + unitdata->opt.len));

		for(curopt = topthdrp; curopt;
			curopt = OPT_NEXTHDR(topthdrp,
				unitdata->opt.len, curopt))
		{
			if(((void *)((char *)curopt +
				((struct t_opthdr *)curopt)->len)) >
						(void *)eoptreq)
			{
				t_errno = TBADOPT;
				return(-1);
			}
			total_len += (int)curopt->len;
		}
		if(total_len != unitdata->opt.len) {
			t_errno = TBADOPT;
			return(-1);
		}
	}
#endif
	udreq->PRIM_type = T_UNITDATA_REQ;
	udreq->DEST_length = unitdata->addr.len;
	udreq->DEST_offset = 0;
	udreq->OPT_length = unitdata->opt.len;
	udreq->OPT_offset = 0;
	size = sizeof(struct T_unitdata_req);

	if (unitdata->addr.len) {
		_t_aligned_copy(buf, unitdata->addr.len, size,
			     unitdata->addr.buf, &udreq->DEST_offset);
		size = udreq->DEST_offset + udreq->DEST_length;
	}
	if (unitdata->opt.len) {
		_t_aligned_copy(buf, unitdata->opt.len, size,
			     unitdata->opt.buf, &udreq->OPT_offset);
		size = udreq->OPT_offset + udreq->OPT_length;
	}

	if (size > tiptr->ti_ctlsize) {
		t_errno = TSYSERR;
		errno = EIO;
		return(-1);
	}
	ctlbuf.maxlen = tiptr->ti_ctlsize;
	ctlbuf.len = size;
	ctlbuf.buf = buf;

	if (putmsg(fd, &ctlbuf, (const struct strbuf *)(unitdata->udata.len? 
			  &unitdata->udata: NULL), 0) < 0) {
		if (errno == EAGAIN)
			t_errno = TFLOW;
		else
			t_errno = TSYSERR;
		return(-1);
	}

	tiptr->ti_state = TLI_NEXTSTATE(T_SNDUDATA, tiptr->ti_state);
	return(0);
}
