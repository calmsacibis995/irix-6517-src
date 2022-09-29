#ident "$Revision: 1.6 $"

#include "fsck.h"

void
Phase6(void)
{
	int bitmapblk;

	idprintf("** Phase 6 - Salvage Free List\n");

	bitmapblk = superblk.fs_bmblock ? superblk.fs_bmblock : BITMAPB;
	if (bwrite(blkmap, bitmapblk, bitmap_blocks) != YES)
		iderrexit("Can't write freelist to disk\n");
}
