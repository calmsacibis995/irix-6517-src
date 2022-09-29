#include <stdio.h>
#include <stdlib.h>
#include "sys/syssgi.h"
#include "sys/types.h"
#include "sys/pmo.h"
#include <sys/mman.h>
#include <sys/fcntl.h>

char empty[] = {0};

extern int errno;

#define	PAGE_SIZE	65536
#define NBPP		16384

void 	alloc_addr(volatile char **, size_t, size_t);
policy_set_t policy = {
	"PlacementDefault", (void *)1,
	"FallbackLargepage", NULL,
	"ReplicationDefault", NULL,
	"MigrationDefault", NULL,	
	"PagingDefault", NULL,
	PAGE_SIZE
};
	
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

	alloc_addr(&addr, 1024*1024*4, 1024*1024*4);

	catte(addr, 1024);

}

int
set_page_size(int size, volatile char *vaddr, int len)
{
	pmo_handle_t	pm;

	policy.page_size = size;
	pm = pm_create( &policy);

	if ( pm < 0) {
		perror("pm_create");
		return -1;
	}
	if (pm_attach(pm, (void *)vaddr, len) < 0) {
		perror("pm_attach");
		return -1;
	}
	return 0;
}

void
alloc_addr(volatile char **base, size_t alloc_size, size_t base_size)
{
	int fd = open("/dev/zero",O_RDWR);	
	volatile char	*mapaddr;

	if ( fd == -1) {
		perror("open /dev/zero");
		exit(1);
	}

	mapaddr = mmap(0, alloc_size, PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	if ( mapaddr == (volatile char *)-1) {
		perror("mmap");
		exit(1);
	}

	printf("Mmap succeeded addr %x\n",mapaddr);
	mapaddr = (char *)((__psunsigned_t)mapaddr &~(NBPP-1));
	if( set_page_size(base_size, mapaddr, alloc_size) == -1) {
		exit(1);
	}

	*base = mapaddr;

	close(fd);
}


