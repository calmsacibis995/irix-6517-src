/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*#ident	"@(#)fuser:fuser.c	1.27.4.7"*/
#ident	"$Revision"
/*
 * Copyright 1992, Silicon Graphics, Inc.  All rights reserved.
 *
 * fuser - identify processes using a file or filesystem
 */

#include <bstring.h>
#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysmp.h>
#include <sys/utssys.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <netinet/in.h>

/*
 * Return a pointer to the mount point matching the given special name,
 * if possible, otherwise, exit 1 if MNTTAB corruption is detected, else
 * return NULL.
 *
 * NOTE:  the underlying storage for mnt->mnt_dir is defined as static
 * in libc.  Repeated calls to getmntent() overwrite it; to save mntent
 * structures would require copying the member strings elsewhere.
 */
char *
spec_to_mount(char *specname)
{
	FILE	*frp;
	struct mntent *mnt;

	/* get mount-point */
	if ((frp = setmntent(MOUNTED, "r")) == NULL)
		return NULL;
	while (mnt = getmntent(frp))
		if (strcmp(mnt->mnt_fsname, specname) == 0)
			break;
	if (ferror(frp)) {
		fprintf(stderr, "fuser: %s is corrupted\n", MNTTAB);
		exit(1);
	}
	endmntent(frp);
	if (mnt)
		return mnt->mnt_dir;
	return NULL;
}

/*
 * The main objective of this routine is to allocate an array of f_user_t's.
 * In order for it to know how large an array to allocate, it must know
 * the value of v.v_proc in the kernel.  To get this, use sysmp(2) to get
 * the address of v, seek to the equivalent offset in /dev/kmem and read v,
 * then allocate v.v_proc f_user_t structs.  Return the allocation result.
 */
f_user_t *
get_f_user_buf()
{
	u_int var_addr;
	int kmfd, error;
	struct var var;

	var_addr = sysmp(MP_KERNADDR, MPKA_VAR) & 0x7fffffff;
	kmfd = open(_PATH_KMEM, O_RDONLY);
	if (kmfd < 0)
		return NULL;
	error = lseek(kmfd, var_addr, 0) != var_addr
		|| read(kmfd, &var, sizeof var) != sizeof var;
	(void) close(kmfd);
	if (error)
		return NULL;
	return malloc(var.v_proc * sizeof(f_user_t));
}

/*
 * display the fuser usage message and exit
 */
void
usage()
{
	fprintf(stderr,
	    "Usage: fuser [-kqu[c|f]] files [-[kqu[c|f]] files]\n");
	fprintf(stderr,
	    "\tfiles can be either pathnames, or socket specifiers of the form:\n");
	fprintf(stderr,
	     "\thost{.,:}port/proto, where 'proto' is one of 'tcp' or 'udp'\n");
	exit(1);
}

struct co_tab {
	int	c_flag;
	char	c_char;
};

static struct co_tab code_tab[] = {
	{F_CDIR, 'c'},
	{F_RDIR, 'r'},
	{F_TEXT, 't'},
	{F_OPEN, 'o'},
	{F_MAP, 'm'},
	{F_TTY, 'y'}, 		/* XXXbe does this work? */
	{F_TRACE, 'a'}		/* trace file */
};

pid_t	mypid;			/* this fuser process's id */
int	quiet;			/* -q: print just a pid list, for ps -p */

/*
 * Show pids and usage indicators for the nusers processes in the users list.
 * When usrid is non-zero, give associated login names.  When gun is non-zero,
 * issue kill -9's to those processes.
 */
void
report(f_user_t *users, int nusers, int usrid, int gun)
{
	int cind;
	static int printed;

	for ( ; nusers; nusers--, users++) {
		if (quiet) {
			fprintf(stdout, "%s%d",
				(printed ? "," : ""), (int) users->fu_pid);
			printed = 1;
		} else {
			fprintf(stdout, " %7d", users->fu_pid);
			fflush(stdout);
			for (cind = 0;
			     cind < sizeof(code_tab) / sizeof(struct co_tab);
			     cind++) {
				if (users->fu_flags & code_tab[cind].c_flag) {
					fprintf(stderr, "%c",
						code_tab[cind].c_char);
				}
			}
		}
		if (usrid) {
			/*
			 * print the login name for the process
			 */
			struct passwd *pwdp;
			if ((pwdp = getpwuid(users->fu_uid)) != NULL) {
			 	fprintf(stderr, "(%s)", pwdp->pw_name);
			}
		}
		/*
		 * Be careful to avoid suicide.  Give the victim process an
		 * opportunity to die via the sginap(0) yield.
		 */
		if (gun && users->fu_pid != mypid) {
			(void) kill(users->fu_pid, SIGKILL);
			sginap(0L);
		}
	}
}

int
try_anonymous(char *ident, f_user_t *users)
{
	char proto[4], host[MAXHOSTNAMELEN], port[50];
	u_short iport;
	struct hostent *hp;
	struct sockaddr_in sin;
	struct fid fid;
	f_anonid_t fa;
	struct protoent *pr;
	struct servent *se;

	/*
	 * Interpret a name such as 1023/tcp or localhost:630/udp as a tcp
	 * or udp port number, possibly qualified by a local address.
	 */
	/* hack to treat 1.2.3.4.25 as 1.2.3.4:25 */
	if (strchr(ident, ':') == 0) {
		char *p = strrchr(ident, '.');
		if (p)
			*p = ':';
	}
	/* try to match host:port/proto */
	if (sscanf(ident, "%254[^:]:%49[^/]/%3s", host, port, proto) == 3) {
		hp = gethostbyname(host);
		if (hp == 0 || hp->h_addrtype != AF_INET) {
			errno = ENOENT;
			return -1;
		}
	} else if (sscanf(ident, "%49[^/]/%3s", port, proto) == 2) {
		/* matched port/proto */
		hp = 0;
	} else {
		errno = ENOENT;
		return -1;
	}

	/* look up proto first, since we need it for getservbyname() */
	pr = getprotobyname(proto);
	if (pr == 0) {
		fprintf(stderr,"fuser: %s: unknown protocol\n", proto);
		errno = ENOENT;
		return -1;
	}
	if ((iport = atoi(port)) <= 0) {
		se = getservbyname(port, proto);
		if (se == 0) {
			fprintf(stderr,
				"fuser: %s/%s: unknown service\n", port, proto);
			errno = ENOENT;
			return -1;
		}
		sin.sin_port = se->s_port;
	} else {
		sin.sin_port = htons(iport);
	}

	fid.fid_len = sizeof(struct sockaddr_in) + 1; /* pass one proto byte */
	sin.sin_family = AF_INET;
	if (hp)
		bcopy(hp->h_addr, &sin.sin_addr, sizeof sin.sin_addr);
	else
		sin.sin_addr.s_addr = INADDR_ANY;
	bzero(sin.sin_zero, sizeof sin.sin_zero);
	bcopy(&sin, fid.fid_data, sizeof sin);
	fid.fid_data[sizeof(struct sockaddr_in)] = (char)pr->p_proto;
	fa.fa_fid = &fid;
	(void) strncpy(fa.fa_fsid, FSID_SOCKET, sizeof fa.fa_fsid);

	return utssys(&fa, F_ANONYMOUS, UTS_FUSERS, users);
}

/*
 * Determine which processes are using a named file or file system.
 * On stdout, show the pid of each process using each command line file
 * with indication(s) of its use(s).  Optionally display the login
 * name with each process.  Also optionally, issue a kill to each process.
 *
 * When any error condition is encountered, possibly after partially
 * completing, fuser exits with status 1.  If no errors are encountered,
 * exits with status 0.
 *
 * The preferred use of the command is with a single file or file system.
 */
main(int argc, char **argv)
{
	int gun = 0, usrid = 0, contained = 0, file_only = 0;
	int newfile = 0;
	register int i, j;
	char *mntname;
	int nusers;
	f_user_t *users;

	if (argc < 2) {
		usage();
	}
	if ((users = get_f_user_buf()) == NULL) {
		fprintf(stderr, "fuser: could not allocate buffer\n");
		exit(1);
	}
	mypid = getpid();
	for (i = 1; i < argc; i++) {
		int okay = 0;

		if (argv[i][0] == '-') {
			/* options processing */
			if (newfile) {
				gun = usrid = contained = file_only =
				    newfile = 0;
			}
			for (j = 1; argv[i][j] != '\0'; j++) {
				switch(argv[i][j]) {
				case 'k':
					if (gun) {
						usage();
					}
					gun = 1;
					break;
				case 'u':
					if (usrid) {
						usage();
					}
					usrid = 1;
					break;
				case 'c':
					if (contained) {
						usage();
					}
					if (file_only) {
						goto picky;
					}
					contained = 1;
					break;
				case 'f':
					if (file_only) {
						usage();
					}
					if (contained) {
				picky:
						fprintf(stderr,
			    "fuser: -c and -f can't both be used for a file\n");
						usage();
					}
					file_only = 1;
					break;
				case 'q':
					quiet = 1;
					break;
				default:
					fprintf(stderr,
					    "fuser: illegal option %c.\n",
					    argv[i][j]);
					usage();
				}
			}
			continue;
		}
		newfile = 1;

		/*
* if not file_only, attempt to translate special name to mount point, via
* /etc/mnttab.  issue: utssys -c mount point if found, and utssys special, too?
* take union of results for report?????
*/
		/* First print file name on stderr (so stdout (pids) can
		 * be piped to kill) */
		if (!quiet)
			fprintf(stderr, "%s: ", argv[i]);

		if (!(file_only || contained) && (mntname =
		    spec_to_mount(argv[i])) != NULL) {
			if ((nusers = utssys(mntname, F_CONTAINED, UTS_FUSERS,
			    users)) != -1) {
				report(users, nusers, usrid, gun);
				okay = 1;
			}
		}
		nusers = utssys(argv[i], contained ? F_CONTAINED : 0,
		    UTS_FUSERS, users);
		if (nusers == -1) {
			if (!okay) {
				nusers = try_anonymous(argv[i], users);
				if (nusers < 0) {
					perror("fuser");
					exit(1);
				}
				report(users, nusers, usrid, gun);
			}
		} else {
			report(users, nusers, usrid, gun);
		}
		if (!quiet)
			fprintf(stderr,"\n");
	}

	/* newfile is set when a file is found.  if it isn't set here,
	 * then the user did not use correct syntax  */
	if (!newfile) {
		fprintf(stderr, "fuser: missing file name\n");
		usage();
	}
	exit(0);
}
