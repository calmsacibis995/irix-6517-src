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
 *	SGI Session Manager - TSIX Remote Host Database.
 */

#ident	"$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socketvar.h>
#include <sys/kmem.h>

#include <sys/sat.h>
#include <sys/acl.h>
#include <sys/mac.h>

#include <sys/t6attrs.h>
#include <sys/sesmgr.h>
#include <sys/t6api_private.h>
#include <sys/t6rhdb.h>
#include <sys/atomic_ops.h>
#include "t6refcnt.h"

extern char *inet_ntoa(struct in_addr);

/*
 *  The remote host table is indexed by taking the host ip address (or network
 *  address) and masking off the high address bits. Hash buckets are used for
 *  resolving collisions.
 */

static struct hashinfo rhdb_info;
static zone_t *rhdb_zone;

void
t6rhdb_init(void)
{
	static struct hashtable rhdb_tbl[HASHTABLE_SIZE];

	hash_init(&rhdb_info, rhdb_tbl, HASHTABLE_SIZE,
		  sizeof (struct in_addr), HASH_MASK);
	rhdb_zone = kmem_zone_init(sizeof(t6rhdb_host_hash_t), "rhdb table");
	ASSERT(rhdb_zone != NULL);
}

/*
 * match RHDB entry by host address with reference counting
 */
/* ARGSUSED */
static int
rhdb_match_addr (struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	t6rhdb_host_hash_t *hh = (t6rhdb_host_hash_t *) h;
	const struct in_addr *addr = (struct in_addr *) key;

	ASSERT (hh != NULL);
	ASSERT (addr != NULL);

	if (!REF_INVALID(hh->hh_refcnt) && hh->hh_addr.s_addr == addr->s_addr)
	{
		(void) atomicAddUint (&hh->hh_refcnt, 1);
		return (1);
	}
	return (0);
}

/* ARGSUSED */
static int
rhdb_refcnt (struct hashbucket *h, caddr_t arg1, caddr_t arg2)
{
	t6rhdb_host_hash_t *hh = (t6rhdb_host_hash_t *) h;
	unsigned int refcnt = atomicAddUint (&hh->hh_refcnt, -1);

	return (REF_CNT(refcnt) == 0);
}

static void
rhdb_rele (struct hashbucket *h)
{
	t6rhdb_host_hash_t *hh = (t6rhdb_host_hash_t *) h;

	if (hh == NULL)
		return;

	spinlock_destroy(&hh->hh_lock);
	kmem_zone_free (rhdb_zone, hh);
}

static int
t6find_host_attr(struct in_addr *host_addr, t6rhdb_kern_buf_t *kb)
{
	t6rhdb_host_hash_t *hh;

	hh = (t6rhdb_host_hash_t *) hash_lookup(&rhdb_info, rhdb_match_addr, (caddr_t) host_addr, (caddr_t) NULL, (caddr_t) NULL);
	if (hh != NULL)
	{
		int s = mutex_spinlock(&hh->hh_lock);
		bcopy (&hh->hh_profile, kb, sizeof (*kb));
		mutex_spinunlock(&hh->hh_lock, s);
		rhdb_rele (hash_refcnt (&rhdb_info, rhdb_refcnt,
					&hh->hh_hash));
		return (1);
	}
	return (0);
}

#define HOST_ALL	0
#define HOST_ONLY	1

int
t6findhost(struct in_addr *host_addr, int flag, t6rhdb_kern_buf_t *kb)
{
	struct in_addr naddr;

	ASSERT (host_addr != NULL);
	ASSERT (kb != NULL);

	/* First try supplied address */
	if (t6find_host_attr(host_addr, kb))
		return (1);
	if (flag == HOST_ONLY)
		return (0);

	/* try class C wildcard */
	naddr.s_addr = host_addr->s_addr & IN_CLASSC_NET;
	if (t6find_host_attr(&naddr, kb))
		return (1);

	/* try class B wildcard */
	naddr.s_addr = host_addr->s_addr & IN_CLASSB_NET;
	if (t6find_host_attr(&naddr, kb))
		return (1);

	/* try class A wildcard */
	naddr.s_addr = host_addr->s_addr & IN_CLASSA_NET;
	if (t6find_host_attr(&naddr, kb))
		return (1);

	/* try generic wildcard */
	naddr.s_addr = 0;
	return t6find_host_attr(&naddr, kb);
}

static int
t6rhdb_add_one_host(struct in_addr *host_addr, t6rhdb_kern_buf_t *profile)
{
	t6rhdb_host_hash_t *hh;

	ASSERT(host_addr != NULL);
	ASSERT(profile != NULL);

#ifdef DEBUG
	printf("Adding host %s\n", inet_ntoa(*host_addr));
#endif

	/* find matching entry */
	hh = (t6rhdb_host_hash_t *) hash_lookup (&rhdb_info, rhdb_match_addr, (caddr_t) host_addr, (caddr_t) NULL, (caddr_t) NULL);
	if (hh != NULL) {
		/* replace contents of hh with new profile */
		int s = mutex_spinlock(&hh->hh_lock);
		bcopy(profile, &hh->hh_profile, sizeof(hh->hh_profile));
		mutex_spinunlock(&hh->hh_lock, s);

		/* release entry */
		rhdb_rele (hash_refcnt (&rhdb_info, rhdb_refcnt,
					&hh->hh_hash));

		/* return success */
		return 1;
	}

	/* we failed above, so allocate new entry */
	hh = kmem_zone_alloc(rhdb_zone, KM_SLEEP);
	if (hh == NULL)
		return 0;

	/* initialize hh */
	hh->hh_addr.s_addr = host_addr->s_addr;
	hh->hh_flags = 0;
	hh->hh_refcnt = 1;
	bcopy(profile, &hh->hh_profile, sizeof(hh->hh_profile));
	spinlock_init(&hh->hh_lock, "rhdb host entry lock");

	/* insert hh into table */
	hash_insert (&rhdb_info, &hh->hh_hash, (caddr_t) host_addr);

	/* return success */
	return 1;
}

/*
 *  System calls for the remote host data base.
 */

/*ARGSUSED*/
int
t6rhdb_flush(struct in_addr *host_addr, rval_t *rvp)
{
	t6rhdb_host_hash_t *hh;
	struct in_addr laddr;

	/* check for appropriate privilege */
	if (!_CAP_ABLE(CAP_NETWORK_MGT))
		return EPERM;

	if (copyin(host_addr, &laddr, sizeof(laddr)))
		return EFAULT;

	hh = (t6rhdb_host_hash_t *) hash_lookup (&rhdb_info, rhdb_match_addr, (caddr_t) &laddr, (caddr_t) NULL, (caddr_t) NULL);
	if (hh == NULL)
		return EINVAL;

	/* mark for deletion */
	(void) atomicAddUint (&hh->hh_refcnt, REF_CNT_INVALID - 1);

	/* release entry */
	rhdb_rele (hash_refcnt (&rhdb_info, rhdb_refcnt, &hh->hh_hash));

	/* AOK */
	return (0);
}

int
t6rhdb_stat(caddr_t ubp)
{
	t6rhdb_rstat_t sb;
	int error = 0;

	sb.host_cnt = 0;
	sb.host_size = 0;
	sb.profile_cnt = 0;
	sb.profile_size = 0;

        if (copyout(&sb, ubp, sizeof(sb)))
                error = EFAULT;
	return error;
}

/*
 *  Add a host profile to the remote host database.  The data block
 *  that is passed in begins with a fixed format structure that is
 *  in the format that it will be stored.  That is followed by a
 *  variable number of IP addresses and label ranges.
 */

/*ARGSUSED*/
void
t6rhdb_host_to_kern(t6rhdb_host_buf_t *hb, t6rhdb_kern_buf_t *kb)
{
	kb->hp_smm_type = hb->hp_smm_type;
	kb->hp_nlm_type = hb->hp_nlm_type;
	kb->hp_auth_type = hb->hp_auth_type;
	kb->hp_encrypt_type = hb->hp_encrypt_type;
	kb->hp_attributes = hb->hp_attributes;
	kb->hp_flags = hb->hp_flags;
	kb->hp_cache_size = hb->hp_cache_size;
	kb->hp_host_cnt = hb->hp_host_cnt;
	bcopy(&hb->hp_def_priv, &kb->hp_def_priv, sizeof (hb->hp_def_priv));
	bcopy(&hb->hp_max_priv, &kb->hp_max_priv, sizeof (hb->hp_max_priv));

	/* copy sensitivity labels */
	if (msen_valid(&hb->hp_def_sl))
		kb->hp_def_sl = msen_add_label(&hb->hp_def_sl);
	else
		kb->hp_def_sl = NULL;
	if (msen_valid(&hb->hp_min_sl))
		kb->hp_min_sl = msen_add_label(&hb->hp_min_sl);
	else
		kb->hp_min_sl = NULL;
	if (msen_valid(&hb->hp_max_sl))
		kb->hp_max_sl = msen_add_label(&hb->hp_max_sl);
	else
		kb->hp_max_sl = NULL;

	/* copy integrity labels */
	if (mint_valid(&hb->hp_def_integ))
		kb->hp_def_integ = mint_add_label(&hb->hp_def_integ);
	else
		kb->hp_def_integ = NULL;
	if (mint_valid(&hb->hp_min_integ))
		kb->hp_min_integ = mint_add_label(&hb->hp_min_integ);
	else
		kb->hp_min_integ = NULL;
	if (mint_valid(&hb->hp_max_integ))
		kb->hp_max_integ = mint_add_label(&hb->hp_max_integ);
	else
		kb->hp_max_integ = NULL;

	/* copy ilb label */
	if (msen_valid(&hb->hp_def_ilb))
		kb->hp_def_ilb = msen_add_label(&hb->hp_def_ilb);
	else
		kb->hp_def_ilb = NULL;

	/* copy ilb labels */
	if (msen_valid(&hb->hp_def_clearance))
		kb->hp_def_clearance = msen_add_label(&hb->hp_def_clearance);
	else
		kb->hp_def_clearance = NULL;

	kb->hp_def_sid = hb->hp_def_sid;
	kb->hp_def_uid = hb->hp_def_uid;
	kb->hp_def_luid = hb->hp_def_luid;
	kb->hp_def_gid = hb->hp_def_gid;
	kb->hp_def_grp_cnt = hb->hp_def_grp_cnt;
	bcopy(hb->hp_def_groups, kb->hp_def_groups, sizeof(hb->hp_def_groups));
}

static void
t6rhdb_kern_to_host(t6rhdb_kern_buf_t *kb, t6rhdb_host_buf_t *hb)
{
	bzero(hb, sizeof(*hb));
	
	hb->hp_smm_type = kb->hp_smm_type;
	hb->hp_nlm_type = kb->hp_nlm_type;
	hb->hp_auth_type = kb->hp_auth_type;
	hb->hp_encrypt_type = kb->hp_encrypt_type;
	hb->hp_attributes = kb->hp_attributes;
	hb->hp_flags = kb->hp_flags;
	hb->hp_cache_size = kb->hp_cache_size;
	hb->hp_host_cnt = kb->hp_host_cnt;
	bcopy(&kb->hp_def_priv, &hb->hp_def_priv, sizeof (hb->hp_def_priv));
	bcopy(&kb->hp_max_priv, &hb->hp_max_priv, sizeof (hb->hp_max_priv));

	/* copy sensitivity labels */
	if (kb->hp_def_sl != NULL)
		bcopy(kb->hp_def_sl, &hb->hp_def_sl, sizeof(hb->hp_def_sl));
	if (kb->hp_min_sl != NULL)
		bcopy(kb->hp_min_sl, &hb->hp_min_sl, sizeof(hb->hp_min_sl));
	if (kb->hp_max_sl != NULL)
		bcopy(kb->hp_max_sl, &hb->hp_max_sl, sizeof(hb->hp_max_sl));

	/* copy integrity labels */
	if (kb->hp_def_integ != NULL)
		bcopy(kb->hp_def_integ, &hb->hp_def_integ,
		      sizeof(hb->hp_def_integ));
	if (kb->hp_min_integ != NULL)
		bcopy(kb->hp_min_integ, &hb->hp_min_integ,
		      sizeof(hb->hp_min_integ));
	if (kb->hp_max_integ != NULL)
		bcopy(kb->hp_max_integ, &hb->hp_max_integ,
		      sizeof(hb->hp_max_integ));

	/* copy ilb label */
	if (kb->hp_def_ilb != NULL)
		bcopy(kb->hp_def_ilb, &hb->hp_def_ilb,
		      sizeof(hb->hp_def_ilb));

	/* copy ilb labels */
	if (kb->hp_def_clearance != NULL)
		bcopy(kb->hp_def_clearance, &hb->hp_def_clearance,
		      sizeof(hb->hp_def_clearance));

	hb->hp_def_sid = kb->hp_def_sid;
	hb->hp_def_uid = kb->hp_def_uid;
	hb->hp_def_luid = kb->hp_def_luid;
	hb->hp_def_gid = kb->hp_def_gid;
	hb->hp_def_grp_cnt = kb->hp_def_grp_cnt;
	bcopy(hb->hp_def_groups, kb->hp_def_groups, sizeof(hb->hp_def_groups));
}

/*ARGSUSED*/
int
t6rhdb_put_host(size_t size, caddr_t data, rval_t *rvp)
{
	int error = 0, i;
	t6rhdb_host_buf_t hb;
	t6rhdb_kern_buf_t kb;
	char *bp = NULL;
	char *dp;

	/* check for appropriate privilege */
	if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
		error = EPERM;
		goto out;
	}

	/* check argument validity */
	if (size < sizeof(hb) || size > 0xffff) {
		error = EINVAL;
		goto out;
	}

	/* allocate host profile buffer */
	if ((bp = kmem_alloc(size, KM_SLEEP)) == NULL) {
		error = ENOMEM;
		goto out;
	}

#ifdef DEBUG
	printf("\nt6rhdb_add_host called: size=%d, addr=0x%08x\n",size,data);
#endif

	/* Copy buffer containing host profile */
	if (copyin(data, bp, size)) {
		error = EFAULT;
		goto out;
	}

	/* copy host profile */
	bcopy(bp, &hb, sizeof(hb));

	/* create in-kernel representation */
	t6rhdb_host_to_kern(&hb, &kb);

	/* Add each host. If it already exists we just replace the old
	   entry with the new one */
	dp = bp + sizeof(hb);
	for (i = 0; i < hb.hp_host_cnt; i++) {
		struct in_addr *ap;

		ap = (struct in_addr *) dp;
		if (!t6rhdb_add_one_host(ap, &kb)) {
			error = EINVAL;
			goto out;
		}
		dp += sizeof(*ap);
	}

out:
	if (bp != NULL)
		kmem_free(bp, size);
	return error;
}

/*ARGSUSED*/
int
t6rhdb_get_host(struct in_addr *addr, size_t size, caddr_t data, rval_t *rvp)
{
	t6rhdb_kern_buf_t kb;
	t6rhdb_host_buf_t hb;
	struct in_addr laddr;

	if (copyin(addr, &laddr, sizeof(laddr)))
		return EFAULT;

#ifdef DEBUG
	printf("t6rhdb_get_host called for %s\n", inet_ntoa(laddr));
#endif

	if (t6findhost(&laddr, HOST_ALL, &kb)) {
		t6rhdb_kern_to_host(&kb, &hb);
		if (copyout(&hb, data, sizeof(hb)))
			return EFAULT;
		return 0;
	} else {
		return EINVAL;
	}
}
