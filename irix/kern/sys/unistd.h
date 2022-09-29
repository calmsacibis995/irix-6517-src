/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* WARNING: This is an implementation-specific header,
** its contents are not guaranteed. Applications
** should include <unistd.h> and not this header.
*/

#ifndef _SYS_UNISTD_H
#define _SYS_UNISTD_H


/* command names for POSIX sysconf */

#define _SC_ARG_MAX             1
#define _SC_CHILD_MAX           2
#define _SC_CLK_TCK             3
#define _SC_NGROUPS_MAX         4
#define _SC_OPEN_MAX            5
#define _SC_JOB_CONTROL         6
#define _SC_SAVED_IDS           7
#define _SC_VERSION             8
#define _SC_PASS_MAX		9
#define _SC_LOGNAME_MAX		10
#define _SC_PAGESIZE		11
#define _SC_PAGE_SIZE		_SC_PAGESIZE
#define _SC_XOPEN_VERSION 	12
#define _SC_NACLS_MAX   13
#define _SC_NPROC_CONF  14
#define _SC_NPROC_ONLN  15
#define _SC_STREAM_MAX  16
#define _SC_TZNAME_MAX  17
#define _SC_RTSIG_MAX		20
#define _SC_SIGQUEUE_MAX	21
#define _SC_REALTIME_SIGNALS	23
#define _SC_PRIORITIZED_IO	24
#define _SC_ACL			25
#define _SC_AUDIT		26	
#define _SC_INF			27
#define _SC_MAC			28
#define _SC_CAP			29
#define _SC_IP_SECOPTS		30
#define	_SC_KERN_POINTERS	31
#define _SC_DELAYTIMER_MAX	32
#define _SC_MQ_OPEN_MAX		33
#define _SC_MQ_PRIO_MAX		34
#define _SC_SEM_NSEMS_MAX	35
#define _SC_SEM_VALUE_MAX	36
#define _SC_TIMER_MAX		37
#define _SC_FSYNC		38
#define _SC_MAPPED_FILES	39
#define _SC_MEMLOCK		40
#define _SC_MEMLOCK_RANGE	41
#define _SC_MEMORY_PROTECTION	42
#define _SC_MESSAGE_PASSING	43
#define _SC_PRIORITY_SCHEDULING 44
#define _SC_SEMAPHORES		45
#define _SC_SHARED_MEMORY_OBJECTS 46
#define _SC_SYNCHRONIZED_IO	47
#define _SC_TIMERS		48
/* The following are numbered out of order due to mips ABI */
#define _SC_ASYNCHRONOUS_IO	64	/* is POSIX AIO supported */
#define _SC_ABI_ASYNCHRONOUS_IO	65	/* is ABI AIO supported */
#define _SC_AIO_LISTIO_MAX	66	/* max items in single listio call */
#define _SC_AIO_MAX		67	/* total outstanding AIO operations */
#define _SC_AIO_PRIO_DELTA_MAX	68
/* start additions for X/Open XPG4 compliance */
#define	_SC_XOPEN_SHM		75
#define	_SC_XOPEN_CRYPT		76
#define	_SC_BC_BASE_MAX		77
#define	_SC_BC_DIM_MAX		78
#define	_SC_BC_SCALE_MAX	79
#define	_SC_BC_STRING_MAX	80
#define	_SC_COLL_WEIGHTS_MAX	81
#define	_SC_EXPR_NEST_MAX	82
#define	_SC_LINE_MAX		83
#define	_SC_RE_DUP_MAX		84
#define	_SC_2_C_BIND		85
#define	_SC_2_C_DEV		86
#define	_SC_2_C_VERSION		87
#define	_SC_2_FORT_DEV		88
#define	_SC_2_FORT_RUN		89
#define	_SC_2_LOCALEDEF		90
#define	_SC_2_SW_DEV		91
#define	_SC_2_UPE		92
#define	_SC_2_VERSION		93
#define	_SC_2_CHAR_TERM		94
#define	_SC_XOPEN_ENH_I18N	95
#define	_SC_IOV_MAX		96
#define	_SC_ATEXIT_MAX		97
#define	_SC_XOPEN_UNIX		98
#define	_SC_XOPEN_XCU_VERSION	99
/* end additions for X/Open XPG4 compliance */
/* additions from POSIX1C - pthreads */
#define _SC_GETGR_R_SIZE_MAX	100
#define _SC_GETPW_R_SIZE_MAX	101
#define _SC_LOGIN_NAME_MAX	102
#define _SC_THREAD_DESTRUCTOR_ITERATIONS	103
#define _SC_THREAD_KEYS_MAX	104
#define _SC_THREAD_STACK_MIN	105
#define _SC_THREAD_THREADS_MAX	106
#define _SC_TTY_NAME_MAX	107
#define _SC_THREADS		108
#define _SC_THREAD_ATTR_STACKADDR	109
#define _SC_THREAD_ATTR_STACKSIZE	110
#define _SC_THREAD_PRIORITY_SCHEDULING	111
#define _SC_THREAD_PRIO_INHERIT	112
#define _SC_THREAD_PRIO_PROTECT	113
#define _SC_THREAD_PROCESS_SHARED	114
#define _SC_THREAD_SAFE_FUNCTIONS	115
/* end POSIX1c additions */
#define _SC_KERN_SIM			116
#define _SC_MMAP_FIXED_ALIGNMENT	117
#define _SC_SOFTPOWER			118
/* additions for X/Open XPG5 compliance */
#define	_SC_XBS5_ILP32_OFF32		119
#define	_SC_XBS5_ILP32_OFFBIG		120
#define	_SC_XBS5_LP64_OFF64		121
#define	_SC_XBS5_LPBIG_OFFBIG		122
#define	_SC_XOPEN_LEGACY		123
#define	_SC_XOPEN_REALTIME		124
/* end XPG5 additions */

/* command names for POSIX pathconf */

#define _PC_LINK_MAX            1	/* maximum number of links */
#define _PC_MAX_CANON           2	/* maximum bytes in a line */
					/* for canoical processing */
#define _PC_MAX_INPUT           3	/* maximum bytes stored in */
					/* the input queue */
#define _PC_NAME_MAX            4	/* longest component name */
#define _PC_PATH_MAX            5	/* longest path name */
#define _PC_PIPE_BUF            6	/* maximum number of bytes that can */
					/* be written atomiclly to a pipe */
#define _PC_CHOWN_RESTRICTED    7	/* chown restricted */
#define _PC_NO_TRUNC            8	/* truncation enabled */
#define _PC_VDISABLE            9	/* spec. character functions disabled */
#define _PC_SYNC_IO		10 	/* synchronous I/O */
#define _PC_PRIO_IO		11	/* priority I/O */
/* The following are numbered out of order due to mips ABI */
#define _PC_ASYNC_IO		64	/* Can this file do POSIX AIO? */
#define _PC_ABI_ASYNC_IO	65	/* Can this file do ABI AIO */
#define _PC_ABI_AIO_XFER_MAX	66	/* biggest ABI AIO xfer for this file */
#if _LFAPI || _XOPEN5
#define _PC_FILESIZEBITS	67	/* num bits for max file size */
#endif

/*
 * Warning: keep in sync with <sys/param.h>, <unistd.h>
 */
#ifndef _POSIX_VERSION
#define _POSIX_VERSION	199506L
#endif

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0 /* Disable special character functions */
#endif

#ifndef _XOPEN_VERSION
#define _XOPEN_VERSION 4
#endif
#ifndef  _POSIX_ASYNCHRONOUS_IO
#define  _POSIX_ASYNCHRONOUS_IO 1
#endif /*  _POSIX_ASYNCHRONOUS_IO */
#if defined(_KERNEL) && !defined(_STANDALONE)

/*
 * Argument structs for pathconf and fpathconf syscalls, also called by
 * syssgi for Irix 4 ABI compatibility.
 */
struct pathconfa {
	char *fname;
	sysarg_t name;
};

struct fpathconfa {
	sysarg_t fdes;
	sysarg_t name;
};

union rval;
int pathconf(struct pathconfa *, union rval *);
int fpathconf(struct fpathconfa *, union rval *);
int sysconf(int, union rval *);
#endif	/* _KERNEL */

#endif	/* _SYS_UNISTD_H */
