/* guicore.h -- definitions for core of standalone gui
 */

#ident "$Revision: 1.4 $"

struct gfxgui {
	struct gui_obj *next;
	struct gui_obj *prev;
	struct htp_state *htp;
	struct htp_fncs *fncs;
	void *hw;
	int textx,texty;
	int flags;
};

extern struct gfxgui gfxgui;

#ifndef lint			/* lint gives cpp warings on color */
#define color(clr)		(*gfxgui.fncs->color)(gfxgui.hw,clr)
#define sboxfi(x1,y1,x2,y2)	(*gfxgui.fncs->sboxfi)(gfxgui.hw,x1,y1,x2,y2)
#define cmov2i(x,y)		(*gfxgui.fncs->cmov2i)(gfxgui.hw,x,y)
#define pnt2i(x,y)		(*gfxgui.fncs->pnt2i)(gfxgui.hw,x,y)
#define drawbitmap(bit)		(*gfxgui.fncs->drawbitmap)(gfxgui.hw,bit)
#endif
