/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Given a port number, Internet protocol name, and possibly null
 * nested protocol pointer, return a string containing the port
 * number and any service name associated with it.
 */
#include <netdb.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <netinet/in.h>
#include "cache.h"
#include "protocol.h"
#include "strings.h"

static Cache *ipservices;

#define	US(cp)	(*(unsigned short *)(cp))

static unsigned int
ip_hashservice(char *port)
{
	return US(port);
}

static int
ip_cmpservices(char *port1, char *port2)
{
	return US(port1) != US(port2);
}

static int
xdr_validate_services(XDR *xdr)
{
	return xdr_validate_ypfile(xdr, "services.byname", "/etc/services");
}

static void
ip_dumpservice(char *port, char *name)
{
	printf("%-5u %s\n", US(port), name);
}

static struct cacheops ipserviceops = {
	{ ip_hashservice, ip_cmpservices, xdr_wrapstring },
	xdr_validate_services, ip_dumpservice
};

char *
ip_service(u_short port, char *protoname, Protocol *servpr)
{
	char *name;
	static char buf[sizeof "ppppp (nnnnnnnnnnnnnn)"];

	if (servpr)
		name = servpr->pr_name;
	else {
		Entry **ep, *ent;

		if (ipservices == 0) {
			c_create("ip.services", 512, sizeof port, 24 HOURS,
				 &ipserviceops, &ipservices);
		}
		ep = c_lookup(ipservices, &port);
		ent = *ep;
		if (ent)
			name = ent->ent_value;	/* whether null or not */
		else {
			struct servent *sp;

			sp = getservbyport(port, protoname);
			if (sp)
				name = strdup(sp->s_name);
			else
				name = 0;
			c_add(ipservices, &port, name, ep);
		}
	}
	if (name)
		(void) sprintf(buf, "%u (%.14s)", port, name);
	else
		(void) sprintf(buf, "%u", port);
	return buf;
}
