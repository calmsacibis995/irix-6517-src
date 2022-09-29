/*
 * For each file (inode) we keep a "file environment" or fenv,
 * of string values of properties that are associated with that
 * inode.  These are values that are more expensive to recompute
 * but only need to be recomputed if the file changes and are not
 * needed in the basic determination of whether the file changed.
 *
 * The entire set of fenv values for a given inode is kept in
 * the hp_fenv heap as a single FV_END terminated string, where
 * each individual value is prefixed with a single byte type
 * from the set of values in the fenv_t enumeration, and is
 * nul-terminated.
 *
 * Whenever fsdump notices that a file has changed, it
 * zeros out the entire fenv for that file.  When fsdump
 * has time left in its quantum after doing all but writing
 * the file back out, it spends this spare time walking
 * its internal tree, looking for nul fenv strings to update.
 *
 * The routines putfenv and getfenv, in lib/fenv.c, manipulate
 * these fenv strings.
 */

typedef enum {
	FV_END = 1,
	FV_RCSREV,	/* For RCS *,v files - top-of-trunk rev number */
	FV_RCSLOCK,	/* For RCS *,v files - list of locker ids and revs */
	FV_RCSDATE,	/* For RCS *,v files - top-of-trunk check in date */
	FV_RCSAUTH,	/* For RCS *,v files - top-of-trunk author id */
	FV_QSUM,	/* For all regular files - quick check sum of file contents */
	FV_QFLAG,	/* For all regular files - qsumflag: see fsdump/qsumfenv.c */
	FV_SYMLINK,	/* For all symlink files: contents of link file */
	FV_MAXVAL=30	/* Leave last in fenv_t enum */
} fenv_t;

#define FV_MINVAL FV_END		/* lower bound on FV values */

char *getfenv (const inod_t *, fenv_t);
void putfenv (inod_t *, fenv_t, char *);
void unsetfenv (inod_t *, fenv_t);

#define BLOCKSIZE	512			/* default logical block size */
