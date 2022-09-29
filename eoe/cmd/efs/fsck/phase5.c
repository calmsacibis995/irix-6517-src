#ident "$Revision: 1.14 $"

#include "fsck.h"
#include "dprintf.h"

void
Phase5(void)
{

	if (sflag)
	{
		fixfree = 1;
		return;
	}
	idprintf("** Phase 5 - Check Free List ");
	newline();
	fastfreechk();
	if (fixfree) {
		idprintf("BAD FREE LIST");
		if (gflag || qflag) {
			newline();
			idprintf("SALVAGED\n");
		}
		else
		if (reply("SALVAGE") == NO)
			fixfree = 0;
	}
}

/* New fast freelist check. Rather than screwing around with block-by block
 * comparisons, we pull in the whole disk bitmap and use bcmp. 
 *
 * Note that the blkmap has been built up in opposite sense to the
 * ondisk bitmap; it must be inverted. For now we'll leave this as is,
 * for efficiency, though, it would really be better to build it in the same 
 * sense as the disk bitmap.
 *
 * Also, before the bcmp, we must mask out those portions of the bitmaps
 * which don't relate to data blocks. (In Cypress & beyond, the inode
 * regions of the bitmap contain inode allocation hints).
 */

void
fastfreechk(void)
{
	int bitmapblk;
	long *lp;
	int longsinmap;

	/* Invert the blkmap and clear non-data areas */

	lp = (long *)blkmap;
	longsinmap = bitmap_blocks * (BBSIZE / sizeof(long));
	while(longsinmap--)
	{
		*lp = ~*lp;
		lp++;
	}
	busyunavail(blkmap);	

	/* read in the disk bitmap, and clear out nondata areas */

	bitmapblk = superblk.fs_bmblock ? superblk.fs_bmblock : BITMAPB;
	if (bread(disk_bitmap, bitmapblk, bitmap_blocks) != YES)
	{
		fixfree = 1;
		return;
	}
	busyunavail(disk_bitmap);	

	if (bcmp(disk_bitmap, blkmap, bitmap_blocks * BBSIZE))
		fixfree = 1;

	if ((n_blks + superblk.fs_tfree) != data_blocks) 
	{
		if (!gflag)
			idprintf("FREE BLK COUNT WRONG IN SUPERBLK");
		if (gflag){
			superblk.fs_tfree = (int)(data_blocks - n_blks);
			sbdirty();
		}
		else if (qflag) {
			superblk.fs_tfree = (int)(data_blocks - n_blks);
			sbdirty();
			newline();
			idprintf("FIXED\n");
		}
		else
		if (reply("FIX") == YES) {
			superblk.fs_tfree = (int)(data_blocks - n_blks);
			sbdirty();
		}
	}
}

void
busyunavail(char *bmap)
{
	int cg, endblocks;
	efs_daddr_t cgbase;

	/* clear area up to first cg */

	bfclr(bmap, 0, superblk.fs_firstcg);

	/* clear all the inode areas */

	cgbase = superblk.fs_firstcg;
	for (cg = 0; cg < superblk.fs_ncg; cg++, cgbase += superblk.fs_cgfsize)
		bfclr(bmap, cgbase, superblk.fs_cgisize);

	/* finally, clear to end of map */

	if ((endblocks = ((bitmap_blocks * BBSIZE * BITSPERBYTE) - fmax)) > 0)
		bfclr(bmap, fmax, endblocks);
}
