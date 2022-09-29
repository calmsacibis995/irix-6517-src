#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio_bandwidth/RCS/grio_raid.c,v 1.9 1998/07/20 23:31:15 kanoj Exp $"

/* 
 *  
 * grio_raid.c
 * 
 */
#include "grio_bandwidth.h"


/*
 * system_failed()
 * 
 * 
 * RETURNS: 
 *	non-0 if the result parameter indicates that the "system" 
 *		call returned an error
 * 	0 otherwise
 * 
 */
int
system_failed( int result, int result_check )
{
	int	ret;

	ret = ( ( result == -1 )         ||
		(!WIFEXITED( result ) )  ||
		( WIFSIGNALED( result )) ||
		( WTERMSIG( result )   ) ||
		(( result_check == 1 )   &&
		( WIFEXITED( result ) && (WEXITSTATUS( result ) != 0))));

	return( ret );
}


/*
 * get_serial_number()
 *
 */
int
get_serial_number(int ctlr, int unit, int *raidsig1, int *raidsig2)
{
#define 	PG25_LEN            	20	
#define  	G0_MSEN         	0x1A   /* Mode Sense */

	struct dsreq	*dsp;
	int 		retry = 2, rc, sig1, sig2, tmp;
	unsigned char 	buf[PG25_LEN];
	char 		raidname[DEV_NMLEN];

	/* 
	 * Make the scsi device name 
	 */
	bzero(raidname, DEV_NMLEN);
	sprintf(raidname, "/dev/scsi/sc%dd%dl0", ctlr, unit);

	/* 
	 * Open the SCSI device.
	 */

	if ((dsp = dsopen(raidname, O_RDONLY)) == NULL) {
		printf("dsopen() failed for %s\n", raidname);
		printf("make sure no process is accessing %s\n", raidname);
		return(-1);
	}
	
	/*
	 * Clear POR; do the request sense twice.
	 */

	while (retry--) {
		if (requestsense03(dsp, SENSEBUF(dsp), SENSELEN(dsp), 0)) {
			printf("requestsense03() failed for %s\n", raidname);
			dsclose(dsp);
			return(-1);
		}
	}

	fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), G0_MSEN, 0x08, 0x25, 
						0, PG25_LEN, 0);
	filldsreq(dsp, buf, PG25_LEN, DSRQ_READ|DSRQ_SENSE);
	retry = 3;
	while (retry--) {
		rc = doscsireq(getfd(dsp), dsp);
		if (rc == 0) break;
	}
	if (rc) {
		printf("modesense failed on %s\n", raidname);
		dsclose(dsp);
		return(-1);
	}
	sig1 = *(uint *)(buf + 8);
	sig2 = *(uint *)(buf + 12);
	if (sig1 > sig2) {
		tmp = sig1;
		sig1 = sig2;
		sig2 = tmp;
	}
	*raidsig1 = sig1;
	*raidsig2 = sig2;
	dsclose(dsp);
	return(0);
}

/*
 * add_raid_info()
 *	Determine if the infomation for the given RAID 
 *	device (serial_number,scsi_ctlr, scsi_unit) is already 
 *	included in the raid_info array. If not, add it.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int
add_raid_info( 
int			*num_raid_boxes, 
system_raid_info_t	raid_info[],
int			sig1, 
int			sig2, 
int			scsi_ctlr, 
int			scsi_unit)
{
	int	i;

	/*
	 * check if a RAID device with this serial number has already been added
 	 */
	for ( i = 0 ; i < *num_raid_boxes; i++ ) {
		if ((raid_info[i].sig1==sig1) && (raid_info[i].sig2==sig2)) {
			if ( (raid_info[i].spb_unit != 0 ) || 
			     (raid_info[i].spb_ctlr != 0 ) ) {
				fprintf(stderr,"grio_bandwidth: duplicate SP"
				" controller for RAID with signatures "
				"0x%x 0x%x.\n",sig1, sig2);
				return( -1 );
			}
			raid_info[i].spb_unit = scsi_unit;
			raid_info[i].spb_ctlr = scsi_ctlr;
			break;
		}
	}

	if ( i == *num_raid_boxes ) {
		if ( *num_raid_boxes >=  MAX_RAIDS_ON_SYSTEM ) {
			fprintf(stderr,"grio_bandwidth: too many RAID devices attached.\n");
			return( -1 );
		}
		(*num_raid_boxes)++;
		raid_info[i].sig1 = sig1;
		raid_info[i].sig2 = sig2;
		raid_info[i].spa_unit = scsi_unit;
		raid_info[i].spa_ctlr = scsi_ctlr;
	}

	return( 0 );
}

/* 
 * get_system_raid_info()
 *	
 *	Fill the raid_info array with information about the RAID
 *	devices attached to the system. num_raid_boxes contains
 *	the valid number of enteries in raid_info[].
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on error
 */
int
get_system_raid_info(
int			*num_raid_boxes,
system_raid_info_t	raid_info[])
{
	int		sig1, sig2, scsi_unit, scsi_ctlr, ret;
	inventory_t	*invp;

	setinvent();
	while (invp = getinvent()) {
		if ((invp->inv_class == INV_SCSI) && 
		  (invp->inv_type == INV_RAIDCTLR)) {
			scsi_ctlr = (int)invp->inv_controller;
			scsi_unit = (int)invp->inv_unit;
			ret = get_serial_number(scsi_ctlr, scsi_unit, 
							&sig1, &sig2);
			if (ret) return(ret);
			ret = add_raid_info( num_raid_boxes, raid_info,
				sig1, sig2, scsi_ctlr, scsi_unit);
			if (ret) return(ret);
		}
	}
	return(0);
}

/*
 * get_raid_luns_on_ctlr_unit() 
 * 
 *	This routine issues an hinv call and
 * 	extracts the information pertaining the RAID devices
 * 	attached to the system.
 * 
 * RETURNS: 
 * 	0 on success
 * 	-1 on error
 */
int
get_raid_luns_on_ctlr_unit(
int	*lun_count,
int	luns[],
int	scsi_ctlr,
int	scsi_unit)
{
	int	tmp, lun, result;
	char	unit_str[32];
	char	ctlr_str[32];
	char	cmd[LINE_LENGTH];
	char	line[LINE_LENGTH];
	char	filename[FILE_NAME_LEN];
	FILE	*fd;

	sprintf( unit_str, "unit %d",scsi_unit);
	sprintf( ctlr_str, "controller %d",scsi_ctlr);
	sprintf( filename, "/tmp/.grio_bandwidth.%d",getpid());

	sprintf( cmd, "hinv -c disk | grep RAID | grep \"%s\" | grep \"%s\" > %s",
			unit_str, ctlr_str, filename);

	/*
	 * issue hinv command and 
	 * look for lines of the form:
	 *
 	 * RAID lun: unit 3, lun 2 on SCSI controller 4
	 *
 	 */
	result = system( cmd );
	if ( system_failed(result, 1 ) ) {
		/*fprintf(stderr,"grio_bandwidth: could not get RAID luns from hinv.\n"); */
		unlink( filename );
		return( -1 );
	}

	result = 0;
	fd = fopen( filename, "r");
	if ( fd == NULL ) {
		fprintf(stderr,"grio_bandwidth: could not get RAID luns from hinv.\n");
		unlink( filename );
		return( -1 );
	};

	while ( fgets(line, LINE_LENGTH, fd ) ) {
		/*
		 * extract RAID info from line
	 	 */
		lun = -1;
		sscanf( line, "  RAID lun: unit %d, lun %d on SCSI controller", &tmp, &lun);
		if ( lun != -1 )  {
			luns[(*lun_count)++] = lun;
		} else {
			fprintf(stderr,"grio_bandwidth: could not determine "
				"RAID luns from hinv.\n");
			result = -1;
			break;
		}
	}
	fclose( fd );
	unlink( filename );

	return( result );
}

/*
 * get_devices_on_ctlr_unit()
 *
 *	Create a list of the names of the devices
 *	attached to the given SCSI ctlr and SCSI unit.
 *	The names are returned in the devicenames array. The
 *	count is returned int num_devices.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on failure
 */
int
get_devices_on_ctlr_unit( 
int		*num_devices, 
device_name_t	devicenames[MAX_NUM_DEVS],
int 		scsi_ctlr, 
int		scsi_unit)
{
	int	lun_count, i, which;
	int	luns[16];
	char	tmpname[DEV_NMLEN];

	lun_count = 0;
	bzero( luns, sizeof(luns) );
	if ( get_raid_luns_on_ctlr_unit( &lun_count, luns, scsi_ctlr, scsi_unit ) ) {
		return( -1 );
	}

	for ( i = 0; i <  lun_count; i++ ) {
		which = (*num_devices)++;
		if ( luns[i] ) {
			sprintf(tmpname,"/dev/rdsk/dks%dd%dl%dvol",
				scsi_ctlr, scsi_unit, luns[i]);
		} else {
			sprintf(tmpname,"/dev/rdsk/dks%dd%dvol",
				scsi_ctlr, scsi_unit);
		}
		if (user_diskname_to_nodename(tmpname, 
			         devicenames[which].devicename) == -1) {
			fprintf(stderr,
				"grio_bandwidth: failed to get disk "
				"on RAID %d, %d.\n", scsi_ctlr, scsi_unit);
			exit(1);
		}
		devicenames[which].scsi_ctlr = scsi_ctlr;
		devicenames[which].scsi_unit = scsi_unit;
		devicenames[which].scsi_lun  = luns[i];
	}
	return( 0 );

}

/*
 * get_maxioops_for_raid_controller()
 *
 *
 * RETURNS:
 *	the sum of the "average" iops counts for each device on a single RAID controller
 *
 */
int
get_maxioops_for_raid_controller(
int	startindex,
int	indexcount)
{
	int	 i, count, eachdisk;

	count = 0;
	for ( i = startindex; i < (startindex + indexcount); i++ ) {
		eachdisk = child_status_average[i];
		/*
                 * degrade performance by 10%
		 * (this is the same algorithm used in reclaim_bw_results()
                 */
                eachdisk = eachdisk - ((eachdisk+9)/10);

		count += eachdisk;
	}
	return( count );
}

/*
 * measure_raid_bw_for_device()
 *
 *	Measure the bandwidth characteristics of
 *	a single RAID device on the system.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int
measure_raid_bw_for_device( 
system_raid_info_t	*raid_info,
int			sampletime,
int			iosize,
int			rw,
int			addparams,
int			verbose, 
int			fairshare)
{
	int		i, num_devices, ret, average;
	int		ctlra_device_count, ctlrb_device_count;
	int		ctlra_io_count, ctlrb_io_count;
	char		nodenamea[DEV_NMLEN], nodenameb[DEV_NMLEN], *t1;
	const char	*lun_keyword = EDGE_LBL_LUN;
	device_name_t	devicenames[MAX_NUM_DEVS];

	bzero( devicenames, sizeof(devicenames) );
	num_devices = 0;

	/*
	 * Get all the luns on RAID controller A
	 */
	get_devices_on_ctlr_unit( &num_devices, devicenames, 
			raid_info->spa_ctlr, raid_info->spa_unit );

	ctlra_device_count = num_devices;
	/*
	 * Get the nodename for the RAID controller
	 */
	if (ctlra_device_count) {
		strncpy(nodenamea, devicenames[0].devicename, DEV_NMLEN);
		t1 = strstr(nodenamea, lun_keyword);
		*(t1 + sizeof(lun_keyword)) = '0';
		*(t1 + sizeof(lun_keyword) + 1) = 0;
	}

	if ( verbose ) {
		printf("Found %d RAID lun devices on RAID controller SPA located on SCSI "
			"controller %d, unit %d.\n", 
			ctlra_device_count, raid_info->spa_ctlr, raid_info->spa_unit);
	}

	/*
	 * Gdd all the luns on RAID controller B
	 */
	get_devices_on_ctlr_unit( &num_devices, devicenames, 
			raid_info->spb_ctlr, raid_info->spb_unit );

	ctlrb_device_count = num_devices - ctlra_device_count;
	/*
	 * Get the nodename for the RAID controller
	 */
	if (ctlrb_device_count) {
		strncpy(nodenameb, 
		     devicenames[ctlra_device_count].devicename, DEV_NMLEN);
		t1 = strstr(nodenameb, lun_keyword);
		*(t1 + sizeof(lun_keyword)) = '0';
		*(t1 + sizeof(lun_keyword) + 1) = 0;
	}


	if ( verbose ) {
		if (ctlrb_device_count) 
			printf("Found %d RAID lun devices on RAID controller "
				"SPB located on SCSI controller %d, unit %d"
				".\n", ctlrb_device_count, raid_info->spb_ctlr, 
				raid_info->spb_unit);
		else
			printf("Found no RAID lun devices on RAID controller "
				"SPB\n");
	}

	/*
	 * This call will handle the measurement of the bandwidth on the RAID disk devices.
	 * It will create a process to issue I/O to each lun and capture the results.
 	 */
	ret = measure_multiple_disks_io( num_devices, devicenames, sampletime, 
		iosize, rw, addparams, verbose, 1, 1, 0);

	/*
	 * Check if error occured during bandwidth measurement.
 	 */
	if ( ret ) {
		return( ret );
	}

	/*
	 * Sum up the I/O for disks on spb_ctlr,
	 * This will be the controller bandwidth.
	 */
	ctlrb_io_count = get_maxioops_for_raid_controller( 
		ctlra_device_count, ctlrb_device_count);

	fprintf( stdout,"\n");
	if (ctlrb_device_count) {
		fprintf( stdout,"RAID controller on SCSI controller %d, unit %d"
			 " provided:\n", raid_info->spb_ctlr, 	
			 raid_info->spb_unit);
		fprintf( stdout,"on average:\t\t%d ops of size %d each "
			 "second\n", ctlrb_io_count, iosize);
	}

	if (verbose && ctlrb_device_count) {
		printf("Device node %s supports %d I/Os per second of size "
			"%d bytes\n", nodenameb,  ctlrb_io_count, iosize);
	}

	/*
	 * add REPLACE line for RAID controller
	 */
	if (addparams && ctlrb_device_count) {
		remove_nodename_params_from_config_file(nodenameb);
		add_nodename_params_to_config_file(nodenameb, ctlrb_io_count, iosize);
	}

	/*
	 * Sum up the I/O for disks on the spa_ctlr,
	 * this will be the controller bandwidth.
 	 */
	ctlra_io_count = get_maxioops_for_raid_controller( 0, ctlra_device_count);

	fprintf( stdout,"RAID controller on SCSI controller %d, unit %d provided:\n",
			raid_info->spa_ctlr, raid_info->spa_unit);
	fprintf( stdout,"on average:\t\t%d ops of size %d each second\n",
			ctlra_io_count, iosize);

	if (verbose && ctlra_device_count) {
		printf("Device node %s supports %d I/Os per second of size %d bytes\n", 
			nodenamea, ctlra_io_count, iosize);
	}

	/*
	 * add REPLACE line for RAID controller
	 */
	if (addparams && ctlra_device_count) {
		remove_nodename_params_from_config_file(nodenamea);
		add_nodename_params_to_config_file(nodenamea, ctlra_io_count, iosize);
	}

	/*
	 * Now generate the I/O bandwidth numbers for each individual lun.
	 */
	if ( fairshare ) {
		for ( i = 0; i <  num_devices; i++ ) {
			printf("\n");
			if ( i < ctlra_device_count ) {
				average =  ctlra_io_count / ctlra_device_count;
			} else {
				average =  ctlrb_io_count / ctlrb_device_count;
			} 

			fprintf( stdout,"device %s:\n", devicenames[i].devicename);
			fprintf( stdout,"fairshare average:\t\t%d ops of size %d each second\n",
				average, iosize);

			if (addparams) {
				remove_nodename_params_from_config_file(
						devicenames[i].devicename);
				add_nodename_params_to_config_file(
				   devicenames[i].devicename, average, iosize);
			}
		}
	} else {
		for ( i = 0; i <  num_devices; i++ ) {
			printf("\n");
			measure_multiple_disks_io( 1, &(devicenames[i]),
				sampletime, iosize, rw, addparams, verbose, 1, 1, 1 );
		}
	}


	return( 0 );
}

/* 
 * measure_raid_bw() 
 * 
 * 	Determine the bandwidth characteristics for each
 * 	RAID device on the system.
 * 
 * RETURNS: 
 * 	0 on success
 *	-1 on error
 */
int
measure_raid_bw(
int	sampletime,
int	iosize,
int	rw, 
int	addparams,
int	verbose,
int	fairshare)
{
	int	 		num_raid_boxes, i;
	system_raid_info_t	raid_info[MAX_RAIDS_ON_SYSTEM];
	
	
	num_raid_boxes = 0;
	bzero( raid_info, sizeof(raid_info) );
	if ( get_system_raid_info( &num_raid_boxes, raid_info ) ) {
		return( -1 );
	}

	if( num_raid_boxes == 0 ) {
		printf("No RAID devices are connected to this system.\n");
		return( 0 );
	}

	for ( i = 0; i < num_raid_boxes; i++ ) {
		if ( measure_raid_bw_for_device( &(raid_info[i]), sampletime,
			 iosize, rw, addparams, verbose, fairshare)) {
			fprintf(stderr,
				"grio_bandwidth: Error occured while"
				" determining bandwidth information for "
				" RAID with signatures 0x%x 0x%x.\n",
				raid_info[i].sig1, raid_info[i].sig2);
			return( -1 );
		}
	}
	return(0);
}
