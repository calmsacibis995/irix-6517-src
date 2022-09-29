/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.3 $"
/***************************************************************************
 * Command: sulogin
 *
 * Notes:	special login program exec'd from init to let user
 *		come up single user, or go multi straight away.
 *
 *		Explain the scoop to the user, and prompt for
 *		root password or ^D. Good root password gets you
 *		single user, ^D exits sulogin, and init will
 *		go multi-user.
 *
 *		If /etc/passwd is missing, or there's no entry for root,
 *		go single user, no questions asked.
 *
 *      	11/29/82
 *
 ***************************************************************************/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/*
 *	MODIFICATION HISTORY
 *
 *	M000	01 May 83	andyp	3.0 upgrade
 *	- index ==> strchr.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <termio.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <utmpx.h>
#include <unistd.h>
#include <errno.h>
#include <paths.h>

#ifdef	M_V7

#define	strchr	index
#define	strrchr	rindex

#endif

#define SCPYN(a, b)	(void) strncpy(a, b, sizeof(a))

static	char	*mygetpass(),
		*ttyn = NULL,
		minus[]	= "-",
		*findttyname(),
		shell[]	= "/bin/su",
		SHELL[]	= _PATH_BSHELL;

extern	int
		devstat();


static struct utmpx *u;

static	void	single();

/*
 * Procedure:     main
 *
 * Restrictions:
 *		printf:		None
*/
main()
{
	struct stat info;			/* buffer for stat() call */
	register struct spwd *shpw;
	register struct passwd *pw;
	register char *pass;			/* password from user	  */
	register char *namep;

	if(stat("/etc/passwd",&info) != 0) {	/* if no pass file, single */
		printf("\n**** PASSWORD FILE MISSING! ****\n\n");
		single(0);			/* doesn't return	  */
	}

	setpwent();
	setspent();
	pw = getpwnam("root");		        /* find entry in passwd   */
	shpw = getspnam("root");
	endpwent();
	endspent();

	if(pw == (struct passwd *)0) {		/* if no root entry, single */
		printf("\n**** NO ENTRY FOR root IN PASSWORD FILE! ****\n\n");
		single(0);			/* doesn't return	  */
	}
	if (shpw == (struct spwd *) 0) {
		printf("\n**** NO ENTRY FOR root IN SHADOW FILE! ****\n\n");
		single(0);
		}
	while(1) {
		(void) printf("\nType Ctrl-d to proceed with normal startup,\n");
		(void) printf("(or give root password for Single User Mode): ");

		if((pass = mygetpass()) == (char *)0)
			exit(0);	/* ^D, so straight to multi-user */

		if ( *shpw->sp_pwdp == '\0' ) 
			namep = pass;
		else
			namep = crypt(pass,shpw->sp_pwdp);

		if(strcmp(namep, shpw->sp_pwdp))
			printf("Login incorrect\n");
		else
			single(1);
	}
	/* NOTREACHED */
}


/*
 * Procedure:     single
 *
 * Restrictions:
 *		printf:		None
 *
 * Notes:	exec shell for single user mode. 
*/
static	void
single(ok)
	int	ok;
{
	/*
	 * update the utmpx file.
	 */
	ttyn = findttyname(0);

	if (ttyn == NULL) 
		ttyn = "/dev/???";

	while ((u = getutxent()) != NULL) {
		if (!strcmp(u->ut_line, (ttyn + sizeof("/dev/") - 1))) {
			(void) time(&u->ut_tv.tv_sec);
			u->ut_type = INIT_PROCESS;
			/*
			 * force user to "look" like
			 * "root" in utmp entry.
			*/
			if (strcmp(u->ut_user, "root")) {
				u->ut_pid = getpid();
				SCPYN(u->ut_user, (char *) "root");
			}
			break;
		}
	}
	if (u != NULL) {
		(void) pututxline(u);
	}

	endutxent();		/* Close utmp file */

	(void) printf("Entering Single User Mode\n\n");

	if (ok) 
		(void) execl(shell, shell, minus, (char *)0);
	else
		(void) execl(SHELL, SHELL, minus, (char *)0);

	exit(0);
	/* NOTREACHED */
}



/*
 * Procedure:     getpass
 *
 * Restrictions:
 *		fopen:		None
 *		setbuf:		None
 *		ioctl(2):	None
 *		fprintf:	None
 *		fclose:		None
 *
 * Notes: 	hacked from the stdio library version so we can
 *		distinguish newline and EOF.  Also don't need this
 *		routine to give a prompt.
 *
 * RETURN:	(char *)address of password string (could be null string)
 *
 *			or
 *
 *		(char *)0 if user typed EOF
*/
static char *
mygetpass()
{
	struct	termio	ttyb;
	static	char	pbuf[PASS_MAX+1];
	int		flags;
	register char	*p;
	register c;
	FILE	*fi;
	void	(*sig)();
	char	*rval;		/* function return value */

	fi = stdin;
	setbuf(fi, (char *)NULL);

	sig = signal(SIGINT, SIG_IGN);
	(void) ioctl(fileno(fi), TCGETA, &ttyb);
	flags = ttyb.c_lflag;
	ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);

	(void) ioctl(fileno(fi), TCSETAF, &ttyb);
	p = pbuf;
	rval = pbuf;
	while ((c = getc(fi)) != '\n') {
		if (c == EOF) {
			if (p == pbuf)		/* ^D, No password */
				rval = (char *)0;
			break;
		}
		if (p < &pbuf[PASS_MAX])
			*p++ = (char) c;
	}
	*p = '\0';			/* terminate password string */
	(void) fprintf(stderr, "\n");		/* echo a newline */

	ttyb.c_lflag = (long) flags;

	(void) ioctl(fileno(fi), TCSETAW, &ttyb);
	(void) signal(SIGINT, sig);

	if (fi != stdin)
		(void) fclose(fi);

	return rval;
}

static char *
findttyname(fd)
	int	fd;
{
	static	char	*l_ttyn;

	l_ttyn = ttyname(fd);

/* do not use syscon or systty if console is present, assuming they are links */
	if (((strcmp(l_ttyn, "/dev/syscon") == 0) ||
		(strcmp(l_ttyn, "/dev/systty") == 0)) &&
		(access("/dev/console", F_OK)))
			l_ttyn = "/dev/console";


	return l_ttyn;
}

