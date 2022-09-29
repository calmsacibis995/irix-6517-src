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

#include "controller.h"


controller_args_t*
controller_args_create(int cpu,                    /* cpu to run frame scheduler */
                       ulong period,               /* period for cc intr */
                       int numprocs,               /* number of procs to frame schedule */
                       uint workloops,             /* cycle burning loops */
                       uint randloops,             /* choose fixed or random workloops */
                       uint workcalls,             /* number of rt loops */
                       uint messages,              /* what messages to print */
                       queue_t* pdesc_pool,        /* free sprocs */
                       queue_t* busy_pool)          /* busy sprocs */
{
        controller_args_t* cargs;

        if ((cargs = (controller_args_t*)malloc(sizeof(controller_args_t))) == NULL) {
                return (NULL);
        }

        cargs->cpu = cpu;
        cargs->period = period;
        cargs->numprocs = numprocs;
        cargs->workloops = workloops;
        cargs->randloops = randloops;
        cargs->workcalls = workcalls;
        cargs->messages = messages;
        cargs->pdesc_pool = pdesc_pool;
        cargs->busy_pool = busy_pool;
        cargs->errorcode = 0;

        return (cargs);
}

void
controller_args_destroy(controller_args_t* cargs)
{
        assert(cargs != 0);
              
        free(cargs);
}

void
controller_args_print(controller_args_t* cargs)
{
        assert(cargs != 0);

        printf("Controller Args:\n");
        printf("cpu: %d, period: %d, numprocs: %d\n",
               cargs->cpu,
               cargs->period,
               cargs->numprocs);
        printf("workloops: %d, randloops: %d, workcalls: %d, errorcode: %d\n",
               cargs->workloops,
               cargs->randloops,
               cargs->workcalls,
               cargs->errorcode);
        printf("Messages:0x%x, pdesc_pool:0x%x, busy_pool: 0x%x\n",
               cargs->messages,
               cargs->pdesc_pool,
               cargs->busy_pool);
}

