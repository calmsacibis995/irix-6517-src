/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Compute the result of an expr operation.
 */
#include <string.h>
#include <values.h>
#include "expr.h"
#include "macros.h"

#define	addrlo(ex)	((ex)->ex_addr.a_low)
#define	addrhi(ex)	((ex)->ex_addr.a_high)
#define	BITSPERADDRHALF	(sizeof(unsigned long) * BITSPERBYTE)

static long
cmp(enum exop op, Expr *ex1, Expr *ex2)
{
	if (ex1->ex_op == EXOP_ADDRESS) {
		long diff;

		diff = addrhi(ex1) - addrhi(ex2);
		return diff ? diff : addrlo(ex1) - addrlo(ex2);
	}
	if (ex1->ex_op == EXOP_STRING) {
		int len1, len2;

		len1 = ex1->ex_str.s_len, len2 = ex2->ex_str.s_len;
		if ((op == EXOP_EQ || op == EXOP_NE) && len1 != len2)
			return 1;
		return memcmp(ex1->ex_str.s_ptr, ex2->ex_str.s_ptr,
			      MIN(len1, len2));
	}
	return -1;
}

/*
 * Perform op on operands lex and rex and store the result in ex.
 * NB: ex may be the same pointer as lex or rex.
 */
ExprType
ex_operate(Expr *ex, enum exop op, Expr *lex, Expr *rex, ExprError *err)
{
	switch (op) {
	  case EXOP_OR:
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = ex_test(lex) && ex_test(rex);
		break;

	  case EXOP_AND:
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = ex_test(lex) || ex_test(rex);
		break;

	  case EXOP_EQ:
		if (lex->ex_op != rex->ex_op) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = (lex->ex_op == EXOP_NUMBER) ?
			lex->ex_val == rex->ex_val :
			!cmp(op, lex, rex);
		break;

	  case EXOP_NE:
		if (lex->ex_op != rex->ex_op) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = (lex->ex_op == EXOP_NUMBER) ?
			lex->ex_val != rex->ex_val :
			cmp(op, lex, rex);
		break;

	  case EXOP_LE:
		if (lex->ex_op != rex->ex_op) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = (lex->ex_op == EXOP_NUMBER) ?
			lex->ex_val <= rex->ex_val :
			cmp(op, lex, rex) <= 0;
		break;

	  case EXOP_LT:
		if (lex->ex_op != rex->ex_op) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = (lex->ex_op == EXOP_NUMBER) ?
			lex->ex_val < rex->ex_val :
			cmp(op, lex, rex) < 0;
		break;

	  case EXOP_GE:
		if (lex->ex_op != rex->ex_op) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = (lex->ex_op == EXOP_NUMBER) ?
			lex->ex_val >= rex->ex_val :
			cmp(op, lex, rex) >= 0;
		break;

	  case EXOP_GT:
		if (lex->ex_op != rex->ex_op) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = (lex->ex_op == EXOP_NUMBER) ?
			lex->ex_val > rex->ex_val :
			cmp(op, lex, rex) > 0;
		break;

	  case EXOP_BITOR:
		if (lex->ex_op != rex->ex_op || lex->ex_op == EXOP_STRING) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = lex->ex_op;
		if (ex->ex_op == EXOP_NUMBER)
			ex->ex_val = lex->ex_val | rex->ex_val;
		else {
			addrlo(ex) = addrlo(lex) | addrlo(rex);
			addrhi(ex) = addrhi(lex) | addrhi(rex);
		}
		break;

	  case EXOP_BITAND:
		if (lex->ex_op != rex->ex_op || lex->ex_op == EXOP_STRING) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = lex->ex_op;
		if (ex->ex_op == EXOP_NUMBER)
			ex->ex_val = lex->ex_val & rex->ex_val;
		else {
			addrlo(ex) = addrlo(lex) & addrlo(rex);
			addrhi(ex) = addrhi(lex) & addrhi(rex);
		}
		break;

	  case EXOP_BITXOR:
		if (lex->ex_op != rex->ex_op || lex->ex_op == EXOP_STRING) {
			ex_badop(lex, err);
			return ET_ERROR;
		}
		ex->ex_op = lex->ex_op;
		if (ex->ex_op == EXOP_NUMBER)
			ex->ex_val = lex->ex_val ^ rex->ex_val;
		else {
			addrlo(ex) = addrlo(lex) ^ addrlo(rex);
			addrhi(ex) = addrhi(lex) ^ addrhi(rex);
		}
		break;

	  case EXOP_LSH: {
		unsigned long shift;

		if (lex->ex_op == EXOP_STRING || rex->ex_op != EXOP_NUMBER) {
			ex_badop(lex->ex_op == EXOP_STRING ? lex : rex, err);
			return ET_ERROR;
		}
		shift = rex->ex_val;
		ex->ex_op = lex->ex_op;
		if (ex->ex_op == EXOP_NUMBER) {
			ex->ex_val = lex->ex_val << shift % BITSPERADDRHALF;
		} else if (shift >= BITSPERADDRHALF) {
			addrhi(ex) = addrlo(lex)
			    << (shift - BITSPERADDRHALF) % BITSPERADDRHALF;
			addrlo(ex) = 0;
		} else {
			addrhi(ex) = addrhi(lex) << shift;
			addrhi(ex) |= addrlo(lex) >> BITSPERADDRHALF - shift;
			addrlo(ex) = addrlo(lex) << shift;
		}
		break;
	  }

	  case EXOP_RSH: {
		unsigned long shift;

		if (lex->ex_op == EXOP_STRING || rex->ex_op != EXOP_NUMBER) {
			ex_badop(lex->ex_op == EXOP_STRING ? lex : rex, err);
			return ET_ERROR;
		}
		shift = rex->ex_val;
		ex->ex_op = lex->ex_op;
		if (ex->ex_op == EXOP_NUMBER) {
			ex->ex_val = lex->ex_val >> shift % BITSPERADDRHALF;
		} else if (shift >= BITSPERADDRHALF) {
			addrlo(ex) = addrhi(lex)
			    >> (shift - BITSPERADDRHALF) % BITSPERADDRHALF;
			addrhi(ex) = 0;
		} else {
			addrlo(ex) = addrlo(lex) >> shift;
			addrlo(ex) |= addrhi(lex) << BITSPERADDRHALF - shift;
			addrhi(ex) = addrhi(lex) >> shift;
		}
		break;
	  }

	  case EXOP_ADD:
		if (lex->ex_op == EXOP_STRING || rex->ex_op == EXOP_STRING) {
			ex_badop(lex->ex_op == EXOP_STRING ? lex : rex, err);
			return ET_ERROR;
		}
		if (lex->ex_op == EXOP_NUMBER && rex->ex_op == EXOP_NUMBER) {
			ex->ex_op = EXOP_NUMBER;
			ex->ex_val = lex->ex_val + rex->ex_val;
		} else {
			unsigned long lhigh, llow, rhigh, rlow, lowsum;

			if (lex->ex_op == EXOP_ADDRESS)
				lhigh = addrhi(lex), llow = addrlo(lex);
			else
				lhigh = 0, llow = lex->ex_val;
			if (rex->ex_op == EXOP_ADDRESS)
				rhigh = addrhi(rex), rlow = addrlo(rex);
			else
				rhigh = 0, rlow = rex->ex_val;
			lowsum = llow + rlow;
			ex->ex_op = EXOP_ADDRESS;
			addrhi(ex) = lhigh + rhigh + (lowsum < MAX(rlow, llow));
			addrlo(ex) = lowsum;
		}
		break;

	  case EXOP_SUB:
		if (lex->ex_op == EXOP_STRING || rex->ex_op == EXOP_STRING) {
			ex_badop(lex->ex_op == EXOP_STRING ? lex : rex, err);
			return ET_ERROR;
		}
		if (lex->ex_op == EXOP_NUMBER && rex->ex_op == EXOP_NUMBER) {
			ex->ex_op = EXOP_NUMBER;
			ex->ex_val = lex->ex_val - rex->ex_val;
		} else {
			unsigned long lhigh, llow, rhigh, rlow, lowdiff;

			if (lex->ex_op == EXOP_ADDRESS)
				lhigh = addrhi(lex), llow = addrlo(lex);
			else
				lhigh = 0, llow = lex->ex_val;
			if (rex->ex_op == EXOP_ADDRESS)
				rhigh = addrhi(rex), rlow = addrlo(rex);
			else
				rhigh = 0, rlow = rex->ex_val;
			lowdiff = llow - rlow;
			ex->ex_op = EXOP_ADDRESS;
			addrhi(ex) = lhigh - rhigh - (lowdiff > llow);
			addrlo(ex) = lowdiff;
		}
		break;

	  case EXOP_MUL:
		if (lex->ex_op == EXOP_STRING || rex->ex_op == EXOP_STRING) {
			ex_badop(lex->ex_op == EXOP_STRING ? lex : rex, err);
			return ET_ERROR;
		}
		if (lex->ex_op == EXOP_NUMBER && rex->ex_op == EXOP_NUMBER) {
			ex->ex_op = EXOP_NUMBER;
			ex->ex_val = lex->ex_val * rex->ex_val;
		} else {
			unsigned long lhigh, llow, rhigh, rlow;

			if (lex->ex_op == EXOP_ADDRESS)
				lhigh = addrhi(lex), llow = addrlo(lex);
			else
				lhigh = 0, llow = lex->ex_val;
			if (rex->ex_op == EXOP_ADDRESS)
				rhigh = addrhi(rex), rlow = addrlo(rex);
			else
				rhigh = 0, rlow = rex->ex_val;
			ex->ex_op = EXOP_ADDRESS;
			addrlo(ex) = llow * rlow;
			addrhi(ex) = lhigh * rlow + llow * rhigh
				+ (addrlo(ex) <= MAX(llow, rlow)
				    && MIN(llow, rlow) != 1);
		}
		break;

	  case EXOP_DIV:
		if (lex->ex_op == EXOP_STRING || rex->ex_op != EXOP_NUMBER) {
			ex_badop(lex->ex_op == EXOP_STRING ? lex : rex, err);
			return ET_ERROR;
		}
		if (rex->ex_val == 0) {
			ex_error(rex, "division by zero", err);
			return ET_ERROR;
		}
		if (lex->ex_op == EXOP_NUMBER) {
			ex->ex_op = EXOP_NUMBER;
			ex->ex_val = lex->ex_val / rex->ex_val;
		} else {
			/* XXX use algorithm D */
			ex_badop(lex, err);
			return ET_ERROR;
		}
		break;

	  case EXOP_MOD:
		if (lex->ex_op == EXOP_STRING || rex->ex_op != EXOP_NUMBER) {
			ex_badop(lex->ex_op == EXOP_STRING ? lex : rex, err);
			return ET_ERROR;
		}
		if (rex->ex_val == 0) {
			ex_error(rex, "modulus with zero", err);
			return ET_ERROR;
		}
		if (lex->ex_op == EXOP_NUMBER) {
			ex->ex_op = EXOP_NUMBER;
			ex->ex_val = lex->ex_val % rex->ex_val;
		} else {
			ex->ex_op = EXOP_ADDRESS;
			addrhi(ex) = 0;
			addrlo(ex) = addrlo(lex) % rex->ex_val;
		}
		break;

	  case EXOP_NOT:
		ex->ex_op = EXOP_NUMBER;
		ex->ex_val = !ex_test(rex);
		break;

	  case EXOP_BITNOT:
		if (rex->ex_op == EXOP_STRING) {
			ex_badop(rex, err);
			return ET_ERROR;
		}
		ex->ex_op = rex->ex_op;
		if (ex->ex_op == EXOP_NUMBER)
			ex->ex_val = ~rex->ex_val;
		else {
			addrhi(ex) = ~addrhi(rex);
			addrlo(ex) = ~addrlo(rex);
		}
		break;

	  case EXOP_NEG:
		if (rex->ex_op == EXOP_STRING) {
			ex_badop(rex, err);
			return ET_ERROR;
		}
		ex->ex_op = rex->ex_op;
		if (ex->ex_op == EXOP_NUMBER)
			ex->ex_val = -rex->ex_val;
		else {
			ex->ex_op = EXOP_ADDRESS;
			addrlo(ex) = ~addrlo(rex) + 1;
			addrhi(ex) = ~addrhi(rex) + (addrlo(ex) == 0);
		}
		break;

	  default:
		return ET_COMPLEX;
	}

	ex->ex_arity = EX_NULLARY;
	return ET_SIMPLE;
}
