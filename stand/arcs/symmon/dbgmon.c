#ident "symmon/dbgmon.c: $Revision: 1.26 $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * dbgmon.c -- main loop for dbgmon
 */

#include <sys/types.h>
#include <sys/idbg.h>
#include <setjmp.h>
#include <fault.h>
#include <saio.h>
#include "mp.h"
#include "dbgmon.h"
#include <libsc.h>

static void exception_close(void);

/*
 * _fatal_error -- deal with internal coding error
 */
void
_fatal_error(const char *msg)
{

	private.pda_nofault = 0;	/* cancel nofault processing */
	printf("DEBUGGER ERROR detected in %s\n", msg);
	/* careful here */
	_restart();	/* enter command mode */
}

/*
 * error -- print error message for syntax and other errors
 */
void
_dbg_error(const char *msg, void *arg1)
{
	jmpbufptr jbp;

	printf("Error: ");
	printf(msg, arg1);
	putchar('\n');
	/*
	 * Check for nofault in case remote debugging is active
	 */
	if (private.pda_nofault) {
		jbp = private.pda_nofault;
		private.pda_nofault = 0;
		longjmp(jbp, 1);
	}
	_restart();
}

/*
 * restart -- reset stack and return to main loop
 */
void
_restart(void)
{
	extern int _prom_mode;
	symmon_spl();	/* use SYMMON_SR */
	private.dbg_mode = MODE_DBGMON;
	exception_close();
	longjmp(private._restart_buf, 1);
}

#define	MAXCLOSE	3
static int close_cnt;
static ULONG close_list[MAXCLOSE];

/*
 * close_on_exception -- mark a descriptor to be closed on any exception
 */
void
close_on_exception(ULONG fd)
{
	if (close_cnt < MAXCLOSE)
		close_list[close_cnt++] = fd;
}

/*
 * exception_close -- close the descriptors which have been marked to be
 * closed on exception
 */
static void
exception_close(void)
{
	while (close_cnt > 0)
		Close(close_list[--close_cnt]);
	close_cnt = 0;
}

#ifdef DEBUG_REMOVE	/* so ASSERT's can be used */
void
assfail(const char *a, const char *f, long l)
{
	panic("assertion failed: %s, file: %s, line: %d",
		a, f, l);
}
#endif
