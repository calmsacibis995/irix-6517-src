/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:sh/error.c	1.4.4.2"

/*
 * UNIX shell
 *
 * S. R. Bourne
 * Rewritten by David Korn
 * AT&T Bell Laboratories
 *
 */

#include	"defs.h"
#include	"jobs.h"
#include	"history.h"


/* These routines are used by this module but defined elsewhere */
extern void	mac_check();
extern void	name_unscope();
extern void	rm_files();
#ifdef VFORK
    extern void	vfork_restore();
#endif	/* VFORK */

/* ========	error handling	======== */

	/* Find out if it is time to go away.
	 * `trapnote' is set to SIGSET when fault is seen and
	 * no trap has been set.
	 */

void sh_cfail(message)
MSG message;
{
	sh_fail(sh.cmdname,message,ERROR);
}

/*
 *  This routine is called when fatal errors are encountered
 *  A message is printed out and the shell tries to exit
 */

void sh_fail(s1,s2,errval)
register const char *s1;
register MSG s2;
register int errval;
{
	mac_check();
	p_setout(ERRIO);
	p_prp(s1);
	if(s2)
	{
		p_str(e_colon,0);
		p_str(s2,NL);
	}
	else
		newline();
	sh_exit(errval);
}
/* same as sh_fail, but print errno msg also, if errno set; started
 * using this in io.c.  Unlike sh_fail, s2 must be valid. Olson, 10/96 */
void
sh_fail_why(const char *s1, MSG s2, int errval)
{
        static char ebuf[256];
	if (!s2)
		s2 = NULLSTR;
       	strcpy(ebuf, s2);
        if(errno) {
                strcat(ebuf, ": ");
                strncat(ebuf, strerror(errno), sizeof(ebuf)-(strlen(s2)+3));
        }
        sh_fail(s1, ebuf, errval);
}


/* Arrive here from `FATAL' errors
 *  a) exit command,
 *  b) default trap,
 *  c) fault with no trap set.
 *
 * Action is to return to command level or exit.
 */

void sh_exit(xno)
int xno;
{
	register unsigned state=(st.states&~(ERRFLG|MONITOR));
	sh.exitval=xno;
	if(xno==SIGFAIL)
		sh.exitval |= sh.lastsig;
	sh.un.com = 0;
	if(state&(BUILTIN|LASTPIPE))
	{
#if VSH || ESH
		tty_cooked(-1);
#endif
		io_clear(sh.savio);
		LONGJMP(*sh.freturn,1);
	}
	state |= is_option(ERRFLG|MONITOR);
	if( (state&(ERRFLG|FORKED)) || 
		(!(state&(PROFILE|PROMPT|FUNCTION)) && job_close() >= 0))
	{
		st.states = state;
		st.peekn = 0;
		sh_done(0);
	}
	else
	{
		if(!(state&FUNCTION))
		{
			p_flush();
			name_unscope();
			arg_clear();
			io_clear((struct fileblk*)0);
			io_restore(0);
			if(st.standin)
			{
				*st.standin->last = 0;
				/* flush out input buffer */
				while(finbuff(st.standin)>0)
					io_readc();
			}
		}
#ifdef VFORK
		vfork_restore();
#endif	/* VFORK */
		st.execbrk = st.breakcnt = 0;
		st.exec_flag = st.subflag = 0;
		st.dot_depth = 0;
		hist_flush();
		state &= ~(FUNCTION|FIXFLG|RWAIT|PROMPT|READPR|MONITOR|BUILTIN|
			LASTPIPE|VFORKED|GRACE);
		state |= is_option(INTFLG|READPR|MONITOR);
		job.pipeflag = 0;
		st.states = state;
		LONGJMP(*sh.freturn,1);
	}
}

/*
 * This is the exit routine for the shell
 */

void sh_done(sig)
register int sig;
{
	register char *t;
	register int savxit = sh.exitval;
	if(sh.trapnote&SIGBEGIN)
		return;
	sh.trapnote = 0;
	if(t=st.trapcom[0])
	{
		st.trapcom[0]=0; /*should free but not long */
		sh.oldexit = savxit;
		sh.intrap++;
		sh_eval(t);
		sh.intrap--;
	}
	else
	{
		/* avoid recursive call for set -e */
		st.states &= ~ERRFLG;
		sh_chktrap();
	}
	sh_freeup();
#ifdef ACCT
	doacct();
#endif	/* ACCT */
#if VSH || ESH
	if(is_option(EMACS|EDITVI|GMACS))
		tty_cooked(-1);
#endif
	if(st.states&RM_TMP)
	/* clean up all temp files */
		rm_files(io_tmpname);
	p_flush();
#ifdef JOBS
	if(sig==SIGHUP || (is_option(INTFLG)&&sh.login_sh))
		job_terminate();
#endif	/* JOBS */
	job_close();
	io_sync();
	if(sig)
	{
		/* generate fault termination code */
		signal(sig,SIG_DFL);
		sigrelease(sig);
		kill(getpid(),sig);
		pause();
	}
	_exit(savxit&EXITMASK);
}

