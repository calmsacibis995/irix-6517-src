/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"libdisk/diskheader.c $Revision: 1.18 $ of $Date: 1996/12/17 16:33:18 $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <errno.h>
#include <diskinfo.h>
#include <unistd.h>
#include <bstring.h>
#include <malloc.h>



int	valid_vh(struct volume_header *vhdr);
int	vhchksum(struct volume_header *vh);

/*	{get,put}diskheader() routines.
 *	get gets the volume header from the driver.
 *
 *	Daveh note 12/12/89: It does NOT, as in earlier versions, try to
 *	read it off the disk, since some disks have turned up where the
 *	on-disk vh is slightly wrong, & the driver sanity-checks it for
 *	us on open.
 *	Note that this implies that any caller of this must open the
 *	CHAR device; block dev won't work!
 *
 *	getheaderfromdisk() ALWAYS gets the vh from disk; see comments.
 *
 *	put notifies the driver first (in case vh partition has changed,
 *		or wasn't valid before) unless diskonly is no-zero,
 *		then writes it to disk.
 *		As stated in dvh.h, it is written to the first sector
 *		of every track in the first cylinder (or as close as
 *		we can tell, with SCSI...).
 *		No validity checking is done, it must be done
 *		before calling this routine.  The chksum field
 *		is generated first.
 *	First version created by Dave Olson @ SGI, 10/88
 */




/*	return 0 on success, -1 on error, or bad volume header */
int
getdiskheader(int fd, struct volume_header *vhdr)
{
	if(ioctl(fd, DIOCGETVH, vhdr) == -1)
		return -1;
	return valid_vh(vhdr);
}


/* always get vh from disk; used for programs that want to work
 * with files, as well as disk drives.  The CDROM build stuff
 * needs this.  Olson, 9/90
*/
int
getheaderfromdisk(int fd, struct volume_header *vhdr)
{
#define LARGE_VH_READ 4096
	char *tmp_header;
	int retval = -1;
        if((tmp_header = malloc(LARGE_VH_READ)) == NULL) {
		fprintf(stderr,"could not malloc space for volume header\n");	
		return -1;
	}

	if(lseek64(fd, 0, SEEK_SET) != 0  || 
	   read(fd, tmp_header, LARGE_VH_READ) != LARGE_VH_READ ) {
		return -1;
	}
	bcopy(tmp_header,vhdr,sizeof(*vhdr));

	retval = valid_vh(vhdr);
	free(tmp_header);
	return(retval);
}

int
valid_vh(struct volume_header *vhdr)
{	
	if(vhdr->vh_magic != VHMAGIC || vhchksum(vhdr)) {
		return -1;
	}
	return 0;
}



/* we no longer write multiple copies of volhdr, since nothing used
 * it, and we ended up needing more room in volhdr on existing customer
 * drives.
*/
int
putdiskheader(int fd, struct volume_header *vhdr, int diskonly)
{
	/* set magic and correct chksum */
	vhdr->vh_magic = VHMAGIC;
	vhdr->vh_csum = 0;
	vhdr->vh_csum = -vhchksum(vhdr);

	if(!diskonly && ioctl(fd, DIOCSETVH, vhdr) == -1)
		return -1;

	if(lseek64(fd, 0, SEEK_SET) == 0 &&
		write(fd, vhdr, sizeof(*vhdr)) == sizeof(*vhdr))
			return 0;
	return -1;
}

int
vhchksum(struct volume_header *vh)
{
	int sum;
	int *ip = (int *)vh;
	int n = sizeof(*vh) / sizeof(*ip);

	for(sum = 0; n > 0; n--)
		sum += *ip++;
	return sum;
}
