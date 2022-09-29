/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Collector I/O
 *
 *	$Revision: 1.3 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "index.h"
#include "collect.h"

int
save_entrylist(char *fn, unsigned long *a, unsigned long *m,
	       time_t *start, time_t *end, Entry **ep, unsigned int *nel)
{
	FILE *fp;
	char cmd[512];
	int ok;

	fp = fopen(fn, "w");
	if (fp == 0 || flock(fileno(fp), LOCK_EX) < 0)
		return 0;
	ok = xdrstdio_entrylist(fp, XDR_ENCODE, a, m, start, end, ep, nel);
	if (!ok)
		(void) unlink(fn);
	(void) flock(fileno(fp), LOCK_UN);
	fclose(fp);
	sprintf(cmd, "compress %s", fn);
	(void) system(cmd);
	return ok;
}
