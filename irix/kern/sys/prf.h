/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1996, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_PRF_H__
#define __SYS_PRF_H__

#include <sys/types.h>

/*
 * /dev/prf ioctl() commands:
 * 
 * NOTE: /dev/par and /dev/prf share the same major device number.  Since they
 *       share absolutely no code in common this is really annoying and simply
 *       leads to potential problems when one piece of code or another is
 *       changed.  To cover this, we use ioctl() codes 0-63 (0x0-0x3f) for
 *       /dev/prf and 64-1023 (0x40-0x3ff) for /dev/par.
 */
#define PRF_GETSTATUS		0x0001	/* get current profiling status */
#define PRF_GETNSYMS		0x0002	/* get size of profile symbol table */
#define PRF_SETPROFTYPE		0x0003	/* set profiling type */

/* PRF_GETSTAT/PRF_SETPROFTYPE values */
#define PRF_DOMAIN_MASK		0x00ff	/* profile sampling domain */
#define PRF_DOMAIN_NONE		0x0000	/* none */
#define PRF_DOMAIN_TIME		0x0001	/* time */
#define PRF_DOMAIN_DCACHE1	0x0002	/* primary data cache misses */
#define PRF_DOMAIN_DCACHE2	0x0003	/* secondary data cache misses */
#define PRF_DOMAIN_ICACHE1	0x0004	/* primary instruction cache misses */
#define PRF_DOMAIN_ICACHE2	0x0005	/* secondary instruction cache misses */
#define PRF_DOMAIN_SCFAIL	0x0006	/* store-conditional failures */
#define PRF_DOMAIN_CYCLES	0x0007	/* cycle time */
#define PRF_DOMAIN_SWTCH	0x0008	/* context switches */
#define PRF_DOMAIN_IPL		0x0009	/* non-zero interrupt priority level */
#define PRF_DOMAIN_BRMISS	0x000a	/* mispredicted branches */
#define PRF_DOMAIN_UPGCLEAN	0x000b	/* upgrades on clean scache lines */
#define PRF_DOMAIN_UPGSHARED	0x000c	/* upgrades on shared scache lines */

#define PRF_RANGE_MASK		0x0f00	/* profile sampling range */
#define PRF_RANGE_NONE		0x0000	/* none */
#define PRF_RANGE_PC		0x0100	/* PCs */
#define PRF_RANGE_STACK		0x0200	/* callstacks */

#define PRF_VALID_SYMTAB	0x1000	/* profile symbol table is valid */
#define PRF_DEBUG		0x2000	/* profiler being used for debugging */

#define PRF_PROFTYPE_MASK	(PRF_DOMAIN_MASK | PRF_RANGE_MASK)
#define PRF_PROFTYPE_NONE	(PRF_DOMAIN_NONE | PRF_RANGE_NONE)

/*
 * Constants for stack profiling (PRF_RANGE_STACK).  The stack backtrace
 * data is carried out of the kernel via the rtmon tracing facility --
 * potentially in multiple rtmon trace events if the stack backtrace is deep
 * enough.  In order to simplify algorithms and constrain the maximum amount
 * of time a stack backtrace can take, we limit the maximum depth of stack
 * backtraces.  In order to deal with multi-event backtraces and possible
 * lost events due to rtmon trace buffer overflows, we use the 2 free bits
 * in the bottom of the PC addresses contained in the stack backtrace to
 * mark the first and last PCs in the backtrace (potentially the same PC
 * address).  Since we always know when we lose rtmon trace events, we can
 * reliably prevent incomplete and corrupted stack backtraces from being
 * saved.
 */
#define PRF_MAXSTACK	24		/* maximum stack backtrace */
#define PRF_STACKSTART	0x1		/* first PC in stack backtrace */
#define PRF_STACKEND	0x2		/* last PC in stack backtrace */

#if defined(_KERNEL)
extern void prfintr(__psunsigned_t, __psunsigned_t,
		    __psunsigned_t, k_machreg_t);
unsigned int prfstacktrace(__psunsigned_t, __psunsigned_t, __psunsigned_t,
			   __psunsigned_t *, unsigned int);
#endif

#endif /* __SYS_PRF_H__ */
