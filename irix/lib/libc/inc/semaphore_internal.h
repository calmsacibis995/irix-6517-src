/*************************************************************************
*                                                                        *
*               Copyright (C) 1997 Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ifndef __SEMAPHORE_INTERNAL_H__
#define __SEMAPHORE_INTERNAL_H__

/*
 * Library internal interface definition file for POSIX 1003.1b semaphores
 */

#include <sys/types.h>
#include <fcntl.h>
#include <ulocks.h>

typedef struct sem_s {
	/*
	 * sem_value must be the first field for unnamed semaphore
	 * atomic operations to work correctly.
	 */
	int		sem_value;
	ushort		sem_rlevel;
	ushort		sem_flags;
	union {
		struct sem_trace_s	*sem_xtrace;	
		__psunsigned_t		sem_xustate;
		__uint64_t		align;
	} sem_x;

	/*
	 * Extended Arena semaphore support fields.
	 *
	 * For binary compatibility reasons, POSIX semaphores can only
	 * reference the fields below under special circumstances
	 * (see sem_spins usage).
	 */

	unsigned	sem_spins;
	int		sem_dev;
	int		sem_opid;
	int		sem_otid;
  	int		sem_refs;
  	int		sem_reserved1;

	union {
		usptr_t		*sem_usptr;
		__uint64_t	align;
	} sem_x2;

	union {
		short		*sem_pfd;
		__uint64_t	align;
	} sem_x3;

	__uint64_t		sem_reserved2;
} sem_t;
 
#define sem_nfd			sem_value
#define sem_xustate		sem_x.sem_xustate
#define sem_xtrace  		sem_x.sem_xtrace
#define sem_usptr  		sem_x2.sem_usptr
#define sem_pfd  		sem_x3.sem_pfd

/*
 * Semaphore flags
 */
#define SEM_FLAGS_UNNAMED	0x0001
#define SEM_FLAGS_NAMED		0x0002
#define SEM_FLAGS_NOSHARE	0x0004
#define SEM_FLAGS_SPINNER	0x0008
#define SEM_FLAGS_ARENA		0x0010
#define SEM_FLAGS_POLLED	0x0020
#define SEM_FLAGS_RECURSE	0x0040
#define SEM_FLAGS_METER		0x0080
#define SEM_FLAGS_DEBUG		0x0100
#define SEM_FLAGS_HISTORY	0x0200
#define SEM_FLAGS_PRIO		0x0400
#define SEM_FLAGS_UNLINKED	0x0800
#define SEM_FLAGS_NOCNCL	0x1000

/*
 * Semaphore limits
 */
#define SEM_SGI_VALUE_MAX	2147483647
#define SEM_SGI_NSEMS_MAX	16384

/*
 * Include usr version of semaphore.h for prototypes
 */
#define _SEM_SGI_INTERNAL
#include <semaphore.h>

#endif /* __SEMAPHORE_INTERNAL_H__ */
