/* 
 * Copyright 1991 by Silicon Graphics, Inc.
 */

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifdef __STDC__
	#pragma weak getservbyport = _getservbyport
	#pragma weak getservbyname = _getservbyname
	#pragma weak setservent = _setservent
	#pragma weak endservent = _endservent
	#pragma weak getservent = _getservent
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getservent.c	1.3 88/03/31 4.0NFSSRC; from	5.3 (Berkeley) 5/19/86"; /* and from 1.19 88/02/08 SMI Copyr 1984 Sun Micro */
#endif	/* LIBC_SCCS and not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rpc/rpc.h>			/* required for prototyping */
#include <rpcsvc/yp_prot.h>		/* required for prototyping */
#include <rpcsvc/ypclnt.h>
#include <arpa/inet.h>

#define	MAXALIASES	35
#define	SERVBUFSIZ	200
/*
 * Internet version.
 */
static struct	servent serv _INITBSSS;

static char 	*current _INITBSS;	/* current entry, analogous to servf */
static int 	currentlen _INITBSS;
#define	SERVDB	"/etc/services"
static FILE 	*servf _INITBSS;
int 		_serv_stayopen _INITBSS;

#define YPMAP "services.byname"

static struct servent *getservent_file(void);
static struct servent *getservent_yp(void);
static int interpret(char *, int);

/*
 * XXXsgi there should be "services.bysname" and "services.byport" NIS maps.
 */
struct servent *
getservbyport(int svc_port, const char *proto)
{
	register struct servent *p = NULL;
	register u_short port = (u_short) svc_port;
	static char **servlines _INITBSS;
	static int maxlines = 512;
	static int numlines _INITBSS;
	char **l;

#define ADDLINE(l) { \
    if (numlines >= maxlines) { \
	maxlines *= 2; \
	if (!(servlines = (char **)realloc((void *)servlines, (size_t)maxlines*sizeof(servlines[0])))) \
	    return NULL; \
    } \
    servlines[numlines++] = (l); \
}

	/* Load the NIS and /etc/services lines into servlines array */
	if (servlines == NULL) {
		if (!(servlines = (char **)malloc((unsigned int)maxlines * sizeof(servlines[0]))))
			return (NULL);

		if (_yellowup(0)) {
			char *current = NULL, *key = NULL, *val = NULL;
			int currentlen, keylen, vallen, status;

			for (;;) {
				if (current == NULL) {
					status = yp_first(_yp_domain, YPMAP,
						&key, &keylen, &val, &vallen);
				} else {
					status = yp_next(_yp_domain, YPMAP,
						current, currentlen,
						&key, &keylen, &val, &vallen);
				}
				if (current)
					free(current);
				if (status) 
					break;
				current = key;
				currentlen = keylen;
				ADDLINE(val);
			}
		}
		if (servf != NULL || (servf = fopen(SERVDB, "r")) != NULL) {
			char line[SERVBUFSIZ+1];

			while (fgets(line, SERVBUFSIZ, servf) != NULL) {
				ADDLINE(strdup(line));
			}
			fclose(servf);
			servf = NULL;
		}
		servlines[numlines] = NULL;
	} 
	for (l = servlines; *l; l++) {
		if (interpret(*l, (int)strlen(*l)) && (unsigned int)serv.s_port == port &&
		    (proto == NULL || strcmp(serv.s_proto, proto) == 0)) {
			p = &serv;
			break;
		}
	}
	return (p);
}

static struct servent *
byname(const char *name, const char *proto, struct servent *(*getent)(void))
{
	register struct servent *p;
	register char **cp;

	while (p = getent()) {
		if (strcmp(name, p->s_name) == 0)
			goto gotname;
		for (cp = p->s_aliases; *cp; cp++)
			if (strcmp(name, *cp) == 0)
				goto gotname;
		continue;
gotname:
		if (proto == 0 || strcmp(p->s_proto, proto) == 0)
			return p;
	}
	return (NULL);
}

int _getserv_file_is_first = 0; /* for used by inetd only */
			        /* if set, read File first, then NIS */
/*
 * First check NIS for the name; if not found, try the local file.
 * (this order can be override by _getserv_file_is_first)
 */
struct servent *
getservbyname(const char *name, const char *proto)
{
	struct servent *serv = NULL;

	FILE *logf;

	setservent(0);
	if (_getserv_file_is_first)
		serv = byname(name, proto, getservent_file);
	if ((serv == NULL) && (_yp_is_bound))
		serv = byname(name, proto, getservent_yp);
	if ((serv == NULL) && (!_getserv_file_is_first))
		serv = byname(name, proto, getservent_file);
	endservent();
	return (serv);
}

void
setservent(int f)
{
	if (servf == NULL)
		servf = fopen(SERVDB, "r");
	else
		rewind(servf);
	if (current)
		free(current);
	current = NULL;
	_serv_stayopen |= f;
	_yellowup(1);	/* recompute whether NIS is up */
}

void
endservent(void)
{
	if (current && !_serv_stayopen) {
		free(current);
		current = NULL;
	}
	if (servf && !_serv_stayopen) {
		fclose(servf);
		servf = NULL;
	}
}

static struct servent *
getservent_file(void)
{
	char	line[SERVBUFSIZ+1];

	if (servf == NULL && (servf = fopen(SERVDB, "r")) == NULL)
		return (NULL);
	do {
		if (fgets(line, SERVBUFSIZ, servf) == NULL)
			return (NULL);
	} while (!interpret(line, (int)strlen(line)));
	return &serv;
}

static struct servent *
getservent_yp(void)
{
	int ok;
	char *key = NULL, *val = NULL;
	int keylen, vallen;

	do {
		if (current == NULL) {
			if (ok =  yp_first(_yp_domain, YPMAP,
			    &key, &keylen, &val, &vallen)) {
#ifdef DEBUG
				fprintf(stderr,"reason yp_first failed is %d\n",
				    ok);
#endif
				return NULL;
			}
		} else {
			if (ok = yp_next(_yp_domain, YPMAP, current,
				currentlen, &key, &keylen, &val, &vallen)) {
#ifdef DEBUG
				fprintf(stderr, "reason yp_next failed is %d\n",
				    ok);
#endif
				return NULL;
			}
		}
		if (current)
			free(current);
		current = key;
		currentlen = keylen;
		ok = interpret(val, vallen);
		free(val);
	} while (!ok);
	return &serv;
}

struct servent *
getservent(void)
{
	return _yellowup(0) ? getservent_yp() : getservent_file();
}

static int
interpret(char *val, int len)
{
	register char *cp, **q, *p, *sp;
#ifdef _LIBC_NONSHARED
	static char line[SERVBUFSIZ+1];
	static char *serv_aliases[MAXALIASES];
#else
	static char *line = NULL, **serv_aliases = NULL;

	if ((line == NULL) && ((line = calloc(1, SERVBUFSIZ+1)) == NULL))
		return (NULL);
	if ((serv_aliases == NULL) && ((serv_aliases = (char **)
	      calloc(1, sizeof(char *) * MAXALIASES)) == NULL))
		return (NULL);
#endif

	if (*val == '#')
		return (0);
	val[len] = '\n';
	cp = strpbrk(val, "#\n");
	if (cp == NULL)
		return (0);
	*cp = '\0';
	cp = strpbrk(val, " \t");
	if (cp == NULL)
		return (0);
	p = cp + 1;
	while (*p == ' ' || *p == '\t')
		p++;
	sp = strpbrk(p, ",/");
	if (sp == NULL)
		return (0);
	/*
	 *    name<WS>port/proto<WS>alias
	 *        ^   ^   ^
	 *        cp  p   sp
	 */
	if (len < SERVBUFSIZ) {
		strcpy(line, val);
	} else {
		strncpy(line, val, SERVBUFSIZ);
		line[SERVBUFSIZ] = '\0';
	}
	serv.s_name = line;
	line[cp - val] = '\0';	/* terminate name */
	cp = &line[sp - val];
	*cp++ = '\0';		/* terminate port */
	serv.s_port = htons((u_short)atoi(p));
	serv.s_proto = cp;
	q = serv.s_aliases = serv_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &(serv_aliases[MAXALIASES - 1]))
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (1);
}
