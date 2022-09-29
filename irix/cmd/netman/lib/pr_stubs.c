/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Stubs for optional or generic protocol entry points.
 */
#include "protodefs.h"

/* ARGSUSED */
int
pr_stub_setopt(int id, char *val)
{
	return 0;
}

/* ARGSUSED */
void
pr_stub_embed(Protocol *pr, long prototype, long qualifier)
{
}

/* ARGSUSED */
Expr *
pr_stub_resolve(char *name, int len, struct snooper *sn)
{
	return 0;
}

/* ARGSUSED */
ExprType
pr_stub_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	return ET_COMPLEX;
}

/* ARGSUSED */
int
pr_stub_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	return 0;
}

/* ARGSUSED */
int
pr_stub_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	return ds_field(ds, pf, 0, rex);
}
