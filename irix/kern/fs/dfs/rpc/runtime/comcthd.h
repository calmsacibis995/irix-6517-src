/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: comcthd.h,v $
 * Revision 65.1  1997/10/20 19:16:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.33.2  1996/02/18  22:55:42  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:14:52  marty]
 *
 * Revision 1.1.33.1  1995/12/08  00:18:08  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:04  root]
 * 
 * Revision 1.1.31.1  1994/01/21  22:35:13  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:02  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:00:00  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:31  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:19:50  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:33:13  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:08:16  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMCTHD_H
#define _COMCTHD_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      comcthd.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definitions of types/constants for the Call Thread Services
**  of the Common Communications Service component of the RPC runtime.
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


#ifdef _cplusplus
extern "C" {
#endif

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ I N I T
 *
 */

PRIVATE void rpc__cthread_init _DCE_PROTOTYPE_ ((
        unsigned32                  * /*status*/
    ));


/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ S T A R T _ A L L
 *
 */

PRIVATE void rpc__cthread_start_all _DCE_PROTOTYPE_ ((
        unsigned32              /*default_pool_cthreads*/,
        unsigned32              * /*status*/
    ));


/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ S T O P _ A L L
 *
 */

PRIVATE void rpc__cthread_stop_all _DCE_PROTOTYPE_ ((
        unsigned32              * /*status*/
    ));


/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ I N V O K E _ N U L L
 *
 */

PRIVATE void rpc__cthread_invoke_null _DCE_PROTOTYPE_ ((
        rpc_call_rep_p_t        /*call_rep*/,
        uuid_p_t                /*object*/,
        uuid_p_t                /*if_uuid*/,
        unsigned32              /*if_ver*/,
        unsigned32              /*if_opnum*/,
        rpc_prot_cthread_executor_fn_t /*cthread_executor*/,
        pointer_t               /*call_args*/,
        unsigned32              * /*status*/
    ));


/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ D E Q U E U E
 *
 */

PRIVATE boolean32 rpc__cthread_dequeue _DCE_PROTOTYPE_ ((
        rpc_call_rep_p_t        /*call*/
    ));


/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ C A N C E L
 *
 */

PRIVATE void rpc__cthread_cancel _DCE_PROTOTYPE_ ((
        rpc_call_rep_p_t        /*call*/
    ));

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ C A N C E L _ C A F
 *
 */

PRIVATE boolean32 rpc__cthread_cancel_caf _DCE_PROTOTYPE_ ((
        rpc_call_rep_p_t        /*call*/
    ));

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ C A N C E L _ E N A B L E _ P O S T I N G
 *
 */
 PRIVATE void rpc__cthread_cancel_enable_post _DCE_PROTOTYPE_ ((
        rpc_call_rep_p_t        /*call*/
    ));


#ifdef _cplusplus
}
#endif

#endif /* _COMCTHD_H */
