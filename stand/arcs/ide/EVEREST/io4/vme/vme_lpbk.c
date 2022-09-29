/*
#endif
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

/***********************************************************************\
*	File:		vme_lpbk.c					*
*									*
*	Test the VMECC loop back mode of operation. This would test	*
*	the data path between the CPU and VME bus.			*
*									*
\***********************************************************************/

#include "vmedefs.h"
#include <prototypes.h>
#include <libsk.h>

#ident "arcs/ide/IP19/io/io4/vme/vme_lpbk.c $Revision: 1.20 $"


/* 
 * This file would have the following tests.
 * 1. DO a pio loopback in byte/half-word/word access modes.
 * 2. Flush Fchip TLB, Do a PIO Access resulting in single-level map,
 *    and check if any IOTLB entries get updated.
 * 3. Flush Fchip TLB. Do a pio loopback resulting in 2 level PIO Access, and 
 *    check which entry gets updated.
 * 4. Reduce the EBUS_TIMEOUT value on IA/ID to a small value. Do a PIO access,
 *    and check the error bits which get set due to timeout.
 */
#define	LEV2_MAP	512	/* Caters for 512*4k space access	*/
#define LEV2_MAP_STEP	(LEV2_MAP * sizeof(paddr_t))
#define	VMES_SIZE	1024	/* Size of Write/read unsigned array  	*/

#if _MIPS_SIM != _ABI64
int vme_a32lpbk(unsigned);
int vme_a24lpbk(unsigned);
int vmecc_ftlbchk(unsigned);
int access_memory(caddr_t, caddr_t);
#else
int vme_a32lpbk(paddr_t);
int vme_a24lpbk(paddr_t);
int vmecc_ftlbchk(paddr_t);
int access_memory(paddr_t, paddr_t);
#endif

#define	USAGE	"%s [io4slot anum]\n"

/* 
 * MALLOC is undefined. So we end up defining vmelpbk_buf. 
 * Once libsk/wd95.c is fixed to release the kmem_alloced buffer in wd95alloc
 * we can define MALLOC and start using mallocced buffer
 */
#ifndef	MALLOC
char	vmelpbk_buf[(VMEDATA << 1)];
#endif

static int vme_loc[3];
static int Slot, Anum;
extern int io4_tslot, io4_tadap;

int
vme_lpbk(int argc, char **argv)
{
    int 	result=0;
    int		found=0;

    if (argc > 1){
	if (io4_select(1, argc, argv)){
	    msg_printf(ERR, USAGE, argv[0]);
	    return(1);
	}
    }
    /* Serves the purpose of checking of Slot, Anum have the needed HW info */
    while(!result && io4_search(IO4_ADAP_FCHIP, &Slot, &Anum)){

	if ((argc > 1) && ((Slot != io4_tslot) || (Anum != io4_tadap)))
	    continue;

	if (fmaster(Slot, Anum) != IO4_ADAP_VMECC)
	    continue;

	found=1;

	result |= vmelpbk_test();
    }
    if (!found) {
	if (argc > 1) {
	    msg_printf(ERR, "Invalid Slot/Adapter number\n");
	    msg_printf(ERR, USAGE, argv[0]);
	}
	result = TEST_SKIPPED;
    }
    if (result)
        io4_search(0, &Slot, &Anum);

    return(result);

}
vmelpbk_test()
{
    int         fail=0;
#ifndef TFP
    unsigned	oldSR, chip_addr;
#else
    __psunsigned_t	oldSR;
    paddr_t		chip_addr;
#endif

    oldSR = GetSR();
    SetSR(oldSR & ~SR_IMASK);

    msg_printf(INFO, "VME PIO Loopback mode tests on IO4slot %d  adap %d\n",
		Slot, Anum);


    if ((chip_addr = get_vmebase(Slot, Anum)) == 0){
	return (TEST_SKIPPED);
    }

    setup_err_intr(Slot, Anum);
    ide_init_tlb();
    io_err_clear(Slot, (1 << Anum));

    vme_loc[0] = Slot; vme_loc[1] = Anum; vme_loc[2] = -1;
    /* Enable the two bits needed in A24 operation. */
    /* used to be 0x6000 - partitions 1 & 2 enabled.  now is
       0xC000 - partitions 2 & 3.  Move due to change from malloc
       to local buffer - the locations map differently */
    EV_SET_REG(chip_addr+VMECC_CONFIG, VMECC_CONFIG_VALUE| 0xC000);
    if(vmecc_ftlbchk(chip_addr) || vme_a32lpbk(chip_addr) || 
       vme_a24lpbk(chip_addr))
	    fail = 1;


    io_err_clear(Slot, (1 << Anum));
    clear_err_intr(Slot, Anum);
    /* ide_init_tlb(); */
    SetSR(oldSR);

    if (fail)
	msg_printf(INFO,"FAILED VME PIO Loopback mode test.\n");
    else
	msg_printf(INFO, "Passed VME PIO Loopback mode test.\n");
    return(fail);

}
/*
 * 0  Within the data_area calculate where the 2k boundary is 
 *    and start the level 2 map there. Define the vmecc area to
 *    be the other portion of data_area
 * 1. Make Entries in the MAPRAM at appropriate indexes to do the 
 *    mapping of requests from VMECC.
 * 2. Update the lev2_map table in the memory to point to the 
 *    Read/Write Area.
 *
 */

#if _MIPS_SIM != _ABI64
setup_lev2map(caddr_t mem, unsigned **l2map, unsigned **lpbk_ptr )
#else
setup_lev2map(paddr_t mem, paddr_t l2map, paddr_t lpbk_ptr )
#endif
{
    /* Assume 'mem' has sufficient memory to have a 2K boundary
     * somewhere. Within this area, setup the lpbk_ptr, and l2map
     * data pointers 
     * Initialize the Level 2 Map table.
     */
#if _MIPS_SIM != _ABI64
    unsigned phys_addr, i, size;
#else
    paddr_t phys_addr;
    unsigned i, size;
#endif

    /* Level 2 map table should start at a 2k boundary  */
    *(unsigned **)l2map = (unsigned *)((unsigned)(mem + 0x800) & ~(0x7ff));

    *(unsigned **)lpbk_ptr = *(unsigned **)l2map + LEV2_MAP;
    if ((unsigned)*(unsigned **)lpbk_ptr & 0x800)
        *(unsigned **)lpbk_ptr += LEV2_MAP;  /* Get to a page boundary */

    msg_printf(DBG,"setup_lev2map: l2map=0x%x lpbkmemory: 0x%x\n",
        *(unsigned **)l2map, *(unsigned **)lpbk_ptr);

    phys_addr = K0_TO_PHYS(*(unsigned **)lpbk_ptr);

    for (i=0; i < LEV2_MAP; i++)
        *(*(unsigned**)l2map + i) = phys_addr >> IAMAP_L2_BLOCKSHIFT;
}

/*
 * For the given address, and level of mapping, make an entry in
 * the Mapram.
 */
#if _MIPS_SIM != _ABI64
mapram_ent(int lvl, caddr_t map, unsigned addr, unsigned *memp)
#else
mapram_ent(int lvl, paddr_t map, paddr_t addr, paddr_t memp)
#endif
{
#if _MIPS_SIM != _ABI64
    unsigned phys_addr, offset;

    phys_addr = K0_TO_PHYS(memp);
#else
    paddr_t offset;
    paddr_t phys_addr;

    phys_addr = K0_TO_PHYS(memp);
#endif


    if (lvl == 2)
	offset = (IAMAP_L2_ENTRY(phys_addr));
    else
	offset = ((phys_addr >> IAMAP_L1_BLOCKSHIFT) << 9);

    /* changed calculation from incorrect addr|phys_addr to addr+phys_addr */
    msg_printf(DBG,"Mapram entry 0x%x, value = 0x%x mem_addr: 0x%x\n", 
		IAMAP_L1_ADDR(map, (addr+phys_addr)), offset, memp);

    EV_SET_REG((IAMAP_L1_ADDR(map, (addr+phys_addr))), offset);
    /* *(__uint32_t *) (IAMAP_L1_ADDR(map, (addr|phys_addr))) = offset; */
}

#if _MIPS_SIM != _ABI64
vme_a32lpbk(unsigned chip_base)
#else
vme_a32lpbk(paddr_t chip_base)
#endif
{
    /* Access the memory area via the A32 loopback mode in both 
&    * 1 level and 2 level mapping.
     & Presume that 'memory' has atleast
     & LEV2_MAP*4 + VMES_SIZE*4+ gap needed to align lev2_map in 
     & 2k boundary.
     */

#if _MIPS_SIM != _ABI64
    unsigned   paddr, level;
    unsigned   *l2map, *vme_slave, *memory;
    caddr_t    vaddr, mapram;
#else
    paddr_t paddr;
    unsigned level;
    paddr_t l2map, vme_slave, memory;
    paddr_t    vaddr, mapram;
#endif
    int        piotest = 0;

    msg_printf(INFO,"Checking VMECC A32 PIO Loopback Mode\n");

#if _MIPS_SIM != _ABI64
#ifdef	MALLOC
    if ((memory = (paddr_t)malloc(VMEDATA)) == (paddr_t)0)
	return(1);
#else
    memory = (unsigned *)vmelpbk_buf;
#endif
    if ((mapram=tlbentry(WINDOW(chip_base), 0, 0)) == 0)
	return(1);

    if ((vaddr = tlbentry(WINDOW(chip_base), ANUM(chip_base), 0)) == (paddr_t)0)
	return(1);
#else
#ifdef	MALLOC
    if ((memory = (paddr_t)malloc(VMEDATA)) == (paddr_t)0)
	return(1);
#else
    memory = (paddr_t)K0_TO_K1(&vmelpbk_buf);
#endif
    mapram=LWIN_PHYS(WINDOW(chip_base), 0);
    vaddr = LWIN_PHYS(WINDOW(chip_base), ANUM(chip_base));
#endif

#if _MIPS_SIM != _ABI64
    setup_lev2map(memory, &l2map, &vme_slave);
#else
    setup_lev2map(memory, (paddr_t)&l2map, (paddr_t)&vme_slave);
#endif
    msg_printf(DBG,"memory:0x%x l2map:0x%x vme_slave: 0x%x\n", 
		memory, l2map, vme_slave);

    if ((level = setjmp(vmebuf))){
	switch(level){
	default:
	case 1:
	    err_msg(((piotest == 0)?VME_A32L1:VME_A32L2), vme_loc); 
	    show_fault();
	    break;
	case 2: err_msg(VME_SWERR, vme_loc); break;
	}
	io_err_log(Slot, (1 << Anum));
	io_err_show(Slot, (1 << Anum));
	clear_nofault();
	piotest = FAILURE;
    }
    else{
	set_nofault(vmebuf);

	msg_printf(VRB, "VME A32 PIO Loopback mode - 1 level mapping\n" ); 

        mapram_ent(1, mapram, A32_ADDR, vme_slave);

#if _MIPS_SIM != _ABI64
	paddr = K0_TO_PHYS(vme_slave);
#else
	paddr = K1_TO_PHYS(vme_slave);
        WR_REG(chip_base+VMECC_A64MASTER, (vme_slave >> 32));
        WR_REG(chip_base+VMECC_A64SLVMATCH, (vme_slave >> 32));
#endif
        WR_REG(chip_base+VMECC_PIOREG(1),
	    ((A32_NPAM << VMECC_PIOREG_AMSHIFT) |
	    ((A32_ADDR|paddr) >> 23)));
    
        paddr &= (VMECC_PIOREG_MAPSIZE - 1);
        access_memory((vaddr+VMECC_PIOREG_MAPSIZE+paddr), vme_slave);

	msg_printf(VRB,"Passed VME A32 PIO loopback mode - 1 level mapping \n");

	piotest++;
	msg_printf(VRB, "VME A32 PIO loopback mode - 2 level map\n");

        mapram_ent(2, mapram, L2MAP_A32, 0);
        mapram_ent(2, mapram, L2MAP_A32, l2map);

#if _MIPS_SIM != _ABI64
	paddr = K0_TO_PHYS(vme_slave);
#else
	paddr = K1_TO_PHYS(vme_slave);
        WR_REG(chip_base+VMECC_A64MASTER, (vme_slave >> 32));
        WR_REG(chip_base+VMECC_A64SLVMATCH, (vme_slave >> 32));
#endif
        WR_REG(chip_base+VMECC_PIOREG(1),
	    ((A32_NPAM << VMECC_PIOREG_AMSHIFT) | ((L2MAP_A32|paddr) >> 23)));
    
	/*
        paddr = K0_TO_PHYS(vme_slave) & 0xfff;
        paddr |= ((K0_TO_PHYS(l2map) & 0x7fc) << IAMAP_L2_BLOCKSHIFT);
	*/
        paddr &= (VMECC_PIOREG_MAPSIZE - 1);
        access_memory((vaddr+VMECC_PIOREG_MAPSIZE+paddr), vme_slave);

	msg_printf(VRB,"Passed VME A32 PIO loopback mode - 2 level mapping.\n");

	piotest = 0;
    }
    


#if _MIPS_SIM != _ABI64
    tlbrmentry(vaddr);
    tlbrmentry(mapram);
#endif

#ifdef MALLOC
    free((paddr_t)memory);
#endif

    if (piotest)
        msg_printf(INFO,"FAILED Checking VMECC A32 PIO Loopback Mode\n");
    else
        msg_printf(INFO,"Passed Checking VMECC A32 PIO Loopback Mode\n");
    return(piotest);

}

#if _MIPS_SIM != _ABI64
#define	A24_MASK	(unsigned)0xff000000
vme_a24lpbk(unsigned chip_base)
#else
#define	A24_MASK	(__uint32_t)0xff000000
vme_a24lpbk(paddr_t chip_base)
#endif
{
    /* Access the memory area via the A24 loopback mode in both 
     * 1 level and 2 level mapping.
     * Presume that 'memory' has atleat
     * LEV2_MAP*4 + VMES_SIZE*4+ gap needed to align lev2_map in 
     * 2k boundary.
     */

#if _MIPS_SIM != _ABI64
    unsigned  paddr, level;
    unsigned  *l2map, *vme_slave, *memory;
    caddr_t  vaddr, mapram ;
#else
    paddr_t paddr, level;
    paddr_t l2map, vme_slave, memory;
    paddr_t  vaddr, mapram ;
#endif
    int      piotest=0;

    msg_printf(INFO,"Checking VMECC A24 PIO Loopback mode\n");

#if _MIPS_SIM != _ABI64
#ifdef	MALLOC
    if ((memory = (unsigned *)malloc(VMEDATA)) == (unsigned *)0)
	return(1);
#else
    memory = (unsigned *)vmelpbk_buf;
#endif
    if ((mapram = tlbentry(WINDOW(chip_base), 0, 0)) == 0)
	return(1);

    if ((vaddr = tlbentry(WINDOW(chip_base), ANUM(chip_base), 0)) == (caddr_t)0)
	return(1);
#else
#ifdef	MALLOC
    if ((memory = (paddr_t)malloc(VMEDATA)) == (paddr_t)0)
	return(1);
#else
    memory = (paddr_t)K0_TO_K1(&vmelpbk_buf);
#endif
    mapram = LWIN_PHYS(WINDOW(chip_base), 0);
    vaddr = LWIN_PHYS(WINDOW(chip_base), ANUM(chip_base));
#endif
    
#if _MIPS_SIM != _ABI64
    setup_lev2map((caddr_t)memory, &l2map, &vme_slave);
#else
    setup_lev2map(memory, (paddr_t)&l2map, (paddr_t)&vme_slave);
#endif

    if (level = setjmp(vmebuf)){
	switch(level){
	default:
	case 1: msg_printf(ERR,"Got exception in VMECC PIO Loopback test\n");
		break;
	case 2: err_msg((piotest ? VME_A24L2 : VME_A24L1), vme_loc);
		break;
	}
	io_err_log(Slot, (1 << Anum));
	io_err_show(Slot, (1 << Anum));

	clear_nofault();
    }
    else{
	set_nofault(vmebuf);
	msg_printf(VRB, "VME A24 PIO Loopback test - 1 level mapping.\n" ); 
        mapram_ent(1, mapram, (A24_MASK| 0x800000), vme_slave);

        WR_REG(chip_base+VMECC_PIOREG(1),
	    ((A24_NPAM << VMECC_PIOREG_AMSHIFT) | (A24_ADDR >> 23)));
    
#if _MIPS_SIM != _ABI64
        paddr = (K0_TO_PHYS(vme_slave) & (VMECC_PIOREG_MAPSIZE - 1));
#else
        paddr = (K1_TO_PHYS(vme_slave) & (VMECC_PIOREG_MAPSIZE - 1));
        WR_REG(chip_base+VMECC_A64MASTER, (vme_slave >> 32));
        WR_REG(chip_base+VMECC_A64SLVMATCH, (vme_slave >> 32));
#endif
        access_memory((vaddr+VMECC_PIOREG_MAPSIZE+paddr), vme_slave);
	msg_printf(VRB, "Passed VME A24 PIO loopback test - 1 level mapping\n"); 
	piotest++;
	msg_printf(VRB, "VME A24 PIO loopback test - 2 level mapping\n");

        mapram_ent(2, mapram, (A24_MASK|L2MAP_A24), l2map);
        WR_REG(chip_base+VMECC_PIOREG(1),
	    ((A24_NPAM << VMECC_PIOREG_AMSHIFT) | (L2MAP_A24 >> 23)));
    
	/*
        paddr = K0_TO_PHYS(vme_slave) & 0xfff;
        paddr |= ((K0_TO_PHYS(l2map) & 0x7fc) << IAMAP_L2_BLOCKSHIFT);
	*/
#if _MIPS_SIM != _ABI64
        paddr = (K0_TO_PHYS(vme_slave) & (VMECC_PIOREG_MAPSIZE - 1));
#else
        paddr = (K1_TO_PHYS(vme_slave) & (VMECC_PIOREG_MAPSIZE - 1));
        WR_REG(chip_base+VMECC_A64MASTER, (vme_slave >> 32));
        WR_REG(chip_base+VMECC_A64SLVMATCH, (vme_slave >> 32));
#endif

        access_memory((vaddr+VMECC_PIOREG_MAPSIZE+paddr), vme_slave);

	msg_printf(VRB,"Passed VME A24 PIO loopback mode - 2 level mapping\n");
	piotest = 0;
    }

#if _MIPS_SIM != _ABI64
    tlbrmentry(vaddr);
    tlbrmentry(mapram);
#endif

#ifdef MALLOC
    free((paddr_t)memory);
#endif

    if (piotest)
        msg_printf(INFO,"FAILED Checking VMECC A24 PIO Loopback Mode\n");
    else
        msg_printf(INFO,"Passed Checking VMECC A24 PIO Loopback Mode\n");
    return(piotest);

}

/*
 * Function : access_memory
 * Description :
 *	Access the specified virtual address, (assuming it has been properly
 *	mapped), using byte, short, and long word accessing mode. 
 *
 *	Write in loopback mode, and read back using 'kseg' address as well
 *	loopback mode and validate the data.
 * NOTE:
 * Size of data accessed in loopback mode should not exceed a page in the 
 * current setup. Hence value of VMES_SIZE is 1024. If more than one page
 * is needed, dynamic setup of the maptable may be needed 
 */
#if _MIPS_SIM != _ABI64
int
access_memory(caddr_t vaddr, caddr_t laddr )
#else
int
access_memory(paddr_t vaddr, paddr_t laddr )
#endif
/* vaddr => VME address. laddr => local kseg address. */
{
#if _MIPS_SIM != _ABI64
    int			i;
    unsigned		val;
    unchar		*caddr, cval;
    ushort		*saddr, sval;
    unsigned		*uaddr;
    volatile unsigned 	*uvaddr;
    int  cerror, serror, uerror;
#else
    int			i;
    __uint32_t		val;

    paddr_t		caddr;
    unchar		cval;

    paddr_t		saddr;
    ushort		sval;

    __uint32_t		uaddr;
    volatile paddr_t	uvaddr;

    int  cerror, serror, uerror;
#endif


    cerror = serror = uerror = 0;

    msg_printf(DBG,"access memory vaddr:0x%x laddr:0x%x\n", vaddr, laddr);
    msg_printf(DBG,"Byte access mode \n");

    for (i=0, caddr = vaddr; i < (VMES_SIZE * sizeof(__uint32_t)); 
	 i++, caddr++){
	 *((unchar *)caddr) = (i&0xff);
	 /* Check for error during writing */

#ifdef NEVER
	 if (cpu_intr_pending()){
	     msg_printf(DBG,"IP0: 0x%Lx, IP1: 0x%Lx \n", 
		EV_GET_REG(EV_IP0), EV_GET_REG(EV_IP1)); 
	     err_msg(VME_LPWRB, vme_loc, caddr);
	     /* longjmp(vmebuf, 2); */
	}
#endif

	us_delay(10);
    }

    /* Read back using the Loopback mode */
    for (i=0,caddr = vaddr ; i < (VMES_SIZE * sizeof(__uint32_t)); 
	 i++, caddr++){

        cval = *(unchar *)caddr;

	if (cval != (unchar)(i&0xff) ){
	    err_msg(VME_LPBKB, vme_loc, caddr, (i & 0x00ff), cval);
	 /*   longjmp(vmebuf, 2); */
	}
    }

    /* flush_iocache(Slot); */

    /* Read back using the coherent address space */
    for (i=0, caddr = K0_TO_K1(laddr); i < (VMES_SIZE * sizeof(__uint32_t)); 
	i++, caddr++){
	cval = *(char *)caddr;
	if (cval != (unchar)(i&0xff)){
	    err_msg(VME_LPCOB, vme_loc, caddr,  (i&0x00ff), cval);
	    /* longjmp(vmebuf, 2);  */
	    cerror++;
	}
    }

    msg_printf(DBG,"Halfword  access mode \n");
    /* Write in the Loopback Mode */
    for (i=0, saddr = vaddr ; i < (VMES_SIZE * sizeof(ushort)); 
	i++, saddr++){
	*(ushort *)saddr = (ushort) i;

	if (cpu_intr_pending()){
	    err_msg(VME_LPWRH, vme_loc, saddr);
	    /* longjmp(vmebuf, 2); */
	}

    }

    us_delay(10);

    for (i=0, saddr = vaddr ; i < (VMES_SIZE * sizeof(ushort)); 
	i++, saddr++){

	sval = *((ushort *)saddr); 

	if (sval != i){
	    err_msg(VME_LPBKH, vme_loc, saddr, i, sval);
	    /* longjmp(vmebuf, 2); */
	}
    }

    /* flush_iocache(Slot); */

    /* Read back in Coherent space */
    for (i=0, saddr = K0_TO_K1(laddr) ; i < (VMES_SIZE * sizeof(ushort)); 
	    i++, saddr++){
	sval = *((ushort *)saddr);

	if (sval != i){
	    err_msg(VME_LPCOH, vme_loc, saddr, i, sval);
	    /* longjmp(vmebuf, 2); */
	    serror++;
	}
    }

    msg_printf(DBG,"Word  access mode \n");
    /* Write in Loopback word mode */
    for (i=0,uvaddr = vaddr; i < VMES_SIZE; i++, uvaddr++){
	*((__uint32_t *)uvaddr) = (__uint32_t) uvaddr;

	if (cpu_intr_pending()){
	    err_msg(VME_LPWRW, vme_loc, uvaddr);
	    /* longjmp(vmebuf, 2); */
	}

    }

    us_delay(10);

    /* Read in loopback mode */
    for (i=0,uvaddr = vaddr ; i < (VMES_SIZE ); i++, uvaddr++){
	val = *((__uint32_t *)uvaddr);

	if (val != (__uint32_t)uvaddr){
	    err_msg(VME_LPBKW, vme_loc, uvaddr, (unsigned)uvaddr, val);
	    /* longjmp(vmebuf, 2); */
	}
    }

    /* Read in Coherent space */
    /* uvaddr is used to cross check with the data pattern written */
    for (i=0,uaddr = K0_TO_K1(laddr), uvaddr = vaddr; i < VMES_SIZE; i++,
	    uaddr++, uvaddr++){

	val = *((__uint32_t *)uaddr);

	if (val != (__uint32_t)uvaddr){
	    err_msg(VME_LPCOW, vme_loc, uaddr, (__uint32_t)uvaddr, val);
	    /* longjmp(vmebuf, 2); */
	}
    }
	    

    return((cerror << 16)|(serror << 8)|uerror);
}

/* 
 * Functin : vmecc_ftlbchk
 * Description:
 *	Setup the PIOMAP to do the VME loopback, Do a PIO loopback 
 * 	operation which results in a 2 level map entry, and check the
 *	F TLB entries. 
 *	Should be run when no other data movement is occuring on the IBUS/FCI
 */
#define	FTLB_MASK	0x200000
#if _MIPS_SIM != _ABI64
int
vmecc_ftlbchk(unsigned chip_base)
#else
vmecc_ftlbchk(paddr_t chip_base)
#endif
{
#if _MIPS_SIM != _ABI64
    caddr_t	vaddr, mapram;
    volatile    caddr_t vmeaddr;
    caddr_t	memory;
    unsigned 	*l2map, *vme_slave;
    int		val, paddr, result=0, base;
#else
    paddr_t	vaddr, mapram;
    volatile    paddr_t vmeaddr;
    paddr_t	memory;
    paddr_t	l2map, vme_slave, paddr;
    __uint32_t	val;
    int		result=0, base;
#endif

    msg_printf(INFO, "Checking F TLB Funtionality \n");


#if _MIPS_SIM != _ABI64
    if ((mapram = tlbentry(WINDOW(chip_base), 0, 0)) == 0)
	    return (1);

#ifdef	MALLOC
    if ((memory = malloc(VMEDATA)) == (paddr_t)0 ){
	tlbrmentry(mapram);
	return(1);
    }
#else
    memory = (paddr_t)vmelpbk_buf;
#endif
    if ((vaddr = tlbentry(WINDOW(chip_base), ANUM(chip_base), 0))==(paddr_t)0){
#ifdef MALLOC
	free(memory);
#endif
#ifndef TFP
	tlbrmentry(mapram);
#endif
	return(1);
    }
    set_pagesize(3);
#else 	/* TFP */
    mapram = LWIN_PHYS(WINDOW(chip_base), 0);
#ifdef	MALLOC
    if ((memory = malloc(VMEDATA)) == (paddr_t)0 ){
	return(1);
    }
#else
    memory = (paddr_t)K0_TO_K1(&vmelpbk_buf);
#endif
    vaddr = LWIN_PHYS(WINDOW(chip_base), ANUM(chip_base));
#endif
    
#if _MIPS_SIM != _ABI64
    setup_lev2map(memory, &l2map, &vme_slave);
#else
    setup_lev2map(memory, (paddr_t)&l2map, (paddr_t)&vme_slave);
#endif

    if (result = setjmp(vmebuf)){ 

	switch(result){
	default:
	case 1: msg_printf(ERR,"Got an Exception while doing F TLB testing\n");
		show_fault();
		break;
	case 2: break;
	}
	io_err_log(Slot, (1 << Anum));
	io_err_show(Slot, (1 << Anum));
    }
    else {
	set_nofault(vmebuf);

        mapram_ent(2, mapram, L2MAP_A32, l2map);

#if _MIPS_SIM != _ABI64
	paddr = K0_TO_PHYS(vme_slave);
#else
	paddr = K1_TO_PHYS(vme_slave);
        WR_REG(chip_base+VMECC_A64MASTER, (vme_slave >> 32));
        WR_REG(chip_base+VMECC_A64SLVMATCH, (vme_slave >> 32));
#endif

        WR_REG(chip_base+VMECC_PIOREG(1),
	    ((A32_NPAM << VMECC_PIOREG_AMSHIFT) | ((L2MAP_A32|paddr) >> 23)));

	vmeaddr = vaddr+VMECC_PIOREG_MAPSIZE+(paddr&(VMECC_PIOREG_MAPSIZE - 1));

	for (base=0; base < 7; base++){

            WR_REG(chip_base+FCHIP_TLB_FLUSH, 0x1fffff);  
	    WR_REG(chip_base + FCHIP_TLB_BASE, base); 

	    us_delay(10);

            *(__uint32_t *)(vmeaddr) = (__uint32_t)vmeaddr;

	    us_delay(10);

	    val = RD_REG(chip_base + FCHIP_TLB_IO0 + (base << 3)); 
            if (val != (FTLB_MASK | ((L2MAP_A32|paddr) >> 13))){
	        msg_printf(ERR,
		"Wrong Value in F TLB Entry %d. Expected: 0x%x, Got=0x%x\n",
			base, FTLB_MASK|((L2MAP_A32|paddr)>>13), val);
		if (continue_on_error() == 0)
		    longjmp(vmebuf, 2);
	    }/* if */
        }/* for */
    }/* else */

    clear_nofault();

#if _MIPS_SIM != _ABI64
    tlbrmentry(vaddr);
    tlbrmentry(mapram);
#endif

#ifdef MALLOC
    free((paddr_t)memory);
#endif

    msg_printf(INFO, "%s Checking F TLB functionality\n", 
	(result ? "FAILED" : "Passed"));

    return(result);
}

	


