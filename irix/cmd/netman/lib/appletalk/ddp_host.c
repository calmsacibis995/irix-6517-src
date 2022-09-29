/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Given a hostname (object:type@zone), resolve an appletalk address
 *   (network #, node id, socket #).
 */
#include <bstring.h>
#include <netdb.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "cache.h"
#include "scope.h"
#include "strings.h"
#include "ddp_host.h"


/*
 * The ddphosts cache associates netnum, nodeid, and socknum with names.
 * These same names are mapped to their addresses by ddpaddrs.
 */
static Cache	*ddphosts;
static void	(*purgehosts)(Cache *);
static Scope	*ddpaddrs;
static char	ddpaddrsname[] = "ddpaddrs";

#define	DDP_HOSTCACHESIZE	256	/* maximum cache/scope size */

/* u_long num is actually 2 bytes of network number, 1 byte of node number,
   and 1 byte of socket number. Network number is assigned by user. Node
   number and socket number are assigned by the protocol software */
#define NET_NUM(num)	(num >> 16)
#define NODE_NUM(num)	((num >> 8) & 0xff)
#define SOCK_NUM(num)	(num & 0xff)

/*
 * Substituted cache purge operation for ddphosts, to purge ddpaddrs
 * as well as itself.
 */
static void
ddp_purgehosts(Cache *c)
{
	(*purgehosts)(c);
	if (ddpaddrs) {
		sc_destroy(ddpaddrs);
		ddpaddrs = 0;
	}
}

unsigned int
ddp_hashaddr(u_long *addr)
{
	u_long hash;

	hash = *addr;
	return ((hash >> 8 ^ hash) >> 8 ^ hash) >> 8 ^ hash;
}

int
ddp_cmpaddrs(u_long *addr1, u_long *addr2)
{
	return addr1 != addr2;
}

int
xdr_ddphost(XDR *xdr, char **namep, void *key)
{
	Symbol *sym;

	switch (xdr->x_op) {
	  case XDR_DECODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
		if (ddpaddrs == 0)
			ddpaddrs = scope(DDP_HOSTCACHESIZE, ddpaddrsname);
		sym = sc_addsym(ddpaddrs, *namep, -1, SYM_NUMBER);
		sym->sym_val = *(u_long *)key;
		break;
	  case XDR_FREE:
		sc_deletesym(ddpaddrs, *namep, -1);
		/* FALL THROUGH */
	  case XDR_ENCODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
	}
	return TRUE;
}

char*
ddp_ntoa (u_long addr)
{
	static char b[14];

	sprintf (b, "%d.%d.%d", NET_NUM(addr), NODE_NUM(addr),
		 SOCK_NUM(addr));
	return (b);
}

static void
ddp_dumphost(void *key, char *name)
{
	u_long tmp;

	printf("%-25s %s\n", ddp_ntoa(*(u_long *)key), name);
}

static struct cacheops ddphostops =
    { { ddp_hashaddr, ddp_cmpaddrs, xdr_ddphost }, 0, ddp_dumphost };

static void
create_ddphosts()
{
	c_create("ddp.hosts", DDP_HOSTCACHESIZE, sizeof(u_long),
		 1 HOUR, &ddphostops, &ddphosts);
	purgehosts = ddphosts->c_info.ci_purge;
	ddphosts->c_info.ci_purge = ddp_purgehosts;
}

/* This routine is to convert an address to appletalk object name. */
/* If name is not in cache, there is no way to add it. */
char *
ddp_hostname(u_long addr)
{
	char *name;

	if (ddphosts == 0)
		create_ddphosts();
	name = c_match(ddphosts, &addr);
	return name;
}

/* Look up network number, node id, and socket number from cache,
   using host name (object). */
int
ddp_hostaddr(char *name, u_long *addr)
{
	Symbol *sym;

	if (ddpaddrs == 0)
		ddpaddrs = scope(DDP_HOSTCACHESIZE, ddpaddrsname);
	sym = sc_lookupsym(ddpaddrs, name, -1);
	if (sym == 0)
	    /* not in scope yet */
	    return 0;

	*addr = sym->sym_val;
	return 1;
}

/* This routine is called when an NBP lookup reply is captured and
   the info is saved for ddp_hostaddr() */
Symbol *
ddp_addhost(char *name, u_long addr)
{
	int namlen;
	Symbol *sym;

	if (ddpaddrs == 0)
		ddpaddrs = scope(DDP_HOSTCACHESIZE, ddpaddrsname);
	namlen = strlen(name);
	sym = sc_lookupsym(ddpaddrs, name, namlen);
	if (sym)
		return sym;

	if (ddphosts == 0)
		create_ddphosts();
	name = strndup(name, namlen);
	c_enter(ddphosts, &addr, name);
	sym = sc_addsym(ddpaddrs, name, namlen, SYM_NUMBER);
	sym->sym_val = addr;
	return sym;
}
