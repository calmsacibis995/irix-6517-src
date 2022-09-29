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

/*
 * Frame Scheduler Tets 
 */

/*
 * This example creates one or more synchronous
 * frame schedulers, all running the same processes
 * as follows:
 * Background:
 *    K1, K2, K3
 * Realtime:
 *    A,D @ 20 Hz
 *    B @ 10 Hz
 *    C @ 5 Hz
 *    E @ 4 Hz
 *    F @ 2 Hz
 *
 */


#include <sys/types.h>
#include <sys/prctl.h>
#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include <ulocks.h>

typedef struct pdesc {
	frs_t*    frs;
	uint      messages;
        uint      nloops;
        uint      xloops;
        uint      ccount;
	int       (*action_function)(struct pdesc*);
} pdesc_t;

#define MAXPROCS   32

#define  PDESC_MSG_JOIN   0x0001
#define  PDESC_MSG_YIELD  0x0002

typedef struct sinfo {
        frs_t*  mfrs;
        int     scpu;
} sinfo_t;

/*
 * Processes will be number according to the
 * following defines:
 */

#define PROCESS_A 0
#define PROCESS_B 1
#define PROCESS_C 2
#define PROCESS_D 3
#define PROCESS_E 4
#define PROCESS_F 5
#define PROCESS_K1 6
#define PROCESS_K2 7
#define PROCESS_K3 8


/*
 * Total number of processes per frs
 */

#define TPROCS 9


extern int processA_loop(pdesc_t*);
extern int processB_loop(pdesc_t*);
extern int processC_loop(pdesc_t*);
extern int processD_loop(pdesc_t*);
extern int processE_loop(pdesc_t*);
extern int processF_loop(pdesc_t*);
extern int processK1_loop(pdesc_t*);
extern int processK2_loop(pdesc_t*);
extern int processK3_loop(pdesc_t*);

extern void sema_init(void);
extern usema_t* sema_create(void);
extern void sema_cleanup();
extern void sema_p(usema_t* sema);
extern void sema_v(usema_t* sema);
extern barrier_t* barrier_create();
