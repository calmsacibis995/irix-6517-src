/*
 * lib/libc/src/sys/pm.c
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

#include "synonyms.h"
#include <assert.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/pmo.h>
#include <sys/syssgi.h>
#include <sys/migr_parms.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>


#define MAPPED_FILE "/dev/zero"

/***************************************************************************
 *                              MLD API                                    *
 ***************************************************************************/
#ifdef __STDC__
	#pragma weak mld_create = _mld_create
	#pragma weak mld_create_special = _mld_create_special
	#pragma weak mld_destroy = _mld_destroy
	#pragma weak mldset_create = _mldset_create
	#pragma weak mldset_create_special = _mldset_create_special
	#pragma weak mldset_place = _mldset_place
	#pragma weak mldset_destroy = _mldset_destroy
	#pragma weak pm_create = _pm_create
	#pragma weak pm_create_simple = _pm_create_simple
	#pragma weak pm_create_special = _pm_create_special
	#pragma weak pm_filldefault = _pm_filldefault
	#pragma weak pm_destroy = _pm_destroy
	#pragma weak pm_attach = _pm_attach
	#pragma weak pm_getdefault = _pm_getdefault
	#pragma weak pm_setdefault = _pm_setdefault
	#pragma weak pm_getall = _pm_getall
	#pragma weak pm_getstat = _pm_getstat
	#pragma weak pm_setpagesize = _pm_setpagesize
	#pragma weak process_mldlink = _process_mldlink
        #pragma weak process_cpulink = _process_cpulink
	#pragma weak migr_range_migrate = _migr_range_migrate
	#pragma weak migr_policy_args_init = _migr_policy_args_init
	#pragma weak numa_acreate = _numa_acreate
#endif

pmo_handle_t
mld_create(int radius, long size)
{
        mld_info_t mld_info;

        mld_info.radius = radius;
        mld_info.size = size;
        return ((pmo_handle_t)pmoctl(PMO_MLDCREATE, &mld_info, NULL));
}

pmo_handle_t
mld_create_special(int radius, long size, pmo_handle_t requested)
{
        mld_info_t mld_info;

        mld_info.radius = radius;
        mld_info.size = size;
        return ((pmo_handle_t)pmoctl(PMO_MLDCREATEX, &mld_info, requested));
}

int
mld_destroy(pmo_handle_t mld_handle)
{
        return ((int)pmoctl(PMO_MLDDESTROY, mld_handle, NULL));
}



/***************************************************************************
 *                           MLDSET API                                    *
 ***************************************************************************/        

pmo_handle_t
mldset_create(pmo_handle_t* mldlist, int mldlist_len)
{
        mldset_info_t mldset_info;

        mldset_info.mldlist = mldlist;
        mldset_info.mldlist_len = mldlist_len;
        return ((pmo_handle_t)pmoctl(PMO_MLDSETCREATE, &mldset_info, NULL));
}

pmo_handle_t
mldset_create_special(pmo_handle_t* mldlist, int mldlist_len, pmo_handle_t requested)
{
        mldset_info_t mldset_info;

        mldset_info.mldlist = mldlist;
        mldset_info.mldlist_len = mldlist_len;
        return ((pmo_handle_t)pmoctl(PMO_MLDSETCREATEX, &mldset_info, requested));
}

int
mldset_place(pmo_handle_t mldset_handle,
             topology_type_t topology_type,
             raff_info_t* rafflist,
             int rafflist_len,
             rqmode_t rqmode)
{
        mldset_placement_info_t mldset_placement_info;

        mldset_placement_info.mldset_handle = mldset_handle;
        mldset_placement_info.topology_type = topology_type;
        mldset_placement_info.rafflist = rafflist;
        mldset_placement_info.rafflist_len = rafflist_len;
        mldset_placement_info.rqmode = rqmode;
        return ((int)pmoctl(PMO_MLDSETPLACE, &mldset_placement_info, NULL));
}

int
mldset_destroy(pmo_handle_t mldset_handle)
{
        return ((int)pmoctl(PMO_MLDSETDESTROY, mldset_handle, NULL));
}
        
/***************************************************************************
 *                             PM  API                                     *
 ***************************************************************************/

pmo_handle_t
pm_create(policy_set_t* policy_set)
{
        pm_info_t pm_info;

        strncpy(pm_info.placement_policy_name,
                policy_set->placement_policy_name,
                PM_NAME_SIZE);
        pm_info.placement_policy_args = policy_set->placement_policy_args;
        
        strncpy(pm_info.fallback_policy_name,
                policy_set->fallback_policy_name,
                PM_NAME_SIZE);
        pm_info.fallback_policy_args = policy_set->fallback_policy_args;
        
        strncpy(pm_info.replication_policy_name,
                policy_set->replication_policy_name,
                PM_NAME_SIZE);
        pm_info.replication_policy_args = policy_set->replication_policy_args;
        
        strncpy(pm_info.migration_policy_name,
                policy_set->migration_policy_name,
                PM_NAME_SIZE);
        pm_info.migration_policy_args = policy_set->migration_policy_args;
        
        strncpy(pm_info.paging_policy_name,
                policy_set->paging_policy_name,
                PM_NAME_SIZE);
        pm_info.paging_policy_args = policy_set->paging_policy_args;

        pm_info.page_size = policy_set->page_size;
	pm_info.policy_flags = policy_set->policy_flags;
	pm_info.page_wait_timeout = policy_set->page_wait_timeout;

        return ((pmo_handle_t)pmoctl(PMO_PMCREATE, &pm_info, NULL));
}

pmo_handle_t
pm_create_special(policy_set_t* policy_set, pmo_handle_t *requested)
{
        pm_info_t pm_info;

        strncpy(pm_info.placement_policy_name,
                policy_set->placement_policy_name,
                PM_NAME_SIZE);
        pm_info.placement_policy_args = policy_set->placement_policy_args;
        
        strncpy(pm_info.fallback_policy_name,
                policy_set->fallback_policy_name,
                PM_NAME_SIZE);
        pm_info.fallback_policy_args = policy_set->fallback_policy_args;
        
        strncpy(pm_info.replication_policy_name,
                policy_set->replication_policy_name,
                PM_NAME_SIZE);
        pm_info.replication_policy_args = policy_set->replication_policy_args;
        
        strncpy(pm_info.migration_policy_name,
                policy_set->migration_policy_name,
                PM_NAME_SIZE);
        pm_info.migration_policy_args = policy_set->migration_policy_args;
        
        strncpy(pm_info.paging_policy_name,
                policy_set->paging_policy_name,
                PM_NAME_SIZE);
        pm_info.paging_policy_args = policy_set->paging_policy_args;

        pm_info.page_size = policy_set->page_size;
	pm_info.policy_flags = policy_set->policy_flags;
	pm_info.page_wait_timeout = policy_set->page_wait_timeout;

        return ((pmo_handle_t)pmoctl(PMO_PMCREATEX, &pm_info, requested));
}

pmo_handle_t
pm_create_simple(char* plac_name,
                 void* plac_args,
                 char* repl_name,
                 void* repl_args,
                 size_t page_size)
{
        pm_info_t pm_info;

        strncpy(pm_info.placement_policy_name,
                plac_name,
                PM_NAME_SIZE);
        pm_info.placement_policy_args = plac_args;
        
        strncpy(pm_info.fallback_policy_name,
                "FallbackDefault",
                PM_NAME_SIZE);
        pm_info.fallback_policy_args = NULL;
        
        strncpy(pm_info.replication_policy_name,
                repl_name,
                PM_NAME_SIZE);
        pm_info.replication_policy_args = repl_args;
        
        strncpy(pm_info.migration_policy_name,
                "MigrationDefault",
                PM_NAME_SIZE);
        pm_info.migration_policy_args = NULL;
        
        strncpy(pm_info.paging_policy_name,
                "PagingDefault",
                PM_NAME_SIZE);
        pm_info.paging_policy_args = NULL;

        pm_info.page_size = page_size;
	pm_info.policy_flags = 0;
	pm_info.page_wait_timeout = 0;
        
        return ((pmo_handle_t)pmoctl(PMO_PMCREATE, &pm_info, NULL));
}

void
pm_filldefault(policy_set_t* policy_set)
{
        policy_set->placement_policy_name = "PlacementDefault";
        policy_set->placement_policy_args = (void*)1L;
        policy_set->fallback_policy_name = "FallbackDefault";
        policy_set->fallback_policy_args = NULL;
        policy_set->replication_policy_name = "ReplicationDefault";
        policy_set->replication_policy_args = NULL;
        policy_set->migration_policy_name = "MigrationDefault";
        policy_set->migration_policy_args = NULL;
        policy_set->paging_policy_name = "PagingDefault";
        policy_set->paging_policy_args = NULL;
        policy_set->page_size = PM_PAGESZ_DEFAULT;
        policy_set->policy_flags = 0;
        policy_set->page_wait_timeout = 0;
}

int
pm_destroy(pmo_handle_t pm_handle)
{
        return ((pmo_handle_t)pmoctl(PMO_PMDESTROY, pm_handle, NULL));
}

int
pm_attach(pmo_handle_t pm_handle, void* base_addr, size_t length)
{
        vrange_t vrange;

        vrange.base_addr = (caddr_t)base_addr;
        vrange.length = length;

        return ((pmo_handle_t)pmoctl(PMO_PMATTACH, pm_handle, &vrange));
}

pmo_handle_t
pm_getdefault(mem_type_t mem_type)
{

        return ((pmo_handle_t)pmoctl(PMO_PMGETDEFAULT, mem_type, NULL));
}

pmo_handle_t
pm_setdefault(pmo_handle_t pm_handle, mem_type_t mem_type)
{

        return ((pmo_handle_t)pmoctl(PMO_PMSETDEFAULT, pm_handle, mem_type));
}

int
pm_getall(void* base_addr, size_t length, pmo_handle_list_t* pmo_handle_list)
{
        vrange_t vrange;

        vrange.base_addr = (caddr_t)base_addr;
        vrange.length = length;

        return ((int)pmoctl(PMO_PMGETALL, &vrange, pmo_handle_list));
}

int
pm_getstat(pmo_handle_t pm_handle, pm_stat_t* pm_stat)
{
        return ((int)pmoctl(PMO_PMSTAT, pm_handle, pm_stat));
}

int
pm_setpagesize(pmo_handle_t pm_handle, size_t page_size)
{
        return ((int)pmoctl(PMO_PMSETPAGESIZE, pm_handle, page_size));
}


/***************************************************************************
 *                            PROCESS API                                  *
 ***************************************************************************/

int
process_mldlink(pid_t pid, pmo_handle_t mld_handle, rqmode_t rqmode)
{
        mldlink_info_t mldlink_info;

        mldlink_info.pid = pid;
        mldlink_info.mld_handle = mld_handle;
        mldlink_info.lcpuid = MLDLINK_ANYCPU;
        mldlink_info.rqmode = rqmode;
        
        return ((int)pmoctl(PMO_PROCMLDLINK, &mldlink_info, NULL));
}

int
process_cpulink(pid_t pid, pmo_handle_t mld_handle, cpuid_t lcpuid, rqmode_t rqmode)
{
        mldlink_info_t mldlink_info;

        mldlink_info.pid = pid;
        mldlink_info.mld_handle = mld_handle;
        mldlink_info.lcpuid = lcpuid;
        mldlink_info.rqmode = rqmode;
        
        return ((int)pmoctl(PMO_PROCMLDLINK, &mldlink_info, NULL));
}
        

/***************************************************************************
 *                     USER INITIATED MIGRATION                            *
 ***************************************************************************/

int
migr_range_migrate(void* base_addr, size_t length, pmo_handle_t pmo_handle)
{
        vrange_t vrange;
        
        vrange.base_addr = (caddr_t)base_addr;
        vrange.length = length;

        return ((int)pmoctl(PMO_USERMIGRATE, &vrange, pmo_handle));
}

/***************************************************************************
 *              Initialization of migr policy args structure               *
 ***************************************************************************/


void
migr_policy_args_init(migr_policy_uparms_t* p)
{
        migr_parms_t migr_parms_base;

        PMOCTL_SETVERSION(p);

        /*
         * Get current system parameters
         */
        if (syssgi(SGI_NUMA_TUNE, MIGR_PARMS_GET, NULL, &migr_parms_base) < 0) {
                p->migr_base_enabled = SETON;
                p->migr_base_threshold = 20;   /* 20% threshold */
                p->migr_freeze_enabled = SETON;
                p->migr_freeze_threshold = 40; /* 40% threshold */
                p->migr_melt_enabled = SETON;
                p->migr_melt_threshold = 80;   /* 80% threshold */
                p->migr_enqonfail_enabled = SETOFF;
                p->migr_dampening_enabled = SETOFF;
                p->migr_dampening_factor = 50; /* 50% factor */
                p->migr_refcnt_enabled = SETON;
                return;
        }

        switch (migr_parms_base.migr_as_kparms.migr_base_mode) {
        case MIGRATION_MODE_DIS:
        case MIGRATION_MODE_OFF:        
                p->migr_base_enabled = SETOFF;
                break;
        case MIGRATION_MODE_EN:
        case MIGRATION_MODE_ON:
                p->migr_base_enabled = SETON;
                break;
        default:
                p->migr_base_enabled = SETOFF;
        }

        switch (migr_parms_base.migr_as_kparms.migr_refcnt_mode) {
        case REFCNT_MODE_DIS:
        case REFCNT_MODE_OFF:
                p->migr_refcnt_enabled = SETOFF;
                break;
        case REFCNT_MODE_EN:
        case REFCNT_MODE_ON:        
                p->migr_refcnt_enabled = SETON;
                break;
        default:
                p->migr_refcnt_enabled = SETOFF;
        }

        p->migr_base_threshold = migr_parms_base.migr_as_kparms.migr_base_threshold;
        p->migr_freeze_enabled = migr_parms_base.migr_as_kparms.migr_freeze_enabled;
        p->migr_freeze_threshold = migr_parms_base.migr_as_kparms.migr_freeze_threshold;
        p->migr_melt_enabled = migr_parms_base.migr_as_kparms.migr_melt_enabled;
        p->migr_melt_threshold = migr_parms_base.migr_as_kparms.migr_melt_threshold;
        p->migr_enqonfail_enabled = migr_parms_base.migr_as_kparms.migr_enqonfail_enabled;
        p->migr_dampening_enabled = migr_parms_base.migr_as_kparms.migr_dampening_enabled;
        p->migr_dampening_factor = migr_parms_base.migr_as_kparms.migr_dampening_factor;

}




/***************************************************************************
 *                     NUMA-AWARE MALLOC API                               *
 ***************************************************************************/

/*
 * Create an arena based on the mld 
 */
void*
numa_acreate(pmo_handle_t mld, size_t arena_size, size_t page_size)
{
	int fd;
	void *base;
	policy_set_t policy_set;
	pmo_handle_t pm;
	void *ap;

	if (arena_size == ARENA_SIZE_DEFAULT) {
		/* We need pick an arena size for the user */
		arena_size = NUMA_ARENA_SIZE_DEFAULT;
	}

	if (page_size == PAGE_SIZE_DEFAULT) {
		/* We need pick a page size for the user */
		page_size = PM_PAGESZ_DEFAULT;
	}
	
	/* 
	 * Map in an address range for our policy module
	 */
	fd = open(MAPPED_FILE, O_RDWR);
	base = mmap(0, arena_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED) {
		return(NULL);
	}

#if DEBUG
	printf("numa_acreate(): [0x%x, 0x%x + 0x%x] is mapped\n", 
	       base, base, arena_size);
#endif /* DEBUG */
	
	/* 
	 * Initialize a policy set for the policy module 
	 */
	policy_set.placement_policy_name = "PlacementFixed";
	policy_set.placement_policy_args = (void *)(__uint64_t)mld;
        policy_set.fallback_policy_name = "FallbackDefault";
        policy_set.fallback_policy_args = NULL;
        policy_set.replication_policy_name = "ReplicationDefault";
        policy_set.replication_policy_args = NULL;
        policy_set.migration_policy_name = "MigrationDefault";
        policy_set.migration_policy_args = NULL;
        policy_set.paging_policy_name = "PagingDefault";
        policy_set.paging_policy_args = NULL;

        policy_set.page_size = page_size;
        policy_set.policy_flags = 0;
        policy_set.page_wait_timeout = 0;
        
	/* 
	 * Create a pm by the given mld and attach it the address space 
	 * At the page fault time, the page will be only allocated at the
	 * mld specified by the policy module
	 */
	pm = pm_create(&policy_set);
	if (pm < 0) {
		return(NULL);
	}
	
	if (pm_attach(pm, base, arena_size) < 0) {
		return(NULL);
	}

	/* 
	 * Create an arena 
	 */
	ap = acreate(base, arena_size, 0, NULL, NULL);
	if (ap == NULL) {
		return(NULL);
	}

#if DEBUG
	printf("numa_acreate(): arena [0x%x, 0x%x + 0x%x] for pm[%d] is created\n", 
	       ap, ap, arena_size, pm);
	amallopt(M_DEBUG, 1, ap); /* Turn on DEBUG flag */
#endif /* DEBUG */
	
	return(ap);
}

/***************************************************************************
 *                     PRIVATE API FOR DPLACE AND SIMILAR PROGRAMS 	   *
 ***************************************************************************/

int
__pm_get_page_info(void* base_addr, size_t length, 
					pm_pginfo_t* pginfo_buf, int buf_len)
{
        vrange_t 	vrange;
	pm_pginfo_list_t pginfo_list;

        vrange.base_addr = (caddr_t)base_addr;
        vrange.length = length;

	pginfo_list.pm_pginfo = pginfo_buf;
	pginfo_list.length = buf_len;

        return ((int)pmoctl(PMO_PMGET_PAGE_INFO, &vrange, &pginfo_list));
}

dev_t
__mld_to_node(pmo_handle_t mld_handle)
{
        return ((int)pmoctl(PMO_MLDGETNODE, mld_handle, 0));
}
