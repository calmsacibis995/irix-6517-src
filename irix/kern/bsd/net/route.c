/*
 * Copyright (c) 1980, 1986, 1991, 1993
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
 *
 *	@(#)route.c	8.2 (Berkeley) 11/15/93
 */

#ident "$Revision: 4.47 $"

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"
#include "sys/atomic_ops.h"
#include "sys/sysctl.h"

#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/kabi.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"	/* XXX for NETSPL */
#include "sys/domain.h"
#include "sys/protosw.h"
#include "sys/systm.h"
#include "sys/capability.h"
#include "sys/kmem.h"
#include "sys/ddi.h"
#include "soioctl.h"
#include "ksoioctl.h"
#include "sys/xlate.h"
#include "bstring.h"
#include "sys/cmn_err.h"

#include "if.h"
#include "bsd/netinet/in.h"
#include "bsd/netinet/in_var.h"
#include "route.h"

mrlock_t route_lock;
#include "sys/pda.h"

/*
 * Externally defined procedures
 */
extern void ifafree(struct ifaddr *);
extern struct ifaddr *ifa_ifwithaddr(struct sockaddr *);
extern struct ifaddr *ifa_ifwithdstaddr(struct sockaddr *);
extern struct ifaddr *ifa_ifwithnet(struct sockaddr *);
extern int mrt_ioctl(int cmd, caddr_t data);

#define	SA(p) ((struct sockaddr *)(p))

/*
 * Modules global variables
 */
struct	rtstat rtstat;
struct	radix_node_head *rt_tables[AF_MAX+1];
int	rttrash;		/* routes not in table but not freed */

void
rtable_init(void **table)
{
	struct domain *dom;
	for (dom = domains; dom; dom = dom->dom_next)
		if (dom->dom_rtattach)
			dom->dom_rtattach(&table[dom->dom_family],
			    dom->dom_rtoffset);
	return;
}

void
route_init()
{
	rn_init();	/* initialize all zeroes, all ones, mask table */
	rtable_init((void **)rt_tables);
	return;
}

/*
 * Packet routing routines.
 */
void
rtalloc(register struct route *ro)
{
	ASSERT(ROUTE_ISLOCKED());
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP)) {
		return;				 /* XXX */
	}
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1);
	return;
}

int
rtrequest_promote(void)
{
	int promoted;

	if (ROUTE_ISWRLOCKED()) {
		promoted = 0;
	} else {
		ASSERT(ROUTE_ISRDLOCKED());
		promoted = 1;
		if (!mrtrypromote(&route_lock)) {
			ROUTE_UNLOCK();
			ROUTE_WRLOCK();
		}
	}
	return promoted;
}

void
rtrequest_demote(int promoted)
{
	if (promoted) {
		ROUTE_DEMOTE();
	}
}

struct rtentry *
rtalloc1(register struct sockaddr *dst, int report)
{
#ifdef _HAVE_SA_LEN
	register struct radix_node_head *rnh = rt_tables[dst->sa_family];
#else
	register struct radix_node_head *rnh = rt_tables[_FAKE_SA_FAMILY(dst)];
#endif
	register struct rtentry *rt;
	register struct radix_node *rn;
	struct rtentry *newrt = 0;
	int promoted;
	struct rt_addrinfo info;
	int  err = 0, msgtype = RTM_MISS;

	ASSERT(ROUTE_ISLOCKED());
retry:
	if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING)) {
			promoted = rtrequest_promote();
			err = rtrequest(RTM_RESOLVE, dst, 0, 0, 0, &newrt);
			if (err) {
#ifdef DEBUG
				printf("rtalloc1: rtrequest(RTM_RESOLVE) "
					"error %d\n", err);
#endif /* DEBUG */
				if (err == EEXIST) {
					/*
					 * Raced with somebody else; retry.
					 * Haven't held route yet, no need to
					 * release.  Drop lock back to read
					 * mode.
					 */
					rtrequest_demote(promoted);
					newrt = 0;
					goto retry;
				}
				newrt = rt;
				RT_HOLD(rt);
				rtrequest_demote(promoted);
				goto miss;
			}
			if ((rt = newrt) && (rt->rt_flags & RTF_XRESOLVE)) {
#ifdef DEBUG
				printf("rtalloc1: RTF_XRESOLVE\n");
#endif /* DEBUG */
				msgtype = RTM_RESOLVE;
				rtrequest_demote(promoted);
				goto miss;
			}
			rtrequest_demote(promoted);
		} else
			RT_HOLD(rt);
	} else {
		rtstat.rts_unreach++;
	miss:	if (report) {
			bzero((caddr_t)&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			rt_missmsg(msgtype, &info, 0, err);
		}
	}
	return (newrt);
}

/*
 * Various fast-path rtfree routines that don't take the write lock unless
 * the route will be released
 */
void
rtfree_needlock(register struct rtentry *rt)
{
	ROUTE_RDLOCK();
	ASSERT(rt);
	/* only decrement refcnt if there is more than 1 reference to route.
	   otherwise, route could be freed between UNLOCK/WRLOCK below */
	if (!RT_CMPANDRELE(rt)) {
		/* refcnt <= 1.  refcnt not decremented */
		ROUTE_UNLOCK();
		ROUTE_WRLOCK();
		rtfree(rt);
		ROUTE_UNLOCK();
	} else {
		ROUTE_UNLOCK();
	}
}

void
rtfree_needpromote(register struct rtentry *rt)
{
	int promoted = 0;

	ASSERT(rt);
	ASSERT(ROUTE_ISRDLOCKED());
	/* only decrement refcnt if there is more than 1 reference to route.
	   otherwise, route could be freed between UNLOCK/WRLOCK below */
	if (!RT_CMPANDRELE(rt)) {
		/* refcnt <= 1.  refcnt not decremented */
		if (mrtrypromote(&route_lock)) {
			promoted = 1;
		} else {
			ROUTE_UNLOCK();
			ROUTE_WRLOCK();
		}
		rtfree(rt);
		if (promoted) {
			ROUTE_DEMOTE();
		} else {
			ROUTE_UNLOCK();
			ROUTE_RDLOCK();
		}
	}
}

void
rtfree(register struct rtentry *rt)
{
	register struct ifaddr *ifa;

	ASSERT(ROUTE_ISWRLOCKED());
	if (rt == 0)
		panic("rtfree");

	RT_RELE(rt);

	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtfree 2");
		rttrash--;
		if (rt->rt_refcnt < 0) {
			printf("rtfree: %x not freed (neg refs)\n", rt);
			return;
		}
		ifa = rt->rt_ifa;
		ifafree(ifa);
		Free(rt_key(rt));
		Free(rt);
	}
	return;
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 *
 * N.B.: must be called at splnet
 */
void
rtredirect(struct sockaddr *dst,
	   struct sockaddr *gateway,
#ifdef _HAVE_SA_LEN
	   struct sockaddr *netmask,
#else
	   struct sockaddr_new *netmask,
#endif
	   int flags,
	   struct sockaddr *src,
	   struct rtentry **rtp)
{
	register struct rtentry *rt;
	int error = 0;
	short *stat = 0;
	struct rt_addrinfo info;
	struct ifaddr *ifa;

	ROUTE_WRLOCK();
	/* verify the gateway is directly reachable */
	if ((ifa = ifa_ifwithnet(gateway)) == 0) {
		error = ENETUNREACH;
		goto out;
	}
	rt = rtalloc1(dst, 0);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#ifdef _HAVE_SA_LEN
#define	equal(a1, a2) (bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
#else
#define	equal(a1, a2) (bcmp((a1), (a2), _FAKE_SA_LEN_SRC(a1)) == 0)
#endif
	if (!(flags & RTF_DONE) && rt &&
	     (!equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway))
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the the lookup failed.  This is necessary for hosts
	 * which use routing redirects generated by smart gateways
	 * to dynamically build the routing tables.
	 */
	if ((rt == 0) || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
		goto create;
	/*
	 * Don't listen to the redirect if it's
	 * for a route to an interface. 
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)) {
			/*
			 * Changing from route to net => route to host.
			 * Create new route, rather than smashing route to net.
			 */
		create:
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			flags &= ~RTF_STATIC;	/* ensure non-static */
			error = rtrequest((int)RTM_ADD, dst, gateway,
				    netmask, flags,
				    (struct rtentry **)0);
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			rt->rt_flags |= RTF_MODIFIED;
			rt->rt_flags &= ~RTF_STATIC;	/* not static anymore */
			flags |= RTF_MODIFIED;
			stat = &rtstat.rts_newgateway;
			rt_setgate(rt, rt_key(rt), gateway);
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt) {
		if (rtp && !error)
			*rtp = rt;
		else
			rtfree(rt);
	}
out:
	if (error)
		rtstat.rts_badredirect++;
	else if (stat != NULL)
		(*stat)++;
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
#ifdef _HAVE_SA_LEN
	info.rti_info[RTAX_NETMASK] = netmask;
#else
	info.rti_info[RTAX_NETMASK] = (struct sockaddr *)netmask;
#endif
	info.rti_info[RTAX_AUTHOR] = src;
	rt_missmsg(RTM_REDIRECT, &info, flags, error);
	ROUTE_UNLOCK();
	return;
}

#if _MIPS_SIM == _ABI64
static struct rtsysctl *
irix5_to_rtsysctl(struct rtsysctl *user_s, struct rtsysctl *native_s)
{
	struct irix5_rtsysctl *i5_s;
	char abi = get_current_abi();

	if (ABI_IS_IRIX5_64(abi))
		return user_s;

	ASSERT(ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi));

	i5_s = (struct irix5_rtsysctl *)user_s;

	native_s->name = (__int32_t*)(__psint_t)i5_s->name;
	native_s->namelen = i5_s->namelen;
	native_s->oldp = (void*)(__psint_t)i5_s->oldp;
	native_s->oldlen = i5_s->oldlen;
	native_s->newp = (void*)(__psint_t)i5_s->newp;
	native_s->newlen = i5_s->newlen;

	return native_s;
}


static struct rtsysctl *
rtsysctl_to_irix5(struct rtsysctl *native_s, struct rtsysctl *user_s)
{
	struct irix5_rtsysctl *i5_s;
	char abi = get_current_abi();

	if (ABI_IS_IRIX5_64(abi))
		return native_s;

	ASSERT(ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi));

	i5_s = (struct irix5_rtsysctl *)user_s;

	i5_s->oldlen = native_s->oldlen;

	return user_s;
}
#endif /* _ABI64 */

/*
 * Routing table ioctl interface.
 */
int
rtioctl(int req, caddr_t data)
{
	int error;
	size_t oldlen;
	caddr_t oldp;
	struct rtsysctl *uap = (struct rtsysctl*)data;
	int res;
#if _MIPS_SIM == _ABI64
	struct rtsysctl native_rtsysctl;
#endif
	int name[CTL_MAXNAME];


	/* kludge to make sysctl(3) work for routing sockets
	 */

#if _MIPS_SIM == _ABI64
	if (req != _SIOCRTSYSCTL && req != IRIX5_SIOCRTSYSCTL)
		return (mrt_ioctl(req, data));
	XLATE_FROM_IRIX5(irix5_to_rtsysctl, uap, &native_rtsysctl);
#else
	if (req != _SIOCRTSYSCTL)
		return (mrt_ioctl(req, data));
#endif

	if (uap->namelen < 2 || uap->namelen > CTL_MAXNAME)
		return (EINVAL);

	if (uap->newp != 0 || uap->newlen != 0)	/* no changing */
		return (EPERM);

	error = copyin(uap->name, name, uap->namelen*sizeof(name[0]));
	if (error)
		return (error);

	if (name[0] != CTL_NET)
		return (EOPNOTSUPP);

	/*
	 * Routing table or interfaces
	 */
	if (uap->namelen == 6 && name[1] == PF_ROUTE && name[2] == 0) {
		/* 4.4BSD locks the user buffer into memory for the duration,
		 * and also locks the data structures.  It uses copyout()
		 * to put the answers into the user buffer while the data
		 * structures are locked.  Page faults, such as can happen
		 * during copyout(), are a no-no in IRIX if you have things
		 * locked.
		 * Instead of doing that, allocate a kernel buffer, lock the
		 * data structures, copy the data into the kernel buffer,
		 * release the data structures, and then copyout() the data.
		 */
		oldp = 0;
		oldlen = 0;
		if (uap->oldp != 0) {
			if ((oldlen = uap->oldlen) <= 0)
				return (EINVAL);
			oldp = (caddr_t)kmem_zalloc(oldlen, KM_SLEEP);
			if (!oldp)
				return (ENOMEM);
		}
		error = sysctl_rtable(&name[3], uap->namelen-3,
				      oldp, &uap->oldlen, 0,0);
#if _MIPS_SIM == _ABI64
		XLATE_TO_IRIX5(rtsysctl_to_irix5,(struct rtsysctl *)data,uap);
#endif
		if (!error && oldlen != 0 && uap->oldlen != 0)
			error = copyout(oldp, uap->oldp,
					MIN(oldlen,uap->oldlen));
		if (oldp)
			kmem_free(oldp,oldlen);
		return (error);
	}

	if (uap->namelen == 4 && name[1] == AF_INET) {
		switch (name[2]) {
		case IPPROTO_UDP:
			switch (name[3]) {
			case UDPCTL_CHECKSUM:
				res = 1;    /* checksums no longer optional */
				break;
			default:
				return (EOPNOTSUPP);
			}
			break;

		case IPPROTO_IP:
			switch (name[3]) {
				extern int ipforwarding, ipsendredirects;
				
			case IPCTL_FORWARDING:
				res = ipforwarding;
				break;
			case IPCTL_SENDREDIRECTS:
				res = ipsendredirects;
				break;
			default:
				return (EOPNOTSUPP);
			}
			break;

		default:
			return (EOPNOTSUPP);
		}

		if (uap->oldp != 0 && uap->oldlen < sizeof(res))
			return (ENOMEM);
		if (uap->oldlen != 0)
			error = copyout(&res, uap->oldp, sizeof(res));
		uap->oldlen = sizeof(res);
#if _MIPS_SIM == _ABI64
		XLATE_TO_IRIX5(rtsysctl_to_irix5,(struct rtsysctl *)data,uap);
#endif
		return (error);
	}

	return (EOPNOTSUPP);
}


struct ifaddr *
ifa_ifwithroute(int flags, struct sockaddr *dst, struct sockaddr *gateway)
{
	register struct ifaddr *ifa;
	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = 0;
		if (flags & RTF_HOST) 
			ifa = ifa_ifwithdstaddr(dst);
		if (ifa == 0)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == 0)
		ifa = ifa_ifwithnet(gateway);
	if (ifa == 0) {
		struct rtentry *rt = rtalloc1(dst, 0);
		if (rt == 0)
			return (0);
		RT_RELE(rt);
		if ((ifa = rt->rt_ifa) == 0)
			return (0);
	}
#ifdef _HAVE_SA_LEN
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
#else
	if (ifa->ifa_addr->sa_family != _FAKE_SA_FAMILY(dst)) {
#endif
		struct ifaddr *oifa = ifa;
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == 0)
			ifa = oifa;
	}
	return (ifa);
}

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))

int
rtrequest(int req,
	struct sockaddr *dst,
	struct sockaddr *gateway,
#ifdef _HAVE_SA_LEN
	struct sockaddr *netmask,
#else
	struct sockaddr_new *netmask,
#endif
	int flags,
	struct rtentry **ret_nrt)
{
	int error = 0;
	register struct rtentry *rt;
	register struct radix_node *rn;
	register struct radix_node_head *rnh;
	struct ifaddr *ifa;
	struct sockaddr *ndst;
#define senderr(x) { error = x ; goto bad; }

	ASSERT(ROUTE_ISWRLOCKED());
#ifdef _HAVE_SA_LEN
	if ((rnh = rt_tables[dst->sa_family]) == 0)
#else
	if ((rnh = rt_tables[_FAKE_SA_FAMILY(dst)]) == 0)
#endif
		senderr(ESRCH);
	if (flags & RTF_HOST)
		netmask = 0;
	switch (req) {
	case RTM_DELETE:
		if ((rn = rnh->rnh_deladdr(dst, netmask, rnh)) == 0)
			senderr(ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");
		rt = (struct rtentry *)rn;
		if (rt->rt_gwroute) {
			rt = rt->rt_gwroute; RTFREE(rt);
			(rt = (struct rtentry *)rn)->rt_gwroute = 0;
		}
		rt->rt_flags &= ~RTF_UP;
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, SA(0));
		rttrash++;
		if (ret_nrt)
			*ret_nrt = rt;
		else if (rt->rt_refcnt <= 0) {

			RT_HOLD(rt);
			rtfree(rt);
		}
		break;

	case RTM_RESOLVE:
		if (ret_nrt == 0 || (rt = *ret_nrt) == 0)
			senderr(EINVAL);
		ifa = rt->rt_ifa;
		flags = rt->rt_flags & ~RTF_CLONING;
		gateway = rt->rt_gateway;
		if ((netmask = rt->rt_genmask) == 0)
			flags |= RTF_HOST;
		goto makeroute;

	case RTM_ADD:
		if ((ifa = ifa_ifwithroute(flags, dst, gateway)) == 0)
			senderr(ENETUNREACH);
	makeroute:
		R_Malloc(rt, struct rtentry *, sizeof(*rt));
		if (rt == 0)
			senderr(ENOBUFS);
		Bzero(rt, sizeof(*rt));
		rt->rt_flags = RTF_UP | flags;
		if (rt_setgate(rt, dst, gateway)) {
			Free(rt);
			senderr(ENOBUFS);
		}
		ndst = rt_key(rt);
		if (netmask) {
			rt_maskedcopy(dst, ndst, netmask);
		} else
#ifdef _HAVE_SA_LEN
			Bcopy(dst, ndst, dst->sa_len);
#else
			bcopy(dst, ndst, _FAKE_SA_LEN_DST(dst));
#endif
		rn = rnh->rnh_addaddr((caddr_t)ndst, (caddr_t)netmask,
					rnh, rt->rt_nodes);
		if (rn == 0) {
			Free(rt_key(rt));
			Free(rt);
			senderr(EEXIST);
		}
		atomicAddUint(&(ifa->ifa_refcnt), 1);

		rt->rt_ifa = ifa;
		rt->rt_srcifa = 0;
		if ((flags & RTF_HOSTALIAS) == 0) {
			/*
			 * If this is a route to one of our non-loopback
			 * addresses, via the loopback, then use the
			 * real address as the source
			 */
			if ((ifa->ifa_ifp->if_flags & IFF_LOOPBACK) &&
			    (rt->rt_srcifa = ifa_ifwithaddr(ndst))) {
				;
			}
		}

		rt->rt_ifp = ifa->ifa_ifp;
		if (req == RTM_RESOLVE)
			rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */
		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt :0));
		if (ret_nrt) {
			*ret_nrt = rt;
			RT_HOLD(rt);
		}
		break;
	}
bad:
	return (error);
}

int
rt_setgate(struct rtentry *rt0, struct sockaddr *dst, struct sockaddr *gate)
{
	caddr_t new, old;
#ifdef _HAVE_SA_LEN
	int dlen = ROUNDUP(dst->sa_len), glen = ROUNDUP(gate->sa_len);
#else
	int dlen = ROUNDUP(_FAKE_SA_LEN_DST(dst));
	int glen = ROUNDUP(_FAKE_SA_LEN_DST(gate));
#endif
	register struct rtentry *rt = rt0;

	ASSERT(ROUTE_ISLOCKED());
#ifdef _HAVE_SA_LEN
	if (rt->rt_gateway == 0 || glen > ROUNDUP(rt->rt_gateway->sa_len)) {
#else
/* } */if (rt->rt_gateway == 0
	   || glen > ROUNDUP(_FAKE_SA_LEN_DST(rt->rt_gateway))) {
#endif
		old = (caddr_t)rt_key(rt);
		R_Malloc(new, caddr_t, dlen + glen);
		if (new == 0)
			return 1;
		rt->rt_nodes->rn_key = new;
	} else {
		new = rt->rt_nodes->rn_key;
		old = 0;
	}
	Bcopy(gate, (rt->rt_gateway = (struct sockaddr *)(new + dlen)), glen);
	if (old) {
		Bcopy(dst, new, dlen);
		Free(old);
	}
	if (rt->rt_gwroute) {
		rt0 = rt->rt_gwroute; rt->rt_gwroute = 0;
		rt = rt0; RTFREE(rt);
	}
	return 0;
}

void
rt_maskedcopy(struct sockaddr *src,
	      struct sockaddr *dst,
#ifdef _HAVE_SA_LEN
	      struct sockaddr *netmask
#else
	      struct sockaddr_new *netmask
#endif
	      )
{
	register u_char *cp1 = (u_char *)src;
	register u_char *cp2 = (u_char *)dst;
	register u_char *cp3 = (u_char *)netmask;
#ifdef _HAVE_SA_LEN
	u_char *cplim = cp2 + *cp3;
	u_char *cplim2 = cp2 + *cp1;
#else
	u_char *cplim = cp2 + _FAKE_SA_LEN_SRC(netmask);
	u_char *cplim2 = cp2 + _FAKE_SA_LEN_DST(src);
#endif

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;
	if (cp2 < cplim2)
		bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}

/*
 * Set up a routing table entry, normally for an interface.
 */
int
rtinit(register struct ifaddr *ifa, int cmd, int flags)
{
	register struct rtentry *rt;
	register struct sockaddr *dst;
	register struct sockaddr *deldst;
	struct mbuf *m = 0;
	struct rtentry *nrt = 0;
	int error;

	ROUTE_WRLOCK();
	dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;

	if (cmd == RTM_DELETE) {
		if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
			m = m_get(M_WAIT, MT_SONAME);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
			dst = deldst;
		}
		if (rt = rtalloc1(dst, 0)) {
			RT_RELE(rt);

			/*
			 * Check if this is a host route AND it's the
			 * host route for an IP alias address. In that case
			 * we skip the more stringent address check where
			 * we must match the structure 'ifaddr' adddress
			 * which was used to create the route. The reason
			 * being that since we install by default a host
			 * route for IP alias address the 'ifaddr' in the
			 * routing table entry has the 'ifaddr' for the
			 * primary address of the interface and NOT the
			 * alias address. Hence without this check the
			 * host routes for alias'es don't get deleted.
			 */
			if ((rt->rt_flags & (RTF_HOST|RTF_HOSTALIAS)) ==
				(RTF_HOST|RTF_HOSTALIAS)) {
					goto request;
			}
			if (rt->rt_ifa != ifa) {
				ROUTE_UNLOCK();
				if (m) {
					(void)m_free(m);
				}
				return (flags & RTF_HOST ? EHOSTUNREACH
							: ENETUNREACH);
			}
		}
	}
request:
	error = rtrequest(cmd, dst, ifa->ifa_addr, ifa->ifa_netmask,
			flags | ifa->ifa_flags, &nrt);
	if (m)
		(void) m_free(m);
	if (cmd == RTM_DELETE && error == 0 && (rt = nrt)) {
		rt_newaddrmsg(cmd, ifa, error, nrt);
		if (rt->rt_refcnt <= 0) {
			RT_HOLD(rt);
			rtfree(rt);
		}
	}
	if (cmd == RTM_ADD && error == 0 && (rt = nrt)) {

		/*
		 * Check for special case of adding a host route for an
		 * ip alias address. We need to avoid resetting the ifaddr
		 * field 'rt_ifa' in the routing table entry in this case.
		 */
		if ((flags & RTF_HOSTALIAS) == 0) {

			RT_RELE(rt);
			if (rt->rt_ifa != ifa) {
#ifdef DEBUG
				printf("rtinit: wrong ifa 0x%x was rt_ifa 0x%x, rt 0x%x\n",
					ifa, rt->rt_ifa, rt);
#endif
				/* delete the old route first */
				if (rt->rt_ifa->ifa_rtrequest) {
				    rt->rt_ifa->ifa_rtrequest(RTM_DELETE,
						rt, SA(0));
				}
				if (rt->rt_llinfo) {
					cmn_err_tag(169,CE_WARN,
						"Memory leak warning: route llinfo non-null in rtinit");
				}

				ifafree(rt->rt_ifa);
				atomicAddUint(&(ifa->ifa_refcnt), 1);
				rt->rt_ifa = ifa;
				rt->rt_ifp = ifa->ifa_ifp;

				/* add the new route */
				if (ifa->ifa_rtrequest) {
				    ifa->ifa_rtrequest(RTM_ADD, rt, SA(0));
				}
			}
		}
		rt->rt_srcifa = 0;

		if ((flags & RTF_HOSTALIAS) == 0) {
			/*
			 * If this is a route to one of our non-loopback
			 * addresses, via the loopback, then use the
			 * real address as the source
			 */
			if ((ifa->ifa_ifp->if_flags & IFF_LOOPBACK) &&
			    (rt->rt_srcifa = ifa_ifwithaddr(dst))) {
				;
			}
		}
			
		rt_newaddrmsg(cmd, ifa, error, nrt);
	}
	ROUTE_UNLOCK();
	return (error);
}
