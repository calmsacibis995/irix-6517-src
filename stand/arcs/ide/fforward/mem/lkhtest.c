#ident	"lkhtest.c:  $Revision: 1.10 $"

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

extern int low_kh_test(int, __psint_t);
extern int _low_kh_test(int, __psint_t);

/* Architecturally dependent DRAM address/data/parity test:
 *	- covers 0-8MB on IP20 and IP22.
 *	- covers 0-16MB on IP26 and IP28.
 */
/*ARGSUSED*/
int
khlow_drv(int argc, char *argv[])
{
#if !_K64PROM32			/* need same width exception handler */
	extern unsigned int _hpc3_write1;
#ifndef IP28
	unsigned int	cpuctrl0;
#endif
	int		error = 0;
	int		pflag = 0;
	k_machreg_t	old_sr;

	msg_printf(VRB, "Low KH test\n");

	flush_cache();

#ifndef IP28
	cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
	    CPUCTRL0_GPR | CPUCTRL0_MPR | CPUCTRL0_CPR) &
	    ~CPUCTRL0_R4K_CHK_PAR_N;
#else
	IP28_MEMTEST_ENTER();
#endif	/* !IP28 */

	old_sr = get_sr();
	error = low_kh_test(pflag,*Reportlevel);
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
		sum_error("low KH test");
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
 *	- IP26 ide uses between 8MB and 11MB only.
 * 	- IP28 ide uses between 8MB and 12MB (mgras diag increased IDE size).
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
low_kh_test(int pflag, __psint_t _Reportlevel)
{
	void *scratch;
	void set_BEV(void *);
	int i,rc = 0;

	/* Make sure GCache is clean.
	 */
	flush_cache();

	/* Test scratch area.
	 */

	/*  I don't know why these functions return 1 with success */

#ifdef	IP26
        msg_printf(VRB, "Testing 12-15MB (scratch area)...\n");
        if (Kh_l(PHYS_TO_K1_RAM(0x00c00000),PHYS_TO_K1_RAM(0x00c00000)+0x300000-1) == 0)
                rc++;
        if (Altaccess_l(PHYS_TO_K1_RAM(0x00c00000),PHYS_TO_K1_RAM(0x00c00000)+0x300000-1) == 0)
                rc++;
#else
	msg_printf(VRB, "Testing 4-8MB (scratch area)...\n");
	if (Kh_l(PHYS_TO_K1_RAM(0x00400000),PHYS_TO_K1_RAM(0x00400000)+0x400000-1) == 0)
		rc++;
	if (Altaccess_l(PHYS_TO_K1_RAM(0x00400000),PHYS_TO_K1_RAM(0x00400000)+0x400000-1) == 0)
		rc++;
#endif

	msg_printf(VRB,"Doing _low_kh_test...\n");

	/*  Do the hard part of testing memory where ide is loaded.  Handles
	 * own bcopy()'s so it can jump back-n-forth.
	 */
	rc += _low_kh_test(pflag,_Reportlevel);

	/* Set-up BEV handler so we don't try to access SPB which will
	 * be trashed by memory tests.
	 */
#ifdef IP26
	set_BEV(SPB->GEVector);
	scratch = (void *)PHYS_TO_K1_RAM(0x00c00000);
#else
	set_SR(get_SR() | SR_BEV);
	scratch = (void *)PHYS_TO_K1_RAM(0x00400000);
#endif

	for (i=0; addrs[i]; i++) {
		msg_printf(VRB, "Testing 0x%x...\n", addrs[i]);
		msg_printf(DBG," bcopy to scratch area %016x  lens[%d] = %016x\n",scratch,i, lens[i]);
		bcopy((void *)addrs[i],scratch,lens[i]);
		if (Kh_l(addrs[i], addrs[i]+lens[i]-1) == 0)
			rc++;
		if (Altaccess_l(addrs[i], addrs[i]+lens[i]-1) == 0)
			rc++;
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
