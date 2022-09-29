/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Basic block operations.
 */
#include <stdio.h>
#include "analyze.h"
#include "basicblock.h"
#include "bitset.h"
#include "debug.h"
#include "heap.h"
#include "pidl.h"
#include "scope.h"
#include "treenode.h"

extern FILE		*bbfile;

static BasicBlock	**prevp;
static BasicBlock	**dfn_to_bb;
static unsigned int	bbtotal;

static char *
leadername(BasicBlock *bb)
{
	TreeNode *tn;
	static char buf[12];

	tn = bb->bb_leader;
	switch (tn->tn_op) {
	  case opNumber:
		(void) sprintf(buf, "%ld", tn->tn_val);
		return buf;
	  case opImport:
	  case opNest:
	  case opDeclaration:
	  case opEnumerator:
	  case opCall:
	  case opName:
		return tn->tn_sym->s_name;
	  default:
		return opnames[tn->tn_op];
	}
}

#define	BBTRACE(bb,pred,name)	if (bbfile) { bbtrace(bb, pred, name); }
#define	MODULE(tn)		((tn)->tn_module ? (tn)->tn_module : "")

static void
bbtrace(BasicBlock *bb, BasicBlock *pred, char *name)
{
	TreeNode *tn, *predtn;
	char *module;

	tn = bb->bb_leader;
	predtn = pred->bb_leader;
	module = MODULE(tn);
	fprintf(bbfile,
		"%s@%s:%u %s@%s:%u %s@%s:%u %s@%s:%u %s\n",
		leadername(pred), module, predtn->tn_lineno,
		leadername(bb), module, tn->tn_lineno,
		leadername(pred), module, predtn->tn_lineno,
		leadername(bb), module, tn->tn_lineno,
		name);
}

static void
succeed(BasicBlock *bb, BasicBlock *pred)
{
	while (pred) {
		if (pred->bb_succ != bb) {
			if (pred->bb_succ)
				succeed(bb, pred->bb_succ);
			else {
				if (prevp != &pred->bb_prev) {
					*prevp = pred;
					prevp = &pred->bb_prev;
				}
				BBTRACE(bb, pred, "succeed");
				pred->bb_succ = bb;
				bb->bb_prefcnt++;
			}
		}
		pred = pred->bb_prev;
	}
}

BasicBlock *
basicblock(Context *cx, TreeNode *leader, BasicBlock *prev, BasicBlock *pred)
{
	BasicBlock *bb;

	bb = new(BasicBlock);
	bb->bb_scope = cx->cx_scope;
	bb->bb_leader = first(leader);
	bb->bb_next = 0;
	bb->bb_prev = prev;
	if (prev && prev->bb_next == 0)
		prev->bb_next = bb;
	bb->bb_succ = 0;
	bb->bb_pred = pred;
	bb->bb_prefcnt = 0;
	bb->bb_visited = 0;
	bb->bb_formal = prev ? prev->bb_formal : 0;	/* XXX */
	bb->bb_width = 0;
	bb->bb_gen = bitset(lastfid);
	bb->bb_out = bitset(lastfid);
	bb->bb_in = bitset(lastfid);

	/*
	 * If bb is the first block in a scope, set the scope's fid in "gen".
	 * Otherwise, because PIDL is goto-less, we know that bb succeeds all
	 * of its predecessor's siblings.
	 */
	if (pred == 0)
		(void) insertbit(bb->bb_gen, cx->cx_scope->s_sym->s_fid);
	else if (pred->bb_succ == 0) {
		prevp = &pred->bb_prev;
		succeed(bb, pred);
	}
	cx->cx_bbcount++;
	return bb;
}

static void
searchblocks(BasicBlock *bb, Context *cx)
{
	BasicBlock *succ;
	int dfnum;

	bb->bb_visited = 1;
	for (succ = bb->bb_succ; succ; succ = succ->bb_next) {
		if (!succ->bb_visited) {
			BBTRACE(succ, bb, "searchblocks");
			searchblocks(succ, cx);
		}
	}
	dfnum = --cx->cx_bbcount;
	bb->bb_dfnum = dfnum;
	dfn_to_bb[dfnum] = bb;
}

static TreeNode *
findfield(BasicBlock *bb, unsigned short fid)
{
	TreeNode *tn;
	Symbol *sym;

	for (tn = bb->bb_leader; tn; tn = tn->tn_next) {
		if (tn->tn_op != opDeclaration)
			continue;
		sym = tn->tn_sym;
		assert(sym);
		if (sym->s_type == stField && sym->s_fid == fid)
			return tn;
		if (tn->tn_flags & tfEndOfBlock)
			break;
	}
	return 0;
}

#ifdef DEBUG
static void
dbs(char *name, BitSet *bs)
{
	int i, j, first;

	if (bs == 0)
		return;
	printf("\t%s {", name);
	first = 1;
	for (i = 0; i < bs->bs_veclen; i++) {
		for (j = 0; j < 32; j++) {
			if (bs->bs_vec[i] & (1<<j)) {
				printf("%s%u", first ? "" : ",", i * 32 + j);
				first = 0;
			}
		}
	}
	printf("}\n");
}

static void
dbb(BasicBlock *bb)
{
	TreeNode *tn;

	tn = bb->bb_leader;
	printf("bb@0x%x: leader %s (\"%s\", %u)\n",
		bb, leadername(bb), MODULE(tn), tn->tn_lineno);
	printf("\tnext 0x%x, prev 0x%x, succ 0x%x, pred 0x%x\n",
		bb->bb_next, bb->bb_prev, bb->bb_succ, bb->bb_pred);
	printf("\tprefcnt %u, visited %u, dfnum %u\n",
		bb->bb_prefcnt, bb->bb_visited, bb->bb_dfnum);
	dbs("gen", bb->bb_gen);
	dbs("out", bb->bb_out);
	dbs("in", bb->bb_in);
}
#endif /* DEBUG */

#ifdef NOTDEF
static int
checkguard(TreeNode *tn, void *sym)
{
	TreeNode *member;

	member = 0;
	if (tn->tn_op == opMember) {
		member = tn;
		tn = tn->tn_right;
	}
	if (tn->tn_op == opName && tn->tn_sym == sym) {
		if (member) {
			destroytree(member->tn_left);
			destroytree(tn);
			tn = member;
			tn->tn_arity = 0;
		}
		tn->tn_op = opNumber;
		tn->tn_val = 1;
	}
	return 1;
}
#endif

static int
checkforward(BasicBlock *bb, TreeNode *fname, int bitoff)
{
	TreeNode *decl, *guard, *next, *cond;
	unsigned short fid;
	int width, found, count;
	BasicBlock *succ;

	decl = fname->tn_sym->s_tree;
	fid = decl->tn_sym->s_fid;
	bb->bb_visited = 1;
	if (memberbit(bb->bb_gen, fid)) {
		width = decl->tn_bitoff - bb->bb_leader->tn_bitoff;
		if (width > 0)
			fname->tn_guard = bitseek(width);
		return 1;
	}
	guard = fname->tn_guard;
	found = 0;
	width = bb->bb_width - bitoff;
	for (succ = bb->bb_succ; succ; succ = succ->bb_next) {
		if (succ->bb_visited)
			continue;
		fname->tn_guard = 0;
		count = checkforward(succ, fname, 0);
		if (count == 0)
			continue;
		found += count;
		next = fname->tn_guard;
#ifdef NOTDEF
		cond = succ->bb_leader->tn_guard;
		if (cond) {
			cond = copytree(cond);
			walktree(cond, checkguard, fname->tn_sym);
		}
#else
		cond = 0;
#endif
		if (width <= 0) {
			if (cond && next)
				next = binary(opAnd, cond, next);
		} else if (cond) {
			if (next == 0)
				next = binary(opAnd, bitseek(width), cond);
			else {
				next = binary(opAnd,
					      binary(opAnd,bitseek(width),cond),
					      next);
			}
		} else if (next) {
			if (next->tn_op == opBitSeek) {
				next->tn_val += width;	/* add adjacent seeks */
			} else {
				next = binary(opAnd, bitseek(width), next);
			}
		} else {
			next = bitseek(width);
		}
		guard = (guard == 0) ? next
			: binary(opOr, guard, unary(opBitRewind, 0, next));
		/* XXX rewinds to tell; need stack of tells */
	}
	fname->tn_guard = guard;
	return found;
}

static int
checknames(TreeNode *tn, void *arg)
{
	Symbol *sym;
	unsigned int fid, n, m;
	BasicBlock *bb, *pred;

	if (tn->tn_op != opName)
		return 1;
	sym = tn->tn_sym;
	fid = sym->s_fid;
	bb = (BasicBlock *) arg;

	switch (sym->s_type) {
	  case stProtocol:
	  case stStruct:
		if (memberbit(bb->bb_in, fid)
		    && !bb->bb_formal /* XXX */) {
			/* XXX should hoist name guard to common predecessor */
			n = m = 0;
			for (pred = bb->bb_pred; pred; pred = pred->bb_prev) {
				if (memberbit(pred->bb_gen, fid)
				    || memberbit(pred->bb_in, fid)) {
					m++;
				}
				n++;
			}
			if (n > 0 && m == n) {	/* XXX bug here */
				(void) removebit(bb->bb_gen, fid);
				destroytree(tn->tn_guard);
				tn->tn_guard = 0;
			}
		}
		if (&SN(sym->s_tree)->sn_scope == bb->bb_scope)
			return 1;	/* XXX need global dataflow */
		break;

	  case stField:
		if (sym->s_scope != bb->bb_scope)
			break;
		if (memberbit(bb->bb_in, fid))
			tn->tn_flags |= tfBackwardRef;
		else if (!(tn->tn_flags & tfBackwardRef)) {
			if (memberbit(bb->bb_gen, fid))
				tn->tn_flags |= tfLocalRef|tfForwardRef;
			else {
				for (n = bb->bb_dfnum; n < bbtotal; n++)
					dfn_to_bb[n]->bb_visited = 0;
				if (checkforward(bb, tn, tn->tn_bitoff))
					tn->tn_flags |= tfForwardRef;
				else {
					tnerror(tn, WARNING, "%s unreachable",
						sym->s_name);
				}
			}
		}
	}
	return 0;
}

int
checkblocks(BasicBlock *bb, Context *cx)
{
	int n, i, bit, ok, change;
	BitSet *in, *err;
	BasicBlock *pred;
	TreeNode *tn;

	n = bbtotal = cx->cx_bbcount;
	dfn_to_bb = vnew(n, BasicBlock *);
	searchblocks(bb, cx);
	ok = 1;
	for (i = 0; i < n; i++) {
		bb = dfn_to_bb[i];
		(void) assignbits(bb->bb_out, bb->bb_gen);
	}

	do {
		change = 0;
		for (i = 0; i < n; i++) {
			bb = dfn_to_bb[i];
			in = bitset(bb->bb_in->bs_len);
			for (pred = bb->bb_pred; pred; pred = pred->bb_prev) {
				BBTRACE(bb, pred, "checkblocks");
				(void) unionbits(in, in, pred->bb_out);
			}
			if (equalbits(in, bb->bb_in))
				destroybitset(in);
			else {
				destroybitset(bb->bb_in);
				bb->bb_in = in;
				err = bitset(in->bs_len);
				(void) intersectbits(err, in, bb->bb_gen);
				while ((bit = firstbit(err)) >= 0) {
					tn = findfield(bb, bit);
					if (tn) {
						tnerror(tn, ERROR,
							"redeclaration of %s",
							tn->tn_sym->s_name);
						ok = 0;
					}
					(void) removebit(err, bit);
				}
				destroybitset(err);
				(void) unionbits(bb->bb_out, in, bb->bb_gen);
				change = 1;
			}
		}
	} while (change);

	for (i = 0; i < n; i++) {
		bb = dfn_to_bb[i];
		for (tn = bb->bb_leader; tn; tn = tn->tn_next) {
			switch (tn->tn_op) {
			  case opExport:
			  case opStruct:
			  case opMacro:
			  case opEnum:
			  case opEnumerator:	/* nothing to do */
				break;

			  case opIfElse:
				walktree(tn->tn_kid3, checknames, bb);
				break;

			  case opSwitch:
				walktree(tn->tn_left, checknames, bb);
				break;

			  case opDeclaration:
				assert(tn->tn_sym);
				if (tn->tn_sym->s_type != stField)
					break;
				/* FALL THROUGH */

			  default:
				walktree(tn, checknames, bb);
			}
			if (tn->tn_flags & tfEndOfBlock)
				break;
		}
	}
	delete(dfn_to_bb);
	return ok;
}

void
destroyblocks(BasicBlock *bb)
{
	BasicBlock *succ, *next;

	for (succ = bb->bb_succ; succ; succ = next) {
		next = succ->bb_next;
		if (--succ->bb_prefcnt == 0)
			destroyblocks(succ);
	}
	destroybitset(bb->bb_gen);
	destroybitset(bb->bb_out);
	destroybitset(bb->bb_in);
	delete(bb);
}
