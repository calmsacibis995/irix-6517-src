#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/vdisk.c,v 1.84 1997/05/15 23:57:23 doucette Exp $"
/*
 * vdisk.c
 *
 *	This file contains the routines that manipulate the virtual
 *	disk data structures.
 *
 *	It allocates and manages vdisk bandwidth reservations.
 *
 *
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <invent.h>
#include <grio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/bsd_types.h>
#include <sys/time.h>
#include <sys/cred.h>
#include <sys/scsi.h>
#include <sys/uuid.h>
#include <sys/ktime.h>
#include <sys/syssgi.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/fs/xfs_inum.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <sys/dkio.h>
#include <sys/capability.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

extern xlv_tab_subvol_t *get_subvol_space(void);
extern xlv_tab_subvol_t *get_subvol_info(dev_t );

/*
 * Definitions for commonly used values.
 *
 * The num extents value needs to be kept in sync with XFS.
 */
#define	NUM_EXTENTS_PER_FILE	32768
#define DISK_PARTS 		XLV_MAX_DISK_PARTS_PER_VE
#define INITIAL_MAXIMUM		10000

/*
 * Determine if part of the given extent resides on the given volume.
 */
#define EXTENT_ELEMENT_OVERLAP( extentp, volep)  			\
	((( extentp->br_startblock + extentp->br_blockcount) >= 	\
		volep->start_block_no ) && 				\
		(extentp->br_startblock <= volep->end_block_no ))

/*
 * Local function prototypes
 */
STATIC dev_t get_vdisk_subvol(dev_t, int );
STATIC int get_file_system_block_size(dev_t );
STATIC int is_file_realtime( dev_t, inode_num_t);
STATIC int free_extents_for_file(grio_bmbt_irec_t *);
#if 0
STATIC void dump_extent_info(grio_bmbt_irec_t *, int );
#endif
STATIC grio_bmbt_irec_t *get_extents_for_file(dev_t,inode_num_t, int * );
int file_in_element(xlv_tab_vol_elmnt_t *,grio_bmbt_irec_t *,int, __uint64_t);
STATIC int add_new_striped_disks_to_list(grio_disk_list_t [],int *,int,
	xlv_tab_vol_elmnt_t *, int , int, int );
int time_is_valid( grio_resv_t *);

int get_all_disks_in_vdisk( dev_t, int, grio_disk_list_t [], int *);

extern int	check_xlv_device(dev_t);





/*
 * vdisk_make_reservations
 *	Call the reserve_path_bandwidth() routine to make the 
 *	bandwidth reservations on each disk.
 *	If this is a NON VOD reservation, 
 * 		then the reservations on
 *		each disk must be successful.
 *	If this is a VOD reseration, 
 *		then only one of the disk
 *		reservations needs to be successful. The os will have
 *		the resposibility of scheduling the the I/Os to the correct
 *		disk at the correct time.
 *
 * RETURN:
 *	the number of reservations successfully made
 *
 */
int
vdisk_make_reservations(
	grio_resv_t		*griorp,
	grio_disk_list_t	disks[],
	dev_t			fs_dev,
	stream_id_t		*gr_stream_id,
	int			diskcount,
	int			*maxreqsize,
	time_t			*maxreqtime)
{
	int		i, status, reservedcount, perdiskreqsize;
	int		numbuckets;
	char		err_dev[DEV_NMLEN];
	dev_t		diskdev;
	time_t		start, duration, perdiskreqtime;

	/*
	 * get the rate request start time and duration.
	 */
	status     	= reservedcount = 0;
	start 	   	= (time_t)griorp->gr_start;
	duration   	= (time_t)griorp->gr_duration;

	/*
  	 * attempt to make the bandwidth reservations 
 	 * on each of the necessary physical disks.
 	 */
	for (i = 0; i < diskcount; i++) {
		
		diskdev		= disks[i].physdev;
		perdiskreqsize 	= disks[i].reqsize;
		perdiskreqtime 	= disks[i].optime;
		numbuckets	= disks[i].numbuckets;
	
		/*
		 * try to reserve the system bandwidth.
		 */
		status = reserve_path_bandwidth(diskdev, fs_dev,
				 perdiskreqsize, perdiskreqtime,
				 numbuckets, start, duration, gr_stream_id,
				 maxreqsize, maxreqtime, err_dev,
				 griorp->gr_flags, griorp->gr_memloc);
			
		if (status) {

			/*
		  	 * not enough bandwidth was available,
		  	 * available bandwidth is in maxreqsize 
			 * and maxreqtime
		  	 */

			griorp->gr_error = status;
			strcpy(griorp->gr_errordev, err_dev);
			break;
		} else {

			/*
			 * bandwidth was available, reservation was made
			 */
			reservedcount++;
		} 

	}
	return( reservedcount );
}

/*
 * generic_reservation()
 *	This performs generic reservation on a per file system basis.
 *
 *
 * RETURNS:
 *	0 on success
 * 	non-zero on failure
 */
int
generic_reservation(
	dev_t			vdev,
	grio_cmd_t		*griocp,
	xlv_tab_subvol_t 	*subvol,
	stream_id_t		*gr_stream_id,
	int			*maxreqsize,
	time_t			*maxreqtime,
	int			rt)
{
	int			i, j, reservedcount, iosize;
	int			totalreqsize, stripesize, alignedsize; 
	int			numextents, diskcount;
	__uint64_t		fsbs;
	time_t			reqtime, perdiskreqsize;
	grio_resv_t		*griorp;
	xlv_tab_plex_t		*plex;
	grio_bmbt_irec_t	*extents;
	grio_disk_list_t	disks[MAX_NUM_DISKS];
	xlv_tab_vol_elmnt_t	*volep;


	dbgprintf(8, ("generic_reservation \n"));

	griorp 		= GRIO_GET_RESV_DATA( griocp );
	iosize	     	= get_grio_iosize_for_filesys( vdev, rt );
	diskcount	= numextents = 0;

	if (iosize <= 0) {
		griorp->gr_error = EACCES;
		return(griorp->gr_error);
	}

	totalreqsize 	= ROUND_UP( griorp->gr_opsize, iosize); 

	if ( totalreqsize == 0 )  {
		return( griorp->gr_error = EIO );
	}

	if ( griorp->gr_optime < 0 ) {
		return( griorp->gr_error = EIO );
	}

	/*
 	 * convert duration from useconds to ticks 
	 */
	reqtime 	= griorp->gr_optime / USEC_PER_TICK;

	if ( reqtime <= 1 )
		reqtime = 1;

	/*
	 * if this a per file guarantee, get extent info.
 	 */
	if ( IS_FILE_GUAR( griorp) ) {
		extents = get_extents_for_file(vdev,griocp->one_end.gr_ino,
							&numextents);
		fsbs = get_file_system_block_size( vdev );
	}
	
	/*
	 * when getting a reservation on a per file system basis, it is
	 * assumed that the reservation will eventually be used on a file
 	 * that is striped across the entire subvolume
	 *
	 * look at each subvolume of each plex in this volume.
	 */
/*
 * WHAT IF num_plexes > 1 ???? 
 */
	for (i = 0; i < subvol->num_plexes; i++ ) {
		plex = subvol->plex[i];

		/*
	 	 * for each subvolume element in this plex.
		 */
		for ( j = 0; j < (int)(plex->num_vol_elmnts); j++ ) {
			volep = &(plex->vol_elmnts[j]);
		
			stripesize  = GET_VOL_STRIPE_SIZE(volep);
			alignedsize = ROUND_UP(totalreqsize,stripesize);

			if ( ( !IS_FILE_GUAR( griorp) ) ||
			     (file_in_element(volep,extents,numextents,fsbs))) {

				if ( volep->type == XLV_VE_TYPE_STRIPE )  {
					perdiskreqsize = alignedsize / 
							volep->grp_size;
				} else {
					perdiskreqsize = totalreqsize;
				}

				griorp->gr_error = 
					add_new_striped_disks_to_list( 
						disks, &diskcount, 
						MAX_NUM_DISKS, volep, 
						perdiskreqsize, reqtime, 
						volep->grp_size);
	
				if ( griorp->gr_error ) {

					if ( IS_FILE_GUAR( griorp ) ) {
						free_extents_for_file(extents);
					}

					return( griorp->gr_error );
				}
			}
		}
	}

	/*
	 * check that disks were added to the list
 	 */
	if ( diskcount == 0 ) {
		dbgprintf(1,("generic_reservation - no disks \n") );
		griorp->gr_error = ENOENT;
		if ( IS_FILE_GUAR( griorp ) ) {
			free_extents_for_file(extents);
		}
		return( griorp->gr_error );
	}

	/*
 	 * make the reservations on the disks
	 */
	reservedcount = vdisk_make_reservations( griorp, disks, vdev, gr_stream_id,
			diskcount, maxreqsize, maxreqtime);

	/*
	 * if reservations could not be obtained on all the disks,
	 * free the reservations on those disks where they were 
	 * obtained and return an error
	 */
	if ( reservedcount != diskcount ) {
		dbgprintf(1,("generic_reservation - reserve failed\n"));
		for (i = 0; i < reservedcount; i++) {
			unreserve_path_bandwidth(disks[i].physdev, 
				   gr_stream_id, griorp->gr_memloc);
		}
	}

	if ( IS_FILE_GUAR( griorp ) ) {
		free_extents_for_file(extents);
	}

	return( griorp->gr_error );
}

/*
 * vod_reservation
 *
 *	
 *
 *
 */
int
vod_reservation(
	dev_t			vdev,
	grio_cmd_t		*griocp,
	xlv_tab_subvol_t 	*subvol,
	stream_id_t		*gr_stream_id,
	int			*maxreqsize,
	time_t			*maxreqtime,
	int			*total_slots,
	int			*max_count_per_slot,
	int			rt)
{
	int			i, j, reservedcount, iosize, diskcount; 
	int			numextents;
	__uint64_t		fsbs, stripe_size;
	time_t			reqtime, perdiskreqtime;
	grio_resv_t		*griorp;
	device_node_t		*dnp;
	xlv_tab_plex_t		*plex;
	grio_disk_list_t	disks[MAX_NUM_DISKS];
	grio_bmbt_irec_t	*extents;
	xlv_tab_vol_elmnt_t	*volep;

	
	dbgprintf(8, ("vod_reservation \n"));

	griorp 			= GRIO_GET_RESV_DATA( griocp );
	iosize	     		= get_grio_iosize_for_filesys( vdev, rt );
	*total_slots		= 0;

	if (iosize <= 0) {
		griorp->gr_error = EACCES;
		return(griorp->gr_error);
	}

	if ( griorp->gr_opsize % iosize ) {
		dbgprintf(1,("vod_reservation - bad iosize \n") );
		griorp->gr_error = EPERM;
		return( EPERM );
	}

	iosize = griorp->gr_opsize;


	/*
 	 * convert duration from useconds to ticks 
	 */
	reqtime 	= griorp->gr_optime / USEC_PER_TICK;
	diskcount	= 0;
	
	/*
	 * if this a per file guarantee, get extent info.
 	 */
	if ( IS_FILE_GUAR( griorp) ) {
		extents = get_extents_for_file(vdev,griocp->one_end.gr_ino,
							&numextents);
		fsbs = get_file_system_block_size( vdev );
	}

	/*
	 * when getting a reservation on a per file system basis, it is
	 * assumed that the reservation will eventually be used on a file
 	 * that is stripped across the entire subvolume
	 *
	 * look at each subvolume of each plex in this volume.
	 */
	for (i = 0; i < subvol->num_plexes; i++ ) {
		plex = subvol->plex[i];

		/*
	 	 * for each subvolume element in this plex.
		 */
		if ( ((int)(plex->num_vol_elmnts)) != 1 ) {
			dbgprintf(1,("vod - multi-elemt fs\n"));
		}

		for ( j = 0; j < (int)(plex->num_vol_elmnts); j++ ) {
			volep = &(plex->vol_elmnts[j]);

			/*		
			 * VOD type reservations may only be made on
			 * a striped file system
			 */
			if ( volep->type == XLV_VE_TYPE_STRIPE )  {
				stripe_size = volep->stripe_unit_size*NBPSCTR;

				if ( stripe_size % iosize) {
					dbgprintf(1,("vod - bad stripe sz\n"));

					if ( IS_FILE_GUAR( griorp ) ) {
						free_extents_for_file(extents);
					}
					return( (griorp->gr_error = EIO) );
				}
			} else {
				dbgprintf(1,("vod - nonstriped fs\n"));

				if ( IS_FILE_GUAR( griorp ) ) {
					free_extents_for_file(extents);
				}
				return( (griorp->gr_error = EIO) );
			}

			if ( (*total_slots) &&
			     (*total_slots != volep->grp_size) ) {
				dbgprintf(1,("vod - uneven ve \n"));

				if ( IS_FILE_GUAR( griorp ) ) {
					free_extents_for_file(extents);
				}
				return( (griorp->gr_error = EIO) );
			}

			(*total_slots) = volep->grp_size;
			perdiskreqtime = reqtime;

			if ( ( !IS_FILE_GUAR( griorp) ) ||
			     (file_in_element(volep,extents,numextents,fsbs))) {

				griorp->gr_error = 
					add_new_striped_disks_to_list( 
						disks, &diskcount, 
						MAX_NUM_DISKS, volep, 
						iosize, perdiskreqtime,
						volep->grp_size);
	
				if ( griorp->gr_error ) {
					if ( IS_FILE_GUAR( griorp ) ) {
						free_extents_for_file(extents);
					}
					return( griorp->gr_error );
				}
			}
		}
	}

	/*
	 * check that disks were added to the list
 	 */
	if ( diskcount == 0 ) {
		dbgprintf(1,("vod_reservation - no disks \n") );
		if ( IS_FILE_GUAR( griorp ) ) {
			free_extents_for_file(extents);
		}
		griorp->gr_error = ENOENT;
		return( griorp->gr_error  );
	}

	/*
	 * determine max number of optios that 
	 * a single disk will support
 	 * 
 	 * it is NOT assumed that all the disks in the stripe
	 * are of the same type. Must find the disk with the minimum
	 * number of slots
	 */
	for (i = 0; i < diskcount; i++ ) {
		dnp = devinfo_from_cache(disks[i].physdev);
		assert( dnp );
		
		if ( 	( i == 0 ) || 
			(*max_count_per_slot) > dnp->resv_info.maxioops ) {

			(*max_count_per_slot) = dnp->resv_info.maxioops;
		}
	}

	/*
 	 * make the reservations on the disks
	 */
	reservedcount = vdisk_make_reservations( griorp, disks, vdev, gr_stream_id,
			diskcount, maxreqsize, maxreqtime);

	/*
	 * if reservations could not be obtained on all the disks,
	 * free the reservations on those disks where they were 
	 * obtained and return an error
	 */
	if ( reservedcount != diskcount ) {
		dbgprintf(1,("vod_reservation - reserve failed\n"));
		for (i = 0; i < reservedcount; i++) {
			unreserve_path_bandwidth(disks[i].physdev, 
				   gr_stream_id, griorp->gr_memloc);
		}
	}

	if ( IS_FILE_GUAR( griorp ) ) {
		free_extents_for_file(extents);
	}
	return( griorp->gr_error );
}

/*
 * reserve_vdsk_bandwith()
 *   
 *	This routine reserves the bandwidth on the various components   
 *	that make up the virtual disk. If bandwidth cannot be reserved on
 *   	each of the components, the previous reservations are cancelled.
 *   
 * RETURN:
 *	0 if reservation was successful
 *	non-zero on error
 */
int 
reserve_vdisk_bandwidth(
	grio_cmd_t		*griocp, 
	stream_id_t		*gr_stream_id,
	int			*maxreqsize,
	time_t			*maxreqtime,
	int			*total_slots,
	int			*max_count_per_slot)
{
	int			status;
	dev_t			vdev, subvol_dev;
	grio_resv_t		*griorp;
	xlv_tab_subvol_t	*subvol;
	int			rt;

	griorp  = GRIO_GET_RESV_DATA( griocp );
	vdev	= griocp->one_end.gr_dev;

	/*
	 * check that gr_start and gr_duration are valid
	 */
	if ( !time_is_valid( griorp ) ) {
		griorp->gr_error = EIO;
		return( griorp->gr_error );
	}

	/*
 	 * get the subvolume for the virtual disk
 	 */
	if ( IS_FILE_GUAR( griorp ) ) {
		if ( is_file_realtime( vdev, griocp->one_end.gr_ino ) ) {
			rt = 1;
			subvol_dev = get_vdisk_subvol(vdev, rt);
		} else {
			rt = 0;
			subvol_dev = get_vdisk_subvol(vdev, rt);
		}
	} else {
		/*
		 * Initially assume request for RT subvol.
		 */
		rt = 1 ;
		subvol_dev = get_vdisk_subvol(vdev, rt);
		/*
		 * If that fails, try the data subvol.
		 */
		if (subvol_dev <= 0) {
			rt = 0;
			subvol_dev = get_vdisk_subvol(vdev, rt);
		}
	}

	if ( subvol_dev <= 0) {
		griorp->gr_error = EBADF;
		return( griorp->gr_error );
	}

	/*
 	 * get the information about the elements in the subvolume.
 	 */
	if ( (subvol = get_subvol_info(subvol_dev)) == NULL ) {
		dbgprintf(1,("could not get subvolume info \n"));
		griorp->gr_error = ENXIO;
		return( griorp->gr_error );
	}

	/*
	 * Call the correct reservation routine.
	 */
	dbgprintf(8,("ggd: guarantee_flags are %x \n",griorp->gr_flags));

	if ( IS_VOD_GUAR( griorp ) ) {
		status = vod_reservation(vdev, griocp, 
				subvol, gr_stream_id, maxreqsize, 
				maxreqtime,total_slots, max_count_per_slot, rt);
	} else {
		status = generic_reservation(vdev, griocp,
				subvol, gr_stream_id, maxreqsize, maxreqtime, rt);
	}

	assert( status == griorp->gr_error );

	return ( griorp->gr_error );

}

/*
 * unreserve_vdisk_bandwith()
 *   
 *	This routine removes the bandwidth reservations on the various 
 *	components that make up the virtual disk. 
 *   
 * RETURN:
 *	0 if reservation was successful
 *	non-zero if reservation was unsuccessful
 */
int 
unreserve_vdisk_bandwidth(
	dev_t vdev, 
	grio_cmd_t *griocp,
	int	*maxreqsize,
	int	*maxreqtime)
{
	__int64_t		lmaxreqsize, lmaxreqtime;
	int			i, diskcount = MAX_NUM_DISKS;
	dev_t			physdev, subvol_dev;
	stream_id_t		*gr_stream_id;
	device_node_t		*dnp;
	grio_disk_list_t 	disks[MAX_NUM_DISKS];
	grio_resv_t		*griorp;
	int			rt;


	griorp 		= GRIO_GET_RESV_DATA( griocp );

	if ( griocp->one_end.gr_ino == 0 ) {
		/* ino == 0 implies filesystem resv. Default to rt = 1 */
		rt = 1;
		/*
	 	 * get subvolume
	 	 */
		subvol_dev = get_vdisk_subvol( vdev, rt );
		if (subvol_dev <= 0) {
			rt = 0;
			subvol_dev = get_vdisk_subvol( vdev, rt );
		}
	} else {
		if ( is_file_realtime( vdev, griocp->one_end.gr_ino ) ) {
			rt = 1;
		} else {
			rt = 0;
		}
		/*
	 	 * get subvolume
	 	 */
		 subvol_dev = get_vdisk_subvol( vdev, rt );
	}


	/*
	 * Create a list of all the physical disks in this
	 * plex. It is not necessary to determine if the file
	 * actually exists on the given disk, we will remove the
	 * disk bandwidth anyway.
	 */
	if ( get_all_disks_in_vdisk( subvol_dev, rt, disks, &diskcount) ) {
		dbgprintf(1,("unreserve: failed - could not get disks\n"));
		return( EIO );
	}

	/*
 	 * Remove the reservations on the physical disks.
 	 */
	gr_stream_id = &(GRIO_GET_RESV_DATA(griocp)->gr_stream_id);

	for (i = 0; i < diskcount; i++) {
		physdev   = disks[i].diskdev;

		/*
		 * Do not check the return value of the unreserve
		 * call. It could legitimately fail because the file
		 * does not really exist on the disk. For example
		 * if it is a concatenated virtual disk.
		 */
		unreserve_path_bandwidth(physdev, gr_stream_id, 
				griorp->gr_memloc);
	}
	assert( diskcount );

	/*
	 * get bandwidth on a given disk dev 
 	 */
	dnp = devinfo_from_cache(physdev);
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

/*
 * file_in_element
 *	This routine returns true if any of the extents of this file
 *	are found in this plex element.
 *
 * RETURNS:
 *	1 if part of file is in volep
 * 	0 if not
 */
int
file_in_element( 
	xlv_tab_vol_elmnt_t	*volep, 
	grio_bmbt_irec_t 	*extentarray, 
	int 			numextents,
	__uint64_t 		fsbs)
{
	int			i;
	__uint64_t		sb, eb;
	grio_bmbt_irec_t	*extentp;

	/*
	 * For each extent in this file,
	 * check if the file blocks are contained on this volume element.
	 */
	for (i = 0 ; i < numextents; i++ ) {
		extentp = &extentarray[i];

		/*
		 * convert start and end extent file 
		 * system blocks to disk sectors
		 */
		sb = extentp->br_startblock * fsbs;
		sb = sb/((__uint64_t)NBPSCTR);

		eb = (extentp->br_startblock + extentp->br_blockcount) * fsbs;
		eb = eb/((__uint64_t)NBPSCTR);

		/*
 		 * Check if this extent has blocks on this element.
		 */
		if (   (eb >= (__uint64_t)(volep->start_block_no)) && 
		       (sb <= (__uint64_t)(volep->end_block_no))) {
				return( 1 );
		}
	}

	return( 0 );
}

/*
 * issue_xlv_ioctl()
 *	Generic interface to issue ioctls to virtual disk device node.
 *
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
int
issue_xlv_ioctl( dev_t vdev, int cmd, char *buffer)
{
	int fd, ret;
	
	if ( (fd = get_vdisk_fd( vdev )) < 0 ) {
		return( 0 );
	}

	if ((ret = ioctl(fd, cmd, buffer)) < 0 ) {
		ret = 0;
	} else {
		ret = 1;
	}

	return( ret );
} 

/*
 * get_vdisk_subvol()
 *	This routine issues the system call to get the dev_t of 
 *	the subvolume of the virtual disk. If rt is set, the
 *	dev_t of the realtime subvolume is returned, otherwise the
 *	dev_t of the data subvolume is returned.
 *
 * RETURNS:
 *	dev_t of the subvolume
 */
STATIC dev_t
get_vdisk_subvol( dev_t vdev, int rt )
{
	xlv_getdev_t getdev_info;

	bzero(&getdev_info, sizeof(xlv_getdev_t));

	if ( issue_xlv_ioctl( vdev, DIOCGETXLVDEV, (char *)(&getdev_info)) ) {
		if (rt) 
			return( getdev_info.rt_subvol_dev);
		else 
			return( getdev_info.data_subvol_dev);
	} else {
		return( -1 );
	}
}

/*
 * get_extents_for_file()
 *	This routine issues the system call to obtain information on
 *	the extent layout of the file.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
STATIC grio_bmbt_irec_t *
get_extents_for_file( dev_t vdev, inode_num_t ino, int *numextents)
{
	grio_file_id_t		fileid;
	grio_bmbt_irec_t 	*extents;
	cap_t			ocap;
	cap_value_t		cap_device_mgt = CAP_DEVICE_MGT;

	extents = (grio_bmbt_irec_t *)malloc( 
		sizeof(grio_bmbt_irec_t) * NUM_EXTENTS_PER_FILE );

	*numextents = NUM_EXTENTS_PER_FILE;
	fileid.ino = ino;
	fileid.fs_dev = vdev;

	ocap = cap_acquire(1, &cap_device_mgt);
	if (syssgi(SGI_GRIO, GRIO_GET_FILE_EXTENTS, 
				&fileid, extents, numextents) == -1) {
		*numextents = 0;
	}
	cap_surrender(ocap);

	return( extents );
}


/*
 * is_file_realtime()
 *	This routine issues a system call to determine if the given
 *	inode on the given file system device is a real time file.
 *
 *
 * RETURNS:
 *	1 if the file in on the real time plex
 *	0 of it is on the data plex.
 */
STATIC int
is_file_realtime( dev_t vdev, inode_num_t ino)
{
	int 		rt = -1;
	grio_file_id_t	fileid;
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;

	fileid.ino = ino;
	fileid.fs_dev = vdev;
	ocap = cap_acquire(1, &cap_device_mgt);
	if (syssgi(SGI_GRIO, GRIO_GET_FILE_RT, &fileid, &rt) == -1 ) {
		dbgprintf(1,("GRIO_GET_FILE_RT failed.\n"));
	}
	cap_surrender(ocap);
	return(rt);
}

/*
 * get_file_system_block_size()
 *	This routine issues the system call to obtain the block size of
 *	the given file system.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
STATIC int
get_file_system_block_size( dev_t vdev )
{
	int block_size = 0;
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;

	ocap = cap_acquire(1, &cap_device_mgt);
	if (syssgi(SGI_GRIO, GRIO_GET_FS_BLOCK_SIZE, vdev, 
					&block_size, 0, 0 ) == -1) {
		dbgprintf(1,("GRIO_GET_FS_BLOCK_SIZE failed.\n"));
	}
	cap_surrender(ocap);
	return(block_size);
}

/*
 * free_extents_for_file()
 *	This routine frees the memory that holds the file extent info.
 *
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
STATIC int
free_extents_for_file( grio_bmbt_irec_t *extents)
{
	free(extents);

	return( 1 );
}



/**************************************************************************
 *   The remainder of the routines in this file are used for debugging.   *
 **************************************************************************/

/* 
 * dump_disk_info()
 *
 *	This routine displays the fields of the
 *	xlv_tab_disk_part_t structure.
 *
 */
STATIC void
dump_disk_info(xlv_tab_disk_part_t *diskp)
{
	char		*uuid_str;
	uint_t		ignore_status;

	printf("\n");
	printf("disk info: \n");

	uuid_to_string (&diskp->uuid, &uuid_str, &ignore_status);
#ifdef DISK_PARTNAME
	printf("   name = %s, dev = %x, uuid = %s \n",
		diskp->part_name, diskp->dev[diskp->active_path], uuid_str);
#else
	printf("   no disk name, dev = %x, uuid = %s \n",
		diskp->dev[diskp->active_path], uuid_str);
#endif
	free (uuid_str);
}

/* 
 * dump_elmnt_info()
 *
 *	This routine displays the fields of the
 *	xlv_tab_vol_elmnt_t structure.
 *
 */
STATIC void
dump_elmnt_info(xlv_tab_vol_elmnt_t *elmntp)
{
	int i;
	char            *uuid_str;
        uint_t  	ignore_status;

        uuid_to_string (&elmntp->uuid, &uuid_str, &ignore_status);

	printf("\n");
	printf("element info: \n");
	printf("   uuid = %s, name = %s, grp_size = %x \n",
		uuid_str, elmntp->veu_name, elmntp->grp_size);
	printf("   strip_unit_size = %x | start_block_no = %llu \n",
		elmntp->stripe_unit_size, elmntp->start_block_no );
	printf("   end_block_no = %llu \n",elmntp->end_block_no);
	for (i = 0; i < elmntp->grp_size; i++) {
		dump_disk_info(&elmntp->disk_parts[i]);
	}

	free (uuid_str);
}

/* 
 * dump_plex_info()
 *
 *	This routine displays the fields of the
 *	xlv_tab_subvol_t structure.
 *
 */
STATIC void
dump_plex_info( xlv_tab_plex_t *plex)
{
	int i;
	char            *uuid_str;
        uint_t  	ignore_status;

	uuid_to_string (&plex->uuid, &uuid_str, &ignore_status);

	printf("\n");
	printf("plex info: \n");
	printf("flags = %x \n",plex->flags);
	printf("   flags = %x: uuid = %s : name = %s\n",
		plex->flags, uuid_str, plex->name);
	printf("   num_vol_elmnts = %x : max_vol_elmnts = %x \n",
		plex->num_vol_elmnts, plex->max_vol_elmnts);
	for (i = 0 ; i < (int)(plex->num_vol_elmnts); i++) {
		printf("dump elmnt info %x \n",&(plex->vol_elmnts[i]));
		dump_elmnt_info(&(plex->vol_elmnts[i]));
	}

	free (uuid_str);
}

/* 
 * dump_subvol_info()
 *
 *	This routine displays the fields of the
 *	xlv_tab_subvol_t structure.
 *
 */
void
dump_subvol_info(xlv_tab_subvol_t *subvol) 
{
	int i;
	char            *uuid_str;
        uint_t  	ignore_status;

        uuid_to_string (&subvol->uuid, &uuid_str, &ignore_status);
	
	printf("subvol info: \n");
	printf("   flags %x: open_flag %x: uuid %s: subvol_type %x\n",
		subvol->flags,subvol->open_flag,uuid_str,subvol->subvol_type);
	printf("   subvol_size = %llx:dev = %x: num_plexes = %x \n",
		subvol->subvol_size,subvol->dev,subvol->num_plexes);
	for (i = 0; i < subvol->num_plexes; i++ ) {
		printf("addr for plex dump is %x \n",subvol->plex[i]);
		dump_plex_info(subvol->plex[i]);
	}

	free (uuid_str);

}

#if 0
STATIC void
dump_extent_info(grio_bmbt_irec_t *extents, int numextents)
{
	int i;
	int	count = 10;

	printf("File extents number of extents = %d \n", numextents);
	for ( i = 0 ; i < numextents; i++) 	{
		printf("starting file offset  %llx\n",
				(__uint64_t)extents[i].br_startoff);
		printf("starting block number %llx\n",
				(__uint64_t)extents[i].br_startblock);
		printf("         block count  %llx\n",extents[i].br_blockcount);
		count--;
		if (!count) break;
	}
}
#endif




int
disk_not_in_list( 
	grio_disk_list_t list[], 
	int	arraycount,
	dev_t	diskdev) 
{
	int	 i;

	for (i = 0; i < arraycount; i++) {
		if (list[i].diskdev == diskdev)
			return( 0 );
	}
	return( 1 );
}



/*
 * get_all_disks_in_vdisk()
 * 
 * 
 * 
 * RETURNS: 
 * 	a list of all the disks in the virtual disk
 * 	"arraysize" will contain the count of the disks
 *	0 is returned on success
 *	-1 on failure
 */
int
get_all_disks_in_vdisk( dev_t vdev, int rt, grio_disk_list_t list[], int *arraysize ) 
{
	int			ac = 0, i, j, k;
	dev_t			physdev, diskdev, subvol_dev;
	xlv_tab_plex_t		*plex;
	xlv_tab_subvol_t	*subvol;
	xlv_tab_vol_elmnt_t	*volep;
	xlv_tab_disk_part_t	*diskp;
	
	if (check_xlv_device(vdev)) {
		/*
 		 * get the subvolume for the virtual disk
 	 	 */
		subvol_dev = get_vdisk_subvol(vdev, rt);

		subvol	= get_subvol_info( subvol_dev );
		assert( subvol );

		if ( subvol ) {
			/*
	 		 * For each plex in the subvolume
	 		 */
			for (i = 0; i < subvol->num_plexes; i++ ) {
				plex = subvol->plex[i];

				/*
	 		 	 * For each volume element in this plex
			 	 */
				for ( j = 0; j < (int)(plex->num_vol_elmnts); j++ ) {
					volep = &(plex->vol_elmnts[j]);
				
					/*
					 * For each disk part in this element
					 */
					for ( k = 0; k < volep->grp_size; k++ ) {
						diskp = &(volep->disk_parts[k]);
						physdev = diskp->dev[diskp->active_path];
						diskdev = get_diskid(physdev);
						if ( disk_not_in_list(
								list, 
								ac,
								diskdev) ) {
							if (ac < *arraysize) {
								list[ac].diskdev = 
									diskdev;
								list[ac++].physdev = 
									physdev;
							} else {
								return( -1 );
							}
						}
					}
	
				}
	
			}
	
		}
	} else { /* non-xlv volume */
		diskdev = get_diskid(vdev);
		list[ac].physdev = vdev;
		list[ac++].diskdev = diskdev;
	}
	*arraysize = ac;

	return ( 0 );
}


int
time_is_valid( grio_resv_t *griorp)
{
	/*
	 * Check if time of reservation has already expired.
	 * Start time can be no earlier than 30 seconds ago.
	 * Duration must be a positive number.
	 */
	if ( ( (griorp->gr_start + griorp->gr_duration) < time(0) )  || 
	     ( (griorp->gr_start + 120) < time(0)		  )  ||
	       (griorp->gr_duration < 0 ) ) {
		return( 0 );
	}
	return( 1 );
}







/*
 * add_new_striped_disks_to_list()
 *	This routine adds all the disks associated with this sub volume	
 *	and their respective bcounts to the list of physical disks.
 *	
 *
 * RETURNS:
 *	0 on success
 *	non-zero on failure
 */
STATIC int
add_new_striped_disks_to_list( 
	grio_disk_list_t 	disks[],
	int		 	*diskcount,
	int		 	maxdisks,
	xlv_tab_vol_elmnt_t 	*volep,
	int reqsize,
	int reqtime,
	int numbuckets)
{
	int 			i, j;
	xlv_tab_disk_part_t 	*diskp;

	dbgprintf( 7, ("add_new_striped_disks_to_list diskcount = %d \n",*diskcount) );

	for (i = 0; i < volep->grp_size ; i++ ) {
		diskp = &(volep->disk_parts[i]);

		for ( j = 0; j < *diskcount; j++) {
			if ( disks[j].physdev == get_diskid(diskp->dev[diskp->active_path])) {
				(*diskcount) = 0;
				dbgprintf(1,("duplicate disk in array\n"));
				return( ENXIO );
			}
		}
		if (*diskcount < maxdisks) {
			disks[*diskcount].physdev	= 
			        get_diskid(diskp->dev[diskp->active_path]);
			disks[*diskcount].reqsize	= reqsize;
			disks[*diskcount].optime  	= reqtime;
			disks[*diskcount].numbuckets	= numbuckets;
			(*diskcount)++;
		} else {
			(*diskcount) = 0;
			dbgprintf(1, ("disk array overflow\n") );
			return( ENXIO );
		}
	}
	return( 0 );
}


typedef struct rt_disk_info {
	dev_t	physdev;
	int	rotate_position;
	int	stripe_unit_size;
} rt_disk_info_t;

rt_disk_info_t xlvdisks[5000];
int   rt_disk_count = 0;

int 
add_xlv_disks( xlv_tab_subvol_t *subvol)
{
	xlv_tab_vol_elmnt_t	*vole;
	xlv_tab_plex_t		*plex;
	int 			p, d;
	long			ve;

	/*
 	 * For each plex ...
 	 */
	for (p = 0; p < subvol->num_plexes; p++ ) {
		plex = subvol->plex[p];
		/*
 		 * For each plex elemnt ...
		 */
		for (ve = 0; ve < plex->num_vol_elmnts; ve++) {
			vole = &(plex->vol_elmnts[ve]);
			/*
	 		 * For each stripe or concatenated 
		  	 * elemnt ...
			 */
			for (d=0;d< vole->grp_size;d++) {
				xlvdisks[rt_disk_count].physdev = vole->disk_parts[d].dev[vole->disk_parts[d].active_path];

				if (vole->type == XLV_VE_TYPE_STRIPE) { 
					xlvdisks[rt_disk_count].rotate_position = d;
					xlvdisks[rt_disk_count].stripe_unit_size = vole->stripe_unit_size*512;
				} else {
					xlvdisks[rt_disk_count].rotate_position = 0;
					xlvdisks[rt_disk_count].stripe_unit_size = 0;
				}
				rt_disk_count++;
			}
		}
	}
	return( 0 );
}


int
free_subvol_space(xlv_tab_subvol_t *subvol)
{
        int i;

        for ( i = 0; i < XLV_MAX_PLEXES; i ++ ) {
                free(subvol->plex[i]);
        }
        free(subvol);
        return (0);
}

/*
 * get_xlv_disks()
 *	This routine calls the xlv subsystem and creates a list of those
 *	disks which are used in realtime volumes. This list is stored in
 *	the global array "xlvdisks".
 *
 * RETURNS:
 *	0 on succes
 *	1 on failure
 */
int
get_xlv_disks( void )
{
	xlv_attr_cursor_t	cursor;
	xlv_attr_req_t		req;
	xlv_tab_subvol_t	*subvol;
	int			status;

        /*
         * First we need to get a XLV cursor.
         */
        if ( syssgi(SGI_XLV_ATTR_CURSOR, &cursor) < 0) {
                printf("Failed to get a XLV cursor\n");
                return(1);
	}

	/*
	 * Allocate memory for subvolume info.
	 */
        if (NULL == (subvol = get_subvol_space())) {
                printf("Cannot malloc a subvolume entry.\n");
                return(1);
        }


	req.attr = XLV_ATTR_SUBVOL;
	req.ar_svp = subvol;
	status = 0;

	while ( status == 0 ) {
		status = syssgi(SGI_XLV_ATTR_GET, &cursor, &req);
		if (status >= 0) {
			/*
			 * Determine if this is a real time subvolume.
			 */
			if (subvol->subvol_type == XLV_SUBVOL_TYPE_RT)
				add_xlv_disks( subvol );
		}
	}
	free_subvol_space( subvol );
	return(0);
}


/*
 * XLV initialization routine.
 * Returns: 0 on succes
 *	    1 on failure
 */
int
vdisk_init()
{
	int i;

	if (get_xlv_disks())
		return(1);
	for (i = 0; i < rt_disk_count; i++) {
		if (disk_setup(xlvdisks[i].physdev, 	
		    xlvdisks[i].rotate_position) == NULL)
			return(1);
	}
	return(0);
}


/*
 * is_realtime_disk()
 *	Determine if the disk specified by the given ctlr, lun, and unit
 *	is to be used as a real time disk.
 *
 * RETURNS:
 *	1 if the disk is set for realtime use.
 *	0 if not.
 * INPUT:
 *	device number
 */
/* ARGSUSED */
int
is_realtime_disk(dev_t sdev, int *stripe_size)
{
	int 	i;

	for (i = 0; i < rt_disk_count; i++) {
		if (xlvdisks[i].physdev == sdev) {
			*stripe_size = xlvdisks[i].stripe_unit_size;
			return( 1 );
		} 
	}
	return( 0 );
}

