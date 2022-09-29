/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: md5_krpc.h,v $
 * Revision 65.1  1997/10/20 19:16:27  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.41.2  1996/02/18  23:46:44  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:23  marty]
 *
 * Revision 1.1.41.1  1995/12/08  00:15:18  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:25  root]
 * 
 * Revision 1.1.39.1  1994/01/21  22:32:15  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:31  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  22:36:38  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:04  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:39:42  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:48  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:16:16  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**
**  NAME
**
**      md5_krpc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definitions of types/constants internal to RPC facility and common
**  to all RPC components.
**
**
*/

/*
 * $Source: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/rpc/kruntime/RCS/md5_krpc.h,v $
 * $Author: jdoak $
 * $Id: md5_krpc.h,v 65.1 1997/10/20 19:16:27 jdoak Exp $
 *
 * Copyright 1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America is assumed
 *   to require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * RSA MD5 header file, with STDC additions.
 */

#ifndef _MD5_KRPC_H
#define _MD5_KRPC_H 1

#include <dce/dce.h>

/* 16 u_char's in the digest */
#define RSA_MD5_CKSUM_LENGTH    16
/* des blocksize is 8, so this works nicely... */
#define RSA_MD5_DES_CKSUM_LENGTH        16

/*
 ***********************************************************************
 ** md5.h -- header file for implementation of MD5                    **
 ** RSA Data Security, Inc. MD5 Message-Digest Algorithm              **
 ** Created: 2/17/90 RLR                                              **
 ** Revised: 12/27/90 SRD,AJ,BSK,JT Reference C version               **
 ** Revised (for MD5): RLR 4/27/91                                    **
 **   -- G modified to have y&~z instead of y&z                       **
 **   -- FF, GG, HH modified to add in last register done             **
 **   -- Access pattern: round 2 works mod 5, round 3 works mod 3     **
 **   -- distinct additive constant for each step                     **
 **   -- round 4 added, working mod 7                                 **
 ***********************************************************************
 */

/*
 **********************************************************************
 ** Copyright (C) 1990, RSA Data Security, Inc. All rights reserved. **
 **                                                                  **
 ** License to copy and use this software is granted provided that   **
 ** it is identified as the "RSA Data Security, Inc. MD5 Message     **
 ** Digest Algorithm" in all material mentioning or referencing this **
 ** software or this function.                                       **
 **                                                                  **
 ** License is also granted to make and use derivative works         **
 ** provided that such works are identified as "derived from the RSA **
 ** Data Security, Inc. MD5 Message Digest Algorithm" in all         **
 ** material mentioning or referencing the derived work.             **
 **                                                                  **
 ** RSA Data Security, Inc. makes no representations concerning      **
 ** either the merchantability of this software or the suitability   **
 ** of this software for any particular purpose.  It is provided "as **
 ** is" without express or implied warranty of any kind.             **
 **                                                                  **
 ** These notices must be retained in any copies of any part of this **
 ** documentation and/or software.                                   **
 **********************************************************************
 */

/* typedef a 32 bit type */
typedef unsigned32 UINT4;

/* Data structure for MD5 (Message Digest) computation */
typedef struct {
  UINT4 i[2];                   /* number of _bits_ handled mod 2^64 */
  UINT4 buf[4];                                    /* scratch buffer */
  unsigned char in[64];                              /* input buffer */
  unsigned char digest[16];     /* actual digest after MD5Final call */
} MD5_CTX;

extern void MD5Init _DCE_PROTOTYPE_ ((MD5_CTX *));
extern void MD5Update _DCE_PROTOTYPE_ ((MD5_CTX *, unsigned char *, unsigned int));
extern void MD5Final _DCE_PROTOTYPE_ ((MD5_CTX *));

/*
 **********************************************************************
 ** End of md5.h                                                     **
 ******************************* (cut) ********************************
 */
#endif /* _MD5_KRPC_H */
