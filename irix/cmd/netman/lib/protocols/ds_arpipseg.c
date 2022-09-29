/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Encode or decode an IP-ARP segment.
 */
#include "datastream.h"
#include "protocols/arp.h"
#include "protocols/ether.h"
#include "protocols/ip.h"

int
ds_arpipseg(DataStream *ds, struct arpipseg *ai)
{
	return ds_etheraddr(ds, &ai->ai_sha)
	    && ds_in_addr(ds, &ai->ai_spa)
	    && ds_etheraddr(ds, &ai->ai_tha)
	    && ds_in_addr(ds, &ai->ai_tpa);
}
