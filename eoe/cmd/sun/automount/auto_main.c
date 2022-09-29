#ifndef lint
static char sccsid[] = 	"@(#)auto_main.c	1.6 90/07/24 4.1NFSSRC Copyr 1990 Sun Micro";
#endif

/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "nfs_prot.h"
#include <netinet/in.h>
#include <mntent.h>
#include <netdb.h>
#include <sys/schedctl.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#define NFSCLIENT
typedef nfs_fh fhandle_t;
#include <sys/mount.h>
#include <sys/fs/nfs_clnt.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>

#include "automount.h"

extern errno;

void catch();
void reap();
void set_timeout();
void domntent();
void loadmaster_file();
void loadmaster_yp();

#define	MAXDIRS	10

#define	MASTER_MAPNAME	"auto.master"

#define	MAX_REPLY_TIME	10	/* at most 10 seconds from reply to kernel */

int maxwait = 60;
int mount_timeout = 30;
int max_link_time = 5*60;
int ping_cache_time = 5;
int verbose, syntaxok;
dev_t tmpdev;
int yp;

u_short myport;
short	nfstype;

main(argc, argv)
	int argc;
	char *argv[];
{
	SVCXPRT *transp;
	extern void nfs_program_2();
	extern void read_mtab();
	static struct sockaddr_in sin;	/* static causes zero init */
	struct nfs_args args;
	struct autodir *dir, *dir_next;
	int pid;
	int bad, nomntpts;
	int ndp = NDPNORMMAX; 		/* non-degrading priority */
	int master_yp = 1;
	char *master_file;
	char *myname = "automount";
	struct hostent *hp;
	struct stat stbuf;
	extern int trace;
	char pidbuf[64];

	if (geteuid() != 0) {
		fprintf(stderr, "Must be root to use automount\n");
		exit(1);
	}

	argc--;
	argv++;

	/* 
	 * Error messages go to both stderr and syslog until the fork().
	 */
	openlog(myname, LOG_PID | LOG_NOWAIT | LOG_PERROR, LOG_DAEMON);

	(void) gethostname(self, sizeof self);
	hp = gethostbyname(self);
	if (hp == NULL) {
		syslog(LOG_ERR, "Can't get my address");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&my_addr, hp->h_length);
	(void) getdomainname(mydomain, sizeof mydomain);
	(void) strcpy(tmpdir, "/tmp_mnt");
	master_file = NULL;
	syntaxok = 1;	/* OK to log map syntax errs to console */

	while (argc && argv[0][0] == '-') switch (argv[0][1]) {
	case 'n':
		nomounts++;
		argc--;
		argv++;
		break;
	case 'm':
		master_yp = 0;
		argc--;
		argv++;
		break;
	case 'f':
		master_file = argv[1];
		argc -= 2;
		argv += 2;
		break;
	case 'M':
		if (stat(argv[1], &stbuf) == 0 
		    && (stbuf.st_mode & S_IFMT) != S_IFDIR) {
			syslog(LOG_ERR, "Bad mountpoint %s", argv[1]);
			usage();
		}
		(void) strcpy(tmpdir, argv[1]);
		argc -= 2;
		argv += 2;
		break;
	case 't':			/* timeouts */
		if (argc < 2) {
			syslog(LOG_ERR, "Bad timeout value");
			usage();
		}
		if (argv[0][2]) {
			set_timeout(argv[0][2], atoi(argv[1]));
		} else {
			char *s;

			for (s = strtok(argv[1], ","); s ;
				s = strtok(NULL, ",")) {
				if (*(s+1) != '=') {
					syslog(LOG_ERR, "Bad timeout value");
					usage();
				}
				set_timeout(*s, atoi(s+2));
			}
		}
		argc -= 2;
		argv += 2;
		break;

	case 'T':
		trace++;
		argc--;
		argv++;
		break;

	case 'D':
		if (argv[0][2])
			(void) putenv(&argv[0][2]);
		else {
			(void) putenv(argv[1]);
			argc--;
			argv++;
		}
		argc--;
		argv++;
		break;

	case 'p':
		ndp = atoi(argv[1]);
		argc -= 2;
		argv += 2;
		break;

	case 'v':
		verbose++;
		argc--;
		argv++;
		break;

	default:
		syslog(LOG_ERR, "unknown option");
		usage();
	}

	if (verbose && argc == 0 && master_yp == 0 && master_file == NULL) {
		syslog(LOG_ERR, "no mount maps specified");
		usage();
	}

	yp = 1;
	if (yp_bind(mydomain)) {
		if (verbose)
			syslog(LOG_ERR, "NIS bind failed: %s",
				yperr_string(bad));
		yp = 0;
	}

	read_mtab();
	/*
	 * Get mountpoints and maps off the command line
	 */
	while (argc >= 2) {
		if (argc >= 3 && argv[2][0] == '-') {
			dirinit(argv[0], argv[1], argv[2]+1, 0);
			argc -= 3;
			argv += 3;
		} else {
			dirinit(argv[0], argv[1], "rw", 0);
			argc -= 2;
			argv += 2;
		}
	}
	if (argc)
		usage();
	
	if (master_file) {
		loadmaster_file(master_file);
	}
	if (master_yp) {
		loadmaster_yp(MASTER_MAPNAME);
	}

	/*
	 * Remove -null map entries
	 */
	for (dir = HEAD(struct autodir, dir_q); dir; dir = dir_next) {
	    	dir_next = NEXT(struct autodir, dir);
		if (strcmp(dir->dir_map, "-null") == 0) {
			REMQUE(dir_q, dir);
		}
	}
	if (HEAD(struct autodir, dir_q) == NULL) {   /* any maps ? */
		syslog(LOG_ERR, "No mountpoints to serve");
		exit(1);
	}

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		syslog(LOG_ERR, "Cannot create UDP service");
		exit(1);
	}
	if (!svc_register(transp, NFS_PROGRAM, NFS_VERSION, nfs_program_2, 0)) {
		syslog(LOG_ERR, "svc_register failed");
		exit(1);
	}
	if (mkdir_r(tmpdir) < 0) {
		syslog(LOG_ERR, "couldn't create %s: %m", tmpdir);
		exit(1);
	}
	if (stat(tmpdir, &stbuf) < 0) {
		syslog(LOG_ERR, "couldn't stat %s: %m", tmpdir);
		exit(1);
	}
	tmpdev = stbuf.st_dev;
	nfstype = sysfs(GETFSIND, FSID_NFS);

	switch (pid = fork()) {
	case -1:
		syslog(LOG_ERR, "Cannot fork: %m");
		exit(1);
	case 0:
		/* child */
		closelog();
		{ int fd = open("/dev/tty", O_RDWR);
		  if (fd > 0)
			(void) ioctl(fd, TIOCNOTTY, (char *)0);
		  for (fd = getdtablehi(); --fd >= 0;) {
			if (fd == transp->xp_sock || trace && fd == 2)
				continue;
			(void) close(fd);
		  }
		}
		/* reopen the log so msgs just go to syslog, if possible */
		openlog(myname, LOG_PID | LOG_NOWAIT | LOG_CONS, LOG_DAEMON);

		signal(SIGTERM, catch);
		signal(SIGHUP, read_mtab);
		signal(SIGCHLD, reap);
		if (schedctl(NDPRI, 0, ndp) < 0)
			syslog(LOG_ERR, "Cannot set non-degrading priority %m");
		auto_run();
		syslog(LOG_ERR, "svc_run returned");
		exit(1);
	}

	/* parent */
	sin.sin_family = AF_INET;
	sin.sin_port = htons(transp->xp_port);
	myport = transp->xp_port;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	args.addr = &sin;
	args.flags = NFSMNT_TIMEO + NFSMNT_HOSTNAME + NFSMNT_RETRANS;
	args.timeo = (mount_timeout + 5) * 10;
	args.retrans = 5;
	if (max_link_time > MAX_REPLY_TIME) {
		args.flags += NFSMNT_SYMTTL;
		args.symttl = max_link_time - MAX_REPLY_TIME;
	} else if (max_link_time >= 0) {
		args.flags += NFSMNT_SYMTTL;
		args.symttl = max_link_time;
	}
	bad = 0;
	nomntpts = 1;

	/*
	 * Mount the daemon at its mount points.
	 * Start at the end of the list because that's
	 * the first on the command line.
	 */
	for (dir = TAIL(struct autodir, dir_q); dir;
	    dir = PREV(struct autodir, dir)) {
		(void) sprintf(pidbuf, "(pid%d@%s)", pid, dir->dir_name);
		if (strlen(pidbuf) >= HOSTNAMESZ-1)
			(void) strcpy(&pidbuf[HOSTNAMESZ-3], "..");
		args.hostname = pidbuf;
		args.fh = (fhandle_t *)&dir->dir_vnode.vn_fh; 
		if (dir->dir_vnode.vn_type == VN_LINK)
			args.flags |= NFSMNT_NOAC;
		else
			args.flags &= ~NFSMNT_NOAC;
		if (mount(dir->dir_name, dir->dir_name, MS_RDONLY|MS_DATA,
			  nfstype, &args, sizeof args)) {
			syslog(LOG_ERR, "Can't mount %s: %m", dir->dir_name);
			bad++;
		} else {
			domntent(pid, dir);
			nomntpts = 0;
		}
	}
	/*
	 * Don't bail because of a failed mount; only if we are left
	 * with no mountpoints to serve.
	 */
	if (nomntpts) {
		syslog(LOG_ERR, "Can't service any mountpoints");
		(void) kill(pid, SIGKILL);
	}
	bad += nomntpts;
	exit(bad);
	/*NOTREACHED*/
}

void
set_timeout(letter, t)
	char letter ; int t;
{
	if (t <= 1 && letter != 'p') {
		syslog(LOG_ERR, "bad timeout value");
		usage();
	}
	switch (letter) {
	case 'm':
		mount_timeout = t;
		break;
	case 'l':
		max_link_time = t;
		if (max_link_time < 0) {
			syslog(LOG_ERR,
			       "Illegal value for -tl option: %d",
			       max_link_time);
			usage();
		}
		break;
	case 'p':
		ping_cache_time = t;
		break;
	case 'w':
		maxwait = t;
		break;
	default:
		syslog(LOG_ERR, "bad timeout switch -t%c", letter);
		usage();
	}
}

void
domntent(pid, dir)
	int pid;
	struct autodir *dir;
{
	FILE *f;
	struct mntent mnt;
	struct stat st;
	char fsname[64];
	char mntopts[100];
	char buf[16];

	f = setmntent(MOUNTED, "a");
	if (f == NULL) {
		syslog(LOG_ERR, "Can't update %s", MOUNTED);
		return;
	}
	(void) sprintf(fsname, "%s:(pid%d)", self, pid);
	mnt.mnt_fsname = fsname;
	mnt.mnt_dir = dir->dir_name;
	mnt.mnt_type = MNTTYPE_IGNORE;
	(void) sprintf(mntopts, "ro,intr,port=%d,map=%s,%s", myport,
		dir->dir_map, 
		dir->dir_vnode.vn_type == VN_LINK ? "direct" : "indirect");
	if (dir->dir_vnode.vn_type == VN_DIR) {
		if (stat(dir->dir_name, &st) == 0) {
			(void) sprintf(buf, ",%s=%04x",
				MNTINFO_DEV, st.st_dev);
			(void) strcat(mntopts, buf);
		}
	}
	mnt.mnt_opts = mntopts;
	mnt.mnt_freq = 0;
	mnt.mnt_passno = 0;	
	(void) addmntent(f, &mnt);	
	(void) endmntent(f);
}

void
catch()
{
	struct autodir *dir;
	int child;
	struct filsys *fs, *fs_next;
	struct stat stbuf;
	struct fattr *fa;

	/*
	 *  The automounter has received a SIGTERM.
	 *  Here it forks a child to carry on servicing
	 *  its mount points in order that those
	 *  mount points can be unmounted.  The child
	 *  checks for symlink mount points and changes them
	 *  into directories to prevent the unmount system
	 *  call from following them.
	 */
	if ((child = fork()) == 0) {
		for (dir = HEAD(struct autodir, dir_q); dir;
		    dir = NEXT(struct autodir, dir)) {
			if (dir->dir_vnode.vn_type != VN_LINK)
				continue;

			dir->dir_vnode.vn_type = VN_DIR;
			fa = &dir->dir_vnode.vn_fattr;
			fa->type = NFDIR;
			fa->mode = NFSMODE_DIR + 0555;
		}
		return;
	}

	for (dir = HEAD(struct autodir, dir_q); dir;
	    dir = NEXT(struct autodir, dir)) {

		/*  This lstat is a kludge to force the kernel
		 *  to flush the attr cache.  If it was a direct
		 *  mount point (symlink) the kernel needs to
		 *  do a getattr to find out that it has changed
		 *  back into a directory.
		 */
		if (lstat(dir->dir_name, &stbuf) < 0) {
			syslog(LOG_ERR, "lstat %s: %m", dir->dir_name);
		}

		if (umount(dir->dir_name) < 0) {
			if (errno != EBUSY)
				syslog(LOG_ERR, "umount %s: %m", dir->dir_name);
		} else {
			clean_mtab(dir->dir_name, 0);
			if (dir->dir_remove)
				(void) rmdir(dir->dir_name);
		}
	}
	(void) kill (child, SIGKILL);

	/*
	 *  Unmount any mounts done by the automounter
	 */
	for (fs = HEAD(struct filsys, fs_q); fs; fs = fs_next) {
		fs_next = NEXT(struct filsys, fs);
		if (fs->fs_mine && fs == fs->fs_rootfs) {
			if (do_unmount(fs))
				fs_next = HEAD(struct filsys, fs_q);
		}
	}

	syslog(LOG_ERR, "exiting");
	exit(0);
}

void
reap()
{
	while (wait3((int *)0, WNOHANG, (struct rusage *)0) > 0)
		;
}

auto_run()
{
	fd_set read_fdset;
	int n;
	long last;
	struct timeval tv;

	last = time((long *)0);
	tv.tv_sec = maxwait;
	tv.tv_usec = 0;
	for (;;) {
		read_fdset = svc_fdset;
		n = select(32, &read_fdset, (fd_set *)0, (fd_set *)0, &tv);
		time_now = time((long *)0);
		if (n) /* hand request off to nfs_program_2 */
			svc_getreqset(&read_fdset);
		/* We may fall into here right after servicing a request */
		if (time_now >= last + maxwait) {
			if (trace > 2 && n) {
				fprintf(stderr,
					"timenow: %d  last: %d  maxwait: %d\n",
					time_now, last, maxwait);
			}

			last = time_now;
			do_timeouts();
		}
	}
}

usage()
{
	fprintf(stderr, "Usage: automount\n%s%s%s%s%s%s%s%s%s%s%s",
		"\t[-n]\t\t(no mounts)\n",
		"\t[-m]\t\t(ignore NIS auto.master)\n",
		"\t[-T]\t\t(trace nfs requests)\n",
		"\t[-v]\t\t(verbose error msgs)\n",
		"\t[-p n]\t\t(process priority)\n",
		"\t[-tl n]\t\t(mount duration)\n",
		"\t[-tm n]\t\t(attempt interval)\n",
		"\t[-tp n]\t\t(ping status duration)\n",
		"\t[-tw n]\t\t(unmount interval)\n",
		"\t[-M dir]\t(temporary mount dir)\n",
		"\t[-D n=s]\t(define env variable)\n",
		"\t[-f file]\t(get mntpnts from file)\n",
		"\t[ dir map [-mntopts] ] ...\n");
	exit(1);
}

void
loadmaster_yp(mapname)
	char *mapname;
{
	int first, err;
	char *key, *nkey, *val;
	int kl, nkl, vl;
	char dir[100], map[100];
	char *p, *opts;


	if (!yp)
		return;

	first = 1;
	key  = NULL; kl  = 0;
	nkey = NULL; nkl = 0;
	val  = NULL; vl  = 0;

	for (;;) {
		if (first) {
			first = 0;
			err = yp_first(mydomain, mapname, &nkey, &nkl, &val, &vl);
		} else {
			err = yp_next(mydomain, mapname, key, kl, &nkey, &nkl,
				&val, &vl);
		}
		if (err) {
			if (err != YPERR_NOMORE && err != YPERR_MAP)
				syslog(LOG_ERR, "%s: %s",
					mapname, yperr_string(err));
			return;
		}
		if (key)
			free(key);
		key = nkey;
		kl = nkl;

		if (kl >= 100 || vl >= 100)
			return;
		if (kl < 2 || vl < 1)
			return;
		if (isspace(*(u_char *)key) || *key == '#')
			return;
		(void) strncpy(dir, key, kl);
		dir[kl] = '\0';
		(void) strncpy(map, val, vl);
		map[vl] = '\0';
		p = map;
		while (*p && !isspace(*(u_char *)p))
			p++;
		opts = "rw";
		if (*p) {
			*p++ = '\0';
			while (*p && isspace(*(u_char *)p))
				p++;
			if (*p == '-')
				opts = p+1;
		}

		dirinit(dir, map, opts, 0);

		free(val);
	}
}

void
loadmaster_file(masterfile)
	char *masterfile;
{
	FILE *fp;
	char *line, *dir, *map, *opts;
	extern char *get_line();
	char linebuf[1024];

	if ((fp = fopen(masterfile, "r")) == NULL) {
		syslog(LOG_ERR, "%s:%m", masterfile);
		return;
	}

	while ((line = get_line(fp, linebuf, sizeof linebuf)) != NULL) {
		dir = line;
		while (*dir && isspace(*(u_char *)dir)) dir++;
		if (*dir == '\0')
			continue;
		map = dir;
		while (*map && !isspace(*(u_char *)map)) map++;
		if (*map)
			*map++ = '\0';
		if (*dir == '+') {
			loadmaster_yp(dir+1);
		} else {
			while (*map && isspace(*(u_char *)map)) map++;
			if (*map == '\0')
				continue;
			opts = map;
			while (*opts && !isspace(*(u_char *)opts)) opts++;
			if (*opts) {
				*opts++ = '\0';
				while (*opts && isspace(*(u_char *)opts)) opts++;
			}
			if (*opts != '-')
				opts = "-rw";
			
			dirinit(dir, map, opts+1, 0);
		}
	}
	(void) fclose(fp);
}
