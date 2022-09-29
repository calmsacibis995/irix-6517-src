/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.22 $"

#include "parms.h"

#ifdef BSD4_2
#define V7
#undef NONAP
#undef FASTTIMER
#endif /* BSD4_2 */

#ifdef FASTTIMER
#undef NONAP
#endif

#ifdef V8
#define V7
#endif /* V8 */

#ifdef sgi
#include <unistd.h>
#include <stdlib.h>
#include <ulimit.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>
#include <sys/param.h>

/*
 * param.h includes types.h and signal.h in 4bsd
 */
#ifdef V7
#include <sgtty.h>
#include <sys/timeb.h>
#else /* !V7 */
#ifdef sgi
#include <termios.h>
#else
#include <termio.h>
#include <sys/types.h>
#endif
#include <signal.h>
#include <fcntl.h>
#endif

#include <sys/stat.h>
#ifndef sgi
#include <sys/dir.h>
#endif

#ifdef BSD4_2
#include <sys/time.h>
#else /* !BSD4_2 */
#include <time.h>
#endif

#include <sys/times.h>
#include <errno.h>
#ifdef sgi
#include <string.h>
#include <bstring.h>
#include <dirent.h>
#include <getopt.h>
#endif

#ifdef ATTSV
#include <sys/sysmacros.h>
#endif /* ATTSV */

#ifdef	RT
#include "rt/types.h"
#include "rt/unix/param.h"
#include "rt/stat.h"
#include <sys/ustat.h>
#endif /* RT */

/* what mode should D. files have upon creation? */
#define DFILEMODE 0600

/* what mode should C. files have upon creation? */
#define CFILEMODE 0644

/* define the value of DIRMASK, for umask call */
/* used for creating system subdirectories */
#define DIRMASK 0022

#define MAXSTART	300	/* how long to wait on startup */

/* define the last characters for ACU  (used for 801/212 dialers) */
#define ACULAST "<"

/*  caution - the fillowing names are also in Makefile
 *    any changes here have to also be made there
 *
 * it's a good idea to make directories .foo, since this ensures
 * that they'll be ignored by processes that search subdirectories in SPOOL
 *
 *  XQTDIR=/var/spool/uucp/.Xqtdir
 *  CORRUPT=/var/spool/uucp/.Corrupt
 *  LOGDIR=/var/spool/uucp/.Log
 *  SEQDIR=/var/spool/uucp/.Sequence
 *  STATDIR=/var/spool/uucp/.Status
 *
 */

/* where to put the STST. files? */
#define STATDIR		"/var/spool/uucp/.Status"

/* where should logfiles be kept? */
#define LOGUUX		"/var/spool/uucp/.Log/uux"
#define LOGUUXQT	"/var/spool/uucp/.Log/uuxqt"
#define LOGUUCP		"/var/spool/uucp/.Log/uucp"
#define LOGCICO		"/var/spool/uucp/.Log/uucico"
#define CORRUPTDIR	"/var/spool/uucp/.Corrupt"

/* some sites use /usr/lib/uucp/.XQTDIR here */
/* use caution since things are linked into there */
#define XQTDIR		"/var/spool/uucp/.Xqtdir"

/* how much of a system name can we print in a [CX]. file? */
/* MAXBASENAME - 1 (pre) - 1 ('.') - 1 (grade) - 4 (sequence number) */
#define SYSNSIZE (MAXBASENAME - 7)

#ifdef USRSPOOLLOCKS
#define LOCKPRE		"/var/spool/locks/LCK."
#else
#define LOCKPRE		"/var/spool/uucp/LCK."
#endif /* USRSPOOLLOCKS */

#define SQFILE		"/etc/uucp/SQFILE"
#define SQTMP		"/etc/uucp/SQTMP"
#define SLCKTIME	5400	/* system/device timeout (LCK.. files) */
#define DIALCODES	"/etc/uucp/Dialcodes"
#define PERMISSIONS	"/etc/uucp/Permissions"

#define SPOOL		"/var/spool/uucp"
#define SEQDIR		"/var/spool/uucp/.Sequence"

#define X_LOCKTIME	3600
#ifdef USRSPOOLLOCKS
#define SEQLOCK		"/var/spool/locks/LCK.SQ."
#define SQLOCK		"/var/spool/locks/LCK.SQ"
#define X_LOCK		"/var/spool/locks/LCK.X"
#define S_LOCK		"/var/spool/locks/LCK.S"
#define X_LOCKDIR	"/var/spool/locks"	/* must be dir part of above */
#else
#define SEQLOCK		"/var/spool/uucp/LCK.SQ."
#define SQLOCK		"/var/spool/uucp/LCK.SQ"
#define X_LOCK		"/var/spool/uucp/LCK.X"
#define S_LOCK		"/var/spool/uucp/LCK.S"
#define X_LOCKDIR	"/var/spool/uucp"	/* must be dir part of above */
#endif /* USRSPOOLLOCKS */
#define X_LOCKPRE	"LCK.X"		/* must be last part of above */

#define PUBDIR		"/var/spool/uucppublic"
#define ADMIN		"/var/spool/uucp/.Admin"
#define ERRLOG		"/var/spool/uucp/.Admin/errors"
#define SYSLOG		"/var/spool/uucp/.Admin/xferstats"
#define RMTDEBUG	"/var/spool/uucp/.Admin/audit"
#define CLEANUPLOGFILE	"/var/spool/uucp/.Admin/uucleanup"

#define	WORKSPACE	"/var/spool/uucp/.Workspace"

#define SQTIME		60
#define TRYCALLS	2	/* number of tries to dial call */
#define MINULIMIT	(1L<<11)	/* minimum reasonable ulimit */
#define	MAX_LOCKTRY	5	/* number of attempts to lock device */

/*
 * CDEBUG is for communication line debugging
 * DEBUG is for program debugging
 */
#define CDEBUG(l, f, s) if (Debug >= l) fprintf(stderr, f, s)
#define CDEBUG0(l, f) if (Debug >= l) fprintf(stderr, f)
#undef DEBUG
#define DEBUG(l, f, s) if (Debug >= l) fprintf(stderr, f, s)
#define DEBUG0(l, f) if (Debug >= l) fprintf(stderr, f)

/*
 * VERBOSE is used by cu and ct to inform the user
 * about the progress of connection attempts.
 * In uucp, this will be NULL.
 */

#ifdef STANDALONE
#define VERBOSE(f, s) if (Verbose > 0) fprintf(stderr, f, s); else
#else
#define VERBOSE(f, s)
#endif

#define PREFIX(pre, str)	(strncmp((pre), (str), strlen(pre)) == SAME)
#define BASENAME(str, c) ((Bnptr = strrchr((str), c)) ? (Bnptr + 1) : (str))
#define EQUALS(a,b)	((a != CNULL) && (b != CNULL) && (strcmp((a),(b))==SAME))
#define EQUALSN(a,b,n)	((a != CNULL) && (b != CNULL) && (strncmp((a),(b),(n))==SAME))
#define LASTCHAR(s)	(s+strlen(s)-1)

#define SAME 0
#define ANYREAD 04
#define ANYWRITE 02
#define FAIL -1
#define SUCCESS 0
#define NULLCHAR	'\0'
#define CNULL (char *) 0
#define STBNULL (struct sgttyb *) 0
#define MASTER 1
#define SLAVE 0
#define MAXBASENAME 14 /* should be DIRSIZ but 4.2bsd prohibits that */
#ifdef sgi
#define MAXFULLNAME 1024	/* 8K is too big; sys/nami.h too much hassle */
#else
#define MAXFULLNAME BUFSIZ
#endif /* sgi */
#define MAXNAMESIZE	64	/* /var/spool/uucp/<14 chars>/<14 chars>+slop */
#define MAXMSGTIME 33
#define MAXEXPECTTIME 45
#define MAXCHARTIME 15
#define NAMESIZE MAXBASENAME+1
#define	SIZEOFPID	10		/* maximum number of digits in a pid */
#define LOCKPID_PAT	"%10ld\n"	/* PID in lock files
					 * --must match SIZEOFPID */
#define EOTMSG "\004\n\004\n"
#define CALLBACK 1

/* manifests for sysfiles.c's sysaccess()	*/
/* check file access for REAL user id */
#define	ACCESS_SYSTEMS	1
#define	ACCESS_DEVICES	2
#define	ACCESS_DIALERS	3
/* check file access for EFFECTIVE user id */
#define	EACCESS_SYSTEMS	4
#define	EACCESS_DEVICES	5
#define	EACCESS_DIALERS	6


/* manifest for chkpth flag */
#define CK_READ		0
#define CK_WRITE	1

/*
 * commands
 */
#define SHELL		"/bin/sh"
#define MAIL		"mail"
#define UUCICO		"/usr/lib/uucp/uucico"
#define UUXQT		"/usr/lib/uucp/uuxqt"
#define UUCP		"uucp"


/* system status stuff */
#define SS_OK			0
#define SS_NO_DEVICE		1
#define SS_TIME_WRONG		2
#define SS_INPROGRESS		3
#define SS_CONVERSATION		4
#define SS_SEQBAD		5
#define SS_LOGIN_FAILED		6
#define SS_DIAL_FAILED		7
#define SS_BAD_LOG_MCH		8
#define SS_LOCKED_DEVICE	9
#define SS_ASSERT_ERROR		10
#define SS_BADSYSTEM		11
#define SS_CANT_ACCESS_DEVICE	12
#define SS_DEVICE_FAILED	13	/* used for interface failure */
#define SS_WRONG_MCH		14
#define SS_CALLBACK		15
#define SS_RLOCKED		16
#define SS_RUNKNOWN		17
#define SS_RLOGIN		18
#define SS_UNKNOWN_RESPONSE	19
#define SS_STARTUP		20
#define SS_CHAT_FAILED		21

#define MAXPH	60	/* maximum phone string size */
#define	MAXC	BUFSIZ

#define	TRUE	1
#define	FALSE	0
#define NAMEBUF	32

/* structure of an Systems file line */
#define F_MAX	50	/* max number of fields in Systems file line */
#define F_NAME 0
#define F_TIME 1
#define F_TYPE 2
#define F_CLASS 3	/* an optional prefix and the speed */
#define F_PHONE 4
#define F_LOGIN 5

/* structure of an Devices file line */
#define D_TYPE 0
#define D_LINE 1
#define D_CALLDEV 2
#define D_CLASS 3
#define D_CALLER 4
#define D_ARG 5
#define D_MAX	50	/* max number of fields in Devices file line */

#define D_ACU 1
#define D_DIRECT 2
#define D_PROT 4

/* past here, local changes are not recommended */
#define CMDPRE		'C'
#define DATAPRE		'D'
#define XQTPRE		'X'

/*
 * stuff for command execution
 */
#define X_RQDFILE	'F'
#define X_STDIN		'I'
#define X_STDOUT	'O'
#define X_CMD		'C'
#define X_USER		'U'
#define X_BRINGBACK	'B'
#define X_MAILF		'M'
#define X_RETADDR	'R'
#define X_COMMENT	'#'
#define X_NONZERO	'Z'
#define X_SENDNOTHING	'N'
#define X_SENDZERO	'n'


/* This structure describes call routines */
struct caller {
	char	*CA_type;
	int	(*CA_caller)(char *[], char *[]);
};

/* This structure describes dialing routines */
struct dialer {
	char	*DI_type;
	int	(*DI_dialer)(char *[], char *[]);
};

struct nstat {
	int	t_pid;		/* process id				*/
	long	t_start;	/* process id				*/
	time_t	t_beg;		/* start  time				*/
	time_t	t_scall;	/* start call to system			*/
	time_t	t_ecall;	/* end call to system			*/
	time_t	t_tacu;		/* acu time				*/
	time_t	t_tlog;		/* login time				*/
	time_t	t_sftp;		/* start file transfer protocol		*/
	time_t	t_sxf;		/* start xfer				*/
	time_t	t_exf;		/* end xfer				*/
	time_t	t_eftp;		/* end file transfer protocol		*/
	time_t	t_qtime;	/* time file queued			*/
	int	t_ndial;	/* # of dials				*/
	int	t_nlogs;	/* # of login trys			*/
	struct tms t_tbb;	/* start execution times		*/
	struct tms t_txfs;	/* xfer start times			*/
	struct tms t_txfe;	/* xfer end times			*/
	struct tms t_tga;	/* garbage execution times		*/
};

/* external declarations */

extern int (*Read)(int, void *, unsigned);
extern int (*Write)(int, const void *, unsigned);
extern int (*Ioctl)(int, int, ...);
extern int (*Setup)(int, int *, int *);
extern int (*Teardown)(int, int, int);

extern int Ifn, Ofn;
extern int Debug, Verbose;
extern int Bspeed;
extern int Uid, Euid;		/* user-id and effective-uid */
extern int Ulimit;
extern mode_t Dev_mode;		/* save device mode here */
extern char Wrkdir[];
extern long Retrytime;
extern char **Env;
extern char Uucp[];
extern char Pchar;
extern struct nstat Nstat;
extern char Dc[];			/* line name			*/
extern char Fwdname[];		/* foward name			*/
extern int Seqn;			/* sequence #			*/
extern int Role;
extern char Logfile[];
extern int linebaudrate;	/* adjust sleep time on read in pk driver */
extern char Rmtname[];
extern char User[];
extern char Loginuser[];
extern char *Thisdir;
extern char *Spool;
extern char *Pubdir;
extern char Myname[];
extern char Progname[];
extern char RemSpool[];
extern char *Bnptr;		/* used when BASENAME macro is expanded */
extern char *sys_errlist[];

extern int conn_trycalls;	/* # of times getto() should try */
extern int AbortSeen, Aborts;

extern char lock_pid[SIZEOFPID+2];  /* PIDs for locked devices */
extern int Nlocks;
extern char *AbortStr[];

extern char Jobid[];		/* Jobid of current C. file */
extern int Uerror;		/* global error code */
extern char *UerrorText[];	/* text for error code */

/*	Some global I need for section 2 and section 3 routines */
extern errno;
extern char *optarg;	/* for getopt() */
extern int optind;	/* for getopt() */

/*	externs for sysfile.c	*/
extern char *Systems[];
extern char *Devices[];
extern char *Dialers[];

#define UERRORTEXT		UerrorText[Uerror]
#define UTEXT(x)		UerrorText[x]

/* things get kind of tricky beyond this point -- please stay out */

#ifdef ATTSV
#define index strchr
#define rindex strrchr
#define vfork fork
#define ATTSVKILL
#define UNAME
#else
#define strchr index
#define strrchr rindex
#endif

#ifdef lint
#define ASSERT(e, s1, s2, i1)	;

#else /* NOT lint */

#define ASSERT(e, s1, s2, i1) if (!(e)) {\
	assert(s1, s2, i1, __FILE__, __LINE__);\
	cleanup(FAIL);};
#endif

extern struct stat __s_;
#define READANY(f)	((stat((f),&__s_)==0) && ((__s_.st_mode&(0004))!=0) )
#define READSOME(f)	((stat((f),&__s_)==0) && ((__s_.st_mode&(0444))!=0) )

#define WRITEANY(f)	((stat((f),&__s_)==0) && ((__s_.st_mode&(0002))!=0) )
#define DIRECTORY(f)	((stat((f),&__s_)==0) && ((__s_.st_mode&(S_IFMT))==S_IFDIR) )
#define NOTEMPTY(f)	((stat((f),&__s_)==0) && (__s_.st_size!=0) )

/* standard functions used */

/* uucp functions and subroutine */
extern void	cleanup(int);

extern void	(*genbrk)(int);

extern int	anlwrk(char *, char **, int);		/* anlwrk.c */
extern int	iswrk(char *);				/* anlwrk.c */
extern int	gtwvec(char *, char **, int);		/* anlwrk.c */

extern void	TMname(char *, int);			/* cico.c */

extern int	cntrl(int);				/* cntrl.c */
extern int	startup(int);				/* cntrl.c */

extern int	processdev(char *[], char *[]);		/* callers.c */
extern int	gdial(char*, char *arps[], int);	/* callers.c */

extern char	*protoString(void);			/* conn.c */
extern int	conn(char *);				/* conn.c */
extern int	chat(int, char *[], int, char *, char *);
extern char	*fdig(char *);				/* conn.c */

extern void	chremdir(char *), mkremdir(char *);	/* chremdir.c */
extern void	toCorrupt(char *);			/* cpmv.c  */
extern int	xcp(char *, char *);			/* cpmv.c */
extern int	xmv(char *, char *);			/* cpmv.c  */
extern int	xfappend(FILE *, FILE *);		/* cpmv.c */
extern int	uidxcp(char *, char *);			/* cpmv.c */

extern int	expfile(char *);			/* expfile.c */
extern int	canPath(char *);			/* expfile.c */
extern int	mkdirs(char *);				/* expfile.c */
extern int	mkdirs2(char *, int);			/* expfile.c */
extern int	ckexpf(char *);				/* expfile.c */
extern void	gename(char, char *, char, char *);	/* gename.c */
extern void	initSeq(void);				/* gename.c */
extern int	getargs(char*,char *arps[],int);	/* getargs.c */
extern void	bsfix(char **);				/* getargs.c */
extern int	gninfo(char*, int *, char *);		/* getpwinfo.c */
extern int	guinfo(int, char *);			/* getpwinfo.c */
extern char	*getprm(char *, char *, char *);	/* getprm.c */
extern int	charType(char);				/* getprm.c */
extern int	split(char *, char *, char *);		/* getprm.c */
extern int	gnamef(DIR *, char *);			/* gnamef.c */
extern int	gdirf(DIR *, char *, char *);		/* gnamef.c */

extern int	gnxseq(char *);				/* gnxseq.c */
extern int	cmtseq(void);				/* gnxseq.c */
extern void	ulkseq(void);				/* gnxseq.c */

extern void	svcfile(char *, char *);		/* gtcfile.c */
extern void	wfcommit(char *, char *, char *);	/* gtcfile.c */
extern void	wfabort(void);				/* gtcfile.c */
extern int	gtcfile(char *, char *);		/* gtcfile.c */
extern void	commitall(void);			/* gtcfile.c */
extern int	gwd(char *);				/* gwd.c */
extern int	uidstat(char *, struct stat *);		/* gwd.c */

extern int	interface(char *);			/* interface.c */

extern void	setline(char);				/* line.c */
extern void	fixline(int, int, int);			/* line.c */
extern int	savline(void);				/* line.c */
extern int	restline(void);				/* line.c */
extern void	ttygenbrk(int);				/* line.c */
extern void	sethup(int);				/* line.c */

extern void	logent(char *, char*);			/* logent.c */
extern void	uusyslog(char *), uucloselog(void);	/* logent.c */
extern time_t	millitick(void);			/* logent.c */

extern void	mailst(char *, char *, char *, char *);	/* mailst.c */
extern void	setuucp(char *);			/* mailst.c */

extern int	imsg(char *, int);			/* imsg.c */
extern int	omsg(char, char *, int);		/* imsg.c */

extern int	logFind(char *, char*);			/* permission.c */
extern int	mchFind(char *);			/* permission.c */
extern int	chkperm(char *, char *, char *);	/* permission.c */
extern int	chkpth(char *, int);			/* permission.c */
extern int	cmdOK(char *, char *);			/* permission.c */
extern int	switchRole(void);			/* permission.c */
extern int	callBack(void), requestOK(void);	/* permission.c */
extern void	myName(char *);				/* permission.c */

extern int	shio(char *, char *, char *, char *);	/* shio.c */

extern void	statlog(char *, unsigned long, time_t);	/* statlog.c */

extern int	eaccess(char *, int);			/* strsave.c */
extern char	*strsave(char *);			/* strsave.c */

extern void	systat(char*, int, char *, long);	/* systat.c */
extern int	callok(char *);				/* systat.c */

extern void	sysreset(void);				/* sysfiles.c */
extern void	devreset(void);				/* sysfiles.c */
extern void	dialreset(void);			/* sysfiles.c */
extern int	getsysline(char *, int);		/* sysfiles.c */
extern int	getdevline(char *, int);		/* sysfiles.c */
extern int	getdialline(char *, int);		/* sysfiles.c */
extern void	setservice(char *);			/* sysfiles.c */
extern void	setdevcfg(char *, char *);		/* sysfiles.c */
extern int	sysaccess(int);				/* sysfiles.c */

extern int	ulockf(char *, time_t);			/* ulockf.c */
extern int	checkLock(char *);			/* ulockf.c */
extern void	delock(char *);				/* ulockf.c */
extern void	stlock(char *);				/* ulockf.c */
#ifdef sgi /* fix ld warnings */
#define mlock mmlock
#endif
extern int	mmlock(char*);				/* ulockf.c */
extern int	lockname(char*,char*,int);		/* ulockf.c */
extern void	rmlock(char*), ultouch(void);		/* ulockf.c */
extern int	filelock(int);				/* ulockf.c */

extern char	*timeStamp(void);			/* utility.c */
extern void	assert(char *, char *, int, char *, int);   /* utility.c */
extern void	errent(char *, char *, int, char *, int);   /* utility.c */
extern void	uucpname(char *);			/* uucpname.c */
extern int	versys(char *);				/* versys.c */
extern void	xuuxqt(char *), xuucico(char *);	/* xqt.c */

extern void pkfail(void);
extern int gturnon(void), gturnoff(void);
extern int grdmsg(char*, int), grddata(int, FILE *);
extern int gwrmsg(char, char*, int), gwrdata(FILE *, int);
extern int dturnon(void), dturnoff(void);
extern int drdmsg(char*, int), drddata(int, FILE *);
extern int dwrmsg(char, char*, int), dwrdata(FILE *, int);
extern int xturnon(void), xturnoff(void);
extern int xrdmsg(char*, int), xrddata(int, FILE *);
extern int xwrmsg(char, char*, int), xwrdata(FILE *, int);
extern int eturnon(void), eturnoff(void);
extern int erdmsg(char*, int), erddata(int, FILE *);
extern int ewrmsg(char, char*, int), ewrdata(FILE *, int);
extern int tturnon(void), tturnoff(void);
extern int trdmsg(char*, int), trddata(int, FILE *);
extern int twrmsg(char, char*, int), twrdata(FILE *, int);


#ifdef ATTSV
#ifndef sgi
unsigned	sleep();
void	exit(), setbuf();
long	ulimit();
#endif /* sgi */
#else /* !ATTSV */
int	sleep(), exit(), setbuf(), ftime();
#endif

#ifndef NOUSTAT
#ifdef V7USTAT
struct  ustat {
	daddr_t	f_tfree;	/* total free */
	ino_t	f_tinode;	/* total inodes free */
};
#else /* !NOUSTAT && !V7USTAT */
#include <ustat.h>
#endif /* V7USTAT */
#endif /* NOUSTAT */

#ifdef UNAME
#include <sys/utsname.h>
#endif /* UNAME */

#ifdef BSD4_2
char *gethostname();
#endif /* BSD4_2 */

/* messages */
extern char *Ct_OPEN;
extern char *Ct_WRITE;
extern char *Ct_READ;
extern char *Ct_CREATE;
extern char *Ct_ALLOCATE;
extern char *Ct_LOCK;
extern char *Ct_STAT;
extern char *Ct_CHOWN;
extern char *Ct_CHMOD;
extern char *Ct_LINK;
extern char *Ct_CHDIR;
extern char *Ct_UNLINK;
extern char *Wr_ROLE;
extern char *Ct_CORRUPT;
extern char *Ct_FORK;
extern char *Ct_CLOSE;
extern char *Fl_EXISTS;
extern char *Ue_BADSYSTEM;
extern char *Ue_TIME_WRONG;
extern char *Ue_SYSTEM_LOCKED;
extern char *Ue_NO_DEVICE;
extern char *Ue_DIAL_FAILED;
extern char *Ue_LOGIN_FAILED;
extern char *Ue_SEQBAD;
extern char *Ue_BAD_LOG_MCH;
extern char *Ue_WRONG_MCH;
extern char *Ue_LOCKED_DEVICE;
extern char *Ue_ASSERT_ERROR;
extern char *Ue_CANT_ACCESS_DEVICE;
extern char *Ue_DEVICE_FAILED;
