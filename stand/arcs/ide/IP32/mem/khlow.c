#ident	"$Id"

#include "mem.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/types.h"

/* architecturally dependent DRAM (0-8MB) address/data/parity test */
int
khlow_drv()
{
	char		*command;
	unsigned int	cpuctrl0;
	int		error = 0;
	int		pflag = 0;
	extern int	*Reportlevel;
	extern unsigned int _hpc3_write1;
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	u_int                   cpuctrl0_cpr;


	msg_printf(VRB, "Low DRAM (0-8MB) Knaizuk Hartmann Test\n");

	flush_cache();
#ifndef IP32
	cpuctrl0_cpr = (orion | r4700 | r5000) ? 0 : CPUCTRL0_CPR;
	cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
		CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &
		~CPUCTRL0_R4K_CHK_PAR_N;
#endif /* !IP32 */
	error = kh_low( *Reportlevel);

	/* Restore control register */
#ifndef IP32
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;

	/* Make sure HPC3_WRITE1 gets updated since we play with
	 * it in ldram*.s.
	 */
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
#endif /* !IP32 */


	if (error)
		sum_error("low DRAM (0-8MB) Knaizuk Hartmann Test");
	else
		okydoky();

	return error;
}
