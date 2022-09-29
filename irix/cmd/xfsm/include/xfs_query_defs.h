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
#ident "$Revision: 1.2 $"

struct xfs_query_attr_list {
        char* attribute_name;
        int attribute_id;
} xfs_query_attr_tab[] = {

        "XLV_VOLUME_NAME", XLV_VOLUME_NAME,
        "XLV_VOLUME_FLAGS", XLV_VOLUME_FLAGS,
        "XLV_VOLUME_STATE", XLV_VOLUME_STATE,
        "XLV_SUBVOLUME_FLAGS", XLV_SUBVOLUME_FLAGS,
        "XLV_SUBVOLUME_TYPE", XLV_SUBVOLUME_TYPE,
        "XLV_SUBVOLUME_SIZE", XLV_SUBVOLUME_SIZE,
        "XLV_SUBVOLUME_INL_RD_SLICE", XLV_SUBVOLUME_INL_RD_SLICE,
        "XLV_SUBVOLUME_NO_PLEX", XLV_SUBVOLUME_NO_PLEX,
        "XLV_PLEX_NAME", XLV_PLEX_NAME,
        "XLV_PLEX_FLAGS", XLV_PLEX_FLAGS,
        "XLV_PLEX_NO_VE", XLV_PLEX_NO_VE,
        "XLV_PLEX_MAX_VE", XLV_PLEX_MAX_VE,
        "XLV_VE_NAME", XLV_VE_NAME,
        "XLV_VE_TYPE", XLV_VE_TYPE,
        "XLV_VE_GRP_SIZE", XLV_VE_GRP_SIZE,
        "XLV_VE_STRIPE_UNIT_SIZE", XLV_VE_STRIPE_UNIT_SIZE,
        "XLV_VE_STATE", XLV_VE_STATE,
        "XLV_VE_START_BLK_NO", XLV_VE_START_BLK_NO,
        "XLV_VE_END_BLK_NO", XLV_VE_END_BLK_NO,

        "FS_MOUNTED_STATE", FS_MOUNTED_STATE,
        "FS_EXPORTED_STATE", FS_EXPORTED_STATE,
        "FS_TYPE_INFO", FS_TYPE_INFO,
        "FS_MOUNT_OPTIONS", FS_MOUNT_OPTIONS,
        "FS_EXPORT_OPTIONS", FS_EXPORT_OPTIONS,
        "FS_MOUNT_FREQ", FS_MOUNT_FREQ,
        "FS_NAME", FS_NAME,
        "FS_MNT_DIR_NAME", FS_MNT_DIR_NAME,

        "DISK_NO_CYLINDERS", DISK_NO_CYLINDERS,
        "DISK_NO_TRACKS_PER_CYLINDER", DISK_NO_TRACKS_PER_CYLINDER,
        "DISK_NO_SECTORS_PER_TRACK", DISK_NO_SECTORS_PER_TRACK,
        "DISK_SECTOR_LENGTH", DISK_SECTOR_LENGTH,
        "DISK_SECTOR_INTERLEAVE", DISK_SECTOR_INTERLEAVE,
        "DISK_CTLR_FLAG", DISK_CTLR_FLAG,
        "DISK_NAME", DISK_NAME,
        "DISK_DRIVECAP", DISK_DRIVECAP

};

#define XFS_QUERY_ATTR_LEN (sizeof(xfs_query_attr_tab) / sizeof(xfs_query_attr_tab[0]))

