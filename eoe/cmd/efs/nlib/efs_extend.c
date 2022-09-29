#ident "$Revision: 1.3 $"

/*
 * Extend the given file by allocating data blocks.
 */
#include "libefs.h"

static void blockclr(int fd, efs_daddr_t bn);
static void copy_block(int fd, efs_daddr_t from, efs_daddr_t to);
static efs_daddr_t findspace(EFS_MOUNT *mp, efs_daddr_t bn, int size);
static efs_daddr_t nextbn(EFS_MOUNT *mp, efs_daddr_t bn);

/*
 * Return next usable disk block.  If block is outside range of cylinder
 * group, move it to next cylinder group.
 */
static efs_daddr_t
nextbn(EFS_MOUNT *mp, efs_daddr_t bn)
{
	int cg;
	int maxdblock;
	struct efs *fs = mp->m_fs;

	maxdblock = fs->fs_firstcg + (fs->fs_ncg * fs->fs_cgfsize);
	for (;;) {
		cg = EFS_BBTOCG(fs, bn);
		if (bn >= maxdblock) {
			fprintf(stderr,
				"%s: filesystem is out of space\n",
				mp->m_progname);
			exit(1);
		} else
		if (bn < fs->fs_firstcg)
			bn = fs->fs_firstcg + fs->fs_cgisize;
		else
		if (bn >= fs->fs_firstcg + fs->fs_cgfsize * (cg + 1))
			bn = fs->fs_firstcg + fs->fs_cgfsize * (cg + 1) +
				fs->fs_cgisize;
		else
			return (bn);
	}
}

/*
 * Clear out the newly allocated disk block
 */
static void
blockclr(int fd, efs_daddr_t bn)
{
	char buf[EFS_BIG_BBSIZE];
	long *lp;

	/* zero out buffer */
	for (lp = (long *) buf; lp < (long *) &buf[EFS_BIG_BBSIZE]; )
		*lp++ = 0;
	if (efs_writeb(fd, buf, bn, 1) != 1)
		error();
}

void
efs_extend(EFS_MOUNT *mp, struct efs_dinode *di, int incr)
{
	efs_daddr_t bn, newbn;
	long newbbs, newextentlen;
	extent *ex;
	static efs_daddr_t nextfree;
	char *bitmap;
	struct efs *fs = mp->m_fs;

	/*
	 * See if incr is already in current allocated size.
	 */
	newbbs = EFS_BTOBB(di->di_size + incr) - EFS_BTOBB(di->di_size);
	if (newbbs == 0)
		goto out;

	/* read in bitmap, if isn't already here */
	if (efs_bget(mp) == -1) {
		fprintf(stderr,"couldn't read bitmap\n");
		exit(1);
	}
	bitmap = mp->m_bitmap;

	/*
	 * Extend last extent if possible.
	 */
top:
	if (di->di_numextents) {
		ex = &di->di_u.di_extents[di->di_numextents-1];
		bn = ex->ex_bn + ex->ex_length;
		while ((ex->ex_length < EFS_MAXEXTENTLEN) && newbbs) {
			newbn = nextbn(mp, bn);
			if (newbn != bn)
				break;
			if (btst(bitmap, newbn) == 0)
				break;
			bclr(bitmap, newbn);
			fs->fs_tfree--;
			blockclr(mp->m_fd, newbn);
			nextfree = newbn + 1;
			ex->ex_length++;
			newbbs--;
			bn++;
		}
		if (newbbs == 0)
			goto out;
	}

	/* Didn't satisfy the request. If the last extent exists and is 
	 * smaller than EFS_MAXEXTENTLEN, as will happen with growing a 
	 * directory after inserting files in it, we are going to chuck that 
	 * extent and replace it with a completely fresh one, copying data as
	 * required. This avoids the problem of directories ending up
	 * with one-block extents and running out of space. We will set the
	 * "next block" rotor to the recyled blocks to avoid ending up with
	 * a lot of space in 1, 2, 3, 4, 5 etc block chunks as a directory
	 * grows. Note that regular files (working from a mkfs proto)
	 * do NOT now go through this route, they are created by a separate
	 * function in mkfs with its own block rotor, so the freed blocks
	 * here do not result in regular file fragmentation.
	 */

	if (di->di_numextents && (ex->ex_length < EFS_MAXEXTENTLEN))
	{
		newextentlen = (long)(ex->ex_length + newbbs);
		if (newextentlen > EFS_MAXEXTENTLEN)
			newextentlen = EFS_MAXEXTENTLEN;
		newbbs -= (newextentlen - ex->ex_length);
	
		if ((newbn = findspace(mp, bn, newextentlen)) == 0)
			goto nospace;

		bn = newbn;	
		if (ex->ex_bn < nextfree)
			nextfree = ex->ex_bn;
		while (ex->ex_length--)
		{
			copy_block(mp->m_fd, ex->ex_bn, newbn++);
			fs->fs_tfree++;
			bset(bitmap, ex->ex_bn);
			ex->ex_bn++;  /* can't do in bset: macro side effects*/
		}
		ex->ex_length = newextentlen;
		ex->ex_bn = bn;
		if (newbbs == 0)
			goto out;
	}

	/*
	 * Allocate a new extent if (a) there are no extents yet,
	 * or (b) growing the last extent failed to satisfy the request.
	 * We don't support indirect extents here.
	 * If we're starting a new inode, reset the block rotor to zero to 
	 * make sure any holes caused by recyled grown-by-moving extents get 
	 * used up.
	 */
	if (di->di_numextents == EFS_DIRECTEXTENTS) {
		fprintf(stderr, "%s: can't extend file, too many extents\n",
				mp->m_progname);
		exit(1);
	}
	ex = &di->di_u.di_extents[di->di_numextents];
	if (di->di_numextents > 0) {
		extent *prev;

		prev = &di->di_u.di_extents[di->di_numextents-1];
		ex->ex_offset = prev->ex_offset + prev->ex_length;
	}
	else
		nextfree = 0;

	di->di_numextents++;

	/*
	 * Now find some free disk space to allocate out of. 
	 */
	bn = nextfree;
	while (bn < fs->fs_size) {
		newbn = nextbn(mp, bn);
		if (btst(bitmap, bn)) {
			/*
			 * Found a free block.  Start over at the top,
			 * trying to extend the file.
			 */
			bclr(bitmap, bn);
			fs->fs_tfree--;
			blockclr(mp->m_fd, newbn);
			ex->ex_bn = bn;
			ex->ex_length = 1;
			nextfree = bn + 1;
			if (--newbbs == 0)
				goto out;
			goto top;
		}
		bn++;
	}

nospace:
	fprintf(stderr, "%s: filesystem is full - can't extend file\n",
			mp->m_progname);
	exit(1);

out:
	di->di_size += incr;
}

/* Findspace(): looks for 'size' contiguous blocks, starting the search at
 * bn. If found, allocates & clears the found blocks, and returns the block
 * number where they start. If none available, returns 0.
 */

static efs_daddr_t
findspace(EFS_MOUNT *mp, efs_daddr_t bn, int size)
{
	efs_daddr_t testbn, runstart;
	int runlength, i;
	struct efs *fs = mp->m_fs;

	runlength = 0;
	runstart = 0;
	for (;;) {
		testbn = nextbn(mp, bn);
		if (testbn != bn) { /* jumped to next cg */
			runlength = 0;
			runstart = 0;
		}
		if (testbn >= fs->fs_size)
			return 0;
		if (btst(mp->m_bitmap, testbn)) {
			runlength++;
			if (!runstart)
				runstart = testbn;
		} else {
			runlength = 0;
			runstart = 0;
		}
		if (runlength == size)
			goto found;
		bn = testbn + 1;
	}

found:

	for (i = 0, testbn = runstart; i < size; i++, testbn++) {
		bclr(mp->m_bitmap, testbn);
		fs->fs_tfree--;
		blockclr(mp->m_fd, testbn);
	}
	return runstart;
}

static void
copy_block(int fd, efs_daddr_t from, efs_daddr_t to)
{
	char buf[EFS_BIG_BBSIZE];

	if (efs_readb(fd, buf, from, 1) != 1)
		error();
	if (efs_writeb(fd, buf, to, 1) != 1)
		error();
}
