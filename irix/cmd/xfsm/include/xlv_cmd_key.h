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

#ident "xlv_cmd_key.h: $Revision: 1.1 $"

/*
 *	The definition of keywords used for xfsXlvCmd() function are listed
 *	here.
 */

#define	XFS_XLV_CMD_STR			"XFS_XLV_CMD"

/* The different xlv commands */
#define	XLV_CREATE_OBJ_STR		"XLV_CREATE_OBJ"
#define	XLV_MODIFY_OBJ_STR		"XLV_MODIFY_OBJ"
#define	XLV_MODIFY_OBJ_STATE_STR	"XLV_MODIFY_OBJ_STATE"
#define	XLV_DEL_OBJ_STR			"XLV_DEL_OBJ"
#define	XLV_DEL_LABEL_STR		"XLV_DEL_LABEL"

/* Keywords for parameters common to all xlv commands */
#define	XLV_OBJ_ID_STR			"XLV_OBJ_ID"
#define	XLV_OBJ_NAME_STR		"XLV_OBJ_NAME"
#define	XLV_OBJ_TYPE_STR		"XLV_OBJ_TYPE"

/* Keywords for the parameters for creating an XLV object */ 
#define	XLV_OBJ_DATA_STR		"XLV_OBJ_DATA"

/* Keywords for the parameters for modifying an XLV object */ 
#define	XLV_OBJ_NAME2_STR		"XLV_OBJ_NAME2"
#define	XLV_OBJ_LOC_STR			"XLV_OBJ_LOC"
#define	XLV_MODIFY_OBJ_OP_STR		"XLV_MODIFY_OBJ_OP"

/* The following operation codes are defined for modifying an XLV object */
#define	ADD_VE_STR			"ADD_VE"
#define	ADD_PL_STR			"ADD_PL"
#define	ADD_SV_STR			"ADD_SV"
#define	ADD_PT_STR			"ADD_PT"
#define	DET_VE_STR			"DET_VE"
#define	DET_PL_STR			"DET_PL"
#define	DET_SV_STR			"DET_SV"
#define	DET_PT_STR			"DET_PT"
#define	REM_VE_STR			"REM_VE"
#define	REM_PL_STR			"REM_PL"
#define	REM_SV_STR			"REM_SV"
#define	REM_PT_STR			"REM_PT"

/* Keywords for the parameters for modifying an XLV object state */ 
#define	XLV_OBJ_STATE_STR		"XLV_OBJ_STATE"

/* Keywords to specify the XLV object to be created */
#define	XLV_BEGIN_VOL_STR		"BEGIN_VOL"
#define	XLV_END_VOL_STR			"END_VOL"

#define	XLV_BEGIN_SUBVOL_STR		"BEGIN_SUBVOL"
#define	XLV_SUBVOL_TYPE_STR		"SUBVOL_TYPE"
#define	XLV_SUBVOLUME_NO_PLEX		"NO_PLEX" 
#define	XLV_END_SUBVOL_STR		"END_SUBVOL"

#define	XLV_BEGIN_PLEX_STR		"BEGIN_PLEX"
#define	XLV_PLEX_NO_VE_STR		"NO_VE"
#define	XLV_END_PLEX_STR		"END_PLEX"

#define	XLV_BEGIN_VE_STR		"BEGIN_VE"
#define	XLV_VE_TYPE_STR			"VE_TYPE"
#define XLV_VE_STRIPE_UNIT_SIZE_STR	"VE_STRIP_SIZE"
#define XLV_VE_PARTITION_STR		"VE_PART"
#define XLV_VE_START_BLK_NO_STR		"VE_START_BLK"
#define XLV_VE_END_BLK_NO_STR		"VE_END_BLK"
#define	XLV_END_VE_STR			"END_VE"

/* Structure to hold all the keywords for defining an XLV object */
struct xlv_obj_keywords {
	char *keyword;
} xlv_obj_keytab[] = {
	XLV_BEGIN_VOL_STR,
	XLV_END_VOL_STR,	
	XLV_BEGIN_SUBVOL_STR,
	XLV_SUBVOL_TYPE_STR,
	XLV_SUBVOLUME_NO_PLEX,
	XLV_END_SUBVOL_STR,
	XLV_BEGIN_PLEX_STR,
	XLV_PLEX_NO_VE_STR,
	XLV_END_PLEX_STR,
	XLV_BEGIN_VE_STR,
	XLV_VE_TYPE_STR	,
	XLV_VE_STRIPE_UNIT_SIZE_STR,
	XLV_VE_PARTITION_STR,
	XLV_VE_START_BLK_NO_STR,
	XLV_VE_END_BLK_NO_STR,
	XLV_END_VE_STR
};

#define	XLV_OBJ_KEYTAB_SIZE (sizeof(xlv_obj_keytab)/sizeof(xlv_obj_keytab[0]))

