/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Interpret a name such as 'host:ypbind.1' as an rpc program version
 * registered with host's portmapper.
 */
#include <bstring.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <rpc/types.h>
#include <sys/socket.h>
#include <rpc/xdr.h>
#include <rpc/pmap_prot.h>
#include <netinet/in.h>
#include "protocols/sunrpc.h"

int
sunrpc_getport(char *name, u_int ipproto, u_short *port)
{
	char *prog, *vers;
	struct sockaddr_in *sin;
	struct sockaddr_in rsin;
	struct rpcent *rp;
	static struct sockaddr_in lsin;

	*port = 0;
	vers = 0;
	prog = strchr(name, ':');
	if (prog) {
		struct hostent *hp;

		*prog++ = '\0';
		hp = gethostbyname(name);
		if (hp == 0)
			goto out;
		sin = &rsin;
		sin->sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, &sin->sin_addr, sizeof sin->sin_addr);
	} else {
		prog = name;
		sin = &lsin;
		if (sin->sin_family == 0)
			get_myaddress(sin);
	}

	vers = strrchr(prog, '.');
	if (vers)
		*vers++ = '\0';
	rp = getrpcbyname(prog);
	if (rp == 0)
		goto out;

	if (vers) {
		u_long version;

		if (sscanf(vers, "%lu", &version) != 1)
			goto out;
		*port = pmap_getport(sin, rp->r_number, version, ipproto);
	} else {
		struct pmap *highpm;
		struct in_addr addr;
		struct pmaplist *head, *pml;

		highpm = 0;
		addr.s_addr = ntohl(sin->sin_addr.s_addr);
		head = sunrpc_getmaps(addr);
		for (pml = head; pml; pml = pml->pml_next) {
			if (pml->pml_map.pm_prog != rp->r_number
			    || pml->pml_map.pm_prot != ipproto) {
				continue;
			}
			if (highpm == 0
			    || pml->pml_map.pm_vers > highpm->pm_vers) {
				highpm = &pml->pml_map;
			}
		}
		if (highpm)
			*port = highpm->pm_port;
	}

out:
	if (vers)
		*--vers = '.';
	if (prog > name)
		*--prog = ':';
	return *port != 0;
}
