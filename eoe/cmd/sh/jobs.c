/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:jobs.c	1.17.32.2"
/*
 * Job control for UNIX Shell
 */

#include	<sys/termio.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<sys/param.h>
#include	<sys/ioctl.h>
#include	<string.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<pfmt.h>
#include	"defs.h"

extern pid_t getpgid();
extern int setpgid();
extern int str2sig();
extern int sig2str();

static int settgid();

#define	BIGPRVS	256

/*
 * one of these for each active job
 */

struct job
{
	struct job *j_nxtp;	/* next job in job ID order */
	struct job *j_curp;	/* next job in job currency order */
	struct termios j_stty;	/* termio save area when job stops */
	pid_t  j_pid;		/* job leader's process ID */
	pid_t  j_pgid;		/* job's process group ID */
	pid_t  j_tgid;		/* job's foreground process group ID */
	uint   j_jid;		/* job ID */
	ushort j_xval;		/* exit code, or exit or stop signal */ 
	ushort j_flag;		/* various status flags defined below */
	char  *j_pwd;		/* job's working directory */
	char  *j_cmd;		/* cmd used to invoke this job */
};

/* defines for j_flag */

#define J_DUMPED	0001	/* job has core dumped */
#define J_NOTIFY	0002	/* job has changed status */
#define J_SAVETTY	0004	/* job was stopped in foreground, and its
				   termio settings were saved */
#define J_STOPPED	0010	/* job has been stopped */
#define J_SIGNALED	0020	/* job has received signal; j_xval has it */
#define J_DONE		0040	/* job has finished */
#define J_RUNNING	0100	/* job is currently running */
#define J_FOREGND	0200	/* job was put in foreground by shell */

/* options to the printjob() function defined below */

#define PR_CUR		00001	/* print job currency ('+', '-', or ' ') */
#define PR_JID		00002	/* print job ID				 */
#define PR_PGID		00004	/* print job's process group ID		 */
#define PR_STAT		00010	/* print status obtained from wait	 */
#define PR_CMD		00020	/* print cmd that invoked job		 */
#define PR_AMP		00040	/* print a '&' if in the background	 */
#define PR_PWD		00100	/* print jobs present working directory	 */

#define PR_DFL		(PR_CUR|PR_JID|PR_STAT|PR_CMD) /* default options */
#define PR_LONG		(PR_DFL|PR_PGID|PR_PWD)	/* long options */

static struct termios 	mystty;	 /* default termio settings		 */
static int 		eofflg,
			jobcnt,	 /* number of active jobs		 */
			jobdone, /* number of active but finished jobs	 */
			jobnote; /* jobs requiring notification		 */
static struct job 	*jobcur, /* active jobs listed in currency order */
			**nextjob,
			*thisjob,
			*joblst; /* active jobs listed in job ID order	 */

static void	sigv();
static void	printjob();

/*
** Function: tcgetpgrp()
** 
** Notes: This function retrieves the process group id of the process
** currently attached to the terminal.  The ioctl does not use
** privilege.
*/
pid_t
tcgetpgrp(fd)
{
	pid_t pgid;
	if (ioctl(fd, TIOCGPGRP, &pgid) == 0)
		return pgid;
	return (pid_t)-1;
}

/*
** Function: tcsetpgrp()
** 
** Notes: This function sets the process group id of the process
** currently attached to the terminal.  The ioctl does not use
** privilege.
*/
static int
tcsetpgrp(fd, pgid)
int fd;
pid_t pgid;
{
	return ioctl(fd, TIOCSPGRP, &pgid);
}

/*
** Function: pgid2job()
** 
** Notes: This function looks up a job in the job list based on the supplied
** process group id.
*/
static struct job *
pgid2job(pgid)
register pid_t pgid;
{
	register struct job *jp;

	for (jp = joblst; jp != 0 && jp->j_pid != pgid; jp = jp->j_nxtp)
		continue;

	return(jp);
}

/*
** Function: str2job()
** 
** Notes: This function looks up a job in the job list based on the supplied
** string.  The string may be any valid jobid string as described in the
** job control section of the manual page.
*/
static struct job *
str2job(cmd, job, mustbejob)
register int	cmd;
register char *job;
int mustbejob;
{
	register struct job *jp,*njp;
	register i;

	if (*job != '%')
		jp = pgid2job(stoi(job));
	else if (*++job == 0 || *job == '+' || *job == '%' || *job == '-') {
		jp = jobcur;
		if (*job == '-' && jp)
			jp = jp->j_curp;
	} else if (*job >= '0' && *job <= '9') {
		i = stoi(job);
		for (jp = joblst; jp && jp->j_jid != i; jp = jp->j_nxtp)
			continue;
	} else if (*job == '?') {
		register j;
		register char *p;
		i = strlen(++job);
		jp = 0;
		for (njp = jobcur; njp; njp = njp->j_curp) {
			if (njp->j_jid == 0)
				continue;
			for (p = njp->j_cmd, j = strlen(p); j >= i; p++, j--) {
				if (strncmp(job,p,i) == 0) {
					if (jp != 0)
						error(cmd, ambiguous, ambiguousid);
					jp = njp;
					break;
				}
			}
		}
	} else {
		i = strlen(job);
		jp = 0;
		for (njp = jobcur; njp; njp = njp->j_curp) {
			if (njp->j_jid == 0)
				continue;
			if (strncmp(job,njp->j_cmd,i) == 0) {
				if (jp != 0)
					error(cmd, ambiguous, ambiguousid);
				jp = njp;
			}
		}
	}

	if (mustbejob && (jp == 0 || jp->j_jid == 0))
		error(cmd, nosuchjob, nosuchjobid);

	return jp;
}

/*
** Function: freejob()
** 
** Notes: This function removes a job structure from the job list and
** frees the structure.
*/
static void
freejob(jp)
register struct job *jp;
{
	register struct job **njp;
	register struct job **cjp;

	for (njp = &joblst; *njp != jp; njp = &(*njp)->j_nxtp)
		continue;

	for (cjp = &jobcur; *cjp != jp; cjp = &(*cjp)->j_curp)
		continue;

	*njp = jp->j_nxtp;
	*cjp = jp->j_curp;
	free(jp);
	jobcnt--;
	jobdone--;
}

/*
** Function: statjob()
**
** Notes: This function analyzes the status of a job.
*/

static int
statjob(jp,stat,fg,rc) 
register struct job *jp;
int stat;
int fg;
int rc;
{
	pid_t tgid;
	int done = 0;

	if (WIFCONTINUED(stat)) {
		if (jp->j_flag & J_STOPPED) {
			jp->j_flag &= ~(J_STOPPED|J_SIGNALED|J_SAVETTY);
			jp->j_flag |= J_RUNNING;
			if (!fg && jp->j_jid) {
				jp->j_flag |= J_NOTIFY;
				jobnote++;
			}
		}
	} else if (WIFSTOPPED(stat)) {
		jp->j_xval = WSTOPSIG(stat);
		jp->j_flag &= ~J_RUNNING;
		jp->j_flag |= (J_SIGNALED|J_STOPPED);
		jp->j_pgid = getpgid(jp->j_pid);
		jp->j_tgid = jp->j_pgid;
		if (fg) {
			if (tgid = settgid(mypgid, jp->j_pgid))
				jp->j_tgid = tgid;
			else {
				jp->j_flag |= J_SAVETTY;
				tcgetattr(0,&jp->j_stty);
				(void)tcsetattr(0,TCSANOW,&mystty);
			}
		} 
		if (jp->j_jid) {
			jp->j_flag |= J_NOTIFY;
			jobnote++;
		}
	} else {
		jp->j_flag &= ~J_RUNNING;
		jp->j_flag |= J_DONE;
		done++;
		jobdone++;
		if (WIFSIGNALED(stat)) {
			jp->j_xval = WTERMSIG(stat);
			jp->j_flag |= J_SIGNALED;
			if (WCOREDUMP(stat))
				jp->j_flag |= J_DUMPED;
			if (!fg || jp->j_xval != SIGINT) {
				jp->j_flag |= J_NOTIFY;
				jobnote++;
			}
		} else { /* WIFEXITED */
			jp->j_xval = WEXITSTATUS(stat);
			jp->j_flag &= ~J_SIGNALED;
			if (!fg && jp->j_jid) {
				jp->j_flag |= J_NOTIFY;
				jobnote++;
			}
		}
		if (fg) {
			if (!settgid(mypgid, jp->j_pgid) 
			  || !settgid(mypgid, getpgid(jp->j_pid)))
				tcgetattr(0,&mystty);
		}
	}
	if (rc) {
		exitval = jp->j_xval;
		if (jp->j_flag & J_SIGNALED)
			exitval |= SIGFLG;
		exitset();
	}
	if (done && !(jp->j_flag & J_NOTIFY))
		freejob(jp);
	return done;
}

/*
** Function: collectjobs()
**
** Notes: This function does the following:
**
**	collect the status of jobs that have recently exited or stopped - 
**	if wnohang == WNOHANG, wait until error, or all jobs are accounted for;
** 
**	called after each command is executed, with wnohang == 0, and as part
**	of "wait" builtin with wnohang == WNOHANG
*/

static void
collectjobs(wnohang)
{
	pid_t pid;
	register struct job *jp;
	int stat, n;

#ifdef _sgi
	/*
	 * SGI - if one gets a WIFCONTINUED, the job isn't done
	 * and we shouldn' return.
	 * To test, create a script:
	 *	#!/bin/sh
	 *	sleep 1000000&
	 *	wait
	 *	date
	 * Run this and ^Z it - then 'fg' it - the date
	 * command shouldn't appear..
	 */
	while (jobcnt - jobdone) {
#else
	for (n = jobcnt - jobdone; n > 0; n--) {
#endif
		if ((pid = waitpid(-1,&stat,wnohang|WUNTRACED|WCONTINUED)) <= 0)
			break;
		if (jp = pgid2job(pid))
			(void)statjob(jp,stat,0,0);
	}

}

/*
** Function: freejobs()
**
** Notes: This function removes all completed jobs from the job list.
*/
void
freejobs()
{
	register struct job *jp;

	collectjobs(WNOHANG);

	if (jobnote) {
		register int savefd = setb(2);
		for (jp = joblst; jp; jp = jp->j_nxtp) {
			if (jp->j_flag & J_NOTIFY) {
				if (jp->j_jid)
					printjob(jp, PR_DFL);
				else if (jp->j_flag & J_FOREGND)
					printjob(jp, PR_STAT);
				else
					printjob(jp, PR_STAT|PR_PGID);
			}
		}
		(void)setb(savefd);
	}

	if (jobdone) {
		for (jp = joblst; jp; jp = jp->j_nxtp) {
			if (jp->j_flag & J_DONE)
				freejob(jp);
		}
	}
}

/*
** Function: waitjob()
**
** Notes: This function waits for the specified job to complete and then
** returns. If an error conditionis set, this function exits from the
** current shell.
*/
static void
waitjob(jp)
register struct job *jp;
{
	int stat;
	int done;
	pid_t pid = jp->j_pid;

	while (waitpid(pid, &stat, WUNTRACED|WNOWAIT) != pid)
		continue;
	done = statjob(jp,stat,1,1);
	waitpid(pid, 0, WUNTRACED);
	if (done && exitval && (flags & errflg))
		exitsh(exitval);
	flags |= eflag;
}

/*
** Function: settgid()
**
** Notes: This function sets the foreground process group to *new* only
** if the current foreground process group is equal to *expected*
*/
static int
settgid(new, expected)
pid_t new, expected;
{
	register pid_t current = tcgetpgrp(0);

	if (current != expected)
		return current;

	if (new != current)
		(void)tcsetpgrp(0, new);

	return 0;
}

/*
** Function: settgid()
**
** Restrictions:
**		kill()	- Should have no privilege
**
** Notes: This function sets the foreground process group to *new* only
** if the current foreground process group is equal to *expected*
*/
static void
restartjob(jp,fg)
register struct job *jp;
{
	if (jp != jobcur) {
		register struct job *t;
		for (t = jobcur; t->j_curp != jp; t = t->j_curp);
		t->j_curp = jp->j_curp;
		jp->j_curp = jobcur;
		jobcur = jp;
	}
	if (fg) {
		if (jp->j_flag & J_SAVETTY) {
			jp->j_stty.c_lflag &= ~TOSTOP;
			jp->j_stty.c_lflag |= (mystty.c_lflag&TOSTOP);
			jp->j_stty.c_cc[VSUSP] = mystty.c_cc[VSUSP];
			jp->j_stty.c_cc[VDSUSP] = mystty.c_cc[VDSUSP];
			(void)tcsetattr(0,TCSADRAIN,&jp->j_stty);
		}
		(void)settgid(jp->j_tgid, mypgid);
	}
	(void)kill(-(jp->j_pgid), SIGCONT);
	if (jp->j_tgid != jp->j_pgid)
		(void)kill(-(jp->j_tgid), SIGCONT);
	jp->j_flag &= ~(J_STOPPED|J_SIGNALED|J_SAVETTY);
	jp->j_flag |= J_RUNNING;
	if (fg)  {
		jp->j_flag |= J_FOREGND;
		printjob(jp, PR_JID|PR_CMD);
		waitjob(jp);
	} else {
		jp->j_flag &= ~J_FOREGND;
		printjob(jp,PR_JID|PR_CMD|PR_AMP);
	}
}

/*
** Function: printjob()
**
** Notes: Print the status of a job.
*/
static void
printjob(jp,propts)
register struct job *jp;
{
	int sp = 0;

	if (jp->j_flag & J_NOTIFY) {
		jobnote--;
		jp->j_flag &= ~J_NOTIFY;
	}

	if (propts & PR_JID) {
		prc_buff('[');
		prn_buff(jp->j_jid);
		prc_buff(']');
		sp = 1;
	}

	if (propts & PR_CUR) {
		while (sp-- > 0)
			prc_buff(SP);
		sp = 1;
		if (jobcur == jp)
			prc_buff('+');
		else if (jobcur != 0 && jobcur->j_curp == jp)
			prc_buff('-');
		else
			sp++;
	}

	if (propts & PR_PGID) {
		while (sp-- > 0)
			prc_buff(SP);
		prn_buff(jp->j_pid);
		sp = 1;
	}

	if (propts & PR_STAT) {
		while (sp-- > 0)
			prc_buff(SP);
		sp = 28;
		if (jp->j_flag & J_SIGNALED) {
			extern char *_sys_siglist[];
			extern int _sys_nsig;
			if (jp->j_xval < (ushort)_sys_nsig) {
				const char *sig = __gtxt("uxlibc", jp->j_xval + 4,
					_sys_siglist[jp->j_xval]);
				sp -= strlen(sig);
				prs_buff(sig);
			} else {
				const char *sig = gettxt(":559", "Signal ");
				itos(jp->j_xval);
				sp -= strlen((char *)numbuf) + strlen(sig);
				prs_buff(sig);
				prs_buff(numbuf);
			}
			if (jp->j_flag & J_DUMPED) {
				const char *core = gettxt(coredumpid, coredump);
				sp -= strlen(core);
				prs_buff(core);
			}
		} else if (jp->j_flag & J_DONE) {
			const char *ex = gettxt(exitedid, exited);
			itos(jp->j_xval);
			sp -= strlen(ex) + strlen((char *)numbuf) + 2;
			prs_buff(ex);
			prc_buff('(');
			itos(jp->j_xval);
			prs_buff(numbuf);
			prc_buff(')');
		} else {
			const char *run = gettxt(runningid, running);
			sp -= strlen(run);
			prs_buff(run);
		}
		if (sp < 1)
			sp = 1;
	}

	if (propts & PR_CMD) {
		while (sp-- > 0)
			prc_buff(SP);
		prs_buff(jp->j_cmd);
		sp = 1;
	}

	if (propts & PR_AMP) {
		while (sp-- > 0)
			prc_buff(SP);
		prc_buff('&');
		sp = 1;
	}

	if (propts & PR_PWD) {
		while (sp-- > 0)
			prc_buff(SP);
		prs_buff(gettxt(":560", "(wd: "));
		prs_buff(jp->j_pwd);
		prc_buff(')');
	}

	prc_buff(NL);
	flushb();

}


/*
** Function: startjobs()
**
** Notes: This function is called to initialize job control for each new
** input file to the shell, and after the "exec" builtin.
*/

void 
startjobs()
{
	if (tcgetattr(0,&mystty) == -1 || svtgid == -1) {
		flags &= ~jcflg;
		return;
	}

	flags |= jcflg;

	handle(SIGTTOU, SIG_IGN);
	handle(SIGTSTP, SIG_IGN);

	if (mysid != mypgid) {
		(void)setpgid(0,0);
		mypgid = mypid;
		(void)settgid(mypgid, svpgid);
	}

}

/*
** Function: endjobs()
**
** Notes: This function checks the status of job control before leaving
** an input stream or at the "exit" builtin.
*/
int
endjobs(check_if)
int check_if;
{
	if ((flags & (jcoff|jcflg)) != jcflg)
		return 1;

	if (check_if && jobcnt && eofflg++ == 0) {
		register struct job *jp;
		if (check_if & JOB_STOPPED) {
			for (jp = joblst; jp; jp = jp->j_nxtp) {
				if (jp->j_jid && (jp->j_flag & J_STOPPED)) {
					warning(0, jobsstopped, jobsstoppedid);
					return 0;
				}
			}
		}
		if (check_if & JOB_RUNNING) {
			for (jp = joblst; jp; jp = jp->j_nxtp) {
				if (jp->j_jid && (jp->j_flag & J_RUNNING)) {
					warning(0, jobsrunning, jobsrunningid);
					return 0;
				}
			}
		}
	}

	if (svpgid != mypgid) {
		(void)settgid(svtgid, mypgid);
		(void)setpgid(0, svpgid);
	}

	return 1;
}

/*
** Function: deallocjob()
**
** Notes: This function frees the currently foregrounded job.
*/
void
deallocjob()
{
	free(thisjob);
	jobcnt--;
}

/*
** Function: allocjob()
**
** Notes: This function is called by the shell to reserve a job slot for
** a job about to be spawned 
*/
void
allocjob(cmd, cwd, monitor)
register char *cmd;
register unchar *cwd;
int monitor;
{
	register struct job *jp,**jpp;
	register int jid, cmdlen, cwdlen;

	cmdlen = strlen(cmd) + 1;
	if (cmd[cmdlen-2] == '&') {
		cmd[cmdlen-3] = 0;
		cmdlen -= 2;
	}
	cwdlen = strlen((char *)cwd) + 1;
	jp = (struct job *)alloc(sizeof(struct job)+cmdlen+cwdlen);
	if (jp == 0)
		error(0, nospace, nospaceid);
	jobcnt++;
	jp->j_cmd = ((char *)jp) + sizeof(struct job);
	(void)strcpy(jp->j_cmd,cmd);
	jp->j_pwd = jp->j_cmd + cmdlen;
	(void)strcpy(jp->j_pwd, (char *)cwd);

	jpp = &joblst;

	if (monitor) {
		for (; *jpp; jpp = &(*jpp)->j_nxtp)
			if ((*jpp)->j_jid != 0)
				break;
		for (jid = 1; *jpp; jpp = &(*jpp)->j_nxtp, jid++)
			if ((*jpp)->j_jid != jid)
				break;
	} else
		jid = 0;

	jp->j_jid = jid;
	nextjob = jpp;
	thisjob = jp;
}

/*
** Function: clearjobs()
**
** Notes: This function completely shuts job control down and clears out
** all job control data structures.
*/
void
clearjobs()
{
	register struct job *jp, *sjp;

	for (jp = joblst; jp; jp = sjp) {
		sjp = jp->j_nxtp;
		free(jp);
	}
	joblst = NULL;
	jobcnt = 0;
	jobnote = 0;
	jobdone = 0;

}

/*
** Function: makejob()
**
** Restrictions:
**		nice none
**
** Notes: This function sets up the current process as a job.  The
** nice() call uses a fixed value, so it will not pose a problem if
** executed with privilege.
*/
void
makejob(monitor, fg)
int monitor, fg;
{
	if (monitor) {
		mypgid = mypid;
		(void)setpgid(0, 0);
		if (fg)
			(void)tcsetpgrp(0, mypid);
		handle(SIGTTOU,SIG_DFL);
		handle(SIGTSTP,SIG_DFL);
	} else if (!fg) {
#ifdef NICE
		nice(NICE);
#endif
		handle(SIGTTIN, SIG_IGN);
		handle(SIGINT,  SIG_IGN);
		handle(SIGQUIT, SIG_IGN);
		if (!ioset)
			renam(chkopen(devnull), 0);
	}
}

/*
** Function: postjob()
**
** Notes: This function is called by the shell after job has been
** spawned, to fill in the job slot, and wait for the job if in the
** foreground.
*/

void
postjob(pid, fg)
pid_t pid;
int fg;
{

	register propts;

	thisjob->j_nxtp = *nextjob;
	*nextjob = thisjob;
	thisjob->j_curp = jobcur;
	jobcur = thisjob;

	if (thisjob->j_jid) {
		thisjob->j_pgid = pid;
		propts = PR_JID|PR_PGID;
	} else {
		thisjob->j_pgid = mypgid;
		propts = PR_PGID;
	}

	thisjob->j_flag = J_RUNNING;
	thisjob->j_tgid = thisjob->j_pgid;
	thisjob->j_pid = pid;
	eofflg = 0;

	if (fg) {
		thisjob->j_flag |= J_FOREGND;
		waitjob(thisjob);
	} else  {
		if  (flags & ttyflg)
			printjob(thisjob, propts);
		assnum(&pcsadr, (long)pid);
	}
}

/*
** Function: sysjobs()
**
** Restrictions:
**		execexp()	should not use any privilege.
**
** Notes: This function implements the builtin "jobs" command.  It can
** cause a command line to execute through "execexp()" so privilege must
** be turned off at that point.
*/

void
sysjobs(argc,argv)
int argc;
char *argv[];
{
	extern int opterr;
	register int cmd = SYSJOBS;
	register struct job *jp;
	register propts, c;
	int savoptind = optind;
	int loptind = -1;
	int savopterr = opterr;
	int savsp = _sp;
	char *savoptarg = optarg;

	optind = 1;
	opterr = 0;
	_sp = 1;
	propts = 0;

	if ((flags & jcflg) == 0)
		error(cmd, nojc, nojcid);

	while ((c = getopt(argc, argv, "lpx")) != -1) {
		if (propts) {
			prusage(cmd, jobsuse, jobsuseid);
			goto err;
		}
		switch(c) {
			case 'x':
				propts = -1;
				break;
			case 'p':
				propts = PR_PGID;
				break;
			case 'l':
				propts = PR_LONG;
				break;
			case '?':
				prusage(cmd, jobsuse, jobsuseid);
				goto err;
		}
	}

	loptind = optind;
err:
	optind = savoptind;
	optarg = savoptarg;
	opterr = savopterr;
	_sp = savsp;
	if (loptind == -1)
		return;
	
	if (propts == -1) {
		register unsigned char *bp;
		register char *cp;
		unsigned char *savebp;
		for (savebp = bp = locstak(); loptind < argc; loptind++) {
			cp = argv[loptind];
			if (*cp == '%') {
				jp = str2job(cmd, cp, 1);
				itos(jp->j_pid);
				cp = (char *)numbuf;
			}
			while (*cp)
				*bp++ = *cp++;
			*bp++ = SP;
		}
		(void)endstak(bp);
		execexp(savebp,0);
		return;
	}

	collectjobs(WNOHANG);

	if (propts == 0)
		propts = PR_DFL;

	if (loptind == argc) {
		for (jp = joblst; jp; jp = jp->j_nxtp) {
			if (jp->j_jid)
				printjob(jp,propts);
		}
	} else do
		printjob(str2job(cmd, argv[loptind++], 1), propts);
	while (loptind < argc);

}

/*
** Function: sysfgbg()
**
** Notes: This function implements the builtin "fg" and "bg" commands.
*/
void
sysfgbg(argc,argv)
int argc;
char *argv[];
{
	register int	cmd;
	register fg;

	if (fg = eq("fg",argv[0]))
		cmd = SYSFG;
	else
		cmd = SYSBG;

	if ((flags & jcflg) == 0)
		error(cmd, nojc, nojcid);

	if (*++argv == 0) {
		struct job *jp;
		for (jp = jobcur; ; jp = jp->j_curp) {
			if (jp == 0)
				error(cmd, nocurjob, nocurjobid);
			if (jp->j_jid)
				break;
		}
		restartjob(jp,fg);
	}

	else do
		restartjob(str2job(cmd, *argv, 1), fg);
	while (*++argv);
}

/*
** Function: syswait()
**
** Notes: This function implements the builtin "wait" commands.
*/

void
syswait(argc,argv)
int argc;
char *argv[];
{
	register int cmd = (int)*argv;
	register struct job *jp;
	int stat;

	if (argc == 1)
		collectjobs(0);
	else while (--argc) {
		if ((jp = str2job(cmd, *++argv, 0)) == 0)
			continue;
		if (!(jp->j_flag & J_RUNNING))
			continue;
		if (waitpid(jp->j_pid,&stat,WUNTRACED) <= 0)
			break;
		(void)statjob(jp,stat,0,1);
	}
}

/*
** Function: sigv()
**
** Restrictions
**		kill() none
**
** Notes: This function is used both within job control and outside of
** it. When it is used by job control routines it should not be granted
** privilege, but when it is used otherwise it should have the privilege
** of the current shell.
*/
static void
sigv(cmd, sig, args)
	int	cmd;
	int sig;
	char *args;
{
	int pgrp = 0;
	pid_t id;

	if (*args == '%') {
		register struct job *jp;
		jp = str2job(cmd, args, 1);
		id = jp->j_pgid;
		pgrp++;
	} else {
		if (*args == '-') {
			pgrp++;
			args++;
		}
		id = 0;
		do {
			if (*args < '0' || *args > '9') {
				error_fail(cmd, badid, badidid);
				return;
			}
			id = (id * 10) + (*args - '0');
		} while (*++args);
		if (id == 0) {
			id = mypgid;
			pgrp++;
		}
	}


	if (pgrp)
		id = -id;

	if(sig == SIGSTOP && id == mysid)	{
		error(cmd, loginsh, loginshid);
		return;
	}

	if (kill(id, sig) < 0) {

		switch (errno) {
			case EPERM:
				error_fail(cmd, eacces, eaccesid);
				break;

			case EINVAL:
				error_fail(cmd, badsig, badsigid);
				break;

			default:
				if (pgrp)
					error_fail(cmd, nosuchpgid,
								nosuchpgidid);
				else
					error_fail(cmd, nosuchpid,
								nosuchpidid);
				break;
		}

	} else if (sig == SIGTERM)
		(void)kill(id, SIGCONT);


}

/*
** Function: sysstop()
**
** Restrictions:
**		sigv()	- Should have no privileges
**
** Notes: This function implements the job control built-in "stop." It
** should, therefore, have no need for privilege.
*/
void
sysstop(argc,argv)
int argc;
char *argv[];
{

	int	cmd = SYSSTOP;

	if (argc <= 1)
		pr_usage(cmd, stopuse, stopuseid);
	while (*++argv)
		sigv(cmd, SIGSTOP, *argv);
}

/*
** Function: syskill()
**
** Restrictions:
**		sigv()	- none
**
** Notes: This function implements the "kill" built-in.  It is not a job
** control function as such, but it can use the job control data
** structures to find a process based on jobid.  To work correctly, this
** command must work with the entire privilege set of the current shell.
*/
void
syskill(argc,argv)
int argc;
char *argv[];
{
	int cmd = SYSKILL;
	int sig = SIGTERM;

	if (argc == 1) {
		prusage(cmd, killuse, killuseid);
		return;
	}

	if (argv[1][0] == '-') {

		if (argc == 2) {

			register i;
			register cnt = 0;
			register char sep = 0;
			char buf[12];

			if (!eq(argv[1],"-l")) {
				prusage(cmd, killuse, killuseid);
				return;
			}

			for (i = 1; i < MAXTRAP; i++) {
				if (sig2str(i,buf) < 0)
					continue;
				if (sep)
					prc_buff(sep);
				prs_buff(buf);
				if ((flags & ttyflg) && (++cnt % 10))
					sep = TAB;
				else
					sep = NL;
			}
			prc_buff(NL);
			return;
		}

		if (str2sig(&argv[1][1],&sig)) {
			error_fail(cmd, badsig, badsigid);
			return;
		}
		argv++;
	}

	while (*++argv)
		sigv(cmd, sig, *argv);

}

/*
** Function: sysstop()
**
** Restrictions:
**		kill()	- Should have no privileges
**
** Notes: This function implements the job control built-in "suspend." It
** should, therefore, have no need for privilege.
*/
void
syssusp(argc, argv)
int argc;
char *argv[];
{

	if (argc != 1)
		error(SYSSUSP, badopt, badoptid);

	if (svpgid == mysid)
		error(SYSSUSP, loginsh, loginshid);

	if (mypgid != svpgid) {
		(void)settgid(svtgid, mypgid);
		(void)setpgid(0, svpgid);
	}

	(void)kill(-svpgid, SIGSTOP);
	if (mypgid != svpgid) {
		(void)setpgid(0, mypgid);
		(void)settgid(mypgid, svtgid);
	}
}
