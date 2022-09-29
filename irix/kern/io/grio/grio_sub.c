#ident "$Header: /proj/irix6.5.7m/isms/irix/kern/io/grio/RCS/grio_sub.c,v 1.52 1998/07/20 22:55:39 kanoj Exp $"

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
#include <ksys/vfile.h>
#include <ksys/vproc.h>
#include <ksys/fdt.h>
#include <sys/bpqueue.h>
#include <sys/xlate.h>
#include <sys/kmem.h>
#include <sys/syssgi.h>
#include <sys/scsi.h>
#include <sys/iograph.h>
#include <sys/invent.h>
#ifdef SIM
#undef _KERNEL
#endif

/*
 * Global variables.
 */
extern int			grio_dbg_level;
				/* ggd daemon sleeps on sema waiting for work */
extern zone_t			*grio_buf_data_zone;
				/* zone allocator structure 		      */
extern sema_t           	grio_q_sema;		
				/* lock for grio_stream_table 		      */
extern lock_t			grio_stream_table_lock; 
				/* global array of per stream grio structures */
extern grio_stream_info_t	*grio_stream_table[GRIO_STREAM_TABLE_SIZE];
extern	grio_idbg_disk_info_t	*griodp_idbg_head, *griodp_idbg_tail;
extern int			grio_determine_vod_priority(
				grio_stream_disk_info_t *, time_t );
extern int			grio_start_next_io( grio_disk_info_t *,
					time_t, int );

/*
 * Local variables.
 */
stream_id_t	non_guaranteed_id;/* uuid for nonguaranteed stream 	      */

/*
 * Forward routine declarations.
 */
int 			grio_get_stream_to_remove( stream_id_t *);
int			grio_add_disk_info( dev_t, int, int, int, int, int);
void 			grio_bp_queue(buf_t *);
void 			grio_wait_for_stream_to_drain( grio_stream_info_t *);
void 			grio_timeout( grio_disk_info_t *);
time_t 			grio_determine_wait_until_priority( 
				grio_stream_disk_info_t *);
time_t 			grio_determine_remaining_priority_time( 
				grio_stream_disk_info_t *);
grio_disk_info_t	*grio_get_disk_info( dev_t );
grio_stream_info_t	*grio_find_stream_with_id( stream_id_t *); 
grio_stream_disk_info_t	*grio_get_stream_disk_info(buf_t *);
STATIC int 		grio_add_stream_disk( dev_t , int, time_t, int, stream_id_t *);
STATIC int		grio_associate_file_with_stream(
				pid_t,dev_t,gr_ino_t,vfile_t *,stream_id_t *);
STATIC int		grio_dissociate_stream(
				pid_t,stream_id_t *);
STATIC int 		grio_add_stream_info(
				int, grio_file_id_t *, stream_id_t *, int);
STATIC int 		grio_remove_stream_info(stream_id_t *);
STATIC int 		grio_remove_stream_disk_info( grio_stream_info_t *);
STATIC dev_t		grio_disk_id(dev_t);
STATIC vertex_hdl_t 	getnext(vertex_hdl_t, vertex_hdl_t, int, int, int);
STATIC vertex_hdl_t 	dev_ioctl_hdl(vertex_hdl_t, int, int);
STATIC int 		dev_class_type(vertex_hdl_t, int *, int *, int *);
STATIC void		grio_set_fpriority_flag(vfile_t *);
STATIC void		grio_reset_fpriority_flag(vfile_t *);
STATIC void		grio_remove_stream_fp_info(grio_stream_info_t *);

/*
 * External routine declaraions.
 */
extern int	grio_issue_async_grio_req( char *);
extern dev_t 	xfs_grio_get_fs_dev( int );
extern gr_ino_t	xfs_grio_get_inumber( int );
extern time_t 	grio_vod_get_period_end( grio_stream_disk_info_t *,time_t);
extern int	grio_vod_get_least_used_slot(grio_stream_disk_info_t *);

/*
 * grio_get_disk_info
 *	Find the grio_disk_info_t structure associated with the
 *	given dev_t.
 *
 * RETURNS:
 *	a pointer to a grio_disk_info_t structure on success
 *	NULL on error
 */
grio_disk_info_t *
grio_get_disk_info(dev_t griodev)
{
	grio_disk_info_t	*griodp;
	
	griodp = grio_disk_info(griodev);
	return griodp;
}


/*
 * grio_get_stream_disk_info
 *	Find the grio_stream_disk_info_t structure associated with the
 *	given buf_t structure.
 *
 * RETURNS:
 *	a pointer to a grio_stream_info_t structure on success
 *	NULL on error
 */
grio_stream_disk_info_t *
grio_get_stream_disk_info( buf_t *bp)
{
	int			s;
	uint_t			status;
	grio_buf_data_t		*griobdp = NULL;
	grio_disk_info_t	*griodp  = NULL;
	grio_stream_disk_info_t	*griosdp = NULL;

	griobdp = (grio_buf_data_t *)(bp->b_grio_private);
	griodp = grio_get_disk_info( bp->b_edev );
	if ( griodp ) {
		/*
		 * lock disk info
		 */
		s = GRIO_DISK_LOCK( griodp );

		/*
		 * find the correct stream disk info
		 */
		griosdp = griodp->diskstreams;
		while ( (griosdp) &&
			( !EQUAL_STREAM_ID((griosdp->thisstream->stream_id), 
				(griobdp->grio_id))) ) {
			griosdp = griosdp->nextstream;
		}

		/*
		 * unlock disk info
		 */
		GRIO_DISK_UNLOCK( griodp, s );
	} else {
		dbg1printf(("Could not find grio disk info for disk 0x%x \n",
			bp->b_edev));
		ASSERT(0);
	}
	return( griosdp );
}

/*
 * grio_determine_stream_priority
 *	 Determine the priority of a given stream.
 * 
 * RETURNS:
 *	a number indicating the scheduling priority of the given
 *	grio stream. The higher the priority number, the more urgent it
 *	is that the I/O be issued.
 */
int
grio_determine_stream_priority( grio_stream_disk_info_t *griosdp, time_t now)
{
	int			priority = 0, multiplier;
	int			iops_time;
	grio_stream_info_t	*griosp;


	griosp		= griosdp->thisstream;
	iops_time	= griosdp->iops_time;

	if ( ROTOR_STREAM( griosp ) ) {

		grio_determine_vod_priority( griosdp, now );

	} else {
		ASSERT( NON_ROTOR_STREAM( griosp ) );


		/*
		 * do not start clock running on system guarantee
		 * until I/O is actually present in the system.
		 */
		if ( ( griosp->flags & SYSTEM_GUAR) &&
		     ( griosdp->bp_list == NULL) ) {
			griosdp->period_end = now + iops_time;
			griosdp->iops_remaining_this_period = 0;
		} 

		if ( griosdp->period_end <= now ) {

			/*
			 * check if the stream has
			 * issued an I/O within the last iops_time
			 */
			if ( (griosdp->period_end + iops_time ) > now ) {
				/*
			 	 * prevent the period_end from 
			 	 * slipping forward in time.
				 */
				if (griosdp->iops_remaining_this_period == 0) {
					griosdp->time_priority_start =
						griosdp->period_end;
				} else if ((griosdp->iops_remaining_this_period == griosdp->num_iops)
					&& ( griosdp->time_priority_start < 
					     (now - (time_t)(5*iops_time)))) {
					/*
					 * Prevent time_priority_start from slipping
					 * too far back in time. If this occurs, then the priority
					 * computed below could overflow a 32 bit integer.
					 * If the reservation holder has not used the reservation 
					 * in the last 5 time quantums, adjust the 
					 * time_priority_start.
					 */
					griosdp->time_priority_start = now;
				}

				griosdp->period_end += iops_time;
			} else {
				/*
				 * stream has not issued an I/O in
			 	 * the last time quantum
	 			 */
				griosdp->period_end = now + iops_time;

				if ((griosdp->iops_remaining_this_period == 0)||
					(griosp->flags & SYSTEM_GUAR) ) {
					griosdp->time_priority_start =  now;
				}
			}
			ASSERT( griosdp->period_end > now );

			/*
			 * check if the stream received all its IOPS 
			 * last time quantum. if not, add in next 
			 * quantums IOPS.
			 */
			if ((griosdp->bp_list != NULL) && 
				(!(griosp->flags & SYSTEM_GUAR)) ) {

				griosdp->iops_remaining_this_period += 
					griosdp->num_iops;
			} else {
				griosdp->iops_remaining_this_period = 
					griosdp->num_iops;
			}

		}
	}


	/*
	 * determine the priority of this stream
	 */
	if ( griosdp->iops_remaining_this_period ) {

		/*
		 * this stream has priority
	 	 * multiply by 100000 to get a whole number 
	 	 */
		if ( now < (griosdp->time_priority_start + iops_time) ) {

			ASSERT( now < griosdp->period_end);

			/*
			 * regular priority request
			 */
			priority = (griosdp->iops_remaining_this_period * 
					1000000) / (griosdp->period_end - now);
			;
		} else {
			/*
			 * this case occurs when a stream was given
			 * priority in the last time quantum but was not
		 	 * able to get its I/O completed.
			 */
			priority = 1000000*griosdp->iops_remaining_this_period;
			multiplier = 
			(now - griosdp->time_priority_start)/
			 	 iops_time;

			ASSERT( multiplier >= 1 );
			priority = (priority*multiplier) + 
				((now - griosdp->time_priority_start)%iops_time);
		}
	} else {
		/*
		 * this stream does not have priority
		 */
		priority = 0;
	}

	return( priority );
}

/*
 * grio_get_priority_bp
 *
 *	Obtain the next I/O request to be issued on this device.
 *
 *	First, find all stream which have unused priority I/Os this time 
 *	quantum that have currently outstanding I/O requests. Select which 
 *	of these streams has the highest priority and issue an I/O.
 *
 *	Next, if all streams have received all their priority I/Os for this
 *	time quantum, AND there is enough time until the end of all the
 *	streams time quantums, then allow one stream to issue an 
 *	overbandwidth I/O request.
 *
 *	Finally, if there are streams that have I/O requests pending BUT
 *	they are outside the scope of the guarantee AND they may impact
 *	other streams that have not received all their guaranteed bandwidth,
 *	do not issue an I/O request. Set a time out and re-evaluate the
 *	streams and the beginning of the next time quantum.
 *
 * NB:
 *	This can be further refined. If no stream which has priority 
 * 	currently has a request outstanding, and some stream wants an 
 *	overbandwidth I/O, determine if the request can be satisfied without
 *	jeopardizing the streams with remaining priority bandwidth.
 *	Currently the assumption is that it will always jeopardize the
 *	guarantees.
 *
 * WARNING:
 *	This routine assumes that GRIO_DISK_LOCK() is held when it is called.
 *
 *
 * RETURNS:
 *	a pointer to a buf_t structure on success	
 *	NULL on failure
 */
buf_t *
grio_get_priority_bp(grio_disk_info_t *griodp, time_t now)
{
	int 			highest_priority, priority;
	int			time_to_complete_ioop;
	int			longest_since_extra;
	int			some_stream_has_priority;
	int			io_pending, num_priority_iops_remaining;
	buf_t			*bp;
	time_t			delaytime, shortest_delaytime = HZ;
	time_t			minimum_priority_time_remaining = HZ;
	time_t			priority_time_remaining, last_op;
	grio_stream_info_t	*griosp;
	grio_stream_disk_info_t	*griosdp, *highestgriosdp, *bestgriosdp;

	/*
	 * initialize variables
	 */
	highestgriosdp = bestgriosdp = NULL;
	highest_priority = io_pending = 0;
	longest_since_extra = -1;
	some_stream_has_priority = 0;
	num_priority_iops_remaining = 0;

	/*
	 * scan the stream_list to determine which I/O 	
	 * is to be issued next
	 */
	griosdp = griodp->diskstreams;

	while  (griosdp) {

		griosp = griosdp->thisstream;

		/*
		 * determine the priority of this stream
		 */
		priority = grio_determine_stream_priority( griosdp, now );
		
		/*
	 	 * a non-zero priority means this stream has a 
		 * reservation for this time quantum.
	 	 */
		if ( priority ) {
			some_stream_has_priority = 1;
		} else {
			/*
			 * if this stream does not currently have ,
			 * priority, determine time until it does have 
			 * priority
	 		 */
			delaytime = 
			   grio_determine_wait_until_priority(griosdp);

			/*
			 * keep track of shortest time until some 
			 * stream that does not currently have 
		 	 * priority gets priority
			 */
			if (delaytime < shortest_delaytime )
				shortest_delaytime = delaytime;


		}

		/*
		 * this stream has no current pending I/O then drop
		 * priority - it cannot currently be used.
		 */
		if ( griosdp->bp_list == NULL ) {

			/*
			 * if this stream has priority but no pending I/O,
			 * and a priority stream has not yet been found,
			 * determine if it is possible to issue an overband
			 * i/o on some other stream
			 */
			if ( priority && (highest_priority == 0) ) {
				priority_time_remaining = 
					grio_determine_remaining_priority_time( griosdp );

				if ( minimum_priority_time_remaining > priority_time_remaining ) {
					minimum_priority_time_remaining = priority_time_remaining;
				}

				num_priority_iops_remaining += 
					griosdp->iops_remaining_this_period;
			}

			priority = 0;
		} else {
			io_pending = 1;
		}

		/*
		 * keep track of the stream with the highest priority
		 * that currently has I/O outstanding.
		 */
		if (priority > highest_priority)  {
			highest_priority = priority;
			highestgriosdp = griosdp;
		}


		/*
		 * if there is not stream with high priority, find the
		 * next best thing.
		 */
		if ( highest_priority == 0 ) {
			/*
		  	 * If this stream is allowed to have overband requests 
		 	 * (this means DEADLINE instead of REALTIME scheduling),
		 	 * and there are I/O requests currently pending from 
		 	 * this stream, determine if this stream is more 
		 	 * deserving of the over-bandwidth I/O request then the
			 * other streams
		 	 */
			last_op = griosp->last_stream_op;

			if ( (!RT_SCHED_STREAM(griosp)) && 
			     (last_op != -1) 		&&
		    	     (griosdp->bp_list)		&& 
		     	     ((now - last_op) > longest_since_extra)) { 

				longest_since_extra = now - last_op;		
				bestgriosdp = griosdp;
			}
		}
		griosdp = griosdp->nextstream;
	}


	/*
	 * found a stream with priority
	 */
	if ( highest_priority ) {

		bp = highestgriosdp->bp_list;
		/* Remove this bp from bp_list and ...*/
		highestgriosdp->bp_list = bp->b_grio_list;

		/* ... put it on the issued_bp_list */
		bp->b_grio_list = highestgriosdp->issued_bp_list;
		highestgriosdp->issued_bp_list = bp;

		highestgriosdp->iops_remaining_this_period--;
		if ( highestgriosdp->iops_remaining_this_period == 0 ) {
			highestgriosdp->time_priority_start = 0;
		}
		highestgriosdp->last_op_time = now;
		highestgriosdp->thisstream->last_stream_op = now;

		return bp;
	}

	/*
	 * no stream with reserved bandwidth has any I/O pending,
	 * and no stream has any unused reservation for current
	 * time quantum, so an over-bandwidth I/O can be issued.
	 */
	if ( bestgriosdp  && (some_stream_has_priority == 0) ) {
	
		/*
 		 * is there enough time to issue this I/O before another 
		 * stream has priority ?
 		 */
		time_to_complete_ioop = HZ/griodp->num_iops_max;

		if ( time_to_complete_ioop < shortest_delaytime ) {
			bp = bestgriosdp->bp_list;
			/* Remove this bp from bp_list and ... */
			bestgriosdp->bp_list = bp->b_grio_list;

			/* ... put it on the issued_bp_list */
			bp->b_grio_list = bestgriosdp->issued_bp_list;
			bestgriosdp->issued_bp_list = bp;

			bestgriosdp->last_op_time = now;
			bestgriosdp->thisstream->last_stream_op = now;
			ASSERT( bestgriosdp->iops_remaining_this_period == 0 );
			bestgriosdp->time_priority_start = 0;
			return bp;
		}
	} 

	/*
 	 * there is I/O to issue, but some other stream has
 	 * an unused reservation. 
	 * check if the i/o can safely be issued without jepordizing
	 * the rate guarantees.
	 * if not, schedule a timeout to run driver later
	 * when this stream has priority
	 */
	if ( io_pending ) {

		time_to_complete_ioop = HZ/griodp->num_iops_max;

		if ( (bestgriosdp) && 
		     (minimum_priority_time_remaining > 
		      (time_to_complete_ioop * ( num_priority_iops_remaining + 1 ))) ) {

			/*
			 * it is safe to issue this i/o.
			 */
			bp = bestgriosdp->bp_list;
			/* Remove this bp from bp_list and ... */
			bestgriosdp->bp_list = bp->b_grio_list;

			/* ... put it on the issued_bp_list */
			bp->b_grio_list = bestgriosdp->issued_bp_list;
			bestgriosdp->issued_bp_list = bp;

			bestgriosdp->last_op_time = now;
			bestgriosdp->thisstream->last_stream_op = now;
			ASSERT( bestgriosdp->iops_remaining_this_period == 0 );
			bestgriosdp->time_priority_start = 0;
			return bp;
		} 

		/*
		 * if there is already a timeout scheduled for this 
		 * drive, and the current shortest_delaytime is shorter
		 * than the outstanding timeout, remove the timeout
	 	 * and start the shorter one.
	 	 */
		if ( griodp->timeout_id == 0 ) {

			/*
			 * no timeout pending, schedule this one
			 */
			ASSERT( griodp->timeout_time == 0 );
			griodp->timeout_time = lbolt + shortest_delaytime;
			griodp->timeout_id = 
			     timeout( grio_timeout, griodp, shortest_delaytime);
			ASSERT( griodp->timeout_id );
		} else if ( (lbolt + shortest_delaytime ) < 
					griodp->timeout_time ) {
	
			/*		
			 * remove current timeout and schedule this one 
			 */
			ASSERT( griodp->timeout_id );
			untimeout( griodp->timeout_id );
			griodp->timeout_time = lbolt + shortest_delaytime;
			griodp->timeout_id = 
			     timeout( grio_timeout, griodp, shortest_delaytime);
			ASSERT( griodp->timeout_id );
		} else {
			/*
			 * time to completion of pending timeout is
	 		 * shorter than the new one
		 	 */
			ASSERT( griodp->timeout_id );
		}
	}


	/*
	 * no request to start at this time
	 */
	return NULL;
}


/*
 * grio_add_disk_wrapper()
 *
 *
 *
 *
 * RETURNS:
 *	0 on success
 *	non zero on error
 */
grio_add_disk_wrapper( sysarg_t sysargptr) 
{
	grio_add_disk_info_struct_t	info;

	if ( copyin((caddr_t)sysargptr, &info, 
			sizeof(grio_add_disk_info_struct_t)) ) {
		return( EFAULT );
	}

	return( grio_add_disk_info(info.gdev,
			info.num_iops_rsv,
			info.num_iops_max,
			info.opt_io_size,
			info.rotation_slot,
			info.realtime_disk) );
}


/* 
 * grio_disk_info_t()
 *  	Convert the device number into a pointer where disk info 
 *	was stored. The input can be the device number of the
 *	partition or the device number of the nonleaf disk node.
 *
 * RETURNS: 
 *	pointer to the disk info on success 
 *	NULL if info wasn't inserted for this device
 * NOTE:
 *	hwgfs ops can sleep, and they provide protection for
 *	associating information with vertices anyway. So, no
 *	need of doing a GRIO_GLOB_LOCK before calling this routine.
 */
grio_disk_info_t *
grio_disk_info(dev_t           gdev)
{
        arbitrary_info_t        ainfo;

	if (hwgraph_edge_get(gdev, EDGE_LBL_VOLUME, 0) != GRAPH_SUCCESS)
		gdev = grio_disk_id(gdev);
	if (gdev == GRAPH_VERTEX_NONE)
		return(NULL);

	/* get the info associated with our label */
        if (hwgraph_info_get_LBL(gdev, INFO_LBL_GRIO_DSK, &ainfo)
                                                != GRAPH_SUCCESS) {
		return(NULL);
	} else {
		return((grio_disk_info_t *) ainfo);
	}
}

/*
 * This takes the device number of a disk partition, or of the
 * non leaf disk node that is taken as an unique disk identifier 
 * for all partitions on the disk. It returns back the unique disk
 * device number. Note that this is the same method that ggd uses
 * in user space to get to the identifier.
 */
STATIC dev_t
grio_disk_id(dev_t  partdev)
{
	dev_t	diskdev = partdev;

        if (hwgraph_edge_get(diskdev, HWGRAPH_EDGELBL_DOTDOT,
                                &diskdev) != GRAPH_SUCCESS)
                return(GRAPH_VERTEX_NONE);
        if (hwgraph_edge_get(diskdev, HWGRAPH_EDGELBL_DOTDOT,
                                &diskdev) != GRAPH_SUCCESS)
                return(NULL);
        if (hwgraph_edge_get(diskdev, HWGRAPH_EDGELBL_DOTDOT,
                                &diskdev) != GRAPH_SUCCESS)
                return(NULL);
	return(diskdev);
}


/*
 * grio_add_disk_info()
 *	Add the guaranteed rate information for a given disk.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 * NOTE:
 *	No GRIO_GLOB_LOCK protection needed here. hwgfs ops
 *	do their own locking, and this code is invoked in the
 *	context of the single ggd process.
 */
int
grio_add_disk_info(
	dev_t		griodev,
	int		num_iops_rsv,
	int		num_iops_max,
	int		opt_io_size,
	int		rotation_slot,
	int		realtime_disk)
{
	int			s;
	grio_disk_info_t	*griodp;
	grio_idbg_disk_info_t	*new_grio_idbg_entry;

	dbg2printf(("add_disk_info: %x, %d, %d, %d %d %d\n",
		griodev, num_iops_rsv, num_iops_max, 
		opt_io_size, rotation_slot, realtime_disk));

	/*
	 * Error check the parameters.
	 */
	if (opt_io_size <= 0 ) {
		return( -1 );
	}

	griodp = grio_disk_info(griodev);

	if ( griodp ) {
		/*
		 * should we check for outstanding disk i/o ?  	TAP
		 */
		s = GRIO_DISK_LOCK( griodp );
		if ( num_iops_max )
			griodp->num_iops_max	= num_iops_max;
		if ( num_iops_rsv )
			griodp->num_iops_rsv	= num_iops_rsv;
		if ( opt_io_size )
			griodp->opt_io_size	= opt_io_size;

		griodp->rotate_position =  rotation_slot;
		griodp->realtime_disk = realtime_disk;

		GRIO_DISK_UNLOCK( griodp, s );
	} else {
		/*
		 * Allocate a grio_disk_info structure.
		 */
		griodp = (grio_disk_info_t *)kmem_zalloc(
			sizeof(grio_disk_info_t), KM_SLEEP);

		if ( griodp == (grio_disk_info_t *)NULL ) {
			cmn_err(CE_CONT,"grio_disk_info alloc failed ");
			return( -1 );
		} else {
			griodp->num_iops_max	= num_iops_max;
			griodp->num_iops_rsv	= num_iops_rsv;
			griodp->opt_io_size	= opt_io_size;
			griodp->diskstreams	= NULL;
			spinlock_init(&griodp->lock, "grio_disk_lock");
			griodev = grio_disk_id(griodev);
			if (griodev == GRAPH_VERTEX_NONE) {
				ASSERT(0);
				return -1;
			}
			if(hwgraph_info_add_LBL(griodev, INFO_LBL_GRIO_DSK,
				(arbitrary_info_t) griodp) 
						!= GRAPH_SUCCESS)  {
				ASSERT(0);
				return -1;
			}
			/* insert the griodp into a global list for idbg */
			new_grio_idbg_entry = kmem_zalloc(
				sizeof(grio_idbg_disk_info_t), 
							KM_SLEEP);
			/* when can we free this memory? kaushik*/

			new_grio_idbg_entry->griodp_ptr = griodp;
			new_grio_idbg_entry->next = NULL;
			if(! griodp_idbg_head)  { 
				/* nothing in list yet */
				griodp_idbg_head = griodp_idbg_tail = 
						new_grio_idbg_entry;
			} else {
				/* update existing list */
				griodp_idbg_tail->next = 
						new_grio_idbg_entry;
				griodp_idbg_tail = new_grio_idbg_entry;
			}
		}

		griodp->rotate_position = rotation_slot;
		griodp->realtime_disk = realtime_disk;
		griodp->diskdev = griodev;

		/*
		 * Add a stream disk info struct for nongrio i/o
 		 * Time is in ticks not usecs
 		 */
		grio_add_stream_disk( griodev, 1, 100, 
			opt_io_size, &non_guaranteed_id );
	}
	return( 0 );
}


/*
 * grio_add_stream_disk_info_wrapper
 *	Wapper routine for grio_add_stream_disk_info(). Does cast of
 *	system call parameters and copyin of uuid.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int
grio_add_stream_disk_wrapper(
	sysarg_t	sysarg_physdev,
	sysarg_t	sysarg_disk_id,
	sysarg_t	sysarg_id)
{
	dev_t			griodev;
	grio_disk_id_t		disk_id;
	stream_id_t		stream_id;

	griodev		= (dev_t)sysarg_physdev;

	if (copyin((caddr_t)sysarg_disk_id, &disk_id, sizeof(grio_disk_id_t))) {
		return(EFAULT );
	}

	if ( copyin((caddr_t)sysarg_id, &stream_id, sizeof(stream_id_t)) ) {
		return( EFAULT );
	}

	return( grio_add_stream_disk(griodev,
			disk_id.num_iops,disk_id.iops_time,
			disk_id.opt_io_size, &stream_id) );
}

/*
 * grio_add_stream_disk()
 *	Add the guaranteed rate information for a given grio stream.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
STATIC int
grio_add_stream_disk( 
	dev_t		gdev, 
	int		num_iops, 
	time_t 		iops_time, 
	int		opt_io_size,
	stream_id_t	*stream_id)
{
	int			s,ds,ss;
	uint_t			status;
	grio_disk_info_t	*griodp;
	grio_stream_info_t	*griosp;
	grio_stream_disk_info_t	*griosdp, *tgriosdp;

	griosp = grio_find_stream_with_id( stream_id );
	ASSERT( griosp );


	griodp = grio_disk_info(gdev);

	if ( griodp ) {
		
		/*
		 * find the correct stream info
		 */
		s = GRIO_DISK_LOCK( griodp );
		tgriosdp = griodp->diskstreams;
		while ( (tgriosdp) &&
			(! EQUAL_STREAM_ID((tgriosdp->thisstream->stream_id), 
				(*stream_id))) ) {
			tgriosdp = tgriosdp->nextstream;
		}

		GRIO_DISK_UNLOCK( griodp, s );

		if ( !tgriosdp ) {
			/*
		 	 * Allocate a new stream structure.
		 	 */
			griosdp = (grio_stream_disk_info_t *)kmem_zalloc(
				sizeof(grio_stream_disk_info_t), KM_NOSLEEP);

			if ( griosdp == (grio_stream_disk_info_t *)NULL ) {
				cmn_err(CE_CONT,
					"grio_stream_disk_nfo alloc failed");
				return( -1 );
			}
	
			spinlock_init(&griosdp->lock, "grio_stream_disk_lock");
			griosdp->realbp				= NULL;
			griosdp->bp_list			= NULL;
			griosdp->issued_bp_list			= NULL;
			griosdp->num_iops			= num_iops;
			griosdp->iops_time 			= iops_time;

			griosdp->thisstream 			= griosp;
			griosdp->period_end			= 0;
			griosdp->opt_io_size			= opt_io_size;
			griosdp->iops_remaining_this_period	= 0;
			griosdp->griodp				= griodp;

			griosdp->last_op_time 			= -1;

			/*
			 * add the structure to the disk list
		 	 */
			ds = GRIO_DISK_LOCK( griodp );
			griosdp->nextstream	= griodp->diskstreams;
			griodp->diskstreams	= griosdp;

			/*
			 * add the structure to the stream list
		 	 */
			ss = GRIO_STREAM_LOCK( griosp );
			griosdp->nextdiskinstream	= griosp->diskstreams;
			griosp->diskstreams		= griosdp;
			GRIO_STREAM_UNLOCK( griosp, ss );
			GRIO_DISK_UNLOCK( griodp, ds );

		} else {
			dbg1printf(("stream_disk already allocated \n"));
			ASSERT( 0 );
		}
	} else {
 		ASSERT( 0 );
	}
	return( 0 );
}

/*
 * grio_associate_file_with_stream_wrapper
 *	Wapper routine for grio_associate_file_with_stream(). Does cast of
 *	system call parameters and copyin of uuid.
 *
 * RETURNS:
 *	0 on success
 *	non zero on failure
 *
 * CAVEAT:
 *	must be called within context of proc_id process
 */
int
grio_associate_file_with_stream_wrapper(
	sysarg_t	sysarg_proc_id,
	sysarg_t	sysarg_file_id,
	sysarg_t	sysarg_id_ptr)
{
	int		proc_id, file_id;
	dev_t		fs_dev;
	gr_ino_t	inum;
	stream_id_t	stream_id;
	vfile_t		*fp;

	proc_id		= (int)sysarg_proc_id;
	file_id		= (int)sysarg_file_id;
	if ( copyin((caddr_t)sysarg_id_ptr, &stream_id, sizeof(stream_id_t)) ) {
		return( EFAULT );
	}

	/*
 	 * Convert file_id to inode number, fs device and fp.
 	 *
 	 */
	inum 	= xfs_grio_get_inumber( file_id );
	if ( inum == (gr_ino_t)0 ) {
		return( EIO );
	}
	fs_dev  = xfs_grio_get_fs_dev( file_id );
	if ( fs_dev == (dev_t)0 ) {
		return( EIO );
	}
        if ( getf( file_id, &fp ) != 0) {
                return( EIO );
        }
	return( grio_associate_file_with_stream( proc_id, fs_dev, inum,
			 fp, &stream_id ) );
}

/*
 * grio_associate_file_with_stream
 *	The grio stream associated with the given uid is attached to the
 *	process/file pair.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on failure
 */
STATIC int
grio_associate_file_with_stream(
	pid_t 		proc_id, 
	dev_t		fs_dev,
	gr_ino_t 	inum,
	vfile_t		*fp,
	stream_id_t 	*idptr)
{
	int 			i, s, hashindex;
	uint_t			status;
	grio_stream_info_t	*griosp, *prevgriosp;
	vfile_t			*oldfp;
	int			stream_was_associated = 0;

#ifdef GRIO_DEBUG
	printf("grio_associate_file_with_stream: stream_id %s\n", (char *)idptr);
#endif
	hashindex = GRIO_STREAM_TABLE_INDEX( (__uint64_t)fp );
	s = GRIO_STABLE_LOCK( );

	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE; i++) {
		griosp = grio_stream_table[i];
		prevgriosp = griosp;
		while ( griosp ) {
			if ( EQUAL_STREAM_ID( (*idptr), griosp->stream_id) )   {

				if ( griosp->flags & PROC_PRIVATE_GUAR ) {
					if ( griosp->procid != proc_id ) {
						/*
						 * this is a private guarantee,
						 * it cannot be used by other
						 * processes.
						 */
						GRIO_STABLE_UNLOCK( s );
						return( EPERM );
					}
				}

				if ( griosp->flags & PER_FILE_GUAR ) {
					if ( griosp->ino != inum ) {
						/*
						 * guarantee is on a single
						 * file, cannot be switched
						 */
						GRIO_STABLE_UNLOCK( s );
						return( EPERM );
					}
				}

				if ( griosp->fs_dev != fs_dev ) {
					/*
					 * file must be on the same
					 * file system as the guarantee
					 */
					GRIO_STABLE_UNLOCK( s );
					return( EPERM );
				}

				oldfp		 = (vfile_t *)griosp->fp;
				stream_was_associated = STREAM_IS_ASSOCIATED(griosp);

				/* Store the new info. in the stream structure. */
				griosp->fp	 = (__uint64_t)fp;
				griosp->procid	 = proc_id;
				griosp->ino	 = inum;
				MARK_STREAM_AS_ASSOCIATED(griosp);

				if ( i != hashindex) {
					if ( prevgriosp != griosp )  {
						prevgriosp->nextstream = 
							griosp->nextstream;
					} else {
						grio_stream_table[i] = 
						 	griosp->nextstream;
					}
					griosp->nextstream = 
						grio_stream_table[hashindex];
					grio_stream_table[hashindex] =
						griosp;
				}
			 	GRIO_STABLE_UNLOCK( s );

				/* Before returning, reset the FPRIORITY flag in
				 * the old file and set it in the new one so that
				 * proper I/O scheduling can take place.
				 */
				if (stream_was_associated)
					grio_reset_fpriority_flag(oldfp);
				grio_set_fpriority_flag(fp);

				return( 0 );
			}
			prevgriosp = griosp;
			griosp = griosp->nextstream;
		}
	}

	GRIO_STABLE_UNLOCK( s );
	return( EIO );
}

/*
 * grio_dissociate_stream_wrapper
 *	Wrapper routine for grio_dissociate_stream(). Does cast of
 *	system call parameters and copyin of uuid.
 *
 * RETURNS:
 *	0 on success
 *	non zero on failure
 *
 * CAVEAT:
 *	must be called within context of proc_id process
 */
int
grio_dissociate_stream_wrapper(
	sysarg_t	sysarg_proc_id,
	sysarg_t	sysarg_id_ptr)
{
	int		proc_id;
	stream_id_t	stream_id;

	proc_id		= (int)sysarg_proc_id;
	if ( copyin((caddr_t)sysarg_id_ptr, &stream_id, sizeof(stream_id_t)) ) {
		return( EFAULT );
	}

	return( grio_dissociate_stream( proc_id, &stream_id ) );
}

/*
 * grio_dissociate_stream
 *	The grio stream associated with the given uuid is detached from the
 *	process/file pair.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on failure
 */
STATIC int
grio_dissociate_stream(
	pid_t		proc_id,
	stream_id_t	*idptr)
{
        int                     i, s;
        uint_t                  status;
        grio_stream_info_t      *griosp, *prevgriosp;
	__uint64_t		oldfp;

#ifdef GRIO_DEBUG
	printf("grio_dissociate_stream: stream_id %s\n", (char *)idptr);
#endif
	s = GRIO_STABLE_LOCK( );

	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE; i++) {
		griosp = grio_stream_table[i];
		prevgriosp = griosp;
		while ( griosp ) {
			if ( EQUAL_STREAM_ID( (*idptr), griosp->stream_id) )   {

				if ( griosp->flags & PROC_PRIVATE_GUAR ) {
					if ( griosp->procid != proc_id ) {
						/*
						 * this is a private guarantee,
						 * it cannot be used by other
						 * processes.
						 */
						GRIO_STABLE_UNLOCK( s );
						return( EPERM );
					}
				}

				/* Store old fp for future use. */
				oldfp = griosp->fp;

				/* Modify stream info. */
				griosp->fp = 0;
				griosp->procid = 0;
				MARK_STREAM_AS_DISSOCIATED(griosp);

				/* If the stream is not in slot zero, put it there.
				 * This is because fp is now zero.
				 */
				if ( i != 0 ) {
					if ( prevgriosp != griosp )  {
						prevgriosp->nextstream =
							griosp->nextstream;
					} else {
						grio_stream_table[i] =
							griosp->nextstream;
					}
					griosp->nextstream =
						grio_stream_table[0];
					grio_stream_table[0] =
						griosp;
				}
				GRIO_STABLE_UNLOCK( s );

				/* Before returning, reset the FPRIORITY flag in
				 * the old file. Note that the user assumes
				 * responsibility to reclaim the used up
				 * resources. (Mediabase requirement)
				 */
				grio_reset_fpriority_flag((vfile_t *)oldfp);

				return( 0 );
			}
			prevgriosp = griosp;
			griosp = griosp->nextstream;
		}
	}

	GRIO_STABLE_UNLOCK( s );
	return( EIO );
}

/*
 * grio_add_stream_wrapper
 *	Wapper routine for grio_add_stream_info(). Does cast of
 *	system call parameters and copyin of uuid.
 * 
 * 
 *  RETURNS:
 *	0 on success 
 * 	-1 on failure
 */
int
grio_add_stream_wrapper(
	sysarg_t	sysarg_flags,
	sysarg_t	sysarg_file_id,
	sysarg_t	sysarg_id,
	sysarg_t	sysarg_vod_info)
{
	int		flags, vod_info;
	stream_id_t	stream_id;
	grio_file_id_t	file_id;

	flags		= (int)sysarg_flags;
	vod_info 	= (int)sysarg_vod_info;
	if ( copyin((caddr_t)sysarg_file_id, &file_id,sizeof(grio_file_id_t))) {
		return( EFAULT );
	}
	if ( copyin((caddr_t)sysarg_id, &stream_id, sizeof(stream_id_t)) ) {
		return( EFAULT );
	}
	return( grio_add_stream_info( flags, &file_id, &stream_id, vod_info ) );
}


/*
 * grio_add_stream_info
 *	Add a grio_stream_info structure for the stream with the
 *	the given parameters to the grio_stream_table.
 *
 * RETURNS:
 *	0 on success
 *	-1 if a stream with this uuid already exists OR the stream
 *	   structure could not be created
 */
STATIC int
grio_add_stream_info(
	int		flags,
	grio_file_id_t	*file_id,
	stream_id_t	*stream_id,
	int		vod_info)
{
	int			s;
	grio_stream_info_t	*griosp;
	int			hashindex = 0;

	
#ifdef GRIO_DEBUG
	printf("grio_add_stream_info: stream_id %s\n", (char *)stream_id);
#endif
	/*
	 * check if a stream with this id already exists.
	 */
	if (grio_find_stream_with_id( stream_id ) ) {
#ifdef GRIO_DEBUG
		printf("found duplicate stream id \n");
#endif
		return( -1 );
	} else {
		griosp = (grio_stream_info_t *)
			kmem_zalloc( sizeof(grio_stream_info_t), KM_SLEEP );
		ASSERT( griosp );

		spinlock_init(&(griosp->lock), "grio_stream_info_lock");
		COPY_STREAM_ID( (*stream_id), griosp->stream_id );

		griosp->flags = flags;

		/*
		 * decode the flags field
		 */
		if ( (flags & SLIP_ROTOR_GUAR)  ||
		     (flags & FIXED_ROTOR_GUAR)  ) {
			griosp->total_slots  	    = (vod_info & 0xffff);
			griosp->max_count_per_slot  = 
					((vod_info & 0xffff0000) >> 16);
		}

		griosp->fp		= file_id->fp;
		griosp->procid		= file_id->procid;
		griosp->ino		= file_id->ino;
		griosp->fs_dev		= file_id->fs_dev;
		
		griosp->last_stream_op	= -1;

		/*
		 * Initialize rotate_slot to -1.
		 */
		griosp->rotate_slot	= -1;

		/* Insert the stream in the right place */
		hashindex = GRIO_STREAM_TABLE_INDEX( file_id->fp );
	
		s = GRIO_STABLE_LOCK();
		griosp->nextstream = grio_stream_table[hashindex];
		grio_stream_table[hashindex] = griosp;
		GRIO_STABLE_UNLOCK( s ) ;

	}
	return( 0 );
}


/*
 * grio_remove_stream_info_wrapper
 *	Call the grio_remove_stream_info() routine after copying the 
 *	specified stream id into system space.
 *
 * RETURNS:
 *	0 on success
 *	non zero on failure
 */
int
grio_remove_stream_info_wrapper(
	sysarg_t	sysarg_id_ptr)
{
	stream_id_t	stream_id;
	
	if ( copyin((caddr_t)sysarg_id_ptr, &stream_id, sizeof(stream_id_t)) ) {
		return( EFAULT );
	}
	return( grio_remove_stream_info( &stream_id ) );
}

/*
 * grio_remove_stream
 *	Remove the grio_stream_info and grio_stream_disk_info structures
 *	associated with the given stream.
 *
 *
 * RETURNS:
 * 	0 on success
 *	-1 if stream could not be found
 */
STATIC int
grio_remove_stream_info(
	stream_id_t	*stream_id)
{
	int			s, i;
	uint_t			status;
	grio_stream_info_t	*prevgriosp, *griosp;

#ifdef GRIO_DEBUG
	printf("grio_remove_stream_info: stream_id %s\n", (char *)stream_id);
#endif
	/*
	 * Get the stream info structure, check for existence.
	 */
	griosp = grio_find_stream_with_id( stream_id ); 
	if ( griosp == NULL ) {
#ifdef GRIO_DEBUG
		printf("could not find stream id to remove stream info \n");
#endif
		return( 0 );
	}
	
	/*
	 * Mark the stream as being in the process of being deleted.
	 * If I/O is in the process of being initiated on this steam,
	 * wait until it has been started.
 	 */
	s = GRIO_STREAM_LOCK( griosp );
	while ( STREAM_INITIATE_IO(griosp ) ) {
		GRIO_STREAM_UNLOCK( griosp, s );
		delay( 1 );
		s = GRIO_STREAM_LOCK( griosp );
	}

	if ( STREAM_BEING_REMOVED( griosp ) ) {
		GRIO_STREAM_UNLOCK( griosp, s );
		return( 0 );
	} else {
		MARK_STREAM_AS_BEING_REMOVED( griosp );
		GRIO_STREAM_UNLOCK( griosp, s );
	}

	ASSERT( !issplhi(getsr()) );

	/*
	 * Wait for I/O to drain from all disks in the stream.
	 */
	grio_wait_for_stream_to_drain( griosp );

	s = GRIO_STABLE_LOCK( );
	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE; i++) {
		griosp = grio_stream_table[i];
		prevgriosp = griosp;
		while ( griosp ) {
			if ( EQUAL_STREAM_ID((*stream_id),griosp->stream_id)) {
				if ( prevgriosp == griosp ) {
					grio_stream_table[i] = 
						griosp->nextstream;
				} else {
					prevgriosp->nextstream = 
						griosp->nextstream;
				}
				GRIO_STABLE_UNLOCK( s );
				grio_remove_stream_disk_info( griosp );
				spinlock_destroy( &(griosp->lock) );

				/* Before returning, reset the FPRIORITY flag
				 * in the fp associated with this stream, if
				 * any. Since griosp->fp has already been set
				 * to 0 for closed files, this should be safe.
				 */
				if (griosp->fp) {
					grio_reset_fpriority_flag((vfile_t *)griosp->fp);
				}

				kmem_free( griosp, sizeof(grio_stream_info_t));
				return( 0 );
			}
			prevgriosp = griosp;
			griosp = griosp->nextstream;
		}
	}

	GRIO_STABLE_UNLOCK( s );

	/*
	 * The grio_stream_info structure must be removed.
 	 */
	ASSERT( 0 );

	return( 0 );
}


/*
 * grio_remove_all_streams
 *	Purge all the GRIO streams. This is done when the ggd daemon
 *	is restarted.
 *
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
int
grio_remove_all_streams( void )
{
	int		ret = 0, done = 0;
	stream_id_t	stream_id;

	/*
	 * remove each stream.
	 */
	while ( (!ret) && (!done) ) {
		if ( grio_get_stream_to_remove( &stream_id ) ) {
			ret = grio_remove_stream_info( &stream_id );
		}  else {
			done = 1;
		}
	}
	return ( ret );
}

/*
 * grio_get_stream_to_remove
 *	Return the uuid of a a guaranteed rate stream.
 *
 * RETURNS:
 *	1 if stream was found
 *	0 otherwise
 *
 */
int
grio_get_stream_to_remove( stream_id_t *stream_id )
{
	int			i, s;
	uint_t			status;
	grio_stream_info_t	*griosp;

	s = GRIO_STABLE_LOCK( );

	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE; i++) {
		griosp = grio_stream_table[i];
		while ( griosp ) {
			if ( !GRIO_NONGUARANTEED_STREAM( griosp ) ) {
				GRIO_STABLE_UNLOCK( s );
				COPY_STREAM_ID( griosp->stream_id, (*stream_id) );
				return( 1 );
			}
			griosp = griosp->nextstream;
		}
	}

	GRIO_STABLE_UNLOCK( s );
	return( 0 );
}

/*
 * grio_remove_stream_disk_info
 *	Remove the grio_stream_disk_info structures associated 
 *	with the given stream.
 *
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
STATIC int
grio_remove_stream_disk_info( grio_stream_info_t *griosp )
{
	int 				s;
	grio_disk_info_t		*griodp;
	grio_stream_disk_info_t 	*griosdp, *prevgriosdp, *ngriosdp;

	griosdp = griosp->diskstreams;
	
	while ( griosdp )  {
		ASSERT( griosdp->realbp == NULL );
		ASSERT( griosdp->bp_list == NULL );
		ASSERT( griosdp->issued_bp_list == NULL );

		griodp = griosdp->griodp;
		
		/*
		 * Remove the grio_stream_disk structure from this
		 * grio_disk stream list.
		 */
		s = GRIO_DISK_LOCK( griodp );
		ngriosdp = griodp->diskstreams;
		prevgriosdp = ngriosdp;

		while (ngriosdp != griosdp ) {
			prevgriosdp = ngriosdp;
			ngriosdp = ngriosdp->nextstream;
		}

		if ( ngriosdp == prevgriosdp ) {
			griodp->diskstreams = griosdp->nextstream;
		} else {
			prevgriosdp->nextstream = griosdp->nextstream;
		}
		GRIO_DISK_UNLOCK( griodp , s );
	
		ngriosdp = griosdp->nextdiskinstream;
		spinlock_destroy( &griosdp->lock);
		kmem_free( griosdp, sizeof(grio_stream_disk_info_t) );

		/*
		 * go on to next grio_stream_disk structure
		 */
		griosdp = ngriosdp;
	}
	return( 0 );
}

/*
 * grio_find_stream_with_uuid
 *	Search the grio_stream_table and return the grio stream 
 *	with the given uuid. 
 *
 *
 * RETURNS:
 *	pointer to grio_stream_info structure on success
 *	NULL on failure
 */
grio_stream_info_t *
grio_find_stream_with_id( stream_id_t *idptr ) 
{
	int			s, i;
	uint_t			status;
	grio_stream_info_t	*griosp;

	s = GRIO_STABLE_LOCK( );

	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE; i++) {
		griosp = grio_stream_table[i];
		while ( griosp ) {
			if ( EQUAL_STREAM_ID( (*idptr), griosp->stream_id) )   {
			 	GRIO_STABLE_UNLOCK( s );
				return( griosp );
			}
			griosp = griosp->nextstream;
		}
	}

	GRIO_STABLE_UNLOCK( s );
	return( NULL );
}

/* 
 * grio_find_stream_with_proc_dev_inum
 *	This routine does the tranlation from process/fs_dev/inumber
 *	to the grio stream. It is called in the file close path
 *	and must be very fast.
 *
 * RETURNS:
 *	pointer to grio_stream_info structure on success
 *	NULL on failure
 */
grio_stream_info_t *
grio_find_stream_with_proc_dev_inum( 
	pid_t proc_id, 
	dev_t fs_dev, 
	gr_ino_t inum)
{
	int			s;
	grio_stream_info_t 	*griosp;
	int			i;
	
	s = GRIO_STABLE_LOCK();
	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE; i++) {
		griosp = grio_stream_table[i];
		while ( griosp ) {

			if ( 	(griosp->procid == proc_id)  &&
				(griosp->fs_dev  == fs_dev)   &&
				(griosp->ino    == inum)	)   {
		 		GRIO_STABLE_UNLOCK( s );
				return( griosp );
			}
			griosp = griosp->nextstream;
		}
	}
	GRIO_STABLE_UNLOCK( s );
	return( NULL );
}

/*
 * grio_find_stream_with_fp
 *	This routine does the translation from fp to the grio stream.
 *	It is called in the file system i/o path and must be very fast.
 *
 * RETURNS:
 *	pointer to grio_stream_info structure on success
 *	NULL on failure
 */
grio_stream_info_t *
grio_find_stream_with_fp(
	vfile_t *fp)
{
	int			hashindex, s;
	grio_stream_info_t	*griosp;

	hashindex	= GRIO_STREAM_TABLE_INDEX( (__uint64_t)fp );

	s = GRIO_STABLE_LOCK();
	griosp = grio_stream_table[hashindex];
	while ( griosp ) {
		if ( griosp->fp == (__uint64_t) fp) {
			GRIO_STABLE_UNLOCK( s );
			return(griosp);
		}
		griosp = griosp->nextstream;
	}
	GRIO_STABLE_UNLOCK( s );
	return( NULL );
}

/*
 * grio_find_associated_stream_with_fp
 *	This routine does the translation from fp to the grio stream.
 *	It is called on the file system i/o path and must be very fast.
 *	A stream is valid only if it is "associated".
 *
 * RETURNS:
 *	pointer to the grio_stream_info structure on success
 *	NULL on failure.
 */
grio_stream_info_t *
grio_find_associated_stream_with_fp(
	vfile_t *fp)
{
	grio_stream_info_t	*griosp;

	griosp = grio_find_stream_with_fp(fp);
	if (griosp)
		if (STREAM_IS_ASSOCIATED(griosp))
			return (griosp);

	return NULL;
}

/*
 * grio_wait_for_stream_to_drain
 *	This routine waits until the completion of the outstanding 
 * 	i/o on this stream.
 *
 * RETURNS:
 *	none
 */
void
grio_wait_for_stream_to_drain( grio_stream_info_t *griosp)
{
	int 				s;
	grio_stream_disk_info_t		*griosdp;

	griosdp = griosp->diskstreams;

	/*
	 * Check each disk in this stream and make sure all I/O
	 * is completed.
	 */
	while (  griosdp ) {

		s = GRIO_STREAM_DISK_LOCK( griosdp );
		while ( griosdp->realbp != NULL) {
			GRIO_STREAM_DISK_UNLOCK( griosdp, s);
			delay( 10 );
			s = GRIO_STREAM_DISK_LOCK( griosdp );
		}
		GRIO_STREAM_DISK_UNLOCK( griosdp, s );
		
		griosdp = griosdp->nextdiskinstream;
	}
	return;
}


/* 
 * grio_mark_as_unreserved
 *	Mark this bp as not belonging to a grio stream.
 *	(set grio_uuid to 0)
 *
 * RETURNS:
 *	none
 */
void
grio_mark_bp_as_unreserved( buf_t *bp )
{
	uint_t		status;
	grio_buf_data_t	*griobdp;

	griobdp = (grio_buf_data_t *)(bp->b_grio_private);
	uuid_create_nil( &(griobdp->grio_id), &status);

	return;
}

/*
 * grio_add_nonguaranteed_stream_info
 *	Add a stream_info structure for the nonguaranteed stream.
 *
 * RETURNS:
 *	none
 */
void
grio_add_nonguaranteed_stream_info()
{
	int			flags;
	uint_t			status;
	grio_file_id_t		file_id;

	uuid_create_nil( &non_guaranteed_id, &status );


	/*
 	 *  dummy up a file id to add stream info.
 	 */
	file_id.ino = 0;
	file_id.fs_dev = 0;
	file_id.procid = 0;
	file_id.fp = 0;
	flags  = PER_FILE_GUAR | PROC_PRIVATE_GUAR;
	flags |= NON_ROTOR_GUAR | SYSTEM_GUAR;

	grio_add_stream_info( flags, &file_id, &non_guaranteed_id, 0 );
	return;
}


/*
 * grio_purge_vdisk_cache()
 *      This routine issues the command to the guarantee granting daemon
 *      which will cause it to purge its cached information concerning the
 *      given xlv device.
 *      The vdev should be the data partition of the xlv device.
 *
 * RETURNS:
 *      0 on success
 *      non 0 on failure
 */
int
grio_purge_vdisk_cache( dev_t vdev)
{
	int             ret;
	grio_cmd_t      griocmd;

	bzero(&griocmd, sizeof(grio_cmd_t));
	griocmd.gr_cmd       	= GRIO_PURGE_VDEV_ASYNC;
	griocmd.one_end.gr_dev       = vdev;
	griocmd.gr_procid       = 1;

	/*
	 * Issue the message, do not wait for completion.
	 */
	ret = grio_issue_async_grio_req((char *)&griocmd);
	return( ret );
}


/*
 * grio_queue_bp_request
 *	Queue the next request for this stream on this disk onto the
 *	list kept in the grio_stream_disk_info_t structure
 * 
 * RETURNS:
 *	always returns 0
 */
int
grio_queue_bp_request( grio_stream_disk_info_t *griosdp, buf_t *bp)
{

	if ( griosdp->queuedbps_front ) {
		griosdp->queuedbps_back->av_back = bp;
		griosdp->queuedbps_back = bp;
	} else {
		ASSERT( griosdp->queuedbps_back == NULL);
		griosdp->queuedbps_front = griosdp->queuedbps_back = bp; 
	}
	bp->av_back = NULL;

	return( 0 );
}

/* 
 * grio_get_queue_bp_request()
 *	Get the next request issued on this disk from this stream.
 *
 * RETURNS:
 *	a pointer to the buffer for the next request
 *	or a NULL if no more requests
 */
buf_t *
grio_get_queued_bp_request( grio_stream_disk_info_t *griosdp )
{
	buf_t	*bp;
	
	if ( (bp = griosdp->queuedbps_front) == NULL ) {
		ASSERT( griosdp->queuedbps_back == NULL);
		return( NULL );
	}

	if (bp->av_back == NULL )
		ASSERT( griosdp->queuedbps_back == bp );

	griosdp->queuedbps_front = bp->av_back;
	if ( bp == griosdp->queuedbps_back ) {
		ASSERT( griosdp->queuedbps_front == NULL);
		griosdp->queuedbps_back = NULL;
	}

	bp->av_forw = bp->av_back = NULL;

	return( bp );
	
}

/*
 * grio_get_info_from_bp()
 *	Get the grio_stream_disk_info_t structure associated with the
 *	the given I/O buffer. If the I/O request is not rate
 *	guaranteed, associate the bp with the non-guaranteed 
 *	rate stream. If the buffer is marked B_PRV_BUF, make sure to 
 *	save the iopri value that was passed in.
 *
 * RETURNS:
 *	a pointer to the grio_stream_disk_info_t structure for the
 *	bp
 */
grio_stream_disk_info_t *
grio_get_info_from_bp( buf_t *bp)
{
	grio_stream_disk_info_t	*griosdp;

	if ( BUF_IS_GRIO(bp) ) {
		griosdp = grio_get_stream_disk_info( bp );
		if ( griosdp == NULL ) {
			COPY_STREAM_ID( non_guaranteed_id,
				BUF_GRIO_PRIVATE(bp)->grio_id);
			griosdp = grio_get_stream_disk_info( bp );
		}
	} else {
		short prval = bp->b_iopri;

		ASSERT((bp->b_flags&B_PRV_BUF)||(BUF_GRIO_PRIVATE(bp)==NULL));
		bp->b_grio_private =
			(grio_buf_data_t *)kmem_zone_alloc(
				grio_buf_data_zone, KM_NOSLEEP);
		ASSERT( BUF_GRIO_PRIVATE(bp) );
		COPY_STREAM_ID( non_guaranteed_id,
			BUF_GRIO_PRIVATE(bp)->grio_id);
		SET_GRIO_IOPRI(bp, prval);
		griosdp = grio_get_stream_disk_info( bp );

	}
	ASSERT( griosdp );
	return( griosdp );
}



/* 
 * grio_free_bp()
 *	Free the grio_buf_data associated with this bp, and 
 *	return the buffer to the free pool.
 *
 * RETURNS:
 *	always returns 0
 */
int
grio_free_bp( buf_t *bp)
{

	/*
	 * Free the bp
	 */
	bp->b_grio_list = NULL;
	bp->b_iodone = 0;
	ASSERT( bp->b_grio_private );
	bp->b_grio_private = NULL;
	freerbuf( bp );

	return( 0 );
}


/*
 * grio_complete_real_bp()
 *	Free the grio_buf_data structure associated with this buffer,
 *	if necessary. Call iodone().
 *
 * RETURNS:
 *	always returns 0
 */
int
grio_complete_real_bp(buf_t *realbp )
{
	short 	prval = 0;

	ASSERT( grio_get_stream_disk_info( realbp ) );

	if ( !BUF_IS_GRIO(realbp) ) {
		ASSERT( BUF_GRIO_PRIVATE(realbp) );
		if (BUF_IS_IOSPL(realbp)) 		/* ie B_PRV_BUF */
			GET_GRIO_IOPRI(realbp, prval);
		kmem_zone_free(grio_buf_data_zone, BUF_GRIO_PRIVATE(realbp));
		realbp->b_grio_private = NULL;
		if (BUF_IS_IOSPL(realbp))
			realbp->b_iopri = prval;
	}
	iodone ( realbp );

	return( 0 );
}




/*
 * grio_get_stream_id_wrapper()
 *	Return the stream id of the stream associated with the given
 *	process and file.
 *
 *
 *
 * RETURNS:
 *	ENOENT if no stream id was found
 *	0 if a stream id was found
 *	-1 on error
 */
/*ARGSUSED*/
int
grio_get_stream_id_wrapper(
	sysarg_t	sysarg_proc_id,
	sysarg_t	sysarg_file_id,
	sysarg_t	sysarg_id_ptr)
{
	int			file_id;
	caddr_t			id_ptr;
	grio_stream_info_t	*griosp;
	vfile_t			*fp;

	file_id		= (int)sysarg_file_id;
	id_ptr		= (caddr_t)sysarg_id_ptr;

	/*
 	 * Convert file_id to fp.
 	 */
	if ( getf( file_id, &fp ) != 0) {
		return -1;
	}

	griosp = grio_find_stream_with_fp( fp );
	if ( griosp )  {
		if ( copyout((void *)(&(griosp->stream_id)), 
				id_ptr, sizeof(stream_id_t)) ) {
			ASSERT( 0 );
			return( -1 );
		}
		return( 0 );
	} 
	return( ENOENT );
}

/*
 * grio_get_fp()
 *	Return the fp for the passed in user fd.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int
grio_get_fp(
	sysarg_t	sysarg_file_id,
	sysarg_t	sysarg_fp_ptr)
{
	int			file_id;
	caddr_t			fp_ptr;
	vfile_t			*fp;

	file_id		= (int)sysarg_file_id;
	fp_ptr		= (caddr_t)sysarg_fp_ptr;

	/* Convert file_id to fp */
	if ( getf( file_id, &fp ) != 0) {
		return -1;
	}

	if (fp) {
		if (copyout((void *)(&fp), fp_ptr, sizeof(vfile_t *))) {
			return -1;
		}
		return 0;
	}
	return -1;
}

/*
 * grio_determine_wait_until_priority
 *	Calculate the time in ticks until this stream has priority
 *	to issue I/O on this disk.
 *
 *
 * RETURNS:
 *	time in ticks until stream has priority on this disk
 */
time_t
grio_determine_wait_until_priority( grio_stream_disk_info_t *griosdp )
{
	int			iops_time;
	time_t			now, delaytime, wake_time;
	grio_stream_info_t	*griosp;


	if ( griosdp->iops_remaining_this_period ) {
		return( (time_t)0 );
	}	

	now 		= lbolt;
	griosp          = griosdp->thisstream;
	iops_time       = griosdp->iops_time;

	if ( ROTOR_STREAM( griosdp->thisstream ) ) {

		/*
		 * no wait if stream has not started yet
		 */
		if (griosp->rotate_slot == -1 ) {
			return( (time_t) 0 );
		}

		/*
		 * determine time until next time quantum
		 */
		delaytime = iops_time - (now % iops_time);

		ASSERT( delaytime <= iops_time );
		ASSERT( delaytime >= 0 );


	} else {
		ASSERT( NON_ROTOR_STREAM( griosp ) );

		if ( griosdp->period_end <= now ) {
			return( (time_t)0 );
		}
		delaytime      = griosdp->period_end - now;
	}

	/*
	 * delay the stream until "delaytime" ticks have passed
	 */
	wake_time = now + delaytime;

	delaytime = wake_time - lbolt - 1;

	if ( delaytime < 0 ) {
		delaytime = 0;
	}

	return( delaytime );
}


/*
 * grio_determine_remaining_priority_time
 *	Calculate the time in ticks until this stream's priority
 *	period completes.
 *
 *
 * RETURNS:
 *	time in ticks until stream's current priority expires
 *
 */
time_t
grio_determine_remaining_priority_time( grio_stream_disk_info_t *griosdp )
{
	time_t			now, prioritytime;
#ifdef DEBUG
	grio_stream_info_t	*griosp = griosdp->thisstream;
#endif


	/*
	 * sanity check: this routine should not be called if
	 * this stream does not have remaining priority
	 */
	if ( griosdp->iops_remaining_this_period == 0) {
		return( (time_t)HZ );
	}	

	now 		= lbolt;

	/*
	 * determine time until end of time quantum
	 */
	if ( ROTOR_STREAM( griosdp->thisstream ) ) {

		/*
		 * always use 0 for ROTOR guarantees
		 */
		prioritytime = 0;

	} else {
		ASSERT( NON_ROTOR_STREAM( griosp ) );

		if ( griosdp->period_end <= now ) {
			return( (time_t)0 );
		}
		prioritytime = griosdp->period_end - now;
	}

	if ( prioritytime <= 1  ) {
		prioritytime = 0;
	}

	return( prioritytime );
}


/*
 * grio_timeout
 *	This routine is called when a timeout occurs on a drive.
 *	Issue the next I/O request to the drive.
 *
 * RETURNS:
 *	none
 */
void
grio_timeout( grio_disk_info_t *griodp)
{
	int  	s;
	time_t	now;

	s = GRIO_DISK_LOCK( griodp );
	
	griodp->timeout_time = 0;
	griodp->timeout_id   = 0;
	
	GRIO_DISK_UNLOCK( griodp, s );

	/*
	 * try to start another I/O
	 */
	now = lbolt;
	grio_start_next_io( griodp, now, 0 );
}

/*
 * grio_remove_stream_fp_info()
 *	Remove the fp info. associated with the stream.
 *
 */
STATIC void
grio_remove_stream_fp_info(
	grio_stream_info_t *griosp)
{
	/*
	 * No locks held here, this file close code races with 
	 * stream associate calls asking to associate the stream
	 * with a different file.
	 */
	if (griosp->fp) {
		MARK_STREAM_AS_DISSOCIATED(griosp);
		grio_reset_fpriority_flag((vfile_t *)griosp->fp);
		griosp->fp = 0;
	}
}

/*
 * grio_remove_reservation() 
 *	Remove the GRIO reservation stream.
 * 
 * RETURNS: 
 *	0 on success 
 * 	non-0 on failure
 */
int
grio_remove_reservation( 
	grio_stream_info_t *griosp)
{
	int             	ret = 0;
	grio_cmd_t      	griocmd;

	if ( griosp ) {

		/* 
		 *  Issue asynchronous remove call to ggd
		 * 
	 	 */
		bzero(&griocmd, sizeof(grio_cmd_t));
		griocmd.gr_cmd       	= GRIO_UNRESV_ASYNC;
		griocmd.one_end.gr_dev	= 0;
		griocmd.gr_procid	= current_pid();
		COPY_STREAM_ID( griosp->stream_id,
                        griocmd.cmd_info.gr_resv.gr_stream_id);

		/*
	 	 * Issue the message, do not wait for completion.
	 	 */
		ret = grio_issue_async_grio_req((char *)&griocmd);

	}
	return( ret );
}

int
grio_remove_reservation_with_fp(
	vfile_t *fp)
{
	int			ret = 0;
	grio_stream_info_t	*griosp;

	griosp = grio_find_stream_with_fp( fp );

	if (griosp) {

		/* Remove the stream fp info. This should be done
		 * here synchronously.
		 */
		grio_remove_stream_fp_info(griosp);

		ret = grio_remove_reservation(griosp);
	}
	return (ret);
}

int
grio_remove_reservations_with_proc_dev_inum(
	pid_t pid, dev_t fs_dev, xfs_ino_t ino)
{
	int			ret = 0;
	grio_stream_info_t	*griosp;
	int			i;
	int			s;
	
	s = GRIO_STABLE_LOCK();
	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE; i++) {
		griosp = grio_stream_table[i];
		while ( griosp ) {

			if ( 	(griosp->procid == pid)  &&
				(griosp->fs_dev  == fs_dev)   &&
				(griosp->ino    == ino)	)   {
				ret = grio_remove_reservation( griosp );
			}
			griosp = griosp->nextstream;
		}
	}
	GRIO_STABLE_UNLOCK( s );

	return (ret);
}

/* 
 * grio_get_all_streams()
 *	Copy the stream ids (up to MAX_STREAM_STAT_COUNT) of the active 
 *	streams into the memory pointed	at by siptr.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 if more that MAX_STREAM_STAT_COUNT
 */
int
grio_get_all_streams( sysarg_t	siptr )
{
	int 			i, streamcount = 0;
	grio_stream_info_t 	*griosp;
	grio_stream_stat_t	grioss;
	
	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE; i++) {
		griosp = grio_stream_table[i];
		while ( griosp ) {

			if ( streamcount >= MAX_STREAM_STAT_COUNT )
				break;

			bzero( &grioss, sizeof(grio_stream_stat_t));


			COPY_STREAM_ID( griosp->stream_id, 
					grioss.stream_id );

			grioss.ino	= griosp->ino;
			grioss.fs_dev	= griosp->fs_dev;
			grioss.procid	= griosp->procid;

			if ( copyout( (void *)(&grioss), (void *)siptr, 
				sizeof(grio_stream_stat_t)) ) {
				return( -1 );
			}
			siptr = siptr + sizeof(grio_stream_stat_t);

			streamcount++;

			griosp = griosp->nextstream;
		}
	}
	
	if ( i == MAX_STREAM_STAT_COUNT)
		return( -1 );


	return( 0 );
}


/* 
 * grio_get_vod_disk_info()
 *	Return a grio_vod_info_t structure for the specified disk device.
 *
 * RETURNS: 
 *	0 on success
 *	-1 on error
 */
int
grio_get_vod_disk_info( sysarg_t sysarg_dev, sysarg_t	siptr )
{
	dev_t			diskdev;
	grio_vod_info_t		griovp;
	grio_disk_info_t	*griodp;
	grio_stream_disk_info_t	*griosdp;

	diskdev = (dev_t)sysarg_dev;
	griodp = grio_get_disk_info( diskdev );

	if (griodp == NULL ) {
		return( -1 );
	}

	bzero( &griovp, sizeof( grio_disk_info_t ) );
	griovp.rotor_position = griodp->rotate_position;
	griovp.num_rotor_slots = 0;

	griosdp = griodp->diskstreams;
	while ( griosdp ) {
		if ( ROTOR_STREAM(griosdp->thisstream) ) {
			if ( griovp.num_rotor_slots != 0 ) {
	 			ASSERT( griovp.num_rotor_slots == 
					griosdp->thisstream->total_slots ) ;
			}
			griovp.num_rotor_slots = 
				griosdp->thisstream->total_slots;

			griovp.num_rotor_streams++;
			griovp.rotor_slot[griosdp->thisstream->rotate_slot]++;

		} else {
			griovp.num_nonrotor_streams++;
		}
		griosdp = griosdp->nextstream;
	}

	if ( copyout( (void *)(&griovp), (void *)siptr, 
		sizeof(grio_vod_info_t)) ) {
		return( -1 );
	}

	return( 0 );
}

/*
 * grio_set_fpriority_flag
 *	This function sets the FPRIORITY flag in the vfile_t.
 */
STATIC void
grio_set_fpriority_flag(
	vfile_t *fp)
{
	int s;

#ifdef GRIO_DEBUG
	printf("grio_set_fpriority_flag: fp %x\n", fp);
#endif
	s = VFLOCK(fp);
	fp->vf_flag |= FPRIORITY;
	VFUNLOCK(fp, s);
}

/*
 * grio_reset_fpriority_flag
 *	This function resets the FPRIORITY flag in the vfile_t.
 */
STATIC void
grio_reset_fpriority_flag(
	vfile_t *fp)
{
	int s;

#ifdef GRIO_DEBUG
	printf("grio_reset_fpriority_flag: fp %x\n", fp);
#endif
	s = VFLOCK(fp);
	fp->vf_flag &= ~FPRIORITY;
	VFUNLOCK(fp, s);
}

/*
 * This code returns the dma path data follows from a device to the
 * memory. It returns an identifier for each hardware component on
 * this path, which can be used to make ioctls to these hardware.
 * It also returns the class/type/state of the device corresponding
 * to the identifier. It does this by using the hwgfs support.
 * Code works on lego, speedo and speedracer for now. Has been 
 * extended to work on older platforms too.
 */
int
grio_vhdl_list(vertex_hdl_t drv_vhdl, grio_dev_info_t *list_head, 
					int max_items, uint *num_items)
{
	vertex_hdl_t 		cur_vhdl, prev_vhdl, t_vhdl;
	grio_dev_info_t		*curptr, *buffer;
	int			count = 0, error = 0;
	int			bufsz = sizeof(grio_dev_info_t) * max_items;

  	/* Initializations */
  	prev_vhdl = cur_vhdl = drv_vhdl;
	curptr = buffer = kmem_alloc(bufsz, KM_SLEEP);

	do  {
		error = dev_class_type(cur_vhdl, &curptr->grio_dev_class, 
			 &curptr->grio_dev_type, &curptr->grio_dev_state);
		if (error) {
			kmem_free(buffer, bufsz);
			return(EPERM);
		}

		curptr->devnum = dev_ioctl_hdl(cur_vhdl, 
			curptr->grio_dev_class, curptr->grio_dev_type);
		if (curptr->devnum == GRAPH_VERTEX_NONE) {
			kmem_free(buffer, bufsz);
			return(EPERM);
		}
#ifdef GRIO_DEBUG
		printf("grio_vhdl_list:: dev %d class %d type %d state %d\n",
			curptr->devnum, curptr->grio_dev_class,
			curptr->grio_dev_type, curptr->grio_dev_state);
#endif /* GRIO_DEBUG */

		count++;
		max_items--;
		if(max_items == 0)
			break;
		t_vhdl = cur_vhdl;
		cur_vhdl = getnext(cur_vhdl,prev_vhdl,curptr->grio_dev_class,
			curptr->grio_dev_type, curptr->grio_dev_state);
		prev_vhdl = t_vhdl;
		curptr++;
	} while(cur_vhdl != GRAPH_VERTEX_NONE);

	/*
	 * Lets copy the data out to user space now.
	 */
	if (copyout(buffer, (caddr_t)list_head, 
			count * sizeof(grio_dev_info_t))) {
		kmem_free(buffer, bufsz);
		return(EIO);
	}

	kmem_free(buffer, bufsz);

	/*
	 * ... and the count.
	 */
	if (copyout(&count, (caddr_t)num_items, sizeof(int))) 
		return(EIO);

	return(0);
}


/*
 * This procedure takes a device number and sees what labels are
 * associated with the vertex. Depending on that, it determines
 * the class and type of the device.
 */
STATIC int
dev_class_type(vertex_hdl_t device, int *class, int *type, int *state)
{
	invplace_t	invplace = INVPLACE_NONE;
	inventory_t	*invrec = 0;

	/*
	 * First try the inventory; recognize only scsi disk and
	 * controllers.
	 */
	if (invrec = device_inventory_get_next(device, &invplace)) {
		if ((invrec->inv_type != INV_SCSICONTROL) &&
#if !IP27 && !IP30
		    (invrec->inv_type != INV_GIO_SCSICONTROL) &&
		    (invrec->inv_type != INV_PCI_SCSICONTROL) &&
#endif
		    (invrec->inv_type != INV_RAIDCTLR) &&
		    (invrec->inv_type != INV_SCSIDRIVE))
			return(-1);
		*class = invrec->inv_class;
		*type = invrec->inv_type;
		*state = invrec->inv_state;
		return(0);
	}

#if !IP27 && !IP30
	/*
	 * In the absence of hwgraph, this is a hack for EVEREST systems,
	 * since we skip modelling the ioas and iobs. But that works
	 * since disks can never saturate either the ibus or the ebus.
	 * For other platforms, grio does not differentiate between 
	 * system types to determine the system bw limit - so, pass
	 * back IP25 indicator. This can be changed to look up the
	 * inventory, but that is unneccesary (since the system bw can
	 * also be tuned via grio_disks.
	 */
	*class = INV_PROCESSOR;
	*type = INV_CPUBOARD;
	*state = INV_IP25BOARD;
	return(0);
#else
	/*
	 * Next, check for PCI Bridge.
	 */
	if ((hwgraph_info_get_LBL(device, INFO_LBL_PFUNCS, 0)) ==
			GRAPH_SUCCESS) {
		*class = INV_MISC;
		*type = INV_PCI_BRIDGE;
		*state = 0;
		return(0);
	}

	/*
	 * On to a xbow (...../xtalk/0).
	 */
	if ((hwgraph_info_get_LBL(device, INFO_LBL_XWIDGET, 0)) ==
			GRAPH_SUCCESS) {
		*class = INV_MISC;
		*type = INV_XBOW;
		*state = 0;
		return(0);
	}

	/*
	 * Then the node, ie the hub (..../node).
	 */
	if ((hwgraph_edge_get(device, EDGE_LBL_XTALK, 0)) ==
			GRAPH_SUCCESS) {
		*class = INV_MISC;
		*type = INV_HUB;
		*state = 0;
		return(0);
	}

	/* Should we do memory too, based on _detail_invent? */

	/*
	 * All matches failed - we do not know the device.
	 */
	return(-1);
#endif
}


/*
 * This takes the nonleaf vertex number and the class and type
 * of device represented by the vertex. It returns a device number
 * which can support ioctls to the underlying device.
 */
STATIC vertex_hdl_t
dev_ioctl_hdl(vertex_hdl_t device, int class, int type)
{
	vertex_hdl_t	ioc_vhdl = GRAPH_VERTEX_NONE;

	/*
	 * For scsi disks and controllers, raid and the hub, no
	 * ioctls are needed.
	 */
	if ((class == INV_DISK) || (type == INV_HUB) || 
				(type == INV_RAIDCTLR))
		return(device);

#if !IP27 && !IP30
	/*
	 * Hack for older platforms
	 */
	if (class == INV_PROCESSOR)
		return (device);
#endif

	/*
	 * For the PCI Bridge, its the "controller".
	 */
	if (type == INV_PCI_BRIDGE) {
		hwgraph_edge_get(device, EDGE_LBL_CONTROLLER, &ioc_vhdl);
		return(ioc_vhdl);
	}

	/*
	 * For the XBOW, its the "xbow".
	 */
	if (type == INV_XBOW) {
		hwgraph_edge_get(device, EDGE_LBL_XBOW, &ioc_vhdl);
		return(ioc_vhdl);
	}

	return(GRAPH_VERTEX_NONE);
}


/*
 * This takes the current and previous components of the DMA path
 * and returns the next component of the path. Most of the code works
 * just by using the type field, and that works for now.
 */
/*ARGSUSED*/
STATIC vertex_hdl_t
getnext(vertex_hdl_t curdev, vertex_hdl_t prevdev, 
	int curclass, int curtype, int curstate)
{
	int		i;
	vertex_hdl_t	pcidev, tdev, hubdev, xbowdev;

#if !IP27 && !IP30
	if (curclass == INV_PROCESSOR)
		return(GRAPH_VERTEX_NONE);
#endif
	tdev = curdev;

	/*
	 * Handle disks.
	 */
	if (curtype == INV_SCSIDRIVE) {
		if (curstate & INV_RAID5_LUN) {	/* Handle raid disks */
			for (i = 0; i < 2; i++) {
				if (hwgraph_edge_get(tdev, 
					HWGRAPH_EDGELBL_DOTDOT, &tdev) 
						!= GRAPH_SUCCESS)
					return(GRAPH_VERTEX_NONE);
			}
			if (hwgraph_edge_get(tdev, "0", 
					&tdev) != GRAPH_SUCCESS)
				return(GRAPH_VERTEX_NONE);
			return(tdev);
		} else {			/* Handle scsi disks */
			for (i = 0; i < 5; i++) {
				if (hwgraph_edge_get(tdev, 
					HWGRAPH_EDGELBL_DOTDOT, &tdev) 
						!= GRAPH_SUCCESS)
					return(GRAPH_VERTEX_NONE);
			}
			return(tdev);
		}
	}

	/*
	 * Handle scsi controllers - parse the name upto pci
	 * Hack for old platforms - go up to the /hw/node level
	 */
#if IP27 || IP30
	if (curtype == INV_SCSICONTROL) {
		for (i = 0; i < 3; i++) {
#else	/* pre O series platforms */
	if ((curtype == INV_SCSICONTROL) || (curtype == INV_PCI_SCSICONTROL) ||
		(curtype == INV_GIO_SCSICONTROL)) {
		for (i = 0; i < 2; i++) {
#endif
			if (hwgraph_edge_get(tdev, HWGRAPH_EDGELBL_DOTDOT, 
					&tdev) != GRAPH_SUCCESS)
				return(GRAPH_VERTEX_NONE);
		}
#if IP27 || IP30
		return(tdev);
#else	/* pre O series platforms */
		if (hwgraph_edge_get(tdev, EDGE_LBL_NODE, &tdev) !=
				GRAPH_SUCCESS)
			return(GRAPH_VERTEX_NONE);
		return(tdev);
#endif
	}

	/*
	 * Handle raid controllers
	 */
	if (curtype == INV_RAIDCTLR) {
		for (i = 0; i < 4; i++) {
			if (hwgraph_edge_get(tdev, HWGRAPH_EDGELBL_DOTDOT, 
					&tdev) != GRAPH_SUCCESS)
				return(GRAPH_VERTEX_NONE);
		}
		return(tdev);
	}

	/*
	 * If the current node is the PCI Bridge or the Xbow, then
	 * get to the master hub.
	 */
	if (curtype == INV_PCI_BRIDGE)
		pcidev = curdev;
	else if (curtype == INV_XBOW)	/* never on speedo */
		pcidev = prevdev;
	else 				/* router code here */
		return(GRAPH_VERTEX_NONE);
	
	if (hwgraph_edge_get(pcidev, HWGRAPH_EDGELBL_DOTDOT, &tdev) !=
				GRAPH_SUCCESS)
		return(GRAPH_VERTEX_NONE);
	hubdev = device_master_get(tdev);
	if (hubdev == GRAPH_VERTEX_NONE)
		return(GRAPH_VERTEX_NONE);

	/*
	 * If we are currently at the Xbow, we are done. If we are
	 * on a speedo, we are also done - but no way to detect that
	 * until later.
	 */
	if (curtype == INV_XBOW)
		return(hubdev);

	/*
	 * Else, we are at the PCI Bridge. Find the xbow connected
	 * to the master hub that we just found. We will fail on a
	 * speedo, assuming that the io widget is NOT numbered 0 on
	 * a speedo.
	 */
	if (hwgraph_edge_get(hubdev, EDGE_LBL_XTALK, &tdev) !=
				GRAPH_SUCCESS)
		return(GRAPH_VERTEX_NONE);
	if (hwgraph_edge_get(tdev, "0", &xbowdev) != GRAPH_SUCCESS) {
		/* 
		 * definitely not on a lego, must be speedo 
		 * return the hub device number.
		 */
		return(hubdev);
	}
	
	/*
	 * We could be on a lego or a speedo with an io widget of 0.
	 * The way to make out is to look at the _pcibr_hints label:
	 * a bridge at widget 0 on a speedo will have it, but a lego 
	 * will not have it.
	 */
	if (hwgraph_info_get_LBL(xbowdev, INFO_LBL_PCIBR_HINTS, 0)
					!= GRAPH_SUCCESS) {
		/*
		 * We are on a lego. Return xbow identifier.
		 */
		return(xbowdev);
	} else {
		/*
	 	 * We are on a speedo with widget 0.
		 */
		return(hubdev);
	}
}

/* 
 * get the cnode on which the MLD was placed; this is to be able 
 * to determine the path on which to do reservations 
 */
/*ARGSUSED*/
int
grio_get_mld_cnode(pmo_handle_t *grio_mld, cnodeid_t *cnode)
{
#if LATER
	mldref_t	mld_ref;
	pmo_handle_t	mld_pmo;

	if ( copyin((caddr_t) grio_mld, &mld_pmo, 
					sizeof(pmo_handle_t)) ) {
		return( EFAULT );
	}

	mld_ref = curpmo_ns_find(mld_pmo, __PMO_MLD);
	if(! mld_ref)  {
		return(EINVAL);
	}

	if(! mld_isplaced((mld_ref))) {
		printf("grio_get_mld_cnode: MLD wasn't placed\n");
		return -1;
	} else  {
		*cnode = mld_getnodeid(mld_ref);
	}
	return	0;
#else
	return EPERM;
#endif /* LATER */
}

