/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	__SYS_DEBUG_H__
#define	__SYS_DEBUG_H__

#ident	"$Revision: 3.24 $"

#if defined(DEBUG)
#ifdef lint
#define ASSERT(EX) 	((void)0) /* avoid "constant in conditional" babble */
#define ASSERT_LEVEL(LVL,EX)	((void)0)
#else
#ifdef __ANSI_CPP__
#define ASSERT(EX) ((!doass||(EX))?((void)0):assfail(#EX, __FILE__, __LINE__))
#define ASSERT_LEVEL(LVL,EX)	ASSERT(EX)
#else
#define ASSERT(EX) ((!doass||(EX))?((void)0):assfail("EX", __FILE__, __LINE__))
#define ASSERT_LEVEL(LVL,EX)	ASSERT(EX)
#endif
#endif	/* lint */
#define	METER(x)	x
/* for sgi compatability */
#define	OS_METER	1
extern int doass;		/* dynamically turn off asserts */

#else /* !DEBUG */

#define ASSERT(x)	((void)0)
#define ASSERT_LEVEL(LVL,EX)	(((LVL)>private.p_asslevel)||!(EX))?((void)0):assfail(#EX, __FILE__, __LINE__)
#define	METER(x)
#undef	OS_METER
#endif /* !DEBUG */

extern void assfail(char *, char *, int);
#pragma mips_frequency_hint NEVER assfail

#ifdef _KERNEL

#ifdef MP
#define ASSERT_MP ASSERT
#define ASSERT_NOMIGRATE ASSERT(issplhi(getsr()) || (curthreadp->k_mustrun == cpuid()) || KT_ISKB(curthreadp))
#else
#define ASSERT_MP(x)		((void)0)
#define ASSERT_NOMIGRATE	((void)0)
#endif

#define ASSERT_DEBUG		ASSERT
#define ASSERT_ALWAYS(EX)	((EX)?((void)0):assfail(#EX, __FILE__, __LINE__))
/*
 * Production assertion levels.  These levels are for use with
 * ASSERT_LEVEL.  They have no effect on ASSERT_DEBUG or ASSERT_ALWAYS.
 */
#define ASSERTION_LEVEL_NONE	0	/* Don't use in assertions. */
#define ASSERTION_LEVEL_HIPERF	25	/* Top perf - eliminates most asserts */
#define ASSERTION_LEVEL_INTEG	50	/* Best data integrity - enables most */
#define ASSERTION_LEVEL_DEBUG	75	/* Debug - enables debugging asserts */
#define ASSERTION_LEVEL_ALL	100	/* Enable all assertions */

void	debug_stop_all_cpus(void *);	/* param is "cpumask_t *" */
#endif

#endif /* __SYS_DEBUG_H__ */
