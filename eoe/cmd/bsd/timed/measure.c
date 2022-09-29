/*-
 * Copyright (c) 1985, 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)measure.c	5.1 (Berkeley) 5/11/93";
#endif /* not lint */

#ifdef sgi
#ident "$Revision: 1.14 $"
#endif

#include "globals.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <cap_net.h>

#define MSEC_DAY	(SECDAY*1000)

#define PACKET_IN	1024

#define MSGS		5		/* timestamps to average */
#define TRIALS		10		/* max # of timestamps sent */

int sock_raw = -1;
int sock_snd = -1;

int measure_delta;

extern int in_cksum(u_short*, int);

static n_short seqno = 0;

/*
 * Measures the differences between machines' clocks using
 * ICMP timestamp messages.
 */
int					/* status val defined in globals.h */
measure(u_long maxmsec,			/* wait this many msec at most */
	u_long wmsec,			/* msec to wait for an answer */
	char *hname,
	struct sockaddr_in *addr,
	int print)			/* print complaints on stderr */
{
	int length;
	int measure_status;
	int rcvcount, trials;
	int cc, count, on;
	fd_set ready;
	long sendtime, recvtime, histime1, histime2;
	long idelta, odelta, total;
	long min_idelta, min_odelta;
	struct timeval tdone, tcur, ttrans, twait, tout;
	union {
		u_char c[PACKET_IN];
		struct ip ip;
	} ipkt;
	struct {
		struct ip ip;
		struct icmp icmp;
	} opkt;
	register struct icmp *icp;

	min_idelta = min_odelta = 0x7fffffff;
	measure_status = HOSTDOWN;
	measure_delta = HOSTDOWN;
	errno = 0;

	/* open raw sockets used to measure time differences */
	if (sock_raw < 0) {
		sock_raw = cap_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (sock_raw < 0)  {
			syslog(LOG_ERR, "opening raw input socket: %m");
			goto quit;
		}
		sock_snd = cap_socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
		if (sock_snd < 0) {
			perror("opening raw output socket");
			(void)close(sock_raw);
			sock_raw = -1;
			return(-1);
		}
	}


	/*
	 * empty the icmp input queue
	 */
	FD_ZERO(&ready);
	for (;;) {
		tout.tv_sec = tout.tv_usec = 0;
		FD_SET(sock_raw, &ready);
		if (select(sock_raw+1, &ready, 0,0, &tout)) {
			length = sizeof(struct sockaddr_in);
			cc = recvfrom(sock_raw, &ipkt, sizeof(ipkt), 0,
				      0,&length);
			if (cc < 0)
				goto quit;
			continue;
		}
		break;
	}

	/*
	 * Choose the smallest transmission time in each of the two
	 * directions. Use these two latter quantities to compute the delta
	 * between the two clocks.
	 */

	bzero(&opkt, sizeof(opkt));
	opkt.ip.ip_dst = addr->sin_addr;
	opkt.ip.ip_tos = IPTOS_LOWDELAY;
	opkt.ip.ip_p = IPPROTO_ICMP;
	opkt.ip.ip_len = sizeof(opkt);
	opkt.ip.ip_ttl = MAXTTL;
	opkt.icmp.icmp_type = ICMP_TSTAMP;
	opkt.icmp.icmp_id = getpid();
	opkt.icmp.icmp_seq = seqno;

	FD_ZERO(&ready);

#ifdef sgi
	sginap(1);			/* start at a clock tick */
#endif /* sgi */

	(void)gettimeofday(&tdone, 0);
	mstotvround(&tout, maxmsec);
	timevaladd(&tdone, &tout);		/* when we give up */

	mstotvround(&twait, wmsec);

	rcvcount = 0;
	trials = 0;
	while (rcvcount < MSGS) {
		(void)gettimeofday(&tcur, 0);

		/*
		 * keep sending until we have sent the max
		 */
		if (trials < TRIALS) {
			trials++;
			opkt.icmp.icmp_otime = htonl((tcur.tv_sec % SECDAY)
						     * 1000
						     + tcur.tv_usec / 1000);
			opkt.icmp.icmp_cksum = 0;
			opkt.icmp.icmp_cksum = in_cksum((u_short*)&opkt.icmp,
							sizeof(opkt.icmp));

			count = sendto(sock_snd, &opkt, opkt.ip.ip_len, 0,
				       (struct sockaddr*)addr,
				       sizeof(struct sockaddr));
			if (count < 0) {
				if (measure_status == HOSTDOWN)
					measure_status = UNREACHABLE;
				goto quit;
			}
			++opkt.icmp.icmp_seq;

			ttrans = tcur;
			timevaladd(&ttrans, &twait);
		} else {
			ttrans = tdone;
		}

		while (rcvcount < trials) {
			timevalsub(&tout, &ttrans, &tcur);
			if (tout.tv_sec < 0)
				tout.tv_sec = 0;

			FD_SET(sock_raw, &ready);
			count = select(sock_raw+1, &ready, (fd_set *)0,
				       (fd_set *)0, &tout);
			(void)gettimeofday(&tcur, (struct timezone *)0);
			if (count <= 0)
				break;

			length = sizeof(struct sockaddr_in);
			cc = recvfrom(sock_raw, &ipkt, sizeof(ipkt), 0,
				      0,&length);
			if (cc < 0)
				goto quit;

			/*
			 * got something.  See if it is ours
			 */
			icp = (struct icmp *)&ipkt.c[ipkt.ip.ip_hl << 2];
			if (cc <= sizeof(ipkt.ip)+4
			    || icp->icmp_type != ICMP_TSTAMPREPLY
			    || icp->icmp_id != opkt.icmp.icmp_id
			    || icp->icmp_seq < seqno
			    || icp->icmp_seq >= opkt.icmp.icmp_seq)
				continue;


			sendtime = ntohl(icp->icmp_otime);
			recvtime = ((tcur.tv_sec % SECDAY) * 1000 +
				    tcur.tv_usec / 1000);

			total = recvtime-sendtime;
			if (total < 0)	/* do not hassle midnight */
				continue;

			rcvcount++;
			histime1 = ntohl(icp->icmp_rtime);
			histime2 = ntohl(icp->icmp_ttime);
			/*
			 * a host using a time format different from
			 * msec. since midnight UT (as per RFC792) should
			 * set the high order bit of the 32-bit time
			 * value it transmits.
			 */
			if ((histime1 & 0x80000000) != 0) {
				measure_status = NONSTDTIME;
				goto quit;
			}
			measure_status = GOOD;

			idelta = recvtime-histime2;
			odelta = histime1-sendtime;

			/* do not be confused by midnight */
			if (idelta < -MSEC_DAY/2) idelta += MSEC_DAY;
			else if (idelta > MSEC_DAY/2) idelta -= MSEC_DAY;

			if (odelta < -MSEC_DAY/2) odelta += MSEC_DAY;
			else if (odelta > MSEC_DAY/2) odelta -= MSEC_DAY;

			/* save the quantization error so that we can get a
			 * measurement finer than our system clock.
			 */
			if (total < MIN_ROUND) {
				measure_delta = (odelta - idelta)/2;
				goto quit;
			}

			if (idelta < min_idelta)
				min_idelta = idelta;
			if (odelta < min_odelta)
				min_odelta = odelta;

			measure_delta = (min_odelta - min_idelta)/2;
		}

		if (tcur.tv_sec > tdone.tv_sec
		    || (tcur.tv_sec == tdone.tv_sec
			&& tcur.tv_usec >= tdone.tv_usec))
			break;
	}

quit:
	seqno += TRIALS;		/* allocate our sequence numbers */

	/*
	 * If no answer is received for TRIALS consecutive times,
	 * the machine is assumed to be down
	 */
	if (measure_status == GOOD) {
		if (trace) {
			fprintf(fd,
				"measured delta %4d, %d trials to %-15s %s\n",
				measure_delta, trials,
				inet_ntoa(addr->sin_addr), hname);
		}
	} else if (print) {
		if (errno != 0)
			fprintf(stderr, "measure %s: %s\n", hname,
				strerror(errno));
	} else {
		if (errno != 0) {
			syslog(LOG_ERR, "measure %s: %m", hname);
		} else {
			syslog(LOG_ERR, "measure: %s did not respond", hname);
		}
		if (trace) {
			fprintf(fd,
				"measure: %s failed after %d trials\n",
				hname, trials);
			(void)fflush(fd);
		}
	}

	return(measure_status);
}





/*
 * round a number of milliseconds into a struct timeval
 */
void
mstotvround(struct timeval *res, long x)
{
#ifndef sgi
	if (x < 0)
		x = -((-x + 3)/5);
	else
		x = (x+3)/5;
	x *= 5;
#endif /* sgi */
	res->tv_sec = x/1000;
	res->tv_usec = (x-res->tv_sec*1000)*1000;
	if (res->tv_usec < 0) {
		res->tv_usec += 1000000;
		res->tv_sec--;
	}
}

void
timevaladd(struct timeval *tv1, struct timeval *tv2)
{
	tv1->tv_sec += tv2->tv_sec;
	tv1->tv_usec += tv2->tv_usec;
	if (tv1->tv_usec >= 1000000) {
		tv1->tv_sec++;
		tv1->tv_usec -= 1000000;
	}
	if (tv1->tv_usec < 0) {
		tv1->tv_sec--;
		tv1->tv_usec += 1000000;
	}
}

void
timevalsub(struct timeval *res, struct timeval *tv1, struct timeval *tv2)
{
	res->tv_sec = tv1->tv_sec - tv2->tv_sec;
	res->tv_usec = tv1->tv_usec - tv2->tv_usec;
	if (res->tv_usec >= 1000000) {
		res->tv_sec++;
		res->tv_usec -= 1000000;
	}
	if (res->tv_usec < 0) {
		res->tv_sec--;
		res->tv_usec += 1000000;
	}
}

