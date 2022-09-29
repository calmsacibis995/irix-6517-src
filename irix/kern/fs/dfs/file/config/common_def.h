/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: common_def.h,v $
 * Revision 65.1  1997/10/20 19:21:48  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.12.1  1996/10/02  17:14:38  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:06:33  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/config/RCS/common_def.h,v 65.1 1997/10/20 19:21:48 jdoak Exp $ */

/*
 *
 *     The common definitions used with common_data.idl types.  This file is
 *     included by the IDL-generated common_data.h file and does not have to be
 *     included explicitly.
 *
 */
#ifndef _TRANSARC_COMMON_DEF_
#define _TRANSARC_COMMON_DEF_

/*
 * ROOT Fid special vnode and unique.
 */
#define AFS_ROOTVNODE  (0xffffffffL)
#define AFS_ROOTUNIQUE (0xffffffffL)

/*
 * Generic operations on afsFids
 */
#define FidCmp(a,b) ((a)->Unique != (b)->Unique \
    || (a)->Vnode != (b)->Vnode \
    || !AFS_hsame((a)->Volume, (b)->Volume) \
    || !AFS_hsame((a)->Cell, (b)->Cell))

#define FidCopy(a, b) \
    ((a)->Cell = (b)->Cell, \
     (a)->Volume = (b)->Volume, \
     (a)->Vnode = (b)->Vnode, \
     (a)->Unique = (b)->Unique)


/*
 * Supported token types
 * All values are powers of 2 so that we can test them using bit masks.
 */

#define        TKN_VERSION             1
#define        TKN_MASK                0x3f /* mask token ID to base ID */

/*
 * First the types for which the byte range is significant in determination
 * of token conflicts:
 */
#define        TKN_LOCK_READ           0x001
#define        TKN_LOCK_WRITE          0x002

#define        TKN_DATA_READ           0x004
#define        TKN_DATA_WRITE          0x008

/*
 * Now, the types for which the byte range is not significant in determination
 * of token conflicts:
 */
#define        TKN_OPEN_READ           0x010
#define        TKN_OPEN_WRITE          0x020
#define        TKN_OPEN_SHARED         0x040
#define        TKN_OPEN_EXCLUSIVE      0x080
#define        TKN_OPEN_DELETE         0x100
#define        TKN_OPEN_PRESERVE       0x200

#define        TKN_STATUS_READ         0x400
#define        TKN_STATUS_WRITE        0x800

/* (interpolated, unused TKN_NUKE here) */

#define	TKN_SPOT_HERE		0x2000
#define	TKN_SPOT_THERE		0x4000

/* some common combinations */
#define        TKN_UPDATE              (TKN_DATA_READ | TKN_DATA_WRITE |\
					TKN_STATUS_READ | TKN_STATUS_WRITE)

#define        TKN_READ                (TKN_DATA_READ | TKN_STATUS_READ)
#define        TKN_UPDATESTAT          (TKN_STATUS_READ | TKN_STATUS_WRITE)

/* some macros to set/get/test values in afsConnParams structure */
#define AFS_CONN_PARAM_SET(paramp, field, value)	\
    do {						\
    	(paramp)->Values[(field)] = (value);		\
    	(paramp)->Mask |= (1 << (field));		\
    } while(0)

#define AFS_CONN_PARAM_GET(paramp, field)		\
    ((paramp)->Values[(field)])

#define AFS_CONN_PARAM_ISVALID(paramp, field)		\
    ((paramp)->Mask & (1 << (field)))

#endif /* _TRANSARC_COMMON_DEF_ */  /* end of common_def.h */
