/*
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

#include <sys/types.h>
#include <sys/pmo.h>
#include <sys/prctl.h>
#include <string.h>
#include <fmtmsg.h>
#include <stdio.h>
#include <ulocks.h>


/*
 * NUMA Setup Example
 */

/***************************************************************************
 *                     Basic Model Description                             *
 ***************************************************************************/

/*
 * MLDs
 * An MLD is a physical memory source, placed on some node
 * or neighborhood of nodes.
 * Parameters:
 * - expected size
 * - radius (radius of sphere that will contain the neighborhood of nodes)
 */

/*
 * MLDSETs
 * An mldset is a group of MLDs.
 * Mldsets can be placed according to a specified topology
 * and device affinity:
 * Parameters for creating an mldset:
 * - list of mlds, with node assignment if topology is PHYSNODES
 * Parameters for placing an mldset:
 * - topology_type
 * - device affinity
 * - request mode (advisory, mandatory)
 */

/*
 * PMs
 * Policy Modules are objects that define some kind of policy.
 * We have defined 6 kinds:
 * +Placement
 * +PageSize
 * +Recovery
 * +Migration
 * +Replication
 * +Paging
 */

/***************************************************************************
 *                            Error Handling                               *
 ***************************************************************************/

void
checkerr(int code)
{
        if (code < 0) {
                fmtmsg(MM_SOFT|MM_OPSYS|MM_PRINT|MM_NRECOV,
                       "NUMA:PMO",
                       MM_ERROR,
                       strerror(oserror()),
                       "check code",
                       "numa(3NUMA)");
                exit (1);
        }
}                
                

/***************************************************************************
 *                        Example Preliminary Description                  *
 ***************************************************************************/

/*
 * We need to run an algorithm using nprocs threads, operating on a data
 * structure that can be partitioned into nprocs/2  memory sections, with
 * 2 threads linked to every memory section. The communication pattern
 * is best accomodated by a 3-d cube.
 * The goal then is to specify memory management policies such that
 * each of the nprocs/2 memory sections is placed on a node, and the home nodes
 * form a 3-d cube.
 */


/***************************************************************************
 *                              Example Steps                              *
 ***************************************************************************/

void
create_mlds(int nprocs, char* target_data, size_t target_len, pmo_handle_t* mldlist)
{
        raff_info_t     rafflist[16];
        pmo_handle_t    mldset_handle;
        pmo_handle_t    errhandle;
        policy_set_t    policy_set;
        pmo_handle_t    pm[128];
        int             nmlds;
            
        size_t chunksz;
        int    i;

        nmlds = (nprocs + 1) / 2;
        
        if (nmlds > 128) {
                printf("Too many procs\n");
                exit(1);
        }
        
        /*
         * We need to create 8 MLDs with unknown expected size and
         * radius = 0.
         * pmo_handle_t mld_create(int radius, size_t size);
         */
        
        for (i = 0; i < nmlds; i++) {
                mldlist[i] = mld_create(0, MLD_DEFAULTSIZE);
                checkerr(mldlist[i]);
        }

        /*
         * Now we need to specify how these mlds should be placed.
         * The basic parameters to be specified here are:
         * a) Placement relative to each other [TOPOLOGY]
         *    I'll ask for a cube,
         * b) Absolute placement in system: [RESOURCE AFFINITY]
         *    I want to be close to the graphics head and to my
         *    data file.
         */

        mldset_handle = mldset_create(mldlist, nmlds);
        checkerr(mldset_handle);


        /*
         * Resource affinity
         */
        rafflist[0].resource = "/dev/console";
        rafflist[0].restype = RAFFIDT_NAME;
        rafflist[0].reslen = 13;
        rafflist[0].radius = 1;
        rafflist[0].attr = RAFFATTR_ATTRACTION;

        errhandle = mldset_place(mldset_handle,
                                 TOPOLOGY_CPUCLUSTER,
                                 rafflist, 1,
                                 RQMODE_ADVISORY);
        
        checkerr(errhandle);


        /*
         * Now I want to partition my target data area into 8 sections (ranges)
         * and then set the placement policy for each data area so that memory
         * gets allocated from some specific MLD.
         * To do this I first need to create 8 policy modules of type
         * "FIXED". Then I need to create the ranges, and associate the
         * policy modules.
         */

        /*
         * Creation of the policy modules.
         */

        /*
         * Setup the policies
         */
        policy_set.placement_policy_name = "PlacementFixed";
        policy_set.placement_policy_args = NULL; /* we'll set the mld handle below */
        policy_set.fallback_policy_name = "FallbackDefault";
        policy_set.fallback_policy_args = NULL;
        policy_set.replication_policy_name = "ReplicationDefault";
        policy_set.replication_policy_args = NULL;
        policy_set.migration_policy_name = "MigrationDefault";
        policy_set.migration_policy_args = NULL;
        policy_set.paging_policy_name = "PagingDefault";
        policy_set.paging_policy_args = NULL;
        policy_set.page_size = PM_PAGESZ_DEFAULT;
        policy_set.policy_flags = 0;
                
        for (i = 0; i < nmlds; i++) {
                policy_set.placement_policy_args = (void*)mldlist[i];
                pm[i] = pm_create(&policy_set);
                checkerr(pm[i]);
        }

        /*
         * And finally we attach the address space ranges to the policy
         * groups.
         */
        chunksz = target_len / 8;
        for (i = 0; i < nmlds; i++) {
                checkerr(pm_attach(pm[i],
                                   &target_data[chunksz*i],
                                   chunksz));
        }

}


void
connect_processes(pid_t* sprocid_list, pmo_handle_t* mldlist, int nprocs)
{
        int i;

        /*
         * Now we want 2 processes linked to each mld.
         * pmo_handle_t mld_linkprocess(pmo_handle_t mld, pid_t pid);
         */

        for (i = 0; i < nprocs; i++) {
                checkerr(process_mldlink(sprocid_list[i],
                                         mldlist[i/2],
                                         RQMODE_ADVISORY));
        }
}

#define DSIZE (4*1024*1024)
char data[DSIZE];
#define NLOOPS 1

volatile int pleasewait;

void
action(void* arg)
{
        char* p = &data[0];
        int i;
        int j;
        int k;
        long sum = 0;
        int size = DSIZE;

        while (*(volatile int*)(&pleasewait)) {
                sginap(0);
        }
        
        for (i = 0; i < size; i++) {
                p[i] = (char)i;
        }

        for (k = 0; k < NLOOPS; k++) {
                for (i = 0, j = size - 1; i < size; i++, j--) {
                        sum += (long)p[i]-- * (long)p[j]++;
                }
        }
}

main(int argc, char** argv)
{
        pid_t proclist[128];
        pmo_handle_t mldlist[128];
        int i;
        int nsprocs;

        if (argc != 2) {
                fprintf(stderr, "Usage: %s <nsprocs>\n", argv[0]);
                exit(1);
        }

        nsprocs = atoi(argv[1]);

        usconfig(CONF_INITUSERS, 512);
        *(volatile int*)(&pleasewait) = 1;
        
        create_mlds(nsprocs, data, DSIZE, mldlist);
        
        for (i = 0; i < nsprocs; i++) {
                proclist[i] = sproc(action, PR_SALL);
                checkerr(proclist[i]);
        }

        connect_processes(proclist, mldlist, nsprocs);

        *(volatile int*)(&pleasewait) = 0;
        
        for (i = 0; i < nsprocs; i++) {
                int status;
                wait(&status);
        }
}


        
