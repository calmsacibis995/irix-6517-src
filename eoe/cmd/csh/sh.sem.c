/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.5 $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.

		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley Software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <rpcsvc/ypclnt.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include "sh.h"
#include "sh.proc.h"
#include "sh.wconst.h"

static void	chkclob(wchar_t *);
static void	doio(struct command *, int *, int *);

/*
 * C shell
 */

void
execute(struct command *t, int wanttty, ...)
{
	struct biltins *bifunc;
	int pid = 0;
	bool forked = 0;
	int pv[2];
	int *pipein = NULL, *pipeout = NULL;
	va_list ap;

#ifdef TRACE
	tprintf("TRACE- execute()\n");
#endif
	if( !t)
	    return;
	if((t->t_dflg & FAND) && wanttty > 0)
	    wanttty = 0;

	switch(t->t_dtyp) {

	case TCOM:
		if(t->t_dcom[0][0] == (wchar_t)S_TOPBIT[0])
/*XXXX bad copy*/
			wscpy(t->t_dcom[0], t->t_dcom[0] + 1);
		if( !(t->t_dflg & FREDO))
			Dfix(t);		/* $ " ' \ */
		if( ! t->t_dcom[0])
			return;
	case TPAR:
		va_start(ap, wanttty);
		pipein = va_arg(ap, int *);
		pipeout = va_arg(ap, int *);
		va_end(ap);
		if (t->t_dflg & FPOU)
			mypipe(pipeout);
		/*
		 * Must do << early so parent will know
		 * where input pointer should be.
		 * If noexec then this is all we do.
		 */
		if (t->t_dflg & FHERE) {
			(void) close(0);
			heredoc(t->t_dlef);
			if (noexec)
				(void) close(0);
		}
		if(noexec)
			break;
		set(S_status, S_0);

		/*
		 * This mess is the necessary kludge to handle the prefix
		 * builtins: nice, nohup, time.  These commands can also
		 * be used by themselves, and this is not handled here.
		 * This will also work when loops are parsed.
		 */
		while(t->t_dtyp == TCOM) {
			if (eq(t->t_dcom[0], S_nice /*"nice"*/))
				if (t->t_dcom[1])
					/*if (any(t->t_dcom[1][0], "+-"))*/
					if (t->t_dcom[1][0] == '+' ||
					    t->t_dcom[1][0] == '-')
						if (t->t_dcom[2]) {
							setname(S_nice /*"nice"*/);
							t->t_nice = getn(t->t_dcom[1]);
							lshift(t->t_dcom, 2);
							t->t_dflg |= FNICE;
						} else
							break;
					else {
						t->t_nice = 4;
						lshift(t->t_dcom, 1);
						t->t_dflg |= FNICE;
					}
				else
					break;
			else if (eq(t->t_dcom[0], S_nohup /*"nohup"*/))
				if (t->t_dcom[1]) {
					t->t_dflg |= FNOHUP;
					lshift(t->t_dcom, 1);
				} else
					break;
			else if (eq(t->t_dcom[0], S_time /*"time"*/))
				if (t->t_dcom[1]) {
					t->t_dflg |= FTIME;
					lshift(t->t_dcom, 1);
				} else
					break;
			else
				break;
		}
		/*
		 * Check if we have a builtin function and remember which one.
		 */
		bifunc = (t->t_dtyp == TCOM)? isbfunc(t) : (struct biltins *)0;

		/*
		 * We fork only if we are timed, or are not the end of
		 * a parenthesized list and not a simple builtin function.
		 * Simple meaning one that is not pipedout, niced, nohupped,
		 * or &'d.
		 * It would be nice(?) to not fork in some of these cases.
		 */
		if(((t->t_dflg & FTIME)
		    || !(t->t_dflg & FPAR)
		    && (!bifunc || t->t_dflg & (FPOU|FAND|FNICE|FNOHUP)))) {
			forked++;
			pid = pfork(t, wanttty);
		}
		if(pid) {
			/*
			 * It would be better if we could wait for the
			 * whole job when we knew the last process
			 * had been started.  Pwait, in fact, does
			 * wait for the whole job anyway, but this test
			 * doesn't really express our intentions.
			 */
			if( !didfds && t->t_dflg&FPIN) {
				(void)close(pipein[0]);
				(void)close(pipein[1]);
			}
			if( !(t->t_dflg & (FPOU | FAND)))
				pwait();
			break;
		}
		doio(t, pipein, pipeout);
		if(t->t_dflg & FPOU) {
			(void)close(pipeout[0]);
			(void)close(pipeout[1]);
		}

		/*
		 * Perform a builtin function.
		 * If we are not forked, arrange for possible stopping
		 */
		if(bifunc) {
			func(t, bifunc);
			if(forked)
				exitstat();
			break;
		}
		if (t->t_dtyp != TPAR) {
			doexec(t);
			/*NOTREACHED*/
		}
		/*
		 * For () commands must put new 0,1,2 in FSH* and recurse
		 */
		OLDSTD = dcopy(0, FOLDSTD);
		SHOUT = dcopy(1, FSHOUT);
		SHDIAG = dcopy(2, FSHDIAG);
		(void) close(SHIN);
		SHIN = -1;
		didfds = 0;
		wanttty = -1;
		t->t_dspr->t_dflg |= t->t_dflg & FINT;
		execute(t->t_dspr, wanttty);
		exitstat();

	case TFIL:
		va_start(ap, wanttty);
		pipein = va_arg(ap, int *);
		pipeout = va_arg(ap, int *);
		va_end(ap);
		t->t_dcar->t_dflg |= FPOU |
		    (t->t_dflg & (FPIN|FAND|FDIAG|FINT));
		execute(t->t_dcar, wanttty, pipein, pv);
		t->t_dcdr->t_dflg |= FPIN |
		    (t->t_dflg & (FPOU|FAND|FPAR|FINT));
		if (wanttty > 0)
			wanttty = 0;		/* got tty already */
		execute(t->t_dcdr, wanttty, pv, pipeout);
		break;

	case TLST:
		if (t->t_dcar) {
			t->t_dcar->t_dflg |= t->t_dflg & FINT;
			execute(t->t_dcar, wanttty);
			/*
			 * In strange case of A&B make a new job after A
			 */
			if (t->t_dcar->t_dflg&FAND && t->t_dcdr &&
			    (t->t_dcdr->t_dflg&FAND) == 0)
				pendjob();
		}
		if (t->t_dcdr) {
			t->t_dcdr->t_dflg |= t->t_dflg & (FPAR|FINT);
			execute(t->t_dcdr, wanttty);
		}
		break;

	case TOR:
	case TAND:
		if (t->t_dcar) {
			t->t_dcar->t_dflg |= t->t_dflg & FINT;
			execute(t->t_dcar, wanttty);
			if((getn(value(S_status)) == 0) != (t->t_dtyp == TAND))
				return;
		}
		if (t->t_dcdr) {
			t->t_dcdr->t_dflg |= t->t_dflg & (FPAR|FINT);
			execute(t->t_dcdr, wanttty);
		}
		break;
	}
	/*
	 * Fall through for all breaks from switch
	 *
	 * If there will be no more executions of this
	 * command, flush all file descriptors.
	 * Places that turn on the FREDO bit are responsible
	 * for doing donefds after the last re-execution
	 */
	if (didfds && !(t->t_dflg & FREDO))
		donefds();
}

/*
 * Perform io redirection.
 * We may or maynot be forked here.
 */
static void
doio(struct command *t, int *pipein, int *pipeout)
{
	register wchar_t *cp;
	register int flags = t->t_dflg;
#if 0 /* Follow tcsh example: yp auto-unbinds if new pid */
	char name[256];
#endif
	wchar_t buf[MB_MAXPATH];

#ifdef TRACE
	tprintf("TRACE- doio()\n");
#endif
	/*
	 * This code used to be:
	 *
	 * if (didfds || (flags & FREDO))
	 *	return;
	 *
	 * The change is to allow the following statement to work:
	 *
	 *	eval 'echo foo > bar'
	 *
	 * The problem is that doio is called twice:  once for eval
	 * and then again for echo.  The second time, didfds == 1.
	 */
	if(didfds && !t->t_dlef && !t->t_drit || (flags & FREDO))
	    return;
#if 0 /* Follow tcsh example: yp auto-unbinds if new pid */
	/*
	 * Since the yellow pages keeps sockets open, close them after
	 * globbing for input/output is done.
	 */
	getdomainname(name, sizeof(name));
	yp_unbind(name);
#endif

	if ((flags & FHERE) == 0) {	/* FHERE already done */
		(void)close(0);
		if (cp = t->t_dlef) {
			cp = globone(Dfix1(cp));
#if 0 /* Follow tcsh example: yp auto-unbinds if new pid */
			yp_unbind(name);
#endif
			if (open_(cp, O_RDONLY) < 0) {
				(void)wscpy(buf, cp);
				xfree(cp);
				Perror(buf);
				/* never returns */
			}
			xfree(cp);
		} else if (flags & FPIN) {
			(void) dup(pipein[0]);
			(void) close(pipein[0]);
			(void) close(pipein[1]);
		} else if ((flags & FINT) && tpgrp == -1) {
			(void) close(0);
			(void) open("/dev/null", 0);
		} else
			(void) dup(OLDSTD);
	}
	(void) close(1);
	if (cp = t->t_drit) {
		cp = globone(Dfix1(cp));
#if 0 /* Follow tcsh example: yp auto-unbinds if new pid */
		yp_unbind(name);
#endif
		if ((flags & FCAT) && open_(cp, O_WRONLY) >= 0)
			(void) lseek(1, 0, SEEK_END);
		else {
			if (!(flags & FANY) && adrof(S_noclobber)) {
				if (flags & FCAT) {
					(void)wscpy(buf, cp);
					xfree(cp);
					Perror(buf);
					/* never returns */
				}
				chkclob(cp);
			}
			if (creat_(cp, 0666) < 0) {
				(void)wscpy(buf, cp);
				xfree(cp);
				Perror(buf);
				/* never returns */
			}
		}
		xfree(cp);
	} else if (flags & FPOU)
		(void) dup(pipeout[1]);
	else
		(void) dup(SHOUT);

	(void) close(2);
	if (flags & FDIAG)
		(void) dup(1);
	else
		(void) dup(SHDIAG);
	didfds = 1;
}

void
mypipe(int *pv)
{

#ifdef TRACE
	tprintf("TRACE- mypipe()\n");
#endif
	if (pipe(pv) < 0)
		goto oops;
	pv[0] = dmove(pv[0], -1);
	pv[1] = dmove(pv[1], -1);
	if (pv[0] >= 0 && pv[1] >= 0)
		return;
oops:
	error(gettxt(_SGI_DMMX_csh_nopipe, "Can't make pipe"));
}

static void
chkclob(wchar_t *cp)
{
	struct stat stb;
	mode_t type;
	char chbuf[MAXNAMELEN * MB_LEN_MAX];

#ifdef TRACE
	tprintf("TRACE- chkclob()\n");
#endif
	if (stat_(cp, &stb) < 0)
		return;
	type = stb.st_mode & S_IFMT;
	if (type == S_IFCHR || type == S_IFIFO)
		return;
	error(gettxt(_SGI_DMMX_csh_fexists, "%s: File exists"),
	    tstostr(chbuf, cp, NOFLAG));
}
