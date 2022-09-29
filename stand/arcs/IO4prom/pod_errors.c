/***********************************************************************\
*	File:		pod_errors.c					*
*									*
*       Contains code for analyzing the error registers in the IA/ID, F *
*       and VMECC path, and see if any sense can be made out of it.     *
*									*
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_errors.c $Revision: 1.16 $"

#include <sys/types.h>
#include <setjmp.h>
#include "libsc.h"
#include "libsk.h"
#include "pod_fvdefs.h"

/*
 * Assumes that the following variables are set.
 * IO4_slotno
 * Ioa_num 	
 * Window
 */
/*
 * Function : analyze error
 * parameters : level 
 *	0 => No Analysis. Just do a jumpbuf.
 *	1 => Dump just the IO4 IA/ID error register
 *	2 => Dump the IO4 and the F[ioadap_num] error registers.
 * 	3 => Dump IO4, F[ioadap_num] and the attached vmecc err regs.
 * 
 * Description :
 *	Depending on the level, dump the requisite error registers.
 *	Make an analysis of the errors ??
 */
void setup_globals(int win, int slot, int ioa_num)
{
    Window = win;
    IO4_slot = slot;
    Ioa_num = ioa_num;
}

#ifdef	NEWTSIM
void analyze_error() {}
void clear_ereg(int level)		{}

#else

void analyze_error(int level)
{
    __psunsigned_t chip_base;
    evreg_t error;

    if (Window == -1 || IO4_slot == -1 || Ioa_num == -1)
	goto  end;
    
    chip_base = SWIN_BASE(Window, Ioa_num);
    switch(level){
    case 3: if(error = RD_REG(chip_base+VMECC_ERRCAUSECLR))
		mk_msg("VMECC in IO4 Slot %d Adap %d Error Reg: 0x%x\n",
			IO4_slot, Ioa_num, error);
    case 2: if (error = RD_REG(chip_base+FCHIP_ERROR_CLEAR))
		mk_msg("Fchip in IO4 Slot %d Adap %d Error Reg: 0x%x\n",
			IO4_slot, Ioa_num, error);
    case 1: if (error = IO4_GETCONF_REG_NOWAR(IO4_slot, IO4_CONF_IBUSERRORCLR))
		mk_msg("IO4 in Slot %d, IBus error register:    0x%x\n",
			IO4_slot, Ioa_num, error);
	    if (error = IO4_GETCONF_REG_NOWAR(IO4_slot, IO4_CONF_EBUSERRORCLR))
		mk_msg("IO4 in Slot %d Ebus error Register:     0x%x\n",
			IO4_slot, Ioa_num, error);
	    if (error = EV_GET_LOCAL(EV_ERTOIP))
		mk_msg("CC ERTOIP                         :     0x%x\n", error);
    default: break;
    }
end:
#ifdef	DEBUG_PROM
    return;	/* Continue even with errors. */
#else
    if (nofault)
	longjmp(nofault, 2);
    else{
	message("PANIC: nofault not set, returning \n");
	return;
    }
#endif
}

/*ARGSUSED*/
void clear_ereg(int level)
{
}
#endif
