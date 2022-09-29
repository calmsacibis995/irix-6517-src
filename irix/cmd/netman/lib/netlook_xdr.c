/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * 	NetLook XDR functions
 *
 *	$Revision: 1.4 $
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

#include <values.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <protocols/ether.h>
#include "netlook.h"

/*
 * XDR a NetLook endpoint structure
 */
static bool_t
xdr_nlendpoint(XDR *xdr, struct nlendpoint *nlep)
{
	char* nm = nlep->nlep_name;
	return (xdr_opaque(xdr, &nlep->nlep_eaddr, sizeof nlep->nlep_eaddr) &&
		xdr_u_short(xdr, &nlep->nlep_port) &&
		xdr_u_long(xdr, &nlep->nlep_naddr) &&
		xdr_string(xdr, &nm, NLP_NAMELEN));
}

/*
 * XDR a NetLook snooped packet structure
 */
static bool_t
xdr_nlspacket(XDR *xdr, struct nlspacket *nlsp)
{
	unsigned short *p = nlsp->nlsp_protolist;

	if (!xdr_u_short(xdr, &nlsp->nlsp_type) ||
	    !xdr_u_short(xdr, &nlsp->nlsp_length) ||
	    !xdr_long(xdr, &nlsp->nlsp_timestamp.tv_sec) ||
	    !xdr_long(xdr, &nlsp->nlsp_timestamp.tv_usec))
		return 0;

	if (!xdr_array(xdr, (char **) &p,
			&nlsp->nlsp_protocols, NLP_MAXPROTOS,
			sizeof nlsp->nlsp_protolist[0], xdr_u_short))
		return 0;

	return xdr_nlendpoint(xdr, &nlsp->nlsp_src) &&
			xdr_nlendpoint(xdr, &nlsp->nlsp_dst);
}

/*
 * XDR a NetLook packet structure
 */
bool_t
xdr_nlpacket(XDR *xdr, struct nlpacket *nlp)
{
	struct nlspacket *nlsp = nlp->nlp_nlsp;

	if (!xdr_u_long(xdr, &nlp->nlp_snoophost) ||
	    !xdr_u_short(xdr, &nlp->nlp_version) ||
	    !xdr_u_short(xdr, &nlp->nlp_type))
		return 0;

	if (nlp->nlp_version != NLP_VERSION)
		return 0;

	switch (nlp->nlp_type) {
	    case NLP_ENDOFDATA:
	    case NLP_SNOOPDATA:
		if (!xdr_array(xdr, (char **) &nlsp,
				&nlp->nlp_count, MAXINT,
				sizeof nlp->nlp_nlsp[0], xdr_nlspacket))
			return 0;
		break;
	}

	return 1;
}
