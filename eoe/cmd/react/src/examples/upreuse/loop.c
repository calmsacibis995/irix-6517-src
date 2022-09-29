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

#include "loop.h"

/*
 * We're using the first 2048 bytes of the
 * per-sproc PRivate Data Area.
 */

static private_area_t* private_area = (private_area_t*)PRDA;

/*
 * All signal handlers set a bit in the signal_state bit-mask,
 * but only the first to be received does a lonjmp
 */

static void
overrun_handler()
{
        if (private_area->messages & MESSAGES_OVERRUN) {
                fprintf(stderr, "[%d]: Overrun\n", private_area->pid);
        }
        if (atomicSetInt((int*)&private_area->sigmap, GOTSIGNAL_OVERRUN) == 0) {
                longjmp(private_area->jbenv, 1);
        }
        
}

static void
underrun_handler()
{
        if (private_area->messages & MESSAGES_UNDERRUN) {
                fprintf(stderr, "[%d]: Underrun\n", private_area->pid);
        }
        if (atomicSetInt((int*)&private_area->sigmap, GOTSIGNAL_UNDERRUN) == 0) {
                longjmp(private_area->jbenv, 1);
        }
}        

static void
termination_handler()
{
        if (private_area->messages & MESSAGES_TERMINATE) {
                fprintf(stderr, "[%d]: Terminate\n", private_area->pid);
        }
        if (atomicSetInt((int*)&private_area->sigmap, GOTSIGNAL_TERMINATE) == 0) {
                longjmp(private_area->jbenv, 1);
        }
}

static void
alarm_handler()
{
        if (private_area->messages & MESSAGES_ALARM) {
                fprintf(stderr, "[%d]: Alarm\n" , private_area->pid);
        }
        if (atomicSetInt((int*)&private_area->sigmap, GOTSIGNAL_ALARM) == 0) {
                longjmp(private_area->jbenv, 1);
        }
}

static int
setup_signals(void)
{
        if ((int)signal(SIGUSR1, underrun_handler) == -1) {
                perror("[setup_signals]: Error while setting underrun_error signal");
                return (-1);
        }

        if ((int)signal(SIGUSR2, overrun_handler) == -1) {
                perror("[setup_signals]: Error while setting overrun_error signal");
                return (-1);
        }
        if ((int)signal(SIGHUP, termination_handler) == -1) {
                perror("[setup_signals]: Error while setting termination signal");
                return (-1);
        }

        if ((int)signal(SIGALRM, alarm_handler) == -1) {
                perror("[setup_signals]: Error while setting alarm signal");
                return (-1);
        }
}

static int
ignore_signals(void)
{
        if ((int)signal(SIGUSR1, SIG_IGN) == -1) {
                perror("[ignore_signals]: Error while disabling underrun_error signal");
                return (-1);
        }

        if ((int)signal(SIGUSR2, SIG_IGN) == -1) {
                perror("[ignore_signals]: Error while disabling overrun_error signal");
                return (-1);
        }
        if ((int)signal(SIGHUP, SIG_IGN) == -1) {
                perror("[ignore_signals]: Error while disabling termination signal");
                return (-1);
        }

        if ((int)signal(SIGALRM, SIG_IGN) == -1) {
                perror("[ignore_signals]: Error while disabling alarm signal");
                return (-1);
        }
}
        
void
loop_dispatcher(void* args)
{
        loop_args_t* loop_args = (loop_args_t*)args;
        int errorcause = 0;
        int errorcode = 0;
        uint sigmap = 0;
        int workfcode = 0;
        int previous_minor;
        uint sigmask;

        assert(loop_args != 0);
        assert(loop_args->in_args.pid != 0);
        assert(loop_args->in_args.workf != 0);
        assert(loop_args->in_args.workf_args != 0);
        assert(loop_args->in_args.ssema_done != 0);

        private_area->messages = loop_args->in_args.messages;
        private_area->pid = loop_args->in_args.pid;
        private_area->sigmap = 0;

        if (setjmp(private_area->jbenv) != 0) {
                /*
                 * When the return value is different
                 * than 0 we are being called from a longjmp
                 */
                errorcause = 0;
                errorcode = LD_ERROR_GOTSIGNAL;
                sigmap = private_area->sigmap;
                goto done;
        }
        
        if (setup_signals() < 0) {
                errorcause = errno;
                errorcode = LD_ERROR_SETUPSIGNALS;
                goto done;
        }

        sigmask = sigmask(SIGUSR1) |
                  sigmask(SIGUSR2) |
                  sigmask(SIGHUP)  |
                  sigmask(SIGALRM);

        (void)sigsetmask(0);
        
        if (loop_args->in_args.frs != 0) {
                barrier(loop_args->in_args.all_procs_enqueued,
                        loop_args->in_args.barrier_waiters);
                if (frs_join(loop_args->in_args.frs) < 0) {
                        errorcause = errno;
                        errorcode = LD_ERROR_JOIN;
                        goto done;
                }
        }

        if (loop_args->in_args.messages | MESSAGES_JOIN) {
                fprintf(stderr, "[%d]: Joined\n", loop_args->in_args.pid);
        }
                
        while (1) {
                  
                if ((workfcode = (*loop_args->in_args.workf)(loop_args->in_args.workf_args)) != 0) {
                        errorcode = 0;
                        goto done;
                }

                /*
                 * After we are done with our computations, we
                 * yield the cpu. THe yield call will not return until
                 * it's our turn to execute again.
                 */

                if (loop_args->in_args.frs != 0) {   
                        if ((previous_minor = frs_yield()) < 0) {
                                errorcode = LD_ERROR_YIELD;
                                goto done;
                        }
                } else {
                        previous_minor = 0;
                }

                if (loop_args->in_args.messages | MESSAGES_YIELD) {
                        fprintf(stderr, "[%d]: Yield (%d)\n",
                                loop_args->in_args.pid,
                                previous_minor);
                }
        }

  done:
        (void)sigsetmask(sigmask);
        if (ignore_signals() < 0) {
                errorcode = errno;
                errorcause = LD_ERROR_IGNSIGNALS;
        }
                
        loop_args->out_args.errorcause = errorcause;
        loop_args->out_args.errorcode = errorcode;
        loop_args->out_args.sigmap = sigmap;
        loop_args->out_args.workfcode = workfcode;

        (void)sigsetmask(0);

        sema_v(loop_args->in_args.ssema_done);
}

                
loop_args_t*
loop_args_create(pid_t pid,
                 frs_t* frs,
                 uint messages,
                 int (*workf)(void*),
                 barrier_t* all_procs_enqueued,
                 int barrier_waiters,
                 void* workf_args)
{
        loop_args_t* loop_args;

        assert(workf);
        assert(workf_args);
        
        if ((loop_args = (loop_args_t*)malloc(sizeof(loop_args_t))) == NULL) {
                return (NULL);
        }

        loop_args->in_args.pid = pid;
        loop_args->in_args.frs = frs;
        loop_args->in_args.messages = messages;
        loop_args->in_args.workf = workf;
        loop_args->in_args.all_procs_enqueued = all_procs_enqueued;
        loop_args->in_args.barrier_waiters = barrier_waiters;
        loop_args->in_args.workf_args = workf_args;

        if ((loop_args->in_args.ssema_done = sema_create()) == NULL) {
                free(loop_args);
                return (NULL);
        }

        loop_args->out_args.errorcause = 0;
        loop_args->out_args.errorcode = 0;
        loop_args->out_args.sigmap = 0;      
        loop_args->out_args.workfcode = 0;

        return (loop_args);
}

void
loop_args_destroy(loop_args_t* loop_args)
{
        sema_destroy(loop_args->in_args.ssema_done);
        free(loop_args);
}


void
loop_wait(loop_args_t* loop_args)
{
        sema_p(loop_args->in_args.ssema_done);
}

void
loop_args_print(loop_args_t* loop_args)
{
        assert(loop_args != 0);
        
        printf("Loop in_args PID[%d]:\n", loop_args->in_args.pid);
        printf("frs: 0x%x, messages: 0x%x, workf: 0x%x, workf_args: 0x%x, sema: 0x%x\n",
               loop_args->in_args.frs,
               loop_args->in_args.messages,
               loop_args->in_args.workf,
               loop_args->in_args.workf_args,
               loop_args->in_args.ssema_done);
        printf("Loop out_args PID[%d]:\n", loop_args->in_args.pid);
        printf("ecause: %d, ecode: %d, sigmap: 0x%x, wcode: %d\n",
               loop_args->out_args.errorcause,
               loop_args->out_args.errorcode,
                loop_args->out_args.sigmap,
                loop_args->out_args.workfcode);
}
