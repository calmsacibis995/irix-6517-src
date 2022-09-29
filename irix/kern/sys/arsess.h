/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_ARSESS_H__
#define __SYS_ARSESS_H__

#ident "$Revision: 1.18 $"

/*
 * Array Sessions
 *
 * An array session is a group of processes all related to each other by a
 * single unique identifier, the "array session handle". The processes
 * don't necessarily have to belong to the same parent-child chain, and
 * don't even have to be running on the same system. However, the default
 * is for a child process to inherit the array session handle of its
 * parent, so in the average case the processes in an array session are
 * parents/siblings/children of each other and reside on the same system.
 *
 * The goal of an array session is to correlate all the processes that
 * belong conceptually to the same login session or batch job, even if
 * those processes are running on several separate machines in an array.
 * Then, with the help of external software, the array session can
 * potentially be treated as a single unit for the purposes of accounting,
 * checkpoint/restart, job control, etc.
 *
 * A process can create a new array session and place itself in it by using
 * the "newarraysess" call. Ordinarily, this would only be done programs
 * like login or rshd, which are effectively logging a user into the
 * system, and programs like batch queue systems or crond, which are
 * running jobs on behalf of another user. A new array session is assigned
 * an array session handle that is unique to the current system only. If
 * the process is actually part of an existing array session on another
 * system (e.g. the process was spawned by mpirun or whatever) it can
 * change the handle of its array session to that of its "parent" on the
 * remote system using the "setash" call. The parent's array session handle
 * would presumably be unique across the entire array, although it would be
 * the parent's responsibility to arrange for that. The range of values
 * that the kernel will assign as array session handles is configurable so
 * that it would be possible to design a server to provide array-unique
 * handles; that is left as an exercise to the reader.
 *
 * An array session is *not* the same thing as a POSIX session, though the
 * two concepts are similar (not identical) on a single system. The most
 * important characteristic of a POSIX session is that all of the processes
 * included within it share a single controlling terminal. That distinction
 * does not exist for array sessions: processes from two different array
 * sessions might both be using the same controlling terminal (e.g. by way
 * of the newproj command) or contrarily two processes in the same array
 * session might have two different controlling terminals (e.g. when
 * running on different systems). However, in the simple case of a local
 * login with no remote interactions an array session and a POSIX session
 * would include the same set of processes.
 * 
 * An array session is now built on top of a process aggregate. A process
 * aggregate is a collection of processes. (Please refer to ksys/vpag.h)
 */

#include <sys/types.h>
#include <sys/extacct.h>
#include <sys/sema.h>


#define ASH_NONE	0	/* "No array session handle assigned" */
#define PRID_NONE	0	/* "No project ID assigned" */

#define MAX_SPI_LEN	1024	/* Maximum length of Service Provider Info */

/* Array session libc functions */
int arsctl(int, void *, int);
int arsop(int, ash_t, void *, int);

/* arsctl function codes */
#define ARSCTL_NOP		0	/* No operation */
#define ARSCTL_GETSAF		1	/* Get Session Accounting Format */
#define ARSCTL_SETSAF		2	/* Set Session Accounting Format */
#define ARSCTL_GETMACHID	3	/* Get machine ID */
#define ARSCTL_SETMACHID	4	/* Set machine ID */
#define ARSCTL_GETARRAYID	5	/* Get array ID */
#define ARSCTL_SETARRAYID	6	/* Set array ID */
#define ARSCTL_GETASHCTR	7	/* Get ASH counter */
#define ARSCTL_SETASHCTR	8 	/* Set ASH counter */
#define ARSCTL_GETASHINCR	9 	/* Get ASH increment */
#define ARSCTL_SETASHINCR	10	/* Set ASH increment */
#define ARSCTL_GETDFLTSPI	11	/* Get default Service Provider Info */
#define ARSCTL_SETDFLTSPI	12	/* Set default Service Provider Info */
#define ARSCTL_GETDFLTSPILEN	13	/* Get default SPI length */
#define ARSCTL_SETDFLTSPILEN	14	/* Set default SPI length */
#define ARSCTL_ALLOCASH		15	/* Allocate ASH */

/* arsop function codes */
#define ARSOP_NOP		0	/* No operation (validate ASH) */
#define ARSOP_GETSPI		1	/* Get Service Provider Information */
#define ARSOP_SETSPI		2	/* Set Service Provider Information */
#define ARSOP_GETSPILEN		3	/* Get Service Provider Info Length */
#define ARSOP_SETSPILEN		4	/* Set Service Provider Info Length */
#define ARSOP_FLUSHACCT		5	/* Flush accounting data */
#define ARSOP_GETINFO		6	/* Get current array session info */
#define ARSOP_GETCHGD		7	/* Get charged accounting data */
#define ARSOP_RESTRICT_NEW	8	/* Restrict new array sessions */
#define ARSOP_ALLOW_NEW		9	/* Allow new array sessions */


/* Array session info from ARSOP_GETINFO */
typedef struct arsess {
	ash_t		as_handle;	/* array session handle */
	prid_t		as_prid;	/* project ID */
	int		as_refcnt;	/* reference count */
	time_t		as_start;	/* start time (secs since 1970) */
	time_t		as_ticks;	/* lbolt at start */
	pid_t		as_pid;		/* pid that started this session */
	int		as_spilen;	/* length of Service Provider Info */
	ushort_t	as_flag;	/* various flags */
	char		as_nice;	/* initial nice value of as_pid */
	char		as_rsrv1[985];	/*   reserved */

	/* Accounting data */
	char		as_spi[MAX_SPI_LEN];	/* Service Provider Info */
	acct_timers_t	as_timers;	/* accounting timers */
	acct_counts_t	as_counts;	/* accounting counters */
	char		as_rsrv2[1888];	/*   reserved */
} arsess_t;

/* as_flag */
#define AS_NEW_RESTRICTED	0x0001	/* newarraysess() is restricted */
#define	AS_GOT_CHGD_INFO	0x0002	/* info on flushed data available */
#define AS_FLUSHED		0x0004	/* this is flushed data */



#ifdef _KERNEL

struct	vpagg_s;

/* Array session kernel functions */
extern int	arsess_ctl(int, void *, int);
extern int	arsess_enumashs(ash_t *, int);
extern int	arsess_getmachid(void);
extern int	arsess_join(pid_t, ash_t);
extern void	arsess_leave(struct vpagg_s *, pid_t);
extern int	arsess_new(pid_t);
extern int	arsess_op(int, ash_t *, void *, int);
extern int	arsess_setash(ash_t, ash_t);
extern int	arsess_setmachid(int);
extern int	arsess_setprid(ash_t, prid_t);

/* Global variables from master.c */
extern int	spilen;
extern int	sessaf;

/* Other global variables */
extern int	eff_spilen;
extern int	eff_sessaf;
extern char	dfltspi[MAX_SPI_LEN];

#endif	 /* _KERNEL */
#endif   /* _SYS_ARSESS_H */
