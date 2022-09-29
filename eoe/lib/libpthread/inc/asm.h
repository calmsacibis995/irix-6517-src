#ifndef _ASM_H_
#define _ASM_H_

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
#define init1		PFX_NAME(init1)
#define pt_crt0		PFX_NAME(pt_crt0)
#define pt_start	PFX_NAME(pt_start)
#define pt_longjmp	PFX_NAME(pt_longjmp)
#define gp_reg		PFX_NAME(gp_reg)
#define cmp_and_swap	PFX_NAME(cmp_and_swap)
#define cmp0_and_swap	PFX_NAME(cmp0_and_swap)
#define add_if_less	PFX_NAME(add_if_less)
#define add_if_greater	PFX_NAME(add_if_greater)
#define add_if_equal	PFX_NAME(add_if_equal)
#define atomic_unlock	PFX_NAME(atomic_unlock)
#define ref_if_same	PFX_NAME(ref_if_same)
#define unref_and_test	PFX_NAME(unref_and_test)


#if !defined(LANGUAGE_ASSEMBLY)

#include <stddef.h>
#include <sys/types.h>

struct slock;
struct pt;

/*
 * Initialization routine.  Called before main().
 */
extern void	init1(void);

/*
 * pt_crt0() is the first function run by a new pthread -- it calls
 * pt_start().
 */
extern void	pt_crt0(void);
extern void	pt_start(struct pt *, void *(*)(void *), void *);

/*
 * Do a fast jump to another stack.
 */
extern void	pt_longjmp(struct pt *, void (*)(struct pt *), void *);

/*
 * Fetch current value of gp register.
 */
extern long	gp_reg(void);

/*
 * Conditional compare and swap.
 */
extern int	cmp_and_swap(void *, void *, void *);
extern int	cmp0_and_swap(void *, void *);

/*
 * Used to atomically compare and increment/decrement global variables.
 */
extern int 	add_if_less(int *, __uint32_t, __uint32_t);
extern int 	add_if_greater(int *, __uint32_t, __uint32_t);
extern int 	add_if_equal(int *, __uint32_t, __uint32_t);
extern int	atomic_unlock(int *, __uint32_t, __uint32_t, __uint32_t);

/*
 * Used by pt_ref/pt_unref to handle reference counts and generation numbers.
 */
extern int	ref_if_same(__uint32_t *, unsigned short);
extern int	unref_and_test(__uint32_t *);

#endif /* !LANGUAGE_ASSEMBLY */

#endif /* !_ASM_H_ */
