#
/*
 * grow the stack very deep once, then exit.
 *
 * this program crashed pmII's when run twice serially.
 */

# include "sys/types.h"
# define PAGESIZE 1021

int NPAGES = 80;

main(argc, argv)
    int argc;
    char **argv;
{
    argc--; argv++;
    if( argc > 0 )
	NPAGES = atoi(*argv);
    child();
    exit(0);
}

child()
{
    burn(NPAGES);
}

burn(n)
    int n;
{
    register int i;
    int buf[PAGESIZE];

    if( --n < 0 )
	return;

    for( i = 0; i < PAGESIZE; i++ )
	buf[i] = (long)(0xFFL<<16L) | (long)buf;
    burn(n);
}
