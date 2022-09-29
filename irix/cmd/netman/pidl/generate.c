/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL code generation.
 */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <net/raw.h>
#include "analyze.h"
#include "debug.h"
#include "heap.h"
#include "generate.h"
#include "macros.h"
#include "pidl.h"
#include "scope.h"
#include "treenode.h"
#include "type.h"

extern CodeGenerator big_endian_cg;
extern CodeGenerator little_endian_cg;

static CodeGenerator *cg;

static CodeGenerator *cgsw[drMax] = {
	&big_endian_cg,
	&big_endian_cg,
	&little_endian_cg,
};

int	level = 1;
FILE	*cfile;
FILE	*hfile;

void
indent(int lev)
{
	while (--lev >= 0)
		putc('\t', cfile);
}

static void	generateCstruct(Symbol *);
static void	generateProtocol(Symbol *, TreeNode *);
static void	passthru(int, char *);

void
generate(TreeNode *tn, Context *cx, char *cname)
{
	int lineno;
	Symbol *psym;
	TreeNode *prot;

	/*
	 * Include prerequisite include files in cfile.
	 */
	lineno = 1;
	if (cx->cx_flags & cxCacheFields) {
		fputs("#include \"cache.h\"\n", cfile);
		lineno++;
	}
	if (cx->cx_flags & cxEnumFields) {
		fputs("#include \"enum.h\"\n", cfile);
		lineno++;
	}
	if (cx->cx_flags & cxHeapFields) {
		fputs("#include \"heap.h\"\n", cfile);
		lineno++;
	}

	/*
	 * Check for protocol-specific prerequisite includes.
	 */
	psym = cx->cx_scope->s_protocols.sl_head;
	if (psym) {
		fputs("#include \"protodefs.h\"\n#include \"protoindex.h\"\n",
		      cfile);
		lineno += 2;
	}

	/*
	 * Generate an #include for each imported module that has exports.
	 */
	for (tn = tn->tn_left; tn; tn = tn->tn_next) {
		if (tn->tn_op == opImport
		    && (tn->tn_sym->s_flags & sfExported)) {
			fprintf(cfile, "#include \"%s.h\"\n",
				basename(tn->tn_sym->s_name, ".pi"));
			lineno++;
		}
	}

	/*
	 * Generate an include for this module's header.
	 */
	if ((cx->cx_flags & cxHeaderFile) && hfile) {
		fprintf(cfile, "#include \"%s.h\"\n", cx->cx_scope->s_name);
		lineno++;
	}

	/*
	 * Incorporate pass-thru before any generated function calls.
	 */
	if (ptfile)
		passthru(lineno, cname);

	/*
	 * Generate all exported protocols in the outer scope.
	 */
	while (psym) {
		if (!(psym->s_flags & sfImported)) {
			prot = psym->s_tree;
			cg = cgsw[prot->tn_type.tw_rep];
			generateCstruct(psym);
			generateProtocol(psym, prot);
		}
		psym = psym->s_next;
	}
}

static void	generatescope(Scope *);
static void	generateCdeclaration(TreeNode *, FILE *);

static void
generateCstruct(Symbol *ssym)
{
	Scope *s, *t;
	FILE *file;
	Symbol *sym;
	TreeNode *tn;

	s = &SN(ssym->s_tree)->sn_scope;
	if (!(ssym->s_flags & sfTemporary))
		generatescope(s);
	file = (ssym->s_flags & sfExported && hfile) ? hfile : cfile;
	fprintf(file, "\nstruct %s {\n", s->s_path);
	switch (ssym->s_type) {
	  case stProtocol:
		fputs("\tProtoStackFrame _psf;\n", file);
		break;
	  case stStruct:
		if (ssym->s_flags & sfTemporary)
			break;
		t = s->s_outer;
		sym = t->s_sym;
		if (sym && sym->s_type == stStruct)
			fprintf(file, "\tstruct %s *_sl;\n", t->s_path);
	}
	for (sym = s->s_fields.sl_head; sym; sym = sym->s_next) {
		tn = sym->s_tree;
		do {
			generateCdeclaration(tn, file);
		} while ((tn = tn->tn_overload) != 0);
	}
	fprintf(file, "}; /* %s */\n", s->s_path);
}

enum entrypoint {
	SetOpt, Embed, Resolve, Compile, Match, Fetch, Decode,
	SizeOf, Offset
};

static void	generateProtoField(Symbol *, typeword_t, TreeNode *, int,
				   unsigned int, char);
static void	generateProtoMacros(Symbol *, char *, char *);
static void	generateEnumerator(Symbol *, char *);
static void	beginfunction(enum entrypoint, char *);
static void	endfunction(enum entrypoint, char *);
static void	generateCprologue(Symbol *, enum entrypoint);
static void	fetchelements(Scope *, void (*)(TreeNode *));
static void	generateCstatement(TreeNode *, void (*)(TreeNode *), void *);

static int
enumcmp(const void *p1, const void *p2)
{
	return (*(Symbol **)p1)->s_val - (*(Symbol **)p2)->s_val;
}

static void
generatescope(Scope *s)
{
	char *path, *name;
	Symbol *sym;
	TreeNode *tn;

	/*
	 * Choose a prefix for this scope's symbols based on its pathname.
	 */
	path = s->s_path;
	assert(path != 0);

	/*
	 * Generate exported typedefs.
	 */
	if (hfile) {
		for (sym = s->s_typedefs.sl_head; sym; sym = sym->s_next) {
			if (!(sym->s_flags & sfExported))
				continue;
			fputs("\ntypedef", hfile);
			generateCdeclaration(sym->s_tree, hfile);
		}
	}

	/*
	 * Generate struct descriptors.
	 */
	for (sym = s->s_structs.sl_head; sym; sym = sym->s_next) {
		if (sym->s_flags & sfTemporary) {
			if (sym->s_flags & sfDirect)
				generateCstruct(sym);
			continue;
		}
		generateCstruct(sym);
		name = sym->s_name;
		/* XXX protocol path is wrong for outer structs */
		fprintf(cfile,
		"\nstatic ProtoStruct %s%s_struct = PSI(\"%s\",%s%s_fields);\n",
			path, name, name, path, name);
	}

	/*
	 * Generate field IDs and descriptors.
	 */
	sym = s->s_fields.sl_head;
	if (sym) {
		int cfirst, hfirst, exported;
		typeword_t tw;

		/*
		 * Generate private and exported fid #defines.
		 */
		cfirst = hfirst = 1;
		do {
			tw = sym->s_tree->tn_type;
			exported = (sym->s_flags & sfExported);
			if (exported && hfile) {
				if (hfirst) {
					putc('\n', hfile);
					hfirst = 0;
				}
			} else {
				if (cfirst) {
					putc('\n', cfile);
					cfirst = 0;
				}
			}
			fprintf(exported && hfile ? hfile : cfile,
				"#define %sfid_%s %u\n",
				path, sym->s_name, sym->s_fid);
		} while ((sym = sym->s_next) != 0);

		/*
		 * Generate recursive ProtoStruct forward declarations.
		 */
		for (sym = s->s_fields.sl_head; sym; sym = sym->s_next) {
			for (tn = sym->s_tree; tn; tn = tn->tn_overload) {
				tw = tn->tn_type;
				if (tw.tw_base != btStruct)
					continue;
				if (!(tn->tn_flags & tfRecursive))
					continue;
				fprintf(cfile,
				    "\nextern ProtoStruct %s_struct;\n",
				    SN(typesym(tw)->s_tree)->sn_scope.s_path);
				break;
			}
		}

		/*
		 * Generate array element descriptors.
		 */
		for (sym = s->s_elements.sl_head; sym; sym = sym->s_next) {
			fprintf(cfile, "\nstatic ProtoField %s%s_element =\n",
				path, sym->s_name + 1);
			generateProtoField(sym, sym->s_etype, sym->s_dimlen,
					   sym->s_ewidth, 0, ';');
		}

		/*
		 * Finally, generate the field descriptor array.
		 */
		fprintf(cfile, "\nstatic ProtoField %s_fields[] = {\n", path);
		for (sym = s->s_fields.sl_head; sym; sym = sym->s_next) {
			tn = sym->s_tree;
			tw = tn->tn_type;
			generateProtoField(sym, tn->tn_type, first(tn->tn_left),
					   tn->tn_width, tn->tn_bitoff, ',');
		}
		fputs("};\n", cfile);
	}

	/*
	 * Generate macro descriptors.
	 */
	sym = s->s_macros.sl_head;
	if (sym)
		generateProtoMacros(sym, path, "macros");

	/*
	 * Generate nickname descriptors.
	 */
	sym = s->s_nicknames.sl_head;
	if (sym)
		generateProtoMacros(sym, path, "nicknames");

	/*
	 * Generate constant descriptors.
	 */
	sym = s->s_consts.sl_head;
	if (sym) {
		int exports;

		fprintf(cfile, "\nstatic Enumerator %s_consts[] = {\n", path);
		for (exports = 0; sym; sym = sym->s_next) {
			if (sym->s_type != stNumber)
				continue;
			if (!exports && (sym->s_flags & sfExported) && hfile) {
				putc('\n', hfile);
				exports = 1;
			}
			generateEnumerator(sym, path);
		}
		fputs("};\n", cfile);
	}

	/*
	 * Generate enums and enumerators.
	 */
	for (sym = s->s_enums.sl_head; sym; sym = sym->s_next) {
		Symbol **vec;
		int i, veclen;

		name = sym->s_name;
		if (sym->s_tree->tn_op == opEnum) {
			fprintf(cfile, "\nstatic Enumeration %s%s;\n",
				path, name);
		}
		fprintf(cfile, "static Enumerator %s%s_values[] = {\n",
			path, name);
		tn = sym->s_tree->tn_kid;
		veclen = tn->tn_arity;
		vec = vnew(veclen, Symbol *);
		i = 0;
		for (tn = tn->tn_left; tn; tn = tn->tn_next)
			vec[i++] = tn->tn_sym;
		assert(i == veclen);
		qsort(vec, veclen, sizeof *vec, enumcmp);
		for (i = 0; i < veclen; i++)
			generateEnumerator(vec[i], path);
		delete(vec);
		fputs("};\n", cfile);
	}

#ifdef NOTDEF
	/*
	 * Generate sizeof and offset functions if we can't inline.
	 * XXX should generate only if called.
	 */
	sym = s->s_sym;
	if (sym->s_flags & sfCantInline) {
		tn = sym->s_tree;
		if (sym->s_type == stProtocol) {
			fprintf(cfile, "\nextern struct %s %s_frame;\n",
				sym->s_name, sym->s_name);
		}

		generateCprologue(sym, SizeOf);
		fputs("\tlong val;\n\n", cfile);
		cg->cg_flags = cgfSizeOf;
		fetchelements(s, cg->cg_sizeof);
		generateCstatement(tn, cg->cg_sizeof, "return 0");
		fputs("\treturn 0;\n", cfile);
		endfunction(SizeOf, s->s_path);

		generateCprologue(sym, Offset);
		fputs("\tint off = 0;\n\tlong val;\n\n", cfile);
		cg->cg_flags = cgfOffset;
		generateCstatement(tn, cg->cg_offset, "return -1");
		fputs("\treturn -1;\n", cfile);
		endfunction(Offset, s->s_path);
	}
#endif
}

void
generateCtype(typeword_t tw, unsigned short width, FILE *file)
{
	Symbol *tsym;

	tsym = typesym(tw);
	switch (tw.tw_base) {
	  case btCache:
		fputs("Cache *", file);
		break;

	  case btAddress:
		if (width <= BitsPerChar)
			fputs("unsigned char", file);
		else if (width <= BitsPerShort)
			fputs("unsigned short", file);
		else if (width <= BitsPerLong)
			fputs("unsigned long", file);
		else if (width < BitsPerAddress)
			fputs("unsigned char", file);
		else
			fputs("Address", file);
		break;

	  case btBool:
		fputs("int", file);
		break;

	  case btOpaque:
		fputs("unsigned char", file);
		break;

	  case btString:
		fputs("char", file);
		break;

	  case btStruct:
		fprintf(file, "struct %s", SN(tsym->s_tree)->sn_scope.s_path);
		break;

	  case btEnum:
		setbasetype(&tw, tsym->s_tree->tn_enumtype.tw_base);
		tsym = typesym(tw);
		/* FALL THROUGH */
	  default:
		if (isunsigned(tw.tw_base)) {
			fputs("unsigned ", file);
			setbasetype(&tw, sign(tw.tw_base));
			tsym = typesym(tw);
		}
		fputs(tsym->s_name, file);
	}
}

static void
generateCdeclarator(TreeNode *tn, typeword_t tw, TreeNode *len, FILE *file)
{
	Symbol *sym;
	enum typequal tq;
	unsigned int width;

	sym = tn->tn_sym;
	tq = (enum typequal)qualifier(tw);
	switch (tq) {
	  case tqPointer:
		putc('*', file);
		generateCdeclarator(tn, unqualify(tw), len, file);
		return;
	  case tqArray:
		if (len->tn_elemsym->s_flags & sfCantInline) {
			tq = tqPointer;
			putc('*', file);
		}
		generateCdeclarator(tn, unqualify(tw), len->tn_next, file);
		if (tq == tqArray)
			fprintf(file, "[%u]", (unsigned)len->tn_val);
		return;
	  case tqVarArray:
		fprintf(cfile, "/* XXX variable array */");
		return;
	}
	if (sym) {
		if (tn->tn_flags & tfRecursive)
			putc('*', file);
		fprintf(file, "%s%s",
			sym->s_name, tn->tn_suffix ? tn->tn_suffix : "");
	}
	if (tw.tw_bitfield) {
		width = len->tn_val;
		if (width > BitsPerLong)
			fprintf(file, "[%u]", width / BitsPerChar);
		else
			fprintf(file, ":%u", width);
	}
}

static void
generateCdeclaration(TreeNode *tn, FILE *file)
{
	typeword_t tw;

	tw = tn->tn_type;
	if (tw.tw_base == btVoid && qualifier(tw) == tqNil)
		return;
	putc('\t', file);
	generateCtype(tn->tn_type, tn->tn_width, file);
	putc(' ', file);
	generateCdeclarator(tn, tn->tn_type, first(tn->tn_left), file);
	fputs(";\n", file);
}

static void
generateProtoField(Symbol *sym, typeword_t tw, TreeNode *len, int width,
		   unsigned int bitoff, char sep)
{
	char *name, *path, *pf_type;
	int element;

	name = sym->s_name;
	path = sym->s_scope->s_path;
	fputs("\t{", cfile);
	element = (sym->s_type == stElement);
	if (element)
		fprintf(cfile, "0,0,0,%u,", sym->s_eid);
	else {
		fprintf(cfile, "\"%s\",%u,\"%s\",%sfid_%s,",
			name, sym->s_namlen, sym->s_title ? sym->s_title : name,
			path, name);
	}
	if (width < 0)
		width = 0;	/* PF_VARIABLE */
	if (qualifier(tw) == tqArray) {
		fprintf(cfile, "&%s%s_element,%d,",
			path, len->tn_elemsym->s_name + 1,
			(len->tn_op == opNumber) ? (int)len->tn_val : 0);
		pf_type = "ARRAY";
	} else if (tw.tw_base == btStruct) {
		fprintf(cfile, "&%s_struct,%d,",
			SN(typesym(tw)->s_tree)->sn_scope.s_path,
			width / BitsPerByte);
		pf_type = "STRUCT";
	} else {
		switch (tw.tw_base) {
		  case btVoid:
			pf_type = "NULL";
			break;
		  case btAddress:
			pf_type = "ADDRESS";
			break;
		  default:
			pf_type = "NUMBER";
		}
		fprintf(cfile, "(void*)DS_%s_EXTEND,%d,",
			issigned(tw.tw_base) ? "SIGN" : "ZERO",
			(width % BitsPerByte) ? -width : width / BitsPerByte);
	}
	fprintf(cfile, "%u,EXOP_%s,%u}%c\n",
		(width % BitsPerByte) ? bitoff : bitoff / BitsPerByte, pf_type,
		element ? sym->s_tree->tn_sym->s_level : sym->s_level, sep);
}

static void
generateProtoMacros(Symbol *msym, char *path, char *table)
{
	TreeNode *tn;
	Symbol *sym, *def;

	fprintf(cfile, "\nstatic ProtoMacro %s_%s[] = {\n", path, table);
	for (; msym; msym = msym->s_next) {
		tn = msym->s_tree;
		sym = tn->tn_sym;
		def = tn->tn_def;
		fprintf(cfile, "\t{\"%s\",%u,\"%s\",%u},\n",
			sym->s_name, sym->s_namlen,
			def->s_name, def->s_namlen);
	}
	fputs("};\n", cfile);
}

static void
generateEnumerator(Symbol *sym, char *path)
{
	char *name;

	name = sym->s_name;
	fprintf(cfile, "\t{\"%s\",%u,%ld},\n", name, sym->s_namlen, sym->s_val);
	if ((sym->s_flags & sfExported) && hfile)
		fprintf(hfile, "#define	%s_%s %ld\n", path, name, sym->s_val);
}

static int	minstackdepth(TreeNode *);
static void	initstruct(Symbol *sym);
static void	declarefunction(enum entrypoint, char *);
static void	generateCprologue(Symbol *, enum entrypoint);
static void	matchprotocol(char *, TreeNode *);
static void	generatefetch(Symbol *, TreeNode *, TreeNode *);
static void	generatedecode(Symbol *, TreeNode *, TreeNode *);
static char	*uppercase(char *);

static void
generateProtocol(Symbol *psym, TreeNode *prot)
{
	char *pname, *load, *time, *cname, *ename;
	TreeNode *tn, *formals, *bases, *type, *qual;
	int discrim, depth, compile, match;
	Scope *s;
	Symbol *sym;
	static char stubprefix[] = "pr_stub";

	/*
	 * Generate a static protocol stack frame struct, for use by match,
	 * fetch, and decode.
	 */
	pname = psym->s_name;
	fprintf(cfile, "\nstatic struct %s %s_frame;\n", pname, pname);

	/*
	 * Generate a protocol index in which to look for higher layer
	 * protocols' typecodes, if we have formals.
	 */
	discrim = -1;
	formals = prot->tn_kid3->tn_left;
	if (formals) {
		fprintf(cfile, "\nstatic ProtoIndex *%s_index;\n", pname);
		tn = first(formals);
		if (tn->tn_op == opName && tn->tn_next == 0)
			discrim = tn->tn_sym->s_index;
	}

	/*
	 * Generate forward protocol op declarations.  Compute minimum
	 * depth (bytes) of possible protocol stacks beneath this one to
	 * decide whether a compile function should be generated.
	 */
	fprintf(cfile, "\nstatic int\t%s_init(void);\n", pname);
#ifdef TODO
	declarefunction(SetOpt, pname);
#endif
	if (formals)
		declarefunction(Embed, pname);
#ifdef TODO
	declarefunction(Resolve, pname);
#endif
	bases = prot->tn_kid3->tn_right;
	depth = minstackdepth(bases);
	compile = (0 <= depth && depth < SNOOP_FILTERLEN * BitsPerByte);
	if (compile)
		declarefunction(Compile, pname);
	match = (formals || (psym->s_flags & sfDynamicNest));
	if (match)
		declarefunction(Match, pname);
	declarefunction(Fetch, pname);
	declarefunction(Decode, pname);

	/*
	 * Generate the initialized protocol struct definition.
	 */
	fprintf(cfile, "\nProtocol %s_proto = {\n", pname);

	fprintf(cfile, "\t\"%s\",%u,\"%s\",PRID_%s,\n",
		pname, psym->s_namlen, psym->s_title ? psym->s_title : pname,
		uppercase(pname));
	fprintf(cfile, "\tDS_%s,%d,",
		cg->cg_drepname, HOWMANY(prot->tn_width, BitsPerByte));
	if (discrim >= 0)
		fprintf(cfile, "&%s_fields[%d],\n", pname, discrim);
	else
		fputs("0,\n", cfile);

	fprintf(cfile, "\t%s_init,%s_setopt,%s_embed,%s_resolve,\n",
		pname, stubprefix, formals ? pname : stubprefix, stubprefix);
	fprintf(cfile, "\t%s_compile,%s_match,%s_fetch,%s_decode,\n",
		compile ? pname : stubprefix, match ? pname : stubprefix,
		pname, pname);
	fprintf(cfile, "\t%s,0,0,0\n};\n",
		(psym->s_flags & sfDynamicNest) ? "PR_MATCHNOTIFY" : "0");

	/*
	 * Generate the protocol init routine.
	 */
	fprintf(cfile, "\nstatic int\n%s_init()\n{\n", pname);

	/*
	 * Generate the pr_register call, including scope loading.
	 */
	fprintf(cfile,
		"\tif (!pr_register(&%s_proto, %s_fields, lengthof(%s_fields),",
		pname, pname, pname);
	s = &SN(prot)->sn_scope;
	load = haspragma(psym, "scopeload");
	fprintf(cfile, "\n\t\t\t %s\t/* scopeload */", load ? load : "1");
	if (s->s_consts.sl_head)
		fprintf(cfile, "\n\t\t\t + lengthof(%s_consts)", pname);
	for (sym = s->s_enums.sl_head; sym; sym = sym->s_next) {
		fprintf(cfile, "\n\t\t\t + lengthof(%s%s_values)",
			pname, sym->s_name);
	}
	if (s->s_macros.sl_head)
		fprintf(cfile, "\n\t\t\t + lengthof(%s_macros)", pname);
	if (s->s_nicknames.sl_head)
		fprintf(cfile, "\n\t\t\t + lengthof(%s_nicknames)", pname);
	fprintf(cfile, ")) {\n\t\treturn 0;\n\t}\n");

	/*
	 * Nest in base protocols, if we're not bottom-layer.
	 */
	if (bases) {
		fputs("\tif (!(", cfile);
		tn = first(bases);
		for (;;) {
			type = tn->tn_kid;
			qual = 0;
			if (type) {
				if (type->tn_op == opQualify) {
					qual = type->tn_right;
					assert(qual->tn_op == opNumber);
					type = type->tn_left;
				}
				assert(type->tn_op == opNumber);
			}
			fprintf(cfile, "pr_nest%s(&%s_proto, PRID_%s, %ld, ",
				qual ? "qual" : "", pname,
				uppercase(tn->tn_sym->s_name),
				type ? type->tn_val : 0);
			if (qual)
				fprintf(cfile, "%ld, ", qual->tn_val);
			if (s->s_nicknames.sl_head == 0)
				fputs("0, 0", cfile);
			else {
				fprintf(cfile,
			    "%s_nicknames,\n\t\t      lengthof(%s_nicknames)",
					pname, pname);
			}
			putc(')', cfile);
			tn = tn->tn_next;
			if (tn == 0)
				break;
			fputs(" &\n\t      ", cfile);
		}
		fputs(")) {\n\t\treturn 0;\n\t}\n", cfile);
	}

	/*
	 * Initialize any caches.  XXX optimize with a separate list?
	 */
	for (sym = s->s_fields.sl_head; sym; sym = sym->s_next) {
		tn = sym->s_tree;
		if (tn->tn_type.tw_base != btCache)
			continue;
		cname = sym->s_name;
		fprintf(cfile, "\t%s%s_create(&%s_frame.%s%s);\n",
			pname, cname, pname, cname,
			tn->tn_suffix ? tn->tn_suffix : "");
	}

	/*
	 * Initialize constants.
	 */
	if (s->s_consts.sl_head) {
		fprintf(cfile,
	    "\n\tpr_addnumbers(&%s_proto, %s_consts, lengthof(%s_consts));\n",
			pname, pname, pname);
	}

	/*
	 * Initialize enums.
	 */
	for (sym = s->s_enums.sl_head; sym; sym = sym->s_next) {
		ename = sym->s_name;
		if (sym->s_tree->tn_op == opEnum) {
			fprintf(cfile,
    "\ten_init(&%s%s, %s%s_values, lengthof(%s%s_values),\n\t\t&%s_proto);\n",
				pname, ename, pname, ename, pname, ename,
				pname);
		} else {
			fprintf(cfile,
	    "\tpr_addnumbers(&%s_proto, %s%s_values, lengthof(%s%s_values));\n",
				pname, pname, ename, pname, ename);
		}
	}

	/*
	 * Initialize macros.
	 */
	if (s->s_macros.sl_head) {
		fprintf(cfile,
	    "\tpr_addmacros(&%s_proto, %s_macros, lengthof(%s_macros));\n",
			pname, pname, pname);
	}

	/*
	 * Initialize structures.
	 */
	for (sym = s->s_structs.sl_head; sym; sym = sym->s_next) {
		if (!(sym->s_flags & sfTemporary))
			initstruct(sym);
	}

	/*
	 * If we're nonterminal, initialize an index to map typecodes to
	 * higher-layer protocols.
	 */
	if (formals) {
		time = haspragma(psym, "freshtime");
		load = haspragma(psym, "indexload");
		fprintf(cfile, "\n\tpin_create(\"%s\", %s, %s, &%s_index);\n",
			pname, time ? time : "0", load ? load : "8", pname);
	}

	fputs("\n\treturn 1;\n}\n", cfile);

	/*
	 * Generate the setopt routine.
	 */
#ifdef TODO
	beginfunction(SetOpt, pname);
	endfunction(SetOpt, pname);
#endif

	/*
	 * Generate the embed routine.
	 */
	if (formals) {
		beginfunction(Embed, pname);
		fprintf(cfile,
			"\tif (pr)\n\t\tpin_enter(%s_index, type, qual, pr);\n",
			pname);
		fprintf(cfile,
			"\telse\n\t\tpin_remove(%s_index, type, qual);\n",
			pname);
		endfunction(Embed, pname);
	}

	/*
	 * Generate the resolve routine.
	 */
#ifdef TODO
	beginfunction(Resolve, pname);
	endfunction(Resolve, pname);
#endif

	/*
	 * Generate the compile routine.
	 */
	if (compile) {
		beginfunction(Compile, pname);
		fprintf(cfile, "\treturn ET_COMPLEX;\n");	/* XXX */
		endfunction(Compile, pname);
	}

	/*
	 * Generate the match routine.
	 * TODO: optimize the fetch(all) into something faster.
	 */
	if (match) {
		generateCprologue(psym, Match);
		fputs("\tProtoField all;\n", cfile);
		if (formals)
			fputs("\tProtocol *pr;\n\tint matched;\n", cfile);
		fprintf(cfile, "\n\tall.pf_id = %u;\n", lastfid);
		fprintf(cfile, "\t(void) %s_fetch(&all, ds, ps, rex);\n",
			pname);
		if (formals == 0)
			fputs("\treturn 1;\n", cfile);
		else {
			if (psym->s_flags & sfDynamicNest)
				fputs("\tif (pex == 0) return 1;\n", cfile);
			fprintf(cfile, "\tPS_PUSH(ps, &%s->_psf, &%s_proto);\n",
				pname, pname);
			matchprotocol(pname, prot);
			fputs("\tmatched = (pr == pex->ex_prsym->sym_proto\n",
				cfile);
			fputs("\t\t&& ex_match(pex, ds, ps, rex));\n", cfile);
			fputs("\tPS_POP(ps);\n", cfile);
			fputs("\treturn matched;\n", cfile);
		}
		endfunction(Match, pname);
	}

	/*
	 * Generate fetch routines.
	 */
	generatefetch(psym, prot->tn_kid2, prot->tn_kid3);

	/*
	 * Generate decode routines.
	 */
	generatedecode(psym, prot->tn_kid2, prot->tn_kid3);
}

static int
minstackdepth(TreeNode *bases)
{
	TreeNode *base, *prot, *foot;
	int mindepth, width, depth;

	mindepth = 0;
	for (base = first(bases); base; base = base->tn_next) {
		prot = base->tn_sym->s_tree;
		assert(prot->tn_op == opProtocol);
		width = prot->tn_width;
		if (width < 0)
			return width;
		foot = prot->tn_kid1;
		if (foot && foot->tn_width >= 0)
			width -= foot->tn_width;
		depth = minstackdepth(prot->tn_kid3->tn_right);
		if (depth < 0)
			return depth;
		depth += width / BitsPerByte;
		if (mindepth == 0 || depth < mindepth)
			mindepth = depth;
	}
	return mindepth;
}

static void
initstruct(Symbol *sym)
{
	Scope *s;
	char *path, *name;
	Symbol *esym;

	s = &SN(sym->s_tree)->sn_scope;
	path = s->s_path;
	if (s->s_consts.sl_head) {
		fprintf(cfile,
    "\tsc_addnumbers(&%s_struct.pst_scope, %s_consts, lengthof(%s_consts));\n",
			path, path, path);
	}
	for (esym = s->s_enums.sl_head; esym; esym = esym->s_next) {
		name = esym->s_name;
		if (esym->s_tree->tn_op == opEnum) {
			fprintf(cfile,
		    "\ten_initscope(&%s%s,%s%s_values,lengthof(%s%s_values),\n",
				path, name, path, name, path, name);
			fprintf(cfile,
				"\t\t     &%s_struct.pst_scope);\n",
				path);
		} else {
			fprintf(cfile,
			"\tsc_addnumbers(&%s_struct.pst_scope, %s%s_values,\n",
				path, path, name);
			fprintf(cfile,
				"\t\t      lengthof(%s%s_values));\n",
				path, name);
		}
	}
	if (s->s_macros.sl_head) {
		fprintf(cfile,
    "\tsc_addmacros(&%s_struct.pst_scope, %s_macros, lengthof(%s_macros));\n",
			path, path, path);
	}
	for (sym = s->s_structs.sl_head; sym; sym = sym->s_next) {
		if (!(sym->s_flags & sfTemporary))
			initstruct(sym);
	}
}

static struct entrystrings {
	char	*type;
	char	*name;
	char	*formals;
} entrystrings[] = {
	{ "int", "setopt",
	  "int id, char *val" },
	{ "void", "embed",
	  "Protocol *pr, long type, long qual" },
	{ "Expr *", "resolve",
	  "char *str, int len, struct snooper *sn" },
	{ "ExprType", "compile",
	  "ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc" },
	{ "int", "match",
	  "Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex" },
	{ "int", "fetch",
	  "ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex" },
	{ "void", "decode",
	  "DataStream *ds, ProtoStack *ps, PacketView *pv" },
	{ "int", "sizeof",
	  "int fid, DataStream *ds, ProtoStack *ps" },
	{ "int", "offset",
	  "int fid, DataStream *ds, ProtoStack *ps" },
};

static void
declarefunction(enum entrypoint ep, char *prefix)
{
	struct entrystrings *es;

	es = &entrystrings[ep];
	fprintf(cfile, "static %s\t%s_%s(%s);\n",
		es->type, prefix, es->name, es->formals);
}

static void
beginfunction(enum entrypoint ep, char *prefix)
{
	struct entrystrings *es;

	es = &entrystrings[ep];
	fprintf(cfile, "\nstatic %s\n%s_%s(%s)\n{\n",
		es->type, prefix, es->name, es->formals);
}

static void
endfunction(enum entrypoint ep, char *prefix)
{
	fprintf(cfile, "} /* %s_%s */\n",
		prefix, entrystrings[ep].name);
}

void
generateCexpr(TreeNode *tn)
{
	enum operator op;
	Symbol *sym;
	int parens, depth;
	TreeNode *kid;

	op = tn->tn_op;
	switch (op) {
	  case opNil:
		fputs("ds->ds_count", cfile);
		break;

	  case opBase:
		fprintf(cfile, "PRID_%s", uppercase(tn->tn_sym->s_name));
		break;

	  case opAssign:
		kid = tn->tn_left;
		if (kid->tn_op != opName) {
			generateCexpr(kid);
			fputs(" = ", cfile);
			generateCexpr(tn->tn_right);
			break;
		}
		sym = kid->tn_sym;
		fprintf(cfile, "%s = ", sym->s_name);
		kid = tn->tn_right;
		parens = (kid->tn_op != opSlinkRef);
		if (parens) {
			fprintf(cfile, "(struct %s *)(",
				SN(sym->s_tree)->sn_scope.s_path);
		}
		generateCexpr(kid);
		if (parens)
			putc(')', cfile);
		break;

	  case opCond:
		generateCexpr(tn->tn_kid3);
		putc('?', cfile);
		kid = tn->tn_kid2;
		parens = (kid->tn_op < op);
		if (parens)
			putc('(', cfile);
		generateCexpr(kid);
		if (parens)
			putc(')', cfile);
		putc(':', cfile);
		kid = tn->tn_kid1;
		parens = (kid->tn_op < op);
		if (parens)
			putc('(', cfile);
		generateCexpr(kid);
		if (parens)
			putc(')', cfile);
		break;

	  case opIndex:
		generateCexpr(tn->tn_left);
		putc('[', cfile);
		generateCexpr(tn->tn_right);
		putc(']', cfile);
		break;

	  case opSizeOf:
		fprintf(cfile, "0");
		break;

	  case opCast:
		kid = tn->tn_left;
		putc('(', cfile);
		generateCtype(kid->tn_type, kid->tn_width, cfile);
		generateCdeclarator(kid, kid->tn_type, first(kid->tn_left),
				    cfile);
		putc(')', cfile);
		kid = tn->tn_right;
		parens = (kid->tn_op < opDeref);
		if (parens)
			putc('(', cfile);
		generateCexpr(kid);
		if (parens)
			putc(')', cfile);
		break;

	  case opCall:
		fprintf(cfile, "%s(", tn->tn_sym->s_name);
		for (kid = tn->tn_kid->tn_left; kid; kid = kid->tn_next) {
			parens = (kid->tn_op <= opComma);
			if (parens)
				putc('(', cfile);
			generateCexpr(kid);
			if (parens)
				putc(')', cfile);
			if (kid->tn_next)
				fputs(", ",cfile);
		}
		putc(')', cfile);
		break;

	  case opBlock:
		/*
		 * tn_left points to the block's struct definition.
		 */
		generateCexpr(tn->tn_right);
		break;

	  case opName:
		sym = tn->tn_sym;
		switch (sym->s_type) {
		  case stProtocol:
		  case stStruct:
			fprintf(cfile,
				(sym->s_flags & sfDirect) ? "%s" : "(*%s)",
				sym->s_name);
			break;
		  case stField:
			fputs(sym->s_name, cfile);
		}
		break;

	  case opNumber:
		fprintf(cfile, "%ld", tn->tn_val);
		break;

	  case opAddress:
		/* XXX how do we know the length? */
		fprintf(cfile, "\"\\%o\\%o\\%o\\%o\\%o\\%o\\%o\\%o\"",
			tn->tn_addr.a_vec[0], tn->tn_addr.a_vec[1],
			tn->tn_addr.a_vec[2], tn->tn_addr.a_vec[3],
			tn->tn_addr.a_vec[4], tn->tn_addr.a_vec[5],
			tn->tn_addr.a_vec[6], tn->tn_addr.a_vec[7]);
		break;

	  case opString:
		fprintf(cfile, "\"%s\"", tn->tn_sym->s_name);
		break;

	  case opDataStream:
	  case opProtoStack:
		fputs(opnames[op], cfile);
		break;

	  case opStackRef:
		fputs("ps->ps_top", cfile);
		for (depth = tn->tn_depth; depth > 0; --depth)
			fputs("->psf_down", cfile);
		break;

	  case opProtoId:
		fputs("psf_proto->pr_id", cfile);
		break;

	  case opSlinkRef:
		fputs(tn->tn_inner->s_name, cfile);
		for (depth = tn->tn_depth; depth > 0; --depth)
			fputs("->_sl", cfile);
		break;

	  case opBitSeek:
		assert(tn->tn_val > 0);
		fprintf(cfile, "ds_seekbit(ds, %d, DS_RELATIVE)", tn->tn_val);
		break;

	  case opBitRewind:
		fputs("ds_seekbit(ds, tell, DS_RELATIVE),", cfile);
		generateCexpr(tn->tn_kid);
		break;

	  default:
		assert(tn->tn_arity);
		switch (tn->tn_arity) {
		  case 2:
			kid = tn->tn_kid2;
			parens = (kid->tn_op < op);
			if (parens)
				putc('(', cfile);
			generateCexpr(kid);
			if (parens)
				putc(')', cfile);
			/* FALL THROUGH */
		  case 1:
			fputs(opnames[op], cfile);
			kid = tn->tn_kid1;
			parens = (kid->tn_op < op);
			if (parens)
				putc('(', cfile);
			generateCexpr(kid);
			if (parens)
				putc(')', cfile);
		}
	}
}

static void	fetchfield(TreeNode *);

static int
validate(TreeNode *tn, void *failcode)
{
	Symbol *sym;
	unsigned short flags;
	TreeNode *decl;

	if (tn->tn_op != opName)
		return 1;
	sym = tn->tn_sym;
	switch (sym->s_type) {
	  case stProtocol:
	  case stStruct:
		if (tn->tn_guard) {
			indent(level);
			generateCexpr(tn->tn_guard);
			fputs(";\n", cfile);
		}
		return 1;

	  case stField:
		flags = tn->tn_flags;
		decl = sym->s_tree;
		if (flags & tfForwardRef) {
			indent(level);
			fputs("{ int tell = ds_tellbit(ds);\n", cfile);
			if (tn->tn_guard) {
				indent(level);
				fputs("if (!(", cfile);
				generateCexpr(tn->tn_guard);
				fputs("))\n", cfile);
				indent(level+1);
				fprintf(cfile, "%s;\n", (char *)failcode);
			}
			indent(level);
			cg->cg_flags |= cgfLookahead;
			fetchfield(decl);
			cg->cg_flags &= ~cgfLookahead;
			indent(level);
			fputs("ds_seekbit(ds, tell, DS_ABSOLUTE); }\n", cfile);
		} else if ((flags & (tfLocalRef|tfBackwardRef)) == tfBackwardRef
			   && sym->s_tree->tn_guard) {
			fprintf(cfile, "/* XXX guarded backref: %s */\n",
				sym->s_name);
		}
	}
	return 0;
}

static void
generateCstatement(TreeNode *tn, void (*fun)(TreeNode *), void *failcode)
{
	TreeNode *kid, *type, *label;
	char *name;
	typeword_t tw;

	switch (tn->tn_op) {
	  case opExport:
	  case opStruct:
	  case opMacro:
	  case opEnum:
	  case opEnumerator:	/* nothing to do */
		break;

	  case opList:
		for (kid = tn->tn_left; kid; kid = kid->tn_next)
			generateCstatement(kid, fun, failcode);
		break;

	  case opNest:
		name = tn->tn_sym->s_name;
		for (kid = first(tn->tn_kid); kid; kid = kid->tn_next) {
			walktree(kid, validate, failcode);
			indent(level);
			fprintf(cfile, "{ extern Protocol %s_proto;\n", name);
			indent(level);
			fprintf(cfile,
				"  %s_proto.pr_flags |= PR_EMBEDCACHED;\n",
				name);
			indent(level);
			fprintf(cfile, "  pr_nestqual(&%s_proto, PRID_%s,\n",
				name, uppercase(kid->tn_sym->s_name));
			indent(level);
			fputs("\t      ", cfile);
			type = kid->tn_kid;
			if (type->tn_op == opQualify) {
				generateCexpr(type->tn_left);
				fputs(", ", cfile);
				generateCexpr(type->tn_right);
			} else {
				generateCexpr(type);
				fputs(", 0", cfile);
			}
			fputs(", 0, 0);\n", cfile);
			indent(level);
			fprintf(cfile,
				"  %s_proto.pr_flags &= ~PR_EMBEDCACHED; }\n",
				name);
		}
		break;

	  case opDeclaration:
		assert(tn->tn_sym);
		if (tn->tn_sym->s_type != stField)
			break;
		tw = tn->tn_type;	/* XXX more systematic? */
		if (tw.tw_base == btVoid || tw.tw_base == btCache)
			break;
		walktree(tn, validate, failcode);
		indent(level);
		(*fun)(tn);
		break;

	  case opIfElse:
		kid = tn->tn_kid3;
		walktree(kid, validate, failcode);
		indent(level);
		fputs("if (", cfile);
		generateCexpr(kid);
		fputs(") {\n", cfile);
		level++;
		generateCstatement(tn->tn_kid2, fun, failcode);
		indent(--level);
		putc('}', cfile);
		kid = tn->tn_kid1;
		if (kid == 0)
			putc('\n', cfile);
		else {
			fputs(" else", cfile);
			if (kid->tn_op != opIfElse) {
				fputs(" {", cfile);
				level++;
			}
			putc('\n', cfile);
			generateCstatement(kid, fun, failcode);
			if (kid->tn_op != opIfElse) {
				indent(--level);
				fputs("}\n", cfile);
			}
		}
		break;

	  case opSwitch:
		kid = tn->tn_left;
		walktree(kid, validate, failcode);
		indent(level);
		fputs("switch (", cfile);
		generateCexpr(kid);
		fputs(") {\n", cfile);
		for (kid = tn->tn_right->tn_left; kid; kid = kid->tn_next) {
			label = first(kid->tn_left);
			do {
				indent(level);
				if (label->tn_op == opDefault)
					fputs("  default", cfile);
				else {
					fputs("  case ", cfile);
					generateCexpr(label);
				}
				fputs(":\n", cfile);
			} while ((label = label->tn_next) != 0);
			level++;
			generateCstatement(kid->tn_right, fun, failcode);
			indent(level);
			fputs("break;\n", cfile);
			--level;
		}
		indent(level);
		fputs("}\n", cfile);
		break;

	  default:
		walktree(tn, validate, failcode);
		indent(level);
		generateCexpr(tn);
		fputs(";\n", cfile);
	}
}

static void
generateCprologue(Symbol *sym, enum entrypoint ep)
{
	Scope *s;
	char *name;
	Symbol *ssym;

	s = &SN(sym->s_tree)->sn_scope;
	beginfunction(ep, s->s_path);
	name = sym->s_name;
	fprintf(cfile, "\tstruct %s *%s = ", s->s_path, name);
	switch (sym->s_type) {
	  case stProtocol:
		fprintf(cfile, "&%s_frame;\n", name);
		break;
	  case stStruct:
		fputs("ps->ps_slink;\n", cfile);
	}
	for (ssym = s->s_protocols.sl_head; ssym; ssym = ssym->s_next) {
		name = ssym->s_name;
		fprintf(cfile, "\tstruct %s *%s;\n", name, name);
	}
	for (ssym = s->s_structs.sl_head; ssym; ssym = ssym->s_next) {
		if (ssym->s_flags & sfTemporary) {
			fprintf(cfile, "\tstruct %s %s%s;\n",
				SN(ssym->s_tree)->sn_scope.s_path,
				(ssym->s_flags & sfDirect) ? "" : "*",
				ssym->s_name);
		}
	}
}

static void
matchprotocol(char *pname, TreeNode *prot)
{
	TreeNode *formals, *formal;

	formals = prot->tn_kid3->tn_left;
	if (formals == 0) {
		indent(level);
		fputs("pr = 0;\n", cfile);
		return;
	}
	for (formal = first(formals); formal; formal = formal->tn_next) {
		walktree(formal, validate, "/* XXXgoto skip */;");
		indent(level);
		fprintf(cfile, "pr = pin_match(%s_index, ", pname);
		if (formal->tn_op == opQualify) {
			generateCexpr(formal->tn_left);
			fputs(", ", cfile);
			generateCexpr(formal->tn_right);
		} else {
			generateCexpr(formal);
			fputs(", 0", cfile);
		}
		fputs(");\n", cfile);
		if (formal->tn_next) {
			indent(level);
			fputs("if (pr == 0) {\n", cfile);
			level++;
		}
	}
	while (level > 1) {
		indent(--level);
		fputs("}\n", cfile);
	}
}

static void
generatefetch(Symbol *sym, TreeNode *body, TreeNode *foot)
{
	Scope *s;
	Symbol *structs, *tsym, *next;
	int alltemp;

	s = &SN(sym->s_tree)->sn_scope;
	structs = s->s_structs.sl_head;
	alltemp = 1;
	for (tsym = structs; tsym; tsym = tsym->s_next) {
		if (tsym->s_flags & sfTemporary)
			continue;
		alltemp = 0;
		generatefetch(tsym, tsym->s_tree->tn_kid, 0);
	}
	generateCprologue(sym, Fetch);
	fputs("\tint ok = 0, fid = pf->pf_id;\n\n", cfile);
	if (sym->s_type == stProtocol) {
		fprintf(cfile, "\tPS_PUSH(ps, &%s->_psf, &%s_proto);\n",
			sym->s_name, sym->s_name);
	}
	cg->cg_flags = cgfFetch;
	fetchelements(s, fetchfield);
	if (structs && !alltemp) {
		tsym = structs;
		fprintf(cfile, "\tif (fid >= %u) {\n", tsym->s_fid);
		do {
			next = tsym->s_next;
			if (tsym->s_flags & sfTemporary)
				continue;
			if (next) {
				fprintf(cfile, "\t\tif (fid < %u) {\n\t",
					next->s_fid);
			}
			fprintf(cfile,
			    "\t\tok = %s_fetch(pf, ds, ps, rex); goto out;\n",
				SN(tsym->s_tree)->sn_scope.s_path);
			if (next)
				fputs("\t\t}\n", cfile);
		} while ((tsym = next) != 0);
		fputs("\t}\n", cfile);
	}
	generateCstatement(body, fetchfield, "goto out");
	fputs("out:\n", cfile);
	if (sym->s_type == stProtocol)
		fputs("\tPS_POP(ps);\n", cfile);
	fputs("\treturn ok;\n", cfile);
	endfunction(Fetch, s->s_path);
}

static void
fetchelements(Scope *s, void (*fun)(TreeNode *))
{
	Symbol *esym, *structs;
	int many;
	TreeNode elem;

	esym = s->s_elements.sl_head;
	if (esym == 0)
		return;
	structs = s->s_structs.sl_head;
	many = (esym->s_next != 0);
	if (many) {
		fprintf(cfile, "\tif (fid >= %u", esym->s_eid);
		if (structs)
			fprintf(cfile, " && fid < %u", structs->s_eid);
		fputs(") {\n\t", cfile);
		level++;
	}
	fputs("\tswitch (fid) {\n", cfile);
	elem.tn_op = opDeclaration;
	elem.tn_flags = 0;
	elem.tn_arity = 2;
	elem.tn_right = 0;
	do {
		elem.tn_type = esym->s_etype;
		elem.tn_width = esym->s_ewidth;
		elem.tn_suffix = esym->s_tree->tn_suffix;
		elem.tn_sym = esym;
		elem.tn_left = esym->s_dimlen;
		indent(level);
		(*fun)(&elem);
	} while ((esym = esym->s_next) != 0);
	if (many)
		fputs("\t\t}\n", cfile);
	fputs("\t}\n", cfile);
	level = 1;
}

static void
fetchfield(TreeNode *decl)
{
	Symbol *sym;
	int element, cc;
	TreeNode *len, *next;
	typeword_t tw;

	sym = decl->tn_sym;
	element = (sym->s_type == stElement);
	cc = nsprintf(cg->cg_member, CG_NAMESIZE, "%s->%s%s",
		      sym->s_scope->s_sym->s_name,
		      (element ? sym->s_tree->tn_sym : sym)->s_name,
		      decl->tn_suffix ? decl->tn_suffix : "");
	if (element) {
		for (len = sym->s_tree->tn_left->tn_left;
		     (next = len->tn_next) != sym->s_dimlen;
		     len = next) {
			cc += nsprintf(&cg->cg_member[cc], CG_NAMESIZE - cc,
				       "[%s%s_element.pf_cookie]",
				       sym->s_scope->s_path,
				       len->tn_elemsym->s_name + 1);
		}
		(void) strcpy(cg->cg_cookie, cg->cg_member);
		cc += nsprintf(&cg->cg_member[cc], CG_NAMESIZE - cc,
			       "[pf->pf_cookie]");
	}

	tw = decl->tn_type;
	switch (qualifier(tw)) {
	  case tqArray:
	  case tqVarArray:
		(*cg->cg_fetcharray)(cg, decl);
		break;
	  default:
		(*cg->cg_fetch[tw.tw_base])(cg, decl);
	}
}

static void	decodefield(TreeNode *);

static void
generatedecode(Symbol *sym, TreeNode *body, TreeNode *foot)
{
	Scope *s;
	Symbol *ssym;
	int protocol;

	s = &SN(sym->s_tree)->sn_scope;
	for (ssym = s->s_structs.sl_head; ssym; ssym = ssym->s_next) {
		if (!(ssym->s_flags & sfTemporary))
			generatedecode(ssym, ssym->s_tree->tn_kid, 0);
	}
	generateCprologue(sym, Decode);
	protocol = (sym->s_type == stProtocol);
	if (protocol)
		fputs("\tProtocol *pr;\n", cfile);
	if (sym->s_flags & sfBitFields)
		fputs("\tlong val;\n", cfile);
	putc('\n', cfile);
	cg->cg_flags = cgfDecode;
	if (protocol) {
		fprintf(cfile, "\tPS_PUSH(ps, &%s->_psf, &%s_proto);\n",
			sym->s_name, sym->s_name);
		generateCstatement(body, decodefield, "goto out");
		matchprotocol(sym->s_name, sym->s_tree);
		fputs("\tpv_decodeframe(pv, pr, ds, ps);\n", cfile);
		fputs("out:\n\tPS_POP(ps);\n", cfile);
	} else {
		fputs("\tpv_push(pv, ps->ps_top->psf_proto,\n", cfile);
		fprintf(cfile, "\t\t%s_struct.pst_parent->pf_name,\n",
			s->s_path);
		fprintf(cfile, "\t\t%s_struct.pst_parent->pf_namlen,\n",
			s->s_path);
		fprintf(cfile, "\t\t%s_struct.pst_parent->pf_title);\n",
			s->s_path);
		generateCstatement(body, decodefield, "goto out");
		fputs("out:\tpv_pop(pv);\n", cfile);
	}
	endfunction(Decode, s->s_path);
}

static void
decodefield(TreeNode *decl)
{
	Symbol *sym;
	typeword_t tw;
	char *path;

	sym = decl->tn_sym;
	(void) nsprintf(cg->cg_member, CG_NAMESIZE, "%s->%s%s",
			sym->s_scope->s_sym->s_name, sym->s_name,
			decl->tn_suffix ? decl->tn_suffix : "");

	tw = decl->tn_type;
	path = sym->s_scope->s_path;
	switch (qualifier(tw)) {
	  case tqArray:
	  case tqVarArray:
		(void) nsprintf(cg->cg_cookie, CG_NAMESIZE, "%s%s_element",
			        path, 
				decl->tn_left->tn_right->tn_elemsym->s_name+1);
		(*cg->cg_decodearray)(cg, decl);
		break;
	  default:
		(void) nsprintf(cg->cg_cookie, CG_NAMESIZE, "%s_fields[%u]",
			        path, sym->s_index);
		(*cg->cg_decode[tw.tw_base])(cg, decl);
	}
}

/*
 * Return an uppercase version of s.
 */
static char *
uppercase(char *s)
{
	int n;
	char *t, c;
	static char u[64];

	n = sizeof u;
	for (t = u; (c = *s) != '\0'; s++, t++) {
		if (--n == 0)
			break;
#ifdef sun
		*t = islower(c) ? toupper(c) : c;
#else
		*t = islower(c) ? _toupper(c) : c;
#endif
	}
	*t = '\0';
	return u;
}

static void
passthru(int line, char *name)
{
	FILE *pf, *cf;
	char *rw, buf[BUFSIZ];

	pf = ptfile, cf = cfile;
	(void) fseek(pf, 0L, SEEK_SET);
	if (ferror(pf)) {
		rw = "write";
		goto fail;
	}
	while (fgets(buf, sizeof buf, pf)) {
		fputs(buf, cf);
		line++;
	}
	if (ferror(pf)) {
		rw = "read";
		goto fail;
	}
	fprintf(cf, "#line %d \"%s\"\n", line + 1, name);
	return;
fail:
	exc_perror(errno, "can't %s %s", rw, ptname);
	status++;
}
