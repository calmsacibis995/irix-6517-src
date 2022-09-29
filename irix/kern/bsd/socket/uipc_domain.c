/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)uipc_domain.c	7.3 (Berkeley) 6/29/88
 */

#include <sys/cdefs.h>
#include "tcp-param.h"
#include "sys/param.h"
#include "sys/socket.h"
#include "sys/protosw.h"
#include "sys/domain.h"
#include "sys/debug.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/systm.h"
#include "net/route.h"
#include "sys/atomic_ops.h"
#include "sys/sema.h"
#include "sys/sockd.h"
#include "sys/errno.h"

#define	ADDDOMAIN(x)	{ \
	extern struct domain __CONCAT(x,domain); \
	__CONCAT(x,domain.dom_next) = domains; \
	domains = &__CONCAT(x,domain); \
}

mutex_t	domain_lock;

struct domain *domains;
char pftimo_active = 0;

extern int uds_add(void);
void domain_drain(void);

void
domaininit(void)
{
	register struct domain *dp;
	register struct protosw *pr;

	DOMAIN_LOCK_INIT;
	(void)uds_add();
	ADDDOMAIN(raw);
	ADDDOMAIN(inet);
#ifdef INET6
	ADDDOMAIN(inet6);
#endif
	ADDDOMAIN(atmarp);
	ADDDOMAIN(route);		/* must be last */

	for (dp = domains; dp; dp = dp->dom_next) {
		if (dp->dom_init)
			(*dp->dom_init)();
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_init)
				(*pr->pr_init)();
	}

	/* turn on the timeout processing by sockd */
	pftimo_active = 1;
}

struct protosw *
pffindtype(int family,
	   int type,
	   int *error)
{
	register struct domain *dp;
	register struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		if (dp->dom_family == family)
			goto found;
	if (error)
	  *error = EAFNOSUPPORT;
	return (0);
found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_type && pr->pr_type == type)
			return (pr);
	if (error)
	  *error = EPROTONOSUPPORT;
	return (0);
}

struct protosw *
pffindproto(int family,
	    int protocol,
	    int type,
	    int *error)
{
	register struct domain *dp;
	register struct protosw *pr;
	struct protosw *maybe = 0;
	int protofound = 0;

	if (family == 0) {
	        if (error)
		  *error = EAFNOSUPPORT;
		return (0);
	}
	for (dp = domains; dp; dp = dp->dom_next)
		if (dp->dom_family == family)
			goto found;
	if (error)
	  *error = EAFNOSUPPORT;
	return (0);
found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if (pr->pr_protocol == protocol) {
		        protofound = 1;
			if (pr->pr_type == type)
			  return (pr);
		}

		if (type == SOCK_RAW && pr->pr_type == SOCK_RAW &&
		    pr->pr_protocol == 0 && maybe == (struct protosw *)0)
			maybe = pr;
	}
	if (!maybe && error) {
	        if (protofound)
		        *error = EPROTOTYPE;
		else 
		        *error = EPROTONOSUPPORT;
	}
	return (maybe);
}

void
pfctlinput(int cmd,
	   struct sockaddr *sa)
{
	register struct domain *dp;
	register struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_ctlinput)
				(*pr->pr_ctlinput)(cmd, sa, 0);
}

void
pfslowtimo(void)
{
	register struct domain *dp;
	register struct protosw *pr;
	extern int mbuf_reclaim_needed;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_slowtimo)
				(*pr->pr_slowtimo)();

	/*
	 * If the allocator is gasping for air, try to help.
	 */
	if (mbuf_reclaim_needed) {
		domain_drain();
	}
}

void
pffasttimo(void)
{
	register struct domain *dp;
	register struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_fasttimo)
				(*pr->pr_fasttimo)();
}

int
domain_add(struct domain *dp)
{
	struct protosw	*pr;

	if (!dp)
		return(1);

	DOMAIN_LOCK(s);
	/* Add to beginning of list */
	dp->dom_next = domains;
	domains = dp;

	/* Call init routine */
	if (dp->dom_init)
		(*dp->dom_init)();

	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_init)
			(*pr->pr_init)();

	DOMAIN_UNLOCK(s);
	if (dp->dom_rtattach) {
		ROUTE_WRLOCK();
		dp->dom_rtattach((void**)&rt_tables[dp->dom_family],
				 dp->dom_rtoffset);
		ROUTE_UNLOCK();
	}

	return(0);
}

int
domain_del(struct domain *remove)
{
	struct  domain	*dp, *prev;

	DOMAIN_LOCK(s);
	for (prev = dp = domains; dp; dp = dp->dom_next) {
		if (remove == dp) {
			if (prev == dp) /* Head of list */
				domains = dp->dom_next;
			else
				prev->dom_next = dp->dom_next;

			/*
			 * do not worry about turning off routing for now 
			 * we'll need to hold the route lock if we change this
			 */
			ASSERT(dp->dom_rtattach == 0);

			DOMAIN_UNLOCK(s);
			return(0);
		}
	}

	DOMAIN_UNLOCK(s);
	return(1);
}

#ifdef MBUF_DEBUG
int domain_drain_debug = 0;
#endif

void
domain_drain(void)
{
	register struct domain *dp;
	register struct protosw *pr;
	extern int mbuf_reclaim_needed;
	extern uint mbdrain;

	if (mbuf_reclaim_needed == 0) {
		/* someone beat us to it */
		return;
	}
	DOMAIN_LOCK(s);
	/*
	 * We hope that none of the drain routines use the domain lock, or
	 * we have a problem.
	 */
	for (dp = domains; dp; dp = dp->dom_next) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
			if (pr->pr_drain) {
#ifdef MBUF_DEBUG
				if (domain_drain_debug) {
					printf("domain_drain: calling 0x%x\n",
						pr->pr_drain);		
				}
#endif
				(*pr->pr_drain)();
			}
		}
	}
	DOMAIN_UNLOCK(s);
	atomicClearInt(&mbuf_reclaim_needed, 0xff);
	atomicAddUint(&mbdrain, 1);
}
