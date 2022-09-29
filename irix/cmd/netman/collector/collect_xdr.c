/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Collector XDR Functions
 *
 *	$Revision: 1.3 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdio.h>
#include <sys/time.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "heap.h"
#include "index.h"
#include "collect.h"

bool_t
xdr_addr(XDR *xdr, struct addr *a)
{
	unsigned int len = sizeof a->a_eaddr;
	char *ea = (char *) a->a_eaddr.ea_vec;

	return xdr_bytes(xdr, &ea, &len, sizeof a->a_eaddr)
	    && xdr_u_short(xdr, &a->a_port)
	    && xdr_u_long(xdr, &a->a_naddr);
}

bool_t
xdr_collectkey(XDR *xdr, struct collectkey *ck)
{
	return xdr_addr(xdr, &ck->ck_src) && xdr_addr(xdr, &ck->ck_dst);
}

bool_t
xdr_collectvalue(XDR *xdr, struct collectvalue *cv)
{
	return xdr_u_int(xdr, (unsigned int *) &cv->cv_next)
	    && xdr_u_int(xdr, &cv->cv_protoid)
	    && xdr_counts(xdr, &cv->cv_count);
}

bool_t
xdr_collectvaluechain(XDR *xdr, struct collectvalue **cp)
{
	struct collectvalue *cv;

	switch (xdr->x_op) {
	    case XDR_ENCODE:
		for (cv = *cp; cv != 0; cv = cv->cv_next) {
			if (!xdr_collectvalue(xdr, cv))
				return 0;
		}
		break;

	    case XDR_DECODE:
		for ( ; ; ) {
			*cp = cv = new(struct collectvalue);
			if (!xdr_collectvalue(xdr, cv)) {
				delete(cv);
				*cp = 0;
				return 0;
			}
			if (cv->cv_next == 0)
				return 1;
			cp = &cv->cv_next;
		}

	    case XDR_FREE:
		for (cv = *cp; cv != 0; cv = *cp) {
			*cp = cv->cv_next;
			delete(cv);
		}
		return 1;
	}
	return 1;
}

bool_t
xdr_entrylist(XDR *xdr, Entry **ep, unsigned int *nel)
{
	unsigned int i, n;
	struct collectkey *ck;
	struct collectvalue *cv;

	if (!xdr_u_int(xdr, nel))
		return 0;
	n = *nel;
	switch (xdr->x_op) {
	     case XDR_ENCODE:
		for (i = 0; i < n; i++) {
			ck = (struct collectkey *) ep[i]->ent_key;
			cv = ep[i]->ent_value;
			if (!xdr_collectkey(xdr, ck)
			    || !xdr_collectvaluechain(xdr, &cv))
				return 0;
		}
		break;

	    default:
		return 0;
	}
	return 1;
}

bool_t
xdrstdio_entrylist(FILE *file, enum xdr_op op,
		   unsigned long *a, unsigned long *m,
		   time_t *start, time_t *end,
		   Entry **ep, unsigned int *nel)
{
	XDR xdr;
	unsigned int magic, version;
	int ok;

	xdrstdio_create(&xdr, file, op);
	switch (op) {
	    case XDR_ENCODE:
		magic = COLLECT_MAGIC;
		version = COLLECT_VERSION;
		ok = xdr_u_int(&xdr, &magic)
		     && xdr_u_int(&xdr, &version)
		     && xdr_u_long(&xdr, a)
		     && xdr_u_long(&xdr, m)
		     && xdr_long(&xdr, start)
		     && xdr_long(&xdr, end)
		     && xdr_entrylist(&xdr, ep, nel);
		break;

	    default:
		ok = 0;
		break;
	}
	xdr_destroy(&xdr);
	return ok;
}
