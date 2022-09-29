/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Return a string representation of an IP timestamp (in host order).
 */
#include <stdio.h>
#include "protocols/ip.h"

char *
ip_timestamp(n_long time)
{
	static char buf[sizeof "hh:mm:ss.mmm"];

	(void) sprintf(buf, "%02lu:%02lu:%02lu.%03lu",
		       (time / (60 * 60 * 1000)) % 24,
		       (time / (60 * 1000)) % 60,
		       (time / 1000) % 60,
		       time % 1000);
	return buf;
}
