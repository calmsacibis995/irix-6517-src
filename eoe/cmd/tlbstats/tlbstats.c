#ident "$Revision: 1.3 $"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "stdlib.h"
#include "malloc.h"
#include "tlbstats.h"

/*
 * tlbstats - simulate tlb hardware
 *
 * The best way to run this on a 'real' program is to
 *	1) pixie -tlbtrace -idtrace_file 9 pgm
 *	2) mknod pipe p
 *	3) tlbstats <pipe&
 *	4) pgm 9>pipe (using ksh)
 */

#define BUFFERSIZE ((128 * 1024)/ sizeof(vaddr))

tlbmodule_t tlbalgs[] = {
	{ "utlbonly", utlbonly_init, utlbonly_probe, utlbonly_fill, 
		utlbonly_print, utlbonlyparms} ,
	{ "irix4.0", irix40_init, irix40_probe, irix40_fill, 
		irix40_print, irix40r3kparms} ,
	{ "irix4.0r4k", irix40_init, irix40_probe, irix40_fill, 
		irix40_print, irix40r4kparms} ,
	0
};

int verbose = 0;
/* a list of all args for all algorithms */
args_t cmdargs[] = {
	{ "algorithm", OPT_ALGORITHM, 0 },
	{ "entries", OPT_ENTRIES, 0 },
	{ "wired", OPT_WIRED, 0 },
	{ "cachehit", OPT_CACHEHIT, 0 },
	{ "mincachepenalty", OPT_MINCACHEPENALTY, 0 },
	{ "maxcachepenalty", OPT_MAXCACHEPENALTY, 0 },
	{ "pagesize", OPT_PAGESIZE, 0 },
	{ "maps2", OPT_MAPS2, 0 },
	{ "v", OPT_VERBOSE, 0 },
	{ "1stpenalty", OPT_FIRSTPENALTY, 0 },
	{ "1.5penalty", OPT_ONEFIVEPENALTY, 0 },
	{ "2ndpenalty", OPT_SECONDPENALTY, 0 },
	{ 0 }
};
static void parseargs(int, char **, tlbparms_t *, tlbmodule_t **);
static void Usage(void);

main(int argc, char **argv)
{
	auto tlbparms_t tp;
	auto tlbmodule_t *tm;
	int buf[BUFFERSIZE], *bp;
	int i, fd, rv;
	int (*prober)(tlbparms_t *, vaddr);

	parseargs(argc, argv, &tp, &tm);

	printf("Using '%s' algorithm with %d entries %d wired entries\n",
		tm->t_name, tp.t_nentries, tp.t_nwired);
	printf(" pte fetch hit percent %d pte miss penalty min:%d max:%d cycles\n",
		tp.t_cachehit, tp.t_mincachepenalty, 
		tp.t_maxcachepenalty);
	printf(" pagesize %d bytes each entry maps %d pages\n",
		tp.t_pagesize, tp.t_flags & TLB_MAPS2 ? 2 : 1);

	tm->t_tlbinit(&tp);
	fd = fileno(stdin);
	prober = tm->t_tlbprobe;
	while ((rv = read(fd, buf, sizeof(buf))) > 0) {
		if (rv % sizeof(vaddr) != 0) {
			fprintf(stderr, "read not on word boundary!\n");
			exit(1);
		}
		rv /= sizeof(vaddr);
		for (i = 0, bp = buf; i < rv; i++, bp++) {
			if (prober(&tp, *bp) < 0)
				 tm->t_tlbfill(&tp, *bp);
		}
	}
	if (rv < 0) {
		perror("read error");
		exit(1);
	}
	tm->t_tlbprint(&tp);

	return(0);
}

static void
parseargs(int argc, char **argv, tlbparms_t *tp, tlbmodule_t **tm)
{
	args_t *ap, *tap;
	tlbmodule_t *tmodule;
	char *source;
	int i, found;

	*tm = NULL;
	for (i = 1, argv++; i < argc; i++, argv++) {
		if (**argv != '-')
			break;
		for (found = 0, ap = cmdargs; ap->name; ap++) {
			if (strstr(ap->name, (*argv)+1) == ap->name) {
				tap = ap;
				found++;
			}
		}
		if (found != 1) {
			fprintf(stderr, "Illegal option %s\n", *argv);
			Usage();
			/* NOTREACHED */
		}
		switch (tap->id) {
		case OPT_ALGORITHM:
		case OPT_ENTRIES:
		case OPT_WIRED:
		case OPT_CACHEHIT:
		case OPT_MINCACHEPENALTY:
		case OPT_MAXCACHEPENALTY:
		case OPT_PAGESIZE:
		case OPT_FIRSTPENALTY:
		case OPT_ONEFIVEPENALTY:
		case OPT_SECONDPENALTY:
			i++;
			argv++;
			/* need another arg */
			if (i >= argc || **argv == '-') {
				fprintf(stderr, "Option %s requires an argument\n",
					tap->name);
				Usage();
			}

			break;
		}

		switch (tap->id) {
		case OPT_ALGORITHM:
			for (tmodule = tlbalgs; tmodule->t_name; tmodule++)
				if (strcmp(tmodule->t_name, *argv) == 0)
					break;
			if (!tmodule->t_name) {
				fprintf(stderr, "Unknown algorithm %s\n",
					*argv);
				Usage();
			}
			*tm = tmodule;
			break;
		case OPT_WIRED:
		case OPT_CACHEHIT:
		case OPT_MINCACHEPENALTY:
		case OPT_MAXCACHEPENALTY:
		case OPT_PAGESIZE:
		case OPT_ENTRIES:
		case OPT_FIRSTPENALTY:
		case OPT_ONEFIVEPENALTY:
		case OPT_SECONDPENALTY:
			tap->defvalue = *argv;
			break;
		case OPT_MAPS2:
			tap->defvalue = "TRUE";
			break;
		case OPT_VERBOSE:
			verbose++;
			break;
		}
	}

	/* handle defaults */
	if (!*tm)
		/* use irix4.0 R3000 */
		*tm = &tlbalgs[1];

	/* set option info into tp */
	for (tap = (*tm)->t_parms; tap->name; tap++) {
		/* see if user gave us something */
		source = tap->defvalue;
		for (ap = cmdargs; ap->name; ap++) {
			if (tap->id == ap->id) {
				if (ap->defvalue) {
					source = ap->defvalue;
					break;
				}
				break;
			}
		}
		switch (tap->id) {
		case OPT_ENTRIES:
			tp->t_nentries = strtol(source, NULL, 0);
			break;
		case OPT_WIRED:
			tp->t_nwired = strtol(source, NULL, 0);
			break;
		case OPT_CACHEHIT:
			tp->t_cachehit = strtol(source, NULL, 0);
			break;
		case OPT_MINCACHEPENALTY:
			tp->t_mincachepenalty = strtol(source, NULL, 0);
			break;
		case OPT_MAXCACHEPENALTY:
			tp->t_maxcachepenalty = strtol(source, NULL, 0);
			break;
		case OPT_FIRSTPENALTY:
			tp->t_1stpenalty = strtol(source, NULL, 0);
			break;
		case OPT_SECONDPENALTY:
			tp->t_2ndpenalty = strtol(source, NULL, 0);
			break;
		case OPT_ONEFIVEPENALTY:
			tp->t_15penalty = strtol(source, NULL, 0);
			break;
		case OPT_PAGESIZE:
			{
			int psz, pshft;
			psz = strtol(source, NULL, 0);
			if ((psz & (psz - 1)) != 0) {
				fprintf(stderr, "pagesize %d must be a power of 2!\n",
					psz);
				Usage();
			}
			tp->t_pagesize = psz;
			/* compute shift */
			for (pshft = 0; psz; psz >>= 1, pshft++)
				;
			tp->t_pageshift = --pshft;
			break;
			}
		case OPT_MAPS2:
			if (strcmp(source, "TRUE") == 0)
				tp->t_flags |= TLB_MAPS2;
			break;
		}
	}
}

static void
Usage(void)
{
	args_t *ap;
	tlbmodule_t *tmodule;

	fprintf(stderr, "Usage:tlbstats -algorithm alg_name [options] <trace_file\n");
	for (tmodule = tlbalgs; tmodule->t_name; tmodule++) {
		fprintf(stderr, " Algorithm '%s' options:\n", tmodule->t_name);
		for (ap = tmodule->t_parms; ap->name; ap++) {
			if (strcmp(ap->defvalue, "0") == 0)
				fprintf(stderr, "\t[-%s]\n", ap->name);
			else
				fprintf(stderr, "\t[-%s (default %s)]\n",
					ap->name, ap->defvalue);
		}
	}
	exit(-1);
}
