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
static char sccsid[] = "@(#)arp.c	8.2 (Berkeley) 1/2/94";
#endif /* not lint */

/*
 * arp - display, set, and delete arp table entries
 */

#define SRCROUTE	1

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <paths.h>
#include <strings.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <cap_net.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdb.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <arpa/inet.h>

struct ether_addr {
	unsigned char ether_addr_octet[6];
};
struct ether_addr *ether_aton(char *);
char *ether_ntoa(struct ether_addr *);

static int pid;
static int nflag = 0;
static int rt_sock = -1;
static int in_sock = -1;

#ifdef SRCROUTE
void sriprint(u_short*);
#endif

static int file(char *);
static void quit(char *, ...);
static void usage(void);
static int dump(in_addr_t, int);
static int delete(void);
static int dump_entry(char *);
static int get_route(struct in_addr, int, struct sockaddr_inarp *,
		     struct sockaddr_dl *, struct sockaddr_dl *);
static int get_host_addr(char *);
static void get_host_name(struct in_addr *);
static int rtmsg(int, struct sockaddr_inarp *, struct sockaddr_dl *,
		 struct sockaddr_inarp *, int, int, int);
static void rt_xaddrs(struct rt_addrinfo *,
		      struct sockaddr *, struct sockaddr *, int);
#define INFO_DST(I)	((I).rti_info[RTAX_DST])
#define INFO_GATE(I)	((I).rti_info[RTAX_GATEWAY])
#define INFO_IFP(I)	((I).rti_info[RTAX_IFP])


static char *progname;

#define DASH_G	0
#define DASH_A	1
#define DASH_C	2
#define DASH_D	3
#define DASH_F	4
#define DASH_S	5
static int option = DASH_G;

static int verbose;

static struct in_addr host_addr;
static char host_name[MAXHOSTNAMELEN+1];

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))

#ifdef _HAVE_SA_LEN
struct	sockaddr_dl blank_sdl = {sizeof(blank_sdl), AF_LINK};
#else /* _HAVE_SA_LEN */
struct	sockaddr_dl blank_sdl = {AF_LINK};
#endif /* _HAVE_SA_LEN */
struct	sockaddr_inarp blank_sarp = {sizeof(blank_sarp), AF_INET};
struct	sockaddr_inarp host_mask = {
	sizeof(host_mask), 0, 0, 0xffffffff
};
struct	sockaddr_inarp proxy_mask = {
	sizeof(proxy_mask), 0, 0, 0xffffffff,0,0,SARP_PROXY
};

struct {
	struct	rt_msghdr m;
	struct sockaddr m_space[32];
} m_rtmsg;

int
main(int argc, char **argv)
{
	int ch, i, flags;
	progname = argv[0];

	pid = getpid();

	flags = 0;
	while ((ch = getopt(argc, argv, "acdnsfv")) != EOF) {
		switch(ch) {
		case 'a':
			option = DASH_A;
			flags++;
			break;
		case 'c':
			option = DASH_C;
			flags++;
			break;
		case 'd':
			option = DASH_D;
			flags++;
			break;
		case 'n':
			nflag = 1;
			continue;
		case 's':
			option = DASH_S;
			flags++;
			break;
		case 'f':
			option = DASH_F;
			flags++;
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (flags > 1)
		usage();

	switch (option) {
	case DASH_A:
		if (argc != 0)
			usage();
		(void)dump(0, 0);
		break;
	case DASH_C:
		(void)dump(0, 1);
		break;
	case DASH_D:
		if (argc != 1)
			usage();
		if (get_host_addr(argv[0]) || delete())
			exit(1);
		break;
	case DASH_S:
		if (argc < 2)
			usage();
		i = set(argc, &argv[0]);
		if (i < 0)
			usage();
		exit(i);
		/* NOTREACHED */
	case DASH_F:
		if (argc != 1)
			usage();
		exit (file(argv[0]) ? 1 : 0);
		/* NOTREACHED */
	default:
		if (argc != 1)
			usage();
		exit(dump_entry(argv[0]));
		/* NOTREACHED */
	}

	exit(0);
	/* NOTREACHED */
}


/*
 * Process a file to set standard arp entries
 */
static int
file(char *name)
{
	FILE *fp;
	int i, retval;
	char line[100], arg[5][50], *args[5];

	if ((fp = fopen(name, "r")) == NULL) {
		fprintf(stderr, "arp: cannot open %s\n", name);
		exit(1);
	}
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while(fgets(line, 100, fp) != NULL) {
		i = sscanf(line, "%49s %49s %49s %49s %49s",
			   arg[0], arg[1], arg[2], arg[3], arg[4]);
		if (i < 2 || i > 4) {
			fprintf(stderr, "arp: bad line: %s\n", line);
			retval = 1;
			if (i < 2)
				continue;
		}
		if (set(i, args))
			retval = 1;
	}
	fclose(fp);
	return retval;
}


void
getsocket(void) {
	if (rt_sock < 0) {
		rt_sock = cap_socket(PF_ROUTE, SOCK_RAW, 0);
		if (rt_sock < 0) {
			fprintf(stderr, "%s: PF_ROUTE socket: %s\n",
				progname, strerror(errno));
			exit(1);
		}
	}
}


static int				/* 0=ok */
set_get_route(int proxy,
	      struct sockaddr_inarp *sarp,
	      struct sockaddr_dl *sdl)
{
	int i;

	i = get_route(host_addr, proxy, sarp, sdl, 0);
	if (i > 0)
		return i;
	if (i) {
		fprintf(stderr, "%s: cannot add entry for unreachable %s\n",
			progname, host_name);
		return 1;
	}
	return 0;
}


/*
 * Set an individual arp entry
 */
static int				/* 1=perror called, -1=usage needed */
set(int argc, char **argv)
{
	struct sockaddr_inarp sarp, sarp_m, *smask;
	struct sockaddr_dl sdl, sdl_m;
	struct ether_addr *ea;
	int expire_time, proxy_only, flags;
	char *eaddr;

	getsocket();

	sarp_m = blank_sarp;

	if (get_host_addr(*argv++))
		return 1;
	sarp.sarp_addr = host_addr;

	eaddr = *argv++;
	argc -= 2;
	ea = ether_aton(eaddr);
	if (!ea) {
		fprintf(stderr,"%s: invalid Ethernet address '%s'\n",
			progname, eaddr);
		return 1;
	}
	sdl_m = blank_sdl;
	bcopy(ea, LLADDR(&sdl_m), sdl_m.sdl_alen = sizeof(*ea));

	flags = RTF_LLINFO;
	expire_time = 0;
	proxy_only = 0;
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0) {
			struct timeval time;
			gettimeofday(&time, 0);
			expire_time = time.tv_sec + 20 * 60;
		} else if (strncmp(argv[0], "pub", 3) == 0) {
			flags |= RTF_ANNOUNCE;
		} else if (strncmp(argv[0], "proxy-only", 10) == 0) {
			flags |= RTF_ANNOUNCE;
			proxy_only = 1;
		} else if (strncmp(argv[0], "trail", 5) == 0) {
			fprintf(stderr, "%s: %s:"
				" Sending trailers is no longer supported\n",
				progname, host_name);
		} else {
			return -1;
		}
		argv++;
	}

	/*
	 * First try to add an ordinary entry.
	 * Look for any sort of route to the target host.  If no such
	 * route exists, then we only proxy for it.
	 * There is a non-proxy route for the target
	 * If it is via a non-ARP interface, we can only add a proxy.
	 *
	 * Ordinary ARP table entries are host routes.
	 * Proxy-only entries use the wierd 5-byte host-routes.
	 */
	if (0 > set_get_route(0, &sarp, &sdl))
		return 1;

	/*
	 * If there is already an ARP table entry, then delete it.
	 *
	 * Unless we are installing a proxy-only route, do the same thing
	 * as the kernel does in arpresolve(). If we have a host route that
	 * points is not an ARP entry, and if the route is not static,
	 * then delete the route.
	 *
	 * Be sure that all versions of the ARP entry are gone.
	 */
	while ((sarp.sarp_addr.s_addr == host_addr.s_addr
		&& sdl.sdl_family == AF_LINK)
	       || (!proxy_only
		   && (m_rtmsg.m.rtm_flags & RTF_GATEWAY)
		   && (m_rtmsg.m.rtm_flags & RTF_HOST)
		   && !(m_rtmsg.m.rtm_flags & RTF_STATIC))) {
		if (0 > rtmsg(RTM_DELETE, &sarp,0,0, 0,0, 0))
			return 1;
		if (0 > set_get_route(0, &sarp, &sdl))
			return 1;
	}

	if (proxy_only
	    || sdl.sdl_family != AF_LINK
	    || (sdl.sdl_family == AF_LINK
		&& sarp.sarp_addr.s_addr == host_addr.s_addr
		&& (m_rtmsg.m.rtm_flags & RTF_LLINFO)
		&& !(m_rtmsg.m.rtm_flags & RTF_GATEWAY))) {
		if (!(flags & RTF_ANNOUNCE)) {
			fprintf(stderr, "%s: can only proxy for %s\n",
				progname, host_name);
			return 1;
		}
		/*
		 * Try to add a purely proxy entry.
		 */
		sarp_m.sarp_addr = host_addr;
		sarp_m.sarp_other = SARP_PROXY;
		sarp_m.sarp_len = sizeof(sarp_m);
		sdl_m.sdl_type = 0;
		sdl_m.sdl_index = 0;
		smask = &proxy_mask;
		return rtmsg(RTM_ADD, &sarp_m, &sdl_m, smask,
			     flags,expire_time, 0);
	}

	/*
	 * Try to create an ordinary ARP entry.
	 * Private entries are host routes and so are not found
	 * by the kernel when it looks for them with the SARP_PROXY
	 * bit set.  Published entries are network routes that match
	 * the lookups by the kernel.
	 */
	sarp_m.sarp_addr = host_addr;
#ifdef _HAVE_SA_LEN
	sarp_m.sarp_len = sizeof(struct sockaddr_in);
#else
	sarp_m.sarp_len = 0;
#endif
	sdl_m.sdl_type = sdl.sdl_type;
	sdl_m.sdl_index = sdl.sdl_index;
	smask = (flags & RTF_ANNOUNCE) ? &host_mask : 0;
	if (rtmsg(RTM_ADD, &sarp_m, &sdl_m, smask, flags,expire_time, 0))
		return 1;

	/*
	 * Without care, you can get both the private ARP entry and
	 * the announced entry in into the kernel routing table.
	 * So look for the private entry and delete it if it leaked in.
	 */
	if ((flags & RTF_ANNOUNCE)
	    && !set_get_route(0, &sarp, &sdl)
	    && !(m_rtmsg.m.rtm_flags & RTF_GATEWAY)
	    && (m_rtmsg.m.rtm_flags & RTF_HOST)
	    && sarp.sarp_addr.s_addr == host_addr.s_addr
	    && sdl.sdl_family == AF_LINK) {
		return rtmsg(RTM_DELETE, &sarp,0,0, 0,0, 0);
	}

	return 0;
}


/*
 * Display an individual arp entry
 */
static int
dump_entry(char *host)
{
	struct sockaddr_inarp sarp_m;

	sarp_m = blank_sarp;
	if (get_host_addr(host))
		exit(1);
	bcopy(&host_addr, &sarp_m.sarp_addr, sizeof(sarp_m.sarp_addr));
	if (!dump(sarp_m.sarp_addr.s_addr, 0)) {
		printf("%s (%s) -- no entry\n",
		       host_name, inet_ntoa(sarp_m.sarp_addr));
		return 1;
	}
	return 0;
}


/*
 * Delete one of the routing table entries constituting an ARP entry
 */
static int				/* -1 = failed--quit now */
delete_entry(int proxy,			/* 0 or SARP_PROXY */
	     int *retvalp,		/* set =0 when something deleted */
	     struct sockaddr_inarp *sarp,
	     struct sockaddr_dl *sdl)
{
	int i;


	i = get_route(host_addr, proxy, sarp, sdl, 0);
	if (i > 0)
		return -1;
	if (i) {
		if (errno == ESRCH) {
			fprintf(stderr, "%s: %s: %s\n",
				progname, host_name, strerror(errno));
			return -1;
		}
		return 0;
	}

	if (sarp->sarp_addr.s_addr == host_addr.s_addr
	    && sdl->sdl_family == AF_LINK
	    && (m_rtmsg.m.rtm_flags & RTF_LLINFO)
	    && !(m_rtmsg.m.rtm_flags & RTF_GATEWAY)) {
		if (!rtmsg(RTM_DELETE, sarp,0,0, 0,0, 1))
			*retvalp= 0;	/* have deleted at least one entry */
	}
	return 0;
}


/*
 * Delete all of an arp entry
 *	host_addr must already be set.
 */
static int				/* 0=ok, != 0 if bad */
delete(void)
{
	struct sockaddr_inarp sarp;
	struct sockaddr_dl sdl;
	int i, retval = 1;

	getsocket();

	/*
	 * Deal with the published entry--if it exists.  By asking for
	 * proxy-only entry, we will get the weird proxy-only entry
	 * if it exists.  If the procy-only entry does not exist, and
	 * if a published entry exists, we will find the proxy entry
	 * instead.
	 */
	if (0 > delete_entry(SARP_PROXY, &retval, &sarp, &sdl))
		return -1;

	/*
	 * Deal with the non-published host-route entry in the routing
	 * table, and then the published network route.
	 * In theory, only one should exist, but as they say,
	 * in theory there is no difference between theory and practice
	 * but in practice there is.  Who knows what manual routing
	 * table fiddling has been done with the `route` command?
	 */
	if (0 > delete_entry(0, &retval, &sarp, &sdl))
		return -1;
	if (0 > delete_entry(0, &retval, &sarp, &sdl))
		return -1;

	if (retval
	    && (sarp.sarp_addr.s_addr != host_addr.s_addr
#ifdef _HAVE_SA_LEN
		|| sarp.sarp_len != sizeof(struct sockaddr_in)
#else

		|| _FAKE_SA_LEN_SRC((struct sockaddr *)&sarp) != _SIN_ADDR_SIZE
#endif
		|| sdl.sdl_family != AF_LINK)) {
		/*
		 * We found a non-ARP (i.e. IP) entry in the kernel table.
		 * That is ok if we are deleting a proxy entry.
		 */
		fprintf(stderr, "%s: cannot locate %s\n",
			progname, host_name);
		return 1;
	}

	if (retval)
		fprintf(stderr, "%s: cannot locate %s\n",
			progname, host_name);
	else
		printf("%s (%s) deleted\n",
		       host_name, inet_ntoa(host_addr));
	return retval;
}


#ifdef SRCROUTE
struct arpreqx *
arget(struct in_addr inaddr)
{
	static struct arpreqx ar;
	struct sockaddr_in *sin;
	u_char *ea;
	char *inet_ntoa();

	bzero(&ar, sizeof(ar));
	ar.arp_pa.sa_family = AF_INET;
	sin = (struct sockaddr_in *)&ar.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr = inaddr;
	if (in_sock < 0) {
		in_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (in_sock < 0) {
			fprintf(stderr, "%s: AF_INET socket: %s\n",
				progname, strerror(errno));
			return 0;
		}
	}
	if (ioctl(in_sock, SIOCGARPX, &ar) < 0) {
		if (errno != ENXIO)
			fprintf(stderr, "%s: SIOCGARPX: %s\n",
				progname, strerror(errno));
		return 0;
	}
	return &ar;
}

void
sriprint(u_short *sri)
{
	int i;
	char buf[128];
	int len = (int)(((sri[0]&SRI_LENMASK)>>8)/sizeof(short));
	if (len > SRI_MAXLEN) {
		printf(" source route info too long");
		return;
	}
	if (len == 0)
		return;

	buf[0] = 0;
	sprintf(buf, "%s", " ri");
	for (i = 0; i < len; i++) {
		sprintf(&buf[strlen(buf)], ":%x", (int)(sri[i]));
	}
	buf[3] = '=';
	printf("%s",buf);
}
#endif /* SRCROUTE */


/*
 * Dump the entire arp table
 */
static int				/* number of entries dumped */
dump(in_addr_t addr, int flushit)
{
	int mib[6];
	size_t needed;
	struct rt_addrinfo info;
	char *lim, *sysctl_buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sarp;
	struct sockaddr_dl if_sdl, *sdl;
#ifdef _HAVE_SA_LEN
	struct sockaddr_in *smask;
#else
	struct sockaddr_in_new *smask;
#endif
	struct arptab *at;
	struct arpreqx *ar;
	int retval = 0;
	time_t now = time(0);

	if (verbose)
		getsocket();

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	for (;;) {
		if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
			quit("sysctl() estimate: %s\n", strerror(errno));
		if ((sysctl_buf = malloc(needed)) == NULL)
			quit("malloc(%d)=0\n", needed);
		if (sysctl(mib, 6, sysctl_buf, &needed, NULL, 0) >= 0)
			break;

		/*
		 * Retry if the table grew between the two calls.
		 * This can happen if enough routes or ARP entries
		 * are being added to overwhelm the kernel's
		 * padded estimate.
		 */
		if (errno != ENOMEM && errno != EFAULT)
			quit("sysctl(): %s\n", strerror(errno));
		free(sysctl_buf);
	}

	lim = sysctl_buf + needed;
	for (next = sysctl_buf; next < lim; ) {
		rtm = (struct rt_msghdr *)next;
		next += rtm->rtm_msglen;
		rt_xaddrs(&info, (struct sockaddr *)(rtm+1),
			  (struct sockaddr *)next,
			  rtm->rtm_addrs);
		sarp = (struct sockaddr_inarp *)INFO_DST(info);
		sdl = (struct sockaddr_dl *)INFO_GATE(info);

		if (addr && addr != sarp->sarp_addr.s_addr)
			continue;
		retval++;

		get_host_name(&sarp->sarp_addr);

		if (flushit) {
			if (rtm->rtm_rmx.rmx_expire != 0) {
				host_addr = sarp->sarp_addr;
				(void)delete();
			}
			continue;
		}

		printf("%s (%s) at ", host_name, inet_ntoa(sarp->sarp_addr));
		if (sdl->sdl_alen) {
			fputs(ether_ntoa((struct ether_addr *)LLADDR(sdl)),
			      stdout);
			if (verbose
			    && !get_route(sarp->sarp_addr,
					  sarp->sarp_other,
					  0, 0, &if_sdl))
				printf(" (via %.*s)", if_sdl.sdl_nlen,
				       if_sdl.sdl_data);
		} else {
			printf("(incomplete)");
		}
		if (rtm->rtm_rmx.rmx_expire == 0) {
			printf(" permanent");
		} else if (verbose) {
			printf(" age=%d", rtm->rtm_rmx.rmx_expire-now);
		}
		if (rtm->rtm_flags & RTF_ANNOUNCE)
			printf(" published");
		if (sarp->sarp_other & SARP_PROXY)
			printf(" proxy-only");
#ifdef SRCROUTE
		if ((ar = arget(sarp->sarp_addr)) && ar->arp_sri[0])
			sriprint((u_short *)&ar->arp_sri[0]);
#endif /* SRCROUTE */
		printf("\n");
	}

	return retval;
}


static void
quit(char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fprintf(stderr, "arp: ");
	vfprintf(stderr, msg, args);
	va_end(args);
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: arp [-nv] hostname\n");
	fprintf(stderr, "       arp [-nv] -a\n");
	fprintf(stderr, "       arp [-n] -c\n");
	fprintf(stderr, "       arp -d hostname\n");
	fprintf(stderr, "       arp -s hostname ether_addr [temp] [pub] [proxy-only]\n");
	fprintf(stderr, "       arp -f filename\n");
	exit(1);
}


static int
get_host_addr(char *host)		/* 0=ok */
{
	struct hostent *hp;

	if (!(hp = gethostbyname(host))) {
		fprintf(stderr, "%s: %s: %s\n",
			progname, host, hstrerror(h_errno));
		return 1;
	}

	bcopy(hp->h_addr, &host_addr, sizeof(host_addr));
	strncpy(host_name, host, sizeof(host_name)-1);
	return 0;
}


static void
get_host_name(struct in_addr *addr)
{
	struct hostent *hp;

	if (!nflag) {
		hp = gethostbyaddr(addr, sizeof(*addr), AF_INET);
		if (hp) {
			strncpy(host_name, hp->h_name, sizeof(host_name)-1);
		} else {
			strcpy(host_name, "?");
			if (h_errno == TRY_AGAIN)
				nflag = 1;
		}
	} else {
		strcpy(host_name, "?");
	}
}


/*
 * Get an entry from the kernel
 */
static int				/* 0=ok, -1=failed, 1=complained */
get_route(struct in_addr addr,		/* for this host */
	  int do_proxy,			/* 0 or SARP_PROXY */
	  struct sockaddr_inarp *sarp,
	  struct sockaddr_dl *sdl,
	  struct sockaddr_dl *if_sdl)
{
	int i;
	struct rt_addrinfo info;
	struct sockaddr_inarp sarp_m;

	sarp_m = blank_sarp;
	sarp_m.sarp_addr = addr;
	if (do_proxy) {
		sarp_m.sarp_other = SARP_PROXY;
	} else {
#ifdef _HAVE_SA_LEN
		sarp_m.sarp_len = sizeof(struct sockaddr_in);
#else
		sarp_m.sarp_len = 0;
#endif
	}
	i = rtmsg(RTM_GET, &sarp_m,0,0, 0,0, 0);
	if (i) {
		if (sarp)
			bzero(sarp, sizeof(*sarp));
		if (sdl)
			bzero(sdl, sizeof(*sdl));
		if (if_sdl)
			bzero(if_sdl, sizeof(*if_sdl));
		return i;
	}

	rt_xaddrs(&info, m_rtmsg.m_space, &m_rtmsg.m_space[RTAX_MAX],
		  m_rtmsg.m.rtm_addrs);
	if (sarp)
		bcopy(INFO_DST(info), sarp, sizeof(*sarp));
	if (sdl)
		bcopy(INFO_GATE(info), sdl, sizeof(*sdl));
	if (if_sdl)
		bcopy(INFO_IFP(info), if_sdl, sizeof(*if_sdl));
	return 0;
}


static int				/* 0=ok, -1=failed, 1=complained */
rtmsg(int cmd,
      struct sockaddr_inarp *sarp,
      struct sockaddr_dl *sdl,
      struct sockaddr_inarp *smask,
      int flags,
      int expire_time,
      int complain)
{
	static int seq;
	char *cp;
	int i;

	bzero(&m_rtmsg, sizeof(m_rtmsg));
	switch (cmd) {
	default:
		fprintf(stderr, "arp: internal wrong cmd\n");
		exit(1);

	case RTM_ADD:
		m_rtmsg.m.rtm_rmx.rmx_expire = expire_time;
		m_rtmsg.m.rtm_inits = RTV_EXPIRE;
		m_rtmsg.m.rtm_flags |= RTF_STATIC;
		break;

	case RTM_GET:
	case RTM_DELETE:
		break;
	}

#ifdef _HAVE_SA_LEN
#define CPADDR(s,l) {bcopy(s, cp, s->l); cp += ROUNDUP(s->l);}
#else
#define CPADDR(s,l) {							\
		bcopy(s, cp, _FAKE_SA_LEN_SRC((struct sockaddr *)s));	\
		cp += ROUNDUP(_FAKE_SA_LEN_DST((struct sockaddr *)s));}
#endif
#define NEXTADDR(w,s,l) {if (s) {m_rtmsg.m.rtm_addrs |= w; CPADDR(s,l);}}
	cp = (char *)m_rtmsg.m_space;
	NEXTADDR(RTA_DST, sarp, sarp_len);
	NEXTADDR(RTA_GATEWAY, sdl, sdl_len);
	if (smask) {
		m_rtmsg.m.rtm_addrs |= RTA_NETMASK;
		CPADDR(smask, sin_len);
	} else {
		flags |= RTF_HOST;
	}
	if (cmd == RTM_GET) {
		m_rtmsg.m.rtm_addrs |= RTA_IFP;
		cp += sizeof(struct sockaddr_dl);
	}
	m_rtmsg.m.rtm_msglen = cp - (char *)&m_rtmsg;

	m_rtmsg.m.rtm_flags |= flags;
	m_rtmsg.m.rtm_version = RTM_VERSION;
	m_rtmsg.m.rtm_seq = ++seq;
	m_rtmsg.m.rtm_type = cmd;

	i = write(rt_sock, &m_rtmsg, m_rtmsg.m.rtm_msglen);
	if (i != m_rtmsg.m.rtm_msglen) {
		if (i >= 0) {
			fprintf(stderr,
				"%s: wrote %d instead of %d to socket\n",
				progname, i, m_rtmsg.m.rtm_msglen);
			return 1;
		} else if (complain || errno != ESRCH
			   || (cmd != RTM_DELETE && cmd != RTM_GET)) {
			fprintf(stderr, "%s: writing to routing socket: %s\n",
				progname, strerror(errno));
			return 1;
		}
		return -1;
	}

	do {
		i = read(rt_sock, &m_rtmsg, sizeof(m_rtmsg));
	} while (i > 0
		 && (m_rtmsg.m.rtm_seq != seq || m_rtmsg.m.rtm_pid != pid));
	if (i < 0) {
		fprintf(stderr, "%s: read from routing socket: %s\n",
			progname, strerror(errno));
		return 1;
	}

	return 0;
}


/*
 * disassemble routing message
 */
void
rt_xaddrs(struct rt_addrinfo *info,
	  struct sockaddr *sa,
	  struct sockaddr *lim,
	  int addrs)
{
	int i;
#ifdef _HAVE_SA_LEN
	static struct sockaddr sa_zero;
#endif

	bzero(info, sizeof(*info));
	info->rti_addrs = addrs;
	for (i = 0; i < RTAX_MAX && sa < lim; i++) {
		if ((addrs & (1 << i)) == 0)
			continue;
#ifdef _HAVE_SA_LEN
		info->rti_info[i] = (sa->sa_len != 0) ? sa : &sa_zero;
		sa = (struct sockaddr *)((char*)(sa)
					 + ROUNDUP(sa->sa_len));
#else
		info->rti_info[i] = sa;
		sa = (struct sockaddr *)((char*)(sa)
					 + ROUNDUP(_FAKE_SA_LEN_DST(sa)));
#endif
	}
}
