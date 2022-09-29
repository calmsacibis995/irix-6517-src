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

#ident "newsess/newsess.c: $Revision: 1.2 $"


/*
 * newsess: run a shell in a new array session
 *
 *	usage: newsess [-l] [-h ASH] [-p project] [[-s] cmd]
 */

#include <sys/types.h>
#include <dlfcn.h>
#include <paths.h>
#include <proj.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syssgi.h>
#include <unistd.h>


main(argc, argv)
	int argc;
	char *argv[];
{
	ash_t	ash = -1LL;
	char	*project = NULL;
	char	*shell, *simpleshell;
	int	askas = 0;
	int	cmdargs = -1;
	int	i;
	int	local = 0;
	prid_t	newprojid;
	struct passwd   *pwent;
	struct prident	*pridentry;
	struct projent  *projentry;
	uid_t	uid;

	/* Parse the command line options */
	for (i = 1;  i < argc;  ++i) {
		if (strcmp(argv[i], "-g") == 0) {
			askas = 1;
		}
		else if (strcmp(argv[i], "-h") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: ash required with -h\n",
					argv[0]);
				exit(1);
			}
			else {
				char  *Ptr;

				ash = strtoll(argv[i], &Ptr, 0);
				if (Ptr == argv[i]  ||  *Ptr != '\0'  ||
				    ash < 0LL)
				{
					fprintf(stderr,
						"%s: Invalid array session "
						     "handle \"%s\"",
						 argv[0], argv[i]);
					exit(1);
				}
			}
		}
		else if (strcmp(argv[i], "-l") == 0) {
			local = 1;
		}
		else if (strcmp(argv[i], "-p") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"%s: project name required with -p\n",
					argv[0]);
				exit(1);
			}
			else
			    project = argv[i];
		}
		else if (strcmp(argv[i], "-s") == 0) {
			cmdargs = i + 1;
			break;
		}
		else {
			cmdargs = i;
			break;
		}
	}

	/* If the user specified an ASH, make sure they are root and did */
	/* not specify -l.						 */
	if (ash >= 0LL) {
		if (getuid() != 0) {
			fprintf(stderr,
				"%s: Only root may specify the -h option\n",
				argv[0]);
			exit(1);
		}
		if (local || askas) {
			fprintf(stderr,
				"%s: -h is incompatible with -g and -l\n",
				argv[0]);
			exit(1);
		}
	}

	/* Similarly, -l & -g are incompatible */
	if (local && askas) {
		fprintf(stderr, "%s: -g and -l are incompatible\n", argv[0]);
		exit(1);
	}

	/* If a global ASH is desired and the kernel itself does not	 */
	/* generate them, then force the request to go to array services */
	if (!local && !askas && syssgi(SGI_GETASMACHID) <= 0) {
		askas = 1;
	}

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
	if (project != NULL) {
		if (uid == 0)
		    newprojid = projid(project);
		else
		    newprojid = validateproj(pwent->pw_name, project);
		
		if (newprojid < 0) {
			perror("Cannot change project");
			exit(1);
		}
	}
	else {
		newprojid = -1LL;
	}

	/* Request a global ASH from array services if necessary. Note	*/
	/* that we must jump through a couple of hoops to avoid having  */
	/* a dependency on array services at build or install time.	*/
	if (askas) {
		void  *libarray;
		ash_t (*allocash)(void *, void *);

		libarray = dlopen("libarray.so", RTLD_LAZY);
		if (libarray == NULL) {
			fprintf(stderr,
				"Unable to generate global array session "
				    "handle: array services not installed\n"); 
			exit(1);
		}

		allocash =
		    (ash_t (*)(void *, void *)) dlsym(libarray, "asallocash");
		if (allocash == NULL) {
			fprintf(stderr, "Unable to find asallocash: %s\n",
				dlerror());
			exit(1);
		}

		ash = (*allocash)(NULL, NULL);
		if (ash < 0LL) {
			perror("Unable to allocate global ASH");
			exit(1);
		}

		dlclose(libarray);
	}

	/* Start a new session and set it is ASH & Project ID */
	if (newarraysess() < 0) {
		perror("Unable to start new array session");
		exit(1);
	}
	if (ash >= 0LL  &&  setash(ash) < 0) {
		perror("Unable to set new array session handle");
		exit(1);
	}
	if (newprojid >= 0LL  &&  setprid(newprojid) < 0) {
		perror("Unable to set new project ID");
		exit(1);
	}

	/* Revoke setuid-ness before invoking shell */
	if (setuid(uid) != 0) {
		perror("Unable to restore uid");
		exit(1);
	}
	
	/* Fire up the shell. If any additional command line */
	/* args were passed to us, forward them to the shell */
	if (cmdargs > 0) {
		argv[cmdargs-1] = simpleshell;
		execv(shell, &argv[cmdargs-1]);
	}
	else {
		execl(shell, simpleshell, 0);
	}

	/* If we make it to here, the exec failed. Die miserably. */
	perror("Unable to exec shell");
	exit(1);
}
