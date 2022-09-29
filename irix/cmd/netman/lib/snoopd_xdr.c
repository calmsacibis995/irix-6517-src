/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * 	Snoop protocol XDR functions
 *
 *	$Revision: 1.7 $
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
#include <netinet/in.h>
#include <net/raw.h>
#include "heap.h"
#include "macros.h"
#include "protocol.h"
#include "snoopd_rpc.h"

bool_t xdr_expr(XDR *, struct expr **, struct scope *);

/*
 * Since xdr_string doesn't do NULL pointers, do it here
 */
static bool_t
xdr_charpointer(XDR *xdr, char **sp)
{
	char *s;
	u_int len;

	s = *sp;
	switch (xdr->x_op) {
	  case XDR_ENCODE:
		len = (s == 0) ? 0 : strlen(s);
		if (!xdr_u_int(xdr, &len))
			return FALSE;
		break;

	  case XDR_DECODE:
		if (!xdr_u_int(xdr, &len))
			return FALSE;

		if (len == 0) {
			*sp = 0;
			return TRUE;
		}

		s = vnew(len + 1, char);
		s[len] = '\0';
		*sp = s;
		break;

	  case XDR_FREE:
		if (s != 0) {
			delete(s);
			*sp = 0;
		}
		return TRUE;
	}

	return xdr_opaque(xdr, s, len);
}

/*
 * XDR a subscription request
 */
bool_t
xdr_subreq(XDR *xdr, struct subreq *sr)
{
	return xdr_charpointer(xdr, &sr->sr_interface)
	    && xdr_enum(xdr, (enum_t *)&sr->sr_service)
	    && xdr_u_int(xdr, &sr->sr_buffer)
	    && xdr_u_int(xdr, &sr->sr_count)
	    && xdr_u_int(xdr, &sr->sr_interval);
}

/*
 * XDR a status code and an integer value
 */
bool_t
xdr_intres(XDR *xdr, struct intres *ir)
{
	if (!xdr_enum(xdr, (enum_t *)&ir->ir_status))
		return FALSE;
	switch (ir->ir_status) {
	  case SNOOP_OK:
		return xdr_int(xdr, &ir->ir_value);
	  case SNOOPERR_BADF:
		return xdr_charpointer(xdr, &ir->ir_message);
	  default:
		return TRUE;
	}
}

/*
 * XDR an expression
 */
bool_t
xdr_expression(XDR *xdr, struct expression *exp)
{
	return xdr_expr(xdr, &exp->exp_expr, &exp->exp_proto->pr_scope);
}

/*
 * XDR snoopstats
 */
bool_t
xdr_snoopstats(XDR *xdr, struct snoopstats *ss)
{
	return xdr_u_long(xdr, &ss->ss_seq)
	    && xdr_u_long(xdr, &ss->ss_ifdrops)
	    && xdr_u_long(xdr, &ss->ss_sbdrops);
}

/*
 * XDR a sockaddr
 */
bool_t
xdr_sockaddr(XDR *xdr, struct sockaddr *sa)
{
	if (!xdr_u_short(xdr, &sa->sa_family))
		return FALSE;

	switch (sa->sa_family) {
	  case AF_UNSPEC:
	  case AF_RAW:
		return xdr_opaque(xdr, sa->sa_data, sizeof sa->sa_data);

	  case AF_INET:
#define	sin	((struct sockaddr_in *)sa)
		return xdr_u_short(xdr, &sin->sin_port)
		    && xdr_u_long(xdr, &sin->sin_addr.s_addr)
		    && xdr_opaque(xdr, sin->sin_zero, sizeof sin->sin_zero);
#undef	sin

	  default:
		return FALSE;
	}
}

/*
 * XDR a getaddr argument struct
 */
bool_t
xdr_getaddrarg(XDR *xdr, struct getaddrarg *gaa)
{
	return xdr_enum(xdr, (enum_t *)&gaa->gaa_cmd)
	    && xdr_sockaddr(xdr, &gaa->gaa_addr);
}

/*
 * XDR a getaddr result struct
 */
bool_t
xdr_getaddrres(XDR *xdr, struct getaddrres *gar)
{
	return xdr_enum(xdr, (enum_t *)&gar->gar_status)
	    && xdr_sockaddr(xdr, &gar->gar_addr);
}
