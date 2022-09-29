/**************************************************************************
 *									  *
 * Copyright (C) 1986-1993 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __MUTEX_H__
#define __MUTEX_H__

#ident "$Revision: 1.3 $"

#include <sgidefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	test_and_set	(*__test_and_set)
#define	test_then_and	(*__test_then_and)
#define	test_then_nand	(*__test_then_nand)
#define	test_then_not	(*__test_then_not)
#define	test_then_nor	(*__test_then_nor)
#define	test_then_xor	(*__test_then_xor)
#define	test_then_or	(*__test_then_or)
#define	test_then_add	(*__test_then_add)
#define	add_then_test	(*__add_then_test)
#define	atomic_set	(void)(*__test_and_set)

#define	test_and_set32	(*__test_and_set32)
#define	test_then_and32	(*__test_then_and32)
#define	test_then_nand32	(*__test_then_nand32)
#define	test_then_not32	(*__test_then_not32)
#define	test_then_nor32	(*__test_then_nor32)
#define	test_then_xor32	(*__test_then_xor32)
#define	test_then_or32	(*__test_then_or32)
#define	test_then_add32	(*__test_then_add32)
#define	add_then_test32	(*__add_then_test32)
#define	atomic_set32	(void)(*__test_and_set32)

extern unsigned long test_and_set(unsigned long *, unsigned long);
extern unsigned long test_then_and(unsigned long *, unsigned long);
extern unsigned long test_then_nand(unsigned long *, unsigned long);
extern unsigned long test_then_not(unsigned long *, unsigned long);
extern unsigned long test_then_nor(unsigned long *, unsigned long);
extern unsigned long test_then_xor(unsigned long *, unsigned long);
extern unsigned long test_then_or(unsigned long *, unsigned long);
extern unsigned long test_then_add(unsigned long *, unsigned long);
extern unsigned long add_then_test(unsigned long *, unsigned long);
extern unsigned long atomic_op(
		unsigned long (*)(unsigned long *, unsigned long),
		unsigned long *, unsigned long);

extern __uint32_t test_and_set32(__uint32_t *, __uint32_t);
extern __uint32_t test_then_and32(__uint32_t *, __uint32_t);
extern __uint32_t test_then_nand32(__uint32_t *, __uint32_t);
extern __uint32_t test_then_not32(__uint32_t *, __uint32_t);
extern __uint32_t test_then_nor32(__uint32_t *, __uint32_t);
extern __uint32_t test_then_xor32(__uint32_t *, __uint32_t);
extern __uint32_t test_then_or32(__uint32_t *, __uint32_t);
extern __uint32_t test_then_add32(__uint32_t *, __uint32_t);
extern __uint32_t add_then_test32(__uint32_t *, __uint32_t);
extern __uint32_t atomic_op32(
		__uint32_t (*)(__uint32_t *, __uint32_t),
		__uint32_t *, __uint32_t);

/* ABI version */
extern int _test_and_set(int *, int);

extern int is_mips2(void);

#ifdef __cplusplus
}
#endif

#endif
