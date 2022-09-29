#ifndef __DISK_QUERY_DEFS_H
#define __DISK_QUERY_DEFS_H

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

#define	DISK_QUERY_ATTR_RANGE_BEGIN 	1000

#define	DISK_NO_CYLINDERS		(DISK_QUERY_ATTR_RANGE_BEGIN+1)
#define	DISK_NO_TRACKS_PER_CYLINDER	(DISK_QUERY_ATTR_RANGE_BEGIN+2)
#define	DISK_NO_SECTORS_PER_TRACK	(DISK_QUERY_ATTR_RANGE_BEGIN+3)
#define	DISK_SECTOR_LENGTH		(DISK_QUERY_ATTR_RANGE_BEGIN+4)
#define	DISK_SECTOR_INTERLEAVE		(DISK_QUERY_ATTR_RANGE_BEGIN+5)
#define	DISK_CTLR_FLAG			(DISK_QUERY_ATTR_RANGE_BEGIN+6)
#define	DISK_PARTITION_TYPE		(DISK_QUERY_ATTR_RANGE_BEGIN+7)
#define	DISK_PARTITION_SIZE		(DISK_QUERY_ATTR_RANGE_BEGIN+8)
#define	DISK_PARTITION_GAP		(DISK_QUERY_ATTR_RANGE_BEGIN+9)
#define	DISK_PARTITION_OVERLAP		(DISK_QUERY_ATTR_RANGE_BEGIN+10)
#define	DISK_NAME			(DISK_QUERY_ATTR_RANGE_BEGIN+11)
#define	DISK_MBSIZE			(DISK_QUERY_ATTR_RANGE_BEGIN+12)
#define	DISK_DRIVECAP			(DISK_QUERY_ATTR_RANGE_BEGIN+13)

#define	DISK_QUERY_ATTR_RANGE_END	(DISK_DRIVECAP)

/* Function Prototypes */
extern void add_disk_to_set(query_set_t *element, query_set_t **set);

extern int xfs_disk_query_eval(xlv_vh_entry_t* disk_info,short attr_id, char* value, short oper);

extern int xfs_disk_query(query_clause_t* query_clause, query_set_t* in_set, query_set_t** out_set);

#endif
