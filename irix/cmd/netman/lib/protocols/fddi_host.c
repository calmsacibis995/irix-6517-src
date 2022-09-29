/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * FDDI hostname/address associations.
 *
 * N.B. this code assumes that FDDI and Ethernet addresses have the same length.
 */
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "cache.h"
#include "scope.h"
#include "strings.h"
#include "protocols/ether.h"
#include "protocols/fddi.h"
#include "strings.h"

int	_fddihostbyname = 1;
int	_fddibitorder = 0;

/*
 * Fddihosts caches address-to-name mappings, while fddiaddrs caches
 * name-to-address mappings.
 */
static Cache	*fddihosts;
static void	(*purgehosts)(Cache *);
static Scope	*fddiaddrs;
static char	fddiaddrsname[] = "fddiaddrs";

extern u_char	bitswaptbl[];	/* defined below */

#define	FDDI_HOSTCACHESIZE	256	/* initial cache/scope size */

/*
 * Substituted cache purge operation for fddihosts, to purge fddiaddrs
 * as well as itself.
 */
static void
fddi_purgehosts(Cache *c)
{
	(*purgehosts)(c);
	if (fddiaddrs) {
		sc_destroy(fddiaddrs);
		fddiaddrs = 0;
	}
}

int
xdr_fddihost(XDR *xdr, char **namep, void *key)
{
	Symbol *sym;

	switch (xdr->x_op) {
	  case XDR_DECODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
		if (fddiaddrs == 0)
			fddiaddrs = scope(FDDI_HOSTCACHESIZE, fddiaddrsname);
		sym = sc_addsym(fddiaddrs, *namep, -1, SYM_NUMBER);
		A_INIT(&sym->sym_addr, key, sizeof(LFDDI_ADDR));
		break;
	  case XDR_FREE:
		sc_deletesym(fddiaddrs, *namep, -1);
		/* FALL THROUGH */
	  case XDR_ENCODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
	}
	return TRUE;
}

#define F(i)   (unsigned int)(a->b[(i)])
#define FB(i)  (unsigned int)(bitswaptbl[a->b[(i)]])

char *
fddi_ntoa(LFDDI_ADDR *a)
{
	static char s[19];

	if (_fddibitorder)
		(void) sprintf(s, "%x-%x-%x-%x-%x-%x",
			       F(0), F(1), F(2), F(3), F(4), F(5));
	else
		(void) sprintf(s, "%x:%x:%x:%x:%x:%x",
			       FB(0), FB(1), FB(2), FB(3), FB(4), FB(5));
	return (s);
}
#undef F
#undef FB

static LFDDI_ADDR *
fddi_aton(char *s)
{
        static LFDDI_ADDR fa;
        register int i;
        unsigned int t[6];

        i = sscanf(s, " %x-%x-%x-%x-%x-%x",
		&t[0], &t[1], &t[2], &t[3], &t[4], &t[5]);
        if (i != 6)
		return ((LFDDI_ADDR *)NULL);
        for (i = 0; i < sizeof(fa); i++)
                fa.b[i] = t[i];
        return (&fa);
}

static void
fddi_dumphost(char *addr, char *name)
{
	printf("%-25s %s\n", fddi_ntoa((LFDDI_ADDR *)addr), name);
}

extern unsigned int ether_hashaddr();
extern int ether_cmpaddrs();
extern int xdr_validate_ethers();

static struct cacheops fddihostops = {
	{ ether_hashaddr, ether_cmpaddrs, xdr_fddihost },
	xdr_validate_ethers, fddi_dumphost
};

static void
create_fddihosts()
{
	c_create(_fddibitorder ? "fddi.mac.hosts": "fddi.ieee.hosts",
		 FDDI_HOSTCACHESIZE, sizeof(LFDDI_ADDR), 12 HOURS,
		 &fddihostops, &fddihosts);
	purgehosts = fddihosts->c_info.ci_purge;
	fddihosts->c_info.ci_purge = fddi_purgehosts;
}

Symbol *
fddi_addhost(char *name, LFDDI_ADDR *fa)
{
	int namlen;
	Symbol *sym;

	if (fddiaddrs == 0)
		fddiaddrs = scope(FDDI_HOSTCACHESIZE, fddiaddrsname);
	namlen = strlen(name);
	sym = sc_lookupsym(fddiaddrs, name, namlen);
	if (sym)
		return sym;

	if (fddihosts == 0)
		create_fddihosts();
	name = strndup(name, namlen);
	c_enter(fddihosts, fa, name);
	sym = sc_addsym(fddiaddrs, name, namlen, SYM_ADDRESS);
	A_INIT(&sym->sym_addr, fa, sizeof *fa);
	return sym;
}

#define BITSWAP(src, dst) \
	((dst)[0] = bitswaptbl[(src)[0]], \
	 (dst)[1] = bitswaptbl[(src)[1]], \
	 (dst)[2] = bitswaptbl[(src)[2]], \
	 (dst)[3] = bitswaptbl[(src)[3]], \
	 (dst)[4] = bitswaptbl[(src)[4]], \
	 (dst)[5] = bitswaptbl[(src)[5]])

void
fddi_bitswap(u_char *src, u_char *dst)
{
	BITSWAP(src, dst);
}

char *
fddi_hostname(LFDDI_ADDR *fa)
{
	char *name;
	struct etheraddr e;

	if (!_fddihostbyname)
		return fddi_ntoa(fa);
	BITSWAP((u_char *)fa, (u_char *)&e);
	if (fddihosts == 0)
		create_fddihosts();
	name = c_match(fddihosts, fa);
	if (name == 0) {
		char host[64+1];

		/*
		 * FDDI addresses can be listed in the "ethers" map/file
		 * using Ethernet bit ordering.
		 */
		if (ether_ntohost(host, &e) == 0)
			name = host;
		else {
			char *abbrev;

			abbrev = ether_vendor(&e);
			if (abbrev) {
				(void) nsprintf(host, sizeof host, "%s/%s",
						fddi_ntoa(fa), abbrev);
				name = host;
			} else
				name = fddi_ntoa(fa);
		}
		name = fddi_addhost(name, fa)->sym_name;
	}
	return name;
}

/*
 * Translate a name or an address specified in FDDI or Ethernet bit ordering
 * into an FDDI address.
 */
LFDDI_ADDR *
fddi_hostaddr(char *name)
{
	Symbol *sym;

	if (fddiaddrs == 0)
		fddiaddrs = scope(FDDI_HOSTCACHESIZE, fddiaddrsname);
	sym = sc_lookupsym(fddiaddrs, name, -1);
	if (sym == 0) {
		LFDDI_ADDR fabuf, *fa;
		struct etheraddr e;

		if (ether_hostton(name, &e) == 0) {
			fa = &fabuf;
			BITSWAP((u_char *)&e, (u_char *)fa);
		} else {
			fa = fddi_aton(name);
			if (fa == 0) {
				struct etheraddr *ea, *ether_aton();

				ea = ether_aton(name);
				if (ea == 0)
					return 0;
				fa = &fabuf;
				BITSWAP((u_char *)ea, (u_char *)fa);
				name = fddi_ntoa(fa);
			}
		}
		sym = fddi_addhost(name, fa);
	}
	return A_CAST(&sym->sym_addr, LFDDI_ADDR);
}

static u_char bitswaptbl[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};
