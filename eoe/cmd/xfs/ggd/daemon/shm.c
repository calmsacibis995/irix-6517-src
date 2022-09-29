#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/shm.c,v 1.20 1997/03/14 19:14:46 singal Exp $"

/*
 * shm.c
 *
 *	This file contains routines which are part of the 
 *	guarantee granting daemon (ggd).
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */


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
#include <sys/scsi.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <sys/ktime.h>
#include <sys/sysmp.h>
#include <sys/ipc.h>
#include <sys/invent.h>
#include <sys/shm.h>
#include "ggd.h"
#include "griotab.h"

/*
 * Global variables
 */
ggd_info_summary_t	*ggd_info;
int			ggd_node_count;
int			ggd_nodes_allocated;

/*
 * Externally defined routines.
 */
extern void ggd_fatal_error( char *errstr );


/*
 * allocate_ggd_info_node() 
 *	Allocate a ggd_node_summary_t structure from the ggd_info 	
 * 	shared memory segment.
 * 
 *  RETURNS:
 *	a pointer to the newly allocate ggd_node_summary_t structure 
 */
ggd_node_summary_t *
allocate_ggd_info_node( void  )
{
	ggd_node_summary_t	*ggd_node;

	ggd_node 		= &(ggd_info->node_info[ggd_nodes_allocated]);

	ggd_nodes_allocated++;

	return( ggd_node );
}

/* 
 * get_ggd_info_node_from_dev()
 *	Return the ggd_info node that corresponds to the given
 *	device node.
 *
 * RETURNS:
 *	a pointer to the ggd_info node for the given device.
 */
ggd_node_summary_t *
get_ggd_info_node_from_dev( device_node_t *devnp ) 
{
	return( &(ggd_info->node_info[devnp->ggd_info_node_num]) );
}


/*
 * initialize_ggd_info_node()
 *	Initialize the contents of a ggd_info node from the contents
 *	of a device node.
 *
 * RETURNS:
 *	always returns 0
 */
int
initialize_ggd_info_node( device_node_t *devnp, ggd_node_summary_t *ggd_node)
{
	int		rate;
	__int64_t	lrate;

	ggd_node->node_num = devnp->node_number;

	lrate = devnp->resv_info.maxioops * 
		((__int64_t)devnp->resv_info.optiosize);
	rate = lrate/1024;

	ggd_node->max_io_rate		= rate;
	ggd_node->remaining_io_rate	= rate;

	ggd_node->optiosize		= devnp->resv_info.optiosize;
	ggd_node->num_opt_ios		= devnp->resv_info.maxioops;

	devnp->ggd_info_node_num = ggd_nodes_allocated - 1;

	ggd_node->devtype = 0;
	ggd_node->devtype = devnp->flags;
	return( 0 );
}

/*
 * update_ggd_info_node 
 *	Update the remaining rate info in the ggd_info node that
 * 	corresponds to the given device node.
 * 
 *  RETURNS:
 * 	always returns 0
 */
int
update_ggd_info_node( device_node_t *devnp, int io_rate )
{
	ggd_node_summary_t *ggd_node;

	ggd_node = get_ggd_info_node_from_dev( devnp );
	ggd_node->remaining_io_rate   =  io_rate;
	return( 0 );
}

int
init_ggd_info_node(device_node_t *devnp)
{
	ggd_node_summary_t	*ggd_node = allocate_ggd_info_node();

	initialize_ggd_info_node(devnp, ggd_node);
	return(0);
}


/*
 * create_ggd_info_segment() 
 *	Try to connect to the ggd_info shared memory segment. If one
 * 	exists, verify that the magic number is correct and then bump
 * 	the revision count. This is so utilities (such as PCP) that look
 *	at this segment know that it may have changed. 
 *
 * 	If the segment does not exists create it.
 *
 * 	If the segment exists but is the wrong size, delete the old
 *	segment and create a new one. 
 * 
 * 
 * RETURNS:
 *	0 on success
 *	-1 on failure
 */
int
create_ggd_info_segment( int numnodes )
{
	int         shmid, segsize, exists;
	key_t       key = GGD_INFO_KEY;               /* magic for ggd_info */

retry:
	segsize = sizeof(ggd_info_summary_t) + 
			(numnodes*sizeof(ggd_node_summary_t));

	/*
	 * see if shared memory segment already exits
	 */
	shmid = shmget(key, segsize, 0644 );
	if (shmid >= 0 ) {
		/*
		 * shared memory segment exists
	 	 */
		exists = 1;
	} else {
		exists = 0;
		shmid = shmget(key, segsize, 0644 | IPC_CREAT);

		if (shmid < 0) {
			if ( errno == ENOSPC ) {
				return( -1 );
			} else if ( errno == EEXIST ) {
				exists = 1;
				shmid = shmget(key, segsize, 0644);
				if (shmid < 0 ) {
					ggd_fatal_error("Could not get shmid "
							"when it exists.");
				}
			} else if ( errno == EINVAL ) {
				/*
			 	 * delete shared memory seg and try again
				 */
				printf("Resizing shared memory segment.\n");
				shmid = shmget(key, 0, 0644);
				if ( shmid < 0 ) {
					ggd_fatal_error("Could not get shmid "
							"when deleting.");
				}

				shmid = shmctl( shmid, IPC_RMID );
				if ( shmid != 0 ) {
					ggd_fatal_error("Could not delete "
							"shmid.");
				} else {
					goto retry;
				}
			}
		}
	}

	/*
	 * attach to segment for reading and writing
	 */
	ggd_info = (ggd_info_summary_t *)shmat(shmid, (void *)0, 0);

	if (ggd_info == NULL ) {
		ggd_fatal_error("Could not attach to shared memory segment");
	}

	/*
	 * if the memory segment already exists,	
 	 * bump the rev_count to indicate that the daemon has restarted
	 */
	if (exists) {
		if ( ggd_info->ggd_info_magic != GGD_INFO_MAGIC ) {
			ggd_fatal_error("Invalid magic number "
					"in shared memory segment.\n");
		}
		ggd_info->rev_count += 1;
	} else {
		bzero( ggd_info, segsize);
		ggd_info->ggd_info_magic = GGD_INFO_MAGIC;
		ggd_info->rev_count =  0;
	}

	/*
	 * initialize the ggd_info segment
	 */
	ggd_info->num_nodes 		= numnodes;
	ggd_info->max_cmd_count    	= GRIO_MAX_COMMANDS;
	ggd_info->active_cmd_count 	= GRIO_ACTIVE_COMMANDS;


	return( 0 );
}
