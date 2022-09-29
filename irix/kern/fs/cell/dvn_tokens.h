/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_FS_CELL_DVN_TOKENS_H_
#define	_FS_CELL_DVN_TOKENS_H_

#ident	"$Id: dvn_tokens.h,v 1.5 1997/08/27 18:47:09 mostek Exp $"

/*
 * fs/cell/dvn_tokens.h -- Vnode tokens for cfs and cxfs
 *
 * This header file defines the tokens used by both cfs and cxfs-v1.  Since
 * these components share tkc and tks structures, they need to agree on 
 * common definitions for the token classes used.
 *
 * This header should only be included by cfs and cxfs-v1 code.
 */
 
#ifndef CELL
#error included by non-CELL configuration
#endif

/*
 * Tokens.
 */
#define	DVN_EXIST_NUM		0
#define	DVN_PCACHE_NUM		1
#define	DVN_PSEARCH_NUM		2
#define	DVN_NAME_NUM		3
#define	DVN_ATTR_NUM		4
#define	DVN_TIMES_NUM		5
#define	DVN_EXTENT_NUM		6
#define	DVN_BIT_NUM		7
#define	DVN_DIT_NUM		8
#define	DVN_NTOKENS		9

#define	DVN_EXIST_READ		TK_MAKE(DVN_EXIST_NUM, TK_READ)
#define	DVN_EXIST_READ_1	TK_MAKE_SINGLETON(DVN_EXIST_NUM, TK_READ)
#define	DVN_EXIST_WRITE		TK_MAKE(DVN_EXIST_NUM, TK_WRITE)
#define	DVN_EXIST_WRITE_1	TK_MAKE_SINGLETON(DVN_EXIST_NUM, TK_WRITE)

#define	DVN_PCACHE_READ		TK_MAKE(DVN_PCACHE_NUM, TK_READ)
#define	DVN_PCACHE_READ_1	TK_MAKE_SINGLETON(DVN_PCACHE_NUM, TK_READ)
#define	DVN_PCACHE_WRITE	TK_MAKE(DVN_PCACHE_NUM, TK_WRITE)
#define	DVN_PCACHE_WRITE_1	TK_MAKE_SINGLETON(DVN_PCACHE_NUM, TK_WRITE)

#define	DVN_PSEARCH_READ	TK_MAKE(DVN_PSEARCH_NUM, TK_READ)
#define	DVN_PSEARCH_READ_1	TK_MAKE_SINGLETON(DVN_PSEARCH_NUM, TK_READ)
#define	DVN_PSEARCH_WRITE	TK_MAKE(DVN_PSEARCH_NUM, TK_WRITE)
#define	DVN_PSEARCH_WRITE_1	TK_MAKE_SINGLETON(DVN_PSEARCH_NUM, TK_WRITE)

#define DVN_PCACHE_PSEARCH_READ TK_ADD_SET(DVN_PCACHE_READ, DVN_PSEARCH_READ)

#define	DVN_NAME_READ		TK_MAKE(DVN_NAME_NUM, TK_READ)
#define	DVN_NAME_READ_1		TK_MAKE_SINGLETON(DVN_NAME_NUM, TK_READ)
#define	DVN_NAME_WRITE		TK_MAKE(DVN_NAME_NUM, TK_WRITE)
#define	DVN_NAME_WRITE_1	TK_MAKE_SINGLETON(DVN_NAME_NUM, TK_WRITE)

#define	DVN_ATTR_READ		TK_MAKE(DVN_ATTR_NUM, TK_READ)
#define	DVN_ATTR_READ_1		TK_MAKE_SINGLETON(DVN_ATTR_NUM, TK_READ)
#define	DVN_ATTR_WRITE		TK_MAKE(DVN_ATTR_NUM, TK_WRITE)
#define	DVN_ATTR_WRITE_1	TK_MAKE_SINGLETON(DVN_ATTR_NUM, TK_WRITE)

#define	DVN_TIMES_READ		TK_MAKE(DVN_TIMES_NUM, TK_READ)
#define	DVN_TIMES_READ_1	TK_MAKE_SINGLETON(DVN_TIMES_NUM, TK_READ)
#define	DVN_TIMES_WRITE		TK_MAKE(DVN_TIMES_NUM, TK_WRITE)
#define	DVN_TIMES_WRITE_1	TK_MAKE_SINGLETON(DVN_TIMES_NUM, TK_WRITE)
#define	DVN_TIMES_SHARED_WRITE	TK_MAKE(DVN_TIMES_NUM, TK_SWRITE)
#define	DVN_TIMES_SHARED_WRITE_1	TK_MAKE_SINGLETON(DVN_TIMES_NUM, TK_SWRITE)

#define	DVN_EXTENT_READ		TK_MAKE(DVN_EXTENT_NUM, TK_READ)
#define	DVN_EXTENT_READ_1	TK_MAKE_SINGLETON(DVN_EXTENT_NUM, TK_READ)
#define	DVN_EXTENT_WRITE	TK_MAKE(DVN_EXTENT_NUM, TK_WRITE)
#define	DVN_EXTENT_WRITE_1	TK_MAKE_SINGLETON(DVN_EXTENT_NUM, TK_WRITE)

#define	DVN_BIT_READ		TK_MAKE(DVN_BIT_NUM, TK_READ)
#define	DVN_BIT_READ_1		TK_MAKE_SINGLETON(DVN_BIT_NUM, TK_READ)
#define	DVN_BIT_WRITE		TK_MAKE(DVN_BIT_NUM, TK_WRITE)
#define	DVN_BIT_WRITE_1		TK_MAKE_SINGLETON(DVN_BIT_NUM, TK_WRITE)
#define	DVN_BIT_SHARED_WRITE	TK_MAKE(DVN_BIT_NUM, TK_SWRITE)
#define	DVN_BIT_SHARED_WRITE_1	TK_MAKE_SINGLETON(DVN_BIT_NUM, TK_SWRITE)

#define	DVN_DIT_READ		TK_MAKE(DVN_DIT_NUM, TK_READ)
#define	DVN_DIT_READ_1		TK_MAKE_SINGLETON(DVN_DIT_NUM, TK_READ)
#define	DVN_DIT_WRITE		TK_MAKE(DVN_DIT_NUM, TK_WRITE)
#define	DVN_DIT_WRITE_1		TK_MAKE_SINGLETON(DVN_DIT_NUM, TK_WRITE)

/*
 * The following define specifies which tokens lookup and other
 * calls try to get.
 *
 * Any obtained tokens will result in the associated object being returned.
 */

/* WHEN ALL IS IN PLACE 
#define DVN_OPTIMISTIC_TOKENS (DVN_NAME_READ | \
	DVN_ATTR_READ | DVN_EXTENT_READ | DVN_TIMES_SHARED_WRITE)
 */
#define DVN_OPTIMISTIC_TOKENS (DVN_ATTR_READ | DVN_EXTENT_READ \
			| DVN_TIMES_SHARED_WRITE)
			
#define DVN_ALL_CLIENT_TOKENS (DVN_EXIST_READ | DVN_PCACHE_READ | DVN_PSEARCH_READ \
		| DVN_ATTR_READ | DVN_EXTENT_READ | DVN_TIMES_SHARED_WRITE)


/*
 * The WRITE_TOKENS define is used extensively whenever we are changing
 * a directory, writing a file, ...
 */

#define DVN_WRITE_TOKENS (DVN_ATTR_WRITE|DVN_EXTENT_WRITE|DVN_TIMES_SHARED_WRITE)


#define DVN_TRUNCATE_TOKENS (DVN_EXTENT_WRITE|DVN_TIMES_SHARED_WRITE)

/*
 * When modifying attributes, the time is usually changed too.
 */

#define DVN_ATTR_MOD_TOKENS (DVN_ATTR_WRITE|DVN_TIMES_SHARED_WRITE)

/*
 * The following are the AT_* flags that are associated with
 * the appropriate tokens.
 */

#define AT_EXTENT_PROTECTED (AT_SIZE|AT_EXTSIZE|AT_NEXTENTS|AT_NBLOCKS)
#define AT_TIMES_PROTECTED  (AT_TIMES)

/*
 * ATTR tokens protect everything except those protected by the EXTENT
 * and TIMES tokens.
 */

#define AT_ATTR_PROTECTED   (AT_ALL & ~(AT_EXTENT_PROTECTED|AT_TIMES_PROTECTED))
#endif /* _FS_CELL_DVN_TOKENS_H_ */
