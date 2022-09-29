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
 * "@(#)repquota.c	1.2 88/03/15 4.0NFSSRC";
 */

#ident  "$Revision: 1.21 $"
/*
 * Quota report
 */
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/quota.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <mntent.h>
#include <pwd.h>
#include <proj.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/capability.h>

#undef dbtob
#define dbtob BBTOB
#define kb(n) ((int)(howmany(dbtob(n), 1024)))
#define xfskb(n) ((unsigned long long) ((dbtob(n) + 1) / 1024))

#define LOGINNAMESIZE	8
typedef struct username {
	struct username *u_next;
	uid_t		u_id;
	char 		u_name[LOGINNAMESIZE + 1];
} username_t;


#define FS_XFS	1
#define FS_EFS	2
#define FS_LOFS	3

#define NHASH 10

username_t *uhead[NHASH];
int	nusers = 0;
int 	nprojects = 0;

#ifndef _NOPROJQUOTAS
username_t *phead[NHASH];
static void 		findprojects(void);
#endif

static struct username *lookup(username_t **, int id);
static struct username *addid(username_t **, int id);
static void 		findusers(void);
static void		header(char *type);
#define usrlookup(uid)	lookup(uhead, (uid))
#define prjlookup(prid)	lookup(phead, (prid))
#define adduid(uid)	addid(uhead, (uid))
#define addprid(prid)	addid(phead, (prid))
static void		fmttime(char *buf, long ftime);
static void		prquota(int uid, struct dqblk *dqp);
static int		prquotaxfs(username_t **head, int uid, 
				   fs_disk_quota_t  *d);
static int		repquotaxfs(username_t **head, int qop, char *fsdev);
static void		prquotaformat(FILE *fp, fs_disk_quota_t *d, char *fsdev);
static int		repquota(char *fsdev, char *fsfile, char *qffile);
static int		printall(char **listp, int listcnt);
static int		getxfsqstat(char *fsdev);
static void		usage(void);
static int 		oneof(char *target, char **listp, int n);
static int		showqstats(struct mntent *);
static void		prtitle(char *fsdev, char *fsdir);

int	vflag;		/* verbose */
int	aflag;		/* all file systems */
int	projflag;	/* display only proj quotas. Default is USR quotas */
int	sflag;		/* status */
int	eflag;		/* output in a format acceptable to edquota */
char 	*listbuf[50];
FILE	*outfilep;

#define QFNAME "quotas"
static void
usage(void)
{
#ifndef _NOPROJQUOTAS
	fprintf(stderr, 
		"Usage:\n\t%s\n\t%s\n%s\n",
		"repquota [-vps] [-e edquota-outfile] -a",
		"repquota [-vps] [-e edquota-outfile] filesys ...",
		"\tp = show projects only\n"
		"\te = output in a format acceptable to edquota\n"
		"\ts = display current XFS specific quota stats\n"
		"\ta = iterate over all filesystems with quotas\n");
#else
	fprintf(stderr, 
		"Usage:\n\t%s\n\t%s\n%s\n",
		"repquota [-vs] [-e edquota-outfile] -a",
		"repquota [-vs] [-e edquota-outfile] filesys ...",
		"\te = output in a format acceptable to edquota\n"
		"\ts = display current XFS specific quota stats\n"
		"\ta = iterate over all filesystems with quotas\n");
#endif
	exit(1);
}

main(
	int 	argc,
	char 	**argv)
{
	register struct mntent 		*mntp;
	char 		**listp;
	int 		listcnt, i;
	FILE		*mounttab;
	int		c;
	char		*getoptstr;
	extern int	optind;
	int		isxfs;
	int		isefs;
	int		qstat;
	
	if (getuid()) {
		fprintf(stderr, "%s: permission denied\n", argv[0]);
		exit(1);
	}
	
#ifndef _NOPROJQUOTAS
	getoptstr = "e:avsp";
#else
	getoptstr = "e:avs";
#endif
	aflag = sflag = vflag = projflag = eflag = 0;

	while ((c = getopt(argc, argv, getoptstr)) != EOF) {
		switch (c) {
		      case 'v':
			vflag++;
			break;
			
		      case 'a':
			aflag++;
			break;

		      case 's':
			sflag++;
			break;
			
		      case 'e':
			if (optarg == NULL)
				usage();
			if ((outfilep = fopen(optarg, "a+")) == NULL) {
				perror(optarg);
				exit(errno);
			}
			eflag++;
			break;

#ifndef _NOPROJQUOTAS
		      case 'p':
			projflag++;
			break;
#endif
		      default:
			usage();
		}
	}
	if (argc - optind <= 0 && !aflag) 
		usage();

	
	for (i = 0; i < NHASH; i++) {
		uhead[i] = NULL;
#ifndef _NOPROJQUOTAS
		phead[i] = NULL;
#endif
	}

	/*
	 * Build the hashtables of all known users and projects
	 */
	findusers();

#ifndef _NOPROJQUOTAS
	findprojects();
#endif
	/*
	 * This is useless for XFS filesystems.
	 */
	if (quotactl(Q_SYNC, NULL, 0, NULL) < 0 && (errno == ENOPKG) && vflag) {
		perror("repquota");
		exit(1);
	}
	/*
	 * If aflag is on, go through mtab and make a list of appropriate
	 * filesystems.
	 */
	if (aflag) {
		listp = listbuf;
		listcnt = 0;
		if ((mounttab = setmntent(MOUNTED, "r")) == NULL) {
			fprintf(stderr, "Can't open ");
			perror(MOUNTED);
			exit(8);
		}
		
		while (mntp = getmntent(mounttab)) {
			isefs = (strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0);
			if (!isefs)
				isxfs = (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0);
			else
				isxfs = 0;
			if (!isefs && !isxfs)
				continue;
	
			/*
			 * If it is XFS root filesystem, we might not be able
			 * to tell if quotaon or not by looking at /etc/mtab.
			 * XXXEven otherwise we can endup with conflicting
			 * mtab entries like noquota,qnoenforce etc.
			 * So, for XFS filesystems, we are better off doing
			 * a QSTAT for the time being.
			 */
			if (isxfs) {
				if ((qstat = getxfsqstat(mntp->mnt_fsname)) < 0)
					continue;
				if (!sflag && (qstat & XFS_QUOTA_UDQ_ACCT) == 0)
					continue;
			} else {
				/* EFS filesystems must have 'quota' mntopt */
				if (!hasmntopt(mntp, MNTOPT_QUOTA)) 
					continue;
				/* They must also not be ReadOnly mounted */
				if (hasmntopt(mntp, MNTOPT_RO))
					continue;
			}
			
			*listp = malloc(strlen(mntp->mnt_fsname) + 1);
			strcpy(*listp, mntp->mnt_fsname);
			listp++;
			listcnt++;
		}
		(void) endmntent(mounttab);
		*listp = (char *)0;
		listp = listbuf;
	} else {
		listp = &argv[optind];
		listcnt = argc - optind;
	}
	exit(printall(listp, listcnt));
	/* NOTREACHED */
}

int
repquota(
	char *fsdev,
	char *fsfile,
	char *qffile)
{
	FILE *qf;
	struct dqblk dqbuf;
	struct stat statb;
	struct username *uidp;
	int i;
	cap_t ocap;
	cap_value_t cap_quota_mgt = CAP_QUOTA_MGT;
	extern int errno;

	if (vflag || aflag)
		prtitle(fsdev, fsfile);
	qf = fopen(qffile, "r");
	if (qf == NULL) {
		perror(qffile);
		return (1);
	}
	if (fstat(fileno(qf), &statb) < 0) {
		perror(qffile);
		fclose(qf);
		return (1);
	}
	if (statb.st_uid) {
	    fprintf(stderr, "quotacheck: %s not owned by root\n", qffile);
	    fclose(qf);
	    return(1);
	}
	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_ACTIVATE, fsdev, 0, qffile)) {
	    cap_surrender(ocap);
	    fprintf(stderr,"Error reading quotas for %s: ", fsdev);
	    perror("");
	    fclose(qf);
	    return(1);
	}
	cap_surrender(ocap);
	header("User");
	for (i=0; i < NHASH; i++) {
	    uidp = uhead[i];
	    for (uidp = uhead[i]; uidp; uidp = uidp->u_next) {
		ocap = cap_acquire(1, &cap_quota_mgt);
		if (quotactl(Q_GETQUOTA, fsdev, (int)uidp->u_id, 
			     (caddr_t)&dqbuf)) {
			cap_surrender(ocap);
			if (errno == ENOENT || errno == ESRCH ||
			    errno == EINVAL || errno == ENOTSUP)
				continue;
			perror(qffile);
			continue;
		}
		cap_surrender(ocap);
		if (!vflag &&
		    dqbuf.dqb_curfiles == 0 && dqbuf.dqb_curblocks == 0)
		    continue;
		prquota((int)uidp->u_id, &dqbuf);
	    }
	}
	fclose(qf);
	return (0);
}

int
repquotaxfs(
	username_t 	**head, 
	int 		quotaop, 
	char 		*fsdev)
{
	fs_disk_quota_t dqbuf;
	struct username *idp;
	int 		i;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;
	extern int 	errno;
	if (!eflag)
		header(quotaop == Q_XGETPQUOTA ? "Project" : "User");
	for (i=0; i < NHASH; i++) {
	    idp = head[i];
	    for (idp = head[i]; idp; idp = idp->u_next) {
		ocap = cap_acquire(1, &cap_quota_mgt);
		if (quotactl(quotaop, fsdev, (int) idp->u_id,
			     (caddr_t)&dqbuf) < 0) {
		    cap_surrender(ocap);
		    if (errno == ENOENT || errno == ESRCH ||
			errno == EINVAL || errno == ENOTSUP)
			    continue;
		    perror("repquota");
		    continue;
		}
		cap_surrender(ocap);
		if (eflag) {
			prquotaformat(outfilep, &dqbuf, fsdev);
			if (vflag)
				prquotaformat(stdout, &dqbuf, fsdev);
			continue;
		}

		if (!vflag &&
		    dqbuf.d_icount == 0 && dqbuf.d_bcount == 0)
		    continue;
		prquotaxfs(head, (int) idp->u_id, &dqbuf);
	    }
	}
	if (!eflag)
		printf("\n");
	return (0);
}

void
header(
	char *type)
{

	printf(
"                      Disk limits                       File limits\n"
	);
	printf(
"%s           used    soft    hard    timeleft   used    soft    hard    timeleft\n",
	       type);
}

#define X_BSOFT(d)	((unsigned long long) (d)->d_blk_softlimit)
#define X_BHARD(d)	((unsigned long long) (d)->d_blk_hardlimit)
#define X_ISOFT(d)	((unsigned long long) (d)->d_ino_softlimit)
#define X_IHARD(d)	((unsigned long long) (d)->d_ino_hardlimit)
#define X_BCOUNT(d)	((unsigned long long) (d)->d_bcount)
#define X_ICOUNT(d)	((unsigned long long) (d)->d_icount)
#define X_BTIMER(d)	(d)->d_btimer
#define X_ITIMER(d)	(d)->d_itimer

int
prquotaxfs(
	username_t	     **head,
	int		       id,
	fs_disk_quota_t       *d)
{

	struct timeval tv;
	char ftimeleft[80], btimeleft[80];
	register struct username *up;

	gettimeofday(&tv, NULL);
	up = lookup(head, id);
	if (up)
		printf("%-10s", up->u_name);
	else
		printf("#%-9d", (int) id);
	
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
	printf("%c%c%7llu %7llu %7llu%12s%7llu %7llu %7llu %12s\n",
	       (X_BSOFT(d) && X_BCOUNT(d) >= X_BSOFT(d)) ? '+' : '-',
	       (X_ISOFT(d) && X_ICOUNT(d) >= X_ISOFT(d)) ? '+' : '-',
	       xfskb(X_BCOUNT(d)),
	       xfskb(X_BSOFT(d)),
	       xfskb(X_BHARD(d)),
	       btimeleft,
	       X_ICOUNT(d),
	       X_ISOFT(d),
	       X_IHARD(d),
	       ftimeleft
	       );
	return (0);
}

static void
prquotaformat(
	FILE		*fp,
	fs_disk_quota_t *d,
	char		*fsdev)
{
	fprintf(fp,
		"fs = %s\n",
		fsdev);
	fprintf(fp, 
		"%-10d %7llu %7llu %7llu %7llu\n",
		d->d_id,
		X_BSOFT(d),
		X_BHARD(d),
		X_ISOFT(d),
		X_IHARD(d)
		);
	
}

static void
prquota(
	int 		uid,
	struct dqblk 	*dqp)
{
	struct timeval tv;
	register struct username *up;
	char ftimeleft[80], btimeleft[80];

	if (dqp->dqb_bsoftlimit == 0 && dqp->dqb_bhardlimit == 0 &&
	    dqp->dqb_fsoftlimit == 0 && dqp->dqb_fhardlimit == 0)
		return;
	(void) gettimeofday(&tv, NULL);
	up = usrlookup(uid);
	if (up)
		printf("%-10s", up->u_name);
	else
		printf("#%-9d", (int) uid);
	if (dqp->dqb_bsoftlimit && dqp->dqb_curblocks>=dqp->dqb_bsoftlimit) {
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
	if (dqp->dqb_fsoftlimit && dqp->dqb_curfiles>=dqp->dqb_fsoftlimit) {
		if (dqp->dqb_ftimelimit == 0) {
			strcpy(ftimeleft, "NOT STARTED");
		} else if (dqp->dqb_ftimelimit > tv.tv_sec) {
			fmttime(ftimeleft,
			    (long)(dqp->dqb_ftimelimit - tv.tv_sec));
		} else {
			strcpy(ftimeleft, "EXPIRED");
		}
	} else {
		ftimeleft[0] = '\0';
	}
	printf("%c%c%7d %7d %7d%12s%7d %7d %7d %12s\n",
		dqp->dqb_bsoftlimit && 
		    dqp->dqb_curblocks >= 
		    dqp->dqb_bsoftlimit ? '+' : '-',
		dqp->dqb_fsoftlimit &&
		    dqp->dqb_curfiles >=
		    dqp->dqb_fsoftlimit ? '+' : '-',
		kb(dqp->dqb_curblocks),
		kb(dqp->dqb_bsoftlimit),
		kb(dqp->dqb_bhardlimit),
		btimeleft,
		(int) dqp->dqb_curfiles,
		(int) dqp->dqb_fsoftlimit,
		(int) dqp->dqb_fhardlimit,
		ftimeleft
	);
}

static void
fmttime(
	char 	*buf,
	long 	ftime)
{
	int 	i;
	double  value;
	static struct {
		int c_secs;		/* conversion units in secs */
		char * c_str;		/* unit string */
	} cunits [] = {
		{60*60*24*28, "month"},
		{60*60*24*7, "week"},
		{60*60*24, "day"},
		{60*60, "hour"},
		{60, "min"},
		{1, "sec"}
	};

	if (ftime <= 0) {
		strcpy(buf, "EXPIRED");
		return;
	}
	for (i = 0; i < sizeof(cunits)/sizeof(cunits[0]); i++) {
		if (ftime >= cunits[i].c_secs)
			break;
	}
	value = ftime/cunits[i].c_secs;
	sprintf(buf, "%.1f %s%s", value, cunits[i].c_str, value > 1.0? "s": "");
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

username_t *
lookup(username_t **head, int id)
{
	username_t *up, *hash;
	
	hash = head[(u_short)id % NHASH];

	for (up = hash; up != 0; up = up->u_next)
		if (id == (int) up->u_id)
			return (up);
	return ((struct username *)0);
}

username_t *
addid(username_t **head, int id)
{
	username_t *up, **uhp;

	if (up = lookup(head, id))
		return (up);
	up = (username_t *)calloc(1, sizeof(username_t));
	if (up == 0) {
		fprintf(stderr, "out of memory for username structures\n");
		exit(1);
	}
	uhp = &head[(u_short)id % NHASH];
	up->u_next = *uhp;
	*uhp = up;
	up->u_id = id;
	return (up);
}

void
findusers(void)
{
	register struct passwd 		*pwp;
	register struct username 	*up;
	
	(void) setpwent();
	while ((pwp = getpwent()) != 0) {
		up = usrlookup((int)pwp->pw_uid);
		if (up == 0) {
			up = adduid((int)pwp->pw_uid);
			strncpy(up->u_name, pwp->pw_name, sizeof(up->u_name));
			nusers++;
		}
	}
	(void) endpwent();
}

#ifndef _NOPROJQUOTAS
void
findprojects(void)
{
	PROJ		token;
	projid_t	*projarr;
	int		nprojs, n;
	username_t	*up;

	/* XXX what if no projects at all ???  is that possible ? */
	if ((token = openproj(NULL, NULL)) == NULL) {
		perror("projects");
		exit(1);
	}
	nprojs = 1;
	projarr = (projid_t *) malloc(nprojs * sizeof(projid_t));

	while ((n = fgetprojall(token, projarr, nprojs)) == nprojs) {
		nprojs *= 2;
		projarr = (projid_t *) realloc(projarr, nprojs * 
					       sizeof(projid_t));
	}
	closeproj(token);
	
	if (n > 0) {
		while (n--) {
			up = prjlookup((int)projarr[n].proj_id);
			if (up == 0) {
				up = addprid((int)projarr[n].proj_id);
				strncpy(up->u_name, 
					projarr[n].proj_name, 
					MAXPROJNAMELEN);
				nprojects++;
			}
		}
		
	}
	free(projarr);
}
#endif

static int
getxfsqstat(
	char *fsdev)
{
	fs_quota_stat_t	qstat;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;

	/*
	 * See if quotas is on. If not, nada.
	 */
	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_GETQSTAT, fsdev, 0, 
		     (caddr_t)&qstat) < 0) {
		cap_surrender(ocap);
		perror(fsdev);
		return (-1);
	}
	cap_surrender(ocap);
	return ((int)qstat.qs_flags);
}

int
printall(
	char	**listp,
	int 	listcnt)
{
	struct mntent 		*mntp;
	FILE 			*mtab;
	int 			errs;
	char 			quotafile[MAXPATHLEN + 1];
	int			qstat;
	int			nqstats;
	int			fstype;
	int			firsttime;
	int 			i;
	char 			**s;
	struct stat 		st;
	int			nfails;

	firsttime = 1;
	errs = 0;
	nqstats = 0;

 again:
	if ((mtab = setmntent(MOUNTED, "r")) == NULL) {
		fprintf(stderr, "Can't open ");
		perror(MOUNTED);
		exit(8);
	}
	
	while (mntp = getmntent(mtab)) {
		if (strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0)
			fstype = FS_XFS;
		else if (strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0)
			fstype = FS_EFS;
		else if (strcmp(mntp->mnt_type, MNTTYPE_LOFS) == 0)
			fstype = FS_LOFS;
		else
			continue;
		
		/* 
		 * The name's gotta match for us to do anything.
		 */
		if (!(oneof(mntp->mnt_fsname, listp, listcnt) ||
		      oneof(mntp->mnt_dir, listp, listcnt)))
			continue;

		/*
		 * If we have a loopback into a local filesystem, 
		 * we need to add the real name of that to our list of
		 * filesystems to report.
		 * Yet another hack for yet another thing. 
		 * This is getting really ugly.
		 */
		if (fstype == FS_LOFS) {
			listp--;
			*listp = malloc(strlen(mntp->mnt_fsname) + 1);
			strcpy(*listp, mntp->mnt_fsname);
			listcnt++;	
			(void) endmntent(mtab);
			goto again;
		}
		
		if (fstype == FS_XFS) {
			if (sflag) {
				if (showqstats(mntp) == 0) {
					nqstats++;
					if (aflag)
						printf("\n");
							
				}
				continue;
			}
			
			if ((qstat = getxfsqstat(mntp->mnt_fsname)) < 0)
				continue;
			if (vflag && (qstat &
				      (XFS_QUOTA_UDQ_ACCT | 
				       XFS_QUOTA_PDQ_ACCT)) == 0) {
				printf("No disk quotas on %s (%s)\n",
				       mntp->mnt_fsname,
				       mntp->mnt_dir);
				continue;
			}
			
			if (! ((nusers &&
				(qstat & XFS_QUOTA_UDQ_ACCT)) ||
			       (nprojects &&
				(qstat & XFS_QUOTA_PDQ_ACCT)))) {
				if (vflag)
					printf("Nothing to "
					       "report on %s\n",
					       mntp->mnt_fsname);
				continue;
				
			}
			
			/*
			 * Print the filesystem name.
			 */
			if (!eflag || vflag) 
				prtitle(mntp->mnt_fsname, mntp->mnt_dir);
			
			if (nusers)
				errs += repquotaxfs(uhead, 
						    Q_XGETQUOTA,
						    mntp->mnt_fsname);
			else if (vflag)
				printf("No user quotas to "
				       "report on %s\n",
				       mntp->mnt_fsname);
#ifndef _NOPROJQUOTAS
			if (nprojects)
				errs += repquotaxfs(phead, 
						    Q_XGETPQUOTA,
						    mntp->mnt_fsname);
			else if (vflag)
				printf("No project quotas to report on"
				       " %s\n",
				       mntp->mnt_fsname);
#endif	
		} else {
			/*
			 * EFS. Don't bother about READONLY mounts.
			 */
			if (!sflag && !eflag && !hasmntopt(mntp, MNTOPT_RO)) {
				sprintf(quotafile, "%s/%s",
					mntp->mnt_dir, QFNAME);
				errs += repquota(mntp->mnt_fsname,mntp->mnt_dir,
						 quotafile);
			}
		}	
		
	}
	(void) endmntent(mtab);
	
	/*
	 * If the user was trying to do a repquota on a lofs filesystem,
	 * then it's possible that it was only soft-mounted. So, do a stat
	 * to trigger the mount in that case.
	 */
	if (listcnt) {
		i = listcnt;
		s = listp;
		nfails = 0;

		while (i--) {
			if (*s) {
				if (firsttime) {
					if (stat(*s,&st) < 0)
						nfails++;
				} else {
					fprintf(stderr, 
					"repquota: cannot report on %s\n", *s);
				}
			}
			s++;
		}
		/*
		 * If stat failed on all of them, no need to go back.
		 * The assumption here is that stat doesn't return -1 on lofs
		 * filesystems (that aren't yet mounted).
		 */
		if (firsttime && listcnt > nfails) {
			firsttime = 0;
			goto again;
		}
	}

	if (vflag && sflag && nqstats == 0)
		fprintf(stderr,
			"repquota: no filesystem quota statistics to report\n");
	if (eflag && outfilep)
		fclose(outfilep);
	return (errs);
}


#define ONOROFF(a) ((a) ? "on" : "off")

static int
showqstats(
	struct mntent 	*mntp)
{
	fs_quota_stat_t	qstat;
	char 		btime[80], itime[80];
	u_int16_t	sbflags;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;

	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_GETQSTAT, mntp->mnt_fsname, 0, (caddr_t)&qstat) < 0) {
		cap_surrender(ocap);
		perror(mntp->mnt_fsname);
		return (-1);
	}
	cap_surrender(ocap);
	/*
	 * Print the filesystem name.
	 */
	prtitle(mntp->mnt_fsname, mntp->mnt_dir);

	if (qstat.qs_version != FS_QSTAT_VERSION) {
		fprintf(stderr, "qstat: Unknown version\n");
		fprintf(stderr, "Expected %d, Q_GETQSTAT returned version %d\n",
			(int) FS_QSTAT_VERSION, (int) qstat.qs_version);
		return (-1);
	}
	
	/*
	 * Quotaon/off flags.
	 */
	fprintf(stderr,
		"Status\n");
	fprintf(stderr,
		"\tuser quota accounting       : %s\n"
		"\tuser quota limit enforcement: %s\n"
#ifndef _NOPROJQUOTAS
		"\tproj quota accounting       : %s\n"
		"\tproj quota limit enforcement: %s\n"
#endif
		,
		ONOROFF(qstat.qs_flags & XFS_QUOTA_UDQ_ACCT),
		ONOROFF(qstat.qs_flags & XFS_QUOTA_UDQ_ENFD)
#ifndef _NOPROJQUOTAS
		,
		ONOROFF(qstat.qs_flags & XFS_QUOTA_PDQ_ACCT),
		ONOROFF(qstat.qs_flags & XFS_QUOTA_PDQ_ENFD)
#endif
		);
	/*
	 * If this is the root file system, it is possible that the
	 * quotas are turned on ondisk, but not incore. Those flags
	 * will be in the HI 8 bits.
	 */
	if (sbflags = (qstat.qs_flags & 0xff00) >> 8) {
		fprintf(stderr,
			"\tuser quota accounting [ondisk]       : %s\n"
			"\tuser quota limit enforcement [ondisk]: %s\n"
#ifndef _NOPROJQUOTAS
			"\tproj quota accounting [ondisk]       : %s\n"
			"\tproj quota limit enforcement [ondisk]: %s\n"
#endif
			,
			ONOROFF(sbflags & XFS_QUOTA_UDQ_ACCT),
			ONOROFF(sbflags & XFS_QUOTA_UDQ_ENFD)
#ifndef _NOPROJQUOTAS
			,
			ONOROFF(sbflags & XFS_QUOTA_PDQ_ACCT),
			ONOROFF(sbflags & XFS_QUOTA_PDQ_ENFD)
#endif
			);
	}

	/*
	 * qfilestat stuff.
	 */
	fprintf(stderr,
		"Quota Storage\n");
	if (qstat.qs_uquota.qfs_ino == -1)
		fprintf(stderr, 
			"\tuser quota inode <null>\n");		
	else
		fprintf(stderr, 
		"\tuser quota inum %lld, blocks %llu, extents %lu\n",
		qstat.qs_uquota.qfs_ino,
		qstat.qs_uquota.qfs_nblks,
		qstat.qs_uquota.qfs_nextents);		
#ifndef _NOPROJQUOTAS
	if (qstat.qs_pquota.qfs_ino == -1)
		fprintf(stderr, 
			"\tproj quota inode <null>\n");
	else
		fprintf(stderr, 
		"\tproj quota inum %lld, blocks %llu, extents %lu\n",
		qstat.qs_pquota.qfs_ino,
		qstat.qs_pquota.qfs_nblks,
		qstat.qs_pquota.qfs_nextents);
#endif
	if (qstat.qs_flags) {
		fprintf(stderr,
			"Default Limits\n");
		if (qstat.qs_btimelimit > 0)
			fmttime(btime, qstat.qs_btimelimit);
		else 
			strcpy(btime, "<uninitialized>");
		if (qstat.qs_itimelimit > 0)
			fmttime(itime, qstat.qs_itimelimit);
		else
			strcpy(itime, "<uninitialized>");
		fprintf(stderr, 
			"\tblocks time limit: %s\n"
			"\tfiles  time limit: %s\n",
			btime, itime);
		fprintf(stderr, 
			"Cache\n");
		fprintf(stderr, 
			"\tdquots currently cached in memory: %d\n",
			qstat.qs_incoredqs);
	}
	return (0);
}

static void
prtitle(
	char 	*fsdev,
	char 	*fsdir)
{
	char	str[255];
	int	i;

	sprintf(str, "%s (%s):", 
		fsdev, fsdir);
		
	printf("%s\n", str);
	for (i = 0; i < strlen(str); i++)
		printf("-");
	printf("\n");
}
