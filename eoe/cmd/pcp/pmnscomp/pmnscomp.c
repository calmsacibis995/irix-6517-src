/*
 * pmnscomp [-n pmnsfile ] [-d debug] outfile
 *
 * Construct a compiled PMNS suitable for "fast" loading in pmLoadNameSpace
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

#ident "$Id: pmnscomp.c,v 2.14 1999/05/11 00:28:03 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

static int	nodecnt;	/* number of nodes */
static int	leafcnt;	/* number of leaf nodes */
static int	symbsize;	/* aggregate string table size */
static __pmnsNode	**_htab;	/* output htab[] */
static __pmnsNode	*_nodetab;	/* output nodes */
static char	*_symbol;	/* string table */
static char	*symp;		/* pointer into same */
static __pmnsNode	**_map;		/* point-to-ordinal map */
static int	i;		/* node counter for traversal */

static int	version = 1;	/* default output format version */

/*
 * 32-bit pointer version of __pmnsNode
 */
typedef struct {
    __int32_t	parent;
    __int32_t	next;
    __int32_t	first;
    __int32_t	hash;
    __int32_t	name;
    pmID	pmid;
} __pmnsNode32;
static __int32_t	*_htab32;
static __pmnsNode32	*_nodetab32;

/*
 * 64-bit pointer version of __pmnsNode
 */
typedef struct {
    __int64_t	parent;
    __int64_t	next;
    __int64_t	first;
    __int64_t	hash;
    __int64_t	name;
    pmID	pmid;
    __int32_t	__pad__;
} __pmnsNode64;
static __int64_t	*_htab64;
static __pmnsNode64	*_nodetab64;

static void
dumpmap(void)
{
    int		n;
    for (n = 0; n < nodecnt; n++) {
	if (n % 8 == 0) {
	    if (n)
		putchar('\n');
	    printf("map[%3d]", n);
	}
	printf(" %08x", _map[n]);
    }
    putchar('\n');
}

static long
nodemap(__pmnsNode *p)
{
    int		n;

    if (p == NULL)
	return -1;

    for (n = 0; n < nodecnt; n++) {
	if (_map[n] == p)
	    return n;
    }
    printf("%s: fatal error, cannot map node addr 0x%x\n", pmProgname, p);
    dumpmap();
    exit(1);
    /*NOTREACHED*/
}

static void
traverse(__pmnsNode *p, void(*func)(__pmnsNode *this))
{
    if (p != NULL) {
	(*func)(p);
	traverse(p->first, func);
	traverse(p->next, func);
    }
}

#ifdef PCP_DEBUG
static void
chkascii(char *tag, char *p)
{
    int	i = 0;
    while (*p) {
	if (!isascii(*p) || !isprint(*p)) {
	    printf("chkascii: %s: non-printable char 0x%02x in \"%s\"[%d] @ 0x%08x\n",
		tag, *p & 0xff, p, i, p);
	    exit(1);
	}
	i++;
	p++;
    }
}

static char *chktag;

static void
chknames(__pmnsNode *p)
{
    chkascii(chktag, p->name);
}
#endif

static void
pass1(__pmnsNode *p)
{
    nodecnt++;
    if (p->pmid != PM_ID_NULL && p->first == NULL)
	leafcnt++;
    symbsize += strlen(p->name)+1;
}

static void
pass2(__pmnsNode *p)
{
    ptrdiff_t	offset;
    _map[i] = p;
    _nodetab[i] = *p;	/* struct assignment */
    strcpy(symp, p->name);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	chkascii("pass2 symtab", symp);
#endif
    offset = symp - _symbol;
    symp += strlen(p->name);
    *symp++ = '\0';
    _nodetab[i].name = (char *)offset;
    i++;
}

/*ARGSUSED*/
static void
pass3(__pmnsNode *p)
{
    _nodetab[i].parent = (struct pn_s *)nodemap(_nodetab[i].parent);
    _nodetab[i].next = (struct pn_s *)nodemap(_nodetab[i].next);
    _nodetab[i].first = (struct pn_s *)nodemap(_nodetab[i].first);
    _nodetab[i].hash = (struct pn_s *)nodemap(_nodetab[i].hash);
    i++;
}

void
main(int argc, char **argv)
{
    int		n;
    int		c;
    int		j;
    int		sts;
    int		force = 0;
    int		dupok = 0;
    char	*pmnsfile = PM_NS_DEFAULT;
    int		errflag = 0;
    char	*endnum;
    FILE	*outf;
    __pmnsNode	*root;
    __pmnsNode	**htab;
    int		htabcnt;	/* count of the number of htab[] entries */
    __int32_t	tmp;
    __int32_t	sum;
    long	startsum;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;
    char	*p;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "dD:fn:v:?")) != EOF) {
	switch (c) {

	case 'd':	/* duplicate PMIDs are allowed */
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

	case 'n':	/* alternative namespace file */
	    pmnsfile = optarg;
	    break;

	case 'v':	/* alternate version */
	    version = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -v requires numeric argument\n", pmProgname);
		errflag++;
	    }
#ifdef BUILDTOOL
	    if (version < 0 || version > 2) {
		fprintf(stderr, "%s: version must be 0, 1 or 2\n", pmProgname);
		errflag++;
	    }
#else
	    if (version < 0 || version > 1) {
		fprintf(stderr, "%s: version must be 0 or 1\n", pmProgname);
		errflag++;
	    }
#endif
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-1) {
	fprintf(stderr,
"Usage: %s [options] outfile\n\
\n\
Options:\n\
  -d		duplicate PMIDs are allowed\n\
  -f		force overwriting of an existing output file if it exists\n\
  -n pmnsfile 	use an alternative PMNS\n\
  -v version	alternate output format version\n",
			pmProgname);	
	exit(1);
    }

    if (force) {
	struct stat	sbuf;
	if (stat(argv[optind], &sbuf) == -1) {
	    if (errno != ENOENT) {
		fprintf(stderr, "%s: cannot stat \"%s\": %s\n",
		    pmProgname, argv[optind], strerror(errno));
		exit(1);
	    }
	}
	else {
	    /* stat is OK, so exists ... must be a regular file */
	    if (!S_ISREG(sbuf.st_mode)) {
		fprintf(stderr, "%s: \"%s\" is not a regular file\n",
		    pmProgname, argv[optind]);
		exit(1);
	    }
	    if (unlink(argv[optind]) == -1) {
		fprintf(stderr, "%s: cannot unlink \"%s\": %s\n",
		    pmProgname, argv[optind], strerror(errno));
		exit(1);
	    }
	}
    }

    if (access(argv[optind], F_OK) == 0) {
	fprintf(stderr, "%s: \"%s\" already exists!\nYou must either remove it first, or use -f\n",
		pmProgname, argv[optind]);
	exit(1);
    }

    if ((n = pmLoadASCIINameSpace(pmnsfile, dupok)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(n));
	exit(1);
    }

    {
        __pmnsTree *t;
	t = __pmExportPMNS();
	if (t == NULL) {
	   /* sanity check - shouldn't ever happen */
	   fprintf(stderr, "%s: Exported PMNS is NULL!\n", pmProgname);
	   exit(1);
	}
	root = t->root;
	htabcnt = t->htabsize;
	htab = t->htab;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	chktag = "pmLoadASCIINameSpace";
	traverse(root, chknames);
    }
#endif

    traverse(root, pass1);

    _htab = (__pmnsNode **)malloc(htabcnt * sizeof(_htab[0]));
    _nodetab = (__pmnsNode *)malloc(nodecnt * sizeof(_nodetab[0]));
    symp = _symbol = (char *)malloc(symbsize);
    _map = (__pmnsNode **)malloc(nodecnt * sizeof(_map[0]));

    if (_htab == NULL || _nodetab == NULL ||
	symp == NULL || _map == NULL) {
	    __pmNoMem("pmnscomp", htabcnt * sizeof(_htab[0]) +
				 nodecnt * sizeof(_nodetab[0]) +
				 symbsize +
				 nodecnt * sizeof(_map[0]), PM_FATAL_ERR);
	    /*NOTREACHED*/
    }

#ifdef MALLOC_AUDIT
    _persistent_(_htab);
    _persistent_(_nodetab);
    _persistent_(symp);
    _persistent_(_map);
#endif

    i = 0;
    traverse(root, pass2);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	chktag = "pass2";
	traverse(root, chknames);
    }
#endif

    memcpy(_htab, htab, htabcnt * sizeof(htab[0]));
    for (j = 0; j < htabcnt; j++)
	_htab[j] = (__pmnsNode *)nodemap(_htab[j]);

    i = 0;
    traverse(root, pass3);

    /*
     * from here on, ignore SIGINT, SIGHUP and SIGTERM to protect
     * the integrity of the new ouput file
     */
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if ((outf = fopen(argv[optind], "w+")) == NULL) {
	fprintf(stderr, "%s: cannot create \"%s\": %s\n", pmProgname, argv[optind], strerror(errno));
	exit(1);
    }

    /*
     * Format verisons
     *	0	PmNs	- original PCP 1.0, 32-bit format
     * 	1	PmN1	- PCP 1.1 format with both 32 and 64-bit formats
     *			  for MIPS
     *  2       PmN2	- same as PmN1, but with initial checksum
     *
     * Note: all of this must be understood by pmLoadNameSpace() as well
     */
    if (version == 0)
	fprintf(outf, "PmNs");
    else if (version == 1)
	fprintf(outf, "PmN1");
    else if (version == 2)
	fprintf(outf, "PmN2");

    if (version == 2) {
	/* dummy at this stage, filled in later */
	fwrite(&sum, sizeof(sum), 1, outf);
	startsum = ftell(outf);
    }

    if (version == 0) {
	/* don't bother doing endian conversion */
	fwrite(&htabcnt, sizeof(int), 1, outf);
	fwrite(&nodecnt, sizeof(int), 1, outf);
	fwrite(&symbsize, sizeof(int), 1, outf);

	fwrite(_htab, sizeof(_htab[0]), htabcnt, outf);
	fwrite(_nodetab, sizeof(_nodetab[0]), nodecnt, outf);
	fwrite(_symbol, sizeof(_symbol[0]), symbsize, outf);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    __psint_t	k;
	    printf("Hash Table\n");
	    for (j = 0; j < htabcnt; j++) {
		if (j % 10 == 0) {
		    if (j)
			putchar('\n');
		    printf("htab[%3d]", j);
		}
		printf(" %5d", _htab[j]);
	    }
	    printf("\n\nNode Table\n");
	    for (j = 0; j < nodecnt; j++) {
		if (j % 20 == 0)
		    printf("           Parent   Next  First   Hash Symbol           PMID\n");
		k = (__psint_t)_nodetab[j].name;
		printf("node[%4d] %6d %6d %6d %6d %-16.16s",
		    j, _nodetab[j].parent, _nodetab[j].next, _nodetab[j].first,
		    _nodetab[j].hash, &_symbol[k]);
		if (_nodetab[j].first == (__pmnsNode *)-1)
		    printf(" %s", pmIDStr(_nodetab[j].pmid));
		putchar('\n');
	    }
	}
#endif
    }
    else if(version == 1 || version == 2) {
	/*
	 * Version 1, after label, comes repetitions of, one for each "style"
	 *
	 * <symbsize>				| __int32_t
	 * <_symbol>				|
	 * <htabcnt><size of _htab[0]>		| style 1, __int32_t
	 * <nodecnt><size of _nodetab[0]>	| __int32_t
	 * <_htab>
	 * <_nodetab>
	 * <htabcnt><size of _htab[0]>		| style 2, __int32_t
	 * <nodecnt><size of _nodetab[0]>	| __int32_t
	 * <_htab>
	 * <_nodetab>
	 * ....					| style 3, ...
	 *
	 * Version 2 is similar, except the checksum follows the label,
	 * then as for Version 1.
	 */

	tmp = htonl((__int32_t)symbsize);
	fwrite(&tmp, sizeof(tmp), 1, outf);

	fwrite(_symbol, sizeof(_symbol[0]), symbsize, outf);

	tmp = htonl((__int32_t)htabcnt);
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)sizeof(_htab32[0]));
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)nodecnt);
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)sizeof(_nodetab32[0]));
	fwrite(&tmp, sizeof(tmp), 1, outf);

	_htab32 = (__int32_t *)malloc(htabcnt * sizeof(__int32_t));
	for (j = 0; j < htabcnt; j++)
	    _htab32[j] = htonl((__int32_t)_htab[j]);
	fwrite(_htab32, sizeof(_htab32[0]), htabcnt, outf);
	_nodetab32 = (__pmnsNode32 *)malloc(nodecnt * sizeof(__pmnsNode32));
	for (j = 0; j < nodecnt; j++) {
	    _nodetab32[j].parent = htonl((__int32_t)_nodetab[j].parent);
	    _nodetab32[j].next = htonl((__int32_t)_nodetab[j].next);
	    _nodetab32[j].first = htonl((__int32_t)_nodetab[j].first);
	    _nodetab32[j].hash = htonl((__int32_t)_nodetab[j].hash);
	    _nodetab32[j].name = htonl((__int32_t)_nodetab[j].name);
	    _nodetab32[j].parent = htonl((__int32_t)_nodetab[j].parent);
	    _nodetab32[j].pmid = __htonpmID(_nodetab[j].pmid);
	}
	fwrite(_nodetab32, sizeof(_nodetab32[0]), nodecnt, outf);

	tmp = htonl((__int32_t)htabcnt);
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)sizeof(_htab64[0]));
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)nodecnt);
	fwrite(&tmp, sizeof(tmp), 1, outf);
	tmp = htonl((__int32_t)sizeof(_nodetab64[0]));
	fwrite(&tmp, sizeof(tmp), 1, outf);

	_htab64 = (__int64_t *)malloc(htabcnt * sizeof(__int64_t));
	for (j = 0; j < htabcnt; j++) {
	    /*
	     * Danger ahead ... serious cast games here to convert
	     * 32-bit ptrs to 64-bit integers _with_ sign extension
	     */
	    __psint_t	k;
	    k = (__psint_t)_htab[j];
#if !defined(HAVE_NETWORK_BYTEORDER)
            __htonll((char *)&k);
#endif
	    _htab64[j] = (__int64_t)k;
	}
	fwrite(_htab64, sizeof(_htab64[0]), htabcnt, outf);
	_nodetab64 = (__pmnsNode64 *)malloc(nodecnt * sizeof(__pmnsNode64));
	for (j = 0; j < nodecnt; j++) {
	    /*
	     * Danger ahead ... serious cast games here to convert
	     * 32-bit ptrs to 64-bit integers _with_ sign extension
	     */
	    __psint_t	parent, next, first, hash, name;

	    parent = (__psint_t)_nodetab[j].parent;
	    next = (__psint_t)_nodetab[j].next;
	    first = (__psint_t)_nodetab[j].first;
	    hash = (__psint_t)_nodetab[j].hash;
	    name = (__psint_t)_nodetab[j].name;
#if !defined(HAVE_NETWORK_BYTEORDER)
	    __htonll((char *)&parent);
	    __htonll((char *)&next);
	    __htonll((char *)&first);
	    __htonll((char *)&hash);
	    __htonll((char *)&name);
#endif
	    _nodetab64[j].parent = (__int64_t)parent;
	    _nodetab64[j].next = (__int64_t)next;
	    _nodetab64[j].first = (__int64_t)first;
	    _nodetab64[j].hash = (__int64_t)hash;
	    _nodetab64[j].name = (__int64_t)name;

	    _nodetab64[j].pmid = __htonpmID(_nodetab[j].pmid);
	    _nodetab64[j].__pad__ = htonl(0xdeadbeef);
	}
	fwrite(_nodetab64, sizeof(_nodetab64[0]), nodecnt, outf);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    __int32_t	k32;
	    __int64_t	k64;
	    printf("32-bit Header: htab[%d] x %d bytes, nodetab[%d] x %d bytes\n",
		htabcnt, sizeof(_htab32[0]), nodecnt, sizeof(_nodetab32[0]));
	    printf("\n32-bit Hash Table\n");
	    for (j = 0; j < htabcnt; j++) {
		if (j % 10 == 0) {
		    if (j)
			putchar('\n');
		    printf("htab32[%3d]", j);
		}
		printf(" %5d", _htab32[j]);
	    }
	    printf("\n\n32-bit Node Table\n");
	    for (j = 0; j < nodecnt; j++) {
		if (j % 20 == 0)
		    printf("             Parent   Next  First   Hash Symbol           PMID\n");
		k32 = _nodetab32[j].name;
		printf("node32[%4d] %6d %6d %6d %6d %-16.16s",
		    j, _nodetab32[j].parent, _nodetab32[j].next, _nodetab32[j].first,
		    _nodetab32[j].hash, &_symbol[k32]);
		if (_nodetab32[j].first == -1)
		    printf(" %s", pmIDStr(_nodetab32[j].pmid));
		putchar('\n');
	    }
	    printf("\n64-bit Header: htab[%d] x %d bytes, nodetab[%d] x %d bytes\n",
		htabcnt, sizeof(_htab64[0]), nodecnt, sizeof(_nodetab64[0]));
	    printf("\n64-bit Hash Table\n");
	    for (j = 0; j < htabcnt; j++) {
		if (j % 10 == 0) {
		    if (j)
			putchar('\n');
		    printf("htab64[%3d]", j);
		}
		printf(" %5lld", _htab64[j]);
	    }
	    printf("\n\n64-bit Node Table\n");
	    for (j = 0; j < nodecnt; j++) {
		if (j % 20 == 0)
		    printf("             Parent   Next  First   Hash Symbol           PMID\n");
		k64 = _nodetab64[j].name;
		printf("node64[%4d] %6lld %6lld %6lld %6lld %-16.16s",
		    j, _nodetab64[j].parent, _nodetab64[j].next, _nodetab64[j].first,
		    _nodetab64[j].hash, &_symbol[(int)k64]);
		if (_nodetab64[j].first == -1)
		    printf(" %s", pmIDStr(_nodetab64[j].pmid));
		putchar('\n');
	    }
	}
#endif
    }

    if (version == 2) {
	fseek(outf, startsum, SEEK_SET);
	sum = __pmCheckSum(outf);
	fseek(outf, startsum - (long)sizeof(sum), SEEK_SET);
	tmp = htonl(sum);
	fwrite(&tmp, sizeof(sum), 1, outf);
    }


#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	if (version == 2)
	    printf("\nChecksum 0x%x\n", sum);
	printf("\nSymbol Table\n");
	for (j = 0; j < symbsize; j++) {
	    if (j % 50 == 0) {
		if (j)
		    putchar('\n');
		printf("symbol[%4d]  ", j);
	    }
	    if (_symbol[j])
		putchar(_symbol[j]);
	    else
		putchar('*');
	}
	putchar('\n');
    }
#endif

    printf("Compiled PMNS contains\n\t%5d hash table entries\n\t%5d leaf nodes\n\t%5d non-leaf nodes\n\t%5d bytes of symbol table\n",
	htabcnt, leafcnt, nodecnt - leafcnt, symbsize);

    exit(0);
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
