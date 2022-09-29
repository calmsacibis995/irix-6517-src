/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 * $Revision: 1.24 $
 *
 * NAME
 *	tlink - clone a file tree via symbolic links
 * SYNOPSIS
 *	tlink [-chnrvX] [-d name] [-x name] source target [path ...]
 * DESCRIPTION
 *	See tlink(1).
 * AUTHOR
 *	Brendan Eich, 01/14/87
 * NOTE
 *	Relative pathname manipulation assumes overlapping bcopy().
 */
#include <assert.h>
#include <bstring.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/wait.h>

/*
 * A pathname structure.  PATH_MAX is the maximum path length including
 * the terminating 0.  The number of significant components is kept for
 * relative symbolic link formulation.
 */
struct path {
	char		name[PATH_MAX];
	unsigned short	len;
	unsigned short	components;
	struct dirstack	*dirstack;
	struct relpath	*toward;
};

struct dirstack {
	unsigned short	kids;
	unsigned short	pruned;
	struct dirstack	*parent;
};

struct relpath {
	struct path	path;
	struct path	*to;
};

/*
 * Call path() with a filename pname to initialize a path struct *pp.
 * Add a component to a path with push(); remove it with pop().
 */
void	path(char *pname, struct path *pp);
void	push(struct path *pp, char *name, int namlen);
void	pop(struct path *pp, int namlen);

/*
 * A limit on the number of file descriptors to keep open during tree-linking.
 * Since we use depth-first search, the limit corresponds to the depth of the
 * tree at which we begin to close and re-open a directory's descriptor each
 * time we descend into one of its children.
 */
int	maxdirfd;
int	curdirfd;

/*
 * A list of regular expressions describing filenames to avoid.
 */
#define	MAXBADPAT	200

char *badbuiltin[] = {
	"^RCS$",
	"^.*,v$",
	0
};
char	*badpat[MAXBADPAT+1];
char	*baddirpat[MAXBADPAT+1];

/*
 * Compile expr into patvec[index] unless index exceeds patveclen, in which
 * case issue an error message saying "too many <toomany>: <expr> ignored",
 * where <toomany> and <expr> stand for the pointed-at strings.
 */
void	setpat(char *expr, int index, char *patvec[], int patveclen,
	       char *toomany);

/*
 * With the -d option, we maintain a vector of regular expressions naming
 * directories to link into the target tree.
 */
#define	MAXDIRLINK	8

char	*dirlink[MAXDIRLINK+1];

int	(*linkfun)(const char *, const char *) = symlink;
				/* -h: make hard rather than symbolic links */
int	null_effect;		/* -n: don't create anything */
int	verbose;		/* -v: spit out created pathnames */
int	relative;		/* -r: create relative symbolic links */

/* error handling utilities */
char	*progname;
int	status;

void	message(char *format, ...);
void	perrorf(char *format, ...);
void	perrnof(int error, char *format, ...);
void	sfail(char *format, ...);
void	fail(char *format, ...);

enum opmode { CLONE, CLEAN, PRUNE };

void	treelink(struct path *fp, struct path *tp, enum opmode mode);

int
main(argc, argv)
	int argc;
	char **argv;
{
	int npat, ndirpat, ndirlink, opt;
	enum opmode mode;
	struct path source, target;

	/*
	 * Initialize.
	 */
	progname = *argv;
	(void) fclose(stdin);
	maxdirfd = getdtablesize() - 1;
	for (npat = 0; badbuiltin[npat]; npat++)
		badpat[npat] = regcmp(badbuiltin[npat], (char *) 0);
	ndirpat = 0;
	ndirlink = 0;

	/*
	 * Process options.
	 */
	mode = CLONE;
	while ((opt = getopt(argc, argv, "cd:hnprvx:X")) != EOF) {
		switch (opt) {
		  case 'c':
			mode = CLEAN;
			break;
		  case 'd':
			setpat(optarg, ndirlink++, dirlink, MAXDIRLINK,
				"directories to link (-d)");
			break;
		  case 'h':
			linkfun = link;
			break;
		  case 'n':
			null_effect = 1;
			break;
		  case 'p':
			mode = PRUNE;
			break;
		  case 'r':
			relative = 1;
			break;
		  case 'v':
			verbose = 1;
			break;
		  case 'X':
			npat = 0;
			break;
		  case 'x':
			if (strchr(optarg, '/')) {
				setpat(optarg, ndirpat++, baddirpat, MAXBADPAT,
					"pathnames to exclude (-x)");
			} else {
				setpat(optarg, npat++, badpat, MAXBADPAT,
					"filenames to exclude (-x)");
			}
			break;
		  default:
			goto usage;
		}
	}
	argc -= optind;
	if (argc < 2)
		goto usage;
	argv += optind;
	if (npat <= MAXBADPAT)
		badpat[npat] = 0;

	/*
	 * Construct the path or restrictive sub-paths and call treelink().
	 */
	path(argv[0], &source);
	path(argv[1], &target);
	if (argc == 2)
		treelink(&source, &target, mode);
	else {
		argc -= 2, argv += 2;
		while (--argc >= 0) {
			int arglen;

			arglen = strlen(*argv);
			push(&source, *argv, arglen);
			push(&target, *argv, arglen);
			treelink(&source, &target, mode);
			pop(&source, arglen);
			pop(&target, arglen);
			argv++;
		}
	}
	return status;

usage:
	/*
	 * Complain about abusage.
	 */
	fprintf(stderr,
	  "usage: %s [-cnprvX] [-d name] [-x name] source target [path ...]\n",
		progname);
	return -1;
}

void
setpat(char *expr, int index, char *patvec[], int patveclen, char *toomany)
{
	if (index >= patveclen) {
		message("too many %s: %s ignored\n", toomany, expr);
		return;
	}
	patvec[index] = regcmp(expr, (char *) 0);
}

int
makedir(char *name, int mode)
{
	if (!null_effect && mkdir(name, mode|S_IWRITE) < 0)
		return -1;
	if (verbose)
		printf("%s\n", name);
	return 0;
}

int
makelink(struct path *sp, char *target)
{
	char *source;

	if (relative && linkfun == symlink) {
		assert(sp->toward && sp->toward->to == sp);
		source = sp->toward->path.name;
	} else
		source = sp->name;
	if (!null_effect && (*linkfun)(source, target) < 0)
		return -1;
	if (verbose)
		printf("%s\n", target);
	return 0;
}

void	fixpath(char *pname, struct path *pp);
char	*getwd(char *buf);

/*
 * Construct in *pp an absolute canonical path to pname.
 */
void
path(char *pname, struct path *pp)
{
	char cwd[PATH_MAX];
	struct path subdir;

	pp->dirstack = 0;
	pp->toward = 0;
	if (pname[0] == '/') {
		fixpath(pname, pp);
		return;
	}

	/* on error, getwd puts a message in its result parameter */
	if (getwd(cwd) == 0)
		sfail(cwd);
	fixpath(cwd, pp);
	fixpath(pname, &subdir);
	if (subdir.len > 0) {
		if (pp->len + 1 + subdir.len >= PATH_MAX)
			fail("pathname %s/%s too long", pp->name, subdir.name);
		pp->name[pp->len++] = '/';
		bcopy(subdir.name, pp->name + pp->len, subdir.len + 1);
		pp->len += subdir.len;
		pp->components += subdir.components;
	}
}

/*
 * Canonicalize pathname pb into the path structure *pp by collapsing /./
 * into / and removing extra slashes.  Compute canonical length and number
 * of components; fail if pathname is too long.
 */
void
fixpath(char *pb, struct path *pp)
{
	char *pb0, *pa;	/* b for before, a for after */

	pa = pp->name;
	pb0 = pb;
	pp->components = 0;
	do {
		while (*pb == '.'
		    && (pb == pb0 || pb[-1] == '/')
		    && (pb[1] == '\0' || pb[1] == '/')) {
			if (pb[1] == '\0') {
				if (pb > pb0)
					--pa;	/* elide /. */
				pb++;
			} else
				pb += 2;	/* elide ./ */
		}
		if (pa - pp->name >= PATH_MAX)
			fail("pathname %s too long", pb0);
		*pa++ = *pb;
		if ((*pb == '\0' || *pb == '/') && pb > pb0)
			pp->components++;
		if (*pb == '/') {
			while (*++pb == '/')	/* compress ///s */
				;
			--pb;
		}
	} while (*pb++ != '\0');
	pp->len = pa - pp->name - 1;
}

void
push(struct path *pp, char *name, int namlen)
{
	int adjslash;	/* character delta for slash separator */

	if (pp->name[pp->len-1] == '/')
		adjslash = (name[0] == '/') ? -1 : 0;
	else
		adjslash = (name[0] != '/') ? 1 : 0;
	if (pp->len + adjslash + namlen >= PATH_MAX)
		fail("pathname %s/%s too long", pp->name, name);
	if (adjslash > 0)
		pp->name[pp->len++] = '/';
	else if (adjslash < 0)
		name++, --namlen;

	(void) strncpy(&pp->name[pp->len], name, namlen);
	pp->len += namlen;
	pp->name[pp->len] = '\0';
	pp->components++;
	if (pp->toward)
		push(&pp->toward->path, name, namlen);
}

void
pop(struct path *pp, int namlen)
{
	pp->len -= namlen;
	if (pp->name[pp->len] != '/') {
		--pp->len;
		assert(pp->name[pp->len] == '/');
	}
	pp->name[pp->len] = '\0';
	--pp->components;
	if (pp->toward)
		pop(&pp->toward->path, namlen);
}

/*
 * Enter the directory named absolutely by pp, relative to the current
 * directory by name.
 */
char	cdfail[] = "cannot change directory to %s";

void
down(struct path *pp, char *name)
{
	if (chdir(name) < 0)
		sfail(cdfail, pp->name);
	if (pp->toward) {
		pp = &pp->toward->path;
		if (3 + pp->len >= PATH_MAX)
			fail("relative pathname ../%s too long", pp->name);
		bcopy(pp->name, pp->name + 3, pp->len + 1);
		bcopy("../", pp->name, 3);
		pp->len += 3;
		pp->components++;
	}
}

/*
 * Leave subdirectory pp, returning to its parent.
 */
void
up(struct path *pp)
{
	if (chdir("..") < 0)
		sfail("cannot change directory from %s to ..", pp->name);
	if (pp->toward) {
		pp = &pp->toward->path;
		assert(pp->name[0]=='.'&&pp->name[1]=='.'&&pp->name[2]=='/');
		bcopy(pp->name + 3, pp->name, pp->len - 2);
		pp->len -= 3;
		--pp->components;
	}
}

/*
 * Compute in rp the relative pathname up from the uncommon part of fp and
 * down to the uncommon part of tp (fp and tp cannot be identical).
 */
void
relatepaths(struct path *fp, struct path *tp, struct relpath *rp)
{
	char *f, *t, *downto;
	int ncommon, ndotdot, downtolen;

	assert(fp->name[0] == '/' && tp->name[0] == '/');
	rp->path.toward = 0;
	downto = tp->name + 1;
	ncommon = 0;
	for (f = fp->name + 1, t = downto; *f == *t; f++, t++) {
		assert(*f != '\0');
		if (*f == '/') {
			downto = t + 1;
			ncommon++;
		}
	}

	ndotdot = fp->components - ncommon;
	rp->path.components = ndotdot + (tp->components - ncommon);
	downtolen = tp->len - (downto - tp->name);
	rp->path.len = 3 * ndotdot + downtolen;
	if (rp->path.len >= PATH_MAX) {
		fail("relative pathname from %s to %s too long",
			fp->name, tp->name);
	}

	t = rp->path.name;
	while (--ndotdot >= 0) {	/* build the "upfrom" path */
		*t++ = '.';
		*t++ = '.';
		*t++ = '/';
	}
	assert(t + downtolen == rp->path.name + rp->path.len);
	bcopy(downto, t, downtolen + 1);
	tp->toward = rp;
	rp->to = tp;
}

#ifdef NOTDEF
void
unrelatepaths(struct relpath *rp)
{
	assert(rp->to->toward == rp);
	rp->to->toward = 0;
	rp->to = 0;
}
#endif

char *
tail(struct path *pp)
{
	char *cp;

	for (cp = pp->name+pp->len-1; *cp != '/'; --cp) {
		if (cp == pp->name)
			return cp;	/* no slash in pp */
	}
	return cp + 1;
}

int	maketargetroot(char *name, int mode);

int 	treewalk(struct path *fp, struct path *tp,
		 int (*nodefun)(struct path *, struct path *));

int	clean(struct path *, struct path *);
int	prune(struct path *, struct path *);
int	clone(struct path *, struct path *);

#define	SAME_INODE(sb1,sb2)	((sb1).st_dev == (sb2).st_dev && \
				 (sb1).st_ino == (sb2).st_ino)

void
treelink(struct path *sp, struct path *tp, enum opmode mode)
{
	int perm;
	struct stat ssb, tsb;
	struct relpath rp;

	if (stat(sp->name, &ssb) < 0 || !S_ISDIR(ssb.st_mode))
		fail("source %s is not a directory", sp->name);
	perm = ssb.st_mode & ~S_IFMT;
	if (stat(tp->name, &tsb) < 0) {
		if (mode != CLONE)
			fail("target %s does not exist", tp->name);
		if (maketargetroot(tp->name, perm) < 0)
			sfail("cannot create target directory %s", tp->name);
	} else if (!S_ISDIR(tsb.st_mode)) {
		fail("target %s is not a directory", tp->name);
	} else if (SAME_INODE(ssb, tsb)) {
		fail("%s and %s are identical", sp->name, tp->name);
	}

	if (mode == CLONE) {
		if (relative)
			relatepaths(tp, sp, &rp);
		if (chdir(sp->name) < 0)
			sfail(cdfail, sp->name);
		(void) treewalk(sp, tp, clone);
	} else {
		if (relative)
			relatepaths(sp, tp, &rp);
		if (chdir(tp->name) < 0)
			sfail(cdfail, tp->name);
		(void) treewalk(tp, sp, (mode == PRUNE) ? prune : clean);
	}
#ifdef NOTDEF
	if (relative)
		unrelatepaths(&rp);
#endif
}

int
maketargetroot(char *name, int mode)
{
	char *lastslash;

	lastslash = strrchr(name, '/');
	if (lastslash) {
		struct stat sb;

		*lastslash = '\0';
		if (stat(name, &sb) < 0) {
			if (maketargetroot(name, mode) < 0)
				return -1;
		}
		*lastslash = '/';
	}
	return makedir(name, mode);
}

int	match(char *name, char **revec);
int	removetree(char *name);

/*
 * Walk a tree rooted at the name in fp, maintaining a parallel name in tp.
 * Our caller must have chdir'd into fp.  Return 1 if the directory named by
 * fp should be removed, 0 otherwise.
 */
int
treewalk(struct path *fp, struct path *tp,
	 int (*pf)(struct path *, struct path *))
{
	DIR *dirp;
	off_t offset;
	struct dirstack fds;
	int remove;
	char **rep;

	dirp = 0;
	offset = 0;
	fds.kids = fds.pruned = 0;
	fds.parent = fp->dirstack;
	fp->dirstack = &fds;

	for (;;) {
		dirent_t *dp;
		int namlen;

		if (dirp == 0) {
			dirp = opendir(".");
			if (dirp == 0) {
				sfail("cannot open current directory %s/..",
				    fp->name);
			}
			curdirfd = dirp->dd_fd;
			if (offset)
				seekdir(dirp, offset);
		}

		dp = readdir(dirp);
		if (dp == 0)
			break;
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;
		fds.kids++;
		if (match(dp->d_name, badpat))
			continue;
		namlen = strlen(dp->d_name);
		push(fp, dp->d_name, namlen);
		if (baddirpat[0] && match(fp->name, baddirpat)) {
			pop(fp, namlen);
			continue;
		}
		push(tp, dp->d_name, namlen);

		if ((*pf)(fp, tp)) {
			down(fp, dp->d_name);
			if (curdirfd >= maxdirfd) {
				offset = telldir(dirp);
				closedir(dirp);
				dirp = 0;
			}
			remove = treewalk(fp, tp, pf);
			up(fp);
			if (dirp)
				curdirfd = dirp->dd_fd;
			if (remove
			    && (null_effect || removetree(fp->name))
			    && verbose) {
				printf("%s\n", fp->name);
			}
		}

		pop(fp, namlen);
		pop(tp, namlen);
	}

	remove = (*pf)(fp, 0);
	fp->dirstack = fds.parent;
	assert(dirp);
	closedir(dirp);
	return remove;
}

/*
 * Return true if name matches one of the patterns in the vector rep.
 */
int
match(char *name, char **rep)
{
	char *res;
	while (*rep) {
		if ((res = regex(*rep, name)) && (*res == '\0')) {
			return 1;
		}
		rep++;
	}
	return 0;
}

int
removetree(char *name)
{
	pid_t pid;
	int fd, rmstatus;

	if (rmdir(name) == 0)
		return 1;
	if (errno != ENOTEMPTY && errno != EEXIST) {
		perrorf("cannot remove %s", name);
		return 0;
	}
	pid = fork();
	if (pid < 0)
		sfail("cannot fork to remove %s", name);
	if (pid == 0) {
		for (fd = curdirfd; fd > 2; --fd)
			(void) close(fd);
		(void) execl("/bin/rm", "rm", "-rf", name, (char *) 0);
		sfail("cannot execute /bin/rm");
	}
	(void) waitpid(pid, &rmstatus, 0);
	if (rmstatus != 0) {
		message("cannot remove %s\n", name);
		return 0;
	}
	return 1;
}

/*
 * Given a file (full path tp->name, relative path tail(tp)) in the target
 * tree, check whether its counterpart sp->name exists.  If not then remove
 * tail(tp).  As clean() is a treewalk() node processing function, it must
 * return 1 if tp names a searchable directory, 0 otherwise.
 */
int
clean(struct path *tp, struct path *sp)
{
	char *target;
	struct stat tsb, ssb;
	int cleaned;

	if (sp == 0)
		return 0;

	target = tail(tp);
	if (lstat(target, &tsb) < 0) {
		perrorf("cannot get attributes of %s", tp->name);
		return 0;
	}

	/*
	 * If the source exists, check whether both target and source are
	 * directories, returning 1 if so.  If the target is a link check
	 * whether it names the source.
	 */
	if (stat(sp->name, &ssb) == 0) {
		if (S_ISDIR(tsb.st_mode)) {
			if (S_ISDIR(ssb.st_mode))
				return 1;
		} else if (S_ISLNK(tsb.st_mode)) {
			struct stat sb;	/* link source attributes */

			if ((!S_ISDIR(ssb.st_mode) || match(tp->name, dirlink))
			    && stat(target, &sb) == 0
			    && SAME_INODE(sb, ssb)) {
				return 0;
			}
		}
	}

	/*
	 * Attempt to remove the target, which may be a non-empty directory.
	 */
	if (null_effect)
		cleaned = 1;	/* so we can be verbose */
	else if (S_ISDIR(tsb.st_mode))
		cleaned = removetree(target);
	else {
		cleaned = (unlink(target) == 0);
		if (!cleaned)
			perrorf("cannot unlink %s", target);
	}
	if (cleaned && verbose)
		printf("%s\n", tp->name);
	return 0;
}

int
prune(struct path *tp, struct path *sp)
{
	struct dirstack *tds;
	char *target;
	struct stat tsb, ssb;
	int cc, pruned;

	tds = tp->dirstack;
	if (sp == 0) {
		if (tds->kids != 0 && tds->pruned == tds->kids) {
			tds = tds->parent;
			if (tds)
				tds->pruned++;
			return 1;
		}
		return 0;
	}

	target = tail(tp);
	if (lstat(target, &tsb) < 0) {
		perrorf("cannot get attributes of %s", tp->name);
		return 0;
	}

	/*
	 * If the target is a directory, return 1 whether or not the source
	 * counterpart exists, so that we can prune dead growth hanging under
	 * obsoleted source directories.
	 */
	if (S_ISDIR(tsb.st_mode))
		return 1;

	/*
	 * Return early if the target is not a dangling link to a non-existent
	 * source file (i.e., a stale link).
	 */
	if (!S_ISLNK(tsb.st_mode) || stat(target, &ssb) == 0)
		return 0;

	/*
	 * Attempt to remove the stale link.
	 */
	if (null_effect)
		pruned = 1;	/* so we can be verbose */
	else {
		pruned = (unlink(target) == 0);
		if (!pruned)
			perrorf("cannot unlink %s", target);
	}

	if (pruned) {
		tds->pruned++;
		if (verbose)
			printf("%s\n", tp->name);
	}
	return 0;
}

int	stalepurge(char *target);

/*
 * If the source is a directory, clone it in the target tree.  Otherwise make
 * a symbolic link named target pointing at source.  Return 1 if source is a
 * directory and should be searched, 0 otherwise.
 */
int
clone(struct path *sp, struct path *tp)
{
	char *source, *target;
	struct stat ssb, tsb;

	if (tp == 0)
		return 0;

	source = tail(sp);
	if (lstat(source, &ssb) < 0) {
		perrorf("cannot get attributes of %s", sp->name);
		return 0;
	}
	if (S_ISLNK(ssb.st_mode)) {
		if (stat(source, &tsb) < 0) {
			if (errno == ENOENT) {
				errno = 0;
				perrorf("%s is a dangling link.", sp->name);
			} else {
				perrorf("couldn't follow symlink %s", sp->name);
			}
		}
	}

	/*
	 * If the target exists, check whether it's a stale link.  If it is a
	 * fresh link, return 0 to prevent searching deeper in the source tree.
	 * Otherwise stalepurge() has removed the target.  If the target is a
	 * directory, check whether the source is also.
	 */
	target = tp->name;
	if (lstat(target, &tsb) == 0) {
		if (S_ISLNK(tsb.st_mode)) {
			if (!stalepurge(target))
				return 0;
		} else {
			if (S_ISDIR(tsb.st_mode)) {
				if (S_ISDIR(ssb.st_mode))
					return 1;
				if (verbose)
					perrnof(EISDIR, target);
			} else if (S_ISDIR(ssb.st_mode)) {
				if (verbose)
					perrnof(ENOTDIR, target);
			}
			return 0;
		}
	}

	/*
	 * Either the target did not exist upon entry, or it was a stale link
	 * and we removed it.  Either way, if the source is a directory and if
	 * tp is not one of the directories to be linked, make an empty twin
	 * in the target tree and search the source directory.  Otherwise make
	 * a symbolic link.
	 */
	if (S_ISDIR(ssb.st_mode) && match(target, dirlink) == 0) {
		if (makedir(target, ssb.st_mode & ~S_IFMT) < 0)
			sfail("cannot make directory %s", target);
		return 1;
	}
	if (makelink(sp, target) < 0)
		sfail("cannot link %s to %s", target, source);
	return 0;
}

/*
 * Return 1 if target, a symbolic link, names a nonexistent file and is
 * successfully removed; return 0 otherwise.
 */
int
stalepurge(char *target)
{
	struct stat sb;

	if (stat(target, &sb) == 0)
		return 0;
	if (!null_effect && unlink(target) < 0) {
		perrorf("cannot unlink stale link %s", target);
		return 0;
	}
	if (verbose)
		perrnof(ENOENT, target);
	return 1;
}

/*
 * Formatted message output, with progname.
 */
void	vmessage(char *format, va_list ap);

void
message(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vmessage(format, ap);
	va_end(ap);
}

/*
 * Formatted perror, with progname.  NB: potentially clobbers errno.
 */
void	vperrnof(int error, char *format, va_list ap);

void
perrorf(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vperrnof(errno, format, ap);
	va_end(ap);
}

void
perrnof(int error, char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vperrnof(error, format, ap);
	va_end(ap);
}

void
vperrnof(int error, char *format, va_list ap)
{
	vmessage(format, ap);
	if (0 < error && error <= sys_nerr)
		fprintf(stderr, ": %s", sys_errlist[error]);
	fputc('\n', stderr);
}

void
vmessage(char *format, va_list ap)
{
	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, format, ap);
	status++;
}

/*
 * System call failure.
 */
void
sfail(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vperrnof(errno, format, ap);
	exit(status);
}

void
fail(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vperrnof(0, format, ap);
	exit(status);
}
