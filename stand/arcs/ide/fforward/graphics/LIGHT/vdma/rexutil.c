/*
 *  REX utility routines
 */

#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/lg1hw.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/lg1.h"
#include "uif.h"
#include "light.h"


extern Rexchip *rex[];

extern struct lg1_info lg1_ginfo;

#define btSet(hw, addr, val) \
	hw->p1.set.configsel = addr; \
	hw->p1.set.rwdac = val; \
	hw->p1.go.rwdac = val;

void
rex_mapcolor(void *hw, int col, int r, int g, int b)
{
	register struct rexchip *base = (struct rexchip*)hw;
        int sync = lg1_ginfo.monitortype == 6 ? 0 : 1;

	if (lg1_ginfo.boardrev < 2) {			/* LG1 */
		btSet(base, WRITE_ADDR, 0x82);
		btSet(base, CONTROL, 0x0f);
	} else {					/* LG2 */
		btSet(base, CONTROL, sync | 0x2);
	}
	btSet(base, WRITE_ADDR, col);
	base->p1.set.configsel = PALETTE_RAM;
	
	base->p1.set.rwdac = r;
	base->p1.go.rwdac = r;
	base->p1.set.rwdac = g;
	base->p1.go.rwdac = g;
	base->p1.set.rwdac = b;
	base->p1.go.rwdac = b;
	
	if (lg1_ginfo.boardrev < 2) {
		/* pixel read mask */
		btSet(base, PIXEL_READ_MASK, 0xff);
	}
}


void
rex_setclt ()	/* Initialize colors 0-7 */
{
	rex_mapcolor(REX,0,0, 0, 0);            /* black */
	rex_mapcolor(REX,1,255, 0, 0);          /* red */
	rex_mapcolor(REX,2,0, 255, 0);          /* green */
	rex_mapcolor(REX,3,255, 255, 0);        /* yellow */
	rex_mapcolor(REX,4,0, 0, 255);          /* blue */
	rex_mapcolor(REX,5,255, 0, 255);        /* magenta */
	rex_mapcolor(REX,6,0, 255, 255);        /* cyan */
	rex_mapcolor(REX,7,255, 255, 255);      /* white */

}

int 
rex_get4pixel (int x, int y)
{
	
	while ((REX->p1.set.configmode) & CHIPBUSY)
		;
	REX->set.xstarti = x;
	REX->set.ystarti = y;
        REX->go.command = REX_NOP;
        REX->go.command = REX_LDPIXEL|QUADMODE; 

	return REX->set.rwaux1;
	
}

int
rex_dualhead(void)
{
	int b0, b1;

	b0 = Lg1Probe(rex[0]);
	b1 = (rex[1] ? Lg1Probe(rex[1]) : 0);

	msg_printf (DBG, "Probe 0 %d Probe 1 %d\n", b0, b1);

	return (b0 && b1);

}
