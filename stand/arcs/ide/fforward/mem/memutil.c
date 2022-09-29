#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <fault.h>
#include <libsc.h>
#include <libsk.h>
#include "mem.h"

#ident "$Revision: 1.29 $"

#define WRD0    0
#define WRD1    0x4
#define WRD2    0x8
#define WRD3    0xc

#if IP22 || IP26 || IP28
extern char *simm_map_fh[3][4];
#endif

#ifdef IP22
extern char *simm_map_g[2][4];
#endif

#ifdef IP20
int ip20_addr_to_bank(unsigned int);
extern char *simm_map[3][4];
#endif

/*
 * forward declarations
 */
static int getsimmx(caddr_t, __psunsigned_t, __psunsigned_t);

static void
memtestcommon_usage(char *progname)
{
	msg_printf(ERR, "usage: %s [beginaddr:endaddr|beginaddr#size]\n",
	    progname);
}


int
mem_setup(int setupargc, char *testname, char *rangestring,
	  struct range *lowrange, struct range *hirange)
{
	unsigned int count;
	int cached;

	msg_printf(DBG,"mem_setup: %s %s\n", testname, rangestring);
  
#ifdef _USE_MAPMEM
	if (memsize <= MAX_LOW_MEM)
		lowrange->ra_base = PROM_STACK;
	else
		lowrange->ra_base = KDM_TO_PHYS(PROM_STACK);
#else
	lowrange->ra_base = PROM_STACK;
#endif
  
	count = (unsigned int)(memsize -
		 (KDM_TO_PHYS(lowrange->ra_base) - PHYS_RAMBASE));

#if _MIPS_SIM == _ABI64
	lowrange->ra_size = sizeof(__psunsigned_t);
#else
	lowrange->ra_size = sizeof(u_int);
#endif	/* _K64U64 */
	lowrange->ra_count = count / lowrange->ra_size;

	msg_printf(DBG,"Default: base=%x size=%x count=%x\n",
		   lowrange->ra_base, lowrange->ra_size,
		   lowrange->ra_count);

	if (setupargc > 2) {
		msg_printf(VRB,"Incorrect argument list\n");
		memtestcommon_usage(testname);
		return -1;
	}

	if (setupargc == 2)
		parse_range(rangestring, lowrange->ra_size, lowrange);

	hirange->ra_base = 0;
	hirange->ra_count = 0;
#ifdef _K64U64
	hirange->ra_size = sizeof(__psunsigned_t);
#else
	hirange->ra_size = sizeof(u_int);
#endif	/* _K64U64 */

	count = (int)(lowrange->ra_size * lowrange->ra_count);
	cached = IS_KSEG0(lowrange->ra_base) ? 1 : 0;

#ifdef SEG1_BASE			/* IP28 can have no hole! */
	if ((KDM_TO_PHYS(lowrange->ra_base) + count) > MAX_LOW_MEM_ADDR) {
#ifdef _USE_MAPMEM
		lowrange->ra_base = (long)(KDM_TO_PHYS(lowrange->ra_base));
		map_mem(cached);
#else
		hirange->ra_base = (long)(SEG1_BASE | (cached ? K0BASE:K1BASE));
		hirange->ra_count = (memsize-cpu_get_low_memsize()) /
				      lowrange->ra_size;
		lowrange->ra_count -= hirange->ra_count;
#endif
	}
#endif	/* SEG1_BASE */

	msg_printf(DBG,"Updated: base=%x count=%x size=%x\n",
		   lowrange->ra_base, lowrange->ra_count,
		   lowrange->ra_size);

	if (hirange->ra_base)
		msg_printf(DBG,"Updated(hi): base=%x count=%x size=%x\n",
			   hirange->ra_base, hirange->ra_count,
			   hirange->ra_size);

	return(0);
}


/* Finds the bad sim based on the failing address and data bits
 */
int
getbadsimm(caddr_t fail_addr, __psunsigned_t bad_bits)
{
	__psunsigned_t	badbnk;

	msg_printf(DBG,"-----badsimm at addr%x and bits %x\n",
	    fail_addr, bad_bits);

#if IP22 || IP26  || IP28
	badbnk = ip22_addr_to_bank( (__uint32_t)KDM_TO_PHYS(fail_addr) );
#else
	badbnk = ip20_addr_to_bank( (__uint32_t)KDM_TO_PHYS(fail_addr) );
#endif /* IP22 || IP26 || IP28 */

	fail_addr = (caddr_t) KDM_TO_PHYS(fail_addr);

	return(getsimmx(fail_addr, badbnk, bad_bits));
}


static int
getsimmx(caddr_t  bad_addr, __psunsigned_t badbnk, __psunsigned_t bad_bits)
{
#ifdef IP22
        int fh = is_fullhouse();
#endif

	msg_printf(DBG,"\n-----getsimmx:  ");

	msg_printf(DBG,"addr %x, bank %x, bits 0x%x\n", bad_addr, badbnk,
		   bad_bits);
	msg_printf(INFO,"\n** Check SIMM: ");

#ifdef IP20
	msg_printf(INFO,"%s ", simm_map[badbnk][((ulong)bad_addr & 0xc)>>2]);
#endif

#ifdef IP22
	msg_printf(INFO,"%s ",
		   fh ? simm_map_fh[badbnk][( (ulong)bad_addr & 0xc)>>2] :
			simm_map_g [badbnk][( (ulong)bad_addr & 0xc)>>2]);
#endif

#ifdef IP26
	/* NOTE: the SIMM index is different in IP26 than IP22 */
	/* the words order is Word 0 in SIMM11, Word 1 in SIMM12, */
	/*                    Word 2 in SIMM10, Word 3 in SIMM9, */
	msg_printf(INFO,"%s ", simm_map_fh[badbnk][( ((ulong)bad_addr & 0x08)==0x0?0:2) + (( ((ulong)bad_bits & 0x00000000FFFFFFFF)==0x0)?0:1)]);
#endif

#ifdef IP28
	/* NOTE: the SIMM index is different in IP26 & IP28 than IP22 */
	/* the words order is Word 0 in SIMM11, Word 1 in SIMM12,     */
	/*                    Word 2 in SIMM10, Word 3 in SIMM9,      */
	/* simm pair is based on address, actual simm based on bad bits */
	if (((ulong)bad_addr & 0x08) == 0) {
		/* SIMM pair 11/12 */
		if (bad_bits & 0xFFFFFFFF00000000) {
			msg_printf(INFO, "%s ", simm_map_fh[badbnk][0]);
		}
		if (bad_bits & 0x00000000FFFFFFFF) {
			msg_printf(INFO, "%s ", simm_map_fh[badbnk][1]);
		}
	}
	else {
		/* SIMM pair  9/10 */
		if (bad_bits & 0xFFFFFFFF00000000) {
			msg_printf(INFO, "%s ", simm_map_fh[badbnk][2]);
		}
		if (bad_bits & 0x00000000FFFFFFFF) {
			msg_printf(INFO, "%s ", simm_map_fh[badbnk][3]);
		}
	}
#endif	/* IP28 */


	/* XXX what is this really on IP20, IP22 and IP22 base boards? */
#ifdef IP20
	if (bad_bits & 0xffff)
		msg_printf(VRB,"%s "," DMUX chip U66");
	if (bad_bits & 0xffff0000)
		msg_printf(VRB,"%s "," DMUX chip U50");
#endif /* IP20 */

	printf("\n");

	return 0;
}

int
testsimm(void)
{
	/* a call to testsimm can be added in cpu_diagcmds */

	(void)getbadsimm((caddr_t)PHYS_TO_K1_RAM(0x00000000),0x1); 
	(void)getbadsimm((caddr_t)PHYS_TO_K1_RAM(0x00000004),0x1); 
	(void)getbadsimm((caddr_t)PHYS_TO_K1_RAM(0x00000008),0x1); 
	(void)getbadsimm((caddr_t)PHYS_TO_K1_RAM(0x0000000C),0x1); 
	(void)getbadsimm((caddr_t)PHYS_TO_K1_RAM(0x00000000),0x80000000); 
	(void)getbadsimm((caddr_t)PHYS_TO_K1_RAM(0x00000004),0x80000000); 
	(void)getbadsimm((caddr_t)PHYS_TO_K1_RAM(0x00000008),0x80000000); 
	(void)getbadsimm((caddr_t)PHYS_TO_K1_RAM(0x0000000C),0x80000000); 

	msg_printf(VRB,"Exiting testsimm\n");
	return (1);
}

/*
 *			f i l l m e m W _ l( )
 *
 * fillmemW- fills memory from the starting address to ending address with the
 * data pattern supplied. Or if the user chooses, writes are done from ending
 * address to the starting address. Writes are done in multiples of 8 to reduce
 * the overhead. The disassembled code takes 12 cycles for the 8 writes versus
 * 32 cycles if done 1 word at a time.
 *
 * inputs:
 *	begadr = starting address
 *	endadr = ending address
 *	pat1   = pattern 1
 *	direction = 0= write first to last, 1= write last to first
 */

#define REVERSE 1
#define FORWARD 0

void
fillmemW(__psunsigned_t *begadr, __psunsigned_t *endadr, __psunsigned_t pat1,
	 __psunsigned_t direct)
{
	/* This code is run in cached mode so a switch statement is not
	 * used because it will pop the code into uncached space since the
	 * code was linked in K1 space.
	 */
	if (direct == 0) {
		/*
	 	 * Do the writes in multiples of 8 for speed
	 	 */

		msg_printf(DBG,"%x %x %x .. %x\n", begadr, (begadr + 1), (begadr + 2), (begadr + 7));
		while (begadr <= (endadr -  8)) {
			*begadr = pat1;
			*(begadr + 1) = pat1;
			*(begadr + 2 ) = pat1;
			*(begadr + 3 ) = pat1;
			*(begadr + 4 ) = pat1;
			*(begadr + 5 ) = pat1;
			*(begadr + 6 ) = pat1;
			*(begadr + 7 ) = pat1;
			begadr += 8;
		}
		/*
	 	 * Finish up remaining writes 
	 	 */
		for ( ;begadr <= endadr; ) {
			*begadr++ = pat1;
		}
	}
	else {
		/*
	 	 * Do the writes in multiples of 8 for speed, from last-> first 
	 	 */
		for( ;endadr >= ( begadr -  8 * sizeof(__psunsigned_t) ); ) {
			*endadr = pat1;
			*(endadr - 1 ) = pat1;
			*(endadr - 2 ) = pat1;
			*(endadr - 3 ) = pat1;
			*(endadr - 4 ) = pat1;
			*(endadr - 5 ) = pat1;
			*(endadr - 6 ) = pat1;
			*(endadr - 7 ) = pat1;
			endadr -= 8;
		}
		/*
	 	 * Finish up remaining writes 
	 	 */
		for ( ;endadr >= begadr; ) {
			*endadr-- = pat1;
		}
	}
}


/*
 *			b l k r d ( )
 *
 * blkrd - reads memory in block mode,i.e the reads are in K0 space. The cache 
 * block size is passed into this routine so that the appropriate
 * if statement can be executed. The reads are done in groups of 128 bytes.
 * Left over reads are done in a slower loop. The below C code causes sequencial
 * reads (lw reg, offset(reg) assembly code).
 *
 * inputs:
 *	begadr = starting address
 *	endadr = ending address
 *	blksize= cache block size
 */

void
blkrd(u_int *begadr, u_int *endadr, u_int blksize)
{
	volatile u_int tmpbuf;

	/*
	 * A switch statement isn't used because the code is linked in
	 * k1 space. This code is run in k0 space (cached) a switch statement
	 * will pop us into k1 space, thus slowing the code down.
	 */
	msg_printf(VRB,"blkrd(0x%x,0x%x)\n",begadr,endadr);
	if( blksize == 16) {
		/*
                 * Do the writes
                 */
		for( ;begadr <= (endadr - 100);) {
			tmpbuf = *begadr;
			tmpbuf = *(begadr + 8);
			tmpbuf = *(begadr + 12);
			tmpbuf = *(begadr + 16);
			tmpbuf = *(begadr + 20);
			tmpbuf = *(begadr + 24);
			tmpbuf = *(begadr + 28);
			tmpbuf = *(begadr + 32);
			tmpbuf = *(begadr + 36);
			tmpbuf = *(begadr + 40);
			tmpbuf = *(begadr + 44);
			tmpbuf = *(begadr + 48);
			tmpbuf = *(begadr + 52);
			tmpbuf = *(begadr + 56);
			tmpbuf = *(begadr + 60);
			tmpbuf = *(begadr + 64);
			tmpbuf = *(begadr + 68);
			tmpbuf = *(begadr + 72);
			tmpbuf = *(begadr + 76);
			tmpbuf = *(begadr + 80);
			tmpbuf = *(begadr + 84);
			tmpbuf = *(begadr + 88);
			tmpbuf = *(begadr + 92);
			tmpbuf = *(begadr + 96);
			begadr += 100;
		}  /* end for */
		/*
                 * Finish up remaining reads
                 */
		for ( ;begadr < endadr; ) {
			tmpbuf = *begadr++;
		}

	}
	else {
		if( blksize == 32) {
			/* Do the reads 
	 	 	 */
			for( ;begadr <= (endadr -  128);) {
				tmpbuf = *begadr;
				tmpbuf = *(begadr + 8);
				tmpbuf = *(begadr + 16);
				tmpbuf = *(begadr + 24);
				tmpbuf = *(begadr + 32);
				tmpbuf = *(begadr + 40);
				tmpbuf = *(begadr + 48);
				tmpbuf = *(begadr + 56);
				tmpbuf = *(begadr + 64);
				tmpbuf = *(begadr + 72);
				tmpbuf = *(begadr + 80);
				tmpbuf = *(begadr + 88);
				tmpbuf = *(begadr + 96);
				tmpbuf = *(begadr + 104);
				tmpbuf = *(begadr + 112);
				tmpbuf = *(begadr + 120);
				begadr += 128;
			}
			/* Finish up remaining reads
	 	 	 */
			for ( ;begadr < endadr; ) {
				tmpbuf = *begadr++;
			}

		}
		else if( blksize == 64) {
			/* Do the reads 
	 	 	 */
			for( ;begadr <= (endadr -  160);) {
				tmpbuf = *begadr;
				tmpbuf = *(begadr + 16);
				tmpbuf = *(begadr + 32);
				tmpbuf = *(begadr + 48);
				tmpbuf = *(begadr + 80);
				tmpbuf = *(begadr + 96);
				tmpbuf = *(begadr + 112);
				tmpbuf = *(begadr + 128);
				tmpbuf = *(begadr + 144);
				begadr += 160;
			}
			/* Finish up remaining reads
	 	 	 */
			for ( ;begadr < endadr; ) {
				tmpbuf = *begadr++;
			}
		}
		else if( blksize == 128) {
			/* Do the reads 
	 	 	 */
			for( ;begadr <= (endadr -  160);) {
				tmpbuf = *begadr;
				tmpbuf = *(begadr + 32);
				tmpbuf = *(begadr + 64);
				tmpbuf = *(begadr + 96);
				tmpbuf = *(begadr + 128);
				begadr += 160;
			}
			/* Finish up remaining reads
	 	 	 */
			for ( ;begadr < endadr; ) {
				tmpbuf = *begadr++;
			}
		}
		else {
			printf("Unknown blocksize\n");
		}
	}
}


/*
 *  Block writes in the same fashion as blkrd(), with the
 * additional parameter of the pattern to be written
 *
 * inputs:
 *      begadr = starting address
 *      endadr = ending address
 *      blksize= cache block size
 *      pattern = specified pattern
 */

void
blkwrt(register u_int *begadr, register u_int *endadr, register u_int blksize, 
       register u_int pattern)
{
	msg_printf(VRB,"blkwrt(0x%x,0x%x)\n",begadr,endadr);
	if (blksize == 16) {
		/*
                 * Do the writes
                 */
		for( ;begadr <= (endadr - 100);) {
			*begadr = pattern;
			*(begadr + 8) = pattern;
			*(begadr + 12) = pattern;
			*(begadr + 16) = pattern;
			*(begadr + 20) = pattern;
			*(begadr + 24) = pattern;
			*(begadr + 28) = pattern;
			*(begadr + 32) = pattern;
			*(begadr + 36) = pattern;
			*(begadr + 40) = pattern;
			*(begadr + 44) = pattern;
			*(begadr + 48) = pattern;
			*(begadr + 52) = pattern;
			*(begadr + 56) = pattern;
			*(begadr + 60) = pattern;
			*(begadr + 64) = pattern;
			*(begadr + 68) = pattern;
			*(begadr + 72) = pattern;
			*(begadr + 76) = pattern;
			*(begadr + 80) = pattern;
			*(begadr + 84) = pattern;
			*(begadr + 88) = pattern;
			*(begadr + 92) = pattern;
			*(begadr + 96) = pattern;
			begadr += 100;
		}  /* end for */
		/*
                 * Finish up remaining writes
                 */
		for ( ;begadr < endadr; ) {
			*begadr++ = pattern;
		}

	}  /* end if */
	else if (blksize == 32) {
		/*
                 * Do the writes
                 */
		for( ;begadr <= (endadr - 124);) {
			*begadr = pattern;
			*(begadr + 8) = pattern;
			*(begadr + 16) = pattern;
			*(begadr + 24) = pattern;
			*(begadr + 32) = pattern;
			*(begadr + 40) = pattern;
			*(begadr + 48) = pattern;
			*(begadr + 56) = pattern;
			*(begadr + 64) = pattern;
			*(begadr + 72) = pattern;
			*(begadr + 80) = pattern;
			*(begadr + 88) = pattern;
			*(begadr + 96) = pattern;
			*(begadr + 104) = pattern;
			*(begadr + 112) = pattern;
			*(begadr + 120) = pattern;
			begadr += 124;
		}  /* end for */
		/*
                 * Finish up remaining writes
                 */
		for ( ;begadr < endadr; ) {
			*begadr++ = pattern;
		}

	}  /* end if */

	else if (blksize == 64) {
		/*
                 * Do the writes
                 */
		for( ;begadr < (endadr -  144);) {
			*begadr = pattern;

			*(begadr + 16) = pattern;
			*(begadr + 32) = pattern;
			*(begadr + 48) = pattern;
			*(begadr + 64) = pattern;
			*(begadr + 80) = pattern;
			*(begadr + 96) = pattern;
			*(begadr + 112) = pattern;
			*(begadr + 128) = pattern;
			begadr += 144;
		} /* end for  */
		/*
                 * Finish up remaining writes
                 */
		for ( ;begadr < endadr; ) {
			*begadr++ = pattern;
		}  /* end for */
	}  /* end if */
	else if (blksize == 128) {
		/*
                 * Do the writes
                 */
		for( ;begadr <= (endadr -  160);) {

			*begadr = pattern;
			*(begadr + 32) = pattern;
			*(begadr + 64) = pattern;
			*(begadr + 96) = pattern;
			*(begadr + 128) = pattern;
			begadr += 160;
		}  /* end for */
		/*
                 * Finish up remaining writes
                 */
		for ( ;begadr < endadr; ) {
			*begadr++ = pattern;
		} /* end for */
	}  /* end if */
	else {
		printf("Unknown blocksize\n");
	}

}

static char *err_string[] = {
	"Address: 0x%x, Expected: 0x%02x, Actual: 0x%02x, XOR: 0x%02x\n",
	"Address: 0x%x, Expected: 0x%04x, Actual: 0x%04x, XOR: 0x%04x\n",
	"Address: 0x%x, Expected: 0x%08x, Actual: 0x%08x, XOR: 0x%08x\n",
	"Address: 0x%x, Expected: 0x%016x, Actual: 0x%016x, XOR: 0x%016x\n",
};

void
memerr(caddr_t badaddr, __psunsigned_t expected, __psunsigned_t actual, int size)
{
	msg_printf(ERR, err_string[size>=8? 3: size>>1],
	    badaddr, expected, actual, expected ^ actual);
	getbadsimm( badaddr, (expected ^ actual) );
}
