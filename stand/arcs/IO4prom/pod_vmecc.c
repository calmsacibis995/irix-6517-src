/***********************************************************************\
*	File:		pod_vmecc.c					*
*									*
*	Contains the routines Used to test the fchip functionality  	*
*	and path, and possibly do the initialization of the Chip also.  *
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_vmecc.c $Revision: 1.25 $"

#include "libsc.h"
#include "libsk.h"
#include "pod_fvdefs.h"

void vmecc_exists(__psunsigned_t, unsigned, unsigned);
int  mk_dups(int, int);


extern	unsigned	random_nos[];

Vmecc_regs vmecc_regs[] = {
{VMECC_CONFIG,		0xfffffffe},
{VMECC_A64SLVMATCH,	0xffffff00},
{VMECC_A64MASTER,	0xffffffff},
{VMECC_ERRADDRVME,	0},
{VMECC_RMWMASK,		0xffffffff},
{VMECC_RMWSET,		0xffffffff},
{VMECC_RMWADDR,		0xffffffff},
{VMECC_RMWAM,		0x003f},
{VMECC_RMWTRIG,		0},
{VMECC_DMAVADDR,	0xffffffff},
{VMECC_DMAEADDR,	0xffffffff},
{VMECC_DMABCNT,		0xffffff},
{VMECC_DMAPARMS,	0xfff},
{VMECC_PIOTIMER,	0},
{VMECC_INT_ENABLE,	0x1fff},
{VMECC_INT_REQUESTSM,	0},
{VMECC_ERRXTRAVME,	0},
{VMECC_ERRORCAUSES,	0},
{VMECC_INT_ENABLECLR,	0},
{VMECC_ERRCAUSECLR,	0},
{VMECC_INT_ENABLESET,	0},
{VMECC_IACK1,		0xff},
{VMECC_IACK2,		0xff},
{VMECC_IACK3,		0xff},
{VMECC_IACK4,		0xff},
{VMECC_IACK5,		0xff},
{VMECC_IACK6,		0xff},
{VMECC_IACK7,		0xff},
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

/* Memory area accessed by the VMECC loopback test */

unsigned vmecc_slave_area[SLAVE_DATA]; /* Data read from here by DMA Engn */

#ifdef	NEWTSIM
unsigned vmecc_slave_rdarea[SLAVE_DATA]; /* Data written to here by DMA Engn */
int  	Dma_state;
#endif

jmp_buf	vbuf;


int
check_vmecc(unsigned window, int io4_slot, int ioa_num )
{

    __psunsigned_t chip_base;
    volatile 	int level=0;
    int 	jval;
    k_machreg_t sr;
    
#ifdef	NEWTSIM
    print_hex(0xbafd0000);
#endif

    sr = get_SR(); set_SR(sr | SR_KX); 

    chip_base = SWIN_BASE (window, ioa_num);

    setup_globals(window, io4_slot, ioa_num);
    message(VME_TEST);

    Nlevel++;

    Mapram = tlbentry(window, 0, 0);

#ifndef	NEWTSIM
    if ((jval = setjmp(vbuf)) == 0){
	set_nofault(vbuf);
	vmecc_exists(chip_base, io4_slot, ioa_num); level++;
        vmecc_regtest(chip_base); level++;
        vmecc_addr_test(chip_base); level++;
        WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE);
        /* vmecc_errtest(chip_base); */ level++;
        vmecc_intrmask(chip_base);  level++;
        vmecc_intrtest(chip_base); level++;
        vmecc_rd_probe(chip_base, window, ioa_num); level++;
        vmecc_dmavmeRd(chip_base); level++;
        vmecc_dmavmeWr(chip_base); level++;
        /* vmecc_rmw_lpbk(chip_base); */ level++;
	tlbrmentry(Mapram); Mapram = 0;
        pod_vmelpbk(window, io4_slot, ioa_num);
	level = SUCCESS;
    }
    else {
	/* Return due to an exception	*/
	if (jval == 1){
	    message("Exception while testing VMECC in <slot:%d adap:%d>\n",
			io4_slot, ioa_num);
	    show_fault();
	}
	switch(level){
	    default:
	    case 0 : message("FAILED to get Master-ID from Fchip for VMECC\n");
		     message("Probably Fchip/FCI/VMECC is bad\n");
		     break;
	    case 1 : message(VCHK_REGRF); break;
	    case 2 : message(VCHK_ADDRF); break;
	    case 3 : message(VCHK_EREGF); break;
	    case 4 : message(VCHK_IMASKF); break;
	    case 5 : message(VCHK_INTRF); break;
	    case 6 : message(VCHK_RDPRBF); break;
	    case 7 : message(VCHK_DMARF); break;
	    case 8 : message(VCHK_DMAWF); break;
	    case 9 : message(VCHK_RMWLF); break;
	    case 10: break; /* Message is already printed  */
	}
	level = FAILURE;
    }
    clear_nofault();
#else	/* Simulation mode */
    Dma_state = 0;
    WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE);
    /* vmecc_errtest(chip_base);  */
    /* vmecc_intrmask(chip_base); PASSED */
    /* vmecc_intrtest(chip_base); PASSED */
    /* vmecc_rmw_lpbk(chip_base); PASSED */
    /* pod_vmelpbk(chip_base, window, ioa_num); PASSED */
#endif

    if(Mapram){
	tlbrmentry(Mapram); Mapram = 0;
    }
    Nlevel--;

#ifndef	NEWTSIM
    (level == SUCCESS) ? message(VME_TESTP) : message(VME_TESTF);
#endif

    set_SR(sr); 

    Return (level);
}

/* Check if the VMECC exists at the given address */
void vmecc_exists(__psunsigned_t chip_base, unsigned ioslot, unsigned ioa_num)
{
    __psunsigned_t chip_type;
    /* Check if at the given base Fchip exists, and its 
     * master register shows FCI master as VMECC 
     */
    fchip_chkver(chip_base, ioslot, ioa_num);
    chip_type = RD_REG(chip_base + FCHIP_MASTER_ID);
    if ((chip_type != IO4_ADAP_VMECC) && (chip_type != 0x12)){
	message("Master Id returned by F: 0x%x doesnot correspond to VMECC\n", 
		chip_type); 
	analyze_error(0);
    }
}

int promvmecc_init(unsigned window, unsigned ioa_num)
{
    __psunsigned_t chip_base = SWIN_BASE(window, ioa_num);

    WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE);
    WR_REG(chip_base+VMECC_A64SLVMATCH, 0);
    WR_REG(chip_base+VMECC_A64MASTER, 0);
    /* If Drop mode bit is set, just cant do anything but resetting system */
    if (RD_REG(chip_base+VMECC_ERRCAUSECLR) & 0x80){
	mk_msg("VMECC is in PIO-DROP mode... Reset the system\n");
	return (FAILURE);
    }

    return(0);

}

int
vmecc_regtest(__psunsigned_t chip_base)
{

    Vmecc_regs	*vregs = vmecc_regs;
    unsigned	failure=0;

    message(VCHK_REGR);

    /* Check all the registers first. We can analyse later	*/
    for(; vregs->reg_no != (-1); vregs++){
        if (check_regbits(chip_base+vregs->reg_no, vregs->bitmask0, 0)) 
	    failure++;
    }
    if (failure)
	analyze_error(3);
         
    message(VCHK_REGRP);

    return ( SUCCESS );
}

int
vmecc_addr_test(__psunsigned_t chip_base)
{
    int		i;
    evreg_t	 value;
    Vmecc_regs	*vregs = vmecc_regs;


    message(VCHK_ADDR);

    for (i=0; vregs->reg_no != (-1); vregs++,i++){
    	if (vregs->bitmask0 == 0)
    	    continue;

    	WR_REG(chip_base+vregs->reg_no,random_nos[i] & vregs->bitmask0);
    }

    /* Read back the values from the registers.	*/

    for (vregs=vmecc_regs, i=0; vregs->reg_no != (-1); vregs++, i++){
        if (vregs->bitmask0 == 0)
    		continue;
        value = RD_REG(chip_base+vregs->reg_no) & vregs->bitmask0;
        if (value != (random_nos[i] & vregs->bitmask0)){
    	    mk_msg(VME_ADDR_ERR, vregs->reg_no, 
			(random_nos[i] & vregs->bitmask0), value);
	    analyze_error(3);
    	    return(FAILURE);
         }
    }

    message(VCHK_ADDRP);

    return (SUCCESS);

}

/* 
 * Function: vmecc_errtest
 * Description :
 *	Write some data to the ERRORCAUSES register, and see if it gets
 *	cleared by reading the ERRCAUSECLR register.
 */
int
vmecc_errtest(__psunsigned_t chip_base)
{
    evreg_t	clrreg, causereg;
    /*
     * Write some data to the VMECC errors register, read it via
     * the errclear register, and check if the err register is 
     * cleared.
     */
    message(VCHK_EREG);

    WR_REG(chip_base+VMECC_ERRORCAUSES, patterns[1]);


    if ((clrreg = RD_REG(chip_base+VMECC_ERRCAUSECLR)) != patterns[1])
	mk_msg(VME_CLREG_ERR,patterns[1], clrreg);

    if ((causereg = RD_REG(chip_base+VMECC_ERRORCAUSES)) != 0)
	mk_msg(VME_CAREG_ERR, 0, causereg);

    if ( (clrreg != patterns[1] ) || ( causereg != 0)){
	analyze_error(3);
    }

    message(VCHK_EREGP);

    return	SUCCESS;
}

/* 
 * Function : vmecc_intrmask
 * Description :
 *	For each of the interrupt enable bits, write to set mask, see
 *	if it gets set in the INTENABLE register, clear the same and 
 *	see if this gets cleared.
 */
int
vmecc_intrmask(__psunsigned_t chip_base)
{
    int 	i;
    short	result=0;
    evreg_t	value, mask;

    message(VCHK_IMASK);

    /* First clear all interrupt enables			*/
    WR_REG(chip_base+VMECC_INT_ENABLECLR, 0x1fff);

    for (i=0; i < 11; i++){ /* No of Interruptible lines	*/
    	mask = 1 << i;

        WR_REG(chip_base+VMECC_INT_ENABLESET, mask);
        if((value = RD_REG(chip_base+VMECC_INT_ENABLE)) != mask){
	    mk_msg(VME_IMSET_ERR, mask, value);
	    result |= mask; 
	}

        WR_REG(chip_base+VMECC_INT_ENABLECLR, mask);
        if ((value = RD_REG(chip_base+VMECC_INT_ENABLE)) != 0){
    	    mk_msg(VME_IMCLR_ERR, 0, value);
	    result |= mask;
	}
    }
    WR_REG(chip_base+VMECC_INT_ENABLECLR, 0x1fff); /* Clear again	*/

    if(result)
	analyze_error(3);
    
    message(VCHK_IMASKP);

    Return(result); 

}

/*
 * Function : vmecc_intrtest
 * Description:
 * 	Enable the VMECC to generate interrupt on a particular level.
 *	Set the Intr No to 1 and destination to 0
 *	Set the INT_REQUESTSM register for that particular level.
 *	see if the cpu gets an interrupt.
 * 	No interrupt handler is attached for this level.
 */
#define	VMECC_INTLVL	(DIAG_INT_LEVEL+1)
int
vmecc_intrtest(__psunsigned_t chip_base)
{

    int		i;
    evreg_t	mask, result;
    unsigned	vector;
    k_machreg_t	status;
    
    message(VCHK_INTR);

    status = get_SR(); set_SR(SR_KX);

    vector = (VMECC_INTLVL << 8)| 0x40;
    RD_REG(chip_base + VMECC_ERRCAUSECLR); /* Clear the error register */
    EV_SET_LOCAL(EV_ILE, EV_EBUSINT_MASK);

    for(i=1; i < 11; i++){

	while(mask=RD_REG(EV_HPIL)){ /* Clear all pending interrupts */
	    WR_REG(EV_CIPL0, mask);
	}

	mask = (1 << i);
        WR_REG(chip_base+VMECC_INT_ENABLECLR, 0x1FFF);	/* Disable interrupt */
        WR_REG(chip_base+(VMECC_VECTORERROR + (i*8)), vector);
    
        WR_REG(chip_base+VMECC_INT_REQUESTSM, mask);
    
        WR_REG(chip_base+VMECC_INT_ENABLESET, mask);	/* Enable VME ERROR  */
    
        /* How long to wait here. One possible way is to do an another
         * PIO onto the VMECC and wait for the PIO response. The 
         * interrupt should have arrived before the PIO response
         * if the ordering is proper.
         */
        (void)RD_REG(chip_base + VMECC_INT_REQUESTSM);
        (void)RD_REG(chip_base + VMECC_INT_REQUESTSM);
        (void)RD_REG(chip_base + VMECC_INT_REQUESTSM);
    
        if (EV_GET_LOCAL(EV_IP0) == 0){
    	    mk_msg(VME_NOINTR_ERR, i);
    	    /* analyze_error(3); */
        } else {
            if ((result = EV_GET_LOCAL(EV_HPIL)) != VMECC_INTLVL){
    	        mk_msg(VME_IRESP_ERR, VMECC_INTLVL, result);
            }
	}
        WR_REG(chip_base+(VMECC_VECTORERROR + (i*8)), 0);

	WR_REG(EV_CIPL0, VMECC_INTLVL);
	EV_SET_LOCAL(EV_ILE, 0);

    }
    message(VCHK_INTRP);
    set_SR(status);

    return SUCCESS;

}

#define	AND_MASK	(unsigned)0xffff
#define	SET_MASK	(unsigned)0xaaaa

#define	ANDR_MASK	(uint)0xffffffff
#define	SETR_MASK	0
/* 
 * Function : vmecc_rmw_lpbk
 * Description :
 * 	Setup the RMW registers appropriately, such that when the RMW 
 *	operation is triggered, it results in vmecc reading the address
 *	from the system memory and returning it.
 * Steps to be followed.
 * 1. Initialize the vme_slave_area for some fixed value.
 * 2. Make an entry in IARAM of this board to have a pointer to 
 *    this location.
 * 3. Enable 32 bit slaves having A31-A28 being 1000 - 1110
 * 4. Program the RMW registers appropriately to access the 
 *    address which can be recognized by the slave.
 * 5. Access the RMWOP register with proper wordsize and 
 *    check the data. 
 */
int
vmecc_rmw_lpbk(__psunsigned_t chip_base, uint *slave_offset)
{
    uint	i, error = 0, data;
    uint	rmwval;
    paddr_t	memoffs;
    volatile	int readit;

#define	SIZE	4

    message(VCHK_RMWL);
    /* Initialize the VMECC Slave area 		*/
    for (i=0; i < SLAVE_DATA ; i++)
    	slave_offset[i] = (i << 8) | i;

    memoffs = K0_TO_PHYS(slave_offset);
    for (i = 0 ; i < SLAVE_DATA; i++, memoffs += SIZE){
	data = slave_offset[i];
        rmwval = a32_rmw_chk(chip_base, AND_MASK, SET_MASK, (uint)memoffs, 
			     SIZE);
        if (rmwval != data){
	    mk_msg(VMELP_RMW_ERR, data, rmwval);
    	    error++;
	    analyze_error(3);
	}

	/* Read back using RMW format itself. */
	data = (data & AND_MASK) | SET_MASK;
	rmwval = a32_rmw_chk(chip_base, ANDR_MASK, SETR_MASK, (uint)memoffs, 
			     SIZE);
	if (rmwval != data){
	    mk_msg(VMELP_RMWD_ERR, data, rmwval);
	    error++;
	    analyze_error(3);
	}
    }

    message(VCHK_RMWLP);

    Return(error);

}

/*
 * Function : vmecc_dmavmeRd
 * Description : Program the dma to do a read operation from an invalid 
 *	VME bus address and observe the errors generated.
 */
#define	DMA_RDPARAMS	(0x100 | VME_A32NPAMOD) /* VME -> EBUS, Word xfer  */
#define	DMA_WRPARAMS	(0x300 | VME_A32NPAMOD) /* EBUS -> VME  Word xfer */

int
vmecc_dmavmeRd(__psunsigned_t chip_base)
{
    evreg_t	dma_error;

    message(VCHK_DMAR);

    WR_REG(chip_base+VMECC_DMAVADDR, A32M_ADDR);
    WR_REG(chip_base+VMECC_DMAEADDR, K0_TO_PHYS(vmecc_slave_area));

    WR_REG(chip_base+VMECC_DMABCNT, 1024);
    /* DMA VME Parameters :
     * VME to EBUS Transfer, word transfers, and A32 AM 
     */
    WR_REG(chip_base+VMECC_DMAPARMS, DMA_RDPARAMS | 0x80000000);

    /* Expecting VMECC timeout error -> bit 1 	*/
    /* This takes about 64 usecs. We need to wait that long */
    dma_error = RD_REG (chip_base+VMECC_ERRORCAUSES);

    message(VCHK_DMARP);

    Return(dma_error);

}

/*
 * Function : vmecc_dmavmeWr
 * Description :
 *	Program the DMA engine on VMECC to do a Ebus Read and
 *	VME Write operation to an invalid VME address, and 
 *	observe the errors reported
 *
 *	This requires the setting up of the DMA MAPRAM on Ebus side. 
 */
int
vmecc_dmavmeWr(__psunsigned_t chip_base)
{
    paddr_t slave_paddr = K0_TO_PHYS(vmecc_slave_area);
    unsigned i;

    message(VCHK_DMAW);

    /* Initialize the VMECC Slave area 		*/
    for (i=0; i < SLAVE_DATA; i++)
    	vmecc_slave_area[i] = (i << 8) | i;

    /* Make IOMAP RAM entry for Ebus virtual address	*/
    *(unsigned *)(Mapram + IAMAP_L1_ADDR(0,slave_paddr)) = (uint)
	((slave_paddr >> IAMAP_L1_BLOCKSHIFT) <<9);

    /* put_word(window, 0, (slave_paddr >> IAMAP_L1_BLOCKSHIFT), 
		(slave_paddr >> IAMAP_L1_BLOCKSHIFT)); */

    /* WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE); */
    WR_REG(chip_base+VMECC_DMAVADDR, A32M_ADDR);
    WR_REG(chip_base+VMECC_DMAEADDR, slave_paddr);
    WR_REG(chip_base+VMECC_DMABCNT, 1024);
    WR_REG(chip_base+VMECC_DMAPARMS, DMA_WRPARAMS | 0x80000000);

    /* Expecting VMECC timeout error -> bit 1 	*/
    (void) RD_REG (chip_base+VMECC_ERRORCAUSES);


    message(VCHK_DMARP);

    Return(SUCCESS);
}

int
vmecc_rd_probe(__psunsigned_t chip_base, int window, int ioa_num)
{
    /* Now let's try an invalid PIO Read and see response	*/
    jmp_buf	rdprb;
    machreg_t	*oldone;
    evreg_t	ereg;

    message(VCHK_RDPRB);

    oldone = nofault;			/* put here to make compiler happy */

    if (setjmp(rdprb)){
	ereg = RD_REG(chip_base+VMECC_ERRCAUSECLR);
	if (ereg != 0x2)
	    mk_msg("Invalid error during VMECC rd probe test: 0x%x Exp: 0x2\n",
			ereg);
	RD_REG(chip_base+FCHIP_ERROR_CLEAR);
	clear_nofault();
	nofault = oldone; /* Copy back the one saved */
    } else {
	set_nofault(rdprb);
        WR_REG(chip_base+VMECC_PIOREG(1), 0x2400);
        get_word(window, ioa_num, 0x800000);
        message(VCHK_RDPRBF);
	return(FAILURE);
    }
    message(VCHK_RDPRBP);

    return(SUCCESS);

}
#ifdef	NEWTSIM
/*
 * Function : vmecc_pio
 * Description :
 *	Do pio to the VME controller stub module sitting on the 
 *	VME Bus. To be used only in Simulation Mode.
 */
int
vmecc_pio(__psunsigned_t chip_base, int window, int ioa_num)
{
    int	i,j;
    /* Do PIOs on the stub controller module out there	*/

    /* First program the PIOMAP registers, and then read data	*/
    WR_REG(chip_base+VMECC_PIOREG(1), 0x2402);

    i = get_word(window, ioa_num, 0x800000);

    put_word(window, ioa_num, 0x800000, 0xdecade);

    i = get_word(window, ioa_num, 0x800000);

}

/*
 * Do a VMECC DMA Read Operation to the slave stub.
 * Read Data from Ebus Memory and Write it to Slave Memory.
 */
#define	A32S_ADDR	0x11000000
#define A32B_AM		0xb	/* A32 Block AM */
#define	DMA_BRDPARAMS	(0x100 | A32B_AM) /* VME -> EBUS, Word xfer  */
#define	DMA_BWRPARAMS	(0x300 | A32B_AM) /* EBUS -> VME  Word xfer */

int rd_len = 0;
int rd_param = 0;
int rd_size  = 1;
int rdlen_arr[] = { 1, 18, 67, 84, 101, 118, 135, 152 }; 

int
vmecc_sl_dmawr(__psunsigned chip_base, unsigned *rdptr, caddr_t map)
{

    unsigned slave_offset, phys_addr;
    int	i;

    for (i=0; i< SLAVE_DATA; i++)
	rdptr[i] = (unsigned)(&rdptr[i]);
    
    /* Make a One level entry in Mapram for this address */
    phys_addr = K0_TO_PHYS(rdptr);
    slave_offset = ((phys_addr >> IAMAP_L1_BLOCKSHIFT) << 9);

    *(unsigned *)(map + IAMAP_L1_ADDR(0, phys_addr)) = slave_offset;

    WR_REG(chip_base+VMECC_DMAVADDR, A32S_ADDR);
    WR_REG(chip_base+VMECC_DMAEADDR, phys_addr);

    WR_REG(chip_base+VMECC_DMABCNT, (rdlen_arr[rd_len] * sizeof(unsigned)));
    /* Generate an interrupt at end of DMA */

    WR_REG(chip_base+VMECC_VECTORDMAENG, ((DMA_WRLEVEL<<8)|RD_REG(EV_SPNUM)));
    WR_REG(chip_base+VMECC_INT_ENABLESET, 0x1fff); /* Enable ALL intr */

    WR_REG(chip_base+VMECC_DMAPARMS, (DMA_BWRPARAMS | 0x80000000));

}

/*
 * VMECC DMA Engine reading data from slave memory.
 */
int
vmecc_sl_dmard(__psunsigned_t chip_base, unsigned *wrptr, caddr_t map )
{

    unsigned slave_offset, phys_addr;
    int	i;

    /* Make a One level entry in map for this address */
    phys_addr = K0_TO_PHYS(wrptr);
    slave_offset = ((phys_addr >> IAMAP_L1_BLOCKSHIFT) << 9);

    *(unsigned *)(map + IAMAP_L1_ADDR(0, phys_addr)) = slave_offset;

    WR_REG(chip_base+VMECC_DMAVADDR, A32S_ADDR);
    WR_REG(chip_base+VMECC_DMAEADDR, phys_addr);

    /*WR_REG(chip_base+VMECC_DMABCNT, (SLAVE_DATA * sizeof(unsigned))); */
    if (rd_len == 8){
	rd_len = 0;
	rd_param = 0x80; /* short word access */
	rd_size = 2;
    }
    WR_REG(chip_base+VMECC_DMABCNT, rdlen_arr[rd_len*rd_size]);
    rd_len++;

    /* Let DMA generate an intr at end of transfer */
    WR_REG(chip_base+VMECC_VECTORDMAENG, ((DMA_RDLEVEL<<8)|RD_REG(EV_SPNUM)));
    WR_REG(chip_base+VMECC_INT_ENABLESET, 0x1fff); /* Enable ALL intr */
    /* DMA VME Parameters :
     * VME to EBUS Transfer, word transfers, and A32 AM 
     */
    /*WR_REG(chip_base+VMECC_DMAPARMS, (DMA_BRDPARAMS | 0x80000000)); */
    WR_REG(chip_base+VMECC_DMAPARMS, (rd_param|0x09 | 0x80000000));
}

/* Do the DMA Operation on the vmecc with base address 'chip_base' */

int
vmecc_sl_dmaop(__psunsigned_t chip_base)
{
    /* Do a DMA Write to the Slave Memory, Poll for it completion
     * and then, do a DMA read from the same address, compare the 
     * data
     */

    unsigned status;

again:
    switch(Dma_state){

    case 0 : vmecc_sl_dmawr(chip_base, vmecc_slave_area, Mapram); 
	     Dma_state++; break;

    case 1 : 
	    status = RD_REG(chip_base+VMECC_DMAPARMS);

	    if (status & (unsigned)0x80000000)
		break;
	    if (RD_REG(EV_HPIL) == DMA_WRLEVEL)
		WR_REG(EV_CIPL0, DMA_WRLEVEL);

	    WR_REG(chip_base+VMECC_INT_ENABLECLR, 0x1fff);

	    if (status & (unsigned)0x60000000){
	        print_hex(status);
		Dma_state = DMA_DONE; 
	        break;
	    }
	    Dma_state++;
	    goto again;

    case 3 : 
	    /* Check if DMA has completed */
	    status = RD_REG(chip_base+VMECC_DMAPARMS);

	    if (status & (unsigned)0x80000000)
		break;	/* Not over yet */
		
#ifdef	DEBUG_POD
	    print_hex(RD_REG(chip_base+VMECC_INT_REQUESTSM));
#endif

	    if (RD_REG(EV_HPIL) == DMA_RDLEVEL)
		WR_REG(EV_CIPL0, DMA_RDLEVEL);
#ifdef	DEBUG_POD
	    else
		print_hex(0xbad1000 | RD_REG(EV_HPIL));
#endif

	    WR_REG(chip_base+VMECC_INT_ENABLECLR, 0x1fff);

	    if (status & (unsigned)0x60000000){
	        print_hex(status);
		Dma_state = DMA_DONE; 
		break;  /* Error Case */
	    }
	    Dma_state++;
	    goto again; 
    
    case 2 : vmecc_sl_dmard(chip_base, vmecc_slave_rdarea, Mapram); 
	     Dma_state++; break;

	    /* Successfull state */
    case 4 : print_hex(0xeeeeeeee); 
	     Dma_state = 0; goto again; /* Do it again */
	     /* Dma_state = DMA_DONE; break; */

    case DMA_DONE : break; 

    default: print_hex((0xaaaaaa<<4) | Dma_state); break;
    }

    return(Dma_state);
}

#endif	/* NEWTSIM	*/

unsigned
a32_rmw_chk(__psunsigned_t chip_base, uint mask, uint set, uint offset, uint size)
{

    uint 	data=0;
    uint	lsb3, addr;

    WR_REG(chip_base+VMECC_RMWMASK, mk_dups(mask, size));
    WR_REG(chip_base+VMECC_RMWSET,  mk_dups(set, size));
    WR_REG(chip_base+VMECC_RMWAM, VME_A32NPAMOD);

    addr = A32_ADDR + offset;

    switch (size){
        case 1: lsb3 = addr & 0x7;
		WR_REG(chip_base+VMECC_RMWADDR, addr);
		data = *(unchar *)(chip_base+VMECC_RMWTRIG + lsb3); 
		break;

        case 2: lsb3 = addr & 0x6;
    		WR_REG(chip_base+VMECC_RMWADDR, addr);
		data = *(ushort *)(chip_base+VMECC_RMWTRIG + lsb3); 
		break;

        default :
        case 4: lsb3 = addr & 0x4;
    		WR_REG(chip_base+VMECC_RMWADDR, addr);
		data = *(unsigned *)(chip_base+VMECC_RMWTRIG + lsb3); 
		break;
    }

    return(data);


}

int mk_dups(int val, int size)
{
     switch(size){
         case 1 : val |= ((val << 8) | (val << 16) | (val << 24)); break;

         case 2 : val |= (val << 16); break;

         default : break;

     }

     return(val);
}

/*ARGSUSED*/
int
vregs(int iowin, int io4slot, int anum)
{
    Vmecc_regs	*vregs = vmecc_regs;
    __psunsigned_t chip_base = SWIN_BASE(iowin, anum);
    int		i;

    for(i=0; vregs->reg_no != -1; vregs++, i++){
	printf("%04x: %08x\t", vregs->reg_no, 
			(unsigned)RD_REG(chip_base+vregs->reg_no));
	if ((i&3) == 3)
		printf("\n");
    }
    printf("\n");
    return SUCCESS;
}
    
int
io_errreg(int window, int ioslot, int anum)
{
/* Dump the error registers in IO4/F/VMECC */
    __psunsigned_t chip_base = SWIN_BASE(window, anum);

    printf("IO4 Ebus error: 0x%x\t IO4 Ibus error: 0x%x\n",
	(uint)IO4_GETCONF_REG(ioslot, IO4_CONF_EBUSERROR), 
	(uint)IO4_GETCONF_REG(ioslot, IO4_CONF_IBUSERROR)); 

    printf("F Error status: 0x%x\t VME Error Causes: 0x%x\n",
	(uint)RD_REG(chip_base+FCHIP_ERROR), 
	(uint)RD_REG(chip_base+VMECC_ERRORCAUSES));

    return SUCCESS;
}

int
io_errcreg(int window, int ioslot, int anum)
{
/* Dump the error registers in IO4/F/VMECC */
    __psunsigned_t chip_base = SWIN_BASE(window, anum);

    printf("IO4 Ebus error : 0x%x\t IO4 Ibus errro : 0x%x\n",
	(uint)IO4_GETCONF_REG_NOWAR(ioslot, IO4_CONF_EBUSERRORCLR), 
	(uint)IO4_GETCONF_REG_NOWAR(ioslot, IO4_CONF_IBUSERRORCLR)); 

    printf("F Error status: 0x%x\t VME Error Causes: 0x%x\n",
	(uint)RD_REG(chip_base+FCHIP_ERROR_CLEAR), 
	(uint)RD_REG(chip_base+VMECC_ERRCAUSECLR));

    return SUCCESS;
}
