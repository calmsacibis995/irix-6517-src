#ident	"IP22diags/mem/ldramtest.c:  $Revision: 1.1 $"

#include "mem.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/types.h"

/* architecturally dependent DRAM (0-8MB) address/data/parity test */
int
low_memtest(int argc, char *argv[])
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

	/* parse command */
	command = *argv;
	while ((--argc > 0) && !error) {
		argv++;
		if ((*argv)[0] == '-') {
			switch ((*argv)[1]) {
			case 'p':		/* check parity bit */
				if ((*argv)[2] != '\0')
					error++;
				else
					pflag = 1;
				break;
			default:
				error++;
				break;
			}
		} else
			error++;
	}

	if (error) {
		msg_printf(ERR, "usage: %s [-p]\n", command);
		return error;
	}

	msg_printf(VRB, pflag ? "Low DRAM (0-8MB) parity bit test\n" :
		"Low DRAM (0-8MB) address/data bit test\n");

	flush_cache();
#ifndef IP32
	cpuctrl0_cpr = (orion | r4700 | r5000) ? 0 : CPUCTRL0_CPR;
	cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
		CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &
		~CPUCTRL0_R4K_CHK_PAR_N;
	error = low_dram_test(pflag, *Reportlevel);
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#endif /* !IP32 */
	busy(0);		/* restore the LED */
	/* Make sure HPC3_WRITE1 gets updated since we play with
	 * it in ldram*.s.
	 */
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;


	if (error)
		sum_error(pflag ? "low DRAM (0-8MB) parity bit" :
			"low DRAM (0-8MB) address/data bit");
	else
		okydoky();

	return error;
}
