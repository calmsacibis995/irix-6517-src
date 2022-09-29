#ident "$Revision: 1.34 $"

#include <stand_htport.h>
#include <guicore.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc.h>
#include <libsk.h>

/* Early initialization of gfxgui (called from pon).
 */
void
initGfxGuiEarly(void)
{
	extern struct htp_state *htp;

	initGfxGui(htp);			/*XXX--hart removed, why?*/
	txConfig(htp,GUI_MODE);
	drawBackground();
	htp->textport.state = GUI;
	(htp->fncs->blankscreen)(htp->hw, 0);
}

/*  Return boolean if prom should do graphics at pon time.
 * console_is_gfx() checks global gfx_ok, which is false
 * if graphics pon fails.
 */
int
doEarlyGui(void)
{
	char *p;

	p = getenv("diagmode");
	return((!p || *p != 'v') && console_is_gfx());
}

/* routines for actually doing pon time graphics: */

static struct Dialog *d;

void
pongui_setup(char *ponmsg, int checkgui)
{
	if (checkgui && doEarlyGui()) {
		initGfxGuiEarly();
		d = createDialog(ponmsg,DIALOGPROGRESS,DIALOGBIGFONT);
		moveObject(guiobj(d),d->gui.x1,
			   MSGY1-((d->gui.y2-d->gui.y1)>>1));

		drawObject(guiobj(d));
		setRedrawUnder((struct gui_obj*)d);
	}
	else
		printf ("\n\n\n%25s%s\n\n"," ",ponmsg);
}

void
pongui_cleanup(void)
{
	if (isGuiMode() && d)
		deleteObject(guiobj(d));
}


