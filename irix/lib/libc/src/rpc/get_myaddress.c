/* Copyright 1990 by Silicon Graphics, Inc. All rights reserved */
/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.5 88/02/08
 */


/*
 * get_myaddress.c
 *
 * Get client's IP address via ioctl.  This avoids using NIS.
 */

#ifdef sgi
#ifdef __STDC__
	#pragma weak get_myaddress = _get_myaddress
#endif
#include "synonyms.h"
#include <stdio.h>
#endif
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/pmap_prot.h>
#include <rpc/errorhandler.h>
#include <errno.h>
#ifndef sgi
#include <stdio.h>
#endif
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>		/* prototype for strerror() */
#include <unistd.h>		/* prototype for close() */

/*
 * Don't use gethostbyname, which would invoke NIS.
 */
void
get_myaddress(addr)
	struct sockaddr_in *addr;
{
	int s, n;
	char buf[BUFSIZ];
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	static struct sockaddr_in prev _INITBSSS;

	/* 
	 * Use the previous result if it's there.
	 */
	if (prev.sin_addr.s_addr != 0) {
		*addr = prev;
		return;
	}

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		_rpc_errorhandler(LOG_ERR, "get_myaddress: socket: %s",
		    strerror(errno));
		exit(1);
	}
	ifc.ifc_len = sizeof (buf);
	ifc.ifc_buf = buf;
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
		_rpc_errorhandler(LOG_ERR,
		    "get_myaddress: ioctl (get interface configuration): %s",
		    strerror(errno));
		exit(1);
	}
	ifr = ifc.ifc_req;
	for (n = ifc.ifc_len/(int)sizeof(*ifr); n--; ifr++) {
		ifreq = *ifr;
		if (ioctl(s, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			_rpc_errorhandler(LOG_ERR,
			    "get_myaddress: ioctl: %s", strerror(errno));
			exit(1);
		}
		if ((ifreq.ifr_flags & IFF_UP) &&
		    ifr->ifr_addr.sa_family == AF_INET) {
			prev.sin_addr =
			      ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;
			break;
		}
	}
	(void) close(s);
	/* XXX 4.4BSD: set sin_len */
	prev.sin_family = AF_INET;
	prev.sin_port = htons(PMAPPORT);
	*addr = prev;
}
