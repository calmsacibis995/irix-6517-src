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

#ident	"$Revision: 1.6 $"

/*
**	fpmode -- run a process in precise|performance|smm|nsmm|spec|nonspec mode
*/

#include	<stdio.h>
#include	<ctype.h>
#include	"sys/types.h"
#include	"sys/syssgi.h"

main(argc, argv)
int argc;
char *argv[];
{
	extern	errno;
	extern	char *sys_errlist[];
	int	precise = -1, smm = -1, spec = -1;
#ifdef NOTDEF
	int	softfp = -1;
#endif

	if (argc < 3) {
usage:
		fprintf(stderr,
			"usage: %s precise|performance|smm|nsmm|spec|nonspec command [args...]\n",
			argv[0]);
		exit(2);
	}

	if (strcmp(argv[1], "precise") == 0)
		precise = 1;
	else if (strcmp(argv[1], "performance") == 0)
		precise = 0;
	else if (strcmp(argv[1], "smm") == 0)
		smm = 1;
	else if (strcmp(argv[1], "nsmm") == 0)
		smm = 0;
	else if (strcmp(argv[1], "spec") == 0)
		spec = 1;
	else if (strcmp(argv[1], "nonspec") == 0)
		spec = 0;
#ifdef NOTDEF
	else if (strcmp(argv[1], "oldsoftfp") == 0)
		softfp = 1;
	else if (strcmp(argv[1], "newsoftfp") == 0)
		softfp = 0;
#endif
	else
		goto usage;

	if ((precise != -1) && (syssgi(SGI_SET_FP_PRECISE, precise) < 0)) {
		fprintf(stderr,
			"%s: could not SGI_SET_FP_PRECISE %d -- %s\n ",
			argv[0], precise, sys_errlist[errno]);
		exit(2);
	}

	if ((smm != -1) && (syssgi(SGI_SET_CONFIG_SMM, smm) < 0)) {
		fprintf(stderr,
			"%s: could not SGI_SET_CONFIG_SMM %d -- %s\n ",
			argv[0], smm, sys_errlist[errno]);
		exit(2);
	}

	if ((spec != -1) && (syssgi(SGI_SPECULATIVE_EXEC, spec) < 0)) {
		fprintf(stderr,
			"%s: could not SGI_SPECULATIVE_EXEC %d -- %s\n ",
			argv[0], spec, sys_errlist[errno]);
		exit(2);
	}

#ifdef NOTDEF
	if ((softfp != -1) && (syssgi(SGI_OLD_SOFTFP, softfp) < 0)) {
		fprintf(stderr,
			"%s: could not SGI_OLD_SOFTFP %d -- %s\n ",
			argv[0], softfp, sys_errlist[errno]);
		exit(2);
	}
#endif

	if (syssgi(SGI_SET_FP_PRESERVE, 1) < 0) {
		fprintf(stderr,
			"%s: could not SGI_SET_FP_PRESERVE -- %s\n ",
			argv[0], sys_errlist[errno]);
		exit(2);
	}

	execvp(argv[2], &argv[2]);
	fprintf(stderr, "%s: %s\n", sys_errlist[errno], argv[2]);
	exit(2);
}
