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
 * "@(#)quotaon.c	1.2 88/03/15 4.0NFSSRC";
 */

#ident  "$Revision: 1.21 $"
/*
 * Turn quota on/off for EFS/XFS filesystem.
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/quota.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <mntent.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <sys/capability.h>

int	vflag;		/* verbose */
int	aflag;		/* all file systems */
int 	rmflag;		/* remove quotafile(s) after quotaoff */
int	oflag;		/* options - quota, uquota, pquota, uqenforce, 
			   pqenforce */

char 	*whoami;
int 	offmode;
uint	quotaflags;
uint	delflags;

#define QFNAME "quotas"
char	quotafile[MAXPATHLEN + 1];
char	*listbuf[50];


static void 	fixmntent(struct mntent *mntp, int offmode, uint flags);
static int 	quotaonoff(struct mntent *mntp, int offmode);
static int 	oneof(char *target, char **listp, int n);
static void 	usage(void);
static int	quotaonoff_xfs(struct mntent *mntp, int, uint);
static void 	parseopts(int *, char ***);
static int	getxfsqstat(char *fsdev, fs_quota_stat_t *qstat);

main(
	int argc, 
	char **argv)
{
	register struct mntent *mntp;
	char 		**listp;
	int 		listcnt;
	FILE 		*mtab, *fstab, *tmp;
	int 		errs = 0;
	char 		*tmpname = "/etc/quotaontmpXXXXXX";
	char 		*arg0;
	struct stat 	statb;
	int		isxfs;
	int		isefs;
	fs_quota_stat_t	qstat;
	
	arg0 = *argv;
	offmode = 0;
	vflag = aflag = rmflag = oflag = 0;
	whoami = rindex(*argv, '/') + 1;
	if (whoami == (char *)1)
		whoami = *argv;
	if (strcmp(whoami, "quotaoff") == 0)
		offmode++;
	else if (strcmp(whoami, "quotaon") != 0) {
		fprintf(stderr, "Name must be quotaon or quotaoff not %s\n",
			whoami);
		exit(1);
	}
	if (getuid()) {
		fprintf(stderr, "%s: Permission denied\n", arg0);
		exit(1);
	}
	
	parseopts(&argc, &argv);

	if (argc <= 0 && !aflag) 
		usage();

	/*
	 * If aflag go through fstab and make a list of appropriate
	 * filesystems.
	 */
	if (aflag) {
		listp = listbuf;
		listcnt = 0;
		fstab = setmntent(MNTTAB, "r");
		while (mntp = getmntent(fstab)) {
			isefs = (strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0);
			if (!isefs)
				isxfs = (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0);
			else
				isxfs = 0;
			if (!isefs && !isxfs)
				continue;

			/*
			 * quotaon -a option won't work on XFS, not even on
			 * the root filesystem. The main reason is that
			 * the /etc/rc* scripts use this option to turn quotas
			 * on for EFS filesystems, and the semantics for XFS
			 * are a bit different. eg. we don't want to find that
			 * quota enforcement is turned on at boot time on all 
			 * XFS filesystems all of a sudden.
			 */
			if (isxfs && !offmode)
				continue; 

			if (hasmntopt(mntp, MNTOPT_RO))
				continue;
			
			/*
			 * For XFS filesystems, we are better off doing
			 * a QSTAT.
			 */
			if (isxfs) {
				assert(offmode);
				if (getxfsqstat(mntp->mnt_fsname, &qstat) < 0)
					continue;
				if (qstat.qs_flags == 0 && !rmflag)
					continue;
			} else if (!hasmntopt(mntp, MNTOPT_QUOTA)) {
				/* EFS filesystems must have 'quota' mntopt */
				continue; 
			}

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

	/*
	 * Open temporary version of mtab
	 */
	mktemp(tmpname);
	tmp = setmntent(tmpname, "a");
	if (tmp == NULL) {
		perror(tmpname);
		(void) unlink(tmpname);
		exit(1);
	}

	/*
	 * Open real mtab
	 */
	mtab = setmntent(MOUNTED, "r");
	if (mtab == NULL) {
		perror(MOUNTED);
		(void) unlink(tmpname);
		exit(1);
	}
	
	/*
	 * If -f specified, make sure that 'quotaflags' are initialized right;
	 * quotaflags indicated what options need to be switched off or on.
	 */
	if (quotaflags == 0) {
		if (offmode)
			 quotaflags = XFS_QUOTA_ALL;
		 else
			 quotaflags = XFS_QUOTA_UDQ_ACCT | XFS_QUOTA_UDQ_ENFD;
	}
	
	/*
	 * Loop through mtab writing mount record to temp mtab.
	 * If a file system gets turn on or off modify the mount
	 * record before writing it.
	 * XFS filesystems generally don't support quotaon in this fashion.
	 */
	while ((mntp = getmntent(mtab)) != NULL) {
		if ((strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0 ||
		     strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) &&
		    !hasmntopt(mntp, MNTOPT_RO) &&
		    (oneof(mntp->mnt_fsname, listp, listcnt) ||
		     oneof(mntp->mnt_dir, listp, listcnt)) ) {
			
			/*
			 * XFS doesn't support a quotaon utility for non-root
			 * file systems.
			 */
			if (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) {
				errs += quotaonoff_xfs(mntp, offmode, quotaflags);
			} else {
				errs += quotaonoff(mntp, offmode);
			}
		}
		addmntent(tmp, mntp);
	}
	fstat(fileno(mtab), &statb);
	fchmod(fileno(tmp), statb.st_mode & 0777);
	endmntent(mtab);
	endmntent(tmp);
	
	/*
	 * Move temp mtab to mtab
	 */
	if (rename(tmpname, MOUNTED) < 0) {
		perror(MOUNTED);
		(void) unlink(tmpname);
		exit(1);
	}
	while (listcnt--) {
		if (*listp)
			fprintf(stderr, 
				"%s: Operation failed on %s\n", 
				whoami,
				*listp);
		listp++;
	}
	exit(errs);
	/* NOTREACHED */
}

static int
quotaonoff(
	struct mntent *mntp, 
	int offmode)
{
	static int printed;
	int r;
	cap_t ocap;
	cap_value_t cap_quota_mgt = CAP_QUOTA_MGT;

	if (offmode) {
		ocap = cap_acquire(1, &cap_quota_mgt);
		r = quotactl(Q_QUOTAOFF, mntp->mnt_fsname, 0, NULL);
		cap_surrender(ocap);
		if (r < 0)
			goto bad;
		if (vflag)
			fprintf(stderr,
				"%s: Quotas turned off\n", 
				mntp->mnt_dir);
	} else {
		(void) sprintf(quotafile, "%s/%s", mntp->mnt_dir, QFNAME);
		ocap = cap_acquire(1, &cap_quota_mgt);
		r = quotactl(Q_QUOTAON, mntp->mnt_fsname, 0, quotafile);
		cap_surrender(ocap);
		if (r < 0)
			goto bad;
		if (vflag)
			fprintf(stderr, 
				"%s: Quotas turned on\n", 
				mntp->mnt_dir);
	}
	fixmntent(mntp, offmode, 0xffff);
	return (0);
bad:
	if (errno == EEXIST) {
	    if (offmode)
		fprintf(stderr, 
			"%s: Quotas need to be enabled first\n",
			mntp->mnt_dir);
	    else
		fprintf(stderr, 
			"%s: Quotas already enabled\n",
			mntp->mnt_dir);
	} else if (errno == ENOPKG) {
		if (!printed) {
			printed++;
			perror("quotas");
		}
	} else {
	    fprintf(stderr, "quotactl: ");
	    if (!offmode)
		fprintf(stderr, "%s or ", quotafile);
	    perror(mntp->mnt_fsname);
	}
    
	return (1);
}

static int
oneof(
	char 	*target,
	char 	**listp,
	int 	n)
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

char opts[1024];
static void
fixmntent(
	struct mntent 	*mntp,
	int 		offmode,
	uint		qflags)
{
	register char *qst, *qend;
	int present = 0;

	/*
	 * If we had only turned on/off limit enforcement,
	 * we don't bother to change the mtab accordingly.
	 * XXXTurning off quotas after mounting qnoenforce gives
	 * wrong results because it leaves qnoenforce,noquota in mtab.
	 * XXXWe should do this right one of these days.
	 */
	if ((qflags & (XFS_QUOTA_UDQ_ACCT|XFS_QUOTA_PDQ_ACCT)) == 0)
		return;

	if (offmode) {
		if (hasmntopt(mntp, MNTOPT_NOQUOTA))
			present++;
		qst = hasmntopt(mntp, MNTOPT_QUOTA);
	} else {
		if (hasmntopt(mntp, MNTOPT_QUOTA))
			present++;
		qst = hasmntopt(mntp, MNTOPT_NOQUOTA);
	}
	if (qst) {
		qend = index(qst, ',');
		if (qst != mntp->mnt_opts)
			qst--;			/* back up to ',' */
		if (qend == NULL)
			*qst = '\0';
		else
			while (*qst++ = *qend++);
	}
	if (!present) {
		sprintf(opts, "%s,%s", mntp->mnt_opts,
		    offmode? MNTOPT_NOQUOTA : MNTOPT_QUOTA);
		mntp->mnt_opts = opts;
	}
}

static int
getxfsqstat(
	char 		*fsdev,
	fs_quota_stat_t	*qstat)
{

	/*
	 * See if quotas is on. If not, nada.
	 */
	if (quotactl(Q_GETQSTAT, fsdev, 0, (caddr_t)qstat) < 0) {
		perror(fsdev);
		return (-1);
	}
	return (0);
}


static int
quotaonoff_xfs(
	struct mntent 	*mntp,
	int 		offmode,
	uint		qflags)
{
	static int 	printed; 
	uint	   	quotaon, rootfs;
	fs_quota_stat_t	qstat;
	int		prinfo;
	int		r;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;

	/*
	 * See if quotas is on. If not, nada.
	 */
	if (getxfsqstat(mntp->mnt_fsname, &qstat) < 0) {
		goto bad;
	}
	quotaon = qstat.qs_flags & (XFS_QUOTA_UDQ_ACCT | XFS_QUOTA_PDQ_ACCT);
	rootfs = (strcmp(mntp->mnt_dir, "/") == 0);
	prinfo = 0;
	if (offmode) {
		if (quotaon || rootfs) {
			ocap = cap_acquire(1, &cap_quota_mgt);
			r = quotactl(Q_QUOTAOFF, mntp->mnt_fsname, 0,
				     (caddr_t) &qflags);
			cap_surrender(ocap);
			if (r < 0)
				goto bad;
			if (vflag) {
				if (qflags & (XFS_QUOTA_UDQ_ACCT | 
					      XFS_QUOTA_PDQ_ACCT))
					fprintf(stderr,
						"%s: Quotas turned off\n", 
						mntp->mnt_dir);
				else
					fprintf(stderr,
					"%s: Quota enforcement turned off\n", 
					mntp->mnt_dir);
			}
		} else if (vflag) {
			fprintf(stderr,
				"%s: Could not turn quotaoff\n",
				mntp->mnt_dir);
		}

		if (rmflag) {
			if (vflag || 
			    /* Hack: warn user if the quota file is 'big' */
			    ((delflags & XFS_USER_QUOTA) && 
			      qstat.qs_uquota.qfs_nblks > 8000) ||
			    ((delflags & XFS_PROJ_QUOTA) && 
			      qstat.qs_pquota.qfs_nblks > 8000)) {
				fprintf(stderr, 
					"%s: Attempting to free quota info\n",
					 mntp->mnt_dir);
				prinfo++;
			}
			ocap = cap_acquire(1, &cap_quota_mgt);
			r = quotactl(Q_QUOTARM, mntp->mnt_fsname, 0,
				     (caddr_t) &delflags);
			cap_surrender(ocap);
			if (r < 0) {
				fprintf(stderr,
					"%s: Could not delete quota info\n",
					mntp->mnt_dir);
				goto bad;
			}
			if (vflag || prinfo)
				fprintf(stderr,
				       "%s: Quota information deleted, "
				       "and blocks freed\n",
				       mntp->mnt_dir);
		}
	} else {
		if (!rootfs && !quotaon) {
			fprintf(stderr,
		      "%s: Must turn on XFS quota accounting at mount time\n",
				mntp->mnt_dir);
			return (1);
		}
		if (!rootfs || (rootfs && quotaon))
			qflags &= (XFS_QUOTA_UDQ_ENFD|XFS_QUOTA_PDQ_ENFD);
		if (qflags == 0)
			return (1);

		ocap = cap_acquire(1, &cap_quota_mgt);
		r = quotactl(Q_QUOTAON, mntp->mnt_fsname, 0,
			     (caddr_t) &qflags);
		cap_surrender(ocap);
		if (r < 0) {
			if (qflags & (XFS_QUOTA_UDQ_ACCT | XFS_QUOTA_PDQ_ACCT))
				fprintf(stderr,
					"%s: Could not turn quotaon\n", 
					mntp->mnt_dir);
			else
				fprintf(stderr,
				   "%s: Could not turn limit enforcement on\n", 
				   mntp->mnt_dir);
			goto bad;
		}
		if (vflag) {
			fprintf(stderr, 
				"%s: Quotas turned on\n", 
				mntp->mnt_dir);
			if (qflags & (XFS_QUOTA_UDQ_ACCT | XFS_QUOTA_PDQ_ACCT))
				fprintf(stderr, 
					"%s: Please reboot for quotaon to"
					" take effect (root filesystem).\n", 
					whoami);
		}
	}

	fixmntent(mntp, offmode, qflags);
	return (0);
bad:
	if (errno == EEXIST || errno == ESRCH) {
	    if (offmode)
		fprintf(stderr, 
			"%s: Quotas need to be enabled first\n",
			mntp->mnt_dir);
	    else
		fprintf(stderr, 
			"%s: Quotas already enabled\n",
			mntp->mnt_dir);
	} else if (errno == ENOPKG) {
	    if (!printed) {
		printed++;
		perror("quotas");
	    }
	} else {
	    fprintf(stderr, "quotactl: ");
	    perror(mntp->mnt_fsname);
	}
	return (1);
}

/*
 * We'd like to give the same quota/qnoenforce sort of
 * syntax like mount to quotaon/quotaoff also, but the difference
 * here is that these flags are not supposed be absolute values like
 * mount flags. For example, quotaon can't turn something off.
 * Hence these.
 */

static char *quotaopts[] = { 
#define OPT_ENFORCE	0
	"enforce",
#define	OPT_UQENFORCE	1
	"uqenforce",

#ifndef _NOPROJQUOTAS
#define OPT_QUOTA	2
	"quota",
#define	OPT_UQUOTA	3
	"uquota",
#define OPT_PQUOTA	4
	"pquota",
#define OPT_PQENFORCE	5
	"pqenforce",
#endif
	0
};


static void
parseopts(
	int	*argc,
	char	***argv)
{	
	char		*value;
	int		c, d;
	char		*options;
	char		*getoptstr;
	extern int	optind;
	extern char	*optarg;

#ifndef _NOPROJQUOTAS
	quotaflags = 0;
#else
	if (offmode)
		quotaflags = 0;
	else
		quotaflags = XFS_QUOTA_UDQ_ACCT | XFS_QUOTA_UDQ_ENFD;
#endif
	delflags = 0;
	if (offmode)
		getoptstr = "vado:";
	else
		getoptstr = "vao:";

	while ((c = getopt(*argc, *argv, getoptstr)) != EOF) {
		switch (c) {
		      case 'v':
			vflag++;
			break;
			
		      case 'a':
			aflag++;
			break;
			
		      case 'd':
			if (!offmode)
				usage();
			rmflag++;
#ifndef _NOPROJQUOTAS			
			options = optarg;
			while (options && *options != '\0') {
				d = getsubopt(&options, quotaopts, &value);
				switch (d) {
				      case OPT_QUOTA:
				      case OPT_UQUOTA:
					delflags |= XFS_QUOTA_UDQ_ACCT;
					break;

				      case OPT_PQUOTA:
					delflags |= XFS_QUOTA_PDQ_ACCT;
					break;

				      default:
					fprintf(stderr,
					   "%s: Invalid -%c suboption '%s'\n",
					   whoami, (char) c, value);
					usage();
					break;
				}
			}
			if (delflags == 0)
				delflags = XFS_PROJ_QUOTA | XFS_USER_QUOTA;
				
#else
			delflags = XFS_USER_QUOTA;
#endif
			break;

		      case 'o':
#ifdef _NOPROJQUOTAS
			if (!offmode)
				usage();
#endif			
			oflag++;
			options = optarg;
			while (options && *options != '\0') {
				d = getsubopt(&options, quotaopts, &value);
				switch (d) {
				      case OPT_ENFORCE:
				      case OPT_UQENFORCE:
					quotaflags |= XFS_QUOTA_UDQ_ENFD;
					break;
#ifndef _NOPROJQUOTAS						
				      case OPT_QUOTA:
				      case OPT_UQUOTA:
					quotaflags |= XFS_QUOTA_UDQ_ACCT | 
						XFS_QUOTA_UDQ_ENFD;
					break;

				      case OPT_PQUOTA:
					quotaflags |= XFS_QUOTA_PDQ_ACCT |
						XFS_QUOTA_PDQ_ENFD;
					break;
			
				      case OPT_PQENFORCE:
					quotaflags |= XFS_QUOTA_PDQ_ENFD;
					break;
#endif						
				      default:
					fprintf(stderr,
					   "%s: Invalid -%c suboption '%s'\n",
					   whoami, (char) c, value);
					usage();
					break;
				};
			}
			/*
			 * If using -o, must have valid options.
			 */
			if (quotaflags == 0)
				usage();
			break;

		      default:
			usage();
			break;
		}
	}
	if (rmflag && !offmode)
		usage();
	
	*argc -= optind;
	*argv = &((*argv)[optind]);
}

static void
usage(void)
{
#ifndef _NOPROJQUOTAS
	if (offmode)
		fprintf(stderr, 
			"Usage:\n"
			"\t%s [-v] [-d flags] [-o flags] -a\n"
			"\t%s [-v] [-d flags] [-o flags] filesys ...\n"
			"\t\t -d = DELETE given 'quota file(s)' [XFS]\n"
			"\t\t      -d uquota,pquota\n"
			"\t\t -o = quota options to get switched off [XFS]\n"
			"\t\t      -o quota,uquota,pquota,uqenforce,pqenforce\n"
			"\t\t -v = display verbose messages\n",
			whoami, whoami);
	else
		fprintf(stderr, 
			"Usage:\n"
			"\t%s [-v] [-o flags] -a\n"
			"\t%s [-v] [-o flags] filesys ...\n"
			"\t\t -o = quota options to get switched on [XFS]\n"
			"\t\t      -o quota,uquota,pquota,uqenforce,pqenforce\n"
			"\t\t -v = display verbose messages\n",
			whoami, whoami);
#else
	if (offmode)
		fprintf(stderr, 
			"Usage:\n"
			"\t%s [-vd] -a\n"
			"\t%s [-vd] filesys ...\n"
			"\t%s [-o enforce] filesys ...\n"
			"\t\t -d = DELETE all quota info ondisk\n",
			whoami, whoami, whoami);
	else
		fprintf(stderr, 
			"Usage:\n"
			"\t%s [-v] -a\n"
			"\t%s [-v] filesys ...\n",
			whoami, whoami);
#endif
	exit(1);
}
