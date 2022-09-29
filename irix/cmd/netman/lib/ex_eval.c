/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Simplify a numeric, address, or string expression.
 * NB: ex_eval knows that ex_kid is equivalent to ex_right.
 */
#include "expr.h"

ExprType
ex_eval(Expr *ex, ExprError *err)
{
	Expr *rex;
	Expr *lex;
	ExprType type;

	switch (ex->ex_arity) {
	  case EX_BINARY:
		lex = ex->ex_left;
		if (lex->ex_op != EXOP_NUMBER && lex->ex_op != EXOP_ADDRESS
		    && lex->ex_op != EXOP_STRING) {
			return ET_COMPLEX;
		}
		/* FALL THROUGH */
	  case EX_UNARY:
		rex = ex->ex_right;
		if (rex->ex_op != EXOP_NUMBER && rex->ex_op != EXOP_ADDRESS
		    && rex->ex_op != EXOP_STRING) {
			return ET_COMPLEX;
		}

		type = ex_operate(ex, ex->ex_op, lex, rex, err);
		if (type == ET_SIMPLE) {
			switch (ex->ex_arity) {
			  case EX_BINARY:
				ex_destroy(lex);
				/* FALL THROUGH */
			  case EX_UNARY:
				ex_destroy(rex);
			}
		}
		break;

	  case EX_NULLARY:
		if (ex->ex_op != EXOP_NUMBER && ex->ex_op != EXOP_ADDRESS
		    && ex->ex_op != EXOP_STRING) {
			return ET_COMPLEX;
		}
		type = ET_SIMPLE;
	}
	return type;
}
