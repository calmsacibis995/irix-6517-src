#include <stdio.h>
#include <stdlib.h>
#include "sys/syssgi.h"
#include "sys/types.h"
#include <sys/mman.h>
#include <sys/fcntl.h>
char empty[] = {0};

extern int errno;
	
main(int argc, char *argv[])
{	
	__uint64_t pp[5];
	int i;
	volatile caddr_t addr;
	volatile long long x;
	void *xxx;
	int fd;

	for (i = 2; i < argc; i++) {	
		pp[i-2] = strtoull(argv[i], NULL, 0);
		printf("Arg %d is 0x%llx\n", i, pp[i-2]);
	}

	fflush(stdout);

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		printf("open failed\n");
		exit(-1);
	}
	
	xxx = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x4000);
	if (xxx == MAP_FAILED) {
		printf("map failed\n");
		exit(-1);
	}
	
	addr = (caddr_t ) xxx;

	if (addr == NULL) {
		printf("unable to alloc mem\n");
		exit(-1);
	}
	
	addr = (caddr_t)((__uint64_t)(addr + 0x3fff) & ~0x3fff);
	if (pp[2] & 0x80) 
	    x = *addr;
	
	if (pp[2] & 0x100)
	    *addr = 5;

	pp[1] = (__uint64_t)addr;
	pp[2] |= 0x40;
	if (syssgi(SGI_ERROR_FORCE, pp[0], pp[1], pp[2], pp[3], pp[4]) == -1) {
		printf("error num %d\n", errno);
		exit(-1);
	}
	if (pp[2] & 0x80) 
	    x = *addr;
	
	if (pp[2] & 0x100)
	    *addr = 5;

	if (syssgi(SGI_ERROR_FORCE, 16, pp[1], 64|0x20000, pp[3], pp[4]) == -1) {
		printf("flush error num %d\n", errno);
		exit(-1);
	}


	if (pp[2] & 0x80) 
	    x = *addr;
	
	if (pp[2] & 0x100)
	    *addr = 5;


}

