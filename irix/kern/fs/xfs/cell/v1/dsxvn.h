/**************************************************************************
 *									  *
 *		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_XFS_CELL_V1_DSXVN_H_
#define	_XFS_CELL_V1_DSXVN_H_

#ident	"$Id: dsxvn.h,v 1.3 1997/08/22 18:31:28 mostek Exp $"

#include <sys/vnode.h>
#include <ksys/cell/tkm.h>
#include <fs/cell/fsc_types.h>

struct dsxvn {
	dsvn_t 		*dsx_dsvn;	/* Associated CFS structure */
	bhv_desc_t	*dsx_bdp;	/* XFS entry in behaviour chain */
        tkc_state_t     dsx_tclient;    /* Pointer to client token state
                                           in dcvn. */
        tks_state_t     dsx_tserver;    /* Pointer to client token state
                                           in dcvn. */
};

#endif /* _XFS_CELL_V1_DSXVN_H_ */
