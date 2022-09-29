/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Set snooper error filter.
 */
#include "exception.h"
#include "snooper.h"

int
sn_seterrflags(Snooper *sn, int flags)
{
	if (!sn_ioctl(sn, SIOCERRSNOOP, &flags)) {
		exc_raise(sn->sn_error, "cannot snoop for errors on %s",
			  sn->sn_name);
		return 0;
	}
	sn->sn_errflags = flags;
	return 1;
}
