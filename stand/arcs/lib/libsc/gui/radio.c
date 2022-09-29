/* Standalone GUI radio button code.
 */

#ident "$Revision: 1.11 $"

#include <stand_htport.h>
#include <guicore.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc.h>
#include <libsc_internal.h>

#define HPERITEM	(RADIOBUTW+RADIOLISTYGAP)

static void drawRadioList(struct radioList *r);
static void drawRadioButton(struct radioButton *b);

static void
radioListEventHandler(struct gui_obj *obj, int event)
{
	struct radioList *r = (struct radioList *)obj;

	switch(event) {
	case _REDRAW:
		drawRadioList(r);
		break;
	case _FREE:
		resetRadioList(r);
		free(r->list);
		free(r);
		break;
	}
}

static void
radioButtonEventHandler(struct gui_obj *obj, int event)
{
	struct radioButton *b = (struct radioButton *)obj;
	struct _radioButton *selected;

	switch(event) {
	case _REDRAW:
		drawRadioButton(b);
		break;
	case _BUTTON_UP:
		selected = b->parent->selected;

		if (selected != (struct _radioButton *)b) {
			b->parent->selected = (struct _radioButton *)b;
			if (selected)
				drawRadioButton(&selected->button);
			drawRadioButton(b);
		}
		break;
	/* case _FREE: -- all radio buttons currently allocated staticly */
	}
}

struct radioList *
createRadioList(int x, int y, int w, int h)
{
	struct radioList *r;

	r = (struct radioList *)guimalloc(sizeof(struct radioList));
	r->gui.type = RADIOLIST;
	r->gui.flags = 0;
	r->gui.eventHandler = radioListEventHandler;

	r->gui.x1 = x;
	r->gui.y1 = y;
	r->gui.x2 = x + w - 1;
	r->gui.y2 = y + h - 1;

	r->selected = 0;
	r->curitems = 0;
	r->maxitems = h / HPERITEM;
	r->list = (struct _radioButton *)guimalloc(sizeof(struct _radioButton)
						   * (size_t)r->maxitems);

	addObject(guiobj(r));

	return(r);
}

struct _radioButton *
appendRadioList(struct radioList *r, char *item, int selected)
{
	struct _radioButton *l;

	if (r->curitems == r->maxitems)
		return(0);

	l = &r->list[r->curitems];
	l->text = (char *)guimalloc(strlen(item)+1);
	strcpy(l->text,item);

	/* first item is autoselected
	 */
	if ((r->curitems++ == 0) || selected)
		r->selected = l;

	l->button.gui.type = GBUTTON;
	l->button.gui.x1 = r->gui.x1;
	l->button.gui.x2 = r->gui.x1 + RADIOBUTW;
	l->button.gui.y1 = r->gui.y2-(r->curitems*HPERITEM);
	l->button.gui.y2 = l->button.gui.y1 + RADIOBUTW;
	l->button.gui.flags = (_WANT_BUTTON_UP|_WANT_MOTION_RD);
	l->button.gui.state = 0;
	l->button.gui.eventHandler = radioButtonEventHandler;
	l->button.parent = r;

	l->userdata = 0;

	addObject((struct gui_obj *)l);

	return(l);
}

void
resetRadioList(struct radioList *r)
{
	register int i;

	for (i=0; i < r->curitems; i++) {
		free(r->list[i].text);
		deleteObject((struct gui_obj *)&r->list[i]);
	}
	r->curitems = 0;
	r->selected = 0;
	drawRadioList(r);
}

struct _radioButton *
setRadioListSelection(struct radioList *l, int sel)
{
	struct _radioButton *old;

	if (sel >= l->curitems)
		return(0);

	old = l->selected;
	l->selected = &l->list[sel];

	return(old);
}

struct _radioButton *
getRadioListSelection(struct radioList *l)
{
	return(l->selected);
}

static void
drawRadioList(struct radioList *r)
{
	struct _radioButton *p;
	int np,i;

	/*  If radio list is parented, it is probably parented by a canvas.
	 */
	if (r->gui.parent)
		clearCanvasArea(&r->gui);

	for (p = &r->list[i=0]; i < r->curitems; i++,p++) {
		drawRadioButton(&p->button);
		color(BLACK);
		cmov2i(p->button.gui.x2+RADIOLISTXGAP,
		       p->button.gui.y1+RADIOLISTDESC);
		np = r->gui.x2 - (p->button.gui.x2+RADIOLISTXGAP);
		putcltext(p->text,helvR10,np);
	}
}

#define base_width 24
#define base_height 24
static unsigned short base_bits[] = {
   0x003c, 0x0000, 0x007e, 0x0000, 0x00ff, 0x0000, 0x01ff, 0x8000,
   0x03ff, 0xc000, 0x07ff, 0xe000, 0x0fff, 0xf000, 0x1fff, 0xf800,
   0x3fff, 0xfc00, 0x7fff, 0xfe00, 0xffff, 0xff00, 0xffff, 0xff00,
   0xffff, 0xff00, 0xffff, 0xff00, 0x7fff, 0xfe00, 0x3fff, 0xfc00,
   0x1fff, 0xf800, 0x0fff, 0xf000, 0x07ff, 0xe000, 0x03ff, 0xc000,
   0x01ff, 0x8000, 0x00ff, 0x0000, 0x007e, 0x0000, 0x003c, 0x0000};

#define lt_1_width 12
#define lt_1_height 12
static unsigned short lt_1_bits[] = {
   0x8000, 0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200,
   0x0100, 0x0080, 0x0040, 0x0030};
#define lt_2_width 12
#define lt_2_height 12
static unsigned short lt_2_bits[] = {
   0x0000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100,
   0x0080, 0x0040, 0x0020, 0x0000};
#define lt_3_width 12
#define lt_3_height 12
static unsigned short lt_3_bits[] = {
   0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100, 0x0080,
   0x0040, 0x0020, 0x0010, 0x0000};
#define lt_4_width 12
#define lt_4_height 12
static unsigned short lt_4_bits[] = {
   0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100, 0x0080, 0x0040,
   0x0020, 0x0010, 0x0000, 0x0000};
#define lt_5_width 12
#define lt_5_height 12
static unsigned short lt_5_bits[] = {
   0x1000, 0x0800, 0x0400, 0x0200, 0x0100, 0x0080, 0x0040, 0x0020,
   0x0010, 0x0000, 0x0000, 0x0000};

#define rt_1_width 12
#define rt_1_height 12
static unsigned short rt_1_bits[] = {
   0x0010, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400,
   0x0800, 0x1000, 0x2000, 0xc000};
#define rt_2_width 12
#define rt_2_height 12
static unsigned short rt_2_bits[] = {
   0x0000, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800,
   0x1000, 0x2000, 0x4000, 0x0000};
#define rt_3_width 12
#define rt_3_height 12
static unsigned short rt_3_bits[] = {
   0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000,
   0x2000, 0x4000, 0x8000, 0x0000};
#define rt_4_width 12
#define rt_4_height 12
static unsigned short rt_4_bits[] = {
   0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000,
   0x4000, 0x8000, 0x0000, 0x0000};
#define rt_5_width 12
#define rt_5_height 12
static unsigned short rt_5_bits[] = {
   0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000,
   0x8000, 0x0000, 0x0000, 0x0000};

#define lb_1_width 12
#define lb_1_height 12
static unsigned short lb_1_bits[] = {
   0x0030, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000,
   0x2000, 0x4000, 0x8000, 0x8000};
#define lb_3_width 12
#define lb_3_height 12
static unsigned short lb_3_bits[] = {
   0x0000, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400,
   0x0800, 0x1000, 0x2000, 0x4000};
#define lb_4_width 12
#define lb_4_height 12
static unsigned short lb_4_bits[] = {
   0x0000, 0x0000, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200,
   0x0400, 0x0800, 0x1000, 0x2000};
#define lb_5_width 12
#define lb_5_height 12
static unsigned short lb_5_bits[] = {
   0x0000, 0x0000, 0x0000, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100,
   0x0200, 0x0400, 0x0800, 0x1000};
#define lb_6_width 16
#define lb_6_height 16
static unsigned short lb_6_bits[] = {
   0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002, 0x0004, 0x0008,
   0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800};

#define rb_1_width 12
#define rb_1_height 12
static unsigned short rb_1_bits[] = {
   0xc000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100, 0x0080,
   0x0040, 0x0020, 0x0010, 0x0010};
#define rb_3_width 12
#define rb_3_height 12
static unsigned short rb_3_bits[] = {
   0x0000, 0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200,
   0x0100, 0x0080, 0x0040, 0x0020};
#define rb_4_width 12
#define rb_4_height 12
static unsigned short rb_4_bits[] = {
   0x0000, 0x0000, 0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400,
   0x0200, 0x0100, 0x0080, 0x0040};
#define rb_5_width 12
#define rb_5_height 12
static unsigned short rb_5_bits[] = {
   0x0000, 0x0000, 0x0000, 0x8000, 0x4000, 0x2000, 0x1000, 0x0800,
   0x0400, 0x0200, 0x0100, 0x0080};
#define rb_6_width 12
#define rb_6_height 12
static unsigned short rb_6_bits[] = {
   0x0000, 0x0000, 0x0000, 0x0000, 0x8000, 0x4000, 0x2000, 0x1000,
   0x0800, 0x0400, 0x0200, 0x0100};

/* button on bits */
#define but_width 5
#define but_height 8
static unsigned short but_bits[] = {
   0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xe000, 0xc000, 0x8000};
#define buthl_width 6
#define buthl_height 6
static unsigned short buthl_bits[] = {
   0x1c00, 0x3800, 0x7000, 0xe000, 0xc000, 0x8000};
#define buts1_width 6
#define buts1_height 5
static unsigned short buts1_bits[] = {
   0x4000, 0x2000, 0x1000, 0x0800, 0x0400};
#define buts2_width 6
#define buts2_height 6
static unsigned short buts2_bits[] = {
   0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400};


#define HALFWIDTH	(RADIOBUTW/2)

unsigned short *lt[] = {
	lt_1_bits,
	lt_2_bits,
	lt_3_bits,
	lt_4_bits,
	lt_5_bits
};
unsigned short *rt[] = {
	rt_1_bits,
	rt_2_bits,
	rt_3_bits,
	rt_4_bits,
	rt_5_bits
};
unsigned short *lb[] = {
	lb_1_bits,
	rt_2_bits,
	lb_3_bits,
	lb_4_bits,
	lb_5_bits,
	lb_6_bits
};
unsigned short *rb[] = {
	rb_1_bits,
	lt_2_bits,
	rb_3_bits,
	rb_4_bits,
	rb_5_bits,
	rb_6_bits
};

static char top[] = {DarkGray,LightGray,White,255};
static char top_h[] = {DarkGray,White,White,255};
static char top_p[] = {DarkGray,DarkGray,LightGray,White,White,255};

static char bot[] = {DarkGray,DarkGray,255};
static char bot_h[] = {DarkGray,DarkGray,255};
static char bot_p[] = {White,White,White,Black,LightGray,LightGray,255};

static char *ba_top[] = {
	top,			/* neurtral */
	top_h,			/* located */
	top,			/* selected */
	top_p			/* loc + sel */
};

static char *ba_bot[] = {
	bot,
	bot_h,
	bot,
	bot_p
};

static void
drawRadioButton(struct radioButton *b)
{
	struct radioList *parent = b->parent;
	char *tp,*bt;
	int i;

	/* base */
	color((b->gui.state&ST_HIGHL)?VeryLightGray:LightGray);
	cmov2i(b->gui.x1,b->gui.y1);
	drawBitmap(base_bits,base_width,base_height);

	if (b->gui.flags&_INVALID)
		return;

	tp = ba_top[b->gui.state&0x03];
	bt = ba_bot[b->gui.state&0x03];

#define draw(map,masks)							\
	for (i=0;map[i] != 255; i++) {					\
		color(map[i]);						\
		drawBitmap(masks[i],HALFWIDTH,HALFWIDTH);		\
	}

	/* top left */
	cmov2i(b->gui.x1,b->gui.y1+HALFWIDTH);
	draw(tp,lt);

	/* top right */
	cmov2i(b->gui.x1+HALFWIDTH,b->gui.y1+HALFWIDTH);
	draw(tp,rt);

	/* bottom left */
	cmov2i(b->gui.x1,b->gui.y1);
	draw(bt,lb);

	/* bottom right */
	cmov2i(b->gui.x1+HALFWIDTH,b->gui.y1);
	draw(bt,rb);

	/* check for this button is selected */
	if (b == (struct radioButton *)parent->selected) {
		cmov2i(b->gui.x1+HALFWIDTH,b->gui.y1+HALFWIDTH+1);
		color(SlateBlue);
		drawBitmap(buthl_bits,buthl_width,buthl_height);
		cmov2i(b->gui.x1+HALFWIDTH,b->gui.y1+8);
		color(BLUE);
		drawBitmap(but_bits,but_width,but_height);
		color(BLACK);
		drawBitmap(buts1_bits,buts1_width,buts1_height);
		cmov2i(b->gui.x1+HALFWIDTH+1,b->gui.y1+7);
		color(DarkGray);
		drawBitmap(buts2_bits,buts2_width,buts2_height);
	}
}
