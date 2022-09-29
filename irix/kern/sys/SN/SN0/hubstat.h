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

#ifndef __SYS_SN_SN0_HUBSTAT_H__
#define __SYS_SN_SN0_HUBSTAT_H__

typedef __int64_t 	hub_count_t;

#define HUBSTAT_VERSION	1

typedef struct hubstat_s {
        char            hs_version;		/* structure version    */
        cnodeid_t       hs_cnode;       	/* cnode of this hub    */
        nasid_t         hs_nasid;       	/* Nasid of same        */	
	__int64_t	hs_timebase;		/* Time of first sample */
	__int64_t	hs_timestamp;		/* Time of last sample	*/
	__int64_t	hs_per_minute;		/* Ticks per minute 	*/

        hubreg_t	hs_ni_stat_rev_id;	/* Status rev ID value  */
        hub_count_t	hs_ni_retry_errors;	/* Total retry errors   */
        hub_count_t	hs_ni_sn_errors;	/* Total sn errors      */
        hub_count_t	hs_ni_cb_errors;	/* Total cb errors      */
        int		hs_ni_overflows;	/* NI count overflows   */
        hub_count_t	hs_ii_sn_errors;	/* Total sn errors      */
        hub_count_t	hs_ii_cb_errors;	/* Total cb errors      */
        int		hs_ii_overflows;	/* II count overflows   */

	/*
	 * Anything below this comment is intended for kernel internal-use
	 * only and may be changed at any time.
	 *
	 * Any members that contain pointers or are conditionally compiled
	 * need to be below here also.
	 */
        __int64_t	hs_last_print;		/* When we last printed */
        char		hs_print;		/* Should we print      */

	char	       *hs_name;		/* This hub's name */
	unsigned char	hs_maint;		/* Should we print to availmon */
} hubstat_t;

#endif /* __SYS_SN_SN0_HUBSTAT_H__ */

