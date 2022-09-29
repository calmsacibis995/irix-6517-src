/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Encode or decode an IP header, without options.
 */
#include "datastream.h"
#include "protocols/ip.h"

int
ds_ip(DataStream *ds, struct ip *ip)
{
	long val;

	if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
		return 0;
	ip->ip_hl = val;
	if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
		return 0;
	ip->ip_v = val;
	return ds_u_char(ds, &ip->ip_tos)
	    && ds_u_short(ds, (u_short *)&ip->ip_len)
	    && ds_u_short(ds, &ip->ip_id)
	    && ds_u_short(ds, (u_short *)&ip->ip_off)
	    && ds_u_char(ds, &ip->ip_ttl)
	    && ds_u_char(ds, &ip->ip_p)
	    && ds_u_short(ds, &ip->ip_sum)
	    && ds_in_addr(ds, &ip->ip_src)
	    && ds_in_addr(ds, &ip->ip_dst);
}
