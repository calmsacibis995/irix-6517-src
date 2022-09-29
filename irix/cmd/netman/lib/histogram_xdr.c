/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * 	Histogram XDR functions
 *
 *	$Revision: 1.5 $
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
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "histogram.h"
#include "snoopd_rpc.h"

bool_t
xdr_counts(XDR* xdr, struct counts* c)
{
        return xdr_float(xdr, &c->c_packets) && xdr_float(xdr, &c->c_bytes);
}

bool_t
xdr_histogram(XDR* xdr, struct histogram *h)
{
	struct counts *c = h->h_count;

	return xdr_long(xdr, &h->h_timestamp.tv_sec) &&
	       xdr_long(xdr, &h->h_timestamp.tv_usec) &&
	       xdr_array(xdr, (char **) &c,
			 &h->h_bins, HIST_MAXBINS, sizeof *c, xdr_counts);
}
