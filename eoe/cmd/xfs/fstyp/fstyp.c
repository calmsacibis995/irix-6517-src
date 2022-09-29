
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.5 $"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fsid.h>
#include <sys/param.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>

union 	{
	struct xfs_sb fs;
	char	block[BBSIZE];
	} sb_un;

#define sblock sb_un.fs

main(argc,argv)
	int argc;
	char *argv[];
{

	int fd;
	char *dev;
	struct stat buf;

	if (argc != 2) 
	{
		fprintf(stderr, "Usage: xfs_fstyp special\n");
		return 1;
	}

	dev = argv[1];
	if (stat(dev, &buf) < 0) 
	{
		fprintf(stderr, "xfs_fstyp: cannot stat <%s>\n", dev);
		return 1;
	}

	if (((buf.st_mode & S_IFMT) != S_IFBLK) &&
	    ((buf.st_mode & S_IFMT) != S_IFCHR)) 
	{
		fprintf(stderr,
		    "xfs_fstyp: <%s> is not a block, or a character device\n",
		    dev);
		return 1;
	}

	/* read the super block */
	if ((fd = open(dev, O_RDONLY)) < 0) 
	{
		fprintf(stderr, "xfs_fstyp: cannot open <%s>\n", dev);
		return 1;
	}
	lseek(fd, XFS_SB_DADDR * BBSIZE, SEEK_SET); 
	if (read(fd, &sblock, BBSIZE) != BBSIZE) 
	{
		fprintf(stderr,"xfs_fstyp: cannot read superblock.\n");
		return 1;
	}

	if (sblock.sb_magicnum != XFS_SB_MAGIC)
	{
		fprintf(stderr, "xfs_fstyp: <%s> is not an XFS file system\n", dev);
		return 1;
	}
	printf("%s\n", FSID_XFS);
	return 0;
}
