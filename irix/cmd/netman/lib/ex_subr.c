/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Expression error reporters.
 */
#include "exception.h"
#include "expr.h"
#include "protocol.h"

/*
 * Common error code for protocol compile functions.
 */
void
ex_badop(Expr *ex, ExprError *err)
{
	ex_error(ex, "illegal operand", err);
}

/*
 * Expression error information reporter.
 */
void
ex_error(Expr *ex, char *message, ExprError *err)
{
	err->err_message = message;
	err->err_token = ex->ex_token;
	if (ex->ex_src)
		err->err_source = *ex->ex_src;
}
