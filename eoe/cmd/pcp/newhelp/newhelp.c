/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: newhelp.c,v 2.19 1998/11/15 08:35:24 kenmcd Exp $"

/*
 * newhelp file
 *
 * in the model of newaliases, build ndbm data files for a PMDA's help file
 * -- given the bloat of the ndbm files, version 2 uses a much simpler
 * file access method, but preserves the file name conventions from
 * version 1 that is based on ndbm.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include "pmapi.h"
#include "impl.h"

#define VERSION 2

extern int	errno;
static int	verbose = 0;
static int	ln = 0;
static char	*filename;
static int	status = 0;
static int	version = 1;
static FILE	*f = NULL;
static DBM	*dbf = NULL;

/* maximum bytes per line and bytes per entry */
#define MAXLINE	128
#define MAXENTRY 1024

typedef struct {
    pmID	pmid;
    __uint32_t	off_oneline;
    __uint32_t	off_text;
} help_idx_t;

static help_idx_t	*hindex = NULL;
static int		numindex = 0;
static int		thisindex = -1;

static void
newentry(char *buf)
{
    int		n;
    char	*p;
    char	*end_name;
    char	end_c;
    char	*name;
    pmID	pmid;
    __pmHelpKey	helpkey;
    datum	key;
    datum	content;
    int		type;
    char	*start;
    int		warn = 0;
    int		i;
    int		sts;

    if (version == 1) {
	key.dptr = (char *)&helpkey;
	key.dsize = sizeof(__pmHelpKey);
    }

    /* skip leading white space ... */
    for (p = buf; isspace(*p); p++)
	;
    /* skip over metric name or indom spec ... */
    name = p;
    for (p = buf; *p != '\n' && !isspace(*p); p++)
	;
    end_c = *p;
    *p = '\0';	/* terminate metric name */
    end_name = p;

    if ((n = pmLookupName(1, &name, &pmid)) < 0) {
	/* apparently not a metric name */
	int	domain;
	int	serial;
	if (sscanf(buf, "%d.%d", &domain, &serial) == 2) {
	    /* an entry for an instance domain */
	    __pmInDom_int	ii;
	    int			*ip;
	    ii.domain = domain;
	    ii.serial = serial;
	    if (version == 1) {
		ip = (int *)&ii;
		helpkey.ident = *ip;
		type = PM_TEXT_INDOM;
	    }
	    else {
		/* set a bit here to disambiguate pmInDom from pmID */
		ii.pad = 1;
		ip = (int *)&ii;
		pmid = (pmID)*ip;
	    }
	}
	else {
	    fprintf(stderr, "%s: [%s:%d] %s: %s, entry abandoned\n",
		    pmProgname, filename, ln, buf, pmErrStr(n));
	    status = 2;
	    return;
	}
    }
    else {
	if (pmid == PM_ID_NULL) {
	    fprintf(stderr, "%s: [%s:%d] %s: unknown metric, entry abandoned\n",
		    pmProgname, filename, ln, name);
	    status = 2;
	    return;
	}
	if (version == 1) {
	    helpkey.ident = pmid;
	    type = PM_TEXT_PMID;
	}
    }

    if (version != 1) {
	for (i = 0; i < thisindex; i++) {
	    if (hindex[thisindex].pmid == pmid) {
		__pmInDom_int	*kp = (__pmInDom_int *)&pmid;
		fprintf(stderr, "%s: [%s:%d] duplicate key (", 
			pmProgname, filename, ln);
		if (kp->pad == 0)
		    fprintf(stderr, "%s", pmIDStr(pmid));
		else {
		    kp->pad = 0;
		    fprintf(stderr, "%s", pmInDomStr((pmInDom)pmid));
		}
		fprintf(stderr, ") entry abandoned\n");
		status = 2;
		return;
	    }
	}

	if (++thisindex >= numindex) {
	    if (numindex == 0)
		numindex = 128;
	    else
		numindex *= 2;
	    if ((hindex = (help_idx_t *)realloc(hindex, numindex * sizeof(hindex[0]))) == NULL) {
		__pmNoMem("newentry", numindex * sizeof(hindex[0]), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	}

	fprintf(f, "\n@ %s ", name);

	hindex[thisindex].pmid = pmid;
	hindex[thisindex].off_oneline = ftell(f);
    }

    /* skip white space ... to start of oneline */
    *p = end_c;
    if (*p != '\n')
	p++;
    for ( ; *p != '\n' && isspace(*p); p++)
	;
    start = p;

    /* skip to end of line ... */
    for ( ; *p != '\n'; p++)
	;
    *p = '\0';
    p++;
    
    if (p - start == 1 && verbose) {
	fprintf(stderr, "%s: [%s:%d] %s: warning, null oneline\n",
	    pmProgname, filename, ln, name);
	warn = 1;
	if (!status) status = 1;
    }

    if (version == 1) {
#ifdef HAVE_NDBM
	content.dptr = start;
	content.dsize = p - start - 1;
	helpkey.type = type | PM_TEXT_ONELINE;
	sts = dbm_store(dbf, key, content, DBM_INSERT);
	if (sts < 0) {
	    fprintf(stderr, "%s: [%s:%d] %s: store oneline failed, entry abandoned\n",
		    pmProgname, filename, ln, name);
	    status = 2;
	    return;
	}
	else if (sts > 0) {
	    fprintf(stderr, "%s: [%s:%d] duplicate key (", 
	    	    pmProgname, filename, ln);
	    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) 
		fprintf(stderr, "%s", pmIDStr(pmid));
	    else {
		fprintf(stderr, "%s", pmInDomStr((pmInDom)pmid));
	    }
	    fprintf(stderr, ") entry abandoned\n");
	    status = 2;
	    return;
	}
#endif
    }
    else {
	fwrite(start, sizeof(*start), p - start, f);
	if (ferror(f)) {
	    fprintf(stderr, "%s: [%s:%d] %s: write oneline failed, entry abandoned\n",
		    pmProgname, filename, ln, name);
	    thisindex--;
	    status = 2;
	    return;
	}

	hindex[thisindex].off_text = ftell(f);
    }

    /* trim all but last newline ... */
    i = (int)strlen(p) - 1;
    while (i >= 0 && p[i] == '\n')
	i--;
    if (i < 0)
	i = 0;
    else {
	/* really have text ... p[i] is last non-newline char */
	i++;
	if (version == 1)
	    p[i++] = '\n';
    }
    p[i] = '\0';

    if (i == 0 && verbose) {
	fprintf(stderr, "%s: [%s:%d] %s: warning, null help\n",
	    pmProgname, filename, ln, name);
	warn = 1;
	if (!status) status = 1;
    }

    if (version == 1) {
#ifdef HAVE_NDBM
	content.dptr = p;
	content.dsize = i+1;
	helpkey.type = type | PM_TEXT_HELP;
	sts = dbm_store(dbf, key, content, DBM_INSERT);
	if (sts < 0) {
	    fprintf(stderr, "%s: [%s:%d] %s: store help failed, entry abandoned\n",
		    pmProgname, filename, ln, name);
	    status = 2;
	    return;
	}
	else if (sts > 0) {
	    fprintf(stderr, "%s: [%s:%d] duplicate key (", 
	    	    pmProgname, filename, ln);
	    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) 
		fprintf(stderr, "%s", pmIDStr(pmid));
	    else {
		fprintf(stderr, "%s", pmInDomStr((pmInDom)pmid));
	    }
	    fprintf(stderr, ") entry abandoned\n");
	    status = 2;
	    return;
	}
#endif
    }
    else {
	fwrite(p, sizeof(*p), i+1, f);
	if (ferror(f)) {
	    fprintf(stderr, "%s: [%s:%d] %s: write help failed, entry abandoned\n",
		    pmProgname, filename, ln, name);
	    thisindex--;
	    status = 2;
	    return;
	}
    }

    if (verbose && warn == 0) {
	*end_name = '\0';
	fprintf(stderr, "%s\n", name);
	*end_name = end_c;
    }
}

static int
idcomp(const void *a, const void *b)
{
    help_idx_t	*ia, *ib;

    ia = (help_idx_t *)a;
    ib = (help_idx_t *)b;
    return ia->pmid - ib->pmid;
}

int
main(argc, argv)
int argc;
char *argv[];
{
    int		n;
    int		c;
    int		i;
    int		sts;
    char	*pmnsfile = PM_NS_DEFAULT;
    int		errflag = 0;
    char	*fname = NULL;
    char	pathname[MAXPATHLEN];
    FILE	*inf;
    char	buf[MAXENTRY+MAXLINE];
    char	*endnum;
    char	*bp;
    char	*p;
    int		skip;
    help_idx_t	hdr;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "D:n:o:Vv:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'n':	/* alternative namespace file */
	    pmnsfile = optarg;
	    break;

	case 'o':	/* alternative output file name */
	    fname = optarg;
	    break;

	case 'V':	/* more chit-chat */
	    verbose++;
	    break;

	case 'v':	/* version 1 or 2 */
	    version = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -v requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    if (version < 1 || version > 2) {
		fprintf(stderr, "%s: version must be 1 or 2\n", pmProgname);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

#ifndef HAVE_NDBM
    if (version == 1) {
	fprintf(stderr, "%s: Error: version 1 not supported on this platform.\n", pmProgname);
	errflag++;
    }
#endif

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] [file ...]\n\
\n\
Options:\n\
  -n pmnsfile    use an alternative PMNS\n\
  -o outputfile  base name for output files\n\
  -V             verbose/diagnostic output\n\
  -v version     version 1 or 2 format\n",
		pmProgname);
	exit(2);
    }

    if ((n = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "%s: pmLoadNameSpace: %s\n",
		pmProgname, pmErrStr(n));
	exit(2);
    }

    do {
	if (optind < argc) {
	    filename = argv[optind];
	    if ((inf = fopen(filename, "r")) == NULL) {
		perror(filename);
		exit(2);
	    }
	    if (fname == NULL)
		fname = filename;
	}
	else {
	    if (fname == NULL) {
		fprintf(stderr, "%s: need either a -o option or a filename argument to name the output file\n", pmProgname);
		exit(2);
	    }
	    filename = "<stdin>";
	    inf = stdin;
	}

	if (version == 1 && dbf == NULL) {
#ifdef HAVE_NDBM
	    if ((dbf = dbm_open(fname, O_RDWR|O_CREAT|O_TRUNC, 0644)) == NULL) {
		fprintf(stderr, "%s: dbm_open(\"%s\", ...) failed: %s\n",
			pmProgname, fname, strerror(errno));
		exit(2);
	    }
#endif
	}
	else if (version == 2 && f == NULL) {
	   sprintf(pathname, "%s.pag", fname);
	    if ((f = fopen(pathname, "w")) == NULL) {
		fprintf(stderr, "%s: fopen(\"%s\", ...) failed: %s\n",
		    pmProgname, pathname, strerror(errno));
		exit(2);
	    }
	    /* header: 2 => pag cf 1 => dir */
	    fprintf(f, "PcPh2%c\n", '0' + VERSION);
	}

	bp = buf;
	skip = 1;
	for ( ; ; ) {
	    if (fgets(bp, MAXLINE, inf) == NULL) {
		skip = -1;
		*bp = '@';
	    }
	    ln++;
	    if (bp[0] == '#')
		continue;
	    if (bp[0] == '@') {
		/* start of a new entry */
		if (bp > buf) {
		    /* really have a prior entry */
		    p = bp - 1;
		    while (p > buf && *p == '\n')
			p--;
		    *++p = '\n';
		    *++p = '\0';
		    newentry(buf);
		}
		if (skip == -1)
		    break;
		skip = 0;
		bp++;	/* skip '@' */
		while (*bp && isspace(*bp))
		    bp++;
		if (bp == '\0') {
		    if (verbose)
			fprintf(stderr, "%s: [%s:%d] null entry?\n", 
				pmProgname, filename, ln);
		    skip = 1;
		    bp = buf;
		    if (!status) status = 1;
		}
		else {
		    for (p = bp; *p; p++)
			;
		    memmove(buf, bp, p - bp + 1);
		    for (bp = buf; *bp; bp++)
			;
		}
	    }
	    if (skip)
		continue;
	    for (p = bp; *p; p++)
		;
	    if (bp > buf && p[-1] != '\n') {
		*p++ = '\n';
		*p = '\0';
		fprintf(stderr, "%s: [%s:%d] long line split after ...\n%s",
			    pmProgname, filename, ln, buf);
		ln--;
		if (!status) status = 1;
	    }
	    bp = p;
	    if (bp > &buf[MAXENTRY]) {
		bp = &buf[MAXENTRY];
		bp[-1] = '\0';
		bp[-2] = '\n';
		fprintf(stderr, "%s: [%s:%d] entry truncated after ... %s",
			    pmProgname, filename, ln, &bp[-64]);
		skip = 1;
		if (!status) status = 1;
	    }
	}

	fclose(inf);
	optind++;
    } while (optind < argc);

#ifdef HAVE_NDBM
    if (dbf != NULL)
	dbm_close(dbf);
    else
#endif
    if (f != NULL) {
	fclose(f);

	/* do the directory index ... */
	sprintf(pathname, "%s.dir", fname);
	if ((f = fopen(pathname, "w")) == NULL) {
	    fprintf(stderr, "%s: fopen(\"%s\", ...) failed: %s\n",
		pmProgname, pathname, strerror(errno));
	    exit(2);
	}

	/* index header */
	hdr.pmid = 0x50635068;		/* "PcPh" */
	/* "1" => dir, next char is version */
	hdr.off_oneline = 0x31000000 | (('0' + VERSION) << 16);
	hdr.off_text = thisindex + 1;	/* # entries */
	fwrite(&hdr, sizeof(hdr), 1, f);
	if (ferror(f)) {
	     fprintf(stderr, "%s: fwrite index failed: %s\n",
		     pmProgname, strerror(errno));
	     exit(2);
	}

	/* sort and write index */
	qsort((void *)hindex, thisindex+1, sizeof(hindex[0]), idcomp);
	for (i = 0; i <= thisindex; i++) {
	    fwrite(&hindex[i], sizeof(hindex[0]), 1, f);
	    if (ferror(f)) {
		 fprintf(stderr, "%s: fwrite index failed: %s\n",
			 pmProgname, strerror(errno));
		 exit(2);
	    }
	}
    }

    exit(status);
    /*NOTREACHED*/
}

#ifdef BUILDTOOL
/*
 * redefine __pmGetLicenseCap(), to avoid PCP license hassles in the
 * build environment
 */

int
__pmGetLicenseCap(void)
{
    return PM_LIC_PCP | PM_LIC_COL | PM_LIC_MON;
}
#endif
