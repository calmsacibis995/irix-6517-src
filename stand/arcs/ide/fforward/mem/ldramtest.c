#ident	"IP22diags/mem/ldramtest.c:  $Revision: 1.22 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include "mem.h"

#if IP26 || IP28
#include <arcs/spb.h>
#endif

extern int low_dram_test(int, __psint_t);
extern int _low_dram_test(int, __psint_t);

/* Architecturally dependent DRAM address/data/parity test:
 *	- covers 0-8MB on IP20 and IP22.
 *	- covers 0-16MB on IP26 and IP28.
 */
int
low_memtest(int argc, char *argv[])
{
#if !_K64PROM32			/* need same width exception handler */
	extern unsigned int _hpc3_write1;
	char		*command;
	int		error = 0;
	int		pflag = 0;
	k_machreg_t	old_sr;
#ifndef IP28
	unsigned int	cpuctrl0;
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	u_int                   cpuctrl0_cpr;
#endif

	/* parse command */
	command = *argv;
	while ((--argc > 0) && !error) {
		argv++;
		if ((*argv)[0] == '-') {
			switch ((*argv)[1]) {
#if IP20 || IP22
			case 'p':		/* check parity bit */
				if ((*argv)[2] != '\0')
					error++;
				else
					pflag = 1;
				break;
#endif
			default:
				error++;
				break;
			}
		} else
			error++;
	}

	if (error) {
#if IP26 || IP28
		msg_printf(ERR, "usage: %s\n", command );
#else
		msg_printf(ERR, "usage: %s [-p]\n", command);
#endif
		return error;
	}

#if IP20 || IP22
	msg_printf(VRB, pflag ? "Low DRAM (0-8MB) parity bit test\n" :
		"Low DRAM (0-8MB) address/data bit test\n");
#elif IP26 || IP28
	msg_printf(VRB, "Low DRAM (0-16MB) address/data bit test\n");
#else
#error unknown CPU!
#endif

	flush_cache();

#ifndef IP28		/* IP28 is runs naked here... */
	cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
	cpuctrl0_cpr = (orion|r4700|r5000) ? 0 : CPUCTRL0_CPR;
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
		CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &
		~CPUCTRL0_R4K_CHK_PAR_N;
#else
	IP28_MEMTEST_ENTER();
#endif	/* !IP28 */

	old_sr = get_sr();
	error = low_dram_test(pflag, *Reportlevel);
	set_sr(old_sr);

#ifndef IP28
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#else
	IP28_MEMTEST_LEAVE();
#endif	/* !IP28 */

	busy(0);		/* restore the LED */

#if HPC3_WRITE1
	/* Make sure HPC3_WRITE1 gets updated since we play with
	 * it in ldram*.s.
	 */
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
#endif


	if (error)
#if IP20 || IP22
		sum_error(pflag ? "low DRAM (0-8MB) parity bit" :
			"low DRAM (0-8MB) address/data bit");
#else
		sum_error("low DRAM (0-16MB) address/data bit");
#endif
	else
		okydoky();

	return error;
#endif /* !_K64PROM32 */
}

#if IP26 || IP28
/*  TETON has a 16MB standalone memory map.  To avoid needing 32MB to complete
 * ldram test we do this in chunks, and only do the very low-level test on
 * the ide chunk.
 *
 * ASSUMPTIONS:
 *	- IP26 ide uses between 8MB and 11MB.
 *	- IP28 ide uses between 8MB and 12MB (mgras diag increased IDE in size)
 *	- 12MB-15MB is free.
 */
#if IP26
static __psunsigned_t addrs[] = {
	PHYS_TO_K1_RAM(0x00000000), PHYS_TO_K1_RAM(0x00200000),
	PHYS_TO_K1_RAM(0x00400000), PHYS_TO_K1_RAM(0x00600000),
	PHYS_TO_K1_RAM(0x00b00000), PHYS_TO_K1_RAM(0x00f00000),
	0
	};
static int lens[] = {
	0x200000, 0x200000, 0x200000, 0x200000, 0x100000, 0x100000
};
#else /* IP28 */
static __psunsigned_t addrs[] = {
        PHYS_TO_K1_RAM(0x00000000), PHYS_TO_K1_RAM(0x00200000),
        PHYS_TO_K1_RAM(0x00c00000), PHYS_TO_K1_RAM(0x00f00000),
	0
        };
static int lens[] = {
        0x200000, 0x200000, 0x300000, 0x100000
};
#endif

int
low_dram_test(int pflag, __psint_t _Reportlevel)
{
	void *scratch;
	void set_BEV(void *);
	int i, rc;

#ifdef IP26
	/* Make sure GCache is clean.
	 */
	flush_cache();
#endif

	/* Test scratch area.
	 */
#ifdef	IP26
	msg_printf(VRB, "Testing 12-15MB (scratch area)...\n");
	rc = dram(PHYS_TO_K1_RAM(0x00c00000),0x300000,pflag);
#else
        msg_printf(VRB, "Testing 4-8MB (scratch area)...\n");
        rc = dram(PHYS_TO_K1_RAM(0x00400000),0x400000,pflag);
#endif

	/*  Do the hard part of testing memory where ide is loaded.  Handles
	 * own bcopy()'s so it can jump back-n-forth.
	 */
#ifdef IP26
        msg_printf(VRB, "Testing 8-11MB (ide location)...\n");
#else
	msg_printf(VRB, "Testing 8-12MB (ide location)...\n");
#endif
	rc += _low_dram_test(pflag, _Reportlevel);

#ifdef IP26
	/* Set-up BEV handler so we don't try to access SPB which will
	 * be trashed by memory tests.
	 */
	set_BEV(SPB->GEVector);
	scratch = (void *)PHYS_TO_K1_RAM(0x00c00000);
#else
	set_SR(get_SR()|SR_BEV);
	scratch = (void *)PHYS_TO_K1_RAM(0x00400000);
#endif

	for (i=0; addrs[i]; i++) {
		msg_printf(VRB, "Testing 0x%x...\n", addrs[i]);
		bcopy((void *)addrs[i],scratch,lens[i]);
		rc += dram(addrs[i],lens[i],pflag);
		bcopy(scratch,(void *)addrs[i],lens[i]);
	}

#ifdef IP26
	set_BEV(0);
#else
	set_SR(get_SR() & ~SR_BEV);
#endif

	return rc;
}
#endif
