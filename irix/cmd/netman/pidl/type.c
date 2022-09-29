/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL type descriptors and operations.
 */
#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "macros.h"
#include "pidl.h"
#include "scope.h"
#include "treenode.h"
#include "type.h"

#define	TSMAX	(1 << TSBITS)

struct symbol	*typesyms[TSMAX];
static unsigned	tsymndx;

#define	TDI(name, type, size) \
	{ name, constrlen(name), type, size * BitsPerByte }

static struct typedesc basetypes[] = {
	TDI("<unknown>",	btNil,		0),
	TDI("void",		btVoid,		0),
	TDI("char",		btChar,		BytesPerChar),
	TDI("u_char",		btUChar,	BytesPerChar),
	TDI("short",		btShort,	BytesPerShort),
	TDI("u_short",		btUShort,	BytesPerShort),
	TDI("int",		btInt,		BytesPerInt),
	TDI("u_int",		btUInt,		BytesPerInt),
	TDI("signed",		btSigned,	BytesPerInt),
	TDI("unsigned",		btUnsigned,	BytesPerInt),
	TDI("long",		btLong,		BytesPerLong),
	TDI("u_long",		btULong,	BytesPerLong),
	TDI("float",		btFloat,	BytesPerFloat),
	TDI("double",		btDouble,	BytesPerDouble),
	TDI("cache",		btCache,	0),
	TDI("address",		btAddress,	BytesPerAddress),
	TDI("bool",		btBool,		BytesPerInt),
	TDI("opaque",		btOpaque,	1),
	TDI("string",		btString,	BytesPerChar),
};

void
installtypes(Scope *s)
{
	int index, count;
	struct typedesc *td;
	Symbol *sym;
	enum basetype bt;

	index = 0;
	count = lengthof(basetypes);
	for (td = basetypes; --count >= 0; td++) {
		sym = enter(s, td->td_name, td->td_namlen,
			    hashname(td->td_name, td->td_namlen),
			    stBaseType, sfTypeName);
		sym->s_desc = td;
		bt = (enum basetype)td->td_code;
		if (bt > index)
			index = bt;
		typesyms[bt] = sym;
		sym->s_tsymndx = bt;
	}
	tsymndx = index + 1;
}

void
definetype(Symbol *sym)
{
	if (tsymndx >= TSMAX) {
		yyerror("sorry, too many types");
		terminate(0);
	}
	typesyms[tsymndx] = sym;
	sym->s_tsymndx = tsymndx++;
	sym->s_trefcnt = 0;
}

static int
basetype(Symbol **sp, int nsym, typeword_t *tw)
{
	enum basetype prevbt, bt;	/* previous and current base type */
	int signedcount;		/* count of "signed" occurences */
	Symbol *sym;			/* current symbol in sp vector */

	tw->tw_word = 0;
	prevbt = btNil;
	signedcount = 0;

	while (--nsym >= 0) {
		sym = *sp++;
		if (sym->s_type != stBaseType) {
			if (prevbt != btNil || nsym > 0)
				return 0;
			switch (sym->s_type) {
			  case stEnum:
				bt = btEnum;
				break;
			  case stStruct:
				bt = btStruct;
				break;
			  case stTypeDef:
				bt = btTypeDef;
			}
			tw->tw_symndx = sym->s_tsymndx;
			break;
		}

		bt = (enum basetype)sym->s_desc->td_code;
		switch (bt) {
		  case btChar:
		  case btShort:
		  case btInt:
			switch (prevbt) {
			  case btNil:
			  case btSigned:
				break;
			  case btUnsigned:
				bt = (enum basetype)unsign(bt);
				break;
			  default:
				return 0;
			}
			break;

		  case btLong:
			switch (prevbt) {
			  case btNil:
			  case btSigned:
				break;
			  case btUnsigned:
				bt = btULong;
				break;
			  case btFloat:
				bt = btDouble;
				break;
			  default:
				return 0;
			}
			break;

		  case btUChar:
		  case btUShort:
		  case btUInt:
		  case btULong:
			switch (prevbt) {
			  case btNil:
				break;
			  default:
				return 0;
			}
			break;

		  case btSigned:
			signedcount++;
			if (prevbt == btNil)
				break;
			if (signedcount > 1 || !signable(prevbt))
				return 0;
			bt = prevbt;
			break;

		  case btUnsigned:
			if (prevbt == btNil)
				break;
			if (signedcount > 0 || !unsignable(prevbt))
				return 0;
			bt = (enum basetype)unsign(prevbt);
			break;

		  case btFloat:
			switch (prevbt) {
			  case btNil:
				break;
			  case btLong:
				bt = btDouble;
				break;
			  default:
				return 0;
			}
			break;

		  case btDouble:
			switch (prevbt) {
			  case btNil:
				break;
			  default:
				return 0;
			}
			break;

		  default:
			if (prevbt != btNil || nsym > 0)
				return 0;
		}
		prevbt = bt;
	}

	if (tw->tw_symndx == 0)
		tw->tw_symndx = bt;
	tw->tw_base = bt;
	return 1;
}

typeword_t
typeword(Symbol *sym1, Symbol *sym2, Symbol *sym3, Symbol *sym4,
	 enum datarep rep)
{
	Symbol **sp, *symvec[4];
	typeword_t tw;

	sp = symvec;
	*sp++ = sym1;
	if (sym2) {
		*sp++ = sym2;
		if (sym3) {
			*sp++ = sym3;
			if (sym4)
				*sp++ = sym4;
		}
	}

	if (!basetype(symvec, sp - symvec, &tw)) {
		yyerror("illegal type combination");
		tw.tw_word = 0;
	}
	tw.tw_rep = rep;
	return tw;
}

typeword_t
tagtype(Symbol *sym, enum datarep dr, enum basetype bt)
{
	typeword_t tw;

	tw = typeword(sym, 0, 0, 0, dr);
	if (tw.tw_base != bt)
		yyerror("redeclaration of %s", typesym(tw)->s_name);
	return tw;
}

typeword_t
qualify(typeword_t tw, enum typequal tq)
{
	unsigned int shift, qual;

	shift = 0;
	for (qual = tw.tw_qual; qual; qual >>= TQSHIFT) {
		shift += TQSHIFT;
		if (shift == TQBITS) {
			yyerror("too many type qualifiers for %s",
				typesym(tw)->s_name);
			return tw;
		}
	}
	tw.tw_qual |= (tq << shift);
	return tw;
}

typeword_t
unqualify(typeword_t tw)
{
	assert(tw.tw_qual);
	tw.tw_qual = (tw.tw_qual >> TQSHIFT);
	return tw;
}

int
dimensions(typeword_t tw)
{
	int dim;
	unsigned int qual;

	dim = 0;
	for (qual = tw.tw_qual; qual; qual >>= TQSHIFT)
		dim++;
	return dim;
}

TreeNode *
declaretype(typeword_t tw, TreeNode *decl)
{
	Symbol *tsym, *dsym;
	TreeNode *dtn, *len;
	typeword_t dtw;
	int dim, ndim, cc;
	unsigned int qual;
	enum typequal tq;
	char *name, buf[64];
	hash_t hash;

	tsym = typesym(tw);
	tsym->s_trefcnt++;
	decl->tn_decltype = tw;

	while (tw.tw_base == btTypeDef) {
		dtn = tsym->s_tree;
		dtw = dtn->tn_type;
		tw.tw_symndx = dtw.tw_symndx;
		tw.tw_base = dtw.tw_base;
		if (dtw.tw_bitfield)
			bitfield(decl, dtn->tn_left);
		dim = dimensions(dtw);
		if (dim > 0) {
			len = first(dtn->tn_left);
			for (qual = dtw.tw_qual; --dim >= 0; qual >>= TQSHIFT) {
				tq = (enum typequal)(qual & TQMASK);
				if (tq == tqPointer)
					continue;
				assert(len);
				array(decl, tq, copytree(len));
				len = len->tn_next;
			}
		}
		if (dtw.tw_rep != drNil) {
			if (tw.tw_rep != drNil && tw.tw_rep != dtw.tw_rep)
				yyerror("incompatible data representations");
			tw.tw_rep = dtw.tw_rep;
		}
	}

	dtw = decl->tn_type;
	switch (tw.tw_base) {
	  case btVoid:
	  case btCache:
		if (qualifier(dtw) == tqArray) {
			yyerror("illegal array of %s", tsym->s_name);
			break;
		}
		/* FALL THROUGH */
	  case btFloat:
	  case btDouble:
	  case btOpaque:
	  case btString:
	  case btStruct:
		if ((tw.tw_base == btOpaque || tw.tw_base == btString)
		    && qualifier(dtw) != tqArray
		    && qualifier(dtw) != tqVarArray) {
			yyerror("illegal %s scalar", tsym->s_name);
			break;
		}
		if (dtw.tw_bitfield)
			yyerror("illegal %s bitfield", tsym->s_name);
	}

	/*
	 * Minimize bitfield loads & stores by copying tw_qual and tw_bitfield
	 * from dtw to tw, then storing all 32 bits of tw into decl->tn_type.
	 */
	tw.tw_qual = dtw.tw_qual;
	tw.tw_bitfield = dtw.tw_bitfield;
	decl->tn_type = tw;

	ndim = dimensions(tw);
	if (ndim > 0) {
		dsym = decl->tn_sym;
		name = dsym ? dsym->s_name : "anon";
		len = first(decl->tn_left);
		dim = 0;
		for (qual = tw.tw_qual; ++dim <= ndim; qual >>= TQSHIFT) {
			if ((qual & TQMASK) == tqPointer)
				continue;
			cc = nsprintf(buf, sizeof buf, ".%s%d", name, dim);
			hash = hashname(buf, cc);
			dsym = lookup(innermost, buf, cc, hash);
			tw = unqualify(tw);
			if (dsym == 0) {
				dsym = enter(innermost, buf, cc, hash,
					     stElement, sfSaveName|sfAnonymous);
				/*
				 * Insert rather than append, so that the code
				 * generator can emit array element descriptors
				 * in list order without making forward refs.
				 */
				INSERTSYM(dsym, &innermost->s_elements);
				dsym->s_tree = decl;
				dsym->s_etype = tw;
				dsym->s_dimlen = len->tn_next;
			}
			len->tn_elemsym = dsym;
			len = len->tn_next;
		}
	}

	return decl;
}

char *
typename(typeword_t tw)
{
	Symbol *tsym;
	int cc;
	unsigned int qual;
	char *what;
	static char buf[128];

	tsym = typesym(tw);
	cc = 0;
	for (qual = tw.tw_qual; qual; qual >>= TQSHIFT) {
		switch (qual & TQMASK) {
		  case tqPointer:
			what = "pointer to ";
			break;
		  case tqArray:
			what = "array of ";
			break;
		  case tqVarArray:
			what = "variable array of ";
		}
		cc += nsprintf(&buf[cc], sizeof buf - cc, what);
	}
	(void) nsprintf(&buf[cc], sizeof buf - cc, tsym->s_name);
	return buf;
}
