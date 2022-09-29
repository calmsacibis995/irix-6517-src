/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/* 
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#ifdef __STDC__
	#pragma weak sethostent = _sethostent
	#pragma weak endhostent = _endhostent
	#pragma weak gethostent = _gethostent
	#pragma weak sethostfile = _sethostfile
	#pragma weak h_errno = _h_errno
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)gethostent.c	1.6 88/07/27 4.0NFSSRC; from	5.3 (Berkeley) 3/9/86";
#endif	/* LIBC_SCCS and not lint */

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <rpc/rpc.h>			/* required for prototyping */
#include <rpcsvc/yp_prot.h>		/* required for prototyping */
#include <rpcsvc/ypclnt.h>
#include <arpa/inet.h>			/* prototype for inet_ntoa() */
#include <strings.h>
#include <alloca.h>

/*
 * Internet version.
 */
static int	_host_stayopen _INITBSS;
#define	MAXALIASES	35
#define	MAXADDRSIZE	14
#define	HBUFSIZ		512
static struct hostent *interpret(char *, int);

static char 	*current _INITBSS;	/* current entry, analogous to hostf */
static int 	currentlen _INITBSS;
static FILE 	*hostf _INITBSS;
static char *HOSTDB = "/etc/hosts";
#define BYADDR "hosts.byaddr"

int h_errno = 0;

struct hostent *
_gethostbyname_yp(register const char *name)
{
	char *lowname, *lp, *val;
	int error, vallen, nlen;
	register struct hostent *hp;

    	if (!_yellowup(0)) {
		h_errno = __HOST_SVC_NOT_AVAIL;
		return (NULL);
    	}
	nlen = (int)strlen(name);
	lowname = (char *)alloca(nlen + 1);
	for (lp = lowname; *name; lp++, name++) {
		*lp = (char)(isupper(*name) ? tolower(*name) : *name);
	}
	*lp = '\0';
	error = yp_match(_yp_domain, "hosts.byname", lowname, nlen,
			 &val, &vallen);
	if (error) {
#ifdef DEBUG
		fprintf(stderr, "reason yp_first failed is %d\n", error);
#endif
		h_errno = (error == YPERR_DOMAIN) ? __HOST_SVC_NOT_AVAIL
			   : HOST_NOT_FOUND;
		return (NULL);
	}
	hp = interpret(val, vallen);
	free(val);
	return (hp);
}

/*ARGSUSED*/
struct hostent *
_gethostbyaddr_yp(const void *addr, register int len, register int type)
{
	register struct hostent *hp;
	char *adrstr, *val = NULL;
	int error, vallen;

	/* XXX should set h_errno but there's no appropriate value */
	if (type != AF_INET)
		return (NULL);

    	if (!_yellowup(0)) {
		h_errno = __HOST_SVC_NOT_AVAIL;
		return (NULL);
    	}
	adrstr = inet_ntoa(*(struct in_addr *)addr);
	error = yp_match(_yp_domain, BYADDR, adrstr, (int)strlen(adrstr),
			 &val, &vallen);
	if (error) {
#ifdef DEBUG
		fprintf(stderr, "reason yp_first failed is %d\n", error);
#endif
		h_errno = (error == YPERR_DOMAIN) ? __HOST_SVC_NOT_AVAIL
			   : HOST_NOT_FOUND;
		return (NULL);
	}
	hp = interpret(val, vallen);
	free(val);
    	return (hp);
}

void
sethostent(int f)
{
	if (hostf == NULL)
		hostf = fopen(HOSTDB, "r");
	else
		rewind(hostf);
	if (current)
		free(current);
	current = NULL;
	_host_stayopen |= f;
	_yellowup(1);	/* recompute whether NIS is up */
}

void
endhostent(void)
{
	if (current && !_host_stayopen) {
		free(current);
		current = NULL;
	}
	if (hostf && !_host_stayopen) {
		fclose(hostf);
		hostf = NULL;
	}
}

struct hostent *
gethostent(void)
{
	struct hostent *hp;
	char *key = NULL, *val = NULL;
	int keylen, vallen;
	char line1[HBUFSIZ+1];

	if (!_yellowup(0)) {
		if (hostf == NULL && (hostf = fopen(HOSTDB, "r")) == NULL) {
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
	        if (fgets(line1, HBUFSIZ, hostf) == NULL) {
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
		return (interpret(line1, (int)strlen(line1)));
	}
	if (current == NULL) {
		if (yp_first(_yp_domain, BYADDR, &key, &keylen,
			     &val, &vallen)) {
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
	} else {
		if (yp_next(_yp_domain, BYADDR, current, currentlen,
			    &key, &keylen, &val, &vallen)) {
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
	}
	if (current)
		free(current);
	current = key;
	currentlen = keylen;
	hp = interpret(val, vallen);
	free(val);
	return (hp);
}

/* move out of function scope so we get a global symbol for use with data cording */
static struct hostent host _INITBSSS;
static char *addr_list[2] _INITBSSS;

static struct hostent *
interpret(char *val, int len)
{
	char *p;
	register char *cp, **q;
#ifdef _LIBC_NONSHARED
	static char line[HBUFSIZ+1];
	static char *host_aliases[MAXALIASES];
	static char hostaddr[MAXADDRSIZE];
#else
	static char *line = NULL, **host_aliases = NULL, *hostaddr = NULL;

	if ((line == NULL) && ((line = calloc(1, HBUFSIZ + 1)) == NULL))
		return (NULL);
	if ((host_aliases == NULL) && ((host_aliases = (char **)
	     calloc(1, sizeof(char *) * MAXALIASES)) == NULL))
		return (NULL);
	if ((hostaddr == NULL) &&
	    ((hostaddr = calloc(1, MAXADDRSIZE)) == NULL))
		return (NULL);
#endif /* _LIBC_NONSHARED */

	strncpy(line, val, (size_t)len);
	p = line;
	line[len] = '\n';
	if (*p == '#')
		return (gethostent());
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		return (gethostent());
	*cp = '\0';
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		return (gethostent());
	*cp++ = '\0';
	/* THIS STUFF IS INTERNET SPECIFIC */
	addr_list[0] = hostaddr;
	addr_list[1] = NULL;
	host.h_addr_list = addr_list;
	*((u_int32_t *)host.h_addr) = (u_int32_t)inet_addr(p);
	host.h_length = sizeof (u_int32_t);
	host.h_addrtype = AF_INET;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	host.h_name = cp;
	q = host.h_aliases = host_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL) 
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &(host_aliases[MAXALIASES - 1]))
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&host);
}

void
sethostfile(char *file)
{
	HOSTDB = file;
}
