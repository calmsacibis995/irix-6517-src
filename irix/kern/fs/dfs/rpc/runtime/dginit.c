/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dginit.c,v 65.5 1998/03/23 17:30:33 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/* 
 * (c) Copyright 1991, 1992 
 *     Siemens-Nixdorf Information Systems, Burlington, MA, USA
 *     All Rights Reserved
 */
/*
 * HISTORY
 * $Log: dginit.c,v $
 * Revision 65.5  1998/03/23 17:30:33  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:21:40  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:40  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:56  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.502.2  1996/02/18  00:03:46  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:16  marty]
 *
 * Revision 1.1.502.1  1995/12/08  00:19:56  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:04  root]
 * 
 * Revision 1.1.499.4  1994/07/15  16:56:20  tatsu_s
 * 	Fix OT10589: Reversed the execution order of fork_handlers.
 * 	[1994/07/09  02:44:38  tatsu_s]
 * 
 * Revision 1.1.499.3  1994/05/19  21:14:39  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:39:30  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:19:53  hopkins]
 * 
 * Revision 1.1.499.2  1994/04/15  18:57:28  tom
 * 	Support for dced (OT 10352):
 * 	  Add call to rpc__dg_fwd_init.
 * 	[1994/04/14  18:47:55  tom]
 * 
 * Revision 1.1.499.1  1994/01/21  22:37:01  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:04  cbrooks]
 * 
 * Revision 1.1.48.2  1993/09/16  20:18:26  damon
 * 	Synched with ODE 2.1 based build
 * 	[1993/09/16  20:18:08  damon]
 * 
 * Revision 1.1.5.3  1993/09/16  18:43:46  tatsu_s
 * 	Bug 8103 - Put the atfork handler under '#ifdef ATFORK_SUPPORTED' for
 * 	kruntime.
 * 	[1993/09/16  18:42:57  tatsu_s]
 * 
 * Revision 1.1.5.2  1993/09/14  16:50:45  tatsu_s
 * 	Bug 8103 - Added the call to rpc__dg_conv_init() in
 * 	rpc__ncadg_init().
 * 	[1993/09/13  17:09:52  tatsu_s]
 * 
 * Revision 1.1.3.5  1993/01/03  23:24:19  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:26  bbelch]
 * 
 * Revision 1.1.3.4  1992/12/23  20:48:27  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:12  zeliff]
 * 
 * Revision 1.1.3.3  1992/12/21  21:30:11  sommerfeld
 * 	Add fork handler for conv thread.
 * 	[1992/12/07  22:12:35  sommerfeld]
 * 
 * Revision 1.1.3.2  1992/09/29  20:41:42  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:45:36  weisman]
 * 
 * Revision 1.1  1992/01/19  03:10:18  devrcs
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
**      dginit.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  
**
**
*/

#include <dg.h>
#include <dghnd.h>
#include <dgcall.h>
#include <dgslive.h>
#include <dgxq.h>
#include <dgrq.h>
#include <dgpkt.h>
#include <dgcct.h>
#include <dgsct.h>
#include <dgccallt.h>
#include <dgfwd.h>

#include <comprot.h>

void rpc__ncadg_init
#ifdef _DCE_PROTO_
(
    rpc_prot_call_epv_t **call_epv,
    rpc_prot_mgmt_epv_t **mgmt_epv,
    rpc_prot_binding_epv_t **binding_epv,
    rpc_prot_network_epv_t **network_epv, 
    rpc_prot_fork_handler_fn_t *fork_handler,
    unsigned32 *st
)
#else
(call_epv, mgmt_epv, binding_epv, network_epv, fork_handler, st)
rpc_prot_call_epv_t **call_epv;
rpc_prot_mgmt_epv_t **mgmt_epv;
rpc_prot_binding_epv_t **binding_epv;
rpc_prot_network_epv_t **network_epv; 
rpc_prot_fork_handler_fn_t *fork_handler;
unsigned32 *st;
#endif
{
#ifdef KERNEL
    extern int gettimeofday(struct timeval *, struct timezone *);
#endif /* KERNEL */

    static rpc_prot_call_epv_t dg_call_epv =
    {
        rpc__dg_call_start,
        rpc__dg_call_transmit,
        rpc__dg_call_transceive,
        rpc__dg_call_receive,
        rpc__dg_call_end,
        rpc__dg_call_block_until_free,
        rpc__dg_call_fault,
        rpc__dg_call_alert,
        rpc__dg_call_receive_fault,
        rpc__dg_call_did_mgr_execute
    };
    static rpc_prot_mgmt_epv_t dg_mgmt_epv =
    {
        rpc__dg_mgmt_inq_calls_sent,
        rpc__dg_mgmt_inq_calls_rcvd,
        rpc__dg_mgmt_inq_pkts_sent,
        rpc__dg_mgmt_inq_pkts_rcvd
    };
    static rpc_prot_binding_epv_t dg_binding_epv =
    {
        rpc__dg_binding_alloc,
        rpc__dg_binding_init,
        rpc__dg_binding_reset,
        rpc__dg_binding_changed,
        rpc__dg_binding_free,
        rpc__dg_binding_inq_addr,
        rpc__dg_binding_inq_client,
        rpc__dg_binding_copy,
        rpc__dg_binding_cross_fork
    };
    static rpc_prot_network_epv_t dg_network_epv =
    {
        rpc__dg_network_use_protseq_sv,
        rpc__dg_network_mon,
        rpc__dg_network_stop_mon,
        rpc__dg_network_maint,
        rpc__dg_network_stop_maint,
        rpc__dg_network_select_dispatch,
        rpc__dg_network_inq_prot_vers
    };
    
    *call_epv    = &dg_call_epv;
    *mgmt_epv    = &dg_mgmt_epv;
    *binding_epv = &dg_binding_epv;
    *network_epv = &dg_network_epv; 
#ifdef ATFORK_SUPPORTED
    *fork_handler= rpc__ncadg_fork_handler;
#else
    *fork_handler= NULL;
#endif
    /*
     * Establish a server boot time.
     */

    if (rpc_g_dg_server_boot_time == 0) {
        struct timeval tv;

#ifdef SNI_SVR4
	gettimeofday (&tv);
#else
        gettimeofday(&tv, NULL);
#endif /* SNI_SVR4 */
        rpc_g_dg_server_boot_time = tv.tv_sec;
    }

    rpc__dg_pkt_pool_init();

    rpc__dg_network_init();
        
    rpc__dg_maintain_init();
    rpc__dg_monitor_init();

    rpc__dg_conv_init();
    rpc__dg_fwd_init();

#ifndef _KERNEL
    if (RPC_DBG(rpc_es_dbg_stats, 5))
    {
        atexit(rpc__dg_stats_print);
    }
#endif

    *st = rpc_s_ok;
}  

#ifdef ATFORK_SUPPORTED
/*
**++
**
**  ROUTINE NAME:       rpc__ncadg_fork_handler
**
**  SCOPE:              PRIVATE - declared in comprot.h
**
**  DESCRIPTION:
**      
**  This routine is called prior to, and immediately after, forking
**  the process's address space.  The input argument specifies which
**  stage of the fork we're currently in.
**
**  INPUTS:             
**
**        stage         indicates the stage in the fork operation
**                      (prefork | postfork_parent | postfork_child)
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/       


void rpc__ncadg_fork_handler
#ifdef _DCE_PROTO_
(
    rpc_fork_stage_id_t stage
)
#else
(stage)
rpc_fork_stage_id_t stage;
#endif
{                          
    /*
     * Pre-fork handlers are called in reverse order of rpc__ncadg_init().
     * Post-fork handlers are called in same order, except pkt_pool.
     *
     * First, call any module specific fork handlers.
     * Next, handle any stage-specific operations for this
     * module.
     */
    switch ((int)stage)
    {
    case RPC_C_PREFORK:
        rpc__dg_conv_fork_handler(stage);
        rpc__dg_cct_fork_handler(stage);
        rpc__dg_sct_fork_handler(stage);
        rpc__dg_ccallt_fork_handler(stage); 
        rpc__dg_monitor_fork_handler(stage);
        rpc__dg_maintain_fork_handler(stage);
        rpc__dg_network_fork_handler(stage);
        rpc__dg_pkt_pool_fork_handler(stage);
        break;
    case RPC_C_POSTFORK_CHILD:  
        /*
         * Clear out statistics gathering structure
         */      
        /* b_z_e_r_o_((char *) &rpc_g_dg_stats, sizeof(rpc_dg_stats_t)); */
        
        memset( &rpc_g_dg_stats, 0, sizeof(rpc_dg_stats_t));
        /*
         * Reset Server Boot Time.  
         */
        rpc_g_dg_server_boot_time = 0;
        /* fall through */
    case RPC_C_POSTFORK_PARENT:
        /* pkt_pool */
        rpc__dg_network_fork_handler(stage);
        rpc__dg_maintain_fork_handler(stage);
        rpc__dg_monitor_fork_handler(stage);
        rpc__dg_ccallt_fork_handler(stage); 
        rpc__dg_sct_fork_handler(stage);
        rpc__dg_cct_fork_handler(stage);
        rpc__dg_conv_fork_handler(stage);
        /*
         * conv_fork_handler() must be called before
         * pkt_pool_fork_handler().
         */
        rpc__dg_pkt_pool_fork_handler(stage);
        break;
    }
}
#endif /* ATFORK_SUPPORTED */
