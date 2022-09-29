/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: dacl_debug.h,v $
 * Revision 65.2  1997/12/16 17:05:41  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:20:15  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.337.1  1996/10/02  18:15:18  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:29  damon]
 *
 * $EndLog$
 */
/*
*/
/*
 *	dacl_debug.h -- the debugging constants for the dacl package
 *
 * Copyright (C) 1994, 1991 Transarc Corporation
 * All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_debug.h,v 65.2 1997/12/16 17:05:41 lmc Exp $ */

#ifndef TRANSARC_DACL_DEBUG_H
#define TRANSARC_DACL_DEBUG_H 1

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/debug.h>

#ifdef AFS_IRIX_ENV
IMPORT int dacl_debug_flag;
#else
IMPORT long int dacl_debug_flag;
#endif /* AFS_IRIX_ENV */

#define DACL_DEBUG_MASK(bitnum)		((1L) << (bitnum - 1))

#define DACL_DEBUG_BIT_WARNINGS		1
#define DACL_DEBUG_BIT_FLATTEN		2
#define DACL_DEBUG_BIT_PARSE		3
#define DACL_DEBUG_BIT_PERMSET		4
#define DACL_DEBUG_BIT_STRINGS		5
#define DACL_DEBUG_BIT_TESTENTRY	6
#define DACL_DEBUG_BIT_ACCESS		7
/* the following debug flag is for uuid translation */
#define DACL_DEBUG_BIT_TRANSLATE	8
/* the following debug flag is for sec_acl translation */
#define DACL_DEBUG_BIT_SECACL_TRANSLATE	9

#define DACL_DEBUG_BIT_RGY_TRACE	10

#define DACL_DEBUG_BIT_VALIDATE		11

#define DACL_DEBUG_BIT_FS		31
/*
 * DACL_DEBUG_BIT_WARNINGS		=> 0x01
 * DACL_DEBUG_BIT_FLATTEN		=> 0x02
 * DACL_DEBUG_BIT_PARSE			=> 0x04
 * DACL_DEBUG_BIT_PERMSET		=> 0x08
 * DACL_DEBUG_BIT_STRINGS		=> 0x10
 * DACL_DEBUG_BIT_TESTENTRY		=> 0x20
 * DACL_DEBUG_BIT_ACCESS		=> 0x40
 * DACL_DEBUG_BIT_TRANSLATE		=> 0x80
 * DACL_DEBUG_BIT_SECACL_TRANSLATE	=> 0x100
 * DACL_DEBUG_BIT_RGY_TRACE		=> 0x200
 * DACL_DEBUG_BIT_VALIDATE		=> 0x400
 * DACL_DEBUG_BIT_FS			=> 0x80000000
 */

IMPORT void dacl_LogParamError _TAKES((
				       char *	routineNameP,
				       char *	paramNameP,
				       int	logCondition,
				       char *	filenameP,
				       int	lineP
				     ));
  
#endif /* TRANSARC_DACL_DEBUG_H */
