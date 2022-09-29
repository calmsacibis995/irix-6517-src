/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Map an internet address to a hostname or dot-notation string.  Given a
 * hostname or dot-string, resolve an IP address.
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
#include "protocols/ip.h"

int	_iphostbyname = 1;

/*
 * The iphosts cache associates IP addresses in net order with names.
 * These same names are mapped to their addresses by ipaddrs.
 */
static Cache	*iphosts;
static void	(*purgehosts)(Cache *);
static Scope	*ipaddrs;
static char	ipaddrsname[] = "ipaddrs";

#define	IP_HOSTCACHESIZE	256	/* maximum cache/scope size */

/*
 * Substituted cache purge operation for iphosts, to purge ipaddrs
 * as well as itself.
 */
static void
ip_purgehosts(Cache *c)
{
	(*purgehosts)(c);
	if (ipaddrs) {
		sc_destroy(ipaddrs);
		ipaddrs = 0;
	}
}

int
xdr_iphost(XDR *xdr, char **namep, void *key)
{
	Symbol *sym;

	switch (xdr->x_op) {
	  case XDR_DECODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
		if (ipaddrs == 0)
			ipaddrs = scope(IP_HOSTCACHESIZE, ipaddrsname);
		sym = sc_addsym(ipaddrs, *namep, -1, SYM_NUMBER);
		sym->sym_val = *(u_long *)key;
		break;
	  case XDR_FREE:
		sc_deletesym(ipaddrs, *namep, -1);
		/* FALL THROUGH */
	  case XDR_ENCODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
	}
	return TRUE;
}

static int
xdr_validate_hosts(XDR *xdr)
{
	return xdr_validate_ypfile(xdr, "hosts.byaddr", "/etc/hosts");
}

static void
ip_dumphost(void *key, char *name)
{
	printf("%-25s %s\n", inet_ntoa(*(struct in_addr *)key), name);
}

static struct cacheops iphostops = {
	{ ip_hashaddr, ip_cmpaddrs, xdr_iphost },
	xdr_validate_hosts, ip_dumphost
};

static void
create_iphosts()
{
	sethostent(1);
	c_create("ip.hosts", IP_HOSTCACHESIZE, sizeof(struct in_addr), 1 HOUR,
		 &iphostops, &iphosts);
	purgehosts = iphosts->c_info.ci_purge;
	iphosts->c_info.ci_purge = ip_purgehosts;
}

char *
ip_hostname(struct in_addr addr, enum iporder order)
{
	char *name;

	if (order == IP_HOST)
		addr.s_addr = htonl(addr.s_addr);
	if (!_iphostbyname)
		return inet_ntoa(addr);
	if (iphosts == 0)
		create_iphosts();
	name = c_match(iphosts, &addr);
	if (name == 0) {
		struct hostent *hp;

		hp = gethostbyaddr((char *) &addr, sizeof addr, AF_INET);
		if (hp)
			name = hp->h_name;
		else
			name = inet_ntoa(addr);
		name = ip_addhost(name, addr)->sym_name;
	}
	return name;
}

int
ip_hostaddr(char *name, enum iporder order, struct in_addr *addr)
{
	Symbol *sym;

	if (ipaddrs == 0)
		ipaddrs = scope(IP_HOSTCACHESIZE, ipaddrsname);
	sym = sc_lookupsym(ipaddrs, name, -1);
	if (sym == 0) {
		struct hostent *hp;
		u_long val;

		hp = gethostbyname(name);
		if (hp && hp->h_addrtype == AF_INET)
			bcopy(hp->h_addr, addr, sizeof *addr);
		else if (ip_addr(name, &val))
			addr->s_addr = htonl(val);
		else
			return 0;
		sym = ip_addhost(name, *addr);
	}
	if (order == IP_HOST)
		addr->s_addr = ntohl(sym->sym_val);
	else
		addr->s_addr = sym->sym_val;
	return 1;
}

Symbol *
ip_addhost(char *name, struct in_addr addr)
{
	int namlen;
	Symbol *sym;

	if (ipaddrs == 0)
		ipaddrs = scope(IP_HOSTCACHESIZE, ipaddrsname);
	namlen = strlen(name);
	sym = sc_lookupsym(ipaddrs, name, namlen);
	if (sym)
		return sym;

	if (iphosts == 0)
		create_iphosts();
	name = strndup(name, namlen);
	c_enter(iphosts, &addr, name);
	sym = sc_addsym(ipaddrs, name, namlen, SYM_NUMBER);
	sym->sym_val = addr.s_addr;
	return sym;
}
