/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Return true if ex is simple and non-zero.
 */
#include <bstring.h>
#include "expr.h"

int
ex_test(Expr *ex)
{
	switch (ex->ex_op) {
	  case EXOP_NUMBER:
		return ex->ex_val != 0;
	  case EXOP_ADDRESS:
		return ex->ex_addr.a_high != 0 || ex->ex_addr.a_high != 0;
	  case EXOP_STRING:
		return ex->ex_str.s_len > 0 && *ex->ex_str.s_ptr != '\0';
	}
	return 0;
}
