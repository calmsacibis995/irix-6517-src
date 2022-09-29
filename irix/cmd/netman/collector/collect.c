/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Collector
 *
 *	$Revision: 1.11 $
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

#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <libc_synonyms.h>
/* #include <getopt.h> */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <net/if.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "protocols/ether.h"
#include "snooper.h"
#include "netlook.h"
#include "snoopstream.h"
#include "expr.h"
#include "exception.h"
#include "index.h"
#include "heap.h"
#include "collect.h"

static void update_entry(Entry *, unsigned int, unsigned int);
static void free_index(Index *);
static unsigned int sort(Index *, Entry ***);
static int compare(const void *, const void *);

#define SECOND			1
#define MINUTE			(60 * SECOND)
#define HOUR			(60 * MINUTE)
#define DAY			(24 * HOUR)

#define DEFAULT_INTERVAL	HOUR
#define DEFAULT_PERIOD		DAY

#define NETLOOK_BUFFERSIZE	60
#define NETLOOK_TIMEOUT		(30 * SECOND * 10)

#ifndef IN_CLASSD_NET
#define IN_CLASSD_NET		0xf0000000
#endif

extern struct indexops collectops;
extern int _yp_disabled;

main(int argc, char** argv)
{
	Index *collectindex;
	SnoopStream ss;
	struct nlpacket nlp;
	time_t interval, starttime, endtime;
	time_t period, startperiod, endperiod;
	pid_t pid;
	unsigned int hashsize;
	struct hostent* hp;
	struct sockaddr_in srv_addr, mask_addr;
	char buf[_POSIX_ARG_MAX], path[_POSIX_PATH_MAX];
	char hostname[64], ifname[8];
	char *interface, *dir;
	int n;
	ExprSource *filter;
	ExprError ee;
	enum snoopertype st;
	unsigned int file = 0;

	exc_progname = "netcollect";
	exc_autofail = 1;

	/* Ignore SIGHUP caused by logout */
	signal(SIGHUP, SIG_IGN);

#ifdef __sgi
	_yp_disabled = 1;
#endif
	initprotocols();
#ifdef __sgi
	_yp_disabled = 0;
#endif

	pid = 0;
	hashsize = 1024;
	if (gethostname(hostname, 64) < 0)
		exc_raise(errno, "couldn't get hostname");
	path[0] = 0;
	interval = DEFAULT_INTERVAL;
	period = DEFAULT_PERIOD;
	while ((n = getopt(argc, argv, "h:i:p:t:T:")) != -1) {
		switch (n) {
		    case 'h':
			hashsize = atoi(optarg);
			break;
		    case 'i':
			strncpy(hostname, optarg, 64);
			break;
		    case 'p':
			strcpy(path, optarg);
			break;
		    case 't':
			interval = atoi(optarg) * 60;
			break;
		    default:
			fputs(
"usage: netcollect [-h hashsize] [-i interface] [-p path] [-t interval] [filter]\n",
				stderr);
			exit(EINVAL);
		}
	}
	/*
	 * Treat arguments as a filter expression.
	 */
	if (optind == argc)
		filter = 0;
	else {
		buf[0] = '\0';
		for (;;) {
			strcat(buf, argv[optind]);
			if (++optind == argc)
				break;
			strcat(buf, " ");
		}
		newExprSource(filter, buf);
	}

	if (interval == 0 || interval > period)
		exc_raise(EINVAL, "interval must be > 0 and < 1440");

	switch (getsnoopertype(hostname, &interface)) {
	    case ST_NULL:
	    case ST_TRACE:
		exc_raise(EINVAL, "interface");

	    case ST_LOCAL:
		if (strcmp(hostname, interface) == 0) {
			strcpy(ifname, interface);
			interface = ifname;
		}
		/* Fall through ... */

	    case ST_REMOTE:
		break;
	}
	hp = gethostbyname(hostname);
	if (hp == NULL)
		exc_raise(errno, "error getting host information for %s",
				 hostname);

	bcopy(hp->h_addr, &srv_addr.sin_addr, hp->h_length);
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = 0;

	if (!ss_open(&ss, hostname, &srv_addr, 0))
		exc_raise(errno, "error in open");

	if (!ss_subscribe(&ss, SS_NETLOOK, interface, NETLOOK_BUFFERSIZE,
			  0, NETLOOK_TIMEOUT))
		exc_raise(errno, "error in subscribe");

	if (!ss_setsnooplen(&ss, 128))
		exc_raise(errno, "error in setsnooplen");

	exc_defer = 1;
	if (!ss_getaddr(&ss, SIOCGIFADDR, (struct sockaddr *) &srv_addr)) {
		/* Must be collecting from a file */
		bcopy(hp->h_addr, &srv_addr.sin_addr, hp->h_length);
		if (IN_CLASSA(srv_addr.sin_addr.s_addr))
			mask_addr.sin_addr.s_addr = IN_CLASSA_NET;
		else if (IN_CLASSB(srv_addr.sin_addr.s_addr))
			mask_addr.sin_addr.s_addr = IN_CLASSB_NET;
		else if (IN_CLASSC(srv_addr.sin_addr.s_addr))
			mask_addr.sin_addr.s_addr = IN_CLASSC_NET;
		else if (IN_CLASSD(srv_addr.sin_addr.s_addr))
			mask_addr.sin_addr.s_addr = IN_CLASSD_NET;
		else
			mask_addr.sin_addr.s_addr = IN_CLASSC_NET;
		file = 1;
		exc_defer = 0;
	} else {
		exc_defer = 0;
		mask_addr.sin_family = AF_INET;
		if (!ss_getaddr(&ss, SIOCGIFNETMASK,
				(struct sockaddr *) &mask_addr))
			exc_raise(errno, "error in getaddr");
	}

	if (ss_compile(&ss, filter, &ee) < 0)
		exc_raise(errno, "error in compile");

	collectindex = index(hashsize, sizeof(struct collectkey), &collectops);

	dir = &path[strlen(path)];
	if (dir != path && *dir != '/')
		*dir++ = '/';

	if (file == 0) {
		struct tm *tm;
		int dst;

		if (time(&starttime) == -1)
			exc_raise(errno, "error getting start time");
		endtime = interval * (starttime / interval + 1) - 1;
		startperiod = starttime;
		tm = localtime(&startperiod);
		endperiod = startperiod + DAY -
			    tm->tm_hour * HOUR - tm->tm_min * MINUTE -
			    tm->tm_sec - 1;
		dst = tm->tm_isdst;
		tm = localtime(&endperiod);
#ifdef __sgi
		endperiod += (dst - tm->tm_isdst) * (_timezone - _altzone);
#else
#ifdef sun
		/* XXX No adjustment! */
#else
		endperiod += (dst - tm->tm_isdst) * (timezone - altzone);
#endif
#endif

		strcpy(dir, getdirectory(&startperiod, &endperiod));
		if (!createdirectory(path))
			exc_raise(errno, "error creating data directory");
	}

	if (!ss_start(&ss))
		exc_raise(errno, "error in start");

	for (nlp.nlp_type = NLP_SNOOPDATA; nlp.nlp_type != NLP_ENDOFDATA; ) {
		struct nlspacket *sp, *end;

		if (pid != 0 && waitpid(pid, 0, WNOHANG) != 0)
			pid = 0;

		if (!ss_read(&ss, xdr_nlpacket, &nlp))
			exc_raise(errno, "error in read");

		if (file != 0 && nlp.nlp_count != 0) {
			struct tm *tm;
			int dst;

			starttime = nlp.nlp_nlsp[0].nlsp_timestamp.tv_sec;
			endtime = interval * (starttime / interval + 1) - 1;
			startperiod = starttime;
			tm = localtime(&startperiod);
			endperiod = startperiod + DAY -
				    tm->tm_hour * HOUR - tm->tm_min * MINUTE -
				    tm->tm_sec - 1;
			dst = tm->tm_isdst;
			tm = localtime(&endperiod);
#ifdef __sgi
			endperiod += (dst - tm->tm_isdst) * (_timezone - _altzone);
#else
#ifdef sun
			/* XXX No adjustment! */
#else
			endperiod += (dst - tm->tm_isdst) * (timezone - altzone);
#endif
#endif

			strcpy(dir, getdirectory(&startperiod, &endperiod));
			if (!createdirectory(path))
				exc_raise(errno, "error creating data directory");
			
			file = 0;
		}

		sp = nlp.nlp_nlsp;
		end = &sp[nlp.nlp_count];
		for ( ; sp < end; sp++) {
			struct collectkey ck;
			Entry **ep;
			int protocols, i;

			if (sp->nlsp_timestamp.tv_sec > endtime) {
				pid = fork();
				if (pid == 0 || pid == -1) {
					Entry **ep;
					unsigned int nel;

					strcpy(dir, getpath(&starttime,
							    &endtime));
					nel = sort(collectindex, &ep);
					if (!save_entrylist(path,
						&srv_addr.sin_addr.s_addr,
						&mask_addr.sin_addr.s_addr,
						&starttime, &endtime, ep, &nel))
						exc_raise(errno,
							"error in write");
					if (pid == 0)
						exit(0);
				}

				free_index(collectindex);
				starttime = endtime + 1;
				endtime += interval;
				if (sp->nlsp_timestamp.tv_sec > endperiod) {
					startperiod = endperiod + 1;
					endperiod += period;
					strcpy(dir, getdirectory(&startperiod,
								 &endperiod));
					if (!createdirectory(path))
						exc_raise(errno,
					    "error creating data directory");
				}
			}

			ck.ck_src.a_eaddr = sp->nlsp_src.nlep_eaddr;
			ck.ck_src.a_port = sp->nlsp_src.nlep_port;
			ck.ck_src.a_naddr = sp->nlsp_src.nlep_naddr;
			ck.ck_dst.a_eaddr = sp->nlsp_dst.nlep_eaddr;
			ck.ck_dst.a_port = sp->nlsp_dst.nlep_port;
			ck.ck_dst.a_naddr = sp->nlsp_dst.nlep_naddr;

			ep = in_lookup(collectindex, &ck);
			if (*ep == 0)
				(void) in_add(collectindex, &ck, 0, ep);

			protocols = sp->nlsp_protocols;
			for (i = 0; i < protocols; i++)
				update_entry(*ep, sp->nlsp_protolist[i],
							sp->nlsp_length);

			if (sp->nlsp_type != 0)
				update_entry(*ep, sp->nlsp_type,
							sp->nlsp_length);
		}
	}

	/* Got end of file from snoopd */
	{
		Entry **ep;
		unsigned int nel;

		endtime = nlp.nlp_nlsp[nlp.nlp_count-1].nlsp_timestamp.tv_sec;
		strcpy(dir, getpath(&starttime, &endtime));
		nel = sort(collectindex, &ep);
		if (!save_entrylist(path,
			&srv_addr.sin_addr.s_addr,
			&mask_addr.sin_addr.s_addr,
			&starttime, &endtime, ep, &nel))
			exc_raise(errno,
				"error in write");
	}
}

static void
update_entry(Entry *ent, unsigned int p, unsigned int len)
{
	struct collectvalue **cp, *cv;

	cv = 0;
	for (cp = (struct collectvalue **) &ent->ent_value; *cp != 0;
						cp = &(*cp)->cv_next) {
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
		cv->cv_count.c_packets = 1.0;
		cv->cv_count.c_bytes = len;
	} else {
		cv->cv_count.c_packets++;
		cv->cv_count.c_bytes += len;
	}
}

static void
free_index(Index *in)
{
	int count;
	Entry **ep, *ent;

	count = in->in_buckets;
	for (ep = in->in_hash; --count >= 0; ep++) {
		while ((ent = *ep) != 0) {
			if (ent->ent_value != 0)
				in_freeval(in, ent);
			*ep = ent->ent_next;
			delete(ent);
		}
	}
	return;
}

static unsigned int
sort(Index *in, Entry ***lp)
{
	int count;
	unsigned int total, i;
	Entry **ep, *ent, **list;

	total = 0;
	count = in->in_buckets;
	for (ep = in->in_hash; --count >= 0; ep++) {
		for (ent = *ep; ent != 0; ent = ent->ent_next) {
			if (ent->ent_value != 0)
				total++;
		}
	}

	*lp = list = vnew(total, Entry *);
	i = 0;
	count = in->in_buckets;
	for (ep = in->in_hash; --count >= 0; ep++) {
		for (ent = *ep; ent != 0; ent = ent->ent_next) {
			if (ent->ent_value != 0)
				list[i++] = ent;
		}
	}

	qsort(list, total, sizeof(Entry *), compare);
	return total;
}

static int
compare(const void *e1, const void *e2)
{
	struct collectkey *c1 = (struct collectkey *) (*(Entry **)e1)->ent_key;
	struct collectkey *c2 = (struct collectkey *) (*(Entry **)e2)->ent_key;

	return keycmp(c1, c2);
}
