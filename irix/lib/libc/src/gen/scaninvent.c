#ident "$Revision: 1.4 $"
#ifdef __STDC__
	#pragma weak scaninvent = _scaninvent
#endif
/*
 * Copyright 1988 Silicon Graphics, Inc. All rights reserved.
 *
 * Hardware inventory scanner.
 */
#include "synonyms.h"
#include <invent.h>

int	_keepinvent;

int
scaninvent(int (*fun)(inventory_t *, void *), void *arg)
{
	inventory_t *ie;
	int rc;

	if (setinvent() < 0)
		return -1;
	rc = 0;
	while (ie = getinvent()) {
		rc = (*fun)(ie, arg);
		if (rc)
			break;
	}
	if (!_keepinvent)
		endinvent();
	return rc;
}
