/*
 * Unifdef expression tree operations.
 */
#include "unifdef.h"

void
destroyexpr(struct expr *exp)
{
	if (exp == 0)
		return;
	switch (exp->e_arity) {
	  case 0:
		switch (exp->e_op) {
		  case EDEFINED:
		  case EIDENT:
			dropsym(exp->e_sym);
		}
		break;
	  case 1:
		destroyexpr(exp->e_kid);
		break;
	  case 2:
		destroyexpr(exp->e_left);
		destroyexpr(exp->e_right);
		break;
	  case 3:
		destroyexpr(exp->e_cond);
		destroyexpr(exp->e_true);
		destroyexpr(exp->e_false);
	}
	delete(exp);
}

static enum symtype dontcare(struct expr *, struct expr *, long);
static enum symtype simplify(struct expr *, struct expr *, long);

static char divbyzero[] = "divide by zero";

enum symtype
evalexpr(struct expr *exp)
{
	enum symtype type, rtype;
	struct expr *kid, *rkid;
	long val, rval;

	if (exp == 0)
		return SUNDEFINED;
	switch (exp->e_arity) {
	  case 0:
		switch (exp->e_op) {
		  case EDEFINED:
		  case EIDENT:
			type = exp->e_sym->type;
			if (type == SDONTCARE)
				return type;
			if (type == SRESERVED)
				return SDONTCARE;
			dropsym(exp->e_sym);
			exp->e_val = exp->e_sym->value;
			exp->e_reduced = 1;
			break;
		  case ENUMBER:
			return SDEFINED;
		}
		break;

	  case 1:
		kid = exp->e_kid;
		type = evalexpr(kid);
		if (type != SDEFINED)
			return type;
		val = kid->e_val;
		exp->e_reduced = kid->e_reduced;
		destroyexpr(kid);
		switch (exp->e_op) {
		  case ENOT:
			exp->e_val = !val;
			break;
		  case EBITNOT:
			exp->e_val = ~val;
			break;
		  case ENEG:
			exp->e_val = -val;
		}
		break;

	  case 2:
		kid = exp->e_left, rkid = exp->e_right;
		type = evalexpr(kid), rtype = evalexpr(rkid);
		if (type == SDONTCARE && rtype == SDONTCARE)
			return type;

		exp->e_reduced = kid->e_reduced || rkid->e_reduced;
		if (type != SDONTCARE) {
			val = (type == SDEFINED) ? kid->e_val : 0;
			destroyexpr(kid);
		}
		if (rtype != SDONTCARE) {
			rval = (rtype == SDEFINED) ? rkid->e_val : 0;
			destroyexpr(rkid);
		}

		if (type == SDONTCARE)
			return dontcare(exp, kid, rval);
		if (rtype == SDONTCARE)
			return dontcare(exp, rkid, val);

		switch (exp->e_op) {
		  case EOR:
			exp->e_val = val || rval;
			break;
		  case EAND:
			exp->e_val = val && rval;
			break;
		  case EEQ:
			exp->e_val = val == rval;
			break;
		  case ENE:
			exp->e_val = val != rval;
			break;
		  case ELT:
			exp->e_val = val < rval;
			break;
		  case ELE:
			exp->e_val = val <= rval;
			break;
		  case EGT:
			exp->e_val = val > rval;
			break;
		  case EGE:
			exp->e_val = val >= rval;
			break;
		  case EBITOR:
			exp->e_val = val | rval;
			break;
		  case EBITXOR:
			exp->e_val = val ^ rval;
			break;
		  case EBITAND:
			exp->e_val = val & rval;
			break;
		  case ELSH:
			exp->e_val = val << rval;
			break;
		  case ERSH:
			exp->e_val = val >> rval;
			break;
		  case EADD:
			exp->e_val = val + rval;
			break;
		  case ESUB:
			exp->e_val = val - rval;
			break;
		  case EMUL:
			exp->e_val = val * rval;
			break;
		  case EDIV:
			if (rval != 0)
				exp->e_val = val / rval;
			else {
				yyerror(divbyzero);
				exp->e_val = 0;
			}
			break;
		  case EMOD:
			if (rval != 0)
				exp->e_val = val % rval;
			else {
				yyerror(divbyzero);
				exp->e_val = 0;
			}
		}
		break;

	  case 3:
		kid = exp->e_cond;
		type = evalexpr(kid);
		if (type == SDONTCARE) {
			(void) evalexpr(exp->e_true);
			(void) evalexpr(exp->e_false);
			return type;
		}
		if (type == SDEFINED && kid->e_val) {
			destroyexpr(exp->e_false);
			rkid = exp->e_true;
			rtype = evalexpr(rkid);
		} else {
			destroyexpr(exp->e_true);
			rkid = exp->e_false;
			rtype = evalexpr(rkid);
		}
		destroyexpr(kid);
		if (rtype == SDONTCARE) {
			*exp = *rkid;
			delete(rkid);
			return rtype;
		}
		exp->e_val = (rtype == SDEFINED && rkid->e_val);
		exp->e_reduced = 1;
	}
	exp->e_op = ENUMBER;
	exp->e_arity = 0;
	return SDEFINED;
}

static enum symtype
dontcare(struct expr *exp, struct expr *kid, long val)
{
	switch (exp->e_op) {
	  case EOR:
		if (val)
			return simplify(exp, kid, 1);
		break;
	  case EAND:
		if (!val)
			return simplify(exp, kid, 0);
		break;
	  case EADD:
	  case ESUB:
		if (val != 0)
			return SDONTCARE;
		break;
	  case EMUL:
		if (val == 0)
			return simplify(exp, kid, val);
		if (val != 1)
			return SDONTCARE;
		break;
	  case EDIV:
	  case EMOD:
		if (val == 0) {
			if (kid == exp->e_left)
				yyerror(divbyzero);
			return simplify(exp, kid, val);
		}
		if (val != 1)
			return SDONTCARE;
		break;
	  default:
		return SDONTCARE;
	}
	*exp = *kid;
	delete(kid);
	return SDONTCARE;
}

static enum symtype
simplify(struct expr *exp, struct expr *kid, long val)
{
	destroyexpr(kid);
	exp->e_op = ENUMBER;
	exp->e_arity = 0;
	exp->e_reduced = 1;
	exp->e_val = val;
	return SDEFINED;
}
