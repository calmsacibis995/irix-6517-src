#ident "$Revision: 1.4 $"

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Find a block in the filesystem.  Print out where its being used, if
 * its being used.  Used to discover what files are affected by a bad spot.
 */
#include "efs.h"

int	fs_fd;
struct	efs *fs;
union {
	struct	efs fs;
	char	data[BBTOB(BTOBB(sizeof(struct efs)))];
} sblock;
char	*progname;
efs_daddr_t bn;
int	err;
efs_ino_t	inum;
int	found;

static void find_file(void);
static void find_inode(void);
static void outofmem(void);
static void search_extents(struct efs_dinode *di);

int
main(int argc, char **argv)
{
	int tblock;

	progname = argv[0];
	if (argc != 3) {
		fprintf(stderr, "usage: %s special bn\n", progname);
		exit(-1);
	}

	if ((fs_fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "%s: can't open %s\n", progname, argv[1]);
		exit(-1);
	}
	fs = &sblock.fs;
	if (efs_mount()) {
		fprintf(stderr, "%s: %s is not an extent filesystem\n",
				progname, argv[1]);
		exit(-1);
	}

	bn = strtol(argv[2], (char **)0, 0);
	if ((bn < 0) || (bn >= fs->fs_size)) {
		fprintf(stderr, "%s: block number %d is not valid\n",
				progname, bn);
		exit(-1);
	}

	/*
	 * First figure out if block is being used by the first few blocks.
	 */
	if (bn == 0) {
		printf("Block %d is the unused boot block\n", bn);
		exit(0);
	}

	if (bn == EFS_SUPERBB) {
		printf("Block %d is the super block\n", bn);
		found++;
	}

	if (bn == fs->fs_replsb) {
		printf("Block %d is the replicated super block\n", bn);
		found++;
	}

	tblock = fs->fs_bmblock ? fs->fs_bmblock : EFS_BITMAPBB;

	if ((bn >= tblock) &&
	    (bn < tblock + BTOBB(fs->fs_bmsize))) {
		printf("Block %d is in the bitmap\n", bn);
		found++;
	}

	if (bn > EFS_SUPERBB && bn < fs->fs_firstcg &&
	    !(bn >= tblock && bn < tblock + BTOBB(fs->fs_bmsize))) {
		printf("Block %d is in the unused region between\n", bn);
		if (tblock == EFS_BITMAPBB)
			printf("the bitmap and the first cylinder group\n");
		else
			printf("the superblock and the first cylinder group\n");
		found++;
	}

	find_inode();

	/*
	 * See if the block is used within a file.
	 */
	find_file();
	if (!err && !found)
		printf("Block %d is an unused free block\n", bn);
	if (found > 1)
		printf("Block %d was multiply used - filesystem is dirty\n",
			bn);
	exit(0);
	/* NOTREACHED */
}

/*
 * See if block is an inode block.
 */
static void
find_inode(void)
{
	int i;
	efs_daddr_t cgbn;

	cgbn = fs->fs_firstcg;
	for (i = 0; i < fs->fs_ncg; i++) {
		if ((cgbn <= bn) && (bn < cgbn + fs->fs_cgisize)) {
			inum = ((bn - cgbn) + i * fs->fs_cgisize) * EFS_INOPBB;
			printf("Block %d is used by inodes %d-%d\n",
				      bn, inum, inum + EFS_INOPBB - 1);
			found++;
		}
		cgbn += fs->fs_cgfsize;
	}
}

/*
 * Read inode list in, and indirect extents if needed to find where a block
 * is being used.
 */
static void
find_file(void)
{
	int i;
	int j;
	struct efs_dinode *di, *ilist;
	int ilist_size;

	ilist_size = BBTOB(fs->fs_cgisize);
	ilist = (struct efs_dinode *) malloc(ilist_size);
	if (!ilist)
		outofmem();
	inum = 0;
	for (i = 0; i < fs->fs_ncg; i++) {
		di = ilist;
		lseek(fs_fd, (long) BBTOB(EFS_CGIMIN(fs, i)), 0);
		if (read(fs_fd, (char *) ilist, ilist_size) != ilist_size) {
			fprintf(stderr, "%s: read error on cg %d's ilist\n",
					progname, i);
			/* try to keep going */
			err++;
			continue;
		}
		for (j = fs->fs_cgisize * EFS_INOPBB; --j >= 0; di++, inum++) {
			if (inum == 0)
				continue;
			if ((di->di_mode == 0) ||
			    (di->di_numextents <= 0) ||
			    (di->di_numextents > EFS_MAXEXTENTS))
				continue;
			if (((di->di_mode & IFMT) == IFREG) ||
			    ((di->di_mode & IFMT) == IFDIR) ||
			    ((di->di_mode & IFMT) == IFLNK))
				search_extents(di);
		}
	}
	free(ilist);
}

static void
search_extents(struct efs_dinode *di)
{
	int i;
	extent *ex, *exsave;

	/*
	 * Search extents
	 */
	ex = efs_getextents(di);
	exsave = ex;
	if (exsave) {
		i = di->di_numextents;
		while (--i >= 0) {
			if ((ex->ex_bn <= bn) &&
			    (bn < ex->ex_bn + ex->ex_length)) {
				printf("Block %d is being used by inode %d\n",
					bn, inum);
				found++;
			}
			ex++;
		}
		free(exsave);
	}

	/* 
	 * efs_getextents() only returns the extents that map file blocks.
	 * Look at indirect blocks iff file blocks are indirect.
	 */
	if (di->di_numextents > EFS_DIRECTEXTENTS) {
		ex = &di->di_u.di_extents[0];
		i = ex->ex_offset;
		while (--i >= 0) {
			if ((ex->ex_bn <= bn) &&
			    (bn < ex->ex_bn + ex->ex_length)) {
				printf("Block %d is being used by inode %d as an indirect extent list\n",
					bn, inum);
				found++;
			}
			ex++;
		}
	}
}

void
error(void)
{
	int old_errno;

	old_errno = errno;
	fprintf(stderr, "%s: i/o error\n", progname);
	errno = old_errno;
	perror(progname);
	err++;
}

static void
outofmem(void)
{
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(1);
}
