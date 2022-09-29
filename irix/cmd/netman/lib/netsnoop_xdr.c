/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * 	Netsnoop XDR functions
 *
 *	$Revision: 1.6 $
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
#include "snoopd_rpc.h"
#include "snooper.h"

bool_t
xdr_snooppacketwrap(XDR *xdr, SnoopPacketWrap *spw)
{
	enum packettype { packet_data = 0, end_of_file = 1 } p;

	switch (xdr->x_op) {
	    case XDR_DECODE:
		if (!xdr_enum(xdr, (enum_t *) &p))
			return FALSE;
		if (p != packet_data) {
			spw->spw_len = 0;
			return TRUE;
		}
		break;

	    case XDR_ENCODE:
		if (spw->spw_len == 0) {
			p = end_of_file;
			return xdr_enum(xdr, (enum_t *) &p);
		}
		p = packet_data;
		if (!xdr_enum(xdr, (enum_t *) &p))
			return FALSE;
		break;
	}
	return xdr_snooppacket(xdr, spw->spw_sp, spw->spw_len, spw->spw_maxlen);
}
