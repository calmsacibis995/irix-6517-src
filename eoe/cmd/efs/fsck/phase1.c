#ident "$Revision: 1.21 $"

#include "fsck.h"

/*
 * Phase1() --
 *
 * cause each inode to be checked, causing each
 * block used by the inode to be checked.
 * build the free block bitmap as a side-effect.
 * report extent botches, badblocks, and dups,
 * optionally clearing the bad inode.
 *
 * Also, unless "fast" mode:
 *
 *	1) check directory blocks
 *	2) Cache directory blocks in the BCACHE hash store.
 *	3) Cache inode blocks containing one or more directory inodes
 *	   in the BCACHE store.
 * 
 */

#define	DUPTBLSIZE	20000		/* num of dup blocks to remember */
efs_daddr_t	duplist[DUPTBLSIZE];	/* dup block table */
efs_daddr_t	*enddup = duplist;	/* next entry in dup table */

int
dupmpinit(void)
{
	dup_lock = usnewsema(fsck_arena, 1);
	return dup_lock != 0;
}

u_long cyl_map[500];

void
clear_cyl_map(void)
{
	bzero(cyl_map, sizeof(cyl_map));
}

int
getcyl(int cyl)
{
	if (cyl >= sizeof(cyl_map)/sizeof(cyl_map[0]))
		return procn == 0;
	if (nsprocs > 1)
		return !test_and_set(&cyl_map[cyl], 1);
	else
		return 1;
}

void
Phase1(void)
{
	int cyl;
	int lastcyl;
	int inc;
	int n;
	DINODE *dp;

	/* Make the printf's look nice */
	if (procn == 0) {
		idprintf("\n  ** Phase 1 - Check Blocks and Sizes\n");
		clear_cyl_map();
	}
	if (mp_barrier)
		barrier(mp_barrier, nsprocs);

	if (procn % 2 == 0) {
		inc = 1;
		cyl = 0;
		lastcyl = superblk.fs_ncg;
	} else {
		inc = -1;
		cyl = superblk.fs_ncg - 1;
		lastcyl = -1;
	}

	pfunc = pass1;

	for ( ; cyl != lastcyl;  cyl += inc)
		if (getcyl(cyl))
			cylpass1(cyl);

	sproc_exit();

	if (enddup != duplist)
	{
		idprintf("** Phase 1b - Rescan For More DUPS\n");
		pfunc = pass1b;
		for (inum = FIRSTINO; inum <= lastino; inum++) 
		{
			switch(getstate()) {
			case FSTATE:
			case DSTATE:
#ifdef AFS
			case VSTATE:
#endif
				if ((dp = ginode()) != NULL)
					if (ckinode(dp,ADDR,0) & STOP)
						inum = lastino;
			}
		}
		printf("\n");
	}

	/* daveh oct 7 1991: move clearing of bad/dups to here, after
	 * any rescan. Otherwise, since dup table entries are removed on
	 * clearing, it is possible to miss dups in the rescan.
	 */
	for (inum = FIRSTINO; inum <= lastino; inum++) 
	{
		n = getstate();
		if ((n == CLEAR || n == TRASH) && (dp = ginode()) != NULL)
			clri("BAD/DUP", YES);
	}

	if (inoblk.b_dirty)
		bwrite(inodearea,startib,niblk);
	inoblk.b_dirty = 0;

	/* Note: after pass1, inode blocks are read singly,
	 * hopefully from cache.
	 */
	invalidate_mdcache();
}

void
cylpass1(int cyl)
{
	DINODE *dp;
	int n;
	int dirinblock = 0;
	efs_ino_t local_imax = 0;
	efs_ino_t imax = MIN(max_inodes, (cyl + 1) * superblk.fs_ipcg - 1);
	efs_ino_t imin = MAX(FIRSTINO, cyl * superblk.fs_ipcg);

	for (inum = imin; inum <= imax; inum++) 
	{
		if ((dp = ginode()) == NULL)
			continue;
		if (ALLOC)
		{
			local_imax = inum;
			if (!ftypeok(dp)) 
			{
				idprintf("UNKNOWN FILE TYPE I=%u",inum);
				if (dp->di_size)
					printf(" (NOT EMPTY)");
				if (gflag)
					gerrexit();
				if (reply("CLEAR") == YES) 
				{
					zapino(dp);
					inodirty();
				}
				goto endiloop;
			}
			n_files++;
#ifdef AFS
			setstate(DIR ? DSTATE : VICEINODE ? VSTATE :FSTATE);
#else
			setstate(DIR ? DSTATE : FSTATE);
#endif
			badblk = dupblk = 0;
			filsize = 0;
			dstateflag = 0;
			dentrycount = 0;
			ckinode(dp, ADDR, 0);
			if (dstateflag)
				rm_ddot(inum);
			/*
			 * If the inode has bad extents, etc., then clear
			 * it now instead of waiting.  This means that the
			 * trash blocks in this inode won't make good blocks
			 * look like dups.
			 */
			if ((n = getstate()) == TRASH)
				clri("BAD", YES);
			if ((n = getstate()) == DSTATE ||
#ifdef AFS
			    n == VSTATE ||
#endif
			    n == FSTATE)
				sizechk(dp);

			/* Note: test below is not redundant since sizechk()
			 * could have cleared the inode!
			 */
			if ((n = getstate()) == DSTATE ||
#ifdef AFS
			     n == VSTATE ||
#endif
			     n == FSTATE)
			{
				if (setlncnt(dp->di_nlink) <= 0) 
				{
					u_long index;

					if (nsprocs > 1)
					    index = test_then_add(&badln, 1);
					else
					    index = badln++;
					if (index < MAXLNCNT) {
						badlncnt[index] = inum;
					}
					else 
					{
					  if (nsprocs > 1)
					    test_then_add(&badln, -1);
					  else
					    badln--;
					  idprintf("LINK COUNT TABLE OVERFLOW");
					  if (gflag)
						gerrexit();
					  if (reply("CONTINUE") == NO)
							errexit("");
					}
				}
			}

			/* Evaluate the directory based on the info gleaned
			 * while scanning its blocks. Clear if bad, else
			 * note that the current inode block contains a 
			 * directory inode.
			 */
			if (n == DSTATE)
			{
				if (direvaluate() == YES)
					dirinblock++;
			}
		}
		else
		if (dp->di_mode != 0) 
		{
			idprintf("PARTIALLY ALLOCATED INODE I=%u",inum);
			if (dp->di_size)
				printf(" (NOT EMPTY)");
			if (gflag)
				gerrexit();
			if (reply("CLEAR") == YES) 
			{
				zapino(dp);
				inodirty();
			}
		}
		else
		if (dp->di_nlink < 0)
			setstate(CLEAR);

endiloop:
		/* On the last inode of each block, check if there were any
		 * directory inodes in the block. If so, cache the block. 
		 * We rather sneakily use the knowledge that dinodes fit
		 * integrally in blocks to calculate the incore
		 * block address...
		 */
		if (((inum % EFS_INOPBB) == (EFS_INOPBB - 1)) && dirinblock)
		{
			dp -= (EFS_INOPBB - 1);         /* sneakyyyy.... */
			bcload(EFS_ITOBB(&superblk, inum), (char *)dp);
			dirinblock = 0;
		}
	}
	lastino = MAX((u_long)local_imax, lastino);
}

void
invalidate_mdcache(void)
{
	if (mdcache)
		bcmddump();
}

/* ARGSUSED */
void
findgoodstart(int *bcount, efs_daddr_t *firstbn, efs_daddr_t *blkcount)
{
	int count = 0;

	/* LATER: */
	/* find maximal subvector > 128 + tail */
	if (*blkcount < 256 && *bcount)
		return;

	while (*bcount++ > 128)
		count += 256;
	*blkcount = count;
}

void
phase1mdread(void)
{
	DINODE *di;
	int count;
	efs_daddr_t blkcount = 0;
	efs_daddr_t knowncount = 0;

#define MAXBUCKET 1600
#define BUCKETCAP 256
#define MAXOFF ((MAXBUCKET - 1) * BUCKETCAP)

	efs_daddr_t firstbn = startib + niblk;

	int bcount[ MAXBUCKET];

	invalidate_mdcache();

	bzero(bcount, sizeof(bcount));

	for (di = (DINODE *)inodearea, count = niblk * EFS_INOPBB;
	     count > 0;
	     di++, count--)
	{
		extent *ex = di->di_u.di_extents;
		int ne;
		if (di->di_numextents < 1)
			continue;
		if (di->di_numextents > EFS_DIRECTEXTENTS) {
			ne = ex->ex_offset;
			if (ne > EFS_DIRECTEXTENTS)
				ne = EFS_DIRECTEXTENTS;
			while (ne-- > 0) {
				efs_daddr_t blkoff = ex->ex_bn - firstbn;
				blkcount += ex->ex_length;
				if (blkoff < 0 || blkoff >= MAXOFF)
					continue;
				bcount[blkoff/BUCKETCAP] += ex++->ex_length;
			}
			ex = di->di_u.di_extents;
		}
		if ((di->di_mode & IFMT) == IFDIR) {
			extent *ex = di->di_u.di_extents;
			int ne = di->di_numextents;
			blkcount += (di->di_size + BBSIZE - 1) >> BBSHIFT;
			if (ne > EFS_DIRECTEXTENTS)
				continue;
			while (ne-- > 0) {
				efs_daddr_t blkoff = ex->ex_bn - firstbn;
				if (blkoff < 0 || blkoff >= MAXOFF)
					continue;
				bcount[blkoff/BUCKETCAP] += ex++->ex_length;
			}
		}
	}

	bcount[MAXBUCKET - 1] = blkcount;

	for (knowncount = blkcount/2, count = 0;
		knowncount > 0;
		knowncount -= bcount[count++])
		;
	if (count >= MAXBUCKET)
		findgoodstart(bcount, &firstbn, &blkcount);
	else {
		blkcount *= 1.05;
	}

	while (blkcount * BBSIZE != mem_reserve(blkcount * BBSIZE, TEMP, 1))
		blkcount *= 0.9;
	if (blkcount == 0)
		return;
	mdcache = tmpmap(blkcount * BBSIZE);
	if (!mdcache)
		return;
	if (bread(mdcache, startib + niblk, blkcount) == NO) {
		freetmpmap(mdcache, blkcount * BBSIZE);
		mdcache = 0;
		startmdcache = endmdcache = 0;
		if (mdbits)
			free(mdbits);
		mdbits = 0;
	} else {
		startmdcache = startib + niblk;
		endmdcache = startmdcache + blkcount;
		mdbits = calloc(blkcount / NBBY + 4, 1);
	}

#undef MAXBUCKET
#undef BUCKETCAP
#undef MAXOFF

}

/*
 * ckinode() --
 *
 * check an inode.
 * cause each block used to be checked.
 */
int
ckinode(DINODE *dp, int flg,
	efs_ino_t pinum)	/* parent inode: from descend() */
{
	int ret;
	int (*func)();
	int stopper;
	efs_daddr_t firstoff;
	struct extent extents[EXTSPERDINODE];

	if (CHR||BLK)
		return KEEPON;

	switch (flg) {

	case ADDR:
		func = pfunc;
		stopper = STOP;
		break;

	case DATA:
		func = dirscan;
		stopper = STOP;
		break;

	case DEMPT:
		func = chkeblk;
		stopper = STOP|SKIP;
		break;
	}

	firstoff = 0;
	memcpy(extents, dp->di_x, sizeof extents);
	if ((unsigned short)dp->di_nx <= EXTSPERDINODE)
		ret = chk_extlist(extents, dp->di_nx,
				func, stopper, &firstoff, pinum, inum);
	else
		ret = chk_iext(extents, dp->di_nx,
				func, stopper, &firstoff, pinum, inum, dp);

	if (ret & stopper)
		return ret;
	return KEEPON;
}

/*
 * chk_iext() --
 *
 * check all indirect extents
 * cause each block of direct extents to be checked.
 */
int
chk_iext(
	struct extent *xp,	/* points to first extent in dinode */
	int nx, int (*func)(), int stopper, efs_daddr_t *(_firstoff),
	efs_ino_t pinum, efs_ino_t curinum, DINODE *dp)
{
	int ixb, xl;
	efs_daddr_t xb;
	int ret;
	BUFAREA ib;
	unsigned nindirs;
	struct extent *ixp = xp;

	if ((unsigned)nx > EFS_MAXEXTENTS) {
		if (pass1check)
		{
			idprintf("RIDICULOUS NUMBER OF EXTENTS (%d)", nx);
			printf(" (Max allowed %d)\n", EFS_MAXEXTENTS);
			setstate(TRASH);	/* mark for possible clearing */
		}
		return SKIP;
	}

	/* The offset field of the first extent in the disk inode holds the
	 * number of indirect extents.
	 */
	nindirs = xp->ex_offset;
	if (nindirs > EFS_DIRECTEXTENTS) {
		if (pass1check)
		{
		   idprintf("ILLEGAL NUMBER OF INDIRECT EXTENTS (%d)", nindirs);
		   setstate(TRASH);	/* mark for possible clearing */
		}
		return SKIP;
	}

	while (nindirs--)
	{
		xl = xp->ex_length;
#ifndef AFS
		if (xp->ex_magic != EFS_EXTENT_MAGIC) {
			if (pass1check)
				exterr("BAD MAGIC IN EXTENT", xp);
			return SKIP;
		}
#endif
		if (xl > EFS_MAXEXTENTLEN)
		{
			if (pass1check)
				exterr("ILLEGAL LENGTH IN EXTENT", xp);
			return SKIP;
		}
		/*
		 * Cache the extent list.
		 * If it's the last indirect extent, look for zero'ed 
		 * extents at the end, and if so, pitch them without
		 * invalidating the whole file.
		 * Using extentbuf is a layering violation. Oh well.
		 */
		if (grabextent(xp->ex_bn, xl) && REG &&
		    nindirs == 0 && pass1check)
		{
			if (chk_zeroext(xp, &nx, (struct extent *)extentbuf, *_firstoff, dp, ixp))
			{
				xl = xp->ex_length;
				fixdone = 1;
				writeextent();
			}
		}
		for (ixb = 0 , xb = xp->ex_bn; ixb < xl; ixb++ , xb++) {

			/* Important!
			 * itrunc relies on indir block being func'd
			 * _before_ the blocks mapped by the indir block!
			 */
			if (func == pfunc)
				if (!((ret = (*func)(xb, INDIRBLOCK)) & KEEPON))
					return ret;
	
			if (outrange(xb))
			{
				setstate(TRASH);
				return SKIP;
			}

			initbarea(&ib);
			if (getblk(&ib, xb) == NULL)
				return SKIP;
	
			ret = chk_extlist(ib.b_un.b_ext,
					nx < EXTSPERBB ? nx : EXTSPERBB,
					func, stopper, _firstoff, pinum, curinum);
			if (ret & stopper)
				return ret;

			nx -= EXTSPERBB;
		}
		xp++;	/* step to next indirect extent */
	}

	if (nx > 0)
	{
		if (pass1check)
		{
			idprintf("INDIRECT EXTENT CORRUPTION");
			setstate(TRASH);	/* mark for possible clearing */
		}
		return SKIP;
	}
	return KEEPON;
}

/*
 * chk_extlist() --
 * cause checking of each (direct) extent in a list,
 * causing each block to be checked.
 */
int
chk_extlist(struct extent *xp, int nx, int (*func)(), int stopper,
	efs_daddr_t *_firstoff, efs_ino_t pinum, efs_ino_t curinum)
{
	int ix;
	int ret;
	
	for (ix = 0; ix < nx; ix++ , xp++) {
		ret = chk_ext(xp, func, stopper, _firstoff, pinum, curinum);
		if (ret & stopper)
			return(ret);
	}
	return KEEPON;
}

/*
 * chk_ext() --
 * cause checking of each block in an extent.
 */
int
chk_ext(struct extent *xp, int (*func)(), int stopper, efs_daddr_t *_firstoff,
	efs_ino_t pinum, efs_ino_t curinum)
{
	int ixb, xl;
	efs_daddr_t xb;
	int ret;
	int blockflag;

	xl = xp->ex_length;

	blockflag = (*_firstoff == 0) ? FIRSTBLOCK : 0;

	if ((unsigned)*_firstoff > xp->ex_offset) {
		if (pass1check)
			exterr("EXTENT OUT OF ORDER", xp);
		return SKIP;
	}
#ifndef AFS
	if (xp->ex_magic != EFS_EXTENT_MAGIC) {
		if (pass1check)
			exterr("BAD MAGIC IN EXTENT", xp);
		return SKIP;
	}
#endif
	if (xp->ex_length == 0) {
		if (pass1check)
			exterr("ZERO LENGTH EXTENT", xp);
		return SKIP;
	}

	*_firstoff = xp->ex_offset + xp->ex_length;

	if (func == pass1) {
		if (getstate() == DSTATE)
			grabextent(xp->ex_bn, xl);
		else
			return pass1i(xp->ex_bn, xl, stopper);
	}

	for (ixb = 0 , xb = xp->ex_bn; ixb < xl; ixb++ , xb++) {
		ret = (*func)(xb, blockflag, pinum, curinum);
		if (ret & stopper)
			return ret;
		blockflag = 0;
	}
	
	return KEEPON;
}

/* Added 10/22/90 by daveh: we now sanity check directory blocks here rather 
 * than in pass 2: this catches bad ones earlier & ensures that even 
 * unreferenced dir blocks are checked. Also, we cache them for future use.
 */

static u_long headmask[BITSPERWORD];
static u_long trailmask[BITSPERWORD];
u_long bitmask[BITSPERWORD];

#define BYTEMASK (NBBY - 1)

void
init_bitmask(void)
{
	unsigned i;

	for (i = 0; i < BITSPERWORD; i++) {
		off_t byte = i / NBBY;
		bitmask[i] = 1UL << ((i & BYTEMASK)
				+ (sizeof(u_long) - 1 - byte) * NBBY);
	}
	trailmask[0] = bitmask[0];
	headmask[BITSPERWORD - 1] = bitmask[BITSPERWORD - 1];
	for (i = 1; i < BITSPERWORD; i++) {
		trailmask[i] = bitmask[i] | trailmask[i-1];
		headmask[BITSPERWORD - 1 - i] =
			bitmask[BITSPERWORD - 1 - i]|headmask[BITSPERWORD - i];
	}
}

int
pass1(efs_daddr_t blk, int blockflag)
{
	u_long *p = (u_long *)blkmap;
	u_long n;
	int word;
	u_long result;

	if (outrange(blk))
		goto badblock;

	word = blk / BITSPERWORD;
	p += word;
	n = bitmask[blk & WORDMASK];
	if (nsprocs > 1)
		result = test_then_or((u_long*)p, n);
	else {
		result = *p;
		*p |= n;
	}

	if (result & n) {
		/*
		 * determine if it's really a dup -
		 * or a bad (we know already it's not
		 * out of range).  ie, if it's within
		 * a valid data area or not.
		 */
		if (!((blk-superblk.fs_firstcg) % superblk.fs_cgfsize
				>= superblk.fs_cgisize
		    || (blk-superblk.fs_firstcg) / superblk.fs_cgfsize
				>= superblk.fs_ncg))
			goto badblock;

		if (dup_record(blk) == STOP)
			return STOP;
	}
	else {
		n_blks++;

		/* Note: chkblk() gets blk in fileblk, where bcload can take
		 * it from. We want to call chkblk only on actual directory
		 * data blocks, hence the INDIRBLOCK test */

		if (!fast &&
		    (getstate() == DSTATE) && 
		    (blockflag != INDIRBLOCK))
		{
			if (chkblk(blk, blockflag, 0, inum) != SKIP)
				bcload(blk, fileblk.b_un.b_buf);
		}
	}

	filsize++;
	return(KEEPON);

 badblock:
	blkerr("BAD",blk,1);
	if (++badblk >= MAXBAD) {
		idprintf("EXCESSIVE BAD BLKS I=%u",inum);
		if (gflag)
			gerrexit();
		if (reply("CONTINUE") == NO)
			errexit("");
		return(STOP);
	}
	return(SKIP);
}

int
pass1i(efs_daddr_t blk, int len, int stopper)
{
#define nextblk(blk) ((blk + BITSPERWORD) & ~WORDMASK)

	u_long *pbase = (u_long *)blkmap;
	int ret;
	efs_daddr_t end = len + blk - 1;

	if (outrange(blk) || outrange(blk + len - 1))
		return pass1i_badextent(blk, len);

	filsize += len;
	n_blks += len;

	for (; blk <= end; blk = nextblk(blk)) {
		u_long *p;
		u_long n;
		u_long result;

		p = &pbase[blk / BITSPERWORD];
		n = (blk & WORDMASK) ? headmask[blk & WORDMASK] : ~0;
		n &= ((blk & ~WORDMASK) == (end & ~WORDMASK))
			? trailmask[end & WORDMASK] : ~0;
		if (nsprocs > 1)
			result = test_then_or((u_long*)p, n);
		else {
			result = *p;
			*p |= n;
		}

		n &= ((blk & ~WORDMASK) == (end & ~WORDMASK))
			? trailmask[end & WORDMASK] : ~0;

		if (result & n) {
			int testblk;

			/*
			 * determine if it's really a dup -
			 * or a bad (we know already it's not
			 * out of range).  ie, if it's within
			 * a valid data area or not.
			 */
			if (!((blk-superblk.fs_firstcg) % superblk.fs_cgfsize
					>= superblk.fs_cgisize
			    || (nextblk(blk)-superblk.fs_firstcg-1)
				   / superblk.fs_cgfsize
					>= superblk.fs_ncg))
			{
				ret = pass1i_badblock(blk);
				if (stopper & ret)
					return ret;
				continue;
			}

			/*
			 * keep track of dups so we can find the other halves
			 * later
			 */

			result &= n;
			blk &= ~WORDMASK;

			for (testblk = 0; testblk < BITSPERWORD; testblk++)
				if (result & bitmask[testblk])
					if (dup_record(blk + testblk) == STOP)
						return STOP;
		}
	}
	return(KEEPON);
}

int
pass1i_badextent(efs_daddr_t blk, int len)
{
	while (len-- > 0) {
		if (pass1i_badblock(blk++) == STOP)
			return STOP;
	}
	return SKIP;
}

int
pass1i_badblock(efs_daddr_t blk)
{
	blkerr("BAD",blk,1);
	if (++badblk >= MAXBAD) {
		idprintf("EXCESSIVE BAD BLKS I=%u",inum);
		if (gflag)
			gerrexit();
		if (reply("CONTINUE") == NO)
			errexit("");
		return(STOP);
	}
	return SKIP;
}

int
dup_record(efs_daddr_t blk)
{
	DUP_LOCK();
	blkerr("DUP",blk,0);

	/* closes a race condition with unwind_bmap */
	setbmap(blk);

	if (++dupblk >= MAXDUP) {
		idprintf("EXCESSIVE DUP BLKS I=%u",inum);
		if (gflag)
			gerrexit();
		if (reply("CONTINUE") == NO)
			errexit("");
		DUP_UNLOCK();
		return(STOP);
	}
	if (enddup >= &duplist[DUPTBLSIZE]) {
		idprintf("DUP TABLE OVERFLOW.");
		if (gflag)
			gerrexit();
		if (reply("CONTINUE") == NO)
			errexit("");
		DUP_UNLOCK();
		return(STOP);
	}
	*enddup++ = blk;
	DUP_UNLOCK();
	return KEEPON;
}

int
pass1b(efs_daddr_t blk)
{
	efs_daddr_t *dlp;

	if (outrange(blk))
		return(SKIP);
	for (dlp = duplist; dlp < enddup; dlp++) {
		if (*dlp == blk) {
			blkerr("DUP",blk,0);
			if (enddup >= &duplist[DUPTBLSIZE]) {
				idprintf("DUP TABLE OVERFLOW.");
				if (gflag)
					gerrexit();
				if (reply("CONTINUE") == NO)
					errexit("");
				return(STOP);
			}
			*enddup++ = blk;
			break;
		}
	}
	return(KEEPON);
}

/* This function is used to "unwind" a previous bit assignment to the blockmap
 * when clearing out an inode.
 */

int
unwind_map(efs_daddr_t blk)
{
	efs_daddr_t *dlp;

	if (outrange(blk))
		return(SKIP);
	if (getbmap(blk)) {
		int cleared = 0;
		DUP_LOCK();
		for (dlp = duplist; dlp < enddup; dlp++) {
			if (*dlp == blk)
				if (cleared) {
					DUP_UNLOCK();
					return(KEEPON);
				} else {
					cleared = 1;
					*dlp = *--enddup;
				}
		}
		clrbmap(blk);
		n_blks--;
		DUP_UNLOCK();
	}
	return(KEEPON);
}

/*
 * busy out the inodes, bitmap, and superblock.
 */
void
init_bmap(void)
{
	int cgno;
	int i;
	efs_daddr_t bn;

	for (bn = superblk.fs_firstcg; --bn >= 0;)
		setbmap(bn);
	for (cgno = 0; cgno < superblk.fs_ncg; cgno++) {
		bn = CGIMIN(&superblk, cgno);
		for (i = superblk.fs_cgisize; --i >= 0;)
			setbmap(bn++);
	}
}

void
exterr(char *s, struct extent *xp)
{
	idprintf("[%ld+%d: %ld] %s I=%u\n",
			(efs_daddr_t)xp->ex_offset, xp->ex_length,
			(efs_daddr_t)xp->ex_bn, s, inum);
	setstate(TRASH);	/* mark for possible clearing */
}

/*
 * Check for and remove zero extents at the end of the last indirect extent.
 */
chk_zeroext(struct extent *ixp, int *nxp, struct extent *dxp,
	efs_daddr_t firstoff, DINODE *dp, struct extent *bixp)
{
	int ix, nz;
	u_int realblks;
	efs_daddr_t newsize;

	for (nz = 0, ix = *nxp - 1; ix >= 0; ix--, nz++)
	{
		if (dxp[ix].ex_length || dxp[ix].ex_offset || dxp[ix].ex_bn)
			break;
#ifndef AFS
		if (dxp[ix].ex_magic)
			break;
#endif
	}
	if (!nz)
		return 0;
	idprintf("%d ZERO EXTENTS AT END OF FILE I=%u", nz, inum);
	if (gflag)
		gerrexit();
	if (reply("TRUNCATE") == NO)
		return 0;
#ifdef FSCKDEBUG
	idprintf("IX (LAST GOOD EXTENT) IS %d, FIRSTOFF IS %u\n", ix, firstoff);
#endif
	if (ix < 0 && dp->di_x[0].ex_offset)
	{
		dp->di_x[0].ex_offset--;
#ifdef FSCKDEBUG
		idprintf("CHANGING DI_X[0].EX_OFFSET FROM %d TO %d\n", 
			dp->di_x[0].ex_offset + 1, dp->di_x[0].ex_offset);
#endif
	}
	*nxp -= nz;
	dp->di_nx -= nz;
#ifdef FSCKDEBUG
	idprintf("DI_NX CHANGED FROM %d TO %d, I=%u\n", dp->di_nx + nz,
		dp->di_nx, inum);
#endif
	realblks = (dp->di_nx + EXTSPERBB - 1) / EXTSPERBB;
	if (ixp->ex_length > realblks)
	{
#ifdef FSCKDEBUG
		idprintf("EX_LENGTH CHANGED FROM %d TO %d, I=%u\n",
			ixp->ex_length, realblks, inum);
#endif
		ixp->ex_length = realblks;
		dp->di_x[ixp - bixp].ex_length = realblks;
	}
	if (ix >= 0)
		newsize = dxp[ix].ex_offset + dxp[ix].ex_length;
	else
		newsize = firstoff;
	if (newsize * BBSIZE < dp->di_size)
	{
#ifdef FSCKDEBUG
		idprintf("DI_SIZE CHANGED FROM %d TO %d, I=%u\n", dp->di_size,
			newsize * BBSIZE, inum);
#endif
		dp->di_size = newsize * BBSIZE;
	}
	inodirty();
	return 1;
}
