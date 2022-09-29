/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.4 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h> 
#include <ulocks.h>
#include <errno.h>
#include <pwd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "stress.h"

char *Cmd;

int
main(int argc, char **argv)
{
	int nloops;
	int nusers;
	int i, c;
	char *afile;
	usptr_t *handle;

	Cmd = errinit(argv[0]);
	nloops = 10;
	nusers = 4;
	while ((c = getopt(argc, argv, "n:u:")) != EOF) {
		switch(c) {
		case 'n':
			nloops = atoi(optarg);
			break;
		case 'u':
			nusers = atoi(optarg);
			break;
		}
	}

	usconfig(CONF_INITUSERS, nusers);
	afile = tempnam(NULL, "usdetach");
	for (i = 0; i < nloops; i++) {
		/* create arena */
		if ((handle = usinit(afile)) == NULL)
			errprintf(1, "usinit of %s failed", afile);
			/* NOTREACHED */

		(void)usnewsema(handle, 0);

		usdetach(handle);
	}
	unlink(afile);
	return 0;
}
