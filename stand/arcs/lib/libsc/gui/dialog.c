/*  Standalone gui pop-up dialog.  Modeled after IRIX xconfirm.
 */

#ident "$Revision: 1.21 $"

#include <stand_htport.h>
#include <arcs/errno.h>
#include <saioctl.h>
#include <guicore.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc.h>
#include <libsc_internal.h>

static void dialogIntBoarder(int x1, int y1, int x2, int y2, int leftshade);
static void drawDialog(struct Dialog *d);

#define dialogfont(d)	(((d)->flags & DIALOGBIGFONT) ? ncenB18 : helvR10)

#define dialogButtonY(D)	((D)->gui.y1+DIALOGBDW+DIALOGMARGIN)

char *buttontext[] = {
	"Continue",
	"Restart",
	"Stop",
	"Cancel",
	"Accept"
};

struct Dialog *
_popupDialog(char *msg, int *bmsg, int type, int flags)
{
	struct Dialog *d;
	int *but;
	int i;

	(void)ioctl(0, TIOCFLUSH, 0);		/* clear input buffer */

	if ((flags&DIALOGFORCEGUI) && doGui())
		goto gui;

	if (!isGuiMode()) {
		__psint_t wantesc = 0;

		wantesc = 0;

		if (flags&DIALOGCENTER)
			p_center();
		else
			p_ljust();

		/* check for requests to return different value on <esc> */
		for (i=0,but=bmsg; but && *but >= 0; but++) {
			if (*but == DIALOGPREVESC)
				wantesc = i-1;
			else if (*but < DIALOGPREVDEF)
				i++;
		}

		p_printf(msg);

		if (bmsg) {
			int ch = getchar() & 0xff;
			if (ch == '\033')
				return((struct Dialog *)wantesc);
			return(0);
		}
		else
			for (;;) ;

	}

gui:
	d = createDialog(msg,type,flags);
	if (bmsg) {
		struct Button *b;

		for (b=NULL,i=0,but=bmsg; but && *but >= 0; but++) {
			if (*but == DIALOGPREVDEF)
				setDefaultButton(b,1);
			else if (*but == DIALOGPREVESC)
				d->which = i-1;
			else {
				b = addDialogButton(d,*but);
				i++;
			}
		}
	}
	return(d);
}

int
_popupPoll(struct Dialog *d)
{
	int c,button;

	drawObject(guiobj(d));

	if (d->flags & DIALOGBUTTONS) {
		while (!(d->flags&DIALOGCLICKED))
			if ((GetReadStatus(0)==ESUCCESS)) {
				c = getchar() & 0xff;

				/*  If looking for escape break since d->which
				 * is set.
				 */
				if (c == '\033')
					break;

				/*  If we have a textfield forward the
				 * character to it, otherwise take any
				 * key to continue.
				 */
				if (!d->textf || putcTextField(d->textf,c)) {
					/* default button is always 0 */
					d->which = 0;
					break;
				}
			}
	}
	else {
		for (;;)
			GetReadStatus(0);	/* track mouse */
	}

	button = d->which;

	deleteObject(guiobj(d));

	return(button);
}

int
popupAlignDialog(char *msg, struct gui_obj *align, int *bmsg, int type,
		 int flags)
{
	struct Dialog *d;
	long di;

	d = _popupDialog(msg,bmsg,type,flags);

	di = (long)d;			/* check tty return codes */
	if (di >= 0 && di <= 5)
		return(di);

	if (align) {
		centerObject(guiobj(d),align);
		moveObject(guiobj(d),d->gui.x1,align->y2-(d->gui.y2-d->gui.y1));
	}
	if (d)
		return(_popupPoll(d));
	return(0);
}


int
popupDialog(char *msg, int *bmsg, int type, int flags)
{
	return(popupAlignDialog(msg,0,bmsg,type,flags));
}

int
popupGets(char *msg, struct gui_obj *align, char *deflt, char *buf, int len)
{
	static int accept[] = { DIALOGACCEPT,DIALOGPREVDEF,
				DIALOGCANCEL,DIALOGPREVESC,
				-1 };
	struct TextField *t;
	struct Dialog *d;
	int rc;

	d = _popupDialog(msg, accept, DIALOGQUESTION,0);

	if (align) {
		centerObject(guiobj(d),align);
		moveObject(guiobj(d),d->gui.x1,align->y2-(d->gui.y2-d->gui.y1));
	}

	/* textfield as big as possible */
	rc = (align->x2-align->x1) - (d->gui.x2-d->gui.x1) - DIALOGTEXTMARGIN;
	t = addDialogTextField(d,rc,0);

	setTextFieldBuffer(t,buf,len);
	if (deflt) setTextFieldString(t,deflt);

	rc = _popupPoll(d);

	deleteObject(guiobj(d));

	return(rc);
}

static void
dialogEventHandler(struct gui_obj *obj, int event)
{
	struct Dialog *d = (struct Dialog *)obj;
	int i;

	switch(event) {
	case _REDRAW:
		drawDialog(d);
		break;

	case _FREE:
		if (d->textf)
			deleteObject(guiobj(d->textf));
		for (i=0; i < DIALOGMAXB; i++)
			if (d->button[i])
				deleteObject(guiobj(d->button[i]));
		free(d);
		break;
	}
}

/* calculate x position of button.
 */
static int
dialogButtonX(struct Dialog *d, int button)
{
	int i,x = d->gui.x2-DIALOGBDW-DIALOGMARGIN;
	struct Button *b;

	for (i=0; i <= button; i++) {
		b = d->button[i];
		x -= b ? b->gui.x2-b->gui.x1 : TEXTBUTW;
	}

	x -= button*BUTTONGAP;

	return(x);
}

/*ARGSUSED*/
static void
dialogButtonHandler(struct Button *b, __scunsigned_t value)
{
	struct Dialog *d = (struct Dialog *)value;
	int i;

	d->flags |= DIALOGCLICKED;
	for (i=0; i < DIALOGMAXB; i++)
		if (b == d->button[i]) {
			d->which = i;
			break;
		}
			
	return;
}

struct Button *
addDialogButton(struct Dialog *d, int bmsg)
{
	int i, move;

	for (i=0; i < DIALOGMAXB; i++) {
		if (d->button[i])
			continue;

		/* Resize dialog so buttons fit.  Keep same y centering.
		 */
		if ((d->flags & DIALOGBUTTONS) == 0) {
			d->flags |= DIALOGBUTTONS;
			move = DIALOGBUTMARGIN + TEXTBUTH;
			d->gui.y1 -= move>>1;
			d->gui.y2 += move>>1 + (move&1);
		}
		
		d->button[i] = createButton(dialogButtonX(d,i),
					     dialogButtonY(d),
					     TEXTBUTW,TEXTBUTH);
		d->button[i]->gui.parent = (struct gui_obj *)d;
		addButtonText(d->button[i],buttontext[bmsg]);
		addButtonCallBack(d->button[i],dialogButtonHandler,(__scunsigned_t)d);
		return(d->button[i]);
	}
	return(0);			/* no button slots left */
}

struct Dialog *
createDialog(char *msg, int type, int flags)
{
	struct Dialog *d;
	int basew = DIALOGBDW+DIALOGMARGIN+DIALOGINTBDW+DIALOGICONW+
		DIALOGINTBDW+DIALOGICONMARGIN+DIALOGINTBDW+DIALOGTEXTMARGIN+
		/* textwidth+ */ DIALOGTEXTMARGIN+DIALOGINTBDW+DIALOGMARGIN+
		DIALOGBDW-1;
	int baseh = DIALOGBDW+DIALOGMARGIN+DIALOGINTBDW+DIALOGTEXTMARGIN+
		/* textheight+ */ DIALOGTEXTMARGIN+DIALOGINTBDW+
		DIALOGMARGIN+DIALOGBDW-1;
	int th,tw;

	d = (struct Dialog *)guimalloc(sizeof(struct Dialog));
	d->gui.type = GDIALOG;
	d->gui.flags = _GRAB_FOCUS;
	d->gui.eventHandler = dialogEventHandler;

	/* strip leading newlines that are used for ttys.
	 */
	while ((*msg == '\n') && (*msg != '\0'))
		msg++;

	d->flags = flags;

	textSize(msg,&tw,&th,dialogfont(d));
	if (flags & DIALOGCENTER)
		d->textw = tw;
	else
		d->textw = 0;
	d->texth = th;
	tw += basew;
	th += baseh;

	/* insure minimum width on dialog box */
	if (tw < 350) {
		if (d->textw)
			d->textw += (350 - tw);
		tw = 350;
	}

	/* default is centered in x, and centered at 2/3s point in y */
	d->gui.x1 = (gfxWidth() - tw) >> 1;
	d->gui.y1 = ((gfxHeight() - th)/3)*2;
	d->gui.x2 = d->gui.x1 + tw;
	d->gui.y2 = d->gui.y1 + th;

	d->textf = 0;
	d->msg = msg;
	d->type = type;
	d->which = 0;

	bzero(d->bmsg,sizeof(d->bmsg));
	bzero(d->button,sizeof(d->button));

	addObject(guiobj(d));

	return(d);
}

static int typecolors[] = {
	lightGrayBlue,
	lightGrayCyan,
	lightGrayGreen,
	lightGrayMagenta,
	lightGrayRed
};

#define acti_w 22
#define acti_h 22
static unsigned short acti_bits[] = {
   0x01fe, 0x0000, 0x07ff, 0x8000, 0x1fff, 0xe000, 0x3e01, 0xf000,
   0x3801, 0xf000, 0x7003, 0xf800, 0x7007, 0xb800, 0xe00f, 0x1c00,
   0xe01e, 0x1c00, 0xe03c, 0x1c00, 0xe078, 0x1c00, 0xe0f0, 0x1c00,
   0xe1e0, 0x1c00, 0xe3c0, 0x1c00, 0xe780, 0x1c00, 0x7f00, 0x3800,
   0x7e00, 0x3800, 0x3c00, 0x7000, 0x3e01, 0xf000, 0x1fff, 0xe000,
   0x07ff, 0x8000, 0x01fe, 0x0000};
#define info_w 6
#define info_h 20
static unsigned short info_bits[] = {
   0xfc00, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800,
   0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0xf800, 0x0000, 0x0000,
   0x0000, 0x7800, 0x7800, 0x7800};
#define prog_w 12
#define prog_h 23
static unsigned short prog_bits[] = {
   0xfff0, 0x8010, 0x9f90, 0x8f10, 0x8610, 0x8010, 0x4020, 0x4020,
   0x2040, 0x1080, 0x0900, 0x0900, 0x0900, 0x1080, 0x2640, 0x4f20,
   0x5fa0, 0x9f90, 0xbfd0, 0xbfd0, 0x8010, 0x8010, 0xfff0};
#define ques_w 11
#define ques_h 20
static unsigned short ques_bits[] = {
   0x0c00, 0x1e00, 0x1e00, 0x1e00, 0x0c00, 0x0000, 0x0000, 0x0000,
   0x0c00, 0x0c00, 0x0e00, 0x0700, 0x0780, 0x03c0, 0x61e0, 0xf0e0,
   0xf0e0, 0xf0e0, 0x79c0, 0x1f80};
#define warn_w 4
#define warn_h 20
static unsigned short warn_bits[] = {
   0x6000, 0xf000, 0xf000, 0xf000, 0x6000, 0x0000, 0x0000, 0x0000,
   0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0xf000, 0xf000,
   0xf000, 0xf000, 0xf000, 0x6000};

static struct {
	unsigned short *bitmap;
	short w,h;
} iconbitmaps[] = {
	info_bits, info_w, info_h,
	prog_bits, prog_w, prog_h,
	ques_bits, ques_w, ques_h,
	warn_bits, warn_w, warn_h,
	acti_bits, acti_w, acti_h
};

static void
drawDialog(struct Dialog *d)
{
	struct htp_font *font;
	int x1 = d->gui.x1;
	int x2 = d->gui.x2;
	int y2 = d->gui.y2;
	int i,cx,cy;

	drawCanvas(&d->gui);

	color(typecolors[d->type]);
	sboxfi(x1+DIALOGBDW+DIALOGMARGIN+DIALOGINTBDW,
	       y2-DIALOGBDW-DIALOGMARGIN-DIALOGINTBDW,
	       x1+DIALOGBDW+DIALOGMARGIN+DIALOGINTBDW+DIALOGICONW-1,
	       y2-DIALOGBDW-DIALOGMARGIN-DIALOGINTBDW-DIALOGICONW+1);

	dialogIntBoarder(x1+DIALOGBDW+DIALOGMARGIN,
		y2-DIALOGBDW-DIALOGMARGIN-2*DIALOGINTBDW-DIALOGICONW+1,
		x1+DIALOGBDW+DIALOGMARGIN+2*DIALOGINTBDW+DIALOGICONW-1,
		y2-DIALOGBDW-DIALOGMARGIN,typecolors[d->type]+1);

	cx = x1+DIALOGBDW+DIALOGMARGIN+DIALOGINTBDW+
		((DIALOGICONW-iconbitmaps[d->type].w)>>1);
	cy = y2-DIALOGBDW-DIALOGMARGIN-DIALOGINTBDW-DIALOGICONW+
		((DIALOGICONW-iconbitmaps[d->type].h)>>1)+1;

	color(BLACK);
	cmov2i(cx,cy);
	drawBitmap(iconbitmaps[d->type].bitmap,
		   iconbitmaps[d->type].w,
		   iconbitmaps[d->type].h);

	if (d->textf)
		drawObject(guiobj(d->textf));

	for (i=0; i < DIALOGMAXB ; i++)
		if (d->button[i])
			drawObject(guiobj(d->button[i]));

	font = &gfxgui.htp->fonts[dialogfont(d)];
	cx = x1+DIALOGTEXTBDX+DIALOGINTBDW+DIALOGTEXTMARGIN;
	cy = y2-DIALOGBDW-DIALOGMARGIN-DIALOGINTBDW-DIALOGTEXTMARGIN+1;

	dialogIntBoarder(x1+DIALOGTEXTBDX,
			 cy-DIALOGTEXTMARGIN-DIALOGINTBDW-d->texth,
			 x2-DIALOGBDW-DIALOGMARGIN,
			 y2-DIALOGBDW-DIALOGMARGIN,
			 0);

	color(BLACK);
	cy -= font->height - font->descender;
	putctext(d->msg,cx,cy,d->textw,dialogfont(d));
}

static void
dialogIntBoarder(int x1, int y1, int x2, int y2, int leftshade)
{
	int i,ramp[DIALOGINTBDW];

#define bd(E,X1,Y1,X2,Y2)				\
	for (i = 0; i < E; i++) {			\
		color(ramp[i]);				\
		sboxfi(X1 i, Y1 i, X2 i, Y2 i);		\
	}

	/* bottom and right */
	ramp[0] = White; ramp[1] = DarkGray; ramp[2] = LightGray;
	bd(DIALOGINTBDW,x1+,y1+,x2-,y1+);
	bd(DIALOGINTBDW,x2-,y1+,x2-,y2-);

	/* top and left */
	ramp[0] = LightGray; ramp[1] = DarkGray;
	ramp[2] = leftshade ? leftshade : White;
	x2--;
	bd(DIALOGINTBDW,x1+,y2-,x2-,y2-);
	bd(DIALOGINTBDW,x1+,y1+,x1+,y2-);
}

void
resizeDialog(struct Dialog *d, int dx, int dy)
{
	int i;

	d->gui.x1 -= (dx/2);
	d->gui.x2 += (dx/2) + (dx%2);
	d->gui.y1 -= (dy/2);
	d->gui.y2 += (dy/2) + (dy%2);

	for (i = 0; i < DIALOGMAXB; i++)
		if (d->button[i])
			moveObject(guiobj(d->button[i]),dialogButtonX(d,i),
				   dialogButtonY(d));
}

struct TextField *
addDialogTextField(struct Dialog *d, int width, int flags)
{
	int x,y,fh;

	resizeDialog(d,width+DIALOGTEXTMARGIN,0);

	fh = gfxgui.htp->fonts[dialogfont(d)].height;
	x = d->gui.x2 - width -
		DIALOGTEXTMARGIN-DIALOGINTBDW-DIALOGMARGIN-DIALOGBDW+1;
	y = d->gui.y2 - DIALOGBDW-DIALOGMARGIN-DIALOGINTBDW-DIALOGTEXTMARGIN-
		fh-TFIELDBDW-TFIELDMARGIN;

	y += (d->flags&DIALOGBIGFONT) ? 1 : -1;

	d->textf =  createTextField(x,y,width,flags);

	d->textf->gui.parent = &d->gui;

	return(d->textf);
}


