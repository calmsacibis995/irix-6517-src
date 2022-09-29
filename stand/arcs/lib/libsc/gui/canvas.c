/* Standalone GUI canvas code (raised box).
 */

#ident "$Revision: 1.4 $"

#include <stand_htport.h>
#include <guicore.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc.h>
#include <libsc_internal.h>

static void
canvasEventHandler(struct gui_obj *obj, int event)
{
	struct Canvas *c = (struct Canvas *)obj;

	switch(event) {
	case _REDRAW:
		drawCanvas(obj);
		break;
	case _FREE:
		free(c);
		break;
	}
	return;
}

struct Canvas *
createCanvas(int w, int h)
{
	struct Canvas *c;

	c = (struct Canvas *)guimalloc(sizeof(struct Canvas));
	c->gui.type = CANVAS;
	c->gui.flags = 0;
	c->gui.eventHandler = canvasEventHandler;

	c->gui.x1 = (gfxWidth()-w) >> 1;
	c->gui.y1 = (gfxHeight()-h) >> 1;
	c->gui.x2 = c->gui.x1 + w - 1;
	c->gui.y2 = c->gui.y1 + h - 1;

	addObject(guiobj(c));

	return(c);
}

void
clearCanvasArea(struct gui_obj *over)
{
	color(VeryLightGray);
	sboxfi(over->x1,over->y1,over->x2,over->y2);
}

void
drawCanvas(struct gui_obj *o)
{
	int x1 = o->x1;
	int x2 = o->x2;
	int y1 = o->y1;
	int y2 = o->y2;
	int ramp[4];
	int i;

#define bd(E,X1,Y1,X2,Y2)				\
	for (i = 0; i < E; i++) {			\
		color(ramp[i]);				\
		sboxfi(X1 i, Y1 i, X2 i, Y2 i);		\
	}

	color(VeryLightGray);
	sboxfi(x1+4,y1+4,x2-4,y2-4);

	/* bottom and right */
	ramp[0] = Black; ramp[1] = DarkGray;
	ramp[2] = MediumGray; ramp[3] = LightGray;
	bd(4,x1+,y1+,x2-,y1+);
	bd(4,x2-,y1+,x2-,y2-);

	/* top and left */
	ramp[0] = Black; ramp[1] = White;
	ramp[2] = White; ramp[3] = White;
	bd(4,x1+,y2-,x2-,y2-);
	bd(4,x1+,y1+,x1+,y2-);
}
