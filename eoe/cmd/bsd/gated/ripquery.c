/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/ripquery.c,v 1.3 1989/11/20 18:34:12 jleong Exp $";
#endif	not lint

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <netdb.h>
#include "routed.h"

#ifdef vax11c
#define perror(s) vmsperror(s)
#endif vax11c

#define WTIME	5		/* Time to wait for responses */
#define STIME	1		/* Time to wait between packets */

int	s;
char	packet[MAXPACKETSIZE];
int	cmds_request[] = {RIPCMD_REQUEST, RIPCMD_POLL, 0};
int	cmds_poll[] = {RIPCMD_POLL, RIPCMD_REQUEST, 0};
int	*cmds = cmds_poll;
int	nflag;
char	*version = "$Revision: 1.3 $";

extern int errno;
extern char *optarg;
extern int optind, opterr;
#ifndef sgi
extern char *inet_ntoa();
#endif /* sgi */

main(argc, argv)
	int argc;
	char *argv[];
{
	int c, cc, bits, errflg = 0, *cmd;
	struct sockaddr from;
	int fromlen = sizeof(from);
	static struct timeval *wait_time,
		long_time = { WTIME, 0 },
		short_time = { STIME, 0 };

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket");
#ifdef vax11c
		exit(0x10000002);
#else  vax11c
		exit(2);
#endif vax11c
	}

	while( (c = getopt(argc, argv, "nprvw:")) != EOF) {
		switch (c) {
		case 'n':
			nflag++;
			break;
		case 'p':
#ifdef sgi
			cmds = cmds_poll;
#else
			cmd = cmds_poll;
#endif
			break;
		case 'r':
#ifdef sgi
			cmds = cmds_request;
#else
			cmd = cmds_request;
#endif
			break;
		case 'v':
			fprintf(stderr, "RIPQuery %s\n", version);
			break;
		case 'w':
			long_time.tv_sec = atoi(optarg);
			fprintf(stderr, "Wait time set to %d\n", long_time.tv_sec);
			break;
		case '?':
			errflg++;
			break;
		}
	}

	if (errflg || (optind >= argc) ) {
		printf("usage: ripquery [ -n ] [ -p ] [ -r ] [ -v ] [ -w time] hosts...\n");
		exit(1);
	}

	setnetent(1);

	for (; optind < argc; optind++) {
		cmd = cmds;
retry:
		query(argv[optind], *cmd);
		bits = 1 << s;
		wait_time = &long_time;
		for (;;) {
#ifndef vax11c
			cc = select(s+1, (struct fd_set *) &bits, (struct fd_set *) 0, (struct fd_set *) 0, wait_time);
#else	vax11c
			cc = Socket_Ready(s, wait_time->tv_sec);
#endif	vax11c
			if (cc == 0) {
				if ( wait_time == &short_time ) {
					break;
				}
				if (*(++cmd)) {
					goto retry;
				} else {
					break;
				}
			} else if ( cc < 0 ) {
				perror("select");
				(void) close(s);
				exit(1);
			} else {
				wait_time = &short_time;
				cc = recvfrom(s, packet, sizeof (packet), 0, &from, &fromlen);
				if (cc <= 0) {
					if (cc < 0) {
						if (errno == EINTR) {
							continue;
						}
						perror("recvfrom");
						(void) close(s);
						exit(1);
					}
					continue;
				}
				rip_input((struct sockaddr_in *) &from, cc);
			}
		}
	}

	endnetent();
#ifdef vax11c
	exit(1);
#endif vax11c
}

query(host, cmd)
	char *host;
	int cmd;
{
	struct sockaddr_in router;
	register struct rip *msg = (struct rip *)packet;
	struct hostent *hp;
	struct servent *sp;

	bzero((char *)&router, sizeof (router));
	hp = gethostbyname(host);
	if (hp == 0) {
		router.sin_addr.s_addr = inet_addr(host);
		if (router.sin_addr.s_addr == (u_long)-1) {
			printf("%s: unknown\n", host);
			exit(1);
		}
	} else {
		bcopy(hp->h_addr, (char *)&router.sin_addr, hp->h_length);
	}
	sp = getservbyname("router", "udp");
	if (sp == NULL) {
		fprintf(stderr,"No service for router available\n");
		exit(1);
	}
	router.sin_family = AF_INET;
	router.sin_port = sp->s_port;
	msg->rip_cmd = cmd;
	msg->rip_vers = RIPVERSION;
	msg->rip_nets[0].rip_dst.sa_family = htons(AF_UNSPEC);
	msg->rip_nets[0].rip_metric = htonl((u_long)HOPCNT_INFINITY);
	if (sendto(s, packet, sizeof (struct rip), 0, (struct sockaddr *)&router, sizeof(router)) < 0)
		perror(host);
}

/*
 * Handle an incoming routing packet.
 */
rip_input(from, size)
	struct sockaddr_in *from;
	int size;
{
	register struct rip *msg = (struct rip *)packet;
	struct netinfo *n;
	char *name;
	long lna, net, subnet;
	struct hostent *hp;
	struct netent *np;

	if (msg->rip_cmd != RIPCMD_RESPONSE)
		return;
	hp = gethostbyaddr((char *)&from->sin_addr, sizeof (struct in_addr), AF_INET);
	name = hp == 0 ? "???" : hp->h_name;
	printf("from %s(%s):\n", name, inet_ntoa(from->sin_addr));
	size -= sizeof (int);
	n = msg->rip_nets;
	while (size > 0) {
		register struct sockaddr_in *sin;
		int i;

		if (size < sizeof (struct netinfo))
			break;
		if (msg->rip_vers) {
			n->rip_dst.sa_family = ntohs(n->rip_dst.sa_family);
			n->rip_metric = ntohl((u_long)n->rip_metric);
		}
		sin = (struct sockaddr_in *)&n->rip_dst;
		if (sin->sin_port) {
		    printf("**Non-zero port (%d) **",
			       sin->sin_port & 0xFFFF);
		}
		for (i=6;i<13;i++)
		  if (n->rip_dst.sa_data[i]) {
		    printf("sockaddr = ");
		    for (i=0;i<14;i++)
		      printf("%d ",n->rip_dst.sa_data[i] & 0xFF);
		    break;
		  }
		net = inet_netof(sin->sin_addr);
		subnet = inet_subnetof(sin->sin_addr);
		lna = inet_lnaof(sin->sin_addr);
		name = "???";
		if (!nflag) {
			if (sin->sin_addr.s_addr == 0)
				name = "default";
			else if (lna == INADDR_ANY) {
				np = getnetbyaddr(net, AF_INET);
				if (np)
					name = np->n_name;
				else if (net == 0)
					name = "default";
			} else if ((lna & 0xff) == 0 &&
			    (np = getnetbyaddr(subnet, AF_INET))) {
#ifdef sgi
				struct in_addr subnaddr;
#else /* sgi */
				struct in_addr subnaddr, inet_makeaddr();
#endif /* sgi */

				subnaddr = inet_makeaddr((int) subnet, (int) INADDR_ANY);
				if (bcmp((char *) &sin->sin_addr, (char *) &subnaddr, sizeof(subnaddr)) == 0)
					name = np->n_name;
				else
					goto host;
			} else {
	host:
				hp = gethostbyaddr((char *)&sin->sin_addr, sizeof (struct in_addr), AF_INET);
				if (hp)
					name = hp->h_name;
			}
			printf("\t%s(%s), metric %d\n", name,
				inet_ntoa(sin->sin_addr), n->rip_metric);
		} else {
			printf("\t%s, metric %d\n",
				inet_ntoa(sin->sin_addr), n->rip_metric);
		}
		size -= sizeof (struct netinfo), n++;
	}
}

/*
 * Return the possible subnetwork number from an internet address.
 * SHOULD FIND OUT WHETHER THIS IS A LOCAL NETWORK BEFORE LOOKING
 * INSIDE OF THE HOST PART.  We can only believe this if we have other
 * information (e.g., we can find a name for this number).
 */
inet_subnetof(in)
	struct in_addr in;
{
	register u_long i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return ((i & IN_CLASSB_NET) >> IN_CLASSB_NSHIFT);
	else if (IN_CLASSB(i))
		return ((i & IN_CLASSC_NET) >> IN_CLASSC_NSHIFT);
	else
		return ((i & 0xffffffc0) >> 28);
}


#ifdef	vax11c
/*
 *	See if a socket is ready for reading (waiting "n" seconds)
 */
static int Socket_Ready(Socket,Wait_Time)
{
#include <vms/iodef.h>
#define EFN_1 20
#define EFN_2 21
	int Status;
	int Timeout_Delta[2];
	int Dummy;
	unsigned short int IOSB[4];

	/*
	 *	Check for data (using MSG_PEEK)
	 */
	Status = SYS$QIO(EFN_1,
			 Socket,
			 IO$_READVBLK,
			 IOSB,
			 0,
			 0,
			 &Dummy,
			 sizeof(Dummy),
			 MSG_PEEK,
			 0,
			 0,
			 0);
	/*
	 *	Check for completion
	 */
	if (IOSB[0] != 0) return(1);
	/*
	 *	Setup timer
	 */
	if (Wait_Time) {
		Timeout_Delta[0] = -(Wait_Time * 10000000);
		Timeout_Delta[1] = -1;
		SYS$SETIMR(EFN_2,Timeout_Delta,0,Socket_Ready);
		SYS$WFLOR(EFN_1, (1<<EFN_1)|(1<<EFN_2));
		if (IOSB[0] != 0) {
			SYS$CANTIM(Socket_Ready,0);
			return(1);
		}
	}
	/*
	 *	Last chance
	 */
	if (IOSB[0] == 0) {
		/*
		 *	Lose:
		 */
		SYS$CANCEL(Socket);
		return(0);
	}
	return(1);
}
#endif	vax11c
