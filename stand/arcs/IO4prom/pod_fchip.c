/***********************************************************************\
*	File:		pod_fchip.c					*
*									*
*	Contains the routines Used to test the fchip functionality  	*
*	and path, and possibly do the initialization of the Chip also.  *
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_fchip.c $Revision: 1.25 $"


#include "libsc.h"
#include "libsk.h"
#include "pod_fvdefs.h"

extern 	unsigned	random_nos[];

Fchip_regs	fchip_regs[] = {
{FCHIP_VERSION_NUMBER,	0,		0},
{FCHIP_MASTER_ID,	0,		0},
{FCHIP_INTR_MAP,	0xff7f,		0},
{FCHIP_FIFO_DEPTH,	0,		0},
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
{FCHIP_ERROR,		0xfffff,	0},
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

static void fchip_pon_values(__psunsigned_t);
static void fchip_lw_regtest(unsigned, int);

static jmp_buf	fbuf;

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
int
check_fchip(int window, int io4_slot, int ioa_num)
{
    volatile int result=0;
    __psunsigned_t chip_base;
    unsigned level;

    chip_base = SWIN_BASE(window, ioa_num);

    setup_globals(window, io4_slot, ioa_num);

    message(FCHIP_TEST);
    Nlevel++;		/* Increment Message nesting level */

    if ((level = setjmp(fbuf)) == 0){
	set_nofault(fbuf);
        fchip_chkver(chip_base, io4_slot, ioa_num); result++;
	fchip_pon_values(chip_base); result++;
        fchip_regs_test(chip_base); result++;
	fchip_lw_regtest(window, ioa_num); result++;
        fchip_addrln_test(chip_base); result++;
        fchip_intr_test(chip_base); result++;
        fchip_errclr_test(chip_base); result++;
        fchip_tlb_flush(chip_base);   result++;
	fchip_errbit_test(chip_base);
	result = SUCCESS;
    }
    else{
	/* Return from an exception routine	*/
	if(level == 1){
	    message("Exception while testing Fchip \n");
	    show_fault();
	}
	switch(result){
	    case 0 : message(FCHIP_NEXIST);
		     message("Probably Fchip doesnot exists/broken\n");
		     break;
	    case 1 : message(FCHK_PONF);
		     break;
	    case 2 : message(FCHK_REGF); break;
	    case 3 : message(FCHK_LWINF); break;
	    case 4 : message(FCHK_ADDRF); break;
	    case 5 : message(FCHK_IMASKF); break;
	    case 6 : message(FCHK_EREGF); break;
	    case 7 : message(FCHK_TLBF);  break;
	    case 8 : message(FCHK_RSTFCIF);  break;
	    default: message(FREG_UNKN_TEST); break;
	}
	result = FAILURE;
    }

    Nlevel--;

#ifndef	NEWTSIM
    (result == SUCCESS) ? message(FCHIP_TESTP) : message(FCHIP_TESTF);
#endif

    clear_nofault();
    Return(result);
}

/*
 * Check the values of some of the registers on reset
 */
static void 
fchip_pon_values(__psunsigned_t chip_base)
{
    evreg_t	val;

    if ((val = RD_REG(chip_base+FCHIP_INTR_MASK)) != 1)
	message("Reset value for Interrupt mask register should be 1, but is 0x%x\n",val);
    
    if ((val = RD_REG(chip_base+FCHIP_TLB_BASE)) != 0)
	message("Reset value for Fchip TLB base register should be 0, but is 0x%x\n", val);

    if ((val = RD_REG(chip_base+FCHIP_DMA_TIMEOUT)) != 0x3FF)
	message("Reset value for Fchip DMA timeout should be 0x3FF, but is 0x%X\n", val);

}


int
fchip_chkver(__psunsigned_t chip_base, int ioslot, int ioa_num)
{
    evreg_t	temp;

    temp = IO4_GETCONF_REG(ioslot, 
		(ioa_num & 4)? IO4_CONF_IODEV1: IO4_CONF_IODEV0);
    temp =  (temp >> ((ioa_num & 3) * 8)) & 0xff;
    if (temp != IO4_ADAP_FCHIP){
	mk_msg("Adapter ID 0x%x at <slot:%d adap:%d> is not of Fchip\n", 
		temp, ioslot, ioa_num);
	longjmp(nofault, 2);
    }

    if ((temp = RD_REG(chip_base + FCHIP_VERSION_NUMBER)) != FCHIP_VERNO){
        mk_msg(FERR_INVAL_VER, FCHIP_VERNO, temp);
	analyze_error(3);
    }

#ifdef	DEBUG_POD
    print_hex(0xfe << 8 | temp);
#endif
    return(SUCCESS);
}

/*
 * Function : fchip_init
 * Description :
 *	Initialize the Fchip for its normal mode of operation.
 *	Not yet defined as to what is expected.
 *	For now, it just clears all error registers, Masks interrupt, and
 *	returns.
 */

int
promfchip_init (unsigned window, unsigned ioa_num)
{
    __psunsigned_t chip_base;
    short	 chip_type;

    chip_base = SWIN_BASE(window, ioa_num);

    RD_REG(chip_base + FCHIP_ERROR_CLEAR);
    WR_REG(chip_base + FCHIP_IBUS_ERROR_CMND,0);
    WR_REG(chip_base + FCHIP_FCI_ERROR_CMND, 0 );
    WR_REG(chip_base + FCHIP_INTR_SET_MASK, 1); /* Disable Interrupts */

    chip_type = (short)RD_REG(chip_base + FCHIP_MASTER_ID);

    if ((chip_type != IO4_ADAP_VMECC) && (chip_type != IO4_ADAP_FCG)){
        mk_msg("Fchip has Invalid Master ID: 0x%x\n", chip_type);
        return(FAILURE);
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
int
fchip_regs_test(__psunsigned_t chip_base)
{

    unsigned	result=0;
    Fchip_regs	*fregs;

    message(FCHK_REG);

    for(fregs=fchip_regs; fregs->reg_no != (-1); fregs++){
    	if (check_regbits(chip_base + fregs->reg_no, fregs->bitmask0, 0)) 
	    result=1;
         

        if (fregs->bitmask1) 
            if (check_regbits(chip_base + fregs->reg_no,fregs->bitmask1,1)) 
		result=1;
    }

    if (result)
	analyze_error(2);

    message(FCHK_REGP);

    Return (result);
}

/*
 * Description: Check if the Fchip registers are accessible using large window
 *		address space
 */
static void
fchip_lw_regtest(unsigned window, int anum)
{
/* Test the Fchip Register accessibility Using Large Window address */
    unsigned	oldv, reg_no, nval;
    unsigned	mask, error=0;
    Fchip_regs	*fregs;
    k_machreg_t	sr;

    window &= 7;

    message(FCHK_LWIN);
    sr = get_SR();
    set_SR(sr | SR_KX);

    for (fregs=fchip_regs; fregs->reg_no != (-1); fregs++){
	if (fregs->bitmask0 == 0)
	    continue;

	reg_no= fregs->reg_no+4; /* To support Big endian access */
	mask  = ALL_5S & fregs->bitmask0;
	oldv  = get_word(window, anum, reg_no);
	put_word(window, anum, reg_no, mask);
	nval = get_word(window, anum, reg_no) & fregs->bitmask0; 
	if (nval != mask){
	    mk_msg(FERR_LWIN_VAL, fregs->reg_no, mask, nval);
	    error=1;
	}
	put_word(window, anum, reg_no, oldv);
    }
    set_SR(sr);
    if (error)
	analyze_error(2);
    
    message(FCHK_LWINP);
}

/* 
 * Description: Test the Interrupt Masking portion of the chip.
 *		Write to INTR_SET_MASK and readback INTRMASK.
 *		Write to INTRMASKCLR and readback INTRMASK.
 */
int
fchip_intr_test(__psunsigned_t chip_base)
{

    int	result=0;

    message(FCHK_IMASK);

    WR_REG (chip_base+FCHIP_INTR_SET_MASK, 1);

    if (RD_REG(chip_base + FCHIP_INTR_MASK) != 1){
        result |= 1;
	mk_msg(FERR_IMASK_SET);
	analyze_error(2);
    }

    WR_REG ( chip_base + FCHIP_INTR_RESET_MASK, 1);

    if (RD_REG (chip_base + FCHIP_INTR_MASK) != 0){
        result |= 2;
	mk_msg(FERR_IMASK_CLR);
	analyze_error(2);
    }

    message(FCHK_IMASKP);

    Return ( result) ; 
}

/*
 * Function : fchip_errclr_test
 * Description :
 *	Write some data to the ERROR register in Fchip. 
 *	Reading ERRORCLR register should return this data as well as
 *	clear the contents of the ERROR register.
 */
#define	NO_FCIMASTER	0x4000000
int
fchip_errclr_test(__psunsigned_t chip_base)
{
    int		result = 0;
    evreg_t     value;
    /* 	
     * Write some data to the FCHIP_ERROR, read the FCHIP_ERRORCLR
     * and check if this value is proper. Also check if FCHIP_ERROR
     * is zero or not.
     */
    
    message(FCHK_EREG);

    WR_REG (chip_base + FCHIP_ERROR , 0x5A5A);

    value = RD_REG(chip_base + FCHIP_ERROR_CLEAR);
    if ((value & 0xffff) != 0x5A5A){
	mk_msg(FERR_NO_ECLR, 0x5A5A, (value & 0xffff));
	result = 1;
    }
    if (value & NO_FCIMASTER)
	mk_msg("Fchip Error Register: Bit 26 set => No VMECC/FCG ??\n");

    if (value = RD_REG(chip_base + FCHIP_ERROR) & ~NO_FCIMASTER){
	mk_msg(FERR_ERR_REG,0, value);
    	result = 1;
	analyze_error(1);
    }

    message(FCHK_EREGP);

    Return (result); 
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

int
fchip_addrln_test(__psunsigned_t chip_base)
{
    evreg_t	regval;
    int		result = 0;
    int		i;
    Fchip_regs	*fregs;


    message(FCHK_ADDR);

    for(fregs=fchip_regs,i=0; fregs->reg_no != (-1); fregs++,i++){
        if (fregs->bitmask0 == 0)
    	continue;
    	
        WR_REG(chip_base+fregs->reg_no,(random_nos[i] & fregs->bitmask0));
    }

    /* Now readback the register values		*/
    for(fregs=fchip_regs,i=0; fregs->reg_no != (-1); fregs++,i++){
        if (fregs->bitmask0 == 0)
    	continue;
    	
	/* Since TLBBASE may get clobbered by flush reg write. */
	if (fregs->reg_no == FCHIP_TLB_BASE) 
	    continue;

        regval = RD_REG(chip_base+fregs->reg_no) & fregs->bitmask0;
        if (regval != (random_nos[i] & fregs->bitmask0)){
    	    mk_msg(FERR_REG_ADDR, fregs->reg_no, 
			random_nos[i] & fregs->bitmask0, regval);
	    analyze_error(2);
	    result++;
        }
    }

    message(FCHK_ADDRP);

    Return(result);

}

/*
 * Function : fchip_tlb_flush.
 * Description:
 *	Make some valid entries into the Fchip TLB, and by writing some
 *	pattern to flush mask and base address, check if the proper
 *	entry gets flushed.
 *
 *      analyze_error(2) is called only in the end so that during the bringup
 *	we get to know all the problems. 
 */
#define	FCHIP_TLBVALID		0x200000 /* Bit 22 =1 => Valid TLB entry */
int
fchip_tlb_flush(__psunsigned_t chip_base)
{
    int	flush_err;

    /* The Ebus addresses are not set since this is just to check if
     * the flush mask works properly
     */

    message(FCHK_TLB);

    WR_REG(chip_base+FCHIP_TLB_FLUSH, 0xffffffff);
    /* Invalidate all entries			*/
    for(flush_err=0; flush_err < 8; flush_err++)
        WR_REG(chip_base+(FCHIP_TLB_IO0 + (flush_err * 8)), 0);

    flush_err = 0;	/* Reset flush_err		*/

    WR_REG(chip_base+FCHIP_TLB_IO0, (FCHIP_TLBVALID | 0xa00)); 
    WR_REG(chip_base+FCHIP_TLB_IO1, (FCHIP_TLBVALID | 0xb00)); 

    /* Writing a 0 should not flush any entry	*/
    WR_REG(chip_base+FCHIP_TLB_FLUSH, 0);

    if ( (( RD_REG(chip_base + FCHIP_TLB_IO0 ) & FCHIP_TLBVALID ) == 0) ||
         (( RD_REG(chip_base + FCHIP_TLB_IO1 ) & FCHIP_TLBVALID ) == 0)){
	mk_msg(FERR_TLB_NOFL);
        flush_err |= 1;
    }


    /* Set the Flush mask to flush First Entry.	*/
    WR_REG(chip_base+FCHIP_TLB_FLUSH, 0xa00);

    /* Now the TLB_IO0 Entry should be invalidated and 
     *         TLB_IO1 Entry should be in place
     */
    if ( (  RD_REG(chip_base + FCHIP_TLB_IO0 ) & FCHIP_TLBVALID ) ||
         (( RD_REG(chip_base + FCHIP_TLB_IO1 ) & FCHIP_TLBVALID ) == 0)){
	mk_msg(FERR_TLB_1FL);
        flush_err |= 2;
    }

    /* Repeat the same using the flush mask and using multiple entries */
    WR_REG(chip_base+FCHIP_TLB_IO0, (FCHIP_TLBVALID | 0xa00)); 
    WR_REG(chip_base+FCHIP_TLB_IO1, (FCHIP_TLBVALID | 0xb00));
    WR_REG(chip_base+FCHIP_TLB_IO2, (FCHIP_TLBVALID | 0xc00));

    /* This command should flush all entries.	*/
    WR_REG(chip_base+FCHIP_TLB_FLUSH, 0xf00);

    if (RD_REG(chip_base+FCHIP_TLB_IO0) & FCHIP_TLBVALID ) 
    	flush_err |= 4;

    if (RD_REG(chip_base+FCHIP_TLB_IO1) & FCHIP_TLBVALID ) 
    	flush_err |= 8;

    if (RD_REG(chip_base+FCHIP_TLB_IO2) & FCHIP_TLBVALID ) 
    	flush_err |= 16;

    if (flush_err){
        mk_msg(FERR_TLB_FLUSH);
	analyze_error(2);
    }
    
    message(FCHK_TLBP);

    Return (flush_err);
}

/*
 * Function : fchip_errbit_test
 * Descriptin: Issue FCI reset command, and check if the bit gets set.
 */

#define	FERR_SW_FCI_RESET	0x800000
int
fchip_errbit_test(__psunsigned_t chip_base)
{
    evreg_t	error;

    /* 1. Reset the FCI and check */
    message(FCHK_RSTFCI);

    WR_REG(chip_base+FCHIP_SW_FCI_RESET, 1);

    if ((error = RD_REG(chip_base+FCHIP_ERROR) & FERR_SW_FCI_RESET) == 0){
	mk_msg(FERR_ERRB_TEST, FERR_SW_FCI_RESET, error);
	analyze_error(2);
    }
    
    message(FCHK_RSTFCIP);
    Return(error);

}

/*
 * This routine is useful only for bringup purpose, and not of much significance
 * later. So could be deleted at appropriate time.
 */
int
fregs(int iowin, int slot, int anum)
{
    Fchip_regs	*fregs=fchip_regs;
    __psunsigned_t 	swin = SWIN_BASE(iowin, anum);
    int		i;

    if (setjmp(fbuf)){
	printf("Exception while printing Fchip registers in IO4: %d Adap: %d\n",
			slot,anum);
	show_fault();
	return(1);
    }
    set_nofault(fbuf);

    swin = SWIN_BASE(iowin, anum);

    for (i=0; fregs->reg_no != (-1); i++, fregs++){
	printf("%04x: %08x\t", fregs->reg_no, 
		(unsigned)RD_REG(fregs->reg_no + swin));
	if ((i&3) == 3)
		printf("\n");
    }
    printf("\n");

    clear_nofault();
    return(0);


}
