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

#include "multi.h"

/*
 * Realtime loops
 */
#define NLOOPS 20
#define WA 10
#define WB 40000

int counterA = NLOOPS;
int counterB = NLOOPS;

int
processA_loop(void)
{
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


