/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Encode or decode a UDP header.
 */
#include <sys/types.h>
#include <netinet/udp.h>
#include "datastream.h"

int
ds_udphdr(DataStream *ds, struct udphdr *uh)
{
	return ds_u_short(ds, &uh->uh_sport)
	    && ds_u_short(ds, &uh->uh_dport)
	    && ds_u_short(ds, (u_short *)&uh->uh_ulen)
	    && ds_u_short(ds, &uh->uh_sum);
}
