#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include "mem.h"

#ident	"$Revision: 1.6 $"

/* architecturally dependent DRAM (0-8MB) address/data/parity test */
int
khlow_drv(void)
{
	extern int kh_low(long);
	char		*command;
	int		error = 0;
	int		pflag = 0;
	extern int	*Reportlevel;
	extern unsigned int _hpc3_write1;
#ifndef IP28
	unsigned int	cpuctrl0;
	u_int           cpuctrl0_cpr;
#ifdef IP22
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
#endif
#endif


	msg_printf(VRB, "Low DRAM (0-8MB) Knaizuk Hartmann Test\n");

	flush_cache();
#ifndef IP28
#ifdef IP22
	cpuctrl0_cpr = (orion|r4700|r5000) ? 0 : CPUCTRL0_CPR;
#else
	cpuctrl0_cpr = CPUCTRL0_CPR;
#endif
	cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
		CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &
		~CPUCTRL0_R4K_CHK_PAR_N;
#endif

	error = kh_low(*Reportlevel);

#ifndef IP28
	/* Restore control register */
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#endif

	/* Make sure HPC3_WRITE1 gets updated since we play with
	 * it in ldram*.s.
	 */
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;


	if (error)
		sum_error("low DRAM (0-8MB) Knaizuk Hartmann Test");
	else
		okydoky();

	return error;
}
