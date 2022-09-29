/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mkmsgs:mkmsgs.c	1.4.7.1"

/***************************************************************************
 * Command: mkmsgs
 * Notes: create message files for use by gettxt. 
 *	  Privileges are required if the file will be created in the public
 *	  directory, /usr/lib/<locale>/LC_MESSAGES.
 ***************************************************************************/


/* 
 * Create message files in a specific format.
 * the gettxt message retrieval function must know the format of
 * the data file created by this utility.
 *
 * 	FORMAT OF MESSAGE FILES

	 __________________________
	|  Number of messages      |
	 --------------------------
	|  offset to the 1st mesg  |
	 --------------------------
	|  offset to the 2nd mesg  |
	 --------------------------
	|  offset to the 3rd mesg  |
	 --------------------------
	|          .		   |
	|	   .	           |
	|	   .		   |
	 --------------------------
	|  offset to the nth mesg  |
	 --------------------------
	|    message #1
	 --------------------------
	|    message #2
	 --------------------------
	|    message #3
	 --------------------------
		   .
		   .
		   .
	 --------------------------
	|    message #n
	 --------------------------
*
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <locale.h>
#include <fmtmsg.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

/* 
 * Definitions
 */

#define	LINESZ	2048	/* max line in input base */
#define STDERR  2
#define P_locale	"/usr/lib/locale/"	/* locale info directory */
#define L_locale	sizeof(P_locale)
#define MESSAGES	"/LC_MESSAGES/"		/* messages category */

/*
 * external functions
 */

extern void	*malloc();
extern char	*tempnam();
extern char	*strccpy();
extern int	access();
extern void	exit();
extern void	free();
extern int	getopt();

/*
 * external variables
 */

extern  int	errno;		/* System error code */
extern  char	*sys_errlist[]; /* Error messages */
extern  int	sys_nerr;	/* Number of sys_errlist entries */
extern  int	optind;
extern  char	*optarg;

/*
 * internal functions
 */

static  void	usage();        /* Displays valid invocations */
static  int	mymkdir();      /* Creates sub-directories */
static  void	clean();	/* removes work file */

/*
 * static variables
 */

static	char    *workp;		/* name of the work file */
static	uid_t	uid;		/* owner of data files */
static	gid_t	gid;		/* group of data files */

char	cmd_label[] = "UX:mkmsgs";

/*
 * usage
 */
static void
usage()
{
	_sgi_nl_usage(SGINL_USAGE, cmd_label,
	    gettxt(_SGI_DMMX_mkmsgs_usage,
		"mkmsgs [-o] [-i locale] ifile msgfile"));
	exit(1);
}

/*
 * some msg prints
 */
static void
err_nomem()
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_outofmem, "Out of memory"));
}

static void
err_open(s)
char *s;
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"), s);
}

static void
err_read(s)
char *s;
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotRead, "Cannot read from %s"), s);
}

static void
err_write(s)
char *s;
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotWrite, "Cannot write to %s"), s);
}

static void
err_stat(s)
char *s;
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_cannotstat, "cannot stat %s"), s);
}

static void
err_chown(s)
char *s;
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_Cannot_chown, "Cannot chown %s"), s);
}

/*
 * Procedure:     main
 *
 * Restrictions:
                 getpwnam:	none
                 getgrnam:	none
                 lvlin:		none
                 getopt:	none
                 access(2):	none
                 fopen:		none
                 tempnam:	none
                 fgets:		none
                 fwrite:	none
                 fclose:	none
                 stat(2):	none
                 fread:		none
                 unlink(2):	none
                 chown(2):	none
                 lvlfile(2):	none
*/

int
main(argc, argv)
int argc;
char *argv[];
{
	int c;				/* contains option letter */
	char	*ifilep;		/* input file name */
	char	*ofilep;		/* output file name */
	char	*localep; 		/* locale name */
	char	*localedirp;    	/* full-path name of parent directory
				 	 * of the output file */
	char	*outfilep;		/* full-path name of output file */
	FILE *fp_inp; 			/* input file FILE pointer */
	FILE *fp_outp;			/* output file FILE pointer */
	char *bufinp, *bufworkp;	/* pointers to input and work areas */
	int  *bufoutp;			/* pointer to the output area */
	char *msgp;			/* pointer to the a message */
	int num_msgs;			/* number of messages in input file */
	int iflag;			/* -i option was specified */
	int oflag;			/* -o option was slecified */
	int nitems;			/* number of bytes to write */
	char *pathoutp;			/* full-path name of output file */
	struct stat buf;		/* buffer to stat the work file */
	unsigned size;			/* used for argument to malloc */
	struct passwd *pwent;		/* passwd entry for file owner */
	struct group *grpent;		/* group entry for file group */
	int i;				

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	/* Initializations */

	localep = (char *)NULL;
	num_msgs = 0;
	iflag   = 0;
	oflag   = 0;

	/* Get owner/group information for file ownership */

	if ((pwent = getpwnam("root")) == (struct passwd *)NULL ||
	    (grpent = getgrnam("sys")) == (struct group *)NULL) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_cannot_get_og,
			"can't get owner/group information"));
		exit(1);
	} else {
		uid = pwent->pw_uid;
		gid = grpent->gr_gid;
	}


	/* Check for invalid number of arguments */

	if (argc < 3 || argc > 6) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_insuffargs, "Insufficient arguments"));
		usage();
	}

	/* Get command line options */

	while ((c = getopt(argc, argv, "oi:")) != EOF) {
		switch (c) {
		case 'o':
			oflag++;
			break;
		case 'i':
			iflag++;
			localep = optarg;
			break;
		case '?':
			usage();
			break;
		}
	}

	/* Initialize pointers to input and output file names */

	ifilep = argv[optind];
	ofilep = argv[optind + 1];

	/* check for invalid invocations */

	if (iflag && oflag && argc != 6)
		usage();
	if (iflag && ! oflag && argc != 5)
		usage();
	if (! iflag && oflag && argc != 4)
		usage();
	if (! iflag && ! oflag && argc != 3)
		usage();

	/* Construct a  full-path to the output file */

	if (localep) {
		size = L_locale + strlen(localep) +
			 sizeof(MESSAGES) + strlen(ofilep);
		if ((pathoutp = malloc(2 * (size + 1))) == NULL) {
			err_nomem();
			exit(1);
		}
		localedirp = pathoutp + size + 1;
		(void)strcpy(pathoutp, P_locale);
		(void)strcpy(&pathoutp[L_locale - 1], localep);
		(void)strcat(pathoutp, MESSAGES);
		(void)strcpy(localedirp, pathoutp);
		(void)strcat(pathoutp, ofilep);
	}

	/* Check for overwrite error conditions */

	if( !oflag) {
	    if( !access(iflag? pathoutp : ofilep, F_OK)) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_mkmsgs_omf_exists,
			"Message file '%s' already exists, no overwrite"),
		    iflag? pathoutp : ofilep);
		exit(1);
	    }
	}

	(void)umask(0022);
	
	/* Open input file */

	if ((fp_inp = fopen(ifilep, "r")) == NULL) {
		err_open(ifilep);
		exit(1);
	}

	/* Allocate buffer for input and work areas */

	if ((bufinp = malloc(2 * LINESZ)) == NULL) {
		err_nomem();
		exit(1);
	}
	bufworkp = bufinp + LINESZ;

	if (sigset(SIGINT, SIG_IGN) == SIG_DFL)
		(void)sigset(SIGINT, clean);

	/* Open work file */

	workp = tempnam(".", "xx");
	if ((fp_outp = fopen(workp, "a+")) == NULL) {
		err_open(workp);
		exit(1);
	}

	/* Search for C-escape sequences in input file and 
	 * replace them by the appropriate characters.
	 * The modified lines are copied to the work area
	 * and written to the work file */

	for(;;) {
		if (!fgets(bufinp, LINESZ, fp_inp)) {
			if (!feof(fp_inp)) {
				err_read(ifilep);
				exit(1);
			}
			break;
		}
		if(*(bufinp+strlen(bufinp)-1)  != '\n') {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_mkmsgs_iferr,
				"data base file %s: error on line %d"),
			    ifilep, num_msgs);
			exit(1);
		}
		*(bufinp + strlen(bufinp) -1) = (char)0; /* delete newline */
		num_msgs++;
		(void)strccpy(bufworkp, bufinp);
		nitems = strlen(bufworkp) + 1;
		if (fwrite(bufworkp, sizeof(*bufworkp), nitems, fp_outp) != nitems) {
			err_write(workp);
			exit(1);
		}
	}
	free(bufinp);
	(void)fclose(fp_outp);

	/* Open and stat the work file */

	if ((fp_outp = fopen(workp, "r")) == NULL) {
		err_open(workp);
		exit(1);
	}
	if ((stat(workp, &buf)) != 0) {
		err_stat(workp);
	}

	/* Find the size of the output message file 
	 * and copy the control information and the messages
	 * to the output file */

	size = sizeof(int) + num_msgs * sizeof(int) + buf.st_size;

	if ( (bufoutp = (int *)malloc((uint)size)) == NULL ) {
		err_nomem();
		exit(1);
	}
	bufinp = (char *)bufoutp;
	if ( (fread(bufinp + sizeof(int) + num_msgs * sizeof(int), 
	    sizeof(*bufinp), buf.st_size, fp_outp)) != buf.st_size ) {
		free(bufinp);
		err_read(workp);
	}
	(void) fclose(fp_outp);
	unlink(workp);
	free(workp);
	msgp = bufinp + sizeof(int) + num_msgs * sizeof(int);
	*bufoutp = num_msgs;
	*(bufoutp + 1) =
		(bufinp + sizeof(int) + num_msgs * sizeof(int)) - bufinp;

	for(i = 2; i <= num_msgs; i++) {
		*(bufoutp + i) = (msgp + strlen(msgp) + 1) - bufinp;
		msgp = msgp + strlen(msgp) + 1;
	}

	if (iflag) { 
		outfilep = pathoutp;
		if ( !mymkdir(localedirp))
			exit(1);
		/* Only need privilege for files under locale directory */
		/* Need P_MACWRITE to be able to write message file */
	} else
		outfilep = ofilep;


	if ((fp_outp = fopen(outfilep, "w")) == NULL) {
		err_open(outfilep);
		exit(1);
	}

	if (fwrite((char *)bufinp, sizeof(*bufinp), size, fp_outp) != size) {
		err_write(ofilep);
		exit(1);
	}

	(void)fclose(fp_outp);

	free(bufinp);
	if (localep)
		free(pathoutp);

	if (iflag) {
		if(chown(outfilep, uid, gid) == -1) {
			err_chown(outfilep);
			exit(1);
		}	
	}
	exit(0);
}

/*
 * Procedure:     mymkdir
 *
 * Restrictions:
 *               access(2):	none
 *               mkdir(2):	none
 *               chown(2):	none
 *               lvlfile(2):	none
 */

static int
mymkdir(localdir)
char	*localdir;
{
	char	*dirp;
	char	*s1 = localdir;
	char	*path;

	if ((path = malloc(strlen(localdir)+1)) == NULL)
		return(0);
	*path = '\0';
	while( (dirp = strtok(s1, "/")) != NULL ) {
		s1 = (char *)NULL;
		(void)strcat(path, "/");
		(void)strcat(path, dirp);
		/*
		 * This seems silly - requiring WRITE permission over
		 * the entire pathname! How about good'ol Unix
		 */
		/*if (access(path, X_OK | W_OK) == 0) */
		if (access(path, X_OK) == 0)
			continue;

		if ( mkdir(path, 0777) == -1 ||
		    chown(path, uid, gid) == -1 ) {
			err_chown(path);
			(void)free(path);
			return(0);
		}
	}
	free(path);
	return(1);
}

/*
 * Procedure:     clean
 *
 * Restrictions:
 *               unlink(2):	none
*/

static void
clean()
{
	(void)sigset(SIGINT, SIG_IGN);
	if (workp)
		(void)unlink(workp);
	exit(1);
}
