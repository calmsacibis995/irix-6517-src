#ident	"IP22diags/mem/parreg.c:  $Revision: 1.6 $"

/* 
 * parreg.c - manipulation of onboard parity registers
 *		enable, disable, clear
 */

#include "sys/cpu.h"
#include "sys/types.h"
#include "sys/sbd.h"
#include "libsk.h"

/* 
 * parenable - enable cpu parity checking
 */
void
parenable(void)
{
	unsigned int cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
	u_int                   cpuctrl0_cpr;
#ifdef IP22
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	cpuctrl0_cpr = (orion|r4700|r5000) ? 0 : CPUCTRL0_CPR;
#else
	cpuctrl0_cpr = CPUCTRL0_CPR;
#endif

	cpuctrl0 |= CPUCTRL0_MPR | cpuctrl0_cpr;
	cpuctrl0 &= ~CPUCTRL0_R4K_CHK_PAR_N;
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0; 

	wbflush();
}

/* 
 * pardisable - disable cpu parity checking
 */
void
pardisable(void)
{
	unsigned int cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);

	cpuctrl0 &= ~(CPUCTRL0_MPR | CPUCTRL0_CPR);
	cpuctrl0 |= CPUCTRL0_R4K_CHK_PAR_N;
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0; 

	wbflush();
}

/*
 * parclrerr - clear the the cpu parity error register.
 */
void
parclrerr(void)
{
	*(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	wbflush();
}

