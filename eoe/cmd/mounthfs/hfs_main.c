/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.11 $"

/*
 * Mounthfs, a HFS filesystem server.
 *
 * Adapted from the mountdos filesystem server.
 *
 * TODO:
 *	We'd like to remount a "down" mount in order to set our options (ro)
 *	rather than use the last guy's.  This requires kernel support.
 */
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <mntent.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termio.h>
#include <unistd.h>
#include <fcntl.h>
#include <rpc/rpc.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>
#include <sys/mount.h>
#include <sys/smfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "hfs.h"

#include <sys/fs/nfs_clnt.h>

#define MNTOPT_TRACE	"trace"
#define	MNTOPT_DEBUG	"debug"
#define	MNTOPT_PART	"partition"

extern void	nfs_service();

mode_t		mode;		/* Mount point mode. */
uid_t		uid;		/* Mount point user ID */
gid_t		gid;		/* Mount point group ID */
int		debug = 0;	/* debug flag */

static char	*progname;
static FILE	*tracefile;
char		*rootdir;	/* shared between main and terminate */
static char	*fsname;
static char	*type;

enum mountstat { UNMOUNTED, MOUNTED_BUT_DOWN, MOUNTED_AND_UP };

static struct timeval	timeout={2,0}; /* Select timeout */
static int    dismountflag;

/* Forward definitions */

static char	*fullpath(char *);
static int	makedir(char *);
static enum mountstat	checkmount(char *, int *);

static void	selfmount(struct mntent *, fhandle_t *, u_short, pid_t, int,
			  enum mountstat);
static void	perrorf(char *, ...);
void		terminate(int);
static int	nopt(struct mntent *, char *opt);
static void	err_printf(char *format, ...);
static void	cleanup_gfstab();
void 		dismount(int);

/******
 * main
 */
main(int argc, char *argv[])
{
    hfs_t 		*hfs;
    int 		opt, sock, error;
    u_int 		flags, partition;
    enum mountstat 	mstat;
    fhandle_t		rootfh;
    SVCXPRT 		*transp;
    pid_t 		pid;
    fd_set 		readfds;
    struct mntent 	mnt;
    int                 needtimeout;

    
    /*
     * Initialize.
     */
    progname = argv[0];
    flags = 0;
    
    /*
     * Check for proper invocation via mount(1M).
     */
    if (argc != 5) {
	err_printf("usage: %s fsname dir type opts\n", progname);
	exit(2);
    }
    
    fsname = mnt.mnt_fsname = argv[1];
    rootdir = mnt.mnt_dir = fullpath(argv[2]);
    type = mnt.mnt_type = argv[3];
    mnt.mnt_opts = argv[4];
    
    if (strchr(mnt.mnt_fsname, ':')) {
	err_printf("%s: remote mount not supported\n", progname);
	exit(1);
    }
    
    if (hasmntopt(&mnt, MNTOPT_TRACE)){
      tracefile = fopen("/usr/tmp/mount_hfs.log","w");
      if (tracefile)
	setbuf(tracefile,NULL);
    }

    if (hasmntopt(&mnt, MNTOPT_DEBUG))
	debug = 1;
    if (hasmntopt(&mnt, MNTOPT_RO))
	flags |= FS_RDONLY;
    partition = nopt(&mnt, MNTOPT_PART);
    
    cleanup_gfstab();
    
    /*
     * Set up mount point and check whether it's already mounted and up.
     */
    sock = RPC_ANYSOCK;
    mstat = checkmount(mnt.mnt_fsname, &sock);
    if (mstat == MOUNTED_AND_UP) {
	err_printf("%s: %s is already mounted\n",
		   progname, rootdir);
	exit(1);
    }
    if (mstat == UNMOUNTED && makedir(rootdir) < 0) {
	perrorf("can't create %s", rootdir);
	exit(1);
    }
    
    /*
     * Set up floppy filesystem and root file handle.
     */
    error = fs_add(mnt.mnt_fsname, flags, partition, &hfs);
    if (error) {
	errno = error;
	if (error == ENOSPC)
	  err_printf("%s: %s was improperly dismounted\n",
		     progname, mnt.mnt_fsname);
	else if (error == ENODEV)
	    err_printf("%s: %s partial-volume device not supported\n",
		       progname, mnt.mnt_fsname);
	else
	  perrorf("can't initialize %s", mnt.mnt_fsname);
	exit(1);
    }

    makerootfh(&rootfh);

    /*
     * Create a service transport and register it.
     */
    pmap_set(NFS_PROGRAM, NFS_VERSION, IPPROTO_UDP);
    transp = svcudp_create(sock);
    if (transp == NULL) {
	err_printf("%s: cannot create udp service\n", progname);
	exit(1);
    }
    if (!svc_register(transp, NFS_PROGRAM, NFS_VERSION, nfs_service,
		      0)) {
	err_printf("%s: can't register myself as an NFS server\n",
		   progname);
	exit(1);
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
    sigset(SIGTERM, dismount);
    
    /*
     * Fork a child to run as an NFS server.  The parent mounts the child
     * server on rootdir, then exits.
     */
    pid = fork();
    if (pid < 0) {
	perrorf("can't fork");
	exit(1);
    }
    if (pid > 0) {
	selfmount(&mnt, &rootfh, transp->xp_port, pid,
		  flags & FS_RDONLY, mstat);
	exit(0);
    }
    
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
	    if (fd == transp->xp_sock || io_isfd(hfs,fd))
		continue;
	    if (tracefile && fd == fileno(tracefile))
		continue;
	    close(fd);
	}
	openlog(progname, LOG_PID, LOG_DAEMON);
    }
    
    /*
     * Serve incoming requests.
     */
    needtimeout = 1; /* XXX assume something might need to be sync'ed already */
    for (;;) {
        if (dismountflag)
	  break;
	readfds = svc_fdset;
	switch (select(transp->xp_sock+1, &readfds, NULL, NULL,
		needtimeout ? &timeout : NULL)) {
	  case -1:
	    needtimeout = 1; /* XXX we may need to sync now */
	    if (errno == EINTR)
		continue;
	    syslog(LOG_ERR, "select: %m");
	    terminate(1);
	  case 0:
	    fs_sync_all();
	    needtimeout = 0; /* we've sync'ed, simply block for more work */
	    break;
	  default:
	    svc_getreqset(&readfds);
	    needtimeout = 1; /* we may need to sync now */
	}
    }

    /*
     * Dismount everthing.
     */
    fs_remove_all();
    terminate(0);
}


/**********
 * fullpath
 */
static char * fullpath(char *pathname)
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


/*********
 * makedir
 */
static int makedir(char *path)
{
    struct stat	sb;
    char 	*slash;
    int 	result;
    
    if (stat(path, &sb) == 0) {
	if (S_ISDIR(sb.st_mode)){
	  gid = sb.st_gid;
	  uid = sb.st_uid;
	  mode= sb.st_mode;
	  return 0;
	}
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
    gid = getgid();
    uid = getuid();
    mode= 0777;
    return mkdir(path, 0777);
}



static char	mounted[] = "/etc/gfstab";

/************
 * checkmount
 */
static enum mountstat checkmount(char *dir, int *sock)
{
    FILE 		*mtab;
    enum mountstat 	mstat;
    long 		pos, nextpos;
    struct mntent 	*mnt;
    char 		*opt;
    pid_t 		pid;
    struct sockaddr_in 	sin;
    
    mstat = UNMOUNTED;
    mtab = setmntent(mounted, "r+");
    if (mtab == NULL)
	return mstat;
    for (pos = 0; (mnt = getmntent(mtab)) != NULL; pos = nextpos) {
	nextpos = ftell(mtab);
	if (strcmp(mnt->mnt_dir, dir) == 0) {
	    if ((opt = hasmntopt(mnt, "pid")) == NULL
		|| sscanf(opt+3, "=%hu", &pid) != 1
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

/***********
 * selfmount
 */
static void selfmount(struct mntent 	*mntp,
		      fhandle_t 	*fh,
		      u_short 		port,
		      pid_t 		pid,
		      int 		rdonly,
		      enum mountstat 	mstat)
{
    struct sockaddr_in 	sin;
    struct nfs_args 	args;
    char 		opts[MNTMAXSTR];
    int 		mflags, optlen, type;
    FILE 		*mtab;
    
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = htons(port);
    bzero(sin.sin_zero, sizeof sin.sin_zero);
    args.addr = &sin;
    args.fh = fh;
    args.flags = NFSMNT_HOSTNAME|NFSMNT_TIMEO|NFSMNT_LOOPBACK|
      NFSMNT_BASETYPE|NFSMNT_NAMEMAX|NFSMNT_PRIVATE;
    strcpy(args.base, FSID_MAC);
    args.namemax=32;
    args.timeo = 100;
    args.hostname = mntp->mnt_fsname;
    
    mflags = MS_FSS|MS_DATA;
    optlen = sprintf(opts, "%s,timeo=%d,port=%u,pid=%u",
		     mntp->mnt_opts, args.timeo, port, pid);
    if (rdonly)
	mflags |= MS_RDONLY;
    if (debug) {
	args.flags |= NFSMNT_SOFT;
	optlen += sprintf(&opts[optlen], ",%s", MNTOPT_SOFT);
    }
    
    type = sysfs(GETFSIND, FSID_NFS);
    if (type < 0) {
	err_printf("%s: NFS is not installed\n", progname);
	exit(1);
    }
    if (mount(mntp->mnt_fsname, mntp->mnt_dir, mflags, type, &args,
	      sizeof args) < 0
	&& !(mstat == MOUNTED_BUT_DOWN && errno == EBUSY)) {
	perrorf("can't mount myself");
	kill(pid, SIGKILL);
	exit(1);
    }
    
    mntp->mnt_opts = opts;
    mntp->mnt_freq = mntp->mnt_passno = 0;
    if (access(mounted, F_OK) < 0)
	close(creat(mounted, 0644));
    
    mtab = setmntent(mounted, "r+");
    if (mtab == NULL || addmntent(mtab, mntp) != 0) {
	perrorf(mounted);
	kill(pid, SIGKILL);
	terminate(1);
    }
    endmntent(mtab);
}


/*********
 * perrorf
 */
static void perrorf(char *format, ...)
{
    char 	buf[256];
    int 	offset;
    unsigned 	error;
    va_list 	ap;
    
    error = errno;
    offset = sprintf(buf, "%s: ", progname);
    
    va_start(ap, format);
    offset += vsprintf(&buf[offset], format, ap);
    va_end(ap);
    
    if (error < sys_nerr)
	offset += sprintf(&buf[offset], ": %s", sys_errlist[error]);
    buf[offset++] = '\n';
    buf[offset] = '\0';
    
    err_printf(buf);
}


/*******
 * trace
 */
void trace(char *format, ...)
{
    va_list ap;
    
    if (tracefile == NULL)
	return;
    va_start(ap, format);
    vfprintf(tracefile, format, ap);
    va_end(ap);
}


/*******
 * panic
 */
void panic(char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    vsyslog(LOG_ALERT, format, ap);
    va_end(ap);
    terminate(-1);
}


/***********
 * terminate
 */
void terminate(int status)
{
    FILE 		*mtab, *tmtab;
    int 		tmpfd;
    struct mntent 	*mnt;
    static char 	tmpname[] = "/etc/mounthfsXXXXXX";
    
    if (status < 0)
	abort();
    if ((mtab = setmntent(mounted, "r+")) == NULL
	|| (tmpfd = mkstemp(tmpname)) < 0
	|| (tmtab = fdopen(tmpfd, "w")) == NULL) {
	syslog(LOG_ERR, "can't open %s: %m", mtab ? tmpname : mounted);
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
	exit(1);
    }
    endmntent(mtab);
    exit(0);
}


/*************
 * safe_malloc
 */
void * safe_malloc(unsigned size)
{
    void *p;
    
    p = malloc(size);
    if (p == NULL) {
	syslog(LOG_ERR, "Out of memory");
	exit(1);
    }
    return p;
}

/******
 * nopt
 *=====
 * Return the value of a numeric option of the form foo=x, if
 * option is not found or is malformed, return 0.
 */
static int nopt(struct mntent *mnt, char *opt)
{
    int 	val = 0;
    char 	*equal;
    char 	*str;
    
    if (str = hasmntopt(mnt, opt)) {
	if (equal = strchr(str, '=')) {
	    val = atoi(&equal[1]);
	} else {
	    err_printf("%s: bad numeric option '%s'\n",
		       progname, str );
	    exit(1);
	}
    }
    return (val);
}


/************
 * err_printf
 */
static void err_printf(char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    if (isatty(2))
	vfprintf(stderr, format, ap);
    else
	vsyslog(LOG_ERR, format, ap);
    va_end(ap);
}

/****************
 * cleanup_gfstab
 */
static void cleanup_gfstab()
{
    FILE            	*gfstab;
    struct mntent   	*mnt;
    int			pid;
    char            	*tmp;
    char		*tmpname = "/etc/gfsXXXXXX";
    FILE		*tmptab;
    
    /* get rid of out-of-date gfstab entry */
    if ((gfstab = setmntent(mounted, "r+")) == NULL)
	return;
    
    if ((tmp = mktemp(tmpname)) == NULL 
	|| (tmptab = setmntent(tmp, "w")) == NULL) {
	perrorf("can't open temporary file");
	endmntent(gfstab);
	return;
    }
    
    while (mnt = getmntent(gfstab)) {
	pid = nopt(mnt, "pid");
	if (kill(pid, 0) < 0 && errno == ESRCH)
	    continue;
	addmntent(tmptab, mnt);
    }
    
    if (fchmod(fileno(tmptab), 0644) < 0)
	perrorf("can't change permission on %s", tmp);
    endmntent(gfstab);
    endmntent(tmptab);
    if (rename(tmp, mounted) < 0)
	perrorf("can't rename %s to %s", tmp, mounted);
}


/**********
 * dismount	Called in response to SIGTERM signal to request dismount.
 */
void dismount(int signo){
    (void) signal(signo, SIG_IGN);	/* ignore repeated signalling */
  /*
   * Just set the dismount flag.  If it's already set, terminate.
   */
  if (dismountflag++)
    terminate(1);
}







