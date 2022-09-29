#ifndef	__XLV_LOWER_ADMIN_H__
#define	__XLV_LOWER_ADMIN_H__

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.7 $"

/*
 * Declarations for the XLV volume manager lower layer admin interface.
 * The lower layer deals with setting kernel configuration and
 * writing the disk labels to the disk volume header.
 */


/*
 * When the following functions returns a value < 0, an error has
 * occur and the caller should check xlv_lower_errno for the reason.
 */

extern int xlv_lower_errno;

#define	XLV_LOWER_ESYSERRNO	1	/* See libc's errno for reason */
#define	XLV_LOWER_EINVALID	2	/* Invalid argument */
#define	XLV_LOWER_EIO		3	/* I/O error */
#define	XLV_LOWER_EKERNEL	4	/* Kernel error */
#define	XLV_LOWER_ENOSUPPORT	5	/* Operation not supported */
#define	XLV_LOWER_ENOIMPL	6	/* Operation not implemented */
#define	XLV_LOWER_ESTALECURSOR	7	/* Stale cursor - kernel changed */

/*
 *	A d d   S e l e c t i o n s
 */

/*
 * Add a volume. This is an unsupported function.
 *
 * Returns -1 with xlv_lower_errno set to XLV_LOWER_ENOSUPPORT.
 */
extern int xlv_lower_add_vol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref);		/* resulting volume object */

/*
 * Add a subvolume to an existing volume.
 *
 * This is an unsupported function. Supporting this function implies
 * stand-alone subvolumes support. Adding a subvolume does not
 * make sense from a file system perspective because one must do a
 * mkfs offline and this function deals online functionality.
 *
 * Returns -1 with xlv_lower_errno set to XLV_LOWER_ENOSUPPORT.
 */
extern int xlv_lower_add_subvol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref);		/* resulting volume object */

/*
 * Add a plex to an existing subvol
 */
extern int xlv_lower_add_plex(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *I_oref);		/* resulting plex/vol object */

/*
 * Add a volume element to an existing plex
 */
extern int xlv_lower_add_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *I_oref);		/* resulting plex/vol object */

extern int xlv_lower_set_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *ve_oref);		/* ve being set */

/*
 * Add a partition to an existing ve
 *
 * This is an unsupported function. A volume element is the lowest
 * layer component that can be administered as this time.
 *
 * Returns -1 with xlv_lower_errno set to XLV_LOWER_ENOSUPPORT.
 */
extern int xlv_lower_add_partition(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *ve_oref);


/*
 *	D e t a c h   S e l e c t i o n s
 *
 * The object is removed from the kernel's configuration and
 * the disk label is clear of (sub)volume state information.
 * The object is now a stand-alone item. Caller does not free
 * the in-core object.
 */

/*
 * Detach a subvolume from an existing volume.
 *
 * This is an unsupported function. Supporting this function implies
 * stand-alone subvolumes support. 
 *
 * Returns -1 with xlv_lower_errno set to XLV_LOWER_ENOSUPPORT.
 */
extern int xlv_lower_detach_subvol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref,		/* resulting vol obj */
	xlv_oref_t	  *sv_oref);		/* removed subvol obj */

/*
 * Detach a plex from an existing subvolume.
 */
extern int xlv_lower_detach_plex(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *sv_oref,		/* resulting subvol obj */
	xlv_oref_t	  *p_oref);		/* removed plex obj */

/*
 * Detach a volume element from an existing plex. That plex
 * may be part of a subvolume (thus volume) or it may be
 * a stand-alone plex. Either case, the plex object must be
 * resolved to the corresponding level.
 */
extern int xlv_lower_detach_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *p_oref,		/* resulting plex obj */
	xlv_oref_t	  *ve_oref);		/* removed ve obj */

/*
 *	R e m o v e   S e l e c t i o n s
 *
 * The object is removed from the kernel's configuration and
 * the disk label is purge from the disk. Caller free the
 * in-core object.
 */

/*
 * Remove a volume.
 */
extern int xlv_lower_rm_vol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t    **vh_list,
	xlv_oref_t        *v_oref);              /* volume object */


/*
 * Remove a subvolume from an existing volume.
 */
extern int xlv_lower_rm_subvol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref,		/* resulting vol obj */
	xlv_oref_t	  *sv_oref);		/* removed subvol obj */

/*
 * Remove a plex from an existing subvolume.
 */
extern int xlv_lower_rm_plex(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *sv_oref,		/* resulting volume obj */
	xlv_oref_t	  *p_oref);		/* removed plex obj */

/*
 * Remove a volume element. The volume element may be a stand-alone ve
 * or it may be part of a plex. "I_oref" is nil if this is a stand-alone
 * volume element.
 */
extern int xlv_lower_rm_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *p_oref,		/* resulting plex obj */
	xlv_oref_t	  *ve_oref);		/* removed ve obj */

/*
 * Remove the component at the given path. The component can be a plex
 * or a volume element.
 */
extern int xlv_lower_rm_path(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *oref,		/* resulting obj */
	xlv_oref_t	  *it_oref,		/* component removed */
	xlv_attr_cursor_t *path);		/* path of component removed */


/*
 * Remove a partition from an existing volume element.
 *
 * This is an unsupported function. A volume element is the lowest
 * layer component that can be administered as this time.
 *
 * Returns -1 with xlv_lower_errno set to XLV_LOWER_ENOSUPPORT.
 */
extern int xlv_lower_rm_partition(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *ve_oref);


/*
 * Set the nodename in the XLV disk labels of all disks associated
 * with the given object(s)
 *
 * Returns 0 on success. 
 */
extern int xlv_lower_set_nodename(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	**vh_list,
	xlv_oref_t	*oref_list,
	xlv_name_t	nodename);


/*
 * Write out the label for the given oref.
 *
 * Returns 0 on success. 
 */
extern int xlv_lower_writelab(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t    **vh_list,
	xlv_oref_t        *oref);

/*
 * Change the name of the given object.
 *
 * Returns 0 on success. 
 */
extern int xlv_lower_rename(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t    **vh_list,
	xlv_oref_t        *oref,
	char              *newname);

extern int xlv_lower_vol(
	xlv_attr_cursor_t       *cursor,
	xlv_vh_entry_t          **vh_list,
	xlv_oref_t              *oref);


#endif	/* __XLV_LOWER_ADMIN_H__ */
