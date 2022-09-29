/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:main.c	1.14.24.1"
/***************************************************************************
 * Command: sh
 * Inheritable Privileges: P_ALLPRIVS
 *       Fixed Privileges: None
 * Notes:
 *
 * The UNIX shell
 *
 *
 ***************************************************************************/

#include	"defs.h"
#include	"sym.h"
#include	"timeout.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/wait.h>
#include	<pfmt.h>
#include	"hash.h"
#include        "dup.h"

#ifdef RES
#include	<sgtty.h>
#endif

pid_t mypid, mypgid, mysid;
pid_t svpgid, svtgid;
uid_t sveuid;

static BOOL	beenhere = FALSE;
unsigned char		tmpout[20] = "/tmp/sh-";
static struct fileblk	stdfile;
struct fileblk *standin = &stdfile;
int mailchk = 0;

static unsigned char	*mailp;
static long	*mod_time = 0;


#if vax
char **execargs = (char**)(0x7ffffffc);
#endif

#if pdp11
char **execargs = (char**)(-2);
#endif

void	settmp();
void	setmode();
void	setmail();
void	setwidth();
void	chkpr();

static void	exfile();
static void	Ldup();
static void	chkmail();

extern int close();
extern int fcntl();
extern int setgid();
extern int setuid();
extern uid_t getgid();
extern uid_t getegid();
extern uid_t getuid();
extern uid_t geteuid();
extern pid_t getpid();
extern pid_t getsid();
extern pid_t getpgid();

main(c, v, e)
int	c;
char	*v[];
char	*e[];
{
	register int	rflag = ttyflg;
	int		rsflag = 1;	/* local restricted flag */
	register unsigned char *flagc = flagadr;
	struct namnod	*n;

	(void)setcat("uxcore.abi");

	mypid = getpid();
	svpgid = mypgid = getpgid(mypid);
	mysid = getsid(mypid);
	svtgid = tcgetpgrp(0);

	/*
	 * initialize storage allocation
	 */

	stakbot = 0;
	addblok((unsigned)0);

	(void)setlocale(LC_MESSAGES, "");

	/*
	 * a shell is also restricted if the simple name of argv(0) is
	 * rsh or -rsh in its simple name
	 */

#ifndef RES

	if (c > 0 && (eq("rsh", simple(*v)) || eq("-rsh", simple(*v))))
		rflag = 0;

#endif

	if (eq("jsh", simple(*v)) || eq("-jsh", simple(*v)))
		flags |= monitorflg;

	stdsigs();

	/*
	 * set names from userenv
	 */

	setup_env();

#if SECURITY_FEATURES
	/*
	 * Get the shell timeout value from the environment.
	 */
	dfault(&tmoutnod, TIMEOUT);
	attrib((&tmoutnod), (N_EXPORT | N_ENVNAM));
	timeout = (unsigned)stoi(tmoutnod.namval);

	/*
	 * Set up the $TFADMIN variable for shell scripts and such that
	 * need it.
	 */
	setup_tfm();
#else
	timeout = 0;
#endif /* SECURITY_FEATURES */

	/*
	 * 'rsflag' is zero if SHELL variable is
	 *  set in environment and 
	 *  the simple file part of the value.
	 *  is rsh
	 */
	if (n = findnam("SHELL"))
	{
		if (eq("rsh", simple(n->namval)))
			rsflag = 0;
	}

	hcreat();
	set_dotpath();
	
	/* initialize multibyte information */
	setwidth();

	/*
	 * look for options
	 * dolc is $#
	 */
	dolc = options(c, v);

	if (dolc < 2)
	{
		flags |= stdflg;
		{

			while (*flagc)
				flagc++;
			*flagc++ = STDFLG;
			*flagc = 0;
		}
	}
	if ((flags & stdflg) == 0)
		dolc--;

#if defined(ENABLE_SVR4_EUID_RESETTING)
	/*
	 * This code is being disabled in IRIX on the following grounds:
	 *   1) it breaks compatibility with some IRIX 4.x applications.
	 *   2) it doesn't add any really meaningful additional security
	 *      to the system to balance 1).  A badly behaved program
	 *      or programmer can subvert this by setting real uid to
	 *      the effective uid before calling system(3), for example.
	 *   3) the special meaning being attached to 100 is completely
	 *      arbitrary and doesn't agree with any recognized
	 *      conventions in IRIX.
	 */
	if ((flags & privflg) == 0) {
		register uid_t euid;
		register gid_t egid;
		register uid_t ruid;
		register gid_t rgid;

		/*
		 Determine all of the user's id #'s for this process and
		 then decide if this shell is being entered as a result
		 of a fork/exec.
		 If the effective uid/gid do NOT match and the euid/egid
		 is < 100 and the egid is NOT 1, reset the uid and gid to
		 the user originally calling this process.
		 */
		euid = geteuid();
		ruid = getuid();
		egid = getegid();
 	      	rgid = getgid();
		if ((euid != ruid) && (euid < 100))
			(void)setuid(ruid);/* reset the uid to the orig user */
		if ((egid != rgid) && ((egid < 100) && (egid != 1)))
			(void)setgid(rgid);/* reset the gid to the orig user */
	}
#endif /* DISABLED_IN_IRIX */
	/* save euid for path name access checks */
	sveuid = geteuid();

	dolv = (unsigned char **)v + c - dolc;
	dolc--;

	/*
	 * return here for shell file execution
	 * but not for parenthesis subshells
	 */
	if (setjmp(subshell)) {
		freejobs();
		flags |= subsh;
	}

	/*
	 * number of positional parameters
	 */
	replace(&cmdadr, dolv[0]);	/* cmdadr is $0 */

	/*
	 * set pidname '$$'
	 */
	assnum(&pidadr, (long)mypid);

	/*
	 * set up temp file names
	 */
	settmp();

	/*
	 * default internal field separators - $IFS
	 */
	dfault(&ifsnod, sptbnl);

	dfault(&mchknod, MAILCHECK);
	mailchk = stoi(mchknod.namval);

	/* initialize OPTIND for getopt */
	
	n = lookup("OPTIND");
	assign(n, "1");
	/*
	 * make sure that option parsing starts 
	 * at first character
	 */
	_sp = 1;
	
	if ((beenhere++) == FALSE)	/* ? profile */
	{
		if (*(simple(cmdadr)) == '-' && (flags & privflg) == 0)
		{			/* system profile */

#ifndef RES

			if ((input = pathopen(nullstr, sysprofile)) >= 0)
				exfile(rflag);		/* file exists */

#endif

			if ((input = pathopen(nullstr, profile)) >= 0)
			{
				exfile(rflag);
				flags &= ~ttyflg;
			}
		}
		if (rsflag == 0 || rflag == 0) {
			if((flags & rshflg) == 0) {
				while(*flagc)
					flagc++;
				*flagc++ = 'r';
				*flagc = '\0';
			}
			flags |= rshflg;
		}

		/*
		 * open input file if specified
		 */
		if (comdiv)
		{
			estabf(comdiv);
			input = -1;
		}
		else
		{
			input = ((flags & stdflg) ? 0 : chkopen(cmdadr));

#ifdef ACCT
			if (input != 0)
				preacct(cmdadr);
#endif
			comdiv--;
		}
	}
#ifdef pdp11
	else
		*execargs = (char *)dolv;	/* for `ps' cmd */
#endif
		
	exfile(0);
	done(0);
/*NOTREACHED*/
}

static void
exfile(prof)
BOOL	prof;
{
	long	mailtime = 0;	/* Must not be a register variable */
	long 	curtime = 0;

	/*
	 * move input
	 */
	if (input > 0)
	{
		Ldup(input, INIO);
		input = INIO;
	}


	setmode(prof);

	if (setjmp(errshell) && prof)
	{
		(void)close(input);
		(void)endjobs(0);
		return;
	}
	/*
	 * error return here
	 */

	loopcnt = peekc = peekn = 0;
	fndef = 0;
	nohash = 0;
	iopend = 0;

	if (input >= 0)
		initf(input);
	/*
	 * command loop
	 */
	for (;;)
	{
		tdystak(0);
		stakchk();	/* may reduce sbrk */
		exitset();

		if ((flags & prompt) && standin->fstak == 0 && !eof)
		{

			if (mailp)
			{
				(void)time(&curtime);

				if ((curtime - mailtime) >= mailchk)
				{
					chkmail();
				        mailtime = curtime;
				}
			}

			/* necessary to print jobs in a timely manner */
			if (trapnote & TRAPSET)
				chktrap();

			prs(ps1nod.namval);

			if(timeout)	{
				(void)alarm(timeout);
			}

		}

		trapnote = 0;
		peekc = readc();
		if (eof) {
			if (endjobs(JOB_STOPPED))
				return;
			eof = 0;
		} 

		if(timeout)	{
			(void)alarm(0);
		}

		{
			register struct trenod *t;
			t = cmd(NL, MTFLG);
			if (t == NULL && flags & ttyflg)
				freejobs();
			else
				execute(t, 0, eflag);
		}

		eof |= (flags & oneflg);

	}
}

void
chkpr()
{
	if ((flags & prompt) && standin->fstak == 0)
		prs(ps2nod.namval);
}

void
settmp()
{
	int i;
	i = ltos(mypid);
	serial = 0;
	tmpname = movstr(numbuf + i, &tmpout[TMPNAM]);
}

static void
Ldup(fa, fb)
register int	fa, fb;
{
#ifdef RES

	dup(fa | DUPFLG, fb);
	(void)close(fa);
	ioctl(fb, FIOCLEX, 0);

#else

	if (fa >= 0) {
		if (fa != fb) 
		{ 
			(void)close(fb);
			fcntl(fa,0,fb);	/* normal dup */
		  	(void)close(fa);
		}
		fcntl(fb, 2, 1);	/* autoclose for fb */
	}

#endif
}

static void
chkmail()
{
	register unsigned char 	*s = mailp;
	register unsigned char	*save;

	long	*ptr = mod_time;
	unsigned char	*start;
	BOOL	flg; 
	struct stat	statb;

	while (*s)
	{
		start = s;
		save = 0;
		flg = 0;

		while (*s)
		{
			if (*s != COLON)	
			{
				if (*s == '%' && save == 0)
					save = s;
			
				s++;
			}
			else
			{
				flg = 1;
				*s = 0;
			}
		}

		if (save)
			*save = 0;

		if (*start && stat((const char *)start, &statb) >= 0)
		{
			if(statb.st_size && *ptr
				&& statb.st_mtime != *ptr)
			{
				if (save)
				{
					prs(save+1);
					newline();
				}
				else
					prs(gettxt(mailmsgid, mailmsg));
			}
			*ptr = statb.st_mtime;
		}
		else if (*ptr == 0)
			*ptr = 1;

		if (save)
			*save = '%';

		if (flg)
			*s++ = COLON;

		ptr++;
	}
}

void
setmail(mailpath)
	unsigned char *mailpath;
{
	register unsigned char	*s = mailpath;
	register int 	cnt = 1;

	long	*ptr;

	free(mod_time);
	mailp = mailpath;
	if (mailp)
	{
		while (*s)
		{
			if (*s == COLON)
				cnt += 1;

			s++;
		}

		ptr = mod_time = (long *)alloc(sizeof(long) * cnt);

		while (cnt)
		{
			*ptr = 0;
			ptr++;
			cnt--;
		}
	}
}

void
setwidth()
{
	unsigned char *name = lookup("LC_CTYPE")->namval;
	if(!name || !*name)
		name = lookup("LANG")->namval;
	/*
	** The following condition is for compatibility with the past.
	*/
	if(!name || !*name)
		name = lookup("CHRCLASS")->namval;

	if(!name || !*name)
		(void)setlocale(LC_CTYPE, "C");
	else
		(void)setlocale(LC_CTYPE, (const char *)name);
}

void
setmode(prof)
{
	/*
	 * decide whether interactive
	 */

	if ((flags & intflg) ||
	    ((flags&oneflg) == 0 &&
	    output >= 0 && isatty(output) &&
	    input >= 0 && isatty(input)) )
	    
	{
		dfault(&ps1nod, (geteuid() ? stdprompt : supprompt));
		dfault(&ps2nod, readmsg);
		flags |= ttyflg | prompt;
		if (mailpnod.namflg != N_DEFAULT)
			setmail(mailpnod.namval);
		else
			setmail(mailnod.namval);
		startjobs();
	}
	else
	{
		flags |= prof;
		flags &= ~prompt;
	}
}
