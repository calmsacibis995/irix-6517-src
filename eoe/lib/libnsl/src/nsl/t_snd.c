/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_snd.c	1.3.3.1"
#ifdef __STDC__
        #pragma weak t_snd	= _t_snd
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
extern int putpmsg();


t_snd(fd, buf, nbytes, flags)
int fd;
register char *buf;
unsigned nbytes;
int flags;
{
	struct strbuf ctlbuf, databuf;
	struct T_data_req *datareq;
	unsigned tmpcnt, tmp;
	char *tmpbuf;
	register struct _ti_user *tiptr;
	int band;
	int ret;
#ifdef _BUILDING_LIBXNET
	int r;
#endif

	if ((tiptr = _t_checkfd(fd)) == NULL)
		return(-1);

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		return(-1);
	}
	if (!(tiptr->ti_flags & SENDZERO) && nbytes == 0) {
		t_errno = TBADDATA;
		return(-1);
	}

#ifdef _BUILDING_LIBXNET
       /*
        *  Check the stream head for a pending T_DISCONNECT event.
        *  If true, fail with TLOOK error.
        */
        if ((r = _t_look(fd)) == T_DISCONNECT) {
                t_errno = TLOOK;
                return(-1);
        }
        if (r == T_ORDREL) {
                t_errno = TLOOK;
                return(-1);
        }

	/* only T_MORE / T_EXPEDITED allowed in flags */
        if (flags & ~(T_MORE | T_EXPEDITED)) {
                t_errno = TBADFLAG;
                return(-1);
        }

	if ((tiptr->ti_tsdu > 0) && (nbytes > tiptr->ti_tsdu)) {
		t_errno = TBADDATA;
		return(-1);
	}
	if (TLI_NEXTSTATE(T_SND, tiptr->ti_state) == nvs) {
		t_errno = TOUTSTATE;
		return(-1);
	}
#endif

	datareq = (struct T_data_req *)tiptr->ti_ctlbuf;
	if (flags&T_EXPEDITED) {
		datareq->PRIM_type = T_EXDATA_REQ;
		if (tiptr->ti_flags & EXPINLINE)
			band = TI_NORMAL;
		else
			band = TI_EXPEDITED;
	} else {
		datareq->PRIM_type = T_DATA_REQ;
		band = TI_NORMAL;
	}


	ctlbuf.maxlen = sizeof(struct T_data_req);
	ctlbuf.len = sizeof(struct T_data_req);
	ctlbuf.buf = tiptr->ti_ctlbuf;
	tmp = nbytes;
	tmpbuf = buf;

	do {
		if ((tmpcnt = tmp) > tiptr->ti_maxpsz) {
			datareq->MORE_flag = 1;
			tmpcnt = tiptr->ti_maxpsz;
		} else {
			if (flags&T_MORE)
				datareq->MORE_flag = 1;
			else
				datareq->MORE_flag = 0;
		}

		databuf.maxlen = tmpcnt;
		databuf.len = tmpcnt;
		databuf.buf = tmpbuf;

#ifndef NO_IMPORT
		if (__putpmsg) {
			ret = putpmsg(fd, &ctlbuf, &databuf, band, MSG_BAND);
		} else {
			ret = putmsg(fd, &ctlbuf, &databuf, 0);
		}
#else
		ret = putpmsg(fd, &ctlbuf, &databuf, band, MSG_BAND);
#endif
		if (ret < 0) {
			if (nbytes == tmp) {
				if (errno == EAGAIN)
					t_errno = TFLOW;
				else
					t_errno = TSYSERR;
				return(-1);
			} else
				tiptr->ti_state = TLI_NEXTSTATE(T_SND, tiptr->ti_state);
				return(nbytes - tmp);
		}
		tmp = tmp - tmpcnt;
		tmpbuf = tmpbuf + tmpcnt;
	} while (tmp);

	tiptr->ti_state = TLI_NEXTSTATE(T_SND, tiptr->ti_state);
	return(nbytes - tmp);
}
