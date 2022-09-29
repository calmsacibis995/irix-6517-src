#include <unistd.h> /* for F_TLOCK */
#include <fcntl.h>  /* for O_RDWR */
#include <errno.h>  /* for EAGAIN */
#define MAX_TRY 10

int
lockWholeFile(int fd, int tries)
{
	int limit = (tries)?tries:MAX_TRY;
	int try;
	lseek(fd,0L,SEEK_SET);	/* set start of lock range */
	for (try = 0; try < limit; ++try)
	{
		if (0 == lockf(fd, F_TLOCK, 0L) )
			break; /* mission accomplished */
		if (errno != EAGAIN)
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
			perror("lockf(F_TLOCK)");
	}
	else
		perror("open()");
}
