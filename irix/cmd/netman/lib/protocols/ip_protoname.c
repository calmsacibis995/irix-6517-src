/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Return a pointer to static store containing proto's name or decimal
 * string representation.
 */
#include <netdb.h>
#include <stdio.h>

char *
ip_protoname(int proto)
{
	struct protoent *pp;
	static char buf[sizeof "ddd"];

	pp = getprotobynumber(proto);
	if (pp)
		return pp->p_name;
	(void) sprintf(buf, "%-3d", proto);
	return buf;
}
