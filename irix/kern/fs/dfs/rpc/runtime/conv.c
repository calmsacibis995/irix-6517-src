/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: conv.c,v 65.5 1998/03/23 17:30:48 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: conv.c,v $
 * Revision 65.5  1998/03/23 17:30:48  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/02/26 19:05:09  lmc
 * Changes for sockets using behaviors.  Prototype and cast changes.
 *
 * Revision 65.3  1998/01/07  17:21:43  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:43  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.619.1  1996/05/10  13:10:43  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:13 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:41 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Submitted the fix for CHFts16024.
 * 	[1995/09/08  15:05 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:00 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:31 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Submitted the fix for CHFts16024.
 * 	[1995/09/08  15:05 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	Update awaiting_ack_timestamp.
 * 	[1995/09/06  21:34 UTC  tatsu_s  /main/HPDCE02/tatsu_s.dced_startup.b0/1]
 *
 * 	Submitted the local rpc security bypass.
 * 	[1995/03/06  17:19 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	Added the trusted parameter to rpc__krb_dg_way_handler().
 * 	[1995/02/27  22:04 UTC  tatsu_s  /main/tatsu_s.local_rpc.b1/1]
 *
 * Revision 1.1.615.2  1994/01/28  23:09:23  burati
 * 	rpc_c_mem_dg_epac is now uppercase for code cleanup
 * 	[1994/01/28  04:23:13  burati]
 * 
 * 	EPAC support - Add conv_who_are_you_auth_more (dlg_bl1)
 * 	[1994/01/24  22:12:48  burati]
 * 
 * Revision 1.1.615.1  1994/01/21  22:36:12  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:17  cbrooks]
 * 
 * Revision 1.1.4.4  1993/01/03  23:23:14  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:03:40  bbelch]
 * 
 * Revision 1.1.4.3  1992/12/23  20:46:09  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:35:26  zeliff]
 * 
 * Revision 1.1.4.2  1992/12/21  21:29:15  sommerfeld
 * 	Fix typo in previous change; use correct LOCK/UNLOCK macros.
 * 	[1992/12/21  20:37:40  sommerfeld]
 * 
 * 	Unlock call lock and global lock around callout to auth handling code.
 * 	[1992/12/18  23:05:00  sommerfeld]
 * 
 * Revision 1.1.2.2  1992/05/01  17:02:53  rsalz
 * 	 05-feb-92 sommerfeld fix stylistic problems.
 * 	 05-feb-92 sommerfeld - deal with key reorg
 * 	                      - don't die if we don't think call is
 * 	                        authenticated
 * 	[1992/05/01  16:50:29  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:07:16  devrcs
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
**      conv.c
**
**  FACILITY:
**
**      Conversation Manager (CONV)
**
**  ABSTRACT:
**
**  Implement conversation manager callback procedures for NCA RPC
**  datagram protocol.
**
**
*/

#include <dg.h>
#include <dgccall.h>
#include <dgccallt.h>

#include <dce/conv.h>
#include <dgglob.h>



/* ========================================================================= */

INTERNAL boolean conv_common  _DCE_PROTOTYPE_ ((
        uuid_t * /*actuid*/,
        unsigned32 /*boot_time*/,
        rpc_dg_ccall_p_t * /*ccall*/,
        unsigned32 * /*st*/
    ));

/* ========================================================================= */

/*
 * C O N V _ C O M M O N
 *
 * Common routine used by conv_who_are_you() and conv_are_you_there()
 * to find the client call handle associated with the incoming callback
 * and verify that we are communicating with the expected instance of
 * the server.
 */
   
INTERNAL boolean conv_common
#ifdef _DCE_PROTO_
(
    uuid_t *actuid,
    unsigned32 boot_time,
    rpc_dg_ccall_p_t *ccall,
    unsigned32 *st
)
#else
(actuid, boot_time, ccall, st)
uuid_t *actuid;
unsigned32 boot_time;
rpc_dg_ccall_p_t *ccall;
unsigned32 *st;
#endif
{
    /*
     * Find the call he's asking about.
     */

    *ccall = rpc__dg_ccallt_lookup(actuid, RPC_C_DG_NO_HINT);

    /*
     * No ccall entry will exist if an old duplicate WAY callback request
     * is received.  If there is no ccall entry corresponding to the
     * incoming activity id then we return an error status code.  The
     * server performing the WAY callback will detect the error and send
     * a reject packet that will be dropped by the client.
     */

    if (*ccall == NULL) 
    {
        *st = nca_s_bad_actid;
        return (false);
    }

    /*
     * If we don't know the boot time yet for this server, stash it away
     * now.
     */

    if ((*ccall)->c.call_server_boot == 0)
    {
        (*ccall)->c.call_server_boot = boot_time;
    }
    else 
    {
        /*
         * We DO know the boot time.  If what the server's supplied boot
         * time isn't what we think it should be, then we must be getting
         * called back by a new instance of the server (i.e., which
         * received a duplicate of an outstanding request of ours).  Since
         * we can't know whether the original instance of the server
         * executed our call or not, we return failure to the server
         * to prevent IT from executing call (i.e., for a possible second
         * time, violating the "at most once" rule).
         */
        if ((*ccall)->c.call_server_boot != boot_time)
        {
            *st = nca_s_you_crashed;
            RPC_DG_CCALL_RELEASE(ccall);
            return (false);
        }
    }

    *st = rpc_s_ok;
    return (true);
}


/*
 * C O N V _ W H O _ A R E _ Y O U
 *
 * This procedure is called remotely by a server that has no "connection"
 * information about a client from which it has just received a call.  This
 * call executes in the context of that client (i.e. it is a "call back").
 * We return the current sequence number of the client and his "identity".
 * We accept the boot time from the server for later use in calls to the
 * same server.
 */

PRIVATE void conv_who_are_you
#ifdef _DCE_PROTO_
(
    handle_t h,       /* Not really */
    uuid_t *actuid,
    unsigned32 boot_time,
    unsigned32 *seq,
    unsigned32 *st
)
#else
(h, actuid, boot_time, seq, st)
handle_t h;       /* Not really */
uuid_t *actuid;
unsigned32 boot_time;
unsigned32 *seq;
unsigned32 *st;
#endif
{
    rpc_dg_ccall_p_t ccall;

    RPC_LOCK_ASSERT(0);

    if (! conv_common(actuid, boot_time, &ccall, st))
    {
        return;
    }

    *seq = ccall->c.call_seq;
    if (*st == rpc_s_ok && ccall->c.xq.awaiting_ack)
    {
	/*
	 * Since we are responding to the valid callback, let's
	 * update the awaiting_ack_timestamp to avoid the delay
	 * caused by this processing.
	 */
	ccall->c.xq.awaiting_ack_timestamp = rpc__clock_stamp();
    }
    RPC_DG_CCALL_RELEASE(&ccall);
} 


/*
 * C O N V _ W H O _ A R E _ Y O U 2
 *
 * This procedure is called remotely by a server that needs to monitor the
 * the liveness of this client.  We return to it a UUID unique to this 
 * particular instance of this particular application.  We also return all
 * the information from a normal WAY callback, so that in the future servers
 * will only need to make this call to get all client information.
 */

PRIVATE void conv_who_are_you2
#ifdef _DCE_PROTO_
(
    handle_t h,       /* Not really */
    uuid_t *actuid,
    unsigned32 boot_time,
    unsigned32 *seq,
    uuid_t *cas_uuid,
    unsigned32 *st
)
#else
(h, actuid, boot_time, seq, cas_uuid, st)
handle_t h;       /* Not really */
uuid_t *actuid;
unsigned32 boot_time;
unsigned32 *seq;
uuid_t *cas_uuid;
unsigned32 *st;
#endif
{
    conv_who_are_you(h, actuid, boot_time, seq, st);
    *cas_uuid = rpc_g_dg_my_cas_uuid;
}   


/*
 * C O N V _ A R E _ Y O U _ T H E R E
 *
 * This procedure is called remotely by a server (while in the receive
 * state) that is trying to verify whether or not it's client is still
 * alive and sending input data.  This call executes in the context of
 * that client (i.e. it is a "call back").  We check that the client
 * call is still active and that the boot time from the server matches
 * that the active CCALL
 * 
 * Note that only the server side of this callback is currently
 * implemented.
 */
   
PRIVATE void conv_are_you_there
#ifdef _DCE_PROTO_
(
    handle_t h,       /* Not really */
    uuid_t *actuid,
    unsigned32 boot_time,
    unsigned32 *st
)
#else
(h, actuid, boot_time, st)
handle_t h;       /* Not really */
uuid_t *actuid;
unsigned32 boot_time;
unsigned32 *st;
#endif
{
    rpc_dg_ccall_p_t ccall;

    RPC_LOCK_ASSERT(0);

    if (! conv_common(actuid, boot_time, &ccall, st))
    {
        return;
    }

    RPC_DG_CCALL_RELEASE(&ccall);
} 


/*
 * C O N V _ W H O _ A R E _ Y O U _ A U T H
 *
 * This procedure is called remotely by a server that has no "connection"
 * information about a client from which it has just received a call,
 * when the incoming call is authenticated.  In addition to the
 * information returned by conv_who_are_you, a challenge-response
 * takes place to inform the server of the identity of the client.
 */

PRIVATE void conv_who_are_you_auth 
#ifdef _DCE_PROTO_
(
    handle_t h, /* not really */
    uuid_t *actuid,
    unsigned32 boot_time,
    ndr_byte *in_data,
    signed32 in_len,
    signed32 out_max_len,
    unsigned32 *seq,
    uuid_t *cas_uuid,
    ndr_byte *out_data,
    signed32 *out_len,
    unsigned32 *st
)
#else
(h, actuid, boot_time, in_data, in_len, out_max_len, seq, cas_uuid, out_data, out_len, st)
handle_t h; /* not really */
uuid_t *actuid;
unsigned32 boot_time;
ndr_byte *in_data;
signed32 in_len;
signed32 out_max_len;
unsigned32 *seq;
uuid_t *cas_uuid;
ndr_byte *out_data;
signed32 *out_len;
unsigned32 *st;
#endif
{
    rpc_dg_ccall_p_t ccall;
    rpc_dg_auth_epv_p_t epv;
    ndr_byte *save_out_data = out_data;
    
    RPC_LOCK_ASSERT(0);
    
    if (! conv_common(actuid, boot_time, &ccall, st))
    {
        return;
    }

    *cas_uuid = rpc_g_dg_my_cas_uuid;
    *seq = ccall->c.call_seq;

    /*
     * If there's already a credentials buffer associated with this
     * call handle, free it.  We rely on the underlying security code
     * to do cacheing if appropriate.
     */
    if (ccall->auth_way_info != NULL)
    {
        RPC_MEM_FREE(ccall->auth_way_info, RPC_C_MEM_DG_EPAC);
        ccall->auth_way_info     = NULL;
        ccall->auth_way_info_len = 0;
    }

    /* 
     * Make sure that we really have an authenticated call here, 
     * lest we dereference null and blow up the process.
     */
    epv = ccall->c.auth_epv;
    if (epv == NULL) 
    {
        *st = rpc_s_binding_has_no_auth;
    } 
    else 
    {
	RPC_DG_CALL_UNLOCK(&(ccall->c));
	RPC_UNLOCK(0);
	
	(*epv->way_handler) (ccall->c.key_info, in_data, in_len,
            out_max_len, &out_data, out_len, st);

	RPC_LOCK(0);
	RPC_DG_CALL_LOCK(&(ccall->c));

        if (*st == rpc_s_ok && *out_len > out_max_len)
        {
            /*
             * If the credentials did not fit in the buffer provided,
             * the WAY handler will have alloced up a buffer big enough
             * to hold them, and returned a pointer to that storage in
             * out_data.  
             *
             * Stash a pointer to this buffer in the call handle, copy 
             * as much of the credentials as will fit in the real response 
             * packet, and return a status that indicates that the caller 
             * needs to fetch the rest of the credentials.
             */
            ccall->auth_way_info = out_data;
            ccall->auth_way_info_len = *out_len;

            memcpy(save_out_data, out_data, out_max_len);
            *out_len = out_max_len;

            *st = rpc_s_partial_credentials;
        }
    }
    if (rpc_g_is_single_threaded
	&& (*st == rpc_s_ok || *st == rpc_s_partial_credentials)
	&& ccall->c.xq.awaiting_ack)
    {
	/*
	 * Since we are responding to the valid callback, let's
	 * update the awaiting_ack_timestamp to avoid the delay
	 * caused by this processing.
	 *
	 * If multi-threaded, we are not inline.
	 */
	ccall->c.xq.awaiting_ack_timestamp = rpc__clock_stamp();
    }
    RPC_DG_CCALL_RELEASE(&ccall);
}

/*
 * C O N V _ W H O _ A R E _ Y O U _ A U T H _ M O R E
 *
 * This routine is used, in conjunction with the conv_who_are_you_auth()
 * operation, for retrieving oversized PACs.  In the event that a client's
 * credentials are too large to fit within a single DG packet, the server
 * can retrieve the PAC, packet by packet, by repeated calls on this
 * operation.
 *
 * Note that because the "conv" interface is serviced directly by the
 * the DG protocol (as opposed to being handled by a call executor thread),
 * there is no additional client overhead with retrieving the PAC by
 * multiple single-packet calls, rather than a single multiple-packet call.
 * The small amount of overhead induced on the server side is more than
 * compensated for by being able to avoid adding flow-control/windowing
 * to the DG protocol's internal handling of the conv interface.
 *
 * Logically, this call returns
 *
 *        client_credentials[index ... (index+out_max_len-1)]
 */

#ifdef SGIMIPS
PRIVATE void conv_who_are_you_auth_more (
handle_t h, /* not really */
uuid_t *actuid,
unsigned32 boot_time,
signed32 index,
signed32 out_max_len,
ndr_byte *out_data,
signed32 *out_len,
unsigned32 *st)
#else
PRIVATE void conv_who_are_you_auth_more (h, actuid, boot_time, index,
                                         out_max_len, out_data, out_len, st)
handle_t h; /* not really */
uuid_t *actuid;
unsigned32 boot_time;
signed32 index;
signed32 out_max_len;
ndr_byte *out_data;
signed32 *out_len;
unsigned32 *st;
#endif
{
    rpc_dg_ccall_p_t ccall;
    rpc_dg_auth_epv_p_t epv;

    RPC_LOCK_ASSERT(0);

    if (! conv_common(actuid, boot_time, &ccall, st))
    {
        return;
    }

    if (ccall->auth_way_info == NULL || ccall->auth_way_info_len == 0
	|| index >= ccall->auth_way_info_len)
    {
        *st = rpc_s_auth_badorder;	/* Need a better status! */
    }
    else
    if (index + out_max_len >= ccall->auth_way_info_len)
    {
        *out_len = ccall->auth_way_info_len - index;
        *st = rpc_s_ok;
    }
    else
    {
        *out_len = out_max_len;
        *st = rpc_s_partial_credentials;
    }

    memcpy(out_data, ccall->auth_way_info + index, *out_len);
    if (rpc_g_is_single_threaded
	&& (*st == rpc_s_ok || *st == rpc_s_partial_credentials)
	&& ccall->c.xq.awaiting_ack)
    {
	/*
	 * Since we are responding to the valid callback, let's
	 * update the awaiting_ack_timestamp to avoid the delay
	 * caused by this processing.
	 *
	 * If multi-threaded, we are not inline.
	 */
	ccall->c.xq.awaiting_ack_timestamp = rpc__clock_stamp();
    }
    RPC_DG_CCALL_RELEASE(&ccall);
}
