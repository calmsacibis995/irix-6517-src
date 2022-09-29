#ifndef __UTMP_H__
#define __UTMP_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.16 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <sys/types.h>

#define	UTMP_FILE	"/var/adm/utmp"
#define	WTMP_FILE	"/var/adm/wtmp"
#define	ut_name	ut_user

/*
 * Pulled out of struct utmp
 */
#ifndef _EXIT_STATUS_
#define _EXIT_STATUS_
struct __exit_status {
    short __e_termination ;	/* Process termination status */
    short __e_exit ;		/* Process exit status */
};
#define exit_status	__exit_status
#define e_termination __e_termination
#define e_exit __e_exit
#endif

struct utmp
  {
	char ut_user[8] ;		/* User login name */
	char ut_id[4] ; 		/* /etc/inittab id(usually line #) */
	char ut_line[12] ;		/* device name (console, lnxx) */
	short ut_pid ;			/* leave short for compatiblity - process id */
	short ut_type ; 		/* type of entry */
	struct __exit_status ut_exit ;	/* The exit status of a process
					 * marked as DEAD_PROCESS.
					 */
	time_t ut_time ;		/* time entry was made */
  } ;

/*	Definitions for ut_type						*/

#define	EMPTY		0
#define	RUN_LVL		1
#define	BOOT_TIME	2
#define	OLD_TIME	3
#define	NEW_TIME	4
#define	INIT_PROCESS	5	/* Process spawned by "init" */
#define	LOGIN_PROCESS	6	/* A "getty" process waiting for login */
#define	USER_PROCESS	7	/* A user process */
#define	DEAD_PROCESS	8
#define	ACCOUNTING	9

#define	UTMAXTYPE	ACCOUNTING	/* Largest legal value of ut_type */

/*	Special strings or formats used in the "ut_line" field when	*/
/*	accounting for something other than a process.			*/
/*	No string for the ut_line field can be more than 11 chars +	*/
/*	a NULL in length.						*/

#define	RUNLVL_MSG	"run-level %c"
#define	BOOT_MSG	"system boot"
#define	OTIME_MSG	"old time"
#define	NTIME_MSG	"new time"

extern void endutent(void);
extern struct utmp *getutent(void);
extern struct utmp *getutid(const struct utmp *);
extern struct utmp *getutline(const struct utmp *);
extern struct utmp *pututline(const struct utmp *); 
extern void setutent(void);
extern int utmpname(const char *);

#ifdef __cplusplus
}
#endif
#endif /* !__UTMP_H__ */
