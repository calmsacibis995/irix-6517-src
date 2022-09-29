/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krbdgsrv.c,v 65.5 1998/03/23 17:29:56 gwehrman Exp $";
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
 * $Log: krbdgsrv.c,v $
 * Revision 65.5  1998/03/23 17:29:56  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:21:31  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:33  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:03  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.925.2  1996/02/18  00:04:36  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:48  marty]
 *
 * Revision 1.1.925.1  1995/12/08  00:20:54  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:34 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/5  1995/08/29  18:59 UTC  tatsu_s
 * 	Submitted the fix for CHFts15938/OT10205.
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.psock_timeout.b0/1  1995/08/21  13:16 UTC  tatsu_s
 * 	Fixed a key version number wrapping.
 * 
 * 	HP revision /main/HPDCE02/4  1995/07/21  19:42 UTC  burati
 * 	Merge uninitialized pointer fix from rps_moth2
 * 
 * 	HP revision /main/HPDCE02/rps_moth2/1  1995/06/23  17:47 UTC  rps
 * 	init cred_head to NULL
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
 * 	HP revision /main/tatsu_s.local_rpc.b1/2  1995/03/03  15:03 UTC  tatsu_s
 * 	Changed the location of the requested bits field.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b1/1  1995/02/22  22:32 UTC  tatsu_s
 * 	Local RPC Security Bypass.
 * 	[1995/12/07  23:59:41  root]
 * 
 * Revision 1.1.920.13  1994/08/18  20:25:06  greg
 * 	Determine authorization format based on our client's protocol
 * 	version and forward it on to the auth message decoding routine.
 * 	[1994/08/16  17:46:09  greg]
 * 
 * Revision 1.1.920.12  1994/08/09  17:32:34  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/09  17:28:38  burati]
 * 
 * Revision 1.1.920.11  1994/07/26  15:48:45  burati
 * 	Big auth info / DFS EPAC changes
 * 	[1994/07/21  18:28:15  burati]
 * 
 * Revision 1.1.920.10  1994/07/06  20:27:02  sommerfeld
 * 	Clean up some comments; make handling of length more consistant.
 * 	[1994/07/06  20:20:01  sommerfeld]
 * 
 * Revision 1.1.920.9  1994/06/30  20:51:20  tom
 * 	Bug 11021 - fix arguments to conv_who_are_you_auth call.
 * 	[1994/06/30  20:51:08  tom]
 * 
 * Revision 1.1.920.8  1994/06/07  18:06:27  tom
 * 	Bug 10876 - Fixed MAX_AUTH_MESSAGE_LEN to fit in a single packet buffer.
 * 	[1994/06/07  18:06:16  tom]
 * 
 * Revision 1.1.920.7  1994/06/06  14:08:12  mdf
 * 	Fix problems that were created on June 3rd submission.
 * 	[1994/06/06  10:26:39  mdf]
 * 
 * Revision 1.1.920.6  1994/06/03  18:33:07  mdf
 * 	fix botched merge
 * 	[1994/06/03  18:05:43  mdf]
 * 
 * Revision 1.1.920.2  1994/01/28  23:09:36  burati
 * 	Apply code cleanup type changes to EPAC changes
 * 	[1994/01/25  16:58:31  burati]
 * 
 * 	EPAC changes - call conv_who_are_you_auth_more (dlg_bl1)
 * 	[1994/01/24  23:57:20  burati]
 * 
 * Revision 1.1.920.1  1994/01/21  22:38:00  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:17  cbrooks]
 * 
 * Revision 1.1.7.1  1993/09/30  14:39:55  sommerfeld
 * 	[OT7680] Deconditionalize prev. fix.
 * 	[1993/09/28  21:28:25  sommerfeld]
 * 
 * Revision 1.1.5.9  1993/04/09  19:07:57  sommerfeld
 * 	Fix missing ; in previous change.
 * 	[1993/04/09  17:37:28  sommerfeld]
 * 
 * 	Conditionalize previous fix based on _KERNEL (well, sort of).
 * 	[1993/04/09  16:09:39  sommerfeld]
 * 
 * 	fix problem with excessive stack frame size.
 * 	[1993/04/05  17:02:49  sommerfe]
 * 
 * 	fix problem with excessive stack frame size.
 * 	[1993/04/05  14:25:09  sommerfe]
 * 
 * Revision 1.1.5.8  1993/01/03  23:26:10  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:08:21  bbelch]
 * 
 * Revision 1.1.5.7  1992/12/23  20:52:15  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:40:07  zeliff]
 * 
 * Revision 1.1.5.6  1992/12/21  21:31:34  sommerfeld
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
 * Revision 1.1.5.5  1992/12/21  20:11:31  bolinger
 * 	Reinstate change made in revision 1.1.5.3.  (Consequences for
 * 	krpc have now been fixed.)
 * 	[1992/12/21  20:08:54  bolinger]
 * 
 * Revision 1.1.5.4  1992/12/17  22:01:23  bolinger
 * 	Back out Bill Sommerfeld revision of 12/11/92, which seems
 * 	to cause krpc hangs on both reference platforms.
 * 	[1992/12/17  21:59:06  bolinger]
 * 
 * Revision 1.1.2.3  1992/05/20  19:22:27  sommerfeld
 * 	Fix deadlock case: on error return from decode, goto "out_unlock"
 * 	instead of "out"
 * 	Clean up error messages.
 * 	[1992/05/20  14:21:30  sommerfeld]
 * 
 * Revision 1.1.2.2  1992/05/01  16:37:36  rsalz
 * 	 10-feb-92 sommerfeld    add necessary cast when storiing auth_info in key_info.
 * 	 04-feb-92 sommerfeld    initialize condition in rpc__krb_dg_create.
 * 	 04-feb-92 sommerfeld    locking reorganization.
 * 	 07-jan-92 mishkin       - Misc style cleanups.
 * 	                         - Flush krb/dg epv.
 * 	                         - Flush auth_prot_info stuff.
 * 	                         - Fix reference to refcount.
 * 	[1992/05/01  16:29:26  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:32  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989, 1994 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      krbdgsrv.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**      Server-side support of kerberos datagram module.
**
**
*/

#include <krbdgp.h>

/*
 * The message must fit in a single packet buffer.
 *   RPC_C_DG_MUST_RECV_FRAG_SIZE
 *     - RPC_C_DG_RAW_PKT_HDR_SIZE
 *     - (other parameters' size)
 */
#define MAX_AUTH_MESSAGE_LEN 1344 /* 1464-80-40 */

typedef struct credentials_ll_elt_t {
    struct credentials_ll_elt_t *next;
    signed32                    buff_len;
    unsigned_char_t             buff[MAX_AUTH_MESSAGE_LEN];
} credentials_ll_elt_t, *credentials_ll_elt_p_t;

#ifdef _KERNEL
  /* Size of the krpc_helper buffer used to pass data out to user space */
extern unsigned long krpc_helper_buffer_size;
  /* Size of the callback info that will be in the above buffer
   * sizeof(status) + sizeof(boot_time) + 4+strlen(string_binding) +
   * sizeof(uuid) + sizeof(challenge) = 4+4+4+36+16+8
   */
#define KRPC_CALLBACK_INFO 72
#endif

/*
 * R P C _ _ K R B _ D G _ W H O _ A R E _ Y O U
 *
 * Issue challenge to client; decompose response and sanity-check it.
 */

PRIVATE void rpc__krb_dg_who_are_you 
#ifdef _DCE_PROTO_
(
        rpc_key_info_p_t key,
        handle_t h,
        uuid_t *actuid,
        unsigned32 boot_time,
        unsigned32 *seq,
        uuid_t *cas_uuid,
        unsigned32 *stp
)
#else
(key, h, actuid, boot_time, seq, cas_uuid, stp)
    rpc_key_info_p_t key;
    handle_t h;
    uuid_t *actuid;
    unsigned32 boot_time;
    unsigned32 *seq;
    uuid_t *cas_uuid;
    unsigned32 *stp;
#endif
{
    rpc_auth_info_p_t info = key->auth_info;
    rpc_krb_key_p_t krb_key = (rpc_krb_key_p_t) key; 
    rpc_krb_info_p_t krb_info = (rpc_krb_info_p_t)info;
    rpc_mp_t mp, omp;
    sec_des_block nonce;
    error_status_t xst, comp_st = 0;
    
    unsigned char inbuf[16];    /* !!! size */
    unsigned char *outbuf_ptr = NULL;
    signed32 outlen, bufsize;
    unsigned32 expiration;
    int st;
    unsigned32 authz_proto;
    unsigned32 ksno, ksno2;
    sec_des_key ses_key;
    sec_des_block ivec;         /* initialization vector. */
    sec_krb_message ap_req;
    unsigned32 authz_fmt;
    
    rpc_authn_level_t level;

    credentials_ll_elt_p_t cred_head, cred_tail, cred_ptr;
   
#ifdef KERNEL
    extern void sec_id_pac_util_free(sec_id_pac_t *);
#endif /* KERNEL */

    ap_req.data = 0;
    ap_req.length = 0;
    
    ksno = krb_key->key_needed;
    krb_key->key_needed = -1;

    cred_head = NULL;
    
    st = sec_des_generate_random_block (&nonce);
    if (st != rpc_s_ok)
        goto out;
    
    /* set up marshalling ptr to buffer  */
    mp = (rpc_mp_t)inbuf;
    
    /* marshall ksno */
    rpc_marshall_be_long_int (mp, ksno);
    rpc_advance_mp(mp, 4);
    
    /* marshall nonce */
    memcpy (mp, &nonce, 8);
    rpc_advance_mp(mp, 8);

    /*
     * save pointer to end of old-style challenge before extending
     * it to form the new-style challenge.
     */
    omp = mp;
    rpc_marshall_be_long_int (mp, SEC_AUTHZ_FMT_V1_1);
    rpc_advance_mp(mp, 4);
    
    RPC_MEM_ALLOC (cred_head, credentials_ll_elt_p_t, sizeof(*cred_head), 
                   RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
    if (cred_head == NULL){
	*stp = rpc_s_no_memory;
	goto out;
    }
     cred_head->next = NULL;
     cred_head->buff_len = 0;
     cred_tail = cred_head;
    
    /* do call */
    TRY
    {
        RPC_DBG_PRINTF(rpc_e_dbg_auth, 4,
            ("(rpc__krb_dg_way) about to call conv_who_are_you_auth\n"));
	/* 
	 * try new style challenge first (ending at "mp"),
	 * then back off to old one if rejected
	 */
        (*conv_v3_0_c_epv.conv_who_are_you_auth)
            (h, actuid, boot_time, inbuf, mp-(rpc_mp_t)inbuf,
                MAX_AUTH_MESSAGE_LEN, seq, cas_uuid, cred_head->buff, 
                &cred_head->buff_len, stp);
	if (*stp != rpc_s_authn_challenge_malformed) 
	{
	    outlen = cred_head->buff_len;

	    authz_fmt = SEC_AUTHZ_FMT_V1_1;

#ifdef _KERNEL
	    /* Get as much auth info as kernel buffer will allow. */
	    bufsize = krpc_helper_buffer_size - KRPC_CALLBACK_INFO;
	    while ((*stp == rpc_s_partial_credentials) &&
		((outlen + MAX_AUTH_MESSAGE_LEN) <= bufsize))
#else
	    while (*stp == rpc_s_partial_credentials)
#endif /* _KERNEL */
	    {
		RPC_MEM_ALLOC (cred_ptr, credentials_ll_elt_p_t, 
			       sizeof(*cred_ptr), RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
		if (cred_ptr == NULL){
		    *stp = rpc_s_no_memory;
		    goto out;
		}
		cred_tail->next = cred_ptr;
		cred_tail = cred_ptr;
		cred_ptr->next = NULL;

		RPC_DBG_PRINTF(rpc_e_dbg_auth, 4,
			       ("(rpc__krb_dg_way) calling conv_who_are_you_auth_more\n"));

		(*conv_v3_0_c_epv.conv_who_are_you_auth_more)
		    (h, actuid, boot_time, outlen, MAX_AUTH_MESSAGE_LEN,
		     cred_ptr->buff, &cred_ptr->buff_len, stp);

		outlen += cred_ptr->buff_len;
	    }

#ifdef _KERNEL

	    /* If we're in the kernel, then auth helper will get rest of
	     * credentials if there's more
	     */
	    comp_st = *stp;
	    if (*stp == rpc_s_partial_credentials) {
		*stp = rpc_s_ok;
	    }

#endif /* _KERNEL */

	} else 
	{
	    authz_fmt = SEC_AUTHZ_FMT_V1_0;

	    /*
	     * Old style challenge, ending at "omp" (it's a prefix of the
	     * new-style challenge
	     */
	    (*conv_v3_0_c_epv.conv_who_are_you_auth)
		(h, actuid, boot_time, inbuf, omp-(rpc_mp_t)inbuf, 
		 MAX_AUTH_MESSAGE_LEN,
		 seq, cas_uuid, cred_head->buff, &cred_head->buff_len, stp);
	    outlen = cred_head->buff_len;
	}

	RPC_DBG_PRINTF(rpc_e_dbg_auth, 4,
		       ("(rpc__krb_dg_way) returned from conv_who_are_you_auth\n"));

    }
    CATCH (pthread_cancel_e)
    {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_who_are_you) cancel exception while performing callback\n"));
        *stp = rpc_s_call_cancelled;
    } 
    CATCH_ALL 
    {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_who_are_you) exception while performing callback\n"));
        *stp = rpc_s_who_are_you_failed;
    } 
    ENDTRY
    
    st = *stp;
    if (st != rpc_s_ok)
    {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_way) conv_who_are_you_auth failed, code %x\n", st));
        goto out;
    }
    
    if (outlen <= MAX_AUTH_MESSAGE_LEN)
    {
        outbuf_ptr = cred_head->buff;
    }
    else
    {
        unsigned_char_p_t tmp;

        RPC_MEM_ALLOC (outbuf_ptr, unsigned_char_p_t, outlen,
                       RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
	if (outbuf_ptr == NULL){
	    *stp = rpc_s_no_memory;
	    goto out;
	}

        for (cred_ptr = cred_head, tmp = outbuf_ptr;
             cred_ptr != NULL; )
        {
            memcpy(tmp, cred_ptr->buff, cred_ptr->buff_len);
            tmp += cred_ptr->buff_len;

            cred_ptr = cred_ptr->next;
            RPC_MEM_FREE(cred_head, RPC_C_MEM_UTIL);
            cred_head = cred_ptr;
        }
    }

    ap_req.data = outbuf_ptr;
    ap_req.length = outlen;

    /*
     * Lock the krb_info, and free up any old strings/pac's held in it
     * (in case of client-induced reauthentication).
     */
    
    RPC_KRB_INFO_LOCK(krb_info);

    /*
     * !!! We should check that client_name, server, client_pac didn't
     * change from before to after, BUT we're just trying to plug a
     * memory leak now, and don't want to change too much.
     * This should be revisited for DCE 1.0.1.
     */
    
    if (krb_info->client_name)
        rpc_string_free(&krb_info->client_name, &xst);
    if (krb_info->server)
        sec_krb_parsed_name_free(&krb_info->server);
    sec_id_pac_util_free(&krb_info->client_pac);

#ifndef _KERNEL

    { /* FAKE-EPAC */
	(void) sec__cred_free_cred_handle(&krb_info->client_creds);
    }

#endif
    
    krb_info->client_name = NULL;
    krb_info->server = NULL;

#ifndef _KERNEL
    st = sec_krb_dg_decode_message (&ap_req, &nonce, authz_fmt,
        &krb_info->client_name, &krb_info->client_pac, &krb_info->client_creds,
        &krb_info->server, &level, &authz_proto, &ksno2, &ses_key, &ivec,
        &expiration);

    RPC_DBG_PRINTF(rpc_e_dbg_auth, 4, ("(rpc__krb_dg_way) after sec_krb_dg_decode_message\n"));
    
    if (st != rpc_s_ok) {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_way) sec_krb_dg_decode_message failed, code %x\n", st));
        goto out_unlock;
    }
    
#else
    
    st = sec_krb_dg_decode_message_kern (&ap_req, &nonce, authz_fmt,
	h, actuid, boot_time, comp_st, &krb_info->client_name,
        &krb_info->client_pac, &krb_info->client_creds, &krb_info->server,
 	&level, &authz_proto,&ksno2, &ses_key, &ivec, &expiration);

    RPC_DBG_PRINTF(rpc_e_dbg_auth, 4, ("(rpc__krb_dg_way) after sec_krb_dg_decode_message_kern\n"));
    
    if (st != rpc_s_ok) {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_way) sec_krb_dg_decode_message_kern failed, code %x\n", st));
        goto out_unlock;
    }
#endif /* _KERNEL */
    
    if (krb_info->level_valid)
    {
        if (krb_info->auth_info.authn_level != level)
        {
            st = rpc_s_authn_level_mismatch;
            goto out_unlock;
        }
    }
    else
    {
        krb_info->auth_info.authn_level = level;
        krb_key->c.authn_level = level;
        krb_info->level_valid = 1;
    }

    if (ksno != -1)
    {
        if (ksno2 != ksno)
        {
            st = rpc_s_key_id_not_found;
            goto out_unlock;
        }
        if (((signed8) ((krb_key->current_key) - (ksno))) < 0)
            krb_key->current_key = ksno;
    }
    else
        krb_key->current_key = ksno2;

    RPC_DBG_PRINTF(rpc_e_dbg_auth, 2, ("(WAY) learned key %d\n", ksno2));
    rpc__krb_add_key (krb_key, ksno2, &ses_key, &ivec);

    if (krb_info->expiration < expiration)
        krb_info->expiration = expiration;

    if (krb_key->expiration < expiration)
        krb_key->expiration = expiration;

    krb_info->auth_info.authz_protocol = authz_proto;
    switch ((int)authz_proto) {
    case rpc_c_authz_name:
        krb_info->auth_info.u.s.privs = (rpc_authz_handle_t) krb_info->client_name;
	{ /* FAKE-EPAC */
	    krb_info->auth_info.u.s.creds = &krb_info->client_creds;
	}

        break;
    case rpc_c_authz_dce:
        krb_info->auth_info.u.s.privs = (rpc_authz_handle_t) &krb_info->client_pac;
	{ /* FAKE-EPAC */
	    krb_info->auth_info.u.s.creds = &krb_info->client_creds;
	}
        break;
    default:
        st = rpc_s_authn_authz_mismatch;
        goto out_unlock;
    }

    if ((krb_info->auth_info.server_princ_name == NULL)
        && (krb_info->server != NULL))
    {
        st = sec_krb_unparse_name (krb_info->server, (unsigned char **)&krb_info->auth_info.server_princ_name);
        RPC_DBG_PRINTF(rpc_e_dbg_auth, 1, ("(rpc__krb_dg_who_are_you) server name is %s\n", krb_info->auth_info.server_princ_name));
    }
    krb_info->client_valid = 1;
    /*
     * !!! little while lie.. we really need to fill in h->cred here.
     */
    krb_info->creds_valid = 1;       
out_unlock:
    RPC_KRB_INFO_UNLOCK(krb_info);
out:

    /*
     * If there was an error, there may still be data on the credentials
     * list.  If so, free it.
     */
    for (cred_ptr = cred_head; cred_ptr != NULL; )
    {
        cred_ptr = cred_ptr->next;
        RPC_MEM_FREE(cred_head, RPC_C_MEM_UTIL);
        cred_head = cred_ptr;
    }
    /*
     * If the credentials fit within a single buffer, then outbuf_ptr 
     * would have pointed into the credential list (which just got freed).
     * Otherwise, outbuf_ptr would have pointed at some memory allocated
     * separately from the credentials list.  If this is the case, free
     * that memory also.
     */
    if (outlen > MAX_AUTH_MESSAGE_LEN && outbuf_ptr != NULL) 
    {
	RPC_MEM_FREE(outbuf_ptr, RPC_C_MEM_UTIL);
    }
    *stp = st;
    return;
}


/*
 * R P C _ _ K R B _ D G _ C R E A T E
 *
 */

PRIVATE rpc_key_info_p_t rpc__krb_dg_create 
#ifdef _DCE_PROTO_
(
        unsigned32 *stp
)
#else
(stp)
    unsigned32 *stp;
#endif
{
    rpc_krb_info_p_t krb_info;
    rpc_krb_key_p_t krb_key;
    
    RPC_MEM_ALLOC (krb_info, rpc_krb_info_p_t, sizeof(*krb_info),
                   RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
    if (krb_info == NULL){
	*stp = rpc_s_no_memory;
	return(NULL);
    }
    RPC_MEM_ALLOC (krb_key, rpc_krb_key_p_t, sizeof(*krb_key),
                   RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
    if (krb_key == NULL){
	RPC_MEM_FREE(krb_info, RPC_C_MEM_UTIL);
	*stp = rpc_s_no_memory;
	return(NULL);
    }
    memset (krb_info, '\0', sizeof(*krb_info));
    memset (krb_key, '\0', sizeof(*krb_key));

    RPC_MUTEX_INIT(krb_info->lock);
    RPC_COND_INIT (krb_info->cond, krb_info->lock);
    
    krb_info->creds_valid = 0;
    krb_info->level_valid = 0;
    krb_info->client_valid = 0;

    krb_key->key_needed = -1;
    krb_key->sched_key_seq = -1;
    krb_key->expiration = 0;
    /*
     * fill in the common key stuff.
     */
    krb_key->c.auth_info = (rpc_auth_info_p_t)krb_info; 
    krb_key->c.refcnt = 1;
    krb_key->c.authn_level = -1;
    krb_key->c.is_server = 1;
    
    /*
     * fill in the common auth_info stuff.
     */
    
    krb_info->auth_info.refcount = 1;
    krb_info->auth_info.server_princ_name = 0;
    krb_info->auth_info.authn_level = -1;
    krb_info->auth_info.authn_protocol = rpc_c_authn_dce_private;
    krb_info->auth_info.authz_protocol = rpc_c_authz_name;
    krb_info->auth_info.is_server = 1;
    krb_info->auth_info.u.s.privs = 0;
    krb_info->auth_info.u.s.creds = 0;

    *stp = 0;
    return (rpc_key_info_p_t) krb_key;
}
