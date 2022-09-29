/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.4 $"

#include "synonyms.h"
#include <errno.h>
#include <pfmt.h>

#define	CATNO_SYSERR	0x0000
#define	CATNO_SGIERR	0x1<<16
#define	CATNO_MASK	CATNO_SYSERR|CATNO_SGIERR

static const char irix_catalog[] = "uxsgierr";
static const char svr4_catalog[] = "uxsyserr";

extern const int _sys_index[];

extern struct errtable {
	int	errnum;
	int	msgidx;
	char	*msg;
} _sgi_errtable[], _sys_errtable[];


/*
 * __irixerror - return a string corresponding to an IRIX errno
 * after doing an appropriate catalog lookup.
 *
 */

const char *
__irixerror(int n)
{
	struct errtable	*ep;

	for (ep = &_sgi_errtable[0]; ep->errnum; ep++)
		if (ep->errnum == n)
		    return (__gtxt(irix_catalog, ep->msgidx, ep->msg));

	return (0);
}


/*
 * __svr4error - return a string corresponding to an errno
 * not in sys_errlist[] after doing an appropriate catalog lookup.
 */

const char *
__svr4error(int n)
{
	struct errtable	*ep;

	for (ep = &_sys_errtable[0]; ep->errnum; ep++)
		if (ep->errnum == n)
		    return (__gtxt(svr4_catalog, ep->msgidx, ep->msg));

	return (0);
}


/*
 * __sys_errlisterror - return a string corresponding to an errno
 * in sys_errlist[] after doing an appropriate catalog lookup.
 */

const char *
__sys_errlisterror(int n)
{
	int msgidx = _sys_index[n];
	const char *catalog;

	if (msgidx & CATNO_SGIERR) {
		catalog = irix_catalog;
		msgidx &= ~CATNO_SGIERR;		/* real index */
	} else
		catalog = svr4_catalog;

	return (__gtxt(catalog, msgidx, _sys_errlist[n]));
}
