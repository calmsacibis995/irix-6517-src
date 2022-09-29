#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio/RCS/rotation.c,v 1.15 1997/03/12 20:40:02 kanoj Exp $"

/*
 * rotation.c
 *	grio utility
 *
 *
 */

#include "griostat.h"

#define		NUM_SUBVOLS_PER_PLEX	50


/*
 * alloc_subvol_space
 *	Allocate heap space for a XLV subovolume structure.
 *
 * RETURNS:
 *	pointer to allocated memory space
 */
xlv_tab_subvol_t *
alloc_subvol_space( void )
{
        xlv_tab_plex_t          *plex;
        xlv_tab_subvol_t        *subvol;
        int                     i, plexsize;

        subvol = malloc(sizeof(xlv_tab_subvol_t));
        bzero(subvol, sizeof(xlv_tab_subvol_t));
        plexsize = sizeof(xlv_tab_plex_t) +
                        (sizeof(xlv_tab_vol_elmnt_t) * NUM_SUBVOLS_PER_PLEX );
        for ( i = 0; i < XLV_MAX_PLEXES; i ++ ) {
                plex = malloc( plexsize );
                bzero(plex, plexsize );
                subvol->plex[i] = plex;
        }
        return( subvol );
}

/*
 * free_subvol_space()
 *	Free heap space for an XLV subvolume structure. 
 * 
 * RETURNS:
 * 	always returns 0
 */
int
free_subvol_space(xlv_tab_subvol_t *subvol)
{
	int i;

	for ( i = 0; i < XLV_MAX_PLEXES; i ++ ) {
		free(subvol->plex[i]);
	}
	free(subvol);
	return( 0 );
}

/*
 * show_ve_rotation()
 *	Display the bandwidth information for each disk device 
 *	associated with this XLV subvolume element.
 *
 *
 * RETURNS:
 *	always returns 0
 */
int
show_ve_rotation(xlv_tab_vol_elmnt_t *volep)
{
	char	device_name[DEV_NAME_LEN];
	xlv_tab_disk_part_t	*diskp;
	int	i, diskcount = 0;
	device_list_t 	disklist[100];

	printf("VE %s:\n",volep->veu_name);
	for (i = 0; i < volep->grp_size ; i++ ) {
		diskp = &(volep->disk_parts[i]);
		bzero(device_name, DEV_NAME_LEN);
		disklist[diskcount].dev = diskp->dev[diskp->active_path];
		convert_devnum_to_nodename(disklist[diskcount].dev, device_name);
		add_to_list(device_name, disklist, &diskcount );
	}
	show_bandwidth_rot(disklist, diskcount);
	return( 0 );
}

/*
 * show_plex_rotation
 *	Display the bandwidth information for each disk device 
 *	associated with this XLV plex.
 *	
 *
 * RETURNS:
 *	always returns 0
 */
int
show_plex_rotation(xlv_tab_plex_t *plex)
{
	xlv_tab_vol_elmnt_t 	*volep;
	int i;

	/*
	 * Look at each sub volume element in the plex.
	 */
	printf("PLEX:	%s:\n", plex->name);
	for ( i = 0; i < plex->num_vol_elmnts; i++ ) {
		volep = &(plex->vol_elmnts[i]);
		show_ve_rotation(volep);
	}
	return( 0 );
}

/*
 * open_rt_device()
 *	Given an XLV volume device node path for a data subvolume,
 *	create and open a device node for the realtime subvolume for
 *	this volume.
 *
 *
 * RETURNS:
 *	an open file descriptor for the realtime subvolume
 *	-1 on failure
 */
int
open_rt_device(char *volname)
{
	int 	fd;
	dev_t	ndev;
	char 	*node_name;
	char	template[20];
	struct	stat	statbuf;
	xlv_getdev_t	getdev_info;

	strncpy(template, "/tmp/grioXXXXXX", 20);

	/*
	 * Stat the data subvolume to obtain the dev_t.
 	 */
	if (stat(volname, &statbuf)) {
		printf("Could not stat %s.\n",volname);
		return( -1 );
	}

	/*
	 * Create the dev_t for the realtime subvolume.
	 */
	ndev = statbuf.st_rdev;
	bzero( &getdev_info, sizeof(xlv_getdev_t) );
	if (issue_vdisk_ioctl(ndev, DIOCGETXLVDEV, (char *)(&getdev_info))) {
		printf("Could not get real time device for %s.\n",volname);
		return( -1 );
	}
	ndev = getdev_info.rt_subvol_dev;

	/*
 	 * Create realtime subvolume node name.
 	 */
	node_name = mktemp(template);
	unlink(node_name);

	/*
	 * Create a device for the real time subvolume.
	 */
	if (mknod(node_name, S_IFCHR, ndev)) {
		if ( getuid != 0 ) {
			printf("Only superuser can create device node to obtain data on realtime subvolume.\n");
			
		} else {
			printf("Could not make real time device %s.\n",node_name);
		}
		unlink(node_name);
		return( -1 );
	}

	/*
 	 *  Open the real time subvolume.	
 	 */
	if ((fd = open(node_name, O_RDONLY)) == -1 ) {
		printf("No real time subvolume for %s.\n", volname);
		unlink(node_name);
		return( -1 );
	}

	/*
 	 * Remove node.
 	 */
	unlink(node_name);

	return(fd);
}

/*
 * show_vol_rotation()
 *	Display the bandwidth information for each disk device 
 *	associated with the realtime subvolume of this XLV volume.
 *
 *
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
int
show_vol_rotation( char *volname )
{
	xlv_tab_plex_t 	*plex;
	xlv_tab_subvol_t *subvol;
	int	fd, i;

	if ((fd = open_rt_device(volname)) == -1 ) {
		return( -1 );
	}

	subvol = alloc_subvol_space();

	if (ioctl(fd, DIOCGETSUBVOLINFO, subvol)  < 0) {
		printf("Could not get volume info for %s.\n", volname);
		return( - 1);
	}
	close(fd);

	printf("Real time subvolume of %s \n",volname);
	for (i = 0; i < subvol->num_plexes; i++ ) {
		plex = subvol->plex[i];
		show_plex_rotation(plex);
	}

	free_subvol_space( subvol );

	return( 0 );
}


/*
 * show_rotation()
 *	Display the bandwidth information for each disk device 
 *	associated with the realtime subvolume for each XLV volume
 *	in the given list.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int
show_rotation(device_list_t vollist[], int volcount)
{
	int	i;
	
	for (i = 0; i < volcount; i++ ) {
		show_vol_rotation(vollist[i].name);
	}
	return( 0 );
}

/*
 * get_vol_disks()  
 *	Create a list of disk devices associated with the realtime
 *  	subvolume for the given XLV volume.
 *
 * RETURNS:  
 *  	0 on success
 *  	-1 on failure
 */
int
get_vol_disks(char *volname, device_list_t disklist[], int *diskcount)
{
	int			fd, i, j, k;
	char			name[DEV_NAME_LEN];
	xlv_tab_plex_t 		*plex;
	xlv_tab_subvol_t 	*subvol;
	xlv_tab_vol_elmnt_t 	*volep;
	xlv_tab_disk_part_t	*diskp;

	/*
	 * Open realtime subvolume device.
 	 */
	if ((fd = open_rt_device(volname)) == -1 ) {
		return( -1 );
	}

	subvol = alloc_subvol_space();

	/*
 	 * Get configuration information for the realtime subvolume.
 	 */
	if (ioctl(fd, DIOCGETSUBVOLINFO, subvol)  < 0) {
		printf("Could not get volume info for %s.\n", volname);
		return( - 1);
	}
	close(fd);

	/*
	 * Extract disk device names from configuration information.
 	 */
	for (i = 0; i < subvol->num_plexes; i++ ) {
		plex = subvol->plex[i];

		for ( j = 0; j < plex->num_vol_elmnts; j++ ) {
			volep = &(plex->vol_elmnts[j]);

			for (k = 0; k < volep->grp_size ; k++ ) {
				diskp = &(volep->disk_parts[k]);

				bzero(name, DEV_NAME_LEN);
				disklist[*diskcount].dev = diskp->dev[diskp->active_path];
				convert_devnum_to_nodename(disklist[*diskcount].dev, name);
				add_to_list(name, disklist, diskcount);
			}
		}
	}
	free_subvol_space( subvol );

	return( 0 );
}

/*
 * show_vol_full()
 *	Display all the bandwidth/reservation information for each
 *	device associated with each disk device in the realtime subvolume
 *	of the given XLV volume.
 *
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
/* ARGSUSED */
int
show_vol_full(char *volname, device_list_t disklist[], int diskcount)
{
	int		i, j, k, max, ret, eachcount;
	int		count[MAX_NUM_DEVS];
	device_list_t	devlist[MAX_NUM_DEVS][MAX_DEV_DEPTH];
	device_list_t	eachlist[MAX_NUM_DEVS];
		

	bzero(devlist, sizeof(device_list_t)*MAX_NUM_DEVS*MAX_DEV_DEPTH);
	bzero(eachlist, sizeof(device_list_t)*MAX_NUM_DEVS);
	bzero(count, sizeof(int)*MAX_NUM_DEVS);

	/*
	 * Build up the list of components for each disk in the subvolume.
 	 */
	for ( i = 0; i < diskcount; i++) {

		if (ret = get_path( disklist[i].name, devlist[i], 
						&count[i])) {
			printf("Could not get device path for %s.\n",
				disklist[i].name);
			return( ret );
		}

		if ( count[i] > MAX_DEV_DEPTH) {
			printf("Device path for %s too long.\n",
				disklist[i].name);
			return( -1 );
		}
	}

	/*
	 * Determine the deepest path to a disk device.
 	 */
	max = 0;
	for ( i = 0; i < diskcount; i++) {
		if (count[i] > max) 
			max = count[i];
	}


	/*
	 * Display the device bandwidth information starting from the
	 * top of the tree (SYSTEM node) and ending with the leaves
	 * (DSK nodes).
	 */
	for ( j = 0; j < max; j++) {
		bzero(eachlist, sizeof(device_list_t)*MAX_NUM_DEVS);
		eachcount = 0;

		for ( i = 0; i < diskcount; i++) {
			if ((eachcount == 0) && (j < count[i])) {
				add_to_list(devlist[i][j].name, 
					eachlist, &eachcount);
			} else if (j < count[i]) {
				if (!item_in_list(devlist[i][j].name,
						eachlist, eachcount)) {
					add_to_list(devlist[i][j].name, 
						eachlist, &eachcount);
				}
			}
		}

		for ( k = 0; k < eachcount; k++) {
			show_bandwidth(eachlist[k].name);
		}
	}
	return( 0 );
	
}

/*
 * show_full
 *
 * 
 * RETURNS: 
 *	0 on success 
 * 	-1 on failure
 */
int
show_full(device_list_t vollist[], int volcount)
{
	int		i, ret, diskcount;
	device_list_t	disklist[MAX_NUM_DEVS];
	
	for (i = 0; i < volcount; i++ ) {
		printf(	"GRIO information for devices "
			"in volume %s \n",
			vollist[i].name);

		bzero(disklist, sizeof(device_list_t)*MAX_NUM_DEVS);
		diskcount = 0;

		if (get_vol_disks(vollist[i].name, disklist, &diskcount)) {
			printf("Could not obtain disks for volume %s \n",
				vollist[i].name);
			return( ret );
		}

		show_vol_full( vollist[i].name, disklist, diskcount);
		printf("\n\n\n\n");
	}
	return( 0 );
}
