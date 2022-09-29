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

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.17 $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/uuid.h>
#include <sys/fs/efs.h>	
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/dvh.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ustat.h>	
#include <diskinfo.h>

/*
 * exit 0 - file system is unmounted and okay
 * exit 1 - file system is unmounted and needs checking
 * exit 2 - file system is mounted
 *          for root file system
 * exit 0 - okay
 * exit 1 - needs checking
 *
 * exit 3 - unexpected failures
 */

int dev;
char *arg, *blk, *raw;
struct stat stbd, stbr;
struct ustat usb;

static int e_checksb(struct efs *fs);
static long e_checksum(struct efs *fs);
static long e_oldchecksum(struct efs *fs);
static void fsstat_efs(void);
static void fsstat_xfs(void);

int
main(int argc, char **argv)
{
	int vhfd;

	if (argc != 2) {
		fprintf(stderr, "usage: fsstat special\n");
		exit(3);
	}
	if (stat(arg = argv[1], &stbd) < 0) {
		fprintf(stderr, "fsstat: cannot stat %s\n", arg);
		exit(3);
	}
	if (!S_ISBLK(stbd.st_mode) && !S_ISCHR(stbd.st_mode)) {
		fprintf(stderr, "fsstat: %s not a device\n", arg);
		exit(3);
	}
	if (S_ISBLK(stbd.st_mode)) {
		blk = arg;
		raw = findrawpath(blk);
	} else {
		raw = arg;
		blk = findblockpath(raw);
		if (!blk || stat(blk, &stbd) < 0 || !S_ISBLK(stbd.st_mode)) {
			fprintf(stderr, "fsstat: no block device for %s\n", raw);
			exit(3);
		}
	}

	/*
	 * Set the blocksize of the device to the blocksize
	 * specified in the volume header. This is necessary 
	 * when mounting efs CDROM on the 12x Toshiba (and 
	 * possibly newer) drives.  It also SHOULD handle 
	 * future devices that support larger than 512 byte blocks.
	 *
	 * Specifically coded the routines to not check return
	 * status for errors as we may be able to check the
	 * device even if we can't set or detect the block size.
	 */

	vhfd = disk_openvh (raw);
	if (vhfd >= 0) {
		{
			struct volume_header vhdr;
			if (getdiskheader(vhfd,&vhdr) == 0)	/* okay */
				(void)disk_setblksz(vhfd,vhdr.vh_dp.dp_secbytes);
		}
		close (vhfd);
	}

	if ((dev = open(raw, O_RDONLY)) < 0) {
		fprintf(stderr, "fsstat: cannot open %s\n", raw);
		exit(3);
	}
	stat("/", &stbr);

	fsstat_xfs();
	fsstat_efs();

	/* if we get here, nobody liked it */
	if (stbr.st_dev == stbd.st_rdev) {	/* root file system */
		fprintf(stderr, "fsstat: root %s is not a valid file system\n",
				arg);
	} else {
		fprintf(stderr, "fsstat: %s not a valid file system\n", arg);
	}
	exit(3);
	/* NOTREACHED */
}

union	{
	struct efs fs;
	char block[BBSIZE];
	} sb_un;

#define fb sb_un.fs

static void
fsstat_efs(void)
{

	lseek(dev, EFS_SUPERBOFF, 0);
	if (read(dev, &fb, BBSIZE) != BBSIZE) 
	{
		fprintf(stderr,"fsstat: cannot read %s\n", arg);
		exit(3);
	}
	if (!IS_EFS_MAGIC(fb.fs_magic))
		return;

	if (stbr.st_dev == stbd.st_rdev)  /* root file system */
	{	
		if (fb.fs_dirty != EFS_ACTIVE) 
		{
			fprintf(stderr, "fsstat: root file system needs checking\n");
			exit(1);
		} else 
		{
			fprintf(stderr, "fsstat: root file system okay\n");
			exit(0);
		}
	}

	if (ustat(stbd.st_rdev, &usb) == 0) 
	{
		fprintf(stderr, "fsstat: %s mounted\n", arg);
		exit(2);
	}

	if (e_checksb(&fb) == 0)
	{
		fprintf(stderr, "fsstat: %s okay\n", arg);
		exit(0);
	}
	else
	{
		fprintf(stderr, "fsstat: %s needs checking\n", arg);
		exit(1);
	}
}

/* Sanity check on superblock. Returns 0 if OK, -1 otherwise. Note that
 * check on fs_heads is now omitted, this was always a bogus field, not
 * used for anything. Duplicated from mount code.
 */

static int
e_checksb(struct efs *fs)
{
	int d_area;

	if ( ((e_checksum(fs) != fs->fs_checksum &&
	      e_oldchecksum(fs) != fs->fs_checksum) ||
	     (fs->fs_dirty != EFS_CLEAN) ||
	     (fs->fs_ncg <= 0) ||
	     (fs->fs_sectors <= 0) ||
	     (fs->fs_bmsize <= 0) ||
	     (fs->fs_bmsize > (100 * (1024*1024 / NBBY))) ||
	     (fs->fs_tinode < 0) ||
	     (fs->fs_tinode > fs->fs_cgisize * EFS_INOPBB * fs->fs_ncg) ||
	     (fs->fs_tfree < 0) ||
	     (fs->fs_tfree > (fs->fs_cgfsize - fs->fs_cgisize)*fs->fs_ncg)))
		return (-1);

	/* size parameterization is different in 3.3 & pre-3.3 */

	d_area = fs->fs_firstcg + fs->fs_cgfsize*fs->fs_ncg;

	if (fs->fs_magic == EFS_MAGIC) 
	{
	     	if (fs->fs_size != d_area)
			return (-1);
	}
	else if (fs->fs_magic == EFS_NEWMAGIC)
	{
		if ((d_area > fs->fs_size) || (d_area > fs->fs_bmblock))
			return (-1);
	}
	return (0);
}

/*
 * e_checksum:
 *	- compute the checksum of the read in portion of the filesystem
 *	  structure
 */

static long
e_checksum(struct efs *fs)
{
	ushort *sp;
	long checksum;

	checksum = 0;
	sp = (ushort *)fs;
	while (sp < (ushort *)&fs->fs_checksum) {
		checksum ^= *sp++;
		checksum = (checksum << 1) | (checksum < 0);
	}
	return (checksum);
}

/*
 * e_oldchecksum:
 *	- this checksum suffers from shifting rather than rotating, thus
 *	  discarding information from all superblock members 32 half-words
 *	  or more before the checksum; since a zero-filled spare array lies
 *	  just before the checksum, much of the computed sum is 0
 */

static long
e_oldchecksum(struct efs *fs)
{
	ushort *sp;
	long checksum;

	checksum = 0;
	sp = (ushort *)fs;
	while (sp < (ushort *)&fs->fs_checksum) {
		checksum ^= *sp++;
		checksum <<= 1;
	}
	return (checksum);
}

static void
fsstat_xfs(void)
{
	union	{
		xfs_sb_t sb;
		char block[BBSIZE];
	} xsb;

	lseek(dev, XFS_SB_DADDR * BBSIZE, 0);
	if (read(dev, (char *)&xsb, sizeof(xsb)) != sizeof(xsb)) 
	{
		fprintf(stderr,"fsstat: cannot read %s\n", arg);
		exit(3);
	}
	if (xsb.sb.sb_magicnum != XFS_SB_MAGIC)
		return;

	if (stbr.st_dev == stbd.st_rdev)  /* root file system */
	{	
		fprintf(stderr, "fsstat: root file system okay\n");
		exit(0);
	}

	if (ustat(stbd.st_rdev, &usb) == 0) 
	{
		fprintf(stderr, "fsstat: %s mounted\n", arg);
		exit(2);
	}

	fprintf(stderr, "fsstat: %s okay\n", arg);
	exit(0);
}
