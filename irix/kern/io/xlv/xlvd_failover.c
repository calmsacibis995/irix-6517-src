/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1994, Silicon Graphics, Inc.                  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.24 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>

#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/bpqueue.h>
#include <sys/sysmacros.h>
#include <sys/scsi.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/uuid.h>
#include <sys/invent.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/failover.h>
#include "xlv_ioctx.h"

extern bp_queue_t	  xlvd_work_queue;

int
xlv_devopen(xlv_tab_disk_part_t *dp, int flag, int otype, struct cred *cred)
{
    dev_t	  pdev;
    dev_t         new_dev;
    int		  errno;
    int           err;
    char          *etext;
    struct bdevsw *my_bdevsw;

    /* active_path is now the only valid path.
     * if the open fails, force failover
     */

    for (;;) {

	pdev = dp->dev[dp->active_path];
	my_bdevsw = get_bdevsw(pdev);
	if (my_bdevsw == NULL) {
		/* How can the bdevsw be NULL? */
		cmn_err(CE_WARN,
		    "XLV: device open error, no bdevsw for dev: 0x%x (%s)\n",
			pdev, devtopath(pdev));
		errno = ENODEV;

	} else if (!(errno = bdhold(my_bdevsw))) {
	    errno = bdopen(my_bdevsw, &pdev, flag, otype, cred);
	    bdrele(my_bdevsw);
	    if (errno == 0)
		    break;
	}

	/* error on open */
	/* XXXsbarr: can we ever enter into a successful failover
	 * but unsuccessful open?
	 */

	/* if there's only one path to disk, we can't fail over */
	if (fo_scsi_device_pathcount(pdev) <= 1) {

		/* if we can't fail over, just return the errno we received */
		return errno;
	}

	cmn_err(CE_WARN,
	    "XLV: device open error; attempting failover for dev: %d (%s)\n",
		minor(pdev), devtopath(pdev));

	err = fo_scsi_device_switch(pdev, &new_dev);
	if (err == FO_SWITCH_SUCCESS) {
		dp->dev[dp->active_path] = new_dev;
		cmn_err(CE_WARN,
			"XLV: failover successful, new dev: %d (%s)\n",
			minor(new_dev), devtopath(new_dev));
		continue;
	}

	/* switch failed, print out error */
	switch (err) {
	case FO_SWITCH_FAIL:
		etext = "FO_SWITCH_FAIL";
		break;
	case FO_SWITCH_INVALID_PATH:
		etext = "FO_SWITCH_INVALID_PATH";
		break;
	case FO_SWITCH_NOPATH:
		etext = "FO_SWITCH_NOPATH";
		break;
	case FO_SWITCH_ONEPATHONLY:
		etext = "FO_SWITCH_ONEPATHONLY";
		break;
	case FO_SWITCH_PATHS_EXHAUSTED:
		etext = "FO_SWITCH_PATHS_EXHAUSTED";
		break;
	default:
		etext = "UNKNOWN FO ERROR";
		break;
	}
	cmn_err_tag(135,CE_WARN, "XLV: device open failover unsuccessful: "
		"fo_scsi_device_switch returned %s for dev: %d, errno: %d\n",
		etext, minor(pdev), errno);

	return errno;
    }

#ifdef FO_STATS
    n_opens++;
#endif /* FO_STATS */

    return 0;
}


int
xlv_devclose(xlv_tab_disk_part_t *dp, int flag, int otype, struct cred *cred)
{
    dev_t	  pdev;
    int		  errno;
    struct bdevsw *my_bdevsw;

    pdev = dp->dev[dp->active_path];
    binval(pdev);
    my_bdevsw = get_bdevsw(pdev);
    ASSERT(my_bdevsw != NULL);
    errno = bdclose(my_bdevsw, pdev, flag, otype, cred);

    if (errno) printf("xlv: bdclose %x returned %d.\n", pdev, errno);

#ifdef FO_STATS
    n_closes++;
    if (n_opens == n_closes) {
	printf("opens: %d (%llo), closes: %d; fos: %d (%d), succ: %d (%d),"
	       " fail: %d (%d);\n", n_opens, n_open_retries, n_closes,
	       n_fos, n_defs, n_succs, n_reissues, n_fails, n_aborts);
	printf("allocs: %d (retries: %d, max: %d), frees: %d; tres: %d"
	       " (retries: %d, max %d).\n", n_allocs, n_alloc_retries,
	       max_alloc_retries, n_frees, n_comms, n_comm_retries,
	       max_comm_retries);
	printf("fo times: min: %d, max: %d, tot: %d, avg: %d.\n", fo_min,
	       fo_max, fo_tot, n_fos ? fo_tot*100/n_fos : 0);
	printf("fl times: min: %d, max: %d, tot: %d, avg: %d.\n", fl_min,
	       fl_max, fl_tot, n_fos ? fl_tot*100/n_fos : 0);
	fo_tot = fl_tot = 0;
	n_opens = n_closes = n_open_retries = n_fos = n_defs = n_succs
	= n_reissues = n_fails = n_aborts = n_allocs = n_alloc_retries
	= max_alloc_retries = n_frees = n_comms = n_comm_retries
	= max_comm_retries = 0;
    }
#endif /* FO_STATS */

    return errno;
}


