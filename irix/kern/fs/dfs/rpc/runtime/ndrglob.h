/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ndrglob.h,v $
 * Revision 65.1  1997/10/20 19:17:04  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.309.2  1996/02/18  22:56:31  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:35  marty]
 *
 * Revision 1.1.309.1  1995/12/08  00:21:04  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:47  root]
 * 
 * Revision 1.1.307.1  1994/01/21  22:38:13  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:27  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:53:07  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:09:22  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:12:26  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:41:07  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:12:24  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _NDRGLOB_H
#define _NDRGLOB_H 1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      ndrglob.h
**
**  FACILITY:
**
**      Network Data Representation (NDR)
**
**  ABSTRACT:
**
**  Runtime global variable (external) declarations.
**
**
*/

    /*
     * Local data representation.
     *
     * "ndr_g_local_drep" is what stubs use when they're interested in
     * the local data rep.  "ndr_local_drep_packed" is the actual correct
     * 4-byte wire format of the local drep, suitable for copying into
     * packet headers.
     */

EXTERNAL u_char ndr_g_local_drep_packed[4];

EXTERNAL ndr_format_t ndr_g_local_drep;

    /*
     * A constant transfer syntax descriptor that says "NDR".
     */

EXTERNAL rpc_transfer_syntax_t ndr_g_transfer_syntax;

#endif /* _NDRGLOB_H */
