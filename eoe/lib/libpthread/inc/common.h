#ifndef _COMMON_H_
#define _COMMON_H_

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* Global names
 */
#define dbg_printf	PFX_NAME(dbg_printf)
#define dbg_trace	PFX_NAME(dbg_trace)
#define panic		PFX_NAME(panic)
#define sig_abort	PFX_NAME(sig_abort)


/*
 * Always included first.
 */

#define TRUE	1
#define FALSE	0

#if !defined(LANGUAGE_ASSEMBLY)

#if defined(_PTHREAD_EXECUTIVE)
typedef struct ptattr		pthread_attr_t;
typedef struct mtx		pthread_mutex_t;
typedef struct mtxattr		pthread_mutexattr_t;
typedef struct cv		pthread_cond_t;
typedef struct cvattr		pthread_condattr_t;
typedef struct rwl		pthread_rwlock_t;
typedef struct rwlattr		pthread_rwlockattr_t;
typedef int			pthread_key_t;
typedef volatile int		pthread_once_t;
#endif

typedef unsigned char schedpri_t;
typedef unsigned char schedpolicy_t;

/* Name space pollution prefix */
#define PFX_NAME(name)	_SGIPT_ ## name

#define MACRO_BEGIN	{
#define MACRO_END	}

extern void	panic(char *, char *);
extern void	sig_abort(void);

/*
 * We want to be able to pass pointer-sized values as the return value from
 * longjmp().  The code in libc does the right thing, but the prototypes in
 * setjmp.h don't reflect that.  Through a bit of hackery, we redefine
 * setjmp() and longjmp().
 */
#undef longjmp
#undef setjmp
#define longjmp		__pt_longjmp
#define setjmp		__pt_setjmp

#include <setjmp.h>

#undef longjmp
#undef setjmp

extern __psunsigned_t 	setjmp(jmp_buf);
extern void		longjmp(jmp_buf, __psunsigned_t);

#include <sys/types.h>
extern void	*_malloc(size_t);	/* libc protos */
extern void	_free(void*);


#ifdef DEBUG

#define T_VP	0x0001
#define T_PT	0x0002
#define T_MTX	0x0004
#define T_CV	0x0008
#define T_CNCL	0x0010
#define T_INTR	0x0020
#define T_EVT	0x0040
#define T_DLY	0x0080
#define T_INH	0x0100
#define T_SEM	0x0200
#define T_MISC	0x0400
#define T_RWL	0x0800
#define T_LIBC	0x1000
#define T_DBG	0x10000000

extern void	dbg_printf(char *, ...);
extern long	dbg_trace;

#define TRACE(type, args) \
	if (dbg_trace & type)	\
		dbg_printf args

/* Avoid libc assert() which uses stdio via abort().
 */
#include <signal.h>
#define ASSERT(EX)	\
	((EX)		\
		? ((void)0)	\
		: (void)(dbg_printf("Assertion failed: " #EX ", file "	\
				    __FILE__ ", line %d", __LINE__),	\
			 sig_abort()))


#else	/* DEBUG */

#define TRACE(lvl, args)

#define ASSERT(EX)	((void)0)

#endif	/* DEBUG */


#ifdef STATS

/* Statistics gathering support
 */
enum {	/* Add new symbols before STAT_LAST
	 * Don't forget to update stat_names[]
	 */
	STAT_BLOCK_NEW,
	STAT_BLOCK_READY,
	STAT_BLOCK_PREEMPTING,
	STAT_IDLE_PAUSE,
	STAT_IDLE,
	STAT_RESUME_NEW,
	STAT_RESUME_SAME,
	STAT_RESUME_SWAP,
	STAT_MTX_SLOW,
	STAT_SPARE0,
	STAT_SPARE1,
	STAT_SPARE2,
	STAT_SPARE3,
	STAT_SPARE4,
	STAT_LAST
};

extern char	*stat_names[];

extern int	stat_counts[];

#define STAT_INCR(name)	stat_counts[name]++
#define STAT_DECR(name)	stat_counts[name]--

#else	/* STATS */
#define STAT_INCR(name)
#define STAT_DECR(name)
#endif	/* STATS */


#else	/* !LANGUAGE_ASSEMBLY */

/* Name space pollution prefix - stupid assembler version */
#define PFX_NAME(name)	_SGIPT_/**/name

#endif	/* !LANGUAGE_ASSEMBLY */

#if !defined(_RLD_PTHREADS_START)
#define _RLD_PTHREADS_START	12
#endif

#endif /* !_COMMON_H_ */
