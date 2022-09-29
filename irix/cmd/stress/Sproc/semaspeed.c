/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.3 $"

#include "stdio.h"
#include "limits.h"
#include "stdlib.h"
#include "unistd.h"
#include "ulocks.h"
#include "task.h"
#include "getopt.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "sys/times.h"

usema_t *s;

int
main(int argc, char **argv)
{
	struct tms tm;
	clock_t start, ti;
	int nloops = 300000;
	int c, i;
	usptr_t *hdr;

	while((c = getopt(argc, argv, "n:")) != EOF)
	switch (c) {
	case 'n':	/* # loops */
		nloops = atoi(optarg);
		break;
	default:
		fprintf(stderr, "semaspeed:ERROR:bad arg\n");
		exit(-1);
	}

	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	hdr = usinit("/usr/tmp/semaspeed.arena");
	s = usnewsema(hdr, 1);

	start = times(&tm);
	for (i = 0; i < nloops; i++) {
		uspsema(s);
		usvsema(s);
	}
	ti = times(&tm) - start;
	printf("semaspeed:start:%x time:%x\n", start, times(&tm));
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("semaspeed:time for %d psema/vsema %d mS or %d uS per psema/svsema\n",
		nloops, ti, (ti*1000)/nloops);
	return 0;
}
