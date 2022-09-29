/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 *	danglib.c - dang chip library functions module		 	*
 *									*
 ************************************************************************/

#ident  "arcs/ide/EVEREST/io4/graphics/danglib.c $Revision: 1.7 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/dang.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/gr2.h>
#include <sys/gr2hw.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>
#include <everr_hints.h>
#include "danglib.h"

/*************************************************************************/

/*
 * various defines needed later on
 */

/*************************************************************************/

#if defined(IP25)
#define FLUSHWG(WAIT,BDATA) {\
    volatile int foo;\
    foo = EV_GET_LOCAL(EV_RTC);\
    if (WAIT) {\
    foo = load_double_lo((long long *)&BDATA[DANG_WG_WDCNT]);\
    }\
}
#define DRAINDANG(bdata) \
{       \
    int i=0;\
    volatile int fifocnt; \
    store_double((long long *)&bdata[DANG_WG_PAUSE], (long long)0); \
    do { \
	fifocnt = load_double_lo((long long *)&bdata[DANG_WG_WDCNT]); \
	if (fifocnt == 1) { \
	 store_double((long long *)&bdata[DANG_WG_POP_FIFO], (long long)0); \
	 fifocnt = load_double_lo((long long *)&bdata[DANG_WG_FIFO_RDADDR]); \
	 fifocnt = load_double_lo((long long *)&bdata[DANG_WG_FIFO_RDADDR]); \
	 store_double((long long *)&bdata[DANG_WG_STREAM_WDCNT],(long long)0); \
	 store_double((long long *)&bdata[DANG_WG_WDCNT],(long long)0); \
	} \
	us_delay(5); \
	if (++i > 100000) {\
	    break;\
	} \
    }while(fifocnt);\
}
#endif

/*
 * convenient way to group bit flag info
 */
typedef struct dang_bitstruct{
    long long mask;			/* 1's for bits in use */
    unsigned int isfield;		/* TRUE if field, not boolean */
    unsigned int fshift;		/* number of bits to rightshift field */
    char * name;			/* name to give in error messages */
}S_Bits;

/*
 * convenient way to group field value data for further interpretation 
 */
typedef struct dang_fstruct{
    unsigned int value;			/* numeric value */
    unsigned int isvalid;		/* TRUE if valid, FALSE if error code */
    char * name;			/* name to give in error messages */
}S_Fields;

/*************************************************************************/

/*
 * DANG PIO error register
 */
static S_Bits dang_pioerr[] = {
{ DANG_PIO_ERR_IDATA, FALSE, 0, " 0: dang_pio_err_idata" },
{ DANG_PIO_ERR_ICOM, FALSE, 0, " 1: dang_pio_err_icom" },
{ DANG_PIO_ERR_ISUPR, FALSE, 0, " 2: dang_pio_err_isupr" },
{ DANG_PIO_ERR_BERR, FALSE, 0, " 3: dang_pio_err_berr" },
{ DANG_PIO_ERR_BERRC, FALSE, 0, " 4: dang_pio_err_berrc" },
{ DANG_PIO_ERR_VERSION, TRUE, 8, "11..8: dang_pio_err_version" },
{ 0, FALSE, 0, "ERROR - invalid entry!" }
};

/*************************************************************************/

/*
 * DANG Master DMA status register
 */
static S_Bits dang_mdmastat[] = {
{ DANG_DMA_STAT_BUSY, FALSE, 0, " 0: dang_dma_stat_busy" },
{ DANG_DMA_STAT_DIR, TRUE, 1, " 1: dang_dma_stat_dir" },
{ DANG_DMAM_STAT_IFETCH, FALSE, 0, " 2: dang_dma_stat_ifetch" },
{ DANG_DMAM_STAT_SFETCH, FALSE, 0, " 3: dang_dma_stat_sfetch" },
{ DANG_DMAM_STAT_CMP, FALSE, 0, " 4: dang_dma_stat_cmp" },
{ 0, FALSE, 0, "ERROR - invalid entry!" }
};

/*
 * DANG DMA status register interface statuses
 * this is done separately since these are numeric states, not bitflags
 * and state names seemed more appropriate.  Since the slave DMA uses the
 * same state table, this is dang_dma... not dang_mdma...
 */
static S_Fields dang_dmaifstat[] = {
{ DANG_DMA_IF_IDLE, TRUE, "dang_dma_if_idle" },
{ DANG_DMA_IF_WSTART, TRUE, "dang_dma_if_wstart" },
{ DANG_DMA_IF_RDEL, TRUE, "dang_dma_if_rdel" },
{ DANG_DMA_IF_WIBUS, TRUE, "dang_dma_if_wibus" },
{ DANG_DMA_IF_WGACK, TRUE, "dang_dma_if_wgack" },
{ DANG_DMA_IF_WIACK, TRUE, "dang_dma_if_wiack" },
{ DANG_DMA_IF_STALLED, TRUE, "dang_dma_if_stalled" },
{ DANG_DMA_IF_BUSY, TRUE, "dang_dma_if_busy" },
{ 0, FALSE, "ERROR - invalid state!" }
};

/*
 * DANG Master DMA error register
 */
static S_Bits dang_mdmaerr[] = {
{ DANG_DMAM_ERR_PIOKILL, FALSE, 0, " 0: dang_dmam_err_piokill" },
{ DANG_DMAM_ERR_IRTOUT, FALSE, 0, " 1: dang_dmam_err_irtout" },
{ DANG_DMAM_ERR_IPAR, FALSE, 0, " 2: dang_dmam_err_ipar" },
{ DANG_DMAM_ERR_GPAR, FALSE, 0, " 3: dang_dmam_err_gpar" },
{ DANG_DMAM_ERR_CPAR, FALSE, 0, " 4: dang_dmam_err_cpar" },
{ 0, FALSE, 0, "ERROR - invalid entry!" }
};

/*************************************************************************/

/*
 * DANG DMA Slave status register
 */
static S_Bits dang_sdmastat[] = {
{ DANG_DMA_STAT_BUSY, FALSE, 0, " 0: dang_dma_stat_busy" },
{ DANG_DMA_STAT_DIR, FALSE, 0, " 1: dang_dma_stat_dir" },
{ DANG_DMAS_STAT_PRE , FALSE, 0, " 2: dang_dmas_stat_pre" },
{ 0, FALSE, 0, "ERROR - invalid entry!" }
};

/*
 * DANG DMA Slave if status
 * use the dang_dmaifstat defines, above -
 * slave dma is a subset of the master dma
 */

/*
 * DANG DMA Slave error register
 */
static S_Bits dang_sdmaerr[] = {
{ DANG_DMAS_ERR_IPAR, FALSE, 0, " 0: dang_dmas_err_ipar" },
{ DANG_DMAS_ERR_GCOM, FALSE, 0, " 1: dang_dmas_err_gcom" },
{ DANG_DMAS_ERR_GBYTE, FALSE, 0, " 2: dang_dmas_err_gbyte" },
{ DANG_DMAS_ERR_GDATA, FALSE, 0, " 3: dang_dmas_err_gdata" },
{ DANG_DMAS_ERR_IRTOUT, FALSE, 0, " 4: dang_dmas_err_irtout" },
{ 0, FALSE, 0, "ERROR - invalid entry!" }
};

/*************************************************************************/

/*
 * DANG Interrupt status register
 */
static S_Bits dang_irstat[] = {
{ DANG_ISTAT_DCHIP, FALSE, 0, " 1: dang_istat_dchip" },
{ DANG_ISTAT_DMAM_CMP, FALSE, 0, " 2: dang_istat_dmam_cmp" },
{ DANG_ISTAT_WG_FLOW, FALSE, 0, " 3: dang_istat_wg_flow" },
{ DANG_ISTAT_WG_FHI, FALSE, 0, " 4: dang_istat_wg_fhi" },
{ DANG_ISTAT_WG_FULL, FALSE, 0, " 5: dang_istat_wg_full" },
{ DANG_ISTAT_WG_PRIV, FALSE, 0, " 6: dang_istat_wg_priv" },
{ DANG_ISTAT_GIO0, FALSE, 0, " 7: dang_istat_gio0" },
{ DANG_ISTAT_GIO1, FALSE, 0, " 8: dang_istat_gio1" },
{ DANG_ISTAT_GIO2, FALSE, 0, " 9: dang_istat_gio2" },
{ DANG_ISTAT_GIOSTAT, FALSE, 0, "10: dang_istat_giostat" },
{ DANG_ISTAT_DSYNC, FALSE, 0, "11: dang_istat_dsync" },
{ DANG_ISTAT_GFXDLY, FALSE, 0, "12: dang_istat_gfxdly" },
{ 0, FALSE, 0, "ERROR - invalid entry!" }
};

/*************************************************************************/

/*
 * DANG Write Gatherer status register
 */
static S_Bits dang_wgstat[] = {
{ DANG_WGSTAT_IDLE, FALSE, 0, " 0: dang_wgstat_idle" },
{ DANG_WGSTAT_WDATA, FALSE, 0, " 1: dang_wgstat_wdata" },
{ DANG_WGSTAT_PRIV, FALSE, 0, " 2: dang_wgstat_priv" },
{ DANG_WGSTAT_FILL, TRUE, 3, " 4..3: dang_wgstat_fill" },
{ DANG_WGSTAT_WEXT, TRUE, 5, " 7..5: dang_wgstat_wext" },
{ DANG_WGSTAT_DRAIN, TRUE, 8, " 9..8: dang_wgstat_drain" },
{ 0, FALSE, 0, "ERROR - invalid entry!" }
};

/*************************************************************************/

/*
 * jump buffer for all the dang code
 */
jmp_buf dangbuf;

/*
 * central location for all status stuff to refer to - this way I can
 * roll the actual setting into one routine in the interrupt service
 * code
 */
long long d_pioerr,
	  d_mdmastat, d_mdmaerr,
	  d_sdmastat, d_sdmaerr,
	  d_intstat,
	  d_wgstat;

/*
 * get the whole status schmere here
 */
void
dang_log_status(int slot, int adap)
{
    int window;
    volatile long long * dangchip;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);
    dangchip = (long long *) SWIN_BASE(window, adap);

    d_pioerr = load_double((long long *)&dangchip[DANG_PIO_ERR]);

    d_mdmastat = load_double((long long *)&dangchip[DANG_DMAM_STATUS]);
    d_mdmaerr = load_double((long long *)&dangchip[DANG_DMAM_ERR]);

    d_sdmastat = load_double((long long *)&dangchip[DANG_DMAS_STATUS]);
    d_sdmaerr = load_double((long long *)&dangchip[DANG_DMAS_ERR]);

    d_intstat = load_double((long long *)&dangchip[DANG_INTR_STATUS]);

    d_wgstat= load_double((long long *)&dangchip[DANG_WG_STATUS]);
}

/*
 * general purpose formatting routine for all bit flags
 */
static void
print_dangbits(char * rn, long long regval, S_Bits *fp)
{
    /*
     * print register name indented by one tab stop
     */
    ev_perr(REG_LVL, "%s: 0x%llx\n", rn, (long long) regval);

    /*
     * print name of each set bitflag and the value of each bitfield, set
     * or not.  All values indented two tab stops
     */
    for ( ; fp->mask; fp++ )
    {
	if (fp->isfield)
	{
	    ev_perr(BIT_LVL, "%s: 0x%llx\n", fp->name,
	    (long long)((long long)(regval & fp->mask) >> fp->fshift));
	}
	else if (regval & fp->mask)
	{
	    ev_perr(BIT_LVL, "%s\n", fp->name);
	}
    }
}

/*
 * general purpuse formatting routine for status fields (ie, meaning by value
 * rather than individual bits)
 */
static void
print_dangfield(char * rn, char *fn, long long fv, S_Fields * fp)
{
    /*
     * locate correct field name
     */
    for ( ; fp->isvalid && (fv != fp->value); fp++);

    /*
     * print register name, field name, value, and condition name
     */
    ev_perr(BIT_LVL, "%s, %s: %s <%llx>\n",
		rn, fn, fp->name, (long long) fv);
}

/*
 * routine to print the name and status of a selected dang register
 */
int
print_dangreg_status(int reg_offset,  long long reg_value)
{
    int errval = 0;

    switch (reg_offset) {
	case  DANG_PIO_ERR:
	    print_dangbits("dang_pio_err", (long long) reg_value, dang_pioerr);
	    break;

    	case DANG_DMAM_STATUS:
	    print_dangbits("dang_dmam_status", (long long) reg_value,
			    dang_mdmastat);

	    print_dangfield("dang_dmam_status", "dma if 1 - ibus to fifo",
			     DANG_DMA_IF_STATUS(0, (long long) reg_value),
			     dang_dmaifstat);
	    print_dangfield("dang_dmam_status", "dma if 2 - fifi to gio ",
			     DANG_DMA_IF_STATUS(1, (long long) reg_value),
			     dang_dmaifstat);
	    print_dangfield("dang_dmam_status", "dma if 3 - gio to fifo ",
			     DANG_DMA_IF_STATUS(2, (long long) reg_value),
			     dang_dmaifstat);
	    print_dangfield("dang_dmam_status", "dma if 4 - fifo to ibus",
			     DANG_DMA_IF_STATUS(3, (long long) reg_value),
			     dang_dmaifstat);
	    break;

	case DANG_DMAM_ERR:
	    print_dangbits("dang_dmam_err", (long long) reg_value,
			    dang_mdmaerr);
	    break;

	case DANG_DMAS_STATUS:
	    print_dangbits("dang_dmas_status", (long long) reg_value,
			    dang_sdmastat);

	    print_dangfield("dang_dmas_status", "dma if 1 - ibus to fifo",
			     DANG_DMA_IF_STATUS(0, (long long) reg_value),
			     dang_dmaifstat);
	    print_dangfield("dang_dmas_status", "dma if 2 - fifi to gio ",
			     DANG_DMA_IF_STATUS(1, (long long) reg_value),
			     dang_dmaifstat);
	    print_dangfield("dang_dmas_status", "dma if 3 - gio to fifo ",
			     DANG_DMA_IF_STATUS(2, (long long) reg_value),
			     dang_dmaifstat);
	    print_dangfield("dang_dmas_status", "dma if 4 - fifo to ibus",
			     DANG_DMA_IF_STATUS(3, (long long) reg_value),
			     dang_dmaifstat);
	    break;

    	case DANG_DMAS_ERR:
	    print_dangbits("dang_dmas_err", (long long) reg_value,
	   		    dang_sdmaerr);
	    break;

	case DANG_INTR_STATUS:
	    print_dangbits("dang_intr_status", (long long) reg_value,
			    dang_irstat);
	    break;

	case DANG_WG_STATUS:
	    print_dangbits("dang_wg_status", (long long) reg_value,
			    dang_wgstat);
	    break;

	default:
	    msg_printf(ERR,
		"print_dangreg_status: unknown dang register 0x%d\n",
		reg_offset);
	    errval++;
	    break;
    }
    return errval;
}
	    

/*
 * routine to print the status of a selected DANG chip
 */
void
dang_show_status(int slot, int adap)
{
    /*
     * print out location
     */
    ev_perr(ASIC_LVL, "DANG Chip: Slot 0x%x, Adapter 0x%x\n", slot, adap);

    print_dangreg_status(DANG_PIO_ERR, (long long) d_pioerr);

    print_dangreg_status(DANG_DMAM_STATUS, (long long) d_mdmastat);

    print_dangreg_status(DANG_DMAM_ERR, (long long) d_mdmaerr);

    print_dangreg_status(DANG_DMAS_STATUS, (long long) d_sdmastat);

    print_dangreg_status(DANG_DMAS_ERR, (long long) d_sdmaerr);

    print_dangreg_status(DANG_INTR_STATUS, (long long) d_intstat);

    print_dangreg_status(DANG_WG_STATUS, (long long) d_wgstat);
}

/*
 * routine to print the status of a selected DANG chip
 */
int
print_dangstatus(int slot, int adap)
{
    /*
     * Get the current interrupt mask contents and disable all interrupt
     * sources - if cannot disable interrupts, exit
     */
    if (setjmp(dangbuf)) {
	msg_printf(ERR, "Exception Accessing DANG chip!\n");
	clear_nofault();
	return (1);
    }
    else {
	set_nofault(dangbuf);
	dang_log_status(slot, adap);
	dang_show_status(slot, adap);
	clear_nofault();
    }

    return(0);
}

/*
 * Do various initialisation.
 * Set up all the DANG registers.
 * Allocate a virtual page to map to the EXPRESS area on the GIO.
 * Set up the write gatherer so that writes to it get dumped to the DANG.
 *
 * This is a tweeked version of the init stuff in the ARCS lib/libsk
 */
static void
*ide_alloc_gfxspace(int pfn)
{
        unsigned        va,old_pgmask;
        unsigned int    pteevn, pteodd;
        #define IOMAP_TLBPAGEMASK       0x1ffe000
        va = (unsigned)0xf0000000;

        pteevn = ((pfn) << 6) | (2 << 3) | (1 << 2) | (1 << 1) | 1;
        pteodd = 0;

        old_pgmask = get_pgmask();
        set_pgmask(IOMAP_TLBPAGEMASK);
        tlbwired(0,0,(caddr_t)va,pteevn,pteodd);  /* Use entry 0 only, it is reserved for graphics */
        set_pgmask(old_pgmask);

        return((void *)va);
}

void *
ide_dang_init(int slot, int adap)
{
    volatile long long *dang_ptr;
    volatile long long reg;
    int window, io_config_reg;
    void * va;

    ide_init_tlb();

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);
    dang_ptr = (long long *) SWIN_BASE(window, adap);

#if defined(IP19)
    va = ide_alloc_gfxspace(LWIN_PFN(window,adap)+(0x3000000>>12));
#elif defined(IP21)
    va = (void *)(((long long)(LWIN_PFN(window,adap)) << 12) + 0x9000000003000000);
#elif defined(IP25)
    va = (void *)(((long long)(LWIN_PFN(window,adap)) << 12) + 0x9000000003000000);
#endif

    /*
     * clears up any outstanding errors and 
     * sets up the interrupt vectors for ide
     */
    setup_err_intr(slot, adap);

    /*
     * Set up GIO address registers - these are the default values
     */
    store_double((long long *)&dang_ptr[DANG_UPPER_GIO_ADDR],(long long)7);
    store_double((long long *)&dang_ptr[DANG_MIDDLE_GIO_ADDR],(long long)0);

    /*
     * make dang quit sending to GIO when GIO interrupts the dang
     */
    store_double((long long *)&dang_ptr[DANG_INTR_BREAK], (long long) 4);

    /*
     * set up the FIFO registers
     */
    store_double((long long *)&dang_ptr[DANG_WG_LOWATER], (long long) 32);
    store_double((long long *)&dang_ptr[DANG_WG_HIWATER],(long long)1024);
    store_double((long long *)&dang_ptr[DANG_WG_FULL], (long long)0xfff);

    /*
     * For now allow user process no access to addresses below 0x3000
     * in shared ram - this allows the priviliged exception code a
     * chance to work
     */
    store_double((long long *)&dang_ptr[DANG_WG_PRIV_LOADDR],
	(long long)3);        /* 5 bits */
    store_double((long long *)&dang_ptr[DANG_WG_PRIV_HIADDR],
	(long long)0x1f);        /* 5 bits */

#ifdef NEVER
    store_double((long long *)&dang_ptr[DANG_WG_PRIV_HIADDR],
	(long long)0x1f);     /* 5 bits - all gr2 memory allowed */
#endif

    /* defines used by wg test code - send to gr2 shram */
    store_double((long long *)&dang_ptr[DANG_WG_GIO_UPPER],
	(long long)0xf80);       /* Upper 15 bits of 0x1f000000 */
    store_double((long long *)&dang_ptr[DANG_WG_GIO_STREAM],
	(long long)0x1f000000);  /* GIO addr for streaming mode (32bits) */

#ifdef NEVER
    /* defines used by os calls */
    store_double((long long *)&dang_ptr[DANG_WG_GIO_UPPER],
	(long long)0xf82);       /* Upper 15 bits of 0x1f040000 */
    store_double((long long *)&dang_ptr[DANG_WG_GIO_STREAM],
	(long long)0x1f040000);  /* GIO addr for streaming mode (32bits) */
#endif

    store_double((long long *)&dang_ptr[DANG_GIO64],
	(long long)0); /* GIO 32 */


    /*
     * Setup the IO4 configuration register since we use the Write Gatherer
     * The individual bits mean :
     * 		DANG IOA present, it is a GFX IOA, and is assigned WG reg 1
     */
    io_config_reg = IO4_GETCONF_REG(slot, IO4_CONF_ADAP);
    io_config_reg |= (0x10101 << adap);
    IO4_SETCONF_REG(slot,IO4_CONF_ADAP,io_config_reg);

    /*
     * Set up write gatherer
     */
#if defined(IP19)
    store_double((long long *)EV_WGCNTRL,
	(long long)2);     			/* Big endian, disabled */
    store_double((long long *)EV_WGCLEAR,
	(long long)0);     			/* Clear WG */
#endif
    reg = (long long)(LWIN_PFN(window,adap));
    reg <<= 12;
    reg |= (cpuid() << 8) | (1 << 15);
#if !defined(IP25)
    store_double((long long *)EV_WGDST,reg);
#else
    store_double((long long *)EV_GRDST,reg);
#endif
						/* Set up destination */
#ifdef IP19
    store_double((long long *)EV_WGCNTRL,
	(long long)3); 				/* Big endian, enabled */
#endif

    return (va) ;
}

int
ide_Gr2Probe(struct gr2_hw * gr2ptr)
{
    int retval;

    if (badaddr(&(gr2ptr->hq.mystery), sizeof(gr2ptr->hq.mystery))) {
	msg_printf(VRB,"\tinvalid gr2 address 0x%llx\n", (long long)gr2ptr);
	retval = FALSE;
    } else if (gr2ptr->hq.mystery != 0xdeadbeef) {
	msg_printf(VRB,
	    "\tgr2ptr->hq.mystery (0x%llx) sb 0xdeadbeef, was 0x%lx\n",
                (long long) &gr2ptr->hq.mystery,
                (long) gr2ptr->hq.mystery);
	retval = FALSE;
    }
    else {
	msg_printf(DBG,"gr2 located at 0x%llx\n", (long long) gr2ptr);
	retval = TRUE;
    }

    return (retval);
}

/*
 * stolen from the standalone library stuff
 */
void ide_Gr2GetInfo(struct gr2_hw *hw, struct gr2_info *info)
{
	int vma, vmb, vmc;
	int GR2version, VBversion;

	msg_printf(DBG, "hw ptr was 0x%llx, info ptr was 0x%llx",
		    (long long) hw, (long long)info);

	GR2version = ~(hw->bdvers.rd0) & 0xf;
	if (!GR2version)	/* Gr2 board not found */
		return;	
	else 
	{
		info->BoardType = GR2_TYPE_GR2;
		
        	/* We found a board */
        	strncpy(info->gfx_info.name, GFX_NAME_GR2, GFX_INFO_NAME_SIZE);
		info->gfx_info.length = sizeof(struct gr2_info);
		
		/* Board rev. number */
			info->GfxBoardRev = GR2version;
		
		/* Video Backend Board rev. number */
		if (GR2version  < 4) { 
			VBversion = (~(hw->bdvers.rd2) & 0xc) >> 2;
		} else {
			VBversion = ~(hw->bdvers.rd1) & 0x3;
		}
				
		if (!VBversion) {	/* Video Backend Board not found */
			return;	
		}
		info->VidBckEndRev = VBversion;

		if (GR2version  < 4) {	

			/* Zbuffer option installed */
			if ((hw->bdvers.rd1 & 0xc) == 0xc)
				info->Zbuffer = 0;
			else
				info->Zbuffer = 1;	

			vma = vmb = vmc = 0;

			/* 8-bit vs 24-bit planes */
			if ((hw->bdvers.rd3 & 0x3) != 0x3)
				vma = 1;
			if ((hw->bdvers.rd3 & 0xc) != 0xc)
				vmb = 1;
			if ((hw->bdvers.rd3 & 0x30) != 0x30)
				vmc = 1;

			/* XXXCheck if VRAMs are skipping any slots or
			 * not installed in order
			 */
			if (vma && vmb && vmc)
				info->Bitplanes = 24;
			else if (vma && vmb)
				info->Bitplanes = 16;	/* Not supportted */
			else
				info->Bitplanes = 8;
		
			info->MonitorType = ((hw->bdvers.rd2 & 0x3) << 1) | (hw->bdvers.rd1 & 0x1);

		} else {
			if ((hw->bdvers.rd1 & 0x20) >> 5)
				info->Zbuffer = 1;
			else
				info->Zbuffer = 0;

			if ((hw->bdvers.rd1 & 0x10) >> 4)
				info->Bitplanes = 24;
			else
				info->Bitplanes = 8;
			info->MonitorType = (hw->bdvers.rd0 & 0xf0) >> 4;
		}
			/* Only 1 option for EXPRESS */
			info->Auxplanes = 4;
			info->Wids = 4;

		if (info->MonitorType == 6) 
		{
			/* We don't support this configuration */
			info->gfx_info.xpmax = 1024;
                	info->gfx_info.ypmax = 768;
			/* We don't support this configuration */
		} else {
			info->gfx_info.xpmax = 1280;
                	info->gfx_info.ypmax = 1024;
		}
	}
}

/******************************************************************************
 *
 * Set up configuration register and start the clock.
 *
 *****************************************************************************/
unsigned char clk107B[] = {
	0xc4,0x0f,0x15,0x20,0x30,0x4c,0x50,0x60,
	0x70,0x80,0x91,0xa5,0xb6,0xd1,0xe0,0xf0,0xc4};

unsigned char PLLclk132[] = {0x00,0x00,0x00,0x04,0x05,0x04,0x06};
unsigned char PLLclk107[] = {0x00,0x10,0x00,0x15,0x18,0x01,0x0F};
static int ide_Gr2StartClock(register struct gr2_hw  *base, struct gr2_info *info)
{
	register int i,j,PLLbyte;

   	if (info->GfxBoardRev < 4) {
		if ((info->MonitorType & 0x7) == 1) {  /* 72 HZ */
                /* Select the 132 MHz crystal*/
                	base->bdvers.rd0 = 0x2;
        	} else if  ((info->MonitorType & 0x7) != 6) { /* 60 HZ */
                /* Select the 107.352 MHz crystal*/
                	base->bdvers.rd0 = 0x1;
        	}

		/* Reset the VC1 chip
	 	 * Must give VC1 time to reset. Don't put this right before
	 	 * taking the VC1 out of reset
	 	 */
		base->bdvers.rd1 = 0x00; 

	        /* 
 	  	 * Must always initialize programmable clock, even if not
		 * being used, else will run at max speed, and may affect
		 * system. 
	 	 */

		if ((info->MonitorType & 0x7) != 6) { /* 1280 x 1024 */
			for (i=0; i<sizeof(clk107B); i++)
				base->clock.write = clk107B[i];
		}

		/* Take the VC1 out of reset and set configuration register 
		 *in expanded backend mode
		 */
		base->bdvers.rd1 = 0x40; 

	} else { /* ultra, dual-head boards */

		base->bdvers.rd0 = 0x47;
		if ((info->MonitorType & 0x7) == 1) {  /* 72 HZ */
		     for (i=0; i<7; i++) { 
			PLLbyte = PLLclk132[i];
			for (j=0;j<8;j++) {
				if ((j == 7) && (i== 6)) 	
					base->clock.write = (PLLbyte & 0x1) | 0x2;
				else 
					base->clock.write = PLLbyte & 0x1;
				PLLbyte = PLLbyte >> 1;
			} 
		     }
		} else if  ((info->MonitorType & 0x7) != 6) { /* 60 HZ */ 
		     for (i=0; i<7; i++) { 
			PLLbyte = PLLclk107[i];
			for (j=0;j<8;j++) {
				if ((j == 7) && (i== 6)) 	
					base->clock.write = (PLLbyte & 0x1) | 0x2;
				else 
					base->clock.write = PLLbyte & 0x1;
				PLLbyte = PLLbyte >> 1;
			} 
		    }
		}

		base->bdvers.rd1 = 0x00; 
		us_delay(3);
		base->bdvers.rd1 = 0x1;
    	}
}

int
ide_Gr2Config(long long *dang_ptr,
	      struct gr2_hw * gr2ptr,
	      struct gr2_info *info)
{
    /* reset the graphics first in case ucode running */
    store_double((long long *)&dang_ptr[DANG_GIORESET],(long long)0);
    us_delay(10);
    store_double((long long *)&dang_ptr[DANG_GIORESET],(long long)1);

    /* get the configuration info */
    ide_Gr2GetInfo(gr2ptr, info);

    /*
     * start the clock and do basic setup
     */
    ide_Gr2StartClock(gr2ptr, info);
}

     


/*
 * returns the number of GEs present
 * code lifted from the IP20 diags
 */
int
ide_Gr2ProbeGEs(struct gr2_hw *base)
{
    int i, numge;

    numge = 1;
    for (i=1; i<8; i++) {
	/* Probing whether we have 1 or 4 or 8 GE's installed */
	base->ge[i].ram0[0] = 0x1f1f1f1f;
	base->ge[i].ram0[31] = 0x2e2e2e2e;
	base->ge[i].ram0[63] = 0x3d3d3d3d;
	base->ge[i].ram0[95] = 0x4c4c4c4c;
	base->ge[i].ram0[127] = 0x5b5b5b5b;

	if (base->ge[i].ram0[0] == 0x1f1f1f1f &&
	    base->ge[i].ram0[31] == 0x2e2e2e2e &&
	    base->ge[i].ram0[63] == 0x3d3d3d3d &&
	    base->ge[i].ram0[95] == 0x4c4c4c4c &&
	    base->ge[i].ram0[127] == 0x5b5b5b5b)
	    numge = i+1;

    }
    return numge;
}

/*
 * given the base buffer address, the base pfn address, and the number of
 * pfns to make, builds the array of 28 bit pfn's from the 40 bit value given
 */
static void build_pfn(char * buf, int *pfn, int numpfn)
{
    int i;

    for (i = 0; i < numpfn; i++) {
	pfn[i] = (int) (((unsigned long) K1_TO_PHYS(buf) >> 12) + i);
    }
}

/*
 * dma xfer setup setup routine
 * code adapted from the gfx library code for dang
 * simplified, in that we don't have to do the funny one-line-at-a-time
 * workaround for backwards strides, but more complex in that we can do
 * multiple-line dma in a single xfer, and all static, sync, and intr-on-
 * completion are programable
 */
void
ide_Gr2mkudmada(
    char *buf,			/* virtual address of data */
    gdmada_t *dmadap,		/* DMA desc. array ptr */
    int rows,			/* number of rows */
    int bytesperrow,		/* bytes to transfer  in one row */
    int bytesperstride,		/* bytes to stride in one row */
    int offset,			/* offset to first bytes to xfer */
    unsigned int direction,	/* 0: GIO to IBUS, 1: IBUS to GIO */
    unsigned int gio_static,	/* 0: free-running GIO addr, 1: GIO fixed */
    unsigned int dma_sync,	/* 0: no sync needed, 1: sync for gio dma */
    unsigned int dma_intr,	/* 0: no completion intr, 1: intr on comp */
    void *gfxaddr)
{
    int npages, chunksiz, i;

    /*
     * figure the total memory usage and the number of pages needed
     * there is a limit on this routine - for now, coded to allow
     * a maximum of IDE_PGSPERSHOT (32) pages at one time.  This can
     * be bumped if need be, but should be enough for testing
     */
    chunksiz = rows * (bytesperstride + bytesperrow);
    npages = (chunksiz / DIAG_GFX_PAGESIZE) + 1;
    if (npages > IDE_PGSPERSHOT)
	npages = IDE_PGSPERSHOT;


    /* fill in the dma descriptor */
    dmadap->stride = bytesperstride;
    dmadap->bytecount = bytesperrow;
    dmadap->linecount = rows;
    dmadap->lsb_addr = offset;
    dmadap->control =   (gio_static ? GIO_ADDR_STATIC :  GIO_ADDR_INCR) |
			(direction ? GDMA_HTOG : GDMA_GTOH) |
			(dma_sync ? DANG_DMA_SYNC : DANG_NODMA_SYNC) |
			(dma_intr ? DANG_DMA_INTR : DANG_DMA_NOINTR);
    dmadap->GIOaddr = ((__uint32_t)gfxaddr + offset);

    /*
     * fix the active pfns
     */
    build_pfn(buf, (int *)&dmadap->ptrbuf[0], npages);

    /*
     * zero the unused pfns
     */
    for (i = npages; i < IDE_PGSPERSHOT; i++)
	dmadap->ptrbuf[i] = 0x0;
}

int
ide_GR2DMAtrigger(
    long long 			*adptr,
    struct gr2_hw		*base,
    void 			*dmahead)
{
    long long physaddr;
    volatile unsigned int sync;

    physaddr = (long long) K1_TO_PHYS(dmahead);

    /* flush wg and drain dang here? may not need since testing */
#if defined(NEVER)
    msg_printf(DBG, "DMA header physical address is %llx\n",
		(long long)physaddr);
    msg_printf(DBG, "int size is %d\n", sizeof(int));
    msg_printf(DBG, "short size is %d\n", sizeof(short));
    FLUSHWG(1,adptr);
    DRAINDANG(adptr);
#endif

    /* Inactivate the DMA sync source */
    base->hq.dmasync = HQ2_DMASYNC_INACTIVE;

    /*
     * Reset ready for DMA flag set by ucode.
     * Must be done before send down DMA tokens.
     */
    base->hq.fin2 = 0;

    /*
     * sync with hq2 hardware by reading
     */
    sync = base->hq.mystery;

#if defined(NEVER)
    /*
     * Wait for DMA sync to go active before kicking off DMA
     * if takes too long, kick it off anyhow, even though it
     * will probably fail
     */
    sync = 1;
    while ( (load_double_lo((long long *)&adptr[DANG_INTR_STATUS]) &
	    (1 << 11)) == 0)
    {
	sync++;
	if (sync > 10000)
	    break;
	us_delay(10);
    }
#endif

    /*
     * kick off the DMA now, ready or not
     */
    store_double((long long*)&adptr[DANG_DMAM_START],
		(long long) physaddr);
}

/*
 * print out the header info - this is for debugging routines
 */
void
print_gdmada_t(gdmada_t * dmadap)
{
   int i;

   msg_printf(DBG, "gdmada_t contents:\n");
   msg_printf(DBG, "stride:             0x%x\n", dmadap->stride);
   msg_printf(DBG, "bytecount:          0x%x\n", dmadap->bytecount);
   msg_printf(DBG, "linecount:          0x%x\n", dmadap->linecount);
   msg_printf(DBG, "lsb_addr:           0x%x\n", dmadap->lsb_addr);
   msg_printf(DBG, "wait for dma sync:  %s\n",
	    ((dmadap->control & DANG_DMA_SYNC) ? "TRUE" : "FALSE"));
   msg_printf(DBG, "int on complete:    %s\n",
	    ((dmadap->control & DANG_DMA_INTR) ? "TRUE" : "FALSE"));
   msg_printf(DBG, "transfer direction: %s\n",
	    ((dmadap->control & GDMA_HTOG) ? "Host to Gfx" : "Gfx to Host"));
   msg_printf(DBG, "static gio address: %s\n",
	    ((dmadap->control & GIO_ADDR_STATIC) ? "TRUE" : "FALSE"));
   msg_printf(DBG, "GIO addr:           0x%x\n", dmadap->GIOaddr);

   /*
    * print all nonzero and first zero pfn
    */
   for (i = 0; i < IDE_PGSPERSHOT; i++)
   {
       msg_printf(DBG, "pfn[%2d]:            0x%x\n", i, dmadap->ptrbuf[i]);
       if (!dmadap->ptrbuf[i])
	    break;
   }
}
