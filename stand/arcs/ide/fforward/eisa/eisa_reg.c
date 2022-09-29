#ident	"IP22diags/eisa/eisa_reg.c:  $Revision: 1.5 $"

/*
 * eisa_reg.c - write/readback of EIU/INTEL registers
 */

#include "uif.h"
#include "eisa.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/param.h"
#include "sys/sbd.h"
#include "libsk.h"

/*
 * test the EIU/INTEL chip registers 
 */
int
eisa_register (void)
{
    int	error_count = 0, ttl_error_count = 0, rval, wval;

        /* EIU registers */
    volatile unsigned int *eiu_stat = (unsigned int *)
        PHYS_TO_K1(EIU_BASE+EIU_STAT_OFFSET);
    volatile unsigned int *eiu_mode = (unsigned int *)
        PHYS_TO_K1(EIU_BASE+EIU_MODE_OFFSET);
    volatile unsigned int *eiu_preemption = (unsigned int *)
        PHYS_TO_K1(EIU_BASE+EIU_PREEMP_OFFSET);
    volatile unsigned int *eiu_qtime = (unsigned int *)
        PHYS_TO_K1(EIU_BASE+EIU_QTIME_OFFSET);

        /* Intel registers */
    volatile unsigned char *intel_reg1 = (unsigned char *)
        PHYS_TO_K1(INTEL_BASE+INTEL_REG1_OFFSET);
    volatile unsigned char *intel_reg2 = (unsigned char *)
        PHYS_TO_K1(INTEL_BASE+INTEL_REG2_OFFSET);

    if (!is_fullhouse ()) {
	msg_printf (VRB, "No EISA bus in this machine.\n");
	return (0);
    }

    msg_printf (VRB, "EIU/Intel registers test\n");


    if (!(wbadaddr(eiu_mode, 4))) {
	msg_printf (VRB, "Testing EIU registers..");
  	/* probe and reset mode register. Causes stat to be zero */
	wval = (EIU_MODE_B26 | EIU_MODE_B30);
	*eiu_mode = wval;
	/* Read back */
	rval = *eiu_mode;
	if (rval != wval) error_count++;

	/* probe stat register */
#ifdef	IP28
	/* it's possible that speculatively executed instructions
	 * cound make the EIU raise it's more-than-one-cycle-read
	 * error condition.  so if we're on an IP28 that allows
	 * speculative execution to the io spaces, clear the register
	 * before we read it
	 */
	if (badaddr((void *)PHYS_TO_K0(CPUCTRL0),4) == 0) {
		*eiu_stat = 0;
	}
#endif	/* IP28 */

	wval = 0;
	rval = *eiu_stat;
	if (rval != wval) error_count++;
	
	/* probe preemp register */
	wval = EIU_TESTVAL1;
	*eiu_preemption = (u_short) wval;
	rval = *eiu_preemption;
        if (rval != wval) error_count++;

	/* probe qtime register */
	wval = EIU_TESTVAL1;
	*eiu_qtime = (u_short) wval;
	rval = *eiu_qtime;
        if (rval != wval) error_count++;

	if (error_count) {
		msg_printf (VRB, "..Failed\n");
		ttl_error_count = error_count;
		error_count = 0;
	} else
		msg_printf (VRB, "..Passed\n");

	msg_printf (VRB, "Testing INTEL registers..");
	/* probe both intel reg registers */
	wval = INTEL_TESTREG;
	*intel_reg1 = (char)wval;
	rval = *intel_reg1;
        if (rval != wval) error_count++;
	*intel_reg2 = (char)wval;
	rval = *intel_reg2;
        if (rval != wval) error_count++;
	if (error_count) 
		msg_printf (VRB, "..Failed\n");
	else
		msg_printf (VRB, "..Passed\n");
 
	ttl_error_count += error_count;
    } else 
   	ttl_error_count++; 

        if (ttl_error_count)
		sum_error ("EIU/Intel registers");
    	else
		okydoky ();

    return (ttl_error_count);
}   /* eisa_register */
