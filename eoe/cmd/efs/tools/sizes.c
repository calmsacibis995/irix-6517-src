#include "efs.h"

main()
{
#ifdef	EFS1
	printf("EFS version 1 (short extents, short filenames)\n");
#endif
#ifdef	EFS2
	printf("EFS version 2 (long extents, short filenames)\n");
#endif
#ifdef	EFS3
	printf("EFS version 3 (long extents, long filenames)\n");
#endif
	printf("Disk inode is %d bytes\n", sizeof(struct efs_dinode));
	printf("Extent takes %d bytes\n", sizeof(extent));
}
