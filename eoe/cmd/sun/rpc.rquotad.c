#ifndef lint
static char sccsid[] = 	"@(#)rpc.rquotad.c	1.2 88/08/03 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */
#define _BSD_SIGNALS

#include <stdio.h>
#include <syslog.h>
#include <mntent.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/quota.h>
#include <sys/errno.h>
#include <signal.h>
#include <rpc/rpc.h>
#include <rpcsvc/rquota.h>
#include <sys/capability.h>

#define QFNAME	"quotas"
#define DEV_BSIZE BBSIZE
#define CLOSEDOWN 120

void rquota_service();
void log_cant_reply();

static int	getefsquota(struct svc_req *rqstp,
			    struct fsquot *fsqp,
			    struct getquota_args *gqa,
			    struct getquota_rslt *gqr);
static int	getxfsquota(struct svc_req *rqstp,
			    struct fsquot *fsqp,
			    struct getquota_args *gqa,
			    struct getquota_rslt *gqr);
static int	getxfsqstat(char *fsdev);
static void	getquota(struct svc_req *rqstp, SVCXPRT *transp);
static void	setupfs(void);
static void	freefs(void);

typedef enum {
  FS_NONE, FS_XFS, FS_EFS
} quota_fstype_t;

struct fsquot {
	struct fsquot 	*fsq_next;
	char 		*fsq_dir;
	char 		*fsq_devname;
	dev_t 		fsq_dev;
	quota_fstype_t	fsq_type;
};

struct fsquot *fsqlist = NULL;
int nosys = 0;
int dirty;
void closedown();

typedef struct authunix_parms *authp;

extern int errno;

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char **argv;
{
	register SVCXPRT *transp;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);

	openlog("rquotad", LOG_PID, LOG_DAEMON);

#ifdef DEBUG
	{
		int s;
		struct sockaddr_in addr;
		int len = sizeof(struct sockaddr_in);

		if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			syslog(LOG_ERR, "socket: %m");
			return -1;
		}
		if (bind(s, &addr, sizeof(addr)) < 0) {
			syslog(LOG_ERR, "bind: %m");
			return -1;
		}
		if (getsockname(s, &addr, &len) != 0) {
			syslog(LOG_ERR, "getsockname: %m");
			(void)close(s);
			return -1;
		}
		pmap_unset(RQUOTAPROG, RQUOTAVERS);
		pmap_set(RQUOTAPROG, RQUOTAVERS, IPPROTO_UDP,
		    ntohs(addr.sin_port));
		if (dup2(s, 0) < 0) {
			syslog(LOG_ERR, "dup2: %m");
			exit(1);
		}
	}
#endif	

	if (getsockname(0, &addr, &len) != 0) {
		syslog(LOG_ERR, "getsockname: %m");
		exit(1);
	}
	if ((transp = svcudp_create(0)) == NULL) {
		syslog(LOG_ERR, "couldn't create UDP transport");
		exit(1);
	}
	if (!svc_register(transp, RQUOTAPROG, RQUOTAVERS, rquota_service, 0)) {
		syslog(LOG_ERR, "couldn't register RQUOTAPROG");
		exit(1);
	}
	signal(SIGALRM, closedown);
	alarm(CLOSEDOWN);
	svc_run();		/* never returns */
	syslog(LOG_ERR, "Error: svc_run shouldn't have returned");
}

void
rquota_service(rqstp, transp)
	register struct svc_req *rqstp;
	register SVCXPRT *transp;
{

	dirty = 1;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		errno = 0;
		if (!svc_sendreply(transp, xdr_void, 0))
			log_cant_reply(transp);
		return;

	case RQUOTAPROC_GETQUOTA:
	case RQUOTAPROC_GETACTIVEQUOTA:
		getquota(rqstp, transp);
		return;

	default: 
		svcerr_noproc(transp);
		return;
	}
}

static int
getxfsquota(
	struct svc_req 	    	 *rqstp,
	struct fsquot 	     	*fsqp,
	struct getquota_args 	*gqa,
	struct getquota_rslt 	*gqr)
{
	fs_disk_quota_t	     	dq;
	struct timeval 		tv;
	cap_t			ocap;
	cap_value_t		cap_quota_mgt = CAP_QUOTA_MGT;

	bzero(gqr, sizeof(struct getquota_rslt));

	/*
	 * XFS keeps accounting even when limits aren't set.
	 * So, if GETQUOTA returns error, we don't need to investigate
	 * further; if it doesn't we do know that everything's fine.
	 */
	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_XGETQUOTA,
		     fsqp->fsq_devname, gqa->gqa_uid, 
		     (caddr_t)&dq) != 0) {
		cap_surrender(ocap);
		gqr->gqr_status = Q_NOQUOTA;
		return (-1);
	}
	cap_surrender(ocap);

	gettimeofday(&tv, NULL);
	gqr->gqr_status = Q_OK;
	gqr->gqr_rquota.rq_active = TRUE;
	gqr->gqr_rquota.rq_bsize = DEV_BSIZE;
	gqr->gqr_rquota.rq_bhardlimit = dq.d_blk_hardlimit;
	gqr->gqr_rquota.rq_bsoftlimit = dq.d_blk_softlimit;
	gqr->gqr_rquota.rq_curblocks = dq.d_bcount;
	gqr->gqr_rquota.rq_fhardlimit = dq.d_ino_hardlimit;
	gqr->gqr_rquota.rq_fsoftlimit = dq.d_ino_softlimit;
	gqr->gqr_rquota.rq_curfiles = dq.d_icount;
	gqr->gqr_rquota.rq_btimeleft = dq.d_btimer - tv.tv_sec;
	gqr->gqr_rquota.rq_ftimeleft = dq.d_itimer - tv.tv_sec;
	
	return (0);
}

static int
getefsquota(
	struct svc_req 	     *rqstp,
	struct fsquot 	     *fsqp,
	struct getquota_args *gqa,
	struct getquota_rslt *gqr)
{
	struct dqblk 	dqblk;
	bool_t 		qactive;
	struct timeval 	tv;
	cap_t		ocap;
	cap_value_t	cap_quota_mgt = CAP_QUOTA_MGT;

	bzero(gqr, sizeof(struct getquota_rslt));

	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_GETQUOTA, fsqp->fsq_devname, 
		     gqa->gqa_uid, (caddr_t)&dqblk) != 0) {
		cap_surrender(ocap);
		ocap = NULL;
		qactive = FALSE;
		if (errno == ENOPKG)
			nosys++;
		if (nosys || rqstp->rq_proc != RQUOTAPROC_GETQUOTA ||
		    !getdiskquota(fsqp, gqa->gqa_uid, &dqblk)) {
			gqr->gqr_status = Q_NOQUOTA;
			return (-1);
		}
	} else {
		qactive = TRUE;
	}
	cap_surrender(ocap);
	if (dqblk.dqb_bhardlimit == 0 && dqblk.dqb_bsoftlimit == 0 &&
	    dqblk.dqb_fhardlimit == 0 && dqblk.dqb_fsoftlimit == 0) {
		gqr->gqr_status = Q_NOQUOTA;
		return (-1);
	}
	/*
	 * We send the remaining time instead of the absolute time
	 * because clock skew between machines should be much greater
	 * than rpc delay.
	 */
	gettimeofday(&tv, NULL);
	gqr->gqr_status = Q_OK;
	gqr->gqr_rquota.rq_active = qactive;
	gqr->gqr_rquota.rq_bsize = DEV_BSIZE;
	gqr->gqr_rquota.rq_bhardlimit = dqblk.dqb_bhardlimit;
	gqr->gqr_rquota.rq_bsoftlimit = dqblk.dqb_bsoftlimit;
	gqr->gqr_rquota.rq_curblocks = dqblk.dqb_curblocks;
	gqr->gqr_rquota.rq_fhardlimit = dqblk.dqb_fhardlimit;
	gqr->gqr_rquota.rq_fsoftlimit = dqblk.dqb_fsoftlimit;
	gqr->gqr_rquota.rq_curfiles = dqblk.dqb_curfiles;
	gqr->gqr_rquota.rq_btimeleft = dqblk.dqb_btimelimit - tv.tv_sec;
	gqr->gqr_rquota.rq_ftimeleft = dqblk.dqb_ftimelimit - tv.tv_sec;

	return (0);
}

static void
getquota(
	struct svc_req *rqstp,
	SVCXPRT *transp)
{
	struct getquota_args gqa;
	struct getquota_rslt gqr;

	struct fsquot *fsqp;
	extern struct fsquot *findfsq();

	gqa.gqa_pathp = NULL;		/* let xdr allocate the storage */
	if (!svc_getargs(transp, xdr_getquota_args, &gqa)) {
		svcerr_decode(transp);
		return;
	}
	/*
	 * This authentication is really bogus with the current rpc
	 * authentication scheme. One day we will have something for real.
	 */
	if (rqstp->rq_cred.oa_flavor != AUTH_UNIX ||
	    ( ((authp)rqstp->rq_clntcred)->aup_uid != 0 &&
	      ((authp)rqstp->rq_clntcred)->aup_uid != gqa.gqa_uid) ) {
		gqr.gqr_status = Q_EPERM;
		goto sendreply;
	}
	fsqp = findfsq(gqa.gqa_pathp);
	if (fsqp == NULL) {
		gqr.gqr_status = Q_NOQUOTA;
		goto sendreply;
	}
	if (fsqp->fsq_type == FS_EFS) {
		(void) getefsquota(rqstp, fsqp, &gqa, &gqr);
	} else if (fsqp->fsq_type == FS_XFS) {
		(void) getxfsquota(rqstp, fsqp, &gqa, &gqr);
	} else {
		gqr.gqr_status = Q_NOQUOTA;
		goto sendreply;
	}
	errno = 0;
 
sendreply:
	if (!svc_sendreply(transp, xdr_getquota_rslt, &gqr))
		log_cant_reply(transp);
}

struct fsquot *
findfsq(dir)
	char *dir;
{
	struct stat sb;
	register struct fsquot *fsqp;
	static time_t lastmtime = 0;

	if (lastmtime == 0 ||
	     stat(MOUNTED, &sb) < 0 || sb.st_mtime != lastmtime) {
		freefs();
		setupfs();
	}
	if (fsqlist == NULL)
		return (NULL);
	if (stat(dir, &sb) < 0)
		return (NULL);
	for (fsqp = fsqlist; fsqp != NULL; fsqp = fsqp->fsq_next) {
		if (sb.st_dev == fsqp->fsq_dev)
			return (fsqp);
	}
	return (NULL);
}

#define MNT_TO_FSTYPE(mntp)	((strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0) ?   \
				 FS_EFS :					\
				 ((strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) ?  \
				  FS_XFS : FS_NONE))

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

void
setupfs()
{
	register struct mntent 	*mntp;
	register struct fsquot 	*fsqp;
	FILE 			*mtab;
	struct stat 		sb;
	char 			qfilename[MAXPATHLEN];
	quota_fstype_t		fstype;

	mtab = setmntent(MOUNTED, "r");
	while (mntp = getmntent(mtab)) {
		fstype = MNT_TO_FSTYPE(mntp);
		/*	
		 * Only XFS and EFS are supported right now.
		 */
		if (fstype != FS_XFS && fstype != FS_EFS)
			continue;

		if (fstype == FS_EFS) {
			if (!hasmntopt(mntp, MNTOPT_QUOTA)) {
				sprintf(qfilename, "%s/%s", mntp->mnt_dir,
					QFNAME);
				if (stat(qfilename, &sb) < 0)
					continue;
			}
		} else {
			/*
			 * For XFS filesystems, use Q_GETQSTAT.
			 * Don't trust the mtab entries.
			 */
			if (getxfsqstat(mntp->mnt_fsname) <= 0)
				continue;
		}
		if (stat(mntp->mnt_fsname, &sb) < 0 ||
		    (sb.st_mode & S_IFMT) != S_IFBLK)
			continue;
		fsqp = (struct fsquot *)malloc(sizeof(struct fsquot));
		if (fsqp == NULL) {
			syslog(LOG_ERR, "out of memory");
			exit (1);
		}
		fsqp->fsq_next = fsqlist;
		fsqp->fsq_dir = malloc(strlen(mntp->mnt_dir) + 1);
		fsqp->fsq_devname = malloc(strlen(mntp->mnt_fsname) + 1);
		if (fsqp->fsq_dir == NULL || fsqp->fsq_devname == NULL) {
			syslog(LOG_ERR, "out of memory");
			exit (1);
		}
		strcpy(fsqp->fsq_dir, mntp->mnt_dir);
		strcpy(fsqp->fsq_devname, mntp->mnt_fsname);
		fsqp->fsq_dev = sb.st_rdev;
		fsqp->fsq_type = fstype;
		fsqlist = fsqp;
	}
	endmntent(mtab);
}

static void
freefs()
{
	register struct fsquot *fsqp;

	while (fsqp = fsqlist) {
		fsqlist = fsqp->fsq_next;
		free(fsqp->fsq_dir);
		free(fsqp->fsq_devname);
		free(fsqp);
	}
}

int
getdiskquota(fsqp, uid, dqp)
	struct fsquot *fsqp;
	int uid;
	struct dqblk *dqp;
{
	int fd;
	char qfilename[MAXPATHLEN];
	cap_t ocap;
	cap_value_t cap_quota_mgt = CAP_QUOTA_MGT;

	sprintf(qfilename, "%s/%s", fsqp->fsq_dir, QFNAME);
	if ((fd = open(qfilename, O_RDONLY)) < 0)
		return (0);
	close(fd);
	ocap = cap_acquire(1, &cap_quota_mgt);
	if (quotactl(Q_ACTIVATE, fsqp->fsq_devname, 0, qfilename) != 0) {
		cap_surrender(ocap);
		return(0);
	}
	if (quotactl(Q_GETQUOTA, fsqp->fsq_devname, uid, (caddr_t)dqp) != 0) {
		cap_surrender(ocap);
	        return(0);
	}
	cap_surrender(ocap);
	return (1);
}

void
log_cant_reply(transp)
	SVCXPRT *transp;
{
	int saverrno;
	struct sockaddr_in actual;
	register struct hostent *hp;
	register char *name;

	saverrno = errno;	/* save error code */
	actual = *svc_getcaller(transp);
	/*
	 * Don't use the unix credentials to get the machine name, instead use
	 * the source IP address. 
	 */
	if ((hp = gethostbyaddr(&actual.sin_addr, sizeof(actual.sin_addr),
	   AF_INET)) != NULL)
		name = hp->h_name;
	else
		name = inet_ntoa(actual.sin_addr);

	errno = saverrno;
	if (errno == 0)
		syslog(LOG_ERR, "couldn't send reply to %s", name);
	else
		syslog(LOG_ERR, "couldn't send reply to %s: %m", name);
}

void
closedown()
{
        if (dirty) {
                dirty = 0;
        }
        else {
                exit(0);
        }
        alarm(CLOSEDOWN);
}
