/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Reporter XDR Functions
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
#include "account.h"

bool_t
xdr_reportvalue(XDR *xdr, struct reportvalue **rp)
{
	struct reportvalue *rv = *rp;

	if (rv == 0) {
		switch (xdr->x_op) {
		    case XDR_ENCODE:
			return 0;

		    case XDR_DECODE:
			*rp = rv = new(struct reportvalue);
			rv->rv_value = 0;
			break;
		}
	}

	return xdr_collectkey(xdr, &rv->rv_key) 
	    && xdr_collectvaluechain(xdr, &rv->rv_value);
}

bool_t
xdr_reportlist(XDR *xdr, unsigned int *nel, struct reportvalue ***rl)
{
	struct reportvalue **rp;
	unsigned int i, n;

	if (!xdr_u_int(xdr, nel))
		return 0;

	n = *nel;
	rp = *rl;
	if (rp == 0) {
		switch (xdr->x_op) {
		    case XDR_ENCODE:
			return 0;

		    case XDR_DECODE:
			*rl = rp = vnew(n, struct reportvalue *);
			bzero(rp, n * sizeof *rp);
			break;
		}
	}

	for (i = 0; i < n; i++) {
		if (!xdr_reportvalue(xdr, &rp[i]))
			return 0;
	}
	return 1;
}

bool_t
xdrstdio_report(FILE *file, enum xdr_op op,
		unsigned long *a, unsigned long *m,
		time_t *start, time_t *end,
		unsigned int *nel, struct reportvalue ***rl)
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
			  && xdr_u_int(&xdr, &version);
			break;

		case XDR_DECODE:
			ok = xdr_u_int(&xdr, &magic)
			  && magic == COLLECT_MAGIC
			  && xdr_u_int(&xdr, &version)
			  && version == COLLECT_VERSION;
			break;
	}
	ok &= xdr_u_long(&xdr, a) && xdr_u_long(&xdr, m)
	   && xdr_long(&xdr, start) && xdr_long(&xdr, end)
	   && xdr_reportlist(&xdr, nel, rl);
	xdr_destroy(&xdr);
	return ok;
}
