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

#ident "$Id: pmns.c,v 1.1 1997/01/30 19:52:44 gberthet Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

/*
 * need this one _only_ for dumptree()
 */
#include "impl.h"

#if DEBUG
/*
 * we may be using the limdmalloc.so package, and summaries from /usr/lib/cpp
 * at the end of the output are less than helpful!
 */
#define CPP "_RLD_LIST=DEFAULT %s -I. -I/var/pcp/pmns -I/usr/pcp/pmns %s"
#else
#define CPP "%s -I. -I/var/pcp/pmns -I/usr/pcp/pmns %s"
#endif

static char	*cpp_path[] = {
    "/lib/cpp",
    "/usr/cpu/sysgen/root/lib/cpp",
    "/usr/lib/cpp",
    "/usr/cpu/sysgen/root/usr/lib/cpp",
    (char *)0
};


#define NAME	1
#define PATH	2
#define PMID	3
#define LBRACE	4
#define RBRACE	5
#define BOGUS	10

#define PMID_MASK	0x3fffffff	/* 30 bits of PMID */
#define MARK_BIT	0x40000000	/* mark bit */

static int	lineno = 0;
static char	linebuf[256];
static char	*linep;
static char	fname[256];
static char	tokbuf[256];
static pmID	tokpmid;
static int	numpmid;

static _pmnsNode	*root;
static _pmnsNode	*seen;
static _pmnsNode **htab;
static int	htabsize;

extern int	errno;

#if !defined(LIBIRIXPMDA)
/*
 * support for restricted pmLoadNameSpace services, based upon license
 * capabilities
 */

static struct {
    int		cap;		/* & with result from  _pmGetLicenseCap() */
    char	*root;		/* what to use for PM_NS_DEFAULT */
    int		ascii;		/* allow ascii format? */
    int		secure;		/* only secure binary format? */
} ctltab[] = {
    { PM_LIC_PCP,	"/var/pcp/pmns/root",		1,	0 },
    { PM_LIC_WEB,	"/var/pcp/pmns/root_web",	0,	1 },
#ifdef DEBUG
    { PM_LIC_DEV,	"/tmp/root_dev",		0,	1 },
#endif
    { 0,		"/var/pcp/pmns/root",		1,	0 },
};
static int	numctl = sizeof(ctltab)/sizeof(ctltab[0]);

/*
 * for debugging, and visible via _pmDumpNameSpace()
 *
 * verbosity is 0 (name), 1 (names and pmids) or 2 (names, pmids and linked-list structures)
 */
static void
dumptree(FILE *f, int level, _pmnsNode *rp, int verbosity)
{
    int		i;
    _pmID_int	*pp;

    if (rp != (_pmnsNode *)0) {
	if (verbosity > 1)
	    fprintf(f, "0x%0x", rp);
	for (i = 0; i < level; i++) {
	    fprintf(f, "    ");
	}
	fprintf(f, " %-16.16s", rp->name);
	pp = (_pmID_int *)&rp->pmid;
	if (verbosity > 0 && rp->first == (_pmnsNode *)0)
	    fprintf(f, " %d %d.%d.%d 0x%08x", rp->pmid,
	        pp->domain, pp->cluster, pp->item,
	        rp->pmid);
	if (verbosity > 1) {
	    fprintf(f, "\t[first: ");
	    if (rp->first) fprintf(f, "0x%8x", rp->first);
	    else fprintf(f, "<null>");
	    fprintf(f, " next: ");
	    if (rp->next) fprintf(f, "0x%8x", rp->next);
	    else fprintf(f, "<null>");
	    fprintf(f, " parent: ");
	    if (rp->parent) fprintf(f, "0x%8x", rp->parent);
	    else fprintf(f, "<null>");
	    fprintf(f, " hash: ");
	    if (rp->hash) fprintf(f, "0x%8x", rp->hash);
	    else fprintf(f, "<null>");
	}
	fputc('\n', f);
	dumptree(f, level+1, rp->first, verbosity);
	dumptree(f, level, rp->next, verbosity);
    }
}
#endif

static void
err(char *s)
{
    if (lineno > 0)
	fprintf(stderr, "[%s:%d] ", fname, lineno);
    fprintf(stderr, "Error Parsing ASCII PMNS: %s\n", s);
    if (lineno > 0) {
	char	*p;
	fprintf(stderr, "    %s", linebuf);
	for (p = linebuf; *p; p++)
	    ;
	if (p[-1] != '\n')
	    fputc('\n', stderr);
	if (linep) {
	    p = linebuf;
	    for (p = linebuf; p < linep; p++) {
		if (!isspace(*p))
		    *p = ' ';
	    }
	    *p++ = '^';
	    *p++ = '\n';
	    *p = '\0';
	    fprintf(stderr, "    %s", linebuf);
	}
    }
}

static int
lex(void)
{
    static int	first = 1;
    static FILE	*fin;
    static char	*lp;
    char	*tp;
    int		colon;
    int		type;
    int		d, c, e;

    if (first) {
	int		i;
	first = 0;
	for (i = 0; cpp_path[i] != (char *)0; i++) {
	    if (access(cpp_path[i], X_OK) != 0)
		continue;
	    if ((lp = (char *)malloc(strlen(CPP) + strlen(fname) +strlen(cpp_path[i]) - 4 + 1)) == (char *)0)
		return -errno;

	    sprintf(lp, CPP, cpp_path[i], fname);

	    fin = popen(lp, "r");
	    free(lp);
	    if (fin == NULL)
		return -errno;
	    break;
	}
	if (cpp_path[i] == (char *)0) {
	    fprintf(stderr, "pmLoadNameSpace: Unable to find an executable cpp at any of ...\n");
	    for (i = 0; cpp_path[i] != (char *)0; i++)
		fprintf(stderr, "    %s\n", cpp_path[i]);
	    fprintf(stderr, "Sorry, but this is fatal\n");
	    exit(1);
	}

	lp = linebuf;
	*lp = '\0';
    }

    while (*lp && isspace(*lp)) lp++;

    while (*lp == '\0') {
	for ( ; ; ) {
	    char	*p;
	    char	*q;
	    int		inspace = 0;

	    if (fgets(linebuf, sizeof(linebuf), fin) == NULL)
		return 0;
	    for (q = p = linebuf; *p; p++) {
		if (isspace(*p)) {
		    if (!inspace) {
			if (q > linebuf && q[-1] != ':')
			    *q++ = *p;
			inspace = 1;
		    }
		}
		else if (*p == ':') {
		    if (inspace) {
			q--;
			inspace = 0;
		    }
		    *q++ = *p;
		}
		else {
		    *q++ = *p;
		    inspace = 0;
		}
	    }
	    if (p[-1] != '\n') {
		err("Absurdly long line, cannot recover");
		pclose(fin);	/* wait for cpp to finish */
		exit(1);
	    }
	    *q = '\0';
	    if (linebuf[0] == '#') {
		/* cpp control line */
		sscanf(linebuf, "# %d \"%s", &lineno, fname);
		--lineno;
		for (p = fname; *p; p++)
		    ;
		*--p = '\0';
		continue;
	    }
	    else
		lineno++;
	    lp = linebuf;
	    while (*lp && isspace(*lp)) lp++;
	    break;
	}
    }

    linep = lp;
    tp = tokbuf;
    while (!isspace(*lp))
	*tp++ = *lp++;
    *tp = '\0';

    if (tokbuf[0] == '{' && tokbuf[1] == '\0') return LBRACE;
    else if (tokbuf[0] == '}' && tokbuf[1] == '\0') return RBRACE;
    else if (isalpha(tokbuf[0])) {
	type = NAME;
	for (tp = &tokbuf[1]; *tp; tp++) {
	    if (*tp == '.')
		type = PATH;
	    else if (!isalpha(*tp) && !isdigit(*tp) && *tp != '_' && *tp != '-')
		break;
	}
	if (*tp == '\0') return type;
    }
    colon = 0;
    for (tp = tokbuf; *tp; tp++) {
	if (*tp == ':') {
	    if (++colon > 3) return BOGUS;
	}
	else if (!isdigit(*tp)) return BOGUS;
    }

    /*
     * Internal PMID format
     * domain 8 bits
     * cluster 12 bits
     * enumerator 10 bits
     */
    if (sscanf(tokbuf, "%d:%d:%d", &d, &c, &e) != 3 || d > 255 || c > 4095 || e > 1023) {
	err("Illegal PMID");
	return BOGUS;
    }
    tokpmid = (d << 22) | (c << 10) | e;
    return PMID;
}

static _pmnsNode *
findseen(char *name)
{
    _pmnsNode	*np;
    _pmnsNode	*lnp;

    for (np = seen, lnp = (_pmnsNode *)0; np != (_pmnsNode *)0; lnp = np, np = np->next) {
	if (strcmp(np->name, name) == 0) {
	    if (np == seen)
		seen = np->next;
	    else
		lnp->next = np->next;
	    np->next = (_pmnsNode *)0;
	    return np;
	}
    }
    return (_pmnsNode *)0;
}

static int
attach(char *base, _pmnsNode *rp)
{
    int		i;
    _pmnsNode	*np;
    _pmnsNode	*xp;
    char	*path;

    if (rp != (_pmnsNode *)0) {
	for (np = rp->first; np != (_pmnsNode *)0; np = np->next) {
	    if (np->pmid == PM_ID_NULL) {
		/* non-terminal node ... */
		if (*base == '\0') {
		    if ((path = (char *)malloc(strlen(np->name)+1)) == (char *)0)
			return -errno;
		    strcpy(path, np->name);
		}
		else {
		    if ((path = (char *)malloc(strlen(base)+strlen(np->name)+2)) == (char *)0)
			return -errno;
		    strcpy(path, base);
		    strcat(path, ".");
		    strcat(path, np->name);
		}
		if ((xp = findseen(path)) == (_pmnsNode *)0) {
		    sprintf(linebuf, "Cannot find definition for non-terminal node \"%s\" in name space",
		        path);
		    err(linebuf);
		    return PM_ERR_PMNS;
		}
		np->first = xp->first;
		free(xp);
		numpmid--;
		i = attach(path, np);
		free(path);
		if (i != 0)
		    return i;
	    }
	}
    }
    return 0;
}

static int
backname(_pmnsNode *np, char **name)
{
    _pmnsNode	*xp;
    char	*p;
    int		nch;

    nch = 0;
    xp = np;
    while (xp->parent != (_pmnsNode *)0) {
	nch += (int)strlen(xp->name)+1;
	xp = xp->parent;
    }

    if ((p = (char *)malloc(nch)) == (char *)0)
	return -errno;

    p[--nch] = '\0';
    xp = np;
    while (xp->parent != (_pmnsNode *)0) {
	int	xl;

	xl = (int)strlen(xp->name);
	nch -= xl;
	strncpy(&p[nch], xp->name, xl);
	xp = xp->parent;
	if (xp->parent == (_pmnsNode *)0)
	    break;
	else
	    p[--nch] = '.';
    }
    *name = p;

    return 0;
}

static int
backlink(_pmnsNode *root, int dupok)
{
    _pmnsNode	*np;
    int		status;

    for (np = root->first; np != (_pmnsNode *)0; np = np->next) {
	np->parent = root;
	if (np->pmid != PM_ID_NULL) {
	    int		i;
	    _pmnsNode	*xp;
	    i = np->pmid % htabsize;
	    for (xp = htab[i]; xp != (_pmnsNode *)0; xp = xp->hash) {
		if (xp->pmid == np->pmid && !dupok) {
		    char *nn, *xn;
		    backname(np, &nn);
		    backname(xp, &xn);
		    sprintf(linebuf, "Duplicate metric id (%s) in name space for metrics \"%s\" and \"%s\"\n",
		        pmIDStr(np->pmid), nn, xn);
		    err(linebuf);
		    free(nn);
		    free(xn);
		    return PM_ERR_PMNS;
		}
	    }
	    np->hash = htab[i];
	    htab[i] = np;
	}
	if (status = backlink(np, dupok))
	    return status;
    }
    return 0;
}

static int
pass2(int dupok)
{
    _pmnsNode	*np;
    int		status;

    lineno = -1;
    if ((root = findseen("root")) == (_pmnsNode *)0) {
	err("No name space entry for \"root\"");
	return PM_ERR_PMNS;
    }

    if (findseen("root") != (_pmnsNode *)0) {
	err("Multiple name space entries for \"root\"");
	return PM_ERR_PMNS;
    }

    if (status = attach("", root))
	return status;
    ;

    for (np = seen; np != (_pmnsNode *)0; np = np->next) {
	sprintf(linebuf, "Disconnected subtree (\"%s\") in name space", np->name);
	err(linebuf);
	status = PM_ERR_PMNS;
    }
    if (status)
	return status;

    /*
     * make the average hash list no longer than 5, and the number
     * of has table entries not a multiple of 2, 3 or 5
     */
    htabsize = numpmid/5;
    if (htabsize % 2 == 0) htabsize++;
    if (htabsize % 3 == 0) htabsize += 2;
    if (htabsize % 5 == 0) htabsize += 2;
    if ((htab = (_pmnsNode **)calloc(htabsize, sizeof(_pmnsNode *))) == (_pmnsNode **)0)
	return -errno;

    return backlink(root, dupok);
}

static _pmnsNode *
locate(char *name, _pmnsNode *root)
{
    char	*tail;
    ptrdiff_t	nch;
    _pmnsNode	*np;

    for (tail = name; *tail && *tail != '.'; tail++)
	;
    nch = tail - name;

    for (np = root->first; np != (_pmnsNode *)0; np = np->next) {
	if (strncmp(name, np->name, (int)nch) == 0 && np->name[(int)nch] == '\0' &&
	    (np->pmid & MARK_BIT) == 0)
	    break;
    }

    if (np == (_pmnsNode *)0)
	return (_pmnsNode *)0;
    else if (*tail == '\0')
	return np;
    else
	return locate(tail+1, np);
}

/*
 * clear/set the "mark" bit used by pmTrimNameSpace, for all pmids
 */
static void
mark_all(int bit)
{
    int		i;
    _pmnsNode	*np;
    _pmnsNode	*pp;

    for (i = 0; i < htabsize; i++) {
	for (np = htab[i]; np != (_pmnsNode *)0; np = np->hash) {
	    for (pp = np ; pp != (_pmnsNode *)0; pp = pp->parent) {
		if (bit)
		    pp->pmid |= MARK_BIT;
		else
		    pp->pmid &= ~MARK_BIT;
	    }
	}
    }
}

/*
 * clear/set the "mark" bit used by pmTrimNameSpace, for one pmid, and
 * for all parent nodes on the path to the root of the PMNS
 */
static void
mark_one(pmID pmid, int bit)
{
    _pmnsNode	*np;

    for (np = htab[pmid % htabsize]; np != (_pmnsNode *)0; np = np->hash) {
	if ((np->pmid & PMID_MASK) == (pmid & PMID_MASK)) {
	    for ( ; np != (_pmnsNode *)0; np = np->parent) {
		if (bit)
		    np->pmid |= MARK_BIT;
		else
		    np->pmid &= ~MARK_BIT;
	    }
	    return;
	}
    }
}

#if !defined(LIBIRIXPMDA)
/*
 * 32-bit and 64-bit dependencies ... there are TWO external format,
 * both created by pmnscomp ... choose the correct one based upon
 * how big pointer is ...
 *
 * Magic cookies in the binary format file
 *	PmNs	- old 32-bit (Version 0)
 *	PmN1	- new 32-bit and 64-bit (Version 1)
 *	PmN2	- new 32-bit and 64-bit (Version 1 + checksum)
 *
 * if "secure", only Version 2 is allowed
 */
static int
loadbinary(int secure)
{
    FILE	*fbin;
    char	magic[4];
    int		nodecnt;
    int		symbsize;
    int		i;
    int		try;
    int		version;
    __int32_t	sum;
    __int32_t	chksum;
    long	endsum;
    __psint_t	ord;
    char	*symbol;

    for (try = 0; try < 2; try++) {
	if (try == 0) {
	    strcpy(linebuf, fname);
	    strcat(linebuf, ".bin");
	}
	else
	    strcpy(linebuf, fname);

#ifdef DEBUG
	if (pmDebug & DBG_TRACE_PMNS)
	    fprintf(stderr, "loadbinary(secure=%d file=%s)\n", secure, linebuf);
#endif
	if ((fbin = fopen(linebuf, "r")) == (FILE *)0)
	    continue;

	if (fread(magic, sizeof(magic), 1, fbin) != 1) {
	    fclose(fbin);
	    continue;
	}
	version = -1;
	if (strncmp(magic, "PmNs", 4) == 0) {
#if _MIPS_SZPTR != 32
	    _pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: old 32-bit format binary file \"%s\"", linebuf);
	    fclose(fbin);
	    continue;
#else
	    version = 0;
#endif
	}
	else if (strncmp(magic, "PmN1", 4) == 0)
	    version= 1;
	else if (strncmp(magic, "PmN2", 4) == 0) {
	    version= 2;
	    if (fread(&sum, sizeof(sum), 1, fbin) != 1) {
		fclose(fbin);
		continue;
	    }
	    endsum = ftell(fbin);
	    chksum = _pmCheckSum(fbin);
#ifdef DEBUG
	    if (pmDebug & DBG_TRACE_PMNS)
		fprintf(stderr, "Version 2 Binary PMNS Checksums: got=%x expected=%x\n", chksum, sum);
#endif
	    if (chksum != sum) {
		_pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: checksum failure for binary file \"%s\"", linebuf);
		fclose(fbin);
		continue;
	    }
	    fseek(fbin, endsum, SEEK_SET);
	}
	if (version == -1 || (secure && version != 2)) {
	    fclose(fbin);
	    continue;
	}

	if (version == 0) {
	    if (fread(&htabsize, sizeof(htabsize), 1, fbin) != 1) goto bad;
	    if (fread(&nodecnt, sizeof(nodecnt), 1, fbin) != 1) goto bad;
	    if (fread(&symbsize, sizeof(symbsize), 1, fbin) != 1) goto bad;

	    htab = (_pmnsNode **)malloc(htabsize * sizeof(htab[0]));
	    root = (_pmnsNode *)malloc(nodecnt * sizeof(*root));
	    symbol = (char *)malloc(symbsize);

	    if (htab == (_pmnsNode **)0 || root == (_pmnsNode *)0 || symbol == (char *)0) {
		_pmNoMem("loadbinary",
			 htabsize * sizeof(htab[0]) + nodecnt * sizeof(*root) + symbsize,
			 PM_FATAL_ERR);
		/*NOTREACHED*/
	    }

	    if (fread(htab, sizeof(htab[0]), htabsize, fbin) != htabsize) goto bad;
	    if (fread(root, sizeof(*root), nodecnt, fbin) != nodecnt) goto bad;
	    if (fread(symbol, sizeof(symbol[0]), symbsize, fbin) != symbsize) goto bad;
#ifdef DEBUG
	    if (pmDebug & DBG_TRACE_PMNS)
		fprintf(stderr, "Loaded Version 0 Binary PMNS\n");
#endif
	}
	else if (version == 1 || version == 2) {
	    int		sz_htab_ent;
	    int		sz_nodetab_ent;

	    if (fread(&symbsize, sizeof(symbsize), 1, fbin) != 1) goto bad;
	    symbol = (char *)malloc(symbsize);
	    if (symbol == (char *)0) {
		_pmNoMem("loadbinary-symbol", symbsize, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    if (fread(symbol, sizeof(symbol[0]), symbsize, fbin) != symbsize) goto bad;
	    /* once for each style ... or until EOF */
	    for ( ; ; ) {

		if (fread(&htabsize, sizeof(htabsize), 1, fbin) != 1) goto bad;
		if (fread(&sz_htab_ent, sizeof(sz_htab_ent), 1, fbin) != 1) goto bad;
		if (fread(&nodecnt, sizeof(nodecnt), 1, fbin) != 1) goto bad;
		if (fread(&sz_nodetab_ent, sizeof(sz_nodetab_ent), 1, fbin) != 1) goto bad;
		if (sz_htab_ent != sizeof(htab[0]) ||
		    sz_nodetab_ent != sizeof(*root)) {
		    long	skip;
		    skip = htabsize * sz_htab_ent + nodecnt * sz_nodetab_ent;
		    fseek(fbin, skip, SEEK_CUR);
		    continue;
		}

		/* the structure elements are all the right size */
		htab = (_pmnsNode **)malloc(htabsize * sizeof(htab[0]));
		root = (_pmnsNode *)malloc(nodecnt * sizeof(*root));

		if (htab == (_pmnsNode **)0 || root == (_pmnsNode *)0) {
		    _pmNoMem("loadbinary-1",
			     htabsize * sizeof(htab[0]) + nodecnt * sizeof(*root),
			     PM_FATAL_ERR);
		    /*NOTREACHED*/
		}

		if (fread(htab, sizeof(htab[0]), htabsize, fbin) != htabsize) goto bad;
		if (fread(root, sizeof(*root), nodecnt, fbin) != nodecnt) goto bad;
#ifdef DEBUG
		if (pmDebug & DBG_TRACE_PMNS)
		    fprintf(stderr, "Loaded Version 1 or 2 Binary PMNS, nodetab ent = %d bytes\n", sz_nodetab_ent);
#endif
		break;
	    }
	}

	fclose(fbin);

	/* relocate */
	for (i = 0; i < htabsize; i++) {
	    ord = (ptrdiff_t)htab[i];
	    if (ord == (__psint_t)-1)
		htab[i] = (_pmnsNode *)0;
	    else
		htab[i] = &root[ord];
	}
	for (i = 0; i < nodecnt; i++) {
	    ord = (__psint_t)root[i].parent;
	    if (ord == (__psint_t)-1)
		root[i].parent = (_pmnsNode *)0;
	    else
		root[i].parent = &root[ord];
	    ord = (__psint_t)root[i].next;
	    if (ord == (__psint_t)-1)
		root[i].next = (_pmnsNode *)0;
	    else
		root[i].next = &root[ord];
	    ord = (__psint_t)root[i].first;
	    if (ord == (__psint_t)-1)
		root[i].first = (_pmnsNode *)0;
	    else
		root[i].first = &root[ord];
	    ord = (__psint_t)root[i].hash;
	    if (ord == (__psint_t)-1)
		root[i].hash = (_pmnsNode *)0;
	    else
		root[i].hash = &root[ord];
	    ord = (__psint_t)root[i].name;
	    root[i].name = &symbol[ord];
	}

	return 1;
	
bad:
	_pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: bad binary file, \"%s\"", linebuf);
	fclose(fbin);
	return 0;
    }

    /* failed to open and/or find magic cookie */
    return 0;
}
#endif

/*
 * PMAPI routines from here down
 */

/*
 * fsa for parser
 *
 *	old	token	new
 *	0	NAME	1
 *	0	PATH	1
 *	1	LBRACE	2
 *      2	NAME	3
 *	2	RBRACE	0
 *	3	NAME	3
 *	3	PMID	2
 *	3	RBRACE	0
 */
static int
loadascii(int dupok)
{
    int		state = 0;
    int		type;
    _pmnsNode	*np;

    if (access(fname, R_OK) == -1) {
	sprintf(linebuf, "Cannot open \"%s\"", fname);
	err(linebuf);
	return -errno;
    }
    lineno = 1;

    while ((type = lex()) > 0) {
	switch (state) {

	case 0:
	    if (type != NAME && type != PATH) {
		err("Expected NAME or PATH");
		return PM_ERR_PMNS;
	    }
	    state = 1;
	    break;

	case 1:
	    if (type != LBRACE) {
		err("{ expected");
		return PM_ERR_PMNS;
	    }
	    state = 2;
	    break;

	case 2:
	    if (type == NAME) {
		state = 3;
	    }
	    else if (type == RBRACE) {
		state = 0;
	    }
	    else {
		err("Expected NAME or }");
		return PM_ERR_PMNS;
	    }
	    break;

	case 3:
	    if (type == NAME) {
		state = 3;
	    }
	    else if (type == PMID) {
		np->pmid = tokpmid;
		state = 2;
	    }
	    else if (type == RBRACE) {
		state = 0;
	    }
	    else {
		err("Expected NAME, PMID or }");
		return PM_ERR_PMNS;
	    }
	    break;

	default:
	    err("Internal botch");
	    abort();

	}

	if (state == 1 || state == 3) {
	    if ((np = (_pmnsNode *)malloc(sizeof(*np))) == (_pmnsNode *)0)
		return -errno;
	    numpmid++;
	    if ((np->name = (char *)malloc(strlen(tokbuf)+1)) == (char *)0)
		return -errno;
	    strcpy(np->name, tokbuf);
	    np->first = np->hash = np->next = np->parent = (_pmnsNode *)0;
	    np->pmid = PM_ID_NULL;
	    if (state == 1) {
		np->next = seen;
		seen = np;
	    }
	    else {
		if (seen->hash)
		    seen->hash->next = np;
		else
		    seen->first = np;
		seen->hash = np;
	    }
	}
	else if (state == 0) {
	    if (seen) {
		_pmnsNode	*xp;

		for (np = seen->first; np != (_pmnsNode *)0; np = np->next) {
		    for (xp = np->next; xp != (_pmnsNode *)0; xp = xp->next) {
			if (strcmp(xp->name, np->name) == 0) {
			    sprintf(linebuf, "Duplicate name \"%s\" in subtree for \"%s\"\n",
			        np->name, seen->name);
			    err(linebuf);
			    return PM_ERR_PMNS;
			}
		    }
		}
	    }
	}
    }

    if (type == 0)
	type = pass2(dupok);

    if (type == 0) {
	mark_all(0);
#ifdef MALLOC_AUDIT
	_malloc_reset_();
	atexit(_malloc_audit_);
#endif
#ifdef DEBUG
	if (pmDebug & DBG_TRACE_PMNS)
	    fprintf(stderr, "Loaded ASCII PMNS\n");
#endif
    }

    return type;
}

static int
load(char *filename, int binok, int dupok)
{
#if !defined(LIBIRIXPMDA)
    int		cap;
#endif
    int 	i;

    if (root != (_pmnsNode *)0)
	return PM_ERR_DUPPMNS;

#if !defined(LIBIRIXPMDA)
#ifdef DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
	fprintf(stderr, "pmLoadNameSpace: control table\n");
	for (i = 0; i < numctl; i++) {
	    fprintf(stderr, "[%d] cap=0x%x ascii=%d secure=%d root=%s\n", i,
		ctltab[i].cap, ctltab[i].ascii, ctltab[i].secure, ctltab[i].root);
	}
    }
#endif

    /*
    cap = _pmGetLicenseCap();
    for (i = 0; i < numctl; i++) {
	if ((cap & ctltab[i].cap) == ctltab[i].cap)
	    break;
    }
    if (i == numctl)
	return PM_ERR_LICENSE;
    */

    i = 0;

    /*
     * 0xffffffff is there to maintain backwards compatibility with
     * PCP 1.0
     */
    if (filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff)
	strcpy(fname, ctltab[i].root);
    else
	strcpy(fname, filename);
#else
    if (filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff) {
	/* in the libirixpmda build context, we _must_ give a file name */
	fprintf(stderr, "pmLoadNameSpace: require filename for libirixpmda build use!\n");
	exit(1);
    }
    strcpy(fname, filename);
#endif

#ifdef DEBUG
    if (pmDebug & DBG_TRACE_PMNS)
	fprintf(stderr, "load(name=%s, binok=%d, dupok=%d) lic case=%d fname=%s\n",
		filename, binok, dupok, i, fname);
#endif

#if !defined(LIBIRIXPMDA)
    /* try the easy way, c/o pmnscomp */
    if (binok && loadbinary(ctltab[i].secure)) {
	mark_all(0);
	return 0;
    }

    if (!ctltab[i].ascii)
	return PM_ERR_LICENSE;
#endif

    /*
     * the hard way, compiling as we go ...
     */
    return loadascii(dupok);
}

int
pmLoadNameSpace(char *filename)
{
    return load(filename, 1, 0);
}

int
pmLoadASCIINameSpace(char *filename, int dupok)
{
    return load(filename, 0, dupok);
}

int
pmLookupName(int numpmid, char *namelist[], pmID pmidlist[])
{
    int		i;
    int		status = 0;
    _pmnsNode	*np;

    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    if (root == (_pmnsNode *)0)
	return PM_ERR_NOPMNS;

    for (i = 0; i < numpmid; i++) {
	if ((np = locate(namelist[i], root)) != (_pmnsNode *)0 &&
	    np->first == (_pmnsNode *)0)
		pmidlist[i] = np->pmid;
	else {
	    status = PM_ERR_NAME;
	    pmidlist[i] = PM_ID_NULL;
	}
    }

    return status ? status : i;
}

#if !defined(LIBIRIXPMDA)
int
pmGetChildren(char *name, char ***offspring)
{
    _pmnsNode	*np;
    _pmnsNode	*tnp;
    int		i;
    int		need;
    int		num;
    char	**result;
    char	*p;

    /* avoids ambiguity, for errors and leaf nodes */
    *offspring = (char **)0;

    if (root == (_pmnsNode *)0)
	return PM_ERR_NOPMNS;

    if (name == (char *)0) {
	/* avoid the mem fault, ... */
	return PM_ERR_NAME;
    }
    else if (*name == '\0')
	/* use "" to name the root of the PMNS */
	np = root;
    else if ((np = locate(name, root)) == (_pmnsNode *)0)
	return PM_ERR_NAME;

    if (np->first == (_pmnsNode *)0)
	/* this is a leaf node */
	return 0;

    need = 0;
    num = 0;
    for (i = 0, tnp = np->first; tnp != (_pmnsNode *)0; tnp = tnp->next, i++) {
	if ((tnp->pmid & MARK_BIT) == 0) {
	    num++;
	    need += sizeof(**offspring) + strlen(tnp->name) + 1;
	}
    }

    if ((result = (char **)malloc(need)) == (char **)0)
	return -errno;
    p = (char *)&result[num];

    for (i = 0, tnp = np->first; tnp != (_pmnsNode *)0; tnp = tnp->next) {
	if ((tnp->pmid & MARK_BIT) == 0) {
	    result[i] = p;
	    strcpy(result[i], tnp->name);
	    p += strlen(tnp->name) + 1;
	    i++;
	}
    }

    *offspring = result;
    return num;
}

int
pmNameID(pmID pmid, char **name)
{
    _pmnsNode	*np;

    if (root == (_pmnsNode *)0)
	return PM_ERR_NOPMNS;

    for (np = htab[pmid % htabsize]; np != (_pmnsNode *)0; np = np->hash) {
	if (np->pmid == pmid)
	    return backname(np, name);
    }

    return PM_ERR_PMID;
}

int
pmNameAll(pmID pmid, char ***name)
{
    _pmnsNode	*np;
    int		sts;
    int		n = 0;
    int		len = 0;
    int		i;
    char	*sp;
    char	**tmp = (char **)0;

    if (root == (_pmnsNode *)0)
	return PM_ERR_NOPMNS;

    for (np = htab[pmid % htabsize]; np != (_pmnsNode *)0; np = np->hash) {
	if (np->pmid == pmid) {
	    n++;
	    if ((tmp = (char **)realloc(tmp, n * sizeof(tmp[0]))) == (char **)0)
		return -errno;
	    if ((sts = backname(np, &tmp[n-1])) < 0) {
		/* error, ... free any partial allocations */
		for (i = n-2; i >= 0; i--)
		    free(tmp[i]);
		free(tmp);
		return sts;
	    }
	    len += strlen(tmp[n-1])+1;
	}
    }

    if (n == 0)
	return PM_ERR_PMID;

    len += n * sizeof(tmp[0]);
    if ((tmp = (char **)realloc(tmp, len)) == (char **)0)
	return -errno;

    sp = (char *)&tmp[n];
    for (i = 0; i < n; i++) {
	strcpy(sp, tmp[i]);
	free(tmp[i]);
	tmp[i] = sp;
	sp += strlen(sp)+1;
    }

    *name = tmp;
    return n;
}

int
pmTrimNameSpace(void)
{
    int		i;
    _pmContext	*ctxp;
    _pmHashCtl	*hcp;
    _pmHashNode	*hp;

    if (root == (_pmnsNode *)0)
	return PM_ERR_NOPMNS;

    if ((ctxp = _pmHandleToPtr(pmWhichContext())) == (_pmContext *)0)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	/* set all of the marks, and done if a host context */
	mark_all(0);
	return 0;
    }

    /*
     * this is an archive, so (1) set all of the marks, and
     * (2) clear the marks for those metrics defined in the archive
     */
    mark_all(1);
    hcp = &ctxp->c_archctl->ac_log->l_hashpmid;

    for (i = 0; i < hcp->hsize; i++) {
	for (hp = hcp->hash[i]; hp != (_pmHashNode *)0; hp = hp->next) {
	    mark_one((pmID)hp->key, 0);
	}
    }

    return 0;
}

void
_pmDumpNameSpace(FILE *f, int verbosity)
{
    dumptree(f, 0, root, verbosity);
}

/*
 * just for pmnscomp to use
 */
void
_pmExportPMNS(_pmnsNode **rootp, int *htablen, _pmnsNode ***htabp)
{
    *rootp = root;
    *htablen = htabsize;
    *htabp = htab;
}
#endif
