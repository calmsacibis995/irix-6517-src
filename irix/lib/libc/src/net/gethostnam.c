/*
 * ++Copyright++ 1985, 1988, 1993
 * -
 * Copyright (c) 1985, 1988, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)gethostnamadr.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] = "Id: gethnamaddr.c,v 4.9.1.1 1993/05/02 22:43:03 vixie Rel vixie $";
#endif /* LIBC_SCCS and not lint */

#include "synonyms.h"
#include <stdlib.h>
#include <stddef.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>		/* prototype for strlen() */
#include <bstring.h>		/* prototype for bcopy() */
#include "net_extern.h"

#define	MAXALIASES	35
#define	MAXADDRS	35
#define HOSTBUFSIZ	1024	/* == 4.3BSD's BUFSIZ */

#ifdef _LIBC_NONSHARED
static char *host_aliases[MAXALIASES];
static char *h_addr_ptrs[MAXADDRS + 1];
static char hostaddr[MAXADDRS];
static char hostbuf[HOSTBUFSIZ+1];
#else
static char **host_aliases = NULL;
static char **h_addr_ptrs = NULL;
static char *hostaddr = NULL;
static char *hostbuf = NULL;
#endif
static FILE *hostf _INITBSS;
static int stayopen _INITBSS;
static char *host_addrs[2] _INITBSSS;
static struct in_addr host_addr _INITBSSS;
static struct hostent host _INITBSSS;

#if PACKETSZ > 1024
#define	MAXPACKET	PACKETSZ
#else
#define	MAXPACKET	1024
#endif

typedef union {
    HEADER hdr;
    u_char buf[MAXPACKET];
} querybuf;

typedef union {
    int32_t al;
    char ac;
} align;

#ifndef _LIBC_NONSHARED
static char **
setup_host_aliases(void)
{
	if (hostaddr == NULL)
	    hostaddr = (char *)calloc(1, sizeof (char *) * (MAXADDRS+1));
	if (h_addr_ptrs == NULL)
	    h_addr_ptrs = (char **)calloc(1, sizeof (char *) * (MAXADDRS+1));
	if (host_aliases == NULL)
	    host_aliases = (char **)calloc(1, sizeof (char *) * MAXALIASES);
	return host_aliases;
}
#endif /* _LIBC_NONSHARED */
/* forward references */
static struct hostent *getanswer(querybuf *, int, int);
static void _sethtent(int);
static void _endhtent(void);
static struct hostent *_gethtent(void);

static struct hostent *
getanswer(querybuf *answer, int anslen, int iquery)
{
	register HEADER *hp;
	register u_char *cp;
	register int n;
	u_char *eom;
	char *bp, **ap;
	int type, class, buflen, ancount, qdcount;
	int haveanswer, getclass = C_ANY;
	char **hap;

	eom = answer->buf + anslen;
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
#ifndef _LIBC_NONSHARED
	if (hostbuf == NULL && ((hostbuf = calloc(1, HOSTBUFSIZ))) == NULL) {
		h_errno = NO_RECOVERY;
		return NULL;
	}
#endif
	bp = hostbuf;
	buflen = HOSTBUFSIZ;
	cp = answer->buf + sizeof(HEADER);
	if (qdcount) {
		if (iquery) {
			if ((n = dn_expand(answer->buf,
					   eom, cp, bp,
			    buflen)) < 0) {
				h_errno = NO_RECOVERY;
				return ((struct hostent *) NULL);
			}
			cp += n + QFIXEDSZ;
			host.h_name = bp;
			n = (int)strlen(bp) + 1;
			bp += n;
			buflen -= n;
		} else
			cp += __dn_skipname(cp, eom) + QFIXEDSZ;
		while (--qdcount > 0)
			cp += __dn_skipname(cp, eom) + QFIXEDSZ;
	} else if (iquery) {
		if (hp->aa)
			h_errno = HOST_NOT_FOUND;
		else
			h_errno = TRY_AGAIN;
		return ((struct hostent *) NULL);
	}
#ifdef _LIBC_NONSHARED
	ap = host_aliases;
#else  /* _LIBC_NONSHARED */
	ap = setup_host_aliases();
#endif /* _LIBC_NONSHARED */
	*ap = NULL;
	host.h_aliases = host_aliases;
	hap = h_addr_ptrs;
	*hap = NULL;
#if BSD >= 43 || defined(h_addr)	/* new-style hostent structure */
	host.h_addr_list = h_addr_ptrs;
#endif
	haveanswer = 0;
	while (--ancount >= 0 && cp < eom) {
		if ((n = dn_expand(answer->buf, eom,
				   cp, bp, buflen)) < 0)
			break;
		cp += n;
		type = _getshort(cp);
 		cp += sizeof(u_int16_t);
		class = _getshort(cp);
 		cp += sizeof(u_int16_t) + sizeof(u_int32_t);
		n = _getshort(cp);
		cp += sizeof(u_int16_t);
		if (type == T_CNAME) {
			cp += n;
			if (ap >= &host_aliases[MAXALIASES-1])
				continue;
			*ap++ = bp;
			n = (int)strlen(bp) + 1;
			bp += n;
			buflen -= n;
			continue;
		}
		if (iquery && type == T_PTR) {
			if ((n = dn_expand(answer->buf,
					   eom, cp, bp,
			    buflen)) < 0)
				break;
			cp += n;
			host.h_name = bp;
			return(&host);
		}
		if (iquery || type != T_A)  {
#ifdef ALLOW_RES_DEBUG
			if (_res.options & RES_DEBUG)
				printf("unexpected answer type %d, size %d\n",
					type, n);
#endif
			cp += n;
			continue;
		}
		if (haveanswer) {
			if (n != host.h_length) {
				cp += n;
				continue;
			}
			if (class != getclass) {
				cp += n;
				continue;
			}
		} else {
			host.h_length = n;
			getclass = class;
			host.h_addrtype = (class == C_IN) ? AF_INET : AF_UNSPEC;
			if (!iquery) {
				host.h_name = bp;
				bp += strlen(bp) + 1;
			}
		}

		bp += sizeof(align) - ((ptrdiff_t)bp % sizeof(align));

#ifdef sgi
		if (bp + n >= &hostbuf[HOSTBUFSIZ]) {
#else
		if (bp + n >= &hostbuf[sizeof(hostbuf)]) {
#endif
#ifdef ALLOW_RES_DEBUG
			if (_res.options & RES_DEBUG)
				printf("size (%d) too big\n", n);
#endif
			break;
		}
		bcopy(cp, *hap++ = bp, n);
		bp +=n;
		cp += n;
		haveanswer++;
	}
	if (haveanswer) {
		*ap = NULL;
#if BSD >= 43 || defined(h_addr)	/* new-style hostent structure */
		*hap = NULL;
#else
		host.h_addr = h_addr_ptrs[0];
#endif
		return (&host);
	} else {
		h_errno = TRY_AGAIN;
		return ((struct hostent *) NULL);
	}
}

#ifdef sgi
/*
 * Create a hostent for an Internet address. Used by gethostbyname() wrapper.
 */
struct hostent *
_inet_atohtent(const char *name)
{
	if (!inet_isaddr(name, &host_addr.s_addr)) {
		h_errno = HOST_NOT_FOUND;
		return((struct hostent *) NULL);
	}
	host.h_name = (char *)name;
#ifdef _LIBC_NONSHARED
	host.h_aliases = host_aliases;
#else  /* _LIBC_NONSHARED */
	host.h_aliases = setup_host_aliases();
#endif /* _LIBC_NONSHARED */
	host_aliases[0] = NULL;
	host.h_addrtype = AF_INET;
	host.h_length = sizeof(u_int32_t);
	h_addr_ptrs[0] = (char *)&host_addr;
	h_addr_ptrs[1] = (char *)0;
	host.h_addr_list = h_addr_ptrs;
	return (&host);
}
#endif

struct hostent *
#ifdef sgi
_gethostbyname_named(const char *name)
#else
gethostbyname(const char *name)
#endif
{
	querybuf buf;
#ifndef sgi
	register char *cp;
#endif
	int n;

#ifndef sgi	/* moved to gethostbyname wrapper */
	/*
	 * disallow names consisting only of digits/dots, unless
	 * they end in a dot.
	 */
	if (isdigit(name[0]))
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-numeric, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.
				 */
				if (!inet_aton(name, &host_addr)) {
					h_errno = HOST_NOT_FOUND;
					return((struct hostent *) NULL);
				}
				host.h_name = (char *)name;
#ifdef _LIBC_NONSHARED
				host.h_aliases = host_aliases;
#else  /* _LIBC_NONSHARED */
				host.h_aliases = setup_host_aliases();
#endif /* _LIBC_NONSHARED */
				host_aliases[0] = NULL;
				host.h_addrtype = AF_INET;
				host.h_length = sizeof(u_int32_t);
				h_addr_ptrs[0] = (char *)&host_addr;
				h_addr_ptrs[1] = (char *)0;
#if BSD >= 43 || defined(h_addr)	/* new-style hostent structure */
				host.h_addr_list = h_addr_ptrs;
#else
				host.h_addr = h_addr_ptrs[0];
#endif
				return (&host);
			}
			if (!isdigit(*cp) && *cp != '.') 
				break;
		}
#endif /* !sgi */

	if ((n = res_search(name, C_IN, T_A, buf.buf, sizeof(buf))) < 0) {
#ifdef ALLOW_RES_DEBUG
		if (_res.options & RES_DEBUG)
			printf("res_search failed\n");
#endif
#ifdef sgi
		if (_oserror() == ECONNREFUSED || _oserror() == ETIMEDOUT)
			h_errno = __HOST_SVC_NOT_AVAIL;
		return ((struct hostent *) NULL);
#else
		if (errno == ECONNREFUSED)
			return (_gethtbyname(name));
		else
			return ((struct hostent *) NULL);
#endif
	}
	return (getanswer(&buf, n, 0));
}

struct hostent *
#ifdef sgi
_gethostbyaddr_named(const char *addr, int len, int type)
#else
gethostbyaddr(const char *addr, int len, int type)
#endif
{
	int n;
	querybuf buf;
	register struct hostent *hp;
	char qbuf[MAXDNAME];
	
	if (type != AF_INET)
		return ((struct hostent *) NULL);
	(void)sprintf(qbuf, "%u.%u.%u.%u.in-addr.arpa",
		((unsigned)addr[3] & 0xff),
		((unsigned)addr[2] & 0xff),
		((unsigned)addr[1] & 0xff),
		((unsigned)addr[0] & 0xff));
#ifdef sgi
	n = res_query(qbuf, C_IN, T_PTR, (u_char *)&buf, sizeof(buf));
#else
	n = res_query(qbuf, C_IN, T_PTR, (char *)&buf, sizeof(buf));
#endif
	if (n < 0) {
#ifdef ALLOW_RES_DEBUG
		if (_res.options & RES_DEBUG)
			printf("res_query failed\n");
#endif
#ifdef sgi
		if (_oserror() == ECONNREFUSED || _oserror() == ETIMEDOUT)
			h_errno = __HOST_SVC_NOT_AVAIL;
#else
		if (errno == ECONNREFUSED)
			return (_gethtbyaddr(addr, len, type));
#endif
		return ((struct hostent *) NULL);
	}
	hp = getanswer(&buf, n, 1);
	if (hp == NULL)
		return ((struct hostent *) NULL);
#ifndef _LIBC_NONSHARED
	(void) setup_host_aliases();
#endif /* _LIBC_NONSHARED */
	hp->h_addrtype = type;
	hp->h_length = len;
	h_addr_ptrs[0] = (char *)&host_addr;
	h_addr_ptrs[1] = (char *)0;
	host_addr = *(struct in_addr *)addr;
#if BSD < 43 && !defined(h_addr)	/* new-style hostent structure */
	hp->h_addr = h_addr_ptrs[0];
#endif
	return(hp);
}

static void
_sethtent(int f)
{
	if (hostf == NULL)
		hostf = fopen(_PATH_HOSTS, "r" );
	else
		rewind(hostf);
	stayopen = f;
}

static void
_endhtent(void)
{
	if (hostf && !stayopen) {
		(void) fclose(hostf);
		hostf = NULL;
	}
}

static struct hostent *
_gethtent(void)
{
	char *p;
	register char *cp, **q;

#ifdef sgi
	if (hostf == NULL && (hostf = fopen(_PATH_HOSTS, "r" )) == NULL) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
#ifndef _LIBC_NONSHARED
	if (hostbuf == NULL && ((hostbuf = calloc(1, HOSTBUFSIZ))) == NULL) {
		h_errno = NO_RECOVERY;
		return NULL;
	}
#endif
again:
	if ((p = fgets(hostbuf, HOSTBUFSIZ-1, hostf)) == NULL) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
#else
	if (hostf == NULL && (hostf = fopen(_PATH_HOSTS, "r" )) == NULL)
		return (NULL);
again:
	if ((p = fgets(hostbuf, BUFSIZ, hostf)) == NULL)
		return (NULL);
#endif
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		goto again;
	*cp = '\0';
#ifdef sgi
	/* Skip white space before the address */
	while (*p == ' ' || *p == '\t')
		p++;
#endif
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		goto again;
	*cp++ = '\0';
	/* THIS STUFF IS INTERNET SPECIFIC */
#ifdef _LIBC_NONSHARED
	q = host.h_aliases = host_aliases;
#else  /* _LIBC_NONSHARED */
	q = host.h_aliases = setup_host_aliases();
#endif /* _LIBC_NONSHARED */
#if BSD >= 43 || defined(h_addr)	/* new-style hostent structure */
	host.h_addr_list = host_addrs;
#endif
	host.h_addr = hostaddr;
	*((u_int32_t *)host.h_addr) = (u_int32_t)inet_addr(p);
	host.h_length = sizeof (u_int32_t);
	host.h_addrtype = AF_INET;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	host.h_name = cp;
	cp = strpbrk(cp, " \t");
	if (cp != NULL) 
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &host_aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&host);
}

struct hostent *
_gethtbyname(const char *name)
{
	register struct hostent *p;
	register char **cp;
	
	_sethtent(0);
	while (p = _gethtent()) {
		if (strcasecmp(p->h_name, name) == 0)
			break;
		for (cp = p->h_aliases; *cp != 0; cp++)
			if (strcasecmp(*cp, name) == 0)
				goto found;
	}
found:
	_endhtent();
	return (p);
}

struct hostent *
_gethtbyaddr(const char *addr, int len, int type)
{
	register struct hostent *p;

	_sethtent(0);
	while (p = _gethtent())
		if (p->h_addrtype == type && !bcmp(p->h_addr, addr, len))
			break;
	_endhtent();
	return (p);
}
