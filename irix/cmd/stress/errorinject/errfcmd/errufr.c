#include <stdio.h>
#include <stdlib.h>
#include "sys/syssgi.h"
#include "sys/types.h"

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

	printf("pid is %d\n", pid);

	for (i = 1; i < argc; i++) {	
		pp[i-1] = strtoull(argv[i], NULL, 0);
		printf("Arg %d is 0x%llx\n", i, pp[i-1]);
	}

	fflush(stdout);
	addr = malloc(65536);
	if (addr == NULL) {
		printf("unable to alloc mem\n");
		exit(-1);
	}
	
	addr = (caddr_t)((__uint64_t)(addr + 0x3fff) & ~0x3fff);

	addr = (caddr_t)((__uint64_t)addr + 0x180);
	pp[1] = (__uint64_t)(addr);
	*(volatile __uint64_t *)(addr + 0x200);
	pp[2] |= 0x40;
	printf("ppi pp2 0x%lx 0x%lx\n", pp[1], pp[2]);
	rc =syssgi(SGI_ERROR_FORCE, pp[0], pp[1], pp[2], pp[3], pp[4]); 
	if ( rc == -1) {
		printf("error num %d\n", errno);
		exit(-1);
	}
	printf ("rc is 0x%llx\n", rc);
	if (pp[2] & 0x80)  {
	    x = *addr;
	    printf("Read 0x%llx\n", x);
        }
	
	if (pp[2] & 0x100) {
	    *addr = 5;
	    printf("Wrote 0x%x\n", 5);
    }
	

}

