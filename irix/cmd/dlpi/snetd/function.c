/*
 *  SpiderTCP Network Daemon
 *
 *      Control Interface Function Lookup
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.function.c
 *	@(#)function.c	1.7
 *
 *	Last delta created	17:06:08 2/4/91
 *	This file extracted	14:52:23 11/13/92
 *
 */

#ident "@(#)function.c	1.7	11/13/92"

#include "debug.h"
#include "function.h"
#include "utils.h"

function_t
flookup(name)
char	*name;
{
	function_t	f;

	for (f = cftab; f->fname; f++)
		if (streq(f->fname, name))
			return(f);

	return F_NULL;
}
