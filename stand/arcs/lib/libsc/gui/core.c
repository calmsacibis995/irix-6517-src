/* standalone gui core routines.
 */

#ident "$Revision: 1.16 $"

#include <arcs/spb.h>
#include <arcs/pvector.h>
#include <stand_htport.h>
#include <gfxgui.h>
#include <guicore.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsc_internal.h>

struct gfxgui gfxgui;

#define MSX gfxgui.htp->x
#define MSY gfxgui.htp->y
#define MSB gfxgui.htp->buttons

static int intersect(struct gui_obj *o, int x1, int y1, int x2, int y2);
void guiCheckMouse(int, int, int);

/* Initialize the gui data structures.
 */
void
initGfxGui(struct htp_state *htp)
{

	char *p;

	if (htp) {
		gfxgui.htp = htp;			/* libsk programs */
		htp->mshandler = 0;
	}
	else {
		gfxgui.htp = __PTV->gethtp();		/* libsc programs */
		if (!gfxgui.htp)
			return;
		gfxgui.htp->mshandler = guiCheckMouse;
	}

	gfxgui.fncs = gfxgui.htp->fncs;
	gfxgui.hw = gfxgui.htp->hw;
	gfxgui.next = gfxgui.prev = (struct gui_obj *)&gfxgui;
	gfxgui.textx = gfxgui.texty = 0;

	/* if sgilogo is unset, or 'n' then dont print the SGIish logos
	 * (for OEMs).  Also prevent logo on small screens since it
	 * just gets in the way.
	 */
	p = getenv("sgilogo");
	if (!p || *p == 'n' || (gfxgui.flags & GUI_LOGOOFF) ||
	    (gfxHeight() < 700))
		gfxgui.flags |= GUI_NEVERLOGO;
	else
		gfxgui.flags &= ~GUI_NEVERLOGO;
}

/*  Returns status of SGI logo.  Some screen paint differently if logo
 * is drawn or not due to screen space.
 */
int
noLogo(void)
{
	return(gfxgui.flags & (GUI_NEVERLOGO|GUI_NOLOGO));
}

void
logoOff(void)
{
	gfxgui.flags |= GUI_LOGOOFF;
}

void
_gfxsetms(int x, int y)
{
	if (!gfxgui.htp)
		return;			/* no gfx! */

	MSX = x;
	MSY = y;

	(*gfxgui.fncs->movec)(gfxgui.hw,x,y);
}

/* Free a linked list of gui objects
 */
static void
cleanList(void)
{
	struct gui_obj *p, *next;

	for (p=gfxgui.next; p != (struct gui_obj *)&gfxgui; p = next) {
		next = p->next;
		(*p->eventHandler)(p,_FREE);
	}
}

/*  Frees malloc()ed storage.  Used when switching modes to remove
 * all gui objects.
 */
void
cleanGfxGui(void)
{
	if (gfxgui.htp) {
		if (gfxgui.next) cleanList();
		gfxgui.next = gfxgui.prev = (struct gui_obj *)&gfxgui;
		gfxgui.htp->textport.aflags &= ~TP_NOBG;
	}
}

/* List management routines
 */
void
addObject(struct gui_obj *o)
{
	o->prev = (struct gui_obj *)&gfxgui;
	o->next = gfxgui.next;
	o->next->prev = o;
	gfxgui.next = o;

	o->parent = 0;
}

void
deleteObject(struct gui_obj *o)
{
	int x1,x2,y1,y2,redraw;

	o->prev->next = o->next;
	o->next->prev = o->prev;

	x1 = o->x1;
	y1 = o->y1;
	x2 = o->x2;
	y2 = o->y2;
	redraw = o->flags & _REDRAW_UNDER;

	(*o->eventHandler)(o,_FREE);

	if (redraw) {
		struct gui_obj *p;

		drawShadedBackground(gfxgui.htp,x1,y1,x2,y2);

		for (p=gfxgui.next; p!=(struct gui_obj *)&gfxgui; p=p->next)
			if (intersect(p,x1,y1,x2,y2))
				p->eventHandler(p,_REDRAW);
	}

	guiCheckMouse(0,0,MSB);
}

void
moveObject(struct gui_obj *o, int newx1, int newy1)
{
	int w = o->x2 - o->x1;
	int h = o->y2 - o->y1;
	int dx = o->x1 - newx1;
	int dy = newy1 - o->y1;
	struct gui_obj *p;

	o->x1 = newx1;
	o->x2 = newx1 + w;
	o->y1 = newy1;
	o->y2 = newy1 + h;

	/* check list for objects who list o as their parent */
	for (p=gfxgui.next; p!= (struct gui_obj *)&gfxgui; p = p->next) {
		if (p->parent == o)
			moveObject(p,p->x1+dx,p->y1+dy);
	}
}

void
centerObject(struct gui_obj *o, struct gui_obj *in)
{
	int x1,y1;

	x1 = in->x1 + ((in->x2 - in->x1 - (o->x2-o->x1)) >> 1);
	y1 = in->y1 + ((in->y2 - in->y1 - (o->y2-o->y1)) >> 1);

	moveObject(o,x1,y1);
}

void
setRedrawUnder(struct gui_obj *o)
{
	o->flags |= _REDRAW_UNDER;
}

/*  Checks if line x1,y2 x2,y2 crosses o.  Lines either vertical, or
 * horizontal, and note [xy]1 <= [xy]2.
 */
static int
lcross(struct gui_obj *o, int x1, int y1, int x2, int y2)
{
	if (y1 == y2) {			/* horizontal */
		if ( (y1 >= o->y1) && (y1 <= o->y2) ) {
			if ( x1 <= o->x1 )
				return(x2 >= o->x1);
			else
				return(x1 <= o->x2);
		}
	}
	else {				/* vertical */
		if ( (x1 >= o->x1) && (x1 <= o->x2) ) {
			if ( y1 <= o->y1)
				return(y2 >= o->y2);
			else
				return(y1 <= o->y2);
		}
	}
		
	return(0);
}

/* point x,y is contained in the area defined by o */
static int
pcontains(struct gui_obj *o, int x, int y)
{
	return( (o->x1 <= x) && (o->x2 >= x) && (o->y1 <= y) && (o->y2 >= y) );
}

/*  Return true if object o, and the rectangle defined by x1,y1 x2,y2
 * intersect.  This happens if any of the line segments in xy intersect
 * with o, or if that isn't true, then any point of xy is inside o (ie
 * all of xy is in o).
 */
static int
intersect(struct gui_obj *o, int x1, int y1, int x2, int y2)
{
	return(lcross(o,x1,y1,x2,y1) || lcross(o,x2,y1,x2,y1) ||
	       lcross(o,x1,y1,x1,y2) || lcross(o,x2,y1,x2,y2) ||
	       pcontains(o,x1,y1));
}

void *
guimalloc(size_t size)
{
	void *p = (void *)malloc(size);
	if (!p) panic("error: cannot malloc space for standalone gui\n");
	return(p);
}

#define button1(X)	(((X)&0x04)==0)
#define button(X)	(((X)&0x07)!=0x07)

void
guiCheckMouse(int dx, int dy, int nb)
{
	int state,lb,x,y,ob;
	struct gui_obj *p;
	extern int _prom;

	if (gfxgui.htp == 0)
		return;

	ob = MSB;		/* get old button state */

	if (_prom) {
		if (dx || dy) {
			MSX += dx;
			MSY += dy;

			if (MSX < 0) MSX = 0;
			if (MSY < 0) MSY = 0;
			if (MSX >= gfxgui.htp->xscreensize)
				MSX = gfxgui.htp->xscreensize-1;
			if (MSY >= gfxgui.htp->yscreensize)
				MSY = gfxgui.htp->yscreensize-1;

			(*gfxgui.fncs->movec)(gfxgui.hw,MSX,MSY);
		}

		/* second level (libsc) mouse handler */
		if (gfxgui.htp->mshandler)
			gfxgui.htp->mshandler(dx,dy,nb);

		MSB = nb;	/* update button state */
	}

	y = gfxHeight() - MSY;
	x = MSX;

	/* button states:
	 *	-1 : unchanged.
	 *	 0 : released.
	 *	 1 : pressed.
	 */
 	if ((lb=button(nb)) && button(ob))
		lb = -1;

	for (p = gfxgui.next; p != (struct gui_obj *)&gfxgui; p = p->next) {
		state = p->state;
		if ((x >= p->x1) && (x <= p->x2) &&
		    (y >= p->y1) && (y <= p->y2)) {
			/*  Set button pressed if button on downstroke,
			 * else highlight button if no mouse buttons pressed
			 * or if we are over the pressed button.
			 */
			if (lb == 1)
				p->state |= ST_PRESSED|ST_HIGHL;
			else if ((button(nb)==0) || (p->state & ST_PRESSED))
				p->state |= ST_HIGHL;
		}
		else
			p->state &= ~ST_HIGHL;

		/*  Button released.  If the current object was pressed, and
		 * it wants button up events, send it that event.
		 */
		if (lb == 0) {
			int tmp = p->state;

			p->state &= ~ST_PRESSED;
			if (((tmp&(ST_PRESSED|ST_HIGHL)) ==
			     (ST_PRESSED|ST_HIGHL)) &&
			    (p->flags&_WANT_BUTTON_UP)) {
				p->eventHandler(p,_REDRAW);
				p->eventHandler(p,_BUTTON_UP);
			}
		}

		/*  If state has changed, and want motion redraw, then redraw
		 * the object.
		 */
		if ((state != p->state) && (p->flags & _WANT_MOTION_RD))
			p->eventHandler(p,_REDRAW);

		/*  If we have an object with the _GRAB_FOCUS flag is set
		 * then do not search the rest of the list since this obj
		 * has grabbed input (usually for pop-up dialogs).
		 *
		 *  Need to remove highlight if any object is selected.
		 */
		if (p->flags & _GRAB_FOCUS) {
			p = p->next;
			while (p != (struct gui_obj *)&gfxgui) {
				if ((p->state & ST_HIGHL) &&
				    (p->flags & _WANT_MOTION_RD)) {
					p->state &= ~ST_HIGHL;
					p->eventHandler(p,_REDRAW);
				}
				p = p->next;
			}
			break;
		}
	}
}

void
drawObject(struct gui_obj *o)
{
	o->eventHandler(o,_REDRAW);
}

void
guiRefresh(void)
{
	struct gui_obj *o;

	for (o = gfxgui.prev; o != (struct gui_obj *)&gfxgui ; o = o->prev)
		o->eventHandler(o, _REDRAW);
}

int
gfxHeight(void)
{
	return(gfxgui.htp->yscreensize);
}

int
gfxWidth(void)
{
	return(gfxgui.htp->xscreensize);
}
