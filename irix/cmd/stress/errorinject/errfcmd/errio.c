#include <stdio.h>
#include <stdlib.h>
#include "sys/syssgi.h"
#include "sys/types.h"
#include <fcntl.h>

char empty[] = {0};

extern int errno;
	
main(int argc, char *argv[])
{	
	__uint64_t pp[5];
	int i;
	volatile caddr_t addr;
	volatile long long x;
	pid_t pid = getpid();
	__uint64_t rc;
	int fd, size;
	
	printf("pid is %d\n", pid);

	for (i = 2; i < argc; i++) {	
	  pp[i-2] = strtoull(argv[i], NULL, 0);
	  printf("Arg %d is 0x%llx\n", i, pp[i-2]);
	}

	fflush(stdout);
	addr = malloc(65536);
	if (addr == NULL) {
	  printf("unable to alloc mem\n");
	  exit(-1);
	}
	
	addr = (caddr_t)((__uint64_t)(addr + 0x3fff) & ~0x3fff);

	mpin(addr, 4096);

	pp[1] = (__uint64_t)(addr + 0x200);
	pp[2] |= 0x40;

	printf("ppi pp2 0x%lx 0x%lx\n", pp[1], pp[2]);

	rc =syssgi(SGI_ERROR_FORCE, pp[0], pp[1], pp[2], pp[3], pp[4]); 
	if ( rc == -1) {
	  printf("error num %d\n", errno);
	  exit(-1);
	}
	sleep(1);
	printf("sleeping more\n");
	sleep(1);
	printf("sleeping done\n");
	fd = open(argv[1], O_RDWR | O_DIRECT);
	if (fd == -1) {
	  printf("open failed %s\n", argv[1]);
	  exit(-1);
	}

	size = read(fd, addr, 0x400);
	printf("errno %d\n", errno);
	printf("read 0x%x\n", size);
	exit(0);
	
}

