/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Encode or decode an ARP header.
 */
#include <sys/types.h>
#include <sys/socket.h>		/* prerequisite of if_arp.h */
#include <net/if_arp.h>
#include "datastream.h"

int
ds_arphdr(DataStream *ds, struct arphdr *ar)
{
	return ds_u_short(ds, &ar->ar_hrd)
	    && ds_u_short(ds, &ar->ar_pro)
	    && ds_u_char(ds, &ar->ar_hln)
	    && ds_u_char(ds, &ar->ar_pln)
	    && ds_u_short(ds, &ar->ar_op);
}
