/* gl simulated textport
 */

#ident "$Revision: 1.10 $"

#include <stand_htport.h>
#include <arcs/hinv.h>
#include <style.h>
#include <libsc.h>
#include <libsk.h>

#include "ipxx.h"

extern int gltpfd;

#define GLTPX1280	1280
#define GLTPY1280	1024
#define GLTPX1024	1024
#define GLTPY1024	768

static COMPONENT gltptmpl = {
	ControllerClass,		/* Class */
	DisplayController,		/* Type */
	ConsoleOut|Output,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	8,				/* IdentifierLength */
	"SGI-gltp"			/* Identifier */
};
extern COMPONENT montmpl;		/* defined in gfx.c */

extern struct htp_fncs gltp_fncs;

int
gl_install(COMPONENT *root)
{
	extern int _gfx_strat(), ioserial;
	COMPONENT *tmp;

	if (ioserial) return 0;
	

	tmp = AddChild(root,&gltptmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL)
		cpu_acpanic("gltp");
	RegisterDriverStrategy(tmp, _gfx_strat);
	tmp = AddChild(tmp,&montmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL)
		cpu_acpanic("monitor");

	return 0;
}

gl_probe(struct htp_fncs **fncs )
{
	extern int ioserial;
	static int i=0;

	if (ioserial) return(0);

	if (fncs) {
		*fncs = &gltp_fncs;
		return(0);
	}

	if (i == 0) {
		i = 1;
		return(1);
	}

	return(0);
}

/* redefine outchar map to use promgl loaded fonts */
void
txOutcharmap(struct htp_state *htp, unsigned char c, int font)
{
	char buf[3];

	buf[0] = 'f';
	if (c > 6) {
		buf[1] = font;
		buf[2] = chartoindex(c) + 1;
	}
	else {
		buf[1] = Special;
		buf[2] = c;
	}
	l_write(gltpfd,buf,3);
}

/* TP FUNCTIONS */

static void
glTpBlankscreen(void *hw, int mode)
{
	char buf[2];

	buf[0] = 'B';
	buf[1] = mode;
	l_write(gltpfd,buf,2);
}

static void
glTpColor(void *hw, int cindex)
{
	color(cindex);
}

static void
glTpMapcolor(void *hw, int col, int r, int g, int b)
{
	char buf[5];

	buf[0] = 'M';
	buf[1] = col;
	buf[2] = r;
	buf[3] = g;
	buf[4] = b;
	l_write(gltpfd,buf,5);
}

static void
glTpSboxfi(void *hw, int x1, int y1, int x2, int y2)
{
	sboxfi(x1,y1,x2,y2);
}

static void
glTpPnt2i(void *hw, int x, int y)
{
	pnt2i(x,y);
}

static void
glTpCmov2i(void *hw, int x, int y)
{
	char buf[5];

	buf[0] = 'm';
	buf[1] = (x & 0xff00) >> 8;
	buf[2] = x & 0xff;
	buf[3] = (y & 0xff00) >> 8;
	buf[4] = y & 0xff;
	l_write(gltpfd,buf,5);
}

static void
glTpDrawbitmap(void *hw, struct htp_bitmap *bitmap)
{
	char buf[100];
	int len;

	/* NO DIRECT GL REPRESENTATION! :-( */
	len = ((((bitmap->xsize-1)/16) + 1)*sizeof(short)) * bitmap->ysize;
	sprintf(buf,"d%d,%d,%d,%d,%d,%d,%d,%d:",
		bitmap->xsize,bitmap->ysize,
		bitmap->xorig,bitmap->yorig,
		bitmap->xmove,bitmap->ymove,
		bitmap->sper,len);
	l_write(gltpfd,buf,strlen(buf));
	l_write(gltpfd,bitmap->buf,len);
}

static void
glTpInit(void *hw, int *x, int *y)
{
	extern int altres_x, altres_y, fullscreen;

	if (altres_x && altres_y) {
		*x = altres_x;
		*y = altres_y;
	}
	else {
		*x = GLTPX1024;
		*y = GLTPY1024;
	}

	if (fullscreen)
		prefposition(0,*x,0,*y);
	else
		prefsize(*x,*y);

	winopen("ARCS PROM Sim");
}

static void
glTpMovec(void *hw, int x, int y)
{
	/* GL/X will move cursor for us! */
}

struct htp_fncs gltp_fncs = {
    glTpBlankscreen,
    glTpColor,
    glTpMapcolor,
    glTpSboxfi,
    glTpPnt2i,
    glTpCmov2i,
    glTpDrawbitmap,
    0,
    glTpInit,
    glTpMovec
};

/* GL -> promgl protocol routines */

winopen(char *str)
{
	char buf[100];
	sprintf(buf,"w%s:",str);
	l_write(gltpfd,buf,strlen(buf));
}

prefsize(int x, int y)
{
	char buf[5];
	buf[0] = 'p';
	buf[1] = (x & 0xff00) >> 8;
	buf[2] = x & 0xff;
	buf[3] = (y & 0xff00) >> 8;
	buf[4] = y & 0xff;
	l_write(gltpfd,buf,5);
}

prefposition(int x1, int x2, int y1, int y2)
{
	char buf[9];
	buf[0] = 'S';
	buf[1] = (x1 & 0xff00) >> 8;
	buf[2] = x1 & 0xff;
	buf[3] = (x2 & 0xff00) >> 8;
	buf[4] = x2 & 0xff;
	buf[5] = (y1 & 0xff00) >> 8;
	buf[6] = y1 & 0xff;
	buf[7] = (y2 & 0xff00) >> 8;
	buf[8] = y2 & 0xff;
	l_write(gltpfd,buf,9);
}

color(int index)
{
	char buf[2];
	buf[0] = 'c';
	buf[1] = (unsigned char )index;
	l_write(gltpfd,buf,2);
}

clear(void)
{
	l_write(gltpfd,"C",1);
}

sboxfi(int x1, int y1, int x2, int y2)
{
	char buf[9];
	int len;

	/* send less data on horizontal lines */
	if (y1 == y2) {
		buf[0] = 'h';
		buf[1] = (x1 & 0xff00) >> 8;
		buf[2] = x1 & 0xff;
		buf[3] = (y1 & 0xff00) >> 8;
		buf[4] = y1 & 0xff;
		buf[5] = ((x2-x1) & 0xff00) >> 8;
		buf[6] = (x2-x1) & 0xff;
		len = 7;
	}
	else {
		buf[0] = 'b';
		buf[1] = (x1 & 0xff00) >> 8;
		buf[2] = x1 & 0xff;
		buf[3] = (y1 & 0xff00) >> 8;
		buf[4] = y1 & 0xff;
		buf[5] = (x2 & 0xff00) >> 8;
		buf[6] = x2 & 0xff;
		buf[7] = (y2 & 0xff00) >> 8;
		buf[8] = y2 & 0xff;
		len = 9;
	}
	l_write(gltpfd,buf,len);
}

pnt2i(int x, int y)
{
	char buf[5];
	buf[0] = 'P';
	buf[1] = (x & 0xff00) >> 8;
	buf[2] = x & 0xff;
	buf[3] = (y & 0xff00) >> 8;
	buf[4] = y & 0xff;
	l_write(gltpfd,buf,5);
}

int
prserial(char *cp)
{
	for (cp = cp; *cp != '\0';  cp++) {
		cpu_errputc(*cp);
		if (*cp == '\n')
			cpu_errputc('\r');
	}
}
