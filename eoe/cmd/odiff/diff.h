/*	diff.h	4.7	85/08/16	*/

/*
 * diff - common declarations
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

/*
 * Output format options; declared in diff.c
 */
extern int opt;

#define	D_NORMAL	0	/* Normal output */
#define	D_EDIT		-1	/* Editor script out */
#define	D_REVERSE	1	/* Reverse editor script */
#define	D_CONTEXT	2	/* Diff with context */
#define	D_IFDEF		3	/* Diff with merged #ifdef's */
#define	D_NREVERSE	4	/* Reverse ed script with numbered
				   lines and no trailing . */
/* scriptoutput(opt) is true if requested output format is
 * apparently intended for consumption by another tool, such
 * as editor or preprocessor.  This test is used to decide
 * whether binary files are noted in the normal output, or
 * are considered an error.

 * The rule being:
 *
 * If scriptoutput is true, then diff treats as an error (print
 * message  to stderr, and exit with status == 2) anything that
 * causes it  to not output a script in the requested format.
 * Otherwise the presense of binary files is described on stdout,
 * along with perhaps other normal diff output.
 */
#define scriptoutput(opt) ((opt)==D_EDIT || (opt)==D_REVERSE || (opt)==D_IFDEF || (opt)==D_NREVERSE)

extern int	tflag;		/* expand tabs on output */

/*
 * Algorithm related options
 */
extern int	hflag;		/* -h, use halfhearted DIFFH */
extern int	bflag;		/* ignore blanks in comparisons */
extern int	wflag;		/* totally ignore blanks in comparisons */
extern int	iflag;		/* ignore case in comparisons */

/*
 * Options on hierarchical diffs.
 */
extern int	lflag;		/* long output format with header */
extern int	rflag;		/* recursively trace directories */
extern int	sflag;		/* announce files which are same */
extern char	*start;		/* do file only if name >= this */

/*
 * Variables for -I D_IFDEF option.
 */
extern int	wantelses;	/* -E */
extern char	*ifdef1;	/* String for -1 */
extern char	*ifdef2;	/* String for -2 */
extern char	*endifname;	/* What we will print on next #endif */
extern int	inifdef;

/*
 * Variables for -c context option.
 */
extern int	context;	/* lines of context to be printed */

/*
 * State for exit status.
 */
extern int	status;
extern int	anychange;
extern char	*tempfile;	/* used when comparing against std input */

/*
 * Variables for diffdir.
 */
extern char	**diffargv;	/* option list to pass to recursive diffs */

/*
 * Input file names.
 * With diffdir, file1 and file2 are allocated BUFSIZ space,
 * and padded with a '/', and then efile0 and efile1 point after
 * the '/'.
 */
extern char	*file1, *file2, *efile1, *efile2;
extern struct	stat64 stb1, stb2;

char	*malloc(), *talloc(), *ralloc();
char	*savestr(), *splice(), *splicen();
char	*copytemp(), *rindex();
void	done();

extern	char diffh[], diff[], pr[];
