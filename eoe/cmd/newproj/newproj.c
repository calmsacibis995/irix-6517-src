/*
 * COPYRIGHT NOTICE
 * Copyright 1995, Silicon Graphics, Inc. All Rights Reserved.
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 *
 */

#ident "newproj/newproj.c: $Revision: 1.4 $"


/*
 * newproj: run a shell under a different project ID
 */

#include <sys/types.h>
#include <paths.h>
#include <proj.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/capability.h>


int
main(int argc, char *argv[])
{
	char	*project;
	char	*shell, *simpleshell;
	prid_t	newprojid;
	struct passwd   *pwent;
	uid_t	uid;
	cap_t	ocap, pcap = NULL;
	cap_value_t cap_setuid = CAP_SETUID;

	/* Set up the project name to switch to */
	if (argc < 2) {
		fprintf(stderr, "usage: %s project [args]\n", argv[0]);
		exit(1);
	}
	project = argv[1];

	if (sysconf(_SC_CAP) > 0)
		pcap = cap_get_proc();

	/* Extract our passwd entry */
	uid = getuid();
	if ((pwent = getpwuid(uid)) == NULL) {
		fprintf(stderr, "Unable to find passwd entry\n");
		exit(1);
	}
	endpwent();

	/* Set up the shell we intend to invoke */
	if (pwent->pw_shell)
	    shell = pwent->pw_shell;
	else {
		shell = getenv("SHELL");
		if (shell == NULL)
		    shell = _PATH_BSHELL;
	}

	simpleshell = strrchr(shell, '/');
	if (simpleshell)
	    ++simpleshell;
	else
	    simpleshell = shell;

	/* Convert the project name to a numeric project ID. If the user */
	/* is anyone other than root, make sure they are authorized.     */
	if (uid == 0)
	    newprojid = projid(project);
	else
	    newprojid = validateproj(pwent->pw_name, project);

	if (newprojid < 0) {
		perror("Cannot change project");
		exit(1);
	}

	/* Start a new session and set it to use the new project ID */
	ocap = cap_acquire(1, &cap_setuid);
	if (newarraysess() < 0) {
		cap_surrender(ocap);
		perror("Unable to start new array session");
		exit(1);
	}
	if (setprid(newprojid) < 0) {
		cap_surrender(ocap);
		perror("Unable to set new project ID");
		exit(1);
	}
	cap_surrender(ocap);

	/* Revoke setuid-ness before invoking shell */
	if (setuid(uid) != 0) {
		perror("Unable to restore uid");
		exit(1);
	}

	/* Revoke CAP_SETUID if the invoker didn't have it */
	if (pcap != NULL)
	{
		cap_flag_value_t cap_flag;

		if (cap_get_flag (pcap, cap_setuid, CAP_INHERITABLE,
				  &cap_flag) != -1 && cap_flag == CAP_CLEAR)
		{
			(void) cap_set_flag (pcap, CAP_PERMITTED, 1,
					     &cap_setuid, CAP_CLEAR);
			(void) cap_set_flag (pcap, CAP_EFFECTIVE, 1,
					     &cap_setuid, CAP_CLEAR);
		}
		(void) cap_set_proc (pcap);
		cap_free (pcap);
	}
	
	/* Fire up the shell. If any additional command line */
	/* args were passed to us, forward them to the shell */
	if (argc > 2) {
		argv[1] = simpleshell;
		execv(shell, &argv[1]);
	}
	else {
		execl(shell, simpleshell, 0);
	}

	/* If we make it to here, the exec failed. Die miserably. */
	perror("Unable to exec shell");
	return(1);
}
