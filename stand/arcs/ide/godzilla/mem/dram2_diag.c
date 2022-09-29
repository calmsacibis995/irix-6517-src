/*
 * dram2_diag.c - 
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
 * more dram_diags (??)
 *
 * NOTE: for IP30, CPUCTRL0 was switched to HEART_MODE because that's
 *       where the ECC bits are set.
 *       what's left to do:  XXX
 *              - delete all non-IP30 code (?)
 */
#ident "ide/godzilla/mem/dram2_diag.c  $Revision: 1.11 $"
 
#if R4000
#define	SR_FORCE_ON	(SR_PROMBASE | SR_CE | SR_IEC)
#define	SR_FORCE_OFF	(SR_PROMBASE | SR_IEC)
#define	SR_FORCE_NOERR	(SR_PROMBASE | SR_CE | SR_DE )
#define	SR_NOERR	(SR_PROMBASE | SR_DE )
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

#define MOD & 
extern unsigned int *p_ctrl;
extern unsigned char *p_stat;
static int block_dram(u_int, u_int *, u_int *, u_int, u_int, u_int,
		      ulong, volatile u_int);

/* MODIFIED 1-28-93 by AR
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

static int dram2(struct range *range, int delay, int patterns);

int 
dram2_diagnostics(int argc, char *argv[])
{

	int			error = 0;
	ulong			old_SR;
	struct range		range;
	unsigned long		count;
	int 			patterns = 1;
	int 			delay_time = 2;
	heartreg_t		tmp_hr_buf[2] = {HEART_MODE, D_EOLIST};
	heartreg_t		tmp_hr_save[2];

	/* to prevent write-back since ide is cached */
	flush_cache();

	/* argc=1 -> no range parsing. */
	if (mem_setup(1,argv[0],"",&range) != 0) {
		error = 1;
		goto done;
	}

	argv++;

	switch (argc) {
	case 1:
		patterns = 1;
		break;
	case 2:
		patterns = convert_chars(argv);
		break;
	case 3:
		patterns = convert_chars(argv);
		argv++;
		delay_time = convert_chars(argv);
		break;
	default:
		error = 1;
		break;
	}
	if (patterns > 16)  {
		error = 1;
	}

	/* just a syntax error at this point */
	if (error) {
		msg_printf(SUM,"\nUsage: dram2 (number of patterns 1-16) (delay time)\n");
		return -1;
	}

	/* clear CPU parity error (was in mc's CPU_ERR_STAT)
	*/
	/* XXX */

        /* save the content of the control/mode register */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);

        /* enable parity/ECC and error reporting */
        /* speedracer CPU control register is on HEART */
        /*  enable ECC and ECC error reporting (bit 15 and 3) */
        /*  set the memory forced write (??) bit 21 */
	d_errors += pio_reg_mod_64(HEART_MODE,
		HM_MEM_ERE | HM_GLOBAL_ECC_EN | HM_MEM_FORCE_WR, SET_MODE);
	old_SR = get_SR();

#if R4000
#if IP30
        /* from heart.h */
	set_SR(SR_PROMBASE|SR_IEC|SR_IBIT_BERR); 	/* XXX */
#else
	set_SR(SR_PROMBASE|SR_IEC|SR_BERRBIT);
#endif  /* IP30 */
#elif TFP
	set_SR(SR_PROMBASE);
#endif

	count = range.ra_count*range.ra_size;
	msg_printf(VRB,"Data block pattern test\n");
	msg_printf(VRB, "Testing from 0x%x to 0x%x(%dM bytes)\n",
		range.ra_base,range.ra_base+count,count/0x100000);

	error = dram2(&range,delay_time,patterns);

	nofault = 0;
#ifdef R4000
#if IP30
        /* from heart.h */
	set_SR(SR_IBIT_BERR | SR_IEC); 	/* XXX */
#else
	set_SR(SR_BERRBIT | SR_IEC);
#endif  /* IP30 */
#endif
	set_SR(old_SR);

        /* restore the content of the modified heart registers */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

	busy(0);

done:
	if (error) {
		msg_printf(VRB,"FAILED dram test with %x errors", error);
		sum_error( "DRAM parity bit");
	}
	else {
		okydoky();
	}
	return error;
}

static int
dram2(struct range *range, int delay_time, int patterns)
{
	/* shift_size	The increment value for each shift from the first addr
	 * max_shift	The maximum number of shifts of size shift_size to do  
	 * pattern_size	The block size of each data pattern (#of addresses)
	 *		The data gets inverted each count of pattern_size  
	 *
	 *  Because the system accesses 4 words in parallel (2 words x 2
	 * interleaves) internal DRAM addresses are incremented every four
	 * word addresses. The larger shift sizes are so the pattern
	 * alternates on the rows.
	 */
	u_int shift_size[3] = { 4, 4, 1024 };
	u_int max_shift[3] = { 4, 16, 8 };
	u_int pattern_size[3]  = { 16, 64, 8192	};
	u_int old_offset;
	unsigned long count;
	u_int old_pat_size;
	u_int array[16];
	u_int data_ptr;
	int error = 0;
	u_int *addrlo;
	u_int *addrhi;
	u_int old_pat;
	u_int offset;
	int blocks;
	int loop;

	addrlo = (u_int *)range->ra_base;
	addrhi = addrlo + range->ra_count - 1;
	count = (range->ra_count-1)*range->ra_size;

	/*  some semi-ordered patterns  */
	array[0] = 0x0; 
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

	if (patterns < 3) {
		blocks = patterns;
		switch (patterns) {
		case 1: 
			msg_printf(VRB,"Doing %d data patterns\n", 5*patterns);
			break;
		case 2: 
			msg_printf(VRB,"Doing %d data patterns\n",
				(5*patterns)+(17*patterns));
			break;
		}
	}
	else {
		blocks = 3;
		msg_printf(VRB,"Doing %d data patterns\n", 31*patterns);
	}

	for (loop = 0; loop < blocks; loop++) {

		for (offset=0; offset <= (shift_size[loop]*max_shift[loop]);
		     offset = offset + shift_size[loop]) {

			/* when offset = 0, set old_offset to top of memory
			 * (count) so loop will be write only
			 */
			old_offset = (!offset) ? (int)count :
						 offset-shift_size[loop];

			for (data_ptr = 0; data_ptr < patterns; data_ptr++) {
				if (data_ptr) { 
					/* look at the previous pattern */
					old_pat = array[data_ptr-1];
				}
				else {
					/* or if the 1st, look at the very
					 * last pattern
					 */
					old_pat = array[patterns-1];
				}

				old_pat_size = pattern_size[loop];

				msg_printf(DBG,"Starting loop: data = %x / %x,"
					" offset = %x, old_offset = %x, "
					" pattern size = %x\n",
					array[data_ptr], ~array[data_ptr],
					offset, old_offset, pattern_size[loop]);

				if (block_dram(array[data_ptr], addrlo, addrhi,
					offset, pattern_size[loop], old_offset,
					old_pat_size, old_pat))
					error++;

				old_offset = offset;
				delay_lp(delay_time);

				msg_printf(DBG," done.\n");

			}       /* data loop             */
		}           /* shift position loop   */
	}                /*  loop thru each block size   */

	return error;
}

static int
block_dram (
register u_int		pattern,
register u_int		*low_addr,
register u_int 		*hi_addr,
register u_int	 	pattern_start,
register u_int		pattern_size,
register u_int		old_start,
register ulong		old_size,
register volatile u_int	old_pattern)
{
	register volatile u_int *addr;
	register volatile u_int	rd_pos = 0;
	register volatile u_int	wr_pos = 0;
	int volatile 		error=0;

	register volatile u_int *top_int;
	register volatile u_int	*top;
	register volatile u_int	*addr_int;
	register volatile u_int *low_int = low_addr;

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
		addr = addr_int;
	} else {       /* DO WRITE ONLY */

		if (old_start > pattern_start) {
			addr_int = low_int + pattern_start;
			top_int =  low_int + old_start;
			top = ((__psunsigned_t)top_int<(__psunsigned_t)hi_addr)
				? (u_int *)top_int : hi_addr;
			addr = addr_int;
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
			addr = addr_int;
			top_int =  low_int + (4*pattern_start);
			top =  (u_int *)top_int;
			/* top = address to begin read/write */
			for ( rd_pos=0; addr < top; addr++, rd_pos++ ) {
				if (rd_pos < old_size) {
					if (*addr != old_pattern) {
						error++;
						memerr((caddr_t)addr,
						    old_pattern,
						    *addr, sizeof(u_int));
					}
				} else {
					rd_pos = 0;
					old_pattern = ~old_pattern;
					if (*addr != pattern) {
						error++;
						memerr((caddr_t)addr,
						    old_pattern,
						    *addr, sizeof(u_int));
					}
				}
			}
		}
	}

	/*  This is the real read-write loop. The read and write loops
	 * separately keep track of the pattern (either complemented or
	 * uncomplemented), and the position within the block size. If
	 * pattern = ~old_pattern, then this will implement a read /
	 * write-inverted data loop
	 */
	for (rd_pos, addr ; addr < hi_addr; addr++)
	{
		if (rd_pos < old_size) {      /* READ OLD PATTERN */
			if (*addr != old_pattern) {
				error++;
				memerr((caddr_t)addr, old_pattern,
				    *addr, sizeof(u_int));
			}
			rd_pos++;
		} else {
			rd_pos = 0;
			old_pattern = ~old_pattern;
			if (*addr != old_pattern) {
				error++;
				memerr((caddr_t)addr, old_pattern,
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
	}
	return (error);
}
