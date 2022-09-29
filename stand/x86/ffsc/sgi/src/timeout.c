/*
 * timeout.c
 *	Timeout timers for the FFSC
 */

#include <vxworks.h>

#include <errno.h>
#include <iolib.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <taskvarlib.h>
#include <time.h>

#include "ffsc.h"
#include "timeout.h"


/* Global variables */
timeout_t *timeoutContext;
int			TimeoutExpired;


/* Internal functions */
static void TimerPop(int);


/*
 * timeoutCancel
 *	Cancel the timeout timer
 */
void
timeoutCancel(void)
{
	timer_cancel(timeoutContext->Timer);
	timeoutContext->Info = NULL;
}


/*
 * timeoutDisable
 *	Disable the timeout timer temporarily
 */
void
timeoutDisable(void)
{
	sigprocmask(SIG_BLOCK, &timeoutContext->WaitSigs, NULL);
}


/*
 * timeoutEnable
 *	Enable a disabled timeout timer
 */
void
timeoutEnable(void)
{
	sigprocmask(SIG_UNBLOCK, &timeoutContext->WaitSigs, NULL);
}


/*
 * timeoutInit
 *	Initialize timeout handlers for a task. Sets up a task variable
 *	containing a timer and other task-specific info and arranges
 *	to trap SIGALRM signals.
 */
STATUS
timeoutInit(void)
{
	struct sigaction AlarmAction;

	/* Make "timeoutContext" a VxWorks task variable */
	if (taskVarAdd(0, (int *) &timeoutContext) != OK) {
		ffscMsgS("Unable to make message context a task variable");
		return ERROR;
	}

	/* Make "TimeoutExpired" a VxWorks task variable */
	if (taskVarAdd(0, &TimeoutExpired) != OK) {
		ffscMsgS("Unable to global varible TimeoutExpired a task variable");
		return ERROR;
	}
	/* Allocate storage for the message context */
	timeoutContext = (timeout_t *) malloc(sizeof(timeout_t));
	if (timeoutContext == NULL) {
		ffscMsg("Unable to allocate storage for timeout timer");
		return ERROR;
	}

	/* Create a posix timer to use for timeouts */
	if (timer_create(CLOCK_REALTIME, NULL, &timeoutContext->Timer) != OK) {
		ffscMsgS("Unable to create posix timeout timer");
		return ERROR;
	}

	/* Set up a SIGALRM handler */
	AlarmAction.sa_flags   = SA_INTERRUPT;
	AlarmAction.sa_handler = TimerPop;
	sigemptyset(&AlarmAction.sa_mask);

	if (sigaction(SIGALRM, &AlarmAction, NULL) != OK) {
		ffscMsgS("Unable to set SIGALRM handler");
		timer_delete(timeoutContext->Timer);
		return ERROR;
	}

	/* Arrange not to be annoyed by SIGALRM for now */
	sigemptyset(&timeoutContext->WaitSigs);
	sigaddset(&timeoutContext->WaitSigs, SIGALRM);
	timeoutDisable();

	/* Note that the timeout timer is not currently in use */
	timeoutContext->Info = NULL;

	TimeoutExpired = 0;

	return OK;
}

/*
 * timeoutRead
 *	Same as regular "read" system call, except that it times
 *	out after the specified period. If a timeout occurs, errno
 *	is set to EINTR.
 *
 *	Due to problem with the longjump inside of the read command.  the
 * timer is always disabled with the read is executed.  An ioctl call
 * is made in the timing loop check for data being available.
 */
int
timeoutRead(int FD, void *Buffer, size_t Len, int Period)
{
	int Result;
	int BytesInBuf;

	Result = 0;

	/* Arrange for the timeout signal */
	if (timeoutSet(Period, "on read") != OK) {
		/* timeoutSet should have logged the error already */
		errno = EINVAL;
		return -1;
	}

	/* 
	 * Enter a loop and check for data before doing a read. "TimeoutExpired" is
	 * set by the timeoutInit function 
	 */
	timeoutEnable();

	while (!TimeoutExpired) {

		/* check for data to be read */
      BytesInBuf = -1;
      ioctl(FD, FIONREAD, (int) &BytesInBuf);

		/* data is present, so pause the timer and go read it */
		if (BytesInBuf > 0) {
			timeoutDisable();
			Result = read(FD, (char *) Buffer, Len);
			break;
		}
		else
			(void) taskDelay(1);
	}
	timeoutDisable();
	timeoutCancel();

	return Result;
}

/*
 * timeoutSet
 *	Go through the necessary gyrations to set the timeout timer
 */
STATUS
timeoutSet(int TimeoutUSecs, const char *Info)
{
	sigset_t Pending;
	struct itimerspec TimerValue;
	struct siginfo SigInfo;

	/* Make sure it is safe to use the timeout timer and longjmp buffer */
	if (timeoutContext->Info != NULL) {
		ffscMsg("timeoutSet called while timeout timer in use!");
		return ERROR;
	}

	/* Purge an outstanding SIGALRM if present */
	if (sigpending(&Pending) != OK) {
		ffscMsgS("sigpending failed in timeoutSet");
		return ERROR;
	}
	if (sigismember(&Pending, SIGALRM) == 1) {
		if (sigwaitinfo(&timeoutContext->WaitSigs, &SigInfo) < 0) {
			ffscMsgS("sigwaitinfo failed in timeoutSet");
			return ERROR;
		}
	}

	/* Start the timeout timer */
	TimerValue.it_interval.tv_sec	= 0;
	TimerValue.it_interval.tv_nsec	= 0;
	TimerValue.it_value.tv_sec	= TimeoutUSecs / 1000000;
	TimerValue.it_value.tv_nsec	= (TimeoutUSecs % 1000000) * 1000;
	if (timer_settime(timeoutContext->Timer, 0, &TimerValue, NULL) != OK) {
		ffscMsgS("Unable to set timeout timer");
		return ERROR;
	}

	/* Note that the timer is in use by setting the Info field */
	timeoutContext->Info = (Info == NULL) ? "event" : Info;

	/* reset the timeout flag */
	TimeoutExpired = 0;
	
	return OK;
}


/*
 * timeoutWrite
 *	Same as regular "write" system call, except that it times
 *	out after the specified period. If a timeout occurs, errno
 *	is set to EINTR.
 */
int
timeoutWrite(int FD, const void *Buffer, size_t Len, int Period)
{
	int Result;

#if 0
	/* If a timeout occurs, we will longjmp back to here */
	if (timeoutOccurred() != 0) {
		timeoutCancel();
		errno = EINTR;
		return -1;
	}

	/* Arrange for the timeout signal */
	if (timeoutSet(Period, "on write") != OK) {
		/* timeoutSet should have logged the error already */
		errno = EINVAL;
		return -1;
	}

	/* Write the data */
	timeoutEnable();
#endif
	Result = write(FD, (char *) Buffer, Len);
#if 0
	timeoutDisable();
	timeoutCancel();
#endif

	return Result;
}



/*----------------------------------------------------------------------*/
/*									*/
/*			     INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * TimerPop
 *	Signal handler for SIGALRM, which should only occur when
 *	a read or write operation has timed out
 */
void
TimerPop(int SigNum)
{
	if (SigNum != SIGALRM) {
		ffscMsg("Reached TimerPop for signal %d!", SigNum);
		return;
	}

	if (timeoutContext->Info == NULL) {
		ffscMsg("Got SIGALRM when timeout timer not in use");
		return;
	}

	ffscMsg("Timed out %s", timeoutContext->Info);

	TimeoutExpired = 1;
}

