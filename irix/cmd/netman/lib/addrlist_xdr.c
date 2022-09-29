/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * 	AddrList XDR functions
 *
 *	$Revision: 1.2 $
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
#include <addrlist.h>

/*
 * XDR a NetLook endpoint structure
 */
static bool_t
xdr_alkey(XDR *xdr, struct alkey *k)
{
	char *n = k->alk_name;
	return (xdr_opaque(xdr, &k->alk_paddr, sizeof k->alk_paddr)
		&& xdr_string(xdr, &n, AL_NAMELEN)
		&& xdr_u_long(xdr, &k->alk_naddr));
}

/*
 * XDR a NetLook snooped packet structure
 */
static bool_t
xdr_alentry(XDR *xdr, struct alentry *e)
{
	return (xdr_alkey(xdr, &e->ale_src)
		&& xdr_alkey(xdr, &e->ale_dst)
		&& xdr_counts(xdr, &e->ale_count));
}

/*
 * XDR an AddrList packet structure
 */
bool_t
xdr_alpacket(XDR *xdr, struct alpacket *al)
{
	struct alentry *e = al->al_entry;

	if (!xdr_u_short(xdr, &al->al_version))
		return 0;
	if (al->al_version != AL_VERSION)
		return 0;

	if (!xdr_u_short(xdr, &al->al_type)
	    || !xdr_u_long(xdr, &al->al_source)
	    || !xdr_long(xdr, &al->al_timestamp.tv_sec)
	    || !xdr_long(xdr, &al->al_timestamp.tv_usec))
		return 0;

	switch (al->al_type) {
	    case AL_ENDOFDATA:
	    case AL_DATATYPE:
		if (!xdr_array(xdr, (char **) &e,
				&al->al_nentries, AL_MAXENTRIES,
				sizeof al->al_entry[0], xdr_alentry))
			return 0;
		break;
	}

	return 1;
}
