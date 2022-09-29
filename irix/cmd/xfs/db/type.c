#ident "$Revision: 1.15 $"

#include "versions.h"
#include <sys/param.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_alloc_btree.h>
#include "sim.h"
#include "agf.h"
#include "agfl.h"
#include "agi.h"
#include "block.h"
#include "command.h"
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "print.h"
#include "sb.h"
#include "inode.h"
#include "bnobt.h"
#include "cntbt.h"
#include "inobt.h"
#include "bmapbt.h"
#include "bmroot.h"
#include "agf.h"
#include "agfl.h"
#include "agi.h"
#include "dir.h"
#include "dirshort.h"
#include "io.h"
#include "output.h"
#include "write.h"
#if VERS >= V_62
#include "attr.h"
#include "dquot.h"
#if VERS >= V_654
#include "dir2.h"
#endif
#endif

static const typ_t	*findtyp(char *name);
static int		type_f(int argc, char **argv);

const typ_t	*cur_typ;

static const cmdinfo_t	type_cmd =
	{ "type", NULL, type_f, 0, 1, 1, "[newtype]",
	  "set/show current data type", NULL };

const typ_t	typtab[] = {
	{ TYP_AGF, "agf", handle_struct, agf_hfld },
	{ TYP_AGFL, "agfl", handle_struct, agfl_hfld },
	{ TYP_AGI, "agi", handle_struct, agi_hfld },
#if VERS >= V_62
	{ TYP_ATTR, "attr", handle_struct, attr_hfld },
	{ TYP_BMAPBTA, "bmapbta", handle_struct, bmapbta_hfld },
#else
	{ TYP_ATTR, "attr", NULL, NULL },
	{ TYP_BMAPBTA, "bmapbta", NULL, NULL },
#endif
	{ TYP_BMAPBTD, "bmapbtd", handle_struct, bmapbtd_hfld },
	{ TYP_BNOBT, "bnobt", handle_struct, bnobt_hfld },
	{ TYP_CNTBT, "cntbt", handle_struct, cntbt_hfld },
	{ TYP_DATA, "data", handle_block, NULL },
	{ TYP_DIR, "dir", handle_struct, dir_hfld },
#if VERS >= V_654
	{ TYP_DIR2, "dir2", handle_struct, dir2_hfld },
#else
	{ TYP_DIR2, "dir2", NULL, NULL },
#endif
#if VERS >= V_62
	{ TYP_DQBLK, "dqblk", handle_struct, dqblk_hfld },
#else
	{ TYP_DQBLK, "dqblk", NULL, NULL },
#endif
	{ TYP_INOBT, "inobt", handle_struct, inobt_hfld },
	{ TYP_INODATA, "inodata", NULL, NULL },
	{ TYP_INODE, "inode", handle_struct, inode_hfld },
	{ TYP_LOG, "log", NULL, NULL },
	{ TYP_RTBITMAP, "rtbitmap", NULL, NULL },
	{ TYP_RTSUMMARY, "rtsummary", NULL, NULL },
	{ TYP_SB, "sb", handle_struct, sb_hfld },
	{ TYP_SYMLINK, "symlink", handle_string, NULL },
	{ TYP_NONE, NULL }
};

static const typ_t *
findtyp(
	char		*name)
{
	const typ_t	*tt;

	for (tt = typtab; tt->name != NULL; tt++) {
		assert(tt->typnm == (typnm_t)(tt - typtab));
		if (strcmp(tt->name, name) == 0)
			return tt;
	}
	return NULL;
}

static int
type_f(
	int		argc,
	char		**argv)
{
	const typ_t	*tt;
	int count = 0;

	if (argc == 0) {
		if (cur_typ == NULL)
			dbprintf("no current type\n");
		else
			dbprintf("current type is \"%s\"\n", cur_typ->name);

		dbprintf("\n supported types are:\n ");
		for (tt = typtab, count = 0; tt->name != NULL; tt++) {
			if ((tt+1)->name != NULL) {
				dbprintf("%s, ", tt->name);
				if ((++count % 8) == 0)
					dbprintf("\n ");
			} else {
				dbprintf("%s\n", tt->name);
			}
		}

		
	} else {
		tt = findtyp(argv[0]);
		if (tt == NULL)
			dbprintf("no such type %s\n", argv[0]);
		else
			iocur_top->typ = cur_typ = tt;
	}
	return 0;
}

void
type_init(void)
{
	add_command(&type_cmd);
}

/* read/write selectors for each major data type */

void
handle_struct(
	int           action,
	const field_t *fields,
	int           argc,
	char          **argv)
{
	if (action == DB_WRITE)
		write_struct(fields, argc, argv);
	else
		print_struct(fields, argc, argv);
}

void
handle_string(
	int           action,
	const field_t *fields,
	int           argc,
	char          **argv)
{
	if (action == DB_WRITE)
		write_string(fields, argc, argv);
	else
		print_string(fields, argc, argv);
}

void
handle_block(
	int           action,
	const field_t *fields,
	int           argc,
	char          **argv)
{
	if (action == DB_WRITE)
		write_block(fields, argc, argv);
	else
		print_block(fields, argc, argv);
}
