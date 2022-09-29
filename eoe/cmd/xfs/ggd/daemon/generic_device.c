#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <grio.h>
#include <sys/ktime.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

extern int	time_is_valid(grio_resv_t *);

/*
 * reserve_device_bandwidth()
 *
 *	This routine reserves the bandwidth on the device.
 *
 * RETURN:
 *	0 if the reservation was successful
 *	non-zero on error
 */
/*ARGSUSED*/
int
reserve_device_bandwidth(
	grio_cmd_t		*griocp,
	stream_id_t		*gr_stream_id,
	int			*maxreqsize,
	time_t			*maxreqtime,
	int			*total_slots,
	int			*max_count_per_slot)
{
	dev_t			dev;
	grio_resv_t		*griorp;
	int			status;
	int			totalreqsize;
	time_t			reqtime;
	time_t			start, duration;
	char			err_dev[DEV_NMLEN];
	int			numbuckets;

	griorp	= GRIO_GET_RESV_DATA( griocp );
	dev	= griocp->one_end.gr_dev;

	/*
	 * check that gr_start and gr_duration are valid
	 */
	if ( !time_is_valid( griorp ) ) {
		griorp->gr_error = EIO;
		return( griorp->gr_error );
	}

/* XXX Move the following 3 lines down to the device reservation layer */
/*
	iosize	= get_grio_iosize_for_dev(dev, 0);

	totalreqsize = ROUND_UP( griorp->gr_opsize, iosize);
*/
	totalreqsize = griorp->gr_opsize;
	if (totalreqsize == 0) {
		return (griorp->gr_error = EIO);
	}

	if (griorp->gr_optime < 0) {
		return (griorp->gr_error = EIO);
	}

	/*
	 * convert duration from useconds to ticks
	 */
	reqtime = griorp->gr_optime / USEC_PER_TICK;

	if (reqtime <= 1)
		reqtime = 1;

	start = (time_t) griorp->gr_start;
	duration = (time_t) griorp->gr_duration;
	numbuckets = 1;

	status = reserve_path_bandwidth(dev, dev, totalreqsize,
			reqtime, numbuckets, start, duration, gr_stream_id,
			maxreqsize, maxreqtime, err_dev, griorp->gr_flags,
			griorp->gr_memloc);

	if (status) {
		/* not enough bandwidth was available,
		 * available bandwidth is in maxreqsize and maxreqtime.
		 */

		griorp->gr_error = status;
		strcpy(griorp->gr_errordev, err_dev);
	}

	return (status);
}

/*
 * unreserve_device_bandwidth()
 *
 *      This routine removes the bandwidth reservations on the device.
 *
 * RETURN:
 *	0 if reservation was successful
 *	non-zero if reservation was unsuccessful.
 */
int
unreserve_device_bandwidth(
	dev_t dev,
	grio_cmd_t *griocp,
	int	*maxreqsize,
	int	*maxreqtime)
{
	stream_id_t		*gr_stream_id;
	grio_resv_t		*griorp;
	__int64_t		lmaxreqtime, lmaxreqsize;
	device_node_t		*dnp;
	int			i;

	griorp = GRIO_GET_RESV_DATA( griocp );

	gr_stream_id = &(GRIO_GET_RESV_DATA(griocp)->gr_stream_id);

	unreserve_path_bandwidth(dev, gr_stream_id,
		griorp->gr_memloc);

	/* get bandwidth on dev */
	dnp = devinfo_from_cache(dev);
	assert( dnp );

	i = determine_remaining_bandwidth( dnp, time(0), &lmaxreqsize, &lmaxreqtime);
	assert( i == 0 );

	/*
	 * Normalize the maxreqtime to 1 second.
	 */
	assert( lmaxreqtime );
	lmaxreqsize = (lmaxreqsize * USEC_PER_SEC)/ lmaxreqtime;

	*maxreqsize = lmaxreqsize;
	*maxreqtime = USEC_PER_SEC;

	return ( 0 );
}
