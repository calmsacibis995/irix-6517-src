
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.7 $"

#include <sys/param.h>
#include <sys/fsid.h>
#include <sys/stat.h>
#include <sys/fs/efs.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

union 	{
	struct efs fs;
	char	block[BBSIZE];
	} sb_un;

#define sblock sb_un.fs

int
main(int argc, char **argv)
{
	int fd;
	char *dev;
	struct stat buf;

	if (argc != 2) 
	{
		fprintf(stderr, "Usage: efs_fstyp special\n");
		exit(1);
	}

	dev = argv[1];
	if (stat(dev, &buf) < 0) 
	{
		fprintf(stderr, "efs_fstyp: cannot stat <%s>\n", dev);
		exit(1);
	}

	if (((buf.st_mode & S_IFMT) != S_IFBLK) &&
	    ((buf.st_mode & S_IFMT) != S_IFCHR)) 
	{
		fprintf(stderr,
		    "efs_fstyp: <%s> is not a block, or a character device\n",
		    dev);
		exit(1);
	}

	/* read the super block */
	if ((fd = open(dev, O_RDONLY)) < 0) 
	{
		fprintf(stderr, "efs_fstyp: cannot open <%s>\n", dev);
		exit(1);
	}
	lseek(fd, (long)EFS_SUPERBOFF, SEEK_SET); 
	if (read(fd, &sblock, BBSIZE) != BBSIZE) 
	{
		fprintf(stderr,"efs_fstyp: cannot read superblock.\n");
		exit(1);
	}

	if (!IS_EFS_MAGIC(sblock.fs_magic))
	{
		fprintf(stderr, "efs_fstyp: <%s> is not an extent file system\n", dev);
		exit(1);
	}
	printf("%s\n", FSID_EFS);
	exit(0);
	/* NOTREACHED */
}
