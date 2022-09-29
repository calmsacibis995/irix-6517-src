/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1990, 1991, 1992, 1993 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)make:dosys.c	1.19.1.2"
#include "defs"
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <pfmt.h>
#include <locale.h>
#include <paths.h>

#ifndef MAKE_SHELL
#define MAKE_SHELL	_PATH_BSHELL
#endif

/*
**	Declate local functions and make LINT happy.
*/

static int	any_metas();
static int	doshell();
static int	await();
static void	doclose();
static int	doexec();
static pid_t waitpid;

int
dosys(comstring, nohalt, Makecall, tarp)	/* do the system call */
CHARSTAR comstring;
int	nohalt, Makecall;
NAMEBLOCK tarp;
{
	register CHARSTAR str = comstring;

	while (	*str == BLANK || *str == TAB ) 
		str++;
	if (!*str)
		return(-1);	/* no command */

	if (IS_ON(NOEX) && !Makecall)
		return(0);

	if(IS_ON(BLOCK)){
		if(!tarp->tbfp)
			(void)open_block(tarp);
		else
			(void)fflush(tarp->tbfp);

		(void)fflush(stdout);
		(void)fflush(stderr);
			
	}

	/* If meta-chars are in comstring, automatically use the shell to
	 * execute the command.  Else, try execing the command directly.
	 * This is because of
	 * a problem in the cshell that will not let it interpret /bin/sh
	 * scripts if invoked as `csh script' (where script is the name of
	 * the (sh) script file).  (A return of -1 means that a null command
	 * was passed.)
 	 */
	if (any_metas(str) || *str != '/')
		return( doshell(str, nohalt, tarp) );
	else
		return( doexec(str, tarp) );
}


static int
any_metas(s)		/* Are there are any Shell meta-characters? */
register CHARSTAR s;
{
	while ( *s )
		if ( funny[(unsigned char) *s++] & META )
			return(YES);
	return(NO);
}


static int
doshell(comstring, nohalt, tarp)
CHARSTAR comstring;
int	nohalt;
NAMEBLOCK tarp;
{
	void	enbint();
	VARBLOCK srchvar();
	register VARBLOCK p;

#ifdef MKDEBUG
	if (IS_ON(DBUG)) 
		printf("comstring=<%s>\n",comstring);
#endif
	if ( !(waitpid = fork()) ) {
		char evalshell[1024];
		char *bs, *subst();
		void	setenv();
		char *getenv(), *myshell;
		enbint(SIG_DFL);
		doclose(tarp);
		setenv();

		if(IS_ON(POSIX)) {
			myshell = (varptr("SHELL"))->varval.charstar;
		} else {
			if (((myshell = getenv("SHELL")) == NULL) || (*myshell == CNULL)) {
				if ((p = srchvar("SHELL")) == NULL)
					myshell = MAKE_SHELL;
				else
					myshell = p->varval.charstar;
			} else {
				subst(myshell, evalshell, 0);
				myshell = evalshell;
			}
		}
		if ((bs = strrchr(myshell, '/')) == NULL)
			bs = myshell;
		else
			bs++;
		if (*myshell == '/')
			(void)execl(myshell, bs,
				STREQ(bs, "csh") ?
				(nohalt ? "-cf" : "-cef") :
				(nohalt ? "-c" : "-ce"),
				comstring, 0);
		else
			(void)execlp(myshell, bs,
				STREQ(bs, "csh") ?
				(nohalt ? "-cf" : "-cef") :
				(nohalt ? "-c" : "-ce"),
				comstring, 0);
		if(errno == E2BIG)
			fatal(":79:couldn't load shell: Argument list too long (bu22)");
		else
			fatal1(_SGI_MMX_make_noshell ":couldn't load shell (bu22):%s",
				strerror(errno));
	} else if ( waitpid == -1 )
		fatal1(_SGI_MMX_make_nofork ":couldn't fork%s", strerror(errno));

	return( await() );
}


static int
await()
{
	register int	pid;
	int	wait(), status;
	void	intrupt();
	void	enbint();

	enbint(intrupt);
	if(IS_ON(PAR))
		return(waitpid);

	while ((pid = wait(&status)) != waitpid)
		if (pid == -1)
			fatal1(_SGI_MMX_make_nowait ":bad wait code (bu23):%s",
				strerror(errno));
	return(status);
}


static void
doclose(tarp)	/* Close open directory files before exec'ing */
NAMEBLOCK tarp;
{
	register OPENDIR od = firstod;
	for (; od; od = od->nextopendir)
		if ( od->dirfc )
			(void)closedir(od->dirfc);

	if(IS_ON(BLOCK)){

		
		(void)close(fileno(stdout));
		
		if(dup(fileno(tarp->tbfp)) < 0 )
			fatal1(":83:Fail to dup block file(%d)",
				fileno(tarp->tbfp));

		(void)close(fileno(stderr));

		if(dup(fileno(tarp->tbfp)) < 0 )
			fatal1(":83:Fail to dup block file(%d)",
				fileno(tarp->tbfp));

		(void)fclose(tarp->tbfp);
	}
}


static int
doexec(str, tarp)
register CHARSTAR str;
NAMEBLOCK tarp;
{
	int	execvp();
	void	enbint();

	if ( *str == CNULL )
		return(-1);	/* no command */
	else {
		register CHARSTAR t = str, *p;
		int incsize = ARGV_SIZE, aryelems = incsize, numelems = 0;
		CHARSTAR *argv = (CHARSTAR *) ck_malloc(aryelems * sizeof(CHARSTAR));

		p = argv;
		for ( ; *t ; ) {
			*p++ = t;
			if (++numelems == aryelems) {
				aryelems += incsize;
				argv = (CHARSTAR *)
					realloc(argv, aryelems * sizeof(CHARSTAR));
				if (!argv) fatal(":84:doexec: out of memory");
				p = argv + (aryelems - incsize);
			}
			while (*t != BLANK && *t != TAB && *t != CNULL)
				++t;
			if (*t)
				for (*t++ = CNULL; *t == BLANK || *t == TAB; ++t)
					;
		}

		*p = NULL;

		if ( !(waitpid = fork()) ) {
			void	setenv();
			enbint(SIG_DFL);
			doclose(tarp);
			setenv();
			(void)execvp(str, argv);
			if(errno == E2BIG)
				fatal1(":85:cannot load %s : Argument list too long (bu24)",
					str);
			else {
				pfmt(stderr, MM_NOSTD, _SGI_MMX_make_noexec
					":cannot load %s (bu24):%s\n", str,
					strerror(errno));
				fatal(NULL);
			}
		} else if ( waitpid == -1 )
			fatal1(_SGI_MMX_make_nofork ":couldn't fork%s",
						strerror(errno));

		free(argv);
		return( await() );
	}
}
