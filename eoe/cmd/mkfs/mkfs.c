#ident "$Revision: 1.7 $"

/*
 * Front-end program for file-system specific mkfs's.
 * Just figure out which program to execute and exec it.
 * We know everything about efs and xfs mkfs arguments.
 * Any other filesystem type can only be reached via -F or -t fstype
 * as the first & second arguments.
 * Since efs was there first, there's a strong preference for it.
 * This means whenever we get anything we don't understand, we let efs
 * mkfs complain about it.
 */

#include <sys/types.h>
#include <bstring.h>
#include <ctype.h>
#include <mntent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>

int chkexec(char *);
void doexec(char *, int, char **);
void fixpath(void);

main(int argc, char **argv)
{
	int		efs = 0;
	int		i;
	char		**nargv;
	int		xfs = 0;
	static char	efsname[] = MNTTYPE_EFS;
	static char	xfsname[] = MNTTYPE_XFS;

	fixpath();
	/*
	 * No arguments.  Give them the xfs usage message.
	 */
	if (argc < 2)
		doexec(xfsname, argc, argv);
	/*
	 * Is the first thing in the argument list -F fstype or -t fstype?
	 */
	if ((strcmp(argv[1], "-F") == 0 || strcmp(argv[1], "-t") == 0) &&
	    argv[2] != NULL) {
		/*
		 * Copy the remaining arguments & re-exec.
		 */
		nargv = malloc((argc - 1) * sizeof(*nargv));
		bcopy(&argv[3], &nargv[1], (argc - 2) * sizeof(*nargv));
		doexec(argv[2], argc - 2, nargv);
	}
	/*
	 * Must examine the argument list more closely, looking for
	 * distinguishing arguments.
	 * Any - arguments are always first, and most single arguments can
	 * be used to distinguish between efs and xfs.
	 */
	for (i = 1; argv[i] && argv[i][0] == '-'; i++) {
		switch (argv[i][1]) {
		/*
		 * EFS-only options
		 */
		case 'a':
			efs++;
			break;
		/*
		 * XFS-only options
		 */
		case 'b':
		case 'C':
		case 'd':
		case 'l':
		case 'p':
			xfs++;
			break;
		/*
		 * Can't tell which one this is.
		 */
		case 'q':
		default:
			break;
		/*
		 * Can tell with a little work.
		 */
		case 'i':
		case 'r':
			/*
			 * Neither one, if the option letter is followed
			 * by something, or there is no next arg.
			 */
			if (argv[i][2] != '\0' || argv[i + 1] == NULL)
				break;
			/*
			 * If another - option or /filename it's efs.
			 */
			else if (argv[i + 1][0] == '-' || argv[i + 1][0] == '/')
				efs++;
			/*
			 * Option value containing = is xfs.
			 */
			else if (strchr(argv[i + 1], '=') != NULL)
				xfs++;
			else if (argv[i][1] == 'i' &&
				 strcmp(argv[i + 1], "align") == 0)
				xfs++;
			/*
			 * Something wrong, keep looking.
			 */
			else
				break;
		/*
		 * Can tell with a little work.
		 */
		case 'n':
			/*
			 * Neither one, both -n options have a following arg.
			 */
			if (argv[i + 1] == NULL)
				break;
			/*
			 * xfs -n option has an embedded =
			 */
			else if (strchr(argv[i + 1], '=') != NULL)
				xfs++;
			/*
			 * efs -n option is a decimal number
			 */
			else if (isdigit(argv[i + 1][0]))
				efs++;
			/*
			 * Something wrong, keep looking.
			 */
			else
				break;
		}
		if (efs)
			doexec(efsname, argc, argv);
		else if (xfs)
			doexec(xfsname, argc, argv);
	}
	/*
	 * See if both mkfs's are there.
	 * If only one is present then we just run that one.
	 */
	if (!chkexec(efsname))
		doexec(xfsname, argc, argv);
	if (!chkexec(xfsname))
		doexec(efsname, argc, argv);
	/*
	 * i'th argument must be the special device name.
	 * If there are any further arguments, it must be efs.
	 */
	if (argc - i > 1)
		doexec(efsname, argc, argv);
	/*
	 * Else just assume it's xfs.
	 */
	doexec(xfsname, argc, argv);
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
	newpath = malloc(strlen(prefix) + strlen(_PATH_ROOTPATH) + 1);
	strcpy(newpath, prefix);
	strcat(newpath, _PATH_ROOTPATH);
	putenv(newpath);
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
	sprintf(name, "mkfs_%s", fstype);
	argv[0] = name;
	execvp(name, argv);
	perror(name);
	exit(1);
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
	sprintf(name, "mkfs_%s", fstype);
	p = path = strdup(_PATH_ROOTPATH);
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
