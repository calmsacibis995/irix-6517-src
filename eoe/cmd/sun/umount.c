/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * umount
 */

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/file.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/termio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <netdb.h>
#include <paths.h>
#include <sat.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/fs/nfs.h>
#include <rpcsvc/mount.h>
#include <sys/types.h>
#include <sys/prctl.h>

/*
 * This structure is used to build a list of mntent structures
 * in reverse order from /etc/mtab.
 */
struct mntlist {
	struct mntent *mntl_mnt;
	struct mntlist *mntl_next;
};

/*
 * Used to build dependency tree of mount points so that umount -a
 * can be performed in parallel.
 */

struct mnttree {
        struct mntent *mt_mnt;
        struct mnttree *mt_sib;
        struct mnttree *mt_kid;
};

#define	KILLSLEEP	5

#define	HANDLE(handler) { \
	(void) sigset(SIGHUP, handler); \
	(void) sigset(SIGINT, handler); \
	(void) sigset(SIGTERM, handler); \
}

static int	all = 0;
static char	*allbut = 0;
static int	autofs = 0;
static jmp_buf	context;
static int	dokill = 0;
static int	host = 0;
static char	*hoststr;
static int	interrupted = 0;
static int	mac_enabled;
static int	status;
static int	type = 0;
static char	*type_list;
static int	verbose = 0;
static int	child_count = 16;
#define MAX_THRD 32

static	int	umount_threads[MAX_THRD];
static	int	threads;


static void		catch(int);
static int		eachreply(void *, struct sockaddr_in *);
static void		fixpath(void);
static void		jump(int);
static mac_t		mac_ip(struct mntent *);
static struct mntlist	*mkmntlist(void);
static struct mntent	*mntdup(struct mntent *);
static void		mntfree(struct mntent *);
static int		nameinlist(char *, char *);
static void		nomem(void);
static void		rpctoserver(struct mntent *);
static void		umountlist(int, char **);
static int		umountmnt(struct mntent *);
static void		usage(void);
static void		*xmalloc(size_t);
static int		substr(char *, char *);
static struct mnttree	*maketree();

int
main(int argc, char **argv)
{
	int	c;

	if (cap_envl(0, CAP_MOUNT_MGT, 0)) {
		(void) fprintf(stderr, "Insufficient privilege\n");
		exit(1);
	}
	umask(0);
	while ((c = getopt(argc, argv, "Aab:F:h:km:t:T:v")) != -1) {
		switch (c) {
		case 'A':
			/*
			 * from autofs, so don't use realpath
			 */
			autofs = 1;
			break;
		case 'a':
			all = 1;
			break;
		case 'b':
			all = 1;
			allbut = optarg;
			break;
		case 'h':
			all = 1;
			host = 1;
			hoststr = optarg;
			break;
		case 'm':
			child_count = atoi(optarg);
			if (child_count == 0 || child_count > MAX_THRD) {
				child_count = 16;
			}
			break;
		case 'k':
			dokill = 1;
			break;
		case 'F':	/*
				 * ABI doesn't really require this,
				 * but do it anyway
				 * to be consistent with the mount command
				 */
		case 't':
		case 'T':	/* multiple types */
			all = 1;
			type = 1;
			type_list = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "umount: unknown option '%c'\n", c);
			usage();
		}
	}
	if (all == (optind < argc))
		usage();
	mac_enabled = sysconf(_SC_MAC) > 0;
	setlinebuf(stderr);
	umountlist(argc - optind, &argv[optind]);
	exit(status);
	/* NOTREACHED */
}

/* ARGSUSED */
static void
catch(int sig)
{
	interrupted = 1;
}


/*
 * Build the umount dependency tree
 */
static struct mnttree *
maketree(mt, mnt)
        struct mnttree *mt;
        struct mntent *mnt;
{
	struct mnttree *new;

        if (mt == NULL) {
                mt = (struct mnttree *)xmalloc(sizeof (struct mnttree));
                mt->mt_mnt = mnt;
                mt->mt_sib = NULL;
                mt->mt_kid = NULL;
        } else {
                if (substr(mnt->mnt_dir, mt->mt_mnt->mnt_dir)) {
                        new = maketree(mt->mt_kid, mnt);
			new->mt_kid = mt;
			mt = new;
		} else if (substr(mt->mt_mnt->mnt_dir, mnt->mnt_dir)) {
			mt->mt_kid = maketree(mt->mt_kid, mnt);
                } else {
			mt->mt_sib = maketree(mt->mt_sib, mnt);
                }
        }
        return (mt);
}

struct mnttree * mkmnttree(void)
{
	struct mnttree *mtree = NULL;
        struct mntent *mnt;
	struct mntlist  *mntl;
	struct mntlist  *mntst = NULL;
        FILE            *mounted;
	char		*colon;

        mounted = setmntent(MOUNTED , "r");
        if (mounted == NULL) {
                perror(MOUNTED);
                exit(1);
        }

	while ((mnt = getmntent(mounted)) != NULL) {
		if (!strcmp(mnt->mnt_dir, "/"))
			continue;

		if (!strcmp(mnt->mnt_type, MNTTYPE_AUTOFS) ||
		    !strcmp(mnt->mnt_type, MNTTYPE_IGNORE))
			continue;

		if (allbut && nameinlist(mnt->mnt_dir, allbut))
			continue;

		if (type && !nameinlist(mnt->mnt_type, type_list))
			continue;


		if (host) {
			if (strcmp(MNTTYPE_NFS, mnt->mnt_type) &&
			    strcmp(MNTTYPE_NFS3, mnt->mnt_type))
				continue;
			colon = strchr(mnt->mnt_fsname, ':');
			if (colon) {
				*colon = '\0';
				if (strcmp(hoststr, mnt->mnt_fsname)) {
					*colon = ':';
					continue;
				}
				*colon = ':';
			} else
				continue;
		}

                mntl = xmalloc(sizeof(*mntl));
                mntl->mntl_mnt = mntdup(mnt);
                mntl->mntl_next = mntst;
                mntst = mntl;
        }
        endmntent(mounted);

        for (; mntst; mntst = mntst->mntl_next) {
		mtree = maketree(mtree, mntst->mntl_mnt);
        }

	return(mtree);
}

int	process_umount(struct mntent *mnt)
{
	int	i = child_count, pid, e_status;

	while (threads >= child_count) {
		pid = wait(&e_status);
		if (pid < 0) {
			perror("on wait");
		}

		/* If someone stopped a child process then we stop too */
		if (WIFSTOPPED(e_status)) {
			continue;
		}

		/* Collect child exit status */
		if (WEXITSTATUS(e_status)) status = 1;
			
		threads--;

		for (i = 0; i < child_count; i++) {
			if (umount_threads[i] == pid) {
				umount_threads[i] = 0;
				break;
			}
		}
	}

	if (i == child_count) {
		for (i = 0; i < child_count; i++) {
			if (umount_threads[i] == 0) {
				break;
			}
		}
	}

	pid = fork();
	switch (pid) {
	case -1:
		fprintf(stderr, "umount: ");
		fflush(stderr);
		perror(mnt->mnt_fsname);
		break;
	case 0:
		umountmnt(mnt);
		exit(status);
	default:
		threads++;
		umount_threads[i] = pid;
	}

	return(0);
}

void	wait_completion()
{
	int	i, pid, e_status;

	while (threads) {
		pid = wait(&e_status);
		/* If someone stopped a child process then we stop too */
		if (WIFSTOPPED(e_status)) {
			continue;
		}

		/* Collect child exit status */
		if (WEXITSTATUS(e_status)) status = 1;

		for (i = 0; i < child_count; i++) {
			if (umount_threads[i] == pid) {
				umount_threads[i] = 0;
				break;
			}
		}
		threads--;
	}
}

void	umounttree(mt)
        struct mnttree *mt;
{
        if (mt) {
		if (mt->mt_kid) {
			umounttree(mt->mt_kid);
			wait_completion();
		}
		process_umount(mt->mt_mnt);
		mntfree(mt->mt_mnt);
		mt->mt_mnt = NULL;
                umounttree(mt->mt_sib);
        }
}


static void
umountlist(int argc, char **argv)
{
	char		*colon;
	char		*dirname;
	int		i;
	int		pid;
	struct mntent	*mnt;
	struct mntlist	*mntcur;
	struct mntlist	*mntl;
	struct mntlist	*mntrev;
	int		oldi;
	struct mntlist	*previous;
	struct mntlist	*temp;
	struct mnttree  *mtree;

	HANDLE(catch);
	if (all) {
		if (!host &&
		    (!type ||
		     (type && nameinlist(MNTTYPE_NFS, type_list) == 0))) {
			pid = fork();
			if (pid < 0) {
				perror("umount: fork");
				status = 1;
			}
			if (pid == 0) {
				int	d;

				d = open("/dev/tty", O_RDWR);
				ioctl(d, TIOCNOTTY, 0);
				for (d = getdtablesize(); --d >= 0; )
					close(d);
				clnt_broadcast(MOUNTPROG, MOUNTVERS,
					MOUNTPROC_UMNTALL, xdr_void, NULL,
					xdr_void, NULL, eachreply);
				exit(0);
			}
		}

		mtree = mkmnttree();
		umounttree(mtree);
		wait_completion();

		return;
	}
	/*
	 * get a last first list of mounted stuff, reverse list and
	 * null out entries that get unmounted.
	 */
	for (mntl = mkmntlist(), mntcur = mntrev = NULL;
	     mntl != NULL;
	     mntcur = mntl, mntl = mntl->mntl_next, mntcur->mntl_next = mntrev,
	     mntrev = mntcur) {
		mnt = mntl->mntl_mnt;
		if (!strcmp(mnt->mnt_dir, "/"))
			continue;
		if (!strcmp(mnt->mnt_type, MNTTYPE_AUTOFS) ||
		    !strcmp(mnt->mnt_type, MNTTYPE_IGNORE))
			continue;
		for (i = 0; i < argc; i++) {
			if (argv[i][0] == '\0')
				continue;
			if (autofs) {
				int hasspace = 0;

				/*
				 * take the trailing space off the
				 * directory name for comparison with
				 * the mount table entry
				 */
				dirname = argv[i];
				if (dirname[strlen(dirname) - 1] == ' ') {
					hasspace = 1;
					dirname[strlen(dirname) - 1] = '\0';
				}
				if (!strcmp(mnt->mnt_dir, dirname)) {
					/*
					 * put the space back on and
					 * substitute the dir name from
					 * the command line for the one
					 * in the mount table entry
					 */
					if (hasspace)
						dirname[strlen(dirname)] = ' ';
					free(mnt->mnt_dir);
					mnt->mnt_dir = dirname;
					if (umountmnt(mnt)) {
						mnt->mnt_dir = NULL;
						mntfree(mnt);
						mntl->mntl_mnt = NULL;
					}
					argv[i][0] = '\0';
					break;
				}
				/*
				 * put the space back on the name
				 */
				if (hasspace)
					dirname[strlen(dirname)] = ' ';
			} else if (!strcmp(mnt->mnt_dir, argv[i]) ||
				   !strcmp(mnt->mnt_fsname, argv[i])) {
				if (umountmnt(mnt)) {
					mntfree(mnt);
					mntl->mntl_mnt = NULL;
				}
				argv[i][0] = '\0';
				break;
			}
		}
	}
	for (i = 0; i < argc; i++) {
		if (argv[i][0] != '\0')
			break;
	}
	if (i == argc)
		return;
	/*
	 * Now find those arguments which are links, resolve them
	 * and then umount them. This is done separately because it
	 * "stats" the arg, and it may hang if the server is down.
	 */
	for (oldi = i, previous = NULL;
	     mntcur != NULL;
	     temp = mntcur, mntcur = mntcur->mntl_next,
	     temp->mntl_next = previous, previous = temp)
		continue;
	for (mntrev = NULL, mntl = previous;
	     mntl != NULL;
	     mntcur = mntl, mntl = mntl->mntl_next,
	     mntcur->mntl_next = mntrev, mntrev = mntcur) {
		struct stat64	mnts;
		int		mnts_state = 0;

		if ((mnt = mntl->mntl_mnt) == NULL) 
			continue;	/* Already unmounted */
		if (!strcmp(mnt->mnt_dir, "/"))
			continue;
		if (!strcmp(mnt->mnt_type, MNTTYPE_AUTOFS) ||
		    !strcmp(mnt->mnt_type, MNTTYPE_IGNORE))
			continue;
		for (i = oldi; i < argc; i++) {
			char		resolve[MAXPATHLEN];
			struct stat64	stb;

			if (argv[i][0] == '\0')
				continue;
			if (!autofs) {
				if (realpath(argv[i], resolve) == 0) {
					fprintf(stderr, "umount: ");
					fflush(stderr);
					perror(resolve);
					argv[i][0] = '\0';
					continue;
				}
			} else
				strcpy(resolve, argv[i]);
			if (!strcmp(mnt->mnt_dir, resolve) ||
			    (!autofs && !strcmp(mnt->mnt_fsname, resolve))) {
				if (umountmnt(mnt)) {
					mntfree(mnt);
					mntl->mntl_mnt = NULL;
				}
				argv[i][0] = '\0';
				break;
			}
			if (autofs)
				continue;
			if (mnts_state == 0) {
				if (stat64(mnt->mnt_fsname, &mnts) != 0)
					mnts_state = -1;
				else
					mnts_state = 1;
			}
			if (mnts_state == 1 &&
			    S_ISBLK(mnts.st_mode) &&
			    stat64(argv[i], &stb) == 0 &&
			    S_ISBLK(stb.st_mode) &&
			    mnts.st_rdev == stb.st_rdev) {
				if (umountmnt(mnt)) {
					mntfree(mnt);
					mntl->mntl_mnt = NULL;
				}
				argv[i][0] = '\0';
				break;
			}
		}
	}
	/*
	 * For all the remaining ones including the ones which have
	 * no corresponding entry in /etc/mtab file.
	 */
	for (i = oldi; i < argc; i++) {
		struct mntent	tmpmnt;

		if (argv[i][0] == '\0')
			continue;
		if (argv[i][0] != '/') {
			fprintf(stderr,
				"umount: must use full path, "
				"%s not unmounted\n",
				argv[i]);
			continue;
		}
		tmpmnt.mnt_fsname = NULL;
		tmpmnt.mnt_dir = argv[i];
		tmpmnt.mnt_type = NULL;
		tmpmnt.mnt_opts = NULL;
		tmpmnt.mnt_passno = tmpmnt.mnt_freq = 0;
		umountmnt(&tmpmnt);
	}
}

/*
 * Returns true if s1 is a pathname substring of s2.
 */
substr(s1, s2)
        char *s1;
        char *s2;
{
        while (*s1 == *s2) {
                s1++;
                s2++;
        }
        if (*s1 == '\0' && *s2 == '/') {
                return (1);
        }
        return (0);
}


static int
nameinlist(char *name, char *list)
{
	char	copy[BUFSIZ];
	char	*element;

	list = strcpy(copy, list);
	while ((element = strtok(list, ",")) != 0) {
		if (!strcmp(element, name))
			return 1;
		list = 0;
	}
	return 0;
}

static void
jump(int sig)
{
	catch(sig);
	longjmp(context, 1);
}

/*
 * Make sure that /etc and /usr/etc are in the
 * path before execing user supplied umount program.
 * We set the path to _PATH_ROOTPATH so no security problems can creep in.
 */
static void
fixpath(void)
{
	char		*newpath;
	static char	prefix[] = "PATH=";

	/*
	 * Allocate enough space for the path and the trailing null.
	 */
	newpath = xmalloc(strlen(prefix) + strlen(_PATH_ROOTPATH) + 1);
	sprintf(newpath, "%s%s", prefix, _PATH_ROOTPATH);
	putenv(newpath);
}

static int
umountmnt(struct mntent *mnt)
{
	cap_value_t	cap_audit_write[] = {CAP_AUDIT_WRITE};
	cap_value_t	cap_mac_write[] = {CAP_MAC_WRITE};
	cap_value_t	umount_caps[] = {CAP_MOUNT_MGT, CAP_MAC_READ,
					 CAP_DAC_READ_SEARCH};
	char		*mntdir;
	cap_t		ocap;

	if (interrupted)
		return 0;
	ocap = cap_acquire(3, umount_caps);
	if (umount(mnt->mnt_dir) < 0) {
		int	serrno;

		cap_surrender(ocap);
		if (errno == EBUSY && dokill) {
			char	cmd[MNTMAXSTR];

			sprintf(cmd,
				verbose ?
					"/sbin/fuser -k %s" :
					"2>&1 /sbin/fuser -k %s > /dev/null",
				mnt->mnt_type &&
				strcmp(mnt->mnt_type, MNTTYPE_AUTOFS) ?
					mnt->mnt_fsname :
					mnt->mnt_dir);
			system(cmd);
			sleep(KILLSLEEP);
			ocap = cap_acquire(3, umount_caps);
			if (umount(mnt->mnt_dir) == 0)
				goto success;
			cap_surrender(ocap);
		}
		serrno = errno;
		/*
		 * audit failure (does nothing if no auditing)
		 */
		ocap = cap_acquire(1, cap_audit_write);
		satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
			"umount: failed to unmount %s (on %s): %s",
			mnt->mnt_fsname, mnt->mnt_dir, strerror(serrno));
		errno = serrno;
		cap_surrender(ocap);
		switch (errno) {
		case EINVAL:
			break;
		case EBUSY:
			/*
			 * If this is autofs and verbose is not on,
			 * just return 0.
			 * Otherwise, print an error message.
			 */
			if (autofs && !verbose) {
				status = 1;
				return 0;
			}
		default:
			perror(mnt->mnt_dir);
			status = 1;
			return 0;
		}
		fprintf(stderr, "%s not mounted\n", mnt->mnt_dir);
		if (autofs && mnt->mnt_dir[strlen(mnt->mnt_dir) - 1] == ' ')
			mnt->mnt_dir[strlen(mnt->mnt_dir) - 1] = '\0';
		/*
		 * remove the entry from the mount table
		 * do this especially when we find an unmounted file system
		 */
		ocap = cap_acquire(1, cap_mac_write);
		delmntent(MOUNTED, mnt);
		cap_surrender(ocap);
		return 1;
	} else {
success:
		cap_surrender(ocap);
		if (autofs && mnt->mnt_dir[strlen(mnt->mnt_dir) - 1] == ' ')
			mnt->mnt_dir[strlen(mnt->mnt_dir) - 1] = '\0';
		/*
		 * umount succeeded, remove the entry from the mount table
		 * we do this for each unmount to ensure the integrity of
		 * the table in the face of potential parallel mounts and
		 * umounts
		 */
		ocap = cap_acquire(1, cap_mac_write);
		delmntent(MOUNTED, mnt);
		cap_surrender(ocap);
		/*
		 * audit success (does nothing if no auditing)
		 */
		ocap = cap_acquire(1, cap_audit_write);
		satvwrite(SAT_AE_MOUNT, SAT_SUCCESS,
			"umount: unmounted %s (on %s)", mnt->mnt_fsname,
			mnt->mnt_dir);
		cap_surrender(ocap);
		/*
		 * mnt_type is NULL if we faked up the entry
		 * (grep for tmpmnt)
		 */
		if (mnt->mnt_type == NULL)
			;
		else if (!strcmp(mnt->mnt_type, MNTTYPE_NFS) ||
			 !strcmp(mnt->mnt_type, MNTTYPE_NFS3)) {
			if (!setjmp(context)) {
				HANDLE(jump);
				rpctoserver(mnt);
				HANDLE(catch);
			}
		} else if (!strcmp(mnt->mnt_type, MNTTYPE_EFS)) {
			;
		} else if (!strcmp(mnt->mnt_type, MNTTYPE_XFS)) {
			;
		} else {
			int	pid;
			char	umountcmd[128];
			int	wstatus;

			/*
			 * user-level filesystem...attempt to exec
			 * external umount_FS program.
			 */
			pid = fork();
			switch (pid) {
			case -1:
				fprintf(stderr, "umount: ");
				fflush(stderr);
				perror(mnt->mnt_fsname);
				break;
			case 0:
				fixpath();
				sprintf(umountcmd, "umount_%s", mnt->mnt_type);
				/*
				 * strip off the autofs trailing blank
				 */
				mntdir = strdup(mnt->mnt_dir);
				if (mntdir[strlen(mntdir) - 1] == ' ')
					mntdir[strlen(mntdir) - 1] = '\0';
				execlp(umountcmd, umountcmd, mnt->mnt_fsname,
					mnt->mnt_dir, mnt->mnt_type,
					mnt->mnt_opts, 0);
				/*
				 * Don't worry if there is no user-written
				 * umount_FS program.
				 */
				exit(1);
			default:
				while (wait(&wstatus) != pid)
					continue;
				/*
				 * Ignore errors in user-written umount_FS.
				 */
			}
		}
		if (verbose)
			fprintf(stderr, "%s: Unmounted\n", mnt->mnt_dir);
		return 1;
	}
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: umount -A -a[kv] [-b list] [-t|F type] "
		"[-T type1,type2...] [-h host]\n");
	fprintf(stderr, "       umount [-kv] path ...\n");
	exit(1);
	/* NOTREACHED */
}

static void
rpctoserver(struct mntent *mnt)
{
	cap_value_t		cap_mac_relabel_subj[] = {CAP_MAC_RELABEL_SUBJ};
	static CLIENT		*client;
	struct hostent		*hp;
	mac_t			ipmac = NULL;
	cap_t			ocap;
	mac_t			original = NULL;
	char			*p;
	struct timeval		pmaptot;
	static char		*prevhost;
	enum clnt_stat		rpc_stat;
	int			s;
	struct sockaddr_in	sin;
	struct timeval		timeout;

	if (mac_enabled)
		ipmac = mac_ip(mnt);
	/*
	 * If the previous server's the same as this one, try to
	 * reuse its client handle. If there's no handle, the
	 * previous effort to contact the server failed there's
	 * no need to try again.
	 */
	if ((p = strchr(mnt->mnt_fsname, ':')) == NULL)
		return;
	*p++ = '\0';
	if (prevhost && !strcmp(prevhost, mnt->mnt_fsname)) {
		if (!client)
			return;
	} else {
		if ((hp = gethostbyname(mnt->mnt_fsname)) == NULL) {
			fprintf(stderr, "%s not in hosts database\n",
				mnt->mnt_fsname);
			return;
		}
		if (prevhost)
			free(prevhost);
		prevhost = strdup(mnt->mnt_fsname);
		bzero(&sin, sizeof(sin));
		bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
		sin.sin_family = AF_INET;
		s = RPC_ANYSOCK;
		timeout.tv_sec  = 2;	/* initial retransmit time */
		pmaptot.tv_sec  = 30;	/* total time for pmap call */
		timeout.tv_usec = pmaptot.tv_usec = 0;
		pmap_settimeouts(timeout, pmaptot);
		if (client)
			clnt_destroy(client);
		if (ipmac != NULL) {
			original = mac_get_proc();
			ocap = cap_acquire(1, cap_mac_relabel_subj);
			mac_set_proc(ipmac);
			cap_surrender(ocap);
		}
		if ((client = clntudp_create(&sin, MOUNTPROG, MOUNTVERS,
				timeout, &s)) == NULL) {
			if (ipmac != NULL) {
				ocap = cap_acquire(1, cap_mac_relabel_subj);
				mac_set_proc(original);
				cap_surrender(ocap);
				mac_free(ipmac);
				mac_free(original);
			}
			fprintf(stderr,
				"Warning: umount: unable to notify %s\n",
				mnt->mnt_fsname);
			return;
		}
		client->cl_auth = authunix_create_default();
	}
	timeout.tv_sec = 30;	/* total time */
	timeout.tv_usec = 0;
	rpc_stat = clnt_call(client, MOUNTPROC_UMNT, xdr_path, &p, xdr_void,
		NULL, timeout);
	if (ipmac != NULL) {
		ocap = cap_acquire(1, cap_mac_relabel_subj);
		mac_set_proc(original);
		cap_surrender(ocap);
		mac_free(original);
		mac_free(ipmac);
	}
	if (rpc_stat != RPC_SUCCESS) {
		fprintf(stderr, "Warning: umount: unable to notify %s\n",
			mnt->mnt_fsname);
		clnt_destroy(client);
		client = NULL;
		return;
	}
}

/* ARGSUSED */
static int
eachreply(void *resultsp, struct sockaddr_in *addrp)
{
	return 1;
}

static void
nomem(void)
{
	fprintf(stderr, "umount: ran out of memory!\n");
	exit(1);
}

static void *
xmalloc(size_t size)
{
	void	*ret;

	if ((ret = malloc(size)) == NULL)
		nomem();
	return ret;
}

static struct mntent *
mntdup(struct mntent *mnt)
{
	struct mntent	*new;

	new = xmalloc(sizeof(*new));
	new->mnt_fsname = strdup(mnt->mnt_fsname);
	new->mnt_dir = strdup(mnt->mnt_dir);
	new->mnt_type = strdup(mnt->mnt_type);
	new->mnt_opts = strdup(mnt->mnt_opts);
	if (!new->mnt_fsname || !new->mnt_dir ||
	    !new->mnt_type || !new->mnt_opts)
		nomem();
	new->mnt_freq = mnt->mnt_freq;
	new->mnt_passno = mnt->mnt_passno;
	return new;
}

static void
mntfree(struct mntent *mnt)
{
	if (mnt->mnt_fsname)
		free(mnt->mnt_fsname);
	if (mnt->mnt_dir)
		free(mnt->mnt_dir);
	if (mnt->mnt_type)
		free(mnt->mnt_type);
	if (mnt->mnt_opts)
		free(mnt->mnt_opts);
	free(mnt);
}

static struct mntlist *
mkmntlist(void)
{
	struct mntent	*mnt;
	struct mntlist	*mntl;
	struct mntlist	*mntst = NULL;
	FILE		*mounted;

	mounted = setmntent(MOUNTED, "r");
	if (mounted == NULL) {
		perror(MOUNTED);
		exit(1);
	}
	while ((mnt = getmntent(mounted)) != NULL) {
		mntl = xmalloc(sizeof(*mntl));
		mntl->mntl_mnt = mntdup(mnt);
		mntl->mntl_next = mntst;
		mntst = mntl;
	}
	endmntent(mounted);
	return mntst;
}

static mac_t
mac_ip(struct mntent *mnt)
{
	char	*equal;
	char	*labelname;
	mac_t	lp;
	char	*str;

	/*
	 * Look for the "eag:" mount option.
	 */
	if ((str = hasmntopt(mnt, MNTOPT_EAG)) == NULL)
		return NULL;
	/*
	 * Look for a "mac-ip=" specification.
	 */
	if ((str = strstr(str, MAC_MOUNT_IP)) == NULL)
		return NULL;
	equal = str + strlen(MAC_MOUNT_IP);
	if (*equal != '=') {
		fprintf(stderr, "umount: bad label for option '%s'\n", str);
		return NULL;
	}
	labelname = strdup(equal + 1);
	if ((equal = strchr(labelname, ':')) != NULL)
		*equal = '\0';
	if ((equal = strchr(labelname, ',')) != NULL)
		*equal = '\0';
	if ((equal = strchr(labelname, ' ')) != NULL)
		*equal = '\0';
	if ((equal = strchr(labelname, '\t')) != NULL)
		*equal = '\0';
	lp = mac_from_text(labelname);
	free(labelname);
	if (lp)
		return lp;
	fprintf(stderr, "umount: illegal label for option '%s'\n", str);
	return NULL;
}
