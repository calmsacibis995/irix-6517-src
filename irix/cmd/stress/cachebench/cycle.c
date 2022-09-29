#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/mman.h>
#include <sys/param.h>

int 
cycle_init(volatile uint64_t **counter_ret, uint64_t *cycleval_ret)
{
    int             pagesize;
    unsigned int    pagemask;
    unsigned int    cperiod;

    int             csize;
    __psunsigned_t  caddr;
    off_t           cpage;
    unsigned int    cpageoffset;

    int             mmem;
    void           *cmmap;

    pagesize = getpagesize();
    pagemask = pagesize - 1;

    csize = syssgi(SGI_CYCLECNTR_SIZE);
    caddr = syssgi(SGI_QUERY_CYCLECNTR, &cperiod);

    cpage = caddr & ~pagemask;
    cpageoffset = caddr & pagemask;

    mmem = open("/dev/mmem", O_RDONLY);
    cmmap = mmap(0, pagesize, PROT_READ, MAP_PRIVATE, mmem, cpage);
    (void) close(mmem);

    *counter_ret = (volatile uint64_t*)
   	 ((__psunsigned_t) cmmap + cpageoffset);
    *cycleval_ret = (uint64_t) cperiod;

    if (csize != 64) {
	(void) fprintf(stderr, "ERROR: 64 bit cycle counter required.\n");
	return 0;
    }

    return 1;
}

uint64_t
cycle_usecs_to_ticks(uint64_t cycleval, double usecs)
{
	return ((1.0E6 * usecs) / cycleval);
}

double
cycle_ticks_to_usecs(uint64_t cycleval, uint64_t ticks)
{
	return ((double) (ticks * cycleval) / 1.0e6);
}

#ifdef TEST_CYCLE

/*ARGSUSED*/
int main(int argc, char *argv[])
{
	volatile uint64_t *counter;
	uint64_t cycleval;
	uint64_t ncycles;
	uint64_t end;

	if (!cycle_init(&counter, &cycleval))
		exit(EXIT_FAILURE);

	ncycles = cycle_usecs_to_ticks(cycleval, 1.0e6 * 15);

	end = *counter + ncycles;
	while (*counter < end)
		;

	exit(EXIT_SUCCESS);
	/*NOTREACHED*/
}

#endif /* TEST_CYCLE */
