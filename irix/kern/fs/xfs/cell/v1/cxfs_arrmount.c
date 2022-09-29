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
#ident	"$Revision: 1.2 $"

/*
 * Array mount support for Cellular XFS V1
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/uuid.h>
#include <sys/kmem.h>
#include <sys/kabi.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/ktime.h>
#include <ksys/behavior.h>
#include <ksys/cell.h>
#include <ksys/cell/service.h>
#include <ksys/cell/subsysid.h>
#include <ksys/cell/wp.h>
#include "fs/cell/cfs_intfc.h"
#include "fs/cell/mount_import.h"
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
#include "invk_cxfs_array_stubs.h"
#include "I_cxfs_array_stubs.h"

typedef struct cxfs_connect {
	xfs_mount_t	*cxc_mp;	/* Pointer to current xfs_mount. */
        uuid_t          cxc_uuid;       /* Id for the filesystem. */
        ulong_t         cxc_wpid;       /* (Temporary) uuid digest for wp. */
        ulong_t         cxc_wpver;      /* (Temporary) wp verification key. */
        int             cxc_timeleft;   /* Milliseconds left until timeout. */
        int             cxc_tries;      /* Number of rpc's trying to connect. */
        service_t       cxc_server;     /* Server we found for this mount. */
        cxfs_sinfo_t    *cxc_sinfo;     /* Associated sinfo, if any. */
} cxfs_connect_t;

#define CXC_WPMASK_CELL	0x3f
#define CXC_WPMASK_VER	(~((wp_value_t) CXC_WPMASK_CELL))

wp_domain_t cxfs_server_domain;
wp_domain_t cxfs_master_domain;

#if DEBUG
#define TEST_UUID_STUFF 1
#endif

STATIC int cxfs_get_servers(xfs_mount_t *,struct xfs_args *, cxfs_sinfo_t **);
STATIC int cxfs_array_cconnect(cxfs_sinfo_t *, struct xfs_args *);
STATIC int cxfs_array_sconnect(cxfs_sinfo_t *, struct xfs_args *, int *);
STATIC int cxfs_cconnect_loop(cxfs_connect_t *);
STATIC void cxfs_connect_setup(cxfs_connect_t *, xfs_mount_t *, uuid_t *);
STATIC int cxfs_connect_find(cxfs_connect_t *);
STATIC int cxfs_connect_try(cxfs_connect_t *);
STATIC int cxfs_arrmount(xfs_mount_t *, struct xfs_args *, int *);
STATIC int cxfs_oldmount(xfs_mount_t *, struct xfs_args *);

/*
 * General cxfs array mount routine.
 */
int
cxfs_mount(
	xfs_mount_t     *mp,		/* Mount structure for current fs. */
        struct xfs_args *ap,            /* Arguments for current mount. */
	dev_t		dev,		/* Main filesystem device */
	int		*is_client)     /* If set, client behaviors have
                                           been installed in the vfs,
                                           replacing the xfs vfs behavior
                                           which the caller should free. */
{
        int		error = 0;

	*is_client = 0;
	if (error = xfs_readsb(mp, ddev)) {
		return(error);
	}
	if (ap && XFSARGS_FOR_CXFSARR(ap))
	        error = cxfs_arrmount(mp, ap, is_client);
	else 
	        error = cxfs_oldmount(mp, ap);
#if TEST_UUID_STUFF
	if (error == 0 && is_client == 0 &&
	    ap && (ap->flags & XFSMNT_TESTUUID)) 
	        error = XFS_ERROR(EINVAL);
#endif
	return (error);
}

/* 
 * Do setup for cxfs array mount
 *
 * This routine is responsible for dealing with the special requirements
 * of an array mount in the case in which one or more of the array-specific
 * mount options is specified.  Depending on the options, it may attempt 
 * to become the server for the file system, and if it succeeds, sets up
 * the auxiliary structures which clients will use to connect with the 
 * serving vfs.
 *
 * When we are not the server, either because we are not allowed to be so,
 * or because another system has already established itself as the server,
 * we return an indication of this fact and build cfs client vfs-level
 * data structures to replace the xfs ones, which the caller should then
 * deallocate.
 */
int
cxfs_arrmount(
	xfs_mount_t     *mp,		/* Mount structure for current fs. */
        struct xfs_args *ap,            /* Arguments for current mount. */
	int		*is_client)     /* If set, client behaviors have
                                           been installed in the vfs,
                                           replacing the xfs vfs behavior
                                           which the caller should free. */
{
	cxfs_sinfo_t    *cxsp = NULL;
	int 		error;

        
        *is_client = 0;
	cxfs_array_setup();

	/*
	 * Unshared mount request is treated as if no special parameters
         * were specified since this (unshared mount) is the default.
	 */
	if (ap->flags & XFSMNT_UNSHARED)
		return (cxfs_oldmount(mp, ap));

	/*
         * If no server_list is specified, check the consistency of 
         * the related arguments.  Then, if none of server_list,
	 * client_only, or unshared were specified, treat this as
         * the default, unshared case.
	 */
	if (ap->scount == -1) {
                if (ap->servers != NULL || ap->servlen != NULL)
			return (XFS_ERROR(EINVAL));
		if ((ap->flags & XFSMNT_CLNTONLY) == 0)
		        return (cxfs_oldmount(mp, ap));
	}

	/*
         * If there is a server_list, check the size.
	 */
	else if (ap->scount <= 0 || ap->scount > MAX_CELLS)
		return (XFS_ERROR(EINVAL));

	/*
         * If we have a valid server_list and it is not overridden by
         * client_only, then read in the server list.  If we are in the
         * the list, then this mount is server-capable.
	 */
	else if ((ap->flags & XFSMNT_CLNTONLY) == 0) {
		error = cxfs_get_servers(mp, ap, &cxsp);
		if (error)
			return (error);
		cxfs_find_self(cxsp);
	}

	/*
         * Deal with the client_only case.
	 */
	if (cxsp == NULL) {
	        ASSERT(ap->flags & XFSMNT_CLNTONLY);
		ap->scount = 0;
		cxsp = cxfs_sinfo_create(ap, mp, 0);
		cxsp->cxs_order = -1;
	}

	/*
         * Do the appropriate thing, depending on whether we are server-
         * capable.  
	 */
	if (ap->ctimeout == -1)
		ap->ctimeout = CXFS_CTIMEOUT_DFLT;
	cxfs_sinfo_install(cxsp);
	if (cxsp->cxs_order >= 0) 
		error = cxfs_array_sconnect(cxsp, ap, is_client);
	else {
	        error = cxfs_array_cconnect(cxsp, ap);
		if (error == 0)
		  	*is_client = 1;
	}
	if (error) {
		cxfs_sinfo_extract(cxsp);
		cxfs_sinfo_free(cxsp);
	}
        return (error);
}

/*
 * Do special setup for old-style mount
 */
/* ARGSUSED */
int
cxfs_oldmount(
	xfs_mount_t     *mp,		/* Current xfs mount structure. */
        struct xfs_args *ap)            /* Arguments for current mount. */
{
#ifdef NOTYET
        if (ap != NULL)
		cxfs_array_setup();
#endif
        mp->m_flags |= XFS_MOUNT_UNSHARED;
        return (0);
}

/*
 * Do cleanup for unmount
 */
void 
cxfs_unmount(
	xfs_mount_t     *mp)		/* Current xfs mount structure. */
{
        cxfs_sinfo_t    *cxsp;

	cxsp = cxfs_sinfo_unmount(mp);
	if (cxsp) {
	        if (cxsp->cxs_server) {
		  	int		error;
			cxfs_connect_t  cparms;

			cxfs_connect_setup(&cparms, mp, &cxsp->cxs_uuid);
			error = wp_clear(cxfs_server_domain, 
					 cparms.cxc_wpid, 1);
			ASSERT(error == 0);
		}
		cxfs_sinfo_free(cxsp);
	} 
}

/*
 * Read the user's server list
 *
 * This routine reads the list of servers specified in the mount arguments
 * and constructs an appropriate cxfs_sinfo_t.  
 *
 * The value returned is an error code.
 */
int
cxfs_get_servers(
	xfs_mount_t     *mp,		/* Current xfs mount structure. */
	struct xfs_args *ap,            /* Mount args copied in from user. */
        cxfs_sinfo_t    **cxspp)        /* Slot to return cxfs_info_t addr */
{
        int             stringlen = 0;
	__int32_t	*lens = NULL;
	char            **addrp;
        char            *stringp;
	int             error = 0;
	cxfs_sinfo_t    *cxsp = NULL;
        int             i;
	char            *uaddr;

	/*
         * Copy in the table of hostname lengths.
	 */
	lens = (__int32_t *) kmem_alloc(ap->scount * sizeof(__int32_t),
					KM_SLEEP);
	if (copyin(ap->servlen, lens, ap->scount * sizeof(__int32_t))) {
	        error = XFS_ERROR(EFAULT);
		goto err;
	}

	/*
         * Compute the total length of the hostname strings.
	 */
	for (i = 0; i < ap->scount; i++) {
	        if (lens[i] <= 0 || lens[i] > MAXNAMELEN) {
		        error = XFS_ERROR(EFAULT);
			goto err;
		}
		stringlen += (lens[i] + 1);
	}

	/*
         * Now allocate our cxfs_sinfo.
	 */
	cxsp = cxfs_sinfo_create(ap, mp, stringlen);
	addrp = cxsp->cxs_addrs;
	stringp = cxsp->cxs_strings;

	/*
	 * We have to copy in the hostname table and the strings pointed
	 * to and place them in the areas appended to the cxfs_sinfo_t.
	 */
	uaddr = (char *) ap->servers;
	for (i = 0; i < ap->scount; i++) {
                int		cval;
                char            *hisp;

		/*
		 * Fill in the current string pointer.
		 */
		*addrp = stringp;
		addrp++;

		/*
                 * Copy in the user's hostname pointer
		 */
#if _MIPS_SIM == _ABI64
	        if (!ABI_IS_IRIX5_64(get_current_abi())) {
			app32_ptr_t	hisp32;
			app32_ptr_t     uaddr32;

			cval = copyin(uaddr, &hisp32, sizeof(hisp32));
			uaddr32 = (app32_ptr_t) ((__psunsigned_t) uaddr);
			uaddr = (char *) (uaddr32 + sizeof(app32_ptr_t));
			hisp = (char *) (__psunsigned_t) hisp32;
		}
	        else 
#endif
		{
		        cval = copyin(uaddr, &hisp, sizeof(hisp));
			uaddr += sizeof(app64_ptr_t);
		}
		if (cval) {
			error = XFS_ERROR(EFAULT);
			goto err;
		}

		/*
		 * Now copy in the hostname itself.
		 */
		if (copyin(hisp, stringp, lens[i])) {
			error = XFS_ERROR(EFAULT);
			goto err;
		}
		*(stringp + lens[i]) = 0;
		if (strlen(stringp) != lens[i]) {
			error = XFS_ERROR(EINVAL);
			goto err;
		}
		stringp += (lens[i] + 1);
	}

	/*
	 * Successful completion.  Clean up and return.
	 */
	*cxspp = cxsp;
	kmem_free(lens, ap->scount * sizeof(__int32_t));
	return (0);

	/*
         * Error completion.  Clean up and return.
	 */
err:
	if (cxsp)
		cxfs_sinfo_free(cxsp);
	if (lens)
	        kmem_free(lens, ap->scount * sizeof(__int32_t));
	return (error);
}


/*
 * Connect to the server in client-only mode
 */
int
cxfs_array_cconnect(
	cxfs_sinfo_t 	*cxsp,
        struct xfs_args *ap)
{
	xfs_mount_t 	*mp = cxsp->cxs_mount;
        cxfs_connect_t  cparms;
        int             error;

	cxfs_connect_setup(&cparms, mp, &mp->m_sb.sb_uuid);
	cparms.cxc_timeleft = ap->ctimeout;
	cparms.cxc_sinfo = cxsp;
	error = cxfs_cconnect_loop(&cparms);
	return (error);
}

/*
 * Connect to or try to become the server
 */
int 
cxfs_array_sconnect(
	cxfs_sinfo_t	*cxsp, 
        struct xfs_args *ap,
        int             *is_client)
{
        cxfs_connect_t  cparms;
        int             error;
	ulong_t         oldbase;
	ulong_t         oldrange;
	wp_value_t      oldval;

	if (ap->stimeout < 0)
		ap->stimeout = CXFS_STIMEOUT_DFLT;
	cxfs_connect_setup(&cparms, cxsp->cxs_mount, &cxsp->cxs_uuid);
	cparms.cxc_timeleft = ap->stimeout * cxsp->cxs_order;
	cparms.cxc_sinfo = cxsp;
	error = cxfs_cconnect_loop(&cparms);
	if (error != ETIMEDOUT) {
	        if (error == 0)
			*is_client = 1;
		return (error);
	}
	error = wp_register(cxfs_server_domain, cparms.cxc_wpid, 1,
			    cparms.cxc_wpver | cellid(), 
			    &oldbase, &oldrange, &oldval);
	if (error == 0) {
	  	cxfs_sinfo_server(cxsp);
		*is_client = 0;
		return (0);
	}
	ASSERT(oldbase == cparms.cxc_wpid && oldrange == 1);
	if (cparms.cxc_wpver != oldval & CXC_WPMASK_VER)
	        panic("wpver mismatch");
	cparms.cxc_timeleft = ap->ctimeout - ap->stimeout * cxsp->cxs_order;
	if (cparms.cxc_timeleft < 0)
		return (ETIMEDOUT);
	error = cxfs_cconnect_loop(&cparms);
	if (error == 0)
	        *is_client = 1;
	return (error);
}

/*
 * Initial client-only server-find loop
 */
int
cxfs_cconnect_loop(
	cxfs_connect_t	*cxcp)
{
        int		error;

	while (1) {
		error = cxfs_connect_find(cxcp);
		if (error)
			break;
		error = cxfs_connect_try(cxcp);
		if (error != ENOENT)
		        break;
	}
	return (error);
}

/*
 * Try to find the server
 */
int 
cxfs_connect_find(
	cxfs_connect_t	*cxcp)
{
        timespec_t	start;
	timespec_t      end;
        timespec_t      limit;
	int             error;

	if (cxcp->cxc_timeleft <= 0)
		return ETIMEDOUT;
	nanotime(&start);
	limit.tv_sec = cxcp->cxc_timeleft/1000;
	limit.tv_nsec = (cxcp->cxc_timeleft % 1000) * 1000000;
	timespec_add(&limit, &start);
	while (1) {
		wp_value_t	wpval;

		error = wp_lookup(cxfs_server_domain, 
				  cxcp->cxc_wpid, 
				  &wpval);
		if (error == 0) {
		        if (cxcp->cxc_wpver != wpval & CXC_WPMASK_VER)
				panic("wpver mismatch");
		        SERVICE_MAKE(cxcp->cxc_server, 
				     wpval & CXC_WPMASK_CELL, SVC_CXFS);
			break;
		}

		nanotime(&end);
		timespec_sub(&end, &limit);
		if (end.tv_sec >= 0) {
			error = ETIMEDOUT;
			break;
		}

		delay(HZ / 20);
	}
	if (error == 0) {
		nanotime(&end);
		timespec_sub(&limit, &end);
		if (limit.tv_sec <= 0)
			cxcp->cxc_timeleft = 0;
		else 
			cxcp->cxc_timeleft = (limit.tv_sec * 1000) +
			                     howmany(limit.tv_nsec, 1000000);
	}
	return (error);
}

/*
 * Connect to server for file system
 */
int
cxfs_connect_try(
	cxfs_connect_t	*cxcp)
{
        int             xerr;
        int             serr;
	mount_import_t  mi;
	vfs_t           *vfsp;

	xerr = invk_cxfs_array_connect(cxcp->cxc_server, 
				       &cxcp->cxc_uuid, cellid(), 
				       &cxcp->cxc_sinfo->cxs_args, 
				       &mi, &serr);
	if (xerr)
		return (xerr);
	if (serr)
		return (serr);
	vfsp = XFS_MTOVFS(cxcp->cxc_mp);
	xfs_mount_free(cxcp->cxc_mp);
	cxcp->cxc_mp = NULL;
	cfs_mountpoint_setup(vfsp, NULL, &mi, cxcp->cxc_sinfo);
	cxfs_sinfo_client(cxcp->cxc_sinfo);
	return(0);
}
		

/*
 * Set up a cxfs_connect structure
 */
void
cxfs_connect_setup(
        cxfs_connect_t	*cxcp,
	xfs_mount_t     *mp,
        uuid_t          *idp)
{
        uchar_t         *p;

        cxcp->cxc_mp = mp;
	cxcp->cxc_uuid = *idp;
	cxcp->cxc_tries = 0;
	cxcp->cxc_sinfo = NULL;
	SERVICE_MAKE_NULL(cxcp->cxc_server);
        cxcp->cxc_wpid = cxfs_sinfo_digest(&cxcp->cxc_uuid);
	bcopy(&cxcp->cxc_uuid, &cxcp->cxc_wpver, sizeof(wp_value_t));
	p = (uchar_t *) &cxcp->cxc_wpver;
	p[0] ^= p[sizeof(wp_value_t) - 1];
	cxcp->cxc_wpver &= CXC_WPMASK_VER;
}

/*
 * Do initialization
 */
void
cxfs_arrinit(void)
{
	mesg_handler_register(cxfs_array_msg_dispatcher, 
			      CXFS_ARRAY_SUBSYSID);
	cxfs_sinfo_init();
}

/*
 * Do wp initialization.  We've had problems doing this early, so for now
 * we do this on the first mount.
 */
void
cxfs_array_wpinit(void)
{
  	if (wp_domain_create(CXFS_WP_MDOMAIN, &cxfs_master_domain))
		panic("cxfs_arrinit: no cxfs_master domain");
  	if (wp_domain_create(CXFS_WP_SDOMAIN, &cxfs_server_domain))
		panic("cxfs_arrinit: no cxfs_server domain");
}

/*
 * Handle a connect message
 */
void
I_cxfs_array_connect(
	uuid_t		*idp,
	cell_t          client,
	struct xfs_args *ap,
	mount_import_t  *mip,
        int             *errorp)
{
        vfs_t           *vfsp;
	vnode_t         *rvp;
	/* REFERENCED */
        int             should_be_zero;

	*errorp = cxfs_sinfo_findvfs(idp, ap, &vfsp);
	if (vfsp == NULL) 
		return;
		
	VFS_ROOT(vfsp, &rvp, should_be_zero);
	ASSERT(should_be_zero == 0);
	vfs_unbusy(vfsp);

	*errorp = 0;
	if (cfs_mountpoint_export(vfsp, rvp, client, mip))
		*errorp = EBUSY;
	VN_RELE(rvp);
}
