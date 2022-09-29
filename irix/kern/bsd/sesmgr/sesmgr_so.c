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

/*
 *  Routines to maintain security attributes on sockets.
 */

#ident	"$Revision: 1.7 $"
#include <bstring.h>
#include <sys/types.h> 
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/tcp-param.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/cred.h>

#include <sys/sat.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/sesmgr.h>
#include <sys/capability.h>
#include "sm_private.h"

#ifdef DEBUG
static void sesmgr_socket_check_attrs(struct socket *);
#endif

static zone_t *ipsec_zone;

/*
 * initialize zone
 */
void
sesmgr_soattr_init(void)
{
	ipsec_zone = kmem_zone_init(sizeof(ipsec_t), "ipsec zone");
}

/*
 *  Allocate space for socket attributes.
 */
struct ipsec *
sesmgr_soattr_alloc(int wait_flag)
{
	struct ipsec *addr;
	int flags;

	flags = (wait_flag == M_WAIT) ? KM_SLEEP : KM_NOSLEEP;

	addr = kmem_zone_alloc(ipsec_zone, flags);
	if (addr == NULL)
		return NULL;
	bzero(addr, sizeof(*addr));
	return addr;
}

/*
 *  Free socket attribute structure.
 */
void
sesmgr_soattr_free(struct ipsec *addr)
{
	kmem_zone_free(ipsec_zone, addr);
}

struct ipsec *
sesmgr_nfs_soattr_copy(struct ipsec *in)
{
	struct ipsec *out;

	ASSERT(in != NULL);
	out = sesmgr_soattr_alloc(M_WAIT);

	ASSERT(out != NULL);
	out->sm_ip_flags = IPSOPT_IPOUT | IPSOPT_CIPSO;
	out->sm_ip_lbl = in->sm_ip_lbl;
	out->sm_ip_uid = in->sm_ip_uid;

	return (out);
}

static void
soattr_init(soattr_t *sa)
{
	sa->sa_mask = T6M_NO_ATTRS;
	sa->sa_msen = NULL;
	sa->sa_mint = NULL;
	sa->sa_sid = 0;
	sa->sa_clearance = NULL;
	bzero(&sa->sa_privs, sizeof(sa->sa_privs));
	sa->sa_audit_id = 0;
	bzero(&sa->sa_ids, sizeof(sa->sa_ids));
}

/*
 *  Allocate a data area for session manager security
 *  attributes and initialize them appropriately for
 *  the process.
 */
void
sesmgr_socket_init_attrs(struct socket *so, cred_t *cred)
{
	struct ipsec *sp;
	soattr_t *sa;

	ASSERT(so->so_sesmgr_data == NULL);
	so->so_sesmgr_data = sp = sesmgr_soattr_alloc(M_WAIT);
	sa = &sp->sm_rcv;

	soattr_init(&sp->sm_msg);
	soattr_init(&sp->sm_dflt);
	soattr_init(&sp->sm_rcv);
	soattr_init(&sp->sm_snd);

	sp->sm_mask = T6M_NO_ATTRS;
	sp->sm_protocol_id = 0x1234;
	sp->sm_samp_cnt = 0;
	sp->sm_samp_seq = 0;

	/* Cipso Compatibility Attributes */
	sp->sm_sendlabel = sp->sm_label = cred->cr_mac;
	sp->sm_rcvuid = cred->cr_uid;
	sp->sm_uid = cred->cr_uid;

	/* Priv Process? */
	sp->sm_cap_net_mgt = 0;

	/* Connection Data */
	sp->sm_conn_data = NULL;

	/*
	 * Initialize TSIX session manager fields.
	 *
  	 * XXX: Fields that are set to null are represent attributes
	 * that are not yet implemented.
	 */
	sa->sa_mask = (T6M_SL | T6M_INTEG_LABEL | T6M_SESSION_ID | T6M_PRIVILEGES | T6M_AUDIT_ID | T6M_UID | T6M_GID | T6M_GROUPS);
	sa->sa_msen = msen_from_mac(cred->cr_mac);
	sa->sa_mint = mint_from_mac(cred->cr_mac);
	sa->sa_sid = cred->cr_uid;
	bcopy(&cred->cr_cap, &sa->sa_privs, sizeof(sa->sa_privs));
	sa->sa_audit_id = cred->cr_uid;
	sa->sa_ids.uid = cred->cr_uid;
	sa->sa_ids.gid = cred->cr_gid;
	sa->sa_ids.groups.ngroups = cred->cr_ngroups;
	bcopy(&cred->cr_groups, sa->sa_ids.groups.groups,
	      sizeof(gid_t) * sa->sa_ids.groups.ngroups);
}

/*
 *  Free any security attribute structures associated with
 *  a socket.  Note that mac, msen, and mint labels on 
 *  the system label lists are not freed.
 */
void
sesmgr_socket_free_attrs(struct socket *so)
{
	struct ipsec *sp = so->so_sesmgr_data;

	if (sp == NULL)
		return;

#ifdef LEANVEOUT
	/* Free socket acl if present */
	if (sp->sm_soacl != NULL)
		mcb_free(sp->sm_soacl, sizeof *sp->sm_soacl, MT_SOACL);
#endif

	/* Free connection data */
	if (sp->sm_conn_data != NULL)
		m_freem(sp->sm_conn_data);

	/* Free attribute area */
	sesmgr_soattr_free(sp);
	so->so_sesmgr_data = NULL;
}

/*
 *  Copy attributes from one socket to another.  This routine
 *  is invoked when an accpet socket is created.
 */
/*ARGSUSED*/
int
sesmgr_socket_copy_attrs(
	struct socket *so_src,
	struct socket *so_dst,
	struct ipsec  *sec_attrs
	)
{
	struct ipsec *src_attrs = so_src->so_sesmgr_data;
	struct ipsec *dst_attrs = so_dst->so_sesmgr_data;

	ASSERT(src_attrs != NULL);
	ASSERT(dst_attrs == NULL);

	/* Allocate space for attributes in the new socket */

	dst_attrs = sesmgr_soattr_alloc(M_DONTWAIT);
	if (dst_attrs == NULL)
		return ENOMEM;

	/*
	 *  First copy the whole security attributes structure,
	 *  then allocate space and copy those attributes that
	 *  can't be shared.
	 */
        bcopy(src_attrs, dst_attrs, sizeof(*dst_attrs));

	/* Initialize the various bookkeeping fields. */

	dst_attrs->sm_samp_cnt = 0;
	dst_attrs->sm_samp_seq = 0;
	dst_attrs->sm_conn_data = NULL;
	dst_attrs->sm_ip_flags = 0;
	dst_attrs->sm_soacl = NULL;

	if (src_attrs->sm_conn_data != NULL) {
		dst_attrs->sm_conn_data = m_copy(src_attrs->sm_conn_data,
						 0, M_COPYALL);
		if (dst_attrs->sm_conn_data == NULL)
			goto error_out;
	}

	/* Clear any inherited samp attributes.  If this is
	 * a samp host the new attributes will be process upstream.
	 */
	soattr_init(&dst_attrs->sm_msg);
	soattr_init(&dst_attrs->sm_dflt);
	soattr_init(&dst_attrs->sm_snd);

	so_dst->so_sesmgr_data = dst_attrs;

	if ( sec_attrs != NULL && (sa_sm_rcv.sa_mask & (T6M_SL|T6M_INTEG_LABEL|T6M_UID)) )
		bcopy( &sec_attrs->sm_rcv,
			&dst_attrs->sm_rcv,
			 sizeof(mac_b_label) );
	else 
		soattr_init(&dst_attrs->sm_rcv);

	return 0;

error_out:

	if (dst_attrs->sm_conn_data != NULL)
		m_freem(dst_attrs->sm_conn_data);

	sesmgr_soattr_free(dst_attrs);

	return ENOMEM;
}

#ifdef DEBUG
static void
sesmgr_socket_check_attrs(struct socket *so)
{
	struct ipsec *soattrs;

	ASSERT(so != NULL);
	soattrs = so->so_sesmgr_data;
	ASSERT(soattrs != NULL);

	ASSERT(soattrs->sm_protocol_id == 0x1234);

	ASSERT(soattrs->sm_sendlabel != NULL);
	ASSERT(!mac_invalid(soattrs->sm_sendlabel));

	ASSERT(soattrs->sm_label != NULL);
	ASSERT(!mac_invalid(soattrs->sm_label));

	if (soattrs->sm_ip_flags & IPSOPT_CIPSO)
		ASSERT(!mac_invalid(soattrs->sm_ip_lbl));

}
#endif

int
siocgetlabel(struct socket *so, caddr_t addr) 
{ 
	struct mac_label * ml = sesmgr_get_label(so);
	return(copyout((caddr_t)ml, addr, mac_size(ml)) );
}

int
siocsetlabel(struct socket *so, caddr_t addr) 
{ 
	mac_label *new_label = NULL;
	int error = 0;

	/* Must have privilege to change socket label */
	if (!_CAP_ABLE(CAP_NETWORK_MGT))  {
		error = EPERM;
		goto out;
	}
	
	/* Copy in the new mac label */
	if ((error = mac_copyin_label((mac_label *)addr, &new_label)) != 0) {
		new_label = NULL;
		goto out;
	}

	/* Set the socket label */
	error = (*so->so_proto->pr_usrreq)(so, PRU_SOCKLABEL, 
					   (struct mbuf *) NULL, 
					   (struct mbuf *) new_label, 
					   (struct mbuf *) NULL);
out:
	/* generate audit record, *u.u_ap is file descriptor */
	/*_SAT_BSDIPC_MAC_CHANGE(*u.u_ap, so, new_label, curprocp, error);*/
		/* SAT_XXX */
	return(error);
}
