/*
 * pmnsdel [-d] [-n pmnsfile ] metricpath [...]
 *
 * Cull subtree(s) from a PCP PMNS
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

#ident "$Id: pmnsdel.c,v 1.12 1999/05/25 10:29:49 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include "pmapi.h"
#include "impl.h"
#include "./pmnsutil.h"

extern int		errno;

static FILE		*outf;		/* output */
static __pmnsNode	*root;		/* result so far */
static char		*fullname;	/* full PMNS pathname for newbie */

static void
delpmns(__pmnsNode *base, char *name)
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
	/* no match ... */
	fprintf(stderr, "%s: Error: metricpath \"%s\" not defined in the PMNS\n",
		pmProgname, fullname);
	exit(1);
    }
    else if (*tail == '\0') {
	/* complete match */
	if (np == base->first) {
	    /* deleted node is first at this level */
	    base->first = np->next;
	    /*
	     * remove predecessors that only exist to connect
	     * deleted node to the rest of the PMNS
	     */
	    np = base;
	    while (np->first == NULL && np->parent != NULL) {
		if (np->parent->first == np) {
		    /* victim is the only one at this level ... */
		    np->parent->first = np->next;
		    np = np->parent;
		}
		else {
		    /* victim has at least one sibling at this level */
		    lastp = np->parent->first;
		    while (lastp->next != np)
			lastp = lastp->next;
		    lastp->next = np->next;
		    break;
		}
	    }
	}
	else
	    /* link around deleted node */
	    lastp->next = np->next;
	return;
    }

    /* descend */
    delpmns(np, tail+1);
}

int
main(int argc, char **argv)
{
    int		sts;
    int		c;
    char	*pmnsfile;
    int		dupok = 0;
    int		errflag = 0;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;
    char	*p;
    char	cmd[80];

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    pmnsfile = getenv("PMNS_DEFAULT");
    if (pmnsfile == NULL)
	pmnsfile = "/var/pcp/pmns/root";

    while ((c = getopt(argc, argv, "dD:n:?")) != EOF) {
	switch (c) {

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

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind > argc-1) {
	fprintf(stderr, "Usage: %s [-d] [-n pmnsfile ] metricpath [...]\n", pmProgname);	
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	fprintf(stderr, "%s: Error: pmLoadNameSpace(%s): %s\n",
	    pmProgname, pmnsfile, pmErrStr(sts));
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
        root = t->root;
    }


    while (optind < argc) {
	delpmns(root, fullname = argv[optind]);
	optind++;
    }

    /*
     * from here on, ignore SIGINT, SIGHUP and SIGTERM to protect
     * the integrity of the new ouput file
     */
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if ((outf = fopen(pmnsfile, "w")) == NULL) {
	fprintf(stderr, "%s: Error: cannot open PMNS file \"%s\" for writing: %s\n",
	    pmProgname, pmnsfile, strerror(errno));
	exit(1);
    }

    pmns_output(root, outf);
    fclose(outf);

    sprintf(cmd, "/usr/pcp/bin/pmnscomp %s -f -n %s %s.bin",
	dupok ? "-d" : "", pmnsfile, pmnsfile);
    sts = system(cmd);

    exit(sts);
    /* NOTREACHED */
}
