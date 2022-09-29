#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/stat.h>
#include "xfs_mkfs.h"

long
filesize(
	int		fd)
{
	struct stat	stb;

	if (fstat(fd, &stb) < 0)
		return -1;
	return (long)stb.st_size;
}
