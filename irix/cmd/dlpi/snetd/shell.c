

/******************************************************************
 *
 *  SpiderX25 - Configuration utility
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  SHELL.C
 *
 *  X25 Control Interface: call configuration program
 *
 ******************************************************************/

/*
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.shell.c
 *	@(#)shell.c	1.6
 *
 *	Last delta created	17:06:16 2/4/91
 *	This file extracted	14:52:29 11/13/92
 *
 */

#include <stdio.h>

#define SHELLFUN "SHELL"

#ifndef SHELLDIR
#define SHELLDIR "/usr/etc"
#endif

extern int	trace, nobuild;

static char command[300];

int
cf_shell(argc, argv)
int	argc;
char	**argv;
{
	if (argc != 2)
	{
		error("usage: %s=\"command\"", SHELLFUN);  
		return 1;
	}

	if (trace)
		printf("%s=\"%s\"\n", SHELLFUN, argv[1]);

	if (nobuild) return(0);

	/* @INTERFACE@  set path and call shell with given string */

	strcpy(command, "export PATH;PATH=");
	strcat(command, SHELLDIR);
	strcat(command, ":$PATH;");
	strcat(command, argv[1]);
	if (system(command) != 0)
	    exit(error("[%s] \"%s\" failed", SHELLFUN, argv[1]));

	return 0;
}
