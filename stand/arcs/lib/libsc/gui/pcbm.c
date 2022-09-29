/* prom color bitmap
 */

#ident "$Revision: 1.3 $"

#include <stand_htport.h>
#include <gfxgui.h>
#include <guicore.h>
#include <style.h>
#include <libsc.h>
#include <libsc_internal.h>

#include <pcbm/pcbm.h>

void
drawPcbm(struct pcbm *pcbm, int x, int y)
{
	struct pcbm_node *p;

	x += pcbm->dx;
	y += pcbm->dy;

	for (p=pcbm->bitlist; p->bits; p++) {
		cmov2i(x+p->dx,y+p->dy);
		color(p->color);
		drawBitmap(p->bits,p->w,p->h);
	}
}
