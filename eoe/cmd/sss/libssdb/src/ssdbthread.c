/*
 * Copyright 1992-1998 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/*
 *                          SSDBTHREAD.C                             
 *                      SSDB Thread Support file                     
 */

#include "ssdbthread.h"

/* -------------- ssdbhtread_init_critical_section ----------------- */
int ssdbthread_init_critical_section(CRITICAL_SECTION *criticalsection)
{
	int retcode = 0;
	if(!criticalsection) return 0;
	#ifdef OS_UNIX
	#ifdef PTHEAD_THREAD /* POSIX.1c */
		retcode = (pthread_mutex_init(criticalsection,0) == 0) ? 1 : 0;
	#else /* SUNTHREAD_THREAD */
		retcode = (mutex_init(criticalsection,USYNC_THREAD,0) == 0) ? 1 : 0;
	#endif
	#endif

	#ifdef OS_WINDOWS
		InitializeCriticalSection(criticalsection); retcode++;
	#endif
	return retcode;
}

/* ------------- ssdbthread_enter_critical_section ----------------- */
int ssdbthread_enter_critical_section(CRITICAL_SECTION *criticalsection)
{
	int retcode = 0;
	if(!criticalsection) return 0;
	#ifdef OS_UNIX
	#ifdef PTHEAD_THREAD /* POSIX.1c */
		retcode = (pthread_mutex_lock(criticalsection) == 0) ? 1 : 0;
	#else /* SUNTHREAD_THREAD */
		retcode = (mutex_lock(criticalsection) == 0) ? 1 : 0;
	#endif
	#endif /* #ifdef OS_UNIX */
	#ifdef OS_WINDOWS
		EnterCriticalSection(criticalsection); retcode++;
	#endif
	return retcode;
}
/* ------------- ssdbthread_leave_critical_section ----------------- */
int ssdbthread_leave_critical_section(CRITICAL_SECTION *criticalsection)
{
	int retcode = 0;
	if(!criticalsection) return 0;
	#ifdef OS_UNIX
	#ifdef PTHEAD_THREAD /* POSIX.1c */
		retcode = (pthread_mutex_unlock(criticalsection) == 0) ? 1 : 0;
	#else /* SUNTHREAD_THREAD */
		retcode = (mutex_unlock(criticalsection) == 0) ? 1 : 0;
	#endif
	#endif /* #ifdef OS_UNIX */
	#ifdef OS_WINDOWS
		LeaveCriticalSection(criticalsection); retcode++;
	#endif
	return retcode;
}
/* ------------ ssdbthread_done_critical_section ------------------ */
int ssdbthread_done_critical_section(CRITICAL_SECTION *criticalsection)
{
	int retcode = 0;
	if(!criticalsection) return 0;
	#ifdef OS_UNIX
	#ifdef PTHEAD_THREAD /* POSIX.1c */
		retcode = (pthread_mutex_destroy(criticalsection) == 0) ? 1 : 0;
	#else /* SUNTHREAD_THREAD */
		retcode = (mutex_destroy(criticalsection) == 0) ? 1 : 0;
	#endif
	#endif /* #ifdef OS_UNIX */
	#ifdef OS_WINDOWS
		DeleteCriticalSection(criticalsection); retcode++;
	#endif
	return retcode;
}

