#ifdef SIM
#define _KERNEL
#include <stdio.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/immu.h>
#include <sys/major.h>
#include <sys/mkdev.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/fs/xfs_inum.h>
#include <sys/uuid.h>
#include <sys/time.h>
#include <sys/grio.h>
#include <sys/bpqueue.h>
#include <sys/pfdat.h>
#include <sys/xlate.h>
#include <sys/kmem.h>
#include <sys/syssgi.h>
#include <sys/scsi.h>
#include <ksys/vfile.h>
#ifdef SIM
#undef _KERNEL
#endif

/*
 * Global variables.
 */
#ifdef DEBUG
int				grio_dbg_level = 0;
#endif
				/* memory zone for allocating grio data       */
zone_t				*grio_buf_data_zone;
				/* ggd daemon sleeps on sema waiting for work */
sv_t				grio_q_sv;
				/* protect global grio data */
lock_t           		grio_global_lock;	
				/* used by bp queue daemon to start new reqs  */
bp_queue_t 			grio_subreq_bpqueue;		
				/* list of user requests to ggd daemon	      */
grio_cmd_queue_t 		*grio_q;
				/* lock for grio_stream_table 		      */
lock_t				grio_stream_table_lock; 
				/* global array of per stream grio structures */
grio_stream_info_t		*grio_stream_table[GRIO_STREAM_TABLE_SIZE];
				/* stream id of the non guaranteed stream     */
extern stream_id_t		non_guaranteed_id;

				/* the disk-info structs needed for idbg */
grio_idbg_disk_info_t   	*griodp_idbg_head, *griodp_idbg_tail;


/*
 * Extern routine declarations.
 */
extern int			grio_free_bp(					
					buf_t *);
extern int			grio_complete_real_bp(
					buf_t *);
extern int			grio_add_disk_wrapper( 
					sysarg_t);
extern int			grio_add_stream_wrapper(
					sysarg_t, sysarg_t, sysarg_t, sysarg_t);
extern int			grio_add_stream_disk_wrapper(
					sysarg_t, sysarg_t, sysarg_t);
extern int			grio_associate_file_with_stream_wrapper(
					sysarg_t, sysarg_t, sysarg_t);
extern int			grio_dissociate_stream_wrapper(
					sysarg_t, sysarg_t);
extern int			grio_remove_stream_info_wrapper(
					sysarg_t);
extern int			grio_get_all_streams( 
					sysarg_t);
extern int			grio_remove_all_streams(
					void);
extern int 			grio_issue_grio_req(
					sysarg_t, sysarg_t );
extern int 			grio_read_num_cmds( 
					sysarg_t, sysarg_t );
extern int 			grio_get_mld_cnode( 
					sysarg_t, sysarg_t );
extern int 			grio_read_grio_req(
					sysarg_t );
extern int 			grio_resp_grio_req( 
					sysarg_t );
extern int			xfs_get_file_extents( 
					sysarg_t, sysarg_t, sysarg_t, sysarg_t);
extern int			xfs_get_file_rt(
					sysarg_t, sysarg_t);
extern int			xfs_get_block_size(
					sysarg_t, sysarg_t);
extern int			grio_get_stream_id_wrapper(
					sysarg_t, sysarg_t, sysarg_t);
extern int			grio_get_fp(
					sysarg_t, sysarg_t);
extern int 			grio_queue_bp_request(
					grio_stream_disk_info_t *, buf_t *);
extern int 			grio_determine_stream_priority(
					grio_stream_disk_info_t *, time_t);
extern int			grio_get_vod_disk_info( sysarg_t, sysarg_t );
extern void			grio_mark_bp_as_unreserved(
					buf_t *);
extern void			grio_add_nonguaranteed_stream_info(
					void );
extern buf_t			*grio_get_priority_bp( 
					grio_disk_info_t *, time_t);
extern buf_t			*grio_get_queued_bp_request(
					grio_stream_disk_info_t *);
extern grio_disk_info_t		*grio_get_disk_info( dev_t);
extern grio_stream_disk_info_t	*grio_get_stream_disk_info( buf_t *);
extern grio_stream_disk_info_t	*grio_get_info_from_bp( buf_t *);
extern int 			grio_vhdl_list(sysarg_t, sysarg_t, 
					sysarg_t, sysarg_t);

extern int			grio_monitor_start( sysarg_t );
extern int			grio_monitor_read( sysarg_t, sysarg_t );
extern int			grio_monitor_stop( sysarg_t );
extern void			grio_start_bpqueue(void);

/*
 * Local forward declarations.
 */
int  		grio_start_next_io( grio_disk_info_t *, time_t , int);
int 		grio_make_bp_list( grio_stream_disk_info_t *, buf_t *);


/*
 * grioinit
 *	Initialize global state for grio driver.
 *
 *
 * RETURNS:
 *	none
 */
void
grioinit()
{
        spinlock_init(&grio_global_lock, "grio_global_lock");
        spinlock_init(&grio_stream_table_lock, "grio_stream_table_lock");
	sv_init(&grio_q_sv, SV_DEFAULT, "grio_q_sv");

	grio_buf_data_zone = kmem_zone_init(sizeof(grio_buf_data_t),
				"grio_buf_data");
	grio_q = NULL;

	/*
 	 * Init NONGUARANTEED stream, uuid is NIL
 	 */
	grio_add_nonguaranteed_stream_info();

	grio_start_bpqueue();
	return;

}

/*
 * griostrategy()
 *	Strategy entry point for grio driver.
 *
 *
 * RETURNS:
 *	none
 */
void
griostrategy( buf_t *bp)
{
	int			s, ds;
	time_t			now;
	grio_disk_info_t	*griodp;
	grio_stream_info_t	*griosp;
	grio_stream_disk_info_t	*griosdp;
	struct bdevsw		*my_bdevsw;

	/*
	 * get grio disk info structure
	 */
	griodp  = grio_get_disk_info( bp->b_edev );
	if ( griodp ) {
		/*
		 * get grio stream disk info structure
		 */
		griosdp = grio_get_info_from_bp( bp );

		/*
		 * grio get stream info structure 
		 */
		griosp  = griosdp->thisstream;


		/*
	 	 * check if this is a non-scheduled stream,
	 	 * if so, issue the I/O immediately.
	 	 * 
	 	 * THIS IS VERY DANGEROUS FOR
	 	 * OTHER RATE GUARANTEED STREAMS.
	 	 */
		if ( NS_SCHED_STREAM( griosp ) ) {
			my_bdevsw = get_bdevsw(bp->b_edev);
			ASSERT(my_bdevsw != NULL);
			MARK_BUF_GRIO_ISSUED(bp);
#ifdef SIM
			_bdstrat( my_bdevsw, bp );
#else
			bdstrat( my_bdevsw, bp );
#endif
			CLR_BUF_GRIO_ISSUED(bp);
			return;
		}

		/* update statistics */
		griodp->ops_issued++;


		/*
	 	 * If the the stream is in the process of being 
		 * removed, mark the I/O as nonguaranteed.
	 	 *
		 * once an i/o has been queued, a stream will not be 
		 * removed until it is complete.
	 	 */
		s = GRIO_STREAM_LOCK( griosp );

/*
 * This opens a race condition. grio_mark_bp_as_unreserved()
 * modifies the bp->b_grio_private area that is shared by
 * all bps generated by XLV for this original bp. If some of these
 * bps have already been initiated on other disks, changing the 
 * stream id out from under them will cause the grio driver to hang.
 *
 * Another way to fix this problem is to modify the XLV code to
 * create different copies of the b_grio_private area for each bp
 * that it generates.
 *
 *		if ( STREAM_BEING_REMOVED( griosp ) ) {
 *
 *			GRIO_STREAM_UNLOCK( griosp, s );
 *			grio_mark_bp_as_unreserved( bp );
 *			griosdp = grio_get_stream_disk_info( bp );
 *			griosp  = griosdp->thisstream;
 *			s = GRIO_STREAM_LOCK( griosp );
 *
 *		}
 */
		MARK_STREAM_AS_INITIATING_IO(griosp);
		GRIO_STREAM_UNLOCK( griosp, s);

		if ( grio_make_bp_list( griosdp, bp ) ) {
			CLEAR_STREAM_AS_INITIATING_IO(griosp);
			bioerror( bp, EIO );
			grio_complete_real_bp( bp );
			return;
		}

		/*
		 * if there is already a bp outstanding on this disk
		 * for this stream, queue the request.
		 * otherwise, mark it as the active request for this disk
		 */
		ds = GRIO_DISK_LOCK( griodp );
                s = GRIO_STREAM_DISK_LOCK( griosdp);
		if ( griosdp->realbp ) {
			grio_queue_bp_request( griosdp, bp );
		} else {
			griosdp->realbp = bp;
			/*      
		 	 * the list of sub-bps attached to this real 
			 * bp is held in the b_grio_list field.
			 */
			griosdp->bp_list = bp->b_grio_list;
		}
               	GRIO_STREAM_DISK_UNLOCK( griosdp, s );

		grio_determine_stream_priority( griosdp, lbolt );

		GRIO_DISK_UNLOCK( griodp, ds );

		s = GRIO_STREAM_LOCK( griosp );
		CLEAR_STREAM_AS_INITIATING_IO(griosp);
		GRIO_STREAM_UNLOCK( griosp, s);

		/*
		 * start next i/o
		 */
		now = lbolt;
		grio_start_next_io( griodp, now, 1 );
	} else {
		my_bdevsw = get_bdevsw(bp->b_edev);
		ASSERT( ( bp->b_flags & B_GR_ISD ) == 0 );
		ASSERT( ( bp->b_flags & B_GR_BUF ) == 0 );
		/* B_PRV_BUF may be set */
#ifdef SIM
		_bdstrat( my_bdevsw, bp );
#else
		bdstrat( my_bdevsw, bp );
#endif
	}

	return;
}


void
griostrategy2( buf_t *bp)
{
	griostrategy(bp);
}


/*
 * grio_make_bp_page_req
 * 	If an I/O request is page mapped, then the subrequests need to
 *	point at the correct page. This routine locates the correct page
 *	for the given offest in the the real request, and associates
 *	the new request structure with this page.
 *
 * RETURNS:
 *	none
 */
void
grio_make_bp_page_req( buf_t *realbp, buf_t *nbp, int offsetcount)
{
        pfd_t                   *pfdp = NULL;

	if ( realbp->b_flags & B_PAGEIO )  {
		pfdp = getnextpg(realbp, pfdp);
		ASSERT( pfdp );
		/*
		 * advance to the correct page
		 */
		while ( offsetcount >= NBPP ) {
			pfdp = getnextpg(realbp, pfdp);
			ASSERT( pfdp != NULL );
			offsetcount -= NBPP;
		}
		nbp->b_pages = pfdp;
	} else {
		ASSERT(realbp->b_pages == NULL );
	}
}


/*
 * grio_make_bp_list
 *	Break the given I/O request into subrequests each of 
 *	some optimal I/O size for this device.
 *
 *
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
int
grio_make_bp_list( grio_stream_disk_info_t *griosdp, buf_t *realbp)
{
	int			count, count_this_bp, offsetcount;
	int			opt_io_size, bpcount, s;
        buf_t			*nbp, *nextbp, *bp_list;
	grio_disk_info_t	*griodp;
#ifdef IO_SIZE_CHECK
	grio_stream_info_t	*griosp	= griosdp->thisstream;
#endif

        /*
         * Get the optimal I/O size for the disk.
         */
	griodp		= griosdp->griodp;
        opt_io_size	= griodp->opt_io_size;
        count		= realbp->b_bcount;

#ifdef GRIO_DEBUG
	realbp->b_start	= lbolt;
#endif

	offsetcount	= 0;
	bpcount		= 0;
	bp_list		= NULL;

#ifdef IO_SIZE_CHECK
	/*
	 * If this is a ROTOR GRIO request, then 
	 * I/O size must be multiple of optimal I/O size of disk.
	 * 
	 * What about reaching EOF, and file is not a multiple of 
 	 * opt_io_size is length?
	 */
	if (	(BUF_IS_GRIO( realbp ) ) && 
		ROTOR_STREAM( griosp ) && 
		( count % opt_io_size ) ) {
		return( -1 );
	}
#endif

        /*
         * Initialize parent buffer.
         */
        realbp->b_resid      = 0;
        realbp->b_flags     &= ~(B_ERROR);

        /*
         * Create bps for opt_io_size sized requests.
         */
	ASSERT( BUF_GRIO_PRIVATE(realbp) );
        while ( count ) {

                nbp = getrbuf(KM_SLEEP);
                ASSERT(nbp);
		ASSERT( BUF_GRIO_PRIVATE(nbp) == NULL );

		nbp->b_grio_private = realbp->b_grio_private;

                /*
                 * Fill in static buffer info.
                 */
                nbp->b_error  = realbp->b_error;
                nbp->b_edev   = realbp->b_edev;
                nbp->b_iodone = grio_iodone;
                nbp->b_flags  = realbp->b_flags;

		if (count > opt_io_size) {
                	count_this_bp	= opt_io_size;
		} else {
                	count_this_bp	= count;
		}

                /*
                 * Fill in values specific to this buffer.
                 */
                nbp->b_un.b_addr  = realbp->b_un.b_addr + offsetcount;
                nbp->b_blkno      = realbp->b_blkno + BTOBB(offsetcount);
                nbp->b_offset     = realbp->b_offset + BTOBB(offsetcount);
               	nbp->b_bcount     = count_this_bp;

		/*
		 * Check if the buffer is PAGEIO
		 */
		grio_make_bp_page_req( realbp, nbp, offsetcount);

		/*
		 * Add the buffer to the stream info buffer list.
	 	 * The buffers are linked through the b_grio_list field.
	 	 */
		if ( bp_list ) {
			nextbp->b_grio_list = nbp;
			nextbp = nbp;
		} else {
			bp_list = nextbp = nbp;
		}
		nextbp->b_grio_list = NULL;

                offsetcount 	+= count_this_bp;
		count 		-= count_this_bp;
		bpcount++;
        }

        ASSERT( offsetcount == realbp->b_bcount );
	ASSERT( count == 0 );

	s = GRIO_STREAM_DISK_LOCK( griosdp );

	griodp->subops_issued += bpcount;

	GRIO_STREAM_DISK_UNLOCK( griosdp, s );

	/*
	 * The real bp buf structure points at the list of
	 * sub-bps through the b_grio_list pointer.
	 */
	realbp->b_grio_list = bp_list;


        return( 0 );
}


/*
 * grio_iodone
 *	Iodone entry point for grio driver.
 *	Determine if this is the last sub-bp request for this real bp.
 *	If so, call iodone() on the real bp.
 *	Start next I/O.
 *
 * RETURNS:
 *	none
 */
void
grio_iodone(buf_t *bp) 
{
	int			ds, sds;
#ifdef GRIO_DEBUG
	int			optime;
#endif
	buf_t			*realbp, *prev_bp, *tmp_bp;
	time_t			now;
	grio_disk_info_t	*griodp;
	grio_stream_info_t 	*griosp;
	grio_stream_disk_info_t	*griosdp;

	/*
	 * check if the grio driver issued this request.
	 */
	if ( BUF_IS_GRIO_ISSUED( bp ) ) {

		/*
	 	 * get grio_stream_info for this bp
	 	 */
		griodp  = grio_get_disk_info( bp->b_edev );

#ifdef LATER
#ifdef GRIO_DEBUG
		optime = 2*((HZ/griodp->num_iops_max) + 1);
		if (	(lbolt - griodp->time_start ) >  optime ) {
			printf("GRIO took %d ticks instead of %d: (dks%dd%d)\n",
				lbolt - griodp->time_start,
				optime/2, 
				SCSI_EXT_CTLR(DKSC_CTLR(griodp->diskdev)),
				DKSC_UNIT( griodp->diskdev ) );
		}
#endif
#endif

		ASSERT( griodp );
		griosdp = grio_get_stream_disk_info( bp );
		ASSERT( griosdp );
		griosp  = griosdp->thisstream;

		ASSERT( griosp );
		ASSERT( griodp->active != 0 );
		ASSERT( griosdp->issued_bp_list );

		ds = GRIO_DISK_LOCK( griodp );
		sds = GRIO_STREAM_DISK_LOCK( griosdp );

		griodp->active --;


		/* Walk issued_bp_list and remove bp */
		prev_bp = tmp_bp = griosdp->issued_bp_list;
		while (tmp_bp != bp) {
			prev_bp = tmp_bp;
			tmp_bp = tmp_bp->b_grio_list;
		}

		if ( griosdp->issued_bp_list == tmp_bp ) {
			griosdp->issued_bp_list = tmp_bp->b_grio_list;
		} else {
			prev_bp->b_grio_list = tmp_bp->b_grio_list;
		}
		tmp_bp->b_grio_list = NULL;


		griodp->subops_complete++;

		/*
		 * real I/O is complete - call iodone		
		 */
		if ( (griosdp->issued_bp_list == NULL) && (griosdp->bp_list == NULL) )  {
			realbp = griosdp->realbp;
			griosdp->realbp = grio_get_queued_bp_request( griosdp );


			if ( ROTOR_STREAM( griosp ) ) {
				ASSERT( griosdp->realbp == NULL );
			}

			if ( griosdp->realbp ) {
				ASSERT(! BUF_IS_GRIO_ISSUED(griosdp->realbp) );
				griosdp->bp_list = griosdp->realbp->b_grio_list;
			}
		} else {
			realbp = NULL;
		}

		GRIO_STREAM_DISK_UNLOCK( griosdp, sds );
		GRIO_DISK_UNLOCK( griodp, ds );

		now = lbolt;
		grio_start_next_io( griodp, now, 0 );

		/*
		 * Free the bp
		 */
		CLR_BUF_GRIO_ISSUED( bp );
		grio_free_bp( bp );

		if ( realbp ) {
			griodp->ops_complete++;
			grio_complete_real_bp( realbp );
		}


	} else {
		ASSERT( 0 );
	}
}

/*
 * grio_start_next_rtdisk_io 
 *	Determine the I/O operation associated with this RT device that
 * 	has the highest priority and issue it to the disk driver.
 * 
 * RETURNS:
 *	1 if i/o was started
 *	0 if i/o was not started
 */
int
grio_start_next_rtdisk_io( grio_disk_info_t *griodp, time_t now, int sleepable )
{
	int		s;
	buf_t		*nextbp;
	struct bdevsw	*my_bdevsw;

	s = GRIO_DISK_LOCK( griodp );
	if (!griodp->active) {
		griodp->active = 1;

		/*
		 * grio_get_priority_bp() assumes DISK is locked
		 */
		nextbp = grio_get_priority_bp( griodp, now );

		if ( nextbp ) {

			MARK_BUF_GRIO_ISSUED( nextbp );

#ifdef GRIO_DEBUG
			griodp->time_start = lbolt;
#endif

			ASSERT( griodp->active == 1 );
			ASSERT( nextbp->b_flags & B_GR_ISD );

			GRIO_DISK_UNLOCK( griodp, s );
			if ( sleepable ) {
				my_bdevsw = get_bdevsw(nextbp->b_edev);
				ASSERT(my_bdevsw != NULL);
#ifdef SIM
				_bdstrat( my_bdevsw, nextbp );
#else
				bdstrat( my_bdevsw, nextbp );
#endif
			} else {

				bp_enqueue( nextbp, &grio_subreq_bpqueue);

			}
			return( 1 );
		} else {
			griodp->active = 0;
		}
	}
	GRIO_DISK_UNLOCK( griodp, s );
	return( 0 );
}

/*
 * grio_start_next_nonrtdisk_io 
 *	Determine the I/O operation(s) associated with this nonRT device that
 * 	has/have the highest priority and issue it/them to the disk driver.
 * 
 * RETURNS:
 *	>0 if i/o(s) was/were started
 *	0 if i/o was not started
 */
int
grio_start_next_nonrtdisk_io( grio_disk_info_t *griodp, time_t now, int sleepable )
{
	int		s;
	buf_t		*nextbp;
	struct bdevsw	*my_bdevsw;
	int		ios_issued = 0;

	s = GRIO_DISK_LOCK( griodp );

	/*
	 * grio_get_priority_bp() assumes DISK is locked
	 */

	while ( (nextbp = grio_get_priority_bp( griodp, now )) ) {

		griodp->active ++;
		ios_issued ++;

		MARK_BUF_GRIO_ISSUED( nextbp );

		ASSERT( griodp->active != 0 );
		ASSERT( nextbp->b_flags & B_GR_ISD );

		GRIO_DISK_UNLOCK( griodp, s );
		if ( sleepable ) {
			my_bdevsw = get_bdevsw(nextbp->b_edev);
			ASSERT(my_bdevsw != NULL);
#ifdef SIM
			_bdstrat( my_bdevsw, nextbp );
#else
			bdstrat( my_bdevsw, nextbp );
#endif
		} else {

			bp_enqueue( nextbp, &grio_subreq_bpqueue);

		}

		s = GRIO_DISK_LOCK( griodp ); /* due to grio_get_priority_bp
					       * assumption.
					       */
	}

	GRIO_DISK_UNLOCK( griodp, s );
	return( ios_issued );
}

/*
 * grio_start_next_io 
 *	Determine the I/O operation(s) associated with this device that
 * 	has/have the highest priority and issue it/them to the disk driver.
 *	Treat RT and nonRT disks appropriately.
 * 
 * RETURNS:
 *	1 if i/o was started
 *	0 if i/o was not started
 */
int
grio_start_next_io( grio_disk_info_t *griodp, time_t now, int sleepable )
{
	if (griodp->realtime_disk)
		return (grio_start_next_rtdisk_io(griodp, now, sleepable));
	else
		return (grio_start_next_nonrtdisk_io(griodp, now, sleepable));
}

/*
 * grio_config()
 *      This routine is called from the syssgi() system call
 *      when the SGI_GRIO arguement is passed. It executes a
 *      variety of commands to support rate guaranteed i/o requests.
 *
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
int
grio_config(
        sysarg_t cmd,
        sysarg_t arg1,
        sysarg_t arg2,
        sysarg_t arg3,
        sysarg_t arg4)
{
	int     ret = 0;

    	if (!_CAP_ABLE(CAP_DEVICE_MGT))
		return EPERM;

	switch (cmd) {
	/*
	 * arg1 = devt          	- devt of disk device
	 * arg2 = num_ios_rsv   	- num of reserved I/O ops
	 * arg3 = num_ios_max   	- max num of I/O ops for devt
	 * arg4 = opt_blk_sz    	- size of opt I/O for devt
	 */
		case GRIO_UPDATE_DISK_INFO:
			arg3 = 0;
			arg4 = 0;
		case GRIO_ADD_DISK_INFO:
			ret = grio_add_disk_wrapper( arg1 );
			break;
	/*
	 * arg1 = flags
	 * arg2 = file_id
	 * arg3 = stream_id_ptr
	 * arge = vod_info
	 */
		case GRIO_ADD_STREAM_INFO:
			ret = grio_add_stream_wrapper( arg1, arg2, arg3, arg4 );
			break;
	/*
	 * arg1 = proc_id 		- process_id
	 * arg2 = file_id		- file_id to complete num_iops i/os
	 * arg3 = stream_id_ptr		- ptr to id for stream
	 */
		case GRIO_ASSOCIATE_FILE_WITH_STREAM:
			ret = grio_associate_file_with_stream_wrapper(
					arg1, arg2, arg3);
			break;

	/*
	 * arg1 = proc_id		- process id
	 * arg2 = stream_id_ptr		- ptr to id for stream
	 */
		case GRIO_DISSOCIATE_STREAM:
			ret = grio_dissociate_stream_wrapper(
					arg1, arg2);
			break;

	/*
	 * arg1 = fs_dev
	 * arg2 = inum
	 * arg3 = extents[]		- array of extents
	 * arg4 = &numexternts		- number of extents
	 */
		case GRIO_GET_FILE_EXTENTS:
			ret = xfs_get_file_extents(arg1, arg2, arg3, arg4);
			break;

	/*
	 * arg1 = drv_vhdl	- the disk from which grio happens
	 * arg2 = *list_head	- memory into which to return path
	 * arg3 = max_items	- the max num of grio_dev_info_t strs taht
					can fit in the memory allocated
	 * arg4 = *num_items	- number of items in path returned
	 */
	
		case GRIO_GET_HWGRAPH_PATH:
			ret = grio_vhdl_list(arg1, arg2, arg3, arg4);
			break;

	/*
	 * arg1 = gdev			- disk device
	 * arg2 = disk_id_ptr
	 * arg3	= stream_id_ptr		- identifying uuid
 	 */
		case GRIO_ADD_STREAM_DISK_INFO:
			ret = grio_add_stream_disk_wrapper( 
					arg1, arg2, arg3 );
			break;
	/*
	 * arg1 = *file_id
	 * arg2 = &rt
	 */
		case GRIO_GET_FILE_RT:
			ret = xfs_get_file_rt( arg1, arg2);
			break;

	/*
	 * arg1 = proc_id 		- process_id
	 * arg2 = file_id		- file_id to complete num_iops i/os
	 * arg3 = uuid_ptr		- uuid for stream
 	 */
		case GRIO_GET_STREAM_ID:
			ret = grio_get_stream_id_wrapper( arg1, arg2, arg3 );
			break;


	/*
	 * arg1 = vdev
	 * arg2 = &block_size
	 */
		case GRIO_GET_FS_BLOCK_SIZE:
			ret = xfs_get_block_size( arg1, arg2);
			break;

	/*
	 * arg1 = uuid_ptr		- pointer to uuid for grio stream
	 */
		case GRIO_REMOVE_STREAM_INFO:
			ret = grio_remove_stream_info_wrapper( arg1 );
			break;
	/*
	 * arg1 = number of commands to be queued up.
	 * arg2 = user address pointer to the grio_cmd_t structure list.
	 */
		case GRIO_WRITE_GRIO_REQ:
			ret = grio_issue_grio_req( arg1, arg2 );
			break;

	/*
	 * arg1 = user address pointer for num of cmds in next queue element.
	 * arg2 = grio cmd structure specifying wait time.
	 */
		case GRIO_READ_NUM_CMDS:
			ret = grio_read_num_cmds( arg1, arg2 );
			break;

	/*
	 * arg1 = user address pointer for the MLD. 
	 * arg2 = user address pointer for returned cnode-id on which MLD 
			resides 
	 */
		case GRIO_GET_MLD_CNODE:
			ret = grio_get_mld_cnode( arg1, arg2 );
			break;

	/*
	 * arg1 = file descriptor.
	 * arg2 = user address pointer for returned fp.
	 */
		case GRIO_GET_FP:
			ret = grio_get_fp( arg1, arg2 );
			break;

	/*
	 * arg1 = user address pointer to a grio_cmd_t structure.
	 */
		case GRIO_READ_GRIO_REQ:
			ret = grio_read_grio_req( arg1 );
			break;

	/*
	 * arg1 = user address pointer to a grio_cmd_t structure
	 */
		case GRIO_WRITE_GRIO_RESP:
			ret = grio_resp_grio_req(arg1);
			break;

	/*
	 *  no arguments
	 *
	 */
		case GRIO_REMOVE_ALL_STREAMS_INFO:
			ret = grio_remove_all_streams();
			break;

		case GRIO_GET_ALL_STREAMS:
			ret = grio_get_all_streams( arg1 );
			break;

		case GRIO_GET_VOD_DISK_INFO:
			ret = grio_get_vod_disk_info( arg1, arg2 );
			break;

		case GRIO_MONITOR_START:
			ret = grio_monitor_start( arg1 );
			break;

		case GRIO_MONITOR_END:
			ret = grio_monitor_stop( arg1 );
			break;

		case GRIO_MONITOR_GET:
			ret = grio_monitor_read( arg1, arg2 );
			break;

		default:
			ret = -1;
	}

	dbg3printf(("grio_config: return value %d \n",ret));

	return( ret ) ;
}


/*
 * grio_io_is_guaranteed()
 *
 * RETURNS:
 *	0 if there is no guarantee
 *	non-zero if there is a guarantee
 */
int
grio_io_is_guaranteed( vfile_t *fp, stream_id_t *stream_id)
{
	extern grio_stream_info_t *grio_find_associated_stream_with_fp(vfile_t *);
	grio_stream_info_t	*griosp;

	griosp = grio_find_associated_stream_with_fp(fp);
	if (griosp) {
		COPY_STREAM_ID( griosp->stream_id, (*stream_id) );
		return (1);
	} else {
		return (0);
	}
}

