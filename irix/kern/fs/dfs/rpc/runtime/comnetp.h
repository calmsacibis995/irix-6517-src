/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: comnetp.h,v $
 * Revision 65.1  1997/10/20 19:16:49  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.39.2  1996/02/18  22:55:50  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:14:58  marty]
 *
 * Revision 1.1.39.1  1995/12/08  00:18:30  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:31 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:17 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/05  18:21 UTC  markar
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/12/07  23:58:17  root]
 * 
 * Revision 1.1.37.1  1994/01/21  22:35:36  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:38  cbrooks]
 * 
 * Revision 1.1.35.1  1993/09/15  15:39:50  damon
 * 	Synched with ODE 2.1 based build
 * 	[1993/09/15  15:32:03  damon]
 * 
 * Revision 1.1.4.2  1993/09/14  16:49:19  tatsu_s
 * 	Bug 8103 - Moved rpc__nlsn_fork_handler() from com.h to here.
 * 	[1993/09/13  17:08:56  tatsu_s]
 * 
 * Revision 1.1.2.3  1993/01/03  23:22:28  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:02:24  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:44:29  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:34:09  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:08:17  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMNETP_H
#define _COMNETP_H 
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      comnetp.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**      Network Listener Service *Internal* types, etc...
**      (see comnet.c and comnlsn.c).
**
**/

/*
 * The max number of socket that the listener can keep track of.
 */

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


#ifndef RPC_C_SERVER_MAX_SOCKETS
#  define RPC_C_SERVER_MAX_SOCKETS      64
#endif

/*
 * A structure that captures the listener's information about a single socket.
 */

typedef struct
{
    rpc_socket_t                desc;           /* socket descriptor */
    rpc_protseq_id_t            protseq_id;
    rpc_protocol_id_t           protocol_id;
    rpc_prot_network_epv_p_t    network_epv;
    pointer_t                   priv_info;      /* prot service private info */
    unsigned                    busy: 1;        /* T => contains valid data */
    unsigned                    is_server: 1;   /* T => created via use_protseq */
    unsigned                    is_dynamic: 1;  /* T => dynamically alloc'd endpoint */
    unsigned                    is_active: 1;   /* T => events should NOT be discarded */
} rpc_listener_sock_t, *rpc_listener_sock_p_t;

/*
 * A structure that captures the listener's state that needs to be shared
 * between modules.
 */

typedef struct 
{
    rpc_mutex_t         mutex;
    rpc_cond_t          cond;
    unsigned16          num_desc;    /* number "busy" */
    unsigned16          high_water;  /* highest entry in use */
    unsigned32          status;      /* used to convey information about */
                                     /* the state of the table.  see     */
                                     /* rpc_server_listen.               */
    rpc_listener_sock_t socks[ RPC_C_SERVER_MAX_SOCKETS ];
    unsigned            reload_pending: 1;
} rpc_listener_state_t, *rpc_listener_state_p_t;

/*
 * The operations provided by any implementation of a Network Listener
 * "thread".
 */

#ifdef __cplusplus
extern "C" {
#endif


PRIVATE void rpc__nlsn_activate_desc _DCE_PROTOTYPE_((
        rpc_listener_state_p_t  /*lstate*/,
        unsigned32              /*idx*/,
        unsigned32              * /*status*/
    ));

PRIVATE void rpc__nlsn_deactivate_desc _DCE_PROTOTYPE_((
        rpc_listener_state_p_t  /*lstate*/,
        unsigned32              /*idx*/,
        unsigned32              * /*status*/
    ));

PRIVATE void rpc__nlsn_fork_handler _DCE_PROTOTYPE_((
        rpc_listener_state_p_t  /*lstate*/,
        rpc_fork_stage_id_t /*stage*/
    ));

#ifdef __cplusplus
}
#endif


#endif /* _COMNETP_H */
