/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright(c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved.					*/

#ident	"@(#)sh:defs.h	1.15.22.2"
/*
 *	UNIX shell
 */

/* execute flags */
#define 	XEC_EXECED	01
#define 	XEC_LINKED	02
#define 	XEC_NOSTOP	04

/* endjobs flags */
#define		JOB_STOPPED	01
#define		JOB_RUNNING	02

/* error exits from various parts of shell */
#define 	ERROR		1
#define 	SYNBAD		2
#define 	SIGFAIL 	2000
#define	 	SIGFLG		0200

/* command tree */
#define 	FPIN		0x0100
#define 	FPOU		0x0200
#define 	FAMP		0x0400
#define 	COMMSK		0x00F0
#define		CNTMSK		0x000F

#define 	TCOM		0x0000
#define 	TPAR		0x0010
#define 	TFIL		0x0020
#define 	TLST		0x0030
#define 	TIF			0x0040
#define 	TWH			0x0050
#define 	TUN			0x0060
#define 	TSW			0x0070
#define 	TAND		0x0080
#define 	TORF		0x0090
#define 	TFORK		0x00A0
#define 	TFOR		0x00B0
#define		TFND		0x00C0
#define		TBRA		0x00D0

/* execute table */
#define 	SYSSET		1
#define 	SYSCD		2
#define 	SYSEXEC		3

#ifdef RES	/*	include login code	*/
#define 	SYSLOGIN	4
#else
#define 	SYSNEWGRP 	4
#endif

#define 	SYSTRAP		5
#define 	SH_SYSEXIT	6
#define 	SYSSHFT 	7
#define 	SYSWAIT		8
#define 	SYSCONT 	9
#define 	SYSBREAK	10
#define 	SYSEVAL 	11
#define 	SYSDOT		12
#define 	SYSRDONLY 	13
#define 	SYSTIMES 	14
#define 	SYSXPORT	15
#define 	SYSNULL 	16
#define 	SYSREAD 	17
#define		SYSTST		18

#ifndef RES	/*	exclude umask code	*/
#define 	SYSUMASK 	20
#define 	SYSULIMIT 	21
#endif

#define 	SYSECHO		22
#define		SYSHASH		23
#define		SYSPWD		24
#define 	SYSRETURN	25
#define		SYSUNS		26
#define		SYSMEM		27
#define		SYSTYPE  	28
#define		SYSGETOPT	29
#define 	SYSJOBS		30
#define 	SYSFGBG		31
#define 	SYSKILL		32
#define 	SYSSUSP		33
#define 	SYSSTOP		34
#define		SYSPRIV		35
#define		SYSMLD		36
#if defined(sgi) && !defined(RES)
#define		SYSLIMIT	37
#define		SYSUNLIMIT	38
#endif

/* used for input and output of shell */
#define 	INIO 		19

/*io nodes*/
#define 	USERIO		10
#define 	IOUFD		15
#define 	IODOC		16
#define 	IOPUT		32
#define 	IOAPP		64
#define 	IOMOV		128
#define 	IORDW		256
#define		IOSTRIP		512
#define 	INPIPE		0
#define 	OTPIPE		1

/* arg list terminator */
#define 	ENDARGS		0

#include	"mac.h"
#include	"mode.h"
#include	"name.h"
#include	<signal.h>
#include	<sys/types.h>

/* id's */
extern pid_t	mypid;
extern pid_t	mypgid;
extern pid_t	mysid;
extern pid_t	svpgid;
extern pid_t	svtgid;

/* getopt */

extern int		optind;
extern int		opterr;
extern int 		_sp;
extern char 		*optarg;

/* result type declarations */

#define 	alloc 		malloc

#ifdef __STDC__
extern void *alloc();
#else
extern char *alloc();
#endif

extern time_t time();			/*libc*/

/*External functions defined within shell source modules*/

/*From args.c*/
extern int options();
extern void setargs();
extern void clearup();
extern void restorargs();
extern struct dolnod *savargs();
extern struct dolnod *useargs();
extern struct dolnod *freeargs();

/*From blok.c*/
extern void free();
extern void addblok();

/*From bltin.c*/
extern void builtin();

/*From cmd.c*/
extern struct trenod *cmd();
extern struct trenod *makefork();

/*From error.c*/
extern void failed();
extern void error();
extern void error_fail();
extern void exitsh();
extern void rmtemp();
extern void rmfunctmp();
extern void failure();
extern void prusage();
extern void pr_usage();
extern int set_label();

/*From expand.c*/
extern int expand();
extern void makearg();

/*From fault.c*/
extern int handle();
extern void mysleep();
extern void chktrap();
extern void done();
extern void stdsigs();
extern void oldsigs();


/*From func.c*/
extern void prf();
extern void prcmd();
extern void freefunc();

/*From hashserv.c*/
extern int findpath();

/*From io.c*/
extern int estabf();
extern int pop();
extern int poptemp();
extern int chkopen();
extern int create();
extern int tmpfil();
extern int savefd();
extern void initf();
extern void push();
extern void chkpipe();
extern void renam();
extern void copy();
extern void link_iodocs();
extern void swap_iodoc_nm();
extern void restore();

/*From jobs.c*/
extern pid_t tcgetpgrp();
extern int endjobs();
extern void allocjob();
extern void deallocjob();
extern void freejobs();
extern void startjobs();
extern void clearjobs();
extern void makejob();
extern void postjob();

/*From macro.c*/
extern unsigned char *macro();
extern void subst();

/*From main.c*/
extern void chkpr();
extern void settmp();
extern void setmail();
extern void setwidth();
extern void setmode();

/*From name.c*/
extern unsigned char *make();
extern unsigned char **setenv();
extern struct namnod *lookup();
extern struct namnod *findnam();
extern int readvar();
extern int syslook();
extern void printnam();
extern void printro();
extern void printexp();
extern void assign();
extern void namscan();
extern void setlist();
extern void replace();
extern void dfault();
extern void assnum();
extern void setup_env();
extern void unset_name();
extern void setup_tfm();

/*from print.c*/
extern void prc_buff();
extern void prs_buff();
extern void prn_buff();
extern void prs_cntl();
extern void prn_buff();
extern void prll_buff();
extern void prl_buff();
extern void prs_cntl();
extern void prp();
extern void prs();
extern void prc();
extern void prt();
extern void prn();
extern void itos();
extern void flushb();
extern int stoi();
extern int lltos();
extern int ltos();
extern int setb();

/*From profile.c*/
extern void monitor();

/*From prv.c*/
extern int clrprivs();
extern void rstprivs();

/*From pwd.c*/
extern unsigned char *cwdget();
extern void cwdprint();
extern void cwd();

/*From setbrk.c*/
extern unsigned char *setbrk();

/*From service.c*/
extern unsigned char *simple();
extern unsigned char *mactrim();
extern unsigned char *catpath();
extern unsigned char *getpath();
extern unsigned char *nextpath();
extern unsigned char **scan();
extern int initio();
extern int pathopen();
extern int getarg();
extern void execa();
extern void trim();
#ifdef ACCT
extern void suspacct();
extern void preacct();
extern void doacct();
#endif

/*From stak.c*/
extern unsigned char *locstak();
extern unsigned char *savstak();
extern unsigned char *endstak();
extern void tdystak();
extern void stakchk();

/*From string.c*/
extern unsigned char *movstr();
extern unsigned char *movstrn();
extern void itos();
extern int any();
extern int anys();
extern int cf();
extern int length();
extern int stoi();

/*From word.c*/
extern int word();
extern unsigned char readc();
extern unsigned char nextc();
extern unsigned char skipc();
extern unsigned char *readw();

/*From xec.c*/
extern void execexp();
extern int execute();


/*Macros*/
#define 	attrib(n,f)		(n->namflg |= f)
#define 	round(a,b)		(((int)(((char *)(a)+b)-1))&~((b)-1))
#define 	closepipe(x)	(close(x[INPIPE]), close(x[OTPIPE]))
#define 	eq(a,b)			(cf(a,b)==0)
#define 	max(a,b)		((a)>(b)?(a):(b))
#define 	assert(x)		;

/* temp files and io */
extern int				output;
extern int				ioset;
extern struct ionod		*iotemp;	/* files to be deleted sometime */
extern struct ionod		*fiotemp;	/* function files to be deleted sometime */
extern struct ionod		*iopend;	/* documents waiting to be read at NL */
extern struct fdsave	fdmap[];
extern int savpipe;

/* substitution */
extern int				dolc;
extern unsigned char				**dolv;
extern struct dolnod	*argfor;
extern struct argnod	*gchain;

/* stak stuff */
#include		"stak.h"

/* string constants */
extern const char				atline[], atlineid[];
extern const char				readmsg[];
extern const char				colon[], colonid[];
extern const char				minus[];
extern const char				nullstr[];
extern const char				sptbnl[];
extern const char				unexpected[], unexpectedid[];
extern const char				endoffile[], endoffileid[];
extern const char				synmsg[], synmsgid[];

/* name tree and words */
extern const struct sysnod	reserved[];
extern const int				no_reserved;
extern const struct sysnod	commands[];
extern const int				no_commands;
extern const struct sysnod	ksh_commands[];
extern const int				no_ksh_commands;

extern int				wdval;
extern int				wdnum;
extern int				fndef;
extern int				nohash;
extern struct argnod	*wdarg;
extern int				wdset;
extern BOOL				reserv;

/* prompting */
extern const char				stdprompt[];
extern const char				supprompt[];
extern const char				profile[];
extern const char				sysprofile[];

/* built in names */
extern struct namnod	cdpnod;
extern struct namnod	ifsnod;
extern struct namnod	homenod;
extern struct namnod	mailnod;
extern struct namnod	pathnod;
extern struct namnod	ps1nod;
extern struct namnod	ps2nod;
extern struct namnod	mchknod;
extern struct namnod	acctnod;
extern struct namnod	mailpnod;
extern struct namnod	tfadminnod;
extern struct namnod	tmoutnod;

/* special names */
extern unsigned char				flagadr[];
extern unsigned char				*pcsadr;
extern unsigned char				*pidadr;
extern unsigned char				*cmdadr;

extern const char				defpath[];

/* names always present */
extern const char				mailname[];
extern const char				homename[];
extern const char				pathname[];
extern const char				cdpname[];
extern const char				ifsname[];
extern const char				ps1name[];
extern const char				ps2name[];
extern const char				mchkname[];
extern const char				acctname[];
extern const char				mailpname[];
extern const char				tfadminname[];
extern const char				timeoutname[];

/* transput */
extern unsigned char				tmpout[];
extern unsigned char				*tmpname;
extern int				serial;

#define		TMPNAM 		7

extern struct fileblk	*standin;

#define 	input		(standin->fdes)
#define 	eof			(standin->feof)

extern unsigned int			peekc;
extern unsigned int			peekn;
extern unsigned char				*comdiv;
extern const char				devnull[];

/* flags */
#define		noexec		01
#define		sysflg		01
#define		intflg		02
#define		prompt		04
#define		setflg		010
#define		errflg		020
#define		ttyflg		040
#define		forked		0100
#define		oneflg		0200
#define		rshflg		0400
#define		subsh		01000
#define		stdflg		02000
#define		STDFLG		's'
#define		execpr		04000
#define		readpr		010000
#define		keyflg		020000
#define		hashflg		040000
#define		nofngflg	0200000
#define		exportflg	0400000
#define		monitorflg	01000000
#define		jcflg		02000000
#define		privflg		04000000
#define		forcexit	010000000
#define		jcoff		020000000

extern long				flags;
extern int				rwait;	/* flags read waiting */

/* error exits from various parts of shell */
#include	<setjmp.h>
extern jmp_buf			subshell;
extern jmp_buf			errshell;

/* fault handling */
#include	"brkincr.h"

extern unsigned			brkincr;
#define 	MINTRAP		0
#define 	MAXTRAP		NSIG

#define 	TRAPSET		2
#define 	SIGSET		4
#define		SIGMOD		8
#define		SIGIGN		16

extern BOOL				trapnote;

/* name tree and words */
extern unsigned char				**environ;
extern unsigned char				numbuf[];
extern const char				export[], exportid[];
extern const char				readonly[], readonlyid[];

/* execflgs */
extern int				exitval;
extern int				retval;
extern BOOL				execbrk;
extern int				loopcnt;
extern int				breakcnt;
extern int				funcnt;

/* messages */
extern const char				nopset[], nopsetid[];
extern const char				setprv[], setprvid[];
extern const char				clrprv[], clrprvid[];
extern const char				setnam[], setnamid[];
extern const char				prvnam[], prvnamid[];
extern const char				getpriv[], getprivid[];
extern const char				rstpriv[], rstprivid[];
extern const char				prvunsup[], prvunsupid[];
extern const char				mldusage[], mldusageid[];
extern const char				nosetmode[], nosetmodeid[];
extern const char				nogetmode[], nogetmodeid[];
extern const char				notsupport[], notsupportid[];
extern const char				invalcomb[], invalcombid[];
extern const char				mldmodeis[], mldmodeisid[];
extern const char				mailmsg[], mailmsgid[];
extern const char				coredump[], coredumpid[];
extern const char				badopt[], badoptid[];
extern const char				badparam[], badparamid[];
extern const char				unset[], unsetid[];
extern const char				badsub[], badsubid[];
extern const char				nospace[], nospaceid[];
extern const char				notfound[], notfoundid[];
extern const char				badtrap[], badtrapid[];
extern const char				baddir[], baddirid[];
extern const char				badshift[], badshiftid[];
extern const char				restricted[], restrictedid[];
extern const char				execpmsg[], execpmsgid[];
extern const char				notid[], notidid[];
extern const char 				badulimit[], badulimitid[];
extern const char				wtfailed[], wtfailedid[];
extern const char				badcreate[], badcreateid[];
extern const char				nofork[], noforkid[];
extern const char				noswap[], noswapid[];
extern const char				piperr[], piperrid[];
extern const char				badopen[], badopenid[];
extern const char				badnum[], badnumid[];
extern const char				badsig[], badsigid[];
extern const char				badid[], badidid[];
extern const char				arglist[], arglistid[];
extern const char				txtbsy[], txtbsyid[];
extern const char				toobig[], toobigid[];
extern const char				badexec[], badexecid[];
extern const char				badfile[], badfileid[];
extern const char				badreturn[], badreturnid[];
extern const char				badexport[], badexportid[];
extern const char				badunset[], badunsetid[];
extern const char				nohome[], nohomeid[];
extern const char				badperm[], badpermid[];
extern const char				mssgargn[], mssgargnid[];
extern const char				libacc[], libaccid[];
extern const char				libbad[], libbadid[];
extern const char				libscn[], libscnid[];
extern const char				libmax[], libmaxid[];
extern const char                             emultihop[], emultihopid[];
extern const char                             nulldir[], nulldirid[];
extern const char                             enotdir[], enotdirid[];
extern const char                             enoent[], enoentid[];
extern const char                             eacces[], eaccesid[];
extern const char                             enolink[], enolinkid[];
extern const char				exited[], exitedid[];
extern const char				running[], runningid[];
extern const char				ambiguous[], ambiguousid[];
extern const char				nosuchjob[], nosuchjobid[];
extern const char				nosuchpid[], nosuchpidid[];
extern const char				nosuchpgid[], nosuchpgidid[];
extern const char				usage[], badusage[];
extern const char				nojc[], nojcid[];
extern const char				killuse[], killuseid[];
extern const char				jobsuse[], jobsuseid[];
extern const char				stopuse[], stopuseid[];
extern const char				ulimuse[], ulimuseid[];
extern const char				nocurjob[], nocurjobid[];
extern const char				loginsh[], loginshid[];
extern const char				jobsstopped[], jobsstoppedid[];
extern const char				jobsrunning[], jobsrunningid[];
#ifdef sgi
extern const char				nosuid[], nosuidid[];
extern const char				badmagic[], badmagicid[];
extern const char				nosuchres[], nosuchresid[];
extern const char				badscale[], badscaleid[];
#endif
extern const char				posix_builtin[], posix_builtin_id[];



/*	'builtin' error messages	*/

extern const char				badop[], badopid[];

/*	fork constant	*/

#define 	FORKLIM 	32

#include	"ctype.h"
#include	<ctype.h>
#include	<locale.h>

extern int				eflag;
extern int				ucb_builtins;

/* Runtime timeout value	*/

extern unsigned int	timeout;

/*
 * Find out if it is time to go away.
 * `trapnote' is set to SIGSET when fault is seen and
 * no trap has been set.
 */

#define		sigchk()	if (trapnote & SIGSET)	\
							exitsh(exitval ? exitval : SIGFAIL)

#define 	exitset()	retval = exitval

/* Multibyte characters */
void setwidth();
unsigned char *readw(); 
#include <stdlib.h>
#include <limits.h>
#define multibyte (MB_CUR_MAX>1)
#define MULTI_BYTE_MAX MB_LEN_MAX

/* Extra command definition for commands sharing the same code */
#define SYSFG		101
#define SYSBG		102

/* Error printing functions */
#ifdef __STDC__
void failed(int, const unsigned char *, const char *, const char *);
void error(int, const char *, const char *);
void error_fail(int, const char *, const char *);
void failure(int, const unsigned char *, const char *, const char *);
void prusage(int, const char *, const char *);
#else
void failed();
void error();
void failure();
void prusage();
void warning();
void error_fail();
#endif
