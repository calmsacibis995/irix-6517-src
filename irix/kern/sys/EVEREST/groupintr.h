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
 
#ifndef __SYS_EVEREST_GROUPINTR_H__
#define __SYS_EVEREST_GROUPINTR_H__

/*
 * Group Interrupt Management - definition
 */

#define NINTRGROUPS 16

typedef cpumask_t groupmask_t;

typedef struct intrgroup {
    cpumask_t cpubv;        /* cpus member of this group */
    int groupid;          
    lock_t lock;
} intrgroup_t;

typedef struct allintrgroups {
    intrgroup_t intrgroup[NINTRGROUPS];
    groupmask_t allocbv;    /* currently allocated groups */
    lock_t lock;
} allintrgroups_t;

#define intrgroup_get_cpubv(intrgroup)     ((intrgroup)->cpubv)
#define intrgroup_get_groupid(intrgroup)   ((intrgroup)->groupid)
  
void intrgroup_init(void);
intrgroup_t* intrgroup_alloc(void);
void intrgroup_join(intrgroup_t* intrgroup, cpuid_t cpuid);
void intrgroup_unjoin(intrgroup_t* intrgroup, cpuid_t cpuid);
void intrgroup_free(intrgroup_t* intrgroup);
int sendgroupintr(intrgroup_t* intrgroup);


#endif /* __SYS_EVEREST_GROUPINTR_H__ */
