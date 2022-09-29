/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Sun RPC portmap caching.
 */
#include <stdio.h>
#include <values.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "cache.h"
#include "heap.h"
#include "protocols/ip.h"

/*
 * Ripped off from Sun's pmap_getmaps, in order to shorten the timeout.
 */
static struct pmaplist *
getpmaplist(struct sockaddr_in *sin)
{
	int socket;
	struct pmaplist *maps;
	struct timeval timeout;
	CLIENT *client;

	sin->sin_port = htons(PMAPPORT);
	socket = RPC_ANYSOCK;
	client = clnttcp_create(sin, PMAPPROG, PMAPVERS, &socket, 50, 500);
	if (client == 0)
		return 0;
	maps = 0;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	(void) clnt_call(client, PMAPPROC_DUMP, xdr_void, (caddr_t) 0,
			 xdr_pmaplist, &maps, timeout);
	clnt_destroy(client);
	(void) close(socket);
	return maps;
}

static void
freepmaplist(struct pmaplist *pml)
{
	while (pml) {
		struct pmaplist *tmp = pml->pml_next;
		mem_free((void *) pml, sizeof *pml);
		pml = tmp;
	}
}

/*
 * A portmapping cache element.
 */
struct pmapinfo {
	struct pmaplist	*pmaplist;
	struct timeval	expiration;	/* XXX redundant given ent_exptime */
};

static Cache *portmappings;

static int
xdr_pmapinfo(XDR *xdr, struct pmapinfo **pip)
{
	struct pmapinfo *pmi;

	pmi = *pip;
	switch (xdr->x_op) {
	  case XDR_DECODE:
		if (pmi == 0) {
			*pip = pmi = new(struct pmapinfo);
			pmi->pmaplist = 0;
		}
		/* FALL THROUGH */
	  case XDR_ENCODE:
		return xdr_pmaplist(xdr, &pmi->pmaplist)
		    && xdr_long(xdr, &pmi->expiration.tv_sec)
		    && xdr_long(xdr, &pmi->expiration.tv_usec);
	  case XDR_FREE:
		freepmaplist(pmi->pmaplist);
		delete(pmi);
		*pip = 0;
	}
	return TRUE;
}

static void
sunrpc_dumpmapent(void *addr, void *info)
{
	struct pmaplist *pml;

	printf("host %s\n", ip_hostname(*(struct in_addr *)addr, IP_HOST));
	for (pml = ((struct pmapinfo *)info)->pmaplist; pml;
	     pml = pml->pml_next) {
		printf("prog %lu vers %lu prot %lu port %lu\n",
			pml->pml_map.pm_prog, pml->pml_map.pm_vers,
			pml->pml_map.pm_prot, pml->pml_map.pm_port);
	}
}

static struct cacheops portmappingops =
    { { ip_hashaddr, ip_cmpaddrs, xdr_pmapinfo }, 0, sunrpc_dumpmapent };

#define	UPTIMEOUT	(1 HOUR)	/* cache mappings up to one hour */
#define	DOWNTIMEOUT	(2 MINUTES)	/* cache non-response this long */

static void
create_portmappings()
{
	c_create("sunrpc.portmappings", 31, sizeof(struct in_addr), UPTIMEOUT,
		 &portmappingops, &portmappings);
}

struct pmaplist *
sunrpc_getmaps(struct in_addr addr)
{
	struct timeval now;
	Entry **ep, *ent;
	struct pmapinfo *pmi;
	struct pmaplist *pml;
	struct sockaddr_in pmsin;

	/*
	 * Automagically initialize the in_addr-to-pmapinfo cache.
	 */
	if (portmappings == 0)
		create_portmappings();

	/*
	 * Get current time and lookup pmapinfo.  If info is stale,
	 * force a cache miss.
	 */
	gettimeofday(&now, (struct timezone *) 0);
	ep = c_lookup(portmappings, &addr);
	ent = *ep;
	if (ent) {
		pmi = ent->ent_value;
		if (pmi && timercmp(&now, &pmi->expiration, <))
			return pmi->pmaplist;
	}

	/*
	 * Cache miss.  Get a new list before freeing the old one, in case
	 * a signal handler is messing with the cache.
	 */
	pmsin.sin_family = AF_INET;
	pmsin.sin_addr.s_addr = htonl(addr.s_addr);
	pml = getpmaplist(&pmsin);
	if (ent)
		freepmaplist(pmi->pmaplist);
	else {
		pmi = new(struct pmapinfo);
		c_add(portmappings, &addr, pmi, ep);
	}
	pmi->pmaplist = pml;
	pmi->expiration = now;
	pmi->expiration.tv_sec += (pml) ? UPTIMEOUT : DOWNTIMEOUT;
	return pml;
}

void
sunrpc_addmap(struct in_addr addr, struct pmap *pm)
{
	Entry **ep, *ent;
	struct pmapinfo *pmi;
	struct pmaplist *pml;

	if (portmappings == 0)
		create_portmappings();
	ep = c_lookup(portmappings, &addr);
	ent = *ep;
	if (ent) {
		pmi = ent->ent_value;
		for (pml = pmi->pmaplist; pml; pml = pml->pml_next)
			if (pml->pml_map.pm_prog == pm->pm_prog
			    && pml->pml_map.pm_vers == pm->pm_vers
			    && pml->pml_map.pm_prot == pm->pm_prot) {
				pml->pml_map.pm_port = pm->pm_port;
				return;
			}
		pml = new(struct pmaplist);
		pml->pml_map = *pm;
		pml->pml_next = pmi->pmaplist;
		pmi->pmaplist = pml;
	} else {
		pml = new(struct pmaplist);
		pml->pml_map = *pm;
		pml->pml_next = 0;
		pmi = new(struct pmapinfo);
		pmi->pmaplist = pml;
		pmi->expiration.tv_sec = -1;
		pmi->expiration.tv_usec = 0;
		c_add(portmappings, &addr, pmi, ep);
	}
}
