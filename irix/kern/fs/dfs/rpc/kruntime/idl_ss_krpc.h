/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: idl_ss_krpc.h,v $
 * Revision 65.1  1997/10/20 19:16:26  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.33.2  1996/02/18  23:46:40  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:21  marty]
 *
 * Revision 1.1.33.1  1995/12/08  00:15:10  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:21  root]
 * 
 * Revision 1.1.31.1  1994/01/21  22:32:05  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:25  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  22:36:26  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:45  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:39:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:28  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:16:26  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */

/*
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**     idl_ss_krpc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Kernel support functions for idl stub support
**
**
*/

#ifndef _IDL_SS_KRPC_H
#define _IDL_SS_KRPC_H 1 

#include <dce/dce.h>

void * idl_malloc_krpc _DCE_PROTOTYPE_ ((size_t size));

void idl_free_krpc _DCE_PROTOTYPE_ ((void *p));

#endif				/* _IDL_SS_KRPC_H */
