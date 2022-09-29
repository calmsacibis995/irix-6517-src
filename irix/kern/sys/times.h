#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#ifdef __cplusplus
extern "C" {
#endif

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* sys/times.h */

#ident	"$Revision: 3.14 $"

#include <sys/types.h>

#ifndef _CLOCK_T
#define _CLOCK_T
#if _MIPS_SZLONG == 32
typedef long	clock_t;
#endif
#if _MIPS_SZLONG == 64
typedef int	clock_t;
#endif
#endif /* !_CLOCK_T */

/*
 * Structure returned by times()
 */
struct tms {
	clock_t	tms_utime;		/* user time */
	clock_t	tms_stime;		/* system time */
	clock_t	tms_cutime;		/* user time, children */
	clock_t	tms_cstime;		/* system time, children */
};

#ifndef _KERNEL
extern clock_t	times (struct tms *);

#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_TIMES_H */
