/*
 *
 * Copyright (c) 1983, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char copyright[] =
"@(#) Copyright (c) 1983, 1989, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)route.c	8.3 (Berkeley) 3/19/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#ifndef sgi
#include <netns/ns.h>
#include <netiso/iso.h>
#include <netccitt/x25.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#ifdef sgi
#include <strings.h>
#include <cap_net.h>
#else
#include <string.h>
#endif
#include <paths.h>

struct keytab {
	char	*kt_cp;
	int	kt_i;
} keywords[] = {
#include "keywords.h"
	{0, 0}
};

union	sockunion {
	struct	sockaddr sa;
	struct	sockaddr_in sin;
#ifndef sgi
	struct	sockaddr_ns sns;
	struct	sockaddr_iso siso;
#endif
	struct	sockaddr_dl sdl;
#ifndef sgi
	struct	sockaddr_x25 sx25;
#endif
} so_dst, so_gate, so_mask, so_genmask, so_ifa, so_ifp;

typedef union sockunion *sup;
int	pid, rtm_addrs, uid;
int	s;
int	forcehost, forcenet, doflush, flushall, nflag, af, qflag, tflag;
int	keyword();
int	iflag, verbose, aflen = sizeof (struct sockaddr_in);
int	locking, lockrest, debugonly;
struct	rt_metrics rt_metrics;
u_long  rtm_inits;
int	exit_val = 0;
struct	in_addr inet_makeaddr();
char	*routename(struct sockaddr *, int), *netname(struct sockaddr *, int);
void	flushroutes(), newroute(), monitor(), sockaddr(), sodump(), bprintf();
void	print_getmsg();
void	print_rtmsg(struct rt_msghdr *, int);
void	pmsg_common(struct rt_msghdr *), mask_addr();
static void pmsg_addrs(char *, int);
int	getaddr(), rtmsg(), x25_makemask();
extern	char *inet_ntoa(), *iso_ntoa(), *link_ntoa();

static void
usage(cp)
	char *cp;
{
	if (cp)
		(void) fprintf(stderr, "route: botched keyword: %s\n", cp);
	(void) fprintf(stderr,
	    "usage: route [ -nqvfF ] cmd [[ -<qualifers> ] args ]\n");
	exit(1);
	/* NOTREACHED */
}

static void
quit(char *s)
{
	int sverrno = errno;

	(void) fprintf(stderr, "route: ");
	if (s)
		(void) fprintf(stderr, "%s: ", s);
	(void) fprintf(stderr, "%s\n", strerror(sverrno));
	exit(1);
}

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;

	if (argc < 2)
		usage((char *)NULL);

	while ((ch = getopt(argc, argv, "nqdtvfF")) != EOF)
		switch(ch) {
		case 'n':
			nflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'd':
			debugonly = 1;
			break;
		case 'f':
			doflush = 1;
			break;
		case 'F':
			doflush = 1;
			flushall = 1;
			nflag = 1;	/* no names without localhost */
			break;
		case '?':
		default:
			usage((char *)NULL);
		}
	argc -= optind;
	argv += optind;

	pid = getpid();
	uid = getuid();
	if (tflag)
		s = open("/dev/null", O_WRONLY, 0);
	else {
		s = cap_socket(PF_ROUTE, SOCK_RAW, 0);
	}
	if (s < 0)
		quit("socket");
	if (*argv)
		switch (keyword(*argv)) {
		case K_GET:
			uid = 0;
			/* FALLTHROUGH */

		case K_CHANGE:
		case K_ADD:
		case K_DELETE:
			newroute(argc, argv);
			exit(exit_val);
			/* NOTREACHED */

		case K_MONITOR:
			monitor();
			/* NOTREACHED */

		case K_FLUSH:
			flushroutes(argc, argv);
			exit(exit_val);
			/* NOTREACHED */
		}
	if (doflush) {
		flushroutes(argc, argv);
		exit(exit_val);
	}
	usage(*argv);
	/* NOTREACHED */
}

/*
 * Purge all entries in the routing tables not
 * associated with network interfaces.
 */
void
flushroutes(argc, argv)
	int argc;
	char *argv[];
{
	size_t needed;
	int mib[6], rlen, seqno;
	char *buf, *next, *lim;
	register struct rt_msghdr *rtm;

	if (uid) {
		errno = EACCES;
		quit("must be root to alter routing table");
	}
	shutdown(s, 0); /* Don't want to read back our messages */
	if (argc > 1) {
		argv++;
		if (argc == 2 && **argv == '-')
		    switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
#ifndef sgi
			case K_XNS:
				af = AF_NS;
				break;
#endif
			case K_LINK:
				af = AF_LINK;
				break;
#ifndef sgi
			case K_ISO:
			case K_OSI:
				af = AF_ISO;
				break;
			case K_X25:
				af = AF_CCITT;
#endif
			default:
				goto bad;
		} else
bad:			usage(*argv);
	}
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, 0, &needed, 0, 0) < 0)
		quit("route-sysctl-estimate");
	if ((buf = malloc(needed)) == 0)
		quit("malloc");
	if (sysctl(mib, 6, buf, &needed, 0, 0) < 0)
		quit("actual retrieval of routing table");
	lim = buf + needed;
	if (verbose)
		(void) printf("Examining routing table from sysctl\n");
	seqno = 0;		/* ??? */
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
#if RTAX_DST != 0
		? this assumes RTAX_DST == 0;
#endif
		rtm = (struct rt_msghdr *)next;
		if (verbose)
			print_rtmsg(rtm, 0);
		if ((rtm->rtm_flags & RTF_GATEWAY) == 0 && !flushall)
			continue;
		if (af) {
			struct sockaddr *sa = (struct sockaddr *)(rtm + 1);

			if (sa->sa_family != af)
				continue;
		}
		if (debugonly)
			continue;
		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno;
		rlen = write(s, next, rtm->rtm_msglen);
		if (rlen < (int)rtm->rtm_msglen) {
			(void) fprintf(stderr,
			    "route: write to routing socket: %s\n",
			    strerror(errno));
			(void) printf("got only %d for rlen\n", rlen);
			exit_val = 2;
			break;
		}
		seqno++;
		if (qflag)
			continue;
		if (verbose)
			print_rtmsg(rtm, 0);
		else {
			struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
			(void) printf("%-20.20s ", rtm->rtm_flags & RTF_HOST ?
			    routename(sa,20) : netname(sa,20));
#ifdef _HAVE_SA_LEN
			sa = (struct sockaddr *)(sa->sa_len + (char *)sa);
#else
			sa = (struct sockaddr *)(_FAKE_SA_LEN_DST(sa)
						 + (char *)sa);
#endif
			(void) printf("%-20.20s done\n", routename(sa,20));
		}
	}
}

char *
routename(struct sockaddr *sa, int width)
{
	register char *cp;
	static char line[50];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;
	char *ns_print();

	if (first) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = index(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}

#ifdef _HAVE_SA_LEN
	if (sa->sa_len == 0)
		strcpy(line, "default");
#else
	if (sa->sa_family == 0)
		strcpy(line, "default");
#endif
	else switch (sa->sa_family) {

	case AF_INET:
	    {	struct in_addr in;
		in = ((struct sockaddr_in *)sa)->sin_addr;

		cp = 0;
#ifdef _HAVE_SA_LEN
		if (in.s_addr == INADDR_ANY || sa->sa_len < 4)
			cp = "default";
#else
		if (in.s_addr == INADDR_ANY)
			cp = "default";
#endif
		if (cp == 0 && !nflag) {
			hp = gethostbyaddr((char *)&in, sizeof (struct in_addr),
				AF_INET);
			if (hp) {
				if ((cp = index(hp->h_name, '.')) &&
				    !strcmp(cp + 1, domain))
					*cp = 0;
				cp = hp->h_name;
			}
		}
		if (cp)
			strcpy(line, cp);
		else {
#define C(x)	((x) & 0xff)
			in.s_addr = ntohl(in.s_addr);
			(void) sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			   C(in.s_addr >> 16), C(in.s_addr >> 8), C(in.s_addr));
		}
		break;
	    }

#ifndef sgi
	case AF_NS:
		return (ns_print((struct sockaddr_ns *)sa));
#endif

	case AF_LINK:
		return (link_ntoa((struct sockaddr_dl *)sa));

#ifndef sgi
	case AF_ISO:
		(void) sprintf(line, "iso %s",
		    iso_ntoa(&((struct sockaddr_iso *)sa)->siso_addr));
		break;
#endif

	default:
	    {	u_short *s = (u_short *)sa;
#ifdef _HAVE_SA_LEN
		u_short *slim = s+(sa->sa_len+1)/2;
		char *cp = line + sprintf(line, "(%d)", sa->sa_family);
#else
		u_short *slim = s+(MIN(_FAKE_SA_LEN_SRC(sa),sizeof(*sa))+1)/2;
		char *cp = line + sprintf(line, "(%d)", _FAKE_SA_FAMILY(sa));
#endif

		while (++s < slim) /* start with sa->sa_data */
			cp += sprintf(cp, "-%04x", *s);
		break;
	    }
	}
#ifdef sgi
	/* Trim the string.
	 *	Trim back a period if possible, but add '~' to indicate
	 *	truncation if not.  Try to leave the period to inidicate
	 *	truncation.
	 */
	if (width > 0
	    && strlen(line) > width) {
		char *period;
		line[width+1] = '\0';
		period = strrchr(line,'.');
		line[width--] = '\0';
		if (!period)
			line[width] = '~';
		else
			period[1] = '\0';
	}
#endif
	return (line);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
#ifdef sgi
char *
netname(struct sockaddr *sa, int width)
#else
char *
netname(sa)
	struct sockaddr *sa;
#endif
{
	char *cp = 0;
	static char line[50];
	struct netent *np = 0;
	u_long net, mask;
	register u_long i;
	int subnetshift;
	char *ns_print();

	switch (sa->sa_family) {

	case AF_INET:
	    {	struct in_addr in;
		in = ((struct sockaddr_in *)sa)->sin_addr;

		i = in.s_addr = ntohl(in.s_addr);
		if (in.s_addr == 0)
			cp = "default";
		else if (!nflag) {
			if (IN_CLASSA(i)) {
				mask = IN_CLASSA_NET;
				subnetshift = 8;
			} else if (IN_CLASSB(i)) {
				mask = IN_CLASSB_NET;
				subnetshift = 8;
			} else {
				mask = IN_CLASSC_NET;
				subnetshift = 4;
			}
			/*
			 * If there are more bits than the standard mask
			 * would suggest, subnets must be in use.
			 * Guess at the subnet mask, assuming reasonable
			 * width subnet fields.
			 */
			while (in.s_addr &~ mask)
				mask = (long)mask >> subnetshift;
			net = in.s_addr & mask;
			while ((mask & 1) == 0)
				mask >>= 1, net >>= 1;
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp)
			strcpy(line, cp);
		else if ((in.s_addr & 0xffffff) == 0)
			(void) sprintf(line, "%u", C(in.s_addr >> 24));
		else if ((in.s_addr & 0xffff) == 0)
			(void) sprintf(line, "%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16));
		else if ((in.s_addr & 0xff) == 0)
			(void) sprintf(line, "%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8));
		else
			(void) sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8),
			    C(in.s_addr));
		break;
	    }

#ifndef sgi
	case AF_NS:
		return (ns_print((struct sockaddr_ns *)sa));
#endif

	case AF_LINK:
		return (link_ntoa((struct sockaddr_dl *)sa));

#ifndef sgi
	case AF_ISO:
		(void) sprintf(line, "iso %s",
		    iso_ntoa(&((struct sockaddr_iso *)sa)->siso_addr));
		break;
#endif

	default:
	    {	u_short *s = (u_short *)sa->sa_data;
#ifdef _HAVE_SA_LEN
		u_short *slim = s + ((sa->sa_len + 1)>>1);
#else
		u_short *slim = s + ((sizeof(*sa) + 1)>>1);
#endif
		char *cp = line + sprintf(line, "af %d:", sa->sa_family);

		while (s < slim)
			cp += sprintf(cp, " %x", *s++);
		break;
	    }
	}
#ifdef sgi
	/* Trim the string.
	 *	Trim back a period if possible, but add '~' to indicate
	 *	truncation if not.  Try to leave the period to inidicate
	 *	truncation.
	 */
	if (width > 0
	    && strlen(line) > width) {
		char *period;
		line[width+1] = '\0';
		period = strrchr(line,'.');
		line[width--] = '\0';
		if (!period)
			line[width] = '~';
		else
			period[1] = '\0';
	}
#endif
	return (line);
}

void
set_metric(value, key)
	char *value;
	int key;
{
	int flag = 0;
#ifdef sgi
	__uint32_t noval, *valp = &noval;
#else
	u_long noval, *valp = &noval;
#endif

	switch (key) {
#define caseof(x, y, z)	case x: valp = &rt_metrics.z; flag = y; break
	caseof(K_MTU, RTV_MTU, rmx_mtu);
	caseof(K_HOPCOUNT, RTV_HOPCOUNT, rmx_hopcount);
	caseof(K_EXPIRE, RTV_EXPIRE, rmx_expire);
	caseof(K_RECVPIPE, RTV_RPIPE, rmx_recvpipe);
	caseof(K_SENDPIPE, RTV_SPIPE, rmx_sendpipe);
	caseof(K_SSTHRESH, RTV_SSTHRESH, rmx_ssthresh);
	caseof(K_RTT, RTV_RTT, rmx_rtt);
	caseof(K_RTTVAR, RTV_RTTVAR, rmx_rttvar);
	}
	rtm_inits |= flag;
	if (lockrest || locking)
		rt_metrics.rmx_locks |= flag;
	if (locking)
		locking = 0;
	*valp = atoi(value);
}

static const char *
rt_strerror(n)
{
	switch (n) {
	case ESRCH:
		return "not in table";
	case EBUSY:
		return "entry in use";
	case ENOBUFS:
		return "routing table overflow";
#if 0
	case ENETUNREACH:
		return "gateway unreachable";
#endif
	default:
		return strerror(n);
	}
}

void
newroute(argc, argv)
	int argc;
	register char **argv;
{
	char *cmd, *dest = "", *gateway = "";
	int ishost = 0, ret, attempts, oerrno, flags = RTF_STATIC;
	int key;
	struct hostent *gate_hp = 0, *dest_hp = 0;

	if (uid) {
		errno = EACCES;
		quit("must be root to alter routing table");
	}
	cmd = argv[0];
	if (*cmd != 'g')
		shutdown(s, 0); /* Don't want to read back our messages */
	while (--argc > 0) {
		if (**(++argv)== '-') {
			switch (key = keyword(1 + *argv)) {
			case K_LINK:
				af = AF_LINK;
				aflen = sizeof(struct sockaddr_dl);
				break;
#ifndef sgi
			case K_OSI:
			case K_ISO:
				af = AF_ISO;
				aflen = sizeof(struct sockaddr_iso);
				break;
#endif
			case K_INET:
				af = AF_INET;
				aflen = sizeof(struct sockaddr_in);
				break;
#ifndef sgi
			case K_X25:
				af = AF_CCITT;
				aflen = sizeof(struct sockaddr_x25);
				break;
#endif
			case K_SA:
				af = PF_ROUTE;
				aflen = sizeof(union sockunion);
				break;
#ifndef sgi
			case K_XNS:
				af = AF_NS;
				aflen = sizeof(struct sockaddr_ns);
				break;
#endif
			case K_IFACE:
			case K_INTERFACE:
				iflag++;
				break;
			case K_NOSTATIC:
				flags &= ~RTF_STATIC;
				break;
			case K_LOCK:
				locking = 1;
				break;
			case K_LOCKREST:
				lockrest = 1;
				break;
			case K_HOST:
				if (forcenet)
					usage("host");
				forcehost++;
				break;
			case K_REJECT:
				flags |= RTF_REJECT;
				break;
			case K_BLACKHOLE:
				flags |= RTF_BLACKHOLE;
				break;
			case K_PROTO1:
				flags |= RTF_PROTO1;
				break;
			case K_PROTO2:
				flags |= RTF_PROTO2;
				break;
#ifdef RTF_CLONING
			case K_CLONING:
				flags |= RTF_CLONING;
				break;
#endif
			case K_XRESOLVE:
				flags |= RTF_XRESOLVE;
				break;
			case K_STATIC:
				flags |= RTF_STATIC;
				break;
			case K_IFA:
				argc--;
				(void) getaddr(RTA_IFA, *++argv, 0);
				break;
			case K_IFP:
				argc--;
				(void) getaddr(RTA_IFP, *++argv, 0);
				break;
			case K_GENMASK:
				argc--;
				(void) getaddr(RTA_GENMASK, *++argv, 0);
				break;
			case K_GATEWAY:
				argc--;
				(void) getaddr(RTA_GATEWAY, *++argv, 0);
				break;
			case K_DST:
				argc--;
				ishost = getaddr(RTA_DST, *++argv, &dest_hp);
				dest = *argv;
				break;
			case K_NETMASK:
				argc--;
				(void) getaddr(RTA_NETMASK, *++argv, 0);
				if (so_mask.sin.sin_addr.s_addr == -1) {
					if (forcenet)
						usage("net");
					forcehost++;
					rtm_addrs &= ~RTA_NETMASK;
					bzero(&so_mask, sizeof(so_mask));
				} else {
					if (forcehost)
						usage("host");
					forcenet++;
				}
				break;
			case K_NET:
				if (forcehost)
					usage("net");
				forcenet++;
				break;
			case K_MTU:
			case K_HOPCOUNT:
			case K_EXPIRE:
			case K_RECVPIPE:
			case K_SENDPIPE:
			case K_SSTHRESH:
			case K_RTT:
			case K_RTTVAR:
				argc--;
				set_metric(*++argv, key);
				break;
			default:
				usage(1+*argv);
			}
		} else {
			if ((rtm_addrs & RTA_DST) == 0) {
#ifdef sgi
				if (argc > 0) {
					if (!strcmp(*argv,"net")) {
						if (forcehost)
							usage("net");
						forcenet++;
						argc--; argv++;
					} else if (!strcmp(*argv,"host")) {
						if (forcehost)
							usage("host");
						forcehost++;
						argc--; argv++;
					}
				}
#endif
				dest = *argv;
				ishost = getaddr(RTA_DST, *argv, &dest_hp);
			} else if ((rtm_addrs & RTA_GATEWAY) == 0) {
				gateway = *argv;
				(void) getaddr(RTA_GATEWAY, *argv, &gate_hp);
			} else {
				char *p;
				int ret = strtoul(*argv, &p, 10);
				if (*p != '\0')
				    usage(0);
				iflag = (ret == 0);
				if (!qflag)
				    printf(iflag ? ("old usage of trailing 0"
						    " assuming route to if\n")
					   :("old usage of trailing metric, "
					     " assuming route via gateway\n"));
				continue;
			}
		}
	}
	if (forcehost)
		ishost = 1;
	if (forcenet)
		ishost = 0;
	if (ishost)
		flags |= RTF_HOST;
	if (iflag == 0)
		flags |= RTF_GATEWAY;
	flags |= RTF_UP;
	for (attempts = 1; ; attempts++) {
		errno = 0;
		if ((ret = rtmsg(*cmd, flags)) == 0)
			break;
		if (errno != ENETUNREACH && errno != ESRCH)
			break;
		if (af == AF_INET && *gateway
		    && gate_hp && gate_hp->h_addr_list[1]) {
			gate_hp->h_addr_list++;
			bcopy(gate_hp->h_addr_list[0], &so_gate.sin.sin_addr,
			      gate_hp->h_length);
		} else
			break;
	}
	if (*cmd == 'g')
		exit(exit_val);
	if (!ret && qflag)
		return;
	oerrno = errno;
	(void) printf("%s %s %s", cmd, ishost? "host" : "net", dest);
	if (*gateway) {
		(void) printf(": gateway %s", gateway);
		if (attempts > 1 && ret == 0 && af == AF_INET)
			(void)printf(" (%s)",
				     inet_ntoa(so_gate.sin.sin_addr));
	}
	if (ret == 0)
		(void) printf("\n");
	else
		(void) printf(": %s\n", rt_strerror(oerrno));
}

int					/* 1=is a host address */
inet_makenetandmask(net, sin)
	u_long net;
	register struct sockaddr_in *sin;
{
	u_long addr, mask = 0;
	register char *cp;

	if (net == 0) {
		mask = addr = 0;
	} else if (rtm_addrs & RTA_NETMASK) {
		addr = net;
		mask = so_mask.sin.sin_addr.s_addr;
	} else if (net < 128) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSA_NET;
#ifdef sgi
	} else if (net < 256
		   && (addr = net << 24, IN_CLASSD(addr))) {
		mask = IN_CLASSD_NET;
#endif
	} else if (net < 65536) {
		addr = net << IN_CLASSB_NSHIFT;
		mask = IN_CLASSB_NET;
	} else if (net < 16777216L) {
		addr = net << IN_CLASSC_NSHIFT;
		mask = IN_CLASSC_NET;
	} else {
		addr = net;
		if ((addr & IN_CLASSD_HOST) == 0)
			mask = IN_CLASSD_NET;
		else if ((addr & IN_CLASSA_HOST) == 0)
			mask =  IN_CLASSA_NET;
		else if ((addr & IN_CLASSB_HOST) == 0)
			mask =  IN_CLASSB_NET;
		else if ((addr & IN_CLASSC_HOST) == 0)
			mask =  IN_CLASSC_NET;
		else
			mask = -1;
	}
	sin->sin_addr.s_addr = htonl(addr);
	if (mask == -1) {
		rtm_addrs &= ~RTA_NETMASK;
		return (1);
	}
	rtm_addrs |= RTA_NETMASK;
	sin = &so_mask.sin;
	sin->sin_addr.s_addr = htonl(mask);
#ifdef _HAVE_SA_LEN
	sin->sin_len = 0;
#endif
#ifdef sgi
	sin->sin_family = AF_INET;
#else
	sin->sin_family = 0;
#endif
#ifdef _HAVE_SA_LEN
	cp = (char *)(&sin->sin_addr + 1);
	while (*--cp == 0 && cp > (char *)sin)
		;
	sin->sin_len = 1 + cp - (char *)sin;
#endif
	return (0);
}

/*
 * Interpret an argument as a network address of some kind,
 * returning 1 if a host address, 0 if a network address.
 */
int
getaddr(which, s, hpp)
	int which;
	char *s;
	struct hostent **hpp;
{
	register sup su;
	struct ns_addr ns_addr();
	struct iso_addr *iso_addr();
	struct hostent *hp;
	struct netent *np;
	u_long val;

	if (s == 0)
		usage(0);
	if (af == 0) {
		af = AF_INET;
		aflen = sizeof(struct sockaddr_in);
	}
	rtm_addrs |= which;
	switch (which) {
	case RTA_DST:
		su = &so_dst;
		su->sa.sa_family = af;
		break;
	case RTA_GATEWAY:
		su = &so_gate;
		su->sa.sa_family = af;
		break;
	case RTA_NETMASK:
		su = &so_mask;
		break;
	case RTA_GENMASK:
		su = &so_genmask;
		break;
	case RTA_IFP:
		su = &so_ifp;
		su->sa.sa_family = af;
		break;
	case RTA_IFA:
		su = &so_ifa;
		su->sa.sa_family = af;
		break;
	default:
		usage("Internal Error");
		/*NOTREACHED*/
	}
#ifdef _HAVE_SA_LEN
	su->sa.sa_len = aflen;
#endif
	if (strcmp(s, "default") == 0) {
		switch (which) {
		case RTA_DST:
			forcenet++;
			(void) getaddr(RTA_NETMASK, s, 0);
			break;
		case RTA_NETMASK:
		case RTA_GENMASK:
			bzero(su, sizeof(*su));
		}
		return (0);
	}
	switch (af) {
#ifndef sgi
	case AF_NS:
		if (which == RTA_DST) {
			extern short ns_bh[3];
			struct sockaddr_ns *sms = &(so_mask.sns);
			bzero((char *)sms, sizeof(*sms));
			sms->sns_family = 0;
			sms->sns_len = 6;
			sms->sns_addr.x_net = *(union ns_net *)ns_bh;
			rtm_addrs |= RTA_NETMASK;
		}
		su->sns.sns_addr = ns_addr(s);
		return (!ns_nullhost(su->sns.sns_addr));

	case AF_OSI:
		su->siso.siso_addr = *iso_addr(s);
		if (which == RTA_NETMASK || which == RTA_GENMASK) {
			register char *cp = (char *)TSEL(&su->siso);
			su->siso.siso_nlen = 0;
			do {--cp ;} while ((cp > (char *)su) && (*cp == 0));
			su->siso.siso_len = 1 + cp - (char *)su;
		}
		return (1);
#endif

	case AF_LINK:
		link_addr(s, &su->sdl);
		return (1);

#ifndef sgi
	case AF_CCITT:
		ccitt_addr(s, &su->sx25);
		return (which == RTA_DST ? x25_makemask() : 1);
#endif

	case PF_ROUTE:
#ifdef _HAVE_SA_LEN
		su->sa.sa_len = sizeof(*su);
#endif
		sockaddr(s, &su->sa);
		return (1);

	case AF_INET:
	default:
		break;
	}

	if (hpp == NULL)
		hpp = &hp;
	*hpp = NULL;
	if (inet_aton(s, &su->sin.sin_addr) &&
	    (which != RTA_DST || forcenet == 0)) {
#ifdef sgi
		su->sin.sin_family = AF_INET;
#ifdef _HAVE_SA_LEN
		su->sin.sin_len = sizeof(su->sin);
#endif
#endif
		val = su->sin.sin_addr.s_addr;
		if (inet_lnaof(su->sin.sin_addr) != INADDR_ANY)
			return (1);
		else {
			val = ntohl(val);
			goto netdone;
		}
	}
	if (which != RTA_GATEWAY	/* gateways can never be networks */
	    && ((val = inet_network(s)) != -1
		|| ((np = getnetbyname(s)) != NULL
		    && (val = np->n_net) != 0))) {
netdone:
		if (which == RTA_DST)
			return (inet_makenetandmask(val, &su->sin));
		return (0);
	}
	hp = gethostbyname(s);
	if (hp) {
		*hpp = hp;
		su->sin.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (char *)&su->sin.sin_addr, hp->h_length);
		return (1);
	}
	(void) fprintf(stderr, "%s: bad value\n", s);
	exit(1);
	/* NOTREACHED */
}

#ifndef sgi
int
x25_makemask()
{
	register char *cp;

	if ((rtm_addrs & RTA_NETMASK) == 0) {
		rtm_addrs |= RTA_NETMASK;
		for (cp = (char *)&so_mask.sx25.x25_net;
		     cp < &so_mask.sx25.x25_opts.op_flags; cp++)
			*cp = -1;
		so_mask.sx25.x25_len = (u_char)&(((sup)0)->sx25.x25_opts);
	}
	return 0;
}

short ns_nullh[] = {0,0,0};
short ns_bh[] = {-1,-1,-1};

char *
ns_print(sns)
	struct sockaddr_ns *sns;
{
	struct ns_addr work;
	union { union ns_net net_e; u_long long_e; } net;
	u_short port;
	static char mybuf[50], cport[10], chost[25];
	char *host = "";
	register char *p;
	register u_char *q;

	work = sns->sns_addr;
	port = ntohs(work.x_port);
	work.x_port = 0;
	net.net_e  = work.x_net;
	if (ns_nullhost(work) && net.long_e == 0) {
		if (!port)
			return ("*.*");
		(void) sprintf(mybuf, "*.%XH", port);
		return (mybuf);
	}

	if (bcmp((char *)ns_bh, (char *)work.x_host.c_host, 6) == 0)
		host = "any";
	else if (bcmp((char *)ns_nullh, (char *)work.x_host.c_host, 6) == 0)
		host = "*";
	else {
		q = work.x_host.c_host;
		(void) sprintf(chost, "%02X%02X%02X%02X%02X%02XH",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			/* void */;
		host = p;
	}
	if (port)
		(void) sprintf(cport, ".%XH", htons(port));
	else
		*cport = 0;

	(void) sprintf(mybuf,"%XH.%s%s", ntohl(net.long_e), host, cport);
	return (mybuf);
}
#endif

void
interfaces()
{
	size_t needed;
	int mib[6];
	char *buf, *lim, *next;
	register struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		quit("route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		quit("malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		quit("actual retrieval of interface table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		print_rtmsg(rtm, 0);
	}
}

void
monitor()
{
	int n;
	union {
		struct rt_msghdr hdr;
		char b[2048];
	} msg;

	verbose = 1;
	if (debugonly) {
		interfaces();
		exit(0);
	}
	for(;;) {
		n = read(s, &msg, sizeof(msg));
		if (n != msg.hdr.rtm_msglen)
			(void) printf("got message of size %d\n", n);
		print_rtmsg(&msg.hdr, 1);
	}
}

struct {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
} m_rtmsg;

int
rtmsg(cmd, flags)
	int cmd, flags;
{
	static int seq;
	int rlen;
	register char *cp = m_rtmsg.m_space;
	register int l;

#ifdef _HAVE_SA_LEN
#define NEXTADDR(w, u) \
	if (rtm_addrs & (w)) {\
	    l = ROUNDUP(u.sa.sa_len); bcopy((char *)&(u), cp, l); cp += l;\
	    if (verbose) sodump(&(u),#u);\
	}
#else
#define NEXTADDR(w, u) \
	if (rtm_addrs & (w)) {\
	    l = _FAKE_SA_LEN_DST(&u.sa); l = ROUNDUP(l);\
	    bcopy((char *)&(u), cp, l); cp += l;\
	    if (verbose) sodump(&(u),#u);\
	}
#endif

	errno = 0;
	bzero((char *)&m_rtmsg, sizeof(m_rtmsg));
	if (cmd == 'a')
		cmd = RTM_ADD;
	else if (cmd == 'c')
		cmd = RTM_CHANGE;
	else if (cmd == 'g') {
		cmd = RTM_GET;
		if (so_ifp.sa.sa_family == 0) {
			so_ifp.sa.sa_family = AF_LINK;
#ifdef _HAVE_SA_LEN
			so_ifp.sa.sa_len == sizeof(struct sockaddr_dl);
#endif
			rtm_addrs |= RTA_IFP;
		}
	} else
		cmd = RTM_DELETE;
#define rtm m_rtmsg.m_rtm
	rtm.rtm_type = cmd;
	rtm.rtm_flags = flags;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++seq;
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;

	if (rtm_addrs & RTA_NETMASK)
		mask_addr();
	NEXTADDR(RTA_DST, so_dst);
	NEXTADDR(RTA_GATEWAY, so_gate);
	NEXTADDR(RTA_NETMASK, so_mask);
	NEXTADDR(RTA_GENMASK, so_genmask);
	NEXTADDR(RTA_IFP, so_ifp);
	NEXTADDR(RTA_IFA, so_ifa);
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;
	if (verbose)
		print_rtmsg(&rtm, 0);
	if (debugonly)
		return (0);
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (verbose)
			(void)fprintf(stderr, ": %s\n", rt_strerror(errno));
		exit_val = 2;
		return (-1);
	}
	if (cmd == RTM_GET) {
		do {
			l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
		} while (l > 0 && (rtm.rtm_seq != seq || rtm.rtm_pid != pid));
		if (l < 0) {
			(void) fprintf(stderr,
				       "route: read from routing socket: %s\n",
				       strerror(errno));
			exit_val = 2;
		} else {
			print_getmsg(&rtm, l);
		}
	}
#undef rtm
	return (0);
}

void
mask_addr()
{
#ifdef _HAVE_SA_LEN
	int olen = so_mask.sa.sa_len;
	register char *cp1 = olen + (char *)&so_mask, *cp2;

	for (so_mask.sa.sa_len = 0; cp1 > (char *)&so_mask; )
		if (*--cp1 != 0) {
			so_mask.sa.sa_len = 1 + cp1 - (char *)&so_mask;
			break;
		}
#else
	register char *cp1, *cp2;

#endif
	if ((rtm_addrs & RTA_DST) == 0)
		return;
	switch (so_dst.sa.sa_family) {
#ifndef sgi
	case AF_NS:
	case AF_INET:
	case AF_CCITT:
#endif
	case 0:
		return;
#ifndef sgi
	case AF_ISO:
		olen = MIN(so_dst.siso.siso_nlen,
			   MAX(so_mask.sa.sa_len - 6, 0));
		break;
#endif
	}
#ifdef _HAVE_SA_LEN
	cp1 = so_mask.sa.sa_len + 1 + (char *)&so_dst;
	cp2 = so_dst.sa.sa_len + 1 + (char *)&so_dst;
	while (cp2 > cp1)
		*--cp2 = 0;
	cp2 = so_mask.sa.sa_len + 1 + (char *)&so_mask;
#else
	cp1 = _FAKE_SA_LEN_SRC(&so_mask.sa) + 1 + (char *)&so_dst;
	cp2 = _FAKE_SA_LEN_DST(&so_dst.sa) + 1 + (char *)&so_dst;
	while (cp2 > cp1)
		*--cp2 = 0;
	cp2 = _FAKE_SA_LEN_SRC(&so_mask.sa) + 1 + (char *)&so_mask;
#endif
	while (cp1 > so_dst.sa.sa_data)
		*--cp1 &= *--cp2;
#ifndef sgi
	switch (so_dst.sa.sa_family) {
	case AF_ISO:
		so_dst.siso.siso_nlen = olen;
		break;
	}
#endif
}

char *msgtypes[] = {
	"",
	"RTM_ADD--Add Route",
	"RTM_DELETE--Delete Route",
	"RTM_CHANGE--Change Metrics or flags",
	"RTM_GET--Report Metrics",
	"RTM_LOSING--Kernel Suspects Partitioning",
	"RTM_REDIRECT--Told to use different route",
	"RTM_MISS--Lookup failed on this address",
	"RTM_LOCK--Fix specified metrics",
	"RTM_OLDADD--caused by SIOCADDRT",
	"RTM_OLDDEL--caused by SIOCDELRT",
	"RTM_RESOLVE--Route created by cloning",
	"RTM_NEWADDR--Address being added to iface",
	"RTM_DELADDR--Address being removed from iface",
	"RTM_IFINFO--Iface status change",
	0,
};

u_char metricnames[] =
"\011pksent\010rttvar\7rtt\6ssthresh\5sendpipe\4recvpipe\3expire\2hopcount\1mtu";
u_char routeflags[] =
"\1UP\2GATEWAY\3HOST\4REJECT\5DYNAMIC\6MODIFIED\7DONE\010MASK_PRESENT\011CLONING\012XRESOLVE\013LLINFO\014STATIC\017PROTO2\020PROTO1\021CKSUM";
u_char ifnetflags[] =
"\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\010NOARP\011PPROMISC\012ALLMULTI\013OFILTMULTI\014MULTICAST\015CKSUM\016ALLCAST\017DRVRLOCK\020PRIVATE\021LINK0\022LINK1\023LINK2\25L2IPFRAG\26L2TCPSEG";
u_char addrnames[] =
"\1DST\2GATEWAY\3NETMASK\4GENMASK\5IFP\6IFA\7AUTHOR\010BRD";

void
print_rtmsg(register struct rt_msghdr *rtm, int pdate)
{
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct timeval now;
	char ts[20];

	if (verbose == 0)
		return;

	if (pdate) {
		(void)gettimeofday(&now, 0);
		(void)cftime(ts, "   %T", &now.tv_sec);
	} else {
		ts[0] = '\0';
	}

	if (rtm->rtm_version != RTM_VERSION) {
		(void) printf("routing message version %d not understood"
			      "%s\n",
			      rtm->rtm_version, ts);
		return;
	}

	(void)printf("%s: len=%d ", msgtypes[rtm->rtm_type], rtm->rtm_msglen);
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		(void) printf("if index=%d%s\n flags", ifm->ifm_index, ts);
		bprintf(stdout, ifm->ifm_flags, ifnetflags);
		pmsg_addrs((char *)(ifm + 1), ifm->ifm_addrs);
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		ifam = (struct ifa_msghdr *)rtm;
		(void) printf("metric=%d%s\n flags", ifam->ifam_metric, ts);
		bprintf(stdout, ifam->ifam_flags, routeflags);
		pmsg_addrs((char *)(ifam + 1), ifam->ifam_addrs);
		break;
	default:
		(void) printf("pid=%d seq=%d ", rtm->rtm_pid, rtm->rtm_seq);
		if (rtm->rtm_errno != 0)
			(void) printf("errno=%d ", rtm->rtm_errno);
		(void) printf("%s\n flags", ts);
		bprintf(stdout, rtm->rtm_flags, routeflags);
		pmsg_common(rtm);
	}
}

void
print_getmsg(rtm, msglen)
	register struct rt_msghdr *rtm;
	int msglen;
{
	struct sockaddr *dst = NULL, *gate = NULL, *mask = NULL;
	struct sockaddr_dl *ifp = NULL;
	register struct sockaddr *sa;
	register char *cp;
	register int i;

#ifdef sgi
	(void) printf("   route to: %s\n",
		      routename((struct sockaddr *)&so_dst,0));
#else
	(void) printf("   route to: %s\n", routename(&so_dst));
#endif
	if (rtm->rtm_version != RTM_VERSION) {
		(void)fprintf(stderr,
		    "routing message version %d not understood\n",
		    rtm->rtm_version);
		exit_val = 2;
		return;
	}
	if (rtm->rtm_msglen > msglen) {
		(void)fprintf(stderr,
		    "message length mismatch, in packet %d, returned %d\n",
		    rtm->rtm_msglen, msglen);
		exit_val = 2;
	}
	if (rtm->rtm_errno)  {
		(void) fprintf(stderr, "RTM_GET: %s (errno %d)\n",
		    strerror(rtm->rtm_errno), rtm->rtm_errno);
		exit_val = 2;
		return;
	}
	cp = ((char *)(rtm + 1));
	if (rtm->rtm_addrs)
		for (i = 1; i; i <<= 1)
			if (i & rtm->rtm_addrs) {
				sa = (struct sockaddr *)cp;
				switch (i) {
				case RTA_DST:
					dst = sa;
					break;
				case RTA_GATEWAY:
					gate = sa;
					break;
				case RTA_NETMASK:
					mask = sa;
					break;
				case RTA_IFP:
					if (sa->sa_family == AF_LINK &&
					   ((struct sockaddr_dl *)sa)->sdl_nlen)
						ifp = (struct sockaddr_dl *)sa;
					break;
				}
				ADVANCE(cp, sa);
			}
	if (dst && mask)
		mask->sa_family = dst->sa_family;	/* XXX */
	if (dst)
		(void)printf("destination: %s\n", routename(dst,0));
	if (mask) {
		int savenflag = nflag;

		nflag = 1;
		(void)printf("       mask: %s\n", routename(mask,0));
		nflag = savenflag;
	}
	if (gate && rtm->rtm_flags & RTF_GATEWAY)
		(void)printf("    gateway: %s\n", routename(gate,0));
	if (gate && rtm->rtm_flags & RTF_LLINFO)
		(void)printf("    gateway: %s\n", routename(gate,0));
	if (ifp)
		(void)printf("  interface: %.*s\n",
		    ifp->sdl_nlen, ifp->sdl_data);
	(void)printf("      flags: ");
	bprintf(stdout, rtm->rtm_flags, routeflags);

#define lock(f)	((rtm->rtm_rmx.rmx_locks & __CONCAT(RTV_,f)) ? 'L' : ' ')
#define msec(u)	(((u) + 500) / 1000)		/* usec to msec */

	(void) printf("\n%s\n", "\
 recvpipe  sendpipe  ssthresh  rtt,msec    rttvar  hopcount      mtu     expire");
	printf("%8d%c ", rtm->rtm_rmx.rmx_recvpipe, lock(RPIPE));
	printf("%8d%c ", rtm->rtm_rmx.rmx_sendpipe, lock(SPIPE));
	printf("%8d%c ", rtm->rtm_rmx.rmx_ssthresh, lock(SSTHRESH));
	printf("%8d%c ", msec(rtm->rtm_rmx.rmx_rtt), lock(RTT));
	printf("%8d%c ", msec(rtm->rtm_rmx.rmx_rttvar), lock(RTTVAR));
	printf("%8d%c ", rtm->rtm_rmx.rmx_hopcount, lock(HOPCOUNT));
	printf("%8d%c ", rtm->rtm_rmx.rmx_mtu, lock(MTU));
	if (rtm->rtm_rmx.rmx_expire)
		rtm->rtm_rmx.rmx_expire -= time(0);
	printf("%8d%c\n", rtm->rtm_rmx.rmx_expire, lock(EXPIRE));
#undef lock
#undef msec
#define	RTA_IGN	(RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_IFP|RTA_IFA|RTA_BRD)
	if (verbose)
		pmsg_common(rtm);
	else if (rtm->rtm_addrs &~ RTA_IGN) {
		(void) printf("sockaddrs: ");
		bprintf(stdout, rtm->rtm_addrs, addrnames);
		putchar('\n');
	}
#undef	RTA_IGN
}

void
pmsg_common(register struct rt_msghdr *rtm)
{
	int i;
	u_char *s;
	u_int *vp;

	if (rtm->rtm_rmx.rmx_locks != 0 || rtm->rtm_inits != 0) {
		if (rtm->rtm_rmx.rmx_locks != 0) {
			(void) printf(" locks");
			bprintf(stdout, rtm->rtm_rmx.rmx_locks, metricnames);
			(void) putchar('\n');
		}
		s = metricnames;
		while (i = *s++) {
			if (rtm->rtm_inits & (1 << (i-1))) {
				(void) putchar(' ');
				for (; (i = *s) > 32; s++)
					(void) putchar(i);
				(void) printf("=%d",
					      (&rtm->rtm_rmx.rmx_mtu)[i]);
			} else {
				while (*s > 32)
					s++;
			}
		}
	}
	pmsg_addrs(((char *)(rtm + 1)), rtm->rtm_addrs);
}

static void
pmsg_addrs(char *sp, int addrs)
{
	register struct sockaddr *sa;
	u_char family = AF_UNSPEC;
	u_char *cp, *cplim;
	int i, j;

	if (addrs == 0) {
		(void) putchar('\n');
		return;
	}
	(void) printf("\n sockaddrs: ");
	bprintf(stdout, addrs, addrnames);
	(void) putchar('\n');
	for (i = 1; i; i <<= 1)
		if (i & addrs) {
			sa = (struct sockaddr *)sp;
			if (i == RTA_NETMASK || i == RTA_GENMASK) {
				(void) fputs(" 0x", stdout);
#ifndef _HAVE_SA_LEN
				cp = (u_char *)&sa->sa_data[2];
				j = _FAKE_SA_LEN_SRC(sa);
				cplim = (u_char *)sa + MIN(j,sizeof(*sa));
#else
				/* XXX fix this when we have SA_LEN and
				 * when address families of 0 make sense.
				 */
				cp = (u_char *)&sa->sa_data[asdf];
				cplim = (u_char *)sa + sa->sa_len;
#endif
				while (cp < cplim)
					(void) printf("%02x", *cp++);
				while (((j++) % 4) != 0)
					(void) printf("00");
			} else {
				if (family == AF_UNSPEC)
					family = sa->sa_family;
				(void) printf(" %s", routename(sa,0));
			}
			ADVANCE(sp, sa);
		}
	(void) putchar('\n');
	(void) fflush(stdout);
}

void
bprintf(fp, b, s)
	register FILE *fp;
	register __uint32_t b;
	register u_char *s;
{
	register int i;
	int gotsome = 0;

	if (b == 0)
		return;
	while (i = *s++) {
		if (b & (1 << (i-1))) {
			if (gotsome == 0)
				i = '<';
			else
				i = ',';
			(void) putc(i, fp);
			gotsome = 1;
			for (; (i = *s) > 32; s++)
				(void) putc(i, fp);
		} else
			while (*s > 32)
				s++;
	}
	if (gotsome)
		(void) putc('>', fp);
}

int
keyword(cp)
	char *cp;
{
	register struct keytab *kt = keywords;

	while (kt->kt_cp && strcmp(kt->kt_cp, cp))
		kt++;
	return kt->kt_i;
}

void
sodump(su, which)
	register sup su;
	char *which;
{
	int i, f;
	u_char *cp;

	switch (su->sa.sa_family) {
	case AF_LINK:
		(void) printf("%s: link %s; ",
		    which, link_ntoa(&su->sdl));
		break;
#ifndef sgi
	case AF_ISO:
		(void) printf("%s: iso %s; ",
		    which, iso_ntoa(&su->siso.siso_addr));
		break;
#endif
	case AF_INET:
		(void) printf("%s: inet %s; ",
		    which, inet_ntoa(su->sin.sin_addr));
		break;
#ifndef sgi
	case AF_NS:
		(void) printf("%s: xns %s; ",
		    which, ns_ntoa(su->sns.sns_addr));
		break;
#endif
	case 0:
		(void) printf("%s: 0x", which);
#ifdef _HAVE_SA_LEN
		i = su->sa.len;
#else
		i = _FAKE_SA_LEN_SRC(&su->sa);
#endif
		cp = (u_char*)su->sa.sa_data;
		f = 0;
		do {
			if (*cp != 0)
				f = 1;
			if (f)
				(void) printf("%02x", *cp++);
		} while (--i > 1);
		(void) printf("%02x; ", *cp);
		break;
	}
	(void) fflush(stdout);
}

/* States*/
#define VIRGIN	0
#define GOTONE	1
#define GOTTWO	2
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define DELIM	(4*2)

void
sockaddr(addr, sa)
	register char *addr;
	register struct sockaddr *sa;
{
	register char *cp = (char *)sa;
#ifdef _HAVE_SA_LEN
	int size = sa->sa_len;
#else
#define size sizeof(*sa)
#endif
	char *cplim = cp + size;
	register int byte = 0, state = VIRGIN, new;

	bzero(cp, size);
	cp++;
	do {
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0)
			state |= END;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case GOTTWO | DIGIT:
			*cp++ = byte; /*FALLTHROUGH*/
		case VIRGIN | DIGIT:
			state = GOTONE; byte = new; continue;
		case GOTONE | DIGIT:
			state = GOTTWO; byte = new + (byte << 4); continue;
		default: /* | DELIM */
			state = VIRGIN; *cp++ = byte; byte = 0; continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte; /* FALLTHROUGH */
		case VIRGIN | END:
			break;
		}
		break;
	} while (cp < cplim);
#ifdef _HAVE_SA_LEN
	sa->sa_len = cp - (char *)sa;
#else
#undef size
#endif
}
