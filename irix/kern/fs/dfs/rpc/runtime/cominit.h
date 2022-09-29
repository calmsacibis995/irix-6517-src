/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cominit.h,v $
 * Revision 65.1  1997/10/20 19:16:47  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.68.2  1996/02/18  22:55:44  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:14:54  marty]
 *
 * Revision 1.1.68.1  1995/12/08  00:18:19  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:30 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/31  21:16 UTC  tatsu_s
 * 	Submitted the local rpc cleanup.
 * 
 * 	HP revision /main/tatsu_s.vtalarm.b0/1  1995/01/26  20:37 UTC  tatsu_s
 * 	Added rpc__exit_handler().
 * 	[1995/12/07  23:58:10  root]
 * 
 * Revision 1.1.66.1  1994/01/21  22:35:25  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:11  cbrooks]
 * 
 * Revision 1.1.5.1  1993/09/28  21:58:08  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:33  weisman]
 * 
 * Revision 1.1.2.3  1993/01/03  23:22:10  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:55  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:43:53  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:33:37  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:07:09  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMINIT_H
#define _COMINIT_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      cominit.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Interface to the Common Communications Service Initialization Service.
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/***********************************************************************/
/*
 * R P C _ _ I N I T
 *
 */

PRIVATE void rpc__init _DCE_PROTOTYPE_ (( void ));


PRIVATE void rpc__fork_handler _DCE_PROTOTYPE_ ((
        rpc_fork_stage_id_t   /*stage*/
        
    ));


PRIVATE void rpc__set_port_restriction_from_string _DCE_PROTOTYPE_ ((
        unsigned_char_p_t  /*input_string*/,
        unsigned32         * /*status*/
    ));

#ifdef __cplusplus
}
#endif

#endif /* _COMINIT_H */

