/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)acct:common/cmd/acct/accton.c	1.11.3.4"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/accton.c,v 1.6 1996/06/14 19:52:13 rdb Exp $"

/*
 *	accton - calls syscall with super-user privileges
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include        "acctdef.h"
#include	<errno.h>
#include	<sys/stat.h>
#include        <pwd.h>
#include        <grp.h>

uid_t     admuid;
gid_t	  admgid;
struct  passwd *pwd; 
struct  group  *grp;

void ckfile(char *);

main(int argc, char **argv)
{
	register uid_t	uid;

	uid = getuid();
	if ((pwd = getpwnam("adm")) == NULL) 
		perror("cannot determine adm's uid"), exit(1);
	admuid = pwd->pw_uid;
	if ((grp = getgrnam("adm")) == NULL) 
		perror("cannot determine adm's gid"), exit(1);
	admgid = grp->gr_gid;
	if(uid == ROOT || uid == admuid) {
		if(setuid(ROOT) == ERR) 
			perror("cannot setuid (check command mode and owner)"), exit(1);
		if (argv[1])
			ckfile(argv[1]);
		if (acct(argc > 1 ? argv[1] : 0) < 0)
			perror(argv[1]), exit(1);
		exit(0);

	}
	fprintf(stderr,"%s: permission denied\n", argv[0]);
	exit(1);
}

void
ckfile(char *admfile)
{
	struct stat		stbuf;
	register struct stat	*s = &stbuf;

	if(stat(admfile, s) == ERR)
		if(creat(admfile, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) == ERR) 
			perror("creat"), exit(1);

	if(s->st_uid != admuid || s->st_gid != admgid)
		if(chown(admfile, admuid, admgid) == ERR) 
			perror("cannot change owner"), exit(1);

	/* was if(s->st_mode & 0777 != 0664) */
	if((s->st_mode & S_IAMB) != S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
		if(chmod(admfile, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) == ERR) 
			perror("cannot chmod"), exit(1);
}
