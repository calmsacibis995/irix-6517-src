#define	SR_FORCE_ON	(SR_CU0 | SR_CU1 | SR_CE | SR_IEC)
#define	SR_FORCE_OFF	(SR_CU0 | SR_CU1 | SR_IEC)
#define	SR_FORCE_NOERR	(SR_CU0 | SR_CU1 | SR_CE | SR_DE )
#define	SR_NOERR	(SR_CU0 | SR_CU1 | SR_DE )

#ident "$Revision: 1.3 $"

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

#ifdef	PiE_EMULATOR
#define	BUSY_COUNT	0x8000
#else
#define	BUSY_COUNT	0x200000
#endif	/* PiE_EMULATOR */

#define MOD & 
extern unsigned int *p_ctrl;
extern unsigned char *p_stat;

/* 
	  * changing algorithm to do Explicit Test Word Operations 
	  * (ETWO) which:
	  * 1) starts at location 0
	  * 2) reads previous pattern
	  * 3) writes new pattern
	  * 4) increments to location 1
	  * 5) reads previous pattern
	  * 6) writes new pattern
	  * 7) etc.
	  * This algorithm assures that if a lower address write operation 
	  * overwrites a higher address, that the failure will be caught
	  * (a normal block write/read will not catch this)

volatile unsigned int *p_ctrl = (unsigned int *)PHYS_TO_K1(PAR_CTRL_ADDR);
volatile unsigned char *p_stat = (unsigned char *)PHYS_TO_K1(PAR_SR_ADDR);
*/

int 
dram2_diagnostics(int argc, char *argv[])
{

	register int		busy_count = 0;
	int			cflag = 0;
	char			*command;
	unsigned int		cpuctrl0;
	int			error = 0;
	jmp_buf			faultbuf;
	int			busy_bit = 0;
	u_int			last_phys_addr;
	extern void		map_mem(int);
	extern u_int		memsize;
/*	extern int		*nofault;  */
	u_int			old_SR;
	struct range		range;
	register u_int		value;
	u_int			count;
	u_int                   *addrlo;
	u_int 			*addrhi;
	u_int                   addrhi_offset;

	u_int			data_ptr;
	u_int			offset;
	u_int			old_pat;
	u_int			old_offset;
	u_int			old_pat_size;
	int			loop;
	int 			patterns = 1;
	int 			delay_time = 2;
	int 			blocks;
	u_int			array[16];
	register volatile u_int	*refresh;
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	u_int                   cpuctrl0_cpr;

	/* shift_size      the increment value for each shift from the first address
			   (the starting offset)

	   max_shift       the maximum number of shifts of size shift_size to do  
			   (the maximum starting offset)

	   pattern_size    the block size of each data pattern (#of addresses)
			   -the data gets inverted each count of pattern_size  

	   Because the system accesses 4 words in parallel (2 words x 2
	   interleaves) internal DRAM addresses are incremented every four
	   word addresses. The larger shift sizes are so the pattern alternates 
	   on the rows.  */

	u_int 	shift_size[2] =       { 
		4,   1024 	};
	u_int	max_shift[2] =        { 
		4,   8 	};
	u_int	pattern_size[2]  =    { 
		16,  8192 	};

	/* test length will be the sum of all max_shift patterns, 
	   ie, 4 + 8 = 12 wr/rds of memory space per data pattern */  

	/*  some semi-ordered patterns  */
	array[0] = 0xc3c3c3c3; 
	array[1] = 0xa5a5a5a5; 
	array[2] = 0x1111eeee; 
	array[3] = 0xef31ef31;
	array[4] = 0x10ce10ce; 
	array[5] = 0xff00ff00; 
	array[6] = 0x01010101; 
	array[7] = 0xfefefefe;

	array[8] = 0x57f75707; 
	array[9] = 0x750575f5; 
	array[10] = 0xfc90fc90; 
	array[11] = 0x13579adf;
	array[12] = 0xeeeeffff; 
	array[13] = 0xf0a5c3e1; 
	array[14] = 0xf87c3e1f; 
	array[15] = 0xa64e5921;

	/* set up default */
	if (memsize <= MAX_LOW_MEM)
		range.ra_base = FREE_MEM_LO;
	else
		range.ra_base = KDM_TO_PHYS(FREE_MEM_LO);

	count = memsize - (KDM_TO_PHYS(range.ra_base) - PHYS_RAMBASE);
	range.ra_size = sizeof(u_int);
	range.ra_count = count / range.ra_size;
	last_phys_addr = KDM_TO_PHYS(range.ra_base) + count;

	if (last_phys_addr > MAX_LOW_MEM_ADDR) {
		range.ra_base = KDM_TO_PHYS(range.ra_base);
		map_mem(cflag);
	}

	old_SR = get_SR();
	set_SR(SR_IEC);
	/*
	set_SR(SR_BERRBIT | SR_IEC);
	set_SR(SR_FORCE_NOERR);
	*/

	addrlo = (u_int *)range.ra_base;
	addrhi = addrlo + range.ra_count;
	addrhi_offset = ((u_int)addrhi - PHYS_RAMBASE) & 0x1fffffff;

	msg_printf(VRB, "Data block pattern test, Base: 0x%08x, Size: %dM bytes\n",
	    range.ra_base, count/0x100000);
	msg_printf(VRB, "Testing from 0x%x to 0x%x(%dM bytes)\n\n",range.ra_base,range.ra_base+count,count/0x100000);

	switch (argc) {
	case 1:
		patterns = 2;
		break;
	case 2:
		patterns = atoi(argv[1]);
		break;
	case 3:
		patterns = atoi(argv[1]);
		delay_time = atoi(argv[2]);
		break;
	default:
		msg_printf (1,"\nUsage: dram2 (number of patterns 1-16) (delay time)\n");
		error = 1;
		break;
	}
	if (patterns > 16)  {
		error = 1;
		msg_printf (1,"\nUsage: dram2 (number of patterns 1-16) (delay time)\n");
	}

	if (!error) {

		for (loop = 0; loop < 2; loop++) {

			for (offset=0; offset <= (shift_size[loop]*max_shift[loop]); offset = offset + shift_size[loop]) {

				/* when offset = 0, set old_offset to top of memory (count) so loop will be write only */
				old_offset = (!offset) ? count : offset - shift_size[loop];

				for (data_ptr = 0; data_ptr < patterns; data_ptr++)
				{
					if (data_ptr) { 
						old_pat = array[data_ptr-1]; /* look at the previous pattern */

					} else old_pat = array[patterns-1];  /* or if the 1st, look at the very last pattern */


					old_pat_size = pattern_size[loop];

					msg_printf(DBG,"Starting loop: data = %x / %x, offset = %x, pattern size = %x\n\n",

					    array[data_ptr], ~array[data_ptr], offset, pattern_size[loop]);

					if (block_dram (array[data_ptr], addrlo, addrhi, offset, pattern_size[loop], 
					    old_pat, old_offset, old_pat_size)) error++;
					old_offset = offset;
					delay_lp(delay_time);
					msg_printf(DBG," done.\n\n");
				}       /* data loop             */
			}           /* shift position loop   */
		}                /*  loop thru each block size   */


	}
	/* *refresh = (u_int)0x0c30;  restore default refresh */

	nofault = 0;
	set_SR(SR_IEC);
	set_SR(old_SR);
	busy(0);
	tlbpurge();

	if (error){
		msg_printf(VRB,"FAILED dram test with %x errors", error);
		sum_error( "DRAM parity bit");
	} else {
		okydoky();
	}
	return error;
}

int
block_dram (pattern, low_addr, hi_addr, pattern_start, pattern_size, old_pattern, old_start, old_size)

register u_int		pattern;
register u_int		*low_addr;
register u_int 		*hi_addr;
register u_int	 	pattern_start;
register u_int		pattern_size;
register u_int		old_start;
register u_int		old_size;
register volatile u_int	old_pattern;
{
	register int		busy_count=0;
	register volatile u_int *addr;
	register volatile u_int	rd_pos = 0;
	register volatile u_int	wr_pos = 0;
	int volatile 		error=0;

	register volatile u_int top_int;
	register volatile u_int	*top;
	register volatile u_int	addr_int;
	register volatile u_int	hi_int = (u_int)hi_addr;
	register volatile u_int low_int = (u_int)low_addr;

	/* These loops check to see which pattern begins first and then 
	   executes either just the read or just the write and increments
	   the address. It was done this way to reduce the decision-making
	   (increase the speed) in the main read-write loop... 
	   - when old pattern start = addrhi, it does a write only
	   - when pattern start = addrhi, it does a read only

	   The start position is important because it sets up the internal
	   alignment of the bits in the DRAM 2-dimensional array. 
	*/
	busy(0);

	if (old_start == pattern_start)
	{    /* go directly to read-write loop */
		addr_int = low_int + (4*pattern_start);
		addr = (u_int *)addr_int;
	} else {       /* DO WRITE ONLY */

		if (old_start > pattern_start) {
			addr_int = low_int + pattern_start;
			top_int =  low_int + old_start;
			top =  (u_int *)top_int;
			addr = (u_int *)addr_int;
			/* top = address where reading begins */
			for (wr_pos=0; addr < top; addr++, wr_pos++ ) {
				if (wr_pos < pattern_size) {
					*addr = pattern;
				} else {
					wr_pos = 0;
					pattern = ~pattern;
					*addr = pattern;
				}
			}
		} else {        /* DO READ ONLY  */

			addr_int = low_int + (4*old_start);
			addr = (u_int *)addr_int;
			top_int =  low_int + (4*pattern_start);
			top =  (u_int *)top_int;
			/* top = address to begin read/write */
			for ( rd_pos=0; addr < top; addr++, rd_pos++ ) {
				if (rd_pos < old_size) {
					if (*addr != old_pattern) {
						error++;
						memerr((u_int)addr, old_pattern,
						    *addr, sizeof(u_int));
					}
				} else {
					rd_pos = 0;
					old_pattern = ~old_pattern;
					if (*addr != pattern) {
						error++;
						memerr((u_int)addr, old_pattern,
						    *addr, sizeof(u_int));
					}
				}
			}
		}
	}

	/* This is the real read-write loop. The read and write loops separately
   keep track of the pattern (either complemented or uncomplemented),
   and the position within the block size. If pattern = ~old_pattern, 
   then this will implement a read / write-inverted data loop
*/
	for (rd_pos, addr ; addr < hi_addr; addr++)
	{
		if (rd_pos < old_size) {      /* READ OLD PATTERN */
			if (*addr != old_pattern) {
				error++;
				memerr((u_int)addr, old_pattern,
				    *addr, sizeof(u_int));
			}
			rd_pos++;
		} else {
			rd_pos = 0;
			old_pattern = ~old_pattern;
			if (*addr != old_pattern) {
				error++;
				memerr((u_int)addr, old_pattern,
				    *addr, sizeof(u_int));
			}
			rd_pos++;
		}
		if (wr_pos < pattern_size) {   /* WRITE NEW PATTERN */
			*addr = pattern;
			wr_pos++;
		} else {
			wr_pos = 0;
			pattern = ~pattern;
			*addr = pattern;
			wr_pos++;
		}

		if (!busy_count--) {
			busy_count = BUSY_COUNT;
			busy(1);
		        }
	}
	return (error);
}
