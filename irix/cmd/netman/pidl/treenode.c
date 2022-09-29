/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL abstract syntax tree node operations.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <values.h>
#include "debug.h"
#include "heap.h"
#include "macros.h"
#include "pidl.h"
#include "scope.h"
#include "treenode.h"
#include "type.h"

extern void	vferror(enum severity, char *, int, char *, va_list);

char *opnames[] = {
	"nil",
	"list",
	"import", "export", "pragma",
	"protocol", "qualify", "base", "nest",
	"declaration",
	"if-else",
	"switch", "case", "default",
	"struct",
	"macro",
	"enum", "bitset", "enumerator",
	"block",
	",",
	"=",
	"?:",
	"||",
	"&&",
	"|",
	"^",
	"&",
	"==", "!=",
	"<", "<=", ">", ">=",
	"<<", ">>",
	"+", "-",
	"*", "/", "%",
	"*", "&",
	"!", "~", "-", "sizeof", "cast",
	"index", ".", "->", "call",
	"name", "number", "address", "string",
	"ds", "ps",
	"stackref", "protoid", "slinkref",
	"bitseek", "bitrewind",
};

static void
initnode(TreeNode *tn, enum operator op, int arity)
{
	tn->tn_op = op;
	tn->tn_refcnt = 1;
	tn->tn_flags = 0;
	tn->tn_type.tw_word = 0;
	tn->tn_arity = arity;
	tn->tn_lineno = yylineno;
	tn->tn_module = yyfilename;
	tn->tn_next = 0;
	tn->tn_width = 0;
	tn->tn_bitoff = 0;
	tn->tn_guard = 0;
}

TreeNode *
treenode(enum operator op, int arity)
{
	TreeNode *tn;

	tn = new(TreeNode);
	initnode(tn, op, arity);
	return tn;
}

ScopeNode *
scopenode(enum operator op, int arity, Symbol *sym)
{
	ScopeNode *sn;

	sn = new(ScopeNode);
	initnode(&sn->sn_node, op, arity);
	initscope(&sn->sn_scope, sym->s_name, sym, innermost);
	return sn;
}

void
tnerror(TreeNode *tn, enum severity sev, char *format, ...)
{
	va_list ap;

	/* CONSTCOND */
	va_start(ap, format);
	vferror(sev, tn->tn_module, tn->tn_lineno, format, ap);
	va_end(ap);
}

/*
 * List:
 *	left	->	first element in list
 *	right	->	last element in list
 */
TreeNode *
list(TreeNode *head)
{
	TreeNode *tn;

	if (head && head->tn_op == opList)
		return head;
	tn = treenode(opList, head != 0);
	tn->tn_left = tn->tn_right = head;
	return tn;
}

TreeNode *
first(TreeNode *tn)
{
	if (tn == 0)
		return 0;
	if (tn->tn_op != opList)
		return tn;
	return tn->tn_left;
}

void
destroyfirst(TreeNode **tnp)
{
	TreeNode *tn, *head;

	tn = *tnp;
	if (tn == 0)
		return;
	if (tn->tn_op != opList) {
		destroytree(tn);
		*tnp = 0;
		return;
	}
	head = tn->tn_left;
	tn->tn_left = head->tn_next;
	destroytree(head);
}

TreeNode *
append(TreeNode *tn, TreeNode *next)
{
	if (tn == 0)
		return next;
	if (next == 0)
		return tn;
	if (tn->tn_op != opList)
		tn = list(tn);
	if (next->tn_op == opList) {
		if (tn->tn_left == 0) {
			tn->tn_left = next->tn_left;
			tn->tn_right = next->tn_right;
		} else {
			tn->tn_right->tn_next = next->tn_left;
			tn->tn_right = next->tn_right;
		}
		tn->tn_arity += next->tn_arity;
		delete(next);
	} else {
		if (tn->tn_left == 0)
			tn->tn_left = tn->tn_right = next;
		else {
			tn->tn_right->tn_next = next;
			tn->tn_right = next;
		}
		tn->tn_arity++;
	}
	return tn;
}

/*
 * Protocol:
 *	kid3	->	Declaration
 *	kid2	->	protocol body (header)
 *	kid1	->	optional footer
 * Declaration:
 *	sym	:	protocol symbol
 *	left	->	formal arguments
 *	right	->	base protocols
 */
TreeNode *
openprotocol(enum datarep rep, TreeNode *decl, TreeNode *formals,
	     TreeNode *bases)
{
	ScopeNode *sn;

	sn = scopenode(opProtocol, 3, decl->tn_sym);
	decl->tn_sym->s_tree = &sn->sn_node;
	decl->tn_left = formals;
	decl->tn_right = bases;
	sn->sn_node.tn_type.tw_rep = rep;
	sn->sn_node.tn_kid3 = decl;
	innermost = &sn->sn_scope;
	return &sn->sn_node;
}

static void
assignfids(ScopeNode *sn)
{
	Symbol *sym;

	sn->sn_scope.s_sym->s_fid = lastfid++;
	for (sym = sn->sn_scope.s_fields.sl_head; sym; sym = sym->s_next)
		sym->s_fid = lastfid++;
	for (sym = sn->sn_scope.s_elements.sl_head; sym; sym = sym->s_next)
		sym->s_eid = lastfid++;
	for (sym = sn->sn_scope.s_structs.sl_head; sym; sym = sym->s_next)
		assignfids(SN(sym->s_tree));
}

TreeNode *
closeprotocol(TreeNode *tn, TreeNode *body, TreeNode *foot)
{
	innermost = SN(tn)->sn_scope.s_outer;
	assignfids(SN(tn));
	tn->tn_kid2 = body;
	tn->tn_kid1 = foot;
	return tn;
}

/*
 * Base:
 *	sym	:	protocol symbol
 *	kid	->	typecode expression
 *		->	Qualify
 * Qualify:
 *	left	->	typecode expression
 *	right	->	qualifier expression
 */
TreeNode *
base(Symbol *sym, TreeNode *type)
{
	if (sym->s_type != stProtocol)
		yyerror("%s is not a protocol", sym->s_name);
	return unary(opBase, sym, type);
}

static Symbol *
declare(Symbol *sym, enum symtype stype, int sflags)
{
	Symbol *nsym;

	nsym = lookup(innermost, sym->s_name, sym->s_namlen, sym->s_hash);
	if (nsym)
		yyerror("redeclaration of %s", sym->s_name);
	else {
		nsym = install(innermost, sym, stype, sflags);
		if (sflags & sfTypeName)
			definetype(nsym);
	}
	return nsym;
}

/*
 * Declaration:
 *	sym	:	declared symbol
 *	left	->	array dimensions or bitfield width
 *	right	->	initializer
 */
TreeNode *
declaration(Symbol *sym, char *title, enum symtype stype)
{
	TreeNode *tn;
	Symbol *osym;
	unsigned short sflags;
	unsigned int anongen;
	int cc;
	char buf[64];
	hash_t hash;

	tn = treenode(opDeclaration, 2);
	switch (stype) {
	  case stProtocol:
		sym = declare(sym, stype, 0);
		APPENDSYM(sym, &innermost->s_protocols);
		sym->s_tree = 0;
		sym->s_title = title;
		break;

	  case stField:
		if (sym) {
			osym = lookup(innermost, sym->s_name, sym->s_namlen,
				      sym->s_hash);
			sflags = 0;
		} else {
			anongen = 0;
			do {
				cc = nsprintf(buf, sizeof buf, "anon%u",
					      anongen++);
				hash = hashname(buf, cc);
				osym = lookup(innermost, buf, cc, hash);
			} while (osym);
			sym = enter(&strings, buf, cc, hash, stNil, sfSaveName);
			sflags = sfAnonymous;
		}
		if (osym == 0) {
			sym = install(innermost, sym, stype, sflags);
			APPENDSYM(sym, &innermost->s_fields);
			sym->s_tree = tn;
			sym->s_title = title;
			sym->s_index = innermost->s_numfields++;
			sym->s_level = 0;
			tn->tn_overload = 0;
		} else if (osym->s_type != stype) {
			yyerror("redeclaration of %s", osym->s_name);
			sym = osym;
		} else {
			assert(osym->s_tree);
			tn->tn_overload = osym->s_tree;
			osym->s_tree = tn;
			osym->s_flags |= sfOverloaded;
			if (title && *title != '\0') {
				if (osym->s_title == 0)
					osym->s_title = title;
				else if (strcmp(osym->s_title, title)) {
					yyerror("more than one title for %s",
						osym->s_name);
				}
			}
			sym = osym;
		}
		break;

	  case stTypeDef:
		sym = declare(sym, stype, sfTypeName);
		APPENDSYM(sym, &innermost->s_typedefs);
		sym->s_tree = tn;
		tn->tn_type.tw_symndx = sym->s_tsymndx;
	}
	tn->tn_suffix = 0;
	tn->tn_sym = sym;
	tn->tn_left = tn->tn_right = 0;
	return tn;
}

void
array(TreeNode *tn, enum typequal atq, TreeNode *len)
{
	assert(len);
	tn->tn_type = qualify(tn->tn_type, atq);
	if (tn->tn_left == 0)
		tn->tn_left = list(len);
	else
		append(tn->tn_left, len);
}

void
bitfield(TreeNode *tn, TreeNode *len)
{
	assert(len);
	tn->tn_type.tw_bitfield = 1;
	tn->tn_left = len;
}

/*
 * Enumerator:
 *	sym	:	enumerator symbol
 *	kid	->	number or address value
 */
TreeNode *
constdefs(typeword_t type, TreeNode *enumlist)
{
	TreeNode *def, *kid;
	Symbol *vsym;
	static Address azero;

	if (type.tw_base == btNil)
		setbasetype(&type, btLong);
	else if (type.tw_base != btAddress && !isintegral(type.tw_base)) {
		yyerror("illegal constant type");
		return enumlist;
	}
	for (def = enumlist->tn_left; def; def = def->tn_next) {
		assert(def->tn_op == opEnumerator);
		kid = def->tn_kid;
		if (kid && !eval(kid, type)) {
			yyerror("constant expression required");
			destroytree(kid);
			kid = 0;
		}
		if (kid == 0) {
			kid = (type.tw_base == btAddress) ? address(azero)
			      : number(0);
			def->tn_kid = kid;
		}
		vsym = def->tn_sym = declare(def->tn_sym,
					     (type.tw_base == btAddress) ?
					     stAddress : stNumber, 0);
		APPENDSYM(vsym, &innermost->s_consts);
		if (vsym->s_type == stNumber)
			vsym->s_val = kid->tn_val;
		else {
			if (kid->tn_op == opNumber) {
				kid->tn_addr.a_low = kid->tn_val;
				kid->tn_addr.a_high = 0;
				kid->tn_op = opAddress;
			}
			vsym->s_addr = kid->tn_addr;
		}
	}
	return enumlist;
}

/*
 * Enum:
 *	sym	->	enum type symbol
 *	kid	->	list of enumerators
 */
TreeNode *
enumdef(enum operator op, enum datarep rep, Symbol *sym, TreeNode *enumlist)
{
	typeword_t tw;
	long nextval, min, max, val;
	int namlen, maxnamlen;
	TreeNode *tn, *kid;
	Symbol *vsym;
	unsigned int width;
	enum basetype bt;

	tw.tw_word = 0;
	setbasetype(&tw, btLong);
	tw.tw_rep = rep;
	nextval = 0;
	min = MAXLONG;
	max = ~MAXLONG;
	maxnamlen = 0;

	for (tn = enumlist->tn_left; tn; tn = tn->tn_next) {
		kid = tn->tn_kid;
		if (kid && !eval(kid, tw)) {
			yyerror("constant expression required");
			destroytree(kid);
			kid = 0;
		}
		if (kid == 0)
			kid = tn->tn_kid = number(nextval);
		vsym = tn->tn_sym = declare(tn->tn_sym, stNumber, 0);
		val = kid->tn_val;
		vsym->s_val = val;
		nextval = val + 1;
		if (val < min)
			min = val;
		if (val > max)
			max = val;
		namlen = tn->tn_sym->s_namlen;
		if (namlen > maxnamlen)
			maxnamlen = namlen;
	}

	if (op == opBitSet) {
		nextval = 0;
		for (tn = enumlist->tn_left; tn; tn = tn->tn_next) {
			kid = tn->tn_kid;
			val = kid->tn_val;
			if (nextval & val)
				yyerror("duplicate enumerator in bitset");
			nextval |= val;
		}
	}

	LOG2CEIL(max - min + 1, width);
	if (width <= BitsPerChar)
		bt = btUChar;
	else if (width <= BitsPerShort)
		bt = btUShort;
	else
		bt = btULong;
	/* XXX should look for negative lexeme */
	if (min < 0)
		bt = (enum basetype)sign(bt);

	tn = treenode(op, 1);
	sym = declare(sym, stEnum, sfTypeName);
	APPENDSYM(sym, &innermost->s_enums);
	sym->s_tree = tn;
	tn->tn_type.tw_symndx = sym->s_tsymndx;
	tn->tn_type.tw_base = btEnum;
	tn->tn_type.tw_rep = rep;
	tn->tn_enumtype.tw_word = 0;
	setbasetype(&tn->tn_enumtype, bt);
	tn->tn_maxnamlen = maxnamlen;
	tn->tn_sym = sym;
	tn->tn_kid = enumlist;
	return tn;
}

/*
 * Struct:
 *	sym	:	struct tag symbol
 *	kid	->	struct members
 */
TreeNode *
openstruct(enum datarep rep, Symbol *sym)
{
	ScopeNode *sn;

	sym = declare(sym, stStruct, sfTypeName);
	APPENDSYM(sym, &innermost->s_structs);
	sn = scopenode(opStruct, 1, sym);
	sym->s_tree = &sn->sn_node;
	sn->sn_node.tn_type.tw_symndx = sym->s_tsymndx;
	sn->sn_node.tn_type.tw_base = btStruct;
	sn->sn_node.tn_type.tw_rep = rep;
	sn->sn_node.tn_sym = sym;
	innermost = &sn->sn_scope;
	return &sn->sn_node;
}

TreeNode *
closestruct(TreeNode *tn, TreeNode *members)
{
	innermost = SN(tn)->sn_scope.s_outer;
	if (innermost->s_outer == 0)
		assignfids(SN(tn));
	tn->tn_kid = members;
	return tn;
}

TreeNode *
block(Symbol *tsym, TreeNode *args)
{
	TreeNode *block;

	switch (tsym->s_type) {
	  case stNil:
		block = openstruct(drNil, tsym);
		block->tn_sym->s_fid = lastfid++;
		lastfid += args->tn_arity;
		(void) closestruct(block, 0);
		block->tn_sym->s_flags |= sfTemporary|sfDirect;
		break;
	  case stStruct:
		block = holdnode(tsym->s_tree);
		break;
	  default:
		yyerror("%s is not a struct", tsym->s_name);
		block = 0;
	}
	return binary(opBlock, block, args);
}

typeword_t niltw;

TreeNode *
ternary(enum operator op, TreeNode *kid3, TreeNode *kid2, TreeNode *kid1)
{
	TreeNode *tn;

	tn = treenode(op, 3);
	if (kid3)
		tn->tn_lineno = kid3->tn_lineno;
	else if (kid2)
		tn->tn_lineno = kid2->tn_lineno;
	else if (kid1)
		tn->tn_lineno = kid1->tn_lineno;
	tn->tn_kid3 = kid3;
	tn->tn_kid2 = kid2;
	tn->tn_kid1 = kid1;
	(void) eval(tn, niltw);
	return tn;
}

TreeNode *
binary(enum operator op, TreeNode *left, TreeNode *right)
{
	TreeNode *tn;

	tn = treenode(op, 2);
	if (left)
		tn->tn_lineno = left->tn_lineno;
	else if (right)
		tn->tn_lineno = right->tn_lineno;
	tn->tn_left = left;
	tn->tn_right = right;
	(void) eval(tn, niltw);
	return tn;
}

TreeNode *
unary(enum operator op, Symbol *sym, TreeNode *kid)
{
	TreeNode *tn;

	tn = treenode(op, 1);
	if (kid)
		tn->tn_lineno = kid->tn_lineno;
	tn->tn_sym = sym;
	tn->tn_kid = kid;
	(void) eval(tn, niltw);
	return tn;
}

TreeNode *
macro(Symbol *sym, Symbol *def)
{
	TreeNode *tn;
	Scope *s;

	tn = treenode(opMacro, 0);
	s = innermost;
	assert(s->s_sym);
	if (isnickname(s, def)) {
		sym = declare(sym, stNickname, 0);
		APPENDSYM(sym, &s->s_nicknames);
	} else {
		sym = declare(sym, stMacro, 0);
		APPENDSYM(sym, &s->s_macros);
	}
	sym->s_tree = tn;
	tn->tn_sym = sym;
	tn->tn_def = def;
	return tn;
}

TreeNode *
pragma(struct pragma *p)
{
	TreeNode *tn;

	tn = treenode(opPragma, 0);
	tn->tn_pragma = p;
	return tn;
}

TreeNode *
name(Symbol *sym)
{
	TreeNode *tn;

	tn = treenode(opName, 0);
	tn->tn_sym = sym;
	return tn;
}

TreeNode *
number(long val)
{
	TreeNode *tn;

	tn = treenode(opNumber, 0);
	setbasetype(&tn->tn_type, btLong);
	tn->tn_val = val;
	return tn;
}

TreeNode *
address(Address addr)
{
	TreeNode *tn;

	tn = treenode(opAddress, 0);
	setbasetype(&tn->tn_type, btAddress);
	tn->tn_addr = addr;
	return tn;
}

TreeNode *
string(Symbol *sym)
{
	TreeNode *tn;

	tn = treenode(opString, 0);
	setbasetype(&tn->tn_type, btChar);
	tn->tn_type.tw_qual = tqArray;
	tn->tn_sym = sym;
	return tn;
}

TreeNode *
scoperef(enum operator op, Scope *inner, int depth)
{
	TreeNode *tn;

	tn = treenode(op, 0);
	tn->tn_type = inner->s_sym->s_tree->tn_type;
	tn->tn_inner = inner;
	tn->tn_depth = depth;
	return tn;
}

TreeNode *
bitseek(int seek)
{
	TreeNode *tn;

	tn = treenode(opBitSeek, 0);
	tn->tn_val = seek;
	return tn;
}

void
walktree(TreeNode *tn, int (*fun)(TreeNode *, void *), void *arg)
{
	if (tn == 0 || !(*fun)(tn, arg))
		return;
	switch (tn->tn_op) {
	  case opList:
		for (tn = tn->tn_left; tn; tn = tn->tn_next)
			walktree(tn, fun, arg);
		break;
	  default:
		switch (tn->tn_arity) {
		  case 3:
			walktree(tn->tn_kid3, fun, arg);
			/* FALL THROUGH */
		  case 2:
			walktree(tn->tn_kid2, fun, arg);
			/* FALL THROUGH */
		  case 1:
			walktree(tn->tn_kid1, fun, arg);
		}
	}
}

TreeNode *
copytree(TreeNode *tn)
{
	TreeNode *copy, *next;

	if (tn == 0)
		return 0;
	copy = 0;
	switch (tn->tn_op) {
	  case opList:
		copy = treenode(tn->tn_op, 0);
		copy->tn_left = 0;
		for (next = tn->tn_left; next; next = next->tn_next)
			append(copy, copytree(next));
		break;
	  case opPragma:
		copy->tn_pragma = holdpragma(tn->tn_pragma);
		break;
	  case opProtocol:
	  case opStruct:
		copy = &scopenode(tn->tn_op, tn->tn_arity, tn->tn_sym)->sn_node;
		copyscope(&SN(tn)->sn_scope, &SN(copy)->sn_scope);
		/* FALL THROUGH */
	  default:
		if (copy == 0)
			copy = treenode(tn->tn_op, tn->tn_arity);
		switch (tn->tn_arity) {
		  case 3:
			copy->tn_kid3 = copytree(tn->tn_kid3);
			/* FALL THROUGH */
		  case 2:
			copy->tn_kid2 = copytree(tn->tn_kid2);
			/* FALL THROUGH */
		  case 1:
			if (tn->tn_arity != 3)
				copy->tn_sym = tn->tn_sym;
			copy->tn_kid1 = copytree(tn->tn_kid1);
			break;
		  case 0:
			copy->tn_data = tn->tn_data;
		}
	}
	copy->tn_flags = tn->tn_flags;
	copy->tn_type = tn->tn_type;
	copy->tn_lineno = tn->tn_lineno;
	copy->tn_module = tn->tn_module;
	copy->tn_next = tn->tn_next;
	copy->tn_width = tn->tn_width;
	copy->tn_bitoff = tn->tn_bitoff;
	copy->tn_var = tn->tn_var;
	return copy;
}

TreeNode *
_destroytree(TreeNode *tn)
{
	enum operator op;
	TreeNode *next;

	if (tn == 0)
		return 0;
	if (--tn->tn_refcnt != 0)
		return tn->tn_next;
	op = tn->tn_op;
	switch (op) {
	  case opList:
		for (next = tn->tn_left; next; next = _destroytree(next))
			;
		break;
	  case opPragma:
		deletepragma(tn->tn_pragma);
		break;
	  case opProtocol:
	  case opStruct:
		freescope(&SN(tn)->sn_scope);
		/* FALL THROUGH */
	  default:
		switch (tn->tn_arity) {
		  case 3:
			destroytree(tn->tn_kid3);
			/* FALL THROUGH */
		  case 2:
			destroytree(tn->tn_kid2);
			/* FALL THROUGH */
		  case 1:
			destroytree(tn->tn_kid1);
		}
	}
	if (tn->tn_guard)
		destroytree(tn->tn_guard);
	if (op == opDeclaration && tn->tn_suffix)
		delete(tn->tn_suffix);
	next = tn->tn_next;
	delete(tn);
	return next;
}
