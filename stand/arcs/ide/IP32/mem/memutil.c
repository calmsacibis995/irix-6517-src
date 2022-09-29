#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <fault.h>
#include <regdef.h>
#include "mem.h"

#ident "$Revision: 1.3 $"


#define WRD0    0
#define WRD1    0x4
#define WRD2    0x8
#define WRD3    0xc

#ifndef IP32
extern char *simm_map_fh[3][4];
extern char *simm_map_g[2][4];
#endif /* !IP32 */

static int getsimmx(void *, int, int);

extern u_int memsize;

static char *err_string[] = {
        "Address: 0x%08x, Expected: 0x%02x, Actual: 0x%02x, XOR: 0x%02x\n",
        "Address: 0x%08x, Expected: 0x%04x, Actual: 0x%04x, XOR: 0x%04x\n",
        "Address: 0x%08x, Expected: 0x%08x, Actual: 0x%08x, XOR: 0x%08x\n",
};


void
memerr(caddr_t badaddr, __psunsigned_t expected, __psunsigned_t actual, int size)
{
        msg_printf(ERR, err_string[size >> 1],
                badaddr, expected, actual, expected ^ actual);
	getbadsimm(badaddr,(expected ^ actual));
}



static void
memtestcommon_usage(char *progname)
{
	msg_printf(ERR, "usage: %s [beginaddr:endaddr|beginaddr#size]\n",
		progname);
}


int
mem_setup(int setupargc, char *testname, char *rangestring,
		struct range *localrange, u_int *count,
		u_int *last_phys_addr, int *cflag)
{
	u_int loop_count = 1;
	int error = 0;
  
	msg_printf(DBG,"mem_setup()\n%s\t%s",testname,rangestring);
  
	if (memsize <= MAX_LOW_MEM)
		localrange->ra_base = FREE_MEM_LO;
	else
		localrange->ra_base = KDM_TO_PHYS(FREE_MEM_LO);
  
	*count = memsize - (KDM_TO_PHYS(localrange->ra_base) - PHYS_RAMBASE);
	localrange->ra_size = sizeof(u_int);
	localrange->ra_count = *count / localrange->ra_size;

	/*  This next statement isn't currently used, because KDM_TO_PHYS takes
 	* the address to Kuseg space, and at this point it will be in Kseg1
 	*/
	*last_phys_addr = KDM_TO_PHYS(localrange->ra_base) + *count;
  
	msg_printf(DBG,"Default:\nra_size=%x\tra_base=%x\t",
		localrange->ra_size, localrange->ra_base);
	msg_printf(DBG,"ra_count=%x\n",localrange->ra_count);

	if (setupargc == 2)  {
		parse_range(rangestring, sizeof(u_int), localrange);
	}
	else {
		if (setupargc > 2) {
			printf("Incorrect argument list\n");
			memtestcommon_usage(testname);
			return -1;
		}
	}
  
	/*  Set cflag for map_mem purposes later */
	*cflag = IS_KSEG0(localrange->ra_base) ? 1 : 0;
	*count = localrange->ra_size * localrange->ra_count;
  
	if (*last_phys_addr > MAX_LOW_MEM_ADDR) {
		localrange->ra_base = KDM_TO_PHYS(localrange->ra_base);
		map_mem(*cflag);
	}

	msg_printf(DBG,"The current values of range are:\n");
	msg_printf(DBG,"base=%x\tcount=%x\t", localrange->ra_base,
			localrange->ra_count);
	msg_printf(DBG,"size=%x\n",localrange->ra_size);
	msg_printf(DBG,"last_phys_addr=%x\tcount=%x\tcflag=%d\n",
			*last_phys_addr, *count, *cflag);
	return(0);
}


void testsimm(void)
{

int val;

val = getbadsimm(0xa8800ff4,0x1); 
val = getbadsimm(0xa8800ff0,0x1);
val = getbadsimm(0xa8800ff8,0xf0000);
val = getbadsimm(0xa8800ffc,0x800000);

val = getbadsimm(0xaa000000,0x1);
val = getbadsimm(0xa9fffffc,0x8);

printf("Exiting testsimm\n");

}

/* Finds the bad sim based on the failing address and data bits
 */
int
getbadsimm(void *fail_addr, ulong bad_bits)
{
	u_int base0_addr_info, base1_addr_info, base2_addr_info;
	u_int mem_config0, mem_config1;
	ulong fail_addr_info;
	u_int badbnk;
  
	msg_printf(DBG,"-----badsimm_guinness with addr%x and bits%x\n",
		fail_addr, bad_bits);

#ifndef IP32
	badbnk = ip22_addr_to_bank( (void *)KDM_TO_PHYS(fail_addr) );
#endif /* !IP32 */

	fail_addr = (void *)KDM_TO_PHYS(fail_addr);

	return(getsimmx(fail_addr, badbnk, bad_bits));
}


static int
getsimmx(void *bad_addr, int badbnk, int bad_bits)
{
#ifndef IP32
	char **bankstr;
#ifndef IP32
        int fh = is_fullhouse();
#else /* !IP32 */
        int fh = 0;
#endif /* !IP32 */
	int dmux;
	msg_printf(DBG,"-----getsimmx:  ");
	
/*	printf("%s\n",simm_map_g[1][1]);
	printf("%d is the bad bank\n",badbnk); */
/*	if (is_fullhouse())
		bankstr = simm_map_fh[badbnk];
	else
		bankstr = simm_map_g[badbnk];

*/	

	dmux=(bad_bits & 0xffff);
	msg_printf(DBG,"addr %x, bank %x, bits 0x%x\n", bad_addr, badbnk, bad_bits);
	msg_printf(VRB,"** Check SIM:");
	msg_printf(VRB,"%s ", fh? simm_map_fh[badbnk][( (ulong)bad_addr & 0xc)>>2] : simm_map_g[badbnk][( (ulong)bad_addr & 0xc)>>2]);
	if (bad_bits & 0xffff) {
		msg_printf(VRB,"%s "," DMUX chip U66");
		}
	if (bad_bits & 0xffff0000) {
		msg_printf(VRB,"%s "," DMUX chip U50");
		}
	printf("\n");
#else /* !IP32 */
	msg_printf(VRB, "Error: bad_addr==0x%x,badbnk==0x%x,bad_bits==0x%x\n",
			bad_addr, badbnk, bad_bits);
#endif /* !IP32 */
}

/*
 *			f i l l m e m W _ l( )
 *
 * fillmemW- fills memory from the starting address to ending address with the
 * data pattern supplied. Or if the user chooses, writes are done from ending
 * address to the starting address. Writes are done in multiples of 8 to reduce
 * the overhead. The disassembled code takes 12 cycles for the 8 writes verses
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
fillmemW(u_int *begadr, u_int *endadr, u_int pat1, u_int direct)
{
	/* This code is run in cached mode so a switch statement is not
	 * used because it will pop the code into uncached space since the
	 * code was linked in K1 space.
	 */
	if (direct == 0) {
		/*
	 	 * Do the writes in multiples of 8 for speed
	 	 */
		while (begadr <= (endadr -  8)) {
			*begadr = pat1;
			*(begadr + 1) = pat1;
			*(begadr + 2) = pat1;
			*(begadr + 3) = pat1;
			*(begadr + 4) = pat1;
			*(begadr + 5) = pat1;
			*(begadr + 6) = pat1;
			*(begadr + 7) = pat1;
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
		for( ;endadr >= (begadr -  8);) {
			*endadr = pat1;
			*(endadr - 1) = pat1;
			*(endadr - 2) = pat1;
			*(endadr - 3) = pat1;
			*(endadr - 4) = pat1;
			*(endadr - 5) = pat1;
			*(endadr - 6) = pat1;
			*(endadr - 7) = pat1;
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
  
	register u_int tmpbuf;
	/*
	 * A switch statement isn't used because the code is linked in
	 * k1 space. This code is run in k0 space (cached) a switch statement
	 * will pop us into k1 space, thus slowing the code down.
	 */
	msg_printf(VRB,"blkrd ( %x  %x )\n",*begadr,*endadr);
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
		for ( ;begadr <= endadr; ) {
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
			for ( ;begadr <= endadr; ) {
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
			for ( ;begadr <= endadr; ) {
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
			for ( ;begadr <= endadr; ) {
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
	msg_printf(VRB,"blkwrt ( %x  %x )\n",*begadr,*endadr);
        if( blksize == 16) {
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
                for ( ;begadr <= endadr; ) {
                        *begadr++ = pattern;
                }

	      }  /* end if */
	else  if( blksize == 32) {
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
                for ( ;begadr <= endadr; ) {
                        *begadr++ = pattern;
                }

	      }  /* end if */

        else if( blksize == 64) {
                /*
                 * Do the writes
                 */
                for( ;begadr <= (endadr -  144);) {
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
                for ( ;begadr <= endadr; ) {
                         *begadr++ = pattern;
                }  /* end for */
        }  /* end if */
        else if( blksize == 128) {
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
                for ( ;begadr <= endadr; ) {
                         *begadr++ = pattern;
                } /* end for */
        }  /* end if */
        else {
                printf("Unknown blocksize\n");
        }

}


/*  This routine is essentially the same as mem_setup, except for different
 * parsing requirements needed to accomodate an extra argc.
 */
int
menumem_setup(int setupargc, char *testname, char *rangestring,
	      struct range *localrange, u_int *count,
	      u_int *last_phys_addr, int *cflag)
{
	u_int loop_count = 1;
	int error = 0;
  
	msg_printf(DBG,"newmem_setup()\n%s\t%s",testname,rangestring);
  
	if (memsize <= MAX_LOW_MEM)
		localrange->ra_base = FREE_MEM_LO;
	else
		localrange->ra_base = KDM_TO_PHYS(FREE_MEM_LO);
  
	*count = memsize - (KDM_TO_PHYS(localrange->ra_base) - PHYS_RAMBASE);
	localrange->ra_size = sizeof(u_int);
	localrange->ra_count = *count / localrange->ra_size;

	/* This next statement isn't currently used, because KDM_TO_PHYS takes
	 * the address to Kuseg space, and at this point it will be in Kseg1
	 */
	*last_phys_addr = KDM_TO_PHYS(localrange->ra_base) + *count;
  
	msg_printf(DBG,"Default:\nra_size=%x\tra_base=%x\t",
		localrange->ra_size, localrange->ra_base);
	msg_printf(DBG,"ra_count=%x\n", localrange->ra_count);

	if (setupargc == 3)  {
		parse_range(rangestring, sizeof(u_int), localrange);
	}
	else if (setupargc > 3) {
		printf("Incorrect argument list\n");
		memtestcommon_usage(testname);
		return -1;
	}
  
	/*  Set cflag for map_mem purposes later */
	*cflag = IS_KSEG0(localrange->ra_base) ? 1 : 0;
	*count = localrange->ra_size * localrange->ra_count;
  
	if (*last_phys_addr > MAX_LOW_MEM_ADDR) {
		localrange->ra_base = KDM_TO_PHYS(localrange->ra_base);
		map_mem(*cflag);
	}

	msg_printf(DBG,"The current values of range are:\n");
	msg_printf(DBG,"base=%x\tcount=%x\t", localrange->ra_base,
		localrange->ra_count);
	msg_printf(DBG,"size=%x\n",localrange->ra_size);
	msg_printf(DBG,"last_phys_addr=%x\tcount=%x\tcflag=%d\n",*
		last_phys_addr, *count, *cflag);

	return(0);
}
