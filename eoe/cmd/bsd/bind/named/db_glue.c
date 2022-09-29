#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_glue.c	4.4 (Berkeley) 6/1/90";
static char rcsid[] = "$Id: db_glue.c,v 3.7 1997/11/15 20:30:34 jes Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986, 1988
 * -
 * Copyright (c) 1986, 1988
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

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include <netdb.h>
#include <resolv.h>
#include <errno.h>
#include <signal.h>

#include "named.h"

struct valuelist {
	struct valuelist *next, *prev;
	char	*name;
	char	*proto;
	int	port;
};
static struct valuelist *servicelist, *protolist;

#if defined(ultrix)
/* ultrix 4.0 has some icky packaging details.  work around them here.
 * since this module is linked into named and named-xfer, we end up
 * forcing both to drag in our own res_send rather than ultrix's hesiod
 * version of that.
 */
static const int (*unused_junk)__P((const u_char *, int, u_char *, int)) =
	res_send;
;
#endif

/*XXX: sin_ntoa() should probably be in libc*/
const char *
sin_ntoa(sin)
	const struct sockaddr_in *sin;
{
	static char ret[sizeof "[111.222.333.444].55555"];

	if (!sin)
		strcpy(ret, "[sin_ntoa(NULL)]");
	else
		sprintf(ret, "[%s].%u",
			inet_ntoa(sin->sin_addr),
			ntohs(sin->sin_port));
	return (ret);
}

/*
 * XXX:	some day we'll make this a varargs function
 */
void
panic(err, msg)
	int err;
	const char *msg;
{
	if (err == -1)
		syslog(LOG_CRIT, "%s - ABORT", msg);
	else
		syslog(LOG_CRIT, "%s: %s - ABORT", msg, strerror(err));
	signal(SIGIOT, SIG_DFL);	/* no POSIX needed here. */
	abort();
}

void
buildservicelist()
{
	struct servent *sp;
	struct valuelist *slp;

#ifdef MAYBE_HESIOD
	setservent(0);
#else
	setservent(1);
#endif
	while (sp = getservent()) {
		slp = (struct valuelist *)malloc(sizeof(struct valuelist));
		if (!slp)
			panic(errno, "malloc(servent)");
		slp->name = savestr(sp->s_name);
		slp->proto = savestr(sp->s_proto);
		slp->port = ntohs((u_int16_t)sp->s_port);  /* host byt order */
		slp->next = servicelist;
		slp->prev = NULL;
		if (servicelist)
			servicelist->prev = slp;
		servicelist = slp;
	}
	endservent();
}

void
buildprotolist()
{
	struct protoent *pp;
	struct valuelist *slp;

#ifdef MAYBE_HESIOD
	setprotoent(0);
#else
	setprotoent(1);
#endif
	while (pp = getprotoent()) {
		slp = (struct valuelist *)malloc(sizeof(struct valuelist));
		if (!slp)
			panic(errno, "malloc(protoent)");
		slp->name = savestr(pp->p_name);
		slp->port = pp->p_proto;	/* host byte order */
		slp->next = protolist;
		slp->prev = NULL;
		if (protolist)
			protolist->prev = slp;
		protolist = slp;
	}
	endprotoent();
}

static int
findservice(s, list)
	register char *s;
	register struct valuelist **list;
{
	register struct valuelist *lp = *list;
	int n;

	for (; lp != NULL; lp = lp->next)
		if (strcasecmp(lp->name, s) == 0) {
			if (lp != *list) {
				lp->prev->next = lp->next;
				if (lp->next)
					lp->next->prev = lp->prev;
				(*list)->prev = lp;
				lp->next = *list;
				*list = lp;
			}
			return (lp->port);	/* host byte order */
		}
	if (sscanf(s, "%d", &n) != 1 || n <= 0)
		n = -1;
	return (n);
}

/*
 * Convert service name or (ascii) number to int.
 */
int
servicenumber(p)
	char *p;
{
	return (findservice(p, &servicelist));
}

/*
 * Convert protocol name or (ascii) number to int.
 */
int
protocolnumber(p)
	char *p;
{
	return (findservice(p, &protolist));
}

#if defined(__STDC__) || defined(__GNUC__)
static struct servent *
cgetservbyport(u_int16_t port,		/* net byte order */
	       char *proto)
#else
static struct servent *
cgetservbyport(port, proto)
	u_int16_t port;			/* net byte order */
	char *proto;
#endif
{
	register struct valuelist **list = &servicelist;
	register struct valuelist *lp = *list;
	static struct servent serv;

	port = ntohs(port);
	for (; lp != NULL; lp = lp->next) {
		if (port != (u_int16_t)lp->port)	/* host byte order */
			continue;
		if (strcasecmp(lp->proto, proto) == 0) {
			if (lp != *list) {
				lp->prev->next = lp->next;
				if (lp->next)
					lp->next->prev = lp->prev;
				(*list)->prev = lp;
				lp->next = *list;
				*list = lp;
			}
			serv.s_name = lp->name;
			serv.s_port = htons((u_int16_t)lp->port);
			serv.s_proto = lp->proto;
			return (&serv);
		}
	}
	return (0);
}

static struct protoent *
cgetprotobynumber(proto)
	register int proto;	/* host byte order */
{
	register struct valuelist **list = &protolist;
	register struct valuelist *lp = *list;
	static struct protoent prot;

	for (; lp != NULL; lp = lp->next)
		if (lp->port == proto) {	/* host byte order */
			if (lp != *list) {
				lp->prev->next = lp->next;
				if (lp->next)
					lp->next->prev = lp->prev;
				(*list)->prev = lp;
				lp->next = *list;
				*list = lp;
			}
			prot.p_name = lp->name;
			prot.p_proto = lp->port;  /* host byte order */
			return (&prot);
		}
	return (0);
}

char *
protocolname(num)
	int num;
{
	static	char number[8];
	struct protoent *pp;

	pp = cgetprotobynumber(num);
	if(pp == 0)  {
		(void) sprintf(number, "%d", num);
		return (number);
	}
	return (pp->p_name);
}

#if defined(__STDC__) || defined(__GNUC__)
char *
servicename(u_int16_t port, char *proto)	/* host byte order */
#else
char *
servicename(port, proto)
	u_int16_t port;				/* host byte order */
	char *proto;
#endif
{
	static	char number[8];
	struct servent *ss;

	ss = cgetservbyport(htons(port), proto);
	if (ss == 0)  {
		(void) sprintf(number, "%d", port);
		return (number);
	}
	return (ss->s_name);
}

u_int
db_getclev(origin)
	const char *origin;
{
	u_int lev = 0;
	dprintf(12, (ddt, "db_getclev of \"%s\"", origin));
	if (origin && *origin)
		lev++;
	while (origin && (origin = strchr(origin, '.'))) {
		origin++;
		lev++;
	}
	dprintf(12, (ddt, " = %d\n", lev));
	return (lev);
}

void
gettime(ttp)
	struct timeval *ttp;
{
	if (gettimeofday(ttp, NULL) < 0)
		syslog(LOG_ERR, "gettimeofday: %m");
	return;
}

#if !defined(BSD)
int
getdtablesize()
{
#if defined(USE_POSIX)
	int j = (int) sysconf(_SC_OPEN_MAX);
 
	if (j >= 0)
		return (j);
#endif /* POSIX */
	return (FD_SETSIZE);
}
#endif /* BSD */

int
my_close(fd)
	int fd;
{
	int s;

	do {
		errno = 0;
		s = close(fd);
	} while (s < 0 && errno == EINTR);

	if (s < 0 && errno != EBADF)
		syslog(LOG_INFO, "close(%d) failed: %m", fd);
	else
		dprintf(3, (ddt, "close(%d) succeeded\n", fd));
	return (s);
}

#ifdef GEN_AXFR
/*
 * Map class names to number
 */
struct map {
	char	*token;
	int	val;
};

static struct map map_class[] = {
	{ "in",		C_IN },
	{ "chaos",	C_CHAOS },
	{ "hs",		C_HS },
	{ NULL,		0 }
};

int
get_class(class)
	char *class;
{
	struct map *mp;

	if (isdigit(*class))
		return (atoi(class));
	for (mp = map_class; mp->token != NULL; mp++)
		if (strcasecmp(class, mp->token) == 0)
			return (mp->val);
	return (C_IN);
}
#endif

int
my_fclose(fp)
	FILE *fp;
{
	int fd = fileno(fp),
	    s = fclose(fp);

	if (s < 0)
		syslog(LOG_INFO, "fclose(%d) failed: %m", fd);
	else
		dprintf(3, (ddt, "fclose(%d) succeeded\n", fd));
	return (s);
}

/*
 * Make a copy of a string and return a pointer to it.
 */
char *
savestr(str)
	const char *str;
{
	char *cp = strdup(str);

	if (!cp)
		panic(errno, "malloc(savestr)");
	return (cp);
}

int
writemsg(rfd, msg, msglen)
	int rfd;
	u_char *msg;
	int msglen;
{
	struct iovec iov[2];
	u_char len[INT16SZ];

	__putshort(msglen, len);
	iov[0].iov_base = (char *)len;
	iov[0].iov_len = INT16SZ;
	iov[1].iov_base = (char *)msg;
	iov[1].iov_len = msglen;
	if (writev(rfd, iov, 2) != INT16SZ + msglen) {
		dprintf(1, (ddt, "write failed %d\n", errno));
		return (-1);
	}
	return (0);
}

/* rm_datum(dp, np, pdp)
 *	remove datum 'dp' from name 'np'.  pdp is previous data pointer.
 * return value:
 *	"next" field from removed datum, suitable for relinking
 */
struct databuf *
rm_datum(dp, np, pdp)
	register struct databuf *dp;
	register struct namebuf *np;
	register struct databuf *pdp;
{
	register struct databuf *ndp = dp->d_next;

	dprintf(3, (ddt, "rm_datum(%lx, %lx, %lx) -> %lx\n",
		    (u_long)dp, (u_long)np->n_data, (u_long)pdp, (u_long)ndp));
#ifdef INVQ
	rminv(dp);
#endif
	if (pdp == NULL)
		np->n_data = ndp;
	else
		pdp->d_next = ndp;
	if ((dp->d_flags & DB_F_ACTIVE) == 0)
		panic(-1, "rm_datum: DB_F_ACTIVE not set");
	dp->d_flags &= ~DB_F_ACTIVE;
	dp->d_next = NULL;
	if (--(dp->d_rcnt)) {
#ifdef DEBUG
		int32_t ii;
#endif

		switch(dp->d_type) {
		case T_NS:
			dprintf(1, (ddt, "rm_datum: %s rcnt = %d\n",
				dp->d_data, dp->d_rcnt));
			break;
		case T_A:

#ifdef DEBUG
			bcopy(dp->d_data, &ii, sizeof(ii));
#endif
			dprintf(1, (ddt, "rm_datum: %08.8X rcnt = %d\n",
				ii, dp->d_rcnt));
			break;
		default:
			dprintf(1, (ddt, "rm_datum: rcnt = %d\n", dp->d_rcnt));
		}
	} else
		db_free(dp);
	return (ndp);
}

/* rm_name(np, he, pnp)
 *	remove name 'np' from parent 'pp'.  pnp is previous name pointer.
 * return value:
 *	"next" field from removed name, suitable for relinking
 */
struct namebuf *
rm_name(np, pp, pnp)
	struct namebuf *np, **pp, *pnp;
{
	struct namebuf *nnp = np->n_next;
	char *msg;

	/* verify */
	if ( (np->n_data && (msg = "data"))
	  || (np->n_hash && (msg = "hash"))
	    ) {
		syslog(LOG_ERR,
		       "rm_name(%#lx(%s)): non-nil %s pointer\n",
		       (u_long)np, NAME(*np), msg);
		panic(-1, "rm_name");
	}

	/* unlink */
	if (pnp) {
		pnp->n_next = nnp;
	} else {
		*pp = nnp;
	}

	/* deallocate */
	free((char*) np);

	/* done */
	return (nnp);
}

/*
 * Get the domain name of 'np' and put in 'buf'.  Bounds checking is done.
 */
void
getname(np, buf, buflen)
	struct namebuf *np;
	char *buf;
	int buflen;
{
	register char *cp;
	register int i;

	cp = buf;
	while (np != NULL) {
		i = (int) NAMELEN(*np);
		if (i + 1 >= buflen) {
			*cp = '\0';
			syslog(LOG_INFO, "domain name too long: %s...\n", buf);
			strcpy(buf, "Name_Too_Long");
			return;
		}
		if (cp != buf)
			*cp++ = '.';
		bcopy(NAME(*np), cp, i);
		cp += i;
		buflen -= i + 1;
		np = np->n_parent;
	}
	*cp = '\0';
}

#ifdef INVQ
/*
 * Add data 'dp' to inverse query tables for name 'np'.
 */
void
addinv(np, dp)
	struct namebuf *np;
	struct databuf *dp;
{
	register struct invbuf *ip;
	register int hval, i;

	switch (dp->d_type) {
	case T_A:
	case T_UID:
	case T_GID:
		break;

	default:
		return;
	}

	hval = dhash(dp->d_data, dp->d_size);
	for (ip = invtab[hval]; ip != NULL; ip = ip->i_next)
		for (i = 0; i < INVBLKSZ; i++)
			if (ip->i_dname[i] == NULL) {
				ip->i_dname[i] = np;
				return;
			}
	ip = saveinv();
	ip->i_next = invtab[hval];
	invtab[hval] = ip;
	ip->i_dname[0] = np;
}

/*
 * Remove data 'odp' from inverse query table.
 */
void
rminv(odp)
	struct databuf *odp;
{
	register struct invbuf *ip;
	register struct databuf *dp;
	struct namebuf *np;
	register int i;

	for (ip = invtab[dhash(odp->d_data, odp->d_size)]; ip != NULL;
	    ip = ip->i_next) {
		for (i = 0; i < INVBLKSZ; i++) {
			if ((np = ip->i_dname[i]) == NULL)
				break;
			for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
				if (dp != odp)
					continue;
				while (i < INVBLKSZ-1) {
					ip->i_dname[i] = ip->i_dname[i+1];
					i++;
				}
				ip->i_dname[i] = NULL;
				return;
			}
		}
	}
}

/*
 * Allocate an inverse query buffer.
 */
struct invbuf *
saveinv()
{
	register struct invbuf *ip;

	ip = (struct invbuf *) malloc(sizeof(struct invbuf));
	if (!ip)
		panic(errno, "malloc(saveinv)");
	ip->i_next = NULL;
	bzero((char *)ip->i_dname, sizeof(ip->i_dname));
	return (ip);
}

/*
 * Compute hash value from data.
 */
int
dhash(dp, dlen)
	register const u_char *dp;
	int dlen;
{
	register u_char *cp;
	register unsigned hval;
	register int n;

	n = dlen;
	if (n > 8)
		n = 8;
	hval = 0;
	while (--n >= 0) {
		hval <<= 1;
		hval += *dp++;
	}
	return (hval % INVHASHSZ);
}
#endif /*INVQ*/

/* int
 * nhash(name)
 *	compute hash for this name and return it; ignore case differences
 */
int
nhash(name)
	register const char *name;
{
	register u_char ch;
	register unsigned hval;

	hval = 0;
	while ((ch = (u_char)*name++) != (u_char)'\0') {
		if (isascii(ch) && isupper(ch))
			ch = tolower(ch);
		hval <<= 1;
		hval += ch;
	}
	return (hval % INVHASHSZ);
}

/*
** SAMEDOMAIN -- Check whether a name belongs to a domain
** ------------------------------------------------------
**
**	Returns:
**		TRUE if the given name lies in the domain.
**		FALSE otherwise.
**
**	Trailing dots are first removed from name and domain.
**	Always compare complete subdomains, not only whether the
**	domain name is the trailing string of the given name.
**
**	"host.foobar.top" lies in "foobar.top" and in "top" and in ""
**	but NOT in "bar.top"
**
**	this implementation of samedomain() is thanks to Bob Heiney.
*/

int
samedomain(a, b)
	const char *a, *b;
{
	size_t la, lb;
	int diff, i, escaped;
	const char *cp;

	la = strlen(a);
	lb = strlen(b);

	/* ignore a trailing label separator (i.e. an unescaped dot) in 'a' */
	if (la && a[la-1] == '.') {
		escaped = 0;
		/* note this loop doesn't get executed if la==1 */
		for (i = la - 2; i >= 0; i--)
			if (a[i] == '\\') {
				if (escaped)
					escaped = 0;
				else
					escaped = 1;
			} else {
				break;
			}
		if (!escaped)
			la--;
	}
	/* ignore a trailing label separator (i.e. an unescaped dot) in 'b' */
	if (lb && b[lb-1] == '.') {
		escaped = 0;
		/* note this loop doesn't get executed if lb==1 */
		for (i = lb - 2; i >= 0; i--)
			if (b[i] == '\\') {
				if (escaped)
					escaped = 0;
				else
					escaped = 1;
			} else {
				break;
			}
		if (!escaped)
			lb--;
	}

	/* lb==0 means 'b' is the root domain, so 'a' must be in 'b'. */
	if (lb == 0)
		return (1);

	/* 'b' longer than 'a' means 'a' can't be in 'b'. */
	if (lb > la)
		return (0);

	/* We use strncasecmp because we might be trying to
	 * ignore a trailing dot. */
	if (lb == la)
		return (strncasecmp(a, b, lb) == 0);

	/* Ok, we know la > lb. */

	diff = la - lb;

	/* If 'a' is only 1 character longer than 'b', then it can't be
	   a subdomain of 'b' (because of the need for the '.' label
	   separator). */
	if (diff < 2)
		return (0);

	/* If the character before the last 'lb' characters of 'b'
	   isn't '.', then it can't be a match (this lets us avoid
	   having "foobar.com" match "bar.com"). */
	if (a[diff-1] != '.')
		return (0);

	/* We're not sure about that '.', however.  It could be escaped
           and thus not a really a label separator. */
	escaped=0;
	for (i = diff-2; i >= 0; i--)
		if (a[i] == '\\') {
			if (escaped)
				escaped = 0;
			else
				escaped = 1;
		}
		else
			break;
	if (escaped)
		return (0);
	  
	/* We use strncasecmp because we might be trying to
	 * ignore trailing dots. */
	cp = a + diff;
	return (strncasecmp(cp, b, lb) == 0);
}

/*
 * Since the fields in a "struct timeval" are longs, and the argument to ctime
 * is a pointer to a time_t (which might not be a long), here's a bridge.
 */
char *
ctimel(l)
	long l;
{
	time_t t = (time_t)l;

	return (ctime(&t));
}

/*
 * This is nec'y for systems that croak when deref'ing unaligned pointers.
 * SPARC is an example.  Note that in_addr.s_addr needn't be a 32-bit int,
 * so we want to avoid bcopy and let the compiler do the casting for us.
 */
struct in_addr
data_inaddr(data)
	const u_char *data;
{
	struct in_addr ret;
	u_int32_t tmp;

	GETLONG(tmp, data);
	ret.s_addr = htonl(tmp);
	return (ret);
}

/* Signal abstraction. */

void
setsignal(catch, block, handler)
	int catch, block;
	SIG_FN (*handler)();
{
#ifdef POSIX_SIGNALS
	/* Modern system - preferred. */
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	if (block != -1)
		sigaddset(&sa.sa_mask, block);
	(void) sigaction(catch, &sa, NULL);
#else /*POSIX_SIGNALS*/
#ifdef SYSV
	/* Ancient system - ugly. */
	if (block != -1)
		syslog(LOG_DEBUG, "danger - unable to block signal %d from %d",
		       block, catch);
	(void) signal(catch, handler);
#else /*SYSV*/
	/* BSD<=4.3 system - odd. */
	struct sigvec sv;
	bzero(&sv, sizeof sv);
	sv.sv_handler = handler;
	sv.sv_mask = sigmask(block);
	(void) sigvec(catch, &sv, NULL);
#endif /*SYSV*/
#endif /*POSIX_SIGNALS*/
}

void
resignal(catch, block, handler)
	int catch, block;
	SIG_FN (*handler)();
{
#if !defined(POSIX_SIGNALS) && defined(SYSV)
	/* Unreliable signals.  Set it back up again. */
	setsignal(catch, block, handler);
#endif
}

void
db_free(dp)
	struct databuf *dp;
{
	int bytes = DATASIZE(dp->d_size);

	if (dp->d_rcnt != 0)
		panic(-1, "db_free: d_rcnt != 0");
	if (dp->d_flags & DB_F_ACTIVE)
		panic(-1, "db_free: DB_F_ACTIVE set");
	if (dp->d_next)
		panic(-1, "db_free: d_next != 0");
	memset(dp, 0x5E, bytes);
	free((char*)dp);
}
