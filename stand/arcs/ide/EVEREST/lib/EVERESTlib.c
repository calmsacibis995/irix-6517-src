/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, 1994 Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifdef EVEREST	/* whole file */

#include <sys/types.h>
#include <sys/debug.h>
#include <setjmp.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/immu.h> 
#include <sys/param.h>
#include <sys/iotlb.h> 
#include "libsk.h"
#include <everr_hints.h>
#include <stdarg.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/dang.h>
#include <sys/wd95a.h>
#include <ide_msg.h>
#include <ide_s1chip.h>
#include <fault.h>
#include <stringlist.h>
#include <prototypes.h>


#ident	"arcs/ide/EVEREST/lib/EVERESTlib.c:  $Revision: 1.65 $"

/* Additional utilities used by IDE test cases. */
#define	ASSERT_CFG	ASSERT(EVCFGINFO->ecfg_magic == EVCFGINFO_MAGIC)
#define	CHECK_SLOT(x)	if((x < 0)||(x >=EV_MAX_SLOTS)) return(EVCLASS_NONE)
#define	CHECK_ADAP(x)	if((x < 0)||(x >=IO4_MAX_PADAPS)) return(IO4_ADAP_NULL)

#define	SLOTTYPE(x) 	(EVCFGINFO->ecfg_board[x].eb_type)
#define	IO4_WINDOW(x)	(EVCFGINFO->ecfg_board[x].eb_io.eb_winnum)
#define	IOATYPE(x, y)	(EVCFGINFO->ecfg_board[x].eb_io.eb_ioas[y].ioa_type) 
#define	IOASTRU(x, y)	&(EVCFGINFO->ecfg_board[x].eb_io.eb_ioas[y]) 

#define	IDE_ERR_VECTOR	0x3f00	 /* Vector 0x3f << 8 */

#define DANG_ADAP	1	/* vector all DANG exceptions to same handler */

extern int clear_bustags(void);
extern void ev_perr(int, char *, ...);
extern void ip_error_clear(int);
extern void ip_error_log(int);
extern void ip_error_show(int);
extern int cc_error_log(void);
extern int inval_tlbentry(uint);

int
board_type(int slot)
{
    unsigned	stype;
    ASSERT_CFG;
    CHECK_SLOT(slot);
    if ((stype = SLOTTYPE(slot)) && hw_boardfound(slot, stype))
	return(stype);

    return(EVCLASS_NONE);

}

int
adap_type(int slot, int anum)
{
    int 	atype;
    ASSERT_CFG;
    CHECK_SLOT(slot);
    CHECK_ADAP(anum);

    if ((SLOTTYPE(slot) != EVTYPE_IO4) && (SLOTTYPE(slot) != EVTYPE_IO5))
	return(IO4_ADAP_NULL);

    if ((atype = IOATYPE(slot, anum)) && hw_adapfound(slot, anum, atype))
	return(atype);

    return(IO4_ADAP_NULL);
}

/* Check if the Hardware config regs have the same idea as the EVCONFIG!! */

/* Few macros borrowed from IP19prom/io4_config.c */
#define EBOARD(_c, _s)           ((_c) & (1 << (_s)))
#define CPUBOARD(_c, _s)        ((_c) & (1 << ((_s) + 16)))
#define MEMBOARD(_c, _s)        ((_c) & (1 << (_s)))
#define	IOBOARD(_c, _s)		(EBOARD(_c, _s) && !CPUBOARD(_c, _s) && !MEMBOARD(_c, _s))

static caddr_t   unknown_name = {"UNKNOWN"};
static caddr_t boardnames[]= {
	"IP19", "IP21", "MC3", "IO4", "IP25"
};
static caddr_t adapnames[] = {
	"EPC", "SCSI", "FCHIP", "VMECC", "FCG"
};


caddr_t
adap_name(int adap_type)
{
    switch(adap_type){
    case IO4_ADAP_EPC  : return(adapnames[0]); break;
    case IO4_ADAP_SCSI : return(adapnames[1]); break;
    case IO4_ADAP_FCHIP: return(adapnames[2]); break;
    case IO4_ADAP_VMECC: return(adapnames[3]); break;
    case IO4_ADAP_FCG  : return(adapnames[4]); break;
    case IO4_ADAP_NULL : return(unknown_name); break;
    }

}

int
hw_boardfound(int slot, int boardtype)
{
    evreg_t	config = EV_GET_REG(EV_SYSCONFIG);
    uint	confhi, conflo;
    caddr_t	btype;

    conflo = config & 0xffffffff;
    confhi = config >> 32;

    if(((boardtype == EVTYPE_IP21) && CPUBOARD(conflo, slot)) ||
       ((boardtype == EVTYPE_IP25) && CPUBOARD(conflo, slot)) ||
       ((boardtype == EVTYPE_IP19) && CPUBOARD(conflo, slot)) ||
       ((boardtype == EVTYPE_MC3)  && MEMBOARD(confhi, slot)) ||
       ((boardtype == EVTYPE_IO4)  && !CPUBOARD(conflo, slot) && 
					!MEMBOARD(confhi, slot)))
	   return(1);

    msg_printf(ERR, "Bad data seems to be in EVCONFIG data structure !!\n"); 

    if (CPUBOARD(config, slot)){
#if	IP19
	btype = boardnames[0];
#endif
#if	IP21
	btype = boardnames[1];
#endif
#if	IP25
	btype = boardnames[4];
#endif
    }
    else if (MEMBOARD(config, slot))
	btype = boardnames[2];
    else if (EBOARD(config, slot))
	btype = boardnames[3];
    else  btype = unknown_name;

    msg_printf(ERR,"Slot %d has <%s> board instead of <%s> found in EVCONFIG\n",
		slot, btype, board_name(boardtype));

    msg_printf(ERR,"Inconsistent/Corrupted EVCONFIG.. Please reboot\n");
    return(0);

}

int
hw_adapfound(int slot, int padap, int adaptype)
{
    unsigned	iodev;

    if (!hw_boardfound(slot, EVTYPE_IO4))
	return(0);

    if (padap & 0x4)
        iodev = IO4_GETCONF_REG(slot, IO4_CONF_IODEV1);
    else 
        iodev = IO4_GETCONF_REG(slot, IO4_CONF_IODEV0);

    iodev = ( iodev >> ((padap & 0x3) * 8)) & 0xff;

    if (iodev == adaptype)
	return 1;

    msg_printf(ERR, "Bad data seems to be in EVCONFIG data structure !!\n"); 
    msg_printf(ERR,"Adapter in IO4 slot %d padap %d is <%s> instead of <%s> found in EVCONFIG\n",
    	slot, padap, adap_name(iodev), adap_name(adaptype));
    msg_printf(ERR,"Inconsistent/Corrupted EVCONFIG.. Please reboot\n");
    return(0);
}

int
io4_window(int slot)
{
    if (board_type(slot) != EVTYPE_IO4)
	return(-1);
    
    return(IO4_WINDOW(slot));
}

int adap_slots(int adap)
{
	/* returns bit-mapped array of slots with specified io4 adapter */
        register int slot, anum;
        register uint slots = 0x0;
	register uint atype;

        for (slot = 0; slot < EV_MAX_SLOTS; slot++)
                if (board_type(slot) == EVTYPE_IO4)
			for (anum = 0; anum < IO4_MAX_PADAPS; anum++) {
				atype = adap_type(slot, anum);
				if (atype == adap)
                        		slots |= (1 << anum);
			}
        return (slots);
}


int 
fmaster(int slot, int anum)
/* Return : IO4_ADAP_NULL OR IO4_ADAP_VMECC OR  IO4_ADAP_FCG */
{
    unsigned window;
    __psunsigned_t      swin;

    if (adap_type(slot, anum) != IO4_ADAP_FCHIP)
	return(IO4_ADAP_NULL);

    window = IO4_WINDOW(slot);
    swin   = SWIN_BASE(window, anum);
    return(EV_GET_REG(swin + FCHIP_MASTER_ID));
}


int 
scsichip(int slot, int anum)
/* Return Value  : S1_H_ADAP_95A, S1_H_ADAP_93B */
{
    /* TBD */
}


/* Error Clear Registers */

extern void everest_error_clear(); /* Defined in everror.c */

void
cpu_err_clear()
/* Description : Clear CC and A chip error regs on IP where test is running.*/
{
    int slot;
    slot = ((unsigned)(EV_GET_REG(EV_SPNUM) & EV_SLOTNUM_MASK)>>EV_SLOTNUM_SHFT);

    ip_error_clear(slot);
    EV_SET_REG(EV_CERTOIP, 0xffff);
}

int 
mem_err_clear(int slot_arr )
/* Description   : Clear the error registers on the Memory Boards.
     slot_arr    : bitmapped array of MC3 slots. 
		   'i'th bit set for MC3 in slot 'i'
		   Clear this CPU's error register. 
*/
{
    int 	i, result=0;

    ASSERT_CFG;

    for (i=1; i < EV_MAX_SLOTS; i++){
	if ((slot_arr & (1 << i)) == 0)
	    continue;

        if (SLOTTYPE(i) != EVTYPE_MC3){
	    result |= i;
	    continue;
	}
	mc3_error_clear(i);
    }
    cpu_err_clear();

    return(result);
}

int 
io_err_clear(int slot, int adap_arr)
/*
 * Description: Clear error registers for IO4 in 'slot' 
	        Clear error registers for the adaps specified by adap_arr 
	        adap_arr : bit 'i'  set for adapter no 'i'

		Clears error registers in ALL Memory boards, and 
		Clears error registers on the IP19 where test is running. 
 */
{
    int		i, result=0;

    ASSERT_CFG;
    CHECK_SLOT(slot);

    if (SLOTTYPE(slot) !=  EVTYPE_IO4)
	return(-1);

    io4_error_clear(slot);

    for (i=0; i < IO4_MAX_PADAPS; i++){
	if ((adap_arr & (1 << i)) == 0)
	    continue;
	
	adap_error_clear(slot, i);
    }

    for (i=0; i < EV_MAX_SLOTS; i++)
	if (SLOTTYPE(slot) == EVTYPE_MC3)
	    result != (1 << i);

    mem_err_clear(result);

    return(0);
}



void 
cpu_err_show()
/* Description : Display the error registers in the running IP19 board */
{
    int slot;
    
    slot = ((unsigned)(EV_GET_REG(EV_SPNUM) & EV_SLOTNUM_MASK)>>EV_SLOTNUM_SHFT);

    ASSERT_CFG;
    ip_error_show(slot);
}


void
cpu_err_log()
{
    int slot ;
    
    slot = (((unsigned)EV_GET_REG(EV_SPNUM) & EV_SLOTNUM_MASK) >>EV_SLOTNUM_SHFT);

    ip_error_log(slot);
    cc_error_log();
}

void 
mem_err_show()
/* Description : Display error registers in all the MC3 boards in system */
{
    int		i;
    ASSERT_CFG;

    for (i=0; i < EV_MAX_SLOTS; i++)
	if (SLOTTYPE(i) == EVTYPE_MC3)
	    mc3_error_show(i);
    
    cpu_err_show();

}

void
mem_err_log()
{
    int		i;

    for (i=0; i < EV_MAX_SLOTS; i++)
	if (SLOTTYPE(i) == EVTYPE_MC3){
	     mc3_mem_error_log(i);
	     mc3_ebus_error_log(i);
	}


    cpu_err_log();
}

int 
io_err_show(int slot, int adap_arr)
/*
 * Description: Display error registers in CPU board where test is running,
		Error registers on all the MC3 boards, 
		Error registers on the IO4 board in 'slot', and 
		Error registers in the adapters specified by adap_arr
 */
{
    int		i;

    ASSERT_CFG;
    CHECK_SLOT(slot);

    if (SLOTTYPE(slot) != EVTYPE_IO4)
	return(-1);

    io4_error_show(slot);

    for (i=0; i < IO4_MAX_PADAPS; i++)
	if( adap_arr & (1 << i))
	    adap_error_show(slot, i);
    
    mem_err_show();
}


io_err_log(int slot, int adap_arr)
{
    int		i;

    CHECK_SLOT(slot);

    if (SLOTTYPE(slot) != EVTYPE_IO4)
	return(-1);
    
    io4_error_log(slot);

    for(i=0; i < IO4_MAX_PADAPS; i++)
	if (adap_arr & (1 << i))
	    adap_error_log(slot, i);
    
    mem_err_log();
}
/* TLB Entry 0 could be used by the Graphics people. So try leaving 
 * entry 0 and 1 free
 */
#define	FIRST_TLB_ENTRY		2
void 
ide_init_tlb()  
/* Description: Initialize all the TLB entries. */
{
    int		i;

#if !defined(TFP)
    /*
     * added to synch with the private tlb setup stuff in lib/libsk/ml
     */
    for (i = 0; !inval_tlbentry(i); i++);

    for(i=FIRST_TLB_ENTRY; i < NTLBENTRIES; i++)
	invaltlb(i);
#else
    flush_tlb(0);
#endif

}

/* returns the s1num (set up by libsk) based on slot, adap */
int
s1num_from_slotadap(int slot, int adap)
{
	int i, j;
	int s1num = 0;
	int atype;

	msg_printf(DBG,"slot is %d and adap is %d\n", slot, adap);
	for (i = EV_MAX_SLOTS; i > 0; i--) {
		if (board_type(i) == EVTYPE_IO4) { 
			for (j = 1; j < IO4_MAX_PADAPS; j++) {
				atype = adap_type(i, j);
				if (atype == IO4_ADAP_SCSI) {
					msg_printf(DBG,"i: %d, j: %d\n", i,j);
					if ((slot == i) && (adap == j)) {
						msg_printf(DBG,
						  "s1num_from_slotadap: ");
						msg_printf(DBG,"s1num %d\n",
							s1num);
						return (s1num);
					}
					s1num++;
				}
			}
		}
	}
	msg_printf(ERR,"No s1 chips found!\n");
}


/* from evintr.c, pieces of evintr_connect() */
int
s1_intr_setup(__psunsigned_t swin, int my_s1_num)
{
        int arg, dest, intrdest, level, proc, slot;

        arg = my_s1_num;
        proc = (EV_GET_LOCAL(EV_SPNUM) & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
        slot = (EV_GET_LOCAL(EV_SPNUM) & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
        msg_printf(DBG, "Running CPU %d on IP19 in slot %d\n", proc, slot);

        intrdest = (slot << 2) | proc;
        msg_printf(DBG,"intrdest is 0x%x\n", intrdest);

        EV_SET_REG(
                swin + S1_DMA_INTERRUPT,
                EVINTR_VECTOR(
                        EVINTR_LEVEL_S1_CHIP(my_s1_num),
                        intrdest));
        level = S1_GET_REG(swin, S1_DMA_INTERRUPT);
        S1_PUT_REG(swin, S1_DMA_INTERRUPT + S1_DMA_INT_OFFSET, level);
        S1_PUT_REG(swin,S1_DMA_INTERRUPT + 2*S1_DMA_INT_OFFSET, level);
        S1_PUT_REG(swin, S1_INTERRUPT, level);
        msg_printf(DBG,"level is 0x%x\n", level);
}

void
setup_err_intr(int slot, int anum)
/*
 * Description: If the 'slot' has a MC3 board, anum is ignored, and
		the Interrupt vector is setup for MC3. Otherwise
		Setup Intr vector for all memory boards, IO4 board in 'slot'
		and for the defined Adapter, 
    		The error interrupts are targeted to CPU where test runs
 */
{
    int			i, ivect, level, my_s1_num, win, btype;
    evioacfg_t		*ioa;
    long long		pend, cel;
    __psunsigned_t      swin;
    volatile long long *adptr;

#if defined(TFP)
    /* clear possible bad parity errors */
    tfp_clear_gparity_error();
#endif
#if defined(TFP) || defined(IP25)
    /* Disable interrupts on the R8K/R10K processor */
    set_SR(get_SR() & ~(SR_IMASK | SRB_SCHEDCLK | SRB_ERR | SR_IE | SR_EXL)); 
#else
    /* Disable interrupts on the R4k processor */
    set_SR(get_SR() & ~SR_IE & ~(SR_EXL | SR_ERL)); 
#endif

    ASSERT_CFG;

    if ((slot < 0) || (slot >= EV_MAX_SLOTS))
	return;

    ivect = (EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK) | IDE_ERR_VECTOR;

    switch(btype = SLOTTYPE(slot)){

    case EVTYPE_IP19: break;

    case EVTYPE_MC3 : MC3_SETREG(slot, MC3_EBUSERRINT, ivect); break;

    case EVTYPE_IO4 : 
	ivect = EVINTR_VECTOR(EVINTR_LEVEL_IO4_ERROR,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
	IO4_SETCONF_REG(slot, IO4_CONF_INTRVECTOR, ivect);
	IO4_SETCONF_REG(slot, IO4_CONF_INTRMASK, 0x2);

	/*
	 * clear any pending errors
	 */
	i = EV_GET_CONFIG(slot, IO4_CONF_IBUSERRORCLR);
	i = EV_GET_CONFIG(slot, IO4_CONF_EBUSERRORCLR);


	win  = IO4_WINDOW(slot);
	ioa  = IOASTRU(slot, anum); 
	swin = (__psunsigned_t)SWIN_BASE(win, anum);
	adptr = (long long *)swin;		/* for dang, mostly */

	/*
	 * 
	 * anum == 0 means set up the io4 ia/id stuff only
	 */
	if (anum) {

	    switch(ioa->ioa_type){

	    case IO4_ADAP_FCHIP:
		EV_SET_REG(swin+FCHIP_INTR_RESET_MASK, 0x1);
		EV_SET_REG(swin+FCHIP_INTR_MAP, ivect);

		if (EV_GET_REG(swin+FCHIP_MASTER_ID) == IO4_ADAP_VMECC){
		    EV_SET_REG(swin+VMECC_INT_ENABLESET, 0x1);
		    EV_SET_REG(swin+VMECC_VECTORERROR, ivect);
		}
		/* Need code for FCG case */
		break;
	    
	    case IO4_ADAP_SCSI: 
		my_s1_num = s1num_from_slotadap(slot, anum);	
		s1_intr_setup(swin, my_s1_num);
		level = S1_GET_REG(swin, S1_DMA_INTERRUPT);
        	S1_PUT_REG(swin, S1_DMA_INTERRUPT + S1_DMA_INT_OFFSET, level);
        	S1_PUT_REG(swin,S1_DMA_INTERRUPT + 2*S1_DMA_INT_OFFSET, level);
        	S1_PUT_REG(swin, S1_INTERRUPT, level);
		break;

	    case IO4_ADAP_EPC : 
		/*
		 * currently, clears existing errors, initializes the interrupt
		 * registers and mask for the duarts and I/O errors
		 */
		EV_SET_REG(swin + EPC_IMRST, 0xFF);
		EV_SET_REG(swin + EPC_IERR, 0x1000000);
		EV_GET_REG(swin + EPC_IERRC);

		ivect = EVINTR_VECTOR(EVINTR_LEVEL_EPC_DUART0,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
					
/*		msg_printf(DBG, "EPC DUART 0 ivect was %x\n", ivect); */
		EV_SET_REG(swin + EPC_IIDDUART0, ivect);

		ivect = EVINTR_VECTOR(EVINTR_LEVEL_EPC_DUART1,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
/*		msg_printf(DBG, "EPC DUART 1/2 ivect was %x\n", ivect); */
		EV_SET_REG(swin + EPC_IIDDUART1, ivect);

		ivect = EVINTR_VECTOR(EVINTR_LEVEL_EPC_PPORT,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
/*		msg_printf(DBG, "EPC PPORT  ivect was %x\n", ivect); */
		EV_SET_REG(swin + EPC_IIDPPORT, ivect);

		ivect = EVINTR_VECTOR(EVINTR_LEVEL_EPC_ERROR,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
/*		msg_printf(DBG, "EPC ERROR ivect was %x\n", ivect); */
		EV_SET_REG(swin + EPC_IIDERROR, ivect);

		ivect = EVINTR_VECTOR(EVINTR_LEVEL_EPC_PROFTIM,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
/*		msg_printf(DBG, "EPC PROFTIM ivect was %x\n", ivect); */
		EV_SET_REG(swin + EPC_IIDPROFTIM, ivect);

		ivect = EVINTR_VECTOR(EVINTR_LEVEL_EPC_ERROR,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
/*		msg_printf(DBG, "EPC PROFTIM ivect was %x\n", ivect); */
		EV_SET_REG(swin + EPC_IIDSPARE, ivect);

		i = (int) EV_GET_REG(swin + EPC_ISTAT) |
		    EPC_INTR_DUART0 | EPC_INTR_DUART12 |
		    EPC_INTR_ERROR | EPC_INTR_PROFTIM  |
		    EPC_INTR_PPORT | EPC_INTR_SPARE;
		EV_SET_REG(swin + EPC_IMSET, i);
		break;

	    case IO4_ADAP_DANG : 

		/*
		 * currently, clears existing errors after disabling all
		 * possible DANG interrupt sources. The interrupt vectors
		 * all map to the same location, but it is up to the
		 * test code to enable an interrupt source and interpret
		 * the interrupt cause.
		 */

		i = (int)load_double((long long*)&adptr[DANG_PIO_ERR_CLR]);
		i = (int)load_double((long long*)&adptr[DANG_DMAM_ERR_CLR]);
		i = (int)load_double((long long*)&adptr[DANG_DMAS_ERR_CLR]);
		i = (int)load_double((long long*)&adptr[DANG_WG_PRIV_ERR_CLR]);
		i = (int)load_double(
			(long long*)&adptr[DANG_DMAM_COMPLETE_CLR]);

		/*
		 * disable all interrupt sources
		 */
		i = (int) DANG_IENA_CLEAR | DANG_ISTAT_DCHIP |
		    DANG_ISTAT_DMAM_CMP | DANG_ISTAT_WG_FLOW |
		    DANG_ISTAT_WG_FHI | DANG_ISTAT_WG_FULL |
		    DANG_ISTAT_WG_PRIV | DANG_ISTAT_GIO0 |
		    DANG_ISTAT_GIO1 | DANG_ISTAT_GIO2;
		store_double((long long*)&adptr[DANG_INTR_ENABLE],
			     (long long) i);

		/*
		 * DANG interrupts will all use the same handler in the
		 * standalones, all the error interrupt vectors point to
		 * the same location - kinda kludgy, when you consider the
		 * EPC has scads o' vectors, but that is the model found
		 * in the kernel and evintr.h
		 * GIO has its own vector, and DMA comes in at the calculated
		 * vector
		 *
		 * yes, invects are built funny here - have to reverse for
		 * some reason
		 */

		ivect = EVINTR_VECTOR(EVINTR_LEVEL_DANG_GIO,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
		store_double((long long*)&adptr[DANG_INTR_GIO_0],
			     (long long) ivect);
		ivect = EVINTR_VECTOR(DANG_INTR_LEVEL(1),
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
		store_double((long long*)&adptr[DANG_INTR_DMAM_COMPLETE],
			     (long long) ivect);
		ivect = EVINTR_VECTOR(EVINTR_LEVEL_DANG_ERROR,
			(EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK));
		store_double((long long*)&adptr[DANG_INTR_PRIV_ERR],
			     (long long) ivect);
		store_double((long long*)&adptr[DANG_INTR_PAUSE],
			     (long long) ivect);
		store_double((long long*)&adptr[DANG_INTR_BREAK],
			     (long long) ivect);

		/* set only dest for these - rest comes in at wg */
		ivect = EVINTR_LEVEL_DANG_ERROR;
		store_double((long long*)&adptr[DANG_INTR_LOWATER],
			     (long long) ivect);
		store_double((long long*)&adptr[DANG_INTR_HIWATER],
			     (long long) ivect);
		store_double((long long*)&adptr[DANG_INTR_FULL],
			     (long long) ivect);

		i = (int)load_double((long long*)&adptr[DANG_PIO_ERR_CLR]);
		i = (int)load_double((long long*)&adptr[DANG_DMAM_ERR_CLR]);
		i = (int)load_double((long long*)&adptr[DANG_DMAS_ERR_CLR]);
		i = (int)load_double((long long*)&adptr[DANG_WG_PRIV_ERR_CLR]);
		i = (int)load_double(
			(long long*)&adptr[DANG_DMAM_COMPLETE_CLR]);

		break;

	    default	: 
		msg_printf(ERR,
			    "Invalid adapter type %x at slot %d adap-no %d\n",
			    ioa->ioa_type, slot, anum);
		  break;
	    }
	}

	for (i=1; i < EV_MAX_SLOTS; i++){
	    if (SLOTTYPE(i) != EVTYPE_MC3)
		continue;
	    MC3_SETREG(i, MC3_EBUSERRINT, ivect);
	}
	break;

    default:
	msg_printf(ERR, "Invalid Board type %d in slot %d\n",btype, slot);
	break;
    }

    /* Some misc stuff */
    /* It is possible that some old interrupts are hanging around.
     * (from initial IP19 PROM launch in particular).  Clean them
     * up.
     */
    for (i = 0; i < EVINTR_MAX_LEVELS; i++)
	    EV_SET_LOCAL(EV_CIPL0, i);
    EV_SET_LOCAL(EV_CIPL124, 0x7);
    EV_SET_LOCAL(EV_IP0, 0);
    EV_SET_LOCAL(EV_IP1, 0);

    /*
     * as long as interrupts still pending, clear them
     */
    while (pend = EV_GET_REG(EV_HPIL)) {
	msg_printf(DBG, "interrupt level %x still pending\n", (int) pend);
	EV_SET_REG(EV_CIPL0, pend);
    }


#if TFP || IP25
    EV_SET_REG(EV_CERTOIP, 0xffff);			/* Clear Bus Error */
    EV_SET_LOCAL(EV_ILE,
	 (EV_EBUSINT_MASK|EV_ERTOINT_MASK));		/* re-enable BE */
#else
    EV_SET_REG(EV_ILE, (EV_GET_REG(EV_ILE) | 0x1));	/* Enable Lev 1 Intr */
#endif

#if TFP || IP25
    /* Disable interrupts on the TFP processor */
    set_SR(get_SR() & ~(SR_IMASK | SRB_SCHEDCLK | SRB_ERR | SR_IE | SR_EXL)); 
#else
    /* Disable interrupts on the R4k processor */
    set_SR(get_SR() & ~SR_IE & ~(SR_EXL | SR_ERL)); 
#endif
}

cpu_intr_pending()
{
#if _MIPS_SIM != _ABI64
    __psunsigned_t t0l, t0h, t1l, t1h;
    int retval;

    /*
     * This nastiness is required to work around a compiler bug -
     * some logical operations mask off the upper 32 bits of 64 bit
     * values, even when the values being compared are all 64 bit values
     */
    t0l = load_double_lo(EV_IP0);
    t0h = load_double_hi(EV_IP0);
    t1l = load_double_lo(EV_IP1);
    t1h = load_double_hi(EV_IP1);

    retval = t0l || t0h || t1l || t1h; 
    if (retval)
      msg_printf(DBG,"IP0 lo: 0x%x, IP0 hi: 0x%x, IP1 lo: 0x%x, IP1 hi:0x%x\n",
		t0l, t0h, t1l, t1h);
#else
    long long t0, t1;
    int retval;

    t0 =  load_double((long long *)EV_IP0);
    t1 =  load_double((long long *)EV_IP1);

    retval = t0 || t1;
    if (retval)
	msg_printf(DBG, "IP0: 0x%llx, IP1: 0x%llx\n",
	    (long long) t0, (long long) t1);
#endif
    return(retval);
}

int
clear_err_intr(int slot, int anum)
/* Description: If 'slot' has memory board, ignore anum, clear that vector
		and disable interrupt.
		Otherwise, Clear intr vector in all memory, and IO4 boards in 
		'slot' and the adapter defined by <slot, anum>
 */
{
    int			i, ivect, win, btype;
    __psunsigned_t      swin;
    evioacfg_t		*ioa;
    long long		pend;
    volatile long long * adptr;

    ASSERT_CFG;
    CHECK_SLOT(slot);

#if TFP
    /* clear possible screwed up parity errors */
    tfp_clear_gparity_error();
#endif
#if TFP || IP25
    /* Disable interrupts on the R8k/R10k processor */
    set_SR(get_SR() & ~(SR_IMASK | SRB_SCHEDCLK | SRB_ERR | SR_IE | SR_EXL)); 
#else
    /* Disable interrupts on the R4k processor */
    set_SR(get_SR() & ~SR_IE & ~(SR_EXL | SR_ERL)); 
#endif

    switch(btype = SLOTTYPE(slot)){

    case EVTYPE_IP21: break;

    case EVTYPE_IP19: break;

    case EVTYPE_MC3 : MC3_SETREG(slot, MC3_EBUSERRINT, 0); break;

    case EVTYPE_IO4 : 
	IO4_SETCONF_REG(slot, IO4_CONF_INTRVECTOR, 0);
	IO4_SETCONF_REG(slot, IO4_CONF_INTRMASK, 0x3);/* Int Disable+Err chk */

	/*
	 * clear any pending errors
	 */
	i = EV_GET_CONFIG(slot, IO4_CONF_IBUSERRORCLR);
	i = EV_GET_CONFIG(slot, IO4_CONF_EBUSERRORCLR);

	win  = IO4_WINDOW(slot);
	ioa  = IOASTRU(slot, anum); 
	swin = SWIN_BASE(win, anum);
	adptr = (long long *) swin;		/* dang, mostly */

	/*
	 * 
	 * anum == 0 means set up the io4 ia/id stuff only
	 */
	if (anum) {

	    switch(ioa->ioa_type){

		case IO4_ADAP_FCHIP:
		    EV_SET_REG(swin+FCHIP_INTR_SET_MASK, 0x1);
		    EV_SET_REG(swin+FCHIP_INTR_MAP, 0);

		    if (EV_GET_REG(swin+FCHIP_MASTER_ID) == IO4_ADAP_VMECC){
			EV_SET_REG(swin+VMECC_INT_ENABLESET, 0);
			EV_SET_REG(swin+VMECC_VECTORERROR, 0);
		    }
		    /* Need code for FCG case */
		    break;
		
		case IO4_ADAP_SCSI: 
		    /*
		     * direct steal from setup_err_intr, above
		     */
		    i = s1num_from_slotadap(slot, anum);
		    s1_intr_setup(swin, i);
		    i = S1_GET_REG(swin, S1_DMA_INTERRUPT);
		    S1_PUT_REG(swin,
			 S1_DMA_INTERRUPT + S1_DMA_INT_OFFSET, i);
		    S1_PUT_REG(swin,
			S1_DMA_INTERRUPT + 2*S1_DMA_INT_OFFSET, i);
		    S1_PUT_REG(swin, S1_INTERRUPT, i);
		    break;

		case IO4_ADAP_EPC : 
		    /*
		     * need to add code to clear EPC stuff - this just
		     * disables the various EPC interupts in
		     */
		    i = (int) EV_GET_REG(swin + EPC_ISTAT) |
			EPC_INTR_DUART0 | EPC_INTR_DUART12 |
			EPC_INTR_ERROR | EPC_INTR_PROFTIM  |
			EPC_INTR_PPORT | EPC_INTR_SPARE;
		    EV_SET_REG(swin + EPC_IMRST, i);
		    break;

		case IO4_ADAP_DANG : 
		    /*
		     * currently, clears existing errors, then disables all
		     * possible DANG interrupt sources.
		     */
		    i = (int)load_double((long long*)&adptr[DANG_PIO_ERR_CLR]);
		    i = (int)load_double((long long*)&adptr[DANG_DMAM_ERR_CLR]);
		    i = (int)load_double((long long*)&adptr[DANG_DMAS_ERR_CLR]);
		    i = (int)load_double(
			    (long long*)&adptr[DANG_WG_PRIV_ERR_CLR]);
		    i = (int)load_double(
			    (long long*)&adptr[DANG_DMAM_COMPLETE_CLR]);

		    i = DANG_IENA_CLEAR | DANG_ISTAT_DCHIP |
			DANG_ISTAT_DMAM_CMP | DANG_ISTAT_WG_FLOW |
			DANG_ISTAT_WG_FHI | DANG_ISTAT_WG_FULL |
			DANG_ISTAT_WG_PRIV | DANG_ISTAT_GIO0 |
			DANG_ISTAT_GIO1 | DANG_ISTAT_GIO2;
		    store_double((long long*)&adptr[DANG_INTR_ENABLE],
				 (long long) i);

		    break;

		default	: 
		    msg_printf(ERR,
			"Invalid adapter type %x at slot %d adap-no %d\n", 
				ioa->ioa_type, slot, anum);
		  break;
	    }
	}

	for (i=1; i < EV_MAX_SLOTS; i++){
	    if (SLOTTYPE(i) != EVTYPE_MC3)
		continue;
	    MC3_SETREG(i, MC3_EBUSERRINT, 0);
	}
	break;

    default:
	msg_printf(ERR, "Invalid Board type %d in slot %d\n",btype, slot);
	break;
    }

    /* Some misc stuff */
    /* It is possible that some old interrupts are hanging around.
     * (from initial IP19 PROM launch in particular).  Clean them
     * up.
     */
#if TFP || IP25
    EV_SET_REG(EV_CERTOIP, 0xffff);			/* Clear Bus Error */
#endif
    EV_SET_LOCAL(EV_ILE,
	 (EV_EBUSINT_MASK|EV_ERTOINT_MASK));		/* re-enable BE */

    for (i = 0; i < EVINTR_MAX_LEVELS; i++)
	    EV_SET_LOCAL(EV_CIPL0, i);
    EV_SET_LOCAL(EV_CIPL124, 0x7);
    EV_SET_LOCAL(EV_IP0, 0);
    EV_SET_LOCAL(EV_IP1, 0);

    /*
     * as long as interrupts still pending, clear them
     */
    while (pend = EV_GET_REG(EV_HPIL)) {
	msg_printf(DBG, "Clearing interrupt vector %x\n", (unsigned)pend);
	EV_SET_REG(EV_CIPL0, pend);
    }

    return(0);
}

int banks_populated(int slot, int leaf)
{
        uint leaf0populated = 0x33;
        uint leaf1populated = 0xcc;
        uint populated = 0;

        if (leaf)  {
                populated = leaf1populated & read_reg(slot, MC3_BANKENB);
        }
        else {
                populated = leaf0populated & read_reg(slot, MC3_BANKENB);
        }

        return(populated);

} /* banks_populated */

#define	CPU_ERROR	1
#define	MEM_ERROR	2
#define	IO_ERROR	4

int
check_cpuereg()
/* Return Value : 1 if error registers has any bit set, 0 Otherwise. */

{
    unsigned	a_error, slot;;

    slot =((EV_GET_REG(EV_SPNUM) & EV_SLOTNUM_MASK)>>EV_SLOTNUM_SHFT);
    a_error = EV_GETCONFIG_REG(slot, 0, EV_A_ERROR);
    if (a_error) {
       msg_printf(ERR, "WARNING: ");
       msg_printf(ERR,"CPU in slot %d, A chip register is non-zero: 0x%x\n",
			slot,a_error);
    }
    msg_printf(DBG,"result is %d at loc 5\n", a_error);
    return((a_error) ? CPU_ERROR : 0);
}

int
check_memereg(int slot_arr)
/* slot_arr : 'i'th bit set for MC3 in slot i.
   Return Value : 2 if error register in any of MC3 specified is set | 
		  1 if the CPU error registers have any bit set.
		  0 Otherwise.
*/
{

    int		i,j,k, result=0;
    uint reg;

    for (i=1; i < EV_MAX_SLOTS; i++){
	if ((slot_arr & (1 << i)) == 0)
	    continue;
	
	reg = MC3_GETREG(i, MC3_EBUSERROR);
	if (reg) {
		msg_printf(ERR, "WARNING: ");
		msg_printf(ERR,
			"slot %d, MC3_EBUSERROR reg is non-zero: 0x%x\n", 
			i, reg);
	    	result |= MEM_ERROR; 
	}
	for (j = 0; j < MC3_NUM_LEAVES; j++) {
		reg = MC3_GETLEAFREG(i,j,MC3LF_ERROR);
		if (reg) {
		   msg_printf(ERR, "WARNING: ");
		   msg_printf(ERR,
			"slot %d, MC3LF_ERROR reg:leaf-%d is non-zero: 0x%x\n", 
			i, j, reg);
	    	   result |= MEM_ERROR; 
		}
		reg = MC3_GETLEAFREG(i,j,MC3LF_ERRADDRHI);
		if (reg) {
		   msg_printf(ERR, "WARNING: ");
		   msg_printf(ERR,
                   "slot %d, MC3LF_ERRADDRHI reg: leaf-%d is non-zero: 0x%x\n",
			i, j, reg);
		   result |= MEM_ERROR;
		}
		reg = MC3_GETLEAFREG(i,j,MC3LF_ERRADDRLO);
		if (reg) {
		   msg_printf(ERR, "WARNING: ");
                   msg_printf(ERR,
                   "slot %d, MC3LF_ERRADDRLO reg: leaf-%d is non-zero: 0x%x\n",
                        i, j, reg);
                   result |= MEM_ERROR;
                }
/*
		if (banks_populated(i, j))
		   for (k = 0; k < MC3_BANKS_PER_LEAF; k++) {
			reg = MC3_GETLEAFREG(i, j, MC3LF_SYNDROME0 + k); 
			if (reg) {
		   	   msg_printf(ERR, "WARNING: ");
			   msg_printf(ERR,
                   	   "slot %d, leaf %d, SYNDROME%d is non-zero: 0x%x\n",
					i, j, k, reg);
				result |= MEM_ERROR;
			} 
		   }
*/
	}
	if (result)
	   	break; /* Since there is some error, why continue */

    }
    msg_printf(DBG,"result is %d at loc 4\n", result);
    result |= check_cpuereg();

    return(result);
}

int
check_ioereg(int slot, int adap_arr)
/* adap_arr  : 'i' the bit set for Adapter 'i'
 * Return Value : 4 if error reg in any of adap in IO4 is set |  (Bit ORing) 
                  2 if error register in any of MC3 is set |     (Bit ORing)
		  1 if the CPU error registers have any bit set.
		  0 Otherwise.
 */
{
    int 		i, result=0;
    unsigned		win, mem_arr=0, reg;
    __psunsigned_t      swin;
    evioacfg_t		*ioa;

	reg = IO4_GETCONF_REG(slot, IO4_CONF_IBUSERROR);
	if (reg) {
		msg_printf(ERR, "WARNING: ");
		msg_printf(ERR,
			"slot %d, IO4_CONF_IBUSERROR Reg is non-zero: 0x%x\n",
			slot, reg);
		result |= IO_ERROR;
	}
	reg = IO4_GETCONF_REG(slot, IO4_CONF_EBUSERROR);
	if (reg) {
		msg_printf(ERR, "WARNING: ");
		msg_printf(ERR,
			"slot %d, IO4_CONF_EBUSERROR Reg is non-zero: 0x%x\n",
			slot, reg);
		result |= IO_ERROR;
	}

    msg_printf(DBG,"result is %d at loc 1\n", result);
    if (result == 0){
	for (i=1; i < IO4_MAX_PADAPS; i++){
	    if ((adap_arr & (1 << i)) == 0)
		continue;
	    
	    win  = IO4_WINDOW(slot);
	    ioa  = IOASTRU(slot, i);
	    swin = SWIN_BASE(win, i);

	    switch(ioa->ioa_type){
	    case IO4_ADAP_VMECC: 
		reg = EV_GET_REG(swin+FCHIP_ERROR);
		if (reg) {
			msg_printf(ERR, "WARNING: ");
			msg_printf(ERR,
			   "slot %d, FCHIP_ERROR Reg is non-zero: 0x%x\n",	
				slot, reg);
			result |= IO_ERROR;
		}
		reg = EV_GET_REG(swin+ VMECC_ERRORCAUSES);
		if (reg) {
			msg_printf(ERR, "WARNING: ");
			msg_printf(ERR,
                          "slot %d, VMECC_ERRORCAUSES Reg is non-zero: 0x%x\n",
                                slot, reg);	
			result |= IO_ERROR;
                }
		break;
		
	    case IO4_ADAP_FCG  : break;
	    case IO4_ADAP_SCSI : break; 
	    case IO4_ADAP_EPC  : break; 
	    default	       : break;
	    }

	    if (result)
		break;
	}
    }
    msg_printf(DBG,"result is %d at loc 2\n", result);

    for(i=1; i < EV_MAX_SLOTS; i++)
	if (SLOTTYPE(i) == EVTYPE_MC3)
	    mem_arr |= (1 << i);

    result |= check_memereg(mem_arr);

    msg_printf(DBG,"result is %d at loc 3\n", result);
    return(result);
}

int report_check_error(int fail)
{
	msg_printf(INFO,"WARNING: ");
        if (!fail)
        	msg_printf(INFO,"Test passed, but got an ");
        msg_printf(INFO,"Unexpected ERROR in a register\n");
}


/*
 * Routine to facilitate accessing memory beyond 512 MB
 * Assumptions:
 *	This is NOT a general purpose TLB mapping routine.
 *	This should be used with extreme care.
 *	It works as below
 *	If the address to be mapped is below 512 Mbytes, then either 
 *	K0 or K1 address is returned based on the value of 'cached'
 *
 *	If the address is above 512 Mbytes, then the routine makes 
 *	as many TLB entries as needed to map 'size' bytes. Maximum
 *	size of memory chunk that can be mapped is 512 Mbytes.
 *
 *	Each time the routine is called, it clears all the TLB entries 
 *	
 *	This should be called only when a new chunk of memory needs to 
 *	be mapped by the memory testing program.
 *
 *	FOR ADDRESS > 512MB, 
 *	THIS SHOULD BE CALLED WITH 'addr' at any 16 Mbyte boundary
 *
 *   NOTE: Provides special treatment for uncached memory address space
 *	   mapping to local resource addresses. 
 */
#define	DIRECT_MAP	0x20000000	/* Address < this Needs no mapping */

#define	UNCACH_START	0x00010000
#define	UNCACH_END	0x00020000
#define	NEW_MAPADDR	0x00ff0000	/* 0xff0000000 >> 12 */

unsigned	pfns[64];

__psunsigned_t map_addr(__psunsigned_t addr, int size, int cached)
{
    unsigned 	paddr, pfaddr, offs, npaddr;
    pde_t	pfnevn, pfnodd;
    int		tlbindx;
    __psunsigned_t	vpn, startaddr;
    unsigned int mypaddr;

#ifdef	USE_K0ADDR
    if (addr < (long long)DIRECT_MAP){
	paddr = (unsigned)addr;
	msg_printf(DBG,"cached is %d\n", cached);

	(cached) ? msg_printf(DBG,"cached\n") : msg_printf(DBG,"uncached\n");
	mypaddr = ((cached) ? PHYS_TO_K0(paddr) : PHYS_TO_K1(paddr));
	msg_printf(DBG,"mypaddr is 0x%x and paddr is 0x%x\n", mypaddr, paddr);
	msg_printf(DBG,"k0: 0x%x, k1:0x%x\n", 
		PHYS_TO_K0(paddr), PHYS_TO_K1(paddr));
	return((cached) ? PHYS_TO_K0(paddr) : PHYS_TO_K1(paddr));
    }
#endif

    offs  = (unsigned)(addr & (long long)(IOMAP_VPAGESIZE - 1));
    paddr = (unsigned)(addr & (long long)~(IOMAP_VPAGESIZE - 1)) >> BPCSHIFT;
    tlbindx = FIRST_TLB_ENTRY; 
    /*startaddr = vpn   = IOMAP_BASE + ( tlbindx * IOMAP_VPAGESIZE * 2); */
    startaddr = vpn   = IOMAP_BASE;

    /* ide_init_tlb(); */

    /* Truncate Max mapped size to 512 Mbyts */
    if (size > DIRECT_MAP)
        size = DIRECT_MAP;

    msg_printf(DBG,"addr: 0x%x size = 0x%x cached: %d\n", paddr, size, cached);

    do{
	invaltlb(tlbindx); /* Invalidate required entry */
	/* If uncached, and falls in the local resource area, map it to
	 * the unique address supported by CC Physical 0xff0000000
	 */

	pfns[tlbindx] = paddr;
	/* Two different checkings for those address range which cross 
	 * this boundary . npaddr is used so that paddr still tracks the
	 * actual physical address, and can be used after crossing 512 MB
	 */
        if (!cached && (paddr >= UNCACH_START) && (paddr < UNCACH_END)){
            npaddr = NEW_MAPADDR + (paddr & 0xffff);
	    pfnevn.pgi = npaddr << 6;
        }
	else  pfnevn.pgi = paddr << 6;

	paddr += (IOMAP_VPAGESIZE >> BPCSHIFT);

        if (!cached && (paddr >= UNCACH_START) && (paddr < UNCACH_END)){
            npaddr = NEW_MAPADDR + (paddr & 0xffff);
	    pfnodd.pgi = npaddr << 6;
        }
	else  pfnodd.pgi = paddr << 6;

        if (cached){
            pfnevn.pgi |= 0x1f; pfnodd.pgi |= 0x1f;
        }
        else{
            pfnevn.pgi |= 0x17; pfnodd.pgi |= 0x17;
        }


	set_pgmask(IOMAP_TLBPAGEMASK);	 /* 16 MB pagesize */
#if !defined(TFP)
	tlbwired(tlbindx++, 0, (caddr_t)vpn, pfnevn.pgi, pfnodd.pgi);
#else
	tlbwired(tlbindx++, 0, (caddr_t)vpn, pfnevn.pte);
#endif
	paddr += (IOMAP_VPAGESIZE >> BPCSHIFT);
	size -= (IOMAP_VPAGESIZE * 2);
	vpn  += (IOMAP_VPAGESIZE * 2);
    
    }while(size > 0);

    return((unsigned)(startaddr | offs));

}

long long
vir2phys(unsigned virtaddr)
{
	int		indx;
	long long	pfn;

	if ((virtaddr < IOMAP_BASE) || (virtaddr > (IOMAP_BASE + DIRECT_MAP)))
		return ( -1);

	indx = (virtaddr - IOMAP_BASE)/(IOMAP_VPAGESIZE * 2) ;
	pfn = (long long)pfns[indx+FIRST_TLB_ENTRY] << BPCSHIFT;

	if (virtaddr & IOMAP_VPAGESIZE)
		pfn += IOMAP_VPAGESIZE ;

	return(pfn + (virtaddr & (IOMAP_VPAGESIZE - 1)));
}

extern	catalog_t	*Global_Catalog[];
char	hint_buf[256];

/*VARARGS2*/
err_msg(uint hint, void *physloc, ...)
{
    va_list     ap;
    hint_t     *h;
    catalog_t	**ct;
    uint	*subhints, got_boardtype = 0;

	msg_printf(ERR, "BOARD_HINT_MASK %x  SUBSYS_HINT_MASK %x  hint %x\n", BOARD_HINT_MASK, SUBSYS_HINT_MASK, hint);
    for (ct=Global_Catalog; *ct; ct++){
	subhints = (*ct)->cat_fntable->subsystem;
	msg_printf(ERR, "*subhints %x\n", *subhints);

	if (((*subhints)&BOARD_HINT_MASK) != (hint & BOARD_HINT_MASK))
	    continue;

	got_boardtype = 0;
	for(; *subhints; subhints++){
	    if (((*subhints)&SUBSYS_HINT_MASK) == (hint & SUBSYS_HINT_MASK))
		break;
	}
	if (*subhints)
	   break;	/* Got the needed catalog */
    }

    if (*ct == (catalog_t *)0){
	if (got_boardtype)
	    msg_printf(ERR,"err_msg: No valid sybsystem for hint:%07x\n",hint);
        else 
	    msg_printf(ERR, "err_msg: Bad Board type for Hint: %07x \n", hint);
        return(0);
    }
    h = (*ct)->hints;
    
    for(h=(*ct)->hints; h->hint_num; h++){
        if (h->hint_num == hint)
            break;
    } 

    if (h->hint_num == 0){
	msg_printf(ERR, "err_msg: No Hint msg for Hint: %07x \n", hint);
        return(0);
    }


    msg_printf(ERR, "ERROR %07x: %s FAILED ", h->hint_num, h->hint_func);

    /* Subsystem specific physical location description */
    if ((*ct)->cat_fntable->printfn)
        ((*ct)->cat_fntable->printfn)(physloc);
    else
        msg_printf(ERR, "\n");

    if (h->hint_msg){
	hint_buf[0] = '+';
        va_start(ap, physloc);
        vsprintf(&hint_buf[1], h->hint_msg, ap);
        va_end(ap);

	msg_printf(ERR, "%s\n", hint_buf);
    }
    ev_perr(0, "HARDWARE ERROR STATE:\n");
    r4kregs();
}


static char *exc_names[] = {
/*0-3*/   "Interrupt", "Page Modified", "Load TLB Miss", "Store TLB Miss", 
/*4-7*/   "Load Addr Err", "Store Addr Err", "Instr Bus Err", "Data Bus Err", 
/*8-11*/  "System Call", "Breakpoint", "Reserved Instr", "Cop. Unusable", 
/*12-15*/ "Overflow", "Trap", "Instr VCE", "Float pt Exc", 
/*16-23*/ "-", "-", "-", "-","-", "-", "-", "Watchpoint", 
/*24-31*/ "-", "-", "-", "-","-", "-", "-", "Data VCE" 
};


void r4kregs(void)
{
    int	exc_code, int_bits, i;

    exc_code = (_cause_save & CAUSE_EXCMASK ) >> CAUSE_EXCSHIFT;
    if ((exc_code != EXC_IBE) || (exc_code != EXC_DBE))
	return;	/* Not interesting */

    ev_perr(1, "(INT:");

    int_bits = (_cause_save & CAUSE_IPMASK) >> CAUSE_IPSHIFT;

    for(i=8; i > 0; i--){
	if (( 1 << (i - 1)) & int_bits)
	    msg_printf(ERR, "%d", i);
	else
	    msg_printf(ERR, "-");
    }
    if (exc_code)
	msg_printf(ERR, " <%s> )\n", exc_names[exc_code]);
    else
	msg_printf(ERR, ")\n"); 

    ev_perr(1, "EPC:    0x%x\tCause: 0x%x\tStatus: 0x%x\n", 
	    _epc_save, _cause_save, _sr_save);
    
#if !defined(TFP) && !defined(IP25) 	/* THIS IS A HACK - must fix */
    ev_perr(1, "ErrEPC: 0x%x\tBadVA: 0x%x\tReturn: 0x%x\n", 
	_error_epc_save, _badvaddr_save, _regs[31]);
#endif

    _cause_save = 0;	/* Reset so that future calls will not be confused */
}

/* Some dummy routines till they get defined in everror.c */
int  sendintr(cpuid_t cpuid, unchar cpuno){}

int clear_IP()
{
	int i;
	long long tmp, tmp1, tmp2;

    tmp = EV_GET_LOCAL(EV_IP0);
    msg_printf(DBG, "IP0 register before test 0x%llx\n", tmp);
    if (tmp)
    {
        msg_printf(DBG, "Clearing IP0 register\n");
        for (i = 0; i < 64; i++)
        {
            if (tmp & ((long long)0x1))
            {
                msg_printf(DBG, 
		  "Pending interrupt at priority 0x%x prior to test\n", i);

                /* Clear pending interrupt. */
                EV_SET_LOCAL(EV_CIPL0, i);

                /* Wait a cycle (is this necessary?). */
                tmp1 = EV_GET_LOCAL(EV_RO_COMPARE);

            }
            tmp = tmp >> 1;
        }
    }
    tmp = EV_GET_LOCAL(EV_IP1);
    msg_printf(DBG, "IP1 register before test 0x%llx\n", tmp);
    if (tmp)
    {
        msg_printf(DBG, "Clearing IP1 register\n");
        for (i = 64; i < 127; i++)
        {
            if (tmp & 1)
            {
                msg_printf(DBG, 
		   "Pending interrupt at priority 0x%x prior to test\n", i);

                /* Clear pending interrupt. */
                EV_SET_LOCAL(EV_CIPL0, i);

                /* Wait a cycle (is this necessary?). */
                tmp1 = EV_GET_LOCAL(EV_RO_COMPARE);

            }
            tmp = tmp >> 1;
        }
    }
}

void ide_invalidate_caches(void)
{
    clear_bustags();
    invalidate_caches();
}
#endif	/* EVEREST */
