#ident "$Revision: 1.3 $"

#define SR_FORCE_NOERR ( (GetSR() | SR_IEC | 0x4000 ) & ~SR_DE)

#include "mem.h"
#include "setjmp.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/types.h"
#include "saioctl.h"
#include "uif.h"
#include "fault.h"
#include <arcs/types.h>
#include <arcs/signal.h>
#include <stdio.h>

static jmp_buf catch;
static int catch_intr();
extern int error_count;
extern int master;
int	volt_margin = 0;

/* parallel printer port addresses for the Indigo function test voltage change */  
volatile unsigned int *p_ctrl = (unsigned int *)PHYS_TO_K1(PAR_CTRL_ADDR);
volatile unsigned char *p_stat = (unsigned char *)PHYS_TO_K1(PAR_SR_ADDR);

#ifdef	PiE_EMULATOR
#define	BUSY_COUNT	0x8000
#else
#define	BUSY_COUNT	0x200000
#endif	/* PiE_EMULATOR */


	
	     	 /*
		 * This test puts a shifted data pattern into successive
		 * addresses repeatedly. It write/reads the entire memory
		 * space with the original starting pattern, then it 
		 * shifts the starting pattern, so each address location 
		 * gets every possible data pattern 

		 * added voltage change: writes at 4.80 volts,
		 * reads at 5.25 volts. Added time delay between write and read, 
		 * additional patterns, refresh set to 125us, error log 
		 */

/* architecturally dependent DRAM address/data/parity test */	
int 
shift_dram(argc, argv)
	int 	argc;
	char	*argv[];
{
	jmp_buf			faultbuf;
/*	extern int		*nofault; */
	register volatile u_int	*addr;
	int			cflag = 0;
	u_int			count;
	unsigned int		cpuctrl0;
	int			error = 0;
	u_int			last_phys_addr;
	extern void		map_mem(int);
	extern u_int		memsize;
	u_int			old_SR;
	struct range		range;
	u_int			data1;
	u_int			array[14]; 
	u_int 			next[14];
	u_int 			surround[2];

	u_int 			*addrhi;
	u_int                   addrhi_offset;
	u_int                   *addrlo;
	u_int			loop_count;			

	int			passes = 1;
	int			pass_count = 1;
	u_int			user_pattern = 0;
	register volatile u_int	*refresh;
	int                     orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int 			r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int 			r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	u_int                   cpuctrl0_cpr;
	

	busy(1);
	/* set up default */
	if (memsize <= MAX_LOW_MEM)
	{
		range.ra_base = FREE_MEM_LO;
	} else { 
		range.ra_base = KDM_TO_PHYS(FREE_MEM_LO);
		}
	count = memsize - (KDM_TO_PHYS(range.ra_base) - PHYS_RAMBASE);
	range.ra_size = sizeof(u_int);
	range.ra_count = count / range.ra_size;
	last_phys_addr = KDM_TO_PHYS(range.ra_base) + count;

	msg_printf(VRB, "DRAM shifted data test.\n");
	msg_printf(VRB, "Testing from 0x%x to 0x%x(%dM bytes)\n",range.ra_base,range.ra_base+count,count/0x100000);

	if (last_phys_addr > MAX_LOW_MEM_ADDR) {
		range.ra_base = KDM_TO_PHYS(range.ra_base);
		map_mem(cflag);
	}

	/* clear CPU parity error */

	old_SR = get_SR();
	/*
	set_SR(SR_BERRBIT | SR_IEC);

	Parity interrupt is disabled so the diag can report the error message 
	on a data miscompare...
	*/
	set_SR(SR_FORCE_NOERR);

	addrlo = (u_int *)range.ra_base;
	addrhi = addrlo + range.ra_count;
	addrhi_offset = ((u_int)addrhi - PHYS_RAMBASE) & 0x1fffffff;

	busy(0);

/* start patterns: each pattern is shifted during each succesive location: 
   the row above any address is address - 0x800, the row below is 
   address + 0x800. Data is shifted 16 bits to the left during each row. 
   NOTE: for some reason, the diag would get a UTLB exception when I made arrays 
	 with larger than 15 words.... don't ask me why 

   This set of patterns implements a surround bit test  */ 

	surround[0] = 0x1111eeee; surround[1] = 0xeeeeffff;

/* some semi-ordered patterns  */
	array[0] = 0xa64e5921; array[1] = 0x0000ffff; array[2] = 0x0f0f0f0f; array[3] = 0xef31ef31; 
	array[4] = 0x10ce10ce; array[5] = 0xff00ff00; array[6] = 0x01010101; array[7] = 0xfefefefe; 
	array[8] = 0x57f75707; array[9] = 0x750575f5; array[10] = 0xfc90fc90; array[11] = 0x13579adf; 
	array[12] = 0xf0a5c3e1; array[13] = 0xf87c3e1f; 
	
/* some more semi-random patterns  */

	next[0] = 0xffefff00; next[1] = 0x887c3eff; next[2] = 0xdeca5550; next[3] = 0x1234fff3; 
	next[4] = 0x036fe5af; next[5] = 0x1c5c95df; next[6] = 0x887c3eff; next[7] = 0xef31ef31; 
	next[8] = 0xff33ff00; next[9] = 0x803c0fff; next[10] = 0x13579adf; next[11] = 0xf87c3e1f;
	next[12] = 0xc66e5aff; next[13] = 0x9ee3cfe3; 

	switch (argc) {
	    case 1:
		break;
	    case 2:
		if (argv[1]=="?") {
				sh_usage();
				return(-1);
			} else if (argv[1] =="v") {
				volt_margin=1; 
				break; 
			} else {
				user_pattern = atoi(argv[1]);
				if ( (user_pattern < 31) && (user_pattern > 0) ) {
					pass_count = user_pattern; 
					msg_printf(VRB,"Using %d data pattern(s)\n  \n", pass_count);
				} else {
					user_pattern = atoi(argv[1]);
					msg_printf(VRB,"Using user-defined pattern = %x\n  \n", user_pattern);
				}
				break;
			}
 	    default: 	
		sh_usage();
		return (-1);
	}	

	if (user_pattern > 30) {
		error = shift_loop(user_pattern, addrlo, addrhi);
	} else {

	for (data1 = 0; (data1 != 2) && (passes <= pass_count) && (!error); data1++, passes++)  {
		error = shift_loop(surround[data1], addrlo, addrhi);
		}

	for (data1 = 0; (data1 != 14) && (passes <= pass_count) && (!error); data1++, passes++)  {
		error = shift_loop(next[data1], addrlo, addrhi);
		}

	for (data1 = 0; (data1 != 14) && (passes <= pass_count) && (!error); data1++, passes++)  {
		error = shift_loop(array[data1], addrlo, addrhi);
		}
	}

	okydoky();
	set_SR(old_SR);
	DMUXfix();

	/*
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
	busy(0);
	tlbpurge();
	*/

	if ( volt_margin ) {
    	/* restore to 5.0 volts */
    	*p_ctrl =  PAR_CTRL_RESET;
    	*p_ctrl = 0;
    	*p_stat = PRINTER_RESET | PRINTER_PRT;  
    	delay_lp(1);
	}
	if (error) msg_printf(VRB, "Failed DRAM Shift data test,\n");
	return (error);
}

int
sh_usage (void)
{
msg_printf (1,"\nUsage: shift (32 bit hex pattern) or shift (number of patterns [1-30])\n");
}

int
shift_loop (start, low_addr, hi_addr)

	register volatile u_int	start;
	register volatile u_int *low_addr;
	register volatile u_int *hi_addr;
	{
	register int		busy_count = 0;
	jmp_buf			faultbuf;
/*	extern int		*nofault; */
	int volatile		error=0;
	register volatile u_int *addr;
	register volatile u_int	start_pattern;
	register volatile u_int pattern;
	register u_int		check_bit = 0x80000000;
	register u_int	 	add_bit = 0x1;	

	start_pattern = start;
	hi_addr = hi_addr - (3*sizeof(u_int));
	msg_printf(VRB, "Data pattern = 0x%x\n", start_pattern); 

	do { 
		if ( volt_margin ) set_voltage_low(); 

		if (start_pattern & check_bit) {
			start_pattern <<= 1;
			start_pattern = start_pattern | add_bit;
		    } else {
			start_pattern <<= 1;
		}
		pattern = start_pattern;
		msg_printf(DBG, "\npattern = 0x%x\n", pattern); 
	     	for (addr = low_addr; addr < hi_addr; addr++) {

			/* write four words at a time (each simm) */
				*addr = (u_int)pattern; 
				addr++;
				*addr = (u_int)pattern; 
				addr++;
				*addr = (u_int)pattern; 
				addr++;
				*addr = (u_int)pattern; 

				if (pattern & check_bit) {
				pattern <<= 1;
				pattern = pattern | add_bit;
				} else {
				pattern <<= 1;
				}

			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			        }
			}	

    		/* READ AND VERIFY */
    		pattern = start_pattern;

    		if (volt_margin) set_voltage_high(); 

		for (addr = low_addr; addr < hi_addr; addr++) {
			if (*addr != (u_int) pattern) {
				memerr((u_int)addr, pattern, *addr, sizeof(u_int));  
				error++;
				}
			addr++;
			if (*addr != (u_int) pattern) {
				memerr((u_int)addr, pattern, *addr, sizeof(u_int));  
				error++;
				}
			addr++;
			if (*addr != (u_int) pattern) {
				memerr((u_int)addr, pattern, *addr, sizeof(u_int));  
				error++;
				}
			addr++;
			if (*addr != (u_int) pattern) {
				memerr((u_int)addr, pattern, *addr, sizeof(u_int));  
				error++;
				}

			if (pattern & check_bit) {
			pattern <<= 1;
			pattern = pattern | add_bit;
			} else {
			pattern <<= 1;
			}

			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			        }
		}	

	} while (start_pattern != start);   /* rotate pattern until it matches original pattern */  

return (error);
}

int 
set_voltage_low()
{
        /* change to 5.0 volts - prevents the system from resetting  */
    	*p_ctrl =  PAR_CTRL_RESET;
    	*p_ctrl = 0;
    	*p_stat = PRINTER_RESET | PRINTER_PRT;  
	delay_lp(1);

    	/* change to 4.8 volts */  
    	*p_ctrl =  PAR_CTRL_RESET;
    	*p_ctrl = 0;
    	*p_stat = PRINTER_PRT;  
	delay_lp(1);
}

int
set_voltage_high()
{
    	/* change to 5.0 volts  - prevents the system from resetting */
   	*p_ctrl =  PAR_CTRL_RESET;
    	*p_ctrl = 0;
    	*p_stat = PRINTER_RESET | PRINTER_PRT;  
    	delay_lp(1);

    	/* change to 5.25 volts */ 
    	*p_ctrl =  PAR_CTRL_RESET;
    	*p_ctrl = 0;
    	*p_stat = PRINTER_RESET ;  
    	delay_lp(1);
}

/* this timing dependent delay loop is used by shift_dram.c and dram2.c 
   to delay time between writes and reads   */

int 
delay_lp(intervals)
	int 	intervals;
	{
	register volatile int	loop1=0;
	register volatile u_int	loop2;
	register volatile int	op1=123456;
	register volatile int	op2=654321;
	register volatile int	op3;

	for ( ; loop1 < intervals; loop1++)
	{
	for ( loop2 = 0x3ffff; loop2 > 0; loop2--) {
	 	op3 = op2+op1;
		op1 = op3-op2;
		op2 = op1+op3;
		}
	}	

}


