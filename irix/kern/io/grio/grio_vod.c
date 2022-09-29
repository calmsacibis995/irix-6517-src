#ident "$Header: /proj/irix6.5.7m/isms/irix/kern/io/grio/RCS/grio_vod.c,v 1.13 1995/10/24 07:44:54 ack Exp $"

#ifdef SIM
#define _KERNEL
#include <stdio.h>
#endif
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/major.h>
#include <sys/mkdev.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_inum.h>
#include <sys/time.h>
#include <sys/grio.h>
#include <sys/bpqueue.h>
#include <sys/xlate.h>
#include <sys/kmem.h>
#include <sys/syssgi.h>
#include <sys/scsi.h>
#ifdef SIM
#undef _KERNEL
#endif


/*
 * Global variables.
 */
extern int			grio_dbg_level;
				/* ggd daemon sleeps on sema waiting for work */
extern lock_t			grio_stream_table_lock;


/*
 * grio_vod_count_streams_in_slot
 *	Count the number of streams accessing this disk.
 * 	(do not count self )
 *	If the reservation is a ROTOR type match the given slot, 
 *	otherwise always count the reservation.
 * 
 * 
 * RETURNS:
 *	The number of streams matching the given slot.
 *
 */
grio_vod_count_streams_in_slot( 
	grio_stream_disk_info_t *griosdp, 
	int current_slot)
{
	int			count = 0;
	grio_disk_info_t	*griodp;
	grio_stream_disk_info_t	*thisgriosdp;

	griodp		= griosdp->griodp;
	thisgriosdp	= griodp->diskstreams;

	while ( thisgriosdp ) {
		if ( griosdp != thisgriosdp ) {
			if ( ROTOR_STREAM( thisgriosdp->thisstream) && 
			     (current_slot == thisgriosdp->thisstream->rotate_slot )) {
				count++;
				ASSERT( griosdp->num_iops == 1 );
			} else if ( !ROTOR_STREAM( thisgriosdp->thisstream) ) {
				count += griosdp->num_iops;
			}
		}
		thisgriosdp = thisgriosdp->nextstream;
	}
	return( count );
}

/*
 * grio_vod_slot_has_room 
 * 	Determine if the ROTOR bucket/slot for the given disk has
 * 	room for another stream.
 * 
 *  RETURNS:
 *	true it the ROTOR bucket/slot has room for another stream 
 * 	false if the ROTOR bucket/slot is full.
 */
int
grio_vod_slot_has_room( grio_stream_disk_info_t *griosdp, int slot ) 
{
	grio_stream_info_t	*griosp;

	griosp 			= griosdp->thisstream;
	if ( grio_vod_count_streams_in_slot( griosdp, slot) < 
		griosp->max_count_per_slot ) {
		return( 1 );
	} 
	return( 0 );
}

/*
 * grio_vod_get_least_used_slot
 *	Determine the ROTOR bucket/slot for the given disk that has
 *	the leaset number of streams assigned to it.
 *
 *
 * RETURNS:
 *	the number of the ROTOR bucket/slot that has the least number
 *	of streams assigned to it.
 */
int
grio_vod_get_least_used_slot( grio_stream_disk_info_t *griosdp )
{
	int			i, total_slots, count, s;
	int			least_used_count, least_used_slot;
	grio_stream_info_t	*griosp;

	griosp 			= griosdp->thisstream;
	total_slots 		= griosp->total_slots;
	least_used_count	= griosp->max_count_per_slot;
	least_used_slot		= -1;
	
	
	s = GRIO_STABLE_LOCK();
	for ( i = 0; i < total_slots; i++) {
		count = grio_vod_count_streams_in_slot( griosdp, i);
		ASSERT( count <= griosp->max_count_per_slot );
		if (count < least_used_count) {
			least_used_slot = i;
			least_used_count = count;
		}
	}
	GRIO_STABLE_UNLOCK( s );
	ASSERT( least_used_slot != -1 );
	ASSERT( least_used_slot < total_slots );

	return( least_used_slot );
}

/*
 * grio_vod_get_period_end 
 * 	Calculate the end of the current period
 *
 *
 * RETURNS:
 *	time at end of current period
 */
time_t
grio_vod_get_period_end( grio_stream_disk_info_t *griosdp, time_t now ) 
{
	int			total_slots, stream_slot;
	int			current_slot, current_position;
	int			desired_position, position_difference;
	time_t			result, iops_time, current_time;
	grio_disk_info_t	*griodp;
	grio_stream_info_t	*griosp;

	griodp			= griosdp->griodp;
	griosp 			= griosdp->thisstream;
	iops_time		= griosdp->iops_time;

	/*
	 * always look forward to the next time slot
*XXX look at current slot
	 */
	current_time		= ((now + iops_time)/iops_time)*iops_time;
	total_slots 		= griosp->total_slots;
	stream_slot		= griosp->rotate_slot;
	current_slot 		= current_time % total_slots;
	desired_position 	= griodp->rotate_position;
	current_position	= (current_slot + stream_slot) % total_slots;

	position_difference	= current_position - desired_position;
	if (position_difference < 0 )
		position_difference = (-1)*position_difference;

	ASSERT( position_difference >= 0 );
	ASSERT( position_difference < total_slots );
	
	/*
	 * calculate the time until it is necessary 
	 * for this I/O to complete
 	 */

	result = current_time + ((position_difference + 1)*iops_time);

	return( result );
}

/*
 * grio_init_vod_slot()
 *	Assign a slot to the ROTOR type stream and initialize the contents
 * 	of the stream disk info structure.
 *	
 *
 * RETURNS:
 *	always returns 0
 */
int
grio_init_vod_slot( 
	grio_stream_disk_info_t	*griosdp, 
	time_t 			now, 
	time_t 			quantum_start)
{
	grio_stream_info_t	*griosp;
	
	griosp = griosdp->thisstream;

	griosp->rotate_slot = grio_vod_get_least_used_slot(griosdp);
	griosdp->last_op_time = now; 
	griosp->last_stream_op = -1;
	griosdp->period_end   = quantum_start;
	griosdp->iops_remaining_this_period = 0;

	return( 0 );
}



/*
 *  grio_find_closest_slot()
 *	Find the slot closest to current_slot which is not full.
 *	Set the rotate_slot field of the stream associated with this
 *	stream disk to the nearest nonfull slot.
 *
 * RETURNS:
 *	the number of the slot chosen
 */
grio_find_closest_slot( 
	grio_stream_disk_info_t *griosdp, 
	int 			current_slot)
{
	int 			desired_slot, last_slot, slot;
	int			s, count;
	grio_stream_info_t	*griosp;

	griosp 			= griosdp->thisstream;


	desired_slot = (griosp->total_slots + 
			griosdp->griodp->rotate_position  - current_slot) %
			griosp->total_slots;

	last_slot = (desired_slot + 1 ) % griosp->total_slots;

	slot = desired_slot;

	s = GRIO_STABLE_LOCK();
	while ( slot != last_slot ) {
		count = grio_vod_count_streams_in_slot( griosdp, slot );
		ASSERT( count <= griosp->max_count_per_slot );

		/*
		 * do not allow slipping if slot is almost full
		 */
		if ( count <  griosp->max_count_per_slot  ) {
			griosp->rotate_slot = slot;
			GRIO_STABLE_UNLOCK( s );
			return( slot );
		} 

		/*
		 * current slot is best slot
		 */
		if ( griosp->rotate_slot == slot ) {
			GRIO_STABLE_UNLOCK( s );
			return( slot );
		}

		slot = (slot - 1 + griosp->total_slots ) % griosp->total_slots;
	}
	/*
	 * there must be room in the last slot.
	 */
	ASSERT( slot == last_slot );
	count = grio_vod_count_streams_in_slot( griosdp, slot );
	ASSERT( count < griosp->max_count_per_slot );

	griosp->rotate_slot = slot;
	GRIO_STABLE_UNLOCK( s );

	return( slot );
}

/* 
 * grio_determine_vod_priority() 
 * 	Determine the priority of the given stream when accessing the
 * 	given disk. Set the iops_remaining_this_period field of the
 * 	grio_stream_disk_info_t struct if this stream has priority.
 * 
 * RETURNS: 
 * 	always returns 0
 */
int
grio_determine_vod_priority( grio_stream_disk_info_t *griosdp, time_t now )
{
	int			current_slot, current_position;
	int			start_priority_io;
	time_t			quantum_start, iops_time;
	grio_stream_info_t	*griosp;

	griosp			= griosdp->thisstream;
	start_priority_io 	= 0;
	iops_time       	= griosdp->iops_time;
	current_slot    	= (now/iops_time) % griosp->total_slots;
	quantum_start   	= (now/iops_time) * iops_time;

	/*
	 * determine scheduling of deadline streams
	 *
	 *
	 * this stream has not yet been assigned a slot
	 */
	if ( ( griosp->rotate_slot == -1 ) && (griosdp->bp_list) )  { 
		grio_find_closest_slot( griosdp, current_slot);
	} else if ( griosp->rotate_slot == -1 ) {
		return( 0 );
	}

	current_position = (current_slot + griosp->rotate_slot) %
       				griosp->total_slots;

	/*
	 * if the time quantum has expired,
	 * set period_end to the end of the next time quantum
	 *
	 * if this stream should have priority when accessing
	 * this disk during this time quantum, set
	 * iops_remaining_this_period to the  number of I/Os
	 * the stream is guaranteed to get during this period
	 */
	if ( griosdp->period_end <= quantum_start ) {

		ASSERT( (griosdp->period_end % iops_time ) == 0 );

		/*
		 * stream is accessing correct disk
		 */
		if ( griosdp->griodp->rotate_position == current_position ) {
			start_priority_io = 1;
		} else {
			/*
			 * stream is not accessing correct disk
			 * and this stream has not accessed this disk
			 * during last time quantum
			 *
 			 * if this is a slipping ROTOR stream,
			 * try to find a bucket that will allow
			 * this stream to access this disk with
			 * priority
			 *
			 * do not allow slipping until there is actually
			 * i/o pending from this stream on this disk.
			 *
			 */
			if  (   ( griosdp->bp_list ) 				&&
				( griosp->last_stream_op != -1 )		&&
				( griosp->last_stream_op < (quantum_start - iops_time)) &&
				SLIP_ROTOR_STREAM( griosp )  			&&
				(!SLIPPED_ONCE_STREAM( griosp )) ) {

				grio_find_closest_slot( griosdp, current_slot);
	
				current_position =
					(current_slot + griosp->rotate_slot) % 
					griosp->total_slots;

				MARK_SLIPPED_ONCE_STREAM( griosp );

				if ( griosdp->griodp->rotate_position == 
					current_position ) {
					start_priority_io = 1;
				}
			} 
		}

		/*
		 * clear any leftover iops 
		 * if no I/O is pending
		 */
		if ( griosdp->bp_list == NULL ) {
			griosdp->iops_remaining_this_period = 0;
		}

		/*
		 * adjust period_end
		 */
		griosdp->period_end = quantum_start + griosdp->iops_time;

#ifdef GRIO_DEBUG
		if ( griosdp->iops_remaining_this_period && 
		     (griosdp->bp_list != NULL ) ) {
			if ( griosdp->realbp->b_start > 
				(quantum_start - iops_time) ) {
				printf("BP issued after quantum_started: "
					"%d, %d, %d \n", 
					griosdp->realbp->b_start,
					quantum_start, 
					now - griosdp->realbp->b_start);
			}  else {
				printf("BP issued before quantum_started "
					": %d, %d, %d, %d  \n", 
					griosdp->realbp->b_start, 
					now, quantum_start, 
					griosp->last_stream_op);
			}

			if ( start_priority_io ) {
				printf("VOD_PRIORITY_IO - SCHEDULE FAILURE\n");
			}

		}
#endif
	}


        if ( start_priority_io ) {

		if ( 	griosdp->iops_remaining_this_period == 0  ) {
       	       		griosdp->time_priority_start  =  quantum_start;
		}

		CLEAR_SLIPPED_ONCE_STREAM( griosp );
		
		if ( griosdp->bp_list == NULL ) {
                	griosdp->iops_remaining_this_period = griosdp->num_iops;
		} else {
                	griosdp->iops_remaining_this_period += griosdp->num_iops;
		}
        }

	return( 0 );
}
