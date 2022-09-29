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

	for (i = 1; i < argc; i++) {	
		pp[i-1] = strtoull(argv[i], NULL, 0);
		printf("Arg %d is 0x%llx\n", i, pp[i-1]);
	}

	fflush(stdout);

	if (syssgi(SGI_ERROR_FORCE, pp[0], pp[1], pp[2], pp[3], pp[4]) == -1) {
		printf("error num %d\n", errno);
		exit(-1);
	}
}
