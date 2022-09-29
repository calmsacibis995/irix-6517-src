#ifndef __XLV_VH_H__
#define __XLV_VH_H__

/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.15 $"

/*
 * xlv_vh.h - typdefs for xlv volumes 
 */

/* Derived from NPARTAB */
#define XLV_MAX_DISK_PARTS 16

/*
 * Each physical disk may contain up to XLV_MAX_DISK_PARTS 
 * disk parts.  In V1, each disk part must be a partition.  
 * Each of these disk parts can be part of a different
 * logical volume.
 * 
 * Xlv keeps 2 copies of the xlv_label on each disk.
 * xlv_lab0 and xlv_lab1. Xlv uses header.vh_seq_number
 * modulo 2 to decide which copy to use.  All of this is kept
 * in the volume header partition (partition 0).
 *
 * An xlv_vh_disk_label_entry_t exists for each disk part on
 * this disk. However, to allow the user to change the sizes
 * and number of disk parts (partitions), xlv allocates the
 * maximum amount of space required.
 *
 */

/*
 * Master block. Determines which label is the current one.
 * This will be a separate "file" in the vh directory.
 * xlv_master.  The data itself will take up one sector,
 * zero-filled.
 */
typedef struct {
	__uint64_t		vh_seq_number;
	unsigned char		zero [512 - 8];
} xlv_vh_master_t;


#define XLV_VH_MAGIC_NUM	0x11201989
#define XLV_VH_VERS		0x0002

/* Header record that describes the label */

typedef struct {
        __uint32_t		magic;
	unsigned short		version;
        unsigned char		disk_parts_on_disk;
	unsigned char		reserved;
	__uint64_t		timestamp;		/* last mount time */

	/*
	 * The transaction_id represents the last
	 * transaction during which this label was written.
	 * Since not all disks will be written for every 
	 * transaction, this number will tend to jump.
	 */
	__uint64_t		transaction_id;

	/*
	 * Node name of machine that wrote this label last. 
	 * This is derived from _SYS_NMLN (sys/utsname.h) and
	 * is at least 257 characters to support Internet hostnames.
	 */
	char			nodename [XLV_NODENAME_LEN];
} xlv_vh_header_t;



/* Information about the volume and subvolume to
   which the current disk part belongs. */

typedef struct {
	__uint32_t	flag;			/* volume flag */
	uuid_t		volume_uuid;
	uuid_t		log_subvol_uuid;
	uuid_t		data_subvol_uuid;
	uuid_t		rt_subvol_uuid;
	xlv_name_t	volume_name;
	__uint32_t	subvol_flags;
	__uint32_t	subvol_padding;		/* reserved */	
	__uint32_t	subvol_timestamp;	/* subvol | plex last touched */
	__uint32_t	sector_size;		/* property of volume */
	unsigned char	subvolume_type;
	unsigned char	reserved;		/* reserved */
	unsigned short	subvol_depth;		/* number of ve rows */
} xlv_vh_vol_t;

/* Information that describes the plex to which
   the current disk part belongs.  */
 
typedef struct {
	uuid_t      	uuid;
	xlv_name_t  	name;
	unsigned char	num_plexes;		/* sibling plexes */
	unsigned char	seq_num;
	unsigned short	num_vol_elmnts;
	unsigned short	flags;
} xlv_vh_plex_t;

/* Information about the volume element. */
typedef struct {
	uuid_t		uuid;
	xlv_name_t	name;
	unsigned char	state;
	unsigned char	type;			/* stripe or concatenate */
	unsigned char	grp_size;
	unsigned char	seq_num_in_plex;
	unsigned short	stripe_unit_size;	/* track size if non-striped */
	unsigned char	reserved[2];
	__int64_t	start_block_no;
	__int64_t	end_block_no;
} xlv_vh_vol_elmnt_t;


/* Information about this disk part */
typedef struct {
        uuid_t          uuid;
	dev_t           dev;
	__uint32_t	size;			/* in blocks */
        unsigned char   seq_in_grp;
        unsigned char   state;			/* XLV_VH_DISK_PART_STATE_XXX */
} xlv_vh_disk_part_t;

#define XLV_VH_DISK_PART_STATE_GOOD	0
#define XLV_VH_DISK_PART_STATE_NODEV	1


/* One entry in the label */
typedef struct {
    xlv_vh_vol_t	vol;
    xlv_vh_plex_t	plex;
    xlv_vh_vol_elmnt_t	vol_elmnt;
    xlv_vh_disk_part_t	disk_part;
} xlv_vh_disk_label_entry_t;


typedef struct {
    xlv_vh_header_t		header;
    xlv_vh_disk_label_entry_t 	label_entry[XLV_MAX_DISK_PARTS];
} xlv_vh_disk_label_t;

#endif /* __XLV_VH_H__ */
