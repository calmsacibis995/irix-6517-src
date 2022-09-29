/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.1 $"

/*
**	runon -- assign a process to a named processor
*/

#include	<stdio.h>
#include	<ctype.h>
#include	"sys/types.h"
#include	"sys/sysmp.h"

main(argc, argv)
int argc;
char *argv[];
{
	extern	errno;
	extern	char *sys_errlist[];
	int	processor;
	register char	*p = argv[1];

	if (argc < 3) {
		fprintf(stderr,
			"usage: %s processor command [args...]\n",
			argv[0]);
		exit(2);
	}

	while(*p) {
		if(!isdigit(*p)) {
			fprintf(stderr,
				"%s: processor argument must be numeric.\n",
				argv[0]);
			exit(2);
		}
		p++;
	}
	processor = atoi(&argv[1][0]);
	if (sysmp(MP_MUSTRUN, processor) < 0) {
		fprintf(stderr,
			"%s: could not attach to processor %d -- %s\n ",
			argv[0], processor, sys_errlist[errno]);
		exit(2);
	}

	execvp(argv[2], &argv[2]);
	fprintf(stderr, "%s: %s\n", sys_errlist[errno], argv[2]);
	exit(2);
}
