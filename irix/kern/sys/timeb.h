#ifndef _SYS_TIMEB_H
#define _SYS_TIMEB_H

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.1 $"

/*
*
* Copyright 1995, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
#include <sys/types.h>

/*
 *  struct returned by 'ftime()' system call
 */
struct timeb {
	time_t	time;		/* the seconds portion of the current time */
	unsigned short millitm;	/* the milliseconds portion of current time*/
	short	timezone;	/* the local timezone in minutes west of GMT */
	short	dstflag;	/* TRUE if Daylight Savings Time in effect */
};

extern int ftime(struct timeb *);

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_TIMEB_H */
