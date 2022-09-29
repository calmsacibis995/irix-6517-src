/*
 * os/numa/debug_levels.c
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ifdef NUMADEBUG

#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/idbg.h>
#include "debug_levels.h"


/*
 * A high debug level setting  means more messages.
 * We print out only those messages whose declared
 * level is less or equal than the current numa
 * debug level for the particular mask (module is).
 * If we want a message to print out all the time,
 * we declare a low level; conversely, if we want the
 * message to print only when we are doing extremely
 * detailed debugging, we declare a very high message level.
 */

__uint64_t numa_debug_mask = 0;
static int numa_debug_level[] = {
        0,       /* DC_MIGR */
        200,     /* DC_MLD */
        200,     /* DC_RAFF */
        200,     /* DC_MLDSET */
        0,       /* DC_PM */
        200,     /* DC_ASPM */
	0,       /* DC_MEMD */
	0,       /* DC_BOUNCE */
	0,       /* DC_QUEUE */
	0,       /* DC_UNPEGGING */
	0,       /* DC_TRAFFIC */
        0
};

static int
mask_to_code(__uint64_t mask)
{
        int i;

        ASSERT (mask != 0);
        
        for (i = 0; mask != 0x1; i++) {
                mask >>= 1;
        }

        return (i);
}

void
numa_message_func(__uint64_t mask, int level, char* m, long long v1, long long v2)
{
        if (mask & numa_debug_mask) {
                if (level <= numa_debug_level[mask_to_code(mask)]) {
                        printf("<NUMA>: %s [0x%x, 0x%x]\n", m, v1, v2);
                }
        }
}

void
numa_debug_selector(__uint64_t mask, int level, df_t f, void* arg1, void* arg2)
{
        if (mask & numa_debug_mask) {
                if (level <= numa_debug_level[mask_to_code(mask)]) {
                        (*f)(arg1, arg2);
                }
        }
}


void
mc_msg(int code, int level, char *fmt, ...)
{
	va_list ap;
        __uint64_t mask = 0x1LL << code;

        if (mask & numa_debug_mask) {
                va_start(ap, fmt);
                if (level < numa_debug_level[code]) {
                        icmn_err(CE_NOTE, fmt, ap);
                }
                va_end(ap);
        }
}

#else /* !DEBUG */

/*ARGSUSED*/
void
mc_msg(int code, int level, char *fmt, ...)
{
	/*NOTREACHED*/
}

#endif /* DEBUG */


