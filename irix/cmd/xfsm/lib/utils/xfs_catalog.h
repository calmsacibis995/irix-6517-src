#ifndef _xfs_catalog_h
#define	_xfs_catalog_h

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
#ident "$Revision: 1.1 $"

enum {
	CAT_DK_1N = 1,
	CAT_DK_2N,
	CAT_DK_3N,
	CAT_DK_4N,
	CAT_DK_5N,
	CAT_DK_6N,
	CAT_DK_7N,
	CAT_DK_9N,
	CAT_DK_9N,
	CAT_DK_10N,
	CAT_DK_11N,

	CAT_FS_1N,
	CAT_FS_2N,
	CAT_FS_3N,
	CAT_FS_4N,
	CAT_FS_5N,
	CAT_FS_6N,
	CAT_FS_7N,
	CAT_FS_8N,
	CAT_FS_9N,
	CAT_FS_10N,
	CAT_FS_11N,
	CAT_FS_12N,
	CAT_FS_13N,
	CAT_FS_14N,
	CAT_FS_15N,
	CAT_FS_16N,
	CAT_FS_17N,
	CAT_FS_18N,
	CAT_FS_19N,
	CAT_FS_20N,
	CAT_FS_21N,
	CAT_FS_22N,
	CAT_FS_23N,
	CAT_FS_24N,
	CAT_FS_25N,
	CAT_FS_26N,
	CAT_FS_27N,
	CAT_FS_28N,
	CAT_FS_29N,
	CAT_FS_30N,
	CAT_FS_31N,
	CAT_FS_32N,
	CAT_FS_33N,
	CAT_FS_34N,
	CAT_FS_35N,
	CAT_FS_36N,
	CAT_FS_37N,
	CAT_FS_38N,
	CAT_FS_39N,
	CAT_FS_40N,
	CAT_FS_41N,
	CAT_FS_42N,
	CAT_FS_43N,
	CAT_FS_44N,
	CAT_FS_45N,
	CAT_FS_46N,
	CAT_FS_47N,
	CAT_FS_48N,
	CAT_FS_49N,
	CAT_FS_50N,
	CAT_FS_51N,
	CAT_FS_52N,
	CAT_FS_53N,
	CAT_FS_54N,
	
	CAT_LV_1N,

	XFS_CAT_MAX
};

#define	CAT_DK_1	"%s: Cannot access disk.\n%s\n"
#define	CAT_DK_2	"%s: Cannot access disk.\n"
#define	CAT_DK_3	"%s: Cannot find equivalent char device.\n"
#define	CAT_DK_4	"%s: Not a device file.\n"
#define	CAT_DK_5	"%s: Error opening volume.\n%s\n"
#define	CAT_DK_6	"%s: Error reading volume.\n%s\n"
#define	CAT_DK_7	"%s: The magic number is incorrect.\n"
#define	CAT_DK_8	"%s: The checksum of the volume header is incorrect.\n"
#define	CAT_DK_9	"%s: Missing volume header partition.\n"
#define	CAT_DK_10	"%s: Seek to sector 0 failed.\n"
#define	CAT_DK_11	"%s: Unable to write volume header.\n%s\n"






#endif	/* _xfs_catalog_h */
