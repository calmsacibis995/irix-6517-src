/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Crack a TCP or UDP "portmap" option's string value and map the protocol
 * named in it to the port, possibly only on a certain host.  We assume that
 * gethostbyname follows RFC 1123 and handles dotted-decimal IP addresses.
 */
#include <bstring.h>
#include <netdb.h>
#include "protocol.h"
#include "strings.h"

int
ip_setport(char *val, void (*embed)(Protocol *, long, long))
{
	char *cp, *name;
	long addr, port;
	Protocol *pr;

	cp = strchr(val, ':');
	if (cp == 0) {
		addr = 0;	/* a well-known port */
	} else {
		struct hostent *hp;

		*cp = '\0';
		hp = gethostbyname(val);
		*cp++ = ':';
		if (hp == 0)
			return 0;
		bcopy(hp->h_addr, &addr, sizeof addr);
		val = cp;
	}
	port = strtol(val, &name, 0);
	if (port == 0 || *name != '/')
		return 0;
	name++;
	pr = findprotobyname(name, strlen(name));
	if (pr == 0)
		return 0;
	(*embed)(pr, port, addr);
	return 1;
}
