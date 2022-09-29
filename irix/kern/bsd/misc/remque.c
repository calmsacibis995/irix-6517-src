/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * VAX insque/remque instruction emulator.
 */
#ident "$Revision: 3.5 $"
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/mips_addrspace.h>
#include <sys/debug.h>

/*
 * generic list struct for use with _insque/_remque
 * any structure that is linked via _insque/_remque should have the
 * list pointers as the first two elements
 */
struct generic_list {
	struct generic_list *link;	/* forward link */
	struct generic_list *rlink;	/* backward link */
};

void
_insque(register struct generic_list *ep, register struct generic_list *pp)
{
	ep->link = pp->link;
	ep->rlink = pp;
	pp->link->rlink = ep;
	pp->link = ep;
}

void
_remque(register struct generic_list *ep)
{
	ep->rlink->link = ep->link;
	ep->link->rlink = ep->rlink;
}

