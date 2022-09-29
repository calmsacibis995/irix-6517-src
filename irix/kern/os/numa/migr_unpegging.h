/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __MIGRATION_UNPEGGING_H__
#define __MIGRATION_UNPEGGING_H__

extern pfn_t migr_periodic_unpegging_control(cnodeid_t nodeid,
                                             pfn_t startpfn,
                                             pfn_t numpfns);

#endif /* __MIGRATION_UNPEGGING_H__ */
