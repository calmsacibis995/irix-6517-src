/* @(#)prot_time.h	1.3 91/06/25 NFSSRC4.1 */

/*
 * This file consists of all timeout definition used by rpc.lockd
 */

#define MAX_LM_TIMEOUT_COUNT	1
#define OLDMSG			30	/* counter to throw away old msg */
#define LM_TIMEOUT_DEFAULT 	10
#define LM_GRACE_DEFAULT 	3

extern int 	LM_TIMEOUT;
extern int	grace_period;
