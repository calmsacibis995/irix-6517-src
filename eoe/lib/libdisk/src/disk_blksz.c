/**************************************************************************
 *									  *
 * 		 Copyright (C) 1998, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/scsi.h>
#include <sys/dvh.h>
#include <sys/dksc.h>
#include <sys/dkio.h>
#include <sys/conf.h>
#include <diskinfo.h>
#include <errno.h>

extern int errno;
extern char *path_to_vhpath(char *path, char *destpath, int *destlen);

/********************************************************************************
*                                                                               *
*   disk_openvh									*
*                                                                               *
*   Open the volume header device associated with path specified.		*
*                                                                               *
*   Entry:                                                                      *
*       Currently called by mount and fsstat.					*
*                                                                               *
*   Input:                                                                      *
*       path - path name of a character or block device.			*
*                                                                               *
*   Output:                                                                     *
*       File opened, file descriptor returned, or return -1 on error.		*
*	errno is set by called routines.					*
*                                                                               *
********************************************************************************/


int
disk_openvh (char *path)
{

	
	char *raw, *blk;
	struct stat stbd;
	char vhpath [MAXDEVNAME];
	int vh_length;

	/*
	** Find the volume header path to the device and open it.
	** Assure both character and block devices exist.
	*/

	if (stat(path, &stbd) < 0) {
		return -1;
	}

	if (!S_ISBLK(stbd.st_mode) && !S_ISCHR(stbd.st_mode)) {
		return -1;
	}

	if (S_ISBLK(stbd.st_mode)) {
		raw = findrawpath(path);
	} 
	else {
		raw = path;
		blk = findblockpath(raw);
		if (!blk || stat(blk, &stbd) < 0 || !S_ISBLK(stbd.st_mode)) {
			return -1;
		}
	}

	if (path_to_vhpath (raw, vhpath, &vh_length)) {
		return open (vhpath, O_RDONLY);
	}

	return -1;
}

/********************************************************************************
*                                                                               *
*   disk_openraw                                                                *
*                                                                               *
*   Open the raw disk device associated with path specified.  Assure that there	*
*   is an associated block device.						*
*                                                                               *
*   Entry:                                                                      *
*                                                                               *
*   Input:                                                                      *
*       path - path name of a character or block device.			*
*                                                                               *
*   Output:                                                                     *
*       File opened, file descriptor returned, or return -1 on error.		*
*	errno is set by called routines.					*
*                                                                               *
********************************************************************************/


int
disk_openraw (char *path)
{

	
	char *raw, *blk;
	struct stat stbd;

	/*
	** Find the raw path to the device and open it.
	** Assure both character and block devices exist.
	*/

	if (stat(path, &stbd) < 0) {
		return -1;
	}

	if (!S_ISBLK(stbd.st_mode) && !S_ISCHR(stbd.st_mode)) {
		return -1;
	}

	if (S_ISBLK(stbd.st_mode)) {
		raw = findrawpath(path);
	} 
	else {
		raw = path;
		blk = findblockpath(raw);
		if (!blk || stat(blk, &stbd) < 0 || !S_ISBLK(stbd.st_mode)) {
			return -1;
		}
	}

	return open(raw, O_RDONLY);
}

/********************************************************************************
*                                                                               *
*   disk_getblksz                                                               *
*                                                                               *
*   Get the block size of a raw disk device.					*
*                                                                               *
*   Entry:                                                                      *
*       Currently called by mount and fsstat.					*
*                                                                               *
*   Input:                                                                      *
*       dev - file descriptor of open raw disk device.				*
*                                                                               *
*   Output:                                                                     *
*       Returns the device blocksize.						*
*	Returns -1 on detectable errors.  errno as set by called routines.	*
*										*
*   Note:									*
*	errno may not have an appropriate value if the ioctl completes but the	*
*	device did not return a block descriptor.				*
*                                                                               *
********************************************************************************/


int
disk_getblksz (int dev)
{

	struct	dk_ioctl_data dki;
	struct	mode_sense_data mode_data;

	int	old_blocksize = -1;

	/*
	** Query the existing block size.
	*/

	dki.i_addr = (caddr_t)&mode_data;
	dki.i_len = sizeof(mode_data);
	dki.i_page = 0;

	if (ioctl(dev, DIOCSENSE, &dki) != -1) {

		if (mode_data.bd_len == 8) {	/* block descriptor */

			old_blocksize = mode_data.block_descrip[5] * 65536 +
					mode_data.block_descrip[6] *   256 +
					mode_data.block_descrip[7];

		}
	}

	return old_blocksize;
}

/********************************************************************************
*                                                                               *
*   disk_setblksz                                                               *
*                                                                               *
*   Set the block size of a raw disk device as specified.			*
*                                                                               *
*   Entry:                                                                      *
*       Currently called by mount and fsstat.					*
*                                                                               *
*   Input:                                                                      *
*       dev - file descriptor of open raw disk device.				*
*	new_blocksize - blocksize to which the device is set			*
*                                                                               *
*   Output:                                                                     *
*       Returns the device blocksize on completion of the operation.  This may	*
*	or may not correspond to the requested new_blocksize if the device	*
*	doesn't report an error but doesn't change the size.			*
*										*
*	Returns -1 on detectable errors.  errno as set by called routines.	*
*                                                                               *
*   Note:									*
*	errno may not have an appropriate value if an ioctl completes but the	*
*	device did not return a block descriptor.				*
*                                                                               *
********************************************************************************/


int
disk_setblksz (int dev, int new_blocksize)
{
	int	old_blocksize;
	int	blocksize = -1;

	struct	dk_ioctl_data dki;
	struct	mode_sense_data mode_data;

	/*
	** Query the existing block size.
	*/

	dki.i_addr = (caddr_t)&mode_data;
	dki.i_len = sizeof(mode_data);
	dki.i_page = 0;

	if (ioctl(dev, DIOCSENSE, &dki) != -1) {

		if (mode_data.bd_len == 8) {	/* block descriptor */

			old_blocksize = mode_data.block_descrip[5] * 65536 +
					mode_data.block_descrip[6] *   256 +
					mode_data.block_descrip[7];

			if (old_blocksize != new_blocksize) {

				/*
				** Change the block size.
				*/

				if (ioctl(dev, DIOCSELFLAGS, 0) != -1) {

					mode_data.sense_len = 0;

					mode_data.block_descrip[5] = new_blocksize / 65536;
					mode_data.block_descrip[6] = new_blocksize /   256;
					mode_data.block_descrip[7] = new_blocksize;

					dki.i_addr = (caddr_t)&mode_data;
					dki.i_len = sizeof(mode_data);
					dki.i_page = 0;

					if (ioctl(dev, DIOCSELECT, &dki) != -1) {
						/*
						** Double check the device.
						*/
						blocksize = disk_getblksz (dev);
					}
				}
			}
			else {
				blocksize = old_blocksize;
			}
		}
	}

	return blocksize;
}
