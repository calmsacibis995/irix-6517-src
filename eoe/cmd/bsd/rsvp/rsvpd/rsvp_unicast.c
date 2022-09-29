/*
 *  @(#) $Id: rsvp_unicast.c,v 1.12 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_unicast.c  ****************************
 *                                                                   *
 *       System-dependent Unicast route lookup routine               *
 *                                                                   *
 *********************************************************************/

#include "rsvp_daemon.h"
#include <sys/param.h>
#include <sys/socket.h>
#ifdef __sgi
#include <cap_net.h>
#endif


#if defined(PF_ROUTE)  && !defined(sgi_53)

/*
 * This code will assumes a routing socket and will work on many newer
 * BSD-like machines
 *
 */

/*
 * Copyright 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <errno.h>
#include <net/if.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>

static int seq;
/*
 * For now, we open a new socket every time we want to get a route, to
 * save some effort in parsing.  Eventually we should keep it open and
 * listen for changes.  For now, just open and close it to make sure that
 * we can.
 */
int
unicast_init(void)
{
	int s;

#ifdef __sgi
	s = cap_socket(PF_ROUTE, SOCK_RAW, AF_INET);
#else
	s = socket(PF_ROUTE, SOCK_RAW, AF_INET);
#endif
	if (s < 0) {
		if (IsDebug(DEBUG_ROUTE))
			log(LOG_DEBUG, errno, 
		   		 "Unicast routing information unavailable");
		return 0;
	}
	close(s);
	return 1;
}

static int
ifname_to_if(char *name) {
	int i;
	for (i = 0; i < if_num; i++)
		if (strncmp(if_vec[i].if_name, name, IFNAMSIZ) == 0)
			return(i);
	log(LOG_ERR, 0, "Couldn't find interface %s\n", name);
	return(-1);
}


#define RTM_BUFLEN 2048		/* XXX should be in header file */
#define WORD_BNDARY(x)  (((x)+sizeof(long)-1) & ~(sizeof(long)-1))
/*
 * Find the kernel's idea of the route to destination DEST (in host order)
 */
int
unicast_route(u_long dest)
{
	int s, i;
	struct sockaddr_in sin, *sinp;
	struct rt_msghdr *mhp;
	char buf[RTM_BUFLEN], *bp;
	struct sockaddr_dl *sdlp;
	char ifname[IFNAMSIZ];
	int one, len;

	/* Open socket */
	s = cap_socket(PF_ROUTE, SOCK_RAW, PF_INET);
	if (s < 0) {
		log(LOG_ERR, errno,
		    "Could not open routing socket");
		return -1;
	}

	one = 1;
	setsockopt(s, SOL_SOCKET, SO_USELOOPBACK, &one, sizeof one);

	/* Set up GET message */
	memset(buf, 0, sizeof buf);

	mhp = (struct rt_msghdr *)buf;
	sinp = (struct sockaddr_in *)
               &buf[WORD_BNDARY(sizeof (struct rt_msghdr))];

	sinp->sin_family = AF_INET;
#ifdef SOCKADDR_LEN
	sinp->sin_len = sizeof(sin);
#endif
	sinp->sin_addr.s_addr = dest;
	sinp->sin_port = 0;

	mhp->rtm_version = RTM_VERSION;
	mhp->rtm_type = RTM_GET;
	mhp->rtm_addrs = RTA_DST|RTA_IFP;
	mhp->rtm_seq = ++seq;
	mhp->rtm_msglen = (char *)(sinp + 1) - buf;

	/* Send the message */
	if (write(s, buf, (size_t)mhp->rtm_msglen) < 0) {
		log(LOG_ERR, errno, "writing message on routing socket");
		close(s);
		return -1;
	}

	/* Read the reply */
	do {
		len = read(s, buf, sizeof buf);
	} while (len > 0 && mhp->rtm_seq != seq && mhp->rtm_pid != getpid());

	close(s);

	if (len < 0 || mhp->rtm_errno) {
		if (IsDebug(DEBUG_ROUTE))
			log(LOG_DEBUG, errno, "unicast route lookup failed");
		return -1;
	}

	if (len == 0) {
		log(LOG_ERR, 0, "unicast route lookup is confused");
		return -1;
	}

	bp = (char *)sinp;

#ifdef __sgi

/* sgi compiler really seems to dislike the ISI way of doing things.
 * This works though... mwang */

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))
#ifdef SOCKADDR_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif
	for (i = 1; i; i <<= 1)
		if (i & mhp->rtm_addrs) {
			switch (i) {
			case RTA_IFP:
				sdlp = (struct sockaddr_dl *)bp;
				break;
			default:
				break;
			}
			ADVANCE(bp, (struct sockaddr *)bp);
		}

#else /* __sgi */

#ifdef SOCKADDR_LEN
#define ADV_PAST(mask) \
	if (mhp->rtm_addrs & (mask)) \
		bp += WORD_BNDARY(((struct sockaddr_in *)bp)->sin_len)
#else
#define ADV_PAST(mask) \
      if (mhp->rtm_addrs & (mask)) \
              bp += WORD_BNDARY(sizeof(struct sockaddr))
#endif /* SOCKADDR_LEN */

	ADV_PAST(RTA_DST);	/* we already know the destination */
	ADV_PAST(RTA_GATEWAY);	/* don't care */
	ADV_PAST(RTA_NETMASK);	/* don't care */
	ADV_PAST(RTA_GENMASK);	/* don't care */
	sdlp = (struct sockaddr_dl *)bp;

#endif /* __sgi */

	if (sdlp->sdl_family != AF_LINK || sdlp->sdl_nlen == 0) {
		if (IsDebug(DEBUG_ROUTE))
			log(LOG_DEBUG, 0, "unicast route through weird ifp");
		return -1;
	}

	strncpy(ifname, sdlp->sdl_data, sdlp->sdl_nlen);
	if (sdlp->sdl_nlen < IFNAMSIZ)
		ifname[sdlp->sdl_nlen] = '\0';

	return ifname_to_if(ifname);
}
		
# else
/* 
 * ifndef PF_ROUTE - no routing socket. OS-specific code required,
 */

#ifdef SOLARIS

/* Following Solaris code was supplied by Don Hoffman of Sun Microsystems.
 */

/*
 * Copyright (c) Sun Microsystems, Inc.  1994. All rights reserved.
 * 
 * License is granted to copy, to use, and to make and to use derivative works
 * for research and evaluation purposes, provided that Sun Microsystems is
 * acknowledged in all documentation pertaining to any such copy or
 * derivative work. Sun Microsystems grants no other licenses expressed or
 * implied. The Sun Microsystems  trade name should not be used in any
 * advertising without its written permission.
 * 
 * SUN MICROSYSTEMS MERCHANTABILITY OF THIS SOFTWARE OR THE SUITABILITY OF THIS
 * SOFTWARE FOR ANY PARTICULAR PURPOSE.  The software is provided "as is"
 * without express or implied warranty of any kind.
 * 
 * These notices must be retained in any copies of any part of this software.
 */
/* Portions of this code were dereived from Solaris route.c and netstat.c */

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <inet/mib2.h>

#include "rsvp_daemon.h"

/* 
 * Defines
 */
#define MAX_ROUTES	(4 * 512)
#ifndef T_CURRENT
#define T_CURRENT       MI_T_CURRENT
#endif

/* 
 * Structure definitions
 */
typedef struct mib_item_s {
	struct mib_item_s	* next_item;
	long			group;
	long			mib_id;
	long			length;
	char			* valp;
} mib_item_t;

struct xroute {
	char ifname[IFNAMSIZ];
	struct sockaddr_in netmask;
	struct sockaddr_in out_if;
	struct sockaddr_in dst;
	struct sockaddr_in gw;
};

/*
 * Forward function declarations
 */
static mib_item_t 	*mibget (int sd);
static void		mibfree(mib_item_t*);
static int		is_host(mib2_ipRouteEntry_t *rp);
static int		is_gateway(mib2_ipRouteEntry_t *rp);

/* 
 * Globals
 */
static int 		sd;
static struct xroute	routes[MAX_ROUTES];
static struct xroute 	default_route;
static int 		have_default = 0;
static int 		nhost = 0;
static int 		nroute = 0;


int
unicast_init() {
	sd = open("/dev/ip", O_RDWR);
	if (sd == -1) {
		perror("can't open mib stream");
		return 0;
	}
	return 1;
}

int
unicast_route(u_long addr) {
	int i;
	struct sockaddr_in sin;
	if (read_routes() == -1)
		return(-1);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	i = rtfindx(&sin);
	if (i == -1 && have_default) {
		if (IsDebug(DEBUG_ROUTE)) {
			log(LOG_DEBUG, 0, "Using default route ...\n");
			log(LOG_DEBUG, 0, "route out interface %s\n", 
							default_route.ifname);
		}
		return(ifname_to_if(default_route.ifname));
	} else if (i == -1) {
		if (IsDebug(DEBUG_ROUTE))
			log(LOG_DEBUG, 0, "No route\n");
		return(-1);
	}
	if (IsDebug(DEBUG_ROUTE))
		log(LOG_DEBUG, 0, "route out interface %s\n", routes[i].ifname);
	return(ifname_to_if(routes[i].ifname));
}

static int
ifname_to_if(char *name) {
	int i;
	for (i = 0; i < if_num; i++)
		if (strncmp(if_vec[i].if_name, name, IFNAMSIZ) == 0)
			return(i);
	log(LOG_ERR, 0, "Couldn't find interface %s\n", name);
	return(-1);
}

/*
 * Find a route to dst as the kernel would.
 */
static int
rtfindx(struct sockaddr_in *dst) {
	int i;

	for (i = 0; i < nhost; i++)
		if (memcmp(&(routes[i].dst), dst, sizeof(struct sockaddr)) == 0)
			return(i);
	for (i = nhost; i < nroute; i++)
		if (net_match(dst, &routes[i]))
			return(i);
	return(-1);
}

static int
net_match(struct sockaddr_in *dst, struct xroute *rp) {
	u_long nm = rp->netmask.sin_addr.s_addr;
	if (inet_netmatch(dst, &rp->dst)) {
		u_long ip1 = dst->sin_addr.s_addr;
		u_long ip2 = rp->dst.sin_addr.s_addr;
		if (inet_netmatch(dst, &rp->out_if)) {
			if ((ip1 & nm) == ip2)
				return(1);
			else
				return(0);
		} else
			return(1);
	}
	return(0);
}

static int
inet_netmatch(struct sockaddr_in *sin1, struct sockaddr_in *sin2) {
	return (inet_netof(sin1->sin_addr) == inet_netof(sin2->sin_addr));
}

static int
read_routes() {
	mib_item_t		*item;
	mib_item_t 		*first_item = nilp(mib_item_t);
	mib2_ipRouteEntry_t	*rp;
	struct rtentry 		rt;
	struct sockaddr_in 	*sin;
	int			ret_code = 1;
	int			doinghost;
	int			i;
	

	/* Get the route list. */
	/* TBD: don't need to do this every time.  Only to a new mibget every
	 *  X seconds? */
	if ((first_item = mibget(sd)) == nilp(mib_item_t)) {
		close(sd);
		ret_code = -1;
		goto leave;
	}
	
	/* Look up the entry */
	have_default = 0;
	nroute = 0;
	nhost = 0;
	
	/* host routes first */
	doinghost = 1;
again:
	for ( item = first_item ; item ; item = item->next_item ) {
		/* skip all the other trash that comes up the mib stream */
		if ((item->group != MIB2_IP) || (item->mib_id != MIB2_IP_21))
			continue;
		
		rp = (mib2_ipRouteEntry_t *)item->valp;
		for(;(u_long)rp < (u_long)(item->valp + item->length); rp++){
			/* skip routes that aren't what we want this pass */
			if(doinghost) {
				if(!is_host(rp))
					continue;
			}
			else {
				if(!is_gateway(rp))
					continue;
			}
			
			/* fill in the blanks */
			memset(&routes[nroute], 0, sizeof(struct xroute));
			routes[nroute].dst.sin_family = AF_INET;
			routes[nroute].dst.sin_addr.s_addr = rp->ipRouteDest;
			routes[nroute].gw.sin_family = AF_INET;
			routes[nroute].gw.sin_addr.s_addr = rp->ipRouteNextHop;
			routes[nroute].netmask.sin_family = AF_INET;
			routes[nroute].netmask.sin_addr.s_addr = 
				rp->ipRouteMask;
			routes[nroute].out_if.sin_family = AF_INET;
			routes[nroute].out_if.sin_addr.s_addr = 
				rp->ipRouteInfo.re_src_addr;
				
			if(rp->ipRouteIfIndex.o_length >= IFNAMSIZ)
				rp->ipRouteIfIndex.o_length = IFNAMSIZ - 1;
			for(i=0; i<rp->ipRouteIfIndex.o_length; i++)
				routes[nroute].ifname[i] = 
					rp->ipRouteIfIndex.o_bytes[i];
			routes[nroute].ifname[i] = '\0';
			
			if (routes[nroute].dst.sin_addr.s_addr == INADDR_ANY) {
				default_route = routes[nroute];
				have_default = 1;
			} else
				nroute++;

		}
	}
	
	/* net routes next */
	if (doinghost) {
		nhost = nroute;
		doinghost = 0;
		goto again;
	}
leave:
	mibfree(first_item);
	first_item = nilp(mib_item_t);
	return ret_code; 
}

static int
is_host(mib2_ipRouteEntry_t *rp)
{
	if (rp->ipRouteMask == (IpAddress)-1) 
		return 1;
	else
		return 0;
}

static int
is_gateway(mib2_ipRouteEntry_t *rp)
{
	if (rp->ipRouteInfo.re_ire_type == IRE_GATEWAY ||
	    rp->ipRouteInfo.re_ire_type == IRE_NET ||
	    rp->ipRouteInfo.re_ire_type == IRE_ROUTE_ASSOC ||
	    rp->ipRouteInfo.re_ire_type == IRE_ROUTE_REDIRECT)
		return 1;
	else
		return 0;
}



static mib_item_t * 
mibget (int sd)
{
	static char		buf[1024 * 24];
	int			flags;
	int			i, j, getcode;
	struct strbuf		ctlbuf, databuf;
	struct T_optmgmt_req	* tor = (struct T_optmgmt_req *)buf;
	struct T_optmgmt_ack	* toa = (struct T_optmgmt_ack *)buf;
	struct T_error_ack	* tea = (struct T_error_ack *)buf;
	struct opthdr		* req;
	mib_item_t		* first_item = nilp(mib_item_t);
	mib_item_t		* last_item  = nilp(mib_item_t);
	mib_item_t		* temp;
	
	tor->PRIM_type = T_OPTMGMT_REQ;
	tor->OPT_offset = sizeof(struct T_optmgmt_req);
	tor->OPT_length = sizeof(struct opthdr);
	tor->MGMT_flags = T_CURRENT;
	req = (struct opthdr *)&tor[1];
	req->level = MIB2_IP;		/* any MIB2_xxx value ok here */
	req->name  = 0;
	req->len   = 0;

	ctlbuf.buf = buf;
	ctlbuf.len = tor->OPT_length + tor->OPT_offset;
	flags = 0;
	if (putmsg(sd, &ctlbuf, nilp(struct strbuf), flags) == -1) {
		perror("mibget: putmsg(ctl) failed");
		goto error_exit;
	}
	/*
	 * each reply consists of a ctl part for one fixed structure
	 * or table, as defined in mib2.h.  The format is a T_OPTMGMT_ACK,
	 * containing an opthdr structure.  level/name identify the entry,
	 * len is the size of the data part of the message.
	 */
	req = (struct opthdr *)&toa[1];
	ctlbuf.maxlen = sizeof(buf);
	for (j=1; ; j++) {
		flags = 0;
		getcode = getmsg(sd, &ctlbuf, nilp(struct strbuf), &flags);
		if (getcode == -1) {
			perror("mibget getmsg(ctl) failed");
			goto error_exit;
		}
		if (getcode == 0
		&& ctlbuf.len >= sizeof(struct T_optmgmt_ack)
		&& toa->PRIM_type == T_OPTMGMT_ACK
		&& toa->MGMT_flags == T_SUCCESS
		&& req->len == 0) {
			return first_item;		/* this is EOD msg */
		}

		if (ctlbuf.len >= sizeof(struct T_error_ack)
		&& tea->PRIM_type == T_ERROR_ACK) {
			errno = (tea->TLI_error == TSYSERR)
				? tea->UNIX_error : EPROTO;
			goto error_exit;
		}
			
		if (getcode != MOREDATA
		|| ctlbuf.len < sizeof(struct T_optmgmt_ack)
		|| toa->PRIM_type != T_OPTMGMT_ACK
		|| toa->MGMT_flags != T_SUCCESS) {
			if (toa->PRIM_type == T_OPTMGMT_ACK)
			errno = ENOMSG;
			goto error_exit;
		}

		temp = (mib_item_t *)malloc(sizeof(mib_item_t));
		if (!temp) {
			perror("mibget malloc failed");
			goto error_exit;
		}
		if (last_item)
			last_item->next_item = temp;
		else
			first_item = temp;
		last_item = temp;
		last_item->next_item = nilp(mib_item_t);
		last_item->group = req->level;
		last_item->mib_id = req->name;
		last_item->length = req->len;
		last_item->valp = (char *)malloc(req->len);

		databuf.maxlen = last_item->length;
		databuf.buf    = last_item->valp;
		databuf.len    = 0;
		flags = 0;
		getcode = getmsg(sd, nilp(struct strbuf), &databuf, &flags);
		if (getcode == -1) {
			perror("mibget getmsg(data) failed");
			goto error_exit;
		} else if (getcode != 0) {
			goto error_exit;
		}
	}

error_exit:;
	while (first_item) {
		last_item = first_item;
		first_item = first_item->next_item;
		free(last_item);
	}
	return first_item;
}

static void
mibfree(mib_item_t *first_item)
{
	mib_item_t	*last_item;
	
	while(first_item) {
		last_item = first_item;
		first_item = first_item->next_item;
		free(last_item);
	}
}

#else /* end of SOLARIS */

#ifdef sgi_53

/* Do not have support for access to unicast routing table in IRIX 5.3.
 */
unicast_init()
	{
#ifdef HOST_ONLY
	return(0);
#else
	return(1);
#endif
}

int
unicast_route(u_long dest)
{
	/*  This dummy routine is here to allow pre-routing-socket code to
	 *  compile.  It should never be called.
	 */
	assert(0);
	return(0);
}

#else  /* end sgi_53 */

/*	Following code should work for many systems derived from
 *	older BSDs, but it is known to work for Sun OS.
 */

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <stdio.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <netdb.h>

#include <assert.h>

#include <syslog.h>

#include "rsvp_daemon.h"

struct nlist nl[] = {
#define	N_RTHOST	0
	{ "_rthost" },
#define	N_RTNET		1
	{ "_rtnet" },
#define N_RTHASHSIZE	2
	{ "_rthashsize" },
	{""}
};

kvm_t	*kd;
char	usage[] = "[-n] host";

struct xroute {
	char ifname[IFNAMSIZ];
	struct sockaddr_in netmask;
	struct sockaddr_in out_if;
	struct sockaddr_in dst;
	struct sockaddr_in gw;
} routes[256];

int nhost = 0;
int nroute = 0;

struct xroute default_route;
int have_default = 0;

int unicast_fd;
int init_failed = 0;

/* forward declarations */
int read_routes();
int net_match(struct sockaddr_in *, struct xroute *);
int inet_netmatch(struct sockaddr_in *, struct sockaddr_in *);
int rtfindx(struct sockaddr_in *);
int ifname_to_if(char *);
int kread(unsigned long addr, char *buf, unsigned nbytes);

int
unicast_init() {
	if ((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, "VM")) == NULL) {
		log(LOG_ERR, 0, "Unable to read kernel VM\n");
		init_failed = 1;
		return(0);
	}
	if (kvm_nlist(kd, nl) < 0) {
		log(LOG_ERR, 0, "netstat: bad namelist\n");
		init_failed = 1;
		return(0);
	}
	unicast_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (unicast_fd < 0) {
		log(LOG_ERR, errno, "unicast socket");
		init_failed = 1;
		return(0);
	}
	return(1);
}

int
unicast_route(u_long addr) {
	int i;
	struct sockaddr_in sin;
	if (init_failed)
		return(-1);
	if (read_routes() == -1)
		return(-1);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	i = rtfindx(&sin);
	if (i == -1 && have_default) {
		if (IsDebug(DEBUG_ROUTE)) {
			log(LOG_DEBUG, 0, "Using default route ...\n");
			log(LOG_DEBUG, 0, "route out interface %s\n", 
						default_route.ifname);
		}
		return(ifname_to_if(default_route.ifname));
	} else if (i == -1) {
		if (IsDebug(DEBUG_ROUTE))
			log(LOG_DEBUG, 0, "No route\n");
		return(-1);
	}
	if (IsDebug(DEBUG_ROUTE))
		log(LOG_DEBUG, 0, "route out interface %s\n", routes[i].ifname);
	return(ifname_to_if(routes[i].ifname));
}

int
ifname_to_if(char *name) {
	int i;
	for (i = 0; i < if_num; i++)
		if (strncmp(if_vec[i].if_name, name, IFNAMSIZ) == 0)
			return(i);
	log(LOG_ERR, 0, "Couldn't find interface %s\n", name);
	return(-1);
}
int
read_routes() {
	struct mbuf mbf, *mbfp = &mbf;
	register struct mbuf *m;
	struct mbuf **rhash;
	int i, doinghost = 1;
	int hashsize;
	struct ifreq ifrbuf, *ifr = &ifrbuf;
	int status;

	kread(nl[N_RTHASHSIZE].n_value, (char *)&hashsize, sizeof (hashsize));
	rhash = (struct mbuf **)malloc( hashsize*sizeof (struct mbuf *) );
	kread(nl[N_RTHOST].n_value, (char *)rhash, hashsize*sizeof (struct mbuf *));
	nroute = 0;
again:
	for (i = 0; i < hashsize; i++) {
		if (rhash[i] == 0)
			continue;
		m = rhash[i];
		while (m) {
			struct ifnet ifz, *ifp = &ifz;
			struct rtentry *rt;
			struct ifaddr ifx, *ifxp = &ifx;
			char ifbuf[IFNAMSIZ];

			kread((u_long)m, (char *)mbfp, sizeof(struct mbuf));
			rt = mtod(mbfp, struct rtentry *);
			assert(rt->rt_ifp);
			routes[nroute].dst = *(struct sockaddr_in *)&rt->rt_dst;
			routes[nroute].gw = *(struct sockaddr_in *)&rt->rt_gateway;
			kread((u_long)rt->rt_ifp, (char *)ifp, sizeof(struct ifnet));
			assert(ifp->if_addrlist);
			kread((u_long)ifp->if_addrlist, (char *)ifxp, sizeof(struct ifaddr));
			assert(ifxp->ifa_addr.sa_family == AF_INET);
			routes[nroute].out_if = *(struct sockaddr_in *)&ifx.ifa_addr;
			kread((u_long)ifp->if_name, ifbuf, IFNAMSIZ);
			sprintf(routes[nroute].ifname, "%s%d", ifbuf, 
				ifp->if_unit);
			memcpy(ifr->ifr_name, routes[nroute].ifname, IFNAMSIZ);
			status = ioctl(unicast_fd, SIOCGIFNETMASK, ifr);
			if (status < 0) {
				perror("ioctl");
				return(-1);
			}
			routes[nroute].netmask = *(struct sockaddr_in *)&ifr->ifr_addr;
			if (routes[nroute].dst.sin_addr.s_addr == INADDR_ANY) {
				default_route = routes[nroute];
				have_default = 1;
			} else
				nroute++;
			assert(nroute < 256);
			m = mbfp->m_next;
		}
	}
	if (doinghost) {
		kread(nl[N_RTNET].n_value, (char *)rhash, hashsize*sizeof (struct mbuf *));
		nhost = nroute;
		doinghost = 0;
		goto again;
	}
	free(rhash);
	return(1);
}

int
kread(unsigned long addr, char *buf, unsigned nbytes) {
	return kvm_read(kd, addr, buf, nbytes);
}

/*
 * Find a route to dst as the kernel would.
 */
int
rtfindx(struct sockaddr_in *dst) {
	int i;

	for (i = 0; i < nhost; i++)
		if (memcmp(&routes[i].dst, dst, sizeof(struct sockaddr)) == 0)
			return(i);
	for (i = nhost; i < nroute; i++)
		if (net_match(dst, &routes[i]))
			return(i);
	return(-1);
}

int
net_match(struct sockaddr_in *dst, struct xroute *rp) {
	u_long nm = rp->netmask.sin_addr.s_addr;
	if (inet_netmatch(dst, &rp->dst)) {
		u_long ip1 = dst->sin_addr.s_addr;
		u_long ip2 = rp->dst.sin_addr.s_addr;
		if (inet_netmatch(dst, &rp->out_if)) {
			if ((ip1 & nm) == ip2)
				return(1);
			else
				return(0);
		} else
			return(1);
	}
	return(0);
}

int
inet_netmatch(struct sockaddr_in *sin1, struct sockaddr_in *sin2) {
	return (inet_netof(sin1->sin_addr) == inet_netof(sin2->sin_addr));
}
#endif /* ! sgi */
#endif /* ! SOLARIS */
#endif /* ! PF_ROUTE */
