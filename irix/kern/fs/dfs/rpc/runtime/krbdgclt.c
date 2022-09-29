/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krbdgclt.c,v 65.5 1998/03/23 17:30:10 gwehrman Exp $";
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
 * $Log: krbdgclt.c,v $
 * Revision 65.5  1998/03/23 17:30:10  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/03/21 19:15:08  lmc
 * Changed the use of "#ifdef DEBUG" to "defined(SGIMIPS) &&
 * defined(SGI_RPC_DEBUG)" to turn on debugging in rpc.
 * This allows debug rpc messages to be turned on without
 * getting the kernel debugging that comes with "DEBUG".
 * "DEBUG" can change the size of some kernel data structures.
 *
 * Revision 65.3  1998/01/07  17:21:34  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:36  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:02  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.621.3  1996/08/09  12:03:07  arvind
 * 	Merge code review cleanup from mb_u2u2
 * 	[1996/07/03  19:29 UTC  burati  /main/DCE_1.2.2/mb_u2u2/1]
 *
 * 	u2u cleanup (rtn for checking u2u status code)
 * 	[1996/06/14  21:23 UTC  burati  /main/DCE_1.2.2/2]
 *
 * 	Use KRB5KDC_ERR_MUST_USE_USER2USER instead of KRB5KDC_ERR_POLICY
 * 	[1996/04/29  20:59 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 *
 * 	merge u2u  work
 * 	[1996/04/29  20:25 UTC  burati  /main/HPDCE02/mb_mothra8/1]
 *
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	Fetch STGT if necessary.
 * 	[1996/01/03  19:02 UTC  psn  /main/DCE_1.2/1]
 *
 * Revision 1.1.621.2  1996/07/08  18:24:01  arvind
 * 	Use KRB5KDC_ERR_MUST_USE_USER2USER instead of KRB5KDC_ERR_POLICY
 * 	[1996/04/29  20:59 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 * 
 * 	merge u2u  work
 * 	[1996/04/29  20:25 UTC  burati  /main/HPDCE02/mb_mothra8/1]
 * 
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	Fetch STGT if necessary.
 * 	[1996/01/03  19:02 UTC  psn  /main/DCE_1.2/1]
 * 
 * Revision 1.1.621.1  1996/06/04  21:55:31  arvind
 * 	u2u:  u2u: Merge fetchof svr TGT on refresh, if necessary, in
 * 	rpc__krb_dg_pre_call()
 * 	[1996/05/06  20:24 UTC  burati  /main/DCE_1.2/2]
 * 
 * 	merge u2u  work
 * 	[1996/04/29  20:59 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 * 
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	Fetch STGT if necessary.
 * 	[1996/04/29  20:25 UTC  burati  /main/HPDCE02/mb_mothra8/1]
 * 
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:33 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 * 
 * 	Merge allocation failure detection cleanup work
 * 	[1995/05/25  21:41 UTC  lmm  /main/HPDCE02/3]
 * 
 * 	allocation failure detection cleanup
 * 	[1995/05/25  21:02 UTC  lmm  /main/HPDCE02/lmm_alloc_fail_clnup/1]
 * 
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:15 UTC  lmm  /main/HPDCE02/2]
 * 
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:19 UTC  lmm  /main/HPDCE02/lmm_rpc_alloc_fail_detect/1]
 * 
 * 	Submitted the local rpc security bypass.
 * 	[1995/03/06  17:20 UTC  tatsu_s  /main/HPDCE02/1]
 * 
 * 	Changed the location of the requested bits field.
 * 	[1995/03/03  15:02 UTC  tatsu_s  /main/tatsu_s.local_rpc.b1/3]
 * 
 * 	Added the trusted parameter to rpc__krb_dg_way_handler().
 * 	[1995/02/27  22:04 UTC  tatsu_s  /main/tatsu_s.local_rpc.b1/2]
 * 
 * 	Local RPC Security Bypass.
 * 	[1995/02/22  22:32 UTC  tatsu_s  /main/tatsu_s.local_rpc.b1/1]
 * 
 * Revision 1.1.619.2  1996/02/18  00:04:32  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:46  marty]
 * 
 * Revision 1.1.619.1  1995/12/08  00:20:50  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/3  1995/05/25  21:41 UTC  lmm
 * 	Merge allocation failure detection cleanup work
 * 
 * 	HP revision /main/HPDCE02/lmm_alloc_fail_clnup/1  1995/05/25  21:02 UTC  lmm
 * 	allocation failure detection cleanup
 * 
 * 	HP revision /main/HPDCE02/2  1995/04/02  01:15 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/HPDCE02/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:19 UTC  lmm
 * 	add memory allocation failure detection
 * 
 * 	HP revision /main/HPDCE02/1  1995/03/06  17:20 UTC  tatsu_s
 * 	Submitted the local rpc security bypass.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b1/3  1995/03/03  15:02 UTC  tatsu_s
 * 	Changed the location of the requested bits field.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b1/2  1995/02/27  22:04 UTC  tatsu_s
 * 	Added the trusted parameter to rpc__krb_dg_way_handler().
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b1/1  1995/02/22  22:32 UTC  tatsu_s
 * 	Local RPC Security Bypass.
 * 	[1995/12/07  23:59:38  root]
 * 
 * Revision 1.1.617.6  1994/06/02  22:04:44  mdf
 * 	Merged with changes from 1.1.617.5
 * 	[1994/06/02  22:04:19  mdf]
 * 
 * 	Merged with changes from 1.1.617.3
 * 	[1994/06/02  19:08:42  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:13:00  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 
 * Revision 1.1.617.3  1994/05/19  21:14:55  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:42:35  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:22:22  hopkins]
 * 
 * Revision 1.1.617.3  1994/05/19  21:14:55  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:42:35  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:22:22  hopkins]
 * 
 * Revision 1.1.617.5  1994/06/02  20:20:01  mdf
 * 	Merged with changes from 1.1.617.3
 * 	[1994/06/02  19:08:42  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:13:00  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 
 * Revision 1.1.617.3  1994/05/19  21:14:55  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:42:35  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:22:22  hopkins]
 * 
 * Revision 1.1.617.3  1994/05/19  21:14:55  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:42:35  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:22:22  hopkins]
 * 
 * Revision 1.1.617.2  1994/01/28  23:09:35  burati
 * 	rpc_c_mem_dg_epac is now uppercase for code cleanup
 * 	[1994/01/28  04:22:09  burati]
 * 
 * 	Code cleanup changes to new EPAC code
 * 	[1994/01/25  17:06:04  burati]
 * 
 * 	EPAC changes in way_handler (dlg_bl1)
 * 	[1994/01/25  03:38:16  burati]
 * 
 * Revision 1.1.617.1  1994/01/21  22:37:56  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:14  cbrooks]
 * 
 * Revision 1.1.4.8  1993/04/09  19:07:52  sommerfeld
 * 	Reduce stack usage by deleting unused character array from
 * 	rpc__krb_dg_way_handler.
 * 	[1993/04/05  14:14:01  sommerfe]
 * 
 * 	Reduce stack usage by deleting unused character array froom
 * 	rpc__krb_dg_wayw_handler
 * 	[1993/04/05  14:13:18  sommerfe]
 * 
 * Revision 1.1.4.7  1993/01/03  23:26:01  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:08:09  bbelch]
 * 
 * Revision 1.1.4.6  1992/12/23  20:52:01  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:39:54  zeliff]
 * 
 * Revision 1.1.4.5  1992/12/21  21:31:24  sommerfeld
 * 	Add some debug code to induce a delay here in order to provoke
 * 	deadlock condition seen in kernel in user space for testing..
 * 	[1992/12/21  20:33:18  sommerfeld]
 * 
 * 	Remove calls to RPC_UNLOCK and RPC_LOCK; this has been pushed up a level.
 * 	[1992/12/18  23:06:54  sommerfeld]
 * 
 * 	Reinstate 1.1.4.3 fix.
 * 	[1992/12/18  22:58:32  sommerfeld]
 * 
 * Revision 1.1.4.3  1992/12/11  22:51:13  sommerfeld
 * 	[OT6207] bump levels where mutual auth doesn't work up one level.
 * 	[OT6369] flush key mutexes.
 * 	[1992/12/09  21:50:04  sommerfeld]
 * 
 * 	Don't keep global lock locked when processing callback; can lead to deadlock.
 * 	[1992/12/07  22:13:43  sommerfeld]
 * 
 * Revision 1.1.4.2  1992/10/23  02:42:07  sommerfeld
 * 	OT5727: don't lock krb_info when allocating new key.. the lock
 * 	is unnecessary and can cause deadlocks.
 * 	[1992/10/20  20:30:36  sommerfeld]
 * 
 * Revision 1.1.2.3  1992/05/20  21:21:35  sommerfeld
 * 	Fix "level_none" -- don't bother with cred fetching if not "reallY"
 * 	authenticated.
 * 	[1992/05/20  19:02:37  sommerfeld]
 * 
 * Revision 1.1.2.2  1992/05/01  17:22:15  rsalz
 * 	 10-feb-92 sommerfeld    add necessary cast when returning key_info
 * 	 05-feb-92 sommerfeld    reference auth_info when creating key_info.
 * 	 04-feb-92 sommerfeld    fix refresh after lock reorganization.
 * 	 04-feb-92 sommerfeld    locking reorg.
 * 	 22-jan-92 sommerfeld    key reorganizaton
 * 	 07-jan-92 mishkin       - Flush client krb/dg epv.
 * 	                         - Get rid of rpc__krb_dg_get_prot_info().
 * 	                         - Misc. style cleanups.
 * 	[1992/05/01  17:17:38  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:21  devrcs
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
**  NAME
**
**      krbdgclt.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**      Client-side support of kerberos datagram module.
**
**
*/

#include <krbdgp.h>

/*
 * R P C _ _ K R B _ D G _ P R E _ C A L L
 * 
 * locking around here is pessimal with respect to preventing other
 * threads from blocking..
 */

PRIVATE void rpc__krb_dg_pre_call 
(
        rpc_key_info_p_t key,
        handle_t h,
        unsigned32 *stp
)
{
    rpc_auth_info_p_t info = key->auth_info;
    rpc_krb_info_p_t krb_info = (rpc_krb_info_p_t)info;
    rpc_krb_key_p_t krb_key = (rpc_krb_key_p_t)key; 
    unsigned32 st = rpc_s_ok;

    /* !!! this needs revision for locking */
    
#ifdef FORCE_KEY_CHANGE
    static int ncalls = 0;
#endif
    
    if (krb_info->status != rpc_s_ok)
    {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_pre_call) poisoned with status %x\n",
            krb_info->status));
        *stp = krb_info->status;
        return;
    }

    if (krb_info->auth_info.authn_level == rpc_c_authn_level_none)
    {
        *stp = rpc_s_ok;
        return;
    }
    
#ifdef FORCE_KEY_CHANGE
    ncalls++;
    if (ncalls > 10) {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_pre_call) generating key...\n"));
        rpc__krb_gen_key(krb_key);
        ncalls = 0;
    }
#endif

    /* write-lock krb_info */
    RPC_KRB_INFO_LOCK(krb_info);

    if (rpc__clock_unix_expired(krb_key->expiration))
    {
        if (rpc__clock_unix_expired(krb_info->expiration))
        {
            krb_info->creds_valid = 0;
            RPC_DBG_PRINTF (rpc_e_dbg_auth, 2,
                ("(rpc__krb_dg_pre_call) trying to refresh tickets; old exp=%d\n",
                krb_info->expiration
            ));
            st = rpc__krb_get_tkt(krb_info);
            RPC_KRB_INFO_LOCK_ASSERT(krb_info);
            if (st != rpc_s_ok) {
                RPC_DBG_GPRINTF((
                    "(rpc__krb_dg_pre_call) rpc__krb_get_tkt failed: st %x\n",
                    st));
#ifndef PRE_U2U
                /* NEW FOR U2U
		 * If we get a NEVER_VALID error, it means the Svr TGT we
		 * used for U2U has expired according to the KDC time, and
		 * we need to fetch a new one.
		 * 
                 * If we got a policy error and aren't already trying u2u,
                 * it means we must attempt to use the user to user protocol
                 * to get the ticket.
                 */
                if (sec_krb_never_valid(st) ||
		    (sec_krb_must_use_u2u(st) &&
                     (krb_info->tgt_length == 0) &&
                     (krb_info->auth_info.authn_protocol >=
                        rpc_c_authn_dce_secret)) ) {
                    unsigned32 tgt_length, tmp_st;
                    unsigned_char_p_t tgt_data_p, server_princ_name;
		    rpc_binding_handle_t tmp_h;

		    /* Throw out old svr TGT if we have one */
		    if (krb_info->tgt_data != NULL) {
                        RPC_MEM_FREE (krb_info->tgt_data, RPC_C_MEM_STRING);
			krb_info->tgt_data = NULL;
			krb_info->tgt_length = 0;
		    }

		    /* Need copy of handle for unauth retrieval of TGT */
		    rpc_binding_copy(h, &tmp_h, &st);
                    if (st != rpc_s_ok) {
                        RPC_DBG_GPRINTF((
                            "(rpc__krb_dg_pre_call) rpc_binding_copy failed: st %x\n",
                            st));
                        goto out;
                    }
		    rpc_binding_set_auth_info(tmp_h, NULL,
                        rpc_c_protect_level_none, rpc_c_authn_none, NULL,
                        rpc_c_authz_none, &tmp_st);

                    /* Attempt to retrieve the TGT data from the server */
                    rpc_mgmt_inq_svr_princ_name_tgt(tmp_h,
                        krb_info->auth_info.authn_protocol,
                        &server_princ_name, &tgt_length, &tgt_data_p, &st);
                    if (st == rpc_s_ok) {
                        krb_info->tgt_length = tgt_length;
                        krb_info->tgt_data = tgt_data_p;

                        RPC_MEM_FREE (server_princ_name, RPC_C_MEM_STRING);

                        /* Try it again now that we have the 2nd ticket data */
                        st = rpc__krb_get_tkt (krb_info);
                    }
                    rpc_binding_free(&tmp_h, &tmp_st);
                }
                if (st != rpc_s_ok) {
                    goto out;
                }
#endif /* ifndef PRE_U2U */

            }
            RPC_DBG_PRINTF(rpc_e_dbg_auth, 3,
                ("(rpc__krb_dg_pre_call) new exp=%d\n", krb_info->expiration
            ));
        }
        krb_key->expiration = krb_info->expiration;
        RPC_DBG_PRINTF (rpc_e_dbg_auth, 3,
            ("(rpc__krb_dg_pre_call) generating key..\n"));
        rpc__krb_gen_key(krb_key);
    }
out:
    *stp = st;

    /* unlock krb_info */
    RPC_KRB_INFO_UNLOCK(krb_info);

}


/*
 * R P C _ _ K R B _ D G _ W A Y _ H A N D L E R
 * 
 */

PRIVATE void rpc__krb_dg_way_handler 
(
        rpc_key_info_p_t key,
        ndr_byte *in_data,
        signed32 in_len,
        signed32 out_max_len,
        ndr_byte **out_data,
        signed32 *out_len,
        unsigned32 *stp
)
{
    rpc_auth_info_p_t info = key->auth_info;
    rpc_krb_key_p_t krb_key = (rpc_krb_key_p_t) key;
    rpc_krb_info_p_t krb_info = (rpc_krb_info_p_t) info;
    rpc_mp_t ump;
    int ksno;
    sec_des_block nonce;
    sec_krb_message ap_req;
    int st;
    int idx;
    unsigned32 authz_fmt;
        
    *out_len = 0;

    ap_req.length = 0;
    ap_req.data = NULL;
    
    RPC_KRB_INFO_LOCK(krb_info);
    
    if (krb_info->auth_info.authn_level != rpc_c_authn_level_none) {
	while (krb_info->cred_fetching) {
	    RPC_KRB_INFO_COND_WAIT(krb_info);
	}
	if (!krb_info->creds_valid) {
	    st = rpc_s_invalid_credentials;
	    goto out;
	}
    }

    if (krb_info->status != rpc_s_ok)
    {
        st = krb_info->status;
        goto out;
    }
    
    if (krb_info->auth_info.authn_level != rpc_c_authn_level_none) {
        /* set unmarshalling ptr to start of argument block */
        ump = (rpc_mp_t)in_data;

	if (in_len == 12) 
	{
	    authz_fmt = SEC_AUTHZ_FMT_V1_0;
	}
	else if (in_len < 16) 
	{
	    st = rpc_s_authn_challenge_malformed;
            goto out;
	}
        /* unmarshall ksno */
        rpc_convert_be_long_int (ump, ksno);
        rpc_advance_mp(ump, 4);

        /* unmarshall nonce */
        memcpy(&nonce, ump, 8);
        rpc_advance_mp(ump, 8);
	
	if (in_len >= 16) 
	{
	    rpc_convert_be_long_int (ump, authz_fmt);
	    rpc_advance_mp(ump, 4);
	}
	
        if (ksno == -1)
            ksno = krb_key->current_key;
        
        st = rpc__krb_set_key (krb_key, ksno);
        if (st != rpc_s_ok)       /* Unknown key! */
            goto out;

        /* CTX is locked */

        /* find the key.. */
        for (idx=0; idx < RPC__KRB_NKEYS; idx++)
            if (krb_key->keyseqs[idx] == ksno)
                break;
    } else {
        idx = 0;
    }
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
    if (RPC_DBG(rpc_es_dbg_auth, 100))
    {
	struct timespec i;
	i.tv_sec = rpc_g_dbg_switches[rpc_es_dbg_auth] - 100;
	i.tv_nsec = 0;
	RPC_DBG_GPRINTF(("(rpc__krb_dg_way_handler) sleeping for %d sec\n", i.tv_sec));
	pthread_delay_np(&i);
    }
#endif	

    /*
     * !!! this must not make remote calls/be canceled 
     */
    st = sec_krb_dg_build_message (
        krb_info->auth_info.u.auth_identity,
        krb_info->cred,
        &nonce,
        krb_key->c.authn_level,
        krb_info->auth_info.authz_protocol,
        ksno,
        &(krb_key->keys[idx]),
        &(krb_key->ivec[idx]), 
	authz_fmt,
        &ap_req
    );

    if (st == rpc_s_ok)
    {
        if (ap_req.length > out_max_len)
        {
            RPC_MEM_ALLOC(*out_data, ndr_byte *, ap_req.length,
                          RPC_C_MEM_DG_EPAC, RPC_C_MEM_WAITOK);
	    if (*out_data == NULL){
		*out_len = 0;
		*stp = rpc_s_no_memory;
		sec_krb_message_free(&ap_req);
		RPC_KRB_INFO_UNLOCK(krb_info);
		return;
	    }
        }
        memcpy (*out_data, ap_req.data, ap_req.length);
        *out_len = ap_req.length;
    }
    sec_krb_message_free(&ap_req);
out:
    if (st != rpc_s_ok)
    {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_way_handler) sec_krb_build_message returned %x\n", st));
    }
    *stp = st;
    RPC_KRB_INFO_UNLOCK(krb_info);
}

/*
 * R P C _ _ K R B _ D G _ N E W _ K E Y
 */

rpc_key_info_p_t rpc__krb_dg_new_key 
(
        rpc_auth_info_p_t                   info
)
{
    rpc_krb_key_p_t krb_key;
    rpc_krb_info_p_t krb_info = (rpc_krb_info_p_t) info;
    unsigned32 level;
    
    RPC_MEM_ALLOC (krb_key, rpc_krb_key_p_t, sizeof(*krb_key),
                   RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
	    if (krb_key == NULL)
		return (NULL);
    memset (krb_key, '\0', sizeof(*krb_key));
    /*
     * fill in the common key stuff.
     */
    rpc__auth_info_reference (info);
    krb_key->c.auth_info = info; 
    krb_key->c.refcnt = 1;

    level = krb_info->auth_info.authn_level;
    if ((level >= rpc_c_protect_level_connect) &&
	(level <= rpc_c_protect_level_call))
	    level = rpc_c_protect_level_pkt;
    krb_key->c.authn_level = level;
    
    krb_key->c.is_server = krb_info->auth_info.is_server;

    krb_key->key_needed = -1;
    krb_key->sched_key_seq = -1;
    krb_key->expiration = krb_info->expiration;
    
    /* !!! error returns?? */
    (void) rpc__krb_gen_key(krb_key);
    
    return (rpc_key_info_p_t) krb_key;
}
