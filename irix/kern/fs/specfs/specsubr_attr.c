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

#ident	"$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/hwg.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/open.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/strmp.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ddi.h>
#include <sys/major.h>

#ifdef	CELL_IRIX
#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_spec_ds_stubs.h"
#include <ksys/cell/service.h>
#endif	/* CELL_IRIX */

#include <fs/specfs/spec_atnode.h>



/*
 * SPECFS Global Data for the "Attribute" side.
 */
struct zone *spec_at_zone;


void
spec_at_init()
{
	spec_at_zone = kmem_zone_init(sizeof(struct atnode), "specFS ATnodes");
}


void
spec_at_lock(atnode_t *atp)
{
	SPEC_STATS(spec_at_lock);

	ASSERT(atp->at_locktrips >= 0);

	if (mutex_mine(&atp->at_lock))
		atp->at_locktrips++;
	else
		mutex_lock(&atp->at_lock, 0);
}


void
spec_at_unlock(atnode_t *atp)
{
	SPEC_STATS(spec_at_unlock);

	ASSERT(mutex_mine(&atp->at_lock));
	ASSERT(atp->at_locktrips >= 0);

	if (atp->at_locktrips > 0)
		atp->at_locktrips--;
	else
		mutex_unlock(&atp->at_lock);
}


/*
 * Create a new atnode and insert its behavior.
 */
void
spec_at_insert_bhv(vnode_t *vp, vnode_t *fsvp)
{
	atnode_t	*atp;
	timespec_t	tv;

	SPEC_STATS(spec_at_insert_bhv);

	atp = kmem_zone_zalloc(spec_at_zone, KM_SLEEP);

	init_mutex(&atp->at_lock, MUTEX_DEFAULT, "spec_atnode", vp->v_rdev);

	/*
	 * If there's an "alternate" vp, this is a "replicate"
	 * case where we'll need to remember to ask the "actual"
	 * underlying file system for attributes.
	 */
	atp->at_fsvp = fsvp;		/* Record alternate vp	*/

	if (fsvp)
		VN_HOLD(fsvp);		/* Sink our teeth into it	*/

	atp->at_dev  = vp->v_rdev;

	nanotime_syscall(&tv);

	atp->at_mtime = tv.tv_sec;
	atp->at_atime = tv.tv_sec;
	atp->at_ctime = tv.tv_sec;

	VN_FLAGSET(vp, VINACTIVE_TEARDOWN);

	/*
	 * Now, create the behavior entry and insert it into
	 * the vnode behavior chain.
	 */
	bhv_desc_init(ATP_TO_BHV(atp), atp, vp, &spec_at_vnodeops);

	VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));

	vn_bhv_insert(VN_BHV_HEAD(vp), ATP_TO_BHV(atp));

	VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));
}


void
spec_at_free(atnode_t *atp)
{
	SPEC_STATS(spec_at_free);

	mutex_destroy(&atp->at_lock);

	kmem_zone_free(spec_at_zone, atp);
}
