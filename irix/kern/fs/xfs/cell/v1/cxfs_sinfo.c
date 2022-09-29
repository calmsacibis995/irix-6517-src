/**************************************************************************
 *									  *
 *		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.1 $"

/*
 * Management of cxfs_sinfo structures for Array mount support
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/uuid.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/utsname.h>
#include <sys/systm.h>
#include <ksys/behavior.h>
#include "fs/xfs/xfs_types.h"
#include "fs/xfs/xfs_error.h"
#include "fs/xfs/xfs_inum.h"
#include "fs/xfs/xfs_log.h"
#include "fs/xfs/xfs_cxfs.h"
#include "fs/xfs/xfs_sb.h"
#include "fs/xfs/xfs_trans.h"
#include "fs/xfs/xfs_mount.h"
#include "fs/xfs/cxfs_clnt.h"
#include "cxfs.h"
#include "cxfs_array.h"
#include "cxfs_sinfo.h"

mrlock_t 	cxfs_sinfo_lock;
kqueuehead_t    cxfs_sinfo_head;
int             cxfs_setup_done;

/*
 * Allocate and do initial fill-in for a cxfs_sinfo
 */
cxfs_sinfo_t *	
cxfs_sinfo_create(
	struct xfs_args	*ap,
	xfs_mount_t 	*mp,
	size_t 		stringlen)
{
	int             tablelen;
	int             baselen;
        int             totlen;
	cxfs_sinfo_t    *cxsp;

	/*
         * First compute the rest of the length information for the 
         * cxfs_info_t.  In addition to the defined structures, there is 
         * table of hostname pointers and space for the strings themselves.
	 */
	tablelen = ap->scount * sizeof(char *);
	baselen = sizeof(cxfs_sinfo_t) + sizeof(char *) + 1 ;
	baselen &= (~(sizeof(char *) - 1));
	totlen = baselen + tablelen + stringlen;

	/* 
         * Allocate the the cxfs_info_t and fill it in.
	 */
	cxsp = (cxfs_sinfo_t *) kmem_zalloc(totlen, KM_SLEEP);
	cxsp->cxs_length = totlen;
	cxsp->cxs_addrs = (char **) (((char *) cxsp) + baselen);
	cxsp->cxs_strings = ((char *) cxsp) + (baselen + tablelen);
        cxsp->cxs_uuid = mp->m_sb.sb_uuid;	
	cxsp->cxs_digest = cxfs_sinfo_digest(&cxsp->cxs_uuid);
        cxsp->cxs_count = ap->scount;
	cxsp->cxs_mount = mp;
	cxsp->cxs_trying = 1;

	/*
         * Copy in the arguments and get rid of those that will not be 
         * compared.
	 */
	bcopy(ap, &cxsp->cxs_args, sizeof(struct xfs_args));
	cxsp->cxs_args.version = 0;
	cxsp->cxs_args.fsname = NULL;
	cxsp->cxs_args.servers = NULL;
	cxsp->cxs_args.servlen = NULL;
	cxsp->cxs_args.uuid = NULL;
	cxsp->cxs_args.flags &= ~(XFSMNT_CLNTONLY | XFSMNT_TESTUUID);
	cxsp->cxs_args.scount = 0;
	cxsp->cxs_args.stimeout = 0;
	cxsp->cxs_args.ctimeout = 0;
	return (cxsp);
}

/*
 * Compute uuid digest
 */
ulong_t
cxfs_sinfo_digest(
	uuid_t 		*idp)
{
        ulong_t		digest = 0;
	uchar_t         *p;
	int             i;

	p = (uchar_t *) &digest;
	for (i = 0; i < sizeof(uuid_t); i++) 
		p[i % sizeof(ulong_t)] ^= idp->__u_bits[i];
	return (digest);
}

/*
 * Check for our hostname in cxfs_sinfo_t
 *
 * This routine checks for our own host name in the server_list within a
 * cxfs_sinfo_t.  It sets cxs_order to the appropriate value.  If we do
 * not appear in the server list, making the current mount not server-
 * capable, the value minus one is stored.
 */
void
cxfs_find_self(
	cxfs_sinfo_t	*cxsp)		/* cxfs_sinfo_t to be checked. */
{
        int 		i;
        char            **hpp;

	for (i = 0, hpp = cxsp->cxs_addrs;
	     i < cxsp->cxs_count;
	     i++, hpp++) {
		if (strcmp(utsname.nodename, *hpp) == 0)
			break;
	}

	if (i == cxsp->cxs_count)
		i = -1;
	cxsp->cxs_order = i;
}

/*
 * Find a vfs corresponding to a uuid
 */

int
cxfs_sinfo_findvfs(
	uuid_t		*idp,
	struct xfs_args *ap,
        vfs_t           **vfspp)
{
        ulong_t         digest = cxfs_sinfo_digest(idp);
	cxfs_sinfo_t    *cxsp;
        uint_t          dce_garbage;
        int             times = 0;

start:
	*vfspp = NULL;
	if (times++)
		mrupdate(&cxfs_sinfo_lock);
	else
		mraccess(&cxfs_sinfo_lock);
	for (cxsp = (cxfs_sinfo_t *) kqueue_first(&cxfs_sinfo_head);
	     cxsp != (cxfs_sinfo_t *) &cxfs_sinfo_head;
	     cxsp = (cxfs_sinfo_t *) kqueue_next(&cxsp->cxs_queue)) {
		if (cxsp->cxs_digest != digest)
			continue;
		if (uuid_equal(idp, &cxsp->cxs_uuid, &dce_garbage)) {
			xfs_mount_t	*mp = cxsp->cxs_mount;
			vfs_t           *vfsp = XFS_MTOVFS(mp);
                        int             error = 0;

			if (cxsp->cxs_trying) {
			        if (times == 1) {
				        mrunlock(&cxfs_sinfo_lock);
					goto start;
				}
				cxsp->cxs_waiting = 1;
				if (sv_mrlock_wait_sig(&cxsp->cxs_sv, 0,
						       &cxfs_sinfo_lock)) {
					return EINTR;
				}
				else
				  	goto start;
			}
			ASSERT(cxsp->cxs_server);
			if (bcmp(ap, &cxsp->cxs_args, 
				 sizeof(struct xfs_args))) {
			        mrunlock(&cxfs_sinfo_lock);
				return EINVAL;
			}
			if (vfs_busy_trywait_mrlock(vfsp, &cxfs_sinfo_lock,
						    &error)) {
				*vfspp = vfsp;
				return (0);
			}
			else if (error)
				return (error);
			else
			  	goto start;
        
		}
	}
	mrunlock(&cxfs_sinfo_lock);
	return ENOENT;
}

/*
 * Install a cxfs_sinfo in the global list
 */
void
cxfs_sinfo_install(
	cxfs_sinfo_t	*cxsp)		/* cxfs_sinfo_t to be installed. */
{
        mrupdate(&cxfs_sinfo_lock);
	ASSERT(cxsp->cxs_inlist == 0 && cxsp->cxs_waiting == 0);
#if DEBUG
	{
	  	cxfs_sinfo_t    *tcx;
		uint_t          dce_garbage;


		for (tcx = (cxfs_sinfo_t *) kqueue_first(&cxfs_sinfo_head);
		     tcx != (cxfs_sinfo_t *) &cxfs_sinfo_head;
		     tcx = (cxfs_sinfo_t *) kqueue_next(&tcx->cxs_queue)) {
			if (tcx->cxs_digest != cxsp->cxs_digest)
				continue;
	       		ASSERT_ALWAYS(!uuid_equal(&cxsp->cxs_uuid, 
						  &tcx->cxs_uuid, 
						  &dce_garbage));
		}
	}
#endif /* DEBUG */
	kqueue_enter(&cxfs_sinfo_head, &cxsp->cxs_queue);
	cxsp->cxs_inlist = 1;
	mrunlock(&cxfs_sinfo_lock);
}

/*
 * Remove a cxfs_sinfo from the global list
 */
void
cxfs_sinfo_extract(
	cxfs_sinfo_t	*cxsp)		/* cxfs_sinfo_t to be installed. */
{
        mrupdate(&cxfs_sinfo_lock);
	ASSERT(cxsp->cxs_inlist);
#if DEBUG
	{
	  	cxfs_sinfo_t    *tcx;

		for (tcx = (cxfs_sinfo_t *) kqueue_first(&cxfs_sinfo_head);
		     tcx != (cxfs_sinfo_t *) &cxfs_sinfo_head;
		     tcx = (cxfs_sinfo_t *) kqueue_next(&tcx->cxs_queue)) {
			if (tcx == cxsp)
				break;
		}
		ASSERT(tcx != (cxfs_sinfo_t *) &cxfs_sinfo_head);
	}
#endif /* DEBUG */
	kqueue_remove(&cxsp->cxs_queue);
	cxsp->cxs_inlist = 0;
	if (cxsp->cxs_waiting) {
		cxsp->cxs_waiting = 0;
		sv_broadcast(&cxsp->cxs_sv);
	}

	mrunlock(&cxfs_sinfo_lock);
}

/*
 * Convert from trying to be a server to succeeding
 */
void
cxfs_sinfo_server(
	cxfs_sinfo_t	*cxsp)		/* cxfs_sinfo_t to be freed. */
{
        mrupdate(&cxfs_sinfo_lock);
	ASSERT(cxsp->cxs_inlist && cxsp->cxs_trying && cxsp->cxs_server == 0);
	cxsp->cxs_trying = 0;
	cxsp->cxs_server = 1;
	if (cxsp->cxs_waiting) {
		cxsp->cxs_waiting = 0;
		sv_broadcast(&cxsp->cxs_sv);
	}
        mrunlock(&cxfs_sinfo_lock);
}

/*
 * Convert from trying to be a server to failing
 */
void
cxfs_sinfo_client(
	cxfs_sinfo_t	*cxsp)		/* cxfs_sinfo_t to be freed. */
{
        mrupdate(&cxfs_sinfo_lock);
	ASSERT(cxsp->cxs_inlist && cxsp->cxs_trying && cxsp->cxs_server == 0);
	cxsp->cxs_trying = 0;
	cxsp->cxs_mount = NULL;
	if (cxsp->cxs_waiting) {
		cxsp->cxs_waiting = 0;
		sv_broadcast(&cxsp->cxs_sv);
	}
        mrunlock(&cxfs_sinfo_lock);
}

/*
 * Extract cxfs_sinfo for a given mount structure
 */
cxfs_sinfo_t *
cxfs_sinfo_unmount(
	xfs_mount_t	*mp)
{
	cxfs_sinfo_t    *cxsp;

	mrupdate(&cxfs_sinfo_lock);
	for (cxsp = (cxfs_sinfo_t *) kqueue_first(&cxfs_sinfo_head);
	     cxsp != (cxfs_sinfo_t *) &cxfs_sinfo_head;
	     cxsp = (cxfs_sinfo_t *) kqueue_next(&cxsp->cxs_queue)) {
		if (cxsp->cxs_mount == mp)
			break;
	}
	if (cxsp == (cxfs_sinfo_t *) &cxfs_sinfo_head)
		cxsp = NULL;
	if (cxsp) {
		ASSERT(cxsp->cxs_inlist);
		kqueue_remove(&cxsp->cxs_queue);
		cxsp->cxs_inlist = 0;
		if (cxsp->cxs_waiting) {
		        cxsp->cxs_waiting = 0;
			sv_broadcast(&cxsp->cxs_sv);
		}
	}
	mrunlock(&cxfs_sinfo_lock);
	return (cxsp);
}

/*
 * Free a cxfs_sinfo structure.
 */
void
cxfs_sinfo_free(
	cxfs_sinfo_t	*cxsp)		/* cxfs_sinfo_t to be freed. */
{
        kmem_free(cxsp, cxsp->cxs_length);
}

/*
 * Initialize this module
 */
void 
cxfs_sinfo_init(void)
{
	kqueue_init(&cxfs_sinfo_head);
	mrlock_init(&cxfs_sinfo_lock, MRLOCK_BARRIER, "cxfs_sinfo", 0);
}

/*
 * Delayed initialization routine.  This is to performa any delayed 
 * initialization.  We were having problems doing the wp_domain_creates
 * in initialization proper.  By putting them here, we piggyback on
 * cxfs_sinfo_lock to make sure the delayed initialzation happens once.
 */
void
cxfs_array_setup(void)
{
	mrupdate(&cxfs_sinfo_lock);
	if (!cxfs_setup_done) {
	      cxfs_array_wpinit();
	      cxfs_setup_done = 1;
	}
	mrunlock(&cxfs_sinfo_lock);
}
