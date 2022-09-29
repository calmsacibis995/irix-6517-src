/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	du	COMPILE:	cc -O du.c -s -i -o du	*/

#ident "$Revision: 1.24 $"

/*
**	du -- summarize disk usage
**		du [-aklmxrsLR] [path ...]
*/

#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<sys/fstyp.h>
#include	<sys/fsid.h>
#include	<sys/statvfs.h>
#include	<limits.h>
#include 	<dirent.h>
#include 	<malloc.h>
#include 	<errno.h>
#include 	<string.h>
#include	<locale.h>
#include 	<pfmt.h>

#define ML		256	/* initial size of hard link table */
#define BLOCKSIZE	512	/* default logical block size */

/*
 * If the -k option is specified, the computations are still done
 * in terms of 512 bytes blocks, since that's the actual minimal
 * fragment size of EFS, but are reported in terms of 1k blocks.
 */
#define blkprint(blks)	(kflag ? howmany((blks),2) : (blks))

struct 	hlink {
	dev_t	dev;
	ino64_t	ino;
};

struct	hlink *ml;	/* table of files with more than one hard link */
int	mlsize = 0;	/* current size of ml table (in terms of entries) */
int	mlcnt = 0;	/* current number of entries in the ml table */

struct	stat64	Statb;
statvfs64_t	Statvfsb;

int 	err = 0;
char	path[PATH_MAX];

int	aflag = 0;
int	rflag = 1;
int	sflag = 0;
int	kflag = 0;
int	lflag = 0;
int	mflag = 0;
int	Lflag = 0;

int	maxdirfd;

__int64_t	descend();
static void mlrealloc();

int	root_st_dev;	/* holds device of root of du if mflag turned on */

char	opts[] = "aklmxrsLR";
static const char errmsg[] = ":37:%s: %s\n";

main(argc, argv)
int argc;
char **argv;
{
	register int c, nargs;
	register __int64_t blocks;
	extern int optind;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore");
	(void)setlabel("UX:du");

	setbuf(stderr, NULL);
	while ((c = getopt(argc, argv, opts)) != EOF)
		switch (c) {
		case 'a':
			aflag++;
			continue;
		case 'k':
			kflag++;
			continue;
		case 'l':
			lflag++;
			continue;
		case 'm':
			mflag++;
			continue;
		case 'x':
			mflag++;
			continue;
		case 'L':
			Lflag++;	/* added for symbolic link */
			continue;
		case 'r':
			rflag = 1;
			continue;
		case 'R':
			rflag = 0;
			continue;
		case 's':
			sflag++;
			continue;
		default:
			pfmt(stderr, MM_ACTION,
			  "uxsgicore:99:Usage: du [-aklmxrsLR] [path ...]\n");
			exit(2);
		}

	nargs = argc - optind;
	argv += optind;
	if (nargs == 0) {
		nargs = 1;
		argv[0] = ".";
	}

	mlrealloc(ML);

	fclose(stdin);
	maxdirfd = getdtablesize() - 1;
	while (--nargs >= 0) {
		strcpy(path, *argv++);
		if (mflag) {
			stat64(path, &Statb);
			root_st_dev = Statb.st_dev;
		}
		blocks = descend(path);
		if(sflag)
			if ((!Lflag ? lstat64(path, &Statb) : stat64(path, &Statb)) >= 0)
				printf("%lld\t%s\n", blkprint(blocks), path);
	}

	exit(err);
}

__int64_t descend(name)
char *name;
{
	register DIR 	*dirf;		/* open directory */
	register dirent64_t	*dp;	/* current directory */
	register char	*c1, *c2;
	__int64_t blocks = 0;
	int	dir = 0;		/* open directory */
	off64_t	offset;
	int	dsize, entries, i;
	char	*endofname;

	if ((!Lflag ? lstat64(name, &Statb) : stat64(name, &Statb)) < 0) {
		if (rflag)
			pfmt(stderr, MM_ERROR, errmsg, name, strerror(errno));
		err = 2;
		return(0);
	}

	if (mflag || lflag) {
		if (statvfs64(name, &Statvfsb) < 0) {
			if (rflag)
				pfmt(stderr, MM_ERROR, errmsg, name,
					strerror(errno));
			err = 2;
			return(0);
		}
	}
	if (mflag) {
		if (Statb.st_dev != root_st_dev)
			return(0);
	}
	if (lflag) {
		if (!(Statvfsb.f_flag & ST_LOCAL))
			return(0);
	}

	if(Statb.st_nlink>1 && (Statb.st_mode&S_IFMT)!=S_IFDIR) {
		/*
		 * Reallocate the hard link table on overflow.
		 */
		if (mlcnt >= mlsize)
			mlrealloc(2*mlsize);
		for(i = 0; i < mlcnt; ++i) {
			if(ml[i].ino==Statb.st_ino && ml[i].dev==Statb.st_dev)
				return 0;
		}
		ml[mlcnt].dev = Statb.st_dev;
		ml[mlcnt].ino = Statb.st_ino;
		++mlcnt;
	}
	blocks = Statb.st_blocks;

	if((Statb.st_mode&S_IFMT)!=S_IFDIR) {
		if(aflag)
			printf("%lld\t%s\n", blkprint(blocks), name);
		return(blocks);
	}

	for(c1 = name; *c1; ++c1);
	endofname = c1;
	if((dirf = opendir(name)) == NULL) {
		if (rflag)
			pfmt(stderr, MM_ERROR, errmsg, name, strerror(errno));
		err = 2;
		return(0);
	}
	offset = 0;

	/*
	 * Foreach entry in the current directory...
	 *
	 * From here till the end of descend() we must always remove the
	 * pathname component last added after endofname before returning.
	 * We must also closedir() dirf if it is non-null before returning.
	 * Hence the several "goto out" statements.
	 */
	while((dp = readdir64(dirf)) != NULL) {
		if(dp->d_ino == 0
		   || !strcmp(dp->d_name, ".")
		   || !strcmp(dp->d_name, ".."))
			continue;
		c1 = endofname;
		if (c1[-1] != '/')
			*c1++ = '/';
		c2 = dp->d_name;
		for(i=0; i<dp->d_reclen; i++)
			if(*c2)
				*c1++ = *c2++;
			else
				break;
		*c1 = '\0';
		if(i == 0) { /* null length name */
			pfmt(stderr, MM_ERROR,
				"uxsgicore:100:bad dir entry %s\n",
				dp->d_name);
			err = 2;
			goto out;
		}
		/*
		 * Recursively call 'descend' for this entry.  If the
		 * recursion is getting too deep, close the directory
		 * and reopen it upon return to prevent running out
		 * of file descriptors.
		 */
		if(dirf->dd_fd > maxdirfd) {
			offset = telldir64(dirf);  /* remember current position */
			closedir(dirf);
			dirf = NULL;
		}
		blocks += descend(name);
		if(dirf == NULL) {
			if((dirf = opendir(name)) == NULL) {
				if (rflag)
					pfmt(stderr, MM_ERROR, errmsg, name,
					strerror(errno));
				err = 2;
				goto out;
			}
			if(offset) {
				seekdir64(dirf, offset);
				offset = 0;
			}
		}
	}
out:
	if(dirf)
		closedir(dirf);
	*endofname = '\0';
	if(!sflag)
		printf("%lld\t%s\n", blkprint(blocks), name);
	return(blocks);
}


static void
mlrealloc(entr)
int entr;
{
	if (entr >= 1<<24) {
		pfmt(stderr, MM_ERROR,
			"uxsgicore:97:link table overflow\n");
		exit(1);
	}
	
	/*
	 * Use malloc the first time, realloc thereafter.
	 */
	if (mlsize == 0)
		ml = (struct hlink *) malloc(entr * sizeof(*ml));
	else
		ml = (struct hlink *) realloc(ml, entr * sizeof(*ml));

	if (ml == NULL) {
		pfmt(stderr, MM_ERROR,
			"uxsgicore:98:Can't allocate %d bytes of memory\n",
			entr * sizeof(*ml));
		exit(2);
	}

	mlsize = entr;
}
