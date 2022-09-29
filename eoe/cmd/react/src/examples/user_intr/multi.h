
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

/*
 * Frame Scheduler -- definition
 * Example II
 */

/*
 * This example creates 3 synchronous frame schedulers
 *
 * Specification for FRS1
 * cpu: 1
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * processes: A@20Hz, K1@BACK
 *
 * Specification for FRS2
 * cpu: 2
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * processes: B@10Hz, C@5Hz, K2@BACK
 *
 * Specification for FRS3
 * cpu: 3
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * processes: D@20Hz, E@4Hz, F@2Hz, K3@BACK
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
	int       (*action_function)(void);
} pdesc_t;

#define MAXPROCS   32

#define  PDESC_MSG_JOIN   0x0001
#define  PDESC_MSG_YIELD  0x0002


extern int processA_loop(void);
extern int processB_loop(void);
extern int processC_loop(void);
extern int processD_loop(void);
extern int processE_loop(void);
extern int processF_loop(void);
extern int processK1_loop(void);
extern int processK2_loop(void);
extern int processK3_loop(void);

extern void sema_init(void);
extern void sema_cleanup();
extern usema_t* sema_create(void);
extern void sema_p(usema_t* sema);
extern void sema_v(usema_t* sema);
extern barrier_t* barrier_create();

