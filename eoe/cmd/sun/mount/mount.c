/*
 * Copyright (c) 1985 Sun Microsystems, Inc.
 */

#define	NFSCLIENT
/*
 * mount
 */
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <diskinfo.h>
#include <errno.h>
#include <limits.h>
#include <mntent.h>
#include <netdb.h>
#include <signal.h>
#include <strings.h>
#include <ustat.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <sys/fs/efs_clnt.h>
#include <sys/fs/nfs.h>
#include <sys/fs/xfs_clnt.h>
#include <sys/fs/bds.h>
#include <rpcsvc/mount.h>
#include <sys/fcntl.h>
#include <sys/fstyp.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sat.h>
#include <paths.h>
#include <mountinfo.h>
#include <sys/fs/nfs_clnt.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <sys/xlv_base.h>

static int mac_enabled;

int	ro = 0;
int	quota = 0;
int	fake = 0;
int	freq = 0;
int	passno = 0;
int	all = 0;
char	*allbut = 0;
char	*host;
int	check = 0;
int	verbose = 0;
int	printed = 0;
int	nomtab = 0;

char	*type_list;

int	mounttree_error = 0;

#define MNTOPTSEP	','	/* separator between mount options in fstab */

/* Longer attribute cache timeouts for "private" mounts. */
#define PRIV_ACREGMIN		30
#define PRIV_ACREGMAX		(3 * ACREGMAX)
#if PRIV_ACREGMAX < PRIV_ACREGMIN 
#undef  PRIV_ACREGMAX
#define PRIV_ACREGMAX		(2 * PRIV_ACREGMIN)
#endif
#define PRIV_ACDIRMIN		(3 * ACDIRMIN)
#define PRIV_ACDIRMAX		(3 * ACDIRMAX)

#define	NRETRY	10000	/* number of times to retry a mount request */
#define	BGSLEEP	5	/* initial sleep time for background mount in seconds */
#define MAXSLEEP 120	/* max sleep time for background mount in seconds */
#define	MNTTYPE_NFS3_PREF	"nfs3pref"
#define	MNTTYPE_NFS2	FSID_NFS2
/*
 * Fake errno for RPC failures that don't map to real errno's
 */
#define ERPC	(10000)
#define OLDENFSREMOTE 135 /* compatibility with ?? */

/*
 * Structure used to build a mount tree.  The tree is traversed to do
 * the mounts and catch dependencies.
 */
struct mnttree {
	struct mntent *mt_mnt;
	struct mnttree *mt_sib;
	struct mnttree *mt_sib_back;
	struct mnttree *mt_kid;
};

/*
 * Structure to hold a list of all currently mounted points from mtab
 */
typedef struct mtab_entry_s {
        char mount_point[PATH_MAX];
        struct mtab_entry_s *next;
} mtab_entry_t;

#define	PARALLEL_MOUNT_MINIMUM	4
int		parallel_mounts = 0;
int		mount_proc_limit = 16;

void		addtomtab(struct mntent *);
void		background(void);
void		fixpath(void);
char		*getnextopt(char **);
struct mnttree	*maketree(struct mnttree *, struct mntent *, struct mnttree *);
void		mntcp(struct mntent *, struct mntent *);
struct mntent	*mntdup(struct mntent *);
int		mount_efs(struct mntent *, struct efs_args *);
int		mount_nfs(struct mntent *, struct nfs_args *, char *, int, int);
#ifdef PCFS
int		mount_pc(struct mntent *, struct pc_args *);
#endif
int		mount_xfs(struct mntent *, struct xfs_args *);
int		mounted(struct mntent *);
int		mountfs(int, struct mntent *);
int		mounttree(struct mnttree *);
int		nameinlist(char *, char *);
int		nopt(struct mntent *, char *, u_int *);
int		adjustnfsargs(char *, struct mntent *);
void		printent(struct mntent *);
void		printmtab(FILE *);
void		printtree(struct mnttree *);
void		replace_opts(char *, int, char *, char *);
void		set_long_timeouts(void);
void		set_short_timeouts(void);
int		substr(char *, char *);
void		usage(void);
void		*xmalloc(int);
int             check_mounted_overlaps(mnt_check_state_t *, struct mntent *);

/*
 * mount -M /root/etc/fstab -P /root -p
 *	prints that fstab with /root prepended to paths
 *	altmtab -- set to "/root/etc/fstab", use instead of MOUNTED "/etc/mtab"
 *	rbase   -- set to "/root", used as prefix for mount and dev paths
 */
char *altmtab = 0;
char *altfstab = 0;
char *rbase = 0;
/*
 * ismntopt implements a more accurate version of hasmntopt.  
 * hasmntopt has no concept of tokens.  hasmntopt(s,"noac")
 * could return pointing to "noac," or "noacl" or "noaction"
 * which causes problems because some joker decided to invent
 * an option called "noacl" which conflict with noac.
 * This routine makes sure that 
 * 	a) hasmntopt's return pointer is actually the beginning of 
 *	   a token.
 *	   (if there is no previous char, or if it is a ',')
 *	b) hasmntopt's return pointer points to a token that is a 
 *	   exact match for opt.
	   (the next char is a '=' or a ',')
 * If this is the case, return the pointer, otherwise return NULL.
 */
static char *
ismntopt(struct mntent *mnt, char *opt)
{
	char *cp, *retp = hasmntopt(mnt, opt);
	if (retp) {
		/* is retp the beginning of a token?*/
		if (retp == mnt->mnt_opts || *(retp-1)==',') {
			/* is retp exactly equal to opt? */
			cp = retp + strlen(opt);
			if (*cp == '\0' || *cp == ',' || *cp == '=') {
				return (retp);
			}  else {
				char c;
				while (*cp != '\0' && *cp != ',' && *cp != '=')
					cp++;
				c = *cp;
				*cp = '\0';
				(void) fprintf(stderr, 
					"mount: invalid option %s ignored\n", 
					retp);
				*cp = c;
			}
		} 
	}

	return (NULL);
}

main(int argc, char **argv)
{
	int realpaths = 1;
	struct mntent mnt;
	struct mntent *mntp;
	FILE *mnttab;
	char *options;
	char *colon;
	struct mnttree *mtree;
	char name[MNTMAXSTR];
	char dir[MNTMAXSTR];
	char type[MNTMAXSTR];
	char opts[MNTMAXSTR];
	int hflag = 0;
	mnt_check_state_t *check_state = 0;
	int skip_mtab_checks = 0;
	mtab_entry_t *mtab_entry_list = 0;
	u_int vers = 0;
	int do_overlap_checks = 1;

	mac_enabled = (sysconf(_SC_MAC) > 0);

	if (argc == 1) {
		mnttab = setmntent(MOUNTED, "r");
		while ((mntp = getmntent(mnttab)) != NULL) {
			if (strcmp(mntp->mnt_type, MNTTYPE_IGNORE) == 0 ||
			    (mntp->mnt_opts != NULL &&
			    strstr(mntp->mnt_opts, "ignore") != NULL)) {
				continue;
			}
			printent(mntp);
		}
		(void) endmntent(mnttab);
		exit(0);
	}

	(void) close(2);
	if (fcntl (1, F_DUPFD, 2) < 0) {
		perror("dup");
		exit(1);
	}

	opts[0] = '\0';
	type[0] = '\0';

	/*
	 * Set options
	 */
	while (argc > 1 && argv[1][0] == '-') {
		options = &argv[1][1];
		while (*options) {
			switch (*options) {
			case 'A':
				/*
				 * The mount is from autofs, so do not
				 * use realpath.  Just accept the mount
				 * point path supplied.
				 */
				realpaths = 0;
				break;
			case 'a':
				all++;
				break;
			case 'b':
				--argc, argv++;
				all++;
				allbut = argv[1];
				break;
			case 'C':
				do_overlap_checks = 0;
				break;
			case 'c':
				check++;
				break;
			case 'f':
				fake++;
				break;
			case 'h':
				--argc, argv++;
				all++;
				hflag = 1;
				host = argv[1];
				break;
			case 'm':
				--argc, argv++;
				mount_proc_limit = strtoll(argv[1], NULL, 0);
				if ((mount_proc_limit <= 0) ||
				    (mount_proc_limit > 128)) {
					usage();
				}
				break;
			case 'n':
				nomtab++;
				break;
			case 'o':
				if (argc < 3) {
					usage();
				}
				(void) strcpy(opts, argv[2]);
				argv++;
				argc--;
				break;
			case 'p':
				if (argc != 2) {
					usage();
				}
				printmtab(stdout);
				exit(0);
			case 'q':
				quota++;
				break;
			case 'r':
				ro++;
				break;
			case 'F':	/* ABI synonym for -t */
			case 't':
				if (argc < 3) {
					usage();
				}
				(void) strcpy(type, argv[2]);
				type_list = type;
				argv++;
				argc--;
				break;
			case 'T':
				if (argc < 3) {
					usage();
				}
				type_list = argv[2];
				all++;
				argv++;
				argc--;
				break;
			case 'v':
				verbose++;
				break;
			case 'M':   /* support alternate to /etc/mtab for -p */
				--argc, argv++;
				altmtab = argv[1];
				break;
			case 'i':   /* support alternate to /etc/fstab */
				--argc, argv++;
				altfstab = argv[1];
				break;
			case 'P':   /* append $rbase to emitted -p paths */
				--argc, argv++;
				rbase = argv[1];
				break;
			default:
				(void) fprintf(stderr,
				    "mount: unknown option: %c\n", *options);
				usage();
			}
			options++;
		}
		argc--, argv++;
	}

	if (cap_envl(0, CAP_MOUNT_MGT, 0)) {
		(void) fprintf(stderr, "Insufficient privilege\n");
		exit(1);
	}

	if (all) {
	    struct stat mnttab_stat;
	    long mnttab_size;
	    int count;
	    int mounts;
	    int which_mtab_read = 0;
	    int max_mtab_entries = 0;
	    FILE *mtab_fp;
	    struct mntent *mtab_entry_p;
	    mtab_entry_t *new_mtab_entry;
	    mtab_entry_t *which_mtab_entry;
	    int continue_flag = 0;

	    if (argc != 1) {
		usage();
	    }

	    /* read all volheaders and setup mounted partition checking */
	    if (do_overlap_checks && (mnt_check_init(&check_state) == -1)) {
		    check_state = 0;
		    (void) fprintf(stderr, "mount: unable to init partition "
				   "checking routines, suspending checks\n");
	    }

	    /* read mount points from /etc/mtab into memory
	     * /etc/mtab is also checked in mountfs() but we do
	     * the checks here to avoid fork() complexty of doing the
	     * partition checks in the back end.
             */
	    mtab_fp = setmntent(MOUNTED, "r");
	    if (mtab_fp == NULL) {
		skip_mtab_checks = 1;
	    } else {
		if (fstat(fileno(mtab_fp), &mnttab_stat) == -1) {
		    skip_mtab_checks = 1;
		    (void) endmntent(mtab_fp);
		} else {

		    for (count = 0; ; count++) {
			if ((mtab_entry_p = getmntent(mtab_fp)) == NULL)
			    break;

			/* copy entry from /etc/mtab */
			new_mtab_entry = xmalloc(sizeof(mtab_entry_t));
			strcpy(new_mtab_entry->mount_point,
			       mtab_entry_p->mnt_dir);
			/* add to list */
			new_mtab_entry->next = mtab_entry_list;
			mtab_entry_list = new_mtab_entry;
		    }

		    if (!count)
			skip_mtab_checks = 1;

		    (void) endmntent(mtab_fp);
		}
	    }


	    /* setup to read mtab */
	    mnttab = setmntent(altfstab ? altfstab : MNTTAB, "r");
	    if (mnttab == NULL) {
		(void) fprintf(stderr, "mount: ");
		perror(altfstab ? altfstab : MNTTAB);
		exit(1);
	    }
	    if (fstat(fileno(mnttab), &mnttab_stat) == -1) {
		(void) fprintf(stderr, "mount: ");
		perror(altfstab ? altfstab : MNTTAB);
		exit(1);
	    }
	    mnttab_size = mnttab_stat.st_size;
	    mtree = NULL;
	    mounts = 0;

	    for (count = 1; ; count++) {
		if ((mntp = getmntent(mnttab)) == NULL) {
		    if (ftell(mnttab) >= mnttab_size)
			break;		/* it's EOF */
		    (void) fprintf(stderr,
				   "mount: %s: illegal entry on line %d\n",
				   altfstab ? altfstab : MNTTAB, count);
		    continue;
		}

		/* 
		 * Addition of hflag is when the -h option (by itself 
		 * and no other "all" options are turned on ) will mount 
		 * entries that have been tagged with "noauto" in the /etc/fstab.
		 *
		 * Skip over 'ignore' 'swap' 'rawdata' 'noauto' and '/'
		 */
		if ((strcmp(mntp->mnt_type, MNTTYPE_IGNORE) == 0)
		    || (strcmp(mntp->mnt_type, MNTTYPE_SWAP) == 0)
		    || (strcmp(mntp->mnt_type, MNTTYPE_RAWDATA) == 0)
		    || (hasmntopt(mntp, MNTOPT_NOAUTO)
			&& (hflag == 0 || all >= 2))
		    || (strcmp(mntp->mnt_dir, "/") == 0) ) {
		    continue;
		}
		if (allbut && nameinlist(mntp->mnt_dir, allbut))
		    continue;
		if (host) {
		    if (strcmp(mntp->mnt_type, MNTTYPE_NFS)
			&& strcmp(mntp->mnt_type, MNTTYPE_NFS2)
			&& strcmp(mntp->mnt_type, MNTTYPE_NFS3)
			&& strcmp(mntp->mnt_type, MNTTYPE_NFS3_PREF))
			continue;
		    strcpy(name, mntp->mnt_fsname);
		    colon = strchr(name, ':');
		    if (colon == 0)
			continue;
		    *colon = '\0';
		    if (strcmp(host, name))
			continue;
		}
		if (type_list && !nameinlist(mntp->mnt_type, type_list))
		    continue;

		/* check if already listed in mtab, if so skip it
		 * this avoids both the mtab check later and possible
                 * partition/mount conflicts during the mountcheck test.
		 */
		if (!skip_mtab_checks && !verbose) {

		    continue_flag = 0;
		    for (which_mtab_entry = mtab_entry_list;
			 which_mtab_entry;
			 which_mtab_entry = which_mtab_entry->next) {

			if (!strcmp(mntp->mnt_dir,
				    which_mtab_entry->mount_point)) {
			    continue_flag++;
			    break;
			}
		    }

		    if (continue_flag)
                        continue;
		}

		/* check for conflicting mounts/overlaps */
		if (check_mounted_overlaps(check_state, mntp))
		    continue;

		if (!strncmp(mntp->mnt_type, MNTTYPE_NFS,strlen(MNTTYPE_NFS))) {
			if (adjustnfsargs(NULL, mntp))
				continue;
		}
		mtree = maketree(mtree, mntp, NULL);
		mounts++;
	    }
	    (void) endmntent(mnttab);
	    /*
	     * If there are enough mounts to do, go ahead and
	     * do them in parallel.
	     */
	    if (mounts >= PARALLEL_MOUNT_MINIMUM) {
		parallel_mounts = 1;
	    }

	    if (check_state)
		mnt_check_end(check_state);

	    exit(mounttree(mtree));
	}

	/*
	 * Command looks like: mount <dev>|<dir>
	 * we walk through /etc/fstab til we match either fsname or dir.
	 */
	if (argc == 2) {
		struct stat mnttab_stat;
		long mnttab_size;
		int count;

		/* read all volheaders and setup mounted partition checking */
		if (do_overlap_checks && (mnt_check_init(&check_state) == -1)) {
		    check_state = 0;
		    (void) fprintf(stderr, "mount: unable to init partition "
				   "checking routines, suspending checks\n");
		}

		mnttab = setmntent(altfstab ? altfstab : MNTTAB, "r");
		if (mnttab == NULL) {
			(void) fprintf(stderr, "mount: ");
			perror(altfstab ? altfstab : MNTTAB);
			exit(1);
		}
		if (fstat(fileno(mnttab), &mnttab_stat) == -1) {
			(void) fprintf(stderr, "mount: ");
			perror(altfstab ? altfstab : MNTTAB);
			exit(1);
		}
		mnttab_size = mnttab_stat.st_size;

		for (count = 1;; count++) {
		    if ((mntp = getmntent(mnttab)) == NULL) {
			if (ftell(mnttab) >= mnttab_size)
			    break;		/* it's EOF */
			(void) fprintf(stderr,
				       "mount: %s: illegal entry on line %d\n",
				       altfstab ? altfstab : MNTTAB, count);
			continue;
		    }
		    if ((strcmp(mntp->mnt_type, MNTTYPE_IGNORE) == 0)
			|| (strcmp(mntp->mnt_type, MNTTYPE_RAWDATA) == 0)
			|| (strcmp(mntp->mnt_type, MNTTYPE_SWAP) == 0)) {
			continue;
		    }
		    if ((strcmp(mntp->mnt_fsname, argv[1]) == 0)
			|| (strcmp(mntp->mnt_dir, argv[1]) == 0)) {

			if (opts[0] != '\0') {
                            /*
			     * "-o" specified; override fstab with
			     * command line options, unless it's
			     * "-o remount", in which case do
			     * nothing if the fstab says R/O (you
			     * can't remount from R/W to R/O, and
			     * remounting from R/O to R/O is not
			     * only invalid but pointless).
			     */
			    if (strcmp(opts, MNTOPT_REMOUNT) == 0
				&& hasmntopt(mntp, MNTOPT_RO))
				exit(0);

			    mntp->mnt_opts = opts;
			    replace_opts(mntp->mnt_opts, ro, MNTOPT_RO,
					 MNTOPT_RW);
			} else if (ro) {
			    mntp->mnt_opts = MNTOPT_RO;
			}

			if (strcmp(type, MNTTYPE_EFS) == 0) {
			    replace_opts(mntp->mnt_opts, quota, MNTOPT_QUOTA,
					 MNTOPT_NOQUOTA);
			}

			replace_opts(mntp->mnt_opts, nomtab, MNTOPT_NOMTAB,
				     NULL);

			if (check_mounted_overlaps(check_state, mntp)) {
			    if (check_state)
				mnt_check_end(check_state);
			    exit(1);
			}

			if (check_state)
			    mnt_check_end(check_state);
		
			if (!strncmp(mntp->mnt_type, 
				     MNTTYPE_NFS, strlen(MNTTYPE_NFS))) {
				if (adjustnfsargs(NULL, mntp))
					exit(1);
			}

			exit(mounttree(maketree(NULL, mntp, NULL)));
		    }
		}
		(void) fprintf(stderr, "mount: %s not found in %s\n", argv[1],
		    altfstab ? altfstab : MNTTAB);
		exit(1);
	}

	if (argc != 3) {
		usage();
	}

	if (realpaths) {
		if (realpath(argv[2], dir) == NULL) {
			(void) fprintf(stderr, "mount: ");
			perror(dir);
			exit(1);
		}
	} else {
		(void) strcpy(dir, argv[2]);
	}
	(void) strcpy(name, argv[1]);

	/*
	 * If the file system type is not given then
	 * assume "nfs" if the name is of the form
	 *     host:path
	 */
	if (index(name, ':') && type[0] == '\0') {
		strcpy(type, MNTTYPE_NFS);
	}
	if (type[0] == '\0') {
		register short fstyp, nfstyp;
		struct statfs sfsb;

		/*
		 * Get filesystem type index and, from it, type name.
		 */
		nfstyp = sysfs(GETNFSTYP);
		for (fstyp = 1; fstyp < nfstyp; fstyp++) {
			if (statfs(name, &sfsb, sizeof sfsb, fstyp) == 0)
				break;
		}
		if (fstyp != sfsb.f_fstyp
		    || sysfs(GETFSTYP, fstyp, type) < 0) {
			perror(name);
			exit(1);
		}
	}
	if (dir[0] != '/') {
		(void) fprintf(stderr,
		    "mount: directory path must begin with '/'\n");
		exit(1);
	}

	if (opts[0] == '\0') {
		strcpy(opts, ro ? MNTOPT_RO : MNTOPT_RW);
	} else
		replace_opts(opts, ro, MNTOPT_RO, MNTOPT_RW);
	if (strcmp(type, MNTTYPE_EFS) == 0)
		replace_opts(opts, quota, MNTOPT_QUOTA, MNTOPT_NOQUOTA);
	replace_opts(opts, nomtab, MNTOPT_NOMTAB, NULL);

	mnt.mnt_opts = opts;

	if (strncmp(type, MNTTYPE_NFS, strlen(MNTTYPE_NFS)) == 0) {
		if (adjustnfsargs(type, &mnt))
			exit(1);
		passno = 0;
		freq = 0;
	} else {
		mnt.mnt_type = type;
	}
	mnt.mnt_fsname = name;
	mnt.mnt_dir = dir;
	mnt.mnt_opts = opts;
	mnt.mnt_freq = freq;
	mnt.mnt_passno = passno;

	/* read all volheaders and setup mounted partition checking */
	if (do_overlap_checks && (mnt_check_init(&check_state) == -1)) {
	        check_state = 0;
	        (void) fprintf(stderr, "mount: unable to init partition "
			       "checking routines, suspending checks\n");
	}

	if (check_mounted_overlaps(check_state, &mnt)) {
	        if (check_state)
		        mnt_check_end(check_state);
	        exit(1);
	}

	if (check_state)
	        mnt_check_end(check_state);

	exit(mounttree(maketree(NULL, &mnt, NULL)));
}

/* check_mounted_overlaps()
 *
 * See if the filesystem we're about to mount conflicts with
 * any other mounted overlapping partition or volumes.
 *
 * ARGUMENTS:
 *  check_state - valid pointer check_state_t from mnt_check_init()
 *  mntp - valid pointer to filled-in mountent structure
 *
 * RETURN VALUE:
 *  0 - no conflicts
 *  1 - overlapping partition conflict or partition already mounted/claimed
 */
int
check_mounted_overlaps(mnt_check_state_t *check_state, struct mntent *mntp)
{
    int retval;

    if (!check_state)
	    return 0;

    /* only check local efs & xfs filesystems */
    if (strcmp(mntp->mnt_type, MNTTYPE_XFS)
	&& strcmp(mntp->mnt_type, MNTTYPE_EFS))
	    return 0;

    retval = mnt_check_and_mark_mounted(check_state, mntp->mnt_fsname);

    if (retval == -1) {
	    if (mnt_causes_test(check_state, MNT_CAUSE_MOUNTED)) {
		(void) fprintf(stderr, "mount: %s is already mounted.\n",
			       mntp->mnt_fsname);
	    } else if (mnt_causes_test(check_state, MNT_CAUSE_OVERLAP)) {
		(void) fprintf(stderr, "mount: %s overlaps a mounted partition.\n", mntp->mnt_fsname);
	    } else {
		mnt_causes_show(check_state, stderr, "mount");
	    }
	    (void) fprintf(stderr, "\n");
	    (void) fflush(stderr);
	    mnt_plist_show(check_state, stderr, "mount");
	    (void) fprintf(stderr, "\n");
	    return 1;
    }

    /* let mount handle stat errors etc */
    return 0;
}


/*
 * Make sure that /etc and /usr/etc are in the
 * path before execing user supplied mount program.
 * We set the path to _PATH_ROOTPATH so no security problems can creep in.
 */
void
fixpath()
{
	char *newpath;
	static char prefix[] = "PATH=";

	/*
	 * Allocate enough space for the path and the trailing null.
	 */
	newpath = malloc(strlen(prefix) + strlen(_PATH_ROOTPATH) + 1);
	strcpy(newpath, prefix);
	strcat(newpath, _PATH_ROOTPATH);
	putenv(newpath);
}

/*
 * attempt to mount file system, return errno or 0
 */
mountfs(int print, struct mntent *mnt)
{
	pid_t waitpid;
	int error;
	int type = -1;
	int flags = 0;
	union data {
		struct nfs_args nfs_args;
		struct efs_args efs_args;
		struct xfs_args xfs_args;
#ifdef PCFS
		struct pc_args pc_args;
#endif
	} data;
	char *fsname = mnt->mnt_fsname;
	int vhfd;
	char *eagopt;
	struct stat statb;
	char opts[MNTMAXSTR];
	cap_t ocap;
	cap_value_t mount_caps[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT,
				    CAP_MAC_READ, CAP_DAC_READ_SEARCH};

	if (eagopt = hasmntopt(mnt, MNTOPT_EAG)) {
		char *cp;
		eagopt = strdup(eagopt);
		if (cp = strchr(eagopt, ','))
			*cp = '\0';
		if (cp = strchr(eagopt, ' '))
			*cp = '\0';
		if (cp = strchr(eagopt, '\t'))
			*cp = '\0';
	}

	if (hasmntopt(mnt, MNTOPT_REMOUNT) == 0) {
		if (mounted(mnt)) {
			if (print && verbose) {
				struct ustat us;
				struct stat sb;

				/*
				 * only works when fsname is a device, so we
				 * skip NFS and DBG, but better than nothing:
				 * the check for a block device is so we won't
				 * get incorrect messages with non EFS fs's
				 */
				if (stat(mnt->mnt_fsname, &sb) == 0
				    && (sb.st_mode & S_IFBLK) == S_IFBLK
				    && ustat(sb.st_rdev, &us) < 0) {
					fprintf(stderr,
				"mount: %s in mount table, but not mounted\n",
						mnt->mnt_fsname);
				} else {
					(void) fprintf(stderr,
						"mount: %s already mounted\n",
						mnt->mnt_fsname);
				}
			}
			return (0);
		}
	} else if (print && verbose)
		(void) fprintf (stderr, "mountfs: remount ignoring mtab\n");
	if (fake) {
		addtomtab(mnt);
		return (0);
	}
	if (strcmp(mnt->mnt_type, MNTTYPE_EFS) == 0) {
		flags |= MS_DATA;
		error = mount_efs(mnt, &data.efs_args);
	} else if (strcmp(mnt->mnt_type, MNTTYPE_XFS) == 0) {
		flags |= MS_DATA;
		if (hasmntopt(mnt, MNTOPT_DMI))
			flags |= MS_DMI;
		error = mount_xfs(mnt, &data.xfs_args);
	} else if (strcmp(mnt->mnt_type, MNTTYPE_DBG) == 0) {
		flags |= MS_FSS;
		error = 0;
	} else if (strcmp(mnt->mnt_type, MNTTYPE_NFS2) == 0) {
		struct mntent tmpmnt;

		tmpmnt = *mnt;
		tmpmnt.mnt_type = MNTTYPE_NFS;
		flags |= MS_DATA;
		error = mount_nfs(&tmpmnt, &data.nfs_args, eagopt,
		    MOUNTVERS, MOUNTVERS_SGI_ORIG);
		if (!error) 
			strcpy(mnt->mnt_type,tmpmnt.mnt_type);
		/*
		 * Gross hack for AT&T 5.3.1 mount: coerce fsname to dir
		 * so that the mount syscall can lookup inodes for both
		 * fsname and dir.  XXX change mount(2) to treat fsname
		 * as a filesystem-dependent parameter.
		 */
		fsname = mnt->mnt_dir;
#ifdef PCFS
	} else if (strcmp(mnt->mnt_type, MNTTYPE_PC) == 0) {
		error = mount_pc(mnt, &data.pc_args);
#endif
	} else if (strcmp(mnt->mnt_type, MNTTYPE_NFS3) == 0) {
		flags |= MS_DATA;
		if (sysfs(GETFSIND, mnt->mnt_type) < 0) {
			error = errno;
		} else {
			error = mount_nfs(mnt, &data.nfs_args, eagopt,
			    MOUNTVERS3, MOUNTVERS_SGI_ORIG_3);
		}
		fsname = mnt->mnt_dir;
	} else if (strcmp(mnt->mnt_type, MNTTYPE_NFS3_PREF) == 0) {
		struct mntent tmpmnt;

		tmpmnt = *mnt;
		tmpmnt.mnt_type = MNTTYPE_NFS;
		
		flags |= MS_DATA;

		/* Make sure V3 is installed */
		tmpmnt.mnt_type = MNTTYPE_NFS3;
		type = sysfs(GETFSIND, tmpmnt.mnt_type);
		if (type > 0) {
			/* Try to V3 mount, otherwise just fall back to V2 */
			
			error = mount_nfs(&tmpmnt, &data.nfs_args, eagopt,
			    MOUNTVERS3, MOUNTVERS_SGI_ORIG_3);
			if (!error) 
				strcpy(mnt->mnt_type,tmpmnt.mnt_type);
			else 
				printf("NFS version 3 mount failed, trying NFS version 2\n");
		} else
			error = 0;
		if (error || (type <= 0)) {
			tmpmnt.mnt_type = MNTTYPE_NFS;
			error = mount_nfs(&tmpmnt, &data.nfs_args, eagopt,
			    MOUNTVERS, MOUNTVERS_SGI_ORIG);
			if (!error) 
				strcpy(mnt->mnt_type,tmpmnt.mnt_type);
		}
		fsname = mnt->mnt_dir;
	} else if (strcmp(mnt->mnt_type, MNTTYPE_FD) == 0) {
		flags |= MS_DATA;
		error = 0;
        } else if (strcmp(mnt->mnt_type, MNTTYPE_HWGFS) == 0) {
		flags |= MS_DATA;
		error = 0;
	} else {
		/*
		 * Invoke "mount" command for particular file system type.
		 * Use this for unknown or third-party file system types.
		 */
		char mountcmd[128];
		int pid;
		int status;

		pid = fork();
		switch (pid) {

		case -1:
			(void) fprintf(stderr,
			    "mount: %s on ", mnt->mnt_fsname);
			perror(mnt->mnt_dir);
			return (errno);
		case 0:
			fixpath();
			(void) sprintf(mountcmd, "mount_%s", mnt->mnt_type);
			execlp(mountcmd, mountcmd, mnt->mnt_fsname,
			    mnt->mnt_dir, mnt->mnt_type, mnt->mnt_opts,
			    (char *)NULL);
			
			/* 
			 * If the mount_* command was not found, let the UMFS
			 * mechanism check if the type is a registered User
			 * Mode filesystem.
			 */
			if (errno == ENOENT) {
			  execlp("mount_umfs", "mount_umfs", mnt->mnt_fsname,
			      mnt->mnt_dir, mnt->mnt_type, mnt->mnt_opts,
			      (char *)NULL);
			}
			
			/*
			 * UMFS is not installed, punt in the usual manner
			 */
			if (errno == ENOENT) {
				(void) fprintf(stderr,
				    "mount: unknown filesystem type: %s\n",
				    mnt->mnt_type);
			} else if (print) {
				(void) fprintf(stderr, "%s: %s on %s ",
				    mountcmd, mnt->mnt_fsname,
				    mnt->mnt_dir); perror("exec failed");
			}
			exit(errno);
		default:
			do {
				waitpid = wait(&status);
				if (waitpid == -1) {
					return(waitpid);
				}
			} while (waitpid != pid);
			if (WIFEXITED(status)) {
				error = WEXITSTATUS(status);
				if (!error) {
					goto itworked;
				}
			} else {
				error = status;
			}
		}
	}
	if (error) {
		return (error);
	}

	type = sysfs(GETFSIND, mnt->mnt_type);
	if (type > 0)
		flags |= MS_FSS;
	else {
		perror(mnt->mnt_type);
		exit(errno);
	}
		
	if (hasmntopt(mnt, MNTOPT_RO))
		flags |= MS_RDONLY;
	if (hasmntopt(mnt, MNTOPT_GRPID))
		flags |= MS_GRPID;
	if (hasmntopt(mnt, MNTOPT_NOSUID))
		flags |= MS_NOSUID;
	if (hasmntopt(mnt, MNTOPT_NODEV))
		flags |= MS_NODEV;
	/* jws - This needs to be set by default */
	flags |= MS_DEFXATTR;
	if (hasmntopt(mnt, MNTOPT_NODEFXATTR))
		flags &= ~(MS_DEFXATTR);
	if (hasmntopt(mnt, MNTOPT_DOXATTR))
		flags |= MS_DOXATTR;

	/*
	 * Set the blocksize of the device to the blocksize
	 * specified in the volume header. This is necessary 
	 * when mounting efs CDROM on the 12x Toshiba (and 
	 * possibly newer) drives.  It also SHOULD handle 
	 * future devices that support larger than 512 byte blocks.
	 *
	 * Specifically coded the routines to not check return
	 * status for errors as we may be able to mount the
	 * device even if we can't set or detect the block size.
	 */

	vhfd = disk_openvh (fsname);
	if (vhfd >= 0) {
		{
			struct volume_header vhdr;
			if (getdiskheader(vhfd,&vhdr) == 0)	/* okay */
				(void)disk_setblksz(vhfd,vhdr.vh_dp.dp_secbytes);
		}
		close(vhfd);
	}

	/*
	 * Use sgi_eag_mount instead of mount if the Plan G
	 * attribute extension mechanism is required by the mount
	 * ("eag:....") options.
	 */

retry:
	ocap = cap_acquire (4, mount_caps);
	if (eagopt || mac_enabled)
		error = sgi_eag_mount(fsname, mnt->mnt_dir, flags, type,
		    &data, sizeof data, eagopt);
	else
		error = mount(fsname, mnt->mnt_dir, flags, type,
		    &data, sizeof data);
	cap_surrender (ocap);
	
	if (error < 0) {
		error = errno;

		/*
		 * Hack for NFS over TCP
		 */
		if ((strncmp(mnt->mnt_type, MNTTYPE_NFS, 
		     	     strlen(MNTTYPE_NFS)) == 0) &&
		    (data.nfs_args.flags & NFSMNT_TCP) && 
		    (errno == ECONNREFUSED)) {
			data.nfs_args.flags &= ~NFSMNT_TCP;
			/*
			 * Fix up options; relies on the fact that tcp and udp
			 * are the same length
			 */
			{
				char *p = strstr(mnt->mnt_opts, "proto=");
				if (p) {
					while (*p != '=') {
						p++;
					}
					p++;
					bcopy("udp", p, 3);
				}
			}
			
			if (print) {
				fprintf(stderr, 
					"mount: mount of %s on %s, %s: TCP unavailable, trying UDP\n", 
					mnt->mnt_fsname, mnt->mnt_dir,
					mnt->mnt_type);
			}
			goto retry;
		}
		if (print) {
			fprintf(stderr, "mount: %s on ", mnt->mnt_fsname);
			if (error == ENOSPC) {
				if ((flags & MS_RDONLY) &&
					strncmp(mnt->mnt_type, MNTTYPE_XFS, 
					    strlen(MNTTYPE_XFS)) == 0) {
					fprintf(stderr, "%s: Dirty XFS filesystem.  Cannot be mounted read-only.\n\tMount read/write, unmount, and try again.\n",
						mnt->mnt_dir);
				} else {
					fprintf(stderr,
						"%s: Dirty filesystem.\n",
						mnt->mnt_dir);
				}
			} else if (error == E2BIG) {
				fprintf(stderr,
				"%s: Filesystem too large for device.\n",
					mnt->mnt_dir);
			} else if (error == EWRONGFS) {
				fprintf(stderr,
					"%s: Wrong filesystem type %s\n",
					mnt->mnt_dir, mnt->mnt_type);
			} else {
				errno = error;
				perror(mnt->mnt_dir);
			}
		}
		return (error);
	}

itworked:
	if (*mnt->mnt_opts == '\0')			/* XXX ??? */
		(void) strcpy(mnt->mnt_opts, MNTOPT_RW);
	
	/*
	 * normalize NFS mount output to look like
	 * we want all nfs output to look like this
	 * blah:blah on /blah type nfs (vers=n,....)
	 * at this point MNTTYPE_NFS means nfs v2,
	 * and MNTTYPE_NFS3 means v3. All others were
	 * factored out above.
	 */

	if ((strcmp(mnt->mnt_type, MNTTYPE_NFS3) == 0)
	    && (stat(mnt->mnt_dir, &statb) == 0)) {
		char *fmtstr;
	        /* Note: printmtab assumes dev is last */
		if (ismntopt(mnt, MNTOPT_VERS) == NULL)
			fmtstr = "vers=3,%s,dev=%04x";
		else
			fmtstr = "%s,dev=%04x";
		sprintf(opts, fmtstr, mnt->mnt_opts, statb.st_dev);
		mnt->mnt_opts = opts;
		mnt->mnt_type = MNTTYPE_NFS;
	}
	else if ((strcmp(mnt->mnt_type, MNTTYPE_NFS) == 0)
	    && (stat(mnt->mnt_dir, &statb) == 0)) {
		char *fmtstr;
	        /* Note: printmtab assumes dev is last */
		if (ismntopt(mnt, MNTOPT_VERS) == NULL)
			fmtstr = "vers=2,%s,dev=%04x";
		else
			fmtstr = "%s,dev=%04x";
		sprintf(opts, fmtstr, mnt->mnt_opts, statb.st_dev);
		mnt->mnt_opts = opts;
	}
	addtomtab(mnt);
	return (0);
}	/* mountfs */

mount_efs(struct mntent *mnt, struct efs_args *args)
{
	int status;
	char cmd[MNTMAXSTR];
	char *nametocheck;
	char *sep = NULL;

	args->flags = 0;

	if (nopt(mnt, "lbsize", &args->lbsize))
		args->flags |= EFSMNT_LBSIZE;

	if (!check)
		return 0;

	sprintf(cmd, "2>&1 /sbin/fsstat %s > /dev/null", mnt->mnt_fsname);
	status = system(cmd);
	if (status >> 8 != 1) {
		return 0;	/* filesystem clean or invalid */
	}
	if (strcmp(mnt->mnt_fsname, "/")
	    && (nametocheck = hasmntopt(mnt, MNTOPT_RAW)) != NULL) {
		/* 
		 * Ignore the next option field for now, but we'll
		 * have to restore the separator for later hasmntopt calls.
		 */
		if (sep = strchr(nametocheck, ','))
			*sep = '\0';

		if ((nametocheck = strchr(nametocheck, '=')) != NULL)
			nametocheck++;	/* advance to option rhs */
	} else {
		/* Nonexistent or malformed raw spec in fstab */
		nametocheck = mnt->mnt_fsname;
	}
	sprintf(cmd, "/sbin/fsck -y %s", nametocheck);
	if (sep)
		*sep = MNTOPTSEP;
	status = system(cmd);
	return (status >> 8);
}	/* mount_efs */

mount_xfs(struct mntent *mnt, struct xfs_args *args)
{
	char *slogbufs, *slogbufsize, *sep, *sunit, *swidth, *biosize;
	int  logbufs = -1;
	int  logbufsize = -1;
	int dsunit, dswidth, xlv_dsunit, xlv_dswidth;
	int iosize;

	iosize = dsunit = dswidth = xlv_dsunit = xlv_dswidth = 0;
	args->version = 3;
	args->flags = 0;
	args->fsname = malloc(PATH_MAX);	/* OK, so I don't free it */
	strcpy(args->fsname, mnt->mnt_dir);
	if ((slogbufs = hasmntopt(mnt, MNTOPT_LOGBUFS)) != NULL) {
		/*
		 * Ignore the next option field for now, but we'll
		 * have to restore the separator for later hasmntopt calls.
		 */
		if (sep = strchr(slogbufs, ','))
			*sep = '\0';

		if ((slogbufs = strchr(slogbufs, '=')) != NULL) {
			slogbufs++;		/* advance to option rhs */
			if (!strcmp(slogbufs, "none")) {
				logbufs = 0;
				fprintf(stderr,
			    "mount: this FS is trash after writing to it\n");
			} else {
				logbufs = atoi(slogbufs);
				if (logbufs < 2 || logbufs > 8) {
					fprintf(stderr,
					"mount: Illegal logbufs amount: %d\n",
						logbufs);
					if (sep)
						*sep = MNTOPTSEP;
					return 1;
				}
			}
		}
		if (sep)
			*sep = MNTOPTSEP;
	}
	if ((slogbufsize = hasmntopt(mnt, MNTOPT_LOGBSIZE)) != NULL) {
		/*
		 * Ignore the next option field for now, but we'll
		 * have to restore the separator for later hasmntopt calls.
		 */
		if (sep = strchr(slogbufsize, ','))
			*sep = '\0';

		if ((slogbufsize = strchr(slogbufsize, '=')) != NULL) {
			slogbufsize++;		/* advance to option rhs */
			logbufsize = atoi(slogbufsize);
			if (logbufsize != 16*1024 && logbufsize != 32*1024) {
				fprintf(stderr,
			"mount: Illegal logbufsize: %d (x == 16k or 32k)\n",
						logbufsize);
					if (sep)
						*sep = MNTOPTSEP;
				return 1;
			}
			if (logbufsize % 512 != 0) {
				fprintf(stderr,
			"mount: logbufsize must be multiple of BBSIZE: %d",
					logbufsize);
				if (sep)
					*sep = MNTOPTSEP;
				return 1;
			}
		}
		if (sep)
			*sep = MNTOPTSEP;
	}
	if ((biosize = hasmntopt(mnt, MNTOPT_BIOSIZE)) != NULL) {
		/*
		 * Ensure we have a null-terminated string to work with but
		 * we have to restore the separator for later hasmntopt calls.
		 */
		if (sep = strchr(biosize, ','))
			*sep = '\0';
		if ((biosize = strchr(biosize, '=')) != NULL) {
			biosize++;		/* advance to option rhs */
			iosize = atoi(biosize);
			if (iosize > 255 || iosize <= 0) {
				fprintf(stderr,
			"mount: illegal biosize %d, value out of bounds\n",
						iosize);
				if (sep)
					*sep = MNTOPTSEP;
				return 1;
			}
			args->flags |= XFSMNT_IOSIZE;
			args->iosizelog = (uint8_t) iosize;
		}
		if (sep)
			*sep = MNTOPTSEP;
	}
	/*
	 * Pass in the wsync option as a flag since it requires
	 * no argument.
	 */
	if (hasmntopt(mnt, MNTOPT_WSYNC) != NULL) {
		args->flags |= XFSMNT_WSYNC;
	}
	if (hasmntopt(mnt, MNTOPT_NOATIME) != NULL) {
		args->flags |= XFSMNT_NOATIME;
	}
	if (hasmntopt(mnt, MNTOPT_OSYNCISDSYNC) != NULL) {
		args->flags |= XFSMNT_OSYNCISDSYNC;
	}
	if (hasmntopt(mnt, MNTOPT_NORECOVERY) != NULL) {
		args->flags |= XFSMNT_NORECOVERY;
		if (hasmntopt(mnt, MNTOPT_RO) == NULL) {
			fprintf(stderr,
			"mount:  no-recovery XFS mounts must be read-only.\n");
			return 1;
		}
	}
	if (hasmntopt(mnt, MNTOPT_SHARED) != NULL) {
		args->flags |= XFSMNT_SHARED;
		if (hasmntopt(mnt, MNTOPT_RO) == NULL) {
			fprintf(stderr,
			"mount:  shared XFS mounts must be read-only.\n");
			return 1;
		}
	}

	/*
	 * Pass in ino64 option as a flag.
	 */
	if (hasmntopt(mnt, MNTOPT_INO64) != NULL) {
#ifdef XFS_BIG
		args->flags |= XFSMNT_INO64;
#else
		fprintf(stderr, "mount: ino64 option not allowed on this system\n");
		return 1;
#endif
	}
	/*
	 * Pass in the quota options.
	 */
	if (hasmntopt(mnt, MNTOPT_UQUOTA) != NULL ||
	    hasmntopt(mnt, MNTOPT_QUOTA) != NULL) {
		/* user quota accounting and limits enforcement */
		args->flags |= XFSMNT_UQUOTA | XFSMNT_UQUOTAENF;
	}
	/* 
         * Don't enforce user quota limits, if asked not to.
         * _*QUOTANOENF does _not_ depend on _*QUOTA option. 
         */
	if (hasmntopt(mnt, MNTOPT_UQUOTANOENF) != NULL ||
	    hasmntopt(mnt, MNTOPT_QUOTANOENF) != NULL) {
		args->flags |= XFSMNT_UQUOTA;
		args->flags &= ~XFSMNT_UQUOTAENF;
	}
	/*
	 * Typically, we turn quotas off if we weren't explicitly asked to mount 
	 * quotas. MNTOPT_MRQUOTA tells us not to do that.
	 * This option is handy in the miniroot, when trying to mount /root.
	 * We can't really know what's in /etc/fstab until /root is already 
	 * mounted! This mntopt stops quotas from getting turned off in the root 
	 * filesystem everytime the system boots up a miniroot.
	 */
	if (hasmntopt(mnt, MNTOPT_MRQUOTA) != NULL) {
		args->flags |= XFSMNT_QUOTAMAYBE;
	}

	/* 
	 * Used to turn off stripe alignment
	 */
	if (hasmntopt(mnt, MNTOPT_NOALIGN) != NULL) 
		args->flags |= XFSMNT_NOALIGN;

	/*
	 * Used to specify the stripe unit for xlv stripe volumes or raid disks 
	 */
	if ((sunit = hasmntopt(mnt, MNTOPT_SUNIT)) != NULL) {
		/*
		 * Ignore the next option field for now, but we'll
		 * have to restore the separator for later hasmntopt calls.
		 */
		if (sep = strchr(sunit, ','))
			*sep = '\0';
		if ((sunit = strchr(sunit, '=')) != NULL) {
			sunit++;
			dsunit = atoi(sunit);
		}
		if (sep)
			*sep = MNTOPTSEP;
	}

	/*
	 * Used to specify the stripe width for xlv stripe volumes or raid disks
	 */
	if ((swidth = hasmntopt(mnt, MNTOPT_SWIDTH)) != NULL) {
		/*
		 * Ignore the next option field for now, but we'll
		 * have to restore the separator for later hasmntopt calls.
		 */
		if (sep = strchr(swidth, ','))
			*sep = '\0';
		if ((swidth = strchr(swidth, '=')) != NULL) { 
			swidth++;
			dswidth = atoi(swidth);
		}
		if (sep)
			*sep = MNTOPTSEP;
	}	

	if ((args->flags & XFSMNT_NOALIGN) && (dsunit || dswidth)) {
		fprintf(stderr,
"mount: sunit and swidth options are incompatible with the noalign option\n");
		return 1;
	}

	if (dsunit && !dswidth || !dsunit && dswidth) {
		fprintf(stderr,
"mount: both sunit and swidth options have to be specified\n");
		return 1;
	}

	if (dsunit && (dswidth % dsunit != 0)) {
		fprintf(stderr,
"mount: stripe width (%d) has to be a multiple of the stripe unit (%d)\n",
			dswidth, dsunit);
		return 1;
	}

	if ((args->flags & XFSMNT_NOALIGN) != XFSMNT_NOALIGN) {
		/*
       	 	 * Extract the stripe width and stripe unit info from the 
		 * underlying volume if applicable.
	 	 */
        	xlv_get_subvol_stripe(mnt->mnt_fsname, XLV_SUBVOL_TYPE_DATA, 
			&xlv_dsunit, &xlv_dswidth);

		if (dsunit && xlv_dsunit && xlv_dsunit != dsunit) {
			fprintf(stderr, 
"mount: Specified data stripe unit (%d) is not the same as the xlv stripe unit (%d)\n",
				dsunit, xlv_dsunit);	
			return 1;
		}

		if (dswidth && xlv_dswidth && xlv_dswidth != dswidth) {
			fprintf(stderr,
"mount: Specified data stripe width (%d) is not the same as the xlv stripe width (%d)\n",
				dswidth, xlv_dswidth);
			return 1;
		}

		if (dsunit) { 
			args->sunit = dsunit;
			args->flags |= XFSMNT_RETERR;
		} else 
			args->sunit = xlv_dsunit;	
		dswidth ? (args->swidth = dswidth) : 
			  (args->swidth = xlv_dswidth);
	} else 
		args->sunit = args->swidth = 0;

	args->logbufs = logbufs;
	args->logbufsize = logbufsize;
	return 0;
}	/* mount_xfs */

nameinlist(char *name, char *list)
{
	char copy[BUFSIZ];
	char *element;

	list = strcpy(copy, list);
	while ((element = strtok(list, ",")) != 0) {
		if (!strcmp(element, name))
			return 1;
		if (!strcmp(element, MNTTYPE_NFS) &&
		    !strncmp(name,element,3) )
			return 1;
		list = 0;
	}
	return 0;
}

/*
 * RPC timeouts in seconds: per retransmission and total for a call.
 * Set to a few seconds for the first (foreground) try if the "bg" option
 * is specified, and reset after backgrounding to longer values.
 */
long		mount_intertry, mount_percall;
struct timeval	pmap_intertry, pmap_percall;

void
set_short_timeouts()
{
	mount_intertry = 5;
	mount_percall = 5;
	pmap_intertry.tv_sec = 5;
	pmap_percall.tv_sec = 5;
	pmap_settimeouts(pmap_intertry, pmap_percall);
}

void
set_long_timeouts()
{
	mount_intertry = 20;
	mount_percall = 20;
	pmap_intertry.tv_sec = 12;
	pmap_percall.tv_sec = 60;
	pmap_settimeouts(pmap_intertry, pmap_percall);
}

mount_nfs(struct mntent *mnt, struct nfs_args *args, char *eagopt,
		int vers, int sgi_vers)
{
	static struct sockaddr_in sin;
	struct hostent *hp;
	static struct fhstatus fhs;
	static struct mountres3 mountres3;
	static nfs_fh3 fh3;
	char *cp;
	char *host, *hostp;
	char *path;
	char *eag_mac_ip = NULL;
	mac_t mac_ip = NULL, save_plabel = NULL;
	char *proto;
	int s;
	struct timeval timeout;
	CLIENT *client;
	enum clnt_stat rpc_stat;
	u_int port;
	u_long prog;
	char *msg;
	cap_t ocap;
	cap_value_t cap_mac_relabel_subj[] = {CAP_MAC_RELABEL_SUBJ};

	if (mac_enabled && eagopt) {
		if (eag_mac_ip = strstr(eagopt, MAC_MOUNT_IP)) {
			eagopt = strdup(eag_mac_ip);
			eag_mac_ip = eagopt + strlen(MAC_MOUNT_IP) + 1;
			if (cp = strchr(eag_mac_ip, ':'))
				*cp = '\0';
			if (cp = strchr(eag_mac_ip, ','))
				*cp = '\0';
			if (cp = strchr(eag_mac_ip, ' '))
				*cp = '\0';
			if (cp = strchr(eag_mac_ip, '\t'))
				*cp = '\0';
			mac_ip = mac_from_text(eag_mac_ip);
			free(eagopt);
			if (mac_ip == NULL) {
				(void) fprintf(stderr,
				    "mount: %s:%s invalid label.\n",
				    MAC_MOUNT_IP, eag_mac_ip);
				return (EINVAL);
			}
		}
	}

	hostp = host = malloc(MNTMAXSTR);
	cp = mnt->mnt_fsname;
	while ((*hostp = *cp) != ':') {
		if (*cp == '\0') {
			(void) fprintf(stderr,
			    "mount: nfs file system; use host:path\n");
			return (EINVAL);
		}
		hostp++;
		cp++;
	}
	*hostp = '\0';
	path = ++cp;
	/*
	 * Get server's address
	 */
	if ((hp = gethostbyname(host)) == NULL) {
		(void) fprintf(stderr,
		    		"mount: %s not in hosts database\n", host);
		return (ENOENT);
	}

	args->flags = 0;
	if (ismntopt(mnt, MNTOPT_SOFT)) {
		args->flags |= NFSMNT_SOFT;
	}
	/*
	 * Make hard mounts interruptible by default.
	 */
	if (ismntopt(mnt, MNTOPT_NOINTR)) {
		args->flags |= NFSMNT_NOINT;
	}
	if (ismntopt(mnt, MNTOPT_NOAC)) {
		args->flags |= NFSMNT_NOAC;
	}
	if (ismntopt(mnt, MNTOPT_PRIVATE)) {
		args->flags |= NFSMNT_PRIVATE;
	}
	if (ismntopt(mnt, MNTOPT_SHORTUID)) {
		args->flags |= NFSMNT_SHORTUID;
	}
	if (nopt(mnt, MNTOPT_SYMTTL, &args->symttl)) {
		args->flags |= NFSMNT_SYMTTL;
	}

	/*
	 * get fhandle of remote path from server's mountd
	 */
	bzero((char *)&sin, sizeof(sin));
	bcopy(hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
	sin.sin_family = AF_INET;
	timeout.tv_usec = 0;
	timeout.tv_sec = mount_intertry;
	s = RPC_ANYSOCK;

	if (ismntopt(mnt, "bds")) {
		args->flags |= NFSMNT_BDS;
		if (nopt(mnt, "bdsauto", &args->bdsauto)) {
			args->flags |= NFSMNT_BDSAUTO;
		}
		if (nopt(mnt, "bdswindow", &args->bdswindow)) {
			args->flags |= NFSMNT_BDSWINDOW;
		}
		if (nopt(mnt, "bdsbuffer", &args->bdsbuflen)) {
			args->flags |= NFSMNT_BDSBUFLEN;
		}
	}

	if (mac_ip) {
		save_plabel = mac_get_proc ();
		ocap = cap_acquire (1, cap_mac_relabel_subj);
		(void) mac_set_proc (mac_ip);
		cap_surrender (ocap);
	}
	/*
	 * Try and get the SGI private mount program which has
	 * a statvfs procedure.  The clntudp_create function doesn't
	 * enforce the version, but remembers it in the client handle.
	 * Therefore, on create we ask for the lowest version number
	 * and then depend on rpc.mountd to respond with RPC_PROCUNAVAIL
	 * if it is the older version.
	 */
	if ((client = clntudp_create(&sin, prog = (u_long)MOUNTPROG_SGI,
			(u_long)sgi_vers, timeout, &s)) == NULL &&
	    (rpc_createerr.cf_stat == RPC_PMAPFAILURE ||
	    (client = clntudp_create(&sin, prog = (u_long)MOUNTPROG,
	    (u_long)vers, timeout, &s)) == NULL)) {
		if (mac_ip) {
			ocap = cap_acquire (1, cap_mac_relabel_subj);
			(void) mac_set_proc (save_plabel);
			cap_surrender (ocap);
		}
		if (!printed) {
			(void) fprintf(stderr,
			    "mount: %s server not responding",
			    mnt->mnt_fsname);
			clnt_pcreateerror("");
			printed = 1;
		}
		return (ETIMEDOUT);
	}
callmount:
	client->cl_auth = authunix_create_default();
	timeout.tv_usec = 0;
	timeout.tv_sec = mount_percall;
	if (vers == MOUNTVERS) {
	    rpc_stat = clnt_call(client, MOUNTPROC_MNT, xdr_path, &path,
		xdr_fhstatus, &fhs, timeout);
	} else {
	    rpc_stat = clnt_call(client, MOUNTPROC_MNT, xdr_path, &path,
		xdr_mountres3, (caddr_t)&mountres3, timeout);
	}
	errno = 0;
	if (rpc_stat != RPC_SUCCESS) {
		/*
		 * Some RPC implementations (like the one written
		 * in Lisp on Symbolics machines) don't check the
		 * program number's validity until it is actually
		 * used.  So if the we fail here with MOUNTPROG_SGI,
		 * then back off to the standard mount program.
		 */
		if (prog == MOUNTPROG_SGI) {
			CLIENT *nclient;

			bzero((char *)&sin, sizeof(sin));
			bcopy(hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
			sin.sin_family = AF_INET;
			timeout.tv_usec = 0;
			timeout.tv_sec = mount_intertry;
			s = RPC_ANYSOCK;

			if (nclient = clntudp_create(&sin, prog = (u_long)MOUNTPROG,
						(u_long)vers, timeout, &s)) {
				clnt_destroy(client);
				client = nclient;
				goto callmount;
			}
		}
		if (!printed) {
			(void) fprintf(stderr,
			    "mount: %s server not responding",
			    mnt->mnt_fsname);
			clnt_perror(client, "");
			printed = 1;
		}
		switch (rpc_stat) {
		case RPC_TIMEDOUT:
		case RPC_PMAPFAILURE:
		case RPC_PROGNOTREGISTERED:
			errno = ETIMEDOUT;
			break;
		case RPC_AUTHERROR:
			errno = EACCES;
			break;
		default:
			errno = ERPC;
			break;
		}
	}
	if (errno) {
		clnt_destroy(client);
		if (mac_ip) {
			ocap = cap_acquire (1, cap_mac_relabel_subj);
			(void) mac_set_proc(save_plabel);
			cap_surrender (ocap);
			mac_free(save_plabel);
			mac_free(mac_ip);
		}
		return(errno);
	}
	if (vers == MOUNTVERS3) {
	    /*
	     * Assume here that most of the MNT3ERR_*
	     * codes map into E* errors.
	     */
	    if ((errno = mountres3.fhs_status) != MNT_OK) {
		switch (errno) {
		case MNT3ERR_NAMETOOLONG:
		    msg = "path name is too long";
		    break;
		case MNT3ERR_NOTSUPP:
		    msg = "operation not supported";
		    break;
		case MNT3ERR_SERVERFAULT:
		    msg = "server fault";
		    break;
		default:
		    msg = NULL;
		    break;
		}
		if (msg)
		    printf("can't mount %s:%s: ", host, path);
		else
		    perror("");
		return (EINVAL);
	    }
	}

	errno = (fhs.fhs_status == OLDENFSREMOTE) ? ENFSREMOTE : fhs.fhs_status;
	if (errno) {
		if (errno == EACCES) {
			(void) fprintf(stderr,
			    "mount: access denied for %s:%s\n", host, path);
		} else {
			(void) fprintf(stderr, "mount: ");
			perror(mnt->mnt_fsname);
		}
		clnt_destroy(client);
		if (mac_ip) {
			ocap = cap_acquire (1, cap_mac_relabel_subj);
			(void) mac_set_proc (save_plabel);
			cap_surrender (ocap);
			mac_free(save_plabel);
			mac_free(mac_ip);
		}
		return (errno);
	}

	/*
	 * Get statvfs from remote mount point to set static values
	 * that aren't supported by the nfs protocol.
	 */
	if (prog == MOUNTPROG_SGI) {
		struct mntrpc_statvfs mntstatvfs;

		rpc_stat = clnt_call(client, MOUNTPROC_STATVFS, xdr_path, &path,
					 xdr_statvfs, &mntstatvfs, timeout);
		if (rpc_stat == RPC_SUCCESS) {
			args->flags |= NFSMNT_NAMEMAX;
			args->namemax = mntstatvfs.f_namemax;
		}
	}
	clnt_destroy(client);
	if (mac_ip) {
		ocap = cap_acquire (1, cap_mac_relabel_subj);
		(void) mac_set_proc (save_plabel);
		cap_surrender (ocap);
		mac_free(save_plabel);
		mac_free(mac_ip);
	}

	if (printed) {
		(void) fprintf(stderr, "mount: %s server ok\n",
		    mnt->mnt_fsname);
		printed = 0;
	}

	/*
	 * set mount args
	 */
	if (vers == MOUNTVERS3) {
	    args->fh_len = mountres3.mountres3_u.mountinfo.fhandle.fhandle3_len;
	    args->fh =
		(fhandle_t *)mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val;
	} else {
	    args->fh = &fhs.fhs_fh;
	}
	args->hostname = host;
	args->flags |= NFSMNT_HOSTNAME;
	if (nopt(mnt, MNTOPT_RSIZE, &args->rsize)) { 
		args->flags |= NFSMNT_RSIZE;
	}
	if (nopt(mnt, MNTOPT_WSIZE, &args->wsize)) {
		args->flags |= NFSMNT_WSIZE;
	}
	if (nopt(mnt, MNTOPT_TIMEO, &args->timeo)) {
		args->flags |= NFSMNT_TIMEO;
	}
	if (nopt(mnt, MNTOPT_RETRANS, &args->retrans)) {
		args->flags |= NFSMNT_RETRANS;
	}
	if (args->flags & NFSMNT_PRIVATE) {
		args->acregmin = PRIV_ACREGMIN;
		args->acregmax = PRIV_ACREGMAX;
		args->acdirmin = PRIV_ACDIRMIN;
		args->acdirmax = PRIV_ACDIRMAX;
		args->flags |= NFSMNT_ACREGMIN | NFSMNT_ACREGMAX |
				NFSMNT_ACDIRMIN | NFSMNT_ACDIRMAX;
	}
	if (nopt(mnt, MNTOPT_ACTIMEO, &args->acregmin)) {
		args->acregmax = args->acregmin;
		args->acdirmin = args->acregmin;
		args->acdirmax = args->acregmin;
		args->flags |= NFSMNT_ACREGMIN | NFSMNT_ACREGMAX |
				NFSMNT_ACDIRMIN | NFSMNT_ACDIRMAX;
	} else {
		if (nopt(mnt, MNTOPT_ACREGMIN, &args->acregmin)) {
			args->flags |= NFSMNT_ACREGMIN;
		}
		if (nopt(mnt, MNTOPT_ACREGMAX, &args->acregmax)) {
			args->flags |= NFSMNT_ACREGMAX;
		}
		if (nopt(mnt, MNTOPT_ACDIRMIN, &args->acdirmin)) {
			args->flags |= NFSMNT_ACDIRMIN;
		}
		if (nopt(mnt, MNTOPT_ACDIRMAX, &args->acdirmax)) {
			args->flags |= NFSMNT_ACDIRMAX;
		}
	}
	if (nopt(mnt, MNTOPT_PORT, &port)) {
		sin.sin_port = htons(port);
	} else {
		sin.sin_port = htons(NFS_PORT);	/* XXX should use portmapper */
	}
	if (ismntopt(mnt, MNTOPT_ASYNCNLM)) {
		args->flags |= NFSMNT_ASYNCNLM;
	}

        proto = malloc(MNTMAXSTR);
        if (nonnopt(mnt, MNTOPT_PROTO, proto)) {
                if (!strncmp(proto,"tcp",3) || !strncmp(proto,"TCP",3)) {
                    args->flags |= NFSMNT_TCP;
#ifdef DEBUG
                    printf("Using TCP");
#endif
                } else if (!strncmp(proto,"udp",3) || !strncmp(proto,"UDP",3))
                    ;
                else {
                    (void) fprintf(stderr, "mount: unrecognized protocol %s\n",proto);
                    free(proto);
                    return(EINVAL);
                }
        }
        free(proto);

	args->addr = &sin;

	/*
	 * should clean up mnt ops to not contain defaults
	 */
	return (0);
}


#ifdef PCFS
mount_pc(struct mntent *mnt, struct pc_args *args)
{
	args->fspec = mnt->mnt_fsname;
	return (0);
}
#endif

void
printent(struct mntent *mnt)
{
	(void) fprintf(stdout, "%s on %s type %s (%s)\n",
	    mnt->mnt_fsname, mnt->mnt_dir, mnt->mnt_type, mnt->mnt_opts);
}

void
printmtab(FILE *outp)
{
	FILE *mnttab;
	struct mntent *mntp;
	int maxfsname = 0;
	int maxdir = 0;
	int maxtype = 0;

	/*
	 * first go through and find the max width of each field
	 */
	mnttab = setmntent(altmtab?altmtab:MOUNTED, "r");
	while ((mntp = getmntent(mnttab)) != NULL) {
		if (strcmp(mntp->mnt_type, MNTTYPE_IGNORE) == 0)
			continue;
		if (strlen(mntp->mnt_fsname) > maxfsname) {
			maxfsname = strlen(mntp->mnt_fsname);
		}
		if (strlen(mntp->mnt_dir) > maxdir) {
			maxdir = strlen(mntp->mnt_dir);
		}
		if (strlen(mntp->mnt_type) > maxtype) {
			maxtype = strlen(mntp->mnt_type);
		}
	}
	(void) endmntent(mnttab);

	/*
	 * now print them out in pretty format
	 */
	if (rbase) maxdir += strlen(rbase);
	mnttab = setmntent(altmtab?altmtab:MOUNTED, "r");
	while ((mntp = getmntent(mnttab)) != NULL) {
		if (strcmp(mntp->mnt_type, MNTTYPE_IGNORE) == 0)
			continue;
		if (strcmp(mntp->mnt_type, MNTTYPE_NFS) == 0) {
			char *cp;
			/* Note: assumes mountfs puts dev last */
			if ((cp = strstr(mntp->mnt_opts, ",dev")) != NULL)
				*cp = '\0';
		}

		(void) fprintf(outp, "%-*s", maxfsname+1, mntp->mnt_fsname);
		{
			char pathbuf[PATH_MAX];
			pathbuf[0] = 0;
			if (rbase && mntp->mnt_dir[0] == '/')
				(void) strcpy (pathbuf, rbase);
			if (!rbase || mntp->mnt_dir[1]) /* avoid "/root/" */
				(void) strcat (pathbuf, mntp->mnt_dir);
			(void) fprintf(outp, "%-*s", maxdir+1, pathbuf);
		}
		(void) fprintf(outp, "%-*s", maxtype+1, mntp->mnt_type);
		(void) fprintf(outp, "%-s  %d %d\n",
			mntp->mnt_opts, mntp->mnt_freq, mntp->mnt_passno);
	}
	(void) endmntent(mnttab);
}

/*
 * Check to see if mntck is already mounted.
 * We have to be careful because getmntent modifies its static struct.
 */
mounted(struct mntent *mntck)
{
	int found = 0;
	struct mntent *mnt, mntsave;
	FILE *mnttab;

	if (nomtab) {
		return (0);
	}
	mnttab = setmntent(MOUNTED, "r");
	if (mnttab == NULL) {
		(void) fprintf(stderr, "mount: ");
		perror(MOUNTED);
		exit(1);
	}
	mntcp(mntck, &mntsave);
	while ((mnt = getmntent(mnttab)) != NULL) {
		if (strcmp(mnt->mnt_type, MNTTYPE_IGNORE) == 0) {
			continue;
		}
		if ((strcmp(mntsave.mnt_fsname, mnt->mnt_fsname) == 0) &&
		    (strcmp(mntsave.mnt_dir, mnt->mnt_dir) == 0)) {
			found = 1;
			break;
		}
	}
	(void) endmntent(mnttab);
	*mntck = mntsave;
	return (found);
}

void
mntcp(struct mntent *mnt1, struct mntent *mnt2)
{
	static char fsname[128], dir[128], type[128], opts[128];

	mnt2->mnt_fsname = fsname;
	(void) strcpy(fsname, mnt1->mnt_fsname);
	mnt2->mnt_dir = dir;
	(void) strcpy(dir, mnt1->mnt_dir);
	mnt2->mnt_type = type;
	(void) strcpy(type, mnt1->mnt_type);
	mnt2->mnt_opts = opts;
	(void) strcpy(opts, mnt1->mnt_opts);
	mnt2->mnt_freq = mnt1->mnt_freq;
	mnt2->mnt_passno = mnt1->mnt_passno;
}

/*
 * Return the value of a non-numeric option of the form foo=string, if
 * option is not found or is malformed, return 0.
 */
nonnopt(struct mntent *mnt, char *opt, char *cpy)
{
        char *str;
        char *equal;

        str = ismntopt(mnt, opt);
        if (str == NULL)
                return (0);
        equal = str + strlen(opt);
        if (*equal != '=') {
                (void) fprintf(stderr,
                    "mount: missing value for option '%s'\n", str);
                return (0);
        }
        if (!sscanf(equal + 1, "%s", cpy)) {
                (void) fprintf(stderr,
                    "mount: illegal value for option '%s'\n", str);
                return (0);
        }
        return (1);
}

/*
 * Return the value of a numeric option of the form foo=x, if
 * option is not found or is malformed, return 0.
 */
nopt(struct mntent *mnt, char *opt, u_int *val)
{
	char *str;
	char *equal;

	str = ismntopt(mnt, opt);
	if (str == NULL)
		return (0);
	equal = str + strlen(opt);
	if (*equal != '=') {
		(void) fprintf(stderr,
		    "mount: missing value for option '%s'\n", str);
		return (0);
	}
	if (!sscanf(equal + 1, "%u", val)) {
		(void) fprintf(stderr,
		    "mount: illegal value for option '%s'\n", str);
		return (0);
	}
	return (1);
}

/*
 * update /etc/mtab
 */
void
addtomtab(struct mntent *mnt)
{
	FILE *mnted;
	struct mntent mget;
	struct mntent mref;
	int i;
	cap_t ocap;
	cap_value_t cap_mac_write = CAP_MAC_WRITE;

	if (nomtab) {
		return;
	}
	ocap = cap_acquire (1, &cap_mac_write);
	mnted = setmntent(MOUNTED, "r+");
	cap_surrender (ocap);
	if (mnted == NULL) {
		(void) fprintf(stderr, "mount: ");
		perror(MOUNTED);
		exit(1);
	}
	/*
	 * add the mount table entry if it is not already there
	 * use getmntany to check for the presence of a mount table entry
	 * check using only the mount point name and the FS type
	 */
	bzero( &mref, sizeof( struct mntent ) );
	mref.mnt_dir = mnt->mnt_dir;
	/*
	 * eliminate trailing blanks
	 */
	for (i = strlen(mnt->mnt_dir) - 1; (i >= 0) && (mnt->mnt_dir[i] == ' ');
		i--) {
			mnt->mnt_dir[i] = 0;
	}
	mref.mnt_type = mnt->mnt_type;
	if ((getmntany(mnted, &mget, &mref) == -1) && addmntent(mnted, mnt)) {
		(void) fprintf(stderr, "mount: ");
		perror(MOUNTED);
		exit(1);
	}
	(void) endmntent(mnted);

	if (verbose) {
		(void) fprintf(stdout, "%s mounted on %s\n",
		    mnt->mnt_fsname, mnt->mnt_dir);
	}
}

void *
xmalloc(int size)
{
	char *ret;

	if ((ret = (char *)malloc((unsigned)size)) == NULL) {
		(void) fprintf(stderr, "umount: ran out of memory!\n");
		exit(1);
	}
	return (ret);
}

struct mntent *
mntdup(struct mntent *mnt)
{
	struct mntent *new;

	new = (struct mntent *)xmalloc(sizeof(*new));

	new->mnt_fsname = (char *)xmalloc(strlen(mnt->mnt_fsname) + 1);
	(void) strcpy(new->mnt_fsname, mnt->mnt_fsname);

	new->mnt_dir = (char *)xmalloc(strlen(mnt->mnt_dir) + 1);
	(void) strcpy(new->mnt_dir, mnt->mnt_dir);

	new->mnt_type = (char *)xmalloc(strlen(mnt->mnt_type) + 1);
	(void) strcpy(new->mnt_type, mnt->mnt_type);

	new->mnt_opts = (char *)xmalloc(strlen(mnt->mnt_opts) + 1);
	(void) strcpy(new->mnt_opts, mnt->mnt_opts);

	new->mnt_freq = mnt->mnt_freq;
	new->mnt_passno = mnt->mnt_passno;

	return (new);
}

/*
 * Build the mount dependency tree.  All mount ordering dependencies are
 * expressed as parent child relationships in the tree.
 */
struct mnttree *
maketree(struct mnttree *mt, struct mntent *mnt, struct mnttree *nmt)
{
	struct mnttree	*lmt;
	struct mnttree	*next_mt;

	if (nmt == NULL) {
		nmt = (struct mnttree *)
			xmalloc(sizeof (struct mnttree));
		nmt->mt_mnt = mntdup(mnt);
		nmt->mt_sib = NULL;
		nmt->mt_sib_back = NULL;
		nmt->mt_kid = NULL;
	}

	/*
	 * If this is the first entry being inserted in the tree,
	 * just return the pointer to it.
	 */
	if (mt == NULL) {
		nmt->mt_sib = NULL;
		nmt->mt_sib_back = NULL;
		nmt->mt_kid = NULL;
		return nmt;
	}

	/*
	 * Always add a new entry at a level as the first entry in the
	 * level.
	 */
	lmt = mt;
	while (lmt != NULL) {
		next_mt = lmt->mt_sib;
		/*
		 * If the entry is a substring of the new entry, add
		 * the new entry as a child of the current one.
		 */
		if (substr(lmt->mt_mnt->mnt_dir, mnt->mnt_dir)) {
			lmt->mt_kid = maketree(lmt->mt_kid, mnt, nmt);
			return mt;
		}

		/*
		 * If the current entry is a substring of the new entry,
		 * pull it from the tree and add it back as a child of
		 * the new entry.  We know that the new entry will be
		 * inserted at this level, because otherwise the one
		 * we are now removing would have been a child of whatever
		 * the new entry would become a child of.
		 */
		if (substr(mnt->mnt_dir, lmt->mt_mnt->mnt_dir)) {
			/*
			 * Keep mt pointing to the head of this level.
			 */
			if (lmt == mt) {
				mt = lmt->mt_sib;
			}
			/*
			 * Fix up the forward and backward pointers of
			 * our siblings.
			 */
			if (lmt->mt_sib != NULL) {
				lmt->mt_sib->mt_sib_back = lmt->mt_sib_back;
			}
			if (lmt->mt_sib_back != NULL) {
				lmt->mt_sib_back->mt_sib = lmt->mt_sib;
			}
			/*
			 * Insert lmt as a child of nmt.
			 */
			nmt->mt_kid = maketree(nmt->mt_kid, lmt->mt_mnt, lmt);
		}
		lmt = next_mt;
	}
	nmt->mt_sib = mt;
	if (mt != NULL) {
		mt->mt_sib_back = nmt;
	}
	nmt->mt_sib_back = NULL;
	return (nmt);
}

void
printtree(struct mnttree *mt)
{
	if (mt) {
		printtree(mt->mt_sib);
		(void) printf("   %s\n", mt->mt_mnt->mnt_dir);
		printtree(mt->mt_kid);
	}
}

int	keepconsoleclosed = 0;
char	consolepath[] = "/dev/console";

void
background()
{
	struct stat ss, cs;

	/* ignore hup so init can't kill us */
	(void) signal(SIGHUP, SIG_IGN);

	/* now we can afford to wait a while for NFS retries */
	set_long_timeouts();

	/*
	 * Check whether stdout and stderr are the console.  Unfortunately,
	 * the window management system can't cope with processes holding the
	 * old console open when it switches to a new console pseudodevice.
	 */
	keepconsoleclosed =
		fstat(fileno(stdout), &ss) == 0
		&& stat(consolepath, &cs) == 0
		&& ss.st_dev == cs.st_dev
		&& ss.st_ino == cs.st_ino;
	if (keepconsoleclosed) {
		register int d = getdtablesize();

		while (--d > 2)
			(void) close(d);
		(void) freopen("/dev/null", "r", stdin);
		d = open("/dev/tty", 2);
		(void) ioctl(d, TIOCNOTTY, 0);
		(void) close(d);
		(void) fclose(stdout);
		(void) fclose(stderr);
	}
}

/*
 * This is the counter used to synchronize with our child processes
 * during a parallel mount.
 */
volatile int	mount_procs;

void
mount_proc_done(void)
{
	pid_t		pid;
	int		status;
	struct mnttree	*lmt;
	int		subtree_error;

	/*
	 * Reap the child proc.
	 */
	pid = wait(&status);

	/*
	 * The child processes return errors via their exit status.
	 * If a child process fails, set the global mounttree_error
	 * so that we can return a proper error to the user.
	 */
	subtree_error = WEXITSTATUS(status);
	if (subtree_error) {
		mounttree_error = subtree_error;
	}

	mount_procs--;

	sigset(SIGCLD, mount_proc_done);

}

/*
 * Mount the file systems described by the given mount dependency
 * tree. Make sure to mount all parents before their children.
 *
 * If there are a sufficient number of mounts to warrant it, we
 * do the mounts in parallel.  To do this we fork a process for
 * each entry at each level.  If there is only 1 mount point at
 * a given level, then we don't bother to fork.  These are
 * dependency levels, not directory levels.
 *
 * This routine is called recursively from mountsubtree().  It is
 * always passed a pointer to the first element in a list of tree
 * entries to be mounted.  The elements in the list do not depend
 * on each other, so they can be processed in parallel.
 *
 * mounttree_error is used to store any errors encountered
 * during the mount process.  It is a global variable so
 * that we can set it from the child process cleanup signal
 * handler when the child process exits with an error.
 */
int
mounttree(struct mnttree *mt)
{
	struct mnttree	*lmt;	/* local 'mt' pointer */
	pid_t		forkret;
	int		subtree_error;

	if (mt == NULL) {
		return 0;
	}

	if ((!parallel_mounts) || (mt->mt_sib == NULL)) {
		/*
		 * Do it serially if there are not enough total
		 * mounts to bother with parallelism or if there
		 * is only one mount point at this level.
		 */
		for (lmt = mt; lmt != NULL; lmt = lmt->mt_sib) {
			subtree_error = mountsubtree(lmt);
			if (subtree_error) {
				mounttree_error = subtree_error;
			}
		}
	} else {
		sigset(SIGCLD, mount_proc_done);
		sighold(SIGCLD);
		mount_procs = 0;
		for (lmt = mt; lmt != NULL; lmt = lmt->mt_sib) {
			forkret = fork();
			if (forkret == 0) {
				/*
				 * Mount the subtree in the child process.
				 */
				sigset(SIGCLD, SIG_DFL);
				sigrelse(SIGCLD);
				subtree_error = mountsubtree(lmt);
				exit(subtree_error);
			} else if (forkret > 0) {
				/*
				 * Keep count of the children at this level.
				 * If we have too many children at this
				 * level, then wait for some to complete
				 * before creating more of them.
				 */
				mount_procs++;
				while (mount_procs > mount_proc_limit) {
					sigpause(SIGCLD);
					sighold(SIGCLD);
				}
			} else {
				/*
				 * The fork failed.  Break out of the loop
				 * and process the rest of the tree in this
				 * proc.
				 */
				break;
			}
		}
		/*
		 * Wait for the processes that we've started to finish.
		 */
		while (mount_procs > 0) {
			sigpause(SIGCLD);
			sighold(SIGCLD);
		}
		/*
		 * If one of the forks above failed, then there may be
		 * more mounts to do at this level.  Do those now
		 * serially.  Waiting until after the forked off procs
		 * have finished simplifies the process accounting since
		 * we don't get a mix of forked and non-forked processing.
		 */
		sigset(SIGCLD, SIG_DFL);
		sigrelse(SIGCLD);
		if (lmt != NULL) {
			mt = lmt;
			parallel_mounts = 0;
			for (lmt = mt; lmt != NULL; lmt = lmt->mt_sib) {
				subtree_error = mountsubtree(lmt);
				if (subtree_error) {
					mounttree_error = subtree_error;
				}
			}
		}
	}
	return mounttree_error;
}

int
mountsubtree(struct mnttree *mt)
{
	int error;
	int slptime;
	int forked;
	int retry;
	int firsttry;
	int status = 0;
	cap_t ocap;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;

	forked = 0;
	printed = 0;
	firsttry = 1;
	slptime = BGSLEEP;
	if (!nopt(mt->mt_mnt, "retry", (uint *)&retry)) {
		retry = NRETRY;
	}
	if (ismntopt(mt->mt_mnt, "bg")) {
		set_short_timeouts();
	} else {
		set_long_timeouts();
	}

	do {
		error = mountfs(!forked, mt->mt_mnt);
		if (error != ETIMEDOUT && error != ENETDOWN &&
		    error != ENETUNREACH && error != ENOBUFS &&
		    error != ECONNREFUSED && error != ECONNABORTED) {
			break;
		}
		/*
		 * mount failed due to network problems, reset options
		 * string and retry (maybe)
		 */
		if (!forked && ismntopt(mt->mt_mnt, "bg")) {
			(void) fprintf(stderr,
				       "mount: backgrounding\n");
			(void) fprintf(stderr, "   %s\n",
				       mt->mt_mnt->mnt_dir);
			printtree(mt->mt_kid);
			if (fork())
				return 0;
			background();
			forked = 1;
		}
		/* don't print retrying if we aren't */
		if (!forked && firsttry && retry) {
			(void) fprintf(stderr, "mount: retrying\n");
			(void) fprintf(stderr, "   %s\n",
				       mt->mt_mnt->mnt_dir);
			printtree(mt->mt_kid);
			firsttry = 0;
		}
		if (keepconsoleclosed) {
			(void) fclose(stdout);
			(void) fclose(stderr);
		}
		sleep(slptime);
		slptime = MIN(slptime << 1, MAXSLEEP);
		if (keepconsoleclosed) {
			(void) freopen(consolepath, "w", stdout);
			(void) freopen(consolepath, "w", stderr);
		}
	} while (--retry >= 0);
	
	/* audit the mount (does nothing if no auditing) */
	ocap = cap_acquire(1, &cap_audit_write);
	if (error)
		satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
			  "mount: failure to mount %s on %s: %s",
			  mt->mt_mnt->mnt_fsname, mt->mt_mnt->mnt_dir,
			  strerror(error));
	else
		satvwrite(SAT_AE_MOUNT, SAT_SUCCESS,
			  "mount: mounted %s on %s", mt->mt_mnt->mnt_fsname,
			  mt->mt_mnt->mnt_dir);
	cap_surrender(ocap);

	if (!error) {
		error = mounttree(mt->mt_kid);
		if (error) {
			status = error;
		}
	} else {
		status = error;
		(void) fprintf(stderr, "mount: giving up on:\n");
		(void) fprintf(stderr, "   %s\n", mt->mt_mnt->mnt_dir);
		printtree(mt->mt_kid);
	}
	if (forked) {
		exit(0);
	}
	return (status);
}

/*
 * Returns true if s1 is a pathname substring of s2.
 */
substr(char *s1, char *s2)
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

void
usage()
{
	fprintf(stdout,
		"Usage: mount [-acfnrpv [-b list] [-h host] [-i altfstab] [-o option] [-t|F type] [-T type1,type2...] [-m numprocs (1 to 128)] [-M altmtab] [-P prefix] ... [fsname] [dir]]\n");
	exit(1);
}


/*
 * Returns the next option in the option string.
 */
char *
getnextopt(char **p)
{
        char *cp = *p;
        char *retstr;

        while (*cp && isspace(*cp))
                cp++;
        retstr = cp;
        while (*cp && *cp != ',')
                cp++;
        if (*cp) {
                *cp = '\0';
                cp++;
        }
        *p = cp;
        return (retstr);
}

/*
 * "trueopt" and "falseopt" are two settings of a Boolean option.
 * If "flag" is true, forcibly set the option to the "true" setting; otherwise,
 * if the option isn't present, set it to the false setting.
 */
void
replace_opts(char *options, int flag, char *trueopt, char *falseopt)
{
	register char *f;
	char tmptopts[MNTMAXSTR];
	char *tmpoptsp;
	register int found;

	(void) strcpy(tmptopts, options);
	tmpoptsp = tmptopts;
	(void) strcpy(options, "");

	found = 0;
	for (f = getnextopt(&tmpoptsp); *f; f = getnextopt(&tmpoptsp)) {
		if (strcmp(f, trueopt) == 0) {
			if (options[0] != '\0')
				(void) strcat(options, ",");
			(void) strcat(options, f);
			found++;
		} else if (falseopt && strcmp(f, falseopt) == 0) {
			if (options[0] != '\0')
				(void) strcat(options, ",");
			if (flag)
				(void) strcat(options, trueopt);
			else
				(void) strcat(options, f);
			found++;
		} else {
			if (options[0] != '\0')
				(void) strcat(options, ",");
			(void) strcat(options, f);
        }
	}
	if (!found) {
		if ( flag ) {
			if (options[0] != '\0')
				(void) strcat(options, ",");
			(void) strcat(options, trueopt);
		} else if ( falseopt ) {
			if (options[0] != '\0')
				(void) strcat(options, ",");
			(void) strcat(options, falseopt);
		}
	}
}

int
adjustnfsargs(char *type, struct mntent *mntp)
{

	u_int vers = 0;
	char *origp, origtype[MNTMAXSTR];

	if (type)
		strcpy(origtype, type);
	else
		strcpy(origtype, mntp->mnt_type);

	/*
	 * look for vers options and adjust type :
	 *  	vers=2 ==> MNTTYPE_NFS2
	 *	vers=3 ==> MNTTYPE_NFS3
	 * 	no vers ==> MNTTYPE_NFS3_PREF
	 * 	invalid vers ==> error
	 * 	vers & MNTTYPE mismatch ==> error
	 */
	if (nopt(mntp, MNTOPT_VERS, &vers)) {
		switch (vers) {
		case 2:
			mntp->mnt_type = MNTTYPE_NFS2;
			break;
		case 3:
			mntp->mnt_type = MNTTYPE_NFS3;
			break;
		default:
			(void) fprintf (stderr, 
			     "mount: invalid option vers=%d\n",
			      vers);
			return (1);
		}
			
	} else if (type) {
		mntp->mnt_type = type;
	}

	/*
	 * if origtype is nfs, then any valid vers= option is fine.
	 * if origtype is not (strcmp return != 0), then if the
	 * vers options changed the type we have a mismatch.
         */

	if (strcmp(origtype, MNTTYPE_NFS) && vers && strcmp(origtype, mntp->mnt_type)) {
		(void) fprintf (stderr, 
	           "mount: mismatched options 'vers=%d' and '-t %s'\n",
		    vers, origtype);
		return (1);
	}

	if (strcmp(mntp->mnt_type,MNTTYPE_NFS) == 0) {
		mntp->mnt_type = MNTTYPE_NFS3_PREF;
	}

	return (0);
}

