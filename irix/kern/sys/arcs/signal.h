/* ARCS signal definitions
 */
#ifndef __ARCS_SIGNAL_H__
#define __ARCS_SIGNAL_H__
#define _SYS_SIGNAL_H			/* has conflicts with <sys/signal.h> */

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.11 $"

typedef void		(*SIGNALHANDLER)(void);

#define	SIGHUP	1	/* hangup */
#define	SIGINT	2	/* interrupt (rubout) */
#define	SIGQUIT	3	/* quit (ASCII FS) */
#define	SIGILL	4	/* illegal instruction (not reset when caught)*/
#define	SIGTRAP	5	/* trace trap (not reset when caught) */
#define	SIGIOT	6	/* IOT instruction */
#define SIGABRT 6	/* used by abort, replace SIGIOT in the  future */
#define	SIGEMT	7	/* EMT instruction */
#define	SIGFPE	8	/* floating point exception */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	SIGBUS	10	/* bus error */
#define	SIGSEGV	11	/* segmentation violation */
#define	SIGSYS	12	/* bad argument to system call */
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGALRM	14	/* alarm clock */
#define	SIGTERM	15	/* software termination signal from kill */
#define	SIGUSR1	16	/* user defined signal 1 */
#define	SIGUSR2	17	/* user defined signal 2 */
#define	SIGCLD	18	/* death of a child */
#define	SIGPWR	19	/* power-fail restart */

#define	NUM_SIG_TYPES	19

#define SIGDefault	(SIGNALHANDLER)(0)
#define SIGIgnore	(SIGNALHANDLER)(__psint_t)(1)

extern SIGNALHANDLER Signal(LONG, SIGNALHANDLER);

#ifdef __cplusplus
}
#endif

#endif /* __ARCS_SIGNAL_H__ */
