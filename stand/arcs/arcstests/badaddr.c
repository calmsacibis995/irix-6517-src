
#include <sys/cpu.h>
#include <sys/sbd.h>

#define INIT_COUNT 1000

main(int argc, char **argv, char **envp)
{
    int count, badcount;
    unsigned address  = PHYS_TO_K1(HPC_1_ID_ADDR);

    for (count = INIT_COUNT, badcount = 0; count; count--)
	if (wbadaddr (address, 1))
	    badcount++;
    printf ("wbadaddr: Found bad address %d of %d times.\n",
	    badcount, INIT_COUNT);

    for (count = INIT_COUNT, badcount = 0; count; count--)
	if (badaddr (address, 1))
	    badcount++;
    printf ("badaddr: Found bad address %d of %d times.\n",
	    badcount, INIT_COUNT);

    return 0;
}
