/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Nest a higher-layer protocol in a lower one, called the parent.
 * The nested protocol is identified in the parent by a long integer
 * protocol type code.  The parent may use a long int qualifier when
 * matching a packet with the nested protocol's type.
 */
#include "protocol.h"
#include "scope.h"

int
pr_nest(Protocol *pr, int parentid, long prototype, ProtoMacro *pm, int npm)
{
	return pr_nestqual(pr, parentid, prototype, 0L, pm, npm);
}

int
pr_nestqual(Protocol *pr, int parentid, long prototype, long qualifier,
	    ProtoMacro *pm, int npm)
{
	Protocol *parent;
	Symbol *sym;

	parent = findprotobyid(parentid);
	if (parent == 0)
		return 0;
	pr_embed(parent, pr, prototype, qualifier);
	sym = pr_addsym(parent, pr->pr_name, pr->pr_namlen, SYM_PROTOCOL);
	sym->sym_proto = pr;
	sym->sym_prototype = prototype;
	if (pm)
		pr_addmacros(parent, pm, npm);
	return 1;
}

int
pr_unnest(Protocol *pr, int parentid, long prototype)
{
	return pr_unnestqual(pr, parentid, prototype, 0L);
}

int
pr_unnestqual(Protocol *pr, int parentid, long prototype, long qualifier)
{
	Protocol *parent;
	Symbol *sym;

	parent = findprotobyid(parentid);
	if (parent == 0)
		return 0;
	pr_embed(parent, 0, prototype, qualifier);
	pr_deletesym(parent, pr->pr_name, pr->pr_namlen);
	return 1;
}
