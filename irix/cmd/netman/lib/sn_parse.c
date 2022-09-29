/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snooper expression filter compilation.
 * XXXbe rename file to sn_compile.c
 */
#include <bstring.h>
#include <errno.h>
#include "exception.h"
#include "expr.h"
#include "snooper.h"

int
sn_compile(Snooper *sn, ExprSource *src, ExprError *err)
{
	Expr *ex;

	if (src == 0) {		/* null means promiscuous */
		ex = 0;
		bzero(err, sizeof *err);
	} else {
		if (sn->sn_rawproto == 0) {
			sn->sn_error = EINVAL;
			exc_raise(0, "cannot compile filter for %s",
				  sn->sn_name);
			goto exception;
		}
		ex = ex_parse(src, sn, sn->sn_rawproto, err);
		if (ex == 0) {
			sn->sn_error = 0;
			goto exprerror;
		}
	}

	if (!sn_add(sn, &ex, err)) {
		ex_destroy(ex);
		if (sn->sn_error) {
			exc_raise(sn->sn_error, "cannot snoop on %s",
				  sn->sn_name);
			goto exception;
		}
		goto exprerror;
	}
	ex_destroy(sn->sn_expr);
	sn->sn_expr = ex;
	return 1;

exprerror:
	exc_raise(0, (err->err_token == 0 || *err->err_token == '\0') ?
		  "%s" : "%s: %.12s...", err->err_message, err->err_token);
	return 0;

exception:
	err->err_message = exc_message;
	err->err_token = 0;
	if (src)
		err->err_source = *src;
	return 0;
}
