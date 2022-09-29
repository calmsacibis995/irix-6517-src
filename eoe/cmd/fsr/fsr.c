#ident "$Id: fsr.c,v 1.1 1999/03/05 23:44:05 cwf Exp $"

/*
 * Front-end program for file-system specific fsr's.
 * Just figure out which program to execute and exec it.  The flags
 * are all EFS and the XFS version ignores the one's it doesn't
 * support (except for -s).
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <bstring.h>
#include <ctype.h>
#include <mntent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>
#include <wait.h>
#include <errno.h>

static void fixpath( void);
static int  chkexec( char *);
static void doexec( char *, int, char **);
static void dosystem( char *, int, char **);
static void mallocargv( int, int, char **, int *, char ***);
static void printargv(char *, int, char **);  /* DEBUG */
static char *efs_leftofffile = "/var/tmp/.fsrlast";
static char *xfs_leftofffile = "/var/tmp/.fsrlast_xfs";

#define DEBUG	1
#ifdef DEBUG
# define	PATHSTR	".:"_PATH_ROOTPATH
#else 
# define	PATHSTR	_PATH_ROOTPATH
#endif /* !DEBUG */

int vflag;
static char *cmd;

int
main(int argc, char **argv)
{
	int		i;
	char		*argname;
	int		optc = 1;
	static char	efsname[] = MNTTYPE_EFS;
	static char	xfsname[] = MNTTYPE_XFS;
	int		xfsargc = 0, efsargc = 0;
	char		**xfsargv, **efsargv;
	struct stat	sb;

	cmd = argv[0];

	fixpath();

	/*
	 * See if both fsr's are there.
	 * If only one is present then we just run that one.
	 */
	if (!chkexec(efsname))
		doexec(xfsname, argc, argv);
	if (!chkexec(xfsname))
		doexec(efsname, argc, argv);

	/* 
	 * Save the option flags.  They need to be passed
	 * to either fsr_efs or fsr_xfs for each files/dirs/FS's 
	 * on the command line.
	 */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			char *optchar;

			optc++;
			optchar = &argv[i][1];
			while (*optchar != '\0') {
				switch (*optchar) {
				case 'v':
					++vflag;
					break;
				/*
				 * Valid EFS options with no optarg's
				 */
				case 'M':
				case 'g':
				case 'n':
				case 'd':
				case 's':
					break;
				/*
				 * Valid EFS options with optarg's
				 */
				case 't':
				case 'f':
				case 'm':
				case 'b':
				case 'p':
					optc++;
					break;
				default:
					fprintf(stderr,
		  "usage: %s [-snvg] [-t mins] [-f leftf] [-m mtab]\n",
					  argv[0]);
					fprintf(stderr,
		  "usage: %s [-snvg] [efsdev | efsdir | efsfile] ...\n",
					  argv[0]);
					fprintf(stderr,
		  "usage: %s [-v] [xfsmntpt | xfsfile] ...\n", argv[0]);
					exit(1);
				}
				optchar++;
			}
		}
	}

	/* 
	 * Loop through the targets and create new arg lists
	 * for the different fsr commands.
	 */
	for (i = optc; i < argc; i++) {
		argname = argv[i];
		if (lstat(argname, &sb) < 0) {
			fprintf(stderr, "could not stat: %s: %s\n",
				argname, strerror(errno));
			continue;
		}
		if (strcmp(sb.st_fstype,"xfs") == 0) {
			if (xfsargc == 0)
			    mallocargv(optc, argc, argv, &xfsargc, &xfsargv);
			xfsargv[xfsargc++] = argname;
		} else {
			if (efsargc == 0)
			    mallocargv(optc, argc, argv, &efsargc, &efsargv);
			efsargv[efsargc++] = argname;
		}
	}
	if (xfsargc && efsargc) {
		dosystem(xfsname, xfsargc, xfsargv);
		doexec(efsname, efsargc, efsargv);
	} else if (xfsargc) {
		doexec(xfsname, xfsargc, xfsargv);
	} else if (efsargc) {
		doexec(efsname, efsargc, efsargv);
	} else {
		/* 
		 * full mtab defrag.  Choose the exec order by
		 * inspecting which one was most recently in 
		 * progress and picking the other.
		 */
		struct stat sb, sb2;

		if (lstat(xfs_leftofffile, &sb) < 0) {
			dosystem(xfsname, argc, argv);
			doexec(efsname, argc, argv);
		} else if (lstat(efs_leftofffile, &sb2) < 0) {
			dosystem(efsname, argc, argv);
			doexec(xfsname, argc, argv);
		} else if (sb.st_mtim.tv_sec < sb2.st_mtim.tv_sec) {
			dosystem(xfsname, argc, argv);
			doexec(efsname, argc, argv);
		} else {
			dosystem(efsname, argc, argv);
			doexec(xfsname, argc, argv);
		}
	}
	return(0);
}

/*
 * Make sure that /sbin is in the
 * path before execing user supplied mkfs program.
 * We set the path to _PATH_ROOTPATH so no security problems can creep in.
 */
void
fixpath(void)
{
	char *newpath;
	static char prefix[] = "PATH=";

	/*
	 * Allocate enough space for the path and the trailing null.
	 */
	newpath = malloc(strlen(prefix) + strlen(PATHSTR) + 1);
	strcpy(newpath, prefix);
	strcat(newpath, PATHSTR);
	putenv(newpath);
}

/*
 * Malloc a new argv and copy specified args into it.
 */
void
mallocargv(
	int copyargs,
	int argc, 
	char **argv, 
	int *nargc, 
	char ***nargv)
{
	if ((*nargv = malloc(argc * sizeof(*nargv))) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	bcopy(argv, *nargv, copyargs * sizeof(*argv));
	*nargc = copyargs;
}

/*
 * Check to see if the executable is there.
 */
int
chkexec(char *fstype)
{
	char	*dir;
	char	*name;
	int	nstrlen;
	char	*p;
	char	*path;
	char	*pn;
	int	rval = 0;

	nstrlen = strlen(fstype) + 5;
	name = malloc(nstrlen + 1);
	sprintf(name, "fsr_%s", fstype);
	p = path = strdup(PATHSTR);
	while (dir = strtok(p, ":")) {
		pn = malloc(strlen(dir) + nstrlen + 2);
		sprintf(pn, "%s/%s", dir, name);
		if (access(pn, EX_OK) == 0) {
			free(pn);
			rval = 1;
			break;
		}
		free(pn);
		p = NULL;
	}
	free(name);
	free(path);
	return rval;
}

/*
 * Construct the name of the program, and exec it.
 * If it fails complain and exit.
 */
void
doexec(char *fstype, int argc, char **argv)
{
	char	*name;

	name = malloc(strlen(fstype) + 6);
	sprintf(name, "fsr_%s", fstype);
	argv[0] = name;
	argv[argc] = NULL;

	if (vflag)
		printargv("doexec", argc, argv);
	execvp(name, argv);
	perror(name);
	exit(1);
}

/*
 * Construct the name of the program, fork/exec it,
 * and wait for it to return.
 */
void
dosystem(char *fstype, int argc, char **argv)
{
	char	*name;
	int	pid;
	int	error = 0;
#define SMALLBUF	128
	char	buf[SMALLBUF];

	name = malloc(strlen(fstype) + 6);
	sprintf(name, "fsr_%s", fstype);
	argv[0] = name;
	argv[argc] = NULL;

	if (vflag)
		printargv("dosystem", argc, argv);

	pid = fork();
	if (pid < 0) {
		sprintf(buf, "%s: failed to fork", cmd);
		perror(buf);
	} else if (pid == 0) {
		execvp(name, argv);
		sprintf(buf, "%s: %s: failed to exec", cmd, name);
		perror(buf);
		exit(1);
	} else {
		wait(&error);
		if (WEXITSTATUS(error)) {
			exit(1);
		}
	}
}

void
printargv(char *str, int argc, char **argv)
{
	int i;
	printf("%s: %s (%d): ", cmd, str, argc);
	for (i=0; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n");
}

