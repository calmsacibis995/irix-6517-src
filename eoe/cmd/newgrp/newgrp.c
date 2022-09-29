/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)newgrp:newgrp.c	1.6"	*/
#ident	"$Revision: 1.18 $"
/*
 * newgrp [group]
 *
 * rules
 *	if no arg, group id in password file is used
 *	else if group id == id in password file
 *	else if login name is in member list
 *	else if password is present and user knows it
 *	else too bad
 */
#include <sys/param.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <paths.h>

#define	SHELL	_PATH_BSHELL

#define ELIM	128

char	PW[] = "Password:";
char	NG[] = "Sorry";
char	PD[] = "Permission denied";
char	UG[] = "Unknown group";
char	NS[] = "You have no shell";

/* 
 * This program may now be invoked either by "newgrp" (old SysV) or 
 * "multgrps" (to take advantage of BSD-style multiple groups).
 */
char	MULTGRPS[] = "multgrps";

char	*getpass();
char	*crypt();

char homedir[5+PATH_MAX] = "HOME=";
#define ENV_SIZE 64
#define MAXLINE 256
char    logname[8+ENV_SIZE] = "LOGNAME=";
char    env_user[6+ENV_SIZE] = "USER=";
char    env_term[6+ENV_SIZE] = "TERM=";
char    env_shell[7+PATH_MAX] = "SHELL=";
char    timez[3+MAXLINE] = "TZ=";
char    rem_user[11+ENV_SIZE] = "REMOTEUSER=";
char    rem_host[11+ENV_SIZE] = "REMOTEHOST=";
char    mail[30] = { "MAIL=/usr/mail/" };
char    path[5+PATH_MAX] = "PATH=";
char    *envinit[ELIM];
extern  char **environ;
char	*ttyn;
char	*cmd;

#ifdef DEBUG
#define DBG(x)	fprintf(stderr, x)
#else
#define DBG(x)
#endif

main(argc,argv)
char *argv[];
{
	register struct passwd *p;
	int eflag = 0;
	int mgrps = 0;	/* indicates if this prog invoked as "multgrps" */
	int uid;
	char *shell, *dir, *name;

#ifdef	DEBUG
	chroot(".");
#endif
	if ((cmd = strrchr(argv[0],'/')) == NULL)
		cmd = argv[0];	/* it's only the prog name */
	else
		cmd++;	/* path was skipped, now inc over the last '/' */

	if ((p = getpwuid(getuid())) == NULL) {
		DBG("1 ");
		error(NG);
	}
	endpwent();

	if(argc > 1 && *argv[1] == '-'){
		switch(argv[1][1]) {
			case 'l':
			case 0:
				eflag++;
			case '-':
				argv++;
				--argc;
				break;
			default:
				error("Bad option");
		}
	}

	if (strcmp(cmd, MULTGRPS) == 0) {
		mgrps++;
	} else {
		if (argc > 1)
			p->pw_gid = chkgrp(argv[1], p);

		if (setgid(p->pw_gid) < 0 || setuid(getuid()) < 0) {
			DBG("2b ");
			error(NG);
		}
	}

	uid = p->pw_uid;
	dir = strcpy(malloc(strlen(p->pw_dir)+1),p->pw_dir);
	name = strcpy(malloc(strlen(p->pw_name)+1),p->pw_name);
	if (!*p->pw_shell) {
		if ((shell = getenv("SHELL")) != NULL) {
			p->pw_shell = shell;
		} else {
			p->pw_shell = SHELL;
		}
	}
	if (eflag) {
		char *simple;
		register int envcnt;
		register char *tp;

		strcat(homedir, dir);
		strcat(logname, name);
		envinit[2] = logname;
		chdir(dir);
		envinit[0] = homedir;
		envinit[1] = path;
		strncat(path, uid == 0 ? _PATH_ROOTPATH : _PATH_USERPATH,
			PATH_MAX);
		envcnt = 3;
		envinit[envcnt++] = strncat(env_shell,p->pw_shell,PATH_MAX);
		envinit[envcnt++] = strncat(env_user,name,ENV_SIZE);
		if (NULL != (tp=getenv("TERM")))
			envinit[envcnt++] = strncat(env_term,tp,ENV_SIZE);
		if ( (tp = getenv("TZ")) != NULL ) {
			strncat(timez,tp,sizeof(timez)-3);
			envinit[envcnt++] = timez;	/* allow for TZ */
		}
                if ( (tp=getenv("REMOTEUSER")) != NULL)
                        envinit[envcnt++] = strncat(rem_user,tp,ENV_SIZE);
                if ( (tp=getenv("REMOTEHOST")) != NULL)
                        envinit[envcnt++] = strncat(rem_host,tp,ENV_SIZE);
		envinit[envcnt++] = mail;
		strncat(mail,p->pw_name, sizeof(mail) - 5);
		envinit[envcnt] = NULL;
		environ = envinit;

		shell = strcpy(malloc(strlen(p->pw_shell) + 2), "-");
		shell = strcat(shell,p->pw_shell);
		simple = strrchr(shell,'/');
		if (simple) {
			*(shell+1) = '\0';
			shell = strcat(shell,++simple);
		}
	} else
		shell = p->pw_shell;

	if (mgrps) {
		if (initgroups(name,(int)p->pw_gid) < 0)
			exit(1);
		if (setuid(getuid()) < 0) {
			DBG("3 ");
			error(NG);
		}
	}

	execl(p->pw_shell,shell, NULL);
	error(NS);
	/*NOTREACHED*/
}

warn(s)
char *s;
{
	fprintf(stderr, "%s: %s\n", cmd, s);
}

error(s)
char *s;
{
	warn(s);
	exit(1);
}

chkgrp(gname, p)
char	*gname;
struct	passwd *p;
{
	register char **t;
	register struct group *g;

	g = getgrnam(gname);
	endgrent();
	if (g == NULL) {
		/* Check for a group id number */
		if(atoi(gname) > 0 &&  (g = getgrgid((gid_t)atoi(gname))) != NULL)
			endgrent();
		else {
			warn(UG);
			return getgid();
		}
	}
	if (p->pw_gid == g->gr_gid || getuid() == 0)
		return g->gr_gid;
	for (t = g->gr_mem; *t; ++t) {
		if (strcmp(p->pw_name, *t) == 0)
			return g->gr_gid;
	}
	if (*g->gr_passwd) {
		if (!isatty(fileno(stdin)))
			error(PD);
		if (strcmp(g->gr_passwd, crypt(getpass(PW), g->gr_passwd)) == 0)
			return g->gr_gid;
	}
	DBG("4 ");
	warn(NG);
	return getgid();
}
