#ifndef _SPYTHREAD_H
#define _SPYTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "spyBase.h"
#include "spyCmd.h"

#include <signal.h>
#include <sys/fault.h>
#include <sys/procfs.h>
#include <sys/syscall.h>
#include <sys/types.h>

/* Thread spying
 *
 * Thread debug libraries implement this interface.
 */

typedef uint32_t	spyThread_t;
#define SPYTHREADNULL	((spyThread_t)-1)

typedef struct spyScanOp {
	uint_t		sso_dom;
	spyThread_t	sso_thd;
	int		(*sso_cb)(spyProc_t*, spyThread_t, void*, void*);
	void*		sso_cbArg;
} spyScanOp_t;

typedef struct spyThreadCalls {
	int	stc_version;

	int	(*stc_ProcNew)(spyProc_t*);
	int	(*stc_ProcDel)(spyProc_t*);

	int	(*stc_ScanOp)(spyProc_t*, int, void*, spyScanOp_t*);

} spyThreadCalls_t;

#define STCSSO(sso, d, t, c, ca)	\
	(sso)->sso_dom = d,	\
	(sso)->sso_thd = t,	\
	(sso)->sso_cb = c,	\
	(sso)->sso_cbArg = ca

/* Options for the ScanOp domain.
 *
 *	STC_PROCESS
 * or	STC_THREAD			named thread
 * or	STC_SCAN_USER			all threads with user contexts
 * or	STC_SCAN_KERN			all threads with kernel contexts
 * or	STC_SCAN_STOPPED		all stopped threads
 * or	STC_SCAN_EVENTS			all threads at an event of interest
 */
#define STC_PROCESS		0x0001
#define STC_THREAD		0x0002
#define STC_SCAN_ALL		(STC_SCAN|STC_USER|STC_KERN)
#define STC_SCAN_USER		(STC_SCAN|STC_USER)
#define STC_SCAN_KERN		(STC_SCAN|STC_KERN)
#define STC_SCAN_STOPPED	(STC_SCAN|STC_KERN|STC_STOPPED)
#define STC_SCAN_EVENTS		(STC_SCAN|STC_KERN|STC_EVENTS)


/* Internal masks.
 */
#define STC_SCAN		0x0004
#define STC_USER		0x0010
#define STC_KERN		0x0020
#define	STC_STOPPED		0x0100
#define	STC_EVENTS		0x0200
#define STC_LIVE		0x1000
#define STC_DEAD		0x2000
#define STC_TGT			0x000f
#define STC_CTX			0x00f0
#define STC_LIFE		0xf000

/* Proc ioctl extensions
 * Anything below ('q'<<8) is free - so we can avoid including <sys/procfs.h>
 */
#define SPYCGINFO0	1	/* state info (char[STC_INFO0BUFLEN]) */
#define SPYCGNAME	3	/* user name (spyThread_t*) */
#define SPYCITER	4	/* No-op, for call backs (0) */


#define STC_INFO0BUFLEN	24	/* buffer for inline info0 requests */

typedef int (*spyThreadInit_t)(spyThreadCalls_t*, spyCmdList_t**);

extern int spyThreadInit(spyThreadCalls_t*, spyCmdList_t**);

#ifdef __cplusplus
}
#endif

#endif /* _SPYTHREAD_H */
