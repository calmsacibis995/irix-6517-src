/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)chown.c	5.11 (Berkeley) 6/18/88";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <locale.h>
#include <unistd.h>
#include <fmtmsg.h>
#include <stdlib.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

/* locals */

char	cmd_label[] = "UX:?????";
char	cmd_chown[] = "chown";
char	cmd_chgrp[] = "chgrp";

static uid_t uid;
static gid_t gid;
static gid_t mygid;
static int fflag, hflag, rflag;
static int retval;
static char *gname, *myname;

/*
 * print the usage message
 */
static void
usage(int cmdflg)
{
	(void)_sgi_nl_usage(SGINL_USAGE, cmd_label,
	    cmdflg != 2?
		gettxt(_SGI_DMMX_usage_chown,
		    "chown [-Rfh] owner[{.|:}group] file ...")
	    :
		gettxt(_SGI_DMMX_usage_chgrp,
		    "chgrp [-Rfh] group  file ..."));
	exit(-1);
}

/*
 * set group
 */
static void
s_setgid(char *s)
{
	struct group *gr;

	if( !*s) {
	    gid = -1;			/* argument was "uid." */
	    return;
	}
	for(gname = s; *s && isdigit(*s); ++s);
	if( !*s)
	    gid = atoi(gname);
	else {
	    if( !(gr = getgrnam(gname))) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_unkngroupid, "unknown group id: %s"),
		    gname);
		exit(1);
	    }
	    gid = gr->gr_gid;
	}
}

static void
s_setuid(char *s)
{
	struct passwd *pwd;
	char *beg;

	if( !*s) {
	    uid = -1;			/* argument was ".gid" */
	    return;
	}
	for(beg = s; *s && isdigit(*s); s++);
	if( !*s)
	    uid = atoi(beg);
	else {
	    if( !(pwd = getpwnam(beg))) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_unknuserid, "unknown user id: %s"),
		    beg);
		exit(1);
	    }
	    uid = pwd->pw_uid;
	}
}

/*
 * error handling
 */
static void
err(char *s)
{
	if(fflag)
	    return;
	_sgi_nl_error(SGINL_SYSERR2, cmd_label, "%s", s);
	retval = 2;
}

static void
chownerr(char *file)
{
	static uid_t euid = -1;
	static int ngroups = -1;

	if (fflag)
		return;
	if (errno != EPERM) {
		err(file);
		return;
	}

	/*
	 * check for chown without being root
	 */
	if(uid != -1 && euid == -1 && (euid = geteuid())) {
	    err(file);
	    return;
	}

	if(gid != -1 && ngroups == -1 && gid != mygid) {
	    gid_t groups[NGROUPS_UMAX];

	    ngroups = getgroups(NGROUPS_UMAX, groups);
	    while (--ngroups >= 0 && gid != groups[ngroups]);
	    if(ngroups < 0 && mygid != gid) {
	        _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			gettxt(_SGI_DMMX_notgrpmbr,
			"you are not a member of group %s"),
			gname);
		return;
	    }
	}
        err(file);
}

/*
 * set owner/group
 */
static void
change(char *file)
{
	register DIR *dirp;
	register dirent64_t *dp;
	struct stat64 buf;
	char savedir[PATH_MAX];

	if (hflag ? lstat64(file, &buf) : stat64(file, &buf)) {
	    err(file);
	    return;
	}

	if (rflag && S_ISDIR(buf.st_mode)) {
	    if (getcwd(savedir, PATH_MAX) == (char *)0) {
		err(file);
		return;
	    }
	    if(chdir(file) < 0 || !(dirp = opendir("."))) {
		err(file);
		return;
	    }
	    for(dp = readdir64(dirp); dp; dp = readdir64(dirp)) {
		if (dp->d_name[0] == '.' && (!dp->d_name[1] ||
		    dp->d_name[1] == '.' && !dp->d_name[2]))
			continue;
		change(dp->d_name);
	    }
	    closedir(dirp);

	/* 
	    Now go back to where we started and chown that
	    as well.  This should work for ".", "..", and
	    anything else.
	*/
	    if(chdir(savedir) < 0) {
		err(savedir);
		exit(fflag? 0 : 1);
	    }
	    if (chown(file, uid, gid))
		chownerr(file);
	} else {
	    if(hflag) {
		if(lchown(file, uid, gid))
			chownerr(file);
	    } else {
	    	if(chown(file, uid, gid)) 
	 		chownerr(file);
	    }
	    return;
	}
}

/*
 * main entry
 */
int
main(int argc, char **argv)
{
	register char *cp;
	char *cmd;
	int cmdflg = 0;
	int ch;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);		/* will be changed later */

	/*
	 * Determine command invoked (chown, chgrp)
	 */
	if(cmd = strrchr(argv[0], '/'))
	    cmd++;
	else
	    cmd = argv[0];
	if( !strcmp(cmd, cmd_chown))
	    cmdflg |= 1;
	if( !strcmp(cmd, cmd_chgrp))
	    cmdflg |= 2;
	if( !cmdflg) {
	    _sgi_ffmtmsg(stderr, 0, cmd_label, MM_ERROR, "%s",
		gettxt(_SGI_DMMX_whoami,
		    "I don't know who I am."));
	    _sgi_ffmtmsg(stderr, 0, cmd_label, MM_FIX, "%s",
		gettxt(_SGI_DMMX_chownchgrp,
		    "Cmd name must be 'chown' or 'chgrp'"));
	    return(1);
	}
	(void)strcpy(cmd_label + 3, cmd);
	(void)setlabel(cmd_label);

	rflag = fflag = hflag = retval = 0;
	while((ch = getopt(argc, argv, "Rfh")) != EOF)
	    switch((char)ch) {
		case 'R':
		    rflag++;
		    break;
		case 'f':
		    fflag++;
		    break;
		case 'h':
		    hflag++;
		    break;
		case '?':
		default:
		    (void)usage(cmdflg);
	    }
	argv += optind;
	argc -= optind;

	if(argc < 2)
	    (void)usage(cmdflg);

	if(cmdflg == 1) {
	    if(cp = strchr(*argv, '.')) {
		*cp++ = '\0';
		(void)s_setgid(cp);
	    } else if(cp = strchr(*argv, ':')) {
		*cp++ = '\0';
		(void)s_setgid(cp);
	    } else
		gid = -1;
	    (void)s_setuid(*argv);
	} else {
	    /* chgrp */
	    uid = -1;
	    (void)s_setgid(*argv);
	}

	mygid = getgid();

	while(*++argv)
	    change(*argv);
	exit(retval);
}
