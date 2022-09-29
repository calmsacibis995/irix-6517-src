/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/frs.h>
#include "pqueue.h"
#include "pdesc.h"
#include "sema.h"
#include "loop.h"
#include "work.h"
#include <sys/wait.h>
#include <sys/resource.h>
#include "controller.h"


static void
single_frs(void* args)
{
        controller_args_t* cargs  = (controller_args_t*)args;
        pdesc_t* pdesc;
        workf_args_t* workf_args;
        loop_args_t* loop_args;
        frs_t* frs;
        barrier_t* all_procs_enqueued;
        int count = 0;

        /*
         * Create sync barrier
         */
        all_procs_enqueued = barrier_create();
        
        if ((frs = frs_create_master(cargs->cpu,
                                     FRS_INTRSOURCE_CCTIMER,
                                     cargs->period,
                                     1,
                                     0)) == NULL) {
                perror("frs_create_master");
                goto error0;
        }


        /*
         * Enqueue processes
         */
                
        for (count = 0; count < cargs->numprocs; count++) {

                if ((pdesc = pdesc_pool_get(cargs->pdesc_pool)) == NULL) {
                        perror("pdesc_pool_get");
                        goto error0;
                }
 
                if (pdesc_pool_put(cargs->busy_pool, pdesc) < 0) {
                        perror("pdesc_pool_put - busy");
                        goto error0;
                }

                if ((workf_args = workf_args_create(cargs->workloops,
                                                    cargs->randloops,
                                                    cargs->workcalls)) == NULL) {
                        perror("workf_args_create");
                        goto error0;
                }

                loop_args =  loop_args_create(pdesc->pid,
                                              frs,
                                              MESSAGES_ALARM | cargs->messages,
                                              example_work_function,
                                              all_procs_enqueued, /* sync barrier */
                                              cargs->numprocs + 1, /* barrier waiters */
                                              workf_args);
                if (loop_args == NULL) {
                        workf_args_destroy(workf_args);
                        perror("loop_args_create");
                        goto error0;
                }


                pdesc_asyncrun_function(pdesc, loop_dispatcher, (void*)loop_args);

                if (frs_enqueue(frs, pdesc->pid, 0, FRS_DISC_RT) < 0) {
                        perror("frs_enqueue");
                        goto error0;
                }
 
                        
        }

        barrier(all_procs_enqueued, cargs->numprocs + 1);

        if (frs_start(frs) < 0) {
                perror("frs_start");
                goto error0;
        }

        for (count = 0; count < cargs->numprocs; count++) {
                if ((pdesc = pdesc_pool_get(cargs->busy_pool)) == NULL) {
                        perror("pdesc_pool_get -- busy_pool");
                        goto error0;
                }

                /*
                 * Wait for each process to finish
                 */
                loop_wait((loop_args_t*)pdesc->args);

                /*
                 * Show exit state for each process
                 */
                loop_args_print((loop_args_t*)pdesc->args);

                /*
                 * Free workf argument storage
                 */
                workf_args_destroy((workf_args_t*)
                                   ((loop_args_t*)pdesc->args)->in_args.workf_args);

                /*
                 * Free pdesc argument storage
                 */
                loop_args_destroy((loop_args_t*)pdesc->args);
                        
                if (pdesc_pool_put(cargs->pdesc_pool, pdesc) < 0) {
                        perror("loop_args_destroy");
                        goto error0;
                }
        }

        barrier_destroy(all_procs_enqueued); 
        frs_destroy(frs);
        cargs->errorcode = 0;
        return;

  error0:
        barrier_destroy(all_procs_enqueued); 
        frs_destroy(frs);
        cargs->errorcode = 1;
        return;
}

void
test_singlefrs(int cpu,                    /* cpu to run frame scheduler */
               ulong period,               /* period for cc intr */
               int nssloops,               /* number of create/destroy cycles */
               int numprocs,               /* number of procs to frame schedule */
               uint workloops,             /* cycle burning loops */
               uint randloops,             /* choose fixed or random workloops */
               uint workcalls,             /* number of rt loops */
               uint messages               /* what messages to print */
               )
{
        queue_t* pdesc_pool;
        queue_t* busy_pool;
        pdesc_t* pdesc;
        int i;

        int errorcode;
        controller_args_t* cargs;
        int stat;
        pid_t cpid;
        
        /*
         * Create a pool of sprocs
         */

        if ((pdesc_pool = pdesc_pool_create()) == NULL) {
                perror("pdesc_pool_create");
                exit(1);
        }
        
        if ((busy_pool = pdesc_pool_create()) == NULL) {
                perror("pdesc_pool_create - busy");
                exit(1);
        }

        for (i = 0; i < (numprocs * 2); i++) {
                pdesc = pdesc_create();
                if (pdesc_pool_put(pdesc_pool, pdesc) < 0) {
                        perror("pdesc_pool_put");
                        exit(1);
                }
        }


        /*
         * This process is going to be the master so
         * I mask off the frs signals. I am handling
         * exceptions at the child level only.
         */


        (void)sigsetmask(sigmask(SIGUSR1) |
                         sigmask(SIGUSR2) |
                         sigmask(SIGHUP));
        
        for (i = 0; i < nssloops; i++) {
                              
                cargs = controller_args_create(cpu,
                                               period,
                                               numprocs,
                                               workloops,
                                               randloops,
                                               workcalls,
                                               messages,
                                               pdesc_pool,
                                               busy_pool);
                                               
                cpid = sproc(single_frs, PR_SALL, (void*)cargs);
                
                waitpid(cpid, &stat, 0);

                controller_args_destroy(cargs);
                
                if (cargs->errorcode != 0) {
                        break;
                }
        }

        pdesc_pool_destroy(pdesc_pool);
        pdesc_pool_destroy(busy_pool);

}

                
