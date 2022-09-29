/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Map a DECnet address to a hostname or dot-notation string.
 */
#include <bstring.h>
#include <netdb.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/socket.h>
#include "cache.h"
#include "scope.h"
#include "protocols/decnet.h"
#include "protocols/dnhost.h"
#include "strings.h"

/*
 * The rphosts cache associates RP addresses in net order with names.
 * These same names are mapped to their addresses by rpaddrs.
 */
static Cache    *rphosts;
static void     (*purgehosts)(Cache *);
static Scope    *rpaddrs;
static char	rpaddrsname[] = "rpaddrs";

#define RP_HOSTCACHESIZE	256	/* maximum cache/scope size */

/*
 * The 32 bit quantity prefixing the 16 bit DECnet node address to form
 * the 48 bit Ethernet physical address.
 */
#define	HIORD_LEN	4
unsigned char ether_hiorder[HIORD_LEN] = { 0xaa, 0x00, 0x04, 0x00 };

/*
 * Well known DECnet multicast addresses.
 */
Ether_addr allroutersaddr  = { 0xab, 0x00, 0x00, 0x03, 0x00, 0x00 };
Ether_addr allendnodesaddr = { 0xab, 0x00, 0x00, 0x04, 0x00, 0x00 };


/*
 * The routine decnet_ntoa() takes a DECnet address in network order
 * (little endian) and returns an ASCII string representing the address
 * in "." notation.
 *
 * Beware that the returned string is a static buffer and will be
 * overwritten by the subsequent call to this procedure.
 */
char*
decnet_ntoa(dn_addr addr)
{
	static char b[10];

	(void) nsprintf(b, sizeof b, "%d.%d", AREA_NUM(addr), NODE_NUM(addr));
	return (b);

}

/*
 * -------------------------------------------------------------------
 *	CACHE INDEX OPERATIONS
 * -------------------------------------------------------------------
 */
#define this_DECNET_ADDR(cp)    (*(dn_addr *)(cp))

/*
 * The DECnet address is in network order, so convert to host order
 * before returning the 16 bit DECnet address as the hashed value. The
 * hash value is better when the low order bits is the node number since
 * the node number will be more distributed than the area number.
 */
unsigned int
rp_hashaddr(void *addr)		/* Network order */
{
	return decnet_ntohs( this_DECNET_ADDR(addr) );
}

/*
 * Compare the two addresses and return zero iff the two addresses are
 * identical, otherwise return non-zero.
 */
int
rp_cmpaddrs(void *addr1, void *addr2)	/* Network order */
{
	return (this_DECNET_ADDR(addr1) - this_DECNET_ADDR(addr2));
}

/*
 * XDR a name string, adding its symbol to rpaddrs if decoding and deleting
 * the symbol if freeing.
 */
int
xdr_rphost(XDR *xdr, char **namep, void *key)
{
	Symbol *sym;

	switch (xdr->x_op) {
	  case XDR_DECODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
		if (rpaddrs == 0)
			rpaddrs = scope(RP_HOSTCACHESIZE, rpaddrsname);
		sym = sc_addsym(rpaddrs, *namep, -1, SYM_NUMBER);
		sym->sym_val = this_DECNET_ADDR(key);
		break;
	  case XDR_FREE:
		sc_deletesym(rpaddrs, *namep, -1);
		/* FALL THROUGH */
	  case XDR_ENCODE:
		if (!xdr_wrapstring(xdr, namep))
			return FALSE;
	}
	return TRUE;
}

/*
 * Translate the network ordered address to ascii and dump it along with
 * it corresponding name.
 */
/* ARGSUSED */
static void
rp_dumphost(void *key, char *name)
{
	printf("%-25s %s\n", decnet_ntoa(this_DECNET_ADDR(key)), name);
}

static struct cacheops rphostops =
    { { rp_hashaddr, rp_cmpaddrs, xdr_rphost }, 0, rp_dumphost };

/*
 * Substituted cache purge operation for rphosts, to purge rpaddrs
 * as well as the cache itself.
 */
static void
rp_purgehosts(Cache *c)
{
	(*purgehosts)(c);
	if (rpaddrs) {
		sc_destroy(rpaddrs);
		rpaddrs = 0;
	}
} /* rp_purgehosts() */

static void
create_rphosts()
{
	/* don't need to sethostent(1) */
	c_create("rp.hosts", RP_HOSTCACHESIZE, sizeof(dn_addr), 1 HOUR,
		 &rphostops, &rphosts);
	purgehosts = rphosts->c_info.ci_purge;	/* cache purge operation */
	rphosts->c_info.ci_purge = rp_purgehosts;
} /* create_rphosts() */


/*
 * -------------------------------------------------------------------
 *	HOST (and caching)  OPERATIONS
 * -------------------------------------------------------------------
 */

/*
 * Add host to host cache. The name must be copied since its continual
 * existence is not guaranteed. 
 */
Symbol *
rp_addhost(char *name, dn_addr addr)	/* in Network order */
{
	int namlen;
	Symbol *sym;

	if (rpaddrs == 0)
		rpaddrs = scope(RP_HOSTCACHESIZE, rpaddrsname);
	namlen = strlen(name);
	sym = sc_lookupsym(rpaddrs, name, namlen);
	if (sym)
		return sym;

	if (rphosts == 0)
		create_rphosts();
	name = strndup(name, namlen);
	c_enter(rphosts, &addr, name);
	sym = sc_addsym(rpaddrs, name, namlen, SYM_NUMBER);
	sym->sym_val = addr;
	return sym;
} /* rp_addhost() */


/*
 * Extract the DECnet address from the ethernet address.
 */
int
rp_addrfromether(Ether_addr *eaddr, enum rporder order, dn_addr *addr)
{
	dn_addr	*p;
	u_char	vec[2];

	if (!addr)		/* nowhere to put result */
		return 0;

	/*
	 * Check that the high order bytes are correct.
	 */
	if ( (eaddr->ea_vec[0] != ether_hiorder[0])
	  || (eaddr->ea_vec[1] != ether_hiorder[1]) 
	  || (eaddr->ea_vec[2] != ether_hiorder[2]) 
	  || (eaddr->ea_vec[3] != ether_hiorder[3]) )
		return 0;

	/*
	 * The DECnet address is derived from the last two bytes
	 * of the ethernet address. Of course the ethernet address
	 * is big endian while the DECnet address is little endian.
	 */
	vec[0] = eaddr->ea_vec[4];
	vec[1] = eaddr->ea_vec[5];
	p = (dn_addr *)vec;
	if (order == RP_HOST) {
		*addr = decnet_ntohs(*p);
	} else {
		*addr = *p;
	}
	return 1;
} /* rp_addrfromether() */


/*
 * Form the ethernet address from the DECnet address.
 */
int
rp_etherfromaddr(Ether_addr *eaddr, enum rporder order, dn_addr *addr)
{
	u_char	vec[2];
	dn_addr	*p = (dn_addr *) vec;

	if (!eaddr)		/* nowhere to put result */
		return 0;

	/*
	 * Set the high order bytes.
	 */
	bcopy(ether_hiorder, eaddr, HIORD_LEN);

	/*
	 * The DECnet address gives the last two bytes
	 * of the ethernet address. Of course the ethernet address
	 * is big endian while the DECnet address is little endian.
	 */
	if (order == RP_HOST)
		*p = decnet_ntohs(*addr);
	else
		*p = *addr;
	eaddr->ea_vec[4] = vec[0];
	eaddr->ea_vec[5] = vec[1];
	return 1;
} /* rp_addrfromether */

/*
 * Translate a hostname or dot-string to a DECnet address.
 */
int
rp_hostaddr(char *name, enum rporder order, dn_addr *addr)
{
	Symbol *sym;

	if (rpaddrs == 0)
		rpaddrs = scope(RP_HOSTCACHESIZE, rpaddrsname);
	sym = sc_lookupsym(rpaddrs, name, -1);
	if (sym == 0) {
		struct hostent *hp;
		dn_addr val;

		hp = gethostbyname(name);
		if (hp && hp->h_addrtype == AF_DECnet)
			bcopy(hp->h_addr, addr, sizeof *addr);
		else if (rp_addr(name, &val))
		{
			char *tmpname;
			*addr = decnet_htons(val);
			/* The name is a dot-string, try to get a node name. */
			if (tmpname = dn_getnodename(*addr, ORD_NET))
				name = tmpname;
		}
		else
			return 0;
		sym = rp_addhost(name, *addr);
	}
	if (order == RP_HOST)
		*addr = decnet_ntohs(sym->sym_val);
	else
		*addr = sym->sym_val;
	return 1;
} /* rp_hostaddr() */


/*
 * Translate a DECnet (Routing Protocol) address in host or net byte
 * order into a hostname or dot-notation string representation.
 */
char *
rp_hostname(dn_addr addr, enum rporder order)
{
	char *name;

	if (order == RP_HOST)
		addr = decnet_htons(addr);
	if (rphosts == 0)
		create_rphosts();
	name = c_match(rphosts, &addr);  /* check cache for addr */
	if (name == 0) {
#ifdef __sgi
		struct hostent *hp;

		hp = gethostbyaddr(&addr, sizeof(addr), AF_DECnet);
		if (hp)
			name = hp->h_name;
		else if ( 0 == (name = dn_getnodename(addr, ORD_NET)) )
#else
		if ( 0 == (name = dn_getnodename(addr, ORD_NET)) )
#endif
			/* Use a dot-notation string as the last resort */
			name = decnet_ntoa(addr);
		name = rp_addhost(name, addr)->sym_name;
	}
	return name;
} /* rp_hostname() */


/*
 * Translate an ethernet address to a string representation indicating:
 *      1) "All DECnet routers" OR "All DECnet endnodes"
 *      2) the corresponding local DECnet node name
 *      3) the DECnet address in dot-notation
 */
char *
rp_hostnamefromether(Ether_addr *eaddr)
{
	dn_addr	addr;
	static char	allrouters[] = "All DECnet routers";
	static char	allendnodes[] = "All DECnet endnodes";
	static char	b[30];
	char		*name;

	if ( !bcmp(eaddr, &allroutersaddr, ETHERADDRLEN) )
		name = allrouters;
	else if ( !bcmp(eaddr, &allendnodesaddr, ETHERADDRLEN) )
		name = allendnodes;
	else if (rp_addrfromether(eaddr, RP_NET, &addr))
		name = rp_hostname(addr, RP_NET);
	else {
		(void) nsprintf(b, sizeof b, "Bad addr %s", ether_ntoa(eaddr));
		name = b;
	}
	return name;
} /* rp_hostnamefromether() */
