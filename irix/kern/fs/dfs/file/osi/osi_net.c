/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_net.c,v 65.4 1998/03/23 16:26:22 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/* Copyright (C) 1995, 1989 Transarc Corporation - All rights reserved */

/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_net.c,v $
 * Revision 65.4  1998/03/23 16:26:22  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 19:58:18  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:12  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:17:44  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.793.1  1996/10/02  18:11:36  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:04  damon]
 *
 * $EndLog$
*/

#include <dcedfs/param.h>	/* Should always be first */
#include <dcedfs/stds.h>
#include <dcedfs/sysincludes.h>	/* Standard vendor system headers */
#include <dcedfs/osi.h>

#if !defined(KERNEL)
#include <dce/pthread.h>
#endif /* !KERNEL */

#ifdef AFS_AIX31_ENV
#include <sys/sleep.h>
#endif

void osi_InitWaitHandle (achandle)
    register struct osi_WaitHandle *achandle;
{
    achandle->proc = (caddr_t) 0;
}

static char waitV;
#ifndef	AFS_AIX31_ENV
static osi_timeout_t WaitHack()
{
    osi_Wakeup(&waitV);
}
#else
static osi_timeout_t WaitHack(pid_t pid) {
    e_post(EVENT_QUEUE, pid);		/* meaningless event type */
}
#endif /* AFS_AIX31_ENV */

#if defined(AFS_SUNOS5_ENV) && defined(KERNEL)
int osi_Wait_r(long ams, struct osi_WaitHandle *ahandle, int aintok)
{
    long code, endTime;

    endTime = osi_Time() + (ams/1000);
    if (ahandle)
	ahandle->proc = (caddr_t) osi_ThreadUnique();
    do {
	int cookie = osi_CallProc_r(WaitHack, (char *)osi_ThreadUnique(), ams);
	code = 0;
	if (aintok) {
	    if (code = osi_SleepInterruptably_r(&waitV))
		code = EINTR;
	} else
	    osi_Sleep_r(&waitV);
	osi_CancelProc_r(WaitHack, (char *) osi_ThreadUnique(), cookie);
	if (code)
	    break;			/* if something happened, quit now */
	/*
	 * if we we're cancelled, quit now
	 */
	if (ahandle && (ahandle->proc == (caddr_t) 0)) {
	    /*
	     * we've been signalled
	     */
	    break;
	}
    } while (osi_Time() < endTime);
    return code;
}
#endif /* SUNOS5 && KERNEL */
/*
 * Cancel osi_Wait
 */
void osi_CancelWait(achandle)
    struct osi_WaitHandle *achandle;
{
#ifndef	AFS_AIX31_ENV
    caddr_t proc;

    proc = achandle->proc;
    if (proc == 0) return;
    achandle->proc = (caddr_t) 0; /* so dude can figure out he was signalled */
    osi_Wakeup(&waitV);
#else
    pid_t pid;

    if ((pid = (pid_t) achandle->proc) == 0)
	return;
    achandle->proc = 0;
    e_post(EVENT_QUEUE, pid);
#endif /* AFS_AIX31_ENV */
}

/*
 * Wait for data on asocket, or ams ms later. asocket may be null.  Returns
 * 0 if timeout and 2 if signalled
 */
int osi_Wait(long ams, struct osi_WaitHandle *ahandle, int aintok)
{
#ifndef	AFS_AIX31_ENV
    long code, endTime;

    endTime = osi_Time() + (ams/1000);
    if (ahandle)
	ahandle->proc = (caddr_t) osi_ThreadUnique();
    do {
	int cookie = osi_CallProc(WaitHack, (char *) osi_ThreadUnique(), ams);
	code = 0;
#ifdef AFS_OSF_ENV
	assert_wait(&waitV, TRUE);
	thread_block();
#else
	if (aintok) {
	    if (code = osi_SleepInterruptably(&waitV))
		code = EINTR;
	} else
	    osi_Sleep(&waitV);
#endif /* AFS_OSF_ENV */
	osi_CancelProc(WaitHack,  (char *) osi_ThreadUnique(), cookie);
	if (code)
	  break;			/* if something happened, quit now */
	/*
	 * if we we're cancelled, quit now
	 */
	if (ahandle && (ahandle->proc == (caddr_t) 0)) {
	    /*
	     * we've been signalled
	     */
	    break;
	}
    } while (osi_Time() < endTime);
#else /* AFS_AIX31_ENV */
    register int code = 0;
    long endTime;
    register pid_t pid = osi_GetPid();

    endTime = osi_Time() + (ams / 1000);
    if (ahandle)
	ahandle->proc = (caddr_t) pid;
    do {
	int cookie = osi_CallProc(WaitHack, (caddr_t) pid, ams);
	switch (e_wait(EVENT_QUEUE, EVENT_QUEUE
		       , aintok ? EVENT_SIGRET : EVENT_SHORT)) {
	    case EVENT_SIG:
		code = EINTR;
		break;
	    default:	/* e_wait return EVENT_SIG or mask of events */
		code = 0;
		break;
	}
	osi_CancelProc(WaitHack,  (char *) pid, cookie);
	if (code)
		break;			/* if something happened, quit now */
	if (ahandle && (ahandle->proc == (caddr_t) 0))
		break;			/* we've been signalled */
    } while (osi_Time() < endTime);
#endif	/* !AFS_AIX31_ENV */
    return code;
}
