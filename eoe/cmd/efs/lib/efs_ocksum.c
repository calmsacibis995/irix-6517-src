#ident "$Revision: 1.3 $"

/*
 * Compute the checksum of the read in portion of the filesystem
 * structure
 *	- this checksum suffers from shifting rather than rotating, thus
 *	  discarding information from all superblock members 32 half-words
 *	  or more before the checksum; since a zero-filled spare array lies
 *	  just before the checksum, much of the computed sum is 0
 */
#include "efs.h"

void
efs_oldchecksum(void)
{
	ushort *sp;
	long checksum;

	checksum = 0;
	sp = (ushort *)fs;
	while (sp < (ushort *)&fs->fs_checksum) {
		checksum ^= *sp++;
		checksum <<= 1;
	}
	fs->fs_checksum = checksum;
}
