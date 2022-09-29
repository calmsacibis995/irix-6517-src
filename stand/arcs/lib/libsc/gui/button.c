/* Button code
 */

#ident "$Revision: 1.13 $"

#include <stand_htport.h>
#include <gfxgui.h>
#include <guicore.h>
#include <style.h>
#include <libsc.h>
#include <libsc_internal.h>
#include <pcbm/pcbm.h>

static void drawButton(struct Button *b);
void drawBitmap(unsigned short *, int, int);

/* data for "enter" arrow on default buttons */
#define default_width 14
#define default_height 12
static unsigned short default_bits[] = {
   0x0400, 0x0c00, 0x1c00, 0x3c00, 0x7c00, 0xfffc, 0x7c04, 0x3c04,
   0x1c04, 0x0c04, 0x0404, 0x0004};

#define defaulthl_width 6
#define defaulthl_height 6
static unsigned short defaulthl_bits[] = {
   0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000};

static void
buttonEventHandler(struct gui_obj *obj, int event)
{
	struct Button *b = (struct Button *)obj;
	switch(event) {
	case _REDRAW:
		drawButton(b);
		break;
	case _BUTTON_UP:
		if (b->cb) (*b->cb)(b,b->value);	/* call callback */
		break;
	case _FREE:
		free(b);
		break;
	}
	return;
}

struct Button *
createButton(int x, int y, int w, int h)
{
	struct Button *b;

	b = (struct Button *)guimalloc(sizeof(struct Button));
	b->gui.type = GBUTTON;
	b->gui.flags = (_WANT_BUTTON_UP|_WANT_MOTION_RD);
	b->gui.state = 0;
	b->gui.x1 = x;
	b->gui.y1 = y;
	b->gui.x2 = x+w-1;
	b->gui.y2 = y+h-1;
	b->gui.eventHandler = buttonEventHandler;

	b->fg = BLACK;
	b->cb = 0;
	b->text = 0;
	b->pcbm = 0;

	addObject(guiobj(b));
	
	return(b);
}

void
addButtonText(struct Button *b, const char *c)
{
	int tw = textWidth(c,BUTTONFONT) + 2*BUTTONMINMARGIN;
	int width = b->gui.x2 - b->gui.x1;

	b->text = c;

	if (tw > width) {
		/* move button x1 over so text fits */
		b->gui.x1 -= (tw-width);
	}
}

void
addButtonImage(struct Button *b, struct pcbm *map)
{
	b->pcbm = map;
}

void
changeButtonFg(struct Button *b, int color)
{
	b->fg = color;
}

/* set little "enter" arrow on button */
void
setDefaultButton(struct Button *b, int keepright)
{
	int tw = b->text ? textWidth(b->text,BUTTONFONT) : 0;
	int width = b->gui.x2 - b->gui.x1;

	b->gui.flags |= _DEFAULT;

	tw += BUTTONAMARGIN + default_width + 2*BUTTONMINMARGIN;

	if (tw > width) {
		int move = tw - width;
		if (keepright)
			b->gui.x1 -= move;
		else
			b->gui.x2 += move;
	}
}

void
invalidateButton(struct Button *b, int on)
{
	if (on) {
		b->gui.flags |= _INVALID;
		b->gui.flags &= ~(_WANT_MOTION_RD|_WANT_BUTTON_UP);
	}
	else {
		b->gui.flags &= ~_INVALID;
		b->gui.flags |= (_WANT_MOTION_RD|_WANT_BUTTON_UP);
	}
}

void
stickButton(struct Button *b, int on)
{
	if (on)
		b->gui.flags |= _FORCE_PRESSED;
	else
		b->gui.flags &= ~_FORCE_PRESSED;
}

void
addButtonCallBack(struct Button *b,
		  void (*func)(struct Button *,__scunsigned_t),
		  __scunsigned_t value)
{
	b->cb = func;
	b->value = value;
}

#define NBRAMP	4			/* number of colors in button ramp */
static char bltod[] = {DarkGray, White, VeryLightGray, VeryLightGray};
static char bltod_h[] = {DarkGray, White, White, White};
static char bltod_p[] = {VeryDarkGray, DarkGray, LightGray, White};

static char bdtol[] = {VeryDarkGray, DarkGray, MediumGray, MediumGray};
static char bdtol_h[] = {VeryDarkGray, DarkGray, LightGray, LightGray};
static char bdtol_p[] = {White, White, Black, LightGray};

static char inv_ltod[] = {MediumGray,LightGray,LightGray,LightGray};

static char *ba_ltod[] = {
	bltod,			/* normal	(neurtral) */
	bltod_h,		/* highlight	(located)  */
	bltod,			/* pressed	(selected) */
	bltod_p			/* pressed highlight (loc+sel) */
};

static char *ba_dtol[] = {	/* same states as above */
	bdtol,
	bdtol_h,
	bdtol,
	bdtol_p
};

static void
drawButton(struct Button *b)
{
	int x1, y1, x2, y2;
 	register int i;
	char *ltod, *dtol;
	int bx, by;
	int fg;

	x1 = b->gui.x1; y1 = b->gui.y1;
	x2 = b->gui.x2; y2 = b->gui.y2;

	if ((b->gui.flags & _INVALID) == 0) {
		int index = b->gui.state&0x03;
		if ((b->gui.flags & _FORCE_PRESSED)) {
			ltod = bltod_p;
			dtol = bdtol_p;
		}
		else {
			ltod = ba_ltod[index];
			dtol = ba_dtol[index];
		}
		color((b->gui.state&ST_HIGHL) ? VeryLightGray : LightGray);
		fg = b->fg;
	}
	else {
		ltod = inv_ltod;
		dtol = inv_ltod;
		color(LightGray);
		fg = DarkGray;
	}

	sboxfi(x1,y1,x2,y2);

#define bbd(ramp,X1,Y1,X2,Y2)				\
	for (i=0; i < NBRAMP; i++) {			\
		color(ramp[i]);				\
		sboxfi(X1 i, Y1 i, X2 i, Y2 i);		\
	}

	bbd(dtol,x1+,y1+,x2-,y1+);		/* bottom */
	bbd(dtol,x2-,y1+,x2-,y2-);		/* right */
	bbd(ltod,x1+,y1+,x1+,y2-);		/* left */
	bbd(ltod,x1+,y2-,x2-,y2-);		/* top */

	if (b->pcbm) {
		bx = x1 + (x2 - x1 + 1 - b->pcbm->w) / 2;
		by = y1 + (y2 - y1 + 1 - b->pcbm->h) / 2;
		drawPcbm(b->pcbm,bx,by);
	}
	if (b->text) {
		bx = x1 + (x2 - x1 + 1 - (i=textWidth(b->text,BUTTONFONT)))/2;
		by = y1 + 9;

		if (b->gui.flags & _DEFAULT) {
			int arrowx;

			bx -= default_width - BUTTONAMARGIN;
			arrowx = bx+i+BUTTONAMARGIN;
			color(fg);
			cmov2i(arrowx,by);
			drawBitmap(default_bits,default_width,default_height);
			color(YELLOW);
			cmov2i(arrowx,by-1);
			drawBitmap(defaulthl_bits,defaulthl_width,
				   defaulthl_height);
			color(DarkGray);
			sboxfi(arrowx+6,by+4,arrowx+13,by+4);
		}

		cmov2i(bx, by);
		color(fg);
		puttext(b->text, BUTTONFONT);
	}
}

void
drawBitmap(unsigned short *bits, int w, int h)
{
	struct htp_bitmap bm;

	bm.buf = bits;
	bm.xsize = (short) w;
	bm.ysize = (short) h;
	bm.xorig = bm.yorig = bm.xmove = bm.ymove = 0;
	bm.sper = (short) ((w + 15) / 16);

	drawbitmap(&bm);
}
