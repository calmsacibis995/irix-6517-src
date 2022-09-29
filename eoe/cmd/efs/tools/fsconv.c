/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1991, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.1 $"

/*
 *	Convert an EFS file system from one "endian" to the other.
 *
 *	This command can be run on either a big-endian or little-endian
 *	file system to produce to the other.  Conversion is done "in place"
 *	within the file system's partition.  Conversion is symmetric from
 *	one endian to another and so a file system may be converted
 *	back and forth any number of times without the loss of any
 *	information.
 *
 *	Only the file system data structures are converted (superblock,
 *	inodes, indirect extents, and directories).  The contents of
 *	user data in regular files is not altered in any way.
 *
 *	Conversion is done by "swizzling" the bytes within the data
 *	structure.  This means the bytes within the individual structure
 *	members are reversed while retaining the same structure offsets
 *	to the beginning of each member.  This way the bytes will be
 *	DMA'ed in the correct order when read by an opposite-endian
 *	system.
 *	
 *	The strategy for conversion is to go "depth first" within the
 *	data structures.  The inodes are done first by making one linear
 *	pass through all inodes in the file system.  During this pass
 *	indirect extents and directories are swizzled, followed by 
 *	a swizzle of the inode itself.  Lastly, the superblock (and
 *	the backup copy) are swizzled.  No change is made to the free
 *	block bitmap since it is always accessed as a byte stream which
 *	is endian-independent.
 */

#include <sex.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/fs/efs.h>
#include <sys/stat.h>
#include <sys/ustat.h>

char		*progname;	/* name this command was invoked as */

struct efs	superblk;	/* superblock for file system */

int		fs_fd;		/* fd for I/O to file system */

/*
 *	Macro to swizzle a 24 bit quantity
 */

#define	swap_triple(a)	((((a) << 16) & 0x00ff0000) | \
			  ((a)        & 0x0000ff00) | \
	   ((unsigned long)(a) >> 16) )


struct extent		*get_ind_extent();
struct efs_dinode	*get_next_inode();

main(argc, argv)
int argc;
char *argv[];
{
	char	*special;	/* special dev name containing fs to convert */

	progname = argv[0];

	if (argc != 2) {
		fprintf(stderr, "Usage: %s special\n", progname);
		exit(-1);
	}

	special = argv[1];

	if (ismounted(special)) {
		fprintf(stderr, "%s: %s is mounted.  File system must be unmounted for conversion\n", progname, special);
		exit(-1);
	}

	initialize(special);
	do_inodes();
	do_superblock();
	clean_up();
	exit(0);
}


/*
 *	Do initialization tasks.  Open special device and read superblock.
 *	Make sanity checks to ensure the file system appears clean and
 *	unmounted before we do anything.
 */

initialize(special)
char *special;
{

	if ((fs_fd = open(special, O_RDWR)) == -1) {
		fprintf(stderr, "%s: cannot open %s, %s\n", progname, special,
			strerror(errno));
		exit(-1);
	}

	bread((char *)&superblk, EFS_SUPERBB, 1);

	if (!IS_EFS_MAGIC(superblk.fs_magic)) {
		fprintf(stderr, "%s: bad magic number in superblock\n", progname);
		exit(-1);
	}

	if (superblk.fs_dirty) {
		fprintf(stderr, "%s: file system needs to be fsck'ed or is marked as being mounted\n",
			progname);
		exit(-1);
	}

	superblk.fs_ipcg = EFS_COMPUTE_IPCG(&superblk);
	initialize_inode_buffer();
}


/*
 *	Clean up.  Only left to do is to close the file.
 */

clean_up()
{
	close(fs_fd);
}


/*
 *	Process each inode in the file system.  We must swizzle indirect
 *	extents and directories followed by the inode itself.  All inode
 *	I/O is handled transparently by the inode package below.
 */

do_inodes()
{
	struct efs_dinode *inode;

	while (inode = get_next_inode()) {
		if (S_ISDIR(inode->di_mode))
			do_directory(inode);

		if (inode->di_numextents > EFS_DIRECTEXTENTS)
			swizzle_ind_extents(inode);

		swizzle_inode(inode);
	}
}


/*
 *	Handle the superblock.
 */

do_superblock()
{
	long	replsb;

	replsb = superblk.fs_replsb;	/* remember where copy of SB goes */
					/* before we swizzle it		 */

	swizzle_superblk();

	bwrite((char *)&superblk, EFS_SUPERBB, 1);

	if (replsb != 0)
		bwrite((char *)&superblk, replsb, 1);
}

 
/*
 *	Process the data in a directory by walking through each extent
 */

do_directory(inode)
struct efs_dinode	*inode;
{
	int ext;		/* current extent in inode */
	int rem_exts;		/* remaining # of direct extents to do */
	int ind_ext;		/* current extent within indirect extent */
	int num_exts;		/* # of extents in current indirect extent */
	struct extent *extp;	/* contents of current indirect extent */
	struct extent *ind_ext_ext; /* extent describing the current indirect */
				    /* extent */

	rem_exts = inode->di_numextents;

	if (rem_exts <= EFS_DIRECTEXTENTS) {

		/*
		 * all extents are direct
		 */

		for (ext = 0; ext < rem_exts; ext++)
			do_dir_extent(&inode->di_u.di_extents[ext]);

	} else {
		/*
		 * Go through each indirect extent...
		 */

		for (ext = 0; ext < inode->di_u.di_extents[0].ex_offset; ext++) {
			ind_ext_ext = &inode->di_u.di_extents[ext];
			extp = get_ind_extent(ind_ext_ext);
			num_exts = MIN(BBTOB(ind_ext_ext->ex_length) /
				       sizeof(struct extent), rem_exts);

			/*
			 * ...and handle an extents worth of directory
			 * entries.
			 */

			for (ind_ext = 0; ind_ext < num_exts; ind_ext++)
				do_dir_extent(&extp[ind_ext]);

			free(extp);
			rem_exts -= num_exts;
		}		
	}
}


/*
 *	Read in an extent's worth of dirblks, swizzle each dirblk, and
 *	write the extent back out.
 */

do_dir_extent(ext)
struct extent *ext;
{
	int blk;
	struct efs_dirblk *dir_extent;

	dir_extent = (struct efs_dirblk *) malloc(BBTOB(ext->ex_length));

	if (dir_extent == NULL) {
		fprintf(stderr, "%s: cannot malloc space for directory blocks\n",
			progname);
		exit(-1);
	}

	bread((char *)dir_extent, ext->ex_bn, ext->ex_length);

	for(blk = 0; blk < ext->ex_length; blk++)
		swizzle_dirblk(&dir_extent[blk]);

	bwrite((char *)dir_extent, ext->ex_bn, ext->ex_length);
	free(dir_extent);
}


/*
 *	Swizzle a block of directory entries.  We only have to swizzle the
 *	magic number and the inode numbers.  The rest of the data is byte
 *	oriented and hence endian independent.
 */

swizzle_dirblk(dp)
struct efs_dirblk *dp;
{
	int slot, slotoff;
	long inum;
	struct efs_dent *dentp;

	dp->magic = swap_half(dp->magic);

	for (slot = 0; slot < dp->slots; slot++) {
		if ((slotoff = EFS_SLOTAT(dp, slot)) == 0)
			continue;	/* skip deleted slots */

		dentp = EFS_SLOTOFF_TO_DEP(dp, slotoff);
		inum = EFS_GET_INUM(dentp);
		EFS_SET_INUM(dentp, swap_word(inum));
	}
}


/*
 *	Swizzle indirect extents of an inode.
 */

swizzle_ind_extents(inode)
struct efs_dinode	*inode;
{
	int ext;			/* index of current extent in inode */
	int rem_exts;			/* remaining # of direct extents to do*/
	int num_exts;			/* # of exts in current indirect ext */
	struct extent *extp;		/* contents of the indirect extent */
	struct extent *ind_extent;	/* extent describing indirect extent */

	rem_exts = inode->di_numextents;

	for (ext = 0; ext < inode->di_u.di_extents[0].ex_offset; ext++) {
		ind_extent = &inode->di_u.di_extents[ext];
		extp = get_ind_extent(ind_extent);
		num_exts = MIN(BBTOB(ind_extent->ex_length)/sizeof(struct extent),
			    rem_exts);
		swizzle_extents(extp, num_exts);
		bwrite((char *)extp, ind_extent->ex_bn, ind_extent->ex_length);
		free(extp);
		rem_exts -= num_exts;
	}
}


/*
 *	Swizzle the fields of an inode.
 */

swizzle_inode(inode)
struct efs_dinode	*inode;
{

	if (S_ISCHR(inode->di_mode) || S_ISBLK(inode->di_mode))
		inode->di_u.di_dev = swap_half(inode->di_u.di_dev);
	else
		swizzle_extents(inode->di_u.di_extents, EFS_DIRECTEXTENTS);

	inode->di_mode =	swap_half(inode->di_mode);
	inode->di_nlink =	swap_half(inode->di_nlink);
	inode->di_uid =		swap_half(inode->di_uid);
	inode->di_gid =		swap_half(inode->di_gid);
	inode->di_size =	swap_word(inode->di_size);
	inode->di_atime =	swap_word(inode->di_atime);
	inode->di_mtime =	swap_word(inode->di_mtime);
	inode->di_ctime =	swap_word(inode->di_ctime);
	inode->di_gen =		swap_word(inode->di_gen);
	inode->di_numextents =	swap_half(inode->di_numextents);
	inode->di_refs =	swap_half(inode->di_refs);
}


/*
 *	Swizzle the superblock
 */

swizzle_superblk()
{
	superblk.fs_size =	swap_word(superblk.fs_size);
	superblk.fs_firstcg =	swap_word(superblk.fs_firstcg);
	superblk.fs_cgfsize = 	swap_word(superblk.fs_cgfsize);
	superblk.fs_cgisize =	swap_half(superblk.fs_cgisize);
	superblk.fs_sectors =	swap_half(superblk.fs_sectors);
	superblk.fs_heads =	swap_half(superblk.fs_heads);
	superblk.fs_ncg =	swap_half(superblk.fs_ncg);
	superblk.fs_dirty =	swap_half(superblk.fs_dirty);
	superblk.fs_time =	swap_word(superblk.fs_time);
	superblk.fs_magic =	swap_word(superblk.fs_magic);
	superblk.fs_bmsize=	swap_word(superblk.fs_bmsize);
	superblk.fs_tfree =	swap_word(superblk.fs_tfree);
	superblk.fs_tinode = 	swap_word(superblk.fs_tinode);
	superblk.fs_bmblock =	swap_word(superblk.fs_bmblock);
	superblk.fs_replsb =	swap_word(superblk.fs_replsb);
	superblk.fs_checksum =	swap_word(superblk.fs_checksum);
}


/*
 *	Swizzle a list of direct extents 
 */

swizzle_extents(extp, numexts)
struct extent	*extp;
int 		numexts;
{
	int ext;

	for (ext = 0; ext < numexts; ext++) {
		extp->ex_bn = swap_triple(extp->ex_bn);
		extp->ex_offset = swap_triple(extp->ex_offset);
		extp++;
	}
}


/*
 *	Allocate a buffer and read in an indirect extent
 */


struct extent *
get_ind_extent(ext)
struct extent *ext;
{
	struct extent *ind_extent;

	ind_extent = (struct extent *) malloc(BBTOB(ext->ex_length));

	if (ind_extent == NULL) {
		fprintf(stderr, 
		   "%s: cannot malloc space for indirect directory extent\n",
		   progname);
		exit(-1);
	}

	bread((char *)ind_extent, ext->ex_bn, ext->ex_length);
	return ind_extent;
}


/*
 *	Initialize the inode buffering data structures.  This package of
 *	routines hides the details of the buffering that is done.  The
 *	idea is to buffer the inodes a CG's worth at a time.
 */

struct efs_dinode *inode_area = NULL;	/* buffer holding a CG's*/
					/* worth of inodes */

struct efs_dinode *last_inodep;	/* last inode in inode_area */

int first_time = 1;	/* so we know if there's a previous buffer to	*/
			/* write back the first time through		*/

initialize_inode_buffer()
{

	inode_area = (struct efs_dinode *) malloc(BBTOB(superblk.fs_cgisize));

	if (inode_area == NULL) {
		fprintf(stderr, 
			"%s: couldn't malloc space for inode buffer\n",
			progname);
		exit(-1);
	}

	last_inodep = &inode_area[superblk.fs_ipcg - 1];
}


/*
 *	Return a pointer to the next inode in the file system.  If we've
 *	come to the end of a buffer's worth, write the modified back and
 *	then read the next one.
 *
 *	Returns NULL when no more inodes left.
 */

struct efs_dinode *
get_next_inode()
{
	static struct efs_dinode *inodep;	/* next inode in inode_area */
						/* to give out */

	static int cgnum = 0;			/* CG num being worked on */

	if (!first_time) {

		if (inodep <= last_inodep)
			return inodep++;

		/*
		 * We've processed all the inodes in the buffer, so write
		 * back the swizzled data now.
		 */

		bwrite((char *)inode_area, EFS_CGIMIN(&superblk, cgnum), 
		       superblk.fs_cgisize);
		cgnum++;

	} else 
		first_time = 0;


	if (cgnum >= superblk.fs_ncg)
		return NULL;

	/*
	 * Read in next CG's list of inodes
	 */

	bread((char *)inode_area, EFS_CGIMIN(&superblk, cgnum), 
		superblk.fs_cgisize);

	inodep = inode_area;

	return inodep++;
}


/*
 *	Read one or more contiguous basic blocks
 */

bread(buf, start, count)
char	*buf;		/* I/O buffer */
int	start;		/* block # of first block to read */
int	count;		/* number of blocks to read */
{
	long	xfer_count;

	if (lseek(fs_fd, BBTOB(start), SEEK_SET) == -1) {
		fprintf(stderr, "%s: cannot seek for read, %s.  Unable to complete conversion.\n",
			progname, strerror(errno));
		exit(-1);
	}

	if ((xfer_count = read(fs_fd, buf, BBTOB(count))) == -1) {
		fprintf(stderr, "%s: read failed, %s.  Unable to complete conversion.\n", 
			progname, strerror(errno));
		exit(-1);
	}

	if (xfer_count != BBTOB(count)) {
		fprintf(stderr, "%s: truncated read, file system may have been corrupted.  Unable to complete conversion\n",
			progname);
		exit(-1);
	}
}


/*
 *	Read one or more contiguous basic blocks
 */

bwrite(buf, start, count)
char	*buf;		/* I/O buffer */
int	start;		/* block # of first block to read */
int	count;		/* number of blocks to read */
{
	long	xfer_count;

	if (lseek(fs_fd, BBTOB(start), SEEK_SET) == -1) {
		fprintf(stderr, "%s: cannot seek for write, %s.  Unable to complete conversion\n", 
			progname, strerror(errno));
		exit(-1);
	}

	if ((xfer_count = write(fs_fd, buf, BBTOB(count))) == -1) {
		fprintf(stderr, "%s: read failed, %s.  Unable to complete conversion\n", 
			progname, strerror(errno));
		exit(-1);
	}

	if (xfer_count != BBTOB(count)) {
		fprintf(stderr, "%s: truncated write.  Unable to complete conversion\n",
			progname);
		exit(-1);
	}
}

/*
 *	Check if there is currently a mounted file system on the given
 *	special file.
 */


ismounted(name)
char *name;
{
	struct ustat ustatarea;
	struct stat sb;

	if (stat(name, &sb) < 0) 
		return (0);

	if (((sb.st_mode & S_IFMT) != S_IFCHR) &&
	    ((sb.st_mode & S_IFMT) != S_IFBLK))  
		return (0);

	if (ustat(sb.st_rdev,&ustatarea) >= 0) 
		return (1);
	else 
		return (0);
}
