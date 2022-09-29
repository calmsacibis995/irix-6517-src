#ident "$Revision: 1.9 $"

#include <sys/param.h>
#include <assert.h>
#include <stdio.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_alloc_btree.h>
#include "sim.h"
#include "addr.h"
#include "command.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "io.h"
#include "flist.h"
#include "inode.h"
#include "output.h"

static int addr_f(int argc, char **argv);
static void addr_help(void);

static const cmdinfo_t addr_cmd =
	{ "addr", "a", addr_f, 0, 1, 1, "[field-expression]",
	  "set current address", addr_help };

static void
addr_help(void)
{
	dbprintf(
"\n"
" 'addr' uses the given field to set the filesystem address and type\n"
"\n"
" Examples:\n"
"\n"
" sb\n"
" a rootino - set the type to inode and set position to the root inode\n"
" a u.bmx[0].startblock (for inode with blockmap)\n"
"\n"
);

}

static int
addr_f(
	int		argc,
	char		**argv)
{
	adfnc_t		adf;
	const ftattr_t	*fa;
	flist_t		*fl;
	const field_t	*fld;
	typnm_t		next;
	flist_t		*tfl;

	if (argc == 0) {
		print_iocur("current", iocur_top);
		return 0;
	}
	if (cur_typ == NULL) {
		dbprintf("no current type\n");
		return 0;
	}
	fld = cur_typ->fields;
	if (fld != NULL && fld->name[0] == '\0') {
		fa = &ftattrtab[fld->ftyp];
		assert(fa->ftyp == fld->ftyp);
		fld = fa->subfld;
	}
	if (fld == NULL) {
		dbprintf("no fields for type %s\n", cur_typ->name);
		return 0;
	}
	fl = flist_scan(argv[0]);
	if (fl == NULL)
		return 0;
	if (!flist_parse(fld, fl, iocur_top->data, 0)) {
		flist_free(fl);
		return 0;
	}
	flist_print(fl);
	for (tfl = fl; tfl->child != NULL; tfl = tfl->child) {
		if ((tfl->flags & FL_OKLOW) && tfl->low < tfl->high) {
			dbprintf("array not allowed for addr command\n");
			flist_free(fl);
			return 0;
		}
	}
	fld = tfl->fld;
	next = fld->next;
	if (next == TYP_INODATA)
		next = inode_next_type();
	if (next == TYP_NONE) {
		dbprintf("no next type for field %s\n", fld->name);
		return 0;
	}
	fa = &ftattrtab[fld->ftyp];
	assert(fa->ftyp == fld->ftyp);
	adf = fa->adfunc;
	if (adf == NULL) {
		dbprintf("no addr function for field %s (type %s)\n",
			fld->name, fa->name);
		return 0;
	}
	(*adf)(iocur_top->data, tfl->offset, next);
	flist_free(fl);
	return 0;
}

void
addr_init(void)
{
	add_command(&addr_cmd);
}
