/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Encode or decode a TCP header.
 */
#include <sys/types.h>
#include <netinet/tcp.h>
#include "datastream.h"
#include "protocols/ip.h"

int
ds_tcphdr(DataStream *ds, struct tcphdr *th)
{
	long val;

	if (!ds_u_short(ds, &th->th_sport)
	    || !ds_u_short(ds, &th->th_dport)
	    || !ds_tcp_seq(ds, &th->th_seq)
	    || !ds_tcp_seq(ds, &th->th_ack)) {
		return 0;
	}
	if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
		return 0;
	th->th_off = val;
	if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
		return 0;
	th->th_x2 = val;
	return ds_u_char(ds, &th->th_flags)
	    && ds_u_short(ds, &th->th_win)
	    && ds_u_short(ds, &th->th_sum)
	    && ds_u_short(ds, &th->th_urp);
}
