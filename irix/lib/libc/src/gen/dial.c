/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.27 $"
/*LINTLIBRARY*/
/***************************************************************
 *      dial() returns an fd for an open tty-line connected to the
 *      specified remote.  The caller should trap all ways to
 *      terminate, and call undial(). This will release the `lock'
 *      file and return the outgoing line to the system.  This routine
 *      would prefer that the calling routine not use the `alarm()'
 *      system call, nor issue a `signal(SIGALRM, xxx)' call.
 *      If you must, then please save and restore the alarm times.
 *      The sleep() library routine is ok, though.
 *
 *	#include <sys/types.h>
 *	#include <sys/stat.h>
 *      #include "dial.h"
 *
 *      int dial(call);
 *      CALL call;
 *
 *      void undial(rlfd);
 *      int rlfd;
 *
 *      rlfd is the "remote-lne file descriptor" returned from dial.
 *
 *      The CALL structure as (defined in dial.h):
 *
 *      typedef struct {
 *              struct termio *attr;    ptr to term attribute structure
 *              int     baud;           transmission baud-rate
 *              int     speed;          212A modem: low=300, high=1200
 *					negative for "Any" speed
 *              char    *line;          device name for out-going line
 *              char    *telno;         ptr to tel-no digit string
 *		char 	*device		no longer used --
 *					left in for backwards compatibility
 *		int	dev_len		no longer used --
 *					left in for backwards compatibility
 *      } CALL;
 *
 *      The error returns from dial are negative, in the range -1
 *      to -13, and their meanings are:
 *
 *              INTRPT   -1: interrupt occured
 *              D_HUNG   -2: dialer hung (no return from write)
 *              NO_ANS   -3: no answer within 20 seconds
 *              ILL_BD   -4: illegal baud-rate
 *              A_PROB   -5: acu problem (open() failure)
 *              L_PROB   -6: line problem (open() failure)
 *              NO_Ldv   -7: can't open L-devs file
 *              DV_NT_A  -8: specified device not available
 *              DV_NT_K  -9: specified device not known
 *              NO_BD_A -10: no device available at requested baud-rate
 *              NO_BD_K -11: no device known at requested baud-rate
 *		DV_NT_E -12: requested speed does not match
 *		BAD_SYS -13: system not in Systems file
 *
 *      Setting attributes in the termio structure indicated in
 *      the `attr' field of the CALL structure before passing the
 *      structure to dial(), will cause those attributes to be set
 *      before the connection is made.  This can be important for
 *      some attributes such as parity and baud.
 *
 *      With an error return (negative value), there will not be
 *      any `lock-file' entry, so no need to call undial().
 ***************************************************************/

#ifdef __STDC__
	#pragma weak dial = _dial
	#pragma weak undial = _undial
#endif

#include "synonyms.h"

#include <stdio.h>
#include <sys/types.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/times.h>
#ifdef sgi
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#endif /* sgi */

#include "dial.h"

#define DIAL	1

/*	these #define's from uucp.h needed for dial().	*/
#define	SUCCESS	0
#define	FAIL	-1

#define MAXBASENAME 14 /* should be DIRSIZ but 4.2bsd prohibits that */
#ifdef sgi
#define MAXFULLNAME 1024	/* 8K is too big; sys/nami.h too much hassle */
#else
#define MAXFULLNAME BUFSIZ
#endif /* sgi */
#define NAMESIZE MAXBASENAME+1
#define MAXNAMESIZE	64	/* /var/spool/uucp/<14 chars>/<14 chars>+slop */
#define SPOOL		"/var/spool/uucp"
#define PUBDIR		"/var/spool/uucppublic"

/* manifests for sysaccess()	*/
/* check file access for REAL user id */
#define	ACCESS_SYSTEMS	1
#define	ACCESS_DEVICES	2
#define	ACCESS_DIALERS	3
/* check file access for EFFECTIVE user id */
#define	EACCESS_SYSTEMS	4
#define	EACCESS_DEVICES	5
#define	EACCESS_DIALERS	6

/* structure of an Systems file line */
#define F_MAX	50	/* max number of fields in Systems file line */
#define F_NAME 0
#define F_TIME 1
#define F_TYPE 2
#define F_CLASS 3	/* an optional prefix and the speed */
#define F_PHONE 4
#define F_LOGIN 5

/* structure of an Devices file line */
#define D_MAX	50	/* max number of fields in Devices file line */
#define D_TYPE 0
#define D_LINE 1
#define D_CALLDEV 2
#define D_CLASS 3
#define D_CALLER 4
#define D_ARG 5

struct nstat {
	int	t_pid;		/* process id				*/
	long	t_start;	/* process id				*/
	time_t	t_beg;		/* start  time				*/
	time_t	t_scall;	/* start call to system			*/
	time_t	t_ecall;	/* end call to system			*/
	time_t	t_tacu;		/* acu time				*/
	time_t	t_tlog;		/* login time				*/
	time_t	t_sftp;		/* start file transfer protocol		*/
	time_t	t_sxf;		/* start xfer 				*/
	time_t	t_exf;		/* end xfer 				*/
	time_t	t_eftp;		/* end file transfer protocol		*/
	time_t	t_qtime;	/* time file queued			*/
	int	t_ndial;	/* # of dials				*/
	int	t_nlogs;	/* # of login trys			*/
	struct tms t_tbb;	/* start execution times		*/
	struct tms t_txfs;	/* xfer start times			*/
	struct tms t_txfe;	/* xfer end times 			*/
	struct tms t_tga;	/* garbage execution times		*/
};

#ifndef sgi
char *strcpy();
#endif

#define YES     1               /* mnemonic */
#define NO      0               /* mnemonic */

/*
 * to save room in the dso since most programs don't use dial - we make
 * a structure and dynamically allocate all this space
 */

struct dialinfo {
jmp_buf Sjbuf;	/* needed by uucp routines */
int
	Terminal,		/* flag; remote is a terminal */
	Oddflag,		/*flag- odd parity option*/
	Evenflag,		/*flag- even parity option*/
	Echoe,			/* save users ECHOE bit */
	Echok,			/* save users ECHOK bit */
	Child,			/* pid for receive process */
	Intrupt,		/* interrupt indicator */
	Duplex,		/* Unix= full duplex=YES; half = NO */ 
	Sstop,		/* NO means remote can't XON/XOFF */
	Rtn_code,		/* default return code */
	Takeflag;		/* indicates a ~%take is in progress */
int	Ifn;
int	Debug;
int	Uid, Euid;		/* user-id and effective-uid */
char	Progname[NAMESIZE];
char	Pchar;
char	Rmtname[MAXFULLNAME];
char	RemSpool[MAXFULLNAME];	/* spool subdirectory for remote system */
char	User[MAXFULLNAME];
char	Uucp[NAMESIZE];
char	Loginuser[NAMESIZE];
char	Myname[MAXBASENAME+1];
char	Wrkdir[MAXFULLNAME];
char	Logfile[MAXFULLNAME];
char	*Spool;
char	*Pubdir;
char	**Env;

long	Retrytime;
struct	nstat Nstat;
char	Dc[15];			/* line name				*/
int	Seqn;			/* sequence #				*/
int	Role;
char	*Bnptr;			/* used when BASENAME macro is expanded */
char	Jobid[NAMESIZE];	/* Jobid of current C. file */
int	Uerror;			/* global error code */

int	Verbose;	/* for cu and ct only */

/* used for READANY and READSOME macros */
struct stat __s_;
int rlfd;                   /* fd for remote comm line */

char *Myline;	/* flag to force the requested line to be used
			   by rddev() in conn.c */
};
static struct dialinfo *dialinfo = NULL;

/* make fewer source code changes .. */
#define Sjbuf		dialinfo->Sjbuf
#define Terminal	dialinfo->Terminal
#define Oddflag		dialinfo->Oddflag
#define Evenflag	dialinfo->Evenflag
#define Echoe		dialinfo->Echoe
#define Echok		dialinfo->Echok
#define Child		dialinfo->Child
#define Intrupt		dialinfo->Intrupt
#define Duplex		dialinfo->Duplex
#define Sstop		dialinfo->Sstop
#define Rtn_code	dialinfo->Rtn_code
#define Takeflag	dialinfo->Takeflag
#define	Ifn		dialinfo->Ifn
#define	Debug		dialinfo->Debug
#define	Uid		dialinfo->Uid
#define	Euid		dialinfo->Euid
#define	Progname	dialinfo->Progname
#define	Pchar		dialinfo->Pchar
#define	Rmtname		dialinfo->Rmtname
#define	RemSpool	dialinfo->RemSpool
#define	User		dialinfo->User
#define	Uucp		dialinfo->Uucp
#define	Loginuser	dialinfo->Loginuser
#define	Myname		dialinfo->Myname
#define	Wrkdir		dialinfo->Wrkdir
#define	Logfile		dialinfo->Logfile
#define	Spool		dialinfo->Spool
#define	Pubdir		dialinfo->Pubdir
#define	Env		dialinfo->Env
#define	Retrytime	dialinfo->Retrytime
#define	Nstat		dialinfo->Nstat
#define	Dc		dialinfo->Dc
#define	Seqn		dialinfo->Seqn
#define	Role		dialinfo->Role
#define	Bnptr		dialinfo->Bnptr
#define	Jobid		dialinfo->Jobid
#define	Uerror		dialinfo->Uerror
#define	Verbose		dialinfo->Verbose
/* used for READANY and READSOME macros */
#define __s_		dialinfo->__s_
#define rlfd		dialinfo->rlfd
#define Myline		dialinfo->Myline

static const char	*P_PARITY = "Parity option error\r\n";

static const char UerrorText[][32] = {
  /* SS_OK			0 */ "SUCCESSFUL",
  /* SS_NO_DEVICE		1 */ "NO DEVICES AVAILABLE",
  /* SS_TIME_WRONG		2 */ "WRONG TIME TO CALL",
  /* SS_INPROGRESS		3 */ "TALKING",
  /* SS_CONVERSATION		4 */ "CONVERSATION FAILED",
  /* SS_SEQBAD			5 */ "BAD SEQUENCE CHECK",
  /* SS_LOGIN_FAILED		6 */ "LOGIN FAILED",
  /* SS_DIAL_FAILED		7 */ "DIAL FAILED",
  /* SS_BAD_LOG_MCH		8 */ "BAD LOGIN/MACHINE COMBINATION",
  /* SS_LOCKED_DEVICE		9 */ "DEVICE LOCKED",
  /* SS_ASSERT_ERROR		10 */ "ASSERT ERROR",
  /* SS_BADSYSTEM		11 */ "SYSTEM NOT IN Systems FILE",
  /* SS_CANT_ACCESS_DEVICE	12 */ "CAN'T ACCESS DEVICE",
  /* SS_DEVICE_FAILED		13 */ "DEVICE FAILED",
  /* SS_WRONG_MCH		14 */ "WRONG MACHINE NAME",
  /* SS_CALLBACK		15 */ "CALLBACK REQUIRED",
  /* SS_RLOCKED			16 */ "REMOTE HAS A LCK FILE FOR ME",
  /* SS_RUNKNOWN		17 */ "REMOTE DOES NOT KNOW ME",
  /* SS_RLOGIN			18 */ "REMOTE REJECT AFTER LOGIN",
  /* SS_UNKNOWN_RESPONSE	19 */ "REMOTE REJECT, UNKNOWN MESSAGE",
  /* SS_STARTUP			20 */ "STARTUP FAILED",
  /* SS_CHAT_FAILED		21 */ "CALLER SCRIPT FAILED",
};

#ifndef sgi
        alarm();
#endif /* !sgi */


static int	finds(char *sysnam, char *flds[], int fldcount);
static int	cleanup(int code, int Cn);
static int	interface(char *label);
static int	conn(char *system);
static int	getto(char *flds[]);
static int	rddev(char *type, char *dev[], char *buf, int devcount);
static int	classmatch(char *flds[], char *dev[]);
static int	processdev(char *flds[], char *dev[]);
static int	getargs(char *s, char *arps[], int count);
static void	bsfix(char **args);
static char	*repphone(char *arg, char *phone, char *trstr);
static int	mlock(char *name);
static int	filelock(int fd);
static int	savline(void);
static void	fixline(int tty, int spwant, int type);
static int	restline(void);
static void	sethup(int dcf);
static int	usetup(int role, int *fdreadp, int *fdwritep);
static int	uteardown(int role, int fdreadp, int fdwritep);
#ifdef TLI
static int	tsetup(int role, int *fdreadp, int *fdwritep);
#endif /* TLI */
#ifdef TLIS
static int	tssetup(int role, int *fdreadp, int *fdwritep);
#endif /* TLIS */
static int	gdial(char *type, char *arps[], int narps);
static void	exphone(char *in, char *out);
static void	translate(char *ttab, char *str);
static int	chat(int nf, char *flds[], int fn, char *phstr1, char *phstr2);
static void	delock(char *s);
#ifdef TLI
static int	tlicall(char *flds[], char *dev[]);
static int	tfaillog(int fd, char *s);
static int	show_tlook(int fd);
static struct nebuf *stoa(char *str, struct nebuf *addr);
static int	dobase(char *s, char *buf, int type);
static void	memcp(char *d, char *s, int n);
static char	*xfer(char *dest, char *src, unsigned len, unsigned max);
#endif /* TLI */
static		int open801(char *dcname, char *dnname, char *phone, int speed);
#ifdef TCP
static int	tcpcall(char *flds[], char *dev[]);
#endif /* TCP */
static int	ulockf(char *file, time_t atime);
#ifdef sgi
static void	alarmtr(void);
#else /* sgi */
static int	alarmtr(void);
#endif /* sgi */
static int	expect(char *str, int fn);
static void	sendthem(char *str, int fn, char *phstr1, char *phstr2);
static void	rmlock(char *name);
static int	onelock(char *pid, char *tempfile, char *name);
static int	checkLock(char *file);
static void	stlock(char *name);
static int	notin(char *sh, char *lg);
static int	wrstr(int fn, char *buf, int len, int echocheck);
static int	wrchr(int fn, char *buf, int len);
static char	*fdig(char *cp);
static void	genbrk(int fn);
#ifdef TLI
static int	twrite(int fd, char *buf, unsigned nbytes);
static int	tread(int fd, char *buf, unsigned nbytes);
static int	tioctl(int fd, int request, int arg);
#endif /* TLI */
static void	nap(unsigned time);
static void	setservice(char *service);
static void	setdevcfg(char *service, char *device);
static int	sysaccess(int type);
static void	scansys(char *service);
static void	scancfg(char *service, char *device);
static int	getline(FILE *f, char *line);
static int	namematch(char *label, char *line, char *name);
static void	nameparse(void);
static void	setfile(char **type, char *line);
static void	setioctl(char **type, char *line);
static void	sysreset(void);
static void	devreset(void);
static void	dialreset(void);
static int	getsysline(char *buf, int len);
static int	nextsystems(void);
static int	getdevline(char *buf, int len);
static int	nextdevices(void);
static int	getdialline(char *buf, int len);
static int	nextdialers(void);
static int	eaccess(char *path, int amode);
static char	*strsave(char *str);

#ifdef TLIS
static int	getpop(char *buf, int len, int *optional);
static int	getpush(char *buf, int len);
#endif



int
dial(CALL call)
{
char *alt[7];
char speed[10];		/* character value of speed passed to dial */
int systemname = 0;

	if (dialinfo == NULL) {
		if ((dialinfo = calloc(1, sizeof(*dialinfo))) == NULL)
			return(DV_NT_A);
		Duplex=YES;
		Sstop=YES;
		Spool = SPOOL;
		Pubdir = PUBDIR;
	}
	/* set service so we know which Sysfiles entries to use, then	*/
	/* be sure can access Devices file(s).  use "cu" entries ...	*/
	/* dial is more like cu than like uucico.			*/
	(void)strcpy(Progname,"cu");
	setservice(Progname);
	if ( sysaccess(EACCESS_DEVICES) != 0 ) {
		/* can't read Devices file(s)	*/
		return(NO_Ldv);
	}

	if (call.speed <= 0) strcpy(speed,"Any");
	else sprintf(speed,"%d",call.speed);
/* Determine whether contents of telno is a phone number or a system name */
	if(strlen(call.telno) != strspn(call.telno,"0123456789=-*#"))
		systemname++;
/* If dial() was given a system name use "conn()" */
	if(systemname)
		rlfd = conn(call.telno);
	else {
		alt[F_NAME] = "dummy";	/* to replace the Systems file fields */

		alt[F_TIME] = "Any";    /* needed for getto(); [F_TYPE] and */
		alt[F_TYPE] = "";	/* [F_PHONE] assignment below       */
		alt[F_CLASS] = speed;
		alt[F_PHONE] = "";
		alt[F_LOGIN] = "";
		alt[6] = "";

		/* If device specified: if is "/dev/device", strip off	*/
		/* "/dev/" because must	exactly match entries in Devices*/
		/* file, which usually omit the "/dev/".  if doesn't	*/
		/* begin with "/dev/", leave it as it is.		*/
		if(call.line != NULL) {
			if ( strncmp(call.line, "/dev/", 5) == 0 )
				Myline = (call.line + 5);
			else
				Myline = call.line;
		}
	
		/* If telno specified */
		if(call.telno != NULL) {
			alt[F_PHONE] = call.telno;
			alt[F_TYPE] = "ACU";
		}
		/* If telno is NULL, it is a direct line */
		else {
			alt[F_TYPE] = "Direct";
		}
	
#ifdef forfutureuse
		if (call->class != NULL)
			alt[F_TYPE] = call->class;
#endif
	
	
		rlfd = getto(alt);
	}
	if (rlfd < 0)
		switch (Uerror) {
			case 1:	return(-10);
			case 7:	return(-2);
			case 9:	return(-8);
			case 11:	return(-13);
			case 12:	return(-6);
			case 21:	return(-3);
			default:	return(-Uerror);
		}
	(void)savline();
	if ((call.attr) && ioctl(rlfd, TCSETA, call.attr) < 0) {
		perror("stty for remote");
		return(L_PROB);
	}
	Euid = geteuid();
	if(setuid(getuid()) && setgid(getgid()) < 0)
		undial(rlfd);
	return(rlfd);
}

/*
* undial(fd) 
*/
void
undial(int fd)
{
	sethup(fd);
	sleep(2);
	cleanup(SUCCESS,fd);
}

/* ARGSUSED */
#ifdef sgi
static void
logent(const char *bunk1, char *bunk2){}	/* so we can load unlockf() */
#else
void
assert(){}	/* for ASSERT in conn() */
void
logent(){}	/* so we can load unlockf() */
#endif /* sgi */

/* parameter "code" - Closes device; removes lock files */
/* parameter "Cn" -fd for remote comm line */

static int
cleanup(int code, int Cn) 	/*this is executed only in the parent process*/
{

	(void)restline();
	(void)setuid(Euid);
	if(Cn > 0) {
		(void)close(Cn);
	}


	rmlock((char*) NULL);	/*uucp routine in ulockf.c*/	
	return(code);		/* code=negative for signal causing disconnect*/
}
/**************system parameters	 from parms.h**************/

/* go through this carefully, configuring for your site */

/* If running SVR3, #define both ATTSVR3 and ATTSV */
#define ATTSVR3	/* System V Release 3 */

/* One of the following four lines should not be commented out.
 * The other three should be unless you are running a unique hybrid.
 */

#define	ATTSV	/* System III or System V */
/* #define	V7	* Version 7 systems (32V, Berkeley 4BSD, 4.1BSD) */
/* #define	BSD4_2	* Berkeley 4.2BSD */
/* #define	V8	* Research Eighth Edition */


/* Owner of setud files running on behalf of uucp.  Needed in case
 * root runs uucp and euid is not honored by kernel.
 * GID is needed for some chown() calls.
 * Also used if guinfo() cannot find the current users ID in the
 * password file.
 */
#define UUCPUID		5
#define UUCPGID		5

/* define ATTSVKILL if your system has a kill(2) that accepts kill(0, pid)
 * as a test for killability.  If ATTSV is defined this will automatically
 * be defined anyway.
 */
#define ATTSVKILL

/*
 * the next two lines control high resolution sleeps, called naps.
 *
 * most UNIX versions have no nap() system call; they want NONAP defined,
 * in which case one is provided in the code.  this includes all standard
 * versions of UNIX.
 *
 * some sites use a fast timer that reads a number of clock ticks and naps
 * for that interval; they want NONAP defined, and FASTTIMER defined as
 * the name of the device, e.g., /dev/ft.
 *
 * repeating, NONAP should be disabled *only* if your standard library has a
 * function called nap.
 */


/* #define NONAP	* nominal case -- no nap() in the standard library */
/* #define FASTTIMER "/dev/ft"   * identify the device used for naps */

#ifndef sgi
#if	defined ( ATTSVR3 ) && ! defined ( DIAL )

#define TLI		/* for AT&T Transport Layer Interface networks */
#define TLIS		/* for AT&T Transport Layer Interface networks */
			/* with streams module "tirdwr" */
#endif /* ATTSVR3, not from dial(3) */
#endif /* sgi */


#define DIAL801	/* 801/212-103 auto dialers */

/* #define X25 * define X25 if you want to use the xio protocol */

/* define DUMB_DN if your dn driver (801 acu) cannot handle '=' */
/* #define DUMB_DN */

/* define DEFAULT_BAUDRATE to be the baud rate you want to use when both */
/* Systems file and Devices file allow Any */
#define DEFAULT_BAUDRATE "9600"

/* NO_MODEM_CTRL - define this if you have very old hardware
 * that does not know how to correctly handle modem control
 * Some old pdp/11 hardware such as dk, dl
 * If you define this, and have DH devices for direct lines,
 * the ports will often hang and be unusable.
*/
/*#define NO_MODEM_CTRL	*/


/* define HZ to be the number of clock ticks per second */
/* #define HZ 60 * not needed for ATTSV or above */

/* define USRSPOOLLOCKS if you like your lock files in /usr/spool/locks
 * be sure other programs such as 'cu' and 'ct' know about this
 */
#define USRSPOOLLOCKS  /* define to use /usr/spool/locks for LCK files */

/**************#include's	from uucp.h**************/
/**************some omitted because are #include'd earlier for dial.c********/

#include <ctype.h>
#include <sys/param.h>
#ifndef sgi
#include <sys/dir.h>
#endif
#include <errno.h>

#ifndef sgi
#ifdef ATTSV
#include <unistd.h>
#include <sys/sysmacros.h>
#define ENOLCK	46	/* temp for running on bc */
#ifndef CDLIMIT
#include <sys/fmgr.h>
#endif /* !CDLIMIT */
#endif /* ATTSV */
#endif

#ifdef TLI
#include <tiuser.h>
#include <memory.h>
#include <malloc.h>
#include <nsaddr.h>
#ifdef TLIS
#include <sys/stropts.h>
#include <sys/conf.h>
#endif /* TLIS */
#endif /* TLI */


#ifdef BSD4_2
#define V7
#undef NONAP
#undef FASTTIMER
#include <sys/time.h>
#else /* !BSD4_2 */
#include <time.h>
#endif

#ifdef V8
#define V7
#endif /* V8 */

/*
 * param.h includes types.h and signal.h in 4bsd
 */
#ifdef V7
#include <sgtty.h>
#include <sys/timeb.h>
#else /* !V7 */
#include <signal.h>
#include <fcntl.h>
#endif

#ifdef	RT
#include "rt/types.h"
#include "rt/unix/param.h"
#include "rt/stat.h"
#include <sys/ustat.h>
#endif /* RT */

#ifdef FASTTIMER
#undef NONAP
#endif

/**************uucp constants	 from uucp.h**************/
#define MASTER 1
#define SLAVE 0
#define NULLCHAR	'\0'
#define SAME 0
#define CNULL (char *) 0
#define	SIZEOFPID	10		/* maximum number of digits in a pid */
#define EOTMSG "\004\n\004\n"
#define MAXEXPECTTIME 45
#define TRYCALLS	2	/* number of tries to dial call */
#define MAXPH	60	/* maximum phone string size */
#define D_ACU 1
#define D_DIRECT 2
#define D_PROT 4

/* how much of a system name can we print in a [CX]. file? */
/* MAXBASENAME - 1 (pre) - 1 ('.') - 1 (grade) - 4 (sequence number) */
#define SYSNSIZE (MAXBASENAME - 7)

#define DIE_UNLESS(e, reason, s1, i1) if (!(e)) {\
	cleanup(FAIL, -1);	\
	fprintf(stderr, "%s: %s %d", reason, s1, i1);  \
	abort();};
#define DIE_UNLESSS(e, reason, s1, s2) if (!(e)) {\
	cleanup(FAIL, -1);	\
	fprintf(stderr, "%s: %s %s", reason, s1, s1);  \
	abort();};


#define PREFIX(pre, str)	(strncmp((pre), (str), strlen(pre)) == SAME)
#define BASENAME(str, c) ((Bnptr = strrchr((str), c)) ? (Bnptr + 1) : (str))
#define EQUALS(a,b)	((a != CNULL) && (b != CNULL) && (strcmp((a),(b))==SAME))
#define EQUALSN(a,b,n)	((a != CNULL) && (b != CNULL) && (strncmp((a),(b),(n))==SAME))

/* define the last characters for ACU  (used for 801/212 dialers) */
#define ACULAST "<"

#define DIALCODES	"/etc/uucp/Dialcodes"
#define MAXLOCKS 10	/* maximum number of lock files */
#define	MAX_LOCKTRY	5	/* number of attempts to lock device */
#ifdef USRSPOOLLOCKS
#define LOCKPRE		"/var/spool/locks/LCK."
#define X_LOCKDIR	"/var/spool/locks"	/* must be dir part of above */
#else
#define LOCKPRE		"/var/spool/uucp/LCK."
#define X_LOCKDIR	"/var/spool/uucp"	/* must be dir part of above */
#endif /* USRSPOOLLOCKS */
#define SLCKTIME	5400	/* system/device timeout (LCK.. files) */

#define UERRORTEXT		UerrorText[Uerror]

/* default administrative files */
#define SYSDIR		"/etc/uucp"
#define SYSFILES	"/etc/uucp/Sysfiles"
#define SYSTEMS		"/etc/uucp/Systems"
#define DEVICES		"/etc/uucp/Devices"
#define DIALERS		"/etc/uucp/Dialers"
#define	DEVCONFIG	"/etc/uucp/Devconfig"

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

#ifdef ATTSV
#ifndef sgi
#define index strchr
#define rindex strrchr 
#endif
#ifdef sgi
#define _vfork _fork
#else
#define vfork fork
#endif
#define ATTSVKILL
#define UNAME
#else
#define strchr index
#define strrchr rindex
#endif


/*********** structure definitions *********/
/* This structure describes call routines */
struct caller {
	char	*CA_type;
	int	(*CA_caller)(char *[], char *[]);
};

/*********** standard functions used **********/

#ifdef sgi
#include <time.h>
#include <sys/times.h>
#include <utime.h>
#else
extern char	*strcat(), *strncpy(), *strrchr();
extern char	*strchr(), *strpbrk();
extern int	strlen(), strcmp(), strncmp();
extern char	*index(), *rindex();
extern char	*calloc();
extern char 	*malloc();
extern long	times(), lseek(), atol();
extern time_t	time();
extern struct tm	*localtime();
extern FILE	*popen();

extern char 	*getlogin(), *ttyname();
extern int	execle(), fork(), pipe(), close(), getopt();
extern int fcntl(); 
#endif /* !sgi */
#ifdef BSD4_2
extern char *sprintf();
#else
#ifndef sgi
extern int sprintf();
#endif
#endif /* BSD4_2 */

/*
 * CDEBUG is for communication line debugging 
 * DEBUG is for program debugging 
 * #define SMALL to compile without the DEBUG code
 */
#define CDEBUG(l, f, s) if (Debug >= l) fprintf(stderr, f, s)
#define CDEBUG0(l, f) if (Debug >= l) fprintf(stderr, f)
#ifndef SMALL
#define DEBUG(l, f, s) if (Debug >= l) fprintf(stderr, f, s)
#define DEBUG0(l, f) if (Debug >= l) fprintf(stderr, f)
#else
#define DEBUG(l, f, s)
#define DEBUG0(l, f, s)
#endif /* SMALL */

/*
 * VERBOSE is used by cu and ct to inform the user
 * about the progress of connection attempts.
 * In uucp, this will be NULL.
 */

#ifdef STANDALONE
#define VERBOSE(f, s) if (Verbose > 0) fprintf(stderr, f, s); else
#define VERBOSE0(f) if (Verbose > 0) fprintf(stderr, f); else
#else
#define VERBOSE(f, s)
#define VERBOSE0(f)
#endif

/*********** external variable declarations ***********/
#ifdef sgi
static char **Lockfile = NULL;
#else  /* sgi */
static char *Lockfile[MAXLOCKS];
#endif /* sgi */
static struct termio *Savettyb = NULL;
static int Nlocks = 0;
/********interface()	uucp:interface.c	1.9********/

#ident	"@(#)uucp:interface.c	1.9"
/*	interface( label )
	provide alternate definitions for the I/O functions through global
	interfaces.
*/

#ifdef DATAKIT
static int	dkteardown();
#endif	/* DATAKIT */

#ifndef sgi
extern int	read(), write(), ioctl();
#endif /* !sgi */

static ssize_t	(*Read)(int, void *, size_t) = read,
	(*Write)(int, const void *, size_t) = write;
#ifdef SYTEK
static int (*Ioctl)(int, int, ...) = ioctl,
	(*Teardown)(int, int, int) = uteardown;
#endif
static int (*Setup)(int, int *, int *) = usetup;

#ifdef TLI
static int	tread(), twrite(),	/* TLI i/o */
		tioctl(),		/* TLI i/o control */
		tsetup(),		/* TLI setup without streams module */
		tssetup(),		/* TLI setup with streams module */
		tteardown();		/* TLI teardown, works with either setup
					*/
#endif /*  TLI  */
/*	The IN_label in Interface[] imply different caller routines:
	e.g. tlicall().
	If so, the names here and the names in callers.c must match.
*/

static const
  struct Interface {
	char	*IN_label;		/* interface name */
	ssize_t	(*IN_read)(int, void *, size_t);	/* read function */
	ssize_t	(*IN_write)(int, const void *, size_t);/* write function */
	int	(*IN_ioctl)(int, int, ...);		/* ioctl function */
		/* setup function, called before first i/o operation */
	int	(*IN_setup)(int, int *, int *);
		/* teardown function, called after last i/o operation */
	int	(*IN_teardown)(int, int, int);
} Interface[] = {
			/* vanilla UNIX */
		{ "UNIX", read, write, ioctl, usetup, uteardown },
#ifdef sgi
		{ "ACU", read, write, ioctl, usetup, uteardown },
		{ "212", read, write, ioctl, usetup, uteardown },
#endif /* sgi */
#ifdef TLI
			/* AT&T Transport Interface Library WITHOUT streams */
		{ "TLI", tread, twrite, tioctl, tsetup, tteardown },
#ifdef TLIS
			/* AT&T Transport Interface Library WITH streams */
		{ "TLIS", read, write, tioctl, tssetup, uteardown },
#endif /*  TLIS  */
#endif /*  TLI  */
#ifdef DATAKIT
		{ "DK", read, write, ioctl, usetup, dkteardown },
#endif /* DATAKIT */
		{ 0, 0, 0, 0, 0, 0 }
	};


static int
interface( char *label )
{
	register int	i;

	for ( i = 0;  Interface[i].IN_label;  ++i ) {
		if( !strcmp( Interface[i].IN_label, label ) ) {
			Read = Interface[i].IN_read;
			Write = Interface[i].IN_write;
			Setup = Interface[i].IN_setup;
#ifdef SYTEK
			Ioctl = Interface[i].IN_ioctl;
			Teardown = Interface[i].IN_teardown;
#endif
			DEBUG(5, "set interface %s\n", label);
			return( 0 );
		}
	}
	return( FAIL );
}
/********conn()	uucp:conn.c	2.13********/
/*
 * conn - place a telephone call to system and login, etc.
 *
 * return codes:
 *	FAIL - connection failed
 *	>0  - file no.  -  connect ok
 * When a failure occurs, Uerror is set.
 */

static int
conn(char *system)
{
	int fn = FAIL;
	char *flds[F_MAX+1];

	CDEBUG(4, "conn(%s)\n", system);
	Uerror = 0;
	while ( finds(system, flds, F_MAX) > 0 ) {
		fn = getto(flds);
		CDEBUG(4, "getto ret %d\n", fn);
		if (fn < 0)
		    continue;

		sysreset();
		return(fn);
	}

	/* finds or getto failed */
	sysreset();
	CDEBUG(1, "Call Failed: %s\n", UERRORTEXT);
	return(FAIL);
}
/********finds()	uucp:conn.c	2.13********/
/*
 * finds	- set system attribute vector
 *
 * input:
 *	fsys - open Systems file descriptor
 *	sysnam - system name to find
 * output:
 *	flds - attibute vector from Systems file
 *	fldcount - number of fields in flds
 * return codes:
 *	>0  -  number of arguments in vector - succeeded
 *	FAIL - failed
 * Uerror set:
 *	0 - found a line in Systems file
 *	SS_BADSYSTEM - no line found in Systems file
 *	SS_TIME_WRONG - wrong time to call
 */

static int
finds(char *sysnam, char *flds[], int fldcount)
{
#ifdef sgi
	static char *info = NULL;
#else  /* sgi */
	static char info[BUFSIZ];
#endif /* sgi */
	int na;

	/* format of fields
	 *	0 name;
	 *	1 time
	 *	2 acu/hardwired
	 *	3 speed
	 *	etc
	 */
	if (sysnam == 0 || *sysnam == 0 ) {
		Uerror = SS_BADSYSTEM;
		return(FAIL);
	}

#ifdef sgi
	if ((info == NULL) && (info = calloc(1, BUFSIZ)) == NULL)
		return (FAIL);
#endif /* sgi */

	while (getsysline(info, BUFSIZ)) {
		if (*sysnam != *info || *info == '#')	/* speedup */
			continue;
		na = getargs(info, flds, fldcount);
		bsfix(flds);	/* replace \X fields */
		if ( !EQUALSN(sysnam, flds[F_NAME], SYSNSIZE))
			continue;
		Uerror = 0;
		return(na);	/* FOUND OK LINE */
	}
	if (!Uerror)
		Uerror = SS_BADSYSTEM;
	return(FAIL);
}
/********getto()	uucp:conn.c	2.13********/
/*
 * getto - connect to remote machine
 *
 * return codes:
 *	>0  -  file number - ok
 *	FAIL  -  failed
 */

static int
getto(char *flds[])
{
	char *dev[D_MAX+2], devbuf[BUFSIZ];
	register int status;
	register int dcf = -1;
	int reread = 0;
	int tries = 0;	/* count of call attempts - for limit purposes */

	CDEBUG(1, "Device Type %s wanted\n", flds[F_TYPE]);
	Uerror = 0;
	while (tries < TRYCALLS) {
		if ((status=rddev(flds[F_TYPE], dev, devbuf, D_MAX)) == FAIL) {
			if (tries == 0 || ++reread >= TRYCALLS)
				break;
			devreset();
			continue;
		}
		/* check class, check (and possibly set) speed */
		if (classmatch(flds, dev) != SUCCESS)
			continue;
		if ((dcf = processdev(flds, dev)) >= 0)
			break;

		switch(Uerror) {
		case SS_CANT_ACCESS_DEVICE:
		case SS_DEVICE_FAILED:
		case SS_LOCKED_DEVICE:
			break;
		default:
			tries++;
			break;
		}
	}
	devreset();	/* reset devices file(s) */
	if (status == FAIL && !Uerror) {
		CDEBUG0(1, "Requested Device Type Not Found\n");
		Uerror = SS_NO_DEVICE;
	}
	return(dcf);
}
/********rddev()	uucp:conn.c	2.13********/
/***
 *	rddev - find and unpack a line from device file for this caller type 
 *	lines starting with whitespace of '#' are comments
 *
 *	return codes:
 *		>0  -  number of arguments in vector - succeeded
 *		FAIL - EOF
 ***/

static int
rddev(char *type, char *dev[], char *buf, int devcount)
{
	char *commap;
	int na;

	while (getdevline(buf, BUFSIZ)) {
		if (buf[0] == ' ' || buf[0] == '\t'
		    ||  buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#')
			continue;
		na = getargs(buf, dev, devcount);
		DIE_UNLESS(na >= D_CALLER, "BAD LINE", buf, na);

		if ( strncmp(dev[D_LINE],"/dev/",5) == 0 ) {
			/* since cu (altconn()) strips off leading */
			/* "/dev/",  do the same here.  */
			strcpy(dev[D_LINE], &(dev[D_LINE][5]) );
		}

/* For cu -- to force the requested line to be used */
		if ((Myline != NULL) && (!EQUALS(Myline, dev[D_LINE])) )
		    continue;

		bsfix(dev);	/* replace \X fields */

		/*
		 * D_TYPE field may have protocol subfield, which
		 * must be pulled off before comparing to desired type.
		 */
		if ((commap = strchr(dev[D_TYPE], ',')) != (char *)NULL )
			*commap = '\0';
		if (EQUALS(dev[D_TYPE], type))
			return(na);
	}
	return(FAIL);
}
/********classmatch()	uucp:conn.c	2.13********/
/*
 * classmatch - process 'Any' in Devices and Systems and
 * 	determine the correct speed, or match for ==
 */

static int
classmatch(char *flds[], char *dev[])
{
	/* check class, check (and possibly set) speed */
	if (EQUALS(flds[F_CLASS], "Any")
	   && EQUALS(dev[D_CLASS], "Any")) {
		dev[D_CLASS] = DEFAULT_BAUDRATE;
		return(SUCCESS);
	} else if (EQUALS(dev[F_CLASS], "Any")) {
		dev[D_CLASS] = flds[F_CLASS];
		return(SUCCESS);
	} else if (EQUALS(flds[F_CLASS], "Any") ||
	EQUALS(flds[F_CLASS], dev[D_CLASS]))
		return(SUCCESS);
	else
		return(FAIL);
}
/********processdev()	uucp:callers.c	2.18********/
/*
 *	to add a new caller:
 *	declare the function that knows how to call on the device,
 *	add a line to the callers table giving the name of the device
 *	(from Devices file) and the name of the function
 *	add the function to the end of this file
 */

#ifdef DIAL801
static int dial801(char *[], char *[]);
#endif

#ifdef DATAKIT
static int	dkcall();
#endif /* DATAKIT */

#ifdef V8
static int	Dialout();
#endif

#ifdef TCP
static int	unetcall();
static int	tcpcall();
#endif /* TCP */

#ifdef SYTEK
static int	sytcall();
#endif /* SYTEK */

#ifdef TLI
static int	tlicall();
#endif /* TLI */

static const struct caller Caller[] = {

#ifdef DIAL801
	{"801",		dial801},
	{"212",		dial801},
#endif /* DIAL801 */

#ifdef V8
	{"Dialout",	Dialout},	/* ditto but using dialout(III) */
#endif

#ifdef TCP
#ifdef BSD4_2
	{"TCP",		tcpcall},	/* 4.2BSD sockets */
#else /* !BSD4_2 */
#ifdef UNET
	{"TCP",		unetcall},	/* 3com implementation of tcp */
	{"Unetserver",	unetcall},
#endif /* UNET */
#endif /* BSD4_2 */
#endif /* TCP */

#ifdef DATAKIT
	{"DK",		dkcall},	/* standard AT&T DATAKIT VCS caller */
#endif /* DATAKIT */

#ifdef SYTEK
	{"Sytek",	sytcall},	/* untested but should work */
#endif /* SYTEK */

#ifdef TLI
	{"TLI",		tlicall},	/* AT&T Transport Layer Interface */
#ifdef TLIS
	{"TLIS",	tlicall},	/* AT&T Transport Layer Interface */
#endif /*  TLIS  */
#endif /* TLI */

	{NULL, 		NULL}		/* this line must be last */
};


/*
 * processdev - Process a line from the Devices file
 *
 * return codes:
 *	file descriptor  -  succeeded
 *	FAIL  -  failed
 */

static int
processdev(register char *flds[], register char *dev[])
{
	int dcf = -1;
	const struct caller	*ca;
	char *args[D_MAX+1], dcname[20];
	register char **sdev;
	register int nullfd;
	char *phonecl;			/* clear phone string */
	char phoneex[2*(MAXPH+2)];	/* expanded phone string */

	sdev = dev;
	for (ca = Caller; ca->CA_type != NULL; ca++) {
		/* This will find built-in caller functions */
		if (EQUALS(ca->CA_type, dev[D_CALLER])) {
			DEBUG(5, "Internal caller type %s\n", dev[D_CALLER]);
			if (dev[D_ARG] == NULL) {
				/* if NULL - assume translate */
				dev[D_ARG+1] = NULL;	/* needed for for loop later to mark the end */
				dev[D_ARG] = "\\T";
			}
			dev[D_ARG] = repphone(dev[D_ARG], flds[F_PHONE], "");
			if ((dcf = (*(ca->CA_caller))(flds, dev)) < 0)
				return(dcf) ;
			if ( interface( ca->CA_type ) ) {
				DEBUG(5, "interface(%s) failed", ca->CA_type);
				Uerror = SS_DEVICE_FAILED;
				/*	restore vanilla unix interface	*/
				(void)interface("UNIX");
				return(FAIL);
			}
			dev += 2; /* Skip to next CALLER and ARG */
			break;
		}
	}
	if (dcf == -1) {
		/* Here if not a built-in caller function	*/

		/* If "DIAL" is defined, the mlock will probably fail	*/
		/* (/usr/spool/locks is usually 755 owned by uucp), but	*/
		/* since are using advisory file locks anyway let it	*/
		/* slide.						*/
		if (mlock(dev[D_LINE]) == FAIL) { /* Lock the line */
			;
		}
		/*
		 * Open the line
		 */
		if ( *dev[D_LINE] != '/' ) {
			(void) sprintf(dcname, "/dev/%s", dev[D_LINE]);
		} else {
			(void) strcpy(dcname, dev[D_LINE] );
		}
		/* take care of the possible partial open fd */
		(void) close(nullfd = open("/", 0));
		if (setjmp(Sjbuf)) {
			(void) close(nullfd);
			DEBUG0(1, "generic open timeout\n");
			logent("generic open", "TIMEOUT");
			Uerror = SS_CANT_ACCESS_DEVICE;
			goto bad;
		}
		(void) signal(SIGALRM, alarmtr);
		(void) alarm(10);
		dcf = open(dcname, O_RDWR);
		(void) alarm(0);
		if (dcf < 0) {
			DEBUG(1, "generic open failed, errno = %d\n", errno);
			logent("generic open", "FAILED");
			Uerror = SS_CANT_ACCESS_DEVICE;
			(void) close(nullfd);
			goto bad;
		}
#ifdef ATTSV
		if ( filelock(dcf) != SUCCESS ) {
			(void)close(dcf);
			DEBUG(1, "failed to lock device %s\n", dcname);
			Uerror = SS_LOCKED_DEVICE;
			goto bad;
		}
#endif /* ATTSV */
		fixline(dcf, atoi(fdig(dev[D_CLASS])), D_DIRECT);
	}

	/*	init device config info	*/
	DEBUG(5, "processdev: calling setdevcfg(%s, ", Progname);
	DEBUG(5, "%s)\n", flds[F_TYPE]);
	setdevcfg(Progname, flds[F_TYPE]);

	if ( (*Setup)( MASTER, &dcf, &dcf ) ) {
		/*	any device|system lock files we should remove?	*/
		DEBUG0(5, "MASTER Setup failed");
		Uerror = SS_DEVICE_FAILED;
		goto bad;
	}
	/*
	 * Now loop through the remaining callers and chat
	 * according to scripts in dialers file.
	 */
	for (; dev[D_CALLER] != NULL; dev += 2) {
		register int w;
		/*
		 * Scan Dialers file to find an entry
		 */
		if ((w = gdial(dev[D_CALLER], args, D_MAX)) < 1) {
			DEBUG(1, "%s not found in Dialers file\n", dev[D_CALLER]);
			logent("generic call to gdial", "FAILED");
			Uerror = SS_CANT_ACCESS_DEVICE;
			goto bad;
		}
		if (w <= 2)	/* do nothing - no chat */
			break;
		/*
		 * Translate the phone number
		 */
		if (dev[D_ARG] == NULL) {
			/* if NULL - assume no translation */
			dev[D_ARG+1] = NULL; /* needed for for loop to mark the end */
			dev[D_ARG] = "\\D";
		}
		
		phonecl = repphone(dev[D_ARG], flds[F_PHONE], args[1]);
		exphone(phonecl, phoneex);
		translate(args[1], phoneex);
		/*
		 * Chat
		 */
		if (chat(w-2, &args[2], dcf, phonecl, phoneex) != SUCCESS) {
			Uerror = SS_CHAT_FAILED;
			goto bad;
		}
	}
	/*
	 * Success at last!
	 */
	strcpy(Dc, sdev[D_LINE]);
	return(dcf);
bad:
	if ( dcf >= 0 )
		(void)close(dcf);
	delock(sdev[D_LINE]);
	/*	restore vanilla unix interface	*/
	(void)interface("UNIX");
	return(FAIL);
}
/********getargs()	uucp:getargs.c	2.3********/
/*
 * generate a vector of pointers (arps) to the
 * substrings in string "s".
 * Each substring is separated by blanks and/or tabs.
 *	s	-> string to analyze -- s GETS MODIFIED
 *	arps	-> array of pointers -- count + 1 pointers
 *	count	-> max number of fields
 * returns:
 *	i	-> # of subfields
 *	arps[i] = NULL
 */

static int
getargs(register char *s, register char *arps[], register int count)
{
	register int i;

	for (i = 0; i < count; i++) {
		while (*s == ' ' || *s == '\t')
			*s++ = '\0';
		if (*s == '\n')
			*s = '\0';
		if (*s == '\0')
			break;
		arps[i] = s++;
		while (*s != '\0' && *s != ' '
			&& *s != '\t' && *s != '\n')
				s++;
	}
	arps[i] = NULL;
	return(i);
}
/********bsfix()	uucp:getargs.c	2.3********/

/***
 *      bsfix(args) - remove backslashes from args
 *
 *      \123 style strings are collapsed into a single character
 *	\000 gets mapped into \N for further processing downline.
 *      \ at end of string is removed
 *	\t gets replaced by a tab
 *	\n gets replaced by a newline
 *	\r gets replaced by a carriage return
 *	\b gets replaced by a backspace
 *	\s gets replaced by a blank 
 *	any other unknown \ sequence is left intact for further processing
 *	downline.
 */

static void
bsfix (char **args)
{
	register char *str, *to, *cp;
	register int num;

	for (; *args; args++) {
		str = *args;
		for (to = str; *str; str++) {
			if (*str == '\\') {
				if (str[1] == '\0')
					break;
				switch (*++str) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					for ( num = 0, cp = str
					    ; cp - str < 3
					    ; cp++
					    ) {
						if ('0' <= *cp && *cp <= '7') {
							num <<= 3;
							num += *cp - '0';
						}
						else
						    break;
					}
					if (num == 0) {
						*to++ = '\\';
						*to++ = 'N';
					} else
						*to++ = (char) num;
					str = cp-1;
					break;

				case 't':
					*to++ = '\t';
					break;

				case 's':	
					*to++ = ' ';
					break;

				case 'n':
					*to++ = '\n';
					break;

				case 'r':
					*to++ = '\r';
					break;

				case 'b':
					*to++ = '\b';
					break;

				default:
					*to++ = '\\';
					*to++ = *str;
					break;
				}
			}
			else
				*to++ = *str;
		}
		*to = '\0';
	}
}
/********repphone()	uucp:callers.c	2.18********/
/*
 * repphone - Replace \D and \T sequences in arg with phone
 * expanding and translating as appropriate.
 */
static char *
repphone(register char *arg, register char *phone, register char *trstr)
{
#ifdef sgi
	static char *pbuf = NULL;
#else  /* sgi */
	static char pbuf[2*(MAXPH+2)];
#endif /* sgi */
	register char *fp, *tp;

#ifdef sgi
	if ((pbuf == NULL) && (pbuf = calloc(1, 2*(MAXPH+2))) == NULL)
		return (NULL);
#endif /* sgi */

	for (tp=pbuf; *arg; arg++) {
		if (*arg != '\\') {
			*tp++ = *arg;
			continue;
		} else {
			switch (*(arg+1)) {
			case 'T':
				exphone(phone, tp);
				translate(trstr, tp);
				for(; *tp; tp++)
				    ;
				arg++;
				break;
			case 'D':
				for(fp=phone; *tp = *fp++; tp++)
				    ;
				arg++;
				break;
			default:
				*tp++ = *arg;
				break;
			}
		}
	}
	*tp = '\0';
	return(pbuf);
}
/********mlock()	uucp:ulockf.c	2.7********/
/*
 * create system or device lock
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */
static int
mlock(char *name)
{
	char lname[MAXNAMESIZE];

	/*
	 * if name has a '/' in it, then it's a device name and it's
	 * not in /dev (i.e., it's a remotely-mounted device or it's
	 * in a subdirectory of /dev).  in either case, creating our normal 
	 * lockfile (/usr/spool/locks/LCK..<dev>) is going to bomb if
	 * <dev> is "/remote/dev/tty14" or "/dev/net/foo/clone", so never 
	 * mind.  since we're using advisory filelocks on the devices 
	 * themselves, it'll be safe.
	 *
	 * of course, programs and people who are used to looking at the
	 * lockfiles to find out what's going on are going to be a trifle
	 * misled.  we really need to re-consider the lockfile naming structure
	 * to accomodate devices in directories other than /dev ... maybe in
	 * the next release.
	 */
	if ( strchr(name, '/') != NULL )
		return(0);
	(void) sprintf(lname, "%s.%s", LOCKPRE, name);
	BASENAME(lname, '/')[MAXBASENAME] = '\0';
	return(ulockf(lname, SLCKTIME) < 0 ? FAIL : 0);
}
/********filelock()	uucp:ulockf.c	2.7********/
#ifdef ATTSVR3

/*
 * filelock(fd)	sets advisory file lock on file descriptor fd
 *
 * returns SUCCESS if all's well; else returns value returned by fcntl()
 *
 * needed to supplement /usr/spool/locks/LCK..<device> because can now
 * have remotely-mounted devices using RFS.  without filelock(), uucico's
 * or cu's on 2 different machines could access the same remotely-mounted 
 * device, since /usr/spool/locks is purely local. 
 *		
 */

static int
filelock(int fd)
{
	register int lockrtn, locktry = 0;
	struct flock lck;

	lck.l_type = F_WRLCK;	/* setting a write lock */
	lck.l_whence = 0;	/* offset l_start from beginning of file */
	lck.l_start = 0L;
	lck.l_len = 0L;		/* until the end of the file address space */

	/*	place advisory file locks	*/
	while ( (lockrtn = fcntl(fd, F_SETLK, &lck)) < 0 ) {

		DEBUG(7, "filelock: F_SETLK returns %d\n", lockrtn);

		switch (lockrtn) {
#ifdef EACCESS
		case EACCESS:	/* already locked */
				/* no break */
#endif /* EACCESS */
		case EAGAIN:	/* already locked */
			if ( locktry++ < MAX_LOCKTRY ) {
				sleep(2);
				continue;
			}
			logent("filelock","F_SETLK failed - lock exists");
			return(lockrtn);
		case ENOLCK:	/* no more locks available */
			logent("filelock","F_SETLK failed -- ENOLCK");
			return(lockrtn);
		case EFAULT:	/* &lck is outside program address space */
			logent("filelock","F_SETLK failed -- EFAULT");
			return(lockrtn);
		default:
			logent("filelock","F_SETLK failed");
			return(lockrtn);
		}
	}
	DEBUG0(7, "filelock: ok\n");
	return(SUCCESS);
}
#endif /* ATTSVR3 */
/********savline()	cu:culine.c	2.5********/
static int
savline(void)
{
	int ret;

	if ((Savettyb == NULL) &&
	    (Savettyb = (struct termio *)
			calloc(1, sizeof (struct termio))) == NULL)
		return (FAIL);

	ret = ioctl(0, TCGETA, Savettyb);
	Savettyb->c_cflag = (Savettyb->c_cflag & ~CS8) | CS7;
	Savettyb->c_oflag |= OPOST;
	Savettyb->c_lflag |= (ISIG|ICANON|ECHO);
	return(ret);
}
/********fixline()	cu:culine.c	2.5********/

/*
 * set speed/echo/mode...
 *	tty 	-> terminal name
 *	spwant 	-> speed
 *	type	-> type
 *
 *	if spwant == 0, speed is untouched
 *	type is unused, but needed for compatibility
 *
 * return:  
 *	none
 */

/* ARGSUSED */
static void
fixline(int tty, int spwant, int type)
{
	struct termio		lv;

	CDEBUG(6, "fixline(%d, ", tty);
	CDEBUG(6, "%d)\n", spwant);
	if (ioctl(tty, TCGETA, &lv) != 0)
		return;

/* set line attributes associated with -h, -t, -e, and -o options */

	lv.c_iflag = lv.c_oflag = lv.c_lflag = (ushort)0;
	lv.c_iflag = (IGNPAR | IGNBRK | ISTRIP | IXON | IXOFF);
	lv.c_cc[VEOF] = '\1';

	if (spwant > 0) {
		lv.c_ospeed = (speed_t) spwant;
	}

	/* Why is HUPCL conditional on the speed?  This makes little sense
	 * but note that there is a subtle semantics change from the
	 * previous rev (which made even less sense) -wsm8/23/96.
	 */
	lv.c_cflag = ( CREAD | (tcflag_t)(spwant ? HUPCL : 0));
	
	if(Evenflag) {				/*even parity -e */
		if(lv.c_cflag & PARENB) {
			VERBOSE(P_PARITY, 0);
			exit(1);
		}else 
			lv.c_cflag |= (PARENB | CS7);
	}
	else if(Oddflag) {			/*odd parity -o */
		if(lv.c_cflag & PARENB) {
			VERBOSE(P_PARITY, 0);
			exit(1);
		}else {
			lv.c_cflag |= PARODD;
			lv.c_cflag |= (PARENB | CS7);
		}
	}
	else
		lv.c_cflag |= CS8;


	if(!Duplex)				/*half duplex -h */
		lv.c_iflag &= ~(IXON | IXOFF);
	if(Terminal)				/* -t */
		lv.c_oflag |= (OPOST | ONLCR);

#ifdef NO_MODEM_CTRL
	/*   CLOCAL may cause problems on pdp11s with DHs */
	if (type == D_DIRECT) {
		CDEBUG(4, "fixline - direct\n", "");
		lv.c_cflag |= CLOCAL;
	} else
#endif /* NO_MODEM_CTRL */

		lv.c_cflag &= ~CLOCAL;
	
	DIE_UNLESS(ioctl(tty, TCSETAW, &lv) >= 0,
	    "RETURN FROM fixline ioctl", "", errno);
	return;
}
/********restline()	cu:culine.c	2.5********/
static int
restline(void)
{
	return(ioctl(0, TCSETAW, &Savettyb));
}
/********sethup()	cu:culine.c	2.5********/
static void
sethup(int dcf)
{
	struct termio ttbuf;

	if (ioctl(dcf, TCGETA, &ttbuf) != 0)
		return;
	if (!(ttbuf.c_cflag & HUPCL)) {
		ttbuf.c_cflag |= HUPCL;
		(void) ioctl(dcf, TCSETAW, &ttbuf);
	}
}
/********usetup()	uucp:interface.c	1.9********/

/*
 *	usetup - vanilla unix setup routine
 */
static int
usetup( int role, int *fdreadp, int *fdwritep )
{
	if ( role == SLAVE )
	{
		*fdreadp = 0;
		*fdwritep = 1;
		/* 2 has been re-opened to RMTDEBUG in main() */
	}
	return(SUCCESS);
}
/********uteardown()	uucp:interface.c	1.9********/
/*
 *	uteardown - vanilla unix teardown routine
 */
static int
uteardown( int role, int fdread, int fdwrite )
{
	int ret;

	if (role == SLAVE) {
		ret = restline();
		DEBUG(4, "ret restline - %d\n", ret);
		sethup(0);
	}
	if (fdread != -1) {
#ifndef DIAL
		ttyn = ttyname(fdread);
		if (ttyn != NULL)
			chmod(ttyn, Dev_mode);	/* can fail, but who cares? */
#endif /* DIAL */
		(void) close(fdread);
		(void) close(fdwrite);
	}
	return(SUCCESS);
}
/********tsetup()	uucp:interface.c	1.9********/
#ifdef TLI

/*
 *	tsetup - tli setup routine
 *	note blatant assumption that *fdreadp == *fdwritep == 0
 */
static int
tsetup( int role, int *fdreadp, int *fdwritep )
{
	
	int		initstate = 0;
	struct stat	statbuf;
	extern int	t_errno;

	if ( role == SLAVE ) {
		*fdreadp = 0;
		*fdwritep = 1;
		/* 2 has been re-opened to RMTDEBUG in main() */
		errno = t_errno = 0;
		if ( t_sync(*fdreadp) == -1 || t_sync(*fdwritep) == -1 ) {
			tfaillog(*fdreadp, "tsetup: t_sync\n");
			return(FAIL);
		}
	}
	return(SUCCESS);
}
#endif /* TLI */
/********tssetup()	uucp:interface.c	1.9********/
#ifdef TLIS

/*
 *	tssetup - tli, with streams module, setup routine
 *	note blatant assumption that *fdreadp == *fdwritep
 */
static int
tssetup( int role, int *fdreadp, int *fdwritep )
{
	char		strmod[FMNAMESZ], onstream[FMNAMESZ];
	int		optional;

	if ( role == SLAVE ) {
		*fdreadp = 0;
		*fdwritep = 1;
		/* 2 has been re-opened to RMTDEBUG in main() */
		DEBUG(5, "tssetup: SLAVE mode: leaving ok\n", 0);
		return(SUCCESS);
	}

	/*	check for streams modules to pop	*/
	while ( getpop(strmod, sizeof(strmod), &optional) ) {
		DEBUG(5, 
			( optional ? "tssetup: optionally POPing %s\n"
				   : "tssetup: POPing %s\n" ),
			strmod);
		if ( ioctl(*fdreadp, I_LOOK, onstream) == -1 ) {
			DEBUG(5, "tssetup: I_LOOK on fd %d failed", *fdreadp);
			return(FAIL);
		}
		if ( strcmp(strmod, onstream) != SAME ) {
			if ( optional )
				continue;
			DEBUG(5, "tssetup: I_POP: %s not there\n", strmod);
			return(FAIL);
		}
		if ( ioctl(*fdreadp, I_POP, 0) == -1 ) {
			DEBUG(5, "tssetup: I_POP on fd %d failed", *fdreadp);
			return(FAIL);
		}
	}

	/*	check for streams modules to push	*/
	while ( getpush(strmod, sizeof(strmod)) ) {
		DEBUG(5, "tssetup: PUSHing %s\n", strmod);
		if ( ioctl(*fdreadp, I_PUSH, strmod) == -1 ) {
			DEBUG(5, "tssetup: I_PUSH on fd %d failed", *fdreadp);
			return(FAIL);
		}
	}

	DEBUG(4, "tssetup: MASTER mode: leaving ok\n", "");
	return(SUCCESS);
}
#endif /* TLIS */
/********gdial()	uucp:callers.c	2.18********/

#define MAXLINE	512
/*
 * Get the information about the dialer.
 * gdial(type, arps, narps)
 *	type	-> type of dialer (e.g., penril)
 *	arps	-> array of pointers returned by gdial
 *	narps	-> number of elements in array returned by gdial
 * Return value:
 *	-1	-> Can't open DIALERFILE
 *	0	-> requested type not found
 *	>0	-> success - number of fields filled in
 */
static int
gdial(register char *type, register char *arps[], register int narps)
{
#ifdef sgi
	static char *info = NULL;
#else  /* sgi */
	static char info[MAXLINE];
#endif /* sgi */
	register int na;

#ifdef sgi
	if ((info == NULL) && (info = calloc(1, MAXLINE)) == NULL)
		return (FAIL);
#endif /* sgi */

	DEBUG(2, "gdial(%s) called\n", type);
	while (getdialline(info, sizeof(info))) {
		if ((info[0] == '#') || (info[0] == ' ') ||
		    (info[0] == '\t') || (info[0] == '\n'))
			continue;
		if ((na = getargs(info, arps, narps)) == 0)
			continue;
		if (EQUALS(arps[0], type)) {
			dialreset();
			bsfix(arps);
			return(na);
		}
	}
	dialreset();
	return(0);
}
/********exphone()	uucp:callers.c	2.18********/
/***
 *	exphone - expand phone number for given prefix and number
 *
 *	return code - none
 */

static void
exphone(char *in, char *out)
{
	FILE *fn;
	char pre[MAXPH], npart[MAXPH], tpre[MAXPH], p[MAXPH];
	char buf[BUFSIZ];
	char *s1;

	if (!isalpha(*in)) {
		(void) strcpy(out, in);
		return;
	}

	s1=pre;
	while (isalpha(*in))
		*s1++ = *in++;
	*s1 = NULLCHAR;
	s1 = npart;
	while (*in != NULLCHAR)
		*s1++ = *in++;
	*s1 = NULLCHAR;

	tpre[0] = NULLCHAR;
	fn = fopen(DIALCODES, "r");
	if (fn != NULL) {
		while (fgets(buf, BUFSIZ, fn)) {
			if ( sscanf(buf, "%s%s", p, tpre) < 1)
				continue;
			if (EQUALS(p, pre))
				break;
			tpre[0] = NULLCHAR;
		}
		fclose(fn);
	}

	(void) strcpy(out, tpre);
	(void) strcat(out, npart);
	return;
}
/********translate()	uucp:callers.c	2.18********/
/*
 * translate the pairs of characters present in the first
 * string whenever the first of the pair appears in the second
 * string.
 */
static void
translate(register char *ttab, register char *str)
{
	register char *s;

	for(;*ttab && *(ttab+1); ttab += 2)
		for(s=str;*s;s++)
			if(*ttab == *s)
				*s = *(ttab+1);
}
/********chat()	uucp:conn.c	2.13********/
/*
 * chat -	do conversation
 * input:
 *	nf - number of fields in flds array
 *	flds - fields from Systems file
 *	fn - write file number
 *	phstr1 - phone number to replace \D
 *	phstr2 - phone number to replace \T
 *
 *	return codes:  0  |  FAIL
 */

static int
chat(int nf, char *flds[], int fn, char *phstr1, char *phstr2)
{
	char *want, *altern;
#ifndef sgi
	extern char *index();
#endif
	int k, ok;

	for (k = 0; k < nf; k += 2) {
		want = flds[k];
		ok = FAIL;
		while (ok != 0) {
			altern = index(want, '-');
			if (altern != NULL)
				*altern++ = NULLCHAR;
			ok = expect(want, fn);
			if (ok == 0)
				break;
			if (altern == NULL) {
				Uerror = SS_LOGIN_FAILED;
				logent(UERRORTEXT, "FAILED");
				return(FAIL);
			}
			want = index(altern, '-');
			if (want != NULL)
				*want++ = NULLCHAR;
			sendthem(altern, fn, phstr1, phstr2);
		}
		sleep(2);
		if (flds[k+1])
		    sendthem(flds[k+1], fn, phstr1, phstr2);
	}
	return(0);
}
/********delock()	uucp:ulockf.c	2.7********/
/*
 * remove a lock file
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */
static void
delock(char *s)
{
	char ln[MAXNAMESIZE];

	(void) sprintf(ln, "%s.%s", LOCKPRE, s);
	BASENAME(ln, '/')[MAXBASENAME] = '\0';
	rmlock(ln);
}
/********dkcall()	uucp:callers.c	2.18********/
#ifdef DATAKIT

/***
 *	dkcall(flds, dev)	make a DATAKIT VCS connection
 *				  DATAKIT VCS is a trademark of AT&T
 *
 *	return codes:
 *		>0 - file number - ok
 *		FAIL - failed
 */

#include "dk.h"

static
dkcall(char *flds[], char *dev[])
{
	register fd;
#ifdef V8
	extern int cdkp_ld;
#endif

	char	dialstring[64];

	strcpy(dialstring, dev[D_ARG]);

#ifndef STANDALONE
	if(*flds[F_CLASS] < '0' || *flds[F_CLASS] > '9')
		sprintf(dialstring, "%s.%s", dev[D_ARG], flds[F_CLASS]);
#endif

	DEBUG(4, "dkcall(%s)\n", dialstring);


#ifdef V8
	if (setjmp(Sjbuf)) {
		Uerror = SS_DIAL_FAILED;
		return(FAIL);
	}

	(void) signal(SIGALRM, alarmtr);
	(void) alarm(15);
	DEBUG(4, "tdkdial(%s", flds[F_PHONE]);
	DEBUG(4, ", %d)\n", atoi(dev[D_CLASS]));
    	if ((fd = tdkdial(flds[F_PHONE], atoi(dev[D_CLASS]))) >= 0)
	    if (dkproto(fd, cdkp_ld) < 0)
	       {
	    	close(fd);
	    	fd = -1;
	       }
	(void) alarm(0);
#else
	fd = dkdial(dialstring);
#endif

	(void) strcpy(Dc, "DK");
	if (fd < 0) {
		Uerror = SS_DIAL_FAILED;
		return(FAIL);
	}
	else
		return(fd);
}

#endif /* DATAKIT */
/********sytcall()	uucp:callers.c	2.18********/
#ifdef SYTEK

/****
 *	sytcall(flds, dev)	make a sytek connection
 *
 *	return codes:
 *		>0 - file number - ok
 *		FAIL - failed
 */

/*ARGSUSED*/
static
sytcall(char *flds[], char *dev[])
{
	int dcr, dcr2, nullfd, ret;
	char dcname[20], command[BUFSIZ];

	(void) sprintf(dcname, "/dev/%s", dev[D_LINE]);
	DEBUG(4, "dc - %s, ", dcname);
	if (mlock(dev[D_LINE]) == FAIL) { /* Lock the line */
		DEBUG(5, "mlock %s failed\n", dev[D_LINE]);
		Uerror = SS_LOCKED_DEVICE;
		return(FAIL);
	}
	dcr = open(dcname, O_WRONLY|O_NDELAY);
	if (dcr < 0) {
		Uerror = SS_DIAL_FAILED;
		DEBUG(4, "OPEN FAILED %s\n", dcname);
		delock(dev[D_LINE]);
		return(FAIL);
	}
	if ( filelock(dcr) != SUCCESS ) {
		(void)close(dcr);
		DEBUG(1, "failed to lock device %s\n", dcname);
		Uerror = SS_LOCKED_DEVICE;
	}

	sytfixline(dcr, atoi(fdig(dev[D_CLASS])), D_DIRECT);
	(void) sleep(2);
	DEBUG(4, "Calling Sytek unit %s\n", dev[D_ARG]);
	(void) sprintf(command,"\r\rcall %s\r", dev[D_ARG]);
	ret = (*Write)(dcr, command, strlen(command));
	(void) sleep(1);
	DEBUG(4, "COM1 return = %d\n", ret);
	sytfix2line(dcr);
	(void) close(nullfd = open("/", 0));
	(void) signal(SIGALRM, alarmtr);
	if (setjmp(Sjbuf)) {
		DEBUG(4, "timeout sytek open\n", 0);
		(void) close(nullfd);
		(void) close(dcr2);
		(void) close(dcr);
		Uerror = SS_DIAL_FAILED;
		delock(dev[D_LINE]);
		return(FAIL);
	}
	(void) alarm(10);
	dcr2 = open(dcname,O_RDWR);
	(void) alarm(0);
	(void) close(dcr);
	if (dcr2 < 0) {
		DEBUG(4, "OPEN 2 FAILED %s\n", dcname);
		Uerror = SS_DIAL_FAILED;
		(void) close(nullfd);	/* kernel might think dc2 is open */
		delock(dev[D_LINE]);
		return(FAIL);
	}
	return(dcr2);
}

#endif /* SYTEK */
/********sytfixline()	uucp:line.c	2.7********/
#ifdef SYTEK

/***
 *	sytfixline(tty, spwant)	set speed/echo/mode...
 *	int tty, spwant;
 *
 *	return codes:  none
 */

static
sytfixline(int tty, int spwant)
{
	struct termio ttbuf;
	int ret;

	if ( (*Ioctl)(tty, TCGETA, &ttbuf) != 0 )
		return;
	ttbuf.c_iflag = (ushort)0;
	ttbuf.c_oflag = (ushort)0;
	ttbuf.c_lflag = (ushort)0;
	ttbuf.c_ospeed = spwant;
	ttbuf.c_cflag = (CS8|CLOCAL);
	ttbuf.c_cc[VMIN] = 6;
	ttbuf.c_cc[VTIME] = 1;
	ret = (*Ioctl)(tty, TCSETAW, &ttbuf);
	DIE_UNLESS(ret >= 0, "RETURN FROM sytfixline", "", ret);
	return;
}

static
sytfix2line(int tty)
{
	struct termio ttbuf;
	int ret;

	if ( (*Ioctl)(tty, TCGETA, &ttbuf) != 0 )
		return;
	ttbuf.c_cflag &= ~CLOCAL;
	ttbuf.c_cflag |= CREAD|HUPCL;
	ret = (*Ioctl)(tty, TCSETAW, &ttbuf);
	DIE_UNLESS(ret >= 0, "RETURN FROM sytfix2line", "", ret);
	return;
}

#endif /* SYTEK */
/********tlicall()	uucp:callers.c	2.18********/
#ifdef TLI
/*
 *
 * AT&T Transport Layer Interface
 *
 * expected in Devices
 *	TLI line1 - - TLI 
 * or
 *	TLIS line1 - - TLIS
 *
 */

#define	CONNECT_ATTEMPTS	3
#define	TFREE(p)	if ( (p) ) t_free( (p) )

/*
 * returns fd to remote uucp daemon
 */
static int
tlicall(char *flds[], char *dev[])
{
	char		addrbuf[ BUFSIZ ];
	char		devname[MAXNAMESIZE];
	int		fd, service;
	register int	i, j;
	struct t_bind	*bind_ret = 0;
	struct t_info	tinfo;
	struct t_call	*sndcall = 0, *rcvcall = 0;
	extern int	t_errno;
	extern char	*t_errlist[];

	extern struct netbuf	*stoa();

	if ( dev[D_LINE][0] != '/' ) {
		/*	dev holds device name relative to /dev	*/
		sprintf(devname, "/dev/%s", dev[D_LINE]);
	} else {
		/*	dev holds full path name of device	*/
		strcpy(devname, dev[D_LINE]);
	}
	/* gimme local transport endpoint */
	errno = t_errno = 0;
	fd = t_open(devname, O_RDWR, &tinfo);
	if (fd < 0) {
		tfaillog(fd, "t_open" );
		Uerror = SS_NO_DEVICE;
		return(FAIL);
	}
	if ( filelock(fd) != SUCCESS ) {
		(void)t_close(fd);
		DEBUG(1, "tlicall: failed to lock device %s\n", devname);
		Uerror = SS_LOCKED_DEVICE;
		return(FAIL);
	}

	/* allocate tli structures	*/
	errno = t_errno = 0;
	if ( (bind_ret = (struct t_bind *)t_alloc(fd, T_BIND, T_ALL)) == 
	    (struct t_bind *)NULL
	|| (sndcall = (struct t_call *)t_alloc(fd, T_CALL, T_ALL)) == 
	    (struct t_call *)NULL
	|| (rcvcall = (struct t_call *)t_alloc(fd, T_CALL, T_ALL)) ==
	    (struct t_call *)NULL ) {
		tfaillog(fd, "t_alloc" );
		TFREE(bind_ret);TFREE(sndcall);TFREE(rcvcall);
		Uerror = SS_NO_DEVICE;
		return(FAIL);
	}

	/* bind */
	errno = t_errno = 0;
	if (t_bind(fd, (struct t_bind *) 0, bind_ret ) < 0) {
		tfaillog(fd, "t_bind" );
		TFREE(bind_ret);TFREE(sndcall);TFREE(rcvcall);
		Uerror = SS_NO_DEVICE;
		(void) t_close(fd);
		return(FAIL);
	}
	DEBUG(5, "tlicall: bound to %s\n", bind_ret->addr.buf);

	/*
	 * Prepare to connect.
	 *
	 * If address begins with "\x", "\X", "\o", or "\O",
	 * assume is hexadecimal or octal address and use stoa()
	 * to convert it.
	 *
	 * Else is usual uucico address -- only \N's left to process.
	 * Walk thru connection address, changing \N's to NULLCHARs.
	 * Note:  If a NULLCHAR must be part of the connection address,
	 * it must be overtly included in the address.  One recommended
	 * way is to do it in the Devices file, thusly:
	 *		Netname /dev/netport - - TLI \D\000
	 * bsfix() turns \000 into \N and then the loop below makes it a
	 * real, included-in-the-length null-byte.
	 *
	 * The DEBUG must print the strecpy'd address (so that
	 * non-printables will have been replaced with C escapes).
	 */

	DEBUG(5, "t_connect to addr \"%s\"\n",
		strecpy( addrbuf, dev[D_ARG], "\\" ) );

	if ( dev[D_ARG][0] == '\\' &&
	( dev[D_ARG][1] == 'x' || dev[D_ARG][1] == 'X'
	|| dev[D_ARG][1] == 'o' || dev[D_ARG][1] == 'O' ) ) {
		if ( stoa(dev[D_ARG], &(sndcall->addr)) == (struct netbuf *)NULL ) {
			DEBUG(5, "tlicall: stoa failed\n","");
			logent("tlicall", "string-to-address failed");
			Uerror = SS_NO_DEVICE;
			(void) t_close(fd);
			return(FAIL);
		}
	} else {
		for( i = j = 0; dev[D_ARG][i] != NULLCHAR; ++i, ++j ) {
			if( dev[D_ARG][i] == '\\'  &&  dev[D_ARG][i+1] == 'N' ) {
				addrbuf[j] = NULLCHAR;
				++i;
			}
			else {
				addrbuf[j] = dev[D_ARG][i];
			}
		}
		sndcall->addr.buf = addrbuf;
		sndcall->addr.len = j;
	}

	if (setjmp(Sjbuf)) {
		DEBUG(4, "timeout tlicall\n", 0);
		logent("tlicall", "TIMEOUT");
		TFREE(bind_ret);TFREE(sndcall);TFREE(rcvcall);
		Uerror = SS_NO_DEVICE;
		(void) t_close(fd);
		return(FAIL);
	}
	(void) signal(SIGALRM, alarmtr);
	(void) alarm(30);

	/* connect to the service -- some listeners can't handle */
	/* multiple connect requests, so try it a few times */
	errno = t_errno = 0;
	for ( i = 0; i < CONNECT_ATTEMPTS; ++i ) {
		if (t_connect(fd, sndcall, rcvcall) == 0)
			break;
		if ( (t_errno == TLOOK) && (t_look(fd) == T_DISCONNECT)) {
			t_rcvdis(fd,NULL);
			(void) alarm(0);
		} else {
			(void) alarm(0);
			tfaillog(fd, "t_connect");
			TFREE(bind_ret);TFREE(sndcall);TFREE(rcvcall);
			Uerror = SS_DIAL_FAILED;
			(void) t_close(fd);
			return(FAIL);
		}
	}
	(void) alarm(0);
	TFREE(bind_ret);TFREE(sndcall);TFREE(rcvcall);
	errno = t_errno = 0;
	return(fd);
}

#undef	CONNECT_ATTEMPTS
#undef	TFREE


#endif /* TLI */
/********tfaillog()	uucp:interface.c	1.9********/
#ifdef TLI

/**
 **	Report why a TLI call failed.
 */
static
tfaillog(int fd, char *s)
{
	extern char	*t_errlist[];
	extern int	t_errno, t_nerr;
	char	fmt[ BUFSIZ ];

	if (0 < t_errno && t_errno < t_nerr) {
		sprintf( fmt, "%s: %%s\n", s );
		DEBUG(5, fmt, t_errlist[t_errno]);
		logent(s, t_errlist[t_errno]);
		if ( t_errno == TSYSERR ) {
			DEBUG(5, "", perror("tlicall: system error"));
		} else if ( t_errno == TLOOK ) {
			show_tlook(fd);
		}
	} else {
		sprintf(fmt, "unknown tli error %d", t_errno);
		logent(s, fmt);
		sprintf(fmt, "%s: unknown tli error %d", s, t_errno);
		DEBUG(5, fmt, 0);
		DEBUG(5, "", perror(s));
	}
}

static int
show_tlook(int fd)
{
	register int reason;
	register char *msg;
/*
 * Find out the current state of the interface.
 */
	errno = t_errno = 0;
	switch( reason = t_getstate(fd) ) {
	case T_UNBND:		msg = "T_UNBIND";	break;
	case T_IDLE:		msg = "T_IDLE";		break;
	case T_OUTCON:		msg = "T_OUTCON";	break;
	case T_INCON:		msg = "T_INCON";	break;
	case T_DATAXFER:	msg = "T_DATAXFER";	break;
	case T_OUTREL:		msg = "T_OUTREL";	break;
	case T_INREL:		msg = "T_INREL";	break;
	default:		msg = NULL;		break;
	}
	if( msg == NULL )
		return;

	DEBUG(5, "state is %s", msg);
	switch( reason = t_look(fd) ) {
	case -1:		msg = ""; break;
	case 0:			msg = "NO ERROR"; break;
	case T_LISTEN:		msg = "T_LISTEN"; break;
	case T_CONNECT:		msg = "T_CONNECT"; break;
	case T_DATA:		msg = "T_DATA";	 break;
	case T_EXDATA:		msg = "T_EXDATA"; break;
	case T_DISCONNECT:	msg = "T_DISCONNECT"; break;
	case T_ORDREL:		msg = "T_ORDREL"; break;
	case T_ERROR:		msg = "T_ERROR"; break;
	case T_UDERR:		msg = "T_UDERR"; break;
	default:		msg = "UNKNOWN ERROR"; break;
	}
	DEBUG(4, " reason is %s\n", msg);

	if ( reason == T_DISCONNECT )
	{
		struct t_discon	*dropped;
		if ( ((dropped = 
			(struct t_discon *)t_alloc(fd, T_DIS, T_ALL)) == 0) 
		||  (t_rcvdis(fd, dropped) == -1 )) {
			return;
		}
		DEBUG(5, "disconnect reason #%d\n", dropped->reason);
		t_free(dropped);
	}
	return;
}

#endif /*  TLI  */
/********stoa()	uucp:stoa.c	1.2********/

#ident	"@(#)uucp:stoa.c	1.2"

#ifdef TLI

/*
	stoa - convert string to address

	If a string begins in \o or \O, the following address is octal
	"  "   "       "    " \x or \X, the following address is hex

	If ok, return pointer to netbuf structure.
	A  NULL is returned on any error(s).
*/



#define	toupper(c)	(islower(c) ? _toupper(c) : (c))
#define	todigit(c)	((int)((c) - '0'))	/* char to digit */
#define	toxdigit(c)	((isdigit(c))?todigit(c):(toupper(c)-(int)'A'+10))
#define	isodigit(c)	(isdigit(c) && ((c) != '9') && ((c) != '8'))
#define	itoac(i)	(((i) > 9) ? ((char)((i)-10) + 'A'):((char)(i) + '0'))	
#define	MASK(n)		((1 << (n)) - 1)

#define	SBUFSIZE	128


static
struct netbuf *
stoa(char *str, struct netbuf *addr)	/* Return netbuf ptr if success else NULL if errror */
{
	char	*xfer();
#ifdef sgi
	static char	*sbuf = NULL;

	if ((sbuf == NULL) && ((sbuf = calloc(1, SBUFSIZE)) == NULL))
		return (NULL);
#else  /* sgi */
	static char	sbuf[SBUFSIZE];
#endif /* sgi */


	int	myadr;		/* was netbuf struct allocated here ? */

	myadr = FALSE;

	if (!str)
		return NULL;
	while (*str && isspace(*str))	/* leading whites are OK */
		++str;

	if (!str || !*str) return NULL;		/* Nothing to convert */

	if (!addr) {
		if ((addr = (struct netbuf *)malloc(sizeof(struct netbuf))) == NULL)
			return NULL;
		myadr = TRUE;
		addr->buf = NULL;
		addr->maxlen = 0;
		addr->len = 0;
	}

	/* Now process the address */
	if (*str == '\\') {
		++str;
		switch (*str) {

		case 'X':	/* hex */
		case 'x':
			addr->len = dobase(++str, sbuf, HEX);
			break;

		case 'o':	/* octal */
		case 'O':
			addr->len = dobase(++str, sbuf, OCT);
			break;

		default:	/* error */
			return NULL;
			break;
		}
	}

	if (addr->len == 0) {	/* Error in conversion */
		if (myadr)
			free(addr);
		return NULL;
	}
	if ((addr->buf = xfer(addr->buf, sbuf, addr->len, addr->maxlen)) == NULL)
		return NULL;
	else
		return addr;
}



/*
	dobase :	converts a hex or octal ASCII string
		to a binary address. Only HEX or OCT may be used
		for type.
	return length of binary string (in bytes), 0 if error.
	The binary result is placed at buf.
*/

static int
dobase(char *s, char *buf, int type)	/* read in an address */
{
	int	bp = SBUFSIZE - 1;
	int	shift = 0;
	char	*end;

	for (end = s; *end && ((type == OCT) ? isodigit(*end) :
		isxdigit(*end)); ++end) ;

	/* any non-white, non-digits cause address to be rejected,
	   other fields are ignored */

	if ((*s == 0) || (end == s) || (!isspace(*end) && *end)) {
		fprintf(stderr, "dobase: Illegal trailer on address string\n");
		buf[0] = '\0';
		return 0;
	}
	--end;

	buf[bp] = '\0';

	while (bp > 0 && end >= s) {
		buf[bp] |= toxdigit(*end) << shift;
		if (type == OCT) {
			if (shift > 5) {
				buf[--bp] = (todigit(*end) >> (8 - shift))
					& MASK(shift-5);
			}
			if ((shift = (shift + 3) % 8) == 0)
				buf[--bp] = 0;
		}
		else	/* hex */
			if ((shift = (shift) ? 0 : 4) == 0)
				buf[--bp] = 0;;
		--end;
	}
	if (bp == 0) {
		fprintf(stderr, "stoa: dobase: number to long\n");
		return 0;
	}

	/* need to catch end case to avoid extra 0's in front	*/
	if (!shift)
		bp++;
	memcp(buf, &buf[bp], (SBUFSIZE - bp));
	return (SBUFSIZE - bp);
}



static void
memcp(char *d, char *s, int n)	/* safe memcpy for overlapping regions */
{
	while (n--)
		*d++ = *s++;
}


/* transfer block to a given destination or allocate one of the
    right size 
    if max = 0 : ignore max
*/

static char *
xfer(char *dest, char *src, unsigned len, unsigned max)
{
	if (max && dest && max < len) {		/* No room */
		fprintf(stderr, "xfer: destination not long enough\n");
		return NULL;
	}
	if (!dest)
		if ((dest = malloc(len)) == NULL) {
			fprintf(stderr, "xfer: malloc failed\n");
			return NULL;
		}

	memcpy(dest, src, (int)len);
	return dest;
}

#endif /* TLI */
/********dial801()	uucp:callers.c	2.18********/
#ifdef DIAL801

/***
 *	dial801(flds, dev)	dial remote machine on 801/801
 *	char *flds[], *dev[];
 *
 *	return codes:
 *		file descriptor  -  succeeded
 *		FAIL  -  failed
 *
 *	unfortunately, open801() is different for usg and non-usg
 */

/*ARGSUSED*/
static int
dial801(char *flds[], char *dev[])
{
	char dcname[20], dnname[20], phone[MAXPH+2];
	int dcf = -1, speed;

	if (mlock(dev[D_LINE]) == FAIL) {
		DEBUG(5, "mlock %s failed\n", dev[D_LINE]);
		Uerror = SS_LOCKED_DEVICE;
		return(FAIL);
	}
	(void) sprintf(dnname, "/dev/%s", dev[D_CALLDEV]);
	(void) sprintf(phone, "%s%s", dev[D_ARG]   , ACULAST);
	(void) sprintf(dcname, "/dev/%s", dev[D_LINE]);
	CDEBUG(1, "Use Port %s, ", dcname);
	DEBUG(4, "acu - %s, ", dnname);
	CDEBUG(1, "Phone Number  %s\n", phone);
	VERBOSE("Trying modem - %s, ", dcname);	/* for cu */
	VERBOSE("acu - %s, ", dnname);	/* for cu */
	VERBOSE("calling  %s:  ", phone);	/* for cu */
	speed = atoi(fdig(dev[D_CLASS]));
	dcf = open801(dcname, dnname, phone, speed);
	if (dcf >= 0) {
		fixline(dcf, speed, D_ACU);
		(void) strcpy(Dc, dev[D_LINE]);	/* for later unlock() */
		VERBOSE0("SUCCEEDED\n");
	} else {
		delock(dev[D_LINE]);
		VERBOSE0("FAILED\n");
	}
	return(dcf);
}


#ifndef ATTSV
/*ARGSUSED*/
static
open801(char *dcname, char *dnname, char *phone, int speed)
{
	int nw, lt, pid = -1, dcf = -1, nullfd, dnf = -1;
	unsigned timelim;

	if ((dnf = open(dnname, 1)) < 0) {
		DEBUG(5, "can't open %s\n", dnname);
		Uerror = SS_CANT_ACCESS_DEVICE;
		return(FAIL);
	}
	DEBUG(5, "%s is open\n", dnname);

	(void) close(nullfd = open("/dev/null", 0));	/* partial open hack */
	if (setjmp(Sjbuf)) {
		DEBUG(4, "timeout modem open\n", 0);
		logent("801 open", "TIMEOUT");
		(void) close(nullfd);
		(void) close(dcf);
		(void) close(dnf);
		if (pid > 0) {
			kill(pid, 9);
			wait((int *) 0);
		}
		Uerror = SS_DIAL_FAILED;
		return(FAIL);
	}
	(void) signal(SIGALRM, alarmtr);
	timelim = 5 * strlen(phone);
	(void) alarm(timelim < 30 ? 30 : timelim);
	if ((pid = fork()) == 0) {
		sleep(2);
		nw = (*Write)(dnf, phone, lt = strlen(phone));
		if (nw != lt) {
			DEBUG(4, "ACU write error %d\n", errno);
			logent("ACU write", "FAILED");
			exit(1);
		}
		DEBUG(4, "ACU write ok%s\n", 0);
		exit(0);
	}
	/*  open line - will return on carrier */
	dcf = open(dcname, 2);

	DEBUG(4, "dcf is %d\n", dcf);
	if (dcf < 0) {	/* handle like a timeout */
		(void) alarm(0);
		longjmp(Sjbuf, 1);
	}

	/* modem is open */
	while ((nw = wait(&lt)) != pid && nw != -1)
		;
	(void) alarm(0);

	(void) close(dnf);	/* no reason to keep the 801 open */
	if (lt != 0) {
		DEBUG(4, "Fork Stat %o\n", lt);
		(void) close(dcf);
		Uerror = SS_DIAL_FAILED;
		return(FAIL);
	}
	return(dcf);
}

#else /* ATTSV */

static int
open801(char *dcname, char *dnname, char *phone, int speed)
{
	int dcf = -1, nullfd, dnf = -1, ret;
	ssize_t nw;
	size_t lt;
	unsigned timelim;

	(void) close(nullfd = open("/", 0));	/* partial open hack */
	if (setjmp(Sjbuf)) {
		DEBUG(4, "DN write %s\n", "timeout");
		(void) close(dnf);
		(void) close(dcf);
		(void) close(nullfd);
		Uerror = SS_DIAL_FAILED;
		return(FAIL);
	}
	(void) signal(SIGALRM, alarmtr);
	timelim = 5 * (unsigned)strlen(phone);
	(void) alarm(timelim < 30 ? 30 : timelim);

	if ((dnf = open(dnname, O_WRONLY)) < 0 ) {
		DEBUG(5, "can't open %s\n", dnname);
		Uerror = SS_CANT_ACCESS_DEVICE;
		return(FAIL);
	}
	DEBUG(5, "%s is open\n", dnname);
	if ( filelock(dnf) != SUCCESS ) {
		(void)close(dnf);
		DEBUG(1, "failed to lock device %s\n", dnname);
		Uerror = SS_LOCKED_DEVICE;
	}
	if (  (dcf = open(dcname, O_RDWR | O_NDELAY)) < 0 ) {
		DEBUG(5, "can't open %s\n", dcname);
		Uerror = SS_CANT_ACCESS_DEVICE;
		return(FAIL);
	}
	if ( filelock(dcf) != SUCCESS ) {
		(void)close(dcf);
		DEBUG(1, "failed to lock device %s\n", dcname);
		Uerror = SS_LOCKED_DEVICE;
	}

	DEBUG(4, "dcf is %d\n", dcf);
	fixline(dcf, speed, D_ACU);
	lt = (strlen(phone));
	nw = (*Write)(dnf, phone, lt);
	if (nw != lt) {
		(void) alarm(0);
		DEBUG(4, "ACU write error %d\n", errno);
		(void) close(dnf);
		(void) close(dcf);
		Uerror = SS_DIAL_FAILED;
		return(FAIL);
	} else 
		DEBUG0(4, "ACU write ok\n");

	(void) close(dnf);
	(void) close(nullfd = open("/", 0));	/* partial open hack */
	ret = open(dcname, 2);  /* wait for carrier  */
	(void) alarm(0);
	(void) close(ret);	/* close 2nd modem open() */
	if (ret < 0) {		/* open() interrupted by alarm */
		DEBUG(4, "Line open %s\n", "failed");
		Uerror = SS_DIAL_FAILED;
		(void) close(nullfd);		/* close partially opened modem */
		return(FAIL);
	}
	(void) fcntl(dcf,F_SETFL, fcntl(dcf, F_GETFL, 0) & ~O_NDELAY);
	return(dcf);
}
#endif /* ATTSV */

#endif /* DIAL801 */
/********Dialout()	uucp:callers.c	2.18********/
#ifdef V8
static
Dialout(char *flds[])
{
    int fd;
    char phone[MAXPH+2];

    exphone(flds[F_PHONE], phone);

    DEBUG(4, "call dialout(%s", phone);
    DEBUG(4, ", %s)\n", dev[D_CLASS]);
    fd = dialout(phone, dev[D_CLASS]);
    if (fd == -1)
	Uerror = SS_NO_DEVICE;
    if (fd == -3)
	Uerror = SS_DIAL_FAILED;
    if (fd == -9)
	Uerror = SS_DEVICE_FAILED;

    (void) strcpy(Dc, "Dialout");

    return(fd);
}
#endif /* V8 */
/********tcpcall()	uucp:callers.c	2.18********/
#ifdef TCP

/***
 *	tcpcall(flds, dev)	make ethernet/socket connection
 *
 *	return codes:
 *		>0 - file number - ok
 *		FAIL - failed
 */

#ifndef BSD4_2
/*ARGSUSED*/
static int
tcpcall(char *flds[], char *dev[])
{
	Uerror = SS_NO_DEVICE;
	return(FAIL);
}
#else /* BSD4_2 */
static
tcpcall(char *flds[], char *dev[])
{
	int ret;
	short port;
	struct servent *sp;
	struct hostent *hp;
	struct sockaddr_in sin;

	port = atoi(dev[D_ARG]);
	if (port == 0) {
		sp = getservbyname("uucp", "tcp");
		DIE_UNLESS(sp != NULL, "No uucp server", 0, 0);
		port = sp->s_port;
	}
	else port = htons(port);
	hp = gethostbyname(flds[F_NAME]);
	if (hp == NULL) {
		logent("tcpopen", "no such host");
		Uerror = SS_NO_DEVICE;
		return(FAIL);
	}
	DEBUG(4, "tcpdial host %s, ", flds[F_NAME]);
	DEBUG(4, "port %d\n", ntohs(port));

	ret = socket(AF_INET, SOCK_STREAM, 0);
	if (ret < 0) {
		if (errno < sys_nerr) {
			DEBUG(5, "no socket: %s\n", sys_errlist[errno]);
			logent("no socket", sys_errlist[errno]);
		}
		else {
			DEBUG(5, "no socket, errno %d\n", errno);
			logent("tcpopen", "NO SOCKET");
		}
		Uerror = SS_NO_DEVICE;
		return(FAIL);
	}
	sin.sin_family = hp->h_addrtype;
	bcopy(hp->h_addr, (caddr_t)&sin.sin_addr, hp->h_length);
	sin.sin_port = port;
	if (setjmp(Sjbuf)) {
		DEBUG(4, "timeout tcpopen\n", 0);
		logent("tcpopen", "TIMEOUT");
		Uerror = SS_NO_DEVICE;
		return(FAIL);
	}
	(void) signal(SIGALRM, alarmtr);
	(void) alarm(30);
	DEBUG(7, "family: %d\n", sin.sin_family);
	DEBUG(7, "port: %d\n", sin.sin_port);
	DEBUG(7, "addr: %08x\n",*((int *) &sin.sin_addr));
	if (connect(ret, (caddr_t)&sin, sizeof (sin)) < 0) {
		(void) alarm(0);
		(void) close(ret);
		if (errno < sys_nerr) {
			DEBUG(5, "connect failed: %s\n", sys_errlist[errno]);
			logent("connect failed", sys_errlist[errno]);
		}
		else {
			DEBUG(5, "connect failed, errno %d\n", errno);
			logent("tcpopen", "CONNECT FAILED");
		}
		Uerror = SS_NO_DEVICE;
		return(FAIL);
	}
	(void) signal(SIGPIPE, SIG_IGN);  /* watch out for broken ipc link...*/
	(void) alarm(0);
	(void) strcpy(Dc, "IPC");
	return(ret);
}

#endif /* BSD4_2 */

/***
 *	unetcall(flds, dev)	make ethernet connection
 *
 *	return codes:
 *		>0 - file number - ok
 *		FAIL - failed
 */

#ifndef UNET
static
unetcall(char *flds[], char *dev[])
{
	Uerror = SS_NO_DEVICE;
	return(FAIL);
}
#else /* UNET */
static
unetcall(char *flds[], char *dev[])
{
	int ret;
	int port;

	port = atoi(dev[D_ARG]);
	DEBUG(4, "unetdial host %s, ", flds[F_NAME]);
	DEBUG(4, "port %d\n", port);
	(void) alarm(30);
	ret = tcpopen(flds[F_NAME], port, 0, TO_ACTIVE, "rw");
	(void) alarm(0);
	endhnent();
	if (ret < 0) {
		DEBUG(5, "tcpopen failed: errno %d\n", errno);
		Uerror = SS_DIAL_FAILED;
		return(FAIL);
	}
	(void) strcpy(Dc, "UNET");
	return(ret);
}
#endif /* UNET */

#endif /* TCP */
/********ulockf()	uucp:ulockf.c	2.7********/

#ifdef ATTSVKILL
/*
 * create a lock file (file).
 * If one already exists, send a signal 0 to the process--if
 * it fails, then unlink it and make a new one.
 *
 * input:
 *	file - name of the lock file
 *	atime - is unused, but we keep it for lint compatibility with non-ATTSVKILL
 *
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */

/* ARGSUSED */
static int
ulockf(register char *file, time_t atime)
{
#ifdef sgi
	static char *pid = NULL, *tempfile = NULL;
#else  /* sgi */
	static char pid[SIZEOFPID+2];		/* +2 for '\n' and NULL */
	static char tempfile[MAXNAMESIZE];
#endif /* sgi */

#ifdef V8
	char *cp;
#endif

#ifdef sgi
	if ((pid == NULL) && (pid = calloc(1, SIZEOFPID+2)) == NULL)
		return (FAIL);
	if ((tempfile == NULL) && (tempfile = calloc(1, MAXNAMESIZE)) == NULL)
		return (FAIL);
#endif /* sgi */

	if (pid[0] == '\0') {
		(void) sprintf(pid, "%*d\n", SIZEOFPID, getpid());
		(void) sprintf(tempfile, "%s/LTMP.%d", X_LOCKDIR, getpid());
	}

#ifdef V8	/* this wouldn't be a problem if we used lock directories */
		/* some day the truncation of system names will bite us */
	cp = rindex(file, '/');
	if (cp++ != CNULL)
	    if (strlen(cp) > DIRSIZ)
		*(cp+DIRSIZ) = NULLCHAR;
#endif /* V8 */
	if (onelock(pid, tempfile, file) == -1) {
		(void) unlink(tempfile);
		if (checkLock(file))
			return(FAIL);
		else {
		    if (onelock(pid, tempfile, file)) {
			(void) unlink(tempfile);
			DEBUG0(4, "ulockf failed in onelock()\n");
			return(FAIL);
		    }
		}
	}

	stlock(file);
	return(0);
}

#else	/* !ATTSVKILL */


/*
 * create a lock file (file).
 * If one already exists, the create time is checked for
 * older than the age time (atime).
 * If it is older, an attempt will be made to unlink it
 * and create a new one.
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */
static
ulockf(register char *file, time_t atime)
{
	register int ret;
	struct stat stbuf;
	static char pid[SIZEOFPID+2];		/* +2 for '\n' and null */
	static char tempfile[MAXNAMESIZE];
	time_t ptime, time();
#ifdef V8
	char *cp;
#endif


	if (pid[0] == '\0') {
		(void) sprintf(pid, "%*d\n", SIZEOFPID, getpid());
		(void) sprintf(tempfile, "%s/LTMP.%d", X_LOCKDIR, getpid());
	}
#ifdef V8	/* this wouldn't be a problem if we used lock directories */
		/* some day the truncation of system names will bite us */
	cp = rindex(file, '/');
	if (cp++ != CNULL)
	    if (strlen(cp) > DIRSIZ)
		*(cp+DIRSIZ) = NULLCHAR;
#endif /* V8 */
	if (onelock(pid, tempfile, file) == -1) {

		/*
		 * lock file exists
		 * get status to check age of the lock file
		 */
		(void) unlink(tempfile);
		ret = stat(file, &stbuf);
		if (ret != -1) {
			(void) time(&ptime);
			if ((ptime - stbuf.st_ctime) < atime) {

				/*
				 * file not old enough to delete
				 */
				return(FAIL);
			}
		}
		ret = unlink(file);
		ret = onelock(pid, tempfile, file);
		if (ret != 0)
			return(FAIL);
	}
	stlock(file);
	return(0);
}
#endif	/* ATTSVKILL */
/********expect()	uucp:conn.c	2.13********/

#define MR 300

/***
 *	alarmtr()  -  catch alarm routine for "expect".
 */

#ifdef sgi
static void
#else /* sgi */
static int
#endif /* sgi */
alarmtr(void)
{
	CDEBUG0(6, "timed out\n");
	longjmp(Sjbuf, 1);
}


/***
 *	expect(str, fn)	look for expected string
 *	char *str;
 *
 *	return codes:
 *		0  -  found
 *		FAIL  -  lost line or too many characters read
 *		some character  -  timed out
 */

static int
expect(char *str, int fn)
{
#ifdef sgi
	static char *rdvec = NULL;
#else  /* sgi */
	static char rdvec[MR];
#endif /* sgi */
	char *rp = 0;
	register int kr, c;
	ssize_t rv;
	char nextch;

#ifdef sgi
	if ((rdvec == NULL) && (rdvec = calloc(1, MR)) == NULL)
		return (FAIL);
#endif /* sgi */

	CDEBUG0(4, "expect: (");
	for (c=0; kr=str[c]; c++)
		if (kr < 040) {
			CDEBUG(4, "^%c", kr | 0100);
		} else
			CDEBUG(4, "%c", kr);
	CDEBUG0(4, ")\n");

	if (EQUALS(str, "\"\"")) {
		CDEBUG0(4, "got it\n");
		return(0);
	}
	if (setjmp(Sjbuf)) {
		return(FAIL);
	}
	(void) signal(SIGALRM, alarmtr);
	alarm(MAXEXPECTTIME);
	while (notin(str, rdvec)) {
		errno = 0;
		rv = (*Read)(fn, &nextch, 1);
		if (rv <= 0) {
			alarm(0);
			CDEBUG(4, "lost line errno - %d\n", errno);
			logent("LOGIN", "LOST LINE");
			return(FAIL);
		}
		c = nextch & 0177;
		CDEBUG(4, "%s", c < 040 ? "^" : "");
		CDEBUG(4, "%c", c < 040 ? c | 0100 : c);
		if ((*rp = (char)(nextch & 0177)) != NULLCHAR)
			rp++;
		if (rp >= rdvec + MR) {
			CDEBUG0(4, "enough already\n");
			alarm(0);
			return(FAIL);
		}
		*rp = NULLCHAR;
	}
	alarm(0);
	CDEBUG0(4, "got it\n");
	return(0);
}
/********sendthem()	uucp:conn.c	2.13********/
/***
 *	sendthem(str, fn, phstr1, phstr2)	send line of chat sequence
 *	char *str, *phstr;
 *
 *	return codes:  none
 */

#define FLUSH() {\
	if (wrstr(fn, buf, (int)(bptr - buf), echocheck) != SUCCESS)\
		goto err;\
	bptr = buf;\
}

static void
sendthem(char *str, int fn, char *phstr1, char *phstr2)
{
	int sendcr = 1, echocheck = 0;
	register char	*sptr, *bptr;
	char	buf[BUFSIZ];

	/* should be EQUALS, but previous versions had BREAK n for integer n */
	if (PREFIX("BREAK", str)) {
		/* send break */
		CDEBUG0(5, "BREAK\n");
		genbrk(fn);
		return;
	}

	if (EQUALS(str, "EOT")) {
		CDEBUG0(5, "EOT\n");
		(void) (*Write)(fn, EOTMSG, strlen(EOTMSG));
		return;
	}

	if (EQUALS(str, "\"\"")) {
		CDEBUG0(5, "\"\"\n");
		str += 2;
	}

	bptr = buf;
	CDEBUG0(5, "sendthem (");
	for (sptr = str; *sptr; sptr++) {
		if (*sptr == '\\') {
			switch(*++sptr) {

			/* adjust switches */
			case 'c':	/* no CR after string */
				if (sptr[1] == NULLCHAR) {
					CDEBUG0(5, "<NO CR>");
					sendcr = 0;
				} else
					CDEBUG0(5, "<NO CR IGNORED>\n");
				continue;
			}

			/* stash in buf and continue */
			switch(*sptr) {
			case 'D':	/* raw phnum */
				strcpy(bptr, phstr1);
				bptr += strlen(bptr);
				continue;
			case 'T':	/* translated phnum */
				strcpy(bptr, phstr2);
				bptr += strlen(bptr);
				continue;
			case 'N':	/* null */
				*bptr++ = 0;
				continue;
			case 's':	/* space */
				*bptr++ = ' ';
				continue;
			case '\\':	/* backslash escapes itself */
				*bptr++ = *sptr;
				continue;
			default:	/* send the backslash */
				*bptr++ = '\\';
				*bptr++ = *sptr;	

			/* flush buf, perform action, and continue */
			case 'E':	/* echo check on */
				FLUSH();
				CDEBUG0(5, "ECHO CHECK ON\n");
				echocheck = 1;
				continue;
			case 'e':	/* echo check off */
				FLUSH();
				CDEBUG0(5, "ECHO CHECK OFF\n");
				echocheck = 0;
				continue;
			case 'd':	/* sleep briefly */
				FLUSH();
				CDEBUG0(5, "DELAY\n");
				sleep(2);
				continue;
			case 'p':	/* pause momentarily */
				FLUSH();
				CDEBUG0(5, "PAUSE\n");
				nap(HZ/4);	/* approximately 1/4 second */
				continue;
			case 'K':	/* inline break */
				FLUSH();
				CDEBUG0(5, "BREAK\n");
				genbrk(fn);
				continue;
			}
		} else
			*bptr++ = *sptr;
	}
	if (sendcr)
		*bptr++ = '\r';
	(void) wrstr(fn, buf, (int)(bptr - buf), echocheck);

err:
	CDEBUG0(5, ")\n");
	return;
}

#undef FLUSH
/********rmlock()	uucp:ulockf.c	2.7********/

/*
 * remove all lock files in list
 * return:
 *	none
 */
static void
rmlock(register char *name)
{
	register int i;
#ifdef V8
	char *cp;

	cp = rindex(name, '/');
	if (cp++ != CNULL)
	    if (strlen(cp) > DIRSIZ)
		*(cp+DIRSIZ) = NULLCHAR;
#endif /* V8 */


	for (i = 0; i < Nlocks; i++) {
		if (Lockfile[i] == NULL)
			continue;
		if (name == NULL || EQUALS(name, Lockfile[i])) {
			(void) unlink(Lockfile[i]);
			(void) free(Lockfile[i]);
			Lockfile[i] = NULL;
		}
	}
	return;
}
/********onelock()	uucp:ulockf.c	2.7********/

/*
 * makes a lock on behalf of pid.
 * input:
 *	pid - process id
 *	tempfile - name of a temporary in the same file system
 *	name - lock file name (full path name)
 * return:
 *	-1 - failed
 *	0  - lock made successfully
 */
static int
onelock(char *pid, char *tempfile, char *name)
{	
	register int fd;
	char	cb[100];

	fd=creat(tempfile, 0444);
	if(fd < 0){
		(void) sprintf(cb, "%s %s %d",tempfile, name, errno);
		logent("ULOCKC", cb);
		if((errno == EMFILE) || (errno == ENFILE))
			(void) unlink(tempfile);
		return(-1);
	}
	(void) write(fd, pid, SIZEOFPID+1);	/* +1 for '\n' */
	(void) chmod(tempfile,0444);
	(void) chown(tempfile, UUCPUID, UUCPGID);
	(void) close(fd);
	if(link(tempfile,name)<0){
		DEBUG(4, "%s: ", sys_errlist[errno]);
		DEBUG(4, "link(%s, ", tempfile);
		DEBUG(4, "%s)\n", name);
		if(unlink(tempfile)< 0){
			(void) sprintf(cb, "ULK err %s %d", tempfile,  errno);
			logent("ULOCKLNK", cb);
		}
		return(-1);
	}
	if(unlink(tempfile)<0){
		(void) sprintf(cb, "%s %d",tempfile,errno);
		logent("ULOCKF", cb);
	}
	return(0);
}
/********checkLock()	uucp:ulockf.c	2.7********/
#ifdef ATTSVKILL
/*
 * check to see if the lock file exists and is still active
 * - use kill(pid,0) - (this only works on ATTSV and some hacked
 * BSD systems at this time)
 * return:
 *	0	-> success (lock file removed - no longer active
 *	FAIL	-> lock file still active
 */
static int
checkLock(register char *file)
{
	register int ret;
	int lpid = -1;
	char alpid[SIZEOFPID+2];	/* +2 for '\n' and NULL */
	int fd;

	fd = open(file, O_RDONLY);
	DEBUG(4, "ulockf file %s\n", file);
	if (fd == -1) {
	    if (errno == ENOENT)  /* file does not exist -- OK */
		return(0);
	    DEBUG(4,"Lock File--can't read (errno %d) --remove it!\n", errno);
	    goto unlk;
	}
	ret = (int)read(fd, (char *) alpid, SIZEOFPID+1); /* +1 for '\n' */
	(void) close(fd);
	if (ret != (SIZEOFPID+1)) {

	    DEBUG0(4, "Lock File--bad format--remove it!\n");
	    goto unlk;
	}
	lpid = atoi(alpid);
	if ((ret=kill(lpid, 0)) == 0 || errno == EPERM) {
	    DEBUG0(4, "Lock File--process still active--not removed\n");
	    return(FAIL);
	}
	else { /* process no longer active */
	    DEBUG(4, "kill pid (%d), ", lpid);
	    DEBUG(4, "returned %d", ret);
	    DEBUG(4, "--ok to remove lock file (%s)\n", file);
	}
unlk:
	
	if (unlink(file) != 0) {
		DEBUG0(4,"ulockf failed in unlink()\n");
		return(FAIL);
	}
	return(0);
}
#else	/* !ATTSVKILL */

/*
 * check to see if the lock file exists and is still active
 * - consider the lock expired after SLCKTIME seconds.
 * return:
 *	0	-> success (lock file removed - no longer active
 *	FAIL	-> lock file still active
 */
static
checkLock(register char *file)
{
	register int ret;
	int lpid = -1;
	int fd;
	struct stat stbuf;
	time_t ptime, time();

	fd = open(file, 0);
	DEBUG(4, "ulockf file %s\n", file);
	if (fd == -1) {
	    if (errno == ENOENT)  /* file does not exist -- OK */
		return(0);
	    DEBUG(4,"Lock File--can't read (errno %d) --remove it!\n", errno);
	    goto unlk;
	}
	ret = stat(file, &stbuf);
	if (ret != -1) {
		(void) time(&ptime);
		if ((ptime - stbuf.st_ctime) < SLCKTIME) {

			/*
			 * file not old enough to delete
			 */
			return(FAIL);
		}
	}
unlk:
	DEBUG(4, "--ok to remove lock file (%s)\n", file);
	
	if (unlink(file) != 0) {
		DEBUG(4,"ulockf failed in unlink()\n","");
		return(FAIL);
	}
	return(0);
}
#endif	/* ATTSVKILL */
/********stlock()	uucp:ulockf.c	2.7********/
/*
 * put name in list of lock files
 * return:
 *	none
 */
static void
stlock(char *name)
{
	register int i;
	char *p;

#ifdef sgi
	if ((Lockfile == NULL) &&
	    (Lockfile = calloc(1, sizeof (char *) * MAXLOCKS)) == NULL)
		return;
#endif /* sgi */

	for (i = 0; i < Nlocks; i++) {
		if (Lockfile[i] == NULL)
			break;
	}
	DIE_UNLESS(i < MAXLOCKS, "TOO MANY LOCKS", "", i);
	if (i >= Nlocks)
		i = Nlocks++;
	p = calloc((unsigned) strlen(name) + 1, sizeof (char));
	DIE_UNLESS(p != NULL, "CAN NOT ALLOCATE FOR", name, 0);
	(void) strcpy(p, name);
	Lockfile[i] = p;
	return;
}
/********notin()	uucp:conn.c	2.13********/
/***
 *	notin(sh, lg)	check for occurrence of substring "sh"
 *	char *sh, *lg;
 *
 *	return codes:
 *		0  -  found the string
 *		1  -  not in the string
 */

static int
notin(char *sh, char *lg)
{
	while (*lg != NULLCHAR) {
		if (PREFIX(sh, lg))
			return(0);
		else
			lg++;
	}
	return(1);
}
/********wrstr()	uucp:conn.c	2.13********/
static int
wrstr(int fn, char *buf, int len, int echocheck)
{
	int 	i;
	char dbuf[BUFSIZ], *dbptr = dbuf;

	buf[len] = 0;
	if (echocheck)
		return(wrchr(fn, buf, len));

	if (Debug >= 5) {
		if (sysaccess(ACCESS_SYSTEMS) == 0) { /* Systems file access ok */
			for (i = 0; i < len; i++) {
				*dbptr = buf[i];
				if (*dbptr < 040) {
					*dbptr++ = '^';
					*dbptr = (char)(buf[i] | 0100);
				}
				dbptr++;
			}
			*dbptr = 0;
		} else
			strcpy(dbuf, "????????");
		CDEBUG(5, dbuf, 0);
	}
	if ((*Write)(fn, buf, (size_t) len) != len)
		return(FAIL);
	return(SUCCESS);
}
/********wrchr()	uucp:conn.c	2.13********/
static int
wrchr(register int fn, register char *buf, int len)
{
	int 	i, saccess;
	char	cin, cout;

	saccess = (sysaccess(ACCESS_SYSTEMS) == 0); /* protect Systems file */
	if (setjmp(Sjbuf))
		return(FAIL);
	(void) signal(SIGALRM, alarmtr);

	for (i = 0; i < len; i++) {
		cout = buf[i];
		if (saccess) {
			CDEBUG(5, "%s", cout < 040 ? "^" : "");
			CDEBUG(5, "%c", cout < 040 ? cout | 0100 : cout);
		} else
			CDEBUG0(5, "?");
		if (((*Write)(fn, &cout, 1)) != 1)
			return(FAIL);
		do {
			(void) alarm(MAXEXPECTTIME);
			if ((*Read)(fn, &cin, 1) != 1)
				return(FAIL);
			(void) alarm(0);
			cin &= 0177;
			if (saccess) {
				CDEBUG(5, "%s", cin < 040 ? "^" : "");
				CDEBUG(5, "%c", cin < 040 ? cin | 0100 : cin);
			} else
				CDEBUG0(5, "?");
		} while (cout != (cin & 0177));
	}
	return(SUCCESS);
}
/********fdig()	uucp:conn.c	2.13********/
/***
 *	char *
 *	fdig(cp)	find first digit in string
 *
 *	return - pointer to first digit in string or end of string
 */

static char *
fdig(char *cp)
{
	char *c;

	for (c = cp; *c; c++)
		if (*c >= '0' && *c <= '9')
			break;
	return(c);
}
/********genbrk()	cu:culine.c	2.5********/
static void
genbrk(register int fn)
{
	if (isatty(fn)) 
		(void) ioctl(fn, TCSBRK, 0);
}
/********twrite()	uucp:interface.c	1.9********/
#ifdef TLI

/*
 *	twrite - tli write routine
 */
#define	N_CHECK	100
static int
twrite(int fd, char *buf, unsigned nbytes)
{
	register int		i, ret;
	static int		n_writ, got_info;
	static struct t_info	info;

	if ( got_info == 0 ) {
		if ( t_getinfo(fd, &info) != 0 ) {
			tfaillog(fd, "twrite: t_getinfo\n");
			return(FAIL);
		}
		got_info = 1;
	}

	/* on every N_CHECKth call, check that are still in DATAXFER state */
	if ( ++n_writ == N_CHECK && t_getstate(fd) != T_DATAXFER )
		return(FAIL);

	if ( info.tsdu <= 0 || nbytes <= info.tsdu )
		return(t_snd(fd, buf, nbytes, NULL));

	/* if get here, then there is a limit on transmit size	*/
	/* (info.tsdu > 0) and buf exceeds it			*/
	i = ret = 0;
	while ( nbytes >= info.tsdu ) {
		if ( (ret = t_snd(fd,  &buf[i], info.tsdu, NULL)) != info.tsdu )
			return( ( ret >= 0 ? (i + ret) : ret ) );
		i += info.tsdu;
		nbytes -= info.tsdu;
	}
	if ( nbytes > 0 ) {
		if ( (ret = t_snd(fd,  &buf[i], nbytes, NULL)) != nbytes )
			return( ( ret >= 0 ? (i + ret) : ret ) );
		i += nbytes;
	}
	return(i);
}
#endif /* TLI */
/********tread()	uucp:interface.c	1.9********/
#ifdef TLI
/*
 *	tread - tli read routine
 */
static int
tread(int fd, char *buf, unsigned nbytes)
{
	int		rcvflags;

	return(t_rcv(fd, buf, nbytes, &rcvflags));

}
#endif /* TLI */
/********tioctl()	uucp:interface.c	1.9********/
#ifdef TLI
/*
 *	tioctl - stub for tli ioctl routine
 */
static int
tioctl(int fd, int request, int arg)
{
	return(SUCCESS);
}
#endif /* TLI */
/********nap()	uucp:conn.c	2.13********/
#ifdef FASTTIMER
/*	Sleep in increments of 60ths of second.	*/
static void
nap (register int time)
{
	static int fd;

	if (fd == 0)
		fd = open (FASTTIMER, 0);

	(*Read) (fd, 0, time);
}

#endif /* FASTTIMER */

#ifdef BSD4_2

	/* nap(n) -- sleep for 'n' ticks of 1/60th sec each. */
	/* This version uses the select system call */


static void
nap(unsigned n)
{
	struct timeval tv;
	int rc;

	if (n==0)
		return;
	tv.tv_sec = n/60;
	tv.tv_usec = ((n%60)*1000000L)/60;
	rc = select(32, 0, 0, 0, &tv);
}

#endif /* BSD4_2 */

#ifdef sgi
static void
nap(register unsigned n)
{
#if HZ != CLK_TIK
	sginap((long)(n*HZ+100/2)/100);
#else
	sginap(n);
#endif
}
#endif /* sgi */
#ifdef NONAP

/*	nap(n) where n is ticks
 *
 *	loop using n/HZ part of a second
 *	if n represents more than 1 second, then
 *	use sleep(time) where time is the equivalent
 *	seconds rounded off to full seconds
 *	NOTE - this is a rough approximation and chews up
 *	processor resource!
 */

static
nap(unsigned n)
{
	struct tms	tbuf;
	long endtime;
	int i;

	if (n > HZ) {
		/* > second, use sleep, rounding time */
		sleep( (int) (((n)+HZ/2)/HZ) );
		return;
	}

	/* use timing loop for < 1 second */
	endtime = times(&tbuf) + 3*n/4;	/* use 3/4 because of scheduler! */
	while (times(&tbuf) < endtime) {
	    for (i=0; i<1000; i++, i*i)
		;
	}
	return;
}


#endif /* NONAP */
/********BEGIN	uucp:sysfiles.c	1.9********/

/* so we can use strtok_r(), in case callers use strtok() across
 * calls to routines that call into these routines */
static char *lasttok;

/*
 * manage systems files (Systems, Devices, and Dialcodes families).
 *
 * also manage new file Devconfig, allows per-device setup.
 * present use is to specify what streams modules to push/pop for
 * AT&T TLI/streams network.  see tssetup() in interface.c for details.
 *
 * TODO:
 *    call bsfix()?
 *    combine the 3 versions of everything (sys, dev, and dial) into one.
 *    allow arbitrary classes of service.
 *    need verifysys() for uucheck.
 *    nameserver interface?
 *    pass sysname (or 0) to getsysline().  (might want reg. exp. or NS processing 
 */

/* private variables */
static void tokenize(void), nameparse(), setfile(), setioctl(),
	scansys(), scancfg();
static int namematch(), nextdialers(), nextsystems(), getline();

/* pointer arrays might be dynamically allocated */
static struct uucpinfo {
	char *Systems[64];	/* list of Systems files */
	char *Devices[64];	/* list of Devices files */
	char *Dialers[64];	/* list of Dialers files */
	char *Pops[64];		/* list of STREAMS modules to be popped */
	char *Pushes[64];	/* list of STREAMS modules to be pushed */
	int nsystems;		/* index into list of Systems files */
	int ndevices;		/* index into list of Devices files */
	int ndialers;		/* index into list of Dialers files */
	int npops;		/* index into list of STREAMS modules */
							/*to be popped */
	int npushes;		/* index into list of STREAMS modules */
							/*to be pushed */
	FILE *fsystems;
	FILE *fdevices;
	FILE *fdialers;

	/* this might be dynamically allocated */
#define NTOKENS 16
	char *tokens[NTOKENS], **tokptr;
} *UUCPINFO = NULL;

#define Systems		UUCPINFO->Systems
#define Devices		UUCPINFO->Devices
#define Dialers		UUCPINFO->Dialers
#define Pops		UUCPINFO->Pops
#define Pushes		UUCPINFO->Pushes
#define nsystems	UUCPINFO->nsystems
#define ndevices	UUCPINFO->ndevices
#define ndialers	UUCPINFO->ndialers
#define npops		UUCPINFO->npops
#define npushes		UUCPINFO->npushes
#define fsystems	UUCPINFO->fsystems
#define fdevices	UUCPINFO->fdevices
#define fdialers	UUCPINFO->fdialers
#define tokens		UUCPINFO->tokens
#define tokptr		UUCPINFO->tokptr

static int
UUCPcreate(void)
{
	if (UUCPINFO == NULL) {
		UUCPINFO = (struct uucpinfo *)
				calloc(1, sizeof (struct uucpinfo));
		if (UUCPINFO == NULL)
			return 0;
	}

	return 1;
}

/*
 * setservice init's Systems, Devices, Dialers lists from Sysfiles
 */
static void
setservice(char *service)
{
	scansys(service);
	return;
}

/*
 * setdevcfg init's Pops, Pushes lists from Devconfig
 */

static void
setdevcfg(char *service, char *device)
{
	scancfg(service, device);
	return;
}

/*	administrative files access */
static int
sysaccess(int type)
{
	if (!UUCPcreate())
		return (FAIL);
	switch (type) {

	case ACCESS_SYSTEMS:
		return(access(Systems[nsystems], R_OK));
	case ACCESS_DEVICES:
		return(access(Devices[ndevices], R_OK));
	case ACCESS_DIALERS:
		return(access(Dialers[ndialers], R_OK));
	case EACCESS_SYSTEMS:
		return(eaccess(Systems[nsystems], R_OK));
	case EACCESS_DEVICES:
		return(eaccess(Devices[ndevices], R_OK));
	case EACCESS_DIALERS:
		return(eaccess(Dialers[ndialers], R_OK));
	default:
		{
		char errformat[64];
		(void)sprintf(errformat, "bad access type %d", type);
		logent(errformat, "sysaccess");
		return(FAIL);
		}
	}
}


/*
 * read Sysfiles, set up lists of Systems/Devices/Dialers file names.
 * allow multiple entries for a given service, allow a service
 * type to describe resources more than once, e.g., systems=foo:baz systems=bar.
 */
static void
scansys(char *service)
{	FILE *f;
	char *tok, buf[BUFSIZ];

	if (!UUCPcreate())
		return;
	Systems[0] = Devices[0] = Dialers[0] = NULL;
	lasttok = NULL;
	if ((f = fopen(SYSFILES, "r")) != 0) {
		while (getline(f, buf) > 0) { 
			/* got a (logical) line from Sysfiles */
			/* strtok's of this buf continue in tokenize() */
			tok = strtok_r(buf, " \t", &lasttok);
			if (namematch("service=", tok, service)) {
				tokenize();
				nameparse();
			}
		}
#ifdef sgi
		fclose(f);
#endif /* sgi */
	}

	/* if didn't find entries in Sysfiles, use defaults */
	if (Systems[0] == NULL) {
		Systems[0] = strsave(SYSTEMS);
		DIE_UNLESSS(Systems[0] != NULL, "CAN'T ALLOCATE", "scansys: Systems",
			Systems[0]);
		Systems[1] = NULL;
	}
	if (Devices[0] == NULL) {
		Devices[0] = strsave(DEVICES);
		DIE_UNLESSS(Devices[0] != NULL, "CAN'T ALLOCATE", "scansys: Devices",
			Devices[0]);
		Devices[1] = NULL;
	}
	if (Dialers[0] == NULL) {
		Dialers[0] = strsave(DIALERS);
		DIE_UNLESSS(Dialers[0] != NULL, "CAN'T ALLOCATE", "scansys: Dialers",
			Dialers[0]);
		Dialers[1] = NULL;
	}
	return;
}


/*
 * read Devconfig.  allow multiple entries for a given service, allow a service
 * type to describe resources more than once, e.g., push=foo:baz push=bar.
 */
static void
scancfg(char *service, char *device)
{	FILE *f;
	char *tok, buf[BUFSIZ];

	if (!UUCPcreate())
		return;
	npops = npushes = 0;
	Pops[0] = Pushes[0] = NULL;
	lasttok = NULL;
	if ((f = fopen(DEVCONFIG, "r")) != 0) {
		while (getline(f, buf) > 0) {
			/* got a (logical) line from Devconfig */
			/* strtok's of this buf continue in tokenize() */
			tok = strtok_r(buf, " \t", &lasttok);
			if (namematch("service=", tok, service)) {
				tok = strtok_r((char *)0, " \t", &lasttok);
				if ( namematch("device=", tok, device)) {
					tokenize();
					nameparse();
				}
			}
		}
#ifdef sgi
		fclose(f);
#endif /* sgi */
	}

	return;

}

/*
 *  given a file pointer and buffer, construct logical line in buffer
 *  (i.e., concatenate lines ending in '\').  return length of line
 *  ASSUMES that buffer is BUFSIZ long!
 */

static int
getline(FILE *f, char *line)
{	
	char *lptr, *lend;

	lptr = line;
	while (fgets(lptr, (int)((line + BUFSIZ) - lptr), f) != NULL) {
		lend = lptr + strlen(lptr);
		if (lend == lptr || lend[-1] != '\n')	
			/* empty buf or line too long! */
			break;
		*--lend = '\0'; /* lop off ending '\n' */
		if ( lend == line ) /* empty line - ignore */
			continue;
		lptr = lend;
		if (lend[-1] != '\\')
			break;
		/* continuation */
		lend[-1] = ' ';
	}
	return((int)(lptr - line));
}

/*
 * given a label (e.g., "service=", "device="), a name ("cu", "uucico"),
 *  and a line:  if line begins with the label and if the name appears
 * in a colon-separated list of names following the label, return true;
 * else return false
 */
static int
namematch(char *label, char *line, char *name)
{	
	char *lend;

	if (strncmp(label, line, strlen(label)) != SAME) {
		return(FALSE);	/* probably a comment line */
	}
	line += strlen(label);
	if (*line == '\0')
		return(FALSE);
	/*
	 * can't use strtok() in the following because scansys(),
	 * scancfg() do an initializing call to strtok() before
	 * coming here and then CONTINUE calling strtok() in tokenize(),
	 * after returning from namematch().
	 * (could switch to strtok_r now, but not worth it).
	 */
	while ((lend = strchr(line, ':')) != NULL) {
		*lend = '\0';
		if (strcmp(line, name) == SAME)
			return(TRUE);
		line = lend+1;
	}
	return(strcmp(line, name) == SAME);
}

/*
 * tokenize() continues pulling tokens out of a buffer -- the
 * initializing call to strtok must have been made before calling
 * tokenize() -- and starts stuffing 'em into tokptr.
 * uses global static (to this file) so we can use strtok_r();
 * the routines that call tokenize() aren't re-entrant anyway.
 */
static void
tokenize(void)
{	
	char *tok;

	if (!UUCPcreate())
		return;
	tokptr = tokens;
	while ((tok = strtok_r((char *) NULL, " \t", &lasttok)) != NULL) {
		*tokptr++ = tok;
		if (tokptr - tokens >= NTOKENS)
			break;
	}
	*tokptr = NULL;
}

/*
 * look at top token in array: should be line of the form
 *	name=item1:item2:item3...
 * if name is one we recognize, then call set[file|ioctl] to set up 
 * corresponding list.  otherwise, log bad name.
 */
static void
nameparse(void)
{	
	char **line, *equals;

	if (!UUCPcreate())
		return;
	for (line = tokens; (line - tokens) < NTOKENS && *line; line++) {
		equals = strchr(*line, '=');
		if (equals == NULL)
			continue;	/* may be meaningful someday? */
		*equals = '\0';
		/* ignore entry with empty rhs */
		if (*++equals == '\0')
			continue;
		if (strcmp(*line, "systems") == SAME)
			setfile(Systems, equals);
		else if (strcmp(*line, "devices") == SAME)
			setfile(Devices, equals);
		else if (strcmp(*line, "dialers") == SAME)
			setfile(Dialers, equals);
		else if (strcmp(*line, "pop") == SAME)
			setioctl(Pops, equals);
		else if (strcmp(*line, "push") == SAME)
			setioctl(Pushes, equals);
		else {
			char errformat[2000];
			(void)sprintf(errformat,"unrecognized label %s",*line);
			logent(errformat, "Sysfiles|Devconfig");
		}
	}
}

/*
 * given the list for a particular type (systems, devices,...)
 * and a line of colon-separated files, add 'em to list
 */

static void
setfile(char **type, char *line)
{	
	char **tptr, *tok;
	char expandpath[BUFSIZ];
	char *lasttok;

	if (*line == 0)
		return;
	tptr = type;
	while (*tptr)		/* skip over existing entries to*/
		tptr++;		/* concatenate multiple entries */

	lasttok = NULL;
	for (tok = strtok_r(line, ":", &lasttok); tok != NULL;
	tok = strtok_r((char *) NULL, ":", &lasttok)) {
		expandpath[0] = '\0';
		if ( *tok != '/' )
			/* by default, file names are relative to SYSDIR */
			sprintf(expandpath, "%s/", SYSDIR);
		strcat(expandpath, tok);
		if (eaccess(expandpath, 4) != 0)
			/* if we can't read it, no point in adding to list */
			continue;
		*tptr = strsave(expandpath);
		DIE_UNLESSS(*tptr != NULL, "CAN'T ALLOCATE", "setfile: tptr", *tptr);
		tptr++;
	}
}

/*
 * given the list for a particular ioctl (push, pop)
 * and a line of colon-separated modules, add 'em to list
 */

static void
setioctl(char **type, char *line)
{	
	char **tptr, *tok;
	char *lasttok;

	if (*line == 0)
		return;
	tptr = type;
	while (*tptr)		/* skip over existing entries to*/
		tptr++;		/* concatenate multiple entries */
	lasttok = NULL;
	for (tok = strtok_r(line, ":", &lasttok); tok != NULL;
	tok = strtok_r((char *) NULL, ":", &lasttok)) {
		*tptr = strsave(tok);
		DIE_UNLESSS(*tptr != NULL, "CAN'T ALLOCATE", "setioctl: tptr", *tptr);
		tptr++;
	}
}



/*
 * reset Systems files
 */
static void
sysreset(void)
{
	if (!UUCPcreate())
		return;
	if (fsystems)
		fclose(fsystems);
	fsystems = NULL;
	nsystems = 0;
	devreset();
}

/*
 * reset Devices files
 */
static void		
devreset(void)
{
	if (!UUCPcreate())
		return;
	if (fdevices)
		fclose(fdevices);
	fdevices = NULL;
	ndevices = 0;
	dialreset();
}

/*
 * reset Dialers files
 */
static void		
dialreset(void)
{
	if (!UUCPcreate())
		return;
	if (fdialers)
		fclose(fdialers);
	fdialers = NULL;
	ndialers = 0;
}

/*
 * get next line from Systems file
 * return TRUE if successful, FALSE if not
 */
static int
getsysline(char *buf, int len)
{
	if (!UUCPcreate())
		return (FALSE);
	if (Systems[0] == NULL)
		/* not initialized via setservice() - use default */
		setservice("uucico");

	/* initialize devices and dialers whenever a new line is read */
	/* from systems */
	devreset();
	if (fsystems == NULL)
		if (nextsystems() == FALSE)
			return(FALSE);
	for(;;) {
		if (fgets(buf, len, fsystems) != NULL)
			return(TRUE);
		if (nextsystems() == FALSE)
			return(FALSE);
	}
}

/*
 * move to next systems file.  return TRUE if successful, FALSE if not
 */
static int
nextsystems(void)
{
	if (!UUCPcreate())
		return (FALSE);
	devreset();

	if (fsystems != NULL) {
		(void) fclose(fsystems);
		nsystems++;
	} else {
		nsystems = 0;
	}
	for ( ; Systems[nsystems] != NULL; nsystems++)
		if ((fsystems = fopen(Systems[nsystems], "r")) != NULL)
			return(TRUE);
	return(FALSE);
}
		
/*
 * get next line from Devices file
 * return TRUE if successful, FALSE if not
 */
static int
getdevline(char *buf, int len)
{
	if (!UUCPcreate())
		return (FALSE);
	if (Devices[0] == NULL)
		/* not initialized via setservice() - use default */
		setservice("uucico");

	if (fdevices == NULL)
		if (nextdevices() == FALSE)
			return(FALSE);
	for(;;) {
		if (fgets(buf, len, fdevices) != NULL)
			return(TRUE);
		if (nextdevices() == FALSE)
			return(FALSE);
	}
}

/*
 * move to next devices file.  return TRUE if successful, FALSE if not
 */
static int
nextdevices(void)
{
	if (!UUCPcreate())
		return (FALSE);
	if (fdevices != NULL) {
		(void) fclose(fdevices);
		ndevices++;
	} else {
		ndevices = 0;
	}
	for ( ; Devices[ndevices] != NULL; ndevices++)
		if ((fdevices = fopen(Devices[ndevices], "r")) != NULL)
			return(TRUE);
	return(FALSE);
}

		
/*
 * get next line from Dialers file
 * return TRUE if successful, FALSE if not
 */

static int
getdialline(char *buf, int len)
{
	if (Dialers[0] == NULL)
		/* not initialized via setservice() - use default */
		setservice("uucico");

	if (fdialers == NULL)
		if (nextdialers() == FALSE)
			return(FALSE);
	for(;;) {
		if (fgets(buf, len, fdialers) != NULL)
			return(TRUE);
		if (nextdialers() == FALSE)
			return(FALSE);
	}
}

/*
 * move to next dialers file.  return TRUE if successful, FALSE if not
 */
static int
nextdialers(void)
{
	if (!UUCPcreate())
		return (FALSE);
	if (fdialers) {
		(void) fclose(fdialers);
		ndialers++;
	} else {
		ndialers = 0;
	}
	
	for ( ; Dialers[ndialers] != NULL; ndialers++)
		if ((fdialers = fopen(Dialers[ndialers], "r")) != NULL)
			return(TRUE);
	return(FALSE);
}

#ifdef TLIS
/*
 * get next module to be popped
 * return TRUE if successful, FALSE if not
 */
static int
getpop(char *buf, int len, int *optional)
{
	int slen;

	if (!UUCPcreate())
		return (FALSE);
	if ( Pops[0] == NULL || Pops[npops] == NULL )
		return(FALSE);

	/*	if the module name is enclosed in parentheses,	*/
	/*	is optional. set flag & strip parens		*/
	slen = strlen(Pops[npops]) - 1;
	if ( Pops[npops][0] == '('  && Pops[npops][slen] == ')' ) {
		*optional = 1;
		--slen;
		strncpy(buf, &(Pops[npops++][1]), (slen <= len ? slen : len) );
	} else {
		*optional = 0;
		strncpy(buf, Pops[npops++], len);
	}
	return(TRUE);
}

/*
 * get next module to be pushed
 * return TRUE if successful, FALSE if not
 */
static int
getpush(char *buf, int len)
{
	if (!UUCPcreate())
		return (FALSE);
	if ( Pushes[0] == NULL || Pushes[npushes] == NULL )
		return(FALSE);
	strncpy(buf, Pushes[npushes++], len);
	return(TRUE);
}
#endif /* TLIS */

/********END	uucp:sysfiles.c	1.9********/
/********eaccess()	uucp:strsave.c	1.2********/
static int
eaccess(char *path, register int amode )
{
	struct stat	s;

	if( stat( path, &s ) == -1 )
		return  -1;
	amode &= 07;
	if( !amode )
		return  0;		/* file exists */

	if((amode & (int)s.st_mode) == amode )
		return  0;		/* access permitted by "other" mode */

	amode <<= 3;
	if( getegid() == s.st_gid  &&  (amode & (int)s.st_mode) == amode )
		return  0;		/* access permitted by group mode */

	amode <<= 3;
	if( geteuid() == s.st_uid  &&  (amode & (int)s.st_mode) == amode )
		return  0;		/* access permitted by owner mode */

	errno = EACCES;
	return  -1;
}
/********strsave()	uucp:strsave.c	1.2********/

/* copy str into data space -- caller should report errors. */

static char *
strsave(register char *str)
{
	register char *rval;
#ifndef sgi
	extern char *malloc();
#endif

	rval = malloc(strlen(str) + 1);
	if (rval != 0)
		strcpy(rval, str);
	return(rval);
}
