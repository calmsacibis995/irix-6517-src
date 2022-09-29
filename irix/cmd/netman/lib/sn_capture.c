/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snooper capture control operations.
 */
#include "exception.h"
#include "snooper.h"

int
sn_startcapture(Snooper *sn)
{
	int on = 1;

	if (!sn_ioctl(sn, SIOCSNOOPING, &on)) {
		exc_raise(sn->sn_error, "cannot start snooping on %s",
			  sn->sn_name);
		return 0;
	}
	return 1;
}

int
sn_stopcapture(Snooper *sn)
{
	int off = 0;

	if (!sn_ioctl(sn, SIOCSNOOPING, &off)) {
		exc_raise(sn->sn_error, "cannot stop snooping on %s",
			  sn->sn_name);
		return 0;
	}
	return 1;
}
