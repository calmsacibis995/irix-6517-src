#ifndef _SPYCOMMON_H
#define _SPYCOMMON_H

#ifdef __cplusplus
extern "C" {
#endif


#ifdef DEBUG
#include "spyIO.h"
#include <stdio.h>
#include <sys/types.h>

#define SANITY(cond, prnt) \
	if (!(cond)) {				\
		ioTrace("InSanity (%s:%d): " #cond, __FILE__, __LINE__);\
		ioTrace prnt;			\
		abort();			\
	}

extern ulong_t	spyTrace;
extern int	spyTraceIndent;

#define TEnter(flg, prnt)	int __ret; TTrace(flg, prnt); spyTraceIndent+=2
		
#define TReturn(r)		return (__ret = (r), spyTraceIndent-=2, __ret)

#define TTrace(flg, prnt) \
	if (spyTrace & (flg)) {				\
		ioTrace("%*s", spyTraceIndent, "");	\
		ioTrace prnt;				\
	}

#define TL0	0x00000001	/* less frequent */
#define TL1	0x00000002
#define TL2	0x00000004	/* more frequent */

#define TIO	0x00000100	/* IO subsystem */
#define TPR	0x00000200	/* procfs ioctls */

#else

#define TEnter(flg, prnt)	/* No-op */
#define TTrace(flg, prnt)	/* No-op */
#define TReturn(r)		return (r)
#define SANITY(cond, prnt)	/* No-op */

#endif


#ifdef __cplusplus
}
#endif

#endif /* _SPYCOMMON_H */
