#ident	"IP22diags/mem/dram_diag.c:  $Revision: 1.3 $"

#include <fault.h>
#include "mem.h"
#include "setjmp.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/types.h"

#ifdef	PiE_EMULATOR
#define	BUSY_COUNT	0x8000
#else
#define	BUSY_COUNT	0x200000
#endif	/* PiE_EMULATOR */


/*
 * algorithms: associate with each 4-bits in the DRAM module is a
 *	unique 18-23 bits address. this test writes the address/address
 *	complement of each bit into itself, 4 bits at a time, then
 *	reads back and verifies. 
 *	this test guarantees any 2 bits within a DRAM module
 *	will take on different values some time during the test*
 *
 *	* we don't walk a one across each 4 bits group since it's covered by
 *	  the walking bit test
 */

/* architecturally dependent DRAM address/data/parity test */	
int 
dram_diagnostics(int argc, char *argv[])
{
	register volatile u_int	*addr;
	register u_int		*addrhi;
	u_int			addrhi_offset;
	u_int			*addrlo;
	register int		bit;
	register int		busy_count = 0;
	int			cflag = 0;
	char			*command;
	int			complement;
	u_int			count;
	unsigned int		cpuctrl0;
	int			error = 0;
	jmp_buf			faultbuf;
	u_int			last_phys_addr;
	extern void		map_mem(int);
	extern u_int		memsize;
	u_int			old_SR;
	register u_int		one;
	int			pflag = 0;
	struct range		range;
	register u_int		value;
	register u_int		zero;
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	u_int                   cpuctrl0_cpr;

	/* set up default */
	if (memsize <= MAX_LOW_MEM)
		range.ra_base = FREE_MEM_LO;
	else
		range.ra_base = KDM_TO_PHYS(FREE_MEM_LO);

	count = memsize - (KDM_TO_PHYS(range.ra_base) - PHYS_RAMBASE);
	range.ra_size = sizeof(u_int);
	range.ra_count = count / range.ra_size;
	last_phys_addr = KDM_TO_PHYS(range.ra_base) + count;

	/* parse command */
	command = *argv;
	while (--argc > 0 && !error) {
		argv++;
		if ((*argv)[0] == '-') {
			switch ((*argv)[1]) {
			case 'p':
				if ((*argv)[2] != '\0')
					error++;
				else
					pflag = 1;
				break;
			default:
				error++;
				break;
			}
		} else {
			if (!(parse_range(*argv, sizeof(u_int), &range))) {
				error++;
				break;
			}

			cflag = IS_KSEG0(range.ra_base) ? 1 : 0;
			count = range.ra_size * range.ra_count;
			last_phys_addr = KDM_TO_PHYS(range.ra_base) + count;
#ifdef NOTDEF
			if (last_phys_addr - PHYS_RAMBASE > memsize) {
				msg_printf(ERR,
					"%s: address out of range\n", argv[0]);
				return -1;
			}
#endif  /* NOTDEF */
		}

	}

	if (error) {
		msg_printf(ERR, "Usage: %s [-p] [RANGE]\n", command);
		return error;
	}

	msg_printf(VRB, pflag ? "DRAM parity bit test\n" :
		"DRAM address/data bit test\n");

	if (last_phys_addr > MAX_LOW_MEM_ADDR) {
		range.ra_base = KDM_TO_PHYS(range.ra_base);
		map_mem(cflag);
	}

	old_SR = get_SR();
	set_SR(SR_BERRBIT | SR_IEC);

	addrlo = (u_int *)range.ra_base;
	addrhi = addrlo + range.ra_count;
	addrhi_offset = ((u_int)addrhi - PHYS_RAMBASE) & 0x1fffffff;

	busy(0);
	for (complement = 0; complement <= 1; complement++) {
		if (pflag) {
			one = complement ? 0x00000000 : 0x01010101;
			zero = complement ? 0x01010101 : 0x00000000;
		} else {
			one = complement ? 0x00000000 : 0xffffffff;
			zero = complement ? 0xffffffff : 0x00000000;
	  	}

	     	/*
		 * since we are using 32 bits wide SIMMS and each bank has 4
		 * SIMMS, address lines to the SIMMs start at bit 4 
		 */
	  	for (bit = 0x10; bit < addrhi_offset; bit <<= 1) {
	     		for (addr = addrlo; addr < addrhi; addr++) {
				*addr = (u_int)addr & bit ? one : zero;
				if (!busy_count--) {
					busy_count = BUSY_COUNT;
					busy(1);
				}
			}

			/* read and verify */
			nofault = faultbuf;
			addr = addrlo;

			/*
			 * this setup allows the test to continue after an
			 * exception
			 */
			if (setjmp(faultbuf)) {
				show_fault();
				set_SR(SR_BERRBIT | SR_IEC);
				nofault = faultbuf;
				error++;
				addr++;
			} else {
				for (; addr < addrhi; addr++) {
					value = (u_int)addr & bit ? one : zero;
					if (*addr != value) {
						error++;
						memerr((u_int)addr, value,
							*addr, sizeof(u_int));
					}

					if (!busy_count--) {
						busy_count = BUSY_COUNT;
						busy(1);
					}
				}
			}
		}
	}

	nofault = 0;
	set_SR(old_SR);
	busy(0);
	tlbpurge();

	if (error)
		sum_error(pflag ? "DRAM parity bit" : "DRAM address/data bit");
	else
		okydoky();

	return error;
}
