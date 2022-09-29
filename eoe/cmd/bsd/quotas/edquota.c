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
 * "@(#)edquota.c	1.2 88/03/10 4.0NFSSRC; from 1.10 88/02/08 SMI";
 */

#ident  "$Revision: 1.25 $"
/*
 * Disk quota editor.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <proj.h>
#include <pwd.h>
#include <ctype.h>
#include <mntent.h>
#include <malloc.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/quota.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/capability.h>

#define	DEFEDITOR	"/usr/bin/vi"
#undef dbtob
#undef btodb
#define dbtob BBTOB
#define btodb BTOBB
#define BBTOOFF64(bbs)	((__uint64_t)(bbs) << BBSHIFT)
#define KBTOBB_ULL(n)	((xfs_qcnt_t)OFFTOBB((unsigned long long)(n) * 0x400ULL))
#define BBTOKB_ULL(n)	((unsigned long long) (howmany(BBTOOFF64(n), 0x400ULL)))

typedef enum {
  FS_NONE, FS_XFS, FS_EFS, FS_NFS
} quota_fstype_t;

struct fsquot {
	struct fsquot 	*fsq_next;
	union {
		struct dqblk 	 dqb_efs;
		fs_disk_quota_t dqb_xfs;
	} fsq_u;
	char 		*fsq_fs;
	char 		*fsq_dev;
	char 		*fsq_qfile;
	quota_fstype_t	fsq_type;
};

struct fsquot *fsqlist;
int	pflag;
int	tflag;
int	aflag;
int	dryrun;
int	iflag;
int 	fsflag;

char	tmpfil[] = "/tmp/EdP.aXXXXX";
char	*qfname = "quotas";
int	fatalerror = 0;
char	*whoami;
char	*infile;

typedef __uint64_t 	xfs_qcnt_t;
typedef __uint32_t	xfs_dqid_t;

static char *myopts[] = { 
#define OPT_IHARD		0
	"ihard", 
#define OPT_ISOFT		1
	"isoft",
#define OPT_BHARD		2
	"bhard", 
#define OPT_BSOFT		3
	"bsoft",
#define OPT_RTBSOFT		4
	"rtbsoft",
#define OPT_RTBHARD		5
	"rtbhard",
#define OPT_UID			6
	"uid",
#define OPT_PROJID		7
	"projid",
	0
};
 
static uid_t 	getentry(char *);
static prid_t 	getprojentry(char *name);
static int	editit(void);
static void	getprivs(uid_t uid);
static void	putprivs(uid_t uid);
static void	gettimes(uid_t uid);
static void	puttimes(uid_t uid);
static char *	next(char *cp, char *match);
static int	alldigits(char *s);
static void	fmttime(char *buf, u_long time);
static int	unfmttime(double value, char *units, u_long *timep);
static void	setupfs(void);
static void	getdquot(uid_t uid);
static void	putdquot(register uid);
static void	moddquot(struct fsquot *fsqp, fs_disk_quota_t *dqp, int);
static void	xfsdqprint(struct fsquot *fsqp, fs_disk_quota_t *d, char *fsdev);
static void	efsdqprint(uid_t id, struct fsquot *fsqp);
static void	initxfsdquot(fs_disk_quota_t *d);
static void	usage(void);
static int 	readinrepquota(char *file);
static int 	doxfsupdate(char *filesysdev, fs_disk_quota_t *ddq, int);
static int	getxfsqstat(char *fsdev);
static int	sanitychkxfs(fs_disk_quota_t *d);
static int	parselimits(char *, char **);
static int	xfssetqlim(char *filesysdev, fs_disk_quota_t *ddq);

main(
	int 	argc,
	char 	**argv)
{
	uid_t 	psrcid, uid;
	char	*psrcidstr;
	int	c;
	char 	*filesysdev;
	extern int	optind;
	extern char	*optarg;

	whoami = argv[0];
	if (getuid()) {
		fprintf(stderr, "%s: permission denied\n", whoami);
		exit(1);
	}

	if (argc < 2) 
		usage();

	setupfs();
	if (fsqlist == NULL) {
		fprintf(stderr, 
			"%s: Couldn't find any filesystems with quotas on.\n",
		    MOUNTED);
		exit(1);
	}

	tflag = pflag = aflag = dryrun = iflag = fsflag = 0;
	while ((c = getopt(argc, argv, "naf:p:ti:l:")) != EOF) {
		switch (c) {
		      case 'n':
			dryrun++;
			break;
		      case '?':
			usage();
			break;
		}
	}
	getoptreset();
	opterr = 0;
	while ((c = getopt(argc, argv, ":naf:p:ti:l:")) != EOF) {
		switch (c) {
		      case 'n':
			dryrun++;
			break;
		      
		      case 'f':
			if (optarg == NULL)
				usage();
			filesysdev = optarg;
			fsflag++;
			break;
			
		      case 'a':
			filesysdev = NULL;
			aflag++;
			break;

		      case 'p':
			if (optarg == NULL)
				usage();
			psrcidstr = optarg;
			pflag++;
			break;

		      case 't':
			tflag++;
			break;
			
		      case 'i':
			if (optarg == NULL)
				usage();
			infile = optarg;
			iflag++;
			break;

		      case 'l':
			if (optarg == NULL) 
				break;
			if (!fsflag && !aflag) {
				fprintf(stderr,
					"%s: must specify filesystem(s)."
					"use -f or -a\n", whoami);
				usage();
			}
			(void) parselimits(filesysdev, &optarg);
			break;

		      default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv = &argv[optind];

	/*
	 * Reading in a file that edquota spat out. We don't combine this 
	 * option with others.
	 */
	if (iflag) {
		exit(readinrepquota(infile));
	}

	if (fsflag && (pflag || tflag)) {
		fprintf(stderr,"%s: Cannot use -f with -p or -t flags\n",
			whoami);
		usage();
	}
	/*
	 * Meaningless for XFS, a good idea for EFS.
	 */
	if (quotactl(Q_SYNC, NULL, 0, NULL) < 0 && errno == ENOPKG) {
		perror("quotas");
		exit(1);
	}

	mktemp(tmpfil);
	close(creat(tmpfil, 0600));
	(void) chown(tmpfil, getuid(), getgid());
	if (tflag) {
		gettimes(0);
		if (editit())
			puttimes(0);
	}

	if (pflag) {
		psrcid = getentry(psrcidstr);
		if (psrcid == -1) {
			(void) unlink(tmpfil);
			exit(1);
		}
		getprivs(psrcid);
		while (argc-- > 0) {
			uid = getentry(*argv++);
			if (uid == -1)
				continue;
			getdquot(uid);
			putprivs(uid);
			if (fatalerror)
				goto errexit;
		}
		(void) unlink(tmpfil);
		exit(0);
	}

		
	while (--argc >= 0) {
		uid = getentry(*argv++);
		if (uid == -1)
			continue;
		getprivs(uid);
		if (editit())
			putprivs(uid);
		if (fatalerror) 
			goto errexit;
	}
	(void) unlink(tmpfil);
	exit(0);
	/* NOTREACHED */

errexit:
	fprintf(stderr,"Last argument processed: %s\n",
		*(--argv));
	unlink(tmpfil);
	exit(1);
	/* NOTREACHED */
}

static uid_t
getentry(char *name)
{
	struct passwd *pw;
	uid_t uid;

	/* XXX is there a MAXUID constant somewhere ? */
	if (alldigits(name))
		uid = (uid_t) atoi(name);
	else if (pw = getpwnam(name))
		uid = pw->pw_uid;
	else {
		fprintf(stderr, "%s: no such user\n", name);
		sleep(1);
		return (-1);
	}
	return (uid);
}

static prid_t
getprojentry(char *name)
{
	prid_t 	prid;
	
	if (alldigits(name))
		prid = (prid_t) atoi(name);
	else if ((prid = projid(name)) < 0) {
		fprintf(stderr, 
			"%s: '%s' - no such project (skipping)\n", whoami,
			name);
		sleep(1);
		return (prid_t)-1;
	}
	return prid;
}


static int
editit(void)
{
	register pid, xpid;
	int status, omask;

#define	mask(s)	(1<<((s)-1))
	omask = sigblock(mask(SIGINT)|mask(SIGQUIT)|mask(SIGHUP));
	if ((pid = fork()) < 0) {
		if (errno == EAGAIN) {
			fprintf(stderr, "You have too many processes\n");
			return(0);
		}
		perror("fork");
		return (0);
	}
	if (pid == 0) {
		char *ed;

		(void) sigsetmask(omask);
		(void) setgid(getgid());
		(void) setuid(getuid());
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = DEFEDITOR;
		execlp(ed, ed, tmpfil, 0);
		perror(ed);
		exit(1);
	}
	while ((xpid = wait(&status)) >= 0)
		if (xpid == pid)
			break;
	(void) sigsetmask(omask);
	/*
	 * Don't return exit status of vi, since it is bogus.
	 * If exec of EDITOR succeeded, then lets examine the tmpfil contents.
	 */
	return (1);
}

static void
getprivs(uid_t uid)
{
	struct fsquot *fsqp;
	FILE *fd;

	getdquot(uid);
	if ((fd = fopen(tmpfil, "w")) == NULL) {
		perror(tmpfil);
		(void) unlink(tmpfil);
		exit(1);
	}
	for (fsqp = fsqlist; fsqp; fsqp = fsqp->fsq_next) {
		if (fsqp->fsq_type == FS_EFS) {
			fprintf(fd,
				"fs %s kbytes (soft = %ld, hard = %ld) "
				"inodes (soft = %ld, hard = %ld)\n"
			, fsqp->fsq_fs
			, (long) (dbtob(fsqp->fsq_u.dqb_efs.dqb_bsoftlimit) / 1024)
			, (long) (dbtob(fsqp->fsq_u.dqb_efs.dqb_bhardlimit) / 1024)
			, (long) fsqp->fsq_u.dqb_efs.dqb_fsoftlimit
			, (long) fsqp->fsq_u.dqb_efs.dqb_fhardlimit
				);
		} else if (fsqp->fsq_type == FS_XFS) {
			fprintf(fd,
				"fs %s kbytes (soft = %llu, hard = %llu) "
				"inodes (soft = %llu, hard = %llu)\n"
			, fsqp->fsq_fs
			, BBTOKB_ULL(fsqp->fsq_u.dqb_xfs.d_blk_softlimit)
			, BBTOKB_ULL(fsqp->fsq_u.dqb_xfs.d_blk_hardlimit)
			, (unsigned long long) fsqp->fsq_u.dqb_xfs.d_ino_softlimit
			, (unsigned long long) fsqp->fsq_u.dqb_xfs.d_ino_hardlimit
				);
		}

	}
	fclose(fd);
}

static void
putprivs(uid_t uid)
{
	FILE 			*fd;
	struct dqblk 		ndqb;
	fs_disk_quota_t		ndqbxfs;
	char 			line[BUFSIZ];
	int 			changed = 0;

	fd = fopen(tmpfil, "r");
	if (fd == NULL) {
		fprintf(stderr, "Can't re-read temp file!!\n");
		return;
	}
	while (fgets(line, sizeof(line), fd) != NULL) {
		struct fsquot *fsqp;
		char *cp, *dp;
		int n;

		cp = next(line, " \t");
		if (cp == NULL)
			break;
		*cp++ = '\0';
		while (*cp && *cp == '\t' && *cp == ' ')
			cp++;
		dp = cp, cp = next(cp, " \t");
		if (cp == NULL)
			break;
		*cp++ = '\0';
		for (fsqp = fsqlist; fsqp; fsqp = fsqp->fsq_next) {
			if (strcmp(dp, fsqp->fsq_fs) == 0)
				break;
		}
		if (fsqp == NULL) {
			fprintf(stderr, "%s: unknown file system\n", cp);
			continue;
		}
		while (*cp && *cp == '\t' && *cp == ' ')
			cp++;
		if (fsqp->fsq_type == FS_EFS) {
			n = sscanf(cp,
				   "kbytes (soft = %ld, hard = %ld) inodes "
				   "(soft = %ld, hard = %ld)\n"
				   , &ndqb.dqb_bsoftlimit
				   , &ndqb.dqb_bhardlimit
				   , &ndqb.dqb_fsoftlimit
				   , &ndqb.dqb_fhardlimit
				   );
			if (n != 4) {
				fprintf(stderr, 
					"%s: bad format, not updating %s quotas "
					"for uid %d\n",
					cp, fsqp->fsq_fs, uid);
				continue;
			}
			changed++;
			ndqb.dqb_bsoftlimit = btodb(ndqb.dqb_bsoftlimit * 1024);
			ndqb.dqb_bhardlimit = btodb(ndqb.dqb_bhardlimit * 1024);
			/*
			 * It we are decreasing the soft limits, set the time limits
			 * to zero, in case the user is now over quota.
			 * the time limit will be started the next time the
			 * user does an allocation.
			 */
			if (ndqb.dqb_bsoftlimit < 
			    fsqp->fsq_u.dqb_efs.dqb_bsoftlimit)
				fsqp->fsq_u.dqb_efs.dqb_btimelimit = 0;
			if (ndqb.dqb_fsoftlimit < 
			    fsqp->fsq_u.dqb_efs.dqb_fsoftlimit)
				fsqp->fsq_u.dqb_efs.dqb_ftimelimit = 0;
			fsqp->fsq_u.dqb_efs.dqb_bsoftlimit = ndqb.dqb_bsoftlimit;
			fsqp->fsq_u.dqb_efs.dqb_bhardlimit = ndqb.dqb_bhardlimit;
			fsqp->fsq_u.dqb_efs.dqb_fsoftlimit = ndqb.dqb_fsoftlimit;
			fsqp->fsq_u.dqb_efs.dqb_fhardlimit = ndqb.dqb_fhardlimit;
		} else {
			assert(fsqp->fsq_type == FS_XFS);
			initxfsdquot(&ndqbxfs);
			n = sscanf(cp,
				   "kbytes (soft = %llu, hard = %llu) inodes "
				   "(soft = %llu, hard = %llu)\n"
				   , (unsigned long long *)&ndqbxfs.d_blk_softlimit
				   , (unsigned long long *)&ndqbxfs.d_blk_hardlimit
				   , (unsigned long long *)&ndqbxfs.d_ino_softlimit
				   , (unsigned long long *)&ndqbxfs.d_ino_hardlimit
				   );
			if (n != 4) {
				fprintf(stderr, 
					"%s: bad format, not updating %s quotas "
					"for uid %d\n",
					cp, fsqp->fsq_fs, uid);
				continue;
			}
			changed++;
			ndqbxfs.d_fieldmask = FS_DQ_ISOFT | FS_DQ_BSOFT | 
				FS_DQ_BHARD | FS_DQ_IHARD;	
			moddquot(fsqp, &ndqbxfs, 1);
		}
	}
	fclose(fd);
	if (changed)
		putdquot(uid);
}

static void
moddquot(
	struct fsquot 	*fsqp,
	fs_disk_quota_t	*dqp,
	int		kbtobb)
{
	fsqp->fsq_u.dqb_xfs.d_fieldmask = dqp->d_fieldmask;
	fsqp->fsq_u.dqb_xfs.d_id = dqp->d_id;
	if (dqp->d_fieldmask & FS_DQ_BSOFT) {
		if (kbtobb)
			fsqp->fsq_u.dqb_xfs.d_blk_softlimit = 
				KBTOBB_ULL(dqp->d_blk_softlimit);
		else
			fsqp->fsq_u.dqb_xfs.d_blk_softlimit =
				dqp->d_blk_softlimit;
	}
	if (dqp->d_fieldmask & FS_DQ_BHARD) {
		if (kbtobb)
			fsqp->fsq_u.dqb_xfs.d_blk_hardlimit
				= KBTOBB_ULL(dqp->d_blk_hardlimit);
		else
			fsqp->fsq_u.dqb_xfs.d_blk_hardlimit
				= dqp->d_blk_hardlimit;
	}
	
	fsqp->fsq_u.dqb_xfs.d_ino_softlimit = 
		dqp->d_ino_softlimit;
	fsqp->fsq_u.dqb_xfs.d_ino_hardlimit =
		dqp->d_ino_hardlimit;
	/* XXX RT */
}

	    
static void
gettimes(uid_t uid)
{
	struct fsquot *fsqp;
	FILE *fd;
	char btime[80], ftime[80];

	getdquot(uid);
	if ((fd = fopen(tmpfil, "w")) == NULL) {
		perror(tmpfil);
		(void) unlink(tmpfil);
		exit(1);
	}
	for (fsqp = fsqlist; fsqp; fsqp = fsqp->fsq_next) {
		if (fsqp->fsq_type == FS_EFS) {
			fmttime(btime, fsqp->fsq_u.dqb_efs.dqb_btimelimit);
			fmttime(ftime, fsqp->fsq_u.dqb_efs.dqb_ftimelimit);
		} else if (fsqp->fsq_type == FS_XFS) {
			fmttime(btime, fsqp->fsq_u.dqb_xfs.d_btimer);
			fmttime(ftime, fsqp->fsq_u.dqb_xfs.d_itimer);
		} else {
			assert(fsqp->fsq_type == FS_EFS ||
			       fsqp->fsq_type == FS_XFS);
			continue;
		}
			
		fprintf(fd,
"fs %s kbytes time limit = %s, files time limit = %s\n"
			, fsqp->fsq_fs
			, btime
			, ftime
		);
	}
	fclose(fd);
}

static void
puttimes(uid_t uid)
{
	FILE 		*fd;
	char 		line[BUFSIZ];
	int 		changed = 0;
	double 		btimelimit, ftimelimit;
	char 		bunits[80], funits[80];
	__int32_t	*btimerp, *itimerp;

	fd = fopen(tmpfil, "r");
	if (fd == NULL) {
		fprintf(stderr, "Can't re-read temp file!!\n");
		return;
	}
	while (fgets(line, sizeof(line), fd) != NULL) {
		struct fsquot *fsqp;
		char *cp, *dp;
		int n;

		cp = next(line, " \t");
		if (cp == NULL)
			break;
		*cp++ = '\0';
		while (*cp && *cp == '\t' && *cp == ' ')
			cp++;
		dp = cp, cp = next(cp, " \t");
		if (cp == NULL)
			break;
		*cp++ = '\0';
		for (fsqp = fsqlist; fsqp; fsqp = fsqp->fsq_next) {
			if (strcmp(dp, fsqp->fsq_fs) == 0)
				break;
		}
		if (fsqp == NULL) {
			fprintf(stderr, "%s: unknown file system\n", cp);
			continue;
		}
		while (*cp && *cp == '\t' && *cp == ' ')
			cp++;
		btimelimit = ftimelimit = 0;
		initxfsdquot(&fsqp->fsq_u.dqb_xfs);

		n = sscanf(cp,
"kbytes time limit = %lf %[^,], files time limit = %lf %s\n"
			, &btimelimit
			, bunits
			, &ftimelimit
			, funits
		);
		if (fsqp->fsq_type == FS_EFS) {
			btimerp = (__int32_t *)&fsqp->fsq_u.dqb_efs.dqb_btimelimit;
			itimerp = (__int32_t *)&fsqp->fsq_u.dqb_efs.dqb_ftimelimit;
		} else if (fsqp->fsq_type == FS_XFS) {
			fsqp->fsq_u.dqb_xfs.d_id = uid;
			btimerp = &fsqp->fsq_u.dqb_xfs.d_btimer;
			itimerp = &fsqp->fsq_u.dqb_xfs.d_itimer;
			fsqp->fsq_u.dqb_xfs.d_fieldmask = FS_DQ_ITIMER |
				FS_DQ_BTIMER;
		}

		if (n != 4 ||
		    !unfmttime(btimelimit, bunits,
			       (u_long *)btimerp) ||
		    !unfmttime(ftimelimit, funits,
			       (u_long *)itimerp)) {
			fprintf(stderr, 
			"%s: bad format, not updating %s quotas for uid %d\n",
				cp, fsqp->fsq_fs, uid);
			continue;
		}
		changed++;
	}
	fclose(fd);
	if (changed)
		putdquot(uid);
}

static char *
next(char *cp, char *match)
{
	char *dp;

	while (cp && *cp) {
		for (dp = match; dp && *dp; dp++)
			if (*dp == *cp)
				return (cp);
		cp++;
	}
	return ((char *)0);
}

static int
alldigits(char *s)
{
	register c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while (c = *s++);
	return (1);
}

static struct {
	int c_secs;			/* conversion units in secs */
	char * c_str;			/* unit string */
} cunits [] = {
	{60*60*24*28, "month"},
	{60*60*24*7, "week"},
	{60*60*24, "day"},
	{60*60, "hour"},
	{60, "min"},
	{1, "sec"}
};

static void
fmttime(char *buf, u_long time)
{
	double value;
	int i;

	if (time == 0) {
		strcpy(buf, "0 (default)");
		return;
	}
	for (i = 0; i < sizeof(cunits)/sizeof(cunits[0]); i++) {
		if (time >= cunits[i].c_secs)
			break;
	}
	value = (double)time / cunits[i].c_secs;
	sprintf(buf, "%.2f %s%s", value, cunits[i].c_str, value > 1.0? "s": "");
}

static int
unfmttime(double value,
	  char *units,
	  u_long *timep)
{
	int i;

	if (value == 0.0) {
		*timep = 0;
		return (1);
	}
	for (i = 0; i < sizeof(cunits)/sizeof(cunits[0]); i++) {
		if (strncmp(cunits[i].c_str,units,strlen(cunits[i].c_str)) == 0)
			break;
	}
	if (i >= sizeof(cunits)/sizeof(cunits[0])) {
		return (0);
	}
	*timep = (u_long)(value * cunits[i].c_secs);
	return (1);
}

static int
getxfsqstat(
	char *fsdev)
{
	fs_quota_stat_t	qstat;

	/*
	 * See if quotas is on. If not, nada.
	 */
	if (quotactl(Q_GETQSTAT, fsdev, 0, 
		     (caddr_t)&qstat) < 0) {
		perror(fsdev);
		return (-1);
	}
	return (qstat.qs_flags & (XFS_QUOTA_UDQ_ACCT | XFS_QUOTA_PDQ_ACCT));
}

static void
setupfs(void)
{
	struct mntent 	*mntp;
	struct fsquot 	*fsqp;
	struct stat 	statb;
	FILE 		*mtab;
	char 		qfilename[MAXPATHLEN + 1];
	quota_fstype_t	fstype;
	dev_t		fsdev;
	int		r;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;

	mtab = setmntent(MOUNTED, "r");
	while (mntp = getmntent(mtab)) {
		if (hasmntopt(mntp, MNTOPT_RO))
			continue;
		
		if (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) {
			/*
			 * See if quotas is on. If not, nada.
			 */
			if (getxfsqstat(mntp->mnt_fsname) <= 0)
				continue;
			fstype = FS_XFS;
		} else if (strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0) {
			if (stat(mntp->mnt_fsname, &statb) < 0)
				continue;
			if ((statb.st_mode & S_IFMT) != S_IFBLK)
				continue;
			fsdev = statb.st_rdev;
			sprintf(qfilename, "%s/%s", mntp->mnt_dir, qfname);
			if (stat(qfilename, &statb) < 0 || statb.st_dev != fsdev)
				continue;
			if (statb.st_uid) {
				fprintf(stderr, 
					"%s: %s not owned by root\n", whoami,
					qfilename);
				continue;
			}
			/*
			 * If quotas has not been enabled for the fs, 
			 * we activate it so that we can make changes if
			 * necessary.
			 */
			if (! dryrun) {
				ocap = cap_acquire(1, &cap_quota_mgt);
				r = quotactl(Q_ACTIVATE, mntp->mnt_fsname, 0,
					     qfilename);
				cap_surrender(ocap);
				if (r != 0) {
					perror(mntp->mnt_fsname);
					continue;
				}
			}
			fstype = FS_EFS;
		} else 
			continue;
			

		fsqp = (struct fsquot *)malloc(sizeof(struct fsquot));
		if (fsqp == NULL) {
			fprintf(stderr, "out of memory\n");
			exit (1);
		}
		fsqp->fsq_next = fsqlist;
		fsqp->fsq_fs = malloc(strlen(mntp->mnt_dir) + 1);
		fsqp->fsq_dev = malloc(strlen(mntp->mnt_fsname) + 1);
		fsqp->fsq_type = fstype;
		/*
		 * XFS quotas doesn't have a /quotas file.
		 */
		if (fstype == FS_EFS)
			fsqp->fsq_qfile = malloc(strlen(qfilename) + 1);
		if (fsqp->fsq_fs == NULL || fsqp->fsq_dev == NULL) {
			fprintf(stderr, "out of memory\n");
			exit (1);
		}
		strcpy(fsqp->fsq_fs, mntp->mnt_dir);
		strcpy(fsqp->fsq_dev, mntp->mnt_fsname);
		if (fstype == FS_EFS)
			strcpy(fsqp->fsq_qfile, qfilename);
		fsqlist = fsqp;
	}
	endmntent(mtab);
}

static void
getdquot(uid_t uid)
{
	struct fsquot *fsqp;
	int		r;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;

	for (fsqp = fsqlist; fsqp; fsqp = fsqp->fsq_next) {
		if (fsqp->fsq_type == FS_EFS) {
			ocap = cap_acquire(1, &cap_quota_mgt);
			r = quotactl(Q_GETQUOTA, fsqp->fsq_dev, (int) uid,
				     (caddr_t) &fsqp->fsq_u.dqb_efs);
			cap_surrender(ocap);
			if (r != 0) {
				perror(fsqp->fsq_qfile);
			}
		} else if (fsqp->fsq_type == FS_XFS) {
			ocap = cap_acquire(1, &cap_quota_mgt);
			r = quotactl(Q_XGETQUOTA, fsqp->fsq_dev, (int) uid,
				     (caddr_t) &fsqp->fsq_u.dqb_xfs);
			cap_surrender(ocap);
			if (r != 0) {
				/*
				 * For XFS, we're not guaranteed to receive
				 * a zeroed quota struct, when the id doesn't
				 * exist.
				 */
				bzero(&fsqp->fsq_u.dqb_xfs, 
				      sizeof(fs_disk_quota_t));
			}
		}
	}
}

void
putdquot(register uid)
{
	struct fsquot *fsqp;
	int		r;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;
	
	for (fsqp = fsqlist; fsqp; fsqp = fsqp->fsq_next) {
		if (fsqp->fsq_type == FS_EFS) {
			if (dryrun) {
				efsdqprint(uid, fsqp);
				continue;
			}
			ocap = cap_acquire(1, &cap_quota_mgt);
			r = quotactl(Q_SETQLIM, fsqp->fsq_dev, (int) uid,
				     (caddr_t)&fsqp->fsq_u.dqb_efs);
			cap_surrender(ocap);
			if (r != 0) {
				fatalerror++;
				if (errno == ENOSPC) {
					fprintf(stderr, 
			      "No more space in the quota file index.\n");
					fprintf(stderr, 
			      "Use quotacheck(1M) (-n option) to increase "
			      "the index size for %s\n",
						fsqp->fsq_qfile);
				} else
					perror(fsqp->fsq_qfile);
			}
		} else if (fsqp->fsq_type == FS_XFS) {
			/* Projects XXX ? */
			if (dryrun) {
				xfsdqprint(fsqp, NULL, NULL);
				continue;
			}
			if (sanitychkxfs(&fsqp->fsq_u.dqb_xfs) < 0)
				continue;

			ocap = cap_acquire(1, &cap_quota_mgt);
			r = quotactl(Q_XSETQLIM, fsqp->fsq_dev, (int) uid,
				     (caddr_t)&fsqp->fsq_u.dqb_xfs);
			cap_surrender(ocap);
			if (r) {
				fatalerror++;
				perror("Q_SETQLIM");
			}
		}
	}
				
}
	
unsigned long long
getlim(char *str)
{
	unsigned long long lim;

	lim = strtoull(str, (char **)NULL, 0);
	if (lim == ULONGLONG_MAX && errno == ERANGE) {
		fprintf(stderr, "Limit out of range\n");
		return (-1);
	}
	return (lim);
}

static void
initxfsdquot(fs_disk_quota_t *d)
{
	bzero(d, sizeof(*d));
	d->d_id = -1;
}

/*
 * Read in an output file produced by repquota.
 */
int
readinrepquota(
	char 	*file)
{
	FILE 	*fp;
	char 	line[255];
	char	fsdev[255];
	int	linenum;
	fs_disk_quota_t d;
	int 	turn;
	int	ndone;

	linenum = turn = ndone = 0;
	fp = fopen(file, "r");
	if (fp == NULL) {
		perror(file);
		return (-1);
	}

	bzero(line, 255);
	while (fgets(line, sizeof(line), fp)) {
		linenum++;

		/*
		 * skip comment lines.
		 */
		if (line[0] == '#')
			continue;
		initxfsdquot(&d);
		if ((turn % 2) == 0) {
			if (sscanf(line,
			       "fs = %s",
			       fsdev) != 1)
				continue;
			turn++;
			continue;
		}
		
		/*
		 * get the limits.
		 */
		if (sscanf(line,
		       "%d %llu %llu %llu %llu",
		       &d.d_id,
		       &d.d_blk_softlimit,
		       &d.d_blk_hardlimit,
		       &d.d_ino_softlimit,
		       &d.d_ino_hardlimit) != 5) {
			continue;
		}

		turn++;
		if ((d.d_blk_hardlimit && 
		     d.d_blk_softlimit > d.d_blk_hardlimit) ||
		    (d.d_ino_hardlimit &&
		     d.d_ino_softlimit > d.d_ino_hardlimit)) {
			fprintf(stderr, 
				"%s, line %d: softlimit cannot be greater than "
				"hardlimit\n",
				file, linenum);
			continue;
		}
		d.d_fieldmask = FS_DQ_ISOFT | FS_DQ_BSOFT | 
			FS_DQ_BHARD | FS_DQ_IHARD;
		if (doxfsupdate(fsdev, &d, 0) < 0) {
			fprintf(stderr, 
				"%s: error in %s, line %d\n",
				whoami, file, linenum);
		} else {
			ndone++;
		}
		bzero(line, 255);
	}
	fclose(fp);
	return (ndone > 0 ? 0: -1);
}

static void
xfsdqprint(struct fsquot *fsqp, fs_disk_quota_t *d, char *fsdev)
{
	int 		c;

	/* XXXhack */
	if (fsqp)
		d = &fsqp->fsq_u.dqb_xfs;
	if (dryrun) {
		printf("NOTE: Dry run; actual operations will not "
		       "be performed.\n");
		printf( "NOTE: The following changes would've been made.\n");
	}
	c = 0;
	        printf( "---------------------------\n");
	        printf( "user id         =  %d\n", (int) d->d_id);
	if (fsqp)
	        printf( "filesys [xfs]   =  %s (%s)\n", 
		       fsqp->fsq_dev, fsqp->fsq_fs);
	else
		printf( "filesys [xfs]   =  %s\n", fsdev);
	if (d->d_fieldmask & FS_DQ_BHARD) {
		printf( "blks hardlimit  =  %llu K\n",
		       (unsigned long long) d->d_blk_hardlimit / 2);
		c++;
	}
	if (d->d_fieldmask & FS_DQ_BSOFT) {
		printf( "blks softlimit  =  %llu K\n",
		       (unsigned long long) d->d_blk_softlimit / 2);
		c++;
	}
	if (d->d_fieldmask & FS_DQ_IHARD) {
		printf( "files hardlimit =  %llu\n", 
		       (unsigned long long) d->d_ino_hardlimit);
		c++;
	}
	if (d->d_fieldmask & FS_DQ_ISOFT) {
		printf( "files softlimit =  %llu\n", 
		       (unsigned long long) d->d_ino_softlimit);
		c++;
	}
	if (d->d_fieldmask & FS_DQ_BTIMER) {
		printf( "blks timer      =  %d\n",
		       d->d_btimer);
		c++;
	}
	if (d->d_fieldmask & FS_DQ_ITIMER) {
		printf( "files timer     =  %d\n",
		       d->d_itimer);
		c++;
	}
	if (!c) {
		printf( "<No Changes>\n");
	}
	        printf( "---------------------------\n");
}

static void
efsdqprint(uid_t id, struct fsquot *fsqp)
{
	int 		c;
	struct dqblk 	*d;

	d = &fsqp->fsq_u.dqb_efs;
	if (dryrun) {
		printf("NOTE: Dry run; actual operations will not "
		       "be performed.\n");
		printf( "NOTE: The following changes would've been made.\n");
	}
	c = 0;
	        printf( "---------------------------\n");
	        printf( "user id         =  %d\n", id);
	        printf( "filesys [efs]   =  %s (%s)\n", 
		       fsqp->fsq_dev, fsqp->fsq_fs);
	if (d->dqb_bhardlimit != -1) {
		printf( "blks hardlimit  =  %llu\n",
		       (unsigned long long) d->dqb_bhardlimit);
		c++;
	}
	if (d->dqb_bsoftlimit != -1) {
		printf( "blks softlimit  =  %llu\n",
		       (unsigned long long) d->dqb_bsoftlimit);
		c++;
	}
	if (d->dqb_fhardlimit != -1) {
		printf( "files hardlimit =  %llu\n", 
		       (unsigned long long) d->dqb_fhardlimit);
		c++;
	}
	if (d->dqb_fsoftlimit != -1) {
		printf( "files softlimit =  %llu\n", 
		       (unsigned long long) d->dqb_fsoftlimit);
		c++;
	}
	if (d->dqb_btimelimit > 0) {
		printf( "blks timer      =  %d\n",
		      d->dqb_btimelimit);
		c++;
	}
	if (d->dqb_ftimelimit > 0) {
		printf( "files timer     =  %d\n",
		       d->dqb_ftimelimit);
		c++;
	}
	if (!c) {
		printf( "<No Changes>\n");
	}
	        printf( "---------------------------\n");
}

static int
sanitychkxfs(
	fs_disk_quota_t *d)
{
	if ((d->d_fieldmask & FS_DQ_BHARD) &&
	    (d->d_fieldmask & FS_DQ_BSOFT) &&
	    d->d_blk_hardlimit && d->d_blk_softlimit > d->d_blk_hardlimit) {
		fprintf(stderr, 
			"%s: bsoftlimit %llu > bhardlimit %llu !\n",
			whoami,
			d->d_blk_softlimit,
			d->d_blk_hardlimit);
		return (-1);
	}

	if ((d->d_fieldmask & FS_DQ_IHARD) &&
	    (d->d_fieldmask & FS_DQ_ISOFT) &&
	    d->d_ino_hardlimit && d->d_ino_softlimit > d->d_ino_hardlimit) {
		fprintf(stderr, 
			"%s: fsoftlimit %llu > fhardlimit %llu !\n",
			whoami,
			d->d_ino_softlimit,
			d->d_ino_hardlimit);
		return (-1);
	}
	return (0);
}

static int
doxfsupdate(
	char 		*filesysdev,
	fs_disk_quota_t *ddq,
	int		doconvert)
{
	struct fsquot 	*fsqp;
	
	if (filesysdev != NULL) {
		int found = 0;
		for (fsqp = fsqlist;
		     fsqp; 
		     fsqp = fsqp->fsq_next) {
			if (strcmp(filesysdev,
				   fsqp->fsq_fs) == 0) {
				filesysdev = fsqp->fsq_dev;
				found++;
				break;
			} else if (strcmp(filesysdev,
					  fsqp->fsq_dev) == 0) {
				found++;
				break;
			}
		}
		if (!found) {
			fprintf(stderr,
				"%s: cannot do %s\n",
				whoami, filesysdev);
			return -1;
		}
		/* iflag || filesysdev */
		moddquot(fsqp, ddq, doconvert);
		(void) xfssetqlim(filesysdev, &fsqp->fsq_u.dqb_xfs);
		
	} else if (aflag) {
		int done = 0;
		if (!iflag) {
			for (fsqp = fsqlist;
			     fsqp; 
			     fsqp = fsqp->fsq_next) {
				filesysdev = fsqp->fsq_dev;
				moddquot(fsqp, ddq, doconvert);
				if (xfssetqlim(filesysdev, &fsqp->fsq_u.dqb_xfs)
				    == 0)
					done++;
			}
			if (!done)
				fprintf(stderr,
				"%s: [-a] no appropriate filesystems found\n",
					whoami);
		}
	} else {
		fprintf(stderr,
			"%s: must specify filesystem(s)."
			"use -f or -a\n", whoami);
		return -1;
	}
	return 0;
}

static int
xfssetqlim(char *filesysdev, fs_disk_quota_t *ddq)
{
	int		r;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;

	if (getxfsqstat(filesysdev) <= 0) {
		fprintf(stderr,
			"%s: cannot do %s\n",
			whoami, filesysdev);
		return -1;
	}
	
	if (sanitychkxfs(ddq) < 0)
		return (-1);
	
	if (! dryrun) {
		if (ddq->d_fieldmask) {
			ocap = cap_acquire(1, &cap_quota_mgt);
			r = quotactl(Q_XSETQLIM, filesysdev, ddq->d_id,
				     (caddr_t) ddq);
			cap_surrender(ocap);
			if (r) {
				perror(whoami);
				return -1;
			} 
		} else {
			fprintf(stderr,
				"%s: nothing to change in ID '%d'\n", 
				whoami, ddq->d_id); 
		}
	} else {
		xfsdqprint(NULL, ddq, filesysdev);
	}
	return (0);
}

int
parselimits(
	char *filesysdev,
	char **options)
{
	unsigned long long 	lim;
	int 			id, d, error;
	fs_disk_quota_t		ddq;
	char *			value; 
	extern char 		*optarg;
	extern int 		optind;
	
	initxfsdquot(&ddq);
	error = 0;
	id = -1;
	while (**options != '\0' && !error) {
		d = getsubopt(options, myopts, &value);
		if (value  == NULL)
			continue;
		
		switch (d) {
		      case OPT_UID:
			id = getentry(value);
			if (id < 0) {
				error = 1;
				break;
			}
			ddq.d_id = (xfs_dqid_t)id;
			break;
			
		      case OPT_PROJID:
			id = (int) getprojentry(value);
			if (id < 0) {
				error = 1;
				break;
			}
			ddq.d_id = (xfs_dqid_t)id;
			break;

		      case OPT_IHARD:
			lim = getlim(value);
			ddq.d_ino_hardlimit = (xfs_qcnt_t)lim;
			ddq.d_fieldmask |= FS_DQ_IHARD;
			break;
			
		      case OPT_ISOFT:
			lim = getlim(value);
			ddq.d_ino_softlimit = (xfs_qcnt_t)lim;
			ddq.d_fieldmask |= FS_DQ_ISOFT;
			break;
			
		      case OPT_BSOFT:
			lim = getlim(value);
			ddq.d_blk_softlimit = (xfs_qcnt_t)lim;
			ddq.d_fieldmask |= FS_DQ_BSOFT;
			break;
			
		      case OPT_BHARD:
			lim = getlim(value);
			ddq.d_blk_hardlimit = (xfs_qcnt_t)lim;
			ddq.d_fieldmask |= FS_DQ_BHARD;
			break;
			
		      case OPT_RTBHARD:
			lim = getlim(value);
			ddq.d_rtb_hardlimit = (xfs_qcnt_t)lim;
			ddq.d_fieldmask |= FS_DQ_RTBHARD;
			break;
			
		      case OPT_RTBSOFT:
			lim = getlim(value);
			ddq.d_rtb_softlimit = (xfs_qcnt_t)lim;
			ddq.d_fieldmask |= FS_DQ_RTBSOFT;
			break;
			
		      default:
			error = 1;
			fprintf(stderr,
				"%s: invalid suboption '%s' under -l\n",
				whoami,
				value);
			break;
		}
		if (error)
			return (error);
	}
	
	if (id >= 0) 
		error = doxfsupdate(filesysdev, &ddq, 1);
	else
		fprintf(stderr,
			"%s: must specify valid uid with -l option\n", 
			whoami);
	return (error);
}
static void
usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s [-n] usernames ...\n", whoami);
	fprintf(stderr, "\t%s [-n] -t\n", whoami);
	fprintf(stderr, "\t%s [-n] [-p proto-user] usernames ...\n", whoami);
	fprintf(stderr, "\t%s [-n] [-i repquota-input-file]\n", whoami);
	fprintf(stderr, "\t%s [-n] [-f filesys [-l <options>]]\n", whoami);
	fprintf(stderr, "\t%s [-n] [-a [-l <options>]]\n", whoami);
	fprintf(stderr, "\t\toptions: uid=?,ihard=?,isoft=?,bhard=?,bsoft=?\n");
	exit(1);
}
