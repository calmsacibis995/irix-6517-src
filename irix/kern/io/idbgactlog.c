
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident  "$Revision: 1.4 $"

#if defined(DEBUG) && defined(ACTLOG)

#include <sys/types.h>
#include <sys/immu.h>	/* PDAPAGE for cpuid() */
#include <sys/pda.h>	/* cpuid() */
#include <sys/sema.h>	/* splock prototypes */
#include <sys/systm.h>	/* spl7 prototypes */
#include <sys/time.h>

#include <sys/idbgactlog.h>

lock_t actlog_lock;
volatile int actlog_enabled = 0;
volatile int actlog_active = 0;
int actlog_log_cpu = LOG_ANY_CPU;

struct actlog sysactlog;

#define LOCK_ACTLOG()		mutex_spinlock_spl(&actlog_lock, spl7)
#define UNLOCK_ACTLOG(s)	mutex_spinunlock(actlog_lock, s)

void
log_act(struct actlog *al, int act, void *info1, void *info2, void *info3)
{
        register int s, index, wind;
	struct actlogentry *aep;
        timespec_t tv;

	actlog_active = 1;

        nanotime(&tv);

        s = LOCK_ACTLOG();

	if (actlog_log_cpu != LOG_ANY_CPU && actlog_log_cpu != cpuid()) {
        	UNLOCK_ACTLOG(s);
		actlog_active = 0;
		return;
	}
        wind = al->al_wind++;
        index = wind % ACTLOG_SIZE;
	aep = &al->al_log[index];

        aep->ae_act = act;
        aep->ae_info1 = info1;
        aep->ae_info2 = info2;
        aep->ae_info3 = info3;
        aep->ae_sec = tv.tv_sec;
        aep->ae_usec = tv.tv_nsec/1000;
        aep->ae_cpu = cpuid();

        UNLOCK_ACTLOG(s);

	actlog_active = 0;
}

#endif /* DEBUG && ACTLOG */
