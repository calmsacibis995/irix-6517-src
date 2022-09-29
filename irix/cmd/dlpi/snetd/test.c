

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
#define SHELLDIR "/usr/lib/snet"
#endif

int	trace, nobuild;

static char command[300];

main(argc, argv)
int	argc;
char	**argv;
{
	if (argc != 2)
	{
		printf("usage: %s=\"command\"", SHELLFUN);  
		return 1;
	}
#ifdef DEBUG 
	trace = 1;

#else
	trace = 0;
#endif
	nobuild = 0;
	if (trace)
		printf("%s=\"%s\"\n", SHELLFUN, argv[1]);


	/* @INTERFACE@  set path and call shell with given string */

	strcpy(command, "export PATH;PATH=");
	strcat(command, SHELLDIR);
	strcat(command, ":$PATH;");
	strcat(command, argv[1]);
	if (system(command) != 0) {
	    printf("[%s] \"%s\" failed", SHELLFUN, argv[1]);
	   exit(1);
	   }

	return 0;
}
