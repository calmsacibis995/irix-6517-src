/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#ifndef	_CORP_H_
#define	_CORP_H_

#include <setjmp.h>
#include <rtc.h>

/*
 * Corp: Coroutines for PROMs
 *
 *	Standard usage of this module is as follows:
 *
 *	void cor1(corp_t *c, __uint64_t arg) { ...; corp_sleep(usec); ... }
 *	void cor2(corp_t *c, __uint64_t arg) { ...; corp_sleep(usec); ... }
 *	void cor3(corp_t *c, __uint64_t arg) { ...; corp_sleep(usec); ... }
 *
 *	__uint64_t	cbuf[CORP_SPACE(2)];	<< Max forked coroutines >>
 *	corp_t	       *c	= (corp_t *) cbuf;
 *	__uint64_t	stk[2][STK_SIZE];
 *
 *	...
 *	corp_init(&c);
 *	corp_fork(&c, cor1, &stk[0][STK_SIZE], arg);
 *	corp_fork(&c, cor2, &stk[1][STK_SIZE], arg);
 *	cor3(c, arg);			<< Optional in-line coroutine >>
 *	corp_wait(&c);
 *	...				<< Optionally fork/wait again >>
 */

#if _LANGUAGE_C

#define CORP_SPACE(n)	((sizeof (corp_t) + (n) * sizeof (corp_proc_t)) / 8)

typedef struct corp_proc_s {		/* Internal Use Only */
    rtc_time_t		wake_time;
    jmp_buf		context;
} corp_proc_t;

typedef struct corp_s {
    int			count;
    corp_proc_t		proc[1];	/* Actual number is max. procs + 1 */
} corp_t;

void	corp_init(corp_t *c);

void	corp_fork(corp_t *c,
		  void (*entry)(corp_t *c, __uint64_t arg),
		  __uint64_t *sp, __uint64_t arg);

void	corp_exit(corp_t *c);

void	corp_sleep(corp_t *c, rtc_time_t usec);

void	corp_wait(corp_t *c);

void	corp_jump(corp_t *c,		/* Internal Use Only */
		  __uint64_t entry, __uint64_t *sp, __uint64_t arg);

#endif /* _LANGUAGE_C */

#endif /* _CORP_H */
