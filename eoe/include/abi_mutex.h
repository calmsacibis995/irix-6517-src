/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _ABI_MUTEX_
#define _ABI_MUTEX_

#ident "$Revision: 1.6 $"

#ifdef __cplusplus
extern "C" {
#endif

#define UNLOCKED 0
#define LOCKED 1

typedef struct abilock {
#if (_MIPS_SZLONG == 32)
	unsigned long abi_lock;
#endif
#if (_MIPS_SZLONG == 64)
	unsigned int abi_lock;
#endif
} abilock_t;

/* ABI mutex functions */
extern int init_lock(abilock_t *);
extern int acquire_lock(abilock_t *);
extern int release_lock(abilock_t *);
extern int stat_lock(abilock_t *);
extern void spin_lock(abilock_t *);

#ifdef __cplusplus
}
#endif
#endif
