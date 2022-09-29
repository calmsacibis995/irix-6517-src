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
 * Test nmalloc
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/pmo.h>
#include <sys/syssgi.h>
#include <sys/prctl.h>
#include <assert.h>
#include <malloc.h>


#define DATA_POOL_SIZE  (1*1024*1024)


void
create_and_place_mlds(int nmlds, int size, char* affpath, pmo_handle_t* mldlist)
{
        pmo_handle_t mldset;
        raff_info_t  rafflist;
        int i;

        for (i = 0; i < nmlds; i++) {
                if ((mldlist[i] = mld_create(0, size)) < 0) {
                        perror("mld_create");
                        exit(1);
                }
        }

        if ((mldset = mldset_create(mldlist, nmlds)) < 0) {
                perror("mldset_create");
                exit(1);
        }

        rafflist.resource = affpath;
        rafflist.restype = RAFFIDT_NAME;
        rafflist.reslen = (ushort)strlen(affpath);
        rafflist.radius = 0;
        rafflist.attr = RAFFATTR_ATTRACTION;

        if (mldset_place(mldset,
                         TOPOLOGY_FREE,
                         &rafflist,
                         1,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
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
        int count = 100;

        while (count--) {
                total += buffer_auto_dotproduct_update(m, size);
                if (!(count % 100)) {
#ifdef NUMA_TEST_VERBOSE                        
                        printf("Count=%d, Total=0x%x\n", count, total);
#endif                        
                }
        }
}

#define NTHREADS (7)
#define MLDSIZE  (16*1024*1024)
#define ARENASIZE MLDSIZE
#define DATASIZE (4*1024*1024)

void
action(void* arg)
{
        char* p = (char*)arg;

        init_buffer(p, DATASIZE);
        do_stuff(p, DATASIZE);
}

void
main(int argc, char** argv)
{
        char* aff_node;
        pmo_handle_t mldlist[NTHREADS];
        void* arena[NTHREADS];
        char* data[NTHREADS];
        int pid;
        int stat;
        int i;
        

        if (argc != 2) {
                fprintf(stderr, "Usage %s <aff-node>\n", argv[0]);
                exit(1);
        }

        aff_node = argv[1];

        /*
         * Create mlds - I'll just do one per thread
         */
        
        create_and_place_mlds(NTHREADS, MLDSIZE, aff_node, mldlist);

        /*
         * Create the arenas attached to mlds
         */
        for (i = 0; i < NTHREADS; i++) {
                arena[i] =  numa_acreate(mldlist[i], ARENASIZE, PAGE_SIZE_DEFAULT);
                if (arena[i] == NULL) {
                        perror("numa_acreate");
                        exit(1);
                }
        }

        /*
         * Allocate data from each arena
         */
        for (i = 0; i < NTHREADS; i++) {
                data[i] =  numa_amalloc(DATASIZE, arena[i]);
                if (data[i] == NULL) {
                        perror("numa_amalloc");
                        exit(1);
                }
        }        

        for (i = 0; i < NTHREADS; i++) {
                if (sproc(action, PR_SALL, (void*)data[i]) < 0) {
                        perror("sproc");
                        exit(1);
                }
        }
        
 	/*
	 * Exit after the children threads die
	 */
	for (i = 0; i < NTHREADS; i++) {
		pid = wait(&stat);
	}

}        
        
        
        
                

        
        

        
