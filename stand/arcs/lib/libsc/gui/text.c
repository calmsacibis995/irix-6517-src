/* standalone gui text/font handling.
 */

#include <stand_htport.h>
#include <gfxgui.h>
#include <guicore.h>
#include <style.h>
#include <libsc.h>
#include <libsc_internal.h>

static void drawText(struct Text *);
void puttext(const char *, int);
void textSize(const char *, int *, int *, int);

void
moveTextPoint(int x, int y)
{
	gfxgui.textx = x;
	gfxgui.texty = y;
}

static void
textEventHandler(struct gui_obj *obj, int event)
{
	struct Text *t = (struct Text *)obj;

	switch(event) {
	case _REDRAW:
		drawText(t);
		break;
	case _FREE:
		free(t->text);
		free(t);
		break;
	}
	return;
}

struct Text *
createText(char *text, int font)
{
	struct Text *t;
	int w, h;

	t = (struct Text *)guimalloc(sizeof(struct Text));
	t->gui.type = GTEXT;
	t->gui.flags = 0;
	t->gui.x1 = t->gui.x2 = gfxgui.textx;
	t->gui.y1 = t->gui.y2 = gfxgui.texty;
	t->gui.eventHandler = textEventHandler;

	textSize(text,&w,&h,font);
	t->gui.x2 += w;
	t->gui.y2 += h;

	t->fg = BLACK;			/* default color is black */
	t->font = (short)font;
	t->text = (char *)guimalloc(strlen(text)+1);
	strcpy(t->text,text);

	addObject(guiobj(t));

	return(t);
}

static void
drawText(struct Text *t)
{
	cmov2i(t->gui.x1, t->gui.y1);
	color(t->fg);
	puttext(t->text, t->font);
}

int
textWidth(const char *s, int font)
{
	int width = 0;
	char *c = (char *)s;

	while ((*c != '\0') && (*c != '\n')) {
		width += gfxgui.htp->fonts[font].info[chartoindex(*c)].xmove;
		c++;
	}
	return width;
}

int
fontHeight(int font)
{
	return(gfxgui.htp->fonts[font].height);
}

/*
 * Draw some text with the specified font.
 */
void
puttext(const char *s, int f)
{
	while (*s != '\0') {
		txOutcharmap(gfxgui.htp, *s, f);
		s++;
	}
}

/*
 * Draw some text with the specified font, clipped to np pixels.
 */
void
putcltext(const char *s, int f, int np)
{
	int x = 0;

	while ((*s != '\0') && (x < np)) {
		x += txOutcharmap(gfxgui.htp, *s, f);
		s++;
	}
}

/*  Find the text size for a multi-line string.
 */
void
textSize(const char *s, int *width, int *height, int font)
{
	int maxw, n, w;

	n = maxw = 0;

	for (n=maxw=0; *s != '\0'; n++) {
		for (w=0; *s != '\n' && *s != '\0'; s++)
			w += gfxgui.htp->fonts[font].info[chartoindex(*s)].xmove;
		while (*s == '\n')
			s++;
		if (w > maxw)
			maxw = w;
	}

	*width = maxw;
	*height = n * gfxgui.htp->fonts[font].height + (n-1)*2;

	return;
}

/*
 * Draw some text with the specified font.
 */
void
putctext(const char *s, int x, int y, int w, int f)
{
	while (*s != '\0') {
		if (w)
			cmov2i(x+((w-textWidth(s,f))>>1),y);
		else
			cmov2i(x,y);
		while ((*s != '\n') && (*s != '\0')) {
			txOutcharmap(gfxgui.htp, *s, f);
			s++;
		}
		if (*s == '\n') {
			y -= (gfxgui.htp->fonts[f].height + 2);
			while (*(++s) == '\n') ;
		}
	}
}
