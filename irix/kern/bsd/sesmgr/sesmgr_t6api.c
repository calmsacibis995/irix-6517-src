/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *	SGI Session Manager
 */

#ident	"$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socketvar.h>
#include <sys/kmem.h>
#include <sys/kabi.h>
#include <sys/xlate.h>
#include <sys/cred.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <sys/sat.h>
#include <sys/acl.h>
#include <sys/mac.h>
#include <sys/mac_label.h>

#include <sys/t6attrs.h>
#include <sys/t6rhdb.h>
#include <sys/sesmgr.h>
#include <sys/vsocket.h>
#include "sesmgr_samp.h"
#include <sys/t6api_private.h>

#include "sm_private.h"

/*
 *  Table of MAX sizes of attributes.  This table
 *  appears also in the TSIX library.  The two should
 *  be kept in sync.
 */

static int t6_attr_size[] = {
        sizeof(struct mac_b_label),     /* T6_SL */
        T6MAX_TEXT_BUF,                 /* T6_NAT_CAVEATS */
        sizeof(struct mac_b_label),     /* T6_INTEG_LABEL */
        sizeof(uid_t),                  /* T6_SESSION_ID */
	sizeof(struct mac_b_label),	/* T6_CLEARANCE */
        sizeof(struct acl),             /* T6_ACL */
        0,                              /* T6_IL */
        sizeof(struct cap_set),         /* T6_PRIVILEGES */
        sizeof(uid_t),                  /* T6_AUDIT_ID */
        sizeof(pid_t),                  /* T6_PID */
	0,				/* T6_RESV10 */
        T6MAX_TEXT_BUF,                 /* T6_AUDIT_INFO */
        sizeof(uid_t),                  /* T6_UID */
        sizeof(gid_t),                  /* T6_GID */
	sizeof(t6groups_t),		/* T6_GROUPS */
	0,				/* T6_PROC_ATTR */
};

#if (_MIPS_SIM == _MIPS_SIM_ABI64)
int irix5_to_t6ctl(enum xlate_mode, void *, int, xlate_info_t *);
int t6ctl_to_irix5(void *, int, xlate_info_t *);
int irix5_to_t6parms(enum xlate_mode, void *, int, xlate_info_t *);
#endif

/*
 *  Common code to copy out the composite attributes.
 */

static int
copyout_attrs(soattr_t *sa, ipsec_t *sp, t6ctl_t *attrs, t6mask_t *mask)
{
	int i, error = 0;
	t6mask_t my_mask;

	/* initialize masks */
	*mask = my_mask = T6M_NO_ATTRS;

	/* Copy out the individual attributes */
	for (i = 0; i < T6_MAX_ATTRS; i++) {
		iovec_t *iovp;
		caddr_t aptr;

		if ((t6_mask(i) & attrs->t6_alloc_mask) == T6M_NO_ATTRS)
			continue;

		iovp = &attrs->t6_iov[i];
		aptr = NULL;

		switch(i) {
		case T6_SL:
			if (sa->sa_mask & T6M_SL)
				aptr = (caddr_t) sa->sa_msen;
			else if (sp && sp->sm_ip_flags)
				aptr = (caddr_t) msen_from_mac(sp->sm_ip_lbl);
			break;

		case T6_NAT_CAVEATS:
			/* Not Supported */
			break;

		case T6_INTEG_LABEL:
			if (sa->sa_mask & T6M_INTEG_LABEL)
				aptr = (caddr_t) sa->sa_mint;
			else if (sp && sp->sm_ip_flags)
				aptr = (caddr_t) mint_from_mac(sp->sm_ip_lbl);
			break;

		case T6_SESSION_ID:
			if (sa->sa_mask & T6M_SESSION_ID)
				aptr = (caddr_t) &sa->sa_sid;
			break;

		case T6_CLEARANCE:
			/* Not supported */
			break;

		case T6_ACL:
			/* Not supported */
			break;

		case T6_IL:
			/* not supported */
			break;

		case T6_PRIVILEGES:
			if (sa->sa_mask & T6M_PRIVILEGES)
				aptr = (caddr_t) &sa->sa_privs;
			break;

		case T6_AUDIT_ID:
			if (sa->sa_mask & T6M_AUDIT_ID)
				aptr = (caddr_t) &sa->sa_audit_id;
			else if (sp && sp->sm_ip_flags)
				aptr = (caddr_t) &sp->sm_ip_uid;
			break;

		case T6_PID:
			/* not supported */
			break;

		case T6_RESV10:
			error = EINVAL;
			goto out;

		case T6_AUDIT_INFO:
			/* not supported */
			break;

		case T6_UID:
			if (sa->sa_mask & T6M_UID)
				aptr = (caddr_t) &sa->sa_ids.uid;
			else if (sp && sp->sm_ip_flags)
				aptr = (caddr_t) &sp->sm_ip_uid;
			break;

		case T6_GID:
			if (sa->sa_mask & T6M_GID)
				aptr = (caddr_t) &sa->sa_ids.gid;
			break;

		case T6_GROUPS:
			if (sa->sa_mask & T6M_GROUPS)
				aptr = (caddr_t) &sa->sa_ids.groups;
			break;
		}

		/* Copy Attribute to User Buffer */
		if (aptr != NULL) {
			if (copyout(aptr, iovp->iov_base, iovp->iov_len)) {
				error = EFAULT;
				goto out;
			}
			my_mask |= t6_mask(i);
		}
	}

	attrs->t6_valid_mask |= (*mask = my_mask);
out:
	return error;;
}

/* 
 * XXX the following implementation of t6ext_attr and t6new_attr
 * uses a int for each flag just to get up and running.  In the
 * future we probably want an bit vector for flags.
 *
 */

/* ARGSUSED */
int
sesmgr_t6ext_attr(int fd, t6cmd_t cmd, rval_t *rvp)
{
	/* this is now a noop */
	return 0;
}

/* ARGSUSED */
int
sesmgr_t6new_attr(int fd, t6cmd_t cmd, rval_t *rvp)
{
        int error = 0;
        struct socket *so;
	struct ipsec *sp;

        if (error = sesmgr_getsock(fd, &so, NULL))
                goto out;

	/* Pointer to sesmgr private data */
	ASSERT(so->so_sesmgr_data != NULL);
	sp = so->so_sesmgr_data;

	/* set option */
	switch (cmd) {
	case T6_ON:
	case T6_OFF:
		sp->sm_new_attr = cmd;
		break;
	default:
		error = EINVAL;
		break;
	}

out:
	/* XXX create audit record here */
	return error;
}

/* ARGSUSED */
int
sesmgr_t6mls_socket(int fd, t6cmd_t cmd, rval_t *rvp)
{
        int error = 0;
        struct socket *so;
	struct ipsec *sp;

	/* Check privilege */
	/* XXX this may need to be specific MAC and DAC caps */
	if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
		error = EPERM;
		goto out;
	}

        if (error = sesmgr_getsock(fd, &so, NULL))
                goto out;

	/* Pointer to sesmgr private data */
	ASSERT(so->so_sesmgr_data != NULL);
	sp = so->so_sesmgr_data;

	/* set option */
	switch (cmd) {
	case T6_ON:
		sp->sm_cap_net_mgt = 1;
		break;
	case T6_OFF:
		sp->sm_cap_net_mgt = 0;
		break;
	default:
		error = EINVAL;
		break;
	}

out:
	/* XXX create audit record here */
	return error;
}

/* ARGSUSED */
int
sesmgr_t6get_endpt_mask(int fd, t6mask_t *mask, rval_t *rvp)
{
        int error = 0;
        struct socket *so;
        struct ipsec *sp;

        if (error = sesmgr_getsock(fd, &so, NULL))
                goto out;

	/* Pointer to sesmgr private data */
	ASSERT(so->so_sesmgr_data != NULL);
	sp = so->so_sesmgr_data;

	if (copyout(&sp->sm_mask, (caddr_t) mask, sizeof(*mask))) {
		error = EFAULT;
		goto out;
	}

out:
	/* XXX create audit record here */
	return error;
}

/* ARGSUSED */
int
sesmgr_t6set_endpt_mask(int fd, t6mask_t mask, rval_t *rvp)
{
        int error = 0;
        struct socket *so;
        struct ipsec *sp;
	/* t6mask_t old_mask;	*/	/* SAT_XXX */

	/* Check privilege */
	/* XXX this may need to be specific MAC and DAC caps */
	if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
		error = EPERM;
		goto out;
	}

        if (error = sesmgr_getsock(fd, &so, NULL))
                goto out;

	/* Pointer to sesmgr private data */
	ASSERT(so->so_sesmgr_data != NULL);
	sp = so->so_sesmgr_data;

	/* Set mask */
	/* old_mask = sp->sm_mask; */  /* SAT_XXXX */
	sp->sm_mask = mask;

out:
	/* XXX create audit record here */
	return error;
}

/* ARGSUSED */
int
sesmgr_t6get_endpt_default(int fd, t6mask_t *mask, t6attr_t attr_ptr,
			   rval_t *rvp)
{
        int error = 0;
        struct socket *so;
	struct ipsec *sp;
	t6ctl_t	attrs;
	t6mask_t my_mask;

        if (error = sesmgr_getsock(fd, &so, NULL))
                goto out;

	/* Copy in the attribute vector and attribute mask */
	if (COPYIN_XLATE(attr_ptr, &attrs, sizeof(attrs),
			 irix5_to_t6ctl, get_current_abi(), 1)) {
		error = EFAULT;
		goto out;
	}

	/* Pointer to sesmgr private data */
	ASSERT(so->so_sesmgr_data != NULL);
	sp = so->so_sesmgr_data;

	if (error = copyout_attrs(&sp->sm_dflt, (ipsec_t *) NULL,
				  &attrs, &my_mask))
		goto out;

	/* Return argument mask */
	if (copyout(&my_mask, (caddr_t) mask, sizeof(my_mask))) {
		error = EFAULT;
		goto out;
	}

	/* Copy just t6_alloc_mask and t6_valid_mask */
	if (copyout(&attrs, attr_ptr,
		    (char *) &attrs.t6_iov[0] - (char *) &attrs)) {
		error = EFAULT;
		goto out;
	}
out:
	/* XXX create audit record here */
	return error;
}

static int
sesmgr_slok(msen_t msen, cred_t *cred)
{
	int error = 0;
	msen_t pmsen;

	/* "equal" sensitivity is special */
	if (msen->b_type == MSEN_EQUAL_LABEL &&
	    !_CAP_CRABLE(cred, CAP_MAC_RELABEL_OPEN)) {
		error = EINVAL;
		goto out;
	}

	/* extract sensitivity portion of process label */
	if ((pmsen = msen_from_mac(cred->cr_mac)) == NULL) {
		error = EINVAL;
		goto out;
	}

	/* need CAP_MAC_MLD to change moldiness */
	if (msen_is_moldy(msen) != msen_is_moldy(pmsen) &&
	    !_CAP_CRABLE(cred, CAP_MAC_MLD)) {
		error = EPERM;
		goto out;
	}

	/* compare requested label with process label */
	if (msen != pmsen) {
		if (msen_dom(msen, pmsen)) {
			if (!_CAP_CRABLE(cred, CAP_MAC_UPGRADE)) {
				error = EPERM;
				goto out;
			}
		}
		else if (!_CAP_CRABLE(cred, CAP_MAC_DOWNGRADE)) {
			error = EPERM;
			goto out;
		}
	}
out:
	return error;
}

static int
sesmgr_ilok(mint_t mint, cred_t *cred)
{
	int error = 0;
	mint_t pmint;

	/* "equal" integrity is special */
	if (mint->b_type == MINT_EQUAL_LABEL &&
	    !_CAP_CRABLE(cred, CAP_MAC_RELABEL_OPEN)) {
		error = EINVAL;
		goto out;
	}

	/* extract integrity portion of process label */
	if ((pmint = mint_from_mac(cred->cr_mac)) == NULL) {
		error = EINVAL;
		goto out;
	}

	/* compare requested label with process label */
	if (mint != pmint) {
		if (mint_dom(pmint, mint)) {
			if (!_CAP_CRABLE(cred, CAP_MAC_UPGRADE)) {
				error = EPERM;
				goto out;
			}
		}
		else if (!_CAP_CRABLE(cred, CAP_MAC_DOWNGRADE)) {
			error = EPERM;
			goto out;
		}
	}
out:
	return error;
}

static int
t6groups_invalid(t6groups_t *grp)
{
	__uint32_t i;

	if (grp->ngroups > ngroups_max)
		return(EINVAL);
	for (i = 0; i < grp->ngroups; i++)
		if (grp->groups[i] < 0 || grp->groups[i] > MAXUID)
			return(EINVAL);
	return(0);
}

union sesmgr_set_soattr {
	char info[T6MAX_TEXT_BUF];
	uid_t uid;
	uid_t aid;
	gid_t gid;
	pid_t pid;
	struct mac_b_label lbl;
	struct acl acl;
	struct cap_set cap;
	t6groups_t groups;
};

static int
sesmgr_set_soattr(soattr_t *sa, t6mask_t mask, t6ctl_t *attrs)
{
	int error = 0, i;

	/* Copy in the individual attributes */
	for (i = 0; i < T6_MAX_ATTRS; i++) {
		iovec_t *iovp;
		caddr_t kptr;		/* pointer to kernel space */
		msen_t msenptr;
		mint_t mintptr;
		union sesmgr_set_soattr soattrs;

		if ((t6_mask(i) & mask) == T6M_NO_ATTRS)
			continue;

		/* set up pointers */
		iovp = &attrs->t6_iov[i];
		kptr = NULL;

		/* check size against max size */
		if (t6_attr_size[i] == 0 || iovp->iov_len > t6_attr_size[i]) {
			error = EINVAL;
			goto out;
		}

		/* copy user buffer to kernel space */
		if (copyin(iovp->iov_base, &soattrs, iovp->iov_len)) {
			error = EFAULT;
			goto out;
		}

		switch(i) {
		case T6_SL:
			if (!msen_valid(&soattrs.lbl)) {
				error = EINVAL;
				goto out;
			}
			msenptr = msen_add_label(&soattrs.lbl);
			if (error = sesmgr_slok(msenptr, get_current_cred()))
				goto out;
			sa->sa_msen = msenptr;
			sa->sa_mask |= t6_mask(i);
			continue;
		case T6_NAT_CAVEATS:
			error = EINVAL;
			goto out;
		case T6_INTEG_LABEL:
			if (!mint_valid(&soattrs.lbl)) {
				error = EINVAL;
				goto out;
			}
			mintptr = mint_add_label(&soattrs.lbl);
			if (error = sesmgr_ilok(mintptr, get_current_cred()))
				goto out;
			sa->sa_mint = mintptr;
			sa->sa_mask |= t6_mask(i);
			continue;
		case T6_SESSION_ID:
			if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
				error = EPERM;
				goto out;
			}
			kptr = (caddr_t) &sa->sa_sid;
			break;
		case T6_CLEARANCE:
			if (!msen_valid(&soattrs.lbl)) {
				error = EINVAL;
				goto out;
			}
			msenptr = msen_add_label(&soattrs.lbl);
			if (error = sesmgr_slok(msenptr, get_current_cred()))
				goto out;
			sa->sa_clearance = msenptr;
			sa->sa_mask |= t6_mask(i);
			continue;
		case T6_ACL:
			error = EINVAL;
			goto out;
		case T6_IL:
			error = EINVAL;
			goto out;
		case T6_PRIVILEGES:
			if (!_CAP_ABLE(CAP_SETPCAP)) {
				error = EPERM;
				goto out;
			}
			kptr = (caddr_t) &sa->sa_privs;
			break;
		case T6_AUDIT_ID:
			if (!_CAP_ABLE(CAP_AUDIT_CONTROL)) {
				error = EPERM;
				goto out;
			}
			if (soattrs.aid < 0 || soattrs.aid > MAXUID) {
				error = EINVAL;
				goto out;
			}
			kptr = (caddr_t) &sa->sa_audit_id;
			break;
		case T6_PID:
			error = EINVAL;
			goto out;
		case T6_RESV10:
			error = EINVAL;
			goto out;
		case T6_AUDIT_INFO:
			error = EINVAL;
			goto out;
		case T6_UID:
			if (!_CAP_ABLE(CAP_SETUID)) {
				error = EPERM;
				goto out;
			}
			if (soattrs.uid < 0 || soattrs.uid > MAXUID) {
				error = EINVAL;
				goto out;
			}
			kptr = (caddr_t) &sa->sa_ids.uid;
			break;
		case T6_GID:
			if (!_CAP_ABLE(CAP_SETGID)) {
				error = EPERM;
				goto out;
			}
			if (soattrs.gid < 0 || soattrs.gid > MAXUID) {
				error = EINVAL;
				goto out;
			}
			kptr = (caddr_t) &sa->sa_ids.gid;
			break;
		case T6_GROUPS:
			if (!_CAP_ABLE(CAP_SETGID)) {
				error = EPERM;
				goto out;
			}
			if (error = t6groups_invalid(&soattrs.groups))
				goto out;
			kptr = (caddr_t) &sa->sa_ids.groups;
			break;
		}

		/* Copy attribute from temporary kernel buffer */
		if (kptr != NULL) {
			bcopy(&soattrs, kptr, iovp->iov_len);
			sa->sa_mask |= t6_mask(i);
		}
	}

out:
	/* XXX create audit record here */
	return error;
}

/* ARGSUSED */
int
sesmgr_t6set_endpt_default(int fd, t6mask_t mask, t6attr_t attr_ptr,
			   rval_t *rvp)
{
        int error = 0;
        struct socket *so;
	struct ipsec *sp;
	soattr_t *sa;
	t6ctl_t	attrs;
	/* t6mask_t old_mask; */	/* SAT_XXX */

        if (error = sesmgr_getsock(fd, &so, NULL))
		goto out;

	/* Copy in the attribute vector */
	if (COPYIN_XLATE(attr_ptr, &attrs, sizeof(attrs),
			 irix5_to_t6ctl, get_current_abi(), 1)) {
		error = EFAULT;
		goto out;
	}

	/* check alloc_mask vs valid_mask */
	if ((attrs.t6_valid_mask & ~attrs.t6_alloc_mask) != T6M_NO_ATTRS ||
	    (mask & ~attrs.t6_valid_mask) != T6M_NO_ATTRS) {
		error = EINVAL;
		goto out;
	}

	/* Pointer to sesmgr private data */
	sp = so->so_sesmgr_data;
	ASSERT(sp != NULL);
	sa = &sp->sm_dflt;

	/* old_mask = sa->sa_mask; */  /* SAT_XXX */
	if (error = sesmgr_set_soattr(sa, mask, &attrs))
		goto out;

out:
	return error;
}

/* ARGSUSED */
int
sesmgr_t6peek_attr(int fd, t6attr_t attr_ptr, t6mask_t *new_attrs,
		   rval_t *rvp)
{
        int error = 0;
        struct socket *so;
	struct ipsec *sp;
	t6ctl_t	attrs;
	t6mask_t my_mask;

        if (error = sesmgr_getsock(fd, &so, NULL))
                goto out;

	/* Pointer to sesmgr private data */
	ASSERT(so->so_sesmgr_data != NULL);
	sp = so->so_sesmgr_data;

	/* Copy in the attribute vector */
	if (COPYIN_XLATE(attr_ptr, &attrs, sizeof(attrs),
			 irix5_to_t6ctl, get_current_abi(), 1)) {
		error = EFAULT;
		goto out;
	}

	/* Is there received data ? */
	if (sesmgr_samp_nread(so, &error, 1) == 0 && error != 0)
		goto out;

	/* Copy out the individual attributes */
	if ((error = copyout_attrs(&sp->sm_rcv, sp, &attrs, &my_mask)) != 0)
		goto out;

	/* Return argument mask */
	if (copyout(&my_mask, (caddr_t) new_attrs, sizeof(my_mask))) {
		error = EFAULT;
		goto out;
	}

	/* Copy just t6_alloc_mask and t6_valid_mask */
	if (copyout(&attrs, attr_ptr,
			(char *) &attrs.t6_iov[0] - (char *) &attrs)) {
		error = EFAULT;
		goto out;
	}
out:
	/* XXX create audit record here */
	return error;
}

/* ARGSUSED */
int
sesmgr_t6last_attr(int fd, t6attr_t attr_ptr, t6mask_t *new_attrs,
		   rval_t *rvp)
{
        int error = 0;
        struct socket *so;
        struct ipsec *sp;
	t6ctl_t	attrs;
	t6mask_t my_mask;

        if (error = sesmgr_getsock(fd, &so, NULL))
                goto out;

	/* Pointer to sesmgr private data */
	ASSERT(so->so_sesmgr_data != NULL);
	sp = so->so_sesmgr_data;

	/* Copy in the attribute vector */
	if (COPYIN_XLATE(attr_ptr, &attrs, sizeof(attrs),
			 irix5_to_t6ctl, get_current_abi(), 1)) {
		error = EFAULT;
		goto out;
	}

	/* Copy out the indivdual attributes */
	if ((error = copyout_attrs(&sp->sm_rcv, sp, &attrs, &my_mask)) != 0)
		goto out;

	/* Return mask */
	if (copyout(&my_mask, (caddr_t) new_attrs, sizeof(my_mask))) {
		error = EFAULT;
		goto out;
	}

	/* Copy just t6_alloc_mask and t6_valid_mask */
	if (copyout(&attrs, attr_ptr,
           		(char *) &attrs.t6_iov[0] - (char *) &attrs)) {
		error = EFAULT;
		goto out;
	}

out:
	/* XXX create audit record here */
	return error;
}

int
sesmgr_t6sendto(int fd, char * buf, int len, int flags, struct sockaddr *to,
		int tolen, t6attr_t attr_ptr, rval_t *rvp)
{
	int error;
	struct socket *so;
	struct ipsec *sp;
	soattr_t *sa = NULL;
	t6ctl_t	attrs;
	struct vsocket *vso;

	struct msghdr msg;
	struct iovec aiov;

	if (error = sesmgr_getsock(fd, &so, &vso))
		goto out;

	/* Copy in the attribute vector */
	if (COPYIN_XLATE(attr_ptr, &attrs, sizeof(attrs),
			 irix5_to_t6ctl, get_current_abi(), 1)) {
		error = EFAULT;
		goto out;
	}

	/* check alloc_mask vs valid_mask */
	if ((attrs.t6_valid_mask & ~attrs.t6_alloc_mask) != T6M_NO_ATTRS) {
		error = EINVAL;
		goto out;
	}

	/* get per-message attribute structure */
	sp = so->so_sesmgr_data;
	ASSERT(sp != NULL);
	sa = &sp->sm_msg;

	/* set the per-message defaults */
	if (error = sesmgr_set_soattr(sa, attrs.t6_valid_mask, &attrs))
		goto out;

	/* Send message */
	msg.msg_name = (caddr_t) to;
	msg.msg_namelen = tolen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = buf;
	aiov.iov_len = len;
	msg.msg_accrights = 0;
	msg.msg_accrightslen = 0;
	VSOP_SENDIT(vso, &msg, flags, rvp, error);

out:
	/* restore attribute defaults */
	if (sa != NULL)
		sa->sa_mask = T6M_NO_ATTRS;

	/* XXX Create audit record here */
	return error;
}

int
sesmgr_t6recvfrom(int fd, char *buf, int buf_len, int flags,
		  struct sockaddr *from, int *fromlen,
		  t6parms_t *p_ptr, rval_t *rvp)
{
        int error = 0, len;
        struct socket *so;
        struct ipsec *sp;
	t6ctl_t	attrs;
	t6parms_t parms;
	t6mask_t my_mask;
	struct vsocket *vso;
	struct msghdr msg;
	struct iovec aiov;

        if (error = sesmgr_getsock(fd, &so, &vso))
                goto out;

	/* Copy in the parms structure */
	if (COPYIN_XLATE(p_ptr, &parms, sizeof(parms),
			 irix5_to_t6parms, get_current_abi(), 1)) {
		error = EFAULT;
		goto out;
	}

	/* Copy in the attribute vector */
	if (COPYIN_XLATE(parms.attrp, &attrs, sizeof(attrs),
			 irix5_to_t6ctl, get_current_abi(), 1)) {
		error = EFAULT;
		goto out;
	}

	/* Pointer to sesmgr private data */
	ASSERT(so->so_sesmgr_data != NULL);
	sp = so->so_sesmgr_data;

	/* Receive message */
        if (fromlen != NULL) {
		error = copyin(fromlen, &len, sizeof (len));
		if (error) {
			goto out;
		}
        } else
		len = 0;

	msg.msg_name = (caddr_t) from;
	msg.msg_namelen = len;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = buf;
	aiov.iov_len = buf_len;
	msg.msg_accrights = 0;
	msg.msg_accrightslen = 0;
	VSOP_RECVIT (vso, &msg, &flags, (caddr_t) fromlen, (void *) 0, rvp,
		     BSD43_SOCKETS, error);
	if (error)
		goto out;

	/* Copy out the individual attributes */
	if ((error = copyout_attrs(&sp->sm_rcv, sp, &attrs, &my_mask)) != 0)
		goto out;

	/* Return mask */
	if (copyout(&my_mask, (caddr_t) parms.maskp, sizeof(my_mask))) {
		error = EFAULT;
		goto out;
	}

	/* Copy just t6_alloc_mask and t6_valid_mask */
	if (copyout(&attrs, (caddr_t) parms.attrp,
		    (char *) &attrs.t6_iov[0] - (char *) &attrs)) {
		error = EFAULT;
		goto out;
	}

out:
	/* XXX create audit record here */
	return error;
}

int
sesmgr_t6_syscall (struct syssesmgra *uap, rval_t *rvp)
{

	/* record command for sat */
	/*_SAT_SET_SUBSYSNUM(curprocp,uap->cmd); */  /* SAT_XXX */

	/* Dispatch to subcommand */
	switch (uap->cmd & 0xffff) {

	case T6EXT_ATTR_CMD:
		return sesmgr_t6ext_attr(uap->arg1, (t6cmd_t) uap->arg2, rvp);

	case T6NEW_ATTR_CMD: 
		return sesmgr_t6new_attr(uap->arg1, (t6cmd_t) uap->arg2, rvp);

	case T6GET_ENDPT_MASK_CMD:
		return sesmgr_t6get_endpt_mask(uap->arg1,
					       (t6mask_t *) uap->arg2, rvp);

	case T6SET_ENDPT_MASK_CMD:
		return sesmgr_t6set_endpt_mask(uap->arg1,
					       (t6mask_t) uap->arg2, rvp);

	case T6GET_ENDPT_DEFAULT_CMD:
		return sesmgr_t6get_endpt_default(uap->arg1,
						  (t6mask_t *) uap->arg2,
						  (t6attr_t) uap->arg3, rvp);

	case T6SET_ENDPT_DEFAULT_CMD:
		return sesmgr_t6set_endpt_default(uap->arg1,
						  (t6mask_t) uap->arg2,
						  (t6attr_t) uap->arg3, rvp);

	case T6PEEK_ATTR_CMD:
		return sesmgr_t6peek_attr(uap->arg1, (t6attr_t) uap->arg2,
					  (t6mask_t *) uap->arg3, rvp);

	case T6LAST_ATTR_CMD:
		return sesmgr_t6last_attr(uap->arg1, (t6attr_t) uap->arg2,
					  (t6mask_t *) uap->arg3, rvp);

	case T6SENDTO_CMD:
		return sesmgr_t6sendto(uap->arg1, (char *) uap->arg2,
				       uap->arg3, uap->arg4,
				       (struct sockaddr *) uap->arg5,
				       uap->arg6, (t6attr_t) uap->arg7, rvp);

	case T6RECVFROM_CMD:
		return sesmgr_t6recvfrom(uap->arg1, (char *) uap->arg2,
					 uap->arg3, uap->arg4,
					 (struct sockaddr *) uap->arg5,
					 (int *) uap->arg6,
					 (t6parms_t *) uap->arg7, rvp);

	case T6MLS_SOCKET_CMD:
		return sesmgr_t6mls_socket(uap->arg1, (t6cmd_t) uap->arg2,
					   rvp);

	case T6RHDB_PUT_HOST_CMD:
		return t6rhdb_put_host((size_t) uap->arg1,
				       (caddr_t) uap->arg2, rvp);

	case T6RHDB_GET_HOST_CMD:
		return t6rhdb_get_host((struct in_addr *) uap->arg1,
				       (size_t) uap->arg2,
				       (caddr_t) uap->arg3, rvp);

	case T6RHDB_FLUSH_CMD:
		return t6rhdb_flush((struct in_addr *) uap->arg1, rvp);

	case T6RHDB_STAT_CMD:
		return ENOPKG;

	default:
		/* Don't know this one */
		return EINVAL;
	}
	/*NOTREACHED*/
	return EINVAL;
}

#if (_MIPS_SIM == _MIPS_SIM_ABI64)

/*ARGSUSED*/
int
irix5_to_t6ctl(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	register int i;
	COPYIN_XLATE_PROLOGUE(irix5_t6ctl, t6ctl);

	target->t6_alloc_mask = source->t6_alloc_mask;
	target->t6_valid_mask = source->t6_valid_mask;
	for (i = 0; i < T6_MAX_ATTRS; i++) {
		target->t6_iov[i].iov_len =
			(size_t)source->t6_iov[i].iov_len;
		target->t6_iov[i].iov_base =
			(void *)(__psunsigned_t)source->t6_iov[i].iov_base;
	}

	return 0;
}

/* ARGSUSED */
int
t6ctl_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register int i;

	XLATE_COPYOUT_PROLOGUE(t6ctl, irix5_t6ctl);
	ASSERT(count == 1);

	target->t6_alloc_mask = source->t6_alloc_mask;
	target->t6_valid_mask = source->t6_valid_mask;
	for (i = 0; i < T6_MAX_ATTRS; i++) {
		target->t6_iov[i].iov_len =
			(app32_int_t) source->t6_iov[i].iov_len;
		target->t6_iov[i].iov_base =
			(app32_ptr_t)(__psunsigned_t)source->t6_iov[i].iov_base;
	}
	return 0;
}

/* ARGSUSED */
int
irix5_to_t6parms(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_t6parms, t6parms);

	target->attrp = (t6ctl_t *) (__psunsigned_t) source->attrp;
	target->maskp = (t6mask_t *)(__psunsigned_t) source->maskp;
	return 0;
}

#endif
