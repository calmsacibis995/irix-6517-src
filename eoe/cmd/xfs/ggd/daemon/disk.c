#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <invent.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <grio.h>
#include <sys/scsi.h>
#include <sys/sysmacros.h>
#include <sys/bsd_types.h>
#include <sys/dkio.h>
#include <fcntl.h>
#include <diskinvent.h>
#include <sys/fs/xfs_inum.h>
#include <sys/uuid.h>
#include <sys/ktime.h>
#include <sys/major.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/conf.h>
#include <sys/iograph.h>
#include <sys/capability.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

#define LOWBANDWIDTH (64 * 1024 * 23)

/*
 * The following code is entirely lifted from the cfg code that
 * runs on old platforms.
 */

typedef struct dk_iosize {
	dev_t	device;
	int     iosize;
} dk_iosize_t;

dev_disk_t 	devdisks[NUMDISKS];
dk_iosize_t	diskiosize[NUMDISKS];	
STATIC dev_info_t	tinfo = { 0, 0 };

STATIC dev_info_t 	*lookup_disk_limits(char *, int);
int 		lookup_default_disk_size(dev_t);
extern int	is_realtime_disk(dev_t, int *);
extern int	time_is_valid(grio_resv_t *);



/*
 *  lookup_disk_limits()
 *      This routine finds and returns the dev_info_t structure for the
 *      disk type with the given id string.
 *
 *
 *
 *
 * RETURNS:
 *      pointer to a dev_info_t on success
 *      NULL on failure
 */
STATIC dev_info_t *
lookup_disk_limits(char *idstr,int iosize)
{
	dev_info_t *devtp = NULL;
	int i;

	for ( i = 0 ; i < NUMDISKS; i ++) {
		if (strncmp(idstr, devdisks[i].disk_id, DISK_ID_SIZE) == 0) {
			if ( (iosize == 0) || 
			     (devdisks[i].dev_info.optiosize == iosize ) ) {
				devtp = &devdisks[i].dev_info;
				break;
			}
		}
	}
	return(	devtp );
}


/*
 * get_disk_io_string()
 *	This routine queries the disk driver about the device id string
 *	of a given disk device. The string is returned in the idstr
 *	parameter.
 * INPUT:
 *	partition character device name
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
int
get_disk_io_string(char *dname, char *idstr)
{
	int fd, ret = 1;

	if ((fd = open(dname, O_RDONLY)) == -1) { 
		printf("open failed on %s\n", dname);
		ret = 0;
	} else {
		if (ioctl(fd, DIOCDRIVETYPE, idstr) == -1 ) {
			printf("ioctl failed \n");
			ret = 0;
		}
		close( fd );
	}
	return( ret );
}


/*
 * get_disk_io_limits 
 *	This routine finds and returns the dev_info_t structure for the
 * 	disk at the given location.
 * INPUT:
 *	partition block device number
 * RETURNS:
 *	pointer to a dev_info_t on success
 */ 
STATIC dev_info_t *
get_disk_io_limits(dev_t sdev)
{
	int		iosize, defiosize, strlen;
	char 		idstr[DISK_ID_SIZE+1];
	dev_info_t 	*devinfop = NULL;
	char 		pathnm[MAXDEVNAME];
	int  		buflen = sizeof(pathnm);
	struct stat64	sbuf;
	char            err_dev[DEV_NMLEN];

	bzero(idstr, DISK_ID_SIZE+1);
	bzero(pathnm, MAXDEVNAME);
	dev_to_rawpath(sdev, pathnm, &buflen, &sbuf);
	iosize =  0;
	if (get_disk_io_string(pathnm, idstr)) {
		if (is_realtime_disk(sdev, &iosize)) {
			/*
			 * if this is a realtime disk, use the
			 * stripe unit size as the optimal I/O size.
			 *
 		 	 * the user has set a default iosize for the drive
		 	 * use this instead of the stripe size.
	 	 	 */
			defiosize = lookup_default_disk_size(sdev);
			if ( defiosize != 0 ) {
				iosize = defiosize;
			}
		}

		/*
		 * if no other i/o size is set,
		 * use the cfg default optimal i/o size
		 */
		if ( iosize == 0 ) {
			iosize = defaultoptiosize * 1024;
		}

		devinfop = lookup_disk_limits( idstr, iosize );
	}
	if (!devinfop) {
		strlen = DEV_NMLEN;
		dev_to_alias(sdev, err_dev, &strlen);
		errorlog("unknown disk %s\n", err_dev);
		errorlog("assuming approx bandwidth 1Mb/s, optiosize %d bytes", iosize);
		errorlog("read man pages for grio_disks and update /etc/grio_disks\n");
		devinfop = &tinfo;
		devinfop->maxioops = MAX(1,(LOWBANDWIDTH / iosize));
		devinfop->optiosize = iosize;
	}
	return ( devinfop );
}


/*
 * add_disk_io_limits 
 * 	This routine adds a new entry to the devdisks array.
 * 
 * 
 *  RETURNS:
 * 	0 on success
 * 	-1 if disk type was already added
 */
int
add_disk_io_limits(char *idstr, int maxioops, int optiosize)
{
	int i,ret = -1;

	if ( lookup_disk_limits( idstr, optiosize ) !=  NULL) {
		return( ret );	
	}
	
	if ( optiosize == 0 ){
		printf("0 is an invalid iosize for %s \n",idstr);
		return( ret );
	}

        for ( i = 0 ; i < NUMDISKS; i ++) {
                if ( devdisks[i].dev_info.optiosize == 0 )  {
			strncpy(devdisks[i].disk_id, idstr, DISK_ID_SIZE);
			devdisks[i].dev_info.maxioops = maxioops;
			devdisks[i].dev_info.optiosize = optiosize;
			ret = 0;
                        break;
                }
        }

	if (ret ) {
		printf("TOO MANY DISKS - ARRAY FULL !!!\n");
	}

	if (lookup_disk_limits(idstr, optiosize) == NULL ) {
		ggd_fatal_error("COULD NOT FIND ID/SIZE AFTER ADDING");
	}

	return ( ret );
}



int
lookup_default_disk_size(dev_t sdev)
{
	int	i;
	
	for (i = 0; i < NUMDISKS; i++ ) {
		if (diskiosize[i].device == sdev)
			return( diskiosize[i].iosize );
	}
	return( 0 );
}

int
add_default_disk_size(dev_t sdev, int iosize)
{

	int 	i;

	if ( lookup_default_disk_size(sdev) != 0 ) {
		return( 0 );
	}

	for (i = 0; i < NUMDISKS; i++ ) {
		if ( diskiosize[i].iosize == 0 ) {
			diskiosize[i].device = sdev;
			diskiosize[i].iosize = iosize;
			return( 0 );
		}
	}

	printf(" DEFAULT DISK SIZE ARRAY FULL \n");
	return( -1 );
}


int
add_grio_disk_info(
dev_t	sdev,
int	num,	
int	optio,
int	rtp)
{
	int				ret;
	grio_add_disk_info_struct_t	info;
	cap_t				ocap;
	cap_value_t			cap_device_mgt = CAP_DEVICE_MGT;

	/*
	 * Add grio disk information to 
	 * grio driver.
	 */
	info.gdev		= sdev;
	info.num_iops_rsv	= 0;
	info.num_iops_max	= num;
	info.opt_io_size	= optio;
	info.rotation_slot	= rtp;
	if (rtp < 0)
		info.realtime_disk = 0;
	else
		info.realtime_disk = 1;

	ocap = cap_acquire(1, &cap_device_mgt);
	ret = syssgi(SGI_GRIO, GRIO_ADD_DISK_INFO, &info);
	cap_surrender(ocap);

	if (ret < 0 ) {
		printf("Could not add grio information for device %d\n", 
					sdev);
		return(1);
	}
	return( 0 );
}

/*
 * Returns a unique disk id for all the partitions on a disk.
 *
 * INPUT:
 *	 block/char device number of partition or volume header or
 *	 disk id.
 * RETURN:
 *	 0 on failure
 *	 id on success
 */

dev_t
get_diskid(dev_t diskpart)
{
	char pathnm[MAXDEVNAME];
	int  buflen = sizeof(pathnm);
	const char *part_keyword = "/" EDGE_LBL_PARTITION "/";
	const char *vhdr_keyword = "/" EDGE_LBL_VOLUME_HEADER "/";
	const char *disk_keyword = "/" EDGE_LBL_DISK;
	char *t1;
	struct stat sbuf;

	bzero(pathnm, MAXDEVNAME);
	dev_to_rawpath(diskpart, pathnm, &buflen, 0);

	/*
	 * Verify it is a disk name.
	 */
	t1 = strstr(pathnm, disk_keyword);
	if (t1 == NULL)
		return(0);

	/*
	 * Now try to slash off the partition/volume_header part.
	 */
	t1 = strstr(pathnm, part_keyword);
	if (t1 == NULL)
		t1 = strstr(pathnm, vhdr_keyword);
	if (t1 != NULL)
		*t1 = 0;
	if (stat(pathnm, &sbuf) == -1)
		return(0);
	return(sbuf.st_rdev);
}


/* mark_disk_noretry()
 *	Turn off dksc driver retry for this RT disk.
 *	This gets
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
int
mark_disk_noretry(dev_t diskdev)
{
	int	fd, ret, length = DEV_NMLEN;
	char    diskname[DEV_NMLEN];

	dev_to_rawpath(diskdev, diskname, &length, 0);

	if ((fd = open (diskname, O_RDONLY)) != -1) {

		/*
		 * turn off dksc driver retry mechanisms
		 */
                ret = ioctl(fd, DIOCNORETRY, 1);
	} else
		ret = -1;

	close(fd);
	if (ret)
		printf("Could not disable retry on disk %s.\n", diskname);

	return( ret );
}


/*
 * This does 4 things:
 * a) Tells ggd about the presence of a disk partition. Inserts info.
 * about the disk in the ggd cache.
 * b) Tells kernel about the disk partition parameters.
 * c) Adds disk info. to the ggd shm.
 * d) For RT disks, turns off retry and sorting
 *
 * For all partitions on a disk, we use a single disk id. Note that
 * due to the REPLACE clause a cache entry might already exist for
 * the disk. A disk with a RT partition will be marked as a rt disk
 * and RT io scheduling will be done on it. A disk w/o rt partitions
 * will have prio scheduling done on it (by the kernel).
 *
 * INPUT:
 *	 block device number of partition
 *	 rotation position for RT striped disks (could be 0)
 *			       0 for RT nonstriped disks
 *			       -1 for non RT disks
 * RETURN:
 *	 0 on failure
 *	 pointer to cached info on success
 */
 
device_node_t * 
disk_setup(dev_t diskdev, int rotposn)
{
	device_node_t	*devnp;
	dev_info_t	*sinfo;
	dev_t		diskid;
	stream_id_t	stream_id;
	int		maxreqsize;
	uint_t		status;
	time_t		maxreqtime;
	char		err_dev[DEV_NMLEN], errstr[80];

	diskid = get_diskid(diskdev);
	if (diskid == 0)
		return(0);
	if ((devnp = devinfo_from_cache(diskid)) == 0) {
		sinfo = get_disk_io_limits(diskdev);
		if ((devnp = (device_node_t*)calloc(1, 
		   sizeof(device_node_t))) == NULL)
			ggd_fatal_error("calloc failed in disk_setup");

		devnp->node_number = diskid;
		devnp->flags = INV_SCSIDRIVE;
		devnp->resv_info.maxioops = sinfo->maxioops;
		devnp->resv_info.optiosize = sinfo->optiosize;
		devnp->resv_info.dev = diskid;
		init_ggd_info_node(devnp);
		if (insert_info_in_cache(diskid,(void *)devnp)) {
			free(devnp);
			return(0);
		}
	} else if (devnp->initialize_done == TOLD_KERNEL)
		return(devnp);
	if (add_grio_disk_info(diskdev, devnp->resv_info.maxioops, 
	    devnp->resv_info.optiosize, rotposn)) {
		return(0);
	}
	devnp->initialize_done = TOLD_KERNEL;
	if (rotposn >= 0) {
		mark_disk_noretry(diskdev);
	}

	/*
	 * Reserve some iops for non-guaranteed file access.
	 */
	uuid_create(&stream_id, &status);
	reserve_path_bandwidth(diskid, 0, devnp->resv_info.optiosize, HZ, 
		0, time(0), SYSTEM_DURATION, &stream_id, &maxreqsize, 
		&maxreqtime, err_dev, NON_ROTOR_GUAR|SYSTEM_GUAR, 0);

	if (status) {
		sprintf(errstr,"Reserve of system bandwidth for device "
				"%d failed", diskdev);
		ggd_fatal_error(errstr);
	}
	return(devnp);
}


/*
 * start_guaranteed_disk_stream
 *      Issue the system calls which will enable a process using
 *      the given stream_id to receive guaranteed rate I/O.
 *
 * RETURNS:
 *      0 on success
 *      non-zero on failure
 */
int
start_guaranteed_disk_stream( reservations_t *resvp)
{
	int             	status;
	dev_t			physdev;
	dev_resv_info_t 	*resvpdp = DEV_RESV_INFO( resvp );
	grio_disk_id_t		grio_disk_id;
	cap_t			ocap;
	cap_value_t		cap_device_mgt = CAP_DEVICE_MGT;
	
	
	if (resvp->flags & SYSTEM_GUAR) return(0);

	/*
	 * call GRIO_ADD_DISK_STREAM_INFO for this disk reservation
	 */
	physdev = resvpdp->diskdev;
	grio_disk_id.num_iops		= resvpdp->num_iops;
	grio_disk_id.iops_time		= resvpdp->io_time;

	grio_disk_id.opt_io_size	= resvpdp->iosize;

	dbgprintf(5,("starting guaranteed stream on disk %x \n",physdev));

	ocap = cap_acquire(1, &cap_device_mgt);
	status = syssgi(SGI_GRIO, GRIO_ADD_STREAM_DISK_INFO,
			physdev, &grio_disk_id, &(resvp->stream_id) );
	cap_surrender(ocap);

	if ( status ) {
		dbgprintf(1, ("ADD_DISK_STREAM failed %d\n",status));
	}

	return( 0 );
}

/*
 * reserve_disk_bandwidth()
 *
 *	This routine reserves the bandwidth on the single disk.
 *
 * RETURN:
 *	0 if the reservation was successful
 *	non-zero on error
 */
/*ARGSUSED*/
int
reserve_disk_bandwidth(
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
	int			iosize, totalreqsize;
	time_t			reqtime;
	time_t			start, duration;
	char			err_dev[DEV_NMLEN];
	int			numbuckets;
	int			length = DEV_NMLEN;
	dev_t			diskid;

	griorp	= GRIO_GET_RESV_DATA( griocp );
	dev	= griocp->one_end.gr_dev;

	/*
	 * check that gr_start and gr_duration are valid
	 */
	if ( !time_is_valid( griorp ) ) {
		griorp->gr_error = EIO;
		return( griorp->gr_error );
	}

	diskid = get_diskid(dev);

	/* Make sure the disk has been initialized. If not,
	 * initialize it.
	 * This is possible because initialization is done in a lazy,
	 * on-demand manner for non-XLV volumes.
	 */
	if (disk_setup(dev, -1) == NULL) {
		griorp->gr_error = EINVAL;
		dev_to_devname(dev, griorp->gr_errordev, &length);
			return (EINVAL);
	}

	iosize	= get_grio_iosize_for_filesys(dev, 0);
	if (iosize <= 0) {
		griorp->gr_error = EACCES;
		return(griorp->gr_error);
	}

	totalreqsize = ROUND_UP( griorp->gr_opsize, iosize);
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

	status = reserve_path_bandwidth(diskid, dev, totalreqsize,
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
 * unreserve_disk_bandwith()
 *
 *      This routine removes the bandwidth reservations on the disk.
 *
 * RETURN:
 *	0 if reservation was successful
 *	non-zero if reservation was unsuccessful.
 */
int
unreserve_disk_bandwidth(
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
	dev_t			diskid;

	griorp = GRIO_GET_RESV_DATA( griocp );

	gr_stream_id = &(GRIO_GET_RESV_DATA(griocp)->gr_stream_id);

	diskid = get_diskid(dev);

	unreserve_path_bandwidth(diskid, gr_stream_id, griorp->gr_memloc);

	/* get bandwidth on disk dev */
	dnp = devinfo_from_cache(diskid);
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
