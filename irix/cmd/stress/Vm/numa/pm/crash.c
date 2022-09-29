/*****************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
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
 ****************************************************************************/

/*
 * Test pm_getall across empty sections
 * of the address space
 */

#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/pmo.h>

#define N 1000

main(int ac,char **av)
{
        pm_stat_t pmstat;
        int number;
        pmo_handle_list_t pmo_list;
        int i;
        void *base = (void*)main;

        
        size_t length = (size_t)((long long)&number - (long long)main);;
        pmo_list.handles = (pmo_handle_t *)malloc(N*sizeof(pmo_handle_t));
        pmo_list.length = N;
#ifdef NUMA_TEST_VERBOSE
        printf("Calling pm_getall, base 0x%x, length 0x%x\n", base, length);
#endif        
        number = pm_getall( base , length , &pmo_list);
        if (number < 0) {
                perror("pm_getall");
                exit(1);
        }

        for (i = 0; i < number; i++) {
                if (pm_getstat(pmo_list.handles[i], &pmstat) < 0) {
                        perror("pm_getstat");
                        exit(1);
                }
#ifdef NUMA_TEST_VERBOSE
                printf("[%d] PM<%d>:\n", i, pmo_list.handles[i]);
                printf("Placement: %s\n", pmstat.placement_policy_name);
                printf("Fallback: %s\n", pmstat.fallback_policy_name);
                printf("Replication: %s\n", pmstat.replication_policy_name);
                printf("Migration: %s\n", pmstat.migration_policy_name);
                printf("Pagesize: 0x%x\n", pmstat.page_size);
                printf("PMO handle: 0x%x\n", pmstat.pmo_handle);
#endif
                
        }
                
#ifdef NUMA_TEST_VERBOSE                
        printf("Found %d PM's.\n",number);
#endif             
}
