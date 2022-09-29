/*
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "arcs/ide/IP19/io/io4/vme/vme_dmaeng.c $Revision: 1.16 $"

#include <sys/cpu.h>
#include <sys/invent.h>
#include <sys/vmereg.h>
#include <fault.h>
#include <prototypes.h>
#include "vmedefs.h"
#include "cdsio.h"
#include <libsc.h>

#define	BAUD_RATE_9600			0x6
#define	DATA_PATTERN			0xaa
#define	INTR_ENABLED			0x8
#define	ROAK_INTR_MODE			0x80

#define	DMASIZE				2048

#define	USAGE	"Usage: %s [io4slot anum] \n"

#define	TIME_LIMIT			0x1000

#define INT_MASK			0x1000000
#define	DMA_BUSY			(unsigned)0x80000000
#define	DMA_ERROR			0x60000000

extern int	io4_tslot, io4_tadap;

#ifndef	MALLOC
extern char	vmelpbk_buf[];
#endif

static	int	vme_loc[3];
/*
 * Check the VMECC DMA engine Functionality using CDSIO board as the 
 * external controller. 
 * This test will only try to RESET the CDSIO board. If this succeeds, 
 * No further command is issued to the CDSIO board. Test then uses the 
 * DMA engine in VMECC to do transfer of data from Host memory to 
 * controller memory and viceversa 
 */
vme_dmaeng(int argc, char **argv)
{

    uint	found=0, fail=0;
    int		retval, tskip=0, trun=0;
    uint	slot, anum;

    if (argc > 1){
        if (io4_select(1, argc, argv)){
            msg_printf(ERR, USAGE, argv[0] );
                return(1);
        }
    }

    while(!fail && io4_search(IO4_ADAP_FCHIP, &slot, &anum)){
        if ((argc > 1) && ((slot != io4_tslot) || (anum != io4_tadap)))
            continue;

	if (fmaster(slot, anum) != IO4_ADAP_VMECC)
	    continue;

        found = 1;

	retval = vmedma_eng(slot, anum);
	if (retval != TEST_SKIPPED) {
	    fail |= retval;
	    trun++;
	} else
	    tskip++;
   
    }
    if (!found) {
	if (argc > 1) {
	    msg_printf(ERR, "Invalid Slot/Adapter number\n");
	    msg_printf(ERR, USAGE, argv[0]);
	}
	fail = TEST_SKIPPED;
    }
    if(fail)
	io4_search(0, &slot, &anum);
    else if (tskip && !trun)
	fail = TEST_SKIPPED;

    return(fail);

}

int
vmedma_eng(int slot, int anum)
{
    int             fail=1;
    unsigned int    jval;
    volatile unsigned int refresh_timer;


    /* The setup phase */
    setup_err_intr(slot, anum);
    ide_init_tlb();
    io_err_clear(slot, anum);

    msg_printf(INFO,"VMECC Dma engine test on IO4 slot=%d padap=%d\n", 
		slot, anum);
    msg_printf(INFO,"VMECC DMA ENGINE TEST NEEDS A CDSIO BOARD TO WORK !!!\n");

    vme_loc[0] = slot; vme_loc[1] = anum; vme_loc[2] = -1;

    if((jval =setjmp(vmebuf))){
	/* Anaylze the cause of error and display appropriate message */
	fail = 1;
	switch(jval){
	default:
	case 1: err_msg(VME_DMAERR, vme_loc); 
		show_fault();break;
	case 2: err_msg(VME_SWERR, vme_loc); break;
	}
	io_err_log(slot, anum);
	io_err_show(slot, anum);
    }
    else{
	set_nofault(vmebuf);

        if ( fail = cdsio_vmesetup(slot, anum))
	    return(fail);

        msg_printf(VRB, "VMECC DMA engine test using CDSIO Board.\n");

	setup_err_intr(slot, anum);
        clear_vme_intr(vmecc_addr);

        /*
         * if the CDSIO board is ready for a command, initiate a board reset 
         */
        refresh_timer = 0x0;

        while (!(*(volatile unsigned char *)status_port & CDSIO_READY) &&
		(refresh_timer++ < TIME_LIMIT))
		ms_delay(100);

        if (!(*(volatile unsigned char *)status_port & CDSIO_READY))
        {
	    msg_printf(ERR, "CDSIO board is busy.....Quitting\n");
        }
	else{
	    *(volatile unsigned char *)command_port = 0; /* reset the board */
	    ms_delay(1000); /* Big enough for the board to reset itself */
	    fail = dmaeng_op(1);
	}
    }

    clear_nofault();
    io_err_clear(slot, anum);
    clear_err_intr(slot, anum);
    ide_init_tlb();

    return(fail);

}

#define	DMA_MRDPAR	(0x280 | VME_A24SAMOD) /* EBUS -> VME transfer  */
#define DMA_MWRPAR	(0x080 | VME_A24SAMOD) /* VME  -> EBUS transfer */

#define	ADDR_MODIFIER	VME_A24SAMOD
#define	DMA_BLK_MODE	0x40
#define	DMA_MEM_READ	0x200
#define	DMA_MEM_WRITE	0
#define	DMA_BYTE_MODE	0
#define	DMA_HALF_MODE	0x80
#define	DMA_WORD_MODE	0x100
#define	DMA_DBL_MODE	0x180
/* Mode words for 
 *  - Read from mem as bytes and write to mem as bytes, 
 *  - Read from mem as bytes and write to mem as halfs,
 *  - Read from mem as halfs and write to mem as bytes 
 *  - Above three in block mode
 */

char *dmaop_mode[] = {
	"DMAEngine mem read bytes, VME write bytes",
	"DMAEngine mem read bytes, VME write halfs",
	"DMAEngine mem read halfs, VME write bytes",
	"DMAEngine mem read bytes, VME write bytes, block mode xfer",
	"DMAEngine mem read bytes, VME write halfs, block mode xfer",
	"DMAEngine mem read halfs, VME write bytes, block mode xfer"
};

uint dmaop_val[] = {
	(DMA_MEM_READ|DMA_BYTE_MODE|ADDR_MODIFIER), 
	(DMA_MEM_WRITE|DMA_BYTE_MODE|ADDR_MODIFIER),

	(DMA_MEM_READ|DMA_BYTE_MODE|ADDR_MODIFIER), 
	(DMA_MEM_WRITE|DMA_HALF_MODE|ADDR_MODIFIER),

	(DMA_MEM_READ|DMA_HALF_MODE|ADDR_MODIFIER),
	(DMA_MEM_WRITE|DMA_BYTE_MODE|ADDR_MODIFIER),

	0, 0, 
	/* Block mode DOESNOT work for CDSIO board */
	(DMA_MEM_READ|DMA_BLK_MODE|DMA_BYTE_MODE|ADDR_MODIFIER), 
	(DMA_MEM_WRITE|DMA_BLK_MODE|DMA_BYTE_MODE|ADDR_MODIFIER),

	(DMA_MEM_READ|DMA_BLK_MODE|DMA_BYTE_MODE|ADDR_MODIFIER), 
	(DMA_MEM_WRITE|DMA_BLK_MODE|DMA_HALF_MODE|ADDR_MODIFIER),

	(DMA_MEM_READ|DMA_BLK_MODE|DMA_HALF_MODE|ADDR_MODIFIER),
	(DMA_MEM_WRITE|DMA_BLK_MODE|DMA_BYTE_MODE|ADDR_MODIFIER),
	0, 0
};

/*
 * Transfer the data to cdsio board memory and read back from there.
 * Compare the two buffers for any difference in data.
 * 
 * This makes use of 1-level mapping of Ebus addresses.
 */
int 
dmaeng_op(int lpcnt)
{
#if _MIPS_SIM != _ABI64
    caddr_t	rbuf, wbuf, cach_wbuf;
    caddr_t	rptr, wptr;
    caddr_t     mapram;
    unsigned    value;
    unsigned	dmaop_indx=0;
    volatile    unsigned  timeout = TIME_LIMIT;

    /* Since first field of CDSIO port structure is output_buf, we can 
     * just use the cdsio_base_addr as the buffer address 
     */
    volatile    unchar *port_buffer = cdsio_base_addr;

#else
    paddr_t	rbuf, wbuf, cach_wbuf;
    paddr_t	rptr, wptr;
    paddr_t     mapram;
    paddr_t	value;
    unsigned	dmaop_indx=0;
    volatile    unsigned  timeout = TIME_LIMIT;

    /* Since first field of CDSIO port structure is output_buf, we can 
     * just use the cdsio_base_addr as the buffer address 
     */
    volatile    paddr_t port_buffer = cdsio_base_addr;

#endif

#if _MIPS_SIM != _ABI64
#ifdef	MALLOC
    if ((rbuf= malloc(VMEDATA)) == (caddr_t)-1){
	msg_printf(ERR,"malloc failed.. returning\n");
	return(1);
    }
#else 
    rbuf = vmelpbk_buf;
#endif
    wbuf = rbuf + DMASIZE;
    cach_wbuf  = (caddr_t)K1_TO_K0(wbuf);

    mapram = tlbentry(WINDOW(vmecc_addr), 0, 0);
#else
#ifdef	MALLOC
    if ((rbuf= (paddr_t)malloc(VMEDATA)) == (caddr_t)-1){
	msg_printf(ERR,"malloc failed.. returning\n");
	return(1);
    }
#else 
    rbuf = (paddr_t)&vmelpbk_buf[0];
#endif
    wbuf = rbuf + DMASIZE;
    cach_wbuf  = K1_TO_K0(wbuf);

    mapram = LWIN_PHYS(WINDOW(vmecc_addr), 0);
#endif

    msg_printf(DBG, "dmaeng_op: cdsio_addr=0x%x mapram addr=0x%x\n", 
		cdsio_base_addr, mapram);

    value = ((K0_TO_PHYS(rbuf) >> IAMAP_L1_BLOCKSHIFT) << 9);
    WR_REG(IAMAP_L1_ADDR(mapram,K0_TO_PHYS(rbuf)), value);

    /*
    *(unsigned *)(mapram + IAMAP_L1_ADDR(0,K0_TO_PHYS((unsigned)rbuf))) = value;
    */

    msg_printf(DBG,"mapram entry addr: 0x%x value: 0x%x\n", 
	IAMAP_L1_ADDR(mapram,K0_TO_PHYS(rbuf)), value);

    value = ((K0_TO_PHYS(wbuf) >> IAMAP_L1_BLOCKSHIFT) << 9);
    WR_REG(IAMAP_L1_ADDR(mapram,K0_TO_PHYS(wbuf)), value);
    /*
    *(unsigned *)(mapram + IAMAP_L1_ADDR(0,K0_TO_PHYS((unsigned)wbuf))) = value;
    */

    msg_printf(DBG,"mapram entry addr: 0x%x value: 0x%x\n", 
	IAMAP_L1_ADDR(mapram,K0_TO_PHYS(wbuf)), value);

    /* Initialize rbuf with some pattern */
    for (rptr=rbuf; rptr < wbuf; rptr++)
	*(char *)rptr = (char)rptr;
    do{
	msg_printf(DBG,"%s\n", dmaop_mode[dmaop_indx/2]);

	/* Clear VMEcc/Fchip error register */
	RD_REG(vmecc_addr+VMECC_ERRCAUSECLR);
	RD_REG(vmecc_addr+FCHIP_ERROR_CLEAR); 

        /* Initialize the DMA for Memory Read and I/O Write */
        WR_REG(vmecc_addr+VMECC_DMAEADDR, K0_TO_PHYS(rbuf));
	msg_printf(DBG,"\tVME_DMAEADDR: 0x%x, Address: 0x%x\n", 
		    vmecc_addr+VMECC_DMAEADDR,
		    RD_REG(vmecc_addr+VMECC_DMAEADDR));
        WR_REG(vmecc_addr+VMECC_DMAVADDR, (port_buffer - vme_base_addr));
	msg_printf(DBG,"\tVME_DMAVADDR: 0x%x, Address: 0x%x\n", 
		    vmecc_addr+VMECC_DMAVADDR,
		    RD_REG(vmecc_addr+VMECC_DMAVADDR));
        WR_REG(vmecc_addr+VMECC_DMABCNT, DMASIZE);
	msg_printf(DBG,"\tVME_DMABCNT: 0x%x, Count: 0x%x\n", 
		    vmecc_addr+VMECC_DMABCNT,
		    RD_REG(vmecc_addr+VMECC_DMABCNT));
        WR_REG(vmecc_addr+VMECC_DMAPARMS, (dmaop_val[dmaop_indx++] | DMA_BUSY));
	msg_printf(DBG,"\tVME_DMAPARMS: 0x%x, Parms: 0x%x\n", 
		    vmecc_addr+VMECC_DMAPARMS,
		    RD_REG(vmecc_addr+VMECC_DMAPARMS));
    
        /* Wait for DMA Completion 	*/
    
	timeout = TIME_LIMIT;
        while(((value=RD_REG(vmecc_addr+VMECC_DMAPARMS))&DMA_BUSY)&& --timeout)
    	    ms_delay(5);
	msg_printf (DBG,"DMA timeout left was %d of %d\n", timeout, TIME_LIMIT);
	msg_printf(DBG,"\tVME_DMAEADDR: 0x%x, Address: 0x%x\n", 
		    vmecc_addr+VMECC_DMAEADDR,
		    RD_REG(vmecc_addr+VMECC_DMAEADDR));
	msg_printf(DBG,"\tVME_DMAVADDR: 0x%x, Address: 0x%x\n", 
		    vmecc_addr+VMECC_DMAVADDR,
		    RD_REG(vmecc_addr+VMECC_DMAVADDR));
	msg_printf(DBG,"\tVME_DMABCNT: 0x%x, Count: 0x%x\n", 
		    vmecc_addr+VMECC_DMABCNT,
		    RD_REG(vmecc_addr+VMECC_DMABCNT));
       	msg_printf (DBG,"\tVMECC_DMAPARMS Value: 0x%x\n", value);
       	msg_printf (DBG,"\tVMECC_ERRCAUSE: 0x%x ERRAddr: 0x%x ErrXtra: 0x%x\n", 
		RD_REG(vmecc_addr+VMECC_ERRORCAUSES),
		RD_REG(vmecc_addr+VMECC_ERRADDRVME),
		RD_REG(vmecc_addr+VMECC_ERRXTRAVME));

        if (value & DMA_BUSY){
	    err_msg(VME_DMARD, vme_loc, value);
    	    free((void *)rbuf);
    	    longjmp(vmebuf, 2);
        }
	if (value & DMA_ERROR){
	    err_msg(VME_ENGERR, vme_loc, dmaop_mode[dmaop_indx/2]);
	    free((void *)rbuf);
	    longjmp(vmebuf, 2);
	}
	msg_printf(DBG,"Completed DMA transfer from Ebus addr 0x%x to VME \n",
			(unsigned)rbuf);
    
	/* Clear VMEcc/Fchip error register */
	RD_REG(vmecc_addr+VMECC_ERRCAUSECLR);
	RD_REG(vmecc_addr+FCHIP_ERROR_CLEAR); 

        /* Initialize DMA for VME space read transfer */
        WR_REG(vmecc_addr+VMECC_DMAEADDR, K0_TO_PHYS(wbuf));
	msg_printf(DBG,"\tVME_DMAEADDR: 0x%x, Address: 0x%x\n", 
		    vmecc_addr+VMECC_DMAEADDR,
		    RD_REG(vmecc_addr+VMECC_DMAEADDR));
        WR_REG(vmecc_addr+VMECC_DMAVADDR, (port_buffer - vme_base_addr));
	msg_printf(DBG,"\tVME_DMAVADDR: 0x%x, Address: 0x%x\n", 
		    vmecc_addr+VMECC_DMAVADDR,
		    RD_REG(vmecc_addr+VMECC_DMAVADDR));
        WR_REG(vmecc_addr+VMECC_DMABCNT, DMASIZE);
	msg_printf(DBG,"\tVME_DMABCNT: 0x%x, Count: 0x%x\n", 
		    vmecc_addr+VMECC_DMABCNT,
		    RD_REG(vmecc_addr+VMECC_DMABCNT));
        WR_REG(vmecc_addr+VMECC_DMAPARMS, (dmaop_val[dmaop_indx++] | DMA_BUSY));
	msg_printf(DBG,"\tVME_DMAPARMS: 0x%x, Parms: 0x%x\n", 
		    vmecc_addr+VMECC_DMAPARMS,
		    RD_REG(vmecc_addr+VMECC_DMAPARMS));
    
        /* Wait for DMA Completion 	*/
    
	timeout = TIME_LIMIT;
        while(((value=RD_REG(vmecc_addr+VMECC_DMAPARMS))&DMA_BUSY)&& --timeout)
    	    ms_delay(5);
	msg_printf (DBG,"DMA timeout left was %d of %d\n", timeout, TIME_LIMIT);
	msg_printf(DBG,"\tVME_DMAEADDR: 0x%x, Address: 0x%x\n", 
		    vmecc_addr+VMECC_DMAEADDR,
		    RD_REG(vmecc_addr+VMECC_DMAEADDR));
	msg_printf(DBG,"\tVME_DMAVADDR: 0x%x, Address: 0x%x\n", 
		    vmecc_addr+VMECC_DMAVADDR,
		    RD_REG(vmecc_addr+VMECC_DMAVADDR));
	msg_printf(DBG,"\tVME_DMABCNT: 0x%x, Count: 0x%x\n", 
		    vmecc_addr+VMECC_DMABCNT,
		    RD_REG(vmecc_addr+VMECC_DMABCNT));
       	msg_printf (DBG,"\tVMECC_DMAPARMS Value: 0x%x\n", value);
       	msg_printf (DBG,"\tVMECC_ERRCAUSE: 0x%x ERRAddr: 0x%x ErrXtra: 0x%x\n", 
		RD_REG(vmecc_addr+VMECC_ERRORCAUSES),
		RD_REG(vmecc_addr+VMECC_ERRADDRVME),
		RD_REG(vmecc_addr+VMECC_ERRXTRAVME));

        if (value & DMA_BUSY){
    	    err_msg(VME_DMAWR, vme_loc, value); 
    	    free((void *)rbuf);
    	    longjmp(vmebuf, 2);
        }
	if (value & DMA_ERROR){
	    err_msg(VME_ENGERR, vme_loc, dmaop_mode[dmaop_indx/2]);
	    free((void *)rbuf);
	    longjmp(vmebuf, 2);
	}

	msg_printf(DBG,"Completed DMA transfer from VME to Ebus addr 0x%x \n", 
			(unsigned)wbuf);

	flush_iocache(vme_loc[0]);

	/* Compare the data */
	for (rptr=rbuf, wptr=wbuf; rptr < wbuf; rptr++, wptr++){
	    if (*(char *)rptr != *(char *)wptr){
	        err_msg(VME_DMAXFER, vme_loc, wptr,
			*(char *)rptr, *(char *)wptr); 
		longjmp(vmebuf, 2);
	    }
	}
	bzero((void *)wbuf, DMASIZE);

    }while(dmaop_val[dmaop_indx] != 0);

#if _MIPS_SIM != _ABI64
    tlbrmentry(mapram);
#endif
    free((void *)rbuf);

    return(0);
}
