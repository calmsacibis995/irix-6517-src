/*****************************************************************************
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

/* 
 * Page frame NUMA descriptor
 */

#ifndef __SYS_PFMS_H__
#define __SYS_PFMS_H__

#ident "$Revision: 1.10 $"

#ifdef	NUMA_BASE
/*
 * Test bit
 */
#define PFMS_TESTBIT              0x80000000  /* test bit */
#define PFMS_TESTBIT_SHFT         31

/*
 * Per page migration thresholds
 */
#define PFMS_MIGR_THRESHOLD        0x7F000000  /* differential migration threshold (%)*/
#define PFMS_MIGR_THRESHOLD_SHFT   24

#define PFMS_FREEZE_THRESHOLD      0x00E00000  /* maximum bounces before freezing (abs) */
#define PFMS_FREEZE_THRESHOLD_SHFT 21

#define PFMS_MELT_THRESHOLD        0x001C0000  /* melting period (abs) */
#define PFMS_MELT_THRESHOLD_SHFT   18

/*
 * Per page migr parameters
 */
#define PFMS_MIGR_ENABLED          0x00020000   /* auto migration is enabled */
#define PFMS_MIGR_ENABLED_SHFT     17

#define PFMS_FREEZE_ENABLED        0x00010000   /* freeze page if bouncing */
#define PFMS_FREEZE_ENABLED_SHFT   16

#define PFMS_MELT_ENABLED          0x00008000   /* if frozen, melt after melting period */
#define PFMS_MELT_ENABLED_SHFT     15

/*
 * Bouncing Control
 */
#define PFMS_FROZEN                0x00004000    /* page frozen due to bouncing */
#define PFMS_FROZEN_SHFT           14
#define PFMS_BOUNCECOUNT           0x00003800    /* migr count or melting period */
#define PFMS_BOUNCECOUNT_SHFT      11             

/*
 * Enqueue on fail
 */
#define PFMS_ENQONFAIL             0x00000400    /* enqueue page for later migration if fail */
#define PFMS_ENQONFAIL_SHFT        10

/*
 * dampening Factor
 */
#define PFMS_MIGRDAMPENFACTOR      0x00000300     /* migration dampening factor */
#define PFMS_MIGRDAMPENFACTOR_SHFT 8

/*
 * Dampening count
 */
#define PFMS_MIGRDAMPENCNT         0x000000C0     /* count number of migr intrs needed to migr*/
#define PFMS_MIGRDAMPENCNT_SHFT    6

/*
 * Dampening enable bit
 */
#define PFMS_MIGRDAMPEN_ENABLED    0x00000020     /* migration dampening enabled */
#define PFMS_MIGRDAMPEN_ENABLED_SHFT 5

/*
 * Refcnt
 */
#define PFMS_MIGRREFCNT_ENABLED      0x00000010     /* enable software extended mem ref counters */
#define PFMS_MIGRREFCNT_ENABLED_SHFT 4

/*
 * PFMS State
 */
#define PFMS_MAPPED                0x00000008    /* page mapped by user */
#define PFMS_MAPPED_SHFT           3

#define PFMS_MIGRED                0x00000004    /* page undergoing migr control */
#define PFMS_MIGRED_SHFT           2

#define PFMS_QUEUED                0x00000002    /* page in migration queue */
#define PFMS_QUEUED_SHFT           1

/*
 * Pfms mutex lock
 */
#define PFMS_LOCKBIT               0x00000001
#define PFMS_LOCKBIT_SHFT          0

/*
 * All pfms bits except the bitlock
 */
#define PFMS_FLAGS                 (~PFMS_LOCKBIT)

/*
 * THe full pfms
 */
#define PFMS_ALL                   0xFFFFFFFF

/*
 * GLobal pfms get/set
 */
#define PFMS_GETV(pfms)           (*(pfms))
#define PFMS_PUTV(pfms, pfmsv)    (*(pfms) = pfmsv)
#define PFMSV_CLR(pfmsv)          ((pfmsv) = 0)
/*
 * Migration parameter accessors
 *     Updates & accesses to these parameters does
 *     not require any locks. They are written
 *     atomically by using ll/sc
 */

/*
 * Migration base threshold
 */
#define PFMS_MGRTHRESHOLD_GET(pfms)       ((*(pfms) & PFMS_MIGR_THRESHOLD) >>  PFMS_MIGR_THRESHOLD_SHFT)

#define PFMS_MGRTHRESHOLD_PUT(pfms, v)    (atomicFieldAssignUint((pfms),              \
                                                                 PFMS_MIGR_THRESHOLD, \
                                                                 ((v) << PFMS_MIGR_THRESHOLD_SHFT) & PFMS_MIGR_THRESHOLD)))

#define PFMSV_MGRTHRESHOLD_GET(pfmsv)     (((pfmsv) & PFMS_MIGR_THRESHOLD) >>  PFMS_MIGR_THRESHOLD_SHFT)

#define PFMSV_MGRTHRESHOLD_PUT(pfmsv, v)  ((pfmsv) &= ~PFMS_MIGR_THRESHOLD, \
                                           (pfmsv) |= ((v) << PFMS_MIGR_THRESHOLD_SHFT) & PFMS_MIGR_THRESHOLD)


/*
 * Freeze threshold
 */
#define PFMS_FRZTHRESHOLD_GET(pfms)       ((*(pfms) & PFMS_FREEZE_THRESHOLD) >>  PFMS_FREEZE_THRESHOLD_SHFT)

#define PFMS_FRZTHRESHOLD_PUT(pfms, v)    (atomicFieldAssignUint((pfms),              \
                                                                 PFMS_FREEZE_THRESHOLD, \
                                                                 ((v) << PFMS_FREEZE_THRESHOLD_SHFT)) & PFMS_FREEZE_THRESHOLD)

#define PFMSV_FRZTHRESHOLD_GET(pfmsv)     (((pfmsv) & PFMS_FREEZE_THRESHOLD) >>  PFMS_FREEZE_THRESHOLD_SHFT)

#define PFMSV_FRZTHRESHOLD_PUT(pfmsv, v)  ((pfmsv) &= ~PFMS_FREEZE_THRESHOLD, \
                                           (pfmsv) |= ((v) << PFMS_FREEZE_THRESHOLD_SHFT) & PFMS_FREEZE_THRESHOLD)

/*
 * Melting threshold
 */
#define PFMS_MLTTHRESHOLD_GET(pfms)       ((*(pfms) & PFMS_MELT_THRESHOLD) >>  PFMS_MELT_THRESHOLD_SHFT)

#define PFMS_MLTTHRESHOLD_PUT(pfms, v)    (atomicFieldAssignUint((pfms),              \
                                                                 PFMS_MELT_THRESHOLD, \
                                                                 ((v) << PFMS_MELT_THRESHOLD_SHFT)) & PFMS_MELT_THRESHOLD)

#define PFMSV_MLTTHRESHOLD_GET(pfmsv)     (((pfmsv) & PFMS_MELT_THRESHOLD) >>  PFMS_MELT_THRESHOLD_SHFT)

#define PFMSV_MLTTHRESHOLD_PUT(pfmsv, v)  ((pfmsv) &= ~PFMS_MELT_THRESHOLD, \
                                           (pfmsv) |= ((v) << PFMS_MELT_THRESHOLD_SHFT) & PFMS_MELT_THRESHOLD)


/*
 * Dampening Count
 */
#define PFMS_MIGRDAMPENCNT_GET(pfms)       ((*(pfms) & PFMS_MIGRDAMPENCNT) >>  PFMS_MIGRDAMPENCNT_SHFT)

#define PFMS_MIGRDAMPENCNT_PUT(pfms, v)    (atomicFieldAssignUint((pfms),            \
                                                                 PFMS_MIGRDAMPENCNT, \
                                                                 ((v) << PFMS_MIGRDAMPENCNT_SHFT)) & PFMS_MIGRDAMPENCNT)

#define PFMSV_MIGRDAMPENCNT_GET(pfmsv)     (((pfmsv) & PFMS_MIGRDAMPENCNT) >>  PFMS_MIGRDAMPENCNT_SHFT)

#define PFMSV_MIGRDAMPENCNT_PUT(pfmsv, v)  ((pfmsv) &= ~PFMS_MIGRDAMPENCNT, \
                                           (pfmsv) |= ((v) << PFMS_MIGRDAMPENCNT_SHFT) & PFMS_MIGRDAMPENCNT)

/*
 * Dampening Factor
 */
#define PFMS_MIGRDAMPENFACTOR_GET(pfms)       ((*(pfms) & PFMS_MIGRDAMPENFACTOR) >>  PFMS_MIGRDAMPENFACTOR_SHFT)

#define PFMS_MIGRDAMPENFACTOR_PUT(pfms, v)    (atomicFieldAssignUint((pfms),            \
                                                                 PFMS_MIGRDAMPENFACTOR, \
                                                                 ((v) << PFMS_MIGRDAMPENFACTOR_SHFT)) & PFMS_MIGRDAMPENFACTOR)

#define PFMSV_MIGRDAMPENFACTOR_GET(pfmsv)     (((pfmsv) & PFMS_MIGRDAMPENFACTOR) >>  PFMS_MIGRDAMPENFACTOR_SHFT)

#define PFMSV_MIGRDAMPENFACTOR_PUT(pfmsv, v)  ((pfmsv) &= ~PFMS_MIGRDAMPENFACTOR, \
                                           (pfmsv) |= ((v) << PFMS_MIGRDAMPENFACTOR_SHFT) & PFMS_MIGRDAMPENFACTOR)

/*
 * Test bit
 */
#define PFMS_IS_TESTBIT(pfms)             (*(pfms) & PFMS_TESTBIT)
#define PFMS_TESTBIT_SET(pfms)            (atomicSetUint((pfms), PFMS_TESTBIT))
#define PFMS_TESTBIT_CLR(pfms)            (atomicClearUint((pfms), PFMS_TESTBIT))
#define PFMSV_IS_TESTBIT(pfmsv)           ((pfmsv) & PFMS_TESTBIT)
#define PFMSV_TESTBIT_SET(pfmsv)          ((pfmsv) |= PFMS_TESTBIT)
#define PFMSV_TESTBIT_CLR(pfmsv)          ((pfmsv) &= ~PFMS_TESTBIT)    

/*
 * Auto migration enable bit
 */
#define PFMS_IS_MIGR_ENABLED(pfms)        (*(pfms) & PFMS_MIGR_ENABLED)
#define PFMS_MIGR_ENABLED_SET(pfms)       (atomicSetUint((pfms), PFMS_MIGR_ENABLED))
#define PFMS_MIGR_ENABLED_CLR(pfms)       (atomicClearUint((pfms), PFMS_MIGR_ENABLED))
#define PFMSV_IS_MIGR_ENABLED(pfmsv)      ((pfmsv) & PFMS_MIGR_ENABLED)
#define PFMSV_MIGR_ENABLED_SET(pfmsv)     ((pfmsv) |= PFMS_MIGR_ENABLED)
#define PFMSV_MIGR_ENABLED_CLR(pfmsv)     ((pfmsv) &= ~PFMS_MIGR_ENABLED)    

/*
 * Freezing enable bit
 */
#define PFMS_IS_FREEZE_ENABLED(pfms)      (*(pfms) & PFMS_FREEZE_ENABLED)
#define PFMS_FREEZE_ENABLED_SET(pfms)     (atomicSetUint((pfms), PFMS_FREEZE_ENABLED))
#define PFMS_FREEZE_ENABLED_CLR(pfms)     (atomicClearUint((pfms), PFMS_FREEZE_ENABLED))
#define PFMSV_IS_FREEZE_ENABLED(pfmsv)    ((pfmsv) & PFMS_FREEZE_ENABLED)
#define PFMSV_FREEZE_ENABLED_SET(pfmsv)   ((pfmsv) |= PFMS_FREEZE_ENABLED)
#define PFMSV_FREEZE_ENABLED_CLR(pfmsv)   ((pfmsv) &= ~PFMS_FREEZE_ENABLED)   

/*
 * Melting enable bit
 */
#define PFMS_IS_MELT_ENABLED(pfms)        (*(pfms) & PFMS_MELT_ENABLED)
#define PFMS_MELT_ENABLED_SET(pfms)       (atomicSetUint((pfms), PFMS_MELT_ENABLED))
#define PFMS_MELT_ENABLED_CLR(pfms)       (atomicClearUint((pfms), PFMS_MELT_ENABLED))
#define PFMSV_IS_MELT_ENABLED(pfmsv)      ((pfmsv) & PFMS_MELT_ENABLED)
#define PFMSV_MELT_ENABLED_SET(pfmsv)     ((pfmsv) |= PFMS_MELT_ENABLED)
#define PFMSV_MELT_ENABLED_CLR(pfmsv)     ((pfmsv) &= ~PFMS_MELT_ENABLED)    

/*
 * Dampening enable bit
 */
#define PFMS_IS_MIGRDAMPEN_ENABLED(pfms)        (*(pfms) & PFMS_MIGRDAMPEN_ENABLED)
#define PFMS_MIGRDAMPEN_ENABLED_SET(pfms)       (atomicSetUint((pfms), PFMS_MIGRDAMPEN_ENABLED))
#define PFMS_MIGRDAMPEN_ENABLED_CLR(pfms)       (atomicClearUint((pfms), PFMS_MIGRDAMPEN_ENABLED))
#define PFMSV_IS_MIGRDAMPEN_ENABLED(pfmsv)      ((pfmsv) & PFMS_MIGRDAMPEN_ENABLED)
#define PFMSV_MIGRDAMPEN_ENABLED_SET(pfmsv)     ((pfmsv) |= PFMS_MIGRDAMPEN_ENABLED)
#define PFMSV_MIGRDAMPEN_ENABLED_CLR(pfmsv)     ((pfmsv) &= ~PFMS_MIGRDAMPEN_ENABLED)    

/*
 * Refcnt enable bit
 */
#define PFMS_IS_MIGRREFCNT_ENABLED(pfms)        (*(pfms) & PFMS_MIGRREFCNT_ENABLED)
#define PFMS_MIGRREFCNT_ENABLED_SET(pfms)       (atomicSetUint((pfms), PFMS_MIGRREFCNT_ENABLED))
#define PFMS_MIGRREFCNT_ENABLED_CLR(pfms)       (atomicClearUint((pfms), PFMS_MIGRREFCNT_ENABLED))
#define PFMSV_IS_MIGRREFCNT_ENABLED(pfmsv)      ((pfmsv) & PFMS_MIGRREFCNT_ENABLED)
#define PFMSV_MIGRREFCNT_ENABLED_SET(pfmsv)     ((pfmsv) |= PFMS_MIGRREFCNT_ENABLED)
#define PFMSV_MIGRREFCNT_ENABLED_CLR(pfmsv)     ((pfmsv) &= ~PFMS_MIGRREFCNT_ENABLED)    


/*
 * Bouncing control
 *     Updates & accesses to these control bits
 *     not require any locks. They are written
 *     atomically by using ll/sc
 */

/*
 * Page frozen bit
 */
#define PFMS_IS_FROZEN(pfms)              (*(pfms) & PFMS_FROZEN)
#define PFMS_FROZEN_SET(pfms)             (atomicSetUint((pfms), PFMS_FROZEN))
#define PFMS_FROZEN_CLR(pfms)             (atomicClearUint((pfms), PFMS_FROZEN))
#define PFMSV_IS_FROZEN(pfmsv)            ((pfmsv) & PFMS_FROZEN)
#define PFMSV_FROZEN_SET(pfmsv)           ((pfmsv) |= PFMS_FROZEN)
#define PFMSV_FROZEN_CLR(pfmsv)           ((pfmsv) &= ~PFMS_FROZEN)    

/*
 * Bounce Counter
 *     If the page is not frozen, this counter
 *     is used to count the number of migrations,
 *     Else, if the page is frozen, this counter
 *     is used to count the melting time.
 */

/*
 * Migration count
 */
#define PFMS_MGRCNT_GET(pfms)             ((*(pfms) & PFMS_BOUNCECOUNT) >>  PFMS_BOUNCECOUNT_SHFT)

#define PFMS_MGRCNT_PUT(pfms, v)          (atomicFieldAssignUint((pfms),              \
                                                                 PFMS_BOUNCECOUNT, \
                                                                 ((v) << PFMS_BOUNCECOUNT_SHFT)) & PFMS_BOUNCECOUNT)

#define PFMSV_MGRCNT_GET(pfmsv)           (((pfmsv) & PFMS_BOUNCECOUNT) >>  PFMS_BOUNCECOUNT_SHFT)

#define PFMSV_MGRCNT_PUT(pfmsv, v)        ((pfmsv) &= ~PFMS_BOUNCECOUNT, \
                                           (pfmsv) |= ((v) << PFMS_BOUNCECOUNT_SHFT) & PFMS_BOUNCECOUNT)


/*
 * Behavior on migration request failure
 */
#define PFMS_IS_ENQONFAIL(pfms)           (*(pfms) & PFMS_ENQONFAIL)
#define PFMS_ENQONFAIL_SET(pfms)          (atomicSetUint((pfms), PFMS_ENQONFAIL))
#define PFMS_ENQONFAIL_CLR(pfms)          (atomicClearUint((pfms), PFMS_ENQONFAIL))
#define PFMSV_IS_ENQONFAIL(pfmsv)         ((pfmsv) & PFMS_ENQONFAIL)
#define PFMSV_ENQONFAIL_SET(pfmsv)        ((pfmsv) |= PFMS_ENQONFAIL)
#define PFMSV_ENQONFAIL_CLR(pfmsv)        ((pfmsv) &= ~PFMS_ENQONFAIL)    

/*
 * Pfms state control bits
 *     These bits can be updated only if the pfms_lock is acquired
 *     And updates also have to be done atomically (to avoid messing up the other fields)
 * PFMS States
 *     The bits PFMS_MAPPED, PFMS_MIGRED, and PFMS_QUEUED
 *     define the state of a pfms.
 *     There are only 4 possible states:
 *       1) PFMS_STATE_UNMAPPED: page is not mapped by any process
 *       2) PFMS_STATE_MAPPED: page is mapped by some process, but not being migrated or queued
 *       3) PFMS_STATE_MIGRED: page is both mapped by a user process & being migrated, but not queued
 *       4) PFMS_STATE_QUEUED: page is mapped by a user and being migrated and queued
 */

#define PFMS_STATE_BITS                 (PFMS_MAPPED | PFMS_MIGRED | PFMS_QUEUED)
#define PFMS_STATE_UNMAPPED             (0)
#define PFMS_STATE_MAPPED               (PFMS_MAPPED)
#define PFMS_STATE_MIGRED               (PFMS_MAPPED | PFMS_MIGRED)
#define PFMS_STATE_QUEUED               (PFMS_MAPPED | PFMS_MIGRED | PFMS_QUEUED)   

#define PFMS_STATE_IS_UNMAPPED(pfms)    ((*(pfms) & PFMS_STATE_BITS) == PFMS_STATE_UNMAPPED)
#define PFMSV_STATE_IS_UNMAPPED(pfmsv)  (((pfmsv) & PFMS_STATE_BITS) == PFMS_STATE_UNMAPPED)
#define PFMS_STATE_IS_MAPPED(pfms)      ((*(pfms) & PFMS_STATE_BITS) == PFMS_STATE_MAPPED)
#define PFMSV_STATE_IS_MAPPED(pfmsv)    (((pfmsv) & PFMS_STATE_BITS) == PFMS_STATE_MAPPED)
#define PFMS_STATE_IS_MIGRED(pfms)      ((*(pfms) & PFMS_STATE_BITS) == PFMS_STATE_MIGRED)
#define PFMSV_STATE_IS_MIGRED(pfmsv)    (((pfmsv) & PFMS_STATE_BITS) == PFMS_STATE_MIGRED)
#define PFMS_STATE_IS_QUEUED(pfms)      ((*(pfms) & PFMS_STATE_BITS) == PFMS_STATE_QUEUED)
#define PFMSV_STATE_IS_QUEUED(pfmsv)    (((pfmsv) & PFMS_STATE_BITS) == PFMS_STATE_QUEUED)

#define PFMS_STATE_UNMAPPED_SET(pfms)   (atomicFieldAssignUint((pfms),               \
                                                               PFMS_STATE_BITS,      \
                                                               PFMS_STATE_UNMAPPED))
#define PFMSV_STATE_UNMAPPED_SET(pfmsv)  ((pfmsv) &= ~PFMS_STATE_BITS,               \
                                           (pfmsv) |=  PFMS_STATE_UNMAPPED)

#define PFMS_STATE_MAPPED_SET(pfms)    (atomicFieldAssignUint((pfms),                \
                                                               PFMS_STATE_BITS,      \
                                                               PFMS_STATE_MAPPED))
#define PFMSV_STATE_MAPPED_SET(pfmsv)  ((pfmsv) &= ~PFMS_STATE_BITS,                 \
                                           (pfmsv) |=  PFMS_STATE_MAPPED)

#define PFMS_STATE_MIGRED_SET(pfms)    (atomicFieldAssignUint((pfms),                \
                                                               PFMS_STATE_BITS,      \
                                                               PFMS_STATE_MIGRED))
#define PFMSV_STATE_MIGRED_SET(pfmsv)  ((pfmsv) &= ~PFMS_STATE_BITS,                 \
                                           (pfmsv) |=  PFMS_STATE_MIGRED)

#define PFMS_STATE_QUEUED_SET(pfms)    (atomicFieldAssignUint((pfms),                \
                                                               PFMS_STATE_BITS,      \
                                                               PFMS_STATE_QUEUED)) 
#define PFMSV_STATE_QUEUED_SET(pfmsv)  ((pfmsv) &= ~PFMS_STATE_BITS,                 \
                                           (pfmsv) |= PFMS_STATE_QUEUED)

/*
 * Pfms mutex calls
 */
#define PFMS_LOCK(pmfs, s)                                                 \
        {                                                                  \
                  (s) = splhi();                                           \
                  bitlock_acquire_32bit((pfms), PFMS_LOCKBIT);             \
        }

#define PFMS_UNLOCK(pfms, s)                                               \
        {                                                                  \
                  bitlock_release_32bit((pfms), PFMS_LOCKBIT);             \
                  splx((s));                                               \
        }

#define NESTED_PFMS_LOCK(pmfs)                                             \
        {                                                                  \
                  bitlock_acquire_32bit((pfms), PFMS_LOCKBIT);             \
        }

#define NESTED_PFMS_UNLOCK(pfms)                                           \
        {                                                                  \
                  bitlock_release_32bit((pfms), PFMS_LOCKBIT);             \
        }

#define PFMS_IS_LOCKED(pfms)   (*(pfms) & PFMS_LOCKBIT)

#define PFMSV_LOCK_CLR(pfmsv)  ((pfmsv) &= ~PFMS_LOCKBIT)

#define PFMSV_IS_LOCKED(pfmsv) ((pfmsv) & PFMS_LOCKBIT)

/*
 * Clear all flags (the full pmfs, except the the lockbit
 */
#define PFMS_FLAGS_CLR(pfms)   (atomicFieldAssignUint((pfms), PFMS_FLAGS, 0))


 
/*
 * Conversion between pfn and pfms_t
 * pfms is always a pointer to the actual pfms_t data
 */

#define PFN_TO_PFMS(pfn)                                  \
        (&((NODEPDA(PFN_TO_CNODEID((pfn))))->mcd->pfms    \
        [pfn_to_slot_num((pfn))][pfn_to_slot_offset((pfn))]))


/*
 * Freezing/Melting migration  counter absolute and relative values
 */
#define PFMSV_MGRCNT_MAX 7

#define PFMSV_MGRCNT_ABS_TO_REL(abs_value) \
        ( ((abs_value) * 100) / PFMSV_MGRCNT_MAX )

#define PFMSV_MGRCNT_REL_TO_ABS(rel_value) \
        ( ((rel_value) * PFMSV_MGRCNT_MAX) / 100 )

#define PFMS_MGRCNT_REL_GET(pfsm)   (PFMSV_MGRCNT_ABS_TO_REL( PFMS_MGRCNT_GET((pfms)) ))
#define PFMSV_MGRCNT_REL_GET(pfmsv) (PFMSV_MGRCNT_ABS_TO_REL( PFMSV_MGRCNT_GET((pfmsv)) ))


/*
 * dampening counter absolute and relative counters
 */
#define PFMSV_MIGRDAMPENFACTOR_MAX 3

#define PFMSV_MIGRDAMPENFACTOR_ABS_TO_REL(abs_value) \
        ( ((abs_value) * 100) / PFMSV_MIGRDAMPENFACTOR_MAX )

#define PFMSV_MIGRDAMPENFACTOR_REL_TO_ABS(rel_value) \
        ( ((rel_value) * PFMSV_MIGRDAMPENFACTOR_MAX) / 100 )

#define PFMS_MIGRDAMPENFACTOR_REL_GET(pfsm)   (PFMSV_MIGRDAMPENFACTOR_ABS_TO_REL( PFMS_MIGRDAMPENFACTOR_GET((pfms)) ))
#define PFMSV_MIGRDAMPENFACTOR_REL_GET(pfmsv) (PFMSV_MIGRDAMPENFACTOR_ABS_TO_REL( PFMSV_MIGRDAMPENFACTOR_GET((pfmsv)) ))




extern pfms_t*
nodepda_pfms_alloc(cnodeid_t node, caddr_t *nextbyte);

#else

#define	PFN_TO_PFMS(pfn)		0
#define	PFMS_LOCK(pfms, pfms_spl)
#define	PFMS_UNLOCK(pfms, pfms_spl)
#define NESTED_PFMS_LOCK(pmfs)
#define NESTED_PFMS_UNLOCK(pfms)
#define	PFMS_STATE_IS_MAPPED(pfms)	1
#define	PFMS_IS_MIGR_ENABLED(pfms)	0
#define	PFMS_STATE_MIGRED_SET(pfms)
#define PFMS_GETV(pfms)                 0

#endif	/* NUMA_BASE */


#endif /* __SYS_PFMS_H__ */







