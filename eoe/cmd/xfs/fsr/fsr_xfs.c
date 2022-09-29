#ident "$Id: fsr_xfs.c,v 1.5 1999/08/03 19:01:53 cwf Exp $"

/*
 * fsr - file system reorganizer
 *
 * This code was pulled over from the EFS version and modified 
 * for use with XFS.   For first release, this version will be
 * pretty basic but hopefully all of the following flags will
 * be functional
 *
 * fsr [-d] [-v] [-n] [-s] [-g] [-t mins] [-f leftf] [-m mtab]
 * fsr [-d] [-v] [-n] [-s] [-g] xfsdev | dir | file ...
 * 
 * If invoked in the first form fsr does the following: starting with the
 * dev/inum specified in /etc/fsrlast this reorgs each reg file in each file
 * system found in /etc/mtab.  After 2 hours of this we record the current
 * dev/inum in /etc/fsrlast.  If there is no /etc/fsrlast fsr starts at the
 * top of /etc/mtab.
 *
 *	-g		print to syslog (default if stdout not a tty)
 *	-m mtab		use something other than /etc/mtab
 *	-t time		how long to run
 *	-f leftoff	use this instead of /etc/fsrlast
 *
 *	-v		verbose. more -v's more verbose
 *	-d		debug. print even more
 *	-n		do nothing.  only interesting with -v.  Not
 *			effective with in mtab mode.
 *	-s		print statistics only.
 *	-p passes	Number of passes before terminating global re-org.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/handle.h>
#include <fcntl.h>

#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/fs/xfs_itable.h>
#include <sys/fs/xfs_dfrag.h>
#include <sys/fs/xfs_error.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <sys/param.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>

#include <diskinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <mntent.h>
#include <signal.h>
#include <getopt.h>
#include <limits.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/capability.h>

#include "types.h"
#include "jdm.h"

int vflag;
int gflag;
static int Mflag;
/* static int nflag; */
int dflag = 0;
/* static int sflag; */
int argv_blksz_dio;
extern int max_ext_size;
static int npasses = 10;
static int startpass = 0;

struct getbmap  *outmap = NULL;
int             outmap_size = 0;
int		RealUid;
int		tmp_agi;
static __int64_t	minimumfree = 2048;

#define SMBUFSZ		1024
#define ROOT		0
#define GRABSZ		64
#define TARGETRANGE	10
#define	V_NONE		0
#define	V_OVERVIEW	1
#define	V_ALL		2
#define BUFFER_SIZE	(1<<16)
#define BUFFER_MAX	(1<<24)
#define min(x, y) ((x) < (y) ? (x) : (y))

static time_t howlong = 7200;		/* default seconds of reorganizing */
static char *leftofffile = "/var/tmp/.fsrlast_xfs";/* where we left off last */
static char *mtab = MOUNTED;
static time_t endtime;
static time_t starttime;
static ino64_t	leftoffino = 0;
static int	pagesize;

void usage(int ret);
static int  fsrfile(char *fname, ino64_t ino);
static int  fsrfile_common( char *fname, char *tname, char *mnt, 
                            int fd, xfs_bstat_t *statp);
static int  packfile(char *fname, char *tname, int fd, 
                     xfs_bstat_t *statp, int flag);
static void fsrdir(char *dirname);
static int  fsrfs(char *mntdir, ino64_t ino, int targetrange);
static void initallfs(char *mtab);
static void fsrallfs(int howlong, char *leftofffile);
static void fsrall_cleanup(int timeout);
static int  getnextents(int);
int xfsrtextsize(int fd);
int xfs_getrt(int fd, struct statvfs64 *sfbp);
char * gettmpname(char *fname);
char * getparent(char *fname);
int fsrprintf(const char *fmt, ...);
int read_fd_bmap(int, xfs_bstat_t *, int *);
int cmp(const void *, const void *);
static void tmp_init(char *mnt);
static char * tmp_next(char *mnt);
static void tmp_close(char *mnt);
int xfs_getgeom(int , xfs_fsop_geom_t * );

xfs_fsop_geom_t fsgeom;	/* geometry of active mounted system */

#define NMOUNT 64
static int numfs;

typedef struct fsdesc {
	char *dev;
	char *mnt;
	int  npass;
} fsdesc_t;

fsdesc_t	*fs, *fsbase, *fsend;
int		fsbufsize = 10;	/* A starting value */

void
aborter(void)
{
	fsrall_cleanup(1);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct stat sb, sb2;
	char *argname;
	char *cp;
	int c;
	struct mntent mntpref;
	register struct mntent *mntp;
	struct mntent mntent;
	register FILE *mtabp;

	setlinebuf(stdout);

	gflag = ! isatty(0);

	while ((c = getopt(argc, argv, "p:e:MgsdnvTt:f:m:b:N:F")) != -1 )
		switch (c) {
		case 'M':
			Mflag = 1;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'n':
			/* nflag = 1; */
			break;
		case 'v':
			++vflag;
			break;
		case 'd':
			dflag = 1;
			break;
		case 's':		/* frag stats only */
			/* sflag = 1; */
			fprintf(stderr, 
				"Stats not yet supported for XFS\n");
			usage(1);
		case 't':
			howlong = atoi(optarg);
			break;
		case 'f':
			leftofffile = optarg;
			break;
		case 'm':
			mtab = optarg;
			break;
		case 'b':
			argv_blksz_dio = atoi(optarg);
			break;
		case 'p':
			npasses = atoi(optarg);
			break;
		default:
			usage(1);
		}
	if (vflag)
		setbuf(stdout, NULL);

	starttime = time(0);

	/* Save the caller's real uid */
	RealUid = getuid();
	
	pagesize = getpagesize();

	if (optind < argc) {
		for (; optind < argc; optind++) {
			argname = argv[optind];
			mntp = NULL;
			if (lstat(argname, &sb) < 0) {
				fprintf(stderr, "could not stat: %s: %s\n",
					argname, strerror(errno));
				continue;
			}
			if (S_ISLNK(sb.st_mode) && stat(argname, &sb2) == 0 &&
			    (S_ISBLK(sb2.st_mode) || S_ISCHR(sb2.st_mode)))
				sb = sb2;
			if (S_ISBLK(sb.st_mode) || (S_ISDIR(sb.st_mode))) {
				if ((mtabp = setmntent(mtab, "r")) == NULL) {
					fprintf(stderr, 
					"failed reading mtab: %s\n", mtab);
					exit(1);
				}
				bzero(&mntpref, sizeof(mntpref));
				if (S_ISDIR(sb.st_mode))
					mntpref.mnt_dir = argname;
				else
					mntpref.mnt_fsname = argname;

				if ((getmntany(mtabp, &mntent, &mntpref) == 0) 
				     &&
				   (strcmp(mntent.mnt_type, MNTTYPE_XFS) == 0))
				{
					mntp = &mntent;
					if (S_ISBLK(sb.st_mode)) {
						cp = mntp->mnt_dir;
						if (cp == NULL || 
						    stat(cp, &sb2) < 0) {
							fprintf(stderr, 
						"could not stat: %s: %s\n",
							argname, 
							strerror(errno));
							continue;
						}
						sb = sb2;
						argname = cp;
					}
				}
			}
			if (mntp != NULL) {
				fsrfs(mntp->mnt_dir, 0, 100);
			} else if (S_ISCHR(sb.st_mode)) {
				fprintf(stderr, 
					"char special not supported: %s\n",
				        argname);
				exit(1);
			} else if (S_ISDIR(sb.st_mode) || S_ISREG(sb.st_mode)) {
				if (strcmp(sb.st_fstype, FSID_XFS) != 0) { 
					fprintf(stderr, 
				        "cannot defragment: %s: Not XFS\n",
				        argname);
					continue;
				}
				if (S_ISDIR(sb.st_mode))
					fsrdir(argname);
				else
					fsrfile(argname, sb.st_ino);
			} else {
				printf(
			"%s: not fsys dev, dir, or reg file, ignoring\n",
					argname);
			}
		}
	} else {
		initallfs(mtab);
		fsrallfs(howlong, leftofffile);
	}
	return 0;
}

void
usage(int ret) 
{
	fprintf(stderr, "usage: fsr_xfs [xfsfile] ...\n");
	exit(ret);
}

/*
 * initallfs -- read the mount table and set up an internal form
 */
static void
initallfs(char *mtab)
{
	FILE *fp;
	struct mntent *mp;
	int mi;
	char *cp;
	struct stat sb;

	fp = setmntent(mtab, "r");
	if (fp == NULL) {
		fsrprintf("could not open mtab file: %s\n", mtab);
		exit(1);
	}

	/* malloc a number of descriptors, increased later if needed */
	if ((fsbase = (fsdesc_t *)malloc(fsbufsize * sizeof(fsdesc_t))) == NULL) {
		fsrprintf("out of memory: %s\n", strerror(errno));
		exit(1);
	}
	fsend = (fsbase + fsbufsize - 1);
	
	/* find all rw xfs file systems */
	mi = 0;
	fs = fsbase;
	while (mp = getmntent(fp)) {
		int rw = 0;

		if (strcmp(mp->mnt_type, FSID_XFS) != 0 ||
		    stat(mp->mnt_fsname, &sb) == -1 ||
		    !S_ISBLK(sb.st_mode))
			continue;

		cp = strtok(mp->mnt_opts,",");
		do {
			if (strcmp("rw", cp) == 0)
				rw++;
		} while ((cp = strtok(NULL, ",")) != NULL);
		if (rw == 0) {
			if (dflag)
				fsrprintf("Skipping %s: not mounted rw\n",
					mp->mnt_fsname);
			continue;
		}

		if (mi == fsbufsize) {
			fsbufsize += NMOUNT;
			if ((fsbase = (fsdesc_t *)realloc((char *)fsbase, 
			              fsbufsize * sizeof(fsdesc_t))) == NULL) {
				fsrprintf("out of memory: %s\n", strerror(errno));
				exit(1);
			}
			if (!fsbase) {
				fsrprintf("out of memory on realloc: %s\n", 
				          strerror(errno));
				exit(1);
			}
			fs = (fsbase + mi);  /* Needed ? */
		}

		fs->dev = strdup(mp->mnt_fsname);
		fs->mnt = strdup(mp->mnt_dir);

		if (fs->mnt == NULL || fs->mnt == NULL) {
			fsrprintf("strdup(%s) failed\n",mp->mnt_fsname);
			exit(1);
		}
		mi++;
		fs++;
	}
	numfs = mi;
	fsend = (fsbase + numfs);
	endmntent(fp);
	if (numfs == 0) {
		fsrprintf("no rw xfs file systems in mtab: %s\n", mtab);
		exit(0);
	}
	if (vflag || dflag) {
		fsrprintf("Found %d mounted, writable, XFS filesystems\n", 
		           numfs);
		if (dflag)
			for (fs = fsbase; fs < fsend; fs++)
			    fsrprintf("\t%-30.30s%-30.30s\n", fs->dev, fs->mnt);
	}
}

static void 
fsrallfs(int howlong, char *leftofffile)
{
	int fd;
	int error;
	int found = 0;
	char *fsname;
	char buf[SMBUFSZ];
	int mdonly = Mflag;
	char *ptr;
	ino64_t startino = 0;
	fsdesc_t *fsp;
	
	fsrprintf("fsr_xfs -m %s -t %d -f %s ...\n", mtab, howlong, leftofffile);

	endtime = starttime + howlong;
	fs = fsbase;

	/* where'd we leave off last time? */
	fd = open(leftofffile, O_RDONLY);
	if (fd == -1) {
		if (errno != ENOENT)
			fsrprintf(
			    "open(%s, O_RDONLY) failed: %s, starting with %s\n",
			    leftofffile, strerror(errno), *fs->dev);
	} else {
		if (read(fd, buf, SMBUFSZ) == -1) {
			fs = fsbase;
			fsrprintf("could not read %s, starting with %s\n",
				leftofffile, *fs->dev);
			
		} else {
			for (fs = fsbase; fs < fsend; fs++) {
				fsname = fs->dev;
				if ((strncmp(buf,fsname,strlen(fsname)) == 0)
				    && buf[strlen(fsname)] == ' ') {
					found = 1;
					break;
				}
			}
			if (! found)
				fs = fsbase;

			ptr = strchr(buf, ' ');
			if (ptr) {
				startpass = atoi(++ptr);
				ptr = strchr(ptr, ' ');
				if (ptr) {
					startino = strtoll(++ptr, 
					                   (char **)NULL, 
					                   10);
				}
			}

			/* Init pass counts */
			for (fsp = fsbase; fsp < fs; fsp++) {
				fsp->npass = startpass + 1;
			}
			for (fsp = fs; fsp <= fsend; fsp++) {
				fsp->npass = startpass;
			}
		}
		close(fd);
	}

	if (vflag) {
		fsrprintf("START: pass=%d ino=%lld %s %s\n", 
		           fs->npass, startino, fs->dev, fs->mnt);
	}

	sigset(SIGABRT, aborter);
	sigset(SIGHUP, aborter);
	sigset(SIGINT, aborter);
	sigset(SIGQUIT, aborter);
	sigset(SIGTERM, aborter);

	/* reorg for 'howlong' -- checked in 'fsrfs' */
	while (endtime > time(0)) {
		pid_t pid;
		if (fs == fsend)
			fs = fsbase;
		if (fs->npass == npasses) {
			fsrprintf("Completed all %d passes\n", npasses);
			break;
		}
		if (npasses > 1 && !fs->npass)
			Mflag = 1;
		else
			Mflag = mdonly;
		pid = fork();
		switch(pid) {
		case -1:
			fsrprintf("couldn't fork sub process:");
			exit(1);
			break;
		case 0:
			error = fsrfs(fs->mnt, startino, TARGETRANGE);
			exit (error);
			break;
		default:
			wait(&error);
			close(fd);
			if (WIFEXITED(error) && WEXITSTATUS(error) == 1) {
				/* child timed out & did fsrall_cleanup */
				exit(0);
			}
			break;
		}
		startino = 0;  /* reset after the first time through */
		fs->npass++;
		fs++;
	}
	fsrall_cleanup(endtime <= time(0));
}

/*
 * fsrall_cleanup -- close files, print next starting location, etc.
 */
static void
fsrall_cleanup(int timeout)
{
	int fd;
	int ret;
	char buf[SMBUFSZ];

	/* record where we left off */
	fd = open(leftofffile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		fsrprintf("open(%s) failed: %s\n", 
		          leftofffile, strerror(errno));
	else {
		if (timeout) {
			ret = sprintf(buf, "%s %d %lld\n", fs->dev, 
			        fs->npass, leftoffino);
			if (write(fd, buf, ret) < strlen(buf))
				fsrprintf("write(%s) failed: %s\n", leftofffile,
						strerror(errno));
			close(fd);
		}
	}

	if (timeout)
		fsrprintf("fsr_xfs startpass %d, endpass %d, time %d seconds\n",
			startpass, fs->npass, time(0) - endtime + howlong);
}

/*
 * fsrfs -- reorganize a file system
 */
static int
fsrfs(char *mntdir, ino64_t startino, int targetrange)
{

	int	fsfd, fd;
	int	count = 0;
	int	ret;
	size_t buflenout;
	xfs_bstat_t buf[GRABSZ];
	char	fname[64];
	char	*tname;
	jdm_fshandle_t	*fshandlep;
	ino64_t	lastino = startino;

	fsrprintf("%s startino=%lld\n", mntdir, startino);

	fshandlep = jdm_getfshandle( mntdir );
	if ( ! fshandlep ) {
		fsrprintf("unable to get handle: %s: %s\n", 
		          mntdir, strerror( errno ));
		return -1;
	}

	if ((fsfd = open(mntdir, O_RDONLY)) < 0) {
		fsrprintf("unable to open: %s: %s\n", 
		          mntdir, strerror( errno ));
		return -1;
	}

	if (xfs_getgeom(fsfd, &fsgeom) < 0 ) {
		fsrprintf("Skipping %s: could not get XFS geom\n",
			  mntdir);
		return -1;
	}

	tmp_init(mntdir);

	while (! syssgi( SGI_FS_BULKSTAT, fsfd, &lastino, 
	                 GRABSZ, &buf[0], &buflenout)) {
		xfs_bstat_t *p;
		xfs_bstat_t *endp;

		if (buflenout == 0 )
			goto out0;

		/* Each loop through, defrag targetrange percent of the files */
		count = (buflenout * targetrange) / 100;

		qsort((char *)buf, buflenout, sizeof(struct xfs_bstat), cmp);

		for ( p = buf, endp = (buf + buflenout); p < endp ; p++ ) {
			/* Do some obvious checks now */
			if (((p->bs_mode & S_IFMT) != S_IFREG) ||
			     (p->bs_extents < 2)) 
				continue;

			if((fd = jdm_open(fshandlep, p, O_RDWR)) < 0) {
				/* This probably means the file was
				 * removed while in progress of handling
				 * it.  Just quietly ignore this file.
				 */
				if (dflag)
					fsrprintf("could not open: ino %llu\n",
						  p->bs_ino);
				continue;
			}

			/* Don't know the pathname, so make up something */
			sprintf(fname, "ino=%lld", p->bs_ino);

			/* Get a tmp file name */
			tname = tmp_next(mntdir);

			ret = fsrfile_common(fname, tname, mntdir, fd, p);
			
			leftoffino = p->bs_ino;

			close(fd);

			if (ret == 0) {
				if (--count <= 0)
					break;
			}
		}
		if (endtime && endtime < time(0)) {
			tmp_close(mntdir);
			close(fsfd);
			fsrall_cleanup(1);
			exit(1);
		}
	}
out0:
	tmp_close(mntdir);
	close(fsfd);
	return 0;
}
		
/*
 * To compare bstat structs for qsort.
 */
cmp(const void *s1, const void *s2)
{
	return( ((xfs_bstat_t *)s2)->bs_extents -
	        ((xfs_bstat_t *)s1)->bs_extents);

}

/*
 * reorganize by directory hierarchy.
 * Stay in dev (a restriction based on structure of this program -- either
 * call efs_{n,u}mount() around each file, something smarter or this)
 */
static void
fsrdir(char *dirname)
{
	fsrprintf("%s: Directory defragmentation not supported\n", dirname);
}

/*
 * Sets up the defragmentation of a file based on the 
 * filepath.  It collects the bstat information, does 
 * an open on the file and passes this all to fsrfile_common.
 */
static int
fsrfile(char *fname, ino64_t ino)
{
	xfs_bstat_t	statbuf;
	jdm_fshandle_t	*fshandlep;
	int	fd, fsfd;
	int	error;
	char	*tname;

	fshandlep = jdm_getfshandle( fname );
	if (! fshandlep) {
		fsrprintf(
		  "unable to construct sys handle for %s: %s\n",
		  fname, strerror(errno));
		return -1;
	}

	/*
	 * Need to open something on the same filesystem as the
	 * file.  Open the parent.
	 */
	fsfd = open(getparent(fname), O_RDONLY);
	if (fsfd < 0) {
		fsrprintf(
		  "unable to open sys handle for %s: %s\n",
		  fname, strerror(errno));
		return -1;
	}
	
	if ((syssgi( SGI_FS_BULKSTAT_SINGLE, fsfd, &ino, &statbuf)) < 0) {
		fsrprintf(
		  "unable to get bstat on %s: %s\n",
		  fname, strerror(errno));
		close(fsfd);
		return -1;
	}
		
	fd = jdm_open( fshandlep, &statbuf, O_RDWR);
	if (fd < 0) {
		fsrprintf(
		  "unable to open handle %s: %s\n",
		  fname, strerror(errno));
		close(fsfd);
		return -1;
	}

	/* Get the fs geometry */
	if (xfs_getgeom(fsfd, &fsgeom) < 0 ) {
		fsrprintf("Unable to get geom on fs for: %s\n", fname);
		close(fsfd);
		return -1;
	}

	close(fsfd);

	tname = gettmpname(fname);

	if (tname)
		error = fsrfile_common(fname, tname, NULL, fd, &statbuf);

	close(fd);

	return error;
}


/*
 * This is the common defrag code for either a full fs
 * defragmentation or a single file.  Check as much as
 * possible with the file, fork a process to setuid to the 
 * target file owner's uid and defragment the file.
 * This is done so the new extents created in a tmp file are 
 * reflected in the owners' quota without having to do any 
 * special code in the kernel.  When the existing extents 
 * are removed, the quotas will be correct.  It's ugly but 
 * it saves us from doing some quota  re-construction in 
 * the extent swap.  The price is that the defragmentation
 * will fail if the owner of the target file is already at
 * their quota limit.
 */
static int
fsrfile_common(
	char		*fname, 
	char		*tname, 
	char		*fsname,
	int		fd,
	xfs_bstat_t	*statp)
{
	int	pid;
	int	error;
	cap_t	ocap;
	cap_value_t cap_setuid = CAP_SETUID;
	struct statvfs64 vfss;
	struct fsxattr  fsx;
	int	do_rt = 0;
	ulong_t bsize;
		
	if (vflag)
		fsrprintf("%s\n", fname);

	if (fsync(fd) < 0) {
		fsrprintf("sync failed: %s: %s\n", fname, strerror(errno));
		return -1;
	}

	if (statp->bs_size == 0) {
		if (vflag)
			fsrprintf("%s: zero size, ignoring\n", fname);
		return(0);
	}

	/* Check if a mandatory lock is set on the file to try and 
	 * avoid blocking indefinitely on the reads later. Note that
	 * someone could still set a mandatory lock after this check 
	 * but before all reads have completed to block fsr reads.
	 * This change just closes the window a bit.
	 */
	if ( (statp->bs_mode & S_ISGID) && ( ! (statp->bs_mode&S_IXGRP) ) ) {
		struct flock fl;

		fl.l_type = F_RDLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = (off_t)0;
		fl.l_len = 0;
		if ((fcntl(fd, F_GETLK, &fl)) < 0 ) {
			if (vflag)
				fsrprintf("locking check failed: %s\n", fname);
			return(-1);
		}
		if (fl.l_type != F_UNLCK) {
			/* Mandatory lock is set */
			if (vflag)
				fsrprintf("mandatory lock: %s: ignoring\n",
					  fname);
			return(-1);
		}
	}

	/* Check if there is room to copy the file */
	if ( statvfs64( (fsname == NULL ? fname : fsname), &vfss) < 0) {
		fsrprintf(
		  "unable to get fs stat on %s: %s\n",
		  fname, strerror(errno));
		return (-1);
	}
	bsize = vfss.f_frsize ? vfss.f_frsize : vfss.f_bsize;

	if (statp->bs_size > ((vfss.f_bfree * bsize) - minimumfree)) {
		fsrprintf(
		  "insufficient freespace for: %s: size=%d: ignoring\n",
		  fname, statp->bs_size);
		return 1;
	}

	/* Check realtime info */
	if ((fcntl(fd, F_FSGETXATTR, &fsx)) < 0) {
		fsrprintf("failed to get attrs: %s\n", fname);
		return(-1);
	}
	if (fsx.fsx_xflags == XFS_XFLAG_REALTIME) {
		if (xfs_getrt(fd, &vfss) < 0) {
			fsrprintf("could not get rt geom for: %s\n", fname);
			return(-1);
		}
		if (statp->bs_size > ((vfss.f_bfree * bsize) - minimumfree)) {
			fsrprintf("low on rt free space: %s: ignoring file\n",
			          fname);
			return(-1);
		}
		do_rt = 1;
	}

	if ((RealUid != ROOT) && (RealUid != statp->bs_uid)) {
		fsrprintf("cannot open: %s: Permission denied\n", fname);
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		fsrprintf("fork failed: %s\n", strerror(errno));
		close(fd);
		exit(1);
	} else if (pid == 0) {
		/* 
		 * Already checked ownership so we're either
		 * root or our realuid owns the file and can safely 
		 * setuid to the file owner.  This is done so
		 * quotas are maintained for the user.  (No group
		 * quotas so no setgid).
		 */
		ocap = cap_acquire(1, &cap_setuid);
		if (setuid(statp->bs_uid) < 0) {
			fsrprintf("setuid failed: uid=%d %s: %s\n", 
				  statp->bs_uid, fname, strerror(errno));
			cap_surrender(ocap);
			exit(1);
		}
		cap_surrender(ocap);

		error = packfile(fname, tname, fd, statp, do_rt);
		exit(error);
	} else if (pid > 0) {
		wait(&error);
		close(fd);
		if (WIFEXITED(error))
			return(WEXITSTATUS(error));
		return -1;
	}
	/* NOTREACHED */
}


/*
 * Do the defragmentation of a single file.
 * We already are pretty sure we can and want to 
 * defragment the file.  Create the tmp file, copy
 * the data (maintaining holes) and call the kernel
 * extent swap routinte.
 */
static int
packfile(char *fname, char *tname, int fd, xfs_bstat_t *statp, int do_rt)
{
	int 		tfd;
	int		srval;
	int		nextents, extent, cur_nextents, new_nextents;
	unsigned	blksz_dio;
	unsigned	dio_min;
	struct dioattr	dio;
	static xfs_swapext_t   sx;
	flock64_t 	space;
	off64_t 	cnt, pos;
	void 		*fbuf;
	int 		ct, wc, wc_b4;
	struct fsxattr  tfsx;

	/*
	 * Work out the extent map - nextents will be set to the
	 * minimum number of extents needed for the file (taking 
	 * into account holes), cur_nextents is the current number
	 * of extents.
	 */
	nextents = read_fd_bmap(fd, statp, &cur_nextents);

	if ( cur_nextents == 1 || cur_nextents <= nextents ) {
		if (vflag)
			fsrprintf("%s already fully defragmented.\n", fname);
		return 1; /* indicates no change/no error */
	}

	if (dflag)
		fsrprintf("%s extents=%d can_save=%d tmp=%s\n", 
		          fname, cur_nextents, (cur_nextents - nextents), 
		          tname);

	if ((tfd = open(tname, O_CREAT|O_EXCL|O_RDWR, 0666)) < 0) {
		if (vflag)
			fsrprintf("could not open tmp as uid %d: %s: %s\n",
				   statp->bs_uid,tname, strerror(errno));
		return -1;
	}
	unlink(tname);

	/* Setup direct I/O */
	if((fcntl(tfd, F_SETFL, FDIRECT)) < 0 ) {
		fsrprintf("could not set FDIRECT on tmp: %s:\n", tname);
		close(tfd);
		return -1;
	}
	if( (fcntl(tfd, F_DIOINFO, &dio)) < 0 ) {
		fsrprintf("could not get I/O info on tmp: %s\n", tname);
		close(tfd);
		return -1;
	}
	if (do_rt) {
		int rt_textsize = fsgeom.rtextsize * fsgeom.blocksize;

		tfsx.fsx_xflags = XFS_XFLAG_REALTIME;

		if ((tfsx.fsx_extsize = rt_textsize) <= 0 ) {
			fsrprintf("realtime geom not avail for tmp: %s\n", fname);
			close(tfd);
			return -1;
		}

		if (fcntl( tfd, F_FSSETXATTR, &tfsx) < 0) {
			fsrprintf("could not set rt on tmp: %s\n", tname);
			close(tfd);
			return -1;
		}
	}

	dio_min   = dio.d_miniosz;
	if (statp->bs_size <= dio_min)
		blksz_dio = dio_min;
	else {
		blksz_dio = min(dio.d_maxiosz, BUFFER_MAX - pagesize);
		if (argv_blksz_dio != 0)
			blksz_dio = min(argv_blksz_dio, blksz_dio);
		blksz_dio = (min(statp->bs_size, blksz_dio) / dio_min) * dio_min;
	}

	if (dflag) {
	    fsrprintf("DEBUG: fsize=%d blsz_dio=%d d_min=%d d_max=%d pgsz=%d\n",
	    statp->bs_size, blksz_dio, dio.d_miniosz, dio.d_maxiosz, pagesize);
	}

	/* Malloc a buffer */
	fbuf = (char *)memalign(dio.d_mem, blksz_dio);

	/* Loop through block map copying the file. */
	for (extent = 0; extent < nextents; extent++) {
		pos = outmap[extent].bmv_offset;
		if (outmap[extent].bmv_block == -1) {
			space.l_whence = 0;
			space.l_start = pos;
			space.l_len = outmap[extent].bmv_length;
			if (fcntl(tfd, F_UNRESVSP64, &space) < 0) {
				fsrprintf("could not trunc tmp %s\n", 
					   tname);
			}
			lseek64(tfd, outmap[extent].bmv_length, SEEK_CUR);
			lseek64(fd, outmap[extent].bmv_length, SEEK_CUR);
			continue;
		} else if (outmap[extent].bmv_length == 0) {
			/* to catch holes at the beginning of the file */
			continue;
		}
		/* do prealloc */
		space.l_whence = SEEK_CUR;
		space.l_start = 0;
		space.l_len = outmap[extent].bmv_length;

		if (cnt > 0) {
			if (fcntl(tfd, F_RESVSP64, &space) < 0) {
				fsrprintf("could not pre-alloc tmp space: %s\n",
					  tname);
				close(tfd);
				return -1;
			}
		}
		for (cnt = outmap[extent].bmv_length; cnt > 0;
		     cnt -= ct, pos += ct) {
			ct = blksz_dio;
			ct = read(fd, fbuf, ct);
			if (ct == 0) {
				/* EOF, stop trying to read */
				extent = nextents;
				break;
			}
			/* Ensure we do direct I/O to correct block
			 * boundaries.
			 */
			if (ct != blksz_dio) {
				wc = ct + dio_min - (ct % dio_min);
			} else {
				wc = ct;
			}
			wc_b4 = wc;
			if (ct < 0 || ((wc = write(tfd, fbuf, wc)) != wc_b4)) {
				if (ct < 0)
				fsrprintf("bad read of %d bytes from %s:%s\n",
				           wc_b4, fname, strerror(errno));
				else if (wc < 0)
				fsrprintf("bad write of %d bytes to %s: %s\n",
					   wc_b4, tname, strerror(errno));
				else {
					/*
					 * Might be out of space
					 *
					 * Try to finish write
					 */
					int resid = ct-wc;

					if ((wc = write(tfd, ((char *)fbuf)+wc,
							resid)) == resid) {
						/* worked on second attempt? */
						continue;
					}
					else
					if (wc < 0) {
				fsrprintf("bad write2 of %d bytes to %s: %s\n",
				           resid, tname, strerror(errno));
					} else {
						fsrprintf("bad copy to %s\n",
					           tname);
					}
				}
				free(fbuf);
				return -1;
			}
		}
	}
	ftruncate64(tfd, statp->bs_size);

	free(fbuf);

	sx.sx_stat     = *statp; /* struct copy */
	sx.sx_version  = XFS_SX_VERSION;
	sx.sx_fdtarget = fd;
	sx.sx_fdtmp    = tfd;
	sx.sx_offset   = 0;
	sx.sx_length   = statp->bs_size;

	/* Check if the extent count improved */
	new_nextents = getnextents(tfd);
	if (cur_nextents <= new_nextents) {
		if (vflag)
			fsrprintf("No improvement made: %s\n", fname);
		close(tfd);
		return 1; /* no change/no error */
	}

	/* Swap the extents */
	srval = syssgi(SGI_FS_SWAPEXT, &sx);
	if (srval < 0) {
		if (errno == ENOTSUP) {
			if (vflag || dflag) 
			   fsrprintf("%s: file type not supported\n", fname);
		} else if (errno == EFAULT) {
			/* The file has changed since we started the copy */
			if (vflag || dflag)
			   fsrprintf("%s:file modified defrag aborted\n", 
				     fname);
		} else if (errno == EBUSY) {
			/* Timestamp has changed or mmap'ed file */
			if (vflag || dflag)
			   fsrprintf("%s: file busy \n", fname);
		} else {
			fsrprintf("SGI_FS_SWAPEXT failed: %s: %s\n", 
				  fname, strerror(errno));
		}
		close(tfd);
		return -1;
	}

	/* Report progress */
	if (vflag)
		fsrprintf("extents before:%d after:%d %s %s\n",
			  cur_nextents, new_nextents, 
			  (new_nextents <= nextents ? "DONE" : "    " ),
		          fname);
	close(tfd);
	return 0;
}

char *
gettmpname(char *fname)
{
	static char	buf[PATH_MAX+1];
	char		sbuf[SMBUFSZ];
	char		*ptr;

	sprintf(sbuf, "/.fsr%d", getpid());

	strcpy(buf, fname);
	ptr = strrchr(buf, '/');
	if (ptr) {
		*ptr = '\0';
	} else {
		strcpy(buf, ".");
	}

	if ((strlen(buf) + strlen (sbuf)) > PATH_MAX) {
		fsrprintf("tmp file name too long: %s\n", fname);
		return(NULL);
	}

	strcat(buf, sbuf);

	return(buf);
}

char *
getparent(char *fname)
{
	static char	buf[PATH_MAX+1];
	char		*ptr;

	strcpy(buf, fname);
	ptr = strrchr(buf, '/');
	if (ptr) {
		if (ptr == &buf[0])
			++ptr;
		*ptr = '\0';
	} else {
		strcpy(buf, ".");
	}

	return(buf);
}

/*
 * Read in block map of the input file, coalesce contiguous
 * extents into a single range, keep all holes. Convert from 512 byte
 * blocks to bytes.
 *
 * This code was borrowed from mv.c with some minor mods.
 */
#define MAPSIZE	128
#define	OUTMAP_SIZE_INCREMENT	MAPSIZE

int	read_fd_bmap(int fd, xfs_bstat_t *sin, int *cur_nextents)
{
	int		i, cnt;
	struct getbmap	map[MAPSIZE];

#define	BUMP_CNT	\
	if (++cnt >= outmap_size) { \
		outmap_size += OUTMAP_SIZE_INCREMENT; \
		outmap = (struct getbmap *)realloc(outmap, \
		                           outmap_size*sizeof(*outmap)); \
		if (outmap == NULL) { \
			fsrprintf("realloc failed: %s\n", \
				strerror(errno)); \
			exit(1); \
		} \
	}

	/*	Initialize the outmap array.  It always grows - never shrinks.
	 *	Left-over memory allocation is saved for the next files.
	 */
	if (outmap_size == 0) {
		outmap_size = OUTMAP_SIZE_INCREMENT; /* Initial size */
		outmap = (struct getbmap *)malloc(outmap_size*sizeof(*outmap));
		if (!outmap) {
			fsrprintf("malloc failed: %s\n",
				strerror(errno));
			exit(1);
		}
	}

	outmap[0].bmv_block = 0;
	outmap[0].bmv_offset = 0;
	outmap[0].bmv_length = sin->bs_size;

	/*
	 * If a non regular file is involved then forget holes
	 */

	if (!S_ISREG(sin->bs_mode))
		return(1);

	outmap[0].bmv_length = 0;

	map[0].bmv_offset = 0;
	map[0].bmv_block = 0;
	map[0].bmv_entries = 0;
	map[0].bmv_count = MAPSIZE;
	map[0].bmv_length = -1;

	cnt = 0;
	*cur_nextents = 0;

	do {
		if (fcntl(fd, F_GETBMAP, map) < 0) {
			fsrprintf("failed reading extents: inode %lld",
			         sin->bs_ino);
			exit(1);
		}

		/* Concatenate extents together and replicate holes into
		 * the output map.
		 */
		*cur_nextents += map[0].bmv_entries;
		for (i = 0; i < map[0].bmv_entries; i++) {
			if (map[i + 1].bmv_block == -1) {
				BUMP_CNT;
				outmap[cnt] = map[i+1];
			} else if (outmap[cnt].bmv_block == -1) {
				BUMP_CNT;
				outmap[cnt] = map[i+1];
			} else {
				outmap[cnt].bmv_length += map[i + 1].bmv_length;
			}
		}
	} while (map[0].bmv_entries == (MAPSIZE-1));
	for (i = 0; i <= cnt; i++) {
		outmap[i].bmv_offset = BBTOB(outmap[i].bmv_offset);
		outmap[i].bmv_length = BBTOB(outmap[i].bmv_length);
	}

	outmap[cnt].bmv_length = sin->bs_size - outmap[cnt].bmv_offset;

	return(cnt+1);
}

/*
 * Read the block map and return the number of extents.
 */
int 
getnextents(int fd)
{
	int		nextents;
	struct getbmap	map[MAPSIZE];

	map[0].bmv_offset = 0;
	map[0].bmv_block = 0;
	map[0].bmv_entries = 0;
	map[0].bmv_count = MAPSIZE;
	map[0].bmv_length = -1;

	nextents = 0;

	do {
		if (fcntl(fd, F_GETBMAP, map) < 0) {
			fsrprintf("failed reading extents");
			exit(1);
		}

		nextents += map[0].bmv_entries;
	} while (map[0].bmv_entries == (MAPSIZE-1));

	return(nextents);
}

/*
 * Get the fs geometry
 */
int
xfs_getgeom(int fd, xfs_fsop_geom_t * fsgeom)
{
	if (syssgi(SGI_XFS_FSOPERATIONS, 
	           fd, 
	           XFS_FS_GEOMETRY, 
	           (void *)0, 
	           fsgeom) < 0) {
		return -1;
	}
	return 0;
}

/*
 * Get xfs realtime space information 
 */
int
xfs_getrt(int fd, struct statvfs64 *sfbp)
{
	ulong_t bsize;
	ulong_t factor;
	xfs_fsop_counts_t cnt;

	if (!fsgeom.rtblocks)
		return -1;

	if (syssgi(SGI_XFS_FSOPERATIONS, 
	           fd, 
	           XFS_FS_COUNTS, 
	           (void *)0, 
	           &cnt) < 0) {
		close(fd);
		return -1;
	}
	bsize = (sfbp->f_frsize ? sfbp->f_frsize : sfbp->f_bsize);
	factor = fsgeom.blocksize / bsize;         /* currently this is == 1 */
	sfbp->f_bfree = (cnt.freertx * fsgeom.rtextsize) * factor;
	return 0;
}

int
fsrprintf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (gflag) {
		static int didopenlog;
		if (!didopenlog) {
			openlog("fsr", LOG_PID, LOG_USER);
			didopenlog = 1;
		}
		vsyslog(LOG_INFO, fmt, ap);
	} else
		vprintf(fmt, ap);
	va_end(ap);
	return 0;
}

/*
 * Initialize a directory for tmp file use.  This is used
 * by the full filesystem defragmentation when we're walking
 * the inodes and do not know the path for the individual
 * files.  Multiple directories are used to spread out the
 * tmp data around to different ag's (since file data is
 * usually allocated to the same ag as the directory and
 * directories allocated round robin from the same
 * parent directory).
 */
static void
tmp_init(char *mnt)
{
	int 	i;
	static char	buf[SMBUFSZ];
	mode_t	mask;

	tmp_agi = 0;
	sprintf(buf, "%s/.fsr", mnt);

	mask = umask(0);
	if (mkdir(buf, 0777) < 0) {
		if (errno == EEXIST) {
			if (dflag)
				fsrprintf("tmpdir already exists: %s\n", buf);
		} else {
			fsrprintf("could not create tmpdir: %s: %s\n", 
			       buf, strerror(errno));
			exit(-1);
		}
	}
	for (i=0; i < fsgeom.agcount; i++) {
		sprintf(buf, "%s/.fsr/ag%d", mnt, i);
		if (mkdir(buf, 0777) < 0) {
			if (errno == EEXIST) {
				if (dflag)
				   fsrprintf("tmpdir already exists: %s\n",buf);
			} else {
				fsrprintf("could not create tmpdir: %s: %s\n", 
				       buf, strerror(errno));
				exit(-1);
			}
		}
	}
	(void)umask(mask);
	return;
}

static char *
tmp_next(char *mnt)
{
	static char	buf[SMBUFSZ];

	sprintf(buf, "%s/.fsr/ag%d/tmp%d", 
	        ( (strcmp(mnt, "/") == 0) ? "" : mnt), 
	        tmp_agi, 
	        getpid());

	if (++tmp_agi == fsgeom.agcount)
		tmp_agi = 0;

	return(buf);
}

static void
tmp_close(char *mnt)
{
	static char	buf[SMBUFSZ];
	int i;

	/* No data is ever actually written so we can just do rmdir's */
	for (i=0; i < fsgeom.agcount; i++) {
		sprintf(buf, "%s/.fsr/ag%d", mnt, i);
		if (rmdir(buf) < 0) {
			if (errno != ENOENT) {
			    fsrprintf("could not remove tmpdir: %s: %s\n",
			              buf, strerror(errno));
			}
		}
	}
	sprintf(buf, "%s/.fsr", mnt);
	if (rmdir(buf) < 0) {
		if (errno != ENOENT) {
			fsrprintf("could not remove tmpdir: %s: %s\n",
			          buf, strerror(errno));
		}
	}
}
