/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgrq.c,v 65.5 1998/03/23 17:29:12 gwehrman Exp $";
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
 * HISTORY
 * $Log: dgrq.c,v $
 * Revision 65.5  1998/03/23 17:29:12  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/03/21 19:15:07  lmc
 * Changed the use of "#ifdef DEBUG" to "defined(SGIMIPS) &&
 * defined(SGI_RPC_DEBUG)" to turn on debugging in rpc.
 * This allows debug rpc messages to be turned on without
 * getting the kernel debugging that comes with "DEBUG".
 * "DEBUG" can change the size of some kernel data structures.
 *
 * Revision 65.3  1998/01/07  17:21:19  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:23  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:58  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.620.3  1996/02/18  00:03:57  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:25  marty]
 *
 * Revision 1.1.620.2  1995/12/08  00:20:09  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1994/10/19  16:49 UTC  tatsu_s
 * 	Merged the fix for OT12540, 12609 & 10454.
 * 
 * 	HP revision /main/tatsu_s.fix_ot12540.b0/1  1994/10/12  16:15 UTC  tatsu_s
 * 	Fixed OT12540: Added rq.first_frag_taken.
 * 	[1995/12/07  23:59:11  root]
 * 
 * Revision 1.1.614.4  1994/06/24  15:49:37  tatsu_s
 * 	MBF code cleanup.
 * 	[1994/06/03  17:58:34  tatsu_s]
 * 
 * Revision 1.1.614.3  1994/05/27  15:36:28  tatsu_s
 * 	Merged up with DCE1_1.
 * 	[1994/05/20  20:56:03  tatsu_s]
 * 
 * 	Added rq->head_fragnum.
 * 	[1994/05/19  20:22:54  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	[1994/04/29  19:16:23  tatsu_s]
 * 
 * Revision 1.1.614.2  1994/05/19  21:14:45  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:41:19  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:21:07  hopkins]
 * 
 * Revision 1.1.614.1  1994/01/21  22:37:11  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:12  cbrooks]
 * 
 * Revision 1.1.5.5  1993/01/14  14:53:42  markar
 * 	    OT CR 6416 fix: initialize recvq's high_serial_num field.
 * 	[1993/01/13  21:57:42  markar]
 * 
 * Revision 1.1.5.4  1993/01/13  21:22:49  markar
 * 	     OT CR 6175 fix: Initialize recvq's is_way_validated field.
 * 	[1993/01/13  19:49:10  markar]
 * 
 * Revision 1.1.5.3  1993/01/03  23:24:35  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:49  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  20:49:00  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:35  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/07  19:27:07  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:33:09  markar]
 * 
 * Revision 1.1  1992/01/19  03:06:18  devrcs
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
**      dgrq.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  Handles receive queues.
**
**
*/

#include <dg.h>
#include <dgrq.h>
#include <dgpkt.h>

/* ========================================================================= */

#define DEFAULT_WAKE_THREAD_QSIZE   (6 * 1024)

/* ========================================================================= */

/*
 * R P C _ _ D G _ R E C V Q _ I N I T
 *
 * Initialize a receive queue (rpc_dg_recv_q_t).
 */

PRIVATE void rpc__dg_recvq_init
#ifdef _DCE_PROTO_
(
    rpc_dg_recvq_p_t rq
)
#else
(rq)
rpc_dg_recvq_p_t rq;
#endif
{
    /*
     * presumably the call is either locked or 'private' at this point
     * RPC_DG_CALL_LOCK_ASSERT(call);
     */

    rq->head = rq->last_inorder = NULL;
                          
    /*
     * high_rcv_frag_size should be set to zero. However, since its
     * primary use is a detection of the sender's MBF capability, we
     * start it from rpc_c_dg_initial_max_pkt_size for a little
     * performance improvement.
     */
    rq->high_rcv_frag_size = RPC_C_DG_INITIAL_MAX_PKT_SIZE;
    rq->next_fragnum    = 0;
    rq->high_fragnum    = -1;
    rq->high_serial_num = -1;
    rq->head_fragnum    = -1;
    rq->head_serial_num = -1;

    rq->wake_thread_qsize = DEFAULT_WAKE_THREAD_QSIZE;
    rq->max_queue_len = RPC_C_DG_MAX_RECVQ_LEN;

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
    /*
     * For testing, allow an override via debug switch 10.
     */
    if (RPC_DBG (rpc_es_dbg_dg_rq_qsize, 1))
        rq->wake_thread_qsize = ((unsigned32)
            (rpc_g_dbg_switches[(int) rpc_es_dbg_dg_rq_qsize])) * 1024;
#endif

    rq->queue_len       = 0;
    rq->inorder_len     = 0;
    rq->recving_frags   = false;
    rq->all_pkts_recvd  = false;
    rq->is_way_validated= false;
    rq->first_frag_taken= false;
}


/*
 * R P C _ _ D G _ R E C V Q _ F R E E
 *
 * Frees data referenced by a receive queue (rpc_dg_recv_q_t).  The
 * receive queue itself is NOT freed, since it's (assumed to be) part
 * of a larger structure.
 */

PRIVATE void rpc__dg_recvq_free
#ifdef _DCE_PROTO_
(
    rpc_dg_recvq_p_t rq
)
#else
(rq)
rpc_dg_recvq_p_t rq;
#endif
{
    /*
     * Presumably the call is either locked or 'private' at this point.
     * The NULL call handle passed to free_rqe() below, implies that we
     * are sure that this call is not currently blocked waiting for a 
     * packet.
     *
     * RPC_DG_CALL_LOCK_ASSERT(call);
     */

    while (rq->head != NULL) {
        rpc_dg_recvq_elt_p_t rqe = rq->head;

        rq->head = rqe->next;
        rpc__dg_pkt_free_rqe(rqe, NULL);
    }
}
