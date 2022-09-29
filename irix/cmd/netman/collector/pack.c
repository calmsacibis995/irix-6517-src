/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Collapse many Data Collector files into 1
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
#include <strings.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/file.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "macros.h"
#include "heap.h"
#include "exception.h"
#include "account.h"

static void merge_report(struct reportvalue **, unsigned int,
			 struct reportvalue **, unsigned int,
			 struct reportvalue ***, unsigned int *);
static void merge_value(struct collectvalue **, struct collectvalue *);

main(int argc, char** argv)
{
	struct reportvalue **rp1, **rp2, **rp3;
	unsigned int n1, n2, n3;
	char *filename;
	time_t starttime, endtime;
	unsigned int verbose, remove;
	unsigned long addr, mask;
	int n;
	char path[_POSIX_PATH_MAX], *dir;
	static char usage[] ="usage: netpack [-p path] [-rv] file1 file2 ...\n";
	extern int _yp_disabled;
	extern char *optarg;
	extern int optind;

	exc_progname = "netpack";
	exc_autofail = 1;

#ifdef __sgi
	_yp_disabled = 1;
#endif
	initprotocols();
#ifdef __sgi
	_yp_disabled = 0;
#endif

	verbose = 0;
	remove = 0;
	path[0] = '\0';
	while ((n = getopt(argc, argv, "p:rv")) != -1) {
		switch (n) {
		    case 'p':
			strcpy(path, optarg);
			break;
		    case 'r':
			remove = 1;
			break;
		    case 'v':
			verbose++;
			break;
		    default:
			fputs(usage, stderr);
			exit(EINVAL);
		}
	}

	if (optind >= argc) {
		fputs(usage, stderr);
		exit(EINVAL);
	}

	n = optind;
	filename = argv[optind++];
	rp1 = 0;
	if (verbose)
		fprintf(stderr, "packing %s...\n", filename);
	if (!read_report(filename, &addr, &mask,
			 &starttime, &endtime,
			 &n1, &rp1, 0))
		exc_raise(errno, "could not read \"%s\"", filename);

	rp2 = rp3 = 0;
	for ( ; optind < argc; optind++) {
		unsigned long a, m;
		time_t s, e;

		filename = argv[optind];
		if (verbose)
			fprintf(stderr, "packing %s...\n", filename);
		if (!read_report(filename, &a, &m, &s, &e, &n2, &rp2, 0))
			exc_raise(errno, "could not read \"%s\"", filename);

		if (starttime != e+1 && endtime != s-1)
			fprintf(stderr,
				"warning: %s: files not contiguous in time\n",
				filename);

		starttime = MIN(s, starttime);
		endtime = MAX(e, endtime);

		merge_report(rp1, n1, rp2, n2, &rp3, &n3);
		delete(rp1);
		delete(rp2);
		rp1 = rp3;
		n1 = n3;
	}

	if (remove) {
		for ( ; n < argc; n++) {
			filename = argv[n];
			if (unlink(filename) != 0)
				exc_raise(errno,
					  "could not unlink \"%s\"", filename);
		}
	}

	dir = &path[strlen(path)];
	if (dir != path) {
		if (*dir != '/')
			*dir++ = '/';
	} else {
		char cwd[_POSIX_PATH_MAX], *ddir, *e;

		/* See if we are in a directory */
		if (getcwd(cwd, _POSIX_PATH_MAX) == 0)
			exc_raise(errno, "could not get current directory");
		else {
			ddir = getdirectory(&starttime, &endtime);
			e = strchr(ddir, '/');
			if (e != 0) {
				*e = '\0';
				e = strstr(cwd, ddir);
				if (e != 0) {
					*e = '\0';
					strcpy(dir, cwd);
					dir = &path[strlen(path)];
				}
			}
		}
	}
	strcpy(dir, getdirectory(&starttime, &endtime));
	if (!createdirectory(path))
		exc_raise(errno, "could not create data directory");
	strcpy(dir, getpath(&starttime, &endtime));
	if (verbose)
		fprintf(stderr, "writing %s...\n", dir);
	if (!save_report(path, &addr, &mask, &starttime, &endtime, &n1, &rp1))
		exc_raise(errno, "could not save \"%s\"", path);

	exit(0);
}

static void
merge_report(struct reportvalue **rp1, unsigned int n1,
	     struct reportvalue **rp2, unsigned int n2,
	     struct reportvalue ***rr, unsigned int *nr)
{
	unsigned int i1, i2, i3, n3;
	int z;
	struct reportvalue **rp3;

	n3 = n1 + n2;
	*rr = rp3 = vnew(n3, struct reportvalue *);

	i1 = i2 = 0;
	for (i3 = 0; i3 < n3; i3++) {
		if (i1 == n1) {
			while (i2 != n2)
				rp3[i3++] = rp2[i2++];
			break;
		}
		if (i2 == n2) {
			while (i1 != n1)
				rp3[i3++] = rp1[i1++];
			break;
		}
		z = keycmp(&rp1[i1]->rv_key, &rp2[i2]->rv_key);
		if (z < 0)
			rp3[i3] = rp1[i1++];
		else if (z > 0)
			rp3[i3] = rp2[i2++];
		else {
			rp3[i3] = rp1[i1++];
			merge_value(&rp3[i3]->rv_value, rp2[i2++]->rv_value);
			if (i1 == n1 && i2 == n2)
				break;
		}
	}
	*nr = i3;
}

static void
merge_value(struct collectvalue **cp, struct collectvalue *newcv)
{
	struct collectvalue *cv, *nnewcv;
	int p;

	for ( ; newcv != 0; newcv = nnewcv) {
		p = newcv->cv_protoid;
		cv = 0;

		for ( ; *cp != 0; cp = &(*cp)->cv_next) {
			if ((*cp)->cv_protoid < p)
				continue;
			if ((*cp)->cv_protoid == p)
				cv = *cp;
			break;
		}
		if (cv == 0) {
			cv = new(struct collectvalue);
			cv->cv_next = *cp;
			*cp = cv;
			cv->cv_protoid = p;
			cv->cv_count.c_packets = newcv->cv_count.c_packets;
			cv->cv_count.c_bytes = newcv->cv_count.c_bytes;
		} else {
			cv->cv_count.c_packets += newcv->cv_count.c_packets;
			cv->cv_count.c_bytes += newcv->cv_count.c_bytes;
		}

		nnewcv = newcv->cv_next;
		delete(newcv);
	}
}
