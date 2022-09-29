#ident	"lib/libsk/graphics/VENICE/venice_init.c:  $Revision: 1.3 $"

/*
 * venice_init.c - initialization functions for VENICE graphics on
 * EVEREST systems.
 */

#include "sys/types.h"
#include "sys/sema.h"
#include "arcs/hinv.h"
#include "arcs/folder.h"
#include "arcs/cfgdata.h"
#include "arcs/cfgtree.h"
#include "sys/gfx.h"
#include "sys/rrm.h"
#include "sys/venice.h"
#include "sys/time.h"
#include "sys/ktime.h"
#include "VENICE/gl/cpcmdstr.h"
#include "sys/EVEREST/fchip.h"
#include "sys/EVEREST/io4.h"
#include "sys/EVEREST/evconfig.h"
#include "sys/mips_addrspace.h"

#if      defined(IP19)
#include "sys/EVEREST/IP19.h"
#elif defined(IP21)
#include "sys/EVEREST/IP21.h"
#elif defined(IP25)
#include "sys/EVEREST/IP25.h"
#endif

#ifdef DEBUG
#define xprintf ttyprintf
#else
/* ARGSUSED */
void xprintf(char *fmt, ...)
{
  /* don't do anything */
}
#endif

#define IOMAP_TLBPAGEMASK	0x1ffe000


extern int init_graphics(void);

#define FIFO_HI_WATER	0xc00
#define FIFO_LOW_WATER	0x100
#define FIFO_ABSOLUTE_MAX 0xfff

#define PAGESIZE	0x1000000	/* 16Mb pages when touching graphics */



#if defined(IP19) || defined(IP21) || defined(IP25)
struct jtag {
    int control;        /* jtag control register */
    int fill1;
    int data;           /* jtag data */
    int fill2;
    int tms;            /* jtag tms */
    int fill3;
    int status;         /* jtag status */
    int fill4;
};

#define SETUP_JTAG	volatile struct jtag *JTAG; \
			JTAG = (struct jtag *)((unsigned long)(fcg_base_va)\
					       + (0x1500 << 3));
#else
struct jtag {
    int fill0;          /* address register 0 */
    int fill1;          /* address register 1 */
    int control;        /* jtag control register */
    int fill2;          /* eeprom data 0 */
    int data;           /* jtag data */
    int tms;            /* jtag tms */
    int status;         /* jtag status */
};
#define SETUP_JTAG      volatile struct jtag *JTAG; \
                        if (ven_pipeaddr == 0) \
                            JTAG = (struct jtag *) \
                            (VENICE_MPG_REGS_ADDR+(VENICE_UBUS_ADDR0<<2)); \
                        else \
                            JTAG = (struct jtag *) \
                            (((unsigned long)ven_pipeaddr)\
			     +(VENICE_UBUS_ADDR0<<2)); 
#endif

/*
 * Global variables:
 */
struct	venice_info venice_info;
volatile struct fcg *fcg_base_va;

/*
 * Externs:
 */
extern struct htp_fncs venice_tp_fncs;

void init_fchip(int, int, void *);
void reset_fci(void *);
static void VeniceInit(volatile struct fcg *, struct venice_info *);
static void VeniceInitInfo(volatile struct fcg *, struct venice_info *);

extern int cpuid(void);
extern void us_delay(int);
extern long long __ll_lshift(long long, long long);
extern void bzero(void *, long);
extern void bcopy(const void *, void *, long);


/*
 * Whenever a FCG is found, initialise the corresponding graphics
 * and set up the graphics board info.
 *
 * Parameter:
 * init_flag	1	Initialise the graphics system to get it ready for
 *			doing graphics.
 *		0	Just find out about gfx information, no need to 
 *			initialise graphics.
 */
void *VeniceProbe(int init_flag, int *window_base)
{
	/* NOTE: this virtual address has to match what has been
	 * set up by the IO4 prom.  Check with stand/arcs/IO4prom/gfx.c
	 */
	fcg_base_va = (volatile struct fcg *)window_base;

	if (fcg_base_va->board_id == 0xee180000) {
		if (init_flag) {
			init_fchip(0,0,(void *)fcg_base_va);
			reset_fci((void *)fcg_base_va);
			VeniceInit(fcg_base_va,&venice_info);
		}
		else {
			reset_fci((void *)fcg_base_va);
			VeniceInitInfo(fcg_base_va,&venice_info);
		}
	}
	return((void *)fcg_base_va);
}

/*
 * Probe routine set by .cf file and used by the textport.  The tp calls
 * with fncs=0, and we return the base of the first VENICE board.  If fncs
 * is != 0, we return the functions for the graphics board at base.
 */
#if 0  /* XXXdbg -- don't need this because we already do it in eve_gfx.c */
void *_venice_probe(struct htp_fncs **fncs)
{
	void *retval;

	xprintf("venice_probe(0x%lx)\n",(unsigned long)fncs);

	if  (retval = VeniceProbe(1, ven_window_base)) {
	    extern unsigned char clogo_data[];
	    extern int clogo_w;
	    extern int clogo_h;
	    extern int clogo_size;

	    bcopy(clogo_data, clogo_data_addr, clogo_size);
	    *clogo_w_addr = clogo_w;
	    *clogo_h_addr = clogo_h;
	}

	return retval;
}
#endif

extern COMPONENT *AddChild(COMPONENT *, COMPONENT *, void *);
extern long RegisterDriverStrategy(COMPONENT *, STATUS (*)(COMPONENT *self, IOBLOCK *));
extern int _gfx_strat(COMPONENT *, IOBLOCK *);

extern long long *tport_prom_base, *ven_window_base;
extern int        interface_type;
extern int        prom_ident, prom_size, prom_rev;
extern int	  ven_io4slot;
extern long	  ven_window, ven_adapter;

/*
 * Re-probe for VENICE graphics boards and add them to the hardware config
 */

int _venice_install(COMPONENT *root, int *window_base)
{
	COMPONENT venicetmpl; 
	COMPONENT *tmp;
	char idbuf[32];		/* e.g.  "SGI-Reality Engine 2 " */
	extern unsigned char clogo_data[], ven_logo_data[];
	extern int clogo_h, clogo_w, ven_logo_h, ven_logo_w, ven_logo_size;

	xprintf("venice_install(0x%lx)\n",(long)root);

	venicetmpl.Class = ControllerClass;
	venicetmpl.Type = DisplayController;
	venicetmpl.Flags = ConsoleOut|Output;
	venicetmpl.Version = SGI_ARCS_VERS;
	venicetmpl.Revision = SGI_ARCS_REV;
	venicetmpl.Key = 0;
	venicetmpl.AffinityMask = 0x01;
	venicetmpl.ConfigurationDataSize = 0;
	venicetmpl.Identifier = idbuf;
	
	VeniceProbe(0, window_base);	/* Probe all boards, but don't initialise them */
	/* figure out VENICE configuration.
	 */
	if (venice_info.ge_rev == VENICE_ID_6GES) {
		strcpy(venicetmpl.Identifier,"SGI VTX");
	}
	else {
		strcpy(venicetmpl.Identifier,"SGI Reality-Engine 2");
	}
	switch (venice_info.ge_rev) {
		case VENICE_ID_12GES: strcat(venicetmpl.Identifier," 12 GEs ");
		                      break;
		case VENICE_ID_8GES:  strcat(venicetmpl.Identifier," 8 GEs ");
		                      break;
		case VENICE_ID_6GES:  strcat(venicetmpl.Identifier," 6 GEs ");
		                      break;
		case VENICE_ID_4GES:  strcat(venicetmpl.Identifier," 4 GEs ");
		                      break;
		case VENICE_ID_2GES:  strcat(venicetmpl.Identifier," 2 GEs ");
		                      break;
		case VENICE_ID_1GE:   strcat(venicetmpl.Identifier," 1 GE ");
		                      break;
	}

	switch (venice_info.rm_count) {
		case 4: strcat(venicetmpl.Identifier," 4 RMs "); break;
		case 2: strcat(venicetmpl.Identifier," 2 RMs "); break;
		case 1: strcat(venicetmpl.Identifier," 1 RM "); break;
	}

	venicetmpl.IdentifierLength = strlen(venicetmpl.Identifier);
	tmp = AddChild(root, &venicetmpl, (void*)NULL);
	if (tmp == (COMPONENT*)NULL)
		xprintf("venice");
	tmp->Key = 0;
	RegisterDriverStrategy(tmp, _gfx_strat);

	/* Set up the prom logo */
	clogo_h = ven_logo_h;
	clogo_w = ven_logo_w;
	bcopy(ven_logo_data,clogo_data,ven_logo_size);
	return 1;
}

/*
 * bigendian == 1 if the system is bigendian, 0 otherwise.  Currently ignored.
 * init_state == PWR_UP, the system is powering up, or has gone
 *	through diagnostics, and is in completely unknown state, do full 
 *	initialisation.  If init_state == RESTART, the system
 *	has been previously initialised, do basic initialisation only.
 *      Currently ignored.
 * base gives the virtual address of the chip.  For the Fchip, base
 *	contains the virtual address of the little window for the Fchip.
 *	For the FCG, base contains the virtual address of the big window
 *	for the FCG.
 */
/* ARGSUSED */
void
init_fchip(int bigendian, int init_state, void *base)
{
	/* volatile struct fchip *fchip; */
	int io_config_reg;	

	xprintf("init_fchip in slot %d, adapter %d\n", ven_io4slot, ven_adapter);
	io_config_reg = (int)IO4_GETCONF_REG(ven_io4slot, 2);/* read IO config reg(reg 2) in the IA*/
	io_config_reg |= (0x10101 << ven_adapter);	/* mark adapter present, is GFX, and uses write gatherer*/
	IO4_SETCONF_REG(ven_io4slot, 2, io_config_reg);
}


/* ARGSUSED */
void
init_fcg(int bigendian, int init_state, void *base)
{
	extern int fcg_cmdmap[];
	volatile struct fcg *fcg;
	int	reg;
	int	i;

	fcg = (volatile struct fcg *)base;

	/* Clear all errors */
	reg = fcg->fcg_error;
	fcg->fcg_error = reg;
	fcg->gef_intr_clr = 1;		     /* Clear the GEF intr interlock */

	/* Enable FCI command overlap */
	fcg->enb_cmd_ovlap = 1;

	/* Program all interrupt registers */
	fcg->intr_dst = cpuid();	     /* Default intr destination is
						the current cpu */

	/* Load the command mapper (all 64 entries) */
	for (i=0; i<64; i++) {
		fcg->cmdmapper[i].value = fcg_cmdmap[i] >= 0 ?
		                            (fcg_cmdmap[i] << 4) : (0);
	}

	/* Program the fifo controls */
	fcg->gfxlow = (FIFO_LOW_WATER >> 4) << 4;
	fcg->gfxhigh = (FIFO_HI_WATER >> 4) << 4;
	fcg->gfxover = (FIFO_ABSOLUTE_MAX >>4) <<4;
}


void reset_fci(void *base)
{
	volatile int *fchip = (volatile int *)base; 
	SETUP_JTAG;

        JTAG->control = 0;
	fchip[FCHIP_SW_FCI_RESET/sizeof(int)+1] = 1;
	DELAY(5000);
}



static void
VeniceInit(volatile struct fcg *fcg, struct venice_info *info)
{
	long long ll;


xprintf("VeniceInit(0x%lx, 0x%lx)\n",(long)fcg,(long)info);


	VeniceInitInfo(fcg,info);

	/* Initialise FCG */
	init_fcg(1,0,(void *)fcg);

	/* Set up the write gatherer in the CC chip so that it
	 * dumps its contents to the graphics pipe.
	 */

#if defined(IP19)
	/* Big Endian, disabled */
	store_double((long long *)EV_WGCNTRL,(long long)2);

	/* Clear the write gatherer */
	store_double((long long *)EV_WGCLEAR,(long long)0);
#endif

	/* Destination is base of large window for FCG */
	ll = (long long)(LWIN_PFN(ven_window, ven_adapter));
#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _MIPS_SIM_ABI32)
	/* register pair long long's */
	ll = __ll_lshift(ll, 12); /* 12 ==> 2^12 == 4096, which is page size */
#else
	/* real 64-bit long long's */
	ll = ll<<12;		  /* 12 ==> 2^12 == 4096, which is page size */
#endif
        ll |= (cpuid() << 8) | (1<<15);
	
#if defined(IP25)
	store_double((long long *)EV_GRDST,ll);
#else
	store_double((long long *)EV_WGDST,ll);
#endif

#if defined(IP19)
	/* Big Endian, enabled */
	store_double((long long *)EV_WGCNTRL,(long long)3); 
#endif  /* defined(IP19) */


	/* Initialise all of venice graphics: download ucode, ..etc */
	init_graphics();

#ifdef DEBUG
xprintf("init_graphics completed\n");
gl_checkpipesync();
#endif

}



static void
VeniceInitInfo(volatile struct fcg *fcg, struct venice_info *info)
{
	int rmmask;

	bzero(info, sizeof(struct venice_info));
	info->ge_rev = (fcg->jtag_misc >> 4)& 0x7; 	/* Number of GEs on board */


	/* rmmask = (RMT << 4) | (RM) */
	rmmask = fcg->vc[V_DG_RM_PRESENT_L].value & 0xf;
	rmmask |= (fcg->vc[V_DG_RMT_PRESENT_L].value & 0xf) << 4;
	switch (rmmask & 0xf0) {
	case 0xf0:
		break;
	default:
		xprintf("illegal RM4 termination, (mask 0x%x)", rmmask);
		break;
	}
	switch (rmmask & 0xf) {
	case 0x0:	info->rm_count = 4; break;
	case 0xc:	info->rm_count = 2; break;
	case 0xe:	info->rm_count = 1; break;
	case 0x3:	info->rm_count = 2; break;
	case 0xb:	info->rm_count = 1; break;
	default:
		xprintf("illegal RM4 board count, (mask 0x%x), defaulting to 1\n", rmmask );
		info->rm_count = 1;
		break;
	}
	bcopy(VENICE_BOARDNAME,info->gfx_info.name,sizeof(VENICE_BOARDNAME));
	info->gfx_info.length = sizeof(struct venice_info);
	info->gfx_info.xpmax = 0;
	info->gfx_info.ypmax = 0;
}



