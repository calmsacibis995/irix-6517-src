/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)acct:common/cmd/acct/utmp2wtmp.c	1.2.5.3"
#ident "$Id: utmp2wtmp.c,v 1.2 1995/03/02 18:35:23 jwag Exp $"
/*
 *	create entries for users who are still logged on when accounting
 *	is being run. Look at utmp, and update the time stamp. New info
 *	goes to wtmp. Call by runacct. 
 * SGI note - there is a race condition between nulling out the wtmp file
 * and re-filling it here - the getut library can resync wtmp from wtmpx
 * if it sees a zero length wtmp file!
 * We add an option that opens for truncate to avoid this.
 * SGI note - use utmpx/wtmpx so we don't lose so much info every day
 *	(like host name, etc).
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <utmpx.h>
#include <time.h>

int
main(int argc, char **argv)
{
	struct utmpx *utmpx;
	FILE *fp;
	char *om = "a+";

	if (argc > 1 && (strcmp(argv[1], "-t") == 0))
		om = "w";
	if ((fp = fopen(WTMPX_FILE, om)) == NULL) {
		fprintf(stderr, "%s: cannot open %s for writing\n",
			argv[0], WTMPX_FILE);
		exit(2);
	}
	while ((utmpx = getutxent()) != NULL) {
		if (utmpx->ut_type == USER_PROCESS) {
			gettimeofday(&utmpx->ut_tv);
			fwrite(utmpx, sizeof(*utmpx), 1, fp);
		}
	}
	fclose(fp);
	/* force syncing to close races and keep info as correct as possible
	 * the best way to do this is to simply truncate the wtmp file
	 * and call syncutmp
	 */
	truncate(WTMP_FILE, 0);
	synchutmp(WTMP_FILE, WTMPX_FILE);
	return 0;
}
