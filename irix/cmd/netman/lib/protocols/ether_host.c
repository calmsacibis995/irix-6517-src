/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Ethernet hostname/address associations.
 */
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "cache.h"
#include "scope.h"
#include "strings.h"
#include "protocols/ether.h"

char *ether_ntoa(unsigned char *e);

int	_etherhostbyname = 1;

/*
 * Etherhosts caches address-to-name mappings, while etheraddrs caches
 * name-to-address mappings.
 */
static Cache	*etherhosts;
static void	(*purgehosts)(Cache *);
static Scope	*etheraddrs;
static char	etheraddrsname[] = "etheraddrs";

#define	ETHER_HOSTCACHESIZE	256	/* initial cache/scope size */

/*
 * Substituted cache purge operation for etherhosts, to purge etheraddrs
 * as well as itself.
 */
static void
ether_purgehosts(Cache *c)
{
	(*purgehosts)(c);
	if (etheraddrs) {
		sc_destroy(etheraddrs);
		etheraddrs = 0;
	}
}

#define	US(cp)	((unsigned short *)(cp))

unsigned int
ether_hashaddr(char *addr)
{
	return US(addr)[2] << 2 ^ US(addr)[1] << 1 ^ US(addr)[0];
}

int
ether_cmpaddrs(char *addr1, char *addr2)
{
	return US(addr1)[2] != US(addr2)[2] || US(addr1)[1] != US(addr2)[1]
	    || US(addr1)[0] != US(addr2)[0];
}

static int
xdr_etherhost(XDR *xdr, char **namep, void *key)
{
	Symbol *sym;

	switch (xdr->x_op) {
	  case XDR_DECODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
		if (etheraddrs == 0)
			etheraddrs = scope(ETHER_HOSTCACHESIZE, etheraddrsname);
		sym = sc_addsym(etheraddrs, *namep, -1, SYM_NUMBER);
		A_INIT(&sym->sym_addr, key, ETHERADDRLEN);
		break;
	  case XDR_FREE:
		sc_deletesym(etheraddrs, *namep, -1);
		/* FALL THROUGH */
	  case XDR_ENCODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
	}
	return TRUE;
}

int
xdr_validate_ethers(XDR *xdr)
{
	return xdr_validate_ypfile(xdr, "ethers.byaddr", "/etc/ethers");
}

static void
ether_dumphost(unsigned char *addr, char *name)
{
	printf("%-25s %s\n", ether_ntoa(addr), name);
}

static struct cacheops etherhostops = {
	{ ether_hashaddr, ether_cmpaddrs, xdr_etherhost },
	xdr_validate_ethers, ether_dumphost
};

static void
create_etherhosts()
{
	c_create("ether.hosts", ETHER_HOSTCACHESIZE, sizeof(struct etheraddr),
		 12 HOURS, &etherhostops, &etherhosts);
	purgehosts = etherhosts->c_info.ci_purge;
	etherhosts->c_info.ci_purge = ether_purgehosts;
}

char *
ether_hostname(struct etheraddr *ea)
{
	char *name;

	if (!_etherhostbyname)
		return ether_ntoa(ea->ea_vec);
	if (etherhosts == 0)
		create_etherhosts();
	name = c_match(etherhosts, ea);
	if (name == 0) {
		char host[64+1];

		if (ether_ntohost(host, ea) == 0)
			name = host;
		else {
			char *abbrev;

			abbrev = ether_vendor(ea);
			if (abbrev) {
				(void) nsprintf(host, sizeof host, "%s/%s",
						ether_ntoa(ea->ea_vec), abbrev);
				name = host;
			} else
				name = ether_ntoa(ea->ea_vec);
		}
		name = ether_addhost(name, ea)->sym_name;
	}
	return name;
}

struct etheraddr *
ether_hostaddr(char *name)
{
	Symbol *sym;

	if (etheraddrs == 0)
		etheraddrs = scope(ETHER_HOSTCACHESIZE, etheraddrsname);
	sym = sc_lookupsym(etheraddrs, name, -1);
	if (sym == 0) {
		struct etheraddr *ea;
		struct etheraddr eabuf;

		ea = &eabuf;
		if (ether_hostton(name, ea) != 0) {
			struct etheraddr *ether_aton();

			ea = ether_aton(name);
			if (ea == 0)
				return 0;
		}
		sym = ether_addhost(name, ea);
	}
	return A_CAST(&sym->sym_addr, struct etheraddr);
}

Symbol *
ether_addhost(char *name, struct etheraddr *ea)
{
	int namlen;
	Symbol *sym;

	if (etheraddrs == 0)
		etheraddrs = scope(ETHER_HOSTCACHESIZE, etheraddrsname);
	namlen = strlen(name);
	sym = sc_lookupsym(etheraddrs, name, namlen);
	if (sym)
		return sym;

	if (etherhosts == 0)
		create_etherhosts();
	name = strndup(name, namlen);
	c_enter(etherhosts, ea, name);
	sym = sc_addsym(etheraddrs, name, namlen, SYM_ADDRESS);
	A_INIT(&sym->sym_addr, ea, sizeof *ea);
	return sym;
}
