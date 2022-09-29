/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Find the rpc program number mapped to port in ipproto on the host
 * named by addr, which is interpreted in host order.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#ifdef sun
#include <protocols/nfs.h>
#else
#include <sys/fs/nfs.h>
#endif
#include "protocols/sunrpc.h"

int _sunrpcportmap = 0;		/* don't use portmap by default */

int
sunrpc_getprog(u_short port, struct in_addr addr, u_int ipproto, u_long *prog)
{
	struct pmaplist *pml;

	/*
	 * If enabled, get and search the host's list of mapped ports.
	 */
	if (_sunrpcportmap) {
		for (pml = sunrpc_getmaps(addr); pml; pml = pml->pml_next)
			if (pml->pml_map.pm_prot == ipproto
			    && pml->pml_map.pm_port == port) {
				*prog = pml->pml_map.pm_prog;
				return 1;
			}
	}

	/*
	 * If the port wasn't mapped, try a well-known mapping.
	 */
	if (ipproto == IPPROTO_UDP && port == NFS_PORT) {
		*prog = NFS_PROGRAM;
		return 1;
	}
	return 0;
}

int
sunrpc_ismapped(u_short port, struct in_addr addr, u_int ipproto)
{
	u_long prog;

	return sunrpc_getprog(port, addr, ipproto, &prog);
}
