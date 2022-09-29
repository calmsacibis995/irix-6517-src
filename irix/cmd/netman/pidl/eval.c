/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL expression evaluation.
 */
#include <stdlib.h>
#include <string.h>
#include <values.h>
#include "debug.h"
#include "heap.h"
#include "macros.h"
#include "treenode.h"
#include "type.h"

#define	simple(op)	(opNumber <= (op) && (op) <= opString)

int
eval(TreeNode *tn, typeword_t tw)
{
	TreeNode *kt, *lt, *rt;

	assert(tn);
	if (tn->tn_op == opList)
		return 0;
	kt = lt = 0;
	switch (tn->tn_arity) {
	  case 3:
		kt = tn->tn_kid3;
		if (kt == 0 || !simple(kt->tn_op) && !eval(kt, tw))
			return 0;
		/* FALL THROUGH */
	  case 2:
		lt = tn->tn_kid2;
		if (lt == 0 || !simple(lt->tn_op) && !eval(lt, tw))
			return 0;
		/* FALL THROUGH */
	  case 1:
		rt = tn->tn_kid1;
		if (rt == 0 || !simple(rt->tn_op) && !eval(rt, tw))
			return 0;
		if (!operate(tn, tn->tn_op, kt, lt, rt))
			return 0;
		destroytree(kt);
		destroytree(lt);
		destroytree(rt);
	}
	if (!simple(tn->tn_op))
		return 0;
	return convert(tn, tw);
}

int
convert(TreeNode *tn, typeword_t ctw)
{
	typeword_t tw;
	enum basetype bt, cbt;
	enum typequal tq, ctq;

	if (tn->tn_op == opCast)
		return 1;
	tw = tn->tn_type;
	if (ctw.tw_base == btNil)
		ctw = tw;
	if (tw.tw_base == btNil)
		tw = ctw;
	else {
		if (ordinaltype(tw) != ordinaltype(ctw))
			goto bad;
		for (;;) {
			tq = (enum typequal)qualifier(tw);
			ctq = (enum typequal)qualifier(ctw);
			if (tq == tqNil && ctq == tqNil)
				break;
			if (tq == tqNil || ctq == tqNil)
				goto bad;
			if (tq == tqArray)
				tq = tqPointer;
			if (ctq == tqArray)
				ctq = tqPointer;
			if (tq != ctq)
				goto bad;
			tw = unqualify(tw), ctw = unqualify(ctw);
		}
	}
	bt = (enum basetype)tw.tw_base;
	if (bt == btEnum) {
		tw = typesym(tw)->s_tree->tn_enumtype;
		bt = (enum basetype)tw.tw_base;
	}
	cbt = (enum basetype)ctw.tw_base;
	if (cbt == btEnum) {
		ctw = typesym(ctw)->s_tree->tn_enumtype;
		cbt = (enum basetype)ctw.tw_base;
	}
	if (isarithmetic(bt)) {
		if (bt < cbt)
			setbasetype(&tw, cbt);
		if (bt < btInt)
			setbasetype(&tw, issigned(bt) ? btInt : btUInt);
	} else {
		if (bt != cbt)
			goto bad;
	}
	tw.tw_bitfield = 0;
	tn->tn_type = tw;
	return 1;
bad:
	tnerror(tn, ERROR, "%s operand%s incompatible types",
		opnames[tn->tn_op],
		(tn->tn_arity > 1) ? "s have" : "has");
	return 0;
}

int
test(TreeNode *tn)
{
	switch (tn->tn_op) {
	  case opNumber:
		return tn->tn_val != 0;
	  case opAddress:
		return tn->tn_addr.a_high != 0 || tn->tn_addr.a_high != 0;
	  case opString:
		return *tn->tn_sym->s_name != '\0';
	}
	return 0;
}

static void
badoperand(TreeNode *tn)
{
	tnerror(tn, ERROR, "illegal operand");
}

#define	addrlo(tn)	((tn)->tn_addr.a_low)
#define	addrhi(tn)	((tn)->tn_addr.a_high)
#define	BITSPERADDRHALF	(sizeof(unsigned long) * BITSPERBYTE)

static long
cmp(enum operator op, TreeNode *ex1, TreeNode *ex2)
{
	long diff;
	int len1, len2;

	switch (ex1->tn_op) {
	  case opAddress:
		diff = addrhi(ex1) - addrhi(ex2);
		return diff != 0 ? diff : addrlo(ex1) - addrlo(ex2);
	  case opString:
		len1 = ex1->tn_sym->s_namlen, len2 = ex2->tn_sym->s_namlen;
		if ((op == opEQ || op == opNE) && len1 != len2)
			return 1;
		return memcmp(ex1->tn_sym->s_name, ex2->tn_sym->s_name,
			      MIN(len1, len2));
	}
	return -1;
}

/*
 * Perform a simple op on operands lt and rt and store the result in tn.
 * NB: tn may be the same pointer as lt or rt.
 */
int
operate(TreeNode *tn, enum operator op, TreeNode *kt, TreeNode *lt,
	TreeNode *rt)
{
	switch (op) {
	  case opCond:
		*tn = test(kt) ? *lt : *rt;
		return 1;	/* avoid common successful return code */

	  case opComma:
		*tn = *rt;
		return 1;	/* avoid common successful return code */

	  case opOr:
		tn->tn_op = opNumber;
		tn->tn_val = test(lt) && test(rt);
		break;

	  case opAnd:
		tn->tn_op = opNumber;
		tn->tn_val = test(lt) || test(rt);
		break;

	  case opEQ:
		if (lt->tn_op != rt->tn_op) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = opNumber;
		tn->tn_val = (lt->tn_op == opNumber) ?
			     lt->tn_val == rt->tn_val :
			     !cmp(op, lt, rt);
		break;

	  case opNE:
		if (lt->tn_op != rt->tn_op) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = opNumber;
		tn->tn_val = (lt->tn_op == opNumber) ?
			     lt->tn_val != rt->tn_val :
			     cmp(op, lt, rt);
		break;

	  case opLE:
		if (lt->tn_op != rt->tn_op) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = opNumber;
		tn->tn_val = (lt->tn_op == opNumber) ?
			     lt->tn_val <= rt->tn_val :
			     cmp(op, lt, rt) <= 0;
		break;

	  case opLT:
		if (lt->tn_op != rt->tn_op) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = opNumber;
		tn->tn_val = (lt->tn_op == opNumber) ?
			     lt->tn_val < rt->tn_val :
			     cmp(op, lt, rt) < 0;
		break;

	  case opGE:
		if (lt->tn_op != rt->tn_op) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = opNumber;
		tn->tn_val = (lt->tn_op == opNumber) ?
			     lt->tn_val >= rt->tn_val :
			     cmp(op, lt, rt) >= 0;
		break;

	  case opGT:
		if (lt->tn_op != rt->tn_op) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = opNumber;
		tn->tn_val = (lt->tn_op == opNumber) ?
			     lt->tn_val > rt->tn_val :
			     cmp(op, lt, rt) > 0;
		break;

	  case opBitOr:
		if (lt->tn_op != rt->tn_op || lt->tn_op == opString) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = lt->tn_op;
		if (tn->tn_op == opNumber)
			tn->tn_val = lt->tn_val | rt->tn_val;
		else {
			addrlo(tn) = addrlo(lt) | addrlo(rt);
			addrhi(tn) = addrhi(lt) | addrhi(rt);
		}
		break;

	  case opBitAnd:
		if (lt->tn_op != rt->tn_op || lt->tn_op == opString) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = lt->tn_op;
		if (tn->tn_op == opNumber)
			tn->tn_val = lt->tn_val & rt->tn_val;
		else {
			addrlo(tn) = addrlo(lt) & addrlo(rt);
			addrhi(tn) = addrhi(lt) & addrhi(rt);
		}
		break;

	  case opBitXor:
		if (lt->tn_op != rt->tn_op || lt->tn_op == opString) {
			badoperand(lt);
			return 0;
		}
		tn->tn_op = lt->tn_op;
		if (tn->tn_op == opNumber)
			tn->tn_val = lt->tn_val ^ rt->tn_val;
		else {
			addrlo(tn) = addrlo(lt) ^ addrlo(rt);
			addrhi(tn) = addrhi(lt) ^ addrhi(rt);
		}
		break;

	  case opLsh: {
		unsigned long shift;

		if (lt->tn_op == opString || rt->tn_op != opNumber) {
			badoperand(lt->tn_op == opString ? lt : rt);
			return 0;
		}
		shift = rt->tn_val;
		tn->tn_op = lt->tn_op;
		if (tn->tn_op == opNumber) {
			tn->tn_val = lt->tn_val << shift % BITSPERADDRHALF;
		} else if (shift >= BITSPERADDRHALF) {
			addrhi(tn) = addrlo(lt)
			    << (shift - BITSPERADDRHALF) % BITSPERADDRHALF;
			addrlo(tn) = 0;
		} else {
			addrhi(tn) = addrhi(lt) << shift;
			addrhi(tn) |= addrlo(lt) >> (BITSPERADDRHALF - shift);
			addrlo(tn) = addrlo(lt) << shift;
		}
		break;
	  }

	  case opRsh: {
		unsigned long shift;

		if (lt->tn_op == opString || rt->tn_op != opNumber) {
			badoperand(lt->tn_op == opString ? lt : rt);
			return 0;
		}
		shift = rt->tn_val;
		tn->tn_op = lt->tn_op;
		if (tn->tn_op == opNumber) {
			tn->tn_val = lt->tn_val >> shift % BITSPERADDRHALF;
		} else if (shift >= BITSPERADDRHALF) {
			addrlo(tn) = addrhi(lt)
			    >> (shift - BITSPERADDRHALF) % BITSPERADDRHALF;
			addrhi(tn) = 0;
		} else {
			addrlo(tn) = addrlo(lt) >> shift;
			addrlo(tn) |= addrhi(lt) << (BITSPERADDRHALF - shift);
			addrhi(tn) = addrhi(lt) >> shift;
		}
		break;
	  }

	  case opAdd:
		if (lt->tn_op == opString || rt->tn_op == opString) {
			badoperand(lt->tn_op == opString ? lt : rt);
			return 0;
		}
		if (lt->tn_op == opNumber && rt->tn_op == opNumber) {
			tn->tn_op = opNumber;
			tn->tn_val = lt->tn_val + rt->tn_val;
		} else {
			unsigned long lhigh, llow, rhigh, rlow, lowsum;

			if (lt->tn_op == opAddress)
				lhigh = addrhi(lt), llow = addrlo(lt);
			else
				lhigh = 0, llow = lt->tn_val;
			if (rt->tn_op == opAddress)
				rhigh = addrhi(rt), rlow = addrlo(rt);
			else
				rhigh = 0, rlow = rt->tn_val;
			lowsum = llow + rlow;
			tn->tn_op = opAddress;
			addrhi(tn) = lhigh + rhigh + (lowsum < MAX(rlow, llow));
			addrlo(tn) = lowsum;
		}
		break;

	  case opSub:
		if (lt->tn_op == opString || rt->tn_op == opString) {
			badoperand(lt->tn_op == opString ? lt : rt);
			return 0;
		}
		if (lt->tn_op == opNumber && rt->tn_op == opNumber) {
			tn->tn_op = opNumber;
			tn->tn_val = lt->tn_val - rt->tn_val;
		} else {
			unsigned long lhigh, llow, rhigh, rlow, lowdiff;

			if (lt->tn_op == opAddress)
				lhigh = addrhi(lt), llow = addrlo(lt);
			else
				lhigh = 0, llow = lt->tn_val;
			if (rt->tn_op == opAddress)
				rhigh = addrhi(rt), rlow = addrlo(rt);
			else
				rhigh = 0, rlow = rt->tn_val;
			lowdiff = llow - rlow;
			tn->tn_op = opAddress;
			addrhi(tn) = lhigh - rhigh - (lowdiff > llow);
			addrlo(tn) = lowdiff;
		}
		break;

	  case opMul:
		if (lt->tn_op == opString || rt->tn_op == opString) {
			badoperand(lt->tn_op == opString ? lt : rt);
			return 0;
		}
		if (lt->tn_op == opNumber && rt->tn_op == opNumber) {
			tn->tn_op = opNumber;
			tn->tn_val = lt->tn_val * rt->tn_val;
		} else {
			unsigned long lhigh, llow, rhigh, rlow;

			if (lt->tn_op == opAddress)
				lhigh = addrhi(lt), llow = addrlo(lt);
			else
				lhigh = 0, llow = lt->tn_val;
			if (rt->tn_op == opAddress)
				rhigh = addrhi(rt), rlow = addrlo(rt);
			else
				rhigh = 0, rlow = rt->tn_val;
			tn->tn_op = opAddress;
			addrlo(tn) = llow * rlow;
			addrhi(tn) = lhigh * rlow + llow * rhigh
				+ (addrlo(tn) <= MAX(llow, rlow)
				    && MIN(llow, rlow) != 1);
		}
		break;

	  case opDiv:
		if (lt->tn_op == opString || rt->tn_op != opNumber) {
			badoperand(lt->tn_op == opString ? lt : rt);
			return 0;
		}
		if (rt->tn_val == 0) {
			tnerror(rt, ERROR, "division by zero");
			return 0;
		}
		if (lt->tn_op == opNumber) {
			tn->tn_op = opNumber;
			tn->tn_val = lt->tn_val / rt->tn_val;
		} else {
			/* TODO: use algorithm D */
			badoperand(lt);
			return 0;
		}
		break;

	  case opMod:
		if (lt->tn_op == opString || rt->tn_op != opNumber) {
			badoperand(lt->tn_op == opString ? lt : rt);
			return 0;
		}
		if (rt->tn_val == 0) {
			tnerror(rt, ERROR, "modulus with zero");
			return 0;
		}
		if (lt->tn_op == opNumber) {
			tn->tn_op = opNumber;
			tn->tn_val = lt->tn_val % rt->tn_val;
		} else {
			tn->tn_op = opAddress;
			addrhi(tn) = 0;
			addrlo(tn) = addrlo(lt) % rt->tn_val;
		}
		break;

	  case opNot:
		tn->tn_op = opNumber;
		tn->tn_val = !test(rt);
		break;

	  case opBitNot:
		if (rt->tn_op == opString) {
			badoperand(rt);
			return 0;
		}
		tn->tn_op = rt->tn_op;
		if (tn->tn_op == opNumber)
			tn->tn_val = ~rt->tn_val;
		else {
			addrhi(tn) = ~addrhi(rt);
			addrlo(tn) = ~addrlo(rt);
		}
		break;

	  case opNeg:
		if (rt->tn_op == opString) {
			badoperand(rt);
			return 0;
		}
		tn->tn_op = rt->tn_op;
		if (tn->tn_op == opNumber)
			tn->tn_val = -rt->tn_val;
		else {
			tn->tn_op = opAddress;
			addrlo(tn) = ~addrlo(rt) + 1;
			addrhi(tn) = ~addrhi(rt) + (addrlo(tn) == 0);
		}
		break;

	  case opSizeOf:
		tn->tn_op = opNumber;
		switch (rt->tn_op) {
		  case opNumber:
			tn->tn_val = sizeof rt->tn_val;
			break;
		  case opAddress:
			tn->tn_val = sizeof rt->tn_addr;
			break;
		  case opString:
			tn->tn_val = tn->tn_sym->s_namlen;
		}
		break;

	  default:
		return 0;
	}

	tn->tn_arity = 0;
	return 1;
}
