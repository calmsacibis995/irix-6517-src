/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:io/clone/clone.c	1.4"*/
#ident	"$Revision: 3.21 $"

/*
 * Clone Driver.
 */

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/mkdev.h>
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/strsubr.h>

#include <sys/ddi.h>

/*
 * The clone driver is intended for drivers which are not hwgraph-aware.
 * hwgraph-aware drivers can implement similar functionality in other ways.
 */

int clnopen(queue_t *, dev_t *, int, int, struct cred *);
static struct module_info clnm_info = { 0, "CLONE", 0, 0, 0, 0 };
static struct qinit clnrinit = { NULL, NULL, clnopen, NULL, NULL, &clnm_info, NULL };
static struct qinit clnwinit = { NULL, NULL, NULL, NULL, NULL, &clnm_info, NULL };
int clndevflag = 0;
struct streamtab clninfo = { &clnrinit, &clnwinit };

/*
 * clone_get_cdevsw is a support routine for clone devices which converts
 * a clone dev_t into the actual cdevsw that it refers to.
 */
struct cdevsw *
clone_get_cdevsw(dev_t dev)
{
	major_t maj, emaj;

	if (!dev_is_vertex(dev)) {
		emaj = getminor(dev);
		maj = etoimajor(emaj);
		if ((maj < cdevcnt) || ((maj < cdevmax) && cdevsw[maj].d_flags))
			return(&cdevsw[maj]);
	}

	return(NULL);
}


/*
 * Clone open.  Maj is the major device number of the streams
 * device to open.  Look up the device in the cdevsw[].  Attach
 * its qinit structures to the read and write queues and call its
 * open with the sflag set to CLONEOPEN.  Swap in a new vnode with
 * the real device number constructed from either
 *	a) for old-style drivers:
 *		maj and the minor returned by the device open, or
 *	b) for new-style drivers:
 *		the whole dev passed back as a reference parameter
 *		from the device open.
 */
int
clnopen(queue_t *qp, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	register struct streamtab *stp;
	dev_t newdev;
	int error;
	minor_t emaj;
	struct cdevsw *my_cdevsw;

	if (sflag)
		return (ENXIO);

	/*
	 * Get the device to open.
	 */
	my_cdevsw = clone_get_cdevsw(*devp);

	/*
	 * If this is a dynamically loadable clone, call cdhold to increment
	 * the refcnt. This does the work of the cdhold call in qattach for
	 * a non-clone driver, so the non-error cdrele call is handled in 
	 * mlqdetach for both clone and non-clone drivers.
	 */

	if (!cdvalid(my_cdevsw))
		return (ENXIO);
	if (*my_cdevsw->d_flags & D_OBSOLD)
		return ENOTSUP;
	if (error = cdhold(my_cdevsw))
		return error;
	if (!(stp = my_cdevsw->d_str)) {
		cdrele (my_cdevsw);
		return (ENXIO);
	}

	emaj = getminor(*devp); /* minor is major for a cloned drivers */

	/*
	 * Substitute the real qinit values for the current ones.
	 */
	setq(qp, stp->st_rdinit, stp->st_wrinit);

	/*
	 * Call the device open with the stream flag CLONEOPEN.  The device
	 * will either fail this or return the whole device number
	 */
	newdev = makedevice(emaj, 0);	/* create new style device number  */

	if (error = (*qp->q_qinfo->qi_qopen)(qp, &newdev, flag, CLONEOPEN, crp)) {
		/* no cdrele() needed; cleaned up by qdetach/mlqdetach */
		return (error);
	}
	my_cdevsw = get_cdevsw(newdev);
	if (!cdvalid(my_cdevsw) || !(stp = my_cdevsw->d_str)) {
		(*qp->q_qinfo->qi_qclose)(qp, flag, crp);
		return (ENXIO);
	}
	*devp = newdev;
	return (0);
}	
