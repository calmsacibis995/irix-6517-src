#ifndef __SYS_XLV_BASE_H__
#define __SYS_XLV_BASE_H__

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
#ident "$Revision: 1.18 $"

#include <sys/param.h>

/*
 * Primitive datatypes used by the XLV volume manager.
 */

#define XLV_NAME_LEN 32
typedef char xlv_name_t[XLV_NAME_LEN];

#define XLV_MAX_DISK_PARTS_PER_VE 100
#define XLV_MAX_VE_PER_PLEX	  128
#define XLV_MAX_PLEXES		    4
#define XLV_MAX_VE_DEPTH	  (XLV_MAX_VE_PER_PLEX * XLV_MAX_PLEXES)

#define MAX_DEVS XLV_MAX_PLEXES*XLV_MAX_VE_PER_PLEX*XLV_MAX_DISK_PARTS_PER_VE
#define MAX_PDEV_NODES (2*MAX_DEVS)+1

#define XLV_SUBVOL_TYPE_LOG	1
#define XLV_SUBVOL_TYPE_DATA	2
#define XLV_SUBVOL_TYPE_RT	3
#define XLV_SUBVOLS_PER_VOL	3
#define XLV_SUBVOL_TYPE_UNKNOWN 5

/* could be full hardware graph path name */
#define XLV_MAXDEVPATHLEN	MAXPATHLEN

#define XLV_NODENAME_LEN 258	/* Derived from _SYS_NMLN in <sys/utsname.h> */

#define XLV_VE_TYPE_NONE	0
#define XLV_VE_TYPE_STRIPE	1
#define XLV_VE_TYPE_CONCAT	2

#define XLV_VE_STATE_EMPTY	0
#define XLV_VE_STATE_CLEAN	1
#define XLV_VE_STATE_ACTIVE	2
#define XLV_VE_STATE_STALE	3
#define XLV_VE_STATE_OFFLINE	4
#define XLV_VE_STATE_INCOMPLETE 5

/*
 * Values assigned to the volume state are in increasing severity.
 */
#define XLV_VOL_STATE_NONE		      0	    /* no subvolumes */
#define XLV_VOL_STATE_COMPLETE		      1	    /* all pieces present */
#define XLV_VOL_STATE_MISS_COMMON_PIECE	      2	    /* missing mirrored piece */
#define XLV_VOL_STATE_ALL_PIECES_BUT_HOLEY    3	    /* hole in address space */
#define XLV_VOL_STATE_MISS_UNIQUE_PIECE	      4     /* hole in address space */
#define XLV_VOL_STATE_MISS_REQUIRED_SUBVOLUME 5     /* Incomplete volume */

#define XLV_OBJ_TYPE_NONE		0
#define XLV_OBJ_TYPE_VOL		1
#define XLV_OBJ_TYPE_SUBVOL		2
#define XLV_OBJ_TYPE_PLEX		3
#define XLV_OBJ_TYPE_VOL_ELMNT		4
#define XLV_OBJ_TYPE_DISK_PART		5
#define XLV_OBJ_TYPE_LOG_SUBVOL		6
#define XLV_OBJ_TYPE_DATA_SUBVOL	7
#define XLV_OBJ_TYPE_RT_SUBVOL		8

typedef int xlv_obj_type_t;

#ifndef NULL
#define NULL 0L
#endif /* NULL */


/*
 * Setting the xlv table via xlv_tab_set() or syssgi(SGI_XLV_SET_TAB),
 * can return one of the following error:
 *
 * 		EPERM, E2BIG, EINVAL, EACCESS, EBUSY
 *
 * or one of the following error code:
 */
#define	XLVE_BASE		200
#define	XLVE_BLOCK_MAP_INVAL	200
#define	XLVE_SUBVOL_TYPE_INVAL	201
#define	XLVE_NUM_PLEXES_INVAL	202
#define	XLVE_NUM_VES_INVAL	203
#define	XLVE_VOL_VERS_INVAL	204
#define	XLVE_SUBVOL_VERS_INVAL	205
#define	XLVE_SUBVOL_INCOMPLETE	206

#endif /* __SYS_XLV_BASE_H__ */
