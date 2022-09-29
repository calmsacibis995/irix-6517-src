/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include "defs.h"
#include "ipc.h"

/*
 * Some fixed real-time loop parameters
 */

#define NLOOPS 100
#define LOGLOOPS 25000

static int
process_loop(void)
{
        double res = 2;
        int i;
        static int overall_count = 0;
        
        for (i = 0; i < LOGLOOPS; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        if (++overall_count > NLOOPS) {
                return (1);
        } else {
                return (0);
        }
}

main(int argc, char** argv)
{
	frs_t* frs;
        enqdesc_t enqdesc_list[4];
        int i;
        pdesc_t* pdesc;
        int host_processor;        

        host_processor = 1;

        if (argc > 1) {
                host_processor = atoi(argv[1]);
        }

        printf("Running Process [%d] on Processor [%d]\n",
               getpid(), host_processor);

        
        /*
         * We want this process to cover half a major frame.
         */

        for (i = 0; i < 4; i += 2) {
                enqdesc_list[i].minor = i;
                enqdesc_list[i].disc =  FRS_DISC_RT |
                                        FRS_DISC_UNDERRUNNABLE |
                                        FRS_DISC_OVERRUNNABLE |
                                        FRS_DISC_CONT;
        }
        for (i = 1; i < 4; i += 2) {
                enqdesc_list[i].minor = i;
                enqdesc_list[i].disc =  FRS_DISC_RT;
        }


        /*
         * Enqueue on frs running on host_processor,
         * Sequence 0 (first on queue),
         * enqueue list of length 4,
         * enqueue_list as specified above.
         */                
        if ((frs = enqueue_client(host_processor, 0, 4, enqdesc_list)) == NULL) {
                fprintf(stderr,
                        "Error while enqueueing process [%d]\n",
                        getpid());
                exit(1);
        }

        if ((pdesc = pdesc_create(frs,
                                  0,
                                  process_loop)) == NULL) {
                fprintf(stderr,
                        "Error while creating pdesc for process [%d]\n",
                        getpid());
                exit(1);
        }

        skeleton(pdesc);

	exit(0);
}
