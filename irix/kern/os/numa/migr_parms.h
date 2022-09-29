/*****************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
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


#ifndef __MIGR_PARMS_H__
#define __MIGR_PARMS_H__

#include <sys/pmo.h>

/*
 * Per address space parameters
 * Thresholds in percentage [0,100]
 */

#define MIGRATION_MODE_DIS     0
#define MIGRATION_MODE_EN      1
#define MIGRATION_MODE_ON      2
#define MIGRATION_MODE_OFF     3

#define REFCNT_MODE_DIS     0
#define REFCNT_MODE_EN      1
#define REFCNT_MODE_ON      2
#define REFCNT_MODE_OFF     3

typedef struct migr_as_kparms {
        uchar_t migr_base_mode            :4,
                migr_freeze_enabled       :2,
                migr_melt_enabled         :2;            
        uchar_t migr_refcnt_mode          :4,
                migr_enqonfail_enabled    :2,
                migr_dampening_enabled    :2;
        uchar_t migr_base_threshold;
        uchar_t migr_min_distance;
        uchar_t migr_min_maxradius;
        uchar_t migr_freeze_threshold;
        uchar_t migr_melt_threshold;
        uchar_t migr_dampening_factor;
} migr_as_kparms_t;


/*
 * System parameters
 * Intervals in seconds,
 * Thresholds in percentage [0,100]
 */
typedef struct migr_system_kparms {
        /*
         * Enable/disable daemons
         */
        uchar_t    memoryd_enabled                     :1,
                   migr_traffic_control_enabled        :1,
                   migr_unpegging_control_enabled      :1,
                   migr_auto_migr_mech                 :1,
                   migr_user_migr_mech                 :1,
                   migr_coaldmigr_mech                 :1;

        /*
         * Migration/Refcnt threshold reference
         */
        uint      migr_threshold_reference;

        /*
         * Effective Refcnt threshold
         */
        uint      migr_effthreshold_refcnt_overflow;
                  
        /*
         * Traffic Control
         */
        uint       migr_traffic_control_interval;
        uchar_t    migr_traffic_control_threshold;

        /*
         * Bounce (freezing and melting + dampening) Control
         */
        uint       migr_bounce_control_interval;

        /*
         * Unpegging Control
         */
        uint       migr_unpegging_control_interval;
        uchar_t    migr_unpegging_control_threshold;

        /*
         * Migration Queue Control
         */
        uchar_t    migr_queue_control_enabled          :1,
                   migr_queue_traffic_trigger_enabled  :1,
                   migr_queue_capacity_trigger_enabled :1,
                   migr_queue_time_trigger_enabled     :1;
        
        uchar_t    migr_queue_maxlen;
        uint       migr_queue_control_interval;
        uchar_t    migr_queue_control_mlimit;
        uchar_t    migr_queue_traffic_trigger_threshold;
        uchar_t    migr_queue_capacity_trigger_threshold;
        uint       migr_queue_time_trigger_interval;

        uchar_t    migr_queue_control_max_page_age;
        
        /*
         * Memory Pressure
         * Minimum free memory in node in order to use
         * as migration destination
         */
        uchar_t    migr_memory_low_enabled      :1;
        uchar_t    migr_memory_low_threshold;

	/*
	 * Migration vehicle: BTE or TLBSYNC
	 */
	uchar_t    migr_vehicle;
        
} migr_system_kparms_t;

typedef struct migr_parms {
        PMOCTL_VERSION_HEADER;
        migr_as_kparms_t      migr_as_kparms;
        migr_system_kparms_t  migr_system_kparms;
        uint                  reserved[512];
} migr_parms_t;



#endif /* __MIGR_PARMS_H__ */
