/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.7 $"

#include "sys/types.h"
#include "unistd.h"
#include "stdio.h"
#include "limits.h"
#include "stdlib.h"
#include "ulocks.h"
#include "signal.h"
#include "sys/prctl.h"
#include "sys/times.h"
#include "sync.h"

spinlock_t pl;
spinlock_trace_t pl_debug;
ulock_t l;

clock_t start, ti;
struct tms tm;
int nloops = 300000;
int debugmode = 0;

void arena_test(void);
void posix_test(void);

int
main(int argc, char **argv)
{
	extern int optind;
	extern char *optarg;
	int c;

	while((c = getopt(argc, argv, "n:d")) != EOF)
	switch (c) {
	case 'n':	/* # loops */
		nloops = atoi(optarg);
		break;
	case 'd':
		debugmode = 1;
		break;
	default:
		fprintf(stderr, "lockspeed:ERROR:bad arg\n");
		exit(-1);
	}

	posix_test();
	arena_test();

	return 0;
}

void
posix_test(void)
{
#if 0
	int i;

	spin_init(&pl);

	if (debugmode) {
		spin_mode(&pl, SPIN_MODE_TRACEINIT, &pl_debug);
		spin_mode(&pl, SPIN_MODE_TRACEON);
	}

	spin_lock(&pl);
	spin_unlock(&pl);

	start = times(&tm);
	for (i = 0; i < nloops; i++) {
		spin_lock(&pl);
		spin_unlock(&pl);
	}
	ti = times(&tm) - start;
	printf("lockspeed: Posix: start:%x time:%x\n", start, times(&tm));
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("lockspeed: Posix: time for %d lock/unlock %d mS or %d nS per lock/unlock\n",
		nloops, ti, (ti*1000000)/nloops);
#endif
}

void
arena_test(void)
{
	int i;
	usptr_t *hdr;

	if (debugmode)
		usconfig(CONF_LOCKTYPE, US_DEBUG);

	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	hdr = usinit("/usr/tmp/lockspeed.arena");
	l = usnewlock(hdr);

	prctl(PR_SETEXITSIG, SIGKILL);

	ussetlock(l);
	usunsetlock(l);

	start = times(&tm);
	for (i = 0; i < nloops; i++) {
		ussetlock(l);
		usunsetlock(l);
	}
	ti = times(&tm) - start;
	printf("lockspeed: Arena: start:%x time:%x\n", start, times(&tm));
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("lockspeed: Arena: time for %d lock/unlock %d mS or %d nS per lock/unlock\n",
		nloops, ti, (ti*1000000)/nloops);
}
