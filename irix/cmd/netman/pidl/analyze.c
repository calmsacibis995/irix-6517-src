/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL second pass: abstract syntax tree analysis.
 */
#include <stdio.h>
#include "analyze.h"
#include "basicblock.h"
#include "bitset.h"
#include "debug.h"
#include "macros.h"
#include "pidl.h"
#include "scope.h"
#include "strings.h"
#include "treenode.h"
#include "type.h"

/*
 * Miscellaneous analyze subroutines.
 */
static int	absmin(int, int);
static int	exportlist(TreeNode *, Context *);
static Symbol	*importname(Symbol *, unsigned short, TreeNode *, Context *);
static int	membernode(TreeNode *, Context *,
			   Symbol *(*)(TreeNode *, Context *),
			   int (*)(TreeNode *, Context *));
static Symbol	*analyzeleft(TreeNode *, Context *);
static Symbol	*lookupname(TreeNode *, Context *);

enum nestmode { StaticNest, DynamicNest };

static int	checkbases(TreeNode *, enum nestmode);
static int	checkformals(TreeNode *);
static void	notconstant(TreeNode *, char *);
static int	fieldwidth(TreeNode *, short *, Context *);
static char	*scopepathname(Symbol *);
static char	*typesignature(TreeNode *);
static int	checkoverload(Scope *);
static Symbol	*importscope(TreeNode *, Symbol *, Context *);

/*
 * Registry of callable functions that require special handling.
 */
static int	c_enter_handler(TreeNode *);
static int	c_match_handler(TreeNode *);

static struct calltab {
	char	*ct_name;
	int	(*ct_handler)(TreeNode *);
} calltab[] = {
	{ "c_enter",	c_enter_handler },
	{ "c_match",	c_match_handler },
	{ 0, }
};

int
analyze(TreeNode *tn, Context *cx)
{
	int ok;
	Symbol *sym;

	ok = 1;
	if (tn == 0)
		return ok;

	switch (tn->tn_op) {
	  case opNil:
		break;

	  case opList: {
		TreeNode *kid, *next;
		BasicBlock *bb, *prev;
		int width;

		tn->tn_width = 0;
		tn->tn_bitoff = cx->cx_bitoff;
		for (kid = tn->tn_left; kid; kid = next) {
			next = kid->tn_next;
			bb = cx->cx_block;
			if (bb)
				prev = bb->bb_prev;
			ok &= analyze(kid, cx);
			width = kid->tn_width;
			switch (kid->tn_op) {
			  case opDeclaration:
				if (bb == 0)
					break;
				if (width < 0) {
					if (bb->bb_width > 0)
						bb->bb_width = -bb->bb_width;
				} else if (width > 0) {
					if (bb->bb_width < 0)
						width = -width;
				}
				bb->bb_width += width;
				break;

			  case opIfElse:
			  case opSwitch:
				if (next == 0)
					break;
				cx->cx_block =
				    basicblock(cx, next, prev, cx->cx_block);
				kid->tn_flags |= tfEndOfBlock;
			}
			if (width < 0) {
				if (tn->tn_width > 0)
					tn->tn_width = -tn->tn_width;
			} else if (width > 0) {
				if (tn->tn_width < 0)
					width = -width;
			}
			tn->tn_width += width;
		}
		break;
	  }

	  case opImport: {
		Symbol *modsym;
		unsigned short sflags;

		modsym = cx->cx_modsym;
		cx->cx_modsym = tn->tn_sym;
		sflags = cx->cx_sflags;
		cx->cx_sflags |= sfImported;
		ok = analyze(tn->tn_kid, cx);
		cx->cx_modsym = modsym;
		cx->cx_sflags = sflags;
		break;
	  }

	  case opExport: {
		if (cx->cx_sflags & sfImported)
			cx->cx_modsym->s_flags |= sfExported;
		else
			cx->cx_flags |= cxHeaderFile;
		ok = exportlist(tn->tn_kid, cx);
		break;
	  }

	  case opPragma: {
		break;
	  }

	  case opProtocol: {
		TreeNode *head, *bases, *body, *formals, *foot;
		BasicBlock *bb, *pred, *prev;
		unsigned short sflags;

		/*
		 * Modify context appropriately.
		 */
		cx->cx_proto = tn;
		cx->cx_scope = &SN(tn)->sn_scope;
		assert(cx->cx_bitoff == 0);
		assert(cx->cx_bbcount == 0);
		assert(cx->cx_block == 0);

		/*
		 * Flag this protocol's symbol according to current context.
		 * Analyze base protocols in which this one nests.
		 */
		head = tn->tn_kid3;
		head->tn_sym->s_flags |= cx->cx_sflags;
		bases = head->tn_right;
		ok = analyze(bases, cx);
		ok &= checkbases(bases, StaticNest);

		/*
		 * Analyze the protocol's body, which declares a packet header.
		 * If there is no body, there may yet be formals and a footer.
		 */
		body = tn->tn_kid2;
		if (body == 0)
			pred = 0;
		else {
			cx->cx_block = bb = basicblock(cx, body, 0, 0);
			ok &= analyze(body, cx);
			tn->tn_width = body->tn_width;
			pred = cx->cx_block;
		}

		/*
		 * Formal arguments are compiled within a basic block that
		 * succeeds the last block in body and lies on an alternate
		 * branch from foot.
		 */
		formals = head->tn_left;
		if (formals == 0)
			prev = 0;
		else {
			cx->cx_block = prev = basicblock(cx, formals, 0, pred);
			prev->bb_formal = 1;	/* XXX */
			ok &= analyze(formals, cx);
			ok &= checkformals(formals);
		}

		/*
		 * Flag footer field symbols and check for reachable name
		 * references given pred.
		 */
		foot = tn->tn_kid1;
		if (foot) {
			sflags = cx->cx_sflags;
			cx->cx_sflags |= sfFooter;
			cx->cx_bitoff = 0;
			cx->cx_block = basicblock(cx, body, prev, pred);
			ok &= analyze(foot, cx);
			if (foot->tn_width >= 0 && tn->tn_width >= 0)
				tn->tn_width += foot->tn_width;
			cx->cx_sflags = sflags;
		}
		ok &= checkblocks(bb, cx);

		/*
		 * Save our scope pathname so generate can emit unique names.
		 * Minimize overloaded fields.
		 */
		sym = head->tn_sym;
		SN(tn)->sn_scope.s_path = scopepathname(sym);
		ok &= checkoverload(&SN(tn)->sn_scope);

		/*
		 * Restore analysis context.
		 */
		cx->cx_proto = 0;
		cx->cx_scope = cx->cx_scope->s_outer;
		cx->cx_bitoff = 0;
		destroyblocks(bb);
		cx->cx_block = 0;
		break;
	  }

	  case opBase:
		sym = tn->tn_sym;
		if (tn->tn_module != cx->cx_modsym->s_name
		    && !(sym->s_flags & sfExported)) {
			tnerror(tn, ERROR, "protocol %s is not exported",
				sym->s_name);
			ok = 0;
		}
		ok &= analyze(tn->tn_kid, cx);
		break;

	  case opNest: {
		TreeNode *bases;

		sym = tn->tn_sym;
		switch (sym->s_type) {
		  case stProtocol:
			break;
		  case stNil:
			sym->s_type = stProtocol;
			sym->s_tree = 0;
			break;
		  default:
			tnerror(tn, ERROR,
				"cannot nest %s %s in base protocols",
				symtypes[sym->s_type], sym->s_name);
			ok = 0;
		}
		bases = tn->tn_kid;
		ok &= analyze(bases, cx);
		ok &= checkbases(bases, DynamicNest);
		cx->cx_scope->s_sym->s_flags |= sfDynamicNest;
		break;
	  }

	  case opDeclaration: {
		TreeNode *left, *right;
		unsigned int bitoff;
		typeword_t tw;
		int width, skip;
		BasicBlock *bb;
		Symbol *tsym;

		/*
		 * Process a field, typedef, or cast (sym == 0) declaration.
		 */
		sym = tn->tn_sym;
		left = tn->tn_left, right = tn->tn_right;
		if (sym == 0) {
			ok = analyze(left, cx);
			ok &= analyze(right, cx);
			break;
		}
		switch (sym->s_type) {
		  case stField:
			/*
			 * Analyze type qualifier lengths on the left first,
			 * as they're needed to compute field width.
			 */
			ok = analyze(left, cx)
			   & fieldwidth(tn, &tn->tn_width, cx);
			if (right == 0 || (right->tn_flags & tfExpected))
				width = tn->tn_width;
			else {
				width = 0;
				ok &= analyze(right, cx);
			}
			tn->tn_bitoff = bitoff = cx->cx_bitoff;
			tw = tn->tn_type;
			if (tw.tw_bitfield)
				cx->cx_scope->s_sym->s_flags |= sfBitFields;
			if (width < 0) {
				cx->cx_bitoff = 0;
				cx->cx_scope->s_sym->s_flags |= sfCantInline;
			} else if (width > 0) {
				if (!tw.tw_bitfield) {
					skip = bitoff % BitsPerByte;
					if (skip != 0) {
						tnerror(tn, WARNING,
						    "undeclared bits before %s",
						    sym->s_flags & sfAnonymous ?
						    typename(tw) : sym->s_name);
						skip = BitsPerByte - skip;
						tn->tn_bitoff += skip;
						cx->cx_bitoff += skip;
					} else if (bitoff % width != 0) {
						/*
						 * Unnatural alignment.
						 */
						cx->cx_scope->s_sym->s_flags
							|= sfCantInline;
					}
				}
				cx->cx_bitoff += width;
			}

			bb = cx->cx_block;
			if (bb) {
				if (memberbit(bb->bb_gen, sym->s_fid)) {
					tnerror(tn, ERROR,
						"redeclaration of %s",
						sym->s_name);
					ok = 0;
				}
				(void) insertbit(bb->bb_gen, sym->s_fid);
			}

			if (cx->cx_guard)
				tn->tn_guard = holdnode(cx->cx_guard);
			if (sym->s_flags & sfOverloaded)
				tn->tn_suffix = typesignature(tn);

			switch (tw.tw_base) {
			  case btCache:
				if (!(cx->cx_sflags & sfImported))
					cx->cx_flags |= cxCacheFields;
				break;
			  case btEnum:
				if (!(cx->cx_sflags & sfImported))
					cx->cx_flags |= cxEnumFields;
				/* FALL THROUGH */
			  case btStruct:
				tsym = typesym(tw);
				(void) importname(tsym, 0, tn, cx);
				if (tsym->s_flags & sfCantInline) {
					cx->cx_scope->s_sym->s_flags
						|= sfCantInline;
				}
				if (tsym->s_flags & sfDynamicNest) {
					cx->cx_scope->s_sym->s_flags
						|= sfDynamicNest;
				}
			}
			break;

		  case stTypeDef:
			assert(right == 0);
			ok = analyze(left, cx)
			   & fieldwidth(tn, &tn->tn_width, cx);
			if (sym->s_trefcnt == 0) {
				tnerror(tn, WARNING, "unused typedef %s",
					sym->s_name);
			}
		}

		/*
		 * Add current symbol flags to sym's flags.
		 */
		sym->s_flags |= cx->cx_sflags;
		break;
	  }

	  case opIfElse: {
		TreeNode *cond, *guard, *thenpart, *elsepart;
		BasicBlock *pred, *prev;
		unsigned int bitoff;
		int width, same;

		cx->cx_scope->s_sym->s_flags |= sfCantInline;
		cond = tn->tn_kid3;
		pred = cx->cx_block;
		pred->bb_prev = 0;
		ok = analyze(cond, cx);
		bitoff = cx->cx_bitoff;

		thenpart = tn->tn_kid2;
		cx->cx_block = prev = basicblock(cx, thenpart, 0, pred);
		guard = cx->cx_guard;
		if (guard)
			cond->tn_guard = holdnode(guard);
		cx->cx_guard = cond;
		ok &= analyze(thenpart, cx);
		cx->cx_bitoff = bitoff;

		elsepart = tn->tn_kid1;
		if (elsepart) {
			cx->cx_block = basicblock(cx, elsepart, prev, pred);
			cx->cx_guard = unary(opNot, 0, holdnode(cond));
			if (guard)
				cx->cx_guard->tn_guard = holdnode(guard);
			ok &= analyze(elsepart, cx);
			destroytree(cx->cx_guard);
		}

		width = thenpart->tn_width;
		same = (elsepart && width >= 0 && width == elsepart->tn_width);
		if (same)
			tn->tn_width = width;
		else if (elsepart)
			tn->tn_width = -absmin(width, elsepart->tn_width);
		else
			tn->tn_width = -width;
		tn->tn_bitoff = bitoff;
		cx->cx_bitoff = same ? bitoff + width : 0;
		cx->cx_guard = guard;
		break;
	  }

	  case opSwitch: {
		TreeNode *discrim, *guard, *kid, *label;
		BasicBlock *pred, *prev;
		unsigned int bitoff;
		int width, kidwidth;

		cx->cx_scope->s_sym->s_flags |= sfCantInline;
		discrim = tn->tn_left;
		ok = analyze(discrim, cx);
		if (ok && !ordinaltype(discrim->tn_type)) {
			tnerror(discrim, ERROR,
				"switch discriminant has non-ordinal type %s",
				typename(discrim->tn_type));
			ok = 0;
		}

		pred = cx->cx_block;
		pred->bb_prev = 0;
		prev = 0;
		bitoff = cx->cx_bitoff;
		guard = cx->cx_guard;
		width = kidwidth = 0;

		for (kid = tn->tn_right->tn_left; kid; kid = kid->tn_next) {
			cx->cx_block = prev =
				basicblock(cx, kid->tn_right, prev, pred);
			label = first(kid->tn_left);
			cx->cx_guard = binary(opEQ, holdnode(discrim),
					      holdnode(label));
			while ((label = label->tn_next) != 0) {
				cx->cx_guard = binary(opOr, cx->cx_guard,
						      binary(opEQ,
							     holdnode(discrim),
							     holdnode(label)));
			}
			if (guard)
				cx->cx_guard->tn_guard = holdnode(guard);
			ok &= analyze(kid, cx);
			kidwidth = kid->tn_width;
			if (width == 0)
				width = kidwidth;
			else if (kidwidth != width)
				width = -absmin(width, kidwidth);
			cx->cx_bitoff = bitoff;
			destroytree(cx->cx_guard);
		}

		tn->tn_width = width;
		tn->tn_bitoff = bitoff;
		cx->cx_bitoff = width ? bitoff + width : 0;
		cx->cx_guard = guard;
		break;
	  }

	  case opCase: {
		TreeNode *labels, *label;

		labels = tn->tn_left;
		ok = analyze(labels, cx);
		ok &= analyze(tn->tn_right, cx);
		for (label = first(labels); label; label = label->tn_next) {
			if (label->tn_op != opDefault
			    && label->tn_op != opNumber) {
				notconstant(label, "case label");
				ok = 0;
			}
		}
		break;
	  }

	  case opDefault:
		setbasetype(&tn->tn_type, btInt);
		break;

	  case opStruct: {
		unsigned short bbcount;
		unsigned short bitoff;
		TreeNode *body;
		BasicBlock *block, *bb;

		cx->cx_scope = &SN(tn)->sn_scope;
		bbcount = cx->cx_bbcount;
		bitoff = cx->cx_bitoff;
		cx->cx_bbcount = 0;
		cx->cx_bitoff = 0;
		body = tn->tn_kid;
		block = cx->cx_block;
		cx->cx_block = bb = basicblock(cx, body, 0, 0);

		sym = tn->tn_sym;
		if (!(sym->s_flags & sfTemporary) && sym->s_trefcnt == 0)
			tnerror(tn, WARNING, "unused struct %s", sym->s_name);
		sym->s_flags |= cx->cx_sflags;
		ok = analyze(body, cx);
		tn->tn_width = body->tn_width;
		ok &= checkblocks(bb, cx);
		SN(tn)->sn_scope.s_path = scopepathname(sym);
		ok &= checkoverload(&SN(tn)->sn_scope);

		cx->cx_scope = cx->cx_scope->s_outer;
		cx->cx_bbcount = bbcount;
		destroyblocks(bb);
		cx->cx_bitoff = bitoff;
		cx->cx_block = block;
		break;
	  }

	  case opMacro:
		sym = tn->tn_sym;
		sym->s_flags |= cx->cx_sflags;
		break;

	  case opEnum:
		sym = tn->tn_sym;
		if (sym->s_trefcnt == 0)
			tnerror(tn, WARNING, "unused enum %s", sym->s_name);
		sym->s_flags |= cx->cx_sflags;
		ok = analyze(tn->tn_kid, cx);
		break;

	  case opEnumerator:
		sym = tn->tn_sym;
		sym->s_flags |= cx->cx_sflags;
		break;

	  case opSizeOf: {
		TreeNode *kid;
		int width;

		kid = tn->tn_kid;
		ok = analyze(kid, cx);
		width = kid->tn_width;
		if (width < 0)		/* XXX more to do? */
			break;
		if (width % BitsPerByte) {
			tnerror(tn, ERROR, "sizeof applied to bitfield type");
			ok = width = 0;
		}
		destroytree(kid);
		tn->tn_op = opNumber;
		tn->tn_type.tw_word = 0;
		setbasetype(&tn->tn_type, btUInt);
		tn->tn_arity = 0;
		tn->tn_val = width / BitsPerByte;
		break;
	  }

	  case opIndex: {
		TreeNode *left;

		left = tn->tn_left;
		ok = analyze(left, cx);
		if (ok) {
			if (qualifier(left->tn_type) == tqArray)
				tn->tn_type = unqualify(left->tn_type);
			else {
				tnerror(left, ERROR, "%s is not an array",
					typename(left->tn_type));
				ok = 0;
			}
		}
		ok &= analyze(tn->tn_right, cx);
		break;
	  }

	  case opMember:
		ok = membernode(tn, cx, analyzeleft, analyze);
		if (ok) {
			TreeNode *right, *next;

			right = tn->tn_right;
			switch (cx->cx_looksym->s_type) {
			  case stNumber:
			  case stAddress:
				destroytree(tn->tn_left);
				next = tn->tn_next;
				*tn = *right;
				tn->tn_next = next;
				destroytree(right);
				break;
			  default:
				tn->tn_type = right->tn_type;
			}
		}
		break;

	  case opCall: {
		TreeNode *arg;
		char *name;
		struct calltab *ct;

		for (arg = tn->tn_kid->tn_left; arg; arg = arg->tn_next)
			ok &= analyze(arg, cx);
		name = tn->tn_sym->s_name;
		for (ct = calltab; ct->ct_name; ct++) {
			if (!strcmp(ct->ct_name, name)) {
				ok &= (*ct->ct_handler)(tn);
				break;
			}
		}
		break;
	  }

	  case opBlock: {
		TreeNode *args, *block, *members, *arg, *decl, *bname,
			 *comma, *assign;
		unsigned int fid, cc, n;
		char buf[64];
		typeword_t atw, dtw;

		args = tn->tn_right;
		if (!analyze(args, cx))
			return 0;
		block = tn->tn_left;
		members = block->tn_kid;
		if (members == 0) {
			fid = block->tn_sym->s_fid;
			innermost = &SN(block)->sn_scope;
			members = list(0);
			for (arg = args->tn_left; arg; arg = arg->tn_next) {
				cc = nsprintf(buf, sizeof buf, "mem%u",
					      members->tn_arity);
				sym = enter(&strings, buf, cc, hashname(buf,cc),
					    stNil, sfSaveName);
				decl = declaration(sym, 0, stField);
				sym = decl->tn_sym;
				sym->s_flags |= sfTemporary;
				sym->s_fid = ++fid;
				(void) convert(arg, niltw);
				append(members,
				       declaretype(arg->tn_type, decl));
			}
			block->tn_kid = members;
			(void) analyze(block, cx);
		} else {
			if (args->tn_arity > members->tn_arity) {
				tnerror(args, ERROR,
					"too many arguments for block %s",
					block->tn_sym->s_name);
			}
			for (arg = args->tn_left, decl = members->tn_left, n=1;
			     arg && decl;
			     arg = arg->tn_next, decl = decl->tn_next, n++) {
				(void) convert(arg, niltw);
				atw = arg->tn_type;
				dtw = decl->tn_type;
				if (atw.tw_word != dtw.tw_word) {
					tnerror(arg, ERROR,
					"argument %d type %s does not match %s",
						n, typename(atw),
						typename(dtw));
				}
			}
		}
		arg = args->tn_left;
		bname = name(block->tn_sym);
		comma = 0;
		for (decl = members->tn_left; decl; decl = decl->tn_next) {
			assign = binary(opAssign,
					binary(opMember, holdnode(bname),
					       name(decl->tn_sym)),
					arg ? holdnode(arg) : number(0));
			if (comma == 0)
				comma = assign;
			else
				comma = binary(opComma, comma, assign);
			if (arg)
				arg = arg->tn_next;
		}
		destroytree(args);
		tn->tn_right = binary(opComma, comma,
				      unary(opAddrOf, 0, bname));
		break;
	  }

	  case opName: {
		BasicBlock *bb;
		TreeNode *left, *right;

		sym = lookupname(tn, cx);
		if (sym == 0)
			return 0;
		if (!(sym->s_flags & sfExported)
		    && sym->s_scope->s_module != cx->cx_modsym->s_name) {
			tnerror(tn, ERROR, "%s %s is not exported",
				symtypes[sym->s_type], sym->s_name);
			ok = 0;
		}

		switch (sym->s_type) {
		  case stProtocol:
			cx->cx_looksym = importscope(tn, sym, cx);
			if (cx->cx_looksym == 0)
				ok = 0;
			break;

		  case stField:
			if (cx->cx_lookfun == find) {
				left = treenode(opName, 0);
				(void) importscope(left,sym->s_scope->s_sym,cx);
				right = copytree(tn);
				tn->tn_op = opMember;
				tn->tn_type = sym->s_tree->tn_type;
				tn->tn_arity = 2;
				tn->tn_next = right->tn_next;
				tn->tn_left = left;
				tn->tn_right = right;
				right->tn_next = 0;
				tn = right;
			}
			bb = cx->cx_block;
			if (bb && memberbit(bb->bb_gen, sym->s_fid))
				tn->tn_flags |= tfLocalRef|tfBackwardRef;
			tn->tn_type = sym->s_tree->tn_type;
			tn->tn_bitoff = cx->cx_bitoff;
			cx->cx_looksym = typesym(tn->tn_type);
			break;

		  case stNumber:
			tn->tn_op = opNumber;
			setbasetype(&tn->tn_type, btLong);
			tn->tn_val = sym->s_val;
			cx->cx_looksym = sym;
			break;

		  case stAddress:
			tn->tn_op = opAddress;
			setbasetype(&tn->tn_type, btAddress);
			tn->tn_addr = sym->s_addr;
			cx->cx_looksym = sym;
			break;

		  default:
			tnerror(tn, ERROR, "illegal use of %s %s",
				symtypes[sym->s_type], sym->s_name);
			return 0;
		}
		break;
	  }

	  default:
		switch (tn->tn_arity) {
		  case 3:
			ok &= analyze(tn->tn_kid3, cx);
			/* FALL THROUGH */
		  case 2:
			ok &= analyze(tn->tn_kid2, cx);
			if (ok)
				tn->tn_type = tn->tn_kid2->tn_type;
			/* FALL THROUGH */
		  case 1:
			ok &= analyze(tn->tn_kid1, cx);
			if (ok)
				ok &= convert(tn, tn->tn_kid1->tn_type);
		}
	}

	return ok;
}

static int
absmin(int a, int b)
{
	if (a < 0)
		a = -a;
	if (b < 0)
		b = -b;
	return MIN(a, b);
}

static int
exportlist(TreeNode *tn, Context *cx)
{
	int ok;
	unsigned short sflags;
	TreeNode *left;
	Symbol *sym;

	ok = 1;
	for (tn = first(tn); tn; tn = tn->tn_next) {
		switch (tn->tn_op) {
		  case opProtocol:
		  case opDeclaration:
		  case opStruct:
		  case opEnum:
		  case opBitSet:
			cx->cx_sflags |= sfExported;
			sflags = cx->cx_sflags;
			ok &= analyze(tn, cx);
			cx->cx_sflags = sflags;
			break;

		  case opMember:
			ok &= membernode(tn, cx, lookupname, exportlist);
			if (ok) {
				left = tn->tn_left;
				assert(left->tn_op == opName);
				left->tn_sym->s_flags |= sfExported;
			}
			break;

		  case opName:
			sym = lookupname(tn, cx);
			if (sym == 0) {
				ok = 0;
				continue;
			}
			if (sym->s_scope->s_module != cx->cx_modsym->s_name) {
				tnerror(tn, ERROR, "illegal export of %s",
					sym->s_name);
				ok = 0;
				continue;
			}
			sym->s_flags |= sfExported;
		}
	}
	return ok;
}

static Symbol *
importname(Symbol *sym, unsigned short sflags, TreeNode *decl, Context *cx)
{
	enum symtype stype;
	Scope *s, *t;
	Symbol *nsym;

	stype = sym->s_type;
	s = cx->cx_scope;
	switch (stype) {
	  case stProtocol:
		if (s == sym->s_scope || s->s_sym == sym)
			return sym;
		break;
	  case stEnum:
	  case stStruct:
		for (t = s; t->s_outer; t = t->s_outer) {
			if (t == sym->s_scope)
				return sym;
			if (t->s_sym == sym) {
				if (decl) {
					decl->tn_flags |= tfRecursive;
					cx->cx_flags |= cxHeapFields;
				}
				return sym;
			}
			if (sflags & sfTemporary)
				break;
		}
	}
	nsym = lookup(s, sym->s_name, sym->s_namlen, sym->s_hash);
	if (nsym)
		return nsym;
	nsym = install(s, sym, stype, (sflags | sym->s_flags) & ~sfImported);
	switch (stype) {
	  case stProtocol:
		APPENDSYM(nsym, &s->s_protocols);
		break;
	  case stEnum:
		APPENDSYM(nsym, &s->s_enums);
		break;
	  case stStruct:
		APPENDSYM(nsym, &s->s_structs);
	}
	nsym->s_data = sym->s_data;
	return nsym;
}

static int
membernode(TreeNode *tn, Context *cx,
	   Symbol *(*leftfun)(TreeNode *, Context *),
	   int (*rightfun)(TreeNode *, Context *))
{
	Symbol *sym;
	Scope *scope;
	lookfun_t lookfun;
	int ok;

	sym = (*leftfun)(tn->tn_left, cx);
	if (sym == 0)
		return 0;
	if (sym->s_type != stProtocol && sym->s_type != stStruct) {
		tnerror(tn->tn_left, ERROR,
			"illegal %s on left side of '.'",
			symtypes[sym->s_type]);
		return 0;
	}
	scope = cx->cx_scope;
	cx->cx_scope = &SN(sym->s_tree)->sn_scope;
	lookfun = cx->cx_lookfun;
	cx->cx_lookfun = lookup;
	ok = (*rightfun)(tn->tn_right, cx);
	cx->cx_scope = scope;
	cx->cx_lookfun = lookfun;
	return ok;
}

static Symbol *
analyzeleft(TreeNode *tn, Context *cx)
{
	if (!analyze(tn, cx))
		return 0;
	assert(cx->cx_looksym);
	return cx->cx_looksym;
}

static Symbol *
lookupname(TreeNode *name, Context *cx)
{
	Symbol *sym;

	sym = name->tn_sym;
	sym = (*cx->cx_lookfun)(cx->cx_scope, sym->s_name, sym->s_namlen,
				sym->s_hash);
	if (sym == 0) {
		tnerror(name, ERROR, "%s undefined", name->tn_sym->s_name);
		return 0;
	}
	name->tn_sym = sym;
	return sym;
}

static int
checkbases(TreeNode *bases, enum nestmode mode)
{
	int ok;
	TreeNode *base, *type, *qual, *formal;

	ok = 1;
	for (base = first(bases); base; base = base->tn_next) {
		type = base->tn_kid;
		if (type && type->tn_op == opQualify) {
			qual = type->tn_right;
			type = type->tn_left;
		} else
			qual = 0;
		if (mode == StaticNest) {
			if (type && type->tn_op != opNumber) {
				notconstant(type, "base discriminant");
				ok = 0;
			}
			if (qual && qual->tn_op != opNumber) {
				notconstant(type, "base qualifier");
				ok = 0;
			}
		}
		for (formal = first(base->tn_sym->s_tree->tn_kid3->tn_left);
		     formal; formal = formal->tn_next) {
			if ((formal->tn_op == opQualify) ^ (qual != 0))
				continue;
			/* XXX match actual against formal */
		}
	}
	return ok;
}

static int
checkordinalfield(TreeNode *tn, char *which)
{
	typeword_t tw;
	Symbol *sym;

	tw = tn->tn_type;
	if (!ordinaltype(tw)) {
		tnerror(tn, ERROR, "%s has non-ordinal type %s",
			which, typename(tw));
		return 0;
	}
	while (tn->tn_op != opName) {
		switch (tn->tn_op) {
		  case opIndex:
			tn = tn->tn_left;
			continue;
		  case opMember:
			tn = tn->tn_right;
			continue;
		  default:
			switch (tn->tn_arity) {
			  case 3:
				if (!checkordinalfield(tn->tn_kid3, which))
					return 0;
			  case 2:
				if (!checkordinalfield(tn->tn_kid2, which))
					return 0;
			  case 1:
				if (!checkordinalfield(tn->tn_kid1, which))
					return 0;
			}
			return 1;
		}
	}
	sym = tn->tn_sym;
	if (sym->s_type != stField) {
		tnerror(tn, ERROR, "%s %s is not a field", which, sym->s_name);
		return 0;
	}
	return 1;
}

static int
checkformals(TreeNode *formals)
{
	int ok;
	TreeNode *formal, *discrim, *qual;

	ok = 1;
	for (formal = first(formals); formal; formal = formal->tn_next) {
		if (formal->tn_op == opQualify) {
			discrim = formal->tn_left;
			qual = formal->tn_right;
		} else {
			discrim = formal;
			qual = 0;
		}
		ok &= checkordinalfield(discrim, "discriminant");
		if (qual)
			ok &= checkordinalfield(qual, "qualifier");
	}
	return ok;
}

static void
notconstant(TreeNode *tn, char *what)
{
	tnerror(tn, ERROR, "%s %s",
		tn->tn_arity > 0 || ordinaltype(tn->tn_type) ?
		"variable" : "non-ordinal", what);
}

static int
fieldwidth(TreeNode *decl, short *rwidth, Context *cx)
{
	int ok, width, save, dim;
	Symbol *sym, *tsym;
	typeword_t tw, u_int_tw, *ordinal_tw;
	TreeNode *len, *lenvec[TQMAX];
	enum basetype bt;

	ok = 1;
	sym = decl->tn_sym;
	tw = decl->tn_type;
	tsym = typesym(tw);
	switch (tsym->s_type) {
	  case stBaseType:
		width = tsym->s_desc->td_width;
		break;
	  case stEnum:
		width = BitsPerLong;
		break;
	  case stStruct:
		width = tsym->s_tree->tn_width;
		break;
	  default:
		tnerror(decl, INTERNAL, "%s has bogus type %s",
			sym->s_name, tsym->s_name);
		ok = 0;
	}

	u_int_tw.tw_word = 0;
	setbasetype(&u_int_tw, btUInt);

	if (tw.tw_bitfield) {
		len = decl->tn_left;
		if (len->tn_op != opNumber && !eval(len, u_int_tw)) {
			tnerror(decl, ERROR, "illegal variable-width %s",
				sym->s_name);
			ok = 0;
		} else {
			if (len->tn_val < 0
			    || len->tn_val > BitsPerAddress
			    || len->tn_val > BitsPerLong
			    && len->tn_val % BitsPerByte != 0) {
				tnerror(decl, ERROR, "%s has illegal width %d",
					sym->s_name, len->tn_val);
				ok = 0;
			} else if (len->tn_val > width) {
				tnerror(decl, ERROR,
					"%s is wider than base type %s",
					sym->s_name, tsym->s_name);
				ok = 0;
			}
			width = len->tn_val;
		}

		if (tw.tw_base != btAddress) {
			if (width <= BitsPerChar)
				bt = btUChar;
			else if (width <= BitsPerShort)
				bt = btUShort;
			else if (width <= BitsPerLong)
				bt = btULong;
			if (tw.tw_base != btEnum)
				ordinal_tw = &decl->tn_type;
			else {
				ordinal_tw = &tsym->s_tree->tn_enumtype;
				tsym = typesym(*ordinal_tw);
				assert(tsym->s_type == stBaseType);
				if (tsym->s_desc->td_width == width) {
					destroyfirst(&decl->tn_left);
					decl->tn_type.tw_bitfield = 0;
					tw = decl->tn_type;
				}
			}
			if (issigned(ordinal_tw->tw_base))
				bt = (enum basetype)sign(bt);
			setbasetype(ordinal_tw, bt);
		}
	}

	if (qualifier(tw) == tqArray) {
		if (tw.tw_bitfield) {
			tnerror(decl, ERROR, "illegal array %s", sym->s_name);
			ok = 0;
		}
		len = first(decl->tn_left);
		save = width;
		for (dim = 1; ; dim++) {
			if (len->tn_op == opNumber || eval(len, u_int_tw))
				width *= len->tn_val;
			else {
				switch (len->tn_type.tw_base) {
				  case btNil:
					if (dim > 1) {
						tnerror(len, ERROR,
				"%s must have a length for array dimension %d",
							sym->s_name, dim);
						ok = 0;
					}
					break;
				  case btVoid:
				  case btFloat:
				  case btDouble:
				  case btCache:
				  case btAddress:
				  case btStruct:
					tnerror(len, ERROR,
				"%s has illegal type %s for array dimension %d",
						sym->s_name,
						typename(len->tn_type), dim);
					ok = 0;
				}
				width = -width;
			}
			if (len->tn_op != opNumber
			    || (tsym->s_flags & sfCantInline)) {
				len->tn_elemsym->s_flags |= sfCantInline;
				cx->cx_flags |= cxHeapFields;
			}
			lenvec[dim-1] = len;
			tw = unqualify(tw);
			if (qualifier(tw) != tqArray)
				break;	/* XXX u_char (*foo[2])[3] */
			len = len->tn_next;
		}
		while (--dim >= 0) {
			len = lenvec[dim];
			len->tn_elemsym->s_ewidth = save;
			if (len->tn_op == opNumber)
				save *= len->tn_val;
			else
				save = -save;
		}
	}

	*rwidth = width;
	return ok;
}

static char *
scopepathname(Symbol *sym)
{
	Scope *s;
	char *bp, *ap;
	char buf[1024];

	bp = strcpy(buf + sizeof buf - (sym->s_namlen + 1), sym->s_name);
	for (s = sym->s_scope; s; s = s->s_outer) {
		sym = s->s_sym;
		if (sym == 0)
			break;
		ap = bp - sym->s_namlen;
		if (ap < buf)
			break;
		bp = strncpy(ap, sym->s_name, sym->s_namlen);
	}
	return strdup(bp);
}

/*
 * Generate a string encoding of declaration node tn's fully-qualified type.
 */
static char *
typesignature(TreeNode *tn)
{
	typeword_t tw;
	int cc, code;
	char buf[64];
	TreeNode *len;
	enum typequal tq;

	assert(tn->tn_op == opDeclaration);
	tw = tn->tn_type;
	cc = nsprintf(buf, sizeof buf, "_%s", typesym(tw)->s_name);
	len = tn->tn_left;
	if (tw.tw_bitfield) {
		assert(len->tn_op == opNumber);
		cc += nsprintf(&buf[cc], sizeof buf - cc, "F%u",
			       (unsigned)len->tn_val);
	} else {
		len = first(len);
		while ((tq = (enum typequal)qualifier(tw)) != tqNil) {
			switch (tq) {
			  case tqPointer:
				code = 'P';
				break;
			  case tqArray:
				code = 'A';
				break;
			  case tqVarArray:
				code = 'V';
			}
			buf[cc++] = code;
			if (len->tn_op == opNumber) {
				cc += nsprintf(&buf[cc], sizeof buf - cc, "%u",
					       (unsigned)len->tn_val);
			}
			tw = unqualify(tw);
			if (tq == tqArray)
				len = len->tn_next;
		}
	}
	return strndup(buf, cc);
}

static int
checkoverload(Scope *s)
{
	int ok;
	Symbol *sym;
	TreeNode *tn, **tnp, *unlinked, *next;
	typeword_t tw;

	ok = 1;
	for (sym = s->s_fields.sl_head; sym; sym = sym->s_next) {
		if (!(sym->s_flags & sfOverloaded))
			continue;
		unlinked = 0;
		for (tn = sym->s_tree; tn; tn = tn->tn_overload) {
			tnp = &tn->tn_overload;
			next = *tnp;
			if (next == 0)
				next = sym->s_tree;
			if (tn->tn_width != next->tn_width)
				sym->s_flags |= sfVariantSize;
			tw = tn->tn_type;
			if (tw.tw_word != next->tn_type.tw_word
			    && (qualifier(tw) == tqArray
				|| tw.tw_base == btCache
				|| tw.tw_base == btStruct)) {
				tnerror(tn, ERROR,
					"illegal overloaded %s %s",
					typename(tw), sym->s_name);
				ok = 0;
			}
			while ((next = *tnp) != 0) {
				if (!strcmp(tn->tn_suffix, next->tn_suffix)) {
					*tnp = next->tn_overload;
					next->tn_overload = unlinked;
					unlinked = next;
					continue;
				}
				tnp = &next->tn_overload;
			}
		}
		tn = sym->s_tree;
		if (tn->tn_overload == 0) {
			sym->s_flags &= ~sfOverloaded;
			delete(tn->tn_suffix);
			tn->tn_suffix = 0;
			for (tn = unlinked; tn; tn = next) {
				delete(tn->tn_suffix);
				tn->tn_suffix = 0;
				next = tn->tn_overload;
				tn->tn_overload = 0;
			}
		}
	}
	return ok;
}

static int
baseprotocol(TreeNode *pname, TreeNode *derived)
{
	TreeNode *guard, *target, *base, *prot;
	Scope *inner;
	int found, count;
	static int depth = 1;

	guard = pname->tn_guard;
	target = pname->tn_sym->s_tree;
	inner = pname->tn_sym->s_scope;
	found = 0;
	for (base = first(derived->tn_kid3->tn_right); base;
	     base = base->tn_next) {
		prot = base->tn_sym->s_tree;
		if (prot == target) {
			if (guard == 0)
				guard = scoperef(opStackRef, inner, depth);
			found++;
			continue;
		}
		pname->tn_guard = 0;
		depth++;
		count = baseprotocol(pname, prot);
		--depth;
		if (count == 0)
			continue;
		if (guard == 0)
			guard = pname->tn_guard;
		else {
			guard =
			    ternary(opCond,
				    binary(opEQ,
					   binary(opArrow,
						  scoperef(opStackRef, inner,
							   depth),
						  treenode(opProtoId, 0)),
					   holdnode(base)),
				    pname->tn_guard, guard);
		}
		found += count;
	}
	pname->tn_guard = guard;
	return found;
}

static Symbol *
importscope(TreeNode *tn, Symbol *sym, Context *cx)
{
	BasicBlock *bb;
	Scope *s, *t;
	TreeNode *guard;
	int depth;

	tn->tn_sym = sym = importname(sym, sfTemporary, 0, cx);
	bb = cx->cx_block;
	if (memberbit(bb->bb_gen, sym->s_fid))
		return sym;
	s = cx->cx_scope;
	switch (sym->s_type) {
	  case stProtocol:
		if (sym->s_tree == cx->cx_proto)
			guard = scoperef(opStackRef, s, 0);
		else {
			if (!baseprotocol(tn, cx->cx_proto)) {
				tnerror(tn, ERROR,
					"%s is not a base protocol of %s",
					sym->s_name,
					cx->cx_proto->tn_kid3->tn_sym->s_name);
				return 0;
			}
			guard = tn->tn_guard;
		}
		break;
	  case stStruct:
		t = &SN(sym->s_tree)->sn_scope;
		if (s == t)
			guard = 0;
		else {
			depth = 1;
			while ((s = s->s_outer) != t)
			guard = scoperef(opSlinkRef, cx->cx_scope, depth);
		}
	}
	if (guard)
		tn->tn_guard = binary(opAssign, holdnode(tn), guard);
	(void) insertbit(bb->bb_gen, sym->s_fid);
	return sym;
}

/*
 * Registered function call handlers and their subroutines.
 */
static int
checkcacheactual(Symbol *sym, TreeNode *args, int nargs)
{
	TreeNode *cname, *cache, *key;

	cname = args->tn_left;
	if (cname->tn_op != opMember
	    || (cname = cname->tn_right)->tn_op != opName
	    || cname->tn_type.tw_base != btCache) {
		tnerror(cname, ERROR,
			"1st %s argument type %s does not match Cache",
			sym->s_name, typename(cname->tn_type));
		return 0;
	}
	if (args->tn_arity != nargs) {
		tnerror(args, ERROR, "%d arguments expected for %s call",
			nargs, sym->s_name);
		return 0;
	}
	cache = cname->tn_sym->s_tree;
	if (cache->tn_left == 0) {
		key = first(args)->tn_next;
		cache->tn_left = holdnode(key);
	}
	return 1;
}

static TreeNode *
voidpointer()
{
	static TreeNode *voidp;

	if (voidp == 0) {
		voidp = declaration(0, 0, stNil);
		voidp->tn_type = qualify(voidp->tn_type, tqPointer);
		setbasetype(&voidp->tn_type, btVoid);
	}
	return holdnode(voidp);
}

static int
c_enter_handler(TreeNode *tn)
{
	TreeNode *args, *value, *cast;
	typeword_t tw;

	args = tn->tn_kid;
	if (!checkcacheactual(tn->tn_sym, args, 3))
		return 0;
	value = args->tn_right;
	tw = value->tn_type;
	if (ordinaltype(tw)) {
		/*
		 * XXX assumes sizeof(u_long) == sizeof(void *)
		 */
		value = copytree(value);
		cast = args->tn_right;
		cast->tn_op = opCast;
		cast->tn_arity = 2;
		cast->tn_left = voidpointer();
		cast->tn_right = value;
	} else if (tw.tw_qual == tqArray && sign(tw.tw_base) == btChar) {
		/* XXX xdr_wrap_string */
	} else {
		/* XXX generate xdr filter */
	}
	return 1;
}

static int
c_match_handler(TreeNode *tn)
{
	if (!checkcacheactual(tn->tn_sym, tn->tn_kid, 2))
		return 0;
	/* XXX */
	return 1;
}
