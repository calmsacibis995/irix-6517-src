/***********************************************************************\
*	File:		pod_iaid.c   					*
*									*
*	Contains the routines used to initialize the IO4 boards and 	*
*	associated IO Adapters (such as the EPC, SCSI, and F chip	*
*	adaptors).  							*
*									*
\***********************************************************************/

#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/IP25.h>

#include "pod_iadefs.h"
#include "prom_intr.h"
#include "prom_externs.h"

#include "ip25prom.h"

IO4cf_regs	io4_regs[]={
{IO4_CONF_INTRVECTOR,    0x7fff},
{IO4_CONF_GFXCOMMAND,    0xff},
{IO4_CONF_ETIMEOUT,      0xfffff},
{IO4_CONF_RTIMEOUT,      0xfffff},
{IO4_CONF_INTRMASK,      0x3},
{ (-1), 0}
};

extern char *ertoip_names[16];
void
dump_errors()
{
#if DEBUG
        int 	i;
        evreg_t ertoip=0;

        ertoip = LD(EV_ERTOIP);
        if (ertoip)
	{
		ccloprintf("\n");
                ccloprintf("*** Error/TimeOut Interrupt(s) Pending: %x ==\n", ertoip);
        	for (i = 0; i < 16; i++)
                	if (ertoip & (1 << i))
                        	ccloprintf("\t %s\n", ertoip_names[i]);
            	ccloprintf("    EPC %x CAUSE %x BADVADDR %x\n", _epc(0, 0), _cause(0, 0), _badvaddr(0, 0));

	}
#endif
}

/*
 * Function : check_io4
 * Description :
 *	Look at the various configuration registers of the IO4, and 
 *	see if they are in good shape.
 * Parameters:
 *	slot   --  Slot containing the IO4 board.
 *	window --  The small and large window number Where it is located.
 */

uint check_io4(unsigned window, unsigned slot)
{
    int result = 0;
    jmp_buf iaid_buf;
    uint *prev_fault;

    ccloprintf("Testing master IA chip registers (slot %b)...\n", slot);

    min_io4config(slot, window);

    if (setfault(iaid_buf, &prev_fault)) {
	/* Returned here due to an error */
        restorefault(prev_fault);
	ccloprintf("*** Exception while testing IA/ID regs on IO4 in slot %d\n",
								slot);
	return EVDIAG_IAREG_BUSERR;
    } else {
        result = io4_check_regs(slot);
    }

    restorefault(prev_fault);

    if (result)
	return result;
    if (setfault(iaid_buf, &prev_fault)) {
        restorefault(prev_fault);
	/* Returned here due to an error */
	ccloprintf("*** Exception while testing PIO timeout on IO4 in slot %d\n",
									slot);
	result = EVDIAG_IAPIO_BUSERR;
    } else {
	result = io4_check_errpio(slot, window); 
    }

    restorefault(prev_fault);
    return result;
}


uint io4_check_regs(int slot)
{
    IO4cf_regs	*ioregs;
    int		i,j;
    evreg_t	value, readv, old_value;

    for (ioregs = io4_regs,i=0; ioregs->reg_no != (-1); ioregs++,i++){
        if (ioregs->bitmask0 == 0)
                continue;
	old_value = EV_GET_CONFIG(slot, ioregs->reg_no);
	for (j=0, value=1; j < sizeof(unsigned); j++, value <<=1){
	    if (!(ioregs->bitmask0 & value))
		continue;
	    EV_SET_CONFIG(slot, ioregs->reg_no, value);
	    readv = EV_GET_CONFIG(slot, ioregs->reg_no);

	    if (readv != value){
		ccloprintf("*** ERROR in IA config reg %d: Expected=0x%x Got: 0x%x\n", 
					ioregs->reg_no, value, readv);
		return EVDIAG_IAREG_FAILED;
	    }
        }
	EV_SET_CONFIG(slot, ioregs->reg_no, old_value);
    }

    return 0;
}


/*
 * Function : io4_check_timeout
 * Description :
 *	Do some invalid PIO operation, and check the error bits which
 *	get set. Do the invalid PIO again, and see if the sticky bit 
 *	gets set.
 */
uint io4_check_errpio(int slot, int window)
{
    evreg_t	ebus_error, olvl;
    int		result;

#define	exp_error 	(IO4_EBUSERROR_BADIOA | IO4_EBUSERROR_STICKY)

    result = 0;
    min_io4config(slot, window);

    EV_GET_CONFIG(slot, IO4_CONF_EBUSERRORCLR);

    olvl = EV_GET_CONFIG(slot, IO4_CONF_INTRVECTOR);

    EV_SET_CONFIG(slot, IO4_CONF_INTRVECTOR, IO4ERR_VECTOR);
    SD_LO(SWIN_BASE(window, 0), 0);	/* NON_EXISTENT_IOA */

    /* This should set the sticky bit 	*/
    SD_LO(SWIN_BASE(window, 0), 0);	

    ebus_error = EV_GET_CONFIG(slot, IO4_CONF_EBUSERRORCLR);

    if (ebus_error != exp_error){
	ccloprintf("*** ERROR: Wrong Error Reg bits set: Exp 0x%x, Act 0x%x\n",
							exp_error, ebus_error);
	result = EVDIAG_IAPIO_BADERR;
    }

    if (!(LD(EV_IP0) & (1 << IO4ERR_LEVEL))) {
	ccloprintf("*** ERROR: IA did not send an interrupt on level 0x%b\n",
							IO4ERR_LEVEL);
	result =  EVDIAG_IAPIO_NOINT;
    }

    /* clean up after ourselves. */
    SD(EV_CIPL0, 0);

    /* Restore interrupt vector */
    EV_SET_CONFIG(slot, IO4_CONF_INTRVECTOR, olvl);

    /* Clean out interrupt */
    SD(EV_CIPL0, IO4ERR_LEVEL);

    /* Clear IO4 error state */
    EV_GET_CONFIG(slot, IO4_CONF_IBUSERRORCLR);
    EV_GET_CONFIG(slot, IO4_CONF_EBUSERRORCLR);

    SD_LO(EV_ILE, EV_ERTOINT_MASK | EV_CMPINT_MASK);
    SD_LO(EV_CIPL124, 0);
    SD_LO(EV_ERTOIP, 0);

    return result;

#undef 	exp_error
}


/*
 * Function : min_io4config
 * Description : Do minimal IO4 configuration
 */
void min_io4config(int slot, int window)
{

    /* First do minimal IO4 configuration	*/
    EV_SET_CONFIG(slot, IO4_CONF_LW, window);
    EV_SET_CONFIG(slot, IO4_CONF_SW, window);

    EV_SET_CONFIG(slot, IO4_CONF_ENDIAN, !getendian());
/* Polarity on getendian? */

    EV_GET_CONFIG(slot, IO4_CONF_IBUSERRORCLR);
    EV_GET_CONFIG(slot, IO4_CONF_EBUSERRORCLR);
    EV_SET_CONFIG(slot, IO4_CONF_INTRMASK, 0x2);
}

