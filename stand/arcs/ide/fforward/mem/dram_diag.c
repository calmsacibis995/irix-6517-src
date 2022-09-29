#ident	"IP22diags/mem/dram_diag.c:  $Revision: 1.27 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include "setjmp.h"
#include "libsc.h"
#include "libsk.h"
#include "mem.h"

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
	char			*command = *argv;
	int			error = 0;
	int			pflag = 0;
	struct range		range, range2;
	char			*rstr = argv[1];

#if !IP26 && !IP28
	if (argc >= 2) {
		if (strcmp(argv[1],"-p") == 0) {
			pflag = 1;
			rstr = argv[2];
			argc--;
		}
	}
#else
	flush_cache();
#endif

	if (mem_setup(argc,command,rstr,&range,&range2) != 0) {
		error = 1;
	}

	if (error) {
#if !IP26 && !IP28
		msg_printf(ERR, "Usage: %s [-p] [RANGE]\n", command);
#else
		msg_printf(ERR, "Usage: %s [RANGE]\n", command);
#endif
		return error;
	}

	msg_printf(VRB, pflag ? "DRAM parity bit test\n" :
				"DRAM address/data bit test\n");

	error = dram(range.ra_base, range.ra_count, pflag);

#ifndef _USE_MAPMEM
	if (range2.ra_base)
		error += dram(range.ra_base, range.ra_count, pflag);
#endif

	if (error)
		sum_error(pflag ? "DRAM parity bit"  : "DRAM address/data bit");
	else
		okydoky();

	return error;
}

int
dram(__psunsigned_t _addrlo, __psunsigned_t count, int pflag)
{
	unsigned int addrhi_offset, one, zero, value, tmp;
	unsigned int *addrhi = (unsigned int *)(_addrlo+count);
	unsigned int *addrlo = (unsigned int *)_addrlo;
	int complement, bit, error=0, busy_count=0;
	volatile unsigned int *addr;
	k_machreg_t old_SR;
	jmp_buf faultbuf;
#ifndef IP28
	u_int cpuctrl0;
	u_int cpuctrl0_cpr;
#ifdef IP22
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;

	cpuctrl0_cpr = (orion|r4700|r5000) ? 0 : CPUCTRL0_CPR;
#else
	cpuctrl0_cpr = CPUCTRL0_CPR;
#endif
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

	old_SR = get_SR();

#if R4000 || R10000
	set_SR(SR_PROMBASE|SR_IEC|SR_BERRBIT);
#elif TFP
	set_SR(SR_PROMBASE);
#endif

	addrhi_offset = KDM_TO_MCPHYS((__psunsigned_t)addrhi - PHYS_RAMBASE);

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
				*addr = (((__psint_t)addr & bit) ? one : zero);
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
#if R4000 || R10000
				set_SR(SR_PROMBASE|SR_IEC|SR_BERRBIT);
#elif TFP
				set_SR(SR_PROMBASE);
#endif
				nofault = faultbuf;
				error++;
				addr++;
			}
			else {
				for (; addr < addrhi; addr++) {
					value = (((__psint_t)addr & bit) ? one : zero);
					if ((tmp=*addr) != value) {
						error++;
						memerr((caddr_t)addr, value,
							tmp, sizeof(u_int));
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
#ifndef IP28
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#else
	IP28_MEMTEST_LEAVE();
#endif	/* !IP28 */

	busy(0);

	tlbpurge();

	return error;
}
