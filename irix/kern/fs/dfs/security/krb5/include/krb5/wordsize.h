/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: wordsize.h,v $
 * Revision 65.1  1997/10/20 19:47:49  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.1  1996/06/04  22:00:24  arvind
 * 	Merge krb5 work to DCE1.2.2 mainline
 * 	[1996/05/29  20:29 UTC  mullan_s  /main/mullan_dce1.2.2_krb5_drop/1]
 *
 * 	Kerb5Beta5 KRB5_FCC_FVNO_3 merge.
 * 	[1993/10/14  17:45:19  mccann  1.1.4.1]
 *
 * Revision 1.1.6.2  1996/02/18  23:03:12  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:21:42  marty]
 * 
 * Revision 1.1.6.1  1995/12/08  17:44:23  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:57:26  root]
 * 
 * Revision 1.1.4.1  1993/10/14  17:45:19  mccann
 * 	CR8651 64bit port
 * 	[1993/10/14  17:42:47  mccann]
 * 
 * Revision 1.1.2.2  1992/12/29  14:02:15  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:54:17  zeliff]
 * 
 * Revision 1.1  1992/01/19  14:49:02  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/

/*  wordsize.h V=4 10/23/91 //littl/prgy/krb5/include/krb5
**
** Copyright (c) Hewlett-Packard Company 1991
** Unpublished work. All Rights Reserved.
**
*/
/*
 * $Source: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/security/krb5/include/krb5/RCS/wordsize.h,v $
 * $Author: jdoak $
 * $Id: wordsize.h,v 65.1 1997/10/20 19:47:49 jdoak Exp $
 *
 * Copyright 1989,1990 by the Massachusetts Institute of Technology.
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
 * Word-size related definition.
 */


#ifndef KRB5_WORDSIZE__
#define KRB5_WORDSIZE__

#if !defined(BITS16) && !defined(BITS32) && !defined(BITS64)
/* OSF_DCE:
 * Default to 32 bit wide words if not otherwise specified
 */
#define BITS32
#endif

#ifdef BITS16
#define __OK
typedef	int	krb5_int16;
typedef	long	krb5_int32;

typedef	unsigned char	krb5_octet;
typedef	unsigned char	krb5_ui_1;
typedef	unsigned int	krb5_ui_2;
typedef	unsigned long	krb5_ui_4;
#define VALID_INT_BITS    0x7fff
#define VALID_UINT_BITS   0xffff
#endif

#if defined(BITS32) || defined(BITS64)
#define __OK
typedef	short	krb5_int16;
typedef	int	krb5_int32;
typedef	unsigned char	krb5_octet;
typedef	unsigned char	krb5_ui_1;
typedef	unsigned short	krb5_ui_2;
typedef	unsigned int	krb5_ui_4;
#define VALID_INT_BITS    0x7fffffff
#define VALID_UINT_BITS   0xffffffff
#endif

#ifndef __OK
 ?==error:  must define word size!
#endif /* __OK */

#undef __OK

#define KRB5_INT32_MAX	2147483647
/* this strange form is necessary since - is a unary operator, not a sign
   indicator */
#define KRB5_INT32_MIN	(-KRB5_INT32_MAX-1)

#endif /* KRB5_WORDSIZE__ */
