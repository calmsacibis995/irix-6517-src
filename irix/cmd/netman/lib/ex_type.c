/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Recursive descent filter expression parser.
 */
#include "expr.h"
#include "protocol.h"
#include "scope.h"

Expr *
ex_typematch(Expr *ex, enum exop type, Scope **scp)
{
	int depth;
	enum exop op;
	ProtoField *pf;

	depth = 0;
	for (;;) {
		if (ex == 0)
			return 0;
		op = ex->ex_op;
		switch (op) {
		  case EXOP_ARRAY:
			ex = ex->ex_left;
			depth++;
			continue;
		  case EXOP_STRUCT:
			ex = ex->ex_right;
			continue;
		  case EXOP_PROTOCOL:
			if (ex->ex_member) {
				ex = ex->ex_member;
				continue;
			}
			switch (type) {
			  case EXOP_STRUCT:	/* protocol == struct, */
			  case EXOP_PROTOCOL:	/* syntactically */
			  case EXOP_NUMBER:
				return ex;
			}
			return 0;
		  case EXOP_FIELD:
			pf = ex->ex_field;
			while (--depth >= 0) {
				if (pf->pf_type != EXOP_ARRAY)
					return 0;
				pf = pf_element(pf);
			}
			if (pf->pf_type != type)
				return 0;
			/*
			 * If we matched a struct field, set *scp to point
			 * at its scope, so any member name that follows can
			 * be found.
			 */
			if (type == EXOP_STRUCT)
				*scp = &pf_struct(pf)->pst_scope;
			return ex;
		  case EXOP_STRING:
			return (type == op || type == EXOP_ARRAY) ? ex : 0;
		  default:
			switch (ex->ex_arity) {
			  case EX_BINARY:
				if (!ex_typematch(ex->ex_left, type, scp))
					return 0;
				ex = ex->ex_right;
				continue;
			  case EX_UNARY:
				ex = ex->ex_kid;
				continue;
			  case EX_NULLARY:
				return (type == op) ? ex : 0;
			}
		}
	}
	/* NOTREACHED */
}
