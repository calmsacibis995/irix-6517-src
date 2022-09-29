
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _SYS_KRESOURCE_H
#define _SYS_KRESOURCE_H

#ifdef _KERNEL

#include <sys/ktime.h>

struct	irix5_rusage {
	struct irix5_timeval ru_utime;
	struct irix5_timeval ru_stime;
	app32_long_t	ru_maxrss;
	app32_long_t	ru_ixrss;
	app32_long_t	ru_idrss;
	app32_long_t	ru_isrss;
	app32_long_t	ru_minflt;
	app32_long_t	ru_majflt;
	app32_long_t	ru_nswap;
	app32_long_t	ru_inblock;
	app32_long_t	ru_oublock;
	app32_long_t	ru_msgsnd;
	app32_long_t	ru_msgrcv;
	app32_long_t	ru_nsignals;
	app32_long_t	ru_nvcsw;
	app32_long_t	ru_nivcsw;
};

#endif	/* _KERNEL */

#endif /* _SYS_KRESOURCE_H */
