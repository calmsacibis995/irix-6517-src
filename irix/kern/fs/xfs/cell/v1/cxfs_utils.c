/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.3 $"

/*
 * General routines for the Cell XFS File System.
 */

#include <sys/types.h>
#include <ksys/cell/service.h>

#include "cxfs.h"

/*
 * CXFS global data.
 */

service_t	cxfs_service_id;		/* our CXFS service id */
int		cxfs_inited = 0;

cell_t cellid(void);

/* 
 * Initialize CXFS.  Called by CFS during system initialization.
 */
void
cxfs_init()
{
	if (cxfs_inited) return;
	cxfs_inited = 1;

	SERVICE_MAKE(cxfs_service_id, cellid(), SVC_CXFS);  /* our service id */
	dsxvn_init();
	dcxvn_init();
	dcxvfs_init();
}
