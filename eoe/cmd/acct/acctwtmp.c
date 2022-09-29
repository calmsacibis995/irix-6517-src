/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)acct:common/cmd/acct/acctwtmp.c	1.9.3.4"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/acctwtmp.c,v 1.3 1993/11/05 04:23:45 jwag Exp $"
/*
 *	acctwtmp reason >> /var/adm/wtmp
 *	writes utmp.h record (with current time) to end of std. output
 *	acctwtmp `uname` >> /var/adm/wtmp as part of startup
 *	acctwtmp pm >> /var/adm/wtmp  (taken down for pm, for example)
 */
#include <stdio.h>
#include <sys/types.h>
#include "acctdef.h"
#include <utmp.h>

struct	utmp	wb;
char	*strncpy();

main(argc, argv)
char **argv;
{
	if(argc < 2) 
		fprintf(stderr, "Usage: %s reason [ >> %s ]\n",
			argv[0], WTMP_FILE), exit(1);

	strncpy(wb.ut_line, argv[1], LSZ);
	wb.ut_line[11] = NULL;
	wb.ut_type = ACCOUNTING;
	time(&wb.ut_time);
	fseek(stdout, 0L, 2);
	fwrite(&wb, sizeof(wb), 1, stdout);
}
