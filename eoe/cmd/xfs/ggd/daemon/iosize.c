#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/iosize.c,v 1.17 1997/04/28 22:41:42 kanoj Exp $"
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
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/lock.h>
#include <sys/sysmp.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

/*
 * iosize.c
 *
 *	This file contains the routines and data structure that determine
 *	the optimal I/O size for a given device, file system, or file.
 *	The optimal I/O sizes are cached in a linked list pointed at
 *	by "iosize_list".
 *
 */

/*
 * The foll defines indicate which fs type entry the iosize cache
 * entry is defined for
 */
#define		FSTYPE_NONRT	0
#define		FSTYPE_RT	1
#define		FSTYPE_ALL	2

/* 
 * Data structure which contains the pairings between
 * file sytem device, and optimal I/O size.
 */
typedef	struct io_size_struct {
	int			iosize;
	dev_t			fs_dev;
	int			fs_type;
	struct io_size_struct	*next;
} io_size_struct_t;

/* 
 * Global pointer to the beginning of the linked list of file system 
 * device, optimal I/O size pairs.
 */
io_size_struct_t *iosize_list = NULL;

extern int get_all_disks_in_vdisk( dev_t, int, grio_disk_list_t [], int *);

int determine_grio_iosize_for_filesys( dev_t, int);
int set_grio_iosize_for_filesys(dev_t , int, int );

/*
 * get_iosize_struct
 *	This routine scans the iosize_list for a entry
 *	containing the give file system device. A pointer to the
 *	io_size_struct_t entry is returned to the caller if a match
 *	iss found. NULL is returned if a match is not found.
 *
 * RETURNS:
 *	a pointer to the io_size_struct_t if found
 *	NULL if not
 */
io_size_struct_t *
get_iosize_struct( dev_t fs_dev )
{
	io_size_struct_t 	*iosp;

	iosp = iosize_list;

	while ( (iosp) && (iosp->fs_dev != fs_dev) )  {
		iosp = iosp->next;
	}

	return( iosp );
}

/*
 * get_grio_iosize_for_filesys
 *	This routine returns the optimal I/O size of the file
 *	system specified by fs_dev to the caller. It first checks
 *	to see if the optimal I/O size of the file system is
 *	cached in the iosize_list by calling the get_iosize_struct() 
 *	routine. If the optimal I/O size of this file system is not
 *	cached, determine_grio_iosize_for_filesystem() is called
 *	to calculate the optimal I/O size. This value is stored in
 *	the iosize_list cache, by calling set_grio_iosize_for_filesystem(),
 *	and returned to the caller.
 *
 * RETURNS:
 *	 the optimal I/O size in bytes for the given file system
 *	 0 if file system is not found
 */
int
get_grio_iosize_for_filesys(dev_t fs_dev, int rt)
{
	int			iosize = 0, fstype;
	io_size_struct_t	*iosp;

	if (rt)
		fstype = FSTYPE_RT;
	else
		fstype = FSTYPE_NONRT;
	iosp = get_iosize_struct( fs_dev );

	if ((iosp == NULL) || (iosp->fs_type == !fstype)) {
		iosize = determine_grio_iosize_for_filesys( fs_dev, rt );
		if (iosp == NULL)
			set_grio_iosize_for_filesys( fs_dev, iosize, fstype);
		else {
			if (iosp->iosize != iosize) {
				iosize = -1;
				printf("Not all disks in the filesystem "
				       "0x%x have the same iosize.\n", fs_dev);
			} else
				iosp->fs_type = FSTYPE_ALL;
		}
	} else {
		iosize = iosp->iosize;
	}
	
	return( iosize );
}

/*
 * set_grio_iosize_for_filesys
 *	Scan the iosize_list looking for a match with the 
 *	given fs_dev. If match is found, update the optimal iosize 
 *	for the file system. If a match is not found, add a new
 *	io_size_struct_t to the iosize_list linked list containing
 *	the fs_dev and iosize.
 *
 * RETURNS:
 *	always returns 0
 */
int
set_grio_iosize_for_filesys(dev_t fs_dev, int iosize, int fstype)
{

	io_size_struct_t	*iosp;

	dbgprintf(9,("setting iosize to %d, for fs %x \n",iosize, fs_dev) );

	iosp = get_iosize_struct( fs_dev );
	assert (iosp == NULL);
	iosp = malloc( sizeof( io_size_struct_t ) );
	iosp->fs_dev = fs_dev;
	iosp->next   = iosize_list;
	iosize_list  = iosp;
	iosp->iosize = iosize;
	iosp->fs_type = fstype;
	return( 0 );
}

/*
 * determine_grio_iosize_for_filesys
 *	Compute the optimal I/O size for the file system.
 *	This is done by finding the optimal I/O  of the disks in
 *	the realtime subpartition of the file system.
 *	All the disks in the realtime subpartition must have the same
 *	optimal I/O size.
 *
 * RETURNS:
 *	the optimal I/O size for the file system
 */
int
determine_grio_iosize_for_filesys( dev_t fs_dev, int rt )
{
	int		 	arraycount = MAX_ROTOR_SLOTS, i, iosize = -1;
	device_node_t		*dvnp;
	grio_disk_list_t	list[MAX_ROTOR_SLOTS];

	/*	
	 * get all the disks associated with this file system
	 */
	if ( get_all_disks_in_vdisk( fs_dev, rt, list, &arraycount ) == 0 ) {

		/*
		 * get the optimal I/O size for each disk and compare
	  	 * it to the optimal I/O size of the other disks in the
		 * file system - they should all match.
		 */
		for (i = 0; i < arraycount; i++ ) {
			/*
			 * Init regular xlv disks.
			 */
			dvnp = disk_setup(list[i].physdev, -1);
			if (dvnp == NULL)		/* ignore request */
				return(-1);
			if (	(iosize != -1) &&
				(dvnp->resv_info.optiosize != iosize) ) {

				printf("Not all disks in the %s subvolume "
				       "of XLV volume 0x%x are the same.\n",
					rt? "realtime": "data", fs_dev);

				return( -1 );
			} 
			iosize = dvnp->resv_info.optiosize;
		}
	}

	/*
	 * failed to get the list of disks in the 
	 * relevant subpartition of the file system.
	 */
	assert( iosize != -1 );

	return( iosize );
}
