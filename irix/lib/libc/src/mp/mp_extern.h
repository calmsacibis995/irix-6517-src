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
#ifndef __MP_EXTERN_H__
#define __MP_EXTERN_H__

#include <semaphore_internal.h>
#include <sync.h>
#include <sgidefs.h>
#include <ulocks.h>
#include "us.h"

/* libmutexc.c */
extern int      __havellsc(void);     /* make sure have working ll/sc */

/* libmutexc.s */
extern unsigned long    _mips2_test_and_set();
extern unsigned long    _mips2_test_then_and();
extern unsigned long    _mips2_test_then_nand();
extern unsigned long    _mips2_test_then_not();
extern unsigned long    _mips2_test_then_nor();
extern unsigned long    _mips2_test_then_xor();
extern unsigned long    _mips2_test_then_or();
extern unsigned long    _mips2_test_then_add();
extern unsigned long    _mips2_add_then_test();

extern __uint32_t    _mips2_test_and_set32();
extern __uint32_t    _mips2_test_then_and32();
extern __uint32_t    _mips2_test_then_nand32();
extern __uint32_t    _mips2_test_then_not32();
extern __uint32_t    _mips2_test_then_nor32();
extern __uint32_t    _mips2_test_then_xor32();
extern __uint32_t    _mips2_test_then_or32();
extern __uint32_t    _mips2_test_then_add32();
extern __uint32_t    _mips2_add_then_test32();

/* r4k.s */
extern int _r4k_dbg_spin_lock(spinlock_t *);
extern int _r4k_dbg_spin_trylock(spinlock_t *);
extern int _r4k_dbg_spin_rewind(spinlock_t *);
extern int _r4k_dbg_spin_unlock(spinlock_t *);

/* r4k_sem.s */
extern int _r4k_sem_post(sem_t *);
extern int _r4k_sem_wait(sem_t *, int);
extern int _r4k_sem_trywait(sem_t *);
extern void _r4k_sem_rewind(sem_t *);

/* taskinit.c */
extern int _taskinit(int);

/* uninit.c */
extern int _us_systype;

/* usconfig.c */
extern size_t _us_mapsize;
extern int _us_maxusers;
extern int _us_locktype;
extern int _us_arenatype;
extern unsigned _us_autogrow;
extern unsigned _us_autoresv;
extern void *_us_attachaddr;

#endif
