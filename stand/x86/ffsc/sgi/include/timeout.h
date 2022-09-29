/*
 * timeout.h
 *	Declarations & prototypes for timeout timers.
 */

#ifndef _TIMEOUT_H_
#define _TIMEOUT_H_

#include <setjmp.h>
#include <signal.h>
#include <timers.h>

/* Unfortunatetly, many of the blocking I/O functions in VxWorks */
/* are irrevocably restartable: if an interrupt occurs while you */
/* are blocked for I/O, the task will resume blocking when the   */
/* interrupt handler is finished. As a result, you can't just    */
/* let a timer interrupt time out an I/O operation, as is done   */
/* in Unix. Instead, you must use setjmp/longjmp to bail out of  */
/* the offending syscall. These timeout timers hide the ugliness */
/* of setjmp/longjmp. Note that a task may have only one timeout */
/* timer.							 */


/* Type declarations */
typedef struct timeout {
	const char *Info;	/* Descriptive info */
	jmp_buf  LongJmpBuf;	/* setjmp/longjmp buffer for timeouts */
	sigset_t WaitSigs;	/* Signals to honor while waiting on R/W */
	timer_t  Timer;		/* Timeout timer ID */
} timeout_t;

extern timeout_t *timeoutContext;


/* Function macros */
#define timeoutOccurred() (setjmp(timeoutContext->LongJmpBuf))


/* Function prototypes */
void	timeoutCancel(void);
void	timeoutDisable(void);
void	timeoutEnable(void);
STATUS	timeoutInit(void);
int	timeoutRead(int, void *, size_t, int);
STATUS	timeoutSet(int, const char *);
int	timeoutWrite(int, const void *, size_t, int);

#endif  /* _TIMEOUT_H_ */
