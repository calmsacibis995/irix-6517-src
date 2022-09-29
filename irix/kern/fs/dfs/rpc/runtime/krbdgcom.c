/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krbdgcom.c,v 65.8 1999/05/04 19:19:32 gwehrman Exp $";
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
 * $Log: krbdgcom.c,v $
 * Revision 65.8  1999/05/04 19:19:32  gwehrman
 * Replaced code use for SGIMIPS_DES_INLINE.  This code is still used when
 * building the user space libraries.  Fix for bug 691629.
 *
 * Revision 65.7  1999/02/04 22:37:19  mek
 * Support for external DES library.
 *
 * Revision 65.6  1999/01/19 20:28:18  gwehrman
 * Replaced include of krb5/des_inline.h with kdes_inline.h for kernel builds.
 *
 * Revision 65.5  1998/03/23 17:30:09  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:21:34  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:35  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:02  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.824.3  1996/02/18  00:04:34  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:47  marty]
 *
 * Revision 1.1.824.2  1995/12/08  00:20:52  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1994/10/19  16:49 UTC  tatsu_s
 * 	Merged the fix for OT12540, 12609 & 10454.
 * 
 * 	HP revision /main/tatsu_s.fix_ot12540.b0/1  1994/10/12  16:15 UTC  tatsu_s
 * 	Fixed OT12540: Added first_frag argument to rpc__krb_dg_recv_ck().
 * 	[1995/12/07  23:59:39  root]
 * 
 * Revision 1.1.818.3  1994/05/27  15:36:54  tatsu_s
 * 	Merged up with DCE1_1.
 * 	[1994/05/20  20:57:45  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	[1994/04/29  19:27:59  tatsu_s]
 * 
 * Revision 1.1.818.2  1994/05/19  21:14:56  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:42:50  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:22:31  hopkins]
 * 
 * Revision 1.1.818.1  1994/01/21  22:37:57  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:15  cbrooks]
 * 
 * Revision 1.1.7.2  1993/06/24  20:19:30  hinman
 * 	[hinman] - Save merged-in SNI version (these files have been freed of nasty code)
 * 	[1993/06/17  14:25:27  hinman]
 * 
 * Revision 1.1.5.4  1993/02/23  21:58:45  sommerfeld
 * 	Set unused fields in verifier to zero.
 * 	[1993/02/18  23:11:29  sommerfeld]
 * 
 * Revision 1.1.5.3  1993/01/03  23:26:05  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:08:14  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  20:52:08  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:39:59  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  16:37:24  rsalz
 * 	 06-feb-92 sommerfeld    move misplaced comma; fix screwed up
 * 	                         label before close-brace.
 * 	 29-jan-92 sommerfeld    locking reorganization.
 * 	                         All locks removed (piggyback on call lock).
 * 	 15-jan-92 sommerfeld    - key reorg: krb_info -> krb_key change
 * 	 07-jan-92 mishkin       - Misc style cleanups.
 * 	                         - Define krb/dg epv here (just one now).
 * 	                         - Add rpc__krb_dg_init().
 * 	[1992/05/01  16:29:17  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:29  devrcs
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
**      krbdgcom.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Functions common to server and client side of Kerberos DG
**  authentication module.
**
**
*/


#include <krbdgp.h>

#include <dce/assert.h>

#ifdef SGIMIPS
#ifdef SGIMIPS_DES_INLINE
#ifdef _KERNEL
#include <kdes_inline.h>
#else /* _KERNEL */
#include <krb5/des_inline.h>
#endif /* _KERNEL */
#else /* SGIMIPS_DES_INLINE */
#ifdef _KERNEL
#include <des.h>
#endif /* _KERNEL */
#endif /* SGIMIPS_DES_INLINE */
#endif /* SGIMIPS */

void dump_mem _DCE_PROTOTYPE_ ((void * /*bp_p*/, int /*len*/));

INTERNAL rpc_dg_auth_epv_t rpc_g_krb_dg_epv =
{
    rpc_c_authn_dce_private,    /* "1" */
    24,                         /* 18 bytes overhead, rounded up */
    8,                          /* 8 byte block */
    rpc__krb_dg_create,
    rpc__krb_dg_pre_call,
    rpc__krb_dg_encrypt,
    rpc__krb_dg_pre_send,
    rpc__krb_dg_recv_ck,
    rpc__krb_dg_who_are_you,
    rpc__krb_dg_way_handler,
    rpc__krb_dg_new_key
};


#ifdef MAX_DEBUG

/*
 * D U M P _ M E M 
 *
 * Dump memory to stdout for debugging.
 */

INTERNAL void dump_mem 
#ifdef _DCE_PROTO_
(
        void *bp_p,
        int len
)
#else
(bp_p, len)
    void *bp_p;
    int len;
#endif
{
#ifdef	_KERNEL
    unsigned16  i, j;

    for (i = 0; i < (len);)
    {
        printf("Offset: <%04x>", i);
        for (j = 0; (i < len) && (j < 16); i++, j++)
        {
            printf(" %02x",
                   (unsigned8)(((unsigned8 *)(bp_p))[i]));
        }
        printf("\n");
    }
#else
    unsigned char *bp = bp_p;
    int i;
    char msgbuf[128];

    for (i=0; i<len; i++) {
        if ((i % 16) == 0) {
	    if(i != 0) RPC_DBG_GPRINTF(("%s\n",msgbuf));
	    sprintf(msgbuf, "\nOffset: <%04x>", i);
	}
        sprintf(msgbuf, "%s %02x", msgbuf, bp[i]);
    }
    if(i != 0) RPC_DBG_GPRINTF(("%s\n",msgbuf));
#endif  /* _KERNEL */
}

#endif

#ifdef SNI_PRIVATE
#endif /* SNI_PRIVATE */

/*
 * R P C _ _ K R B _ D G _ I N I T
 *
 * Perform Kerberos/DG specific initialization and return a pointer to
 * the Kerberos/DG specific entry points.
 */

PRIVATE rpc_protocol_id_t rpc__krb_dg_init 
#ifdef _DCE_PROTO_
(
        rpc_auth_rpc_prot_epv_p_t       *epv,
        unsigned32                      *st
)
#else
(epv, st)
    rpc_auth_rpc_prot_epv_p_t       *epv;
    unsigned32                      *st;
#endif
{
    *epv = (rpc_auth_rpc_prot_epv_p_t) (&rpc_g_krb_dg_epv);
    *st = rpc_s_ok;
    return (RPC_C_PROTOCOL_ID_NCADG);
}



#define BLOCK_SIZE sizeof(sec_des_block)
#define LAST_BLOCK(ptr, len) \
    ((sec_des_block *)(((unsigned char *)(ptr)) + (((len) - 1) & ~(BLOCK_SIZE-1))))

/*
 * R P C _ _ K R B _ D G _ E N C R Y P T
 *
 * Optionally encrypt user data in the packet.
 */

PRIVATE void rpc__krb_dg_encrypt
#ifdef _DCE_PROTO_
(
        rpc_key_info_p_t               info,
        rpc_dg_xmitq_elt_p_t            xqe,
        unsigned32                      *stp
)
#else
(info, xqe, stp)
    rpc_key_info_p_t               info;
    rpc_dg_xmitq_elt_p_t            xqe;
    unsigned32                      *stp;
#endif
{
    unsigned32 st = 0;
#ifndef NOENCRYPTION
    rpc_krb_key_p_t krb_key = (rpc_krb_key_p_t)info;
    byte_p_t raw_body;
    sec_des_block plain_confounder;
    rpc_mp_t mp;
    int len = xqe->frag_len;
    rpc_dg_xmitq_elt_p_t last_xqe = xqe;
    sec_des_block *last_block;
    byte_p_t verifier;
    unsigned32 crc;
    
    if (krb_key->c.authn_level == rpc_c_authn_level_pkt_privacy)
    {
        st = rpc__krb_set_key (krb_key, krb_key->current_key);

        if (st != rpc_s_ok)
            goto out;
        
        /*
         * Compute pseudo-header, consisting of random confounder
         * concatenated with length.
         */
        st = sec_des_generate_random_block (&plain_confounder);
        if (st != rpc_s_ok)
            goto out;
        
#ifdef INLINE_PACKETS
	raw_body = xqe->body;
#else
        raw_body = (byte_p_t)(&xqe->body->args[0]);
#endif

        mp = (rpc_mp_t)&plain_confounder;

        /* skip over four random bytes */
        rpc_advance_mp(mp, 4);
        rpc_marshall_long_int (mp, len);

        /* compute CRC of pseudo-header + body */
        crc = rpc__krb_crc (0, (byte_p_t) &plain_confounder, 8);
        if (len > 0)
        {
            /*
             * Compute the crc.
             */

#ifdef INLINE_PACKETS
	    raw_body = last_xqe->body;
#else
            raw_body = (byte_p_t)(&last_xqe->body->args[0]);
#endif
            crc = rpc__krb_crc (crc, raw_body, last_xqe->body_len);

            while (last_xqe->more_data != NULL)
            {
#ifdef INLINE_PACKETS
		raw_body = last_xqe->more_data->body;
#else
                raw_body = (byte_p_t)(&last_xqe->more_data->body->args[0]);
#endif
                crc = rpc__krb_crc (crc, raw_body,
                                    last_xqe->more_data->body_len);
                last_xqe = last_xqe->more_data;
            }            
        }

        verifier = raw_body + last_xqe->body_len;
        rpc_marshall_long_int ((verifier+4), crc);

        /* encrypt confounder */
        sec_des_cbc_encrypt (&plain_confounder, (sec_des_block *)(verifier+8), 8,
            krb_key->sched, &krb_key->ivec[krb_key->sched_key_idx],
            sec_des_encrypt);

        /* encrypt body in place (chained off of confounder) */
        if (len > 0)
        {
            last_xqe = xqe;
#ifdef INLINE_PACKETS
	    raw_body = last_xqe->body;
#else
            raw_body = (byte_p_t)(&last_xqe->body->args[0]);
#endif
            sec_des_cbc_encrypt ((sec_des_block *)raw_body,
                (sec_des_block *)raw_body, last_xqe->body_len,
                krb_key->sched, (sec_des_block *)(verifier+8),
                sec_des_encrypt);
            last_block = LAST_BLOCK(raw_body, last_xqe->body_len);

            last_xqe = last_xqe->more_data;
            while (last_xqe != NULL)
            {
#ifdef INLINE_PACKETS
		raw_body = last_xqe->body;
#else
                raw_body = (byte_p_t)(&last_xqe->body->args[0]);
#endif
                sec_des_cbc_encrypt ((sec_des_block *)raw_body,
                    (sec_des_block *)raw_body, last_xqe->body_len,
                    krb_key->sched, last_block,
                    sec_des_encrypt);
                last_block = LAST_BLOCK(raw_body, last_xqe->body_len);

                last_xqe = last_xqe->more_data;
            }
        }
    out:
        ;
    }
#endif
    
    *stp = st;
}
    

/*
 * R P C _ _ K R B _ D G _ P R E _ S E N D
 *
 * Compute a verifier for a packet, optionally encrypt the arguments
 * in the packet.
 *
 * The amount of work depends on the security level in use.
 */

PRIVATE void rpc__krb_dg_pre_send 
#ifdef _DCE_PROTO_
(
        rpc_key_info_p_t info,
        rpc_dg_xmitq_elt_p_t pkt,
        rpc_dg_pkt_hdr_p_t hdrp,
        rpc_socket_iovec_p_t iov,
        int iovlen,
        pointer_t cksum,
        error_status_t *stp
)
#else
(info, pkt, hdrp, iov, iovlen, cksum, stp)
    rpc_key_info_p_t info;
    rpc_dg_xmitq_elt_p_t pkt;
    rpc_dg_pkt_hdr_p_t hdrp;
    rpc_socket_iovec_p_t iov;
    int iovlen;
    pointer_t cksum;
    error_status_t *stp;
#endif
{
    int st;
    unsigned32 pkt_crc, fragnum, seq;

    rpc_krb_key_p_t krb_key = (rpc_krb_key_p_t)info;
    rpc_authn_level_t level = krb_key->c.authn_level;
    int current_key = krb_key->current_key;
    rpc_mp_t mp, mp1;

    sec_des_block plaintext, hdrsum;
    sec_des_block *raw_header;
    sec_des_block *last_block;
    sec_md_struct mdstate;
    int i;

    assert ((iovlen > 0) && (iovlen <= RPC_C_DG_MAX_NUM_PKTS_IN_FRAG + 1));

    raw_header = (sec_des_block *)(iov[0].base);

    
    fragnum = hdrp->fragnum;

    st = rpc__krb_set_key (krb_key, current_key);
    
    /* krb_key is locked now */
    if (st != rpc_s_ok)
        goto out;
    
    /*
     * Marshall the fixed part of the verifier.
     */
    
    mp = cksum;
    
    rpc_marshall_byte (mp, level);
    rpc_advance_mp (mp, 1);
    rpc_marshall_byte (mp, current_key);
    rpc_advance_mp (mp, 1);
    rpc_marshall_byte (mp, 0);
    rpc_advance_mp (mp, 1);    
    rpc_marshall_byte (mp, 0);
    rpc_advance_mp (mp, 1);    
    /*
     * Set up a second marshalling ptr into the plaintext area.
     * We will marshall any plaintext into there, then encrypt it and
     * marshall it into the verifier.
     */

    switch ((int)level)
    {
    case rpc_c_authn_level_none:
    case rpc_c_authn_level_connect:
        /*
         * Do nothing here.
         */
        break;
        

        /*
         * Only authenticate the first pkt of each call.
         */
    case rpc_c_authn_level_call:
        /* flushed as bogus; fall back to next version */
        /*
         * Authenticate each packet
         */
    case rpc_c_authn_level_pkt:
        /*
         * Marshall sequence number and fragment number.
         * For a "direction bit" we use the high-order bit of the
         * sequence number.
         */
        seq = hdrp->seq;

        if (krb_key->c.is_server)
            seq ^= 0x80000000U;

        mp1 = (rpc_mp_t)&plaintext;
        rpc_marshall_long_int (mp1, seq); 
        rpc_advance_mp (mp1, 4);
        rpc_marshall_long_int (mp1, fragnum);
        rpc_advance_mp (mp1, 4);

        /*
         * Encrypt plaintext into vrfier
         */
        sec_des_cbc_encrypt (&plaintext, (sec_des_block *)mp, 8,
            krb_key->sched, &krb_key->ivec[krb_key->sched_key_idx],
            sec_des_encrypt);
        break;

        /*
         * Authenticate each packet with a strong integrity check.
         */
        
    case rpc_c_authn_level_pkt_integrity:   

        /*
         * Compute MDx digest of header and packet body; then encrypt
         * it using cbc mode into the verifier.
         */

        sec_md_begin (&mdstate);

        sec_md_update (&mdstate, (byte_p_t)raw_header, RPC_C_DG_RAW_PKT_HDR_SIZE);
        for (i = 1; i < iovlen; i++)
        {
            sec_md_update(&mdstate, (byte_p_t)(iov[i].base), (int)(iov[i].len));
        }

        sec_md_final (&mdstate);

        sec_des_cbc_encrypt ((sec_des_block *)mdstate.digest,
            (sec_des_block *)mp, sizeof(mdstate.digest),
            krb_key->sched, &krb_key->ivec[krb_key->sched_key_idx],
            sec_des_encrypt);

        break;
        
#ifndef NOENCRYPTION
        /*
         * Encrypt arguments
         */        
    case rpc_c_authn_level_pkt_privacy:

        if (iovlen > 1) {
            last_block = LAST_BLOCK(iov[iovlen-1].base, iov[iovlen-1].len);
        } else
            last_block = (sec_des_block *)(mp+4);

#ifdef MAX_DEBUG 
        if (RPC_DBG(rpc_es_dbg_auth, 7))
        {
            RPC_DBG_GPRINTF(("last block:\n"));
            dump_mem(last_block, 8);
        }
#endif
        
        /* finish CRC computation */
        rpc_unmarshall_long_int (mp, pkt_crc);

        pkt_crc = rpc__krb_crc (pkt_crc, (byte_p_t)raw_header, RPC_C_DG_RAW_PKT_HDR_SIZE);
        
        /* compute CBC cksum of raw header */
        sec_des_cbc_cksum (raw_header, &hdrsum, RPC_C_DG_RAW_PKT_HDR_SIZE,
            krb_key->sched, last_block);

#ifdef MAX_DEBUG
        if (RPC_DBG(rpc_es_dbg_auth, 7))
        {
            RPC_DBG_GPRINTF(("hdrsum:\n"));
            dump_mem(&hdrsum, 8);
        }
#endif

        /*
         * Marshall sequence number and CRC.
         */

    
        mp1 = (rpc_mp_t)&plaintext;
        rpc_marshall_long_int (mp1, hdrp->seq);
        rpc_advance_mp (mp1, 4);
        rpc_marshall_long_int (mp1, pkt_crc);
        rpc_advance_mp (mp1, 4);        

        /*
         * Encrypt plaintext into vrfier using cbc_cksum of header as ivec.
         */

        sec_des_cbc_encrypt (&plaintext, (sec_des_block *)(mp+12), 8,
            krb_key->sched, &hdrsum, sec_des_encrypt);

        break;
#endif  

    default:
        st = rpc_s_unsupported_authn_level;
        goto out;
    }
    st = rpc_s_ok;
out:
#if 0
    RPC_KRB_KEY_UNLOCK(krb_key);
#endif
    *stp = st;
    return;
}


/*
 * R P C _ _ K R B _ D G _ R E C V _ C K
 *
 * Receive a packet, decrypt the verifier and (optionally) the
 * arguments, and make sure that it's authentic.
 *
 * Depending on the security level, this may be easy or difficult.
 */

PRIVATE void rpc__krb_dg_recv_ck 
#ifdef _DCE_PROTO_
(
        rpc_key_info_p_t key_info,
        rpc_dg_recvq_elt_p_t pkt,
        pointer_t cksum,
        boolean32 first_frag,
        error_status_t *stp
)
#else
(key_info, pkt, cksum, stp)
    rpc_key_info_p_t key_info;
    rpc_dg_recvq_elt_p_t pkt;
    pointer_t cksum;
    boolean32 first_frag;
    error_status_t *stp;
#endif
{
    rpc_krb_key_p_t krb_key = (rpc_krb_key_p_t)key_info;
    rpc_dg_pkt_hdr_p_t hdrp=pkt->hdrp;
    rpc_authn_level_t level;
    int current_key;
    rpc_mp_t ump, ump1;
    ndr_format_t sender_drep;
    sec_des_block plaintext, hdrsum;
    sec_des_block *raw_header = (sec_des_block *) &(pkt->pkt->hdr); 
    sec_des_block *raw_body =
        (sec_des_block *) &(pkt->pkt->body);
    sec_des_block *last_block;
    sec_md_struct mdstate;
    unsigned32 st;
    unsigned32 vf_seq, vf_crc, vf_fragnum, pkt_crc, vf_len;
    int raw_bodysize = hdrp->len;
    rpc_dg_recvq_elt_p_t last_rqe = pkt;
    sec_des_block saved_last_block;
    unsigned32 pkt_bodysize;
    rpc_krb_info_p_t krb_info = (rpc_krb_info_p_t)(key_info->auth_info);

    /*
     * Find drep of sender.
     */
    NDR_UNPACK_DREP(&sender_drep, hdrp->drep);

    /*
     * Unmarshall the fixed part of the verifier.
     */

    ump = cksum;
    rpc_convert_byte (ndr_g_local_drep, sender_drep, ump, level);
    rpc_advance_mp (ump, 1);
    rpc_convert_byte (ndr_g_local_drep, sender_drep, ump, current_key);
    rpc_advance_mp (ump, 3);

    /*
     * Cached scall may have an unvalidated krb_info.
     */

    if (krb_key->c.is_server && !krb_info->level_valid)
    {
        /*
         * We can't do the call here, because that would violate the
         * locking hierarchy.
         * Instead, ask for our caller (almost certainly dgexec.c) to
         * call us back. 
         */
        
        RPC_DBG_GPRINTF((
            "(rpc__krb_dg_recv_ck) forcing callback for krb_info\n"));
        st = rpc_s_dg_need_way_auth;
        goto out;
    }

    /*
     * Try to find the key.
     */

    st = rpc__krb_set_key (krb_key, current_key);

    /* CTX is locked. */
    if ((st == rpc_s_key_id_not_found)
        && krb_key->c.is_server)
    {
        /*
         * This may be a new key that we can learn.
         * We can't do the call here, because that would violate the
         * locking hierarchy.
         * Instead, ask for our caller (almost certainly dgexec.c) to
         * call us back. 
         */
        
        RPC_DBG_GPRINTF((
            "(rpc__krb_dg_recv_ck) forcing callback for key %d\n",
            current_key));
        krb_key->key_needed = current_key;
        st = rpc_s_dg_need_way_auth;
        goto out;
    } else if (st != rpc_s_ok)
        goto out;

    /*
     * Is the context properly initialized?
     */
    
    if (level != krb_key->c.authn_level)  {
        st = rpc_s_authn_level_mismatch;
        goto out;
    }

    /*
     * Check for authentication expiration.
     */
    if (krb_key->c.is_server &&
        level > rpc_c_authn_level_none &&
        first_frag && hdrp->fragnum == 0 &&
        rpc__clock_unix_expired(krb_key->expiration+5*60))
    {
        RPC_DBG_GPRINTF(("(rpc__krb_dg_recv_ck) authentication expired\n"));
        st = rpc_s_auth_tkt_expired;
        goto out;
    }

    /*
     * Dispatch based on level to appropriate protocol banging.
     */

    switch ((int)level)
    {
        /*
         * Don't do any per-packet stuff for connect level.
         */
    case rpc_c_authn_level_none:
    case rpc_c_authn_level_connect:
        break;
        /*
         * Authenticate only the first packet of each call.
         */
        /* flushed as bogus; fall back to next level */
    case rpc_c_authn_level_call:
        /* 
         * Authenticate each packet.
         */
        
    case rpc_c_authn_level_pkt:
        /*
         * Decrypt verifier, then unmarshall sequence and fragno.
         */

#ifdef MAX_DEBUG
        if (RPC_DBG(rpc_es_dbg_auth, 7))
        {
            RPC_DBG_GPRINTF(("vf in pkt:"));
            dump_mem(ump, sizeof(sec_des_block));
        }
        if (RPC_DBG(rpc_es_dbg_auth, 8))
        {
            RPC_DBG_GPRINTF(("key sched:"));
            dump_mem(krb_key->sched, sizeof(krb_key->sched));
        }
#endif
        sec_des_cbc_encrypt ((sec_des_block *)ump, &plaintext, 8,
            krb_key->sched, &krb_key->ivec[krb_key->sched_key_idx],
            sec_des_decrypt);

#ifdef MAX_DEBUG
        if (RPC_DBG(rpc_es_dbg_auth, 7))
        {
            RPC_DBG_GPRINTF(("decrypted vf:"));
#ifdef KERNEL	/* the ficus 6.4 mongoose compiler wants this */
            ump1 = (rpc_mp_t)&plaintext;
#endif /* KERNEL */
            dump_mem(ump1, sizeof(sec_des_block));
        }
#endif
        ump1 = (rpc_mp_t)&plaintext;
        rpc_convert_long_int (ndr_g_local_drep, sender_drep, ump1, vf_seq);
        rpc_advance_mp (ump1, 4);
        rpc_convert_long_int (ndr_g_local_drep, sender_drep, ump1, vf_fragnum);
        rpc_advance_mp (ump1, 4);

        /*
         * Un-do direction bit.
         */
         
        if (!krb_key->c.is_server)
            vf_seq  ^= 0x80000000U;
        
        if ((hdrp->seq != vf_seq) ||
            (hdrp->fragnum != vf_fragnum))
        {
            RPC_DBG_GPRINTF(("vf mismatch: pkt %x.%x vs vf %x.%x\n",
                hdrp->fragnum, hdrp->seq,
                vf_fragnum, vf_seq));
            st = rpc_s_invalid_crc;
            goto out;
        }
        break;
        
    case rpc_c_authn_level_pkt_integrity:   /* strong integrity check */

        /*
         * compute MDx digest of raw packet (header/body),
         * then encrypt it in CBC mode under the connections session key/ivec;
         * verify that it matches the encrypted verifier.
         */
        
        sec_md_begin(&mdstate);

        sec_md_update(&mdstate, (byte_p_t)raw_header,  RPC_C_DG_RAW_PKT_HDR_SIZE);
        
        if (raw_bodysize != 0)
        {
            pkt_bodysize = MIN(raw_bodysize,
                               last_rqe->pkt_len - RPC_C_DG_RAW_PKT_HDR_SIZE);
            sec_md_update(&mdstate,
                          (byte_p_t)&(last_rqe->pkt->body),
                          pkt_bodysize);
            raw_bodysize -= pkt_bodysize;

            last_rqe = last_rqe->more_data;
            while (last_rqe != NULL && raw_bodysize > 0)
            {
                pkt_bodysize = MIN(raw_bodysize,
                                   last_rqe->pkt_len);
                sec_md_update(&mdstate,
                              (byte_p_t)&(last_rqe->pkt->body),
                              pkt_bodysize);

                raw_bodysize -= pkt_bodysize;
                last_rqe = last_rqe->more_data;
            }            
        }

        sec_md_final(&mdstate);
        
        sec_des_cbc_encrypt ((sec_des_block *)mdstate.digest,
            (sec_des_block *)mdstate.digest,
            sizeof(mdstate.digest),
            krb_key->sched, &krb_key->ivec[krb_key->sched_key_idx],
            sec_des_encrypt);
        
        if (memcmp (mdstate.digest, ump, sizeof(mdstate.digest)) != 0)
        {
#ifdef MAX_DEBUG
            if (RPC_DBG(rpc_es_dbg_general, 1)) 
            {
                RPC_DBG_GPRINTF(("vf mismatch; pkt:\n"));
                dump_mem(ump, 16);
                RPC_DBG_GPRINTF(("computed:\n"));
                dump_mem(mdstate.digest, 16);
            }
#endif
            st = rpc_s_auth_bad_integrity;
            goto out;
        }
        break;

#ifndef NOENCRYPTION
        /*
         * Encrypt all arguments.
         */
    case rpc_c_authn_level_pkt_privacy:
        
        /*
         * Unpack <confounder, length>.
         */

        sec_des_cbc_encrypt ((sec_des_block *)(ump+4), &plaintext, 8,
            krb_key->sched, &krb_key->ivec[krb_key->sched_key_idx],
            sec_des_decrypt);

        ump1 = (rpc_mp_t)&plaintext;
        rpc_advance_mp(ump1, 4);
        
        rpc_convert_long_int (ndr_g_local_drep, sender_drep, ump1, vf_len);
        if (vf_len != raw_bodysize)
        {
            st = rpc_s_auth_bad_integrity;
            goto out;
        }

        /*
         * Compute crc32 of pseudo-header.
         */
        pkt_crc = rpc__krb_crc (0, (byte_p_t)&plaintext, 8);

        /*
         * Find last_block in the last packet buffer.
         */

        if (raw_bodysize == 0)
            last_block = (sec_des_block *)(ump+4);
        else
        /*
         * ... while decrypting body in place and computing crc32 of
         * plaintext packet body.
         */
        {
            sec_des_block raw_last_block;

            pkt_bodysize = MIN(raw_bodysize,
                               last_rqe->pkt_len - RPC_C_DG_RAW_PKT_HDR_SIZE);

            raw_body = (sec_des_block *) &(last_rqe->pkt->body);
            last_block = LAST_BLOCK(raw_body, pkt_bodysize);
            saved_last_block = *last_block;

            sec_des_cbc_encrypt (raw_body, raw_body, pkt_bodysize,
                                 krb_key->sched, (sec_des_block *)(ump+4),
                                 sec_des_decrypt);

            pkt_crc = rpc__krb_crc (pkt_crc, (byte_p_t)raw_body, pkt_bodysize);

            raw_bodysize -= pkt_bodysize;

            last_rqe = last_rqe->more_data;
            while (last_rqe != NULL && raw_bodysize > 0)
            {
                pkt_bodysize = MIN(raw_bodysize, last_rqe->pkt_len);
                raw_body = (sec_des_block *) &(last_rqe->pkt->body);
                last_block = LAST_BLOCK(raw_body, pkt_bodysize);
                raw_last_block = saved_last_block;
                saved_last_block = *last_block;

                sec_des_cbc_encrypt (raw_body, raw_body, pkt_bodysize,
                                     krb_key->sched, &raw_last_block,
                                     sec_des_decrypt);

                pkt_crc = rpc__krb_crc (pkt_crc, (byte_p_t)raw_body,
                                        pkt_bodysize);

                raw_bodysize -= pkt_bodysize;
                last_rqe = last_rqe->more_data;
            }            

            last_block = &saved_last_block;
        }

        /*
         * compute CBC_cksum of header, with ivec from rest of packet.
         */
        sec_des_cbc_cksum (raw_header, &hdrsum, RPC_C_DG_RAW_PKT_HDR_SIZE,
            krb_key->sched, last_block);
        
#ifdef MAX_DEBUG
        if (RPC_DBG(rpc_es_dbg_auth, 7))
        {
            RPC_DBG_GPRINTF(("hdrsum:\n"));
            dump_mem(&hdrsum, 8);
            RPC_DBG_GPRINTF(("ciphertext args, take 1:\n"));
            dump_mem(raw_body, raw_bodysize);
        }
#endif

        /*
         * Compute crc32 of pseudo-header + body + header.
         */
        pkt_crc = rpc__krb_crc (pkt_crc, (byte_p_t)raw_header, RPC_C_DG_RAW_PKT_HDR_SIZE);
        
        /*
         * Decrypt vrfier into plaintext using last block of CBC as ivec
         */

        sec_des_cbc_encrypt ((sec_des_block *)(ump+12), &plaintext, 8,
            krb_key->sched, &hdrsum, sec_des_decrypt);

#ifdef MAX_DEBUG
        if (RPC_DBG(rpc_es_dbg_auth, 7))
        {
            RPC_DBG_GPRINTF(("plaintext vf:\n"));
            dump_mem(&plaintext, 8);
        }
#endif
        ump1 = (rpc_mp_t)&plaintext;
        rpc_convert_long_int (ndr_g_local_drep, sender_drep, ump1, vf_seq);
        rpc_advance_mp (ump1, 4);

        rpc_convert_long_int (ndr_g_local_drep, sender_drep, ump1, vf_crc);
        rpc_advance_mp (ump1, 4);

        /*
         * Verify that packet is intact.
         */
        
        if ((vf_crc != pkt_crc)
            || (vf_seq != hdrp->seq))
        {
            RPC_DBG_GPRINTF(("vf mismatch: pkt %x.%x vs vf %x.%x\n",
                pkt_crc, hdrp->seq,
                vf_crc, vf_seq));
            st = rpc_s_auth_bad_integrity;
            goto out;
        }           
        break;
#endif  
        
    default:
        st = rpc_s_unsupported_authn_level;
        goto out;
    }
    st = rpc_s_ok;
out:
#if 0
    RPC_KRB_KEY_UNLOCK(krb_key);
#endif
    *stp = st;
    return;
}
