/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN0_WAR_H__
#define __SYS_SN_SN0_WAR_H__
/****************************************************************************
 * Support macros and defitions for hardware workarounds in		    *
 * early chip versions.                                                     *
 ****************************************************************************/

/*
 * This is the bitmap of runtime-switched workarounds.
 */
typedef short warbits_t;

extern int warbits_override;

#define WAR_II_IFDR_BIT		(0x0001)
#define WAR_ERR_STS_BIT		(0x0002)
#define WAR_MD_MIGR_BIT         (0x0004)
#define	WAR_HUB_POQ_BIT		(0x0008)
#define WAR_EXTREME_ERR_STS_BIT	(0x0010)

#define WAR_IS_ENABLED(_bit)	(private.p_warbits & (_bit))

#define WAR_II_IFDR_ENABLED	WAR_IS_ENABLED(WAR_II_IFDR_BIT)
#define WAR_ERR_STS_ENABLED	WAR_IS_ENABLED(WAR_ERR_STS_BIT)
#define WAR_MD_MIGR_ENABLED(n)	                                            \
	(CNODE_TO_CPU_BASE(n) != -1 ? 					     \
        pdaindr[CNODE_TO_CPU_BASE(n)].pda->p_warbits & WAR_MD_MIGR_BIT : 1)

/*
 * HUB POQ WAR is enabled by default. We use this bit to indicate
 * that this war should be disabled via warbits_override.
 */

#define	WAR_HUB_POQ_DISABLED()	(private.p_warbits & WAR_HUB_POQ_BIT)

#define WAR_HUB2_0_WAR_BITS	(WAR_II_IFDR_BIT |                           \
				 WAR_ERR_STS_BIT |                           \
				 WAR_EXTREME_ERR_STS_BIT |		     \
				 WAR_MD_MIGR_BIT)

#define WAR_HUB2_1_WAR_BITS	0
#define WAR_HUB2_2_WAR_BITS	0
#define WAR_HUB2_3_WAR_BITS	(WAR_HUB_POQ_BIT)
#define WAR_HUB2_4_WAR_BITS	(WAR_HUB_POQ_BIT)
				/* Setting WAR_HUB_POQ_BIT disables war */

#if HUB_MIGR_WAR
/*
 * In order to work around the problem with the migration difference 
 * threshold register in Hub 1.0 and 2.0, we have to fix the migration 
 * difference threshold to 0xFFFFF, in both standard and preimum directory
 * modes.
 */
#define MIGR_DIFF_THRESHOLD_WAR    0xFFFFF  
#endif /* HUB_MIGR_WAR */

extern void sn0_poq_war(void *);
#endif /*  __SYS_SN_SN0_WAR_H__ */
