/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgccallt.h,v $
 * Revision 65.1  1997/10/20 19:16:54  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.307.2  1996/02/18  22:56:06  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:14  marty]
 *
 * Revision 1.1.307.1  1995/12/08  00:19:31  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:50  root]
 * 
 * Revision 1.1.305.1  1994/01/21  22:36:34  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:32  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:23:47  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:29  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:47:17  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:16  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:07:18  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      dgccallt.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines
**
**
*/

#ifndef _DGCCALLT_H
#define _DGCCALLT_H	1	

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

PRIVATE void rpc__dg_ccallt_insert _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t /*ccall*/
    ));

PRIVATE void rpc__dg_ccallt_remove _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t /*ccall*/
    ));

PRIVATE rpc_dg_ccall_p_t rpc__dg_ccallt_lookup _DCE_PROTOTYPE_((
        uuid_p_t /*actid*/,
        unsigned32 /*probe_hint*/
    ));

PRIVATE void rpc__dg_ccallt_fork_handler _DCE_PROTOTYPE_((
        rpc_fork_stage_id_t /*stage*/
   ));

#ifdef __cplusplus
}
#endif


#endif /* _DGCCALLT_H */
