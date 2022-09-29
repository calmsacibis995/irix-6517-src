/*
 * Copyright 1989 Silicon Graphics, Inc. All rights reserved.
 * $Revision: 1.28 $
 *
 * NAME
 *	install - install software OR generate an installation database
 * SYNOPSIS
 *	install [-m mode] [-u owner] [-g group] [-idb attribute] [-new]
 *	    [-o] [-O] [-rawidb idbpath] [-root rootpath] [-s] [-t] [-v]
 *	    optionally, one of [-blk maj,min[,off]] [-chr maj,min[,off]] [-dir]
 *			       [-fifo] [-ln path] [-lns path] [-src path]
 *	    one of [-f dir] [-F dir]	(mandatory unless -dir is used)
 *	    file1 file2 ... fileN	(pathnames of files to install)
 * DESCRIPTION
 *	See install(1).
 * ENVIRONMENT
 *	The following optional environment variables are used if set:
 *	    INSTOPTS	Install options, overridden by command line options.
 *	    RAWIDB	Absolute pathname of raw idb to generate.
 *	    ROOT	Pathname of target tree root, overridden by -root.
 *	    SRC		Pathname of source tree top (idb generation only).
 * AUTHOR
 *	Brendan Eich, 04/29/88
 *	Ariel Faigon, April 1997:
 *		Added BSD compatibility	to keep GNU, FreeBSD, Linux,
 *		and XFree Makefiles and configure's happy.
 * NOTE
 *	The somewhat awkward options are derived from a version of the
 *	V.3 install script, heavily hacked by SGI and MIPS.
 *
 *	The  -o and -O options don't work when using range arguments
 *	for the minor number with -chr and -blk.  It would be painful
 *	to do so, and doesn't really make sense anyway.
 * TODO
 *	Unify -f, -F and -dir.
 *	Obsolete old options and use getopt (it'll never happen)
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/* for lockf flags and utimbuf */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>

/*
 * Maximum number of IDB attributes as listed by multiple -idb options.
 * Maximum number of quoted arguments, also options with arguments.
 */
#define	MAXIDBTAGS	10
#define	MAXARGS		100

/*
 * A file's type from the point of view of installation.
 */
enum ftype { REG, BLKDEV, CHRDEV, DIR, FIFO, LINK, SYMLINK };

/*
 * A structure for mode and user/group id.
 */
struct value {
	char	*name;
	int	number;
};

/*
 * Global error reporting and option variables.
 */
char	*progname;	/* invoked name for diagnostics */
int	superuser;	/* true if effective uid is 0 */
int	trytoremoveold;	/* whether to attempt to unlink an existing target */
int	saveold;	/* whether to save an existing target as OLDtarget */
int	trytosymlink;	/* install by symlinking dst to src if possible */
int	newtimes;	/* install with new access and modified times */

/*
 * Necessary forward function prototypes.
 */
void	parseoptstr(char *options, char *varname);
int	parseoptvec(int argc, char **argv);
void	usage(void);
int	mtoi(char *s, int *ip);
int	utoi(char *s, int *ip);
int	gtoi(char *s, int *ip);
void	initowner(struct value *vp);
void	initgroup(struct value *vp);
void	fail(char *format, ...);
void	sfail(char *format, ...);
void	makedirpath(char *name, struct value *owner, struct value *group);
void	install(enum ftype ftype, int mode, struct value *owner,
		struct value *group, char *dst, char *src);
void	idbopen(char *name);
void	idbwrite(enum ftype ftype, char *mode, char *owner, char *group,
	         char *dst, char *src, int tagc, char **tagv);
void	idbclose(char *name);

/*
 * Variables shared by main and parseoptvec.
 */
char		*root, *rawidb;		/* ROOT and RAWIDB values */
enum ftype	ftype = REG;		/* file type */
struct value	mode, owner, group;	/* file permission attributes */
char		*dstdir, *src;		/* destination directory, source path */
char		*idbtagvec[MAXIDBTAGS];	/* vector of idb attributes */
int		idbtagcnt;		/* count of idb attributes */
int		makedstdir;		/* create prerequisite directories */
int		verbose;		/* if true, blab while installing */

/*
 * Option processing definitions.  Options are looked-up by name in a table
 * which associates an enum opt value with each valid name.
 */
enum opt {
	Opt_Unknown = -1,
	Opt_m, Opt_u, Opt_g, Opt_idb, Opt_o, Opt_O, Opt_s, Opt_v,
	Opt_blk, Opt_chr, Opt_dir, Opt_fifo, Opt_ln, Opt_lns,
	Opt_src, Opt_f, Opt_F, Opt_t, Opt_root, Opt_rawidb, Opt_new,
	Opt_c, Opt_d
};

char *optnamemap[] = {
	"m", "u", "g", "idb", "o", "O", "s", "v",
	"blk", "chr", "dir", "fifo", "ln", "lns",
	"src", "f", "F", "t", "root", "rawidb", "new",
	"c", "d"
};
#define	NOPTS		(sizeof optnamemap / sizeof optnamemap[0])
#define	optname(opt)	optnamemap[(int)(opt)]

char	optvarname[] = "INSTOPTS";
char	white[] = " \t\n";

int	*argc_p;	/* for BSD compatibility argument faking */
int	opt_s;		/* for BSD compatibility: strip */
char	*strip_prog = "/bin/strip";

main(int argc, char **argv)
{
	char *list, *path;
	int first, nosrc;
	struct value dirowner, dirgroup;
	char *nargv[MAXARGS];
	char srcpath[MAXPATHLEN];

	/*
	 * Initialize and process environment variables and options.
	 * Set default root and open rawidb after parsing options, since
	 * these parameters may be set by either the INSTOPTS envariable
	 * or by command line options.
	 */
	argc_p = &argc;		/* for BSD compat transformations */

	progname = *argv;
	superuser = (geteuid() == 0);
	root = getenv("ROOT");
	rawidb = getenv("RAWIDB");
	list = getenv(optvarname);
	if (list)
		parseoptstr(list, optvarname);
	first = parseoptvec(argc, argv);
	if (root == 0)
		root = "";
	if (rawidb && *rawidb == '\0')
		rawidb = NULL;
	if (rawidb)
		idbopen(rawidb);
	else
		umask(0);

	/*
	 * If no arguments remain, or no destination directory was specified,
	 * complain and bail out.
	 */
	argc -= first;
	argv += first;
	if (argc <= 0 || dstdir == 0)
		usage();

	/*
	 * Now set default mode, owner, and group if not set by options.
	 */
	if (mode.name == 0) {
		switch (ftype) {
		  case REG:
		  case DIR:
			mode.name = "755";
			break;
		  case LINK:
		  case SYMLINK:
			mode.name = "0";
			break;
		  default:
			mode.name = "666";
		}
		mtoi(mode.name, &mode.number);
	}
	if (owner.name == 0) {
		owner.name = "root";
		if (rawidb == 0
		    && (!superuser || !utoi(owner.name, &owner.number))) {
			initowner(&owner);
		}
	}
	if (group.name == 0) {
		group.name = "sys";
		if (rawidb == 0
		    && (!superuser || !gtoi(group.name, &group.number))) {
			initgroup(&group);
		}
	}

	/*
	 * If -F was specified, initialize dirowner and dirgroup so that
	 * the user can write in implicitly created directories.
	 */
	if (makedstdir) {
		if (rawidb == 0 && !superuser) {
			initowner(&dirowner);
			initgroup(&dirgroup);
		} else {
			dirowner = owner;
			dirgroup = group;
		}
	}

	/*
	 * If there's only one argument and it contains blanks, break it
	 * up into arguments (backwards compatibility).
	 */
	if (argc == 1 && strpbrk(*argv, white)) {
		argc = 0;
		list = *argv;
		while ((path = strtok(list, white)) != 0) {
			if (argc >= MAXARGS)
				fail("too many quoted arguments");
			list = 0;
			nargv[argc++] = path;
		}
		nargv[argc] = 0;
		argv = nargv;
	}

	/*
	 * Process remaining arguments as relative paths from the current
	 * directory to files to be installed.  Prepend $ROOT to absolute
	 * pathnames if and only if we are not generating an idb.
	 */
	nosrc = (src == 0);
	if (rawidb == 0 && src && src[0] == '/') {
		sprintf(srcpath, "%s%s", root, src);
		src = srcpath;
	}
	while (--argc >= 0) {
		char dst[MAXPATHLEN];

		path = *argv++;
		if (nosrc)
			src = path;
		while (*path == '/')
			path++;
		if (rawidb == 0) {
			sprintf(dst, "%s%s/%s", root, dstdir, path);
			if (nosrc && src[0] == '/') {
				sprintf(srcpath, "%s%s", root, src);
				src = srcpath;
			}
			if (makedstdir)
				makedirpath(dst, &dirowner, &dirgroup);
			install(ftype, mode.number, &owner, &group, dst, src);
			if (verbose)
				printf("%s installed as %s.\n", src, dst);
		} else {
			/*
			 * Idb generation is always silent.
			 */
			sprintf(dst, "%s/%s", dstdir, path);
			idbwrite(ftype, mode.name, owner.name, group.name,
				 dst, src, idbtagcnt, idbtagvec);
		}
	}
	if (rawidb)
		idbclose(rawidb);
	return 0;
}

/*
 * More option processing gunk.  The opts vector is used by findopt to
 * record options and detect duplicates.  The next free slot in opts is
 * indexed by nextoptslot.
 */
enum opt opts[NOPTS];
int nextoptslot;

/*
 * Parse options listed in the string options, which is the value of the
 * envariable named by varname.
 */
void
parseoptstr(char *options, char *varname)
{
	int argc;
	char *argv[MAXARGS+1];
	char *word;

	argc = 0;
	argv[argc++] = progname;
	while ((word = strtok(options, white)) != 0) {
		if (argc >= MAXARGS)
			fail("too many %s options", varname);
		options = 0;
		argv[argc++] = word;
	}

	/*
	 * Reset nextoptslot after parsing, to avoid checking for
	 * environment/argv duplicates, which are ok.
	 */
	argv[argc] = 0;
	parseoptvec(argc, argv);
	nextoptslot = 0;
}

enum opt findopt(char *name);
void missingoptarg(enum opt opt);
void badoptarg(enum opt opt, char *arg);
char *target_dir(char * name);
char *filename(char * name);
int is_dir(char * name);

int
parseoptvec(int argc, char **argv)
{
	int oargc;
	enum opt opt;

	oargc = argc;
	while (--argc > 0 && (*++argv)[0] == '-') {
		opt = findopt(&(*argv)[1]);
		switch (opt) {
		  case Opt_m:
			if (--argc == 0)
				missingoptarg(opt);
			if (!mtoi(*++argv, &mode.number))
				badoptarg(opt, *argv);
			mode.name = *argv;
			break;

		  case Opt_u:
			if (--argc == 0)
				missingoptarg(opt);
			argv++;
			if (rawidb == 0 && !utoi(*argv, &owner.number))
				badoptarg(opt, *argv);
			owner.name = *argv;
			break;

		  case Opt_g:
			if (--argc == 0)
				missingoptarg(opt);
			argv++;
			if (rawidb == 0 && !gtoi(*argv, &group.number))
				badoptarg(opt, *argv);
			group.name = *argv;
			break;

		  case Opt_idb:
			if (--argc == 0)
				missingoptarg(opt);
			if (idbtagcnt >= MAXIDBTAGS)
				fail("too many -%s options", optname(opt));
			idbtagvec[idbtagcnt++] = *++argv;
			break;

		  case Opt_O:
			trytoremoveold = 1;
			/* FALL THROUGH */
		  case Opt_o:
			saveold = 1;
			break;

		  case Opt_s:
			/* silence was optional, is now the default */
			/*
			 * Note: In BSD: -s means strip
			 * But on SGI strip is not necessarily installed
			 * so we cannot guarantee BSD compatibility here :-(
			 */
			opt_s = 1;
			break;

		  case Opt_v:
			verbose = 1;
			break;

		  case Opt_blk:
		  case Opt_chr:
			if (--argc == 0)
				missingoptarg(opt);
			ftype = (opt == Opt_blk) ? BLKDEV : CHRDEV;
			src = *++argv;
			break;

		  case Opt_d:	/* BSD compat */
		  case Opt_dir:
			ftype = DIR;
			if (dstdir == 0)
				dstdir = "";
			makedstdir = 1;
			break;

		  case Opt_fifo:
			ftype = FIFO;
			break;

		  case Opt_ln:
		  case Opt_lns:
			if (--argc == 0)
				missingoptarg(opt);
			src = *++argv;
			ftype = (opt == Opt_ln) ? LINK : SYMLINK;
			break;

		  case Opt_src:
			if (--argc == 0 || src)
				missingoptarg(opt);
			src = *++argv;
			break;

		  case Opt_f:
		  case Opt_F:
			if (--argc == 0)
				missingoptarg(opt);
			dstdir = *++argv;
			makedstdir = (opt == Opt_F);
			break;

		  case Opt_t:
			trytosymlink = 1;
			break;

		  case Opt_root:
			if (--argc == 0)
				missingoptarg(opt);
			root = *++argv;
			break;

		  case Opt_rawidb:
			if (--argc == 0)
				missingoptarg(opt);
			rawidb = *++argv;
			break;

		  case Opt_new:
			newtimes = 1;
			break;

		  case Opt_c:	/* BSD old compatibility mode: no-op */
			break;

		  default:
			fprintf(stderr, "%s: illegal option %s.\n", progname,
				*argv);
			usage();
		}
	}
	/*
	 * BSD/GNU compatibility:
	 * If we didn't get any SGI specific (and mandatory) flags
	 * -f, -F, or -dir, we assume a BSD call:
	 *
	 *	install source... dest
	 *
	 * where dest can be a directory.
	 * The trick here is to accept the BSD invocation and to do
	 * the right thing while supporting the SGI idb cool stuff
	 * so that you may do a "make install" in a GNU directory
	 * and get the IDB file populated correctly.
	 *
	 * Still this is not 100% BSD compatible where we have
	 * a conflict with the SGI version. Here's What is not covered:
	 *
	 *	o -s (for strip) if strip isn't installed just give a warning
	 * 	o long GNU options --version --help
	 *	o Behavior of BSD in the presence on RAWIDB and SRC
	 *	  env variables may be confusing to those not familiar
	 *	  with them.
	 */
	if (! dstdir && argc >= 2) {

		char * file1 = filename(argv[0]);
		char * fileN = filename(argv[argc-1]);

		if (opt_s) {
			if (access(strip_prog, X_OK) != 0) {
				/* probably not installed */
				fprintf(stderr, "%s: warning: "
					"BSD compatibility: strip not found, "
					"won't strip.\n", progname);
				strip_prog = NULL;
			}
		} else
			strip_prog = NULL;

		if (is_dir(argv[argc-1])) {
			dstdir = argv[argc-1];
			argv[argc-1] = NULL;
			makedstdir = 1;
			argc--;
			oargc--;
			(*argc_p)--;

		} else if (argc > 2) { /* last arg must be a dir */
			usage();

		} else if (argc == 2) {
			/* extract directory part of last name */

			dstdir = target_dir(argv[argc-1]);
			makedstdir = 1;
			argv[argc-1] = fileN;		/* ignore dir part */

			argc--;			/* to skip the first arg */

			/*
			 * two cases:
			 *	filename1 == filename2 (already handled above)
			 *	filename1 != filename2
			 */
			if (file1 && fileN && strcmp(file1, fileN)) {
				/*
				 * filenames are different
				 * emulate -src filename1
				 */
				src = *argv++;
			}
		}
	} else {			/* not a BSD/GNU call */
		strip_prog = NULL;
	}
	return oargc - argc;
}

/*
 * Option processing tables and option name lookup function.
 */
struct sortedopt {
	char	 **namep;
	enum opt opt;
} sortedopts[NOPTS] = {
#define	SOI(opt)	{ &optname(opt), opt }
	SOI(Opt_F),
	SOI(Opt_O),
	SOI(Opt_blk),
	SOI(Opt_c),
	SOI(Opt_chr),
	SOI(Opt_d),
	SOI(Opt_dir),
	SOI(Opt_f),
	SOI(Opt_fifo),
	SOI(Opt_g),
	SOI(Opt_idb),
	SOI(Opt_ln),
	SOI(Opt_lns),
	SOI(Opt_m),
	SOI(Opt_new),
	SOI(Opt_o),
	SOI(Opt_rawidb),
	SOI(Opt_root),
	SOI(Opt_s),
	SOI(Opt_src),
	SOI(Opt_t),
	SOI(Opt_u),
	SOI(Opt_v),
#undef SOI
};

char optcompat[] = {
/* m      */  0,
/* u      */  1,0,
/* g      */  1,1,0,
/* idb    */  1,1,1,1,
/* o      */  1,1,1,1,1,
/* O      */  1,1,1,1,0,1,
/* s      */  1,1,1,1,1,1,1,
/* v      */  1,1,1,1,1,1,1,1,
/* blk    */  1,1,1,1,1,1,1,1,0,
/* chr    */  1,1,1,1,1,1,1,1,0,0,
/* dir    */  1,1,1,1,1,0,1,1,0,0,1,
/* fifo   */  1,1,1,1,1,1,1,1,0,0,0,1,
/* ln     */  1,1,1,1,1,1,1,1,0,0,0,0,0,
/* lns    */  1,1,1,1,1,1,1,1,0,0,0,0,0,0,
/* src    */  1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
/* f      */  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
/* F      */  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
/* t      */  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* root   */  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* rawidb */  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* new    */  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* c      */  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* d      */  1,1,1,1,1,0,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1
/*
              m u g i o O s v b c d f l l s f F t r r n c d
                    d         l h i i n n r       o a e
                    b         k r r f   s c       o w w
                                    o             t i
                                                    d
                                                    b
 */
};

enum opt
findopt(char *name)
{
	enum opt opt;
	int low, high, slot;

	/*
	 * Binary search for name in the sorted option name table.
	 */
	opt = Opt_Unknown;
	low = 0;
	high = (sizeof sortedopts / sizeof sortedopts[0]) - 1;
	while (low <= high) {
		int mid, cmp;

		mid = (low+high) / 2;
		cmp = strcmp(name, *sortedopts[mid].namep);
		if (cmp < 0)
			high = mid - 1;
		else if (cmp > 0)
			low = mid + 1;
		else {
			opt = sortedopts[mid].opt;
			break;
		}
	}
	if (opt == Opt_Unknown)
		return opt;
	/*
	 * Check for incompatible or duplicate options.  Use high and low
	 * to compute a lower triangular matrix index.
	 */
	for (slot = 0; slot < nextoptslot; slot++) {
		low = (int) opt;
		high = (int) opts[slot];
		if (high < low) {
			low = high;
			high = (int) opt;
		}
		if (!optcompat[(high*(high+1))/2 + low]) {
			if (opts[slot] == opt)
				fail("duplicate use of -%s option", name);
			fail("incompatible options -%s and -%s",
			     optname(opts[slot]), name);
		}
	}
	if (nextoptslot >= NOPTS)
		fail("too many options (-%s)", name);
	opts[nextoptslot++] = opt;
	return opt;
}

/*
 * Abusage notifiers.
 */
void
usage(void)
{
	fprintf(stderr, "\
Usage:	%s [-m mode] [-u owner] [-g group] [-idb attribute] [-new]\n\
	[-o] [-O] [-rawidb idbpath] [-root rootpath] [-s] [-t] [-v]\n\
	optionally, one of [-blk maj,min[,off]] [-chr maj,min[,off]] [-dir]\n\
			   [-fifo] [-ln path] [-lns path] [-src path]\n\
	one of [-f dir] [-F dir]	(mandatory unless -dir is used)\n\
	file1 file2 ...	fileN		(pathnames of files to install)\n\
\n\
BSD/GNU compatibility (if none of -f, -F, or -dir was given):\n\
	%s [options] file1 file2\n\
	%s [options] files... directory\n\
		-c: ignored.\n\
		-d: equivalent to -dir\n\
		-s: strip target (if strip is installed)\n\
",
		progname, progname, progname);

	exit(-1);
}

void
missingoptarg(enum opt opt)
{
	fprintf(stderr, "%s: missing argument for -%s option.\n", progname,
		optname(opt));
	usage();
}

void
badoptarg(enum opt opt, char *arg)
{
	fail("malformed argument %s for -%s option", arg, optname(opt));
}

/*
 * Convert mode/user/group to integer.
 */
int
mtoi(char *s, int *ip)
{
	return sscanf(s, "%o", ip);
}

int
utoi(char *s, int *ip)
{
	struct passwd *pw;

	pw = getpwnam(s);
	if (pw) {
		*ip = pw->pw_uid;
		return 1;
	}
	return sscanf(s, "%d", ip);
}

int
gtoi(char *s, int *ip)
{
	struct group *gr;

	gr = getgrnam(s);
	if (gr) {
		*ip = gr->gr_gid;
		return 1;
	}
	return sscanf(s, "%d", ip);
}

/*
 * Set default values from effective ids.
 */
void
initowner(struct value *vp)
{
	int uid;
	struct passwd *pw;
	static char buf[8];

	uid = geteuid();
	pw = getpwuid(uid);
	if (pw) {
		vp->name = pw->pw_name;
		vp->number = pw->pw_uid;
		return;
	}
	sprintf(buf, "%d", uid);
	vp->name = buf;
	vp->number = uid;
}

void
initgroup(struct value *vp)
{
	int gid;
	struct group *gr;
	static char buf[8];

	gid = getegid();
	gr = getgrgid(gid);
	if (gr) {
		vp->name = gr->gr_name;
		vp->number = gr->gr_gid;
		return;
	}
	sprintf(buf, "%d", gid);
	vp->name = buf;
	vp->number = gid;
}

/*
 * Create a directory ancestor line.
 */
void
makedirpath(char *name, struct value *owner, struct value *group)
{
	struct stat64 sb;
	char *lastslash;

	lastslash = strrchr(name, '/');
	if (lastslash == 0 || lastslash == name)
		return;
	*lastslash = '\0';
	if (stat64(name, &sb) == 0) {
		if (S_ISDIR(sb.st_mode)) {
			*lastslash = '/';
			return;
		}
		errno = EEXIST;
		sfail("can't create directory %s", name);
	}
	makedirpath(name, owner, group);
	install(DIR, 0755, owner, group, name, 0);
	*lastslash = '/';
}

/*
 * Return target directory for install:
 * if argument is a directory - return it,
 * Otherwise, extract the directory part of the name in a non-destructive way
 * (unlike dirname(3G)) and return it.  Used for BSD compatibility.
 */
int is_dir(char * name)
{
	struct stat64 sb;

	return (stat64(name, &sb) == 0 && S_ISDIR(sb.st_mode)) ? 1 : 0;
}

char *
target_dir(char * name)
{
	char *lastslash;
	char *dirname;

	/* arg is directory ? */
	if (is_dir(name)) {
		return name;
	}

	/* filename */
	dirname = strdup(name);
	lastslash = strrchr(dirname, '/');
	if (lastslash == 0) {		/* no slashes, assume current dir */
		return ".";
	}

	*lastslash = '\0';
	return dirname;
}

char *
filename(char * name)
{
        char *lastslash;

	/* arg is directory ? */
	if (is_dir(name))
		return NULL;

	lastslash = strrchr(name, '/');
	return (lastslash == NULL) ? name : lastslash + 1;
}


int	removeold(char *dst, mode_t mode);
int	renameold(char *dst);
struct	devname;
void	devname(char *magic, struct devname *dn);
void	dtoi(char *s, char **p, int *ip);
void	makedevice(struct stat64 *dsb, int mode, struct value *owner,
		   struct value *group, char *dst, char *src);
void	setowner(char *dst, struct value *owner, struct value *group);
void	lsetowner(char *dst, struct value *owner, struct value *group);

/*
 * Install src of type ftype as dst with attributes mode/owner/group.
 */
void
install(enum ftype ftype, int mode, struct value *owner, struct value *group,
	char *dst, char *src)
{
	struct stat64 sb;
	struct stat64 *dsb;
	char abspath[MAXPATHLEN];
	static char cwd[MAXPATHLEN];

	/*
	 * Implement the -O and -o options if the target node exists.
	 * If the target exists and can't be removed, dsb points to its
	 * attributes.  Otherwise dsb is null.
	 */
	if (lstat64(dst, &sb) == 0
	    && (!trytoremoveold || removeold(dst, sb.st_mode) < 0)
	    && (!saveold || renameold(dst) < 0)) {
		dsb = &sb;
	} else
		dsb = 0;

	/*
	 * Implement the -t option to optimize regular file installation
	 * with a symbolic link.
	 */
	if (trytosymlink && ftype == REG) {
		ftype = SYMLINK;
		if (src[0] != '/') {
			if (cwd[0] == '\0' && getwd(cwd) == 0)
				fail(cwd);
			sprintf(abspath, "%s/%s", cwd, src);
			src = abspath;
		}
	}

	switch (ftype) {
	  case REG: {
		int from, to, cc, wc;
		char buf[BUFSIZ];

		from = open(src, O_RDONLY);
		if (from < 0)
			sfail("can't open %s", src);

		/*
		 * If dst exists and is not a regular file, try to remove it.
		 */
		if (dsb && !S_ISREG(dsb->st_mode)
		    && removeold(dst, dsb->st_mode) == 0) {
			dsb = 0;
		}

		/*
		 * If dst exists, check for a non-privileged attempt to
		 * overwrite a readonly file.
		 */
		if (dsb && !superuser
		    && (!S_ISLNK(dsb->st_mode) || stat64(dst, dsb) == 0)
		    && dsb->st_uid == owner->number
		    && !(dsb->st_mode & S_IWRITE)) {
			/*
			 * Perhaps we cannot unlink dst because we aren't
			 * allowed to write in the target directory; try to
			 * acquire write permission on the file.
			 */
			dsb->st_mode |= S_IWRITE;
			if (chmod(dst, dsb->st_mode) < 0)
				sfail("can't overwrite %s", dst);
		}

		/*
		 * Truncate after overwriting dst rather than opening with
		 * O_TRUNC, just in cast dst is identical to src.
		 */
		to = open(dst, O_WRONLY|O_CREAT, mode|S_IWRITE);
		if (to < 0)
			sfail("can't create %s", dst);

		while ((cc = read(from, buf, sizeof buf)) > 0) {
			wc = write(to, buf, cc);
			if (wc < 0)
				sfail("error writing %s", dst);
			if (wc != cc)
				fail("short write to %s", dst);
		}
		if (cc < 0)
			sfail("error reading %s", src);

		/*
		 * Truncate to size of from, according to tell(from).
		 */
		if (ftruncate(to, lseek(from, 0L, 1)) < 0)
			sfail("can't truncate %s", dst);
		close(from);

		/*
		 * Remove the write permission bit we added when creating
		 * or (possibly) prior to opening.
		 */
		if (fchmod(to, mode) < 0)
			sfail("can't change mode of %s to %#o", dst, mode);
		close(to);

		/*
		 * BSD compatibility and -s: do strip
		 * Don't use system(3) since this is a setuid prog.
		 */
		if (strip_prog) {
			pid_t	pid;

			switch (pid = fork()) {
			  case -1:
				sfail("can't fork for stripping %s", dst);
			  case 0:
				execl(strip_prog, "strip", dst, (char *)0);
				sfail("can't exec %s", strip_prog);
			}
			/*
			 * Parent: wait for child/strip to complete
			 * I don't check the status because strip will
			 * complain nicely if the file is not ELF etc.
			 */
			waitpid(pid, (int *)0, 0);
		}

		/*
		 * Set dst's timestamps to match src's.
		 */
		if (!newtimes) {
			struct stat64 ssb;
			struct utimbuf utb;

			if (stat64(src, &ssb) < 0)
				sfail("can't get attributes for %s", src);
			utb.actime = ssb.st_atime;
			utb.modtime = ssb.st_mtime;
			if (utime(dst, &utb))
				sfail("can't set times for %s", dst);
		}
		break;
	  }

	  /*
	   * Device cases return early because they exclusively create one or
	   * more targets and set owner and group for each node.
	   */
	  case BLKDEV:
		makedevice(dsb, S_IFBLK | mode, owner, group, dst, src);
		return;

	  case CHRDEV:
		makedevice(dsb, S_IFCHR | mode, owner, group, dst, src);
		return;

	  case FIFO:
		if (dsb && !S_ISDIR(dsb->st_mode) && unlink(dst) < 0)
			sfail("can't unlink target node %s", dst);
		dsb = 0;
		if (mknod(dst, S_IFIFO | mode, NODEV) < 0)
			sfail("can't create pipe %s", dst);
		break;

	  case DIR:
		if ((dsb == 0 || !S_ISDIR(dsb->st_mode))
		    && mkdir(dst, mode) < 0) {
			sfail("can't make directory %s", dst);
		}
		break;

	  /*
	   * Link and symlink return early to avoid following the link when
	   * setting attributes.
	   */
	  case LINK:
		if (dsb && !S_ISDIR(dsb->st_mode)) {
			struct stat64 ssb;

			if (lstat64(src, &ssb) == 0
			    && ssb.st_dev == dsb->st_dev
			    && ssb.st_ino == dsb->st_ino) {
				return;
			}
			unlink(dst);
		}
		if (link(src, dst) < 0)
			sfail("can't make hard link %s to %s", dst, src);
		return;

	  case SYMLINK:
		if (dsb && !S_ISDIR(dsb->st_mode)) {
			if (S_ISLNK(dsb->st_mode)) {
				register int cc;
				char val[MAXPATHLEN];

				cc = readlink(dst, val, sizeof val - 1);
				if (cc >= 0) {
					val[cc] = '\0';
					if (!strcmp(val, src))
						return;
				}
			} else {
				struct stat64 ssb;

				if (lstat64(src, &ssb) == 0
				    && ssb.st_dev == dsb->st_dev
				    && ssb.st_ino == dsb->st_ino
				    && ssb.st_nlink == 1) {
					return;
				}
			}
			unlink(dst);
		}
		if (symlink(src, dst) < 0)
			sfail("can't make symbolic link %s to %s", dst, src);
		else
			lsetowner(dst, owner, group);
		return;
	}

	/*
	 * Set target owner and group.  We don't need to set mode because
	 * our umask was set to 0 early in main.
	 */
	if (dsb == 0
	    || dsb->st_uid != owner->number
	    || dsb->st_gid != group->number) {
		setowner(dst, owner, group);
	}
}

/*
 * A wrapper for unlink or rmdir, depending on inode format.
 */
int
removeold(char *dst, mode_t mode)
{
	return S_ISDIR(mode) ? rmdir(dst) : unlink(dst);
}

/*
 * A wrapper for rename that formulates the "old" name used by -o.
 */
int
renameold(char *dst)
{
	char *lastslash;
	char saved[MAXPATHLEN];

	lastslash = strrchr(dst, '/');
	assert(lastslash);
	*lastslash++ = '\0';
	sprintf(saved, "%s/OLD%s", dst, lastslash);
	*--lastslash = '/';
	return rename(dst, saved);
}

/*
 * Structure for one or a range of minor devices.
 */
struct devname {
	int	maj;		/* major device number */
	int	min;		/* minor device number or first if range */
	int	lastmin;	/* last minor if range, -1 otherwise */
	int	offset;		/* added to minor when naming nodes in range */
};

void
devname(char *magic, struct devname *dn)
{
	char *low, *high;

	dtoi(magic, &low, &dn->maj);
	if (*low != ',')
		fail("comma expected in device name %s", magic);
	dtoi(low + 1, &high, &dn->min);
	if (*high == '-')
		dtoi(high + 1, &high, &dn->lastmin);
	else
		dn->lastmin = -1;
	if (*high == ',')
		dtoi(high + 1, 0, &dn->offset);
	else
		dn->offset = 0;
}

void
dtoi(char *s, char **p, int *ip)
{
	unsigned long ul;
	char *cp;

	ul = strtoul(s, &cp, 0);
	if (ul == 0 && cp == s)
		fail("malformed device number %s", s);
	if (p)
		*p = cp;
	*ip = (int) ul;
}

void	makedevnode(struct stat64 *dsb, int mode, struct value *owner,
		    struct value *group, char *dst, dev_t rdev);

/*
 * Make a special filesystem node named dst with the given inode format,
 * mode, and magic device name (src).
 */
void
makedevice(struct stat64 *dsb, int mode, struct value *owner,
	   struct value *group, char *dst, char *src)
{
	struct devname dn;
	int min;

	devname(src, &dn);
	if (dn.lastmin < 0) {
		makedevnode(dsb, mode, owner, group, dst,
			    makedev(dn.maj, dn.min));
		return;
	}
	for (min = dn.min; min <= dn.lastmin; min++) {
		char devpath[MAXPATHLEN];
		struct stat64 sb;

		sprintf(devpath, "%s%d", dst, min + dn.offset);
		dsb = (lstat64(devpath, &sb) < 0) ? 0 : &sb;
		makedevnode(dsb, mode, owner, group, devpath,
			    makedev(dn.maj, min));
	}
}

/*
 * Note that this routine now leaves an existing device alone if it has the
 * specified device type and number.  It does NOT check or change the owner
 * or mode, since the main use of this is to leave existing tty's, etc. with
 * the owners and modes that the system manager has set them to.
 */
void
makedevnode(struct stat64 *dsb, int mode, struct value *owner,
	    struct value *group, char *dst, dev_t rdev)
{
	if (dsb) {
		if (dsb->st_rdev == rdev
		    && (dsb->st_mode & S_IFMT) == (mode & S_IFMT)) {
			return;
		}
		if (unlink(dst) < 0)
			sfail("can't unlink target node %s", dst);
	}
	if (mknod(dst, mode, rdev) < 0)
		sfail("can't create device %s", dst);
	setowner(dst, owner, group);
}

void
setowner(char *dst, struct value *owner, struct value *group)
{
	if (chown(dst, owner->number, group->number) < 0) {
		sfail("can't change ownership of %s to %s.%s",
			dst, owner->name, group->name);
	}
}

void
lsetowner(char *dst, struct value *owner, struct value *group)
{
	if (lchown(dst, owner->number, group->number) < 0) {
		sfail("can't change symlink ownership of %s to %s.%s",
			dst, owner->name, group->name);
	}
}

/*
 * Idb construction variables and procedures.
 */
FILE	*rawidbf;
char	srcbase[MAXPATHLEN];

void
idbopen(char *name)
{
	char *base, *dir, *lastdir;

	rawidbf = fopen(name, "a");
	if (rawidbf == 0)
		sfail("can't open idb %s", name);
	if (lockf(fileno(rawidbf), F_LOCK, 0) < 0)
		sfail("can't lock idb %s", name);

	if (getwd(srcbase) == 0)
		fail(srcbase);
	base = getenv("SRC");
	if (base) {
		int len = strlen(base);

		if (!strncmp(base, srcbase, len)) {
			if (srcbase[len] != '\0')
				strcpy(srcbase, srcbase + len + 1);
			else
				srcbase[0] = '\0';
			if (srcbase[0] == '\0')
				strcpy(srcbase, ".");
			return;
		}
	}
		
	base = srcbase;
	lastdir = ".";
	while ((dir = strtok(base, "/")) != 0) {
		if (!strcmp(dir, "src")) {
			dir += 4;
			if (*dir)
				strcpy(srcbase, dir + strspn(dir, "/"));
			else
				strcpy(srcbase, ".");
			return;
		}
		base = 0;
		lastdir = dir;
	}
	fail("can't find 'src' directory above %s", lastdir);
}

void
idbclose(char *name)
{
	if (fclose(rawidbf) == EOF)
		sfail("error writing idb %s", name);
}

void
idbwrite(enum ftype ftype, char *mode, char *owner, char *group,
	 char *dst, char *src, int tagc, char **tagv)
{
	int min;
	struct devname dn;

	dst += strspn(dst, "/");
	if (*dst == '\0')
		dst = ".";	/* idb-ish for the root directory */

	switch (ftype) {
	  case REG:
		fprintf(rawidbf, "f %s %s %s %s %s/%s",
			mode, owner, group, dst, srcbase, src);
		break;

	  case BLKDEV:
	  case CHRDEV:
		devname(src, &dn);	/* validate device name */
		if (dn.lastmin < 0) {
			fprintf(rawidbf, "%c %s %s %s %s %s dev(%d,%d)",
				(ftype == BLKDEV) ? 'b' : 'c',
				mode, owner, group, dst, srcbase,
				dn.maj, dn.min);
			break;
		}
		for (min = dn.min; min <= dn.lastmin; min++) {
			fprintf(rawidbf, "%c %s %s %s %s%d %s dev(%d,%d)",
				(ftype == BLKDEV) ? 'b' : 'c',
				mode, owner, group, dst, min, srcbase,
				dn.maj, min);
			if (min < dn.lastmin)
				putc('\n', rawidbf);
		}
		break;

	  case DIR:
	  case FIFO:
		fprintf(rawidbf, "%c %s %s %s %s %s",
			(ftype == DIR) ? 'd' : 'p',
			mode, owner, group, dst, srcbase);
		break;

	  case LINK:
	  case SYMLINK:
		fprintf(rawidbf, "%c %s %s %s %s %s %s(%s)",
			(ftype == LINK) ? 'h' : 'l',
			mode, owner, group, dst, srcbase,
			(ftype == LINK) ? "links" : "symval",
			src);
	}

	while (--tagc >= 0)
		fprintf(rawidbf, " %s", *tagv++);
	putc('\n', rawidbf);
}

/*
 * Fatal error reporters: sfail is for system call failure.
 */
void
fail(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, ".\n");
	exit(-1);
}

void
sfail(char *format, ...)
{
	int error;
	va_list ap;

	error = errno;
	va_start(ap, format);
	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, format, ap);
	va_end(ap);
	assert(0 < error && error < sys_nerr);
	fprintf(stderr, ": %s.\n", strerror(error));
	exit(error);
}
