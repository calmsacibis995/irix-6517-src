/***********************************************************************\
*	File:		fchip_reg.c					*
*									*
*	Contains the routines Used to test the fchip functionality  	*
*	and path, and possibly do the initialization of the Chip also.  *
*									*
\***********************************************************************/

#ident "arcs/ide/EVEREST/io4/vme/fchip_reg.c $Revision: 1.14 $"


#include "vmedefs.h"
#include <prototypes.h>
#include <libsk.h>

extern 	unsigned	random_nos[];

Fchip_regs	fchip_regs[] = {
{FCHIP_VERSION_NUMBER,  0,              0},
{FCHIP_MASTER_ID,       0,              0},
{FCHIP_INTR_MAP,	0xff7f,		0},
{FCHIP_FIFO_DEPTH,      0,              0},
{FCHIP_FCI_ERROR_CMND,	0x7f,		0},
{FCHIP_TLB_BASE,	0x7,		0},
{FCHIP_ORDER_READ_RESP,	0x3,		0},
{FCHIP_DMA_TIMEOUT,	0x3ff,		0},
{FCHIP_INTR_MASK,	0,		0},
{FCHIP_INTR_SET_MASK,	0,		0},
{FCHIP_INTR_RESET_MASK,	0,		0},
{FCHIP_SW_FCI_RESET,	0,		0},
{FCHIP_IBUS_ERROR_CMND,	0xffffffff,	0x0000ffff},
{FCHIP_TLB_FLUSH,	0xfffff,	0x001fffff},
{FCHIP_ERROR,		0xffffff,	0},
{FCHIP_ERROR_CLEAR,	0,		0},
{FCHIP_TLB_IO0,		0x1fffff,	0},
{FCHIP_TLB_IO1,		0x1fffff,	0},
{FCHIP_TLB_IO2,		0x1fffff,	0},
{FCHIP_TLB_IO3,		0x1fffff,	0},
{FCHIP_TLB_IO4,		0x1fffff,	0},
{FCHIP_TLB_IO5,		0x1fffff,	0},
{FCHIP_TLB_IO6,		0x1fffff,	0},
{FCHIP_TLB_IO7,		0x1fffff,	0},
{FCHIP_TLB_EBUS0,	0xfffffff,	0xfffffff},
{FCHIP_TLB_EBUS1,	0xfffffff,	0xfffffff},
{FCHIP_TLB_EBUS2,	0xfffffff,	0xfffffff},
{FCHIP_TLB_EBUS3,	0xfffffff,	0xfffffff},
{FCHIP_TLB_EBUS4,	0xfffffff,	0xfffffff},
{FCHIP_TLB_EBUS5,	0xfffffff,	0xfffffff},
{FCHIP_TLB_EBUS6,	0xfffffff,	0xfffffff},
{FCHIP_TLB_EBUS7,	0xfffffff, 	0xfffffff},
{-1,0,0}

};

#define	FCHIP_REGCNT		(sizeof(fchip_regs)/sizeof(Fchip_regs))
#define	BAD_FCIMASTER		(0x4000000)


/*
 * Function : check_fchip
 * Description : 
 *	This routine would probe different registers of the F-chip and
 *	tries to check if they are ok. It does the following for each
 *	of the F-chip registers.
 *
 *	0. Check if the version no returned by Fchip is ok or not.
 *	1. Check if each bit of the register can be read/written.
 *	2. Check if the Fchip registers are accessible using large window 
 *	   address space.
 *	3. Check if the (Re)setting of the interrupt bit causes the appropriate
 *	   action. (eg. writing to set mask reg should set the mask 
 *	   bit in the intr_mask regr.)
 *	4. Check if the reading of error clear register clears it or not?
 *	5. Do a bunch of operation on the Fchip TLB
 *	Return the result of the test as success/failure.
 *
 */
static jmp_buf	fbuf;
static int	floc[3];
extern int	io4_tslot, io4_tadap;

#define	USAGE	"USAGE: %s [io4slot anum]\n"

fchip_test(int argc, char **argv)
{
    int 	slot, anum, fail=0;
    int		found=0;
    int		jval;

    if (argc > 1){
	if (io4_select(1, argc, argv)){
	    msg_printf(ERR, USAGE, argv[0]);
	    return(1);
	}
    }

    while(!fail && io4_search(IO4_ADAP_FCHIP, &slot, &anum)){
	if ((argc > 1) && ((io4_tslot != slot) || (io4_tadap != anum)))
	    continue;
	else found=1;


        floc[0] = slot; floc[1] = anum ; floc[2] = -1;
        io_err_clear(slot, (1 << anum));

        if ((jval = setjmp(fbuf))){
	    switch(jval){
	    default:
	    case 1:
		err_msg(F_GENRIC,floc, anum, slot);
		break;
	    case 2:
		err_msg(F_SWERR, floc);
		break;
	    case 3:
		break;
	    }
	    show_fault();
	    io_err_log(slot, (1<< anum));
	    io_err_show(slot, (1<< anum));
	    fail=jval;
	}
        else{
            set_nofault(fbuf);
            fail |= fchip_reg(slot, anum);
        }

        clear_nofault();
        io_err_clear(slot, (1<< anum));
    }
    if (!found) {
	if (argc > 1) {
	    msg_printf(ERR, "Invalid Slot/Adapter number\n");
	    msg_printf(ERR, USAGE, argv[0]);
	}
	fail = TEST_SKIPPED;
    }
    if (fail)
	io4_search(0, &slot, &anum);

    return(fail);
}
	    
fchip_reg(int io4_slot, int ioa_num)
{
    unsigned 	level, result;
#if !defined(TFP) && !defined(IP25)
    unsigned 	chip_base;
#else
    __psunsigned_t      chip_base;
#endif
    int		window;

    msg_printf(INFO,"Checking Fchip Registers: IO4: %d Adap: %d\n",
	io4_slot, ioa_num);

    level = adap_type(io4_slot, ioa_num);
    if (level != IO4_ADAP_FCHIP){
	msg_printf(ERR,"Adapter Type 0x%x is not of Fchip\n", level);
	return(1);
    }

    window = io4_window(io4_slot);
    chip_base = SWIN_BASE(window, ioa_num);


    fchip_chkver(chip_base);
    fchip_regs_test(chip_base);
    fchip_lw_regtest(window, ioa_num);
    fchip_addrln_test(chip_base);
    fchip_intr_test(chip_base);
    fchip_errclr_test(chip_base);
    fchip_tlb_flush(chip_base);
    return(SUCCESS);
}

/*
 * This checks the version number of Fchip. Also serves the double purpose of
 * checking the existance of the Fchip. 
 */

#if  !defined(TFP) && !defined(IP25)
fchip_chkver(unsigned chip_base)
#else
fchip_chkver(__psunsigned_t chip_base)
#endif
{
    unsigned short chip_type;
    char verno, *chip_name;

    verno = RD_REG(chip_base + FCHIP_VERSION_NUMBER);
    msg_printf(DBG, "Fchip Revision %d\n", verno);
    if ((verno != FCHIP_VERNO) &&
	(verno != FCHIP2_VERNO) &&
	(verno != FCHIP3_VERNO))
    {
	err_msg(F_BADVER, floc, FCHIP_VERNO, FCHIP2_VERNO, verno);
	longjmp(fbuf, 2);
    }

    /*
     * if rev 1 F chip, check to be sure it is not connected to a
     * graphics adapter
     */
    if (verno == FCHIP_VERNO) {
	chip_type = RD_REG(chip_base + FCHIP_MASTER_ID);

	/*
	 * may change to if != VMECC
	 */
	if ((chip_type == IO4_ADAP_FCG) || (chip_type == IO4_ADAP_DANG)) {
	    switch (chip_type) {
		case IO4_ADAP_FCG:
		    chip_name = "FCG";
		    break;
		case IO4_ADAP_DANG:
		    chip_name = "DANG";
		    break;
		default:
		    chip_name = "UNKNOWN";
		    break;
	    }

	    err_msg(F_BADGVER, floc, verno, chip_type, chip_name);
	    longjmp(fbuf, 2);
	}
    }

    return(SUCCESS);
}

/*
 * Function : finit
 * Description :
 *	Initialize the Fchip for its normal mode of operation.
 *	Not yet defined as to what is expected.
 *	For now, it just clears all error registers, Masks interrupt, and
 *	returns.
 */

finit (unsigned window, unsigned ioa_num)
{
#if  !defined(TFP) && !defined(IP25)
    unsigned chip_base;
#else
    __psunsigned_t chip_base;
#endif
    short	 chip_type;

    chip_base = SWIN_BASE(window, ioa_num);

    WR_REG(chip_base + FCHIP_ERROR, 0);
    WR_REG(chip_base + FCHIP_IBUS_ERROR_CMND,0);
    WR_REG(chip_base + FCHIP_FCI_ERROR_CMND, 0 );
    WR_REG(chip_base + FCHIP_INTR_SET_MASK, 1); /* Disable Interrupts */

    chip_type = RD_REG(chip_base + FCHIP_MASTER_ID);

    if ((chip_type != IO4_ADAP_VMECC) || (chip_type != IO4_ADAP_GIOCC)){
	err_msg(F_INVMID, floc, chip_type, IO4_ADAP_VMECC, IO4_ADAP_GIOCC);
	longjmp(fbuf, 2);
    }

    return(SUCCESS);
}

/* 
 * Function : fchip_regs_test
 * Description :
 * 	Check each of the register bits individually.
 * 	and flag if the data matches or Not. Let's not
 * 	take any decision as to what is correct....
 */
#if  !defined(TFP) && !defined(IP25)
fchip_regs_test(unsigned chip_base)
#else
fchip_regs_test(__psunsigned_t chip_base)
#endif
{

    int		i, failure=0;
    unsigned	mask, oldval, val;
    Fchip_regs	*fregs=fchip_regs;

    msg_printf(VRB, "Checking Fchip Registers for stuck bits\n");

    for(i=0; fregs->reg_no != (-1); fregs++,i++){
	if (fregs->bitmask0 == 0)
	    continue;

	oldval = RD_REG(chip_base+fregs->reg_no);
	mask   = fregs->bitmask0;

	msg_printf(DBG,"fchip_reg checking 0x%x\n", fregs->reg_no);
	for(i=0; mask !=0; i++, mask >>= 1){
	    
	    if (!(mask & 1))
		continue;
	    
	    WR_REG((chip_base+fregs->reg_no), (1 << i));

	    val = RD_REG(chip_base+fregs->reg_no);
	    if ((val & fregs->bitmask0) != (1 << i)){
		err_msg(F_BADREG, floc, fregs->reg_no, (1 << i), val);
		failure++;
	    }
	}

	WR_REG((chip_base+fregs->reg_no), oldval);
    }
    if (failure)
        longjmp(fbuf, 2);

    msg_printf(INFO, "Passed checking Fchip Registers\n");

    return (SUCCESS);
}

/*
 * Description: Check if the Fchip registers are accessible using large window
 *		address space
 */
fchip_lw_regtest(unsigned window, unsigned padap)
{
/* Test the Fchip Register accessibility Using Large Window address */
    unsigned	i, block, failure=0;
    unsigned	val, sr;
    Fchip_regs	*fregs = fchip_regs;

    msg_printf(VRB,"Testing Fchip Registers using Large Window Address \n");

    /*block  = ((window << LWIN_REGIONSHIFT) + (padap << LWIN_PADAPSHIFT));*/
    block  = LWIN_PFN(window, padap) << 4;
    msg_printf(DBG, "fchip_lw: window: %d adap: %d block: 0x%x\n", 
		window, padap, block);

    sr = get_SR();
    set_SR(sr | SR_KX);

    for (i=0; fregs->reg_no != (-1); fregs++){
	if (fregs->bitmask0 == 0)
	    continue;

	u64sw(block, fregs->reg_no+4, ALL_5S & fregs->bitmask0);
	val = u64lw(block, fregs->reg_no+4);

	if ((val&fregs->bitmask0) != (ALL_5S & fregs->bitmask0)){
	    err_msg(F_LWREG, floc,fregs->reg_no,(ALL_5S & fregs->bitmask0),val);
	    failure++;
	}
	u64sw(block, fregs->reg_no+4, val);
    }
    set_SR(sr);
    if (failure)
	longjmp(fbuf, 2);
    msg_printf(VRB,"Passed Testing Fchip Registers using Large Window Address\n");

    return(SUCCESS);
}

/* 
 * Description: Test the Interrupt Masking portion of the chip.
 *		Write to INTR_SET_MASK and readback INTRMASK.
 *		Write to INTRMASKCLR and readback INTRMASK.
 */
#if  !defined(TFP) && !defined(IP25)
fchip_intr_test(unsigned chip_base)
#else
fchip_intr_test(__psunsigned_t chip_base)
#endif
{

    unchar	val;

    msg_printf(VRB, "Testing F Interrupt Masking\n");

    WR_REG (chip_base+FCHIP_INTR_SET_MASK, 1);

    if ((val = RD_REG(chip_base + FCHIP_INTR_MASK)) != 1){
	err_msg(F_INTSET, floc, 1, val);
	longjmp(fbuf, 2);
    }

    WR_REG ( chip_base + FCHIP_INTR_RESET_MASK, 1);

    if ((val = RD_REG (chip_base + FCHIP_INTR_MASK)) != 0){
	err_msg(F_INTRST, floc, 1, val);
	longjmp(fbuf, 2);
    }

    msg_printf(VRB,"Passed testing F interrupt masking\n");

    return (SUCCESS) ; 
}

/*
 * Function : fchip_errclr_test
 * Description :
 *	Write some data to the ERROR register in Fchip. 
 *	Reading ERRORCLR register should return this data as well as
 *	clear the contents of the ERROR register.
 */
#define	ERR_PATTERN		0x5A5A5A
#if  !defined(TFP) && !defined(IP25)
fchip_errclr_test(unsigned chip_base)
#else
fchip_errclr_test(__psunsigned_t chip_base)
#endif
{
    unsigned    value;
    /* 	
     * Write some data to the FCHIP_ERROR, read the FCHIP_ERRORCLR
     * and check if this value is proper. Also check if FCHIP_ERROR
     * is zero or not.
     */
    
    msg_printf(VRB, "Checking F chip Error clearing feature\n");

    WR_REG (chip_base + FCHIP_ERROR , ERR_PATTERN);

    value = RD_REG(chip_base + FCHIP_ERROR_CLEAR);
    if (value != ERR_PATTERN){
	if(value == (BAD_FCIMASTER|ERR_PATTERN))
	    msg_printf(ERR,"Fchip in IO4 slot %d padap %d has NO/Bad FCI master\n",
		floc[0], floc[1]);
	else{
	    err_msg(F_ERRCLR, floc, ERR_PATTERN, value);
	    longjmp(fbuf, 2);
	}
    }
    value = RD_REG(chip_base + FCHIP_ERROR);

    if ((value != 0) && (value != BAD_FCIMASTER)){
	err_msg(F_ERRCLR, floc, 0, value);
	longjmp(fbuf, 2);
    }

    msg_printf(VRB, "Passed checking Fchip Error clearing feature\n");

    return (SUCCESS); 
}

/*
 * Function : fchip_addrln_test
 * Description :
 *  	Address line testing.
 *	Write a set pattern to different registers once and read back
 *	after writing all registers. This would check if there is any 
 *	corruption in the addressing of the registers.
 * 	You should do this only for fully R/W registers.
 */
#if  !defined(TFP) && !defined(IP25)
fchip_addrln_test(unsigned chip_base)
#else
fchip_addrln_test(__psunsigned_t chip_base)
#endif
{

    unsigned	failure=0,regval;
    int		i;
    Fchip_regs	*fregs;


    msg_printf(VRB, "Checking Fchip register Addressability\n");

    for(fregs=fchip_regs,i=0; fregs->reg_no != (-1); fregs++,i++){
        if (fregs->bitmask0 == 0)
    	continue;
    	
        WR_REG(chip_base+fregs->reg_no,
		((unsigned)&fregs->reg_no & fregs->bitmask0));
    }

    /* Now readback the register values		*/
    for(fregs=fchip_regs,i=0; fregs->reg_no != (-1); fregs++,i++){
        if (fregs->bitmask0 == 0)
    	continue;
    	
	/* This could get clobbered by write to flush register */
	if (fregs->reg_no == FCHIP_TLB_BASE)
	    continue;

	regval = RD_REG(chip_base+fregs->reg_no) & fregs->bitmask0;
	if (regval != ((unsigned)&fregs->reg_no & fregs->bitmask0)){
	    err_msg(F_BADRADR, floc, fregs->reg_no, 
		((unsigned)&fregs->reg_no & fregs->bitmask0), regval);
		failure++;
        }
    }
    if (failure)
	    longjmp(fbuf, 2);

    msg_printf(VRB, "Passed checking Fchip register Addressibility\n");

    return(SUCCESS);

}

/*
 * Function : fchip_tlb_flush.
 * Description:
 *	Make some valid entries into the Fchip TLB, and by writing some
 *	pattern to flush mask and base address, check if the proper
 *	entry gets flushed.
 *
 */
#if  !defined(TFP) && !defined(IP25)
fchip_tlb_flush(unsigned chip_base)
#else
fchip_tlb_flush(__psunsigned_t chip_base)
#endif
{
    int		i, flush_err;
#if  !defined(TFP) && !defined(IP25)
    unsigned	reg1, reg2;
#else
    paddr_t	reg1, reg2;
#endif

    /* The Ebus addresses are not set since this is just to check if
     * the flush mask works properly
     */

    msg_printf(VRB, "Checking Fchip TLB flushing capability\n");

    /* Invalidate all entries			*/
    for(flush_err=0; flush_err < 8; flush_err++)
        WR_REG(chip_base+(FCHIP_TLB_IO0 + (flush_err * 8)), 0);

    flush_err = 0;
    WR_REG(chip_base+FCHIP_TLB_IO0, 0x200a00); /* 22nd bit -> valid bit */
    WR_REG(chip_base+FCHIP_TLB_IO1, 0x200b00); 

    /* Writing a 0 should not flush any entry	*/
    WR_REG(chip_base+FCHIP_TLB_FLUSH, 0);

    if ( (( RD_REG(chip_base + FCHIP_TLB_IO0 ) & 0x0200000 ) == 0) ||
         (( RD_REG(chip_base + FCHIP_TLB_IO1 ) & 0x0200000 ) == 0)){
	msg_printf(ERR, "Fchip flushed TLB entries when it should not have\n");
	flush_err++;
    }


    for (i=0; i < 7; i++){
	reg1 = chip_base + FCHIP_TLB_IO0 + (i*8);
	reg2 = chip_base + FCHIP_TLB_IO0 + ((i+1)*8);

	/* Create two valid entries */
        WR_REG(reg1, 0x200a00);
        WR_REG(reg2, 0x200b00); 

        WR_REG(chip_base+FCHIP_TLB_FLUSH, 0xa00);

        /* Now the reg1 Entry should be invalidated and 
         *         reg2 Entry should be in place
         */
        if (RD_REG(reg1) & 0x0200000 ){
	    msg_printf(ERR,"Fchip Failed to flush TLB entry %d\n",i);
	    flush_err++;
	}
    
        if ((RD_REG(reg2) & 0x0200000 ) == 0){
	    msg_printf(ERR, "Fchip Flushed TLB entry <%d> instead of <%d> \n",
			i+1, i);
	    flush_err++;
	}

    }


    for (i=0; i < 8; i++)
	WR_REG(chip_base+FCHIP_TLB_IO0+(i*8), 0x200a00);

    /* This command should flush all entries.	*/
    WR_REG(chip_base+FCHIP_TLB_FLUSH, (long long)0x00000A0000000F00L);

    for (i=0; i < 8; i++){
        if(RD_REG(chip_base+FCHIP_TLB_IO0+(i*8)) & 0x0200000 ) {
	    msg_printf(ERR,"Fchip failed to clear TLB entry %d \n", i);
	    flush_err++;
	}
    }

    if (flush_err){
	msg_printf(ERR,"FAILED Fchip TLB flush test \n");
	return(FAILURE);
    }

    msg_printf(VRB, "Passed Fchip TLB flush test\n");

    Return (flush_err);
}
#if 0
static
u64lw(unsigned block, unsigned offs)
{
    unsigned  	oldsr, value;
    evreg_t	lladdr;

    oldsr = get_SR();
    set_SR(SR_KX);
    lladdr = (LWINDOW_BASE) + ((evreg_t)block << 32) + offs;
    value = EV_GET_REG(lladdr);
    set_SR(oldsr);
    return(value);

}
static
u64sw(unsigned block, unsigned offs, int val)
{
    unsigned  	oldsr;
    evreg_t	lladdr;

    oldsr = get_SR();
    set_SR(SR_KX);
    lladdr = (LWINDOW_BASE) + ((evreg_t)block << 32) + offs;
    EV_SET_REG(lladdr, val);
    set_SR(oldsr);

    return(0);
}
#endif
