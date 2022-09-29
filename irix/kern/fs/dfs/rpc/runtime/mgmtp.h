/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: mgmtp.h,v $
 * Revision 65.1  1997/10/20 19:17:04  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.307.2  1996/02/18  22:56:29  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:34  marty]
 *
 * Revision 1.1.307.1  1995/12/08  00:21:02  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:45  root]
 * 
 * Revision 1.1.305.1  1994/01/21  22:38:08  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:25  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:53:00  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:09:09  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:12:09  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:40:54  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:10:59  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _MGMTP_H
#define _MGMTP_H	1

/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      mgmtp.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Private management services.
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


PRIVATE unsigned32 rpc__mgmt_init _DCE_PROTOTYPE_ (( void ));

PRIVATE boolean32 rpc__mgmt_authorization_check _DCE_PROTOTYPE_ ((
        rpc_binding_handle_t     /*binding_h*/,
        unsigned32               /*op*/,
        boolean32                /*deflt*/,
        unsigned32              * /*status*/
    ));

PRIVATE void rpc__mgmt_stop_server_lsn_mgr _DCE_PROTOTYPE_ ((            
        rpc_binding_handle_t     /*binding_h*/,
        unsigned32              * /*status*/
    ));


#ifdef __cplusplus
}
#endif

#endif /* _MGMTP_H */
