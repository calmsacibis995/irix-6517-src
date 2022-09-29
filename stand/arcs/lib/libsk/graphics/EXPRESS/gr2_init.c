#ident	"lib/libsk/graphics/EXPRESS/gr2_init.c:  $Revision: 1.66 $"

/*
 * gr2_init.c - initialization functions for EXPRESS graphics
 */

#include "sys/sbd.h"
#include "sys/param.h"
#include "arcs/hinv.h"
#include "sys/gr2hw.h"
#include "sys/sema.h"
#include "sys/gr2.h"
#include "sys/cpu.h"
#include "sys/immu.h"
#include "vidtim.h"
#include "sys/gr2_if.h"
#include "mc.out.h"
#include "arcshqucout.h"
#include "arcsgeucout.h"

#include "cursor.h"
#include <libsc.h>
#include <libsk.h>

#if defined(IP19) || defined(IP21) || defined(IP25)
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/dang.h>
int tport_window=-1, tport_adap=-1;
int dang_inited = 0;
volatile long long *dang_ptr=(long long *)NULL;

static void *everest_dang_init(int , int, int);
static int find_dang(int, int *, int *);
#endif

#if defined(IP19)
static volatile struct _expPipeEntryRec *wg = (volatile struct _expPipeEntryRec *)EV_WGINPUT_BASE;
#define FIFO(x,y) 	wg[(x)].pipeUnion.l = (y)
#define FLUSHBUS()	wg[0xff].pipeUnion.l = 0
#elif defined(IP21)
#define EVEN_OFFSET	0
#define FLUSH_OFFSET	4
static volatile union _expPipeEntryRec *wg = (volatile union _expPipeEntryRec *)EV_WGINPUT_BASE;
#define FIFO(x,y) 	{ wg[EVEN_OFFSET].i = (x); wg[EVEN_OFFSET].i = (y); }
#define FLUSHBUS()	wg[FLUSH_OFFSET].i = 0
#elif defined(IP25)
static volatile union _expPipeEntryRec *wg = (volatile union _expPipeEntryRec *)EV_WGINPUT_BASE;
#define FIFO(x,y) 	{ wg[0].i = (x); wg[0].i = (y); }
#define FLUSHBUS()	load_double((long long *)&dang_ptr[DANG_UPPER_GIO_ADDR])
#else
#define FIFO(x,y)	base->fifo[(x)] = (y)
#define FLUSHBUS()	*((volatile int *)PHYS_TO_K1(0x1fc00000))
#endif

extern unsigned char vc1_ptr_bits[];
extern unsigned char vc1_ptr_ol_bits[];

#if defined (FLAT_PANEL)
#include "sys/fpanel.h"
#endif /* FLAT_PANEL */

#define GR2_VR_TIMEOUT		20000
#define GR2_TIMEOUT_XMAP	10

extern struct tp_fncs gr2_tp_fncs;

#if IP22
int is_indyelan(void);
#endif

#if IP22 || IP26 || IP28
#define MAXGR2	2
#else
#define MAXGR2	1
#endif

struct gr2_info gr2_ginfo[MAXGR2];

#if defined(IP19) || defined(IP21) || defined(IP25)
struct gr2_hw *expr[] = {
	(struct gr2_hw *)NULL, /* XXX need to create a page(s) to map the EXPRESS in GIO space */
	(struct gr2_hw *)NULL
};
#else
struct gr2_hw *expr[] = {
	(struct gr2_hw*)PHYS_TO_K1(GR2_BASE_PHYS),
#if IP22 || IP26 || IP28
	(struct gr2_hw*)PHYS_TO_K1(GR2_BASE1_PHYS),
#endif /* IP22 || IP26 || IP28 */
	(struct gr2_hw*)NULL
};
#endif

void Gr2TpMapcolor(void *,int,int,int,int);
extern COMPONENT montmpl;		/* defined in gfx.c */

/******************************************************************************
 *
 * Check to see if a private bus GR2 type board is there.  
 *
 * Returns:
 *	0: board not found
 *	1: found board, part of info struct filled in
 *
 *****************************************************************************/
int Gr2Probe(struct gr2_hw *hw)
{
        if (badaddr(&hw->hq.mystery,4) || hw->hq.mystery != 0xdeadbeef) {
#if defined(IP19) || defined(IP21) || defined(IP25)
		/* Clean up since we probably have a DANG timeout */
		store_double((long long *)&dang_ptr[DANG_CLR_TIMEOUT], (long long)1);	/*Clear timeout */
#endif
		return 0;
	}
	else {
		return 1;
	}
}

static void Gr2InitInfo(struct gr2_hw *hw, struct gr2_info *info)
{
	int vma, vmb, vmc;
	int GR2version, VBversion;

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

struct gr2_info *
getGr2Info(void *hw)
{
	struct gr2_hw *express;
	int i;

	for (i = 0, express=expr[0]; express != 0; express=expr[++i]) {
		if ((void *)express == hw)
			return(&gr2_ginfo[i]);
	}
	return(0);
}

unsigned char clk107B[] = {
	0xc4,0x0f,0x15,0x20,0x30,0x4c,0x50,0x60,
	0x70,0x80,0x91,0xa5,0xb6,0xd1,0xe0,0xf0,0xc4};

/******************************************************************************
 *
 * Set up configuration register and start the clock.
 *
 *****************************************************************************/
unsigned char PLLclk132[] = {0x00,0x00,0x00,0x04,0x05,0x04,0x06};
unsigned char PLLclk107[] = {0x00,0x10,0x00,0x15,0x18,0x01,0x0F};
static void
Gr2StartClock(register struct gr2_hw  *base, struct gr2_info *info)
{
	register int i,j;
	register unsigned char PLLbyte, *PLLclk = NULL;

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
 	 	* Must always initialize programmable clock, even if not being used,
	 	* else will run at max speed, and may affect system. 
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

                if ((info->MonitorType & 0x7) == 1)   /* 72 HZ */
                        PLLclk = PLLclk132;
                else if ((info->MonitorType & 0x7) != 6) /* 60 HZ */
                        PLLclk = PLLclk107;

                if (PLLclk) {
                        for (i=0; i<7; i++) {
                                PLLbyte = PLLclk[i];
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
		DELAY(3);
		base->bdvers.rd1 = 0x1;
    	}
}

static void Gr2DacInit(register struct gr2_bt457_dac *dac)
{
	register int i;

	dac->addr = GR2_DAC_READMASK;
	dac->cmd2 = 0x0;			/* Turn off DAC*/
	dac->addr = GR2_DAC_BLINKMASK;
	dac->cmd2 = 0;
	dac->addr = GR2_DAC_CMD;
	dac->cmd2 = GR2_DAC_5TO1MUX|GR2_DAC_RAMENBL|GR2_DAC_OL0ENBL;
	dac->addr = GR2_DAC_TEST;
	dac->cmd2 = 0;
	
	/* Identity gamma ramp */
	dac->addr = 0;
	for (i = 0; i < GR2_DAC_NGAMMAENT; i++) 
		dac->paltram = i;

	/* overlay color palette initialization */
	dac->addr = 0;
	for (i = 0; i < GR2_DAC_NOVLMAPENT; i++) 
		dac->ovrlay = i;
}

static void Gr2LoadVC1SRAM(
	register struct gr2_hw *base,	/* base address in K1 seg */
	unsigned short *data,
	unsigned int addr,
	unsigned int length)
{
	register int i;

	base->vc1.addrhi = addr >> 8;
	base->vc1.addrlo = addr & 0x00ff;

	for (i = 0; i < length; i++)
    		base->vc1.sram = data[i];
}

/*
 * The first section of XMAP initialization.
 */
static void
Gr2XMAPInit(
	register struct gr2_hw *base, /* board base address in K1 seg */
	struct gr2_info *info)
{
	int pix_mode, pd_mode, pix_pg;
	
	/*
	 * Initialize mode registers
	 */
	
	/* Init XMAP to 8 bit color index mode
	 *	IMPT: Need to set color mode for textport bringup.
	 */
	base->xmapall.addrlo = 0;
	base->xmapall.addrhi = 0;

	/* Only the mode registers of DID 0(default) is initalized */
	pix_mode = 2;	/* 8-bit color index, buffer 0 */
	pd_mode = 0;
	pix_pg = 16;	/* DID 0 only */
	base->xmapall.mode = (((pix_pg<<3) | pix_mode) << 24) | (pd_mode<<16);

	/*
	 * Initialize misc. registers
	 *  XXX X server should write this when we get ext. fifo 
	 */
	base->xmapall.addrlo = 0;
	base->xmapall.addrhi = 0;

	base->xmapall.misc = 0x0;	/* red component of blackout value */
	base->xmapall.misc = 0x0;	/* green component of blackout value */
	base->xmapall.misc = 0x0;	/* blue component of blackout value */
	
	/* color index GR2_CURSCMAP_BASE+1 will be used as normal cursor color 
         *	for initialization 
	 */
	base->xmapall.misc = 14;	/* pgreg */	
	base->xmapall.misc = 0;		/* cu_reg */

#if defined (FLAT_PANEL)
        /*
         * If we're using the flat panel display,
         * enable output to the video connector.
         */
        if (info->MonitorType == MONITORID_CORONA || 
	    info->MonitorType == MONITORID_ICHIBAN) {
                base->xmap[0].fifostatus = 0xf;
                base->xmap[1].fifostatus = 0xc;
                base->xmap[2].fifostatus = 0xd;
                base->xmap[3].fifostatus = 0xe;
                base->xmap[4].fifostatus = 0xf;
		i2cPanelOn();
        }
#endif /* FLAT_PANEL */
}

/*Initialize hq2 internal registers */
static void Gr2HQ2RegInit(struct gr2_hw *base,	struct gr2_info *info)
{
	int i;
	info->GEs = 1;

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
			info->GEs = i+1;
	}


        /*
         * configure numge register, including fast ShRAM buffer for ultra
         */
        if (info->GEs == 8) {
            base->hq.numge = HQ2_FASTSHRAMCNT_4 | info->GEs;
        } else {
            base->hq.numge = info->GEs;
        }
	base->hq.refresh = 0x10;
	base->hq.fifo_full_timeout = 100;

	/* Need to be finalized */
	base->hq.fifo_empty_timeout = 10;

	base->hq.fifo_full = 40;

	/* No using it */
	base->hq.fifo_empty = 1;
}

/******************************************************************************
 *
 * Probe all chip revisions.
 *
 *****************************************************************************/
static void Gr2ProbeChipRev(struct gr2_hw *base, struct gr2_info *info)
{
	/* HQ2 rev. number */
	info->HQ2Rev = base->hq.version >> 23;

	/* GE7 rev. number */
	base->ge[0].ram0[0xFD] = 0;
	info->GE7Rev = (base->ge[0].ram0[0xFD] & 0xff) >> 5;

	/* VC1 rev. number */
        base->vc1.addrhi = 5 >> 8;
        base->vc1.addrlo = 5 & 0x00ff;

	info->VC1Rev = base->vc1.testreg & 0x7;
}

static void Gr2RegisterInit(struct gr2_hw *base, struct gr2_info *info)
{
	register didnum;
	register i;
	unsigned short frtableDID[1024];
	unsigned short ltableDID[8];
        register ysize = info->gfx_info.ypmax;    /* display resolution;
                                                 * XXX Shouldn't be hardcoded
                                                 */
	
#define NPIX_NormMon	1279	/* Must be between 1276-1279, so VC1 will use 
				  same DID # as previous pixel*/
	/**********************************************************************
	 * Init CPU graphics registers
	 *********************************************************************/

#if IP20
	/* clear vertical retrace interrupt */
        *((volatile unchar*)PHYS_TO_K1(PORT_CONFIG)) &= ~PCON_CLEARVRI; 
        flushbus();
        *((volatile unchar*)PHYS_TO_K1(PORT_CONFIG)) |= PCON_CLEARVRI;
#elif IP22
	/* clear vertical retrace interrupt */
	if (is_ioc1()) {
	    /* XXX - The IOC1 chip has fixed this hardware kludge and this code
	     * should be changed.
	     */
		/* INDY with Elan Board */
                if(is_indyelan()) {
                        *((volatile int *)PHYS_TO_K1(HPC3_GEN_CONTROL)) &= ~0x1;
                        flushbus();
                        *((volatile int *)PHYS_TO_K1(HPC3_GEN_CONTROL)) |= 0x00000001;
                }
	}
	else {
	    *((volatile unchar*)PHYS_TO_K1(PORT_CONFIG)) &=
			~(PCON_CLR_SG_RETRACE_N|PCON_CLR_S0_RETRACE_N);
	    flushbus();
	    *((volatile unchar*)PHYS_TO_K1(PORT_CONFIG)) |=
			 (PCON_CLR_SG_RETRACE_N|PCON_CLR_S0_RETRACE_N);
	}
#elif IP26 || IP28
	*((volatile unchar*)PHYS_TO_K1(PORT_CONFIG)) &=
			~(PCON_CLR_SG_RETRACE_N|PCON_CLR_S0_RETRACE_N);
	flushbus();
	*((volatile unchar*)PHYS_TO_K1(PORT_CONFIG)) |=
			 (PCON_CLR_SG_RETRACE_N|PCON_CLR_S0_RETRACE_N);
#endif
	
	/**********************************************************************
	 * Init DACs
	 *********************************************************************/
	
	Gr2DacInit(&base->reddac);
	Gr2DacInit(&base->grndac);
	Gr2DacInit(&base->bluedac);

	/**********************************************************************
	 * Init VC1
	 *********************************************************************/
	/* 
         * Disable interrupt to VC1, blackout the display.
	 *	GR2_VC1_INTERRUPT 
	 *  Don't use macro.
	 */
	base->vc1.sysctl = 0x1;

	/*
	 * Load video timing table 
	 * Timing table for cursor to move 
	 *	from 	off screen on the left, 
	 *	to 	1023
         * On older (GR2version < 4) systems
         * there are 3 bits of monitor id, one newer there are 4, and
         * on IndyElan, we have cleverly used bit 7 to indicate a
         * flat panel display is present.  Hence the multi-labeled cases
         * in the switch below.
	 */ 

	switch(info->MonitorType)
	{
		/* 72HZ Monitor */
		case 1: /* Mitsubishi 19" monitor, or Sony 19" 72HZ only */
		case 9:

		Gr2LoadVC1SRAM(base, (unsigned short *)ltableH72, GR2_VIDTIM_LST_BASE,
			sizeof(ltableH72)/sizeof(short));
		Gr2LoadVC1SRAM(base, (unsigned short *)frtableH72, GR2_VIDTIM_FRMT_BASE,
			sizeof(frtableH72)/sizeof(short));
		Gr2LoadVC1SRAM(base, (unsigned short *)ltableH72_400,
			GR2_VIDTIM_CURSLST_BASE, sizeof(ltableH72_400)/sizeof(short));
		Gr2LoadVC1SRAM(base, (unsigned short *)frtableH72_400,
			GR2_VIDTIM_CURSFRMT_BASE, sizeof(frtableH72_400)/sizeof(short));
		break;	

		case 6:
		case 14:
		break;	

#if defined (FLAT_PANEL)
                case MONITORID_CORONA:       
				/* low-res flat panel display */
                                /* Since the flat panel is low-res we
                                   only need 1 timing table (no vc1 cursor
                                   bug to work around in this case) */
                Gr2LoadVC1SRAM(base, (unsigned short *)ltableFP, GR2_VIDTIM_LST_BASE,
                               sizeof(ltableFP)/sizeof(short));
                Gr2LoadVC1SRAM(base, (unsigned short *)frtableFP, GR2_VIDTIM_FRMT_BASE,
                               sizeof(frtableFP)/sizeof(short));
                break;

		case MONITORID_ICHIBAN : /* hi-res flat panel display */

		Gr2LoadVC1SRAM(base, (unsigned short *)ltableIchiban,
				GR2_VIDTIM_LST_BASE,
				sizeof(ltableIchiban)/sizeof(short));
		Gr2LoadVC1SRAM(base, (unsigned short *)frtableIchiban,
				GR2_VIDTIM_FRMT_BASE,
				sizeof(frtableIchiban)/sizeof(short));
		Gr2LoadVC1SRAM(base, (unsigned short *)ltableIchiban_400,
				GR2_VIDTIM_CURSLST_BASE,
				sizeof(ltableIchiban_400)/sizeof(short));
		Gr2LoadVC1SRAM(base, (unsigned short *)frtableIchiban_400,
				GR2_VIDTIM_CURSFRMT_BASE,
				sizeof(frtableIchiban_400)/sizeof(short));
		break;


#endif /* FLAT_PANEL */

		default:
		Gr2LoadVC1SRAM(base, (unsigned short*)ltableH60, GR2_VIDTIM_LST_BASE, 
			sizeof(ltableH60)/sizeof(short));
		Gr2LoadVC1SRAM(base, (unsigned short*)frtableH60,
			GR2_VIDTIM_FRMT_BASE,
			sizeof(frtableH60)/sizeof(short));
		/* Timing table for cursor to move 
	 	*	from 	1024, 
	 	*	to      offscreen on the right.	
 	 	*
	 	* Line table is loaded at 0x400 in VC1 SRAM.
	 	*/ 
		Gr2LoadVC1SRAM(base, (unsigned short*)ltableH60_400, 
			GR2_VIDTIM_CURSLST_BASE,
			sizeof(ltableH60_400)/sizeof(short));
	
		Gr2LoadVC1SRAM(base, (unsigned short*)frtableH60_400, 
			GR2_VIDTIM_CURSFRMT_BASE,
			sizeof(frtableH60_400)/sizeof(short));
		break;
	}
	/* 
	 * Load DID table
	 */
	/*XXX found out where defines the X,Y resolution */
	/*XXX REPLACE 1023 later */
	/*XXX Need modify, if VC1 supports line repeat count */
	for (i = 0; i < ysize - 1; i++) 
		frtableDID[i] = GR2_DID_LST_BASE;
	
	/* HACK, bug in VC1.  4 dummy DID needed. */
	frtableDID[i] = GR2_DID_LST_BASE + 4; 
	
	for (didnum = 0; didnum < 1; didnum++) {
		ltableDID[didnum*8] = 1;
		ltableDID[didnum*8+1] = didnum;	/* Same DID for whole line */
		/* 
		 * HACK, bug in VC1.  4 dummy DID with the previous scanline
		 * entry needed at the end of LST.
		 */
		ltableDID[didnum*8+2] = ltableDID[didnum*8] + 4;
		ltableDID[didnum*8+3] = ltableDID[didnum*8+1];
		for (i = 4; i <= 7; i++)
			ltableDID[(didnum*8)+i] = 0;
	}
	
	Gr2LoadVC1SRAM(base, ltableDID, GR2_DID_LST_BASE, sizeof(ltableDID));
	Gr2LoadVC1SRAM(base, frtableDID, GR2_DID_FRMT_BASE, ysize); 

	/* Load entry pointer of the video timing table */
	base->vc1.addrhi = (GR2_VID_EP >> 8) & 0xff;
	base->vc1.addrlo = GR2_VID_EP & 0xff;
        if (info->gfx_info.xpmax > 1025)
                base->vc1.cmd0 = GR2_VIDTIM_CURSFRMT_BASE;
        else
                base->vc1.cmd0 = GR2_VIDTIM_FRMT_BASE;

	
	/* Load GR2_VID_ENAB */
	base->vc1.addrhi = (GR2_VID_ENAB >> 8) & 0xff;
	base->vc1.addrlo = GR2_VID_ENAB & 0xff;
	
	/* Always write in pair of bytes, even only 1 byte data is needed */
	base->vc1.cmd0 = 0;
	
	/* Load registers of DID table */
	base->vc1.addrhi = (GR2_DID_EP >> 8) & 0xff;
	base->vc1.addrlo = GR2_DID_EP & 0xff;
	base->vc1.cmd0 = GR2_DID_FRMT_BASE;
	base->vc1.cmd0 = GR2_DID_FRMT_BASE +
			 (unsigned short)(ysize*sizeof(short));
	base->vc1.cmd0 = ((NPIX_NormMon / 5) << 8) | (NPIX_NormMon % 5);
	
	/* Load black out control registers */
	base->vc1.addrhi = (GR2_BLKOUT_EVEN >> 8) & 0xff;
	base->vc1.addrlo = GR2_BLKOUT_EVEN & 0xff;
	base->vc1.cmd0 = 0;	/* index 0 of the color map */
	
	{
		unsigned short buf[128];

		bzero(buf, sizeof(buf));
		for (i = 0; i < 16; i++) {
			buf[i*2] = (vc1_ptr_bits[i*2] << 8)
				  | vc1_ptr_bits[i*2+1];
		}
		for (i = 0; i < 16; i++) {
			buf[64+i*2] = (vc1_ptr_ol_bits[i*2] << 8)
				     | vc1_ptr_ol_bits[i*2+1];
		}

		Gr2LoadVC1SRAM(base, buf, GR2_CURSOR0_BASE,
				sizeof(buf)/sizeof(short));

		base->vc1.addrhi = GR2_CUR_EP >> 8;
		base->vc1.addrlo = GR2_CUR_EP & 0xff;

		base->vc1.cmd0 = GR2_CURSOR0_BASE; /* CUR_EP */
		base->vc1.cmd0 = 0x000;		/* CUR_XL */
		base->vc1.cmd0 = 0x000;		/* CUR_XY */
		base->vc1.cmd0 = 0x000;		/* normal cursor mode */
	}

	/*  Enable VC1, cursor function and display, DID display 
	 * GR2_VC1_VC1 | GR2_VC1_DID | GR2_VC1_CURSOR| GR2_VC1_CURSOR_DISPLAY
	 * Don't use macro.
	 */
	base->vc1.sysctl = GR2_VC1_VC1 | GR2_VC1_DID
		         | GR2_VC1_CURSOR | GR2_VC1_CURSOR_DISPLAY;

	/**********************************************************************
	 * Init XMAPs
	 *********************************************************************/

	Gr2XMAPInit(base, info);

	/**********************************************************************
	 * Init other misc. regs.
	 *********************************************************************/

	Gr2HQ2RegInit(base, info);
	Gr2ProbeChipRev(base, info);
}

/*
 * Textport and diagnostic compiled in microcode.  Allows a standalone
 * executable to contain its own microcode.
 */

/*****************************************************************************
 * Set up the textport microcode.
 *****************************************************************************/

extern int _prom;

static mcout_t *Gr2TpHQUcodeSetup(void) 
{
	mcout_t *hqptr = (mcout_t *)(hq_ucode_str.hq_ucode);

	if (!_prom)			/* cannot write to prom .data! */
#if IP20
		strcpy(hqptr->version, "IP20/GR2 PROM HQ Microcode\n");
#elif IP22
		strcpy(hqptr->version, "IP22/GR2 PROM HQ Microcode\n");
#elif IP19
		strcpy(hqptr->version, "IP19/GR2 PROM HQ Microcode\n");
#elif IP21
		strcpy(hqptr->version, "IP21/GR2 PROM HQ Microcode\n");
#elif IP25
		strcpy(hqptr->version, "IP25/GR2 PROM HQ Microcode\n");
#elif IP26
		strcpy(hqptr->version, "IP26/GR2 PROM HQ Microcode\n");
#elif IP28
		strcpy(hqptr->version, "IP28/GR2 PROM HQ Microcode\n");
#endif
	return hqptr;
}

static mcout_t *Gr2TpGEUcodeSetup(void)
{
	mcout_t *geptr = (mcout_t *)(ge_ucode_str.ge_ucode);

	if (!_prom)			/* cannot write to prom .data! */
#if IP20
		strcpy(geptr->version, "IP20/GR2 PROM GE Microcode\n");
#elif IP22
		strcpy(geptr->version, "IP22/GR2 PROM GE Microcode\n");
#elif IP19
		strcpy(geptr->version, "IP19/GR2 PROM GE Microcode\n");
#elif IP21
		strcpy(geptr->version, "IP21/GR2 PROM GE Microcode\n");
#elif IP25
		strcpy(geptr->version, "IP25/GR2 PROM GE Microcode\n");
#elif IP26
		strcpy(geptr->version, "IP26/GR2 PROM GE Microcode\n");
#elif IP28
		strcpy(geptr->version, "IP28/GR2 PROM GE Microcode\n");
#endif
	return geptr;
}

static int Gr2DownloadHQ2(struct gr2_hw *base, mcout_t *hq_ucode)
{
	HQ_RECORD 	*baseptr; 
	
	unsigned short *code_start;
	int record_count, i,error;

	if(hq_ucode == 0)
		return 0;
#ifdef DEBUG
	/* In case, f_magic is incorrect, you'll be able to see what ucode is 
	 * being loaded.
	 */
	printf("Magic   : %x\n",    hq_ucode->f_magic);
	printf("Version : %s",      hq_ucode->version);
	printf("Date    : %s",      hq_ucode->date);
	printf("Path    : %s\n",    hq_ucode->path);
	printf("Length  : %d bytes per microword   =>   %d bit control word\n",
	       hq_ucode->length, hq_ucode->length * 8);
#endif
	if (hq_ucode->f_magic != HQ2_MAGIC) {
		return 0;	
	}
	
	/* Load HQ2 attribute jump table */
	
	for (i = 0; i < 16; i++) {
		base->hq.attrjmp[i] = hq_ucode->data[i];
	}
	
	code_start = (unsigned short *)
		((__psunsigned_t)hq_ucode + hq_ucode->f_codeoff);
	
	record_count = 0;
	
	while (1) {
		baseptr = (HQ_RECORD*)code_start;
		if (baseptr->address == 0xffff)
			break;
		base->hqucode[baseptr->address]
			= (baseptr->ucode[0] << 16) | baseptr->ucode[1];
		code_start += (hq_ucode->length + 6)/sizeof(short);
		record_count++;
	}
	
	code_start = (unsigned short *)
		((__psunsigned_t)hq_ucode + hq_ucode->f_codeoff);

	error = 0;
	
	while (1) {
		baseptr = (HQ_RECORD*)code_start;
		if (baseptr->address == 0xffff)
			break;
		if (((baseptr->ucode[0] << 16)
		    | baseptr->ucode[1]) != base->hqucode[baseptr->address]) {
#ifdef DEBUG
			printf("ERROR in hq ucode ****  Address -> %x    expected -> %x    actual ->  %x\n",
			       baseptr->address,
			       base->hqucode[baseptr->address],
			       ((baseptr->ucode[0]<<16) | baseptr->ucode[1]));
#endif
			
			error++;
		}
		code_start += (hq_ucode->length + 6)/sizeof(short);
		
	}
	if (error) {
#ifdef DEBUG
        	printf("%d errors were detected in hq_ucode\n", error);
#endif
		return 0;
	} else {
#ifdef DEBUG
        	printf("Download was successful no errors detected.....\n");
		printf("%d Microcode Records Downloaded..\n\n", record_count);
#endif
		return 1;
	}
}

#define	GE7URAM_SIZE	64*1024
static int Gr2DownloadGE7(struct gr2_hw *base, mcout_t *ge_ucode)
{
	GE_RECORD *baseptr;    
	unsigned short *code_start;
	
	int record_count, i, error, sherror;

	if(ge_ucode == 0)
		return 0;
#ifdef DEBUG
    /* In case, f_magic is incorrect, you'll be able to see what ucode is 
     * being loaded.
     */
	printf("Magic   : %x\n",    ge_ucode->f_magic);
    	printf("Version : %s",      ge_ucode->version);
    	printf("Date    : %s",      ge_ucode->date);
    	printf("Path    : %s\n",    ge_ucode->path);
    	printf("Length  : %d bytes per microword   =>   %d bit control word\n",
	       ge_ucode->length, ge_ucode->length * 8);
#endif
	if (ge_ucode->f_magic != GE7_MAGIC) {
                return 0;
	}
	
	for (i = 0; i < 512; i++)
		base->shram[CONSTMEM + i] = ge_ucode->data[i];
	
	sherror = 0;
	for (i = 0; i < 512; i++) {
		if( base->shram[CONSTMEM + i] != ge_ucode->data[i]) {
			printf("ERROR in loading constants**** Shram Location -> %x     expected -> %x 	actual -> %x\n", 
			       CONSTMEM + i, ge_ucode->data[i], base->shram[CONSTMEM + i]);
			sherror++;
		}
        }
	
#ifdef DEBUG
	if (sherror)
        	printf("%d errors were detected in shram constants\n",sherror);
#endif

	/* Clear GE ucode ram with zeros to workaround ucode/RAM initialization
	 * problem.
	 */
	for (i=0; i< GE7URAM_SIZE;i++)
	{
		base->hq.gepc = i;
		base->ge[0].ram0[0xf8] = 0x0; 
		base->ge[0].ram0[0xf9] = 0x0;  
		base->ge[0].ram0[0xfa] = 0x0;
		base->ge[0].ram0[0xfb] = 0x0; 
		base->hq.ge7loaducode =  0x0; 
	}

	code_start = (unsigned short *)
		((__psunsigned_t)ge_ucode + ge_ucode->f_codeoff);
	
	record_count = 0;
	
	while (1) {
		baseptr = (GE_RECORD*)code_start;
		if (baseptr->address == 0xffff)
			break;
		
		base->hq.gepc = baseptr->address;
		
		base->ge[0].ram0[0xf8]
			= (baseptr->ucode[8] << 16 ) | baseptr->ucode[9];
		base->ge[0].ram0[0xf9]
			= (baseptr->ucode[6] << 16 ) | baseptr->ucode[7];
		base->ge[0].ram0[0xfa]
			= (baseptr->ucode[4] << 16 ) | baseptr->ucode[5];
		base->ge[0].ram0[0xfb]
			= (baseptr->ucode[2] << 16 ) | baseptr->ucode[3];

		base->hq.ge7loaducode
			= (baseptr->ucode[0] << 16 ) | baseptr->ucode[1];

		code_start += (ge_ucode->length + 6)/sizeof(short);
		record_count++;
	}
	code_start = (unsigned short *)
		((__psunsigned_t)ge_ucode + ge_ucode->f_codeoff);

	error = 0;
	while (1) {
		baseptr = (GE_RECORD*)code_start;
		if (baseptr->address == 0xffff)
			break;

		base->hq.gepc = baseptr->address;
		
		if ((base->hq.ge7loaducode & 0x3dfffff)
		    != (((baseptr->ucode[0] << 16)
			 | baseptr->ucode[1]) & 0x3dfffff)) {
#ifdef DEBUG
			printf("ERROR: ge7ucodeload [25:0] to HQ, offset 0x%x\n",baseptr->address);
			printf("\texpected 0x%x, returned 0x%x\n",(((baseptr->ucode[0] << 16 ) | baseptr->ucode[1]) & 0x3dfffff), (base->hq.ge7loaducode & 0x3dfffff));
#endif
			error++;
		}
		
		if (base->ge[0].ram0[0xf8]
		    != ((baseptr->ucode[8] << 16) | baseptr->ucode[9])) { 
#ifdef DEBUG
			printf("ERROR: Bits[31:0] GE7ucode, offset 0x%x\n",
			       baseptr->address);
			printf("\texpected 0x%x, returned 0x%x\n",
			       ((baseptr->ucode[8]  << 16 ) | baseptr->ucode[9]), base->ge[0].ram0[0xf8]);
#endif
			error++;
		}
		if (base->ge[0].ram0[0xf9]
		    != ((baseptr->ucode[6] << 16) | baseptr->ucode[7])) { 
#ifdef DEBUG
			printf("ERROR: Bits [63:32] GE7ucode, offset 0x%x\n",
			       baseptr->address);
			printf("\texpected 0x%x, returned 0x%x\n",
			       ((baseptr->ucode[6]  << 16 ) | baseptr->ucode[7]), base->ge[0].ram0[0xf9]);
#endif
			error++;
		}
		if (base->ge[0].ram0[0xfa]
		    != ((baseptr->ucode[4] << 16 ) | baseptr->ucode[5])) { 
#ifdef DEBUG
			printf("ERROR: Bits [95:64] GE7ucode, offset 0x%x\n",
			       baseptr->address);
			printf("\texpected 0x%x, returned 0x%x\n",
			       ((baseptr->ucode[4] << 16) | baseptr->ucode[5]),base->ge[0].ram0[0xfa]);
#endif
			error++;
		}
		if ((base->ge[0].ram0[0xfb] & 0x7f)
		    != (((baseptr->ucode[2] << 16) | baseptr->ucode[3]) & 0x7f)) { 
#ifdef DEBUG
			printf("ERROR: Bits [102:96] GE7ucode, offset 0x%x\n",
			       baseptr->address);
			printf("\texpected 0x%x, returned 0x%x\n",
			       (((baseptr->ucode[2] << 16) | baseptr->ucode[3]) & 0x7f),(base->ge[0].ram0[0xfb] & 0x7f));
#endif
			error++;
		}
		code_start += (ge_ucode->length + 6)/sizeof(short);
	}
	
#ifdef DEBUG
	if (error)
        	printf("%d errors were detected in ge_ucode\n", error);
	else {
        	printf("GE7 ucode download was successful -- no errors detected\n");
     		printf("%d Microcode Records Downloaded...\n", record_count);
	}
#endif
	return 1;
}

static int Gr2TpSetup(struct gr2_hw *base, struct gr2_info *info)
{
	if (Gr2DownloadHQ2(base,Gr2TpHQUcodeSetup()) == 0) {
		base->shram[TP_PROBE_ID] = 0;
		return -1;
	}
	
	if (Gr2DownloadGE7(base,Gr2TpGEUcodeSetup()) == 0) {
		base->shram[TP_PROBE_ID] = 0;
		return -1;
	}
	
	/* Set GE pc to 0 */
	base->hq.gepc = 0;
	
	/* Unstall HQ and GE */
	base->hq.unstall = 0;

	switch(info->GfxBoardRev) { 
		case 0:
		case 1:
		case 2:
		case 3:
			FIFO(PUC_INIT,0);
			break;
		case 4:
		case 9:
		case 10:
		case 11:
			FIFO(PUC_INIT,1);
			break;
		case 5:
		case 6:
		case 7:
		case 8:
		default:
			FIFO(PUC_INIT,2);
			break;
	}
	FLUSHBUS();		/* make sure init completes */
	DELAY(9);

	base->shram[TP_PROBE_ID] = UCODE_TP;
	return 0;
}

static void Gr2SetCursorColor(struct gr2_hw *hw,
	int fr, int fg, int fb, int br, int bg, int bb)
{
	Gr2TpMapcolor(hw, GR2_CURSCMAP_BASE-4096 + 1, fr, fg, fb);
	Gr2TpMapcolor(hw, GR2_CURSCMAP_BASE-4096 + 2, br, bg, bb);
}

static void Gr2Init(struct gr2_hw *hw, struct gr2_info *info)
{

	/* must reset the graphics board in case ucode is running
	 */
#if IP22
	unsigned char mask;
	if(is_fullhouse()) {
		mask = (hw == (struct gr2_hw *)PHYS_TO_K1(GR2_BASE_PHYS)) ?
			PCON_SG_RESET_N :
			PCON_S0_RESET_N ;
		*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) &= ~mask;
		flushbus();
		DELAY(5);
		*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) |= mask;
		flushbus();
	} else if(is_indyelan()) { /* Indy Elan Graphics */
		*(volatile unsigned int *)PHYS_TO_K1(CPU_CONFIG) &= ~CONFIG_GRRESET;
                flushbus();
                DELAY(5);
                *(volatile unsigned int *)PHYS_TO_K1(CPU_CONFIG) |= CONFIG_GRRESET;
                flushbus();
        }
#elif defined(IP26) || defined(IP28)
	unsigned char mask;
	mask = (hw == (struct gr2_hw *)PHYS_TO_K1(GR2_BASE_PHYS)) ?
		PCON_SG_RESET_N :
		PCON_S0_RESET_N ;
	*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) &= ~mask;
	flushbus();
	DELAY(5);
	*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) |= mask;
	flushbus();
#elif defined(IP19) || defined(IP21) || defined(IP25)
	store_double((long long *)&dang_ptr[DANG_GIORESET],(long long)0);
	DELAY(10);
	store_double((long long *)&dang_ptr[DANG_GIORESET],(long long)1);
#else
	*(volatile unsigned int *)PHYS_TO_K1(CPU_CONFIG) &= ~CONFIG_GRRESET;
	flushbus();
	DELAY(5);
	*(volatile unsigned int *)PHYS_TO_K1(CPU_CONFIG) |= CONFIG_GRRESET;
	flushbus();
#endif

	Gr2InitInfo(hw, info);

	/*
	 * Start the clock and set up the configuration
	 */
	Gr2StartClock(hw, info);
	
#if defined (FLAT_PANEL)
	{
       		int panel = 0xdeadbeef;
			
               	/*
                 * No video support, check for flat panel.  If
                 * we find one, we'll use it as the display.
                 */
		if ( gr2_i2cProbe(hw) && i2cSetup() &&
                        		i2cPanelID(&panel) &&
                        		(panel == PANEL_XGA_MITSUBISHI || 
                        		 panel == PANEL_EWS_MITSUBISHI))
		{
			unsigned char *PLLclk, PLLbyte;
			int i,j;

			i2cPanelOff();		/* reduct flash! */
			switch (panel) {
			case PANEL_EWS_MITSUBISHI :
				info->MonitorType = MONITORID_ICHIBAN;
				info->gfx_info.xpmax = 1280;
				info->gfx_info.ypmax = 1024;
                        	PLLclk = PLLclkIchiban;
				break;
			case PANEL_XGA_MITSUBISHI :
				info->MonitorType = MONITORID_CORONA;
				info->gfx_info.xpmax = 1025;
				info->gfx_info.ypmax = 768;
                        	PLLclk = PLLclkFP;
#if IP22 || IP26 || IP28
				/* The Indigo2 logo does not
				   fit on the small screen */
				if (is_fullhouse())
					logoOff();
#endif
				break;
			}

			/* Re-program clock to flat panel clock */	

			hw->bdvers.rd0 = 0x47;

                        for (i=0; i<7; i++) {
                                PLLbyte = PLLclk[i];
                                for (j=0;j<8;j++) {
                                        if ((j == 7) && (i== 6))
                                                hw->clock.write = (PLLbyte & 0x1) | 0x2;
                                        else
                                                hw->clock.write = PLLbyte & 0x1;
                                        PLLbyte = PLLbyte >> 1;
                                }
                        }
			hw->bdvers.rd1 = 0x00; 
			DELAY(3);
			hw->bdvers.rd1 = 0x1;

		}
	}
#endif /* FLAT_PANEL */
	/*
	 * Initialize registers. Screen will be blanked.
	 */
	Gr2RegisterInit(hw, info);
	
	/* download ucode (tport & diag) */

	Gr2TpSetup(hw,info);
	
	/* init cursor color */
	Gr2SetCursorColor(hw, 0xff, 0, 0, 0xff, 0xff, 0xff);
}

int
Gr2ProbeAll(char *base[])
{
	struct gr2_hw *express;
	int i, bd = 0;

	/* Init all express boards, but use first found as default.
	 */
	for (i = 0, express=expr[0]; express != 0; express=expr[++i]) {
		if (Gr2Probe(express)) {
			Gr2Init(express, &gr2_ginfo[i]);
			base[bd++] = (char *)express;
		}
	}

	return bd;
}

/*
 * Probe routine set by .cf file and used by the textport.  The tp calls
 * with fncs=0, and we return the base of the next EXPRESS board.  If fncs
 * is != 0, we return the functions for the graphics board at base.
 */

void *
gr2_probe(struct tp_fncs **fncs)
{
	static char *base[MAXGR2+1];
	static int i;
	
#if defined(IP19) || defined(IP21) || defined(IP25)
	int	slot;
	void	*gio_va;	/* Virtual address of GIO 0x1f000000 */
#endif

	if (fncs) {
		*fncs = &gr2_tp_fncs;
		i = 0;
		return 0;
	}

#if defined(IP19) || defined(IP21) || defined(IP25)
	/* In an IP19 or IP21 Everest chassis.
	 * Look for the presence of DANG chip, figure out the Ibus
	 * slot and Ibus adapter number.
	 */
	if (!dang_inited) {
		if ((slot = find_dang(0,&tport_window,&tport_adap)) == -1) {
			/* Can't find the DANG chip, assume no gr2 present */
			return 0;
		}
		else {
			dang_ptr = (volatile long long *)SWIN_BASE(tport_window,tport_adap);
		}
		/* Initialise the DANG chip in the given slot, window, and adapter
		 * number.  Allocate a virtual page that maps onto the EXPRESS
		 * area within the GIO.
		 */

		gio_va = everest_dang_init(slot,tport_window,tport_adap);
		expr[0] = (struct gr2_hw *)gio_va;
		dang_inited = 1;
	}
#endif

	/* allow looping to get multiple heads */
	if (i == 0) {
		bzero(base,sizeof(base));
		Gr2ProbeAll(base);
	}

	if (i >= MAXGR2+1)
		return 0;

	return (void *)base[i++];
}

/*
 * Re-probe for EXPRESS graphics boards and add them to the hardware config
 */
int
gr2_install(COMPONENT *root)
{
	char idbuf[44];	/* max is "SGI-GR2-XZ missing bitplanes missing Z" */
	COMPONENT *tmp, gr2tmpl;
	struct gr2_info *pinfo;
	struct gr2_hw **pexpr;
	extern int gfxboard;
	int i;
#if defined(IP19) || defined(IP21) || defined(IP25)
	void	*gio_va;	/* Virtual address of GIO 0x1f000000 */
	int slot;


	if (!dang_inited) {
		if ((slot = find_dang(0,&tport_window,&tport_adap)) == -1) {
			/* Can't find the DANG chip, assume no gr2 present */
			return 0;
		}
		else {
			dang_ptr = (volatile long long *)SWIN_BASE(tport_window,tport_adap);
		}
		/* Initialise the DANG chip in the given slot, window, and adapter
		 * number.  Allocate a virtual page that maps onto the EXPRESS
		 * area within the GIO.
		 */

		gio_va = everest_dang_init(slot,tport_window,tport_adap);
		expr[0] = (struct gr2_hw *)gio_va;
		dang_inited = 1;
	}
#endif

	gr2tmpl.Class = ControllerClass;
	gr2tmpl.Type = DisplayController;
	gr2tmpl.Flags = ConsoleOut|Output;
	gr2tmpl.Version = SGI_ARCS_VERS;
	gr2tmpl.Revision = SGI_ARCS_REV;
	gr2tmpl.Key = 0;
	gr2tmpl.AffinityMask = 0x01;
	gr2tmpl.ConfigurationDataSize = 0;
	gr2tmpl.Identifier = idbuf;

	for(i=0, pexpr=expr; *pexpr != 0; pexpr++,i++) {
		if (Gr2Probe(*pexpr)) {
			char *ext;

			pinfo = &gr2_ginfo[i];

			/*  Info is not initialized make sure we kill
			 * lingering magic number, then probe enough
			 * to figure out board type.
			 */
			if (pinfo->GEs == 0) {
				(*pexpr)->shram[TP_PROBE_ID] = 0;
				Gr2InitInfo(*pexpr,pinfo);
				Gr2HQ2RegInit(*pexpr,pinfo);
			}

			/* figure out GR2 config:
			 *	Ultra - 8 GE, 24 bit, with Z
			 *	Elan  - 4 GE, 24 bit, with Z
			 *	XZ   -  2 GE, 24 bit, with Z 
			 *	XS24  - 1 GE, 24 bit, opt Z
			 *	XSM   - 4 GE, 8 bit, no Z (ala Siemens)
			 *	XS    - 1 GE, 8 bit, opt Z
			 */
#if IP20
			strcpy(gr2tmpl.Identifier,"SGI-GR2");
#else
			switch(pinfo->GfxBoardRev) {
			case 0:
			case 1:
			case 2:
			case 3:
				strcpy(gr2tmpl.Identifier,"SGI-GR2");
				break;
			case 4:
				strcpy(gr2tmpl.Identifier,"SGI-GR3");
				break;
			case 9:
			case 10:
			case 11:
				strcpy(gr2tmpl.Identifier,"SGI-GR5");
				break;
			case 5:
			case 6:
			case 7:
			case 8:
			default:
				strcpy(gr2tmpl.Identifier,"SGI-GU1");
				break;
			}
#endif
			switch (pinfo->GEs) {
				case 8:
					ext = "-Extreme";
					break;
				case 4:
					if (pinfo->Bitplanes == 24) {
						ext = "-Elan";
#if defined(IP22)
                                                if(is_indyelan())
                                                        ext = "-XZ";
#endif
						switch (pinfo->GfxBoardRev) {
							case 9:
							case 10:
							case 11:
                                                        	ext = "-XZ";
								break;
							default:
								break;
						}
					}
					else
						ext = "-XSM";
					break;
				case 2:
					ext = "-XZ";  
					if (pinfo->Bitplanes != 24) {
						strcat(gr2tmpl.Identifier,ext);
						ext = " missing bitplanes";
					}
					if (pinfo->Zbuffer == 0) {
						strcat(gr2tmpl.Identifier,ext);
						ext = " missing Z";
					}
					break;
				case 1:		/* 1 GE */
					if (pinfo->Bitplanes == 24)
						ext = "-XS24";
					else
						ext = "-XS";
					if (pinfo->Zbuffer) {
						strcat(gr2tmpl.Identifier,ext);
						ext = "Z";
					}
					break;
				default:
					ext = " unknown GE configuration";
					break;
			}
			strcat(gr2tmpl.Identifier, ext);

			gr2tmpl.IdentifierLength = strlen(gr2tmpl.Identifier);

			tmp = AddChild(root, &gr2tmpl, (void*)NULL);
			if (tmp == (COMPONENT*)NULL)
				cpu_acpanic("gr2");
			tmp->Key = gfxboard++;
			RegisterDriverStrategy(tmp, _gfx_strat);

			if (pinfo->MonitorType != 7) {
				tmp = AddChild(tmp, &montmpl, (void*)NULL);
				if (tmp == (COMPONENT*)NULL)
					cpu_acpanic("monitor");
			}
		}
	}
	return (1);
}


#if defined(IP19) || defined(IP21) || defined(IP25)

#ifdef IP19
/* This routine maps virtual address 0xf0000000 to 0xf0ffffff to
 * first 16Mb of the given pfn.
 *
 * Returns:
 *	virtual address of a 16Mb page that maps onto the given pfn.
 */
static void
*alloc_gfxspace(int pfn)
{
	__psunsigned_t va;
	int old_pgmask;
	union pte_int {
		pte_t	pte;
		int	x;
	} pteevn, pteodd;
	#define IOMAP_TLBPAGEMASK	0x1ffe000
	va = (unsigned)0xf0000000;

	pteevn.x = (((pfn) << 6) | (2 << 3) | (1 << 2) | (1 << 1) | 1);
	pteodd.x = 0;

	old_pgmask = get_pgmask();
	set_pgmask(IOMAP_TLBPAGEMASK);
	tlbwired(0,0,(caddr_t)va,pteevn.x,pteodd.x);  /* Use entry 0 only, it is reserved for graphics */
	set_pgmask(old_pgmask);

	return((void *)va);
}
#endif /* IP19 */

/*
 * Given a logical DANG number, find the physical slot
 * number of the IO4 attached to the given DANG, and return
 * the window number as well as the adapter number that is
 * associated with the given DANG chip.
 *
 * Returns:
 *	-1	Can't find the given DANG.
 *	Physical slot number of the IO4 connected to the given DANG
 */
static int
find_dang(int dangnum, int *window, int *adapter)
{
    evcfginfo_t *cfginfo = EVCFGINFO;
    evbrdinfo_t *brd;
    int slot;
    int dang_found = 0;
    int adap;
    /*
     * Start from the highest physical slot number.  The convention
     * is that the first DANG is attached to the IO4 located
     * at the highest physical slot.  The last DANG is attached
     * to the IO4 at the lowest physical slot.
     */
    for ( slot=EV_MAX_SLOTS ; slot-- ; ) {
	brd = &(cfginfo->ecfg_board[slot]);
	if  ( (brd->eb_type & (EVCLASS_MASK|EVTYPE_MASK)) == EVTYPE_IO4
	   && (brd->eb_enabled)
	    ) {
	    /* Found an IO4 board and it is enabled. 
	     * Now look at all the adapters in the IO4, and look
	     * for the DANG chip.
	     */
	    for (adap=1; adap<=7; adap++) { /* adapter numbers from 1 to 7 */
		if (brd->eb_un.ebun_io.eb_ioas[adap].ioa_type == IO4_ADAP_DANG) {
			if (dang_found == dangnum) {
				/* This is the DANG we are looking for */
				*window = brd->eb_un.ebun_io.eb_winnum;
				*adapter = adap;
				return(slot);
			}
			else {
				/* We found a DANG, but it
				 * isn't the DANG we are looking for, try again.
				 */
				dang_found++;
			}
		}
	    }
	}

    }
    return -1;
}

/*
 * Do various initialisation.
 * Set up all the DANG registers.
 * Allocate a virtual page to map to the EXPRESS area on the GIO.
 * Set up the write gatherer so that writes to it get dumped to the DANG.
 *
 * Returns:
 *	virtual address of GIO address 0x1f000000 (where EXPRESS sits).
 */
static void 
*everest_dang_init(int slot,int window,int adapter)
{
	volatile long long reg;
	int io_config_reg;
	void *va;

#if defined(IP19)
	/* running in 32bit mode, with 4KB page size */
	va = alloc_gfxspace(LWIN_PFN(window,adapter)+(0x3000000>>12));
#elif defined(IP21) || defined(IP25)
	/* running in 64bit mode, with 16KB page size */
	va = (void *)(((long long)(LWIN_PFN(window,adapter)) << 12) + 0x9000000003000000);
#endif
	/* Clear all DANG errors */
	reg = load_double((long long *)&dang_ptr[DANG_PIO_ERR_CLR]);
	reg = load_double((long long *)&dang_ptr[DANG_DMAM_ERR_CLR]);
	reg = load_double((long long *)&dang_ptr[DANG_DMAS_ERR_CLR]);
	reg = load_double((long long *)&dang_ptr[DANG_DMAM_COMPLETE_CLR]);
	/* Set up GIO address registers */
	store_double((long long *)&dang_ptr[DANG_UPPER_GIO_ADDR],(long long)7);
	store_double((long long *)&dang_ptr[DANG_MIDDLE_GIO_ADDR],(long long)0);
	/* Set up interrupt registers */
	store_double((long long *)&dang_ptr[DANG_INTR_BREAK],(long long)4); /*Make DANG stop sending to GIO when GIO interrupts the dang */
	/* All interrupt registers default to have interrupts disabled */
	/* Set up FIFO registers */
	store_double((long long *)&dang_ptr[DANG_WG_LOWATER],(long long)32);
	store_double((long long *)&dang_ptr[DANG_WG_HIWATER],(long long)1024);
	store_double((long long *)&dang_ptr[DANG_WG_FULL], (long long)0xfff);
	/* For now allow user process to access entire GR2 FIFO space */
	store_double((long long *)&dang_ptr[DANG_WG_PRIV_LOADDR], (long long)0);	/* 5 bits */
	store_double((long long *)&dang_ptr[DANG_WG_PRIV_HIADDR], (long long)0x1f);	/* 5 bits */

	store_double((long long *)&dang_ptr[DANG_WG_GIO_UPPER],(long long)0xf82);	/* Upper 15 bits of 0x1f040000 */
	store_double((long long *)&dang_ptr[DANG_WG_GIO_STREAM], (long long)0x1f040000);	/* GIO addr for streaming mode (32bits) */

	store_double((long long *)&dang_ptr[DANG_GIO64], (long long)0);	/* GIO 32 */
	store_double((long long *)&dang_ptr[DANG_MAX_GIO_TIMEOUT],(long long)1000000); /* 500K GIO clocks */

	/* Setup the IO4 configuration register since we use the Write Gatherer */
	io_config_reg = (int)IO4_GETCONF_REG(slot, 2);
	io_config_reg |= (0x10101 << adapter);
	IO4_SETCONF_REG(slot,2,io_config_reg);

	/* Set up write gatherer */
#if defined(IP19)
	store_double((long long *)EV_WGCNTRL,(long long)2); /* Big endian, disabled */
	store_double((long long *)EV_WGCLEAR,(long long)0); /* Clear WG */
	reg = (long long)(LWIN_PFN(window,adapter));
	reg <<= 12;
	reg |= (cpuid() << 8) | (1 << 15);
	store_double((long long *)EV_WGDST,reg);  /* Set up destination */
	store_double((long long *)EV_WGCNTRL,(long long)3); /* Big endian, enabled */
#elif defined(IP21)
	reg = (long long)(LWIN_PFN(window,adapter));
	reg <<= 12;
	reg |= (cpuid() << 8) | (1 << 15);
	store_double((long long *)EV_WGDST,reg);  /* Set up destination */
#elif defined(IP25)
	reg = (long long)(LWIN_PFN(window,adapter));
	reg <<= 12;
	reg |= (cpuid() << 8) | (1 << 15);
	store_double((long long *)EV_GRDST,reg);  /* Set up destination */
#endif
	
	return(va);
}

#endif
