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

#ifndef __LOOP_H__
#define __LOOP_H__


#include <sys/types.h>
#include <sys/frs.h>
#include <setjmp.h>
#include <signal.h>
#include <errno.h>
#include "sema.h"
#include "atomic_ops.h"


typedef struct in_args {
        pid_t      pid;
        frs_t*     frs;
        uint       messages;
        int        (*workf)(void*);
        barrier_t* all_procs_enqueued;
        int        barrier_waiters;
        void*      workf_args;
        usema_t*   ssema_done;
} in_args_t;

typedef struct out_args {
        int  errorcause;
        int  errorcode;
        uint sigmap;
        int  workfcode;
} out_args_t;

typedef struct loop_args {
        in_args_t in_args;
        out_args_t out_args;
} loop_args_t;

typedef struct private_area {
        uint       sigmap;
        uint       messages;
        pid_t      pid;
        jmp_buf    jbenv;
} private_area_t;


/*
 * Masks for messages
 */

#define MESSAGES_OVERRUN     0x00000001
#define MESSAGES_UNDERRUN    0x00000002
#define MESSAGES_TERMINATE   0x00000004
#define MESSAGES_ALARM       0x00000008

#define MESSAGES_EXCP        0x0000000F

#define MESSAGES_JOIN        0x00000010
#define MESSAGES_YIELD       0x00000020

/*
 * Masks for received signals
 */

#define GOTSIGNAL_OVERRUN    0x00000001
#define GOTSIGNAL_UNDERRUN   0x00000002
#define GOTSIGNAL_TERMINATE  0x00000004
#define GOTSIGNAL_ALARM      0x00000008

/*
 * Error Codes
 */

#define LD_ERROR_GOTSIGNAL     -1
#define LD_ERROR_SETUPSIGNALS  -2
#define LD_ERROR_JOIN          -3
#define LD_ERROR_YIELD         -4
#define LD_ERROR_IGNSIGNALS    -5



void loop_dispatcher(void* args);
loop_args_t* loop_args_create(pid_t pid,
                              frs_t* frs,
                              uint messages,
                              int (*workf)(void*),
                              barrier_t* all_procs_enqueued,
                              int barrier_waiters,
                              void* workf_args);
void loop_args_destroy(loop_args_t* loop_args);
void loop_wait(loop_args_t* loop_args);
void loop_args_print(loop_args_t* loop_args);


#endif /* __LOOP_H__ */
