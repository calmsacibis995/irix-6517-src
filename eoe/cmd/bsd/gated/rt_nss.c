/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 */

#ifndef	lint
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/rt_nss.c,v 1.1 1989/09/18 19:02:19 jleong Exp $";
#endif	not lint

 /*
 * Some modified from Routing Table Management Daemon routed/tables.c.
 *
 */

#include "include.h"

#ifdef	NSS
/*
 * Add a route for the virtual interface route by using the IP address
 * specified for the hostid.
 *
 */
addrouteforbackbone()
{
  struct sockaddr_in gateway;
  struct interface *ifp;
    
  bzero((char *)&gateway, sizeof (gateway));
  gateway.sin_family = AF_INET;
  gateway.sin_addr.s_addr = gethostid();
  if ((ifp = if_withnet(&gateway)) == NULL) {
    TRACE_EXT("addrouteforbackbone: no interface for %s\n", inet_ntoa(gateway.sin_addr));
    return;
  } else {
    addrouteforif(ifp);
  }
}


/*
 * Returns EGP routes metric distribution
 */
egp_metric_distrib(metric_dist, max_metric)
int	*metric_dist;
int	max_metric;
{
	struct rt_entry	*rt;
	struct rthash	*rh;

	bzero((char *) metric_dist, sizeof(int) * max_metric);

	for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++) {
		for (rt = rh->rt_forw; rt != (struct rt_entry *) rh; rt = rt->rt_forw) {
			if ((rt->rt_state & RTS_EXTERIOR) == 0)
				continue;
			if (rt->rt_proto != RTPROTO_EGP)
				continue;
			if (rt->rt_metric > max_metric) {
				TRACE_INT("egp_metric_distrib: max_metric %d, rt_metric %d\n",
					max_metric, rt->rt_metric);
				continue;
			}
			metric_dist[rt->rt_metric]++;
		}
	}
}


struct rt_entry *
rt_lookupnext(dst)
struct sockaddr_in *dst;
{
		
	struct rt_entry	*rt;
	struct rthash	*rh;
	struct sockaddr_in	*rtdst;
	static struct rt_entry	*rt_table = NULL;
	int	i;
	int					rt_compar();


	if (rttable_changed) {
		rttable_changed = 0;
		if (rt_table != NULL)
			free((char *) rt_table);
		rt_table = (struct rt_entry *) calloc(n_routes, sizeof(struct rt_entry));
		if (rt_table == NULL) {
			syslog(LOG_ERR, "rt_lookupnext malloc failed");
			return(NULL);
		}
		for (i = 0, rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++) {
			for (rt = rh->rt_forw; rt != (struct rt_entry *) rh; rt = rt->rt_forw) {
				bcopy((char *) rt, (char *) &rt_table[i++], sizeof(*rt));
				if (i > n_routes) {
					syslog(LOG_ERR, "rt_lookupnext n_routes = %d is too small", 
						n_routes);
					return(NULL);
				}
			}
		}

		if (i != n_routes) {
			syslog(LOG_ERR, "rt_lookupnext n_routes = %d, i = %d",
				n_routes, i);
			return(NULL);
		}
		qsort((char *) rt_table, n_routes, sizeof(struct rt_entry), rt_compar);
	}

	for (i = 0; i < n_routes; i++) {
		rtdst = (struct sockaddr_in *) &rt_table[i].rt_dst;
		if (dst->sin_addr.s_addr < rtdst->sin_addr.s_addr) {
			break;
		}
	}

	/*
	 * Check End Of Table first
	 */
	if (i == n_routes)
		return(NULL);
	else
		return(&rt_table[i]);

}

rt_compar(rt1, rt2)
struct rt_entry	*rt1, *rt2;
{
	struct sockaddr_in	*rt_dst1 = (struct sockaddr_in *) &rt1->rt_dst;
	struct sockaddr_in	*rt_dst2 = (struct sockaddr_in *) &rt2->rt_dst;

	if (rt_dst1->sin_addr.s_addr < rt_dst2->sin_addr.s_addr)
		return(-1);
	else
	if (rt_dst1->sin_addr.s_addr > rt_dst2->sin_addr.s_addr)
		return(1);
	else {
		if (rt1->rt_metric < rt2->rt_metric)
			return(-1);
		else
		if (rt1->rt_metric > rt2->rt_metric)
			return(1);
		else {
			if (rt1->rt_proto == RTPROTO_IGP)
				return(-1);
			else
				return(1);
		}
	}
}


/*
 * igp_sanity_chk() checks whether a particular IGP route is sane
 */
igp_sanity_chk(rt)
  struct rt_entry *rt;
{
  struct rt_entry *old_rt;
	if (!find_es_pdu(rt)) { 
		old_rt = rt;
		rt = rt->rt_back;
		rt_delete(old_rt, KERNEL_INTR);
		return(1);
	}
	else
		return(0);
}
#endif	NSS
