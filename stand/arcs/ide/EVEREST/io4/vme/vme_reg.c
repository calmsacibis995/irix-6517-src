/***********************************************************************\
*	File:		vme_reg.c					*
*									*
*	Contains the routines Used to test the vmecc functionality  	*
*	and path, and possibly do the initialization of the Chip also.  *
*									*
\***********************************************************************/

#ident "arcs/ide/EVEREST/io4/vme/vme_reg.c $Revision: 1.10 $"

#include "vmedefs.h"
#include <prototypes.h>

Vmecc_regs vmecc_regs[] = {
{VMECC_RMWMASK,		0xffffffff},
{VMECC_RMWSET,		0xffffffff},
{VMECC_RMWADDR,		0xffffffff},
{VMECC_RMWAM,		0x003f},
{VMECC_RMWTRIG,		0},
{VMECC_ERRADDRVME,	0},
{VMECC_ERRXTRAVME,	0},
{VMECC_ERRORCAUSES,	0},
{VMECC_ERRCAUSECLR,	0},
{VMECC_DMAVADDR,	0xffffffff},
{VMECC_DMAEADDR,	0xffffffff},
{VMECC_DMABCNT,		0xffffff},
{VMECC_DMAPARMS,	0xfff},
{VMECC_CONFIG,		0xffffffff},
{VMECC_A64SLVMATCH,	0xffffff00},
{VMECC_A64MASTER,	0xffffffff},
{VMECC_VECTORERROR,	0xffff},
{VMECC_VECTORIRQ1,	0xffff},
{VMECC_VECTORIRQ2,	0xffff},
{VMECC_VECTORIRQ3,	0xffff},
{VMECC_VECTORIRQ4,	0xffff},
{VMECC_VECTORIRQ5,	0xffff},
{VMECC_VECTORIRQ6,	0xffff},
{VMECC_VECTORIRQ7,	0xffff},
{VMECC_VECTORDMAENG,	0xffff},
{VMECC_VECTORAUX0,	0xffff},
{VMECC_VECTORAUX1,	0xffff},
{VMECC_IACK1,		0xff},
{VMECC_IACK2,		0xff},
{VMECC_IACK3,		0xff},
{VMECC_IACK4,		0xff},
{VMECC_IACK5,		0xff},
{VMECC_IACK6,		0xff},
{VMECC_IACK7,		0xff},
{VMECC_INT_ENABLE,	0x1fff},
{VMECC_INT_REQUESTSM,	0},
{VMECC_INT_ENABLESET,	0},
{VMECC_INT_ENABLECLR,	0},
{VMECC_PIOTIMER,	0},
{0x1388,		0xffff},
{0x1390,		0xffff},
{0x1398,		0xffff},
{0x13A0,		0xffff},
{0x13A8,		0xffff},
{0x13B0,		0xffff},
{0x13B8,		0xffff},
{0x13C0,		0xffff},
{0x13C8,		0xffff},
{0x13D0,		0xffff},
{0x13D8,		0xffff},
{0x13E0,		0xffff},
{0x13E8,		0xffff},
{0x13F0,		0xffff},
{0x13F8,		0xffff},
{-1,			0}
};

#define	VMECC_REGCNT	(sizeof(vmecc_regs)/sizeof(Vmecc_regs))

/* Buffer used by all the tests. */

jmp_buf	vmebuf;

/* Memory area accessed by the VMECC loopback test */

#if _MIPS_SIM != _ABI64
static caddr_t	Mapram;
#else
static paddr_t	Mapram;
#endif

static int	vmeloc[3];
extern int	io4_tslot, io4_tadap;

#define USAGE   "USAGE: %s [io4slot anum]\n"

vmecc_test(int argc, char **argv)
{
     int 	slot, anum;
     int	jval, fail=0,found=0;

    io4_tslot = io4_tadap = 0;
    if (argc > 1){
	if (io4_select(1, argc, argv)){
	    msg_printf(ERR, USAGE, argv[0]);
	    return(1);
	}
    }
    while (!fail && io4_search(IO4_ADAP_FCHIP, &slot, &anum)){
	if ((argc > 1) && ((slot != io4_tslot) || (anum != io4_tadap)))
	    continue;

	if (fmaster(slot, anum) != IO4_ADAP_VMECC)
	    continue;

	/* found VMECC chip! */
	found = 1;

	vmeloc[0] = slot; vmeloc[1] = anum; vmeloc[2] = -1;

        io_err_clear(slot, (1 << anum));

        if ((jval = setjmp(vmebuf))){
	    switch(jval){
	    default:
	    case 1: err_msg(VME_REGR,vmeloc); break;
	    case 2: err_msg(VME_SWERR,vmeloc); break;
	    }
	    show_fault();
	    io_err_log(slot, (1 << anum));
	    io_err_show(slot, (1 << anum));
        }
        else{
            set_nofault(vmebuf);
            fail |= vmecc_reg(slot, anum);
        }

        clear_nofault();
        io_err_clear(slot, (1 << anum));
    }
    if (!found) {
	if (argc > 1) {
	    msg_printf(ERR, "Invalid Slot/Adapter number\n");
	    msg_printf(ERR, USAGE, argv[0]);
	}
	fail = TEST_SKIPPED;
    }
    if (fail)
	io4_search(0, &slot, &anum);	/* reset variables for future use */

    return (fail);


}

vmecc_reg(int io4_slot, int ioa_num )
{
#if _MIPS_SIM != _ABI64
    int		chip_base,level = 0, i;
    unsigned	oldsr;
    int		window;
    int		retval;
#else
    paddr_t	chip_base;
    int		level = 0, i;
    unsigned	oldsr;
    int		window;
    int		retval;
#endif
    

    msg_printf(INFO, "Checking VMECC on IO4 slot %d adapter %d\n", 
		io4_slot, ioa_num);

    if ((chip_base = get_vmebase(io4_slot, ioa_num)) == 0)
	return(TEST_SKIPPED);

    window = io4_window(io4_slot);

#if _MIPS_SIM != _ABI64
    Mapram = tlbentry(window, 0, 0);
#else    
    Mapram = LWIN_PHYS(window, 0);
#endif

    oldsr = get_SR();
    set_SR(oldsr|SR_KX);


    if (vmecc_exists(chip_base, io4_slot, ioa_num)) {
	retval = TEST_SKIPPED;
    }
    else {
	vmecc_regtest(chip_base);
	vmecc_addr_test(chip_base);
	WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE);
	/* vmecc_errtest(chip_base);  */
	vmecc_intrmask(chip_base);

	msg_printf(VRB,"VMECC tests completed successfully\n");

	retval = TEST_PASSED;
    }

#if _MIPS_SIM != _ABI64
    tlbrmentry(Mapram); Mapram = 0;
#endif
    set_SR(oldsr);
    return(retval);
}



/* Check if the VMECC exists at the given address */
#if _MIPS_SIM != _ABI64
vmecc_exists(unsigned chip_base, int slot, int anum)
#else
vmecc_exists(paddr_t chip_base, int slot, int anum)
#endif
{
    unsigned chip_type;
    /* Check if at the given base Fchip exists, and its 
     * master register shows FCI master as VMECC 
     */
    chip_type = RD_REG(chip_base + FCHIP_MASTER_ID);
    if (chip_type != IO4_ADAP_VMECC){
	msg_printf(ERR, "Adapter in Slot %d adapter %d is NOT VMECC \n",
			slot, anum);
	return (1);
    }
    if (RD_REG(chip_base+VMECC_ERRCAUSECLR) & 0x80){
	msg_printf(ERR,"VMECC in Slot %d Adapter %d is in PIO Drop mode.. Reset system\n", slot, anum);
	return(1);
    }
    return(0);
}


#if _MIPS_SIM != _ABI64
vmecc_regtest(unsigned chip_base)
#else
vmecc_regtest(paddr_t chip_base)
#endif
{

    Vmecc_regs	*vregs = vmecc_regs;
    unsigned	failure=0;
    unsigned	mask, i, val, oldval;

    msg_printf(VRB,"Checking VMECC registers\n");

    /* Check all the registers first. We can analyse later	*/
    for(; vregs->reg_no != (-1); vregs++){

	if (vregs->bitmask0 == 0)
	    continue;

	oldval = RD_REG(chip_base+vregs->reg_no);
        mask = vregs->bitmask0;

	msg_printf(DBG,"Checking register addr 0x%x mask= 0x%x\n", 
		vregs->reg_no, mask);
	for (i=0; mask != 0; i++, (mask >>= 1)){
	    if ((mask & 1) == 0)
		continue;

	    WR_REG((chip_base+vregs->reg_no), (1 << i));

	    val = RD_REG(chip_base+vregs->reg_no);
	    if (val != (1 << i)){
		err_msg(VME_BADREG, vmeloc, vregs->reg_no, (1 << i), val);
		if (continue_on_error() == 0)
		    longjmp(vmebuf, 2);
		failure++;
	    }
	 }
	 WR_REG((chip_base+vregs->reg_no), oldval);
    }
    if(failure)
	longjmp(vmebuf, 2);
         
    msg_printf(INFO, "Passed checking VMECC registers \n");

    return(SUCCESS );
}

#if _MIPS_SIM != _ABI64
vmecc_addr_test(unsigned chip_base)
#else
vmecc_addr_test(paddr_t chip_base)
#endif
{
    int		result=0;
    int		i;
    unsigned 	value, oldval;
    Vmecc_regs	*vregs = vmecc_regs;


    msg_printf(VRB, "Checking Addresibility of VMECC registers\n");

    for (i=0; vregs->reg_no != (-1); vregs++,i++){
    	if (vregs->bitmask0 == 0)
    	    continue;

    	WR_REG(chip_base+vregs->reg_no, vregs->reg_no & vregs->bitmask0);
    }

    /* Read back the values from the registers.	*/

    for (vregs=vmecc_regs, i=0; vregs->reg_no != (-1); vregs++, i++){
        if (vregs->bitmask0 == 0)
    		continue;

        value = RD_REG(chip_base+vregs->reg_no) & vregs->bitmask0;
	if (value != (vregs->reg_no & vregs->bitmask0)){
    	    err_msg(VME_BADRADR, vmeloc, vregs->reg_no, 
		(vregs->reg_no & vregs->bitmask0), value);
	    if (continue_on_error() == 0)
	        longjmp(vmebuf, 2);
         }
    }

    msg_printf(VRB,"Passed checking the VMECC register addressibility\n");

    return (SUCCESS);

}

/* 
 * Function: vmecc_errtest
 * Description :
 *	Write some data to the ERRORCAUSES register, and see if it gets
 *	cleared by reading the ERRCAUSECLR register.
 *
 * This test will fail since ERRCAUSES register is Read only 
 */
#if _MIPS_SIM != _ABI64
vmecc_errtest(unsigned chip_base)
{
    unsigned	clrreg,causereg;
#else
vmecc_errtest(paddr_t chip_base)
{
    paddr_t	clrreg,causereg;
#endif
    /*
     * Write some data to the VMECC errors register, read it via
     * the errclear register, and check if the err register is 
     * cleared.
     */
#define	ERRREG_VAL	0x7f

    msg_printf(VRB,"Checking Error register clearing in VMECC\n");

    WR_REG(chip_base+VMECC_ERRORCAUSES, ERRREG_VAL);


    if ((clrreg = RD_REG(chip_base+VMECC_ERRCAUSECLR)) != ERRREG_VAL){
	err_msg(VME_BADCLR, vmeloc, ERRREG_VAL, clrreg); 
	longjmp(vmebuf, 2);
    }

    if ((causereg = RD_REG(chip_base+VMECC_ERRORCAUSES)) != 0){
	err_msg(VME_BADCAU, vmeloc, 0, causereg);
	longjmp(vmebuf, 2);
    }
    
    msg_printf(VRB,"Passed checking Error register Clearing in VMECC\n");

    return (SUCCESS);
}

/* 
 * Function : vmecc_intrmask
 * Description :
 *	For each of the interrupt enable bits, write to set mask, see
 *	if it gets set in the INTENABLE register, clear the same and 
 *	see if this gets cleared.
 */
#if _MIPS_SIM != _ABI64
vmecc_intrmask(unsigned chip_base)
#else
vmecc_intrmask(paddr_t chip_base)
#endif
{
    int i;
    short	mask, result,value;

    msg_printf(VRB,"Checking Interrupt maskability in VMECC\n");

    /* First clear all interrupt enables			*/
    WR_REG(chip_base+VMECC_INT_ENABLE, 0);

    for (i=0; i < 13; i++){ /* No of Interruptible lines	*/
    	mask = 1 << i;

        WR_REG(chip_base+VMECC_INT_ENABLESET, mask);
        if((value = RD_REG(chip_base+VMECC_INT_ENABLE)) != mask){
	    err_msg(VME_INTSET, vmeloc, i);
	    longjmp(vmebuf, 2);
	}

        WR_REG(chip_base+VMECC_INT_ENABLECLR, mask);
        if ((value = RD_REG(chip_base+VMECC_INT_ENABLE)) != 0){
    	    err_msg(VME_INTCLR, vmeloc, i);
	    longjmp(vmebuf, 2);
	}
    }
    WR_REG(chip_base+VMECC_INT_ENABLE, 0); /* Clear again	*/
    msg_printf(VRB,"Passed checking VMECC Interrupt maskability\n");
    return(SUCCESS); 

}
