#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/cachectl.h>
#include <sys/sysmacros.h>

#define page_round(a)	((unsigned long)(a) & ~(pagesize-1))

main()
{
	int fd;
	void *addr;
	int pagesize = getpagesize();

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("/dev/zero");
		exit(1);
	}

	if ((addr = mmap(NULL, pagesize, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd,
			0)) == (void *)-1) {
		perror("mmap");
		exit(1);
	}

	if (cachectl((void *)page_round(&addr), pagesize, UNCACHEABLE) == -1) {
		perror("UNCACHEABLE");
		exit(1);
	}


	if (cachectl((void *)page_round(&addr), pagesize, CACHEABLE) == -1) {
		perror("CACHEABLE");
		exit(1);
	}

	exit(0);
}
