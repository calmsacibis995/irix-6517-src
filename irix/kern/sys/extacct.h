/*
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

#ifndef	__SYS_EXTACCT_H
#define	__SYS_EXTACCT_H
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.10 $"

#include <sys/types.h>

/*
 * Structures and other useful information about SGI extended accounting
 *
 * NOTE: the structures for the actual extended accounting records (both
 *       process and session) are contained in sat.h.
 */

#define PROC_ACCT_VERSION  1
#define SESS_ACCT_VERSION  1	/* Could be 2 with proper arsctl */

/*
 * Accounting timers. These are all in units of nanoseconds.
 */
typedef struct acct_timers {	/* Accounting times */
	accum_t	 ac_utime;		/* User time */
	accum_t	 ac_stime;		/* System time */
	accum_t	 ac_bwtime;		/* Time waiting for block I/O */
	accum_t	 ac_rwtime;		/* Time waiting for raw I/O */
	accum_t	 ac_qwtime;		/* Time waiting on run queue */
	accum_t	 ac_spare[3];		/*   reserved */
} acct_timers_t;

/*
 * Accounting counters
 */
typedef struct acct_counts {	/* Accounting counts */
	accum_t	 ac_mem;		/* "Memory usage" (weird acct units) */
	accum_t  ac_swaps;		/* # swaps */
	accum_t  ac_chr;		/* # bytes read */
	accum_t  ac_chw;		/* # bytes written */
	accum_t  ac_br;			/* # blocks read */
	accum_t  ac_bw;			/* # blocks written */
	accum_t  ac_syscr;		/* # read syscalls */
	accum_t  ac_syscw;		/* # write syscalls */
	accum_t  ac_disk;		/* not implemented */
	accum_t  ac_spare[3];		/*   reserved */
} acct_counts_t;


/*
 * Format 1 Service Provider Information. Strings should be null-terminated.
 */
typedef struct acct_spi {	/* Service provider information (format 1) */
	char	spi_company[8];		/* Name of company providing service */
	char	spi_initiator[8];	/* Name of machine initiating job */
	char	spi_origin[16];		/* Name of queue */
	char	spi_spi[16];		/* Available for service provider */
	char	spi_spare[16];		/*   reserved */
} acct_spi_t;

/*
 * Format 2 Service Provider Information. Strings should be null-terminated.
 */
typedef struct acct_spi_2 {	/* Service provider information (format 2) */
	char	spi_company[8];		/* Name of company providing service */
	char	spi_initiator[8];	/* Name of machine initiating job */
	char	spi_origin[16];		/* Name of queue */
	char	spi_spi[16];		/* Available for service provider */
	char	spi_spare[16];		/*   reserved */
	char	spi_jobname[32];	/* Job name */
	int64_t spi_subtime;		/* Queue submission time */
	int64_t spi_exectime;		/* Queue execution time */
	int64_t spi_waittime;		/* Queue wait time */
	int64_t spi_rsrvd;		/*   reserved */
} acct_spi_2_t;



/*
 * Shadow accounting data: though uncommon, it is possible for a process
 * or array session to continue running even after an accounting record
 * has been written for it. Typically, this would occur when the process
 * or array session has been restored after a checkpoint. This shadow
 * accounting data is a copy of the accounting timers & counter at the
 * time the process/arsess was restored. They will be deducted from any
 * future accounting record(s) produced for this process/arsess.
 *	We don't just deduct this information directly from the counters
 * and timers in proc because we want things like ps or /proc to show the
 * total accumulated times/counts, not just the values since restore.
 */

typedef struct shacct {
	int		sha_type;	/* Type of acct info */
	int		sha_rsrvd[3];	/*   reserved */
	accum_t		sha_ioch;	/* p_ioch */
	acct_timers_t	sha_timers;	/* see above */
	acct_counts_t	sha_counts;
} shacct_t;

/* sha_type values */
#define SHATYPE_PROC	1	/* Process acct info */
#define SHATYPE_SESS	2	/* Array session acct info */


#if defined(_KERNEL) || defined(_KMEMUSER)

struct proc;
extern void shadowacct(struct proc *, int);

#endif /* _KERNEL  ||  _KMEMUSER */


#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _SYS_EXTACCT_H */
