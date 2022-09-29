/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * 	Expression XDR functions
 *
 *	$Revision: 1.8 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <bstring.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <values.h>
#include "heap.h"
#include "protocol.h"
#include "scope.h"

bool_t xdr_expr(XDR *xdr, Expr **exp, Scope *sc);

/*
 * XDR an Expr structure
 */
static bool_t
xdr_s_expr(XDR *xdr, Expr *ex, Scope *sc)
{
	static Symbol snoopsym;
	extern Protocol snoop_proto;

	if (!xdr_enum(xdr, (enum_t *) &ex->ex_op))
		return FALSE;

	if (!xdr_enum(xdr, (enum_t *) &ex->ex_arity))
		return FALSE;

	/* Don't transfer ex_token and ex_src */
	if (xdr->x_op == XDR_DECODE) {
		ex->ex_token = 0;
		ex->ex_src = 0;
	}

	switch (ex->ex_op) {
	    case EXOP_STRUCT:
		if (!xdr_expr(xdr, &ex->ex_left, sc))
			return FALSE;
		if (!ex_typematch(ex->ex_left, EXOP_STRUCT, &sc))
			return FALSE;
		if (!xdr_expr(xdr, &ex->ex_right, sc))
			return FALSE;
		break;

	    case EXOP_PROTOCOL:
	    {
		Protocol *pr;

		if (xdr->x_op == XDR_DECODE) {
			Symbol *sym;
			unsigned int protoid;

			if (!xdr_u_int(xdr, &protoid))
				return FALSE;
			pr = findprotobyid(protoid);
			if (pr == 0)
				return FALSE;
			if (pr == &snoop_proto) {
				if (snoopsym.sym_name == 0) {
					snoopsym.sym_name = pr->pr_name;
					snoopsym.sym_namlen = pr->pr_namlen;
					snoopsym.sym_type = SYM_PROTOCOL;
					snoopsym.sym_proto = &snoop_proto;
				}
				sym = &snoopsym;
			} else {
				sym = sc_lookupsym(sc, pr->pr_name,
						   pr->pr_namlen);
			}
			if (sym == 0 || sym->sym_type != SYM_PROTOCOL)
				return FALSE;
			ex->ex_prsym = sym;
			sc = &pr->pr_scope;
		} else {
			pr = ex->ex_prsym->sym_proto;
			sc = &pr->pr_scope;
			if (!xdr_int(xdr, &pr->pr_id))
				return FALSE;
		}
		if (!xdr_expr(xdr, &ex->ex_member, sc))
			return FALSE;
		break;
	    }

	    case EXOP_FIELD:
		if (xdr->x_op == XDR_DECODE) {
			int pfid;

			if (!xdr_int(xdr, &pfid))
				return FALSE;
			ex->ex_field = sc_findfield(sc, pfid);
			if (ex->ex_field == 0)
				return FALSE;
		} else {
			if (!xdr_int(xdr, &ex->ex_field->pf_id))
				return FALSE;
		}
		break;

	    case EXOP_NUMBER:
		if (!xdr_long(xdr, &ex->ex_val))
			return FALSE;
		break;

	    case EXOP_ADDRESS:
		if (!xdr_opaque(xdr, &ex->ex_addr, sizeof ex->ex_addr))
			return FALSE;
		break;

	    case EXOP_STRING:
		if (!xdr_bytes(xdr, &ex->ex_str.s_ptr,
			       (unsigned int *) &ex->ex_str.s_len, MAXINT))
			return FALSE;
		break;

	    case EXOP_FETCH:
		if (!xdr_u_int(xdr, &ex->ex_size) ||
		    !xdr_u_int(xdr, &ex->ex_off) ||
		    !xdr_enum(xdr, (enum_t *) &ex->ex_type))
			return FALSE;
		break;

	    case EXOP_CALL:
	    {
		Symbol *sym;
		int n;

		if (xdr->x_op == XDR_DECODE) {
			char *name = 0;

			if (!xdr_wrapstring(xdr, &name))
				return FALSE;
			sym = sc_lookupsym(sc, name, -1);
			if (sym == 0 || sym->sym_type != SYM_FUNCTION)
				return FALSE;

			ex->ex_func = sym->sym_func.fd_func;
		} else {
			int (*func)() = ex->ex_func;

			n = sc->sc_length;
			for (sym = sc->sc_table; ; sym++) {
				if (--n < 0)
					return FALSE;
				if (sym->sym_type != SYM_FUNCTION)
					continue;
				if (sym->sym_func.fd_func == func)
					break;
			}
			if (!xdr_wrapstring(xdr, &sym->sym_name))
				return FALSE;
		}
		for (n = 0; n < MAXCALLARGS; n++) {
			if (ex->ex_args[n] == 0)
				break;
			if (!xdr_expr(xdr, &ex->ex_args[n], sc))
				return FALSE;
		}
		break;
	    }
	    default:
		switch (ex->ex_arity) {
		    case EX_BINARY:
			if (!xdr_expr(xdr, &ex->ex_left, sc))
				return FALSE;
			/* Fall through ... */

		    case EX_UNARY:
			if (!xdr_expr(xdr, &ex->ex_right, sc))
				return FALSE;
			break;
		}
		break;
	}

	return TRUE;
}

/*
 * XDR a pointer to an Expr structure
 */
bool_t
xdr_expr(XDR *xdr, Expr **exp, Scope *sc)
{
	bool_t more = (*exp != 0);
	bool_t freeit = FALSE;
	bool_t stat;

	if (!xdr_bool(xdr, &more))
		return FALSE;

	if (!more) {
		*exp = 0;
		return TRUE;
	}

	if (*exp == 0) {
		switch (xdr->x_op) {
		    case XDR_FREE:
			return TRUE;

		    case XDR_DECODE:
			freeit = TRUE;
			*exp = expr(EXOP_NULL, EX_NULLARY, 0);
			break;
		}
	}

	stat = xdr_s_expr(xdr, *exp, sc);

	if (!stat && freeit || xdr->x_op == XDR_FREE) {
		ex_destroy(*exp);
		*exp = 0;
	}

	return stat;
}
