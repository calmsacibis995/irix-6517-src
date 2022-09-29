#include <fcntl.h>
#include <errno.h>
#define MAX_TRY 10

int
lockWholeFile(int fd, int tries)
{
	int limit = (tries)?tries:MAX_TRY;
	int try;
	struct flock lck;
	lck.l_type = F_WRLCK;		/* write (exclusive) lock */
	lck.l_whence = 0;			/* 0 offset for l_start */
	lck.l_start = 0L;			/* lock starts at BOF */
	lck.l_len = 0L;				/* extent is entire file */
	for (try = 0; try < limit; ++try)
	{
		if ( 0 == fcntl(fd, F_SETLK, &lck) )
			break; /* mission accomplished */
		if ((errno != EAGAIN) && (errno != EACCES))
			break; /* mission impossible */
	}
	return errno;
}

#include <stdio.h>
int main(int argc, char **argv)
{
	int fd, ret;
	fd = open(argv[1],O_RDWR);
	if (-1 != fd)
	{
		ret = lockWholeFile(fd,1);
		if (!ret)
		{ /* hold lock until we are stopped */
			while (1) sginap(100);
		}
		else
			perror("fcntl(F_SETLK)");
	}
	else
		perror("open()");
}
