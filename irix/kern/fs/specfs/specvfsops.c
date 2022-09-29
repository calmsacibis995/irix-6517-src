/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

/* #ident	"@(#)uts-comm:fs/specfs/specvfsops.c	1.3" */
#ident	"$Revision: 1.18 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/fs_subr.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/strsubr.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <fs/specfs/spec_lsnode.h>

extern void spec_flush(void);


/*
 * Run though all the snodes and force write-back
 * of all dirty pages on the block devices.
 * Since currently, spec_write does ASYNC writes not DELWRI - there
 * is no reason for bdflush to cause anything to happen here. In any
 * case bdflush already goes through every once in a while flushing
 * DELWRI buffers.
 */
/* ARGSUSED */
static int
spec_sync(
	bhv_desc_t	*bdp,
	int		flag,
	struct cred	*credp)
{
	if (flag & SYNC_BDFLUSH)
		return 0;

	if (mutex_trylock(&spec_sync_lock) == 0)
		return 0;

	if (flag & SYNC_CLOSE)
		(void) strpunlink(credp);

	/* 
	 * spec_flush is called to traverse the hash list, it must lock it
	 * during traversal
	 */
	if (!(flag & SYNC_ATTR)) {
		spec_flush();
	}
	mutex_unlock(&spec_sync_lock);
	return 0;
}

vfsops_t spec_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	fs_nosys,		/* mount */
	fs_nosys,		/* rootinit */
	fs_nosys,		/* mntupdate */
	fs_nosys,		/* dounmount */
	fs_nosys,		/* unmount */
	fs_nosys,		/* root */
	fs_nosys,		/* statvfs */
	spec_sync,
	fs_nosys,		/* vget */
	fs_nosys,		/* mountroot */
	fs_noerr,		/* realvfsops */
	fs_import,		/* import */
	fs_quotactl,		/* quotactl */
};

