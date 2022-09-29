/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __HFS_H__
#define __HFS_H__

#ident "$Revision: 1.5 $"

/*
 * Declarations for mount_hfs, a user-level NFS server that implements an HFS
 * filesystem.
 */


/*
 * NOTE:  Mount_hfs relies on a collection of major structures and routines
 *        related to their manipulation:
 *        
 *      Prefix  Structure	Description
 * 	===================================================================
 *	ar_	ar_t		Ancillary records - used to emulate XINET
 *				ancillary files.
 *	bit_	 --		Bit operations.
 *	bt_	bt_t		Btree - contains hfs directory records and
 *				extent overflows.
 *	er_	erec_t		Extent records.
 *	fr_	frec_t		File record - directory records.
 *	fs_	hfs_t		File system - one per each mounted file
 *				system.
 *	hfs_	 --		HFS specific file operations.
 *	hl_	hlnode_t	Hash list nodes - used for organizing hash
 *				lists of structures.
 *	hn_	hnode_t		HFS "inode"
 *	io_	 --		Low level I/O operations.
 *	nfs_	 --		NFS server operations.
 *      ====================================================================
 */


/*
 * Duh:  There's a stupidity about the way TRACE is handled that is too
 *       embarrassing to discuss here.
 */

#ifndef NOTRACE
#define TRACE(m) trace m
#define TRACEONCE(m) {static int o; if (o++ ==0) TRACE(("trace-once:  " m)); }
#else
#define TRACE(m) ((void)0)
#define TRACEONCE(m) ((void)0)
#endif

typedef struct hnode hnode_t;
typedef struct frec frec_t;
typedef struct hfs hfs_t;

#include <sys/types.h>
#include <assert.h>
#include "hfs_types.h"

/*
 * Error check macros.
 */
#define __chk(h,exp,s) hfs_chk(h,exp,#s)
#define __chkr(h,exp,s) {int __e; if (__e=__chk(h,exp,s)) return __e; }

#include "hfs_hl.h"

#include "hfs_er.h"
#include "hfs_fr.h"
#include "hfs_bt.h"
#include "hfs_hn.h"
#include "hfs_bit.h"
#include "hfs_ar.h"

#include "hfs_fs.h"
#include "hfs_io.h"
#include "hfs_nfs.h"
#include "hfs_dir.h"
#include "hfs_hfs.h"


#endif






