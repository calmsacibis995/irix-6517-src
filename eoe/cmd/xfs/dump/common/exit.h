#ifndef EXIT_H
#define EXIT_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/exit.h,v 1.1 1995/06/21 22:47:13 cbullis Exp $"

/* exit codes for main and child processes
 */
#define EXIT_NORMAL	0	/* normal completion / don't exit */
#define EXIT_ERROR	1	/* resource error or resource exhaustion */
#define EXIT_INTERRUPT	2	/* interrupted (operator or device error) */
#define EXIT_FAULT	4	/* code fault */

typedef size_t exit_t;

#endif /* EXIT_H */
