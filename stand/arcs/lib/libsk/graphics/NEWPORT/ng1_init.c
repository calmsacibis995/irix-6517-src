#ident	"lib/libsk/graphics/NEWPORT/ng1_init.c:  $Revision$"

/*
 * ng1_init.c - initialization functions for NEWPORT graphics
 */

#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/cpu.h"
#include "arcs/hinv.h"
#include "sys/ng1hw.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/ng1.h"
#include "sys/vc2.h"
#include "sys/ng1_cmap.h"
#include "sys/xmap9.h"
#include "np_timing.h"
#include "sys/sbd.h"
#include "ide_msg.h"
#include "cursor.h"
#include "libsc.h"
#include "libsk.h"		/* for us_delay() prototype */
#include "sys/fpanel.h"

extern int _prom;
int debug_probe;

#define REX3_OLAY_MODE_8  ( DM1_OLAYPLANES | DM1_DRAWDEPTH8 | \
			DM1_HOSTDEPTH8 | DM1_COLORCOMPARE )
#define REX3_PUP_MODE   ( DM1_PUPPLANES | DM1_DRAWDEPTH8 | \
			DM1_HOSTDEPTH8 | DM1_COLORCOMPARE)
#define REX3_CID_MODE   ( DM1_CIDPLANES | DM1_DRAWDEPTH8 | \
			DM1_HOSTDEPTH8 | DM1_COLORCOMPARE | DM1_LO_SRC )

#define REX3_OLAY_WRMASK	0xFFFF00
#define REX3_PUP_WRMASK		0x0000CC
#define REX3_CID_WRMASK		0x000033

#define CURSOR_CMAP_MSB		0xE7

#define rex3Set( hwpage, reg, val ) \
    hwpage->set.reg = (val);

#define rex3SetConfig( hwpage, reg, val ) \
    hwpage->p1.set.reg = (val);

#define rex3GetConfig( hwpage, reg, val ) \
    val = hwpage->p1.set.reg;

#define rex3SetAndGo( hwpage, reg, val ) \
    hwpage->go.reg = (val);

extern unsigned char vc1_ptr_bits[];
extern unsigned char vc1_ptr_ol_bits[];

extern struct tp_fncs ng1_tp_fncs;

struct ng1_timing_info *ng1_timing[MAXNG1];
struct ng1_info ng1_ginfo[MAXNG1];

/*
 * Timing tables for ng1 rev 3+ are not compatible with r2,
 * so we have 2 tables for each monitor/resolution.
 */
typedef struct {
        struct ng1_timing_info *n1024_70;
        struct ng1_timing_info *n1024_60;
        struct ng1_timing_info *n1280_60;
        struct ng1_timing_info *n1280_72;
        struct ng1_timing_info *n1280_76;
	struct ng1_timing_info *presenter;	/* lo-res flat panel display */
	struct ng1_timing_info *presenter_hires;  /* Hi-res flat panel display */
} timing_tables;

static timing_tables *fudge[MAXNG1];    /* point to one of the below */

static timing_tables old_fudge = {
        &n1024_70_info, &n1024_info, &n1280_info,
        &np1280_72_info, &sony_1280_76_info,
	&mitsubishi_r3_info, &mit_hires_r3_info
};

static timing_tables new_fudge = {
        &n1024_70_r3_info, &n1024_r3_info, &n1280_r3_info,
        &np1280_72_r3_info, &sony_1280_76_r3_info,
	&mitsubishi_r3_info, &mit_hires_r3_info
};

static timing_tables newest_fudge = {
        &n1024_70_r4_info, &n1024_r4_info, &n1280_r4_info,
        &np1280_72_r4_info, &sony_1280_76_r4_info,
	&mitsubishi_r4_info, &mit_hires_r4_info
};

int bt445_xbias[2];

static int vcogain[2];		/* XXX This is semi-monitor dependent, should
			 	 * come from np_timing.h */

unsigned char   lg3_version[2];

struct rex3chip *rex3base[] = {
	/* Order is important for GIO64 set-up */
	(struct rex3chip *)PHYS_TO_K1(REX3_GIO_ADDR_0),
#if MAXNG1 > 1
	(struct rex3chip *)PHYS_TO_K1(REX3_GIO_ADDR_1),
#endif
#if MAXNG1 > 2
	(struct rex3chip *)PHYS_TO_K1(REX3_GIO_ADDR_2),
#endif
	(struct rex3chip *)NULL
};

#define INDIGO2_ONLY		0x0001		/* only probe on Indigo2 */

int rex3flags[] = {
	0,
#if MAXNG1 > 1
	0,
#endif
#if MAXNG1 > 2
	INDIGO2_ONLY,
#endif
};

static COMPONENT ng1tmpl = {
	ControllerClass,		/* Class */
	DisplayController,		/* Type */
	ConsoleOut|Output,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	16,				/* IdentifierLength */
	""				/* Identifier */
};

extern COMPONENT montmpl;		/* defined in gfx.c */

/*
 * gfifo high water = 16/32, bfifo high water = 8/16
 * {g,b} fifo high interrupts disabled
 * 24 bit fb (XXX), minimum vram refresh
 */

int rex3_config_default;
int rex3_config_default_init = (1 << VREFRESH_SHIFT)
#ifdef GFX_GIO64
                        | BUSWIDTH
#endif
			;

int
Ng1Index(struct rex3chip *rex3)
{
	int i;

	for (i=0; rex3base[i]; i++) {
		if (rex3base[i] == rex3)
			return(i);
	}
	return(0);			/* should never happen -- assume 0 */
}

/******************************************************************************
 *
 * Check to see if a private bus NG1 type board is there.  
 *
 * Returns:
 *	0: board not found
 *	1: found board, part of info struct filled in
 *
 *****************************************************************************/
int Ng1Probe(struct rex3chip *rex3, int idx)
{
	int mask = GIO64_ARB_GRX_SIZE_64 << idx;
	extern uint _gio64_arb;
	uint gio64_arb_save;
	int i;

	if(!(_prom) && (debug_probe))
		ttyprintf("Checking if REX3 present\n");


	/* Only probe for ng1 on boards earlier than Rev. 4 */
	if ( idx && ! is_fullhouse()) {
		if ( ( (*(unsigned int *)(PHYS_TO_K1(HPC3_SYS_ID)) & 
				BOARD_REV_MASK)>>BOARD_REV_SHIFT) < 4 )
		{
		    return 0;
		}
	}

	/*
	 * Before reading from Rex3, must configure the bus interface.
	 */
	

	/* OR in EXTREGXCVR bit here since 
	 * different routines call Ng1Probe() 
	 */ 
	if (is_fullhouse()) {
		rex3_config_default |= EXTREGXCVR;

		/* GIO BUS timeout value for Indigo2 Newport */
		rex3_config_default &= ~TIMEOUT_MASK ;
		rex3_config_default |= 3 << TIMEOUT_SHIFT;
	}
	
	gio64_arb_save = _gio64_arb;
	if (rex3_config_default & BUSWIDTH)
		_gio64_arb |= mask;
	else
		_gio64_arb &= ~mask;

	/* Indicate a pipelined device on this slot. */
	if ( idx && !is_fullhouse())
		_gio64_arb |= GIO64_ARB_EXP0_PIPED << (idx-1);

	*(unsigned int *)(PHYS_TO_K1(GIO64_ARB)) = _gio64_arb;


	/* original IP22 boards do not have all three address spaces.
	 */
	if (badaddr(rex3,4)) {
		return 0;
	}

	if(!(_prom) && (debug_probe)) {
		ttyprintf("Writing CONFIG register at 0x%x with 0x%x\n",
			rex3+0x1330, rex3_config_default);
	}
	rex3->p1.set.config = rex3_config_default;

#define	PROBE_TIMEOUT	100000
#define	PROM_BASE	0xbfc00000

	for (i = 0; i < PROBE_TIMEOUT; i++)
		if (rex3->p1.set.status & GFXBUSY)
			*(volatile uint *)PROM_BASE;	/* ~1 usec */
		else
			break;

	if (i == PROBE_TIMEOUT)
		goto nocard;    /* no hw present, or Rex is hung */

	/*
	 * Write an int into the 16-bit xstarti register.
	 * Read it back from the fixed-point xstartf register.
	 * The format is 16 bits of integer, 4 bits of fraction,
	 * all << 7.  In this case, the fraction is 0, so we expect
	 * to read (0x12348765 & 0xffff) << 11.
	 */

	if(!(_prom) && (debug_probe)) {
		ttyprintf("Writing XSTARTI Reg 0x%x with 0x12348765\n",
				rex3+0x0148);
	}
	rex3->set.xstarti = 0x12348765;
	if(!(_prom) && (debug_probe)) {
		ttyprintf("Reading XSTART Reg 0x%x should get 0x%x\n",
			rex3+0x0100, ((0x12348765 & 0xffff) << 11));
	}
	if ( rex3->set._xstart.word == ((0x12348765 & 0xffff) << 11) )
		return 1;

nocard:
	_gio64_arb = gio64_arb_save;
	*(unsigned int *)(PHYS_TO_K1(GIO64_ARB)) = _gio64_arb;

	return 0;
}

static void Ng1InitInfo(struct rex3chip *rex3, struct ng1_info *info)
{
	unsigned long int tmp;
	int panel;
	int idx = Ng1Index(rex3);
	int fp_monitor=0;

				/* gfx info */
	strncpy(info->gfx_info.name, GFX_NAME_NEWPORT, GFX_INFO_NAME_SIZE);

	info->gfx_info.length = sizeof(struct ng1_info);

	Bt445Get (rex3, RDAC_CRS_REVISION, RDAC_REV_REG, tmp);
	info->bt445rev = tmp >> 4;

        /*
         * (Guinness)
         * Read board revision from CMAP 0.
         * If boardrev > 2, the msb of cmap0 revision indicates
         * frame buffer depth.
         * (Fullhouse)
         * Board rev derived from lg3_version.
         * (All)
         * Read cmap revision from cmap0.
         */

	BFIFOWAIT( rex3 );

        cmapGetReg (rex3, CMAP_CRS_REV_REG, 0, tmp);
        info->cmaprev = tmp & 0x07;

	if (is_fullhouse()) {
		if (lg3_version[idx] == LG3_BD_001)
			info->boardrev = 1;
		else if (lg3_version[idx] == LG3_BD_002)
			info->boardrev = 2;
		else info->boardrev = lg3_version[idx];
	}
	else {
		tmp >>= 4;
		info->boardrev = tmp & 7;
		if (idx)
			info->boardrev += 8;	/* ng2 */
		if (info->boardrev > 1)
			info->bitplanes = (tmp & 8) ? 8 : 24;
	}

	/*
	 * Point fudge to the appropriate set of
	 * timing tables for this rev of ng1.
	 */
        if (info->boardrev < 3)
                fudge[idx] = &old_fudge;
        else if (info->boardrev > 3 && info->bt445rev > 0xa)
                fudge[idx] = &newest_fudge;
	else
                fudge[idx] = &new_fudge;

  	/* if flat panel, set fp_monitor and the monitor type */
  
	if (ng1_i2cProbe(rex3) && i2cSetup() && i2cPanelID(&panel)) {
        	fp_monitor = 1;
    		switch (panel) {
#if 0 /* flat panels not used in corona */
    	        case PANEL_XGA_SHARP:
      			info->monitortype = 128;        /* hack */
      			break;
    		case PANEL_XGA_NEC:
      			info->monitortype = 129;        /* hack */
      			break;
#endif
    		case PANEL_XGA_MITSUBISHI:
      			info->monitortype = MONITORID_CORONA;
			break;
		    case PANEL_EWS_MITSUBISHI:
			info->monitortype = MONITORID_ICHIBAN;        
      			break;
    		default:
      			fp_monitor = 0;
    		}
  	}

        /*
         * Read the monitor type from the revision register on CMAP 1.
         */

	cmapGetReg (rex3, CMAP_CRS_REV_REG, 1, tmp);
	
	if(!fp_monitor) {
        	info->monitortype = ( tmp >> 4 ) & 0x0f;
	}

        /*
         * XXX If both cmaps aren't the same revision,
         * say we have the older revision installed.
         */
        if ((tmp &= 7) < info->cmaprev)
        	info->cmaprev = tmp;

        /* Initialize RAMDAC for using internal or external PLL */
	vcogain[idx] = 5;
	if(info->boardrev == 0 || info->boardrev > 5)
		vcogain[idx] |= 0x80;

	DELAY(100*1000);        /* wait 100 ms */
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_PLL_CTRL, vcogain[idx] );

	vc2GetReg (rex3,VC2_CONFIG,tmp);
	info->vc2rev = (tmp >> 2) & 0xF;

	tmp = xmapRevision(rex3);
	info->xmap9rev = tmp & 0x07;

	info->rex3rev = rex3->p1.set.status & REX3VERSION_MASK;
        info->mcrev = 1;
}

/*
 * init the bt445
 */
void Ng1DacInit(struct rex3chip *rex3, struct ng1_info *info)
{
	int idx = Ng1Index(rex3);
	int i, j;
	char PLLbyte;
	char *cp;

	if(!(_prom) && (debug_probe))
		ttyprintf("Initializing BT445\n");
	/*
	 * Set up for whatever monitor we have.
	 */
	switch (info->monitortype) {

	case 1:		/* New Sony multiscan 19" */
	case 2:		/* New Sony multiscan 16" */
	case 11:        /* Hitachi multiscan 21" */

		/* 1280x1024 @ 76 Hz */
		ng1_timing[idx] = fudge[idx]->n1280_76;
		break;

	case 9:		/* Dual-sync 19" Mitsubishi or
				  single-scan Sony. */

		/* 1280x1024 @ 72 Hz */
		ng1_timing[idx] = fudge[idx]->n1280_72;
		break;

	case 6:		/* New Wyse 15" single scan */

		/* 1024x768 @ 70Hz*/
		ng1_timing[idx] = fudge[idx]->n1024_70;
		break;

	case 13:	/* Dual scan Sony 16", Iris Indigo */
	case 12:	/* Dual scan Sony 19", Iris Indigo */
	case 10:	/* 16" Mitsubishi */

		/* 1280x1024 @ 60Hz */
		ng1_timing[idx] = fudge[idx]->n1280_60;
		break;
#if 0 /* sharp and nec flat panels not used in corona */
    	case 128 :      /* sharp 1024x768 flat panel */
      		ng1_timing[idx] = &sharp_info;
      		break;      
    	case 129 :      /* nec 1024x768 flat panel */
      		ng1_timing[idx] = &nec_info;
      		break;
#endif
    	case MONITORID_CORONA: /* Mitsubishi 1024x768 flat panel */
		ng1_timing[idx] = fudge[idx]->presenter;
      		break;
	case MONITORID_ICHIBAN:      /* mitsubishi 1280x1024 flat panel */
		ng1_timing[idx] = fudge[idx]->presenter_hires;
		break;

	default:	/* Unknown */
	case 0:		/* Unknown */
	case 15:	/* Uknown, maybe old Hitachi */
		if (is_fullhouse()) {
			/* 1280x1024 @ 60Hz */
			ng1_timing[idx] = fudge[idx]->n1280_60;
		}
		else {
			/* Default to low-res, unless ... */
			if (cp = getenv ("monitor")) {
				if (*cp == 'h' || *cp == 'H') {
					/* 1280x1024 @ 60Hz */
					ng1_timing[idx] = fudge[idx]->n1280_60;
					break;
				}
				else if (*cp == 'S') {
					/* "Super Res": 1280x1024 @ 76 Hz */
					ng1_timing[idx] = fudge[idx]->n1280_76;
					break;
				}
			}
			/* 1024x768 @ 60Hz*/
			ng1_timing[idx] = fudge[idx]->n1024_60;
		}
		break;
	}
        /*
         * Don't default to 76Hz unless ng1 rev > 3
         */

        if (ng1_timing[idx] == fudge[idx]->n1280_76 && info->boardrev < 4)
                ng1_timing[idx] = fudge[idx]->n1280_72;

        bt445_xbias[idx] = (ng1_timing[idx]->flags & NG1_VOF_EXTRAPIX_MASK) >>
                                                (NG1_VOF_EXTRAPIX_SHIFT-1);

	info->gfx_info.xpmax = ng1_timing[idx]->w;
	info->gfx_info.ypmax = ng1_timing[idx]->h;

	/*
	 * Put vc2 in reset (since the pixel clock will change)
	 */
	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */
	vc2SetReg (rex3, VC2_CONFIG, 0);	
	Bt445Set( rex3, RDAC_CRS_REVISION, RDAC_COMMAND_REG0, 0x40 );

	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */

	/*
	 * We use a 25 bit pixel. Bits 0-7 red, 8-15 green, 16-23 blue.
	 * The palette bypass bit is bit 24.  We send 2 pixels at a time
	 * to the 64 bit ramdac pixel input port.  The first pixel is
	 * in bits 0-24, the second in bits 32-56.  We set the pixels size to
	 * 32 bits, bits 25-31 are just padding.
	 */
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_DEPTH_CTRL, 32 );
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_PXL_START_POS, 0 );
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_PAL_BYPASS_POS, 24 );
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_FORMAT_CTRL, 0x82 );

	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */

	/* Red - 0-7 */
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_RED_MSB_POS, 7);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_RED_WIDTH, 8);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_RED_DISPLAY_ENA, 0xff);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_RED_BLINK_ENA, 0);
	
	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */
	
	/* Green - 8-15 */
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_GREEN_MSB_POS, 15);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_GREEN_WIDTH, 8);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_GREEN_DISPLAY_ENA, 0xff);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_GREEN_BLINK_ENA, 0);
	
	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */
	
	/* Blue - 16-23 */
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_BLUE_MSB_POS, 23);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_BLUE_WIDTH, 8);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_BLUE_DISPLAY_ENA, 0xff);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_BLUE_BLINK_ENA, 0);
	
	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */
	
	/* Bypass : bit 24, Ovl : 25-28 */
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_OVL_MSB_POS, 28);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_OVL_WIDTH, 4);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_OVERLAY_CTRL, 0);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_OVL_BLINK_ENA, 0);
	
	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */
	
	/* Cursor : 29-31 */
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_CURS_MSB_POS, 31);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_CURS_WIDTH, 3);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_CURSOR_CTRL, 0);
	Bt445Set(rex3, RDAC_CRS_RGB_CTRL, RDAC_CURS_BLINK_ENA, 0);

	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */

	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_PLL_RATE0,
		  ng1_timing[idx]->plltab[0] );
	DELAY(100*1000);	/* wait 100 ms */
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_PLL_RATE0,
		  ng1_timing[idx]->plltab[0] );
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_PLL_RATE1,
			( ng1_timing[idx]->plltab[2] << 6 ) |
			ng1_timing[idx]->plltab[1] );
	DELAY(100*1000);	/* wait 100 ms */

	{
        	int pll;
        	pll = ng1_timing[idx]->cfreq << ng1_timing[idx]->plltab[2];

        	if (pll < 90)
                	vcogain[idx] = 7;
        	else if (pll < 120)
                	vcogain[idx] = 6;
        	else if (pll < 140)
                	vcogain[idx] = 4;
        	else
                	vcogain[idx] = 1;
        }

	/* Fullhouse selects the ICS1562 PLL,
         * Guiness selects ICSPLL for p1 board only.
         */

        if (!is_fullhouse()) {
                if(info->boardrev == 0 || info->boardrev > 5)
                        vcogain[idx] |= 0x80;
        }
        else if (lg3_version[idx] == LG3_BD_001)     /* if version 001 then no ICS */                vcogain[idx] |= 0x80;


	if ((vcogain[idx] & 0x80) == 0)
        	for (i=0; i<7; i++) {
                	PLLbyte = ng1_timing[idx]->icsplltab[i];
                	for (j=0;j<8;j++) {
                        	if ((j == 7) && (i== 6)) {
                                	Ics1562Set( rex3, (PLLbyte & 0x1)|0x2);
                        	}
                        	else  {
                                	Ics1562Set( rex3, PLLbyte & 0x1);
                        	}
                        	PLLbyte = PLLbyte >> 1;
                	}
        	}
	
	DELAY(100*1000);	/* wait 100 ms */
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_PLL_CTRL, vcogain[idx] );

	DELAY(100*1000);	/* wait 100 ms */
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_VIDCLK_CTRL, 0x01 );

	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */

	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_MPX_RATE, 0x01 );

	/*
	 * Wait for bt445 PLL to lock up.  This can 
	 * take a while - try 100ms for now.
	 */

	DELAY (100 * 1000);

	/*
	 * Reset the pipeline depth (bit 0 -> 0->1).  
	 * Set for sync on green and blank=black.
	 */
 	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_COMMAND_REG1, 0 );
	BFIFOWAIT( rex3 ); /* Each Bt445 macro is 4 writes to backend */
 	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_COMMAND_REG1, 0xc1 );

	/* Enable VIDCLK out, and un-reset vc2. */
	Bt445Set( rex3, RDAC_CRS_SETUP, RDAC_PXL_LOAD_CTRL, 0x04 );

	vc2SetReg (rex3, VC2_CONFIG, 1);
}

void Ng1XmapInit(struct rex3chip *rex3, struct ng1_info *info)
{
	int i;
	int idx = Ng1Index(rex3);

	BFIFOWAIT( rex3 );
	if(!(_prom) && (debug_probe))
		ttyprintf("Initializing XMAP9\n");

	/*
	 * Set 8/24 bit frame buffer, enable PUP planes
	 */
	if (info->monitortype & 0x80){ /* flat panel attached */
    		xmap9LoadConfig(rex3, XM9_PUPMODE | XM9_8_BITPLANES | 
                                XM9_EXPRESS_VIDEO | XM9_VIDEO_OPTION | 
                                (1 << XM9_VIDEO_RGBMAP_SHIFT));
  	} else if ( info->bitplanes == 24 ) {
	    xmap9LoadConfig( rex3, 0x01 );
	} else {
	    xmap9LoadConfig( rex3, 0x05 );
	}

	/*
	 * Cursor and PUP data use color palette 0.
	 */
	BFIFOWAIT( rex3 );
        xmap9SetReg (rex3, XM9_CRS_CURS_CMAP_MSB, CURSOR_CMAP_MSB);
	xmap9SetReg (rex3, XM9_CRS_PUP_CMAP_MSB, 0);

	/*
	 * Set mode register 0 for 8 bit CI.
	 */
	for(i = 0; i < 32; i++){
	    xmap9FIFOWait( rex3 );
	    xmap9SetModeReg( rex3, i, XM9_PIXSIZE_8, ng1_timing[idx]->cfreq);
	}
}


static void
SetGammaIdentity( struct rex3chip *rex3)
{

	/* 
	 *  Booooring, just set it to the identity for now.
	 */

	int i;
	Bt445SetAddr( rex3, 0 );

	for ( i = 0; i < 256; i++ ) {
	    if ( ( i & 0x0F ) == 0x0F )
		BFIFOWAIT( rex3 );

	    Bt445SetRGB( rex3, i, i, i );
	}
}

void Ng1CmapInit(struct rex3chip *rex3, struct ng1_info *info)
{
	int i,j;

	/*
	 * The CMAP has a 64 location FIFO that should be checked to
	 * prevent glitches on the screen.  But since overflowing this
	 * FIFO just forces entries to be written (instead of dropping
	 * them) and since we don't care about glitches at initialization
	 * we won't check it right now.
	 *
	 */
	BFIFOWAIT( rex3 );

	if(!(_prom) && (debug_probe))
		ttyprintf("Initializing CMAP\n");

        /*
         * Clear rev d3 special diag mode,
         * just in case.
         */

        if (info->cmaprev > 1) {
                cmapGetReg(rex3, CMAP_CRS_DIAG_READ, 0, i);
                cmapGetReg(rex3, CMAP_CRS_DIAG_READ, 1, i);
        }


	/*  tport sets up color map for console head, but we must ensure
	 * index 0 is black for screen blanking when console is not on
	 * this head (console=d or multi-head).
	 */
	/* Disable CBLANK before writing to CMAP & then Enable it */

	cmapSetReg (rex3, CMAP_CRS_CMD_REG, 7);
	cmapSetAddr( rex3, 0 );
	cmapSetRGB( rex3, 0, 0, 0);
	cmapSetReg (rex3, CMAP_CRS_CMD_REG, 3);

	/*
	 * Initialize the RGB (sort of gamma) ramps
	 */
	BFIFOWAIT( rex3 );

	/* Set the start address at  0x1D00  ( 8K - 768 ) */

	for ( j = 0; j < 3; j++ )
	    for ( i = 0; i < 256; i++ ) {
		if ( (i & 0x0F) == 0x0F )
		    BFIFOWAIT(rex3);
		/* Setting the address twice every time we check cmapFIFOWAIT 
		 * is a s/w work around for the cmap problem. The address
		 * is lost ramdomly when the bus is switched from write to
	 	 * read mode or vice versa.
		 */

		if ((i & 0x1F) == 0x1F) {
		    cmapFIFOWAIT(rex3);
		    cmapSetAddr(rex3, 0x1D00 + (j * 256) + i);
		    cmapSetAddr(rex3, 0x1D00 + (j * 256) + i);
		}

		cmapSetReg (rex3, CMAP_CRS_CMD_REG, 7);
		cmapSetAddr(rex3, 0x1D00 + (j * 256) + i);
		cmapSetRGB( rex3, i, i, i);
		cmapSetReg (rex3, CMAP_CRS_CMD_REG, 3);
	    }

	/*
	 * Set FIFO to trip on half full
	 */
	cmapSetReg (rex3, CMAP_CRS_CMD_REG, 3);

	SetGammaIdentity( rex3 );

	return;
}

void
vc2LoadSRAM(struct rex3chip *rex3, unsigned short *data, unsigned int addr, unsigned int length)
{

/* 
 *  data points to shorts; length is in shorts.
 */
	int i;

	BFIFOWAIT( rex3 );
	vc2SetupRamAddr(rex3, addr);

	for (i = 0; i < length; i++)
	{
	    if ( (i & 0x0F) == 0xF )
		BFIFOWAIT( rex3 );

	    vc2SetRam(rex3, data[i]);
	}
	return;
}

static unsigned short did2_linetable[] = { 0x0000, 0x7ff << 5 };

void Ng1VC2Init(struct rex3chip *rex3, struct ng1_info *info)
{
	int idx = Ng1Index(rex3);
	int i;

	BFIFOWAIT( rex3 );
	if(!(_prom) && (debug_probe))
		ttyprintf("Initializing VC2\n");

	if ( ng1_timing[idx]->cfreq < 70 ) {
	    vc2SetReg( rex3, VC2_CONFIG, 0x03 );
	}
	else {
	    vc2SetReg( rex3, VC2_CONFIG, 0x01 );
	}
	vc2LoadSRAM( rex3, ng1_timing[idx]->ltab, VC2_VID_LINE_TAB_ADDR,
					ng1_timing[idx]->ltab_len);
	vc2LoadSRAM( rex3, ng1_timing[idx]->ftab, VC2_VID_FRAME_TAB_ADDR,
					ng1_timing[idx]->ftab_len);
	vc2SetReg( rex3, VC2_VIDEO_ENTRY_PTR, VC2_VID_FRAME_TAB_ADDR );


	/*
	 * Load the DID table
	 */
	if(!(_prom) && (debug_probe))
		ttyprintf("Loading DID Tables.\n");

	vc2SetupRamAddr (rex3, VC2_DID_FRAME_TAB_ADDR);
	i = ng1_timing[idx]->h;
	BFIFOWAIT (rex3);
	while (i-- > 0) {
		vc2SetRam (rex3, VC2_DID_LINE_TAB_ADDR);
		if ((i & 0xf) == 0)
			BFIFOWAIT (rex3);
	}
	vc2SetRam (rex3, 0xffff);	/* end of table flag */

	vc2LoadSRAM( rex3, did2_linetable, VC2_DID_LINE_TAB_ADDR,
                                     sizeof(did2_linetable)/sizeof(short) );

	BFIFOWAIT( rex3 );

	vc2SetReg( rex3, VC2_DID_ENTRY_PTR, VC2_DID_FRAME_TAB_ADDR);

        vc2SetReg( rex3, VC2_SCAN_LENGTH,
				((ng1_timing[idx]->w + 2*bt445_xbias[idx])<< 5));

        /*
         * Enable video timing so we can init xmap and cmap.
         * We'll enable the rest of the vc2 functions later.
         * XXX gensync and interlace.
         */

        vc2SetReg( rex3, VC2_DC_CONTROL, VC2_ENA_VIDEO);
}

static void
getfbdepth (struct rex3chip *rex3, struct ng1_info *info)
{
	register unsigned int tmp;
	/*
	 * Do an experiment to see if frame buffer is 8/24 bits deep.
	 * Write a 12 bit pixel in CI mode, read it back.  If all
	 * 12 bits are there , it's a 24 bit frame buffer, else 8.
	 * Wait a little for vram refresh to get started, since this
	 * code is called shortly after vc2 starts generating vram
	 * refresh signals.
	 */
	DELAY (1000);	/* XXX long enough ? */

	REX3WAIT (rex3);
	rex3->set.wrmask = 0xfff;
	rex3->set.xystarti = 0;
	rex3->set.xyendi = 1 << 16 | 0;
	rex3->set.drawmode1 = DM1_COLORCOMPARE | DM1_RGBPLANES | 
			      DM1_DRAWDEPTH12 | DM1_HOSTDEPTH12 | 
			      DM1_RWPACKED | DM1_LO_SRC | DM1_ENPREFETCH;
	rex3->set.drawmode0 = DM0_COLORHOST | DM0_DOSETUP | 
			      DM0_DRAW | DM0_BLOCK; 
	rex3->go.hostrw0 = 0xabc << 16 | 0xdef;
	REX3WAIT (rex3);
	rex3->set.xystarti = 0;
	rex3->go.drawmode0 = DM0_DOSETUP | DM0_READ | DM0_BLOCK | 
			     DM0_COLORHOST;
	REX3WAIT (rex3);
	tmp = rex3->go.hostrw0;

	/*
	 * XXX Maybe should check for tmp & 0xff == 0xbc
	 * Maybe should check a bunch of pixels, but doesn't
	 * the prom do a vram test ?  Maybe should ignore the
	 * low byte - it's not in question here !
	 */
	if ((tmp & 0x0fff0fff) == 0x0abc0def) {
        	info->bitplanes = 24;
	}
	else {
        	info->bitplanes = 8;
	}
	REX3WAIT (rex3);
	rex3->p1.set.config = rex3_config_default;
}

void Ng1RegisterInit(struct rex3chip *rex3, struct ng1_info *info)
{
    int j;


    BFIFOWAIT(rex3);

    /*
     * Take vc2 out of reset, and disable video timing etc
     */
    vc2SetReg (rex3, VC2_CONFIG, VC2_RESET);
    vc2SetReg (rex3, VC2_DC_CONTROL, 0);

    Ng1DacInit( rex3, info );

    /*
     * Order is important here.  vc2 must be generating
     * video timing in order for xmap and cmap to read
     * mode reg / color palette data from their respective fifos.
     * Also, vram uses video timing signals to refresh itself,
     * so vc2 must be operating before we can do the frame buffer
     * depth check, which must preceed initXMAP9 (xmap needs to
     * know if fb is 8/24).
     */

    Ng1VC2Init( rex3, info );	/* video timing is enabled now */
    if (info->monitortype & 0x80)
      i2cPanelOn(); /* turn on the flat panel */
    if((info->boardrev < 2) || (is_fullhouse()))
    	getfbdepth( rex3, info );
    Ng1XmapInit( rex3, info );
    Ng1CmapInit( rex3, info );

    BFIFOWAIT(rex3);
    /*
     * Enable all vc2 functions except display.
     */

    vc2GetReg (rex3, VC2_DC_CONTROL, j);
    j |= VC2_ENA_VINTR|VC2_ENA_DIDS|VC2_ENA_CURS_FUNCTION|
				VC2_ENA_CURS_DISPLAY|VC2_ENA_DISPLAY;
    vc2SetReg (rex3, VC2_DC_CONTROL, j);

    REX3WAIT(rex3);

    if(!(_prom) && (debug_probe))
	ttyprintf("Initializing REX3\n");

    rex3Set( rex3, lsmode, 0x0);
    rex3Set( rex3, lspattern, 0x0);
    rex3Set( rex3, lspatsave, 0x0);
    rex3Set( rex3, zpattern, 0x0);
    rex3Set( rex3, colorback, 0xdeadbeef);
    rex3Set( rex3, colorvram, 0xffffff);
    rex3Set( rex3, smask0x, 0x0);
    rex3Set( rex3, smask0y, 0x0);
    rex3Set( rex3, xsave, 0x0);
    rex3Set( rex3, xymove, 0x0);

    REX3WAIT(rex3);

    rex3Set( rex3, bresd.word, 0x0);
    rex3Set( rex3, bress1.word, 0x0);
    rex3Set( rex3, bresoctinc1, 0x0);
    rex3Set( rex3, bresrndinc2, 0xff000000);
    rex3Set( rex3, brese1, 0x0);
    rex3Set( rex3, bress2, 0x0);
    rex3Set( rex3, aweight0, 0x0);
    rex3Set( rex3, aweight1, 0x0);
    rex3Set( rex3, colorred.word, 0x0);
    rex3Set( rex3, colorgrn.word, 0x0);
    rex3Set( rex3, colorblue.word, 0x0);
    rex3Set( rex3, coloralpha.word, 0x0);
    rex3Set( rex3, wrmask, 0xff);

	/*
	 * Need some black pixels before the first 'real' pixels
	 * on each scan line.  To make sure they *stay* black, we
	 * do all ordinary rendering with xywin set to make those
	 * pixels untouchable.  The only exception is in rexClear,
	 * where we want to be sure and zero those pixels.
	 */
    rex3SetConfig(rex3, xywin, 0x10001000 + (bt445_xbias[Ng1Index(rex3)] << 16));
    rex3SetConfig(rex3, clipmode, 0x1e00);
    rex3SetConfig(rex3, topscan, 0x3ff);
}

void
rex3Clear (register struct rex3chip *rex3, struct ng1_info *info)
{
	int j;
	int _xywin;
	int idx = Ng1Index(rex3);

	BFIFOWAIT(rex3);

	if (_prom) {
	    vc2GetReg (rex3, VC2_DC_CONTROL, j);
	    j &= ~VC2_ENA_DISPLAY;
    	    vc2SetReg (rex3, VC2_DC_CONTROL, j);
	}

        REX3WAIT(rex3);

	rex3GetConfig(rex3, xywin, _xywin);	/* save callers xywin */
	rex3SetConfig(rex3, xywin, 0x10001000);	/* set to draw all pixels */

        rex3Set( rex3, drawmode0, DM0_DRAW | DM0_BLOCK |
            DM0_DOSETUP | DM0_STOPONX | DM0_STOPONY);

        /* clear CID plane */
	if(!(_prom) && (debug_probe))
		ttyprintf("Clearing CID planes\n");

        rex3Set( rex3, wrmask, REX3_CID_WRMASK);
        rex3Set( rex3, drawmode1, REX3_CID_MODE | DM1_COLORCOMPARE);
        rex3Set( rex3, colori, 0 );
        rex3Set( rex3, xystarti, 0);
        rex3SetAndGo( rex3, xyendi, ((1280+63) << 16) | (ng1_timing[idx]->h));

        REX3WAIT(rex3);

        /* clear PUP plane */
	if(!(_prom) && (debug_probe))
		ttyprintf("Clearing PUP planes\n");

        rex3Set( rex3, wrmask, REX3_PUP_WRMASK);
        rex3Set( rex3, drawmode1, REX3_PUP_MODE | DM1_LO_SRC);
        rex3Set( rex3, colori, 0 );
        rex3Set( rex3, xystarti, 0);
        rex3SetAndGo( rex3, xyendi, ((1280+63) << 16) | (ng1_timing[idx]->h));

        REX3WAIT(rex3);

        if (info->bitplanes == 24){
                /* clear OLAY plane */
		if(!(_prom) && (debug_probe))
			ttyprintf("Clearing OLAY planes\n");

                rex3Set( rex3, wrmask, REX3_OLAY_WRMASK);
                rex3Set( rex3, drawmode1, REX3_OLAY_MODE_8 | DM1_LO_SRC);
                rex3Set( rex3, colori, 0 );
                rex3Set( rex3, xystarti, 0);
                rex3SetAndGo( rex3, xyendi, ((1280+63) << 16)|(ng1_timing[idx]->h));

                REX3WAIT(rex3);

                /* 24 bit fb clear */

		if(!(_prom) && (debug_probe))
			ttyprintf("Clearing 24 bit Frame Buffer\n");

                rex3Set( rex3, wrmask, 0xffffff);
                rex3Set(rex3, drawmode1, ( DM1_RGBPLANES | DM1_DRAWDEPTH24 |
                        DM1_HOSTDEPTH32 | DM1_COLORCOMPARE | DM1_LO_SRC));
        }
        else {
                REX3WAIT(rex3);

                /* 8 bit fb clear */
		if(!(_prom) && (debug_probe))
			ttyprintf("Clearing 8 bit Frame Buffer\n");

                rex3Set( rex3, wrmask, 0xff);
                rex3Set(rex3, drawmode1, ( DM1_RGBPLANES | DM1_DRAWDEPTH8 |
                        DM1_HOSTDEPTH8 | DM1_COLORCOMPARE | DM1_LO_SRC));
        }
        rex3Set( rex3, colori, 0 );
        rex3Set( rex3, xystarti, 0);
        rex3SetAndGo( rex3, xyendi, ((1280+63) << 16)|(ng1_timing[idx]->h));

        REX3WAIT(rex3);

        /* always 8 bit CI for the rest of operations */
        rex3Set(rex3, drawmode1, ( DM1_RGBPLANES | DM1_DRAWDEPTH8 |
            DM1_HOSTDEPTH8 | DM1_COLORCOMPARE | DM1_LO_SRC));

	rex3SetConfig(rex3, xywin, _xywin);	/* restore xywin */

	BFIFOWAIT(rex3);

	if (_prom) {
	    vc2GetReg (rex3, VC2_DC_CONTROL, j);
	    j |= VC2_ENA_DISPLAY;
    	    vc2SetReg (rex3, VC2_DC_CONTROL, j);
	}
}

void Ng1CursorInit(struct rex3chip *rex3,
	int fr, int fg, int fb, int br, int bg, int bb)
{
/* XXX Set the cursor colors ! */
	int i;
	unsigned char buf[256];

	if(!(_prom) && (debug_probe))
		ttyprintf("Initializing Cursor\n");

	/* load null cursor glyph */

        BFIFOWAIT( rex3 );

	/* load cursor glyph */

	bzero(buf, sizeof(buf));
	for (i = 0; i < 16; i++) {
		buf[i*4] = vc1_ptr_bits[i*2];
		buf[i*4+1] = vc1_ptr_bits[i*2+1];
	}
	for (i = 0; i < 16; i++) {
		buf[128+i*4] = vc1_ptr_ol_bits[i*2];
		buf[128+i*4+1] = vc1_ptr_ol_bits[i*2+1];
	}

	vc2LoadSRAM( rex3,(unsigned short *) buf, VC2_CURSOR_GLYPH_ADDR,
                                     sizeof(buf)/sizeof(short) );

	/* Set vc2 cursor position and cursor glyph registers */

	vc2SetReg( rex3, VC2_CURS_ENTRY_PTR, VC2_CURSOR_GLYPH_ADDR );
        BFIFOWAIT( rex3 );
/* Cursor position should be initialized to 0,0 here. */
/* Whoever, initializes the mouse can initialize the x, y position too! */
	vc2SetReg( rex3, VC2_CURS_X_LOC, 0 );
	vc2SetReg( rex3, VC2_CURS_Y_LOC, 0 );


/* XXX Set the cursor colors ! */

        BFIFOWAIT( rex3 );

	cmapSetReg (rex3, CMAP_CRS_CMD_REG, 7);
        cmapSetAddr( rex3, CURSOR_CMAP_MSB << 5 );

	cmapSetRGB( rex3,  0,  0,  0);
        BFIFOWAIT( rex3 );
	cmapSetRGB( rex3, fr, fg, fb);
	cmapSetRGB( rex3, br, bg, bb);
	cmapSetReg (rex3, CMAP_CRS_CMD_REG, 3);
}

static void Ng1Init(struct rex3chip *rex3, struct ng1_info *info)
{

	int idx = Ng1Index(rex3);

	if (is_fullhouse())
        {
		lg3BdVersGet(rex3, lg3_version[idx]); /* get current board version */
		lg3_version[idx] = (lg3_version[idx] >> 5) & 0x07;
		lg3BdVersSet(rex3, LG3_PLL_UNRESET );
		DELAY(10*1000);  /* Wait 10ms after unresetting pll */
		lg3BdVersSet(rex3, LG3_PLL_UNRESET | LG3_VC2_UNRESET | LG3_GFX_UNRESET);
	}

	Ng1InitInfo(rex3, info);

	/*
	 * Initialize registers. Screen will be blanked.
	 */
	Ng1RegisterInit(rex3, info);
	rex3Clear(rex3, info);

	/* init cursor color */
	Ng1CursorInit(rex3, 0xff, 0, 0, 0xff, 0xff, 0xff);
}

int
Ng1ProbeAll(char *base[])
{
	struct rex3chip *prex3;
	struct ng1_info *pinfo;
	int i, bd=0;

	for (i = 0, prex3 = rex3base[i]; prex3 != 0; prex3 = rex3base[++i]) {
		if (Ng1Probe(prex3,i)) {
			pinfo = &ng1_ginfo[i];
			Ng1Init(prex3, pinfo);
			base[bd++] = (char *)prex3;
		}
	}
	return bd;
}

/*
 * Probe routine set by .cf file and used by the textport.  The tp calls
 * with fncs=0 to find base addresses.  Each call with fncs=0 should return
 * the base of the next board.  We return 0 to indicate all the boards
 * have been reported.  If fncs is != 0, we return the functions for the
 * graphics board.
 */
long
ng1_probe(struct tp_fncs **fncs)
{
	static char *base[MAXNG1+1];
	static int i;

	if (fncs) {
		*fncs = &ng1_tp_fncs;
		i = 0;
		return 0;
	}

	if (i == 0) {
		bzero(base,sizeof(base));
		Ng1ProbeAll(base);
	}

	if (i >= MAXNG1+1)
		return 0;

	return (long)base[i++];
}

/*
 * Re-probe for NEWPORT graphics boards and add them to the hardware config
 */
void ng1_install(COMPONENT *root)
{
	struct rex3chip **prex3;
	struct ng1_info *pinfo;
	extern int gfxboard;
	COMPONENT *tmp;
	int idx;

	rex3_config_default = rex3_config_default_init;

	for (idx=0, prex3 = rex3base; *prex3 != 0; idx++, prex3++) {
		if (Ng1Probe(*prex3,idx)) {
			pinfo = &ng1_ginfo[idx];
			if(pinfo->bitplanes == 0) {
		 		/* Initialize the pinfo structure */
				Ng1Init(*prex3, pinfo);
				/* Initialize the bitplanes */
				if((pinfo->boardrev < 2) || (is_fullhouse))
				 	getfbdepth( *prex3, pinfo );
			}

			tmp = AddChild(root,&ng1tmpl,(void*)NULL);
			if (tmp == (COMPONENT*)NULL)
				cpu_acpanic("ng1");
			tmp->Key = gfxboard++;

			/* Indy calls NG1 "Indy" graphics.
			 * Indigo2 calls NG1 "XL" graphics.
			 */
			strcpy(tmp->Identifier,"SGI-");
			if (is_fullhouse()) {
				strcat(tmp->Identifier,"XL");
				tmp->IdentifierLength = 7;
			}
			else if (pinfo->bitplanes == 24) {
				strcat(tmp->Identifier,"Indy 24-bit");
				tmp->IdentifierLength = 16;
			}
			else {
				strcat(tmp->Identifier,"Indy 8-bit");
				tmp->IdentifierLength = 15;
			}

			RegisterDriverStrategy(tmp, _gfx_strat);

			if (ng1_ginfo[idx].monitortype != 7) {
				tmp = AddChild(tmp, &montmpl, (void*)NULL);
				if (tmp == (COMPONENT*)NULL)
					cpu_acpanic("monitor");
			}
		}
	}
}
