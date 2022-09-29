/*
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
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
 */

/*
 * mklbldb()
 * a system adminstrator command to initialize the mls database file,
 * perform the uniqueness check and load information into binary file format.
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

char *newroot = NULL;

main(int argc, char **argv)
{
	int c;	/* command line option letter */
	int mkbinflag;
	extern char *optarg;

	/*
	 * process command line options.
	 */

	mkbinflag = 1;
	while ( (c = getopt(argc, argv, "R:n") ) != -1)
		switch (c) {

		case 'R':		/* new "root" directory */
			newroot = optarg;
			break;
		case 'n':		/* don't build binary file */
			mkbinflag = 0;
			break;
		case '?':
			fprintf(stderr,"usage: %s [-R root] [-n]\n", argv[0]);
			exit(1);
		default:
			break;
		}

	mls_init();
	mls_unique();

	if (mkbinflag == 1)
		mls_mkdb();

	exit(0);
}

