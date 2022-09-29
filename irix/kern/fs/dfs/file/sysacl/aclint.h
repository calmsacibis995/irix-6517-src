/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: aclint.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1997/12/16 17:05:42  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:20:41  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.1  1996/10/02  21:01:01  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:47:57  damon]
 *
 * Revision 1.1.5.1  1994/06/09  14:20:18  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:31:51  annie]
 * 
 * Revision 1.1.3.2  1993/01/21  15:20:44  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  15:59:49  cjd]
 * 
 * Revision 1.1  1992/01/19  02:55:11  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
 * (C) Copyright 1994, 1990 Transarc Corporation
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/sysacl/RCS/aclint.h,v 65.3 1999/07/21 19:01:14 gwehrman Exp $
 *
 * aclint.h
 *	Definitions used in passing ACL's across various interfaces.
 */

#ifndef _TRANSARC_ACLINT_H_
#define _TRANSARC_ACLINT_H_ 1
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

/* values for which-acl */

#define VNX_ACL_REGULAR_ACL	0	/* regular */
#define VNX_ACL_DEFAULT_ACL	1	/* default for (sub)directories */
#define VNX_ACL_INITIAL_ACL	2	/* default for (sub)files */

#define VNX_ACL_DELEGATE_ENTRY_OK 4  

#define VNX_ACL_TYPE_MASK  (VNX_ACL_REGULAR_ACL | VNX_ACL_DEFAULT_ACL | \
			    VNX_ACL_INITIAL_ACL)

/* structure identical to afsACL */

/* define this as (8k - sizeof(long)) so that a ``struct dfs_acl'' fits in an 8k free-buffer. */
#define MAXDFSACL	8188
#ifdef SGIMIPS64
struct dfs_acl {			/* identical to afsACL in afs4int.h */
    int dfs_acl_len;			/* length */
    char dfs_acl_val[MAXDFSACL];	/* data */
};
#else
struct dfs_acl {			/* identical to afsACL in afs4int.h */
    long dfs_acl_len;			/* length */
    char dfs_acl_val[MAXDFSACL];	/* data */
};
#endif /* SGIMIPS64 */

/* Opcodes for syscall */

#define VNX_OPCODE_GETACL	0
#define VNX_OPCODE_SETACL	1
#define VNX_OPCODE_COPYACL	2
#define VNX_OPCODE_QUOTA	3

#endif /* _TRANSARC_ACLINT_H_ */
