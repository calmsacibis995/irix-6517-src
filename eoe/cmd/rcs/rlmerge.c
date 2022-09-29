/* three-way file merge internals */

/* Copyright 1991, 1992, 1993, 1994, 1995 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

#include "rcsbase.h"

	static char const *normalize_arg P((char const*,char**));
	static int libmerge
	    P((char const*, char const*[3], char const*[3], RCSTMP *));

	static char const *
normalize_arg(s, b)
	char const *s;
	char **b;
/*
 * If S looks like an option, prepend ./ to it.  Yield the result.
 * Set *B to the address of any storage that was allocated.
 */
{
	char *t;
	if (*s == '-') {
		*b = t = testalloc(strlen(s) + 3);
		VOID sprintf(t, ".%c%s", SLASH, s);
		return t;
	} else {
		*b = 0;
		return s;
	}
}

	int
rcsMerge(conn, hdl1, hdl2, hdl3, outhdl, diff3_flags)
	RCSCONN conn;
	RCSTMP hdl1, hdl2, hdl3, *outhdl;
	const char *diff3_flags;
{
	/* Perform a merge(1) of the changes from hdl2 to hdl3 into hdl1,
	 * producing a new output file, returned in outhdl.
	 *
	 * Returns: 0 (no overlaps)
	 *	    1 (overlaps)
	 *	    negative values (error codes)
	 */

	int rc;
	char *ptr;
	const char *argv[3];		/* names of files to merge */
	const char *edarg = NULL;	/* optional -A, -E, or -e */
	int labels = 0;			/* number of -L flags handled */
	const char *label[3];		/* -L flags to pass to merge */
	char *flags;			/* writable copy of diff3_flags */

	if ((rc = sigsetjmp(rcsApiEnv, 1)) != 0) return rc > 0 ? -rc : rc;

	if (rcsMapTmpHdlToName(hdl1, &argv[0]) < 0 ||
	    rcsMapTmpHdlToName(hdl2, &argv[1]) < 0 ||
	    rcsMapTmpHdlToName(hdl3, &argv[2]) < 0)
		return ERR_APPL_ARGS;

	if (diff3_flags && *diff3_flags) {
	    flags = str_save(diff3_flags);
	
	    /* Gather merge flags */
	    for (ptr = flags;;) {
		while(*ptr && isspace(*ptr)) ptr++;	/* skip whitespace */
		if (!*ptr) break;
		if (*ptr++ != '-') {
		    rc = ERR_APPL_ARGS;
		    break;
		}
		switch(*ptr) {
		case 'A': case 'E': case 'e':
		    if (edarg && edarg[1] != *ptr)
			error("-%c and -%c are incompatible", edarg[1], *ptr);
		    edarg = ptr-1;
		    ptr++;		/* Set ptr to where NULL belongs */
		    break;
		case 'L':
		    if (3 <= labels) {
			error("too many -L options");
			rc = ERR_APPL_ARGS;
			break;
		    }
		    /* Skip whitespace, if any, following -L */
		    while (*++ptr && isspace(*ptr));

		    /* -L must have argument */
		    if (!*ptr) {
			error("-L needs following argument");
			rc = ERR_APPL_ARGS;
			break;
		    }
		    label[labels++] = ptr;

		    /* Set ptr to where NULL terminator of label belongs */
		    while (*++ptr && !isspace(*ptr));
		    break;
		default:
		    error("bad option: -%c", *ptr);
		    rc = ERR_APPL_ARGS;
		    break;
		}
	        if (rc < 0) {if (flags) tfree(flags); return rc;}
		if (!*ptr) break;	/* we are at end of input string */
		*ptr++ = '\0';
	    }
	}

	/* Fill in remaining values (labels) */
	switch(labels) {
	case 0: label[0] = "handle1";
	case 1: label[1] = "handle2";
	case 2: label[2] = "handle3";
	default:
		break;
	}
		
	rc = libmerge(edarg, label, argv, outhdl);
	if (flags) tfree(flags);
	return rc;
}

	static int
libmerge(edarg, label, argv, outhdl)
	char const *edarg;
	char const *label[3];
	char const *argv[3];
	RCSTMP *outhdl;
/*
 * Do `merge [-p] EDARG -L l0 -L l1 -L l2 a0 a1 a2',
 * where the output goes into a temp file, returning its handle.
 * EDARG gives the editing type (e.g. "-A", or null for the default),
 * LABEL gives l0, l1 and l2, and ARGV gives a0, a1 and a2.
 * Yield 0 (no diff), 1 (differences), or ERR_INTERNAL.
 */
{
	register int i;
	char const *a[3];
	char *b[3];
	int rc;
	int fd;
	char *outname;

	for (i=3; 0<=--i; )
		a[i] = normalize_arg(argv[i], &b[i]);
	
	if (!edarg)
		edarg = "-E";
	
	/* Create temp output file, along with handle to return */
	if ((fd = rcsMakeTmpHdl(outhdl, &outname, S_IWUSR | S_IRUSR)) < 0) {
	    return fd;
	}
	close(fd);

/* Rely upon DIFF3 being available */
	rc = run(
		-1, outname,
		DIFF3, edarg, "-am",
		"-L", label[0],
		"-L", label[1],
		"-L", label[2],
		a[0], a[1], a[2], (char*)0
	);
	switch (rc) {
		case DIFF_SUCCESS:
			rc = 0;
			break;
		case DIFF_FAILURE:
			rc = 1;
			warn("conflicts during merge");
			break;
		default:
			rc = ERR_INTERNAL;
			break;
	}

	/* Free any malloc'd space used locally */
	for (i=3; 0<=--i; )
		if (b[i])
			tfree(b[i]);
	return rc;
}
