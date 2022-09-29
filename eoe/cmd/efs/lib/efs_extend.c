#ident "$Revision: 1.7 $"

/*
 * Extend the given file by allocating data blocks.
 */
#include "efs.h"

static void blockclr(efs_daddr_t);
static void copy_block(efs_daddr_t, efs_daddr_t);
static efs_daddr_t findspace(efs_daddr_t, int);
static efs_daddr_t nextbn(efs_daddr_t);

/*
 * Return next usable disk block.  If block is outside range of cylinder
 * group, move it to next cylinder group.
 */
static efs_daddr_t
nextbn(efs_daddr_t bn)
{
	int cg;
	int maxdblock;

	maxdblock = fs->fs_firstcg + (fs->fs_ncg * fs->fs_cgfsize);
	for (;;) {
		cg = EFS_BBTOCG(fs, bn);
		if (bn >= maxdblock) {
			fprintf(stderr,
				"%s: filesystem is out of space\n", progname);
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
blockclr(efs_daddr_t bn)
{
	char buf[BBSIZE];
	long *lp;

	/* zero out buffer */
	for (lp = (long *) buf; lp < (long *) &buf[BBSIZE]; )
		*lp++ = 0;
	lseek(fs_fd, BBTOB(bn), SEEK_SET);
	if (write(fs_fd, buf, BBSIZE) != BBSIZE)
		error();
}

void
efs_extend(struct efs_dinode *di, int incr)
{
	efs_daddr_t bn, newbn;
	long newbbs, newextentlen;
	extent *ex;
	static efs_daddr_t nextfree;

	/*
	 * See if incr is already in current allocated size.
	 */
	newbbs = (long)(BTOBB(di->di_size + incr) - BTOBB(di->di_size));
	if (newbbs == 0)
		goto out;

	/* read in bitmap, if isn't already here */
	if (bitmap == (char *)0)
		efs_bget();

	/*
	 * Extend last extent if possible.
	 */
top:
	if (di->di_numextents) {
		ex = &di->di_u.di_extents[di->di_numextents-1];
		bn = ex->ex_bn + ex->ex_length;
		while ((ex->ex_length < EFS_MAXEXTENTLEN) && newbbs) {
			newbn = nextbn(bn);
			if (newbn != bn)
				break;
			if (btst(bitmap, newbn) == 0)
				break;
			bclr(bitmap, newbn);
			fs->fs_tfree--;
			blockclr(newbn);
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
	
		if ((newbn = findspace(bn, newextentlen)) == 0)
			goto nospace;

		bn = newbn;	
		if (ex->ex_bn < nextfree)
			nextfree = ex->ex_bn;
		while (ex->ex_length--)
		{
			copy_block(ex->ex_bn, newbn++);
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
				progname);
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
		newbn = nextbn(bn);
		if (btst(bitmap, bn)) {
			/*
			 * Found a free block.  Start over at the top,
			 * trying to extend the file.
			 */
			bclr(bitmap, bn);
			fs->fs_tfree--;
			blockclr(newbn);
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
			progname);
	exit(1);

out:
	di->di_size += incr;
}

/* Findspace(): looks for 'size' contiguous blocks, starting the search at
 * bn. If found, allocates & clears the found blocks, and returns the block
 * number where they start. If none available, returns 0.
 */

static efs_daddr_t
findspace(efs_daddr_t bn, int size)
{
	efs_daddr_t testbn, runstart;
	int runlength, i;

	runlength = 0;
	runstart = 0;
	for (;;)
	{
		testbn = nextbn(bn);
		if (testbn != bn)	/* jumped to next cg */
		{
			runlength = 0;
			runstart = 0;
		}
		if (testbn >= fs->fs_size)
			return 0;
		if (btst(bitmap, testbn))
		{
			runlength++;
			if (!runstart)
				runstart = testbn;
		}
		else
		{
			runlength = 0;
			runstart = 0;
		}
		if (runlength == size)
			goto found;
		bn = testbn + 1;
	}

found:

	for (i = 0, testbn = runstart; i < size; i++, testbn++)
	{
		bclr(bitmap, testbn);
		fs->fs_tfree--;
		blockclr(testbn);
	}
	return runstart;
}

static void
copy_block(efs_daddr_t from, efs_daddr_t to)
{
	char buf[BBSIZE];

	lseek(fs_fd, BBTOB(from), SEEK_SET);
	if (read(fs_fd, buf, BBSIZE) != BBSIZE)
		error();
	lseek(fs_fd, BBTOB(to), SEEK_SET);
	if (write(fs_fd, buf, BBSIZE) != BBSIZE)
		error();
}
