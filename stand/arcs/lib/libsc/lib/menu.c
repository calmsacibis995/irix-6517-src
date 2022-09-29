/*
 * Simple Menu manager for PROM and Standalone programs
 *
 */

#ident "$Revision: 1.44 $"
 
#include <arcs/io.h>
#include <libsc.h>
#include <menu.h>
#include <parser.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc_internal.h>

int m_graphics;

static mitem_t *get_menu_choice(menu_t *);
static void display_menu(menu_t *);

/*
 * Entry point
 *
 */
void
menu_parser(menu_t *pmenu)
{
	mitem_t *pitem;
	int redrawmenu=1;

	if (m_graphics=doGui())
		setGuiMode(1,0);

	for (;;) {
		if (redrawmenu)
			display_menu(pmenu);
		if ((pitem = get_menu_choice(pmenu)) &&
			(!(pitem->flags & M_PASSWD) || validate_passwd())) {

			if (m_graphics) setGuiMode(0,0);

			if ((*pitem->service)())
				return;			/* done */

			if (m_graphics) setGuiMode(1,0);
		}
		else if (m_graphics) {
			redrawmenu = 0;
			continue;
		}
		redrawmenu = 1;
	}
}

/*
 * Magic box characters
 *
 */

#define LEFTSIDE	'\001'
#define RIGHTSIDE	'\006'

static int mschoice;
/*ARGSUSED*/
static void
menuCallBack(struct Button *b, __scunsigned_t button)
{
	mschoice = '0' + (int)button;
}

/*
 * menu displayer
 *
 */
static void
display_menu(menu_t *pmenu)
{
	int i, base = 0;

	if (m_graphics) {
		int menuh, gfxh, availh;

		/* no logo use whole screen, logo use upper 2/3s */
		menuh = pmenu->size*MENUBUTTH + (pmenu->size-1)*MENUVSPACE;
		gfxh = gfxHeight();
		availh = noLogo() ? gfxh : ((gfxh<<1)/3);
		base = gfxh - ((availh - menuh) >> 1) - MENUBUTTH;
	}
	else
		p_clear();

	if (pmenu->title && !m_graphics)
		p_printf("\n\n%s\n\n",pmenu->title);
	
	for ( i = 0; i < pmenu->size; i++ ) {
		if (m_graphics) {
			struct Button *b;
			int bx = MSGX;

			b = createButton(bx,base,MENUBUTTW,MENUBUTTH);
			if (pmenu->item[i].flags & M_INVALID)
				invalidateButton(b,1);
			if (pmenu->item[i].bits)
				addButtonImage(b,pmenu->item[i].bits);
			addButtonCallBack(b,menuCallBack,(__scunsigned_t)i+1);
			moveTextPoint(bx+MENUBUTTW+10,base+15);
			createText(pmenu->item[i].prompt, ncenB18);
			base -= MENUBUTTH+MENUVSPACE;
		}
		else if (!(pmenu->item[i].flags & M_GUIONLY)) {
			if (!(pmenu->item[i].flags & M_INVALID))
				p_printf("%d) %s\n", i+1, pmenu->item[i].prompt);
			else
				p_printf("\n");
		}
	}

	if (m_graphics)
		guiRefresh();
	
	p_offset(0);
}
/*
 * Prompt user
 *
 */
static mitem_t *
get_menu_choice(menu_t *pmenu)
{
	char buffer[LINESIZE];
	int j;
	int i;

	mschoice = 0;

	getchoice:

	if ( ! m_graphics ) {
		printf("\n%s ",pmenu->prompt);
		if (gets(buffer) == NULL)
			return 0;
	}
	else {
		int c = 0;

		while (!c) {
			if (GetReadStatus(StandardIn) == 0)
				c = getchar();
			else if (mschoice) {
				c = mschoice;
				mschoice = 0;
			}		
		}
		if ( c == EOF || c == '\0' )
			goto getchoice;
		buffer[0] = (char)c;
		buffer[1] = '\0';
	}

	if ( buffer[0] == '\0' ) {
		return 0;
	}
	else {
		for ( j = i = 0; buffer[i] && buffer[i] != '\n'; i++ )
			if ( buffer[i] >= '0' && buffer[i] <= '9' )
				j = (j*10) + (buffer[i]-'0');
			else
				goto error;
	}
	
	if (j >= 1 && j <= pmenu->size && !(pmenu->item[j-1].flags & M_INVALID)
	    && (!(pmenu->item[j-1].flags & M_GUIONLY) || m_graphics))
		return &pmenu->item[j-1];

	if ( m_graphics ) {
		bell();
		goto getchoice;
	}
	else
		printf("Item %d is not on the menu.\n", j);

	error:
	if ( m_graphics ) {
		bell();
		goto getchoice;
	}
	else {
		for (j=0; j < pmenu->size; j++)
			if (pmenu->item[j].flags & M_GUIONLY)
				break;
		printf("Please enter a number between 1 and %d.\n", j);
	}

	return 0;
}
