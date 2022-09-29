#ident	"lib/libsk/graphics/LIGHT/lg1_init.c:  $Revision: 1.40 $"

/*
 * lg1_init.c - initialization functions for LIGHT graphics
 */

#include "sys/sbd.h"
#include "sys/param.h"
#include "arcs/hinv.h"
#include "sys/lg1hw.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/lg1.h"
#include "macros.h"
#include "vidtim.h"
#include "sys/sbd.h"
#include <libsc.h>
#include <libsk.h>

#include "cursor.h"
extern unsigned char vc1_ptr_bits[];
extern unsigned char vc1_ptr_ol_bits[];

extern struct tp_fncs lg1_tp_fncs;

struct rexchip *lg1_ghw;
struct lg1_info lg1_ginfo;

struct rexchip *rex[] = {
    (struct rexchip *)PHYS_TO_K1(0x1F3F0000),
    (struct rexchip *)PHYS_TO_K1(0x1F3F8000),
    (struct rexchip *)NULL
};

static COMPONENT lg1tmpl = {
	ControllerClass,		/* Class */
	DisplayController,		/* Type */
	ConsoleOut|Output,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	8,				/* IdentifierLength */
	"SGI-LG1"			/* Identifier */
};

extern COMPONENT montmpl;		/* defined in gfx.c */

#ifdef IP20
/*  On dual-head lg1 with 50Mhz IP20, we seem to need a bit of delay when
 * reading two registers in a row.
 */
void
IP20lg1delay(void){}
#endif

/******************************************************************************
 *
 * Check to see if a private bus LG1 type board is there.  
 *
 * Returns:
 *	0: board not found
 *	1: found board, part of info struct filled in
 *
 *****************************************************************************/
int Lg1Probe(struct rexchip *hw)
{
	unsigned long junk;

	hw->set.xstarti = 0x12345678;
	hw->set.command = 0L;	/* do a nop without writing */
	junk = hw->go.command;
	IP20DELAY;
	if (hw->set.xstart.word == 0x33C0000)
		return 1;
	else
		return 0;
}

static void Lg1InitInfo(struct rexchip *hw, struct lg1_info *info)
{
	unsigned long int tmp;

				/* gfx info */
	strncpy(info->gfx_info.name, GFX_NAME_LIGHT, GFX_INFO_NAME_SIZE);
	info->gfx_info.xpmax = 1024;
	info->gfx_info.ypmax = 768;
	info->gfx_info.length = sizeof(struct lg1_info);


	/* board rev */
	hw->p1.set.configsel = 4;
	tmp = hw->p1.go.wclock;
	DELAY(1);		/* wait for read interlock (rex0 bug) */
	tmp = hw->p1.set.wclock;

	info->boardrev = tmp & 0x07;
	info->monitortype = (tmp >> 3) & 0x07;

	if (tmp & 0x40)
		info->videoinstalled = 0;
	else
		info->videoinstalled = 1;

	/* rex rev */
	hw->set.rwmask = 0xff;
	hw->set.command = REX_DRAW|COLORAUX|QUADMODE|(3<<28);
	hw->set.xstarti = 0;
	hw->set.ystarti = 0;
	hw->set.xendi = 3;
	hw->set.yendi = 0;
	hw->go.rwaux1 = 0x01081020;

	hw->set.command = REX_DRAW|COLORAUX|QUADMODE|COLORCOMP|(3<<28);
	hw->set.aux1 = 0x100;
	hw->go.rwaux1 = 0x09090909;
	
	hw->set.command = REX_LDPIXEL|COLORAUX|QUADMODE|(3<<28);
	tmp = hw->go.rwaux1;
	tmp = hw->go.rwaux1;

	if (tmp == 0x01080909)
		info->rexrev = 0;
	else if (tmp == 0x09091020)
		info->rexrev = 1;
	else
		info->rexrev = -1;
	
	/* vc1 rev */
	hw->p1.set.configsel = 5;
	hw->p1.set.rwvc1 = 0;
	hw->p1.go.rwvc1 = 0;
	hw->p1.set.configsel = 4;
	hw->p1.set.rwvc1 = 5;
	hw->p1.go.rwvc1 = 5;
	hw->p1.set.configsel = 3;
        tmp = hw->p1.go.rwvc1;
	DELAY(1);
	tmp = hw->p1.set.rwvc1;

	info->vc1rev = (tmp & 0x7) - 1;
}

static void Lg1StartClock(struct rexchip *hw)
{
	register int i;
	static unsigned char cd[] = {
		0xc4,0x00,0x10,0x24,0x30,0x40,0x59,0x60,
		0x72,0x80,0x90,0xad,0xb6,0xd1,0xe0,0xf0,0xc4};

	hw->p1.go.configsel = 2;
	for (i = 0; i < sizeof(cd); i++) {
		hw->p1.set.wclock = cd[i];
		hw->p1.go.wclock = cd[i];
	}
}

/*
 * init the lut1/bt479
 */
static void Lg1DacInit(struct rexchip *hw, int sync, int bt)
{
	int i;
	
	if (!bt) {
		hw->p1.set.configsel = LUT_COMMAND;
		if (sync)
			hw->p1.go.rwdac = 3; /* sync on green */
		else
			hw->p1.go.rwdac = 2;
	} else {
		/* address of window bounds registers */
		btSet(hw, WRITE_ADDR, 0);
		
		/* init 8 bytes for each of 16 windows to 0 */
		for (i = 0; i < 8*16; i++) {
			btSet(hw, WRITE_ADDR, i);
			btSet(hw, CONTROL, 0);
		}
		
		btSet(hw, WRITE_ADDR, 0x82);

		/* command register 0 */
		btSet(hw, CONTROL, 0);

		/* command register 1 */
		btSet(hw, CONTROL, 0x2 | (sync << 3));

		/* flood register lo */
		btSet(hw, CONTROL, 0);

		/* flood register hi */
		btSet(hw, CONTROL, 0);
		
		/* pixel read mask */
		btSet(hw, PIXEL_READ_MASK, 0xff);
	}
}

static void Lg1LoadVC1SRAM(
	struct rexchip *hw,
	unsigned char *data,
	unsigned int addr,
	unsigned int length)
{
	int i;

	VC1SetAddr(hw, addr, 2);
	for (i = 0; i < length; i += 2) {
		VC1SetByte(hw, data[i]);
		VC1SetByte(hw, data[i+1]);
	}
}

void Lg1RegisterInit(struct rexchip *hw, struct lg1_info *info)
{
	unsigned char frtableDID[768*2];
        unsigned char ltableDID[8*2] = {
		0x0,0x1,0x0,0x0,0x0,0x5,0x0,0x0,
		0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0
	};
	int i;

	/**********************************************************************
	 * Init REX
	 *********************************************************************/
	/*
	 * must wait for chip to be idle before setting these
	 * configuration registers!!!
	 */
	RexWait(hw);
	
	/* set current origin to be upper left of screen */
	RexSetConfig(hw, xywin, 0x08000800);

	/* set fifo levels to disable interrupts, set vc1 clock for 1024 */
	RexSetConfig(hw, configmode, (0x1f << 13) | (0x1f << 18) | (1 << 31));

	/* set drawing into normal planes */
	RexSetConfig(hw, aux2, PIXELPLANES);

	/*
	 * for rex bug, latch nextcontext into currcontext, so that reading
	 * aux2 works
	 */
	hw->go.command = REX_NOP;

	/* these bits are for the GL, init to 0 for X */
	hw->set.aux1 = 0;

	/**********************************************************************
	 * Init DACs
	 *********************************************************************/

	i = !(info->monitortype == 6);
	if (info->boardrev >= 2)
		Lg1DacInit(hw, i, 0);
	else
		Lg1DacInit(hw, i, 1);

	/**********************************************************************
	 * Init VC1
	 *********************************************************************/

	/* write VC1 SYS_CTL register - disable */
	hw->p1.go.configsel = 6;
	VC1SetByte(hw, 3);

	/* load video timing table */
	Lg1LoadVC1SRAM(hw, vtg_ltable, VC1_VID_LINE_TAB_ADDR,
		       sizeof(vtg_ltable));
	Lg1LoadVC1SRAM(hw, vtg_frtable, VC1_VID_FRAME_TAB_ADDR,
		       sizeof(vtg_frtable));

	/* write VC1 VID_EP, VID_ENCODE(0x1D) register */
	VC1SetAddr(hw, 0x00, 0); /* VID_EP */
	VC1SetWord(hw, (VC1_VID_FRAME_TAB_ADDR)|0x8000); /* hi bit set for VC1A PAL fix */
	VC1SetAddr(hw, 0x14, 0); /* VID_ENABLE */
	VC1SetWord(hw, 0x1d00 );

	/* load DID table */
	for (i = 0; i < sizeof(frtableDID); i += 2) {
		frtableDID[i] = 0x48;
		frtableDID[i+1] = 0 ;
	}
	frtableDID[767*2] = 0x48;
	frtableDID[767*2+1] = 0x4;

	Lg1LoadVC1SRAM(hw, frtableDID, 0x4000, sizeof(frtableDID));
	Lg1LoadVC1SRAM(hw, ltableDID, 0x4800, sizeof(ltableDID));

	/* write VC1 WID_EP,WID_END_EP,WID_HOR_DIV & WID_HOR_MOD */
	VC1SetAddr(hw, 0x40, 0); /* WID_EP */
	VC1SetWord(hw, 0x4000);
	VC1SetWord(hw, 0x4600);
	VC1SetByte(hw, 1024/5);  /* div */
	VC1SetByte(hw, 1024%5);  /* mod */
	VC1SetAddr(hw, 0x60, 0); /* blackout */
	VC1SetByte(hw, 0x1);
	VC1SetByte(hw, 0x1);

	/* write VC1 DID mode registers (32x32) */
	VC1SetAddr(hw, 0, 1); /* mode table */
	for (i = 0; i < 0x40; i += 2) {
		VC1SetWord(hw, 0);
	}

	{
		unsigned char buf[256];

		bzero(buf, sizeof(buf));
		for (i = 0; i < 16; i++) {
			buf[i*4] = vc1_ptr_bits[i*2];
			buf[i*4+1] = vc1_ptr_bits[i*2+1];
		}
		for (i = 0; i < 16; i++) {
			buf[128+i*4] = vc1_ptr_ol_bits[i*2];
			buf[128+i*4+1] = vc1_ptr_ol_bits[i*2+1];
		}
		Lg1LoadVC1SRAM(hw,buf,0x3000,sizeof(buf));

		VC1SetAddr(hw, 0x20, 0);	/* cursor control */
		VC1SetWord(hw, 0x3000);		/* CUR_EP */
		VC1SetWord(hw, 0x0000);		/* CUR_XL */
		VC1SetWord(hw, 0x0000);		/* CUR_XY */
		VC1SetWord(hw, 0x0000);		/* normal cursor mode */
	}
	
	/* write VC1 SYS_CTL register - enable VC1 function bit */
	hw->p1.go.configsel = 6;
	VC1SetByte(hw, 0xb9);
}

static void Lg1SetCursorColor(struct rexchip *hw,
	int fr, int fg, int fb, int br, int bg, int bb)
{
	int lutcmd;
	int lutrev = lg1_ginfo.boardrev >= 2 ? 1 : 0;

	/* palette 3 contains the cursor colors */
	if (lutrev) {
		btGet(hw, CONTROL, lutcmd);
		lutcmd = (lutcmd & 0xf) | (3 << 6);
		btSet(hw, CONTROL, lutcmd);
	} else {
		btSet(hw, WRITE_ADDR, 0x82);
		btSet(hw, CONTROL, 0x3f);
	}

	/* set foreground color */
	btSet(hw, WRITE_ADDR, 1);
	btSet(hw, PALETTE_RAM, fr);
	btSet(hw, PALETTE_RAM, fg);
	btSet(hw, PALETTE_RAM, fb);

	/* set background color */
	btSet(hw, WRITE_ADDR, 2);
	btSet(hw, PALETTE_RAM, br);
	btSet(hw, PALETTE_RAM, bg);
	btSet(hw, PALETTE_RAM, bb);

	/* set command register 0 back just in case... */
	if (lutrev) {
		btGet(hw, CONTROL, lutcmd);
		lutcmd = lutcmd & 0xf;
		btSet(hw, CONTROL, lutcmd);
	} else {
		btSet(hw, WRITE_ADDR, 0x82);
		btSet(hw, CONTROL, 0x0f);
	}

	/* set cursor mode register */
	VC1SetAddr(hw, 0x26, 0);		/* cursor mode register */
	VC1SetWord(hw, 0xc000);			/* map 3, submap 0 */
}

static void Lg1Init(struct rexchip *hw, struct lg1_info *info)
{
	Lg1InitInfo(hw, info);

	/*
	 * Start the clock
	 */
	Lg1StartClock(hw);
	
	/*
	 * Initialize registers. Screen will be blanked.
	 */
	Lg1RegisterInit(hw, info);
	
	/* init cursor color */
	Lg1SetCursorColor(hw, 0xff, 0, 0, 0xff, 0xff, 0xff);
}     

int Lg1ProbeAll(void)
{
	struct rexchip *prex;
	struct lg1_info l_info, *pinfo;
	int i;
	
	for (i = 0, prex = rex[i]; prex != 0; prex = rex[++i]) {
		if (Lg1Probe(prex)) {
			pinfo = &lg1_ginfo;
			lg1_ghw = prex;
			Lg1Init(prex, pinfo);
			return (int)prex;
		}
	}
	return (int)0;
}

/*
 * Probe routine set by .cf file and used by the textport.  The tp calls
 * with fncs=0, and we return the base of the first lg board.  If fncs is
 * != 0, we return the functions for the graphics board at base.
 */
void *
lg1_probe(struct tp_fncs **fncs)
{
	if (fncs) {
		/* vc1rev == 1 */
		*fncs = &lg1_tp_fncs;
		return 0;
	}
	return (void *)Lg1ProbeAll();
}

/*
 * Re-probe for LIGHT graphics boards and add them to the hardware config
 */
void lg1_install(COMPONENT *root)
{
	struct rexchip **prex;
	COMPONENT *tmp;
	int i=0;

	for (prex = rex; *prex != 0; prex++) {
		if (Lg1Probe(*prex)) {
			tmp = AddChild(root,&lg1tmpl,(void*)NULL);
			if (tmp == (COMPONENT*)NULL)
				cpu_acpanic("lg2");
			tmp->Key = i++;
			RegisterDriverStrategy(tmp, _gfx_strat);

			if (lg1_ginfo.monitortype != 7) {
				tmp = AddChild(tmp, &montmpl, (void*)NULL);
				if (tmp == (COMPONENT*)NULL)
					cpu_acpanic("monitor");
			}
		}
	}
}
