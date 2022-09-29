
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

#ident "$Revision: 1.12 $"

#include <stdio.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/fs/efs.h>
#include <diskinfo.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <ustat.h>

#define  BITSPERBYTE 		8

#define howmany(x,y)	(((x)+((y)-1))/(y))

union 	{
	struct efs efs;
	char buf[BBSIZE];
	} sb_un;

#define sblk sb_un.efs

unsigned gpbuf[BBSIZE / sizeof (unsigned)];

static int blkread(int fd, char *buf, efs_daddr_t b, int n);
static int blkwrite(int fd, char *buf, efs_daddr_t b, int n);
static int check_sb(struct efs *fs);
static long efs_checksum(struct efs *fs);
static long efs_oldchecksum(struct efs *fs);
static int init_inodes(int fd, int newcgs, struct efs *fs);
static void newbmsetup(char *bitmap, int newcgs, struct efs *fs);

int
main(int ac, char **av)
{
	int		c;
	efs_daddr_t	newsize = 0;
	efs_daddr_t	oldbmblock;
	int		oldbmblox;
	int		newbmsize;
	int		newbmblox;
	efs_daddr_t	newbmblock;
	int		newcgs;
	int		wrterr = 0;
	struct stat	sbuf;
	struct ustat	usbuf;
	int		fd;
	char		*path;
	char		*rawpath;
	char		*bitmap;
	long long	lnewsize;

	if (getuid() != 0)
	{
		fprintf(stderr,"must be superuser to use growfs.\n");
		exit(1);
	}	

	/* Check for correct usage */
	if( ac < 2 ) 
	{
ucrap:
		fprintf(stderr,"usage: growfs [-s blocks] special \n");
		exit(1);
	}

	/*
	 * Parse the arguments.
	 */

	while ((c = getopt(ac, av, "s:")) != EOF) 
	{
		switch (c) {
		case 's':
			newsize = strtol(optarg, 0, 0);
			break;
		default: goto ucrap;
		}
	}
	ac -= optind;
	if (ac <= 0) goto ucrap;
	path = av[optind];

	if (stat(path, &sbuf) < 0)
	{
		fprintf(stderr,"growfs: can't access %s\n", path);
		exit(1);
	}
	switch (sbuf.st_mode & S_IFMT)
	{
		case S_IFCHR :
				rawpath = path;
				break;
		case S_IFBLK :
				if ((rawpath = findrawpath(path)) == NULL)
				{
					fprintf(stderr,"growfs: can't find raw device corresponding to %s\n", path);
					exit(1);
				}
				break;
		default:

			fprintf(stderr,"growfs: %s is not a device.\n",path);
			exit(1);
	};

	/* We work always from the raw device, whether invoked on block or
	 * raw: this gives us reliable feedback about the success of writes.
	 * The idea being, we won't rewrite the changed superblock until
	 * everything else has succeeded.
	 * Before doing anything, check for a currently mounted filesystem.
	 */

	if (ustat(sbuf.st_rdev, &usbuf) >= 0)
	{
		fprintf(stderr,"growfs: %s contains a mounted filesystem.\n", path);
		exit(1);
	}

	if ((fd = open(rawpath, O_RDWR)) < 0)
	{
		fprintf(stderr,"growfs: can't open %s\n", rawpath);
		exit(1);
	}

	/* Read and check the superblock. We shan't bother to try the
	 * (possible) replicated superblock here; if the primary
	 * superblock is bad the filesystem's gotta be fixed up with
	 * fsck before we'll allow any attempt to grow it.
	 */

	if (blkread(fd, sb_un.buf, EFS_SUPERBB, 1))
	{
		fprintf(stderr,"growfs: can't read %s\n", rawpath);
		exit(1);
	}
	
	if (!IS_EFS_MAGIC(sblk.fs_magic)) 
	{
		fprintf(stderr,"growfs: no filesystem found on %s\n", path);
		exit(1);
	}

	if (check_sb(&sblk))
	{
		fprintf(stderr,"growfs: filesystem on %s needs cleaning.\n", path);
		exit(1);
	}

	/* If we have a newsize (explicitly given), check that we can read 
	 * from the implied maximum block. If not, we are to use all of
	 * available storage & must find the size of this.
	 */

	if (newsize)
	{
		if (blkread(fd, (char *)gpbuf, (newsize - 1), 1))
		{
			fprintf(stderr,"growfs: can't access %d blocks on %s.\n", newsize, path);
			exit(1);
		}
	}
	else if ((lnewsize = findsize(rawpath)) <= 0LL)
	{
		fprintf(stderr,"growfs: can't determine size of %s.\n", path);
		exit(1);
	}
	else if (lnewsize > 0x7fffffffLL)
	{
		newsize = 0x7fffffff;
		/* this provokes the warning below */
	}
	else
	{
		newsize = (int)lnewsize;
	}

	/* efs extent structure currently imposes a limit of 8 gig. Don't
	 * let fs grow beyond that...
	 */

	if (newsize > 0xffffff)
	{
		fprintf(stderr,"growfs: filesystems may not exceed 8 gigabytes: truncating\n");
		newsize = 0xffffff;
	}

	/* Preliminary write check: push back the unmodified block. */

	if (blkwrite(fd, sb_un.buf, EFS_SUPERBB, 1))
	{
		fprintf(stderr,"growfs: can't write to %s\n", rawpath);
		exit(1);
	}

	/* Now we're in business: we have the valid original superblock &
	 * the new size. Do initial sanity check of the new size.
	 */

	oldbmblox = (int)BTOBB(sblk.fs_bmsize);
	if (newsize <= (sblk.fs_size + oldbmblox))
	{
		fprintf(stderr,"growfs: not enough space to expand filesystem.\n");
		exit(1);
	}

	/* Determine new bitmap size. */

	newbmsize = howmany(newsize, BITSPERBYTE);
	newbmblox = (int)BTOBB(newbmsize);
	newbmblock = newsize - newbmblox - 1; /* -1 to allow repl sblock */

	/* calculate number of new cylinder groups we can fit between the
	 * end of the last existing one and the start of the new bitmap
	 */

	newcgs = (newbmblock - sblk.fs_size) / sblk.fs_cgfsize;
	if (newcgs <= 0)
	{
		fprintf(stderr,"growfs: not enough space to expand filesystem.\n");
		exit(1);
	}

	/* allocate space for the new bitmap; read the old bitmap into the
	 * beginning of this. Note that the old bitmap may be in the
	 * default place (block 2) for a pre-3.3 filesystem.
	 */

	if ((bitmap = malloc(newbmblox * BBSIZE)) == NULL)
	{
		fprintf(stderr,"growfs: can't allocate memory!\n");
		exit(1);
	}
	bzero(bitmap, newbmsize);
	oldbmblock = sblk.fs_bmblock ? sblk.fs_bmblock : EFS_BITMAPBB;
	if (blkread(fd, bitmap, oldbmblock, oldbmblox))
	{
		fprintf(stderr,"growfs: can't read %s\n", rawpath);
		exit(1);
	}

	/* Initialize the portions of the new bitmap referring to new free
	 * data blocks in the new cylinder groups, then write it.
	 */

	newbmsetup(bitmap, newcgs, &sblk);
	if (blkwrite(fd, bitmap, newbmblock, newbmblox))
	{
		fprintf(stderr,"growfs: can't write to %s\n", rawpath);
		exit(1);
	}

	/* Initialize the inode portions of the new cylinder groups. */

	if (init_inodes(fd, newcgs, &sblk) != 0)
	{
		fprintf(stderr,"growfs: can't write to %s\n", rawpath);
		exit(1);
	}

	/* New cylinder groups & bitmap successfully initialized. Now all
	 * we have to do is update the superblock & rewrite it.
	 * Note we force the magic number to EFS_NEWMAGIC to lock the new fs
	 * out of pre-3.3 systems; we also have to change fs_size to be
	 * the actual size (since old fsck didn't look at the magic no)!
	 */

	sblk.fs_magic = EFS_NEWMAGIC;
	sblk.fs_size = newsize;
	sblk.fs_ncg += newcgs;
	sblk.fs_bmsize = newbmsize;
	sblk.fs_tfree += (newcgs * (sblk.fs_cgfsize - sblk.fs_cgisize));
	sblk.fs_tinode += (newcgs * (sblk.fs_cgisize * EFS_INOPBB));
	sblk.fs_bmblock = newbmblock;
	sblk.fs_replsb = newsize - 1;
	sblk.fs_checksum = efs_checksum(&sblk);
	if (blkwrite(fd, sb_un.buf, EFS_SUPERBB, 1))
	{
		fprintf(stderr,"growfs warning: can't write primary superblock to %s\n", rawpath);
		wrterr = -1;
	}
	/* Now write the replicated superblock */
	if (blkwrite(fd, sb_un.buf, newsize - 1, 1))
	{
		fprintf(stderr,"growfs warning: can't write secondary superblock to %s\n", rawpath);
		exit(1);
	}
	exit(wrterr);
	/* NOTREACHED */
}

/* NOTE (daveh 8/29/89): changed to use the new readb & writeb system calls
 * to allow filesystems > 2 gig. UNLIKE fsck we do NOT try a fallback
 * to lseek/read/write: if we're on a pre-3.3 system we've no business
 * trying to grow files!
 */

/* blkread(): read n blocks from block offset b into buf */

static int
blkread(int fd, char *buf, efs_daddr_t b, int n)
{
	if (readb(fd, buf, b, n) == n)
		return (0);
	else
		return (-1);
}

/* blkwrite(): write n blocks to block offset b from buf */

static int
blkwrite(int fd, char *buf, efs_daddr_t b, int n)
{
	if (writeb(fd, buf, b, n) == n)
		return (0);
	else
		return (-1);
}

/* check_sb() does sanity checks on a superblock */

static int
check_sb(struct efs *fs)
{
	int d_area;

	if ( ((efs_checksum(fs) != fs->fs_checksum &&
	      efs_oldchecksum(fs) != fs->fs_checksum) ||
	     (fs->fs_dirty != EFS_CLEAN) ||
	     (fs->fs_ncg <= 0) ||
	     (fs->fs_sectors <= 0) ||
	     (fs->fs_bmsize <= 0) ||
	     (fs->fs_bmsize > (100 * (1024*1024 / BITSPERBYTE))) ||
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
	else
		return (-1);
	return (0);
}

/*
 * efs_checksum:
 *	- compute the checksum of the read in portion of the filesystem
 *	  structure.  
 */
static long
efs_checksum(struct efs *fs)
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
 * efs_oldchecksum:
 *	- this checksum suffers from shifting rather than rotating, thus
 *	  discarding information from all superblock members 32 half-words
 *	  or more before the checksum; since a zero-filled spare array lies
 *	  just before the checksum, much of the computed sum is 0
 */

static long
efs_oldchecksum(struct efs *fs)
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

/* Newbmsetup(): set portions of bitmap corresponding to the data blocks
 * of new cylinder groups to 'one's.
 */

static void
newbmsetup(char *bitmap, int newcgs, struct efs *fs)
{
	int i, len, doffset;

	len = fs->fs_cgfsize - fs->fs_cgisize; /* data blox per cg */
	for (i = fs->fs_ncg; i < (fs->fs_ncg + newcgs); i++)
	{
		doffset = fs->fs_firstcg 
			  + (i * fs->fs_cgfsize) 
			  + fs->fs_cgisize; 
		bfset(bitmap, doffset, len);	  
	}
}

/* init_inodes(): write zeros into the inode parts of each new cylinder
 * group. (Inodes are at the start of cylinder groups).
 *
 * A security precaution: we will do this BACKWARDS from the end of storage;
 * that way if a write fails (eg due to a bad block) we won't have trashed
 * the old bitmap & the old filesystem will still be good!
 */

static int
init_inodes(int fd, int newcgs, struct efs *fs)
{
	int i;
	char *ibuf;
	efs_daddr_t ioffset;

	/* allocate memory of the size of inodes per cg. */

	if ((ibuf = malloc(BBTOB(fs->fs_cgisize))) == NULL)
	{
		fprintf(stderr,"growfs: can't allocate memory!\n");
		exit(1);
	}
	bzero(ibuf, BBTOB(fs->fs_cgisize)); /* to be sure */
	for (i = (fs->fs_ncg + newcgs - 1); i >= fs->fs_ncg; i--)
	{
		ioffset = fs->fs_firstcg + (i * fs->fs_cgfsize); 
		if (blkwrite(fd, ibuf, ioffset, fs->fs_cgisize))
			return (-1);
	}
	free(ibuf);
	return (0);
}
