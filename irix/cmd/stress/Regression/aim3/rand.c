/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 #* All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) rand.c:3.2 5/30/92 20:18:47" };
#endif

#define MULT 1103515245l
#define INCR  12345l

static unsigned long seed = 1l;
static unsigned long seed2 = 1l;

unsigned int
rand()
{
    seed = seed * MULT + INCR;		/* modulus is span of a long */
    return((unsigned int)((seed >> 16)&0x7fff));
}

srand(input)
unsigned int input;
{
    seed = (unsigned long)input;
}

unsigned int
rand2()
{
    seed2 = seed2 * MULT + INCR;		/* modulus is span of a long */
    return((unsigned int)((seed2 >> 16)&0x7fff));
}

srand2(input)
unsigned int input;
{
    seed2 = (unsigned long)input;
}
