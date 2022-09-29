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

#if defined(EVEREST) || defined(SN0) || defined(IP30)

/*
 * Implementation of sleeping barriers for kernel inter-process
 * synchronization.
 */

/*
 * Data Structures
 * Shown here for convenience; they're defined
 * in <frame_semabarrier.h>
 */


/*
 * typedef struct semabarrier {
 *     int nclients;
 *     int in_counter;
 *     mutex_t lock;
 *     sv_t wait;
 * } semabarrier_t;
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include "frame_semabarrier.h"
#include <sys/pda.h>
#include <sys/idbgentry.h>

#if IP30
#define sb_lock(b) mutex_spinlock_spl(&((b)->lock), splprof)
#else
#define sb_lock(b) mutex_spinlock(&((b)->lock))
#endif

#define sb_unlock(b, s) mutex_spinunlock(&((b)->lock), s)


semabarrier_t*
semabarrier_create(int nclients)
{
        semabarrier_t* semabarrier;

        semabarrier = (semabarrier_t*)
		kmem_zalloc(sizeof(semabarrier_t), KM_CACHEALIGN | KM_SLEEP);
        if (semabarrier == NULL) {
                return (NULL);
        }
        semabarrier->nclients = nclients;
        semabarrier->in_counter = 0;
        spinlock_init(&semabarrier->lock, "SBLCK");
        sv_init(&semabarrier->wait, SV_DEFAULT, "SBSEM");

        return (semabarrier);
}

void
semabarrier_destroy(semabarrier_t* semabarrier)
{
        
#if DEBUG
        int ospl;
        ASSERT (semabarrier != NULL);
        ospl = sb_lock(semabarrier);
        ASSERT (semabarrier->in_counter == 0);
	sb_unlock(semabarrier, ospl);
#endif
     
	ASSERT (semabarrier->in_counter == 0);

        sv_destroy(&semabarrier->wait);
        spinlock_destroy(&semabarrier->lock);
        kmem_free(semabarrier, sizeof(semabarrier_t));
}

int
semabarrier_wait(semabarrier_t* semabarrier, int pri)
{
        int ospl;
        int errorcode;

        ASSERT (semabarrier != 0);
        ospl = sb_lock(semabarrier);
        if (++semabarrier->in_counter < semabarrier->nclients) {
		if ((pri & PMASK) > PZERO) {
			errorcode =  sv_wait_sig(&semabarrier->wait, pri,
                                                 &semabarrier->lock, ospl);
                        if (errorcode < 0) {
				semabarrier->in_counter = 0;
                        }
                        return (errorcode);
		} else {
			sv_wait(&semabarrier->wait, pri,
				&semabarrier->lock, ospl);
			return 0;
		}
        }
        
        /*
         * The last client to join the barrier reaches this point and
         * wakes up every process waiting on the barrier semaphore.
         */
	sv_broadcast(&semabarrier->wait);

        semabarrier->in_counter = 0;

	sb_unlock(semabarrier, ospl);

        return (0);
}

void
semabarrier_dec(semabarrier_t* semabarrier)
{
        int ospl;
        
        ASSERT (semabarrier != NULL);
        
        ospl = sb_lock(semabarrier);
        semabarrier->nclients--;

	ASSERT (semabarrier->nclients >= 0);

        if (semabarrier->in_counter < semabarrier->nclients) {
                /*
                 * The barrier is still not ready to be released.
                 * We just return.
                 */
		sb_unlock(semabarrier, ospl);
                return;
        }

        /*
         * The decrement on the number of clients has brought the
         * barrier to the release state. We have to wake up every thread
         * waiting on the barrier sync variable.
         */
	sv_broadcast(&semabarrier->wait);

        semabarrier->in_counter = 0;
	sb_unlock(semabarrier, ospl);
}       

void
semabarrier_inc(semabarrier_t* semabarrier)
{
        int ospl;
        
        ASSERT (semabarrier != NULL);
        
        ospl = sb_lock(semabarrier);
        semabarrier->nclients++;
	sb_unlock(semabarrier, ospl);
}
                    
int
semabarrier_getclients(semabarrier_t* semabarrier)
{
        int ospl;
        int rval;
        
        ASSERT (semabarrier != NULL);
        
        ospl = sb_lock(semabarrier);
        rval = semabarrier->nclients;
	sb_unlock(semabarrier, ospl);

        return (rval);
}

void
semabarrier_print(semabarrier_t* semabarrier)
{
        ASSERT (semabarrier != NULL);

        printf("Clients: %d, InCounter: %d\n",
               semabarrier->nclients, semabarrier->in_counter);
}

#endif /* defined(EVEREST) || defined(SN0) || defined(IP30) */
