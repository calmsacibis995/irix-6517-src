/*
 * $Revision: 1.17 $
 */

#if defined(_STANDALONE)
#define DO_PON_PUTS 0
#include "libsc.h"
#include "libsk.h"
#ifdef NOTDEF
# include "cursor.h"
#endif
#include "stand_htport.h"

#if DO_PON_PUTS
extern void pon_puts(char *);
extern void pon_puthex(int);
#else /* !DO_PON_PUTS */
#define pon_puts(msg)
#define pon_puthex(num)
#endif /* DO_PON_PUTS */

#else /* !_STANDALONE */
#include "sys/htport.h"
#define pon_puts(msg)
#define pon_puthex(num)
#endif /* _STANDALONE */

#include "sys/types.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "sys/debug.h"
#if defined(_STANDALONE)
#include "cursor.h"
#endif

#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgras_internals.h"

static int cpos_x;
static int cpos_y;

#if defined (FLAT_PANEL)
#include "sys/fpanel.h"
static char lores_flat_panel;		/* true if display is Corona flat panel */
#endif /* FLAT_PANEL */

static void
MgrasTpInit(void *base_in)
{
	mgras_hw *base = (mgras_hw *)base_in;
	int idx;
	struct mgras_info *info;

	pon_puts("Entering MgrasTpInit\n\r");

	idx = mgras_baseindex(base);
	ASSERT((idx >= 0) && (idx < MGRAS_MAXBOARDS));
	info = MgrasBoards[idx].info;
	ASSERT(info);

	/* reset and re-initialize board */
	if (!MgrasBoards[idx].boardInitted)
		mgrasInitBoard(base, idx);
#ifdef NOTDEF
	MgrasReset(base);
	MgrasInitHQ(base, 0);
	MgrasInitRasterBackend(base, info);
#endif
	/* Set screenmasks  to max dimensions */
	mgras_re4Set(base, scrmsk1x, (info->gfx_info.xpmax) - 1);
	mgras_re4Set(base, scrmsk1y, (info->gfx_info.ypmax) - 1);

	/* Set pp1winmode for CID disabled and origin at (0,0) */
	mgras_re4Set(base,pp1winmode.val, 0);

	/* Set up 8 bit CI */
	mgras_re4Set(base, pp1fillmode.val,     TP_PP1_INITCI);
	mgras_re4Set(base, PixCmd, 		PP_PIXCMD_DRAW);

	/* Initialize DID 0 to 8-bit CI */
	mgras_BFIFOPOLL(base, 4);
	mgras_xmapSetMainMode(base,0, MGRAS_XMAP_8BIT_CI);

	/* Enable Display */
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBCONTROL0_ADDR);
	mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBCONTROL0_DEFAULT |
			     MGRAS_XMAP_DISPLAY_ENABLE_BIT);

#if defined (FLAT_PANEL)
	/*
	 * Check for low-res flat panel display.
	 */
	if (idx >= 0)
	    if (MgrasBoards[idx].info &&
		 MgrasBoards[idx].info->MonitorType == MONITORID_CORONA )
			lores_flat_panel = 1;
	    else
		    lores_flat_panel = 0;
#endif /* FLAT_PANEL */

	pon_puts("Leaving MgrasTpInit\n\r");
}

static void 
MgrasTpMapcolor(void *base_in, int index, int r, int g, int b)
{
	mgras_hw *base = (mgras_hw *)base_in;

	pon_puts("Entering MgrasTpMapColor\n\r"); /**/

	/* Index to 8-bit color map */
        mgras_cmapFIFOWAIT (base);                    /* does a BFIFOWAIT() */
	mgras_cmapSetAddr(base, index);
	mgras_cmapSetAddr(base, index);

	/* Force CBlank High (to prevent update of CMAP fifo - bug) */
	mgras_xmapSetPP1Select(base,MGRAS_XMAP_WRITEALLPP1);
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
	mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT |
			     MGRAS_XMAP_DISABLE_CBLANK_BIT);

        mgras_cmapSetAddr(base, index);   /* setup write to cmap palette */
        mgras_cmapSetRGB( base, r, g, b );

	/* Let CBlank go low, so CMAP can drain fifo */
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
	mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT);

	pon_puts("Leaving MgrasTpMapColor\n\r"); /**/
}

static void 
MgrasTpColor(void *base_in, int index)
{
	mgras_hw *base = (mgras_hw *)base_in;

	pon_puts("Entering MgrasTpColor\n\r"); /**/
	
	mgras_CFIFOWAIT(base, 1);

	/* Shift index into whole part 13:12 of floating pnt. register*/
 	mgras_re4Set(base,red, index << 12); 

	pon_puts("Leaving MgrasTpColor\n\r"); /**/
}

static void 
MgrasTpSboxfi(void *base_in, int x1, int y1, int x2, int y2)
{
	mgras_hw *base = (mgras_hw *)base_in;

	pon_puts("Entering MgrasTpSBoxfi\n\r"); /**/
#if defined (FLAT_PANEL)
	if (lores_flat_panel) {	/* adjust for low res display */
		y1 += 256;
		y2 += 256;
	}
#endif /* FLAT_PANEL */
	
	mgras_CFIFOWAIT(base, 4);
	
	mgras_re4Set(base, block_xystarti, y1 | (x1 << 16) );
	mgras_re4Set(base, block_xyendi, y2 | (x2 << 16) );
	/*Set CI clamp */
 	mgras_re4Set(base, fillmode.val, TP_RE4_CIBLOCK);
	mgras_re4SetExec(base, ir.val, TP_RE4_BLOCK_IR_SETUP);

	pon_puts("Leaving MgrasTpSBoxfi\n\r"); /**/
}
static void 
MgrasTpPnt2i(void *base_in, int x, int y)
{
	mgras_hw *base = (mgras_hw *)base_in;

	pon_puts("Entering MgrasTpPnt2i\n\r"); /**/
#if defined (FLAT_PANEL)
	if (lores_flat_panel) 	/* adjust for low res display */
		y += 256;
#endif /* FLAT_PANEL */
	
	mgras_CFIFOWAIT(base, 4);

	/* GLINE is in 13:8 format */
	mgras_re4Set(base, gline_xstartf, x << 8);
	mgras_re4Set(base, gline_ystartf, y << 8);
 	mgras_re4Set(base, fillmode.val, TP_RE4_CIMODE_INIT);
	mgras_re4SetExec(base, ir.val,TP_RE4_POINT_IR_SETUP);

	pon_puts("Leaving MgrasTpPnt2i\n\r"); /**/
}

static void 
MgrasTpDrawbitmap(void *base_in, struct htp_bitmap *bitmap)
{
	int xleft, xright,ybot,ytop;
	int xsize, ysize, shortsPerRow, shortsRemaining;
	unsigned short *bufptr;
	register __uint32_t data32;
	mgras_hw *base = (mgras_hw *)base_in;

	pon_puts("Entering MgrasTpDrawbitmap\n\r"); /**/

	xleft = cpos_x - bitmap->xorig; 
	xsize = bitmap->xsize;
	xright = xleft + xsize - 1; 
	shortsPerRow = bitmap->sper;

	ybot = cpos_y - bitmap->yorig;
	ysize = bitmap->ysize;
	ytop = ybot + ysize -1; 

	bufptr = bitmap->buf;
	
	mgras_CFIFOWAIT(base, 4);

 	mgras_re4Set(base, fillmode.val, TP_RE4_CICHAR);
	mgras_re4Set(base, block_xystarti, (xleft << 16) | ybot  );
	mgras_re4Set(base, block_xyendi,  (xright << 16) | ytop );
	mgras_re4SetExec(base, ir.val,TP_RE4_BLOCK_IR_SETUP);

	while (ysize--) {
		shortsRemaining = shortsPerRow;

		/* Write 64-bit chunks */
		while (shortsRemaining >= 4) {
			mgras_CFIFOWAIT(base, 2);
			data32 = (*(bufptr) << 16) | *(bufptr+1);
			mgras_re4Set(base,char_h, data32);
			data32 = (*(bufptr+2) << 16) | *(bufptr+3);
			mgras_re4SetExec(base,char_l, data32);
			shortsRemaining -= 4;
			bufptr += 4;
		}

		mgras_CFIFOWAIT(base, 2);
		/* Take care of remaining chunk (<4 shorts) */
		switch(shortsRemaining) {
		case 3:
			data32 = (*(bufptr) << 16) | *(bufptr+1);
			mgras_re4Set(base,char_h, data32);
			data32 = (*(bufptr+2) << 16);
			mgras_re4SetExec(base,char_l, data32);
			shortsRemaining -= 3;
			bufptr += 3;
			break;
		case 2:
			data32 = (*(bufptr) << 16) | *(bufptr+1);
			mgras_re4SetExec(base,char_h, data32);
			shortsRemaining -= 2;
			bufptr += 2;
			break;
		case 1:
			data32 = (*(bufptr) << 16);
			mgras_re4SetExec(base,char_h, data32);
			shortsRemaining -= 1;
			bufptr += 1;
			break;
		case 0:
			break;
		}
	} 

	cpos_x += bitmap->xmove;
	cpos_y += bitmap->ymove;

	pon_puts("Leaving MgrasTpDrawbitmap\n\r"); /**/
}


/*ARGSUSED3*/
static void
MgrasTpCmov2i (void *base_in, int x, int y)
{

	pon_puts("Entering MgrasTpCmov2i\n\r"); /**/
#if defined (FLAT_PANEL)
	if (lores_flat_panel) 	/* adjust for low res display */
		y += 256;
#endif /* FLAT_PANEL */
	
	cpos_x = x;
	cpos_y = y;

	pon_puts("Leaving MgrasTpCmov2i\n\r"); /**/
}

static void
MgrasTpBlankscreen(void *base_in, int blank)
{
	mgras_hw *base = (mgras_hw *)base_in;

	pon_puts("Entering MgrasTpBlankscreen\n\r");

	/* Enable/Disable Display */
	mgras_BFIFOPOLL(base, 2);
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBCONTROL0_ADDR);
	if (blank) {
	  mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBCONTROL0_DEFAULT);
	} else {
	  mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBCONTROL0_DEFAULT |
			       MGRAS_XMAP_DISPLAY_ENABLE_BIT);
	}

	pon_puts("Leaving MgrasTpBlankscreen\n\r"); /**/
}


#ifdef _STANDALONE

/*ARGSUSED3*/
static void
MgrasTpMovec(void *base_in, int x, int y)
{
	mgras_hw *base = (mgras_hw *)base_in;

	pon_puts("Entering MgrasTpMovec\n\r"); /**/

	mgras_BFIFOPOLL(base, 2);
	mgras_vc3SetReg(base, VC3_CURS_X_LOC, x + VC3_CURS_XOFF);
	mgras_vc3SetReg(base, VC3_CURS_Y_LOC, y + VC3_CURS_YOFF);

	pon_puts("Leaving MgrasTpMovec\n\r"); /**/
}

static void
MgrasPROMTpInit(void *base_in, int *x, int *y)
{
	mgras_hw *base = (mgras_hw *)base_in;
	int idx = mgras_baseindex(base);

	pon_puts("Entering MgrasPROMTpInit\n\r"); /**/

	MgrasTpInit(base_in);

        *x = MgrasBoards[idx].info->gfx_info.xpmax;
        *y = MgrasBoards[idx].info->gfx_info.ypmax;

	MgrasInitCursor(base_in);
#if 0
	MgrasTpColor(base_in, 0);
	MgrasTpMapcolor(base_in, 0, 0, 0, 0 );
	MgrasTpSboxfi(base_in, 0, 0, 1279, 1023 ); 
	MgrasTpBlankscreen(base_in, 0);
#endif
	pon_puts("Leaving MgrasPROMTpInit\n\r"); /**/
}

struct htp_fncs mgras_htp_fncs = {
	MgrasTpBlankscreen,
	MgrasTpColor,
        MgrasTpMapcolor,
        MgrasTpSboxfi,
        MgrasTpPnt2i,
        MgrasTpCmov2i,
        MgrasTpDrawbitmap,
        0x0,
	MgrasPROMTpInit,
        MgrasTpMovec
};

#else  /* if !defined(_STANDALONE) */

static void
MgrasTpUnblank(void *base_in)
{
	MgrasTpBlankscreen(base_in, 0);
}

/*ARGSUSED2*/
static int
MgrasTpGetdesc(void *base_in, int code)
{

	/* Any other bit maps ? */
        switch (code ) {
                case HTP_GD_BIGMAPS: /* handles large bit maps */
                        return 1;
        }

        return 0;
}

struct htp_fncs mgras_htp_fncs = {
	MgrasTpInit,
        MgrasTpMapcolor,
        MgrasTpColor,
        MgrasTpSboxfi,
        MgrasTpPnt2i,
        MgrasTpDrawbitmap,
        MgrasTpCmov2i,
        MgrasTpUnblank,
        MgrasTpGetdesc,
        0
};
#endif /*!_STANDALONE */
