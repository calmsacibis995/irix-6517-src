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

#ifndef	_FS_CELL_CXFS_INTFC_H_
#define	_FS_CELL_CXFS_INTFC_H_

#ident	"$Id: cxfs_intfc.h,v 1.5 1997/08/22 18:30:48 mostek Exp $"

/*
 * fs/cell/cxfs_intfc.h -- Special cxfs interface routines for cfs
 *
 * This header defines the special interface provided by cxfs to cfs.
 * It should only be included by cfs and cxfs-v1 code.
 */

#include <sys/vnode.h>
#include <ksys/cell/tkm.h>
#include <fs/cell/fsc_types.h>

/*** CLIENT ****/

/*
 * Called from cfs (DC) when creating a new dcvn.
 */
extern dcxvn_t *cxfs_dcxvn_make(dcvn_t *, dcxvfs_t *, vattr_t *, tkc_state_t *,
		int, char *, size_t);

/*
 * Called from cfs (DC) after RPC which obtains a token from the server.
 * The token RPC might also return some objects.
 */
extern void    cxfs_dcxvn_obtain(dcxvn_t *, tk_set_t, tk_set_t, tk_set_t, tk_set_t,
				int, char *, size_t);

/*
 * Called from cfs (DC) when a token is being recalled and we are in
 * the client's RPC server routine called via the token callout from
 * DS. It must return the tokens that CFS should continue dealing with.
 */
extern tk_set_t    cxfs_dcxvn_recall(dcxvn_t *, tk_set_t, tk_disp_t);

/*
 * Called from cfs (DC) when a token is being revoked and we are in
 * the client *(tkc_return) entry point in DC.
 */
extern void    cxfs_dcxvn_return(dcxvn_t *, tk_set_t, tk_set_t, tk_disp_t);

/*
 * Called from cfs when a new mount point is being setup and there is xfs
 * specific data to insert into the dcvfs_t structure.
 */

extern dcxvfs_t *cxfs_dcvfs_make(vfs_t *, dcvfs_t *);
extern void cxfs_dcvfs_destroy(dcxvfs_t *);


/*
 * Called whenever a dcvn is destroyed.
 */
extern void cxfs_dcxvn_destroy(dcxvn_t *);

/* Debugging print routine */
extern void cxfs_prdcxvn(dcxvn_t *);

/*** SERVER ****/

/*
 * Called from cfs (DS) when the dsvn is created.
 */
extern dsxvn_t *cxfs_dsxvn_make(dsvn_t *, bhv_desc_t *, tks_state_t *,
			tkc_state_t *);

/*
 * Called from cfs (DS) when a token is obtained.
 */
extern int cxfs_dsxvn_obtain(dsxvn_t *, char **, size_t *, int *,
			tk_set_t, tk_set_t, tk_set_t, void **);

/*
 * Called from cfs (DS) via an RPC callout indicating a bufferof RPC reply is
 * done.
 */
void cxfs_dsxvn_obtain_done(char *, size_t, void *);

/*
 * Called from cfs (DS) when a token is returned.
 */
extern void cxfs_dsxvn_return(dsxvn_t *, cell_t, tk_set_t, tk_set_t, tk_disp_t);

/*
 * Called whenever a dsvn is destroyed.
 */
extern void cxfs_dsxvn_destroy(dsxvn_t *);

/* Debugging print routine */
extern void cxfs_prdsxvn(dsxvn_t *);
#endif /* _FS_CELL_CXFS_INTFC_H_ */
