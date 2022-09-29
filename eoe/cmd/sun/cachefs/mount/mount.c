/*
 *
 *			mount.c
 *
 * Cachefs mount program.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #pragma ident "@(#)mount.c   1.24     94/04/21 SMI" */

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *  (c) 1986,1987,1988.1989  Sun Microsystems, Inc
 *  (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <mntent.h>
#include <cachefs/cachefs_fs.h>
#include <sys/fstyp.h>
#include <sys/statfs.h>
#include <assert.h>
#include <sys/syssgi.h>
#include "subr.h"

char *cfs_opts[] = {
	#define CFSOPT_BACKFSTYPE	0
		"backfstype",
	#define CFSOPT_CACHEDIR		1
		"cachedir",
	#define CFSOPT_CACHEID		2
		"cacheid",
	#define CFSOPT_BACKPATH		3
		"backpath",

	#define CFSOPT_WRITEAROUND	4
		"write-around",
	#define CFSOPT_NONSHARED	5
		"non-shared",

	#define CFSOPT_NOCONST		6
		"noconst",

	#define CFSOPT_LOCALACCESS	7
		"local-access",
	#define	CFSOPT_PURGE		8
		"purge",

	#define CFSOPT_RW		9
		"rw",
	#define CFSOPT_RO		10
		"ro",
	#define CFSOPT_SUID		11
		"suid",
	#define CFSOPT_NOSUID		12
		"nosuid",
	#define CFSOPT_REMOUNT		13
		"remount",

	#define CFSOPT_ACREGMIN		14
		"acregmin",
	#define CFSOPT_ACREGMAX		15
		"acregmax",
	#define CFSOPT_ACDIRMIN		16
		"acdirmin",
	#define CFSOPT_ACDIRMAX		17
		"acdirmax",
	#define CFSOPT_ACTIMEO		18
		"actimeo",

	#define CFSOPT_NOMTAB		19
		MNTOPT_NOMTAB,

	#define CFSOPT_BG			20
		"bg",

	#define CFSOPT_DISCONNECT	21
		"disconnect",

	#define CFSOPT_NONBLOCK		22
		"non-block",

	#define CFSOPT_DEBUG		23
		"debug",

	#define CFSOPT_PRIVATE		24
		"private",

		NULL
};


#define	bad(val) (val == NULL || !isdigit(*val))

char *Progname;
int Debug = 0;

/* forward references */
void usage(char *msgp);
int set_cfs_args(char *, struct cachefs_mountargs *, int *, int *, char **,
	char **);
int addtomtab(char *, char *, char *, char *);
void doexec(char *fstype, char **newargv, char *myname);
char *get_back_fsid(char *specp);
char *get_old_cacheid(char *);

void
rename_old_cache(char *cachedir, char *fsid, char *cacheid)
{
	char *old_cacheid = get_old_cacheid(fsid);
	char oldpath[MAXPATHLEN];
	char newpath[MAXPATHLEN];

	if (old_cacheid) {
		sprintf(oldpath, "%s/%s", cachedir, old_cacheid);
		sprintf(newpath, "%s/%s", cachedir, cacheid);
		rename(oldpath, newpath);
	}
}

/*
 *
 *			main
 *
 * Description:
 *	Main routine for the cachefs mount program.
 * Arguments:
 *	argc	number of command line arguments
 *	argv	list of command line arguments
 * Returns:
 *	Returns 0 for success, 1 an error was encountered.
 * Preconditions:
 */

main(int argc, char **argv)
{
	int shared = 1;
	int back_mount_done = 0;
	int ischild = 0;
	register short fstyp, nfstyp;
	struct statfs sfsb;
	char type[MNTMAXSTR];
	char optbuf[MNTMAXSTR + 12];
	int error;
	char cmdbuf[MAXPATHLEN + 9];
	int newcache = 0;
	struct user_values uv;
	char *myname;
	char *optionp;
	char *opigp;
	int mflag;
	int nomnttab;
	int readonly;
	struct cachefs_mountargs margs;
	char *backfstypep;
	char *reducep;
	char *specp;
	int xx;
	int stat_loc;
	char *newargv[20];
	char *mntp;
	pid_t pid;
	int mounted;
	int c;
	int lockid;
	char *cacheidpath;
	struct stat sb;
	char *cfs_fsid = NULL;

	(void) setlocale(LC_ALL, "");
#ifndef TEXT_DOMAIN
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

	Progname = *argv;

	/* verify root running command */
	if (geteuid() != 0) {
		pr_err(gettext("must be run by root"));
		return (1);
	}

	if (argv[0]) {
		myname = strrchr(argv[0], '/');
		if (myname)
			myname++;
		else
			myname = argv[0];
	} else {
		myname = "path unknown";
	}

	optionp = NULL;
	nomnttab = 0;
	readonly = 0;

	/* process command line options */
	while ((c = getopt(argc, argv, "no:r")) != EOF) {
		switch (c) {
		case 'n':	/* no entry in /etc/mnttab */
			nomnttab = 1;
			break;

		case 'o':
			optionp = optarg;
			break;


		case 'r':	/* read only mount */
			readonly = 1;
			break;

		default:
			usage("invalid option");
			return (1);
		}
	}

	/* if -o not specified */
	if (optionp == NULL) {
		switch ( argc - optind ) {
			case 4:		/* invocation from mount */
			case 3:
				specp = argv[optind];
				mntp = argv[optind + 1];
				optionp = argv[optind + 3];
				break;
			case 2:		/* direct invocation with missing -o */
				specp = argv[optind];
				mntp = argv[optind + 1];
				optionp = NULL;
				break;
			default:	/* other possiblities */
				usage(gettext("must specify special device and mount point"));
				return (1);
		}
	} else {
		if (argc - optind < 2) {
			usage(gettext("must specify special device and mount point"));
			return (1);
		} else {
			specp = argv[optind];
			mntp = argv[optind + 1];
		}
	}

	/* Initialize default mount values */
	margs.cfs_options = CFS_ACCESS_BACKFS;
	margs.cfs_cacheid = NULL;
	margs.cfs_cachedir = CFS_DEF_DIR;
	margs.cfs_backfs = NULL;
	margs.cfs_acregmin = CFS_DEF_ACREGMIN;
	margs.cfs_acregmax = CFS_DEF_ACREGMAX;
	margs.cfs_acdirmin = CFS_DEF_ACDIRMIN;
	margs.cfs_acdirmax = CFS_DEF_ACDIRMAX;
	mflag = 0;
	backfstypep = NULL;

	/* process -o options */
	xx = set_cfs_args(optionp, &margs, &mflag, &nomnttab, &backfstypep,
		&reducep);
	if (xx) {
		return (1);
	}

	/* set default write mode if not specified */
	if ((margs.cfs_options & (CFS_WRITE_AROUND|CFS_DUAL_WRITE)) == 0) {
		margs.cfs_options |= CFS_DUAL_WRITE;
		if (backfstypep && ((strcmp(backfstypep, FSID_ISO9660) == 0) ||
			(strcmp(backfstypep, FSID_CDFS) == 0)))
				mflag |= MS_RDONLY;
	}

	/* if read-only was specified with the -r option */
	if (readonly) {
		mflag |= MS_RDONLY;
	}


	/* get the fsid of the backfs and the cacheid */
	cfs_fsid = get_back_fsid(specp);
	if (cfs_fsid == NULL) {
		pr_err(gettext("out of memory"));
		return (1);
	}

	/* get the front file system cache id if necessary */
	if (margs.cfs_cacheid == NULL) {
		margs.cfs_cacheid = get_cacheid(cfs_fsid, mntp);

		if (margs.cfs_cacheid == NULL) {
			pr_err(gettext("cacheid too long"));
			return (1);
		}

		rename_old_cache(margs.cfs_cachedir, cfs_fsid, margs.cfs_cacheid);

	}
	cacheidpath = malloc(MAXPATHLEN);
	if (!cacheidpath) {
		pr_err(gettext("out of memory"));
		return(1);
	}
	sprintf(cacheidpath, "%s/%s", margs.cfs_cachedir, CACHELABEL_NAME);
	/*
	 * Check the cache directory to see if it exists.  If it does not,
	 * create it.  Or if it exists and has no label file, create it.
	 */
	if ((stat(margs.cfs_cachedir, &sb) == -1) ||
		(stat(cacheidpath, &sb) == -1)) {
			/*
			 * set shared to 0 so that we will acquire an exclusive lock
			 * on the cache directory below
			 */
			shared = 0;
			/* get default cache paramaters */
			user_values_defaults(&uv);
			xx = create_cache(margs.cfs_cachedir, &uv);
			switch (xx) {
				case 0:
					break;
				case -2:
					/* remove a partially created cache dir */
					(void) delete_all_cache(margs.cfs_cachedir);
				default:
					pr_err(gettext("Unable to create cache"));
					return(1);
			}
	}
	/*
	 * Check the cacheid directory to see if it exists.  If it does not,
	 * create it.
	 */
	sprintf(cacheidpath, "%s/%s", margs.cfs_cachedir, margs.cfs_cacheid);
	if (stat(cacheidpath, &sb) == -1) {
		if (errno != ENOENT) {
			pr_err(gettext("cannot stat cacheid directory %s: %s"), cacheidpath,
				strerror(errno));
			return(1);
		} else if ( mkdir(cacheidpath, CACHEFS_DIRMODE) == -1 ) {
			pr_err(gettext("cannot create cacheid directory %s: %s"),
				cacheidpath, strerror(errno));
			return(1);
		} else {
			newcache = 1;
		}
	}

	/* lock the cache directory shared */
	lockid = cachefs_dir_lock(margs.cfs_cachedir, shared);
	if (lockid == -1) {
		/* exit if could not get the lock */
		return (1);
	}

	/* if no mount point was specified */
	mounted = 0;
	if (margs.cfs_backfs == NULL) {
		mounted = 1;

		/* get a suitable mount point */
		xx = get_mount_point(margs.cfs_cachedir, margs.cfs_cacheid,
		    &margs.cfs_backfs, 1);
		switch (xx) {
			case 0:		/* not mounted */
				/*
				 * construct argument list for mounting the back file
				 * system
				 */
				xx = 1;
				newargv[xx++] = "mount";
				if (readonly)
					newargv[xx++] = "-r";
				newargv[xx++] = "-nv";
				if (backfstypep) {
					newargv[xx++] = "-t";
					newargv[xx++] = backfstypep;
				}
				if (reducep) {
					newargv[xx++] = "-o";
					newargv[xx++] = reducep;
				}
				newargv[xx++] = specp;
				newargv[xx++] = margs.cfs_backfs;
				newargv[xx++] = NULL;

				do {
					/* fork */
					if ((pid = fork()) == -1) {
						pr_err(gettext("could not fork %s"),
						    strerror(errno));
						cachefs_dir_unlock(lockid);
						if (ischild) {
							pr_err("Warning, back FS mount of %s for %s "
								"failed: %s", margs.cfs_backfs, mntp,
								strerror(error));
						}
						return (1);
					}

					/* if the child */
					if (pid == 0) {
						/* do the mount */
						doexec(backfstypep, newargv, "mount");
						/* NOTREACHED */
					}

					/* else if the parent */

					assert(pid > 0);

					/* wait for the child to exit */
					if (wait(&stat_loc) == -1) {
						pr_err(gettext("wait failed %s"),
				   		 strerror(errno));
						cachefs_dir_unlock(lockid);
						if (ischild) {
							pr_err("Warning, back FS mount of %s for %s "
								"failed: %s", margs.cfs_backfs, mntp,
								strerror(error));
						}
						return (1);
					}

					if (!WIFEXITED(stat_loc)) {
						cachefs_dir_unlock(lockid);
						if (WIFSIGNALED(stat_loc)) {
							pr_err("back mount terminated on signal %d%s",
								WTERMSIG(stat_loc),
								WCOREDUMP(stat_loc) ? " (core dumped)" :
								"");
						} else if (WIFSTOPPED(stat_loc)) {
							pr_err("back mount stopped on signal %d",
								WSTOPSIG(stat_loc));
						} else {
							pr_err(gettext("back mount did not exit"));
						}
						if (ischild) {
							pr_err("Warning, back FS mount of %s for %s "
								"failed: %s", margs.cfs_backfs, mntp,
								strerror(error));
						}
						return (1);
					}

					/*
					 * Get the exit status.  If it is not 0, check for
					 * timeout and disconnected mode mount.
					 */
					if ((error = WEXITSTATUS(stat_loc)) != 0) {
						if (error == ETIMEDOUT) {
							/*
							 * Is this the child process for a disconnected
							 * mode mount?  If so, just continue the loop.
							 */
							if (!ischild) {
								/*
								 * Not the child.
								 * Are we doing a disconnected mode mount
								 * but not a remount?
								 */
								if (!(margs.cfs_options & CFS_ADD_BACK) &&
									(margs.cfs_options & CFS_DISCONNECT)) {
									/*
									 * Yes, a disconnected mode mount.
									 * fork a child to mount the back FS
									 * Let the parent continue with the
									 * mount.
									 */
									pr_err("mounting %s disconnected.\n",
										margs.cfs_backfs);
									switch (fork()) {
										case 0:		/* child */
											ischild = 1;
											margs.cfs_options |= CFS_ADD_BACK;
											continue;
										case -1:	/* error */
											error = errno;
											pr_err("Unable to fork mount "
												"for %s: %s\n",
												margs.cfs_backfs,
												strerror(error));
											return(error);
										default:	/* parent */
											/*
											 * Set CFS_NO_BACKFS
											 * to indicate that there is no
											 * back FS yet.  The child process
											 * handling the back FS mount will
											 * do a remount to set the back FS.
											 * Set back_mount_done so that
											 * we don't loop.
											 * Clear the error.
											 */
											margs.cfs_options |= CFS_NO_BACKFS;
											back_mount_done = 1;
											error = 0;
									}
								} else {
									/*
									 * Not doing a disconnected mode mount,
									 * so pass the error back to the parent
									 * process.
									 */
									cachefs_dir_unlock(lockid);
									return (error);
								}
							}
						} else {
							/*
							 * The back FS mount exited with some error other
							 * than a timeout.  Pass that along to the
							 * parent process.
							 */
							if (ischild) {
								pr_err("Warning, back FS mount of %s for %s "
									"failed: %s", margs.cfs_backfs, mntp,
									strerror(error));
							}
							cachefs_dir_unlock(lockid);
							return (error);
						}
					} else {
						back_mount_done = 1;
					}
				} while (!back_mount_done);
				break;
			case -2:	/* mounted */
				break;
			default:	/* error */
				cachefs_dir_unlock(lockid);
				return (1);
		}

	}

	/* mount the cache file system */
	if (margs.cfs_options & CFS_ADD_BACK) {
		xx = syssgi(SGI_CACHEFS_SYS, CACHEFSSYS_MOUNT, &margs);
	} else {
		xx = mount(margs.cfs_backfs, mntp, mflag | MS_DATA, MNTTYPE_CACHEFS,
		    &margs, sizeof (margs));
	}
	if (xx == -1) {
		switch (errno) {
			case ESTALE:
				if (!newcache) {
					pr_err(gettext("Stale cache -- removing %s"),
						cacheidpath);
					/*
					 * The cache is stale.  Remove all of it and start over.
					 */
					if (delete_cache(margs.cfs_cachedir, margs.cfs_cacheid)
						== -1) {
							pr_err(gettext(
								"Unable to remove cacheid directory %s: %s"),
								cacheidpath, strerror(errno));
					} else {
						if ( mkdir(cacheidpath, CACHEFS_DIRMODE) == -1 ) {
							pr_err(gettext(
								"cannot create cacheid directory %s: %s"),
								cacheidpath, strerror(errno));
						} else {
							/* mount the cache file system again */
							xx = mount(margs.cfs_backfs, mntp,
								mflag | MS_DATA, MNTTYPE_CACHEFS, &margs,
								sizeof (margs));
							if (xx == 0) {
								/*
								 * break out of the switch if and only
								 * if the mount succeeds.
								 */
								break;
							}
						}
					}
				}
				/*
				 * All of the above falls through to the default on
				 * any failure.
				 */
			default:
				pr_err(gettext("mount failed %s"), strerror(errno));

				/* try to unmount the back file system if we mounted it */
				if (mounted) {
					xx = 1;
					newargv[xx++] = "umount";
					newargv[xx++] = margs.cfs_backfs;
					newargv[xx++] = NULL;

					/* fork */
					if ((pid = fork()) == -1) {
						pr_err(gettext("could not fork: %s"),
						    strerror(errno));
						cachefs_dir_unlock(lockid);
						return (1);
					}

					/* if the child */
					if (pid == 0) {
						/* do the unmount */
						doexec(backfstypep, newargv, "umount");
					}

					/* else if the parent */
					else {
						wait(0);
					}
				}

				cachefs_dir_unlock(lockid);
				return (1);
		}
	}

	/*
	 * Start the garbage collection daemon.  If one is already running,
	 * the replacement daemon will detect that and exit.
	 */
	newargv[1] = "cachefs_replacement";
	if (Debug) {
		newargv[2] = "-d";
		newargv[3] = margs.cfs_cachedir;
		newargv[4] = NULL;
	} else {
		newargv[2] = margs.cfs_cachedir;
		newargv[3] = NULL;
	}

	/* fork */
	if ((pid = fork()) == -1) {
		pr_err(gettext("could not fork: %s"), strerror(errno));
		cachefs_dir_unlock(lockid);
		return (1);
	}

	/* if the child */
	if (pid == 0) {
		doexec("cachefs", newargv, "cachefs_replacement");
		/* make sure we exit if doexec fails */
		exit(1);
	}

	/* release the lock on the cache directory */
	cachefs_dir_unlock(lockid);

	/* update mnttab file if necessary */
	if (!nomnttab) {
		/* add entry for front file system */
#ifdef ORIGINAL
		xx = addtomtab(margs.cfs_backfs, mntp, MNTTYPE_CACHEFS, optionp);
#else
		xx = addtomtab(specp, mntp, MNTTYPE_CACHEFS, optionp);
#endif
		if (xx != 0)
			return (1);


		/* if we added the back file system, add it also with ignore */
		if (mounted && margs.cfs_backfs) {
			xx = addtomtab(specp, margs.cfs_backfs,
			    MNTTYPE_IGNORE, reducep ? reducep : "");
			if (xx != 0)
				return (1);
		}
	}

	/* return success */
	return (0);
}


/*
 *
 *			usage
 *
 * Description:
 *	Prints a short usage message.
 * Arguments:
 *	msgp	message to include with the usage message
 * Returns:
 * Preconditions:
 */

void
usage(char *msgp)
{
	if (msgp) {
		pr_err(gettext("%s"), msgp);
	}

	fprintf(stderr,
	    gettext("Usage: mount -t cachefs [generic options] "
	    "-o backfstype=file_system_type[,FSTypespecific_options] "
	    "special mount_point\n"));
}

/*
 *
 *			set_cfs_args
 *
 * Description:
 *	Parse the comma delimited set of options specified by optionp
 *	and puts the results in margsp, mflagp, and backfstypepp.
 *	A string is constructed of options which are not specific to
 *	cfs and is placed in reducepp.
 *	Pointers to strings are invalid if this routine is called again.
 *	No initialization is done on margsp, mflagp, or backfstypepp.
 * Arguments:
 *	optionp		string of comma delimited options
 *	margsp		option results for the mount dataptr arg
 *	mflagp		option results for the mount mflag arg
 *	backfstypepp	set to name of back file system type
 *	reducepp	set to the option string without cfs specific options
 * Returns:
 *	Returns 0 for success, -1 for an error.
 * Preconditions:
 *	precond(optionp)
 *	precond(margsp)
 *	precond(mflagp)
 *	precond(backfstypepp)
 *	precond(reducepp)
 */

int
set_cfs_args(
	char *optionp,
	struct cachefs_mountargs *margsp,
	int *mflagp,
	int *nomnttabp,
    char **backfstypepp,
	char **reducepp)
{
	int bg = 0;
	int disconnect = 0;
	int optno;
	static char *optstrp = NULL;
	static char *reducep = NULL;
	char *savep, *strp, *valp;
	int badopt;
	int ret;
	int o_rw = 0;
	int o_ro = 0;
	int o_suid = 0;
	int o_nosuid = 0;
	int xx;
	u_long yy;

	/* free up any previous options */
	free(optstrp);
	free(reducep);
	if (!optionp) {
		return(0);
	}

	/* make a copy of the options so we can modify it */
	optstrp = strp = strdup(optionp);
	/*
	 * If disconnect is specified, 10 extra bytes may be required in the
	 * options passed to the back file system to set the retransmission
	 * count.
	 */
	reducep = malloc(strlen(optionp) + 10);
	if ((strp == NULL) || (reducep == NULL)) {
		pr_err(gettext("out of memory"));
		return (-1);
	}
	*reducep = '\0';

	/* parse the options */
	badopt = 0;
	ret = 0;
	while (*strp) {
		savep = strp;
		switch (optno = getsubopt(&strp, cfs_opts, &valp)) {

		case CFSOPT_BACKFSTYPE:
			if (valp == NULL)
				badopt = 1;
			else
				*backfstypepp = valp;
			break;

		case CFSOPT_CACHEDIR:
			if (valp == NULL)
				badopt = 1;
			else
				margsp->cfs_cachedir = valp;
			break;

		case CFSOPT_CACHEID:
			if (valp == NULL) {
				badopt = 1;
				break;
			}

			if (strlen(valp) >= (size_t)MAXPATHLEN) {
				pr_err(gettext("name too long"));
				badopt = 1;
				break;
			}

			margsp->cfs_cacheid = strdup(valp);
			break;

		case CFSOPT_BACKPATH:
			if (valp == NULL)
				badopt = 1;
			else
				margsp->cfs_backfs = valp;
			break;

		case CFSOPT_WRITEAROUND:
			margsp->cfs_options |= CFS_WRITE_AROUND;
			break;

		case CFSOPT_NONSHARED:
			margsp->cfs_options |= CFS_DUAL_WRITE;
			break;

		case CFSOPT_NOCONST:
			margsp->cfs_options |= CFS_NOCONST_MODE;
			break;

		case CFSOPT_LOCALACCESS:
			margsp->cfs_options &= ~CFS_ACCESS_BACKFS;
			break;

		case CFSOPT_PURGE:
			margsp->cfs_options |= CFS_PURGE;
			break;

		case CFSOPT_RW:
			o_rw = 1;
			*mflagp &= ~MS_RDONLY;
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_RO:
			o_ro = 1;
			*mflagp |= MS_RDONLY;
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_SUID:
			o_suid = 1;
			*mflagp &= ~MS_NOSUID;
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_NOSUID:
			o_nosuid = 1;
			*mflagp |= MS_NOSUID;
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_REMOUNT:
			margsp->cfs_options |= CFS_ADD_BACK;
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_ACREGMIN:
			if (bad(valp))
				badopt = 1;
			else
				margsp->cfs_acregmin = strtoul(valp, NULL, 10);
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_ACREGMAX:
			if (bad(valp))
				badopt = 1;
			else
				margsp->cfs_acregmax = strtoul(valp, NULL, 10);
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_ACDIRMIN:
			if (bad(valp))
				badopt = 1;
			else
				margsp->cfs_acdirmin = strtoul(valp, NULL, 10);
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_ACDIRMAX:
			if (bad(valp))
				badopt = 1;
			else
				margsp->cfs_acdirmax = strtoul(valp, NULL, 10);
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_ACTIMEO:
			if (bad(valp))
				badopt = 1;
			else {
				yy = strtoul(valp, NULL, 10);
				margsp->cfs_acregmin = yy;
				margsp->cfs_acregmax = yy;
				margsp->cfs_acdirmin = yy;
				margsp->cfs_acdirmax = yy;
			}
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;

		case CFSOPT_NOMTAB:
			*nomnttabp = 1;
			break;

		case CFSOPT_BG:
			/*
			 * Filter out the bg option.  We do not want to pass it to
			 * the back FS mount.  The generic mount will handle it.
			 */
			if (!disconnect) {
				strcat(reducep, ",retry=0");
				bg = 1;
			}
			break;

		case CFSOPT_DISCONNECT:
			margsp->cfs_options |= CFS_DISCONNECT;
			if (bg) {
				strcat(reducep, ",soft,timeo=1,retrans=1");
			} else {
				strcat(reducep, ",soft,retry=0,timeo=1,retrans=1");
				disconnect = 1;
			}
			break;

		case CFSOPT_NONBLOCK:
			margsp->cfs_options |= CFS_NONBLOCK;
			strcat(reducep, ",soft");
			break;

		case CFSOPT_DEBUG:
			Debug = 1;
			break;

		case CFSOPT_PRIVATE:
			margsp->cfs_options |= CFS_PRIVATE;
			break;

		default:
			/* unknown option, save for the back file system */
			strcat(reducep, ",");
			strcat(reducep, savep);
			break;
		}

		/* if a lexical error occurred */
		if (badopt) {
			pr_err(gettext("invalid argument to option: \"%s\" optno %d"),
			    savep, optno);
			badopt = 0;
			ret = -1;
		}
	}

	/* check for conflicting options */
	if (o_suid && o_nosuid) {
		pr_err(gettext("suid and nosuid are mutually exclusive"));
		ret = -1;
	}
	if (o_rw && o_ro) {
		pr_err(gettext("rw and ro are mutually exclusive"));
		ret = -1;
	}
	if (margsp->cfs_acregmin > margsp->cfs_acregmax) {
		pr_err(gettext("acregmin cannot be greater than acregmax"));
		ret = -1;
	}
	if (margsp->cfs_acdirmin > margsp->cfs_acdirmax) {
		pr_err(gettext("acdirmin cannot be greater than acdirmax"));
		ret = -1;
	}

	switch (margsp->cfs_options & (CFS_DUAL_WRITE | CFS_WRITE_AROUND)) {
		case 0:
		case CFS_DUAL_WRITE:
		case CFS_WRITE_AROUND:
			break;
		default:	/* CFS_DUAL_WRITE | CFS_WRITE_AROUND */
			pr_err(gettext(
			    "only one of non-shared or write-around may be specified"));
			ret = -1;
	}

	/* if an error occured */
	if (ret)
		return (-1);

	/* if there are any options which are not mount specific */
	if (*reducep)
		*reducepp = reducep + 1;
	else
		*reducepp = NULL;

	/* return success */
	return (0);
}

/*
 *
 *			doexec
 *
 * Description:
 *	Execs the specified program with the specified command line arguments.
 *	This function never returns.
 * Arguments:
 *	fstype		type of file system
 *	newargv		command line arguments
 *	progp		name of program to exec
 * Returns:
 * Preconditions:
 *	precond(fstype)
 *	precond(newargv)
 */

void
doexec(char *fstype, char *newargv[], char *progp)
{
	int error = 1;

	if (execvp( progp, &newargv[1] ) == -1) {
		error = errno;
		pr_err(gettext("operation %s not applicable to FSType %s: %s"), progp,
			fstype ? fstype : "NULL", strerror(error));
	} else {
		pr_err(gettext("operation %s not applicable to FSType %s"), progp,
			fstype ? fstype : "NULL");
	}

	exit(error);
}

/*
 *
 *			get_back_fsid
 *
 * Description:
 *	Determines a unique identifier for the back file system.
 * Arguments:
 *	specp	the special file of the back fs
 * Returns:
 *	Returns a malloc string which is the unique identifer
 *	or NULL on failure.  NULL is only returned if malloc fails.
 * Preconditions:
 *	precond(specp)
 */

char *
get_back_fsid(char *specp)
{
	return (strdup(specp));
}

/*
 *
 *			get_old_cacheid
 *
 * Description:
 *	Determines an identifier for the front file system cache.
 *	The length of the returned string is < PATH_MAX.
 *	The cache ID is of the form CACHEID_PREFIX<fsid> where fsid
 *	is the back file system FS name.
 * Arguments:
 *	fsidp	back file system id
 * Returns:
 *	Returns a pointer to the string identifier, or NULL if the
 *	identifier was overflowed.
 * Preconditions:
 *	precond(fsidp)
 */

char *
get_old_cacheid(char *fsidp)
{
	char *c1;
	char *buf;
	int len = strlen(fsidp) + strlen(CACHEID_PREFIX);

	if (len >= (size_t) PATH_MAX)
		return (NULL);

	buf = malloc(len + 1);
	if (!buf)
		return(NULL);
	strcpy(buf, CACHEID_PREFIX);
	strcat(buf, fsidp);
	c1 = buf;
	while ((c1 = strchr(c1, '/')) != NULL)
		*c1 = '_';
	return (buf);
}

/* -----------------------------------------------------------------
 *
 *			addtomtab
 *
 * Description:
 *	Adds information about the mounting of the front file system
 *	to the mnttab file.
 * Arguments:
 *	specp	the special file
 *	mntp	the mount point
 *	fstypep	the file system type
 *	optionp	the mount options
 * Returns:
 *	Returns 0 for success, -1 for an error.
 * Preconditions:
 */

#define	TIME_MAX 16

int
addtomtab(char *specp, char *mntp, char *fstypep, char *optionp)
{
	struct mntent mnt;
	FILE *fout;
	char tbuf[TIME_MAX];
	char optbuf[256];
	struct stat st;

	mnt.mnt_dir = mntp;
	mnt.mnt_type = fstypep;
	mnt.mnt_fsname = specp;
	mnt.mnt_freq = mnt.mnt_passno = 0;

	/* Get device id */
	if (stat(mntp, &st) < 0) {
		pr_err(gettext("could not stat %s %s"), mntp,
		    strerror(errno));
		return (-1);
	}
	if (optionp) {
		strcpy(optbuf, optionp);
		/*
		 * We fill in dev= with the value of st_rdev.  This is so that the
		 * dev= value in the mount entry will be the same as that vfs_dev
		 * in the vfs structure for the mount in the kernel.  This is done
		 * for autofs because it expects vfs_dev to be the same as the
		 * dev= value in the mount entry.
		 */
		sprintf(&optbuf[strlen(optionp)], ",dev=%x", st.st_rdev);
	} else {
		sprintf(optbuf, "dev=%x", st.st_rdev);
	}
	mnt.mnt_opts = optbuf;

	/* open the file */
	fout = setmntent(MOUNTED, "a+");
	if (fout == NULL) {
		pr_err(gettext("could not open %s %s"), MOUNTED,
		    strerror(errno));
		return (-1);
	}

	/* lock the file */
	if (lockf(fileno(fout), F_LOCK, 0L) < 0) {
		pr_err(gettext("could not lock %s %s"), MOUNTED,
		    strerror(errno));
		return (-1);
	}

	fseek(fout, 0L, 2); /* guarantee at EOF */

	if ( addmntent(fout, &mnt) ) {
		pr_err(gettext("could not add mount entry"));
		(void) endmntent( fout );
		return (-1);
	}
	(void) endmntent( fout );

	/* return success */
	return (0);
}
