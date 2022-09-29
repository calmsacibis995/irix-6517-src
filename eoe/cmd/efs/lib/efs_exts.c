#ident "$Revision: 1.5 $"

/*
 * Get in the extents for a file into memory.
 */
#include "efs.h"

/*
 * Get memory to hold all the extents this file has.  Caller is responsible
 * for freeing the memory allocated.
 */
extent *
efs_getextents(struct efs_dinode *di)
{
	struct extent *exp;
	struct extent *exbuf;
	struct extent *ex;
	int i, j, left;
	int amount;
	char buf[BBSIZE];

	ex = (extent *) malloc(sizeof(extent) * di->di_numextents);
	if (ex == NULL) {
		fprintf(stderr, "%s: out of memory reading in extents\n",
				progname);
		exit(1);
	}
	if (di->di_numextents <= EFS_DIRECTEXTENTS) {
		memcpy(ex, di->di_u.di_extents,
			   di->di_numextents * sizeof(extent));
		return (ex);
	}
	/*
	 * Read in indirect extents
	 */
	exbuf = ex;
	exp = &di->di_u.di_extents[0];
	left = di->di_numextents;
	for (i = 0; i < di->di_u.di_extents[0].ex_offset; i++, exp++) {
		for (j = 0; j < exp->ex_length; j++) {
			lseek(fs_fd, (long) BBTOB(exp->ex_bn + j), SEEK_SET);
			if (read(fs_fd, buf, BBSIZE) != BBSIZE)
				error();
			amount = BBSIZE / sizeof(extent);
			if (amount > left)
				amount = left;
			memcpy(exbuf, buf, amount * sizeof(extent));
			left -= amount;
			if (left == 0)
				return ex;
			exbuf += amount;
		}
	}
	return (ex);
}
