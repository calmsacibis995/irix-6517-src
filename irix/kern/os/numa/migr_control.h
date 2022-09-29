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
 * NUMA portion of the nodepda
 */

#ifndef __MIGR_CONTROL_H__
#define __MIGR_CONTROL_H__


/*
 * lboot defined parameters for migration control
 */

/* Basic Migration */
extern int numa_migr_default_mode;
extern int numa_migr_default_threshold;
extern int numa_migr_threshold_reference;
extern int numa_migr_min_distance;
extern int numa_migr_min_maxradius;
extern int numa_migr_freeze_enabled;
extern int numa_migr_freeze_threshold;
extern int numa_migr_melt_enabled;
extern int numa_migr_melt_threshold;
extern int numa_migr_enqonfail_enabled;
extern int numa_migr_dampening_enabled;
extern int numa_migr_dampening_factor;  

/* Selection of migration mechanism */
extern int numa_migr_auto_migr_mech;
extern int numa_migr_user_migr_mech;
extern int numa_migr_coaldmigr_mech;

/* Selection of migration vehicle */
extern int numa_migr_vehicle;

/* Refcnt mode */
extern int numa_refcnt_default_mode;
extern int numa_refcnt_overflow_threshold;

/* Memory Daemon */
extern int memoryd_enabled;
extern int numa_migr_unpegging_control_enabled;
extern int numa_migr_unpegging_control_interval;
extern int numa_migr_unpegging_control_threshold;
extern int numa_migr_traffic_control_enabled;
extern int numa_migr_traffic_control_interval;
extern int numa_migr_traffic_control_threshold;
extern int numa_migr_bounce_control_interval;


/* Migration Queue */
extern int numa_migr_queue_maxlen;
extern int numa_migr_queue_control_enabled;
extern int numa_migr_queue_control_interval;
extern int numa_migr_queue_control_mlimit;
extern int numa_migr_queue_traffic_trigger_enabled;
extern int numa_migr_queue_traffic_trigger_threshold;
extern int numa_migr_queue_capacity_trigger_enabled;
extern int numa_migr_queue_capacity_trigger_threshold;
extern int numa_migr_queue_time_trigger_enabled;
extern int numa_migr_queue_time_trigger_interval;
extern int numa_migr_queue_control_max_page_age;

/* Memory Pressure */
extern int numa_migr_memory_low_enabled;
extern int numa_migr_memory_low_threshold;

#include "migr_parms.h"

typedef struct migr_memoryd_state {
	uchar_t traffic_period_count;
	uchar_t queue_period_count;
} migr_memoryd_state_t;

struct migr_page_list_s;

/* data needed for the page migration control */
typedef struct migr_control_data_s {

        /*
         * System defaults for AS migr parameters
         */
        migr_as_kparms_t migr_as_kparms;

        /*
         * System default for system migr parameters
         */
        migr_system_kparms_t migr_system_kparms;

        /*
         * Current memoryd state
         */
        migr_memoryd_state_t migr_memoryd_state;

        /*
         * Migration Queue
         */
	struct migr_queue_s *migr_queue; 

	/*
	 * Local list which will be used by a process_migr_queue()
	 * to do cluster migration
	 */
	struct migr_page_list_s	*local_migr_page_list;
	/* 
	 * a 2-dimensional array for bounce control data:
	 */
        pfms_t* pfms;

	/* 
	 * A spinlock for accessing NUMA related hub hardware registers:
	 * 1. migration threshold register 
	 * 2. reference counters 
	 */
	lock_t hub_lock;

	/*
	 * Cached hub register data for use in migration module
	 * XXX Maybe we should cache some hub registers in a unique 
	 * place of nodepda for use by other kernel modules. 
	 */
	unsigned char hub_dir_type;  /* Type of DIMM used for directory */

        /*
         * Can we use poison bits?
         */
        unsigned char bte_poison_ok;
        

} migr_control_data_t;

#define NUMA_MIGR_BASE_EFFTHRESHOLD_MIN    32
#define NUMA_MIGR_BASE_RELTHRESHOLD_DEF    50

#define NUMA_REFCNT_BASE_EFFTHRESHOLD_MIN 200
#define NUMA_REFCNT_BASE_EFFTHRESHOLD_DEF MIGR_COUNTER_STANDARD_MAX - 100

#define THRESH_LOCK(node) \
	(mutex_spinlock(&(NODEPDA_MCD(node)->hub_lock)))

#define THRESH_UNLOCK(node, s) \
	(mutex_spinunlock(&(NODEPDA_MCD(node)->hub_lock), (s)))

extern void migr_control_init(cnodeid_t node, caddr_t* nextbyte);

#define MIGR_THRESHREF_STANDARD   0
#define MIGR_THRESHREF_PREMIUM    1

#endif /* __MIGR_CONTROL_H__ */






