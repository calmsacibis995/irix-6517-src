/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_SEMABARRIER_H__
#define __SYS_SEMABARRIER_H__

#include <sys/sema.h>

/*
 * Implementation of sleeping barriers based
 * on our kernel semaphores.
 */


typedef struct semabarrier {
        int nclients;
        int in_counter;
        lock_t lock;
        sv_t wait;
} semabarrier_t;

extern semabarrier_t* semabarrier_create(int nclients);
extern void semabarrier_destroy(semabarrier_t* semabarrier);
extern int semabarrier_wait(semabarrier_t* semabarrier, int pri);
extern void semabarrier_dec(semabarrier_t* semabarrier);
extern void semabarrier_inc(semabarrier_t* semabarrier);
extern int semabarrier_getclients(semabarrier_t* semabarrier);
extern void semabarrier_print(semabarrier_t* semabarrier);

#endif /* __SYS__SEMABARRIER_H__ */
