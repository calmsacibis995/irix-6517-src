/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snoopd test program
 *
 *	$Revision: 1.15 $
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
#include <bstring.h>
#include <errno.h>
/* #include <getopt.h> */
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "exception.h"
#include "expr.h"
#include "protocol.h"
#include "histogram.h"
#include "netlook.h"
#include "addrlist.h"
#include "snoopstream.h"
#include "snooper.h"

#ifndef INADDR_NONE
#define INADDR_NONE     0xffffffff
#endif

main(int argc, char** argv)
{
	struct hostent* hp;
	struct sockaddr_in srv_addr;
	SnoopStream ss;
	struct histogram hist;
	ExprSource source;
	ExprError error;
	struct nlpacket nlp;
	struct alpacket al;
	int bin, n, dohist = 0, donl = 0, dolist = 0;
	u_int interval = 10, i;
	char *hostname = 0;
	char *interface;
	struct timeval tv;
	static char *filters[6] = {
		"snoop", "ip.host(squaw)", "ip.host(fddi-bonnie)",
		"snoop", "ip.host(squaw)", "ip.host(fddi-bonnie)"
	};
#ifdef __sgi
	extern int _yp_disabled;
#endif

	exc_progname = "client";
	exc_autofail = 1;

#ifdef __sgi
	_yp_disabled = 1;
#endif
	initprotocols();
#ifdef __sgi
	_yp_disabled = 0;
#endif

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	while ((n = getopt(argc, argv, "ahi:np:t:")) != -1) {
		switch (n) {
		    case 'a':
			dolist = 1;
			break;

		    case 'i':
			hostname = optarg;
			break;

		    case 'n':
			donl = 1;
			break;

		    case 'h':
			dohist = 1;
			break;

		    case 'p':
			tv.tv_sec = atoi(optarg);
			break;

		    case 't':
			interval = atoi(optarg);
			break;
		    default:
			break;
		}
	}

	if (dohist == 0 && donl == 0 && dolist == 0)
		dohist = donl = dolist = 1;

	if (hostname == 0) {
		char name[64];
		if (gethostname(name, sizeof name) < 0)
			exc_raise(errno, "couldn't get hostname");
		hp = gethostbyname(name);
	} else {
		enum snoopertype st = getsnoopertype(hostname, &interface); 
		if (st != ST_REMOTE)
			exc_raise(errno, "not remote snooper type");
		hp = gethostbyname(hostname);
	}

	if (hp == NULL) {
		perror("couldn't get hostent");
		exit(1);
	}

	fprintf(stderr, "Open...");
	bcopy(hp->h_addr, &srv_addr.sin_addr, hp->h_length);
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = 0;

	if (!ss_open(&ss, hp->h_name, &srv_addr, &tv)) {
		fprintf(stderr, "error in open\n");
		exit(1);
	}
	fprintf(stderr, "successful\n");

	/*
	 * Test the histogram stuff
	 */
	if (dohist == 0)
		goto testnetlook;

	fprintf(stderr, "Subscribe to histogram...");
	if (!ss_subscribe(&ss, SS_HISTOGRAM, interface, 0, 0, interval)) {
		fprintf(stderr, "error in subscribe\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Set snoop length...");
	if (!ss_setsnooplen(&ss, 128)) {
		fprintf(stderr, "error in setsnooplen\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Add promiscuous filter...");
	bin = ss_add(&ss, 0, &error);
	if (bin < 0) {
		fprintf(stderr, "error in add: %s\n", error.err_message);
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	source.src_path = argv[0];
	source.src_line = 0;
	if (optind < argc)
		source.src_buf = argv[optind];
	else
		source.src_buf = "nfs";
	fprintf(stderr, "Add \"%s\" filter...", source.src_buf);
	bin = ss_compile(&ss, &source, &error);
	if (bin < 0) {
		fprintf(stderr, "error in add\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Start...");
	if (!ss_start(&ss)) {
		fprintf(stderr, "error in start\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Reading...\n");
	fprintf(stderr, "\ttime\t\tpromiscuous\t%s\n", source.src_buf);
	for (i = 0; i < 30; i++) {
		struct tm *tm;
		int j;

		if (!ss_read(&ss, (xdrproc_t) xdr_histogram, &hist)) {
			fprintf(stderr, "error in read\n");
			ss_close(&ss);
			exit(1);
		}

		if (!timerisset(&hist.h_timestamp))
			break;

		tm = localtime(&hist.h_timestamp.tv_sec);

		fprintf(stderr, "%02d:%02d:%02d.%03ld\t",
				tm->tm_hour, tm->tm_min, tm->tm_sec,
				hist.h_timestamp.tv_usec / 1000);

		for (j = 0; j <= bin; j++) {
			fprintf(stderr, "%7.0f %7.0f\t",
					hist.h_count[j].c_packets,
					hist.h_count[j].c_bytes);
		}
		putc('\n', stderr);

		if (i == 9) {
			fprintf(stderr, "Stop...");
			if (!ss_stop(&ss)) {
				fprintf(stderr, "error in stop\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
			interval *= 2;
			fprintf(stderr, "Changing interval to %d...", interval);
			if (!ss_setinterval(&ss, interval)) {
				fprintf(stderr, "error in setinverval\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
			fprintf(stderr, "Start...");
			if (!ss_start(&ss)) {
				fprintf(stderr, "error in start\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
		}
		if (i == 19) {
			fprintf(stderr, "Stop...");
			if (!ss_stop(&ss)) {
				fprintf(stderr, "error in stop\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
			interval /= 2;
			fprintf(stderr, "Changing interval to %d...", interval);
			if (!ss_setinterval(&ss, interval)) {
				fprintf(stderr, "error in setinverval\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
			fprintf(stderr, "Start...");
			if (!ss_start(&ss)) {
				fprintf(stderr, "error in start\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
		}
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Stop...");
	if (!ss_stop(&ss)) {
		fprintf(stderr, "error in stop\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Remove...");
	for ( ; bin >= 0; bin--) {
		if (!ss_delete(&ss, bin)) {
			fprintf(stderr, "error in remove\n");
			ss_close(&ss);
			exit(1);
		}
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Unsubscribe...");
	if (!ss_unsubscribe(&ss)) {
		fprintf(stderr, "error in unsubscribe\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	/*
	 * Test the histogram matrix
	 */
	fprintf(stderr, "Subscribe to histogram matrix...");
	if (!ss_subscribe(&ss, SS_HISTOGRAM, interface, 3, 3, interval)) {
		fprintf(stderr, "error in subscribe\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Set snoop length...");
	if (!ss_setsnooplen(&ss, 128)) {
		fprintf(stderr, "error in setsnooplen\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Add filters...");
	for (i = 0; i < 6; i++) {
		source.src_buf = filters[i];
		bin = ss_compile(&ss, &source, &error);
		if (bin < 0) {
			fprintf(stderr,"error in add: %s\n", error.err_message);
			ss_close(&ss);
			exit(1);
		}
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Start...");
	if (!ss_start(&ss)) {
		fprintf(stderr, "error in start\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Reading...\n");
	for (i = 0; i < 30; i++) {
		struct tm *tm;
		int j, k;

		if (!ss_read(&ss, (xdrproc_t) xdr_histogram, &hist)) {
			fprintf(stderr, "error in read\n");
			ss_close(&ss);
			exit(1);
		}

		if (!timerisset(&hist.h_timestamp))
			break;

		tm = localtime(&hist.h_timestamp.tv_sec);

		for (k = 0; k < 3; k++) {
			if (k == 0)
				fprintf(stderr, "%02d:%02d:%02d.%03ld\t",
					tm->tm_hour, tm->tm_min, tm->tm_sec,
					hist.h_timestamp.tv_usec / 1000);
			else
				fprintf(stderr, "\t\t");
			for (j = 0; j < 3; j++) {
				fprintf(stderr, "%7.0f %7.0f\t",
						hist.h_count[k*3+j].c_packets,
						hist.h_count[k*3+j].c_bytes);
			}
			putc('\n', stderr);
		}

		if (i == 9) {
			fprintf(stderr, "Stop...");
			if (!ss_stop(&ss)) {
				fprintf(stderr, "error in stop\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
			interval *= 2;
			fprintf(stderr, "Changing interval to %d...", interval);
			if (!ss_setinterval(&ss, interval)) {
				fprintf(stderr, "error in setinverval\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
			fprintf(stderr, "Start...");
			if (!ss_start(&ss)) {
				fprintf(stderr, "error in start\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
		}
		if (i == 19) {
			fprintf(stderr, "Stop...");
			if (!ss_stop(&ss)) {
				fprintf(stderr, "error in stop\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
			interval /= 2;
			fprintf(stderr, "Changing interval to %d...", interval);
			if (!ss_setinterval(&ss, interval)) {
				fprintf(stderr, "error in setinverval\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
			fprintf(stderr, "Start...");
			if (!ss_start(&ss)) {
				fprintf(stderr, "error in start\n");
				ss_close(&ss);
				exit(1);
			}
			fprintf(stderr, "successful\n");
		}
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Stop...");
	if (!ss_stop(&ss)) {
		fprintf(stderr, "error in stop\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Remove...");
	for ( ; bin >= 0; bin--) {
		if (!ss_delete(&ss, bin)) {
			fprintf(stderr, "error in remove\n");
			ss_close(&ss);
			exit(1);
		}
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Unsubscribe...");
	if (!ss_unsubscribe(&ss)) {
		fprintf(stderr, "error in unsubscribe\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

testnetlook:
	/*
	 * Test the netlook stuff
	 */
	if (donl == 0)
		goto testaddrlist;

	fprintf(stderr, "Subscribe to NetLook...");
	if (!ss_subscribe(&ss, SS_NETLOOK, interface, 10, 0, interval)) {
		fprintf(stderr, "error in subscribe\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Set snoop length...");
	if (!ss_setsnooplen(&ss, 128)) {
		fprintf(stderr, "error in setsnooplen\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	if (optind < argc) {
		source.src_buf = argv[optind];
		fprintf(stderr, "Add \"%s\" filter...", source.src_buf);
		bin = ss_compile(&ss, &source, &error);
	} else {
		fprintf(stderr, "Adding promiscuous filter...");
		bin = ss_add(&ss, 0, &error);
	}
	if (bin < 0) {
		fprintf(stderr, "error in add: %s\n", error.err_message);
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Start...");
	if (!ss_start(&ss)) {
		fprintf(stderr, "error in start\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Reading...\n");
	for (i = 0; i < 5; i++) {
		struct nlspacket* sp = nlp.nlp_nlsp;
		int j;

		if (!ss_read(&ss, (xdrproc_t) xdr_nlpacket, &nlp)) {
			fprintf(stderr, "error in read\n");
			ss_close(&ss);
			exit(1);
		}

		for (j = 0; j < nlp.nlp_count; j++, sp++) {
			int k;
			struct tm *tm = localtime(&sp->nlsp_timestamp.tv_sec);

			fprintf(stderr, "----- %d -----\n", j);
			fprintf(stderr, "type: %#04x\n", sp->nlsp_type);
			fprintf(stderr, "length: %d\n", sp->nlsp_length);
			fprintf(stderr, "time: %02d:%02d:%02d.%03ld\n",
				tm->tm_hour, tm->tm_min, tm->tm_sec,
				sp->nlsp_timestamp.tv_usec / 1000);
			fprintf(stderr, "protocols: %d\t",
					sp->nlsp_protocols);
			for (k = 0; k < sp->nlsp_protocols; k++) {
				Protocol *pr =
					findprotobyid(sp->nlsp_protolist[k]);
				if (pr != 0)
					fprintf(stderr, "%s%s",
						k == 0 ? "(" : ", ",
						pr->pr_name);
				else
					fprintf(stderr, "%s%s",
						k == 0 ? "(" : ", ",
						sp->nlsp_protolist[k]);
			}
			fprintf(stderr, ")\n");

			/* Don't use the names if they are addresses */
			if (ether_aton(sp->nlsp_src.nlep_name) != NULL ||
			    inet_addr(sp->nlsp_src.nlep_name) != INADDR_NONE)
				sp->nlsp_src.nlep_name[0] = '\0';

			if (ether_aton(sp->nlsp_dst.nlep_name) != NULL ||
                            inet_addr(sp->nlsp_dst.nlep_name) != INADDR_NONE)
				sp->nlsp_dst.nlep_name[0] = '\0';

			fprintf(stderr, "src:");
			fprintf(stderr, "\t%s",
					ether_ntoa(&sp->nlsp_src.nlep_eaddr));
			if (sp->nlsp_src.nlep_naddr != 0)
				fprintf(stderr, "\t%s",
					inet_ntoa(sp->nlsp_src.nlep_naddr));
			else
				fprintf(stderr, "\t\t");
			if (sp->nlsp_src.nlep_name[0] != '\0')
				fprintf(stderr,
					"\t%s", sp->nlsp_src.nlep_name);
			else
				fprintf(stderr, "\t\t\t\t");
			if (sp->nlsp_src.nlep_port != 0)
				fprintf(stderr, "\t%d",
					sp->nlsp_src.nlep_port);
			putc('\n', stderr);

			fprintf(stderr, "dst:");
			fprintf(stderr, "\t%s",
					ether_ntoa(&sp->nlsp_dst.nlep_eaddr));
			if (sp->nlsp_dst.nlep_naddr != 0)
				fprintf(stderr, "\t%s",
					inet_ntoa(sp->nlsp_dst.nlep_naddr));
			if (sp->nlsp_dst.nlep_name[0] != '\0')
				fprintf(stderr, "\t%s",
					sp->nlsp_dst.nlep_name);
			if (sp->nlsp_dst.nlep_port != 0)
				fprintf(stderr, "\t%d",
					sp->nlsp_dst.nlep_port);
			putc('\n', stderr);
		}

		if (nlp.nlp_type == NLP_ENDOFDATA)
			break;
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Stop...");
	if (!ss_stop(&ss)) {
		fprintf(stderr, "error in stop\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Remove...");
	if (!ss_delete(&ss, 0)) {
		fprintf(stderr, "error in remove\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Unsubscribe...");
	if (!ss_unsubscribe(&ss)) {
		fprintf(stderr, "error in unsubscribe\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

testaddrlist:
	/*
	 * Test the addrlist stuff
	 */
	if (dolist == 0)
		goto done;

	fprintf(stderr, "Subscribe to AddrList...");
	if (!ss_subscribe(&ss, SS_ADDRLIST, interface, 10, 0, interval)) {
		fprintf(stderr, "error in subscribe\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Set snoop length...");
	if (!ss_setsnooplen(&ss, 128)) {
		fprintf(stderr, "error in setsnooplen\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	if (optind < argc) {
		source.src_buf = argv[optind];
		fprintf(stderr, "Add \"%s\" filter...", source.src_buf);
		bin = ss_compile(&ss, &source, &error);
	} else {
		fprintf(stderr, "Adding promiscuous filter...");
		bin = ss_add(&ss, 0, &error);
	}
	if (bin < 0) {
		fprintf(stderr, "error in add: %s\n", error.err_message);
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Start...");
	if (!ss_start(&ss)) {
		fprintf(stderr, "error in start\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Reading...\n");
	for (i = 0; i < 5; i++) {
		struct tm *tm;
		int j;
		struct alentry *e = al.al_entry;

		if (!ss_read(&ss, (xdrproc_t) xdr_alpacket, &al)) {
			fprintf(stderr, "error in read\n");
			ss_close(&ss);
			exit(1);
		}

		fprintf(stderr, "===== %d =====\n", i);
		fprintf(stderr, "ver:\t%d\n", al.al_version);
		fprintf(stderr, "type:\t%d\n", al.al_type);
		fprintf(stderr, "source:\t%s\n", inet_ntoa(al.al_source));
		tm = localtime(&al.al_timestamp.tv_sec);
		fprintf(stderr, "time:\t%02d:%02d:%02d.%03ld\n",
			tm->tm_hour, tm->tm_min, tm->tm_sec,
			al.al_timestamp.tv_usec / 1000);

		for (j = 0; j < al.al_nentries; j++, e++) {
			int k;

			fprintf(stderr, "----- %d -----\n", j);
			/* Don't use the names if they are addresses */
			if (ether_aton(e->ale_src.alk_name) != NULL ||
			    inet_addr(e->ale_src.alk_name) != INADDR_NONE)
				e->ale_src.alk_name[0] = '\0';
			if (ether_aton(e->ale_dst.alk_name) != NULL ||
                            inet_addr(e->ale_dst.alk_name) != INADDR_NONE)
				e->ale_dst.alk_name[0] = '\0';

			fprintf(stderr, "src:\t%s",
					ether_ntoa(&e->ale_src.alk_paddr));
			if (e->ale_src.alk_naddr != 0)
				fprintf(stderr, "\t%s",
					inet_ntoa(e->ale_src.alk_naddr));
			else
				fputs("\t", stderr);
			if (e->ale_src.alk_name[0] != '\0')
				fprintf(stderr, "\t%s", e->ale_src.alk_name);
			putc('\n', stderr);

			fprintf(stderr, "dst:\t%s",
					ether_ntoa(&e->ale_dst.alk_paddr));
			if (e->ale_dst.alk_naddr != 0)
				fprintf(stderr, "\t%s",
					inet_ntoa(e->ale_dst.alk_naddr));
			else
				fputs("\t", stderr);
			if (e->ale_dst.alk_name[0] != '\0')
				fprintf(stderr, "\t%s",
					e->ale_dst.alk_name);
			putc('\n', stderr);
			fprintf(stderr, "%.0f\t%.0f\n",
				e->ale_count.c_packets,
				e->ale_count.c_bytes);
		}

		if (al.al_type == AL_ENDOFDATA)
			break;
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Stop...");
	if (!ss_stop(&ss)) {
		fprintf(stderr, "error in stop\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Remove...");
	if (!ss_delete(&ss, 0)) {
		fprintf(stderr, "error in remove\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

	fprintf(stderr, "Unsubscribe...");
	if (!ss_unsubscribe(&ss)) {
		fprintf(stderr, "error in unsubscribe\n");
		ss_close(&ss);
		exit(1);
	}
	fprintf(stderr, "successful\n");

done:
	fprintf(stderr, "Close...");
	ss_close(&ss);
	fprintf(stderr, "successful\n");
	exit(0);
}
