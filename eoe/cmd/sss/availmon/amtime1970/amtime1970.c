/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

#ident "$Revision: 1.2 $"

/*---------------------------------------------------------------------------*/
/* amtime1970.c                                                              */
/* A simple time utility that converts char time to seconds since Jan 1 1970 */
/* and vice versa.  Without any options, it prints out the secs since Jan 1  */
/* 1970.                                                                     */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>
#include <time.h>


main(int argc, char **argv)
{
    struct tms	t; /* not used */
    time_t	uptime;
    time_t	now;
    time_t	atboot;
    time_t	crtime;
    struct tm	tm;
    struct tm   *xtm = NULL;
    char	*Date = "Day Mon 00 00:00:00 0000\n";
    char	*Day  = "SunMonTueWedThuFriSat";
    char	*Month = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char	*p, c;
    int		i;
    extern int	daylight;

    if (argc == 1)
	printf("%d\n", time(0));
    else if ((argc == 2) && (strcmp(argv[1], "-i") == 0)) {
	uptime = times(&t) / HZ; /* time in seconds */
	now = time(NULL);
	atboot = now - uptime;
	printf("START|%d|%s", atboot, ctime(&atboot));
    } else if ((argc == 3) && (strcmp(argv[1], "-x") == 0)) {
	crtime = atol(argv[2]);
	xtm = localtime(&crtime);
        if ( xtm ) 
	    printf("%d/%d/%d\n",xtm->tm_mon+1,xtm->tm_mday,1900+xtm->tm_year);
    } else if ((argc == 3) && (strcmp(argv[1], "-d") == 0)) {
	crtime = atol(argv[2]);
	printf("%s", ctime(&crtime));
    } else if ((argc == 7) && (strcmp(argv[1], "-t") == 0)) {
	if (p = strstr(Month, argv[3]))
	    tm.tm_mon = (p - Month) / 3;
	else
	    fprintf(stderr, "Incorrect date string\n");

	tm.tm_mday = atoi(argv[4]);
	tm.tm_hour = (argv[5][0] - '0') * 10 + argv[5][1] - '0';
	tm.tm_min = (argv[5][3] - '0') * 10 + argv[5][4] - '0';
	tm.tm_sec = (argv[5][6] - '0') * 10 + argv[5][7] - '0';
	tm.tm_year = atoi(argv[6]) - 1900;
	tm.tm_isdst = -1;
	crtime = mktime(&tm);
	printf("%d\n", crtime);
    } else
	fprintf(stderr, "Error: incorrect arguments.\n");
}
