/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: pac_util_krpc.h,v $
 * Revision 65.1  1997/10/20 19:16:27  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.33.2  1996/02/18  23:46:45  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:24  marty]
 *
 * Revision 1.1.33.1  1995/12/08  00:15:21  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:26  root]
 * 
 * Revision 1.1.31.1  1994/01/21  22:32:19  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:33  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  22:36:42  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:11  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:39:52  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:55  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:16:20  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**
**  NAME
**
**      pac_util_krpc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Declarations of PAC utility routines for use in kernel.
**
**
*/

#ifndef _PAC_UTIL_KRPC_H
#define _PAC_UTIL_KRPC_H 1

#include <dce/dce.h>

#ifdef __cplusplus
    extern "C" {
#endif

#include <dce/id_base.h>

/*
 * Functions
 */


/* s e c _ i d _ p a c _ u t i l _ f r e e
 * 
 * Release dynamic storage associated with a PAC.
 */

void sec_id_pac_util_free _DCE_PROTOTYPE_ (( sec_id_pac_t * ));


/* s e c _ i d _ p a c _ u t i l _ c o p y
 *
 * Utility function to copy a pac.
 */

error_status_t sec_id_pac_util_copy _DCE_PROTOTYPE_ (
( sec_id_pac_t *, sec_id_pac_t *)
);

#ifdef __cplusplus
    }
#endif

#endif /* _PAC_UTIL_KRPC_H */
