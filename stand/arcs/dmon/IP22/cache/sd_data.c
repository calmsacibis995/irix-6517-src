static char rcsid[] = "$Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/IP22/cache/RCS/sd_data.c,v 1.1 1994/07/20 23:46:57 davidl Exp $";

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */


#include "sys/types.h"
#include "sys/sbd.h"
/*#include "machine/cpu.h"*/

#define PASS 0
#define FAIL 1

extern int pdcache_size;	/* Primary Dcache size  */
extern int pdcache_linesize;	/* Primary Dcache linesize */
extern int scache_size;	/* Secondary Cache size */
extern int scache_linesize;	/* Secondary line size  */

#define WORD_ADDR_MASK 0xfffffffc

/* Use the compliment of the address as data */
#define COMPLIMENT 1

/* write data from low to high */
#define FORWARD 0

#define PATTERN1	0xaaaaaaaa
#define PATTERN2	0x55555555

/* performs an address in address test on the secondary data cache */
sd_data(first_addr, last_addr)
register int first_addr;
register int last_addr;

{
	register u_int i;
	register u_int k1addr;
	register u_int tmp;
	register u_int expected;
	u_int loop;
	u_int errflag;

	int error;

/* Enable cache error exceptions ?? 
	SetSR( GetSR() & ~SR_DE);
*/

	/* Get the secondary cache size and line size */
	scache_size = size_2nd_cache();
	scache_linesize= scache_ln_size();

	/* Get the primary data cache size and line size */
	pdcache_size = dcache_size();
	pdcache_linesize = dcache_ln_size();


/* Disable cache error exceptions ?? 
	SetSR( GetSR() | SR_DE);
*/


	if( noSCache() ) {
		/* Indicate Scache not present */
		puts("No Secondary Cache detected\n");
		return;
	}

	/* Get users input for test */
	first_addr = GetFirstAddr();
	last_addr = GetLastAddr();
	loop = GetOptions_Loop();
	errflag= GetOptions_Quiet();

	if( !errflag) {
		puts("start test\r\n");
		puthex(first_addr);
		puts("\r\n");
		puthex(last_addr);
		puts("\r\n");
	}


restart:
	if( !errflag)
		puts("Invalidate D/SC Cache\r\n");

	/* Invalidate all the caches */
	invalidate_dcache(pdcache_size, pdcache_linesize);
	invalidate_scache( scache_size, scache_linesize);


	if( !errflag)
		puts("TAG Memory ~addr\n\r");

	/* Load MEMORY with pattern1 */
	fillmemW( K0_TO_K1(first_addr), K0_TO_K1(last_addr), PATTERN1, FORWARD);

	if( !errflag)
		puts("Dirty Cache\n\r");

	/* DIRTY SCACHE */
	for(i= first_addr; i < last_addr ;  ) {
		*(u_int *)i = PATTERN1 ;
		i = i + scache_linesize;
	}

	if( !errflag)
		puts("Set memory to 0xdeadbeef\n\r");

	/* Load memory with a background pattern */
	fillmemW( K0_TO_K1(first_addr), K0_TO_K1(last_addr), 0xdeadbeef, FORWARD);

	if( !errflag)
		puts("Force SC writeback (~addr)\n\r");

	/* Write the second pattern forcing the first pattern back to memory */
	fillmemW( first_addr + 0x100000, last_addr + 0x100000, 0x55555555, FORWARD);
	
	if( !errflag)
		puts("Check memory\n\r");

	/* check the data in memory */
	expected = PATTERN1;
	k1addr = K0_TO_K1(last_addr);
	for(i= K0_TO_K1(first_addr); i < k1addr; i += 4) {
		tmp = *(u_int *)i;
		if( expected != tmp) {
			
			if( !errflag) {
				puts("Secondary Memory Error Pattern 1\n\r");
				puts("Address ");
				puthex(i);
				puts("\r\nexpected ");
				puthex(expected);
				puts(", actual ");
				puthex(tmp);
				puts(", XOR ");
				puthex( tmp ^ expected);
				bad_scache_chip( i, tmp ^ expected );
			}
			if( loop)
				goto restart;
		}
	}

	/* now check the second pattern in the secondary cache */
	/* Force the writeback */
	if( !errflag)
		puts("Force SC writeback (addr)\n\r");

	/* FORCE writeback */
	for(i= first_addr; i < last_addr ;  ) {
		*(u_int *)i = 0x0;
		i ++;
	}

	/* check the data in memory */
	k1addr = K0_TO_K1(last_addr + 0x100000);
	expected = PATTERN2;
	for(i= K0_TO_K1(first_addr + 0x100000); i < k1addr; i += 4) {
		tmp = *(u_int *)i;
		if( tmp != expected) {
			
			if( !errflag) {
				puts("Secondary Memory Error Pattern 2\n\r");
				puts("Address ");
				puthex(i);
				puts("\r\nexpected ");
				puthex(expected);
				puts(", actual ");
				puthex(tmp);
				puts(", XOR ");
				puthex( tmp ^ expected);
				bad_scache_chip( i, tmp ^ expected );
			}
			if( loop)
				goto restart;
		}
	}
	if( loop)
		goto restart;

	if( !errflag)
		puts("done");
	return(PASS);
}
