/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krbcom.c,v 65.9 1999/05/04 19:19:32 gwehrman Exp $";
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
 * $Log: krbcom.c,v $
 * Revision 65.9  1999/05/04 19:19:32  gwehrman
 * Replaced code use for SGIMIPS_DES_INLINE.  This code is still used when
 * building the user space libraries.  Fix for bug 691629.
 *
 * Revision 65.8  1999/02/04 22:37:19  mek
 * Support for external DES library.
 *
 * Revision 65.7  1999/01/19 20:28:17  gwehrman
 * Replaced include of krb5/des_inline.h with kdes_inline.h for kernel builds.
 *
 * Revision 65.6  1998/12/23 19:17:38  gwehrman
 * Changed the usage of sec_des_key_sched() to be compatible with the inlined
 * definition.
 *
 * Revision 65.5  1998/03/23 17:28:15  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:21:06  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:06  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:02  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.719.1  1996/06/04  21:55:15  arvind
 * 	u2u: Merge rpc__krb_srv_reg_auth_u2u and rpc__krb_inq_my_princ_name_tgt
 * 	to the  rpc_auth_epv_t rpc_g_krb_epv.
 * 	[1996/05/06  20:22 UTC  burati  /main/DCE_1.2/2]
 *
 * 	merge u2u work
 * 	[1996/04/29  20:45 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 *
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/04/29  20:08 UTC  burati  /main/HPDCE02/mb_mothra8/1]
 *
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:33 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Submitted the local rpc security bypass.
 * 	[1995/03/06  17:20 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	Fixed KRPC.
 * 	[1995/03/01  19:33 UTC  tatsu_s  /main/tatsu_s.local_rpc.b1/2]
 *
 * 	Local RPC Security Bypass.
 * 	[1995/02/22  22:32 UTC  tatsu_s  /main/tatsu_s.local_rpc.b1/1]
 *
 * Revision 1.1.717.2  1996/02/18  00:04:28  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:44  marty]
 * 
 * Revision 1.1.717.1  1995/12/08  00:20:47  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/03/06  17:20 UTC  tatsu_s
 * 	Submitted the local rpc security bypass.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b1/2  1995/03/01  19:33 UTC  tatsu_s
 * 	Fixed KRPC.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b1/1  1995/02/22  22:32 UTC  tatsu_s
 * 	Local RPC Security Bypass.
 * 	[1995/12/07  23:59:36  root]
 * 
 * Revision 1.1.715.2  1994/01/28  23:09:34  burati
 * 	EPAC changes - free creds in rpc__krb_free_info() (dlg_bl1)
 * 	[1994/01/25  03:44:03  burati]
 * 
 * Revision 1.1.715.1  1994/01/21  22:37:53  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:12  cbrooks]
 * 
 * Revision 1.1.5.5  1993/01/03  23:25:57  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:08:02  bbelch]
 * 
 * Revision 1.1.5.4  1992/12/23  20:51:54  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:39:47  zeliff]
 * 
 * Revision 1.1.5.3  1992/12/11  22:44:35  sommerfeld
 * 	*** empty log message ***
 * 
 * Revision 1.1.5.2  1992/12/11  21:12:03  sommerfeld
 * 	[CR6369] Flush references to RPC_KRB_KEY_LOCK (had been #if 0'ed before).
 * 	[1992/12/09  21:47:32  sommerfeld]
 * 
 * Revision 1.1.2.3  1992/05/21  13:17:05  mishkin
 * 	Raise debug level on a debug printf.
 * 	[1992/05/20  21:14:41  mishkin]
 * 
 * Revision 1.1.2.2  1992/05/01  17:22:10  rsalz
 * 	"Changed as part of rpc6 drop."
 * 	[1992/05/01  17:17:33  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:27  devrcs
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
**      krbcom.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**      Code common to both client and server sides of the
**      authentication module.
**
**
*/

#include <krbp.h>

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

INTERNAL rpc_auth_rpc_prot_epv_p_t rpc_g_krb_rpc_prot_epv[RPC_C_PROTOCOL_ID_MAX];

INTERNAL rpc_auth_epv_t rpc_g_krb_epv =
{
    rpc__krb_bnd_set_auth,
    rpc__krb_srv_reg_auth,
    rpc__krb_mgt_inq_def,
    rpc__krb_inq_my_princ_name,
    rpc__krb_free_info,
    rpc__krb_free_key,
    (rpc_auth_resolve_identity_fn_t) sec_krb_get_cc,
    (rpc_auth_release_identity_fn_t) sec_krb_ccache_free,
    rpc__krb_srv_reg_auth_u2u,
    rpc__krb_inq_my_princ_name_tgt
};


/*
 * R P C _ _ K R B _ I N I T
 *
 * Initialize the world.
 */

PRIVATE void rpc__krb_init 
#ifdef _DCE_PROTO_
(
    rpc_auth_epv_p_t *epv,
    rpc_auth_rpc_prot_epv_tbl_t *rpc_prot_epv,
    unsigned32 *st
)
#else
(epv, rpc_prot_epv, st)
rpc_auth_epv_p_t *epv;
rpc_auth_rpc_prot_epv_tbl_t *rpc_prot_epv;
unsigned32 *st;
#endif
{
    unsigned32                  prot_id;
    rpc_auth_rpc_prot_epv_t     *prot_epv;


    rpc__krb_crc_init();
    sec_krb_init();
    
    /*
     * Initialize the RPC-protocol-specific EPVs for the RPC protocols
     * we work with (ncadg and ncacn).
     */
#ifdef AUTH_KRB_DG
    prot_id = rpc__krb_dg_init (&prot_epv, st);
    if (*st == rpc_s_ok)
    {
        rpc_g_krb_rpc_prot_epv[prot_id] = prot_epv;
    }
#endif
#ifdef AUTH_KRB_CN
    prot_id = rpc__krb_cn_init (&prot_epv, st);
    if (*st == rpc_s_ok)
    {
        rpc_g_krb_rpc_prot_epv[prot_id] = prot_epv;
    }
#endif

    /*
     * Return information for this (Kerberos) authentication service.
     */
    *epv = &rpc_g_krb_epv;
    *rpc_prot_epv = rpc_g_krb_rpc_prot_epv;

    *st = 0;
}


/*
 * R P C _ _ K R B _ F R E E _ I N F O
 *
 * Free info and related resources.
 */

PRIVATE void rpc__krb_free_info 
#ifdef _DCE_PROTO_
(
        rpc_auth_info_p_t *info
)
#else
(info)
    rpc_auth_info_p_t *info;
#endif
{
    rpc_krb_info_p_t krb_info = (rpc_krb_info_p_t)*info ;
    error_status_t tst;
    void sec_id_pac_util_free(sec_id_pac_t *);

    RPC_DBG_PRINTF(rpc_e_dbg_general, 3, ("(rpc__krb_free_info) freeing auth_info.\n"));

    if (krb_info->auth_info.server_princ_name)
        rpc_string_free (&krb_info->auth_info.server_princ_name, &tst);
    RPC_COND_DELETE(krb_info->cond, krb_info->lock);
    RPC_MUTEX_DELETE(krb_info->lock);
    sec_krb_parsed_name_free (&krb_info->server);
    if (krb_info->cred)
        sec_krb_cred_free (&krb_info->cred);
    if (krb_info->client_name)
        rpc_string_free (&krb_info->client_name, &tst);
    sec_id_pac_util_free (&krb_info->client_pac);
    if (krb_info->auth_info.is_server == 0)
    {
        sec_krb_ccache_free ((sec_krb_ccache *) &krb_info->auth_info.u.auth_identity);
    } else {

#ifndef _KERNEL  /* FAKE-EPAC */
	tst = sec__cred_free_cred_handle(&krb_info->client_creds);
#endif

    }

    memset(krb_info, 0, sizeof(*krb_info));
    RPC_MEM_FREE(krb_info, RPC_C_MEM_UTIL);
}

/*
 * RPC__KRB_FREE_KEY
 *
 */

PRIVATE void rpc__krb_free_key 
#ifdef _DCE_PROTO_
(
        rpc_key_info_p_t *key
)
#else
(key)
    rpc_key_info_p_t *key;
#endif
{
    rpc_krb_key_p_t krb_key = (rpc_krb_key_p_t) *key;

    rpc__auth_info_release (&krb_key->c.auth_info);

    /* Zero out encryption keys */
    memset (krb_key, 0, sizeof(*krb_key));
    
    RPC_MEM_FREE(krb_key, RPC_C_MEM_UTIL);
}

/* 
 * R P C _ _ K R B _ S E T _ K E Y
 *
 * Make the "current" key of the context be "ksno", if it exists.
 * Returns with the context locked.
 */

PRIVATE unsigned32 rpc__krb_set_key 
#ifdef _DCE_PROTO_
(
        rpc_krb_key_p_t krb_key,
        int ksno
)
#else
(krb_key, ksno)
    rpc_krb_key_p_t krb_key;
    int ksno;
#endif
{
    int i;

    if (krb_key->c.authn_level == rpc_c_authn_level_none)
        return 0;
    
    if (krb_key->sched_key_seq == ksno)
        return 0;

    for (i=0; i < RPC__KRB_NKEYS; i++)
    {
        if (krb_key->keyseqs[i] == ksno)
        {
            krb_key->sched_key_idx = i;
            krb_key->sched_key_seq = ksno;

            /* we know this key is good already */
#ifdef SGIMIPS_DES_INLINE
            sec_des_key_sched (&krb_key->keys[i], krb_key->sched);
#else
            (void) sec_des_key_sched (&krb_key->keys[i], krb_key->sched);
#endif
            return 0;
        }
    }
    return rpc_s_key_id_not_found;
}

/*
 * R P C _ _ K R B _ A D D _ K E Y
 *
 * Add a new key to the context, with sequence number "ksno".
 * Must be called with the context locked.
 */

PRIVATE void rpc__krb_add_key 
#ifdef _DCE_PROTO_
(
        rpc_krb_key_p_t krb_key,
        int ksno,
        sec_des_key *key,
        sec_des_block *ivec
)
#else
(krb_key, ksno, key, ivec)
    rpc_krb_key_p_t krb_key;
    int ksno;
    sec_des_key *key;
    sec_des_block *ivec;
#endif
{
    int i;

    for (i=RPC__KRB_NKEYS-1; i > 0; i--)
    {
        krb_key->keys[i] = krb_key->keys[i-1];
        krb_key->ivec[i] = krb_key->ivec[i-1];
        krb_key->keyseqs[i] = krb_key->keyseqs[i-1];
    }
    krb_key->keys[0] = *key;
    krb_key->ivec[0] = *ivec;
    krb_key->keyseqs[0] = ksno;
}


/*
 * R P C _ _ K R B _ M G T _ I N Q _ D E F
 *
 * Return default authentication level
 *
 * !!! should read this from a config file.
 */

PRIVATE void rpc__krb_mgt_inq_def
#ifdef _DCE_PROTO_
(
        unsigned32 *authn_level,
        unsigned32 *st
)
#else
(authn_level, st)
    unsigned32 *authn_level;
    unsigned32 *st;
#endif
{
    *authn_level = rpc_c_authn_level_pkt_integrity;
    *st = 0;
}
