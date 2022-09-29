#include <sys/types.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

main (argc, argv) char *argv[];
{
    extern int errno;
    int fd, er1, mapsize;
    int pagesize = getpagesize();
    char *start, *tmp, *top, *sbrk();

    start = sbrk(0);
    if (((int)start)%pagesize != 0) {
        sbrk(pagesize-(((int)start)%pagesize));
        start = sbrk(0);
    }
    
    if ((fd = open("/dev/zero", O_RDWR)) < 0) {
        printf("couldn't open /dev/zero\n");
        exit(1);
    }

    mapsize = 20*pagesize;

    tmp = sbrk(mapsize);
    er1 = errno;
    printf("before: 0x%x(err:%d) -> 0x%x(err:%d)\n", tmp, er1, sbrk(0), errno);

    if (sbrk(0) != start + mapsize || er1 == -1) {
	printf("test failed\n");
	exit(1);
    }

    printf("map addr = 0x%x\n", start);

    if (mmap(start, mapsize, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_FIXED | MAP_PRIVATE, fd, 0)
        == (caddr_t)-1) {
        printf("couldn't mmap file; errno = %d\n", errno);
        exit(1);
    }
    top = sbrk(pagesize);
    er1 = errno;
    printf("after: 0x%x(err:%d) -> 0x%x(err:%d)\n", top, er1, sbrk(0), errno);

    if (sbrk(0) != start + mapsize + pagesize || er1 == -1) {
	printf("test failed after second grow\n");
	exit(1);
    }

    exit(0);
}
