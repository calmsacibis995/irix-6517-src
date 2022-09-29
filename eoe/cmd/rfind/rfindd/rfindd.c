/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#define NDEBUG
#include <assert.h>
#include <grp.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <ndbm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ctype.h>
#include <libgen.h>

#include <sys/utsname.h>

#include <sys/fsid.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
extern int errno;
extern char *sys_errlist[];
#include <sys/fstyp.h>
#include <sys/statfs.h>
#include <sys/param.h>

#include "fsdump.h"
#include "fenv.h"
#include "rpc_io.h"
#include "pagesize.h"

static void descend (inod_t *);		/* walk file tree below spec'd inode */
static  void logit (char *);		/* write entry to log file */

static void error2 (char *s, char *t);

static int stgmatch(const char *, const char *);	/* subtree gmatch */
static const char *skipsign (const char *);			/* skip over '+'/'-' sign */
static inod_t *xsymlink(const inod_t *p);		/* map symlink inod to referenced inod */

/*
 * binary search hp_dnd2 for bottom and top of interval
 * of possible matches to string.
 */
static void find2interval (str5_t *, char *, hpindex_t *, hpindex_t *);

/*
 * binary search hp_dndx for bottom and top of interval
 * of possible matches to string.
 */
static void findxinterval (dndx_t *, char *, hpindex_t *, hpindex_t *);

int fs_fd;					/* file descriptor open on old dumpfile */

static void restore (char *, char *);   /* map in, setup heap mh for a dump file */
static int getunum(int t, char *s);

/*
 * UID and GID select user or group operations in routine getunum().
 */
#define	UID	1
#define	GID	2

#define A_DAY	86400L				/* a day full of seconds */
#define EQ(x, y) (strcmp(x, y)==0)

int	e2_nested;				/* set when e2() is > 1 level recursed */

heap_set mh;				/* ... the main memory heap */
struct fh fh;					/* file image of heap */
hpindex_t primeno;
ino64_t rootino;

char fsdumpline[512];


/*
 * The "current file" concept used in the rfind(1) man page lives
 * as the values of Inodp and Pathname:
 */
inod_t *Inodp;					/* pointer to current inod_t */
char	Pathname[PATH_MAX];
char	Newdirname[PATH_MAX];

char	Mntpt[PATH_MAX];			/* Mount point pathname */
dev_t	Mntdev;					/* Mount point device number */

/*
 * When dealing with the actual file system, with what's
 * mounted according to /etc/mtab, with the Mntpt as recorded
 * in the fsdump files, ... we have to use real paths, with
 * symlinks and such squeezed out, using realpath(3C).
 * 
 * But the user wants to provide, and get back, pathnames that
 * might require symlinks and ".." entries to resolve.
 * 
 * So we convert all inputs paths (-root, -newer and friends)
 * to realpaths (using realpath(3C)), use real paths internally,
 * and convert back to the user's view of the rootpath on output.
 * 
 * We were already careful to only match on inodes that are
 * accessible below the user provided root, even if that means
 * silently ignoring inodes elsewhere in the file system.
 *
 * So this root prefix substitution should always be possible.
 * If somehow it isn't, we will print w/o the substitution.
 */

char	realrootpath[PATH_MAX];			/* canonical rootpath */
char	*userrootpath;				/* user's idea of rootpath */

int pathlogfd = -1;
int	Argc, Ai, MaxArgc;
char	**Argv;

/*
 * Replace realrootpath prefix with userrootpath
 * and return pointer to static result.
 * If provided path doesn't have realrootpath as a prefix,
 * then just return a static copy of the provided path.
 */
 
static char *real_to_user_root (char *path)
{
    static char buf[PATH_MAX];
    size_t realrootlen = strlen (realrootpath);

    if (isancestor (realrootpath, path, realrootlen)) {
	strcpy (buf, userrootpath);
	strcat (buf, path + realrootlen);
    } else {
	if (pathlogfd > 0) {
	    int jj;
	    char *msg;

	    for (jj = 0; jj < Argc; jj++) {
		if (jj > 0) write (pathlogfd, " ", 1);
		write (pathlogfd, Argv[jj], strlen(Argv[jj]));
	    }
	    msg = ":: real_to_user_root failed: realpath: ";
	    write (pathlogfd, msg, strlen(msg));
	    write (pathlogfd, path, strlen(path));
	    msg = ", rootpath: ";
	    write (pathlogfd, msg, strlen(msg));
	    write (pathlogfd, realrootpath, strlen(realrootpath));
	    write (pathlogfd, "\n", 1);
	}
	strcpy (buf, path);
    }

    return buf;
}

/*
 * Convert input path to realpath.
 */

static char *user_to_real_path (char *path)
{
    static char buf[PATH_MAX];
    my_realpath (path, buf);
    if (pathlogfd > 0 && strcmp (path, buf) == 0) {
	    int jj;
	    char *msg;

	    for (jj = 0; jj < Argc; jj++) {
		if (jj > 0) write (pathlogfd, " ", 1);
		write (pathlogfd, Argv[jj], strlen(Argv[jj]));
	    }
	    msg = ":: user_to_real_path failed: user path: ";
	    write (pathlogfd, msg, strlen(msg));
	    write (pathlogfd, path, strlen(path));
	    msg = ", realpath: ";
	    write (pathlogfd, msg, strlen(msg));
	    write (pathlogfd, buf, strlen(buf));
	    write (pathlogfd, "\n", 1);	
    }
    return buf;
}

/*
 * Variant of trim_lofs() that returns result in
 * freshly allocated memory.
 */

static char * trim_lofs_alloc (const char *inpath)
{
	char path[PATH_MAX];
	if (strlen (inpath) + 1 > sizeof (path))
		return strdup (inpath);
	strcpy (path, inpath);
	trim_lofs (path);
	return strdup (path);
}

/*
 * In the original cmd/find/find.c code, each predicate had
 * a corresponding runtime function that was called to evaluate
 * that predicate on each inode.
 *
 * Here we support multiple search strategies.  In addition to
 * the full tree walk (descend) similar to the one find has,
 * we also can search by name (using hp_dndx) and inum (hp_inum).
 * So here, the runtime routines are each expanded to respond
 * to multiple needs, which are distinguished by an additional,
 * first, argument, chosen from the following operations:
 */

enum op {
	WGHT,		/* compute best search strategy: return its weight */
	SCAN,		/* scan inodes using best strategy */
	DUMP,		/* for debugging: display node name */
	EVAL		/* evaluate predicate on current (Inodp) inode, ala old find */
};

typedef enum op op_t;

#define MAXWGHT 100			/* search weights (costs) normal range is 0..MAXWGHT */
#define HUGEWGHT (10*MAXWGHT)		/* a bigger number than 0..MAXWGHT */

/*
 * The following table summarizes how the various predicate
 * runtimes respond to the WGHT and SCAN operations:
 *
 *    predicate		WGHT		 	SCAN
 *    =========		===================	=======================
 *
 *	or		sum of kids		do both kids
 *
 *	and		weight best kid		scan best kid
 *
 *	not		infinite weight		fatal error
 *
 *	root		relative size of	walk named subtree
 *			named subtree
 *
 *	name		relative size of	scan named hp_dndx subset
 *	(glob)		named hp_dndx subset
 *
 *	inum		reciprocal of number	evaluate specified inode
 *			of inodes
 *
 *	*		infinite weight		fatal error
 */

/*
 *    Weights are integers in the set: { 0..MAXWGHT, HUGEWGHT } where HUGEWGHT
 *    is an essentially infinite weight, and the values in 0..MAXWGHT
 *    are estimates of what percentage of the whole dump would have
 *    to be exhaustively searched.  Smaller weights promise faster
 *    scans - because less of the dump must be searched.
 */

/*
 * The steps in the overall algorithm are:
 *
 * 1) Restore the fsdump file named in the first argument.
 *
 * 2) Expand remaining arguments to include some of the
 *    defaults, such as explicit -root predicates and
 *    the default "-root /" if other leading -roots not seen.
 *
 * 3) Parse the expanded argument list into the exlist parse tree.
 *
 * 4) Walk the tree doing operation WGHT.  Each "and" predicate stores
 *    an indication of which of its 2 children nodes bid the lowest
 *    weight, and all nodes compute and return a weight.
 *
 *    The overall weight of the tree indicates how fast the search
 *    can be done - this value is unused currently.  What matters
 *    is that the "and" nodes each know which child can scan best.
 *
 * 5) Walk the tree doing operation SCAN.  Most nodes either
 *    pass this op their children, or hope never to see this op.
 *    The nodes that know some particular search strategy
 *    (by subtree, name or inode number) respond to this op
 *    by invoking step (6) on each inode in the range that
 *    the node applies to.
 *
 * 6) For each inode covered by the SCAN, load a pointer
 *    to it in the global Inodp, and invoke the entire
 *    parse tree with the EVAL op.
 */

struct anode {
	int (*F)(op_t, struct anode *);	/* the runtime function to invoke */
	struct anode *L, *R;		/* Left and Right parse tree kids */
	uint64_t X;			/* eXtra 64 bits anode private stash */
	uint64_t Y;			/* 		" "		 */
	uint64_t Z;			/* 		" "		 */
	char *dumparg;			/* optional arg string, for -DUMP */
};

typedef struct anode anode_t;

anode_t *Node;

/*
 * Forward declarations of all the runtime predicate functions.
 */


static int and (op_t op, anode_t *p);
static int or (op_t op, anode_t *p);
static int not (op_t op, anode_t *p);
static int xglob (op_t op, anode_t *p);
static int print (op_t op, anode_t *p);
static int ls (op_t op, anode_t *p);
static int xprintf (op_t op, anode_t *p);
static int ncheck (op_t op, anode_t *p);
static int mtime (op_t op, anode_t *p);
static int atime (op_t op, anode_t *p);
static int xctime (op_t op, anode_t *p);
static int user (op_t op, anode_t *p);
static int inum (op_t op, anode_t *p);
static int group (op_t op, anode_t *p);
static int links (op_t op, anode_t *p);
static int size (op_t op, anode_t *p);
static int csize (op_t op, anode_t *p);
static int stsize (op_t op, anode_t *p);
static int perm (op_t op, anode_t *p);
static int type (op_t op, anode_t *p);
static int prune (op_t op, anode_t *p);
static int root (op_t op, anode_t *p);
static int sttotal (op_t op, anode_t *p);
static int stcount (op_t op, anode_t *p);
static int noop (op_t op, anode_t *p);
static int mnewer (op_t op, anode_t *p);
static int cnewer (op_t op, anode_t *p);
static int anewer (op_t op, anode_t *p);
static int changed (op_t op, anode_t *p);
static int lchanged (op_t op, anode_t *p);
static int xexit (op_t op, anode_t *p);
static int newdiradd (op_t op, anode_t *p);

void doxprnt (const char *format);
void list (char *file);
int scomp (uint64_t a, uint64_t b, char sign);


typedef enum { quiet = 0, showerrors = 1 } t_eflag;
inod_t	*xxstat (char *, t_eflag);
t_eflag ErrorFlag;

time_t	Now;
char	*ArgEnd = ")";
int	depthf = 0;
int	followf = 0;
int	showdots = 0;	/* set if -showdots: makes "." and ".." entries visible */
int	actoptseen = 0;	/* set if action op such as -print, -printf, -ncheck seen */
int	mustdescend=0;	/* set -prune, -depth, -follow or -sttotal - must descend tree */
int	giveup = 0;	/* abort search in this directory */
int	defaulttrunk=0;	/* default to trunk rev */
/*
 * Routines stcount and stsize pass data to routines
 * sttotal and descend via the global subtotal.  Subtotal
 * contains not yet aggregate total for current level.
 */
uint64_t	subtotal = (uint64_t)0;		/* running subtotal */

/*
 * Agsubtotal is the aggregate subtotal: when sttotal()
 * sees subtotal exceed threshold, it moves the current
 * subtotal value into agsubtotal.
 */
uint64_t	agsubtotal = (uint64_t)0;		/* aggregate subtotal */

char	revnum[PATH_MAX] = "";		/* Rcs rev num */
char	revdate[PATH_MAX] = "";		/* Rcs rev date */

int rootflag = 0;	/* set to indicate current file is top -root specified path */

int	dumpflg;	/* set if -DUMP is 1st arg: invokes DUMP op over parse tree */
int	dumplevel;
const char *dumpspaces = "                                                                     ";

struct stat64 Statb;

anode_t	*exp(char *), *e1(char *), *e2(char *), *e3(char *);
anode_t	*mk(int (*F)(op_t, anode_t *), anode_t *L, anode_t *R, uint64_t X, uint64_t Y, char *dumparg);
anode_t *exlist;		/* Root of the parse tree */
char	*nxtarg(void), *peekarg(void);

/* major part of a device external from the kernel */
#define	major(x)	(int)(((unsigned)(x)>>8)&0x7F)
/* minor part of a device */
#define	minor(x)	(int)((x)&0xFF)

#ifdef standalone
#define main_find main
#define rpcexit exit
#define rpcsetbuf getpid
#define rpcflushall getpid
#endif

int hadsigpipe = 0;			/* if sigpipe sets, must be client vanished */

/* ARGSUSED */
void sigpipe (int s) {
	signal(SIGPIPE, sigpipe);
	hadsigpipe = 1;
}

/* ARGSUSED */
void sigalrm (int s) {
	signal(SIGALRM, SIG_DFL);
	fprintf (stderr, "rfind daemon timed out\n");
	rpcexit (1);
}

void get_fsdump_line(char *buf) {
        static char *cur_ptr;
        char *line_end;
        size_t len = 0;

        if (buf) {
                cur_ptr = buf;
        }
        line_end = strchr(cur_ptr, '\n');
        if (line_end) {
                len = line_end - cur_ptr;
                strncpy(fsdumpline, cur_ptr, len);
        }
        fsdumpline[len] = '\0';
        cur_ptr = ++line_end;
}

void main_find(u_int argc, char *argv[]) {
	int npaths = 0;
	char *logmsg = "-LOGGING: ";
	int i;
	DBM *db;
	datum key, content;
	char *longestkey;
	char *mntpt, *dumpfile;

	rpcsetbuf();

	errno = 0;

	if (time(&Now) == (time_t) -1) {
		(void) fprintf(stderr, "rfindd: time() %s\n", sys_errlist[errno]);
		rpcexit(2);
	}

	if(argc<3) {
		(void) fprintf(stderr,"Usage: rfind file-system search-expression\n");
		rpcexit(1);
	}

	if (EQ(argv[1],"-DUMP")) {
		argv++;
		argc--;
		dumpflg++;
	}
	if (strncmp (argv[1], logmsg, strlen(logmsg)) == 0) {
		logit (argv[1] + strlen(logmsg));
		argv++;
		argc--;
	}

	if ((db = dbm_open ("fsdump", O_RDONLY, 0644)) == NULL)
		error2 ("Cannot open fsdump.{dir,pag} files:", "");
	if ((longestkey = strdup("")) == NULL)
		error2 ("no memory","");

	userrootpath = argv[1];
	dumpfile = NULL;

	/*
	 * Grandfather the special hook for ptools - which
	 * instead of setting argv[1] to the desired mount point,
	 * rather sets argv[1] to the constant string "-ptools",
	 * and guarantees that argv[2] is a full pathname to some
	 * place that is below the desired mount point.
	 *
	 * If the file "ptools.LOG" already exists and is writable,
	 * echo the command line args into it, as an analysis aid
	 * in figuring out the usage patterns ptools has on rfind.
	 * Actually, just write out argv[2], the path to search.
	 */
	if (EQ(argv[1],"-ptools")) {
		int logfd;
		userrootpath = argv[2];
		logfd = open ("ptools.LOG", 9);
		if (logfd > 0) {
			int jj;
#if 1
			for (jj = 0; jj < argc; jj++) {
				if (jj > 0) write (logfd, " ", 1);
#else
			for (jj = 2; jj < 3; jj++) {
#endif
				write (logfd, argv[jj], strlen(argv[jj]));
			}
			write (logfd, "\n", 1);
			close (logfd);
		}
	}

	pathlogfd = open ("path.LOG", 9);

	/*
	 * The normal case - "userrootpath" contains a path at or below the
	 * desired mount point.  Look up the corresponding dumpfile name in
	 * the fsdump.{dir,pag} ndbm database files that fsdump updates.
	 */

	if (dumpfile == NULL) {
		my_realpath (userrootpath, realrootpath);
		for (key = dbm_firstkey(db); key.dptr != NULL; key = dbm_nextkey(db)) {
			if (
			    isancestor (key.dptr, realrootpath, key.dsize-1)
			 &&
			    key.dsize-1 > strlen (longestkey)
			) {
				free (longestkey);
				longestkey = strdup (key.dptr);
			}
		}
		if (strlen (longestkey) == 0)
			error2 ("Can't find fsdump file for path:", userrootpath);
		key.dptr = mntpt = longestkey;
		key.dsize = strlen (mntpt) + 1;
		content = dbm_fetch (db, key);
		if (content.dptr == NULL)
			error2 ("No fsdump available for mount point:", argv[1]);
		if ((dumpfile = strdup (content.dptr)) == NULL)
			error2 ("no memory","");
	}

	dbm_close (db);
	restore(mntpt, dumpfile);
	free (longestkey);

	if (PINO(&mh,rootino)->i_mode == 0)
		error2 ("Corrupt fsdump file:", dumpfile);

	free (dumpfile);

	MaxArgc = 3*argc+20;	/* Hopeful upper limit on # args after default expansion */
	if ((Argv = calloc (MaxArgc, sizeof(*Argv))) == NULL)
		error2 ("Argv", "no memory");
	if ((Node = calloc (MaxArgc, sizeof(*Node))) == NULL)
		error2 ("Argv", "no memory");

	Ai = Argc = 0;

	/*
	 * Copy argv to Argv, expanding defaults as we go:
	 * 1) Provide leading "-root realrootpath"
	 * 2) Leading '/' based paths are provided default "-root" predicate
	 * 3) Put "-o" between leading list of roots, and surround them with "(" and ")"
	 *    so that:
	 *	/root1 /root2 -this -that
	 *    becomes:
	 *	-root realrootpath -a ( -root /root1 -o -root /root2 ) -a ( -this -that
	 * 4) any unrecognized arg not starting with "-" gets default "-name" predicate.
	 * 5) after parsing given args, close still open parentheses.
	 * 6) and after that, if no action, add "-print"
	 * Default expansions (4), (5) and (6) are delayed until nxtarg() sees end of Argv.
	 */

	i = 2;

	Argv[Argc++] = "-root";
	Argv[Argc++] = realrootpath;
	Argv[Argc++] = "-a";

	while (i < argc) {
		if (*argv[i] != '/')
			break;
		if (npaths++ == 0)
			Argv[Argc++] = "(";
		else
			Argv[Argc++] = "-o";
		Argv[Argc++] = "-root";
		Argv[Argc++] = user_to_real_path (argv[i++]);
	}
	if (npaths != 0) {
		Argv[Argc++] = ")";
		Argv[Argc++] = "-a";
	}

	Argv[Argc++] = "(";

	while (i < argc)
		Argv[Argc++] = argv[i++];
	if (Argc > MaxArgc)
		error2 ("too many expanded arguments","");

#if 0
	for (i=0; i<Argc; i++)
		printf("Argv[%d] = %s\n", i, Argv[i]);
#endif


	if(!(exlist = exp(peekarg()))) { /* parse and compile the arguments */
		(void) fprintf(stderr,"rfind: parsing error\n");
		rpcexit(1);
	}
	if(Ai<Argc) {
		error2 ("too many closing parentheses", "");
	}
	if (actoptseen == 0)
		exlist = mk (
		    and,
		    exlist,
		    mk (print, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"),
		    (uint64_t)0,
		    (uint64_t)0,
		    "-"
		);

	if (dumpflg)
		(*exlist->F)(DUMP, exlist);

	/*
	 * We must do the WGHT tree walk even if mustdescend is set,
	 * because the WGHT cases in root() and xglob() squirrel
	 * away critical info in the parse tree for later use
	 * during the EVAL phase.  A dubious performance hack ...
	 *
	 * Later: not so dubious ...  I once had code here
	 * that would simply descend from the root inode
	 * if mustdescend was set, but really what I need
	 * is to have the WGHT evaluation code respect
	 * mustdescend, and give different weights which
	 * will force only tree walks of the designated roots
	 * and avoid any inum or glob scans.  Otherwise, a query
	 * with -prune over a small tree ends up walking it all.
	 */
	(*exlist->F)(WGHT, exlist);
	(*exlist->F)(SCAN, exlist);
	rpcexit(0);	/*NOTREACHED*/
}

/* compile time functions:  priority is  exp()<e1()<e2()<e3()  */

anode_t *exp(char *ap) { /* parse ALTERNATION (-o)  */
	register anode_t * p1;

	p1 = e1(ap);
	ap = peekarg();
	if(EQ(ap, "-o")) {
		(void) nxtarg();
		e2_nested--;
		return(mk(or, p1, exp(peekarg()), (uint64_t)0, (uint64_t)0, "-"));
	}
	return(p1);
}
anode_t *e1(char *ap) { /* parse CONCATENATION (formerly -a) */
	register anode_t * p1;

	p1 = e2(ap);
	ap = peekarg();
	if ((ap == ArgEnd) || EQ(ap, "-o") || EQ(ap, ")"))
		return(p1);
	if (EQ(ap, "-a"))
		(void) nxtarg();
	e2_nested--;
	return(mk(and, p1, e1(peekarg()), (uint64_t)0, (uint64_t)0, "-"));
}
anode_t *e2(char *ap) { /* parse NOT (!) */
	if(e2_nested) {
		(void) fprintf(stderr,"rfind: operand follows operand\n");
		rpcexit(1);
	}
	e2_nested++;
	if(EQ(ap, "!")) {
		(void) nxtarg();
		return(mk(not, e3(peekarg()), (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
	}
	return(e3(ap));
}
/* ARGSUSED */
anode_t *e3(char *ap) { /* parse parens and predicates */
	anode_t *p1;
	register char *ap1, *ap2;
	char sign;
	inod_t *p;

	if (EQ(peekarg(), ")")) {
		actoptseen++;
		return(mk(print, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
	}

	ap1 = nxtarg();

	if(EQ(ap1, "(")) {
		e2_nested--;
		p1 = exp(peekarg());
		ap1 = nxtarg();
		if(!EQ(ap1, ")") && ap1 != ArgEnd)
			error2 ("missing close parenthesis",ap1);
		return(p1);
	}
	else if(EQ(ap1, "-print")) {
		actoptseen++;
		return(mk(print, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
	}
	else if(EQ(ap1, "-ls")) {
		actoptseen++;
		return(mk(ls, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
	}
	else if(EQ(ap1, "-none")) {
		actoptseen++;
		return(mk(noop, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "none"));
	}
	else if(EQ(ap1, "-changed"))
		return(mk(changed, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
        else if(EQ(ap1, "-lchanged"))
                return(mk(lchanged, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
	else if(EQ(ap1, "-stcount"))
		return(mk(stcount, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
	else if(EQ(ap1, "-stsize"))
		return(mk(stsize, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
	else if(EQ(ap1, "-showdots")) {
		showdots = 1;
		return(mk(noop, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "showdots"));
	}
	else if(EQ(ap1, "-prune")) {
		mustdescend++;
		return(mk(prune, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "-"));
	}
	else if(EQ(ap1, "-depth")) {
		depthf = 1;
		mustdescend++;
		return(mk(noop, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "depth"));
	}
	else if(EQ(ap1, "-follow")) {
		followf = 1;
		mustdescend++;
		return(mk(noop, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "follow"));
	}
	else if(EQ(ap1, "-defaulttrunk")) {
		defaulttrunk = 1;
		return(mk(noop, (anode_t *)0, (anode_t *)0, (uint64_t)0, (uint64_t)0, "defaulttrunk"));
	}
	else if (*ap1 != '-') {
		char *trimmed_name = trim_lofs_alloc(ap1);
		return(mk(xglob, (anode_t *)0, (anode_t *)0, (uint64_t)trimmed_name, (uint64_t)0, trimmed_name));
	}

	/*
	 * Nota bene: all predicates that take zero args go above this point,
	 *	      and all predicates that take one arg go below here.
	 */

	ap2 = nxtarg();
	sign = *ap2;

	if(EQ(ap1, "-name")) {
		char *trimmed_name = trim_lofs_alloc(ap2);
		return(mk(xglob, (anode_t *)0, (anode_t *)0, (uint64_t)trimmed_name, (uint64_t)0, trimmed_name));
	}
	if(EQ(ap1, "-root")) {
		char *trimmed_name = trim_lofs_alloc(pathcomp(ap2));
		return(mk(root, (anode_t *)0, (anode_t *)0, (uint64_t)trimmed_name, (uint64_t)0, trimmed_name));
	}
	if(EQ(ap1, "-printf")) {
		actoptseen++;
		return(mk(xprintf, (anode_t *)0, (anode_t *)0, (uint64_t)ap2, (uint64_t)0, ap2));
	}
	else if(EQ(ap1, "-user")) {
		uid_t uid;
		if (sign == '-' || sign == '+')
			error2 ("use of +/- sign on -user arg not supported:", ap2);
		if((uid = getunum(UID, ap2)) == -1) {
			char *endchar;
			uid = (uid_t) strtol (ap2, &endchar, 10);
			/* if ap2 has trailing chars -- error */
			if (*endchar != '\0')
				error2 ("invalid -user name:", ap2);
		}
		return(mk(user, (anode_t *)0, (anode_t *)0, (uint64_t)uid, (uint64_t)0, ap2));
	}
	else if(EQ(ap1, "-group")) {
		uid_t gid;
		if (sign == '-' || sign == '+')
			error2 ("use of +/- sign on -group arg not supported:", ap2);
		if((gid = getunum(GID, ap2)) == -1) {
			char *endchar;
			gid = (uid_t) strtol (ap2, &endchar, 10);
			/* if ap2 has trailing chars -- error */
			if (*endchar != '\0')
				error2 ("invalid -group name:", ap2);
		}
		return(mk(group, (anode_t *)0, (anode_t *)0, (uint64_t)gid, (uint64_t)0, ap2));
	}
	else if(EQ(ap1, "-inum"))
		return(mk(inum, (anode_t *)0, (anode_t *)0, (uint64_t)strtoull(skipsign(ap2),NULL,10), (uint64_t)sign, ap2));
	else if(EQ(ap1, "-ncheck")) {
		actoptseen++;
		return(mk(ncheck, (anode_t *)0, (anode_t *)0, (uint64_t)strtoull(skipsign(ap2),NULL,10), (uint64_t)sign, ap2));
	}
	else if(EQ(ap1, "-type")) {
		int t;
		t = *ap2=='d' ? S_IFDIR :
		    *ap2=='b' ? S_IFBLK :
		    *ap2=='c' ? S_IFCHR :
		    *ap2=='p' ? S_IFIFO :
		    *ap2=='f' ? S_IFREG :
		    *ap2=='l' ? S_IFLNK :
		    *ap2=='s' ? S_IFSOCK :
		    0;
		if ( t == 0 )
		{
			fprintf( stderr, "rfind: unknown file type specified with -type\n" );
			rpcexit(1);
		}
		return(mk(type, (anode_t *)0, (anode_t *)0, (uint64_t)t, (uint64_t)0, ap2));
	}
	else if(EQ(ap1, "-newer")) {
		char *realpath = user_to_real_path (ap2);
		time_t mt;
		if ((p = xxstat (realpath, ErrorFlag = showerrors)) == NULL)
			error2 ("-newer argument not found in file-system:", ap2);
		mt = p->i_mtime;
		return(mk(mnewer, (anode_t *)0, (anode_t *)0, (uint64_t)mt, (uint64_t)0, realpath));
	}
	else if(EQ(ap1, "-cnewer")) {
		char *realpath = user_to_real_path (ap2);
		time_t ct;
		if ((p = xxstat (realpath, ErrorFlag = showerrors)) == NULL)
			error2 ("-cnewer argument not found in file-system:", ap2);
		ct = p->i_ctime;
		return(mk(cnewer, (anode_t *)0, (anode_t *)0, (uint64_t)ct, (uint64_t)0, realpath));
	}
	else if(EQ(ap1, "-anewer")) {
		char *realpath = user_to_real_path (ap2);
		time_t at;
		if ((p = xxstat (realpath, ErrorFlag = showerrors)) == NULL)
			error2 ("-anewer argument not found in file-system:", ap2);
		at = p->i_atime;
		return(mk(anewer, (anode_t *)0, (anode_t *)0, (uint64_t)at, (uint64_t)0, realpath));
	}
	else if(EQ(ap1, "-mtime"))
		return(mk(mtime, (anode_t *)0, (anode_t *)0, (uint64_t)strtoul(skipsign(ap2),0,10), (uint64_t)sign, ap2));
	else if(EQ(ap1, "-atime"))
		return(mk(atime, (anode_t *)0, (anode_t *)0, (uint64_t)strtoul(skipsign(ap2),0,10), (uint64_t)sign, ap2));
	else if(EQ(ap1, "-ctime"))
		return(mk(xctime, (anode_t *)0, (anode_t *)0, (uint64_t)strtoul(skipsign(ap2),0,10), (uint64_t)sign, ap2));
	else if(EQ(ap1, "-size")) {
		char *endchar;
		uint64_t sz = strtoull(skipsign(ap2), &endchar, 10);
		if(*endchar == 'c')
			return(mk(csize, (anode_t *)0, (anode_t *)0, sz, (uint64_t)sign, ap2));
		else if (*endchar == '\0')
			return(mk(size, (anode_t *)0, (anode_t *)0, sz, (uint64_t)sign, ap2));
		else
			error2 ("invalid -size suffix (use 'c' for bytes):", ap2);
	}
	else if(EQ(ap1, "-sttotal")) {
		mustdescend++;
		depthf = 1;
		return(mk(sttotal, (anode_t *)0, (anode_t *)0, (uint64_t)strtoull(skipsign(ap2),0,10), (uint64_t)0, ap2));
	}
	else if(EQ(ap1, "-perm"))
		return(mk(perm, (anode_t *)0, (anode_t *)0, (uint64_t)strtoul(skipsign(ap2), NULL, 8), (uint64_t)sign, ap2));
	else if(EQ(ap1, "-links"))
		return(mk(links, (anode_t *)0, (anode_t *)0, (uint64_t)strtoul(skipsign(ap2),0,10), (uint64_t)sign, ap2));
	else if(EQ(ap1, "-exit"))
		return(mk(xexit, (anode_t *)0, (anode_t *)0, (uint64_t)strtol(ap2,0,10), (uint64_t)0, ap2));
	else if(EQ(ap1, "-newdiradd")) {
                actoptseen++;
                return(mk(newdiradd, (anode_t *)0, (anode_t *)0, (uint64_t)ap2, (uint64_t)0, ap2));
        } else {
		(void) fprintf(stderr,"rfind: bad option %s\n", ap1);
		(void) fprintf(stderr,"Usage: rfind file-system search-expression\n");
		rpcexit(1);
	}
	/* NOTREACHED */
}

anode_t *mk (int (*f)(op_t, anode_t *), anode_t *l, anode_t *r, uint64_t x, uint64_t y, char *dumparg)
{
	static int Nn;  /* number of nodes */

	if(Nn == (MaxArgc-1)) {
		fprintf(stderr, "rfind: more than %d expanded predicates\n", MaxArgc);
		rpcexit(1);
	}
	Node[Nn].F = f;
	Node[Nn].L = l;
	Node[Nn].R = r;
	Node[Nn].X = x;
	Node[Nn].Y = y;
	/* nodes have 3 extra args, but only 2 of them, X & Y, settable via mk() */
	Node[Nn].Z = (uint64_t)0;
	Node[Nn].dumparg = dumparg;
	return(&(Node[Nn++]));
}

char *nxtarg(void) { /* get next arg from command line */
	if(Ai >= Argc)
		return ArgEnd;
	return(Argv[Ai++]);
}

char *peekarg(void) { /* peek at next arg from command line */
	if(Ai >= Argc)
		return ArgEnd;
	return(Argv[Ai]);
}

/* runtime functions */

static int and (op_t op, anode_t *p)
{
	char *funcname = "and";
	int wL, wR;			/* weights of Left and Right children */
	anode_t *pbestchild;

	switch (op) {
	case WGHT:
		wL = ((*p->L->F)(WGHT, p->L));
		wR = ((*p->R->F)(WGHT, p->R));
		if (wL <= wR) {
			p->X = (uint64_t) p->L;	    /*pbestchild */
			return (wL);
		} else {
			p->X = (uint64_t) p->R;	    /*pbestchild */
			return (wR);
		}
	case SCAN:
		pbestchild = (anode_t *)(p->X);
		(pbestchild->F)(SCAN, pbestchild);
		return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		dumplevel++;
		((*p->L->F)(DUMP, p->L));
		((*p->R->F)(DUMP, p->R));
		dumplevel--;
		return 0;
	case EVAL:
		return((*p->L->F)(EVAL, p->L)) && ((*p->R->F)(EVAL, p->R));
	}
	return 0;
}

static int or (op_t op, anode_t *p)
{
	char *funcname = "or";
	int wL, wR;

	switch (op) {
	case WGHT:
		wL = ((*p->L->F)(WGHT, p->L));
		wR = ((*p->R->F)(WGHT, p->R));
		if (wL == HUGEWGHT || wR == HUGEWGHT)
			return HUGEWGHT;
		else if (wL + wR >= MAXWGHT)
			return MAXWGHT;
		else
			return wL + wR;
	case SCAN:
		((*p->L->F)(SCAN, p->L));
		((*p->R->F)(SCAN, p->R));
		return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		dumplevel++;
		((*p->L->F)(DUMP, p->L));
		((*p->R->F)(DUMP, p->R));
		dumplevel--;
		return 0;
	case EVAL:
		return((*p->L->F)(EVAL, p->L)) || ((*p->R->F)(EVAL, p->R));
	}
	return 0;
}

static int not (op_t op, anode_t *p)
{
	char *funcname = "not";

	switch (op) {
	case WGHT:
		((*p->L->F)(WGHT, p->L));
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		dumplevel++;
		((*p->L->F)(DUMP, p->L));
		dumplevel--;
		return 0;
	case EVAL:
		return( !((*p->L->F)(EVAL, p->L)));
	}
	return 0;
}

static int xglob (op_t op, anode_t *p)
{
	char *funcname = "xglob";
	char *pat = (char *) (p->X);

	int w;				/* weight in range {0..MAXWGHT,HUGEWGHT} */
	hpindex_t bot2, top2;		/* lower, upper bounds in hp_dnd2 of matching names */
	hpindex_t botx, topx;		/* lower, upper bounds in hp_dndx of matching names */
	size_t segsz;			/* number of hp_dndx elements per hp_dnd2 element */
	hpindex_t i;
	char *endofname;

	switch (op) {
	case WGHT:
		if (mustdescend) return HUGEWGHT;
		find2interval (mh.hp_dnd2, basename(pat), &bot2, &top2);
		p->Y = (uint64_t) bot2;
		p->Z = (uint64_t) top2;
		w = MAXWGHT * (int)(top2 - bot2);
		w /= heapnsize ((void **)(&mh.hp_dnd2));
		if (w > MAXWGHT) w = MAXWGHT;
#if 0
printf("xglob WGHT <%s> bot2 %d top2 %d weight %d\n", basename(pat), bot2, top2, w);
#endif
		return w;
	case SCAN:
		if (mustdescend) error2 ("SCAN botch in:", funcname);
		bot2 = (hpindex_t)(p->Y);
		top2 = (hpindex_t)(p->Z);
		segsz = ((heapnsize ((void **)&mh.hp_dndx) * sizeof(*mh.hp_dnd2)) / NBPC);
		if (segsz < 1) segsz = 1;
		botx = bot2 * segsz;
		topx = top2 * segsz;

		if (
		    top2 == heapnsize ((void **)(&mh.hp_dnd2))
		 ||
		    topx > heapnsize ((void **)(&mh.hp_dndx))
		)
			topx = heapnsize ((void **)(&mh.hp_dndx));

		findxinterval (mh.hp_dndx, basename(pat), &botx, &topx);
#if 0
printf("xglob SCAN <%s> botx 0x%08x topx 0x%08x\n", basename(pat), botx, topx);
#endif
		if (Mntpt[1] == '\0')		/* if Mntpt == "/" */
			Pathname[0] = '\0';
		else
			(void) strcpy (Pathname, Mntpt);
		endofname = Pathname + strlen (Pathname);

		for (i = botx; i < topx; i++) {
			dent_t *ep = mh.hp_dent + mh.hp_dndx[i].dx_dent;
			char *fname = PNAM (mh, ep);

			if (fname[0] == '.'
			    && (fname[1] == '\0'
				|| fname[1] == '.'
				    && fname[2] == '\0')
					&& !showdots
			) {
				continue;
			}

#if 0
printf("\t<%s>\n",fname);
#endif
			if (gmatch (fname, basename(pat))) {
				inod_t *p2 = PINO (&mh, ep->e_ino);
				if (p2->i_mode == 0) continue;
				(void) strcpy (endofname, path (ep));
				Inodp = PINO (&mh, ep->e_ino);
				(*exlist->F)(EVAL, exlist);
				rpcflushall();
			}
		}
		return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
#if 0
printf("xglob EVAL Pathname <%s> basename <%s> pattern <%s>\n",Pathname,basename(Pathname),pat);
#endif
		return(stgmatch(Pathname, pat));
	}
	return 0;
}

static int print (op_t op, anode_t *p)
{
	char *funcname = "print";

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		puts(real_to_user_root (Pathname));
		return(1);
	}
	return 0;
}

static int ls (op_t op, anode_t *p)
{
	char *funcname = "ls";

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		list(Pathname);
		return(1);
	}
	return 0;
}

static int xprintf (op_t op, anode_t *p)
{
	char *funcname = "xprintf";
	const char *format = (const char *) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		doxprnt (format);
		return(1);
	}
	return 0;
}

static int ncheck (op_t op, anode_t *p)
{
	char *funcname = "ncheck";
	ino64_t inod = (ino64_t) (p->X);
	char sign = (char) (p->Y);

	hpindex_t i;
	char *endofname;

	switch (op) {
	case WGHT:
		if (mustdescend) return HUGEWGHT;
		if (sign == '+' || sign == '-')
			return HUGEWGHT;
		return 1;		/* small weight for single inode lookup */
	case SCAN:
		if (mustdescend) error2 ("SCAN botch in:", funcname);
		if (sign == '+' || sign == '-')
			error2 ("SCAN botch in:", funcname);

		if (Mntpt[1] == '\0')		/* if Mntpt == "/" */
			Pathname[0] = '\0';
		else
			(void) strcpy (Pathname, Mntpt);
		endofname = Pathname + strlen (Pathname);

		Inodp = PINO (&mh, inod);

		for (i = 0; i < Inodp->i_xnlink; i++) {
			dent_t *ep = PLNK (mh, Inodp, i);
			char *fname = PNAM (mh, ep);


			if (fname[0] == '.'
			    && (fname[1] == '\0'
				|| fname[1] == '.'
				    && fname[2] == '\0')
					&& !showdots
			) {
				continue;
			}

			(void) strcpy (endofname, path (ep));
			(*exlist->F)(EVAL, exlist);
			rpcflushall();
		}
		return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		if (scomp((Inodp->i_xino), inod, sign)) {
			printf ("%llu\t%s\n", Inodp->i_xino, real_to_user_root (Pathname));
			return(1);
		}
		return 0;
	}
	return 0;
}

static int mtime (op_t op, anode_t *p)
{
	char *funcname = "mtime";
	time_t mt = (time_t) (p->X);
	char sign = (char) (p->Y);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return scomp ((Now - Inodp->i_mtime) / A_DAY, mt, sign);
	}
	return 0;
}

static int atime (op_t op, anode_t *p)
{
	char *funcname = "atime";
	time_t at = (time_t) (p->X);
	char sign = (char) (p->Y);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return scomp ((Now - Inodp->i_atime) / A_DAY, at, sign);
	}
	return 0;
}

static int xctime (op_t op, anode_t *p)
{
	char *funcname = "xctime";
	time_t ct = (time_t) (p->X);
	char sign = (char) (p->Y);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return scomp ((Now - Inodp->i_ctime) / A_DAY, ct, sign);
	}
	return 0;
}

static int user (op_t op, anode_t *p)
{
	char *funcname = "user";
	uid_t uid = (uid_t) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return(uid == Inodp->i_uid);
	}
	return 0;
}

static int inum (op_t op, anode_t *p)
{
	char *funcname = "inum";
	ino64_t ino = (ino64_t) (p->X);
	char sign = (char) (p->Y);

	hpindex_t i;
	char *endofname;
	dent_t *ep;
	char *fname;

	switch (op) {
	case WGHT:
		if (mustdescend) return HUGEWGHT;
		if (sign == '+' || sign == '-')		/* huge weight for any inode range */
			return HUGEWGHT;
		return 1;				/* small weight to lookup 1 inode */
	case SCAN:
		if (mustdescend) error2 ("SCAN botch in:", funcname);
		if (sign == '+' || sign == '-')
			error2 ("SCAN botch in:", funcname);

		if (Mntpt[1] == '\0')		/* if Mntpt == "/" */
			Pathname[0] = '\0';
		else
			(void) strcpy (Pathname, Mntpt);
		endofname = Pathname + strlen (Pathname);

		Inodp = PINO (&mh, ino);

		for (i = 0; i < Inodp->i_xnlink; i++) {
			ep = PLNK (mh, Inodp, i);
			fname = PNAM (mh, ep);

			if (fname[0] == '.'
			    && (fname[1] == '\0'
				|| fname[1] == '.'
				    && fname[2] == '\0')
					&& !showdots
			) {
				continue;
			}
			break;
		}
		if (i == Inodp->i_xnlink)
			(void) strcpy (endofname, "");
		else
			(void) strcpy (endofname, path (ep));
		(*exlist->F)(EVAL, exlist);
		rpcflushall();
		return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return(scomp((Inodp->i_xino), ino, sign));
	}
	return 0;
}

static int group (op_t op, anode_t *p)
{
	char *funcname = "group";
	uid_t gid = (uid_t) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return(gid == Inodp->i_gid);
	}
	return 0;
}

static int links (op_t op, anode_t *p)
{
	char *funcname = "links";
	nlink_t linkcount = (nlink_t) (p->X);
	char sign = (char) (p->Y);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return(scomp(Inodp->i_nlink, linkcount, sign));
	}
	return 0;
}

static int size (op_t op, anode_t *p)
{
	char *funcname = "size";
	uint64_t sz = (uint64_t) (p->X);
	char sign = (char) (p->Y);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return (scomp (howmany (Inodp->i_size, BLOCKSIZE), sz, sign));
	}
	return 0;
}

static int csize (op_t op, anode_t *p)
{
	char *funcname = "csize";
	uint64_t csz = (uint64_t) (p->X);
	char sign = (char) (p->Y);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return(scomp(Inodp->i_size, csz, sign));
	}
	return 0;
}

static int stsize (op_t op, anode_t *p)
{
	char *funcname = "stsize";

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		subtotal += howmany(Inodp->i_size, BLOCKSIZE);
		return 1;
	}
	return 0;
}

static int perm (op_t op, anode_t *p)
{
	char *funcname = "perm";
	mode_t perm = (mode_t) (p->X);
	char sign = (char) (p->Y);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		if (sign == '-')		/* '-' means only arg bits */
			return ((Inodp->i_mode & perm & 07777) == perm);
		else
			return ((Inodp->i_mode & 07777) == perm);
	}
	return 0;
}

static int type (op_t op, anode_t *p)
{
	char *funcname = "type";
	int per = (int) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return ((Inodp->i_mode & S_IFMT) == per);
	}
	return 0;
}

/* ARGSUSED */
static int prune (op_t op, anode_t *p)
{
	char *funcname = "prune";

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		giveup = 1;
		return(1);
	}
	return 0;
}

static int root (op_t op, anode_t *p)
{
	char *funcname = "root";
	char *rootpath = (char *) (p->X);

	inod_t *q;
	size_t i;
	size_t rootlen;

	switch (op) {
	case WGHT:
		if ((q = xxstat (rootpath, ErrorFlag = showerrors)) == NULL) {
			fprintf(stderr,"Specified path <%s> not found\n", rootpath);
			error2 ("Searching file-system mounted at:", Mntpt);
		}
		/* The next line squirrels strlen (p->t) away for use in SCAN */
		p->Y = (uint64_t) strlen (rootpath);	/* rootlen */
		i = q->i_xnsubtree;
		i *= MAXWGHT;
		i /= PINO (&mh, rootino) -> i_xnsubtree;
#if 0
printf("root WGHT <%s> weight %d\n", rootpath, i);
#endif
		return (int) i;
	case SCAN:
		(void) strcpy(Pathname, rootpath);
		if ((q = xxstat (Pathname, ErrorFlag = showerrors)) == NULL) {
			fprintf(stderr,"Specified path <%s> not found\n", rootpath);
			error2 ("Searching file-system mounted at:", Mntpt);
		}
#if 0
printf("root SCAN <%s> descending\n", Pathname);
#endif
		rootflag = 1;
		descend(q);
		return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		rootlen = (size_t) (p->Y);
#if 0
printf("root EVAL <%s> Pathname <%s> strlen %d", rootpath, Pathname, rootlen);
printf(" ==> %d\n", isancestor (rootpath, Pathname, rootlen));
#endif
		return isancestor (rootpath, Pathname, rootlen);
	}
	return 0;
}

static int sttotal (op_t op, anode_t *p)
{
	char *funcname = "sttotal";
	uint64_t threshold = (uint64_t) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
#if 0
printf("sttotal threshold %lu subtotal %lu rootflag %d\n", threshold, subtotal, rootflag);
#endif
		if (subtotal >= threshold || (subtotal > (uint64_t)0 && rootflag == 1)) {
			agsubtotal = subtotal;
			subtotal = (uint64_t)0;
			return 1;
		} else {
			return 0;
		}
	}
	return 0;
}

static int stcount (op_t op, anode_t *p)
{
	char *funcname = "stcount";

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		subtotal += 1;
		return 1;
	}
	return 0;
}

static int noop (op_t op, anode_t *p)
{
	char *funcname = "noop";

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return(1);
	}
	return 0;
}

static int mnewer (op_t op, anode_t *p)
{
	char *funcname = "mnewer";
	time_t mt = (time_t) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return Inodp->i_mtime > mt;
	}
	return 0;
}

static int cnewer (op_t op, anode_t *p)
{
	char *funcname = "cnewer";
	time_t ct = (time_t) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return Inodp->i_ctime > ct;
	}
	return 0;
}

static int anewer (op_t op, anode_t *p)
{
	char *funcname = "anewer";
	time_t at = (time_t) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		return Inodp->i_atime > at;
	}
	return 0;
}

/*
 * Explanation of what -changed does (and doesn't).
 *
 * -changed applies to the current filename.
 * In the typical usage (by ptools) the current filename
 * is that of an RCS directory, and the objective is to
 * discover if the latest fsdump contains stale data, without
 * doing a whole lot of stat's.  The presumption is that if
 * some RCS file within the current directory was updated,
 * then the directory got touched as a side affect.  So we
 * only need to stat the directories, not the individual files.
 * But we need to be careful to chase down any symlinks to
 * find the real directory that contains the real files.
 *
 * [[ By reports on, below, I mean changed() stats the RCS path
 *    and returns true if some change had occurred since the last dump. ]]
 * 
 * 1) Changed() reports on the parent directory of every hard link of
 *    every regular file contained directly in the directory or referred
 *    to by symlinks from within the directory.
 *
 * 2) It also reports on the initial, current Inodp, file, and if that
 *    was a symlink, it reports on the target of the symlink.
 * 
 * 3) If an RCS directory contains a symlink "foo.c,v", then
 *    changed() returns 1 if the target of the symlink is off the
 *    file system or dangles, or if the target of the symlink is
 *    a file contained in a directory that has changed, BUT not if
 *    the symlink "foo.c,v" itself is newly changed.  So if someone
 *    goes onto bonnie:/cypress and modifies a symlink without touching
 *    its directory, changed() will miss that until the next dump.
 *
 * 4) But I know of no way off hand to change a symlink without
 *    touching the directory it is in, since the symlink(2) call
 *    only modifies a symlink by creating it afresh, and refuses to
 *    do so if the name is already in use.
 *
 * 5) So about the only case that I believe is uncovered is the case
 *    of multi-hop symlinks, where I don't detect if an intermediate
 *    hop has changed.
 * 
 */

static int changed (op_t op, anode_t *p)
{
	char *funcname = "changed";
	inod_t *p1;
	inod_t *last_fchg_inodp;			/* previous inode fchanged called on */

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		if (fchanged (Inodp, Pathname, NULL))
			return 1;
		last_fchg_inodp = Inodp;
		if (S_ISLNK(Inodp->i_mode)) {
			if ((p1 = xsymlink(Inodp)) == NULL)
				return 1;
			if (fchanged (p1, NULL, Mntpt))
				return 1;
			last_fchg_inodp = p1;
		} else {
			p1 = Inodp;
		}
		if ((p1->i_mode&S_IFMT) == S_IFDIR) {
			dent_t *epbase;			/* base of entries in p1 */
			nlink_t i;			/* scans entries in p1 */
			inod_t *q;			/* pointer to inode in dir p1 */
			nlink_t j;			/* scans links to inod q */
			dent_t *ep;			/* a directory entry for q */
			inod_t *qpar = last_fchg_inodp;	/* parent inode of entries of q */

			epbase = PENT (mh, p1, 0);
			for (i=0; i < p1->i_xndent; i++) {
				q = PINO (&mh, epbase[i].e_ino);
				if (S_ISLNK(q->i_mode)) {
					/* cheat - set q to the target of the symlink */
					char *pathend = Pathname + strlen (Pathname);
					*pathend = '/';
					q = xsymlink(q);
					*pathend = 0;
					if (q == NULL)
						return 1;
				}
				if ( ! S_ISREG(q->i_mode) ) continue;

				/*
				 * now q is a regular file that is in directory p1,
				 * or at least pointed to by a symlink therein.
				 */

				for (j=0; j < q->i_xnlink; j++) {
					ep = PLNK (mh, q, j);
					last_fchg_inodp = qpar;
					qpar = PINO (&mh, PDDE(mh,ep)->e_ino);
					if (qpar == last_fchg_inodp) continue;

					if (fchanged (qpar, qpar == Inodp ? Pathname : NULL, Mntpt))
						return 1;
				}
			}
		}
		return 0;
	}
	return 0;
}

static int lchanged (op_t op, anode_t *p)
{
        char *funcname = "lchanged";
        inod_t *p1;
        inod_t *last_fchg_inodp;                        /* previous inode fchanged called on */

        switch (op) {
        case WGHT:
                return HUGEWGHT;
        case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
        case DUMP:
                printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
                return 0;
        case EVAL:
                if (fchanged (Inodp, Pathname, NULL))
                        return 1;
                last_fchg_inodp = Inodp;
		p1 = Inodp;

                if ((p1->i_mode&S_IFMT) == S_IFDIR) {
                        dent_t *epbase;                 /* base of entries in p1 */
                        nlink_t i;                      /* scans entries in p1 */
                        inod_t *q;                      /* pointer to inode in dir p1 */
                        nlink_t j;                      /* scans links to inod q */
                        dent_t *ep;                     /* a directory entry for q */
                        inod_t *qpar = last_fchg_inodp; /* parent inode of entries of q */

                        epbase = PENT (mh, p1, 0);
                        for (i=0; i < p1->i_xndent; i++) {
                                q = PINO (&mh, epbase[i].e_ino);
                                if ( ! S_ISREG(q->i_mode) ) continue;

                                /*
                                 * now q is a regular file that is in directory
p1,
                                 * or at least pointed to by a symlink therein.
                                 */

                                for (j=0; j < q->i_xnlink; j++) {
                                        ep = PLNK (mh, q, j);
                                        last_fchg_inodp = qpar;
                                        qpar = PINO (&mh, PDDE(mh,ep)->e_ino);
                                        if (qpar == last_fchg_inodp) continue;

                                        if (fchanged (qpar, qpar == Inodp ? Pathname : NULL, Mntpt))
                                                return 1;
                                }
                        }
                }
                return 0;
        }
	return 0;
}

static int xexit (op_t op, anode_t *p)
{
	char *funcname = "xexit";
	int exitval = (int) (p->X);

	switch (op) {
	case WGHT:
		return HUGEWGHT;
	case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
	case DUMP:
		printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
		return 0;
	case EVAL:
		rpcexit (exitval);
		/* NOTREACHED */
	}
	return 0;
}

/*
 * Return non-zero iff par is a parent of child
 */

int isparent (inod_t* par, inod_t* child) {
        nlink_t i;      /* scans links to inod child */
        dent_t *ep;     /* a directory entry for child */

        for (i = 0; i < child->i_xnlink; i++) {
                ep = PLNK (mh, child, i);
                if (par->i_xino == PDDE(mh, ep)->e_ino)
                        return 1;
        }
        return 0;
}

static int newdiradd (op_t op, anode_t *p)
{
	const char *format = (const char *) (p->X);

        DIR *dirp;
        struct dirent *dp;
        struct stat64 statbuf;
        inod_t *q;

	char *funcname = "newdiradd";

	switch (op) {
        case WGHT:
                return HUGEWGHT;
        case SCAN:
		error2 ("SCAN botch in:", funcname); return 0;
        case DUMP:
                printf ("%.*s%s[%s]\n",2*dumplevel,dumpspaces,funcname,p->dumparg);
                return 0;
        case EVAL:
		Newdirname[0] = '\0';
		if (!fchanged(Inodp, Pathname, NULL)) 
                	return(0);

		if ((dirp = opendir (Pathname)) == NULL) 
			return(0);

		while ((dp = readdir(dirp)) != NULL) {
			if (strcmp(dp->d_name, ".") == 0 ||
				strcmp(dp->d_name, "..") == 0)
				continue;
			if (dp->d_ino == 0)
				continue;
			q = PINO(&mh,dp->d_ino);
	
			/*
			 * If this inode wasn't assigned or wasn't a directory
			 * or was a directory, but not here, at the last fsdump:
			 */
			if (q->i_mode == 0 || ! S_ISDIR(q->i_mode) || ! isparent (Inodp, q)) {
				/*
				 * but maybe this _pathname_ was a directory here ...
				 * in which case this is a changed directory, not new
				 */
				inod_t *p1;
				strcpy (Newdirname, Pathname);
				strcat (Newdirname, "/");
				strcat (Newdirname, dp->d_name);
				if ((p1 = xxstat (Newdirname, ErrorFlag = quiet)) != NULL
				 && S_ISDIR(p1->i_mode)
				) {
					/* not a _new_ directory - just reincarnated */
					continue;
				}

				/* got one -- if it's a directory, issue a report */
				strcpy (Newdirname, real_to_user_root (Pathname));
				strcat (Newdirname,"/");
				strcat (Newdirname, dp->d_name);
				if (lstat64 (Newdirname, &statbuf) == 0 &&
				   (statbuf.st_mode & S_IFMT) == S_IFDIR) {
					doxprnt(format);
					rpcflushall ();
				}
			}
		}
		return(0);
        }
	return 0;
}
	

/* funny sign compare */
int scomp (uint64_t a, uint64_t b, char sign) {
#if 0
printf("scomp(%u,%u,%c)\n", a, b, sign?sign:'*');
#endif
	if(sign == '+')
		return(a > b);
	if(sign == '-')
		return(a < b);
	return(a == b);
}

const char *skipsign (const char *s) {
	if (s && (*s == '+' || *s == '-'))
		return s + 1;
	return s;
}

static int getunum (
    int     t,          /* do we want UID or GID? */
    char    *s)         /* user or group name */
{
	register int i;
	struct	passwd	*pw;
	struct	group	*gr;

	i = -1;
	if( t == UID ){
		if( ((pw = getpwnam( s )) != (struct passwd *)NULL))
			i = pw->pw_uid;
	} else {
		if( ((gr = getgrnam( s )) != (struct group *)NULL))
			i = gr->gr_gid;
	}
	return(i);
}

/*
 * Implements "-printf format" -- called from xprintf().
 */

void doxprnt (const char *format) {
	register const char *p;
	register char *q;
	char *xp;

	for (p = format; *p; p++) {
	switch (*p) {
	default:
		putchar (*p);
		continue;
	case '\\':
		switch (*++p) {
		case 'b':
			putchar ('\b');
			continue;
		case 'f':
			putchar ('\f');
			continue;
		case 'n':
			putchar ('\n');
			continue;
		case 'r':
			putchar ('\r');
			continue;
		case 't':
			putchar ('\t');
			continue;
		case 'v':
			putchar ('\v');
			continue;
		case '\\':
			putchar ('\\');
			continue;
		default:
			putchar ('\\');
			putchar (*p);
			continue;
		}
	case '%':
		switch (*++p) {
		case 'a':
			printf("%u", Inodp->i_atime);
			continue;
		case 'c':
			printf("%u", Inodp->i_ctime);
			continue;
		case 'm':
			printf("%u", Inodp->i_mtime);
			continue;
		case 'u':
			printf("%u", Inodp->i_uid);
			continue;
		case 'g':
			printf("%u", Inodp->i_gid);
			continue;
		case 'p':
			printf("0%o", Inodp->i_mode);
			continue;
		case 'n':
			printf("%u", Inodp->i_nlink);
			continue;
		case 'i':
			printf("%llu", Inodp->i_xino);
			continue;
		case 's':
			printf("%llu", Inodp->i_size);
			continue;
		case 'e':
			printf("%u", Inodp->i_gen);
			continue;
		case 'd':
			if (S_ISBLK(Inodp->i_mode) || S_ISCHR(Inodp->i_mode))
				printf("%u", Inodp->i_rdev);
			else
				printf("%u", 0);
			continue;
		case 'x':
			if (S_ISDIR(Inodp->i_mode) || S_ISREG(Inodp->i_mode))
				printf("%u", Inodp->i_rdev);	/* numextents */
			else
				printf("%u", 0);
			continue;
		case 'N':
			printf("%u", Inodp->i_xnlink);
			continue;
		case 'E':
			printf("%u", Inodp->i_xndent);
			continue;
		case 'S':
			printf("%llu", Inodp->i_xnsubtree);
			continue;
		case 'I':
			printf("%llu", PINO(&mh, rootino) -> i_xnsubtree);
			continue;
		case 'Q':
			printf("%s", (xp = getfenv(Inodp, FV_QSUM)) == NULL ? "" : xp);
			continue;
		case 'R':
			printf("%s", (xp = getfenv(Inodp, FV_RCSREV)) == NULL ? "" : xp);

			continue;
		case 'T':
			printf("%s", (xp = getfenv(Inodp, FV_RCSDATE)) == NULL ? "" : xp);
			continue;
		case 'L':
			printf("%s", (xp = getfenv(Inodp, FV_RCSLOCK)) == NULL ? "" : xp);
			continue;
		case 'A':
			printf("%s", (xp = getfenv(Inodp, FV_RCSAUTH)) == NULL ? "" : xp);
			continue;
		case 'Y':
			printf("%s", (xp = getfenv(Inodp, FV_SYMLINK)) == NULL ? "" : xp);
			continue;
		case 'U':
			printf("%llu", agsubtotal);
			continue;
		case 'B':
			printf("%s", basename(Pathname));
			continue;
		case 'D':
			q = strdup (real_to_user_root (Pathname));
			printf("%s", dirname(q));
			free (q);
			continue;
		case 'P':
                        printf("%s", real_to_user_root (Pathname));
			continue;
		case 'W':
			printf("%s", Pathname);
			continue;
		case 'Z':
			printf("%s", Newdirname);
			continue;
		case 'M':
			printf("%s", Mntpt);
			continue;
		case '%':
			putchar ('%');
			continue;
		default:
			putchar ('%');
			putchar (*p);
			continue;
		}
	} /* end switch */
	} /* end for loop */

}

/* get name from passwd/group file for a given uid/gid
 *  returns -1 if uid is not in file
 */
int getname (unsigned uid, char *buf, int gidflag) {
	if (gidflag) {
		register struct group *gp;
		gp = getgrgid((int)uid);
		if (!gp)
			return -1;
		sprintf(buf, "%-9.9s", gp->gr_name);
	} else {
		register struct passwd *pp;
		pp = getpwuid((int)uid);
		if (!pp)
			return -1;
		sprintf(buf, "%-9.9s", pp->pw_name);
	}
	return(0);
}


/*
 * Mimics a long ls(1) command print - taken from UCB BSD Tahoe
 */
#define permoffset(who)		((who) * 3)
#define permission(who, type)	((type) >> permoffset(who))

void list(char *file) {
	char pmode[32], uname[32], gname[32], fsize[32], ftime[32];
	static long special[] =  { S_ISUID, S_ISGID, S_ISVTX };
	static char special_char[] = { 's',     's',      't' };
	static time_t sixmonthsago = -1;
	char *flink;
	register int who;
	register char *cp;

	if (file == NULL) return;

	if (sixmonthsago == -1)
		sixmonthsago = Now - 6*30*24*60*60;

	switch ((int) (Inodp->i_mode & S_IFMT)) {
	case S_IFDIR:	/* directory */
		pmode[0] = 'd';
		break;
	case S_IFCHR:	/* character special */
		pmode[0] = 'c';
		break;
	case S_IFBLK:	/* block special */
		pmode[0] = 'b';
		break;
	case S_IFLNK:	/* symbolic link */
		pmode[0] = 'l';
		break;
	case S_IFSOCK:	/* socket */
		pmode[0] = 's';
		break;
	case S_IFREG:	/* regular */
	default:
		pmode[0] = '-';
		break;
	}

	for (who = 0; who < 3; who++) {
		if (Inodp->i_mode & permission(who, S_IREAD))
			pmode[permoffset(who) + 1] = 'r';
		else
			pmode[permoffset(who) + 1] = '-';

		if (Inodp->i_mode & permission(who, S_IWRITE))
			pmode[permoffset(who) + 2] = 'w';
		else
			pmode[permoffset(who) + 2] = '-';

		if (Inodp->i_mode & special[who])
			pmode[permoffset(who) + 3] = special_char[who];
		else if (Inodp->i_mode & permission(who, S_IEXEC))
			pmode[permoffset(who) + 3] = 'x';
		else
			pmode[permoffset(who) + 3] = '-';
	}
	pmode[permoffset(who) + 1] = '\0';

	if (getname(Inodp->i_uid, uname, 0) != 0)
		sprintf(uname, "%-9d", Inodp->i_uid);

	if (getname(Inodp->i_gid, gname, 1) != 0)
		sprintf(gname, "%-9d", Inodp->i_gid);

	if (pmode[0] == 'b' || pmode[0] == 'c')
		sprintf(fsize, "%3d,%4d",
			major(Inodp->i_rdev), minor(Inodp->i_rdev));
	else {
		sprintf(fsize, "%8lld", Inodp->i_size);
		if (S_ISLNK(Inodp->i_mode)) {
			if ((flink = getfenv (Inodp, FV_SYMLINK)) == NULL)
				flink = "";
		}
	}

	cp = ctime(&Inodp->i_mtime);
	if (Inodp->i_mtime < sixmonthsago || Inodp->i_mtime > Now)
		sprintf(ftime, "%-7.7s %-4.4s", cp + 4, cp + 20);
	else
		sprintf(ftime, "%-12.12s", cp + 4);

	printf("%5llu %s %2d %s%s%s %s %s%s%s\n",
		Inodp->i_xino,				/* inode #	*/
		pmode,					/* protection	*/
		Inodp->i_nlink,				/* # of links	*/
		uname,					/* owner	*/
		gname,					/* group	*/
		fsize,					/* # of bytes	*/
		ftime,					/* modify time	*/
		real_to_user_root (file),		/* name		*/
		(pmode[0] == 'l') ? " -> " : "",
		(pmode[0] == 'l') ? flink  : ""		/* symlink	*/
	);

	return;
}

inod_t *dirscan (inod_t *ip, char *name) {
	hpindex_t i;		/* scans number of directory entries */
	dent_t *ep;		/* pointer to each entry in directory */
	char *fname;		/* basename of each entry in directory */

	if ( ! S_ISDIR(ip->i_mode))
		return NULL;

	for (i=0; i < ip->i_xndent; i++) {
		ep = PENT (mh, ip, i);
		fname = PNAM (mh, ep);
		if (strcmp (fname, name) == 0)
			return PINO (&mh, ep->e_ino);
	}

	return NULL;
}

/*
 * Map a root based path to the inode of the named file
 * Return NULL if not found or error.
 * Resolves symbolic links as much as possible (with the current file system).
 * If ErrorFlag == showerrors - it displays error messages and
 *	perhaps exits, otherwise (== quiet) it just returns NULL on error.
 */
 /* ARGSUSED */
inod_t *xxstat (char *pathname, t_eflag errorflag) {
	int loopcnt = 1000;	/* detect infinite symlink loops */
	char buf1[PATH_MAX];	/* mungable copy of pathname */
	char buf2[2*PATH_MAX];	/* tmp copy of buf1 */
	inod_t *ip;		/* current directory/file of evaluation */
	char *restofpath;	/* the still unresolved portion of pathname */
	char *nextcomponent;	/* next component of pathname */
	inod_t *nextip;		/* inode corresponding to nextcomponent */
	char *linkpath;		/* if S_ISLNK: contents of symlink */

	if (*pathname != '/') {
		if (ErrorFlag == showerrors)
			fprintf(stderr,"Path not root based: %s\n", pathname);
		return NULL;
	}

	if ( ! isancestor (Mntpt, pathname, strlen(Mntpt)) ) {
		if (ErrorFlag == showerrors)
			fprintf(stderr,"Path <%s> points off file system <%s>.\n",
				pathname, Mntpt);
		return NULL;
	}

	ip = PINO (&mh, rootino);		/* the root directory for this file system */
	if (strlen (pathname) >= PATH_MAX) {
		if (ErrorFlag == showerrors)
			error2 ("path length >= PATH_MAX", pathname);
		return NULL;
	}
	strcpy (buf1, pathname);
	pathcomp (buf1);
	restofpath = buf1 + strlen (Mntpt);

	while (--loopcnt > 0 && (nextcomponent = strtok (restofpath, "/")) != NULL) {
		restofpath = NULL;
		if ((nextip = dirscan (ip, nextcomponent)) == NULL)
			return NULL;

		if (S_ISLNK (nextip->i_mode)) {
			if ((linkpath = getfenv (nextip, FV_SYMLINK)) == NULL)
				return NULL;
			strcpy (buf2, linkpath);
			if ((restofpath = strtok (0, "")) != NULL) {
				strcat (buf2, "/");
				strcat (buf2, restofpath);
			}
			pathcomp (buf2);
			if (strlen (buf2) >= PATH_MAX) {
				if (ErrorFlag == showerrors)
					error2 ("symlink resolved path length >= PATH_MAX", buf2);
				return NULL;
			}
			strcpy (buf1, buf2);
			restofpath = buf1;
			if (*buf1 == '/') {
				if ( ! isancestor (Mntpt, buf1, strlen(Mntpt)) ) {
					if (ErrorFlag == showerrors)
						fprintf(stderr, "Symlink points off file system: %s\n",buf1);
					return NULL;
				}
				ip = PINO (&mh, rootino);
				restofpath += strlen (Mntpt);
			}
		} else {
			ip = nextip;
		}
	}

	if (loopcnt > 0)
		return ip;
	else {
		if (ErrorFlag == showerrors)
			error2 ("Too many levels of symbolic links:", pathname);
		return NULL;
	}
}

/*
 * xsymlink - converts (inod_t *) that points to a symlink
 *           into an (inod_t *) that points to the target
 *           of that symlink, resolving multiple symlink
 *           hops if present.  Returns NULL if symlink
 *           dangles, loops, points outside the file
 *           system being examined, generates too long a
 *           path, or any other inability to resolve the
 *           path to a non-symlink at the other end.
 */

inod_t *xsymlink(const inod_t *p) {
	char *cp,*dp;
	char spath[2*PATH_MAX];
	inod_t *p1;

	if ((cp = getfenv (p, FV_SYMLINK)) == NULL)
		return NULL;
	if (cp == NULL || *cp == '\0')
		return NULL;
	if (*cp == '/') {
		strcpy (spath, cp);
	} else {
		strcpy (spath, Pathname);
		if ((dp = strrchr (spath, '/')) == NULL)
			return NULL;
		dp[1] = 0;
		strcat (dp, cp);
	}
	if ((p1 = xxstat (spath, ErrorFlag = quiet)) == NULL)
		return NULL;
	if (S_ISLNK (p1->i_mode))
		return NULL;
	return p1;
}

/*
 * nxtcomp  --  working from right to left, return the
 *		next component in a slash separated path.
 *
 *		Dies with assert botch if misused.
 *		Places nul chars into passed in path.
 *		Intended for direct use by stgmatch, below.
 */

char *nxtcomp (char *pathname, char *p) {
	if (p == NULL)
		p = pathname + strlen (pathname);
	if (p == pathname)
		return NULL;
	assert (*p != '/' && p > pathname);
	p--;
	assert (*p == '/' || p[1] == 0);
	if (p == pathname)
		return NULL;
	while (*p == '/' && p > pathname)
		p--;
	if (*p == '/')
		return NULL;
	p[1] = '\0';
	while (*p != '/' && p > pathname)
		p--;
	if (*p == '/')
		p++;
	return p;
}

/*
 * stgmatch -- subtree gmatch -- uses gmatch ('g' for glob)
 * on pattern with multiple components.
 *
 * For example, allows looking for:
 *	-name RCS *,v
 */

int stgmatch (const char *name, const char *pat) {
	char *p = NULL, *q = NULL;
	int rval;
	char *name_tmp, *pat_tmp;

#if 0
printf("stgmatch(name <%s> pat <%s> ", name, pat);
#endif
	if (*pat == 'R' && strcmp (pat, "RCS/*,v") == 0) {
		size_t len = strlen (name);
		const char *p;

#if 0
printf("***\n");
#endif
		if (len < 6) return 0;
		p = name + len;
		p--;
		if (*p-- != 'v') return 0;
		if (*p-- != ',') return 0;
		while (*p != '/' && p > name)
			p--;
		while (*p == '/' && p > name)
			p--;
		p -= 3;
		if (p < name) return 0;
		return p[0] == '/' && p[1] == 'R' && p[2] == 'C' && p[3] == 'S';
	}

	if (strchr (pat, '/') == NULL) {
		/*
		 * Prototype for basename() says it can munge its 1st arg,
		 * but we know better ;).  Ok to pass in const first arg.
		 */
		rval = gmatch (basename((char *)name), pat);
#if 0
printf("\nbasename(%s) ==> %s\n", name, basename(name));
printf("simple ==> %d\n", rval);
#endif
		return rval;
	}

	name_tmp = strdup (name);   /* Since nxtcomp mangles these - must free later */
	pat_tmp = strdup (pat);

	while ((p = nxtcomp (pat_tmp, p)) != NULL) {
		if ((q = nxtcomp (name_tmp, q)) == NULL) {
			rval = 0;
			goto done;
		}
		if (gmatch (q, p) == 0) {
			rval = 0;
			goto done;
		}
	}
	if (*pat_tmp == '/') {
		if (nxtcomp (name_tmp, q) != NULL) {
			rval = 0;
			goto done;
		}
	}
	rval = 1;			/* all components match -- success */
done:
	free (name_tmp);
	free (pat_tmp);
#if 0
printf("==> %d\n", rval);
#endif
	return rval;
}

hpindex_t find2loweredge (str5_t *base, str5_t lopat, str5_t *bp, str5_t *tp) {
	str5_t *probe;

	while (tp > bp) {
		probe = bp + (tp - bp)/2;
		if (*probe < lopat)
			bp = probe + 1;
		else
			tp = probe;
	}
	return (((bp - 1) < base) ? base : (bp - 1)) - base;
}

hpindex_t find2upperedge (str5_t *base, str5_t hipat, str5_t *bp, str5_t *tp) {
	str5_t *probe;

	while (tp > bp) {
		probe = bp + (tp - bp)/2;
		if (*probe <= hipat)
			bp = probe + 1;
		else
			tp = probe;
	}
	return tp - base;
}

void find2interval (str5_t *base, char *pat, hpindex_t *botp, hpindex_t *topp) {
	str5_t *bp, *tp, *probe;
	size_t x;
	char *p;
	char duppat[STR5LEN+1];		/* duplicate modifiable copy of pattern */
	str5_t lopat, hipat;

	bp = base;
	tp = base + heapnsize ((void **)(&mh.hp_dnd2));

	strncpy (duppat, pat, STR5LEN);
	duppat[STR5LEN] = 0;

	p = strpbrk (duppat, "\\[?*");	/* truncate pattern at first wildcard char */
	if (p != NULL) *p = '\0';

	lopat = str5pack (duppat);
	for (x = strlen(duppat); x < STR5LEN; x++)
		duppat[x] = 0xff;
	hipat = str5pack (duppat) + 1;
#if 0
printf("Looking from str5 pattern 0x%08x <%s> ", lopat, str5unpack(lopat));
printf("to 0x%08x <%s>\n", hipat, str5unpack(hipat));
#endif

	while (tp > bp) {
		probe = bp + (tp - bp)/2;
		if (lopat <= *probe) {
			if (*probe < hipat) {
				*botp = find2loweredge (base, lopat, bp, probe);
				*topp = find2upperedge (base, hipat, probe, tp);
#if 0
printf("find2interval ==> botp 0x%x <%s> ", base[*botp], str5unpack(base[*botp]));
printf("0x%x <%s>\n", base[*topp], str5unpack(base[*topp]));
#endif
				return;
			} else {
				tp = probe;
			}
		} else {
			bp = probe+1;
		}
	}
	*botp = (((bp - 1) < base) ? base : (bp - 1)) - base;
	*topp = tp - base;
#if 0
printf("find2interval ==> botp 0x%x <%s> ", base[*botp], str5unpack(base[*botp]));
printf("0x%x <%s>\n", base[*topp], str5unpack(base[*topp]));
#endif
	return;
}

hpindex_t findxloweredge (dndx_t *base, str5_t lopat, dndx_t *bp, dndx_t *tp) {
	dndx_t *probe;

	while (tp > bp) {
		probe = bp + (tp - bp)/2;
		if (probe->dx_str5 < lopat)
			bp = probe + 1;
		else
			tp = probe;
	}
	return (((bp - 1) < base) ? base : (bp - 1)) - base;
}

hpindex_t findxupperedge (dndx_t *base, str5_t hipat, dndx_t *bp, dndx_t *tp) {
	dndx_t *probe;

	while (tp > bp) {
		probe = bp + (tp - bp)/2;
		if (probe->dx_str5 <= hipat)
			bp = probe + 1;
		else
			tp = probe;
	}
	return tp - base;
}

void findxinterval (dndx_t *base, char *pat, hpindex_t *botp, hpindex_t *topp) {
	dndx_t *bp, *tp, *probe;
	size_t x;
	char *p;
	char duppat[STR5LEN+1];		/* duplicate modifiable copy of pattern */
	str5_t lopat, hipat;

	bp = base + *botp;
	tp = base + *topp;

	strncpy (duppat, pat, STR5LEN);
	duppat[STR5LEN] = 0;

	p = strpbrk (duppat, "\\[?*");	/* truncate pattern at first wildcard char */
	if (p != NULL) *p = '\0';

	lopat = str5pack (duppat);
	for (x = strlen(duppat); x < STR5LEN; x++)
		duppat[x] = 0xff;
	hipat = str5pack (duppat) + 1;
#if 0
printf("Looking from str5 pattern 0x%08x <%s> ", lopat, str5unpack(lopat));
printf("to 0x%08x <%s>\n", hipat, str5unpack(hipat));
#endif

	while (tp > bp) {
		probe = bp + (tp - bp)/2;
		if (lopat <= probe->dx_str5) {
			if (probe->dx_str5 < hipat) {
				*botp = findxloweredge (base, lopat, bp, probe);
				*topp = findxupperedge (base, hipat, probe, tp);
#if 0
printf("findxinterval ==> botp 0x%x <%s> ", base[*botp].dx_str5, str5unpack(base[*botp].dx_str5));
printf("0x%x <%s>\n", base[*topp].dx_str5, str5unpack(base[*topp].dx_str5));
#endif
				return;
			} else {
				tp = probe;
			}
		} else {
			bp = probe+1;
		}
	}
	*botp = (((bp - 1) < base) ? base : (bp - 1)) - base;
	*topp = tp - base;
#if 0
printf("findxinterval ==> botp 0x%x <%s> ", base[*botp].dx_str5, str5unpack(base[*botp].dx_str5));
printf("0x%x <%s>\n", base[*topp].dx_str5, str5unpack(base[*topp].dx_str5));
#endif
	return;
}

static void descend(inod_t *p) {
	dent_t *epbase;			/* base of hp_dent entries for this directory */
	register char *c1;		/* original Pathname nul: append next component here */
	size_t namelen;		/* length of Pathname so far */
	char *fname, *endofname;	/* used in appending current filename to Pathname */
	hpindex_t i;			/* scans entries of current directory */
	int dontdescend;		/* set for all but descendable subtrees */
	uint64_t subtotal_this_level = (uint64_t)0;	/* running subtotal for this subtree */
	int rootflag_this_level;	/* set if this level is the final, top, root level */

	rpcflushall();

#if 0
printf("descend(%s)\n", Pathname);
#endif
	Inodp = p;
	if(!depthf)
		(*exlist->F)(EVAL, exlist);
	if((p->i_mode&S_IFMT)!=S_IFDIR) {
		assert (rootflag == 1);
		if(depthf)
			(*exlist->F)(EVAL, exlist);
		return;
	}
	if (giveup) {
		giveup = 0;
		return;
	}

	rootflag_this_level = rootflag;
	rootflag = 0;

	namelen = strlen (Pathname);
	c1 = Pathname + namelen;
	if (*(c1-1) == '/') --c1;
	endofname = c1;

	epbase = PENT (mh, p, 0);

	for (i=0; i < p->i_xndent; i++) {
		inod_t *q = PINO (&mh, epbase[i].e_ino);

		if (q->i_mode == 0)
			(void) fprintf (stderr, "descend (%s) zero inode\n", Pathname);
		fname = PNAM (mh, epbase + i);
		c1 = endofname;
		*c1++ = '/';
		if (namelen + 1 + strlen(fname) + 1 >= PATH_MAX)
			error2 ("rfind: pathname too long",Pathname);
		(void) strcpy(c1, fname);

		if (S_ISLNK(q->i_mode) && followf) {
			inod_t *q1 = xsymlink(q);
			if (q1 != NULL)
				q = q1;
		}

		dontdescend = 0;
		if ((q->i_mode&S_IFMT) != S_IFDIR)
			dontdescend = 1;
		else if (fname[0] == '.'
		    && (fname[1] == '\0'
			|| fname[1] == '.'
			    && fname[2] == '\0')
		) {
			if (!showdots)
				continue;
			else
				dontdescend = 1;
		}

		assert (subtotal == (uint64_t)0);
		assert (agsubtotal == (uint64_t)0);
		if (dontdescend == 1) {
			inod_t *Isavp = Inodp;
			Inodp = q;
			rpcflushall();
			(*exlist->F)(EVAL, exlist);
			Inodp = Isavp;
		} else {
			descend(q);
		}
#if 0
printf("descend subtotal %lu ==> subtotal_this_level %lu\n", subtotal, subtotal_this_level);
#endif
		subtotal_this_level += subtotal;
		agsubtotal = subtotal = (uint64_t)0;
	}
	c1 = endofname;
	*c1 = '\0';
	if (namelen == 1 && strlen (Pathname) == 0)
		strcpy (Pathname, "/");
	if(depthf) {
		Inodp = p;
		subtotal = subtotal_this_level;
		rootflag = rootflag_this_level;
		(*exlist->F)(EVAL, exlist);
	}
	return;
}




/*
 * Load the heap set that is in dumpfile into memory heap set mh,
 * using file heap set header fh along the way.  The main contents of
 * the heap set in dumpfile is simply mmap'd into our memory space.
 */

struct utsname un;

static void restore (char *mntpt, char *dumpfile) {
	char *buf;		/* memory mapped buffer for old dumpfile */
	int flvl;		/* format level seen in buf */
	struct stat64 statbuf;	/* stat results of old dumpfile */
	char unnbuf[sizeof(un.sysname)];
	char unnnbuf[sizeof(un.nodename)];
	char unmbuf[sizeof(un.machine)];
	long	totalinum;

#if 0
printf("restoring mntpt <%s> from dumpfile <%s>\n", mntpt, dumpfile);
#endif
	if ((fs_fd = open (dumpfile, 0)) < 0)
		error2 ("open failed",dumpfile);
	if (fstat64 (fs_fd, &statbuf) < 0)
		error2 ("fstat failed", dumpfile);
	if (statbuf.st_size == 0)
		error2 ("zero length file", dumpfile);
	buf = (char *)mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fs_fd, 0);
	if (buf == (char *)MAP_FAILED)
		error2 ("mmap failed", dumpfile);

        get_fsdump_line(buf);
	if (sscanf (fsdumpline, "fsdump format level %d", &flvl) != 1)
		error2 ("fsdumpfile", "line 1");
	if (flvl < DATA_FILE_FORMAT_LEVEL) {
                (void) fprintf (stderr, "rfind: fatal error: obsolete fsdump data file: %s\n\n", dumpfile);
                (void) fprintf (stderr, "Once \"runfsdump\" has successfully completed, whether from\n");
                (void) fprintf (stderr, "root's crontab entry or executed manually by root in the\n");
                (void) fprintf (stderr, "directory /var/rfindd, then this fsdump data file should\n");
                (void) fprintf (stderr, "be automatically upgraded to the current format.\n");
                (void) fprintf (stderr, "\n");
                (void) fprintf (stderr, "Try rfind again, after this has happened.  On many system\n");
                (void) fprintf (stderr, "configurations, this will be within an hour.\n");
		rpcexit (1);
		/* NOTREACHED */
	} else if (flvl != DATA_FILE_FORMAT_LEVEL) {
		error2 ("Unrecognized fsdump level in", dumpfile);
	}

        get_fsdump_line(NULL);
	if (sscanf (fsdumpline, "%s %s %*s %*s %s", unnbuf, unnnbuf, unmbuf) != 3)
		error2 ("fsdumpfile", "line 2");

        get_fsdump_line(NULL);
	if (sscanf (fsdumpline, "%*s %*s %*s %*s %*s") != 0)		/* xctime */
		error2 ("fsdumpfile", "line 3");
	/* should look at date and see if older ?? */

        get_fsdump_line(NULL);
	if (sscanf (fsdumpline, "Mount point: %s device %d", Mntpt, &Mntdev) != 2)
		error2 ("fsdumpfile", "line 4");

	if (strcmp (mntpt, Mntpt) != 0) {
		fprintf (stderr, "Requested mount point <%s> doesn't match mount point <%s> in fsdump file <%s>\n", mntpt, Mntpt, dumpfile);
		rpcexit (1);
	}

        get_fsdump_line(NULL);
        if (sscanf (fsdumpline, "Total Inodes %ld", &totalinum) != 1)
                error2 ("fsdumpfile", "line 5");
        get_fsdump_line(NULL);
        if (sscanf (fsdumpline, "Root inode %lld", &rootino) != 1)
                error2 ("fsdumpfile", "line 6");
        get_fsdump_line(NULL);
        if (sscanf (fsdumpline, "Prime number used %lld", &primeno) != 1)
                error2 ("fsdumpfile", "line 7");

        get_fsdump_line(NULL);
	if(sscanf(fsdumpline,"inum offset: %lld size %llu", &fh.fh_inum_off, &fh.fh_inum_sz)!=2)
		error2 ("fsdumpfile", "line 8");

        get_fsdump_line(NULL);
        if(sscanf(fsdumpline,"inm2 offset: %lld size %llu", &fh.fh_inm2_off, &fh.fh_inm2_sz)!=2)
                error2 ("fsdumpfile line 9", "");

        get_fsdump_line(NULL);
	if(sscanf(fsdumpline,"inod offset: %lld size %llu", &fh.fh_inod_off, &fh.fh_inod_sz)!=2)
		error2 ("fsdumpfile", "line 10");

        get_fsdump_line(NULL);
	if(sscanf(fsdumpline,"dent offset: %lld size %llu", &fh.fh_dent_off, &fh.fh_dent_sz)!=2)
		error2 ("fsdumpfile", "line 11");

        get_fsdump_line(NULL);
	if(sscanf(fsdumpline,"link offset: %lld size %llu", &fh.fh_link_off, &fh.fh_link_sz)!=2)
		error2 ("fsdumpfile", "line 12");

        get_fsdump_line(NULL);
	if(sscanf(fsdumpline,"name offset: %lld size %llu", &fh.fh_name_off, &fh.fh_name_sz)!=2)
		error2 ("fsdumpfile", "line 13");

        get_fsdump_line(NULL);
	if(sscanf(fsdumpline,"dndx offset: %lld size %llu", &fh.fh_dndx_off, &fh.fh_dndx_sz)!=2)
		error2 ("fsdumpfile", "line 14");

        get_fsdump_line(NULL);
	if(sscanf(fsdumpline,"dnd2 offset: %lld size %llu", &fh.fh_dnd2_off, &fh.fh_dnd2_sz)!=2)
		error2 ("fsdumpfile", "line 15");

        get_fsdump_line(NULL);
	if(sscanf(fsdumpline,"fenv offset: %lld size %llu", &fh.fh_fenv_off, &fh.fh_fenv_sz)!=2)
		error2 ("fsdumpfile", "line 16");

	heapfinit ((void **)(&mh.hp_inum),
                buf, fh.fh_inum_off, fh.fh_inm2_off, sizeof(*mh.hp_inum), fh.fh_inum_sz);
        heapfinit ((void **)(&mh.hp_inm2),
                buf, fh.fh_inm2_off, fh.fh_inod_off, sizeof(*mh.hp_inm2), fh.fh_inm2_sz);
	heapfinit ((void **)(&mh.hp_inod),
		buf, fh.fh_inod_off, fh.fh_dent_off, sizeof(*mh.hp_inod), fh.fh_inod_sz);
	heapfinit ((void **)(&mh.hp_dent),
		buf, fh.fh_dent_off, fh.fh_link_off, sizeof(*mh.hp_dent), fh.fh_dent_sz);
	heapfinit ((void **)(&mh.hp_link),
		buf, fh.fh_link_off, fh.fh_name_off, sizeof(*mh.hp_link), fh.fh_link_sz);
	heapfinit ((void **)(&mh.hp_name),
		buf, fh.fh_name_off, fh.fh_dndx_off, sizeof(*mh.hp_name), fh.fh_name_sz);
	heapfinit ((void **)(&mh.hp_dndx),
		buf, fh.fh_dndx_off, fh.fh_dnd2_off, sizeof(*mh.hp_dndx), fh.fh_dndx_sz);
	heapfinit ((void **)(&mh.hp_dnd2),
		buf, fh.fh_dnd2_off, fh.fh_fenv_off, sizeof(*mh.hp_dnd2), fh.fh_dnd2_sz);
	heapfinit ((void **)(&mh.hp_fenv),
		buf, fh.fh_fenv_off, statbuf.st_size, sizeof(*mh.hp_fenv), fh.fh_fenv_sz);

#if 0
	printf("inod %lu/%lu (%d%%) dent %lu/%lu (%d%%)\nlink %lu/%lu (%d%%) name %lu/%lu (%d%%)\ndndx %lu/%lu (%d%%) fenv %lu/%lu (%d%%)\n",
		A(&mh.hp_inod), B(&mh.hp_inod), percent (A(&mh.hp_inod), B(&mh.hp_inod)),
		A(&mh.hp_dent), B(&mh.hp_dent), percent (A(&mh.hp_dent), B(&mh.hp_dent)),
		A(&mh.hp_link), B(&mh.hp_link), percent (A(&mh.hp_link), B(&mh.hp_link)),
		A(&mh.hp_name), B(&mh.hp_name), percent (A(&mh.hp_name), B(&mh.hp_name)),
		A(&mh.hp_dndx), B(&mh.hp_dndx), percent (A(&mh.hp_dndx), B(&mh.hp_dndx)),
		A(&mh.hp_fenv), B(&mh.hp_fenv), percent (A(&mh.hp_fenv), B(&mh.hp_fenv))
	);
#endif
#if 0
	inumloop();
#endif
#if 0
	namiloop(B(&mh.hp_inod));
#endif
}

/* ARGSUSED */
static void logit (char *s) {
	int fd;
	if ((fd = open ("/var/rfindd/rfindd.log", O_CREAT|O_APPEND|O_WRONLY, 0666)) < 0)
		return;
	write (fd, s, strlen(s));
	close (fd);
}

/* ARGSUSED */
void warn (char *s, char *t) { }		/* rfindd is silent about warnings */

static void error2 (char *s, char *t) {

	if (errno)
		(void) fprintf(stderr,"rfind: fatal error: %s %s <errno: %d>\n", s, t, errno);
	else
		(void) fprintf(stderr,"rfind: fatal error: %s %s \n", s, t);
	rpcexit (1);
	/* NOTREACHED */
}

void error (char *msg) {
	error2 (msg, "");
}
