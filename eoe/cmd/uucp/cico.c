/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.10 $"

/*

 * uucp file transfer program:
 * to place a call to a remote machine, login, and
 * copy files between the two machines.

*/
#include "uucp.h"
#ifdef sgi
#include <paths.h>
#endif

#ifndef	V7
#include <sys/sysmacros.h>
#endif /* V7 */

#ifdef TLI
#include	<sys/tiuser.h>
#endif /* TLI */

jmp_buf Sjbuf;
extern int Errorrate;
char	uuxqtarg[MAXBASENAME] = {'\0'};

extern int	(*Setup)(), (*Teardown)();	/* defined in interface.c */

#define USAGE	"[-xNUM] [-r[0|1]] -sSYSTEM -uUSERID -dSPOOL -iINTERFACE"

static void closedem(void);
static char *pskip(char *);
#ifdef NOSTRANGERS
static void checkrmt(void);
#endif /* NOSTRANGERS */
static void cleanTM(void);


void
main(argc, argv, envp)
char *argv[];
char **envp;
{

#ifdef sgi
	extern void onintr(), timeout();
	extern void intrEXIT();
#else
	extern onintr(), timeout();
	extern intrEXIT();
#endif /* sgi */
	int ret, seq, exitcode;
	char file[NAMESIZE];
	char msg[BUFSIZ], *p, *q;
	char xflag[6];	/* -xN N is single digit */
	char *ttyn;
	char *iface;	/* interface name	*/
	char	cb[128];
	time_t	ts, tconv;
#ifndef	V7
	long 	minulimit, dummy = 0;
#endif /* V7 */

	Ulimit = ulimit(1,0L);
	Uid = getuid();
	Euid = geteuid();	/* this should be UUCPUID */
	if (Uid == 0)
	    setuid(UUCPUID);
	Env = envp;
	Role = SLAVE;
	strcpy(Logfile, LOGCICO);
	*Rmtname = NULLCHAR;

	closedem();
	time(&Nstat.t_qtime);
	tconv = Nstat.t_start = Nstat.t_qtime;
	strcpy(Progname, "uucico");
	setservice(Progname);
	ret = sysaccess(EACCESS_SYSTEMS);
	ASSERT(ret == 0, Ct_OPEN, "Systems", ret);
	ret = sysaccess(EACCESS_DEVICES);
	ASSERT(ret == 0, Ct_OPEN, "Devices", ret);
	ret = sysaccess(EACCESS_DIALERS);
	ASSERT(ret == 0, Ct_OPEN, "Dialers", ret);
	Pchar = 'C';
	(void) signal(SIGILL, intrEXIT);
	(void) signal(SIGTRAP, intrEXIT);
	(void) signal(SIGIOT, intrEXIT);
	(void) signal(SIGEMT, intrEXIT);
	(void) signal(SIGFPE, intrEXIT);
	(void) signal(SIGBUS, intrEXIT);
	(void) signal(SIGSEGV, intrEXIT);
	(void) signal(SIGSYS, intrEXIT);
	if (signal(SIGPIPE, SIG_IGN) != SIG_IGN)	/* This for sockets */
		(void) signal(SIGPIPE, intrEXIT);
	(void) signal(SIGINT, onintr);
	(void) signal(SIGHUP, onintr);
	(void) signal(SIGQUIT, onintr);
	(void) signal(SIGTERM, onintr);
#ifdef ATTSV
	(void) signal(SIGUSR1, SIG_IGN);
	(void) signal(SIGUSR2, SIG_IGN);
#endif

	ret = guinfo(Euid, User);
	ASSERT(ret == 0, "BAD UID ", "", ret);
	strncpy(Uucp, User, NAMESIZE);

	setuucp(User);

	*xflag = NULLCHAR;
	Ifn = Ofn = -1;
	iface = "UNIX";

	while ((ret = getopt(argc, argv, "d:r:s:x:u:i:")) != EOF) {
		switch (ret) {
		case 'd':
			if ( eaccess(optarg, 01) != 0 ) {
				(void) fprintf(stderr,
					"%s: cannot access spool directory %s\n",
					Progname, optarg);
				exit(1);
			}
			Spool = optarg;
			break;
		case 'r':
			if ( (Role = atoi(optarg)) != MASTER && Role != SLAVE ) {
				(void) fprintf(stderr,
					"%s: bad value '%s' for -r argument\nUSAGE:\t%s %s\n",
					Progname, optarg, Progname, USAGE );
				exit(1);
			}
			break;
		case 's':
			if (versys(optarg)) {
			    (void) fprintf(stderr,
				"%s: %s not in Systems file\n",
				Progname, optarg);
			    cleanup(101);
			}
			strncpy(Rmtname, optarg, MAXBASENAME);
			Rmtname[MAXBASENAME] = '\0';
			/* set args for possible xuuxqt call */
			strcpy(uuxqtarg, Rmtname);
			break;
		case 'x':
			Debug = atoi(optarg);
			if (Debug <= 0)
				Debug = 1;
			if (Debug > 9)
				Debug = 9;
			(void) sprintf(xflag, "-x%d", Debug);
			break;
		case 'u':
			DEBUG(4, "Loginuser %s specified\n", optarg);
			strncpy(Loginuser, optarg, NAMESIZE);
			Loginuser[NAMESIZE - 1] = NULLCHAR;
			break;
		case 'i':
			/*	interface type		*/
			iface = optarg;
			break;
		default:
			(void) fprintf(stderr, "\tusage: %s %s\n",
			    Progname, USAGE);
			exit(1);
		}
	}

	if (Role == MASTER || *Loginuser == NULLCHAR) {
	    ret = guinfo(Uid, Loginuser);
	    ASSERT(ret == 0, "BAD LOGIN_UID ", "", ret);
	}
#ifdef sgi
	/*
	 * shut down if /etc/nologin exists.
	 */
	if (!access(_PATH_NOLOGIN, F_OK)) {
		(void)sprintf(msg, "%s exists",_PATH_NOLOGIN);
		logent(msg, "FAILED");
		DEBUG(4, "*** stopping ***  -  %s exists,", _PATH_NOLOGIN);
		cleanup(101);
	}
#endif /* sgi */

	if (Role == MASTER) {
	    if (*Rmtname == NULLCHAR) {
		DEBUG0(5, "No -s specified\n" );
		cleanup(101);
	    }
	    /* get Myname - it depends on who I'm calling--Rmtname */
	    (void) mchFind(Rmtname);
	    myName(Myname);
	    if (EQUALSN(Rmtname, Myname, SYSNSIZE)) {
		DEBUG(5, "This system specified: -sMyname: %s, ", Myname);
		cleanup(101);
	    }
	}

	ASSERT(chdir(Spool) == 0, Ct_CHDIR, Spool, errno);
	strcpy(Wrkdir, Spool);

	if (Role == SLAVE) {

		if (freopen(RMTDEBUG, "a", stderr) == 0) {
			errent(Ct_OPEN, RMTDEBUG, errno, __FILE__, __LINE__);
			freopen("/dev/null", "w", stderr);
		}
		if ( interface(iface) ) {
			(void)fprintf(stderr,
			"%s: invalid interface %s\n", Progname, iface);
			cleanup(101);
		}
		/*master setup will be called from processdev()*/
		if ( (*Setup)( Role, &Ifn, &Ofn ) ) {
			DEBUG0(5, "SLAVE Setup failed");
			cleanup(101);
		}

		/*
		 * initial handshake
		 */
		ret = savline();
		fixline(Ifn, 0, D_ACU);
		/* get MyName - use logFind to check PERMISSIONS file */
		(void) logFind(Loginuser, "");
		myName(Myname);

		DEBUG(4,"cico.c: Myname - %s\n",Myname);
		DEBUG(4,"cico.c: Loginuser - %s\n",Loginuser);
		fflush(stderr);
		Nstat.t_scall = times(&Nstat.t_tga);
		(void) sprintf(msg, "here=%s", Myname);
		omsg('S', msg, Ofn);
		(void) signal(SIGALRM, timeout);
		(void) alarm(2 * MAXMSGTIME);	/* give slow machines a second chance */
		if (setjmp(Sjbuf)) {

			/*
			 * timed out
			 */
			ret = restline();
			rmlock(CNULL);
			exit(0);
		}
		for (;;) {
			ret = imsg(msg, Ifn);
			if (ret != 0) {
				(void) alarm(0);
				ret = restline();
				rmlock(CNULL);
				exit(0);
			}
			if (msg[0] == 'S')
				break;
		}
		Nstat.t_ecall = times(&Nstat.t_tga);
		(void) alarm(0);
		q = &msg[1];
		p = pskip(q);
		strncpy(Rmtname, q, MAXBASENAME);
		Rmtname[MAXBASENAME] = '\0';

		seq = 0;
		while (*p == '-') {
			q = pskip(p);
			switch(*(++p)) {
			case 'x':
				Debug = atoi(++p);
				if (Debug <= 0)
					Debug = 1;
				break;
			case 'Q':
				seq = atoi(++p);
				break;
			default:
				break;
			}
			p = q;
		}
		DEBUG(4, "sys-%s\n", Rmtname);

#ifdef NOSTRANGERS
		checkrmt();	/* do we know the remote system? */
#endif /* NOSTRANGERS */

		if (mlock(Rmtname)) {
			omsg('R', "LCK", Ofn);
			cleanup(101);
		}
		
		/* validate login using PERMISSIONS file */
		if (logFind(Loginuser, Rmtname) == FAIL) {
			Uerror = SS_BAD_LOG_MCH;
			logent(UERRORTEXT, "FAILED");
			systat(Rmtname, SS_BAD_LOG_MCH, UERRORTEXT,
			    Retrytime);
			omsg('R', "LOGIN", Ofn);
			cleanup(101);
		}

		ret = callBack();
		DEBUG(4,"return from callcheck: %s",ret ? "TRUE" : "FALSE");
		if (ret==TRUE) {
			(void) signal(SIGINT, SIG_IGN);
			(void) signal(SIGHUP, SIG_IGN);
			omsg('R', "CB", Ofn);
			logent("CALLBACK", "REQUIRED");

			/*
			 * set up for call back
			 */
			systat(Rmtname, SS_CALLBACK, "CALL BACK", Retrytime);
			gename(CMDPRE, Rmtname, 'C', file);
			(void) close(creat(file, CFILEMODE));
			xuucico(Rmtname);
			cleanup(101);
		}

		if (callok(Rmtname) == SS_SEQBAD) {
			Uerror = SS_SEQBAD;
			logent(UERRORTEXT, "PREVIOUS");
			omsg('R', "BADSEQ", Ofn);
			cleanup(101);
		}

		if ((ret = gnxseq(Rmtname)) == seq) {
			omsg('R', "OK", Ofn);
			cmtseq();
		} else {
			Uerror = SS_SEQBAD;
			systat(Rmtname, SS_SEQBAD, UERRORTEXT, Retrytime);
			logent(UERRORTEXT, "HANDSHAKE FAILED");
			ulkseq();
			omsg('R', "BADSEQ", Ofn);
			cleanup(101);
		}
		ttyn = ttyname(Ifn);
		if (ttyn != NULL) {
			struct stat ttysbuf;
			if ( fstat(Ifn,&ttysbuf) == 0 )
				Dev_mode = ttysbuf.st_mode;
			else
				Dev_mode = R_DEVICEMODE;
			strcpy(Dc, BASENAME(ttyn, '/'));
			chmod(ttyn, S_DEVICEMODE);
		} else
			strcpy(Dc, "notty");
		/* set args for possible xuuxqt call */
		strcpy(uuxqtarg, Rmtname);
	}

	strcpy(User, Uucp);
/*
 *  Ensure reasonable ulimit (MINULIMIT)
 */

#ifndef	V7
	minulimit = ulimit(1,dummy);
	ASSERT(minulimit >= MINULIMIT, "ULIMIT TOO SMALL",
	    Loginuser, minulimit);
#endif
	if (Role == MASTER && callok(Rmtname) != 0) {
		logent("SYSTEM STATUS", "CAN NOT CALL");
		cleanup(101);
	}

	chremdir(Rmtname);

	(void) strcpy(Wrkdir, RemSpool);
	if (Role == MASTER) {

		/*
		 * master part
		 */
		(void) signal(SIGINT, SIG_IGN);
		(void) signal(SIGHUP, SIG_IGN);
		(void) signal(SIGQUIT, SIG_IGN);
		if (Ifn != -1 && Role == MASTER) {
			(void) (*Write)(Ofn, EOTMSG, strlen(EOTMSG));
			(void) close(Ofn);
			(void) close(Ifn);
			Ifn = Ofn = -1;
			rmlock(CNULL);
			sleep(3);
		}
		(void) sprintf(msg, "call to %s ", Rmtname);
		if (mlock(Rmtname) != 0) {
			logent(msg, "LOCKED");
			CDEBUG(1, "Currently Talking With %s\n",
			    Rmtname);
 			cleanup(100);
		}
		Nstat.t_scall = times(&Nstat.t_tga);
		Ofn = Ifn = conn(Rmtname);
		Nstat.t_ecall = times(&Nstat.t_tga);
		if (Ofn < 0) {
			delock(Rmtname);
#ifdef sgi
			if (Debug > 0 || Uerror != SS_TIME_WRONG)
				logent(UERRORTEXT, "CONN FAILED");
#else
			logent(UERRORTEXT, "CONN FAILED");
#endif
			systat(Rmtname, Uerror, UERRORTEXT, Retrytime);
			cleanup(101);
		} else {
			logent(msg, "SUCCEEDED");
			ttyn = ttyname(Ifn);
			if (ttyn != NULL) {
				struct stat ttysbuf;
				if ( fstat(Ifn,&ttysbuf) == 0 )
					Dev_mode = ttysbuf.st_mode;
				else
					Dev_mode = R_DEVICEMODE;
				chmod(ttyn, M_DEVICEMODE);
			}
		}
	
		if (setjmp(Sjbuf)) {
			delock(Rmtname);
			Uerror = SS_LOGIN_FAILED;
			logent(Rmtname, UERRORTEXT);
			systat(Rmtname, SS_LOGIN_FAILED,
			    UERRORTEXT, Retrytime);
			DEBUG(4, "%s - failed\n", UERRORTEXT);
			cleanup(101);
		}
		(void) signal(SIGALRM, timeout);
		/* give slow guys lots of time to thrash */
		(void) alarm(3 * MAXMSGTIME);
		for (;;) {
			ret = imsg(msg, Ifn);
#ifdef sgi
			if (ret < 0) 
				longjmp(Sjbuf, 1);
#endif
			if (ret != 0) {
				continue; /* try again */
			}
			if (msg[0] == 'S')
				break;
		}
		(void) alarm(0);
		if(EQUALSN("here=", &msg[1], 5)){
			/*
			 * this is a problem.  We'd like to compare with an
			 * untruncated Rmtname but we fear incompatability.
			 * So we'll look at most 6 chars (at most).
			 */
			if(!EQUALSN(&msg[6], Rmtname, (strlen(Rmtname)< 7 ?
						strlen(Rmtname) : 6))){
				delock(Rmtname);
				Uerror = SS_WRONG_MCH;
				logent(&msg[6], UERRORTEXT);
				systat(Rmtname, SS_WRONG_MCH, UERRORTEXT,
				     Retrytime);
				DEBUG(4, "%s - failed\n", UERRORTEXT);
				cleanup(101);
			}
		}
		CDEBUG(1,"Login Successful: System=%s\n",&msg[6]);
		seq = gnxseq(Rmtname);
		(void) sprintf(msg, "%s -Q%d %s", Myname, seq, xflag);
		omsg('S', msg, Ofn);
		(void) alarm(2 * MAXMSGTIME);	/* give slow guys some thrash time */
		for (;;) {
			ret = imsg(msg, Ifn);
			DEBUG(4, "msg-%s\n", msg);
			if (ret != 0) {
				(void) alarm(0);
				delock(Rmtname);
				ulkseq();
				cleanup(101);
			}
			if (msg[0] == 'R')
				break;
		}
		(void) alarm(0);

		/*  check for rejects from remote */
		Uerror = 0;
		if (EQUALS(&msg[1], "LCK")) 
			Uerror = SS_RLOCKED;
		else if (EQUALS(&msg[1], "LOGIN"))
			Uerror = SS_RLOGIN;
		else if (EQUALS(&msg[1], "CB"))
			Uerror = SS_CALLBACK;
		else if (EQUALS(&msg[1], "You are unknown to me"))
			Uerror = SS_RUNKNOWN;
		else if (EQUALS(&msg[1], "BADSEQ"))
			Uerror = SS_SEQBAD;
		else if (!EQUALS(&msg[1], "OK"))
			Uerror = SS_UNKNOWN_RESPONSE;
		if (Uerror)  {
			delock(Rmtname);
			systat(Rmtname, Uerror, UERRORTEXT, Retrytime);
			logent(UERRORTEXT, "HANDSHAKE FAILED");
			CDEBUG(1, "HANDSHAKE FAILED: %s\n", UERRORTEXT);
			ulkseq();
			cleanup(101);
		}
		cmtseq();
	}
	DEBUG(4, " Rmtname %s, ", Rmtname);
	DEBUG(4, "Role %s,  ", Role ? "MASTER" : "SLAVE");
	DEBUG(4, "Ifn - %d, ", Ifn);
	DEBUG(4, "Loginuser - %s\n", Loginuser);

	/* alarm/setjmp added here due to experience with uucico
	 * hanging for hours in imsg().
	 */
	if (setjmp(Sjbuf)) {
		delock(Rmtname);
		logent("startup", "TIMEOUT");
		DEBUG(4, "%s - timeout\n", "startup");
		cleanup(101);
	}
	(void) alarm(MAXSTART);
	ret = startup(Role);
	(void) alarm(0);

	if (ret != SUCCESS) {
		delock(Rmtname);
		logent("startup", "FAILED");
		Uerror = SS_STARTUP;
		CDEBUG(1, "%s\n", UERRORTEXT);
		systat(Rmtname, Uerror, UERRORTEXT, Retrytime);
		exitcode = 101;
	} else {
		logent("startup", "OK");
		systat(Rmtname, SS_INPROGRESS, UTEXT(SS_INPROGRESS),Retrytime);
		Nstat.t_sftp = times(&Nstat.t_tga);

		exitcode = cntrl(Role);
		Nstat.t_eftp = times(&Nstat.t_tga);
		DEBUG(4, "cntrl - %d\n", exitcode);
		(void) signal(SIGINT, SIG_IGN);
		(void) signal(SIGHUP, SIG_IGN);
		(void) signal(SIGALRM, timeout);

		if (exitcode == 0) {
			(void) time(&ts);
			(void) sprintf(cb, "conversation complete %s %ld",
				Dc, ts - tconv);
			logent(cb, "OK");
			systat(Rmtname, SS_OK, UTEXT(SS_OK), Retrytime);

		} else {
			logent("conversation complete", "FAILED");
			systat(Rmtname, SS_CONVERSATION,
			    UTEXT(SS_CONVERSATION), Retrytime);
		}
		(void) alarm(2 * MAXMSGTIME);	/* give slow guys some thrash time */
		omsg('O', "OOOOO", Ofn);
		CDEBUG(4, "send OO %d,", ret);
		if (!setjmp(Sjbuf)) {
			for (;;) {
				omsg('O', "OOOOO", Ofn);
				ret = imsg(msg, Ifn);
				if (ret != 0)
					break;
				if (msg[0] == 'O')
					break;
			}
		}
		(void) alarm(0);
	}
	cleanup(exitcode);
}

/*
 * clean and exit with "code" status
 */
void
cleanup(code)
register int code;
{
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGHUP, SIG_IGN);
	rmlock(CNULL);
	closedem();
	(*Teardown)( Role, Ifn, Ofn );
	DEBUG(4, "exit code %d\n", code);
	CDEBUG(1, "Conversation Complete: Status %s\n\n", 
	    code ? "FAILED" : "SUCCEEDED");

	cleanTM();
	if (code == 0)
		xuuxqt(uuxqtarg);
	exit(code);
}

short TM_cnt = 0;
char TM_name[MAXNAMESIZE];

static void
cleanTM(void)
{
	register int i;
	char tm_name[MAXNAMESIZE];

	DEBUG(7,"TM_cnt: %d\n",TM_cnt);
	for(i=0; i < TM_cnt; i++) {
		(void) sprintf(tm_name, "%s.%3.3d", TM_name, i);
		DEBUG(7, "tm_name: %s\n", tm_name);
		unlink(tm_name);
	}
}

void
TMname(file, pnum)
char *file;
{

	(void) sprintf(file, "%s/TM.%.5d.%.3d", RemSpool, pnum, TM_cnt);
	if (TM_cnt == 0)
	    (void) sprintf(TM_name, "%s/TM.%.5d", RemSpool, pnum);
	DEBUG(7, "TMname(%s)\n", file);
	TM_cnt++;
}

/*
 * intrrupt - remove locks and exit
 */
#ifdef sgi
void
#endif
onintr(inter)
register int inter;
{
	char str[30];
	/* I'm putting a test for zero here because I saw it happen
	 * and don't know how or why, but it seemed to then loop
	 * here for ever?
	 */
	if (inter == 0)
	    exit(99);
	(void) signal(inter, SIG_IGN);
	(void) sprintf(str, "SIGNAL %d", inter);
	logent(str, "CAUGHT");
	cleanup(inter);
}

/*ARGSUSED*/
#ifdef sgi
void
#endif
intrEXIT(inter)
{
	char	cb[10];
	extern int errno;

	(void) sprintf(cb, "%d", errno);
	logent("INTREXIT", cb);
	(void) signal(SIGIOT, SIG_DFL);
	(void) signal(SIGILL, SIG_DFL);
	rmlock(CNULL);
	closedem();
	(void) setuid(Uid);
	abort();
}

/*
 * catch SIGALRM routine
 */
#ifdef sgi
void
#endif
timeout()
{
	longjmp(Sjbuf, 1);
}

static char *
pskip(p)
register char *p;
{
	while( *p && *p != ' ' )
		++p;
	if( *p ) *p++ = 0;
	return(p);
}

static void
closedem(void)
{
	register i, maxfiles;

#ifdef ATTSVR3
	maxfiles = ulimit(4,0);
#else /* !ATTSVR3 */
	maxfiles = _NFILE;
#endif /* ATTSVR3 */

	for (  i = 3; i < maxfiles; i++ )
		if ( i != Ifn && i != Ofn && i != fileno(stderr) )
			(void) close(i);
}


#ifdef NOSTRANGERS

/*
 *	checkrmt()
 *
 *	if the command NOSTRANGERS is executable and if we don't
 *	know the remote system (i.e., it doesn't appear in the
 *	Systems file), run NOSTRANGERS to log the attempt and hang up.
 */

static void
checkrmt()
{
	int	pid, waitrv;

	if ( (access(NOSTRANGERS, 1) == 0) && versys(Rmtname)) {

		DEBUG(5, "Invoking NOSTRANGERS for %s\n", Rmtname);
		(void) signal(SIGHUP, SIG_IGN);
		errno = 0;
		if ( (pid = fork()) == 0 ) {
			execlp(NOSTRANGERS, "stranger",
					Rmtname, (char *)NULL);
			perror("cico.c: execlp NOSTRANGERS failed");
		} else if ( pid < 0 ) {
			perror("cico.c: fork failed");
		} else {
			while ( (waitrv = wait((int *)0)) != pid ) {
				if ( waitrv < 0  && errno != EINTR ) {
					perror("cico.c: wait failed");
					break;
				}
			}
		}

		omsg('R', "You are unknown to me", Ofn);
		cleanup(101);
	}
	return;
}
#endif /* NOSTRANGERS */
