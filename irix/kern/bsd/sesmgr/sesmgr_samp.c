/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.7 $"
#include <bstring.h>
#include <sys/types.h> 
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/tcp-param.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/xlate.h>
#include <sys/var.h>
#include <sys/kmem.h>
#include <sys/tcpipstats.h>
#include <sys/cred.h>

#include <net/if.h>
#include <net/route.h>
#include <net/ksoioctl.h>
#include <sys/kuio.h>
#include <sys/xlate.h>
#include <sys/sema.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <sys/sat.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/sesmgr.h>
#include <sys/t6rhdb.h>
#include <sys/t6samp.h>
#include <sys/t6satmp.h>
#include <sys/t6api_private.h>
#include <sys/atomic_ops.h>
#include <sys/ip_secopts.h>
#include <sys/sesmgr_samp.h>
#include <sys/sm_private.h>
#include <sys/t6refcnt.h>

/*
 * structure for holding attribute data
 */
typedef union _attr_entry_t {
	uid_t attr_sid;
	cap_set_t attr_privs;
	uid_t attr_audit_id;
	t6ids_t attr_groups;
} token_buf_t;

#ifdef DEBUG
extern void sbccverify(struct sockbuf *); /* from socket/uipc_socket2.c */
#endif

extern struct ifnet *inaddr_to_ifp(struct in_addr addr);

static int samp_attr_from_token(int, token_entry_t *, token_entry_t *);
static int samp_token_from_attr(int, token_entry_t *, token_entry_t *);

static struct hashinfo *token_tbls[T6SAMP_MAX_TOKENS];
static struct hashinfo host_info;
static zone_t *host_tbl_zone;
static zone_t *token_tbl_zone;
static zone_t *attr_zone;

/*
 *  Initialize the host and token hash tables.  This routine is invoked
 *  from with the sesmgr_init routine, and must also be invoked
 *  only after kern heap is initialized.
 */
void
sesmgr_samp_init(void)
{
	static struct hashtable htbls[SAMP_TOKEN_NTABLES][HASHTABLE_SIZE];
	static struct hashinfo token_info[SAMP_TOKEN_NTABLES];
	static struct hashtable host_tbl[HASHTABLE_SIZE];

	/* initialize host hash tables */
	hash_init (&host_info, host_tbl, HASHTABLE_SIZE,
		   sizeof (struct in_addr), HASH_MASK);
	host_tbl_zone = kmem_zone_init(sizeof(host_entry_t),
				       "samp host table");
	ASSERT (host_tbl_zone != NULL);

	/* initialize token hash tables */
	hash_init (token_tbls[T6SAMP_SL] = &token_info[0], htbls[0],
		   HASHTABLE_SIZE, sizeof(struct in_addr), HASH_MASK);
	hash_init (token_tbls[T6SAMP_INTEG_LABEL] = &token_info[1], htbls[1],
		   HASHTABLE_SIZE, sizeof(struct in_addr), HASH_MASK);
	hash_init (token_tbls[T6SAMP_SID] = &token_info[2], htbls[2],
		   HASHTABLE_SIZE, sizeof(struct in_addr), HASH_MASK);
	hash_init (token_tbls[T6SAMP_PRIVS] = &token_info[3], htbls[3],
		   HASHTABLE_SIZE, sizeof(struct in_addr), HASH_MASK);
	hash_init (token_tbls[T6SAMP_LUID] = &token_info[4], htbls[4],
		   HASHTABLE_SIZE, sizeof(struct in_addr), HASH_MASK);
	hash_init (token_tbls[T6SAMP_IDS] = &token_info[5], htbls[5],
		   HASHTABLE_SIZE, sizeof(struct in_addr), HASH_MASK);
	token_tbl_zone = kmem_zone_init(sizeof(token_entry_t),
					"samp token table");
	ASSERT (token_tbl_zone != NULL);

	/* initialize attribute zone */
	attr_zone = kmem_zone_init(sizeof(token_buf_t), "attribute table");
	ASSERT (attr_zone != NULL);
}

/**********************************************************************
 *  The following insert samp headers and attributes into the tcp/udp
 *  data streams.  Note: sesmgr_soreceive() which processes the incoming
 *  samp headers is located in socket/uipc_socket.c.
 */

/*
 *  This routine prepends an mbuf containing the attributes of the
 *  sending process to the data being written.
 *
 *  UDP has a full set of samp attributes on each write, TCP only
 *  needs attributes when they have changed, however, because of the
 *  need to specify the number of bytes of data covered by the SAMP
 *  header, we put a samp header on each user write.  This samp header
 *  may be a null header (just length, no attributes) if the process
 *  attributes have not changed since the last header.
 */
int
sesmgr_put_samp(struct socket * so, struct mbuf **data, struct mbuf *nam)
{
	short protocol = so->so_proto->pr_protocol;
	struct inpcb *inp = sotoinpcb(so);
	struct sockaddr_in *sin;
	struct in_addr dst;
	int user_data_len;
	
	t6rhdb_kern_buf_t hsec;
	struct mbuf *sm;
	struct ipsec *soattrs;

	soattrs = so->so_sesmgr_data;
	ASSERT(soattrs != NULL);

	/* Only applies to TCP and UDP */
	if (so->so_proto->pr_domain->dom_family != AF_INET &&
	    so->so_proto->pr_domain->dom_family != AF_UNIX)
		return 0;
	if (so->so_proto->pr_domain->dom_family == AF_INET)
		if (protocol != IPPROTO_UDP && protocol != IPPROTO_TCP)
			return 0;

	if (so->so_proto->pr_domain->dom_family == AF_INET) {
		/* Determine destination host */ 
		switch (protocol) {
		case IPPROTO_TCP:
			dst.s_addr = inp->inp_faddr.s_addr;
			break;
		case IPPROTO_UDP:
			if (nam) {
				sin = mtod(nam, struct sockaddr_in *);
				if (nam->m_len != sizeof(*sin))
					return EINVAL;
				dst.s_addr = sin->sin_addr.s_addr;
			} else {
				dst.s_addr = inp->inp_faddr.s_addr;
			}
		}

		/* Valid Address */
		if (dst.s_addr == INADDR_ANY)
			dst.s_addr = INADDR_LOOPBACK;
	} else {
		/* Unix Domain Sockets */
		dst.s_addr = INADDR_LOOPBACK;
	}

	/*
	 *  If the destination host is unknown we can't talk to
	 *  it, if it is a non-samp host we don't do anything.  If
	 *  it's a samp host and the token mapping daemon is not 
	 *  running we can't talk to it.  Otherwise, put samp headers
	 *  on the outgoing data.
	 */
	if (!t6findhost(&dst, 0, &hsec)) {
		return ENETUNREACH;
	}
	if (hsec.hp_smm_type == T6RHDB_SMM_TSIX_1_1) {
		if (sesmgr_satmpd_started() == 0) {
			return ENETUNREACH;
		}
	} else
		return 0;

	/* Create a samp header, and prepend it the outgoing data */
	user_data_len = m_length(*data);
	if ((sm = samp_create_header(so, dst, user_data_len, 0)) == NULL) {
#ifdef DEBUG
		printf("Failed to make header\n");
#endif
		return EPERM;
	}
	m_cat(sm,*data);
	*data = sm;
	soattrs->sm_samp_seq++;

#ifdef DEBUG
        sbccverify(&so->so_snd);
#endif

	/* Done */
	return 0;
}


/**********************************************************************
 *  The following routines operate on samp headers.
 */

/*
 *  This routine validates the type and version of samp header.
 */
int
sesmgr_samp_check(t6samp_header_t * hdr)
{
	if (hdr == NULL)
		return 0;
	
	/* Check the protocol header */
	if (hdr->samp_type != T6SAMP_ID || hdr->samp_version != T6SAMP_VERSION)
		return 0;

	/* Check for minimum length */
	if (hdr->samp_length < T6SAMP_MIN_HEADER)
		return 0;
	
	/* Check the attributes type */
	if (hdr->attr_type != CIPSO_FORMAT_OPTION)
		return 0;

	return 1;
}

/*
 * match a host entry by address with reference counting
 */
/* ARGSUSED */
static int
he_match_addr (struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	host_entry_t *he = (host_entry_t *) h;
	const struct in_addr *addr = (struct in_addr *) key;

	ASSERT (he != NULL);
	ASSERT (addr != NULL);

	if (!REF_INVALID(he->he_refcnt) &&
	    he->he_host.s_addr == addr->s_addr) {
		(void) atomicAddUint (&he->he_refcnt, 1);
		return (1);
	}
	return (0);
}

/*
 * decrement reference count of a host entry
 * return non-zero if refcount indicates storage
 * should be reclaimed
 */
/* ARGSUSED */
static int
he_refcnt (struct hashbucket *h, caddr_t arg1, caddr_t arg2)
{
	host_entry_t *he = (host_entry_t *) h;
	unsigned int refcnt = atomicAddUint (&he->he_refcnt, -1);

	return (REF_CNT(refcnt) == 0);
}

/*
 * release storage allocated for a host entry
 */
static void
he_rele (struct hashbucket *h)
{
	host_entry_t *he = (host_entry_t *) h;

	if (he == NULL)
		return;

	spinlock_destroy(&he->he_lock);
	kmem_zone_free(host_tbl_zone, he);
}

/*
 * decrement reference count of a token entry
 * return non-zero if refcount indicates storage
 * should be reclaimed
 */
/* ARGSUSED */
static int
tk_refcnt (struct hashbucket *h, caddr_t arg1, caddr_t arg2)
{
	token_entry_t *tk = (token_entry_t *) h;
	unsigned int refcnt = atomicAddUint (&tk->tk_refcnt, -1);

	return (REF_CNT(refcnt) == 0);
}

/*
 * release storage allocated for a host entry
 */
static void
tk_rele (struct hashbucket *h)
{
	token_entry_t *tk = (token_entry_t *) h;

	if (tk == NULL)
		return;

	sv_destroy(&tk->tk_wait);
	spinlock_destroy(&tk->tk_lock);
	if (tk->tk_attr != NULL &&
	    tk->tk_attr_id != T6SAMP_SL &&
	    tk->tk_attr_id != T6SAMP_INTEG_LABEL)
		kmem_zone_free(attr_zone, tk->tk_attr);
	kmem_zone_free(token_tbl_zone, tk);
}

/*
 *  This routine creates a samp header for privileged processes,
 *  such as the token mapping daemon, where tokens can not be
 *  sent due to the potential of creating a token mapping deadlock.
 */
struct mbuf *
samp_create_priv_hdr(int ulen)
{
	int token_index = 0, s;
	struct mbuf *sm;
	t6samp_header_t	*hdr;
	host_entry_t *he;
	struct in_addr src;

	/*
	 * Allocate an mbuf to hold the security attributes
	 * and fill in the SAMP types and version numbers.
	 */
	if ((sm = m_getclr(M_WAIT, MT_DATA)) == NULL) {
		return NULL;
	}
	hdr = mtod(sm, t6samp_header_t *);
	hdr->samp_type = T6SAMP_ID;
	hdr->samp_version = T6SAMP_VERSION;	/* "01" */
	hdr->attr_type = CIPSO_FORMAT_OPTION;

	/*
	 * Session ID
	 */
	hdr->tokens[token_index++] = T6SAMP_PRIV_SID;
	hdr->token_mask |= T6SAMP_MASK(T6SAMP_SID);

	/*
	 * Copy generation number
	 */

	/* All tokens are created relative to our local token server */
	src.s_addr = INADDR_LOOPBACK;
	he = (host_entry_t *) hash_lookup (&host_info, he_match_addr, (caddr_t) &src, (caddr_t) NULL, (caddr_t) NULL);
	if (he == NULL) {
		m_freem(sm);
		return NULL;
	}
	s = mutex_spinlock(&he->he_lock);
	bcopy(he->he_gen, hdr->token_generation, sizeof (he->he_gen));
	mutex_spinunlock(&he->he_lock, s);
	he_rele (hash_refcnt (&host_info, he_refcnt, &he->he_hash));

	/* Update lengths */
	if (token_index == 0) {
		hdr->attr_length = T6SAMP_NULL_ATTR_LEN;
	} else  {
		hdr->token_type  = T6SAMP_TOKEN_TYPE;
		hdr->attr_length = T6SAMP_ATTR_HEADER_LEN +
			(token_index * sizeof(u_int32_t));
	}
	hdr->samp_length = T6SAMP_HEADER_LEN + hdr->attr_length + ulen;
	sm->m_len = T6SAMP_HEADER_LEN + hdr->attr_length;
	return sm;
}

static int
compare_privs (const cap_t a, const cap_t b)
{
	return (a->cap_effective != b->cap_effective ||
		a->cap_permitted != b->cap_permitted ||
		a->cap_inheritable != b->cap_inheritable);
}

static int
compare_ids (const t6ids_t *a, const t6ids_t *b)
{
	return (a->uid != b->uid || a->gid != b->gid ||
		a->groups.ngroups != b->groups.ngroups ||
		bcmp(a->groups.groups, b->groups.groups,
		     a->groups.ngroups * sizeof(*a->groups.groups)) != 0);
}

/*
 *  This routine creates a samp header using the attributes of the 
 *  current process and any endpoint defaults that are in effect.
 */
/* ARGSUSED */
struct mbuf *
samp_create_header(struct socket *so, struct in_addr dst, int ulen, int flags)
{
	int token_index = 0, s;
	int udp = (so->so_proto->pr_protocol == IPPROTO_UDP);
	struct mbuf *sm;
	t6samp_header_t	*hdr;
	struct ipsec *sp;
	soattr_t *sa_dflt, *sa_msg, *sa_snd;
	token_buf_t tb;
	token_entry_t tkin, tkout;
	host_entry_t *he;
	t6ids_t	ids;
	u_int generation = 0;
	cred_t *cr = get_current_cred();

	/* Find socket security attributes */
	sp = so->so_sesmgr_data;
	ASSERT(sp != NULL);
	sa_dflt = &sp->sm_dflt;
	sa_msg = &sp->sm_msg;
	sa_snd = &sp->sm_snd;

	/*
	 * Allocate an mbuf to hold the security attributes
	 * and fill in the SAMP types and version numbers.
	 */
	if ((sm = m_getclr(M_WAIT, MT_DATA)) == NULL) {
		return NULL;
	}
	hdr = mtod(sm, t6samp_header_t *);
	hdr->samp_type = T6SAMP_ID;
	hdr->samp_version = T6SAMP_VERSION;	/* "01" */
	hdr->attr_type = CIPSO_FORMAT_OPTION;

	/*
	 * Session ID
	 *
	 * Session ID is used to identify privileged traffic that must
	 * not be token mapped.  Send it only if it has been specified
	 * as an endpoint default.
	 *
	 * If session id is T6SAMP_PRIV_SID, do not send other attributes.
	 */
	tkin.tk_attr = NULL;
	if (sa_msg->sa_mask & T6M_SESSION_ID)
		tkin.tk_attr = (caddr_t) &sa_msg->sa_sid;
	else if (sp->sm_mask & sa_dflt->sa_mask & T6M_SESSION_ID)
		tkin.tk_attr = (caddr_t) &sa_dflt->sa_sid;
	if (tkin.tk_attr != NULL) {
		uid_t sid = *(uid_t *) tkin.tk_attr;
		if (sid == T6SAMP_PRIV_SID) {
			hdr->tokens[token_index++] = sid;
			hdr->token_mask |= T6SAMP_MASK(T6SAMP_SID);
			goto priv_header;
		}
	}

	/*
	 *  Do we need to handshake with remote host before
	 *  we map any tokens ?   Map addresses of all interfaces
	 *  on this host to the loopback address.
	 */
	if (dst.s_addr != INADDR_LOOPBACK && inaddr_to_ifp(dst) == NULL) {
		he = (host_entry_t *) hash_lookup (&host_info, he_match_addr, (caddr_t) &dst, (caddr_t) NULL, (caddr_t) NULL);
		if (he == NULL) {
			if (sesmgr_init_request(dst.s_addr, &generation) != 0 || samp_update_host (dst.s_addr, generation) != 0) {
				m_freem(sm);
				return NULL;
			}
		}
		else {
			he_rele (hash_refcnt (&host_info, he_refcnt,
					      &he->he_hash));
		}
	}

	/*
	 *  All tokens are created relative to our local
	 *  token server.
	 */
	tkin.tk_addr.s_addr = INADDR_LOOPBACK;
	he = (host_entry_t *) hash_lookup (&host_info, he_match_addr, (caddr_t) &tkin.tk_addr, (caddr_t) NULL, (caddr_t) NULL);
	if (he == NULL) {
		m_freem(sm);
		return NULL;
	}
	s = mutex_spinlock(&he->he_lock);
	bcopy(he->he_gen, hdr->token_generation, sizeof (he->he_gen));
	mutex_spinunlock(&he->he_lock, s);
	he_rele (hash_refcnt (&host_info, he_refcnt, &he->he_hash));
	
	/*
	 *  Convert the current process attributes into tokens to be sent.  
	 *  An endpoint default, if set, overrides the current process
	 *  attribute.  We don't have to send an attribute if it's value has
	 *  not changed. If no attributes have changed, we sent a null samp
	 *  header.  For UDP we send all attributes on each packet.
	 *
	 *  XXX: For this initial implementation we map tokens one at a time.
	 *	 This mapping needs to be done only once per attribute per
	 *	 token server.  If measurement shows that we are incurring
	 *	 too much overhead, we can in the future take advantage of
	 *	 the provisions of the token mapping protocol to aggregate
	 *	 requests.
	 */

	/*
	 * tkout invariants
	 */
	tkout.tk_attr = (caddr_t) &tb;

	/*
	 * Sensitivity Label
	 */
	if (sa_msg->sa_mask & T6M_SL) {
		tkin.tk_attr = (caddr_t) sa_msg->sa_msen;
	} else if (sp->sm_mask & sa_dflt->sa_mask & T6M_SL) {
		tkin.tk_attr = (caddr_t) sa_dflt->sa_msen;
	} else {
		tkin.tk_attr = (caddr_t) msen_from_mac(cr->cr_mac);
	}
	tkin.tk_attr_len = sizeof(*sa_msg->sa_msen);
	if (!samp_token_from_attr(T6SAMP_SL, &tkin, &tkout)) {
		msen_t slbl = (msen_t) tkout.tk_attr;
		if (udp || (sa_snd->sa_mask & T6SAMP_MASK(T6SAMP_SL)) == T6M_NO_ATTRS || slbl != sa_snd->sa_msen) {
			hdr->tokens[token_index++] = tkout.tk_token;
			hdr->token_mask |= T6SAMP_MASK(T6SAMP_SL);
			sa_snd->sa_mask |= T6SAMP_MASK(T6SAMP_SL);
			sa_snd->sa_msen = slbl;
		}
	}
	tkout.tk_attr = (caddr_t) &tb;

	/*
	 * Integrity Label
	 */
	if (sa_msg->sa_mask & T6M_INTEG_LABEL) {
		tkin.tk_attr = (caddr_t) sa_msg->sa_mint;
	} else if (sp->sm_mask & sa_dflt->sa_mask & T6M_INTEG_LABEL) {
		tkin.tk_attr = (caddr_t) sa_dflt->sa_mint;
	} else {
		tkin.tk_attr = (caddr_t) mint_from_mac(cr->cr_mac);
	}
	tkin.tk_attr_len = sizeof(*sa_msg->sa_mint);
	if (!samp_token_from_attr(T6SAMP_INTEG_LABEL, &tkin, &tkout)) {
		mint_t ilbl = (mint_t) tkout.tk_attr;
		if (udp || (sa_snd->sa_mask & T6SAMP_MASK(T6SAMP_INTEG_LABEL)) == T6M_NO_ATTRS || ilbl != sa_snd->sa_mint) {
			hdr->tokens[token_index++] = tkout.tk_token;
			hdr->token_mask |= T6SAMP_MASK(T6SAMP_INTEG_LABEL);
			sa_snd->sa_mask |= T6SAMP_MASK(T6SAMP_INTEG_LABEL);
			sa_snd->sa_mint = ilbl;
		}
	}
	tkout.tk_attr = (caddr_t) &tb;

	/*
	 * Capabilities
	 */
	if (sa_msg->sa_mask & T6M_PRIVILEGES) {
		tkin.tk_attr = (caddr_t) &sa_msg->sa_privs;
	} else if (sp->sm_mask & sa_dflt->sa_mask & T6M_PRIVILEGES) {
		tkin.tk_attr = (caddr_t) &sa_dflt->sa_privs;
	} else {
		tkin.tk_attr = (caddr_t) &cr->cr_cap;
	}
	tkin.tk_attr_len = sizeof(sa_msg->sa_privs);
	if (!samp_token_from_attr(T6SAMP_PRIVS, &tkin, &tkout)) {
		cap_t privs = (cap_t) tkout.tk_attr;
		if (udp || (sa_snd->sa_mask & T6SAMP_MASK(T6SAMP_PRIVS)) == T6M_NO_ATTRS || compare_privs(privs, &sa_snd->sa_privs)) {
			hdr->tokens[token_index++] = tkout.tk_token;
			hdr->token_mask |= T6SAMP_MASK(T6SAMP_PRIVS);
			sa_snd->sa_mask |= T6SAMP_MASK(T6SAMP_PRIVS);
			sa_snd->sa_privs = *privs;
		}
	}

	/*
	 * Audit ID
	 */
	if (sa_msg->sa_mask & T6M_AUDIT_ID) {
		tkin.tk_attr = (caddr_t) &sa_msg->sa_audit_id;
	} else if (sp->sm_mask & sa_dflt->sa_mask & T6M_AUDIT_ID) {
		tkin.tk_attr = (caddr_t) &sa_dflt->sa_audit_id;
	} else {
		tkin.tk_attr = (caddr_t) &cr->cr_uid;
	}
	tkin.tk_attr_len = sizeof(sa_msg->sa_audit_id);
	if (!samp_token_from_attr(T6SAMP_LUID, &tkin, &tkout)) {
		uid_t *luid = (uid_t *) tkout.tk_attr;
		if (udp || (sa_snd->sa_mask & T6SAMP_MASK(T6SAMP_LUID)) == T6M_NO_ATTRS || *luid != sa_snd->sa_audit_id) {
			hdr->tokens[token_index++] = tkout.tk_token;
			hdr->token_mask |= T6SAMP_MASK(T6SAMP_LUID);
			sa_snd->sa_mask |= T6SAMP_MASK(T6SAMP_LUID);
			sa_snd->sa_audit_id = *luid;
		}
	}

	/*
	 * SAMP IDS.
	 *
	 * Note: the samp ids type is a composite of uid, gid, and
	 * group membership list.
	 */
	if (sa_msg->sa_mask & T6M_UID)
		ids.uid = sa_msg->sa_ids.uid;
	else if (sp->sm_mask & sa_dflt->sa_mask & T6M_UID)
		ids.uid = sa_dflt->sa_ids.uid;
	else
		ids.uid = cr->cr_uid;

	if (sa_msg->sa_mask & T6M_GID)
		ids.gid = sa_msg->sa_ids.gid;
	else if (sp->sm_mask & sa_dflt->sa_mask & T6M_GID)
		ids.gid = sa_dflt->sa_ids.gid;
	else
		ids.gid = cr->cr_gid;

	if (sa_msg->sa_mask & T6M_GROUPS) {
		ids.groups.ngroups = sa_msg->sa_ids.groups.ngroups;
		bcopy((caddr_t) sa_msg->sa_ids.groups.groups,
		      (caddr_t) ids.groups.groups,
		      ids.groups.ngroups * sizeof (gid_t));
	} else if (sp->sm_mask & sa_dflt->sa_mask & T6M_GROUPS) {
		ids.groups.ngroups = sa_dflt->sa_ids.groups.ngroups;
		bcopy((caddr_t) sa_dflt->sa_ids.groups.groups,
		      (caddr_t) ids.groups.groups,
		      ids.groups.ngroups * sizeof (gid_t));
	} else {
		ids.groups.ngroups = cr->cr_ngroups;
		bcopy((caddr_t) cr->cr_groups,
		      (caddr_t) ids.groups.groups, 
		      (ids.groups.ngroups * sizeof (gid_t)));
	}

	tkin.tk_attr = (caddr_t) &ids;
	tkin.tk_attr_len = sizeof(sa_msg->sa_ids);
	if (!samp_token_from_attr(T6SAMP_IDS, &tkin, &tkout)) {
		t6ids_t *idp = (t6ids_t *) tkout.tk_attr;
		if (udp || (sa_snd->sa_mask & T6SAMP_MASK(T6SAMP_IDS)) == T6M_NO_ATTRS || compare_ids(idp, &sa_snd->sa_ids)) {
			hdr->tokens[token_index++] = tkout.tk_token;
			hdr->token_mask |= T6SAMP_MASK(T6SAMP_IDS);
			sa_snd->sa_mask |= T6SAMP_MASK(T6SAMP_IDS);
			sa_snd->sa_ids = *idp;
		}
	}

priv_header:

	/*
	 * Update lengths.
	 */
	if (token_index == 0) {
		hdr->attr_length = T6SAMP_NULL_ATTR_LEN;
	} else  {
		hdr->token_type  = T6SAMP_TOKEN_TYPE;
		hdr->attr_length = T6SAMP_ATTR_HEADER_LEN +
			(token_index * sizeof(u_int32_t));
	}
	hdr->samp_length = T6SAMP_HEADER_LEN + hdr->attr_length + ulen;
	sm->m_len = T6SAMP_HEADER_LEN + hdr->attr_length;
	if (udp)
		sa_snd->sa_mask = T6M_NO_ATTRS;
	return sm;
}

/*
 *  This routine takes a received samp header, converts the tokens
 *  to attributes, and enforces the relevant security policy.
 */  
int
samp_get_attrs(struct socket *so, struct in_addr addr, t6samp_header_t *hdr)
{
	int i, rv, s;
	int token_index;
	struct ipsec *soattrs;
	soattr_t *sa;
	u_int generation = 0;
	token_buf_t tb;
	token_entry_t tkin, tkout;
	host_entry_t *he;
	mac_t samp_lbl;

	/* Get pointer to Sesmgr data area */
	soattrs = so->so_sesmgr_data;
	ASSERT(soattrs != NULL);
	sa = &soattrs->sm_rcv;

	/*
	 *  Check for privileged traffic.
	 *  XXX: need check that receiver is privileged.
	 */
	token_index = 0;
	for (i = 0; i < T6_MAX_ATTRS; i++) {
		if ((hdr->token_mask & T6SAMP_MASK(i)) == T6M_NO_ATTRS)
			continue;
		if (i == T6SAMP_SID) {
			if (hdr->tokens[token_index] == T6SAMP_PRIV_SID)
				return 0;
			break;
		}
		token_index++;
	}

	/*
	 *  Check for null samp header.  
	 *  XXX gl A null samp header is only valid for tcp and only after
	 *  the initial connection has been made.
	 */
	if (hdr->attr_length == T6SAMP_NULL_ATTR_LEN) {
		return 0;
	}

        /*  Local Host */
        if (addr.s_addr == INADDR_LOOPBACK || inaddr_to_ifp(addr) != NULL)
                tkin.tk_addr.s_addr = INADDR_LOOPBACK;
	else
		tkin.tk_addr.s_addr = addr.s_addr;

	he = (host_entry_t *) hash_lookup (&host_info, he_match_addr, (caddr_t) &tkin.tk_addr, (caddr_t) NULL, (caddr_t) NULL);
	if (he != NULL) {
		s = mutex_spinlock(&he->he_lock);
		rv = bcmp (he->he_gen, hdr->token_generation,
			   sizeof (he->he_gen));
		mutex_spinunlock(&he->he_lock, s);
		he_rele (hash_refcnt (&host_info, he_refcnt, &he->he_hash));
	}
	else
		rv = 1;
	if (rv) {
		if (sesmgr_init_request(tkin.tk_addr.s_addr, &generation) != 0 || samp_update_host (tkin.tk_addr.s_addr, generation) != 0)
			return ECONNRESET;
	}

	/*
	 *  Calculate the number of tokens present by taking the
	 *  attribute length field in the header, subtracting the
	 *  12 bytes of non-token fields and dividing the result
	 *  by 4/bytes per token.
	 */
	ASSERT((hdr->attr_length % 4) == 0);

	/*
	 *  Scan through the recieved tokens translating them
	 *  into local attributes.  
	 *
	 *  XXX: For this initial implementation we map tokens one at a time.
	 *	 This mapping needs to be done only once per attribute per
	 *	 token server.  If measurement shows that we are incurring
	 *	 too much overhead, we can in the future take advantage of
	 *	 the provisions of the token mapping protocol to aggregate
	 *	 requests.
	 */
	tkin.tk_gen = 0;
	bcopy(hdr->token_generation, (caddr_t)&tkin.tk_gen+1,
	      sizeof(hdr->token_generation));
	tkout.tk_attr = (caddr_t) &tb;
	token_index = 0;
	for (i = 0; i < T6_MAX_ATTRS; i++) {

		if ((hdr->token_mask & T6SAMP_MASK(i)) == T6M_NO_ATTRS)
			continue;

		tkin.tk_token = hdr->tokens[token_index++];

		switch(i) {
		case T6SAMP_SL:
			if (samp_attr_from_token(i, &tkin, &tkout))
				break;

			sa->sa_mask |= T6M_SL;
			sa->sa_msen = (msen_t) tkout.tk_attr;
			tkout.tk_attr = (caddr_t) &tb;
			break;

		case T6SAMP_INTEG_LABEL:
			if (samp_attr_from_token(i, &tkin, &tkout))
				break;

			sa->sa_mask |= T6M_INTEG_LABEL;
			sa->sa_mint = (mint_t) tkout.tk_attr;
			tkout.tk_attr = (caddr_t) &tb;
			break;

		case T6SAMP_SID:
			if (samp_attr_from_token(i, &tkin, &tkout))
				break;

			sa->sa_mask |= T6M_SESSION_ID;
			sa->sa_sid = *(uid_t *) tkout.tk_attr;
			break;

		case T6SAMP_PRIVS:
			if (samp_attr_from_token(i, &tkin, &tkout))
				break;

			sa->sa_mask |= T6M_PRIVILEGES;
			sa->sa_privs = *(cap_t) tkout.tk_attr;
			break;

		case T6SAMP_LUID:
			if (samp_attr_from_token(i, &tkin, &tkout))
				break;

			sa->sa_mask |= T6M_AUDIT_ID;
			sa->sa_audit_id = *(uid_t *) tkout.tk_attr;
			break;

		case T6SAMP_IDS:
			if (samp_attr_from_token(i, &tkin, &tkout))
				break;

			sa->sa_mask |= (T6M_UID | T6M_GID | T6M_GROUPS);
			sa->sa_ids = *(t6ids_t *) tkout.tk_attr;
			break;

		default:
			/* Unsupported Attribute */
			break;
		}
	}

	/*
	 *  Verify that all required attributes are present.  If any
	 *  are missing, we can substitute defaults at the host level,
	 *  or at the interface level.
	 */
	if ((sa->sa_mask & T6M_SL) == 0 ||
	    (sa->sa_mask & T6M_INTEG_LABEL) == 0 ||
	    (sa->sa_mask & T6M_UID) == 0)
		return (EPERM);

	/*
	 * Enforce MAC policy
	 */
	if (soattrs->sm_cap_net_mgt == 1)
		return (0);

	if (!msen_valid(sa->sa_msen) || !mint_valid(sa->sa_mint))
		return (EPERM);

	samp_lbl = mac_from_msen_mint(sa->sa_msen, sa->sa_mint);
	if (mac_invalid(samp_lbl) ||
	    !mac_equ(get_current_cred()->cr_mac, samp_lbl))
		return (EPERM);

	return (0);
}

/**********************************************************************
 *  The following routines convert between token and attributes, and
 *  maintain the token hash tables.
 */

/*
 *   Token hash tables.  Token_tables is an array of pointers,
 *   indexed by samp attribute id, which points to a hash table
 *   for that attribute.  The attribute hash table is accessed
 *   by hashing on IP address.  Collisions are kept in 
 *   a hash chain off that entry.
 */
static unsigned int mapping_id = 0;

int
samp_update_host (u_int hostid, u_int generation)
{
	host_entry_t *he;
	struct in_addr addr;

	/* initialize host identity */
	addr.s_addr = hostid;

	/* find current entry, if it exists */
	he = (host_entry_t *) hash_lookup (&host_info, he_match_addr, (caddr_t) &addr, (caddr_t) NULL, (caddr_t) NULL);
	if (he == NULL)
	{
		he = kmem_zone_alloc(host_tbl_zone, KM_NOSLEEP);
		if (he == NULL)
			return (ENOMEM);
		he->he_host = addr;
		he->he_refcnt = 1;
		bcopy ((char *) &generation + 1, he->he_gen,
		       sizeof (he->he_gen));
		spinlock_init(&he->he_lock, "samp host entry lock");
		hash_insert (&host_info, &he->he_hash, (caddr_t) &addr);
	}
	else {
		int s = mutex_spinlock(&he->he_lock);
		bcopy ((char *) &generation + 1, he->he_gen,
		       sizeof (he->he_gen));
		mutex_spinunlock(&he->he_lock, s);
		he_rele (hash_refcnt (&host_info, he_refcnt, &he->he_hash));
	}

	/* AOK */
	return (0);
}

/*
 * match token entry by mapping identifier
 */
/* ARGSUSED */
static int
tk_match_mid (struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	token_entry_t *atk = (token_entry_t *) h;
	const token_entry_t *btk = (token_entry_t *) arg1;

	ASSERT (atk != NULL);
	ASSERT (btk != NULL);

	if (!REF_INVALID(atk->tk_refcnt) &&
	    atk->tk_addr.s_addr == btk->tk_addr.s_addr &&
	    atk->tk_mid == btk->tk_mid) {
		(void) atomicAddUint (&atk->tk_refcnt, 1);
		return (1);
	}
	return (0);
}

/*
 * match token entry by token
 */
/* ARGSUSED */
static int
tk_match_token (struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	token_entry_t *atk = (token_entry_t *) h;
	const token_entry_t *btk = (token_entry_t *) arg1;

	ASSERT (atk != NULL);
	ASSERT (btk != NULL);

	if (!REF_INVALID(atk->tk_refcnt) &&
	    atk->tk_addr.s_addr == btk->tk_addr.s_addr &&
	    atk->tk_token == btk->tk_token) {
		(void) atomicAddUint (&atk->tk_refcnt, 1);
		return (1);
	}
	return (0);
}

/*
 *  Compare two attributes.  If the attributes are msen or mint
 *  labels, then they are pointers to the system label lists and
 *  we can compare the pointers for equality.  For all other
 *  attributes we must do a byte comparison.
 */
static int
samp_attr_equal(const token_entry_t *atk, const token_entry_t *btk)
{
	int attr_id = atk->tk_attr_id;

	if (atk->tk_attr == NULL || btk->tk_attr == NULL)
		return (0);
	if (attr_id == T6SAMP_SL || attr_id == T6SAMP_INTEG_LABEL) {
		return (atk->tk_attr == btk->tk_attr);
	} else if (attr_id == T6SAMP_PRIVS) {
		cap_t a = (cap_t) atk->tk_attr;
		cap_t b = (cap_t) btk->tk_attr;
		return (compare_privs (a, b) == 0);
	} else if (attr_id == T6SAMP_IDS) {
		t6ids_t *a = (t6ids_t *) atk->tk_attr;
		t6ids_t *b = (t6ids_t *) btk->tk_attr;
		return (compare_ids(a, b) == 0);
	} else if (attr_id == T6SAMP_SID || attr_id == T6SAMP_LUID) {
		uid_t *a = (uid_t *) atk->tk_attr;
		uid_t *b = (uid_t *) btk->tk_attr;
		return (*a == *b);
	}
	return 0;
}

/*
 * match token entry by attribute
 */
/* ARGSUSED */
static int
tk_match_attr (struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	token_entry_t *atk = (token_entry_t *) h;
	const token_entry_t *btk = (token_entry_t *) arg1;

	ASSERT (atk != NULL);
	ASSERT (btk != NULL);

	if (!REF_INVALID(atk->tk_refcnt) &&
	    atk->tk_addr.s_addr == btk->tk_addr.s_addr &&
	    samp_attr_equal(atk, btk) != 0) {
		(void) atomicAddUint (&atk->tk_refcnt, 1);
		return (1);
	}
	return (0);
}

static token_entry_t *
samp_find (int attr_id, token_entry_t *tk, struct hashinfo **hi,
	   int (*mf) (struct hashbucket *, caddr_t, caddr_t, caddr_t))
{
	ASSERT (tk != NULL);
	ASSERT (hi != NULL);
	ASSERT (mf != NULL);

	/* locate hashinfo structure for this attribute */
	*hi = token_tbls[attr_id];
	if (*hi == NULL)
		return (NULL);

	/* locate entry */
	return ((token_entry_t *) hash_lookup (*hi, mf, (caddr_t) &tk->tk_addr,
					       (caddr_t) tk, (caddr_t) NULL));
}

/*
 *  Given a token, return the token table entry that contains the
 *  corresponding attributes.  If no entry exists, make a pending
 *  entry, notify the token mapping daemon, and sleep on the sync
 *  varaible.  The token mapping daemon will wake us up when it
 *  fill in the value.
 *
 *  Note: if we find a token entry with an invalid generation number
 *  we request that it be mapped.  The token daemon will then detect
 *  the stale generation, flush the cache, and re-handshake with the
 *  remote token mapping daemon.  It may be helpful to add a thread
 *  through the table on IP Address.
 *
 *  If the local_mapping_only flag is set, not finding an entry
 *  is a fatal error.
 */
static int
samp_attr_from_token(int attr_id, token_entry_t *tkin, token_entry_t *tkout)
{
	token_entry_t *tk;
	satmp_token_list tk_list;
	int error = 0, s;
	struct hashinfo *hi;

	ASSERT (tkin != NULL);
	ASSERT (tkout != NULL);

	/* Look for existing entry */
	tk = samp_find(attr_id, tkin, &hi, tk_match_token);
	if (tk != NULL && tk->tk_gen == tkin->tk_gen) {
		/* lock it */
		s = mutex_spinlock(&tk->tk_lock);

		/* If entry is pending, wait for it */
		/* XXX gl we may want to allow signals here */
		while (REF_WAIT(tk->tk_refcnt)) {
			sv_wait(&tk->tk_wait, PZERO, &tk->tk_lock, s);
			s = mutex_spinlock(&tk->tk_lock);
		}

		/* If entry is valid, return it */
		if (REF_VALID(tk->tk_refcnt)) {
			if (attr_id == T6SAMP_SL ||
			    attr_id == T6SAMP_INTEG_LABEL)
				tkout->tk_attr = tk->tk_attr;
			else
				bcopy(tk->tk_attr, tkout->tk_attr,
				      tk->tk_attr_len);
		}
		else
			error = 1;

		goto release;
	}

	/*
	 *  Either no entry exists, or the generation number has
	 *  expired. Request that the token be mapped.
	 */

	/* initialize token map request structure */
	tk_list.hostid = tkin->tk_addr.s_addr;
	tk_list.generation = tkin->tk_gen;
	tk_list.mask = T6SAMP_MASK(attr_id);
	tk_list.lrtok_list[0].mid = atomicAddUint(&mapping_id, 1);
	tk_list.lrtok_list[0].token = tkin->tk_token;

	/* Create new entry */
	if (tk == NULL) {
		tk = kmem_zone_alloc(token_tbl_zone, KM_NOSLEEP);
		if (tk == NULL)
			return (1);

		/* initialize new token entry */
		tk->tk_addr.s_addr = tkin->tk_addr.s_addr;
		sv_init(&tk->tk_wait, SV_DEFAULT, "token entry sync variable");
		tk->tk_token = tkin->tk_token;
		tk->tk_gen = tkin->tk_gen;
		tk->tk_mid = tk_list.lrtok_list[0].mid;
		tk->tk_attr_id = attr_id;
		tk->tk_attr_len = 0;
		tk->tk_attr = NULL;
		tk->tk_refcnt = REF_CNT_WAIT | 2;
		spinlock_init(&tk->tk_lock, "token entry lock");

		/* lock token entry */
		s = mutex_spinlock(&tk->tk_lock);

		/* add to token table */
		hash_insert (hi, &tk->tk_hash, (caddr_t) &tkin->tk_addr);
	} else {
		/* lock token entry */
		s = mutex_spinlock(&tk->tk_lock);

		/* fill in existing token entry */
		(void) atomicSetUint(&tk->tk_refcnt, REF_CNT_WAIT);
		tk->tk_mid = tk_list.lrtok_list[0].mid;
		tk->tk_gen = tkin->tk_gen;
	}


	/* Request Mapping of token to local representation */	
	if (sesmgr_get_attr_request(&tk_list) != 0) {
		error = 1;
		goto release;
	}

	/* Wait for response */
	while (REF_WAIT(tk->tk_refcnt)) {
		sv_wait (&tk->tk_wait, PZERO, &tk->tk_lock, s);
		s = mutex_spinlock(&tk->tk_lock);
	}

	if (REF_VALID(tk->tk_refcnt)) {
		if (attr_id == T6SAMP_SL || attr_id == T6SAMP_INTEG_LABEL)
			tkout->tk_attr = tk->tk_attr;
		else
			bcopy(tk->tk_attr, tkout->tk_attr, tk->tk_attr_len);
	}
	else
		error = 1;

release:
	mutex_spinunlock(&tk->tk_lock, s);
	tk_rele (hash_refcnt (hi, tk_refcnt, &tk->tk_hash));
	return error;
}

/*
 *  Given a local attribute, find the token table entry that contains
 *  the corresponding token number.  If no entry exists, make a pending
 *  entry, notify the token mapping daemon, and sleep on the sync
 *  variable.  The token mapping daemon will wake us up when it fills in
 *  the value in the table.
 *
 *  if the local_mapping_only flag is set and a new entry needs to be made
 *  just fill in the next sequential token number.
 */
static int
samp_token_from_attr(int attr_id, token_entry_t *tkin, token_entry_t *tkout)
{
	token_entry_t *tk;
	satmp_lrep_list attr_list;
	int error = 0, s;
	struct hashinfo *hi;

	ASSERT (tkin != NULL);
	ASSERT (tkout != NULL);

	/* Is there an existing entry */
	tk = samp_find(attr_id, tkin, &hi, tk_match_attr);
	if (tk != NULL) {
		/* lock it */
		s = mutex_spinlock(&tk->tk_lock);

		/* If entry is pending, wait for it */
		while (REF_WAIT(tk->tk_refcnt)) {
			if (sv_wait_sig(&tk->tk_wait, PZERO, &tk->tk_lock, s))
			{
				error = 1;
				goto release2;
			}
			s = mutex_spinlock(&tk->tk_lock);
		}

		/* If entry is valid, return it */
		if (REF_VALID(tk->tk_refcnt)) {
			if (attr_id == T6SAMP_SL ||
			    attr_id == T6SAMP_INTEG_LABEL)
				tkout->tk_attr = tk->tk_attr;
			else
				bcopy(tk->tk_attr, tkout->tk_attr,
				      tk->tk_attr_len);
			tkout->tk_token = tk->tk_token;
		}
		else
			error = 1;

		goto release;
	}

	/* Create new entry */
	tk = kmem_zone_alloc(token_tbl_zone, KM_NOSLEEP);
	if (tk == NULL)
		return (1);

	/* initialize new token entry */
	tk->tk_addr.s_addr = tkin->tk_addr.s_addr;
	sv_init(&tk->tk_wait, SV_DEFAULT, "token entry sync variable");
	tk->tk_token = 0;
	tk->tk_gen = 0;
	tk->tk_mid = atomicAddUint(&mapping_id, 1);
	tk->tk_attr_id = attr_id;
	tk->tk_refcnt = REF_CNT_WAIT | 2;
	spinlock_init(&tk->tk_lock, "token entry lock");

	/* For msen and mint labels we use the pointer to the
	 * system label list, for all other attributes we have to
	 * make our own copy.
	 */
	if (attr_id == T6SAMP_SL || attr_id == T6SAMP_INTEG_LABEL) {
		tk->tk_attr = tkin->tk_attr;
	} else {
		tk->tk_attr = kmem_zone_alloc(attr_zone, KM_SLEEP);
		if (tk->tk_attr == NULL) {
			sv_destroy(&tk->tk_wait);
			spinlock_destroy(&tk->tk_lock);
			kmem_zone_free(token_tbl_zone, tk);
			return (1);
		}
		bcopy(tkin->tk_attr, tk->tk_attr, tkin->tk_attr_len);
	}
	tk->tk_attr_len = tkin->tk_attr_len;
		
	/* copy pertinent info to token map request structure */
	attr_list.hostid = tk->tk_addr.s_addr;
	attr_list.generation = 0;
	attr_list.mask = T6SAMP_MASK(tk->tk_attr_id);
	attr_list.lr_list[0].mid = tk->tk_mid;
	attr_list.lr_list[0].lr = tk->tk_attr;
	attr_list.lr_list[0].lr_len = tk->tk_attr_len;

	/* lock token entry */
	s = mutex_spinlock(&tk->tk_lock);

	/* add to token table */
	hash_insert (hi, &tk->tk_hash, (caddr_t) &tkin->tk_addr);

	/* request mapping of local representation to token */
	if (sesmgr_get_lrtok_request(&attr_list) != 0) {
		error = 1;
		goto release;
	}

	/* Wait for entry to be filled in */
	while (REF_WAIT(tk->tk_refcnt)) {
		if (sv_wait_sig(&tk->tk_wait, PZERO, &tk->tk_lock, s)) {
			error = 1;
			goto release2;
		}
		s = mutex_spinlock(&tk->tk_lock);
	}

	if (REF_VALID(tk->tk_refcnt)) {
		if (attr_id == T6SAMP_SL || attr_id == T6SAMP_INTEG_LABEL)
			tkout->tk_attr = tk->tk_attr;
		else
			bcopy(tk->tk_attr, tkout->tk_attr, tk->tk_attr_len);
		tkout->tk_token = tk->tk_token;
	}
	else
		error = 1;

release:
	mutex_spinunlock(&tk->tk_lock, s);
release2:
	tk_rele (hash_refcnt (hi, tk_refcnt, &tk->tk_hash));
	return error;
}

void
samp_update_token(get_lrtok_reply *token_list)
{
	int i, s;
	token_entry_t *tk, tkin;
	get_lrtok_reply_token_spec *ap;
	struct hashinfo *hi;

	tkin.tk_addr.s_addr = token_list->hostid;
	for (i = 0, ap = token_list->token_spec;
	     i < token_list->token_spec_count; i++, ap++) {
		ASSERT(ap != NULL);

		/* Find the entry */
		tkin.tk_mid = ap->mid;
		tk = samp_find(ap->attribute, &tkin, &hi, tk_match_mid);
		if (tk == NULL) {
#ifdef DEBUG
			printf("\\samp_update_token: failed to find mid %d\n",
			       ap->mid);
#endif
			continue;
		}

		if (!REF_WAIT(tk->tk_refcnt)) {
			tk_rele (hash_refcnt (hi, tk_refcnt, &tk->tk_hash));
#ifdef DEBUG
			printf("\\samp_update_token: token entry not in wait 0x%08x\n", tk);
#endif
			continue;
		}

		/* acquire object lock */
		s = mutex_spinlock(&tk->tk_lock);

		if (ap->status != SATMP_REPLY_OK) {
			/* mark invalid */
			(void) atomicSetUint (&tk->tk_refcnt, REF_CNT_INVALID);
			(void) atomicAddUint (&tk->tk_refcnt, -1);

			/* Wake up anybody waiting */
			sv_broadcast(&tk->tk_wait);

			/* release object lock */
			mutex_spinunlock(&tk->tk_lock, s);
#ifdef DEBUG
			printf("\\samp_update_token: invalid entry 0x%08x\n",
			       tk);
#endif
		} else {
			/* Update Entry */
			tk->tk_token = ap->token;
			tk->tk_gen = token_list->generation;
			(void) atomicClearUint (&tk->tk_refcnt,
						REF_CNT_WAIT);

			/* Wake up anybody waiting */
			sv_broadcast(&tk->tk_wait);

			/* release object lock */
			mutex_spinunlock(&tk->tk_lock, s);
		}

		tk_rele (hash_refcnt (hi, tk_refcnt, &tk->tk_hash));
	}
}

/*
 *  Find the token table entry for this <host, attr, token> and
 *  update the entry with the corresponding attribute value.  
 */
void
samp_update_attr(get_attr_reply *attr_list)
{
	int i, s;
	token_entry_t *tk, tkin;
	get_attr_reply_attr_spec *ap;
	struct hashinfo *hi;

	tkin.tk_addr.s_addr = attr_list->hostid;
	for (i = 0, ap = attr_list->attr_spec; i < attr_list->attr_spec_count;
	     i++, ap++) {
		ASSERT(ap != NULL);

		/* Find the entry */
		tkin.tk_token = ap->token;
		tk = samp_find(ap->attribute, &tkin, &hi, tk_match_token);
		if (tk == NULL) {
#ifdef DEBUG
			printf("\\samp_update_attr: failed to find token\n");
#endif
			continue;
		}

		/* check for valid state */
		if (!REF_WAIT(tk->tk_refcnt)) {
			tk_rele (hash_refcnt (hi, tk_refcnt, &tk->tk_hash));
#ifdef DEBUG
			printf("\\samp_update_attr: token entry not in wait 0x%08x\n", tk);
#endif
			continue;
		}

		/* acquire object lock */
		s = mutex_spinlock(&tk->tk_lock);

		if (ap->status != SATMP_REPLY_OK) {
			/* mark invalid */
			(void) atomicSetUint (&tk->tk_refcnt, REF_CNT_INVALID);
			(void) atomicAddUint (&tk->tk_refcnt, -1);

			/* Wake up anybody waiting */
			sv_broadcast(&tk->tk_wait);

			/* release object lock */
			mutex_spinunlock(&tk->tk_lock, s);

#ifdef DEBUG
			printf("\\samp_update_attr: invalid entry 0x%08x\n",
			       tk);
#endif
		} else {
			/* Update Entry */
			tk->tk_gen = attr_list->generation;
			tk->tk_attr_len = ap->nr_length;
			if (ap->attribute == T6SAMP_SL)
				tk->tk_attr = (caddr_t) msen_add_label((msen_t) ap->nr);
			else if (ap->attribute == T6SAMP_INTEG_LABEL) {
				tk->tk_attr = (caddr_t) mint_add_label((mint_t) ap->nr);
			} else {
				if (tk->tk_attr == NULL)
					tk->tk_attr = kmem_zone_alloc(attr_zone, KM_SLEEP);
				bcopy((caddr_t) ap->nr, tk->tk_attr,
				      ap->nr_length);
			}

			(void) atomicClearUint (&tk->tk_refcnt,
						REF_CNT_WAIT);

			/* Wake up anybody waiting */
			sv_broadcast(&tk->tk_wait);

			/* release object lock */
			mutex_spinunlock(&tk->tk_lock, s);
		}

		tk_rele (hash_refcnt (hi, tk_refcnt, &tk->tk_hash));
	}
}
