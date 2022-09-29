/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.12 $"

#ifdef __STDC__
	#pragma weak init_lock = _init_lock
	#pragma weak release_lock = _release_lock
	#pragma weak stat_lock = _stat_lock
	#pragma weak atomic_op = _atomic_op
	#pragma weak atomic_op32 = _atomic_op32
#endif

#include "sgidefs.h"
#include "synonyms.h"
#include "abi_mutex.h"
#include "ulocks.h"
#include "mutex.h"
#include "us.h"
#include "time.h"
#include "stdlib.h"
#include "mp_extern.h"

/*
	Public Interfaces
*/
unsigned long (*__test_and_set)(unsigned long *, unsigned long) = _mips2_test_and_set;
unsigned long (*__test_then_and)(unsigned long *, unsigned long) = _mips2_test_then_and;
unsigned long (*__test_then_nand)(unsigned long *, unsigned long) = _mips2_test_then_nand;
unsigned long (*__test_then_not)(unsigned long *, unsigned long) = _mips2_test_then_not;
unsigned long (*__test_then_nor)(unsigned long *, unsigned long) = _mips2_test_then_nor;
unsigned long (*__test_then_xor)(unsigned long *, unsigned long) = _mips2_test_then_xor;
unsigned long (*__test_then_or)(unsigned long *, unsigned long) = _mips2_test_then_or;
unsigned long (*__test_then_add)(unsigned long *, unsigned long) = _mips2_test_then_add;
unsigned long (*__add_then_test)(unsigned long *, unsigned long) = _mips2_add_then_test;

__uint32_t (*__test_and_set32)(__uint32_t *, __uint32_t) = _mips2_test_and_set32;
__uint32_t (*__test_then_and32)(__uint32_t *, __uint32_t) = _mips2_test_then_and32;
__uint32_t (*__test_then_nand32)(__uint32_t *, __uint32_t) = _mips2_test_then_nand32;
__uint32_t (*__test_then_not32)(__uint32_t *, __uint32_t) = _mips2_test_then_not32;
__uint32_t (*__test_then_nor32)(__uint32_t *, __uint32_t) = _mips2_test_then_nor32;
__uint32_t (*__test_then_xor32)(__uint32_t *, __uint32_t) = _mips2_test_then_xor32;
__uint32_t (*__test_then_or32)(__uint32_t *, __uint32_t) = _mips2_test_then_or32;
__uint32_t (*__test_then_add32)(__uint32_t *, __uint32_t) = _mips2_test_then_add32;
__uint32_t (*__add_then_test32)(__uint32_t *, __uint32_t) = _mips2_add_then_test32;

/*
 * ABI interface for _test_and_set
 */
int
_test_and_set(int *a, int b)
{
#if (_MIPS_SZINT == 32)
	return((int)test_and_set32((__uint32_t *)a, (__uint32_t)b));
#else
BOMB!!! --- write LL_INT & SC_INT in *.s to handle this for general case
#endif
}


unsigned long
atomic_op(unsigned long (*func)(unsigned long *, unsigned long), unsigned long *addr, unsigned long val)
{
	return (*func)(addr, val);
}

__uint32_t
atomic_op32(__uint32_t (*func)(__uint32_t *, __uint32_t), __uint32_t *addr, __uint32_t val)
{
	return (*func)(addr, val);
}


/*
 * ABI mutex functions
 * NOTE: these are BOTH for 32 bit and 64 bit - though the abilock_t
 * struct is always 32 bits.
 */
struct timespec __usnano = { 0, 1000 };

int
init_lock(abilock_t *addr)
{
        addr->abi_lock = UNLOCKED;
	return 0;
}

int
release_lock(abilock_t *addr)
{
        addr->abi_lock = UNLOCKED;
	return 0;
}

int
stat_lock(abilock_t *addr)
{
        int retval;

        if (addr->abi_lock == UNLOCKED)
                retval = UNLOCKED;
        else
                retval = LOCKED;

        return(retval);
}

int
__havellsc(void)
{
	return is_mips2();
}
