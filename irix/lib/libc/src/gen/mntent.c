#if !defined(lint) && defined(SCCSIDS)
static	char sccsid[] = "@(#)mntent.c	1.2 90/07/20 4.1NFSSRC; from 1.14 88/02/08 SMI";
#endif

#ifdef __STDC__
	#pragma weak setmntent = _setmntent
	#pragma weak getmntent = _getmntent
	#pragma weak addmntent = _addmntent
	#pragma weak endmntent = _endmntent
	#pragma weak hasmntopt = _hasmntopt
	#pragma weak getmntany = _getmntany
	#pragma weak delmntent = _delmntent
#endif
#include "synonyms.h"
#include <stdio.h>
#include <ctype.h>
#include <mntent.h>
#include <sys/file.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This structure is used to build a list of mntent structures
 * in reverse order from /etc/mtab.
 */
struct mntlist {
	struct mntent *mntl_mnt;
	struct mntlist *mntl_next;
};

static char *mntstr(char **);
static int mntdigit(char **);
static int mnttabscan(FILE *, struct mntent *);
static char *mntopt(char **);
static int mntprtent(FILE *, const struct mntent *);
static int copymntent(struct mntent *, struct mntent *);
static void freemntlist(struct mntlist *);

static char *
mntstr(char **p)
{
	char *cp = *p;
	char *retstr;

	while (*cp && isspace(*cp))
		cp++;
	retstr = cp;
	while (*cp && !isspace(*cp))
		cp++;
	if (*cp) {
		*cp = '\0';
		cp++;
	}
	*p = cp;
	return (retstr);
}

static int
mntdigit(char **p)
{
	int value = 0;
	char *cp = *p;

	while (*cp && isspace(*cp))
		cp++;
	for (; *cp && isdigit(*cp); cp++) {
		value *= 10;
		value += *cp - '0';
	}
	while (*cp && !isspace(*cp))
		cp++;
	if (*cp) {
		*cp = '\0';
		cp++;
	}
	*p = cp;
	return (value);
}

/* move out of function scope so we get a global symbol for use with data cording */
static char *line _INITBSS;

static int
mnttabscan(FILE *mnttabp, struct mntent *mnt)
{
	char *cp;

	if ((line == NULL) && (line = (char *)calloc(1, MNTMAXSTR*2)) == NULL)
		return (0);
	do {
		cp = fgets(line, MNTMAXSTR * 2, mnttabp);
		if (cp == NULL) {
			return (0);
		}
	} while (*cp == '#' || *cp == '\n');
	mnt->mnt_fsname = mntstr(&cp);
	if (*cp == '\0')
		return (1);
	mnt->mnt_dir = mntstr(&cp);
	if (*cp == '\0')
		return (2);
	mnt->mnt_type = mntstr(&cp);
	if (*cp == '\0')
		return (3);
	mnt->mnt_opts = mntstr(&cp);
	if (*cp == '\0')
		return (4);
	mnt->mnt_freq = mntdigit(&cp);
	if (*cp == '\0')
		return (5);
	mnt->mnt_passno = mntdigit(&cp);
	return (6);
}
	
FILE *
setmntent(const char *fname, const char *flag)
{
	FILE *mnttabp;

	if ((mnttabp = fopen(fname, flag)) == NULL) {
		return (NULL);
	}
	for (; *flag ; flag++) {
		if (*flag == 'w' || *flag == 'a' || *flag == '+') {
			if (flock(fileno(mnttabp), LOCK_EX) < 0) {
				fclose(mnttabp);
				return (NULL);
			}
			break;
		}
	}
	return (mnttabp);
}

int
endmntent(FILE *mnttabp)
{

	if (mnttabp) {
		fclose(mnttabp);
	}
	return (1);
}

/* move out of function scope so we get a global symbol for use with data cording */
static struct mntent gmnt _INITBSSS;

struct mntent *
getmntent(FILE *mnttabp)
{
	int nfields;

	if (mnttabp == 0)
		return ((struct mntent *)0);
	nfields = mnttabscan(mnttabp, &gmnt);
	if (nfields < 4)
		return ((struct mntent *)0);
	return (&gmnt);
}

int
addmntent(FILE *mnttabp, const struct mntent *mnt)
{
	if (fseek(mnttabp, 0L, 2) < 0)
		return (1);
	if (mnt == (struct mntent *)0)
		return (1);
	if (mnt->mnt_fsname == NULL || mnt->mnt_dir  == NULL ||
	    mnt->mnt_type   == NULL || mnt->mnt_opts == NULL)
		return (1);

	mntprtent(mnttabp, mnt);
	return (0);
}

static char *
mntopt(char **p)
{
	char *cp = *p;
	char *retstr;

	while (*cp && isspace(*cp))
		cp++;
	retstr = cp;
	while (*cp && *cp != ',')
		cp++;
	if (*cp) {
		*cp = '\0';
		cp++;
	}
	*p = cp;
	return (retstr);
}

char *
hasmntopt(const struct mntent *mnt, const char *opt)
{
	char *f, *opts;
	char *tmpopts = NULL;

	if ((tmpopts = (char *)calloc(1, 256)) == NULL)
		return (NULL);
	strcpy(tmpopts, mnt->mnt_opts);
	opts = tmpopts;
	f = mntopt(&opts);
	for (; *f; f = mntopt(&opts)) {
		if (strncmp(opt, f, strlen(opt)) == 0) {
			free (tmpopts);
			return (f - tmpopts + mnt->mnt_opts);
		}
	} 
	free (tmpopts);
	return (NULL);
}

static int
mntprtent(FILE *mnttabp, const struct mntent *mnt)
{
	fprintf(mnttabp, "%s %s %s %s %d %d\n",
	    mnt->mnt_fsname,
	    mnt->mnt_dir,
	    mnt->mnt_type,
	    mnt->mnt_opts,
	    mnt->mnt_freq,
	    mnt->mnt_passno);
	return(0);
}

static int
copymntent( struct mntent *src, struct mntent *dst )
{
	(void) memset((char *)dst, 0, sizeof (*dst));
	dst->mnt_fsname = strdup(src->mnt_fsname);
	if (dst->mnt_fsname == NULL) {
		return( -1 );
	}
	dst->mnt_dir = strdup(src->mnt_dir);
	if (dst->mnt_dir == NULL) {
		return( -1 );
	}
	dst->mnt_type = strdup(src->mnt_type);
	if (dst->mnt_type == NULL) {
		return( -1 );
	}
	dst->mnt_opts = strdup(src->mnt_opts);
	if (dst->mnt_opts == NULL) {
		return( -1 );
	}
	dst->mnt_freq = src->mnt_freq;
	dst->mnt_passno = src->mnt_passno;
	return( 0 );
}

#define	DIFF(mget, mref, xx)\
	((mref)->xx != NULL && ((mget)->xx == NULL ||\
	 strcmp((mref)->xx, (mget)->xx) != 0))
#define	SDIFF(mget, xx, typem, typer)\
	((mget)->xx == NULL || stat64((mget)->xx, &statb) == -1 ||\
	(statb.st_mode & S_IFMT) != typem ||\
	 statb.st_rdev != typer)
#define DIFF_MNTENT(mget, mref) \
	((bstat == 0 && DIFF(mget, mref, mnt_fsname)) || \
	(bstat == 1 && SDIFF(mget, mnt_fsname, bmode, brdev)) || \
	DIFF(mget, mref, mnt_dir) || DIFF(mget, mref, mnt_type) || \
	DIFF(mget, mref, mnt_opts))

int
getmntany( FILE *fp, struct mntent *mget, const struct mntent *mref )
{
	struct mntent *mtp = NULL;
	int	bstat;
	mode_t	bmode;
	dev_t	brdev;
	struct stat64	statb;

	if ( !mget || !mref ) {
		setoserror(EINVAL);
		return( -1 );
	}
	if (mref->mnt_fsname && stat64(mref->mnt_fsname, &statb) == 0 &&
	  ((bmode = (statb.st_mode & S_IFMT)) == S_IFBLK || bmode == S_IFCHR)) {
		bstat = 1;
		brdev = statb.st_rdev;
	} else
		bstat = 0;
	while ((mtp = getmntent(fp)) != NULL && DIFF_MNTENT(mtp, mref))
		;
	return( mtp ? copymntent( mtp, mget ) : -1);
}

static void
freemntlist(struct mntlist *ml)
{
	struct mntlist *next;
	struct mntent *me;

	while (ml) {
		next = ml->mntl_next;
		me = ml->mntl_mnt;
		if (me->mnt_fsname)
			free(me->mnt_fsname);
		if (me->mnt_dir)
			free(me->mnt_dir);
		if (me->mnt_type)
			free(me->mnt_type);
		if (me->mnt_opts)
			free(me->mnt_opts);
		free(me);
		free(ml);
		ml = next;
	}
}

int
delmntent(const char *fname, struct mntent *mref)
{
	FILE *fp;
	struct mntent *mnt;
	struct mntlist *mntl_head = NULL;
	struct mntlist *mntl_prev, *mntl;
	int delete = 0;
	off_t offset = (off_t)0;
	struct stat64	statb;
	int	bstat = 0;
	mode_t	bmode;
	dev_t	brdev;
	sigset_t allsigs;
	sigset_t oldsigset;
	int ret = 0;

	/* open the mnttab file */
	/*
	 * setmntent will lock the file since r+ is specified
	 */
	fp = setmntent( fname, "r+" );
	if (fp == NULL) {
		return -1;
	}

	/*
	 * Read the entire mnttab into memory.
	 * Remember the *first* instance of the unmounted
	 * mount point and make sure that it's not written
	 * back out.
	 * Record the offset of the start of the entry to be removed.
	 * Make a list in memory of the entries after the one to be deleted.
	 */
	mntl_prev = NULL;
	while (mnt = getmntent(fp)) {
		if (!DIFF_MNTENT(mnt, mref)) {
			delete = 1;
		} else if (delete) {
			mntl = (struct mntlist *) malloc(sizeof (*mntl));
			if (mntl == NULL) {
				endmntent(fp);
				freemntlist(mntl_head);
				return -1;
			}
			if (mntl_head == NULL)
				mntl_head = mntl;
			else
				mntl_prev->mntl_next = mntl;
			mntl_prev = mntl;
			mntl->mntl_next = NULL;
			mntl->mntl_mnt = (struct mntent *)malloc(sizeof(struct mntent));
			if (mntl->mntl_mnt == NULL) {
				endmntent(fp);
				freemntlist(mntl_head);
				return -1;
			}
			if (copymntent(mnt, mntl->mntl_mnt) == -1) {
				endmntent(fp);
				freemntlist(mntl_head);
				return -1;
			}
		} else {
			offset = ftell(fp);
		}
	}

	if (!delete) {
		(void) endmntent(fp);
		return 0;
	}

	/*
	 * block all user signals while updating the file
	 * if this is successful, it is necessary to ensure that we do not
	 * return without restoring the original mask
	 */
	if ( (sigfillset( &allsigs ) == -1) ||
		(sigprocmask( SIG_SETMASK, &allsigs, &oldsigset ) == -1) ) {
			(void) endmntent(fp);
			freemntlist(mntl_head);
			return -1;
	}

	/*
	 * truncate everything in the mount table after offset
	 * add all of the entries in the list at the end of the file
	 */
	if (ftruncate(fileno(fp), offset) < 0) {
		ret = -1;
	} else if (fseek(fp, (off_t)0, SEEK_END) == -1) {
		ret = -1;
	} else {

		/* output the mnttab entries */
		ret = 1;
		for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
			/* output the entry */
			if (addmntent(fp, mntl->mntl_mnt) != 0) {
				ret = -1;
				break;
			}
		}
	}

	(void)sigprocmask( SIG_SETMASK, &oldsigset, NULL );

	(void) endmntent(fp);
	freemntlist(mntl_head);
	return( ret );
}
