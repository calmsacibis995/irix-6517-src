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
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * "@(#)quotacheck.c	1.2 88/03/15 4.0NFSSRC";
 */

#ident  "$Revision: 1.17 $"
/*
 * Fix up / report on disc quotas & usage
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sema.h>
#include <sys/quota.h>
#include <sys/fs/efs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <mntent.h>
#include <pwd.h>
#include <sys/syssgi.h>
#include <strings.h>
#include <unistd.h>
#include <sys/capability.h>

#define MAXMOUNT	50

#define ROOTINO EFS_ROOTINO
#undef dbtob
#define dbtob BBTOB
/*
 * An sblock that is atleast basic block size, used for transfering bb size
 * from disk. bbsize is the minimum transfer size required by the system.
 */
union bb_sblock { 
	struct efs 	sb;
	char		filler[BBSIZE];
};

union bb_sblock bbs;
#define sblock	(bbs.sb)

#define kb(n) ((int)(howmany(dbtob(n), 1024)))

struct	efs_dinode *itab = NULL;

#define LOGINNAMESIZE	8
struct fileusage {
	struct fileusage *fu_next;
	u_long fu_curfiles;
	u_long fu_curblocks;
	u_short	fu_uid;
	char fu_name[LOGINNAMESIZE + 1];
};
#define FUHASH 997
struct fileusage *	fuhead[FUHASH];

static struct fileusage *	lookup(u_short);
static struct fileusage *	adduid(u_short);
static void			acctEFS(struct efs_dinode *ip);
static void			bread(long unsigned bno, char *buf, int cnt);
static int 			preen(int listcnt, char **listp);
static int 			oneof(char *target, char **listp, int n);
static int 			bump_isize(char *fsdev, char *fsfile, char *qf);
static int 			chkquota(char *fsdev, char *fsfile, char *qf);

int fi;
ino_t ino;

int	vflag;		/* verbose */
int	aflag;		/* all file systems */
int	pflag;		/* fsck like parallel check */
int	nflag;		/* resize the index in the start of the quotas file */
int	nusers;		/* new number of users */
int	isize;		/* new index size for above option */

#define QFNAME "quotas"
char *listbuf[MAXMOUNT];

extern char 	*findrawpath(char *); 
extern int 	errno;

main(argc, argv)
	int argc;
	char **argv;
{
	register struct mntent *mntp;
	register struct fileusage *fup;
	char **listp;
	int listcnt;
	char quotafile[MAXPATHLEN + 1];
	FILE *mtab, *fstab;
	int errs = 0;
	struct passwd *pw;
	char *arg0;

	arg0 = *argv;
again:
	argc--, argv++;
	if (argc > 0 && strcmp(*argv, "-v") == 0) {
		vflag++;
		goto again;
	}
	if (argc > 0 && strcmp(*argv, "-a") == 0) {
		aflag++;
		goto again;
	}
	if (argc > 0 && strcmp(*argv, "-p") == 0) {
		pflag++;
		goto again;
	}
	if (argc > 0 && strcmp(*argv, "-n") == 0) {
		nflag++;
		argc --, argv++;
		if (argc == 0)
			goto usage;
		nusers = atoi(*argv);
		isize = nusers * sizeof(u_short);
		/*
		 * round it off to the next BBSIZE
		 */
		isize = (isize + (512 -1)) & ~(512 - 1);
		if (isize < Q_MININDEX) {
			fprintf(stderr, "quotacheck: nusers value %d too small\n", nusers);
			exit(1);
		}
		if (isize > Q_MAXINDEX) {
			fprintf(stderr, "quotacheck: nusers value %d too big\n", nusers);
			exit(1);
		}
		goto again;
	}
	if (argc <= 0 && !aflag) {
usage:
		fprintf(stderr, "Usage:\n\t%s\n\t%s\n\t%s\n\t%s\n",
			"quotacheck [-v] [-p] -a",
			"quotacheck [-v] [-p] filesys ...",
			"quotacheck [-v] -n nusers -a",
			"quotacheck [-v] -n nusers filesys ...");
		exit(1);
	}

	if (getuid()) {
		fprintf(stderr, "%s: permission denied\n", arg0);
		exit(1);
	}
	if (quotactl(Q_SYNC, NULL, 0, NULL) < 0 && (errno == ENOPKG) && vflag) {
		perror("quotas");
		exit(1);
	}
	sync();

	if (nflag) {
		/*
		 * Don't preen for resizing of index.
		 */
		pflag = 0;
	} else {
		/*
		 * Don't need passwd entries for -n option
		 */
		(void) setpwent();
		while ((pw = getpwent()) != 0) {
			fup = lookup((u_short)pw->pw_uid);
			if (fup == 0) {
				fup = adduid((u_short)pw->pw_uid);
				strncpy(fup->fu_name, pw->pw_name,
					sizeof(fup->fu_name));
			}
		}
		(void) endpwent();
	}

	if (aflag) {
		/*
		 * Go through fstab and make a list of appropriate
		 * filesystems.
		 */
		listp = listbuf;
		listcnt = 0;
		if ((fstab = setmntent(MNTTAB, "r")) == NULL) {
			fprintf(stderr, "Can't open ");
			perror(MNTTAB);
			exit(8);
		}
		while (mntp = getmntent(fstab)) {
			if (strcmp(mntp->mnt_type, MNTTYPE_EFS) != 0 ||
			    !hasmntopt(mntp, MNTOPT_QUOTA) ||
			    hasmntopt(mntp, MNTOPT_RO))
				continue;
			*listp = malloc(strlen(mntp->mnt_fsname) + 1);
			strcpy(*listp, mntp->mnt_fsname);
			listp++;
			listcnt++;
		}
		endmntent(fstab);
		*listp = (char *)0;
		listp = listbuf;
	} else {
		listp = argv;
		listcnt = argc;
	}

	if (pflag) {
		errs = preen(listcnt, listp);
	} else {
		if ((mtab = setmntent(MOUNTED, "r")) == NULL) {
			fprintf(stderr, "Can't open ");
			perror(MOUNTED);
			exit(8);
		}
		while (mntp = getmntent(mtab)) {
			if (!(oneof(mntp->mnt_fsname, listp, listcnt) ||
			      oneof(mntp->mnt_dir, listp, listcnt))) 
				continue;

			if (hasmntopt(mntp, MNTOPT_RO)) {
				if (!aflag)
					fprintf(stderr, 
						"%s: Read only filesystem.\n",
						mntp->mnt_dir);
				continue;
			}
			if (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) {
				if (!aflag) 
					fprintf(stderr, 
						"%s: XFS filesystems do not need "
						"quotacheck.\n",
						mntp->mnt_dir);
				continue;
			}
		
			if (strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0) {
				/*
				 * to get "/quotas" instead of "//quotas"
				 */
				if (strcmp(mntp->mnt_dir, "/") == 0)
				    sprintf(quotafile, "/%s", QFNAME);
				else
				    sprintf(quotafile, "%s/%s",
					mntp->mnt_dir, QFNAME);
				if (nflag) 
					errs += bump_isize(mntp->mnt_fsname,
						mntp->mnt_dir, quotafile);
				else
					errs += chkquota(mntp->mnt_fsname,
					mntp->mnt_dir, quotafile);
			}
		}
		endmntent(mtab);
	}
	while (listcnt--) {
		if (*listp) {
			fprintf(stderr, "Cannot %s %s\n",
				nflag ? "resize index for" : "check", *listp);
		}
		listp++;
	}
	exit(errs);
	/* NOTREACHED */
}

int
preen(
	int listcnt,
	char **listp)
{
	register struct mntent *mntp;
	register int passno, anygtr;
	register int errs;
	FILE *mtab;
	union wait status;
	char quotafile[MAXPATHLEN + 1];

	passno = 0;
	errs = 0;
	if ((mtab = setmntent(MOUNTED, "r")) == NULL) {
		fprintf(stderr, "Can't open ");
		perror(MOUNTED);
		exit(8);
	}
	do {
		rewind(mtab);
		anygtr = 0;

		while (mntp = getmntent(mtab)) {
			if (mntp->mnt_passno > passno)
				anygtr = 1;

			if (mntp->mnt_passno != passno)
				continue;

			if (strcmp(mntp->mnt_type, MNTTYPE_EFS) != 0 ||
			    hasmntopt(mntp, MNTOPT_RO) ||
			    (!oneof(mntp->mnt_fsname, listp, listcnt) &&
			     !oneof(mntp->mnt_dir, listp, listcnt)) )
				continue;

			switch (fork()) {
			case -1:
				perror("fork");
				exit(8);
				break;

			case 0:
				if (strcmp(mntp->mnt_dir, "/") == 0)
				    sprintf(quotafile, "/%s", QFNAME);
				else
				    sprintf(quotafile, "%s/%s",
					mntp->mnt_dir, QFNAME);
				exit(chkquota(mntp->mnt_fsname,
					mntp->mnt_dir, quotafile));
			}
		}

		while (wait((int *)&status) != -1) 
			errs += status.w_retcode;

		passno++;
	} while (anygtr);
	endmntent(mtab);

	return (errs);
}

int
chkquota(
	char *fsdev,
	char *fsfile,
	char *qffile)
{
	register struct fileusage *fup;
	dev_t quotadev;
	int qfd = -1;
	unsigned long iblk;
	int cg, i;
	struct stat statb;
	struct dqblk dqbuf;
	int rval = 1;
	char *rawdev;
	cap_t ocap;
	cap_value_t cap_quota_mgt = CAP_QUOTA_MGT;

	if ((rawdev = findrawpath(fsdev)) == NULL) {
		fprintf(stderr, "Cannot find raw device for %s\n", fsdev);
		return(1);
	}
	if (vflag)
		printf("*** Checking quotas for %s (%s)\n", fsdev, fsfile);
	fi = open(rawdev, 0);
	if (fi < 0) {
		perror(rawdev);
		return (1);
	}
	if (((qfd = open(qffile, O_RDONLY)) == -1) || (fstat(qfd, &statb))) {
		perror(qffile);
		goto errexit;
	}
	if (statb.st_uid) {
		fprintf(stderr, "quotacheck: %s not owned by root\n", qffile);
		goto errexit;
	}
	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_ACTIVATE, fsdev, 0, qffile)) {
		cap_surrender(ocap);
		perror(qffile);
		goto errexit;
	}
	cap_surrender(ocap);
	quotadev = statb.st_dev;
	if (stat(fsdev, &statb) < 0) {
		perror(fsdev);
		goto errexit;
	}
	if (quotadev != statb.st_rdev) {
		fprintf(stderr, "%s dev (0x%x) mismatch %s dev (0x%x)\n",
			qffile, quotadev, fsdev, statb.st_rdev);
		goto errexit;
	}
	bread(EFS_SUPERBB, (char *)&sblock, BBSIZE);
	if (!IS_EFS_MAGIC(sblock.fs_magic)) {
	    fprintf(stderr,"quotacheck: bad magic number in super block for %s\n", fsdev);
	    goto errexit;
	}
	sblock.fs_ipcg = EFS_COMPUTE_IPCG(&sblock);
	if ((itab = (struct efs_dinode *)malloc(EFS_COMPUTE_IPCG(&sblock) *
			(1 << EFS_EFSINOSHIFT))) == NULL) {
		fprintf(stderr, "Couldn't malloc %d bytes for inode table for %s\n",
		    EFS_COMPUTE_IPCG(&sblock) * (1 << EFS_EFSINOSHIFT), fsdev);
		goto errexit;
	}
	ino = 0;
	for (cg = 0; cg < sblock.fs_ncg; cg++) {
		iblk = (unsigned long) EFS_ITOBB(&sblock, ino);
		bread((u_long)iblk, (char *)itab,
		    (EFS_COMPUTE_IPCG(&sblock) * (1 << EFS_EFSINOSHIFT)));
		for (i = 0; i < EFS_COMPUTE_IPCG(&sblock); i++) {
			if (ino++ < ROOTINO)
			    continue;
			acctEFS((struct efs_dinode *)itab + i);
		}
	}
	free(itab);
	itab = NULL;

	for (i=0; i < FUHASH; i++) {
	    for (fup=fuhead[i]; fup; fup = fup->fu_next) {
		ocap = cap_acquire(1, &cap_quota_mgt);
		if (quotactl(Q_GETQUOTA, fsdev, fup->fu_uid, (caddr_t)&dqbuf)) {
		    cap_surrender(ocap);
		    perror("Couldn't read quota");
		    goto errexit;
		}
		cap_surrender(ocap);
		if (!(dqbuf.dqb_bhardlimit || dqbuf.dqb_bsoftlimit ||
			dqbuf.dqb_fhardlimit || dqbuf.dqb_fsoftlimit)) {
		    fup->fu_curfiles = 0;
		    fup->fu_curblocks = 0;
		}
		if (dqbuf.dqb_curfiles == fup->fu_curfiles &&
		    dqbuf.dqb_curblocks == fup->fu_curblocks) {
			fup->fu_curfiles = 0;
			fup->fu_curblocks = 0;
			continue;
		}
		if (vflag) {
			if (pflag || aflag)
				printf("%s: ", fsdev);
			if (fup->fu_name[0] != '\0')
				printf("%-10s fixed:", fup->fu_name);
			else
				printf("#%-9d fixed:", (int) fup->fu_uid);
			if (dqbuf.dqb_curfiles != fup->fu_curfiles)
				printf("  files %d -> %d",
				    (int) dqbuf.dqb_curfiles, (int) fup->fu_curfiles);
			if (dqbuf.dqb_curblocks != fup->fu_curblocks)
				printf("  kbytes %d -> %d",
				    kb(dqbuf.dqb_curblocks),
				    kb(fup->fu_curblocks));
			printf("\n");
		}
		if ((dqbuf.dqb_curfiles != fup->fu_curfiles) ||
		    (dqbuf.dqb_curblocks != fup->fu_curblocks)) {
		    dqbuf.dqb_curfiles = fup->fu_curfiles;
		    dqbuf.dqb_curblocks = fup->fu_curblocks;
		    ocap = cap_acquire(1, &cap_quota_mgt);
		    if (quotactl(Q_SETQUOTA, fsdev, fup->fu_uid, (caddr_t)&dqbuf)) {
			cap_surrender(ocap);
			fprintf(stderr,"Couldn't write quota for %d in %s\n",
				fup->fu_uid, qffile);
			goto errexit;
		    }
		    cap_surrender(ocap);
		}
		fup->fu_curfiles = 0;
		fup->fu_curblocks = 0;
	    }
	}
	rval = 0;
errexit:
	if (qfd != -1) {
		(void) fsync(qfd);
		(void) close(qfd);
	}
	close(fi);
	return (rval);
}

static void
acctEFS(
	struct efs_dinode *ip)
{
	struct fileusage *fup;
	int i, niex;

	if (ip == NULL)
		return;
	if (ip->di_mode == 0)
		return;
	fup = adduid((u_short)ip->di_uid);
	fup->fu_curfiles++;
	if ((ip->di_mode & IFMT) == IFCHR || (ip->di_mode & IFMT) == IFBLK)
		return;
	fup->fu_curblocks += BTOBB(ip->di_size);
	/*
	 * Account for indirect extent blocks
	 */
	if (ip->di_numextents > EFS_DIRECTEXTENTS) {
		if ((niex = ip->di_u.di_extents[0].ex_offset) >
				EFS_DIRECTEXTENTS)
			return;
		for (i = 0; i < niex; i++)
		    fup->fu_curblocks += (u_int)ip->di_u.di_extents[i].ex_length;
	}
}

int
oneof(
	char *target,
	char **listp,
	int n)
{

	while (n--) {
		if (*listp && strcmp(target, *listp) == 0) {
			*listp = (char *)0;
			return (1);
		}
		listp++;
	}
	return (0);
}

static void
bread(
	long unsigned bno,
	char *buf,
	int cnt)
{
	if (syssgi(SGI_READB, fi, buf, bno, BTOBBT(cnt)) != BTOBBT(cnt)) {
		perror("syssgi");
		fprintf(stderr, "quotacheck: read error at block %u\n", bno);
		exit(1);
	}
}

struct fileusage *
lookup(
	u_short uid)
{
	register struct fileusage *fup;

	for (fup = fuhead[uid % FUHASH]; fup != 0; fup = fup->fu_next)
		if (fup->fu_uid == uid)
			return (fup);
	return ((struct fileusage *)0);
}

struct fileusage *
adduid(
	u_short uid)
{
	struct fileusage *fup, **fhp;

	fup = lookup(uid);
	if (fup != 0)
		return (fup);
	fup = (struct fileusage *)calloc(1, sizeof(struct fileusage));
	if (fup == 0) {
		fprintf(stderr, "out of memory for fileusage structures\n");
		exit(1);
	}
	fhp = &fuhead[uid % FUHASH];
	fup->fu_next = *fhp;
	*fhp = fup;
	fup->fu_uid = uid;
	fup->fu_curfiles = fup->fu_curblocks = 0;
	return (fup);
}

int
bump_isize(
	char *fsdev,
	char *fsfile,
	char *qffile)
{
	dev_t quotadev;
	int tmpfd = -1;
	int unlink_tmp = 1;
	int qfd = -1;
	struct stat statb;
	int wason = 0;
	char tmpbuf[MAXPATHLEN+1];
	char *tmpname = &tmpbuf[0];
	struct qt_header qth;
	int qfsize;
	char buffer[4096];
	int rval = 1;
	int copysize, wsize, oisize;
	cap_t ocap;
	cap_value_t cap_quota_mgt = CAP_QUOTA_MGT;

	(void) sprintf(tmpname, "%s/%s", fsfile, "quotachecktmpXXXXXX");
	if (vflag)
		printf("*** Resizing index for %s (%s)\n", fsdev, fsfile);
	if (isize == Q_MININDEX)
		return(0);
	fi = open(fsdev, 0);
	if (fi < 0) {
		perror(fsdev);
		return (1);
	}
	if (((qfd = open(qffile, O_RDONLY)) == -1) || (fstat(qfd, &statb))) {
		perror(qffile);
		goto eexit;
	}
	if (statb.st_uid) {
		fprintf(stderr, "quotacheck: %s not owned by root\n", qffile);
		goto eexit;
	}
	quotadev = statb.st_dev;
	if (stat(fsdev, &statb) < 0) {
		perror(fsdev);
		goto eexit;
	}
	if (quotadev != statb.st_rdev) {
		fprintf(stderr, "%s dev (0x%x) mismatch %s dev (0x%x)\n",
			qffile, quotadev, fsdev, statb.st_rdev);
		goto eexit;
	}
	if ((tmpfd = mkstemp(tmpname)) == -1) {
		fprintf(stderr, "Cannot create temp file %s\n", tmpname);
		goto eexit;
	}
	if (fchmod(tmpfd, statb.st_mode & 0777)) {
		perror(tmpname);
		goto eexit;
	}

	/*
	 * Turn quotaoff so that all quotas get synced to disk and get file
	 * size.
	 */
	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_QUOTAOFF, fsdev, 0, NULL)) {
		if (errno != EEXIST) {
			cap_surrender(ocap);
			perror(qffile);
			goto eexit;
		}
	} else
		wason++;
	cap_surrender(ocap);
	if (fstat(qfd, &statb)) {
		perror("stat");
		goto eexit;
	}
	qfsize = statb.st_size;

	/*
	 * Quotas is now turned off for the filesystem. Copy the existing
	 * quotas file to the new tmp file, incrementing the index size as
	 * we do so. wason indicates whether to turn quotas back on, once
	 * we are done.
	 */
	if ((read(qfd, &qth, Q_HEADER)) != Q_HEADER) {
		perror("read");
		goto eexit;
	}

	/*
	 * Sanity check the quotas header
	 */
	if (qth.qh_magic != Q_MAGIC) {
		fprintf(stderr, "Illegal magic number 0x%x in %s\n",
			qth.qh_magic, qffile);
		goto eexit;
	}
	if (isize == qth.qh_index)
		goto clnexit;
	if (isize < qth.qh_index) {
		fprintf(stderr, "Cannot shrink quotas index for %s\n", qffile);
		goto eexit;
	}
	if ((qth.qh_index + qth.qh_nusers * sizeof(struct dqblk)) != qfsize) {
		fprintf(stderr, "Corrupted quotas file %s\n", qffile);
		goto eexit;
	}
	if (qth.qh_index > qfsize) {
		fprintf(stderr, "Bogus index size in %s\n", qffile);
		goto eexit;
	}

	oisize = qth.qh_index;
	qth.qh_index = isize;
	if ((write(tmpfd, &qth, Q_HEADER)) != Q_HEADER) {
		perror("write");
		goto eexit;
	}
	/*
	 * Now copy the index
	 */
	wsize = oisize - Q_HEADER;	/* copy wsize from old qffile */
	while (wsize > 0) {
		copysize = wsize < sizeof(buffer) ? wsize : sizeof(buffer);
		if ((read(qfd, buffer, copysize)) != copysize) {
			perror("read");
			goto eexit;
		}
		if ((write(tmpfd, buffer, copysize)) != copysize) {
			perror("write");
			goto eexit;
		}
		wsize -= copysize;
	}

	/*
	 * Fill in with zeroes the increased size of the index.
	 */
	bzero((char *)buffer, sizeof(buffer));
	wsize = isize - oisize;
	while (wsize > 0) {
		copysize = wsize < sizeof(buffer) ? wsize : sizeof(buffer);
		if ((write(tmpfd, buffer, copysize)) != copysize) {
			perror("write");
			goto eexit;
		}
		wsize -= copysize;
	}

	/*
	 * Copy the quotas information from the old qffile.
	 */
	wsize = qfsize - oisize;
	while (wsize > 0) {
		copysize = wsize < sizeof(buffer) ? wsize : sizeof(buffer);
		if ((read(qfd, buffer, copysize)) != copysize) {
			perror("read");
			goto eexit;
		}
		if ((write(tmpfd, buffer, copysize)) != copysize) {
			perror("write");
			goto eexit;
		}
		wsize -= copysize;
	}

	fsync(tmpfd);
	unlink_tmp = 0;
	close(tmpfd); tmpfd = -1;
	close(qfd); qfd = -1;

	if (rename(tmpname, qffile) < 0) {
		perror(qffile);
		goto eexit;
	}

clnexit:
	rval = 0;			/* No error */
eexit:
	if (qfd != -1)
		close(qfd);
	if (tmpfd != -1) {
		close(tmpfd);
		if (unlink_tmp)
			unlink(tmpname);
	}
	close(fi);
	if (wason) {
		ocap = cap_acquire(1, &cap_quota_mgt);
		if (quotactl(Q_QUOTAON, fsdev, 0, qffile)) {
			perror(fsdev);
			fprintf(stderr, "Couldn't re-enable quotas.\n");
		}
		cap_surrender(ocap);
	}
	return (rval);
}
