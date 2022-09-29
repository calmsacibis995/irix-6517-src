/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snooper initialization operations.
 */
#include <bstring.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include "exception.h"
#include "expr.h"
#include "protocol.h"
#include "snooper.h"

#define	SETSNOOPLEN(sn, snooplen, rawproto) {				\
	sn->sn_snooplen = snooplen;					\
	sn->sn_packetsize = sn->sn_rawhdrpad				\
			    + (rawproto ? rawproto->pr_maxhdrlen : 0)	\
			    + snooplen;					\
}

int
sn_init(Snooper *sn, void *private, struct snops *ops, char *name, int file,
	Protocol *rawproto)
{
	Symbol *sym;
	int snooplen;

	sn->sn_private = private;
	sn->sn_ops = ops;
	sn->sn_name = name;
	sn->sn_file = file;
	bzero(&sn->sn_ifaddr, sizeof sn->sn_ifaddr);
	sn->sn_rawproto = rawproto;
	sn->sn_rawhdrpad = rawproto ? RAW_HDRPAD(rawproto->pr_maxhdrlen) : 0;
	snooplen = (rawproto
		    && (sym = pr_lookupsym(rawproto, "MTU", 3))
		    && sym->sym_type == SYM_NUMBER) ? sym->sym_val : 128;
	SETSNOOPLEN(sn, snooplen, rawproto);
	sn->sn_error = 0;
	sn->sn_errflags = 0;
	sn->sn_expr = 0;
	return 1;
}

void
sn_destroy(Snooper *sn)
{
	if (sn->sn_expr)
		ex_destroy(sn->sn_expr);
	(*sn->sn_ops->sno_destroy)(sn);
}

/* XXXbe move to sn_snooplen.c */
int
sn_setsnooplen(Snooper *sn, int snooplen)
{
	Protocol *rawproto;

	if (!sn_ioctl(sn, SIOCSNOOPLEN, &snooplen)) {
		exc_raise(sn->sn_error, "cannot set snooplen of %s to %d",
			  sn->sn_name, snooplen);
		return 0;
	}
	rawproto = sn->sn_rawproto;
	SETSNOOPLEN(sn, snooplen, rawproto);
	return 1;
}

/*
 * Determine the type of snooper to use.
 *
 *	Input				Output
 *
 *	name		type		name		*interface
 *	----		----		----		---------
 *	0		ST_LOCAL	0		0
 *	if		ST_LOCAL	if		if
 *	remotehost	ST_REMOTE	remotehost	remotehost
 *	remotehost:	ST_REMOTE	remotehost	0
 *	remotehost:if	ST_REMOTE	remotehost	if
 *	thishost	ST_LOCAL	thishost	if
 *	thishost:	ST_REMOTE	thishost	0
 *	thishost:if	ST_REMOTE	thishost	if
 *	error		ST_NULL		error		0
 *
 * 		host could be a name or an IP address.
 */
enum snoopertype
getsnoopertype(char *name, char **interface)
{
	int s, n;
	char buf[8192];
	struct ifconf ifc;
	struct ifreq *ifr;
	char *colon;
	struct hostent *hp;
	u_long addr;
	static char ifname[IFNAMSIZ+1];

	/* Trivial accept for default interface */
	if (name == 0) {
		*interface = 0;
		return ST_LOCAL;
	}

	/* Try a trace file */
	if (access(name, 0) == 0) {
		*interface = name;
		return ST_TRACE;
	}

	/* Open a socket and get interface configuration */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return ST_NULL;
	ifc.ifc_len = sizeof buf;
	ifc.ifc_buf = buf;
	if (ioctl(s, SIOCGIFCONF, (caddr_t) &ifc) < 0) {
		close(s);
		return ST_NULL;
	}
	close(s);
	s = n = ifc.ifc_len / sizeof *ifr;

	/* Check for interface name */
	for (ifr = ifc.ifc_req; --s >= 0; ifr++) {
		if (strcmp(ifr->ifr_name, name) == 0) {
			*interface = name;
			return ST_LOCAL;
		}
	}

	/* Now see if there is a colon indicating a remote interface */
	colon = strchr(name, ':');
	if (colon)
		*colon = '\0';

	/* See if it's a valid host name or IP address */
	hp = gethostbyname(name);
	if (hp && hp->h_addrtype == AF_INET)
		bcopy(hp->h_addr, &addr, sizeof addr);
	else
		addr = inet_addr(name);
	if (addr == INADDR_NONE) {
		if (colon)
			*colon = ':';
		*interface = 0;
		return ST_NULL;		/* Give up! */
	}
	if (colon) {
		*interface = (colon[1] == '\0') ? 0 : colon + 1;
		return ST_REMOTE;
	}

	/* We have an IP address: see if it is one of ours */
#define	ifr_sin(ifr)	((struct sockaddr_in *) &(ifr)->ifr_addr)
	for (ifr = ifc.ifc_req; --n >= 0; ifr++) {
		if (ifr->ifr_addr.sa_family == AF_INET
		    && ifr_sin(ifr)->sin_addr.s_addr == addr) {
			*interface = strncpy(ifname, ifr->ifr_name, IFNAMSIZ);
			return ST_LOCAL;
		}
	}
#undef	ifr_sin

	*interface = name;
	return ST_REMOTE;
}
