/**************************************************************************
 *									  *
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * This module implements commonly used interfaces such as startup,
 * debugging and diagnostics.
 */

/* Use raw libc interfaces. */
#define write	__write

#include "common.h"
#include "asm.h"
#include "pt.h"
#include "ptdbg.h"
#include "sys.h"
#include "vp.h"

#include <mutex.h>
#include <rld_interface.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


void init1(void);


#ifdef STATS
int	stat_counts[STAT_LAST];
char	*stat_names[STAT_LAST] = {
	"BLOCK_NEW",
	"BLOCK_READY",
	"BLOCK_PREEMPTING",
	"IDLE_PAUSE",
	"IDLE",
	"RESUME_NEW",
	"RESUME_SAME",
	"RESUME_SWAP",
	"MTX_SLOW",
	"SPARE0",
	"SPARE1",
	"SPARE2",
	"SPARE3",
	"SPARE4",
};
static void stats_exit(void);
#endif

#ifdef DEBUG
static void dbg_bootstrap(void);
#endif

/*
 * init1(void)
 *
 * Library initialisation code invoked by C runtime.
 */
void
init1(void)
{
	/* REFERENCED */
	auto volatile int	local;
	static unsigned int	inited = 0;

	if (test_and_set32(&inited, 1)) {
		return;
	}

	/* The initial stack is allocated to the initial vp on demand.
	 * Since the initial vp may not be the one accessing it we
	 * extend it here by touching a page.
	 */
	*((char *)&local - PT_STACK_DEFAULT) = 0;

#ifdef DEBUG
	dbg_bootstrap();
#endif

	pt_bootstrap();

	/* Tell rld that we are multithreaded to avoid it being surprised
	 * by vp creation.
	 */
	_rld_new_interface(_RLD_PTHREADS_START, &jt_rld_pt);

#ifdef STATS
	atexit(stats_exit);
#endif
}


#ifdef	STATS
static void
stats_exit(void)
{
	int	i;

	printf("STATS\n");
	for (i = 0; i < STAT_LAST; i++) {
		printf("\t%-20s: %d\n", stat_names[i], stat_counts[i]);
	}
}
#endif


#ifdef DEBUG
#include <stdarg.h>
#include <fcntl.h>

long		dbg_trace;

static void
dbg_bootstrap(void)
{
	char		*flags;

	/* Trace flags from the environment.
	 */
	if (flags = getenv("PT_TRACE")) {
		dbg_trace = strtol(flags, 0, 0);
	}
}


void
dbg_printf(char *fmt, ...)
{
	char	buf[256];
	int 	cnt;
	va_list	va;

	va_start(va, fmt);
	cnt = vsprintf(buf, fmt, va);
	va_end(va);
	if (buf[cnt - 1] != '\n') {
		cnt += sprintf(buf + cnt,
			" {%#d, 0x%p, 0x%p}\n", VP_PID, VP, VP ? VP->vp_pt : 0);
	}
	write(2, buf, cnt);
}

#endif /* DEBUG */

void
panic(char *func, char *msg)
{
	char	buf[256];
	int 	cnt;

	cnt = sprintf(buf, "PANIC %s() %s - {0x%p, 0x%p}\n", func, msg,
		      VP, VP ? VP->vp_pt : 0);
	write(2, buf, cnt);
	sig_abort();
}
