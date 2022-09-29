/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"cmd/find/find.c: $Revision: 1.48 $"

/*	find	COMPILE:	cc -o find -s -O -i find.c -lS	*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/types.h>

#include <dirent.h>
#include <limits.h>
#include <errno.h>

#include <sys/stat.h>
#include <ctype.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <sys/statvfs.h>
#include <sys/fsid.h>
#include <sys/fcntl.h>
#include <sys/fstyp.h>
#include <mntent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <regex.h>
#include <sys/mac.h>
#include <sys/mac_label.h>

#define	UID	1
#define	GID	2
#define	ERRSTAT	(199)	/* a (hopefully) unique exit status */

#define PATHLEN	PATH_MAX

#define A_DAY	86400L		 /* a day full of seconds */
#define EQ(x, y)	(strcmp(x, y)==0)

#define MK_USHORT(a)	(a & 0177777)	/* Make unsigned shorts for        */
					/* portable header.  Hardware may  */
					/* only know integer operations    */
					/* and sign extend the large       */
					/* unsigned short resulting in 8   */
					/* rather than 6 octal char in the */
					/* header.			   */
#define BUFSIZE	512		/* In u370 I can't use BUFSIZ nor BSIZE */
#define CPIOBSZ	4096
#define Bufsize	5120

int	Randlast;
char	Pathname[PATHLEN];

#define MAXNODES 2000	/* max number of nodes.  Used to core dump
	if too many were given... The max value was also much lower
	(only 100) */

int	Nn; 		 /* number of nodes */

struct anode;
typedef	int (*f_t)(struct anode *);
typedef __int64_t i_t;
typedef void *p_t;
typedef __uint64_t x_t;	/* big enough to hold i_t, p_t, anode * */
typedef union aval {
	struct anode *A;
	i_t I;
	p_t P;
} aval_t;
#define	L_A	0x1	/* left node is anode */
#define	L_I	0x2	/* left node is integer */
#define	L_P	0x4	/* left node is pointer */
#define	L_XXX	(L_A|L_I|L_P)
#define	R_A	0x8	/* right node is anode */
#define	R_I	0x10	/* right node is integer */
#define	R_P	0x20	/* right node is pointer */
#define	R_XXX	(R_A|R_I|R_P)

typedef struct anode {
	f_t F;		/* function */
	aval_t L, R;	/* left & right values */
} anode_t;
anode_t Node[MAXNODES];

char	*Fname;
time_t	Now;
int	Argc,
	Ai,
	Pi;
char	**Argv;

/* cpio stuff */

int	Cpio;
short	*SBuf, *Dbuf, *Wp;
char	*Buf, *Cbuf, *Cp;
char	Strhdr[500],
	*Chdr = Strhdr;
int	Wct = Bufsize / 2, Cct = Bufsize;
int	Cflag;
int	depthf = 0;
long	Newer;

int	maxdirfd;
int	followf = 0;

#define	STAT(name,stbp) \
	(followf ? stat64(name, stbp) : lstat64(name, stbp))
#define	CHDIR(ancestor) \
	(followf ? -1 : chdir(ancestor))

int	giveup = 0;	/* abort search in this directory */

struct stat64 Statb;
static mac_t mac_lbl;
static int   mac_label_needed;
static int   mac_enabled;

anode_t	*exp(void), *e1(void), *e2(void), *e3(void);
anode_t	*mk(f_t, x_t, x_t, int);
int	and(anode_t *), or(anode_t *), not(anode_t *), exeq(anode_t *),
	ok(anode_t *), fglob(anode_t *), mtime(anode_t *), atime(anode_t *),
	user(anode_t *), group(anode_t *), size(anode_t *), csize(anode_t *),
	perm(anode_t *), links(anode_t *), print(anode_t *), type(anode_t *),
	ino(anode_t *), depth(anode_t *), cpio(anode_t *), newer(anode_t *),
	localf(anode_t *), mountf(anode_t *), fctime(anode_t *),
	nogroup(anode_t *), nouser(anode_t *), fstype(anode_t *),
	cnewer(anode_t *), anewer(anode_t *), follow(anode_t *),
	prune(anode_t *), mac_inexact(anode_t *), mac_exact(anode_t *),
	mac_dominates(anode_t *), mac_dominated(anode_t *);
char	*nxtarg(void);
char	Home[PATHLEN];

/* Distributed UNIX options:  mount and local				   */
/*									   */
/* mount--When mount is specified the search is restricted to the file	   */
/* system containing the directory specified or, in the case no 	   */
/* directory is specified, the current directory.  This is intended	   */
/* for use by the administrator when backing up file systems.		   */
/*								 	   */
/* local--returns true if the file physically resides on the local machine.*/
/*									   */
/* When find is not used with distributed UNIX 				   */
/*									   */
/* mount--works no different.						   */
/*									   */
/* local--always return true.						   */

statvfs64_t Statvfsb;
char 	*cur_path;

struct mntlist {
	struct mntent	mnt;
	struct mntlist *next;
} *mnthead;

char	local;
char	mount_flag;
char	*openfilelist;		/* which fd's were already open */
int	cur_dev;
long	Blocks;

static mode_t	getmode(char *);
static char	*basename(char *);
void		bintochar(long);
static void	buildmnttab(void);
void		bwrite(short *, int);
int		chgreel(int, int);
int		descend(char *, char *, anode_t *);
int		doex(int);
static int	getunum(int, char *);
static int	ismounted(char *);
static int	isremote(char *);
long		mklong(short *);
int		scomp(i_t, i_t, int);
static void	usage(void);
void		writehdr(char *, int);

	/* Cpio header format */
	struct header {
		short	h_magic,
			h_dev;
		ushort	h_ino,
			h_mode,
			h_uid,
			h_gid;
		short	h_nlink,
			h_rdev,
			h_mtime[2],
			h_namesize,
			h_filesize[2];
		char	h_name[PATHLEN];
	} hdr;

char	cmd_label[] = "UX:find";

int	xpg4 = 0;

/*
 * error handling
 */
static void
err_nomem(void)
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_outofmem, "Out of memory"));
}

static void
err_sys(char *s)
{
	_sgi_nl_error(SGINL_SYSERR2, cmd_label, s);
}

static void
err_nosys(char *s)
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label, s);
}

static void
err_open(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"),
	    s);
}

static void
err_read(char *s)
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotRead, "Cannot read from %s"),
	    s);
}

static void
err_stat(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_cannotstat, "cannot stat %s"),
	    s);
}

static void
err_creat(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotCreat, "Cannot create %s"),
	    s);
}

static void
err_access(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotAccess, "Cannot access %s"),
	    s);
}

static void
err_multiple(char *s)
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_Multiple, "Multiple %s"),
	    s);
}

static void
err_path(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_BadPathname, "%s: Bad pathname"),
	    s);
}

static void
err_id(char *fmt, char *s)
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label, fmt, s);
}

/*
 * SGI support functions
 */
static char *
basename(char *s)
{
	char	*p;

	if( !s || !*s )				/* zero or empty argument */
	    return(".");
	p = s + strlen( s );
	while(p != s && *--p == '/');		/* skip trailing /s */
	while(p != s ) {
	    if(*--p == '/' )
		return(++p);
	}
	return(p);
}

/*
 * buildmnttab
 */
static void
buildmnttab(void)
{
	FILE *mnttabp;
	struct mntent *mnt;
	struct mntlist *mntlistp, *newmntlist;
	char *p;
	static int bagmtab;

	if(bagmtab)
	    return;
	if( !(mnttabp = setmntent(MOUNTED, "r"))) {
	    bagmtab = 1;		/* cant't use mtab, have to stat file */
	    return;
	}

	while ( (mnt = getmntent(mnttabp)) != 0 ) {
		newmntlist = malloc(sizeof(struct mntlist));
		if( !newmntlist) {
			err_nomem();
			goto end;
		}
		p = malloc(strlen(mnt->mnt_dir)+1);
		if( !p) {
			err_nomem();
			goto end;
		}
		strcpy(p, mnt->mnt_dir); 
		newmntlist->mnt.mnt_dir = p;
		p = malloc(strlen(mnt->mnt_type)+1);
		if( !p) {
			err_nomem();
			goto end;
		}
		strcpy(p, mnt->mnt_type); 
		newmntlist->mnt.mnt_type = p;
		newmntlist->mnt.mnt_freq = mnt->mnt_freq;
		newmntlist->mnt.mnt_passno = mnt->mnt_passno;

		if ( mnthead == 0 ) {
			mnthead = newmntlist;
			mntlistp = newmntlist;
			continue;
		}
		mntlistp->next = newmntlist;
		mntlistp = newmntlist;	
	}
	if (mnthead == 0) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_emptymtab, "empty mtab"));
		bagmtab = 1;
		goto end;
	}
	mntlistp->next = 0;
end:
	endmntent(mnttabp);
	return;
}

static int
isremote(char *name)
{
	struct mntlist *mntp;

	if( !mnthead)
	    buildmnttab();
	mntp = mnthead;
	if( !mntp)
	    return(0);
	do {
	    if( !strcmp(mntp->mnt.mnt_dir, name)) {
		/*
		 * mounted
		 * check for NFS and pseudo file systems
		 */
		if( !strcmp(mntp->mnt.mnt_type, MNTTYPE_NFS)
		    || !strcmp(mntp->mnt.mnt_type, MNTTYPE_DBG)
		    || !strcmp(mntp->mnt.mnt_type, MNTTYPE_AFS)
		    || !strcmp(mntp->mnt.mnt_type, MNTTYPE_MFS)
		    || !strcmp(mntp->mnt.mnt_type, MNTTYPE_HWGFS)
		    || !strcmp(mntp->mnt.mnt_type, MNTTYPE_IGNORE))
			return(1);
		else
		    return(0);			/* is local */
	    }
	    mntp = mntp->next;
	} while(mntp);
	return(0);				/* not found = local */
}

static int
ismounted(char *name)
{
	struct mntlist *mntp;

	if( !mnthead)
	    buildmnttab();
	mntp = mnthead;
	if( !mntp)
	    return(0);
	do {
	    if( !strcmp(mntp->mnt.mnt_dir, name)) {
		if(strcmp(name, cur_path))
		    return(1);			/* mounted and not cur_path */
		else
		    return(0);			/* mounted it and cur_path */
	    }
	    mntp = mntp->next;
	} while(mntp) ;
	return(0);
}

static int
getunum(int t, char *s)
{
	int i;
	struct	passwd	*pw;
	struct	group	*gr;

	i = -1;
	if(t == UID) {
	    if( (pw = getpwnam(s)) && pw != (struct passwd *)EOF)
		i = pw->pw_uid;
	} else {
	    if( (gr = getgrnam(s)) && gr != (struct group *)EOF)
		i = gr->gr_gid;
	}
	return(i);
}

/*
 * usage message
 */
static void
usage(void)
{
	(void)_sgi_nl_usage(SGINL_USAGE, cmd_label,
	    gettxt(_SGI_DMMX_usage_find, "find path-list predicate-list"));
	exit(1);
}

/*
 * main entry
 */
int
main(int argc, char *argv[])
{
	anode_t *exlist;
	int paths, x, err = 0;
	char *cp, *sp = 0;
	char	*xpgenv;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	mac_enabled = (sysconf(_SC_MAC) > 0);

	if(time(&Now) == (time_t) -1) {
	    err_sys(gettxt(_SGI_DMMX_canttime, "cannot perform time()"));
	    exit(2);
	}
	/* Is it XPG4 ? */
	xpgenv = (char *)getenv("_XPG");
	xpg4 = xpgenv && (atoi(xpgenv) > 0);

	maxdirfd = getdtablesize();
	openfilelist = calloc(maxdirfd, sizeof(char));
	{
		int fd, maxopen;

		/*
		 * Remember what file descriptors were already open
		 * when find began, and leave only those open in doex().
		 */
		maxopen = getdtablehi();
		for (fd = 0; fd < maxopen; fd++) {
			if ((fcntl(fd, F_GETFD, 0) < 0) && errno == EBADF)
				openfilelist[fd] = 0;
			else
				openfilelist[fd] = 1;
		}
	}
	if (getwd(Home) == NULL) {
		_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO, "%s", Home);
		exit(2);
	}
	Argc = argc; Argv = argv;
	if(argc < 2) {
	    err_nosys(gettxt(_SGI_DMMX_insuffargs, "Insufficient arguments"));
	    usage();
	}
	for(Ai = paths = 1; Ai < argc; ++Ai, ++paths)
	    if(*Argv[Ai] == '-' || EQ(Argv[Ai], "(") || EQ(Argv[Ai], "!"))
		break;
	if(paths == 1)
	    (void)usage();				/* no path list */
	/*
	 * parse and compile the arguments
	 * If no expression is present, implicitly use -print
	 */
	if (Ai < argc) {
	    if(!(exlist = exp())) {
		err_nosys(gettxt(_SGI_DMMX_parserror, "parsing error"));
		exit(1);
	    }
	}
	else {
	    exlist = mk(print, (x_t)0, (x_t)0, 0);
	}

	/* make sure there is at least one of -print, -exec or -ok
	 * in the expression. If not, add an implicit -print
	 */
	for(x = 0; x < Nn; x++)
	    if (Node[x].F == print || Node[x].F == exeq || Node[x].F == ok)
		break;
	if (x == Nn)
	    exlist = mk(and,
			(x_t)exlist,
			(x_t)mk(print, (x_t)0, (x_t)0, 0),
			L_A | R_A);
	
	if(Ai < argc) {
	    err_nosys(gettxt(_SGI_DMMX_missconj, "missing conjunction"));
	    exit(1);
	}
	for(Pi = 1; Pi < paths; ++Pi) {
		sp = "\0";
		strcpy(Pathname, Argv[Pi]);
		if(*Pathname != '/')
			chdir(Home);
		if(cp = strrchr(Pathname, '/')) {
			sp = cp + 1;
			if (*sp != '\0') {
				*cp = '\0';
				if(chdir(*Pathname? Pathname: "/") == -1) {
					err_path(*Pathname? Pathname : "/");
					exit(2);
				}
				*cp = '/';
			}
		}
		Fname = *sp? sp: Pathname;
		if(mount_flag) {
			cur_path = Pathname;
			if (stat64(Fname, &Statb) <0) {
				err_stat(Pathname);
				exit(2);
			} else {
				cur_dev = Statb.st_dev;
			}
		}
		err |= descend(Pathname, Fname, exlist); /* find files that match */
	}
	if(Cpio) {
		strcpy(Pathname, "TRAILER!!!");
		Statb.st_size = 0;
		cpio(NULL);
	}
	exit(err);	/*NOTREACHED*/
}

/* compile time functions:  priority is  exp()<e1()<e2()<e3()  */

anode_t *
exp(void)			/* parse ALTERNATION (-o)  */
{
	anode_t *p1;

	p1 = e1() /* get left operand */ ;
	if(EQ(nxtarg(), "-o")) {
		Randlast--;
		return(mk(or, (x_t)p1, (x_t)exp(), L_A|R_A));
	}
	else if(Ai <= Argc) --Ai;
	return(p1);
}

anode_t *
e1(void)			/* parse CONCATENATION (formerly -a) */
{
	anode_t *p1;
	char *a;

	p1 = e2();
	a = nxtarg();
	if(EQ(a, "-a")) {
And:
		Randlast--;
		return(mk(and, (x_t)p1, (x_t)e1(), L_A|R_A));
	} else if(EQ(a, "(") || EQ(a, "!") || (*a=='-' && !EQ(a, "-o"))) {
		--Ai;
		goto And;
	} else if(Ai <= Argc) --Ai;
	return(p1);
}

anode_t *
e2(void)			/* parse NOT (!) */
{
	if(Randlast) {
		err_nosys(gettxt(_SGI_DMMX_opfollop, "operand follows operand"));
		exit(1);
	}
	Randlast++;
	if(EQ(nxtarg(), "!"))
		return(mk(not, (x_t)e3(), (x_t)0, L_A));
	else if(Ai <= Argc) --Ai;
	return(e3());
}

anode_t *
e3(void)			/* parse parens and predicates */
{
	anode_t *p1, *mkret;
	int s;
	i_t i;
	char *a, *b;

	a = nxtarg();
	if(EQ(a, "(")) {
		Randlast--;
		p1 = exp();
		a = nxtarg();
		if(!EQ(a, ")")) goto err;
		return(p1);
	}
	else if(EQ(a, "-print")) {
		return(mk(print, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-prune")) {
		return(mk(prune, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-depth")) {
		depthf = 1;
		return(mk(depth, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-follow")) {
		followf = 1;
		return(mk(follow, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-local")) {
		local = 1;
		return(mk(localf, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-nouser")) {
		return(mk(nouser, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-nogroup")) {
		return(mk(nogroup, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-mount")) {
		mount_flag = 1;
		return(mk(mountf, (x_t)0, (x_t)0, 0));
	}
	b = nxtarg();
	s = *b;
	if(EQ(a, "-name"))
		return(mk(fglob, (x_t)b, (x_t)0, L_P));
	else if(EQ(a, "-user")) {
		if((i=(i_t)getunum(UID, b)) == -1) {
		    if(!fnmatch("[0-9][0-9][0-9]*", b, 0)
			|| !fnmatch("[0-9][0-9]", b, 0)
			|| !fnmatch("[0-9]", b, 0))
				return mk(user, (x_t)atoll(b), (x_t)s, L_I|R_I);
		    err_id(gettxt(_SGI_DMMX_IllegalUid, "Illegal user id: %s"),
			b);
		    exit(1);
		}
		return(mk(user, (x_t)i, (x_t)s, L_I|R_I));
	}
	else if(EQ(a, "-inum"))
		return(mk(ino, (x_t)atoll(b), (x_t)s, L_I|R_I));
	else if(EQ(a, "-group")) {
		if((i=(i_t)getunum(GID, b)) == -1) {
		    if(!fnmatch("[0-9][0-9][0-9]*", b, 0)
			|| !fnmatch("[0-9][0-9]", b, 0)
			|| !fnmatch("[0-9]", b, 0))
				return mk(group, (x_t)atoll(b), (x_t)s, L_I|R_I);
		    err_id(gettxt(_SGI_DMMX_IllegalGid, "Illegal group id: %s"),
			b);
		    exit(1);
		}
		return(mk(group, (x_t)i, (x_t)s, L_I|R_I));
	}
	else if(EQ(a, "-perm")) {
		if (*b == '-')
		    b++;
		if (*b >= '0' && *b <= '7') {
		    for(i=0; *b ; ++b) {
			if(*b=='-') continue;
			i <<= 3;
			i = i + (*b - '0');
		    }
		}
		else
		    i = getmode(b);
		return(mk(perm, (x_t)i, (x_t)s, L_I|R_I));
	}
	else if(EQ(a, "-type")) {
		i = s=='d' ? S_IFDIR :
		    s=='b' ? S_IFBLK :
		    s=='c' ? S_IFCHR :
		    s=='p' ? S_IFIFO :
		    s=='f' ? S_IFREG :
		    s=='l' ? S_IFLNK :
		    s=='s' ? S_IFSOCK :
		    0;
		if( !i) {
		    char xft[2];

		    xft[0] = s; xft[1] = '\0';
		    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			gettxt(_SGI_DMMX_illfiletype, "%s: illegal file type"),
			xft);
		    exit(1);
		}
		return(mk(type, (x_t)i, (x_t)0, L_I));
	}
	else if(EQ(a, "-fstype")) {
		return(mk(fstype, (x_t)b, (x_t)0, L_P));
	}
	else if (EQ(a, "-exec")) {
		i = Ai - 1;
		while(!EQ(nxtarg(), ";"));
		return(mk(exeq, (x_t)i, (x_t)0, L_I));
	}
	else if (EQ(a, "-ok")) {
		i = Ai - 1;
		while(!EQ(nxtarg(), ";"));
		return(mk(ok, (x_t)i, (x_t)0, L_I));
	}
	else if(EQ(a, "-cpio")) {
		if((Cpio = creat(b, 0666)) < 0) {
			err_creat(b);
			exit(1);
		}
		SBuf = malloc(CPIOBSZ);
		Wp = Dbuf = malloc(Bufsize);
		if (!SBuf || !Dbuf) {
			err_nomem();
			exit(1);
		}
		depthf = 1;
		return(mk(cpio, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-ncpio")) {
		if((Cpio = creat(b, 0666)) < 0) {
			err_creat(b);
			exit(1);
		}
		Buf = malloc(CPIOBSZ);
		Cp = Cbuf = malloc(Bufsize);
		if (!Buf || !Cbuf) {
			err_nomem();
			exit(1);
		}
		Cflag++;
		depthf = 1;
		return(mk(cpio, (x_t)0, (x_t)0, 0));
	}
	else if(EQ(a, "-newer")) {
		if(stat64(b, &Statb) < 0) {
			err_access(b);
			exit(1);
		}
		return mk(newer, (x_t)Statb.st_mtime, (x_t)0, L_I);
	}
	else if(EQ(a, "-cnewer")) {
		if(stat64(b, &Statb) < 0) {
			err_access(b);
			exit(1);
		}
		return mk(cnewer, (x_t)Statb.st_ctime, (x_t)0, L_I);
	}
	else if(EQ(a, "-anewer")) {
		if(stat64(b, &Statb) < 0) {
			err_access(b);
			exit(1);
		}
		return mk(anewer, (x_t)Statb.st_atime, (x_t)0, L_I);
	}
	else if(EQ(a, "-label")) {
		mac_t lbl = mac_from_text(b);
		if (lbl == NULL) {
			(void) _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					     gettxt(_SGI_DMMX_BadOption,
						    "%s: bad option"), b);
			exit(1);
		}
		mac_label_needed = 1;
		return mk(mac_inexact, (x_t)lbl, (x_t)0, L_P);
	}
	else if(EQ(a, "-xlabel")) {
		mac_t lbl = mac_from_text(b);
		if (lbl == NULL) {
			(void) _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					     gettxt(_SGI_DMMX_BadOption,
						    "%s: bad option"), b);
			exit(1);
		}
		mac_label_needed = 1;
		return mk(mac_exact, (x_t)lbl, (x_t)0, L_P);
	}
	else if(EQ(a, "-dominates")) {
		mac_t lbl = mac_from_text(b);
		if (lbl == NULL) {
			(void) _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					     gettxt(_SGI_DMMX_BadOption,
						    "%s: bad option"), b);
			exit(1);
		}
		mac_label_needed = 1;
		return mk(mac_dominates, (x_t)lbl, (x_t)0, L_P);
	}
	else if(EQ(a, "-dominated")) {
		mac_t lbl = mac_from_text(b);
		if (lbl == NULL) {
			(void) _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					     gettxt(_SGI_DMMX_BadOption,
						    "%s: bad option"), b);
			exit(1);
		}
		mac_label_needed = 1;
		return mk(mac_dominated, (x_t)lbl, (x_t)0, L_P);
	}
	else {
		/* This block holds the code which has a second argument
		 * that can have the form '+n'
		 */
		if(s=='+') b++;
		if(EQ(a, "-mtime"))
			return(mk(mtime, (x_t)atoll(b), (x_t)s, L_I|R_I));
		else if(EQ(a, "-atime"))
			return(mk(atime, (x_t)atoll(b), (x_t)s, L_I|R_I));
		else if(EQ(a, "-ctime"))
			return(mk(fctime, (x_t)atoll(b), (x_t)s, L_I|R_I));
		else if(EQ(a, "-size")) {
			mkret = mk(size, (x_t)atoll(b), (x_t)s, L_I|R_I);
			if(*b == '+' || *b == '-')b++;
			while(isdigit(*b))b++;
			if(*b == 'c') Node[Nn-1].F = csize;
			return(mkret);
		} else if(EQ(a, "-links"))
			return(mk(links, (x_t)atoll(b), (x_t)s, L_I|R_I));
	}
err:
	(void)_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_BadOption, "%s: bad option"),
	    a);
	(void)usage();
	/*NOTREACHED*/
}

anode_t *
mk(f_t f, x_t l, x_t r, int m)
{
	if(Nn == (MAXNODES-1)) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_find_handle,
			"can't handle more than %d predicates"),
		    MAXNODES);
		exit(1);
	}
	Node[Nn].F = f;
	switch (m & L_XXX) {
	case L_A:	Node[Nn].L.A = (anode_t *)(__psint_t)l;	break;
	case L_I:	Node[Nn].L.I = (i_t)l;			break;
	case L_P:	Node[Nn].L.P = (p_t)(__psint_t)l;	break;
	}
	switch (m & R_XXX) {
	case R_A:	Node[Nn].R.A = (anode_t *)(__psint_t)r;	break;
	case R_I:	Node[Nn].R.I = (i_t)r;			break;
	case R_P:	Node[Nn].R.P = (p_t)(__psint_t)r;	break;
	}
	return(&(Node[Nn++]));
}

char *
nxtarg(void)			/* get next arg from command line */
{
	static strikes = 0;

	if(strikes==3) {
		err_nosys(gettxt(_SGI_DMMX_IncomplStmt,
		    "incomplete statement"));
		exit(1);
	}
	if(Ai >= Argc) {
		strikes++;
		Ai = Argc + 1;
		return("");
	}
	return(Argv[Ai++]);
}

/* execution time functions */

and(anode_t *p)
{
	return(((*p->L.A->F)(p->L.A)) && ((*p->R.A->F)(p->R.A))?1:0);
}

or(anode_t *p)
{
	 return(((*p->L.A->F)(p->L.A)) || ((*p->R.A->F)(p->R.A))?1:0);
}

not(anode_t *p)
{
	return( !((*p->L.A->F)(p->L.A)));
}

static int	name_flag = 0;
static regex_t  preg;

fglob(anode_t *p)
{

	if (xpg4) {
		/* Whenever fnmatch() uses regcomp, then this
		 * code can be removed.
		 */
		char	*s = (char *)p->L.P;
	
		if (strstr(s, "[") && strstr(s, "]")) {
			regmatch_t  pmatch[1];
			register int nomatch = 0;
			register int	reflag = REG_NOSUB|REG_NEWLINE;
			register int	rexec_flg = 0;
			static	int	re_cmp;
			char	pattern[BUFSIZE];
			char	*t = pattern;
	
			if (!name_flag) {
	
				*t++ = '^';
				while (s && *s) {
					if (*s == '*') {
						*t++ = '.';
						*t++ = '*';
					} else if (*s == '!' && (s-1) && *(s-1) == '[')
						*t++ = '^';	/* Map ! to ^ */
					else
						*t++ = *s;
					s++;
				}
				*t++ = '$';
				*t = '\0';
		
				re_cmp = regcomp(&preg, pattern, reflag);
				name_flag = 1;
			}
		
			if(re_cmp)
				return(0);
			nomatch = regexec(&preg, Fname, (size_t)1, pmatch, rexec_flg);
		
			return(!nomatch);
		}
	}
	return(!fnmatch((char *)p->L.P, Fname, 0));
}

print(anode_t *p)
{
	puts(Pathname);
	return(1);
}

nouser(anode_t *p)
{
	return getpwuid(Statb.st_uid) ? 0 : 1;
}

nogroup(anode_t *p)
{
	return getgrgid(Statb.st_gid) ? 0 : 1;
}

localf(anode_t *p)
{
	if (Statb.st_dev == cur_dev || (Statb.st_mode&S_IFMT) != S_IFDIR)
		return 1;
	return (Statvfsb.f_flag & ST_LOCAL);
}

mountf(anode_t *p)
{
	return (Statb.st_dev == cur_dev);
}


int
cmptime(anode_t *p, time_t tm)
{
	i_t age;
	
	/* -n means less than n*24 hours ago, +n means more than
	 * n*24 hours ago, and n means between (n-1)*24 and n*24 
	 * hours ago.
	 */
	if ((char)p->R.I == '-')
	    age = (Now - tm) / A_DAY;
	else
	    age = (Now - tm) / A_DAY + 1;
	return(scomp(age, p->L.I, p->R.I));
}

mtime(anode_t *p)
{
	return(cmptime(p, Statb.st_mtime));
}

atime(anode_t *p)
{
	i_t	timeref;
	register int	thres;
 
	if ((p->R.I == '-') || (p->R.I == '+')) {
		/* +n   more than n
		 * -n   less than n
		 */
		thres = (Now - Statb.st_atime);
		timeref = p->L.I * A_DAY;
	} else {
		/* n    exactly n
		 * between (n-1) and n of 24hr
		 */
		thres = (Now - Statb.st_atime)/A_DAY + 1;
		timeref = p->L.I;
	}

	return(scomp(thres, timeref, p->R.I));
}

fctime(anode_t *p)
{
	return(cmptime(p, Statb.st_ctime));
}

user(anode_t *p)
{
	return(scomp(Statb.st_uid, p->L.I, p->R.I));
}

ino(anode_t *p)
{
	return(scomp(Statb.st_ino, p->L.I, p->R.I));
}

group(anode_t *p)
{
	return(p->L.I == Statb.st_gid);
}

links(anode_t *p)
{
	return(scomp(Statb.st_nlink, p->L.I, p->R.I));
}

size(anode_t *p)
{
	return(scomp((Statb.st_size + BUFSIZE - 1) / BUFSIZE, p->L.I, p->R.I));
}

csize(anode_t *p)
{
	return(scomp(Statb.st_size, p->L.I, p->R.I));
}

#define	USER	05700	/* user's bits */
#define	GROUP	02070	/* group's bits */
#define	OTHER	00007	/* other's bits */
#define	ALL	07777	/* all */

#define	READ	00444	/* read permit */
#define	WRITE	00222	/* write permit */
#define	EXEC	00111	/* exec permit */
#define	SETID	06000	/* set[ug]id */
#define	LOCK	02000	/* lock permit */
#define	STICKY	01000	/* sticky bit */

static mode_t
who(char **msp)
{
	register mode_t m;

	m = 0;
	for (;; (*msp)++) switch (**msp) {
	case 'u':
		m |= USER;
		continue;
	case 'g':
		m |= GROUP;
		continue;
	case 'o':
		m |= OTHER;
		continue;
	case 'a':
		m |= ALL;
		continue;
	default:
		if (m == 0)
			m = ALL;
		return m;
	}
}

static int
what(char **msp)
{
	switch (**msp) {
	case '+':
	case '-':
	case '=':
		return *(*msp)++;
	}
	return(0);
}

static mode_t
getmode(char *str)
{
    /* m contains USER|GROUP|OTHER information
       o contains +|-|= information
       b contains rwx(slt) information  */

    char *strsave = str;
    mode_t m, b;
    register int o;
    register int lcheck, scheck, xcheck, goon;
    mode_t nm = 0;
    
    do {
	m = who(&str);
	while (o = what(&str)) {
	    /*
	       this section processes permissions
	       */
	    b = 0;
	    goon = 0;
	    lcheck = scheck = xcheck = 0;
	    switch (*str) {
	      case 'u':
		b = (nm & USER) >> 6;
		goto dup;
	      case 'g':
		b = (nm & GROUP) >> 3;
		goto dup;
	      case 'o':
		b = (nm & OTHER);
	      dup:
		b &= (READ|WRITE|EXEC);
		b |= (b << 3) | (b << 6);
		str++;
		goon = 1;
	    }
	    while (goon == 0) switch (*str++) {
	      case 'r':
		b |= READ;
		continue;
	      case 'w':
		b |= WRITE;
		continue;
	      case 'x':
		b |= EXEC;
		xcheck = 1;
		continue;
	      case 'l':
		b |= LOCK;
		m |= LOCK;
		lcheck = 1;
		continue;
	      case 's':
		b |= SETID;
		scheck = 1;
		continue;
	      case 't':
		b |= STICKY;
		continue;
	      default:
		str--;
		goon = 1;
	    }

	    b &= m;

	    switch (o) {
	      case '+':

		/* create new mode */
		nm |= b;
		break;
	      case '-':

		/* create new mode */
		nm &= ~b;
		break;
	      case '=':

		/* create new mode */
		nm &= ~m;
		nm |= b;
		break;
	    }
	}
    } while (*str++ == ',');
    if (*--str) {
	(void)_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_BadOption, "%s: bad option"),
			    strsave);
	(void)usage();
	/*NOTREACHED*/
    }
    return(nm);
}

perm(anode_t *p)
{
	int i;
	i = (p->R.I=='-') ? p->L.I : 07777; /* '-' means only arg bits */
	return((Statb.st_mode & i & 07777) == p->L.I);
}

type(anode_t *p)
{
	return((Statb.st_mode&S_IFMT)==p->L.I);
}

fstype(anode_t *p)
{
	struct statvfs64 svb;
	char *statname = ((Statb.st_mode&S_IFMT) == S_IFLNK) ? "." : Fname;

	if (statvfs64(statname, &svb) == -1) {
		fprintf(stderr,"%s: Can't stat vfs: ", Pathname);
		perror("");
		return 0;
	}
	return !strcmp((char *)p->L.P, svb.f_basetype);
}

prune(anode_t *p)
{
	if ((Statb.st_mode&S_IFMT)==S_IFDIR)
		giveup = 1;
	return(1);
}

exeq(anode_t *p)
{
	fflush(stdout); /* to flush possible `-print' */
	return(doex(p->L.I));
}

ok(anode_t *p)
{
	int c, yes=0;

	fflush(stdout); /* to flush possible `-print' */
	(void) fprintf(stderr,"< %s ... %s >?   ", Argv[p->L.I], Pathname);
	fflush(stderr);
	if((c=getchar())=='y') yes = 1;
	while(c!='\n')
		if(c==EOF)
			exit(2);
		else
			c = getchar();
	return(yes? doex(p->L.I): 0);
}

#define MKSHORT(v, lv) {U.l=1L;if(U.c[0]) U.l=lv, v[0]=U.s[1], v[1]=U.s[0]; else U.l=lv, v[0]=U.s[0], v[1]=U.s[1];}
union { long l; short s[2]; char c[4]; } U;

long
mklong(short *v)
{
	U.l = 1;
	if(U.c[0] /* VAX */)
		U.s[0] = v[1], U.s[1] = v[0];
	else
		U.s[0] = v[0], U.s[1] = v[1];
	return U.l;
}

depth(anode_t *p)
{
	return(1);
}

follow(anode_t *p)
{
	return(1);
}

#define	MAGIC	070707
#define HDRSIZE	(hdr.h_name - (char *)&hdr)	/* hdr size - filename field */
#define CHARS	76			/* ASCII hdr size - filename field */

cpio(anode_t *p)
{
	int ifile, ct;
	long fsz;
	int i;

	strcpy(hdr.h_name, !strncmp(Pathname, "./", 2)? Pathname+2: Pathname);
	hdr.h_magic = MAGIC;
	hdr.h_namesize = strlen(hdr.h_name) + 1;
	hdr.h_uid = Statb.st_uid;
	hdr.h_gid = Statb.st_gid;
	hdr.h_dev = Statb.st_dev;
	hdr.h_ino = Statb.st_ino;
	hdr.h_mode = Statb.st_mode;
	MKSHORT(hdr.h_mtime, Statb.st_mtime);
	hdr.h_nlink = Statb.st_nlink;
	/*
	 * Added for xfs
	 */
	if (Statb.st_size > LONG_MAX) {
		fprintf(stderr, "Skipping file -> %s. Use cpio command with -K flag to archive.\n", hdr.h_name);
		fflush(stderr);
		return 0;
	}
	fsz = (hdr.h_mode & S_IFMT) == S_IFREG? (long)Statb.st_size: 0L;
	MKSHORT(hdr.h_filesize, fsz);
	hdr.h_rdev = Statb.st_rdev;
	if (Cflag)
		bintochar(fsz);

	if(EQ(hdr.h_name, "TRAILER!!!")) {
		Cflag? writehdr(Chdr, CHARS + hdr.h_namesize):
			bwrite((short *)&hdr, HDRSIZE + hdr.h_namesize);
		for (i = 0; i < 10; ++i)
			Cflag? writehdr(Buf, BUFSIZE): bwrite(SBuf, BUFSIZE);
		return 0;
	}
	if(!mklong(hdr.h_filesize)) {
		Cflag? writehdr(Chdr, CHARS + hdr.h_namesize):
			bwrite((short *)&hdr, HDRSIZE + hdr.h_namesize);
		return 0;
	}
	if((ifile = open(Fname, 0)) < 0) {
		(void)_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_CannotCopy, "Cannot copy %s"),
		    hdr.h_name);
		return 0;
	}
	Cflag? writehdr(Chdr, CHARS + hdr.h_namesize):
		bwrite((short *)&hdr, HDRSIZE+hdr.h_namesize);
	for(fsz = mklong(hdr.h_filesize); fsz > 0; fsz -= CPIOBSZ) {
		ct = fsz>CPIOBSZ? CPIOBSZ: fsz;
		if(read(ifile, Cflag? Buf: (char *)SBuf, ct) < 0)  {
			err_read(hdr.h_name);
			continue;
		}
		Cflag? writehdr(Buf, ct): bwrite(SBuf, ct);
	}
	close(ifile);
	return 1;
}

void
bintochar(long t)		/* ASCII header write */
{
	sprintf(Chdr,"%.6ho%.6ho%.6ho%.6ho%.6ho%.6ho%.6ho%.6ho%.11lo%.6ho%.11lo%s",
		MAGIC, MK_USHORT(Statb.st_dev), MK_USHORT(Statb.st_ino),
		Statb.st_mode, Statb.st_uid,
		Statb.st_gid, Statb.st_nlink, MK_USHORT(Statb.st_rdev),
		Statb.st_mtime, (short)strlen(hdr.h_name)+1, t, hdr.h_name);
}

newer(anode_t *p)
{
	return Statb.st_mtime > p->L.I;
}

cnewer(anode_t *p)
{
	return Statb.st_ctime > p->L.I;
}

anewer(anode_t *p)
{
	return Statb.st_atime > p->L.I;
}

mac_exact(anode_t *p)
{
	mac_t P = (mac_t) p->L.P;

	return (mac_enabled ? P->ml_msen_type == mac_lbl->ml_msen_type && P->ml_mint_type == mac_lbl->ml_mint_type && mac_equal(P, mac_lbl) > 0 : 1);
}

mac_inexact(anode_t *p)
{
	return (mac_enabled ? mac_equal((mac_t) p->L.P, mac_lbl) > 0 : 1);
}

mac_dominates(anode_t *p)
{
	return (mac_enabled ? mac_dominate((mac_t) p->L.P, mac_lbl) > 0 : 1);
}

mac_dominated(anode_t *p)
{
	return (mac_enabled ? mac_dominate(mac_lbl, (mac_t) p->L.P) > 0 : 1);
}

/* support functions */
scomp(i_t a, i_t b, int s) /* funny signed compare */
{
	if(s == '+')
		return(a > b);
	if(s == '-')
		return(a < -(b));
	return(a == b);
}

doex(int com)
{
	int np;
	char *na;
	static char *nargv[50];
	static ccode;
	static pid;

	ccode = np = 0;
	while (na=Argv[com++]) {
		if(strcmp(na, ";")==0) break;
		if(strcmp(na, "{}")==0) nargv[np++] = Pathname;
		else nargv[np++] = na;
	}
	nargv[np] = 0;
	if (np==0) return(9);
	if(pid = fork())
		if (pid < 0)
			err_sys(gettxt(_SGI_DMMX_CannotFork, "Cannot fork"));
		else
		while(wait(&ccode) != pid);
	else { /*child*/
		int fd;
		int maxopen = getdtablehi();
		for (fd = 0; fd < maxopen; fd++)
			if (openfilelist[fd] == 0)
				(void) close(fd);
		chdir(Home);
		execvp(nargv[0], nargv);
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt("uxcore:148", "Cannot execute %s: %s"),
		    nargv[0], strerror(errno));
		exit(ERRSTAT);
	}

	if (WEXITSTATUS(ccode) == ERRSTAT)
		exit(1);

	return(ccode ? 0:1);
}

static int
do_label(const char *name)
{
	if (mac_label_needed && mac_enabled) {
		if (mac_lbl != NULL)
			mac_free(mac_lbl);
		mac_lbl = mac_get_file(name);
		return (mac_lbl == NULL ? -1 : 0);
	}
	return (0);
}

int
descend(char *name, char *fname, anode_t *exlist)
{
	int	dir = 0, /* open directory */
		dsize,
		entries,
		cdval = 0;
	int err = 0;
	DIR *dirp;		/* pointer to "directory" file */
	dirent64_t *dp;	/* pointer to abstract entry */
	char *c1, *c2;
	off64_t	offset;
	int i;
	int namelen;
	char *endofname;

	/*
	 * try to determine whether mounted/local without actually
	 * stat'ing file - this makes find work much better
	 * with dead-NFS mounts
	 */
	if (mount_flag && ismounted(name))
		return(0);
	if (local && isremote(name))
		return(0);
	if(STAT(fname, &Statb)<0) {
		err_stat(name);
		return(1);
	}
	if(mount_flag && (Statb.st_dev != cur_dev))
		return(0);
	if(local && (Statb.st_mode&S_IFMT)==S_IFDIR
	   && Statb.st_dev != cur_dev) {
		if(statvfs64(fname, &Statvfsb) < 0) {
			_sgi_nl_error(SGINL_SYSERR, cmd_label,
			    gettxt(_SGI_DMMX_cannotstatfs, "cannot statfs %s"),
			    name);
			return(1);
		}
		if(!localf(NULL))
			return(0);
	}
	if (do_label(fname) == -1) {
		err_stat(name);
		return(1);
	}
	if((Statb.st_mode&S_IFMT)!=S_IFDIR) {
		(*exlist->F)(exlist);
		return(0);
	} else if(!depthf) {
		(*exlist->F)(exlist);

		if (giveup) {
			giveup = 0;
			return(0);
		}
	}

	for(c1 = name; *c1; ++c1);
	namelen = (int)(c1-name);
	if(*(c1-1) == '/')
		--c1;
	endofname = c1;

	dirp = NULL;
	offset = 0;
	if((cdval=chdir(fname)) == -1) {
		_sgi_nl_error(SGINL_SYSERR, cmd_label,
		    gettxt(_SGI_DMMX_CannotChdir, "Cannot chdir to %s"),
		    name);
		err = 1;
	} else {
		for (;;) {
			if (dirp == NULL) {
				dirp = opendir(".");
				if (dirp == NULL) {
					err_open(name);
					err = 1;
					break;
				}
				if (offset != 0) {
					seekdir64(dirp, offset);
				}
			}
			dp = readdir64(dirp);
			if (dp == NULL) {
				break;
			}
			if (dp->d_ino == 0
			    || (dp->d_name[0] == '.'
				&& (dp->d_name[1] == '\0'
				    || dp->d_name[1] == '.'
					&& dp->d_name[2] == '\0'))) {
				continue;
			}
			c1 = endofname;
			*c1++ = '/';
			if (namelen + 1 + strlen(dp->d_name) + 1 >= PATHLEN) {
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_Path2long,
					"Pathname too long (%s/%s)"),
				    name, dp->d_name);
				exit(2);
			}
			(void) strcpy(c1, dp->d_name);
			Fname = endofname+1;
			if (dirp->dd_fd == maxdirfd-1) {
				offset = telldir64(dirp);
				closedir(dirp);
				dirp = NULL;
			}
			err |= descend(name, Fname, exlist);
		}
	}
	if(dirp)
		closedir(dirp);
	c1 = endofname;
	if(c1==fname) *c1='/'; else *c1 = '\0';	
	Fname = basename(name);
	if(cdval == -1 || CHDIR("..") == -1) {
		if((endofname=strrchr(Pathname,'/')) == Pathname)
			chdir("/");
		else {
			if(endofname != NULL)
				*endofname = '\0';
			chdir(Home);
			if(chdir(Pathname) == -1) {
				err_path(Pathname);
				exit(1);
			}
			if(endofname != NULL)
				*endofname = '/';
		}
	}
	if(depthf) {
		if(STAT(fname, &Statb) < 0) {
			err_stat(fname);
			err = 1;
		}
		if (do_label(fname) == -1) {
			err_stat(fname);
			err = 1;
		}
		(*exlist->F)(exlist);
		/* -prune and -depth don't really play.. */
		giveup = 0;
	}
/*	*c1 = '/';	*/
	return(err);
}

void
bwrite(short *rp, int c)
{
	short *wp = Wp;

	c = (c+1) >> 1;
	while(c--) {
		if(!Wct) {
again:
			if(write(Cpio, (char *)Dbuf, Bufsize)<0) {
				Cpio = chgreel(1, Cpio);
				goto again;
			}
			Wct = Bufsize >> 1;
			wp = Dbuf;
			++Blocks;
		}
		*wp++ = *rp++;
		--Wct;
	}
	Wp = wp;
}

void
writehdr(char *rp, int c)
{
	char *cp = Cp;

	while (c--)  {
		if (!Cct)  {
again:
			if(write(Cpio, Cbuf, Bufsize) < 0)  {
				Cpio = chgreel(1, Cpio);
				goto again;
			}
			Cct = Bufsize;
			cp = Cbuf;
			++Blocks;
		}
		*cp++ = *rp++;
		--Cct;
	}
	Cp = cp;
}

chgreel(int x, int fl)
{
	int f;
	char str[22];
	FILE *devtty;
	struct stat64 statb;

	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    x? gettxt(_SGI_DMMX_CannotWr, "Cannot write output")
	    : gettxt(_SGI_DMMX_CannotRd, "Cannot read input"));
	fstat64(fl, &statb);
	if((statb.st_mode&S_IFMT) != S_IFCHR)
		exit(1);
again:
	close(fl);
	_sgi_ffmtmsg(stderr, 0, cmd_label, MM_FIX, gettxt(_SGI_DMMX_find_GoOn,
	    "If you want to go on, type device/file name"));
	devtty = fopen("/dev/tty", "r");
	fgets(str, 20, devtty);
	str[strlen(str) - 1] = '\0';
	if(!*str)
		exit(1);
	if((f = open(str, x? 1: 0)) < 0) {
		_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO,
		    gettxt(_SGI_DMMX_didnotwork, "That didn't work"));
		fclose(devtty);
		goto again;
	}
	return f;
}
