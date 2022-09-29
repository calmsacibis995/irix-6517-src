/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

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

#ifndef _xfs_fs_defs_h_
#define	_xfs_fs_defs_h_

#ident  "xfs_fs_defs.h: $Revision: 1.1 $"

#include <mntent.h>
#include <exportent.h>

#define FSTAB			"/etc/fstab"
#define MTAB			"/etc/mtab"
#define EXTAB			"/etc/exports"
#define XTAB			"/etc/xtab"

#define	EFS_FS_TYPE		"efs"
#define	XFS_FS_TYPE		"xfs"
#define	ISO9660_FS_TYPE		"iso9660"
#define	NFS_FS_TYPE		"nfs"
#define	SWAP_FS_TYPE		"swap"
#define	CACHE_FS_TYPE		"cache"

#define	XFS_MKFS_EFS_CMD	"/sbin/mkfs_efs"
#define	XFS_MKFS_XFS_CMD	"/sbin/mkfs_xfs"

#define POPEN_CMD_BUF_LEN	256

#define MOUNT_FS		0x01
#define EXPORT_FS		0x02
#define UNMOUNT_FS		0x01
#define UNEXPORT_FS		0x02

#define	IS_MOUNT_FLAG(flag)	(flag & MOUNT_FS)
#define	IS_EXPORT_FLAG(flag)	(flag & EXPORT_FS)
#define	IS_UNEXPORT_FLAG(flag)	(flag & UNEXPORT_FS)
#define	IS_UNMOUNT_FLAG(flag)	(flag & UNMOUNT_FS)

#define	DEL_FSTAB_ENTRY_FLAG	0x01
#define	DEL_EXPORT_ENTRY_FLAG	0x02

#define	IS_DEL_FSTAB_FLAG(flag)		(flag & DEL_FSTAB_ENTRY_FLAG)
#define	IS_DEL_EXPORT_FLAG(flag)	(flag & DEL_EXPORT_ENTRY_FLAG)

#define	FS_FLAG			0x01
#define	MNT_FLAG		0x02
#define	EXP_FLAG		0x04

#define	INVALID_FS_NAME_STR	"_NULL_"

#define	XFS_REG_UMOUNT_FS	0
#define	XFS_FORCE_UMOUNT_FS	1

#define	XFS_FS_NAME_STR			"XFS_FS_NAME"
#define	XFS_MNT_DIR_STR			"XFS_MNT_DIR"
#define	XFS_FS_TYPE_STR			"XFS_FS_TYPE"
#define	XFS_FS_DUMP_STR			"XFS_FS_DUMP_FREQ"
#define	XFS_FS_FSCK_PASS_STR		"XFS_FS_FSCK_PASS"
#define	XFS_RM_FSTAB_ENTRY_STR		"XFS_RM_FSTAB_ENTRY"
#define	XFS_RM_EXPORTS_ENTRY_STR	"XFS_RM_EXPORTS_ENTRY"
#define	XFS_MOUNT_FS_STR		"XFS_MOUNT_FS"
#define	XFS_EXPORT_FS_STR		"XFS_EXPORT_FS"
#define	XFS_UNEXPORT_FS_STR		"XFS_UNEXPORT_FS"
#define	XFS_REMOTE_HOST_STR		"XFS_REMOTE_HOST"
#define	XFS_UPDATE_FSTAB_STR		"XFS_UPDATE_FSTAB"
#define	XFS_UPDATE_EXPORTS_STR		"XFS_UPDATE_EXPORTS"

/* Keywords to create the mount directory */
#define	XFS_MNT_DIR_CREATE_STR		"XFS_MNT_DIR_CREATE"
#define	XFS_MNT_DIR_OWNER_STR		"XFS_MNT_DIR_OWNER"
#define	XFS_MNT_DIR_GROUP_STR		"XFS_MNT_DIR_GROUP"
#define	XFS_MNT_DIR_MODE_STR		"XFS_MNT_DIR_MODE"

/* Filesystem options */
struct fs_option {
	char*	str;		/* The option attribute name */
	char*	opts_str;	/* The option as represented in /etc/fstab */
	char*	true_bool_opts_str;
				/* The option corresponding to "true" boolean
				   value for the attribute */
	char*	false_bool_opts_str;
				/* The option corresponding to "false" boolean
				   value for the attribute */
	char*	default_opts_str;
				/* The default value for the attribute */
};

/* Filesystem options - Generic */
#define	XFS_FS_OPTS_RO_STR	"XFS_FS_OPTS_RO"
#define	XFS_FS_OPTS_NOAUTO_STR	"XFS_FS_OPTS_NOAUTO"
#define	XFS_FS_OPTS_GRPID_STR	"XFS_FS_OPTS_GRPID"
#define	XFS_FS_OPTS_NOSUID_STR	"XFS_FS_OPTS_NOSUID"
#define	XFS_FS_OPTS_NODEV_STR	"XFS_FS_OPTS_NODEV"
#define	XFS_FS_OPTS_RAW_STR	"XFS_FS_OPTS_RAW"

/* Filesystem options - xfs */
#define	XFS_FS_OPTS_DMI_STR	"XFS_FS_OPTS_DMI"

/* Filesystem options - efs */
#define	XFS_FS_OPTS_FSCK_STR	"XFS_FS_OPTS_FSCK"
#define	XFS_FS_OPTS_QUOTA_STR	"XFS_FS_OPTS_QUOTA"
#define	XFS_FS_OPTS_LBSIZE_STR	"XFS_FS_OPTS_LBSIZE"

/* Filesystem options - nfs */
#define	XFS_FS_OPTS_BG_STR	"XFS_FS_OPTS_BG"
#define	XFS_FS_OPTS_RETRY_STR	"XFS_FS_OPTS_RETRY"
#define	XFS_FS_OPTS_RSIZE_STR	"XFS_FS_OPTS_RSIZE"
#define	XFS_FS_OPTS_WSIZE_STR	"XFS_FS_OPTS_WSIZE"
#define	XFS_FS_OPTS_TIMEO_STR	"XFS_FS_OPTS_TIMEO"
#define	XFS_FS_OPTS_RETRANS_STR	"XFS_FS_OPTS_RETRANS"
#define	XFS_FS_OPTS_PORT_STR	"XFS_FS_OPTS_PORT"
#define	XFS_FS_OPTS_HARD_STR	"XFS_FS_OPTS_HARD"
#define	XFS_FS_OPTS_INTR_STR	"XFS_FS_OPTS_INTR"
#define	XFS_FS_OPTS_ACTIMEO_STR	"XFS_FS_OPTS_ACTIMEO"
#define	XFS_FS_OPTS_NOAC_STR	"XFS_FS_OPTS_NOAC"
#define	XFS_FS_OPTS_PRIV_STR	"XFS_FS_OPTS_PRIV"
#define	XFS_FS_OPTS_SYMTTL_STR	"XFS_FS_OPTS_SYMTTL"
#define	XFS_FS_OPTS_ACREGMIN_STR	"XFS_FS_OPTS_ACREGMIN"
#define	XFS_FS_OPTS_ACREGMAX_STR	"XFS_FS_OPTS_ACREGMAX"
#define	XFS_FS_OPTS_ACDIRMIN_STR	"XFS_FS_OPTS_ACDIRMIN"
#define	XFS_FS_OPTS_ACDIRMAX_STR	"XFS_FS_OPTS_ACDIRMAX"

/* Filesystem options - iso9660 */
#define	XFS_FS_OPTS_SETX_STR	"XFS_FS_OPTS_SETX"
#define	XFS_FS_OPTS_NTRANS_STR	"XFS_FS_OPTS_NTRANS"
#define	XFS_FS_OPTS_CACHE_STR	"XFS_FS_OPTS_CACHE"
#define	XFS_FS_OPTS_NOEXT_STR	"XFS_FS_OPTS_NOEXT"
#define	XFS_FS_OPTS_SUSP_STR	"XFS_FS_OPTS_SUSP"
#define	XFS_FS_OPTS_RRIP_STR	"XFS_FS_OPTS_RRIP"
#define	XFS_FS_OPTS_MMCNV_STR	"XFS_FS_OPTS_MMCNV"

/* Filesystem options - swap */
#define	XFS_FS_OPTS_PRI_STR	"XFS_FS_OPTS_PRI"
#define	XFS_FS_OPTS_SWPLO_STR	"XFS_FS_OPTS_SWPLO"
#define	XFS_FS_OPTS_LEN_STR	"XFS_FS_OPTS_LEN"
#define	XFS_FS_OPTS_MAXL_STR	"XFS_FS_OPTS_MAXL"
#define	XFS_FS_OPTS_VLEN_STR	"XFS_FS_OPTS_VLEN"

/* Filesystem options - CacheFS */
#define	XFS_FS_OPTS_BACKFS_STR	"XFS_FS_OPTS_BACKFS"
#define	XFS_FS_OPTS_BACKP_STR	"XFS_FS_OPTS_BACKP"
#define	XFS_FS_OPTS_CACHEDIR_STR	"XFS_FS_OPTS_CACHEDIR"
#define	XFS_FS_OPTS_CACHEID_STR	"XFS_FS_OPTS_CACHEID"
#define	XFS_FS_OPTS_WR_AD_STR	"XFS_FS_OPTS_WR_AD"
#define	XFS_FS_OPTS_NCONST_STR	"XFS_FS_OPTS_NCONST"
#define	XFS_FS_OPTS_LACCESS_STR	"XFS_FS_OPTS_LACCESS"
#define	XFS_FS_OPTS_PURGE_STR	"XFS_FS_OPTS_PURGE"
#define	XFS_FS_OPTS_SUID_STR	"XFS_FS_OPTS_SUID"


/* Filesystem export options keywords */
#define	XFS_EXPOPTS_RO_STR	"XFS_EXPOPTS_RO"
#define	XFS_EXPOPTS_RW_STR	"rw"
#define	XFS_EXPOPTS_ANON_STR	"XFS_EXPOPTS_ANON"
#define	XFS_EXPOPTS_ROOT_STR	"root"
#define	XFS_EXPOPTS_ACCESS_STR	"access"
#define	XFS_EXPOPTS_NOHIDE_STR	"XFS_EXPOPTS_NOHIDE"
#define	XFS_EXPOPTS_WSYNC_STR	"XFS_EXPOPTS_WSYNC"
#define	XFS_EXPOPTS_RWROOT_STR	"rw,root"


/* mkfs_xfs options pre-defined values */
#define	XFS_DATA_FILE_REGULAR_TYPE	"XFS_DATA_REGULAR"
#define	XFS_DATA_FILE_SPECIAL_TYPE	"XFS_DATA_SPECIAL"

/* mkfs_efs filesystem options */
#define	EFS_RECOVER_FLAG_STR	"EFS_RECOVER_FLAG"
#define	EFS_ALIGN_FLAG_STR	"EFS_ALIGN_FLAG"
#define	EFS_NO_INODES_STR	"EFS_NO_INODES"
#define	EFS_NO_BLOCKS_STR	"EFS_NO_BLOCKS"
#define	EFS_SECTORS_TRACK_STR	"EFS_SECTORS_TRACK"
#define	EFS_CYL_GRP_SIZE_STR	"EFS_CYL_GRP_SIZE"
#define	EFS_CGALIGN_STR		"EFS_CGALIGN"
#define	EFS_IALIGN_STR		"EFS_IALIGN"
#define	EFS_PROTO_STR		"EFS_PROTO"

/* mkfs_xfs filesystem options */
#define	XFS_BLK_SIZE_LOG_STR	"XFS_BLK_SIZE_LOG"
#define	XFS_BLK_SIZE_STR	"XFS_BLK_SIZE"
#define	XFS_DATA_AGCOUNT_STR	"XFS_DATA_AGCOUNT"
#define	XFS_DATA_FILE_STR	"XFS_DATA_FILE"
#define	XFS_DATA_FILE_TYPE_STR	"XFS_DATA_FILE_TYPE"
#define	XFS_DATA_SIZE_STR	"XFS_DATA_SIZE"
#define	XFS_INODE_LOG_STR	"XFS_INODE_LOG"
#define	XFS_INODE_PERBLK_STR	"XFS_INODE_PERBLK"
#define	XFS_INODE_SIZE_STR	"XFS_INODE_SIZE"
#define	XFS_LOG_INTERNAL_STR	"XFS_LOG_INTERNAL"
#define	XFS_LOG_SIZE_STR	"XFS_LOG_SIZE"
#define	XFS_PROTO_FILE_STR	"XFS_PROTO_FILE"
#define	XFS_RT_EXTSIZE_STR	"XFS_RT_EXTSIZE"
#define	XFS_RT_SIZE_STR		"XFS_RT_SIZE"
#define	XFS_XLV_DEVICE_STR	"XFS_XLV_DEVICE_NAME"

static char* xfs_fs_types[] = { "efs", "xfs", "nfs", "proc", 
				"fd", "cdfs", "iso9660", "dos", 
				"hfs", "swap", "cachefs", "ignore", 
				NULL };

static struct fs_option xfs_export_opts_tab[] = {
	XFS_EXPOPTS_RW_STR,	"rw=%s",	NULL,		NULL,	NULL,
	XFS_EXPOPTS_ROOT_STR,	"root=%s",	NULL,		NULL,	NULL,
	XFS_EXPOPTS_RO_STR,	"ro",		"ro",		NULL,	NULL,
	XFS_EXPOPTS_ANON_STR,	"anon=%s",	"anon=nobody",NULL,	NULL,
	XFS_EXPOPTS_ACCESS_STR,	"access=%s",	NULL,		NULL,	NULL,
	XFS_EXPOPTS_NOHIDE_STR,	"nohide",	"nohide",	NULL,	NULL,
	XFS_EXPOPTS_WSYNC_STR,	"wsync",	"wsync",	NULL,	NULL
};

#define	XFS_EXPORT_OPTS_TAB_LEN	(sizeof(xfs_export_opts_tab) / sizeof(xfs_export_opts_tab[0]))

static struct fs_option generic_fs_options[] = {
	XFS_FS_OPTS_RO_STR,	"ro",		"ro",		"rw",	NULL,
	XFS_FS_OPTS_NOAUTO_STR,	"noauto",	"noauto",	NULL,	NULL,
	XFS_FS_OPTS_GRPID_STR,	"grpid",	"grpid",	NULL,	NULL,
	XFS_FS_OPTS_NOSUID_STR,	"nosuid",	"nosuid",	NULL,	NULL,
	XFS_FS_OPTS_NODEV_STR,	"nodev",	"nodev",	NULL,	NULL,
	XFS_FS_OPTS_RAW_STR,	"raw=%s",	NULL,		NULL,	NULL
};
	
#define	GENERIC_FS_OPTS_TAB_LEN	(sizeof(generic_fs_options) / sizeof(generic_fs_options[0]))

static struct fs_option efs_fs_options[] = {
	XFS_FS_OPTS_FSCK_STR,	"fsck",		"fsck",		"nofsck", NULL,
	XFS_FS_OPTS_QUOTA_STR,	"quota",	"quota",	"noquota",NULL,
	XFS_FS_OPTS_LBSIZE_STR,	"lbsize=%s",	NULL,		NULL,	"32768"
};

#define	EFS_FS_OPTS_TAB_LEN	(sizeof(efs_fs_options) / sizeof(efs_fs_options[0]))

static struct fs_option xfs_fs_options[] = {
	XFS_FS_OPTS_DMI_STR,	"dmi",		"dmi",		NULL,	NULL
};

#define	XFS_FS_OPTS_TAB_LEN	(sizeof(xfs_fs_options) / sizeof(xfs_fs_options[0]))

static struct fs_option nfs_fs_options[] = {
	XFS_FS_OPTS_BG_STR,	"bg",		"bg",		"fg",	"fg",
	XFS_FS_OPTS_RETRY_STR,	"retry=%s",	NULL,		NULL,  "10000",
	XFS_FS_OPTS_RSIZE_STR,	"rsize=%s",	NULL,		NULL,	"8192",
	XFS_FS_OPTS_WSIZE_STR,	"wsize=%s",	NULL,		NULL,	"8192",
	XFS_FS_OPTS_TIMEO_STR,	"timeo=%s",	NULL,		NULL,	"11",
	XFS_FS_OPTS_RETRANS_STR,"retrans=%s",	NULL,		NULL,	"5",
	XFS_FS_OPTS_PORT_STR,	"port=%s",	NULL,		NULL,	"2049",
	XFS_FS_OPTS_HARD_STR,	"hard",		"hard",		"soft",	"hard",
	XFS_FS_OPTS_INTR_STR,	"intr",		"intr",		NULL,	NULL,	
	XFS_FS_OPTS_ACREGMIN_STR,"acregmin=%s",	NULL,		NULL,	"3",
	XFS_FS_OPTS_ACREGMAX_STR,"acregmax=%s",	NULL,		NULL,	"60",
	XFS_FS_OPTS_ACDIRMIN_STR,"acdirmin=%s",	NULL,		NULL,	"30",
	XFS_FS_OPTS_ACDIRMAX_STR,"acdirmax=%s",	NULL,		NULL,	"60",
	XFS_FS_OPTS_ACTIMEO_STR,"actimeo=%s",	NULL,		NULL,	NULL,
	XFS_FS_OPTS_NOAC_STR,	"noac",		"noac",		NULL,	NULL,
	XFS_FS_OPTS_PRIV_STR,	"private",	"private",	NULL,	NULL,
	XFS_FS_OPTS_SYMTTL_STR,	"symttl=%s",	NULL,		NULL,	"3600"
};

#define	NFS_FS_OPTS_TAB_LEN  (sizeof(nfs_fs_options)/sizeof(nfs_fs_options[0]))

static struct fs_option iso9660_fs_options[] = {
	XFS_FS_OPTS_SETX_STR,	"setx",		NULL,		NULL,	NULL,
	XFS_FS_OPTS_NTRANS_STR,	"notranslate",	"notranslate",NULL,	NULL,
	XFS_FS_OPTS_CACHE_STR,	"cache=%s",	NULL,		NULL,	"128",
	XFS_FS_OPTS_NOEXT_STR,	"noext",	"noext",	NULL,	NULL,
	XFS_FS_OPTS_SUSP_STR,	"susp",		"susp",		"nosusp","susp",
	XFS_FS_OPTS_RRIP_STR,	"rrip",		"rrip",		"norrip","rrip",
	XFS_FS_OPTS_MMCNV_STR,	"nmconv=%s",	NULL,		NULL,	NULL
};

#define	ISO9660_FS_OPTS_TAB_LEN	(sizeof(iso9660_fs_options) / sizeof(iso9660_fs_options[0]))

static struct fs_option swap_fs_options[] = {
	XFS_FS_OPTS_PRI_STR,	"pri=%s",	NULL,		NULL,	NULL,
	XFS_FS_OPTS_SWPLO_STR,	"swplo=%s",	NULL,		NULL,	"0",
	XFS_FS_OPTS_LEN_STR,	"length=%s",	NULL,		NULL,	NULL,
	XFS_FS_OPTS_MAXL_STR,	"maxlength=%s",	NULL,		NULL,	NULL,
	XFS_FS_OPTS_VLEN_STR,	"vlength=%s",	NULL,		NULL,	NULL,
};

#define	SWAP_FS_OPTS_TAB_LEN	(sizeof(swap_fs_options) / sizeof(swap_fs_options[0]))

static struct fs_option cache_fs_options[] = {
	XFS_FS_OPTS_BACKFS_STR,		"backfstype=%s",	NULL, NULL, NULL,
	XFS_FS_OPTS_BACKP_STR,		"backpath=%s",	NULL, NULL, NULL,
	XFS_FS_OPTS_CACHEDIR_STR,	"cachedir=%s",	NULL, NULL, NULL,
	XFS_FS_OPTS_CACHEID_STR,	"cacheid=%s",	NULL, NULL, NULL,
	XFS_FS_OPTS_WR_AD_STR,		"write_around",	"write_around","non_shared", "write_around",
	XFS_FS_OPTS_NCONST_STR,		"noconst",	"noconst",NULL,NULL,	
	XFS_FS_OPTS_LACCESS_STR,	"local-access",	NULL, NULL, NULL,
	XFS_FS_OPTS_PURGE_STR,		"purge",	NULL, NULL, NULL,
	XFS_FS_OPTS_SUID_STR,		"suid",		"suid","nosuid", "suid",
	XFS_FS_OPTS_ACREGMIN_STR,	"acregmin=%s",	NULL,NULL,"30",
	XFS_FS_OPTS_ACREGMAX_STR,	"acregmax=%s",	NULL,NULL,"30",
	XFS_FS_OPTS_ACDIRMIN_STR,	"acdirmin=%s",	NULL,NULL,"30",
	XFS_FS_OPTS_ACDIRMAX_STR,	"acdirmax=%s",	NULL,NULL,"30",
	XFS_FS_OPTS_ACTIMEO_STR,	"actimeo=%s",	NULL,NULL, NULL
};
	
#define	CACHE_FS_OPTS_TAB_LEN	(sizeof(cache_fs_options) / sizeof(cache_fs_options[0]))

/* Keyword for unmounting a filesystem forcefully */
#define	XFS_FORCE_UMOUNT_STR	"XFS_FORCE_UMOUNT_FLAG"

typedef struct fs_info_entry_s {
	int	info;
	struct mntent *fs_entry;
	struct mntent *mount_entry;
	struct exportent *export_entry;
	struct fs_info_entry_s *next;
} fs_info_entry_t;

#define gettxt(str_line_num,str_default)        str_default

/* Function definitions */

/*
 *	xfsutils.c
 */
extern int isvalidmntpt(char *);
extern int remmntent(FILE *, char *, char *);
extern int xfs_getmntent(char *, char *, struct mntent *);
extern int xfs_replace_mntent(char *, struct mntent *, struct mntent *);
extern int xfs_get_exportent(char *, char *, struct exportent *);
extern int xfs_replace_exportent(struct exportent *, struct exportent *);


/*
 *	xfsfuncs.c
 */
extern int create_mnt_dir(const char* username, const char* groupname, const char* mode_str, const char* dirname,char** msg);
extern int is_valid_fs_type(const char* fs_type);
extern char* to_lower_fs_type_str(const char* fs_type_str);
extern char* get_fs_name(char* tabfile,char* mnt_dir);
extern char* get_mnt_dir(char* tabfile,char* filename);
extern char* get_export_opts(char* fs_mnt_dir);
extern char* get_mnt_opts(char* fs_name);
extern int is_fstab_entry(char* fs_name);
extern int is_mounted(char* fs_name);
extern int is_exported(char* fs_name, char *mntdir);
extern int is_fs_type(char* fs_name,char* fs_type);
extern int add_fstab_entry(struct mntent *new_fs_entry,char** msg);
extern int delete_fstab_entry(char* fsname,char** msg);
extern int mount_fs(char* fsname,char* mnt_dir,char* mnt_options,char** msg);
extern int unmount_fs(char* fsname,int force_flag,char** msg);

/*
 *	Export/Unexport
 */
extern int xfs_export(char *,char *,char *, boolean_t, char *, char **);
extern int export_fs(char *, boolean_t, char *, char **);

extern int xfs_unexport(char* host,char* fsname,int delete_flag,char** msg);
extern int unexport_fs(char* fsname,char** msg);

extern int add_export_entry(struct exportent *new_export_entry, char** msg);
extern int delete_export_entry(char* fsname,char** msg);

/*
 *	Mount/Unmount
 */
extern int xfs_mount(char *, char *,char *, char *, char *, int, char **);
extern int xfs_umount(char *, char *, int, int, char **);

/*
 *	Create/Delete
 */
extern int xfs_fs_create(char *, struct mntent *, char *, char *,
				char *, int, char **);
extern int xfs_fs_delete(char *, char *, int, int, char **);


extern fs_info_entry_t* xfs_info(char* host,char** msg);
extern void add_mount_info_to_fs_list(fs_info_entry_t **list,struct mntent *mount_entry,int type);
extern void add_export_info_to_fs_list(fs_info_entry_t **list,struct exportent *export_entry,int type);
extern void add_to_fs_info_list(fs_info_entry_t **list,struct mntent *entry,int type);
extern fs_info_entry_t* xfs_info(char* host,char** msg);
extern int xfs_fs_edit(char* host,struct mntent *new_fstab_entry,char* export_opts, char** msg);
extern void delete_xfs_info_entry(fs_info_entry_t* fs_info_entry);
extern void create_default_values_str(struct fs_option options_tab[],int options_tab_len,char** default_values_str);
extern char* get_default_values(char* fs_type,char** default_values_str);

#endif	/* _xfs_fs_defs_h_ */
