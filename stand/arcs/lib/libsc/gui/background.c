/* Draw background for gfxgui.
 */

#ident "$Revision: 1.9 $"

#include <stand_htport.h>
#include <company.h>
#include <guicore.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc_internal.h>

static void
drawCalligraphy(unsigned char *p, int x, int y)
{
	struct htp_state *htp = gfxgui.htp;
	int clogow = htp->clogow;
	int clogoh = htp->clogoh & 0xfffff;
	int yend = y + clogoh;
	int px, ccol;

	for ( ; y < yend ; y++) {
		for (px=ccol=0; px < clogow; px += *p++) {
			if (ccol && *p)
				sboxfi(px+x, y, px+x + *p - 1, y);
			ccol = !ccol;
		}
	}
}

static unsigned char *
nextImage(unsigned char *p)
{
	struct htp_state *htp = gfxgui.htp;
	int px, y, yend = htp->clogoh & 0xfffff;
	int clogow = htp->clogow;

	for (y=0; y < yend ; y++)
		for (px=0; px < clogow; px += *p++)
			;

	return p;
}

static unsigned char sgcs[] = {
	0x3, 0x8, 0x9, 0x8, 0x5, 0xc, 0xb, 0x0,
	0x2, 0xe, 0x4, 0xd, 0x7, 0x8, 0x5, 0xf, 0x0,
	0x1, 0xc, 0xa, 0xd, 0x11, 0x10, 0x6, 0xe, 0x0,
	0x3, 0x12, 0xf, 0x10, 0x6, 0xa, 0xf, 0xff
};

/*
 * Draw "Silicon Graphics Computer Systems"
 */
static void
drawCompanyName(void)
{
	unsigned char *i;

	for (i = sgcs; *i != 0xff; i++) {
		struct htp_bitmap b;
		struct fontinfo info;

		info = companyInfo[*i];
		b.buf = companyData + info.offset;
		b.xsize = info.xsize;
		b.ysize = info.ysize;
		b.xorig = (short) -info.xorig;
		b.yorig = (short) -info.yorig;
		b.xmove = info.xmove;
		b.ymove = 0;
		b.sper = (short)((b.xsize + 15) / 16);
		drawbitmap(&b);
	}
}

/* draw gfx gui background.
 */
void
drawBackground(void)
{
	char *w = "W   E   L   C   O   M   E     T   O";
	struct htp_state *htp = gfxgui.htp;
	int welcomex,welcomey,clogox,companynamex,companynamedy;
	unsigned char *shadow = htp->clogodata;
	int clogoh = htp->clogoh;
	int clogodx = 0;
	int clogody = 0;
	int clogobdy = 0;

	if (htp->clogow > 600) {		/* need to center logo */
		/* Check for seperate foreground and background images
		 * printed with no offset.  The two drawing both use
		 * the same colors.
		 */
		if (clogoh & 0x40000000) {	/* seperate fg/bg */
			clogoh &= 0xfffff;
			shadow = nextImage(shadow);
			clogodx = -CLOGODX;
			clogobdy = -CLOGODY;
		}
		else if (clogoh & 0x20000000) {
		  /*
		   * Use this flag to reduce the spacing between the
		   * shadow and the original image. This is used for
		   * KONA currently.
		   */
		        clogoh &= 0xfffff;
			clogodx = -(CLOGODX - 3);
			clogobdy = -(CLOGODY + 3);
		}

		clogody = (256 - clogoh)/2;
		clogox = (gfxWidth() - htp->clogow)/2 + CLOGODY_C;
		welcomex = WELCOMEX;
		welcomey = CLOGOY + clogody + clogoh + 20;
		companynamex = COMPANYNAMEX_C;
		companynamedy = clogody - 20;
	}
	else if (gfxWidth() <= APPROX1024) {
		welcomex = WELCOMEX1024;
		welcomey = WELCOMEY;
		clogox = CLOGOX1024;
		companynamex = COMPANYNAMEX;
		companynamedy = 0;
	}
	else {
		welcomex = WELCOMEX;
		welcomey = WELCOMEY;
		clogox = CLOGOX;
		companynamex = COMPANYNAMEX;
		companynamedy = 0;
	}

	drawShadedBackground(gfxgui.htp,0,0,gfxWidth()-1,gfxHeight()-1);

	if (gfxgui.flags & (GUI_NOLOGO|GUI_NEVERLOGO))
		return;

	color(CLOGOSHADOWCOLOR);
	drawCalligraphy(shadow, clogox+CLOGODX+clogodx,
			CLOGOY+CLOGODY+clogody+clogobdy);

	cmov2i(welcomex+companynamex, COMPANYNAMEY+companynamedy);
	color(COMPANYNAMECOLOR);
	drawCompanyName();

	color(CLOGOCOLOR);
	drawCalligraphy(htp->clogodata, clogox, CLOGOY + clogody);

	cmov2i(welcomex+WELCOMEDX, welcomey+WELCOMEDY);
	color(CLOGOSHADOWCOLOR);
	puttext(w, WELCOMEFONT);

	cmov2i(welcomex, welcomey);
	color(CLOGOCOLOR);
	puttext(w, WELCOMEFONT);
}
