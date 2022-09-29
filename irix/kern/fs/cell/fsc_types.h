/**************************************************************************
 *									  *
 *		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_FS_CELL_FSC_TYPES_H_
#define	_FS_CELL_FSC_TYPES_H_

#ident	"$Id: fsc_types.h,v 1.3 1997/08/26 17:58:20 dnoveck Exp $"

/*
 * fs/cell/fsc_types.h -- Basic type definitions for cellular fs's
 *
 * This header file includes basic type definitions to be used by all of
 * the file systems that implement cellular distribtion (i.e cfs and cxfs).
 * It should not be included:
 *
 *      In files compiled without CELL defined
 *      In files outside the fs directory
 *
 * These definitions allow cfs and cxfs to refer to one another's structures
 * in an opaque way (i.e without any knowledge of their contents). 
 */
 
#ifndef CELL
#error included by non-CELL configuration
#endif

struct dcvfs;
struct dsvfs;
struct dcvn;
struct dsvn;
struct dcxvn;
struct dsxvn;
struct dcxvfs;
struct cxfs_sinfo;
struct mount_import;

typedef struct dcvfs dcvfs_t;
typedef struct dsvfs dsvfs_t;
typedef struct dcvn dcvn_t;
typedef struct dsvn dsvn_t;
typedef struct dcxvn dcxvn_t;
typedef struct dsxvn dsxvn_t;
typedef struct dcxvfs dcxvfs_t;
typedef struct cxfs_sinfo cxfs_sinfo_t;
typedef struct mount_import mount_import_t;

#endif /* _FS_CELL_FSC_TYPES_H_ */
