/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Reporter
 *
 *	$Revision: 1.14 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <libc_synonyms.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "protoid.h"
#include "protocol.h"
#include "exception.h"
#include "index.h"
#include "heap.h"
#include "license.h"
#include "account.h"

char *ether_ntoa(struct etheraddr *);

#ifdef sun
extern int daylight;
extern char *tzname[2];
#endif

static void total(struct reportvalue **, unsigned int, struct collectvalue **);
static void print_total(struct collectvalue *);
static void source_rank(struct reportvalue **, unsigned int,
			struct collectvalue *, unsigned int);
static void source_summary(struct reportvalue **, unsigned int,
			   struct collectvalue *, unsigned int);
static void dest_rank(struct reportvalue **, unsigned int,
		      struct collectvalue *, unsigned int);
static void dest_summary(struct reportvalue **, unsigned int,
			 struct collectvalue *, unsigned int);
static void dump(struct reportvalue **, unsigned int);
static char *sprinthost(struct addr *);
static void printcollectvaluechain(struct collectvalue *,
				struct collectvalue *, struct collectvalue *);
static void merge_value(struct collectvalue **, struct collectvalue *);
static int samehost(struct addr *, struct addr *);
static void space(int);

static unsigned int verbose;
static int protolist[PRID_MAX];

#define DEFAULT_RANK	5
#define DEFAULT_SUMM	25

#define PR_NAMLEN	9

main(int argc, char** argv)
{
	struct reportvalue **srcorder, **dstorder;
	struct collectvalue *totalv;
	struct in_addr addr, mask, net;
	struct addr a;
	struct netent *ne;
	char *filename, *timestring;
	unsigned int shift, nel;
	unsigned int nameservice, dosumm, dorank, dototal, dodump;
	unsigned int nrank, nsumm;
	time_t start, end;
	int n;
	static char usage[] =
	  "usage: netaccount [-p proto] [-tvy] [-r nrank] [-s nsumm] file\n";
	extern int _yp_disabled;
	extern char *optarg, *tzname[];
	extern int optind;
        char *message;

	exc_progname = "netaccount";
	exc_autofail = 1;

	if (getLicense("NetAccount", &message) == 0)
		exc_raise(errno, message);
	if (message != 0) {
		fprintf(stderr, "%s: %s.\n", exc_progname, message);
		delete(message);
	}

#ifdef __sgi
	_yp_disabled = 1;
#endif
	initprotocols();
#ifdef __sgi
	_yp_disabled = 0;
#endif

	filename = 0;
	verbose = 0;
	nameservice = 0;
	dosumm = dorank = dototal = dodump = 0;
	bzero(protolist, sizeof protolist);
	while ((n = getopt(argc, argv, "dp:r:s:tvy")) != -1) {
		char *c;
		switch (n) {
		    case 'd':
			dodump = 1;
			break;
		    case 'p':
		    {
			Protocol *pr;
			int i;

			pr = findprotobyname(optarg, strlen(optarg));
			if (pr == 0)
				exc_raise(0, "unknown protocol");
			for (i = 0; i < PRID_MAX; i++) {
				if (protolist[i] == 0) {
					protolist[i] = pr->pr_id;
					break;
				}
			}
			break;
		    }
		    case 'r':
		    {
			char *s;
			long i = strtol(optarg, &s, 0);
			if (s == optarg || i < 0)
				exc_raise(0, "invalid rank count");
			nrank = i;
			dorank = 1;
			break;
		    }
		    case 's':
		    {
			char *s;
			long i = strtol(optarg, &s, 0);
			if (s == optarg || i < 0)
				exc_raise(EINVAL, "invalid summarize count");
			nsumm = i;
			dosumm = 1;
			break;
		    }
		    case 't':
			dototal = 1;
			break;
		    case 'v':
			verbose++;
			break;
		    case 'y':
			nameservice = 1;
			break;
		    default:
			fputs(usage, stderr);
			exit(EINVAL);
		}
	}

	if (optind < argc)
		filename = argv[optind];
	else {
		fputs(usage, stderr);
		exit(EINVAL);
	}

	if (dosumm + dorank + dototal == 0) {
		dosumm = dorank = dototal = 1;
		nrank = DEFAULT_RANK;
		nsumm = DEFAULT_SUMM;
	}

#ifdef __sgi
	if (!nameservice) {
		(void) sethostresorder("local");
		_yp_disabled = 1;
	}
#endif

	if (dodump != 0) {
		srcorder = 0;
		if (read_report(filename, &addr.s_addr, &mask.s_addr,
				&start, &end, &nel, &srcorder, 0) == 0)
			exc_raise(errno, "error reading \"%s\"", filename);
		dump(srcorder, nel);
		exit(0);
	}

	srcorder = dstorder = 0;
	if (read_report(filename, &addr.s_addr, &mask.s_addr,
			&start, &end, &nel, &srcorder, &dstorder) == 0)
		exc_raise(errno, "error reading \"%s\"", filename);

	puts("\n\t\t==========   Traffic Summary   ==========\n");
	net.s_addr = addr.s_addr & mask.s_addr;
	fputs("Network:\t", stdout);
	for (shift = 0; ((1 << shift) & mask.s_addr) == 0; shift++)
		;
	ne = getnetbyaddr(net.s_addr >> shift, AF_INET);
	if (ne != 0)
		fputs(ne->n_name, stdout);
	printf("(%s)\n", inet_ntoa(net));
	bzero(&a, sizeof a);
	a.a_naddr = addr.s_addr;
	printf("Collector:\t%s\n", sprinthost(&a));
	timestring = ctime(&start);
	timestring[24] = '\0';
#ifdef sun
	printf("From:\t\t%s %s\n", timestring, tzname[daylight]);
#else
	printf("From:\t\t%s %s\n", timestring, _tzname[_daylight]);
#endif
	timestring = ctime(&end);
	timestring[24] = '\0';
#ifdef sun
	printf("To:\t\t%s %s\n\n", timestring, tzname[daylight]);
#else
	printf("To:\t\t%s %s\n\n", timestring, _tzname[_daylight]);
#endif

	totalv = 0;
	total(srcorder, nel, &totalv);
	if (dototal)
		print_total(totalv);

	if (dorank != 0)
		source_rank(srcorder, nel, totalv, nrank);
	if (dosumm) {
		source_summary(srcorder, nel, totalv, nsumm);
		putchar('\n');
	}

	if (dorank != 0)
		dest_rank(dstorder, nel, totalv, nrank);
	if (dosumm)
		dest_summary(dstorder, nel, totalv, nsumm);

	exit(0);
}

static void
total(struct reportvalue **rp, unsigned int nel, struct collectvalue **cp)
{
	unsigned int i;

	for (i = 0; i < nel; i++)
		merge_value(cp, rp[i]->rv_value);
}

static void
print_total(struct collectvalue *totalv)
{
	struct collectvalue *cv;
	char *desc;

	puts("\n\t\t----------        Total        ----------\n");
	space(4);
	fputs("Protocol", stdout);
	space(7);
	fputs("Packets", stdout);
	space(28);
	puts("Bytes\n");
	printcollectvaluechain(totalv, 0, 0);
	putchar('\n');
	if (protolist[0] != 0)
		return;

	/* Print out unsupported ethernet types */
	for (cv = totalv; cv != 0; cv = cv->cv_next) {
		if (findprotobyid(cv->cv_protoid) == 0) {
			desc = ether_typedesc(cv->cv_protoid);
			if (desc != 0)
				printf("    %#.4x = \"%s\"\n", cv->cv_protoid,
							     desc);
		}
	}
}

static void
source_rank(struct reportvalue **rp, unsigned int nel,
	    struct collectvalue *tv, unsigned int max)
{
	struct collectvalue *c, *cv;
	struct collectkey *ck;
	struct protocol *pr;
	struct ranking {
		struct reportvalue *rv;
		struct counts cnt;
	} *topp, *topb;
	float p, b;
	unsigned int r, e, n, i, equal, m;

	topp = vnew(max, struct ranking);
	topb = vnew(max, struct ranking);
	m = max - 1;

	puts("\n\t\t----------   Source Ranking    ----------");
	printf("\t\t\t\t Top %d\n\n", max);

	for (c = tv; c != 0; c = c->cv_next) {
		if (protolist[0] != 0) {
			for (i = 0; i < PRID_MAX; i++) {
				if (protolist[i] == c->cv_protoid)
					break;
			}
			if (i == PRID_MAX)
				continue;
		}

		bzero(topp, max * sizeof *topp);
		bzero(topb, max * sizeof *topb);

		for (r = 0; r < nel; r = n) {
			for (p = 0.0, b = 0.0, e = r; ; e = n) {
				ck = &rp[e]->rv_key;
				cv = rp[e]->rv_value;

				for ( ; cv != 0; cv = cv->cv_next) {
					if (cv->cv_protoid < c->cv_protoid)
						continue;
					if (cv->cv_protoid == c->cv_protoid) {
						p += cv->cv_count.c_packets;
						b += cv->cv_count.c_bytes;
					}
					break;
				}

				n = e + 1;
				if (n == nel)
					break;
				if (verbose == 0 && ck->ck_src.a_naddr != 0) {
					if (ck->ck_src.a_naddr !=
					    rp[n]->rv_key.ck_src.a_naddr)
						break;
				} else if (!samehost(&rp[n]->rv_key.ck_src,
								&ck->ck_src))
					break;
			}
			if (p > topp[m].cnt.c_packets ||
				(p == topp[m].cnt.c_packets &&
					b > topp[m].cnt.c_bytes)) {
				for (i = 0; i < m; i++) {
					if (p < topp[i].cnt.c_packets ||
						(p == topp[i].cnt.c_packets &&
						    b < topp[i].cnt.c_bytes))
						continue;
					break;
				}
				if (i < m)
					bcopy(&topp[i], &topp[i+1],
						(m - i) * sizeof topp[i]);
				topp[i].cnt.c_packets = p;
				topp[i].cnt.c_bytes = b;
				topp[i].rv = rp[r];
			}
			if (b > topb[m].cnt.c_bytes ||
				(b == topb[m].cnt.c_bytes &&
					p > topb[m].cnt.c_packets)) {
				for (i = 0; i < m; i++) {
					if (b < topb[i].cnt.c_bytes ||
						(b == topb[i].cnt.c_bytes &&
						    p < topb[i].cnt.c_packets))
						continue;
					break;
				}
				if (i < m)
					bcopy(&topb[i], &topb[i+1],
						(m - i) * sizeof topb[i]);
				topb[i].cnt.c_packets = p;
				topb[i].cnt.c_bytes = b;
				topb[i].rv = rp[r];
			}
		}

		pr = findprotobyid(c->cv_protoid);
		if (pr != 0)
			puts(pr->pr_name);
		else {
			char *desc = ether_typedesc(c->cv_protoid);
			if (desc != 0)
				printf("%#.4x \"%s\"\n", c->cv_protoid, desc);
			else
				printf("%#.4x\n", c->cv_protoid);
		}

		for (equal = 1, i = 0; i < max; i++) {
			if (topp[i].rv == topb[i].rv ||
				samehost(&topp[i].rv->rv_key.ck_src,
					&topb[i].rv->rv_key.ck_src))
				continue;
			equal = 0;
			break;
		}

		if (equal) {
			for (i = 0; i < max; i++) {
				if (topb[i].rv == 0)
					break;
				printf("    %d. %s\n", i+1,
					sprinthost(&topp[i].rv->rv_key.ck_src));
				space(14);
				printf("%12.0f [%6.2f%%]",
					topp[i].cnt.c_packets,
					100.0 * topp[i].cnt.c_packets /
						c->cv_count.c_packets);
				space(11);
				printf("%12.0f [%6.2f%%]\n",
					topb[i].cnt.c_bytes,
					100.0 * topb[i].cnt.c_bytes /
							c->cv_count.c_bytes);
			}
		} else {
			puts("  Ranked by packets:");
			for (i = 0; i < max; i++) {
				if (topp[i].rv == 0)
					break;
				printf("    %d. %s\n", i+1,
					sprinthost(&topp[i].rv->rv_key.ck_src));
				space(14);
				printf("%12.0f [%6.2f%%]",
					topp[i].cnt.c_packets,
					100.0 * topp[i].cnt.c_packets /
						c->cv_count.c_packets);
				space(11);
				printf("%12.0f [%6.2f%%]\n",
					topp[i].cnt.c_bytes,
					100.0 * topp[i].cnt.c_bytes /
							c->cv_count.c_bytes);
			}
			puts("\n  Ranked by bytes:");
			for (i = 0; i < max; i++) {
				if (topb[i].rv == 0)
					break;
				printf("    %d. %s\n", i+1,
					sprinthost(&topb[i].rv->rv_key.ck_src));
				space(14);
				printf("%12.0f [%6.2f%%]",
					topb[i].cnt.c_packets,
					100.0 * topb[i].cnt.c_packets /
						c->cv_count.c_packets);
				space(11);
				printf("%12.0f [%6.2f%%]\n",
					topb[i].cnt.c_bytes,
					100.0 * topb[i].cnt.c_bytes /
							c->cv_count.c_bytes);
			}
		}
		putchar('\n');
	}
}

static void
source_summary(struct reportvalue **rp, unsigned int nel,
	       struct collectvalue *totalv, unsigned int nsumm)
{
	struct collectkey *ck;
	struct collectvalue *cv, *cvt, *cvh, *c, *t;
	unsigned int i, j, e, n;
	float p, ns;

	puts("\n\t\t----------   Source Summary    ----------");
	printf("\t\tMinimum %d%% of protocol packets or bytes\n", nsumm);

	ns = (float) nsumm;
	cvt = cvh = 0;
	for (i = 0; i < nel; i = e) {
		/*
		 * Run through the list totaling all values for this host.
		 */
		for (e = i; ; e = n) {
			ck = &rp[e]->rv_key;
			cv = rp[e]->rv_value;

			merge_value(&cvt, cv);
			n = e + 1;
			if (n == nel)
				break;
			if (verbose == 0 && ck->ck_src.a_naddr != 0) {
				if (ck->ck_src.a_naddr !=
						rp[n]->rv_key.ck_src.a_naddr)
					break;
			} else if (!samehost(&rp[n]->rv_key.ck_src,&ck->ck_src))
				break;
		}
		e = n;
		/*
		 * e is now the element after the last element for this host.
		 */

		/*
		 * See if this host meets minimum
		 */
		for (c = cvt, t = totalv; c != 0; c = c->cv_next) {
			if (protolist[0] != 0) {
				for (j = 0; j < PRID_MAX; j++) {
					if (protolist[j] == c->cv_protoid)
						break;
				}
				if (j == PRID_MAX)
					continue;
			}

			for ( ; t->cv_protoid != c->cv_protoid; t = t->cv_next)
				;
			p = 100.0 * c->cv_count.c_packets /
						t->cv_count.c_packets;
			if (p >= ns)
				break;
			p = 100.0 * c->cv_count.c_bytes / t->cv_count.c_bytes;
			if (p >= ns)
				break;
		}
		if (c == 0)
			goto freeit;

		putchar('\n');
		puts(sprinthost(&rp[i]->rv_key.ck_src));

		switch (verbose) {
		    case 0: /* Totals for endpoint */
		    case 1: /* Totals for separate etherent addresses */
			break;

		    case 2: /* Individual hosts */
		    {
			unsigned int d;

			/*
			 * Run through the values one at a time and see if
			 * the destination was already done.
			 */
			cvh = 0;
			for (n = i; n < e; n++) {
				/* See if this destination has been done */
				for (d = i; d < n; d++) {
					if (samehost(&rp[n]->rv_key.ck_dst,
							&rp[d]->rv_key.ck_dst))
						break;
				}
				if (d != n)
					continue;

				merge_value(&cvh, rp[n]->rv_value);
				for (d = n + 1; d < e; d++) {
					if (samehost(&rp[n]->rv_key.ck_dst,
							&rp[d]->rv_key.ck_dst))
						merge_value(&cvh,
							rp[d]->rv_value);
				}
				printf("          -> %s\n",
					sprinthost(&rp[n]->rv_key.ck_dst));
				printcollectvaluechain(cvh, totalv, cvt);
				for ( ; cvh != 0; cvh = cv) {
					cv = cvh->cv_next;
					delete(cvh);
				}
			}
			break;
		    }

		    default: /* Individual ports */
			for (n = i; n != e; n++) {
				struct servent *se;
				char *proto = 0;

				ck = &rp[n]->rv_key;
				cv = rp[n]->rv_value;

				for (cvh = cv; cvh != 0; cvh = cvh->cv_next) {
					if (cvh->cv_protoid == PRID_TCP) {
						proto = "tcp";
						break;
					}
					else if (cvh->cv_protoid == PRID_UDP) {
						proto = "udp";
						break;
					}
				}

				if (ck->ck_src.a_port == 0)
					fputs("       ", stdout);
				else {
					if (proto != 0)
						se = getservbyport(
							ck->ck_src.a_port,
									proto);
					else
						se = 0;
					if (se != 0)
						printf("%9s", se->s_name);
					else
						printf("%9d",ck->ck_src.a_port);
				}
				fputs(" -> ", stdout);
				fputs(sprinthost(&ck->ck_dst), stdout);

				if (ck->ck_dst.a_port == 0)
					putchar('\n');
				else {
					if (proto != 0)
						se = getservbyport(
							ck->ck_dst.a_port,
									proto);
					if (se != 0)
						printf(".%-9s\n", se->s_name);
					else
						printf(".%d\n",
							ck->ck_dst.a_port);
				}

				printcollectvaluechain(cv, totalv, cvt);
			}
		}

		puts("  total:");
		printcollectvaluechain(cvt, totalv, 0);
freeit:
		for ( ; cvt != 0; cvt = cv) {
			cv = cvt->cv_next;
			delete(cvt);
		}
	}
}

static void
dest_rank(struct reportvalue **rp, unsigned int nel,
	  struct collectvalue *tv, unsigned int max)
{
	struct collectvalue *c, *cv;
	struct collectkey *ck;
	struct protocol *pr;
	struct ranking {
		struct reportvalue *rv;
		struct counts cnt;
	} *topp, *topb;
	float p, b;
	unsigned int r, e, n, i, equal, m;

	topp = vnew(max, struct ranking);
	topb = vnew(max, struct ranking);
	m = max - 1;

	puts("\n\t\t---------- Destination Ranking ----------");
	printf("\t\t\t\t Top %d\n\n", max);

	for (c = tv; c != 0; c = c->cv_next) {
		if (protolist[0] != 0) {
			for (i = 0; i < PRID_MAX; i++) {
				if (protolist[i] == c->cv_protoid)
					break;
			}
			if (i == PRID_MAX)
				continue;
		}

		bzero(topp, max * sizeof *topp);
		bzero(topb, max * sizeof *topb);

		for (r = 0; r < nel; r = n) {
			for (p = 0.0, b = 0.0, e = r; ; e = n) {
				ck = &rp[e]->rv_key;
				cv = rp[e]->rv_value;

				for ( ; cv != 0; cv = cv->cv_next) {
					if (cv->cv_protoid < c->cv_protoid)
						continue;
					if (cv->cv_protoid == c->cv_protoid) {
						p += cv->cv_count.c_packets;
						b += cv->cv_count.c_bytes;
					}
					break;
				}

				n = e + 1;
				if (n == nel)
					break;
				if (verbose == 0 && ck->ck_dst.a_naddr != 0) {
					if (ck->ck_dst.a_naddr !=
					    rp[n]->rv_key.ck_dst.a_naddr)
						break;
				} else if (!samehost(&rp[n]->rv_key.ck_dst,
								&ck->ck_dst))
					break;
			}
			if (p > topp[m].cnt.c_packets ||
				(p == topp[m].cnt.c_packets &&
					b > topp[m].cnt.c_bytes)) {
				for (i = 0; i < m; i++) {
					if (p < topp[i].cnt.c_packets ||
						(p == topp[i].cnt.c_packets &&
						    b < topp[i].cnt.c_bytes))
						continue;
					break;
				}
				if (i < m)
					bcopy(&topp[i], &topp[i+1],
						(m - i) * sizeof topp[i]);
				topp[i].cnt.c_packets = p;
				topp[i].cnt.c_bytes = b;
				topp[i].rv = rp[r];
			}
			if (b > topb[m].cnt.c_bytes ||
				(b == topb[m].cnt.c_bytes &&
					p > topb[m].cnt.c_packets)) {
				for (i = 0; i < m; i++) {
					if (b < topb[i].cnt.c_bytes ||
						(b == topb[i].cnt.c_bytes &&
						    p < topb[i].cnt.c_packets))
						continue;
					break;
				}
				if (i < m)
					bcopy(&topb[i], &topb[i+1],
						(m - i) * sizeof topb[i]);
				topb[i].cnt.c_packets = p;
				topb[i].cnt.c_bytes = b;
				topb[i].rv = rp[r];
			}
		}

		pr = findprotobyid(c->cv_protoid);
		if (pr != 0)
			puts(pr->pr_name);
		else {
			char *desc = ether_typedesc(c->cv_protoid);
			if (desc != 0)
				printf("%#.4x \"%s\"\n", c->cv_protoid, desc);
			else
				printf("%#.4x\n", c->cv_protoid);
		}

		for (equal = 1, i = 0; i < max; i++) {
			if (topp[i].rv == topb[i].rv ||
				samehost(&topp[i].rv->rv_key.ck_dst,
					&topb[i].rv->rv_key.ck_dst))
				continue;
			equal = 0;
			break;
		}

		if (equal) {
			for (i = 0; i < max; i++) {
				if (topb[i].rv == 0)
					break;
				printf("    %d. %s\n", i+1,
					sprinthost(&topp[i].rv->rv_key.ck_dst));
				space(14);
				printf("%12.0f [%6.2f%%]",
					topp[i].cnt.c_packets,
					100.0 * topp[i].cnt.c_packets /
						c->cv_count.c_packets);
				space(11);
				printf("%12.0f [%6.2f%%]\n",
					topb[i].cnt.c_bytes,
					100.0 * topb[i].cnt.c_bytes /
							c->cv_count.c_bytes);
			}
		} else {
			puts("  Ranked by packets:");
			for (i = 0; i < max; i++) {
				if (topp[i].rv == 0)
					break;
				printf("    %d. %s\n", i+1,
					sprinthost(&topp[i].rv->rv_key.ck_dst));
				space(14);
				printf("%12.0f [%6.2f%%]",
					topp[i].cnt.c_packets,
					100.0 * topp[i].cnt.c_packets /
						c->cv_count.c_packets);
				space(11);
				printf("%12.0f [%6.2f%%]\n",
					topp[i].cnt.c_bytes,
					100.0 * topp[i].cnt.c_bytes /
							c->cv_count.c_bytes);
			}
			puts("\n  Ranked by bytes:");
			for (i = 0; i < max; i++) {
				if (topb[i].rv == 0)
					break;
				printf("    %d. %s\n", i+1,
					sprinthost(&topb[i].rv->rv_key.ck_dst));
				space(14);
				printf("%12.0f [%6.2f%%]",
					topb[i].cnt.c_packets,
					100.0 * topb[i].cnt.c_packets /
						c->cv_count.c_packets);
				space(11);
				printf("%12.0f [%6.2f%%]\n",
					topb[i].cnt.c_bytes,
					100.0 * topb[i].cnt.c_bytes /
							c->cv_count.c_bytes);
			}
		}
		putchar('\n');
	}
}

static void
dest_summary(struct reportvalue **rp, unsigned int nel,
	     struct collectvalue *totalv, unsigned int nsumm)
{
	struct collectkey *ck;
	struct collectvalue *cv, *cvt, *cvh, *c, *t;
	unsigned int i, j, e, n;
	float p, ns;

	puts("\n\t\t---------- Destination Summary ----------");
	printf("\t\tMinimum %d%% of protocol packets or bytes\n", nsumm);

	ns = (float) nsumm;
	cvt = cvh = 0;
	for (i = 0; i < nel; i = e) {
		/*
		 * Run through the list totaling all values for this host.
		 */
		for (e = i; ; e = n) {
			ck = &rp[e]->rv_key;
			cv = rp[e]->rv_value;

			merge_value(&cvt, cv);
			n = e + 1;
			if (n == nel)
				break;
			if (verbose == 0 && ck->ck_dst.a_naddr != 0) {
				if (ck->ck_dst.a_naddr !=
						rp[n]->rv_key.ck_dst.a_naddr)
					break;
			} else if (!samehost(&rp[n]->rv_key.ck_dst,&ck->ck_dst))
				break;
		}
		e = n;
		/*
		 * e is now the element after the last element for this host.
		 */

		/*
		 * See if this host meets minimum
		 */
		for (c = cvt, t = totalv; c != 0; c = c->cv_next) {
			if (protolist[0] != 0) {
				for (j = 0; j < PRID_MAX; j++) {
					if (protolist[j] == c->cv_protoid)
						break;
				}
				if (j == PRID_MAX)
					continue;
			}

			for ( ; t->cv_protoid != c->cv_protoid; t = t->cv_next)
				;
			p = 100.0 * c->cv_count.c_packets /
						t->cv_count.c_packets;
			if (p >= ns)
				break;
			p = 100.0 * c->cv_count.c_bytes / t->cv_count.c_bytes;
			if (p >= ns)
				break;
		}
		if (c == 0)
			goto freeit;

		putchar('\n');
		puts(sprinthost(&rp[i]->rv_key.ck_dst));

		switch (verbose) {
		    case 0: /* Totals for endpoint */
		    case 1: /* Totals for separate etherent addresses */
			break;

		    case 2: /* Individual hosts */
		    {
			unsigned int d;

			/*
			 * Run through the values one at a time and see if
			 * the destination was already done.
			 */
			cvh = 0;
			for (n = i; n < e; n++) {
				/* See if this destination has been done */
				for (d = i; d < n; d++) {
					if (samehost(&rp[n]->rv_key.ck_src,
							&rp[d]->rv_key.ck_src))
						break;
				}
				if (d != n)
					continue;

				merge_value(&cvh, rp[n]->rv_value);
				for (d = n + 1; d < e; d++) {
					if (samehost(&rp[n]->rv_key.ck_src,
							&rp[d]->rv_key.ck_src))
						merge_value(&cvh,
							rp[d]->rv_value);
				}
				printf("        <- %s\n",
					sprinthost(&rp[n]->rv_key.ck_src));
				printcollectvaluechain(cvh, totalv, cvt);
				for ( ; cvh != 0; cvh = cv) {
					cv = cvh->cv_next;
					delete(cvh);
				}
			}
			break;
		    }

		    default: /* Individual ports */
			for (n = i; n != e; n++) {
				struct servent *se;
				char *proto = 0;

				ck = &rp[n]->rv_key;
				cv = rp[n]->rv_value;

				for (cvh = cv; cvh != 0; cvh = cvh->cv_next) {
					if (cvh->cv_protoid == PRID_TCP) {
						proto = "tcp";
						break;
					}
					else if (cvh->cv_protoid == PRID_UDP) {
						proto = "udp";
						break;
					}
				}

				if (ck->ck_dst.a_port == 0)
					fputs("       ", stdout);
				else {
					if (proto != 0)
						se = getservbyport(
							ck->ck_dst.a_port,
									proto);
					else
						se = 0;
					if (se != 0)
						printf("%9s", se->s_name);
					else
						printf("%9d",ck->ck_dst.a_port);
				}
				fputs(" <- ", stdout);
				fputs(sprinthost(&ck->ck_src), stdout);

				if (ck->ck_src.a_port == 0)
					putchar('\n');
				else {
					if (proto != 0)
						se = getservbyport(
							ck->ck_src.a_port,
									proto);
					if (se != 0)
						printf(".%-9s\n", se->s_name);
					else
						printf(".%d\n",
							ck->ck_src.a_port);
				}

				printcollectvaluechain(cv, totalv, cvt);
			}
		}

		puts("  total:");
		printcollectvaluechain(cvt, totalv, 0);
freeit:
		for ( ; cvt != 0; cvt = cv) {
			cv = cvt->cv_next;
			delete(cvt);
		}
	}
}

static void
dump(struct reportvalue **rp, unsigned int nel)
{
	unsigned int i;
	struct collectkey *ck;
	struct collectvalue *cv;
	struct in_addr ia;
	Protocol *pr;

	for (i = 0; i < nel; i++) {
		ck = &rp[i]->rv_key;

		/* Print source */
		fputs(ether_ntoa(&ck->ck_src.a_eaddr), stdout);
		putchar('\t');
		if (ck->ck_src.a_naddr == 0)
			printf("0\t0\t");
		else {
			ia.s_addr = ck->ck_src.a_naddr;
			printf("%s\t%d\t", inet_ntoa(ia), ck->ck_src.a_port);
		}

		/* Print destination */
		fputs(ether_ntoa(&ck->ck_dst.a_eaddr), stdout);
		putchar('\t');
		if (ck->ck_dst.a_naddr == 0)
			printf("0\t0\t");
		else {
			ia.s_addr = ck->ck_dst.a_naddr;
			printf("%s\t%d\t", inet_ntoa(ia), ck->ck_dst.a_port);
		}

		/* Print protocol and values */
		for (cv = rp[i]->rv_value; cv != 0; cv = cv->cv_next) {
			if (cv != rp[i]->rv_value)
				putchar('\t');

			pr = findprotobyid(cv->cv_protoid);
			if (pr == 0)
				printf("%#.4x\t", cv->cv_protoid);
			else {
				fputs(pr->pr_name, stdout);
				putchar('\t');
			}
			printf("%12.0f\t%12.0f", cv->cv_count.c_packets,
						 cv->cv_count.c_bytes);
		}
		putchar('\n');
	}
}

static char *
sprinthost(struct addr *a)
{
	static char buf[80], *bufp;
	static struct etheraddr noaddr = { 0, 0, 0, 0, 0, 0 };

	bufp = buf;
	if (a->a_naddr != 0) {
		struct hostent *he;
		struct in_addr ia;

		ia.s_addr = a->a_naddr;
		he = gethostbyaddr(&ia, sizeof ia, AF_INET);
		if (he != 0) {
			strcpy(bufp, he->h_name);
			bufp += strlen(bufp);
		}
		*bufp++ = '(';
		strcpy(bufp, inet_ntoa(ia));
		bufp += strlen(bufp);
		*bufp++ = ')';
	}
	if (bcmp(&noaddr, &a->a_eaddr, sizeof noaddr) || a->a_naddr == 0) {
		char *vendor;

		if (bufp == buf) {
			if (ether_ntohost(bufp, &a->a_eaddr) == 0)
				bufp += strlen(bufp);
		}
		*bufp++ = '[';
		strcpy(bufp, ether_ntoa(&a->a_eaddr));
		bufp += strlen(bufp);
		vendor = ether_vendor(&a->a_eaddr);
		if (vendor != 0) {
			*bufp++ = '/';
			strcpy(bufp, vendor);
			bufp += strlen(bufp);
		}
		*bufp++ = ']';
	}
	*bufp = 0;
	return buf;
}

static void
printcollectvaluechain(struct collectvalue *cv, struct collectvalue *cvt,
			struct collectvalue *cvh)
{
	struct collectvalue *h, *t;
	struct protocol *pr;

	for (t = cvt, h = cvh; cv != 0; cv = cv->cv_next) {
		if (protolist[0] != 0) {
			int i;
			for (i = 0; i < PRID_MAX; i++) {
				if (protolist[i] == cv->cv_protoid)
					break;
			}
			if (i == PRID_MAX)
				continue;
		}

		pr = findprotobyid(cv->cv_protoid);
		if (pr != 0) {
			space(4);
			fputs(pr->pr_name, stdout);
			space(PR_NAMLEN - pr->pr_namlen);
		} else {
			printf("    %#.4x", cv->cv_protoid);
			space(PR_NAMLEN - 6);
		}
		printf(" %12.0f", cv->cv_count.c_packets);
		for ( ; t != 0; t = t->cv_next) {
			if (t->cv_protoid >= cv->cv_protoid)
				break;
		}
		if (t != 0 && t->cv_protoid == cv->cv_protoid) {
			float p = 100.0 * cv->cv_count.c_packets /
							t->cv_count.c_packets;
			printf(" [%6.2f%%]", p);
		} else
			space(10);
		for ( ; h != 0; h = h->cv_next) {
			if (h->cv_protoid >= cv->cv_protoid)
				break;
		}
		if (h != 0 && h->cv_protoid == cv->cv_protoid) {
			float p = 100.0 * cv->cv_count.c_packets /
							h->cv_count.c_packets;
			printf(" [%6.2f%%]", p);
		} else
			space(10);

		printf(" %12.0f", cv->cv_count.c_bytes);
		if (t != 0 && t->cv_protoid == cv->cv_protoid) {
			float b = 100.0 * cv->cv_count.c_bytes /
							t->cv_count.c_bytes;
			printf(" [%6.2f%%]", b);
		} else
			space(10);
		if (h != 0 && h->cv_protoid == cv->cv_protoid) {
			float b = 100.0 * cv->cv_count.c_bytes /
							h->cv_count.c_bytes;
			printf(" [%6.2f%%]", b);
		}
		putchar('\n');
	}
}

static void
merge_value(struct collectvalue **cp, struct collectvalue *newcv)
{
	struct collectvalue *cv;
	int p;

	for ( ; newcv != 0; newcv = newcv->cv_next) {
		p = newcv->cv_protoid;
		cv = 0;

		for ( ; *cp != 0; cp = &(*cp)->cv_next) {
			if ((*cp)->cv_protoid < p)
				continue;
			if ((*cp)->cv_protoid == p)
				cv = *cp;
			break;
		}
		if (cv == 0) {
			cv = new(struct collectvalue);
			cv->cv_next = *cp;
			*cp = cv;
			cv->cv_protoid = p;
			cv->cv_count.c_packets = newcv->cv_count.c_packets;
			cv->cv_count.c_bytes = newcv->cv_count.c_bytes;
		} else {
			cv->cv_count.c_packets += newcv->cv_count.c_packets;
			cv->cv_count.c_bytes += newcv->cv_count.c_bytes;
		}
	}
}

static int
samehost(struct addr *a1, struct addr *a2)
{
	if (a1->a_naddr == 0 || a2->a_naddr == 0) {
		if (a1->a_naddr == 0 && a2->a_naddr == 0) {
			/*
			 * Both are ethernet addresses, just compare
			 */
			return !bcmp(&a1->a_eaddr, &a2->a_eaddr,
						sizeof a1->a_eaddr);
		}
		/*
		 * One is an ethernet address and one is a network address,
		 * this should have been resolved already so return 0.
		 */
		return 0;
	}

	return (a1->a_naddr == a2->a_naddr &&
		!bcmp(&a1->a_eaddr, &a2->a_eaddr, sizeof a1->a_eaddr));
}

static void
space(int len)
{
#define MAX_BLANK	80
	static char blanks[] = "                                                                                ";

	if (len > 0)
		fputs(&blanks[MAX_BLANK - len], stdout);
}
