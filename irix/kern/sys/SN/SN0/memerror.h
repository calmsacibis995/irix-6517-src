/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1996-1997 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.6 $"

#include <sys/SN/SN0/addrs.h>
#include <sys/SN/SN0/hubmd.h>

#define SBE_EVENTS_PER_NODE	16	/* Storage space allocated       */
#define SBE_TIMEOUT		60	/* Seconds                       */
#define SBE_DISCARD_TIMEOUT	600	/* Time between discard attempts */

#define SBE_EVENT_DIR		0x01	/* Flags                         */

/* systunable sbe_mem_verbose sets the level of sbe reporting verbosity.
 * Additional flags for this tunable should be added here.
 */
extern int sbe_mem_verbose;
#define SBE_MEM_VERBOSE_REPORT_DISCARD        0x0001

#define SBE_MAX_INTR_PER_MIN	600

typedef struct sbe_event_s {
    pfn_t		pfn;		/* Page of error                 */
    int			flags;
    int			repcnt;
    time_t		expire;		/* Time until sbe is forgotten   */
} sbe_event_t;

typedef struct sbe_info_s {
    int			disabled;	/* True if polling is disabled   */
    int			log_cnt;	/* Num. log array entries in use */
    sbe_event_t		log[SBE_EVENTS_PER_NODE];
    int			bank_cnt[MD_MEM_BANKS];	 /* Per-bank error count */
    int			intr_ct;	/* Safety threshold intr count   */
    time_t		intr_tm;	/* Safety threshold reset time   */
} sbe_info_t;
