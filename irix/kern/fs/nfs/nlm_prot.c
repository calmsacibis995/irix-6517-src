/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)xdr_nlm.c	1.8	93/07/09 SMI"   SVr4.0 1.1 */
/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */
/*
 */

#if 0
static char sccsid[] = "@(#)xdr_nlm.c	1.3 90/07/23 NFSSRC4.0 1.7 88/02/07 SMI";
#endif

/* 
 * This file was originally generated using rpcgen from nlm_prot.x.
 */

#include <rpc/types.h>          /* some typedefs */
#include <rpc/xdr.h>
#include <rpcsvc/nlm_prot.h>


bool_t
xdr_nlm_stats(xdrs, objp)
	XDR *xdrs;
	nlm_stats *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_holder(xdrs, objp)
	XDR *xdrs;
	nlm_holder *objp;
{
	if (!xdr_bool(xdrs, &objp->exclusive)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->svid)) {
		return (FALSE);
	}
	if (!xdr_netobj(xdrs, &objp->oh)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->l_offset)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->l_len)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_testrply(xdrs, objp)
	XDR *xdrs;
	nlm_testrply *objp;
{
	if (!xdr_nlm_stats(xdrs, &objp->stat)) {
		return (FALSE);
	}
	switch (objp->stat) {
	case nlm_denied:
		if (!xdr_nlm_holder(xdrs, &objp->nlm_testrply_u.holder)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}




bool_t
xdr_nlm_stat(xdrs, objp)
	XDR *xdrs;
	nlm_stat *objp;
{
	if (!xdr_nlm_stats(xdrs, &objp->stat)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_res(xdrs, objp)
	XDR *xdrs;
	nlm_res *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_nlm_stat(xdrs, &objp->stat)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_testres(xdrs, objp)
	XDR *xdrs;
	nlm_testres *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_nlm_testrply(xdrs, &objp->stat)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_lock(xdrs, objp)
	XDR *xdrs;
	nlm_lock *objp;
{
	if (!xdr_string(xdrs, &objp->caller_name, LM_MAXSTRLEN)) {
		return (FALSE);
	}
	if (!xdr_netobj(xdrs, &objp->fh)) {
		return (FALSE);
	}
	if (!xdr_netobj(xdrs, &objp->oh)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->svid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->l_offset)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->l_len)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_lockargs(xdrs, objp)
	XDR *xdrs;
	nlm_lockargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->block)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->exclusive)) {
		return (FALSE);
	}
	if (!xdr_nlm_lock(xdrs, &objp->alock)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->reclaim)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->state)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_cancargs(xdrs, objp)
	XDR *xdrs;
	nlm_cancargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->block)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->exclusive)) {
		return (FALSE);
	}
	if (!xdr_nlm_lock(xdrs, &objp->alock)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_testargs(xdrs, objp)
	XDR *xdrs;
	nlm_testargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->exclusive)) {
		return (FALSE);
	}
	if (!xdr_nlm_lock(xdrs, &objp->alock)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_unlockargs(xdrs, objp)
	XDR *xdrs;
	nlm_unlockargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_nlm_lock(xdrs, &objp->alock)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_fsh_mode(xdrs, objp)
	XDR *xdrs;
	fsh_mode *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_fsh_access(xdrs, objp)
	XDR *xdrs;
	fsh_access *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_share(xdrs, objp)
	XDR *xdrs;
	nlm_share *objp;
{
	if (!xdr_string(xdrs, &objp->caller_name, LM_MAXSTRLEN)) {
		return (FALSE);
	}
	if (!xdr_netobj(xdrs, &objp->fh)) {
		return (FALSE);
	}
	if (!xdr_netobj(xdrs, &objp->oh)) {
		return (FALSE);
	}
	if (!xdr_fsh_mode(xdrs, &objp->mode)) {
		return (FALSE);
	}
	if (!xdr_fsh_access(xdrs, &objp->access)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_shareargs(xdrs, objp)
	XDR *xdrs;
	nlm_shareargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_nlm_share(xdrs, &objp->share)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->reclaim)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_shareres(xdrs, objp)
	XDR *xdrs;
	nlm_shareres *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_nlm_stats(xdrs, &objp->stat)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->sequence)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nlm_notify(xdrs, objp)
	XDR *xdrs;
	nlm_notify *objp;
{
	if (!xdr_string(xdrs, &objp->name, MAXNAMELEN)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->state)) {
		return (FALSE);
	}
	return (TRUE);
}


bool_t
xdr_nlm4_stats(xdrs, objp)
	register XDR *xdrs;
	nlm4_stats *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_holder(xdrs, objp)
	register XDR *xdrs;
	nlm4_holder *objp;
{
	if (!xdr_bool(xdrs, &objp->exclusive))
		return (FALSE);
	if (!xdr_int(xdrs, &objp->svid))
		return (FALSE);
	if (!xdr_netobj(xdrs, &objp->oh))
		return (FALSE);
	if (!xdr_u_longlong_t(xdrs, &objp->l_offset))
		return (FALSE);
	if (!xdr_u_longlong_t(xdrs, &objp->l_len))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_testrply(xdrs, objp)
	register XDR *xdrs;
	nlm4_testrply *objp;
{
	if (!xdr_nlm4_stats(xdrs, &objp->stat))
		return (FALSE);
	switch (objp->stat) {
	case NLM4_DENIED:
		if (!xdr_nlm4_holder(xdrs, &objp->nlm4_testrply_u.holder))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_nlm4_stat(xdrs, objp)
	register XDR *xdrs;
	nlm4_stat *objp;
{
	if (!xdr_nlm4_stats(xdrs, &objp->stat))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_res(xdrs, objp)
	register XDR *xdrs;
	nlm4_res *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_nlm4_stat(xdrs, &objp->stat))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_testres(xdrs, objp)
	register XDR *xdrs;
	nlm4_testres *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_nlm4_testrply(xdrs, &objp->stat))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_lock(xdrs, objp)
	register XDR *xdrs;
	nlm4_lock *objp;
{
	if (!xdr_string(xdrs, &objp->caller_name, LM_MAXSTRLEN))
		return (FALSE);
	if (!xdr_netobj(xdrs, &objp->fh))
		return (FALSE);
	if (!xdr_netobj(xdrs, &objp->oh))
		return (FALSE);
	if (!xdr_int(xdrs, &objp->svid))
		return (FALSE);
	if (!xdr_u_longlong_t(xdrs, &objp->l_offset))
		return (FALSE);
	if (!xdr_u_longlong_t(xdrs, &objp->l_len))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_lockargs(xdrs, objp)
	register XDR *xdrs;
	nlm4_lockargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->block))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->exclusive))
		return (FALSE);
	if (!xdr_nlm4_lock(xdrs, &objp->alock))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->reclaim))
		return (FALSE);
	if (!xdr_int(xdrs, &objp->state))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_cancargs(xdrs, objp)
	register XDR *xdrs;
	nlm4_cancargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->block))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->exclusive))
		return (FALSE);
	if (!xdr_nlm4_lock(xdrs, &objp->alock))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_testargs(xdrs, objp)
	register XDR *xdrs;
	nlm4_testargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->exclusive))
		return (FALSE);
	if (!xdr_nlm4_lock(xdrs, &objp->alock))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_unlockargs(xdrs, objp)
	register XDR *xdrs;
	nlm4_unlockargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_nlm4_lock(xdrs, &objp->alock))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_fsh4_mode(xdrs, objp)
	register XDR *xdrs;
	fsh4_mode *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_fsh4_access(xdrs, objp)
	register XDR *xdrs;
	fsh4_access *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_share(xdrs, objp)
	register XDR *xdrs;
	nlm4_share *objp;
{
	if (!xdr_string(xdrs, &objp->caller_name, LM_MAXSTRLEN))
		return (FALSE);
	if (!xdr_netobj(xdrs, &objp->fh))
		return (FALSE);
	if (!xdr_netobj(xdrs, &objp->oh))
		return (FALSE);
	if (!xdr_fsh4_mode(xdrs, &objp->mode))
		return (FALSE);
	if (!xdr_fsh4_access(xdrs, &objp->access))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_shareargs(xdrs, objp)
	register XDR *xdrs;
	nlm4_shareargs *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_nlm4_share(xdrs, &objp->share))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->reclaim))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_shareres(xdrs, objp)
	register XDR *xdrs;
	nlm4_shareres *objp;
{
	if (!xdr_netobj(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_nlm4_stats(xdrs, &objp->stat))
		return (FALSE);
	if (!xdr_int(xdrs, &objp->sequence))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nlm4_notify(xdrs, objp)
	register XDR *xdrs;
	nlm4_notify *objp;
{
	if (!xdr_string(xdrs, &objp->name, MAXNAMELEN))
		return (FALSE);
	if (!xdr_int(xdrs, &objp->state))
		return (FALSE);
	return (TRUE);
}
