#ifdef __STDC__
	#pragma weak getprotobynumber = _getprotobynumber
	#pragma weak getprotobyname = _getprotobyname
	#pragma weak setprotoent = _setprotoent
	#pragma weak endprotoent = _endprotoent
	#pragma weak getprotoent = _getprotoent
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getprotoent.c	1.3 88/03/31 4.0NFSSRC; from	5.3 (Berkeley) 5/19/86"; /* and from 1.17 88/02/08 SMI Copyr 1984 Sun Micro */
#endif	/* LIBC_SCCS and not lint */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>
#include <rpc/rpc.h>			/* required for prototyping */
#include <rpcsvc/yp_prot.h>		/* required for prototyping */
#include <rpcsvc/ypclnt.h>

/*
 * Internet version.
 */
#define	MAXALIASES	35
#define	MAXADDRSIZE	14
#define	PRBUFSIZ	200

static char *current _INITBSS;	/* current entry, analogous to protof */
static int currentlen _INITBSS;
static struct protoent *interpret(char *, int);
char *inet_ntoa();
#define PROTODB "/etc/protocols"
#define BYNUMBER "protocols.bynumber"
static FILE *protof _INITBSS;
static int _proto_stayopen _INITBSS;

struct protoent *
getprotobynumber(proto)
{
	register struct protoent *p;
	int reason;
	char adrstr[12], *val = NULL;
	int vallen;

	setprotoent(0);
	if (!_yp_is_bound) {
		while (p = getprotoent()) {
			if (p->p_proto == proto)
				break;
		}
	} else {
		sprintf(adrstr, "%d", proto);
		reason = yp_match(_yp_domain, BYNUMBER,
				  adrstr, (int)strlen(adrstr), &val, &vallen);
		if (reason) {
#ifdef DEBUG
			fprintf(stderr, "reason yp_match failed is %d\n",
			    	reason);
#endif
			p = NULL;
		} else {
			p = interpret(val, vallen);
			free(val);
		}
	}
	endprotoent();
	return (p);
}

struct protoent *
getprotobyname(name)
	register const char *name;
{
	register struct protoent *p;
	register char **cp;
	int reason;
	char *val = NULL;
	int vallen;

	setprotoent(0);
	if (!_yp_is_bound) {
		while (p = getprotoent()) {
			if (strcmp(p->p_name, name) == 0)
				break;
			for (cp = p->p_aliases; *cp != 0; cp++)
				if (strcmp(*cp, name) == 0)
					goto found;
		}
	} else {
		reason = yp_match(_yp_domain, "protocols.byname",
				  name, (int)strlen(name), &val, &vallen);
		if (reason) {
#ifdef DEBUG
			fprintf(stderr, "reason yp_match failed is %d\n",
			    reason);
#endif
			p = NULL;
		} else {
			p = interpret(val, vallen);
			free(val);
		}
	}
found:
	endprotoent();
	return (p);
}

void
setprotoent(f)
	int f;
{
	if (protof == NULL)
		protof = fopen(PROTODB, "r");
	else
		rewind(protof);
	if (current)
		free(current);
	current = NULL;
	_proto_stayopen |= f;
	_yellowup(1);	/* recompute whether NIS is up */
}

void
endprotoent()
{
	if (current && !_proto_stayopen) {
		free(current);
		current = NULL;
	}
	if (protof && !_proto_stayopen) {
		fclose(protof);
		protof = NULL;
	}
}

struct protoent *
getprotoent()
{
	int reason;
	char *key = NULL, *val = NULL;
	int keylen, vallen;
	char line1[PRBUFSIZ+1];
	struct protoent *pp;

	if (!_yellowup(0)) {
		if (protof == NULL && (protof = fopen(PROTODB, "r")) == NULL)
			return (NULL);
	        if (fgets(line1, PRBUFSIZ, protof) == NULL)
			return (NULL);
		return interpret(line1, (int)strlen(line1));
	}
	if (current == NULL) {
		reason = yp_first(_yp_domain, BYNUMBER,
				  &key, &keylen, &val, &vallen);
		if (reason) {
#ifdef DEBUG
			fprintf(stderr, "reason yp_first failed is %d\n",
			    reason);
#endif
			return NULL;
		}
	} else {
		reason = yp_next(_yp_domain, BYNUMBER, current, currentlen,
				 &key, &keylen, &val, &vallen);
		if (reason) {
#ifdef DEBUG
			fprintf(stderr, "reason yp_next failed is %d\n",
			    reason);
#endif
			return NULL;
		}
	}
	if (current)
		free(current);
	current = key;
	currentlen = keylen;
	pp = interpret(val, vallen);
	free(val);
	return (pp);
}

static struct protoent *
interpret(val, len)
	char * val;
	int len;
{
	char *p;
	register char *cp, **q;
	static struct	protoent proto _INITBSSS;
#ifdef _LIBC_NONSHARED
	static char *proto_aliases[MAXALIASES];
	static char line[PRBUFSIZ];
#else
	static char *line = NULL, **proto_aliases = NULL;

	if ((line == NULL) && ((line = calloc(1, PRBUFSIZ)) == NULL))
		return (NULL);
	if ((proto_aliases == NULL) && ((proto_aliases = (char **)
	      calloc(1, sizeof(char *) * MAXALIASES)) == NULL))
		return (NULL);
#endif

	strncpy(line, val, (size_t)len);
	p = line;
	line[len] = '\n';
	if (*p == '#')
		return (getprotoent());
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		return (getprotoent());
	*cp = '\0';
	proto.p_name = p;
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		return (getprotoent());
	*cp++ = '\0';
	while (*cp == ' ' || *cp == '\t')
		cp++;
	p = strpbrk(cp, " \t");
	if (p != NULL)
		*p++ = '\0';
	proto.p_proto = atoi(cp);
	q = proto.p_aliases = proto_aliases;
	if (p != NULL) {
		cp = p;
		while (cp && *cp) {
			if (*cp == ' ' || *cp == '\t') {
				cp++;
				continue;
			}
			if (q < &(proto_aliases[MAXALIASES - 1]))
				*q++ = cp;
			cp = strpbrk(cp, " \t");
			if (cp != NULL)
				*cp++ = '\0';
		}
	}
	*q = NULL;
	return (&proto);
}
