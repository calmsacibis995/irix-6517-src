/*
 * pmnsmerge [-adfv] infile [...] outfile
 *
 * Merge PCP PMNS files
 *
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

#ident "$Id: pmnsmerge.c,v 1.15 1997/09/05 01:04:41 markgw Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "./pmnsutil.h"

extern int		errno;

static FILE		*outf;		/* output */
static __pmnsNode	*root;		/* result so far */
static char		*fullname;	/* full PMNS pathname for newbie */
static int		verbose = 0;

typedef struct {
    char	*fname;
    char	*date;
} datestamp_t;

#define STAMP	"_DATESTAMP"

static int
sortcmp(const void *a, const void *b)
{
    datestamp_t	*pa = (datestamp_t *)a;
    datestamp_t	*pb = (datestamp_t *)b;

    if (pa->date == NULL) return -1;
    if (pb->date == NULL) return 1;
    return strcmp(pa->date, pb->date);
}

/*
 * scan for #define _DATESTAMP and re-order args accordingly
 */
static void
sortargs(char **argv, int argc)
{
    FILE	*f;
    datestamp_t	*tab;
    char	*p;
    char	*q;
    int		i;
    char	lbuf[40];

    tab = (datestamp_t *)malloc(argc * sizeof(datestamp_t));

    for (i = 0; i <argc; i++) {
	if ((f = fopen(argv[i], "r")) == NULL) {
	    fprintf(stderr, "%s: Error: cannot open input PMNS file \"%s\"\n",
		pmProgname, argv[i]);
	    exit(1);
	}
	tab[i].fname = strdup(argv[i]);
	tab[i].date = NULL;
	while (fgets(lbuf, sizeof(lbuf), f) != NULL) {
	    if (strncmp(lbuf, "#define", 7) != 0)
		continue;
	    p = &lbuf[7];
	    while (*p && isspace(*p))
		p++;
	    if (*p == '\0' || strncmp(p, STAMP, strlen(STAMP)) != 0)
		continue;
	    p += strlen(STAMP);
	    while (*p && isspace(*p))
		p++;
	    q = p;
	    while (*p && !isspace(*p))
		p++;
	    *p = '\0';
	    tab[i].date = strdup(q);
	    break;
	}
	fclose(f);
    }

    qsort(tab, argc, sizeof(tab[0]), sortcmp);
    for (i = 0; i <argc; i++) {
	argv[i] = tab[i].fname;
	if (verbose > 1)
	    printf("arg[%d] %s _DATESTAMP=%s\n", i, tab[i].fname, tab[i].date);
    }
}

static void
addpmns(__pmnsNode *base, char *name, __pmnsNode *p)
{
    char	*tail;
    ptrdiff_t	nch;
    __pmnsNode	*np;
    __pmnsNode	*lastp;

    for (tail = name; *tail && *tail != '.'; tail++)
	;
    nch = tail - name;

    for (np = base->first; np != NULL; np = np->next) {
	if (strlen(np->name) == nch && strncmp(name, np->name, (int)nch) == 0)
	    break;
	lastp = np;
    }

    if (np == NULL) {
	/* no match ... add here */
	np = (__pmnsNode *)malloc(sizeof(__pmnsNode));
	if (base->first) {
	    lastp->next = np;
	    np->parent = lastp->parent;
	}
	else {
	    base->first = np;
	    np->parent = base;
	}
	np->first = np->next = NULL;
	np->hash = NULL;		/* we do not need this here */
	np->name = (char *)malloc(nch+1);
	strncpy(np->name, name, nch);
	np->name[nch] = '\0';
	if (*tail == '\0') {
	    np->pmid = p->pmid;
	    return;
	}
	np->pmid = PM_ID_NULL;
    }
    else if (*tail == '\0') {
	/* complete match */
	if (np->pmid != p->pmid) {
	    fprintf(stderr, "%s: Warning: performance metric \"%s\" has multiple PMIDs.\n... using PMID %s and ignoring PMID",
		pmProgname, fullname, pmIDStr(np->pmid));
	    fprintf(stderr, " %s\n",
		pmIDStr(p->pmid));
	}
	return;
    }

    /* descend */
    addpmns(np, tail+1, p);
}


/*
 * merge, adding new nodes if required
 */
static void
merge(__pmnsNode *p, int depth, char *path)
{
    char	*name;

    if (depth < 1 || p->pmid == PM_ID_NULL || p->first != NULL)
	return;
    name = (char *)malloc(strlen(path)+strlen(p->name)+2);
    if (*path == '\0')
	strcpy(name, p->name);
    else {
	strcpy(name, path);
	strcat(name, ".");
	strcat(name, p->name);
    }
    fullname = name;
    addpmns(root, name, p);
    free(name);
}

int
main(int argc, char **argv)
{
    int		sts;
    int		first = 1;
    int		c;
    int		j;
    int		force = 0;
    int		asis = 0;
    int		dupok = 0;
    int		errflag = 0;
    char	*pmnscomp = "/usr/pcp/bin/pmnscomp";
    __pmnsNode	*tmp;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;
    char	*p;
    char	cmd[1024];

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    umask((mode_t)022);		/* anything else is pretty silly */

    while ((c = getopt(argc, argv, "aD:dfvx:?")) != EOF) {
	switch (c) {

	case 'a':
	    asis = 1;
	    break;

	case 'd':	/* duplicate PMIDs are OK */
	    dupok = 1;
	    break;

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

	case 'f':	/* force ... unlink file first */
	    force = 1;
	    break;

	case 'v':
	    verbose++;
	    break;

#ifdef BUILDTOOL
	case 'x':		/* alternate pmnscomp */
	    pmnscomp = optarg;
	    break;
#endif

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind > argc-2) {
	fprintf(stderr,
"Usage: %s [options] infile [...] outfile\n\
\n\
Options:\n\
  -a	process files in order, ignoring embedded _DATESTAMP control lines\n\
  -d    duplicate PMIDs are acceptable\n\
  -f	force overwriting of an existing output file, if it exists\n\
  -v	verbose, echo input file names as processed\n",
			pmProgname);	
	exit(1);
    }

    if (force)
	unlink(argv[argc-1]);

    if (access(argv[argc-1], F_OK) == 0) {
	fprintf(stderr, "%s: Error: output PMNS file \"%s\" already exists!\nYou must either remove it first, or use -f\n",
		pmProgname, argv[argc-1]);
	exit(1);
    }

    /*
     * from here on, ignore SIGINT, SIGHUP and SIGTERM to protect
     * the integrity of the new ouput file
     */
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if ((outf = fopen(argv[argc-1], "w+")) == NULL) {
	fprintf(stderr, "%s: Error: cannot create output PMNS file \"%s\": %s\n", pmProgname, argv[argc-1], strerror(errno));
	exit(1);
    }

    if (!asis)
	sortargs(&argv[optind], argc - optind - 1);

    j = optind;
    while (j < argc-1) {
	if (verbose)
	    printf("%s:\n", argv[j]);

	if ((sts = pmLoadASCIINameSpace(argv[j], 1)) < 0) {
	    fprintf(stderr, "%s: Error: pmLoadNameSpace(%s): %s\n",
		pmProgname, argv[j], pmErrStr(sts));
	    exit(1);
	}
	{
	    __pmnsTree *t;
	    t = __pmExportPMNS();
	    if (t == NULL) {
	       /* sanity check - shouldn't ever happen */
	       fprintf(stderr, "Exported PMNS is NULL !");
	       exit(1);
	    }
	    tmp = t->root;
	}


	if (first) {
	    root = tmp;
	    first = 0;
	}
	else {
	    pmns_traverse(tmp, 0, "", merge);
	}
	j++;
    }

    pmns_output(root, outf);
    fclose(outf);

    sprintf(cmd, "%s %s -f -n %s %s.bin",
	pmnscomp, dupok ? "-d" : "", argv[argc-1], argv[argc-1]);
    sts = system(cmd);

    exit(sts);
    /* NOTREACHED */
}
