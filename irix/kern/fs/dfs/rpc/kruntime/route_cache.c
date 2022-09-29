/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: route_cache.c,v 65.7 1999/10/20 18:33:01 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif

/*
*
* Copyright 1997 Silicon Graphics, Inc.
* All Rights Reserved.
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

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/pcb.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <commonp.h>
#include <com.h>

/* ========================================================================= */


/*
 * We keep a small cache mapping from destination IP addresses to
 * the local IP address to be used for responses and the max
 * packet size for this route.
 *
 * The cache is used because every RPC call will wants to check the
 * packet size and the overhead of regenerating this data every
 * time is too much.
 */

#define ROUTE_CACHE_SIZE 8
#define MAX_ROUTE_AGE 120

typedef struct {
    struct in_addr dest_addr;	/* Destination IP address	 */
    struct in_addr if_addr;	/* Local interface address	 */
    struct route route;		/* Route structure		 */
    struct sockaddr dst;	/* Complete dest structure	 */
    int max_tpdu;		/* Max message size to use	 */
    int mtu_size;		/* MTU size of interface	 */
    int cksum;			/* Checksum indicator		 */
    int stamp;			/* Time generated		 */
}   rpc_route_cache;

struct in_ifaddr *ifatoia(struct ifaddr *ifa);
rpc_socket_error_t rpc__get_ifaddr_from_destaddr (
	struct in_addr to,
	rpc_route_cache *cache);

rpc_route_cache Route_Cache[ROUTE_CACHE_SIZE];
mutex_t	route_mutex;

#define KRPC_GOOD_FDDI_MTU 16384 + 32

#define KRPC_LOCK()	mutex_lock(&route_mutex, PZERO)
#define KRPC_UNLOCK()	mutex_unlock(&route_mutex)

rpc_socket_error_t rpc_route_socket_init (void)
{
    mutex_init(&route_mutex,  MUTEX_DEFAULT, NULL);
    bzero(&Route_Cache, sizeof(rpc_route_cache) * ROUTE_CACHE_SIZE);
    return (0);
}

int find_route (struct in_addr to, rpc_route_cache *result)
{
    register int i, match = 0, ts = time;
    register rpc_route_cache *cache = Route_Cache, *oldest = Route_Cache;
    int	error;

    KRPC_LOCK();
    for (i = 0; i < ROUTE_CACHE_SIZE; i++, cache++) {
	if ((cache->dest_addr.s_addr == to.s_addr) &&
	    (cache->route.ro_rt->rt_flags & RTF_UP)) {
	    match++;
	    if (cache->stamp + MAX_ROUTE_AGE < time) {
		oldest = cache;
		break;
	    }
	    *result = *cache;
	    KRPC_UNLOCK();
	    return (0);
	}
	if (cache->stamp < ts) {
	    ts = cache->stamp;
	    oldest = cache;
	}
    }

 /* Need to regenerate route, oldest points at slot to use */
    error = rpc__get_ifaddr_from_destaddr (to, oldest);

 /* Only bomb if there really is no route */
    if (error && !match) {
	/* remove this entry from the cache */
	bzero(oldest, sizeof(rpc_route_cache));

	KRPC_UNLOCK();
	return (error);
    }

    oldest->dest_addr = to;
    oldest->stamp = time;

    *result = *oldest;

    KRPC_UNLOCK();
    return (0);
}


/*
 * rpc__ifaddr_from_destaddr
 *
 * This function finds the IP address of the local interface used
 * to send to a destination IP address. It also calculates the tpdu
 * size allowed to this destination.
 */

rpc_socket_error_t rpc__ifaddr_from_destaddr (to, ifaddr, max_tpdu)
struct in_addr to, *ifaddr;
int *max_tpdu;
{
    rpc_socket_error_t error = 0;
    rpc_route_cache cache;

    error = find_route (to, &cache);
    if (!error) {
	if (ifaddr) {
	    *ifaddr = cache.if_addr;
	}
	if (max_tpdu) {
	    *max_tpdu = cache.max_tpdu;
	}
    }
    return (error);
}

#include <net/if_types.h>
/*
 * The following value was determined by experiment.  It works out
 * to be about 4 full fddi packets.  The issues involved in choosing
 * a good value for this parameter include (1) if it's too small then
 * performance suffers due to cpu overhead incurred processing each
 * packet, (2) but if it's too big then output errors start to happen
 * which really kill performance.
 */
int krpc_fddi_mtu = KRPC_GOOD_FDDI_MTU;
int krpc_small_mtu = 8592;
int dfs_cksum = 0;


/* This is the original version which makes direct access to
 * the routing tables.
 */
rpc_socket_error_t rpc__get_ifaddr_from_destaddr (
	struct in_addr to,
	rpc_route_cache *cache)
{
    struct route *ro = &cache->route;
    struct in_ifaddr *ifa;
    struct ifnet *ifp;
    int mtu, fragmentation_ok = 0;
    int preferred_mtu = 0;

    ROUTE_RDLOCK();
    if (ro->ro_rt)
	rtfree_needpromote (ro->ro_rt);

    bzero ((char *) ro, sizeof (struct route));
    bzero ((char *) &cache->dst, sizeof (struct sockaddr_in));
    ro->ro_dst.sa_family = AF_INET;
#ifdef _HAVE_SA_LEN
    ro->ro_dst.sa_len = sizeof (struct sockaddr_in);
#endif
    ((struct sockaddr_in *) &ro->ro_dst)->sin_addr = to;
    rtalloc (ro);
    if (ro->ro_rt == 0 || ro->ro_rt->rt_ifp == 0) {
        cache->max_tpdu = 0;

	ROUTE_UNLOCK();
	if (in_localaddr (to))
	    return (EHOSTUNREACH);
	return (ENETUNREACH);
    }

    ifp = ro->ro_rt->rt_ifp;
    if (ro->ro_rt->rt_flags & RTF_GATEWAY) {
	cache->dst = *(ro->ro_rt->rt_gateway);
	cache->cksum = 1;
    } else {
	cache->dst = ro->ro_dst;
	cache->cksum = dfs_cksum;
	if ((ifp->if_type == IFT_FDDI) && krpc_fddi_mtu) {
	    fragmentation_ok = 1;
	}
    }

    mtu = ifp->if_mtu;
    cache->mtu_size = mtu;

    if (fragmentation_ok && (mtu < krpc_fddi_mtu)) {
	mtu = krpc_fddi_mtu;
    }

    if (preferred_mtu > mtu) {
	preferred_mtu = mtu;
    }
    if (mtu > IP_MAXPACKET)
	mtu = IP_MAXPACKET;
    cache->max_tpdu = (mtu - 28) & ~7;	/* subtract UDP and IP headers */

    ifa = ro->ro_rt->rt_srcifa ?
		ifatoia(ro->ro_rt->rt_srcifa) :
		ifatoia(ro->ro_rt->rt_ifa);
    if (ifa->ia_addr.sin_family == AF_INET) {
	cache->if_addr = ifa->ia_addr.sin_addr;
	ROUTE_UNLOCK();
	return(0);
    }
    rtfree_needpromote (ro->ro_rt);
    ROUTE_UNLOCK();
    return (EHOSTUNREACH);
}

