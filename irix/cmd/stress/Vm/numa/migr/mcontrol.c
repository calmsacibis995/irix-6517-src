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


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/pmo.h>
#include <sys/syssgi.h>
#include <sys/numa_tests.h>


#define DATA_POOL_SIZE  (8*1024*1024)
#define CACHE_TRASH_SIZE ((4*1024*1024)/sizeof(long))

char floating_data_pool[DATA_POOL_SIZE];
char fixed_data_pool[DATA_POOL_SIZE];
long cache_trash_buffer[CACHE_TRASH_SIZE];


void
place_data(char* vaddr, int size, char* node, int migr_on)
{
        pmo_handle_t mld;
        pmo_handle_t mldset;
        raff_info_t  rafflist;
        pmo_handle_t pm;
        policy_set_t policy_set;
        migr_policy_uparms_t migr_parms;

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

        migr_policy_args_init(&migr_parms);
        migr_parms.migr_base_enabled = migr_on;
        migr_parms.migr_base_threshold = 10;

        pm_filldefault(&policy_set);

        policy_set.placement_policy_name = "PlacementFixed";
        policy_set.placement_policy_args = (void*)mld;
        policy_set.migration_policy_name = "MigrationControl";
        policy_set.migration_policy_args = (void*)&migr_parms;
        
        if ((pm = pm_create(&policy_set)) < 0) {
                perror("pm_create");
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

long
cache_trash(long* m, size_t long_size)
{
        int i;
        long sum = 0;

        for (i = 0; i < long_size; i++) {
               m[i] = i;
        }

        for (i = 0; i < long_size; i++) {
                sum += m[i];
        }

        return (sum);
}



void
do_stuff(void* m, size_t size, int loops, char* label)
{
        long total = 0;
        int count = loops;

        while (count--) {
                total += buffer_auto_dotproduct_update(m, size);
                total += cache_trash(cache_trash_buffer, CACHE_TRASH_SIZE);
        }
}

void
main(int argc, char** argv)
{
        char* thread_node;
        char* mem_node;

        if (argc != 3) {
                fprintf(stderr, "Usage %s <thread-node> <mem-node>\n", argv[0]);
                exit(1);
        }

        thread_node = argv[1];
        mem_node = argv[2];

        /*
         * Place data, migr on
         */
        place_data(&floating_data_pool[0], DATA_POOL_SIZE, mem_node, 1);
        init_buffer(&floating_data_pool[0], DATA_POOL_SIZE);

        /*
         * Place data, migr off
         */
        place_data(&fixed_data_pool[0], DATA_POOL_SIZE, mem_node, 0);
        init_buffer(&fixed_data_pool[0], DATA_POOL_SIZE);
        
        /*
         * Place process
         */
        place_process(thread_node);

        /*
         * Reference pages & verify
         */

        do_stuff(floating_data_pool, DATA_POOL_SIZE, 50, "FLOATING");
        do_stuff(fixed_data_pool, DATA_POOL_SIZE, 50, "FIXED");
}        
        
        
        
                

        
        

        
