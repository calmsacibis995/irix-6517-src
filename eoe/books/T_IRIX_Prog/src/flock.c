#define _BSD_COMPAT
#include <sys/file.h> /* includes fcntl.h */
#include <errno.h>  /* for EAGAIN */
#define MAX_TRY 10

int
lockWholeFile(int fd, int tries)
{
	int limit = (tries)?tries:MAX_TRY;
	int try;
	for (try = 0; try < limit; ++try)
	{
		if ( 0 == flock(fd, LOCK_EX+LOCK_NB) )
			break; /* mission accomplished */
		if (errno != EWOULDBLOCK)
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
			perror("flock(LOCK_EX)");
	}
	else
		perror("open()");
}
