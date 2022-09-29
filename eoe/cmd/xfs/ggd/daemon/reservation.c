#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/reservation.c,v 1.21 1997/03/29 00:09:06 kanoj Exp $"
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
#include <sys/invent.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/lock.h>
#include <sys/sysmp.h>
#include <sys/ktime.h>
#include <sys/capability.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"


/*
 * External function declarations.
 */
extern void ggd_fatal_error( char *);
extern void add_reservation( device_reserv_info_t *, reservations_t *);
extern void remove_reservation( device_reserv_info_t *, reservations_t *);
extern void deallocate_stream( void );
extern reservations_t *get_resv_from_id(reservations_t *,dev_t,stream_id_t *);

/*
 * Local function declarations.
 */
#if 0
STATIC void print_resvp(reservations_t *);
#endif
STATIC int end_guaranteed_stream(reservations_t *);
STATIC int start_guaranteed_stream( reservations_t *);

/*
 * Currently, there is one global list of all guaranteed rate reservations.
 * This should be broken down onto a per file system basis.
 */
file_reservation_t	file_reservation_list;

/*
 * reservation.c
 *
 *	This file contains the routines and data structure that 
 *	maintain the list of rate guaranteed reservations
 *
 * RETURNS:
 *	0 on sucess
 *	no-zero on failure
 */
int
add_new_reservation(
	grio_cmd_t		*griocp,
	dev_t			fs_dev,
	int			total_slots,
	int			max_count_per_slot)
{
	grio_resv_t		*griorp;
	reservations_t		*resvp;
	glob_resv_info_t	*resvpgp;
	device_reserv_info_t	*dresvp;


	griorp = GRIO_GET_RESV_DATA( griocp );
	dresvp = &(FILE_RSV_INFO());

	/*
	 * check if a reservation with this uuid already exits !!!!
	 */
	if ( get_resv_from_id( dresvp->resvs, 0, &(griorp->gr_stream_id)) ) {
		dbgprintf(1,("duplicate stream id\n"));
		return( -1 );
	}

	if ((resvp = malloc( sizeof (struct reservations )))) {
		bzero( resvp, sizeof( struct reservations) );
		resvpgp				= GLOB_RESV_INFO( resvp );

		resvp->start_time		= griorp->gr_start;
		resvp->end_time			= griorp->gr_start+griorp->gr_duration;
		resvp->sort_time		= griorp->gr_start;
		resvp->flags			= griorp->gr_flags;
		resvp->total_slots		= total_slots;
		resvp->max_count_per_slot	= max_count_per_slot;
		COPY_STREAM_ID( griorp->gr_stream_id, resvp->stream_id );
		resvp->nextresv 		= NULL;

		resvpgp->file_type		= griocp->one_end.gr_end_type;
		resvpgp->fs_dev			= fs_dev;
		resvpgp->procid			= griocp->gr_procid;
		resvpgp->inum			= griocp->one_end.gr_ino;
		resvpgp->fp			= griocp->gr_fp;

		add_reservation( dresvp, resvp);

	} else {
		ggd_fatal_error("malloc failure");
	}

	return( 0 );
}

/*
 * expire_reservation()
 *	This routine finds the reservation associated with the given
 *	file reservation associated with the given stream_id.
 *	The next time update_file_reservations() is run the reservation
 *	will be removed from the list.
 *
 * RETURNS:
 *	0 if the reservation was expired
 *	non-zero if reservation was not expired, or was missing
 */
/* ARGSUSED */
int
expire_reservation( pid_t procid, uuid_t *gr_stream_id)
{
	int			status = 0;
	reservations_t 		*resvp;

	/*
	 * Find the reservation structure for this id.
 	 */
	if ( resvp = get_resv_from_id(FILE_RSV_LIST(), 0, gr_stream_id) )  {
		/*
		 * Mark the start and end times as 0 so it will
		 * appear as if the reservation period is over.
		 */
		resvp->start_time 	= 0;
		resvp->end_time 	= 0;

	} else {
		dbgprintf(1,("could not find file resv to expire \n") );
		status = EIO;
	}

	return( status );
}

/*
 * remove_expired_file_reservations()
 *	This routine scans the list of file reservations and determines
 *	if any reservations are now complete (i.e. start + duration < now).
 *	These reservations are removed from the the global list.
 *
 * RETURNS:
 *	1 if a file reservation expired
 *	0 if no file reservations expired
 */
STATIC int
remove_expired_file_reservations( time_t now )
{
	int		ret = 0;
	reservations_t 	*resvp, *nextresvp;
	
	resvp = FILE_RSV_LIST();
	while ( resvp ) {
		nextresvp = resvp->nextresv;
		if ( resvp->end_time < now ) {

			if ( RESV_STARTED( resvp ) ) {
				end_guaranteed_stream( resvp );
			} 

			remove_reservation( &FILE_RSV_INFO(), resvp);

			deallocate_stream();

			ret = 1;
		}
		resvp = nextresvp;
	}
	return (ret);
}

/*
 * start_new_reservations
 *
 *
 * RETURNS:
 *	1 if a new reservation became active
 *	0 if not
 */
int
start_new_reservations( time_t now )
{

	int		ret = 0;
	reservations_t	*resvp;

	resvp = FILE_RSV_LIST();
	while (resvp && (resvp->start_time <= now)) {
		if (!RESV_STARTED( resvp )) {
			/*
 		 	 * Start the new request.
		 	 */
			RESV_MARK_STARTED( resvp );
			start_guaranteed_stream( resvp );

			ret = 1;
		}
		resvp  = resvp->nextresv;
	}
	return (ret);
}

/*
 * start_new_device_reservations - do component specific rate
 * starting.
 *
 * RETURNS:
 *	1 if a new reservation became active
 *	0 if not
 */
int
start_new_device_reservations(device_node_t *devnp, time_t now)
{

	int			ret = 0, type;
	device_reserv_info_t    *dresvp = RESV_INFO(devnp);
	reservations_t		*resvp;

	type = devnp->flags;
	switch (type) {
		case INV_SCSIDRIVE:
		case INV_PCI_BRIDGE:
		case INV_XBOW:
			break;
		default:
			return(ret);
	}

	resvp = dresvp->resvs;
	while (resvp && (resvp->start_time <= now)) {
		if ((IS_START_RESV(resvp)) && (!RESV_STARTED(resvp))) {

			/*
 		 	 * Start the new request.
		 	 */
			RESV_MARK_STARTED(resvp);
			switch (type) {
				case INV_SCSIDRIVE:
					start_guaranteed_disk_stream(resvp);
					break;
				case INV_PCI_BRIDGE:
				case INV_XBOW:
					access_guaranteed_registers(devnp, 
						resvp);
					break;
			}
			ret = 1;
		} else if ((IS_END_RESV(resvp)) && (!RESV_STARTED(resvp)))
			RESV_MARK_STARTED(resvp);
		resvp  = resvp->nextresv;
	}
	return (ret);
}


/*
 * start_guaranteed_stream
 *      Issue the system calls which will enable a process using
 *      the given stream_id to receive guaranteed rate I/O.
 *
 * RETURNS:
 *      0 on success
 *      non-zero on failure
 */
STATIC int
start_guaranteed_stream( reservations_t *resvp)
{
	int             	status;
	grio_file_id_t		fileid;
	glob_resv_info_t 	*resvpgp = GLOB_RESV_INFO( resvp );
	int			vod_info;
	cap_t			ocap;
	cap_value_t		cap_device_mgt = CAP_DEVICE_MGT;

	if (resvp->flags & SYSTEM_GUAR) return(0);
	
	dbgprintf(5, ("starting guaranteed stream \n"));

	/*
	 * call GRIO_ADD_STREAM_INFO
	 * this call tells the os about a given stream_id and the
	 * file system it is associated with.
	 */
	fileid.ino	= resvpgp->inum;
	fileid.fs_dev	= resvpgp->fs_dev;
	fileid.procid	= resvpgp->procid;
	fileid.fp	= resvpgp->fp;
	vod_info 	= resvp->total_slots | (resvp->max_count_per_slot << 16);
	ocap = cap_acquire(1, &cap_device_mgt);
	status = syssgi( SGI_GRIO, GRIO_ADD_STREAM_INFO,
			resvp->flags, &fileid, &(resvp->stream_id), vod_info);
	cap_surrender(ocap);

	if ( status ) {
		dbgprintf(1, ("ADD_STREAM failed %d\n",status));
		return( status );
	}
	
	/*
	 * the update tree code will issue the GRIO_ADD_DISK_STREAM_INFO
	 */
	return( 0 );
}


/*
 * end_guaranteed_stream
 *      Issue the system calls which will remove the guaranteed rate
 *      stream_id from the system.
 *
 * RETURNS:
 *      0 on success
 *       -1 on failure
 */
STATIC int
end_guaranteed_stream(reservations_t *resvp)
{
	int             status;
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;

	ocap = cap_acquire(1, &cap_device_mgt);
	status = syssgi(SGI_GRIO,GRIO_REMOVE_STREAM_INFO,&(resvp->stream_id));
	cap_surrender(ocap);

	if ( status ) {
		dbgprintf(1, ("REMOVE_STREAM failed %d\n",status));
	}

	return( status );
}



/*
 * next_file_start_time
 *
 *
 */
time_t
next_file_start_time( time_t now )
{
	time_t          start_time = 0;
	reservations_t  *resvp;

	resvp = FILE_RSV_LIST();
	while (resvp && (resvp->start_time <= now)) {
		resvp  = resvp->nextresv;
	}
	if (resvp) {
		start_time = resvp->start_time;
	}
	return( start_time );
}

/*
 * next_file_expire_time
 *
 *
 */
time_t
next_file_expire_time( time_t now )
{
	time_t          expire_time = 0;
	reservations_t  *resvp;

	resvp = FILE_RSV_LIST();
	while (resvp && (resvp->start_time < now)) {
		if ( !expire_time ) {
			expire_time = resvp->end_time;
		} else if ( expire_time > resvp->end_time ) {
			expire_time = resvp->end_time;
		}
		resvp = resvp->nextresv;
	}
	return( expire_time );
}

/*
 * get_time_until_next_file_operation
 *
 *
 *
 * RETURNS:
 *	always returns 0
 *
 */
int
get_time_until_next_file_operation( grio_cmd_t *griocp, time_t now )
{
	time_t 	expire_time, start_time, wait_time;

	expire_time = next_file_expire_time(now);
	start_time = next_file_start_time(now);
	if ( (start_time == 0) && (expire_time == 0) ) {
		/*
		 * Wait for 60 seconds.
		 */
		wait_time =  60;
	} else if ( (start_time > now) && (start_time < expire_time) ) {
		wait_time = start_time - now;
	} else {
		if (expire_time >= now ) {
			wait_time = expire_time - now;
		} else {
			printf("expire_time = %d, now = %d \n",
				expire_time, now);
			wait_time = 60;
		}
	}

	if ( wait_time < 0 ) {
		wait_time = 10;
	}

	dbgprintf(10, ("get_next_time: wait_time = %d, start_time = %d, expire_time = %d, now = %d\n", wait_time, start_time, expire_time, now) );

	griocp->gr_time_to_wait = wait_time;
	return( 0 );
}

/*
 * update_file_reservations()
 *	This routine expires old reservations and starts new ones.
 *
 * RETURNS:
 *	1 if there were changes to the active reservation list.
 *	0 if there were no reservation changes.
 */
int
update_file_reservations( time_t now)
{
	int ret1, ret2;
	
	ret1 = remove_expired_file_reservations( now );
	ret2 = start_new_reservations( now );
	return (ret1 || ret2);
}

#if 0
/*
 * Debug routines. 
 */
STATIC void
print_resvp( reservations_t *resvp)
{
	printf("resvp = %x \n",resvp);
}
#endif
