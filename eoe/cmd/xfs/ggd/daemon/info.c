#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/info.c,v 1.11 1997/03/13 22:15:56 kanoj Exp $"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <bstring.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <time.h>
#include <grio.h>
#include <sys/mkdev.h>
#include <sys/fs/xfs_inum.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/lock.h>
#include <sys/sysmp.h>
#include <sys/ktime.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

/*
 * info.c
 *
 *
 */
extern int 		determine_remaining_bandwidth( device_node_t *, 
			time_t, __int64_t *, __int64_t *);

/*
 * grio_info_get_file_resvs
 *	This routine is called when a user process isses the GRIO_GET_INFO
 *	call with the GRIO_FILE_RESVS subcommand.  It searches the
 *	file reservation list for reservations on the given file, and returns 
 * 	a count to the caller.
 *
 * RETURNS:
 *	0 on success
 */
int
grio_info_get_file_resvs(
	dev_t		fsdev, 
	gr_ino_t	ino,
	grio_stats_t 	*griostp)
{
	ulong_t 	count = 0;
	reservations_t 	*resvp;

	extern int	totalstreamcount;

	resvp   = (&(FILE_RSV_INFO()))->resvs;

	while (resvp) {
		if (	(resvp->resv_info.glob_resv_info.fs_dev == fsdev) && 
			(resvp->resv_info.glob_resv_info.inum == ino)) {
			count++;
		}
		resvp = resvp->nextresv;
	}

	griostp->gs_count 	= count;
	griostp->gs_maxresv   	= (ulong_t)totalstreamcount;
	griostp->gs_optiosize  	= -1;

	return( 0 );
}

/*
 * grio_info_get_proc_resvs
 *	This routine is called when a user process isses the GRIO_GET_INFO
 *	call with the GRIO_PROC_RESVS subcommand.  It returns the
 *	number of reservations on the given device to the caller.
 *
 * RETURNS:
 *	0 on success
 */
int
grio_info_get_proc_resvs( 
	pid_t		procid, 
	grio_stats_t	*griostp)
{
	ulong_t		count = 0;
	reservations_t 	*resvp;
	extern int	totalstreamcount;

	resvp   = (&(FILE_RSV_INFO()))->resvs;

	while (resvp) {
		if ( resvp->resv_info.glob_resv_info.procid == procid ) {
			count++;
		}
		resvp = resvp->nextresv;
	}

	griostp->gs_count 	= count;
	griostp->gs_maxresv   	= (ulong_t)totalstreamcount;
	griostp->gs_optiosize  	= -1;

	return( 0 );
}


/*
 * grio_info_get_proc_resvs
 *	This routine is called when a user process isses the GRIO_GET_INFO
 *	call with the GRIO_PROC_RESVS subcommand.  It searches the
 *	file reservation list for reservations made by this process and
 *	returns a count to the caller.
 *      For old platforms, the device name is a grio node name from the
 *      grio_config file, since there is no other way to refer to some
 *      components like the IO4 board. For new platforms, devanme is a 
 *      hwgraph path name.
 * RETURNS:
 *	0 on success
 */
int
grio_info_get_dev_resvs(
	char		*devname, 
	grio_stats_t	*griostp)
{
	__int64_t		maxreqtime = 0, maxreqsize = 0; 
	int			seconds, total, remaining;
	device_node_t 		*devnp;
	struct stat     	statbuf;

	if (stat(devname, &statbuf))
		devnp = NULL;
	else devnp = devinfo_from_cache(statbuf.st_rdev);

	if (devnp != NULL) {
		/*
		 * Device node was found.
 		 */

		/*
		 * Determine amount of bandwidth currently used.
	 	 * This is done by subtracting the amount of remaining
		 * bandwidth from the total available bandwidth.
	 	 */
		griostp->gs_maxresv   	= (ulong_t)devnp->resv_info.maxioops;
		griostp->gs_optiosize  	= (ulong_t)devnp->resv_info.optiosize;


		determine_remaining_bandwidth( devnp, time(0), &maxreqsize, &maxreqtime);

		total 		= (int)griostp->gs_maxresv;
		seconds 	= (int)(maxreqtime / USEC_PER_SEC);
		if (seconds) {
			remaining  = (int)(maxreqsize/seconds) /
					(int)griostp->gs_optiosize;
			griostp->gs_count = total - remaining;
		} else {
			griostp->gs_count = total;
		}

	} else {
		griostp->gs_count 	= -1;
		griostp->gs_maxresv   	= -1;
		griostp->gs_optiosize  	= -1;
	}

	return( 0 );
}
