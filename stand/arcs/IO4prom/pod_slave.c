/***********************************************************************\
*	File:		pod_slave.c					*
*									*
*       Contains code to be run on the slave processor present on the   *
*       Everest bus. The slave processor operates on the same set of    *
*	F-VMECC as the master processor.                                *
*									*
*	At present, this is duplicating some of the work done by the    *
*       pod_proc2.c. At some point of time both of them will converge   *
*       But this may not go into the pod_prom as a permanent feature.   *
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_slave.c $Revision: 1.4 $"

#include "pod_fvdefs.h"
/*
 * Assumes that the following variables are set.
 * IO4_slotno
 * Ioa_num 	
 * Window
 */

unsigned slave_data[SLAVE_DATA];
volatile unsigned Start_slave=0;

extern int	svmecc_piolpbk(unsigned, int, int);


int
slave_op()
{
    /* Define the operations to be carried out by the slave processor
     * Planning to use this facility to stress the VMECC.
     */
    __psunsigned_t chip_base;

    /* Wait till you are asked to start	*/

    WR_REG(EV_IGRMASK, DIAG_GRP_MASK); /* Our Group Mask */
    init_tlb();
    while (Start_slave != VMECC_SETUP);

    chip_base = SWIN_BASE(Window, Ioa_num);
    print_hex(0xcafade);
    svmecc_piolpbk(chip_base, Window, Ioa_num);
    svmecc_piolpbk(chip_base, Window, Ioa_num); /* Do it again possibly */
    svmecc_piolpbk(chip_base, Window, Ioa_num); /* Do it again possibly */
   
    
}

int
svmecc_piolpbk(unsigned chip_base, int window, int ioa_num)
{

    unsigned 	value, error=0, slave_offset;
    caddr_t	vaddr;
    volatile unsigned *vvaddr;
    jmp_buf	jbuf, *obuf;
    int		i;


#ifdef	DEBUG_POD
    print_hex(0xbeda);
#endif

    vaddr = tlbentry(window, ioa_num, 0x02000000); 
    slave_offset = (K0_TO_PHYS(slave_data) & (LW_VPAGESIZE - 1));

    /* Slave would just do a lot of 1 level rd/write to A32 space */

    /* Pump the writes first 	*/
    vvaddr = (unsigned *) (vaddr + slave_offset);
    for (i=0; i < SLAVE_DATA; i++, vvaddr++ )
	*(vvaddr) = (unsigned) (&slave_data[i]);

    vvaddr = (unsigned *) (vaddr + slave_offset);

    for (i=0; i < 4 ; i++, vvaddr++){
	value = *(vvaddr );
#ifdef	DEBUG_POD
        print_hex(value);
#endif
	if (value != (unsigned)(&slave_data[i])){
	    error++;
	}
    }


    tlbrmentry(vaddr);

    Return (error);

}
