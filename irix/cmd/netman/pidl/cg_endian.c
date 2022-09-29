/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Big- and little-endian code generators.
 */
#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "generate.h"
#include "macros.h"
#include "pidl.h"
#include "scope.h"
#include "treenode.h"
#include "type.h"

static void	ambi_noop(CodeGenerator *, TreeNode *);
static void	ambi_sizeof(TreeNode *);
static void	ambi_offset(TreeNode *);
static void	ambi_fetch_array(CodeGenerator *, TreeNode *);
static void	ambi_fetch_number(CodeGenerator *, TreeNode *);
static void	ambi_fetch_address(CodeGenerator *, TreeNode *);
static void	ambi_fetch_bool(CodeGenerator *, TreeNode *);
static void	ambi_fetch_opaque(CodeGenerator *, TreeNode *);
static void	ambi_fetch_string(CodeGenerator *, TreeNode *);
static void	ambi_fetch_struct(CodeGenerator *, TreeNode *);

static void	ambi_decode_array(CodeGenerator *, TreeNode *);
static void	ambi_decode_number(CodeGenerator *, TreeNode *);
static void	ambi_decode_address(CodeGenerator *, TreeNode *);
static void	ambi_decode_bool(CodeGenerator *, TreeNode *);
static void	ambi_decode_opaque(CodeGenerator *, TreeNode *);
static void	ambi_decode_string(CodeGenerator *, TreeNode *);
static void	ambi_decode_struct(CodeGenerator *, TreeNode *);

CodeGenerator big_endian_cg = {
	drBigEndian, "BIG_ENDIAN", 0,
	ambi_sizeof, ambi_offset, ambi_fetch_array,
	{ ambi_noop, ambi_noop,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_noop, ambi_fetch_address,
	  ambi_fetch_bool, ambi_fetch_opaque, ambi_fetch_string,
	  ambi_fetch_number, ambi_fetch_struct },
	ambi_decode_array,
	{ ambi_noop, ambi_noop,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_noop, ambi_decode_address,
	  ambi_decode_bool, ambi_decode_opaque, ambi_decode_string,
	  ambi_decode_number, ambi_decode_struct },
};

CodeGenerator little_endian_cg = {
	drLittleEndian, "LITTLE_ENDIAN", 0,
	ambi_sizeof, ambi_offset, ambi_fetch_array,
	{ ambi_noop, ambi_noop,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_fetch_number, ambi_fetch_number,
	  ambi_noop, ambi_fetch_address,
	  ambi_fetch_bool, ambi_fetch_opaque, ambi_fetch_string,
	  ambi_fetch_number, ambi_fetch_struct },
	ambi_decode_array,
	{ ambi_noop, ambi_noop,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_decode_number, ambi_decode_number,
	  ambi_noop, ambi_decode_address,
	  ambi_decode_bool, ambi_decode_opaque, ambi_decode_string,
	  ambi_decode_number, ambi_decode_struct },
};

/* ARGSUSED */
static void
ambi_noop(CodeGenerator *cg, TreeNode *decl)
{
}

static void
ambi_sizeof(TreeNode *decl)
{
	Symbol *sym;

	sym = decl->tn_sym;
	if (sym->s_type == stElement) {
	}
}

static void
ambi_offset(TreeNode *decl)
{
}

/*
 * Ambi-endian fetch functions.
 */
static void	beginfetch(CodeGenerator *, Symbol *);
static void	endfetch(CodeGenerator *, Symbol *);
static void	allocatearray(char *, TreeNode *);

static void
ambi_fetch_array(CodeGenerator *cg, TreeNode *decl)
{
	typeword_t tw;
	TreeNode *len;
	Symbol *sym;

	tw = decl->tn_type;
	len = first(decl->tn_left);	/* XXX elem.tn_left hack */
	if (cg->cg_flags & cgfLookahead) {
		if (len->tn_elemsym->s_flags & sfCantInline) {
			allocatearray(cg->cg_member, len);
			indent(level);
		}
		fprintf(cfile, "if (!ds_bytes(ds, %s, ", cg->cg_member);
		generateCexpr(len);
		fputs("))\n", cfile);
		indent(level+1);
		if (cg->cg_flags & cgfFetch)
			fputs("goto out;\n", cfile);
		else
			fputs("return;\n", cfile);
		return;
	}
	sym = decl->tn_sym;
	beginfetch(cg, sym);
	if (tw.tw_qual == tqArray && tw.tw_base == btChar) {
		fputs("rex->ex_op = EXOP_STRING;\n", cfile);
		indent(level);
		fputs("rex->ex_str.s_ptr = (char *) ds->ds_next;\n", cfile);
		indent(level);
		fputs("rex->ex_str.s_len = ", cfile);
		if (len->tn_op != opNil)
			fputs("MIN(", cfile);
		generateCexpr(len);
		if (len->tn_op != opNil)
			fputs(", ds->ds_count)", cfile);
		fputs(";\n", cfile);
	} else {
		fputs("rex->ex_op = EXOP_FIELD;\n", cfile);
		indent(level);
		fprintf(cfile, "rex->ex_field = &%s%s_element;\n",
			sym->s_scope->s_path, len->tn_elemsym->s_name + 1);
	}
	indent(level);
	fputs("ok = 1; goto out;\n", cfile);
	endfetch(cg, sym);

	if (sym->s_type != stElement) {	/* XXX hack for one-dimensional case */
		indent(level);
		fprintf(cfile, "if (!ds_seek(ds, %d * ",
			len->tn_elemsym->s_ewidth / BitsPerByte);
		generateCexpr(len);
		fputs(", DS_RELATIVE)) goto out;\n", cfile);
	}
}

static void
ambi_fetch_number(CodeGenerator *cg, TreeNode *decl)
{
	typeword_t tw;
	Symbol *sym;
	char *result;		/* XXX */
	TreeNode *init;

	tw = decl->tn_type;
	if (tw.tw_base == btEnum)
		setbasetype(&tw, typesym(tw)->s_tree->tn_enumtype.tw_base);
	sym = decl->tn_sym;
	if (sym->s_type == stElement)
		beginfetch(cg, sym);
	result = (cg->cg_flags & cgfFetch) ? "rex->ex_val" : "val";
	init = decl->tn_right;
	if (init && !(init->tn_flags & tfExpected)) {
		if (tw.tw_bitfield)
			fprintf(cfile, "%s = ", result);
		else
			fprintf(cfile, "%s = ", cg->cg_member);
		generateCexpr(init);
		fputs(";\n", cfile);
	} else {
		fputs("if (!ds_", cfile);
		if (tw.tw_bitfield) {
			fprintf(cfile, "bits(ds, &%s, %u, DS_%s_EXTEND)",
				result, (unsigned)decl->tn_left->tn_val,
				issigned(tw.tw_base) ? "SIGN" : "ZERO");
		} else {
			fprintf(cfile, "%s(ds, &%s)",
				typesym(tw)->s_name, cg->cg_member);
		}
		fputs(")\n", cfile);
		indent(level+1);
		fprintf(cfile, "%s;\n",
			(cg->cg_flags & cgfFetch) ? "goto out" : "return");
	}
	if (!(cg->cg_flags & cgfLookahead)) {
		indent(level);
		if (sym->s_type != stElement)
			beginfetch(cg, sym);
		fputs("rex->ex_op = EXOP_NUMBER;\n", cfile);
		indent(level);
		if (!tw.tw_bitfield) {
			fprintf(cfile, "%s = %s;\n", result, cg->cg_member);
			indent(level);
		}
		fputs("ok = 1; goto out;\n", cfile);
		endfetch(cg, sym);
	}
	if (tw.tw_bitfield) {
		indent(level);
		fprintf(cfile, "%s = %s;\n", cg->cg_member, result);
	}
}

static void
ambi_fetch_address(CodeGenerator *cg, TreeNode *decl)
{
	typeword_t tw;
	Symbol *sym;
	int bytes, bitaddr;
	TreeNode *init;

	tw = decl->tn_type;
	sym = decl->tn_sym;
	bytes = decl->tn_width / BitsPerByte;
	bitaddr = tw.tw_bitfield && bytes < BytesPerLong;
	if (sym->s_type == stElement)
		beginfetch(cg, sym);
	init = decl->tn_right;
	if (init && !(init->tn_flags & tfExpected)) {
		if (bitaddr)
			fputs("rex->ex_val = ", cfile);
		else
			fprintf(cfile, "%s = ", cg->cg_member);
		generateCexpr(init);
		fputs(";\n", cfile);
		indent(level);
	} else {
		fputs("if (!ds_", cfile);
		if (bitaddr) {
			fprintf(cfile,
			    "bits(ds, &rex->ex_val, %u, DS_%s_EXTEND)",
				(unsigned)decl->tn_left->tn_val,
				issigned(tw.tw_base) ? "SIGN" : "ZERO");
		} else {
			fprintf(cfile, "bytes(ds, %s, %d)",
				cg->cg_member, bytes);
		}
		fputs(")\n", cfile);
		indent(++level);
		fputs("goto out;\n", cfile);
		indent(--level);
	}
	if (sym->s_type != stElement)
		beginfetch(cg, sym);
	fputs("rex->ex_op = EXOP_ADDRESS;\n", cfile);
	if (!bitaddr) {
		indent(level);
		fprintf(cfile,
			"bcopy(%s, A_BASE(&rex->ex_addr, %d), %d);\n",
			cg->cg_member, bytes, bytes);
	}
	indent(level);
	fputs("ok = 1; goto out;\n", cfile);
	endfetch(cg, sym);
	if (bitaddr) {
		indent(level);
		fprintf(cfile, "%s = rex->ex_val;\n", cg->cg_member);
	}
}

static void
ambi_fetch_bool(CodeGenerator *cg, TreeNode *decl)
{
	fprintf(cfile, "/* XXX fetch bool */\n");
}

static void
ambi_fetch_opaque(CodeGenerator *cg, TreeNode *decl)
{
	fprintf(cfile, "/* XXX fetch opaque */\n");
}

static void
ambi_fetch_string(CodeGenerator *cg, TreeNode *decl)
{
	fprintf(cfile, "/* XXX fetch string */\n");
}

static void
ambi_fetch_struct(CodeGenerator *cg, TreeNode *decl)
{
	Symbol *sym, *tsym;

	sym = decl->tn_sym;
	tsym = typesym(decl->tn_type);
	beginfetch(cg, sym);
	fputs("rex->ex_op = EXOP_FIELD;\n", cfile);
	indent(level);
	if (sym->s_type == stElement)
		fputs("rex->ex_field = pf;\n", cfile);
	else {
		fprintf(cfile,
			"rex->ex_field = pf = &%s_fields[%u];\n",
			sym->s_scope->s_path, sym->s_index);
	}
	indent(level);
	if (sym->s_type == stElement) {
		if (tsym->s_flags & sfCantInline) {
			fprintf(cfile,
			"/* XXX can't inline struct element %s */;\n",
				sym->s_name);
		} else {
			fprintf(cfile,
			"/* XXX inlineable struct element %s */;\n",
				sym->s_name);
		}
	} else {
		fprintf(cfile, "pf->pf_cookie = %u;\n",
			decl->tn_bitoff / BitsPerByte);	/* XXX */
	}
	indent(level);
	if (sym->s_scope->s_sym->s_type == stStruct
	    && tsym->s_scope == sym->s_scope) {
		fprintf(cfile, "%s._sl = %s, ",
			cg->cg_member, sym->s_scope->s_sym->s_name);
	}
	if (decl->tn_flags & tfRecursive) {
		fprintf(cfile, "if (%s == 0)\n", cg->cg_member);
		indent(level+1);
		fprintf(cfile, "%s = znew(struct %s);\n",
			cg->cg_member, SN(tsym->s_tree)->sn_scope.s_path);
		indent(level);
		fprintf(cfile, "ps->ps_slink = %s;\n", cg->cg_member);
	} else {
		fprintf(cfile, "ps->ps_slink = &%s;\n", cg->cg_member);
	}
	indent(level);
	fputs("ok = 1; goto out;\n", cfile);
	endfetch(cg, sym);
}

static void
allocatearray(char *member, TreeNode *len)
{
	Symbol *esym;
	int width;

	esym = len->tn_elemsym;
	fprintf(cfile, "%s = ", member);
	if (len->tn_op == opNil) {
		width = esym->s_ewidth;
		if (width < 0)
			width = -width;
		fprintf(cfile,
			"_renew(%s, HOWMANY(ds->ds_count, %d) * sizeof(",
			member, HOWMANY(width, BitsPerByte));
		generateCtype(esym->s_etype, esym->s_ewidth, cfile);
		fputs("));\n", cfile);
	} else {
		fprintf(cfile, "renew(%s, ", member);
		generateCexpr(len);
		fputs(", ", cfile);
		generateCtype(esym->s_etype, esym->s_ewidth, cfile);
		fputs(");\n", cfile);
	}
	indent(level);
	fprintf(cfile, "if (%s == 0) goto out;\n", member);
}

static void
beginfetch(CodeGenerator *cg, Symbol *sym)
{
	TreeNode *decl, *dimlen, *len, *next;

	if (cg->cg_flags & cgfLookahead)
		return;
	level++;
	if (sym->s_type == stField) {
		fprintf(cfile, "if (fid == %sfid_%s) {\n",
			sym->s_scope->s_path, sym->s_name);
		indent(level);
		return;
	}
	assert(sym->s_type == stElement);
	fprintf(cfile, "  case %u:\n", sym->s_eid);
	decl = sym->s_tree;
	dimlen = sym->s_dimlen;
	if (dimlen == 0) {
		/* XXX wrong if non-flat */
		indent(level);
		fprintf(cfile, "if (!ds_seek(ds, %u + ",
			decl->tn_bitoff / BitsPerByte);
	}
	for (len = decl->tn_left->tn_left;
	     (next = len->tn_next) != dimlen;
	     len = next) {
		if (dimlen == 0) {
			putc('(', cfile);
			generateCexpr(len);
			fputs(") * ", cfile);
		}
	}
	if (dimlen == 0) {
		fprintf(cfile, "%d * pf->pf_cookie, DS_RELATIVE))\n",
			len->tn_elemsym->s_ewidth / BitsPerByte);
		indent(level+1);
		fputs("goto out;\n", cfile);
	}
	if (len->tn_elemsym->s_flags & sfCantInline) {
		indent(level);
		allocatearray(cg->cg_cookie, len);
	}
	indent(level);
}

static void
endfetch(CodeGenerator *cg, Symbol *sym)
{
	if (cg->cg_flags & cgfLookahead)
		return;
	--level;
	if (sym->s_type == stField) {
		indent(level);
		fputs("}\n", cfile);
	}
}

/*
 * Ambi-endian decode functions.
 */
static void	replayoffset(TreeNode *);
static void	replayoffsetandsize(TreeNode *, TreeNode *);
static char	*typeformat(typeword_t);

static void
ambi_decode_array(CodeGenerator *cg, TreeNode *decl)
{
	typeword_t tw;
	Symbol *sym, *esym;
	enum basetype bt;
	int array, index;
	TreeNode *len, *first, *next;
	unsigned int i, cc;
	char *format, *title;
	char pfname[128];	/* XXX unchecked */

	tw = decl->tn_type;
	sym = decl->tn_sym;
	bt = (enum basetype)tw.tw_base;
	array = (qualifier(tw) == tqArray
		 && !(bt == btChar || bt == btString || bt == btOpaque));

	if (array) {
		format = strcpy(pfname, sym->s_name) + sym->s_namlen;
		fputs("{ int ", cfile);
		i = 0;
		for (len = first = decl->tn_left->tn_left; len; len = next) {
			next = len->tn_next;
			format = strcpy(format, "[%d]") + 4;
			fprintf(cfile, "i%u%s", i, next ? ", " : ";\n");
			i++;
		}
		index = i;
		cc = strlen(cg->cg_member);
		for (len = first, i = 0; len; len = len->tn_next, i++) {
			esym = len->tn_elemsym;
			if (esym->s_flags & sfCantInline) {
				indent(level);
				allocatearray(cg->cg_member, len);
			}
			indent(level++);
			fprintf(cfile, "for (i%u = 0; ", i);
			if (len->tn_op == opNil)
				fputs("ds->ds_count > 0", cfile);
			else {
				fprintf(cfile, "i%u < ", i);
				generateCexpr(len);
			}
			fprintf(cfile, "; i%u++) {\n", i);
			cc += nsprintf(&cg->cg_member[cc], CG_NAMESIZE - cc,
				       "[i%u]", i);
		}
		indent(level);
		i = (format - pfname) + 10;
		title = sym->s_title;
		fprintf(cfile, "char name[%u]", i);
		if (title) {
			fprintf(cfile, ", title[%u]",
				i - sym->s_namlen + strlen(title));
		}
		fputs(";\n", cfile);
		indent(level);
		fprintf(cfile,
			"%s.pf_namlen = nsprintf(name, sizeof name, \"%s\"",
			cg->cg_cookie, pfname);
		for (i = 0; i < index; i++)
			fprintf(cfile, ", i%u", i);
		fputs(");\n", cfile);
		if (title) {
			indent(level);
			fprintf(cfile, "nsprintf(title, sizeof title, \"%s%s\"",
				title, pfname + sym->s_namlen);
			for (i = 0; i < index; i++)
				fprintf(cfile, ", i%u", i);
			fputs(");\n", cfile);
		}
		indent(level);
		fprintf(cfile, "%s.pf_name = name, %s.pf_title = %s;\n",
			cg->cg_cookie, cg->cg_cookie, title ? "title" : "name");
		indent(level);
	}
	
	(*cg->cg_decode[bt])(cg, decl);

	if (array) {
		while (--index >= 0) {
			indent(--level);
			fprintf(cfile, "} /* for i%d */\n", index);
		}
		indent(level);
		fputs("}\n", cfile);
	}
}

static void
ambi_decode_number(CodeGenerator *cg, TreeNode *decl)
{
	typeword_t tw, savetw;
	enum basetype bt;
	Symbol *sym, *esym;
	int string, variable, character;
	TreeNode *len, *init, *expect, *etn;
	char *format, *epath, *ename;

	tw = savetw = decl->tn_type;
	bt = (enum basetype)tw.tw_base;
	if (tw.tw_base == btEnum)
		setbasetype(&tw, typesym(tw)->s_tree->tn_enumtype.tw_base);
	sym = decl->tn_sym;
	string = (qualifier(tw) == tqArray && (bt == btChar || bt == btString));
	variable = 0;
	if (string) {
		(void) nsprintf(cg->cg_cookie, CG_NAMESIZE, "%s_fields[%u]",
				sym->s_scope->s_path, sym->s_index);
		len = decl->tn_left->tn_left;
		variable = len->tn_op != opNumber;
		if (len->tn_elemsym->s_flags & sfCantInline) {
			allocatearray(cg->cg_member, len);
			indent(level);
		}
	}
	init = decl->tn_right;
	if (init && (init->tn_flags & tfExpected))
		expect = init, init = 0;
	else
		expect = 0;
	if (init) {
		if (tw.tw_bitfield)
			fputs("val = ", cfile);
		else
			fprintf(cfile, "%s = ", cg->cg_member);
		generateCexpr(init);
	} else {
		fputs("if (!ds_", cfile);
		if (tw.tw_bitfield) {
			fprintf(cfile,
				"bits(ds, &val, %u, DS_%s_EXTEND)",
				(unsigned)decl->tn_width,
				issigned(tw.tw_base) ? "SIGN" : "ZERO");
		} else if (string) {
			fprintf(cfile, "bytes(ds, %s, MIN(ds->ds_count, ",
				cg->cg_member);
			generateCexpr(len);
			fputs("))", cfile);
		} else {
			fprintf(cfile, "%s(ds, &%s)",
				typesym(tw)->s_name, cg->cg_member);
		}
		fputs(")\n", cfile);
		indent(level+1);
		fputs("ds->ds_count = 0", cfile);
		if (tw.tw_bitfield) {
			fputs(";\n", cfile);
			indent(level);
			fprintf(cfile, "%s = val", cg->cg_member);
		}
	}
	fputs(";\n", cfile);
	if (qualifier(tw) != tqArray	/* XXX */
	    && (sym->s_flags & sfVariantSize)) {
		indent(level);
		fprintf(cfile, "%s.pf_size = %d;\n",
			cg->cg_cookie, tw.tw_bitfield ? -decl->tn_width
			: decl->tn_width / BitsPerByte);
	}
	indent(level);
	fprintf(cfile, "pv_%sfield(pv, &%s, ",
		(init ? "replay" : (variable ? "showvar" : "show")),
		cg->cg_cookie);
	if (tw.tw_bitfield) {
		fprintf(cfile, "&val");	/* XXX change lib/protocols */
		if (init)
			replayoffsetandsize(decl, init);
	} else if (string) {
		fputs(cg->cg_member, cfile);
		if (init)
			replayoffset(init);
		if (variable) {
			fputs(", ", cfile);
			generateCexpr(len);
		}
	} else {
		fprintf(cfile, "&%s", cg->cg_member);
		if (init)
			replayoffsetandsize(decl, init);
	}
	fputs(",\n", cfile);
	indent(level);
	fprintf(cfile, "\t     %s",
		(init ? "  " : (string ? "   " : "")));
	if (bt == btEnum) {
		etn = typesym(savetw)->s_tree;
		fprintf(cfile, "\"%%-%us", etn->tn_maxnamlen);
		if (expect)
			fputs(" [%%%s]", cfile);
		fputs("\", ", cfile);
		format = (etn->tn_op == opEnum) ? "en_name(&%s%s"
		       : "en_bitset(%s%s_values, lengthof(%s%s_values)";
		esym = etn->tn_sym;
		epath = esym->s_scope->s_path;
		ename = esym->s_name;
		fprintf(cfile, format, epath, ename, epath, ename);
		fprintf(cfile, ", %s)", cg->cg_member);
		if (expect) {
			fputs(", ", cfile);
			fprintf(cfile, format, epath, ename, epath, ename);
			fputs(", ", cfile);
			generateCexpr(expect);
			putc(')', cfile);
		}
	} else {
		format = typeformat(tw);
		fprintf(cfile, "\"%%%s", string ? ".*s" : format);
		if (expect)
			fprintf(cfile, " [%%%s]", format+strlen(format)-1);
		fputs("\", ", cfile);
		if (string) {
			generateCexpr(len);
			fputs(", ", cfile);
		}
		character = !tw.tw_qual && tw.tw_base == btChar;
		if (character)
			fputs("_char_image(", cfile);
		fputs(cg->cg_member, cfile);
		if (character)
			putc(')', cfile);
		if (expect) {
			fputs(", ", cfile);
			if (character)
				fputs("_char_image(", cfile);
			generateCexpr(expect);
			if (character)
				putc(')', cfile);
		}
	}
	fputs(");\n", cfile);
}

static void
ambi_decode_address(CodeGenerator *cg, TreeNode *decl)
{
	typeword_t tw;
	int bytes, bitaddr;
	TreeNode *init, *expect;
	Symbol *tsym;

	tw = decl->tn_type;
	bytes = decl->tn_width / BitsPerByte;
	bitaddr = tw.tw_bitfield && bytes < BytesPerLong;
	init = decl->tn_right;
	if (init && (init->tn_flags & tfExpected))
		expect = init, init = 0;
	else
		expect = 0;
	if (init) {
		if (bitaddr)
			fputs("val = ", cfile);
		else
			fprintf(cfile, "%s = ", cg->cg_member);
		generateCexpr(init);
	} else {
		fputs("if (!ds_", cfile);
		if (bitaddr) {
			fprintf(cfile,
				"bits(ds, &val, %u, DS_%s_EXTEND)",
				(unsigned)decl->tn_width,
				issigned(tw.tw_base) ? "SIGN" : "ZERO");
		} else {
			fprintf(cfile, "bytes(ds, %s, %d)",
				cg->cg_member, bytes);
		}
		fputs(")\n", cfile);
		indent(level+1);
		fputs("ds->ds_count = 0", cfile);
		if (bitaddr) {
			fputs(";\n", cfile);
			indent(level);
			fprintf(cfile, "%s = val", cg->cg_member);
		}
	}
	fputs(";\n", cfile);
	if (decl->tn_sym->s_flags & sfVariantSize) {
		indent(level);
		fprintf(cfile, "%s.pf_size = %d;\n",
			cg->cg_cookie, bitaddr ? -decl->tn_width : bytes);
	}
	indent(level);
	fprintf(cfile, "pv_%sfield(pv, &%s, ",
		(init ? "replay" : "show"), cg->cg_cookie);
	if (bitaddr) {
		fprintf(cfile, "&val");	/* XXX change lib/protocols */
		if (init)
			replayoffsetandsize(decl, init);
	} else {
		fprintf(cfile, "%s", cg->cg_member);
		if (init)
			replayoffsetandsize(decl, init);
	}
	fputs(",\n", cfile);
	indent(level);
	fprintf(cfile, "\t     %s\"%%s", init ? "  " : "");
	if (expect)
		fprintf(cfile, " [%%s]");
	tsym = typesym(decl->tn_decltype);
	fprintf(cfile, "\", %s_to_name(%s)", tsym->s_name, cg->cg_member);
	if (expect) {
		fprintf(cfile, ", %s_to_name(", tsym->s_name);
		generateCexpr(expect);
		putc(')', cfile);
	}
	fputs(");\n", cfile);
}

static void
ambi_decode_bool(CodeGenerator *cg, TreeNode *decl)
{
	fprintf(cfile, "/* XXX decode bool */\n");
}

static void
ambi_decode_opaque(CodeGenerator *cg, TreeNode *decl)
{
	Symbol *sym;

	sym = decl->tn_sym;
	fprintf(cfile, "pv_showvarfield(pv, &%s_fields[%u], ds->ds_next, ",
		sym->s_scope->s_path, sym->s_index);
	generateCexpr(decl->tn_left->tn_left);
	fputs(", 0);\n", cfile);
}

static void
ambi_decode_string(CodeGenerator *cg, TreeNode *decl)
{
	fprintf(cfile, "/* XXX decode string */\n");
}

static void
ambi_decode_struct(CodeGenerator *cg, TreeNode *decl)
{
	Symbol *sym, *psym, *tsym;

	sym = decl->tn_sym;
	psym = sym->s_scope->s_sym;
	tsym = typesym(decl->tn_type);
	if (psym->s_type == stStruct && tsym->s_scope == sym->s_scope)
		fprintf(cfile, "%s._sl = %s, ", cg->cg_member, psym->s_name);
	if (decl->tn_flags & tfRecursive) {
		/* XXX subroutine or unify otherwise with fetchfield */
		fprintf(cfile, "if (%s == 0)\n", cg->cg_member);
		indent(level+1);
		fprintf(cfile, "%s = znew(struct %s);\n",
			cg->cg_member, SN(tsym->s_tree)->sn_scope.s_path);
		indent(level);
		fprintf(cfile, "ps->ps_slink = %s;\n", cg->cg_member);
	} else {
		fprintf(cfile, "ps->ps_slink = &%s;\n", cg->cg_member);
	}
	indent(level);
	fprintf(cfile, "%s_struct.pst_parent = &%s;\n",
		SN(tsym->s_tree)->sn_scope.s_path, cg->cg_cookie);
	indent(level);
	fprintf(cfile, "%s_decode(ds, ps, pv);\n",
		SN(tsym->s_tree)->sn_scope.s_path);
}

static void
replayoffset(TreeNode *init)
{
	if (init->tn_op == opName && init->tn_sym->s_type == stField)
		fprintf(cfile, ", -1/*XXXoffset*/");
	else
		fputs(", -1", cfile);
}

static void
replayoffsetandsize(TreeNode *decl, TreeNode *init)
{
	int width;

	replayoffset(init);
	width = decl->tn_width;
	fprintf(cfile, ", %d",
		decl->tn_type.tw_bitfield ? -width : width / BitsPerByte);
}

static char *
typeformat(typeword_t tw)
{
	switch (tw.tw_base) {
	  case btChar:
		return "s";
	  case btUChar:
		return "-3u";
	  case btShort:
		return "-6d";
	  case btUShort:
		return "-5u";
	  case btInt:
	  case btSigned:
		return "-11d";
	  case btUInt:
	  case btUnsigned:
		return "-10u";
	  case btLong:
		return "-11ld";
	  case btULong:
		return "-10lu";
	  case btFloat:
	  case btDouble:
		return "g";
	}
	return "x";
}
