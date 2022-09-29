#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <sys/reg.h>
#include <sys/pda.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * temporary handing of exceptions.
 */

extern void disable_ints(int);

void
check_norm_except(__psunsigned_t a, __psunsigned_t b, __psunsigned_t c)
{
	cmn_err(CE_NOTE, "Exception: EPC 0x%llx Cause 0x%x\n",
		b, c);
	if (c & CAUSE_EXCMASK) {
		cmn_err(CE_PANIC, "fatal EXCEPTION cause code 0x%x",
			c & CAUSE_EXCMASK);
	}
	else {
		cmn_err(CE_PANIC, "unable to find cause of exception");

	}
	
}

void
check_interrupt(__psunsigned_t a, __psunsigned_t b, __psunsigned_t c, __psunsigned_t d)
{
	cmn_err(CE_NOTE, "Interrupt Exception: EPC 0x%llx Cause 0x%x RA %llx\n",
		b, c, d);
	if (c & CAUSE_IPMASK) {
		cmn_err(CE_NOTE, "Interrupt exception. Bits %x. Disabling that level..\n",
			(c & CAUSE_IPMASK) >> CAUSE_IPSHIFT);
		disable_ints((c & CAUSE_IPMASK));
	}
	else {
		cmn_err(CE_PANIC, "No interrupt found on interrupt exception");

	}
	
}



