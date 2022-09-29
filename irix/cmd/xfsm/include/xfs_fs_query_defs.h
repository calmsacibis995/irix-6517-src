#ifndef __FS_QUERY_DEFS_H__
#define	__FS_QUERY_DEFS_H__

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

#define	FS_QUERY_ATTR_RANGE_BEGIN	2000

#define	FS_MOUNTED_STATE		(FS_QUERY_ATTR_RANGE_BEGIN+1)
#define	FS_EXPORTED_STATE		(FS_QUERY_ATTR_RANGE_BEGIN+2)
#define	FS_TYPE_INFO			(FS_QUERY_ATTR_RANGE_BEGIN+3)
#define	FS_MOUNT_OPTIONS		(FS_QUERY_ATTR_RANGE_BEGIN+4)
#define	FS_EXPORT_OPTIONS		(FS_QUERY_ATTR_RANGE_BEGIN+5)
#define	FS_MOUNT_FREQ			(FS_QUERY_ATTR_RANGE_BEGIN+6)
#define	FS_NAME				(FS_QUERY_ATTR_RANGE_BEGIN+7)
#define	FS_MNT_DIR_NAME			(FS_QUERY_ATTR_RANGE_BEGIN+8)

#define	FS_QUERY_ATTR_RANGE_END		(FS_MNT_DIR_NAME)

/* Function Prototypes */
extern void add_fs_to_set(query_set_t *element, query_set_t **set);

extern int xfs_fs_query_eval(fs_info_entry_t* fs_info,short attr_id,char* value, short oper);

extern int xfs_fs_query(query_clause_t* query_clause,query_set_t* in_set,query_set_t** out_set);

#endif
