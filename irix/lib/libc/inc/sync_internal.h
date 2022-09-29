/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYNC_INTERNAL_H__
#define __SYNC_INTERNAL_H__

/*
 * Library internal interface definition file for spinlocks
 */

#if (defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS))

/*
 *  Spin lock structure
 *
 *  NOTE: accessed from ASM - be sure to change ASM offsets below
 */
typedef struct spinlock_s {

	unsigned int		spin_members;
	unsigned int		spin_reserved1;

	unsigned int		spin_flags;
	unsigned int		spin_reserved2;

	union {
		struct spinlock_trace_s	*trace;
		__uint64_t		align;
	} spin_tp;

	union {
 		void		*usptr;
		__uint64_t	align;
	} spin_up;

  	__uint64_t		spin_reserved3[2];
} spinlock_t;

#define spin_usptr spin_up.usptr
#define spin_trace spin_tp.trace
#define spin_opid spin_trace->st_opid
#define spin_otid spin_trace->st_otid

#define _SPIN_SGI_INTERNAL
#include <sync.h>

#endif /* (_LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS) */

/*
 * Spin lock flags
 */
#define SPIN_FLAGS_ARENA 0x0001
#define SPIN_FLAGS_POSIX 0x0002
#define SPIN_FLAGS_PRIO  0x0004
#define SPIN_FLAGS_UP    0x0008
#define SPIN_FLAGS_METER 0x0010
#define SPIN_FLAGS_DEBUG 0x0020
#define SPIN_FLAGS_DPLUS 0x0040

#if defined(_LANGUAGE_ASSEMBLY)
#define SPIN_FLAGS	8	/* flags field offset */
#endif /* (_LANGUAGE_ASSEMBLY) */

#endif /* __SYNC_INTERNAL_H__ */
