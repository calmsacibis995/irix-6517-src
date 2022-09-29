/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Netsnoop, a network monitor.
 */
#ifdef LICENSE
#include <license.h>
#endif

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <values.h>
#include "cache.h"
#include "debug.h"
#include "exception.h"
#include "expr.h"
#include "macros.h"
#include "options.h"
#include "packetbuf.h"
#include "packetview.h"
#include "protocol.h"
#include "snooper.h"
#include "strings.h"

#ifdef __sgi
extern int	_yp_disabled;	/* libsun NIS off switch */
#endif
extern Protocol	*_protocols[];	/* protocol configuration table */
extern int	docachedump;	/* for secret -C option */
extern int	dometerdump;	/* for secret -M option */

static Snooper	*snooper;	/* use by finish, the fatal signal handler */
static PacketView *viewer;
static Snooper	*tracer;

static void	list(char *);
static void	finish();	/* interrupt handler */
static void	timeout() { }	/* alarm handler */
static int	tracepacket(PacketBuf *, Packet *, void *);

#ifdef LICENSE
static char* AnnotString = "noNetsnoop";

#define PROD_ID 6
#define VERSION "2.0"
#define CHK_PER 20
#define NUM_LICENSES 1
#endif

main(int argc, char **argv)
{
	struct options opts;
	int argp, count, stop, stopped;
	ExprSource *filter;
	ExprError error;
	char buf[_POSIX_ARG_MAX], *ifname;
	Snooper *sn;
	PacketView *pv;
	PacketBuf pb;
	MatchArgs ma;
	DecodeArgs da;
	struct timeval tv;
	struct snoopstats ss;

#ifdef LICENSE

	/*
	 * Request a license; if none is available, we won't run
	 */
	long daysleft;
	char annot[80];
	char* msg;
	char* annotmsg;

	if (!get_license( "Analyzer", PROD_ID, VERSION, CHK_PER,
			NUM_LICENSES, &daysleft, &msg, annot, &annotmsg))
	{
	    printf ("\n%s\n\n", msg);
	    exit (1);
	}
	else
	{
	    char* cpt = strstr(annot, AnnotString);
	    if (cpt)
		if (strncmp (cpt, AnnotString, strlen(AnnotString)) == 0)
		{
		    printf ("\n%s\n\n", annotmsg);
		    exit (1);
		}
	}

#ifdef TRYBUY
	printf ("\n%s\n\n", msg);
	printf ("Press the Enter key to continue:");
	getchar();
#endif

	/*
	 * return_license returns our license to the server
	 */
	atexit( return_license );
#endif

	/*
	 * Setup exception handling.
	 */
	exc_progname = argv[0];
	exc_autofail = 1;

	/*
	 * Initialize protocols before processing options.
	 * XXX We must disable NIS temporarily, as protocol init routines
	 * XXX tend to call getservbyname, which is horrendously slow thanks
	 * XXX to Sun's quadratic-growth lookup botch.
	 */
#ifdef __sgi
	_yp_disabled = 1;
#endif
	initprotocols();

	/*
	 * Process options, including the use-NIS flag.
	 */
	argp = setoptions(argc, argv, &opts);
	if (opts.listprotos) {
		list(opts.listprotos);
		exit(0);
	}

	/*
	 * Treat arguments as a filter expression.
	 */
	if (argp == argc)
		filter = 0;
	else {
		buf[0] = '\0';
		for (;;) {
			strcat(buf, argv[argp]);
			if (++argp == argc)
				break;
			strcat(buf, " ");
		}
		newExprSource(filter, buf);
	}

#if 0
	/*
	 * Set NIS switch based on its option.
	 */
#ifdef __sgi
	if (opts.useyp)
		_yp_disabled = 0;
	else
		sethostresorder("local");
#endif
#endif

	/*
	 * Create a snooper for the given interface.
	 */
	stop = opts.stop;
	switch (getsnoopertype(opts.interface, &ifname)) {
	  case ST_TRACE:
		sn = tracesnooper(opts.interface, "r", 0);
		break;

	  case ST_LOCAL:
#ifdef sun
		sn = sunsnooper(ifname, opts.queuelimit);
#else
		sn = localsnooper(ifname, 0, opts.queuelimit);
#endif
		break;

	  case ST_REMOTE:
		sn = remotesnooper(opts.interface, ifname, 0, opts.count,
				   stop ? opts.count : 0, opts.interval);
		break;

	  default:
		exc_raise(errno, opts.interface);
	}
	if (opts.snooplen != -1)
		sn_setsnooplen(sn, opts.snooplen);
	snooper = sn;

	/*
	 * Create the specified packetview instance.
	 */
	if (opts.viewer == 0 || !strcmp(opts.viewer, "stdio"))
		pv = stdio_packetview(stdout, "stdout", opts.viewlevel);
	else if (!strcmp(opts.viewer, "null"))
		pv = null_packetview();
	else
		exc_raise(0, "unknown packet viewer %s", opts.viewer);
	viewer = pv;

	/*
	 * Validate and hexify or nullify decoding of certain protocols.
	 */
	if (opts.hexprotos)
		pv_hexify(pv, opts.hexprotos);
	if (opts.nullprotos)
		pv_nullify(pv, opts.nullprotos);

	/*
	 * Snoop for errors if opts.errflags and compile the filter.
	 */
	opts.interface = sn->sn_name;
	if (opts.errflags)
		sn_seterrflags(sn, opts.errflags);
	if (filter || !opts.errflags) {
		exc_autofail = 0;
		if (!sn_compile(sn, filter, &error))
			badexpr(sn->sn_error, &error);
		else if (sn->sn_expr) {
			filter = sn->sn_expr->ex_src;
			if (filter && filter->src_buf != buf)
				fprintf(stderr, "[%s]\n", filter->src_buf);
		}
		exc_autofail = 1;
	}

	/*
	 * Create a tracesnooper if writing undecoded packets to a file.
	 */
	if (opts.tracefile)
		tracer = tracesnooper(opts.tracefile, "w", sn->sn_rawproto);

	/*
	 * Initialize a packet buffer and its scanners' arguments.
	 */
	count = opts.count;
	pb_init(&pb, count, sn->sn_packetsize);
	MA_INIT(&ma, opts.rawsequence ? SEQ_RAW : SEQ_MATCH, sn);
	if (tracer == 0)
		DA_INIT(&da, sn, opts.hexopt, pv);

	/*
	 * Install finish as the handler for common fatal signals.
	 */
	sigset(SIGHUP, finish);
	sigset(SIGINT, finish);
	sigset(SIGTERM, finish);

	/*
	 * Set tv into the future by the snoop interval so we can compare it
	 * to the timestamps in arriving packets.  Set an alarm in case no
	 * packet arrives.
	 */
	if (opts.interval) {
		gettimeofday(&tv, (struct timezone *) 0);
		tv.tv_sec += opts.interval;
		sigset(SIGALRM, timeout);
		alarm(opts.interval);
	}

	/*
	 * Capture until count matching packets have been caught, or forever
	 * if stop is zero, or until the timeout expires.
	 */
	sn_startcapture(sn);
	stopped = 0;
	while (!stop || count > 0) {
		Packet *p;
		int cc;

		p = pb_get(&pb);
		cc = sn_read(sn, &p->p_sp, sn->sn_packetsize);
		if (cc <= 0) {
			(void) pb_put(&pb, p);
			if (cc < 0) {
				if (sn->sn_error == EINTR) {
					stopped = 1;	/* timeout expired */
					sn_stopcapture(snooper);
					(void) sn_ioctl(snooper, FIONBIO,
							&stopped);
				} else if (sn->sn_error != EWOULDBLOCK) {
					exc_raise(sn->sn_error, sn->sn_name);
				}
			}
			break;
		}
		if (opts.interval && timercmp(&p->p_sp.sp_timestamp, &tv, >)) {
			(void) pb_put(&pb, p);
			break;
		}
		p->p_len = cc;
		if (stop) {
			if (pb_matchpacket(&pb, p, &ma))
				--count;
			continue;
		}
		if (PB_FULL(&pb)) {
			pb_scan(&pb, pb_matchpacket, &ma);
			if (tracer == 0)
				pb_scan(&pb, pb_decodepacket, &da);
			else
				pb_scan(&pb, tracepacket, 0);
			pb_flush(&pb);
		}
	}

	/*
	 * Get statistics ASAP.  We must cancel any outstanding alarm first.
	 */
	if (opts.interval)
		alarm(0);
	if (opts.dostats) {
		ss.ss_seq = ss.ss_ifdrops = ss.ss_sbdrops = 0;
		sn_getstats(sn, &ss);
	}

	/*
	 * Stop capture and shut down socket to free unwanted input buffers.
	 */
	if (!stopped)
		sn_stopcapture(sn);
	(void) sn_shutdown(sn, SNSHUTDOWN_READ);

	/*
	 * Decode buffered packets, optionally showing statistics.
	 */
	if (stop || opts.interval) {
		if (tracer == 0)
			pb_scan(&pb, pb_decodepacket, &da);
		else
			pb_scan(&pb, tracepacket, 0);
	}

	if (opts.dostats) {
		static char fmt[] = "%lu packet%s %s\n";
#define	pv_showstat(pv, num, msg) \
	pv_printf(pv, fmt, num, (num != 1) ? "s" : "", msg)

		pv_showstat(pv, ss.ss_seq, "received by snoop protocol");
		pv_showstat(pv, ss.ss_ifdrops, "dropped at network interface");
		pv_showstat(pv, ss.ss_sbdrops, "dropped at socket buffer");
#undef	pv_showstat
	}

	/*
	 * Clean up and exit.
	 */
	pb_finish(&pb);
	if (docachedump)
		dumpcaches(DUMP_SUMMARY|DUMP_METERS|DUMP_ENTRIES);
#ifdef METERING
	if (dometerdump)
		dumpmeters();
#endif
	finish();
}

#ifdef METERING
dumpmeters()
{
	Protocol **prp, *pr;

	for (prp = _protocols; (pr = *prp) != 0; prp++) {
		unsigned long searches, probes, hits, misses, total;

		printf("%s length %u numfree %u\n", pr->pr_name,
			pr->pr_scope.sc_length, pr->pr_scope.sc_numfree);
		searches = pr->pr_scope.sc_meter.sm_searches;
		probes = pr->pr_scope.sc_meter.sm_probes;
		hits = pr->pr_scope.sc_meter.sm_hits;
		misses = pr->pr_scope.sc_meter.sm_misses;
		printf("\tsearches %lu probes %lu hits %lu misses %lu\n",
			searches, probes, hits, misses);
		printf("\tadds %lu grows %lu deletes %lu\n",
			pr->pr_scope.sc_meter.sm_adds,
			pr->pr_scope.sc_meter.sm_grows,
			pr->pr_scope.sc_meter.sm_deletes);
		if (searches != 0) {
			printf("\tchain-length %g\n",
				(double)(searches + probes) / searches);
		}
		total = hits + misses;
		if (total != 0)
			printf("\thit-ratio %g\n", (double)hits / total);
	}
}
#endif

static void	listproto(Protocol *, char *);

/*
 * Scan protlist for names of protocols to list, respecting "all" by listing
 * all protocol symbol tables.
 */
static void
list(char *protlist)
{
	char *prot, *name;
	int protlen;
	Protocol **prp, *pr;

	while ((prot = strtok(protlist, ",")) != 0) {
		protlist = 0;
		if (!strcmp(prot, "all")) {
			for (prp = _protocols; *prp; prp++)
				listproto(*prp, 0);
			continue;
		}
		name = strchr(prot, '.');
		if (name == 0)
			protlen = strlen(prot);
		else {
			protlen = name - prot;
			*name++ = '\0';
		}
		pr = findprotobyname(prot, protlen);
		if (pr)
			listproto(pr, name);
	}
}

/*
 * The following code knows all about Scope's innards and the order of the
 * symtype enumeration's literal values.  Note also that various strings and
 * formats ensure that columns are a multiple of ten spaces in width.
 */
#define	COLWIDTH	10

static struct typeheader {
	char	*name;
	char	*info;
} typeheaders[] = {
	{ 0,		0 },
	{ "Address",	"contents" },
	{ "Constant",	"value     (hexadecimal)" },
	{ "Macro",	"definition" },
	{ "Function",	"description" },
	{ "Protocol",	"typecode  (decimal)" },
	{ "Field",	"type                level     title" },
};

static char	headerformat[] = "  o %-19.19s %s\n";

static int	comparesyms(const void *, const void *);
static void	printleader(char *, int);
static void	listfield(ProtoField *, char *, int, int);

static void
listproto(Protocol *pr, char *name)
{
	int cnt, len;
	Symbol **vec;
	Symbol *sym;
	enum symtype last;
	ProtOptDesc *pod;

	cnt = pr->pr_scope.sc_length;
	vec = vnew(cnt, Symbol *);
	len = 0;
	for (sym = pr->pr_scope.sc_table; --cnt >= 0; sym++) {
		if (sym->sym_type != SYM_FREE)
			vec[len++] = sym;
	}
	qsort(vec, len, sizeof *vec, comparesyms);

	printf("%s (%s):\n", pr->pr_name, pr->pr_title);
	last = SYM_FREE;
	for (cnt = 0; cnt < len; cnt++) {
		sym = vec[cnt];
		if (name && strcmp(sym->sym_name, name))
			continue;
		if (last != sym->sym_type) {
			struct typeheader *th;

			th = &typeheaders[(int) sym->sym_type];
			putchar('\n');
			printf(headerformat, th->name, th->info);
		}
		last = sym->sym_type;

		switch (sym->sym_type) {
		  case SYM_ADDRESS: {
			int bytes;
			unsigned char *bp;

			printleader(sym->sym_name, sym->sym_namlen);
			bytes = sizeof sym->sym_addr.a_vec;
			for (bp = sym->sym_addr.a_vec; bytes > 0; bp++) {
				if (*bp != 0)
					break;
				--bytes;
			}
			if (bytes == 0)
				putchar('0');
			else {
				for (;;) {
					printf("%x", *bp);
					if (--bytes == 0)
						break;
					putchar(':');
					bp++;
				}
			}
			break;
		  }

		  case SYM_NUMBER: {
			long val = sym->sym_val;

			printleader(sym->sym_name, sym->sym_namlen);
			printf("%-9ld %lx", val, val);
			break;
		  }

		  case SYM_MACRO: {
			struct string *sp = &sym->sym_def.md_string;

			printleader(sym->sym_name, sym->sym_namlen);
			printf("%.*s", sp->s_len, sp->s_ptr);
			break;
		  }

		  case SYM_FUNCTION: {
			char *args, *desc;
			char buf[40];

			desc = sym->sym_func.fd_desc;
			args = strrchr(desc, '@');
			if (args == 0)
				printleader(sym->sym_name, sym->sym_namlen);
			else {
				*args++ = '\0';
				printleader(buf,
					    nsprintf(buf, sizeof buf, "%s(%s)",
						     sym->sym_name, args));
			}
			fputs(desc, stdout);
			break;
		  }

		  case SYM_PROTOCOL: {
			long prototype = sym->sym_prototype;

			printleader(sym->sym_name, sym->sym_namlen);
			printf("%#06lx    %ld", prototype, prototype);
			break;
		  }

		  case SYM_FIELD:
			listfield(sym->sym_field, sym->sym_name,
				  sym->sym_namlen, 0);
			break;
		}
		putchar('\n');
	}
	delete(vec);

	pod = pr->pr_optdesc;
	if (pod) {
		len = 0;
		for (cnt = pr->pr_numoptions; --cnt >= 0; pod++) {
			if (name && strcmp(pod->pod_name, name))
				continue;
			if (len == 0) {
				putchar('\n');
				printf(headerformat, "Option", "description");
				len = 1;
			}
			printf("    %-18s  %s\n", pod->pod_name, pod->pod_desc);
		}
	}
	putchar('\n');
}

/*
 * Compare symbols of different type by their reversed type order, so that
 * fields come first, protocols second, etc.  Compare fields by descriptor
 * address, so that fields list in their tabulated order.  Compare all other
 * symbols lexicographically.
 */
static int
comparesyms(const void *p1, const void *p2)
{
	Symbol *sym1, *sym2;

	sym1 = *(Symbol **)p1, sym2 = *(Symbol **)p2;
	if (sym1->sym_type != sym2->sym_type)
		return (int) sym2->sym_type - (int) sym1->sym_type;
	if (sym1->sym_type == SYM_FIELD)
		return sym1->sym_field - sym2->sym_field;
	return strcmp(sym1->sym_name, sym2->sym_name);
}

static void
printleader(char *name, int namlen)
{
	if (namlen > 19)
		printf("    %s\n%24s", name, " ");
	else
		printf("    %-19s ", name);
}

static char	*typesignature(ProtoField *);

static void
listfield(ProtoField *pf, char *name, int namlen, int depth)
{
	int m, n, cc;
	char buf[40];
	ProtoField *spf, *mpf;

	printleader(name, namlen);
	printf("%-19.19s -", typesignature(pf));
	for (m = n = MIN(pf->pf_level, COLWIDTH - 2); n > 0; --n)
		putchar('v');
	for (n = COLWIDTH - 1 - m; n > 0; --n)
		putchar(' ');
	fputs(pf->pf_title, stdout);

	for (cc = nsprintf(buf, sizeof buf, "%s", name);
	     pf->pf_type == EXOP_ARRAY; depth++) {
		cc += nsprintf(&buf[cc], sizeof buf - cc, "[%c]", 'i' + depth);
		pf = pf_element(pf);
	}
	if (pf->pf_type == EXOP_STRUCT
	    && (spf = pf_struct(pf)->pst_parent) != 0) {
		pf_struct(pf)->pst_parent = 0;	/* avoid recursion */
		n = pf_struct(pf)->pst_numfields;
		for (mpf = pf_struct(pf)->pst_fields; --n >= 0; mpf++) {
			putchar('\n');
			listfield(mpf, buf,
				  cc + nsprintf(&buf[cc], sizeof buf - cc,
						".%s", mpf->pf_name),
				  depth);
		}
		pf_struct(pf)->pst_parent = spf;
	}
}

static char *
typesignature(ProtoField *pf)
{
	int cc;
	char xb[40];
	static char buf[40];

	switch (pf->pf_type) {
	  case EXOP_ARRAY:
		cc = 0;
		do {
			if (pf->pf_size == PF_VARIABLE)
				cc += nsprintf(&xb[cc], sizeof xb - cc, "[]");
			else {
				cc += nsprintf(&xb[cc], sizeof xb - cc, "[%d]",
					       pf->pf_size);
			}
			pf = pf_element(pf);
		} while (pf->pf_type == EXOP_ARRAY);
		(void) nsprintf(buf, sizeof buf, "%s%s", typesignature(pf), xb);
		return buf;
	  case EXOP_STRUCT:
		return pf_struct(pf)->pst_name;
	  case EXOP_NUMBER:
		switch (pf->pf_size) {
		  case sizeof(char):
			return (pf_extendhow(pf) == DS_ZERO_EXTEND) ?
				"u_char" : "char";
		  case sizeof(short):
			return (pf_extendhow(pf) == DS_ZERO_EXTEND) ?
				"u_short" : "short";
		  case sizeof(long):
			return (pf_extendhow(pf) == DS_ZERO_EXTEND) ?
				"u_long" : "long";
		  default:
			if (pf->pf_size > 0 || pf->pf_size == PF_VARIABLE)
				break;
			(void) nsprintf(buf, sizeof buf, "%sint:%d",
					(pf_extendhow(pf) == DS_ZERO_EXTEND) ?
					"u_" : "", -pf->pf_size);
			return buf;
		}
		break;
	  case EXOP_ADDRESS:
		if (pf->pf_size != sizeof(Address)) {
			(void) sprintf(buf, "address:%d",
				       pf->pf_size * BITSPERBYTE);
		} else
			strcpy(buf, "address");
		return buf;
	  case EXOP_STRING:
		if (pf->pf_size == PF_VARIABLE)
			return "char[]";
		(void) sprintf(buf, "char[%d]", pf->pf_size);
		return buf;
	}
	return "???";
}

/*
 * This packetbuf scan function writes each buffered packet to tracer.
 */
/* ARGSUSED */
static int
tracepacket(PacketBuf *pb, Packet *p, void *arg)
{
	if (sn_write(tracer, &p->p_sp, p->p_len) != p->p_len) {
		exc_raise(tracer->sn_error, "cannot write to %s",
			  tracer->sn_name);
		return 0;
	}
	return 1;
}

/*
 * Destroy snoopers and viewer, save caches in files to speed up the next
 * netsnoop session, and exit.
 */
static void
finish()
{
	sn_destroy(snooper);
	pv_destroy(viewer);
	if (tracer)
		sn_destroy(tracer);
	savecaches();
	exit(0);
}
