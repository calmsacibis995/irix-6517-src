/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Netsnoop option handling.
 */
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "exception.h"
#include "expr.h"
/* #include "getopt.h" */
#include "heap.h"
#include "options.h"
#include "packetview.h"
#include "protocol.h"
#include "snooper.h"
#include "strings.h"

int	docachedump;	/* for secret -C option */
int	dometerdump;	/* for secret -M option */

char*	analyrc;	/* analyzer rc file name from -u option */

/*
 * Option processing error handling: If optpath is non-null, it names the
 * file or variable containing illegal options.  If optpath is non-null and
 * optline is non-zero, optline tells the line number of the bad options.
 * Badopt() prints a formatted error message prefixed appropriately by
 * optpath and optline.
 */
static char	*optpath;
static int	optline;

static void	badopt(char *, ...);

/*
 * Options come from three places: a run-command file read at startup, an
 * environment variable, and the command line.
 *
 * NETSNOOPOPTS are remove for netvis 2.0
 */
static char	rcname[] = ".netsnooprc";
/*static char	envname[] = "NETSNOOPOPTS";*/
static char	white[] = " \t\n";

/*
 * Netsnoop rc-file keyword names and interpreters.
 */
static void	scandefstr(char *);
static void	scanoptstr(char *, struct options *);
/* static int	scanoptvec(int, char **, struct options *); */
static void	scansetstr(char *);

static struct keyword {
	char	*name;
	void	(*scan)();
} keywords[] = {
	{ "define",	scandefstr },
	{ "option",	scanoptstr },
	{ "set",	scansetstr },
	{ 0, }
};

/*
 * Set options in *op from .netsnooprc, NETSNOOPOPTS, and argc/argv.
 */
int
setoptions(int argc, char **argv, struct options *op)
{
	FILE *rcf;
	static char rcpath[_POSIX_PATH_MAX];/* static for optpath references */
	char *val;

	/* init input rc file pointer */
	analyrc = 0;

	/*
	 * Set default options.
	 */
	op->count = 1;
	op->stop = 0;
	op->errflags = 0;
	op->hexprotos = 0;
	op->interface = 0;
	op->listprotos = 0;
	op->snooplen = -1;
	op->nullprotos = 0;
	op->tracefile = 0;
	op->queuelimit = 60000;
	op->rawsequence = 0;
	op->dostats = 0;
	op->interval = 0;
	op->viewer = 0;
	op->viewlevel = PV_TERSE;
	op->hexopt = HEXDUMP_NONE;
	op->useyp = 0;

	/*
	 * Look for .netsnooprc in $HOME not in current directory.
	 */
/*
	rcf = fopen(rcname, "r");
	if (rcf)
		optpath = rcname;
	else {
*/
		val = getenv("HOME");
		if (val) {
			(void) nsprintf(rcpath, sizeof rcpath, "%s/%s",
					val, rcname);
			rcf = fopen(rcpath, "r");
			if (rcf)
				optpath = rcpath;
		}
/*	} */

	/*
	 * If we've found an rc-file, interpret its contents.
	 */
	if (rcf) {
		char buf[BUFSIZ];
		struct keyword *kw;

		for (optline = 1; fgets(buf, sizeof buf, rcf); optline++) {
			if (buf[0] == '#' || buf[0] == '\n')
				continue;
			val = strpbrk(buf, white);
			if (val == 0) {
				badopt("missing value after %s", buf);
				continue;
			}
			*val++ = '\0';
			for (kw = keywords; kw->name; kw++) {
				if (!strcmp(kw->name, buf))
					break;
			}
			if (kw->scan == 0) {
				badopt("illegal keyword %s", buf);
				continue;
			}
			(*kw->scan)(val, op);
		}
		optline = 0;
		fclose(rcf);
	}

	/*
	 * Look for a NETSNOOPOPTS environment variable.
	 */
	/*
	val = getenv(envname);
	if (val) {
		optpath = envname;
		scanoptstr(val, op);
	}
	*/

	/*
	 * Finally, scan command line arguments.
	 */
	optpath = 0;
	return scanoptvec(argc, argv, op);
}

static int	scanprotoname(char *, char **, Protocol **);

/*
 * Scan a string of the form "prot.name expr" and define the named macro
 * in prot's scope to expand to expr.
 */
static void
scandefstr(char *prot)
{
	char *expr, *name;
	Protocol *pr;
	int explen, namlen;
	ExprSource *src;

	prot = strtok(prot, white);
	if (prot == 0) {
		badopt("missing macro definition");
		return;
	}
	expr = strtok(0, "\n");
	if (expr == 0) {
		badopt("missing expression after %s", prot);
		return;
	}
	if (!scanprotoname(prot, &name, &pr))
		return;
	namlen = strlen(name);
	name = strndup(name, namlen);
	explen = strlen(expr);
	expr = strndup(expr, explen);
	src = new(ExprSource);
	src->src_path = optpath;
	src->src_line = optline;
	src->src_buf = expr;
	pr_addmacro(pr, name, namlen, expr, explen, src);
}

static int
scanprotoname(char *prot, char **namep, Protocol **prp)
{
	char *name;
	Protocol *pr;

	name = strchr(prot, '.');
	if (name == 0) {
		badopt("missing protocol before %s", prot);
		return 0;
	}
	*name++ = '\0';
	pr = findprotobyname(prot, name-prot-1);
	if (pr == 0) {
		badopt("unknown protocol %s", prot);
		return 0;
	}
	*namep = name;
	*prp = pr;
	return 1;
}

/*
 * Scan a string of options to put in *op.
 */
#define	MAXOPTS	30

static void
scanoptstr(char *args, struct options *op)
{
	char *argv[MAXOPTS+2];
	int argc;
	char *arg;

	argv[0] = exc_progname;
	argc = 1;
	while (arg = strtok(args, white)) {
		if (argc > MAXOPTS) {
			badopt("too many options");
			break;
		}
		argv[argc++] = arg;
		args = 0;
	}
	argv[argc] = 0;
	if (scanoptvec(argc, argv, op) != argc)
		badopt("illegal argument");
}

static void
scansetstr(char *prot)
{
	char *name, *val;
	Protocol *pr;
	ProtOptDesc *pod;
	static char sep[] = ", \t\n";

	for (prot = strtok(prot, sep); prot; prot = strtok(0, sep)) {
		if (!scanprotoname(prot, &name, &pr))
			continue;
		val = strchr(name, '=');
		if (val)
			*val++ = '\0';
		else if (!strncmp(name, "no", 2))
			name += 2, val = "0";
		else
			val = "1";
		pod = pr_findoptdesc(pr, name);
		if (pod == 0) {
			badopt("unknown %s option %s", prot, name);
			continue;
		}
		if (!pr_setopt(pr, pod->pod_id, val))
			badopt("illegal %s option %s=%s", prot, name, val);
	}
}

static void	savestr(char *, char **);
static int	scanerrflags(char *list);
static void	scanintopt(char, char *, int *);
static void	usage(void);

/*
 * Scan a vector of argument words looking for options to put in *op.
 * Return the index of the first non-option word in argv.
 */
int
scanoptvec(int argc, char **argv, struct options *op)
{
	int opt;
	static char optcodes[] = "CL:MV:b:c:de:h:u:i:l:n:o:p:q:rst:vxy";

	opterr = 0;
	optind = 1;
	while ((opt = getopt(argc, argv, optcodes)) != EOF) {
		switch (opt) {
		  case 'C':
			docachedump = 1;
			break;
		  case 'L':
			savestr(optarg, &op->listprotos);
			break;
		  case 'M':
			dometerdump = 1;
			break;
		  case 'V':
			savestr(optarg, &op->viewer);
			break;
		  case 'c':
			op->stop = 1;
			/* FALL THROUGH */
		  case 'b':
			scanintopt(opt, optarg, &op->count);
			break;
		  case 'd':
			op->hexopt = HEXDUMP_PACKET;
			break;
		  case 'e':
			op->errflags = scanerrflags(optarg);
			if (op->errflags < 0)
			    optind = op->errflags;
			break;
		  case 'h':
			savestr(optarg, &op->hexprotos);
			break;
		  case 'i':
			savestr(optarg, &op->interface);
			break;
		  case 'l':
			scanintopt(opt, optarg, &op->snooplen);
			break;
		  case 'n':
			savestr(optarg, &op->nullprotos);
			break;
		  case 'o':
			savestr(optarg, &op->tracefile);
			break;
		  case 'p':
			scansetstr(optarg);
			break;
		  case 'q':
			scanintopt(opt, optarg, &op->queuelimit);
			break;
		  case 'r':
			op->rawsequence = 1;
			break;
		  case 's':
			op->dostats = 1;
			break;
		  case 't':
			scanintopt(opt, optarg, &op->interval);
			break;
		  case 'u':
			analyrc = optarg;
			break;
		  case 'v':
			op->viewlevel++;
			break;
		  case 'x':
			op->hexopt = HEXDUMP_EXTRA;
			break;
		  case 'y':
			op->useyp = 1;
			break;
		  default:
			if (strcmp(exc_progname, "netsnoop") == 0)
			{
			    if (strchr(optcodes, optopt))
				badopt("-%c requires an argument", optopt);
			    else
				badopt("illegal option -%c", optopt);
			    usage();
			}
			else
			    return -1;
		}
	}
	return optind;
}

/*
 * Scan a non-negative integer argument to an option.
 */
static void
scanintopt(char opt, char *arg, int *ip)
{
	char *next;
	long val;

	val = strtol(arg, &next, 0);
	if (val == 0 && next == arg || val < 0) {
		badopt("-%c requires a non-negative integer argument", opt);
		exit(-1);
	}
	*ip = val;
}

static void
savestr(char *unsafe, char **sp)
{
	if (*sp)
		delete(*sp);
	*sp = strdup(unsafe);
}

/*
 * Snoop error flag option scanning.
 */
static struct errflag {
	char		*name;
	unsigned short	value;
} errflags[] = {
	{ "any",	SN_ERROR },
	{ "frame",	SNERR_FRAME },
	{ "checksum",	SNERR_CHECKSUM },
	{ "toobig",	SNERR_TOOBIG },
	{ "toosmall",	SNERR_TOOSMALL },
	{ "nobufs",	SNERR_NOBUFS },
	{ "overflow",	SNERR_OVERFLOW },
	{ "memory",	SNERR_MEMORY },
	{ 0, }
};

static int
scanerrflags(char *list)
{
	int flags;
	char *name;
	struct errflag *ef;

	flags = 0;
	while (name = strtok(list, ",")) {
		for (ef = errflags; ; ef++) {
			if (ef->name == 0) {
			    if (strcmp(exc_progname, "netsnoop") == 0)
			    {
				fprintf(stderr, "%s: illegal error flag %s\n",
					exc_progname, name);
				usage();
			    }
			    else
				return -1;
			}
			if (!strcasecmp(ef->name, name)) {
				flags |= ef->value;
				break;
			}
		}
		list = 0;
	}
	return flags;
}

/*
 * Usage, unfortunately coupled to the getopt call in scanoptvec().
 */
static void
usage()
{
	struct errflag *ef;

	ef = &errflags[0];
	fprintf(stderr,
	  "usage: %s [-L protocols] [-b buffer] [-c count] [-d]\n\t[-e %s",
		exc_progname, ef->name);
	for (ef++; ef->name; ef++)
		fprintf(stderr, ",%s", ef->name);
	fprintf(stderr, "]\n");
	fprintf(stderr,
	  "\t[-h hexprotos] [-i interface] [-l length] [-n nullprotos]\n");
	fprintf(stderr,
	  "\t[-o tracefile] [-p protopts] [-q limit] [-t interval] [-rsvxy]\n");
	fprintf(stderr, "\t[filter ...]\n");
	exit(-1);
}

/*
 * Filter expression syntax error reporter.
 */
void
badexpr(int error, ExprError *err)
{
	char *mp, *bp, *tp;
	int col, tab;

	mp = err->err_message;
	if (error) {
		exc_perror(error, mp);
		exit(error);
	}
	optpath = err->err_path;
	optline = err->err_line;
	bp = err->err_buf;
	tp = err->err_token;
	if (bp == 0 || tp == 0) {
		badopt(tp == 0 || *tp == '\0' ? "%s" : "%s: %.12s...", mp, tp);
		exit(-1);
	}
	badopt(mp);
	fprintf(stderr, "%s\n", bp);
	for (col = 0; bp < tp; bp++, col++) {
		if (*bp == '\t') {
			for (tab = 8 - col % 8; --tab > 0; col++)
				fputc('.', stderr);
		}
		fputc('.', stderr);
	}
	fprintf(stderr, "^\n");
	exit(-1);
}

/*
 * Universal option error reporter.
 */
static void
badopt(char *format, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", exc_progname);
	if (optpath) {
		if (optline)
			fprintf(stderr, "%s, line %d: ", optpath, optline);
		else
			fprintf(stderr, "%s: ", optpath);
	}
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, ".\n");
}


/*
char* getrcname(int argc, char **argv)
{
	int opt;
	static char optcodes[] = "CL:MV:b:c:de:h:u:i:l:n:o:p:q:rst:vxy";

	analyrc = 0;
	opterr = 0;
	optind = 1;
	while ((opt = getopt(argc, argv, optcodes)) != EOF) {
		if (opt == 'u')
		{
			analyrc = optarg;
			break;
		}
	}
	return analyrc;
}
*/
