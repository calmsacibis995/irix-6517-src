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
 * Force migration interrupts by purposely running a thread
 * on one node, and placing its memory on a different node.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/pmo.h>
#include <sys/syssgi.h>
#include <sys/numa_tests.h>


#define DATA_POOL_SIZE  (8*1024*1024)

char data_pool[DATA_POOL_SIZE];

void
place_data(char* vaddr, int size, char* node)
{
        pmo_handle_t mld;
        pmo_handle_t mldset;
        raff_info_t  rafflist;
        pmo_handle_t pm;

        if ((mld = mld_create(0, size)) < 0) {
                perror("mld_create");
                exit(1);
        }

        if ((mldset = mldset_create(&mld, 1)) < 0) {
                perror("mldst_create");
                exit(1);
        }

        rafflist.resource = node;
        rafflist.restype = RAFFIDT_NAME;
        rafflist.reslen = (ushort)strlen(node);
        rafflist.radius = 0;
        rafflist.attr = RAFFATTR_ATTRACTION;

        if (mldset_place(mldset,
                         TOPOLOGY_PHYSNODES,
                         &rafflist,
                         1,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
                exit(1);
        }

        if ((pm = pm_create_simple("PlacementFixed",
                                   (void*)(ulong)mld,
                                   "ReplicationDefault",
                                   NULL,
                                   PM_PAGESZ_DEFAULT)) < 0) {
                perror("pm_create_simple");
                exit(1);
        }

        if (pm_attach(pm, vaddr, size) < 0) {
                perror("pm_attach");
                exit(1);
        }

}

void
place_process(char* node)
{
        pmo_handle_t mld;
        pmo_handle_t mldset;
        raff_info_t  rafflist;
        
        /*
         * The mld, radius = 0 (from one node only)
         */
        
        if ((mld = mld_create(0, 0)) < 0) {
                perror("mld_create");
                exit(1);
        }

        /*
         * The mldset
         */

        if ((mldset = mldset_create(&mld, 1)) < 0) {
                perror("mldset_create");
                exit(1);
        }

        /*
         * Placing the mldset with the one mld
         */

        rafflist.resource = node;
        rafflist.restype = RAFFIDT_NAME;
        rafflist.reslen = (ushort)strlen(node);
        rafflist.radius = 0;
        rafflist.attr = RAFFATTR_ATTRACTION;

        if (mldset_place(mldset,
                         TOPOLOGY_PHYSNODES,
                         &rafflist, 1,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
                exit(1);
        }

        /*
         * Attach this process to run only on the node
         * where thr mld has been placed.
         */

        if (process_mldlink(0, mld, RQMODE_MANDATORY) < 0) {
                perror("process_mldlink");
                exit(1);
        }

}

void
migr_data(char* vaddr, int size, char* node)
{
        pmo_handle_t mld;
        pmo_handle_t mldset;
        raff_info_t  rafflist;
        pmo_handle_t pm;

        if ((mld = mld_create(0, size)) < 0) {
                perror("mld_create");
                exit(1);
        }

        if ((mldset = mldset_create(&mld, 1)) < 0) {
                perror("mldst_create");
                exit(1);
        }

        rafflist.resource = node;
        rafflist.restype = RAFFIDT_NAME;
        rafflist.reslen = (ushort)strlen(node);
        rafflist.radius = 0;
        rafflist.attr = RAFFATTR_ATTRACTION;

        if (mldset_place(mldset,
                         TOPOLOGY_PHYSNODES,
                         &rafflist,
                         1,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
                exit(1);
        }

        if (migr_range_migrate(vaddr, size, mld) <  0) {
                perror("migr_range_migrate");
                exit(1);
        }

}
void
print_refcounters(int pfn)
{
        refcounter_info_t refcounter_info;
        int numnodes;
        int i;
        int node;

        refcounter_info.pfn = (pfn_t)pfn;
        
        if (syssgi(SGI_NUMA_TESTS, REFCOUNTER_GET, -1, &refcounter_info) < 0) {
                perror("SGI_NUMA_TESTS, REFCOUNTER_GET");
                exit(1);
        }

        if (syssgi(SGI_NUMA_TESTS, NUMNODES_GET, -1, &numnodes) < 0) {
                perror("SGI_NUMA_TESTS, NUMNODES_GET");
                exit(1);
        }
        
        for (i = 0; i < NUMATEST_HWPAGES; i++) {
                printf("PFN: 0x%x, SUB 0x%x:\n", pfn, i);
                for (node = 0; node < numnodes; node++) {
                        printf("COUNTER[%d]=0x%x\n", node,
                               refcounter_info.set[i].counter[node]);
                }
        }
}


void
init_buffer(void* m, size_t size)
{
        size_t i;
        char* p = (char*)m;
        
        for (i = 0; i < size; i++) {
                p[i] = (char)i;
        }
}

long
buffer_auto_dotproduct_update(void* m, size_t size)
{
        size_t i;
        size_t j;
        char* p = (char*)m;
        long sum = 0;

        
        for (i = 0, j = size - 1; i < size; i++, j--) {
                sum += (long)p[i]-- * (long)p[j]++;
        }

        return (sum);
}
        



void
do_stuff(void* m, size_t size)
{
        long total = 0;
        int count = 10000;

        while (count--) {
                total += buffer_auto_dotproduct_update(m, size);
        }
}

void
main(int argc, char** argv)
{
        char* thread_node;
        char* mem_node;
        char* migr_node;


        if (argc != 4) {
                fprintf(stderr, "Usage %s <thread-node> <mem-node> <migr-node>\n", argv[0]);
                exit(1);
        }

        thread_node = argv[1];
        mem_node = argv[2];
        migr_node = argv[3];

        /*
         * Place data
         */
        place_data(&data_pool[0], DATA_POOL_SIZE, mem_node);
        init_buffer(&data_pool[0], DATA_POOL_SIZE);
        
        /*
         * Place process
         */
        place_process(thread_node);

        /*
         * Migration
         */
        migr_data(&data_pool[0], DATA_POOL_SIZE, migr_node);

}        
        
        
        
                

        
        

        
