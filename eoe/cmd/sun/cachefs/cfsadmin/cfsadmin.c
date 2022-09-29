/* -----------------------------------------------------------------
 *
 *			cfsadmin.c
 *
 * Cache FS admin utility.
 */

/* #pragma ident "@(#)cfsadmin.c   1.18     93/06/03 SMI" */

/*
 *  (c) 1992  Sun Microsystems, Inc
 *	All rights reserved.
 */

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mman.h>
#include <mntent.h>
#include <cachefs/cachefs_fs.h>
#include <sys/syssgi.h>
#include "subr.h"

char *cfsadmin_opts[] = {
	#define COPT_MAXBLOCKS		0
		"maxblocks",

	#define	COPT_MAXFILES 		1
		"maxfiles",

	#define COPT_HIBLOCKS		2
		"hiblocks",
	#define COPT_LOWBLOCKS		3
		"lowblocks",
	#define COPT_HIFILES		4
		"hifiles",
	#define COPT_LOWFILES		5
		"lowfiles",
		NULL
};

/* size to make a buffer for holding a pathname */
#define	XMAXPATH (PATH_MAX + MAXNAMELEN + 2)

#define	bad(val)	((val) == NULL || !isdigit(*(val)))

/* numbers must be valid percentages ranging from 0 to 100 */
#define	badpercent(val) \
	((val) == NULL || !isdigit(*(val)) || \
	    atoi((val)) < 0 || atoi((val)) > 100)

char *Progname;
ulong_t Fsid;

/* forward references */
void usage(char *msg);
int cfs_get_opts(char *oarg, struct user_values *uvp);
int update_cachelabel(char *dirp, char *optionp);
int cache_stats(char *dirp);
int convert_old_cacheids(char *dir);

int
checkfid(const char *path, const struct stat *sbp, int filetype,
	struct FTW *ftwp)
{
	checkfidarg_t cfarg;
	int status = 0;
	int fd;
	char fileheader[FILEHEADER_SIZE];
	fileheader_t *fhp = (fileheader_t *)fileheader;

	/*
	 * Do not process any files at the root level.
	 */
	if (ftwp->level == 0) {
		return(0);
	}
	switch (filetype) {
		case FTW_F:		/* regular file */
			/*
			 * Read the file header.
			 */
			if (read_file_header(path, fileheader)) {
				cfarg.cf_fsid = Fsid;
				cfarg.cf_fid = fhp->fh_metadata.md_backfid;
				if (syssgi(SGI_CACHEFS_SYS, CACHEFSSYS_CHECKFID, &cfarg) ==
					-1) {
						switch (errno) {
							case ESTALE:
								if ( unlink(path) == -1 ) {
									pr_err("Unable to remove file %s: %s",
										path, strerror(errno));
								} else {
									pr_err("Stale file %s removed", path);
								}
								break;
							case EINTR:
								status = -1;
								break;
							default:
								pr_err("Unable to check file %s: %s", path,
									strerror(errno));
								break;
						}
				}
			}
			break;
		case FTW_D:
			/*
			 * Ignore directories
			 */
			break;
		case FTW_DP:
			break;
		case FTW_SLN:	/* symlink -- there should be none of these */
			if ( unlink(path) == -1 ) {
				pr_err("Symbolic link found but not removed: %s, error %s",
					path, strerror(errno));
			} else {
				pr_err("Symbolic link found and removed: %s", path);
			}
			break;
		case FTW_DNR:	/* unreadable directory -- log an error */
			pr_err("Directory %s cannot be read", path);
			break;
		case FTW_NS:	/* unsatable file/directory -- log an error */
			pr_err("Stat failed for %s", path);
			break;
		default:		/* invalid file type -- log an error */
			pr_err("Invalid file type %d", filetype);
	}
	return(status);
}

void
garbage_collect(char *mntdir, char *backmnt)
{
	char *fsidp;
	char cacheid[PATH_MAX];
	char *cp;

	fsidp = strrchr(backmnt, '/');
	if (!fsidp) {
		pr_err("Invalid back mount path %s", backmnt);
		return;
	}
	strcpy(cacheid, fsidp + 1);
	strcat(cacheid, ":");
	cp = &cacheid[strlen(cacheid)];
	strcat(cacheid, mntdir);
	while ((cp = strpbrk(cp, "/")) != NULL)
		*cp = '_';
	if (nftw(cacheid, checkfid, 5, FTW_PHYS) == -1) {
		if (errno != EINTR) {
			pr_err("nftw failed: %s", strerror(errno));
		}
	}
}

/* -----------------------------------------------------------------
 *
 *			main
 *
 * Description:
 *	Main routine for the cfsadmin program.
 * Arguments:
 *	argc	number of command line arguments
 *	argv	command line arguments
 * Returns:
 *	Returns 0 for failure, > 0 for an error.
 * Preconditions:
 */

int
main(int argc, char **argv)
{
	struct statvfs svb;
	struct mntent mget;
	struct mntent mref;
	FILE *finp;
	struct user_values uv;
	int c;
	int xx;
	int lockid;

	char *cacheid;
	char *dir;

	int gflag;
	int cflag;
	int uflag;
	int dflag;
	int allflag;
	int lflag;
	char *optionp;
	int convert = 0;

	Progname = *argv;

	/* verify root running command */
	if (getuid() != 0) {
		pr_err(gettext("must be run by root"));
		return (1);
	}

	(void) setlocale(LC_ALL, "");
#ifndef TEXT_DOMAIN
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

	/* set defaults for command line options */
	cflag = 0;
	uflag = 0;
	dflag = 0;
	allflag = 0;
	lflag = 0;
	gflag = 0;
	optionp = NULL;

	/* parse the command line arguments */
	while ((c = getopt(argc, argv, "Cgcuo:d:l")) != EOF) {
		switch (c) {

		case 'C':
			convert = 1;
			break;;

		case 'c':		/* create */
			cflag = 1;
			break;

		case 'u':		/* update */
			uflag = 1;
			break;

		case 'd':		/* delete */
			dflag = 1;
			if (!strcmp(optarg, "all"))
				allflag = 1;
			else
				cacheid = optarg;
			break;

		case 'l':		/* list cache ids */
			lflag = 1;
			break;

		case 'o':		/* options for update and create */
			optionp = optarg;
			break;

		case 'g':		/* garbage collection */
			gflag = 1;
			break;

		default:
			usage(gettext("illegal option"));
			return (1);
		}
	}

		/* make sure dir is specified */
		if (argc - 1 != optind) {
			usage(gettext("cache directory or mount point not specified"));
			return (1);
		}
		dir = argv[argc-1];

	/* make sure a reasonable set of flags were specified */
	if (convert + gflag + cflag + uflag + dflag + lflag != 1) {
		/* flags are mutually exclusive, at least one must be set */
		usage(gettext(
		    "exactly one of -C, -c, -u, -d, -l must be specified"));
		return (1);
	}

	/* make sure -o specified with -c or -u */
	if (optionp && !(cflag|uflag)) {
		usage(gettext("-o can only be used with -c or -u"));
		return (1);
	}

	/* if creating a cache */
	if (cflag) {
		/* get default cache paramaters */
		user_values_defaults(&uv);

		/* parse the options if specified */
		if (optionp) {
			xx = cfs_get_opts(optionp, &uv);
			if (xx)
				return (-1);
		}

		xx = create_cache(dir, &uv);
		if (xx != 0) {
			if (xx == -2) {
				/* remove a partially created cache dir */
				(void) delete_all_cache(dir);
			}
			return (1);
		}
	}

	/* else if updating resource parameters */
	else if (uflag) {
		/* lock the cache directory non-shared */
		lockid = cachefs_dir_lock(dir, 0);
		if (lockid == -1) {
			/* quit if could not get the lock */
			return (1);
		}

		xx = update_cachelabel(dir, optionp);
		cachefs_dir_unlock(lockid);
		if (xx != 0) {
			return (1);
		}
	}

	/* else if deleting a specific cacheID (or all caches) */
	else if (dflag) {
		/* lock the cache directory non-shared */
		lockid = cachefs_dir_lock(dir, 0);
		if (lockid == -1) {
			/* quit if could not get the lock */
			return (1);
		}

		/* if the cache is in use */
		if (cachefs_inuse(dir)) {
			pr_err(gettext("Cache %s is in use and cannot be modified."), dir);
			cachefs_dir_unlock(lockid);
			return (1);
		}

		if (allflag)
			xx = delete_all_cache(dir);
		else {
			xx = delete_cache(dir, cacheid);
		}
		cachefs_dir_unlock(lockid);
		if (xx != 0) {
			return (1);
		}
	}

	/* else if listing cache statistics */
	else if (lflag) {
		xx = cache_stats(dir);
		if (xx != 0)
			return (1);
	}

	/* else if garbage collecting */
	else if (gflag) {
		finp = setmntent(MOUNTED, "r");
		if (finp) {
			bzero(&mref, sizeof(mref));
			mref.mnt_dir = dir;
			mref.mnt_type = MNTTYPE_CACHEFS;
			xx = getmntany(finp, &mget, &mref);
			if (xx != -1) {
				if (statvfs(mget.mnt_fsname, &svb) == 0) {
					Fsid = svb.f_fsid;
					garbage_collect(dir, mget.mnt_fsname);
				} else {
					pr_err(gettext("Cannot stat %s: %s"), mget.mnt_fsname,
						strerror(errno));
				}
			} else {
				pr_err(gettext("%s not mounted"), dir);
			}
			endmntent(finp);
		}
	} else if (convert) {
		return(convert_old_cacheids(dir));
	}

	/* return success */
	return (0);
}


/* -----------------------------------------------------------------
 *
 *			usage
 *
 * Description:
 *	Prints a usage message for this utility.
 * Arguments:
 *	msgp	message to include with the usage message
 * Returns:
 * Preconditions:
 *	precond(msgp)
 */

void
usage(char *msgp)
{
	fprintf(stderr, gettext("cfsadmin: %s\n"), msgp);
	fprintf(stderr, gettext(
	    "usage: cfsadmin -[cu] [-o parameter-list] cachedir\n"));
	fprintf(stderr, gettext("       cfsadmin -d [CacheID|all] cachedir\n"));
	fprintf(stderr, gettext("       cfsadmin -l cachedir\n"));
	fprintf(stderr, gettext("       cfsadmin -g mntpoint\n"));
}

/* -----------------------------------------------------------------
 *
 *			cfs_get_opts
 *
 * Description:
 *	Decodes cfs options specified with -o.
 *	Only the fields referenced by the options are modified.
 * Arguments:
 *	oarg	options from -o option
 *	uvp	place to put options
 * Returns:
 *	Returns 0 for success, -1 for an error.
 * Preconditions:
 *	precond(oarg)
 *	precond(uvp)
 */

int
cfs_get_opts(char *oarg, struct user_values *uvp)
{
	char *optstr, *opts, *val;
	char *saveopts;
	int badopt;

	/* make a copy of the options because getsubopt modifies it */
	optstr = opts = strdup(oarg);
	if (opts == NULL) {
		pr_err(gettext("no memory"));
		return (-1);
	}

	/* process the options */
	badopt = 0;
	while (*opts && !badopt) {
		saveopts = opts;
		switch (getsubopt(&opts, cfsadmin_opts, &val)) {
		case COPT_MAXBLOCKS:
			if (badpercent(val))
				badopt = 1;
			else
				uvp->uv_maxblocks = atoi(val);
			break;

		case COPT_MAXFILES:
			if (badpercent(val))
				badopt = 1;
			else
				uvp->uv_maxfiles = atoi(val);
			break;

		case COPT_HIBLOCKS:
			if (badpercent(val))
				badopt = 1;
			else
				uvp->uv_hiblocks = atoi(val);
			break;
		case COPT_LOWBLOCKS:
			if (badpercent(val))
				badopt = 1;
			else
				uvp->uv_lowblocks = atoi(val);
			break;
		case COPT_HIFILES:
			if (badpercent(val))
				badopt = 1;
			else
				uvp->uv_hifiles = atoi(val);
			break;
		case COPT_LOWFILES:
			if (badpercent(val))
				badopt = 1;
			else
				uvp->uv_lowfiles = atoi(val);
			break;
		default:
			/* if a bad option argument */
			pr_err(gettext("Invalid option %s"), saveopts);
			return (-1);
		}
	}

	/* if a bad value for an option, display an error message */
	if (badopt) {
		pr_err(gettext("invalid argument to option: \"%s\""),
		    saveopts);
	}

	/* free the duplicated option string */
	free(optstr);

	/* return the result */
	return (badopt ? -1 : 0);
}

/* -----------------------------------------------------------------
 *
 *			update_cachelabel
 *
 * Description:
 *	Changes the parameters of the cache_label.
 *	If optionp is NULL then the cache_label is set to
 *	default values.
 * Arguments:
 *	dirp		the name of the cache directory
 *	optionp		comma delimited options
 * Returns:
 *	Returns 0 for success and -1 for an error.
 * Preconditions:
 *	precond(dirp)
 */

int
update_cachelabel(char *dirp, char *optionp)
{
	char path[XMAXPATH];
	struct cache_label clabel_new;
	struct cache_label clabel_orig;
	struct user_values uv_orig, uv_new;
	int xx;

	/* if the cache is in use */
	if (cachefs_inuse(dirp)) {
		pr_err(gettext("Cache %s is in use and cannot be modified."),
		    dirp);
		return (-1);
	}

	/* make sure we don't overwrite path */
	if (strlen(dirp) > (size_t)PATH_MAX) {
		pr_err(gettext("name of label file %s is too long."),
		    dirp);
		return (-1);
	}

	/* construct the pathname to the cach_label file */
	sprintf(path, "%s/%s", dirp, CACHELABEL_NAME);

	/* read the current set of parameters */
	xx = cachefs_label_file_get(path, &clabel_orig);
	if (xx == -1) {
		pr_err(gettext("reading %s failed"), path);
		return (-1);
	}

	clabel_new = clabel_orig;

	/* convert the cache_label to user values */
	xx = convert_cl2uv(&clabel_orig, &uv_orig, dirp);
	if (xx) {
		return (-1);
	}

	/* if options were specified */
	if (optionp) {
		/* start with the original values */
		uv_new = uv_orig;

		/* parse the options */
		xx = cfs_get_opts(optionp, &uv_new);
		if (xx) {
			return (-1);
		}

		/* verify options are reasonable */
		xx = check_user_values_for_sanity(&uv_new);
		if (xx) {
			return (-1);
		}
	}

	/* else if options where not specified, get defaults */
	else {
		user_values_defaults(&uv_new);
	}

	/* convert user values to a cache_label */
	xx = convert_uv2cl(&uv_new, &clabel_new, dirp);
	if (xx) {
		return (-1);
	}

	/* write back the new values */
	xx = cachefs_label_file_put(path, &clabel_new);
	if (xx == -1) {
		pr_err(gettext("writing %s failed"), path);
		return (-1);
	}

	/* put the new values in the duplicate cache label file also */
	sprintf(path, "%s/%s.dup", dirp, CACHELABEL_NAME);
	xx = cachefs_label_file_put(path, &clabel_new);
	if (xx == -1) {
		pr_err(gettext("writing %s failed"), path);
		return (-1);
	}

	/* return status */
	return (xx);
}

/* -----------------------------------------------------------------
 *
 *			cache_stats
 *
 * Description:
 *	Show each cache in the directory, cache resource statistics,
 *	and, for each fs in the cache, the name of the fs, and the
 *	cache resource parameters.
 * Arguments:
 *	dirp	name of the cache directory
 * Returns:
 *	Returns 0 for success, -1 for an error.
 * Errors:
 * Preconditions:
 */

int
cache_stats(char *dirp)
{
	DIR *dp;
	struct dirent *dep;
	char path[XMAXPATH];
	struct stat statinfo;
	int ret;
	int xx;
	struct cache_label clabel;
	struct user_values uv;
	struct statvfs svb;

	/* make sure cache dir name is not too long */
	if (strlen(dirp) > (size_t)PATH_MAX) {
		pr_err(gettext("path name %s is too long."), dirp);
		return (-1);
	}

	if (statvfs(dirp, &svb) == -1) {
		pr_err(gettext("statvfs failed for %s."), dirp);
		return (-1);
	}

	/* read the cache label file */
	sprintf(path, "%s/%s", dirp, CACHELABEL_NAME);
	xx = cachefs_label_file_get(path, &clabel);
	if (xx == -1) {
		pr_err(gettext("Reading %s failed."), path);
		return (-1);
	}

	/* convert the cache_label to user values */
	xx = convert_cl2uv(&clabel, &uv, dirp);
	if (xx)
		return (-1);

	/* display the parameters */
	printf(gettext("cfsadmin: list cache FS information\n"));
	printf(gettext("   Version      %3d %3d %3d\n"),
		(unsigned int)clabel.cl_cfsversion, (unsigned int)clabel.cl_cfslongsize,
		(unsigned int)clabel.cl_cfsmetasize);
	/*
	 * report the block limits in 512 byte units for consistency with
	 * df
	 */
	printf(gettext("   maxblocks    %3d%% (%d blocks)\n"), uv.uv_maxblocks,
		clabel.cl_maxblks * (svb.f_frsize / 512));
	printf(gettext("    hiblocks    %3d%% (%d blocks)\n"), uv.uv_hiblocks,
		clabel.cl_blkhiwat * (svb.f_frsize / 512));
	printf(gettext("   lowblocks    %3d%% (%d blocks)\n"), uv.uv_lowblocks,
		clabel.cl_blklowat * (svb.f_frsize / 512));
	printf(gettext("   maxfiles     %3d%% (%d files)\n"), uv.uv_maxfiles,
		clabel.cl_maxfiles);
	printf(gettext("    hifiles     %3d%% (%d files)\n"), uv.uv_hifiles,
		clabel.cl_filehiwat);
	printf(gettext("   lowfiles     %3d%% (%d files)\n"), uv.uv_lowfiles,
		clabel.cl_filelowat);

	/* open the directory */
	if ((dp = opendir(dirp)) == NULL) {
		pr_err(gettext("opendir %s failed: %s"), dirp,
		    strerror(errno));
		return (-1);
	}

	/* loop reading the contents of the directory */
	ret = 0;
	while ((dep = readdir(dp)) != NULL) {
		/* ignore . and .. */
		/*
		 * all cache ID names begin with CACHEID_PREFIX
		 */
		if (strncmp(dep->d_name, CACHEID_PREFIX, CACHEID_PREFIX_LEN) == 0) {

			/* stat the file */
			sprintf(path, "%s/%s", dirp, dep->d_name);
			if (lstat(path, &statinfo) == -1) {
				pr_err(gettext("lstat %s failed: %s"),
				    path, strerror(errno));
				closedir(dp);
				return (-1);
			}
			if (S_ISDIR(statinfo.st_mode)) {
				/* print the file system cache directory name */
				printf(gettext("  %s\n"), dep->d_name);
			}
		}

	}
	closedir(dp);

	/* return status */
	return (ret);
}

char *
get_fsname(char *cacheid)
{
	char *cp;
	char *last_colon;
	int colons;
	char *fsname = cacheid;

	/*
	 * count the colons and point at the last one
	 */
	for (cp = cacheid, colons = 0, last_colon = NULL;
		cp = strchr(cp, ':');
		last_colon = cp, colons++, cp++)
			;
	switch (colons) {
		case 1:
		case 2:
			assert(last_colon);
			fsname = malloc(last_colon - cacheid + 1);
			if (!fsname) {
				return(NULL);
			}
			strncpy(fsname, cacheid, last_colon - cacheid);
			fsname[last_colon - cacheid] = '\0';
			break;
		default:
			printf("Old cache IDs are of the form <fsname>:<mountpoint>\n");
			printf("The fsname could not be ddetermined for %s.\n", cacheid);
			fsname = malloc(strlen(cacheid) + 1);
			if (!fsname) {
				return(NULL);
			}
			printf("Please enter the fsname: ");
			fsname = fgets(fsname, strlen(cacheid), stdin);
	}
	return(fsname);
}

int
convert_old_cacheids(char *dirp)
{
	int repeat_question;
	DIR *dp;
	struct dirent *dep;
	char newname[MAXNAMELEN];
	char *fsname;
	struct stat statinfo;

	printf("Converting old cache IDs to new ones for cache %s.\n", dirp);
	if (chdir(dirp) == -1) {
		pr_err(gettext("Could not chdir to %s: %s"), dirp,
		    strerror(errno));
		return(-1);
	}
	/* open the directory */
	if ((dp = opendir(".")) == NULL) {
		pr_err(gettext("opendir failed for %s: %s"), dirp,
		    strerror(errno));
		return(-1);
	}

	/* loop reading the contents of the directory */
	while ((dep = readdir(dp)) != NULL) {
		/*
		 * skip ., .., and BACKMNT_NAME
		 */
		if ((strcmp(dep->d_name, ".") == 0) ||
			(strcmp(dep->d_name, "..") == 0) ||
			(strcmp(dep->d_name, BACKMNT_NAME) == 0)) {
				continue;
		}
		/*
		 * all cache ID names begin with CACHEID_PREFIX
		 */

		/* stat the file */
		if (lstat(dep->d_name, &statinfo) == -1) {
			pr_err(gettext("lstat %s failed: %s"),
			    dep->d_name, strerror(errno));
			closedir(dp);
			return(-1);
		}
		if (S_ISDIR(statinfo.st_mode) &&
			(strncmp(dep->d_name, CACHEID_PREFIX, CACHEID_PREFIX_LEN) != 0)) {
				fsname = get_fsname(dep->d_name);
				if (!fsname) {
					pr_err(gettext("Unable to get fsname for %s"), dep->d_name);
					closedir(dp);
					return(-1);
				}
				strcpy(newname, CACHEID_PREFIX);
				strcat(newname, fsname);
				if (rename(dep->d_name, newname) == -1) {
					if (errno == EEXIST) {
						printf("Failed to rename %s to %s\n", dep->d_name,
							newname);
						printf("%s exists and is not empty.\n", newname);
						do {
							repeat_question = 0;
							printf("Remove %s (y(es), n(o), or q(uit))? ",
								newname);
							switch (getchar()) {
								case 'y':
								case 'Y':
									if (delete_cache(dirp, newname) == -1) {
										closedir(dp);
										return(-1);
									}
									if (rename(dep->d_name, newname) == -1) {
										pr_err(gettext(
											"Unable to rename %s to %s: %s"),
											dep->d_name, newname,
											strerror(errno));
										closedir(dp);
										return(-1);
									} else {
										printf("%s renamed to %s\n",
											dep->d_name, newname);
									}
									break;
								case 'n':
								case 'N':
									do {
										repeat_question = 0;
										printf("\nRemove %s (y(es) or n(o))? ",
											dep->d_name);
										switch (getchar()) {
											case 'y':
											case 'Y':
												if (delete_cache(dirp,
													dep->d_name) == -1) {
														closedir(dp);
														return(-1);
												} else {
													printf("%s removed\n",
														dep->d_name);
												}
												break;
											case 'n':
											case 'N':
												strcpy(newname, CACHEID_PREFIX);
												strcat(newname, dep->d_name);
												if (rename(dep->d_name,
													newname) == -1) {
														pr_err(gettext(
															"Unable to rename "
															"%s to %s: %s"),
															dep->d_name,
															newname,
															strerror(errno));
														closedir(dp);
														return(-1);
												} else {
													printf("%s renamed to %s\n",
														dep->d_name, newname);
												}
												break;
											default:
												repeat_question = 1;
										}
									} while (repeat_question);
									break;
								case 'q':
								case 'Q':
									closedir(dp);
									return(0);
								default:
									repeat_question = 1;
							}
							putchar('\n');
						} while (repeat_question);
					} else {
						pr_err(gettext("Unable to rename %s to %s: %s"),
							dep->d_name, newname, strerror(errno));
						closedir(dp);
						return(-1);
					}
				} else {
					printf("%s renamed to %s\n", dep->d_name, newname);
				}
				rewinddir(dp);
		}
	}
	closedir(dp);
	return(0);
}
