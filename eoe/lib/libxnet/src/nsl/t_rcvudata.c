/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_rcvudata.c	1.5"
#ifdef __STDC__
        #pragma weak t_rcvudata	= _t_rcvudata
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
#include "sys/param.h"
#include "sys/types.h"
#include "errno.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/tihdr.h"
#include "sys/timod.h"
#include "sys/tiuser.h"
#include "memory.h"
#include "_import.h"


extern int t_errno;
extern struct _ti_user *_t_checkfd();


t_rcvudata(fd, unitdata, flags)
int fd;
register struct t_unitdata *unitdata;
int *flags;
{
	struct strbuf ctlbuf;
	int retval, flg = 0;
	register union T_primitives *pptr;
	register struct _ti_user *tiptr;

	if ((tiptr = _t_checkfd(fd)) == NULL)
		return(-1);

	if (tiptr->ti_servtype != T_CLTS) {
		t_errno = TNOTSUPPORT;
		return(-1);
	}
#ifdef _BUILDING_LIBXNET
	if (tiptr->ti_state != T_IDLE) {
		t_errno = TOUTSTATE;
		return(-1);
	}
#endif	/* _BUILDING_LIBXNET */

	/*
         * check if there is something in look buffer
	 */
	if (tiptr->ti_lookflg) {
		t_errno = TLOOK;
		return(-1);
	}

	ctlbuf.maxlen = tiptr->ti_ctlsize;
	ctlbuf.len = 0;
	ctlbuf.buf = tiptr->ti_ctlbuf;
	*flags = 0;

	/*
	 * data goes right in user buffer
	 */
	if ((retval = getmsg(fd, &ctlbuf, (struct strbuf *) &unitdata->udata,
				&flg)) < 0) {
		if (errno == EAGAIN)
			t_errno = TNODATA;
		else
			t_errno = TSYSERR;
		return(-1);
	}
	if (unitdata->udata.len == -1) unitdata->udata.len = 0;

	/*
	 * is there control piece with data?
	 */
	if (ctlbuf.len > 0) {
		if (ctlbuf.len < sizeof(long)) {
			t_errno = TSYSERR;
			errno = EPROTO;
			unitdata->udata.len = 0;
			return(-1);
		}
	
		pptr = (union T_primitives *)ctlbuf.buf;
	
		switch(pptr->type) {
	
			case T_UNITDATA_IND:
				if ((ctlbuf.len < sizeof(struct T_unitdata_ind)) ||
				    (pptr->unitdata_ind.OPT_length &&
				     (ctlbuf.len < (pptr->unitdata_ind.OPT_length
				     + pptr->unitdata_ind.OPT_offset)))) {
					t_errno = TSYSERR;
					errno = EPROTO;
					unitdata->udata.len = 0;
					return(-1);
				}
#ifdef _BUILDING_LIBXNET
				if (((unitdata->addr.maxlen != 0) &&
				    ((pptr->unitdata_ind.SRC_length > 
						unitdata->addr.maxlen))) ||
				    ((unitdata->opt.maxlen != 0) &&
				    ((pptr->unitdata_ind.OPT_length > 
						unitdata->opt.maxlen))))
				{
#else
				if ((pptr->unitdata_ind.SRC_length > 
						unitdata->addr.maxlen) ||
				    (pptr->unitdata_ind.OPT_length > 
						unitdata->opt.maxlen))
				{
#endif
					t_errno = TBUFOVFLW;
					unitdata->udata.len = 0;
					return(-1);
				}
	
				if (retval)
					*flags |= T_MORE;
	
#ifdef _BUILDING_LIBXNET
				if (unitdata->addr.maxlen != 0) {
#endif
				memcpy(unitdata->addr.buf, ctlbuf.buf +
					pptr->unitdata_ind.SRC_offset,
					(int)pptr->unitdata_ind.SRC_length);
				unitdata->addr.len = pptr->unitdata_ind.SRC_length;
#ifdef _BUILDING_LIBXNET
				}
#endif

#ifdef _BUILDING_LIBXNET
				if (unitdata->opt.maxlen != 0) {
#endif
				memcpy(unitdata->opt.buf, ctlbuf.buf +
					pptr->unitdata_ind.OPT_offset,
					(int)pptr->unitdata_ind.OPT_length);
				unitdata->opt.len = pptr->unitdata_ind.OPT_length;
#ifdef _BUILDING_LIBXNET
				}
#endif
	
				tiptr->ti_state = TLI_NEXTSTATE(T_RCVUDATA, tiptr->ti_state);
				return(0);
	
			case T_UDERROR_IND:
				_t_putback(tiptr, unitdata->udata.buf, 0, ctlbuf.buf, ctlbuf.len);
				unitdata->udata.len = 0;
				t_errno = TLOOK;
				return(-1);
		
			default:
				break;
		}
		
		t_errno = TSYSERR;
		errno = EPROTO;
		return(-1);
	} else { 
		unitdata->addr.len = 0;
		unitdata->opt.len = 0;
		/*
		 * only data in message no control piece
		 */
		if (retval)
			*flags = T_MORE;

		tiptr->ti_state = TLI_NEXTSTATE(T_RCVUDATA, tiptr->ti_state);
		return(0);
	}
}
