/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krbclt.c,v 65.5 1998/03/23 17:28:18 gwehrman Exp $";
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
 * $Log: krbclt.c,v $
 * Revision 65.5  1998/03/23 17:28:18  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:21:06  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:07  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/27  18:49:20  jdoak
 * 6.4 + 1.2.2 merge
 *
 * Revision 65.1  1997/10/20  19:17:01  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.919.3  1996/08/09  12:02:38  arvind
 * 	Merge code review cleanup from mb_u2u2
 * 	[1996/07/03  19:29 UTC  burati  /main/DCE_1.2.2/mb_u2u2/1]
 *
 * 	u2u cleanup (rtn for checking u2u status code)
 * 	[1996/06/14  21:23 UTC  burati  /main/DCE_1.2.2/2]
 *
 * 	Use KRB5KDC_ERR_MUST_USE_USER2USER instead of KRB5KDC_ERR_POLICY
 * 	[1996/04/29  20:34 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 *
 * 	merge u2u  work.
 * 	[1996/04/29  20:07 UTC  burati  /main/HPDCE02/mb_mothra8/1]
 *
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/01/03  19:02 UTC  psn  /main/DCE_1.2/1]
 *
 * Revision 1.1.919.2  1996/07/08  18:23:35  arvind
 * 	Use KRB5KDC_ERR_MUST_USE_USER2USER instead of KRB5KDC_ERR_POLICY
 * 	[1996/04/29  20:34 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 * 
 * 	merge u2u  work.
 * 	[1996/04/29  20:07 UTC  burati  /main/HPDCE02/mb_mothra8/1]
 * 
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/01/03  19:02 UTC  psn  /main/DCE_1.2/1]
 * 
 * Revision 1.1.919.1  1996/06/04  21:54:45  arvind
 * 	u2u: Merge code handling u2u case  to rpc__krb_bnd_set_auth()
 * 	[1996/05/06  20:20 UTC  burati  /main/DCE_1.2/2]
 * 
 * 	merge u2u  work.
 * 	[1996/04/29  20:34 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 * 
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/04/29  20:07 UTC  burati  /main/HPDCE02/mb_mothra8/1]
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
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:18 UTC  tatsu_s  /main/HPDCE02/1]
 * 
 * 	Fix OT12572: rpc part of fix.
 * 	The security runtime needs to be fixed.
 * 	[1994/11/30  22:24 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/1]
 * 
 * Revision 1.1.915.2  1996/02/18  00:04:20  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:39  marty]
 * 
 * Revision 1.1.915.1  1995/12/08  00:20:38  root
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
 * 	HP revision /main/HPDCE02/1  1994/12/09  19:18 UTC  tatsu_s
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 
 * 	HP revision /main/tatsu_s.st_dg.b0/1  1994/11/30  22:24 UTC  tatsu_s
 * 	Fix OT12572: rpc part of fix.
 * 	The security runtime needs to be fixed.
 * 	[1995/12/07  23:59:30  root]
 * 
 * Revision 1.1.913.2  1994/08/15  19:04:53  ganni
 * 	protection level need to be upgraded to the next higher
 * 	supported level, if the given level is not supported.
 * 	[1994/08/15  19:01:52  ganni]
 * 
 * Revision 1.1.913.1  1994/01/21  22:37:46  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:05  cbrooks]
 * 
 * Revision 1.1.8.2  1993/06/24  20:17:16  hinman
 * 	[hinman] - Save merged-in SNI version (these files have been freed of nasty code)
 * 	[1993/06/17  14:23:44  hinman]
 * 
 * Revision 1.1.6.2  1993/06/02  15:22:57  jaffe
 * 	ot 7929: remove auth_tkt_expired failure from client side rpc
 * 	[1993/06/02  13:53:27  jaffe]
 * 
 * Revision 1.1.4.3  1993/01/03  23:25:47  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:07:46  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:51:39  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:39:30  zeliff]
 * 
 * Revision 1.1.2.3  1992/07/02  20:02:13  sommerfeld
 * 	[CR4593] broadcast rather than signal so that we catch multiple
 * 	processes stacked up waiting for the creds
 * 	[1992/06/30  22:58:48  sommerfeld]
 * 
 * 	Move mutex, condition variable init/lock higher in file so that
 * 	"poison" still works correctly when presented with bogus parameters.
 * 	[1992/06/29  20:36:31  sommerfeld]
 * 
 * Revision 1.1.2.2  1992/05/01  17:22:04  rsalz
 * 	 06-apr-92 wh            Move check for null principal name back
 * 	                         to rpc_binding_set_auth_info.
 * 	 04-apr-92 ebm           Change rpc__stralloc to rpc_stralloc.
 * 	 04-feb-92 sommerfeld    Fix refresh after lock reorganization.
 * 	 04-feb-92 sommerfeld    lock reorganization.
 * 	 27-jan-92 sommerfeld    key reorganization (redone).
 * 	 07-dec-92 mishkin       - Correct reference to refcount auth info field
 * 	                           in rpc__krb_bnd_set_auth().
 * 	                         - Misc style cleanup.
 * 	[1992/05/01  17:17:29  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:26  devrcs
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
**      krbclt.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**      Client-side support of kerberos module.
**
**
*/

#include <krbp.h>

/*
 * Size of buffer used when asking for remote server's principal name
 */
#define MAX_SERVER_PRINC_NAME_LEN 500

extern rpc_clock_unix_t rpc_g_clock_unix_curr;

#ifdef SNI_PRIVATE
#endif /* SNI_PRIVATE */

/*
 * R P C _ _ K R B _ G E T _ T K T
 *
 */

PRIVATE unsigned32 rpc__krb_get_tkt 
(
        rpc_krb_info_p_t krb_info
)
{
    error_status_t st = error_status_ok;
    unsigned32 oldexp, exp;
    
    RPC_KRB_INFO_LOCK_ASSERT(krb_info);
    while (krb_info->cred_fetching) {
        RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, 
            ("(rpc__krb_get_tkt) waiting for someone else\n"));
	TRY
	    RPC_KRB_INFO_COND_WAIT(krb_info);
        CATCH(pthread_cancel_e)
	    st = rpc_s_call_cancelled;
        CATCH_ALL
	    /* 
	     * Any other type of exception is something serious.
	     */
	    /*
	     * rpc_m_unexpected_exc
	     * "(%s) Unexpected exception was raised"
	     */
	    RPC_DCE_SVC_PRINTF ((
		DCE_SVC(RPC__SVC_HANDLE, "%s"),
	        rpc_svc_recv,
	        svc_c_sev_fatal | svc_c_action_abort,
		rpc_m_unexpected_exc,
		"rpc__krb_get_tkt" ));
        ENDTRY
        if (st != rpc_s_ok)
	    return st;
    }
    if (krb_info->creds_valid) {
        RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, 
            ("(rpc__krb_get_tkt) already valid\n"));
    } else{
        krb_info->cred_fetching = 1;
        sec_krb_cred_free(&krb_info->cred);

        oldexp = krb_info->expiration;
        RPC_KRB_INFO_UNLOCK(krb_info);
        
        /* 
         * !!! this is a cancel point here.. 
         * on cancel, must set cred_fetching to 0.
         */
        RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, 
            ("(rpc__krb_get_tkt) fetching credentials\n"));
	/* 
	 * We should probably disable general cancellability here because the
	 * security runtime's state is unknown if it gets cancelled...
	 */
	TRY
        st = sec_krb_get_cred (krb_info->auth_info.u.auth_identity,
            krb_info->server,
            krb_info->auth_info.authn_level,
            krb_info->auth_info.authz_protocol,
            krb_info->tgt_length, krb_info->tgt_data, 
            &krb_info->cred, &exp);

        RPC_KRB_INFO_LOCK (krb_info);
        krb_info->expiration = exp;
        
        if (st != rpc_s_ok) {     
            RPC_DBG_GPRINTF
                (("(rpc__krb_get_tkt) refresh failed (status %x)\n", st));
        } 
        else 
        {
            RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, 
                ("(rpc__krb_get_tkt) fetch succeeded\n"));
            krb_info->creds_valid = 1;
        }
        CATCH(pthread_cancel_e)
	    RPC_KRB_INFO_LOCK (krb_info);
	    st = rpc_s_call_cancelled;
        CATCH_ALL
	    /* 
	     * Any other type of exception is something serious.
	     */
	    /*
	     * rpc_m_unexpected_exc
	     * "(%s) Unexpected exception was raised"
	     */
	    RPC_DCE_SVC_PRINTF ((
		DCE_SVC(RPC__SVC_HANDLE, "%s"),
	        rpc_svc_recv,
	        svc_c_sev_fatal | svc_c_action_abort,
		rpc_m_unexpected_exc,
		"rpc__krb_get_tkt" ));
        ENDTRY
        krb_info->cred_fetching = 0;
        RPC_KRB_INFO_COND_BROADCAST(krb_info);
    }
    RPC_KRB_INFO_LOCK_ASSERT(krb_info);
    return st;
}
 

/*
 * R P C _ _ K R B _ B N D _ S E T _ A U T H
 *
 */

PRIVATE void rpc__krb_bnd_set_auth 
(
        unsigned_char_p_t server_name,
        rpc_authn_level_t level,
        rpc_auth_identity_handle_t auth_ident,
        rpc_authz_protocol_id_t authz_prot,
        rpc_binding_handle_t binding_h,
        rpc_auth_info_p_t *infop,
        unsigned32 *stp
)
{
    int st;
    rpc_krb_info_p_t krb_info;
    rpc_binding_rep_p_t	binding_rep = (rpc_binding_rep_p_t) binding_h;

    RPC_MEM_ALLOC (krb_info, rpc_krb_info_p_t, sizeof(*krb_info),
                   RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
    if (krb_info == NULL){
	*stp = rpc_s_no_memory;
	return;
    }

    memset (krb_info, 0, sizeof(*krb_info));
    
    RPC_MUTEX_INIT(krb_info->lock);
    RPC_COND_INIT(krb_info->cond, krb_info->lock);
    
    RPC_KRB_INFO_LOCK(krb_info);
    
    if ((authz_prot != rpc_c_authz_name) &&
        (authz_prot != rpc_c_authz_dce))
    {
        RPC_DBG_GPRINTF(("(rpc__krb_bnd_set_auth) authz %d unsupported\n", authz_prot)); 
        st = rpc_s_authn_authz_mismatch;
        goto poison;
    }

    if ((level < rpc_c_authn_level_none) ||
#ifdef NOENCRYPTION
        (level > rpc_c_authn_level_pkt_integrity)
#else
        (level > rpc_c_authn_level_pkt_privacy)
#endif
        )
    {
        RPC_DBG_GPRINTF(("(rpc__krb_bnd_set_auth) level %d unsupported\n", level));
        st = rpc_s_unsupported_authn_level;
        goto poison;
    }

    switch(binding_rep->protocol_id)
        {
        case RPC_C_PROTOCOL_ID_NCACN:
		if (level == rpc_c_authn_level_call)
		    level = rpc_c_authn_level_pkt;
                break;

        case RPC_C_PROTOCOL_ID_NCADG:
		if ((level >= rpc_c_protect_level_connect) &&
		    (level <= rpc_c_protect_level_call))
		    level = rpc_c_protect_level_pkt;
                break;

        default:
                break;
        }

    krb_info->auth_info.server_princ_name = rpc_stralloc(server_name);
    if ((krb_info->auth_info.server_princ_name == NULL) && server_name != NULL){
	st = rpc_s_no_memory;
	goto poison;
    }
    krb_info->auth_info.authn_level = level;
    krb_info->auth_info.authn_protocol = rpc_c_authn_dce_private;
    krb_info->auth_info.authz_protocol = authz_prot;
    krb_info->auth_info.is_server = 0;
    krb_info->auth_info.u.auth_identity = auth_ident;
    krb_info->auth_info.refcount = 1;

    krb_info->level_valid = 1;
    krb_info->client_valid = 1;      /* sort of.. */

    st = sec_krb_get_cc(auth_ident, 
                        (sec_krb_ccache *) &krb_info->auth_info.u.auth_identity);
    if (st != rpc_s_ok) {
        RPC_DBG_GPRINTF(("(rpc__krb_bnd_set_auth) get_cc of %x failed: %x\n",
            auth_ident, st));
        goto poison;
    }
    

    st = sec_krb_sec_parse_name(auth_ident, level,
        server_name, &krb_info->server);
    if (st != rpc_s_ok) {
        RPC_DBG_GPRINTF(("(rpc__krb_bnd_set_auth) parse_name of %s failed: %x\n",
            server_name, st));
        goto poison;
    }

    if (level != rpc_c_authn_level_none)
    {
        st = rpc__krb_get_tkt (krb_info);

	/* NEW FOR U2U
	 * If we got a policy error and aren't already trying u2u, it means
	 * we must try to use the user to user protocol to get the ticket.
	 */
        if (sec_krb_must_use_u2u(st) &&
            (krb_info->auth_info.authn_protocol >= rpc_c_authn_dce_secret)) {
            unsigned32 tgt_length, tmp_st;
            unsigned_char_p_t tgt_data_p, server_princ_name;

            /* Attempt to retrieve the TGT data from the server */
            rpc_mgmt_inq_svr_princ_name_tgt(binding_h,
                krb_info->auth_info.authn_protocol,
                &server_princ_name, &tgt_length, &tgt_data_p, &tmp_st);

            if (tmp_st != rpc_s_ok)
                goto poison;

	    krb_info->tgt_length = tgt_length;
	    krb_info->tgt_data = tgt_data_p;

            RPC_MEM_FREE (server_princ_name, RPC_C_MEM_STRING);

	    /* Try it again now that we have the 2nd ticket data */
            st = rpc__krb_get_tkt (krb_info);

        }
        if (st != rpc_s_ok)
            goto poison;
    }
    st = rpc_s_ok;
poison:
    /* changed for OT 13114 - added free krb_info */
    if (st != rpc_s_ok) {
	 RPC_KRB_INFO_UNLOCK(krb_info);
	 rpc__krb_free_info((rpc_auth_info_p_t *) &krb_info);
	 *infop = NULL;
    }
    else {
	 *infop = (rpc_auth_info_p_t) &krb_info->auth_info;
	 RPC_KRB_INFO_UNLOCK(krb_info);
	 krb_info->status = st;
    }
    *stp = st;
    return;
}    


/*
 * R P C _ _ K R B _ G E N _ K E Y
 *
 * Generate new session key for use in the given connection.
 */

PRIVATE error_status_t rpc__krb_gen_key 
(
        rpc_krb_key_p_t krb_key
)
{
    sec_des_key random_key;
    sec_des_block ivec;
    error_status_t st;
    
    st = sec_des_new_random_key (&random_key);
    if (st == rpc_s_ok) {
        st = sec_des_generate_random_block (&ivec);
    }
    if (st == rpc_s_ok) {
        krb_key->current_key = ((krb_key->current_key+1) & 0xff);
        rpc__krb_add_key (krb_key, krb_key->current_key, &random_key, &ivec);
    }
    return st;
}
