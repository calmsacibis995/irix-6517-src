/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifdef __STDC__
	#pragma weak getnetbyaddr = _getnetbyaddr
	#pragma weak getnetbyname = _getnetbyname
	#pragma weak setnetent = _setnetent
	#pragma weak endnetent = _endnetent
	#pragma weak getnetent = _getnetent
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getnetent.c	1.3 88/03/31 4.0NFSSRC; from	5.3 (Berkeley) 5/19/86"; /* and from 1.21 88/02/08 SMI Copyr 1984 Sun Micro */
#endif	/* LIBC_SCCS and not lint */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rpc/rpc.h>			/* required for prototyping */
#include <rpcsvc/yp_prot.h>		/* required for prototyping */
#include <rpcsvc/ypclnt.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>			/* prototype for [r]index() */

/*
 * Internet version.
 */
#define	MAXALIASES	35
#define	MAXADDRSIZE	14
#define	NBUFSIZ		200
static char *current _INITBSS;	/* current entry, analogous to netf */
static int currentlen _INITBSS;
static FILE *netf _INITBSS;
static char _net_stayopen _INITBSS;
static struct netent net _INITBSSS;

static struct netent *interpret(char *, int);
static char *nettoa(long, char *);

#define NETWORKS "/etc/networks"

struct netent *
getnetbyaddr(long anet, int type)
{
	register struct netent *p;
	char *adrstr, *val = NULL;
	int vallen;
	char ntoabuf[16];

	setnetent(0);
	if (!_yp_is_bound) {
		while (p = getnetent()) {
			if (p->n_addrtype == type && p->n_net == (unsigned long)anet)
				break;
		}
	} else {
		adrstr = nettoa(anet, ntoabuf);
		if (yp_match(_yp_domain, "networks.byaddr",
			     adrstr, (int)strlen(adrstr), &val, &vallen)) {
			p = NULL;
		} else {
			p = interpret(val, vallen);
			free(val);
		}
	}
	endnetent();
	return (p);
}

struct netent *
getnetbyname(register const char *name)
{
	register struct netent *p;
	register char **cp;
	char *val = NULL;
	int vallen;

	setnetent(0);
	if (!_yp_is_bound) {
		while (p = getnetent()) {
			if (strcmp(p->n_name, name) == 0)
				break;
			for (cp = p->n_aliases; *cp != 0; cp++)
				if (strcmp(*cp, name) == 0)
					goto found;
		}
	} else {
		if (yp_match(_yp_domain, "networks.byname",
			     name, (int)strlen(name), &val, &vallen)) {
			p = NULL;
		} else {
			p = interpret(val, vallen);
			free(val);
		}
	}
found:
	endnetent();
	return (p);
}

void
setnetent(f)
	int f;
{
	if (netf == NULL)
		netf = fopen(NETWORKS, "r");
	else
		rewind(netf);
	if (current)
		free(current);
	current = NULL;
	_net_stayopen |= f;
	_yellowup(1);	/* recompute whether NIS is up */
}

void
endnetent()
{
	if (current && !_net_stayopen) {
		free(current);
		current = NULL;
	}
	if (netf && !_net_stayopen) {
		fclose(netf);
		netf = NULL;
	}
}

static const char BYADDR[] = "networks.byaddr";

struct netent *
getnetent()
{
	char *key = NULL, *val = NULL;
	int keylen, vallen;
	char line1[NBUFSIZ+1];
	struct netent *np;

	if (!_yellowup(0)) {
		if (netf == NULL && (netf = fopen(NETWORKS, "r")) == NULL)
			return (NULL);
	        if (fgets(line1, NBUFSIZ, netf) == NULL)
			return (NULL);
		return interpret(line1, (int)strlen(line1));
	}
	if (current == NULL) {
		if (yp_first(_yp_domain, BYADDR, &key, &keylen, &val, &vallen))
			return (NULL);
	} else {
		if (yp_next(_yp_domain, BYADDR, current, currentlen,
			    &key, &keylen, &val, &vallen)) {
			return (NULL);
		}
	}
	if (current)
		free(current);
	current = key;
	currentlen = keylen;
	np = interpret(val, vallen);
	free(val);
	return (np);
}

static struct netent *
interpret(val, len)
	char *val;
	int len;
{
	char *p;
	register char *cp, **q;
#ifdef _LIBC_NONSHARED
	static char line[NBUFSIZ+1];
	static char *net_aliases[MAXALIASES];
#else
	static char *line = NULL, **net_aliases = NULL;

	if ((line == NULL) && ((line = calloc(1, NBUFSIZ+1)) == NULL))
		return (NULL);
	if ((net_aliases == NULL) && ((net_aliases = (char **)
	      calloc(1, sizeof(char *) * MAXALIASES)) == NULL))
		return (NULL);
#endif /* _LIBC_NONSHARED */

	strncpy(line, val, (size_t)len);
	p = line;
	line[len] = '\n';
	if (*p == '#')
		return (getnetent());
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		return (getnetent());
	*cp = '\0';
	net.n_name = p;
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		return (getnetent());
	*cp++ = '\0';
	while (*cp == ' ' || *cp == '\t')
		cp++;
	p = strpbrk(cp, " \t");
	if (p != NULL)
		*p++ = '\0';
	net.n_net = inet_network(cp);
	net.n_addrtype = AF_INET;
	q = net.n_aliases = net_aliases;
	if (p != NULL) 
		cp = p;
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &net_aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&net);
}

static char *
nettoa(long anet, char *ntoabuf)
{
	char *p;
	struct in_addr in;
	int addr;

	in = inet_makeaddr((int)anet, INADDR_ANY);
	addr = (int)in.s_addr;
	strcpy(ntoabuf, inet_ntoa(in));
	if ((htonl(addr) & IN_CLASSA_HOST) == 0) {
		p = index(ntoabuf, '.');
		if (p == NULL)
			return (NULL);
		*p = 0;
	} else if ((htonl(addr) & IN_CLASSB_HOST) == 0) {
		p = index(ntoabuf, '.');
		if (p == NULL)
			return (NULL);
		p = index(p+1, '.');
		if (p == NULL)
			return (NULL);
		*p = 0;
	} else if ((htonl(addr) & IN_CLASSC_HOST) == 0) {
		p = rindex(ntoabuf, '.');
		if (p == NULL)
			return (NULL);
		*p = 0;
	}
	return (ntoabuf);
}
