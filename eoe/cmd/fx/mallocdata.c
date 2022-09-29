/* override the version of this file in saio/lib.  fx needs a large
 * malloc area to deal with cylinder at a time exercising on drives
 * like the Seagate 1.2Gb (15 heads * 71 sec/trk (avg) * 2 buffer)
 * (2 buffers for wr-cmp and rd-cmp) = 1064 Kbytes.
 * we add a bit more for next generation of drives.
 * I've moved the load address above sash, so now we can
 * make it big enough to last a while (I hope).  0x1f0000
 * was *barely* enough for the largest current drives.
*/

#define MALSIZE 0x280000

/* this needs to have a higher address than the allocs array in malloc.c,
 * or the code needs to be modified, since the code assumes the sbrk'ed mem
 * is above the bss...  Since malbuf is no longer static, and allocs
 * is, we are OK as long as bss follows data, which is a pretty
 * good bet! */
char malbuf[MALSIZE];

int _max_malloc = sizeof(malbuf);	/* most we can ever sbrk at once */
