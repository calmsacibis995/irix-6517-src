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

#include "main.h"

/*
 * Realtime loops
 */
#define NLOOPS 20
#define WA 20000
#define WB 40000

int counterA = NLOOPS;
int counterB = NLOOPS;

int
processA_loop(void)
{
        frs_overrun_info_t overrun_info;
        extern  frs_t* master_frs;
        int pid = getpid();
        
        assert(master_frs != 0);

        if (frs_getattr(master_frs, 0, pid, FRS_ATTR_OVERRUNS, &overrun_info) < 0) {
                perror("[processA_loop]: could not get ovruns");
                exit(1);
        }
        printf("MasterFRS, Process[%d]-> OV:%d, UD:%d\n",
               pid,
               overrun_info.overruns,
               overrun_info.underruns);

        
	if (counterA-- > 0) {
		volatile int i;
		volatile double res = 2;
		for (i = 0; i < WA; i++) {
			res = res * log(res) - res * sqrt(res);
		}
		return (0);
	} else {
		return (1);
	}
}

int
processB_loop(void)
{
        frs_overrun_info_t overrun_info;
        extern  frs_t* slave_frs;
        int pid = getpid();
        
        assert(slave_frs != 0);

        if (frs_getattr(slave_frs, 0, pid, FRS_ATTR_OVERRUNS, &overrun_info) < 0) {
                perror("[processB_loop]: could not get ovruns");
                exit(1);
        }
        printf("SlaveFRS, Process[%d]-> OV:%d, UD:%d\n",
               pid,
               overrun_info.overruns,
               overrun_info.underruns);

	if (counterB-- > 0) {
		volatile int i;
		volatile double res = 2;
		for (i = 0; i < WB; i++) {
			res = res * log(res) - res * sqrt(res);
		}
		return (0);
	} else {
		return (1);
	}
}


