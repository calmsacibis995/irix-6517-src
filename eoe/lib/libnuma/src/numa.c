/*
 * lib/libnuma/src/numa.c
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

#include <assert.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/pmo.h>
#include <sys/syssgi.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#define MAPPED_FILE "/dev/zero"

/***************************************************************************
 *                              MLD API                                    *
 ***************************************************************************/

pmo_handle_t
mld_create(int radius, long size)
{
        mld_info_t mld_info;

        mld_info.radius = radius;
        mld_info.size = size;
        return (pmoctl(PMO_MLDCREATE, &mld_info, NULL));
}

int
mld_destroy(pmo_handle_t mld_handle)
{
        return (pmoctl(PMO_MLDDESTROY, mld_handle, NULL));
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
        return (pmoctl(PMO_MLDSETCREATE, &mldset_info, NULL));
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
        return (pmoctl(PMO_MLDSETPLACE, &mldset_placement_info, NULL));
}

int
mldset_destroy(pmo_handle_t mldset_handle)
{
        return (pmoctl(PMO_MLDSETDESTROY, mldset_handle, NULL));
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
        
        strncpy(pm_info.recovery_policy_name,
                policy_set->recovery_policy_name,
                PM_NAME_SIZE);
        pm_info.recovery_policy_args = policy_set->recovery_policy_args;
        
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

        return (pmoctl(PMO_PMCREATE, &pm_info, NULL));
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
        
        strncpy(pm_info.recovery_policy_name,
                "RecoveryDefault",
                PM_NAME_SIZE);
        pm_info.recovery_policy_args = NULL;
        
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
        
        return (pmoctl(PMO_PMCREATE, &pm_info, NULL));
}

int
pm_destroy(pmo_handle_t pm_handle)
{
        return (pmoctl(PMO_PMDESTROY, pm_handle, NULL));
}

int
pm_attach(pmo_handle_t pm_handle, void* base_addr, size_t length)
{
        vrange_t vrange;

        vrange.base_addr = (caddr_t)base_addr;
        vrange.length = length;

        return (pmoctl(PMO_PMATTACH, pm_handle, &vrange));
}

pmo_handle_t
pm_getdefault()
{
        return (pmoctl(PMO_PMGETDEFAULT, NULL, NULL));
}

pmo_handle_t
pm_setdefault(pmo_handle_t pm_handle)
{
        return (pmoctl(PMO_PMSETDEFAULT, pm_handle, NULL));
}

int
pm_getall(void* base_addr, size_t length, pmo_handle_list_t* pmo_handle_list)
{
        vrange_t vrange;

        vrange.base_addr = (caddr_t)base_addr;
        vrange.length = length;

        return (pmoctl(PMO_PMGETALL, &vrange, pmo_handle_list));
}

int
pm_getstate(pmo_handle_t pm_handle, pm_stat_t* pm_stat)
{
        return (pmoctl(PMO_PMSTAT, pm_handle, pm_stat));
}

int
pm_setpagesize(pmo_handle_t pm_handle, size_t page_size)
{
        return (pmoctl(PMO_PMSETPAGESIZE, pm_handle, page_size));
}


/***************************************************************************
 *                            PROCESS API                                  *
 ***************************************************************************/

int
process_mldlink(pid_t pid, pmo_handle_t mld_handle)
{
        return (pmoctl(PMO_PROCMLDLINK, pid, mld_handle));
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

        return (pmoctl(PMO_USERMIGRATE, &vrange, pmo_handle));
}
        
/***************************************************************************
 *                     NUMA-AWARE MALLOC API                               *
 ***************************************************************************/

/*
 * Create an arena based on the mld 
 */
void *
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
	policy_set.placement_policy_args = (void *)mld;
        policy_set.recovery_policy_name = "RecoveryDefault";
        policy_set.recovery_policy_args = NULL;
        policy_set.replication_policy_name = "ReplicationDefault";
        policy_set.replication_policy_args = NULL;
        policy_set.migration_policy_name = "MigrationDefault";
        policy_set.migration_policy_args = NULL;
        policy_set.paging_policy_name = "PagingDefault";
        policy_set.paging_policy_args = NULL;

        policy_set.page_size = page_size;
        
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







