/*  Short graphics utility routines.
 */

#include <arcs/io.h>
#include <stand_htport.h>
#include <gfxgui.h>
#include <guicore.h>
#include <style.h>
#include <libsc.h>
#include <libsc_internal.h>

#ident "$Revision: 1.9 $"

/* In general do gui if we are outputting to graphics.
 */
int
doGui(void)
{
	/*  If sgivers() == 0, then we have a pre-version controlled
	 * prom which can't do libsc only gui.
	 */
	char *p = getenv("nogui");
	return(sgivers() && (!p || (*p != 'y')) && isgraphic(StandardOut));
}

/* turns on gui mode */
void
setGuiMode(int on, int flags)
{
	gfxgui.flags = flags | (gfxgui.flags & 0x7fff0000);
	if (on) {
		puts("\033[35h");
		drawBackground();
	}
	else {
		cleanGfxGui();
		if (flags & GUI_RESET)
			gfxgui.htp->textport.state = DIRTY;
	}
}

/* returns true if still in gui mode (textport hasn't popped up) */
int
isGuiMode(void)
{
	return(gfxgui.htp && (gfxgui.htp->textport.state == GUI));
}
