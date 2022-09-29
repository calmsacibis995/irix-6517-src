/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: krbp.h,v $
 * Revision 65.2  1997/10/27 17:33:20  jdoak
 * 6.4 + 1.2.2 merge
 *
 * Revision 65.1  1997/10/20  19:17:03  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.105.1  1996/06/04  21:55:43  arvind
 * 	Merge u2u: Add tgt_length and tgt_data to  krb_info structure
 * 	Add prototype for rpc__krb_srv_reg_auth_u2u()
 * 	Add  prototype for rpc__krb_inq_my_princ_name_tgt()
 * 	Fix prototype(unsigned -> void returned)  for rpc__krb_add_key()
 * 	[1996/05/06  20:28 UTC  burati  /main/DCE_1.2/2]
 *
 * 	merge  u2u  work
 * 	[1996/04/29  21:06 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 *
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/04/29  20:26 UTC  burati  /main/HPDCE02/mb_mothra8/1]
 *
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:34 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Submitted the local rpc security bypass.
 * 	[1995/03/06  17:20 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	Local RPC Security Bypass.
 * 	[1995/02/22  22:32 UTC  tatsu_s  /main/tatsu_s.local_rpc.b1/1]
 *
 * Revision 1.1.103.2  1996/02/18  22:56:28  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:33  marty]
 * 
 * Revision 1.1.103.1  1995/12/08  00:20:56  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:34 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/03/06  17:20 UTC  tatsu_s
 * 	Submitted the local rpc security bypass.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b1/1  1995/02/22  22:32 UTC  tatsu_s
 * 	Local RPC Security Bypass.
 * 	[1995/12/07  23:59:42  root]
 * 
 * Revision 1.1.101.2  1994/01/28  23:09:39  burati
 * 	EPAC changes - add client_creds to rpc_krb_info_t (dlg_bl1)
 * 	[1994/01/24  23:50:46  burati]
 * 
 * Revision 1.1.101.1  1994/01/21  22:38:02  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:19  cbrooks]
 * 
 * Revision 1.1.8.1  1993/09/30  14:39:57  sommerfeld
 * 	Flush machine-specific #ifdef.
 * 	[1993/09/28  21:34:57  sommerfeld]
 * 
 * Revision 1.1.6.5  1993/01/03  23:26:13  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:08:25  bbelch]
 * 
 * Revision 1.1.6.4  1992/12/23  20:52:18  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:40:11  zeliff]
 * 
 * Revision 1.1.6.3  1992/12/11  22:45:10  sommerfeld
 * 	*** empty log message ***
 * 
 * Revision 1.1.6.2  1992/12/11  21:12:53  sommerfeld
 * 	[CR6369] flush references to key mutex (was #if 0'ed out before).
 * 	[1992/12/09  21:47:53  sommerfeld]
 * 
 * Revision 1.1.2.4  1992/07/02  20:02:25  sommerfeld
 * 	[CR4539] Add RPC_KRB_INFO_COND_BROADCAST.
 * 	[1992/06/30  22:59:25  sommerfeld]
 * 
 * Revision 1.1.2.3  1992/06/26  06:01:34  jim
 * 	included sys/domain.h and sys/protosw.h for the AIX
 * 	kernel extension on AIX 3.2.
 * 	[1992/05/29  18:26:03  jim]
 * 
 * Revision 1.1.2.2  1992/05/01  16:37:42  rsalz
 * 	 30-mar-92 af            Fix prototype for rpc__krb_crc() to match
 * 	                         the function definition.  Return type
 * 	                         changed from unsigned long to unsigned32 and
 * 	                         parameter seed changed from unsigned long to
 * 	                         unsigned32.
 * 	 04-feb-92 ws            add RPC_KRB_INFO_COND_SIGNAL macro.
 * 	 04-feb-92 ws            locking reorganization.
 * 	 15-jan-92 ws            Full key reorg:
 * 	                         - split keys into separate structure.
 * 	 07-jan-92 nm            - Merge auth5 changes.
 * 	                         - Add AUTH_KRB_{DG,CN} stuff.
 * 	                         - Flush rpc__krb_get_prot_info().
 * 	                         - Misc. style cleanups.
 * 	 07-jan-92 ko            Remove refcount from rpc_krb_info_t
 * 	                         (its in the common rpc_auth_info_t) now.
 * 	                         Add declaration of rpc__krb_cn_init().
 * 	                         Remove declarations of
 * 	                         rpc__krb_[reference, release].
 * 	[1992/05/01  16:29:31  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:10:52  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _KRBP_H
#define _KRBP_H 1

/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**
**      krbp.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**      Types and routines private to the kerberos authentication module.
**
**
*/

#include <commonp.h>
#include <com.h>
#include <comp.h>
    
#include <netinet/in.h>         /* for htonl/ntohl */

#include <dce/sec_authn.h>

/*
 * We allow a little flexibility in whether we support one or both RPC
 * protocols for Kerberos authentication.  To simplify Makefiles, etc.
 * if neither RPC-protocol-specific symbol is defined, just assume we
 * want both.
 */
#if !defined(AUTH_KRB_DG) && !defined(AUTH_KRB_CN)
#define AUTH_KRB_DG
#define AUTH_KRB_CN
#endif

/*
 * Max number of keys kept at once on each end of the conversation.
 * This assumes that keys are changed in an interval >> than the round
 * trip time between client and server.
 */

#define RPC__KRB_NKEYS 3

/*
 * State block containing all the state of one end of an authenticated
 * connection.
 */

typedef struct rpc_krb_info_t {
    rpc_auth_info_t auth_info;  /* This must be the first element. */
    rpc_mutex_t lock;
    rpc_cond_t cond;            /* wait on this when valid is turned off. */
    error_status_t status;          /* "poison" status. */
    sec_krb_parsed_name server; /* parsed server name */
    sec_krb_cred cred;          /* credential used */
    unsigned32 expiration;            /* expiration time of credentials. */

    unsigned char *client_name; /* client name */
    sec_id_pac_t client_pac;   /* client PAC */

    /* FAKE-EPAC */
    rpc_authz_cred_handle_t  client_creds;  /* 1.1 epac-style cred handle */

    unsigned int creds_valid: 1;         /* credentials valid */
    unsigned int level_valid: 1;         /* level valid */
    unsigned int client_valid: 1;        /* is client valid? */
    unsigned int cred_fetching: 1;       /* cred fetch in process. */
    
    unsigned32 tgt_length;      /* TGT For user to user protocol */
    unsigned_char_p_t tgt_data;
    
    /* put addl flags here. */
#ifdef KEY_IN_INFO
    sec_des_key keys[ RPC__KRB_NKEYS ];           /* currently active keys, in inverse order */
    sec_des_block ivec[ RPC__KRB_NKEYS ];           /* ivec associated with above */
    int keyseqs[ RPC__KRB_NKEYS ];            /* key seq nos of these keys. */
    
    int sched_key_seq;          /* key sequence of following key schedule */
    int sched_key_idx;          /* index into keys array of above */
    sec_des_key_schedule sched;        /* this is big; only keep one */

    int current_key;            /* current key */
    int key_needed;             /* ksno needed */
    unsigned32 expiration;            /* expiration time of credentials. */
#endif
} rpc_krb_info_t, *rpc_krb_info_p_t;

typedef struct rpc_krb_key_t {
    rpc_key_info_t           c;               /* common */
    sec_des_key keys[RPC__KRB_NKEYS];           /* currently active keys, in inverse order */
    sec_des_block ivec[RPC__KRB_NKEYS];           /* ivec associated with above */
    int keyseqs[RPC__KRB_NKEYS];            /* key seq nos of these keys. */
    
    int sched_key_seq;          /* key sequence of following key schedule */
    int sched_key_idx;          /* index into keys array of above */
#ifdef	SGIMIPS64
    long pad1;		/* For 8-byte alignment of sched */
#endif
    sec_des_key_schedule sched;        /* this is big; only keep one */

    int current_key;            /* current key */
    int key_needed;             /* ksno needed */
    unsigned32 expiration;            /* expiration time of credentials. */
} rpc_krb_key_t, *rpc_krb_key_p_t; 
    
/*
 * Locking macros.
 */

#define RPC_KRB_INFO_LOCK(info) RPC_MUTEX_LOCK ((info)->lock)
#define RPC_KRB_INFO_UNLOCK(info) RPC_MUTEX_UNLOCK ((info)->lock)
#define RPC_KRB_INFO_LOCK_ASSERT(info) RPC_MUTEX_LOCK_ASSERT((info)->lock)
#define RPC_KRB_INFO_COND_WAIT(info) RPC_COND_WAIT((info)->cond, (info)->lock)
#define RPC_KRB_INFO_COND_SIGNAL(info) RPC_COND_SIGNAL((info)->cond, (info)->lock)
#define RPC_KRB_INFO_COND_BROADCAST(info) RPC_COND_BROADCAST((info)->cond, (info)->lock)


/*
 * Prototypes for PRIVATE routines.
 */

#ifdef __cplusplus
extern "C" {
#endif


PRIVATE rpc_protocol_id_t       rpc__krb_cn_init _DCE_PROTOTYPE_ ((
         rpc_auth_rpc_prot_epv_p_t      * /*epv*/,
         unsigned32                     * /*st*/
    ));

PRIVATE rpc_protocol_id_t       rpc__krb_dg_init _DCE_PROTOTYPE_ ((
         rpc_auth_rpc_prot_epv_p_t      * /*epv*/,
         unsigned32                     * /*st*/
    ));    

/*
 * Prototypes for API EPV routines.
 */

void rpc__krb_bnd_set_auth _DCE_PROTOTYPE_ ((
        unsigned_char_p_t                   /* in  */    /*server_princ_name*/,
        rpc_authn_level_t                   /* in  */    /*authn_level*/,
        rpc_auth_identity_handle_t          /* in  */    /*auth_identity*/,
        rpc_authz_protocol_id_t             /* in  */    /*authz_protocol*/,
        rpc_binding_handle_t                /* in  */    /*binding_h*/,
        rpc_auth_info_p_t                   /* out */   * /*auth_info*/,
        unsigned32                          /* out */   * /*st*/
    ));

void rpc__krb_srv_reg_auth _DCE_PROTOTYPE_ ((
        unsigned_char_p_t                   /* in  */    /*server_princ_name*/,
        rpc_auth_key_retrieval_fn_t         /* in  */    /*get_key_func*/,
        pointer_t                           /* in  */    /*arg*/,
        unsigned32                          /* out */   * /*st*/
    ));

void rpc__krb_srv_reg_auth_u2u _DCE_PROTOTYPE_ ((
        unsigned_char_p_t                   /* in  */    /*server_princ_name*/,
        rpc_auth_identity_handle_t          /* in  */    /*id_h*/,
        unsigned32                          /* out */   * /*st*/
    ));

void rpc__krb_mgt_inq_def _DCE_PROTOTYPE_ ((
        unsigned32                          /* out */   * /*authn_level*/,
        unsigned32                          /* out */   * /*st*/
    ));

void rpc__krb_inq_my_princ_name _DCE_PROTOTYPE_ ((
        unsigned32                          /* in */     /*princ_name_size*/,
        unsigned_char_p_t                   /* out */    /*princ_name*/,
        unsigned32                          /* out */  * /*st*/
    ));

void rpc__krb_inq_my_princ_name_tgt _DCE_PROTOTYPE_ ((
        unsigned32                          /* in */     /*name_size*/,
        unsigned32                          /* in */     /*max_tgt_size*/,
        unsigned_char_p_t                   /* out */    /*name*/,
        unsigned32                          /* out */  * /*tgt_length*/,
        unsigned_char_p_t                   /* out */    /*tgt_data*/,
        unsigned32                          /* out */  * /*st*/
    ));

void rpc__krb_free_info _DCE_PROTOTYPE_((
        rpc_auth_info_p_t                   /* in/out */ * /*info*/
    ));

void rpc__krb_free_key _DCE_PROTOTYPE_((
        rpc_key_info_p_t                   /* in/out */ * /*info*/
    ));


/*
 * Miscellaneous internal entry points.
 */

/*
 * add this key to context, bumping out older keys as we go along;
 */
void rpc__krb_add_key _DCE_PROTOTYPE_((
        rpc_krb_key_p_t  /*info*/,
        int  /*ksno*/,
        sec_des_key * /*key*/,
        sec_des_block * /*ivec*/
    ));

/*
 * make the key sched reflect this key; returns nonzero code if not possible 
 * leaves handle read-locked
 */
unsigned32 rpc__krb_set_key _DCE_PROTOTYPE_((
        rpc_krb_key_p_t  /*info*/,
        int  /*ksno*/
    ));

/* 
 * get a current ticket for this connection
 */
unsigned32 rpc__krb_get_tkt _DCE_PROTOTYPE_((
        rpc_krb_info_p_t  /*info*/
    ));

/* 
 * get PAC instead of ticket for this connection (much hair here)
 */
unsigned32 rpc__krb_get_pac _DCE_PROTOTYPE_((
        rpc_krb_info_p_t  /*info*/
    ));

/*
 * Random number generation.
 */
extern unsigned32 rpc__krb_gen_key _DCE_PROTOTYPE_((
        rpc_krb_key_p_t                
    ));

extern void rpc__krb_generate_nonce _DCE_PROTOTYPE_((
        char * /*nonce*/
    ));

/*
 * Prototypes for CRC functions.
 */

void rpc__krb_crc_init _DCE_PROTOTYPE_((                         
        void                            
    ));

unsigned32 rpc__krb_crc _DCE_PROTOTYPE_((                         
        unsigned32                       /*seed*/,
        byte_p_t                         /*bytes*/,
        int                              /*nbytes*/
    ));

#ifdef __cplusplus
}
#endif

#endif /* _KRBP_H */
