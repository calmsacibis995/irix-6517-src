/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_ipaddr.c,v 1.2 1996/07/06 17:50:47 nn Exp $"

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

jmp_buf nisjmp;

#define	MAXHASH 1024  /* must be a power of 2 */

struct hostdata {
	struct hostdata *h_next;
	char		*h_hostname;
	struct in_addr	h_ipaddr;
	int		h_pktsout;
	int		h_pktsin;
};

struct hostdata *addhost();

struct hostdata *h_table[MAXHASH];

#define	iphash(e)  ((e) & (MAXHASH-1))

static void
wakeup(n)
	int n;
{
	longjmp(nisjmp, 1);
}

extern char *inet_ntoa();

struct hostdata *
iplookup(ipaddr)
	struct in_addr ipaddr;
{
	register struct hostdata *h;
	struct hostent *hp = NULL;
	struct netent *np;

	for (h = h_table[iphash(ipaddr.s_addr)]; h; h = h->h_next) {
		if (h->h_ipaddr.s_addr == ipaddr.s_addr)
			return (h);
	}

	/* not found.  Put it in */

	if (ipaddr.s_addr == htonl(INADDR_BROADCAST))
		return (addhost(ipaddr, "BROADCAST"));
	if (ipaddr.s_addr == htonl(INADDR_ANY))
		return (addhost(ipaddr, "OLD-BROADCAST"));

	/*
	 * Set an alarm here so we don't get held up by
	 * an unresponsive NIS server.
	 * Give it 3 sec to do its work.
	 */
	if (setjmp(nisjmp) == 0) {
		signal(SIGALRM, wakeup);
		alarm(3);
		hp = gethostbyaddr((char *)&ipaddr, sizeof (int), AF_INET);
		if (hp == NULL && inet_lnaof(ipaddr) == 0) {
		np = getnetbyaddr(inet_netof(ipaddr), AF_INET);
		if (np)
			return (addhost(ipaddr, np->n_name));
		}
		alarm(0);
	} else {
		hp = NULL;
	}

	return (addhost(ipaddr, hp ? hp->h_name : inet_ntoa(ipaddr)));
}

struct hostdata *
addhost(ipaddr, name)
	struct in_addr ipaddr;
	char *name;
{
	register struct hostdata **hp, *n;
	extern FILE *namefile;
	char *p;

	n = (struct hostdata *) malloc(sizeof (*n));
	if (n == NULL)
		goto alloc_failed;

	/*
	 * nn
	 * If the name isn't in A.B.C.D numeric format,
	 * then trim off the domainname presumably after the first dot.
	 */
	if (isalpha(*name) && (p = strchr(name, '.')))
		*p = '\0';

	memset(n, 0, sizeof (*n));
	n->h_hostname = strdup(name);
	if (n->h_hostname == NULL)
		goto alloc_failed;
	n->h_ipaddr = ipaddr;
	hp = &h_table[iphash(ipaddr.s_addr)];
	n->h_next = *hp;
	*hp = n;

	if (namefile)
		(void) fprintf(namefile, "%s\t%s\n",
			inet_ntoa(ipaddr), name);
	return (n);

alloc_failed:
	(void) fprintf(stderr, "addhost: no mem\n");
	return (NULL);
}

char *
addrtoname(ipaddr)
	struct in_addr ipaddr;
{
	return(iplookup(ipaddr)->h_hostname);
}

void
load_names(fname)
	char *fname;
{
	char addr [32];
	char name [256];
	FILE *f;
	struct in_addr iaddr;

	(void) fprintf(stderr, "Loading name file %s\n", fname);
	f = fopen(fname, "r");
	if (f == NULL) {
		perror(fname);
		return;
	}

	while (fscanf(f, "%s%s\n", addr, name) != EOF) {
		iaddr.s_addr = inet_addr(addr);
		(void) addhost(iaddr, name);
	}

	(void) fclose(f);
}
