#ident "$Revision: 1.5 $"

#define _KERNEL 1
#include <sys/param.h>
#undef _KERNEL

#include <stdio.h>
#include <unistd.h>

#include <sys/sema.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"

/* initialize the xfs simulation library, it either works or the program exits */

void
sim_init(sim_init_t *simargs)
{
	if (isa_file)  {
		simargs->disfile = 1;
		simargs->dname = fs_name;
		simargs->volname = NULL;
	} else  {
		simargs->disfile = 0;
		simargs->volname = fs_name;
		simargs->dname = NULL;
	}

	simargs->rtname = NULL;
	simargs->logname = NULL;
	simargs->dcreat = 0;
	simargs->lisfile = 0;
	simargs->lcreat = 0;
	simargs->risfile = 0;
	simargs->rcreat = 0;

	simargs->notvolmsg = "you should never get this message - %s";
	simargs->notvolok = 1;

	if (no_modify)
		simargs->isreadonly = XFS_SIM_ISREADONLY;
	else
		simargs->isreadonly = 0;

	if (!xfs_sim_init(simargs))  {
		do_error("couldn't initialize simulation library\n");
	}
}

/* xfs_mount reads in the root inode */
