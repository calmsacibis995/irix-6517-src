/*
 * shift_dram.c - architecturally dependent DRAM address/data/parity test 
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * architecturally dependent DRAM address/data/parity test 
 * 11-10-92 by Alan Raetz 
 *
 * This test puts a shifted data pattern into successive
 * addresses repeatedly. It write/reads the entire memory
 * space with the original starting pattern, then it 
 * shifts the starting pattern, so each address location 
 * gets every possible data pattern 

 * 1_30 to 2_15_93: added voltage change: writes at 4.80 volts,
 * reads at 5.25 volts. Added time delay between write and read, 
 * additional patterns, refresh set to 125us, error log 
 *
 * NOTE: for IP30, CPUCTRL0 was switched to HEART_MODE because that's
 *       where the ECC bits are set.
 *       what's left to do:	XXX
 *              - make sure that we need ForceMemWr true
 *              - delete all non-IP30 code (?)
 * 		- clear CPU parity error ?? XXX
 */
#ident "ide/godzilla/mem/shift_dram.c  $Revision: 1.11 $"

#if R4000
#define SR_FORCE_NOERR (SR_PROMBASE|SR_DE)
#endif

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "saioctl.h"
#include "setjmp.h"
#include "uif.h"
#include "fault.h"
#include <arcs/types.h>
#include <arcs/signal.h>
#include "libsc.h"
#include "libsk.h"
#include "d_mem.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "sys/RACER/heart.h"

/* forward declarations */
static int shift_loop (u_int, u_int *, u_int *, int);
static int shift(struct range *range, int delay_time, int pass_count);

int 
shift_dram(int argc, char *argv[])
{
	unsigned long		count;
	int			error = 0;
	struct range		range;
	ulong			old_SR;
	int			pass_count;
	int 			delay_time = 2;
	heartreg_t tmp_hr_buf[2] = {HEART_MODE, D_EOLIST};
	heartreg_t tmp_hr_save[2];

	switch (argc) {
	case 1:
		pass_count = 1;
		break;
	case 2:
		pass_count = convert_chars(&argv[1]); 
		break;
	case 3:
		pass_count = convert_chars(&argv[1]); 
		delay_time = convert_chars(&argv[2]);
		break;
 	default: 	
		msg_printf (1,"\nUsage: shift_dram [number of patterns] [delay time]\n");
		error = 1;
		goto done;
	}
	
	busy(1);

	if (mem_setup(1,argv[0],"",&range) != 0) {
		error = 1;
		goto done;
	}

	count = range.ra_count*range.ra_size;
	msg_printf(VRB, "DRAM shifted data test.\n"
			"Testing from 0x%x to 0x%x(%dM bytes)\n",
			range.ra_base,range.ra_base+count,count/0x100000);

	/* clear CPU parity error */
	/* XXX */

       	/* save the content of the control register */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);

       /* enable parity/ECC and error reporting */
	/* speedracer CPU control register is on HEART */
        /*  enable ECC and ECC error reporting (bit 15 and 3) */
        /*  set the memory forced write (??) bit 21 */
	d_errors += pio_reg_mod_64(HEART_MODE,
		HM_MEM_ERE | HM_GLOBAL_ECC_EN | HM_MEM_FORCE_WR, SET_MODE);

	/* for ref. only: the control register is on MC 		*/
	/* *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |	*/
	/*	CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &		*/
	/*	~CPUCTRL0_R4K_CHK_PAR_N;				*/

	/* Parity interrupt is disabled so the diag can report the error
	 * message on a data miscompare...
	 */
	old_SR = get_SR();
#if R4000
	set_SR(SR_FORCE_NOERR);
#elif R10000	/* find equiv XXX */
#endif

	error = shift(&range,delay_time,pass_count);

	set_SR(old_SR);

        /* restore the content of the control register */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

    	delay_lp(1);
done:
	busy(0);	
	return (error);
}

static int
shift(struct range *range, int delay_time, int pass_count)
{
	u_int *addrhi;
	u_int *addrlo;
	int passes = 1;
	int error = 0;
	u_int data1;
	u_int array[14]; 
	u_int next[14];
	u_int surround[2];

	addrlo = (u_int *)range->ra_base;
	addrhi = addrlo + range->ra_count;

	busy(1);

	/* start patterns: each pattern is shifted during each succesive
	 * location: the row above any address is address - 0x800, the row
	 * below is address + 0x800. Data is shifted 16 bits to the left
	 * during each row. 
	 *
	 * NOTE: for some reason, the diag would get a UTLB exception when
	 *	 I made arrays with larger than 15 words.... don't ask me why 
	 *
	 * This set of patterns implements a surround bit test
	 */ 

	surround[0] = 0x1111eeee; surround[1] = 0xeeeeffff;

	/* some semi-ordered patterns  */
	array[0] = 0xa64e5921; array[1] = 0x0000ffff; array[2] = 0x0f0f0f0f;
	array[3] = 0xef31ef31; array[4] = 0x10ce10ce; array[5] = 0xff00ff00;
	array[6] = 0x01010101; array[7] = 0xfefefefe; array[8] = 0x57f75707;
	array[9] = 0x750575f5; array[10] = 0xfc90fc90; array[11] = 0x13579adf; 
	array[12]= 0xf0a5c3e1; array[13] = 0xf87c3e1f; 
	
	/* some more semi-random patterns  */
	next[0] = 0xffefff00; next[1] = 0xfc90fc90; next[2] = 0xdeca5550;
	next[3] = 0x1234fff3; next[4] = 0x036fe5af; next[5] = 0x1c5c95df;		next[6] = 0x887c3eff; next[7] = 0xef31ef31; next[8] = 0xff33ff00;
	next[9] = 0x803c0fff; next[10] = 0x13579adf; next[11] = 0xf87c3e1f;
	next[12]= 0xc66e5aff; next[13] = 0x9ee3cfe3; 


	if (!error) {
	msg_printf(VRB,"Using %d data pattern(s) with write to read delay set to %d:\n", pass_count, delay_time);
#if 0
	/* reduce refresh rate from 62.5us to 125us */
	*(volatile unsigned int *)refresh = 0xbfa00044;
	*refresh = (u_int)0x1900;
#endif

	for (data1 = 0; (data1 != 2) && (passes <= pass_count); data1++, passes++)  {
		error = shift_loop(surround[data1], addrlo, addrhi, delay_time);
	}

	for (data1 = 0; (data1 != 15) && (passes <= pass_count); data1++, passes++)  {
		error = shift_loop(next[data1], addrlo, addrhi, delay_time);
	}

	for (data1 = 0; (data1 != 15) && (passes <= pass_count); data1++, passes++)  {
		error = shift_loop(array[data1], addrlo, addrhi, delay_time);
	}


	if (error == 0)
		okydoky();
	} else {
		msg_printf(VRB,"Failed DRAM Shift data test,\n");
		/*  read_log(error); */
	} 

	return error;
}


static int
shift_loop (u_int pstart, u_int *plow_addr, u_int *phi_addr, int delay_time)
{
	register volatile u_int	start = pstart;
	register volatile u_int *low_addr = plow_addr;
	register volatile u_int *hi_addr = phi_addr;
	int volatile		error=0;
	register volatile u_int *addr;
	register volatile u_int	start_pattern;
	register volatile u_int pattern;
	register u_int		check_bit = 0x80000000;
	register u_int	 	add_bit = 0x1;	
	register volatile u_int	pass=0;
	register volatile u_int	rd_pat;

	start_pattern = start;
	msg_printf(VRB, "Data pattern = 0x%x\n", start_pattern); 

	busy(1);

	do { 
		rd_pat = start_pattern;
		if (start_pattern & check_bit) {
			start_pattern <<= 1;
			start_pattern = start_pattern | add_bit;
		} else {
			start_pattern <<= 1;
		}
		pattern = start_pattern;
		msg_printf(DBG, "\npattern = 0x%x\n", pattern); 
     		for (addr = low_addr; addr < hi_addr; addr++) {
			*addr = (u_int)pattern; 
			rd_pat = pattern;
			if (pattern & check_bit) {
				pattern <<= 1;
				pattern = pattern | add_bit;
			} else {
				pattern <<= 1;
			}
		}

		busy(1);

		/* READ AND VERIFY */
    		addr = low_addr;
		pattern = start_pattern;

    		delay_lp(delay_time);

		busy(0);

		for (; addr < hi_addr; addr++) {
			if (*addr != (u_int) pattern) {
				memerr((caddr_t)addr, pattern, *addr,
					sizeof(u_int));  
			}
			if (pattern & check_bit) {
				pattern <<= 1;
				pattern = pattern | add_bit;
			} else {
				pattern <<= 1;
			}
		}	
		pass++;

		busy(1);

	} while (start_pattern != start);   /* shift entire 32 bits */  

	msg_printf(VRB," done\n");
	return (error);
}


/* this timing dependent delay loop is used by shift_dram.c and dram2.c 
   to delay time between writes and reads   -AR 1-20-93 */

void 
delay_lp(int intervals)
{
	register volatile int	loop1=0;
	register volatile u_int	loop2;
	register volatile int	op1=123456;
	register volatile int	op2=654321;
	register volatile int	op3;

	for ( ; loop1 < intervals; loop1++) {
		for ( loop2 = 0x3ffff; loop2 > 0; loop2--) {
	 		op3 = op2+op1;
			op1 = op3-op2;
			op2 = op1+op3;
		}
	}	
}

int
convert_chars (char *ptr[])
{
	int	ones;
	int	tens;
	int 	result;
	int 	bad_var=0;

	if ((*ptr)[1] == '\0') {
		ones = (*ptr)[0];
		if ( ones < '0' || ones > '9')
			bad_var++;
		result = ones - '0';
	}
	else {		
		if ((*ptr)[2] == '\0') {
			tens = (*ptr)[0];
			ones = (*ptr)[1];	
			if (tens < '0' || ones < '0' || tens > '9' || ones > '9')
				bad_var++;
			result = ((tens - '0') * 10) + (ones - '0');
		}
	}
	if (bad_var)
		return(1);

	return(result);
}

/*
int
write_log (pass, addr, written, read)   XXX of interest
	u_int pass;
	u_int addr;
	u_int written;
	u_int read;
	{
	static int	fail_ptr = 0;

	if (fail_ptr > 49 ) fail_ptr = 1;

	each_failure[fail_ptr].pass = pass;
	each_failure[fail_ptr].addr = addr;
	each_failure[fail_ptr].written = written;
	each_failure[fail_ptr].read = read;
	fail_ptr++;
	return (fail_ptr);
}	


int read_log (pointer) 			XXX of interest
  int pointer;
	{
	int	read_ptr;
	while (pointer > 0 && pointer < 50 )
		{
		for (read_ptr=0; read_ptr <=  pointer; read_ptr++) {
			printf("The following errors were detected:\n");
			printf("pass %d, address = %x, written = %x, read = %x\n", each_failure[read_ptr].pass, 
			each_failure[read_ptr].addr, each_failure[read_ptr].written, each_failure[read_ptr].read);
			if (read_ptr == 23)  wait();
			}	
  		} 
		wait();
}
*/
