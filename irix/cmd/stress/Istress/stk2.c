#include "stdio.h"
#include "fcntl.h"


main()
{
	extern int errno;
	int fd;

	fd = open("/tmp/stk2", O_RDWR|O_CREAT, 0666);

	errno = 0;
	write(fd, (char *)0x7ffff000L, 0x2000);
	printf("errno:%d\n", errno);
}
