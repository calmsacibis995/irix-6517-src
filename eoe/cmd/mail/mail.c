/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)mail:mail.c	1.83" */
/*
 	Mail to a remote machine will normally use the uux command.
 	If available, it may be better to send mail via 3bsend, nusend,
 	or usend, although delivery is not as reliable as uux.
 	Mail may be compiled to take advantage
 	of these other networks by adding:
    
		#define USE_NISEND  for 3bsend
	and
 		#define USE_NUSEND  for nusend
 	and
 		#define USE_USEND   for usend.
 
 	NOTE:  If any or all defines are specified, those networks
 	will be tried before uux.
 */

#include	<unistd.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<signal.h>
#include	<grp.h>
#include	<pwd.h>
#include	<time.h>
#include	<utmp.h>
#include	<sys/stat.h>
#include	<setjmp.h>
#include	<string.h>
#include	<sys/utsname.h>
#ifdef	sgi
#include	<sys/capability.h>
#include	<sys/wait.h>
#include	<ctype.h>
#define	stat stat64	/* turn all structs & calls into 64-bit form */
#include	<dirent.h>
#include	<limits.h>
#undef EX_OK	/*confict with unistd.h */
#include	<sysexits.h>
#define	SENDMAIL	"/usr/lib/sendmail"
int	isdelivermail = 0;
int	pwonly = 0;
int	rstchown;
#else
#include	<sys/dir.h>
#endif

#if 0
#define USE_NISEND 1			/* switch for 3bsend */
#define USE_NUSEND 1			/* switch for nusend */
#define USE_USEND  1			/* switch for usend  */
#endif

#define LOCKON		1	/* lock set */
#define LOCKOFF		0	/* lock released */
#define CHILD		0
#define SAME		0

#define CERROR		-1
#define CSUCCESS	0

#define TRUE	1
#define FALSE	0

#define E_FLGE	1	/* flge error */
#define E_FILE	2	/* file error */
#define E_SPACE	3	/* no space */
#define E_FRWD	4	/* cannot forward */
#define E_SYNTAX 5      /* syntax error */
#define E_FRWL	6	/* forwarding loop */
#define E_SNDR  7	/* invalid sender */
#define E_USER  8	/* invalid user */
#define E_FROM  9	/* too many From lines */
#define E_PERM  10 	/* bad permissions */
#define E_MBOX  11 	/* mbox problem */
#define E_TMP	12 	/* temporary file problem */
#define E_DEAD  13 	/* Cannot create dead.letter */
#define E_UNBND 14 	/* Unbounded forwarding */
#define E_LOCK  15 	/* cannot create lock file - XXXrs */
#define E_GROUP	16	/* no group id of 'mail' */
#define	E_MEM	17	/* malloc failure */
#define E_FORK	18	/* could not fork */
#define	E_PIPE	19	/* could not pipe */
#define	E_OWNR	20	/* invoker does not own mailfile */
#ifdef	sgi
#define	E_SMAIL	21	/* can't call sendmail */
#endif

char	*errlist[]={"",
	"Unknown system/user",
	"Problem with mailfile",
	"Space problem",
	"Unable to forward mail, check permissions and group",
	"Syntax error",
	"Forwarding loop",
	"Invalid sender",
	"Invalid recipient",
	"Too many From lines",
	"Invalid permissions",
	"Cannot open mbox",
	"Temporary file problem",
	"Cannot create dead.letter",
	"Unbounded forwarding",
	"Cannot create lock file",
	"No group id of 'mail'",
	"Problem allocating memory",
	"Could not fork",
	"Cannot pipe",
	"Must be owner to modify mailfile",
#ifdef	sgi
	"Can't start /usr/lib/sendmail",
#endif
};

/*
	copylet flags
*/
#define	REMOTE		1		/* remote mail, add rmtmsg */
#define ORDINARY	2
#define ZAP		3		/* zap header and trailing empty line */
#define FORWARD		4

#define	LSIZE		BUFSIZ		/* maximum size of a line */
#ifdef sgi
#define	MAXLET		3000		/* maximum number of letters */
#else
#define	MAXLET		300		/* maximum number of letters */
#endif
#define FROMLEVELS	20		/* maxium number of forwards */
#define MAXFILENAME	NAME_MAX	/* max length of a filename */
#define DEADPERM	0600		/* permissions of dead.letter */

#ifndef	MFMODE
#define	MFMODE		0660		/* create mode for `/var/mail' files */
#endif

#define A_OK		0		/* return value for access */
#define A_EXECUTE       1
#define A_EXIST		0		/* access check for existence */
#define A_WRITE		2		/* access check for write permission */
#define A_READ		4		/* access check for read permission */

struct utimbuf tims, *utimep;

extern	int errno;
extern	char *optarg;	/* for getopt */
extern	int optind;
extern	unsigned sleep(unsigned);
extern	char *sys_errlist[];

struct	let {
	long	adr;
	char	change;
} let[MAXLET];

struct	utsname utsn;
int	changed;	/* > 0 says mailfile has changed */
#ifdef sgi
char	curlock[PATH_MAX];
#else
char	curlock[50];
#endif
int	curlockfd = -1;	/* lock file descriptor */
char	dead[] = "/dead.letter";	/* name of dead.letter */
void	resetsig();
int	delflg = 1;
int	dflag = 0;	/* 1 says returning unsendable mail */
int	error = 0;	/* Local value for error */
int	file_size;
int	flge = 0;	/* 1 says 'e' option specified */
#ifndef sgi
int	flgF = 0;	/* 1 = Installing/Removing  Forwarding */
#endif /* !sgi */
int	flgf = 0;	/* 1 says 'f' option specified */
int	flgh = 0;	/* 1 says 'h' option specified */
int	flago = 0;	/* 1 says 'o' specified */
int	flgp = 0;	/* 1 says 'p' option specified */
int	flgs = 0;	/* 1 says 's' option specified */
int	flgw = 0;	/* 1 says 'w' option specified */
int	forward;	/* 1 says print in fifo order */
#define FORWMSG	" forwarded by %s\n"
char	from[] = "From ";	/* from-line sentinel */
#ifdef sgi
char	replyto[] = "Reply-To:";
char	from822[] = "From:";
#endif
int	fromlevel = 0;	/* Counts number of From lines */
#define FRWLMSG "%s: Forwarding loop detected in %s's mailfile on system %s \n\n"
char	frwrd[] = "Forward to ";	/* forwarding sentinel */
int	goerr = 0;	/* counts parsing errors */
struct	group *grpptr;	/* pointer to struct group */
char	*hmbox;		/* pointer to $HOME/mbox */
char	*hmdead;	/* pointer to $HOME/dead.letter */
char	*home;		/* pointer to $HOME */
long	iop;
int	interactive = 0;	/* 1 says user is interactive */
int	ismail = TRUE;		/* default to program=mail */
char	*lettmp;		/* pointer to tmp filename */
char	lfil[50];
char	line[LSIZE+1];	/* holds a line of a letter in many places
			   plus a possible extra null from getline() */
char	maildir[] = "/var/mail/";	/* directory for mail files */
char	*mailfile;	/* pointer to mailfile */
int	mailgrp;	/* numeric id of group 'mail' */
char	maillock[] = ".lock";	/* suffix for lock files */
char	mailsave[] = "/var/mail/:saved/";	/* dir for save files */
FILE	*malf;		/* File pointer for mailfile */
int	maxerr = 0;	/* largest value of error */
char	mbox[] = "/mbox";	/* name for mbox */
char	*my_name;	/* user's name who invoked this command */
int	my_uid, my_gid;	/* uid and gid of users mailfile */
int	nlet	= 0;	/* current number of letters in mailfile */
int	oarg = 0;	/* index into argv of -o */
int	onlet	= 0;	/* number of letters in mailfile at startup*/
int	optcnt = 0;	/* Number of options specified */
int	optsw;		/* for getopt() */
#if !defined(sgi)
struct	passwd *pwd;
#endif
char	program[NAME_MAX];	/* program name */
int	replying = 0;	/* 1 says we are replying to a letter */
char	resp[LSIZE];	/* holds user's response in interactive mode */
#ifdef sgi
char	savefile[PATH_MAX];
#else
char	savefile[256];	/* holds filename of save file */
#endif
char	tmpdir[] = "";	/* default directory for tmp files */
#define RMTMSG	" remote from %s\n"
#define RTRNMSG	"***** UNDELIVERABLE MAIL sent to %s, being returned by %s!%s *****\n"
void	savdead();
void	done(int);
#if defined(sgi)
void	(*saveint)(int);
#else
int	(*saveint)();
#endif
static	char	xsendto[1024];	/* avoid name conflict with BSD system call */
#if defined(sgi)
void	(*setsig(int i, void (*f) (int)))(int);
#else
int	(*setsig())();
#endif
#ifdef sgi
char	thissys[64];
#else
char	*thissys;	/* Holds name of the system we are on */
#endif
FILE	*tmpf;		/* file pointer for temporary files */
char	TO[] = "To: ";
char	uval[1024];
int	targ = 0;	/* index into argv of -t */
int	toflg = 0;	/* 1 says add To: line to letter */
#if defined(sgi)
void createmf(char *, uid_t);
void printmail(int, char **);
void copyback(void);
void copymt(FILE *, FILE *);
void sendmail(int, char **);
void cat(char *, char *, char *);
void goback(int);
void stamp(void);
void tmperr(void);
void mktmp(void);
void setletr(int, char);
void mkdead(void);
void errmsg(int, char *);
void cksaved(char *);
void unlock(void);
int  dowait(pid_t);
int  legal(char *);
int  parse(int, char **);
int  lock(char *);
int  copylet(int, FILE *, int);
int  gethead(int, int);
int  validmsg(int);
int  getnumbr(char *);
char *getarg(char *, char *);
int  mychown(char *, uid_t, gid_t);
int  substr(char *, char *);
struct passwd *isuser(char *);
int  sendrmt(int, char *);
int  wtmpf(char *, size_t);
int  getline(int, FILE *);
int  xsend(int, char *, int);
int  pipletr(int, char *, int);
int  e_to_sys(int);
int  systm(char *);
int  extractadr(char *, size_t, char *, char *);
#endif

jmp_buf	sjbuf;

#ifdef USE_NISEND
jmp_buf nisendfail;
int     nisendjmp = FALSE;
#endif
 
#ifdef USE_NUSEND
jmp_buf nusendfail;
int	nusendjmp = FALSE;
#endif

#ifdef USE_USEND
jmp_buf usendfail;
int	usendjmp = FALSE;
#endif

#ifdef sgi
mode_t	umsave;
#else
unsigned umsave;
#endif
char	*help[] = {
	"#\t\tdisplay message number #\n",
	"-\t\tprint previous\n",
	"+\t\tnext (no delete)\n",
	"! cmd\t\texecute cmd\n",
	"a\t\tposition at and read newly arrived mail\n",
	"dq\t\tdelete current message and exit\n",
	"d [#]\t\tdelete message # (default current message)\n",
	"h a\t\tdisplay all headers\n",
	"h d\t\tdisplay headers of letters scheduled for deletion\n",
	"h [#]\t\tdisplay headers around # (default current message)\n",
	"m user  \tmail (and delete) current message to user\n",
	"n\t\tnext (no delete)\n",
	"p\t\tprint\n",
	"q\t\tquit\n",
	"r [args]\treply to (and delete) current letter via mail [args]\n",
	"s [file]\tsave (and delete) current message (default mbox)\n",
	"u [#]\t\tundelete message # (default current message)\n",
	"w [file]\tsave (and delete) current message without header\n",
	"x\t\texit without changing mail\n",
	"y [file]\tsave (and delete) current message (default mbox)\n",
	0
};

/*
	Generic open routine.
	Exits on error with passed error value.
	Returns file pointer on success.

	Note: This should be used only for critical files
	as it will terminate mail(1) on failure.
*/
#if defined(sgi)
FILE *
doopen(char *file, char *type, int errnum)
#else
FILE *
doopen(file, type, errnum)
char	*type, *file;
#endif
{
FILE *fptr;

	if ((fptr = fopen(file, type)) == NULL) {
		fprintf(stderr, "%s: Can't open %s  type:%s\n",program,file,type);
		error = errnum;
		done(0);
	}
	return(fptr);
}
/*
	mail [ + ] [ -ionrpqethw ]  [ -f file ] [ -F user(s) ]
	mail [ -t ] persons 
	rmail [ -ontw ] persons 
*/
main(argc, argv)
char	**argv;
{
	register int i;
	char *cptr;
	struct passwd *lnm;

	if(ismail == TRUE) {
		strcpy(program, "mail");
	}

#ifdef sgi
	if (cap_envl(CAP_ENV_SETUID|CAP_ENV_RECALC, CAP_SETUID,
		     CAP_CHOWN, 0) == -1) {
		fprintf(stderr, "insufficient privilege\n");
		exit(1);
	}
	rstchown = (pathconf(".", _PC_CHOWN_RESTRICTED) > 0);
#endif /* sgi */
	/*	Check that argv[0] is non NULL for security */
	if(argv[0] == NULL) {
		errmsg(E_SYNTAX, "Program name cannot be NULL.\n");
		done(0);
	}
	/*
		Strip off path name of this command for use in messages
	*/
	if ((cptr = strrchr(argv[0],'/')) != NULL) cptr++;
	else cptr = argv[0];

	strncpy(program,cptr, sizeof(program));
	program[sizeof(program) -1]='\0';

	/*
		Get group id for mail, exit if none exists
	*/
	if ((grpptr = getgrnam("mail")) == NULL) {
		errmsg(E_GROUP,"");
		done(0);
	}
	else mailgrp = grpptr->gr_gid;

	/*
		What command (rmail or mail)?
	*/
	if (strcmp(program, "rmail") == SAME) ismail = FALSE;

	/*
		Parse the command line and adjust argc and argv 
		to compensate for any options
	*/
	i = parse(argc,argv);
	if (i > 1 && i < argc) {
		argv += i - 1;
		argc -= i - 1;
	}

	if (!ismail && (goerr > 0 || !i)) {
		if (goerr > 0) errmsg(E_SYNTAX,"usage: rmail [-otw] person(s)");
		if (!i) errmsg(E_SYNTAX,"At least one user must be specified");
		done(0);
	}

	umsave = umask(7);
	setbuf(stdout, malloc(BUFSIZ));
#ifdef sgi
	gethostname(thissys, sizeof thissys);
#else
	uname(&utsn);
	thissys = utsn.nodename;
#endif

	/* 
		Use environment variables, logname is used for from
	*/
	if (((home = getenv("HOME")) == NULL) || (strlen(home) == 0)) home = ".";
	my_name=getenv("LOGNAME");
	if((my_name == NULL) || (strlen(my_name) == 0)) {
		my_name=getlogin();
	}
	else {
#ifdef sgi
		lnm=isuser(my_name);
#else
		lnm=getpwnam(my_name);
#endif
		if(lnm == NULL)
			my_name=getlogin();
	}
	if ((my_name == NULL) || (strlen(my_name) == 0)) {
		register struct passwd *pwp;
		static char none_here[40];
		pwp = getpwuid(geteuid());
		if (!pwp) {			/* allow missing passwd entry */
			(void)sprintf(none_here, "(euid=%d)", geteuid());
			my_name = none_here;
		} else {
			my_name = pwp->pw_name;
		}
	}

	/*
		Catch signals for cleanup
	*/
	if (setjmp(sjbuf)) done(0);
	for (i=SIGINT; i<SIGCLD; i++) setsig(i, resetsig);
	setsig(SIGHUP, done);

	cksaved(my_name);

	/*
		Rmail always invoked to send mail
	*/
	if (ismail && (argc==1 || argv[1][0] == '+' || argv[1][0] == '-')) {
		printmail(argc, argv);
#ifndef sgi
		if (flgF) doFopt(argc);
#endif /* !sgi */
	}
	else sendmail(argc, argv);
	done(flge? 1: 0); /*NOTREACHED*/
}

/*
	Signal reset
	signals that are not being ignored will be 
	caught by function f
		i	-> signal number
		f	-> signal routine
	return
		rc	-> former signal
 */
#if defined(sgi)
void (*setsig(int i, void (*f) (int)))(int)
#else
int (*setsig(i, f))()
int      i;
int      (*f)();
#endif
{
#if defined(sgi)
	register void (*rc)(int);

	if ((rc = signal(i, SIG_IGN)) != SIG_IGN) signal(i, f);
#else
	register int (*rc)();

	if ((rc = signal(i, SIG_IGN)) != (int (*)()) SIG_IGN) signal(i, f);
#endif
	return(rc);
}

/*
	Print mail entries
		argc	-> argument count
		argv	-> arguments
*/
#if defined(sgi)
void
printmail(int argc, char **argv)
#else
printmail(argc, argv)
char	**argv;
#endif
{
	int	flg, i, j, k, print, aret, stret, rc;
#if defined(sgi)
	char	*p;
#else
	char	*p, *getarg();
#endif
	struct	stat stbuf;
	struct	stat *stbufp;

	stbufp = &stbuf;

	if (optcnt == 1 && toflg == 1 && argc > 2) {
		argv++;
		sendmail(--argc, argv);
		done(0);
	}

	/*
		create working directory mbox name
	*/
	if ((hmbox = malloc(strlen(home) + strlen(mbox) + 1)) == NULL) {
		errmsg(E_MBOX,"");
		return;
	}
	cat(hmbox, home, mbox);

	/*
		If we are not using an alternate mailfile, then get
		the $MAIL value and build the filename for the mailfile
	*/
	if (!flgf) {
		if (((mailfile = getenv("MAIL")) == NULL) || (strlen(mailfile) == 0)) {
			if ((mailfile = malloc(strlen(maildir) + strlen(my_name) + 1)) == NULL) {
				errmsg(E_MEM,"");
				return;
			}
			cat(mailfile, maildir, my_name);
		}
	}

	/*
		Get ACCESS and MODIFICATION times of mailfile BEFORE we
		use it. This allows us to put them back when we are 
		done. If we didn't, the shell would think NEW mail had 
		arrived since the file times would have changed.
	*/
	stret = CERROR;
	if (access(mailfile, A_EXIST) == A_OK) {
		if ((stret = stat(mailfile, stbufp)) != A_OK) {
			errmsg(E_FILE,"Cannot stat mailfile");
			return;
		}
		my_gid = stbufp->st_gid;
		my_uid = stbufp->st_uid;
		utimep = &tims;
		utimep->actime = stbufp->st_atime;
		utimep->modtime = stbufp->st_mtime;
		file_size = stbufp->st_size;
	}

	/*
		Check accessibility of mail file, and open it
	*/
	if ((aret=access(mailfile, A_READ)) == A_OK) malf = fopen(mailfile, "r");
	/*
		stat succeeded, but we cannot access the mailfile
	*/
	if (stret == CSUCCESS && aret == CERROR) {
		errmsg(E_PERM,"");
		return;
	}else 
	/*
		using an alternate mailfile, but we failed on access
	*/
	if (flgf && (aret == CERROR || (malf == NULL))) {
		errmsg(E_FILE, "Cannot open mailfile");
		return;
	}else 
	/*
		we failed to access OR the file is empty
	*/
	if (aret == CERROR || (malf == NULL) || (stbuf.st_size == 0)) {
		if (!flge) printf("No mail.\n");
		error = E_FLGE;
		return;
	}

	/*
		Secure the mailfile to guarantee integrity
	*/
#ifdef sgi
	if (!flge)
#endif
	lock(mailfile);

#ifndef sgi
	/*
		See if mail is to be forwarded to another system
	*/
	if (areforwarding(mailfile)) {
		unlock();
		if (flge) {
			error = E_FLGE;
			return;
		}
		/*
			We require a group id of mail on the mailfile,
			as well as a minimum of 0660 on the mailfile
		*/
		if (!((stbuf.st_gid == mailgrp) && ((stbuf.st_mode & 0660) == MFMODE))) {
			errmsg(E_FRWD,"");
			return;
		}
		printf("Your mail is being forwarded to %s", xsendto);
		return;
	}
#endif /* !sgi */

	if (flge) {
		unlock();
		return;
	}
	/*
		copy mail to temp file and mark each letter in the 
		let array --- mailfile is still locked !!!
	*/
	mktmp();
	copymt(malf, tmpf);
	onlet = nlet;
	fclose(malf);
	fclose(tmpf);
	unlock();	/* All done, OK to unlock now */
	tmpf = doopen(lettmp,"r",E_TMP);
	changed = 0;
	print = 1;
	for (i = 0; i < nlet; ) {
		/*
			reverse order ?
		*/
		j = forward ? i : nlet - i - 1;

		if (setjmp(sjbuf) == 0 && print != 0) {
				if (i != 0 || !flgh) copylet(j, stdout, ORDINARY);
				else {
					gethead(j,0);
					i--;
					j = forward ? i : nlet - i - 1;
				}
				flgh = 0;	/* Only first time */
		}

		/*
			print only
		*/
		if (flgp) {
			i++;
			continue;
		}
		/*
			Interactive
		*/
		interactive = 1;
		setjmp(sjbuf);
		stat(mailfile,stbufp);
		if (stbufp->st_size != file_size) {
			/*
				New mail has arrived, load it
			*/
			k = nlet;
			lock(mailfile);
			malf = doopen(mailfile,"r",E_FILE);
			fclose(tmpf);
			tmpf = doopen(lettmp,"a",E_TMP);
			fseek(malf,let[nlet].adr,0);
			copymt(malf,tmpf);
			file_size = stbufp->st_size;
			fclose(malf);
			fclose(tmpf);
			unlock();
			tmpf = doopen(lettmp,"r",E_TMP);
			printf("New mail loaded into letter(s) %d",++k);
			for (k++; k <= nlet;) printf(", %d",k++);
			printf("\n");
			i++;
		}
		printf("? ");
		fflush(stdout);
		fflush(stderr);
		if (fgets(resp, sizeof(resp), stdin) == NULL) break;
		print = 1;
		if ((rc = atoi(resp)) != 0) {
			if (!validmsg(rc)) print = 0;
			else i = forward ? rc - 1 : nlet - rc;
		}
		else switch (resp[0]) {
			default:
				printf("usage\n");
			/*
				help
			*/
			case '?':
				print = 0;
				for (rc = 0; help[rc]; rc++) printf("%s", help[rc]);
				break;
			/*
				headers
			*/
			case 'h':
				print = 0;
				if ( resp[2] != 'd' && resp[2] != 'a' && (rc = getnumbr(resp+1)) > 0) {
					j = rc - 1;
					i = forward ? rc - 1 : nlet - rc- 1;
				}
				if (rc == -1 && resp[2] != 'a' && resp[2] != 'd') break;
				if (resp[2] == 'a') rc = 1;
				else if (resp[2] == 'd') rc = 2;
					else rc = 0;

				if (!validmsg(j)) break;
				gethead(j,rc);
				break;
			/*
				skip entry
			*/
			case '+':
	                case 'n':
			case '\n':
				i++;
			case 'p':
				break;
			case 'x':
				changed = 0;
			case 'q':
				goto donep;
			/*
				Previous entry
			*/
			case '^':
			case '-':
				if (--i < 0) i = 0;
				break;
			/*
				Save in file without header
			*/
	                case 'y':
			case 'w':
			/*
				Save mail with header
			*/
			case 's':
				print = 0;
				if (!validmsg(i)) break;
				if (resp[1] == '\n' || resp[1] == '\0')
					cat(resp+1, hmbox, "");
				else if (resp[1] != ' ') {
					printf("Invalid command\n");
					break;
				}
				umask(umsave);
				flg = 0;
				p = resp + 1;
				if (getarg(lfil, p) == NULL)
					cat(resp + 1, hmbox, "");
				for (p = resp + 1;
				     (p = getarg(lfil, p)) != NULL; ) {
					if ((aret = legal(lfil)))
						malf = fopen(lfil, "a");
					if ((malf == NULL) || (aret == 0)) {
						fprintf(stderr,
						    "%s: Cannot append to %s\n",
							program,lfil);
						flg++;
					}
					else if (aret == 2) {
#ifdef sgi
						mychown(lfil, geteuid(),
							getgid());
#else
						chown(lfil, geteuid(),
						      getgid());
#endif /* sgi */
					}
					if (!flg && copylet(j, malf,
					    resp[0] == 'w' ? ZAP: ORDINARY)
					    == FALSE) {
						fprintf(stderr,
						    "%s: Cannot save mail\n",
							program);
						flg++;
					}
					fclose(malf);
				}
				umask(7);
				if (!flg) {
					setletr(j,resp[0]);
					print = 1;
					i++;
				}
				break;
			/*
				Reply to a letter
			*/
			case 'r':
				print = 0;
				if (!validmsg(i)) break;
				replying = 1;
				for (k=1; resp[k] == ' ' || resp[k] == '\t'; ++k);
				resp[strlen(resp)-1]='\0';
				strcpy(xsendto,resp+k);
				goback(j);
				replying = 0;
				setletr(j,resp[0]);
				break;
			/*
				Undelete
			*/
			case 'u':
				print = 0;
				if ((k = getnumbr(resp+1)) <= 0) k=j;
				else k--;
				if (!validmsg(k)) break;
				setletr(k,' ');
				break;
			/*
				Mail letter to someone else
			*/
			case 'm':
				print = 0;
				if (!validmsg(i)) break;
				flg = 0;
				k = 0;
				if (substr(resp," -") != -1 ||
					substr(resp,"\t-") != -1) {
					printf("Only users may be specified\n");
					break;
				}
				for (p = resp + 1; (p = getarg(lfil, p)) != NULL; ) {
					if (lfil[0] == '$' && !(getenv(&lfil[1]))) {
						fprintf(stderr,"%s: %s has no value or is not exported.\n",program,lfil);
						flg++;
					}
					else if (sendrmt(j, lfil) == FALSE) flg++;
					if (lfil[0] != '\0') k++;
				}
				if (k && !flg) {
					setletr(j,'m');
					print = 1;
					i++;
				}
				if (!k) printf("Invalid command\n");
				break;
			/*
				Read new letters
			*/
			case 'a':
				if (onlet == nlet) {
					printf("No new mail\n");
					print = 0;
					break;
				}
				i = 0;
				print = 1;
				break;
			/*
				Escape to shell
			*/
			case '!':
				systm(resp + 1);
				printf("!\n");
				print = 0;
				break;
			/*
				Delete an entry
			*/
			case 'd':
				print = 0;
				k = 0;
				if (strncmp("dq",resp,2) != SAME &&
					strncmp("dp",resp,2) != SAME) 
					if ((k = getnumbr(resp+1)) == -1) break;
				if (k == 0) {
					k = j;
					if (!validmsg(i)) break;
					print = 1;
					i++;
				}
				else k--;

				setletr(k,'d');
				if (resp[1] == 'p') print = 1;
				if (resp[1] == 'q') goto donep;
				break;
		}
	}
	/*
		Copy updated mailfile back
	*/
   donep:
	if (changed) {
		copyback();
		stamp();
	}
}

/*
	copy temp or whatever back to /var/mail
*/
#if defined(sgi)
void
copyback(void)
#else
copyback()
#endif
{
#if defined(sgi)
	register int i, n, c;
	mode_t mailmode, omask;
	int new = 0;
#else
	register i, n, c;
	int mailmode, new = 0, omask;
#endif
	struct stat stbuf;

	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	lock(mailfile);
	stat(mailfile, &stbuf);
	mailmode = stbuf.st_mode;
	/*
		Has new mail arrived?
	*/
	if (stbuf.st_size != let[nlet].adr) {
		malf = doopen(mailfile, "r",E_FILE);
		fseek(malf, let[nlet].adr, 0);
		fclose(tmpf);
		tmpf = doopen(lettmp,"a",E_TMP);
		/*
			Append new mail assume only one new letter
		*/
		while ((c = fgetc(malf)) != EOF)
			if (fputc(c, tmpf) == EOF) {
				fclose(malf);
				tmperr();
				done(0);
			}
		fclose(malf);
		fclose(tmpf);
		tmpf = doopen(lettmp,"r",E_TMP);
		if (nlet == (MAXLET-2)) {
			errmsg(E_SPACE,"");
			done(0);
		}
		let[++nlet].adr = (long) stbuf.st_size;
		new = 1;
	}
	/*
		Copy mail back to mail file
	*/
	omask = umask(0117);
	/*
		The invoker must own the mailfile being copied to
	*/
	if (stbuf.st_uid != geteuid() && stbuf.st_uid != getuid()) {
		errmsg(E_OWNR,"");
		done(0);
	}


	/* 
		If user specified the '-f' option we dont do 
		the routines to handle :saved files.
		As we would (incorrectly) restore to the user's
		mailfile upon next execution!
	*/
	if (flgf) strcpy(savefile,mailfile);
	else cat(savefile,mailsave,my_name);

	if ((malf = fopen(savefile, "w")) == NULL) {
		if (!flgf) errmsg(E_FILE,"Cannot open savefile");
		else errmsg(E_FILE,"Cannot re-write the alternate file");
		done(0);
	}
#ifdef sgi
	if (mychown(savefile,my_uid,my_gid) == -1) {
#else
	if (chown(savefile,my_uid,my_gid) == -1) {
#endif /* sgi */
		errmsg(E_FILE,"Cannot chown savefile");
		done(0);
	}
	umask(omask);
	n = 0;
	for (i = 0; i < nlet; i++) {
		/*
			Note: any action other than an undelete, or a 
			plain read causes the letter acted upon to be 
			deleted
		*/
		if (let[i].change == ' ') {
			if (copylet(i, malf, ORDINARY) == FALSE) {
				errmsg(E_FILE,"Cannot copy mail to savefile");
				fprintf(stderr,
				    "%s: A copy of your mailfile is in '%s'\n",
					program, lettmp);
				done(1);	/* keep temp file */
			}	
			n++;
		}
	}
	fclose(malf);
	if (!flgf) {
		if (unlink(mailfile) != 0) {
			errmsg(E_FILE,"Cannot unlink mailfile");
			done(0);
		}
		if (link(savefile,mailfile) != 0) {
			errmsg(E_FILE,"Cannot link savefile to mailfile");
			done(0);
		}
		chmod(mailfile,mailmode);
		if (unlink(savefile) != 0) {
			errmsg(E_FILE,"Cannot unlink save file");
			done(0);
		}
	}
	/*
		Empty mailbox?
	*/
	if ((n == 0) && (((stbuf.st_mode|MFMODE)&0777) == MFMODE)) {
		unlink(mailfile);
	}
	if (new && !flgf) printf("New mail arrived\n");
	unlock();
}

/*
	copy mail (f1) to temp (f2)
*/
#if defined(sgi)
void
copymt(FILE *f1, FILE *f2)
#else
copymt(f1, f2)	
register FILE *f1, *f2;
#endif
{
	long nextadr;
	int n, newline = 1;
	int mesg = 0;

	if (!let[1].adr) {
		nlet = nextadr = 0;
		let[0].adr = 0;
	}
	else nextadr = let[nlet].adr;

	while ((n=getline(LSIZE,f1)) > 0) {
		/*
			bug nlet should be checked
		*/
		if(newline && line[0] == from[0])
			if (strncmp(line,from,5) == SAME) {
				let[nlet].change = ' ';
				if(nlet >= (MAXLET-2)) {
					if (!mesg) {
						fprintf(stderr,"%s: Too many letters, overflowing letters concatenated\n\n",program);
						mesg++;
					}
				}
				else let[nlet++].adr = nextadr;
			}
		nextadr += n;
		if (fwrite(line,1,n,f2) != n) {
			fclose(f1);
			fclose(f2);
			errmsg(E_FILE,"Write error in copymt()");
			done(0);
		}
		if (nlet == 0) {
			fclose(f1);
			fclose(f2);
			errmsg(E_FILE,"mailfile does not begin with a 'From' line");
			done(0);
		}
		if (line[n-1] == '\n') newline = 1;
		else newline = 0;
	}

	/*
		last plus 1
	*/
	let[nlet].adr = nextadr;
	let[nlet].change = ' ';
}

#ifndef sgi
/*
	check to see if mail is being forwarded
		s	-> mail file

	xsendto is used as work area, and if forwarding then
	xsendto is set to the list of users

	returns
		TRUE	-> forwarding
		FALSE	-> local
*/
areforwarding(s)
char *s;
{
	FILE *fd;
	if ((fd = fopen(s, "r")) == NULL) return(FALSE);
	fread(xsendto, (unsigned)(sizeof(frwrd) - 1), 1, fd);
	if (strncmp(xsendto, frwrd, sizeof(frwrd) - 1) == SAME) {
		fgets(xsendto, sizeof(xsendto), fd);
		fclose(fd);
		return(TRUE);
	}
	xsendto[0] = '\0';	/* lets be nice */
	fclose(fd);
	return(FALSE);
}
#endif /* !sgi */

/*
	copy letter 
		ln	-> index into: letter table
		f	-> file descrptor to copy file to
		type	-> copy type
*/
#if defined(sgi)
int
copylet(int ln, FILE *f, int type)
#else
copylet(ln, f, type) 
register FILE *f;
#endif
{
	register char *s;
	char	buf[LSIZE], lastc, ch;
	int	n, j;
	long	i, k;
#if defined(sgi)
	unsigned long	num;
#else
	int	num;
#endif

	fseek(tmpf, let[ln].adr, 0);
	k = let[ln+1].adr - let[ln].adr;
	for(i = 0; i < k;){
		s = buf;
		num = ((k-i) > sizeof(buf))?sizeof(buf):(k-i);

		if ((n = fread(buf,1,num,tmpf)) <= 0) 
			return(FALSE);

		lastc = buf[n-1];
		if (i == 0) {
			for (j = 0; j < n; j++,s++) 
				if (*s == '\n') break;

			if (type != ZAP) 
				if (fwrite(buf,1,j,f) != j) 
					return(FALSE);
			i += j+1;
			n -= j+1;
			ch = *s++;
			switch(type) {
				case REMOTE:
					fprintf(f, RMTMSG, thissys);
					break;

				case ORDINARY:
					fprintf(f, "%c", ch);
					break;

				case FORWARD:
					fprintf(f, FORWMSG, my_name);
					break;
			}
			if (error > 0 && dflag == 1) {
				fprintf(f, RTRNMSG, uval, thissys, my_name);
				if (error == E_FRWL) fprintf(f, FRWLMSG, program, uval, thissys);
				else fprintf(f, "%s: Error # %d '%s' encountered on system %s\n\n", program, error, errlist[error], thissys);
			}
			fflush(f);
		}
		if (fwrite(s,1,n,f) != n) return(FALSE);
		i += n;
	}

	if (type != ZAP && lastc != '\n') 
		if (fwrite("\n",1,1,f) != 1) return(FALSE);

#ifdef sgi
	/* we want to ensure that the buffered data is written to disk NOW */
	fsync(fileno(f));
#endif
	return(TRUE);
}

/*
	 Send mail - High level sending routine
		argc	-> argument count
		argv	-> argument list
 */
#if defined(sgi)
void
sendmail(int argc, char **argv)
#else
sendmail(argc, argv)
char **argv;
#endif
{
	int	aret;
	char	**args;
	int	fromflg = 0,i;
#if !defined(sgi)
	struct	tm *bp;
	char	*tp, *zp;
#endif
	int	n,newline = 1,lnum = 0;
	char	buf[128],last1c,last2c;
#if !defined(sgi)
	int	pid;
#endif

	for (i = 1; i < argc; ++i) {
	        if (argv[i][0] == '-') {
		        if (argv[i][1] == '\0') {
				errmsg(E_SYNTAX,"Hyphens MAY NOT be followed by spaces");
				done(0);
			}
		        if (i > 1) {
				errmsg(E_SYNTAX,"Options MUST PRECEDE persons");
				done(0);
			}
	        }
		/*
			Ensure no NULL names in list
		*/
	        if (argv[i][0] == '\0' || argv[i][strlen(argv[i])-1] == '!') {
			errmsg(E_SYNTAX,"Null names are not allowed");
	  	       	done(0);
		}
	}
#ifdef	sgi
	if (!isdelivermail) {
		/*
	 	 * Call sendmail to send the mail
	 	 */
		argv[0] = "-sendmail";
		/*
		 * The berkeley version of /bin/mail sends a "-s"
		 * to sendmail, ASSUMING that /bin/rmail has
		 * already stripped out the leading from line
		 * provided by the uucp sender.  Unfortunately,
		 * if we are running HERE, then we are /bin/rmail,
		 * and could not have done the from stripping.
		 * OOPS.  So, for system V, we just call sendmail
		 * with our arguments, and leave it at that.
		 */
/*		setsig(SIGINT, SIG_IGN); */
		execv(SENDMAIL, argv);
		errmsg(E_SMAIL,"");

		/* fall through and send it the old fashioned way */
	}
#endif
	mktmp();
	/*
		Format time
	*/
	time(&iop);
#ifndef sgi
	bp = localtime(&iop);
	tp = asctime(bp);
	zp = tzname[bp->tm_isdst];
	sprintf(buf, "%s%s %.16s %.3s %.5s", from, my_name, tp, zp, tp+20);
	if (!wtmpf(buf, strlen(buf))) done(0);
#endif
	/*
		Copy to list in mail entry?
	*/
	if (toflg == 1 && argc > 1) {
		aret = argc;
		args = argv;
		sprintf(buf,"%s ",TO);
		if (!wtmpf(buf, strlen(buf))) done(0);
		while (--aret > 0) {
			sprintf(buf,"%s ",*++args);
			if (!wtmpf(buf, strlen(buf))) done(0);
		}
		if (!wtmpf("\n", 1)) done(0);
	}
	iop = ftell(tmpf);
	flgf = 1;
	/*
		Read mail message, allowing for lines of infinite 
		length. This is tricky, have to watch for newlines.
	*/
	saveint = setsig(SIGINT, savdead);
	fromlevel = 0;
	last1c = ' ';	/* anything other than newline */
	while ((n=getline(LSIZE,stdin)) > 0) {
		/*
			Are we returning mail from a failure?
			If so, we won't return on failure
		*/
		if (newline && n > 32) 
			if ((i = strncmp("***** UNDELIVERABLE MAIL sent to",line,32)) == SAME) dflag = 9; /* 9 says do not return on failure */

#ifdef sgi
		if (!isdelivermail
		    && newline && line[0] == '.' && line[1] == '\n') break;
#else
		if (newline && line[0] == '.' && line[1] == '\n') break;
#endif
		last2c = last1c;
		last1c = line[n-1];

		if (last1c == '\n') {
			newline = 1;
			lnum++;
		}
		else newline = 0;

		if (n > 1) last2c = line[n-2];

		/*
			If we are in headers, and we have a "fresh" line
			beginning with "From"; prepend with a ">",
			so it is no longer interpreted as the last 
			system fowarding the mail.
		*/
#ifndef sgi
		if (newline && line[0] == from[0]) 
			if (strncmp(line,from,5) == SAME)
				if (!wtmpf(">", 1)) done(0);
#endif

		/*
			Find out how many "from" lines
		*/
		if (newline && fromflg == 0 && (strncmp(line, from, 5) == SAME || strncmp(line, ">From ", 6) == SAME)) fromlevel++;
		else fromflg = 1;

		/*
			If there are no From/>From lines, then ensure
			the presence of at least one new-line to mark
			the end-of-headers. The only exception to this
			is if the line begins with Subject: which mailx
			recognizes as the subject line. We do this only 
			if the -s option is not specified. mailx(1) 
			MUST call mail(1) with the -s option!
		*/
		if (lnum == 1) {
			if (!flgs && fromflg && line[0] != '\n' && strncmp(line,"Subject:",8)) 
					if (!wtmpf("\n", 1)) done(0);
		}

		if (!wtmpf(line, n)) done(0);
		flgf = 0;
	}
	/*
		A little tricky:

			If the last two bytes of the message are
			newlines, do nothing.

			If the last character of the message is a 
			newline, add one more.

			If the last character of the message is not
			a newline, add two more.
	*/
	if (last1c != last2c || last2c != '\n') {

		if (last1c != '\n') i = 2;
		else i = 1;

		if (!wtmpf("\n\n", i)) done(0);
	}
	/*
		In order to use some of the subroutines that are used to
		read mail, the let array must be set up
	*/
	nlet = 1;
	let[0].adr = 0;
	let[1].adr = ftell(tmpf);
	if (fclose(tmpf) == EOF) {
		tmperr();
		return;
	}
	if (flgf) return;
	tmpf = doopen(lettmp,"r",E_TMP);
	/*
		Send a copy of the letter to the specified users
	*/
	if (!interactive && flgw) {
		if ((i = fork()) == 0) {
			setpgrp();
			while (--argc > 0 && dflag != 2) {
				xsend(0, *++argv, 0);
			}
			fclose(tmpf);
			done(0);
		}
		if (i == CERROR) errmsg(E_FORK,"");
		done(1);	/* Keep tmp file! */
	}
	else while (--argc > 0 && dflag != 2) {
		xsend(0, *++argv, 0);
	}
	if (maxerr && dflag == 2) mkdead();
	fclose(tmpf);
}

void
savdead()
{
	setsig(SIGINT, saveint);
	dflag = 2;	/* do not send back letter on interrupt */
	if (!error) error = 1;
	maxerr = error;
}

/*
	send mail to remote system taking fowarding into account
		n	-> index into mail table
		name	-> mail destination
	returns
		TRUE	-> sent mail
		FALSE	-> can't send mail
 */
#if defined(sgi)
int
sendrmt(int n, char *name)
#else
sendrmt(n, name)
register char *name;
#endif
{
# define NSCCONS	"/usr/nsc/cons/"
	register char *p;
#if defined(sgi)
	register int local;
#else
	register local;
#endif
	char rsys[64], cmd[200];
#if !defined(sgi)
	char remote[30];

	char dir[64], pfx[64], *tmpremote;
#endif


	/*
		assume mail is for remote, look for bang to confirm that
		assumption
	*/
	local = 0;
	while (*name=='!') name++;

	for (p=rsys; *name!='!'; *p++ = *name++)
		if (*name=='\0') {
			local++;
			break;
		}
	*p = '\0';

	if (local) sprintf(cmd, "rmail %s", rsys);
	if ((strcmp(thissys, rsys) == SAME)) {
		local++;
		sprintf(cmd, "rmail %s", name+1);
	}

	/*
		send local mail or remote via uux
	*/
	if (!local) {
		if (fromlevel > FROMLEVELS) {
			error = E_FROM;
			return(FALSE);
		}

#ifdef USE_NISEND
		/*
			If mail can't be sent over 3B network try the 
			next selected method.
		*/
		if (setjmp(nisendfail) == 0) {
			nisendjmp = TRUE;
			/*
				Send mail over 3B network
			*/
			if (access("/usr/3bnet/lib/3bsend", A_EXIST) != CERROR) {
				sprintf(dir,"/usr/3bnet/tmp");
				sprintf(pfx,"%s",thissys);
				tmpremote = tempnam(dir,pfx);
				sprintf(cmd, "/usr/3bnet/lib/3bsend -s -e -d%s -f%s -!'rmail %s < %s; rm %s' - 2> /dev/null", rsys, tmpremote, name+1, tmpremote, tmpremote);
#ifdef DEBUG
printf("%s\n", cmd);
#endif
				if (pipletr(n,cmd,local) == TRUE) {
					nisendjmp = FALSE;
					return(TRUE);
				}
			}
		}
		nisendjmp = FALSE;
#endif

#ifdef USE_NUSEND
		/*
			If mail can't be sent over NSC network use uucp.
		*/
		if (setjmp(nusendfail) == 0) {
			nusendjmp = TRUE;
			sprintf(remote, "%s%s", NSCCONS, rsys);
			if (access(remote, A_EXIST) != CERROR) {
				/*
					Send mail over NSC network
				*/
				sprintf(cmd, "/usr/bin/nusend -d %s -s -e -!'rmail %s' - 2>/dev/null", rsys, name+1);
#ifdef DEBUG
printf("%s\n", cmd);
#endif
				if (pipletr(n,cmd,local) == TRUE) {
					nusendjmp = FALSE;
					return(TRUE);
				}
			}
		}
		nusendjmp = FALSE;
#endif

#ifdef USE_USEND
		if (setjmp(usendfail) == 0) {
			usendjmp = TRUE;
			if (access("/usr/bin/usend", A_EXIST) != CERROR){
				sprintf(cmd, "/usr/bin/usend -s -d%s -uNoLogin -!'rmail %s' - 2>/dev/null", rsys, name+1);
#ifdef DEBUG
printf("%s\n", cmd);
#endif
				if (pipletr(n,cmd,local) == TRUE) {
					usendjmp = FALSE;
					return(TRUE);
				}
			}
		}
		usendjmp = FALSE;
#endif

		/*
			Use uux to send mail
		*/
		if (strchr(name+1, '!')) sprintf(cmd, "/usr/bin/uux - %s!rmail \\(%s\\)", rsys, name+1);
		else sprintf(cmd, "/usr/bin/uux - %s!rmail %s", rsys, name+1);
#ifdef DEBUG
printf("%s\n", cmd);
#endif
	}
	/*
		copy letter to pipe
	*/
	return(pipletr(n,cmd,local));
}

/*
	send letter n to name
		n	-> letter number
		name	-> mail destination
		level	-> depth of recursion for forwarding
	returns
		TRUE	-> mail sent
		FALSE	-> can't send mail
*/
#if defined(sgi)
int
xsend(int n, char *name, int level)
#else
xsend(n, name, level)	/* change name to avoid conflict with BSD syscall */
int	n;
char	*name;
#endif
{
	register char *p;
#if defined(sgi)
	struct	passwd	*pwd;
	char	file[MAXFILENAME];
	int rc=0;
#else
	char	file[MAXFILENAME],savefile[MAXFILENAME];
	int 	i=0, j=0, rc=0,lastsys=0;
	char	user[80];
#endif
	char	*temp;
#if !defined(sgi)
	char 	tmpsendto[1024];
#endif
	int	old_len;
	struct	stat	malf_stat;

	/*
		Shorten address of recipient --- if possible
	*/
	if (!flago) {
		temp=malloc((unsigned)(strlen(thissys) +3));
		sprintf(temp,"!%s!",thissys);
		/*
			Look for !thissys! in the address.
			to insure that thissys won't match substrings
			in other machine names.
		*/
		while ((rc = substr(name, temp)) > 0) {
			name = name + rc + strlen(temp);
		}
	}

	strcpy(uval, name);
	if(level > 20) {
		errmsg(E_UNBND,"");
		return(FALSE);
	}
	if (strcmp(name, "-") == SAME) return(TRUE);

	/*
		See if mail is to be fowarded
	*/
	for (p = name; *p != '!' && *p != '\0'; p++);
	if (*p == '!') {
		if (sendrmt(n, name) == FALSE) goback(0);
		return(TRUE);
	}

#ifndef sgi
	/*
		See if user has specified that mail is to be fowarded
	*/
	cat(file, maildir, name);
	if (areforwarding(file)) {
		strcpy(tmpsendto,xsendto);
		for (i = 0; tmpsendto[i] == ' ';) i++;
		while (tmpsendto[i] != '\n' && tmpsendto[i] != '\0') {
			for (j = 0; tmpsendto[i] != '\n' && tmpsendto[i] != ' ' && tmpsendto[i] != '\0';) user[j++] = tmpsendto[i++];
			user[j] = '\0';
			if (j) {
				if (!notme(user, name)) goback(0);
				else {
					xsend(n, user, level + 1);
				}
				if (tmpsendto[0] == '\0') break;
				while (tmpsendto[i] == ' ') i++;
			}
		}
		return(TRUE);
	}
#endif /* !sgi */

	/*
		see if user exists on this system
	*/
	setpwent();	
#ifdef sgi
	if((pwd = isuser(name)) == NULL) {
#else
	if((pwd = getpwnam(name)) == NULL) {
#endif /* sgi */
		fprintf(stderr, "%s: Can't send to %s\n",program,name);
		error = E_USER;
#ifdef sgi
        	if (!isdelivermail) goback(0);
#else
		goback(0);
#endif
		return(FALSE);
	}
	cat(file, maildir, name);

	/*
		Before we append to the user's mailfile we check
		for a prior failure and restore if need be
	*/
	cksaved(name);
	lock(file);
#if defined(sgi)
	createmf(file, pwd->pw_uid);
#else
	createmf(file);
#endif

	/*
		Append letter to mail box
	*/
	rc = 0;
	if (stat(file, &malf_stat) < 0)
		old_len = 0;
        else	old_len = malf_stat.st_size;

	if ((malf = fopen(file, "a")) == NULL) rc = 1;
	else if (copylet(n, malf, ORDINARY) == FALSE) rc = 1;
	if (rc) {
		fprintf(stderr, "%s: Cannot append to %s\n",program,file);

		fclose(malf);

		truncate(file, old_len);

		malf = fopen(file, "a");

		unlock();
		error = E_FILE;
#ifdef sgi
        	if (!isdelivermail) goback(0);
#else
		goback(0);
#endif
		return(FALSE);
	}
	fclose(malf);
	unlock();
	return(TRUE);
}

/*
	signal catching routine --- reset signals on quits and interupts
	exit on other signals
		i	-> signal #
*/
void
resetsig(i)
register int i;
{
	setsig(i, resetsig);

#ifdef USE_NISEND
	if (i == SIGPIPE && nisendjmp == TRUE) longjmp(nisendfail, 1);
#endif

#ifdef USE_NUSEND
	if (i == SIGPIPE && nusendjmp == TRUE) longjmp(nusendfail, 1);
#endif

#ifdef USE_USEND
	if (i == SIGPIPE && usendjmp == TRUE) longjmp(usendfail, 1);
#endif

	if (i > SIGQUIT) fprintf(stderr, "%s: ERROR signal %d\n",program,i);
	else fprintf(stderr, "\n");

	if (delflg && (i==SIGINT || i==SIGQUIT)) longjmp(sjbuf, 1);
	done(0);
}

/*
 * e_to_sys --
 *	Guess which errno's are temporary.  Gag me.
 */
int
#if defined(sgi)
e_to_sys(int num)
#else
e_to_sys(num)
	int num;
#endif
{
	int eval;

	if (num == 0)
		return(EX_OK);

	switch(num) {		
	case E_USER:		/* most likely due name serivce error */
	case E_FILE:
	case E_SPACE:
	case E_FORK:
	case E_PIPE:
	case E_LOCK:
	case E_MEM:
	case E_MBOX:
	case E_TMP:
		eval = EX_TEMPFAIL;
		break;
	default:
		eval = EX_UNAVAILABLE;
		break;
	}
	
	return(eval);
}

/*
	clean up lock files and exit
*/
void
done(int needtmp)
{
	unlock();
	if (!needtmp) unlink(lettmp);
	if (!maxerr) maxerr = error;
	if (isdelivermail) 
		exit(e_to_sys(maxerr));
	else	exit(maxerr);
}

/*
	concatenate from1 and from2 to to
		to	-> destination string
		from1	-> source string
		from2	-> source string
*/
#if defined(sgi)
void
cat(char *to, char *from1, char *from2)
#else
cat(to, from1, from2)
register char *to, *from1, *from2;
#endif
{
	for (; *from1;) *to++ = *from1++;
	for (; *from2;) *to++ = *from2++;
	*to = '\0';
}

/*
	get next token
		p	-> string to be searched
		s	-> area to return token
	returns:
		p	-> updated string pointer
		s	-> token
		NULL	-> no token
*/
#if defined(sgi)
char *
getarg(char *s, char *p)
#else
char *getarg(s, p)	
register char *s, *p;
#endif
{
	while (*p == ' ' || *p == '\t') p++;
	if (*p == '\n' || *p == '\0') return(NULL);
	while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\0') *s++ = *p++;
	*s = '\0';
	return(p);
}

/*
	check existence of file
		file	-> file to check
	returns:
		0	-> exists unwriteable
		1	-> exists writeable
		2	-> does not exist
*/
#if defined(sgi)
int
legal(char *file)
#else
legal(file)
register char *file;
#endif
{
	register char *sp;
	char dfile[MAXFILENAME];

	/*
		If file does not exist then try "." if file name has 
		no "/". For file names that have a "/", try check
		for existence of previous directory.
	*/
	if (access(file, A_EXIST) == A_OK) {
		if (access(file, A_WRITE) == A_OK)
			return(1);
		return(0);
	}
	else {
		if ((sp=strrchr(file, '/')) == NULL)
			cat(dfile, ".", "");
		else {
			strncpy(dfile, file, sp - file);
			dfile[sp - file] = '\0';
		}
		if (access(dfile, A_WRITE) == CERROR)
			return(0);
		return(2);
	}
}

/*
 	invoke shell to execute command waiting for command to terminate
		s	-> command string
	return:
		status	-> command exit status
*/
#if defined(sgi)
int
systm(char *s)
#else
systm(s)
char *s;
#endif
{
	int	pid;

	/*
		Spawn the shell to execute command, however, since the 
		mail command runs setgid mode reset the effective group 
		id to the real group id so that the command does not
		acquire any special privileges
	*/
	if ((pid = fork()) == CHILD) {
		setgid(getgid());
		execl("/bin/sh", "sh", "-c", s, NULL);
		exit(127);
	}
	return(1 - dowait(pid));
}

#ifndef sgi
/*
	Don't allow forwarding to oneself.

	If we are sending to system!user and he has 
		"Forward to system!user" 
			or
		"Forward to user" - error
*/
notme(fto, myname)
char *fto, *myname;
{
	char tosys[10], touser[10], work[20];
	int bangs = 0, k, l, lastbang = 0, priorbang = 0;

	if (strrchr(fto, '!') == NULL) {
		if (strcmp(fto, myname) == SAME) {
			strcpy(uval, myname);
			error = E_FRWL;
			return(FALSE);
		}
	}
	else {
		for (k = 0; fto[k] != '\0'; k++) 
			if (fto[k] == '!') {
				bangs++;
				priorbang=lastbang;
				lastbang=k;
			}

		if (bangs > 1) {
			for (k = 0; fto[priorbang + k + 1] != '\0'; k++) work[k] = fto[priorbang + k + 1];
			work[k] = '\0';
		}
		else strcpy(work, fto);

		for (k = 0; work[k] != '!'; k++) tosys[k] = work[k];
		tosys[k++] = '\0';

		for (l = 0; work[k] != '\0'; k++) touser[l++] = work[k];
		touser[l] = '\0';

		if ((strcmp(touser, myname) == SAME) && (strcmp(tosys, thissys) == SAME)) {
			strcpy(uval, myname);
			error = E_FRWL;
			return(FALSE);
		}
	}
	return(TRUE);
}
/*
	Handles installing/removeing forwarding
*/
doFopt(argcount)
{
	int	i,j;
	char	hold[1024], work[20];
#if defined(sgi)
	struct passwd *pwd;
#endif

	if (optcnt != 1) {
		errmsg(E_SYNTAX,"Forwarding is mutually exclusive of other options");
		done(0);
	}

	if (argcount > 3) {
		fprintf(stderr, "%s: Too many arguments for forwarding\n%s: To forward to multiple users say '%s -F \"user1 user2 ...\"'\n", program, program, program);
		error = E_SYNTAX;
		done(0);
	}

	if (error != E_FLGE) {
		errmsg(E_FILE,"Cannot install/remove forwarding without empty mailfile");
		done(0);
	}

	lock(mailfile);
	setpwent();
	pwd = getpwnam(my_name);
#if defined(sgi)
	if (pwd == NULL) {
		errmsg(E_USER, "Cannot get password entry");
		done(0);
	}
	createmf(mailfile, pwd->pw_uid);
#else
	createmf(mailfile);
#endif

	if (strlen(uval) > 0) {
		/*
			Remove excess blanks/tabs from uval
			Accept comma or space as delimeter
		*/
		for (i = 0, j = 0; uval[i] != '\0';) {
			if (uval[i] == ' ' || uval[i] == ',' || uval[i] == '\t') {
				for (i++; uval[i] == ' ' || uval[i] == '\t' || uval[i] == ','; i++);
				if (j > 0) hold[j++]=' ';
			}
			hold[j++] = uval[i++];
		}
		hold[j] = '\0';

		/*
			Don't allow forwarding to oneself
		*/
		for (i = 0, j = 0; hold[i] != '\0'; i++) {
			if (hold[i] == ' ') {
				work[j] = '\0';
				if (!notme(work, my_name)) {
					errmsg(E_SYNTAX,"Cannot install forwarding to oneself");
					done(0);
				}
				j = 0;
			}
			else work[j++] = hold[i];
		}
		work[j] = '\0';
		if (!notme(work, my_name)) {
			errmsg(E_SYNTAX,"Cannot install forwarding to oneself");
			done(0);
		}
		malf = doopen(mailfile, "w",E_FILE);
		printf("Forwarding to %s\n",hold);
		fprintf(malf, "Forward to %s\n",hold);
                fclose(malf);
	}
	else {
		if (areforwarding(mailfile)) {
			malf = doopen(mailfile, "w",E_FILE);
			fclose(malf);
			printf("Forwarding removed\n");
		}
		else fprintf(stderr,"%s: No forwarding to remove\n",program);
	}
	stamp();
	unlock();
	done(0);
}
#endif /* !sgi */

/*
	If mail file does not exist create it with the correct uid 
	and gid
*/
#if defined(sgi)
void
createmf(char *file, uid_t user)
#else
createmf(file)
char *file;
#endif
{
#if defined(sgi)
	void (*istat)(int), (*qstat)(int), (*hstat)(int);
#else
	int (*istat)(), (*qstat)(), (*hstat)();
#endif
	uid_t euid;

	if (access(file, A_EXIST) == CERROR) {
#if defined(sgi)
		const cap_value_t cv = CAP_SETUID;
		cap_t ocap;
		int fd;
		mode_t omask = umask(0);
#else
		umask(0);
#endif
		istat = signal(SIGINT, SIG_IGN);
		qstat = signal(SIGQUIT, SIG_IGN);
		hstat = signal(SIGHUP, SIG_IGN);
		euid = geteuid();
#if defined(sgi)
		ocap = cap_acquire(1, &cv);
		if (setreuid((uid_t) -1, user) == -1)
			perror("setreuid");
		cap_surrender(ocap);
		fd = creat(file, MFMODE);
		if (setreuid((uid_t) -1, euid) == -1)
			perror("setreuid");
		if (fd == -1)
			perror(file);
		(void) close(fd);
		(void) umask(omask);
#else
		seteuid(0);
		seteuid(pwd->pw_uid); /* set euid to recipient */
		close(creat(file, MFMODE));
		seteuid(0);
		seteuid(euid);        /* restore orginal euid */
		umask(7);
#endif
		signal(SIGINT, istat);
		signal(SIGQUIT, qstat);
		signal(SIGHUP, hstat);
	}
}
/*
	This routine looks for string2 in string1.
	If found, it returns the position string2 is found at,
	otherwise it returns a -1.
*/
#if defined(sgi)
int
substr(char *string1, char *string2)
#else
substr(string1, string2)
char *string1, *string2;
#endif
{
	int i,j, len1, len2;

	len1 = strlen(string1);
	len2 = strlen(string2);
	for (i = 0; i < len1 - len2 + 1; i++) {
		for (j = 0; j < len2 && string1[i+j] == string2[j]; j++);
		if (j == len2) return(i);
	}
	return(-1);
}


#ifdef sgi

int
extractadr(char *line, size_t linelen, char *tag, char *rtnbuf)
{
	size_t w, i;
	for (w = i = strlen(tag); line[w] != '\n' && w < linelen; w++)
		rtnbuf[w-i] =line[w];
	rtnbuf[w-i] = '\0';
	for (w = 0; rtnbuf[w] != '\0'; w++) {
		if (!isspace(rtnbuf[w]))
			return(1);
	}
	return(0);
}

#endif /* sgi */


/*
	This routine returns undeliverable mail as well as handles
	replying to letters
*/
#if defined(sgi)
void
goback(int ln)
#else
goback(ln)
int	ln;
#endif
{
int	i, w;
#ifdef sgi
int	 letlen, linelen, havereplyto, havefrom822;
#endif
char	buf[LSIZE], *cp, work[LSIZE], wuser[LSIZE];

	if (!error) error = 1;
	if (dflag < 2) {
		work[0] = '\0';
		wuser[0] = '\0';
		fclose(tmpf);
		if (!replying) dflag = 1;
		tmpf = doopen(lettmp,"r",E_TMP);
		if (replying) fseek(tmpf, let[ln].adr, 0);
#ifdef sgi
		/*
		 * When replying, look for a reply-to header field first
		 * and reply to any address there, after that, see if there
		 * is an RFC822-style "From:" header field and reply to any
		 * address there.
		 */
		letlen = let[ln+1].adr - let[ln].adr;
		havereplyto = 0;
		havefrom822 = 0;
		while (replying &&
		       fgets(line, LSIZE, tmpf) != NULL &&
		       line[0] != '\n') {

			linelen = strlen(line);
			if (strncasecmp(line, replyto, strlen(replyto))
			     == SAME) {
				if (extractadr(line, linelen, replyto, wuser)) {
					havereplyto++;
					break; /* replyto overrides all */
				}
			}
			else if (strncasecmp(line, from822, strlen(from822))
				 == SAME) {
				if (extractadr(line, linelen, from822, wuser)) {
					havefrom822++;
				}
			}
			else {
				if((letlen -= linelen ) <= 0)
					break;
				while(line[linelen-1] != '\n') {
					if (fgets(line, LSIZE, tmpf) == NULL)
						break;
				}
			}
		}
		if (!havereplyto && !havefrom822) {
			fseek(tmpf, let[ln].adr, 0);
#endif
		for (fgets(line,LSIZE,tmpf); strncmp(line,from,5) == SAME || strncmp(line +1,from,5) == SAME;) {
			if ((i = substr(line, "remote from")) != -1) {
				
				for (i = 0, cp = strrchr(line,' ') + 1; *cp != '\n'; cp++) buf[i++] = *cp;
				buf[i++] = '!';
				buf[i] = '\0';
				strcat(work, buf);
				if (line[0] == '>') i = 6;
				else i = 5;
				for (w = i; line[w] != ' '; w++) wuser[w-i] =line[w];
				wuser[w-i] = '\0';
			}
			else if ((i = substr(line, "forwarded by")) == -1) {
				if (line[0] == '>') break;
				else i = 5;
				for (w = i; line[w] != ' '; w++) wuser[w-i] =line[w];
				wuser[w-i] = '\0';
			}
			else if ((i = substr(line, "forwarded by")) > -1) break;
			fgets(line,LSIZE,tmpf);
		}
#ifdef sgi
		}
#endif
		strcat(work,wuser);
		fclose(tmpf);
		tmpf = doopen(lettmp,"r",E_TMP);
		if (strlen(work) > 0) {
			if (replying) {
				sprintf(buf,"mail %s '%s'", xsendto,work);
				printf("%s\n",buf);
				systm(buf);
				return;
			}
			if (interactive) strcpy(work,my_name);
			fprintf(stderr, "%s: Return to %s\n", program, work);
			sendrmt(0, work);
		}
	}
	if (dflag == 9) fprintf(stderr,"%s: Cannot return mail\n", program);
	if (dflag < 2) {
		dflag = 0;
		if (!maxerr) maxerr = error;
		error = 0;
	}
}
/*
	If the mailfile still exists (it may have been deleted), 
	time-stamp it; so that our re-writing of mail back to the 
	mailfile does not make shell think that NEW mail has arrived 
	(by having the file times change).
*/
#if defined(sgi)
void
stamp(void)
#else
stamp()
#endif
{
	if (access(mailfile, A_EXIST) == A_OK) 
		if (utime(mailfile, utimep) != A_OK)
			errmsg(E_FILE,"Cannot time-stamp mailfile");
}
/*
	Parse the command line.
	Return index of first non-option field (i.e. user)
*/
#if defined(sgi)
int
parse(int argcnt, char **arglst)
#else
parse(argcnt, arglst)
int	argcnt;
char	**arglst;
#endif
{
	register int c;

	/*
		Print in reverse order
	*/
	if (argcnt > 1
	    && arglst[1][0] == '+') {
		if (ismail) {
			forward = 1;
			argcnt--;
			arglst++;
		}
		else goerr++;
	}
#ifdef	sgi
#define	ARGLIST		"df:hropPqestw:"
#else
#define	ARGLIST		"f:hropqestwF:"
#endif
	while ((c = getopt(argcnt, arglst, ARGLIST)) != EOF) switch(c) {
#ifdef	sgi
		/*
		 * Check for delivermail flag.  If set, don't call sendmail
		 * to send mail; just drop it in the mailbox if possible.
		 */
		case 'd':
			isdelivermail++;
			optcnt++;
			break;
#endif
		/*
			do not print mail
 		*/
		case 'e':
			if (ismail) flge = 1;
			else goerr++;
			optcnt++;
			break;

		/*
			use alternate file as mailfile
		*/
		case 'f':
			if (ismail) {
				flgf = 1;
				mailfile = optarg;
				if (optarg[0] == '-') {
					errmsg(E_SYNTAX,"Files names must not begin with '-'");
			   		done(0);
				}
			}
			else goerr++;
			optcnt++;
			break;
		/*
			Print headers first
		*/
		case 'h':
			if (ismail) flgh = 1;
			else goerr++;
			optcnt++;
			break;
#ifndef sgi
		/*
			Install/Remove Forwarding
 		*/
		case 'F':
			if (ismail) {
				flge = 1;	/* set -e option */
                        	flgF = 1;	/* Indicate Forwarding */
				strncpy(uval, optarg, sizeof(uval));
				uval[sizeof(uval)-1]='\0';
			}
			else goerr++;
			optcnt++;
			break;
#endif /* !sgi */
		/*
			Don't add top new-line
		*/
		case 's':
			flgs = 1;
			optcnt++;
			break;
		/*
			turn off address optimization
		*/
		case 'o':
			flago = 1;
			oarg = optind - 1;
			optcnt++;
			break;
		/* 
			print without prompting
		*/
		case 'p':
			if (ismail) flgp++;
			else goerr++;
			optcnt++;
			break;
#ifdef sgi
		/*
			validate local users by checking the passwd file
			only.
		*/
		case 'P':
			pwonly++;
			optcnt++;
			break;
#endif /* sgi */

		/* 
			terminate on deletes
		*/
		case 'q':
			if (ismail) delflg = 0;
			else goerr++;
			optcnt++;
			break;
		/* 
			print by first in, first out order
		*/
		case 'r':
			if (ismail) forward = 1;
			else goerr++;
			optcnt++;
			break;
		/*
			add To: line to letters
		*/
		case 't':
			toflg = 1;
			targ = optind - 1;
			optcnt++;
			break;
		/*
			don't wait on sends
		*/
		case 'w':
			flgw = 1;
			break;
		/*
			bad option
		*/
		case '?':
			goerr++;
		}
		if (ismail && goerr > 0) {
			errmsg(E_SYNTAX,
#ifdef sgi
		   "usage: mail [-ehopqrstw] [-f file] [persons]");
#else
		   "usage: mail [-ehopqrstw] [-F user(s)] [-f file] [persons]");
#endif
			done(0);
		}
		if (optind < argcnt) return(optind);
		else return(0);
}

/*
	display all headers, indicating current and status
	current is the displacement into the mailfile of the 
	current letter
*/
#ifdef sgi
#define SENDERLEN 20
#endif

#if defined(sgi)
int
gethead(int current, int all)
#else
gethead(current,all)
int	current,all;
#endif
{

int	displayed = 0;
FILE	*file;
char	*hold;
char	holdval[100];
char	*wline;
char	wlineval[100];
int	ln;
char	mark;
int	rc, size, start,stop,ix;
#ifdef sgi
char	userval[SENDERLEN];
#else
char	userval[20];
#endif

	hold = holdval;
	wline = wlineval;

	printf("%d letters found in %s, %d scheduled for deletion, %d newly arrived\n", nlet, mailfile, changed, nlet - onlet);

	if (all==2 && !changed) return(0);

	file = doopen(lettmp,"r",E_TMP);
	if (!forward) {
		stop = current - 6;
		if (stop < -1) stop = -1;
		start = current + 5;
		if (start > nlet - 1) start = nlet - 1;
		if (all) {
			start = nlet -1;
			stop = -1;
		}
	}
	else {
		stop = current + 6;
		if (stop > nlet - 1) stop = nlet - 1;
		start = current - 5;
		if (start < 0) start = 0;
		if (all) {
			start = 0;
			stop = nlet;
		}
	}
	for (ln = start; ln != stop; ln = forward ? ln + 1 : ln - 1) {
		size = let[ln+1].adr - let[ln].adr;
		if ((rc = fseek(file, let[ln].adr, 0)) != 0) {
			errmsg(E_FILE,"Cannot seek header");
			fclose(file);
			return(1);
		}
		if (fgets(wline, 100, file) == NULL) {
			errmsg(E_FILE,"Cannot read header");
			fclose(file);
			return(1);
		}
		if ((rc = strncmp(wline, from, 5)) != SAME) {
			errmsg(E_FILE,"Invalid header encountered");
			fclose(file);
			return(1);
		}
		strcpy(hold, wline + 5);
		fgets(wline, 100, file);
		while ((rc = strncmp(wline, ">From ", 6)) == SAME && substr(wline,"remote from ") != -1) {
			strcpy(hold, wline + 6);
			fgets(wline, 100, file);
		}
	
#ifdef sgi
		for (ix = 0,rc = 0; hold[rc] != ' ' && hold[rc] != '\t'
                                    && ix < SENDERLEN; ++rc) {
#else
		for (ix = 0,rc = 0; hold[rc] != ' ' && hold[rc] != '\t'; ++rc) {
#endif
			userval[ix++] = hold[rc];
			if (hold[rc] == '!') ix = 0;
		}
		userval[ix] = '\0';

#ifdef sgi
		for (; !isspace(hold[rc]) && hold[rc] != '\0'; ++rc);
#endif
		for (; hold[rc] == ' ' || hold[rc] == '\t'; ++rc);
		strcpy(wline, hold + rc);
		for (rc = 0; wline[rc] != '\n'; ++rc);
		wline[rc] = '\0';
	
		if (!flgh && current == ln) mark = '>';
		else mark = ' ';
	
		if (all == 2) {
			if (displayed >= changed) {
				fclose(file);
				return(0);
			}
			if (let[ln].change == ' ') continue;
		}
#ifdef sgi
		printf("%c %3d  %c  %-5d  %-*s  %s\n", mark, ln + 1, let[ln].change, size, SENDERLEN, userval, wline);
#else
		printf("%c %3d  %c  %-5d  %-10s  %s\n", mark, ln + 1, let[ln].change, size, userval, wline);
#endif
		displayed++;
	}
	fclose(file);
	return(0);
}

#if defined(sgi)
void
tmperr(void)
#else
tmperr()
#endif
{
	fclose(tmpf);
	errmsg(E_TMP,"");
	return;
}
/*
	Write a string out to tmp file, with error checking.
	Return 1 on success, else 0
*/
#if defined(sgi)
int
wtmpf(char *string, size_t length)
#else
wtmpf(string,length)
char	*string;
#endif
{
	if (fwrite(string,1,length,tmpf) != length) {
		tmperr();
		return(0);
	}
	return(1);
}
/*
	Read a line from stdin, assign it to line and
	return number of bytes in length
*/
#if defined(sgi)
int
getline(int maxlen, FILE *f)
#else
getline(maxlen,f)
FILE	*f;
#endif
{
int	i,ch;
	for (i=0; (ch=getc(f)) != EOF  && i < maxlen; ) 
		if ((line[i++] = ch) == '\n') break;
	line[i] = '\0';
	return(i);
}
/*
	Make temporary file for letter
*/
#if defined(sgi)
void
mktmp(void)
#else
mktmp()
#endif
{
	lettmp = tempnam(tmpdir,"mail");
	tmpf = doopen(lettmp,"w",E_TMP);
}
/*
	Get a number from user's reply,
	return its value or zero if none present, -1 on error
*/
#if defined(sgi)
int
getnumbr(char *s)
#else
getnumbr(s)
char	*s;
#endif
{
	int	k = 0;

	for ( ;*s == ' ' || *s == '\t'; ) s++;

	if (*s != '\0') {
		if ((k = atoi(s)) != 0) 
			if (!validmsg(k)) return(-1);

		for (; *s >= '0' && *s <= '9';) s++;
		if (*s != '\0' && *s != '\n') {
			printf("Illegal numeric\n");
			return(-1);
		}
		return(k);
	}
	return(0);
}
/*
	If valid msgnum return 1,
		else print message and return 0
*/
#if defined(sgi)
int
validmsg(int i)
#else
validmsg(i)
#endif
{
	if (i < 0) {
		printf("No current message\n");
		return(0);
	}
	if (i > nlet) {
		printf("No such message\n");
		return(0);
	}
	return(1);
}
/*
	Set letter to passed status, and adjust changed as necessary
*/
#if defined(sgi)
void
setletr(int letter, char status)
#else
setletr(letter,status)
char	status;
#endif
{
	if (status == ' ') {
		if (let[letter].change != ' ') 
			if (changed) changed--;
	}
	else if (let[letter].change == ' ') changed++;
	let[letter].change = status;
}
/*
	Fork, reset the users identity, pipe the specified letter 
	into the passed command, wait.

	Returns TRUE on success, FALSE on failure
*/
#if defined(sgi)
int
pipletr(int letter, char *command, int local)
#else
pipletr(letter, command, local)
int	letter, local;
char	*command;
#endif
{
FILE	*rmf;

	int	pid, rc;

	/*
		Spawn the shell to execute command, however, since the 
		mail command runs setgid mode reset the effective group 
		id to the real group id so that the command does not
		acquire any special privileges
	*/
	if ((pid = fork()) == CHILD) {
		setgid(getgid());
		if ((rmf = popen(command, "w")) != NULL) {
			copylet(letter, rmf, local? FORWARD: REMOTE);
			rc = pclose(rmf);
			if (rc != -1) exit(rc>>8);
		}
		errmsg(E_PIPE,"");
		exit(1);
	}
	return(dowait(pid));
}
/*
	Routine creates dead.letter
*/
#if defined(sgi)
void
mkdead(void)
#else
mkdead()
#endif
{
	int aret;

		/*
			Try to create dead letter in current directory
			or in home directory
		*/
		umask(umsave);
		if ((aret = legal(&dead[1]))) malf = fopen(&dead[1], "w");
		if ((malf == NULL) || (aret == 0)) {
			/*
				try to create in $HOME
			*/
			if((hmdead = malloc(strlen(home) + strlen(dead) + 1)) == NULL) {
				fprintf(stderr, "%s: Can't malloc\n",program);
				goto out;
			}
			cat(hmdead, home, dead);
			if ((aret=legal(hmdead))) malf = fopen(hmdead, "w");
			if ((malf == NULL) || (aret == 0)) {
				fprintf(stderr, "%s: Cannot create %s\n",program,&dead[1]);
			out:
				fclose(tmpf);
				error = E_FILE;
				umask(7);
				return;
			}  else {
				chmod(hmdead, DEADPERM);
				fprintf(stderr,"%s: Mail saved in %s\n",program,hmdead);
			}
		} else {
			chmod(&dead[1], DEADPERM);
			fprintf(stderr,"%s: Mail saved in %s\n",program,&dead[1]);
		}
		/*
			Copy letter into dead letter box
		*/
		umask(7);
		if (copylet(0, malf, ZAP) == FALSE) errmsg(E_DEAD,"");
		fclose(malf);
}
#if defined(sgi)
int
dowait(pid_t pidval)
#else
dowait(pidval)
int	pidval;
#endif
{
#if defined(sgi)
	pid_t w;
#else
	register int pid, w;
#endif
	int status;
#if defined(sgi)
	void (*istat)(), (*qstat)();
#else
	int (*istat)(), (*qstat)();
#endif

	/*
		Parent temporarily ignores signals so it will remain 
		around for command to finish
	*/
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);

	while ((w = wait(&status)) != pidval && w != CERROR);
	if (w == CERROR) status = CERROR;

	signal(SIGINT, istat);
	signal(SIGQUIT, qstat);
	if (!status) return(TRUE);
	else return(FALSE);
}
/*
	Prints error messages. If string is supplied that is text for
	the message, otherwise the text for the err_val message is
	gotten from the errlist[] array
*/
#if defined(sgi)
void
errmsg(int err_val, char *err_txt)
#else
errmsg(err_val,err_txt)
char	*err_txt;
#endif
{
	error = err_val;
	if (strlen(err_txt)) fprintf(stderr,"%s: %s\n",program,err_txt);
	else fprintf(stderr,"%s: %s\n",program,errlist[err_val]);
}
#if defined(sgi)
void
cksaved(char *user)
#else
cksaved(user)
char	*user;
#endif
{
	struct stat stbuf;
	struct passwd *pwd_ptr;
	int restore = 0,c,m_age,s_age;
	FILE *Istream, *Ostream;
#if defined(sgi)
	char command[100];
#else
	char *cptr,command[100];
#endif
	char save[100],mail[100], home[100];

	cat(mail,maildir,user);
	cat(save,mailsave,user);

	/*
		If no save file return, otherwise save the size
		and time of the savefile
	*/
#if defined(sgi)
	if (stat(save,&stbuf) != 0) return;
#else
	if (stat(save,&stbuf) != 0) return(0);
#endif
	s_age = stbuf.st_mtime;

	/*
		Ok, we have a savefile, if no mailfile exists,
		then we want to restore to the mailfile,
		else we restore to the $HOME directory
	*/
	if (stat(mail,&stbuf) != 0) restore = 1;
	else {
		m_age = stbuf.st_mtime;
		if (m_age > s_age && (pwd_ptr = getpwnam(user)) != NULL) {
			lock(mail);
			strcpy(home,pwd_ptr->pw_dir);
			strcat(home,"/MAIL.SAVED");
			if ((Ostream = fopen(home,"a")) == NULL) {
				fprintf(stderr,"%s: Cannot open file '%s' for output\n",program,home);
				unlock();
				return;
			}
			if ((Istream = fopen(save,"r")) == NULL) {
				fprintf(stderr,"%s: Cannot open saved file '%s' for reading\n",program,save);
				fclose(Ostream);
				unlock();
				return;
			}
			while ((c = fgetc(Istream)) != EOF) fputc(c,Ostream);
			fclose(Istream);
			fclose(Ostream);
			if (unlink(save) != 0) {
				perror("Unlink of save file failed");
				unlock();
				return;
			}
			unlock();
			chmod(home,MFMODE);
			sprintf(command,"echo \"Your mail save file has just been appended to\n'%s' by the mail program.\" | mail %s",home,user);
			systm(command);
			return;
		}
	}
	if (restore) {
		/*
			Restore from the save file by linking
			it to $MAIL then unlinking save file
		*/
		lock(mail);
		if (link(save,mail) != 0) {
			unlock();
			perror("Restore failed to link to mailfile");
#if defined(sgi)
			return;
#else
			return(-1);
#endif
		}

		chmod(mail,MFMODE);

		if (unlink(save) != 0) {
			unlock();
			perror("Cannot unlink saved file");
#if defined(sgi)
			return;
#else
			return(-1);
#endif
		}

		unlock();
		/*
			Try to send mail to the user whose file
			is being restored.
		*/
		sprintf(command,"echo \"Your mailfile was just restored by the mail program.\nPermissions of your mailfile are set to 0660.\"| mail %s",user);
		systm(command);

	}
#if !defined(sgi)
	return(0);
#endif
}


#define GOT_LOCK	0
#define TRY_AGAIN	1
#define SYS_ERROR	2
#define LOCK_EXISTS	3

#define PHONY_PID	"0"

static int debug = 0;	/* XXXrs */

int
makelock(char *name, char *suffix, char *lockname, int *lockfd, int timeout)
{
	char lckname[PATH_MAX], tmpname[PATH_MAX];
	int fd;
	struct stat sbuf;
	time_t lcktime, tmptime;

	/* Manufacture lock name and temp name */
	strcpy(lckname, name);
	strcat(lckname, suffix);
	strcpy(tmpname, lckname);
	strcat(tmpname, ".temp");

	if (debug)
		printf("makelock: attempting to lock %s...\n", name);

	/* Create temp file */
	if ((fd = open(tmpname, O_CREAT|O_TRUNC|O_RDWR, 0660)) < 0) {
		if (debug)
			printf("makelock: error opening %s (%s)\n",
			       tmpname, strerror(errno));
		return SYS_ERROR;
	}

	/* Write an always-good pid into it for backwards compatibility */
	if (write(fd, PHONY_PID, strlen(PHONY_PID)) != strlen(PHONY_PID)) {
		if (debug)
			printf("makelock: error writing to %s (%s)\n",
			       tmpname, strerror(errno));
		unlink(tmpname);
		close(fd);
		return SYS_ERROR;
	}

	/* Link to lock name */
	if (link(tmpname, lckname) == 0) {
		/* Lock is ours! */
		if (debug)
			printf("makelock: got lock %s\n", lckname);
		unlink(tmpname);
		strcpy(lockname, lckname);
		*lockfd = fd;
		return GOT_LOCK;
	}

	if (errno != EEXIST) {
		if (debug)
			printf("makelock: error linking %s to %s (%s)\n",
			       tmpname, lckname, strerror(errno));
		if (errno == ENOENT) {
			/*
			 * We must have raced with another process
			 * creating the temp file.  Try again
			 * immediately.
			 */
			close(fd);
			return TRY_AGAIN;
		}
		unlink(tmpname);
		close(fd);
		return SYS_ERROR;
	}

	/* Lock file exists. */

	if (!timeout) { /* Lock is good indefinitely */
		if (debug)
			printf("makelock: lock %s exists\n", lckname);
		unlink(tmpname);
		close(fd);
		return LOCK_EXISTS;
	}

	/* See if lock file is "too old" */
	if (stat(tmpname, &sbuf) < 0) {
		if (debug)
			printf("makelock: error stating %s (%s)\n",
			       tmpname, strerror(errno));
		if (errno == ENOENT) {
			close(fd);
			return TRY_AGAIN;
		}
		unlink(tmpname);
		close(fd);
		return SYS_ERROR;
	}
	tmptime = sbuf.st_ctime;
	unlink(tmpname);
	close(fd);
	if (stat(lckname, &sbuf) < 0) {
		if (debug)
			printf("makelock: error stating %s (%s)\n",
			       lckname, strerror(errno));
		if (errno == ENOENT)
			return TRY_AGAIN;
		return SYS_ERROR;
	}
	lcktime = sbuf.st_ctime;
	if (tmptime > lcktime && (tmptime - lcktime) > timeout) {
		if (debug)
			printf("makelock: lock %s is older than %d sec\n",
			       lckname, timeout);
		unlink(lckname);
		return TRY_AGAIN;
	}
	if (debug)
		printf("makelock: lock %s exists\n", lckname);
	return LOCK_EXISTS;
}


#if defined(sgi)
int
lock(char *file)
#else
lock(file)
char *file;
#endif
{
	int lckstat, trys;

	if (file[0] == '\0') {
		if (debug)
			printf("lockit: no filename supplied.\n");
		return 0;
	}

	if (curlockfd != -1) {
		if (debug)
			printf("lockit: already holding %s\n", curlock);
		return -1;
	}

	if (debug)
		printf("lockit: attempting to lock %s\n", file);

	for (trys = 0; trys < 100; trys++) {

		lckstat = makelock(file, maillock, &curlock[0],
				   &curlockfd, 300);

		switch(lckstat) {

		case GOT_LOCK:
			return 0;

		case LOCK_EXISTS:
			sleep(5);
			/* fall through... */

		case TRY_AGAIN:
			if (debug)
				printf("lockit: retrying lock...\n");
			continue;

		case SYS_ERROR:
		default:
			return -1;
		}
	}
	if (debug)
		printf("lockit: giving up after 100 trys.\n");
	return -1;
}

/*
 * Remove the mail lock, and note that we no longer
 * have it locked.
 */

#if defined(sgi)
void
unlock(void)
#else
unlock()
#endif
{
	if (curlockfd == -1) {
		if (debug)
			printf("unlock: no lock held?!?\n");
		return;
	}
	close(curlockfd);
	unlink(curlock);
	curlockfd = -1;
}

#ifdef sgi
struct passwd *
isuser(char *name)
{
	struct passwd *pwp;
	struct dirent64 *dp;
	DIR *dirp;
	char *dirname, *basename;

	if (((pwp = getpwnam(name)) == NULL) || pwonly)
		return(pwp);
	if ((dirname = strdup(pwp->pw_dir)) == NULL)
		return(NULL);
	if (dirname[0] == '/' && dirname[1] == '\0') {
		free(dirname);
		return(pwp);
	}
	do {
		if ((basename = strrchr(dirname, '/')) == NULL) {
			free(dirname);
			return(NULL);
		}
		*basename++ = '\0';
	} while (*basename == '\0');

	/*
	 * Note that a simple stat or access is not used in the interest
	 * of avoiding automount storms.
	 */
	dirp = opendir(*dirname == '\0' ? "/" : dirname);
	if (dirp) {
		while ((dp = readdir64(dirp)) != NULL) {
			if (strcmp(dp->d_name, basename) == 0) {
				free(dirname);
				return(pwp);
			}
		}
		closedir(dirp);
	}
	free(dirname);
	return(NULL);
}

int
mychown(char *file, uid_t uid, gid_t gid)
{
	const cap_value_t cv = CAP_CHOWN;
	cap_t ocap;
	int rtn;

	if (rstchown)
		ocap = cap_acquire(1, &cv);
	rtn = chown(file, uid, gid);
	if (rstchown)
		cap_surrender(ocap);
	return(rtn);
}
#endif /* sgi */
