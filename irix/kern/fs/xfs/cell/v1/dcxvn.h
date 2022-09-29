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

#ifndef	_XFS_CELL_V1_DCXVN_H_
#define	_XFS_CELL_V1_DCXVN_H_

#ident	"$Id: dcxvn.h,v 1.4 1997/08/22 18:31:26 mostek Exp $"

#include <sys/vnode.h>
#include <ksys/cell/tkm.h>
#include <fs/cell/fsc_types.h>

struct dcxvn {
	dcvn_t 		*dcx_dcvn;	/* Associated CFS structure */
        vattr_t 	*dcx_vap;       /* Pointer to associated file
                                           attributes in dcvn. */
        tkc_state_t     *dcx_tokens;    /* Pointer to client token state
                                           in dcvn. */
	caddr_t		dcx_extents;	/* File extents */
	size_t		dcx_ext_count;	/* Size of extents */
	int		dcx_lastext;	/* Last extent we looked at */
	dcxvfs_t	*dcx_vfs;	/* VFS pointer */
};

#endif /* _XFS_CELL_V1_DCXVN_H_ */
