/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgsct.c,v 65.4 1998/03/23 17:28:12 gwehrman Exp $";
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
 * $Log: dgsct.c,v $
 * Revision 65.4  1998/03/23 17:28:12  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:05  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:05  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:58  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.820.2  1996/02/18  00:04:01  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:27  marty]
 *
 * Revision 1.1.820.1  1995/12/08  00:20:14  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/3  1995/04/02  01:15 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/HPDCE02/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:19 UTC  lmm
 * 	add memory allocation failure detection
 * 
 * 	HP revision /main/HPDCE02/2  1995/03/06  17:20 UTC  tatsu_s
 * 	Submitted the local rpc security bypass.
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.local_rpc.b1/1  1995/02/22  22:31 UTC  tatsu_s
 * 	Local RPC Security Bypass.
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:16 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/16  14:22 UTC  markar
 * 	handle case here alloc_sock fails
 * 	[1995/12/07  23:59:15  root]
 * 
 * Revision 1.1.818.4  1994/06/21  21:54:02  hopkins
 * 	Serviceability
 * 	[1994/06/08  21:31:55  hopkins]
 * 
 * Revision 1.1.818.3  1994/05/27  15:36:32  tatsu_s
 * 	Merged up to the latest.
 * 	[1994/05/19  18:45:40  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	[1994/04/29  19:18:15  tatsu_s]
 * 
 * Revision 1.1.818.2  1994/05/04  20:29:41  tom
 * 	Bug 9508 - add explicit return value in rpc__dg_sct_make_way_binding.
 * 	[1994/05/04  20:26:36  tom]
 * 
 * Revision 1.1.818.1  1994/01/21  22:37:17  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:33  cbrooks]
 * 
 * Revision 1.1.7.1  1993/10/04  18:16:44  markar
 * 	    OT CR 1236 fix: Make sure inq_scall doesn't return unlocked scall
 * 	[1993/10/04  14:16:37  markar]
 * 
 * 	     OT CR 1236 fix: Make back-to-back maybe calls work.
 * 	[1993/09/30  13:39:08  markar]
 * 
 * Revision 1.1.5.4  1993/03/31  22:20:38  markar
 * 	     OT CR 6212 fix: user server com timeout setting when
 * 	                                 creating WAY handle.
 * 	[1993/03/31  21:00:47  markar]
 * 
 * Revision 1.1.5.3  1993/01/03  23:24:45  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:05  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  20:49:25  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:51  zeliff]
 * 
 * Revision 1.1.2.6  1992/05/21  13:16:50  mishkin
 * 	Raise debug level on a debug printf.
 * 	[1992/05/20  21:13:48  mishkin]
 * 
 * Revision 1.1.2.5  1992/05/15  17:23:17  mishkin
 * 	Fixes to rpc__dg_sct_way_validate():
 * 	- Make sure SCTE still has an SCALL before trying to assign to
 * 	  scte->scall->client_needs_sboot.
 * 	- Rename "force" param to "force_way_auth" so its purpose is clearer.
 * 	- Delete unused "scall" variable.
 * 	[1992/05/15  17:03:15  mishkin]
 * 
 * Revision 1.1.2.4  1992/05/12  16:18:43  mishkin
 * 	bmerge rationing from mainline
 * 	[1992/05/08  21:16:00  mishkin]
 * 
 * Revision 1.1.2.3  1992/05/07  19:42:52  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:35:12  markar]
 * 
 * Revision 1.1.3.2  1992/05/08  12:44:56  mishkin
 * 	- Convert RPC_DG_SCT_INQ_SCALL, RPC_DG_SCT_NEW_CALL, and
 * 	  RPC_DG_SCT_BACKOUT_NEW_CALL from macros to functions.
 * 	- Add support in rpc__sct_way_validate() for doing WAY's when client
 * 	  presented a zero boot time even when we otherwise would have not
 * 	  bothered to do the WAY.
 * 
 * Revision 1.1.2.2  1992/05/01  17:21:13  rsalz
 * 	 20-feb-92 mishkin       Make timeout vars be internal (for VMS).
 * 	 12-dec-91 sommerfeld    let sct timeouts be configurable.
 * 	                         add extra braces around RPC_DG_AUTH_WAY.
 * 	 05-feb-92 sommerfeld    Fix stylistic problems.
 * 	 23-jan-92 sommerfeld    bugfix to key reorg: set auth_epv to NULL when
 * 	                         zeroing key_info.
 * 	 23-jan-92 sommerfeld    key reorg.
 * 	 10-jan-92 sommerfeld    deal with change to RPC_DG_AUTH macros
 * 	                         (add extra braces in one place).
 * 	[1992/05/01  17:16:48  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:10:23  devrcs
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
**      dgsct.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  Handle server connection table (SCT).
**
**
*/

#include <dg.h>
#include <dgsct.h>
#include <dgslive.h>
#include <dgscall.h>
#include <comauth.h>

#include <dce/conv.h>

/* ========================================================================= */

/*
 * The following structure is used to register the network monitor function
 * in the timer queue.
 */

INTERNAL rpc_timer_t sct_timer;

typedef struct {
    rpc_clock_t timer_timestamp;
    unsigned32  timer_last_scanned_count;
    unsigned32  timer_last_freed_count;
} sct_stats_t;

INTERNAL sct_stats_t sct_stats;

/* 
 * Keep a count of the number of SCT entries, active or not, so we know when we
 * need to be running the monitor routine.  (We especially don't want to be 
 * running the monitor for clients that would never need it.)
 *
 * This variable is only used under the protection of the global lock.
 */

INTERNAL unsigned32 num_sct_entries = 0;
                                 
/*
 * Run the SCT monitor routine every 5 minutes, clean out entries that are over
 * 10 minutes old.
 */

#define SCT_MONITOR_INTERVAL  300
#define SCTE_TIMEOUT_INTERVAL 600

INTERNAL int rpc_g_dg_sct_mon_int = SCT_MONITOR_INTERVAL;
INTERNAL int rpc_g_dg_sct_timeout = SCTE_TIMEOUT_INTERVAL;

/* ========================================================================= */

INTERNAL void rpc__dg_sct_timer _DCE_PROTOTYPE_(( pointer_t ));


/* ========================================================================= */

/*
 * R P C _ _ D G _ S C T _ I N Q _ S C A L L
 *
 * Return the connection's locked scall.  If the connection doesn't have
 * an (accurate) previous / in-progress scall then for all practical
 * purposes it has no scall; return NULL.
 *
 * Also, check the scte's maybe chain if the current request is using
 * maybe semantics.
 */

PRIVATE void rpc__dg_sct_inq_scall
#ifdef _DCE_PROTO_
(
    rpc_dg_sct_elt_p_t scte,
    rpc_dg_scall_p_t *scallp,
    rpc_dg_recvq_elt_p_t rqe
)
#else
(scte, scallp, rqe) 
rpc_dg_sct_elt_p_t scte;
rpc_dg_scall_p_t *scallp;
rpc_dg_recvq_elt_p_t rqe;
#endif
{
    unsigned32 cur_rqe_seq = rqe->hdrp->seq;
    rpc_dg_scall_p_t temp;

    RPC_LOCK_ASSERT(0);

    /*
     * First see if the SCTE's ".scall" field holds a useable scall.
     */
    *scallp = scte->scall;
    if (*scallp != NULL)
    {
        RPC_DG_CALL_LOCK(&(*scallp)->c);

        /*
         * If the current RQE's sequence number is equal to the scall's
         * sequence number, then this RQE is for the currently running
         * call associated with the scall.  If the RQE's sequence number
         * is greater than the scall sequence number, then this RQE is
         * for a new call.  In either case, we want to use the current
         * scall for deciding how to dispense with this new RQE.
         */
        if (RPC_DG_SEQ_IS_LTE((*scallp)->c.call_seq, cur_rqe_seq))
        {
            /*
             * One last check we need to do is to make sure that the current
             * .scall does not represent a call that was "backed out" (see
             * rpc__dg_sct_backout_new_call).  We can tell this by the 
             * sequence number in the SCTE.  If this is the case, don't 
             * return the current scall, since it doesn't represent any 
             * useful state.
             */
            if ((*scallp)->c.call_seq != (scte)->high_seq)
            {
                RPC_DG_CALL_UNLOCK(&(*scallp)->c);
                *scallp = NULL;
                RPC_DBG_PRINTF(rpc_e_dbg_general, 3, (
                    "(rpc__dg_sct_inq_scall) not using backed out scall\n"));

                return;

            }
            RPC_DBG_PRINTF(rpc_e_dbg_general, 3, (
                "(rpc__dg_sct_inq_scall) using cached scall\n"));
            return;
        }
        
        /*
         * The RQE's sequence number indicates that it is for a call
         * prior to the one associated with the current .scall.  Unlock
         * the .scall, and go look for a matching scall on the maybe_chain.
         */
        RPC_DG_CALL_UNLOCK(&(*scallp)->c);
        *scallp = NULL;
    }
  
    for (temp = scte->maybe_chain; temp != NULL;
         temp = (rpc_dg_scall_p_t) temp->c.next)
    {
        RPC_DG_CALL_LOCK(&temp->c);
        if (temp->c.call_seq == cur_rqe_seq)
        {
            *scallp = temp;
            RPC_DBG_PRINTF(rpc_e_dbg_general, 3, (
                "(rpc__dg_sct_inq_scall) using scall from maybe_chain\n"));
            return;
        }
        RPC_DG_CALL_UNLOCK(&temp->c);
    }

    /*
     * return with *scallp set to NULL
     */
    RPC_DBG_PRINTF(rpc_e_dbg_general, 3, (
       "(rpc__dg_sct_inq_scall) didn't find scall\n"));
}
    

/*
 * R P C _ _ D G _ S C T _ N E W _ C A L L
 *
 * Setup the connection for the new call to be run.  Return a locked
 * scall for the new call (create one if necessary).  Make certain that
 * no one does anything crazy with the connection sequence number. The
 * scallp arg is [in,out].  If it is NULL a scall will be "materialized",
 * otherwise, the scall it identifies must already be the one associated
 * with the connection and it must already be locked (this operation
 * is paired with INQ_SCALL() above).
 */

PRIVATE void rpc__dg_sct_new_call
#ifdef _DCE_PROTO_
(
    rpc_dg_sct_elt_p_t scte,
    rpc_dg_sock_pool_elt_p_t si,
    rpc_dg_recvq_elt_p_t rqe,
    rpc_dg_scall_p_t *scallp
)
#else
(scte, si, rqe, scallp) 
rpc_dg_sct_elt_p_t scte;
rpc_dg_sock_pool_elt_p_t si;
rpc_dg_recvq_elt_p_t rqe;
rpc_dg_scall_p_t *scallp;
#endif
{
    boolean  maybe = RPC_DG_HDR_FLAG_IS_SET(rqe->hdrp, RPC_C_DG_PF_MAYBE);
    unsigned32 cur_rqe_seq = rqe->hdrp->seq;

    RPC_LOCK_ASSERT(0);

    if (*(scallp) == NULL)
    {
       /*
        * Use the SCTE's ".scall" pointer if available.  Maybe calls require
        * a little different treatment;  if the maybe call was originally sent
        * prior to the call associated with the ".scall" (i.e. has a lower 
        * sequence number), we want to make sure it doesn't count as an 
        * implicit ACK.  In this case, just alloc up a new scall.
        */

        *scallp = scte->scall;

        if (*scallp != NULL)
        {
            RPC_DG_CALL_LOCK(&(*scallp)->c);

            if (maybe && 
                RPC_DG_SEQ_IS_LT( cur_rqe_seq, (*scallp)->c.call_seq))
            {
                RPC_DG_CALL_UNLOCK(&(*scallp)->c);
                *scallp = NULL;

                RPC_DBG_PRINTF(rpc_e_dbg_general, 3, (
                    "(rpc__dg_sct_new_call) handling out-of-order maybe\n"));
            }
            else
            {
                RPC_DBG_PRINTF(rpc_e_dbg_general, 3, (
                    "(rpc__dg_sct_new_call) using cached scall\n"));
            }
        }
    }
    else
    {
        /* assert(*scallp == scte->scall || *scallp is on maybe_chain); */
    }
    if (*scallp != NULL)
    {
        rpc__dg_scall_reinit(*scallp, si, rqe);
    }
    else
    {
        *scallp = rpc__dg_scall_alloc(scte, si, rqe);
    }
    if (*scallp != NULL)
    {
        if (RPC_DG_SEQ_IS_LT((*scallp)->c.call_seq, scte->high_seq) && !maybe)
        {
	    /*
	     * rpc_m_invalid_seqnum
	     * "(%s) Invalid call sequence number"
	     */
	    RPC_DCE_SVC_PRINTF ((
		DCE_SVC(RPC__SVC_HANDLE, "%s"),
		rpc_svc_general,
		svc_c_sev_fatal | svc_c_action_abort,
		rpc_m_invalid_seqnum,
		"rpc__dg_sct_new_call" ));
        }
        if (!maybe || RPC_DG_SEQ_IS_LT(scte->high_seq, (*scallp)->c.call_seq))
            scte->high_seq = (*scallp)->c.call_seq;
    }
}


/*
 * R P C _ _ D G _ S C T _ B A C K O U T _ N E W _ C A L L
 *
 * A previously "tentatively accepted" RPC has now been unaccepted due
 * to some server resource constraints.  Backup the connection's notion
 * of the highest sequence to ensure that a (possible) subsequent
 * retransmission will be allowed to retry.
 * 
 * Don't EVER use this if the call has actually been run!
 */

PRIVATE void rpc__dg_sct_backout_new_call
#ifdef _DCE_PROTO_
(
    rpc_dg_sct_elt_p_t scte,
    unsigned32 seq
)
#else
(scte, seq) 
rpc_dg_sct_elt_p_t scte;
unsigned32 seq;
#endif
{
    RPC_LOCK_ASSERT(0);

    if (scte != NULL && seq == (scte)->high_seq)
    {
        scte->high_seq--;
    }
}


/*
 * R P C _ _ D G _ S C T _ L O O K U P
 *
 * Lookup the connection (SCTE) in the server connection table using
 * the provided actid and probe hint (ahint).  Return the scte or NULL
 * if no matching entry is found.
 * 
 * This increments the SCTE reference count (if one is found), making
 * it unavailable for GCing - it should be released when the reference
 * is no longer needed.
 */


PRIVATE rpc_dg_sct_elt_p_t rpc__dg_sct_lookup
#ifdef _DCE_PROTO_
(
    uuid_p_t actid,
    unsigned32 probe_hint
)
#else
(actid, probe_hint)
uuid_p_t actid;
unsigned32 probe_hint;
#endif
{
    rpc_dg_sct_elt_p_t scte;
    unsigned16 probe;
    boolean once = false;
    unsigned32 st;

    RPC_LOCK_ASSERT(0);

    /*
     * Determine the hash chain to use
     */

    if (probe_hint == RPC_C_DG_NO_HINT || probe_hint >= RPC_DG_SCT_SIZE)
        probe = rpc__dg_uuid_hash(actid) % RPC_DG_SCT_SIZE;
    else
        probe = probe_hint;

    /*
     * Scan down the probe chain, reserve and return a matching SCTE.
     */

RETRY:
    scte = rpc_g_dg_sct[probe];

    while (scte != NULL) 
    {
        if (UUID_EQ(*actid, scte->actid, &st)) 
        {
            RPC_DG_SCT_REFERENCE(scte);
            return(scte);
        }
        else
            scte = scte->next;
    }

    /*
     * No matching entry found.  If we used the provided hint, try
     * recomputing the probe and if this yields a new probe, give it
     * one more try.
     */

    if (probe == probe_hint && !once) 
    {
        once = true;
        probe = rpc__dg_uuid_hash(actid) % RPC_DG_SCT_SIZE;
        if (probe != probe_hint)
            goto RETRY;
    }

    return(NULL);
}


/*
 * R P C _ _ D G _ S C T _ G E T
 *
 * Lookup the connection (SCTE) in the server connection table using
 * the provided actid and probe hint (ahint); create one if necessary
 * (based on the call seq that is inducing its creation).
 * Return the SCTE.
 * 
 * This increments the SCTE reference count, making it unavailable for
 * GCing - it should be released when the reference is no longer needed.
 */

PRIVATE rpc_dg_sct_elt_p_t rpc__dg_sct_get
#ifdef _DCE_PROTO_
(
    uuid_p_t actid,
    unsigned32 probe_hint,
    unsigned32 seq
)
#else
(actid, probe_hint, seq)
uuid_p_t actid;
unsigned32 probe_hint;
unsigned32 seq;
#endif
{
    rpc_dg_sct_elt_p_t scte;
    unsigned16 probe;

    /*
     * Determine the hash chain to use.
     */

    if (probe_hint == RPC_C_DG_NO_HINT || probe_hint >= RPC_DG_SCT_SIZE)
        probe = rpc__dg_uuid_hash(actid) % RPC_DG_SCT_SIZE;
    else
        probe = probe_hint;

    /*
     * Use an existing entry if we've got one.
     */

    scte = rpc__dg_sct_lookup(actid, probe);
    if (scte != NULL)
        return(scte);

    /*
     * Create the new SCTE, reserve it and add it to the head of the list.
     */

    RPC_MEM_ALLOC(scte, rpc_dg_sct_elt_p_t, sizeof *scte, 
            RPC_C_MEM_DG_SCTE, RPC_C_MEM_NOWAIT);
    if (scte == NULL)
	return (scte);
    scte->refcnt = 0;
    scte->high_seq = seq - 1;
    scte->high_seq_is_way_validated = false;
    scte->auth_epv = NULL;
    scte->key_info = NULL;
    scte->scall = NULL;
    scte->maybe_chain = NULL;
    scte->actid = *actid;
    scte->ahint = probe;
    scte->timestamp = RPC_CLOCK_SEC(0); 
    scte->client = NULL;

    RPC_LOCK_ASSERT(0);

    /*
     * Add the new SCTE to the head of the list.
     */

    scte->next = rpc_g_dg_sct[probe];
    rpc_g_dg_sct[probe] = scte;
    RPC_DG_SCT_REFERENCE(scte);     /* This is for the reference from the SCT */

    /*
     * Increment the count of SCTE's.  If this is the first/only one,
     * add the SCT monitor to the timer queue to handle aging SCTE's.
     */

    num_sct_entries++;
    if (num_sct_entries == 1)
    {
        rpc__timer_set(&sct_timer, rpc__dg_sct_timer,
           (pointer_t) NULL, RPC_CLOCK_SEC(rpc_g_dg_sct_mon_int));
    }

    /*
     * Reserve a reference to and return the SCTE.
     */

    RPC_DG_SCT_REFERENCE(scte);

    return(scte);
} 

/*
 * R P C _ _ D G _ S C T _ T I M E R 
 * 
 * This routine searches through the SCT for entries that have not been in use
 * for a certain period of time.  It is run peridically from the timer queue.
 */

INTERNAL void rpc__dg_sct_timer
#ifdef _DCE_PROTO_
(
    pointer_t junk
)
#else
(junk)
pointer_t junk;
#endif
{
    rpc_dg_sct_elt_p_t scte, prev_scte;
    unsigned32 i;
    int sct_timeout = rpc_g_dg_sct_timeout;

    /*
     * The SCT is protected by the global lock.
     */

    RPC_LOCK(0);

    sct_stats.timer_timestamp = rpc__clock_stamp();
    sct_stats.timer_last_freed_count = 0;
    sct_stats.timer_last_scanned_count = 0;
                               
    /*
     * Look in each bucket of hash table...
     */
                    
    for (i = 0; i < RPC_DG_SCT_SIZE; i++)
    {
        scte = rpc_g_dg_sct[i];
        prev_scte = NULL;

        /*
         * Scan each chain for scte's that have aged.
         */

        while (scte != NULL)
        {
            sct_stats.timer_last_scanned_count++;
            /*
             * If the SCTE reference count is one, then the only reference
             * is from the SCT itself.  (Any other references would be
             * from SCALLs.)  So if the count is 1, and we're not
             * monitoring liveness for client associated with this SCTE,
             * and the SCTE hasn't been used in a while, release the
             * SCTE.
             */

            if (scte->refcnt == 1 &&
                (scte->client == NULL || scte->client->rundown == NULL) &&
                rpc__clock_aged(scte->timestamp, 
                                RPC_CLOCK_SEC(sct_timeout)))
            { 
                sct_stats.timer_last_freed_count++;
                if (prev_scte == NULL)
                    rpc_g_dg_sct[i] = scte->next;
                else
                    prev_scte->next = scte->next;

                /*
                 * If the SCTE has reference to some auth_info, free it.
                 */

                if (scte->key_info) 
                {
                    RPC_DG_KEY_RELEASE(scte->key_info);
                    CLOBBER_PTR(scte->key_info);
                }
                
                /*
                 * If the SCTE has a reference to a client handle (ie. was
                 * monitoring liveness) release that reference.
                 */
        
                RPC_DG_CLIENT_RELEASE(scte);
                
                /*
                 * Release the actual SCTE.
                 */

                RPC_MEM_FREE(scte, RPC_C_MEM_DG_SCTE);

                num_sct_entries--;

                scte = (prev_scte == NULL) ? rpc_g_dg_sct[i] : prev_scte->next;
            }                
            else
            {
                prev_scte = scte;
                scte = scte->next;
            }
        }
    }

    RPC_DBG_PRINTF(rpc_e_dbg_general, 5, 
        ("(sct_timer) Scanned %d Freed %d\n", 
            sct_stats.timer_last_scanned_count,
            sct_stats.timer_last_freed_count));

    /*
     * If there are no more SCTE's in the SCT, then remove the monitor
     * routine from the timer queue.
     */

    if (num_sct_entries == 0)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_general, 2, 
            ("(sct_timer) removing SCT monitor from timer queue\n"));

        rpc__timer_clear(&sct_timer);
    }

    RPC_UNLOCK(0);
}


/*
 * R P C _ _ D G _ S C T _ M A K E _ W A Y _ B I N D I N G
 *
 * Make a binding to do a WAY call with.
 *
 * All WAY calls *must* be made using a activity id other than our own
 * (i.e., we must not do a "real callback"; see conv.idl).  To achieve
 * this, we create a new call handle bound to the client.
 *
 * Alloc up a copy of the address before creating the binding handle,
 * binding_free will expect to be able to free this pointer to the address.
 */

PRIVATE rpc_binding_handle_t rpc__dg_sct_make_way_binding
#ifdef _DCE_PROTO_
(
    rpc_dg_sct_elt_p_t scte,
    unsigned32 *st
)
#else
(scte, st)
rpc_dg_sct_elt_p_t scte;
unsigned32 *st;
#endif
{
    rpc_addr_p_t way_addr;
    rpc_dg_binding_client_p_t client_binding; 
    unsigned32 xst;

    RPC_LOCK_ASSERT(0);

    /*
     * Since the scte doesn't directly contain the info necessary
     * to create a binding to the client, use the scall.
     */
    if (scte->scall == NULL)
    {
        RPC_DBG_GPRINTF(("(rpc__dg_sct_make_way_binding) no scall\n"));
        *st = rpc_s_who_are_you_failed;
        return (NULL);
    }

    rpc__naf_addr_copy(scte->scall->c.addr, &way_addr, st);

    client_binding = (rpc_dg_binding_client_p_t) rpc__binding_alloc
        (false, &uuid_g_nil_uuid, RPC_C_PROTOCOL_ID_NCADG, way_addr, st);

    if (*st != rpc_s_ok) 
    {
        RPC_DBG_GPRINTF
            (("(rpc__dg_make_way_binding) Couldn't create handle, st=0x%x\n", *st));
        rpc__naf_addr_free(&way_addr, &xst);
        return (NULL);
    }
                   
    /*
     * Indicate that this handle was created for making calls on the
     * conv interface.  The packet rationing code needs to know this
     * in order to allow the WAY/WAY2 call to inherit the packet
     * reservation made by the originating scall.
     */

    client_binding->is_WAY_binding = scte->scall->c.n_resvs;

    /*
     * Set the com_timeout value for this handle to match the one in use
     * by the server.
     */
    rpc_mgmt_set_com_timeout((rpc_binding_handle_t) client_binding,
                             rpc_mgmt_inq_server_com_timeout(), st);

    return ((rpc_binding_handle_t) client_binding);
}


/*
 * R P C _ _ D G _ S C T _ W A Y _ V A L I D A T E
 *
 * Ensure that the connection sequence information is Who-Are-You
 * Validated.  WAY validated sequence information is necessary to
 * to ensure that non-idempotent calls have at-most-once semantics.
 *
 * Server's interlude to "conv_who_are_you" remote procedure (or the
 * equivalent local procedure that handles authentication).
 *
 * The analogous "who_are_you" processing necessary for non-idempotent
 * callbacks (from a server manager to the client originating the call,
 * who needs to validate the callback's seq) is performed elsewhere.
 */

PRIVATE void rpc__dg_sct_way_validate
#ifdef _DCE_PROTO_
(
    rpc_dg_sct_elt_p_t scte,
    unsigned32 force_way_auth,
    unsigned32 *st
)
#else
(scte, force_way_auth, st)
rpc_dg_sct_elt_p_t scte;
unsigned32 force_way_auth;
unsigned32 *st;
#endif
{
    rpc_dg_sct_elt_p_t scte_ref;
    unsigned32 seq;
    unsigned32 xst;
    rpc_key_info_p_t key_info;
    uuid_t cas_uuid;            /* retrieved but ignored here. */
    rpc_binding_handle_t h;

    /*
     * The connection must be locked.
     */
    RPC_LOCK_ASSERT(0);

    *st = rpc_s_ok;

    /*
     * for lazy coders...
     */
    if (RPC_DG_SCT_IS_WAY_VALIDATED(scte) && ! force_way_auth)
        return;

    h = rpc__dg_sct_make_way_binding(scte, st);

    if (*st != rpc_s_ok)
        return;

    key_info = scte->key_info;

    /*
     * Since we're going to unlock the scte while performing the WAY,
     * acquire a reference to it to ensure that it doesn't dissappear...
     */
    scte_ref = scte;
    RPC_DG_SCT_REFERENCE(scte_ref);

    RPC_UNLOCK(0);

    /*
     * It's not entirely clear if the caller of this code will always
     * (or must) have general cancel delivery disabled.  To be flexible,
     * try to do the right thing regardless of how we were called.  A
     * failure in either scenario results is a bad status (in the case
     * of a cancel, we give the caller the opportunity to do something based
     * on this knowledge.
     *
     * Note our use of "scte->actid" is "safe" even though we don't
     * hold the scte lock at this time: (a) we do have a 'reference' to the
     * scte, so it isn't going away and (b) nobody will be changing the
     * actid.  We could copy this info while holding the lock, but why waste
     * the time / stack space?
     */
    TRY 
    {
        /*
         * Do either an AUTH-WAY or a plain WAY.  At least one AUTH-WAY
         * (per call) is required if this is an authenticated call that
         * we're validating (key_info != NULL).  However, if the SCTE
         * says that the high seq has been WAY validated, then we know
         * we must have done the AUTH-WAY (and we're here because the
         * client simply failed to present a non-zero boot time in the
         * original request); in that case, just do the plain WAY (unless
         * we're "forcing" a WAY, which the auth logic might be doing
         * in some key expiration case).  And clearly if the call is
         * not authenticated, we just do the plain WAY.
         */

        if (key_info != NULL && (force_way_auth || ! scte->high_seq_is_way_validated))
        {
            rpc_dg_auth_epv_p_t auth_epv = scte->auth_epv;

            RPC_DBG_PRINTF(rpc_e_dbg_general, 3, 
                ("(rpc__dg_sct_way_validate) Doing AUTH who-are-you callback\n"));

            (*auth_epv->way) 
                (key_info, h, &scte->actid, rpc_g_dg_server_boot_time, &seq, 
                 &cas_uuid, st);
        }
        else
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 3, 
                ("(rpc__dg_sct_way_validate) Doing who-are-you callback\n"));

            (*conv_v3_0_c_epv.conv_who_are_you)
                (h, &scte->actid, rpc_g_dg_server_boot_time, &seq, st);
        }
    } 
    CATCH (pthread_cancel_e)
    {
        RPC_DBG_GPRINTF(("(rpc__dg_sct_way_validate) cancel exception while performing callback\n"));
        *st = rpc_s_call_cancelled;
    } 
    CATCH_ALL 
    {
        RPC_DBG_GPRINTF(("(rpc__dg_sct_way_validate) exception while performing callback\n"));
        *st = rpc_s_who_are_you_failed;
    } 
    ENDTRY

    rpc_binding_free((rpc_binding_handle_t *) &h, &xst);

    /*
     * Reaquire the connection's lock and release our temporary reference.
     */
    RPC_LOCK(0);
    RPC_DG_SCT_RELEASE(&scte_ref);

    if (*st != rpc_s_ok)
    {
        RPC_DBG_GPRINTF
            (("(rpc__dg_sct_way_validate) who_are_you failed, st=0x%x\n", *st));
        return;
    }

    /*
     * The WAY succeded; update the connection's WAY validated sequence info.
     *
     * BE CAREFUL...
     * Since we've had things unlocked, it is possible the the client
     * has made a *new* call to us since we actually performed the WAY.
     * If this has happened our newly determined seq will be less that
     * what's in the scte; DON'T SET IT BACKWARDS - it is still correct
     * to set it as way_validated.
     */

    if (! RPC_DG_SEQ_IS_LT(seq, scte->high_seq))
    {
        scte->high_seq = seq;
    }

    scte->high_seq_is_way_validated = true;

    if (scte->scall == NULL)
    {
        RPC_DBG_GPRINTF(("(rpc__dg_sct_way_validate) SCTE's SCALL was NULL\n"));
    }
    else
    {
        RPC_DG_CALL_LOCK(&scte->scall->c);
        scte->scall->client_needs_sboot = false;
        RPC_DG_CALL_UNLOCK(&scte->scall->c);
    }
}


/*
 * R P C _ _ D G _ S C T _ F O R K _ H A N D L E R 
 *
 * Handle fork related processing for this module.
 */

PRIVATE void rpc__dg_sct_fork_handler
#ifdef _DCE_PROTO_
(
    rpc_fork_stage_id_t stage
)
#else
(stage)
rpc_fork_stage_id_t stage;
#endif
{                           
    unsigned32 i;

    switch ((int)stage)
    {
        case RPC_C_PREFORK:
            break;
        case RPC_C_POSTFORK_PARENT:
            break;
        case RPC_C_POSTFORK_CHILD:  
            /*
             * Clear out the Server Connection Table 
             */ 
            num_sct_entries = 0;        
            for (i = 0; i < RPC_DG_SCT_SIZE; i++)    
                rpc_g_dg_sct[i] = NULL; 
            break;
    }
}
