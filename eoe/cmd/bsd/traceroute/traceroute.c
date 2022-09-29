#ifndef lint
static char *rcsid =
	"@(#)Header: traceroute.c,v 1.17 89/02/28 21:01:13 van Exp $ (LBL)";
#endif

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 *
 * Attempt to trace the route an ip packet would follow to some
 * internet host.  We find out intermediate hops by launching probe
 * packets with a small ttl (time to live) then listening for an
 * icmp "time exceeded" reply from a gateway.  We start our probes
 * with a ttl of one and increase by one until we get an icmp "port
 * unreachable" (which means we got to "host") or hit a max (which
 * defaults to 30 hops & can be changed with the -m flag).  Three
 * probes (change with -q flag) are sent at each ttl setting and a
 * line is printed showing the ttl, address of the gateway and
 * round trip time of each probe.  If the probe answers come from
 * different gateways, the address of each responding system will
 * be printed.  If there is no response within a 5 sec. timeout
 * interval (changed with the -w flag), a "*" is printed for that
 * probe.  If the -l flag is used the ttl from the icmp reply will
 * be printed, otherwise it will be printed only if it has an
 * unexpected value.
 *
 * Probe packets are UDP format.  We don't want the destination
 * host to process them so the destination port is set to an
 * unlikely value (if some clod on the destination is using that
 * value, it can be changed with the -p flag).
 *
 * A sample use might be:
 *
 *     [yak 71]% traceroute nis.nsf.net.
 *     traceroute to nis.nsf.net (35.1.1.48), 30 hops max, 56 byte packet
 *      1  helios.ee.lbl.gov (128.3.112.1)  19 ms  19 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  39 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  40 ms  59 ms  59 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  59 ms
 *      8  129.140.70.13 (129.140.70.13)  99 ms  99 ms  80 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  239 ms  319 ms
 *     10  129.140.81.7 (129.140.81.7)  220 ms  199 ms  199 ms
 *     11  nic.merit.edu (35.1.1.48)  239 ms  239 ms  239 ms
 *
 * Note that lines 2 & 3 are the same.  This is due to a buggy
 * kernel on the 2nd hop system -- lbl-csam.arpa -- that forwards
 * packets with a zero ttl.
 *
 * A more interesting example is:
 *
 *     [yak 72]% traceroute allspice.lcs.mit.edu.
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 30 hops max
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  19 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  19 ms  39 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  20 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  59 ms  119 ms  39 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  39 ms
 *      8  129.140.70.13 (129.140.70.13)  80 ms  79 ms  99 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  139 ms  159 ms
 *     10  129.140.81.7 (129.140.81.7)  199 ms  180 ms  300 ms
 *     11  129.140.72.17 (129.140.72.17)  300 ms  239 ms  239 ms
 *     12  * * *
 *     13  128.121.54.72 (128.121.54.72)  259 ms  499 ms  279 ms
 *     14  * * *
 *     15  * * *
 *     16  * * *
 *     17  * * *
 *     18  ALLSPICE.LCS.MIT.EDU (18.26.0.115)  339 ms  279 ms  279 ms
 *
 * (I start to see why I'm having so much trouble with mail to
 * MIT.)  Note that the gateways 12, 14, 15, 16 & 17 hops away
 * either don't send ICMP "time exceeded" messages or send them
 * with a ttl too small to reach us.  14 - 17 are running the
 * MIT C Gateway code that doesn't send "time exceeded"s.  God
 * only knows what's going on with 12.
 *
 * The silent gateway 12 in the above may be the result of a bug in
 * the 4.[23]BSD network code (and its derivatives):  4.x (x <= 3)
 * sends an unreachable message using whatever ttl remains in the
 * original datagram.  Since, for gateways, the remaining ttl is
 * zero, the icmp "time exceeded" is guaranteed to not make it back
 * to us.  The behavior of this bug is slightly more interesting
 * when it appears on the destination system:
 *
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  39 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  19 ms
 *      5  ccn-nerif35.Berkeley.EDU (128.32.168.35)  39 ms  39 ms  39 ms
 *      6  csgw.Berkeley.EDU (128.32.133.254)  39 ms  59 ms  39 ms
 *      7  * * *
 *      8  * * *
 *      9  * * *
 *     10  * * *
 *     11  * * *
 *     12  * * *
 *     13  rip.Berkeley.EDU (128.32.131.22)  59 ms !  39 ms !  39 ms !
 *
 * Notice that there are 12 "gateways" (13 is the final
 * destination) and exactly the last half of them are "missing".
 * What's really happening is that rip (a Sun-3 running Sun OS3.5)
 * is using the ttl from our arriving datagram as the ttl in its
 * icmp reply.  So, the reply will time out on the return path
 * (with no notice sent to anyone since icmp's aren't sent for
 * icmp's) until we probe with a ttl that's at least twice the path
 * length.  I.e., rip is really only 7 hops away.  A reply that
 * returns with a ttl of 1 is a clue this problem exists.
 * Traceroute prints a "!" after the time if the ttl is <= 1.
 * Since vendors ship a lot of obsolete (DEC's Ultrix, Sun 3.x) or
 * non-standard (HPUX) software, expect to see this problem
 * frequently and/or take care picking the target host of your
 * probes.
 *
 * Other possible annotations after the time are !H, !N, !P (got a host,
 * network or protocol unreachable, respectively), !S or !F (source
 * route failed or fragmentation needed -- neither of these should
 * ever occur and the associated gateway is busted if you see one).  If
 * almost all the probes result in some kind of unreachable, traceroute
 * will give up and exit.
 *
 * Still other annotations are (ttl="n") and (ttl="n"!), where "n" is the
 * ttl from the icmp reply packet we received.  The first form will be
 * printed for every packet if you use the -l option.  The second form
 * will be printed (with or without -l) if this ttl is an unexpected value.
 * We expect that the return path from the n'th hop will contain n hops,
 * otherwise we know the reply packet is coming back via a different path than
 * it went out on.  Unfortunately, not everyone uses the same starting ttl
 * value for icmp messages.  The common ones used by routers are 29
 * (Proteon 8.1 and lower software), 59 (Proteon 8.2), 255 (cisco, BSD
 * since 4.3 tahoe).  30 and 60 are also often used by hosts, and probably
 * by some routers, because they were the BSD TCP ttl values.  This makes
 * some "off by one" return paths hard to detect, you might try removing
 * OLD_BSD_TCP and NEW_BSD_TCP from the case statement if this annoys you.
 * If you are using the lsrr (-g) code with the -l code you will see many
 * bogus "!".  
 *
 * With the -l option you will see the TTL value of the ICMP msg that
 * came back to you, as in:
 *
 * nettlerash> traceroute -l rip.berkeley.edu
 * traceroute to rip.berkeley.edu (128.32.131.22), 30 hops max, 40 byte packets
 *  1  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  0 ms (ttl=59)  10 ms (ttl=59)
 *  2  ccn-nerif35.Berkeley.EDU (128.32.168.35)  10 ms (ttl=58)  10 ms (ttl=58)
 *  3  csgw.Berkeley.EDU (128.32.133.254)  10 ms (ttl=253)  20 ms (ttl=253)
 *  4  rip.Berkeley.EDU (128.32.131.22)  30 ms (ttl=252)  30 ms (ttl=252)
 *
 * This shows that from nettlerash the route goes through two proteons (ttl=
 * 59, then 58), followed by a router using max_ttl to rip which also uses
 * max_ttl.  (-l and printing ttl stuff added by cliff@berkeley.edu.)
 *
 * Notes
 * -----
 * This program must be run by root or be setuid.  (I suggest that
 * you *don't* make it setuid -- casual use could result in a lot
 * of unnecessary traffic on our poor, congested nets.)
 *
 * This program requires a kernel mod that does not appear in any
 * system available from Berkeley:  A raw ip socket using proto
 * IPPROTO_RAW must interpret the data sent as an ip datagram (as
 * opposed to data to be wrapped in a ip datagram).  See the README
 * file that came with the source to this program for a description
 * of the mods I made to /sys/netinet/raw_ip.c.  Your mileage may
 * vary.  But, again, ANY 4.x (x < 4) BSD KERNEL WILL HAVE TO BE
 * MODIFIED TO RUN THIS PROGRAM.
 *
 * The udp port usage may appear bizarre (well, ok, it is bizarre).
 * The problem is that an icmp message only contains 8 bytes of
 * data from the original datagram.  8 bytes is the size of a udp
 * header so, if we want to associate replies with the original
 * datagram, the necessary information must be encoded into the
 * udp header (the ip id could be used but there's no way to
 * interlock with the kernel's assignment of ip id's and, anyway,
 * it would have taken a lot more kernel hacking to allow this
 * code to set the ip id).  So, to allow two or more users to
 * use traceroute simultaneously, we use this task's pid as the
 * source port (the high bit is set to move the port number out
 * of the "likely" range).  To keep track of which probe is being
 * replied to (so times and/or hop counts don't get confused by a
 * reply that was delayed in transit), we increment the destination
 * port number before each probe.
 *
 * Don't use this as a coding example.  I was trying to find a
 * routing problem and this code sort-of popped out after 48 hours
 * without sleep.  I was amazed it ever compiled, much less ran.
 *
 * I stole the idea for this program from Steve Deering.  Since
 * the first release, I've learned that had I attended the right
 * IETF working group meetings, I also could have stolen it from Guy
 * Almes or Matt Mathis.  I don't know (or care) who came up with
 * the idea first.  I envy the originators' perspicacity and I'm
 * glad they didn't keep the idea a secret.
 *
 * Tim Seaver, Ken Adelman and C. Philip Wood provided bug fixes and/or
 * enhancements to the original distribution.
 *
 * I've hacked up a round-trip-route version of this that works by
 * sending a loose-source-routed udp datagram through the destination
 * back to yourself.  Unfortunately, SO many gateways botch source
 * routing, the thing is almost worthless.  Maybe one day...
 *
 *  -- Van Jacobson (van@helios.ee.lbl.gov)
 *     Tue Dec 20 03:50:13 PST 1988
 *
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <sys/time.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netdb.h>
#include <ctype.h>

#ifdef sgi
/* XXX use old-style because this program doesn't deal with IP_HDRINCL & 
 * IP options correctly. */
#undef IP_HDRINCL	
#endif

#define	MAXPACKET	65535	/* max ip packet size */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif

#ifndef FD_SET
#define NFDBITS         (8*sizeof(fd_set))
#define FD_SETSIZE      NFDBITS
#define FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)      bzero((char *)(p), sizeof(*(p)))
#endif

#define Fprintf (void)fprintf
#define Sprintf (void)sprintf
#define Printf (void)printf

#define	OLD_PROTEON_TTL	30
#define	PROTEON_TTL	60
#define	MAX_START_TTL	256
#define	OLD_BSD_TCP	31
#define	NEW_BSD_TCP	61

extern	int errno;
extern  char *malloc();
extern  char *inet_ntoa();
extern  u_long inet_addr();

/*
 * format of a (udp) probe packet.
 */
struct opacket {
	struct ip ip;
	struct udphdr udp;
	u_char seq;		/* sequence number of this packet */
	u_char ttl;		/* ttl packet left with */
	struct timeval tv;	/* time packet left */
};

u_char	packet[512];		/* last inbound (icmp) packet */
struct opacket	*outpacket;	/* last output (udp) packet */
char *inetname();

int s;				/* receive (icmp) socket file descriptor */
int sndsock;			/* send (udp) socket file descriptor */
struct timezone tz;		/* leftover */

struct sockaddr whereto;	/* Who to try to reach */
int datalen;			/* How much data */

char *source = 0;
char *hostname;
char hnamebuf[MAXHOSTNAMELEN];

int nprobes = 3;
int max_ttl = 0;
int min_ttl = 1;
u_short ident;
u_short port = 32768+666;	/* start udp dest port # for probe packets */

int options;			/* socket options */
int verbose;
int waittime = 5;		/* time to wait for response (in seconds) */
int nflag;			/* print addresses numerically */
int ttl_flag = 0;		/* print all ttls received */

char usage[]="Usage: traceroute [-dlnrv] [-w wait] [-m max_ttl] [-M min_ttl]\n"
"      [-p port#] [-q nqueries] [-t tos] [-s src_addr] [-g gateway]\n"
"      host [data size]\n";


main(argc, argv)
	char *argv[];
{
	struct sockaddr_in from;
	struct sockaddr_in *to = (struct sockaddr_in *) &whereto;
	int on = 1;
	struct protoent *pe;
	int ttl, probe, i;
	int seq = 0;
	int tos = 0;
	struct hostent *hp;
	int lsrr = 0, c;
	u_long gw;
	u_char optlist[MAX_IPOPTLEN], *oix;
	char *p;

	oix = optlist;
	bzero(optlist, sizeof(optlist));

	while ((c = getopt(argc, argv, "dlnrvw:m:M:p:q:t:s:g:")) != EOF) {
		switch (c) {
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'g':
			if ((lsrr+1) >= ((MAX_IPOPTLEN-IPOPT_MINOFF)/sizeof(u_long))) {
			  Fprintf(stderr,"No more than %d gateways\n",
				  ((MAX_IPOPTLEN-IPOPT_MINOFF)/sizeof(u_long))-1);
			  exit(1);
			}
			if (lsrr == 0) {
			  *oix++ = IPOPT_LSRR;
			  *oix++;	/* Fill in total length later */
			  *oix++ = IPOPT_MINOFF; /* Pointer to LSRR addresses */
			}
			lsrr++;
			if (isdigit(*optarg)) {
			  gw = inet_addr(optarg);
			  if (gw) {
			    bcopy(&gw, oix, sizeof(u_long));
			  } else {
			    Fprintf(stderr, "Unknown host %s\n", optarg);
			    exit(1);
			  }
			} else {
			  hp = gethostbyname(optarg);
			  if (hp) {
			    bcopy(hp->h_addr, oix, sizeof(u_long));
			  } else {
			    Fprintf(stderr, "Unknown host %s\n", optarg);
			    exit(1);
			  }
			}
			oix += sizeof(u_long);
			break;
		case 'l':
			ttl_flag++;
			break;
		case 'm':
			max_ttl = strtoul(optarg,&p,0);
			if (*p != '\0') {
				Fprintf(stderr, "bad max TTL \"%s\"\n",optarg);
				exit(1);
			}
			if (max_ttl <= min_ttl) {
				Fprintf(stderr,
					"max TTL must be > min TTL=%d\n",
					min_ttl);
				exit(1);
			}
			break;
		case 'M':
			min_ttl = strtoul(optarg,&p,0);
			if (*p != '\0') {
				Fprintf(stderr, "bad min TTL \"%s\"\n",optarg);
				exit(1);
			}
			if (max_ttl <= min_ttl && max_ttl != 0) {
				Fprintf(stderr,
					"min TTL must be < max TTL=%d\n",
					max_ttl);
				exit(1);
			}
			break;
		case 'n':
			nflag++;
			break;
		case 'p':
			port = atoi(optarg);
			if (port < 1) {
				Fprintf(stderr, "port must be >0\n");
				exit(1);
			}
			break;
		case 'q':
			nprobes = strtoul(optarg,&p,0);
			if (nprobes < 1 || *p != '\0') {
				Fprintf(stderr, "nprobes must be >0\n");
				exit(1);
			}
			break;
		case 'r':
			options |= SO_DONTROUTE;
			break;
		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;
		case 't':
			tos = atoi(optarg);
			if (tos < 0 || tos > 255) {
				Fprintf(stderr, "tos must be 0 to 255\n");
				exit(1);
			}
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			waittime = atoi(optarg);
			if (waittime < 1) {
				Fprintf(stderr, "wait must be >=1 sec\n");
				exit(1);
			}
			break;
		default:
			Printf(usage);
			exit(1);
		}
	}
	if (optind >= argc)  {
		Printf(usage);
		exit(1);
	}
	if (max_ttl == 0) {
		max_ttl = MAX(min_ttl+1,30);
	} else if (max_ttl <= min_ttl) {
		Fprintf(stderr, "min TTL=%d must be < max TTL=%d\n",
			min_ttl,max_ttl);
		exit(1);
	}
	setlinebuf (stdout);

	(void) bzero((char *)&whereto, sizeof(struct sockaddr));
	to->sin_family = AF_INET;
	to->sin_addr.s_addr = inet_addr(argv[optind]);
	if (to->sin_addr.s_addr != -1) {
		(void) strcpy(hnamebuf, argv[optind]);
		hostname = hnamebuf;
	} else {
		hp = gethostbyname(argv[optind]);
		if (hp) {
			to->sin_family = hp->h_addrtype;
			bcopy(hp->h_addr, (caddr_t)&to->sin_addr, hp->h_length);
			hostname = hp->h_name;
		} else {
			Printf("%s: unknown host %s\n", argv[0], argv[optind]);
			exit(1);
		}
	}


	optind++;
	if (optind < argc)
		datalen = atoi(argv[optind]);
	if (datalen < 0 || datalen >= MAXPACKET - sizeof(struct opacket)) {
		Fprintf(stderr, "traceroute: packet size must be 0 <= s < %ld\n",
			MAXPACKET - sizeof(struct opacket));
		exit(1);
	}
	datalen += sizeof(struct opacket);
	outpacket = (struct opacket *)malloc((unsigned)datalen);
	if (! outpacket) {
		perror("traceroute: malloc");
		exit(1);
	}
	(void) bzero((char *)outpacket, datalen);
	outpacket->ip.ip_dst = to->sin_addr;
	outpacket->ip.ip_tos = tos;

	ident = (getpid() & 0xffff) | 0x8000;

	if ((pe = getprotobyname("icmp")) == NULL) {
		Fprintf(stderr, "icmp: unknown protocol\n");
		exit(10);
	}
	if ((s = socket(AF_INET, SOCK_RAW, pe->p_proto)) < 0) {
		perror("traceroute: icmp socket");
		exit(5);
	}
	if (options & SO_DEBUG)
		(void) setsockopt(s, SOL_SOCKET, SO_DEBUG,
				  (char *)&on, sizeof(on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(s, SOL_SOCKET, SO_DONTROUTE,
				  (char *)&on, sizeof(on));

	if ((sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("traceroute: raw socket");
		exit(5);
	}

	if (lsrr > 0) {
	  lsrr++;
#ifdef sgi
	  /* 
	   * XXX add 1 more octet to length to avoid misaligned ptr bug 
	   * in IRIX 3.3 kernel options handling.
	   * Assumes at least 1 NOP octet at the end.
	   */
#if IPOPT_MINOFF != 4
?!?! error -- fix optlen
#endif
	  optlist[IPOPT_OLEN]=IPOPT_MINOFF + (lsrr*sizeof(u_long));
#else
	  optlist[IPOPT_OLEN]=IPOPT_MINOFF-1+(lsrr*sizeof(u_long));
#endif
	  bcopy((caddr_t)&to->sin_addr, oix, sizeof(u_long));
	  oix += sizeof(u_long);
	  while ((oix - optlist)&3) oix++;		/* Pad to an even boundry */

	  if ((pe = getprotobyname("ip")) == NULL) {
	    perror("traceroute: unknown protocol ip\n");
	    exit(10);
	  }
	  if ((setsockopt(sndsock, pe->p_proto, IP_OPTIONS, optlist, oix-optlist)) < 0) {
	    perror("traceroute: lsrr options");
	    exit(5);
	  }
	}

#ifdef SO_SNDBUF
	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&datalen,
		       sizeof(datalen)) < 0) {
		perror("traceroute: SO_SNDBUF");
		exit(6);
	}
#endif /* SO_SNDBUF */
#ifdef IP_HDRINCL
	if (setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *)&on,
		       sizeof(on)) < 0) {
		perror("traceroute: IP_HDRINCL");
		exit(6);
	}
#endif /* IP_HDRINCL */
	if (options & SO_DEBUG)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG,
				  (char *)&on, sizeof(on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE,
				  (char *)&on, sizeof(on));

	if (source) {
		(void) bzero((char *)&from, sizeof(struct sockaddr));
		from.sin_family = AF_INET;
		from.sin_addr.s_addr = inet_addr(source);
		if (from.sin_addr.s_addr == -1) {
			Printf("traceroute: unknown host %s\n", source);
			exit(1);
		}
		outpacket->ip.ip_src = from.sin_addr;
#ifndef IP_HDRINCL
		if (bind(sndsock, (struct sockaddr *)&from, sizeof(from)) < 0) {
			perror ("traceroute: bind:");
			exit (1);
		}
#endif /* IP_HDRINCL */
	}

	Fprintf(stderr, "traceroute to %s (%s)", hostname,
		inet_ntoa(to->sin_addr));
	if (source)
		Fprintf(stderr, " from %s", source);
	Fprintf(stderr, ", %d hops max, %d byte packets\n", max_ttl, datalen);
	(void) fflush(stderr);

	for (ttl = min_ttl; ttl <= max_ttl; ++ttl) {
		u_long lastaddr = 0;
		int got_there = 0;
		int unreachable = 0;

		Printf("%2d ", ttl);
		for (probe = 0; probe < nprobes; ++probe) {
			int cc;
			char ttl_char;
			struct ip *ip = (struct ip *)packet;

			send_probe(++seq, ttl);
			while (cc = wait_for_reply(s, &from)) {
				if ((i = packet_ok(packet, cc, &from, seq))) {
					int dt = deltaT(&outpacket->tv);
					if (from.sin_addr.s_addr != lastaddr) {
						print(packet, cc, &from);
						lastaddr = from.sin_addr.s_addr;
					}
					Printf("  %d ms", dt);
	
					if (ttl_flag >= 0) {
						switch(ttl + ip->ip_ttl) {
#ifndef sgi
						case OLD_BSD_TCP:
						case NEW_BSD_TCP:
#endif
						case OLD_PROTEON_TTL:
						case PROTEON_TTL:
						case MAX_START_TTL:
							ttl_char = '\0';
							break;
						default:
							ttl_char = '!';
							break;
						}
					    if ( ((ttl_char == '!') &&
						  (lsrr <= 0)) ||
						 (ttl_flag > 0) )
	      				   Printf(" (ttl=%d%c)", ip->ip_ttl,
					   	    ttl_char );
					}

					switch(i - 1) {
					case ICMP_UNREACH_PORT:
#ifndef ARCHAIC
						if (ip->ip_ttl <= 1)
							Printf(" !");
#endif /* ARCHAIC */
						++got_there;
						break;
					case ICMP_UNREACH_NET:
						++unreachable;
						Printf(" !N");
						break;
					case ICMP_UNREACH_HOST:
						++unreachable;
						Printf(" !H");
						break;
					case ICMP_UNREACH_PROTOCOL:
						++got_there;
						Printf(" !P");
						break;
					case ICMP_UNREACH_NEEDFRAG:
						++unreachable;
						Printf(" !F");
						break;
					case ICMP_UNREACH_SRCFAIL:
						++unreachable;
						Printf(" !S");
						break;
					}
					break;
				}
			}
			if (cc == 0)
				Printf(" *");
			(void) fflush(stdout);
		}
		putchar('\n');
		if (got_there
		    || (unreachable > 0 && unreachable >= nprobes-1))
			exit(0);
	}
}

wait_for_reply(sock, from)
	int sock;
	struct sockaddr_in *from;
{
	fd_set fds;
	struct timeval wait;
	int cc = 0;
	int fromlen = sizeof (*from);

	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	/*
	 * Do not keep waiting just because we are receiving many ICMP
	 * error messages about unrelated traffic.
	 */
	gettimeofday(&wait,0);
	wait.tv_sec = waittime + outpacket->tv.tv_sec - wait.tv_sec;
	wait.tv_usec = outpacket->tv.tv_usec - wait.tv_usec;
	while (wait.tv_usec < 0) {
		wait.tv_usec += 1000000;
		wait.tv_sec--;
	}
	if (wait.tv_sec < -1)
		return(cc);

	if (select(sock+1, &fds, (fd_set *)0, (fd_set *)0, &wait) > 0)
		cc=recvfrom(s, (char *)packet, sizeof(packet), 0,
			    (struct sockaddr *)from, &fromlen);

	return(cc);
}


send_probe(seq, ttl)
{
	struct opacket *op = outpacket;
	struct ip *ip = &op->ip;
	struct udphdr *up = &op->udp;
	int i;

	ip->ip_off = 0;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_len = datalen;
	ip->ip_ttl = ttl;

	up->uh_sport = htons(ident);
	up->uh_dport = htons(port+seq);
	up->uh_ulen = htons((u_short)(datalen - sizeof(struct ip)));
	up->uh_sum = 0;

	op->seq = seq;
	op->ttl = ttl;
	(void) gettimeofday(&op->tv, &tz);

	i = sendto(sndsock, (char *)outpacket, datalen, 0, &whereto,
		   sizeof(struct sockaddr));
	if (i < 0 || i != datalen)  {
		if (i<0)
			perror("sendto");
		Printf("traceroute: wrote %s %d chars, ret=%d\n", hostname,
			datalen, i);
		(void) fflush(stdout);
	}
}


deltaT(tp)
	struct timeval *tp;
{
	struct timeval tv;

	(void) gettimeofday(&tv, &tz);
	tvsub(&tv, tp);
	return (tv.tv_sec*1000 + (tv.tv_usec + 500)/1000);
}


/*
 * Convert an ICMP "type" field to a printable string.
 */
char *
pr_type(t)
	u_char t;
{
	static char *ttab[] = {
	"Echo Reply",	"ICMP 1",	"ICMP 2",	"Dest Unreachable",
	"Source Quench", "Redirect",	"ICMP 6",	"ICMP 7",
	"Echo",		"ICMP 9",	"ICMP 10",	"Time Exceeded",
	"Param Problem", "Timestamp",	"Timestamp Reply", "Info Request",
	"Info Reply"
	};

	if(t > 16)
		return("OUT-OF-RANGE");

	return(ttab[t]);
}


packet_ok(buf, cc, from, seq)
	u_char *buf;
	int cc;
	struct sockaddr_in *from;
	int seq;
{
	register struct icmp *icp;
	u_char type, code;
	int hlen;
#ifndef ARCHAIC
	struct ip *ip;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			Printf("packet too short (%d bytes) from %s\n", cc,
				inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
#else
	icp = (struct icmp *)buf;
#endif /* ARCHAIC */
	type = icp->icmp_type; code = icp->icmp_code;
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH) {
		struct ip *hip;
		struct udphdr *up;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;
		up = (struct udphdr *)((u_char *)hip + hlen);
		if (hlen + 12 <= cc && hip->ip_p == IPPROTO_UDP &&
		    up->uh_sport == htons(ident) &&
		    up->uh_dport == htons(port+seq))
			return (type == ICMP_TIMXCEED? -1 : code+1);
	}
#ifndef ARCHAIC
	if (verbose) {
		int i;
		u_long *lp = (u_long *)&icp->icmp_ip;

		Printf("\n%d bytes from %s to %s", cc,
			inet_ntoa(from->sin_addr), inet_ntoa(ip->ip_dst));
		Printf(": icmp type %d (%s) code %d\n", type, pr_type(type),
		       icp->icmp_code);
		for (i = 4; i < cc ; i += sizeof(long))
			Printf("%2d: x%8.8lx\n", i, *lp++);
	}
#endif /* ARCHAIC */
	return(0);
}


print(buf, cc, from)
	u_char *buf;
	int cc;
	struct sockaddr_in *from;
{
	struct ip *ip;
	int hlen;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	cc -= hlen;

	if (nflag)
		Printf(" %s", inet_ntoa(from->sin_addr));
	else
		Printf(" %s (%s)", inetname(from->sin_addr),
		       inet_ntoa(from->sin_addr));

	if (verbose)
		Printf (" %d bytes to %s", cc, inet_ntoa (ip->ip_dst));
}


#ifdef notyet
/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */
in_cksum(addr, len)
u_short *addr;
int len;
{
	register int nleft = len;
	register u_short *w = addr;
	register u_short answer;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1)
		sum += *(u_char *)w;

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}
#endif /* notyet */

/*
 * Subtract 2 timeval structs:  out = out - in.
 * Out is assumed to be >= in.
 */
tvsub(out, in)
register struct timeval *out, *in;
{
	if ((out->tv_usec -= in->tv_usec) < 0)   {
		out->tv_sec--;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}


/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give 
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(in)
	struct in_addr in;
{
	register char *cp;
	static char line[50];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = index(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && in.s_addr != INADDR_ANY) {
		hp = gethostbyaddr((char *)&in, sizeof (in), AF_INET);
		if (hp) {
			if ((cp = index(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (cp)
		(void) strcpy(line, cp);
	else {
		in.s_addr = ntohl(in.s_addr);
#define C(x)	((x) & 0xff)
		Sprintf(line, "%lu.%lu.%lu.%lu", C(in.s_addr >> 24),
			C(in.s_addr >> 16), C(in.s_addr >> 8), C(in.s_addr));
	}
	return (line);
}
