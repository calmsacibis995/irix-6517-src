/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Expression node constructor and destructor.
 */
#include <bstring.h>
#include "expr.h"
#include "heap.h"

static Expr *exfreelist;	/* private expr allocation list */

Expr *
expr(enum exop op, enum exarity arity, char *token)
{
	Expr *ex;
	int len;

	ex = exfreelist;
	if (ex == 0) {
		len = 16;
		ex = exfreelist = vnew(len, Expr);
		while (--len > 0) {
			ex->ex_kid = ex + 1;
			ex++;
		}
		ex->ex_kid = 0;
		ex = exfreelist;
	}
	exfreelist = ex->ex_kid;
	ex->ex_op = op;
	ex->ex_arity = arity;
	ex->ex_count = 1;
	ex->ex_token = token;
	ex->ex_src = 0;
	bzero(&ex->ex_u, sizeof ex->ex_u);
	return ex;
}

/*
 * NB: ex_destroy knows that ex_kid is equivalent to ex_right.
 */
void
ex_destroy(Expr *ex)
{
	int n;

	if (ex == 0 || --ex->ex_count != 0)
		return;
	switch (ex->ex_arity) {
	  case EX_NULLARY:
		switch (ex->ex_op) {
		  case EXOP_PROTOCOL:
			ex_destroy(ex->ex_member);
			break;
		  case EXOP_CALL:
			for (n = 0; n < MAXCALLARGS; n++) {
				if (ex->ex_args[n])
					ex_destroy(ex->ex_args[n]);
			}
		}
		break;
	  case EX_BINARY:
		ex_destroy(ex->ex_left);
		/* FALL THROUGH */
	  case EX_UNARY:
		ex_destroy(ex->ex_right);
		break;
	}
	ex_dropsrc(ex);
	ex->ex_kid = exfreelist;
	exfreelist = ex;
}

void
ex_holdsrc(Expr *ex, ExprSource *src)
{
	ex->ex_src = holdExprSource(src);
}

void
ex_dropsrc(Expr *ex)
{
	ExprSource *src;

	src = ex->ex_src;
	dropExprSource(src);
	ex->ex_src = 0;
}
