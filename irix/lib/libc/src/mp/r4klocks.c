/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.9 $ $Author: joecd $"

#include "synonyms.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <ulocks.h>
#include "shlib.h"
#include "us.h"

/*
 * _usr4klocks_init: Routes the Arena spin lock functions to the POSIX routines
 */
/* ARGSUSED */
 void
_usr4klocks_init(int locktype, int systype)
{
	extern int posix_spin_lock(ulock_t);
	extern int spin_unlock(ulock_t);
	extern int spin_lockc(ulock_t, unsigned int spins);
	extern int spin_lockw(ulock_t, unsigned int spins);
	extern int spin_test(ulock_t);
	extern void spin_destroy(ulock_t, usptr_t *);
	extern int spin_print(ulock_t, FILE *, const char *);
	extern int spin_mode(ulock_t, int cmd, ...);
	extern int _r4k_cas(void *, ptrdiff_t, ptrdiff_t, usptr_t *);
	extern int _r4k_cas32(void *, int32_t, int32_t, usptr_t *);

	_nlock = _spin_arena_alloc;
	_ilock = _spin_arena_init;

	_freelock = spin_destroy;
	_dlock = spin_print;
	_ctlock = spin_mode;

	_lock  = posix_spin_lock;
	_ulock = spin_unlock;
	_clock = spin_lockc;
	_wlock = spin_lockw;
	_tlock = spin_test;

	_cas = _r4k_cas;
	_cas32 = _r4k_cas32;
}
