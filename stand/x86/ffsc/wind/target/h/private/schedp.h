/* schedP.h - sched private header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,05jan94,kdl  added include of sched.h.
01a,09nov93,dvs  written
*/

#ifndef __INCschedPh
#define __INCschedPh

#ifdef __cplusplus
extern "C" {
#endif

#include "sched.h"

#define VXWORKS_LOW_PRI         255     /* low priority VxWorks numbering */
#define VXWORKS_HIGH_PRI        0       /* high priority VxWorks numbering */
#define POSIX_LOW_PRI           0       /* low priority POSIX numbering */
#define POSIX_HIGH_PRI          255     /* high priority POSIX numbering */

/* conversion macro */
#define PX_VX_PRIORITY_CONVERT(prior) (posixPriorityNumbering ? \
                                           POSIX_HIGH_PRI - prior : prior)
#define PX_NUMBER_CONVERT(prior) (posixPriorityNumbering ? \
				  prior : POSIX_HIGH_PRI - prior )


#ifdef __cplusplus
}
#endif

#endif /* __INCschedPh */

