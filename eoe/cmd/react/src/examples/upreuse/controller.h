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
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

#include <sys/types.h>
#include <sys/frs.h>
#include <setjmp.h>
#include <signal.h>
#include <errno.h>
#include "sema.h"
#include "atomic_ops.h"
#include "pqueue.h"
#include "assert.h"

typedef struct controller_args {
        int cpu;                    /* cpu to run frame scheduler */
        ulong period;               /* period for cc intr */
        int numprocs;               /* number of procs to frame schedule */
        uint workloops;             /* cycle burning loops */
        uint randloops;             /* choose fixed or random workloops */
        uint workcalls;             /* number of rt loops */
        uint messages;              /* what messages to print */
        queue_t* pdesc_pool;        /* Queue of available sprocs */
        queue_t* busy_pool;         /* Queue of busy sprocs */
        int  errorcode;             /* errorcode returned by controller */
} controller_args_t;

extern controller_args_t*
controller_args_create(int cpu,                    /* cpu to run frame scheduler */
                       ulong period,               /* period for cc intr */
                       int numprocs,               /* number of procs to frame schedule */
                       uint workloops,             /* cycle burning loops */
                       uint randloops,             /* choose fixed or random workloops */
                       uint workcalls,             /* number of rt loops */
                       uint messages,              /* what messages to print */
                       queue_t* pdesc_pool,        /* free sprocs */
                       queue_t* busy_pool);        /* busy sprocs */
extern void
controller_args_destroy(controller_args_t* cargs);
extern void
controller_args_print(controller_args_t* cargs);

#endif /* __CONTROLLER_H__ */
