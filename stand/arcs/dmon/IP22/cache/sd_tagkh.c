static char rcsid[] = "$Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/IP22/cache/RCS/sd_tagkh.c,v 1.1 1994/07/20 23:47:11 davidl Exp $";

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

/*
 *  Knaizuk Hartmann Memory Test
 *
 *  This algorithm is used to perform a fast but non-ehaustive memory test.
 *  It will test a memory subsystem for stuck-at faults in both the address
 *  lines as well as the data locations.  Wired or memory array behavior and
 *  non creative decoder design are assummed.  It makes only 4n memory accesses
 *  where n is the number of locations to be tested.  This alogorithm trades
 *  completeness for speed. No free lunch around here.  Hence this algorithm
 *  is not able to isolate pattern sensitivity or intermittent errors.  C'est
 *  la vie. This algorithm is excellent when a quick memory test is needed.
 *
 *  The algorithm breaks up the memory to be tested into 3 partitions.  Partion
 *  0 consists of memory locations 0, 3, 6, ... partition 1 consists of
 *  memory locations 1,4,7,...  partition 2 consists oflocations 2,5,8...
 *  The partitions are filled with either an all ones pattern or an all
 *  zeroes pattern.  By varying the order in which the partitions are filled
 *  and then checked, this algorithm manages to check all combinations
 *  of possible stuck at faults.  If you don't believe me, you can find
 *  a rigorous mathematical proof (set notation and every thing) in
 *  a correspondence called "An Optimal Algorithm for Testing Stuck-at Faults
 *  in Random Access Memories" in the November 1977 issue of IEEE Transactions
 *  on Computing, volume C-26 #11 page 1141.
 *
 *  INPUTS:  first_address - address of the first memory location to be tested
 *           last_address - address of the last 32 bit word to be tested.
 */

#ifdef	TOAD
#include "bup.h"
#include "types.h"

#else
#include "sys/types.h"
#endif	/*TOAD*/

#define PASS 0
#define FAIL 1
#define SKIP 2
#include "sys/sbd.h"

static int scache_size;		/* Secondary Cache size */
static int scache_linesize;	/* Secondary line size  */
static int dcache_size;		/* Primary Dcache size  */
static int dcache_linesize;	/* Primary Dcache linesize */

static u_int SdTagKh(int, int);

sd_tagkh()
{
	extern u_int scacherrx;

	/* Check for a secondary cache */
	if( noSCache() ) {
		puts("No Secondary Cache\r\n");
		return(SKIP);
	}

	SetSR(GetSR() | SR_DE);


	if(SdTagKh( 0xa8000000, 0xa80f0000)) 
		return(FAIL);
	else
		return(PASS);
}

	u_int quiet;
static
u_int SdTagKh(first_address, last_address)
int first_address;
int last_address;
{
	 u_int last_addr;
	 u_int last_value_read;
	 u_int local_expected_data;
	 u_int ptr;
	 u_int dmask;
	 u_int oneloc;
	 u_int twolocs;
	 u_int threelocs;
		 u_int loop;

	u_int first_addr;
	int error;

	/* Get the secondary cache size and line size */
	scache_size = size_2nd_cache();
	scache_linesize= scache_ln_size();

	/* Invalidate all the caches */
	invalidate_scache(scache_size, scache_linesize);

	first_address &= 0xfffffffc;
	last_address &= 0xfffffffc;

	dmask= 0xffffff80;
	scache_linesize= 0x80;
	
	/*
	 * Load the variables that enable us to jump to the
	 * next sequencial or 2/3 locations away. Since the address for the
	 * tag locations isn't sequencial in the address space.
	 */
	oneloc = scache_linesize;
	twolocs = scache_linesize * 2;
	threelocs = scache_linesize * 3;


	loop= GetOptions_Loop();
	quiet= GetOptions_Quiet();


restart:
	first_addr = first_address;
	last_addr = last_address;
	error = 0;

	/*
	 * Set partitions 1 and 2 to 0's.
	 */
	local_expected_data = 0xaaaaaaaa & dmask;
	for (ptr = first_addr + oneloc ; ptr <= last_addr; ptr += twolocs) {

		/* *ptr++ = local_expected_data; */
		IndxStorTag_SD(ptr, local_expected_data);

		/*ptr ++;*/
		ptr = ptr + oneloc;

		if (ptr <= last_addr) {
			/* *ptr = local_expected_data;*/
			IndxStorTag_SD( ptr, local_expected_data);
		}
	}

	/*
	 * Set partition 0 to ones.
	 */

	/* ask off bits 0-5 which aren't used */
	local_expected_data = (0x55555555 & dmask);

	for (ptr = first_addr; ptr <= last_addr; ptr += threelocs) {
		/* *ptr = local_expected_data; */
		IndxStorTag_SD( ptr, local_expected_data);
	}

	/*
	 * Verify partition 1 is still 0's.
	 */
	local_expected_data = 0xaaaaaaaa & dmask;
	for (ptr = first_addr + oneloc; ptr <= last_addr; ptr += threelocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read != local_expected_data) {
			error = 1;
			DISPLAYERROR(1, local_expected_data, last_value_read, ptr);
			if( loop)
				goto restart;
		}
	}

	/*
	 * Set partition 1 to ones.
	 */
	local_expected_data = 0x55555555 & dmask;
	for (ptr = first_addr + oneloc; ptr <= last_addr; ptr += threelocs) {

		/* *ptr = local_expected_data; */
		IndxStorTag_SD( ptr, local_expected_data);
	}

	/*
	 * Verify that partition 2 is zeroes.
	 */
	local_expected_data = 0xaaaaaaaa & dmask;
	for (ptr = first_addr + twolocs; ptr <= last_addr; ptr += threelocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read != local_expected_data) {

			error = 1;
			DISPLAYERROR(2, local_expected_data, last_value_read, ptr);
			if( loop)
				goto restart;
		}
	}

	/*
	 * Verify that partitions 0 and 1 are still ones.
	 */
	local_expected_data = (0x55555555  & dmask);
	for (ptr = first_addr; ptr <= last_addr; ptr += twolocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read == local_expected_data) {
		   	if((ptr = ptr + oneloc) <= last_addr) {
				last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
				if(last_value_read != local_expected_data) {

					error = 1;
					DISPLAYERROR(3, local_expected_data, last_value_read, ptr);
					if( loop)
						goto restart;
				}
			}
		}
		else {
			error = 1;
			DISPLAYERROR(3, local_expected_data, last_value_read, ptr);
			if( loop)
				goto restart;
		}
	}

	/*
	 * Set partition 0 to zeroes.
	 */
	local_expected_data = 0xaaaaaaaa & dmask;
	for (ptr = first_addr; ptr <= last_addr; ptr += threelocs) {

		/* *ptr = local_expected_data;*/
		IndxStorTag_SD( ptr, local_expected_data);
	}

	/*
	 * Check partition 0 for zeroes.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr += threelocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read != local_expected_data) {

			error = 1;
			DISPLAYERROR(4, local_expected_data, last_value_read, ptr);
			if( loop)
				goto restart;
		}
	}

	/*
	 * Set partition 2 to ones.
	 */
	local_expected_data = 0x55555555 & dmask;
	for (ptr = first_addr + twolocs; ptr <= last_addr; ptr += threelocs) {

		/* *ptr = local_expected_data; */
		IndxStorTag_SD( ptr, local_expected_data);
	}

	/*
	 * Check partition 2 for onees.
	 */
	local_expected_data = (0x55555555 & dmask);
	for (ptr = first_addr + twolocs; ptr <= last_addr; ptr += threelocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read != local_expected_data) {

			error = 1;
			DISPLAYERROR(5, local_expected_data, last_value_read, ptr);
			if( loop)
				goto restart;
		}
	}

done:
	puts("Done");
	invalidate_scache(scache_size, scache_linesize);
	if( loop)
		goto restart;
	if (error == 0) {
		return(PASS);
	}

	return(FAIL);
}
DISPLAYERROR(errcode, edata, adata, addr)
u_int errcode;
u_int edata;
u_int adata;
u_int addr;
{
	extern u_int quiet;
	
	puts("Error\r\n");
	if( !quiet) {
		puts("Address ");
		puthex(addr);
		puts(",  error code ");
		puthex(errcode);
		puts("\r\nexpected ");
		puthex(edata);
		puts(", actual ");
		puthex(adata);
		bad_stag_chip( edata ^ adata);
	}
}

/* STAG bits as on the schematics */
#define BITS_24_9	0xffff0000	/* U7 */
#define BITS_8_0	0x0000ff80	/* U10 */

bad_stag_chip( u_int bad_bit)
{
	u_int i, tmp;

	/* Shift contents of TAG register over
	 * to reflect the bits on the schematic
	 */
	tmp = bad_bit >> 7;
	puts("\r\nFailure on STAG bit(s):\r\n");

	for(i=0; i< 25; i++ ) {
		if( tmp & 1)
			putdec( i, 3);
		tmp >>= 1;
	}

	/* Determine failing chip for the different fabs */
	puts("\r\nProbable failing chip:\r\n");
	puts("  FAB 3 -> ");
	if( bad_bit & BITS_24_9)
		puts("u7 ");
	if( bad_bit & BITS_8_0)
		puts(" u10");

	puts("\r\n  FAB 4 -> ");
	if( bad_bit & BITS_24_9)
		puts("u8");
	if( bad_bit & BITS_8_0)
		puts(" u10");

	puts("\r\n");
}
