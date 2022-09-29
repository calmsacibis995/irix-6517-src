#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/vdisk_cache.c,v 1.22 1997/02/02 00:04:09 kanoj Exp $"


/*
 * vdisk_cache.c
 *
 *
 *
 */

#include <time.h>
#include <bstring.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/bsd_types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/cred.h>
#include <sys/scsi.h>
#include <sys/syssgi.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_inum.h>
#include <sys/grio.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dkio.h>
#include "ggd.h"


extern int issue_xlv_ioctl( dev_t, int, char *);
extern void dump_subvol_info( xlv_tab_subvol_t *);

#define	NUM_SUBVOLS_PER_PLEX	50
#define	NUM_EXTENTS_PER_FILE	XFS_MAX_INCORE_EXTENTS
#define DISK_PARTS		XLV_MAX_DISK_PARTS_PER_VE
#define NUM_OPEN_VDISKS		2000

#define TMPVDEV			"/tmp/grio_vdev"

typedef struct vdev_lookup {
        dev_t   		vdev;
	xlv_tab_subvol_t 	*subvol;
        int     		fd;
} vdev_lookup_t;


STATIC vdev_lookup_t    vdev_table[NUM_OPEN_VDISKS];
STATIC int              vdev_count = 0;

/*
 * get_vdev()
 *	Return the dev_t of the vdisk indicated by the index.
 *	Increment the index before returning.
 *
 * RETURNS:
 *	dev_t of file system referenced by index
 *	0 if index is out of range
 *
 */
dev_t
get_vdev( int *index )
{
	dev_t	vdev;

	if ( (*index) < vdev_count ) {
                vdev = vdev_table[ (*index) ].vdev;
		(*index)++;
		return( vdev );
	}
	return( 0 );
}

/*
 * get_vdisk_cache_index()
 *	This routine returns the index into the vdev_table that
 * 	corresponds to the entry for the given vdev.
 *
 * RETURNS:
 *	index number on success
 *	-1 on failure
 */
STATIC int
get_vdisk_cache_index( dev_t vdev )
{
	int	i;

        /*
         * Scan table to vdisk index 
         */
        for (i = 0; i < NUM_OPEN_VDISKS; i++) {
                if (vdev_table[i].vdev == vdev) {
			return ( i );
                }
        }
	return( -1 );
}

/*
 * get_vdisk_fd_from_cache()
 *	This routine finds the vdisk cache entry for the given vdev
 *	and returns the open file descriptor.
 *
 * RETURNS:
 *	file descriptor on success
 *	-1 on failure
 */
STATIC int
get_vdisk_fd_from_cache( dev_t vdev)
{
        int     i;

	i = get_vdisk_cache_index( vdev);
	if (i == -1)
		return( -1 );
	else
        	return(vdev_table[i].fd);

}

/*
 * get_vdisk_subvol_from_cache()
 *	This routine finds the vdisk cache entry for the given vdev
 *	and returns the subvolume information.
 *
 *
 * RETURNS:
 *	pointer to subvol info on success
 *	-1 on failure.
 */
STATIC xlv_tab_subvol_t *
get_vdisk_subvol_from_cache( dev_t vdev)
{
        int     i;

	i = get_vdisk_cache_index( vdev );
	if (i == -1) {
		return( (xlv_tab_subvol_t *)(-1) );
	} else {
		if (!vdev_table[i].subvol)
			return( (xlv_tab_subvol_t *)(-1) );
                return(vdev_table[i].subvol);
	}
}

/*
 * add_vdisk_fd_to_cache()
 *	This routine creates a vdisk cache entry for the given vdisk vdev.
 *	It is a critical error to try to add the same vdev device to
 *	the cache twice.
 *
 *
 */
STATIC int
add_vdisk_fd_to_cache(dev_t vdev)
{
        char    dev_name[25];
	int 	fd, i, slot;

	
	i = get_vdisk_cache_index( vdev );

        if ((i != -1) || (vdev_count == NUM_OPEN_VDISKS)) {
                /*
		 * Table overflow.
                 */
                printf("GGD: Internal vdev table is full !\n");
                exit(-1);
        }

        /*
         * Scan table to find unused vdisk index.
         */
        for (i = 0; i < NUM_OPEN_VDISKS; i++) {
                if (vdev_table[i].vdev == NULL) {
			slot = i;
			break;
                }
        }

        sprintf(dev_name,"%s.%d",TMPVDEV, slot);
        unlink( dev_name );

        /*
         * Make virtual disk dev node.
         */
        if (mknod( dev_name, (mode_t)(S_IFCHR), vdev)) {
                printf("Could not create %s.\n",dev_name);
                return( -1 );
        }

        if ((fd = open( dev_name, O_RDWR )) == -1 ) {
                printf("Could not open %s. Error %d \n",dev_name, errno);
                unlink( dev_name );
                return( -1 );
        }

        vdev_table[slot].fd   = fd;
        vdev_table[slot].vdev = vdev;

        vdev_count++;

        return(fd);
}


/*
 * get_subvol_space()
 *      This routine allocates memory space to hold the virtual disk
 *      subvolumne information.
 *
 * RETURNS:
 *      1 on success
 *      0 on failure
 */
xlv_tab_subvol_t *
get_subvol_space( void )
{      
        xlv_tab_plex_t		*plex;
        xlv_tab_subvol_t	*subvol;
        int			i, plexsize;

        subvol = malloc(sizeof(xlv_tab_subvol_t));
        bzero(subvol, sizeof(xlv_tab_subvol_t));
        plexsize = sizeof(xlv_tab_plex_t) +
                        (sizeof(xlv_tab_vol_elmnt_t) * NUM_SUBVOLS_PER_PLEX );
        for ( i = 0; i < XLV_MAX_PLEXES; i ++ ) {
                plex = malloc( (size_t)plexsize );
                bzero(plex, plexsize );
                subvol->plex[i] = plex;
        }
        return( subvol );
}

/*
 * get_vdisk_fd()
 *	This routine returns the open file descriptor corresponding
 *	to the given virtual disk vdev. If the vdev is not already 
 *	in the vdisk cache it is added.
 *
 *
 */
int
get_vdisk_fd( dev_t vdev )
{
	int	fd;

	if ( (fd = get_vdisk_fd_from_cache( vdev )) == -1) {
		fd = add_vdisk_fd_to_cache( vdev );
	}
	return(fd);
}

/*
 * add_vdisk_subvol_to_cache()
 *	This routine updates the subvol cache information of the 
 *	vdev device if it is already cached, and creates the vdisk cache
 *	entry otherwise.
 *
 *
 */
xlv_tab_subvol_t *
add_vdisk_subvol_to_cache( dev_t vdev )
{
	xlv_tab_subvol_t	*subvol;
	int 			i;


        if ((i = get_vdisk_cache_index( vdev)) == -1) {
		get_vdisk_fd( vdev );
		i = get_vdisk_cache_index( vdev );
	}

	if ((vdev_table[i].vdev != vdev)) {
		printf("Internal error getting subvol info.\n");
		exit(-1);
	}

	subvol = get_subvol_space();
	issue_xlv_ioctl( vdev, DIOCGETSUBVOLINFO, (char *)subvol);

	vdev_table[i].subvol = subvol;
	return(subvol);
}


/*
 * get_subvol_info()
 *	This routine returns the subvol information for the given vdev 
 *	device from the cache. If the device is not cached a cache 
 *	entry is created.
 *
 */
xlv_tab_subvol_t *
get_subvol_info( dev_t vdev )
{
	xlv_tab_subvol_t	*subvol;


	if ((subvol = get_vdisk_subvol_from_cache( vdev )) == 
					((xlv_tab_subvol_t *)(-1))) {
		subvol = add_vdisk_subvol_to_cache( vdev );
	}
	return(subvol);
}


/*
 * vdisk_purge_enty()
 *	This routine removes the cached xlv information for the
 *	given device.
 *
 * RETURNS:
 *	0 on success
 */
int
vdisk_purge_cache_entry( dev_t vdev )
{
	xlv_tab_subvol_t	*subvol;
	char			dev_name[25];
	int			index = -1, i;

	if ((index = get_vdisk_cache_index( vdev )) != -1) {
	
		/*
		 * Free subvol info if necessary.
		 */
		if ((subvol = vdev_table[index].subvol) != NULL ) {
	        	for ( i = 0; i < XLV_MAX_PLEXES; i ++ ) {
				free(subvol->plex[i]);
       			}
        		free(subvol);
			vdev_table[index].subvol = NULL;
		}

		/*
		 * Close file descriptor.
		 *
		 */
		close(vdev_table[index].fd);

		vdev_table[index].fd   = -1;
		vdev_table[index].vdev = 0;

		vdev_count--;
	
		/*
		 * Remove file entry.
		 */
		sprintf(dev_name,"%s.%d",TMPVDEV, index);
		unlink( dev_name );

	}
	return( 0 );
}
