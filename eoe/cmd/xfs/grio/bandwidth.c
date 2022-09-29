#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio/RCS/bandwidth.c,v 1.20 1997/03/12 22:45:33 kanoj Exp $"


/*
 * bandwidth.c
 *	grio utility
 *
 */

#include "griostat.h"

STATIC int
show_bandwidth_dev(dev_t devnum)
{
	int 	i, length = DEV_NMLEN;
	int	resvops;
	ggd_node_summary_t	*ggd_node;
	char	devname[DEV_NMLEN];

	for (i = 0; i < ggd_info->num_nodes; i++) {
		ggd_node = &(ggd_info->node_info[i]);
	
		if (ggd_node->node_num == devnum) {
			dev_to_devname(devnum, devname, &length);
        		printf("GRIO information for device: %s \n",devname);
        		printf("opt i/o size: %8d bytes\n",
                		ggd_node->optiosize);

			if (ggd_node->optiosize) {
				resvops =  ((ggd_node->max_io_rate - 
					ggd_node->remaining_io_rate)*1024) /
					ggd_node->optiosize;

        			printf( "reservations: %4d ops currently "
					"reserved (max. per quantum), "
					"%4d ops maximum\n",
                			resvops,
                			ggd_node->num_opt_ios);

				printf("   bandwidth: %8d kbytes/sec "
					"available, %8d kbytes/sec maximum\n",
					ggd_node->remaining_io_rate,
					ggd_node->max_io_rate);
			}
			return(0);
		}
	}
	return(-1);
}

/*
 * show_bandwidth()
 *
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int 
show_bandwidth(char *devname)
{
	struct stat		statbuf;

	printf("\n");
	if (stat(devname, &statbuf))
		return(-1);
	return(show_bandwidth_dev(statbuf.st_rdev));
}

/*
 * show_bandwidth_for_devs()
 *
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int 
show_bandwidth_for_devs(device_list_t devlist[], int devcount)
{
	int 		i;
	char		devname[DEV_NMLEN];
	
	for (i = 0; i < devcount; i ++ ) {

		bzero( devname, DEV_NMLEN );
		/*
		 * First, try to convert the user's disk alias name.
		 */
		if (user_diskname_to_nodename(devlist[i].name, devname)) {
			strncpy(devname, devlist[i].name, DEV_NMLEN);
		}
		show_bandwidth(devname);
	}

	return( 0 );
}

/*
 * show_bandwidth_rot()
 *
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int 
show_bandwidth_rot(device_list_t devlist[], int devcount)
{
	int 		i, j;
	grio_vod_info_t	griovod;

	
	for (i = 0; i < devcount; i ++ ) {

		/*
		 * get rotor/nonrotor reservation infor from kernel
	 	 */
		bzero( &griovod, sizeof(grio_vod_info_t) );
		if ( syssgi(SGI_GRIO, GRIO_GET_VOD_DISK_INFO, 
			devlist[i].dev, &griovod ) ) {
			exit( -1 );
		}

		show_bandwidth(devlist[i].name);

		printf("\t\t\tVOD Reservations: %10d \n",
			griovod.num_rotor_streams);

		printf("\t\t\t  Rotation Slot  Reservations \n");
		for ( j = 0; j < griovod.num_rotor_slots; j++ ) {
			printf("\t\t\t%10d %10d\n", j,
			griovod.rotor_slot[j]);
		}
		printf("\t\t\tNon VOD Reservations: %10d \n",
			griovod.num_nonrotor_streams);
	}
	return( 0 );
}

/*
 * convert_and_show_bandwidth
 *
 *
 *
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
int
convert_and_show_bandwidth(device_list_t disklist[], int diskcount)
{
	int		i, ret, devcount = 0;
	device_list_t	devlist[MAX_NUM_DEVS];


	if ( diskcount > ( MAX_NUM_DEVS/MAX_DEV_DEPTH) ) {
		printf("Too many disks specified. A maximum of %d disks can "
			"be specified on the command line.\n",
			MAX_NUM_DEVS/MAX_DEV_DEPTH);
		return( -1 );
	} 

	for (i = 0; i < diskcount; i++) {
		devcount = 0;
		bzero(devlist, sizeof(device_list_t)*MAX_NUM_DEVS);
		if (ret = get_path(disklist[i].name, devlist, &devcount)) {
			return( ret );
		}
		printf("GRIO information for path of  disk device %s \n",
			disklist[i].name);
		show_bandwidth_for_devs( devlist, devcount);
		printf("\n\n\n\n");
	}

	return( 0 );
}

int
show_all_bandwidth(void)
{
	int			i;

	for (i = 0; i < ggd_info->num_nodes; i++) {
		if (ggd_info->node_info[i].devtype == INV_SCSIDRIVE) {
			/* convert_devnum_to_nodename(device, devname); */
			show_bandwidth_dev(ggd_info->node_info[i].node_num);
		}
	}
	return( 0 );
}

int
get_ggd_info( void )
{
	int	shmid;
	ulong	ret;

	shmid = shmget( GGD_INFO_KEY, sizeof(ggd_info_summary_t), 0444);

	if (shmid < 0 ) {
		printf("Could not get GRIO shared memory id.\n");
		printf("ggd daemon may not be running. \n");
		return( -1 );
	}

	ret = (ulong)shmat( shmid, (void *)0, SHM_RDONLY );
	if ( ret == (ulong)(-1) ) {
		printf("Could not attach to GRIO shared memory segment.\n");
		return( -1 );
	}
	ggd_info = (ggd_info_summary_t *)ret;
	if (ggd_info->ggd_info_magic != GGD_INFO_MAGIC ) {
		printf("GRIO shared memory segment has "
		       "invalid magic number.\n");
		return( -1 );
	}
	return( 0 );
}
