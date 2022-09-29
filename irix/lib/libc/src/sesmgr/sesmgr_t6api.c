#ifdef __STDC__
	#pragma weak t6alloc_blk = _t6alloc_blk
	#pragma weak t6attr_alloc = _t6attr_alloc
	#pragma weak t6create_attr = _t6create_attr
	#pragma weak t6free_blk = _t6free_blk
	#pragma weak t6free_attr = _t6free_attr
	#pragma weak t6cmp_blk = _t6cmp_blk
	#pragma weak t6copy_blk = _t6copy_blk
	#pragma weak t6copy_attr = _t6copy_attr
	#pragma weak t6dup_blk = _t6dup_blk
	#pragma weak t6dup_attr = _t6dup_attr
	#pragma weak t6clear_blk = _t6clear_blk
	#pragma weak t6cmp_attrs = _t6cmp_attrs
	#pragma weak t6get_attr = _t6get_attr
	#pragma weak t6set_attr = _t6set_attr
	#pragma weak t6size_attr = _t6size_attr
	#pragma weak t6ext_attr = _t6ext_attr
	#pragma weak t6new_attr = _t6new_attr
	#pragma weak t6mls_socket = _t6mls_socket
	#pragma weak t6get_endpt_mask = _t6get_endpt_mask
	#pragma weak t6set_endpt_mask = _t6set_endpt_mask
	#pragma weak t6get_endpt_default = _t6get_endpt_default
	#pragma weak t6set_endpt_default = _t6set_endpt_default
	#pragma weak t6last_attr = _t6last_attr
	#pragma weak t6peek_attr = _t6peek_attr
	#pragma weak t6recvfrom = _t6recvfrom
	#pragma weak t6sendto = _t6sendto
	#pragma weak t6rhdb_put_host = _t6rhdb_put_host
	#pragma weak t6rhdb_get_host = _t6rhdb_get_host
	#pragma weak t6rhdb_flush = _t6rhdb_flush
	#pragma weak t6rhdb_stat = _t6rhdb_stat
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sys/mac_label.h>

#include <sys/sesmgr.h>
#include <sys/t6attrs.h>
#include <sys/t6api_private.h>
#include <sys/t6rhdb.h>

#ident "$Revision: 1.4 $"

/*
 *  This API is defined by the TSIG TSIX(re) 1.1 specification.
 *
 *
 */

static const int t6_attr_size[] = {
	sizeof(struct mac_b_label),	/* T6_SL */
	T6MAX_TEXT_BUF,			/* T6_NAT_CAVEATS */
	sizeof(struct mac_b_label),	/* T6_INTEG_LABEL */
	sizeof(uid_t),			/* T6_SESSION_ID */
	sizeof(struct mac_b_label),	/* T6_CLEARANCE */
	sizeof(struct acl),		/* T6_ACL */
	0,				/* T6_IL */
	sizeof(struct cap_set),		/* T6_PRIVILEGES */
	sizeof(uid_t),			/* T6_AUDIT_ID */
	sizeof(pid_t),			/* T6_PID */
	0,				/* T6_RESV10 */
	T6MAX_TEXT_BUF,			/* T6_AUDIT_INFO */
	sizeof(uid_t),			/* T6_UID */
	sizeof(gid_t),			/* T6_GID */
	sizeof(t6groups_t),		/* T6_GROUPS */
	0,				/* T6_PROC_ATTR */
};

t6attr_t
t6alloc_blk(t6mask_t alloc_mask)
{
	t6ctl_t *acb;
	int i, error = ENOMEM;

	if ((acb = (t6ctl_t *) malloc(sizeof (*acb))) == NULL) {
		setoserror(error);
		return(NULL);
	}

	/* save request mask */
	acb->t6_alloc_mask = alloc_mask;
	acb->t6_valid_mask = T6M_NO_ATTRS;

	/* set all attribute vectors to NULL */
	for (i = 0; i < T6_MAX_ATTRS; i++) {
		acb->t6_iov[i].iov_len = 0;
		acb->t6_iov[i].iov_base = NULL;
	}

	/*
	 * Allocate space for individual attributes.
	 *
	 * XXX as a future optimization we may want to put all the
	 *     uid & gid data types into a block.
	 */

	if (acb->t6_alloc_mask & T6M_SL) {
		acb->t6_iov[T6_SL].iov_len = t6_attr_size[T6_SL];
		acb->t6_iov[T6_SL].iov_base = malloc(t6_attr_size[T6_SL]);
		if (acb->t6_iov[T6_SL].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_NAT_CAVEATS) {
		acb->t6_iov[T6_NAT_CAVEATS].iov_len =
			t6_attr_size[T6_NAT_CAVEATS];
		acb->t6_iov[T6_NAT_CAVEATS].iov_base =
			malloc(t6_attr_size[T6_NAT_CAVEATS]);
		if (acb->t6_iov[T6_NAT_CAVEATS].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_INTEG_LABEL) {
		acb->t6_iov[T6_INTEG_LABEL].iov_len =
			t6_attr_size[T6_INTEG_LABEL];
		acb->t6_iov[T6_INTEG_LABEL].iov_base =
			malloc(t6_attr_size[T6_INTEG_LABEL]);
		if (acb->t6_iov[T6_INTEG_LABEL].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_SESSION_ID) {
		acb->t6_iov[T6_SESSION_ID].iov_len =
			t6_attr_size[T6_SESSION_ID];
		acb->t6_iov[T6_SESSION_ID].iov_base =
			malloc(t6_attr_size[T6_SESSION_ID]);
		if (acb->t6_iov[T6_SESSION_ID].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_CLEARANCE) {
		acb->t6_iov[T6_CLEARANCE].iov_len = t6_attr_size[T6_CLEARANCE];
		acb->t6_iov[T6_CLEARANCE].iov_base =
			malloc(t6_attr_size[T6_CLEARANCE]);
		if (acb->t6_iov[T6_CLEARANCE].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_ACL) {
		acb->t6_iov[T6_ACL].iov_len = t6_attr_size[T6_ACL];
		acb->t6_iov[T6_ACL].iov_base = malloc(t6_attr_size[T6_ACL]);
		if (acb->t6_iov[T6_ACL].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_IL) {
		/* unsupported attribute */
		acb->t6_alloc_mask &= ~T6M_IL;
		acb->t6_iov[T6_IL].iov_len = t6_attr_size[T6_IL];
		acb->t6_iov[T6_IL].iov_base = NULL;
	}

	if (acb->t6_alloc_mask & T6M_PRIVILEGES) {  /* AKA capability */
		acb->t6_iov[T6_PRIVILEGES].iov_len =
			t6_attr_size[T6_PRIVILEGES];
		acb->t6_iov[T6_PRIVILEGES].iov_base =
			malloc(t6_attr_size[T6_PRIVILEGES]);
		if (acb->t6_iov[T6_PRIVILEGES].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_AUDIT_ID) {
		acb->t6_iov[T6_AUDIT_ID].iov_len = t6_attr_size[T6_AUDIT_ID];
		acb->t6_iov[T6_AUDIT_ID].iov_base =
			malloc(t6_attr_size[T6_AUDIT_ID]);
		if (acb->t6_iov[T6_AUDIT_ID].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_PID) {
		acb->t6_iov[T6_PID].iov_len = t6_attr_size[T6_PID];
		acb->t6_iov[T6_PID].iov_base = malloc(t6_attr_size[T6_PID]);
		if (acb->t6_iov[T6_PID].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_RESV10) {
		acb->t6_alloc_mask &= ~T6M_RESV10;
		acb->t6_iov[T6_RESV10].iov_len = t6_attr_size[T6_RESV10];
		acb->t6_iov[T6_RESV10].iov_base = NULL;
	}

	if (acb->t6_alloc_mask & T6M_AUDIT_INFO) {
		acb->t6_iov[T6_AUDIT_INFO].iov_len =
			t6_attr_size[T6_AUDIT_INFO];
		acb->t6_iov[T6_AUDIT_INFO].iov_base =
			malloc(t6_attr_size[T6_AUDIT_INFO]);
		if (acb->t6_iov[T6_AUDIT_INFO].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_UID) {
		acb->t6_iov[T6_UID].iov_len = t6_attr_size[T6_UID];
		acb->t6_iov[T6_UID].iov_base = malloc(t6_attr_size[T6_UID]);
		if (acb->t6_iov[T6_UID].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_GID) {
		acb->t6_iov[T6_GID].iov_len = t6_attr_size[T6_GID];
		acb->t6_iov[T6_GID].iov_base = malloc(t6_attr_size[T6_GID]);
		if (acb->t6_iov[T6_GID].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_GROUPS) {
		acb->t6_iov[T6_GROUPS].iov_len = t6_attr_size[T6_GROUPS];
		acb->t6_iov[T6_GROUPS].iov_base =
			malloc(t6_attr_size[T6_GROUPS]);
		if (acb->t6_iov[T6_GROUPS].iov_base == NULL)
			goto fail;
	}

	if (acb->t6_alloc_mask & T6M_PROC_ATTR) {
		/* unsupported attribute */
		acb->t6_alloc_mask &= ~T6M_PROC_ATTR;
		acb->t6_iov[T6_PROC_ATTR].iov_len = t6_attr_size[T6_PROC_ATTR];
		acb->t6_iov[T6_PROC_ATTR].iov_base = NULL;
	}

	return((t6attr_t) acb);
fail:
	t6free_blk(acb);
	setoserror(error);
	return(NULL);
}

t6attr_t
t6attr_alloc(void)
{
	return(t6alloc_blk(T6M_ALL_ATTRS));
}

t6attr_t
t6create_attr(t6mask_t alloc_mask)
{
	return(t6alloc_blk(alloc_mask));
}

void 
t6free_blk(t6attr_t t6ctl)
{
	t6ctl_t *acb = (t6ctl_t *) t6ctl;
	int i;

	/* Free any space allocated for attributes */
	for (i = 0; i < T6_MAX_ATTRS; i++)
		if (acb->t6_iov[i].iov_base != NULL)
			free(acb->t6_iov[i].iov_base);

	/* Free the control block */
	free(acb);
}

void
t6free_attr(t6attr_t t6ctl)
{
	t6free_blk(t6ctl);
}

void 
t6copy_blk(const t6attr_t attr_src, t6attr_t attr_dest)
{
	t6ctl_t *src_acb = (t6ctl_t *) attr_src;
	t6ctl_t *dst_acb = (t6ctl_t *) attr_dest;
	t6mask_t copy_mask;
	int i;

	/* Check that pointers are not NULL */
	if (src_acb == NULL || dst_acb == NULL)
		return;

	/* Check there are common attributes */
	copy_mask = src_acb->t6_valid_mask & dst_acb->t6_alloc_mask;
	if (copy_mask == T6M_NO_ATTRS)
		return;

	/* Find common attributes and copy. */
	dst_acb->t6_valid_mask = T6M_NO_ATTRS;
	for (i = 0; i < T6_MAX_ATTRS; i++) {
		if ((t6_mask(i) & copy_mask) == T6M_NO_ATTRS)
			continue;
		if (src_acb->t6_iov[i].iov_len > dst_acb->t6_iov[i].iov_len)
			continue;
		memcpy(dst_acb->t6_iov[i].iov_base,
		       src_acb->t6_iov[i].iov_base,
		       src_acb->t6_iov[i].iov_len);
		dst_acb->t6_valid_mask |= t6_mask(i);
		dst_acb->t6_iov[i].iov_len = src_acb->t6_iov[i].iov_len;
	}
}

void
t6copy_attr(const t6attr_t attr_src, t6attr_t attr_dest)
{
	t6copy_blk(attr_src, attr_dest);
}

t6attr_t
t6dup_blk(const t6attr_t attr_src)
{
	t6ctl_t *src_acb = (t6ctl_t *) attr_src;
	t6attr_t new;

	if (src_acb == NULL)
		return(NULL);

	if ((new = t6alloc_blk(src_acb->t6_valid_mask)) == NULL)
		return(NULL);

	t6copy_blk(attr_src, new);
	return(new);
}

t6attr_t
t6dup_attr(const t6attr_t attr_src)
{
	return(t6dup_blk(attr_src));
}

void
t6clear_blk(t6attr_t t6ctl, t6mask_t vmask)
{
	t6ctl_t *acb = (t6ctl_t *) t6ctl;

	if (acb == NULL)
		return;
	acb->t6_valid_mask &= ~vmask;
}

void *
t6get_attr(t6attr_id_t attr_type, const t6attr_t t6ctl)
{
	t6ctl_t *acb = (t6ctl_t *) t6ctl;

	/* check arguments */
	if (acb == NULL || attr_type < 0 || attr_type >= T6_MAX_ATTRS ||
	    (t6_mask(attr_type) & acb->t6_valid_mask) == T6M_NO_ATTRS)
		return(NULL);

	return((void *) acb->t6_iov[attr_type].iov_base);
}

static int
t6groups_valid(const t6groups_t *grp)
{
	__uint32_t i;

	if (grp->ngroups > NGROUPS_UMAX)
		return(0);
	for (i = 0; i < grp->ngroups; i++)
		if (grp->groups[i] < 0 || grp->groups[i] > MAXUID)
			return(0);
	return(1);
}

int
t6set_attr(t6attr_id_t attr_type, const void *attr, t6attr_t t6ctl)
{
	t6ctl_t *acb = (t6ctl_t *) t6ctl;
	t6mask_t mask;

	/* check validity of arguments */
	if (acb == NULL || attr == NULL)
		goto fail;
	mask = t6_mask(attr_type) & acb->t6_alloc_mask;
	if (mask == T6M_NO_ATTRS)
		goto fail;

	/*
	 * Check validity of individual attributes, if possible
	 */
	if (mask & T6M_SL) {
		if (!msen_valid((msen_t) attr))
			goto fail;
	}

	if (mask & T6M_NAT_CAVEATS) {
		/* do nothing for now */
	}

	if (mask & T6M_INTEG_LABEL) {
		if (!mint_valid((mint_t) attr))
			goto fail;
	}

	if (mask & T6M_SESSION_ID) {
		/* do nothing for now */
	}

	if (mask & T6M_CLEARANCE) {
		if (!msen_valid((msen_t) attr))
			goto fail;
	}

	if (mask & T6M_ACL) {
		if (!acl_valid((acl_t) attr))
			goto fail;
	}

	if (mask & T6M_IL) {
		/* unsupported attribute */
		goto fail;
	}

	if (mask & T6M_PRIVILEGES) {
		/* these seem to be always valid */
	}

	if (mask & T6M_AUDIT_ID) {
		const uid_t *u = (const uid_t *) attr;
		if (*u < 0 || *u > MAXUID)
			goto fail;
	}

	if (mask & T6M_PID) {
		const pid_t *p = (const pid_t *) attr;
		if (*p < 0 || *p > MAXPID)
			goto fail;
	}

	if (mask & T6M_RESV10) {
		/* unsupported attribute */
		goto fail;
	}

	if (mask & T6M_AUDIT_INFO) {
		/* do nothing for now */
	}

	if (mask & T6M_UID) {
		const uid_t *u = (const uid_t *) attr;
		if (*u < 0 || *u > MAXUID)
			goto fail;
	}

	if (mask & T6M_GID) {
		const gid_t *gr = (const gid_t *) attr;
		if (*gr < 0 || *gr > MAXUID)
			goto fail;
	}

	if (mask & T6M_GROUPS) {
		if (!t6groups_valid((const t6groups_t *) attr))
			goto fail;
	}

	memcpy(acb->t6_iov[attr_type].iov_base, attr,
	       acb->t6_iov[attr_type].iov_len);
	acb->t6_valid_mask |= t6_mask(attr_type);
	return(0);
fail:
	setoserror(EINVAL);
	return(-1);
}

size_t
t6size_attr(t6attr_id_t attr_type, const t6attr_t t6ctl)
{
	t6ctl_t *acb = (t6ctl_t *) t6ctl;

	/* Check if in range */
	if (attr_type < 0 || attr_type >= T6_MAX_ATTRS)
		return(0);

	/* Max value from table */
	if (acb == NULL)
		return(t6_attr_size[attr_type]);
	
#if 0
	/* should this be t6_alloc_mask instead? */
	if ((t6_mask(attr_type) & acb->t6_valid_mask) == T6M_NO_ATTRS)
		return(0);
#endif

	/* Actual value, which currently is same as max value */
	return((size_t) acb->t6_iov[attr_type].iov_len);
}

/*
 *  The following are the library entry points for the session
 *  manager system call.
 */

int
t6ext_attr(int fd, t6cmd_t cmd)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6EXT_ATTR_CMD),
			fd,cmd);
}

int
t6new_attr(int fd, t6cmd_t cmd)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6NEW_ATTR_CMD),
			fd,cmd);
}

int
t6mls_socket(int fd, t6cmd_t cmd)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6MLS_SOCKET_CMD),
			fd,cmd);
}

int
t6get_endpt_mask(int fd, t6mask_t *mask)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6GET_ENDPT_MASK_CMD),
			fd,mask);
}

int
t6set_endpt_mask(int fd, t6mask_t mask)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6SET_ENDPT_MASK_CMD),
			fd,mask);
}

int
t6get_endpt_default(int fd, t6mask_t *mask, t6attr_t attr)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6GET_ENDPT_DEFAULT_CMD),
			fd, mask, attr);
}

int
t6set_endpt_default(int fd, t6mask_t mask, const t6attr_t attr_ptr)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6SET_ENDPT_DEFAULT_CMD),
			fd, mask, attr_ptr);
}

int
t6last_attr(int fd, t6attr_t attr_ptr, t6mask_t *new_attrs)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6LAST_ATTR_CMD),
			fd, attr_ptr, new_attrs);
}

int
t6peek_attr(int fd, t6attr_t attr_ptr, t6mask_t *new_attrs)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6PEEK_ATTR_CMD),
			fd, attr_ptr, new_attrs);

}

int
t6recvfrom(int fd, char *buf, int len, int flags, 
			struct sockaddr *from, int *fromlen, 
			t6attr_t attr_ptr, t6mask_t *new_attrs)
{
	t6parms_t parms;

	parms.attrp = (t6ctl_t *) attr_ptr;
	parms.maskp = new_attrs;
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6RECVFROM_CMD),
			fd, buf, len, flags, from, fromlen, &parms);
}

int
t6sendto(int fd, const char *msg, int len, int flags,
		           const struct sockaddr *to, int tolen, 
			   const t6attr_t attr_ptr)
{
	return (int)sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6SENDTO_CMD),
			fd, msg, len, flags, to, tolen, attr_ptr);
}

int
t6rhdb_put_host(size_t size, caddr_t data)
{
	return (int) sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6RHDB_PUT_HOST_CMD),
			size, data);
}

int
t6rhdb_get_host(struct in_addr *addr, size_t size, caddr_t data)
{
	return (int) sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6RHDB_GET_HOST_CMD),
			addr, size, data);
}

int
t6rhdb_flush(const struct in_addr *addr)
{
	return (int) sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6RHDB_FLUSH_CMD),
			addr);
}

int
t6rhdb_stat(caddr_t data)
{
	return (int) sgi_sesmgr(
			SESMGR_MAKE_CMD(SESMGR_TSIX_ID,T6RHDB_STAT_CMD), data);
}

/*
 *  t6cmp_attrs() - compare the two sets of attributes
 *
 *	John Elliott IV <iv@engr.sgi.COM>
 *
 *	Returns 1 if they are the same
 *	Returns 0 if they are not the same
 *	Returns -1 on error
 *
 */
int
t6cmp_attrs(const t6attr_t *arg1, const t6attr_t *arg2)
{
	const t6ctl_t	*a1, *a2;
	t6mask_t mask;
	msen_t	msen1, msen2;
	mint_t	mint1, mint2;

	/* Make sure we have valid pointers */
	if (arg1 == 0 || arg2 == 0) {
		errno = EINVAL;
		return -1;
	}

	a1 = (const t6ctl_t *)arg1;
	a2 = (const t6ctl_t *)arg2;

	/* If the attributes present are not the same, they don't compare */
	if (a1->t6_valid_mask != a2->t6_valid_mask)
		return 0;

	/* Get a copy of the mask of attributes present
	 * (we know they're both the same at this point)
	 */
	mask = a1->t6_valid_mask;

	if (mask & t6_mask(T6_UID)) 
	{
		uid_t	*uid1, *uid2;

		uid1 = (uid_t *)a1->t6_iov[T6_UID].iov_base;
		uid2 = (uid_t *)a2->t6_iov[T6_UID].iov_base;
		if (*uid1 != *uid2)
			return 0;
	}

	if (mask & t6_mask(T6_GID)) 
	{
		gid_t	*gid1, *gid2;

		gid1 = (gid_t *)a1->t6_iov[T6_GID].iov_base;
		gid2 = (gid_t *)a2->t6_iov[T6_GID].iov_base;
		if (*gid1 != *gid2)
			return 0;
	}

	if (mask & t6_mask(T6_SESSION_ID)) 
	{
		uid_t	*sid1, *sid2;

		sid1 = (uid_t *)a1->t6_iov[T6_SESSION_ID].iov_base;
		sid2 = (uid_t *)a2->t6_iov[T6_SESSION_ID].iov_base;
		if (*sid1 != *sid2)
			return 0;
	}

	if (mask & t6_mask(T6_PID)) 
	{
		pid_t	*pid1, *pid2;

		pid1 = (pid_t *)a1->t6_iov[T6_PID].iov_base;
		pid2 = (pid_t *)a2->t6_iov[T6_PID].iov_base;
		if (*pid1 != *pid2)
			return 0;
	}

	if (mask & t6_mask(T6_PRIVILEGES))
	{
		cap_t	cap1, cap2;

		cap1 = (cap_t)a1->t6_iov[T6_PRIVILEGES].iov_base;
		cap2 = (cap_t)a2->t6_iov[T6_PRIVILEGES].iov_base;

		if (cap1->cap_effective != cap2->cap_effective
		    || cap1->cap_permitted != cap2->cap_permitted
		    || cap1->cap_inheritable != cap2->cap_inheritable)
		{
			return 0;
		}
	}

	if (mask & t6_mask(T6_GROUPS)) 
	{
		t6groups_t  *t6gp1, *t6gp2;
		gid_t	*gp1, *gp2, *ep;
		int	sz1, sz2;

		t6gp1 = (t6groups_t *)a1->t6_iov[T6_GROUPS].iov_base;
		t6gp2 = (t6groups_t *)a2->t6_iov[T6_GROUPS].iov_base;
		gp1 = t6gp1->groups;
		gp2 = t6gp2->groups;
		sz1 = t6gp1->ngroups;
		sz2 = t6gp2->ngroups;

		if (sz1 != sz2)
			return 0;

		ep = &gp1[sz1];

		for ( ; gp1 < ep; ++gp1, ++gp2) {
			if (*gp1 != *gp2)
			    return 0;
		}
	}

	/* Compare labels, if present */
	if (mask & t6_mask(T6_SL)) {
		msen1 = (msen_t)a1->t6_iov[T6_SL].iov_base;
		msen2 = (msen_t)a2->t6_iov[T6_SL].iov_base;
		if (msen1->b_type != msen2->b_type || !msen_equal(msen1, msen2))
			return 0;
	}
	if (mask & t6_mask(T6_INTEG_LABEL)) {
		mint1 = (mint_t)a1->t6_iov[T6_INTEG_LABEL].iov_base;
		mint2 = (mint_t)a2->t6_iov[T6_INTEG_LABEL].iov_base;
		if (mint1->b_type != mint2->b_type || !mint_equal(mint1, mint2))
			return 0;
	}
	if (mask & t6_mask(T6_CLEARANCE)) {
		msen1 = (msen_t)a1->t6_iov[T6_CLEARANCE].iov_base;
		msen2 = (msen_t)a2->t6_iov[T6_CLEARANCE].iov_base;
		if (msen1->b_type != msen2->b_type || !msen_equal(msen1, msen2))
			return 0;
	}


	/* The following entries are ignored:
	 *	T6_ACL
	 *	T6_NAT_CAVEATS
	 *	T6_IL
	 *	T6_AUDIT_ID
	 *	T6_AUDIT_INFO
	 */


	return 1;
}

int
t6cmp_blk(const t6attr_t *arg1, const t6attr_t *arg2)
{
	return (t6cmp_attrs(arg1, arg2));
}
