/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 * $Revision: 1.10 $
 *
 * NAME
 *	rsync - synchronize files with their latest RCS revisions
 * SYNOPSIS
 *	rsync [-c command] [-v] [-r rcsdir] directory ...
 * DESCRIPTION
 *	Synchronize RCS working files with the top revisions in their
 *	version files.
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

/*
 * A bound on the number of file arguments to the synchronizer command.
 * There are at most two additional arguments: the command itself and a
 * verbosity option.
 */
#define	FILESPEREXEC	48
#define	ARGSPEREXEC	(2+FILESPEREXEC)

/* formatted message output, to consolidate output conventions */
enum id_type { USER, GROUP };

void	fmessage(char *, char *, char *, ...);
void	umessage(char *, char *, char *, enum id_type, int);

/* error handling declarations */
char	*progname;
int	perrorf(char *, ...);

/* global option variables */
int	recursive;		/* true if sync'ing file trees */
char	*vec[ARGSPEREXEC+1];	/* sync command argument vector */
int	count;			/* number of elements in vec */
int	first;			/* index of first argument in vec */
char	*rcsdir = "RCS";	/* name of RCS subdirectory */
int	verbose;		/* verbosity level */
int	maxfd;			/* file descriptor upper bound */

void	syncwd(char *);

main(int argc, char **argv)
{
	int opt, fd;
	char top[MAXPATHLEN];
	extern char *optarg;
	extern int optind;

	/*
	 * Process options.
	 */
	progname = argv[0];
	while ((opt = getopt(argc, argv, "Rc:r:v")) != EOF) {
		switch (opt) {
		  case 'R':
			recursive = 1;
			break;
		  case 'c':
			if (count > 0) {
				fprintf(stderr, "%s: extra -c ignored.\n",
					progname);
				break;
			}
			vec[count++] = optarg;
			break;
		  case 'r':
			rcsdir = optarg;
			break;
		  case 'v':
			if (++verbose > 1) {
				/*
				 * If more than one 'v' is seen, we print
				 * working directory names on unbuffered
				 * stdout so that they'll sync with errors
				 * on stderr.
				 */
				setbuf(stdout, (char *)0);
			}
			break;
		  default:
			fprintf(stderr,
		    "usage: %s [-c command] [-r rcsdir] [-Rv] [dirname ...]\n",
				progname);
			exit(-1);
		}
	}

	/*
	 * Adjust argc/argv to skip progname and options, and do the default
	 * argument if necessary.
	 */
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		argc = 1;
		*argv = ".";
	}

	/*
	 * Put the command to run on unsynchronized working files in vec[0],
	 * unless the user put one there with -c.
	 */
	if (count == 0) {
		vec[count++] = "co";
		if (verbose == 0)
			vec[count++] = "-q";
	}
	first = count;
	if (getwd(top) == 0)
		exit(perrorf(top));

	/*
	 * Close any open files that we and our kids don't need, remembering
	 * the last available file descriptor in maxfd.
	 */
	maxfd = getdtablesize() -1 ;
	for (fd = getdtablehi()-1; fd > 2; --fd)
		close(fd);

	/*
	 * Each argument is a path to a working source directory.
	 * Change back to original top-level directory so relative
	 * paths work correctly.
	 */
	do {
		char *workdir;

		if (chdir(top) < 0)
			exit(perrorf(top));
		workdir = *argv++;
		if (chdir(workdir) < 0)
			perrorf(workdir);
		else
			syncwd(workdir);
	} while (--argc > 0);
	exit(0);
}

int	islocked(char *);
void	rsync(void);

void
syncwd(char *workdir)
{
	DIR *dirp;
	struct dirent *dp;
	char *versdir;
	int rcsislink;	/* is rcs file a symlink? */
	int workislink;	/* is working file a symlink? */
	int noexist;

	/*
	 * If recursive, look for directories to synchronize under the
	 * current working directory.  Conserve the last available file
	 * descriptor if deep in a tree.
	 */
	if (recursive) {
		long offset;

		dirp = 0;
		offset = 0;
		for (;;) {
			int namlen;
			char pathname[MAXPATHLEN];
			struct stat sb;
			char *up;

			if (dirp == 0) {
				dirp = opendir(".");
				if (dirp == 0) {
					perrorf("%s", workdir);
					return;
				}
				seekdir(dirp, offset);
			}
			dp = readdir(dirp);
			if (dp == 0)
				break;
			if (!strcmp(dp->d_name, ".")
			    || !strcmp(dp->d_name, "..")
			    || !strcmp(dp->d_name, rcsdir)) {
				continue;
			}
			namlen = strlen(dp->d_name);
			if (namlen > 5
			    && !strcmp(dp->d_name + namlen - 5, ".skip")) {
				continue;
			}
			sprintf(pathname, "%s/%s", workdir, dp->d_name);
			if (lstat(dp->d_name, &sb) < 0) {
				perrorf("lstat failed on %s", pathname);
				continue;
			}
			if (S_ISLNK(sb.st_mode)) {
				if (stat(dp->d_name, &sb) < 0) {
					perrorf("stat on symlink failed on %s", pathname);
					continue;
				}
				if (!S_ISDIR(sb.st_mode))
					continue;
			}
			else if(!S_ISDIR(sb.st_mode))
				continue;

			/* not ..; want to work in messed up trees */
			up = getcwd(NULL, MAXPATHLEN);
			if(!up) {
				perrorf("couldn't get cwd, skipping %s", pathname);
				continue;
			}

			if (chdir(dp->d_name) < 0) {
				perrorf(pathname);
				free(up);
				continue;
			}
			if (dirp->dd_fd == maxfd) {
				offset = telldir(dirp);
				closedir(dirp);
				dirp = 0;
			}
			syncwd(pathname);
			if(chdir(up))
				perrorf("could not cd back up to %s", up);
			free(up);
		}
		closedir(dirp);
	}

	/*
	 * Look for the RCS subdirectory.  Whine if it doesn't exist.
	 */
	versdir = rcsdir;
	dirp = opendir(versdir);
	if (dirp == 0) {
		perrorf("opendir %s/%s failed", workdir, versdir);
		return;
	}
	if (verbose > 1)
		printf("%s:\n", workdir);

	/*
	 * Check each entry in the version directory against its
	 * working file in the current directory.
	 */
	while ((dp = readdir(dirp)) != 0) {
		int namlen;
		struct stat vsb, wsb;
		char verspath[MAXPATHLEN];
		char *workname;

		namlen = strlen(dp->d_name);
		if (namlen < 3
		    || dp->d_name[namlen-2] != ','
		    && dp->d_name[namlen-1] != 'v') {
			if (strcmp(dp->d_name, ".")
			    && strcmp(dp->d_name, "..")
			    && strcmp(dp->d_name, "old")
			    && strcmp(versdir, ".")) {
				fmessage(workdir, versdir,
					"/%s: not an RCS version file.",
					dp->d_name);
			}
			continue;
		}
		sprintf(verspath, "%s/%s", versdir, dp->d_name);
		if (lstat(verspath, &vsb) < 0) {
			perrorf("stat %s/%s failed", workdir,verspath);
			continue;
		}
		rcsislink = S_ISLNK(vsb.st_mode);
		if (rcsislink && stat(verspath, &vsb) < 0) {
			perrorf("stat %s/%s failed", workdir,verspath);
			continue;
		}

		/*
		 * Make dp->d_name be the working filename by zapping
		 * the ",v" suffix.  Then check whether we should use
		 * the synchronizer.
		 */
		workname = dp->d_name;
		workname[namlen-2] = '\0';
		noexist = lstat(workname, &wsb);
		workislink = S_ISLNK(wsb.st_mode);
		if(!noexist && workislink)
			noexist = stat(workname, &wsb);
		if(noexist || (workislink && !rcsislink)
		    || (wsb.st_mtime < vsb.st_mtime
		    && wsb.st_size < vsb.st_size)) {
			vec[count] = strdup(workname);
			if (vec[count] == 0) {
				exit(perrorf("out of memory in %s",
					     workdir));
			}
			if (++count == ARGSPEREXEC)
				rsync();
			continue;
		}

		/*
		 * The working file looks ok, but we should check for
		 * various RCS irregularities.
		 */
		if ((wsb.st_mode & S_IWRITE) && !islocked(verspath)) {
			umessage(workdir, workname,
				 "unlocked but writable by", USER,
				 wsb.st_uid);
		}
		if (wsb.st_mode & (S_IWRITE>>3)) {
			umessage(workdir, workname,
				 "writable by group", GROUP,
				 wsb.st_gid);
		}
		if (wsb.st_mode & (S_IWRITE>>6)) {
			fmessage(workdir, workname,
				 ": writable by other.");
		}
		if (wsb.st_size >= (vsb.st_size+512)) {
			fmessage(workdir, workname,
			    ": checked out file larger than RCS file, time to check in?");
		}
	}

	/*
	 * Close the directory (note that we can't check for readdir error)
	 * and do any remaining arguments in vec.
	 */
	closedir(dirp);
	if (count > first)
		rsync();
}

/*
 * Apply the command in vec[0] to a bunch of arguments in vec[first..count).
 */
void
rsync()
{
	int pid;
	
	pid = fork();
	if (pid) {
		int argc;
		char **argv;

		if (pid < 0)
			(void) perrorf("fork");
		argc = count - first;
		argv = &vec[first];
		while (--argc >= 0) {
			assert(*argv);
			free(*argv++);
		}
		count = first;
		if (pid > 0)
			(void) wait((int *) 0);
		return;
	}
	assert(count <= ARGSPEREXEC);
	vec[count] = 0;
	execvp(vec[0], vec);
	exit(perrorf("exec"));
	/* NOTREACHED */
}

/*
 * Check whether the named RCS file has a lock, by searching the first few
 * lines for the "locks" keyword.
 */
#define	LINESTOSEARCH	50
#define	MAXLINESIZE	256

int
islocked(char *name)
{
	int locked;
	FILE *file;

	locked = 0;
	file = fopen(name, "r");
	if (file) {
		int lineno;
		char line[MAXLINESIZE], user[MAXLINESIZE];

		for (lineno = 0; lineno < LINESTOSEARCH; lineno++) {
			if (fgets(line, sizeof line, file) == 0)
				break;
			if (sscanf(line, "locks %[a-z]s:%*d.%*d;", user)) {
				locked = 1;
				break;
			}
		}
		fclose(file);
	}
	return locked;
}

/*
 * Message output tailored to map user/group id to a name if possible.
 */
void
umessage(char *dir, char *file, char *msg, enum id_type type, int id)
{
	char *name = 0;
	
	if (type == USER) {
		struct passwd *pw = getpwuid(id);
		if (pw != 0)
			name = pw->pw_name;
	} else {
		struct group *gr = getgrgid(id);
		if (gr != 0)
			name = gr->gr_name;
	}
	if (name)
		fmessage(dir, file, ": %s %s.", msg, name);
	else
		fmessage(dir, file, ": %s %d.", msg, id);
}

/*
 * Print an error message about the file at path dir/file.
 */
void
fmessage(char *dir, char *file, char *format, ...)
{
	va_list ap;

	fprintf(stderr, "%s: %s/%s", progname, dir, file);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fputc('\n', stderr);
	va_end(ap);
}

/*
 * Formatted perror with a usable return value.
 */
int
perrorf(char *format, ...)
{
	int error;
	va_list ap;

	error = errno;
	fprintf(stderr, "%s: ", progname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	if (0 < error && error < sys_nerr)
		fprintf(stderr, ": %s", sys_errlist[error]);
	fprintf(stderr, ".\n");
	return error;
}
