#ident "$Revision: 1.12 $"

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)dumptraverse.c	5.4 (Berkeley) 2/23/87";
 */

#include "dump.h"
#include <sys/mkdev.h>

static int		dadded;		/* directory added flag */
static efs_ino_t	maxino;
static int		nsubdir;

static int			add_dent(void *);
static int			baddirblk(struct efs_dirblk *);
static int			badextent(struct extent *);
static char			*dumpalloc(int);
static int			dumpbb(struct extent *, walkarg_t);
static char			*efs_inline(struct efs_dinode *);
static dev_t			efs_getdev(union di_addr *);
static void			efs_to_sun(struct efs_dinode *, struct sun_dinode *);
static struct efs_dinode	*getino(efs_ino_t);

void
pass(void (*fn)(struct efs_dinode *), char *map)
{
	int bits;

	maxino = sblock->fs_ipcg * sblock->fs_ncg - 1;
	for (ino = 0; ino < maxino; ) {
		if ((ino % NBBY) == 0) {
			bits = ~0;
			if (map != NULL)
				bits = *map++;
		}
		ino++;
		if (bits & 1)
			(*fn)(getino(ino));
		bits >>= 1;
	}
}

void
mark(struct efs_dinode *ip)
{
	int f;

	f = ip->di_mode & IFMT;
	if (f == 0)
		return;
	BIS(ino, clrmap);
	if (f == IFDIR)
		BIS(ino, dirmap);
	if ((ip->di_mtime >= spcl.c_ddate || ip->di_ctime >= spcl.c_ddate) &&
	    !BIT(ino, nodmap)) {
		BIS(ino, nodmap);
		if (f != IFREG && f != IFDIR && f != IFLNK) {
			esize += 1;
			return;
		}
		est(ip);
	} else if (f == IFDIR)
		anydskipped = 1;
}

void
add(struct efs_dinode *ip)
{
	if(BIT(ino, nodmap))
		return;
	if ((ip->di_mode&IFMT) != IFDIR || ip->di_nlink == 0)
		return;
	nsubdir = 0;
	dadded = 0;
	if (ip->di_size & EFS_DIRBMASK) {
		msg("directory size is not a multiple of %d: ino %d; skipped\n",
			EFS_DIRBSIZE, ino);
		return;
	}
	/*
	 * Process all entries in this directory.
	 */
	if (walkextents(ip, walkdir, add_dent) < 0)
		return;
	if(dadded) {
		nadded++;
		/*
		 * One of the directory entries has to be backed up.
		 * If this directory (ino) is not being backed up, do so now.
		 * Increment nadded so that we go thru one more pass.
		 */
		if (!BIT(ino, nodmap)) {
			BIS(ino, nodmap);
			est(ip);
		}
	}
	if(nsubdir == 0)
		if(!BIT(ino, nodmap))
			BIC(ino, dirmap);
}

void
dirdump(struct efs_dinode *ip)
{
	/* watchout for dir inodes deleted and maybe reallocated */
	if ((ip->di_mode & IFMT) != IFDIR)
		return;
	dump(ip);
}

/*
 * These are shared between dump() and dumpbb().
 */
static unsigned start;	/* offset into file being dumped (BB) */
static int count;	/* number of BB's to dump */

void
dump(struct efs_dinode *ip)
{
	int i, todo, error;
	char *src = NULL;

	if (newtape) {
		newtape = 0;
		bitmap(nodmap, TS_BITS);
	}
	BIC(ino, nodmap);
	i = ip->di_mode & IFMT;
	if (i == 0)
		return;		/* free inode */
	if (ip->di_nlink == 0)
		return;		/* unlinked under us */
	if (i == IFDIR) {
		if (!dirphase)
			return;	/* file became directory */
		if (bsdformat)
			/* Convert to BSD format, return pointer to data */
			if ((src = bsd_dumpdir(ip)) == NULL)
				return;
	} else if (i == IFLNK) {
		if (ip->di_numextents == 0 && ip->di_size <= EFS_MAX_INLINE) {
			/* Grab inline data, return pointer to data */
			if ((src = efs_inline(ip)) == NULL)
				return;
		}
	}

	efs_to_sun(ip, &spcl.c_dinode);
	spcl.c_type = TS_INODE;
	spcl.c_count = 0;
	if ((i != IFDIR && i != IFREG && i != IFLNK) || ip->di_size == 0) {
		spclrec();
		return;
	}

#define MAXBB	TPTOBB(TP_NINDIR)	/* max. BB's after a header record */
	/*
	 * The big fat complication with dumping EFS disks is that
	 * the basic block size is less than TP_BSIZE.  The BSD FFS
	 * doesn't have this problem.  So we must be careful to pad
	 * the last tape record out.  Worse, we must keep track of where we
	 * are in the big tblock[] buffer in BB units while we're dumping
	 * a file, then switch the BB counters back to TP_BSIZE units
	 * when a header must be written (headers can't straddle two
	 * tape blocks).
	 *
	 * Here's what's going on with this loop: each trip around,
	 * we dump MAXBB basic blocks.  The last trip dumps the remainder.
	 * We know that MAXBB is a multiple of TP_BSIZE, so it's safe
	 * to write a TS_ADDR header after each trip.  We convert the
	 * counters back at that time by calling dmpblk_sync().
	 * After the last trip, we pad if necessary in dmpblk_end().
	 *
	 * The variable "start" is the current BB offset into the file.
	 * At each trip around the loop, we dump "count" BB's
	 * from the file, starting at "start".
	 *
	 * If we're dumping a directory, and are converting to BSD
	 * directory format, "src" will point to that data.
	 *
	 * If anything goes wrong, we must be careful to pad the output
	 * with zero blocks, or restore will freak out.
	 *
	 * There is some duplication of effort here, since
	 * walkextents() is called every time around, and must scan
	 * from 0 to start, possibly re-reading indirect blocks.
	 * But that happens too rarely to worry about, and "fixing"
	 * it would further complicate this bag of hair.
	 */
	error = 0;
	start = 0;
	todo = (int)BTOBB(ip->di_size);	/* rounds up */
	for (;;) {
		count = MIN(todo, MAXBB);
		spcl.c_count = howmany(BBTOB(count), TP_BSIZE);	/* round up */
		/* Note that all of spcl.c_addr[] were set to 1 in main() */
		spclrec();
		/*
		 * We are now committed to dumping exactly
		 * spcl.c_count tape blocks.
		 */
		dmpblk_start();
		if (src)
			dmpblk(start, count, src);	/* directory data */
		else if (error)
			dmpblk_pad(count); /* zeroes to keep restore happy */
		else {
			/*
			 * Walk ip, skip start BB's, then dump
			 * the next count BB's.  dumpbb() decrements
			 * count as it dumps, so count should be zero
			 * if all goes well.
			 */
			i = count;
			if (walkextents(ip, dumpbb, (int (*)())0) < 0)
				error++;
			else if (count != 0) {
				msg("File %d shrank! Padding %d blocks with zeroes.\n",
					ino, count);
				error++;
			}
			if (count > 0)
				dmpblk_pad(count);	/* keep restore happy */
			count = i;
		}
		start += count;
		todo -= count;
		if (todo == 0)
			break;
		dmpblk_sync();	/* reset tape counters */
		spcl.c_type = TS_ADDR;
	}
	if (src)
		free(src);
	dmpblk_end();		/* sync, and pad if necessary */
}

/*
 * Walk the file extents, calling fn(ex,arg) for each extent ex.
 * (The extents are sanity checked here also.)
 * Return value is -1 on error, 0 if all extents were visited.
 * fn should return a non-zero value to stop processing prematurely;
 * in that case we return that value.
 */
int
walkextents(struct efs_dinode *ip, walkfunc_t fn, walkarg_t arg)
{
	unsigned nx, nindir, boff;
	struct extent *ex, *ex2;
	char buf[BBSIZE];
	int i, ret;

	nx = ip->di_numextents;
	assert(nx > 0);		/* if 0, di_size should have been 0 too */
	if (nx > EFS_MAXEXTENTS) {
		msg("too many extents (%d) in ino %d; skipped\n", nx, ino);
		return -1;
	}
	ex = &ip->di_u.di_extents[0];
	if (nx <= EFS_DIRECTEXTENTS) {
		/*
		 * ex describes all extents
		 */
		for (i = 0; i < nx; i++, ex++) {
			if (badextent(ex))
				return -1;
			if (ret = (*fn)(ex, arg))
				return ret;
		}
		return 0;
	}
	/*
	 * ex describes a list of indirect extents.
	 * Read each.
	 */
	nindir = ex->ex_offset;
	if (nindir == 0 || nindir > EFS_DIRECTEXTENTS) {
		msg("%s indirect extents (%d) in ino %d; skipped\n",
			nindir ? "too many" : "zero", nindir, ino);
		return -1;
	}
	for ( ; nindir--; ex++) {
		if (badextent(ex))
			return -1;
		for (boff = 0; boff < ex->ex_length; boff++) {
			bread(ex->ex_bn+boff, buf, BBSIZE);
			assert((int)nx > 0);
			ex2 = (struct extent *)buf;
			for (i = MIN(nx, EXTSPERBB); --i >= 0; ex2++) {
				if (badextent(ex2))
					return -1;
				if (ret = (*fn)(ex2, arg))
					return ret;
			}
			nx -= (MIN(nx, EXTSPERBB));
                }
        }
        if ((int)nx > 0) {
		msg("too many extents in ino %d (%d)!\n", ino, nx);
		return -1;
	}
	return 0;
}

/*
 * Dump count bb's starting at start.
 */
/* ARGSUSED */
static int
dumpbb(struct extent *ex, walkarg_t fn)
{
	int l, delta;

	assert(count > 0);
	/*
	 * Advance to the extent containing start.
	 */
	if (ex->ex_offset + ex->ex_length <= start)
		return 0;	/* not there yet */
	/*
	 * Dump, until we run out of extents or reach the max.
	 */
	delta = ex->ex_offset < start ? start - ex->ex_offset : 0;
	l = MIN(ex->ex_length - delta, count);
	dmpblk(ex->ex_bn + delta, l, (char *)NULL);
	count -= l;
	return count == 0 ? 1 : 0;
}

/*
 * Validate an extent.
 */
static int
badextent(struct extent *ex)
{
#ifdef nodef
	if (ex->ex_magic != 0) {
		msg("bad magic # (%d) in extent (ino %d); skipped\n",
			ex->ex_magic, ino);
		return 1;
	}
#endif
	if (ex->ex_length == 0 || ex->ex_length > EFS_MAXEXTENTLEN) {
		msg("bad length (%d) in extent (ino %d); skipped\n",
			ex->ex_length, ino);
		return 1;
	}
	if (ex->ex_bn < sblock->fs_firstcg ||
	    ex->ex_bn >= EFS_CGIMIN(sblock, sblock->fs_ncg)) {
		msg("bad block address (%d) in extent (ino %d); skipped\n",
			ex->ex_bn, ino);
		return 1;
	}
	return 0;
}

void
bitmap(char *map, int typ)
{
	int i;
	char *cp;

	spcl.c_type = typ;
	spcl.c_count = howmany(msiz * (int)sizeof(map[0]), TP_BSIZE);
	spclrec();
	for (i = 0, cp = map; i < spcl.c_count; i++, cp += TP_BSIZE)
		taprec(cp);
}

void
spclrec(void)
{
	int s, i, *ip;

	spcl.c_inumber = ino;
	spcl.c_magic = NFS_MAGIC;
	spcl.c_checksum = 0;
	ip = (int *)&spcl;
	s = 0;
	i = sizeof(union u_spcl) / (4*sizeof(int));
	while (--i >= 0) {
		s += *ip++; s += *ip++;
		s += *ip++; s += *ip++;
	}
	spcl.c_checksum = CHECKSUM - s;
	taprec((char *)&spcl);
}

/*
 * Loop through each block in the directory,
 * verify that the block looks reasonable, and invoke
 * the given function on each directory entry.
 * fn should return non-zero to stop the scan.
 */
int
walkdir(struct extent *ex, walkarg_t fn)
{
	unsigned boff, i, slot;
	struct efs_dirblk db;
	struct efs_dent *dep;
	int ret;
	efs_ino_t inum;

#if EFS_DIRBSIZE != BBSIZE
	!! EFS directory blocksize has changed.  Re-think this code.
#endif
	for (boff = 0; boff < ex->ex_length; boff++) {	
		bread(ex->ex_bn+boff, (char *)&db, EFS_DIRBSIZE);
		if (baddirblk(&db))
			return -1;
		for (i = 0; i < db.slots; i++) {
			slot = EFS_SLOTAT(&db, i);
			if (slot == 0)
				continue;
			dep = EFS_SLOTOFF_TO_DEP(&db, slot);
			if ((inum = EFS_GET_INUM(dep)) == 0)
				continue;
			/*
			 * Bad inode number could screw up everthing.
			 * XXX - Maybe we should be conservative and abort!
			 */
			if (inum > maxino) {
				msg("Bad inode number %d in directory ino %d\n",
					inum, ino);
				continue;
			}
			if (ret = (*fn)(dep))
				return ret;
		}
	}
	return 0;
}

/*
 * Validate a directory block.
 */
static int
baddirblk(struct efs_dirblk *dp)
{

	if (dp->magic != EFS_DIRBLK_MAGIC )  {
		msg("Bad dir magic#, ino %d skipped. was=x%x, s/b=x%x\n",
			ino, dp->magic, EFS_DIRBLK_MAGIC);
		return(-1);
	}
	if (dp->slots > EFS_MAXENTS )  {
		msg("Dir too long, ino %d skipped.  was=%d, lim=%d\n",
			ino, dp->slots, EFS_MAXENTS);
		return(-1);
	}
	if (EFS_DIR_FREEOFF(dp->firstused) < EFS_DIRBLK_HEADERSIZE + dp->slots){
		msg("corrupted dir, ino %d skipped. firstused=%d, slots=%d, %d < %d\n",
			ino, dp->firstused, dp->slots,
			EFS_DIR_FREEOFF(dp->firstused),
			EFS_DIRBLK_HEADERSIZE + dp->slots );
		return(-1);
	}
	return 0;
}

/*
 * Called by add() to scan directory entries.
 */
static int
add_dent(void *arg)
{
	struct efs_dent *dep = arg;
	efs_ino_t inum;

	if (dep->d_name[0] == '.') {
		if (dep->d_name[1] == '\0')
			return 0;
		if (dep->d_name[1] == '.' && dep->d_name[2] == '\0')
			return 0;
	}
	inum = EFS_GET_INUM(dep);
	if (BIT(inum, nodmap)) {
		dadded++;
		return 1;	/* not an error -- just stop here */
	}
	if (BIT(inum, dirmap))
		nsubdir++;
	return 0;
}

/*
 * We cache one cylinder group's worth of inodes.
 */
static struct efs_dinode *
getino(efs_ino_t inum)
{
	static struct efs_dinode *itab = NULL;
	static int cg = -1;	/* current cylinder group */

	if (itab == NULL) {
		itab = (struct efs_dinode *)dumpalloc(BBTOB(sblock->fs_cgisize));
		if (itab == NULL) {
			msg("Out of memory");
			dumpabort();
		}
	}
	if (EFS_ITOCG(sblock, inum) != cg) {
		cg = EFS_ITOCG(sblock, inum);
		bread((unsigned)EFS_CGIMIN(sblock, cg), (char *)itab, BBTOB(sblock->fs_cgisize));
	}
	return &itab[EFS_ITOCGOFF(sblock, inum)];
}

static int	breaderrors = 0;		
#define	BREADEMAX 32

void
bread(
	unsigned da,		/* disk block number */
	char *ba,		/* buffer */
	int cnt)		/* size in bytes */
{
	int n;

	if (cnt%BBSIZE != 0) {
		msg("Illegal read size - truncating\n");
		cnt -= (cnt % BBSIZE);
	}
	n = syssgi(SGI_READB, fi, ba, da, BTOBB(cnt));
	if (BBTOB(n) == cnt)
		return;
	perror("read");
	msg("(This should not happen) bread from %s [block %d]: count=%d, got=%d\n",
		disk, da, cnt, n);
	bzero(ba, cnt);		/* don't return random garbage */
	/*
	 * Try reading one sector at a time.  We might get
	 * some of the data, or at least pin down the offending sector.
	 */
	if (cnt%dev_bsize == 0 && cnt > dev_bsize) {
		for (n = 0; n < cnt/dev_bsize; n++)
			bread(da+n, ba + n*dev_bsize, (int) dev_bsize);
		return;
	}
	if (++breaderrors > BREADEMAX){
		msg("More than %d block read errors from %s\n",
			BREADEMAX, disk);
		broadcast("DUMP IS AILING!\n");
		msg("This is an unrecoverable error.\n");
		if (!query("Do you want to attempt to continue?", 0)){
			dumpabort();
			/*NOTREACHED*/
		} else
			breaderrors = 0;
	}
}

/*
 * Convert disk inode from EFS to Sun format.
 * We only care about the fields used by restore.
 */
static void
efs_to_sun(struct efs_dinode *e, struct sun_dinode *s)
{
	struct icommon *c = &s->di_un.di_icom;

	c->ic_mode = e->di_mode;
	c->ic_uid = e->di_uid;
	c->ic_gid = e->di_gid;
	c->ic_size.val[0] = 0;
	c->ic_size.val[1] = 0;
/*
 * These ifdefs must agree with those used by restore.
 */
#if defined(vax) || defined(i386)
	c->ic_size.val[0] = e->di_size;
#endif
#if defined(mc68000) || defined(sparc) || defined(mips)
	c->ic_size.val[1] = e->di_size;
#endif
	c->ic_atime = e->di_atime;
	c->ic_mtime = e->di_mtime;
	c->ic_ctime = e->di_ctime;
	c->ic_db[0] = (baddr_t)efs_getdev(&e->di_u);
}

/* page-aligned malloc() */
static char *
dumpalloc(int n)
{
	char *p;
	unsigned i, pmask;
	static int pagesize;

	if (pagesize == 0)
		pagesize = getpagesize();
	pmask = pagesize-1;
	if ((p = malloc(n+pmask)) == NULL)
		return NULL;
	i = ((int)p + pmask) & ~pmask;
	return (char *)i;
}

/*
 * Translate the on-disk representation of dev (may be old or extended
 * style) into a dev_t. A new dev beyond the old range is indicated
 * by a -1 in the old dev field: this will cause older kernels to fail
 * to open the device.
 * We return old style device numbers as is, to try and maintain
 * portability of tapes. Thus a 5.0 dump tape may be restored
 * on a 4.0.5 system. Restore, can and must, handle both formats.
 * If a 5.0 system has new device formats, it will not restore correctly
 * on a 4.0.5 system.
 */
static dev_t
efs_getdev(union di_addr *di)
{
	if (di->di_dev.odev == -1) /* return new style device numbers */
		return (di->di_dev.ndev);
	else /* return old style device numbers */
		return((dev_t)di->di_dev.odev);
}

static char *
efs_inline(struct efs_dinode *ip)
{
	int allocsize;
	char *inlinebuf;

	if (ip->di_numextents != 0 ||
	    ip->di_size == 0 ||
	    ip->di_size > EFS_MAX_INLINE)
		return NULL;

	allocsize = (int)BBTOB(BTOBB(ip->di_size));

	if ((inlinebuf = calloc(allocsize, 1)) == NULL) {
		perror("  DUMP: no memory to read inline info");
		dumpabort();
	}

	bcopy((char *)&ip->di_u.di_extents[0], inlinebuf, ip->di_size);

	return inlinebuf;
}
