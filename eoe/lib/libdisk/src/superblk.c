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

#ident	"libdisk/diskinfo.c $Revision: 1.27 $ of $Date: 1997/01/01 00:04:06 $"

/*	getsuperblk(), putsuperblk(), labelfs() routines.
	First version created by Dave Olson @ SGI, 10/88
*/


#include <sys/types.h>
#include <sys/sema.h>
#include <sys/param.h>	/* for BBTOB() macros */
#include <sys/buf.h>
#include <sys/uuid.h>
#include <diskinvent.h>
#include <sys/iograph.h>
/*
 * EFS include files.
 */
#include <sys/fs/efs_ino.h>
#include <sys/fs/efs_sb.h>
#include <sys/fs/efs_fs.h>
/*
 * XFS include files.
 */
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>

#include <sys/stat.h>
#include <sys/major.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <mntent.h>
#include <fcntl.h>
#include <diskinfo.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <bstring.h>

static char fname[32];	/* filesystem 'name' from super block */

int fs_readonly;	/* set as a side effect by getmountpt(), IF the
	mount point is found from fstab or mtab */

static	__int32_t efs_checksum(struct efs *);
static int chksb_efs(register struct efs *);
static int chksb_xfs(register xfs_sb_t *);
int	valid_efs(struct efs *, int);
int	valid_xfs(xfs_sb_t *, long long);
int	getsuperblk_efs(char *, struct efs *);
int	getsuperblk_xfs(char *, xfs_sb_t *);
int	readsuperblk_efs(int, struct efs *);
int	readsuperblk_efs_relative(int, struct efs *);
int	readsuperblk_efs_gen(int, struct efs *, int);
int	readsuperblk_xfs(int, xfs_sb_t *);
int     readsuperblk_xfs_relative(int, xfs_sb_t *);
int     readsuperblk_xfs_gen(int, xfs_sb_t *, int);

/*	
 *	Read from the given disk device and determine what type of
 *	file system is present, if any. If a file system exists, 
 * 	determine if the size of the file system is approximately the 
 * 	same as what the caller has specified.
 *
 *	Return 1 if an XFS or EFS file system is present and the size of
 *	the file system is consistent, return 0 otherwise.
 * 
 * NB. 
 *	This interface will have to be expanded so that the caller
 *	can specify a size of larger than 32 bits. 
 * 
 */

int
has_fs(char *dskname, __int64_t size)
{
	int		ret = 0;
	xfs_sb_t   	xfs_sblk;
	struct efs 	efs_sblk;

	if ( getsuperblk_efs(dskname, &efs_sblk) )  {
		if ( valid_efs(&efs_sblk, (int)size) == 1) {
			ret = 1;
		} 
	} else if (getsuperblk_xfs(dskname, &xfs_sblk) )  {
		if ( valid_xfs(&xfs_sblk, size) == 1) {
			ret = 1;
		}
	}

	return ret ;
}

int
has_xfs_fs(char *dskname)
{
	xfs_sb_t xfs_sblk;

	if (getsuperblk_xfs(dskname, &xfs_sblk)) {
		/* returns 1 if bad */
		if (chksb_xfs(&xfs_sblk))
			return 0;
		return 1;
	}
	return 0;
}

int
has_efs_fs(char *dskname)
{
	struct efs efs_sblk;

	if (getsuperblk_efs(dskname, &efs_sblk)) {
		/* returns 1 if bad */
		if (chksb_efs(&efs_sblk))
			return 0;
		return 1;
	}
	return 0;
}


/* 
 * Check EFS superblock for validity.
 */
static int
chksb_efs(register struct efs *sb)
{
	return (!IS_EFS_MAGIC(sb->fs_magic))	||
			sb->fs_sectors < 0 	|| 
			sb->fs_size < 0 	|| 
		sb->fs_checksum != efs_checksum(sb);
}

/* 
 * check XFS superblock for validity 
 */
static int
chksb_xfs(register xfs_sb_t *sb)
{
	return (sb->sb_magicnum != XFS_SB_MAGIC) ||
	       !XFS_SB_GOOD_SASH_VERSION(sb);
}

/* 
 * This function attempts to determine whether a real EFS filesystem 
 * of the specified size exists on the given partition character special 
 * file.  It checks the size because of the possiblity of overlapping 
 * partitions that start at the same place (i.e, standard 0 vs 7).
 * Returns -1 if not a good super block, 1 if ok, and 0 if superblock
 * ok, but size disagrees.
 */
int
valid_efs(struct efs *sblk, int size)
{
	*fname = '\0';	/* in case of errors */

	if (chksb_efs(sblk))
		return -1;

	if (size < sblk->fs_size)
		return 0;

	/* preserve fname for possible later use by getmountpt() */
	strncpy(fname, sblk->fs_fname, sizeof(sblk->fs_fname));
	return 1;
}

/* 
 * This function attempts to determine whether a real XFS filesystem 
 * of the specified size exists on the given partition character special 
 * file.  It checks the size because of the possiblity of overlapping 
 * partitions that start at the same place (i.e, standard 0 vs 7).
 * Returns -1 if not a good super block, 1 if ok, and 0 if superblock
 * ok, but size disagrees.
 */

int
valid_xfs(xfs_sb_t *sblk, long long size)
{
	long long fssize;

	*fname = '\0';	/* in case of errors */

	if (chksb_xfs(sblk)) {
		return -1;
	}

	/*
	 * Size of XFS file system in disk blocks.
 	 */
	fssize = (long long)(sblk->sb_dblocks * sblk->sb_blocksize)/DEV_BSIZE;

	if (size < fssize)
		return 0;

	/*  
	 * preserve fname for possible later use by getmountpt() 
	 */
	strncpy(fname, sblk->sb_fname, sizeof(sblk->sb_fname));

	return 1;
}

/*	
 * Get the EFS superblock for the given partition.  The magic # is
 * checked, as well as the number of sectors, size, and checksum.
 * Returns 1 on success, 0 on error.
 */
int
getsuperblk_efs(char *dskname, struct efs *sblk)
{
	int fd;

	if ((fd = open(dskname, O_RDONLY)) < 0)
		return 0;

	if (readsuperblk_efs(fd, sblk)) {
		close(fd);
		return 0;
	}

	close(fd);
	return 1;
}

/*	
 * Get the XFS superblock for the given partition.  The magic # is 
 * checked.
 * Returns 1 on success, 0 on error.
 */
int
getsuperblk_xfs(char *dskname, xfs_sb_t *sblk)
{
	int fd;

	if ((fd = open(dskname, O_RDONLY)) < 0)
		return 0;

	if (readsuperblk_xfs(fd, sblk)) {
		close(fd);
		return 0;
	}

	close(fd);
	return 1;
}

/* readsuperblk_efs_gen()
 *
 * General routine to read the EFS superblock, seeking from the
 * current offset or from 0.
 *
 * Seeking from the current offset allows callers to step through the
 * disk by seeking to the start of each partition, and then calling
 * this routine to get the superblock without all callers having to
 * know where in the partition the superblock is located.
 *
 * If the partition does not contain an EFS superblock, the file pointer
 * is returned to its original location.
 *
 * ARGUMENTS:
 *  fd - open file descriptor of partition or volume to read from
 *  sblk - pointer to efs superblock to store superblock into
 *  relative - seek from 0 or current file pointer offset
 *
 * RETURN VALUE:
 *  0 - superblock successfully read
 *  -1 - failure
 *
 * ASSOCIATED ROUTINES:
 *  readsuperblk_efs() - calls readsuperblk_efs_gen() with relative = 1
 *  readsuperblk_efs_relative()
 *                     - calls readsuperblk_efs_gen() with relative = 0
 */

int
readsuperblk_efs_gen(int fd, struct efs *sblk, int relative)
{
	/*	
	 * Have to round size up to sector size, since usually 
	 * dealing with the raw device 
	 */
	static char sbuf[BBTOB(BTOBB(sizeof(*sblk)))];
	struct efs *sb = (struct efs *)sbuf;
	off64_t	current_loc;
	int relative_position = SEEK_SET;

	if (relative)
	    relative_position = SEEK_CUR;

	/*
	 * Get current location of file pointer.
	 */
	if ((current_loc = lseek64(fd, 0,  SEEK_CUR)) < 0)
		return -1;

	if (lseek64(fd, EFS_SUPERBOFF, relative_position) < 0)
	        return -1;

	if (read(fd, sbuf, sizeof(sbuf)) != sizeof(sbuf))
		return -1;

	if (chksb_efs(sb)) {
	        /* return pointer on failure */
		lseek64( fd, current_loc, SEEK_SET);
		return -1;
	}

	EFS_SETUP_SUPERB(sb);
	*sblk = *sb;	/* give it to the user */
	return 0;
}

int
readsuperblk_efs_relative(int fd, struct efs *sblk)
{
        return readsuperblk_efs_gen(fd, sblk, 1);
}

int
readsuperblk_efs(int fd, struct efs *sblk)
{
        return readsuperblk_efs_gen(fd, sblk, 0);
}

/* readsuperblk_xfs_gen()
 *
 * General routine to read the XFS superblock, seeking from the
 * current offset or from 0.
 *
 * Seeking from the current offset allows callers to step through the
 * disk by seeking to the start of each partition, and then calling
 * this routine to get the superblock without all callers having to
 * know where in the partition the superblock is located.
 *
 * If the partition does not contain an XFS superblock, the file pointer
 * is returned to its original location.
 *
 * ARGUMENTS:
 *  fd - open file descriptor of partition or volume to read from
 *  sblk - pointer to efs superblock to store superblock into
 *  relative - seek from 0 or current file pointer offset
 *
 * RETURN VALUE:
 *  0 - superblock successfully read
 *  -1 - failure
 *
 * ASSOCIATED ROUTINES:
 *  readsuperblk_xfs() - calls readsuperblk_xfs_gen() with relative = 1
 *  readsuperblk_xfs_relative()
 *                     - calls readsuperblk_xfs_gen() with relative = 0
 */

int
readsuperblk_xfs_gen(int fd, xfs_sb_t *sblk, int relative)
{
	/*	
	 * have to round size up to sector size, since usually 
	 * dealing with the raw device 
	 */
	static char sbuf[BBTOB(BTOBB(sizeof(*sblk)))];
	xfs_sb_t *sb = (xfs_sb_t *)sbuf;
	off64_t	current_loc;
	int relative_position = SEEK_SET;

	if (relative)
	    relative_position = SEEK_CUR;
	

	/*
	 * Get current location of file pointer.
	 */
	if ((current_loc = lseek64(fd, 0, SEEK_CUR)) < 0)
		return -1;

	if (lseek64(fd, BBTOB(XFS_SB_DADDR), relative_position) < 0)
		return -1;

	if (read(fd, sbuf, sizeof(sbuf)) != sizeof(sbuf))
		return -1;

	if (chksb_xfs(sb)) {
		lseek64( fd, current_loc, SEEK_SET);
		return -1;
	}

	/*
	 * Copy superblock to the caller.
	 */
	*sblk = *sb;
	return 0;
}

int
readsuperblk_xfs_relative(int fd, xfs_sb_t *sblk)
{
        return readsuperblk_xfs_gen(fd, sblk, 1);
}

int
readsuperblk_xfs(int fd, xfs_sb_t *sblk)
{
        return readsuperblk_xfs_gen(fd, sblk, 0);
}


/*	write the superblock, seeking from current offset;
	this allows callers to step through the entire
	disk, seeking to start of partition, and then calling this
	routine to put the superblocks, without all callers having to
	know where in the partition the superblocks is located.
	Warning: this routine has no effect on the superblocks of
	mounted filesystems, since the next sync will write the
	incore copy.
*/
int
putsuperblk_efs(int fd, struct efs *sblk)
{
	/*	have to round size up to sector size, since usually dealing
		with the raw device */
	static char sbuf[BBTOB(BTOBB(sizeof(*sblk)))];
	struct efs *sb = (struct efs *)sbuf;

	sblk->fs_checksum = efs_checksum(sblk);
	if(chksb_efs(sblk))
		return -1;
	sblk->fs_time = time(NULL);

	if(lseek64(fd, EFS_SUPERBOFF, SEEK_SET) == -1)
		return -1;

	bzero(sbuf, sizeof(sbuf));
	*sb = *sblk;

	if(write(fd, sbuf, sizeof(sbuf)) != sizeof(sbuf))
		return -1;

	return 0;
}


int mtpt_fromwhere;

/*	Try to discover the name of the directory 
	where the filesystem (if any) on the partition is mounted.
	Check for the full name in fstab and mtab;
	the mtab directory prefix is also derived from fstab.
	If not found, check for nicknames in the directory above
	(normally /dev).  For this to work correctly,
	'name' must be a full pathname.
	If all else fails, use the name from the
	superblock, if it's ever been set.
	Also sets the global variable mtpt_fromwhere to indicate
	how the mountpt was determined.
*/
char *
getmountpt(char *fstab, char *name)
{
	static char mntpt[MAXDEVNAME];
	char devpath[MAXDEVNAME];
	int devpathlen = sizeof(devpath);
	char *devpathp;
	struct stat64 sb;
	dev_t dev;
	struct mntent *m = NULL;
	FILE *mf;
	int match = 0;
	char *tmp, *savfstab;
	char mtab[128];

	*mtab = *mntpt = '\0';
	fs_readonly = 0;
	mtpt_fromwhere = MTPT_NOTFOUND;
	devpathp = name;

	if(!name)
		return mntpt;

	/*	we look in mtab first, then in fstab, since in theory, mtab
		is more reliable indicator of the system state */
	savfstab = fstab;
	strncpy(mtab, fstab, sizeof(mtab));
	tmp = strrchr(mtab, '/');
	if(tmp)
		strcpy(++tmp, "mtab");
	fstab = mtab;

	/* get canonical block name for hwg device */
	/* this will fall through for non HWG devices */
	if (filename_to_devname(name, devpath, &devpathlen)) {

		/* convert to block device name if not already */
		if ((tmp = strstr(devpath, EDGE_LBL_CHAR)) != NULL)
			strcpy(tmp, EDGE_LBL_BLOCK);

		devpathp = devpath;
	}

	/* stat so we can look for 'nicknames */
	if (stat64(devpathp, &sb) == -1)
		return mntpt;

	dev = sb.st_rdev;

chkmtab:
	if(mf = setmntent(fstab, "r")) {
		while (m = getmntent(mf)){
			/*
			 * check to see if the fsname entry matches the
			 * dev_t of the block device we were handed.
			 */

			struct stat64 nick;

			if (stat64(m->mnt_fsname, &nick) == -1)
				continue;

			if (nick.st_rdev == dev) {
				match++;
				mtpt_fromwhere =
					fstab==savfstab ? MTPT_FROM_FSTAB :
					                  MTPT_FROM_MTAB;
				break;
			}
		}
		endmntent(mf);
	}
	/* else can't read given fstab. Oh well */

	if(m && match) {
		register char *start;
		strcpy(mntpt, m->mnt_dir);
		tmp = m->mnt_opts;
		while(start=strchr(tmp, 'r')) {
			if(start[1] == 'o' && (!start[2] || start[2] == ',') && 
				(start == m->mnt_opts || start[-1] == ',')) {
				fs_readonly = 1;
				break;
			}
			tmp = start+1;
		}
	}
	if(!match && savfstab != fstab) { /* look in /etc/fstab */
		fstab = savfstab;
		goto chkmtab;
	}

	return mntpt;
}


/* function which takes the label name & partition number, and
   constructs the special file name for that partition. It returns
   a pointer to the constructed name (relative portion only)
   in a static buffer.  Doesn't work with floppy disks.

   NOTE: I'm leaving this here for older utils.  The right way to do
   this is to call path_to_partpath() directly. XXXsbarr
*/
char *
partname(char *label, unsigned idx)
{
	static char nbuf[MAXDEVNAME];
	int len = sizeof(nbuf);
	char devname[MAXDEVNAME];
	int devnamelen = sizeof(devname);
	struct stat64 sbuf;
	int type = 0;

	nbuf[0] = '\0';
	/*
	 * For xlv's just return nothing, so uses of the result will fail.
	 */
	if (stat64(label, &sbuf) == -1)
		return nbuf;

	if (S_ISCHR(sbuf.st_mode))
		type = S_IFCHR;
	if (S_ISBLK(sbuf.st_mode))
		type = S_IFBLK;
	if (type == 0)
		return nbuf;

	/* remove the major test when xlv becomes part of the HWGRAPH */
	if (major(sbuf.st_rdev) == XLV_MAJOR) {
		return nbuf;
	} else {
		if (dev_to_drivername(sbuf.st_rdev, devname, &devnamelen))
			if (strncmp(devname, "xlv", 3) == 0)
				return nbuf;
	}

	if (path_to_partpath(label, idx, type, nbuf, &len, NULL) == NULL)
		return "";

	return nbuf;
}


/*
 * efs_checksum:
 *	- compute the checksum of the read in portion of the filesystem
 *	  structure.  Take as is from the kernel source on 24 Oct 88.
 */
static __int32_t
efs_checksum(register struct efs *fs)
{
	register ushort *sp;
	register __int32_t checksum;

	checksum = 0;
	sp = (ushort *)fs;
	while (sp < (ushort *)&fs->fs_checksum) {
		checksum ^= *sp++;
		checksum = (checksum << 1) | (checksum < 0);
	}
	return (checksum);
}
