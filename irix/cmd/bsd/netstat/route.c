/*
 * Copyright (c) 1983, 1988, 1993
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
static char sccsid[] = "@(#)route.c	8.5 (Berkeley) 11/8/94";
#endif /* not lint */

#ident "$Revision: 2.22 $"

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#define _KERNEL
#include <net/route.h>
#undef _KERNEL
#include <netinet/in.h>

#include <netns/ns.h>

#ifdef sgi
#include <net/soioctl.h>
#endif
#include <sys/sysctl.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include "netstat.h"


#ifdef sgi
#define kget(p, d) (klseek(kmem,(ns_off_t)p,0), kread(kmem,&(d),sizeof(d)))
#else
#define kget(p, d) (kread((u_long)(p), (char *)&(d), sizeof (d)))
#endif

/*
 * Definitions for showing gateway flags.
 */
struct bits {
	__uint32_t	b_mask;
	char	b_val;
} bits[] = {
	{ RTF_UP,	'U' },
	{ RTF_GATEWAY,	'G' },
	{ RTF_HOST,	'H' },
	{ RTF_REJECT,	'R' },
	{ RTF_DYNAMIC,	'D' },
	{ RTF_MODIFIED,	'M' },
	{ RTF_DONE,	'd' }, /* Completed -- for routing messages only */
	{ RTF_MASK,	'm' }, /* Mask Present -- for routing messages only */
	{ RTF_CLONING,	'C' },
	{ RTF_XRESOLVE,	'X' },
	{ RTF_LLINFO,	'L' },
	{ RTF_STATIC,	'S' },
	{ RTF_PROTO1,	'1' },
	{ RTF_PROTO2,	'2' },
	{ RTF_CKSUM,	'c' },
	{ 0 }
};

static union {
	struct	sockaddr u_sa;
	u_short	u_data[128];
} pt_u;

int	do_rtent = 0;
struct	rtentry rtentry;
struct	radix_node rnode;
struct	radix_mask rmask;

int	NewTree = 0;

static struct sockaddr *kgetsa __P((struct sockaddr *)); 
static void p_tree __P((struct radix_node *)); 
static void p_rtnode __P(());
static void ntreestuff __P(());
static void np_rtentry __P((struct rt_msghdr *));
static void p_sockaddr __P((struct sockaddr *, int, int, char*));
static void p_flags __P((int, char *));
static void p_rtentry __P((struct rtentry *));
struct sockaddr_in* ifnametoaddr __P((const char* ifname, int size));


struct	radix_node_head *rt_tables[AF_MAX+1];

/*
 * Print routing tables.
 */
void
routepr(rtree)
	u_long rtree;
{
	struct radix_node_head *rnh, head;
	int i;

	printf("Routing tables\n");

#ifdef sgi
	if (tflag)
#else
	if (Aflag == 0 && NewTree)
#endif
		ntreestuff();
	else {
		if (rtree == 0) {
			printf("rt_tables: symbol not in namelist\n");
			return;
		}

		kget(rtree, rt_tables);
		for (i = 0; i <= AF_MAX; i++) {
			if ((rnh = rt_tables[i]) == 0)
				continue;
			kget(rnh, head);
			if (i == AF_UNSPEC) {
				if (Aflag && af == 0) {
					printf("Netmasks:\n");
					p_tree(head.rnh_treetop);
				}
			} else if (af == AF_UNSPEC || af == i) {
				pr_family(i);
				do_rtent = 1;
				pr_rthdr();
				p_tree(head.rnh_treetop);
			}
		}
	}
}

/*
 * Print address family header before a section of the routing table.
 */
void
pr_family(af)
	int af;
{
	char *afname;

	switch (af) {
	case AF_INET:
		afname = "Internet";
		break;
	case AF_NS:
		afname = "XNS";
		break;
	case AF_ISO:
		afname = "ISO";
		break;
	case AF_CCITT:
		afname = "X.25";
		break;
	default:
		afname = NULL;
		break;
	}
	if (afname)
		printf("\n%s:\n", afname);
	else
		printf("\nProtocol Family %d:\n", af);
}

/* column widths; each followed by one space */
#define	WID_DST		16	/* width of destination column */
#define	WID_GW		18	/* width of gateway column */

/*
 * Print header for routing table columns.
 */
void
pr_rthdr()
{

	if (Aflag)
		printf("%-8.8s ","Address");
#ifdef sgi
	printf("%-*.*s %-*.*s %-10s %-6.6s %6.6s%8.8s  %s\n",
	       WID_DST, WID_DST, "Destination",
	       WID_GW, WID_GW, "Gateway",
	       "Netmask",
	       "Flags", "Refs", "Use", "Interface");
#else
	printf("%-*.*s %-*.*s %-6.6s  %6.6s%8.8s  %s\n",
		WID_DST, WID_DST, "Destination",
		WID_GW, WID_GW, "Gateway",
		"Flags", "Refs", "Use", "Interface");
#endif
}

static struct sockaddr *
kgetsa(dst)
	register struct sockaddr *dst;
{

	kget(dst, pt_u.u_sa);
#ifdef _HAVE_SA_LEN
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa))
		kread((u_long)dst, (char *)pt_u.u_data, pt_u.u_sa.sa_len);
#endif
	return (&pt_u.u_sa);
}

static void
p_tree(struct radix_node *rn)
{

again:
	kget(rn, rnode);
	if (rnode.rn_b < 0) {
#if _MIPS_SZLONG == 64
		if (Aflag)
			printf("%-8.8llx ", rn);
#else
		if (Aflag)
			printf("%-8.8x ", rn);
#endif
		if (rnode.rn_flags & RNF_ROOT) {
			if (Aflag)
				printf("(root node)%s",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
		} else if (do_rtent) {
			kget(rn, rtentry);
			p_rtentry(&rtentry);
			if (Aflag)
				p_rtnode();
		} else {
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_key),
			    0, 44, 0);
			putchar('\n');
		}
		if (rn = rnode.rn_dupedkey)
			goto again;
	} else {
		if (Aflag && do_rtent) {
#if _MIPS_SZLONG == 64
			printf("%-8.8llx ", rn);
#else
			printf("%-8.8x ", rn);
#endif
			p_rtnode();
		}
		rn = rnode.rn_r;
		p_tree(rnode.rn_l);
		p_tree(rn);
	}
}

char	nbuf[20];

static void
p_rtnode()
{
	struct radix_mask *rm = rnode.rn_mklist;

	if (rnode.rn_b < 0) {
		if (rnode.rn_mask) {
			printf("\t  mask ");
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_mask),
				    0, -1, 0);
		} else if (rm == 0)
			return;
	} else {
		sprintf(nbuf, "(%d)", rnode.rn_b);
#if _MIPS_SZLONG == 64
		printf("%6.6s %8.8llx : %8.8llx",nbuf, rnode.rn_l, rnode.rn_r);
#else
		printf("%6.6s %8.8x : %8.8x", nbuf, rnode.rn_l, rnode.rn_r);
#endif
	}
	while (rm) {
		kget(rm, rmask);
		sprintf(nbuf, " %d refs, ", rmask.rm_refs);
#if _MIPS_SZLONG == 64
		printf(" mk = %8.8llx {(%d),%s",
		       rm, -1 - rmask.rm_b, rmask.rm_refs ? nbuf : " ");
#else
		printf(" mk = %8.8x {(%d),%s",
		       rm, -1 - rmask.rm_b, rmask.rm_refs ? nbuf : " ");
#endif
		if (rmask.rm_flags & RNF_NORMAL) {
			struct radix_node rnode_aux;
			printf(" <normal>, ");
			kget(rmask.rm_leaf, rnode_aux);
			p_sockaddr(
			 kgetsa((struct sockaddr *)rnode_aux.rn_mask), 0,-1,0);
		} else
		    p_sockaddr(kgetsa((struct sockaddr *)rmask.rm_mask),0,-1,0);
		putchar('}');
		if (rm = rmask.rm_mklist)
			printf(" ->");
	}
	putchar('\n');
}

/* print a netmask as a hex number, evenif it is longer than 4 bytes */
static void
p_mask_hex(struct sockaddr *sa)
{
	int len, j;
	u_char *cp, *cplim;

	len = 11;
	if (sa && ((struct sockaddr_in*)sa)->sin_addr.s_addr != 0) {
		(void) fputs("0x", stdout);
		len -= 2;
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
		while (cp < cplim) {
			(void)printf("%02x", *cp++);
			len -= 2;
		}
		while (((j++) % 4) != 0) {
			(void)printf("00");
			len -= 2;
		}
	}
	do {
		putc(' ', stdout);
	} while (--len > 0);
}


static void
ntreestuff()
{
	size_t needed;
	int mib[6];
	char *buf, *next, *lim;
	register struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		{ perror("route-sysctl-estimate"); exit(1);}
	if ((buf = malloc(needed)) == 0)
		{ printf("out of space\n"); exit(1);}
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		{ perror("sysctl of routing table"); exit(1);}
	lim  = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		np_rtentry(rtm);
	}
}

static void
np_rtentry(rtm)
	register struct rt_msghdr *rtm;
{
	register struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
	static int old_af;
	int af = 0, interesting = RTF_UP | RTF_GATEWAY | RTF_HOST;

#ifdef _HAVE_SA_LEN
	af = sa->sa_family;
#else
	af = _FAKE_SA_FAMILY(sa);
#endif
	if (af != old_af) {
		pr_family(af);
		old_af = af;
	}
	if (!(rtm->rtm_addrs & RTA_GATEWAY)) {
		p_sockaddr(sa, 0, WID_DST+WID_GW, 0);
	} else {
		p_sockaddr(sa, rtm->rtm_flags, WID_DST, 0);
#ifdef _HAVE_SA_LEN
		if (sa->sa_len == 0)
			sa->sa_len = sizeof(long);
		sa = (struct sockaddr *)(sa->sa_len + (char *)sa);
#else
		sa = (struct sockaddr *)(_FAKE_SA_LEN_DST(sa) + (char *)sa);
#endif
		p_sockaddr(sa, 0, WID_GW, 0);
	}
#ifdef _HAVE_SA_LEN
	if (sa->sa_len == 0)
		sa->sa_len = sizeof(long);
	sa = (struct sockaddr *)(sa->sa_len + (char *)sa);
#else
	sa = (struct sockaddr *)(_FAKE_SA_LEN_DST(sa) + (char *)sa);
#endif
	p_mask_hex((rtm->rtm_addrs & RTA_NETMASK) ? sa : 0);
	p_flags(rtm->rtm_flags & interesting, "%-6.6s ");
	putchar('\n');
}

static void
p_sockaddr(sa, flags, width, ifname)
	struct sockaddr *sa;
	int flags, width;
	char *ifname;
{
	char workbuf[MAXHOSTNAMELEN+1], *cplim;
	register char *cp = workbuf;

	switch(sa->sa_family) {
	case AF_INET:
	    {
		register struct sockaddr_in *sin = (struct sockaddr_in *)sa;

		cp = (sin->sin_addr.s_addr == 0) ? "default" :
		      ((flags & RTF_HOST) ?
			routename(sin->sin_addr.s_addr, width) :
			netname(sin->sin_addr.s_addr, 0L, width));
		break;
	    }

	case AF_NS:
		cp = ns_print(sa);
		break;

	case AF_LINK:
	    {
		register struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

		if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 &&
		    sdl->sdl_slen == 0) {
			struct sockaddr_in *sin;
			if (Nflag && ifname && 
			    (sin=ifnametoaddr(ifname,sdl->sdl_index)))
				(void) sprintf(workbuf, "%s", 
					       routename(sin->sin_addr.s_addr,
							 width));
			else
				(void) sprintf(workbuf, "link#%d", 
					       sdl->sdl_index);
		}
		else switch (sdl->sdl_type) {
		case IFT_ETHER:
		case IFT_FDDI:
		    {
			register int i;
			register u_char *lla = (u_char *)sdl->sdl_data +
			    sdl->sdl_nlen;

			cplim = "";
			for (i = 0; i < sdl->sdl_alen; i++, lla++) {
				cp += sprintf(cp, "%s%x", cplim, *lla);
				cplim = ":";
			}
			cp = workbuf;
			break;
		    }
		default:
			cp = link_ntoa(sdl);
			break;
		}
		break;
	    }

	default:
	    {
		register u_char *s = (u_char *)sa->sa_data, *slim;

#ifdef _HAVE_SA_LEN
		slim =  sa->sa_len + (u_char *) sa;
#else
		slim =  _FAKE_SA_LEN_SRC(sa) + (u_char*)sa;
#endif
		cplim = cp + sizeof(workbuf) - 6;
#ifdef _HAVE_SA_LEN
		cp += sprintf(cp, "(%d)", sa->sa_family);
#else
		cp += sprintf(cp, "(%d)", _FAKE_SA_FAMILY(sa));
#endif
		while (s < slim && cp < cplim) {
			cp += sprintf(cp, "-%02x", *s++);
			if (s < slim)
			    cp += sprintf(cp, "%02x", *s++);
		}
		cp = workbuf;
	    }
	}
	if (width < 0 )
		printf("%s ", cp);
	else {
		if (nflag)
			printf("%-*s ", width, cp);
		else
			printf("%-*.*s ", width, width, cp);
	}
}

static void
p_flags(f, format)
	register int f;
	char *format;
{
	char name[33], *flags;
	register struct bits *p = bits;

	for (flags = name; p->b_mask; p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	printf(format, name);
}

static void
p_rtentry(register struct rtentry *rt)
{
	static struct ifnet ifnet, *lastif;
	static char name[16], ifname[26];

	/* do not print ARP entries unless asked */
	if ((rt->rt_flags & RTF_LLINFO) && !aflag)
		return;

	if (rt->rt_ifp) {
		if (rt->rt_ifp != lastif) {
			kget(rt->rt_ifp, ifnet);
#ifdef sgi
			(void)klseek(kmem,(ns_off_t)ifnet.if_name,0);
			kread(kmem, name, sizeof(name));
#else
			kread((u_long)ifnet.if_name, name, 16);
#endif
			lastif = rt->rt_ifp;
			sprintf(ifname, "%.15s%d", name, ifnet.if_unit);
		}
	}

	p_sockaddr(kgetsa(rt_key(rt)), rt->rt_flags, WID_DST, 0);
	p_sockaddr(kgetsa(rt->rt_gateway), RTF_HOST, WID_GW, ifname);
#ifdef sgi
	p_mask_hex(rt_mask(rt) ? kgetsa((struct sockaddr*)rt_mask(rt)) : 0);
#endif
	p_flags(rt->rt_flags, "%-6.6s ");
	printf("%6d %8d ", rt->rt_refcnt, rt->rt_use);
	if (rt->rt_ifp) {
		printf(" %s%s", ifname,
			rt->rt_nodes[0].rn_dupedkey ? " =>" : "");
	}
	putchar('\n');
	if (Aflag) {
#if _MIPS_SZLONG == 64
		printf("gwroute = %llx\n", rt->rt_gwroute);
#else
		printf("gwroute = %x\n", rt->rt_gwroute);
#endif
	}
}

char *
routename(__uint32_t in, int width)
{
	register char *cp;
	static char line[MAXHOSTNAMELEN + 1];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = index(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && in) {
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
		strncpy(line, cp, sizeof(line) - 1);
	else {
#define C(x)	((x) & 0xff)
		in = ntohl(in);
		sprintf(line, "%u.%u.%u.%u",
		    C(in >> 24), C(in >> 16), C(in >> 8), C(in));
	}

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
		if (!period)
			line[width] = '~';
		else
			period[1] = '\0';
	}
	return (line);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(__uint32_t in, __uint32_t mask, int width)
{
	char *cp = 0;
	static char line[MAXHOSTNAMELEN + 1];
	struct netent *np = 0;
	__uint32_t net;
	register u_int i;
	int subnetshift;

	i = ntohl(in);
	if (!nflag && i) {
		/*
		 * guess at the netmask if necessary
		 */
		if (mask == 0) {
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
			while (i &~ mask)
				mask = ((__int32_t)mask) >> subnetshift;
		}
		net = i & mask;
		while ((mask & 1) == 0)
			mask >>= 1, net >>= 1;
		np = getnetbyaddr(net, AF_INET);
		if (!np) {
			net = i & mask;
			np = getnetbyaddr(net, AF_INET);
		}
		if (np)
			cp = np->n_name;
	}
	if (cp)
		strncpy(line, cp, sizeof(line) - 1);
	else if ((i & 0xffffff) == 0)
		sprintf(line, "%u", C(i >> 24));
	else if ((i & 0xffff) == 0)
		sprintf(line, "%u.%u", C(i >> 24) , C(i >> 16));
	else if ((i & 0xff) == 0)
		sprintf(line, "%u.%u.%u", C(i >> 24), C(i >> 16), C(i >> 8));
	else
		sprintf(line, "%u.%u.%u.%u", C(i >> 24),
			C(i >> 16), C(i >> 8), C(i));

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
	return (line);
}

/*
 * Print routing statistics
 */
void
rt_stats(ns_off_t off)
{
	struct rtstat rtstat;

	if (off == 0) {
		printf("rtstat: symbol not in namelist\n");
		return;
	}
#ifdef sgi
	(void)klseek(kmem,off,0);
	kread(kmem, &rtstat, sizeof (rtstat));
#else
	kread(off, (char *)&rtstat, sizeof (rtstat));
#endif
	printf("routing:\n");
	printf("\t%u bad routing redirect%s\n",
		rtstat.rts_badredirect, plural(rtstat.rts_badredirect));
	printf("\t%u dynamically created route%s\n",
		rtstat.rts_dynamic, plural(rtstat.rts_dynamic));
	printf("\t%u new gateway%s due to redirects\n",
		rtstat.rts_newgateway, plural(rtstat.rts_newgateway));
	printf("\t%u destination%s found unreachable\n",
		rtstat.rts_unreach, plural(rtstat.rts_unreach));
	printf("\t%u use%s of a wildcard route\n",
		rtstat.rts_wildcard, plural(rtstat.rts_wildcard));
}

short ns_nullh[] = {0,0,0};
short ns_bh[] = {-1,-1,-1};

char *
ns_print(sa)
	register struct sockaddr *sa;
{
	register struct sockaddr_ns *sns = (struct sockaddr_ns*)sa;
	struct ns_addr work;
	union { union ns_net net_e; u_long long_e; } net;
	u_short port;
	static char mybuf[50], cport[10], chost[25];
	char *host = "";
	register char *p; register u_char *q;

	work = sns->sns_addr;
	port = ntohs(work.x_port);
	work.x_port = 0;
	net.net_e  = work.x_net;
	if (ns_nullhost(work) && net.long_e == 0) {
		if (port ) {
			sprintf(mybuf, "*.%xH", port);
			upHex(mybuf);
		} else
			sprintf(mybuf, "*.*");
		return (mybuf);
	}

	if (bcmp(ns_bh, work.x_host.c_host, 6) == 0) {
		host = "any";
	} else if (bcmp(ns_nullh, work.x_host.c_host, 6) == 0) {
		host = "*";
	} else {
		q = work.x_host.c_host;
		sprintf(chost, "%02x%02x%02x%02x%02x%02xH",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			continue;
		host = p;
	}
	if (port)
		sprintf(cport, ".%xH", htons(port));
	else
		*cport = 0;

	sprintf(mybuf,"%lxH.%s%s", ntohl(net.long_e), host, cport);
	upHex(mybuf);
	return(mybuf);
}

char *
ns_phost(struct sockaddr_ns *sns)
{
	struct sockaddr_ns work;
	static union ns_net ns_zeronet;
	char *p;

	work = *sns;
	work.sns_addr.x_port = 0;
	work.sns_addr.x_net = ns_zeronet;

	p = ns_print((struct sockaddr *)&work);
	if (strncmp("0H.", p, 3) == 0) p += 3;
	return(p);
}

void
upHex(p0)
	char *p0;
{
	register char *p = p0;
	for (; *p; p++) switch (*p) {

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		*p += ('A' - 'a');
	}
}


