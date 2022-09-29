 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ifdef __STDC__
	#pragma weak sched_get_priority_max = _sched_get_priority_max
	#pragma weak sched_get_priority_min = _sched_get_priority_min
#endif
#include "synonyms.h"
#include <errno.h>
#include <sched.h>

int
sched_get_priority_max(int policy)
{
	switch(policy) {
		case SCHED_FIFO:
		case SCHED_RR:
		case SCHED_NP:
			return PX_PRIO_MAX;
		case SCHED_TS:
			return TS_PRIO_MAX;
		default:
			setoserror(EINVAL);
			return -1;
	}
}

int
sched_get_priority_min(int policy)
{
	switch(policy) {
		case SCHED_FIFO:
		case SCHED_RR:
		case SCHED_NP:
			return PX_PRIO_MIN;
		case SCHED_TS:
			return TS_PRIO_MIN;
		default:
			setoserror(EINVAL);
			return -1;
	}
}
