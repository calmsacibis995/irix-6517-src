/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgcct.c,v 65.6 1998/04/21 19:39:08 lmc Exp $";
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
 * $Log: dgcct.c,v $
 * Revision 65.6  1998/04/21 19:39:08  lmc
 * Change a cast to unsigned so we don't get sign extension.  This was causing
 * a panic because the result is used as an index to an array.  When it was
 * sign extended it was much too large.
 *
 * Revision 65.5  1998/04/01  14:16:58  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	In the definition of the macro AUTH_INFO_HASH(), type cast the
 * 	arguement as a long instead of as unsigned32.
 *
 * Revision 65.4  1998/03/23  17:28:13  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:05  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:06  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:55  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.608.2  1996/02/18  00:03:28  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:02  marty]
 *
 * Revision 1.1.608.1  1995/12/08  00:19:32  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:32 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/3  1995/05/25  21:40 UTC  lmm
 * 	Merge allocation failure detection cleanup work
 * 
 * 	HP revision /main/HPDCE02/lmm_alloc_fail_clnup/1  1995/05/25  21:01 UTC  lmm
 * 	allocation failure detection cleanup
 * 
 * 	HP revision /main/HPDCE02/2  1995/04/02  01:14 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/HPDCE02/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:18 UTC  lmm
 * 	add memory allocation failure detection
 * 
 * 	HP revision /main/HPDCE02/1  1995/03/06  17:19 UTC  tatsu_s
 * 	Submitted the local rpc security bypass.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b1/1  1995/02/22  22:31 UTC  tatsu_s
 * 	Local RPC Security Bypass.
 * 	[1995/12/07  23:58:51  root]
 * 
 * Revision 1.1.606.4  1994/07/15  16:56:18  tatsu_s
 * 	Fix OT11153: Changed svc_c_action_abort to svc_c_action_exit_bad.
 * 	[1994/07/09  02:43:53  tatsu_s]
 * 
 * Revision 1.1.606.3  1994/06/21  21:53:54  hopkins
 * 	Serviceability
 * 	[1994/06/08  21:31:10  hopkins]
 * 
 * Revision 1.1.606.2  1994/05/19  20:11:17  tom
 * 	Bug 6224 - Check return value of uuid_create.
 * 	[1994/05/18  21:22:54  tom]
 * 
 * Revision 1.1.606.1  1994/01/21  22:36:36  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:33  cbrooks]
 * 
 * Revision 1.1.5.3  1993/01/03  23:23:50  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:34  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  20:47:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:20  zeliff]
 * 
 * Revision 1.1.2.5  1992/05/21  13:16:11  mishkin
 * 	Raise debug level on a debug printf.
 * 	[1992/05/20  21:11:50  mishkin]
 * 
 * Revision 1.1.2.4  1992/05/15  17:22:40  mishkin
 * 	Simplify soft ref check in rpc__dg_cct_get().
 * 	[1992/05/15  16:16:31  mishkin]
 * 
 * 	Pass ccall to rpc__dg_cct_get() so we can update the CCT-related fields
 * 	ourselves rather than letting our caller do it (or not do it [bug]
 * 	which is how I came to the conclusion that it needed to be changed).
 * 	[1992/05/14  19:39:46  mishkin]
 * 
 * Revision 1.1.2.3  1992/05/12  16:15:56  mishkin
 * 	Remove "force_new" parameter from rpc__dg_cct_get().
 * 	[1992/05/08  12:39:26  mishkin]
 * 
 * Revision 1.1.2.2  1992/05/01  17:20:39  rsalz
 * 	 20-feb-92 mishkin       Make timeout vars be internal (for VMS).
 * 	 04-dec-91 sommerfeld    make CCT timeout be patchable in global variable.
 * 	 05-feb-92 sommerfeld    Fix stylistic problems.
 * 	 05-feb-92 sommerfeld    don't forget to release reference to
 * 	                         key_info when freeing ccte.
 * 	 15-jan-92 sommerfeld    key reorg.
 * 	[1992/05/01  17:16:18  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:10:13  devrcs
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
**      dgcct.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  Handles client connection table (CCT).
**
**
*/

#include <dg.h>
#include <dgcct.h>
#include <comauth.h>
#include <dce/assert.h>

/* ========================================================================= */

/*
 * The following structure is used to register the CCT monitor function
 * in the timer queue.
 */

INTERNAL rpc_timer_t cct_timer;

typedef struct {
    rpc_clock_t timer_timestamp;
    unsigned32  timer_last_scanned_count;
    unsigned32  timer_last_freed_count;
} cct_stats_t;

INTERNAL cct_stats_t cct_stats;

/* 
 * Keep a count of the number of CCT entries, active or not, so we know when we
 * need to be running the monitor routine.  (We especially don't want to be 
 * running the monitor for servers that would never need it.)
 *
 * This variable is protected by the global lock.
 */

INTERNAL unsigned32 num_cct_entries = 0;
                                 
/*
 * Run the CCT monitor routine every 5 minutes, clean out entries that are over
 * 10 minutes old.
 */

#define CCT_MONITOR_INTERVAL  300
#define CCTE_TIMEOUT_INTERVAL 600
INTERNAL int rpc_g_dg_cct_mon_int = CCT_MONITOR_INTERVAL;
INTERNAL int rpc_g_dg_cct_timeout = CCTE_TIMEOUT_INTERVAL;
    

/*
 * A U T H _ I N F O _ H A S H
 *
 * Hash an auth_info.
 *
 * No lock requirements.
 */

#ifdef SGIMIPS
#define AUTH_INFO_HASH(auth_info) \
    ((unsigned long) (auth_info))
#else
#define AUTH_INFO_HASH(auth_info) \
    ((unsigned32) (auth_info))
#endif

/* ========================================================================= */

INTERNAL void update_ccall_from_ccte _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t /*ccall*/,
        rpc_dg_cct_elt_p_t  /*ccte*/
    ));

INTERNAL rpc_dg_cct_elt_p_t ccte_create _DCE_PROTOTYPE_((
        rpc_auth_info_p_t /*auth_info*/,
        unsigned32  /*probe*/
    ));

INTERNAL void cct_timer_fn _DCE_PROTOTYPE_((
        pointer_t  /*p*/
    ));

INTERNAL void create_activity_uuid _DCE_PROTOTYPE_((
        uuid_t * /*uuid*/
    ));

/* ========================================================================= */

/*
 * U P D A T E _ C C A L L _ F R O M _ C C T E
 *
 * Update the CCTE-related fields in a CCALL from a CCTE.
 */

INTERNAL void update_ccall_from_ccte
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall,
    rpc_dg_cct_elt_p_t ccte
)
#else
(ccall, ccte)
rpc_dg_ccall_p_t ccall;
rpc_dg_cct_elt_p_t ccte;
#endif
{
    ccall->ccte_ref.ccte      = ccte;
    ccall->ccte_ref.gc_count  = rpc_g_dg_cct.gc_count;

    RPC_DG_CCT_REFERENCE(ccte);         /* For reference we're returning */

    ccall->c.actid_hash = ccte->actid_hash;
    ccall->c.auth_epv   = ccte->auth_epv;
    ccall->c.call_actid = ccte->actid;
    ccall->c.call_ahint = RPC_C_DG_NO_HINT;

    /*
     * If there is some key info already, make sure we release our reference
     * to it.  If we're assigning key info into the CCALL, make sure we
     * count our reference to it.
     */

    if (ccall->c.key_info != NULL)
    {
        RPC_DG_KEY_RELEASE(ccall->c.key_info);
    }

    ccall->c.key_info   = ccte->key_info;

    if (ccall->c.key_info != NULL)
    {
        RPC_DG_KEY_REFERENCE(ccall->c.key_info);
    }
}


/*
 * C C T E _ C R E A T E
 *
 * Allocate, thread, and initialize a new CCTE.
 */

INTERNAL rpc_dg_cct_elt_p_t ccte_create
#ifdef _DCE_PROTO_
(
    rpc_auth_info_p_t auth_info,
    unsigned32 probe
)
#else
(auth_info, probe)
rpc_auth_info_p_t auth_info;
unsigned32 probe;
#endif
{
    rpc_dg_cct_elt_p_t ccte;

    RPC_MEM_ALLOC(ccte, rpc_dg_cct_elt_p_t, sizeof *ccte, 
            RPC_C_MEM_DG_CCTE, RPC_C_MEM_NOWAIT);
    if (ccte == NULL)
	return (NULL);

    if (auth_info != NULL)
    {
        rpc_dg_auth_epv_p_t epv =
            (rpc_dg_auth_epv_p_t) rpc__auth_rpc_prot_epv
                (auth_info->authn_protocol, RPC_C_PROTOCOL_ID_NCADG);

        ccte->auth_epv  = epv;
        ccte->key_info  = (*epv->new_key) (auth_info);
	if (ccte->key_info == NULL){
	    RPC_MEM_FREE(ccte, RPC_C_MEM_DG_CCTE);
	    return(NULL);
	}
        ccte->auth_info = auth_info;

        RPC_DG_AUTH_REFERENCE(auth_info);
    }
    else
    {
        ccte->auth_info = NULL;
        ccte->key_info  = NULL;
        ccte->auth_epv  = NULL;
    }

    create_activity_uuid(&ccte->actid);

    ccte->actid_hash    = rpc__dg_uuid_hash(&ccte->actid);
    ccte->seq           = 0;
    ccte->timestamp     = rpc__clock_stamp();
    ccte->refcnt        = 0;
    ccte->next          = NULL;

    /*
     * Maintain an oldest-first list ordering.
     */

    if (rpc_g_dg_cct.t[probe].first == NULL)
        rpc_g_dg_cct.t[probe].first = ccte;
    else
        rpc_g_dg_cct.t[probe].last->next = ccte;

    rpc_g_dg_cct.t[probe].last = ccte;

    RPC_DG_CCT_REFERENCE(ccte);                 /* For reference from CCT */

    /*
     * Increment the count of CCTE's.  If this is the first/only one,
     * add the CCT monitor to the timer queue to handle aging CCTE's.
     */

    num_cct_entries++;
    if (num_cct_entries == 1)
    {                           
        rpc__timer_set(&cct_timer, cct_timer_fn,
           (pointer_t) NULL, RPC_CLOCK_SEC(rpc_g_dg_cct_mon_int));
    }

    return (ccte);
}


/*
 * R P C _ _ D G _ C C T _ G E T
 *
 * Lookup and reserve an available CCTE matching the specified auth-info
 * in the CCT.  If an non-inuse entry isn't found, create one.  ("inuse"
 * means the CCTE's reference count is > 1, since the CCT's reference
 * itself counts as a reference.)  Return a soft reference to the ccte
 * (in ccte_ref).  If a soft ref was provided (ccte_ref->ccte != NULL),
 * attempt to use it.
 *
 * Return an indication whether or not an existing CCTE ref was reused.
 *
 * We want to reuse a single actid as much as we can, (a) to minimize
 * the creation (and to a larger extent management) of actids (the CCT),
 * and (b) to minimize the need for servers to call back clients to learn
 * the activities sequence number or session key.
 * 
 * Unfortunately, this desire to maximally reuse actids conflicts with
 * the ability of a client to minimize the cost of acquiring a call handle
 * necessary to perform a RPC.  The bottom line is: "how much processing
 * are we willing to do on a start_call() to acquire the call handle.
 * I.e, where is the performance tradeoff between the client doing extra
 * work and the server having to revalidate a client conversation?"
 * Whether or not authenticated RPC is being used does NOT affect this
 * decision; a callback for either is approximately the same cost.
 * 
 * For maximum reuseability, start_call() must scan the CCT for a CCTE
 * that is not "inuse" (inuse refering to whether or not a RPC is currently
 * being performed using that CCTE's activity) and has a matching
 * auth-info.  While it is arguably beneficial to remember what CCTEs
 * had previously been used to communicate with what servers we believe
 * that we can minimize the number of actids generated for a particular
 * authinfo.  Therefore we'll either end up reusing the same actid for
 * a server (because we only have one) or there will be so few of them
 * that the server will eventually learn them all.  Note that the ahint
 * and ihint aren't part of the CCTE.  We avoid having to relearn the
 * ahint and ihint for a particular call by caching these hints in the
 * call handle (which is bound to a particular binding handle, hence
 * server destination).
 */

PRIVATE void rpc__dg_cct_get
#ifdef _DCE_PROTO_
(
    rpc_auth_info_p_t auth_info,
    rpc_dg_ccall_p_t ccall
)
#else
(auth_info, ccall)
rpc_auth_info_p_t auth_info;
rpc_dg_ccall_p_t ccall;
#endif
{
    unsigned32 probe;
    rpc_dg_cct_elt_p_t ccte;

    RPC_LOCK_ASSERT(0);

    ccte = ccall->ccte_ref.ccte;

    /*
     * If we have a soft reference, and the CCT hasn't been gc'd, and the
     * CCTE we're referencing isn't being used (i.e., refcnt == 1), then
     * just grab that CCTE.
     */

    if (ccte != NULL && 
        ccall->ccte_ref.gc_count == rpc_g_dg_cct.gc_count &&
        ccte->refcnt == 1) 
    {
        RPC_DG_CCT_REFERENCE(ccte);         /* For reference we're returning */
        return;
    }
    
    /*
     * Scan down the hash chain.  If we don't find a match on a CCTE
     * that's unused (i.e., refcnt == 1), make a new one.
     */
    
    probe = AUTH_INFO_HASH(auth_info) % RPC_DG_CCT_SIZE;
    ccte = rpc_g_dg_cct.t[probe].first;
    
    while (ccte != NULL) 
    {
        if (ccte->refcnt == 1 && auth_info == ccte->auth_info) 
        {
            break;
        }
        ccte = ccte->next;
    }

    if (ccte == NULL) 
    {
        ccte = ccte_create(auth_info, probe);
	if (ccte == NULL){
	    ccall->ccte_ref.ccte = NULL;
	    return;
	}
    }

    update_ccall_from_ccte(ccall, ccte);
}


/*
 * C C T _ T I M E R 
 *
 * This routine searches through the CCT for entries that have not been in use
 * for a certain period of time.  It is run peridically from the timer queue.
 */

INTERNAL void cct_timer_fn
#ifdef _DCE_PROTO_
(
    pointer_t junk
)
#else
(junk)
pointer_t junk;
#endif
{
    rpc_dg_cct_elt_p_t ccte, prev_ccte, next_ccte;
    unsigned32 i;
    int cct_timeout = rpc_g_dg_cct_timeout;

    assert(num_cct_entries != 0);

    /*
     * The CCT is protected by the global lock.
     */

    RPC_LOCK(0);

    cct_stats.timer_timestamp = rpc__clock_stamp();
    cct_stats.timer_last_freed_count = 0;
    cct_stats.timer_last_scanned_count = 0;

    /*
     * Look in each bucket of hash table...
     */
                         
    for (i = 0; i < RPC_DG_CCT_SIZE; i++)
    {
        ccte = rpc_g_dg_cct.t[i].first;
        prev_ccte = NULL;

        /*
         * Scan each chain for ccte's that have aged.
         */

        while (ccte != NULL)
        {
            cct_stats.timer_last_scanned_count++;
            if (ccte->refcnt == 1 &&
                rpc__clock_aged(ccte->timestamp, 
                                RPC_CLOCK_SEC(cct_timeout)))
            { 
                cct_stats.timer_last_freed_count++;

                next_ccte = ccte->next;

                if (prev_ccte == NULL)
                    rpc_g_dg_cct.t[i].first = next_ccte;
                else
                    prev_ccte->next = next_ccte;
                if (rpc_g_dg_cct.t[i].last == ccte)
                    rpc_g_dg_cct.t[i].last = prev_ccte;

                if (ccte->auth_info)
                {
                    RPC_DG_AUTH_RELEASE(ccte->auth_info);
                    CLOBBER_PTR(ccte->auth_info);
                }
                if (ccte->key_info)
                {
                    RPC_DG_KEY_RELEASE(ccte->key_info);
                    CLOBBER_PTR(ccte->key_info);
                }
                CLOBBER_PTR(ccte->auth_epv);
                RPC_MEM_FREE(ccte, RPC_C_MEM_DG_CCTE);
                num_cct_entries--;
                ccte = next_ccte;

                /*
                 * Bump the CCT garbage-collection count.
                 */
                
                rpc_g_dg_cct.gc_count++;
            }                
            else
            {
                prev_ccte = ccte;
                ccte = ccte->next;
            }
        }
    }

    RPC_DBG_PRINTF(rpc_e_dbg_general, 5, 
        ("(cct_timer) Scanned %d Freed %d\n", 
            cct_stats.timer_last_scanned_count,
            cct_stats.timer_last_freed_count));

    /*
     * If there are no more CCTE's in the CCT, then remove the monitor
     * routine from the timer queue.
     */

    if (num_cct_entries == 0)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_general, 3,
            ("(cct_timer) removing CCT monitor from timer queue\n"));
            rpc__timer_clear(&cct_timer);
    }                         

    RPC_UNLOCK(0);
}


/*
 * C R E A T E _ A C T I V I T Y _ U U I D
 *
 * Create a new UUID to be used as an activity UUID.  This routine does
 * not simply call "uuid_create" to avoid a bug in old systems.
 * 
 * Old (pre-2.0) UUIDs have the same field structure as current UUIDs,
 * but the semantics of the fields are different.  By careful construction,
 * no old and new UUIDs have the same bit values.  The fact that the
 * field structure is the same means that new UUIDs can be (un)marshalled
 * by old code and current UUIDs can be (un)marshalled by new code.  (See
 * comments in "uuid.c" for details.)
 * 
 * Unfortunately, pre-2.0 RPC does not correctly unmarshall the UUIDs
 * that appear in the RPC DG protocol packet headers.  (The unmarshalling
 * code is "hand written", not generated by the NIDL compiler.)  In
 * particular, the 16 bit "reserved" field in old UUIDs never gets byte
 * swapped (i.e., when the integer dreps of sender and receiver differ).
 * This is not a problem when only pre-2.0 systems talk since in this
 * case the "reserved" field is always zero.  Since current UUIDs have
 * non-zero values in the field that's parallel to the old "reserved"
 * field ("time_hi_and_version"), problems can arise when pre-2.0 and
 * 2.0 systems talk unless we generate activity IDs "carefully".
 * 
 * (Failure to be "careful" leads to the following scenario: A big-endian
 * 2.0 client calls a little-endian pre-2.0 server.  The server calls
 * "conv_who_are_you" and passes the original request's activity ID (which
 * is now corrupt, as the server did not byte swap the "reserved" field)
 * as input.  The 2.0 client (now server to the callback) unmarshalls
 * the activity ID from the callback request, tries to look it up in
 * its local database of activities, and the lookup fails because the
 * database contains the ID in the correct format, which doesn't match
 * what the 1.5 system passed over.  The call fails.  Interoperability
 * between 2.0 clients and 1.5 servers of different integer endianness
 * doesn't exist.)
 * 
 * What we do to avoid the problem is to generate a UUID and then carefully
 * fry some of the bits in the "time_hi_and_version" field.  (This is
 * the field that's not getting byte swapped by pre-2.0 systems.)  The
 * thing to note is that we're OK as long as the contents of this field
 * are always symmetric; i.e., that the high and low bytes have the same
 * value.  The contents of this field are as follows:  the high 4 bits
 * are the version, and the low 12 bits are the most significant bits
 * of the 60 timestamp.  Here's what we do.  The field looks like:
 * 
 * 
 *     |<--4-->|<--4-->|<--4-->|<--4-->|
 *     +-------+-------+-------+-------+
 *     | ver # |<59:56>|<55:52>|<51:48>|
 *     +-------+-------+-------+-------+
 * 
 * where <x:y> represents pieces of the current UUID timestamp.  Each
 * box above is 4 bits wide.  We transform the above to:
 * 
 *     +-------+-------+-------+-------+
 *     | ver # |<51:48>| ver # |<51:48>|
 *     +-------+-------+-------+-------+
 * 
 * Note that we have discarded the highest 8 bits of the time.  Is this
 * a problem?  We think not.  Consider:  we now have a 52 bit clock instead
 * of a 60 bit clock.  This means the clock will wrap every 2^52 clock
 * ticks.  A clock tick is 100 ns.  This works out to a wrap about every
 * 20 years.  Activity IDs don't persist for anything even close to this
 * amount of time.
 *
 * Note finally that pre-2.0 systems also mess up the interface and object
 * IDs that appear in the DG packet header.  However, no pre-2.0
 * application can possibly call or execute an interface with a new UUID
 * because no such UUIDs could have existed when the pre-2.0 application
 * was created.  Thus, if a 2.0 application calls a pre-2.0 server using
 * a new interface UUID, the fact that the UUIDs is screwed up can't
 * matter -- the pre-2.0 server doesn't know about the interface anyway.
 * 
 * Object IDs are slightly more problematic.  If some pre-2.0 server
 * inquires the object UUID from a (server-side) binding handle, stores
 * it away, and then, say, returns it as an output parameters from some
 * other call, then the caller of that second call will get a screwed
 * up UUID.  We hope that no one is really doing stuff like this.
 */

INTERNAL void create_activity_uuid
#ifdef _DCE_PROTO_
(
    uuid_t *uuid
)
#else
(uuid)
uuid_t *uuid;
#endif
{
    unsigned32 st;
    unsigned16 tmp;

    uuid_create(uuid, &st);
    if (st != rpc_s_ok)
    {
	/*
	 * rpc_m_cant_create_uuid
	 * "(%s) Can't create UUID"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_exit_bad,
	    rpc_m_cant_create_uuid,
	    "create_activity_uuid" ));
    }

    tmp = uuid->time_hi_and_version;

    uuid->time_hi_and_version = 
        (tmp        & 0xf000) |     /* Preserve version # */
        ((tmp << 8) & 0x0f00) |     /* Dup <51:48> of time into high byte */
        ((tmp >> 8) & 0x00f0) |     /* Dup version # down to low byte */
        (tmp        & 0x000f);      /* Preserve <51:48> of time */
}

/*
 * R P C _ _ D G _ C C T _ F O R K _ H A N D L E R 
 *
 * Handle fork related processing for this module.
 */
PRIVATE void rpc__dg_cct_fork_handler
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
             * The Client Connection Table.  
             */ 
            num_cct_entries = 0;        
            rpc_g_dg_cct.gc_count    = 0;
            for (i = 0; i < RPC_DG_CCT_SIZE; i++)
            {
                rpc_g_dg_cct.t[i].first = NULL;
                rpc_g_dg_cct.t[i].last = NULL;
            }
            break;
    }
}
