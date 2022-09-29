/*
 * Pthread internal information.
 */

#include "spyIO.h"

#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>


struct descriptor {
	char*	fname;
	int	foff;
};

#define typeBegin(typ)	\
	struct descriptor TNAME(typ) [] = { { #typ, sizeof(typ) },

#define field(typ, fld)	\
	{ "_" #fld, offsetof(typ, fld) }

#define typeEnd()	\
	{ 0, 0 } };


char*	ABI_Names[] = { "O32", "N32", "N64" };
#define TNAME(typ)	typ ## Info
#define TLABEL(typ)	#typ "Info"

/* ------------------------------------------------------------------
 * Interesting types:
 *
 * Add a type definition and a dump call to main.
 *
 * The following are generated:
 *	definition of per abi, per type offset data structures
 *	definition of per type array of abi definitions
 *	declarations of the above types
 */

#include <pt/common.h>
#include <pt/ptdbg.h>
#include <pt/pt.h>
#include <pt/vp.h>
#include <pt/mtx.h>
#include <pt/mtxattr.h>

typeBegin(pt_lib_info_t)
	field(pt_lib_info_t, ptl_info_version),
	field(pt_lib_info_t, ptl_pt_tbl),
	field(pt_lib_info_t, ptl_pt_count),
typeEnd()

typeBegin(pt_t)
	field(pt_t, pt_vp),
	field(pt_t, pt_label),
	field(pt_t, pt_state),
	field(pt_t, pt_bits),
	field(pt_t, pt_blocked),
	field(pt_t, pt_occupied),
	field(pt_t, pt_stk),
	field(pt_t, pt_sync),
	field(pt_t, pt_context),
typeEnd()

typeBegin(vp_t)
	field(vp_t, vp_state),
	field(vp_t, vp_pid),
	field(vp_t, vp_pt),
typeEnd()

typeBegin(mtx_t)
	field(mtx_t, mtx_attr),
	field(mtx_t, mtx_owner),
	field(mtx_t, mtx_waitq),
	field(mtx_t, mtx_thread),
	field(mtx_t, mtx_pid),
	field(mtx_t, mtx_waiters),
typeEnd()

typeBegin(mtxattr_t)
	field(mtxattr_t, ma_protocol),
	field(mtxattr_t, ma_priority),
	field(mtxattr_t, ma_type),
typeEnd()

typedef struct prda_lib prda_lib_t;
typeBegin(prda_lib_t)
	field(prda_lib_t, pthread_data),
typeEnd()

typedef struct prda_sys prda_sys_t;
typeBegin(prda_sys_t)
	field(prda_sys_t, t_rpid),
typeEnd()

/* ------------------------------------------------------------------ */

static void dump(int, struct descriptor*, char*);
static void decls(struct descriptor*, char*);
static void defs(char*);
static void details(struct descriptor*, char*);


int
main(int argc, char *argv[])
{

#define ARGS	"hc"
#define USAGE	"Usage: %s [-hc][-u]\n"

#define DECLS	'h'
#define DEFS	'c'
#define ABIS	0

	int	c, errflg = 0;
	int	opt = ABIS;

	optarg = NULL;
	while (!errflg && (c = getopt(argc, argv, ARGS)) != EOF)
		switch (c) {
		case 'h'	:
			opt = 'h';
			break;
		case 'c'	:
			opt = 'c';
			break;
		case 'u'	:
			printf(USAGE, argv[0]);
			printf(
				"\th\tdump declarations\n"
				"\tc\tdump definitions\n"
				"\tu\tusage message\n"
			);
			exit(0);
			break;
		default :
			errflg++;
		}
	if (errflg || optind < argc) {
		fprintf(stderr, USAGE, argv[0]);
		exit(0);
	}

	if (opt == DECLS) {
		printf("#ifndef ABIDEFS\n");
		printf("#define ABIDEFS\n");
	} else if (opt == ABIS) {
		printf("#ifndef ABIDEFS_%s\n", ABI_Names[IONATIVE]);
		printf("#define ABIDEFS_%s\n", ABI_Names[IONATIVE]);
	}

	dump(opt, TNAME(pt_lib_info_t), TLABEL(pt_lib_info_t));

	dump(opt, TNAME(pt_t), TLABEL(pt_t));

	dump(opt, TNAME(vp_t), TLABEL(vp_t));

	dump(opt, TNAME(mtx_t), TLABEL(mtx_t));
	dump(opt, TNAME(mtxattr_t), TLABEL(mtxattr_t));

	dump(opt, TNAME(prda_lib_t), TLABEL(prda_lib_t));

	dump(opt, TNAME(prda_sys_t), TLABEL(prda_sys_t));

	if (opt == DECLS || opt == ABIS) {
		printf("\n#endif\n");
	}

	return (0);
}

static void
dump(int opt, struct descriptor* desc, char* label)
{
	switch (opt) {
	case DECLS	:
		decls(desc, label);
		break;
	case DEFS	:
		defs(label);
		break;
	default	:
		details(desc, label);
	}
}

static void
decls(struct descriptor* desc, char* label)
{
	struct descriptor*	d;

	/* ABI neutral declaration.
	 * For type <foo>, fields <fld>
	 *	struct foo {
	 *		int	foo_size;
	 *		int	fld;
	 *		...
	 *	};
	 *	extern struct foo *foo[];
	 */

	printf("struct %s {\n", label);

	printf("\tint\t%s_size;\n", desc[0].fname);
	for (d = desc + 1; d->fname; d++) {
		printf("\tint\t%s;\n", d->fname);
	}
	printf("};\n");

	printf("extern struct %s *%s[];\n\n", label, label);
}

static void
defs(char* label)
{
	char**			s;

	/* All ABI definition.
	 * For type <foo>, fields <fld>, for all ABIs.
	 *	struct foo *foo[] = { &fooABI0, &fooABI1, ...};
	 */

	printf("struct %s *%s[] =\n\t{ ", label, label);
	for (s = ABI_Names;
	     s < ABI_Names + sizeof(ABI_Names)/sizeof(char*); s++) {
		printf("&%s%s, ", label, *s);
	}
	printf("};\n");
}

static void
details(struct descriptor* desc, char* label)
{
	/* ABI specific data.
	 * For type <foo>, fields <fld>, for the compiled ABI:
	 *	struct foo fooABI = {
	 *		<sizeof(foo)>, <offsetof(foo, fld)>, ...
	 *	};
	 */

	struct descriptor*	d;

	printf("struct %s %s%s = {\n",
		label, label, ABI_Names[IONATIVE]);
	for (d = desc; d->fname; d++) {
		printf("\t%d, ", d->foff);
	}
	printf("};\n\n");
}
