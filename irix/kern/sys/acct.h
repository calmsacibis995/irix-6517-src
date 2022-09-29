/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _PROC_ACCT_H	/* wrapper symbol for kernel use */
#define _PROC_ACCT_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

/*#ident	"@(#)uts-3b2:proc/acct.h	1.4"*/
#ident	"$Revision: 3.15 $"
/*
 * Accounting structures
 *
 * NOTE: these structures apply only to the "old" BSD-style process
 *	 accounting. Structures for the newer SGI process and session
 *	 accounting can be found in extacct.h and sat.h.
 */
#include <sys/types.h>
#include <sys/cdefs.h>

typedef	ushort_t comp_t;  /* "floating point" 13-bit fraction,
	     * 3-bit exponent; decode using the formula:
	     *    val_in_Hz = (comp_t & 0x1fff) << (3 * (comp_t >> 13))
	     */
/* Max number representable by a comp_t */
#if _MIPS_SZLONG == 32
#define MAX_COMP_T MAXLONG           /* 0xffffffff */
#endif
#if _MIPS_SZLONG == 64
#define MAX_COMP_T 17177772032L      /* 0x1fff * 8**7 */
#endif

/* SVR4 acct structure */
struct	acct
{
	char	ac_flag;		/* Accounting flag */
	char	ac_stat;		/* Exit status */
	uid_t	ac_uid;			/* Accounting user ID */
	gid_t	ac_gid;			/* Accounting group ID */
	dev_t	ac_tty;			/* control typewriter */
	time_t	ac_btime;		/* Beginning time */
	comp_t	ac_utime;		/* acctng user time in clock ticks */
	comp_t	ac_stime;		/* acctng system time in clock ticks */
	comp_t	ac_etime;		/* acctng elapsed time in clock ticks */
	comp_t	ac_mem;			/* memory usage */
	comp_t	ac_io;			/* chars transferred */
	comp_t	ac_rw;			/* blocks read or written */
	char	ac_comm[8];		/* command name */
};	

/* Account commands will use this header to read SVR3
** accounting data files.
*/

struct	o_acct
{
	char	ac_flag;		/* Accounting flag */
	char	ac_stat;		/* Exit status */
	o_uid_t	ac_uid;			/* Accounting user ID */
	o_gid_t	ac_gid;			/* Accounting group ID */
	o_dev_t	ac_tty;			/* control typewriter */
	time_t	ac_btime;		/* Beginning time */
	comp_t	ac_utime;		/* acctng user time in clock ticks */
	comp_t	ac_stime;		/* acctng system time in clock ticks */
	comp_t	ac_etime;		/* acctng elapsed time in clock ticks */
	comp_t	ac_mem;			/* memory usage */
	comp_t	ac_io;			/* chars transferred */
	comp_t	ac_rw;			/* blocks read or written */
	char	ac_comm[8];		/* command name */
};	

#ifndef _KERNEL
extern int	acct __P((const char *));
#else
struct proc;
extern void	acct(char);
extern void	extacct(struct proc *);

/* Additional flag for proc.p_acflag/acct.ac_flag */
#define ACKPT	04		/* process has been checkpointed */
#endif

#define	AFORK	01		/* has executed fork, but no exec */
#define	ASU	02		/* used privilege */
#define	ACCTF	0300		/* record type: 00 = acct */
#define AEXPND	040		/* expanded acct structure */

#ifdef __cplusplus
}
#endif

#endif	/* _PROC_ACCT_H */
