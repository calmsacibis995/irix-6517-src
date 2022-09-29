/*
 * FILE: eoe/cmd/miser/cmd/miser/policy.c
 *
 * DESCRIPTION:
 *	Miser queue policy definition file.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include <sys/miser_public.h>


error_t 
default_policy(void*, bid_t, miser_seg_t*, miser_seg_t*, miser_time_t*);

error_t 
repack_schedule(void*, bid_t, miser_seg_t*, miser_seg_t*, miser_time_t*);

error_t 
repack_remove(void*, bid_t, miser_seg_t*, miser_seg_t*, miser_time_t*);

/* 
 * For policy's that dont have a policy in either the do_policy 
 * or redo_policy
 */
error_t 
null_policy(void* v, bid_t bid, miser_seg_t* r, miser_seg_t* s, 
		miser_time_t* ok) { 
	return MISER_ERR_OK; 
}


static struct 
policy_defs miser_policy_defs[] = 
{
	{ "DEFAULT", default_policy, null_policy },
	{ "REPACK", repack_schedule, repack_remove }
};

struct policy_defs *miser_policy_begin = miser_policy_defs;

struct policy_defs *miser_policy_end = &miser_policy_defs 
		[sizeof(miser_policy_defs) / sizeof(miser_policy_defs[0])];
