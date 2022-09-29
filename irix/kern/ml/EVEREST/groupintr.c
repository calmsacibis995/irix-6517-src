/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.16 $"

/*
 * Group Interrupt Management - implementation
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reg.h>
#include <sys/pda.h>
#include <sys/sysinfo.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/groupintr.h>
#include <sys/runq.h>			/* for setmustrun, etc prototypes */
#include <sys/frs.h>
#include <sys/rtmon.h>

static allintrgroups_t allintrgroups;

void
intrgroup_init(void)
{
    int i;

    CPUMASK_CLRALL(allintrgroups.allocbv);
    spinlock_init(&allintrgroups.lock, "IGRPMAIN");
    for (i = 0; i < NINTRGROUPS; i++) {
	CPUMASK_CLRALL(allintrgroups.intrgroup[i].cpubv);
	allintrgroups.intrgroup[i].groupid = i;
	spinlock_init(&allintrgroups.intrgroup[i].lock, "IGRPGRP");
    }
}

intrgroup_t*
intrgroup_alloc(void)
{
    int i;
    int s;

    s = mutex_spinlock(&allintrgroups.lock);
    for (i = 0; i < NINTRGROUPS; i++) {
	if (CPUMASK_TSTB(allintrgroups.allocbv, i)) {
	    continue;
	} else {
	    CPUMASK_SETB(allintrgroups.allocbv, i);
	    CPUMASK_CLRALL(allintrgroups.intrgroup[i].cpubv);
	    ASSERT(allintrgroups.intrgroup[i].groupid == i);
	    mutex_spinunlock(&allintrgroups.lock, s);
	    return (&allintrgroups.intrgroup[i]);
	}
    }
    mutex_spinunlock(&allintrgroups.lock, s);
    return (0);
}

void
intrgroup_join(intrgroup_t* intrgroup, cpuid_t cpuid)
{
    int s;
    int flag = 0;
    cpu_cookie_t was_running;
    groupmask_t curr_group_membership;

    ASSERT(intrgroup);
    s = mutex_spinlock(&intrgroup->lock);
    if (CPUMASK_TSTB(intrgroup->cpubv, cpuid)) {
	mutex_spinunlock(&intrgroup->lock, s);
	return;
    }
    mutex_spinunlock(&intrgroup->lock, s);

#if LARGE_CPU_COUNT
    panic("fixme: intrgroup_join, EV_IGRMASK");
#else
    if (cpuid != private.p_cpuid) {
	    flag = 1;
            was_running = setmustrun(cpuid);
    }
    
    s = mutex_spinlock(&intrgroup->lock);
    CPUMASK_SETB(intrgroup->cpubv, cpuid);
    curr_group_membership = EV_GET_LOCAL(EV_IGRMASK);
    CPUMASK_SETB(curr_group_membership, intrgroup->groupid);
    EV_SET_LOCAL(EV_IGRMASK, curr_group_membership);
    mutex_spinunlock(&intrgroup->lock, s);

    if (flag) 
            restoremustrun(was_running);

#endif
}

    
void
intrgroup_unjoin(intrgroup_t* intrgroup, cpuid_t cpuid)
{
    int s, flag = 0;
    cpu_cookie_t was_running;
    groupmask_t curr_group_membership;

    ASSERT(intrgroup);
    ASSERT(cpuid < maxcpus);
    
    s = mutex_spinlock(&intrgroup->lock);
    if (!(CPUMASK_TSTB(intrgroup->cpubv, cpuid))) {
	mutex_spinunlock(&intrgroup->lock, s);
	return;
    }
    mutex_spinunlock(&intrgroup->lock, s);

#if LARGE_CPU_COUNT
    panic("fixme: intrgroup_unjoin, EV_IGRMASK");
#else
    if (cpuid != private.p_cpuid) {
	    flag = 1;
            was_running = setmustrun(cpuid);
    }	
    s = mutex_spinlock(&intrgroup->lock);
    CPUMASK_CLRB(intrgroup->cpubv, cpuid);
    curr_group_membership = EV_GET_LOCAL(EV_IGRMASK);
    CPUMASK_CLRB(curr_group_membership, intrgroup->groupid);
    EV_SET_LOCAL(EV_IGRMASK, curr_group_membership);
    mutex_spinunlock(&intrgroup->lock, s);

    if (flag) 
            restoremustrun(was_running);

#endif
}

void
intrgroup_free(intrgroup_t* intrgroup)
{
    int s;
    int i;

    ASSERT(intrgroup);

    for (i = 0; i < EV_MAX_CPUS; i++) {
	if (CPUMASK_TSTB(intrgroup->cpubv, i)) {
	    intrgroup_unjoin(intrgroup, i);
	}
    }

    ASSERT(CPUMASK_IS_ZERO(intrgroup->cpubv));

    s = mutex_spinlock(&allintrgroups.lock);
    CPUMASK_CLRB(allintrgroups.allocbv, intrgroup->groupid);
    mutex_spinunlock(&allintrgroups.lock, s);
}


    
/* ARGSUSED */
void
groupintr(eframe_t *ep, void *arg)
{
    void frs_handle_group_intr(void);

    LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_GROUP_INTR, NULL, NULL, NULL, NULL);
    
	/*
	 * Frame Scheduler
	 */
	if (private.p_frs_flags) {
		frs_handle_group_intr();
	}
    LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_GROUP_INTR, NULL, NULL, NULL);
}

int
sendgroupintr(intrgroup_t* intrgroup)
{
	ASSERT(intrgroup != 0);
    
	EV_SET_LOCAL(EV_SENDINT,
			EVINTR_VECTOR(EVINTR_LEVEL_GROUPACTION,
			EVINTR_DEST(EVINTR_GROUPDEST(intrgroup->groupid))));
	return 0;
}	    
