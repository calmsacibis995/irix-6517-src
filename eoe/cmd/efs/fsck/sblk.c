#ident "$Revision: 1.5 $"

#include "fsck.h"

/*
 * efs_supersum() --
 *
 *	sp		pointer to efs superblock
 *
 * returns the efs checksum of the given superblock.
 */
long
efs_supersum(struct efs *sp)
{
	return efs_cksum((unsigned short *)sp,
		(unsigned short *)&sp->fs_checksum - (unsigned short *)sp);
}

/*
 * efs_cksum() --
 *
 *	sp		ptr to data to be summed
 *	len		its length in shortwords
 *
 * returns the efs checksum of the given area.
 */
long
efs_cksum(ushort *sp, int len)
{
	long checksum;

	checksum = 0;
	while (--len >= 0) {
		checksum ^= *sp++;
		checksum = (checksum << 1) | (checksum < 0);
	}
	return checksum;
}
