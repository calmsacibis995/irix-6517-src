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

#ident "$Revision: 1.10 $"

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
#include <sys/SN/agent.h>
#include <sys/SN/intr.h>
#include <sys/SN/groupintr.h>
#include <sys/proc.h>			/* for runq_private.h */
#include <sys/frs.h>
#include <sys/rtmon.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include "sn_private.h"

void groupintr(void *);

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
	    allintrgroups.intrgroup[i].cpuarray =
		     kmem_zalloc(sizeof(groupintrinfo_t) * maxcpus, KM_SLEEP);
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
    char intrname[16];
    int bit, rv;

    ASSERT(intrgroup);
    s = mutex_spinlock(&intrgroup->lock);
    if (CPUMASK_TSTB(intrgroup->cpubv, cpuid)) {
	mutex_spinunlock(&intrgroup->lock, s);
	return;
    }

    CPUMASK_SETB(intrgroup->cpubv, cpuid);
    sprintf(intrname, "Group %d", intrgroup->groupid);
    bit = intr_reserve_level(cpuid, -1, 0, GRAPH_VERTEX_NONE, intrname);
    if (bit == -1)
	cmn_err(CE_PANIC, "Can't reserve bit %d for group %d interrupt!\n",
		bit, intrgroup->groupid);
    rv = intr_connect_level(cpuid, bit, 0, groupintr,
			    (void *)(__psint_t)intrgroup->groupid, NULL);
    if (rv == -1)
	cmn_err(CE_PANIC, "Can't connect bit %d for group %d interrupt!\n",
		bit, intrgroup->groupid);
    intrgroup->cpuarray[cpuid].intr_level = bit;
    intrgroup->cpuarray[cpuid].dest_nasid = pdaindr[cpuid].pda->p_nasid;
	
    mutex_spinunlock(&intrgroup->lock, s);

}

void
intrgroup_unjoin(intrgroup_t* intrgroup, cpuid_t cpuid)
{
    int s;
    int bit, rv;

    ASSERT(intrgroup);
    s = mutex_spinlock(&intrgroup->lock);
    if (!CPUMASK_TSTB(intrgroup->cpubv, cpuid)) {
	mutex_spinunlock(&intrgroup->lock, s);
	return;
    }

    CPUMASK_CLRB(intrgroup->cpubv, cpuid);

    bit = intrgroup->cpuarray[cpuid].intr_level;
    rv = intr_disconnect_level(cpuid, bit);
    if (rv == -1)
	cmn_err(CE_PANIC, "Can't disconnect bit %d for group %d interrupt!\n",
		bit, intrgroup->groupid);
    intr_unreserve_level(cpuid, intrgroup->cpuarray[cpuid].intr_level);

    intrgroup->cpuarray[cpuid].intr_level = 0;
    intrgroup->cpuarray[cpuid].dest_nasid = INVALID_NASID;
	
    mutex_spinunlock(&intrgroup->lock, s);

}

void
intrgroup_free(intrgroup_t* intrgroup)
{
    int s;
    int i;

    ASSERT(intrgroup);

    for (i = 0; i < maxcpus; i++) {
	if (CPUMASK_TSTB(intrgroup->cpubv, i)) {
	    intrgroup_unjoin(intrgroup, i);
	}
    }

    ASSERT(CPUMASK_IS_ZERO(intrgroup->cpubv));

    kmem_free(intrgroup->cpuarray,
		sizeof(groupintrinfo_t) * maxcpus);

    s = mutex_spinlock(&allintrgroups.lock);
    CPUMASK_CLRB(allintrgroups.allocbv, intrgroup->groupid);
    mutex_spinunlock(&allintrgroups.lock, s);
}

    
/* ARGSUSED */
void
groupintr(void *arg)
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

#if 0
	cmn_err(CE_NOTE, "Cpu %d received group interrupt", cpuid());
#endif
}

int
sendgroupintr(intrgroup_t* intrgroup)
{
	int i;
	cpuid_t my_cpu = cpuid();

	ASSERT(intrgroup != 0);

	
	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(intrgroup->cpubv, i) && (i != my_cpu))
			REMOTE_HUB_SEND_INTR(intrgroup->cpuarray[i].dest_nasid,
					intrgroup->cpuarray[i].intr_level);
	}

	if (CPUMASK_TSTB(intrgroup->cpubv, my_cpu))
		groupintr(0);

	return 0;
}	    


