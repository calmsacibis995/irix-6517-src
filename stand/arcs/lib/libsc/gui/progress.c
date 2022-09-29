/*
 * Progress Field code
 *
 * Draws a sliding bar progress box.
 */

#ident "$Revision: 1.4 $"

#include <stand_htport.h>
#include <gfxgui.h>
#include <guicore.h>
#include <style.h>
#include <libsc.h>
#include <libsc_internal.h>

#define getProgressBoxHeight(a)	20

static void drawProgressBox(struct ProgressBox *);
static void drawProgressBar(struct ProgressBox *);

static void
ProgressBoxEventHandler(struct gui_obj *obj, int event)
{
	struct ProgressBox *pg = (struct ProgressBox *)obj;
	switch(event) {
	case _REDRAW:
		drawProgressBox(pg);
		break;
	case _FREE:
		free(pg);
		break;
	}
	return;
}

struct ProgressBox *
createProgressBox(int x, int y, int w)
{
	struct ProgressBox *pg;

	pg = (struct ProgressBox *)guimalloc(sizeof(struct ProgressBox));
	pg->gui.type = PROGRESSBOX;
	pg->gui.state = 0;
	pg->gui.x1 = x;
	pg->gui.y1 = y;
	pg->gui.x2 = x+w-1;
	pg->gui.y2 = y+getProgressBoxHeight(pg)-1;
	pg->gui.eventHandler = ProgressBoxEventHandler;

	pg->percent = 0;

	addObject(guiobj(pg));
	
	return(pg);
}

void
changeProgressBox(struct ProgressBox *prog, int per, int tenth)
{
	prog->percent = per;
	prog->tenth = tenth;
	drawProgressBar(prog);
}

static char tf_dtol[] = { LightGray, DarkGray, DarkGray, Black };
static char tf_ltod[] = { White, White, TerraCotta, Black };

static void
drawProgressBox(struct ProgressBox *pg)
{
	int i,x1,x2,y1,y2;

	x1 = pg->gui.x1;
	x2 = pg->gui.x2;
	y1 = pg->gui.y1;
	y2 = pg->gui.y2;

#define tfbd(ramp,X1,Y1,X2,Y2)				\
	for (i=0; i < TFIELDBDW; i++) {			\
		color(ramp[i]);				\
		sboxfi(X1 i, Y1 i, X2 i, Y2 i);		\
	}

	tfbd(tf_ltod,x1+,y1+,x2-,y1+);		/* bottom */
	tfbd(tf_dtol,x1+,y1+,x1+,y2-);		/* left */
	tfbd(tf_dtol,x1+,y2-,x2-,y2-);		/* top */
	tfbd(tf_ltod,x2-,y1+,x2-,y2-);		/* right */

	drawProgressBar(pg);
}

static void
drawProgressBar(struct ProgressBox *pg)
{
	int x1,x2,y1,y2;
	int width;
	int pixpp;
	int rempp;
	int pixpt;
	int split;

	x1 = pg->gui.x1+TFIELDBDW;
	x2 = pg->gui.x2-TFIELDBDW;
	y1 = pg->gui.y1+TFIELDBDW;
	y2 = pg->gui.y2-TFIELDBDW;

	width = x2-x1;

	if (pg->percent < 100) {
		pixpp = width / 100;
		rempp = width % 100;
		pixpt = (pixpp*pg->tenth)/10;

		split = pixpp * pg->percent;
		split += (split/(width/rempp)) + pixpt;
	}
	else
		split = width;

	if (split) {
		color(VeryLightGray);
		sboxfi(x1,y1, x1+split,y2);
	}
	if (!split || (split < x2)) {
		color(TerraCotta);
		sboxfi(x1+split,y1, x2,y2);
	}
}
