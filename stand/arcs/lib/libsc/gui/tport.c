/* routines used by textport, and high level gui code
 */

#ident "$Revision: 1.5 $"

#include <stand_htport.h>
#include <style.h>
#include <libsc_internal.h>

void
drawShadedBackground(struct htp_state *htp, int x0, int y0, int x1, int y1)
{
	int i, c, inc;

	inc = (RAMPSIZE << 16) / (htp->yscreensize - 1);

	c = (RAMPSTART << 16) + y0 * inc;

	for (i = y0; i <= y1; i++, c += inc) {
		(htp->fncs->color)(htp->hw,c>>16);
		(htp->fncs->sboxfi)(htp->hw,x0, i, x1, i);
	}
}

/*
 * Set up the bitmap structure and draw the bitmap. 
 */
int
txOutcharmap(struct htp_state *htp, unsigned char c, int f)
{
	struct fontinfo *info;
	struct htp_bitmap b;
	int index;

	/* Characters <= 6 use the special font */
	if (c > 6) {
		index = chartoindex(c);
		if (index < 0) return(0);
	}
	else {
		f = Special;
		index = c;
	}
	info = &htp->fonts[f].info[index];
	b.buf = htp->fonts[f].data + info->offset;

 	b.xsize = (short)info->xsize;
	b.ysize = (short)info->ysize;
	b.xorig = (short) -info->xorig;
	b.yorig = (short) -info->yorig;
	b.xmove = info->xmove;
	b.ymove = 0;
	b.sper = (short) ((b.xsize + 15) / 16);

	(*htp->fncs->drawbitmap)(htp->hw, &b);

	return(info->xmove);
}
