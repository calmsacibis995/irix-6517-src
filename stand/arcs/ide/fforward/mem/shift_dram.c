#ident "$Revision: 1.29 $"

#if TFP
#define SR_FORCE_NOERR (SR_PROMBASE & ~(SR_IBIT9|SR_IBIT10))
#endif
#if R4000 || R10000
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
#include "mem.h"

static int shift_loop (u_int, u_int *, u_int *, int);
static int shift(struct range *range, int delay_time, int pass_count);

#if 0
/* parallel printer port addresses for the Indigo function test voltage change   */
volatile unsigned int *p_ctrl = (unsigned int *)PHYS_TO_K1(PAR_CTRL_ADDR);
volatile unsigned char *p_stat = (unsigned char *)PHYS_TO_K1(PAR_SR_ADDR);
#endif

		/* 11-10-92 by Alan Raetz 
	     	 *
		 * This test puts a shifted data pattern into successive
		 * addresses repeatedly. It write/reads the entire memory
		 * space with the original starting pattern, then it 
		 * shifts the starting pattern, so each address location 
		 * gets every possible data pattern 

		 * 1_30 to 2_15_93: added voltage change: writes at 4.80 volts,
		 * reads at 5.25 volts. Added time delay between write and read, 
		 * additional patterns, refresh set to 125us, error log 
		 */

/* architecturally dependent DRAM address/data/parity test */	
int 
shift_dram(int argc, char *argv[])
{
	unsigned long		count;
	int			error = 0;
	struct range		range, range2;
	ulong			old_SR;
	int			pass_count;
	int 			delay_time = 2;
#ifndef IP28
	unsigned int		cpuctrl0;
	u_int                   cpuctrl0_cpr;
#ifdef IP22
	int                 orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int                 r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int                 r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;

	cpuctrl0_cpr = (orion|r4700|r5000) ? 0 : CPUCTRL0_CPR;
#else
	cpuctrl0_cpr = CPUCTRL0_CPR;
#endif
#endif
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

	if (mem_setup(1,argv[0],"",&range,&range2) != 0) {
		error = 1;
		goto done;
	}

	count = range.ra_count*range.ra_size;
	msg_printf(VRB, "DRAM shifted data test.\n"
			"Testing from 0x%x to 0x%x(%dM bytes)\n",
			range.ra_base,range.ra_base+count,count/0x100000);
#ifndef USE_MAPMEM
	if (range2.ra_base) {
		count = range2.ra_count*range2.ra_size;
		msg_printf(VRB, "Testing from 0x%x to 0x%x(%dM bytes)\n",
			range2.ra_base,range2.ra_base+count,count/0x100000);
	}
#endif

	/* clear CPU parity error */
	*(volatile int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
#ifndef IP28
	cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
		CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &
		~CPUCTRL0_R4K_CHK_PAR_N;
#else
	IP28_MEMTEST_ENTER();
#endif	/* !IP28 */

	/* Parity interrupt is disabled so the diag can report the error
	 * message on a data miscompare...
	 */
	old_SR = get_SR();
	set_SR(SR_FORCE_NOERR);

	error = shift(&range,delay_time,pass_count);
#ifndef MAP_MEM
	if (range2.ra_base)
		error += shift(&range2,delay_time,pass_count);
#endif

	set_SR(old_SR);
#ifndef IP28
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#else
	IP28_MEMTEST_LEAVE();
#endif	/* !IP28 */

	tlbpurge();

#if 0
	/* restore default refresh rate */
	*refresh = (u_int)0xc30;   
#endif

#if 0
    	/* restore to 5.0 volts */
    	*p_ctrl =  PAR_CTRL_RESET;
    	*p_ctrl = 0;
    	*p_stat = PRINTER_RESET | PRINTER_PRT;  
#endif

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
#if 0
    		/* change to 5.0 volts - prevents the system from
		 * resetting.
		 */
   		*p_ctrl =  PAR_CTRL_RESET;
    		*p_ctrl = 0;
    		*p_stat = PRINTER_RESET | PRINTER_PRT;  
		delay_lp(1);

    		change to 4.8 volts   
    		*p_ctrl =  PAR_CTRL_RESET;
    		*p_ctrl = 0;
    		*p_stat = PRINTER_PRT;  
		delay_lp(1);
#endif

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

#if 0
    		/* change to 5.0 volts - prevents the system from
		 * resetting
		 */
    		*p_ctrl =  PAR_CTRL_RESET;
    		*p_ctrl = 0;
    		*p_stat = PRINTER_RESET | PRINTER_PRT;  
    		delay_lp(1);

    		/* change to 5.25 volts */
    		*p_ctrl =  PAR_CTRL_RESET;
    		*p_ctrl = 0;
    		*p_stat = PRINTER_RESET ;  
#endif

    		delay_lp(delay_time);

		busy(1);

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
write_log (pass, addr, written, read) 
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


int read_log (pointer) 
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
