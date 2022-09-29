#ident "$Revision: 1.4 $"

/*
 * Compute the checksum of the read in portion of the filesystem
 * structure
 */
#include "efs.h"

void
efs_checksum(void)
{
	ushort *sp;
	long checksum;

	checksum = 0;
	sp = (ushort *)fs;
	while (sp < (ushort *)&fs->fs_checksum) {
		checksum ^= *sp++;
		checksum = (checksum << 1) | (checksum < 0);
	}
	fs->fs_checksum = checksum;
}
