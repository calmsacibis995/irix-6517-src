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
 * Generate user group interrupts for Frame Scheduler
 */

#include <sys/types.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include <sys/errno.h>

main()
{
	int i = 0;
	while (1) {
		if (schedctl(MPTS_FRS_INTR, 1) < 0) {
			if (errno != EINVAL) {
				perror("schedctl MPTS_FRS_INTR");
			}
		}
		sleep(1);
	}
}

