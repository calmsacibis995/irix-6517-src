#ifndef __UTMPX_H__
#define __UTMPX_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.14 $"
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


/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 


#include <standards.h>
#include <sys/types.h>

/*
 * from sys/time.h
 */
#ifndef _TIMEVAL_T
#define _TIMEVAL_T
struct timeval {
#if _MIPS_SZLONG == 64
	__int32_t :32;
#endif
        time_t  tv_sec;         /* seconds */
        long    tv_usec;        /* and microseconds */
};

/*
 * Since SVID API explicitly gives timeval struct to contain longs ...
 * Thus need a second timeval struct for 64bit binaries to correctly
 * access specific files (ie. struct utmpx in utmpx.h).
 */
struct __irix5_timeval {
        __int32_t    tv_sec;         /* seconds */
        __int32_t    tv_usec;        /* and microseconds */
};
#endif /* _TIMEVAL_T */

#if _NO_XOPEN4
#include <utmp.h>

#define	UTMPX_FILE	"/var/adm/utmpx"
#define	WTMPX_FILE	"/var/adm/wtmpx"
#endif	/* _NO_XOPEN4 */

#define	ut_name	ut_user
#define ut_xtime ut_tv.tv_sec

#ifndef _EXIT_STATUS_
#define _EXIT_STATUS_
struct __exit_status {
    short __e_termination ;	/* Process termination status */
    short __e_exit ;		/* Process exit status */
};
#if _NO_XOPEN4
#define exit_status __exit_status
#define e_termination __e_termination
#define e_exit __e_exit
#endif
#endif

struct utmpx
  {
	char		ut_user[32];		/* user login name */
	char		ut_id[4]; 		/* inittab id */
	char		ut_line[32];		/* device name (console, lnxx) */
	pid_t		ut_pid;			/* process id */
	short		ut_type; 		/* type of entry */
	struct __exit_status ut_exit;		/* process termination/exit status */
#if (_MIPS_SZLONG == 64)
	struct __irix5_timeval 	ut_tv;		/* time entry was made */
	__int32_t	ut_session;		/* session ID, used for windowing */
	__int32_t	ut_pad[5];		/* reserved for future use */
#else
	struct timeval 	ut_tv;			/* time entry was made */
	long		ut_session;		/* session ID, used for windowing */
	long		ut_pad[5];		/* reserved for future use */
#endif
	short		ut_syslen;		/* significant length of ut_host */
						/*   including terminating null */
	char		ut_host[257];		/* remote host name */
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
#if _ABIAPI || _SGIAPI
#define	ACCOUNTING	9
#define	UTMAXTYPE	ACCOUNTING	/* Largest legal value of ut_type */
#endif /* _ABIAPI || _SGIAPI */

#if _NO_XOPEN4
/*	Special strings or formats used in the "ut_line" field when	*/
/*	accounting for something other than a process.			*/
/*	No string for the ut_line field can be more than 		*/
/*	31 chars + NULL in length.					*/

#define	RUNLVL_MSG	"run-level %c"
#define	BOOT_MSG	"system boot"
#define	OTIME_MSG	"old time"
#define	NTIME_MSG	"new time"
#define MOD_WIN		10
#endif	/* _NO_XOPEN4 */

extern void endutxent(void);
extern struct utmpx *getutxent(void);
extern struct utmpx *getutxid(const struct utmpx *);
extern struct utmpx *getutxline(const struct utmpx *);
extern struct utmpx *pututxline(const struct utmpx *); 
extern void setutxent(void);

#if _NO_XOPEN4

extern int utmpxname(const char *);
extern struct utmpx *makeutx(const struct utmpx *);
extern struct utmpx *modutx(const struct utmpx *);
extern void getutmp(const struct utmpx *, struct utmp *);
extern void getutmpx(const struct utmp *, struct utmpx *);
extern void updwtmp(const char *, struct utmp *);
extern void updwtmpx(const char *, struct utmpx *);

#endif	/* _NO_XOPEN4 */

#ifdef __cplusplus
}
#endif
#endif /* !__UTMPX_H__ */
