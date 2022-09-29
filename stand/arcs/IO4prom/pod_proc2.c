/***********************************************************************\
*	File:		pod_proc2.c					*
*									*
*       Contains the code run on the second processor in 2 cpu/2f/2vme  *
*	configuration. 							*
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_proc2.c $Revision: 1.3 $"

#include "pod_fvdefs.h"

unsigned proc2_data[SLAVE_DATA];

unsigned p2Dma_rd[SLAVE_DATA], p2Dma_wr[SLAVE_DATA];

int p2Dma_state;

volatile unsigned Proc2_start=0;

extern int	svmecc_piolpbk(unsigned, int, int);

caddr_t	Mapram2=0;


int
proc2_op(int window, int anum)
{
    /* Define the operations to be carried out by the slave processor
     * Planning to use this facility to stress the VMECC.
     */
    __psunsigned_t chip_base, i;

    pod_initialize();

    /* Wait till you are asked to start	*/
    while(Proc2_start != VMECC_SETUP);

    print_hex(0xfed);
    chip_base = SWIN_BASE(window, anum);

    p2Dma_state = 0;

    if ((Mapram2 == 0) && ((Mapram2 = tlbentry(window, 0, 0)) == (caddr_t)0)){
	Return(0xbeefed);
    }

    fchip_init(window, anum);

    WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE);
    setup_lpbk(chip_base, window, anum, Mapram2);
    p2vmecc_lpbk(chip_base, window, anum);
    p2vmecc_lpbk(chip_base, window, anum); /* Do it again possibly */
    p2vmecc_lpbk(chip_base, window, anum); /* Do it again possibly */

    tlbrmentry(Mapram2); Mapram2 = 0;
   
    
}

int
p2vmecc_lpbk(__psunsigned_t chip_base, int window, int ioa_num)
{

    unsigned 	value, error=0, slave_offset;
    caddr_t	vaddr;
    volatile unsigned *vvaddr;
    jmp_buf	jbuf, *obuf;
    int		i;


#ifdef	DEBUG_POD
    print_hex(0xbece);
#endif

    vaddr = tlbentry(window, ioa_num, 0x02000000); 
    slave_offset = (K0_TO_PHYS(proc2_data) & (LW_VPAGESIZE - 1));


    /* Slave would just do a lot of 1 level rd/write to A32 space */

    /* Pump the writes first 	*/
    vvaddr = (unsigned *) (vaddr + slave_offset);
    for (i=0; i < SLAVE_DATA; i++, vvaddr++ ){
	*(vvaddr) = (unsigned) (&proc2_data[i]);

	if ((i % 8) == 0)
            vmep2_sl_dmaop(chip_base);
    }

    vvaddr = (unsigned *) (vaddr + slave_offset);

    for (i=0; i < 4 ; i++, vvaddr++){
	value = *(vvaddr );
#ifdef	DEBUG_POD
        print_hex(value);
#endif
	if (value != (unsigned)(&proc2_data[i])){
	    error++;
	}
        vmep2_sl_dmaop(chip_base);
    }


    tlbrmentry(vaddr);

    Return (error);

}

/* DMA initalization and management for a second processor */
int
vmep2_sl_dmaop(__psunsigned_t chip_base)
{
    /* Do a DMA Write to the Slave Memory, Poll for it completion
     * and then, do a DMA read from the same address, compare the 
     * data
     */

    unsigned status;

again:
    switch(p2Dma_state){

    case 0 : vmecc_sl_dmawr(chip_base, p2Dma_rd, Mapram2); 
	     p2Dma_state++; break;

    case 1 : 
    case 3 : 
	    /* Check if DMA has completed */
	    status = RD_REG(chip_base+VMECC_DMAPARMS);

	    if (status & (unsigned)0x80000000)
		break;	/* Not over yet */
		
	    if (status & (unsigned)0x60000000){
		p2Dma_state = DMA_DONE; 
	        print_hex(status);
		break;  /* Error Case */
	    }
	    p2Dma_state++;
	    goto again; /* Why not take further action here ?? */
    
    case 2 : vmecc_sl_dmard(chip_base, p2Dma_wr, Mapram2); 
	     p2Dma_state++; break;

	    /* Successfull state */
    case 4 : print_hex(0xeeeeeeee); 
	     p2Dma_state = 0; goto again; /* Do it again */
	     /* p2Dma_state = DMA_DONE; break; */

    case DMA_DONE : break; 

    default: print_hex((0xaaaaaa<<4) | p2Dma_state); break;
    }

    Return(0xcac0 | p2Dma_state);
}
