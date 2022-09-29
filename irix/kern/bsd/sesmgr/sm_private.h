/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SM_PRIVATE_H__
#define __SM_PRIVATE_H__

#ident "$Id: sm_private.h,v 1.5 1999/07/21 17:30:24 michaelk Exp $"

#include <sys/acl.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <sys/t6attrs.h>
#include <sys/vsocket.h>

#ifdef _KERNEL

/* Flags */
/*
 * The following two flags should be removed, but is
 * still in some unused code that also needs to be
 * cleaned up.
 */
#define IPSOPT_CIPSO	0x04
#define IPSOPT_IPOUT	0x80		/* IP Options Only, free when done */

/* 
 * Structure for holding samp ids.
 */
typedef struct t6ids {
	uid_t		uid;
	gid_t		gid;
	t6groups_t	groups;
} t6ids_t;

typedef struct soattr {
	t6mask_t	sa_mask;
	msen_t		sa_msen;
	mint_t		sa_mint;
	uid_t		sa_sid;
	msen_t		sa_clearance;
	cap_set_t	sa_privs;
	uid_t		sa_audit_id;
	t6ids_t		sa_ids;
} soattr_t;

typedef struct ipsec {
	/* XXX following should be header struct */
	int			sm_protocol_id;
	
	/* Session Manager Flags */
	int			sm_cap_net_mgt;	/* indicates privileged
						   process */
	int			sm_new_attr;
	u_int			sm_samp_seq;
	t6mask_t		sm_mask;	/* endpoint-default mask */
	size_t			sm_samp_cnt;	/* number of bytes till next */
						/* samp header (TCP)         */

	/* Cipso compatibility stuff */
	mac_t			sm_label;	/* dominates all data
						   rcvd on socket */
	mac_t			sm_sendlabel;	/* label sent by udp_output */
	struct soacl *		sm_soacl;	/* ACL on socket */
	uid_t			sm_uid;		/* uid sent */
	uid_t			sm_rcvuid;	/* uid received - set by
						   sonewconn */

	/* IP Security Options */
	short			sm_ip_flags;
	short			sm_ip_mask;	/* currently unused */
	mac_t			sm_ip_lbl;
	uid_t			sm_ip_uid;
	struct ifnet *		sm_ip_ifp;

	/*
	 * This set of attributes can be set by processes
	 * with appropriate privilege and are sent, on a
	 * per-message basis, instead of the corresponding
	 * default attribute or process attribute.
	 */
	soattr_t		sm_msg;

	/*
	 * Default Attributes.  These attributes can be set by 
	 * processes with appropriate privilege and are sent instead
	 * of the corresponding process attribute.
	 */
	soattr_t		sm_dflt;

	/*
	 *  This set of attributes are the composite of the received
	 *  attributes and any defaults specified for the interface or
	 *  for the remote host.
	 */
	soattr_t		sm_rcv;

	/*
	 *  This set of attributes are the last sent attributes.  A copy
	 *  is saved so that any changes in process attributes since the
	 *  last send can be detected and only the changed attribute sent.
	 */
	soattr_t		sm_snd;		/* last sent message attrs */

	/* data to be sent when connection is established */
	struct mbuf *		sm_conn_data;
} ipsec_t;

#define	sa_sm_rcv	sec_attrs->sm_rcv
#define	sa_sm_snd	sec_attrs->sm_snd
#define	sa_sm_msg	sec_attrs->sm_msg
#define	sa_sm_dflt	sec_attrs->sm_dflt

/*
 * This structure has been opsoleted and should be removed, but
 * it is used in the ifnet{} (if.h) structure.  I was told NOT to 
 * alter the size of the ifnet{} structure, so it can not be
 * completely removed until it is has been cleared from the
 * ifnet{} structure.
 */
typedef struct t6if_attrs {
	int *			ifs_opsoleted_struct;
} t6if_attrs_t;

int sesmgr_getsock(int fd, struct socket **sop, struct vsocket **);

#endif  /* _KERNEL */

#endif	/* __SM_PRIVATE_H__ */
