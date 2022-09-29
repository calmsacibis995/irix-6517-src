/* 
 * Copyright 1991 by Silicon Graphics, Inc.
 */

#ifdef __STDC__
	#pragma weak getrpcbynumber = _getrpcbynumber
	#pragma weak getrpcbyname = _getrpcbyname
	#pragma weak setrpcent = _setrpcent
	#pragma weak endrpcent = _endrpcent
	#pragma weak getrpcent = _getrpcent
#endif
#include "synonyms.h"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = 	"@(#)getrpcent.c	1.1 88/05/10 4.0NFSSRC SMI"; /* @(#) from SUN 1.12   */
#endif

/* 
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>			/* required for prototyping */
#include <rpcsvc/yp_prot.h>		/* required for prototyping */
#include <rpcsvc/ypclnt.h>
#include <arpa/inet.h>

#define	MAXALIASES	35
#define	RPCBUFSIZ	200
static struct rpcent rpc _INITBSSS;

static int stayopen _INITBSS;
static char *current _INITBSS;	/* current entry, analogous to hostf */
static int currentlen _INITBSS;
#define RPCDB "/etc/rpc"
static FILE *rpcf _INITBSS;
#define YPMAP "rpc.bynumber"

static struct rpcent *getrpcent_file(void);
static struct rpcent *getrpcent_yp(void);
static int interpret(char *, int);

struct rpcent *
getrpcbynumber(register int number)
{
	register struct rpcent *p = NULL;
	int reason;
	char adrstr[16], *val = NULL;
	int vallen;

	setrpcent(0);
	if (_yp_is_bound) {
		sprintf(adrstr, "%d", number);
		reason = yp_match(_yp_domain, YPMAP,
				  adrstr, (int)strlen(adrstr), &val, &vallen);
		if (reason == 0) {
			if (interpret(val, vallen))
				p = &rpc;
			free(val);
#ifdef DEBUG
		} else {
			fprintf(stderr, "reason yp_match failed is %d\n",
			    reason);
#endif
		}
	}
	if (!p) {
		while (p = getrpcent_file()) {
			if (p->r_number == number)
				break;
		}
	}
	endrpcent();
	return (p);
}

static struct rpcent *
byname(const char *name, struct rpcent *(*getent)(void))
{
	register struct rpcent *rpc;
	register char **rp;

	while (rpc = getent()) {
		if (strcmp(rpc->r_name, name) == 0) {
			return (rpc);
		}
		for (rp = rpc->r_aliases; *rp != NULL; rp++) {
			if (strcmp(*rp, name) == 0) {
				return (rpc);
			}
		}
	}
	return (NULL);
}

/*
 * First check NIS for the name; if not found, try the local file.
 * XXXsgi there should be an "rpc.byname" NIS map.
 */
struct rpcent *
getrpcbyname(const char *name)
{
	struct rpcent *rpc = NULL;

	setrpcent(0);
	if (_yp_is_bound) {
		rpc = byname(name, getrpcent_yp);
	}
	if (rpc == NULL) {
		rpc = byname(name, getrpcent_file);
	}
	endrpcent();
	return (rpc);
}

void
setrpcent(int f)
{
	if (rpcf == NULL)
		rpcf = fopen(RPCDB, "r");
	else
		rewind(rpcf);
	if (current)
		free(current);
	current = NULL;
	stayopen |= f;
	_yellowup(1);	/* recompute whether NIS is up */
}

void
endrpcent(void)
{
	if (current && !stayopen) {
		free(current);
		current = NULL;
	}
	if (rpcf && !stayopen) {
		fclose(rpcf);
		rpcf = NULL;
	}
}

static struct rpcent *
getrpcent_file(void)
{
	char 	line[RPCBUFSIZ+1];

	if (rpcf == NULL && (rpcf = fopen(RPCDB, "r")) == NULL)
		return (NULL);
	do {
		if (fgets(line, RPCBUFSIZ, rpcf) == NULL)
			return (NULL);
	} while (!interpret(line, (int)strlen(line)));
	return &rpc;
}

static struct rpcent *
getrpcent_yp(void)
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
	return &rpc;
}

struct rpcent *
getrpcent(void)
{
	return _yellowup(0) ? getrpcent_yp() : getrpcent_file();
}

static int
interpret(char *val, int len)
{
	register char *cp, **q;
#ifdef _LIBC_NONSHARED
	static char line[RPCBUFSIZ+1];
	static char *rpc_aliases[MAXALIASES];
#else
	static char *line = NULL, **rpc_aliases = NULL;

	if ((line == NULL) && ((line = calloc(1, RPCBUFSIZ+1)) == NULL))
		return (0);
	if ((rpc_aliases == NULL) && ((rpc_aliases = (char **)
	      calloc(1, sizeof(char *) * MAXALIASES)) == NULL))
		return (0);
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
	if (len < RPCBUFSIZ) {
		strcpy(line, val);
	} else {
		strncpy(line, val, RPCBUFSIZ);
		line[RPCBUFSIZ] = '\0';
	}
	cp = &line[cp - val];
	*cp++ = '\0';
	/* THIS STUFF IS INTERNET SPECIFIC */
	rpc.r_name = line;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	rpc.r_number = atoi(cp);
	q = rpc.r_aliases = rpc_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL) 
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &(rpc_aliases[MAXALIASES - 1]))
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (1);
}
