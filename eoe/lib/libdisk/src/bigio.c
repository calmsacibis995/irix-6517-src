 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.5 $"

#include <sys/param.h>
#include <errno.h>
#include <unistd.h>


/* Wrappers for the 'block read' and 'block write' system calls for 
 * accessing > 2 gig on a disk or logical volume.
 */
int
readb(int fd, void *buf, daddr_t blk, int nblks)
{
	int rval;
	int nextnblks;

	if (lseek64(fd, (off64_t)blk << BBSHIFT, SEEK_SET) < 0)
		return -1;
	rval = (int)read(fd, buf, BBTOB(nblks));
	if (rval >= 0)
		return (int)BTOBBT(rval);
	if (errno != ENOMEM || nblks < 128)
		return -1;
	nextnblks = nblks / 2;
	rval = readb(fd, buf, blk, nextnblks);
	if (rval < nextnblks)
		return rval;
	rval = readb(fd, (void *)((char *)buf + BBTOB(nextnblks)),
		     blk + nextnblks, nblks - nextnblks);
	if (rval >= 0)
		return nextnblks + rval;
	return -1;
}

int
writeb(int fd, void *buf, daddr_t blk, int nblks)
{
	int rval;
	int nextnblks;

	if (lseek64(fd, (off64_t)blk << BBSHIFT, SEEK_SET) < 0)
		return -1;
	rval = (int)write(fd, buf, BBTOB(nblks));
	if (rval >= 0)
		return (int)BTOBBT(rval);
	if (errno != ENOMEM || nblks < 128)
		return -1;
	nextnblks = nblks / 2;
	rval = writeb(fd, buf, blk, nextnblks);
	if (rval < nextnblks)
		return rval;
	rval = writeb(fd, (void *)((char *)buf + BBTOB(nextnblks)),
		      blk + nextnblks, nblks - nextnblks);
	if (rval >= 0)
		return nextnblks + rval;
	return -1;
}
