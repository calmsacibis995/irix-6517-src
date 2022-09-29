/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.3 $"

/*
 *	oncell -- assign a process to a named cell
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/syssgi.h>

extern void Usage(void);
char *Cmd;

int
main(int argc, char **argv)
{
	int c, i, foptind;
	int mycell = 0;
	cell_t cell;
	char *p = argv[1];

	Cmd = argv[0];

	while ((c = getopt(argc, argv, "")) != EOF)
		switch (c) {
		case '?':
			Usage();
			exit(1);
		default:
			Usage();
			exit(1);
		}

	/*
	 * Determine if the OS knows anything about cells.
	 */
        if (syssgi(SGI_CELL, SGI_CELL_PID_TO_CELLID, 0, &cell) == -1) {
	        fprintf(stderr, 
			"Operating system version does not support cells\n");
		exit(1);
	}

	if ((argc - optind) == 0) {
	        /* just print local cell number */
		printf("%d\n", cell);
		exit(0);
	} else if ((argc - optind) < 2) {
		Usage();
		exit(1);
	}

	p = argv[optind];
	while (*p) {
		if (!isdigit(*p)) {
			fprintf(stderr,
				"%s: cell argument must be numeric.\n",
				Cmd);
			exit(1);
		}
		p++;
	}
	cell = atoi(argv[optind]);
	optind++;

	rexecvp(cell, argv[optind], &argv[optind]);
	fprintf(stderr, "%s: %s\n", Cmd, strerror(errno));
	exit(1);
}

void
Usage(void)
{
	fprintf(stderr, "Usage: %s [cell command [args...]\n", Cmd);
}
