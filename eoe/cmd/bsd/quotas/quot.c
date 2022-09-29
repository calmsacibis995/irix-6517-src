/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * "@(#)quot.c	1.2 88/03/15 4.0NFSSRC";
 */

#ident  "$Revision: 1.19 $"
/*
 * quot
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <mntent.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/fs/efs.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <pwd.h>
#include <utmp.h>
#include <sys/syssgi.h>
#include <sys/capability.h>
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_itable.h>

#define ROOTINO	EFS_ROOTINO
#define DEV_BSIZE BBSIZE
#define BTOKB(s)	((unsigned long long)(howmany((s), 0x400ULL)))

/*
 * An sblock that is atleast basic block size, used for transfering bb size
 * from disk. bbsize is the minimum transfer size required by the system.
 */
union bb_sblock { 
	struct efs 	sb;
	char		filler[BBSIZE];
};

typedef enum {
  FS_NONE, FS_XFS, FS_EFS, FS_NFS
} quota_fstype_t;


union bb_sblock bbs;
#define sblock	(bbs.sb)

struct efs_dinode *itab = NULL;		/* disk inodes per cylinder group */
#define	NDU		60000

struct du {
	struct	du 	  	*next;
	unsigned long long 	blocks;
	unsigned long long	blocks30;
	unsigned long long	blocks60;
	unsigned long long	blocks90;
	unsigned long long	nfiles;
	int			uid;
} du[NDU];
int	ndu;
#define	DUHASH	8209	/* smallest prime >= 4 * NDU */
#define	HASH(u)	((u) % DUHASH)
struct	du *duhash[DUHASH];

#define	TSIZE		500
unsigned long long	sizes[TSIZE];
unsigned long long	overflow;

int	nflg;
int	fflg;
int	cflg;
int	vflg;
int	hflg;
int	aflg;
time_t	now;

unsigned	ino;

char		*getname(int);
extern char 	*findrawpath(char *); 
static int	checkXFS(char *file, char *fsdir);
static int	check(char *file, char *fsdir);
static void	acctXFS(xfs_bstat_t *p);
static void 	acctEFS(struct efs_dinode *ip);
static void	quotall(void);
static void	bread(int fd, unsigned 	bno, char *buf, int cnt);
static int 	qcmp(struct du *p1, struct du *p2);
static void	report(void);
static int	getdev(char **devpp, char *mntdir, quota_fstype_t *fstype);

main(argc, argv)
	int argc;
	char *argv[];
{
	char 		mntdir[80];
	quota_fstype_t	fstype;
		
	if (getuid()) {
		fprintf(stderr, "%s: permission denied\n", argv[0]);
		exit(1);
	}

	now = time(0);
	argc--, argv++;
	if (argc == 0)
		goto usage;
	while (argc > 0 && argv[0][0] == '-') {
		register char *cp;

		for (cp = &argv[0][1]; *cp; cp++)
			switch (*cp) {
			case 'n':
				nflg++; break;
			case 'f':
				fflg++; break;
			case 'c':
				cflg++; break;
			case 'v':
				vflg++; break;
			case 'a':
				aflg++; break;
			default:
usage:
				fprintf(stderr,
				    "usage: quot [-nfcva] [filesystem ...]\n");
				exit(1);
			}
		argc--, argv++;
	}
	if (aflg) {
		quotall();
	}
	while (argc-- > 0) {
		if (getdev(argv, mntdir, &fstype) == 0) {
			if (fstype == FS_XFS) {
				if (nflg) {
					fprintf(stderr,
				         "quot: -n invalid for XFS filesystems\n");
					nflg = 0;
				}
				if (checkXFS(*argv, mntdir) == 0)
					report();
			} else if (fstype == FS_EFS) {
				if (check(*argv, mntdir) == 0)
					report();
			}
		} else {
			if (vflg)
				fprintf(stderr, 
			    "quot: Can't find block device for filesystem %s\n", 
					*argv);
		}
		argv++;
	}
	exit (0);
	/* NOTREACHED */
}

static void
quotall(void)
{
	FILE *fstab;
	register struct mntent *mntp;
	char dev[80];

	fstab = setmntent(MOUNTED, "r");
	if (fstab == NULL) {
		fprintf(stderr, "quot: no %s file\n", MOUNTED);
		exit(1);
	}
	while (mntp = getmntent(fstab)) {
		strcpy(dev, mntp->mnt_fsname);
		if (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) {
			if (nflg) {
				if (vflg)
					fprintf(stderr,
					 "quot: -n invalid for XFS filesystems\n");
				nflg = 0;
			}
			if (checkXFS(dev, mntp->mnt_dir) == 0)
				report();
		} else if (strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0) {
			if (check(dev, mntp->mnt_dir) == 0)
				report();
		}
	}
	endmntent(fstab);
}

static void
acctXFS(
	xfs_bstat_t		*p)
{
	register struct du 	*dp;
	struct du 		**hp;
	unsigned long long	size;

	if ((p->bs_mode & IFMT) == 0)
		return;
	size = BTOKB(p->bs_blocks * p->bs_blksize);
	
	if (cflg) {
		if ((p->bs_mode&IFMT) != IFDIR && (p->bs_mode&IFMT) != IFREG)
			return;
		if (size >= TSIZE) {
			overflow += size;
			size = TSIZE-1;
		}
		sizes[(int)size]++;
		return;
	}
	hp = &duhash[HASH(p->bs_uid)];
	for (dp = *hp; dp; dp = dp->next)
		if (dp->uid == p->bs_uid)
			break;
	if (dp == 0) {
		if (ndu >= NDU)
			return;
		dp = &du[ndu++];
		dp->next = *hp;
		*hp = dp;
		dp->uid = p->bs_uid;
		dp->nfiles = 0;
		dp->blocks = 0;
		dp->blocks30 = 0;
		dp->blocks60 = 0;
		dp->blocks90 = 0;
	}
	dp->blocks += size;

#define	DAY (60 * 60 * 24)	/* seconds per day */
	if (now - p->bs_atime.tv_sec > 30 * DAY)
		dp->blocks30 += size;
	if (now - p->bs_atime.tv_sec > 60 * DAY)
		dp->blocks60 += size;
	if (now - p->bs_atime.tv_sec > 90 * DAY)
		dp->blocks90 += size;
	dp->nfiles++;
}

static int
checkXFS(char *file, char *fsdir)
{
	int		fsfd, i=0;
	ino64_t 	lastino;
	xfs_bstat_t 	*buf;
	xfs_bstat_t 	*p;
	xfs_bstat_t 	*endp;
	int		buflen;
	struct du 	**dp;
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;
	int 		r;

	
#define BUFLEN	1024	
	/*
	 * Initialize tables between checks;
	 * because of the qsort done in report()
	 * the hash tables must be rebuilt each time.
	 */
	for (i = 0; i < TSIZE; i++)
		sizes[i] = 0;
	overflow = 0;
	for (dp = duhash; dp < &duhash[DUHASH]; dp++)
		*dp = 0;
	ndu = 0;

	fsfd = open(file, O_RDONLY);
	if (fsfd < 0) {
		fprintf(stderr, "quot: ");
		perror(file);
		return (-1);
	}
	printf("%s", file);
	if (fsdir != NULL && *fsdir != '\0')
		printf(" (%s)", fsdir);
	printf(":\n");
	sync();
	
	lastino = 0;
	buf = (xfs_bstat_t *) calloc(BUFLEN, sizeof(xfs_bstat_t));
	for (;;) {
		ocap = cap_acquire(1, &cap_device_mgt);
		r = syssgi(SGI_FS_BULKSTAT, fsfd, &lastino, BUFLEN, buf,
			   &buflen);
		cap_surrender(ocap);
		if (r)
			break;

		if (buflen == 0)
			break;

		for (p = buf, endp = buf + buflen; p < endp; p++) {
			acctXFS(p);
			i++;
		}
	}
	
	free((void *)buf);
	close(fsfd);
	return (0);
}

static int
check(
	char *file,
	char *fsdir)
{
	register int 	i, j, nfiles;
	struct du 	**dp;
	daddr_t 	iblk;
	int 		c, fd;
	char 		*rawdev;

	if ((rawdev = findrawpath(file)) == NULL) {
		fprintf(stderr, "Cannot find raw device for %s\n", file);
		return(-1);
	}
	/*
	 * Initialize tables between checks;
	 * because of the qsort done in report()
	 * the hash tables must be rebuilt each time.
	 */
	for (i = 0; i < TSIZE; i++)
		sizes[i] = 0;
	overflow = 0;
	for (dp = duhash; dp < &duhash[DUHASH]; dp++)
		*dp = 0;
	ndu = 0;
	fd = open(rawdev, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "quot: ");
		perror(rawdev);
		return (-1);
	}
	printf("%s", file);
	if (fsdir != NULL && *fsdir != '\0')
		printf(" (%s)", fsdir);
	printf(":\n");
	sync();
	bread(fd, EFS_SUPERBB, (char *)&sblock, BBSIZE);
	if (!IS_EFS_MAGIC(sblock.fs_magic)) {
	    fprintf(stderr,"quot: bad magic number in super block\n");
	    exit(1);
	}
	sblock.fs_ipcg = EFS_COMPUTE_IPCG(&sblock);
	if (nflg) {
		if (isdigit(c = getchar()))
			ungetc(c, stdin);
		else while (c != '\n' && c != EOF)
			c = getchar();
	}
	if ((itab = (struct efs_dinode *)malloc(EFS_COMPUTE_IPCG(&sblock) *
			(1 << EFS_EFSINOSHIFT))) == NULL) {
		fprintf(stderr, "Couldn't malloc %d bytes for inode table\n",
			EFS_COMPUTE_IPCG(&sblock) * (1 << EFS_EFSINOSHIFT));
		exit(1);
	}
	nfiles = EFS_COMPUTE_IPCG(&sblock) * sblock.fs_ncg;
	for (ino = 0; ino < nfiles; ) {
		iblk = EFS_ITOBB(&sblock, ino);
		bread(fd, (daddr_t)iblk, (char *)itab, 
			(EFS_COMPUTE_IPCG(&sblock) * (1 << EFS_EFSINOSHIFT)));
		for (j = 0; j < EFS_COMPUTE_IPCG(&sblock); j++, ino++) {
			if (ino < ROOTINO)
				continue;
			acctEFS((struct efs_dinode *)itab + j);
		}
	}
	close(fd);
	free(itab);
	itab = NULL;
	return (0);
}

static void
acctEFS(struct efs_dinode *ip)
{
	struct du *dp;
	struct du **hp;
	long size;
	int n, niex, i;
	static fino;

	if ((ip->di_mode & IFMT) == 0)
		return;
	size = ip->di_size;
	if (ip->di_numextents > EFS_DIRECTEXTENTS) {
		if ((niex = ip->di_u.di_extents[0].ex_offset) >
				EFS_DIRECTEXTENTS)
			return;
		for (i = 0; i < niex; i++)
			size += (u_int)ip->di_u.di_extents[i].ex_length;
	}
	size = (long) BTOKB(size);

	if (cflg) {
		if ((ip->di_mode&IFMT) != IFDIR && (ip->di_mode&IFMT) != IFREG)
			return;
		if (size >= TSIZE) {
			overflow += (unsigned long long) size;
			size = TSIZE-1;
		}
		sizes[size]++;
		return;
	}
	hp = &duhash[HASH(ip->di_uid)];
	for (dp = *hp; dp; dp = dp->next)
		if (dp->uid == ip->di_uid)
			break;
	if (dp == 0) {
		if (ndu >= NDU)
			return;
		dp = &du[ndu++];
		dp->next = *hp;
		*hp = dp;
		dp->uid = ip->di_uid;
		dp->nfiles = 0;
		dp->blocks = 0;
		dp->blocks30 = 0;
		dp->blocks60 = 0;
		dp->blocks90 = 0;
	}
	dp->blocks += size;
#define	DAY (60 * 60 * 24)	/* seconds per day */
	if (now - ip->di_atime > 30 * DAY)
		dp->blocks30 += size;
	if (now - ip->di_atime > 60 * DAY)
		dp->blocks60 += size;
	if (now - ip->di_atime > 90 * DAY)
		dp->blocks90 += size;
	dp->nfiles++;
	while (nflg) {
		register char *np;

		if (fino == 0)
			if (scanf("%d", &fino) <= 0)
				return;
		if (fino > ino)
			return;
		if (fino < ino) {
			while ((n = getchar()) != '\n' && n != EOF)
				;
			fino = 0;
			continue;
		}
		if (np = getname(dp->uid))
			printf("%.7s	", np);
		else
			printf("%d	", (int) ip->di_uid);
		while ((n = getchar()) == ' ' || n == '\t')
			;
		(void) putchar(n);
		while (n != EOF && n != '\n') {
			n = getchar();
			(void) putchar(n);
		}
		fino = 0;
		break;
	}
}
static void
bread(
	int		fd,
	unsigned 	bno,
	char 		*buf,
	int		cnt)
{
	if (syssgi(SGI_READB, fd, buf, bno, BTOBBT(cnt)) != BTOBBT(cnt)) {
		perror("syssgi");
		fprintf(stderr, "quot: read error at block %u\n", bno);
		exit(1);
	}
}

static int
qcmp(struct du *p1, struct du *p2)
{
	if (p1->blocks > p2->blocks)
		return (-1);
	if (p1->blocks < p2->blocks)
		return (1);
	if (p1->uid > p2->uid)
		return(1);
	else if (p1->uid < p2->uid)
		return(-1);
	return(0);
}

static void
report(void)
{
	register i;
	register struct du *dp;

	if (nflg)
		return;
	if (cflg) {
		unsigned long long t = 0;

		for (i = 0; i < TSIZE - 1; i++)
			if (sizes[i] > 0) {
				t += sizes[i] * i;
				printf("%d\t%llu\t%llu\n", i, sizes[i], t);
			}
		printf("%d\t%llu\t%llu\n",
		       TSIZE - 1, sizes[TSIZE - 1], overflow + t);
		return;
	}
	qsort(du, ndu, sizeof (du[0]), (int (*)(const void *, const void *))qcmp);
	for (dp = du; dp < &du[ndu]; dp++) {
		register char *cp;

		if (dp->blocks == 0)
			return;
		printf("%8llu    ", dp->blocks);
		if (fflg)
			printf("%8llu    ", dp->nfiles);
		if (cp = getname(dp->uid))
			printf("%-8.8s", cp);
		else
			printf("#%-7d", dp->uid);
		if (vflg)
			printf("    %8llu    %8llu    %8llu",
			       dp->blocks30, 
			       dp->blocks60,
			       dp->blocks90);
		printf("\n");
	}
}

struct	utmp utmp;

#define NUID	256		/* power of two */
#define UIDMASK	(NUID-1)

#define	NMAX	(sizeof (utmp.ut_name))
#define SCPYN(a, b)	strncpy(a, b, NMAX)

struct uidcache {
	int	uid;
	char	name[NMAX + 1];
} nc[NUID];
int entriesleft = NUID;

char *
getname(
	int uid)
{
	register struct passwd *pw;
	register struct uidcache *ncp;

	/*
	 * Check cache for name.
	 */
	ncp = &nc[uid & UIDMASK];
	if (ncp->uid == uid && ncp->name[0])
		return (ncp->name);
	if (entriesleft) {
		/*
		 * If we haven't gone through the passwd file then
		 * fill the cache while seaching for name.
		 * This lets us run through passwd serially.
		 */
		if (entriesleft == NUID)
			setpwent();
		while ((pw = getpwent()) && entriesleft) {
			entriesleft--;
			ncp = &nc[pw->pw_uid & UIDMASK];
			if (ncp->name[0] == '\0' || pw->pw_uid == uid) {
				SCPYN(ncp->name, pw->pw_name);
				ncp->uid = uid;
			}
			if (pw->pw_uid == uid)
				return (ncp->name);
		}
		endpwent();
		entriesleft = 0;
		ncp = &nc[uid & UIDMASK];
	}
	/*
	 * Not in cache. Do it the slow way.
	 */
	pw = getpwuid(uid);
	if (!pw)
		return (0);
	SCPYN(ncp->name, pw->pw_name);
	ncp->uid = uid;
	return (ncp->name);
}

int
getdev(
	char 		**devpp,
	char 		*mntdir,
	quota_fstype_t	*fstype)
{
	FILE 		*fstab;
	struct mntent 	*mntp;
	struct stat	st;

	*fstype = FS_NONE;
	
	/*
	 * We do this to trigger the mount (of the filesystem) 
	 * if the path given is that of a loopback filesystem.
	 */
	stat(*devpp, &st);

 again:
	strcpy(mntdir, *devpp);
	fstab = setmntent(MOUNTED, "r");
	if (fstab == NULL) {
		fprintf(stderr, "quot: no %s file\n", MOUNTED);
		exit(1);
	}
	while (mntp = getmntent(fstab)) {
		if (strcmp(mntp->mnt_dir, *devpp) == 0 ||
		    strcmp(mntp->mnt_fsname, *devpp) == 0) {
			if (strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0) {
				*fstype = FS_EFS;
			} else if (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) {
				*fstype = FS_XFS;
			} else if (strcmp(mntp->mnt_type, MNTTYPE_LOFS) == 0) {
				/*
				 * If this is a loopbackfs, then we ought to
				 * look at the real filesystem.
				 */

				/* XXX gross memory abuse */
				*devpp = malloc(strlen(mntp->mnt_fsname) + 1);
				strcpy(*devpp, mntp->mnt_fsname);
				endmntent(fstab);
				goto again; /* all over again */
			} else {
				fprintf(stderr,
				    "quot: %s not efs or xfs filesystem\n",
				    *devpp);
				break;
			}

			assert(*fstype == FS_EFS || *fstype == FS_XFS);
			*devpp = malloc(strlen(mntp->mnt_fsname) + 1);
			strcpy(*devpp, mntp->mnt_fsname);
			strcpy(mntdir, mntp->mnt_dir);
			endmntent(fstab);
			return (0);
		}
	}
	endmntent(fstab);
	return (-1);
}
