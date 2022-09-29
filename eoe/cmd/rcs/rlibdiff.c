/* Compare RCS revisions.  */

/* Copyright 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
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

/*
 * $Log: rlibdiff.c,v $
 * Revision 1.1  1996/07/10 19:45:47  msf
 * Initial revision
 *
 * Revision 5.23  1996/07/10 17:47:45  msf
 * Bug fix: allocate correct number of argv slots.
 *
 * Revision 5.22  1996/07/08 20:17:13  msf
 * Make rcsDiff into a function, rather than a main program.
 *
 */
#include "rcsbase.h"
#include "rcspriv.h"

#define WHITESPACE " \t\n"

#if DIFF_L
static char const *setup_label P((struct buf*,char const*,char const[datesize]));
#endif

static struct stat workstat;
static bool expandbinary;

	int
rcsDiff(conn, hdl1, hdl2, hdlout, diff_flags)
	RCSCONN conn;
	RCSTMP hdl1, hdl2, *hdlout;
	const char *diff_flags;
{
	/* Perform a diff(1) of two files represented by file handles.
	 * The output is stored in another temp file, and its file handle
	 * is returned.
	 *
	 * Returns: 0 (No change)
	 *	    1 (Differences)
	 *	    negative error codes on failure
	 */

#if DIFF_L
    static struct buf labelbuf[2];
    int file_labels;
    char const **diff_label1, **diff_label2;
    char date2[datesize];
#endif
    char const *cov[10 + !DIFF_L];
    char const **diffv, **diffp, **diffpend;	/* argv for subsidiary diff */
    static int no_diff_means_no_output;
    int rc;					/* return code */
    int numflags;				/* number of flags */
    char *flags;				/* R/W copy of diff_flags */
    register c;
    char *ptr;					/* ptr to walk through flags */
    char *dcp;					/* in-place copy ptr */
    const char *fname1;				/* name of file for hdl1 */
    const char *fname2;				/* name of file for hdl2 */
    char *arg;					/* beginning of current arg */
    int fd;					/* file desc. of output file */
    char *outname;				/* file name of output file */

    /* Memory heap managed locally (since we do not work with archive files,
     * the f* memory management routines will fail here)
     */
    static struct rcsHeap localHeap = {NULL, NULL, NULL};
    static int has_been_called = false;

#if DIFF_L
    file_labels = 0;
#endif
    no_diff_means_no_output = true;

    /* In case of internal core code error, return fatal error
     * to caller, rather than exiting.
     */
    if ((rc = sigsetjmp(rcsApiEnv, 1)) != 0) return rc > 0 ? -rc : rc;

    if (has_been_called) rcsHeapFree(&localHeap);

    if (rcsMapTmpHdlToName(hdl1, &fname1) < 0 ||
	rcsMapTmpHdlToName(hdl2, &fname2) < 0)
		return ERR_APPL_ARGS;
    has_been_called = true;
    expandbinary = false;

   /* Copy diff flags to a writable buffer
    * (we will replace whitespace with NULLs, to produce an argv array)
    */
   if (diff_flags && *diff_flags) {
	flags = hstr_save(&localHeap, diff_flags);
	/* Count number of flags; be conservative (overestimate) */
        for (numflags = 1, ptr = flags; ptr = strpbrk(ptr, WHITESPACE);
	     numflags++) {
	    while (*ptr && isspace(*ptr)) *ptr++;
	}
   }
   else {
	flags = "";
	numflags = 0;
   }

   /*
    * Room for runv extra + args [+ --binary] [+ 2 labels]
    * + 2 files + 1 trailing null.
    */
    if (!(diffv = 
        rcsHeapAlloc(&localHeap, sizeof(const char *) * 
		     (2 + numflags + !!OPEN_O_BINARY + 2*DIFF_L + 3) )))
	    faterror("Out of Memory");
    diffp = diffv + 1;
    *diffp++ = DIFF;

    /* gather diff flags */
    for (ptr = flags;;) {
	while (*ptr && isspace(*ptr)) ptr++;	/* skip whitespace */
	if (!*ptr) break;
	arg = ptr;				/* remember beginning of flag */
	if (*ptr++ != '-') return ERR_APPL_ARGS;
	dcp = ptr;

	/* Allow multiple flags, e.g. -bcD exp */
	while ((c = *ptr++) && !isspace(c)) switch (c) {

	    /* D, C, F, I, L, W are GNU diff options which require
	     * an argument which may immediately follow the flag (no
	     * whitespace).  We verify that there is an argument following,
	     * with or without the whitespace.
	     */
	    case '-': case 'D':
		    no_diff_means_no_output = false;
		    /* fall into */
	    case 'C': case 'F': case 'I': case 'L': case 'W':
#if DIFF_L
		    if (c == 'L'  &&  ++file_labels == 2)
			faterror("too many -L options");
#endif
		    *dcp++ = c;
		    /* Continue copying in place, eliminating whitespace
		     * (if any) between flag and argument
		     */
		    while (*ptr && isspace(*ptr)) ptr++;  /* skip whitespace */
		    if (*ptr) {
			do *dcp++ = *ptr++;
			while (*ptr && !isspace(*ptr));
		    }
		    else {
			faterror("-%c needs following argument", c);
		    }
		    break;

	    case 'y':
		    no_diff_means_no_output = false;
		    /* fall into */
	    case 'B': case 'H':
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
	    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	    case 'h': case 'i': case 'n': case 'p':
	    case 't': case 'u': case 'w':
		    *dcp++ = c;
		    break;

	    /* Allow -kb to indicate a binary diff */
	    case 'k':
		    if (*ptr++ == 'b' && (!*ptr || isspace(*ptr))) {
			expandbinary = true;
			goto option_handled;
		    }
		    /* fall into */
	    default:
	    unknown:
		    error("unknown option: -%c", c);
		    
	    };

      option_handled:
	    if (dcp != arg+1) {  /* not a naked '-', or -k, or  */
		*dcp = '\0';
		*diffp++ = arg;
	    }
    } /* end of option processing */


#if DIFF_L
    diff_label1 = diff_label2 = 0;
    if (file_labels < 2) {
	    if (!file_labels)
		    diff_label1 = diffp++;
	    diff_label2 = diffp++;
    }
#endif
    diffpend = diffp;

    do {
#if DIFF_L
	    if (diff_label1)
	    {
		if (stat(fname1, &workstat) < 0) {
		    rc = ERR_INTERNAL;
		    break;
		}
		time2date(workstat.st_mtime, date2);
		*diff_label1 = setup_label(&labelbuf[0], "handle1", date2);
	    }
#endif
#if DIFF_L
	    if (diff_label2)
	    {
		if (stat(fname2, &workstat) < 0) {
		    rc  = ERR_INTERNAL;
		    break;
		}
		time2date(workstat.st_mtime, date2);
		*diff_label2 = setup_label(&labelbuf[1], "handle2", date2);
	    }
#endif

	    diffp = diffpend;
#	    if OPEN_O_BINARY
		    if (expandbinary)
			    *diffp++ = "--binary";
#	    endif
	    diffp[0] = fname1;
	    diffp[1] = fname2;
	    diffp[2] = 0;
	    
	    /* Create temp output file, along with handle to return */
	    if ((fd = rcsMakeTmpHdl(hdlout, &outname, S_IWUSR | S_IRUSR)) < 0) {
		    rc = fd;
		    break;
	    }
	    close(fd);
	    switch (runv(-1, outname, diffv)) {
		    case DIFF_SUCCESS:
			    break;
		    case DIFF_FAILURE:
			    rc = 1;	/* differences found */
			    break;
		    default:
			    rcsFreeTmpHdl(*hdlout);
			    workerror("diff failed");
			    break;
	    }
    } while (0);
    rcsHeapFree(&localHeap);
    return rc;
}


#if DIFF_L
	static char const *
setup_label(b, fname, date)
	struct buf *b;
	char const *fname;
	char const date[datesize];
{
	char *p;
	char datestr[datesize + zonelenmax];
	VOID date2str(date, datestr);
	
	bufalloc(b,
		strlen(fname)
		+ sizeof datestr + 4
	);
	p = b->string;
	VOID sprintf(p, "-L%s\t%s", fname, datestr);
	return p;
}
#endif
