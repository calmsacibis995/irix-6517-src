#ifndef __XLV_LAB_H__
#define __XLV_LAB_H__

/**************************************************************************
 *                                                                        *
 *           Copyright (C) 1993-1994, Silicon Graphics, Inc.              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.41 $"

/* Library routines for manipulating XLV volume manager disk labels. */

#ifndef MAXDEVPATHLEN
#define	MAXDEVPATHLEN	MAXPATHLEN
#endif


/*
 * We keep all the volume headers (vh) and disk labels in
 * core on a linked list.
 * The structure of the list elements are defined by xlv_lab1.
 * The list itself is owned by xlv_lab2.
 */
typedef struct xlv_vh_entry_s {
        struct xlv_vh_entry_s           *next;
        dev_t                           dev;
        char                            vh_pathname[MAXDEVPATHLEN];
        struct volume_header            vh;
        int                             vfd;
        int                             dev_bsize;
        /*
	 * The following is a pointer to a malloc'ed memory
         * containing the entire disk label - master block,
         * primary, secondary copy.
	 */
        int                             label_lbn;
        xlv_vh_master_t                 *xlv_label;
        xlv_vh_disk_label_t             *label0_p;
        xlv_vh_disk_label_t             *label1_p;

	/*
	 * xlv_tab_list is a linked list of references back
	 * to the xlv_tab and xlv_tab_vol tables that describe
	 * the structure of the logical volumes of which this
	 * is a part.  It is used by the routines in xlv_lab2
	 * when writing labels. 
	 *
	 * NOTE: The following should really be declared
	 *       explicitly as xlv_oref_t.  We don't do that
	 *	 because these 2 include files have mutual
	 *	 dependencies and there are environments (e.g.,
	 *	 the kernel, that do not need both.)
	 *
	 *  NOTE that xlv_lab1_done() assumes that the first
	 *  field of every list element is the pointer to the
	 *  next element in the list.
	 */

	void				*xlv_tab_list;
} xlv_vh_entry_t;


#define XLV__LAB1_PRIMARY(vh) \
 ( ((vh)->xlv_label->vh_seq_number & 0x1) ? (vh)->label1_p : (vh)->label0_p )
#define XLV__LAB1_SECONDARY(vh) \
 ( ((vh)->xlv_label->vh_seq_number & 0x1) ? (vh)->label0_p : (vh)->label1_p )

/*
 * Flags for reading the volume header list.
 */
#define	XLV_READ_DOMESTIC	0x1	/* locally configurated disks */
#define	XLV_READ_FOREIGN	0x2	/* configured on another system */
#define	XLV_READ_ALL	(XLV_READ_DOMESTIC|XLV_READ_FOREIGN)
#define	XLV_READ_CREATE_DEVICE	0x10	/* create volume device */
#define	XLV_READ_NO_CONVERT	0x20	/* create volume device */



/* 
 * The xlv_lab1 routines presents the abstraction of complete
 * labels.  The write routine will update the master block
 * after writing the label (synchronously) and will read the
 * master block to determine which label to read.
 *
 * These routines own the sequence numbers contained in the
 * master block and the header of each label.
 *
 * These routines are designed to be called from user-mode only.
 */

/*
 * Open the volume header given a path to the device special
 * file for the volume header partition.  It returns 0 on success.
 * This routine reads in the volume header plus the XLV label 
 * (if it exists) into the vh_entry.  
 * The fd to the volume header is kept open.
 * This routine must be called before any other xlv_lab1
 * entrypoints.
 */
extern int
xlv_lab1_open (
	char		*vh_pathname,
	xlv_tab_disk_part_t *dp,
	xlv_vh_entry_t	*vh_entry);

/*
 * Closes the file descriptor associated with this 
 * volume header and frees the vh_entry.
 */
extern void xlv_lab1_close (xlv_vh_entry_t **vh_entry);


/*
 * Allocates a new label (xlv_master, xlv_lab0, xlv_lab1)
 * on the disk and updates xlv_vh_entry.
 * The labels and the xlv master block sequence number are 
 * initialized to 0.
 */
extern int xlv_lab1_init (xlv_vh_entry_t *vh_entry);

/*
 * Writes the in-core master block to disk.
 */
extern int xlv_lab1_write_master_blk (xlv_vh_entry_t *vh_entry);

/*
 * Writes either the primary or secondary label to disk.
 * The label to be written is assumed to be pointed to by the
 * vh_entry->xlv_label argument.
 * Note that this operation does not update the master
 * block.
 */
extern int xlv_lab1_write (xlv_vh_entry_t *vh_entry);

/*
 * Deletes the XLV label from the disk volume header.
 * Does not check that the volume header handle is okay.
 */
extern int xlv_lab1_delete_label(xlv_vh_entry_t *vh_entry);

/*
 * Close the file descriptor associated with the underlying
 * device for this vh_entry.
 */
extern void xlv_lab1_reclaim_fd (xlv_vh_entry_t *vh_entry);

/*
 * Reopen a file descriptor that has been reclaimed.
 */
extern int xlv_lab1_restore_fd (xlv_vh_entry_t *vh_entry);

/*
 * Update the partition types in the volume header to tag them
 * as pieces of XLV volumes.
 */
#define XLV_MAX_DISKS 600		/* 40 channels * 15 units */
typedef struct {
	char				vh_pathname[MAXDEVPATHLEN];
	int				num_partitions;
	dev_t				partition[XLV_MAX_DISK_PARTS];
	int				partition_type[XLV_MAX_DISK_PARTS];
} xlv_ptype_update_list_t;

extern int xlv_lab1_update_ptype (xlv_ptype_update_list_t *ptype_update_list);

/*
 * -----------------------------------------------------------
 */
/*
 * The xlv_lab2 routines read and write xlv_tab entries to disk.
 * They hide the fact that these labels are distributed across
 * many disk parts.
 * Internally, these routines create the consistent label 
 * abstraction - eliminating labels that were not written to all
 * the disk parts that belong to a volume.
 * 
 * These routines own the header block and sequence number in
 * each label entry.
 */

/*
 * Mode of writing:
 *	FULL    - Writes must go to all disks. All or nothing.
 *	PARTIAL - Writes go to all disks accessable
 */
#define	XLV_LAB_WRITE_FULL	0x1	/* Must write to all disks */
#define	XLV_LAB_WRITE_PARTIAL	0x2	/* Okay to fail on some disks */

/*
 * Update the node name in the XLV disk label header on all the
 * disks that are associated with the list of objects.
 */
extern void xlv_lab2_write_nodename_oref (
	xlv_vh_entry_t	**vh_list,
	xlv_oref_t	*oref_list,
	xlv_name_t	nodename,
	int		*status);


/*
 * Look at an xlv_tab_vol_entry and write the labels that describe 
 * the geometry out to all the disk parts that make up the volume.
 * vh_list is the in-core list of all the xlv disk labels. They
 * are updated along with the on-disk structures.
 *
 * The obj_uuid describes the "scope" of the
 * configuration change that corresponds to a label update.
 * This allows xlv to update only the disk parts affected
 * instead of always updating every disk part in a volume.
 * The scope is hierarchical.  If the object affected is the
 * plex, for example, then all the volume elements are
 * automatically affected also.
 * 
 * This routine will also generate a new sequence number.
 */
extern void xlv_lab2_write (
	xlv_vh_entry_t      	**vh_list,
	xlv_tab_vol_entry_t     *tab_vol_entry,
	uuid_t			*obj_uuid,
	int                     *status);

/*
 * Takes 2 xlv_oref_t and write the geometry that describes
 * the objects to all the disk parts that make them up.
 * vh_list is the in-core list of all the xlv disk labels. They
 * are updated along with the on-disk structures.
 *
 * We take 2 objects because when we do dynamic change, we
 * may be moving a piece from one object to another.  (Since
 * each move operation is atomic, we can never have a operation
 * that involves more than 3 objects.)
 *
 * This routine will also generate a new sequence number.
 */
extern void xlv_lab2_write_oref (
	xlv_vh_entry_t          **vh_list,
        xlv_oref_t     		*oref1,
	xlv_oref_t		*oref2,
        int                     *status,
	int			mode);

/*
 * Takes the passed-in component, which is associated with the
 * xlv_oref_t, and writes its values to all the disk parts that
 * make up that component.
 *
 * This routine is called to change component (subvol, plex, ve)
 * specific values.
 *
 * This routine will also generate a new sequence number.
 */

extern void xlv_lab2_write_oref_component (
        xlv_vh_entry_t          **vh_list,
        xlv_oref_t              *oref1,
        xlv_obj_type_t          ocomponent1,
        xlv_oref_t              *oref2,
        xlv_obj_type_t          ocomponent2,
        int                     *status,
	int			mode);

/*
 * Purge all disk labels associated with the given component.
 *
 * This routine is called to change component (subvol, plex, ve)
 * specific values.
 *
 * This routine will also generate a new sequence number.
 */
extern void xlv_lab2_remove_oref_component (
        xlv_vh_entry_t          **vh_list,
        xlv_oref_t              *oref,
        xlv_obj_type_t          ocomponent,
        int                     *status,
	int			mode);

/*
 * Locates all the disks attached to the system, read their
 * labels, filters out partial updates, and create xlv_vol_tab
 * and xlv_tab tables that describes the geometry of all the 
 * logical volumes on this system.
 *
 * Error messages will be printed on the console if orphaned
 * disks are discovered.
 */
extern void xlv_lab2_read (
	xlv_vh_entry_t		**vh_list,	/* OUT */
	xlv_tab_vol_t		*xlv_tab_vol,	/* OUT */
	xlv_tab_t		*xlv_tab,	/* OUT */
	int			flags,		/* XLV_READ_ flags */
	int			*status);

/*
 * Sanity check the populated xlv_tab_vol and xlv_tab.
 * Equivalent of fsck.
 */
extern void xlv_lab2_check (
	xlv_vh_entry_t		*vh_list,
	xlv_tab_vol_t		*xlv_tab_vol_ptr,
	xlv_tab_t		*xlv_tab_ptr,
	int			*status);

/*
 * Locates all disks that belong to XLV and /or those that do not.
 * Read their labels into a list of vh_info's allocated from heap.
 *
 * May print errors on the console.
 */
extern void xlv_lab2_create_vh_info_from_hinv (
	xlv_vh_entry_t		**xlv_vh_list_p,	/* IN/OUT */
	xlv_vh_entry_t		**non_xlv_vh_list_p,	/* IN/OUT */
	int			*status);

/*
 * Clean up of the list of vh_info's allocated in
 * xlv_lab2_create_vh_info_from_hinv().
 */
extern void xlv_lab2_destroy_vh_info (
	xlv_vh_entry_t		**xlv_vh_list_p,	/* IN/OUT */
	int			*status);


/*
 * Free all the vh_entries in IO_vh_list.
 * Upon returning, *IO_vh_list is set to NULL.
 */
extern void xlv_lab2_free_vh_list (xlv_vh_entry_t **IO_vh_list);

#ifdef NOT_HWGRAPH
/*
 * Scan the list of volume headers and determine if any disk migrated.
 * If a disk has migrated, calculate the correct dev_t.
 */
extern void xlv_dev_make_consistent (xlv_vh_entry_t *vh_list, int *status);
#endif

/*
 * Scan the disk labels in disk_info.  If the last transaction was
 * not completed, finish the transaction.
 */
extern void xlv_lab2_make_consistent (xlv_vh_entry_t  *vh_list, int *status);


/*
 * Delete the XLV label from the given volume header.
 * Checks that the volume header handle is valid.
 */
extern int xlv_delete_xlv_label(xlv_vh_entry_t *vh, xlv_vh_entry_t *vh_list);


/*
 * -----------------------------------------------------------
 */

/*
 * Regenerate all the objects described by the volume labels in
 * vh_list. Return them in a linked list of object ref's.
 *
 * If tab_vol_p or tab_sv_p is not NULL, the referenced pointer is
 * set the malloc volume or subvolume table.
 *
 * Incomplete volumes are removed from the table when <filter> is set.
 */
extern int xlv_vh_list_to_oref (
	xlv_vh_entry_t	*vh_list,
	xlv_oref_t	**oref_list,
        xlv_tab_vol_t	**tab_vol_p,	/* OUT */
	xlv_tab_t	**tab_sv_p,	/* OUT */
	int		filter,
	int		flags);		/* XLV_READ_ flags */


/*
 * Create the volume and subvolume information that is described
 * by the disk labels in vh_list.
 */
extern void xlv_vh_to_xlv_tab (
        xlv_vh_entry_t	*vh_list,
        xlv_tab_vol_t	*tab_vol,
        xlv_tab_t     	*xlv_tab,
        xlv_vh_entry_t	**orphaned_vh_list,
	int		flags,		 	/* XLV_READ_ flags */
        int		*status);

/*
 * Return TRUE if the entry belongs to the local node
 */
extern boolean_t xlv_local_volume (
	xlv_tab_vol_entry_t *vol_p);

/*
 * Create the standalone plex and volume element oref's from the
 * disk labels in vh_list.
 * This routine will skip over vh_entries that belong to
 * volumes [and subvolumes].
 */
extern void xlv_vh_to_xlv_pieces (
	xlv_vh_entry_t	*vh_list,
	xlv_oref_t	**oref_list_p,
	int		*status,
	int		filter);	/* remove incomplete pieces */

/*
 * Create a block map that describes the range of blocks in a
 * given subvolume.
 */
extern xlv_block_map_t *xlv_tab_create_block_map (
	xlv_tab_subvol_t      *sv);

extern int xlv_convert_old_labels(xlv_vh_entry_t *vh_entry, int flags);

#endif /* __XLV_LAB_H__ */
