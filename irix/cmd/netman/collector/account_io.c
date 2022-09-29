/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Reporter I/O
 *
 *	$Revision: 1.6 $
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
#ifdef __sgi
#include <stdlib.h>
#endif
#include <strings.h>
#include <sys/types.h>
#include <sys/file.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "heap.h"
#include "account.h"

static int srccmp(const void *, const void *);
static int dstcmp(const void *, const void *);
static void resolve(struct reportvalue **r, unsigned int n,
		    unsigned long srvaddr, unsigned long mask, struct addr *a1);

int
save_report(char *fn, unsigned long *a, unsigned long *m,
	    time_t *start, time_t *end,
	    unsigned int *nel, struct reportvalue ***list)
{
	FILE *fp;
	char cmd[512];
	int ok;

	fp = fopen(fn, "w");
	if (fp == 0 || flock(fileno(fp), LOCK_EX) < 0)
		return 0;
	ok = xdrstdio_report(fp, XDR_ENCODE, a, m, start, end, nel, list);
	if (!ok)
		(void) unlink(fn);
	(void) flock(fileno(fp), LOCK_UN);
	fclose(fp);
	sprintf(cmd, "compress %s", fn);
	(void) system(cmd);
	return ok;
}

int
read_report(char *fn, unsigned long *a, unsigned long *m,
	    time_t *start, time_t *end, unsigned int *nel,
	    struct reportvalue ***sl, struct reportvalue ***dl)
{
	FILE *fp;
	int ok;
	unsigned int i, n, pipe;
	struct reportvalue **ul;
	struct addr *a0;

	if (sl == 0 && dl == 0)
		return 0;

	i = strlen(fn);
	if (i == 0)
		return 0;
	if (i < 2 || fn[i - 2] != '.' || fn[i - 1] != 'Z') {
		fp = fopen(fn, "r");
		if (fp == 0 || flock(fileno(fp), LOCK_SH) < 0)
			return 0;
		pipe = 0;
	} else {
		char cmd[512];

		sprintf(cmd, "zcat %s", fn);
		fp = popen(cmd, "r");
		if (fp == 0)
			return 0;
		pipe = 1;
	}
	ul = 0;
	ok = xdrstdio_report(fp, XDR_DECODE, a, m, start, end, nel, &ul);
	if (pipe != 0)
		pclose(fp);
	else {
		(void) flock(fileno(fp), LOCK_UN);
		fclose(fp);
	}
	if (!ok)
		return 0;

	n = *nel;
	for (i = 0; i < n; i++) {
		a0 = &ul[i]->rv_key.ck_src;
		if (a0->a_naddr == 0)
			resolve(ul, n, *a, *m, a0);
		a0 = &ul[i]->rv_key.ck_dst;
		if (a0->a_naddr == 0)
			resolve(ul, n, *a, *m, a0);
	}

	if (sl != 0) {
		*sl = ul;
		qsort(ul, n, sizeof *ul, srccmp);
	} else {
		*dl = ul;
		qsort(ul, n, sizeof *ul, dstcmp);
		return 1;
	}
	if (dl != 0) {
		*dl = ul = vnew(n, struct reportvalue *);
		bcopy(*sl, ul, n * sizeof *ul);
		qsort(ul, n, sizeof *ul, dstcmp);
	}
	return 1;
}

static int
srccmp(const void *r1, const void *r2)
{
	return keycmp(&((*(struct reportvalue **)r1)->rv_key),
		      &((*(struct reportvalue **)r2)->rv_key));
}

static int
dstcmp(const void *r1, const void *r2)
{
	const struct collectkey *c1 = &((*(struct reportvalue **)r1)->rv_key);
	const struct collectkey *c2 = &((*(struct reportvalue **)r2)->rv_key);
	int rv;

	rv = addrcmp(&c1->ck_dst, &c2->ck_dst);
	if (rv != 0)
		return rv;

	return addrcmp(&c1->ck_src, &c2->ck_src);
}

static void
resolve(struct reportvalue **r, unsigned int n,
	unsigned long srvaddr, unsigned long mask, struct addr *a1)
{
	struct addr *a2;
	unsigned int i;

	for (i = 0; i < n; i++) {
		a2 = &r[i]->rv_key.ck_src;
		if ((srvaddr & mask) == (a2->a_naddr & mask)
			&& bcmp(&a1->a_eaddr, &a2->a_eaddr,
				sizeof a1->a_eaddr) == 0) {
			a1->a_naddr = a2->a_naddr;
			break;
		}
		a2 = &r[i]->rv_key.ck_dst;
		if ((srvaddr & mask) == (a2->a_naddr & mask)
			&& bcmp(&a1->a_eaddr, &a2->a_eaddr,
				sizeof a1->a_eaddr) == 0) {
			a1->a_naddr = a2->a_naddr;
			break;
		}
	}
}
