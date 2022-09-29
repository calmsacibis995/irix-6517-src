/***********************************************************************\
*	File:		pod_lpbk.c					*
*									*
*	Contains routines used to test the IO4 board upto VMECC in  	*
*	Loop back mode.                                                 *
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_lpbk.c $Revision: 1.25 $"

#include "libsc.h"
#include "libsk.h"
#include "pod_fvdefs.h"
#include <sys/EVEREST/evconfig.h>

int	access_mem(caddr_t, unsigned *);
int	promvmecc_init(unsigned, unsigned);
int	vmea64_lpbk(int, int, int);
void	setup_lev2map(void);

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

#ifdef	NEWTSIM
#define	DATA_SIZE	768
#define	LEV2_MAP	16	/* Caters for 16*2k space access	*/
#define	VMES_SIZE	4
#else
#define	VMES_SIZE	512
#define	LEV2_MAP	512	/* 2Kbytes of map in Main Memory 	*/
#define	DATA_SIZE	2048	/* Due to increased LEV2_MAP size 	*/
#endif
unsigned	data_area[DATA_SIZE];	/* Eats up 3k/8k bss space	*/

#ifdef	NEWTSIM
unsigned	Tmp_Chip_base;
#endif

unsigned 	*vmecc_slave;
unsigned	*lev2_map;

#define	SETUP_DONE	0xdcba
static jmp_buf 	vbuf;

int 
pod_vmelpbk(int window, int ioslot, int ioa_num)
{
    machreg_t		*obuf;
    int			jval, setmap;
    volatile 		int level=0;
    __psunsigned_t	chip_base;


    if (promfchip_init(window, ioa_num) || (promvmecc_init(window, ioa_num)))
	return (FAILURE);

    chip_base = SWIN_BASE(window, ioa_num);
    setup_globals(window, ioslot, ioa_num);
    message(VCHK_LPBK);
    Nlevel++;

#ifdef	DEBUG_PROM
    setup_intr(chip_base, ioslot, ioa_num);	/* For Debugging */

    message("pod_vmelpbk: window: %d ioslot: %d ioa_num: %d\n", 
		window, ioslot, ioa_num);
#endif
    if (Mapram == 0) {
	if ((Mapram = tlbentry(window, 0, 0)) == 0)
	    return (FAILURE);
	setmap = 1;
	Mapram += 4;
    }
    else setmap = 0;

    if ((jval = setjmp(vbuf)) == 0){
	obuf = nofault;
	set_nofault(vbuf);
        setup_lev2map();
	setup_lpbk(chip_base, Mapram, vmecc_slave); level++;
	vmecc_ftlbchk(chip_base, window, ioa_num); level++;
        vmecc_piolpbk(chip_base, window, ioa_num); level++;
	vmea64_lpbk(window, ioslot, ioa_num);	level++;
	level = 0;
    }
    else{
	/* Return due to some exception   */
	if (jval == 1){
	    message("Exception during VMECC PIO loopback testing \n");
	    show_fault();
	}
	else{
	  switch(level){
	    case 0 : message(VCHK_LPSETF); break;
	    case 1 : message(VCHK_FTLBF); break;
	    case 2 : message(VCHK_LPIOF); break;
	    case 3 : message("Failed testing VMEA64 loopback mode\n"); break;
	    default: message(VCHK_LPUNKN); break;
	  }
	}
	if (setmap){
	    tlbrmentry(Mapram);
	    Mapram = 0;
	    setmap = 0;
	}
	clear_nofault();
	nofault = obuf;
	Nlevel--;
	if (nofault)
	    longjmp(nofault, 2);
	/*NOTREACHED*/
    }

    if (setmap){
        tlbrmentry(Mapram);
	Mapram = 0;
    }
    clear_nofault();
    nofault = obuf;
    Nlevel--;

    if (level)
       message(VCHK_LPBKF);
    else message(VCHK_LPBKP);

    return(level);

}

void setup_lev2map(void)
{
    unsigned 		i;
    paddr_t		phys_addr;

    lev2_map = (unsigned *)((__scunsigned_t)(data_area + 0x200) & ~0x7ff);
    vmecc_slave = lev2_map + LEV2_MAP;

    if ((__scunsigned_t)vmecc_slave & 0x800)
	vmecc_slave += LEV2_MAP; /* bump to a page boundary */
#if 0
    lev2_map = (unsigned *)((__scunsigned_t)&data_area[DATA_SIZE-1] & ~(0x7ff));

    vmecc_slave = ((lev2_map - data_area) >= VMES_SIZE ? 
    				data_area : &lev2_map[LEV2_MAP + 1]);
#endif
    phys_addr = K0_TO_PHYS(vmecc_slave) >> IAMAP_L2_BLOCKSHIFT;

    /* All entries in 2 level map, point to same physical address */
    for (i = 0; i < LEV2_MAP; i++)
        *(lev2_map + i) = (unsigned)phys_addr;
}

/* Steps to be followed.
 * 0  Within the data_area calculate where the 2k boundary is 
 *    and start the level 2 map there. Define the vmecc area to
 *    be the other portion of data_area
 * 1. Make Entries in the MAPRAM at appropriate indexes to do the 
 *    mapping of requests from VMECC.
 * 2. Update the lev2_map table in the memory to point to the 
 *    vmecc_slave area .
 * 3. Enable 32 bit slaves having A31-A28 being 1000 - 1110
 *    and 24 bit slaves having a23-22 as 01 and 10.
 * 4. Program PIOMAP1-4 entries with appropriate A32 and A24 AM 
 *    values, as well as the address bits for A31-23.
 *
 * NOTE : The slave program when launched, also uses the same PIOMAP entries.
 *	  It makes a tlbentry with offset of 0x02000000 which falls in the 
 *	  address range of the 4th PIOMAP entry.
 *	  If you are changing the Mapping entry, change the slave code 
 *	  appropriately.
 * NOTE : A24 Level 2 is not being enabled since A24 and A24L2 have addresses
 *	  which ends up using the same entry in Mapram. So only one of the
 *	  address mode is being tested.
 */
/* 
 * VMECC sets the upper 8 bits to 1 in case of A24 slave DMA. So this 
 * mask is used while calculating the mapping ram address 
 */
#define	A24_MASK	(unsigned)0xff000000

int
setup_lpbk(__psunsigned_t chip_base, caddr_t map, unsigned *lpbkmem)
{
    unsigned 		i, *addr;
    paddr_t		memoffs, slave_offset;

    /* Fill mapram with some bad values */
    for (i=0, addr= (unsigned *)map; i < 0x2000; i++, addr += 2)
	*addr = (0x400 + i) << 9;

    memoffs = K0_TO_PHYS(lpbkmem);

    slave_offset = ((K0_TO_PHYS(lpbkmem) >> IAMAP_L1_BLOCKSHIFT) << 9);

    /* Make entries in the DMA MAPRAM 		*/
    *(unsigned *)(map + IAMAP_L1_ADDR(0,(A32_ADDR|memoffs))) = 
	(unsigned)slave_offset;
    *(unsigned *)(map + IAMAP_L1_ADDR(0,(A24_MASK|A24_ADDR|memoffs))) = 
	(unsigned)slave_offset;

#ifdef	DEBUG_PROM
    message("setup_lpbk slave offset:0x%x a32offs: 0x%x a24offs: 0x%x\n", 
		slave_offset, IAMAP_L1_ADDR(0,(A32_ADDR|memoffs)),
		IAMAP_L1_ADDR(0,(A24_MASK|A24_ADDR|memoffs)));
#endif

    slave_offset = IAMAP_L2_ENTRY(K0_TO_PHYS(lev2_map));

    *(unsigned *)(map + IAMAP_L1_ADDR(0,L2MAP_A32)) = 
	(unsigned)slave_offset;
    *(unsigned *)(map + IAMAP_L1_ADDR(0,(L2MAP_A32|memoffs))) = 
	(unsigned)slave_offset;
#if	0
    *(unsigned *)(map + IAMAP_L1_ADDR(0,(A24_MASK|L2MAP_A24|memoffs))) =slave_offset; 
#endif

#ifdef	DEBUG_PROM
    message("2 Lvl setup_lpbk slave offset:0x%x a32offs: 0x%x a24offs: 0x%x\n", 
		slave_offset, IAMAP_L1_ADDR(0,(L2MAP_A32|memoffs)),
		IAMAP_L1_ADDR(0,(A24_MASK|L2MAP_A24|memoffs)));
#endif

    /* Initialize the VMECC config register appropriately	*/
    /* ensure to enable All A24 Slave address range.		*/
    WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE | 0xf000);

    /* CAUTION : Before changing this part of the code, look at the NOTE
     * at the start of the function.
     */
    /* Place appropriate values in PIOMAP Regs		*/
    WR_REG(chip_base+VMECC_PIOREG(4), 
    	((VME_A32NPAMOD << VMECC_PIOREG_AMSHIFT) | ((A32_ADDR|memoffs)>>23)));

    WR_REG(chip_base+VMECC_PIOREG(5),
    	((VME_A24NPAMOD << VMECC_PIOREG_AMSHIFT) | (A24_ADDR >> 23)));

    WR_REG(chip_base+VMECC_PIOREG(6), 
    	((VME_A32NPAMOD << VMECC_PIOREG_AMSHIFT) | ((L2MAP_A32|memoffs)>>23)));

    WR_REG(chip_base+VMECC_PIOREG(7),
    	((VME_A24NPAMOD << VMECC_PIOREG_AMSHIFT) | (L2MAP_A24 >> 23)));

    return (SUCCESS);
}

int
vmecc_piolpbk(__psunsigned_t chip_base, int window, int ioa_num)
{

    volatile 	int level=0, jval;
    caddr_t	vaddr;
    jmp_buf	jbuf;
    machreg_t	*obuf;

    message(VCHK_LPIO);

#ifdef	NEWTSIM
    Tmp_Chip_base = chip_base;
#endif

    /* This maps 32 Mb from window 4-7 which is good enough	*/
    vaddr = tlbentry(window, ioa_num, 0x02000000); 

    /* Although the virtual addresses used to access the memory in the
     * loopback mode varies, they all map to the same address in the 
     * main memory, due to the mapping done using the PIOMAP
     */


    if ((jval = setjmp(jbuf)) == 0) {
	obuf = nofault;
	set_nofault(jbuf);
	WR_REG(chip_base + FCHIP_TLB_FLUSH, 0xffffffff);
#ifdef	IA_TESTING
  	master_loop(vaddr+0x00000000+(A32_ADDR  & 0x7fffff),1,vmecc_slave);
#endif

  	access_mem(vaddr + 0x00000000 + (A32_ADDR  & 0x7fffff), vmecc_slave);
	level++;
	WR_REG(chip_base + FCHIP_TLB_FLUSH, 0xffffffff);
	access_mem(vaddr + 0x00800000 + (A24_ADDR  & 0x7fffff), vmecc_slave);
        level++;
	WR_REG(chip_base + FCHIP_TLB_FLUSH, 0xffffffff);
  	access_mem(vaddr + 0x01000000 + (L2MAP_A32 & 0x7fffff), vmecc_slave);
	level++;
#if	0
	WR_REG(chip_base + FCHIP_TLB_FLUSH, 0xffffffff);
	access_mem(vaddr + 0x01800000 + (L2MAP_A24 & 0x7fffff), vmecc_slave);
	level = 0;
#endif
	WR_REG(chip_base + FCHIP_TLB_FLUSH, 0xffffffff);
    }
    else {
	if (jval == 1){
	    message("Exception during memory access in VME Loopback mode\n");
	    show_fault();
	}
	switch(level){
	    default:
	    case 0 : message(VCHK_LPA32F); break;
	    case 1 : message(VCHK_LPA24F); break;
	    case 2 : message(VCHK_LPL2A32F); break;
	    case 3 : message(VCHK_LPL2A24F); break;
	}
	clear_nofault();
	nofault = obuf;
	tlbrmentry(vaddr);
	longjmp(nofault, 2);
    }

    tlbrmentry(vaddr);

    message(VCHK_LPIOP);
    Return (level);

}

#ifdef	DEBUG_PROM
#define	CHECK_INTR(i)	if((i&0xff) == 0xff) check_intr(i)
#else
#define	CHECK_INTR(i)
#endif
/*
 * Function : access_mem
 * Description :
 *	Access the specified virtual address, (assuming it has been properly
 *	mapped), using byte, short, and long word accessing mode. In all
 *	these cases, data read directly from memory should match the ones
 *	read in the loopback mode.
 *	Reading of the memory is done using the kseg0 address.
 */
int
access_mem(caddr_t vaddr, unsigned *lpbkmem)
{
    int			i, val;
    paddr_t		val1;
    unchar		*caddr, cval;
    ushort		*saddr, sval;
    unsigned		*uaddr;
    volatile unchar	*cvaddr; 
    volatile ushort	*svaddr;
    volatile unsigned 	*uvaddr;
    int  cerror, serror, uerror;


    cerror = serror = uerror = 0;

    val1 = (K0_TO_PHYS(lpbkmem) & (VMECC_PIOREG_MAPSIZE - 1)); 

    vaddr += val1;

#ifdef	DEBUG_PROM
    message("access_mem: vaddr: 0x%x lpbkmem:0x%x val1:0x%x\n",
		vaddr, lpbkmem, val1);
#endif

    /*message("VMECC Loopback memory access in Byte mode...\n");*/

    for (i=0,cvaddr = (unchar *)vaddr ;
    	 i < (VMES_SIZE * sizeof(unsigned)); i++,cvaddr++){

	*(cvaddr) = (i & 0xff); /* Loopback write */
	CHECK_INTR(i);
    }

    /*message("Reading data in Loopback Byte mode \n");*/

    /* Read in loopback mode */
    for (i=0,cvaddr = (unchar *)vaddr,caddr = (unchar *)lpbkmem ; 
    	 i < (VMES_SIZE * sizeof(unsigned)); i++,cvaddr++, caddr++){

	 /* First from the loopback space */
	 if ((cval = *(caddr)) != (i&0xff)){
	    mk_msg(VMELP_MEMBY_ERR,caddr, cvaddr, (i&0xff), cval);
	    cerror++; 
	    analyze_error(3);
	}
	/* Now from local address space */
	if ((cval = *(caddr)) != (i&0xff)){
	    mk_msg(VMELP_MEMBY_ERR,caddr, cvaddr, (i&0xff), cval);
	    cerror++; 
	    analyze_error(3);
	}
	CHECK_INTR(i);
    }

    /*message("VMECC Loopback memory access in half word mode \n");*/
    for (i=0, svaddr = (ushort *)vaddr ; i < (VMES_SIZE * sizeof(ushort)); 
	i++, svaddr++){
	*svaddr = i & 0xffff;
	CHECK_INTR(i);
    }

    /*message("Reading data in Loopback halfword mode \n");*/

    for (i=0, saddr = (ushort *)lpbkmem, svaddr = (ushort *)vaddr ;
    	i < (VMES_SIZE * sizeof(ushort));  i++, saddr++, svaddr++){

	if ((sval = *(svaddr)) != (i & 0xffff)){
	    mk_msg(VMELP_MEMWD_ERR, saddr, svaddr, (i&0xffff), sval);
	    serror++;
	    analyze_error(3);
	}
	if ((sval = *(saddr)) != (i & 0xffff)){
	    mk_msg(VMELP_MEMWD_ERR, saddr, svaddr, (i&0xffff), sval);
	    serror++;
	    analyze_error(3);
	}
	CHECK_INTR(i);
    }


    /*message("VMECC loopback memory access in word mode \n");*/
    for (i=0,uvaddr = (unsigned *)vaddr ; i < (VMES_SIZE ); i++, uvaddr++){
	*uvaddr = (unsigned)i;
        CHECK_INTR(i);
    }
    
    /*message("Reading data in Loopback word mode \n");*/

    for (i=0,uaddr = (unsigned *)lpbkmem, uvaddr = (unsigned *)vaddr; 
	 i < VMES_SIZE; i++, uaddr++, uvaddr++){

	if ((val = *uvaddr) != (unsigned)i){
	    mk_msg(VMELP_MEMLO_ERR, uaddr, uvaddr, uvaddr, val);
	    uerror++;
	    analyze_error(3);
	}

	if ((val = *uaddr) != (unsigned)i){
	    mk_msg(VMELP_MEMLO_ERR, uaddr, uvaddr, uvaddr, val);
	    uerror++;
	    analyze_error(3);
	}
	CHECK_INTR(i);
    }

    Return((cerror << 16)|(serror << 8)|uerror);
}

#define	FTLB_MASK	0x200000
/* 
 * Functin : vmecc_ftlbchk
 * Description:
 *	Setup the PIOMAP to do the VME loopback, Do a PIO loopback 
 * 	operation which results in a 2 level map entry, and check the
 *	F TLB entries. 
 *	Should be run when no other data movement is occuring on the IBUS/FCI
 *	Expects Mapram to have Valid Virtual address to access mapram.
 *	Expects the VMECC PIO map registers to have been setup right.
 */
int
vmecc_ftlbchk(__psunsigned_t chip_base, int window, int ioa_num)
{
    caddr_t		vaddr;
    volatile unsigned  *vmeaddr;
    uint		base;
    paddr_t		paddr;
    evreg_t		val;

    clear_ereg(3);
    message(VCHK_FTLB);

    /* Flush FTLB  */
    WR_REG(chip_base+FCHIP_TLB_FLUSH, 0x1fffff);  

    vaddr = tlbentry(window, ioa_num, 0x02000000);

    /* 0x1000000 takes the address to A32L2_ADDR. So hope to get 
     * A TLB Entry in place 
     */
    paddr  = K0_TO_PHYS(vmecc_slave);
    vmeaddr = (unsigned *)(vaddr + (VMECC_PIOREG_MAPSIZE*2) + 
	     (paddr & (VMECC_PIOREG_MAPSIZE - 1))); 
    
    paddr = FTLB_MASK | ((L2MAP_A32|paddr) >> 13);
    for (base=0; base < 7; base++){
	WR_REG(chip_base+FCHIP_TLB_FLUSH, 0x1fffff);
	WR_REG(chip_base+FCHIP_TLB_BASE, base);

        *(vmeaddr);
	val = RD_REG(chip_base+ FCHIP_TLB_IO0 + (base * 8) );

	if (!(val & FTLB_MASK)){
	    mk_msg("Fchip Didnot Validate F-TLB entry %d\n", base);
	    tlbrmentry(vaddr);
	    analyze_error(3);
	}

	if (val != paddr){
	    mk_msg(FTLB_VAL_ERR, paddr, val);
	    tlbrmentry(vaddr);
	    analyze_error(3);
	}

    }

    tlbrmentry(vaddr);
    message(VCHK_FTLBP);

    return(SUCCESS);
}

extern unsigned	vmecc_slave_area[];

/*ARGSUSED*/
int
vmea64_lpbk(int iowin, int slot, int anum)
{
    paddr_t		phys_addr = K0_TO_PHYS(vmecc_slave_area);
    __psunsigned_t 	vaddr;
    unsigned 		volatile *v1;
    __psunsigned_t  	chip_base = SWIN_BASE(iowin, anum);
    unsigned		val, i;

    /* Enable A64 address mapping */
    WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE|0x800);
    WR_REG(chip_base+VMECC_A64SLVMATCH, 0);
    WR_REG(chip_base+VMECC_A64MASTER, 0);
    WR_REG(chip_base+VMECC_PIOREG(1), (0x01 << 10)|(phys_addr >> 23));

    message("Checking VMECC in A64 loopback mode \n");

    vaddr = (__psunsigned_t)tlbentry(iowin, anum, 0);
    vaddr += 0x800000;	/* Advance it by 8Mb to move it to PIOREG(1) */

    vaddr |= ((__psunsigned_t)vmecc_slave_area & 0x7fffff); /* lower 23 bits */

    /* message("==Virtual addr: 0x%x maps to Physical Memory address 0x%x ==\n",
		vaddr, K0_TO_PHYS(vmecc_slave_area));
    */

    /* Initialize the memory to some known state */
    for (i=0; i < 128; i++)
	vmecc_slave_area[i] = 0;

    /* Loopback writes */
    for (i=0, v1 = (unsigned *)vaddr; i < 128; i++, v1++)
	*v1 = (unsigned)i;

    for (i=0, v1 = (unsigned *)vaddr ; i < 128; i++, v1++){
	/* Loopback read */
	val = *v1;
	if (val != (unsigned)i){
	    mk_msg("VMEA64 Loopback read: vaddr %x Expected: 0x%x Got 0x%x\n",
			v1, i, val);
	    longjmp(nofault, 2);
	}
	/* Direct read */
	val = vmecc_slave_area[i];
	if (val != (unsigned)i ){
	    mk_msg("VMEA64 Loopback: vaddr %x Expected: 0x%x Got 0x%x\n",
			v1, i, val);
	    longjmp(nofault, 2);
	}
    }

    tlbrmentry((caddr_t)vaddr);
    message("VME A64 Loopback test passed \n");
    return SUCCESS;
}


/*ARGSUSED*/
void setup_intr(__psunsigned_t chip_base, int slot, int anum)
{
    uint    	procnum = (uint)(EV_GET_LOCAL(EV_SPNUM) & EV_SPNUM_MASK);
    evreg_t 	ev_ile, tmp;
    k_machreg_t sr;


    sr = get_SR(); 
    set_SR(sr & ~SR_IE);

    while (tmp = EV_GET_LOCAL(EV_HPIL)){
	EV_SET_LOCAL(EV_CIPL0, tmp);
    }

    IO4_SETCONF_REG(slot, IO4_CONF_INTRMASK, 0x2);
    IO4_SETCONF_REG(slot, IO4_CONF_INTRVECTOR, (0x7b00|procnum)); /* IO4 intr */

    RD_REG(chip_base+FCHIP_ERROR_CLEAR); /* Clear Ferror register */
    WR_REG(chip_base+FCHIP_INTR_MAP, 0x7a00 | procnum);
    WR_REG(chip_base+FCHIP_INTR_RESET_MASK, 1); /* Enable F error interrupt */

    RD_REG(chip_base+VMECC_ERRCAUSECLR);  /* Clear VME Error register */
    WR_REG(chip_base+VMECC_VECTORERROR, 0x7c00 | procnum);
    WR_REG(chip_base+VMECC_INT_ENABLESET, 1);

    ev_ile = EV_GET_LOCAL(EV_ILE);
    EV_SET_LOCAL(EV_ILE, ev_ile|0x1); /* Enable Interrupts */

}

void check_intr(int i)
{
    evreg_t	hpil;
    if (hpil = EV_GET_LOCAL(EV_HPIL)){
	message("Interrupt 0x%x is the HPIL, Loopoffset= 0x%x \n", hpil, i);
	EV_SET_LOCAL(EV_CIPL0, hpil);
	message("IP1 = 0x%llx\n", EV_GET_LOCAL(EV_IP1));
	analyze_error(3);
    }
}

#if	IA_TESTING
/* Bunch of routines to help isolate the IA bug being seen */
/* Also requires slave proc stacks to be in cached space in sysinit.c */

uint loopcount[EV_MAX_CPUS];
uint stopslave[EV_MAX_CPUS];

void
sl_loop(volatile uint *cacheline)
{

    while (1){
	__dcache_inval(cacheline, 0x80);

	   (void) *(cacheline + 4);
    }

}
extern int Debug;

master_loop(caddr_t vaddr, int level, unsigned *lpbkmem)
{
    int			i, val;
    unsigned		val1;
    uint		count, vpid;
    uint		slotnum;
    unsigned		slavemem;
    unsigned		*uaddr;
    volatile unsigned 	*uvaddr;
    evcpucfg_t		*cpuarr;


    val1 = (K0_TO_PHYS(lpbkmem) & (VMECC_PIOREG_MAPSIZE - 1)); 
    vaddr += val1;
 
    /* Start the slave loops HERE */
    slotnum = (EV_GET_LOCAL(EV_SPNUM) >> EV_SLOTNUM_SHFT) & EV_SLOTNUM_MASK;
    cpuarr  = EVCFGINFO->ecfg_board[slotnum].eb_cpuarr;

    for (i=0; i < EV_MAX_CPUS; i++){
	loopcount[i] = 0;
	stopslave[i] = 0;
    }

    Debug = 1;
    slavemem = ((uint)lpbkmem & ~0xfff) | ((uint)vaddr & 0xfff);

    for (i=0; i < EV_MAX_CPUS_BOARD; i++){
	vpid = cpuarr[i].cpu_vpid;
	if (vpid == cpuid())
	    continue;
	
	message("Launching slave with vpid %x ", vpid);
	if ((vpid & 0x3) == 3){
	    message("Cacheline 0x%x\n", slavemem+0x128);
	    launch_slave(vpid, sl_loop, slavemem+0x128, 0, 0, 0);
	}
	else {
	    message("Cacheline 0x%x\n", slavemem);
	    launch_slave(vpid, sl_loop, slavemem, 0, 0, 0);
	}
	us_delay(1000*1000);
    }


    message("master VMECC loopback memory access in word mode \n");
    count = 0;
    uvaddr = (unsigned *)vaddr;
    while (1){
	(void) *(uvaddr);

#ifdef NOTDEF 
        for (i=0,uvaddr = (unsigned *)vaddr ; i < 32; i++, uvaddr++){
	    val = *uvaddr;
	    /* *(uvaddr) = (unsigned)uvaddr; Dont write. */
            CHECK_INTR(i);
        }

	if ((++count & 0xffff) == 0xffff)
	    message("Completed %x loops of loopbacking \n", count);
        for (i=0,uaddr = (unsigned *)lpbkmem, uvaddr = (unsigned *)vaddr; 
	 i < VMES_SIZE; i++, uaddr++, uvaddr++){

	    if ((val = *uvaddr) != (unsigned)uvaddr){
	        mk_msg(VMELP_MEMLO_ERR, uaddr, uvaddr, (unsigned)uvaddr, val);
	        analyze_error(3);
	    }

	    if ((val = *uaddr) != (unsigned)uvaddr){
	        mk_msg(VMELP_MEMLO_ERR, uaddr, uvaddr, (unsigned)uvaddr, val);
	        analyze_error(3);
	    }
	    CHECK_INTR(i);
       }
#endif

    }
}
#endif
