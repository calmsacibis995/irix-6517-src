#ident "$Revision: 1.12 $"

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* "@(#)ncheck:ncheck.c	1.13" */
/*
 * ncheck -- obtain file names from reading filesystem
 */

/* number of inode blocks to process at a time */
#define	NB	3000

/* number of directories per block */
#define	NDIR	(BBSIZE / sizeof(struct efs_direct))

#define HSIZE	32068

#include "libefs.h"
#include <ctype.h>
#include <diskinfo.h>
#include <mntent.h>
#include <signal.h>

char	*fstab = "/etc/fstab";

struct	efs_mount *mp;

struct	htab
{
	efs_ino_t	h_ino;
	efs_ino_t	h_pino;
	char		*h_name;
} htab[HSIZE];

extent		*ex;
int		aflg;
int		iflg;
int		sflg;
efs_ino_t	ino;
int		nhent;
int		nxfile;
int		nerror;
char		*progname;
efs_ino_t	ilist[NB];

static void check(char *file);
static int dotname(struct efs_dent *dep);
static char *emalloc(int size);
static struct htab *lookup(efs_ino_t i, int ef);
static void nxtpass(int pass, efs_ino_t inode);
static void pass1(struct efs_dinode *dip);
static void pname(efs_ino_t i, int lev);
static void reinit(void);

int
main(int argc, char **argv)
{
	int i;
	FILE *fp;
	long n;

	progname = argv[0];
	while (--argc) {
		argv++;
		if (**argv=='-')
		switch ((*argv)[1]) {

		case 'a':
			aflg++;
			continue;

		case 'i':
			for(i=0; i<NB;) {
				if (argc == 1) {
					if (i != 0)
						break;
				fprintf(stderr,
				   "%s: -i needs at least one inode number\n",
				   progname);
					exit(2);
				}
				n = atol(argv[1]);
				if(n == 0)
					break;
				ilist[i] = n;
				iflg = ++i;
				argv++;
				argc--;
			}
			continue;

		case 's':
			sflg++;
			continue;

		default:
			fprintf(stderr, "%s: bad flag %c\n", progname, (*argv)[1]);
			nerror++;
		} else
			break;
	}
	if (nerror)
		return(nerror);
	nxfile = iflg;
	ilist[nxfile] = 0;
	if(argc) {		/* arg list has file names */
		while(argc-- > 0)
			check(*argv++);
	}
	else {			/* use default fstab */
		struct mntent *me;

		if ((fp = setmntent(fstab, "r")) == NULL) {
			fprintf(stderr,"%s: can't open fstab file: %s\n",
					    progname, fstab);
			exit(2);
		}
		while (me = getmntent(fp)) {
			if (strcmp(MNTTYPE_EFS, me->mnt_type) == 0)
				check(me->mnt_fsname);
		}
		endmntent(fp);
	}
	return(nerror);
}

static char *
emalloc(int size)
{
	char *p;
	
	p = malloc(size);
	if (p == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		exit(1);
	}
	return (p);
}

static void
check(char *file)
{
	int i, j;
	struct efs_dinode *itab, *dip;
	int pass;
	char *rawpath;

	/*
	 * Make sure that we have the character rather than block
	 * devices (since readb expects character devs)
         */
	if ((rawpath = findrawpath(file)) == NULL) {
		fprintf(stderr, "%s: cannot find character dev for %s\n", 
			progname, file);
		return;
	}
 
	printf("%s:\n", file);
	sync();
	if ((mp = efs_mount(rawpath, 0)) == NULL) {
	  	fprintf(stderr, "%s: problems reading %s, skipping...\n",
			progname, file);
		return;
	}

	/*
	 * For each cylinder group, read in its ilist and scan it.
	 */
	ino = 0;
	for (i = 0; i < mp->m_fs->fs_ncg; i++) {
		itab = dip = efs_igetcg(mp, i);
		for (j = mp->m_fs->fs_cgisize * EFS_INOPBB; --j >= 0;
		     ino++, dip++) {
			if (ino < 2)
				continue;
			pass1(dip);
		}
		free(itab);
	}
	ilist[nxfile] = 0;

	for(pass = 2; pass <= 3; pass++) {
		ino = 0;
		for (i = 0; i < mp->m_fs->fs_ncg; i++) {
			itab = dip = efs_igetcg(mp, i);
			for (j = mp->m_fs->fs_cgisize * EFS_INOPBB; --j >= 0;
			       ino++, dip++) {
				if (ino < 2)
					continue;
				if ((dip->di_mode & IFMT) == IFDIR)
					nxtpass(pass, ino);
			}
			free(itab);
		}
	}

	efs_umount(mp);
	reinit();
}

static void
pass1(struct efs_dinode *dip)
{
	if ((dip->di_mode & IFMT) != IFDIR) {
		if (sflg != 1) return;
		if (nxfile >= NB){
			++sflg;
			fprintf(stderr, "%s: too many special files\n",
					progname);
			return;
		}
		if (((dip->di_mode & IFMT) == IFBLK) ||
		    ((dip->di_mode & IFMT) == IFCHR) ||
		    (dip->di_mode & (ISUID | ISGID)))
			ilist[nxfile++] = ino;
		return;
	}
	lookup(ino, 1);
}

static void
nxtpass(int pass, efs_ino_t inode)
{
	EFS_DIR *dir;
	struct efs_dent *dep;
	int k;
	struct htab *hp;

	if ((dir = efs_opendiri(mp, inode)) == NULL) {
		fprintf(stderr, "%s: could not open file for inode %d\n",
			progname, ino);
		return;
	}

	while ((dep = efs_readdir(dir)) != NULL) {
		switch(pass) {
			case 2:
				if(((hp = lookup(EFS_GET_INUM(dep), 0)) == 0)
				|| (dotname(dep)))
					continue;
				hp->h_pino = ino;
				hp->h_name = emalloc(dep->d_namelen+1);
				bcopy(dep->d_name, hp->h_name, dep->d_namelen);
				hp->h_name[dep->d_namelen] = '\0';
				break;
			case 3:
				if(aflg==0 && dotname(dep))
					continue;
				if(ilist[0] == 0 && sflg==0)
					goto pr;
				for(k=0; ilist[k] != 0; k++)
					if(ilist[k] == EFS_GET_INUM(dep))
						goto pr;
				continue;
			pr:
				printf("%u	", EFS_GET_INUM(dep));
				pname(ino, 0);
				printf("%.*s", dep->d_namelen, dep->d_name);
				if (lookup(EFS_GET_INUM(dep), 0))
					printf("/.");
				printf("\n");
		}
	}
	efs_closedir(dir);
}

static int
dotname(struct efs_dent *dep)
{
	if (dep->d_name[0]=='.')
		if ((dep->d_namelen==1) ||
		    (dep->d_name[1]=='.' && dep->d_namelen==2))
			return(1);
	return(0);
}

static void
reinit(void)
{
	struct htab *hp, *hplim;

	hplim = &htab[HSIZE];
	for(hp = &htab[0]; hp < hplim; hp++)
		hp->h_ino = 0;
	nxfile = iflg;
	ilist[nxfile] = 0;
	if(sflg)
		sflg = 1;
}

static void
pname(efs_ino_t i, int lev)
{
	struct htab *hp;

	if (i==EFS_ROOTINO) {
		printf("/");
		return;
	}
	if ((hp = lookup(i, 0)) == 0) {
		printf("???");
		return;
	}
	if (lev > 10) {
		printf("...");
		return;
	}
	pname(hp->h_pino, ++lev);
	printf("%s/", hp->h_name);
}

static struct htab *
lookup(efs_ino_t i, int ef)
{
	struct htab *hp;

	for (hp = &htab[i%HSIZE]; hp->h_ino;) {
		if (hp->h_ino==i)
			return(hp);
		if (++hp >= &htab[HSIZE])
			hp = htab;
	}
	if (ef==0)
		return(0);
	if (++nhent >= HSIZE) {
		fprintf(stderr, "ncheck: out of core-- increase HSIZE\n");
		exit(1);
	}
	hp->h_ino = i;
	return(hp);
}

/*
 * Print out a nice message when an i/o error occurs
 */
void
error(void)
{
	int old_errno;

	old_errno = errno;
	fprintf(stderr, "%s: i/o error\n", progname);
	errno = old_errno;
	perror(progname);
	exit(1);
}
