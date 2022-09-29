/* Text Field code
 */

#ident "$Revision: 1.6 $"

#include <stand_htport.h>
#include <gfxgui.h>
#include <guicore.h>
#include <style.h>
#include <ctype.h>
#include <libsc.h>
#include <libsc_internal.h>

static void drawTextField(struct TextField *t);
static void drawCursor(struct TextField *t, int on);

#define tffont(d)	(((d)->flags & TFIELDBIGFONT) ? ncenB18 : ScrB18)

static void
textFieldEventHandler(struct gui_obj *obj, int event)
{
	struct TextField *t = (struct TextField *)obj;
	switch(event) {
	case _REDRAW:
		drawTextField(t);
		break;
	case _FREE:
		if (t->text) free(t->text);
		free(t);
		break;
	}
	return;
}

static int
getTextFieldHeight(struct TextField *t)
{
	int fontheight = gfxgui.htp->fonts[tffont(t)].height;
	return(TFIELDBDW+TFIELDMARGIN+fontheight+TFIELDMARGIN+TFIELDBDW);
}

struct TextField *
createTextField(int x, int y, int w, int flags)
{
	struct TextField *t;

	t = (struct TextField *)guimalloc(sizeof(struct TextField));
	t->gui.type = TFIELD;
	t->gui.flags = 0;
	t->gui.state = 0;
	t->gui.x1 = x;
	t->gui.y1 = y;
	t->gui.x2 = x+w-1;
	t->gui.eventHandler = textFieldEventHandler;

	t->flags = flags;
	t->text = (char *)NULL;
	t->len = 0;

	setTextFieldBuffer(t,0,0);

	t->gui.y2 = y+getTextFieldHeight(t)-1;

	addObject(guiobj(t));
	
	return(t);
}

void
setTextFieldBuffer(struct TextField *t, char *buf, int length)
{
	if (buf) *buf = '\0';
	t->text = buf;
	t->len = length;
	t->cx = TFIELDBDW+TFIELDMARGIN;
	t->begpos = t->cpos = 0;
}

void
setTextFieldString(struct TextField *t, char *str)
{
	char *dst;
	int i;

	if (!t->text) return;

	dst = t->text;
	for (i=0; *str && (i < t->len); i++, t->cpos)
		*dst++ = *str++;
	t->cpos = i;
	t->begpos = t->cx = -1;		/* for recalculation */
	t->text[i] = '\0';
}

static void
updateText(struct TextField *t, struct htp_font *font, int oldcx)
{
	int maxx = t->gui.x2-t->gui.x1-TFIELDBDW-TFIELDMARGIN;
	int homex = TFIELDBDW+TFIELDMARGIN;
	int fontn = tffont(t);
	int redraw=0;
	int i,cx,y;

	y = t->gui.y1 + TFIELDBDW + TFIELDMARGIN + 1 + font->descender;

	/*  If we have overflowed, slide characters left until we can
	 * squeeze the new character in.
	 */
rightjustify:
	if (t->cx > maxx) {
		redraw = 1;
		t->cx -= font->info[chartoindex(t->text[t->begpos])].xmove;
		t->begpos++;
		goto rightjustify;
	}
	/*  If we backspace to the beginning left, check to see if
	 * we have anything to move back out.
	 */
	else if (t->cx <= homex) {
		if (t->begpos != 0) {
			redraw = 1;
			t->begpos = 0;
			t->cx = homex;
			for (i=0; i < t->cpos; i++)
				t->cx += font->info[chartoindex(t->text[i])].
					xmove;
			goto rightjustify;
		}
	}
	/*  if oldcx is set we should be able to just stuff the last character
	 * at oldcx and not redraw the whole line.
	 */
	else if (oldcx && !redraw) {
		color(BLACK);
		cmov2i(t->gui.x1+oldcx,y);
		txOutcharmap(gfxgui.htp,t->text[t->cpos-1],fontn);
		return;
	}

	/* Redraw the current showing text */
	color(TerraCotta);
	sboxfi(t->gui.x1+TFIELDBDW,t->gui.y1+TFIELDBDW,
	       t->gui.x2-TFIELDBDW,t->gui.y2-TFIELDBDW);

	color(BLACK);
	for (cx=homex,i=t->begpos; i < t->cpos; i++) {
		cmov2i(t->gui.x1+cx, y);
		txOutcharmap(gfxgui.htp,t->text[i],fontn);
		cx += font->info[chartoindex(t->text[i])].xmove;
	}
}

int
putcTextField(struct TextField *t, int c)
{
	int oldcx=0, xmove, findex;
	struct htp_font *font;
	int rc = 0;

	font = &gfxgui.htp->fonts[tffont(t)];

	drawCursor(t,0);

	switch(c) {
	case -1:			/* end of line */
	case '\n':
	case '\r':
		rc=1;
		break;
	case 025:			/* ^U erase line */
		setTextFieldBuffer(t,t->text,t->len);
		break;
	case '\177':			/* delete */
	case '\b':			/* backspace */
		if (t->cpos <= 0)
			break;
		t->cpos--;
		findex = chartoindex(t->text[t->cpos]);
		xmove = font->info[findex].xmove;
		t->text[t->cpos] = '\0';
		t->cx -= xmove; 
		break;
	default:
		if (t->cpos < (t->len-2) && isprint(c)) {
			findex = chartoindex(c);
			t->text[t->cpos++] = (char)c;
			t->text[t->cpos] = '\0';
			oldcx = t->cx;
			t->cx += font->info[findex].xmove;
		}
		break;
	}

	updateText(t,font,oldcx);
	drawCursor(t,1);
	return(rc);
}

static void
drawCursor(struct TextField *t, int on)
{
	int x = t->gui.x1 + t->cx + 1;
	int y1,y2;

	color(on ? BLUE : TerraCotta);

	if (t->flags&TFIELDDOTCURSOR) {
		y2 = t->gui.y1 + (t->gui.y2-t->gui.y1)/2;
		y1 = y2-1;
	}
	else {
		y1 = t->gui.y1 + TFIELDBDW+TFIELDMARGIN-1;
		y2 = t->gui.y2 - TFIELD-TFIELDMARGIN+1;
	}

	sboxfi(x,y1,x+1,y2);
}

static char tf_dtol[] = { DarkGray, Black, LightGray, TerraCotta };
static char tf_ltod[] = { White, VeryLightGray, DarkGray, DarkTerraCotta };

static void
drawTextField(struct TextField *t)
{
	int i,x1,x2,y1,y2;

	x1 = t->gui.x1;
	x2 = t->gui.x2;
	y1 = t->gui.y1;
	y2 = t->gui.y2;

#define tfbd(ramp,X1,Y1,X2,Y2)				\
	for (i=0; i < TFIELDBDW; i++) {			\
		color(ramp[i]);				\
		sboxfi(X1 i, Y1 i, X2 i, Y2 i);		\
	}

	tfbd(tf_ltod,x1+,y1+,x2-,y1+);		/* bottom */
	tfbd(tf_dtol,x1+,y1+,x1+,y2-);		/* left */
	tfbd(tf_dtol,x1+,y2-,x2-,y2-);		/* top */
	tfbd(tf_ltod,x2-,y1+,x2-,y2-);		/* right */

	color(TerraCotta);
	sboxfi(x1+TFIELDBDW,y1+TFIELDBDW, x2-TFIELDBDW,y2-TFIELDBDW);

	updateText(t,&gfxgui.htp->fonts[tffont(t)],0);

	drawCursor(t,1);
}
