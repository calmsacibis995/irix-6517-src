/*	$NetBSD: arp.c,v 1.11 1995/03/01 11:56:13 chopps Exp $ */

/*
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
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
"@(#) Copyright (c) 1984, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)arp.c	8.2 (Berkeley) 1/2/94";*/
static char *rcsid = "$NetBSD: arp.c,v 1.11 1995/03/01 11:56:13 chopps Exp $";
#endif /* not lint */

/*
 * ndp - display, set, and delete ND protocol table entries
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/if_ndp6.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <errno.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <paths.h>

int delete __P((const char *, const char *));
void dump __P((struct in6_addr *));
int ether_aton __P((const char *, u_char *));
void ether_print __P((const u_char *));
int file __P((char *));
void get __P((const char *));
int getinetaddr __P((const char *, struct in6_addr *));
void getsocket __P((void));
int rtmsg __P((int));
int set __P((int, char **));
void usage __P((void));

static int pid;
static int nflag;
static int s = -1;
static int ifindex = 0;
static char *progname;

void
err(int code, char *msg)
{
	perror(msg);
	exit(code);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	int ifopt = 0;
	extern char *optarg;

	progname = argv[0];
	pid = getpid();
	while ((ch = getopt(argc, argv, "andsi:")) != EOF)
		switch((char)ch) {
		case 'a':
			dump(NULL);
			return (0);
		case 'd':
			if (argc < 3 || argc > 4 || ifopt)
				usage();
			(void)delete(argv[2], argv[3]);
			return (0);
		case 'i':
			ifopt = 2;
			if ((ifindex = atoi(optarg)) <= 0)
				usage();
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			if ((argc - ifopt) < 4 || (argc - ifopt) > 7)
				usage();
			if (strcmp(argv[2], "-i") == 0)
				usage();
			return (set(argc-ifopt-2, &argv[2+ifopt]) ? 1 : 0);
		case '?':
		default:
			usage();
		}
	if (ifopt)
		usage();
	if (argc == 2 || (argc == 3 && nflag))
		get(argv[argc - 1]);
	else
		usage();
	return (0);
}

void
getsocket()
{
	if (s >= 0)
		return;
	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		err(1, "socket");
}

struct	sockaddr_in6 blank_sin = {sizeof(blank_sin), AF_INET6 }, sin_m;
struct	sockaddr_dl blank_sdl = {sizeof(blank_sdl), AF_LINK }, sdl_m;
int	expire_time, found_entry;
struct	{
	struct	rt_msghdr m_rtm;
	char	m_space[512];
}	m_rtmsg;

/*
 * Set an individual ndp entry 
 */
int
set(argc, argv)
	int argc;
	char **argv;
{
	register struct sockaddr_in6 *sin;
	register struct sockaddr_dl *sdl;
	register struct rt_msghdr *rtm;
	u_char *ea;
	char *host = argv[0], *eaddr;

	sin = &sin_m;
	rtm = &(m_rtmsg.m_rtm);
	eaddr = argv[1];

	getsocket();
	argc -= 2;
	argv += 2;
	sdl_m = blank_sdl;		/* struct copy */
	sin_m = blank_sin;		/* struct copy */
	if (getinetaddr(host, &sin->sin6_addr) == -1)
		return (1);
	ea = (u_char *)LLADDR(&sdl_m);
	if (ether_aton(eaddr, ea) == 0)
		sdl_m.sdl_alen = 6;
	sdl_m.sdl_index = ifindex;
	expire_time = 0;
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0) {
			struct timeval time;
			(void)gettimeofday(&time, 0);
			expire_time = time.tv_sec + 2 * 60;
		}
		argv++;
	}

	if (rtmsg(RTM_GET) < 0) {
		warn("%s", host);
		return (1);
	}
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(sin->sin6_len + (char *)sin);
	if (IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025:
			goto overwrite;
		}
		warn("bad network type (%d)\n", sdl->sdl_type);
		return (1);
	}
overwrite:
	if (sdl->sdl_family != AF_LINK) {
		(void)printf("cannot intuit interface index and type for %s\n",
		    host);
		return (1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
	return (rtmsg(RTM_ADD));
}

/*
 * Display an individual ndp entry
 */
void
get(host)
	const char *host;
{
	struct sockaddr_in6 *sin;
	char hname[64];
	u_char *ea;

	sin = &sin_m;
	sin_m = blank_sin;		/* struct copy */
	if (getinetaddr(host, &sin->sin6_addr) == -1)
		exit(1);
	dump(&sin->sin6_addr);
	if (found_entry == 0) {
		(void)printf("%s (%s) -- no entry\n", host,
		    inet_ntop(AF_INET6, &sin->sin6_addr, hname, 64));
		exit(1);
	}
}

/*
 * Delete a ndp entry 
 */
int
delete(host, info)
	const char *host;
	const char *info;
{
	register struct sockaddr_in6 *sin;
	register struct rt_msghdr *rtm;
	struct sockaddr_dl *sdl;
	u_char *ea;
	char hname[64], *eaddr;

	sin = &sin_m;
	rtm = &m_rtmsg.m_rtm;

	getsocket();
	sin_m = blank_sin;		/* struct copy */
	if (getinetaddr(host, &sin->sin6_addr) == -1)
		return (1);

	if (rtmsg(RTM_GET) < 0) {
		warn("%s", host);
		return (1);
	}
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(sin->sin6_len + (char *)sin);
	if (IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025:
			goto delete;
		}
	}
	warnx("delete: can't locate %s", host);
	return (1);
delete:
	if (sdl->sdl_family != AF_LINK) {
		(void)printf("cannot locate %s\n", host);
		return (1);
	}
	if (rtmsg(RTM_DELETE)) 
		return (1);
	(void)printf("%s (%s) deleted\n", host,
		     inet_ntop(AF_INET6, &sin->sin6_addr, hname, 64));
	return (0);
}

/*
 * Dump the entire ndp table
 */
void
dump(addr)
	struct in6_addr *addr;
{
	int mib[6];
	size_t needed;
	char *host, *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_in6 *sin;
	struct sockaddr_dl *sdl;
	extern int h_errno;
	struct hostent *hp;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(1, "actual retrieval of routing table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		char hbuf[64];

		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_in6 *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(sin + 1);
		if (sdl->sdl_family != AF_LINK)
			continue;
		if (addr) {
			if (!IN6_ARE_ADDR_EQUAL(addr, &sin->sin6_addr))
				continue;
			found_entry = 1;
		} else if (IN6_IS_ADDR_MULTICAST(&sin->sin6_addr))
			continue;
		if (nflag == 0)
			hp = gethostbyaddr((char *)&sin->sin6_addr,
			    sizeof sin->sin6_addr, AF_INET6);
		else
			hp = 0;
		if (hp)
			host = hp->h_name;
		else {
			host = "?";
			if (h_errno == TRY_AGAIN)
				nflag = 1;
		}
		(void)printf("%s (%s) at link#%d ", host,
			     inet_ntop(AF_INET6, &sin->sin6_addr,
				       hbuf, sizeof(hbuf)),
			     sdl->sdl_index);
		if (sdl->sdl_alen)
			ether_print((const u_char *)LLADDR(sdl));
		else
			(void)printf("(incomplete)");
		if (rtm->rtm_rmx.rmx_expire == 0)
			(void)printf(" permanent");
		if (rtm->rtm_addrs & RTA_NETMASK) {
			sin = (struct sockaddr_in6 *)
#ifdef _HAVE_SA_LEN           
				(sdl->sdl_len + (char *)sdl);
#else
				(_FAKE_SA_LEN_DST((struct sockaddr*)sdl) +
				(char *)sdl);
#endif
			if (!IN6_IS_ADDR_UNSPECIFIED(&sin->sin6_addr))
				(void)printf(" published");
			if (sin->sin6_len != 24)
				(void)printf("(wierd)");
		}
		(void)printf("\n");
	}
}

void
ether_print(cp)
	const u_char *cp;
{
	(void)printf("%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3], cp[4],
	    cp[5]);
}

int
ether_aton(a, n)
	const char *a;
	u_char *n;
{
	int i, o[6];

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2], &o[3], &o[4],
	    &o[5]);
	if (i != 6) {
		warnx("invalid Ethernet address '%s'", a);
		return (1);
	}
	for (i=0; i<6; i++)
		n[i] = o[i];
	return (0);
}

void
usage()
{
	(void)fprintf(stderr, "usage: ndp [-n] hostname\n");
	(void)fprintf(stderr, "usage: ndp [-n] -a\n");
	(void)fprintf(stderr, "usage: ndp -d hostname\n");
	(void)fprintf(stderr, "usage: ndp [-i if_index] -s hostname ether_addr [temp]\n");
	exit(1);
}

int
rtmsg(cmd)
	int cmd;
{
	static int seq;
	int rlen;
	register struct rt_msghdr *rtm;
	register char *cp;
	register int l;

	rtm = &m_rtmsg.m_rtm;
	cp = m_rtmsg.m_space;
	errno = 0;

	if (cmd == RTM_DELETE)
		goto doit;
	(void)memset(&m_rtmsg, 0, sizeof(m_rtmsg));
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		errx(1, "internal wrong cmd");
		/*NOTREACHED*/
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		rtm->rtm_rmx.rmx_expire = expire_time;
		rtm->rtm_inits = RTV_EXPIRE;
		rtm->rtm_flags |= (RTF_HOST | RTF_DYNAMIC);
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}
#define NEXTADDR(w, s) \
	if (rtm->rtm_addrs & (w)) { \
		(void)memcpy(cp, &s, sizeof(s)); cp += sizeof(s);}

	NEXTADDR(RTA_DST, sin_m);
	NEXTADDR(RTA_GATEWAY, sdl_m);

	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			warn("writing to routing socket");
			return (-1);
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		warn("read from routing socket");
	return (0);
}

int
getinetaddr(host, inap)
	const char *host;
	struct in6_addr *inap;
{
	struct hostent *hp;

	if (inet_pton(AF_INET6, host, inap) > 0)
		return (0);
#if 0
	if ((hp = gethostbyname2(host, AF_INET6)) == NULL) {
#endif
		(void)fprintf(stderr, "%s: %s: ", progname, host);
		herror(NULL);
		return (-1);
#if 0
	}
	(void)memcpy(inap, hp->h_addr, sizeof(*inap));
	return (0);
#endif
}
