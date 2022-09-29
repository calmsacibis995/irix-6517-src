/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1988, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "xfs_info_defs.h: $Revision: 1.5 $"

#define GET_OBJ_BUF_SIZE        BUFSIZ

/* RPC timeout value for xfs calls() */
#define	XFS_RPC_TIMEOUT_VAL	60

#define	XFS_INFO_SCSIDRIVE	"dks"
#define	XFS_INFO_DKIPDRIVE	"ips"
#define	XFS_INFO_SMDDRIVE	"xyl"
#define	XFS_INFO_IPIDRIVE	"ipi"
#define	XFS_INFO_VSCSIDRIVE	"jag"
#define	XFS_INFO_SCSIRAIDDRIVE	"rad"

#define HOST_PATTERN_KEY        "HOST_PATTERN"
#define OBJ_PATTERN_KEY         "OBJ_PATTERN"
#define OBJTYP_PATTERN_KEY      "OBJ_TYPE"
#define QUERY_DEFN_KEY      	"QUERY_DEFN"

#define FS_TYPE                 "FS"
#define DISK_TYPE               "DISK"
#define VOL_TYPE                "VOL"

#define	LOG_SUBVOL_TYPE		"LOG"
#define	DATA_SUBVOL_TYPE	"DATA"
#define	RT_SUBVOL_TYPE		"RT"
#define	PLEX_TYPE		"PLEX"
#define	VE_TYPE			"VE"
#define	UNKNOWN_TYPE		"UNKNOWN"

/* Host to execute the xlv_make operation */
#define	XFS_HOSTNAME_STR	"XFS_HOST"

#define	XFS_INFO_FS_NAME	"FS_NAME"
#define	XFS_INFO_FS_PATH_PREFIX	"FS_DIR"
#define	XFS_INFO_FS_TYPE	"FS_TYPE"
#define	XFS_INFO_FS_OPTIONS	"FS_OPTIONS"
#define	XFS_INFO_FS_DUMP_FREQ	"FS_DUMP_FREQ"
#define	XFS_INFO_FS_PASS_NO	"FS_PASS_NO"

#define	XFS_INFO_MNT_FS_NAME	"MNT_NAME"
#define	XFS_INFO_MNT_DIR	"MNT_DIR"
#define	XFS_INFO_MNT_TYPE	"MNT_TYPE"
#define	XFS_INFO_MNT_OPTIONS	"MNT_OPTIONS"
#define	XFS_INFO_MNT_FREQ	"MNT_DUMP_FREQ"
#define	XFS_INFO_MNT_PASSNO	"MNT_PASS_NO"

#define	XFS_INFO_EXP_DIR	"EXP_DIR"
#define	XFS_INFO_EXP_OPTIONS	"EXP_OPTIONS"

#define	XFS_INFO_DISK_TYPE	"DISK_TYPE"
#define	XFS_INFO_DISK_NAME	"DISK_NAME"
#define	XFS_INFO_DISK_CYLS	"DISK_NO_CYLS"
#define	XFS_INFO_DISK_SEC_LEN	"DISK_SEC_LEN"
#define	XFS_INFO_DISK_TRKS_PER_CYL	"DISK_TRKS_PER_CYL"
#define	XFS_INFO_DISK_SECS_PER_TRK	"DISK_SECS_PER_TRK"
#define	XFS_INFO_DISK_SPARES_CYL	"DISK_SPARES_CYL"
#define	XFS_INFO_DISK_INTERLV_FLAG	"DISK_INTERLV_FLAG"
#define	XFS_INFO_DISK_BBLK	"DISK_BAD_BLKS"
#define	XFS_INFO_DISK_DRIVECAP	"DISK_DRIVECAP"
#define	XFS_INFO_DISK_MBSIZE	"DISK_MBSIZE"

#define	XFS_INFO_DISK_SCSI	"SCSI"
#define	XFS_INFO_DISK_DKIP	"DKIP"
#define	XFS_INFO_DISK_SMD	"xy logics SMD"
#define	XFS_INFO_DISK_IPI	"xy logics IPI"
#define	XFS_INFO_DISK_VSCSI	"VSCSI"
#define	XFS_INFO_DISK_SCSIRAID	"SCSIRAID"

#define	XFS_INFO_DISK_INTERLV_DP_SECTSLIP	"DP_SECTSLIP"
#define	XFS_INFO_DISK_INTERLV_DP_SECTFWD	"DP_SECTFWD"
#define	XFS_INFO_DISK_INTERLV_DP_TRKFWD		"DP_TRKFWD"
#define	XFS_INFO_DISK_INTERLV_DP_MULTIVOL	"DP_MULTIVOL"
#define	XFS_INFO_DISK_INTERLV_DP_IGNOREERRORS	"DP_IGNOREERRORS"
#define	XFS_INFO_DISK_INTERLV_DP_RESEEK		"DP_RESEEK"
#define	XFS_INFO_DISK_INTERLV_DP_CTQ_EN		"DP_CTQ_EN"

#define	XFS_INFO_VOL_NAME	"VOLUME_NAME"
#define	XFS_INFO_VOL_FLAGS	"VOLUME_FLAGS"
#define	XFS_INFO_VOL_STATE	"VOLUME_STATE"

#define	XFS_INFO_SUBVOL_FLAGS	"SUBVOLUME_FLAGS"
#define	XFS_INFO_SUBVOL_TYPE	"SUBVOLUME_TYPE"
#define	XFS_INFO_SUBVOL_SIZE	"SUBVOLUME_SIZE"
#define XFS_INFO_SUBVOL_NO_PLEX	"SUBVOLUME_NO_PLEX"
#define XFS_INFO_SUBVOL_INL_RD_SLICE	"SUBVOLUME_INL_RD_SLICE"

#define XFS_INFO_PLEX_NAME	"PLEX_NAME"
#define XFS_INFO_PLEX_FLAGS	"PLEX_FLAGS"
#define XFS_INFO_PLEX_NO_VE	"PLEX_NO_VE"
#define XFS_INFO_PLEX_MAX_VE	"PLEX_MAX_VE"

#define XFS_INFO_VE_NAME	"VE_NAME"
#define XFS_INFO_VE_TYPE	"VE_TYPE"
#define XFS_INFO_VE_STATE	"VE_STATE"
#define XFS_INFO_VE_GRP_SIZE	"VE_GRP_SIZE"
#define XFS_INFO_VE_STRIPE_UNIT_SIZE	"VE_STRIPE_UNIT_SIZE"
#define	XFS_INFO_VE_START_BLK_NO	"VE_START_BLK_NO"
#define	XFS_INFO_VE_END_BLK_NO		"VE_END_BLK_NO"


/* Function Prototypes */
extern char* get_partition_info(char* buffer,char** msg);
extern int update_partition_table(char* buffer,char** msg);

extern int xfsCreateInternal(const char* parameters, char **msg);
extern int xfsEditInternal(const char* parameters, char **msg);
extern int xfsExportInternal(const char* parameters, char **msg);
extern int xfsUnexportInternal(const char* parameters, char **msg);
extern int xfsMountInternal(const char* parameters, char **msg);
extern int xfsUnmountInternal(const char* parameters, char **msg);
extern int xfsDeleteInternal(const char* parameters, char **msg);
extern int xfsGetDefaultsInternal(const char *,char **, char **);
extern int xfsGetExportInfoInternal(const char *, char **, char **);
extern int xfsSimpleFsInfoInternal(const char *, char **, char **);

extern int getObjectsInternal(const char *criteria, char **objs, char **msg);
extern int getInfoInternal(const char *objectId, char **info, char **msg);
extern int xfsGetHostsInternal(const char* parameters, char **info, char **msg);
extern int xfsGroupInternal(const char* parameters, char **info, char **msg);
extern int xfsGetBBlkInternal(const char* parameters, char **info, char **msg);
extern int xfsChkMountsInternal(const char *parameters, char **info, char **msg);

extern int xfsXlvCmdInternal(const char* parameters, char **info, char **msg);
extern int xlvCreateInternal(const char* parameters, char **msg);
extern int xlvDeleteInternal(const char* parameters, char **msg);
extern int xlvDetachInternal(const char* parameters, char **msg);
extern int xlvAttachInternal(const char* parameters, char **msg);
extern int xlvRemoveInternal(const char* parameters, char **msg);
extern int xlvSynopsisInternal(const char* parameters, char **msg);
extern int xlvInfoInternal(const char* parameters, char **msg);
extern int getPartsUseInternal(const char* parameters, char **msg);
