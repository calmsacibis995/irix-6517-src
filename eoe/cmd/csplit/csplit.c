/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)csplit:csplit.c	1.13.1.1"
/*
*	csplit - Context or line file splitter
*	Compile: cc -O -s -o csplit csplit.c
*/

#include <stdio.h>
#include <errno.h>
#define _MODERN_C_
#include <regexpr.h>
#include <signal.h>
#include <locale.h>
#include <pfmt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

#define LAST	0LL
#define ERR	-1
#define FALSE	0
#define TRUE	1
#define EXPMODE	2
#define LINMODE	3
#define	LINSIZ	256
#define MAXFLS	99
#define	UXDFM		0	/* message catalogue flag */
#define	UXSGICORE	1	/* message catalogue flag */

	/* Globals */

char *strrchr();
char linbuf[LINSIZ];		/* Input line buffer */
char *expbuf;
char tmpbuf[BUFSIZ];		/* Temporary buffer for stdin */
char file[256] = "xx";		/* File name buffer */
int inumber = 2;		/* decimal number length of suffix */
char anumber[256];		/* holding area for -n number in ascii */
char *targ;			/* Arg ptr for error messages */
char *sptr;
FILE *infile, *outfile;		/* I/O file streams */
int silent, keep, create;	/* Flags: -s(ilent), -k(eep), (create) */
int errflg;
extern int optind;
extern char *optarg;
off64_t offset;			/* Regular expression offset value */
off64_t curline;			/* Current line in input file */

/*
*	These defines are needed for regexp handling (see regexp(7))
*/
#define PERROR(x)	fatal(MM_ERROR,":5:%s: Illegal Regular Expression\n",targ,NULL,UXDFM);

static char *getline(int bumpcur);
static void fatal(int howbad,const char *string,char *arg1,char *arg2,int cat);
static void re_arg(char *string);
static void num_arg(register char *arg,int md);
static void line_arg(char *line);
static void to_line(off64_t ln);
static void closefile(void);
static off64_t findline(register char *expr,off64_t oset);
static FILE *getfile(void);

static	const	char	outofrangee[] = ":6:%s - Out of range\n";
static	char	cmd_label[] = "UX:csplit";

main(argc,argv)
int argc;
char **argv;
{
	int ch, mode;
	void sig();
	char *ptr;

	anumber[0] = '2';		/* default -n number is "2" */
	anumber[1] = anumber[255] = 0;	/* terminate with zero */

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxdfm");
	(void)setlabel(cmd_label);
	while((ch=getopt(argc,argv,"skf:n:")) != EOF) {
		switch(ch) {
			case 'f':
				if((int)strlen(optarg) > 
					(int)(sizeof(file) - 1))
				{
					fatal(MM_ERROR,":7:Prefix %s too long\n",ptr,NULL,UXDFM);
				}
				strcpy(file,optarg);
				if((ptr=strrchr(optarg,'/')) == NULL)
					ptr = optarg;
				else
					ptr++;

				if((int)strlen(ptr) > 12)
					fatal(MM_ERROR,":7:Prefix %s too long\n",ptr,NULL,UXDFM);
				break;
			case 's':
				silent++;
				break;
			case 'k':
				keep++;
				break;
			case 'n':
				errno = 0;
				strncpy(anumber, optarg, sizeof(anumber) - 1);
				anumber[strlen(anumber) + 1] = 0;
				inumber = atoi(anumber);
				if(errno) {
					setcat("uxsgicore");
					_sgi_nl_error(SGINL_NOSYSERR,
						cmd_label,
				gettxt(_SGI_DMMX_xpg_csplit_number_invalid,
				"cplit: -n is an out of range integer"));
					setcat("uxdfm");
					fatal(MM_ERROR,"dummy",optarg,
						NULL,UXSGICORE);
				}
				break;
			case '?':
				errflg++;
		}
	}
	argv = &argv[optind];
	argc -= optind;
	if(argc < 1 || errflg) {
		if (!errflg)
			pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
		setcat("uxsgicore");
		_sgi_nl_usage(SGINL_USAGE, cmd_label,
			gettxt(_SGI_DMMX_xpg_csplit_usage,
		    "csplit [-s] [-k] [-f prefix] [-n number] file args ..."));
		setcat("uxdfm");
		fatal(MM_ACTION, "dummy", NULL, NULL, UXSGICORE);
	}
	if ((strlen(file) + inumber) > NAME_MAX) {
		setcat("uxsgicore");
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			gettxt(_SGI_DMMX_xpg_csplit_filenametoolong,
				"cplit: created file name will be too long"));
		setcat("uxdfm");
		fatal(MM_ACTION, "dummy", NULL, NULL, UXSGICORE);
	}
	if(strcmp(*argv, "-") == 0) {
		int n;
		infile = tmpfile();

		while((n = fread(tmpbuf, 1, BUFSIZ, stdin)) != 0) {
			if(fwrite(tmpbuf, 1, n, infile) == 0)
				if(errno == ENOSPC) {
					pfmt(stderr, MM_ERROR,
						":9:No space left on device\n");
					exit(1);
				}else{
					pfmt(stderr, MM_ERROR,
						":10:Cannot write temporary file: %s\n",
						strerror (errno));
					exit(1);
				}

	/*	clear the buffer to get correct size when writing buffer	*/

			memset(tmpbuf,'\0',sizeof(tmpbuf));
		}
		rewind(infile);
	}
	else if((infile = fopen(*argv,"r")) == NULL)
		fatal(MM_ERROR,":3:Cannot open %s: %s\n",
			*argv, strerror (errno), UXDFM);
	++argv;
	curline = 1LL;
	signal(SIGINT,sig);

	/*
	*	The following for loop handles the different argument types.
	*	A switch is performed on the first character of the argument
	*	and each case calls the appropriate argument handling routine.
	*/

	for(; *argv; ++argv) {
		targ = *argv;
		switch(**argv) {
		case '/':
			mode = EXPMODE;
			create = TRUE;
			re_arg(*argv);
			break;
		case '%':
			mode = EXPMODE;
			create = FALSE;
			re_arg(*argv);
			break;
		case '{':
			num_arg(*argv,mode);
			mode = FALSE;
			break;
		default:
			mode = LINMODE;
			create = TRUE;
			line_arg(*argv);
			break;
		}
	}
	create = TRUE;
	to_line(LAST);
}

/*
*	Atol takes an ascii argument (str) and converts it to a long (plc)
*	It returns ERR if an illegal character.  The reason that atol
*	does not return an answer (long) is that any value for the long
*	is legal, and this version of atol detects error strings.
*/

static int
my_atol(register char *str,long *plc)
{
	register int f;
	*plc = 0;
	f = 0;
	for(;;str++) {
		switch(*str) {
		case ' ':
		case '\t':
			continue;
		case '-':
			f++;
		case '+':
			str++;
		}
		break;
	}
	for(; *str != NULL; str++)
		if(*str >= '0' && *str <= '9')
			*plc = *plc * 10 + *str - '0';
		else
			return(ERR);
	if(f)
		*plc = -(*plc);
	return(TRUE);	/* not error */
}

/*
*	Atol takes an ascii argument (str) and converts it to a long long (plc)
*	It returns ERR if an illegal character.  The reason that atol
*	does not return an answer (long long) is that any value for the long long
*	is legal, and this version of atol detects error strings.
*/

static int
my_atoll(register char*str,long long *plc)
{
	register int f;
	*plc = 0;
	f = 0;
	for(;;str++) {
		switch(*str) {
		case ' ':
		case '\t':
			continue;
		case '-':
			f++;
		case '+':
			str++;
		}
		break;
	}
	for(; *str != NULL; str++)
		if(*str >= '0' && *str <= '9')
			*plc = *plc * 10 + *str - '0';
		else
			return(ERR);
	if(f)
		*plc = -(*plc);
	return(TRUE);	/* not error */
}

/*
*	Closefile prints the byte count of the file created, (via fseek
*	and ftell), if the create flag is on and the silent flag is not on.
*	If the create flag is on closefile then closes the file (fclose).
*/

static void
closefile(void)
{
	if(!silent && create) {
		fseek64(outfile,0LL,2);
		fprintf(stdout,"%lld\n",ftell64(outfile));
	}
	if(create)
		fclose(outfile);
}

/*
*	Fatal handles error messages and cleanup.
*	Because "arg" can be the global file, and the cleanup processing
*	uses the global file, the error message is printed first.  If the
*	"keep" flag is not set, fatal unlinks all created files.  If the
*	"keep" flag is set, fatal closes the current file (if there is one).
*	Fatal exits with a value of 1.
*/

static void
fatal(int howbad, const char *string,char *arg1, char *arg2, int cat)
{
	register char *fls;
	register int num;
	int ret;
	char tbuf[256];

	tbuf[0] = '%';
	tbuf[1] = '.';
	tbuf[2] = '0';
	ret = sprintf(&tbuf[3], "%s", anumber);
	tbuf[ret+3] = 'd';
	tbuf[ret+4] = 0;

	/*
	 *  Don't print message if cat flag is UXSGICORE.  It's
	 *  assumed that the printing has taken place before the
	 *  call to fatal() if cat is UXSGICORE and that the setcat()
	 *  back to "uxdfm" has been done.
	 */
	if(cat == UXDFM)
		pfmt(stderr, howbad, string, arg1, arg2);
	if(!keep) {
		if(outfile) {
			fclose(outfile);
			for(fls=file; *fls != NULL; fls++);
			fls -= inumber;
			for(num=atoi(fls); num >= 0; num--) {
				sprintf(fls,tbuf,num);
				unlink(file);
			}
		}
	} else
		if(outfile)
			closefile();
	exit(1);
}

/*
*	Findline returns the line number referenced by the current argument.
*	Its arguments are a pointer to the compiled regular expression (expr),
*	and an offset (oset).  The variable lncnt is used to count the number
*	of lines searched.  First the current stream location is saved via
*	ftell(), and getline is called so that R.E. searching starts at the
*	line after the previously referenced line.  The while loop checks
*	that there are more lines (error if none), bumps the line count, and
*	checks for the R.E. on each line.  If the R.E. matches on one of the
*	lines the old stream location is restored, and the line number
*	referenced by the R.E. and the offset is returned.
*/

static off64_t
findline(register char *expr,off64_t oset)
{
	static int benhere;
	off64_t lncnt = 0;
	off64_t  saveloc;

	saveloc = ftell64(infile);
	if(curline != 1LL || benhere)		/* If first line, first time, */
		getline(FALSE);			/* then don't skip */
	else
		lncnt--;
	benhere = 1;
	while(getline(FALSE) != NULL) {
		lncnt++;
		if((sptr=strrchr(linbuf,'\n')) != NULL)
			*sptr = '\0';
		if(step(linbuf,expr)) {
			fseek64(infile,saveloc,0);
			return(curline+lncnt+oset);
		}
	}
	fseek64(infile,saveloc,0);
	return(curline+lncnt+oset+2);
}

/*
*	Flush uses fputs to put lines on the output file stream (outfile)
*	Since fputs does its own buffering, flush doesn't need to.
*	Flush does nothing if the create flag is not set.
*/

static void
flush(void)
{
	if(create)
		fputs(linbuf,outfile);
}

/*
*	Getfile does nothing if the create flag is not set.  If the
*	create flag is set, getfile positions the file pointer (fptr) at
*	the end of the file name prefix on the first call (fptr=0).
*	Next the file counter (ctr) is tested for MAXFLS, fatal if too
*	many file creations are attempted.  Then the file counter is
*	stored in the file name and incremented.  If the subsequent
*	fopen fails, the file name is copied to tfile for the error
*	message, the previous file name is restored for cleanup, and
*	fatal is called.  If the fopen succecedes, the stream (opfil)
*	is returned.
*/

static FILE *getfile(void)
{
	static char *fptr;
	static int ctr;
	FILE *opfil;
	int ret;
	char tfile[15];
	char tbuf[256];

	tbuf[0] = '%';
	tbuf[1] = '.';
	tbuf[2] = '0';
	ret = sprintf(&tbuf[3], "%s", anumber);
	tbuf[ret+3] = 'd';
	tbuf[ret+4] = 0;
	if(create) {
		if(fptr == 0)
			for(fptr = file; *fptr != NULL; fptr++);
		if(ctr > MAXFLS)
			fatal(MM_ERROR,":11:100 file limit reached at arg %s\n",targ, NULL, UXDFM);
		sprintf(fptr,tbuf,ctr++);
		if((opfil = fopen(file,"w")) == NULL) {
			strcpy(tfile,file);
			sprintf(fptr,tbuf,(ctr-2));
			fatal(MM_ERROR,":12:Cannot create %s: %s\n",tfile, strerror (errno), UXDFM);
		}
		return(opfil);
	}
	return(NULL);
}

/*
*	Getline gets a line via fgets from the input stream "infile".
*	The line is put into linbuf and may not be larger than LINSIZ.
*	If getline is called with a non-zero value, the current line
*	is bumped, otherwise it is not (for R.E. searching).
*/

static char *getline(int bumpcur)
{
	char *ret;
	if(bumpcur)
		curline++;
	ret=fgets(linbuf,LINSIZ,infile);
	return(ret);
}

/*
*	Line_arg handles line number arguments.
*	line_arg takes as its argument a pointer to a character string
*	(assumed to be a line number).  If that character string can be
*	converted to a number (long), to_line is called with that number,
*	otherwise error.
*/

static void
line_arg(char *line)
{
	off64_t to;

	if(my_atoll(line,&to) == ERR)
		fatal(MM_ERROR,":13:%s: Bad line number\n",line, NULL, UXDFM);
	to_line(to);
}

/*
*	Num_arg handles repeat arguments.
*	Num_arg copies the numeric argument to "rep" (error if number is
*	larger than 11 characters or } is left off).  Num_arg then converts
*	the number and checks for validity.  Next num_arg checks the mode
*	of the previous argument, and applys the argument the correct number
*	of times. If the mode is not set properly its an error.
*/

static void
num_arg(register char *arg,int md)
{
	long repeat;
	off64_t  toline;
	char rep[12];
	register char *ptr;

	ptr = rep;
	for(++arg; *arg != '}'; arg++) {
		if(ptr == &rep[11])
			fatal(MM_ERROR,":14:%s: Repeat count too large\n",targ, NULL, UXDFM);
		if(*arg == NULL)
			fatal(MM_ERROR,":15:%s: Missing '}'\n",targ, NULL, UXDFM);
		*ptr++ = *arg;
	}
	*ptr = NULL;
	if((my_atol(rep,&repeat) == ERR) || repeat < 0L)
		fatal(MM_ERROR,":16:Illegal repeat count: %s\n",targ, NULL, UXDFM);
	if(md == LINMODE) {
		toline = offset = curline;
		for(;repeat > 0L; repeat--) {
			toline += offset;
			to_line(toline);
		}
	} else	if(md == EXPMODE)
			for(;repeat > 0L; repeat--)
				to_line(findline(expbuf,offset));
		else
			fatal(MM_ERROR,":17:No operation for %s\n",targ, NULL, UXDFM);
}

/*
*	Re_arg handles regular expression arguments.
*	Re_arg takes a csplit regular expression argument.  It checks for
*	delimiter balance, computes any offset, and compiles the regular
*	expression.  Findline is called with the compiled expression and
*	offset, and returns the corresponding line number, which is used
*	as input to the to_line function.
*/

static void
re_arg(char *string)
{
	register char *ptr;
	register char ch;

	ch = *string;
	ptr = string;
	while(*(++ptr) != ch) {
		if(*ptr == '\\')
			++ptr;
		if(*ptr == NULL)
			fatal(MM_ERROR,":18:%s: Missing delimiter\n",targ, NULL, UXDFM);
	}

	/*	The line below was added because compile no longer supports	*
	 *	the fourth argument being passed.   The fourth argument used	*
	 *	to be '/' or '%'.						*/

	*ptr = NULL;
	if(my_atoll(++ptr,&offset) == ERR)
		fatal(MM_ERROR,":19:%s: Illegal offset\n",string, NULL, UXDFM);

	/*	The line below was added because INIT which did this for us*
	 *	was removed from compile in regexp.h			   */

	string++;
	expbuf = compile(string, (char *)0, (char *)0);
	if (regerrno)
		PERROR(regerrno);
	to_line(findline(expbuf,offset));
}

/*
*	Sig handles breaks.  When a break occurs the signal is reset,
*	and fatal is called to clean up and print the argument which
*	was being processed at the time the interrupt occured.
*/

void
sig(s)
int	s;
{
	signal(SIGINT,sig);
	fatal(MM_ERROR,":20:Interrupt - Program aborted at arg '%s'\n",targ, NULL, UXDFM);
}

/*
*	To_line creates split files.
*	To_line gets as its argument the line which the current argument
*	referenced.  To_line calls getfile for a new output stream, which
*	does nothing if create is False.  If to_line's argument is not LAST
*	it checks that the current line is not greater than its argument.
*	While the current line is less than the desired line to_line gets
*	lines and flushes (error if EOF is reached).
*	If to_line's argument is LAST, it checks for more lines, and gets
*	and flushes lines till the end of file.
*	Finally, to_line calls closefile to close the output stream.
*/

static void
to_line(off64_t ln)
{
	outfile = getfile();
	if(ln != LAST) {
		if(curline > ln)
			fatal(MM_ERROR,outofrangee,targ, NULL, UXDFM);
		while(curline < ln) {
			if(getline(TRUE) == NULL)
				fatal(MM_ERROR,outofrangee,targ, NULL, UXDFM);
			flush();
		}
	} else		/* last file */
		if(getline(TRUE) != NULL) {
			flush();
			while(TRUE) {
				if(getline(TRUE) == NULL)
					break;
				flush();
			}
		} else
			fatal(MM_ERROR,outofrangee,targ, NULL, UXDFM);
	closefile();
}
