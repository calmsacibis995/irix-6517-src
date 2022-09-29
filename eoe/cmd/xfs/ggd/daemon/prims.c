#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/prims.c,v 1.69 1999/03/04 02:22:31 rr Exp $"

/*
 *	This file contains the routines which perform the primitive
 * 	reservation activities for the guarantee granting daemon.
 *
 *	prims.c
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <values.h>
#include <invent.h>
#include <time.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <bstring.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <grio.h>
#include <sys/invent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/scsi.h>
#include <sys/sysmacros.h>
#include <sys/bsd_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/ktime.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

#define	ADJUST_FOR_OVERFLOW(crttime, total_time, total_bytes) \
	{								\
		__int64_t limit = (MAXLONGLONG/total_bytes) * total_time;\
		if (crttime >= limit)  {				\
			crttime = limit - 1;				\
			crttime = ((crttime/HZ) * HZ);			\
		}							\
	}

#define	ROUNDUP(a, b)	(((a % b) == 0) ? (a/b) : ((a + b - 1) / b))

device_node_t *error_device_node;	/* device failing bw resv */

/*
 * Internal static function declarations
 */
STATIC int determine_used_bandwidth( device_node_t *, 
	time_t , time_t, __int64_t *, __int64_t	*);

STATIC int determine_nonvod_reserved_bandwidth( device_node_t *, 
	time_t, time_t, __int64_t *, __int64_t *);

STATIC int determine_vod_reserved_bandwidth_on_all_fs_except( device_node_t *,
	time_t, time_t, dev_t, __int64_t *, __int64_t *);

STATIC int count_nonvod_reservation( 
	reservations_t *, __uint64_t, __uint64_t);

STATIC int add_to_lcm_list( __int64_t [], 
	int *,  __int64_t );

STATIC int add_resevs_to_lcm_list( __int64_t [], 
	int *,device_reserv_info_t *,time_t);

STATIC __uint64_t compute_lcm( __int64_t [], 
	int );

STATIC int convert_size_and_time( int, 
	time_t, int, time_t, int *, time_t *);

STATIC int determine_vod_iops_reserved_on_fs(
	device_node_t *, time_t, time_t, dev_t, int *, int *, int *);

STATIC int add_to_unique_disk_list( int	, dev_t	[], dev_t );

/*
 * Forward declarations of internal functions.
 */
int determine_remaining_bandwidth( device_node_t *,time_t,
	__int64_t *, __int64_t *);

reservations_t *get_resv_from_id( reservations_t *, 
	dev_t, stream_id_t *);

/*
 * add_reservation ()
 *
 *      Insert the given reservation into the reservation list for 
 *	the given device. The list is sorted by the sorttime field.
 *
 * RETURN:
 *      nothing
 */
void
add_reservation(
	device_reserv_info_t	*dresvp, 
	reservations_t 		*resvp)
{
        reservations_t *prevp;
	reservations_t *list;

	list = dresvp->resvs;
	dresvp->numresvs++;
        if ((!list) ||  (resvp->sort_time < list->sort_time)) {
                resvp->nextresv = list;
		dresvp->resvs = resvp;
        } else  {
                prevp = list;
                while ((prevp->nextresv) &&
                        (prevp->nextresv->sort_time < resvp->sort_time)) {
                        prevp = prevp->nextresv;
                }
                resvp->nextresv = prevp->nextresv;
                prevp->nextresv = resvp;
		dresvp->resvs = list;
        }
	return;
}

/*
 * remove_reservation()
 *
 *      Remove the given reservation structure from the specified
 *	reservation list. Assert that the reservation always exists in
 *	the reservation list.
 *
 * RETURN:
 *      nothing
 */
void
remove_reservation( 
	device_reserv_info_t	*dresvp, 
	reservations_t 		*resvp)
{
        reservations_t *prevp, *list;

	list = dresvp->resvs;
	dresvp->numresvs--;
        if (list == resvp) {
                list = list->nextresv;
        } else {
                prevp = list;

                while ( prevp && (prevp->nextresv != resvp) ) {
                        prevp = prevp->nextresv;
                }
		assert( prevp );

                prevp->nextresv = resvp->nextresv;
        }
	dresvp->resvs = list;

        resvp->nextresv = NULL;
	free(resvp);

	return;
}

/*
 * remove_reservation_with_id
 *	Remove all the reservations from the device specified by
 *	the device_reserv_info structure pointed at by dresvp, which have 
 * 	a stream id matching gr_stream_id.
 *
 *
 * RETURN:
 *	0 if at least one reservation was removed
 *	EIO if no reservations could not be found 
 */
int
remove_reservation_with_id ( 
	device_node_t		*devnp, 
	dev_t			physdev,
	stream_id_t  		*gr_stream_id)
{
	int		status = EIO;
	reservations_t	*resvp;
	device_reserv_info_t	*dresvp = RESV_INFO(devnp);

	/*
 	 * if no reservations can be made on this device, 
	 * then not finding any reservations to removes is not an error
 	 */
	if ( dresvp->maxioops == 0 ) {
		status = 0;
	} else {

		/*
	 	 * remove reservations with matching ids.
	 	 */
		while (resvp = get_resv_from_id(dresvp->resvs,physdev,
					gr_stream_id)) {
			if (IS_END_RESV(resvp)) {
				switch(devnp->flags) {
					case INV_PCI_BRIDGE:
					case INV_XBOW:
						access_guaranteed_registers(devnp, resvp);
						break;
					default:
						break;
				}
			}
			remove_reservation( dresvp, resvp);
			status = 0;
		}
	}

	return  ( status );
}

/*
 * get_resv_from_id()
 *
 *	Scan the list of reservations pointed at by the resvp pointer.
 *	Return a pointer to the reservation_t structure of the first 
 *	reservation in the list that has a stream id that matches the
 *	specified stream id, gr_stream_id, and whose disk device matches 
 *	the physdev.
 *	There may be multiple reservations on a given device with the
 *	same stream_id. This is because they there may be multiple
 *	disks involved with a single reservation.
 *
 * RETURN:
 *	a pointer to the first matching reservation in the list
 *	NULL if no matching reservation was found in the list
 */
reservations_t *
get_resv_from_id( 
	reservations_t	*resvp, 
	dev_t		physdev,
	stream_id_t 	*gr_stream_id)
{
	uint_t		status;
	dev_resv_info_t	*resvpdp;
	

        while (resvp) {

		resvpdp = DEV_RESV_INFO( resvp );
		if ( EQUAL_STREAM_ID( resvp->stream_id, (*gr_stream_id)) &&
		     ((physdev == 0 ) || (resvpdp->diskdev == physdev)) ) {
                        return( resvp );
                }
                resvp = resvp->nextresv;
        }

	assert( resvp == NULL );
        return( resvp );
}

/*
 * add_start_stop_reservations()
 *
 *	This routine adds two new reservation structures to the given 
 *	reservation list. Rreservation lists are sorted	on the sort_time 
 * 	field of the reservation structure. A reservation structure in the 
 *	list can indicate either the starting or the ending of a reservation.
 * 
 *	The first reservation structure is marked as a start reservation and 
 *	is inserted into the reservation list with sort_time set to start_time.
 *  	This marks the start of the new	reservation. The second reservation 
 * 	structure is marked as an end reservation ans is inserted into the
 *	reservation list with sort_time set to start_time + duration. This 
 *	structure marks the end of the reservation.
 *
 * RETURN:
 *	always returns 0.
 *	(ggd will exit on a malloc failure)
 */
int
add_start_stop_reservations( 
	device_reserv_info_t	*dresvp,
	time_t			start_time, 
	time_t			duration_time,
	dev_t			fs_dev,
	int			arg1,
	int			arg2,
	int			perdevreqnumiops,
	int			perdevreqsize,
	int			perdevreqtime,
	dev_t			physdev,
	stream_id_t 		*gr_stream_id, 
	int			flags)
{
        reservations_t		*startresvp, *endresvp;
	dev_resv_info_t		*resvpdp;

	if ( (startresvp = malloc( sizeof (struct reservations ))) == NULL) {
		ggd_fatal_error("malloc failed");
	}

	if ( (endresvp = malloc( sizeof (struct reservations ))) == NULL) {
		ggd_fatal_error("malloc failed");
	}

	
	resvpdp = DEV_RESV_INFO( startresvp );

	/*
	 * fill in applicable reservation information
 	 */
	startresvp->start_time	= start_time;
	startresvp->end_time	= start_time + duration_time;
	startresvp->flags	= flags;
	startresvp->prdevprresvarg1	= arg1;
	startresvp->prdevprresvarg2	= arg2;
	startresvp->fs_dev	= fs_dev;

	COPY_STREAM_ID( (*gr_stream_id), startresvp->stream_id );
	startresvp->nextresv	= NULL;

	resvpdp->io_time	= perdevreqtime;
	resvpdp->num_iops	= perdevreqnumiops;
	resvpdp->iosize		= perdevreqsize;
	resvpdp->diskdev	= physdev;

	/*
	 * structure copy
	 */
	*endresvp = *startresvp;

	/*
	 * mark this reservation structure as the
	 * beginning of the  new reservation.
	 */
	SET_START_RESV( startresvp );
	startresvp->sort_time	  = start_time;
	add_reservation( dresvp, startresvp );

	/*
	 * mark this reservation structure as the 
	 * expiration of the new reservation.
	 */
	SET_END_RESV( endresvp );
	endresvp->sort_time	  = start_time + duration_time;
	add_reservation( dresvp, endresvp );

	return(0);
}



/*
 * can_add_vod_reservation()
 *	A request has been made for a new VOD guarantee. Determine if 
 *	enough bandwidth remains on the given device to satisfy the 
 *	guarantee. 
 *
 *	The request is for a guarantee of "perdevreqsize" bytes every
 *	"perdevreqtime" ticks. The request is from a virtual disk which has 
 *	"numbuckets" VOD buckets in its rotor. (it this is not a disk device,
 *	VOD reservations with different numbers of VOD buckets may exist) 
 * 	The guarantee stream will begin at "start_time"	seconds and continue 
 *	for "duration" seconds. The physical disk this reservation is 
 *	associated with is "physdev" and the stream id is "gr_stream_id"
 *
 *	The routine determines the maximum bandwidth use of the 
 *	reservations currently guaranteed on this device, between start_time
 * 	and start_time + duration.  This is done by computing the device
 *	bandwidth currently reserved by NONVOD reservations in terms of
 *	bytes per time quantum, and the amount of device bandwidth 
 *	currently reserved by VOD reservations in terms of rotor slots used.
 *
 *	The NONVOD reserved banwidth is subtracted from the total device
 *	bandwidth. This remaining device bandwidth is converted into the
 *	number of available ROTOR entries. The number of ROTOR entries
 *	consumed by current VOD reservations is subtracted. If the
 *	new reservation request  can be satisfied by this remaining number
 *	of ROTOR entries, then the request can be granted.
 *	





 *
 *
 * RETURNS:
 *	1 if the VOD reservation request can be satisfied
 *	0 if not enough bandwidth exists to grant the request,
 *		*err_dev will contain a string indicating the name of 
 * 		the device with insufficient bandwidth.
 *
 * OUTPUT PARAMETERS:
 *	The bandwidth remaining on the specified device is described by
 *	*maxreqsize bytes every *maxreqtime micro seconds.
 *	*err_dev will contain a string indicating the name of the device
 *
 */
/* ARGSUSED */
int
can_add_vod_reservation( 
	device_node_t	*dnp, 
	dev_t		fs_dev,
	int 		perdevreqsize, 
	int 		perdevreqtime, 
	int		numbuckets,
	time_t 		start_time, 
	time_t 		duration_time,  
	dev_t		physdev,
	stream_id_t 	*gr_stream_id, 
	int 		*maxreqsize,
	time_t		*maxreqtime,
	char 		*err_dev)
{
	int			slots_per_bucket;
	int			used_entries, needed_entries;
	int			remaining_entries, total_entries;
	int			newreqsize, disknode, maxsingleslot, buckets;
	time_t			end_time, newreqtime, entries_per_bucket;

	__uint64_t		crttime;
	__int64_t		multiple;
	__int64_t		vod_bytes,		vod_time;
	__int64_t		total_bytes,		total_time;
	__int64_t		nonvod_bytes,		nonvod_time;
	__int64_t		remaining_bytes,	remaining_time;

	device_reserv_info_t	*dresvp;
	int                     lcmarraycnt = 0;
	__int64_t               lcmarray[MAX_NUM_RESERVATIONS];
	int			length = DEV_NMLEN;

	/*
	 * get the reservation information for this device
	 */
	dresvp = RESV_INFO(dnp);

	/*
	 * determine if this device is a disk drive
	 */
	disknode = 0;
	if (dnp->flags == INV_SCSIDRIVE)
		disknode = 1;
	
	/*
	 * if this is disk drive, special checks need to be made
	 */
	if ( disknode ) {
		/*
		 * for a VOD reservation, the predevreqsize must be
		 * a multiple of the disk deivce i/o size
	 	 */
		if ( perdevreqsize % dresvp->optiosize) {
			dev_to_devname(dnp->node_number, err_dev, &length);
                	*maxreqsize = dresvp->optiosize;
                	*maxreqtime = perdevreqtime;
                	dbgprintf(1,("VOD reserv: perdevreqsize wrong\n"));
                	return( 0 );
		}

		/*
		 * Is it necessary to check that the "numbuckets"
	 	 * for this reservation is equal to the "total_slots"
		 * count of existing VOD reservations?
	 	 *
	 	 * XXX - not at this time
		 */
	}

	/*
	 * compute reservation expiration time
 	 */
	end_time = start_time + duration_time;

	/*
	 * determine the bandwidth already 
	 * used by NONVOD reservations
	 */
	determine_nonvod_reserved_bandwidth( dnp,
		start_time, end_time, &nonvod_bytes, &nonvod_time);

	/*
	 * determine the bandwidth already 
	 * used by VOD reservations NOT on this file system
	 */
	determine_vod_reserved_bandwidth_on_all_fs_except( dnp, 
		start_time, end_time, fs_dev,  &vod_bytes, &vod_time );

	/*
	 * determine total bandwidth available on the device
	 */
	total_bytes 	= ((__int64_t)(dresvp->maxioops))*dresvp->optiosize;
	total_time	= HZ;

        /*
         * calculate a common time period - in ticks
         */
        add_to_lcm_list( lcmarray, &lcmarraycnt, total_time);
        add_to_lcm_list( lcmarray, &lcmarraycnt, vod_time);
        add_to_lcm_list( lcmarray, &lcmarraycnt, nonvod_time);
        crttime = compute_lcm( lcmarray, lcmarraycnt );
/*
	assert( (crttime % total_time) == 0 );
	assert( (crttime % vod_time) == 0 );
	assert( (crttime % nonvod_time) == 0 );
*/
	ADJUST_FOR_OVERFLOW(crttime, total_time, total_bytes);

        /*
         * convert byte values to a common base of crttime ticks
         */
	multiple	= crttime/total_time;
        total_bytes     = total_bytes*multiple;

	assert( total_bytes >= 0 );

	multiple	= ROUNDUP(crttime, vod_time);
        vod_bytes  	= vod_bytes * multiple;
	assert( vod_bytes >= 0 );

	multiple	= ROUNDUP(crttime, nonvod_time);
        nonvod_bytes    = nonvod_bytes * multiple;
	assert( nonvod_bytes >= 0 );

        /*
         * compute device bandwidth remaining on device
	 * by subtractint used  bandwidth from total device bandwidth
 	 */
        remaining_bytes = total_bytes - vod_bytes - nonvod_bytes;
	remaining_time  = crttime;

	/*
	 * convert remaining_bytes  to a time quantum of HZ ticks, one second.
	 */
	crttime		= HZ;
	remaining_bytes	= (remaining_bytes	* crttime)/remaining_time;

	/*
	 * calculate the number of slots per bucket remaining after
	 * the nonvod reservations have been subtracted out
	 */
	slots_per_bucket =  remaining_bytes / dresvp->optiosize;


	/*
	 * determine the number of vod entries used on this file system 
	 */
	determine_vod_iops_reserved_on_fs( dnp,
		start_time, end_time, fs_dev, 
		&maxsingleslot, &buckets, &used_entries);

	if ( disknode ) {
		assert( (buckets == 0) || (numbuckets == buckets) );
	} else {
		numbuckets = buckets;
	}
	

	/*
	 * convert the reservation to be added to a time quantum  
 	 * of HZ ticks, one second
 	 */
	convert_size_and_time( 
		perdevreqsize,
		perdevreqtime,
		dresvp->optiosize, HZ,
		&newreqsize, &newreqtime);

	assert( newreqtime == HZ );

	/*
	 * calculate the VOD entries needed to satisfy this new reservation
 	 */
	needed_entries = newreqsize / dresvp->optiosize; 
	assert( needed_entries );

	/*
	 * calculate the total number of VOD entries needed if this
	 * reservation is accecpted.
 	 */
	total_entries = used_entries + needed_entries;

	/*
	 * if disknode, convert total number of VOD entries to 
 	 * number of entries per VOD rotor bucket
	 */
	if ( buckets ) {
		entries_per_bucket = (total_entries + buckets - 1) / buckets;
	} else {
		entries_per_bucket = 0;
	}

	/*
 	 * the number of entries needs to be an even multiple of the
 	 * largest number of entries needed by a single reservation in a
	 * single time quantum, this prevents the possibility of 
 	 * a single reservations requests been spread over multiple
	 * time quantums.
 	 *
	 * for example:
	 *	assume numbuckets is 4
	 *	optiosize is 64k
	 *	and remaining slots_per_bucket after subtracting out
	 *	NONVOD reservations is 1
 	 *	
	 *	if the new reservation is for 128K bytes per second
	 *	(assuming a disk stripe step size of 256)
	 *	then 
 	 *
	 *	need_entries = 2
	 *	total_entries = 0 + 2 = 2 
	 *	entries_per_bucket = (2 + 4 - 1) /4 =  1
	 *	
	 *	so the request would be granted. However, the
	 *	guarantee could not be met. It is required that
	 * 	both of the I/O operations occur in the same time quantum
	 *	therefore it is necessary to check that the entries_per_bucket
	 *	count is an even multiple of maxsingleslot.
 	 */
	if ( entries_per_bucket % maxsingleslot ) {
		entries_per_bucket = ((entries_per_bucket/maxsingleslot) + 1) *
					maxsingleslot;
	}

	/*
	 * calculate remaining VOD entries
	 */
	remaining_entries = (slots_per_bucket - entries_per_bucket)*buckets;

	*maxreqtime = HZ * USEC_PER_TICK;

	/*
	 * check if the bandwidth is available on the device
	 */
	if ( remaining_entries >= 0  ) {
		/*
		 * bandwidth is available
 		 * Normalized to a maxreqtime of 1 second.
		 */
		assert( *maxreqtime == USEC_PER_SEC );
		*maxreqsize =  remaining_entries * dresvp->optiosize;	
		return( 1 );
	} else {

		/*
	 	 * bandwidth is not available
		 */
		*maxreqsize = 0;
		dev_to_devname(dnp->node_number, err_dev, &length);
		dbgprintf(1,("out of bandwidth \n"));
		return( 0 );
	}
}


/*
 * can_add_nonvod_reservation()
 *	A request has been made for a new NONVOD guarantee. Determine if 
 *	enough bandwidth remains on the given device to satisfy the 
 *	guarantee. 
 *
 *	The request is for a guarantee of "perdevreqsize" bytes every
 *	"perdevreqtime" ticks. 
 * 	The guarantee stream will begin at "start_time"	seconds and continue 
 *	for "duration" seconds. The physical disk this reservation is 
 *	associated with is "physdev" and the stream id is "gr_stream_id"
 *
 *	The routine determines the maximum bandwidth use of the 
 *	reservations currently guaranteed on this device, between start_time
 * 	and start_time + duration. determine_used_bandwidth() returns
 *	a ratio of bytes per time quantum to describe the currently	
 *	reserved device bandwidth.  The bandwidth requirements of the new
 *	reservation request are added to this ratio, and available of
 *	the bandwidth is determined.
 *
 *
 * RETURNS:
 *	1 if the NONVOD reservation request can be satisfied
 *	0 if not enough bandwidth exists to grant the request,
 *		*err_dev will contain a string indicating the name of 
 * 		the device with insufficient bandwidth.
 *
 * OUTPUT PARAMETERS:
 *	The bandwidth remaining on the specified device is described by
 *	*maxreqsize bytes every *maxreqtime micro seconds.
 *	*err_dev will contain a string indicating the name of the device
 *
 */
/* ARGSUSED */
int
can_add_nonvod_reservation(
	device_node_t	*dnp, 
	int 		perdevreqsize, 
	int 		perdevreqtime, 
	time_t 		start_time, 
	time_t 		duration_time,  
	dev_t		physdev,
	stream_id_t 	*gr_stream_id, 
	int 		*maxreqsize,
	time_t		*maxreqtime,
	char 		*err_dev, 
	int 		flags)
{
	int			lcmarraycnt = 0;
	__int64_t		lcmarray[MAX_NUM_RESERVATIONS];
	__uint64_t		crttime;
	__int64_t		multiple;
	time_t			end_time;
	device_reserv_info_t	*dresvp;
	__int64_t		reserved_bytes, reserved_time;
	__int64_t		total_bytes, 	total_time;
	__int64_t		needed_bytes, 	needed_time;
	__int64_t		result_bytes;
	int			length = DEV_NMLEN;

	/*
	 * get the reservation information for this device
	 */
	dresvp = RESV_INFO(dnp);

	/*
	 * round the request size up to a multiple of the device iosize
	 */
	if ( perdevreqsize % dresvp->optiosize )  {
		perdevreqsize = ((perdevreqsize / dresvp->optiosize) + 1) *
			dresvp->optiosize;
	}

	/*
	 * calculate the expiration time of the new reservation.
	 */
	end_time = start_time + duration_time;

	/*
	 * determine bandwidth needed for this reservation
 	 */
	needed_bytes	= perdevreqsize;
	needed_time	= perdevreqtime;

	/* 	
	 * determine tne total maximum bandwidth of the device
         */
        total_bytes = ((__int64_t)dresvp->maxioops)*dresvp->optiosize;
        total_time = HZ;

	/*
	 * determine bandwidth already reserved on the system
	 */
	determine_used_bandwidth(dnp, 
		start_time, end_time, &reserved_bytes, &reserved_time) ;

	assert( reserved_bytes >= 0 );

	/*
	 * convert reserved time base to ticks;
	 */
	reserved_time = reserved_time / USEC_PER_TICK;

	/*
	 * calculate a common time period - in ticks
	 */
	add_to_lcm_list( lcmarray, &lcmarraycnt, total_time);
	add_to_lcm_list( lcmarray, &lcmarraycnt, reserved_time);
	add_to_lcm_list( lcmarray, &lcmarraycnt, needed_time);
	crttime = compute_lcm( lcmarray, lcmarraycnt );
/*
	assert( (crttime % total_time) == 0 );
	assert( (crttime % reserved_time) == 0 );
	assert( (crttime % needed_time) == 0 );
*/
	ADJUST_FOR_OVERFLOW(crttime, total_time, total_bytes);

	/*
	 * convert byte values to a common base of crttime ticks
	 */
	multiple	= crttime/total_time;
	total_bytes     = total_bytes * multiple;

	assert( total_bytes >= 0 );

	multiple	= ROUNDUP(crttime, reserved_time);
	reserved_bytes	= reserved_bytes * multiple;
	assert( reserved_bytes >= 0 );

#if 0
	if ( (crttime % needed_time) != 0 ) {
		printf("crttime = %lld, needed_time = %lld \n",
			crttime, needed_time);
	}
#endif

	multiple	= ROUNDUP(crttime, needed_time);
	needed_bytes	= needed_bytes * multiple;
	assert( needed_bytes >= 0 );

	/*
	 * compute device bandwidth remaining
	 * if reservation request is granted.
	 */
	result_bytes = total_bytes - reserved_bytes - needed_bytes;

	/*
	 * convert ticks to microseconds
 	 */
	*maxreqtime = (time_t)(crttime * USEC_PER_TICK);

	if ( result_bytes < 0 ) {
		/*
		 * device bandwidth exceeded, return maximum 
		 * bandwidth currently available
		 * Normalize to a maxreqtime of 1 second.
		 */
		result_bytes = total_bytes - reserved_bytes;
		result_bytes = (result_bytes * HZ) / crttime;

		*maxreqsize = (int)result_bytes;
		*maxreqtime = USEC_PER_SEC;

		dev_to_devname(dnp->node_number, err_dev, &length);
		dbgprintf(1,("out of bandwidth \n"));
		return( 0 );
	} else {
		/*
		 * device bandwidth available, return remaining
		 * bandwidth. Normalize bytes/time to bytes/second.
		 */
		result_bytes = (result_bytes * HZ) / crttime;

		*maxreqsize = result_bytes;
		*maxreqtime = USEC_PER_SEC;
	}
	return( 1 );
}




/*
 * reserve_device_bandwidth()
 *
 *	Call the appropriate routine to determine if the new VOD or NONVOD
 *	rate reservation can be accepted by this device. The reservation
 *	request is for maxreqsize bytes every perdevreqtime ticks over the
 *	period of start_time to start_time + duration seconds.
 *	If the rate guarantee can be satisfied, the reservation is added
 *	to the device reservation list.
 *
 * RETURN:
 *      0 	if bandwidth was reserved 
 *	EACCESS if HARD type guarantee not available of this device
 *     	ENOSPC  if the device does not have enough bandwidth available
 *
 * OUTPUT PARAMETERS:
 *	The bandwidth remaining on the specified device is described by
 *	*maxreqsize bytes every *maxreqtime micro seconds.
 *	*err_dev will contain a string indicating the name of the device
 
 */
int
reserve_bandwidth( 
	device_node_t	*dnp, 
	dev_t		fs_dev,
	int 		perdevreqsize, 
	int 		perdevreqtime, 
	int		arg1,
	int		arg2,
	time_t 		start_time, 
	time_t 		duration_time,  
	dev_t		physdev,
	stream_id_t 	*gr_stream_id, 
	int 		*maxreqsize,
	time_t		*maxreqtime,
	char 		*err_dev, 
	int 		flags)
{
	int			addreservation, perdevreqnumiops;
	device_reserv_info_t	*dresvp;

	/*
	 * get the reservation information for this device
	 */
	dresvp = RESV_INFO(dnp);

	/*
 	 * if the device does not have valid bandwidth information, 
	 * cannot make a reservation
	 */
	if ( !dresvp->maxioops ) {
		return ( ENOSPC );
	}

	/*
	 * round the request size up to a multiple of the device iosize
	 */
	perdevreqsize =	ROUND_UP( perdevreqsize, dresvp->optiosize );

	if ( VOD_RESERVATION( flags ) ) {
		/*
		 * determine if a VOD reservation can be made
		 */
		addreservation = can_add_vod_reservation(
					dnp, 
					fs_dev,
					perdevreqsize, 
					perdevreqtime, 
					arg1,
					start_time, 
					duration_time,  
					physdev,
					gr_stream_id, 
					maxreqsize,
					maxreqtime,
					err_dev );
	} else {
		/*
	 	 * determine if a NONVOD reservation can be made
		 */
		addreservation = can_add_nonvod_reservation(
					dnp, 
					perdevreqsize, 
					perdevreqtime, 
					start_time, 
					duration_time,  
					physdev,
					gr_stream_id, 
					maxreqsize,
					maxreqtime,
					err_dev, 
					flags);
	}

	if (addreservation) {
		/*
 	 	 * Success! this reservation request can be added.
	 	 */
		perdevreqnumiops = perdevreqsize / dresvp->optiosize;

		add_start_stop_reservations( dresvp, 
			start_time, duration_time, fs_dev, arg1, arg2,
			perdevreqnumiops, perdevreqsize, perdevreqtime, 
			physdev, gr_stream_id, flags);
		return( 0 );
	} else {
		error_device_node = dnp;
		return( ENOSPC );
	}
}


/*
 * remove_expired_reservations
 *
 *	This routine removes expired reservation requests from the 
 * 	reservation list. When a reservation expires, both the starting and
 * 	ending reservation structures are removed from the list.
 *
 * RETURN:
 *	none.	
 */
void
remove_expired_reservations( 
	device_node_t		*devnp, 	
	time_t 			now)
{
	int 		i, count;
	reservations_t	*resvp, *nextresvp;
	device_reserv_info_t	*dresvp = RESV_INFO(devnp);

        resvp = dresvp->resvs;
	count = dresvp->numresvs;

        for ( i = 0; i < count; i++ ) {
		nextresvp = resvp->nextresv;

		/*
		 * This will remove both START and END type reservations.
		 */
		if ( resvp->end_time < now) {
			if ( resvp->flags & SYSTEM_GUAR)
				ggd_fatal_error("System guarantees should never expire.");
			/*
			 * sanity for both START and END resvs.
			 */
			assert(RESV_STARTED(resvp));
			if (IS_END_RESV(resvp)) {
				switch(devnp->flags) {
					case INV_PCI_BRIDGE:
					case INV_XBOW:
						access_guaranteed_registers(devnp, resvp);
						break;
					default:
						break;
				}
			}
			remove_reservation( dresvp, resvp);
		}

		resvp = nextresvp;
        }
	return;
}

/*
 * count_nonvod_reservation()
 *	Add or subtract the bandwidth used by this NONVOD reservation. 
 * 
 *
 * RETURNS:
 *	current reservation count.
 */
int
count_nonvod_reservation( 
	reservations_t 	*resvp, 
	__uint64_t	count,
	__uint64_t 	crttime)
{

	__uint64_t	thisres;

	dev_resv_info_t	 *resvpdp = DEV_RESV_INFO( resvp );
	
	/*
	 * Would have like to assert, but ADJUST_FOR_OVERFLOW
	 * might make this a false assert.
	 * assert( (crttime % resvpdp->io_time) == 0LL );
	 */

	thisres = (ROUNDUP(crttime, resvpdp->io_time) * resvpdp->num_iops);

	if (IS_END_RESV(resvp)) {
		count -= thisres;
	} else if (IS_START_RESV(resvp)) {
		count += thisres;
	} else {
		printf("UNKNOWN RESV TYPE ON LIST\n");
	}
	return( count );
}


/*
 * add_to_lcm_list
 *	Scan the lcm list and if "newtime" is not in the list
 *	add it.
 *
 * RETURNS: 
 *	1 if newtime was added to the list
 *	0 if it was not
 */
int
add_to_lcm_list( __int64_t lcmarray[], int *lcmarraycnt,  __int64_t newtime)
{
	int 	i;

	for ( i = 0; i < *lcmarraycnt; i++ ) {
		if ( lcmarray[i] == newtime ) {
			return( 0 );
		}
	}
	lcmarray[*lcmarraycnt] = newtime;
	(*lcmarraycnt) = (*lcmarraycnt) + 1;

	return( 1 );
}

/*
 * add_resevs_to_lcm_list
 *	Scan the device reservation list and add the 
 *	periods of the reservations active at time "when"
 *	to the lcm list.
 *
 * RETURNS:
 *	always returns 1
 */
int
add_resevs_to_lcm_list(
	__int64_t 		lcmarray[], 
	int 			*lcmarraycnt, 
	device_reserv_info_t	*dresvp,
	time_t 			when)
{
        reservations_t		*resvp;
	dev_resv_info_t		*resvpdp;

	resvp = dresvp->resvs;
	while ( resvp && (resvp->sort_time <= when) ) {

		resvpdp = DEV_RESV_INFO( resvp );

		if (IS_START_RESV(resvp)) {
			add_to_lcm_list( lcmarray, lcmarraycnt, 
				(__int64_t)resvpdp->io_time);
		}
		resvp = resvp->nextresv;
	}
	return ( 1 );
}

/*
 * compute_lcm
 *	Compute the multiple of the values in
 *	the lcmarray.
 *
 *
 * RETURNS:
 *	the multiple of all the elements in the lcmarray
 */
__uint64_t
compute_lcm( __int64_t lcmarray[], int lcmarraycnt)
{
	int		i;
	__uint64_t	lcm = 1LL;

	for ( i = 0; i < lcmarraycnt; i++) {
		if (i) {
		    /* compute LCM against next element using Euclid */
		    __uint64_t m = lcm;
		    __uint64_t n = lcmarray[i];
		    assert(n);
		    if (!n) {
			continue;
		    }
		    for (;;) {
			__uint64_t r = m % n;
			if (r == 0) {
			    break;
			}
			m = n;
			n = r;
		    }
		    m = lcmarray[i] / n;
		    if (m > (MAXLONGLONG/lcm)) {
			/* LCM overflows our int. */
			/* Callers handle rounding if overflow occurs. */
			return MAXLONGLONG;
		    }
		    lcm *= m;
		} else {
		    lcm = lcmarray[i];
		}
	}
	if ( lcm == 1LL )
		return( 0LL );
	else
		return( lcm );
}


/* 
 * determine_used_bandwidth()
 *
 *	Determine the maximum amount of bandwidth reserved on the given  
 *	device over the time period start_time to end_time. 
 *	This is calculated by determining the amount of bandwidth used
 *	by VOD reservations and the about of bandwidth used by NONVOD
 * 	reservations. These values are added and the result is returned.
 *
 * RETURNS:
 *	0 on success
 *	non-zero on error
 *	
 * OUTPUT PARAMETERS:
 *	The reserved bandwidth on the specified device is described by
 *	*maxreqsize bytes every *maxreqsize micro seconds.
 *
 */
STATIC int
determine_used_bandwidth(
	device_node_t	*dnp, 
	time_t		start_time,
	time_t 		end_time, 
	__int64_t 	*maxreqsize,
	__int64_t	*maxreqtime)
{
	int			lcmarraycnt = 0;
	__int64_t		lcmarray[MAX_NUM_RESERVATIONS];
	__uint64_t		crttime;
	__int64_t		multiple;
	__int64_t		vod_bytes,	vod_time;
	__int64_t		nonvod_bytes,	nonvod_time;
	__int64_t		result_bytes,	result_time;
	__int64_t		total_time = HZ, total_bytes;
	device_reserv_info_t	*dresvp;

	/*
	 * get the reservation information for this device
	 */
	dresvp = RESV_INFO(dnp);

	/*
 	 * if the device does not have valid bandwidth information, 
	 * there are not reservations
	 */
	if ( ! dresvp->maxioops ) {
		return ( ENOSPC );
	}

	determine_vod_reserved_bandwidth_on_all_fs_except(dnp, 
		start_time, end_time, 0, &vod_bytes, &vod_time);

	assert( vod_time );
	assert( vod_bytes >= 0 );

	determine_nonvod_reserved_bandwidth(dnp, 
		start_time, end_time, &nonvod_bytes, &nonvod_time);

	assert( nonvod_time );
	assert( nonvod_bytes >= 0 );

	/*
	 * calculate a common time period - in ticks
	 */
	add_to_lcm_list( lcmarray, &lcmarraycnt, vod_time);
	add_to_lcm_list( lcmarray, &lcmarraycnt, nonvod_time);
        crttime = compute_lcm( lcmarray, lcmarraycnt );
/*
	assert( (crttime % vod_time) == 0 );
	assert( (crttime % nonvod_time) == 0 );
*/
        total_bytes = ((__int64_t)dresvp->maxioops)*dresvp->optiosize;
	ADJUST_FOR_OVERFLOW(crttime, total_time, total_bytes);

	/*
	 * convert vod_bytes and nonvod_bytes
	 * a common base of crttime ticks
 	 */
	multiple 	= ROUNDUP(crttime, vod_time);
	vod_bytes 	= vod_bytes * multiple;
	assert( vod_bytes >= 0 );

	multiple 	= ROUNDUP(crttime, nonvod_time);
	nonvod_bytes 	= nonvod_bytes * multiple; 
	assert( nonvod_bytes >= 0 );

	/*
	 * calculate used bandwidth
	 */
	result_bytes = vod_bytes + nonvod_bytes;
	result_time  = crttime;

	/*
	 * convert the ticks time to useconds
	 */
	result_time = result_time*USEC_PER_TICK;

	/* 
	 * return time period
	 */
	*maxreqtime = result_time;

	/* 
 	 * return byte amount
	 */
	*maxreqsize = result_bytes;

	return( 0 );
}

/*
 * determine_remaining_bandwidth() 
 *
 *	Determine the amount of bandwidth remaining on the given device
 *	device at time "when". 
 *	This is calculated by determining the amount of bandwidth used
 *	by VOD reservations and the about of bandwidth used by NONVOD
 * 	reservations. These values are subtracted from the total amount
 * 	of bandwidth supported by the device and the result is returned.
 *
 * RETURNS:
 *	0 on success
 *	non-zero on error
 *
 * OUTPUT PARAMETERS:
 *	The remaining bandwidth on the specified device is described by
 *	*maxreqsize bytes every *maxreqsize micro seconds.
 */
int
determine_remaining_bandwidth(
	device_node_t	*dnp, 
	time_t 		when, 
	__int64_t 	*maxreqsize,
	__int64_t	*maxreqtime)
{
	int			lcmarraycnt = 0;
	__int64_t		lcmarray[MAX_NUM_RESERVATIONS];
	__uint64_t		crttime;
	__int64_t		multiple;
	__int64_t		vod_bytes,	vod_time;
	__int64_t		nonvod_bytes,	nonvod_time;
	__int64_t		total_bytes,	total_time;
	__int64_t		result_bytes,	result_time;
	device_reserv_info_t	*dresvp;

	/*
	 * get the reservation information for this device
	 */
	dresvp = RESV_INFO(dnp);

	/*
 	 * if the device does not have valid bandwidth information, 
	 * there are not reservations
	 */
	if ( ! dresvp->maxioops ) {
		return ( ENOSPC );
	}

	determine_vod_reserved_bandwidth_on_all_fs_except(dnp, 
		0, when, 0, &vod_bytes, &vod_time);

	determine_nonvod_reserved_bandwidth(dnp, 
		0, when, &nonvod_bytes, &nonvod_time);

	/*
	 * determine total maximum bandwidth of the device.
	 */
	total_bytes = ((__int64_t)dresvp->maxioops)*dresvp->optiosize;
	total_time = HZ;

	/*
	 * calculate a common time period - in ticks
	 */
	add_to_lcm_list( lcmarray, &lcmarraycnt, vod_time);
	add_to_lcm_list( lcmarray, &lcmarraycnt, nonvod_time);
	add_to_lcm_list( lcmarray, &lcmarraycnt, total_time);
        crttime = compute_lcm( lcmarray, lcmarraycnt );
/*
	assert( (crttime % vod_time) == 0 );
	assert( (crttime % nonvod_time) == 0 );
	assert( (crttime % total_time) == 0 );
*/
	ADJUST_FOR_OVERFLOW(crttime, total_time, total_bytes);

	/*
	 * convert vod_bytes, nonvod_bytes, and total_bytes to 
	 * a common base of crttime ticks
 	 */
	multiple	= ROUNDUP(crttime, vod_time);
	vod_bytes 	= vod_bytes * multiple;
	assert( vod_bytes >= 0 );

	multiple	= ROUNDUP(crttime, nonvod_time);
	nonvod_bytes 	= nonvod_bytes * multiple;
	assert( nonvod_bytes >= 0 );


	multiple	= crttime/total_time;
	total_bytes 	= total_bytes * multiple;
	assert( total_bytes >= 0 );

	/*
	 * calculate remaining bandwidth
	 */
	result_bytes = total_bytes - vod_bytes - nonvod_bytes;
	result_time  = crttime;

	/*
	 * convert the ticks time to useconds
	 */
	result_time = result_time*USEC_PER_TICK;

	/* 
	 * return time period
	 */	 
	*maxreqtime = result_time;

	/* 
 	 * return byte amount
	 */
	*maxreqsize = result_bytes;

	return( 0 );
}


/*
 * determine_nonvod_reserved_bandwidth()
 *
 *	Calculate the amount of bandwidth that is reserved on the given
 *	device from start_time to end_time by NONVOD type rate reservations.
 *	This is expressed as a rate of *nonvod_bytes bytes per *nonvod_time
 *	ticks.
 *
 *	This is done by computing a time interval that is an even multiple 
 * 	of each of the time quantums of the existing NONVOD reservations. The 
 *	number of I/O operations required for each reservation over this period 
 * 	is calculated and a total computed. 
 *
 * RETURNS:
 *	always returns zero
 *
 * OUTPUT PARAMETERS:
 *	The NONVOD reserved bandwidth on the specified device is described by
 *	*nonvod_bytes bytes every *nonvod_time ticks.
 *
 */
STATIC int
determine_nonvod_reserved_bandwidth( 
	device_node_t	*dnp,
	time_t		start_time,
	time_t		end_time,
	__int64_t 	*nonvod_bytes,
	__int64_t 	*nonvod_time)
{

	__int64_t               lcmarray[MAX_NUM_RESERVATIONS];
	int                     lcmarraycnt = 0;
	__uint64_t		crttime;
	__int64_t               iocount, maxops;
	__int64_t		highestopcount, total_bytes;
	reservations_t          *resvp;
	device_reserv_info_t    *dresvp;

	
	dresvp = RESV_INFO(dnp);

	/*
	 * add streams to list
	 */
        add_resevs_to_lcm_list( lcmarray, &lcmarraycnt, dresvp, end_time);

	/*
	 * compute common time quantum over all streams
	 */
        crttime = compute_lcm( lcmarray, lcmarraycnt );
	total_bytes = ((__int64_t)(dresvp->maxioops))*dresvp->optiosize;
	ADJUST_FOR_OVERFLOW(crttime, HZ, total_bytes);
	assert((crttime % HZ) == 0);
	
	if ( crttime == 0 ) crttime = HZ;

	maxops = (crttime/HZ) * ((__int64_t)dresvp->maxioops);

        /*
         * tally up the i/o op counts for all overlapping non-VOD 
	 * reservations
         */
        resvp = dresvp->resvs;
	iocount = highestopcount = 0;
        while ( resvp && (resvp->sort_time <= end_time) ) {

                if (!RESV_IS_VOD(resvp)) {
                	iocount = count_nonvod_reservation(
				resvp,iocount,crttime);
		}

                if ( (!RESV_IS_VOD(resvp)) 	&& 
		     (IS_START_RESV(resvp))) {

                        /*
                         * For the period of time that this
			 * reservation is active, determine
                         * the maximum number of i/o ops.
                         */
                        if ( resvp->end_time >= start_time ) {
                                if ( iocount > maxops ) {
					/*
					 * Is it possible for this to 
					 * happen due to rounding?
					 */
					ggd_fatal_error("too many iops");
				} else if (iocount > highestopcount) {
					highestopcount = iocount;
				}
			}
		}
		resvp = resvp->nextresv;
	}

	*nonvod_bytes	= highestopcount *((__int64_t)dresvp->optiosize);
	*nonvod_time	= crttime;

	return( 0 );
}


/*
 * convert_size_and_time
 *
 *
 *
 * convert 1 "reqsize" byte request in "reqtime" ticks into
 * some number of "basesize" byte requests in "basetime" ticks.
 *
 * this is a normalizing function so that VOD requests of 
 * varying size and time quantum can be accounted for accurately.
 *
 * RETURNS:
 *	always returns zero
 */
int
convert_size_and_time(
	int	reqsize, 
	time_t	reqtime, 
	int	basesize, 
	time_t	basetime,
	int	*newreqsize,
	time_t	*newreqtime)
{
	int	numops;

	/*
	 * round the reqsize up to a multiple of the basesize
	 */
	reqsize = ROUND_UP( reqsize, basesize );

	/*	
	 * determine the number of operations that must be done in
	 * "basetime" ticks 
	 */
	numops = basetime / reqtime;
	if (  basetime % reqtime ) {
		numops++;
	}

	/*
	 * the minimum is 1 op per "basetime"
	 */
	if (numops == 0)
		numops = 1;

	/*
	 * it is necessary to issue "numops" number of "basesize" requests in
	 * "basetime" inorder to satisfy the original guarantee
	 */
	assert( (reqsize % basesize) == 0 );

	numops = (numops) * (reqsize/basesize);

	*newreqsize = numops * basesize;
	*newreqtime = basetime;

	return( 0 );
}


/* 
 * determine_vod_reserved_bandwidth_on_all_fs_except()
 *
 *	Calculate the amount of bandwidth that is reserved on the given
 *	device from start_time to end_time by VOD type rate reservations.
 *	EXCEPT those VOD reservations on the file system specified by 
 *	skip_fs_dev. This is expressed as a rate of *vod_bytes bytes 
 * 	per *vod_time ticks.
 *
 *	Call determine_vod_iops_reserved_on_fs() for each file system other
 *	than skip_fs_dev, and total up the num_iops used.
 *
 *	The maximum number of rotor bucket entries used by VOD 
 *	reservations on a single file system is returned in num_iops.
 *	This is converted to the number of rotor entries in a single bucket.
 *	The number of rotor entries per bucket needs to be an even multiple 
 *	of the largest number of entries needed by a single reservation in a
 * 	single time quantum, this prevents the possibility of a single 
 * 	reservation's requests being spread over multiple
 * 	time quantums.
 *
 * for example:
 *	assume numbuckets is 4
 *	optiosize is 64k
 *	and remaining slots_per_bucket after subtracting out
 *	NONVOD reservations is 1
 *	
 *	if the new reservation is for 128K bytes per second
 *	(assuming a disk stripe step size of 256)
 *	then 
 *
 *	need_entries = 2
 *	total_entries = 0 + 2 = 2 
 *	entries_per_bucket = (2 + 4 - 1) /4 =  1
 *	
 *	so the request would be granted. However, the
 *	guarantee could not be met. It is required that
 * 	both of the I/O operations occur in the same time quantum
 *	therefore it is necessary to check that the entries_per_bucket
 *	count is an even multiple of maxsingleslot.
 *
 *	In
 *
 * RETURNS:
 *	always returns zero
 *
 * OUTPUT PARAMETERS:
 *	The VOD reserved bandwidth on the specified device is described by
 *	*vod_bytes bytes every *vod_time ticks.
 */
STATIC int
determine_vod_reserved_bandwidth_on_all_fs_except(
	device_node_t	*dnp,
	time_t		start_time,
	time_t		end_time, 
	dev_t		skip_fs_dev,
	__int64_t	*vod_bytes,
	__int64_t	*vod_time)
{
	int	max_single, num_buckets, num_iops,i;
	int 	iops_per_bucket, total_iops_per_bucket;
	dev_t	fs_dev;
	device_reserv_info_t    *dresvp;

	extern dev_t get_vdev( int * );

	i		= 0;
	total_iops_per_bucket = 0;

	dresvp		= RESV_INFO(dnp);
	/*
	 * look thru the vdisk cache to get the file systems
 	 */
	while ( (fs_dev = get_vdev( &i )) != 0  ) {

		if ( fs_dev == skip_fs_dev ) {
			continue;
		}

		determine_vod_iops_reserved_on_fs( dnp,
			start_time, end_time, fs_dev,
			&max_single, &num_buckets, &num_iops);

		/*
	 	 * convert total number of VOD entries to number of entries
	 	 * per VOD rotor bucket
	 	 */
		if ( num_buckets ) {
			iops_per_bucket = (num_iops + num_buckets - 1) /
						num_buckets;
			assert( iops_per_bucket );
		} else {
			assert( num_iops == 0 );
			assert( max_single == 1 );
			iops_per_bucket = 0;
		}

		/*
	 	 * round up to a even number of max_single, 
		 * (the largest number of rotor entries that need to be 
		 * issued in the same time quantum).
	 	 */
		if ( iops_per_bucket % max_single) {
			iops_per_bucket = ((iops_per_bucket/max_single) + 1) *
					 	max_single;
			assert( iops_per_bucket );
		}

		total_iops_per_bucket += iops_per_bucket;
	}

	*vod_time = HZ;
	*vod_bytes = ((__int64_t)total_iops_per_bucket) *
			((__int64_t)dresvp->optiosize);


	return( 0 );
}


/*
 * determine_vod_iops_reserved_on_fs()
 * 
 *	Count the highest number of VOD rotor entries currently reserved by
 *	active VOD reservations on the given device between start_time and 
 *	end_time. 
 *
 * RETURNS:
 *	always returns zero
 *
 * OUTPUT PARAMETERS:
 *	*maxsingleslot contains the number of  VOD entries that MUST be
 *	performed in a single time quantum
 *	*num_disks contains the number of unique disks in this fs on this 
 *		device, except for disk device where it contains the number
 *		rotor slots
 *	*rotor_entries contains the total number of VOD rotor entries used
 *
 */
/* ARGSUSED */
STATIC int
determine_vod_iops_reserved_on_fs(
	device_node_t	*dnp,
	time_t		start_time,
	time_t		end_time,
	dev_t		fs_dev,
	int		*maxsingleslot,
	int		*num_disks,
	int		*rotor_entries)
{
	int			numslotsused, maxslotsused, disknode;
	int			numbuckets, newreqsize, slots, unique_disks;
	dev_t			disk_list[MAX_ROTOR_SLOTS];
	time_t			newreqtime;
	reservations_t          *resvp;
	device_reserv_info_t    *dresvp;

	dresvp		= RESV_INFO(dnp);
	resvp		= dresvp->resvs;

	bzero( disk_list, sizeof(dev_t)*MAX_ROTOR_SLOTS);
	numslotsused	= 0;
	maxslotsused 	= 0;
	*maxsingleslot	= 0;
	unique_disks	= 0;
	numbuckets	= 0;
	disknode	= 0;

	/*
	 * determine if this is a disk node
	 */
	if (dnp->flags == INV_SCSIDRIVE)
		disknode = 1;
	*maxsingleslot 	= 1;

	while (resvp && (resvp->sort_time <= end_time) ) {
		
		if (	RESV_IS_VOD(resvp)		&&
		    	(resvp->fs_dev == fs_dev ))    {

			convert_size_and_time( 
				DEV_RESV_INFO(resvp)->iosize,
				DEV_RESV_INFO(resvp)->io_time,
				dresvp->optiosize, HZ,
				&newreqsize, &newreqtime);

			unique_disks = add_to_unique_disk_list( 
				unique_disks, disk_list,
				DEV_RESV_INFO(resvp)->diskdev);

			/*
			 * if this is a disk node, the number of buckets
			 * and the iosize must match.
			 */
			if ( disknode ) {
				assert( (DEV_RESV_INFO(resvp)->iosize %
					dresvp->optiosize) == 0 );
				/*
			 	 * get number of buckets in rotor, it will
			 	 * be constant across the file system
			 	 */
				if ( numbuckets == 0 ) {
					numbuckets = resvp->total_slots;
				}
				assert( numbuckets == resvp->total_slots );
			}



			assert( newreqtime == HZ );
			slots = newreqsize / dresvp->optiosize; 
			assert( slots );

			if ( IS_START_RESV( resvp ) )
				numslotsused += slots;
			else
				numslotsused -= slots;

			if ( slots > *maxsingleslot ) {
				*maxsingleslot = slots;
			}
		}
		assert( numslotsused >= 0 );

		if (numslotsused > maxslotsused) 
			maxslotsused = numslotsused;

		resvp = resvp->nextresv;
	}

	/*
	 * return 0 bytes if there are no VOD reservations
	 */
	if ( maxslotsused == 0 ) {
		*num_disks	= 0;
		*rotor_entries	= 0;
		return( 0 );
	}

	/*
	 * return number of disks/ rotor size
	 */
	if (disknode) {
		assert( unique_disks == 1 );
		*num_disks = numbuckets;
	} else {
		*num_disks = unique_disks;
	}

	/*
 	 * return number of VOD reservations set
	 */
	*rotor_entries		= maxslotsused;
	
	return( 0 );
}


STATIC int
add_to_unique_disk_list( 
	int	unique_disks, 
	dev_t	disk_list[],
	dev_t	disk_dev)
{
	int 	i;

	for ( i = 0; i < unique_disks; i++ ) {
		if ( disk_list[i] == disk_dev ) {
			return( unique_disks );
		} 
	}
	disk_list[i] = disk_dev;
	return( i + 1 );
}
