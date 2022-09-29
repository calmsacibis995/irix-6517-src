/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Get snooper statistics.
 */
#include "exception.h"
#include "snooper.h"

int
sn_getstats(Snooper *sn, struct snoopstats *ss)
{
	struct rawstats rs;

	if (!sn_ioctl(sn, SIOCRAWSTATS, &rs)) {
		exc_raise(sn->sn_error, "cannot get snoop stats from %s",
			  sn->sn_name);
		return 0;
	}
	*ss = rs.rs_snoop;
	return 1;
}
