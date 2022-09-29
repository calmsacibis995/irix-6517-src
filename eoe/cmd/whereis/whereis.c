char _Origin_[] = "UC Berkeley";

#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

static char *bindirs[] = {
	"/usr/bin",
	"/usr/sbin",
	"/usr/bsd",
	"/etc",
	"/usr/etc",
	"/usr/ucb",
	"/usr/games",
	"/usr/demos",
	"/sbin",
	"/lib",
	"/lib32",
	"/lib64",
	"/usr/lib",
	"/usr/lib32",
	"/usr/lib64",
	"/usr/lbin",
	"/usr/local/bin",
	"/usr/local/etc",
	"/usr/local/lib",
	"/usr/bin/X11",
	"/usr/freeware/bin",
	"/usr/freeware/lib",
	0
};

static char *mandirs[] = {
#ifdef sgi
	"/usr/share/catman/a_man/cat1",
	"/usr/share/catman/a_man/cat7",
	"/usr/share/catman/g_man/cat3",
	"/usr/share/catman/p_man/cat1",
	"/usr/share/catman/p_man/cat2",
	"/usr/share/catman/p_man/cat3",
	"/usr/share/catman/p_man/cat3c",
	"/usr/share/catman/p_man/cat3g",
	"/usr/share/catman/p_man/cat3n",
	"/usr/share/catman/p_man/cat3p",
	"/usr/share/catman/p_man/cat3s",
	"/usr/share/catman/p_man/cat3t",
	"/usr/share/catman/p_man/cat3w",
	"/usr/share/catman/p_man/cat3x",
	"/usr/share/catman/p_man/cat4",
	"/usr/share/catman/p_man/cat5",
	"/usr/share/catman/p_man/catD",
	"/usr/share/catman/u_man/cat1",
	"/usr/share/catman/u_man/cat3",
	"/usr/share/catman/u_man/cat6",
	"/usr/share/man/a_man/cat1",
	"/usr/share/man/a_man/cat7",
	"/usr/share/man/g_man/cat3",
	"/usr/share/man/p_man/cat1",
	"/usr/share/man/p_man/cat2",
	"/usr/share/man/p_man/cat3",
	"/usr/share/man/p_man/cat3c",
	"/usr/share/man/p_man/cat3g",
	"/usr/share/man/p_man/cat3n",
	"/usr/share/man/p_man/cat3p",
	"/usr/share/man/p_man/cat3s",
	"/usr/share/man/p_man/cat3t",
	"/usr/share/man/p_man/cat3w",
	"/usr/share/man/p_man/cat3x",
	"/usr/share/man/p_man/cat4",
	"/usr/share/man/p_man/cat5",
	"/usr/share/man/u_man/cat1",
	"/usr/share/man/u_man/cat3",
	"/usr/catman/a_man/cat1",
	"/usr/catman/a_man/cat7",
	"/usr/catman/g_man/cat3",
	"/usr/catman/p_man/cat1",
	"/usr/catman/p_man/cat2",
	"/usr/catman/p_man/cat3",
	"/usr/catman/p_man/cat3c",
	"/usr/catman/p_man/cat3g",
	"/usr/catman/p_man/cat3n",
	"/usr/catman/p_man/cat3p",
	"/usr/catman/p_man/cat3s",
	"/usr/catman/p_man/cat3t",
	"/usr/catman/p_man/cat3w",
	"/usr/catman/p_man/cat3x",
	"/usr/catman/p_man/cat4",
	"/usr/catman/p_man/cat5",
	"/usr/catman/u_man/cat1",
	"/usr/catman/u_man/cat3",
	"/usr/catman/u_man/cat6",
	"/usr/freeware/catman/a_man/cat1",
	"/usr/freeware/catman/a_man/cat7",
	"/usr/freeware/catman/g_man/cat3",
	"/usr/freeware/catman/p_man/cat1",
	"/usr/freeware/catman/p_man/cat2",
	"/usr/freeware/catman/p_man/cat3",
	"/usr/freeware/catman/p_man/cat4",
	"/usr/freeware/catman/p_man/cat5",
	"/usr/freeware/catman/u_man/cat1",
	"/usr/freeware/catman/u_man/cat3",
	"/usr/freeware/catman/u_man/cat6",
#endif
	"/usr/man/u_man/man1",
	"/usr/man/a_man/man1",
	"/usr/man/u_man/man2",
	"/usr/man/u_man/man3",
	"/usr/man/u_man/man4",
	"/usr/man/u_man/man5",
	"/usr/man/u_man/man6",
	"/usr/man/a_man/man7",
	"/usr/man/a_man/man8",
	0
};

static char *mansubdirs[] = {
	"a_man/cat1",
	"a_man/cat7",
	"g_man/cat3",
	"p_man/cat1",
	"p_man/cat2",
	"p_man/cat3",
	"p_man/cat3c",
	"p_man/cat3g",
	"p_man/cat3n",
	"p_man/cat3p",
	"p_man/cat3s",
	"p_man/cat3t",
	"p_man/cat3w",
	"p_man/cat3x",
	"p_man/cat4",
	"p_man/cat5",
	"p_man/catD",
	"u_man/cat1",
	"u_man/cat3",
	"u_man/cat6",
	"cat1",
	"cat2",
	"cat3",
	"cat3g",
	"cat3n",
	"cat3p",
	"cat3s",
	"cat3t",
	"cat3w",
	"cat3x",
	"cat4",
	"cat5",
	"cat6",
	"cat7",
	"cat8",
	"man1",
	"man2",
	"man3",
	"man4",
	"man5",
	"man6",
	"man7",
	"man8",
	0
};

static char *srcdirs[]  = {
	"/usr/src/cmd",
	0
};

char	sflag = 1;
char	bflag = 1;
char	mflag = 1;

char	**Sflag;
int	Scnt;
char	**Bflag;
int	Bcnt;
char	**Mflag;
int	Mcnt;
char	**Pflag;		/* envariable PATH */
int	Pcnt;
char	**MPflag;		/* envariable MANPATH */
int	MPcnt;

char	uflag;
int	count;
int	print;
int	exit_ok=0;

char	cmd_label[] = "UX:whereis";

/*
 * some message prints
 */
static void
usage(void)
{
	_sgi_nl_usage(SGINL_USAGE, cmd_label,
	    gettxt(_SGI_DMMX_whereis_usage,
		"whereis [ -sbmuP ] [ -SBM dir ... -f ] name ..."));
	exit(1);
}

/*
 * error handling
 */
static void
err_nomem(void)
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_outofmem, "Out of memory"));
}

/*
 * get a directory list from command line
 */
static void
getlist(int *argcp, char ***argvp, char ***flagp, int *cntp)
{

	(*argvp)++;
	*flagp = *argvp;
	*cntp = 0;
	for ((*argcp)--; *argcp > 0 && (*argvp)[0][0] != '-'; (*argcp)--)
		(*cntp)++, (*argvp)++;
	(*argcp)++;
	(*argvp)--;
}

/*
 * check if name is already in bin path
 */
int
isinbin(char *p)
{
	register char **s;

	for(s = bindirs; *s; s++) {
	    if( !strcmp(*s, p))
		return(1);
	}
	return(0);
}

/*
 * make a directory list from PATH/MANPATH environment variable
 */
static void
getenvpath(char ***flagp, int *cntp, char *envname)
{
	register char *s, *p;
	register char **pp;
	register int npath;

	*flagp = 0;
	*cntp = 0;
	if( !(s = getenv(envname)))
	    return;
	if( !(p = malloc(strlen(s) + 1))) {
	    err_nomem();
	    return;
	}
	(void)strcpy(p, s);			/* save env string */
	for(npath = 0, s = p; *s; s++) {
	    if(*s == ':')
		npath++;
	}
	if( !(pp = (char **)calloc(npath + 1, sizeof(char *)))) {
	    err_nomem();
	    return;
	}
	for(*flagp = pp, npath = 0, s = p;; s++) {
	    if( !*s)
		npath++;
	    if(npath || (*s == ':')) {
		*s = 0;
		if( !isinbin(p)) {
		    *pp++ = p;			/* make list of char ** */
		    (*cntp)++;
		}
		p = s + 1;			/* next name of path */
	    }
	    if(npath)
		break;				/* end of path */
	}
	*pp = 0;
}

/*
 * clear the flags
 */
static void
zerof(void)
{
	if (sflag && bflag && mflag)
		sflag = bflag = mflag = 0;
}

int
itsit(char *cp, char *dp)
{
	register int i = 14;

	if (dp[0] == 's' && dp[1] == '.' && itsit(cp, dp+2))
		return (1);
	while(*cp && *dp && (*cp == *dp))
		cp++, dp++, i--;
	if(*cp == 0 && *dp == 0)
		return (1);
	while (isdigit(*dp))
		dp++;
	if (*cp == 0 && *dp++ == '.') {
		--i;
		while (i > 0 && *dp)
#ifdef sgi
			if (--i, *dp++ == '.' && *dp != 'z')
#else
			if (--i, *dp++ == '.' )
#endif
				return (*dp++ == 'C' && *dp++ == 0);
		return (1);
	}
	return (0);
}

/*
 * scan directory dir for name cp
 */
void
findin(char *dir, char *cp)
{
	register DIR *d;
	register struct dirent64 *ep;

	if( !(d = opendir(dir)))
	    return;
	while(ep = readdir64(d)) {
	    if( !ep->d_ino)
		continue;
	    if(itsit(cp, ep->d_name)) {
		count++;
		exit_ok++;
		if(print)
		    printf(" %s/%s", dir, ep->d_name);
	    }
	}
	closedir(d);
}

/*
 * search for man page in the standard man subdirectories.
 */
void
findvman(char **dirv, int dirc, char *cp)
{
	char buf[100];
	char **subdir;

	for ( ; dirc > 0; dirv++, dirc--) {
	    subdir = mansubdirs;
	    while (*subdir) {
		sprintf(buf, "%s/%s", *dirv, *subdir++);
		findin(buf, cp);
	    }
	}
}

/*
 * two search routines
 */
void
findv(char **dirv, int dirc, char *cp)
{
	while (dirc > 0)
	    findin(*dirv++, cp), dirc--;
}

void
find(char **dirs, char *cp)
{
	while (*dirs)
	    findin(*dirs++, cp);
}

/*
 * search src, bin, manual or path lists
 */
void
looksrc(char *cp)
{
	if( !Sflag) {
	    find(srcdirs, cp);
	} else
	    findv(Sflag, Scnt, cp);
}

void
lookbin(char *cp)
{
	if( !Bflag)
	    find(bindirs, cp);
	else
	    findv(Bflag, Bcnt, cp);
}

void
lookman(char *cp)
{
	if( !Mflag) {
	    find(mandirs, cp);
	} else
	    findv(Mflag, Mcnt, cp);
}

/*ARGSUSED*/
void
lookpath(char *cp)
{
}

/*
 * search for name
 */
void
lookup(char *cp)
{
	register char *dp;

	for (dp = cp; *dp; dp++)
		continue;
	for (; dp > cp; dp--) {
		if (*dp == '.') {
			*dp = 0;
			break;
		}
	}
	for (dp = cp; *dp; dp++)
		if (*dp == '/')
			cp = dp + 1;
	if (uflag) {
		print = 0;
		count = 0;
	} else
		print = 1;
again:
	if (print)
		printf("%s:", cp);
	if (sflag) {
		looksrc(cp);
		if (uflag && print == 0 && count != 1) {
			print = 1;
			goto again;
		}
	}
	count = 0;
	if (bflag) {
		lookbin(cp);
		if (uflag && print == 0 && count != 1) {
			print = 1;
			goto again;
		}
	}
	count = 0;
	if (mflag) {
		lookman(cp);
		if (uflag && print == 0 && count != 1) {
			print = 1;
			goto again;
		}
	}
	if(Pflag)
	    findv(Pflag, Pcnt, cp);
	if(MPflag)
	    findvman(MPflag, MPcnt, cp);

	if (print)
		printf("\n");
}

/*
 * whereis name
 * look for source, documentation and binaries
 */
int
main(int argc, char *argv[])
{
	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	argc--, argv++;
	if (argc == 0) {
		usage();
	}
	do
		if (argv[0][0] == '-') {
			register char *cp = argv[0] + 1;
			while (*cp) switch (*cp++) {

			case 'f':
				break;

			case 'S':
				getlist(&argc, &argv, &Sflag, &Scnt);
				break;

			case 'B':
				getlist(&argc, &argv, &Bflag, &Bcnt);
				break;

			case 'M':
				getlist(&argc, &argv, &Mflag, &Mcnt);
				break;

			case 'P' : /* XXX can be broken into 2 options */
				getenvpath(&Pflag, &Pcnt, "PATH");
				getenvpath(&MPflag, &MPcnt, "MANPATH");
				continue;

			case 's':
				zerof();
				sflag++;
				continue;

			case 'u':
				uflag++;
				continue;

			case 'b':
				zerof();
				bflag++;
				continue;

			case 'm':
				zerof();
				mflag++;
				continue;

			default:
				usage();
			}
			argv++;
		} else
			lookup(*argv++);
	while (--argc > 0);

	if (exit_ok)
		exit(0);
	else	exit(1);
	/*NOTREACHED*/
}
