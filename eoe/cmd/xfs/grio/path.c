#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio/RCS/path.c,v 1.19 1997/03/19 20:35:12 kanoj Exp $"


/*
 * path.c
 *	grio utility
 *
 *
 */
#include "griostat.h"

/*
 * This is copied from daemon/disk.c.
 * INPUT:
 *	 block/char device number of partition or volume header or
 *	 disk id.
 * RETURN:
 *	 0 on failure
 *	 1 on success
 */
STATIC int
get_diskname(dev_t diskpart, char *pathnm)
{
	int  buflen = DEV_NAME_LEN;
	const char *part_keyword = "/" EDGE_LBL_PARTITION "/";
	const char *vhdr_keyword = "/" EDGE_LBL_VOLUME_HEADER "/";
	const char *disk_keyword = "/" EDGE_LBL_DISK;
	const char *dvol_keyword = "/" EDGE_LBL_VOLUME "/";
	char *t1;
	struct stat sbuf;

	bzero(pathnm, DEV_NAME_LEN);
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
	if (t1 == NULL)
		t1 = strstr(pathnm, dvol_keyword);
	if (t1 != NULL)
		*t1 = 0;
	if (stat(pathnm, &sbuf) == -1)
		return(0);
	return(1);
}


/*
 * The input device # is the block device # of the disk reported
 * by xlv or the unique disk name passed from ggd shm segment.
 */
int 
convert_devnum_to_nodename(dev_t device, char *nodename)
{
	char pathbuf[DEV_NAME_LEN];

	bzero(pathbuf, DEV_NAME_LEN);
	get_diskname(device, nodename);
	return(0);
}


/*
 * The user may have passed in the hwgraph name or the alias 
 * name of the disk.
 */
int
user_diskname_to_nodename(char *user_diskname, char *nodename)
{
	struct stat statbuf;

	if (stat(user_diskname, &statbuf))
		return(-1);
	return(convert_devnum_to_nodename(statbuf.st_rdev, nodename));
}


/*
 * print_path
 *
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
int
print_path(device_list_t devlist[], int devcount)
{
	int		i;

	/*
	 * Print path.
	 */
	printf("Device Path: \n");
	for (i = 0; i < devcount; i++) {
		printf("\t%s\n",devlist[i].name);
	}
	return( 0 );
}

/*
 * show_path_for_devs()
 *
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int 
show_path_for_devs(device_list_t disklist[], int diskcount)
{
	int 		i, ret, devcount, status;
	device_list_t	devlist[MAX_NUM_DEVS];

	for (i = 0; i < diskcount; i ++ ) {

		/*
	 	 * Convert the device path name to a grio node name.
	 	 */
		devcount = 0;
		bzero( devlist, sizeof(device_list_t)*MAX_NUM_DEVS);
		status = get_path(disklist[i].name, devlist, &devcount);

		if ( (status) || ( devcount == 0 ))  {
			printf("Could not get path for %s \n",disklist[i].name);
			return( 0 );
		} 

		/*
		 * Print returned path.
		 */
		if (ret = print_path(devlist, devcount)) {
			return( ret );
		}

	}

	return( 0 );
}


int
get_parent_of_device( char *node_name )
{
	char	line[LINE_LENGTH], *tok;
	FILE	*fd;

	fd = fopen( GRIOCONFIG, "r");
	if (fd == NULL )
		return( -1 );

	while ( fgets( line, LINE_LENGTH, fd) ) {
		/*
		 * remove carriage return
		 */
		if (strlen(line)) {
			line[strlen(line) - 1] = NULL;
		}

		if ( strchr(line, ':' ) != NULL ) {

			tok = strtok( line, " \t");
			tok = strtok( NULL, " \t" );
			while ( (tok) && (strcmp( tok, node_name) != 0) ) {
				tok = strtok( NULL, " \t" );
			}

			if ( tok != NULL ) {
				tok = strtok( line, ":");
				bzero( node_name, DEV_NAME_LEN);
				bcopy( tok, node_name, strlen(tok) );
				fclose( fd );
				return( 0 );
			}
		}
	}
	fclose( fd );
	return( -1 );
}

/*
 * get_path()
 *
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int 
get_path(char *node_name, device_list_t devlist[], int *devcount)
{
	int 		i, length;
	char		devname[DEV_NMLEN];
	grio_dev_info_t *buf;
	dev_t		physdev;
	struct stat	statbuf;

	/*
	 * Allow only disk paths to be reported.
	 */

	if (user_diskname_to_nodename(node_name, devname))
		return(-1);
	if (stat(devname, &statbuf))
		return(-1);
	physdev = statbuf.st_rdev;

	/*
	 * Allocate buffer space to read the path in from kernel.
	 */
	buf = (grio_dev_info_t *)malloc(MAXCOMPS * sizeof(grio_dev_info_t));
	if (buf == NULL)
		return(-1);

	/*
	 * Call into the kernel to read the path.
	 */
	if (syssgi(SGI_GRIO, GRIO_GET_HWGRAPH_PATH, physdev, buf, MAXCOMPS,
				devcount) == -1) {
		free(buf);
		return(-1);
	}

	/*
	 * copy devices in reverse order into the devlist[]
	 */
	for (i = 0; i < (*devcount); i++) {
		length = DEV_NMLEN;
		dev_to_devname(buf[i].devnum, devlist[i].name, &length);
	}
	
	free(buf);
	return( 0 );
}

