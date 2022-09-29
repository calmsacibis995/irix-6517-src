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

#ifndef	_OS_PROC_DCPROC_H_
#define	_OS_PROC_DCPROC_H_ 1

#include "dproc.h"

typedef struct dcproc {
	TKC_DECL(dcp_tclient, VPROC_NTOKENS);
	obj_handle_t	dcp_handle;
	bhv_desc_t	dcp_bhv;
} dcproc_t;

#define BHV_TO_DCPROC(b)        \
		(ASSERT(BHV_POSITION(b) == VPROC_BHV_DC), \
		(dcproc_t *)(BHV_PDATA(b)))

#endif	/* _OS_PROC_DCPROC_H_ */
