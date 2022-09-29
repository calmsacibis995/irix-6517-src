/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Id: hsmapi.h,v 1.1 1998/05/13 20:48:44 kcm Exp $"

#ifndef HSMAPI_H
#define HSMAPI_H

#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/stat.h>
#include <sys/fs/xfs_itable.h>
#include <sys/dmi.h>

#define	HSM_API_VERSION_1	1	/* only version supported so far */

typedef	void	hsm_fs_ctxt_t;		/* opaque HSM filesystem context */
typedef	void	hsm_f_ctxt_t;		/* opaque HSM file context */


/******************************************************************************
* Name
*	HsmInitFsysContext - allocate and initialize an HSM filesystem context
*
* Description
*	HsmInitFsysContext allocates and initializes an HSM filesystem
*	context to hold all filesystem information that might later be needed
*	by the other HSM routines.  The context is read-only, and can be shared
*	by multiple xfsdump dump streams.  It should eventually be freed by
*	calling HsmDeleteFsysContext().  The caller must provide the mount
*	point of the filesystem to be dumped and the HSM API version that
*	xfsdump was compiled with.
*
* Returns
*	!= NULL, then a pointer to the filesystem context that was allocated.
*	== NULL, either the HSM libary is not compatible with xfsdump, or
*	      the filesystem is not under HSM management.
******************************************************************************/

extern hsm_fs_ctxt_t *
HsmInitFsysContext(
const	char		*mountpoint,
	int		dumpversion);


/******************************************************************************
* Name
*	HsmDeleteFsysContext - delete an HSM filesystem context
*
* Description
*	HsmDeleteFsysContext releases all storage previously allocated to a
*	HSM filesystem context via HsmInitFsysContext.
*
* Returns
*	None.
******************************************************************************/

extern void
HsmDeleteFsysContext(
	hsm_fs_ctxt_t	*contextp);


/******************************************************************************
* Name
*	HsmEstimateFileSpace - return estimated offline file size
*
* Description
*	HsmEstimateFileSpace is called from within estimate_dump_space() only
*	if -a is selected.  It estimates the number of bytes needed to dump
*	the file assuming that all dual-residency data will be dumped as holes.
*
* Returns
*	!= 0, then *bytes contains the estimated size of the file in bytes.
*	== 0, then no estimate made.  Caller should use his default estimator.
******************************************************************************/

extern int
HsmEstimateFileSpace(
	hsm_fs_ctxt_t	*contextp,
const	xfs_bstat_t	*statp,
	off64_t		*bytes);


/******************************************************************************
* Name
*	HsmEstimateFileOffset - return estimated file offset
*
* Description
*	HsmEstimateFileOffset is called from within quantity2offset() only
*	if -a is selected.  It estimates the offset within the file that has
*	'bytecount' bytes of physical data preceding it assuming that all
*	dual-residency data in the file will be dumped as holes.
*
* Returns
*	!= 0, then *byteoffset contains the estimated offset within the file.
*	== 0, then no estimate made.  Caller should use his default estimator.
******************************************************************************/

extern int
HsmEstimateFileOffset(
	hsm_fs_ctxt_t	*contextp,
const	xfs_bstat_t	*statp,
	off64_t		bytecount,
	off64_t		*byteoffset);


/******************************************************************************
* Name
*	HsmAllocateFileContext - allocate an HSM file context
*
* Description
*	HsmAllocateFileContext mallocs the maximum-sized file context that
*	might later be needed by HsmInitFileContext().  The context is
*	read-write.  Each xfsdump stream must have its own file context.  This
*	context should eventually be freed by calling HsmDeleteFileContext().
*	The caller must provide the HSM filesystem context for the filesystem
*	being dumped.
*
* Returns
*	!= NULL, then a pointer to the file context that was allocated.
******************************************************************************/

extern hsm_f_ctxt_t *
HsmAllocateFileContext(
	hsm_fs_ctxt_t	*contextp);


/******************************************************************************
* Name
*	HsmDeleteFileContext - delete a previously created HSM file context
*
* Description
*	HsmDeleteFileContext releases all storage previously allocated to a
*	HSM file context via HsmAllocateFileContext.
*
* Returns
*	None.
******************************************************************************/

extern void
HsmDeleteFileContext(
	hsm_f_ctxt_t	*contextp);


/******************************************************************************
* Name
*	HsmInitFileContext - initialize the HSM context for a particular file
*
* Description
*	HsmInitFileContext initializes an existing HSM file context for
*	subsequent operations on a particular regular file.  Other HSM routines
*	use the context to access information collected by HsmInitFileContext
*	about the file rather than having to recollect the file's information
*	on each call.
*
* Returns
*	!= 0, context was created.
*	== 0, if something is wrong with the file and it should not be dumped.
******************************************************************************/

extern int
HsmInitFileContext(
	hsm_f_ctxt_t	*contextp,
const	xfs_bstat_t	*statp);


/******************************************************************************
* Name
*	HsmModifyInode - modify a xfs_bstat_t to make a file appear offline
*
* Description
*	HsmModifyInode uses the context provided by a previous
*	HsmInitFileContext call to determine how to modify a xfs_bstat_t
*	structure to make a dual-residency HSM file appear to be offline.
*
* Returns
*	!= 0, xfs_bstat_t structure was modified.
*	== 0, if something is wrong with the file and it should not be dumped.
******************************************************************************/

extern int
HsmModifyInode(
	hsm_f_ctxt_t	*contextp,
	xfs_bstat_t	*statp);


/******************************************************************************
* Name
*	HsmModifyExtentMap - modify getbmapx array to make file appear offline
*
* Description
*	HsmModifyExtentMap uses the context provided by a previous
*	HsmInitFileContext call to determine how to modify a contiguous array
*	of getbmapx structures to make a dual-residency HSM file appear to
*	be offline.
*
* Returns
*	!= 0, getbmapx array was successfully modified.
*	== 0, if something is wrong with the file and it should not be dumped.
******************************************************************************/

extern int
HsmModifyExtentMap(
	hsm_f_ctxt_t	*contextp,
	struct getbmapx	*bmap);


/******************************************************************************
* Name
*	HsmFilterExistingAttribute - filter out unwanted extended attributes
*
* Description
*	HsmFilterExistingAttribute uses the context provided by a previous
*	HsmInitFileContext call to determine whether or not the extended
*	attribute with name 'namep' should be included in a file's dump image.
*	(An extended attribute can be modified within the dump by filtering
*	it out with this routine, then adding the new version of the attribute
*	back with HsmAddNewAttribute.)
*
*	Note: this routine must be idempotent.  It is possible that xfsdump
*	will call this routine a second time for the same attribute if after
*	the first call it discovers that there isn't room in its buffer to
*	hold the attribute value.
*
* Returns
*	!= 0, the attribute was successfully examined.  If '*skip_entry' is
*	      non-zero, xfsdump will not add this attribute to the dump.
*	== 0, if something is wrong with the file and it should not be dumped.
******************************************************************************/

extern int
HsmFilterExistingAttribute(
	hsm_f_ctxt_t	*hsm_f_ctxtp,
const	char		*namep,		/* name of attribute to filter */
	u_int32_t	valuesz,	/* attribute's current value size */
	int		isroot,		/* != 0, an ATTR_ROOT attribute */
	int		*skip_entry);


/******************************************************************************
* Name
*	HsmAddNewAttribute - add zero or more HSM attributes to a file's dump
*
* Description
*	HsmAddNewAttribute uses the context provided by a previous
*	HsmInitFileContext call to determine whether or not additional HSM
*	extended attributes need to be added to a file's dump image.  On the
*	first call for a file, 'cursor' will be zero.  xfsdump will increment
*	'cursor' by one each time it asks for a new attribute.  When no more
*	attributes are to be added, '*namepp' should be set to NULL.
*
*	Note: this routine must be idempotent.  It is possible that xfsdump
*	will call this routine a second time using the same cursor value if
*	it discovers that there isn't room in its buffer to hold the attribute
*	value it was given in the first call.
*
* Returns
*	!= 0, call was successful.  If '*namepp' is non-NULL, then it is the
*	      name of an attribute to be added to the file's dump.  '*valuep'
*	      points the the value of the attribute, and '*valueszp' is the
*	      value's size.  If '*namep* is NULL, then there are no more
*	      attributes to be added.
*	== 0, if something is wrong with the file and it should not be dumped.
******************************************************************************/

extern int
HsmAddNewAttribute(
	hsm_f_ctxt_t	*hsm_f_ctxtp,
	int		cursor,
	int		isroot,		/* != 0, wants ATTR_ROOT attributes */
	char		**namepp,	/* pointer to new attribute name */
	char		**valuepp,	/* pointer to its value */
	u_int32_t	*valueszp);	/* pointer to the value size */


#endif	/* HSMAPI_H */
