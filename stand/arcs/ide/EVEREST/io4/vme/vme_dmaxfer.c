/*
 * io/vme_dmaxfer.c
 *
 *
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

#ident "arcs/ide/IP19/io/io4/vme/vme_dmaxfer.c $Revision: 1.9 $"

/*
 * vme_dmaxfer     
 *
 * Transfer data between controller memory & host memory by DMA; no disk
 * data is involved (and no disk need be connected). 
 * The controller imposes some constraints on what can be done with this test: 
 * it will transfer only a single sector (512 bytes).
 */

#include <sys/cpu.h>
#include "vmedefs.h"
#include <sys/vmereg.h>
#include <prototypes.h>
#include <libsc.h>

#define	LEV2MAP_SIZE	128
#define	BLOCKSIZE	512


int vmea32_dma(unsigned, unsigned, unsigned, unsigned);
int vmea24_dma(unsigned, unsigned, unsigned, unsigned);
int vme_initdma(unsigned, unsigned, unsigned, unsigned, unsigned);
int vme_dodma(int, int);

#ifndef	MALLOC
extern char vmelpbk_buf[];
#endif

vme_dmaxfer(int argc, char **argv)
{
    int fail = 0;
    int slot, anum;

    /* Parse the Parameters and evaluate */


    if (setjmp(vmebuf)){
	/* Error Processing */
    }
    else{

	nofault = vmebuf;

	if (vme_dodma(slot, anum))
	    fail = 1;
    }

    nofault = 0;

    return(fail);
}

vme_dodma(int slot, int anum)
{

#if _MIPS_SIM != _ABI64
    int         io4win;
    unsigned    lev2map, rbuffer, wbuffer;
    unsigned	i,fail = 1;
    caddr_t     mapram;
    caddr_t	memory;
#else
    int         io4win;
    unsigned    lev2map, rbuffer, wbuffer;
    unsigned	i,fail = 1;
    paddr_t     mapram;
    caddr_t	memory;
#endif

#ifdef	MALLOC
    if ((memory = malloc(VMEDATA)) == (char *)0)
        return(1);
#else
    memory = vmelpbk_buf;
#endif

    lev2map = (unsigned)(memory + 0x800) & ~0x7ff;

    wbuffer = lev2map + LEV2MAP_SIZE;
    /* Keep rbuffer about 2k away from wbuffer. This results in a separate
     * entry in the lev2map.
     */
    rbuffer = wbuffer + BLOCKSIZE ; 

    for (i=0; i < LEV2MAP_SIZE; i++)
        *(unsigned *)(lev2map + i) = 0;

    if (((io4win     = io4_window(slot)) < 0) ||
        ((vmecc_addr = get_vmebase(slot, anum)) == 0) ||
#if _MIPS_SIM != _ABI64
        ((mapram     = tlbentry(io4win, 0, 0)) == (caddr_t)0))
#else
        ((mapram     = LWIN_PHYS(io4win, 0)) == (paddr_t)0))
#endif
	goto relmem;

    /* Make 2 level map entries for rbuffer and wbuffer */

    i = (K0_TO_PHYS(wbuffer) >> IAMAP_L2_BLOCKSHIFT) & 0x1ff ;
    *(unsigned *)(lev2map+i) = (K0_TO_PHYS(wbuffer) >> IAMAP_L2_BLOCKSHIFT);

    i = (K0_TO_PHYS(rbuffer) >> IAMAP_L2_BLOCKSHIFT) & 0x1ff ;
    *(unsigned *)(lev2map + i) = (K0_TO_PHYS(rbuffer) >> IAMAP_L2_BLOCKSHIFT);


    if (((vmea32_dma(rbuffer, wbuffer, lev2map, (unsigned)mapram )) == 0) &&
        ((vmea24_dma(rbuffer, wbuffer, lev2map, (unsigned)mapram )) == 0))
	    fail = 0;

relmem:
    free(memory);

    if (vmecc_addr == 0)
	return (TEST_SKIPPED);
    else
	return(fail);
}


#define	NO_SLAVES	0xfff

vmea32_dma(unsigned rbuf, unsigned wbuf, unsigned l2map, unsigned mapram)
{
    int         i, j, fail;
    unsigned 	value, wrentry, rdentry, iowaddr, ioraddr;
    unsigned    lev2map;

    for (i = 0; i < 16; i++){
	/* Enable section 'i' of vmecc to respond as slave. */
	WR_REG(vmecc_addr+VMECC_CONFIG, 
	       (VMECC_CONFIG_VALUE & NO_SLAVES) | (1 << (i + 16)));

	value   = (K0_TO_PHYS(lev2map) >> IAMAP_L1_ADDRSHIFT) | IAMAP_L2_FLAG;

	/* Values in 'j' tries to run through bit combinations in a27-a21 */
	for (j=0x7f; j > 0; j-=8){

	    iowaddr = (i << 28) | (j << IAMAP_L1_ADDRSHIFT) | 
		      (K0_TO_PHYS(wbuf) & (IAMAP_L1_BLOCKSIZE - 1));
	    ioraddr = (i << 28) | (j << IAMAP_L1_ADDRSHIFT) | 
		      (K0_TO_PHYS(rbuf) & (IAMAP_L1_BLOCKSIZE - 1));

	    wrentry = *((unsigned *)IAMAP_L1_ADDR(mapram, iowaddr));
	    rdentry = *((unsigned *)IAMAP_L1_ADDR(mapram, ioraddr));

	    *(unsigned *)(IAMAP_L1_ADDR(mapram, iowaddr)) = value; 
	    *(unsigned *)(IAMAP_L1_ADDR(mapram, ioraddr)) = value; 

	    fail = vme_initdma(rbuf, wbuf, ioraddr, iowaddr, VME_A32NPBAMOD);

	    *(unsigned *)(IAMAP_L1_ADDR(mapram, iowaddr)) = wrentry;
	    *(unsigned *)(IAMAP_L1_ADDR(mapram, ioraddr)) = rdentry;

	    if (fail)
	        return(fail);
	}
    }

    WR_REG(vmecc_addr+VMECC_CONFIG, VMECC_CONFIG_VALUE);
    return(0);

}

vmea24_dma(unsigned rbuf, unsigned wbuf, unsigned l2map, unsigned mapram)
{

    int         i, fail;
    unsigned 	value, wrentry, rdentry, iowaddr, ioraddr;

    for (i = 0; i < 4; i++){
	/* Enable section 'i' of vmecc to respond as slave. */
	WR_REG(vmecc_addr+VMECC_CONFIG, 
	       (VMECC_CONFIG_VALUE & NO_SLAVES) | (1 << (i + 12)));

	value   = (K0_TO_PHYS(l2map) >> IAMAP_L1_ADDRSHIFT) | IAMAP_L2_FLAG;

	iowaddr = (i << 22) |(K0_TO_PHYS(wbuf) & (IAMAP_L1_BLOCKSIZE - 1));
	ioraddr = (i << 22) |(K0_TO_PHYS(rbuf) & (IAMAP_L1_BLOCKSIZE - 1));

	/* In A24 Mode, VMECC makes bits 24-31 as Ones */
	wrentry = *((unsigned *)IAMAP_L1_ADDR(mapram, (0xff000000|iowaddr)));
	rdentry = *((unsigned *)IAMAP_L1_ADDR(mapram, (0xff000000|ioraddr)));

	*(unsigned *)(IAMAP_L1_ADDR(mapram, (0xff000000|iowaddr))) = value; 
	*(unsigned *)(IAMAP_L1_ADDR(mapram, (0xff000000|ioraddr))) = value; 

	fail = vme_initdma(rbuf, wbuf, ioraddr, iowaddr, VME_A24NPBAMOD);

	*(unsigned *)(IAMAP_L1_ADDR(mapram, iowaddr)) = wrentry;
	*(unsigned *)(IAMAP_L1_ADDR(mapram, ioraddr)) = rdentry;

	if (fail)
	    return(fail);
    }

    WR_REG(vmecc_addr+VMECC_CONFIG, VMECC_CONFIG_VALUE);
    return(0);
}

int
vme_initdma(unsigned rbuffer, unsigned wbuffer, unsigned readaddr, 
	  unsigned writeaddr, unsigned addrmod)
{
    register unsigned ptr, ptr2;
    register unsigned data;
    register unsigned udata;
    int ctrl;
    int fail = 0;
    
    /* zero out buffers */
    bzero((caddr_t)wbuffer,VMEDATA);

    /* Initialize buffer */

    data = 0;
    udata = 0xffff;
    for (ptr = wbuffer; ptr < (wbuffer + (BLOCKSIZE >> 2) );
        ptr++, data += 4, udata -= 4) 
        *(unsigned *)ptr = (data << 16) | (udata & 0xffff);

    /* write to controller's memory */

    if (_dkipdmatst(0, writeaddr, 1, addrmod)) {
        msg_printf(ERR, "No dkip disk controller 0\n");
        return(1);
    }

    if (_dkipdmatst(0, readaddr, 0, addrmod)) {
        msg_printf(ERR, "No dkip disk controller 0\n");
        return(1);
    }

    /* verify data */
    for (ptr = wbuffer, ptr2 = rbuffer; 
	 ptr < wbuffer + (BLOCKSIZE >> 2);
         ptr++, ptr2++)
    {
        if ( *(unsigned *)ptr != *(unsigned *)ptr2){
            msg_printf(ERR, "Data error, expected 0x%x , actual 0x%x\n",
                  *(unsigned *)ptr, *(unsigned *)ptr2);
            msg_printf(ERR, "buffer address = 0x%x\n ", ptr2);
            fail = 1;
            return(fail);
        }
    }
    return(fail);
}
