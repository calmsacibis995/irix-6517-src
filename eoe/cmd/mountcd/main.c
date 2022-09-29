/*
 * Mountiso9660, an ISO 9660 filesystem server.
 *
 * Cannibalized from mountdos
 *
 *    History:
 *      rogerc      12/18/90    Commenced cannibalization
 *
 * TODO:
 *    We'd like to remount a "down" mount in order to set our options (ro)
 *    rather than use the last guy's.  This requires kernel support.
 */

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <stdio.h>
#include <mntent.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <termio.h>
#include <unistd.h>
#include <fcntl.h>
#include <rpc/rpc.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <rpc/types.h>
#include "iso.h"
#include <sys/fs/nfs_clnt.h>
#include "main.h"
#include "remote.h"
#include "testcd_prot.h"

#define MNTOPT_DEBUG		"debug"
#define MNTOPT_CDDEBUG		"cddebug"
#define MNTOPT_CACHEDEBUG	"cadebug" /* can't have "cache" in it */
#define MNTOPT_DSDEBUG		"dsdebug"
#define MNTOPT_ISODEBUG		"isodebug"
#define MNTOPT_CACHE		"cache"
#define MNTOPT_NOTRANSLATE	"notranslate"
#define MNTOPT_SETX		"setx"
#define MNTOPT_NOEXT		"noext"
	/*
	 * for ABI compliance; "nmconv" is only needed when invoked as
	 * mount_cdfs, but take for all cases; the MIPS ABI Black Book
	 * 1.2 specifies system defaults vary, and defines 3 options:
	 * =c: no conversion (==notranslate), =l: convert to lowercase
	 * (the default for us), and =m: don't show version number
	 * (the default for us).
	 */
#define MNTOPT_NMCONV		"nmconv"
#define MNTOPT_SUSP		"susp"		/* ABI noop */
#define MNTOPT_NOSUSP		"nosusp"	/* ABI synonym for noext */
#define MNTOPT_RRIP		"rrip"		/* ABI noop */
#define MNTOPT_NORRIP		"norrip"	/* ABI synonym for next */
#define MNTOPT_TESTCD		"testcd"	/* If set, register the */
						/* testcd  protocol */

extern void    nfs_service();

uid_t       uid;        /* real user ID */
gid_t       gid;        /* real group ID */
int         debug = 0;        /* debug flag */
char        *progname;

static FILE    *tracefile;
static char    *rootdir;    /* shared between main and terminate */

#ifdef	DEBUG
static int      print_cache_statistics = 1;
#else	/* ! DEBUG */
static int      print_cache_statistics = 0;
#endif	/* ! DEBUG */

enum mountstat { UNMOUNTED, MOUNTED_BUT_DOWN, MOUNTED_AND_UP };

void            usage();
static char     *fullpath(char *pathname);
static int      makedir(char *);
static enum mountstat  checkmount(char *, int *);
static char            *type, *fsname;

static void selfmount(struct mntent *, fhandle_t *, u_short, pid_t,
		      enum mountstat);
static void perrorf(char *, ...);
void terminate(int status);
static int
nopt(struct mntent *mnt, char *opt);
static char *
str_opt(struct mntent *mnt, char *opt);

/*
 *  main(int argc, char *argv[])
 *
 *  Description:
 *      Mount an iso9660 file system, and act as an NFS server for that
 *      system.
 *
 *      This program is intended to only be used by mount(1M).  When
 *      mount encounters a file system type other than efs, nfs, or debug,
 *      it executes mount_%s, %s being replaced by the file system type.
 *      mount_iso9660, therefore, is invoked by mount(1M) when it
 *      encounters a file system type of iso9660.
 *
 *  Usage:
 *      mount_iso9660 fsname dir type opts
 *          fsname      Must be /dev/rdsk/dks*vol
 *          dir         The mount point
 *          type        Should be "iso9660"
 *          opts        Supported options (in addition to standard
 *                      mount(1M) options:
 *                          debug - Program is not detatched from
 *                              the terminal
 *                          cache - Numeric option; specify the number
 *                              of blocks to be cached by the server
 */

main(int argc, char *argv[])
{
    int opt, sock, error;
    enum mountstat mstat;
    fhandle_t rootfh;
    SVCXPRT *transp;
    pid_t pid;
    fd_set readfds;
    extern char *optarg;
    extern int      opterr, optind, optopt;
    int cache_blocks = 256;
    char *pc, *cdfsopt=NULL;
    struct mntent mnt;

    /*
     * Initialize.
     */
    uid = getuid();
    gid = getgid();
    progname = argv[0];

    /*
     * Check for proper invocation via mount(1M).
     */
    if (argc != 5) {
	fprintf(stderr, "usage: %s fsname dir type opts\n", progname);
	exit(2);
    }

    fsname = mnt.mnt_fsname = argv[1];
    mnt.mnt_dir		    = argv[2];
    type = mnt.mnt_type	    = argv[3];
    mnt.mnt_opts	    = argv[4];

    rootdir = mnt.mnt_dir = fullpath(mnt.mnt_dir);

    /*
     *  Do the NFS mount if file system is on another machine
     */
    /*
     * We should get rid of this eventually, but this will allow
     * us to mount CDs from machines with old OS releases
     */
    pc = strchr(mnt.mnt_fsname, ':');
    if (pc) {
	remote_mount(&mnt);
	exit (0);
    }

    /*
     * Check mount options for debug and cache size
     */
    if (hasmntopt(&mnt, MNTOPT_DEBUG)) {
	debug = 1;
    }

    if (hasmntopt(&mnt, MNTOPT_CDDEBUG)) {
	extern int cddebug;

	cddebug = 1;
    }

    if (hasmntopt(&mnt, MNTOPT_CACHEDEBUG)) {
	extern int cachedebug;

	cachedebug = 1;
    }

    if (hasmntopt(&mnt, MNTOPT_DSDEBUG)) {
	extern int dsdebug;

	dsdebug = 3;
    }

    if (hasmntopt(&mnt, MNTOPT_ISODEBUG)) {
	extern int isodebug;

	isodebug = 1;
    }

    if (hasmntopt(&mnt, MNTOPT_CACHE)) {
	cache_blocks = nopt(&mnt, MNTOPT_CACHE);
    }

    if (hasmntopt(&mnt, MNTOPT_NOTRANSLATE)) {
	iso_disable_name_translations('c');
    }

    if (hasmntopt(&mnt, MNTOPT_SETX)) {
	iso_setx();
    }

    if (hasmntopt(&mnt, MNTOPT_NOEXT)
	|| hasmntopt(&mnt, MNTOPT_NOSUSP)
	|| hasmntopt(&mnt, MNTOPT_NORRIP)) {
	iso_disable_extensions();
    }

    if (cdfsopt = str_opt(&mnt, MNTOPT_NMCONV)) {
	if (*cdfsopt && !cdfsopt[1] && index("clm", *cdfsopt)) {

		iso_disable_name_translations(*cdfsopt);

	} else {
	    (void) fprintf(stderr, "%s: bad string option '%s=%s'\n",
			   progname, MNTOPT_NMCONV, cdfsopt );
	}
    }

    /*
     * Set up mount point and check whether it's already mounted and up.
     */
    sock = RPC_ANYSOCK;
    mstat = checkmount(mnt.mnt_fsname, &sock);

    if (mstat == MOUNTED_AND_UP) {
	fprintf(stderr, "%s: %s is already mounted.\n",
		progname, rootdir);
	exit(1);
    }

    if (mstat == UNMOUNTED && makedir(rootdir) < 0) {
	perrorf("can't create %s", rootdir);
	exit(1);
    }

    /*
     * Set up iso 9660 filesystem and root file handle.
     */
    iso_init(cache_blocks);

    error = iso_openfs(mnt.mnt_fsname, rootdir, &rootfh);

    if (error) {
	errno = error;
	perrorf("can't initialize %s", mnt.mnt_fsname);
	exit(1);
    }

    /*
     * Create a service transport and register it.
     */
    pmap_set(NFS_PROGRAM, NFS_VERSION, IPPROTO_UDP);

    transp = svcudp_create(sock);

    if (transp == NULL) {
	fprintf(stderr, "%s: cannot create udp service.\n", progname);
	exit(1);
    }

    if (!svc_register(transp, NFS_PROGRAM, NFS_VERSION, nfs_service, 0)) {

	fprintf(stderr, "%s: can't register myself as an NFS server.\n",
								progname);
	exit(1);
    }

    pmap_set(SGI_NFS_PROGRAM, SGI_NFS_VERSION, IPPROTO_UDP);

    if (!svc_register(transp, SGI_NFS_PROGRAM, SGI_NFS_VERSION,
							nfs_service, 0)) {
	fprintf(stderr,
		"%s: can't register myself as an SGI NFS server.\n", progname);
    }

    /*
     * If the testcd mount option is set, we register an
     * additional protocol to allow the test program to access the
     * data on the CD.
     */
    if (hasmntopt(&mnt, MNTOPT_TESTCD)) {

	pmap_set(TESTCD_PROGRAM, TESTCD_VERSION, IPPROTO_UDP);

	if (!svc_register(transp, TESTCD_PROGRAM, TESTCD_VERSION,
							nfs_service, 0)) {
	    fprintf(stderr,
		    "%s: can't register myself as a TEST server.\n",
							    progname);
	}
    }

    opt = 50000;

    setsockopt(transp->xp_sock, SOL_SOCKET, SO_RCVBUF, &opt, sizeof opt);
    setsockopt(transp->xp_sock, SOL_SOCKET, SO_SNDBUF, &opt, sizeof opt);

    svcudp_enablecache(transp, 64);

    /*
     * Setup fatal signal handlers before mounting.
     */
    sigset(SIGHUP, terminate);
    sigset(SIGINT, terminate);
    sigset(SIGTERM, terminate);

    /*
     * Fork a child to run as an NFS server.  The parent mounts the child
     * server on rootdir, then exits.
     */
#ifdef PROFILE
    pid = fork();
    if (pid < 0) {
	perrorf("can't fork");
	exit(1);
    }
    if (pid == 0) {
	selfmount(&mnt, &rootfh, transp->xp_port, getppid(), mstat);
	exit(0);
    }

    pid = getpid();
#else
    pid = fork();
    if (pid < 0) {
	perrorf("can't fork");
	exit(1);
    }
    if (pid > 0) {
	selfmount(&mnt, &rootfh, transp->xp_port, pid, mstat);
	exit(0);
    }
#endif

    /*
     * Put the child in the background unless debugging.
     */
    if (debug) {
	openlog(progname, LOG_PID|LOG_PERROR, LOG_DAEMON);
    } else {
	int fd = open("/dev/tty", 0);

	if (fd >= 0)
	    ioctl(fd, TIOCNOTTY);

	for (fd = getdtablesize(); --fd >= 0; ) {

	    if (fd == transp->xp_sock || iso_isfd(fd))
		continue;

	    if (tracefile && fd == fileno(tracefile))
		continue;

	    close(fd);
	}

	openlog(progname, LOG_PID, LOG_DAEMON);

	(void)setsid();

	(void)chdir("/");
    }

    /*
     * Serve incoming requests.
     */
    for (;;) {

	readfds = svc_fdset;

	switch (select(transp->xp_sock+1, &readfds, NULL, NULL, NULL)) {
	case -1:
	    if (errno == EINTR)
		continue;

	    syslog(LOG_ERR, "select: %m");

	    terminate(1);

	case 0:
	    break;

	default:
	    svc_getreqset(&readfds);
	}
    }
    /* NOTREACHED */
}

static char *
fullpath(char *pathname)
{
    static char buf[PATH_MAX];

    if (pathname[0] != '/') {
	if (getwd(buf) == NULL) {
	    perrorf(buf);
	    exit(1);
	}
	sprintf(buf + strlen(buf), "/%s", pathname);
	pathname = buf;
    }
    return pathname;
}

static int
makedir(char *path)
{
    struct stat sb;
    char *slash;
    int result;

    if (stat(path, &sb) == 0) {
	if (S_ISDIR(sb.st_mode))
	    return 0;
	errno = ENOTDIR;
	return -1;
    }
    if (errno != ENOENT)
	return -1;
    slash = strrchr(path, '/');
    if (slash && slash != path) {
	*slash = '\0';
	result = makedir(path);
	*slash = '/';
	if (result < 0)
	    return result;
    }
    return mkdir(path, 0777);
}

static char    mounted[] = "/etc/gfstab";
static char    lockfile[] = "/etc/.gfslock";

static enum mountstat
checkmount(char *dir, int *sock)
{
    FILE *mtab;
    enum mountstat mstat;
    long pos, nextpos;
    struct mntent *mnt;
    char *opt;
    pid_t pid;
    struct sockaddr_in sin;

    mstat = UNMOUNTED;
    mtab = setmntent(mounted, "r+");
    if (mtab == NULL)
	return mstat;
    for (pos = 0; (mnt = getmntent(mtab)) != NULL; pos = nextpos) {
	nextpos = ftell(mtab);
	if (strcmp(mnt->mnt_dir, dir) == 0) {
	    if ((opt = hasmntopt(mnt, "pid")) == NULL
		|| sscanf(opt+3, "=%u", &pid) != 1
		|| (opt = hasmntopt(mnt, "port")) == NULL
		|| sscanf(opt+4, "=%hu", &sin.sin_port) != 1) {
		fseek(mtab, pos, 0);
		putc('#', mtab);
		fseek(mtab, nextpos, 0);
		continue;
	    }

	    sin.sin_family = AF_INET;
	    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	    sin.sin_port = htons(sin.sin_port);
	    bzero(sin.sin_zero, sizeof sin.sin_zero);
	    *sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	    if (*sock < 0) {
		perrorf("can't create socket");
		exit(1);
	    }

	    if (bind(*sock, &sin, sizeof sin) == 0
		|| kill(pid, 0) < 0 && errno == ESRCH) {
		fseek(mtab, pos, 0);
		putc('#', mtab);
		mstat = MOUNTED_BUT_DOWN;
	    } else {
		if (errno != EADDRINUSE) {
		    perrorf("can't bind UDP port %u",
			    ntohs(sin.sin_port));
		    exit(1);
		}
		mstat = MOUNTED_AND_UP;
	    }
	    break;
	}
    }
    endmntent(mtab);
    return mstat;
}

static void
selfmount(struct mntent *mntp, fhandle_t *fh, u_short port, pid_t pid,
	  enum mountstat mstat)
{
    struct sockaddr_in sin;
    struct nfs_args args;
    char opts[MNTMAXSTR];
    int mflags, optlen, type;
    FILE *mtab;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = htons(port);

    bzero(sin.sin_zero, sizeof sin.sin_zero);

    args.addr = &sin;
    args.fh = fh;

#ifdef FSID_ISO9660
    args.flags = NFSMNT_PRIVATE | NFSMNT_HOSTNAME | NFSMNT_TIMEO | NFSMNT_LOOPBACK
		| NFSMNT_BASETYPE;
    strcpy(args.base, FSID_ISO9660);
#else
    args.flags = NFSMNT_PRIVATE | NFSMNT_HOSTNAME | NFSMNT_TIMEO | NFSMNT_LOOPBACK;
#endif
    args.timeo = 100;
    args.hostname = mntp->mnt_fsname;

    mflags = MS_FSS|MS_DATA|MS_RDONLY;

    optlen = sprintf(opts, "%s,timeo=%d,port=%u,pid=%u,%s",
		     mntp->mnt_opts, (int)args.timeo, port, pid, MNTOPT_RO);

    /*
     * Make mounts soft by default; it's too easy to eject a disc
     */
    if (!hasmntopt(mntp, MNTOPT_HARD)) {
	args.flags |= NFSMNT_SOFT;
	optlen += sprintf(&opts[optlen], ",%s", MNTOPT_SOFT);
    }
    if (hasmntopt(mntp, MNTOPT_NOSUID)) {
	mflags |= MS_NOSUID;
    }

    type = sysfs(GETFSIND, FSID_NFS);

    if (type < 0) {
	fprintf(stderr, "%s: NFS is not installed.\n", progname);
	exit(1);
    }

    if (     (mount(mntp->mnt_fsname, mntp->mnt_dir, mflags, type,
						&args, sizeof args) < 0)
	&& ! (mstat == MOUNTED_BUT_DOWN && errno == EBUSY) ) {
	perrorf("can't mount myself");

	kill(pid, SIGKILL);

	exit(1);
    }

    mntp->mnt_opts = opts;
    mntp->mnt_freq = mntp->mnt_passno = 0;

    if (access(mounted, F_OK) < 0) {
	int fd;

	fd = creat(mounted, 0644);
	close(fd);
    }

    mtab = setmntent(mounted, "r+");

    if (mtab == NULL || addmntent(mtab, mntp) != 0) {
	perrorf(mounted);
	kill(pid, SIGKILL);
	terminate(1);
    }

    endmntent(mtab);
}

static void
perrorf(char *format, ...)
{
    unsigned error;
    va_list ap;

    error = errno;
    fprintf(stderr, "%s: ", progname);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    if (error < sys_nerr)
	fprintf(stderr, ": %s", sys_errlist[error]);
    fputs(".\n", stderr);
}

void
trace(char *format, ...)
{
    va_list ap;

    if (tracefile == NULL)
	return;
    va_start(ap, format);
    vfprintf(tracefile, format, ap);
    va_end(ap);
}

void
panic(char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsyslog(LOG_ALERT, format, ap);
    va_end(ap);
    terminate(-1);
}

extern int  cache_hits;
extern int  cache_misses;

void
terminate(int status)
{
    FILE *mtab, *tmtab;
    int tmpfd;
    struct mntent *mnt;
    static char tmpname[] = "/etc/mountcdXXXXXX";
    int lockfd;

    if (status < 0)
	abort();

    if (print_cache_statistics)
	fprintf(stderr, "%s: cache hits = %d, cache misses = %d.\n",
	       progname, cache_hits, cache_misses);

    /*
     * This cleans up wrt devscsi
     */
    iso_removefs();

    /*
     * This lock file eliminates race conditions that can occur
     * with multiple CD-ROM drives or with floppy drives (mountdos
     * also use /etc/gfstab).  If two processes execute the code
     * below at the same time, /etc/gfstab can get hosed.
     */
    lockfd = open(lockfile, O_RDWR | O_CREAT, 0644);

    if (lockfd != -1) {
	lockf(lockfd, F_LOCK, 0);
    }

    /*
     * Remove our gfstab entry
     */
    if ((mtab = setmntent(mounted, "r+")) == NULL
	|| (tmpfd = mkstemp(tmpname)) < 0
	|| (tmtab = fdopen(tmpfd, "w")) == NULL) {
	syslog(LOG_ERR, "can't open %s: %m", mtab ? tmpname : mounted);
	endmntent(mtab);
	if (lockfd != -1) {
	    lockf(lockfd, F_ULOCK, 0);
	}
	exit(1);
    }
    while ((mnt = getmntent(mtab)) != NULL) {
	if (strcmp(mnt->mnt_dir, rootdir) == 0
	    && strcmp(mnt->mnt_fsname, fsname) == 0
	    && strcmp(mnt->mnt_type, type) == 0)
	    continue;
	fprintf(tmtab, "%s %s %s %s %d %d\n",
		mnt->mnt_fsname, mnt->mnt_dir,
		mnt->mnt_type, mnt->mnt_opts,
		mnt->mnt_freq, mnt->mnt_passno);
    }
    if (fchmod(tmpfd, 0644) < 0 || fclose(tmtab) == EOF
	|| rename(tmpname, mounted) < 0) {
	syslog(LOG_ERR, "can't update %s: %m", mounted);
	(void) unlink(tmpname);
	endmntent(mtab);
	if (lockfd != -1) {
	    lockf(lockfd, F_ULOCK, 0);
	}
	exit(1);
    }

    endmntent(mtab);
    if (lockfd != -1) {
	lockf(lockfd, F_ULOCK, 0);
    }
    exit(0);
}

/*
 * Return the value of a numeric option of the form foo=x, if
 * option is not found or is malformed, return 0.
 */
static int
nopt(struct mntent *mnt, char *opt)
{
    int val = 0;
    char *equal;
    char *str;

    if (str = hasmntopt(mnt, opt)) {
	if (equal = index(str, '=')) {
	    val = atoi(&equal[1]);
	} else {
	    (void) fprintf(stderr, "%s: bad numeric option '%s'\n",
			   progname, str );
	}
    }
    return (val);
}

/*
 * Return the value of a string option of the form foo=x, if
 * option is not found or is malformed, return NULL.
 */
static char *
str_opt(struct mntent *mnt, char *opt)
{
    char *equal, *index();
    char *str;

    if (str = hasmntopt(mnt, opt)) {
	if((equal = index(str, '=')) && equal[1]) {
	    return &equal[1];
	} else {
	    (void) fprintf(stderr, "%s: bad string option '%s'\n",
			   progname, str );
	}
    }
    return NULL;
}
