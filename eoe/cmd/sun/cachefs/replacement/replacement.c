/* -----------------------------------------------------------------
 *
 *			replacement.c
 *
 * Cache FS replacement daemon.
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
#include <ftw.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mman.h>
#include <time.h>
#include <mntent.h>
/* #include <sys/dir.h> */
#include <sys/syssgi.h>
#include <cachefs/cachefs_fs.h>
#include "subr.h"
#include <syslog.h>

#undef ASSERT
#ifdef DEBUG
#define ASSERT(E)	assert(E)
#else /* DEBUG */
#define ASSERT(E)
#endif /* DEBUG */

#define CACHEFS_REPLACEMENT_LOCK   ".cfs_replacement_lock"

#define TIMECMP(t1, t2) ((t1) - (t2))

#define USEC_TO_SEC(usec)	(usec / 1000000)

#define TV_TO_SEC(tv)	((tv).tv_sec + USEC_TO_SEC((tv).tv_usec))

#define MAXLEVEL			32

/*
 * replacement list entries
 */
struct reprecord {
	char				*rep_path;
	time_t				rep_mtime;
	time_t				rep_atime;
	long				rep_blocks;
	int					rep_level;
	struct reprecord	**rep_next;
};

typedef struct reprecord reprec_t;

struct list {
	reprec_t	list_head;
	int			list_levels;
};

typedef struct list list_t;

double Prob = 0.5;
char *Progname;
int File_count = 0;
int Block_count = 0;
char *Cachedir;
char *Hexdigits = "0123456789abcdef";
int Verbose = 0;
struct cache_label Label;
long Reconstruct_threshold = 600;

extern int errno;

#ifdef DEBUG
int Page_size = 0;
int Sigsegv = 0;

#undef ROUNDUP
#define ROUNDDOWN( x, y )   ( ((x) / (y)) * (y) )
#define ROUNDUP( x, y )     ROUNDDOWN( (x) + (y) - 1, (y) )

void
sigsegv_handler( int sig, int code, struct sigcontext *scp )
{
	Sigsegv = 1;
}

/*
 * check all of the pages in the specified range for validity
 * only check for readability
 * check by attempting to read the first byte of each page
 * catch SIGSEGV to identify an invalid page without killing the program
 * this has the side effect of loading all of the pages into memory
 */
int
test_pages( caddr_t start, int len )
{
	caddr_t test_addr;
	char ch;
	void *func;

	Sigsegv = 0;
	func = signal( SIGSEGV, sigsegv_handler );
	for ( test_addr = start;
		!Sigsegv && (test_addr < start + len);
		test_addr += Page_size ) {
			ch = *(char *)test_addr;
	}
	signal( SIGSEGV, func );
	return( !Sigsegv );
}

int
valid_addresses( caddr_t addr, int len )
{
	int valid;
	caddr_t start_page;
	int page_bytes;

	if ( !Page_size ) {
		Page_size = getpagesize();
	}
	start_page = (caddr_t)ROUNDDOWN( (u_long)addr, (u_long)Page_size );
	page_bytes = (int)ROUNDUP( (u_long)len, (u_long)Page_size );
	/*
	 * if the address addr is NULL, it is invalid
	 * otherwise, use mincore to check the validity
	 */
	if ( !addr ) {
		valid = 0;
	} else if ( !test_pages( start_page, page_bytes ) ) {
		if ( errno == ENOMEM ) {
			fprintf( stderr, "%s: bad address range [0x%x, 0x%x)\n", Progname,
				addr, addr + len );
		} else {
			fprintf( stderr, "%s: ", Progname );
			perror( "valid_addresses: mincore" );
		}
		valid = 0;
	} else {
		valid = 1;
	}
	return( valid );
}
#endif /* DEBUG */

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
	fprintf(stderr, gettext("cachefs_replacement: %s\n"), msgp);
	fprintf(stderr, gettext("usage: cachefs_replacement cachedir\n"));
}

/* -----------------------------------------------------------------
 *
 *			replacement_lock
 *
 * Description:
 *	Lock out other replacement daemons.  There should only be
 *	One for each cache directory.  Locking is done via fcntl.  If the
 *	lock file cannot be locked, another daemon is already running.
 * Arguments:
 *	cachedir	cache directory for which this daemon is to be responsible
 * Returns:
 * Preconditions:
 *	precond(cachedir)
 */

int
replacement_lock(char *cachedir)
{
	int fd;
	struct flock fl;

	ASSERT(cachedir);
	/* create and open the cache directory lock file */
	fd = open(CACHEFS_REPLACEMENT_LOCK, O_RDWR | O_CREAT, 0700);
	if (fd == -1) {
		log_err(gettext("(%s): Cannot open lock file %s: %s"), Cachedir,
			CACHEFS_REPLACEMENT_LOCK, strerror(errno));
		return (-1);
	}

	/* try to set the lock */
	fl.l_type = F_WRLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_sysid = 0;
	fl.l_pid = 0;
	if (fcntl(fd, F_SETLK, &fl) == -1) {
		switch (errno) {
			case EACCES:
			case EAGAIN:
				break;
			default:
				log_err(gettext("(%s): lock error %s on %s: %s"), Cachedir,
					strerror(errno), CACHEFS_REPLACEMENT_LOCK, strerror(errno));
		}
		close(fd);
		fd = -1;
	}

	/* return the file descriptor which can be used to release the lock */
	return (fd);
}

/* -----------------------------------------------------------------
 *
 *			replacement_unlock
 *
 * Description:
 *	Unlock the replacement daemon lock file to allow another daemon
 *	to run.  This should be done before exiting.
 * Arguments:
 *	fd	file descriptor for the lock file
 * Returns:
 * Preconditions:
 *	precond(cachedir)
 */

void
replacement_unlock(int fd)
{
	struct flock fl;

	/* release the lock */
	fl.l_type = F_UNLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_sysid = 0;
	fl.l_pid = 0;
	if (fcntl(fd, F_SETLK, &fl) == -1) {
		log_err(gettext("(%s): unlock error %s on %s"), Cachedir,
			strerror(errno), CACHEFS_REPLACEMENT_LOCK);
	}

	/* close the lock file */
	close(fd);
}

reprec_t *
newrec(int level, char *path, struct stat *sbp)
{
	reprec_t *rec;

	rec = (reprec_t *)malloc(sizeof(reprec_t));
	if (rec) {
		rec->rep_next = (reprec_t **)calloc(level, sizeof(reprec_t *));
		if (rec->rep_next) {
			rec->rep_path = path;
			rec->rep_mtime = sbp->st_mtime;
			rec->rep_atime = sbp->st_atime;
			rec->rep_blocks = sbp->st_blocks;
			rec->rep_level = level;
			return(rec);
		} else {
			free(rec);
		}
	}
	return(NULL);
}

void
free_rec(reprec_t *reprec)
{
	if (reprec->rep_path) {
		free(reprec->rep_path);
	}
	free(reprec->rep_next);
	free(reprec);
}

int
random_level(void)
{
	int newlevel = 1;
	while (drand48() < Prob) {
		newlevel++;
	}
	return((newlevel > MAXLEVEL) ? MAXLEVEL : newlevel);
}

/* -----------------------------------------------------------------
 *
 *			addfile
 *
 * Description:
 *  Insert a replacement record into the replacement list.  The list
 *  is maintained sorted by mod/access time.
 *	A skip list is used to keep the insertion cost reasonable.
 * Arguments:
 *  reprec	pointer to record to be inserted
 * Returns:
 * Preconditions:
 *	precond(list)
 *	precond(path)
 *	precond(sbp)
 */

int
addfile(list_t *listp, char *path, struct stat *sbp)
{
	int status = 0;
	reprec_t *rp;
	reprec_t *update[MAXLEVEL];
	int i;
	int newlevel;

	/*
	 * find the position in the list starting with the highest existing
	 * level
	 */
	rp = &listp->list_head;
	for (i = listp->list_levels - 1; i >= 0; i--) {
		while (rp->rep_next[i] &&
			((rp->rep_next[i]->rep_mtime < sbp->st_mtime) ||
			(rp->rep_next[i]->rep_atime < sbp->st_atime))) {
				rp = rp->rep_next[i];
		}
		update[i] = rp;
	}
	/*
	 * insert a new record, assigning it a random level
	 * the insertion point is given by update which points to the record
	 * which should precede the new one
	 */
	newlevel = random_level();
	rp = newrec(newlevel, path, sbp);
	if (rp) {
		if (newlevel > listp->list_levels) {
			for (i = listp->list_levels; i < newlevel; i++) {
				update[i] = &listp->list_head;
			}
			listp->list_levels = newlevel;
		}
		for (i = 0; i < newlevel; i++) {
			rp->rep_next[i] = update[i]->rep_next[i];
			update[i]->rep_next[i] = rp;
		}
		File_count++;
		Block_count += sbp->st_blocks;
	} else {
		status = -1;
		errno = ENOMEM;
	}
	return(status);
}

/*
 * pop the first item off the list
 * it is up to the caller to free the item
 */
reprec_t *
popfile(list_t *list)
{
	reprec_t *rep = NULL;
	int i;

	if (list && (rep = list->list_head.rep_next[0])) {
		for (i = rep->rep_level - 1; i >= 0; i--) {
			list->list_head.rep_next[i] = rep->rep_next[i];
		}
	}
	return(rep);
}

int
construct_file_list(char *cachedir, list_t *list, time_t *ltime)
{
	char *filepath = NULL;
	int status = 0;
	DIR *cachedp = NULL;
	DIR *cacheiddp = NULL;
	DIR *dp = NULL;
	struct dirent *cache_dep;
	struct dirent *cacheid_dep;
	struct dirent *dep;
	char path[MAXPATHLEN];
	struct stat statinfo;

	/*
	 * go through all of the cache IDs and make a list of all
	 * regular files in the cache sorted by access time
	 */

	File_count = Block_count = 0;
	if (chdir(cachedir) == -1) {
		pr_err(gettext("chdir %s failed: %s"), cachedir,
		    strerror(errno));
		return (-1);
	}
	/* open the cache directory */
	if ((cachedp = opendir(".")) == NULL) {
		pr_err(gettext("opendir %s failed: %s"), cachedir,
		    strerror(errno));
		return (-1);
	}

	/* loop reading the contents of the directory */
	while ((cache_dep = readdir(cachedp)) != NULL) {
		/* ignore . and .. */
		/*
		 * all cache ID names begin with CACHEID_PREFIX
		 */
		if (!strncmp(cache_dep->d_name, CACHEID_PREFIX, CACHEID_PREFIX_LEN)) {

			/* try to chdir */
			sprintf(path, "%s/%s", cachedir, cache_dep->d_name);
			if (chdir(cache_dep->d_name) == -1) {
				if (errno == ENOTDIR) {
					continue;
				}
				pr_err(gettext("chdir %s failed: %s"),
				    path, strerror(errno));
				status = -1;
				break;
			}
			/* open the cacheid directory */
			if ((cacheiddp = opendir(".")) == NULL) {
				pr_err(gettext("opendir %s failed: %s"), path,
					strerror(errno));
				status = -1;
				break;
			}

			/* loop reading the contents of the directory */
			while ((cacheid_dep = readdir(cacheiddp)) != NULL) {
				/* ignore . and .. and root */
				if (((cacheid_dep->d_name[0] == '.') &&
					((cacheid_dep->d_name[1] == '\0') ||
					((cacheid_dep->d_name[1] == '.') &&
					(cacheid_dep->d_name[2] == '\0')))) ||
					(strcmp(cacheid_dep->d_name, "root") == 0))
						continue;

				/* stat the file */
				sprintf(path, "%s/%s/%s", cachedir, cache_dep->d_name,
					cacheid_dep->d_name);
				if (chdir(cacheid_dep->d_name) == -1) {
					if (errno == ENOTDIR) {
						continue;
					}
					pr_err(gettext("chdir %s failed: %s"),
					    path, strerror(errno));
					status = -1;
					break;;
				}
				/* open the directory */
				if ((dp = opendir(".")) == NULL) {
					pr_err(gettext("opendir %s failed: %s"), path,
						strerror(errno));
					status = -1;
					goto out;
				}

				/* loop reading the contents of the directory */
				while ((dep = readdir(dp)) != NULL) {
					/* ignore . and .. */
					if ((dep->d_name[0] == '.') &&
						((dep->d_name[1] == '\0') ||
						((dep->d_name[1] == '.') &&
						(dep->d_name[2] == '\0'))))
							continue;

					/* stat the file */
					filepath = malloc(strlen(path) + strlen(dep->d_name) + 2);
					if (!filepath) {
						pr_err(gettext("malloc failed"));
						status = -1;
						goto out;
					}
					strcpy(filepath, path);
					strcat(filepath, "/");
					strcat(filepath, dep->d_name);
					if (stat(dep->d_name, &statinfo) == -1) {
						pr_err(gettext("lstat %s failed: %s"),
						    path, strerror(errno));
						status = -1;
						goto out;
					}

					/*
					 * only process regular files at this level
					 */
					if (S_ISREG(statinfo.st_mode) &&
						(addfile(list, filepath, &statinfo) == -1)) {
							status = -1;
							goto out;
					}
				}
				if (chdir("..") == -1) {
					pr_err(gettext("chdir .. failed: %s"), strerror(errno));
					status = -1;
					goto out;
				}
				closedir(dp);
				dp = NULL;
			}
			if (chdir("..") == -1) {
				pr_err(gettext("chdir .. failed: %s"), strerror(errno));
				status = -1;
				goto out;
			}
			closedir(cacheiddp);
			cacheiddp = NULL;
		}
	}
out:
	/*
	 * record the list construction time
	 */
	*ltime = time(NULL);
	if (chdir("/") == -1) {
		pr_err(gettext("chdir / failed: %s"), strerror(errno));
		status = -1;
	}
	if (cachedp) {
		closedir(cachedp);
	}
	if (cacheiddp) {
		closedir(cacheiddp);
	}
	if (dp) {
		closedir(dp);
	}
	return(status);
}

void
init_list(list_t *list)
{
	bzero(list, sizeof(list_t));
	list->list_head.rep_next = (reprec_t **)calloc(MAXLEVEL,
		sizeof(reprec_t *));
}

void
free_list(list_t *listp)
{
	reprec_t *rec;

	while (rec = popfile(listp)) {
		free_rec(rec);
	}
	bzero(listp->list_head.rep_next, MAXLEVEL * sizeof(reprec_t *));
	listp->list_levels = 0;
}

int
replace_files(char *cachedir, replarg_t *rap, list_t *list,
	ulong_t blocks_needed, ulong_t files_needed)
{
	int keep;
	cachefsmeta_t meta;
	ulong_t blocks;
	int fd;
	int j;
	int keepcount = 0;
	int i;
	int entries = 0;
	reprec_t *repp;
	reprec_t *prev;
	reprec_t *next;

	if (rap->ra_list) {
		for (i = 0; i < rap->ra_ents; i++) {
			if (rap->ra_list[i].rep_path) {
				free(rap->ra_list[i].rep_path);
			}
		}
		free(rap->ra_list);
		rap->ra_list = NULL;
		rap->ra_ents = 0;
	}
	i = 0;
	while ((blocks_needed || files_needed) && (repp = popfile(list))) {
		ASSERT(valid_addresses((caddr_t)repp, sizeof(*repp)));
		ASSERT(valid_addresses(repp->rep_path, 1));
		entries++;
		fd = open(repp->rep_path, O_RDONLY, 0);
		keep = 0;
		if (fd >= 0) {
			if ((read(fd, &meta, sizeof(meta)) == sizeof(meta)) &&
				(meta.md_state & MD_KEEP)) {
					keep = 1;
			}
			close(fd);
		}
		if (!keep) {
			/*
			 * each entry represents one file
			 */
			if (files_needed) {
				files_needed--;
			}
			blocks = repp->rep_blocks;
			unlink(repp->rep_path);
		} else if (rap->ra_list) {
			rap->ra_list = (cachefsrep_t *)realloc(rap->ra_list,
					(i + 1) * sizeof(cachefsrep_t));
			rap->ra_list[i].rep_path = repp->rep_path;
			/*
			 * point to the cacheid in the path string
			 * the cacheid is the first path component immediately
			 * following the cachedir component
			 */
			rap->ra_list[i].rep_cacheid = (char *)(strlen(cachedir) + 1);
			/*
			 * clear rep_path so that free_rec
			 * won't free the data
			 */
			repp->rep_path = NULL;
			i++;
			if (repp->rep_blocks > 0) {
				blocks = repp->rep_blocks - 1;
			} else {
				blocks = 0;
			}
		}
		/*
		 * account for the blocks allocated to this file
		 */
		if (blocks_needed <= blocks) {
			blocks_needed = 0;
		} else {
			blocks_needed -= blocks;
		}
		free_rec(repp);
	}
	rap->ra_ents = i;
	return(entries);
}

void
calculate_needs(char *cachedir, struct cache_label *label, ulong_t *min_blocks,
	ulong_t *max_blocks, ulong_t *min_files, ulong_t *max_files)
{
	ulong_t blocks_used;
	ulong_t files_used;
	struct statvfs svb;
	int status;

	ASSERT(cachedir);
	if (statvfs(cachedir, &svb) == -1) {
		if (errno == EINTR) {
			status = 0;
		} else {
			log_err("(%s): Unable to statvfs cache directory: %s",
				cachedir, strerror(errno));
			status = 1;
		}
		exit(status);
	}
	/*
	 * if the number of files used on the front file system is
	 * greater than the high water mark, remove files until we
	 * get down to the low water mark
	 */
	files_used = svb.f_files - svb.f_ffree;
	if (files_used > Label.cl_filehiwat) {
		*max_files = files_used - Label.cl_filelowat;
		*min_files = files_used - Label.cl_filehiwat;
	} else {
		*min_files = *max_files = 0;
	}
	/*
	 * if the number of blocks used on the front file system is
	 * greater than the high water mark, remove/truncate files until we
	 * get down to the low water mark
	 */
	blocks_used = svb.f_blocks - svb.f_bfree;
	if (blocks_used > Label.cl_blkhiwat) {
		*max_blocks = blocks_used - Label.cl_blklowat;
		*min_blocks = blocks_used - Label.cl_blkhiwat;
		/*
		 * convert to 512-byte blocks if necessary
		 */
		if ((svb.f_frsize > 0) && (svb.f_frsize != 512)) {
			*max_blocks = (*max_blocks * svb.f_frsize) / 512;
			*min_blocks = (*min_blocks * svb.f_frsize) / 512;
		}
	} else {
		*max_blocks = *min_blocks = 0;
	}
	ASSERT(*min_blocks <= *max_blocks);
	ASSERT(*max_blocks <= svb.f_blocks);
	ASSERT(*min_files <= *max_files);
	ASSERT(*max_files <= svb.f_files);
}

void
signal_handler(int sig)
{
	log_err("Cachefs replacement daemon for %s exiting on signal %d\n",
		Cachedir, sig);
	if ((sig == SIGINT) || (sig == SIGTERM) || (sig == SIGQUIT)) {
		exit(1);
	}
	abort();
}

/*
 * replacement
 *
 * Algorithm:
 *
 * 1. calculate needed files and/or blocks
 * 2. construct file list sorted by access/mod time and record
 *    file and block counts
 * 3. split list into files to be replaced and those not
 * 4. pass replacement list to kernel, keep remaining list
 * 5. wait for a replacement request or the timer, terminate on error
 */
int
replacement(char *cachedir, struct cache_label *label)
{
	time_t ltime = 0;
	int files_needed = 0;
	int blocks_needed = 0;
	replarg_t reparg;
	int term = 0;
	ulong_t min_blocks_needed = 0;
	ulong_t max_blocks_needed = 0;
	ulong_t min_files_needed = 0;
	ulong_t max_files_needed = 0;
	list_t file_list;
	time_t sys_start;

	bzero(&reparg, sizeof(reparg));
	reparg.ra_cachedir = cachedir;
	init_list(&file_list);
	/*
	 * set the timer value to zero
	 * this will cause the kernel to use its default timeout value
	 * set in var/sysgen/mtune/cachefs
	 */
	reparg.ra_timeout = 0;
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGILL, signal_handler);
	do {
		/*
		 * 1. calculate needed files and/or blocks
		 */
		calculate_needs(cachedir, label, &min_blocks_needed,
			&max_blocks_needed, &min_files_needed, &max_files_needed);
		if (Verbose) {
			syslog(LOG_DEBUG, "[%d, %d] blocks and [%d, %d] files needed\n",
				min_blocks_needed, max_blocks_needed, min_files_needed,
				max_files_needed);
		}
		/*
		 * 2. if the file list is empty, construct it sorted by
		 *    access/mod time and record file and block counts
		 */
		if (Verbose) {
			syslog(LOG_DEBUG, "constructing file list\n");
		}
		if ((!file_list.list_head.rep_next[0] ||
			((((long)time(NULL) - (long)ltime) > Reconstruct_threshold) &&
			!(min_blocks_needed || min_files_needed))) &&
			(construct_file_list(cachedir, &file_list, &ltime) == -1)) {
				if (errno == EINTR) {
					return(0);
				}
				log_err("(%s): Replacement failed: %s", Cachedir,
					strerror(errno));
		}
		/*
		 * 3. replace or truncate the necessary files
		 */
		File_count -= replace_files(cachedir, &reparg, &file_list,
			max_blocks_needed, max_files_needed);
		/*
		 * 4. wait for a replacement request, terminate on error
		 */
		/*
		 * record the system call start time so we can estimate the
		 * timeout that is being used by the kernel
		 */
		sys_start = time(NULL);
		if (syssgi(SGI_CACHEFS_SYS, CACHEFSSYS_REPLACEMENT, &reparg) == -1) {
			/*
			 * always terminate on error from syssgi
			 */
			switch (errno) {
				case ETIMEDOUT:
					if (Verbose) {
						syslog(LOG_DEBUG, "Replacement timeout\n");
					}
					/*
					 * set the reconstruction threshold to be the timeout
					 */
					Reconstruct_threshold = (long)time(NULL) - (long)sys_start;
					free_list(&file_list);
					break;
				case EINTR:
				case ESTALE:
					return(0);
				case ENOENT:
					log_err("(%s): No file systems mounted on cache",
						cachedir);
					return(0);
				default:
					log_err("(%s): cachefs_replacement failed: %s",
						cachedir, strerror(errno));
					return(-1);
			}
		}
	} while (1);
	/* NOTREACHED */
}

int
main(int argc, char **argv)
{
	char *dirname;
	int status = 0;
	int lockfd;
	int i;
	char dirpath[MAXPATHLEN];
	int c;
	char log_id[MAXPATHLEN];
	extern char *optarg;

	Progname = argv[0];

	/* verify root running command */
	if (getuid() != 0) {
		log_err(gettext("(%s): must be run by root"), Cachedir);
		return (1);
	}

	(void) setlocale(LC_ALL, "");

	while ((c = getopt(argc, argv, "vt:")) != EOF) {
		switch (c) {
			case 'v':
				Verbose++;
				break;
			default:
				usage(gettext("illegal option"));
				return(1);
		}
	}

	if (optind >= argc) {
		usage("Cache directory must be sxpecified.");
		return(1);
	}

	Cachedir = argv[optind];

	if (chdir(Cachedir) == -1) {
		pr_err("chdir(%s): %s", Cachedir, strerror(errno));
		return(1);
	}

	if (_daemonize(_DF_NOFORK | _DF_NOCHDIR, (Verbose > 1) ? 2 : -1, -1, -1)
		== -1) {
			pr_err("daemonize: %s", strerror(errno));
			return(1);
	}

	sprintf(log_id, "%s(%s)", Progname, Cachedir);

	openlog(log_id, (Verbose > 1) ? LOG_PERROR : 0, LOG_DAEMON);
	setlogmask(LOG_UPTO(Verbose ? LOG_DEBUG : LOG_NOTICE));

	if (Verbose) {
		syslog(LOG_DEBUG, "replacement started for %s\n", Cachedir);
	}

	if ((lockfd = replacement_lock(Cachedir)) == -1) {
		return(0);
	}

	if (cachefs_label_file_get(CACHELABEL_NAME, &Label) == -1) {
		log_err("(%s): Unable to read cache label file", Cachedir);
		replacement_unlock(lockfd);
		return(-1);
	}
	status = replacement(Cachedir, &Label);
	replacement_unlock(lockfd);
	if (status == -1) {
		switch (fork()) {
			case 0:
				if ((status = execvp(Progname, argv)) == -1) {
					log_err("(%s): Unable to restart cachefs_replacement",
						Cachedir);
				}
				break;
			case -1:
				log_err("(%s): Unable to restart cachefs_replacement",
					Cachedir);
				break;
			default:
				break;
		}
	}
	return(status);
}
