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
 * "@(#)quota.c	1.3 88/08/03 4.0NFSSRC SMI";
 */

#ident  "$Revision: 1.20 $"
/*
 * Disk quota reporting program, for both EFS and XFS 
 * file systems
 */
#include <stdio.h>
#include <mntent.h>
#include <ctype.h>
#include <sys/types.h>
#include <paths.h>
#include <proj.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/quota.h>
#include <sys/statvfs.h>
#include <sys/capability.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rpcsvc/rquota.h>
#include <strings.h>

int	vflag;
int	nonfs;
int	nolocalquota;
int	prjquota;
char 	prjname[MAXPROJNAMELEN];
char	*whoami;

#define QFNAME	"quotas"

#define DEV_BSIZE BBSIZE
#define BBTOOFF64(bbs)	((__uint64_t)(bbs) << BBSHIFT)
#define BBTOKB(n)	(howmany(BBTOOFF64(n), 0x400ULL))
#define BBTOKB_INT(n) ((int) BBTOKB(n))
#define BBTOKB_ULL(n) ((unsigned long long) BBTOKB(n))
#define kb(n)	       ((int) howmany(BBTOB(n), 0x400ULL))

typedef enum {
  FS_NONE, FS_XFS, FS_EFS, FS_NFS
} quota_fstype_t;

static void	showuid(int uid);
static void	showname(char *name);
static void	showquotas(int uid,char * name);
static void	showprojid(prid_t prid);
static void	showprojname(char *name);
static void	showprojquotas(int prid,char * name);
static void	warnxfs(struct mntent *mntp, fs_disk_quota_t *d, int fsbsz);
static void	warn(register struct mntent *mntp, register struct dqblk *dqp);
static int	prxfsquota(struct mntent *mntp, fs_disk_quota_t *d);
static void	prquota(register struct mntent *mntp,register struct dqblk *dqp);
static void	fmttime(char *buf, register long ftime);
static int	alldigits(register char *s);
static int	getdiskquota(struct mntent *mntp, int uid, struct dqblk *dqp);
static int	getnfsquota(struct mntent *mntp, int uid, struct dqblk *dqp);
static void	heading(int id, char *name);
static void	usage(void);
static int	callaurpc(char *host, int prognum, int versnum, int procnum,
			  xdrproc_t inproc, char *in, xdrproc_t outproc, 
			  char *out);
static AUTH *authunix_create_local(void);


main(argc, argv)
	int argc;
	char *argv[];
{
	register char *cp;
	nonfs = 0;
	vflag = 0;
	prjquota = 0;
	whoami = argv[0];

	argc--,argv++;
	while (argc > 0) {
		if (argv[0][0] == '-')
			for (cp = &argv[0][1]; *cp; cp++) switch (*cp) {

			case 'n':
				nonfs++;
				break;
#ifndef _NOPROJQUOTAS				
			case 'p':
				prjquota++;
				break;
#endif
			case 'v':
				vflag++;
				break;

			default:
				fprintf(stderr, "quota: %c: unknown option\n",
					*cp);
				usage();
			}
		else
			break;
		argc--, argv++;
	}
	/* 
	 * No need to do this for XFS filesystems
	 */
	if (quotactl(Q_SYNC, NULL, 0, NULL) < 0 && (errno == ENOPKG)) {
		perror("quotas");
		if (nonfs)		/* nothing to print out */
			exit(1);
		nolocalquota++;
	}
	if (argc == 0) {
		if (prjquota)
			showprojid(getprid());
		else
			showuid(getuid());
		exit(0);
	}
	for (; argc > 0; argc--, argv++) {
		if (alldigits(*argv)) {
			/*
			 * Project IDs are truncated to 32bits for quota
			 * purposes. In fact, they are just 16 bits in the inode.
			 */
			if (prjquota)
				showprojid(atoi(*argv));
			else
				showuid(atoi(*argv));
		} else {
			if (prjquota)
				showprojname(*argv);
			else
				showname(*argv);
		}
	}
	exit(0);
	/* NOTREACHED */
}

static void
showuid(int uid)
{
	struct passwd *pwd = getpwuid(uid);

	if (pwd == NULL)
		showquotas(uid, "(no account)");
	else
		showquotas(uid, pwd->pw_name);
}

static void
showprojid(prid_t prid)
{
	(void) projname(prid, prjname, MAXPROJNAMELEN);

	if (prjname[0] == '\0')
		showprojquotas((int) prid, "(no account)");
	else
		showprojquotas((int) prid, prjname);
}

static void
showname(char *name)
{
	struct passwd *pwd = getpwnam(name);

	if (pwd == NULL) {
		fprintf(stderr, "quota: %s: unknown user\n", name);
		return;
	}
	showquotas(pwd->pw_uid, name);
}

static void
showprojname(char *name)
{
	prid_t	prid;
	if ((prid = projid(name)) < 0) {
		fprintf(stderr, "quota: %s: unknown project\n", name);
		return;
	}
	
	showprojquotas((int) prid, name);
}

static void
showquotas(int uid,char * name)
{
        register struct mntent 	*mntp;
        FILE 			*mtab;
	struct dqblk 		dqblk;
	fs_disk_quota_t 	xfsdqblk;
	quota_fstype_t 		type;
	int 			myuid;
	int 			printed, warned;
	struct statvfs 		vfsstat;
	cap_t			ocap;
	cap_value_t		cap_quota_mgt = CAP_QUOTA_MGT;

	myuid = getuid();
	if (uid != myuid && myuid != 0) {
		printf("quota: %s (uid %d): permission denied\n", name, uid);
		return;
	}
	warned = printed = 0;
	type = FS_NONE;
	mtab = setmntent(MOUNTED, "r");

	while (mntp = getmntent(mtab)) {
	        if (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) {
			ocap = cap_acquire(1, &cap_quota_mgt);
			if (quotactl(Q_XGETQUOTA,
				      mntp->mnt_fsname, uid, 
				      (caddr_t)&xfsdqblk) != 0) {
				cap_surrender(ocap);
				continue;
			}
			cap_surrender(ocap);
			type = FS_XFS;

		} else if (strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0) {
			if (uid == 0) {
				if (vflag)
					printf(
				"No disk quota for %s (uid 0) :%s\n",
					       name, mntp->mnt_dir);
				warned++;
				continue;
			}
			ocap = cap_acquire(1, &cap_quota_mgt);
			if (nolocalquota ||
			    (quotactl(Q_GETQUOTA,
				      mntp->mnt_fsname, uid, 
				      (caddr_t)&dqblk) != 0 &&
			     !(vflag && getdiskquota(mntp, uid, &dqblk)))) {
				cap_surrender(ocap);
				continue;
			}
			cap_surrender(ocap);

			type = FS_EFS;
		} else if ((strcmp(mntp->mnt_type, MNTTYPE_NFS) == 0)  ||
			   (strcmp(mntp->mnt_type, MNTTYPE_NFS3) == 0) ||
			   (strcmp(mntp->mnt_type, MNTTYPE_NFS2) == 0)) {
			/*
			 * If we have been asked not to query nfs filesystems
			 * or if the /etc/mtab entry has not been mounted with
			 * the "quota" option or has been mounted with the
			 * "noquota" option we do not query the nfs server.
			 * This reduces the possibility that we hang waiting
			 * for a reply.
			 */
			if (nonfs || hasmntopt(mntp, MNTOPT_NOQUOTA))
				continue;
			if (! (hasmntopt(mntp, MNTOPT_QUOTA) ||
			       hasmntopt(mntp, MNTOPT_UQUOTA) ||
			       hasmntopt(mntp, MNTOPT_PQUOTA) ||
			       hasmntopt(mntp, MNTOPT_QUOTANOENF) ||
			       hasmntopt(mntp, MNTOPT_UQUOTANOENF) ||
			       hasmntopt(mntp, MNTOPT_PQUOTANOENF)))
				continue;
			if (!getnfsquota(mntp, uid, &dqblk))
				continue;
			type = FS_NFS;
		} else {
			continue;
		}
		/*
		 * EFS doesn't do any accounting when limits aren't set. 
		 * XFS does, both locally and over NFS.
		 */
		if (type == FS_EFS &&
		    dqblk.dqb_bsoftlimit == 0 && dqblk.dqb_bhardlimit == 0 &&
		    dqblk.dqb_fsoftlimit == 0 && dqblk.dqb_fhardlimit == 0) {
		        type = FS_NONE;
			continue;
                }
		if (vflag) {
			if (!printed) {
				heading(uid, name);
				printed++;
			}
			if (type == FS_XFS)
				prxfsquota(mntp, &xfsdqblk);
			else
				prquota(mntp, &dqblk);
		} else {
			
			if (type == FS_XFS) {
				/*
				 * We need to find the filesystem blocksize.
				 * We don't care to use the statvfs64 for that.
				 */
				if (statvfs(mntp->mnt_dir, &vfsstat) < 0) {
					printf("quota: Can't stat %s\n",
					       mntp->mnt_dir);
					continue;
				}
				warnxfs(mntp, &xfsdqblk, 
					(int) BTOBB(vfsstat.f_bsize));
			} else {
				warn(mntp, &dqblk);
			}
                }
	        type = FS_NONE;

	}
	endmntent(mtab);
	if (vflag && !warned && !printed)
		printf("No quotas for %s (uid %d)\n", name, uid);
}

static void
showprojquotas(int prid,char * name)
{
        register struct mntent 	*mntp;
        FILE 			*mtab;
	fs_disk_quota_t 	xfsdqblk;
	int 			myprid;
	int 			printed = 0;
	struct statvfs 		vfsstat;
	cap_t			ocap;
	cap_value_t		cap_quota_mgt = CAP_QUOTA_MGT;

	myprid = (int) getprid();
	if (myprid != prid && getuid() != 0) {
		printf("quota: %s (projid %d): permission denied\n", name, prid);
		return;
	}
	mtab = setmntent(MOUNTED, "r");
	while (mntp = getmntent(mtab)) {
	        if (strcmp(mntp->mnt_type, MNTTYPE_XFS) != 0)
			continue;

		ocap = cap_acquire(1, &cap_quota_mgt);
		if (quotactl(Q_XGETPQUOTA,
			      mntp->mnt_fsname, prid, 
			      (caddr_t)&xfsdqblk) != 0) {
			cap_surrender(ocap);
			continue;
		}
		cap_surrender(ocap);

		if (vflag) {
			if (!printed) {
				heading(prid, name);
				printed++;
			}
			prxfsquota(mntp, &xfsdqblk);
		} else {
			if (statvfs(mntp->mnt_dir, &vfsstat) < 0) {
				printf("Can't stat %s\n", mntp->mnt_dir);
				continue;
			}
			warnxfs(mntp, &xfsdqblk, (int) BTOBB(vfsstat.f_bsize));
                }
	}
	endmntent(mtab);
	if (vflag && !printed)
		printf("No quotas for %s (projid %d)\n", name, prid);
}

#define X_BSOFT(d)	(d)->d_blk_softlimit
#define X_BHARD(d)	(d)->d_blk_hardlimit
#define X_ISOFT(d)	(d)->d_ino_softlimit
#define X_IHARD(d)	(d)->d_ino_hardlimit
#define X_BCOUNT(d)	(d)->d_bcount
#define X_ICOUNT(d)	(d)->d_icount
#define X_BTIMER(d)	(d)->d_btimer
#define X_ITIMER(d)	(d)->d_itimer

#define IDIFF(d)		(X_ICOUNT(d) - X_ISOFT(d) + 1)
#define BDIFF(d, fsb)		(roundup((X_BCOUNT(d) - X_BSOFT(d) + 1), (fsb)))
#define ICOUNT_EXCESS(d)	((unsigned long long)(IDIFF(d)))
#define BCOUNT_EXCESS(d, fsb)	(BBTOKB_ULL(BDIFF((d), (__uint64_t)(fsb))))
#define PLURAL_IEXCESS_TO_STR(d) (IDIFF(d) > 1 ? "s" : "")

static void
warnxfs(
	struct mntent 	*mntp,
	fs_disk_quota_t *d,
	int		fsbsz) /* FSB size in BBs */
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	
	if (d->d_id == 0)
		return;

	if (X_BHARD(d) && X_BCOUNT(d) >= X_BHARD(d)) {
		printf(
		       "Disk limit reached on %s\n",
		       mntp->mnt_dir
		       );
	} else if (X_BSOFT(d) && X_BCOUNT(d) >= X_BSOFT(d)) {
		if (X_BTIMER(d) == 0) {
			printf(
			       "Over disk quota on %s, remove %lluK\n",
			       mntp->mnt_dir,
			       BCOUNT_EXCESS(d, fsbsz)
			       );
		} else if (X_BTIMER(d) > tv.tv_sec) {
			char btimeleft[80];

			fmttime(btimeleft, X_BTIMER(d) - tv.tv_sec);
			printf(
			       "Over disk quota on %s, remove %lluK within %s\n",
			       mntp->mnt_dir,
			       BCOUNT_EXCESS(d, fsbsz),
			       btimeleft
			);
		} else {
			printf(
			       "Over disk quota on %s, time limit has expired, "
			       "remove %lluK\n",
			       mntp->mnt_dir,
			       BCOUNT_EXCESS(d, fsbsz)
			);
		}
	}

	if (X_IHARD(d) && X_ICOUNT(d) >= X_IHARD(d)) {
		printf(
		      "File count limit reached on %s\n",
		       mntp->mnt_dir
		       );
	} else if (X_ISOFT(d) && 
		   (X_ICOUNT(d) >= X_ISOFT(d))) {
		if (X_ITIMER(d) == 0) {
			printf(
			       "Over file quota on %s, remove %llu file%s\n",
			       mntp->mnt_dir,
			       ICOUNT_EXCESS(d),
			       PLURAL_IEXCESS_TO_STR(d)
			);
		} else if (X_ITIMER(d) > tv.tv_sec) {
			char ftimeleft[80];

			fmttime(ftimeleft, X_ITIMER(d) - tv.tv_sec);
			printf(
			       "Over file quota on %s, remove %llu file%s "
			       "within %s\n",
			       mntp->mnt_dir,
			       ICOUNT_EXCESS(d),
			       PLURAL_IEXCESS_TO_STR(d),
			       ftimeleft
			);
		} else {
			printf(
			       "Over file quota on %s, time limit has expired, "
			       "remove %llu file%s\n",
			       mntp->mnt_dir,
			       ICOUNT_EXCESS(d),
			       PLURAL_IEXCESS_TO_STR(d)
			);
		}
	}
}

static void
warn(
     register struct mntent *mntp,
     register struct dqblk *dqp)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	if (dqp->dqb_bhardlimit &&
	     dqp->dqb_curblocks >= dqp->dqb_bhardlimit) {
		printf(
"Disk limit reached on %s\n",
		    mntp->mnt_dir
		);
	} else if (dqp->dqb_bsoftlimit &&
	     dqp->dqb_curblocks >= dqp->dqb_bsoftlimit) {
		if (dqp->dqb_btimelimit == 0) {
			printf(
"Over disk quota on %s, remove %dK\n",
			    mntp->mnt_dir,
			    BBTOKB_INT(dqp->dqb_curblocks - dqp->dqb_bsoftlimit + 1)
			);
		} else if (dqp->dqb_btimelimit > tv.tv_sec) {
			char btimeleft[80];

			fmttime(btimeleft, 
				(long) (dqp->dqb_btimelimit - tv.tv_sec));
			printf(
"Over disk quota on %s, remove %dK within %s\n",
			    mntp->mnt_dir,
			    BBTOKB_INT(dqp->dqb_curblocks - dqp->dqb_bsoftlimit + 1),
			    btimeleft
			);
		} else {
			printf(
"Over disk quota on %s, time limit has expired, remove %dK\n",
			    mntp->mnt_dir,
			    BBTOKB_INT(dqp->dqb_curblocks - dqp->dqb_bsoftlimit + 1)
			);
		}
	}
	if (dqp->dqb_fhardlimit &&
	    dqp->dqb_curfiles >= dqp->dqb_fhardlimit) {
		printf(
"File count limit reached on %s\n",
		    mntp->mnt_dir
		);
	} else if (dqp->dqb_fsoftlimit &&
	    dqp->dqb_curfiles >= dqp->dqb_fsoftlimit) {
		if (dqp->dqb_ftimelimit == 0) {
			printf(
"Over file quota on %s, remove %d file%s\n",
			    mntp->mnt_dir,
			    (int) (dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1),
			    ((dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1) > 1 ?
				"s" :
				"" )
			);
		} else if (dqp->dqb_ftimelimit > tv.tv_sec) {
			char ftimeleft[80];

			fmttime(ftimeleft, 
				(long) (dqp->dqb_ftimelimit - tv.tv_sec));
			printf(
"Over file quota on %s, remove %d file%s within %s\n",
			    mntp->mnt_dir,
			    (int) (dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1),
			    ((dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1) > 1 ?
				"s" :
				"" ),
			    ftimeleft
			);
		} else {
			printf(
"Over file quota on %s, time limit has expired, remove %d file%s\n",
			    mntp->mnt_dir,
			    (int) (dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1),
			    ((dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1) > 1 ?
				"s" :
				"" )
			);
		}
	}
}

static void
heading(int id, char *name)
{
	if (prjquota)
		printf("Disk quotas for %s (projid %d):\n", name, id);
	else
		printf("Disk quotas for %s (uid %d):\n", name, id);

	printf("%-12s%7s %7s %7s%12s%7s %7s %7s%12s\n"
		, "Filesystem"
		, "usage"
		, "quota"
		, "limit"
		, "timeleft"
		, "files"
		, "quota"
		, "limit"
		, "timeleft"
	);
}

static int
prxfsquota(
	struct mntent 	*mntp,
	fs_disk_quota_t *d)
{

	struct timeval tv;
	char ftimeleft[80], btimeleft[80];
	char *cp;

	gettimeofday(&tv, NULL);
	/*
	 * The blk count first.
	 */
	if (X_BSOFT(d) && X_BCOUNT(d) >= X_BSOFT(d)) {
		if (X_BTIMER(d) == 0) {
			strcpy(btimeleft, "NOT STARTED");
		} else if (X_BTIMER(d) > tv.tv_sec) {
			fmttime(btimeleft, X_BTIMER(d) - tv.tv_sec);
		} else {
			strcpy(btimeleft, "EXPIRED");
		}
	} else {
		btimeleft[0] = '\0';
	}
	
	/*
	 * inode count
	 */
	if (X_ISOFT(d) && X_ICOUNT(d) >= X_ISOFT(d)) {
		if (X_ITIMER(d) == 0) {
			strcpy(ftimeleft, "NOT STARTED");
		} else if (X_ITIMER(d) > tv.tv_sec) {
			fmttime(ftimeleft, X_ITIMER(d) - tv.tv_sec);
		} else {
			strcpy(ftimeleft, "EXPIRED");
		}
	} else {
		ftimeleft[0] = '\0';
	}


	if (strlen(mntp->mnt_dir) > 12) {
		printf("%s\n", mntp->mnt_dir);
		cp = "";
	} else {
		cp = mntp->mnt_dir;
	}

	printf("%-12.12s%7llu %7llu %7llu%12s%7llu %7llu %7llu%12s\n",
	    cp,
	    BBTOKB_ULL(X_BCOUNT(d)),
	    BBTOKB_ULL(X_BSOFT(d)),
	    BBTOKB_ULL(X_BHARD(d)),
	    btimeleft,
	    (unsigned long long) X_ICOUNT(d),
	    (unsigned long long) X_ISOFT(d),
	    (unsigned long long) X_IHARD(d),
	    ftimeleft
	);
	return (0);
}

static void
prquota(
	register struct mntent *mntp,
	register struct dqblk *dqp)
{
	struct timeval tv;
	char ftimeleft[80], btimeleft[80];
	char *cp;

	gettimeofday(&tv, NULL);
	if (dqp->dqb_bsoftlimit && dqp->dqb_curblocks >= dqp->dqb_bsoftlimit) {
		if (dqp->dqb_btimelimit == 0) {
			strcpy(btimeleft, "NOT STARTED");
		} else if (dqp->dqb_btimelimit > tv.tv_sec) {
			fmttime(btimeleft, 
				(long)(dqp->dqb_btimelimit - tv.tv_sec));
		} else {
			strcpy(btimeleft, "EXPIRED");
		}
	} else {
		btimeleft[0] = '\0';
	}
	if (dqp->dqb_fsoftlimit && dqp->dqb_curfiles >= dqp->dqb_fsoftlimit) {
		if (dqp->dqb_ftimelimit == 0) {
			strcpy(ftimeleft, "NOT STARTED");
		} else if (dqp->dqb_ftimelimit > tv.tv_sec) {
			fmttime(ftimeleft, 
				(long) (dqp->dqb_ftimelimit - tv.tv_sec));
		} else {
			strcpy(ftimeleft, "EXPIRED");
		}
	} else {
		ftimeleft[0] = '\0';
	}
	if (strlen(mntp->mnt_dir) > 12) {
		printf("%s\n", mntp->mnt_dir);
		cp = "";
	} else {
		cp = mntp->mnt_dir;
	}
	printf("%-12.12s%7d %7d %7d%12s%7d %7d %7d%12s\n",
	    cp,
	    BBTOKB_INT(dqp->dqb_curblocks),
	    BBTOKB_INT(dqp->dqb_bsoftlimit),
	    BBTOKB_INT(dqp->dqb_bhardlimit),
	    btimeleft,
	    (int) dqp->dqb_curfiles,
	    (int) dqp->dqb_fsoftlimit,
	    (int) dqp->dqb_fhardlimit,
	    ftimeleft
	);
}

static void
fmttime(
	char *buf,
	register long ftime)
{
	int i;
	static struct {
		int c_secs;		/* conversion units in secs */
		char * c_str;		/* unit string */
	} cunits [] = {
		{60*60*24*28, "months"},
		{60*60*24*7, "weeks"},
		{60*60*24, "days"},
		{60*60, "hours"},
		{60, "mins"},
		{1, "secs"}
	};

	if (ftime <= 0) {
		strcpy(buf, "EXPIRED");
		return;
	}
	for (i = 0; i < sizeof(cunits)/sizeof(cunits[0]); i++) {
		if (ftime >= cunits[i].c_secs)
			break;
	}
	sprintf(buf, "%.1f %s", (double)ftime/cunits[i].c_secs, cunits[i].c_str);
}

static int
alldigits(
	register char *s)
{
	register c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while (c = *s++);
	return (1);
}

/*
 * There's no way to Q_ACTIVATE in XFS. If quota's OFF,
 * the contents of dquot structures are undefined in XFS.
 * There's no need to do a Q_ACTIVATE in XFS anyway, because one can
 * switch ON quota accounting, while quota enforcement is OFF.
 * So, this routine only applies to non-XFS fs's...
 */
static int
getdiskquota(
	struct mntent *mntp,
	int uid,
	struct dqblk *dqp)
{
	dev_t fsdev;
	struct stat statb;
	char qfilename[MAXPATHLEN];
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;

	if (stat(mntp->mnt_fsname, &statb) < 0 ||
	    (statb.st_mode & S_IFMT) != S_IFBLK)
		return (0);
	fsdev = statb.st_rdev;
	sprintf(qfilename, "%s/%s", mntp->mnt_dir, QFNAME);
	if (stat(qfilename, &statb) < 0 || statb.st_dev != fsdev)
		return (0);
	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_ACTIVATE, mntp->mnt_fsname, 0, qfilename) != 0) {
		cap_surrender(ocap);
		return(0);
	}
	cap_surrender(ocap);
	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_GETQUOTA, mntp->mnt_fsname, uid, (caddr_t)dqp)) {
		cap_surrender(ocap);
		return(0);
	}
	cap_surrender(ocap);
	return(1);
}


static int
callaurpc(char *host, int prognum, int versnum, int procnum, xdrproc_t inproc, char *in, xdrproc_t outproc, char *out)
{
	struct sockaddr_in server_addr;
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct timeval timeout, tottimeout;

	static CLIENT *client = NULL;
	static int mysocket = RPC_ANYSOCK;
	static int valid = 0;
	static int oldprognum, oldversnum;
	static char oldhost[256];

	if (valid && oldprognum == prognum && oldversnum == versnum
		&& strcmp(oldhost, host) == 0) {
		/* reuse old client */		
	}
	else {
		valid = 0;
		close(mysocket);
		mysocket = RPC_ANYSOCK;
		if (client) {
			clnt_destroy(client);
			client = NULL;
		}
		if ((hp = gethostbyname(host)) == NULL)
			return ((int) RPC_UNKNOWNHOST);
		timeout.tv_usec = 0;
		timeout.tv_sec = 6;
		bcopy(hp->h_addr, &server_addr.sin_addr, hp->h_length);
		server_addr.sin_family = AF_INET;
		/* ping the remote end via tcp to see if it is up */
		server_addr.sin_port =  htons(PMAPPORT);
		if ((client = clnttcp_create(&server_addr, PMAPPROG,
		    PMAPVERS, &mysocket, 0, 0)) == NULL) {
			return ((int) rpc_createerr.cf_stat);
		} else {
			/* the fact we succeeded means the machine is up */
			close(mysocket);
			mysocket = RPC_ANYSOCK;
			clnt_destroy(client);
			client = NULL;
		}
		/* now really create a udp client handle */
		server_addr.sin_port =  0;
		if ((client = clntudp_create(&server_addr, prognum,
		    versnum, timeout, &mysocket)) == NULL)
			return ((int) rpc_createerr.cf_stat);
		client->cl_auth = authunix_create_local();
		valid = 1;
		oldprognum = prognum;
		oldversnum = versnum;
		strcpy(oldhost, host);
	}
	tottimeout.tv_sec = 25;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(client, procnum, inproc, in,
	    outproc, out, tottimeout);
	/* 
	 * if call failed, empty cache
	 */
	if (clnt_stat != RPC_SUCCESS)
		valid = 0;
	return ((int) clnt_stat);
}

static AUTH *
authunix_create_local(void)
{
	char machname[MAX_MACHINE_NAME + 1];
	register uid_t uid;
	register gid_t gid;

	if (gethostname(machname, MAX_MACHINE_NAME) == -1) {
		fprintf(stderr, 
			"authsys_create_local:  gethostname failed:  %s",
			machname);
		abort();
	}
	machname[MAX_MACHINE_NAME] = 0;
	uid = geteuid();
	gid = getegid();
	return (authunix_create(machname, uid, gid, 0, NULL));
}


static int
getnfsquota(
	struct mntent *mntp,
	int uid,
	struct dqblk *dqp)
{
	char *hostp;
	char *cp;
	struct getquota_args gq_args;
	struct getquota_rslt gq_rslt;
	struct timeval tv;

	hostp = mntp->mnt_fsname;
	cp = index(mntp->mnt_fsname, ':');
	if (cp == 0) {
		fprintf(stderr, "cannot find hostname for %s\n", mntp->mnt_dir);
		return (0);
	}
	*cp = '\0';
	gq_args.gqa_pathp = cp + 1;
	gq_args.gqa_uid = uid;
	if (callaurpc(hostp, RQUOTAPROG, RQUOTAVERS,
		      (vflag? RQUOTAPROC_GETQUOTA: RQUOTAPROC_GETACTIVEQUOTA),
		      xdr_getquota_args, (void *)&gq_args, 
		      xdr_getquota_rslt, (void *)&gq_rslt) != 0) {
		*cp = ':';
		return (0);
	}
	switch (gq_rslt.gqr_status) {
	case Q_OK:
		if (!vflag && gq_rslt.gqr_rquota.rq_active == FALSE)
			return (0);
		gettimeofday(&tv, NULL);
		dqp->dqb_bhardlimit =
		    ((__uint64_t) gq_rslt.gqr_rquota.rq_bhardlimit *
		     gq_rslt.gqr_rquota.rq_bsize) / DEV_BSIZE;
		dqp->dqb_bsoftlimit =
		    ((__uint64_t) gq_rslt.gqr_rquota.rq_bsoftlimit *
		    gq_rslt.gqr_rquota.rq_bsize) / DEV_BSIZE;
		dqp->dqb_curblocks =
		    ((__uint64_t) gq_rslt.gqr_rquota.rq_curblocks *
		    gq_rslt.gqr_rquota.rq_bsize) / DEV_BSIZE;
		dqp->dqb_fhardlimit = gq_rslt.gqr_rquota.rq_fhardlimit;
		dqp->dqb_fsoftlimit = gq_rslt.gqr_rquota.rq_fsoftlimit;
		dqp->dqb_curfiles = gq_rslt.gqr_rquota.rq_curfiles;
		dqp->dqb_btimelimit =
		    tv.tv_sec + gq_rslt.gqr_rquota.rq_btimeleft;
		dqp->dqb_ftimelimit =
		    tv.tv_sec + gq_rslt.gqr_rquota.rq_ftimeleft;
		*cp = ':';
		
		return (1);

	case Q_NOQUOTA:
		break;

	case Q_EPERM:
		fprintf(stderr, "quota permission error, host: %s\n", hostp);
		break;

	default:
		fprintf(stderr, "bad rpc result, host: %s\n",  hostp);
		break;
	}
	*cp = ':';
	return (0);
}


static void
usage(void)
{
#ifndef _NOPROJQUOTAS	
	printf("usage: %s [-npv] [user ...]\n", whoami);
#else
	printf("usage: %s [-nv] [user ...]\n", whoami);
#endif
	exit(1);
}
